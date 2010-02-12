import sqlite3
from datetime import datetime
from threading import Thread
from Queue import Queue
from lib.odict import OrderedDict

class Storage(Thread):
	'''Handles the details of storing data in an sqlite db'''

	COMMAND_KEY = 'command'
	ERROR_KEY = 'error'
	REQUIRED_TABLES = []
	REQUIRED_TABLE_INIT = {}

	__created = False

	def __init__(self, database_path):
		assert not Storage.__created
		Thread.__init__(self)
		self.path = database_path
		self.query_queue = Queue()

		Storage.__created = True
		self.start()

	def stop(self, *args):
		self.query_queue.put(self.SqlQuery('', isStop=True))

	def run(self):
		try:
			conn = sqlite3.connect(self.path)
			conn.row_factory = sqlite3.Row
			query = self.query_queue.get()
			while not query.isStop:
				commitneeded = False
				result = []
				try:
					for command in query.commands:
						cursor = conn.execute(command)
						if not command.upper().startswith("SELECT"):
							commitneeded = True
						for row in cursor.fetchall():
							result.append(row)
					if commitneeded:
						conn.commit()
				except Exception as e:
					result.append({self.COMMAND_KEY : command, self.ERROR_KEY : e})
				query.response.put(result)
				# get the next query
				query = self.query_queue.get()
		except Exception as e:
			print "Exception in thread:", e
		finally:
			# shutdown database connection
			conn.close()
			Storage.__created = False

	class SqlQuery():
		def __init__(self, commands, isStop=False):
			self.commands = commands
			self.response = Queue()
			self.isStop = isStop

	def __execSqlCmd(self, command):
			return self.__execSqlList([command])

	def __execSqlList(self, commands):
			query = self.SqlQuery(commands)
			self.query_queue.put(query)
			response = query.response.get()
			if response and self.ERROR_KEY in response[-1].keys():
				raise response[-1][self.ERROR_KEY]
			return response

	def runQuerySingleResponse(self, query):
		response = self.__execSqlCmd(query)
		values = OrderedDict()
		if response:
			for key in response[0].keys():
				values[key] = response[0][key]
		return values

	def runQuery(self, query):
		list = []
		for line in self.__execSqlCmd(query):
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
		except sqlite3.Error:
			return False;

	def createRequiredTables(self):
		for table in self.REQUIRED_TABLES:
			try:
				self.runQuery('select * from %s' % table)
			except sqlite3.Error:
				self.runQuery('create table %s (%s)'
						% (table, self.REQUIRED_TABLE_INIT[table]))


class BowerbirdStorage(Storage):
	'''Handles the details of storing data about calls and categories'''

	REQUIRED_TABLES = ['categories', 'calls']
	REQUIRED_TABLE_INIT = {
			'categories' : 'label text, calls integer, length real, '
				'length_stddev real, frequency real, frequency_stddev real, '
				'harmonics_min integer, harmonics_max integer, AM text, '
				'FM text, bandwidth real, bandwidth_stddev real, '
				'energy real, energy_stddev real, time_of_day_start text, '
				'time_of_day_finish text',
			'calls' : 'example boolean, filename text, '
				'date_and_time timestamp, label text, category text, '
				'length real, frequency real, harmonics integer, AM text, '
				'FM text, bandwidth real, energy real'
			}

	def getCategories(self, sort_key=None, sort_order='asc'):
		if sort_key:
			query = ('select * from categories order by %s %s'
					% (sort_key , sort_order))
		else:
			query = 'select * from categories'

		try:
			return self.runQuery(query)
		except sqlite3.Error, inst:
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
			for line in self.__execSqlCmd(query):
				list.append(line['label'])
		except sqlite3.Error:
			# just ignore missing table for now
			pass
		return list

	def getCategory(self, label):
		try:
			return self.runQuerySingleResponse(
					'select * from categories where label="%s"' % label)
		except sqlite3.Error:
			# just ignore missing table for now
			pass
		return OrderedDict()

	def updateCategory(self, old_label, new_label):
		try:
			self.runQuery('update categories set label="%s" where label="%s"'
					% (new_label, old_label))
			self.runQuery('update calls set category="%s" where category="%s"'
					% (new_label, old_label))
		except sqlite3.Error:
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
		except sqlite3.Error:
			# just ignore missing table for now
			pass
		return []

	def getCall(self, filename):
		try:
			return self.runQuerySingleResponse(
					'select * from calls where filename="%s"' % filename)
		except sqlite3.Error:
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
				self.__execSqlCmd('update calls set label="%s",category="%s",'
						'example="%s" where filename="%s"'
					% (label, category, example, filename))
			except sqlite3.Error:
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
		except sqlite3.Error:
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
		except sqlite3.Error, inst:
			# just ignore errors for now
			print inst


	def getPreviousConnections(self, sort_key='last_connected', sort_order='asc'):
		query = ('select * from previous_connections order by %s %s'
				% (sort_key , sort_order))
		try:
			return self.runQuery(query)
		except sqlite3.Error, inst:
			# just ignore errors for now
			print inst
		return []


def get_formatted_current_time():
	return datetime.now().ctime()
