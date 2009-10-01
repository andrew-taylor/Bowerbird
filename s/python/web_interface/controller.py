import cherrypy, os, sys
from ConfigParser import RawConfigParser
from lib import storage, ajax, template
from lib.odict import OrderedDict
from lib.sonogram import generateSonogram
from lib.configparser import ConfigParser
from genshi.filters import HTMLFormFiller

WEB_CONFIG = 'bowerbird.conf'
CONFIG_KEY = 'bowerbird_config'
CONFIG_DEFAULTS_KEY = 'bowerbird_config_defaults'
DATABASE_KEY = 'database'
FREQUENCY_SCALES = ['Linear', 'Logarithmic']
DEFAULT_FFT_STEP = 256 # in milliseconds
SONOGRAM_DIRECTORY = os.path.join("static", "sonograms")
STATION_SECTION_NAME = 'station_information'
STATION_NAME_KEY = 'name'


class Root(object):

	def __init__(self, db):
		self.db = db
		if cherrypy.config.has_key(CONFIG_DEFAULTS_KEY):
			defaults = cherrypy.config[CONFIG_DEFAULTS_KEY]
		else:
			defaults = None
		self.conf = ConfigParser(cherrypy.config[CONFIG_KEY], defaults)

	def get_station_name(self):
		return self.conf.get_value2(STATION_SECTION_NAME, STATION_NAME_KEY)

	@cherrypy.expose
	@template.output('index.html')
	def index(self, **ignored):
		return template.render(station=self.get_station_name())

	@cherrypy.expose
	@template.output('config.html')
	def config(self, load_defaults=False, cancel=False, apply=False, **data):
		if cancel:
			raise cherrypy.HTTPRedirect('/')
		elif apply:
			for key in data:
				self.conf.set_value1(key, data[key])
			# update file
			self.conf.save_to_file()
			# bounce back to homepage
			raise cherrypy.HTTPRedirect('/')

		if load_defaults:
			values = self.conf.default_values()
		else:
			values = self.conf.values()
		return template.render(station=self.get_station_name(),
				using_defaults=load_defaults, values=values,
				file=self.conf.filename, 
				defaults_file=self.conf.defaults_filename)

	@cherrypy.expose
	@template.output('categories.html')
	def categories(self, sort='label', sort_order='asc', **ignored):
		return template.render(station=self.get_station_name(), \
				categories=self.db.getCategories(sort, sort_order),
				sort=sort, sort_order=sort_order)

	@cherrypy.expose
	@template.output('category.html')
	def category(self, label=None, new_label=None, update_details=None,
			sort='date_and_time', sort_order='asc', **ignored):
		if update_details and new_label and new_label != label:
			self.db.updateCategory(label, new_label)
			raise cherrypy.HTTPRedirect('/category/%s' % new_label)

		calls = self.db.getCalls(sort, sort_order, label)
		call_sonograms = {}
		for call in calls:
			call_sonograms[call['filename']] = self.getSonogram(call, FREQUENCY_SCALES[0], DEFAULT_FFT_STEP)
		return template.render(station=self.get_station_name(), \
				category=self.db.getCategory(label),
				calls=calls, call_sonograms=call_sonograms, sort=sort, sort_order=sort_order)

	@cherrypy.expose
	@template.output('calls.html')
	def calls(self, sort='date_and_time', sort_order='asc', category=None, **ignored):
		return template.render(station=self.get_station_name(), \
				calls=self.db.getCalls(sort, sort_order, category),
				sort=sort, sort_order=sort_order)

	@cherrypy.expose
	@template.output('call.html')
	def call(self, call_filename=None,
			update_details=None, label=None, category=None, example=None,
			update_sonogram=None, frequency_scale=FREQUENCY_SCALES[0],
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
			return template.render(station=self.get_station_name(), call=call,
					categories=",".join(self.db.getCategoryNames()),
					sonogram_filename=self.getSonogram(call, frequency_scale, fft_step),
					fft_step=fft_step, frequency_scale=frequency_scale,
					frequency_scales=FREQUENCY_SCALES,
					prev_next_files=self.db.getPrevAndNextCalls(call))

	def getSonogram(self, call, frequency_scale, fft_step):
		if call:
			destination_dir = os.path.abspath(SONOGRAM_DIRECTORY)
			return '/media/sonograms/%s' % generateSonogram(call['filename'], destination_dir, frequency_scale, fft_step)
		return ''


def loadWebConfig():
	# load config from file
	cherrypy.config.update(WEB_CONFIG)

	# set static directories to be relative to script
	cherrypy.config.update({
			'tools.staticdir.root': os.path.abspath(os.path.dirname(__file__)),
			})

	required_keys = [CONFIG_KEY, DATABASE_KEY]
	for key in required_keys:
		if not cherrypy.config.has_key(key):
			sys.stderr.write('Web config file "%s" is missing definition for "%s"\n'
					% (WEB_CONFIG, key))
			return False

	return True


def main(args):
	# check minimum settings are specified in config
	if not loadWebConfig():
		sys.exit(1)

	# initialise storage
	db = storage.Storage(cherrypy.config[DATABASE_KEY])

	cherrypy.engine.subscribe('stop_thread', db.stop)

	cherrypy.tree.mount(Root(db), '/', {
		'/media': {
			'tools.staticdir.on': True,
			'tools.staticdir.dir': 'static'
		}
	})
	cherrypy.engine.start()
	cherrypy.engine.block()


if __name__ == '__main__':
	main(sys.argv)
