#!/usr/bin/env python
import sys, re
from collections import defaultdict 
from numpy import array,zeros 
from Bio.Cluster import kcluster 
import database

if __name__ == "__main__":
#   test(1000)
    for db_name in sys.argv[1:]:
        sources = []
        units = []
        sources = database.read_db(db_name, units, max_units_read=1000)
        X = array([[getattr(u,x) for x in ['length','frequency_min', 'frequency_max', 'frequency_mean']] + [x for x in u.freq_fft] for u in units])
        n_clusters = 200
        clusterids, error, nfound = kcluster(X, n_clusters)
        sources_cluster = defaultdict(list)
        species_cluster = defaultdict(lambda:zeros(n_clusters, 'i'))
        total_cluster = zeros(n_clusters, 'i')
        for i in range(len(units)):
            source = units[i].source
            clusterid = clusterids[i]
            total_cluster[clusterid] += 1
            sources_cluster[source].append(clusterid)
            species_cluster[source.scientific_name()][clusterid] += 1
        for source in sources:
            print source, sources_cluster[source]
        for scientific_name in sorted(species_cluster.keys()):
            print "%-16s " % (re.sub(r'^(.)[a-z]*', r'\1', scientific_name)), " ".join(['%2d' % i for i in species_cluster[scientific_name]])
        c = []
        for clusterid in range(n_clusters):
        	l = [(species_cluster[scientific_name][clusterid]/float(total_cluster[clusterid]), scientific_name) for scientific_name in species_cluster.keys() if species_cluster[scientific_name][clusterid]]
        	l = sorted(l, reverse=True)
        	if not l: continue
        	c.append((l[0][0],l,clusterid))
        c = sorted(c, reverse=True)
        for (f,l,clusterid) in c:
            print "Cluster %3d %3d members %2d species: " % (clusterid,total_cluster[clusterid],len(l)),
            i = 0
            for (fraction,name) in sorted(l, reverse=True):
                print "%.2f %s" % (fraction, re.sub(r'^(.)[a-z]*', r'\1', name)),
            	if fraction < 0.02 and i > 3: break
            	i += 1
            print
