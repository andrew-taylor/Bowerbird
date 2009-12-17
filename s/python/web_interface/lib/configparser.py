import os, sys, time, string
from copy import deepcopy
from bowerbird.configobj import ConfigObj
from lib.odict import OrderedDict
from lib.common import SECTION_META_KEY

COMMENTS_LINE_DELIMITER = '___'

# scheduled capture section is handled differently
SCHEDULE_SECTION = 'scheduled_capture'
SCHEDULE_COMMENTS = '''
# This section stores the times for scheduled captures.
It will always be moved back to the top, so don't bother moving it.'''
CONFIG_HEADER = '''# configuration file for bowerbird deployment

# Some formatting of comments is required to provide nice looking edit
# pages. General explanatory comments are welcome, but the last comment 
# line just before an item should be as described in the next
# paragraph.

# Comment just before section should be a display version of the section name
# Comment just before key-value pairs should be 4 comma separated values:
# 1) display name for value (this should be the for value groups such as a
# 		min-max range pair)
# 2) display subname for values that are part of a group
# 3) variable type (float, string, int, time, etc) - Currently ignored
# 4) units for value (for display)

# leave this section header here to workaround 
# a strangeness in comment parsing'''

# keys for cache
K_OBJ = 'config_obj'
K_TIME = 'timestamp'
K_VAL = 'values'
K_DICT = 'dict'
K_SCHED = 'sched'

# clumsy way of doing an enum for cache value indices
I_NAME = 0
I_SUBNAME = I_NAME + 1
I_KEY = I_SUBNAME + 1
I_VAL = I_KEY + 1
I_TYPE = I_VAL + 1
I_UNITS = I_TYPE + 1


class ConfigParser(object):

	def __init__(self, config_filename, config_defaults_filename=None):
		self.filename = config_filename
		self.cache = {}
		self.read_from_file(self.filename, self.cache)

		self.defaults_filename = config_defaults_filename
		self.defaults_cache = {}
		if config_defaults_filename:
			self.read_from_file(self.defaults_filename, self.defaults_cache)

	def get_raw_values(self):
		return deepcopy(self.cache[K_DICT])

	def get_values(self):
		# update if file has been modified
		self.read_from_file(self.filename, self.cache)
		# return a copy
		return deepcopy(self.cache[K_VAL])

	def get_default_values(self):
		# update if file has been modified
		if self.defaults_filename:
			self.read_from_file(self.defaults_filename, self.defaults_cache)
			# return a copy
			return deepcopy(self.defaults_cache[K_VAL])
		return None

	def get_value1(self, key):
		# update if file has been modified
		self.read_from_file(self.filename, self.cache)

		# use try/catch because most of the time key will be there
		try:
			return self.cache[K_DICT][key][I_VAL]
		except KeyError:
			return None

	def get_value2(self, section, key):
		return self.get_value1('.'.join((section, key)))

	def set_value1(self, key, value):
		# update config obj
		section, s_key = key.split('.')
		if not self.cache[K_OBJ].has_key(section):
			self.cache[K_OBJ][section] = {}
		self.cache[K_OBJ][section][s_key] = value
		# update cache(s)
		self.update_cache_from_configobj(self.cache)

	def set_value2(self, section, key, value):
		self.set_value1('.'.join(section, key))

	def set_meta1(self, key, meta):
		# update config obj
		section, s_key = key.split('.')
		if not self.cache[K_OBJ].has_key(section):
			self.cache[K_OBJ][section] = {}
		if not self.cache[K_OBJ][section].has_key(s_key):
			self.cache[K_OBJ][section][s_key] = ''
		meta_bits = meta.split(',')
		(name, subname, type, units) = map(string.strip, meta_bits[:4])
		description = ','.join(meta_bits[4:]).strip()

		# don't write the description lines if they're empty 
		# (otherwise, it inserts a blank line)
		if description:
			self.cache[K_OBJ][section].comments[s_key] = description.split(
					COMMENTS_LINE_DELIMITER)

		# don't write the meta-information line if it's the defaults
		if name != s_key or subname != '' or type != 'string' or units != '':
			self.cache[K_OBJ][section].comments[s_key].append(
					"# %s, %s, %s, %s" % (name, subname, type, units))
		# update cache(s)
		self.update_cache_from_configobj(self.cache)

	def set_meta2(self, section, key, value):
		self.set_meta1('.'.join(section, key))

	def set_smeta(self, section, meta):
		# update config obj
		if not self.cache[K_OBJ].has_key(section):
			self.cache[K_OBJ][section] = {}
		index = meta.find(',')
		name = meta[:index]
		description = meta[index+1:].strip()

		# if the section comments don't start with a blank line then add one
		# (to separate it from the next section)
		if not description.startswith(COMMENTS_LINE_DELIMITER):
			self.cache[K_OBJ].comments[section] = ['']
		
		# don't write the description lines if they're empty 
		# (otherwise, it inserts a blank line)
		if description:
			self.cache[K_OBJ].comments[section].extend(description.split(
					COMMENTS_LINE_DELIMITER))

		# don't write the meta-information line if it's the defaults
		if name != section:
			self.cache[K_OBJ].comments[section].append("# " + name)
		# update cache(s)
		self.update_cache_from_configobj(self.cache)

	def clear_config(self):
		'''delete all config entries from cache (not including schedule)'''
		# clear all values from both cache and raw cache
		self.cache[K_VAL].clear();
		self.cache[K_DICT].clear();

		# clear the config obj
		self.cache[K_OBJ].clear();
		self.cache[K_OBJ].initial_comment = CONFIG_HEADER.split('\n')
		self.cache[K_OBJ]['dummy'] = {}
		# restore the schedule
		self.cache[K_OBJ][SCHEDULE_SECTION] = {}
		self.cache[K_OBJ].comments[SCHEDULE_SECTION] \
				= SCHEDULE_COMMENTS.split('\n')
		for label in self.cache[K_SCHED]:
			self.cache[K_OBJ][SCHEDULE_SECTION][label] \
					= self.cache[K_SCHED][label]

		# update timestamp
		self.cache[K_TIME] = time.time();

	def get_schedules(self):
		# update if file has been modified
		self.read_from_file(self.filename, self.cache)
		# return a copy
		return deepcopy(self.cache[K_SCHED])

	def get_default_schedules(self):
		# update if file has been modified
		if self.defaults_filename:
			self.read_from_file(self.defaults_filename, self.defaults_cache)
			# return a copy
			return deepcopy(self.defaults_cache[K_SCHED])
		return None

	def get_schedule(self, label):
		return self.cache[K_SCHED][label]

	def set_schedule(self, label, schedule):
		self.cache[K_SCHED][label] = schedule

		# update config obj
		if not self.cache[K_OBJ].has_key(SCHEDULE_SECTION):
			self.cache[K_OBJ][SCHEDULE_SECTION] = {}
		self.cache[K_OBJ][SCHEDULE_SECTION][label] = schedule
	
	def delete_schedule(self, label):
		del(self.cache[K_SCHED][label])
		# update config obj
		del(self.cache[K_OBJ][SCHEDULE_SECTION][label])

	def clear_schedules(self):
		self.cache[K_SCHED].clear()
		# update config obj
		self.cache[K_OBJ][SCHEDULE_SECTION].clear()

	def save_to_file(self):
		try:
			# update file
			with open(self.filename, 'w') as save_file:
				self.cache[K_OBJ].write(save_file)
		except:
			# if this fails we should re-read the file to keep them synced
			self.cache[K_OBJ].reload()
			self.cache[K_TIME] = os.stat(self.cache[K_OBJ].filename).st_mtime
			raise

	def read_from_file(self, filename, cache):
		if cache and cache.has_key(K_OBJ):
			# if already loaded and file hasn't changed then use cached value
			# if config_obj exists, timestamp should too
			obj_timestamp = os.stat(filename).st_mtime
			if cache[K_TIME] < obj_timestamp:
				cache[K_OBJ].reload()
				cache[K_TIME] = obj_timestamp
		else:
			cache[K_OBJ] = ConfigObj(filename, list_values=False, 
					write_empty_values=True)
			cache[K_TIME] = os.stat(filename).st_mtime

		self.update_cache_from_configobj(cache)

	def update_cache_from_configobj(self, cache):
		cache[K_VAL] = OrderedDict()
		cache[K_DICT] = OrderedDict()
		cache[K_SCHED] = OrderedDict()
		for section in cache[K_OBJ]:
			if section == SCHEDULE_SECTION:
				# add to sched structure
				for label in cache[K_OBJ][section]:
					cache[K_SCHED][label] = cache[K_OBJ][section][label]
			else:
				section_dict = OrderedDict()
				section_comments = cache[K_OBJ].comments[section]
				# get last line of comment for section
				if section_comments and section_comments[-1:][0]:
					# remove '#' and spare whitespace
					section_name = section_comments[-1:][0][1:].strip()
					section_description = COMMENTS_LINE_DELIMITER.join(
							section_comments[:-1])
					# add an entry with the section metadata (conforming to the
					# shape of the other entries)
					section_dict[SECTION_META_KEY] = [(section_name, '', 
							section, '', '', '', section_description)]

				for key in cache[K_OBJ][section]:
					option_id = '.'.join((section, key))
					key_comment = cache[K_OBJ][section].comments[key][-1:]
					name = None
					if key_comment and key_comment[0]:
						# remove '#' then split and strip the spec comment
						specs = [s.strip() for s in \
								key_comment[0][1:].split(',')]
						if len(specs) == 4:
							(name, subname, input_type, units) = specs
							description = COMMENTS_LINE_DELIMITER.join(
									cache[K_OBJ][section].comments[key][:-1])
					if name == None:
						# no (or invalid) spec in last comment so use defaults
						name = key
						subname = ''
						input_type = 'string'
						units = ''
						description = COMMENTS_LINE_DELIMITER.join(
								cache[K_OBJ][section].comments[key])
					if not section_dict.has_key(name):
						section_dict[name] = []
					values = (name, subname, option_id, 
							cache[K_OBJ][section][key], input_type, units,
							description)
					# store here for display purposes
					section_dict[name].append(values)
					# store here for easy access
					cache[K_DICT][option_id] = values
				if section_dict:
					cache[K_VAL][section] = section_dict

	def parse_file(self, file):
		'''
		Parse the new file and return the values, but don't cache them.
		The argument to this method can be anything that ConfigObj can take as an
		infile parameter to its constructor. This includes filenames, lists of
		strings, a dictionary, or anything with a read method.
		'''
		cache = {}
		cache[K_OBJ] = ConfigObj(file, list_values=False, write_empty_values=True)
		self.update_cache_from_configobj(cache)

		return cache[K_VAL]
