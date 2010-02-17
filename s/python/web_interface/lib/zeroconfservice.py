import avahi
import dbus

__all__ = ["ZeroconfService"]

class ZeroconfService:
	"""A simple class to publish a network service with zeroconf using
	avahi."""

	def __init__(self, name, port, stype="_http._tcp",
				 domain="", host="", **kwargs):
		self.name = name
		self.stype = stype
		self.domain = domain
		self.host = host
		self.port = port
		self.text_dict = kwargs
	
	def publish(self):
		bus = dbus.SystemBus()
		server = dbus.Interface(
						 bus.get_object(
								 avahi.DBUS_NAME,
								 avahi.DBUS_PATH_SERVER),
						avahi.DBUS_INTERFACE_SERVER)

		group = dbus.Interface(
					bus.get_object(avahi.DBUS_NAME,
								   server.EntryGroupNew()),
					avahi.DBUS_INTERFACE_ENTRY_GROUP)

		# TODO check for collisions with the service name
		text = []
		for key in self.text_dict:
			text.append("%s=%s" % (key, self.text_dict[key]))
		group.AddService(avahi.IF_UNSPEC, avahi.PROTO_UNSPEC,dbus.UInt32(0),
					 self.name, self.stype, self.domain, self.host,
					 dbus.UInt16(self.port), text)

		group.Commit()
		self.group = group
	
	def unpublish(self):
		self.group.Reset()


def test():
	service = ZeroconfService(name="TestService", port=3000, 
			test='This is test text')
	service.publish()
	raw_input("Press any key to unpublish the service ")
	service.unpublish()


if __name__ == "__main__":
	test()
