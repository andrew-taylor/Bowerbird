import dbus, gobject, avahi
from copy import deepcopy
from dbus import DBusException
from dbus.mainloop.glib import DBusGMainLoop
from threading import Thread

__all__ = ["ZeroconfScanner"]

DEFAULT_TYPE = '_http._tcp'
DEFAULT_SCAN_TIME_MS = 1000

class ZeroconfScanner(object):
	'''synchronous wrapper to asynchronous dbus interface of avahi'''

	def __init__(self, type=DEFAULT_TYPE, scan_time_ms=DEFAULT_SCAN_TIME_MS):
		self.type = type
		self.scan_time_ms = scan_time_ms

		self.__services = []

		self.bus = dbus.SystemBus(mainloop=DBusGMainLoop())
		self.server = dbus.Interface(self.bus.get_object(avahi.DBUS_NAME, '/'),
				'org.freedesktop.Avahi.Server')

		self.gobject_loop = None

		self.rescan()

	def __iter__(self):
		for service in self.__services:
			yield service

	def services(self):
		return deepcopy(__services)

	def abort_scan(self):
		self.__quit_main_loop()

	def __quit_main_loop(self):
		if self.gobject_loop and self.gobject_loop.is_running:
			self.gobject_loop.quit()
			self.gobject_loop = None
		return False

	def rescan(self):
		# ensure previous loop is not running
		self.__quit_main_loop()

		self.__services = []
		sbrowser = dbus.Interface(self.bus.get_object(avahi.DBUS_NAME,
				self.server.ServiceBrowserNew(avahi.IF_UNSPEC,
				avahi.PROTO_UNSPEC, self.type, 'local',
				dbus.UInt32(0))), avahi.DBUS_INTERFACE_SERVICE_BROWSER)
		sbrowser.connect_to_signal("ItemNew", self.__add_server_to_list)

		gobject.timeout_add(self.scan_time_ms, self.__quit_main_loop)

		self.gobject_loop = gobject.MainLoop()
		self.gobject_loop.run()

	def __add_server_to_list(self, interface, protocol, name, stype, domain, 
			flags):
		self.server.ResolveService(interface, protocol, name, stype, 
			domain, avahi.PROTO_UNSPEC, dbus.UInt32(0), 
			reply_handler=self.__service_resolved, 
			error_handler=self.__print_error)

	def __service_resolved(self, *args):
		new_service = { 
				'name': unicode(args[2]), 
				'address': unicode(args[7]),
				'port':	int(args[8]) }
		for entry in avahi.txt_array_to_string_array(args[9]):
			equals_pos = entry.find('=')
			key = entry[:equals_pos]
			data = entry[equals_pos+1:]
			# ignore duplicate keys
			if not new_service.has_key(key):
				new_service[key] = data
			else:
				print "duplicate entry in text array: ", entry

		# prevent duplicates (until we figure out how to avoid this)
		if self.__services.count(new_service):
			print "duplicate service found:", new_service
		else:
			self.__services.append(new_service)

	def __print_error(self, *args):
		print 'error from zeroconf:', args


def test():
	scanner = ZeroconfScanner(type='_http._tcp')
	for service in scanner:
		print service

if __name__ == '__main__':
	test()
