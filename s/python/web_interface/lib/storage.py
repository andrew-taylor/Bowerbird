import sqlite3
from threading import Thread
from Queue import Queue
from lib.odict import OrderedDict

class Storage(Thread):
	'''Handles the details of storing data about calls and categories'''
	
	__created = False
	
	def __init__(self,  database_path):
		assert not Storage.__created
		Thread.__init__(self)
		self.path = database_path
		self.query_queue = Queue()
		
		Storage.__created = True
		self.start()
	
	def stop(self,  *args):
		self.query_queue.put(self.SqlQuery('', isStop=True))

	def run(self):
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
			except:
				result.append(sqlite3.Row(conn.cursor(), ('Query Failed: "%s"' %  command, )))
			if commitneeded: 
				conn.commit()
			query.response.put(result)
			# get the next query
			query = self.query_queue.get()
		
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
			return query.response.get()

	def runQuerySingle(self, query):
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
			for key in line.keys():
				values[key] = line[key]
			list.append(values)
		return list
	
	def getCategories(self, sort_key=None, sort_order='asc'):
		if sort_key:
			# always use date_and_time column as secondary for sorting
			query = 'select * from categories order by %s %s' % (sort_key , sort_order)
		else:
			query = 'select * from categories'
		
		return self.runQuery(query)
		
	def getCategoryNames(self, sort_key=None, sort_order='asc'):
		if sort_key:
			query = 'select label from categories order by "%s" %s' % (sort_key , sort_order)
		else:
			query = 'select label from categories'
			
		list = []
		for line in self.__execSqlCmd(query):			
			list.append(line['label'])
		return list
		
	def getCategory(self, label):
		return self.runQuerySingle('select * from categories where label="%s"' %  label)
		
	def updateCategory(self, old_label,  new_label):
		self.runQuery('update categories set label="%s" where label="%s"' % (new_label, old_label))
		self.runQuery('update calls set category="%s" where category="%s"' % (new_label, old_label))

	def getCalls(self, sort_key='date_and_time', sort_order='asc',  category=None):
		if category:
			query = 'select * from calls where category="%s" order by %s %s' % (category, sort_key , sort_order)
		else:
			query = 'select * from calls order by %s %s' % (sort_key , sort_order)

		return self.runQuery(query)
		
	def getCall(self, filename):
		return self.runQuerySingle('select * from calls where filename="%s"' %  filename)
		
	def updateCall(self, filename, label, category, example):
		if filename:
			if example:
				example = 'checked'
			else:
				example = ''
			self.__execSqlCmd('update calls set label="%s",category="%s",example="%s" where filename="%s"'
					% (label, category, example, filename))
	
	def getPrevAndNextCalls(self, call):
		files = {}
		files['prev'] = self.runQuerySingle('select * from calls where date_and_time < "%s" order by date_and_time desc limit 1' 
				% call['date_and_time'])
		files['prev_in_cat'] = self.runQuerySingle('select * from calls where category = "%s" and date_and_time < "%s" order by date_and_time desc limit 1' 
				% (call['category'],  call['date_and_time']))
		files['next'] = self.runQuerySingle('select * from calls where date_and_time > "%s" order by date_and_time asc limit 1' 
				% call['date_and_time'])
		files['next_in_cat'] = self.runQuerySingle('select * from calls where category = "%s" and date_and_time > "%s" order by date_and_time asc limit 1' 
				% (call['category'],  call['date_and_time']))
		return files
