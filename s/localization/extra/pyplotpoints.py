#!/usr/bin/env python

from pylab import *

f = open('/tmp/pyplot.points')
str = f.readline()
lst = eval(str)
f.close()

xdata = [x for (x,y) in lst]
ydata = [y for (x,y) in lst]

plot(xdata,ydata,'o')
show()

#plot(lst,'o')
#axis('scaled')
#show()
