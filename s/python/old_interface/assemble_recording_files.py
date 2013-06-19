#!/usr/bin/env python

from optparse import OptionParser

from bowerbird.configparser import ConfigParser
from bowerbird.scheduleparser import ScheduleParser
from bowerbird.storage import BowerbirdStorage
import sys, os.path

# default location of configuration file
BASE_DIRECTORY = '/var/lib/bowerbird'
DEFAULT_CONFIG = 'bowerbird_config'
DEFAULT_SCHEDULE = 'bowerbird_schedule'
DEFAULT_DATABASE = 'bowerbird.db'


def main():
    # parse commandline options
    parser = OptionParser()
    parser.add_option('-b', '--base_directory', dest='base_directory',
            help='base directory', default=BASE_DIRECTORY)
    parser.add_option('-c', '--config', dest='config',
            help='configuration_file')
    parser.add_option('-d', '--database-file', dest='database',
            help='database file')
    parser.add_option('-s', '--schedule-file', dest='schedule',
            help='schedule file')
    parser.add_option('-f', '--force-rescan', action='store_true',
            dest='force_rescan', help='force reconstruction of recordings',
            default=False)
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
            help='add commentary to the process', default=False)
    (options, args) = parser.parse_args()
    if not options.config: options.config = os.path.join(options.base_directory, DEFAULT_CONFIG)
    if not options.database: options.database = os.path.join(options.base_directory, DEFAULT_DATABASE)
    if not options.schedule: options.schedule = os.path.join(options.base_directory, DEFAULT_SCHEDULE)
    if not os.path.exists(options.config):
    	print >> sys.stderr, "fatal error: config file '%s' does not exist" %  options.config
    	sys.exit(1)
    config = ConfigParser(options.config)
    schedule = ScheduleParser(options.config, options.schedule)
    storage = BowerbirdStorage(options.database, config, schedule)

    # if requested, clear all recordings so they are re-constructed
    if options.force_rescan:
        storage.clearRecordingFiles()
        storage.clearRecordings()

    # call the update method
    storage.updateRecordingsFromFiles(options.force_rescan, options.verbose)


if __name__ == '__main__':
    main()
