import sys, os, os.path, errno, cherrypy, datetime, subprocess, genshi, tempfile
import json
from zipfile import ZipFile
from cherrypy.lib import sessions

from bowerbird.common import *
from bowerbird.configparser import ConfigParser
from bowerbird.scheduleparser import ScheduleParser, RecordingSpec
from bowerbird.storage import BowerbirdStorage, UNTITLED

from lib import ajax, template, jsonpickle
from lib.common import *
from lib.recordingshtmlcalendar import RecordingsHTMLCalendar
from lib.sonogram import generateSonogram
from lib.zeroconfservice import ZeroconfService

# constants
RECORDING_KB_PER_SECOND = 700

# web interface config file
WEB_CONFIG = 'bowerbird.conf'
SERVER_PORT_KEY = 'server.socket_port'

# parameters for parsing contents of web interface config file
CONFIG_KEY = 'bowerbird_config'
CONFIG_DEFAULTS_KEY = 'bowerbird_config_defaults'
SCHEDULE_KEY = 'bowerbird_schedule'
DATABASE_KEY = 'database'
REQUIRED_KEYS = [CONFIG_KEY, DATABASE_KEY]
CONFIG_CACHE_PATH = 'config/current_config'

# general configuration
FREQUENCY_SCALES = ['Linear', 'Logarithmic']
DEFAULT_FFT_STEP = 256 # in milliseconds
SONOGRAM_DIRECTORY = os.path.join("static", "sonograms")


class Root(object):
    def __init__(self, path, database_file):
        self.path = path

        # parse config file(s)
        config_filename = cherrypy.config[CONFIG_KEY]
        if not os.path.exists(config_filename):
            raise IOError(errno.ENOENT, "Config file '%s' not found"
                    % config_filename)
        if cherrypy.config.has_key(CONFIG_DEFAULTS_KEY):
            defaults_filename = cherrypy.config[CONFIG_DEFAULTS_KEY]
            # if defaults file doesn't exist, then don't use it
            if not os.path.exists(defaults_filename):
                sys.stderr.write("Warning: configured defaults file "
                        " '%s' doesn't exist\n" % defaults_filename)
                defaults_filename = None
        else:
            defaults_filename = None
        self._config = ConfigParser(config_filename, defaults_filename)

        # initialise the schedule manager
        schedule_filename = cherrypy.config[SCHEDULE_KEY]
        self._schedule = ScheduleParser(config_filename, schedule_filename)

        # initialise storage
        self._storage = BowerbirdStorage(database_file, self._config,
                self._schedule)


    @cherrypy.expose
    def index(self, **ignored):
        # just redirect to status page
        raise cherrypy.HTTPRedirect('/status')


    @cherrypy.expose
    def name(self, **ignored):
        return self.getStationName()


    @cherrypy.expose
    @template.output('status.html')
    def status(self, **ignored):
        return template.render(station = self.getStationName(),
                uptime = self.getUptime(),
                disk_space = self.getDiskSpace(),
                local_time = self.getLocalTime(),
                last_recording = self.getLastRecording(),
                next_recording = self.getNextRecording())


    @cherrypy.expose
    @template.output('config.html')
    def config(self, config_timestamp=0, load_defaults=False, cancel=False,
            apply=False, export_config=False, new_config=None,
            import_config=False, **data):
        error = None
        values = None

        if cancel:
            raise cherrypy.HTTPRedirect('/')
        elif export_config:
            # use cherrypy utility to push the file for download. This also
            # means that we don't have to move the config file into the
            # web-accessible filesystem hierarchy
            return cherrypy.lib.static.serve_file(
                    os.path.realpath(self._config.filename),
                    "application/x-download", "attachment",
                    os.path.basename(self._config.filename))
        elif apply:
            # if someone else has modified the config, then warn the user
            # before saving their changes (overwriting the other's changes)
            if int(config_timestamp) == self._config.getTimestamp():
                self.updateConfigFromPostData(self._config, data)

                # update file
                try:
                    self._config.saveToFile()
                    self._config.exportForShell(self._config.filename + ".sh")
                    # bounce back to homepage
                    raise cherrypy.HTTPRedirect('/')
                except IOError as e:
                    # if save failed, put up error message and stay on the page
                    error = 'Error saving: %s' % e
            else:
                error = genshi.HTML('''WARNING:
                        Configuration has been changed externally.<br />
                        If you wish to keep your changes and lose the external
                        changes, then click on 'Apply' again. <br />
                        To lose your changes and preserve the external changes,
                        click 'Cancel'.''')
                # load the post data into a temporary configparser to preserve
                # the user's changes when the page is loaded again. This means
                # we don't have to duplicate the horrible POST-to-config code
                temp_conf = ConfigParser()
                self.updateConfigFromPostData(temp_conf, data)
                values = temp_conf.getValues()
                # the page loading below will send the new config timestamp so
                # we don't have to anything else here.

        if load_defaults:
            values = self._config.getDefaultValues()
        elif import_config:
            if new_config.filename:
                try:
                    values = self._config.parseFile(new_config.file)
                except Exception as e:
                    values = None
                    error = 'Unable to parse config file: "%s"' % e
            else:
                error = 'No filename provided for config import'

        if not values:
            values = self._config.getValues()
        return template.render(station=self.getStationName(),
                config_timestamp=self._config.getTimestamp(),
                error=error, using_defaults=load_defaults, values=values,
                file=self._config.filename,
                defaults_file=self._config.defaults_filename)


    @cherrypy.expose
    @template.output('schedule.html')
    def schedule(self, cancel=None, apply=None, add=None, **data):
        error = None
        if cancel:
            raise cherrypy.HTTPRedirect('/')
        elif apply:

            # use dictionary so we can untangle the POST data
            schedules = {}
            # we must check for uniqueness of titles
            titles = set()
            # just get the titles
            for key in data:
                # each schedule comes in three parts: ?.title, ?.start, ?.finish
                if key.endswith('title'):
                    id = key.split('.')[0]
                    title = data[key]
                    if title in titles:
                        error = ('Schedule titles must be unique. Found '
                                'duplicate of "%s"' % title)
                        return template.render(station=self.getStationName(),
                                recording_specs=self._schedule.getScheduleSpecs(),
                                error=error, file=self._schedule.filename, add=None)
                    schedules[id] = title
                    titles.add(title)

            # clear all schedules
            # and add them back in the order they were on the webpage
            self._schedule.clearScheduleSpecs()

            # sort the labels by their id, then add them in that order
            schedule_ids = schedules.keys()
            schedule_ids.sort()
            for id in schedule_ids:
                start_key = "%s.start" % id
                finish_key = "%s.finish" % id
                if data.has_key(start_key) and data.has_key(finish_key):
                    self._schedule.setScheduleSpec(
                            ScheduleParser.parseRecordingSpec(schedules[id],
                            data[start_key], data[finish_key]))

            # update file
            try:
                self._schedule.saveToFile()
                # bounce back to homepage
                raise cherrypy.HTTPRedirect('/')
            except IOError as e:
                # if save failed, put up error message and stay on the page
                error = "Error saving: %s" % e

        elif not add:
            # this should only happen when a remove button has been clicked
            # find the remove key
            for key in data:
                if key.endswith('remove'):
                    # get the schedule key from the title
                    id = key.split('.')[0]
                    title_key = "%s.title" % id
                    if data.has_key(title_key):
                        schedule_key = data[title_key]
                        self._schedule.deleteScheduleSpec(schedule_key)

        return template.render(station=self.getStationName(), error=error,
                recording_specs=self._schedule.getScheduleSpecs(), add=add,
                file=self._schedule.filename)


    @cherrypy.expose
    @template.output('recordings.html')
    def recordings(self, view='month', year=None, month=None, day=None,
            recording_id=None, set_recording_id=False, clear_recording_id=False,
            go_to_start=False, go_to_finish=False, go_to_today=False,
            filter_title=None, filter_start=None, filter_finish=None,
            update_filter=None, reset_filter=None, clear_selected_date=False,
            set_filter_title=None, set_filter_start=None,
            set_filter_finish=None, export_recording=False,
            export_start_time=None, export_finish_time=None,
            export_selection=False, delete_cache=False,
            **ignored):
        # HACK: this helps development slightly
        if ignored:
            print "IGNORED",ignored

        # initialise error list
        errors = []

        if delete_cache:
            # Clear all recordings. The cron job will do the scanning/refilling.
            self._storage.clearRecordings()
            # Clear all recording file entries
            self._storage.clearRecordingFiles()

        if reset_filter:
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
        elif recording_id and (set_filter_title or set_filter_start
                or set_filter_finish):
            # handle requests to update the filter from a given recording
            recording = self._storage.getRecording(int(recording_id))
            if set_filter_title:
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
            filter_title = None
            filter_start = None
            filter_finish = None
        # fill in any unset values
        # getSession returns None when key is not found
        if not filter_title:
            filter_title = getSession(SESSION_FILTER_TITLE_KEY)
        if not filter_start:
            filter_start = getSession(SESSION_FILTER_START_KEY)
        if not filter_finish:
            filter_finish = getSession(SESSION_FILTER_FINISH_KEY)

        # handle requests to export a recording
        if export_recording and recording_id:
            # get the name of the served file from the recording
            recording = self._storage.getRecording(int(recording_id))
            recording_filepath = os.path.realpath(recording.abspath)
            served_filename = os.path.basename(recording.path)

            # Some shenanigans here. We want to find out if a special export
            # is requested, meaning at least one of start and finish are inside
            # the recordings time range. The other can be also inside, or blank
            # or equal to the limit. If special export *is* needed we need to
            # get the start offset (if any) and the duration.
            # NOTE: we assume export start/finish are within the range (this may
            # mean we assume a 24hour wrap)
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
                    title=filter_title,
                    min_start_date=filter_start, max_finish_date=filter_finish))

        # if export selection is requested, then zip up all files and send them
        if export_selection and selected_recordings:
            try:
                with tempfile.NamedTemporaryFile(suffix='.zip') as tmp:
                    # the file transfer can take a long time; by default
                    # cherrypy limits responses to 300s; we increase it to 1h
                    cherrypy.response.timeout = 3600
                    try:
                        # create the zipfile
                        archive = ZipFile(tmp.name, 'w')
                        # add all the recording files
                        for recording in selected_recordings:
                            # ignore missing files
                            if recording.fileExists():
                                archive.write(recording.abspath, recording.path)
                    finally:
                        # safer this way
                        archive.close()

                    # create a suitable name for the export
                    if selected_date:
                        served_filename = ('Recordings_%s.zip'
                                % formatDateIso(selected_date))
                    elif filter_start == filter_finish:
                        served_filename = ('Recordings_%s.zip'
                                % formatDateIso(filter_start))
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
                errors.append('Error exporting recordings: %s (%s)' % (inst,
                        type(inst)))

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

        # get the recording titles for the filter, and add a "show all" option
        schedule_titles = [(NO_FILTER_TITLE, '')]
        schedule_titles.extend(((title, title) for title in
                self._storage.getRecordingTitles()))

        if view == 'month':
            calendar = RecordingsHTMLCalendar(year, month, today, self._storage,
                    filter_title, filter_start, filter_finish, selected_date,
                    selected_recording).showMonth()
        else:
            calendar = ('<h2>Unimplemented</h2>'
                    'That calendar format is not supported')
        return template.render(station=self.getStationName(), errors=errors,
                schedule_titles=schedule_titles, filter_title=filter_title,
                filter_start=formatDateUI(filter_start),
                filter_finish=formatDateUI(filter_finish),
                calendar=genshi.HTML(calendar), selected_date=selected_date,
                selected_recording=selected_recording,
                selected_recordings=selected_recordings)


    @cherrypy.expose
    def recordings_json(self):
        return jsonpickle.encode(list(self._storage.getRecordings()))


    @cherrypy.expose
    def recording_by_hash(self, hash=None):
        if not hash:
            print 'You must provide the hash of the desired recording in the POST or GET data.'
            raise cherrypy.HTTPError(404, 'You must provide the hash of the '
                    'desired recording in the POST or GET data.')

        print 'searching for recording with hash "%s"' % hash
        station_name = self.getStationName()
        for recording in self._storage.getRecordings():
            # must fill in station name
            recording.station = station_name
            if recording.hash == hash:
                # use cherrypy utility to push the file for download. This
                # also means that we don't have to move the config file into
                # the web-accessible filesystem hierarchy
                return cherrypy.lib.static.serve_file(recording.abspath,
                        "application/x-download", "attachment",
                        os.path.basename(recording.path))

        print 'The provided recording hash does not match any of the stored recordings.'
        raise cherrypy.HTTPError(404, 'The provided recording hash does not '
                'match any of the stored recordings.')


#    @cherrypy.expose
    @template.output('categories.html')
    def categories(self, sort='label', sort_order='asc', **ignored):
        categories = self._storage.getCategories(sort, sort_order)
        return template.render(station=self.getStationName(),
                categories=categories,
                sort=sort, sort_order=sort_order)


#    @cherrypy.expose
    @template.output('category.html')
    def category(self, label=None, new_label=None, update_details=None,
            sort='date_and_time', sort_order='asc', **ignored):
        if update_details and new_label and new_label != label:
            self._storage.updateCategory(label, new_label)
            raise cherrypy.HTTPRedirect('/category/%s' % new_label)

        calls = self._storage.getCalls(sort, sort_order, label)
        call_sonograms = {}
        for call in calls:
            call_sonograms[call['filename']] = self.getSonogram(call,
                    FREQUENCY_SCALES[0], DEFAULT_FFT_STEP)
        return template.render(station=self.getStationName(),
                category=self._storage.getCategory(label),
                calls=calls, call_sonograms=call_sonograms, sort=sort,
                sort_order=sort_order)


#    @cherrypy.expose
    @template.output('calls.html')
    def calls(self, sort='date_and_time', sort_order='asc', category=None,
            **ignored):
        return template.render(station=self.getStationName(),
                calls=self._storage.getCalls(sort, sort_order, category),
                sort=sort, sort_order=sort_order)


#    @cherrypy.expose
    @template.output('call.html')
    def call(self, call_filename=None,
            update_details=None, label=None, category=None, example=None,
            update_sonogram=None, frequency_scale=FREQUENCY_SCALES[0],
            fft_step=DEFAULT_FFT_STEP,
            **ignored):
        if not call_filename:
            raise cherrypy.NotFound()

        fft_step = int(fft_step)

        if call_filename and update_details:
            self._storage.updateCall(call_filename, label, category, example)
            if ajax.isXmlHttpRequest():
                return # ajax update doesn't require a response

        call=self._storage.getCall(call_filename)
        if ajax.isXmlHttpRequest():
            # only sonogram updates are possible here
            assert(update_sonogram)
            return template.render('_sonogram.html', ajah=True,
                    sonogram_filename=self.getSonogram(call, frequency_scale,
                            fft_step), fft_step=fft_step,
                    frequency_scale=frequency_scale,
                    frequency_scales=FREQUENCY_SCALES)
        else:
            return template.render(station=self.getStationName(), call=call,
                    categories=",".join(self._storage.getCategoryNames()),
                    sonogram_filename=self.getSonogram(call, frequency_scale,
                            fft_step), fft_step=fft_step,
                    frequency_scale=frequency_scale,
                    frequency_scales=FREQUENCY_SCALES,
                    prev_next_files=self._storage.getPrevAndNextCalls(call))


    def getStationName(self):
        return self._config.getValue2(STATION_SECTION_NAME, STATION_NAME_KEY)


    def updateConfigFromPostData(self, config, data):
        # clear out the configuration and re-populate it
        config.clearConfig()

        # parse the data, then sort it so it can be entered in the same order we
        # sent it (to preserve the order that gets mixed up by the POST data)
        sections = {}
        keys = {}
        values = {}
        for key in data:
            if key.startswith(META_PREFIX):
                split_data = data[key].split(',')
                section_index = int(split_data[0])
                name_index = int(split_data[1])
                subname_index = int(split_data[2])
                value = ','.join(split_data[3:])
                real_key = key[len(META_PREFIX):]
                if not keys.has_key(section_index):
                    keys[section_index] = {}
                if not keys[section_index].has_key(name_index):
                    keys[section_index][name_index] = {}
                keys[section_index][name_index][subname_index] = (real_key,
                        value)
            elif key.startswith(SECTION_META_PREFIX):
                index = data[key].find(',')
                id = int(data[key][:index])
                value = data[key][index+1:]
                real_key = key[len(SECTION_META_PREFIX):]
                sections[id] = (real_key, value)
            else:
                values[key] = data[key]

        # first insert the sections in sorted order
        section_indicies = sections.keys()
        section_indicies.sort()
        for section_index in section_indicies:
            key, value = sections[section_index]
            config.setSectionMeta(key, value)

        # next insert the key meta data in sorted order
        section_indicies = keys.keys()
        section_indicies.sort()
        for section_index in section_indicies:
            name_indicies = keys[section_index].keys()
            name_indicies.sort()
            for name_index in name_indicies:
                subname_indicies = keys[section_index][name_index].keys()
                subname_indicies.sort()
                for subname_index in subname_indicies:
                    key, value = keys[section_index][name_index][subname_index]
                    config.setMeta1(key, value)

        # now set the actual values
        for key in values:
            config.setValue1(key, values[key])


    def getSonogram(self, call, frequency_scale, fft_step):
        if call:
            destination_dir = os.path.abspath(SONOGRAM_DIRECTORY)
            return '/media/sonograms/%s' % generateSonogram(call['filename'],
                    destination_dir, frequency_scale, fft_step)
        return ''


    def getUptime(self):
        return subprocess.Popen("uptime", stdout=subprocess.PIPE).communicate(
                )[0]


    def getDiskSpace(self):
        root_dir = self._storage.getRootDir()
        if not os.path.exists(root_dir):
            return ("%s doesn't exist: Fix Config %s->%s"
                    % (root_dir, CAPTURE_SECTION_NAME, CAPTURE_ROOT_DIR_KEY))
        # split to remove title line, then split into fields
        (_,_,_,available,percent,_) = subprocess.Popen(["df", "-k", root_dir],
                stdout=subprocess.PIPE).communicate()[0].split('\n')[1].split()
        percent_free = 100 - int(percent[:-1])
        available = int(available)
        return ("%s free (%d%%) Approx. %s recording time left"
                % (formatSize(available), percent_free,
                        formatTimeLong(available/RECORDING_KB_PER_SECOND)))


    def getLocalTime(self):
        now = datetime.datetime.now()
        # TODO add info about sunrise/set relative time
        # "18:36 (1 hour until sunset), 01 February 2010"
        return now.strftime('%H:%M, %d %B %Y')


    def getLastRecording(self):
        # find the most recent recording in the database?
        # or should we scan the scheduled recordings and assume they occurred
        last = self._storage.getRecordingBefore(datetime.datetime.now())
        if last:
            return '%s: %s %s to %s' % (last.title,
                    formatDateUI(last.start_date),
                    formatTimeUI(last.start_time, compact=True),
                    formatTimeUI(last.finish_time, compact=True))
        return 'No previous recordings'

    def getNextRecording(self):
        now = datetime.datetime.now()
        # times come out sorted, so just return the first one in the future
        # TODO: this should be moved into ScheduleParser
        for schedule in self._schedule.getSchedules(
                days_to_schedule=2):
            if schedule.start > now:
                return '%s: %s %s to %s' % (schedule.title,
                        formatDateUI(schedule.start),
                        formatTimeUI(schedule.start, compact=True),
                        formatTimeUI(schedule.finish, compact=True))

        return 'No recordings scheduled'


def loadWebConfig():
    # load config from file
    cherrypy.config.update(WEB_CONFIG)

    # set static directories to be relative to script
    cherrypy.config.update({
            'tools.staticdir.root': os.path.abspath(os.path.dirname(__file__)),
            'tools.sessions.on': True
            })

    # increase server socket timeout to 60s; we are more tolerant of bad
    # quality client-server connections (cherrypy's defult is 10s)
    cherrypy.server.socket_timeout = 60

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

    database_file = cherrypy.config[DATABASE_KEY]
    path = os.path.dirname(os.path.realpath(args[0]))

    try:
        root = Root(path, database_file)
        cherrypy.tree.mount(root, '/', {
            '/media': {
                'tools.staticdir.on': True,
                'tools.staticdir.dir': 'static'
            }
        })
    except IOError as e:
        sys.stderr.write('Error: %s.\n' % e);
        sys.exit(1)

    # create zeroconf service
    # TODO handle if avahi and/or dbus is not working
    service = ZeroconfService(name="Bowerbird [%s]" % root.getStationName(),
            port=cherrypy.config[SERVER_PORT_KEY], stype=ZEROCONF_TYPE,
            magic=ZEROCONF_TEXT_TO_IDENTIFY_BOWERBIRD)

    cherrypy.engine.start()
    service.publish()

    cherrypy.engine.block()

    service.unpublish()


if __name__ == '__main__':
    main(sys.argv)
