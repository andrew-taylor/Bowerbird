import cherrypy, os, sys, shutil, errno, calendar
import urllib2, lxml.html, lxml.etree
from cherrypy.lib import sessions
from lib import common, storage, ajax, template
from lib.configparser import ConfigParser
from genshi import HTML
from cStringIO import StringIO
from urllib import urlencode

# web interface config file
WEB_CONFIG = 'bowerbird-proxy.conf'
# parameters for parsing contents of web interface config file
BOWERBIRD_PORT_KEY = 'bowerbird_socket_port'
DEFAULT_BOWERBIRD_PORT = '8080'
DATABASE_KEY = 'database'
REQUIRED_KEYS = [DATABASE_KEY]

SESSION_STATION_KEY = 'station'

UNIMPLEMENTED = '''<html>
<head><title>Uninplemented</title></head>
<body>Page not implemented yet. Please press the back button.</body>
</html>'''


class Root(object):
	def __init__(self, db, path):
		self.db = db
		self.path = path

		if cherrypy.config.has_key(BOWERBIRD_PORT_KEY):
			self.bowerbird_port = cherrypy.config[BOWERBIRD_PORT_KEY]
		else:
			self.bowerbird_port = DEFAULT_BOWERBIRD_PORT

		# initialise database perhaps


	@cherrypy.expose
	@template.output('connect.html')
	def connect(self, history=None, local=None, manual=None, **ignored):
		# if already connected, then just bounce to the status page
		if cherrypy.session.has_key(SESSION_STATION_KEY):
			raise cherrypy.HTTPRedirect('/status')

		if manual:
			# TODO check if there's a bowerbird on that address
			cherrypy.session[SESSION_STATION_KEY] = manual
			raise cherrypy.HTTPRedirect('/status')

		return template.render(is_proxy=True, is_connected=False)


	@cherrypy.expose
	def disconnect(self, **ignored):
		# set disconnected
		del cherrypy.session[SESSION_STATION_KEY]
		raise cherrypy.HTTPRedirect('/')


	@cherrypy.expose
	def index(self, **ignored):
		# just redirect to status page if connected, otherwise connect page
		if cherrypy.session.has_key(SESSION_STATION_KEY):
			raise cherrypy.HTTPRedirect('/status')
		else:
			raise cherrypy.HTTPRedirect('/connect')


	@cherrypy.expose
	def status(self, **kwargs):
		# delete next line
		cherrypy.session[SESSION_STATION_KEY] = 'localhost' #HACK

		return self.tunnel_connection("status", kwargs)


	@cherrypy.expose
	def config(self, **kwargs):
		return self.tunnel_connection("config", kwargs)


	@cherrypy.expose
	def schedule(self, **kwargs):
		return self.tunnel_connection("schedule", kwargs)


	@cherrypy.expose
	@template.output('recordings.html')
	def recordings(self, **ignored):
		return UNIMPLEMENTED


	def assert_connected(self):
		'''Asserts that a connection to a Bowerbird system exists. 
		   Redirects to connect page if not connected'''
		if not cherrypy.session.has_key(SESSION_STATION_KEY):
			raise cherrypy.HTTPRedirect('/connect')

	
	# update the html from a remote Bowerbird to add disconnect links etc
	# NOTE: html_source should be a file-like object with a "read" method
	def update_html(self, html_source):
		# create new li
		li = lxml.html.Element('li')
		# add new hyperlink and text to the li
		lxml.etree.SubElement(li, 'a', href='/disconnect').text = "Disconnect"
		# create a dom object from the html
		dom = lxml.html.parse(html_source)
		# find the ul in the menu div
		ul = dom.find('//div[@id="menu"]/ul')
		# add the new disconnect li to the ul
		ul.insert(0, li)
		# return the modified html
		return lxml.etree.tostring(dom, method='html', pretty_print=True)

	@template.output('connection_failure.html')
	def show_connection_failure(self):
		if cherrypy.session.has_key(SESSION_STATION_KEY):
			station_address = cherrypy.session[SESSION_STATION_KEY]
			del cherrypy.session[SESSION_STATION_KEY]
		else:
			station_address = "unknown"

		return template.render(is_proxy=True, is_connected=False,
				station_address=station_address)
		

	def tunnel_connection(self, page, kwargs):
		'''tunnel connection back from remote Bowerbird system'''

		self.assert_connected()

		try:
			return self.update_html(urllib2.urlopen('http://%s:%s/%s' %
					(cherrypy.session[SESSION_STATION_KEY], self.bowerbird_port, 
						page), urlencode(kwargs)))
		except urllib2.URLError:
			return self.show_connection_failure()


def loadWebConfig():
	# load config from file
	cherrypy.config.update(WEB_CONFIG)

	# set static directories to be relative to script and enable sessions
	cherrypy.config.update({
			'tools.staticdir.root':	os.path.abspath(os.path.dirname(__file__)),
			'tools.sessions.on': True
			})

	for key in REQUIRED_KEYS:
		if not cherrypy.config.has_key(key):
			sys.stderr.write('Web config file "%s" is missing definition for "%s"\n'
					% (WEB_CONFIG, key))
			return False

	return True


def main(args):
	# check minimum settings are specified in config
	if not loadWebConfig():
		sys.exit(1)

	path = os.path.dirname(os.path.realpath(args[0]))

	try:
		# HACK
		db = {}
		cherrypy.tree.mount(Root(db, path), '/', {
			'/media': {
				'tools.staticdir.on': True,
				'tools.staticdir.dir': 'static'
			}
		})
	except IOError as e:
		sys.stderr.write('Error: %s.\n' % e);
		sys.exit(1)

	cherrypy.engine.start()
	cherrypy.engine.block()


if __name__ == '__main__':
	main(sys.argv)
