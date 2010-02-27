import sys, os, apsw, datetime
from threading import RLock

from lib import common
from lib.odict import OrderedDict
from bowerbird.scheduleparser import RecordingTime


RECORDINGS_TABLE = 'recordings'
FILES_TABLE = 'recording_files'
LINKS_TABLE = 'recording_links'
ADJACENT_GAP_TOLERANCE = datetime.timedelta(seconds=4)
UNTITLED = 'Untitled'


class Storage(object):
	'''Handles the details of storing data in an sqlite db'''

	REQUIRED_TABLES = []
	REQUIRED_TABLE_INIT = {}

	def __init__(self, database_file, root_dir):
		self._file = database_file
		self._root_dir = root_dir

		# issue warning if database file didn't exist
		if not os.path.exists(database_file):
			sys.stderr.write('Warning: configured database file '
					'"%s" does not exist. Creating a new one...\n'
					% database_file)

		self._conn = apsw.Connection(self._file)
		self._conn.setbusytimeout(1000)
		self._lock = RLock()

		# make sure that database has key tables
		if not self.hasRequiredTables():
			sys.stderr.write('Warning: configured database file '
					'"%s" missing required tables. Creating them...\n'
					% database_file)
			self.createRequiredTables()

	def __del__(self):
		if self.__dict__.has_key('_conn'):
			self._conn.close()

	def __convertToDictionary(self, cursor, row):
		'''A row trace method to emulate the Row Factory of sqlite3'''
		desc = cursor.getdescription()
		dict = {}
		for i in xrange(len(desc)):
			dict[desc[i][0]] = row[i]
		return dict

	def __execSqlQuery(self, query, convert_to_dictionary):
		# ensure that only one thread accesses the database at a time
		with self._lock:
			cursor = self._conn.cursor()
			if convert_to_dictionary:
				cursor.setrowtrace(self.__convertToDictionary)
			cursor.execute(query)
			return cursor.fetchall()

	def runQuerySingleResponse(self, query):
		response = self.__execSqlQuery(query, True)
		values = OrderedDict()
		if response:
			for key in response[0].keys():
				values[key] = response[0][key]
		return values

	def runQuery(self, query):
		list = []
		for line in self.__execSqlQuery(query, True):
			values = OrderedDict()
			keys = line.keys()
			for key in keys:
				values[key] = line[key]
			list.append(values)
		return list


	def runQueryRaw(self, query):
		return self.__execSqlQuery(query, False)


	def hasRequiredTables(self):
		for table in self.REQUIRED_TABLES:
			if not self.runQueryRaw('pragma table_info("%s")' % table):
				return False
		return True
		#try:
		#	for table in self.REQUIRED_TABLES:
		#		self.runQueryRaw('select * from %s' % table)
		#	return True
		#except apsw.Error:
		#	return False;


	def createRequiredTables(self):
		for table in self.REQUIRED_TABLES:
			self.runQueryRaw('create table if not exists %s (%s)'
					% (table, self.REQUIRED_TABLE_INIT[table]))


class Recording():
	def __init__(self, dict):
		self.id = int(dict['id'])
		self.title = dict['title']
		self.start_date = convertIsoStringToDate(dict['start_date'])
		self.start_time = convertIsoStringToDateTime(dict['start_time'])
		self.finish_time = convertIsoStringToDateTime(dict['finish_time'])
		self.start_limit = convertIsoStringToDateTime(dict['start_limit'])
		self.finish_limit = convertIsoStringToDateTime(dict['finish_limit'])

	def isTitled(self):
		return self.title != UNTITLED

	def __str__(self):
		return '%s (%d): %s - %s [%s - %s]' % (self.title, self.id,
				self.start_time.strftime('%H:%M:%S'),
				self.finish_time.strftime('%H:%M:%S'),
				self.start_limit.strftime('%H:%M:%S'),
				self.finish_limit.strftime('%H:%M:%S'))


class RecordingFile():
	def __init__(self, dict, duration):
		self.id = int(dict['id'])
		self.path = dict['path']
		self.finish = getTimestampFromFilePath(self.path)
		self.start = self.finish - duration

	def __str__(self):
		return '%s (%d): %s - %s' % (self.path, self.id,
				self.start.strftime('%H:%M:%S'),
				self.finish.strftime('%H:%M:%S'))


class BowerbirdStorage(Storage):
	'''Handles the details of storing data about calls and categories'''

	REQUIRED_TABLES = [RECORDINGS_TABLE, FILES_TABLE, LINKS_TABLE,
			'categories', 'calls']
	REQUIRED_TABLE_INIT = {
			RECORDINGS_TABLE: 'id integer primary key, title text, '
					'start_date text, start_time text, finish_time text, '
					'start_limit text, finish_limit text',
			FILES_TABLE: 'id integer primary key, path text',
			LINKS_TABLE: 'id integer primary key, '
					'recording_id integer, file_id integer',
			'categories': 'label text, calls integer, length real, '
					'length_stddev real, frequency real, frequency_stddev real, '
					'harmonics_min integer, harmonics_max integer, AM text, '
					'FM text, bandwidth real, bandwidth_stddev real, '
					'energy real, energy_stddev real, time_of_day_start text, '
					'time_of_day_finish text',
			'calls': 'example boolean, filename text, '
					'date_and_time timestamp, label text, category text, '
					'length real, frequency real, harmonics integer, AM text, '
					'FM text, bandwidth real, energy real'
			}

	def __init__(self, database_file, config, schedule):
		self._config = config
		self._schedule = schedule
		Storage.__init__(self, database_file, self.getRootDir())
		self._recording_ext = self.getRecordingExt()
		self._file_duration = self.getFileDuration()

		# this field is used to identify new recordings
		self._dir_timestamp = -1


	def getRecordings(self, date):
		query = ('select * from "%s" where start_date="%s"'
				% (RECORDINGS_TABLE, date.isoformat()))
		return (Recording(row) for row in self.runQuery(query))


	def clearRecordings(self):
		for table in (RECORDINGS_TABLE, LINKS_TABLE, FILES_TABLE):
			self.runQueryRaw('delete from "%s"' % table)
		self._dir_timestamp = -1


	def updateRecordings(self, force_rescan=False):
		'''Scan the storage directories and update the recordings table'''

		# Scan the storage directories and update the recording files table
		# and also keep track of new files added
		new_files = []
		for dir in os.listdir(self._root_dir):
			dir_path = os.path.join(self._root_dir, dir)
			# directory timestamp is equal to when the last file was created
			# so if it's old we know that there's no new files
			dir_mtime = os.path.getmtime(dir_path)
			if (os.path.isdir(dir_path)
					and (force_rescan or dir_mtime > self._dir_timestamp)):
				for recording_file in os.listdir(dir_path):
					# ignore files with the wrong extension
					if recording_file.endswith(self._recording_ext):
						file_path = os.path.join(dir, recording_file)
						if not self.hasRecordingFile(file_path):
							self.addRecordingFile(file_path)
			# update the timestamp
			self._dir_timestamp = dir_mtime

		# Compare the new files to schedule to add new recordings.
		# Recordings can be labelled (when they line up with a schedule).
		# New files that extend existing recordings should be merged.
		# TODO: improve mechanism to determine adjacent recordings
		for recording_file in self.getFilesWithoutRecording():
#			print recording_file
			# see if the recording file is in a scheduled timeslot
			matched_schedules = self._schedule.findSchedulesCovering(
					recording_file.start, recording_file.finish)

			self.addFileToAppropriateRecordings(recording_file,
					matched_schedules)

	def addFileToAppropriateRecordings(self, recording_file, matched_schedules):
		# see if there's any existing recordings that "overlap" the file
		prev_recordings = self.getRecordingsAtTime(recording_file.start
				- ADJACENT_GAP_TOLERANCE)
		next_recordings = self.getRecordingsAtTime(recording_file.finish
				+ ADJACENT_GAP_TOLERANCE)

		if matched_schedules:
			# search first for merges of title-matched file, prev and next
			# must iterate over a copy because we might remove a schedule
			for match in matched_schedules[:]:
				matched_prev = next((rec for rec in prev_recordings
						if rec.title == match.title), None)
				matched_next = next((rec for rec in next_recordings
						if rec.title == match.title), None)
				if matched_prev and matched_next:
					self.mergeRecordings(matched_prev, matched_next,
								recording_file)
					# remove considerations for matching
					matched_schedules.remove(match)
					# if there's no more matched schedules, then return
					if not matched_schedules:
						return
			# now search for recordings we could add the file to
			# must iterate over a copy because we might remove a schedule
			for match in matched_schedules[:]:
				# search for title-matched recordings (next and prev)
				for recording in (rec for rec in prev_recordings
						+ next_recordings if rec.title == match.title):
					self.addFileToRecording(recording, recording_file)
					# remove considerations for matching
					matched_schedules.remove(match)
			# add the remaining matched schedules
			for match in matched_schedules:
				self.createNewRecordingFromFile(recording_file, match)

		else: # matched_schedules
			# remove untitled recordings from the list if titled recordings are
			# also in there, because title recordings are "higher" priority
			for recordings in next_recordings, prev_recordings:
				found_untitled = None
				found_titled = False
				# first pass: see if there's a titled recording, and merge any
				# overlapping untitled recordings
				for recording in recordings:
					if recording.isTitled():
						found_titled = True
					else:
						if found_untitled:
							mergeRecordings(untitled, recording)
						else:
							found_untitled = recording
				# second pass, if required, remove untitled
				if found_titled:
					# must iterate over a copy because we might remove from list
					for recording in recordings[:]:
						if not recording.isTitled():
							recordings.remove(recording)

			# we must keep track of if we've matched a titled recording, because
			# we can't then match an untitled recording
			not_matched_titled = True

			# first for titled recordings

			# search first for merges of title-matched prev and next
			# must use list comprehension instead of generator because we
			# might remove entries from prev_recordings
			for prev in [rec for rec in prev_recordings if rec.isTitled()]:
				matched_next = next((rec for rec in next_recordings
						if rec.title == prev.title), None)
				if matched_next:
					self.mergeRecordings(prev, matched_next, recording_file)
					# remove considerations for matching
					prev_recordings.remove(prev)
					next_recordings.remove(matched_next)
					if prev.isTitled():
						not_matched_titled = False

			# now search for recordings we could add the file to
			# must use list comprehension instead of generator because we
			# might remove entries from recordings lists
			for recording in [rec for rec in prev_recordings if rec.isTitled()]:
				self.addFileToRecording(recording, recording_file)
				prev_recordings.remove(recording)
				not_matched_titled = False
			for recording in [rec for rec in next_recordings if rec.isTitled()]:
				self.addFileToRecording(recording, recording_file)
				next_recordings.remove(recording)
				not_matched_titled = False

			# if we haven't matched any titled recordings, then keep going
			if not_matched_titled:
				# check for possible merge between two untitled recordings
				# (at this point we know no titled recordings remain, and there
				# is, at most, one recording in each list)
				if prev_recordings and next_recordings:
					self.mergeRecordings(prev_recordings[0], next_recordings[0],
							recording_file)
				elif prev_recordings:
					self.addFileToRecording(prev_recordings[0], recording_file)
				elif next_recordings:
					self.addFileToRecording(next_recordings[0], recording_file)
				else:
					self.createNewRecordingFromFile(recording_file)


	def hasRecordingFile(self, file_path):
		query = 'select id from "%s" where path="%s"' % (FILES_TABLE, file_path)
		return len(self.runQuery(query)) > 0


	def addRecordingFile(self, file_path):
		query = 'insert into "%s" values(NULL, "%s")' % (FILES_TABLE, file_path)
		self.runQueryRaw(query)


	def getRecordingsAtTime(self, time):
		assert type(time) == datetime.datetime, ('time parameter must be a '
				'datetime, not a "%s"' % type(time))
		# check that the given time is not only overlaps the recording, but also
		# is within the limits of the recording (from the schedule)
		query = ('select * from %(table)s where "%(time)s" between start_time '
				'and finish_time and "%(time)s" between start_limit and '
				'finish_limit'
				% {'table': RECORDINGS_TABLE, 'time': time.isoformat()})
		return [Recording(row) for row in self.runQuery(query)]


	def getFilesWithoutRecording(self):
		'''Scan the recording files and links tables to find files not
			associated with a recording'''
		query = ('select file.id,file.path from %(files_table)s file left join '
				'%(links_table)s link on file.id = link.file_id '
				'where link.file_id is null' % {'files_table': FILES_TABLE,
				'links_table': LINKS_TABLE})
		return (RecordingFile(row, self._file_duration) for row in
				self.runQuery(query))


	def mergeRecordings(self, prev_recording, next_recording,
			recording_file=None):
		'''Merges two recordings (optionally with the file that joins them).
			Requires merging times in recordings table, and modifying the links
			table'''
		assert isinstance(prev_recording, Recording), ('prev_recording must be '
				'a Recording, not a %s' % prev_recording.__class__.__name__)
		assert isinstance(next_recording, Recording), ('next_recording must be '
				'a Recording, not a %s' % next_recording.__class__.__name__)
		assert (recording_file is None
				or isinstance(recording_file, RecordingFile)), (
				'recording_file must be a RecordingFile, not a %s'
				% recording_file.__class__.__name__)

		# extend prev to include next
		# limits and date should not need changing
		query = ('update "%s" set finish_time = "%s" where id=%d'
				% (RECORDINGS_TABLE, next_recording.finish_time.isoformat(),
				prev_recording.id))
		self.runQueryRaw(query)
		# change all links to next recording to prev recording
		query = ('update "%s" set recording_id=%d where recording_id=%d'
				% (LINKS_TABLE, prev_recording.id, next_recording.id))
		self.runQueryRaw(query)
		# delete the obsolete (next) recording
		query = 'delete from recordings where id=%d' % next_recording.id
		self.runQueryRaw(query)

		if recording_file:
			# add new file to links
			query = ('insert into "%s" values(NULL, %d, %d)'
					% (LINKS_TABLE, prev_recording.id, recording_file.id))
			self.runQueryRaw(query)


	def addFileToRecording(self, recording, recording_file):
		assert isinstance(recording, Recording), ('next_recording must be '
				'a Recording, not a %s' % next_recording.__class__.__name__)
		assert isinstance(recording_file, RecordingFile), (
				'recording_file must be a RecordingFile, not a %s'
				% recording_file.__class__.__name__)

		# expand the recording to include the time covered by the file
		query = ('update "%s" set start_time="%s", finish_time="%s" where '
				'id=%d' % (RECORDINGS_TABLE,
				min(recording.start_time, recording_file.start).isoformat(),
				max(recording.finish_time, recording_file.finish).isoformat(),
				recording.id))
		self.runQueryRaw(query)
		# link the file to the recording
		query = ('insert into "%s" values(NULL, %d, %d)'
				% (LINKS_TABLE, recording.id, recording_file.id))
		self.runQueryRaw(query)


	def createNewRecordingFromFile(self, recording_file, matched_schedule=None):
		assert isinstance(recording_file, RecordingFile), (
				'recording_file parameter must be a RecordingFile, not a "%s"'
				% recording_file.__class__.__name__)

		if not matched_schedule:
			# create new anonymous recording (whose limits are the entire day)
			start = recording_file.finish.replace(hour=0, minute=0, second=0,
					microsecond=0)
			finish = start + datetime.timedelta(1, 0, -1)
			matched_schedule = RecordingTime(UNTITLED, start, finish)

		# add new recording entry
		query = ('insert into "%s" values(NULL, "%s", "%s", "%s", "%s", '
				'"%s", "%s")' % (RECORDINGS_TABLE, matched_schedule.title,
				recording_file.finish.date().isoformat(),
				recording_file.start.isoformat(),
				recording_file.finish.isoformat(),
				matched_schedule.start.isoformat(),
				matched_schedule.finish.isoformat()))
		self.runQueryRaw(query)

		# add link between file and recording
		query = ('insert into %(links_table)s ("recording_id", "file_id") '
				'select recording.id, file.id '
				'from %(recordings_table)s as recording, '
				'%(recording_files)s as file '
				'where recording.start_time = "%(start)s" '
				'and recording.finish_time = "%(finish)s" '
				'and file.path = "%(file)s"'
				% {'links_table': LINKS_TABLE,
				'recordings_table': RECORDINGS_TABLE,
				'recording_files': FILES_TABLE,
				'start': recording_file.start.isoformat(),
				'finish': recording_file.finish.isoformat(),
				'file': recording_file.path})
		self.runQueryRaw(query)


	def getRootDir(self):
		return self._config.getValue2(common.CAPTURE_SECTION_NAME,
				common.CAPTURE_ROOT_DIR_KEY)


	def getRecordingExt(self):
		return self._config.getValue2(common.CAPTURE_SECTION_NAME,
				common.CAPTURE_FILE_EXT_KEY)


	def getFileDuration(self):
		return datetime.timedelta(seconds = int(self._config.getValue2(
				common.CAPTURE_SECTION_NAME, common.CAPTURE_BUFFER_FRAMES_KEY))
				/ int(self._config.getValue2(common.CAPTURE_SECTION_NAME,
						common.CAPTURE_SAMPLING_RATE)))


	def getCategories(self, sort_key=None, sort_order='asc'):
		if sort_key:
			query = ('select * from categories order by %s %s'
					% (sort_key , sort_order))
		else:
			query = 'select * from categories'

		try:
			return self.runQuery(query)
		except apsw.Error, inst:
			# just ignore missing table for now
			print inst
		return []


	def getCategoryNames(self, sort_key=None, sort_order='asc'):
		if sort_key:
			query = ('select label from categories order by "%s" %s'
					% (sort_key , sort_order))
		else:
			query = 'select label from categories'

		list = []
		try:
			for line in self.__execSqlQuery(query):
				list.append(line['label'])
		except apsw.Error:
			# just ignore missing table for now
			pass
		return list


	def getCategory(self, label):
		try:
			return self.runQuerySingleResponse(
					'select * from categories where label="%s"' % label)
		except apsw.Error:
			# just ignore missing table for now
			pass
		return OrderedDict()


	def updateCategory(self, old_label, new_label):
		self.runQueryRaw('update categories set label="%s" where label="%s"'
				% (new_label, old_label))
		self.runQueryRaw('update calls set category="%s" where category="%s"'
				% (new_label, old_label))


	def getCalls(self, sort_key='date_and_time', sort_order='asc',
			category=None):
		if category:
			query = ('select * from calls where category="%s" order by %s %s'
					% (category, sort_key , sort_order))
		else:
			query = ('select * from calls order by %s %s'
					% (sort_key , sort_order))

		return self.runQuery(query)


	def getCall(self, filename):
		return self.runQuerySingleResponse(
				'select * from calls where filename="%s"' % filename)


	def updateCall(self, filename, label, category, example):
		if filename:
			if example:
				example = 'checked'
			else:
				example = ''
			self.__execSqlQuery('update calls set label="%s",category="%s",'
					'example="%s" where filename="%s"'
					% (label, category, example, filename))


	def getPrevAndNextCalls(self, call):
		return {
				'prev': self.runQuerySingleResponse('select * from calls '
					'where date_and_time < "%s" order by date_and_time desc '
					'limit 1' % call['date_and_time']),
				'prev_in_cat': self.runQuerySingleResponse(
					'select * from calls where category = "%s" and '
					'date_and_time < "%s" order by date_and_time desc limit 1'
					% (call['category'], call['date_and_time'])),
				'next': self.runQuerySingleResponse('select * from calls '
					'where date_and_time > "%s" order by date_and_time asc '
					'limit 1' % call['date_and_time']),
				'next_in_cat': self.runQuerySingleResponse(
					'select * from calls where category = "%s" and '
					'date_and_time > "%s" order by date_and_time asc limit 1'
					% (call['category'], call['date_and_time']))
		}


class ProxyStorage(Storage):
	'''Handles the details of storing info about previous connections made,
	   and recordings in the database'''

	REQUIRED_TABLES = ['previous_connections']
	REQUIRED_TABLE_INIT = {
			'previous_connections' : 'name, address, last_connected, '
					'times_connected'
			}

	def addConnection(self, name, address):
		# check it's not a known address
		response = self.runQuerySingleResponse(
				'select name, times_connected from previous_connections '
				'where address="%s"' % address)
		if response:
			times_connected = response['times_connected'] + 1
			self.runQueryRaw('update previous_connections set name="%s", '
					'last_connected="%s", times_connected=%d where address="%s"'
					% (name, getFormattedCurrentTime(), times_connected,
						address))
		else:
			self.runQueryRaw('insert into previous_connections values '
					'("%s", "%s", "%s", 1)'
					% (name, address, getFormattedCurrentTime()))


	def removeConnection(self, address):
		self.runQueryRaw('delete from previous_connections where address="%s"'
				% address)


	def getPreviousConnections(self, sort_key='last_connected',
			sort_order='asc'):
		query = ('select * from previous_connections order by %s %s'
				% (sort_key , sort_order))
		return self.runQuery(query)


def getTimestampFromFilePath(file_path):
	return convertIsoStringToDateTime(os.path.splitext(file_path.replace(
			'/', 'T'))[0])


def convertIsoStringToDateTime(iso_datetime):
	# ISO string is allowed to omit microseconds if they're zero
	if iso_datetime.count('.'):
		return datetime.datetime.strptime(iso_datetime, '%Y-%m-%dT%H:%M:%S.%f')
	return datetime.datetime.strptime(iso_datetime,'%Y-%m-%dT%H:%M:%S')


def convertIsoStringToTime(iso_time):
	# ISO string is allowed to omit microseconds if they're zero
	if iso_time.count('.'):
		return datetime.datetime.strptime(iso_time,'%H:%M:%S.%f').time()
	return datetime.datetime.strptime(iso_time,'%H:%M:%S').time()


def convertIsoStringToDate(iso_date):
	return datetime.datetime.strptime(iso_date,'%Y-%m-%d').date()


def getFormattedCurrentTime():
	return datetime.datetime.now().ctime()


def test():
	class FF():
		def __init__(self, start, finish):
			self.start = start
			self.finish = finish
		def __str__(self):
			return 'F(%s - %s)' % (self.start, self.finish)

	class FR():
		def __init__(self, title, tag):
			self.title = title
			self.tag = tag
		def isTitled(self):
			return self.title != 'u'
		def __str__(self):
			#return 'R(%s, %s)' % (self.title,self.tag)
			return self.title

	f1 = FR('s1','f')
	f2 = FR('s2','f')
	fs = [[], [f1], [f1,f2]]

	rup = FR('u','p')
	r1p = FR('s1','p')
	r2p = FR('s2','p')
	run = FR('u','n')
	r1n = FR('s1','n')
	r2n = FR('s2','n')
	r3n = FR('s3','n')
	rat = [[], [rup], [r1p], [rup,r1p], [r2p], [r1p,r2p],
			[], [run], [r1n], [run,r1n], [r2n], [r1n,r2n],[r3n],[r2n,r3n],
			[r1n,r2n,r3n]]

	from copy import deepcopy

	class BSTest(BowerbirdStorage):
		def __init__(self):
			pass
		def getRecordingsAtTime(self,time):
			return deepcopy(rat[time.day - 1])
		def mergeRecordings(self, prev, next, rfile):
			print 'm(%s)' % prev.title,
		def createNewRecordingFromFile(self, rfile, schd=None):
			if schd:
				print 'c(%s)' % schd.title,
			else:
				print "c(u)",
		def addFileToRecording(self, rec, rfile):
			print 'a(%s,%s)' % (rec.title, rec.tag),

	for match in fs:
		for p in xrange(6):
			for n in range(6,15):
				prev = datetime.date(1, 1, p+1)
				next = datetime.date(1, 1, n+1)
				print ('%s\t%s\t%s\t'
						% (map(str,match),map(str,rat[p]),map(str,rat[n]))),
				BSTest().addFileToAppropriateRecordings(FF(prev, next),
						deepcopy(match))
				print

if __name__ == '__main__':
	test()
