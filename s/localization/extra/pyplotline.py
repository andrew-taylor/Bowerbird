#!/usr/bin/env python

from pylab import *

fx = open('/tmp/pyplot.linex')
fy = open('/tmp/pyplot.liney')
strx = fx.readline()
stry = fy.readline()
lx = eval(strx)
ly = eval(stry)
fx.close()
fy.close()

plot(lx,ly,'o')
#axis('scaled')
show()
