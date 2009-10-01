import os, sys
from bowerbird.configobj import ConfigObj
from lib.odict import OrderedDict

# keys for cache
K_OBJ = 'config_obj'
K_TIME = 'timestamp'
K_VAL = 'values'
K_DICT = 'dict'

# clumsy way of doing an enum for cache value indices
I_NAME = 0
I_SUBNAME = I_NAME + 1
I_KEY = I_SUBNAME + 1
I_VAL = I_KEY + 1
I_TYPE = I_VAL + 1
I_UNIT = I_TYPE + 1


class ConfigParser(object):

	def __init__(self, config_filename, config_defaults_filename=None):
		self.filename = config_filename
		self.cache = {}
		self.read_from_file(self.filename, self.cache)

		self.defaults_filename = config_defaults_filename
		self.defaults_cache = {}
		if config_defaults_filename:
			self.read_from_file(self.defaults_filename, self.defaults_cache)

	def raw_values(self):
		return self.cache[K_DICT]

	def values(self):
		# update if file has been modified
		self.read_from_file(self.filename, self.cache)
		return self.cache[K_VAL]

	def default_values(self):
		# update if file has been modified
		if self.defaults_filename:
			self.read_from_file(self.defaults_filename, self.defaults_cache)
		return self.defaults_cache[K_VAL]

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
		# update cache
		if not self.cache[K_DICT].has_key(key):
			self.cache[K_DICT][key][I_VAL] = value
		else:
			# create dummy entry (which won't have spec info)
			self.cache[K_DICT][key] = ['','',key,value,'','']

		# update config obj
		section, s_key = key.split('.')
		if not self.cache[K_OBJ].has_key(section):
			self.cache[K_OBJ][section] = {}
		self.cache[K_OBJ][section][s_key] = value

	def set_value2(self, section, key, value):
		self.set_value1('.'.join(section, key))

	def save_to_file(self):
		# update file
		with open(self.filename, 'w') as save_file:
			self.cache[K_OBJ].write(save_file)
	
	def read_from_file(self, filename, cache):
		if cache and cache.has_key(K_OBJ):
			# if already loaded and file hasn't changed then use cached value
			# if config_obj exists, timestamp should too
			obj_timestamp = os.stat(filename).st_mtime
			if cache[K_TIME] < obj_timestamp:
				cache[K_OBJ].reload()
				cache[K_TIME] = obj_timestamp
		else:
			cache[K_OBJ] = ConfigObj(filename)
			cache[K_TIME] = os.stat(filename).st_mtime

		cache[K_VAL] = OrderedDict()
		cache[K_DICT] = OrderedDict()
		for section in cache[K_OBJ]:
			section_dict = OrderedDict()
			# get last line of comment for section
			section_comment = cache[K_OBJ].comments[section][-1:]
			if section_comment and section_comment[0]:
				# remove '#' and spare whitespace
				section_name = section_comment[0][1:].strip()
			else:
				section_name = section

			for key in cache[K_OBJ][section]:
				option_id = '.'.join((section, key))
				key_comment = cache[K_OBJ][section].comments[key][-1:]
				name = None
				if key_comment and key_comment[0]:
					# remove '#' then split and strip the spec comment
					specs = [s.strip() for s in key_comment[0][1:].split(',')]
					if len(specs) == 4:
						(name, subname, input_type, units) = specs
				if name == None:
					# no (or invalid) spec in last comment line so use defaults
					name = key
					subname = ''
					input_type = 'text'
					units = ''
				if not section_dict.has_key(name):
					section_dict[name] = []
				values = [name, subname, option_id, cache[K_OBJ][section][key], input_type, units]
				# store here for display purposes
				section_dict[name].append(values)
				# store here for easy access
				cache[K_DICT][option_id] = values
			if section_dict:
				cache[K_VAL][section_name] = section_dict
