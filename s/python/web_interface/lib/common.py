# some common variables
SECTION_META_KEY = '__smeta_key__'
SECTION_META_PREFIX = '__smeta__'
META_PREFIX = '__meta__'

# zeroconf type. Using HTTP means that normal zeroconf-supporting web browsers
# will see our bowerbird systems.
ZEROCONF_TYPE = '_http._tcp'
# zeroconf text to identify bowerbird
ZEROCONF_TEXT_TO_IDENTIFY_BOWERBIRD = 'This is a bowerbird system.'

# parameters for parsing the contents of the bowerbird config file
STATION_SECTION_NAME = 'station_information'
STATION_NAME_KEY = 'name'

# paramaters for monitoring disk usage by sound capture
CAPTURE_SECTION_NAME = 'sound_capture'
CAPTURE_ROOT_DIR_KEY = 'sound_file_root_dir'
CAPTURE_FILE_EXT_KEY = 'sound_file_ext'
CAPTURE_BUFFER_FRAMES_KEY = 'sound_buffer_frames'
CAPTURE_SAMPLING_RATE = 'alsa_sampling_rate'


class MissingConfigKeyError(RuntimeError):
	def __init__(self, value):
		self.value = value
	def __str__(self):
		return repr(self.value)

import datetime

def compactTimeFormat(time):
	# first round time to nearest minute
	if time.second * 1000000 + time.microsecond > 30000000:
		my_time = time + datetime.timedelta(minutes=1)
	else:
		my_time = time

	if my_time.minute == 0:
		return my_time.strftime('%H')
	return my_time.strftime('%H:%M')
