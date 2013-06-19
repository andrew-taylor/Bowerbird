import os, sys, shutil, errno, tempfile
import cherrypy, calendar, genshi, urllib2, lxml.html, lxml.etree
from cherrypy.lib import sessions
from cStringIO import StringIO
from urllib import urlencode
from zipfile import ZipFile

from bowerbird.common import *
from bowerbird.configparser import ConfigParser
from bowerbird.storage import ProxyStorage, Recording

from lib import ajax, template, jsonpickle
from lib.common import *
from lib.recordingshtmlcalendar import RecordingsHTMLCalendar
from lib.zeroconfscanner import ZeroconfScanner
from lib.transfer import TransferManager

# web interface config file
WEB_CONFIG = 'bowerbird-proxy.conf'
# parameters for parsing contents of web interface config file
BOWERBIRD_PORT_KEY = 'bowerbird_socket_port'
DEFAULT_BOWERBIRD_PORT = '8080'
DATABASE_KEY = 'database'
ROOT_DIR_KEY = 'root_dir'
EXTENSION_KEY = 'extension'
LOCALNET_SCAN_TIME_MS_KEY = 'localnet_scan_time_ms'
CONCURRENT_TRANSFERS_KEY = 'concurrent_transfers'
TRANSFER_BANDWIDTH_KEY = 'transfer_bandwidth'
TRANSFER_CHUNK_SIZE_KEY = 'transfer_chunk_size'
REQUIRED_KEYS = [DATABASE_KEY, ROOT_DIR_KEY, EXTENSION_KEY]

CONNECT_FAILURE_REDIRECTION_TIMEOUT = 10 # in seconds

# default number of concurrent transfers from remote bowerbirds
DEFAULT_CONCURRENT_TRANSFERS = 2
# default value for how long to wait for responses when probing for bowerbirds
DEFAULT_LOCALNET_SCAN_TIME_MS = 500
# default bandwidth to use for each transfer (in bytes/second)
DEFAULT_TRANSFER_BANDWIDTH = 100
# default chunk size for retrieving files (in bytes)
DEFAULT_TRANSFER_CHUNK_SIZE = 1048576 # a megabyte


UNIMPLEMENTED = '''<html>
<head><title>Uninplemented</title></head>
<body>Page not implemented yet. Please press the back button.</body>
</html>'''


class Root(object):
    def __init__(self, path, database_file, root_dir, extension,
            bowerbird_port, localnet_scan_time_ms, concurrent_transfers,
            transfer_bandwidth, transfer_chunk_size):
        self.path = path
        self.bowerbird_port = bowerbird_port

        # initialise storage
        self._storage = ProxyStorage(database_file, root_dir, extension)

        # create the zeroconf scanner to detect local bowerbirds
        self._scanner = ZeroconfScanner(ZEROCONF_TYPE, localnet_scan_time_ms)

        self._transfer_manager = TransferManager(bowerbird_port,
                concurrent_transfers, transfer_bandwidth, transfer_chunk_size)


    @cherrypy.expose
    @template.output('connect.html')
    def connect(self, address=None, remove=None, rescan=False, **ignored):
        # if already connected, then just bounce to the status page
        if hasSession(SESSION_STATION_ADDRESS_KEY):
            raise cherrypy.HTTPRedirect('/status')

        # ignore address if "rescan" was requested
        if not rescan and address:
            # if "Remove" button has been clicked, delete entry from history
            if remove:
                self._storage.removeConnection(address)
            else:
                # get the name from the address
                try:
                    name = self.getRemoteName(address)
                except urllib2.URLError:
                    return self.showConnectionFailure(
                            error="could not be established", address=address)

                self._storage.addConnection(name, address)
                setSession(SESSION_STATION_ADDRESS_KEY, address)
                setSession(SESSION_STATION_NAME_KEY, name)
                # make sure new connection's recordings are not filtered out
                if (hasSession(SESSION_FILTER_STATION_KEY)
                        and getSession(SESSION_FILTER_STATION_KEY) != name):
                    clearSession(SESSION_FILTER_STATION_KEY)
                raise cherrypy.HTTPRedirect('/status')

        return template.render(is_proxy=True, station=None,
                previous_connections=self._storage.getPreviousConnections(),
                local_connections=self.findLocalBowerbirds(rescan))


    @cherrypy.expose
    def disconnect(self, **ignored):
        # set disconnected
        clearSession(SESSION_STATION_ADDRESS_KEY)
        clearSession(SESSION_STATION_NAME_KEY)
        raise cherrypy.HTTPRedirect('/')


    @cherrypy.expose
    def index(self, **ignored):
        # just redirect to status page if connected, otherwise connect page
        if hasSession(SESSION_STATION_ADDRESS_KEY):
            raise cherrypy.HTTPRedirect('/status')
        else:
            raise cherrypy.HTTPRedirect('/connect')


    @cherrypy.expose
    def status(self, **kwargs):
        return self.updateHtmlFromBowerbird(self.tunnelConnectionToBowerbird(
                'status', kwargs))


    @cherrypy.expose
    def config(self, **kwargs):
        # TODO catch station name changes and update connection history
        return self.updateHtmlFromBowerbird(self.tunnelConnectionToBowerbird(
                'config', kwargs))


    @cherrypy.expose
    def schedule(self, **kwargs):
        return self.updateHtmlFromBowerbird(self.tunnelConnectionToBowerbird(
                'schedule', kwargs))


    @cherrypy.expose
    @template.output('recordings.html')
    def recordings(self, view='month', year=None, month=None, day=None,
            recording_id=None, set_recording_id=False, clear_recording_id=False,
            go_to_start=False, go_to_finish=False, go_to_today=False,
            filter_station=None, filter_title=None, filter_start=None,
            filter_finish=None, update_filter=None, reset_filter=None,
            clear_selected_date=False,
            set_filter_station=None, set_filter_title=None,
            set_filter_start=None, set_filter_finish=None,
            retrieve_recording=False, export_recording=False,
            export_start_time=None, export_finish_time=None,
            retrieve_selection=False, export_selection=False,
            sync_with_station=False, delete_cache=False,
            **ignored):
        # HACK: this helps development slightly
        if ignored:
            print "IGNORED",ignored

        # initialise error list
        errors = []

        if delete_cache:
            # Clear all recordings. The cron job will do the scanning/refilling.
            self._storage.clearRecordings()

        # HACK to stay connected
        #if not hasSession(SESSION_STATION_ADDRESS_KEY):
        #    setSession(SESSION_STATION_ADDRESS_KEY, '192.168.0.201')
        #    setSession(SESSION_STATION_NAME_KEY, 'Frankie')

        # update recordings from connected bowerbird
        if hasSession(SESSION_STATION_ADDRESS_KEY):
            self.updateRecordingsFromBowerbird()

        # handle requests to sync remote bowerbird by adding any missing
        # recording files to the transfer queue
        if sync_with_station and hasSession(SESSION_STATION_ADDRESS_KEY):
            station_address = getSession(SESSION_STATION_ADDRESS_KEY)
            for recording in self._storage.getRecordings(
                    station=getSession(SESSION_STATION_NAME_KEY)):
                if not recording.fileExists():
                    self._transfer_manager.add(recording, station_address)

        if reset_filter:
            filter_station = None
            clearSession(SESSION_FILTER_STATION_KEY)
            filter_title = None
            clearSession(SESSION_FILTER_TITLE_KEY)
            filter_start = None
            clearSession(SESSION_FILTER_START_KEY)
            filter_finish = None
            clearSession(SESSION_FILTER_FINISH_KEY)

        if clear_selected_date:
            clearSession(SESSION_DATE_KEY)

        if clear_recording_id:
            clearSession(SESSION_RECORD_ID_KEY)

            # update filters
        if update_filter:
            # filtering on station
            if filter_station == NO_FILTER_STATION:
                filter_station = None
                clearSession(SESSION_FILTER_STATION_KEY)
            else:
                setSession(SESSION_FILTER_STATION_KEY, filter_station)

            # filtering on title
            if filter_title == NO_FILTER_TITLE:
                filter_title = None
                clearSession(SESSION_FILTER_TITLE_KEY)
            else:
                setSession(SESSION_FILTER_TITLE_KEY, filter_title)

            # filtering on start date & time
            if filter_start is not None:
                if filter_start:
                    try:
                        filter_start = parseDateUI(filter_start)
                        setSession(SESSION_FILTER_START_KEY, filter_start)
                    except ValueError, inst:
                        errors.append('Errror parsing filter start time: %s'
                                % inst)
                        filter_start = getSession(SESSION_FILTER_START_KEY)
                else:
                    clearSession(SESSION_FILTER_START_KEY)
                    filter_start = None
            if filter_finish is not None:
                if filter_finish:
                    try:
                        filter_finish = parseDateUI(filter_finish)
                        setSession(SESSION_FILTER_FINISH_KEY, filter_finish)
                    except ValueError, inst:
                        errors.append('Errror parsing filter finish time: %s'
                                % inst)
                        filter_finish = getSession(SESSION_FILTER_FINISH_KEY)
                else:
                    clearSession(SESSION_FILTER_FINISH_KEY)
                    filter_finish = None
        elif recording_id and (set_filter_station or set_filter_title
                or set_filter_start or set_filter_finish):
            # handle requests to update the filter from a given recording
            recording = self._storage.getRecording(int(recording_id))
            if set_filter_station:
                filter_station = recording.station
                setSession(SESSION_FILTER_STATION_KEY, filter_station)
            elif set_filter_title:
                filter_title = recording.title
                setSession(SESSION_FILTER_TITLE_KEY, filter_title)
            elif set_filter_start:
                filter_start = recording.start_time.date()
                setSession(SESSION_FILTER_START_KEY, filter_start)
            elif set_filter_finish:
                filter_finish = recording.finish_time.date()
                setSession(SESSION_FILTER_FINISH_KEY, filter_finish)
        elif set_filter_start:
            filter_start = getSession(SESSION_DATE_KEY)
            setSession(SESSION_FILTER_START_KEY, filter_start)
        elif set_filter_finish:
            filter_finish = getSession(SESSION_DATE_KEY)
            setSession(SESSION_FILTER_FINISH_KEY, filter_finish)
        else:
            # clear all values
            filter_station = None
            filter_title = None
            filter_start = None
            filter_finish = None
        # fill in any unset values
        # getSession returns None when key is not found
        if not filter_station:
            filter_station = getSession(SESSION_FILTER_STATION_KEY)
        if not filter_title:
            filter_title = getSession(SESSION_FILTER_TITLE_KEY)
        if not filter_start:
            filter_start = getSession(SESSION_FILTER_START_KEY)
        if not filter_finish:
            filter_finish = getSession(SESSION_FILTER_FINISH_KEY)

        # handle requests to retrieve a recording
        if retrieve_recording and recording_id:
            # add the recording to the transfer queue
            recording = self._storage.getRecording(int(recording_id))
            if not recording.fileExists():
                self._transfer_manager.add(recording,
                        getSession(SESSION_STATION_ADDRESS_KEY))

        # handle requests to export a recording
        if export_recording and recording_id:
            # retrieve the recording
            recording = self._storage.getRecording(int(recording_id))
            # get the path to the file from the recording
            recording_filepath = os.path.realpath(recording.abspath)
            # get the name of the served file from the recording
            served_filename = os.path.basename(recording.path)

            # Some shenanigans here. We want to find out if a special export
            # is requested, meaning at least one of start and finish are inside
            # the recordings time range. The other can be also inside, or blank
            # or equal to the limit. If special export *is* needed we need to
            # get the start offset (if any) and the duration.
            # NOTE: we assume export start/finish are within the range
            # (this may mean we assume a 24hour wrap)
            try:
                export_start_time = getExportTime(export_start_time,
                        recording.start_time, recording.finish_time)
            except ValueError, inst:
                errors.append('invalid export start time: %s' % inst)
            try:
                export_finish_time = getExportTime(export_finish_time,
                        recording.start_time, recording.finish_time)
            except ValueError, inst:
                errors.append('invalid export finish time: %s' % inst)

            # bypass export if there were errors
            if not errors:
                created_subrange_file = False
                file_handle = None
                try:
                    # if sub-section of the recording is requested, generate it
                    if export_start_time or export_finish_time:
                        # create the subrange file and a special filename
                        # get a tempfilename
                        handle, recording_filepath = tempfile.mkstemp(
                                suffix=recording.extension)
                        created_subrange = True

                        # use its name as the destination for subrange file
                        recording.createSubRangeFile(recording_filepath,
                                export_start_time, export_finish_time)

                        # create a special name
                        base, ext = os.path.splitext(served_filename)
                        # use export times if defined, otherwise use recording's
                        served_filename = ('%s (%s to %s)%s' % (base,
                                formatTimeUI(export_start_time
                                        or recording.start_time,
                                        show_seconds=True),
                                formatTimeUI(export_finish_time
                                        or recording.finish_time,
                                        show_seconds=True),
                                ext))

                    # use cherrypy utility to push the file for download. This
                    # also means that we don't have to move the config file into
                    # the web-accessible filesystem hierarchy
                    return cherrypy.lib.static.serve_file(recording_filepath,
                            "application/x-download", "attachment",
                            served_filename)
                except Exception, inst:
                    errors.append('Error exporting recording: %s' % inst)
                finally:
                    if created_subrange_file:
                        # close the file handle
                        if file_handle:
                            os.close(file_handle)
                        # delete the file
                        if os.path.exists(recording_filepath):
                            os.remove(recording_filepath)

        # the rest are inter-related so set defaults (in decreasing priority)
        selected_recording = None
        selected_date = None
        selected_recordings = None

        # if setting recording id, then save it to session if we were passed one
        if set_recording_id and recording_id:
            setSession(SESSION_RECORD_ID_KEY, int(recording_id))

        # if we have a recording id, then get the record (& clear selected day)
        recording_id = getSession(SESSION_RECORD_ID_KEY)
        if recording_id:
            selected_recording = self._storage.getRecording(recording_id)

            # ensure record is valid (or remove session key)
            if not selected_recording:
                clearSession(SESSION_RECORD_ID_KEY)
        # only highlight selected date if it's specified (and no record is)
        else:
            if year and month and day:
                selected_date = datetime.date(int(year), int(month), int(day))
                setSession(SESSION_DATE_KEY, selected_date)
            else:
                selected_date = getSession(SESSION_DATE_KEY)

        if selected_date:
            # must convert generator to a list
            selected_recordings = list(self._storage.getRecordings(
                    date=selected_date))
        elif filter_start and filter_finish:
            # must convert generator to a list
            selected_recordings = list(self._storage.getRecordings(
                    station=filter_station, title=filter_title,
                    min_start_date=filter_start, max_finish_date=filter_finish))

        # if retrieve selection is requested, then add all selected recordings
        # to it that are not already in (and not already available)
        if retrieve_selection and selected_recordings:
            station_address = getSession(SESSION_STATION_ADDRESS_KEY)
            for recording in selected_recordings:
                if not recording.fileExists():
                    self._transfer_manager.add(recording, station_address)

        # if export selection is requested, then zip up all files and send them
        if export_selection and selected_recordings:
            try:
                with tempfile.NamedTemporaryFile(suffix='.zip') as tmp:
                    # the file transfer can take a long time; by default
                    # cherrypy limits responses to 300s; we increase it to 1h
                    cherrypy.response.timeout = 3600
                    archive = None
                    try:
                        # create the zipfile
                        archive = ZipFile(tmp.name, 'w')
                        # add all the recording files
                        for recording in selected_recordings:
                            archive.write(recording.abspath, recording.path)
                    finally:
                        # safer this way
                        if archive:
                            archive.close()

                    # create a suitable name for the export
                    if selected_date:
                        served_filename = ('Recordings_%s.zip'
                                % formatDateIso(selected_date))
                    else:
                        served_filename = ('Recordings_%s_to_%s.zip'
                                % (formatDateIso(filter_start),
                                formatDateIso(filter_finish)))

                    # use cherrypy utility to push the file for download. This
                    # also means that we don't have to move the config file into
                    # the web-accessible filesystem hierarchy
                    return cherrypy.lib.static.serve_file(tmp.name,
                            "application/x-download", "attachment",
                            served_filename)
            except Exception, inst:
                errors.append('Error exporting recordings: %s' % inst)

        # determine which days to highlight
        # always show today
        today = datetime.date.today()

        # "go to" buttons trump other month/year settings
        if go_to_start and filter_start:
            year = filter_start.year
            month = filter_start.month
            setSession(SESSION_YEAR_KEY, year)
            setSession(SESSION_MONTH_KEY, month)
        elif go_to_finish and filter_finish:
            year = filter_finish.year
            month = filter_finish.month
            setSession(SESSION_YEAR_KEY, year)
            setSession(SESSION_MONTH_KEY, month)
        elif go_to_today:
            year = today.year
            month = today.month
            setSession(SESSION_YEAR_KEY, year)
            setSession(SESSION_MONTH_KEY, month)
        else:
            # parse selected year (and check sessions)
            if year:
                year = int(year)
                setSession(SESSION_YEAR_KEY, year)
            else:
                year = getSession(SESSION_YEAR_KEY) or today.year

            # parse selected month (and check sessions)
            if month:
                month = int(month)
                setSession(SESSION_MONTH_KEY, month)
            else:
                month = getSession(SESSION_MONTH_KEY) or today.month

        # get the station titles for the filter, and add a "show all" option
        station_names = [(NO_FILTER_STATION, '')]
        station_names.extend(((title, title) for title in
                self._storage.getRecordingStations()))

        # get the recording titles for the filter, and add a "show all" option
        schedule_titles = [(NO_FILTER_TITLE, '')]
        schedule_titles.extend(((title, title) for title in
                self._storage.getRecordingTitles()))

        station = getSession(SESSION_STATION_NAME_KEY)
        transfer_queue_ids = self._transfer_manager.queue_ids

        selected_action_mode, selected_action_size = calculateActionForSelected(
                selected_recordings, station, transfer_queue_ids)

        if view == 'month':
            calendar = RecordingsHTMLCalendar(year, month, today, self._storage,
                    filter_station, filter_title, filter_start, filter_finish,
                    selected_date, selected_recording).showMonth()
        else:
            calendar = ('<h2>Unimplemented</h2>'
                    'That calendar format is not supported')

        return template.render(is_proxy=True,
                station=station, errors=errors,
                station_names=station_names, schedule_titles=schedule_titles,
                filter_station=filter_station, filter_title=filter_title,
                filter_start=formatDateUI(filter_start),
                filter_finish=formatDateUI(filter_finish),
                calendar=genshi.HTML(calendar), selected_date=selected_date,
                selected_recording=selected_recording,
                selected_recordings=selected_recordings,
                selected_action_mode=selected_action_mode,
                selected_action_size=selected_action_size,
                transfer_queue_ids=transfer_queue_ids)


    @cherrypy.expose
    @template.output('export.html')
    def export(self, filter_start=None, filter_finish=None, filter_station=None,
            export_partial=None, start_type=None, start_time=None,
            duration=None, filter_title=None, export=None,
            sync_with_station=False, **ignored):
        # HACK: this helps development slightly
        if ignored:
            print "IGNORED",ignored

        errors = []

        # handle requests to sync remote bowerbird by adding any missing
        # recording files to the transfer queue
        if sync_with_station and hasSession(SESSION_STATION_ADDRESS_KEY):
            station_address = getSession(SESSION_STATION_ADDRESS_KEY)
            for recording in self._storage.getRecordings(
                    station=getSession(SESSION_STATION_NAME_KEY)):
                if not recording.fileExists():
                    self._transfer_manager.add(recording, station_address)

        if export:
            try:
                start_date_ = parseDateUI(filter_start)
            except ValueError:
                errors.append('invalid start date: "%s", must be DD/MM/YYYY'
                        % filter_start)
            try:
                finish_date_ = parseDateUI(filter_finish)
            except ValueError:
                errors.append('invalid finish date: "%s", must be DD/MM/YYYY'
                        % filter_finish)

            if export_partial and int(export_partial):
                try:
                    start_time_ = parseTimeUI(start_time)
                except ValueError:
                    errors.append('invalid start time: "%s", must be HH[:MM[:SS]]'
                            % start_time)
                try:
                    duration_ = parseTimeDelta(duration)
                except ValueError:
                    errors.append('invalid duration: "%s", must be [[HH:]MM:]SS'
                            % duration)
            errors.append('Sorry: export is not implemented yet')

        # get the station titles for the filter, and add a "show all" option
        station_names = [(NO_FILTER_STATION, '')]
        station_names.extend(((title, title) for title in
                self._storage.getRecordingStations()))

        # get the recording titles for the filter, and add a "show all" option
        schedule_titles = [(NO_FILTER_TITLE, '')]
        schedule_titles.extend(((title, title) for title in
                self._storage.getRecordingTitles()))

        return template.render(is_proxy=True,
                transfer_queue_ids=self._transfer_manager.queue_ids,
                station=getSession(SESSION_STATION_NAME_KEY), errors=errors,
                filter_start=filter_start, filter_finish=filter_finish,
                filter_station=filter_station, station_names=station_names,
                export_partial=export_partial, start_type=start_type,
                start_time=start_time, duration=duration,
                filter_title=filter_title, schedule_titles=schedule_titles)


    @cherrypy.expose
    @template.output('queue.html')
    def queue(self, recording_id=None, cancel_transfer=None,
            cancel_all_transfers=None, **ignored):
        # HACK: this helps development slightly
        if ignored:
            print "IGNORED",ignored

        if cancel_all_transfers:
            self._transfer_manager.clear()
            raise cherrypy.HTTPRedirect('/recordings')

        if recording_id and cancel_transfer:
            self._transfer_manager.remove(int(recording_id))

        return template.render(is_proxy=True,
                station=getSession(SESSION_STATION_NAME_KEY),
                transfer_queue=self._transfer_manager.queue)


    def findLocalBowerbirds(self, rescan=False):
        if rescan:
            self._scanner.rescan()

        bowerbirds = []
        for service in self._scanner:
            if (service.has_key('magic') and service['magic']
                    == ZEROCONF_TEXT_TO_IDENTIFY_BOWERBIRD):
                name = service['name']
                # strip the wrapper from the name
                name = name[name.find('[')+1 : name.rfind(']')]
                bowerbirds.append({'name': name, 'address': service['address']})

        return bowerbirds


    def getRemoteName(self, address):
        return urllib2.urlopen('http://%s:%s/name' %
                (address, self.bowerbird_port)).read()


    def assertConnected(self):
        '''Asserts that a connection to a Bowerbird system exists.
           Redirects to connect page if not connected'''
        if not hasSession(SESSION_STATION_ADDRESS_KEY):
            raise cherrypy.HTTPRedirect('/connect')


    # update the html from a remote Bowerbird to add disconnect links etc
    # NOTE: html_source should be a file-like object with a "read" method
    def updateHtmlFromBowerbird(self, html_source):
        # create a dom object from the html
        dom = lxml.html.parse(html_source)
        # find the ul in the menu div
        ul = dom.find('//div[@id="menu"]/ul')

        # create disconnect li
        d_li = lxml.html.Element('li')
        # add new hyperlink and text to the li
        lxml.etree.SubElement(d_li, 'a', href='/disconnect').text = "Disconnect"
        # add the new li to the ul
        ul.insert(0, d_li)

        # add queue link if there's transfers going on
        if self._transfer_manager.queue_ids:
            # create queue li
            q_li = lxml.html.Element('li')
            # add new hyperlink and text to the li
            lxml.etree.SubElement(q_li, 'a', href='/queue').text = "Transfers"
            # add the new li to the ul
            ul.append(q_li)

        # return the modified html
        return lxml.etree.tostring(dom, method='html', pretty_print=True)


    @template.output('connection_failure.html')
    def showConnectionFailure(self, error='was lost', name=None, address=None):
        if address:
            station_address = address
        else:
            station_address = getSession(SESSION_STATION_ADDRESS_KEY)
            clearSession(SESSION_STATION_ADDRESS_KEY)

        if name:
            station_name = name
        else:
            station_name = getSession(SESSION_STATION_NAME_KEY)
            clearSession(SESSION_STATION_NAME_KEY)

        return template.render(is_proxy=True, station=None, error=error,
                station_name=station_name, station_address=station_address,
                timeout=CONNECT_FAILURE_REDIRECTION_TIMEOUT)


    def tunnelConnectionToBowerbird(self, page, kwargs):
        '''tunnel connection back from remote Bowerbird system'''

        self.assertConnected()

        try:
            return urllib2.urlopen(
                    'http://%s:%s/%s' %
                    (getSession(SESSION_STATION_ADDRESS_KEY),
                    self.bowerbird_port, page), urlencode(kwargs))
        except urllib2.URLError:
            return self.showConnectionFailure()


    def updateRecordingsFromBowerbird(self, **kwargs):
        '''request all recording metadata from connected Bowerbird.'''

        self.assertConnected()

        # add the recordings that aren't already there
        # first build a set of hashes of existing recordings
        recordings = self._storage.getRecordings()
        hashes = frozenset(recording.hash for recording in recordings)
        checksums = frozenset(recording.checksum for recording in recordings)
        current_station = getSession(SESSION_STATION_NAME_KEY)
        for recording in jsonpickle.decode(self.tunnelConnectionToBowerbird(
                'recordings_json', kwargs).read()):
            # this must be set because the station doesn't store it
            recording.station = current_station
            if recording.hash not in hashes:
                self._storage.addRecording(recording)
            elif recording.checksum not in checksums:
                self._storage.updateRecording(recording)


def loadWebConfig():
    # load config from file
    cherrypy.config.update(WEB_CONFIG)

    # set static directories to be relative to script and enable sessions
    cherrypy.config.update({
            'tools.staticdir.root':    os.path.abspath(os.path.dirname(__file__)),
            'tools.sessions.on': True
            })

    for key in REQUIRED_KEYS:
        if not cherrypy.config.has_key(key):
            sys.stderr.write(
                    'Web config file "%s" is missing definition for "%s"\n'
                    % (WEB_CONFIG, key))
            return False

    return True


def main(args):
    # check minimum settings are specified in config
    if not loadWebConfig():
        sys.exit(1)

    if cherrypy.config.has_key(BOWERBIRD_PORT_KEY):
        bowerbird_port = cherrypy.config[BOWERBIRD_PORT_KEY]
    else:
        bowerbird_port = DEFAULT_BOWERBIRD_PORT

    path = os.path.dirname(os.path.realpath(args[0]))
    database_file = cherrypy.config[DATABASE_KEY]
    root_dir = cherrypy.config[ROOT_DIR_KEY]
    extension = cherrypy.config[EXTENSION_KEY]
    if cherrypy.config.has_key(LOCALNET_SCAN_TIME_MS_KEY):
        localnet_scan_time_ms = cherrypy.config[LOCALNET_SCAN_TIME_MS_KEY]
    else:
        localnet_scan_time_ms = DEFAULT_LOCALNET_SCAN_TIME_MS
    if cherrypy.config.has_key(CONCURRENT_TRANSFERS_KEY):
        concurrent_transfers = cherrypy.config[CONCURRENT_TRANSFERS_KEY]
    else:
        concurrent_transfers = DEFAULT_CONCURRENT_TRANSFERS
    if cherrypy.config.has_key(TRANSFER_BANDWIDTH_KEY):
        transfer_bandwidth = cherrypy.config[TRANSFER_BANDWIDTH_KEY]
    else:
        transfer_bandwidth = DEFAULT_TRANSFER_BANDWIDTH
    if cherrypy.config.has_key(TRANSFER_CHUNK_SIZE_KEY):
        transfer_chunk_size = cherrypy.config[TRANSFER_CHUNK_SIZE_KEY]
    else:
        transfer_chunk_size = DEFAULT_TRANSFER_CHUNK_SIZE

    try:
        cherrypy.tree.mount(Root(path, database_file, root_dir, extension,
                bowerbird_port, localnet_scan_time_ms, concurrent_transfers,
                transfer_bandwidth, transfer_chunk_size),
                '/', { '/media': {
                    'tools.staticdir.on': True,
                    'tools.staticdir.dir': 'static'
                }})
    except IOError as e:
        sys.stderr.write('Error: %s.\n' % e);
        sys.exit(1)

    cherrypy.engine.start()
    cherrypy.engine.block()


if __name__ == '__main__':
    main(sys.argv)
