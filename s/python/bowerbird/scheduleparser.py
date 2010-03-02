#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# schedule management and information, including sun-related timing

import sys, os, re, operator, time, datetime
from copy import deepcopy

from bowerbird.sun import Sun
from bowerbird.configobj import ConfigObj

TIME_PATTERN = '([SR]?) *([+-]?[0-9]{1,2}):([0-9]{2})'
TIME_RE = re.compile(TIME_PATTERN)
SCHEDULE_PATTERN = '^ *%s *- *%s *$' % (TIME_PATTERN, TIME_PATTERN)
SCHEDULE_RE = re.compile(SCHEDULE_PATTERN)
SCHEDULE_SECTION = 'scheduled_capture'

STATION_SECTION = 'station_information'
STATION_TZ_KEY = 'tz_offset'
STATION_LAT_KEY = 'latitude'
STATION_LONG_KEY = 'longitude'

CONFIG_SECTION = 'sound_capture'
CONFIG_COMMAND = 'schedule_command'
CONFIG_USER = 'schedule_user'
CONFIG_DAYS_KEY = 'schedule_days'


SCHEDULE_HEADER = '''# schedule file for bowerbird deployment

# This file stores the times for scheduled captures.
# Format of each line is "$title = $start - $finish".
# Start and finish times can be absolute 24hour times or relative to sunrise
# or sunset. Relative times start with "R" for sunrise-relative or "S" for
# sunset-relative, then have the time offset, which can be positive or
# negative.
'''


class ScheduleParser(object):
	def __init__(self, config_filename, schedule_filename):
		self.__command = None
		self.__user = None
		self.__days_to_schedule = None
		self.__tz_offset = None
		self.__latitude = None
		self.__longitude = None
		self.__recording_specs = []

		# get a sun calculating object
		self.__sun = Sun()

		# create a config object around the config file
		self.__config = ConfigObj(config_filename)
		self.__config_timestamp = 0
		self.updateConfigFromFile()

		# create a config object around the schedule file
		self.__schedule = ConfigObj(schedule_filename)
		self.__timestamp = -1
		self.updateFromFile()


	@property
	def command(self):
		self.updateConfigFromFile()
		return self.__command
	@command.setter
	def command(self, command):
		self.updateConfigFromFile()
		self.__command = command
	def assertCommandIsDefined(self):
		if not self.__command:
			sys.stderr.write('%s must be specified in the %s section of "%s"\n'
					% (CONFIG_COMMAND, CONFIG_SECTION, self.__config.filename))
			return False
		return True

	@property
	def user(self):
		self.updateConfigFromFile()
		return self.__user
	@user.setter
	def user(self, user):
		self.updateConfigFromFile()
		self.__user = user

	@property
	def days_to_schedule(self):
		self.updateConfigFromFile()
		return self.__days_to_schedule
	@days_to_schedule.setter
	def days_to_schedule(self, days_to_schedule):
		self.updateConfigFromFile()
		self.__days_to_schedule = days_to_schedule

	@property
	def tz_offset(self):
		self.updateConfigFromFile()
		return self.__tz_offset
	@tz_offset.setter
	def setTzOffset(self, tz_offset):
		self.updateConfigFromFile()
		self.__tz_offset = tz_offset

	@property
	def latitude(self):
		self.updateConfigFromFile()
		return self.__latitude
	@latitude.setter
	def latitude(self, latitude):
		self.updateConfigFromFile()
		self.__latitude = latitude

	@property
	def longitude(self):
		self.updateConfigFromFile()
		return self.__longitude
	@longitude.setter
	def longitude(self, longitude):
		self.updateConfigFromFile()
		self.__longitude = longitude

	@property
	def filename(self):
		return self.__schedule.filename


	def updateConfigFromFile(self):
		if self.__config_timestamp >= self.getConfigTimestamp():
			return

		# read station information
		station_section = self.__config[STATION_SECTION]
		for key in station_section:
			if key == STATION_TZ_KEY:
				# get timezone from config
				self.__tz_offset = int(station_section[key])
			elif key == STATION_LAT_KEY:
				# get latitude from config
				self.__latitude = float(station_section[key])
			elif key == STATION_LONG_KEY:
				# get longitude from config
				self.__longitude = float(station_section[key])

		# this information is required for now, (eventually ask GPS)
		if not self.__latitude or not self.__longitude:
			raise MissingConfigKeyError('%s and %s must be specified in the %s '
					'section of "%s" (at least until we can talk to the GPS)\n'
					% (CONFIG_LAT_KEY, CONFIG_LONG_KEY, CONFIG_SECTION,
					self.__config.filename))

		# if timezone is not specified in config, then get it from python
		if not self.__tz_offset:
			# python tz offsets are negative and in seconds
			self.__tz_offset = -time.timezone / 3600

		# read sound capture info
		config_section = self.__config[CONFIG_SECTION]
		for key in config_section:
			if key == CONFIG_COMMAND:
				self.__command = config_section[key]
			elif key == CONFIG_USER:
				self.__user = config_section[key]
			elif key == CONFIG_DAYS_KEY:
				self.__days_to_schedule = int(config_section[key])


	def updateFromFile(self):
		file_timestamp = self.getTimestamp()
		if self.__timestamp >= file_timestamp:
			return

		self.__schedule.reload()
		# if this is a new file (or has been modified, put header back)
		if not self.__schedule.initial_comment:
			self.__schedule.initial_comment = SCHEDULE_HEADER.split('\n')
		self.__timestamp = file_timestamp
		self.updateFromConfigObj()


	def updateFromConfigObj(self):
		# parse the schedule to get the recording times
		self.__recording_specs = []
		for key in self.__schedule:
			self.__recording_specs.append(self.parseRecordingSpec(key,
					self.__schedule[key]))


	# convert given string into a TimeSpec
	@staticmethod
	def parseTimeSpec(spec):
		assert type(spec) == str or type(spec) == unicode, (
				'time spec should be string, not %s' % type(spec))

		match = TIME_RE.match(spec)
		if match:
			start_type, start_hour, start_minute = match.groups()
			return TimeSpec(TimeSpec.Type(start_type), int(start_hour),
					int(start_minute))
		else:
			raise ValueError('"%s" is not a valid time spec' % spec)


	# convert given string into a RecordingSpec
	@classmethod
	def parseRecordingSpec(cls, title, spec_part1, spec_part2=None):
		if spec_part2 == None:
			# used when spec is a single string
			assert type(spec_part1) == str or type(spec_part1) == unicode, (
					'recording spec should be string, not %s' % (spec_part1))

			match = SCHEDULE_RE.match(spec_part1)
			if match:
				(start_type, start_hour, start_minute, finish_type, finish_hour,
						finish_minute) = match.groups()
				return RecordingSpec(title, TimeSpec(TimeSpec.Type(start_type),
								int(start_hour), int(start_minute)),
						TimeSpec(TimeSpec.Type(finish_type),
								int(finish_hour), int(finish_minute)))
			else:
				raise ValueError('"%s" is not a valid recording spec (%s->%s)'
						% (spec, SCHEDULE_SECTION, title, spec))
		else:
			# when the spec is in two parts (a start and a finish)
			return RecordingSpec(title, cls.parseTimeSpec(spec_part1),
					cls.parseTimeSpec(spec_part2))


	def getSchedules(self, schedule_day=None, days_to_schedule=None):
		# update specs if file has been modified
		self.updateFromFile()

		if type(schedule_day) == datetime.datetime:
			schedule_day = schedule_day.replace(hour=0, minute=0, second=0,
					microsecond=0)
		elif type(schedule_day) == datetime.date:
			schedule_day = datetime.datetime(schedule_day.year,
					schedule_day.month, schedule_day.day)
		elif not schedule_day:
			schedule_day = datetime.datetime.today().replace(hour=0, minute=0,
					second=0, microsecond=0)
		else:
			raise TypeError('schedule_day must be datetime or date, not "%s"'
					% type(schedule_day))

		assert type(days_to_schedule) == int, ('invalid type for '
				'days_to_schedule "%s"' % type(days_to_schedule))

		if not days_to_schedule:
			days_to_schedule = self.__days_to_schedule

		recording_times = []
		for i in range(days_to_schedule):
			day = schedule_day + datetime.timedelta(days=i)
			#TODO optimisation: skip sunrise/set calc if all times are absolute
			rise, set = [datetime.timedelta(hours=(t + self.tz_offset)) for t in
					self.__sun.sunRiseSet(day.year, day.month, day.day,
							self.longitude, self.latitude)]
			for recording_spec in self.__recording_specs:
				times = []
				for time_spec in (recording_spec.start, recording_spec.finish):
					if time_spec.type.isSunriseRelative():
						times.append(day + rise + time_spec.offset)
					elif time_spec.type.isSunsetRelative():
						times.append(day + set + time_spec.offset)
					else:
						times.append(day + time_spec.offset)
				recording_times.append(RecordingTime(recording_spec.title,
						times[0], times[1]))
		# sort times by start time
		recording_times.sort(key=operator.attrgetter('start'))

		return recording_times


	def findSchedulesCovering(self, start, finish):
		'''Find all schedules that cover the given period. It may be that there
			are more than one, due to overlapping schedules, or one recording
			may overlap the boundary between two'''
		schedules = []
		# (search 1 day either side)
 		for schedule in self.getSchedules(start
				- datetime.timedelta(1), (finish - start).days + 2):
			# if either start or finish of recording file is in the schedule
			if (start in schedule or finish in schedule):
				schedules.append(schedule)

		return schedules


	def getScheduleSpecs(self):
		# update if file has been modified
		self.updateFromFile()

		# return a copy
		return deepcopy(self.__recording_specs)


	def getScheduleSpec(self, title):
		# update if file has been modified
		self.updateFromFile()

		return next((spec for spec in self.__recording_specs
				if spec.title == title), None)


	def setScheduleSpec(self, spec):
		assert isinstance(spec, RecordingSpec)

		self.__schedule[spec.title] = '%s - %s' % (spec.start, spec.finish)
		self.updateFromConfigObj()


	def deleteScheduleSpec(self, title):
		del(self.__schedule[title])
		self.updateFromConfigObj()


	def clearScheduleSpecs(self):
		self.__schedule.clear()
		self.updateFromConfigObj()


	def getScheduleTitles(self):
		# update if file has been modified
		self.updateFromFile()

		return [deepcopy(spec.title) for spec in self.__recording_specs]


	def getTimestamp(self):
		if (self.__schedule.filename
				and os.path.exists(self.__schedule.filename)):
			return int(os.path.getmtime(self.__schedule.filename))
		return 0


	def getConfigTimestamp(self):
		if self.__config.filename and os.path.exists(self.__config.filename):
			return int(os.path.getmtime(self.__config.filename))
		return 0


	def saveToFile(self):
		try:
			# update file
			self.__schedule.write()
			self.__timestamp = self.getTimestamp();
		except:
			# if this fails we should re-read the file to keep them synced
			self.__schedule.reload()
			self.__timestamp = self.getTimestamp();
			raise


	def export(self, export_filename):
		with open(export_filename, 'w') as save_file:
			self.__schedule.write(save_file)



class RecordingSpec(object):
	def __init__(self, title, start, finish):
		assert isinstance(start, TimeSpec)
		assert isinstance(finish, TimeSpec)
		self.title = title
		self.start = start
		self.finish = finish


	def __str__(self):
		return "%s: %s - %s" % (self.title, self.start, self.finish)



class RecordingTime(object):
	def __init__(self, title, start, finish):
		assert type(start) == datetime.datetime, ('type of start is "%s"'
				% type(start))
		assert type(finish) == datetime.datetime, ('type of finish is "%s"'
				% type(finish))
		self.title = title
		self.start = start
		self.finish = finish


	def getStartAndFinish(self):
		return self.start, self.finish


	def __contains__(self, time):
		assert type(time) == datetime.datetime, (
				'time must be of type datetime, called with "%s"'
				% type(time))
		return time >= self.start and time <= self.finish


	def __str__(self):
		return '%s: %s - %s' % (self.title, self.start.strftime('%H:%M'),
				self.finish.strftime('%H:%M'))



class TimeSpec(object):
	class Type:
		UNKNOWN = 0
		ABSOLUTE = 1
		SUNRISE = 2
		SUNSET = 3


		def __init__(self, string):
			if string == '':
				self.value = self.ABSOLUTE
			elif string == 'R':
				self.value = self.SUNRISE
			elif string == 'S':
				self.value = self.SUNSET
			else:
				raise ValueError('Unknown TimeSpec Type: ' + string)


		def __str__(self):
			if self.value == self.ABSOLUTE:
				return ''
			elif self.value == self.SUNRISE:
				return 'R'
			elif self.value == self.SUNSET:
				return 'S'
			else:
				return 'Unknown'


		def isAbsolute(self): return self.value == self.ABSOLUTE
		def isSunriseRelative(self): return self.value == self.SUNRISE
		def isSunsetRelative(self): return self.value == self.SUNSET


	def __init__(self, offset_type, hours, minutes):
		assert isinstance(offset_type, self.Type)
		assert type(hours) == int
		assert type(minutes) == int
		self.type = offset_type
		self.offset = datetime.timedelta(hours=hours, minutes=minutes)


	def __str__(self):
		# handle negative offsets
		total_seconds = 24*60*60 * self.offset.days + self.offset.seconds
		hours, minutes = divmod(total_seconds // 60, 60)
		type_prefix = '%s' % self.type
		if type_prefix:
			type_prefix += ' '
		return "%s%d:%02d" % (type_prefix, hours, minutes)



class MissingConfigKeyError(RuntimeError):
	def __init__(self, value):
		self.value = value
	def __str__(self):
		return repr(self.value)



def test():
	return 0


if __name__ == '__main__':
	test()
