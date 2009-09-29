import cherrypy,  os,  sys
from ConfigParser import RawConfigParser
from lib import storage, ajax, template
from bowerbird.configobj import ConfigObj
from lib.odict import OrderedDict
from lib.sonogram import generateSonogram
from genshi.filters import HTMLFormFiller

FREQUENCY_SCALES = ['Linear',  'Logarithmic']
DEFAULT_FFT_STEP = 256 # in milliseconds
STATION_SECTION_NAME = 'station_information'
STATION_NAME_KEY = 'name'

sonogram_directory = os.path.join("static", "sonograms")

class Root(object):
	
	def __init__(self, db):
		self.db = db
		self.config_obj = ConfigObj(cherrypy.config['binary_config'])
		self.config_filename = None
		self.config_timestamp = 0
		self.station_name = ""
		self.update_station_name()
	
	def update_station_name(self):
		if self.config_obj and self.config_obj.has_key(STATION_SECTION_NAME) \
				and self.config_obj[STATION_SECTION_NAME].has_key(STATION_NAME_KEY):
			self.station_name = self.config_obj[STATION_SECTION_NAME][STATION_NAME_KEY]

	@cherrypy.expose
	@template.output('index.html')
	def index(self, **ignored):
		return template.render(station=self.station_name)	
		
	@cherrypy.expose
	@template.output('config.html')
	def config(self, load_defaults=False, cancel=False, apply=False,  **data):
		if cancel:
			raise cherrypy.HTTPRedirect('/')
		elif apply:
			# update config if we have one loaded
			if self.config_obj:
				for key in data:
					section, option = key.split('.')
					self.config_obj[section][option] = data[key]
				# update file (we have to specify the file object otherwise the defaults file may be written over
				with open(cherrypy.config['binary_config'],  'w') as save_file:
					self.config_obj.write(save_file)
				self.update_station_name()
			raise cherrypy.HTTPRedirect('/')

		config_desc, config_keys, config_desc_errors = self.parseBinaryConfigDesc()
		if load_defaults:
			config_values, config_errors = self.parseBinaryConfig(config_keys,  \
					cherrypy.config['binary_config_defaults'])
		else:
			config_values, config_errors = self.parseBinaryConfig(config_keys, \
					cherrypy.config['binary_config'])
		return template.render(station=self.station_name, \
				desc_file=cherrypy.config['binary_config_defaults'], \
				desc=config_desc, desc_errors=config_desc_errors, \
				file=cherrypy.config['binary_config'], \
				errors=config_errors) | HTMLFormFiller(data=config_values)

	def parseBinaryConfig(self, valid_keys, config_filename):
		if self.config_obj:
			# if already loaded and file hasn't changed (name or timestamp) then use cached value
			if self.config_filename != config_filename or self.config_timestamp < os.stat(config_filename).st_mtime:
				new_config_obj = ConfigObj(config_filename)
				for section in new_config_obj:
					for option in new_config_obj[section]:
						self.config_obj[section][option] = new_config_obj[section][option]
				self.update_station_name()
		else:
			self.config_obj = ConfigObj(config_filename)
			self.update_station_name()
		self.config_filename = config_filename
		self.config_timestamp = os.stat(config_filename).st_mtime
		values = {}
		errors = []
		for section in self.config_obj:
			for option in self.config_obj[section]:
				option_id = '.'.join((section, option))
				if option_id in valid_keys:
					values[option_id] = self.config_obj[section][option]
				else:
					# warn if config has keys not in the description
					errors.append('Unknown key "%s" (value: "%s")' % (option_id, self.config_obj[section][option]))
		return values, errors
	
	def parseBinaryConfigDesc(self):
		# TODO if already loaded and file hasn't changed (name or timestamp) then use cached value
		config = ConfigObj(cherrypy.config['binary_config_desc'], list_values=False)
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

	@cherrypy.expose
	@template.output('categories.html')
	def categories(self, sort='label', sort_order='asc', **ignored):
		return template.render(station=self.station_name, \
				categories=self.db.getCategories(sort,  sort_order), 
				sort=sort, sort_order=sort_order)

	@cherrypy.expose
	@template.output('category.html')
	def category(self, label=None, new_label=None, update_details=None,  
			sort='date_and_time', sort_order='asc', **ignored):
		if update_details and new_label and new_label != label:
			self.db.updateCategory(label,  new_label)
			raise cherrypy.HTTPRedirect('/category/%s' % new_label)
		
		calls = self.db.getCalls(sort,  sort_order,  label)
		call_sonograms = {}
		for call in calls:
			call_sonograms[call['filename']] = self.getSonogram(call, FREQUENCY_SCALES[0], DEFAULT_FFT_STEP)
		return template.render(station=self.station_name, \
				category=self.db.getCategory(label),
				calls=calls, call_sonograms=call_sonograms, sort=sort, sort_order=sort_order)

	@cherrypy.expose
	@template.output('calls.html')
	def calls(self, sort='date_and_time', sort_order='asc', category=None, **ignored):
		return template.render(station=self.station_name, \
				calls=self.db.getCalls(sort,  sort_order,  category), 
				sort=sort, sort_order=sort_order)

	@cherrypy.expose
	@template.output('call.html')
	def call(self, call_filename=None, 
			update_details=None, label=None, category=None, example=None, 
			update_sonogram=None,  frequency_scale=FREQUENCY_SCALES[0],
			fft_step=DEFAULT_FFT_STEP, 
			**ignored):
		if not call_filename:
			raise cherrypy.NotFound()
		
		fft_step = int(fft_step)
		
		if call_filename and update_details:
			self.db.updateCall(call_filename, label, category, example)
			if ajax.is_xhr():
				return # ajax update doesn't require a response
			
		call=self.db.getCall(call_filename)
		if ajax.is_xhr():
			# only sonogram updates are possible here
			assert(update_sonogram)
			return template.render('_sonogram.html', ajah=True, 
					sonogram_filename=self.getSonogram(call, frequency_scale, fft_step),
					fft_step=fft_step, frequency_scale=frequency_scale,
					frequency_scales=FREQUENCY_SCALES)
		else:
			return template.render(station=self.station_name, call=call,  
					categories=",".join(self.db.getCategoryNames()),
					sonogram_filename=self.getSonogram(call, frequency_scale, fft_step),
					fft_step=fft_step, frequency_scale=frequency_scale,
					frequency_scales=FREQUENCY_SCALES,
					prev_next_files=self.db.getPrevAndNextCalls(call))

	def getSonogram(self, call, frequency_scale, fft_step):
		if call:
			destination_dir = os.path.abspath(sonogram_directory)
			return '/media/sonograms/%s' % generateSonogram(call['filename'], destination_dir, frequency_scale, fft_step)
		return ''


def main(args):
	# load config from file
	cherrypy.config.update('bowerbird.conf')
	# set static directories to be relative to script
	cherrypy.config.update({
			'tools.staticdir.root':  os.path.abspath(os.path.dirname(__file__)), 
			})
	
	# initialise storage
	db = storage.Storage(cherrypy.config['database'])
	
	cherrypy.engine.subscribe('stop_thread',  db.stop)
	
	cherrypy.quickstart(Root(db), '/', {
		'/media': {
			'tools.staticdir.on': True,
			'tools.staticdir.dir': 'static'
		}
	})


if __name__ == '__main__':
	main(sys.argv)
