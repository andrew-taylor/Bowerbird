#!/usr/bin/env python

# parse the config file and output it in a format readable for shell scripts

from optparse import OptionParser

from bowerbird.common import convertConfig, DEFAULT_SHELL_SEPARATOR
from bowerbird.configobj import ConfigObj


# default location of configuration file
DEFAULT_CONFIG_FILE = '../bowerbird_config'
DEFAULT_OUTPUT_FILE = "/dev/stdout"


if __name__ == '__main__':
	# parse commandline options
	parser = OptionParser()
	parser.add_option("-c", "--config", dest="config_file",
			help="configuration file to convert", default=DEFAULT_CONFIG_FILE)
	parser.add_option("-o", "--output", dest="output_file",
			help="file to write shell variables",
			default=DEFAULT_OUTPUT_FILE)
	parser.add_option("-s", "--shell-separator", dest="shell_separator",
			help="separator to use in output", default=DEFAULT_SHELL_SEPARATOR)
	(options, args) = parser.parse_args()

	# open configuration file
	config = ConfigObj(options.config_file)
	# open output file
	output = file(options.output_file, "w")

	# write converted config out
	convertConfig(config, output, options.shell_separator)

	# close file handles
	output.close()
	#unsupported: config.close()
