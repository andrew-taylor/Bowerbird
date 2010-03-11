import os, sys, time, string
from copy import deepcopy

from bowerbird.common import SECTION_META_KEY, convertConfig
from bowerbird.configobj import ConfigObj
from bowerbird.odict import OrderedDict


COMMENTS_LINE_DELIMITER = '___'

# scheduled capture section is handled differently
SCHEDULE_SECTION = 'scheduled_capture'
SCHEDULE_COMMENTS = '''
# This section stores the times for scheduled captures.
# It will always be moved back to the top, so don't bother moving it.'''
CONFIG_HEADER = '''# configuration file for bowerbird deployment

# Some formatting of comments is required to provide nice looking edit
# pages. General explanatory comments are welcome, but the last comment
# line just before an item should be as described in the next
# paragraph.

# Comment just before section should be a display version of the section name
# Comment just before key-value pairs should be 4 comma separated values:
# 1) display name for value (this should be the for value groups such as a
#         min-max range pair)
# 2) display subname for values that are part of a group
# 3) variable type (float, string, int, time, etc) - Currently ignored
# 4) units for value (for display)

# leave this section header here to workaround
# a strangeness in comment parsing'''

# keys for cache
K_OBJ = 'config_obj'
K_TIME = 'timestamp'
K_VAL = 'values'
K_DICT = 'dict'
K_SCHED = 'sched'

# clumsy way of doing an enum for cache value indices
I_NAME = 0
I_SUBNAME = I_NAME + 1
I_KEY = I_SUBNAME + 1
I_VAL = I_KEY + 1
I_TYPE = I_VAL + 1
I_UNITS = I_TYPE + 1


class ConfigParser(object):

    def __init__(self, config_filename=None, config_defaults_filename=None):
        self.filename = config_filename
        self.cache = {}
        if config_filename:
            self.__updateCacheFromFile(self.filename, self.cache)
        else:
            self.__initCache(self.cache)

        self.defaults_filename = config_defaults_filename
        self.defaults_cache = {}
        if config_defaults_filename:
            self.__updateCacheFromFile(self.defaults_filename,
                    self.defaults_cache)
        else:
            self.__initCache(self.defaults_cache)

    # cache is a dictionary
    def __initCache(self, cache):
        cache[K_OBJ] = ConfigObj()
        cache[K_TIME] = 0
        cache[K_VAL] = OrderedDict()
        cache[K_DICT] = OrderedDict()

    def getRawValues(self):
        return deepcopy(self.cache[K_DICT])

    def getValues(self):
        # update if file has been modified
        self.__updateCacheFromFile(self.filename, self.cache)
        # return a copy
        return deepcopy(self.cache[K_VAL])

    def getDefaultValues(self):
        # update if file has been modified
        if self.defaults_filename:
            self.__updateCacheFromFile(self.defaults_filename,
                    self.defaults_cache)
            # return a copy
            return deepcopy(self.defaults_cache[K_VAL])
        return None

    def getValue1(self, key):
        # update if file has been modified
        self.__updateCacheFromFile(self.filename, self.cache)

        # use try/catch because most of the time key will be there
        try:
            return self.cache[K_DICT][key][I_VAL]
        except KeyError:
            return None

    def getValue2(self, section, key):
        return self.getValue1('.'.join((section, key)))

    def setValue1(self, key, value):
        # update config obj
        section, s_key = key.split('.')
        if not self.cache[K_OBJ].has_key(section):
            self.cache[K_OBJ][section] = {}
        self.cache[K_OBJ][section][s_key] = value
        # update cache(s)
        self.__updateCacheFromConfigObj(self.cache)

    def setValue2(self, section, key, value):
        self.setValue1('.'.join(section, key))

    def setMeta1(self, key, meta):
        # update config obj
        section, s_key = key.split('.')
        if not self.cache[K_OBJ].has_key(section):
            self.cache[K_OBJ][section] = {}
        if not self.cache[K_OBJ][section].has_key(s_key):
            self.cache[K_OBJ][section][s_key] = ''
        meta_bits = meta.split(',')
        (name, subname, type, units) = map(string.strip, meta_bits[:4])
        description = ','.join(meta_bits[4:]).strip()

        # don't write the description lines if they're empty
        # (otherwise, it inserts a blank line)
        if description:
            self.cache[K_OBJ][section].comments[s_key] = description.split(
                    COMMENTS_LINE_DELIMITER)

        # don't write the meta-information line if it's the defaults
        if name != s_key or subname != '' or type != 'string' or units != '':
            self.cache[K_OBJ][section].comments[s_key].append(
                    "# %s, %s, %s, %s" % (name, subname, type, units))
        # update cache(s)
        self.__updateCacheFromConfigObj(self.cache)

    def setMeta2(self, section, key, value):
        self.setMeta1('.'.join(section, key))

    def setSectionMeta(self, section, meta):
        # update config obj
        if not self.cache[K_OBJ].has_key(section):
            self.cache[K_OBJ][section] = {}
        index = meta.find(',')
        name = meta[:index]
        description = meta[index+1:].strip()

        # if the section comments don't start with a blank line then add one
        # (to separate it from the next section)
        if not description.startswith(COMMENTS_LINE_DELIMITER):
            self.cache[K_OBJ].comments[section] = ['']

        # don't write the description lines if they're empty
        # (otherwise, it inserts a blank line)
        if description:
            self.cache[K_OBJ].comments[section].extend(description.split(
                    COMMENTS_LINE_DELIMITER))

        # don't write the meta-information line if it's the defaults
        if name != section:
            self.cache[K_OBJ].comments[section].append("# " + name)
        # update cache(s)
        self.__updateCacheFromConfigObj(self.cache)

    def clearConfig(self):
        '''delete all config entries from cache (not including schedule)'''
        # clear all values from both cache and raw cache
        self.cache[K_VAL].clear();
        self.cache[K_DICT].clear();

        # clear the config obj
        self.cache[K_OBJ].clear();
        self.cache[K_OBJ].initial_comment = CONFIG_HEADER.split('\n')
        self.cache[K_OBJ]['dummy'] = {}

        # update timestamp
        self.cache[K_TIME] = time.time();

    def getTimestamp(self):
        if self.filename and os.path.exists(self.filename):
            return int(os.path.getmtime(self.filename))
        return 0

    def saveToFile(self):
        try:
            # update file
            with open(self.filename, 'w') as save_file:
                self.cache[K_OBJ].write(save_file)
        except:
            # if this fails we should re-read the file to keep them synced
            self.cache[K_OBJ].reload()
            self.cache[K_TIME] = self.getTimestamp();
            raise


    def export(self, export_filename):
        with open(export_filename, 'w') as save_file:
            self.cache[K_OBJ].write(save_file)


    def exportForShell(self, export_filename):
        with open(export_filename, 'w') as save_file:
            convertConfig(self.cache[K_OBJ], save_file)


    def __updateCacheFromFile(self, filename, cache):
        if cache and cache.has_key(K_OBJ):
            # if already loaded and file hasn't changed then use cached value
            # if config_obj exists, timestamp should too
            obj_timestamp = self.getTimestamp()
            if cache[K_TIME] < obj_timestamp:
                cache[K_OBJ].reload()
                cache[K_TIME] = obj_timestamp
        else:
            cache[K_OBJ] = ConfigObj(filename, list_values=False,
                    write_empty_values=True)
            cache[K_TIME] = self.getTimestamp()

        self.__updateCacheFromConfigObj(cache)

    def __updateCacheFromConfigObj(self, cache):
        cache[K_VAL] = OrderedDict()
        cache[K_DICT] = OrderedDict()
        for section in cache[K_OBJ]:
            section_dict = OrderedDict()
            section_comments = cache[K_OBJ].comments[section]
            # get last line of comment for section
            if section_comments and section_comments[-1:][0]:
                # remove '#' and spare whitespace
                section_name = section_comments[-1:][0][1:].strip()
                section_description = COMMENTS_LINE_DELIMITER.join(
                        section_comments[:-1])
                # add an entry with the section metadata (conforming to the
                # shape of the other entries)
                section_dict[SECTION_META_KEY] = [(section_name, '',
                        section, '', '', '', section_description)]

            for key in cache[K_OBJ][section]:
                option_id = '.'.join((section, key))
                key_comment = cache[K_OBJ][section].comments[key][-1:]
                name = None
                if key_comment and key_comment[0]:
                    # remove '#' then split and strip the spec comment
                    specs = [s.strip() for s in
                            key_comment[0][1:].split(',')]
                    if len(specs) == 4:
                        (name, subname, input_type, units) = specs
                        description = COMMENTS_LINE_DELIMITER.join(
                                cache[K_OBJ][section].comments[key][:-1])
                if name == None:
                    # no (or invalid) spec in last comment so use defaults
                    name = key
                    subname = ''
                    input_type = 'string'
                    units = ''
                    description = COMMENTS_LINE_DELIMITER.join(
                            cache[K_OBJ][section].comments[key])
                if not section_dict.has_key(name):
                    section_dict[name] = []
                values = (name, subname, option_id,
                        cache[K_OBJ][section][key], input_type, units,
                        description)
                # store here for display purposes
                section_dict[name].append(values)
                # store here for easy access
                cache[K_DICT][option_id] = values
            if section_dict:
                cache[K_VAL][section] = section_dict

    def parseFile(self, file):
        '''
        Parse the new file and return the values, but don't cache them.
        The argument to this method can be anything that ConfigObj can take as an
        infile parameter to its constructor. This includes filenames, lists of
        strings, a dictionary, or anything with a read method.
        '''
        cache = {}
        cache[K_OBJ] = ConfigObj(file, list_values=False, write_empty_values=True)
        self.__updateCacheFromConfigObj(cache)

        return cache[K_VAL]
