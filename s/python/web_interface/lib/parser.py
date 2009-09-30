import os, sys
from bowerbird.configobj import ConfigObj
from lib.odict import OrderedDict

# clumsy way of doing an enum for cache indices
K_OBJ = 'config_obj'
K_TIME = 'timestamp'
K_DESC = 'descriptions'
K_DESC_E = 'description errors'
K_VAL = 'values'
K_VAL_E = 'value errors'


class Config(object):

	def __init__(self, config_filename, config_desc, config_defaults_filename=None):
		# next line is to be removed when we move the description into the comments
		self.config_desc = config_desc
		self.filename = config_filename
		self.cache = {}
		self.read_from_file(self.filename, self.cache)

		self.defaults_filename = config_defaults_filename
		self.defaults_cache = {}
		if config_defaults_filename:
			self.read_from_file(self.defaults_filename, self.defaults_cache)

	def values(self):
		# update if file has been modified
		self.read_from_file(self.filename, self.cache)

		return (self.cache[K_DESC], self.cache[K_VAL], 
				self.cache[K_DESC_E], self.cache[K_VAL_E])

	def default_values(self):
		# update if file has been modified
		if config_defaults_filename:
			self.read_from_file(self.defaults_filename, self.defaults_cache)

		return (self.defaults_cache[K_DESC], self.defaults_cache[K_VAL], 
				self.defaults_cache[K_DESC_E], self.defaults_cache[K_VAL_E])

	def get_value1(self, key):
		# update if file has been modified
		self.read_from_file(self.filename, self.cache)

		# use try/catch because most of the time key will be there
		try:
			return self.cache[K_VAL][key]
		except NameError:
			return None

	def get_value2(self, section, key):
		return self.get_value1('.'.join((section, key)))

	def set_value1(self, key, value):
		# update cache
		self.cache[K_VAL][key] = value
		# update config obj
		section, s_key = key.split('.')
		if not self.config_obj.has_key(section):
			self.config_obj[section] = {}
		self.config_obj[section][s_key] = value

	def set_value2(self, section, key, value):
		self.set_value1('.'.join(section, key))

	def save_to_file(self):
		# update file
		with open(self.filename, 'w') as save_file:
			self.config_obj.write(save_file)
	
	def read_from_file(self, filename, cache):
		config_desc, config_keys, config_desc_errors = self.parseBinaryConfigDesc()
		cache[K_DESC] = config_desc
		cache[K_DESC_E] = config_desc_errors
		config_values, config_errors \
				= self.parseBinaryConfig(config_keys, filename, cache)
		cache[K_VAL] = config_values
		cache[K_VAL_E] = config_errors

	def parseBinaryConfig(self, valid_keys, config_filename, cache):
		if cache and cache.has_key(K_OBJ):
			# if already loaded and file hasn't changed then use cached value
			# if config_obj exists, timestamp should too
			obj_timestamp = os.stat(config_filename).st_mtime
			if cache[K_TIME] < obj_timestamp:
				cache[K_OBJ].reload()
				cache[K_TIME] = obj_timestamp
		else:
			cache[K_OBJ] = ConfigObj(config_filename)
			cache[K_TIME] = os.stat(config_filename).st_mtime

		values = {}
		errors = []
		for section in cache[K_OBJ]:
			for option in cache[K_OBJ][section]:
				option_id = '.'.join((section, option))
				if option_id in valid_keys:
					values[option_id] = cache[K_OBJ][section][option]
				else:
					# warn if config has keys not in the description
					errors.append('Unknown key "%s" (value: "%s")' % (option_id, cache[K_OBJ][section][option]))
		return values, errors

	def parseBinaryConfigDesc(self):
		# TODO if already loaded and file hasn't changed (name or timestamp) then use cached value
		config = ConfigObj(self.config_desc, list_values=False)
		keys = []
		errors = []
		desc = OrderedDict()
		for section in config:
			section_dict = OrderedDict()
			section_name = config[section]['section']
			for option in config[section]:
				if option != 'section':
					option_id = '.'.join((section, option))
					keys.append(option_id)
					values = [s.strip() for s in config[section][option].split(',')]
					if len(values) == 4:
						(name,  subname,  input_type,  units) = values
						if not section_dict.has_key(name):
							section_dict[name] = OrderedDict()
						section_dict[name][subname] = (option_id,  input_type,  units)
					else:
						errors.append('%s : "%s" has ill-formed value: "%s"'
								% (section_name,  option, config[section][option]))
			if section_dict:
				desc[section_name] = section_dict
		return desc, keys, errors

