#!/usr/bin/env python

from optparse import OptionParser

from bowerbird.configparser import ConfigParser
from bowerbird.scheduleparser import ScheduleParser
from bowerbird.storage import BowerbirdStorage

# default location of configuration file
DEFAULT_CONFIG = '../bowerbird_config'
DEFAULT_SCHEDULE = '../bowerbird_schedule'
DEFAULT_DATABASE = 'web_interface/bowerbird.db'


def main():
	# parse commandline options
	parser = OptionParser()
	parser.add_option('-c', '--config', dest='config',
			help='configuration file', default=DEFAULT_CONFIG)
	parser.add_option('-d', '--database-file', dest='database',
			help='database file', default=DEFAULT_DATABASE)
	parser.add_option('-f', '--force-rescan', action='store_true',
			dest='force_rescan', help='force reconstruction of recordings',
			default=False)
	parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
			help='add commentary to the process', default=False)
	parser.add_option('-s', '--schedule-file', dest='schedule',
			help='schedule file', default=DEFAULT_SCHEDULE)
	(options, args) = parser.parse_args()

	# create all required objects
	config = ConfigParser(options.config)
	schedule = ScheduleParser(options.config, options.schedule)
	storage = BowerbirdStorage(options.database, config, schedule)

	# if requested, clear all recordings so they are re-constructed
	if options.force_rescan:
		storage.clearRecordings()

	# call the update method
	storage.updateRecordingsFromFiles(options.force_rescan, options.verbose)


if __name__ == '__main__':
	main()
