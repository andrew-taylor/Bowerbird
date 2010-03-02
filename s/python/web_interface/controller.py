import sys, os, os.path, errno, cherrypy, datetime
from subprocess import Popen, PIPE
from genshi import HTML
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
SESSION_FILTER_KEY = 'filter'
SESSION_DATE_KEY = 'date'
SESSION_RECORD_ID_KEY = 'record_id'

NO_FILTER_LABEL = 'All'


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
				error = HTML('''WARNING:
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
			selected_record_id=None, go_to_today=False,
			filter=None, update_filter=None,
			clear_selected_date=False, clear_selected_recording=False,
			rescan_disk=False, **ignored):
		# HACK: this helps development slightly
		if ignored:
			print "IGNORED",ignored

		if clear_selected_date:
			if cherrypy.session.has_key(SESSION_DATE_KEY):
				del cherrypy.session[SESSION_DATE_KEY]
		if clear_selected_recording:
			if cherrypy.session.has_key(SESSION_RECORD_ID_KEY):
				del cherrypy.session[SESSION_RECORD_ID_KEY]

		if rescan_disk:
			self._storage.clearRecordings()

		# check if the directory tree has changed
		self._storage.updateRecordings()

		# update filter
		if update_filter:
			if filter == NO_FILTER_LABEL:
				filter = None
				if cherrypy.session.has_key(SESSION_FILTER_KEY):
					del cherrypy.session[SESSION_FILTER_KEY]
			else:
				cherrypy.session[SESSION_FILTER_KEY] = filter
		elif cherrypy.session.has_key(SESSION_FILTER_KEY):
			filter = cherrypy.session[SESSION_FILTER_KEY]
		else:
			# filter should not be set without update_filter (unless sessioned)
			filter = None

		# the rest are inter-related so set defaults
		selected_date = None
		selected_recording = None
		selected_recordings = None
		# if passed a record id, get the record (and clear selected day)
		if (selected_record_id
				or cherrypy.session.has_key(SESSION_RECORD_ID_KEY)):
			if selected_record_id:
				cherrypy.session[SESSION_RECORD_ID_KEY] = int(
						selected_record_id)

			selected_recording = self._storage.getRecording(
					cherrypy.session[SESSION_RECORD_ID_KEY])

			# ensure record is valid (or remove session key)
			if not selected_recording:
				del cherrypy.session[SESSION_RECORD_ID_KEY]
			# enable filter to "filter out" selected recording
			elif filter and selected_recording.title != filter:
				selected_recording = None
				del cherrypy.session[SESSION_RECORD_ID_KEY]


			# clear day from session (record trumps day)
			if cherrypy.session.has_key(SESSION_DATE_KEY):
				del cherrypy.session[SESSION_DATE_KEY]
		# only highlight selected date if it's specified (and no record is)
		elif ((year and month and day)
				or cherrypy.session.has_key(SESSION_DATE_KEY)):
			if day:
				selected_date = datetime.date(int(year), int(month), int(day))
				cherrypy.session[SESSION_DATE_KEY] = selected_date
			else:
				selected_date = cherrypy.session[SESSION_DATE_KEY]
			# just convert generator to a list
			selected_recordings = [rec for rec in
					self._storage.getRecordings(selected_date, filter)]

		# determine which days to highlight
		# always show today
		today = datetime.date.today()

		# go to today button trumps other month/year settings
		if go_to_today:
			year = today.year
			month = today.month
			cherrypy.session[SESSION_YEAR_KEY] = year
			cherrypy.session[SESSION_MONTH_KEY] = month
		else:
			# parse selected year (and check sessions)
			if year:
				year = int(year)
				cherrypy.session[SESSION_YEAR_KEY] = year
			elif cherrypy.session.has_key(SESSION_YEAR_KEY):
				year = cherrypy.session[SESSION_YEAR_KEY]
			else:
				year = today.year

			# parse selected month (and check sessions)
			if month:
				month = int(month)
				cherrypy.session[SESSION_MONTH_KEY] = month
			elif cherrypy.session.has_key(SESSION_MONTH_KEY):
				month = cherrypy.session[SESSION_MONTH_KEY]
			else:
				month = today.month

		# get the schedule titles for the filter, and add a "show all" option
		schedule_titles = [NO_FILTER_LABEL]
		schedule_titles.extend(self._schedule.getScheduleTitles())
		schedule_titles.append(UNTITLED)

		if view == 'month':
			calendar = RecordingsHTMLCalendar(year, month, today, self._storage,
					filter, selected_date, selected_recording).showMonth()
		else:
			calendar = ('<h2>Unimplemented</h2>'
					'That calendar format is not supported')

		return template.render(station=self.getStationName(),
				schedule_titles=schedule_titles, filter=filter,
				calendar=HTML(calendar), selected_recording=selected_recording,
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
		return Popen("uptime", stdout=PIPE).communicate()[0]


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
