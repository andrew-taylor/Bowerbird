import sys, os, apsw, datetime, subprocess, tempfile
from copy import deepcopy
from threading import RLock

from bowerbird.common import *
from bowerbird.odict import OrderedDict
from bowerbird.scheduleparser import RecordingTime


SOX_PATH = '/usr/bin/sox'
WAVPACK_PATH = '/usr/bin/wavpack'

# Storage tables
CONFIG_TABLE = 'config'
# BowerbirdStorage tables
RECORDINGS_TABLE = 'recordings'
FILES_TABLE = 'recording_files'
LINKS_TABLE = 'recording_links'
# ProxyStorage tables
PREVIOUS_CONNECTIONS_TABLE = 'previous_connections'

# these are used in the config database to provide statefulness
CONFIG_TABLE_TIMESTAMP_KEY = 'timestamp'

RECORDINGS_DIR = 'recordings'
ADJACENT_GAP_TOLERANCE = datetime.timedelta(seconds=4)
UNTITLED = 'Untitled'


class Storage(object):
	'''Handles the details of storing data in an sqlite db'''

	__REQUIRED_TABLES = {CONFIG_TABLE: 'key TEXT, value TEXT'}


	def __init__(self, database_file, root_dir):
		self._file = database_file
		self._root_dir = root_dir
		self._recordings_dir = os.path.join(root_dir, RECORDINGS_DIR)

		# issue warning if database file didn't exist
		if not os.path.exists(database_file):
			sys.stderr.write('Warning: configured database file '
					'"%s" does not exist. Creating a new one...\n'
					% database_file)

		self._conn = apsw.Connection(self._file)
		self._conn.setbusytimeout(1000)
		self._lock = RLock()

		# make sure recordings directory exists
		ensureDirectoryExists(self._recordings_dir)

		# ensure all required tables exist
		self._ensureTablesExist(Storage.__REQUIRED_TABLES)


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


	def _ensureTablesExist(self, tables):
		'''make sure that database has key tables'''
		for table in tables:
			self.__execSqlQuery('create table if not exists %s (%s)'
					% (table, tables[table]), False)


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


	def runQueryRawSingleResponse(self, query):
		return self.__execSqlQuery(query, False)[0]


	def readFromConfigTable(self, key, default=None):
		query = 'select value from "%s" where key="%s"' % (CONFIG_TABLE, key)
		response = self.runQueryRaw(query)
		if response:
			return response[0][0]
		return default


	def writeToConfigTable(self, key, value):
		# if we have that key in the database then update it,
		# otherwise insert new record
		if self.readFromConfigTable(key):
			query = ('update "%s" set value="%s" where key="%s"'
					% (CONFIG_TABLE, value, key))
		else:
			query = ('insert into "%s" values ("%s", "%s")'
					% (CONFIG_TABLE, key, value))
		self.runQueryRaw(query)


class BowerbirdStorage(Storage):
	'''Handles the details of storing data about calls and categories'''

	__REQUIRED_TABLES = {
			RECORDINGS_TABLE: 'id INTEGER PRIMARY KEY, title TEXT, '
					'start_date TEXT, start_time TEXT, finish_time TEXT, '
					'start_limit TEXT, finish_limit TEXT',
			FILES_TABLE: 'id INTEGER PRIMARY KEY, path TEXT',
			LINKS_TABLE: 'id INTEGER PRIMARY KEY, '
					'recording_id INTEGER, file_id INTEGER',
			'categories': 'label TEXT, calls INTEGER, length REAL, '
					'length_stddev REAL, frequency REAL, frequency_stddev REAL, '
					'harmonics_min INTEGER, harmonics_max INTEGER, AM TEXT, '
					'FM TEXT, bandwidth REAL, bandwidth_stddev REAL, '
					'energy REAL, energy_stddev REAL, time_of_day_start TEXT, '
					'time_of_day_finish TEXT',
			'calls': 'example boolean, filename TEXT, '
					'date_and_time timestamp, label TEXT, category TEXT, '
					'length REAL, frequency REAL, harmonics INTEGER, AM TEXT, '
					'FM TEXT, bandwidth REAL, energy REAL'
			}


	def __init__(self, database_file, config, schedule):
		self._config = config
		self._schedule = schedule
		self._recording_extension = self.getRecordingExtension()
		self._file_duration = self.getFileDuration()

		Storage.__init__(self, database_file, self.getRootDir())

		# ensure all required tables exist
		self._ensureTablesExist(BowerbirdStorage.__REQUIRED_TABLES)


	@property
	def recording_extension(self):
		return self._recording_extension

	@property
	def file_duration(self):
		return self._file_duration

	@property
	def dir_timestamp(self):
		'''this field is used to identify new recordings'''
		return float(self.readFromConfigTable(CONFIG_TABLE_TIMESTAMP_KEY, -1))
	@dir_timestamp.setter
	def dir_timestamp(self, value):
		self.writeToConfigTable(CONFIG_TABLE_TIMESTAMP_KEY, value)


	def getRecording(self, record_id):
		query = 'select * from "%s" where id=%d' % (RECORDINGS_TABLE, record_id)
		return Recording(self._recordings_dir, self._recording_extension,
				self.runQuerySingleResponse(query))


	def getRecordings(self, date=None, title=None, min_start_date=None,
			max_finish_date=None):
		assert date==None or type(date) == datetime.date, (
				'date parameter must be a date, not a "%s"' % type(date))
		assert min_start_date==None or type(min_start_date) == datetime.date, (
				'date parameter must be a date, not a "%s"'
				% type(min_start_date))
		assert max_finish_date==None or type(max_finish_date) == datetime.date, (
				'date parameter must be a date, not a "%s"'
				% type(max_finish_date))
		query = 'select * from "%s"' % RECORDINGS_TABLE
		conjunction = 'where'
		if date:
			query += ' %s start_date = "%s"' % (conjunction, date.isoformat())
			conjunction = 'and'
		if title:
			query += ' %s title = "%s"' % (conjunction, title)
			conjunction = 'and'
		if min_start_date:
			query += ' %s start_time >= "%s"' % (conjunction, min_start_date)
			conjunction = 'and'
		if max_finish_date:
			# add a day to finish day and check inequality
			query += ' %s finish_time < "%s"' % (conjunction,
					max_finish_date + datetime.timedelta(1))
			conjunction = 'and'
		# sort increasing by start time
		query += ' order by start_time'
		return (Recording(self._recordings_dir, self._recording_extension, row)
				for row in self.runQuery(query))


	def clearRecordings(self):
		# delete all table entries
		for table in (RECORDINGS_TABLE, LINKS_TABLE, FILES_TABLE):
			self.runQueryRaw('delete from "%s"' % table)
		# reset timestamp
		self.dir_timestamp = -1
		# delete all constructed recording files
		for root, dirs, files in os.walk(self._recordings_dir, topdown=False):
			for name in files:
				os.remove(os.path.join(root, name))
			for name in dirs:
				os.rmdir(os.path.join(root, name))


	def updateRecordings(self, force_rescan=False, verbose=False):
		'''Scan the storage directories and update the recordings table.
		Warning: this will take a lot of time, so should not be called by the
		web interface'''

		# Scan the storage directories and update the recording files table
		# and also keep track of new files added
		new_files = []
		dir_list = os.listdir(self._root_dir)
		dir_list.sort()
		latest_dir_mtime = -1
		for dir in dir_list:
			# only scan directories that are dates
			try:
				parseDateIso(dir)
			except:
				continue

			dir_path = os.path.join(self._root_dir, dir)
			# directory timestamp is equal to when the last file was created
			# so if it's old we know that there's no new files
			dir_mtime = os.path.getmtime(dir_path)
			if (os.path.isdir(dir_path)
					and (force_rescan or dir_mtime > self.dir_timestamp)):
				if verbose:
					print 'scanning', dir
				for recording_file in os.listdir(dir_path):
					# ignore files with the wrong extension
					if (os.path.splitext(recording_file)[1]
							== self._recording_extension):
						file_path = os.path.join(dir, recording_file)
						if not self.hasRecordingFile(file_path):
							self.addRecordingFile(file_path)
			if dir_mtime > latest_dir_mtime:
				latest_dir_mtime = dir_mtime

		# up the timestamp
		self.dir_timestamp = latest_dir_mtime

		# Note which recording files get put into which recordings
		# Key is recording_id, value is a list of relative pathnames of files
		# to merge (in order), with destination path calculated from recording
		file_merges = {}
		# Note which recordings get merged away so we can remove their files
		file_deletes = []

		# see if any existing recordings are missing their files
		for recording in self.getRecordings():
			if not recording.fileExists():
				print ('found recording with missing file: %s'
						% os.path.basename(recording.path))
				# must convert the generator of tuples into a list of paths
				file_merges[recording.id] = [file for file, in
						self.getFilesForRecording(recording.id)]

		# Compare the new files to schedule to add new recordings.
		# Recordings can be labelled (when they line up with a schedule).
		# New files that extend existing recordings should be merged.
		# TODO: improve mechanism to determine adjacent recordings
		files = self.findFilesWithoutRecording()
		if verbose:
			files = list(files)
			if len(files):
				print "%d new files found" % len(files)
		for recording_file in files:
			# see if the recording file is in a scheduled timeslot
			matched_schedules = self._schedule.findSchedulesCovering(
					recording_file.start, recording_file.finish)

			self.addFileToAppropriateRecordings(recording_file,
					matched_schedules, file_merges, file_deletes)

		for id in file_merges:
			recording = self.getRecording(id)
			if verbose:
				print 'assembling', recording.path
			concatSoundFiles(recording.abspath,
					(os.path.join(self._root_dir, path) for path in
					file_merges[id]))

		for filepath in file_deletes:
			if os.path.exists(filepath):
				if verbose:
					print 'removing', filepath
				os.remove(filepath)


	def addFileToAppropriateRecordings(self, recording_file, matched_schedules,
			file_merges, file_deletes):
		# see if there's any existing recordings that "overlap" the file
		# must convert generator to a list
		prev_recordings = list(self.getRecordingsAtTime(recording_file.start
				- ADJACENT_GAP_TOLERANCE))
		next_recordings = list(self.getRecordingsAtTime(recording_file.finish
				+ ADJACENT_GAP_TOLERANCE))

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
								recording_file, file_merges, file_deletes)
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
					self.addFileToRecording(recording, recording_file,
							file_merges)
					# remove considerations for matching
					matched_schedules.remove(match)
			# add the remaining matched schedules
			for match in matched_schedules:
				self.createNewRecordingFromFile(recording_file, match,
						file_merges)

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
							mergeRecordings(untitled, recording, None,
									file_merges, file_deletes)
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
					self.mergeRecordings(prev, matched_next, recording_file,
							file_merges, file_deletes)
					# remove considerations for matching
					prev_recordings.remove(prev)
					next_recordings.remove(matched_next)
					if prev.isTitled():
						not_matched_titled = False

			# now search for recordings we could add the file to
			# must use list comprehension instead of generator because we
			# might remove entries from recordings lists
			for recording in [rec for rec in prev_recordings if rec.isTitled()]:
				self.addFileToRecording(recording, recording_file, file_merges)
				prev_recordings.remove(recording)
				not_matched_titled = False
			for recording in [rec for rec in next_recordings if rec.isTitled()]:
				self.addFileToRecording(recording, recording_file, file_merges)
				next_recordings.remove(recording)
				not_matched_titled = False

			# if we haven't matched any titled recordings, then keep going
			if not_matched_titled:
				# check for possible merge between two untitled recordings
				# (at this point we know no titled recordings remain, and there
				# is, at most, one recording in each list)
				if prev_recordings and next_recordings:
					self.mergeRecordings(prev_recordings[0], next_recordings[0],
							recording_file, file_merges, file_deletes)
				elif prev_recordings:
					self.addFileToRecording(prev_recordings[0], recording_file,
							file_merges)
				elif next_recordings:
					self.addFileToRecording(next_recordings[0], recording_file,
							file_merges)
				else:
					self.createNewRecordingFromFile(recording_file, None,
							file_merges)


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
		return (Recording(self._recordings_dir, self._recording_extension, row)
				for row in self.runQuery(query))


	def getFilesForRecording(self, recording_id):
		'''Return all files that are part of the given recording'''
		query = ('select file.path from recording_files file, '
				'recording_links link where file.id = link.file_id '
				' and link.recording_id = %d' % recording_id)
		return self.runQueryRaw(query)


	def getFilesForRecordingAbsolute(self, recording_id):
		'''Return all files that are part of the given recording'''
		query = ('select file.path from recording_files file, '
				'recording_links link where file.id = link.file_id '
				' and link.recording_id = %d' % recording_id)
		return [os.path.join(self._root_dir,path[0]) for path in
				self.runQueryRaw(query)]


	def findFilesWithoutRecording(self):
		'''Scan the recording files and links tables to find files not
			associated with a recording'''
		query = ('select file.id,file.path from %(files_table)s file left join '
				'%(links_table)s link on file.id = link.file_id '
				'where link.file_id is null order by file.path'
				% {'files_table': FILES_TABLE, 'links_table': LINKS_TABLE})
		return (RecordingFile(row, self._file_duration) for row in
				self.runQuery(query))


	def mergeRecordings(self, prev_recording, next_recording, recording_file,
			file_merges, file_deletes):
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

		# get previous merges related to these records
		if file_merges.has_key(prev_recording.id):
			prev_merges = file_merges[prev_recording.id]
		else:
			prev_merges = [prev_recording.abspath]
		if file_merges.has_key(next_recording.id):
			next_merges = file_merges[next_recording.id]
			# remove the obsolete entry
			del file_merges[next_recording.id]
		else:
			next_merges = [next_recording.abspath]

		if recording_file:
			# add new file to links
			query = ('insert into "%s" values(NULL, %d, %d)'
					% (LINKS_TABLE, prev_recording.id, recording_file.id))
			self.runQueryRaw(query)
			prev_merges.append(recording_file.path)

		prev_merges.extend(next_merges)
		file_merges[prev_recording.id] = prev_merges

		# remember the file to be deleted
		file_deletes.append(next_recording.abspath)


	def addFileToRecording(self, recording, recording_file, file_merges):
		assert isinstance(recording, Recording), ('next_recording must be '
				'a Recording, not a %s' % next_recording.__class__.__name__)
		assert isinstance(recording_file, RecordingFile), (
				'recording_file must be a RecordingFile, not a %s'
				% recording_file.__class__.__name__)

		# get previous merges related to this record
		if file_merges.has_key(recording.id):
			merges = file_merges[recording.id]
		else:
			merges = [recording.abspath]

		# expand the recording to include the time covered by the file
		if recording_file.start < recording.start_time:
			assignment = 'start_time="%s"' % recording_file.start.isoformat()
			merges.insert(0, recording_file.path)
		elif recording_file.finish > recording.finish_time:
			assignment = 'finish_time="%s"' % recording_file.finish.isoformat()
			merges.append(recording_file.path)
		else:
			raise ValueError('file "%s" does not expand recording "%s"'
					% (recording_file, recording))

		self.runQueryRaw('update "%s" set %s where id=%d'
					% (RECORDINGS_TABLE, assignment, recording.id))
		file_merges[recording.id] = merges

		# link the file to the recording
		query = ('insert into "%s" values(NULL, %d, %d)'
				% (LINKS_TABLE, recording.id, recording_file.id))
		self.runQueryRaw(query)


	def createNewRecordingFromFile(self, recording_file, matched_schedule,
			file_merges):
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
				matched_schedule.finish.isoformat(),
				))
		self.runQueryRaw(query)

		# get the new recording_id
		query = ('select id from "%s" where title = "%s" '
				'and start_time = "%s" and finish_time = "%s" '
				% (RECORDINGS_TABLE, matched_schedule.title,
				recording_file.start.isoformat(),
				recording_file.finish.isoformat()))
		recording_id, = self.runQueryRawSingleResponse(query)

		# add link between file and recording
		query = ('insert into %s values (NULL, %d, %d) '
				% (LINKS_TABLE, recording_id, recording_file.id))
		self.runQueryRaw(query)

		# add file operation
		file_merges[recording_id] = [recording_file.path]


	def getRootDir(self):
		return self._config.getValue2(CAPTURE_SECTION_NAME,
				CAPTURE_ROOT_DIR_KEY)


	def getRecordingExtension(self):
		return self._config.getValue2(CAPTURE_SECTION_NAME,
				CAPTURE_FILE_EXT_KEY)


	def getFileDuration(self):
		return datetime.timedelta(seconds = int(self._config.getValue2(
				CAPTURE_SECTION_NAME, CAPTURE_BUFFER_FRAMES_KEY))
				/ int(self._config.getValue2(CAPTURE_SECTION_NAME,
						CAPTURE_SAMPLING_RATE)))


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

	__REQUIRED_TABLES = {PREVIOUS_CONNECTIONS_TABLE: 'name TEXT, address TEXT, '
			'last_connected TEXT, times_connected INTEGER'
			}


	def __init__(self, database_file, root_dir):
		Storage.__init__(self, database_file, root_dir)

		# ensure all required tables exist
		self._ensureTablesExist(ProxyStorage.__REQUIRED_TABLES)


	def addConnection(self, name, address):
		# check it's not a known address
		response = self.runQuerySingleResponse(
				'select name, times_connected from previous_connections '
				'where address="%s"' % address)
		if response:
			times_connected = response['times_connected'] + 1
			self.runQueryRaw('update previous_connections set name="%s", '
					'last_connected="%s", times_connected=%d where address="%s"'
					% (name, getCurrentTimeCString(), times_connected,
						address))
		else:
			self.runQueryRaw('insert into previous_connections values '
					'("%s", "%s", "%s", 1)'
					% (name, address, getCurrentTimeCString()))


	def removeConnection(self, address):
		self.runQueryRaw('delete from previous_connections where address="%s"'
				% address)


	def getPreviousConnections(self, sort_key='last_connected',
			sort_order='asc'):
		query = ('select * from previous_connections order by %s %s'
				% (sort_key , sort_order))
		return self.runQuery(query)



class Recording():
	def __init__(self, recordings_dir, extension, dict):
		self.id = int(dict['id'])
		self.title = dict['title']
		self.start_date = parseDateIso(dict['start_date'])
		self.start_time = parseDateTimeIso(dict['start_time'])
		self.finish_time = parseDateTimeIso(dict['finish_time'])
		self.start_limit = parseDateTimeIso(dict['start_limit'])
		self.finish_limit = parseDateTimeIso(dict['finish_limit'])
		self.extension = extension
		date_str = formatDateIso(self.start_date)
		self.path = '%s/%s %s %s%s' % (date_str, self.title, date_str,
				formatTimeUI(self.start_time), extension)
		self.abspath = os.path.join(recordings_dir, self.path)

	def isTitled(self):
		return self.title != UNTITLED

	def fileExists(self):
		return os.path.exists(self.abspath)

	def __str__(self):
		return '%s (%d): %s - %s [%s - %s]' % (self.title, self.id,
				formatTimeUI(self.start_time), formatTimeUI(self.finish_time),
				formatTimeUI(self.start_limit), formatTimeUI(self.finish_limit))

 	def getSubRangeFile(self, start, finish):
		# open a tempfile, then use its name as the destination for sox
		with tempfile.NamedTemporaryFile(suffix=self.extension) as tmp:
			# use sox's trim command which takes 2 args: start_offset & duration
			command = [SOX_PATH, '-q', self.abspath, tmp.name,
					'trim']
			# determine the start and finish times to send to trim
			if start:
				command.append(formatTimeDelta(start - self.start_time))
			else:
				command.append('0')
			if finish:
				command.append(formatTimeDelta(finish - start))
			subprocess.call(command)
			return tmp.name


class RecordingFile():
	def __init__(self, dict, duration):
		self.id = int(dict['id'])
		self.path = dict['path']
		self.finish = getTimestampFromFilePath(self.path)
		self.start = self.finish - duration

	def __str__(self):
		return '%s (%d): %s - %s' % (self.path, self.id,
				formatTimeUI(self.start, show_seconds=True),
				formatTimeUI(self.finish, show_seconds=True))


def getTimestampFromFilePath(file_path):
	return parseDateTimeIso(os.path.splitext(file_path.replace(
			'/', 'T'))[0])


def concatSoundFiles(dest, sources):
	'''concatenate all the source audio files to the destination file.
		TODO: look into using sox's "splice" command to smooth joins.'''

	# make sure destination directory exists
	ensureDirectoryExists(os.path.dirname(dest))

	# escape spaces for the shell
	uncompressed_dest = dest.replace('.wv', '.wav')

	# I've discovered that the quickest way to concat wavpack files
	# is to concat them (with sox) to an uncompressed file, then compress it

	# make sure the uncompressed version of the destination file
	# doesn't exist already (to prevent questions from sox)
	if os.path.exists(uncompressed_dest):
		os.remove(uncompressed_dest)

	# construct command for sox, then run it
	command = [SOX_PATH, '-q']
	command.extend(sources)
	command.append(uncompressed_dest)
	subprocess.call(command)

	# make sure the destination file doesn't exist already
	# (to prevent questions from wavpack)
	if os.path.exists(dest):
		os.remove(dest)

	# run wavpack command (not using outfile argument because wavpack treats
	# it as an input (-d used to delete raw wav file)
	command = [WAVPACK_PATH, '-q', '-d', uncompressed_dest]
	subprocess.call(command)


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
