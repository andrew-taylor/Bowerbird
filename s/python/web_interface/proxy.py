import cherrypy, os, sys, shutil, errno, calendar
import urllib2, lxml.html, lxml.etree
from cherrypy.lib import sessions
from lib import common, storage, ajax, template
from lib.configparser import ConfigParser
from lib.storage import ProxyStorage
from lib.zeroconfscanner import ZeroconfScanner
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

SESSION_STATION_NAME_KEY = 'station_name'
SESSION_STATION_ADDRESS_KEY = 'station_address'
CONNECT_FAILURE_REDIRECTION_TIMEOUT = 10 # in seconds

# how long zeroconf will wait for responses when probing for bowerbirds
ZEROCONF_SCAN_TIME_MS = 500

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

		self.scanner = ZeroconfScanner(common.ZEROCONF_TYPE, 
				ZEROCONF_SCAN_TIME_MS)


	@cherrypy.expose
	@template.output('connect.html')
	def connect(self, address=None, remove=None, **ignored):
		# if already connected, then just bounce to the status page
		if cherrypy.session.has_key(SESSION_STATION_ADDRESS_KEY):
			raise cherrypy.HTTPRedirect('/status')

		if address:
			# if "Remove" button has been clicked, delete entry from history
			if remove:
				self.db.removeConnection(address)
			else:
				# get the name from the address
				try:
					name = self.get_remote_name(address)
				except urllib2.URLError:
					return self.show_connection_failure(
							error="could not be established", address=address)

				self.db.addConnection(name, address)
				cherrypy.session[SESSION_STATION_ADDRESS_KEY] = address
				cherrypy.session[SESSION_STATION_NAME_KEY] = name
				raise cherrypy.HTTPRedirect('/status')

		return template.render(is_proxy=True, is_connected=False,
				previous_connections=self.db.getPreviousConnections(),
				local_connections=self.findLocalBowerbirds())


	@cherrypy.expose
	def disconnect(self, **ignored):
		# set disconnected
		if cherrypy.session.has_key(SESSION_STATION_ADDRESS_KEY):
			del cherrypy.session[SESSION_STATION_ADDRESS_KEY]
		raise cherrypy.HTTPRedirect('/')


	@cherrypy.expose
	def index(self, **ignored):
		# just redirect to status page if connected, otherwise connect page
		if cherrypy.session.has_key(SESSION_STATION_ADDRESS_KEY):
			raise cherrypy.HTTPRedirect('/status')
		else:
			raise cherrypy.HTTPRedirect('/connect')


	@cherrypy.expose
	def status(self, **kwargs):
		return self.tunnel_connection("status", kwargs)


	@cherrypy.expose
	def config(self, **kwargs):
		# TODO catch station name changes and update connection history
		return self.tunnel_connection("config", kwargs)


	@cherrypy.expose
	def schedule(self, **kwargs):
		return self.tunnel_connection("schedule", kwargs)


	@cherrypy.expose
	@template.output('recordings.html')
	def recordings(self, **ignored):
		return UNIMPLEMENTED


	def findLocalBowerbirds(self):
		bowerbirds = []
		for service in self.scanner:
			if (service['text'] == common.ZEROCONF_TEXT_TO_IDENTIFY_BOWERBIRD):
				name = service['name']
				# strip the wrapper from the name
				name = name[name.find('[')+1 : name.rfind(']')]
				bowerbirds.append({'name': name, 'address': service['address']})

		return bowerbirds

	
	def get_remote_name(self, address):
		return urllib2.urlopen('http://%s:%s/name' %
				(address, self.bowerbird_port)).read()


	def assert_connected(self):
		'''Asserts that a connection to a Bowerbird system exists.
		   Redirects to connect page if not connected'''
		if not cherrypy.session.has_key(SESSION_STATION_ADDRESS_KEY):
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
	def show_connection_failure(self, error='was lost', name=None, address=None):
		if address:
			station_address = address
		elif cherrypy.session.has_key(SESSION_STATION_ADDRESS_KEY):
			station_address = cherrypy.session[SESSION_STATION_ADDRESS_KEY]
			del cherrypy.session[SESSION_STATION_ADDRESS_KEY]
		else:
			station_address = ""

		if name:
			station_name = name
		elif cherrypy.session.has_key(SESSION_STATION_NAME_KEY):
			station_name = cherrypy.session[SESSION_STATION_NAME_KEY]
			del cherrypy.session[SESSION_STATION_NAME_KEY]
		else:
			station_name = ""

		return template.render(is_proxy=True, is_connected=False, error=error,
				station_name=station_name, station_address=station_address,
				timeout=CONNECT_FAILURE_REDIRECTION_TIMEOUT)


	def tunnel_connection(self, page, kwargs):
		'''tunnel connection back from remote Bowerbird system'''

		self.assert_connected()

		try:
			return self.update_html(urllib2.urlopen('http://%s:%s/%s' %
					(cherrypy.session[SESSION_STATION_ADDRESS_KEY], 
						self.bowerbird_port, page), urlencode(kwargs)))
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

	# initialise storage
	database_file = cherrypy.config[DATABASE_KEY]
	if not os.path.exists(database_file):
		sys.stderr.write('Warning: configured database file '
				'"%s" does not exist. Creating a new one...\n'
				% database_file)
	db = ProxyStorage(database_file)
	# make sure that database has key tables
	if not db.hasRequiredTables():
		sys.stderr.write('Warning: configured database file '
				'"%s" missing required tables. Creating them...\n'
				% database_file)
		db.createRequiredTables()

	cherrypy.engine.subscribe('stop_thread', db.stop)

	path = os.path.dirname(os.path.realpath(args[0]))

	try:
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
