import apsw, datetime
from threading import RLock
from lib.odict import OrderedDict

class Storage(object):
	'''Handles the details of storing data in an sqlite db'''

	REQUIRED_TABLES = []
	REQUIRED_TABLE_INIT = {}

	def __init__(self, database_path):
		self.path = database_path

		self.conn = apsw.Connection(self.path)
		self.conn.setrowtrace(self.__convertToDictionary)
		self.conn.setbusytimeout(1000)
		self.lock = RLock()

	def __del__(self):
		self.conn.close()
	
	def __convertToDictionary(self, cursor, row):
		'''A row trace method to emulate the Row Factory of sqlite3'''
		desc = cursor.getdescription()
		dict = {}
		for i in xrange(len(desc)):
			dict[desc[i][0]] = row[i]
		return dict

	def __execSqlQuery(self, query):
		# ensure that only one thread accesses the database at a time
		with self.lock:
			cursor = self.conn.cursor()
			cursor.execute(query)
			return cursor.fetchall()

	def runQuerySingleResponse(self, query):
		response = self.__execSqlQuery(query)
		values = OrderedDict()
		if response:
			for key in response[0].keys():
				values[key] = response[0][key]
		return values

	def runQuery(self, query):
		list = []
		for line in self.__execSqlQuery(query):
			values = OrderedDict()
			keys = line.keys()
			for key in keys:
				values[key] = line[key]
			list.append(values)
		return list

	def hasRequiredTables(self):
		try:
			for table in self.REQUIRED_TABLES:
				self.runQuery('select * from %s' % table)
			return True
		except apsw.Error:
			return False;

	def createRequiredTables(self):
		for table in self.REQUIRED_TABLES:
			try:
				self.runQuery('select * from %s' % table)
			except apsw.Error:
				self.runQuery('create table %s (%s)'
						% (table, self.REQUIRED_TABLE_INIT[table]))

class Recording(object):
	def __init__(self, dict):
		self.title = dict['title']
		self.date = convertIntToDate(int(dict['date']))
		self.start_time = convertIntToTime(int(dict['start_time']))
		self.finish_time = convertIntToTime(int(dict['finish_time']))


class BowerbirdStorage(Storage):
	'''Handles the details of storing data about calls and categories'''

	REQUIRED_TABLES = ['recordings', 'categories', 'calls']
	REQUIRED_TABLE_INIT = {
			'recordings': 'title text, date int, start_time int, '
				'finish_time int',
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

	def getRecordings(self, date):
		int_date = convertDateToInt(date)
		query = 'select * from recordings where date="%s"' % int_date
		try:
			return [Recording(row) for row in self.runQuery(query)]
		except apsw.Error, inst:
			# just ignore missing table for now
			print inst

		return []


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
		try:
			self.runQuery('update categories set label="%s" where label="%s"'
					% (new_label, old_label))
			self.runQuery('update calls set category="%s" where category="%s"'
					% (new_label, old_label))
		except apsw.Error:
			# just ignore missing table for now
			pass

	def getCalls(self, sort_key='date_and_time', sort_order='asc',
			category=None):
		if category:
			query = ('select * from calls where category="%s" order by %s %s'
					% (category, sort_key , sort_order))
		else:
			query = ('select * from calls order by %s %s'
					% (sort_key , sort_order))

		try:
			return self.runQuery(query)
		except apsw.Error:
			# just ignore missing table for now
			pass
		return []

	def getCall(self, filename):
		try:
			return self.runQuerySingleResponse(
					'select * from calls where filename="%s"' % filename)
		except apsw.Error:
			# just ignore missing table for now
			pass
		return OrderedDict()

	def updateCall(self, filename, label, category, example):
		if filename:
			if example:
				example = 'checked'
			else:
				example = ''
			try:
				self.__execSqlQuery('update calls set label="%s",category="%s",'
						'example="%s" where filename="%s"'
					% (label, category, example, filename))
			except apsw.Error:
				# just ignore missing table for now
				pass

	def getPrevAndNextCalls(self, call):
		files = {}
		try:
			files['prev'] = self.runQuerySingleResponse('select * from calls '
					'where date_and_time < "%s" order by date_and_time desc '
					'limit 1' % call['date_and_time'])
			files['prev_in_cat'] = self.runQuerySingleResponse(
					'select * from calls where category = "%s" and '
					'date_and_time < "%s" order by date_and_time desc limit 1'
					% (call['category'], call['date_and_time']))
			files['next'] = self.runQuerySingleResponse('select * from calls '
					'where date_and_time > "%s" order by date_and_time asc '
					'limit 1' % call['date_and_time'])
			files['next_in_cat'] = self.runQuerySingleResponse(
					'select * from calls where category = "%s" and '
					'date_and_time > "%s" order by date_and_time asc limit 1'
					% (call['category'], call['date_and_time']))
		except apsw.Error:
			# just ignore missing table for now
			pass

		return files


class ProxyStorage(Storage):
	'''Handles the details of storing info about previous connections made,
	   and recordings in the database'''

	REQUIRED_TABLES = ['previous_connections']
	REQUIRED_TABLE_INIT = {
			'previous_connections' : 'name, address, last_connected, '
					'times_connected'
			}

	def addConnection(self, name, address):
		print "addConnection",name, address
		# check it's not a known address
		response = self.runQuerySingleResponse(
				'select name, times_connected from previous_connections '
				'where address="%s"' % address)
		print response
		if response:
			times_connected = response['times_connected'] + 1
			self.runQuery('update previous_connections set name="%s", '
					'last_connected="%s", times_connected=%d where address="%s"'
					% (name, get_formatted_current_time(), times_connected,
						address))
		else:
			self.runQuery('insert into previous_connections values '
					'("%s", "%s", "%s", 1)'
					% (name, address, get_formatted_current_time()))


	def removeConnection(self, address):
		try:
			self.runQuery('delete from previous_connections where address="%s"'
					% address)
		except apsw.Error, inst:
			# just ignore errors for now
			print inst


	def getPreviousConnections(self, sort_key='last_connected', sort_order='asc'):
		query = ('select * from previous_connections order by %s %s'
				% (sort_key , sort_order))
		try:
			return self.runQuery(query)
		except apsw.Error, inst:
			# just ignore errors for now
			print inst
		return []


def convertTimeToInt(time):
	return time.hour * 10000 + time.minute * 100 + time.second

def convertIntToTime(time_int):
	assert time_int >= 10**4, ('invalid time int: %d is less than %d' 
			% (time_int, 10**4))
	return datetime.time(time_int / 10000, (time_int / 100) % 100, 
			time_int % 100)

def convertDateToInt(date):
	return date.year * 10000 + date.month * 100 + date.day

def convertIntToDate(date_int):
	assert date_int >= 10**7, ('invalid date int: %d is less than %d'
			% (date_int, 10*7))
	return datetime.date(date_int / 10000, (date_int / 100) % 100, 
			date_int % 100)

def get_formatted_current_time():
	return datetime.datetime.now().ctime()
