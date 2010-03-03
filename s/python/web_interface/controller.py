import sys, os, os.path, errno, cherrypy, datetime, subprocess, tempfile, genshi
import re
from cherrypy.lib import sessions

from lib import common, ajax, template
from lib.storage import BowerbirdStorage, UNTITLED
from lib.recordingshtmlcalendar import RecordingsHTMLCalendar
from lib.sonogram import generateSonogram
from lib.configparser import ConfigParser
from lib.zeroconfservice import ZeroconfService
from bowerbird.scheduleparser import ScheduleParser, RecordingSpec

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

# cherrypy sessions for recordings page
SESSION_YEAR_KEY = 'year'
SESSION_MONTH_KEY = 'month'
SESSION_FILTER_TITLE_KEY = 'filter_title'
SESSION_FILTER_START_KEY = 'filter_start'
SESSION_FILTER_FINISH_KEY = 'filter_finish'
SESSION_DATE_KEY = 'date'
SESSION_RECORD_ID_KEY = 'record_id'

NO_FILTER_TITLE = 'All Recordings'

SOX_PATH = '/usr/bin/sox'

DATETIME_RE = re.compile(
		'^([0-9]+)/([0-9]+)/([0-9]+)(?: ([0-9]+):([0-9]+)){0,1}$')
DATETIME_UI_FORMAT = '%d/%m/%Y %H:%M'


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
			# clear all schedules
			# and add them back in the order they were on the webpage
			self._schedule.clearScheduleSpecs()
			schedules = {}

			# just get the labels
			for key in data:
				# each schedule comes in three parts: ?.title, ?.start, ?.finish
				if key.endswith('title'):
					id = key.split('.')[0]
					schedules[id] = data[key]

			# sort the labels by their id, then add them in that order
			schedule_ids = schedules.keys()
			schedule_ids.sort()
			for id in schedule_ids:
				start_key = "%s.start" % id
				finish_key = "%s.finish" % id
				if data.has_key(start_key) and data.has_key(finish_key):
					schedule_title = schedules[id]
					schedule_value = ("%s - %s"
							% (data[start_key], data[finish_key]))
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

		recording_specs = self._schedule.getScheduleSpecs()

		return template.render(station=self.getStationName(), error=error,
				recording_specs=recording_specs, add=add,
				file=self._schedule.filename)


	@cherrypy.expose
	@template.output('recordings.html')
	def recordings(self, view='month', year=None, month=None, day=None,
			recording_id=None, set_recording_id=False, go_to_today=False,
			filter_title=None, filter_start=None, filter_finish=None,
			update_filter=None, reset_filter=None, clear_selected_date=False,
			set_filter_title=None, set_filter_start=None,
			set_filter_finish=None, export_recording=False, rescan_disk=False,
			**ignored):
		# HACK: this helps development slightly
		if ignored:
			print "IGNORED",ignored

		# initialise error list
		errors = []

		if reset_filter:
			filter_title = None
			clearSession(SESSION_FILTER_TITLE_KEY)
			filter_start = None
			clearSession(SESSION_FILTER_START_KEY)
			filter_finish = None
			clearSession(SESSION_FILTER_FINISH_KEY)

		if export_recording and recording_id:
			# convert id to int
			recording_id = int(recording_id)

			# cache for compactness
			extension = self._storage.recording_ext

			# concatenate all the files that make up this recording and send it
			handle, recording_filename = tempfile.mkstemp(suffix=extension)
			# close the file (because sox is going to play with it directly)
			os.close(handle)
			# build the command list (this is how subprocess wants it)
			command_list = [SOX_PATH]
			command_list.extend(self._storage.getFilesForRecordingAbsolute(
					recording_id))
			command_list.append(recording_filename)
			t = datetime.datetime.today()
			subprocess.check_call(command_list)
			print "sox took %s" % str(datetime.datetime.today() - t)

			# create the name of the served file from the recording
			recording = self._storage.getRecording(recording_id)
			served_filename = '%s %s%s' % (recording.title,
					recording.start_time.strftime('%Y/%m/%d %H:%M'),
					extension)

			# use cherrypy utility to push the file for download. This also
			# means that we don't have to move the config file into the
			# web-accessible filesystem hierarchy
			return cherrypy.lib.static.serve_file(
					os.path.realpath(recording_filename),
					"application/x-download", "attachment", served_filename)

		if clear_selected_date:
			clearSession(SESSION_DATE_KEY)

		if rescan_disk:
			self._storage.clearRecordings()

		# check if the directory tree has changed
		self._storage.updateRecordings()

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
						filter_start = parseDateTime(filter_start)
						setSession(SESSION_FILTER_START_KEY, filter_start)
					except ValueError, inst:
						errors.append('Errror parsing filter start time: %s'
								% inst)
						filter_start = getSession(SESSION_FILTER_START_KEY)
				else:
					clearSession(SESSION_FILTER_START_KEY)
			if filter_finish is not None:
				if filter_finish:
					try:
						filter_finish = parseDateTime(filter_finish)
						setSession(SESSION_FILTER_FINISH_KEY, filter_finish)
					except ValueError, inst:
						errors.append('Errror parsing filter finish time: %s'
								% inst)
						filter_finish = getSession(SESSION_FILTER_FINISH_KEY)
				else:
					clearSession(SESSION_FILTER_FINISH_KEY)
		elif recording_id and (set_filter_title or set_filter_start
				or set_filter_finish):
			# handle requests to update the filter from a given recording
			recording = self._storage.getRecording(int(recording_id))
			if set_filter_title:
				filter_title = recording.title
				setSession(SESSION_FILTER_TITLE_KEY, filter_title)
				filter_start = getSession(SESSION_FILTER_START_KEY)
				filter_finish = getSession(SESSION_FILTER_FINISH_KEY)
			elif set_filter_start:
				filter_start = recording.start_time
				setSession(SESSION_FILTER_START_KEY, filter_start)
				filter_title = getSession(SESSION_FILTER_TITLE_KEY)
				filter_finish = getSession(SESSION_FILTER_FINISH_KEY)
			elif set_filter_finish:
				filter_finish = recording.finish_time
				setSession(SESSION_FILTER_FINISH_KEY, filter_finish)
				filter_title = getSession(SESSION_FILTER_TITLE_KEY)
				filter_start = getSession(SESSION_FILTER_START_KEY)
		else:
			# filter should not be set without update_filter (unless sessioned)
			# getSession returns None when key is not found
			filter_title = getSession(SESSION_FILTER_TITLE_KEY)
			filter_start = getSession(SESSION_FILTER_START_KEY)
			filter_finish = getSession(SESSION_FILTER_FINISH_KEY)

		# the rest are inter-related so set defaults
		selected_date = None
		selected_recording = None
		selected_recordings = None

		# if setting recording id, then save it to session if we were passed one
		# (or delete one saved in session if we weren't)
		if set_recording_id:
			if recording_id is not None:
				setSession(SESSION_RECORD_ID_KEY, int(recording_id))
			else:
				clearSession(SESSION_RECORD_ID_KEY)

		# if we have a recording id, then get the record (& clear selected day)
		recording_id = getSession(SESSION_RECORD_ID_KEY)
		if recording_id:
			selected_recording = self._storage.getRecording(recording_id)

			# ensure record is valid (or remove session key)
			if not selected_recording:
				clearSession(SESSION_RECORD_ID_KEY)
			# enable filter to "filter out" selected recording.
			# if this happens, select date of filtered out recording to keep
			# the same part of calendar being displayed
			#elif ((filter_title and filter_title != NO_FILTER_TITLE
			#		and selected_recording.title != filter_title)
			#		or (filter_start
			#		and filter_start > selected_recording.finish_time)
			#		or (filter_finish
			#		and filter_finish < selected_recording.start_time)):
			#	selected_date = selected_recording.start_date
			#	setSession(SESSION_DATE_KEY, selected_date)
			#	selected_recording = None
			#	clearSession(SESSION_RECORD_ID_KEY)
			# clear date from session (record trumps date)
			else:
				clearSession(SESSION_DATE_KEY)
		# only highlight selected date if it's specified (and no record is)
		else:
			if year and month and day:
				selected_date = datetime.date(int(year), int(month), int(day))
				setSession(SESSION_DATE_KEY, selected_date)
			else:
				selected_date = getSession(SESSION_DATE_KEY)

			if selected_date:
				# must convert generator to a list
				selected_recordings = [rec for rec in
						self._storage.getRecordings(selected_date,
						filter_title, filter_start, filter_finish)]

		# determine which days to highlight
		# always show today
		today = datetime.date.today()

		# "go to today" button trumps other month/year settings
		if go_to_today:
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

		# get the schedule titles for the filter, and add a "show all" option
		schedule_titles = [NO_FILTER_TITLE]
		schedule_titles.extend(self._schedule.getScheduleTitles())
		schedule_titles.append(UNTITLED)

		if view == 'month':
			calendar = RecordingsHTMLCalendar(year, month, today, self._storage,
					filter_title, filter_start, filter_finish, selected_date,
					selected_recording).showMonth()
		else:
			calendar = ('<h2>Unimplemented</h2>'
					'That calendar format is not supported')

		return template.render(station=self.getStationName(), errors=errors,
				schedule_titles=schedule_titles, filter_title=filter_title,
				filter_start=outputDateTime(filter_start),
				filter_finish=outputDateTime(filter_finish),
				calendar=genshi.HTML(calendar),
				selected_recording=selected_recording,
				selected_recordings=selected_recordings)


#	@cherrypy.expose
	@template.output('categories.html')
	def categories(self, sort='label', sort_order='asc', **ignored):
		categories = self._storage.getCategories(sort, sort_order)
		return template.render(station=self.getStationName(),
				categories=categories,
				sort=sort, sort_order=sort_order)


#	@cherrypy.expose
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


#	@cherrypy.expose
	@template.output('calls.html')
	def calls(self, sort='date_and_time', sort_order='asc', category=None,
			**ignored):
		return template.render(station=self.getStationName(),
				calls=self._storage.getCalls(sort, sort_order, category),
				sort=sort, sort_order=sort_order)


#	@cherrypy.expose
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
		return self._config.getValue2(common.STATION_SECTION_NAME,
				common.STATION_NAME_KEY)


	def updateConfigFromPostData(self, config, data):
		# clear out the configuration and re-populate it
		config.clearConfig()

		# parse the data, then sort it so it can be entered in the same order we
		# sent it (to preserve the order that gets mixed up by the POST data)
		sections = {}
		keys = {}
		values = {}
		for key in data:
			if key.startswith(common.META_PREFIX):
				split_data = data[key].split(',')
				section_index = int(split_data[0])
				name_index = int(split_data[1])
				subname_index = int(split_data[2])
				value = ','.join(split_data[3:])
				real_key = key[len(common.META_PREFIX):]
				if not keys.has_key(section_index):
					keys[section_index] = {}
				if not keys[section_index].has_key(name_index):
					keys[section_index][name_index] = {}
				keys[section_index][name_index][subname_index] = (real_key,
						value)
			elif key.startswith(common.SECTION_META_PREFIX):
				index = data[key].find(',')
				id = int(data[key][:index])
				value = data[key][index+1:]
				real_key = key[len(common.SECTION_META_PREFIX):]
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
					% (root_dir, common.CAPTURE_SECTION_NAME,
					common.CAPTURE_ROOT_DIR_KEY))
		# split to remove title line, then split into fields
		(_,_,_,available,percent,_) = Popen(["df", "-k", root_dir],
				stdout=PIPE).communicate()[0].split('\n')[1].split()
		percent_free = 100 - int(percent[:-1])
		available = int(available)
		return ("%s free (%d%%) Approx. %s recording time left"
				% (prettyPrintSize(available), percent_free,
						prettyPrintTime(available/RECORDING_KB_PER_SECOND)))


	def getLocalTime(self):
		now = datetime.datetime.now()
		# TODO add info about sunrise/set relative time
		# "18:36 (1 hour until sunset), 01 February 2010"
		return now.strftime('%H:%M, %d %B %Y')


	def getLastRecording(self):
		# find the most recent recording in the database?
		# or should we scan the scheduled recordings and assume they occurred
		return '?? Poison Dart Frogs (18:00-18:30)'


	def getNextRecording(self):
		now = datetime.datetime.now()
		#times come out sorted, so just return the first one in the future
		for recording_time in self._schedule.getSchedules(
				days_to_schedule=2):
			if recording_time.start > now:
				return recording_time

		return None


def setSession(key, value):
	cherrypy.session[key] = value

def getSession(key):
	if cherrypy.session.has_key(key):
		return cherrypy.session[key]
	return None

def clearSession(key):
	if cherrypy.session.has_key(key):
		del cherrypy.session[key]


def outputDateTime(date):
	if type(date) == datetime.datetime:
		return date.strftime(DATETIME_UI_FORMAT)
	return ''

def parseDateTime(date_string):
	match = DATETIME_RE.match(date_string)
	if match:
		day, month, year, hour, minute = match.groups()
		day, month, year = map(int, (day, month, year))
		if year < 1900:
			raise ValueError('Invalid year=%d: must be >= 1900' % year)
		if hour:
			hour = int(hour)
			minute = int(minute)
		else:
			hour = 0
			minute = 0
		return datetime.datetime(year, month, day, hour, minute)
	raise ValueError('Invalid date/time string: "%s". Must be of the form: '
			'days/months/years [hours:minutes]' % date_string)


def prettyPrintSize(kilobytes):
	sizes = [ (1024**3, "T"), (1024**2, "G"), (1024, "M"), (1, "k") ]
	for size,unit in sizes:
		if kilobytes > size:
			return "%.1f%sB" % (float(kilobytes) / size, unit)
	return "error generating pretty size for %f" % kilobytes


def prettyPrintTime(seconds, show_seconds=False):
	sizes = [ (365*24*60*60, "years"), (30*24*60*60, "months"),
			(24*60*60, "days"), (60*60, "hours"),
			(60, "minutes"), (1, "seconds") ]
	if not show_seconds:
		sizes = sizes[:-1]
	pretty = ""
	for size,unit in sizes:
		if seconds > size:
			pretty += "%d %s " % (seconds / size, unit)
			seconds %= size
	return pretty


def loadWebConfig():
	# load config from file
	cherrypy.config.update(WEB_CONFIG)

	# set static directories to be relative to script
	cherrypy.config.update({
			'tools.staticdir.root': os.path.abspath(os.path.dirname(__file__)),
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
			port=cherrypy.config[SERVER_PORT_KEY], stype=common.ZEROCONF_TYPE,
			magic=common.ZEROCONF_TEXT_TO_IDENTIFY_BOWERBIRD)

	cherrypy.engine.start()
	service.publish()

	cherrypy.engine.block()

	service.unpublish()


if __name__ == '__main__':
	main(sys.argv)
