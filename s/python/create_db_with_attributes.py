#!/usr/bin/python
import sys

from bowerbird import Database, UnitAttributes
from bowerbird.database_interface import find_databases
#from bowerbird.unit import form_unit_sets_between_channels, mark_sequences

if __name__ == "__main__":
    new_db = Database(sys.argv[1])
    unwritten_units = 0
    unwritten_sources = []
    for pathname in find_databases(sys.argv[2:]):
        db = Database(pathname)
        sources = db.read_sources()
        for source in sources:
            ul =  db.read_units(source)
            for u in ul:
                u.attributes = UnitAttributes(unit=u)
            n_units_read = len(ul)
            unwritten_units += n_units_read
            print '%d units read from %s'% (n_units_read, source)
#            us = form_unit_sets_between_channels(ul)
#            sequences = mark_sequences(us)
        unwritten_sources += sources
        # buffer up database writes
        if unwritten_units > 10000:
            new_db.dump_attributes(unwritten_sources)
            unwritten_units = 0
            unwritten_sources = []
    if unwritten_sources:
        new_db.dump_attributes(unwritten_sources)
    new_db.close()
