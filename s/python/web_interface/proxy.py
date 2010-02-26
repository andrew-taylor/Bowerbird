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
ROOT_DIR_KEY = 'root_dir'
REQUIRED_KEYS = [DATABASE_KEY, ROOT_DIR_KEY]

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
	def __init__(self, path, database_file, root_dir, bowerbird_port):
		self.path = path
		self.bowerbird_port = bowerbird_port

		# initialise storage
		self.db = ProxyStorage(database_file, root_dir)

		# create the zeroconf scanner to detect local bowerbirds
		self.scanner = ZeroconfScanner(common.ZEROCONF_TYPE,
				ZEROCONF_SCAN_TIME_MS)


	@cherrypy.expose
	@template.output('connect.html')
	def connect(self, address=None, remove=None, rescan=False, **ignored):
		# if already connected, then just bounce to the status page
		if cherrypy.session.has_key(SESSION_STATION_ADDRESS_KEY):
			raise cherrypy.HTTPRedirect('/status')

		# ignore address if "rescan" was requested
		if not rescan and address:
			# if "Remove" button has been clicked, delete entry from history
			if remove:
				self.db.removeConnection(address)
			else:
				# get the name from the address
				try:
					name = self.getRemoteName(address)
				except urllib2.URLError:
					return self.showConnectionFailure(
							error="could not be established", address=address)

				self.db.addConnection(name, address)
				cherrypy.session[SESSION_STATION_ADDRESS_KEY] = address
				cherrypy.session[SESSION_STATION_NAME_KEY] = name
				raise cherrypy.HTTPRedirect('/status')

		return template.render(is_proxy=True, is_connected=False,
				previous_connections=self.db.getPreviousConnections(),
				local_connections=self.findLocalBowerbirds(rescan))


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
		return self.tunnelConnection("status", kwargs)


	@cherrypy.expose
	def config(self, **kwargs):
		# TODO catch station name changes and update connection history
		return self.tunnelConnection("config", kwargs)


	@cherrypy.expose
	def schedule(self, **kwargs):
		return self.tunnelConnection("schedule", kwargs)


	@cherrypy.expose
	@template.output('recordings.html')
	def recordings(self, **ignored):
		return UNIMPLEMENTED


	def findLocalBowerbirds(self, rescan=False):
		if rescan:
			self.scanner.rescan()

		bowerbirds = []
		for service in self.scanner:
			if (service.has_key('magic') and service['magic']
					== common.ZEROCONF_TEXT_TO_IDENTIFY_BOWERBIRD):
				name = service['name']
				# strip the wrapper from the name
				name = name[name.find('[')+1 : name.rfind(']')]
				bowerbirds.append({'name': name, 'address': service['address']})

		return bowerbirds


	def getRemoteName(self, address):
		return urllib2.urlopen('http://%s:%s/name' %
				(address, self.bowerbird_port)).read()


	def assertConnected(self):
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
	def showConnectionFailure(self, error='was lost', name=None, address=None):
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


	def tunnelConnection(self, page, kwargs):
		'''tunnel connection back from remote Bowerbird system'''

		self.assertConnected()

		try:
			return self.update_html(urllib2.urlopen('http://%s:%s/%s' %
					(cherrypy.session[SESSION_STATION_ADDRESS_KEY],
						self.bowerbird_port, page), urlencode(kwargs)))
		except urllib2.URLError:
			return self.showConnectionFailure()


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

	if cherrypy.config.has_key(BOWERBIRD_PORT_KEY):
		bowerbird_port = cherrypy.config[BOWERBIRD_PORT_KEY]
	else:
		bowerbird_port = DEFAULT_BOWERBIRD_PORT

	path = os.path.dirname(os.path.realpath(args[0]))
	database_file = cherrypy.config[DATABASE_KEY]
	root_dir = cherrypy.config[ROOT_DIR_KEY]

	try:
		cherrypy.tree.mount(Root(path, database_file, root_dir, bowerbird_port),
			'/', { '/media': {
				'tools.staticdir.on': True,
				'tools.staticdir.dir': 'static'
			}})
	except IOError as e:
		sys.stderr.write('Error: %s.\n' % e);
		sys.exit(1)

	cherrypy.engine.start()
	cherrypy.engine.block()


if __name__ == '__main__':
	main(sys.argv)
