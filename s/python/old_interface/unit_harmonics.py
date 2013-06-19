#!/usr/bin/env python
import sys, sets, sqlite3
import bowerbird.database as database
from bowerbird.intervals.intersection import IntervalTree

class a:
    def __init__(self,x,y):
        self.start=x
        self.end=y
        
def intersect_intervals(i1, i2)
if __name__ == "__main__":
    for db_name in sys.argv[1:]:
        db = sqlite3.connect(db_name)
        sources = database.read_db_sources(db)
        for source in sources:
#            print source
            units = []
            database.read_db_units(db, units, source)
            intersecter = IntervalTree()
            for u in units:
                intersecter.insert_interval(u)
            printed = sets.Set()
            for u in units:
                if u in printed: continue
                overlapping = intersecter.find(u.start, u.end)
                if len(overlapping) == 1: continue
                assert(overlapping)
                print source, [(o.start,o.end) for o in overlapping]
                for o in overlapping: printed.add(o)
            