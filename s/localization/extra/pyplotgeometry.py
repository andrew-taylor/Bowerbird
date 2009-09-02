#!/usr/bin/env python
############################################################################
# This python program plots the geometry involved in the computation of the
# location from TDOAs.  It duplicates much of what is contained in 
#  	locate_point_in_plane(...)
# from geometry.c
# See there for more detailed information.
############################################################################
from pylab import *
from string import atof

class Point:

   def __init__(self,x,y):
        self.x=x
	self.y=y

   def plot(self,col='g'):
	   plot([self.x],[self.y],'o',color=col)

   def normsqr(self):
	   return self.x*self.x + self.y*self.y

   def rotate(self,slope,pointx=0,pointy=0):
	   (self.x,self.y) = rotate(self.x,self.y,slope,pointx,pointy)

   def translate(self,dx,dy):
	   self.x += dx
	   self.y += dy

   def flip(self):
	   (self.x,self.y) = (self.y,self.x)

def sgn(x):
	if x >= 0:
		return 1
	return -1

def samesgn(x,y):
	return (sgn(x*y) == 1)

def rotate(x,y,slope,pointx=0,pointy=0):
	m = slope
	newx = (x-pointx-m*(y-pointy))/sqrt(1+m**2)+pointx
	newy = (m*(x-pointx)+y-pointy)/sqrt(1+m**2)+pointy
	return (newx, newy)

def dist(point1,point2):
	return sqrt( (point1.x-point2.x)**2 + (point1.y-point2.y)**2 )

def distt(x1,y1,x2,y2):
	return sqrt( (x1-x2)**2 + (y1-y2)**2 )

class Hyperbola:
	def __init__(self,focus1,focus2,constant_distance,num_sheets=1):
		self.focus1 = focus1
		self.focus2 = focus2
		self.constant_distance = constant_distance
		self.num_sheets = num_sheets
		self.datas = []

		if dist(focus1,focus2) < abs(constant_distance):
			print "Locus for hyperbola is empty"
			return

		a = self.constant_distance/2.0
		c = dist(self.focus1,self.focus2)/2.0
		asqr = a**2
		bsqr = c**2 - asqr
		# note that a <= |c| is true, hence bsqr is non-negative

		dY = focus2.y - focus1.y
		dX = focus2.x - focus1.x
		if (dY < -abs(dX) or dY > abs(dX)):
			flipped = sgn(dY)
			M = dX/dY
		else:
			flipped=0
			M = dY/dX

		hyper = lambda x : sqrt(bsqr*((x/a)**2 - 1))
		granularity = 300.0
		startx = a
		stopx = 2*a
		xs1 = arange(-stopx,-startx,(stopx-startx)/granularity)
		xs2 = arange(startx,stopx,(stopx-startx)/granularity)

		datas = [zip(xs2,hyper(xs2)),zip(xs2,-hyper(xs2))]
		# add the next one for a two-sheeted hyperbola
		if (self.num_sheets == 2):
			datas = datas + [zip(xs1,hyper(xs1)),zip(xs1,-hyper(xs1))]
		self.datas = []
		for data in datas:
			#rotate them
			data = [rotate(x,y,M) for (x,y) in data]
			if flipped == 1:
				# reflect along y=x
				data = [(y,x) for (x,y) in data]
			elif flipped == -1:
				# reflect along y=-x
				data = [(-y,-x) for (x,y) in data]
			elif flipped == 0 and dX < 0:
				# rotate by 180 degrees
				data = [(-x,-y) for (x,y) in data]

			data = [(x+(focus1.x+focus2.x)/2.0,y+(focus1.y+focus2.y)/2.0) for (x,y) in data]
			self.datas.append(data)

	def points(self):
		alldata = []
		map(alldata.extend,self.datas)
		return alldata
		
	def draw(self,col='g'):
		#self.focus1.plot('b')
		#self.focus2.plot('b')
		for data in self.datas:

			[datax,datay] = zip(*data)
			plot(datax,datay,color=col)



class Line:
	def __init__(self,A,B,C):
		self.A = A
		self.B = B
		self.C = C

	def draw(self,x1,x2,col='g'):
		xs = arange(x1,x2,0.05)
		linefunc = lambda x : (self.C-self.A*x)/(1.0*self.B)
		ys = linefunc(xs)
		plot(xs,ys,color=col)


class ConicSection:
   def __init__(self,p0,p1,p2):
	u = 1.0*( (p2.x-p1.x)*(p0.y**2) + (p0.x-p2.x)*(p1.y**2) + (p1.x-p0.x)*(p2.y**2)  ) / ( (p1.x**2)*p2.x - p1.x*(p2.x**2) + p1.x*(p0.x**2) -p0.x*(p1.x**2) +p0.x*(p2.x**2) - (p0.x**2)*p2.x  )
	v = ( p2.y**2 - p1.y**2 + (p2.x**2 - p1.x**2)*u)/(p1.x-p2.x)
	w = -(p0.y**2) - v*p0.x - u*(p0.x**2)

	xc = -v/(2*u)
	asqr = xc**2 - w/u
	bsqr = u*asqr

	if (bsqr < 0):
		self.ellipse = False
		distance=sqrt(asqr-bsqr)
		bsqr = abs(bsqr)
		#print "Equation of hyperbola is (x- %.2f)^2/%.2f - y^2/%.2f = 1" % (xc,asqr,bsqr)
	else:
		self.ellipse = True
		b = sqrt(bsqr)
		distance=sqrt(abs(asqr-bsqr))
		#print "Equation of ellipse is (x- %.2f)^2/%.2f + y^2/%.2f = 1" % (xc,asqr,bsqr)

	a = sqrt(asqr)

	# now build the data points
	granularity = 300.0
	if self.ellipse:
		conicfunc = lambda x : b*sqrt(1 - ( (x-xc)/a )**2)
		xs1 = arange(xc-a,xc,1.0*a/granularity)
		xs2 = arange(xc,xc+a,1.0*a/granularity)
	else:
		conicfunc = lambda x : sqrt( bsqr*( ((x-xc)/a)**2 - 1))
		xs1 = arange(xc-2*a,xc-a-0.0001,1.0*a/granularity)
		xs2 = arange(xc+a+0.0001,xc+2*a,1.0*a/granularity)


	self.datas = [(xs1,conicfunc(xs1)),
		     (xs1,-conicfunc(xs1)),
		     (xs2,conicfunc(xs2)),
		     (xs2,-conicfunc(xs2))]
	self.focus1 = Point(xc+distance,0)
	self.focus2 = Point(xc-distance,0)


   def flip(self):
	   self.focus1.flip()
	   self.focus2.flip()
	   newdatas = []
	   for data in self.datas:
		   datax = data[0]
		   datay = data[1]
		   newdatas = newdatas+[(datay,datax)]
	   self.datas = newdatas

   def translate(self,dx,dy):
	   self.focus1.translate(dx,dy)
	   self.focus2.translate(dx,dy)
	   newdatas = []
	   for data in self.datas:
		   datax = [x+dx for x in data[0]]
		   datay = [y+dy for y in data[1]]
		   data = (datax,datay)
		   newdatas = newdatas + [(datax,datay)]
	   self.datas = newdatas

   def rotate(self,m,px=0,py=0):
	   self.focus1.rotate(m,px,py)
	   self.focus2.rotate(m,px,py)
	   newdatas = []
	   for data in self.datas:
		   datazipped = zip(data[0],data[1])
		   datax = [rotate(x,y,m,px,py)[0] for (x,y) in datazipped]
		   datay = [rotate(x,y,m,px,py)[1] for (x,y) in datazipped]
		   newdatas = newdatas + [(datax,datay)]
	   self.datas = newdatas

   def draw(self,col='g'):
	 self.focus1.plot(col)
	 self.focus2.plot(col)
	 for data in self.datas:
		plot(data[0],data[1],color=col)



###################################################################
############# BEGIN ###############################################
###################################################################

if len(sys.argv) != 10:
	sys.exit(1)

stations = []
stations = stations + [Point(atof(sys.argv[1]),atof(sys.argv[2]))]
stations = stations + [Point(atof(sys.argv[3]),atof(sys.argv[4]))]
stations = stations + [Point(atof(sys.argv[5]),atof(sys.argv[6]))]

TDOA_01 = atof(sys.argv[7])
TDOA_12 = atof(sys.argv[8])
TDOA_20 = atof(sys.argv[9])

for station in stations: station.plot('r')

SPEED_OF_SOUND = 340.29

del01 = TDOA_01 * SPEED_OF_SOUND
del12 = TDOA_12 * SPEED_OF_SOUND
del20 = TDOA_20 * SPEED_OF_SOUND

sum = del01+del12+del20
del01_normalized = del01 - sum/3.0
del12_normalized = del12 - sum/3.0
del20_normalized = del20 - sum/3.0


############### first we do the hyperbolas #####################

hyper01            = Hyperbola(stations[0],stations[1],del01)
hyper12            = Hyperbola(stations[1],stations[2],del12)
hyper20            = Hyperbola(stations[2],stations[0],del20)
hyper01.draw('r')
hyper12.draw('r')
hyper20.draw('r')

hyper01_normalized = Hyperbola(stations[0],stations[1],del01_normalized)
hyper12_normalized = Hyperbola(stations[1],stations[2],del12_normalized)
hyper20_normalized = Hyperbola(stations[2],stations[0],del20_normalized)
hyper01_normalized.draw('b')
hyper12_normalized.draw('b')
hyper20_normalized.draw('b')

# let's search for the best point on these hyperbolas

fitnessfunc = lambda p : abs(del01 - distt(p[0],p[1],stations[0].x,stations[0].y) + distt(p[0],p[1],stations[1].x,stations[1].y)) + abs(del12 - distt(p[0],p[1],stations[1].x,stations[1].y) + distt(p[0],p[1],stations[2].x,stations[2].y))  + abs(del20 - distt(p[0],p[1],stations[2].x,stations[2].y) + distt(p[0],p[1],stations[0].x,stations[0].y))

fitnessfuncc = lambda p : abs(del01 - dist(p,stations[0]) + dist(p,stations[1])) + abs(del12 - dist(p,stations[1]) + dist(p,stations[2])) + abs(del20 - dist(p,stations[2]) + dist(p,stations[0]))

points = []
points = points + hyper01_normalized.points()
points = points + hyper12_normalized.points()
points = points + hyper20_normalized.points()
fitnesses = [fitnessfunc(p) for p in points]
best_on_normalized = None
if (len(points) > 0):
	minp = points[fitnesses.index(min(fitnesses))]
	best_on_normalized = Point(minp[0],minp[1])

points = []
points = points + hyper01.points()
points = points + hyper12.points()
points = points + hyper20.points()
fitnesses = [fitnessfunc(p) for p in points]
best_on_original = None
if (len(points) > 0):
	minp = points[fitnesses.index(min(fitnesses))]
	best_on_original = Point(minp[0],minp[1])

if (best_on_original):
	print "Best point on original hypers   is (%f,%f) with fitness %f" %(best_on_original.x,best_on_original.y,fitnessfuncc(best_on_original))
	best_on_original.plot('r')
if (best_on_normalized):
	print "Best point on normalized hypers is (%f,%f) with fitness %f" %(best_on_normalized.x,best_on_normalized.y,fitnessfuncc(best_on_normalized))
	best_on_normalized.plot('b')

########### now let's do the dual conic section approach ##########

# first construct the line of axis
A = stations[0].x*del12 + stations[1].x*del20 + stations[2].x*del01
B = stations[0].y*del12 + stations[1].y*del20 + stations[2].y*del01
C = 0.5*(del01*del12*del20 +
		stations[0].normsqr()*del12 +
		stations[1].normsqr()*del20 +
		stations[2].normsqr()*del01)

line = Line(A,B,C)
#line.draw(-10,10)

flipped = 0

if ((abs(B) > -A) or (-A > abs(B))):
	flipped = 1
	temp = A
	A = B
	B = temp

if (C > 1000000*abs(B)):
	print "Unexpected!"
	print "A,B,C = %f,%f,%f"%(A,B,C)
	sys.exit(1)

yintercept = C/(1.0*B)

m = -A/(1.0*B)
p0 = Point(stations[0].x,stations[0].y)
p1 = Point(stations[1].x,stations[1].y)
p2 = Point(stations[2].x,stations[2].y)

if flipped:
	p0.flip()
	p1.flip()
	p2.flip()

p0.y -= yintercept
p1.y -= yintercept
p2.y -= yintercept

p0.rotate(-m,0,0)
p1.rotate(-m,0,0)
p2.rotate(-m,0,0)

conic = ConicSection(p0,p1,p2)
conic.rotate(m)
conic.translate(0,yintercept)
if flipped:
	conic.flip()

conic.draw('g')


focus1 = conic.focus1
focus2 = conic.focus2
best_focus = focus1
if fitnessfuncc(focus2) < fitnessfuncc(focus1): best_focus = focus2


print "Best focus                      is (%f,%f) with fitness %f" %(best_focus.x,best_focus.y,fitnessfuncc(best_focus))



axis('scaled')
title('Geometry for TDOA_01=%g, TDOA_12=%g, TDOA_20=%g'%(TDOA_01,TDOA_12,TDOA_20))
show()

