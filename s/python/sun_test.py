#!/usr/bin/env python

# for comparison with sunrise & set times 

from math import floor
from sun import Sun

utc_offset = 10
year = 2009
latitude = -33 + 13.0/60.0
longitude = 150 + 50.0/60.0
m_days = [31,28,31,30,31,30,31,31,30,31,30,31]
months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN",
		"JUL", "AUG", "SEP", "OCT", "NOV", "DEC"]

def pp(time):
	hours = floor(time)
	minutes = floor((time - hours) * 60)
	return hours*100 + minutes

s = Sun()
for m in range(1,13):
	print "\t%s\n\trise\tset" % months[m-1]
	for d in range(1,m_days[m-1]+1):
		rise, set = [pp(t + utc_offset) for t in 
				s.sunRiseSet(year, m, d, longitude, latitude)]
		print "%2d\t%04d\t%04d" % (d, rise, set)

