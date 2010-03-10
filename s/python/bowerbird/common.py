import datetime, os.path


# some common variables
SECTION_META_KEY = '__smeta_key__'
SECTION_META_PREFIX = '__smeta__'
META_PREFIX = '__meta__'

# parameters for parsing the contents of the bowerbird config file
STATION_SECTION_NAME = 'station_information'
STATION_NAME_KEY = 'name'

# paramaters for monitoring disk usage by sound capture
CAPTURE_SECTION_NAME = 'sound_capture'
CAPTURE_ROOT_DIR_KEY = 'sound_file_root_dir'
CAPTURE_FILE_EXT_KEY = 'sound_file_ext'
CAPTURE_BUFFER_FRAMES_KEY = 'sound_buffer_frames'
CAPTURE_SAMPLING_RATE = 'alsa_sampling_rate'

DEFAULT_SHELL_SEPARATOR = '__'
DEFAULT_PREAMBLE = '''# global variables automatically generated by
# %(script_file)s
# from
# %(config_file)s
# to permit easy access for shell scripts

# Do not edit directly as it will be overwritten
'''

DATE_FORMAT_ISO = '%Y-%m-%d'
TIME_FORMAT_ISO = '%H:%M:%S'
TIME_FORMAT_ISO_W_US = TIME_FORMAT_ISO + '.%f'
DATETIME_FORMAT_ISO = '%sT%s' % (DATE_FORMAT_ISO, TIME_FORMAT_ISO)
DATETIME_FORMAT_ISO_W_US = '%sT%s' % (DATE_FORMAT_ISO, TIME_FORMAT_ISO_W_US)
DATE_FORMAT_UI = '%d/%m/%Y'
TIME_FORMAT_UI_HMS = '%H:%M:%S'
TIME_FORMAT_UI_HM = '%H:%M'
TIME_FORMAT_UI_H = '%H'


def formatStationName(name, compact=False):
	'''Return the name with square brackets around it, or just the initials
		if compact form is requested.'''
	if not name:
		return ''

	if compact:
		return '[%s]' % ''.join((char for char in name if char.isupper()))
	return '[%s]' % name



def formatDateTimeIso(my_datetime):
	return my_datetime.strftime(DATETIME_FORMAT_ISO_W_US)


def parseDateTimeIso(iso_datetime):
	# ISO string is allowed to omit microseconds if they're zero
	if iso_datetime.count('.'):
		return datetime.datetime.strptime(iso_datetime,
			DATETIME_FORMAT_ISO_W_US)
	return datetime.datetime.strptime(iso_datetime, DATETIME_FORMAT_ISO)


def formatTimeIso(time):
	return time.strftime(TIME_FORMAT_ISO_W_US)


def parseTimeIso(iso_time):
	# ISO string is allowed to omit microseconds if they're zero
	if iso_time.count('.'):
		return datetime.datetime.strptime(iso_time, TIME_FORMAT_ISO_W_US).time()
	return datetime.datetime.strptime(iso_time, TIME_FORMAT_ISO).time()


def formatDateIso(date):
	return date.strftime(DATE_FORMAT_ISO)


def parseDateIso(iso_date):
	return datetime.datetime.strptime(iso_date, DATE_FORMAT_ISO).date()


def getCurrentTimeCString():
	return datetime.datetime.now().ctime()


def formatTimeUI(time, show_seconds=False, compact=False):
	# create a local variable so we can change it
	my_time = time
	# first round time
	if show_seconds:
		# to nearest second
		if time.microsecond > 500000:
			my_time += datetime.timedelta(seconds=1)
		# compact if appropriate
		if compact and my_time.second == 0:
			if my_time.minute == 0:
				return my_time.strftime(TIME_FORMAT_UI_H)
			else:
				return my_time.strftime(TIME_FORMAT_UI_HM)
		return my_time.strftime(TIME_FORMAT_UI_HMS)

	else:
		# to nearest minute
		if time.second * 1000000 + time.microsecond > 30000000:
			my_time += datetime.timedelta(minutes=1)
		# compact if appropriate
		if compact and my_time.minute == 0:
			return my_time.strftime(TIME_FORMAT_UI_H)
		else:
			return my_time.strftime(TIME_FORMAT_UI_HM)


def parseTimeUI(time_string):
	'''Flexible time parsing, accepting formats of hours, hours:minutes, and
		hours:minutes:seconds'''
	num_colons = time_string.count(':')
	if num_colons == 2:
		return datetime.datetime.strptime(time_string,
				TIME_FORMAT_UI_HMS).time()
	elif num_colons == 1:
		return datetime.datetime.strptime(time_string, TIME_FORMAT_UI_HM).time()
	elif num_colons == 0:
		return datetime.datetime.strptime(time_string, TIME_FORMAT_UI_H).time()

	raise ValueError("time data '%s' does not match any recognised time format"
			% time_string)


def formatDateUI(date):
	if type(date) == datetime.date or type(date) == datetime.datetime:
		return date.strftime(DATE_FORMAT_UI)
	return ''


def parseDateUI(date_string):
	return datetime.datetime.strptime(date_string, DATE_FORMAT_UI).date()


def formatTimeDelta(delta):
	sizes = [(60*60, '%s:'), (60, '%s'), (1, '%s') ]
	delta_string = ""
	seconds = delta.seconds
	for size,format in sizes:
		if seconds >= size:
			this, seconds = divmod(seconds, size)
			delta_string += format % this
	if delta.microseconds:
		delta_string += '.%06d' % delta.microseconds
	return delta_string


def formatSize(kilobytes):
	sizes = [ (1024**3, "T"), (1024**2, "G"), (1024, "M"), (1, "k") ]
	for size,unit in sizes:
		if kilobytes > size:
			return "%.1f%sB" % (float(kilobytes) / size, unit)
	return "error formatting size for %f" % kilobytes


def formatTimeLong(seconds, show_seconds=False):
	sizes = [ (365*24*60*60, "years"), (30*24*60*60, "months"),
			(24*60*60, "days"), (60*60, "hours"),
			(60, "minutes"), (1, "seconds") ]
	if not show_seconds:
		sizes = sizes[:-1]
	pretty = ""
	for size,unit in sizes:
		if seconds >= size:
			pretty += "%d %s " % (seconds / size, unit)
			seconds %= size
	return pretty


def datetimeFromDate(date):
	#return datetime.datetime.combine(date, datetime.time())
	return datetime.datetime(date.year, date.month, date.day)


def ensureDirectoryExists(dir):
	'''Ensure the given directory exists. If it doesn't then it recursively
		calls itself on its parent, then makes the directory'''
	if not os.path.exists(dir):
		ensureDirectoryExists(os.path.dirname(dir))
		os.mkdir(dir)


def convertConfig(config_obj, output_file,
		shell_separator=DEFAULT_SHELL_SEPARATOR, preamble=DEFAULT_PREAMBLE):
	# print preamble
	output_file.write(preamble % {'script_file':__file__,
			'config_file': config_obj.filename})

	# read station information
	for section in config_obj:
		if section != "dummy":
			# write the section comments
			section_comments = '\n'.join(config_obj.comments[section])
			if section_comments.strip():
				output_file.write(section_comments + '\n')
			else:
				output_file.write("\n# Section %s:\n" % section)

			for key in config_obj[section]:
				# write the key comments
				key_comments = '\n'.join(config_obj[section].comments[key])
				if key_comments:
					output_file.write(key_comments + '\n')
				output_file.write("%s%s%s=\"%s\"\n" % (section, shell_separator,
						key, config_obj[section][key]))


class MissingConfigKeyError(RuntimeError):
	def __init__(self, value):
		self.value = value
	def __str__(self):
		return repr(self.value)


