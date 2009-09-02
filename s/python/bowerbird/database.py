#!/usr/bin/env python
import sys, sqlite3, re, array, numpy


SOURCE_DB_COLUMN_NAMES = [('source_id', 'INTEGER PRIMARY KEY'), ('name', 'TEXT'), ('sampling_rate', 'DOUBLE'), ('fft_n_bins', 'INTEGER'), ('fft_step_size', 'INTEGER'), ('fft_window_size', 'INTEGER')]
UNIT_DB_COLUMNS = [('unit_id', 'INTEGER PRIMARY KEY'), ('source_id', 'INTEGER'), ('channel', 'INTEGER'), ('first_frame', 'INTEGER'), ('n_frames', 'INTEGER'), ('frequency', 'BLOB'), ('amplitude', 'BLOB'), ('phase', 'BLOB')]

UNIT_ATTRIBUTE_FIELDS = ['unit_id', 'length', 'frequency_min', 'frequency_max', 'frequency_mean', 'amplitude_min', 'amplitude_max', 'amplitude_mean']
CREATE_ATTRIBUTE_TABLE = 'create table if not exists unit_attributes (unit_id INTEGER PRIMARY KEY, length INTEGER, frequency_min INTEGER, frequency_max INTEGER, frequency_mean INTEGER, amplitude_min INTEGER, amplitude_max INTEGER, amplitude_mean)'
ATTRIBUTE_TABLE_NAME = 'unit_attributes'


class Source:
    def __init__(self, db_columns):
        for ((attribute,sql_type),value) in zip(SOURCE_DB_COLUMN_NAMES, db_columns):
            setattr(self, attribute, value)
        if not 'genus' in self.__dict__:
            p = re.findall(r'/([A-Z][a-z]+)_([a-z]{2,})/', self.name)
            if (len(p) == 1):
                [(self.genus,self.species)] = p
        self.__dict__.setdefault('genus', '?')
        self.__dict__.setdefault('species', '?')
    def scientific_name(self):
        return self.genus + '_' + self.species;
    def short_scientific_name(self):
        return re.sub(r'^(.)[a-z]*', r'\1', self.scientific_name())
    def to_db_columns(self):
        return [getattr(self, field) for field in SOURCE_DB_COLUMN_NAMES]
    def insert_db(self, db, table):
        columns = self.to_db_columns()
        db.execute(('INSERT INTO %s values(NULL' + len(columns) * ',?' +')') % table, columns)
    def __str__(self):
        return self.name
    def __repr__ (self):
        return 'Source("%s")>' % self.name
        

from scikits.samplerate import resample
from scipy.fftpack import rfft

class Unit:
    def __init__(self, db_columns, source=None, sources_hash=None):
        for ((attribute,sql_type),value) in zip(UNIT_DB_COLUMNS, db_columns):
            if sql_type == 'BLOB':
                setattr(self, attribute, numpy.fromstring(value))
            else:
                setattr(self, attribute, value)
        if sources_hash:
            self.source = sources_hash[self.source_id]
        else:
            self.source = source
        self.calculate_attributes()
    def calculate_attributes(self):
        source = self.source
        freq = self.frequency
        sampling_rate = float(source.sampling_rate)
        fft_sampling_rate = sampling_rate/float(source.fft_step_size)
        window_length = float(source.fft_window_size)/sampling_rate
        # FIXME real time should be passed in as an extra field
        self.start = window_length
        if self.first_frame > 0:
            self.start += (self.first_frame - 1)/fft_sampling_rate
        self.length = window_length
        if len(freq) > 1:
#            if 'length' not in self.__dict__:
#                print self.__dict__
#            print self.__dict__['length']
            self.length += (len(freq) - 1)/fft_sampling_rate
#        print self.length
        self.end = self.start + self.length
        for k in ('frequency','amplitude'):
            a = getattr(self,k)
            setattr(self, k+'_min', min(a))
            setattr(self, k+'_max', max(a))
            setattr(self, k+'_mean', sum(a)/len(a))
        freq_window = 2 # seconds
        freq_fft_size = 128
        resampled_freq = resample(freq, freq_window*freq_fft_size/fft_sampling_rate, 'sinc_fastest') # FIXME truncate array?
        self.freq_fft = abs(rfft(resampled_freq,n=freq_fft_size,overwrite_x=True))[1:]
    def to_attribute_db_columns(self):
        return [getattr(self,field) for field in UNIT_ATTRIBUTE_FIELDS]
    def insert_attribute_db(self, db, table):
        columns = self.to_attribute_db_columns()
        db.execute(insert_unit_attributes_db_command(table), columns)
    def __str__(self):
        return "%s[%d]" % (self.source.name, self.unit_id)
    def __repr__ (self):
        return 'Unit("%s[%d]")>' %  (self.source.name, self.unit_id)

def read_db_sources(db, sources_hash=None):
    c = db.execute('SELECT * FROM sources')
    row = c.fetchone()
    sources = []
    while row:
        source = Source(row)
        if sources_hash is not None: sources_hash[source.source_id] = source
        sources.append(source)
        row = c.fetchone()
    return sources
    
def read_db_units(db, units, source=None, sources_hash=None, max_units_read=None):
    if (source):
        cursor = db.execute('SELECT * FROM units where source_id = %s' % source.source_id)
    else:
        cursor = db.execute('SELECT * FROM units')
    row = cursor.fetchone()
    row_count=0
    while row:
        units.append(Unit(row,source=source,sources_hash=sources_hash))
        row_count += 1
        if (max_units_read is not None and row_count >= max_units_read): break
        row = cursor.fetchone()

def read_db(db_name=None, db=None, units=None, max_units_read=None, sources_hash = {}):
    if not db:
       db = sqlite3.connect(db_name)
    sources = read_db_sources(db, sources_hash=sources_hash)
    if units is not None:
        read_db_units(db, units, sources_hash=sources_hash, max_units_read=max_units_read)
    return sources 

def dump_db(db):
    c = db.execute('SELECT * FROM units')
    row = c.fetchone()
    units = []
    while row:
        print 'unit',
        for ((attribute,sql_type),value) in zip(UNIT_DB_COLUMNS, row):
            if sql_type != 'BLOB': print "%s=%s" % (attribute,value),
#            for i in range(UNIT_BLOBS_START_COLUMN,len(row)):
#                print "%s=%s" % (UNIT_FIELDS[i], blob_to_array(row[i])),
        print
        units.append(row)
        row = c.fetchone()

def blob_to_array(blob):
    a = array.array('d')
    a.fromstring(blob)
    return a
    

if __name__ == "__main__":
    for db_name in sys.argv[1:]:
         units = []
         sources = read_db(db_name,units)
#        db.execute(CREATE_ATTRIBUTE_TABLE)
#        Unit.update_db_unit_attributes(db, ATTRIBUTE_TABLE_NAME)

##
#http://www.shokhirev.com/nikolai/abc/sql/sql.htmlselect count(*) from sources;
#select count(DISTINCT source) from sources;
#select * from sources where id not in (select id from sources group by source having  max(id));
#select id,source from units where source not in (select id from sources group by source having  max(id));
#
## delete  units for duplicated sources
#delete from units  where source not in (select max(id) from sources group by source);
## delete duplicated sources
#delete from sources  where id not in (select max(id) from sources group by source);
## units with no source entry
#select id,source from units where source not in (select id from sources);
## count of units from each source
#select count(source), source from units group by source;
##  count of units from each source including name
#select count(units.source),sources.source from units inner join sources on sources.id = units.source group by units.source;
#BEGIN TRANSACTION;
#CREATE TEMPORARY TABLE s as select * from units;
#drop table units;
#create table units (unit_id INTEGER PRIMARY KEY, source_id INTEGER,  channel INTEGER, first_frame INTEGER, n_frames INTEGER, frequency BLOB, amplitude BLOB, phase BLOB);
#INSERT INTO units select id as unit_id,source as source_id,channel,first_frame,n_frames,frequency,amplitude,phase  from s;
#drop table s;
#commit;
