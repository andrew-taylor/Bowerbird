#!/usr/bin/env python
import sys, re
from collections import defaultdict 
from numpy import array,zeros
from Bio.Cluster import kcluster 
import db
import matplotlib.pyplot

if __name__ == "__main__":
#   test(1000)
    for db_name in sys.argv[1:]:
        sources = []
        units = []
        db.readDb(db_name, sources, units, max_units_read=10000)
        x = [u.a['length'] for u in units]
        print min(x),max(x)
        n, bins, patches = matplotlib.pyplot.hist(x,100)
        matplotlib.pyplot.show()
