/* these are geometry routines for dealing with the (not-so-)spherical earth.
 * we also have the code here for pinpointing a location from TDOAs. */
#include "i.h"

#define pow2	square_d

#define WGS84__EQUATORIAL_RADIUS   6378137.0
#define WGS84__POLAR_RADIUS        6356752.31424518


/* note that the length of a degree of latitude or longitude depends on what
 * latitude you are at.  the relevant equations can be found in the
 * wikipedia article on latitude:
 *          http://en.wikipedia.org/wiki/Latitude
 * This article also has information on the WGS84 ellipsoid, which is the
 * approximation of the earth used by (all?) GPS devices. */
double degree_of_latitude(double lat_rad)
{
	double a = WGS84__EQUATORIAL_RADIUS;
	double b = WGS84__POLAR_RADIUS;
	double numer = pow2(a*b);
	double denom = sqrt(pow(pow2(a*cos(lat_rad))+pow2(b*sin(lat_rad)),3));
	return (numer/denom)*(PI/180);
}

double degree_of_longitude(double lat_rad)
{
	double a = WGS84__EQUATORIAL_RADIUS;
	double b = WGS84__POLAR_RADIUS;
	double numer = pow2(a);
	double denom = sqrt(pow2(a*cos(lat_rad))+pow2(b*sin(lat_rad)));
	return (numer/denom)*cos(lat_rad)*(PI/180);
}

double normsquared2(point2d_t point)
{
	return (point.x*point.x + point.y*point.y);
}

double norm2(point2d_t point)
{
	return sqrt(point.x*point.x + point.y*point.y);
}

double dist2(point2d_t *p1, point2d_t *p2)
{
	return sqrt((p1->x-p2->x)*(p1->x-p2->x) + (p1->y-p2->y)*(p1->y-p2->y));
}

/* assumes earth is the WGS-84 ellipsoid.
 * This iterative method for calculating distance is much more accurate than usual
 * methods. It is Vincenty's algorithm:
 *
 * 	T. Vincenty - Direct and Inverse Solutions of Geodesics on the Ellipsoid
 * 	              with Applications of Nested Equations, 
 * 	              Survey Review, vol. 23, no. 176
 * 	              April 1975, pp. 88-93
 *
 * This code is based on matlab code by Peter Cederholm.
 *
 * One of the advantages of this code is that it is accurate for very small
 * distances.  On the other hand it is ill-conditioned for points which are 
 * nearly antipodal. (You need to be careful with some of the common methods
 * for computing distance, which are very bad at small distances).
 *
 * The resulting distance is measured in metres.
 */         
double distance_from_latlng_radians(double lat1,double lng1, double lat2, double lng2)
{
	assert(fabs(lat1) <= 90 && fabs(lat2) <= 90);

	double a = 6378137;
	double b = 6356752.31424518;

	// correct for errors at exact poles by adjusting 0.6 mmm
	if (fabs(PI/2 - fabs(lat1))< 1e-10)
	{
		lat1 = sgn(lat1)*(PI/2-(1e-10));
	}
	if (fabs(PI/2 - fabs(lat2))< 1e-10)
	{
		lat2 = sgn(lat2)*(PI/2-(1e-10));
	}

	double f = (a-b)/a;
	double U1 = atan( (1-f)*tan(lat1));
	double U2 = atan( (1-f)*tan(lat2));
	//
	double L = fabs(lng2 - lng1);
	if (L > PI)
	{
		L = 2*PI - L;
	}
	double lambda = L;
	double lambdaold = 0;
	int itercount = 0;
	double sigma=0, alpha=0, cos2sigmam=0, C=0;
	while (!itercount || fabs(lambda-lambdaold)>1e-12)
	{
		itercount++;
		if (itercount > 50)
		{
			lambda = PI;
			break;
		}
		lambdaold = lambda;
		double sinsigma;
		sinsigma = sqrt(pow2(cos(U2)*sin(lambda))+
			   pow2(cos(U1)*sin(U2) - 
			    sin(U1)*cos(U2)*cos(lambda)));
		double cossigma;
		cossigma = sin(U1)*sin(U2)+cos(U1)*cos(U2)*cos(lambda);
		sigma = atan2(sinsigma,cossigma);
		alpha = asin(cos(U1)*cos(U2)*sin(lambda)/sin(sigma));
		cos2sigmam = cos(sigma)-2*sin(U1)*sin(U2)/pow2(cos(alpha));
		C = f/16*pow2(cos(alpha))*(4+f*(4-3*pow2(cos(alpha))));
		lambda = L+(1-C)*f*sin(alpha)*(sigma+C*sin(sigma)*(cos2sigmam+C*cos(sigma)*(-1+2*pow2(cos2sigmam))));
		if (lambda > PI)
		{
			lambda = PI;
			break;
		}
	}
	double u2 = pow2(cos(alpha))*(a*a-b*b)/(b*b);
	double A = 1+u2/16384*(4096+u2*(-768+u2*(320-175*u2)));
	double B = u2/1024*(256+u2*(-128+u2*(74-47*u2)));
	double deltasigma = B*sin(sigma)*(cos2sigmam+B/4*(cos(sigma)*(-1+2*pow2(cos2sigmam))-B/6*cos2sigmam*(-3+4*pow2(sin(sigma)))*(-3+4*pow2(cos2sigmam))));
	double s = b*A*(sigma-deltasigma);
	return s;
}

/* compute the distance between two points on earth (in metres) */
double earth_distance(earthpos_t *pos1, earthpos_t *pos2)
{
	double latrad1 = deg2rad(pos1->lat_deg,pos1->lat_min);
	double lngrad1 = deg2rad(pos1->lng_deg,pos1->lng_min);
	double latrad2 = deg2rad(pos2->lat_deg,pos2->lat_min);
	double lngrad2 = deg2rad(pos2->lng_deg,pos2->lng_min);
	return distance_from_latlng_radians(latrad1,lngrad1,latrad2,lngrad2);
}

/* as a part of our localization algorithm we convert the GPS coordinates of the
 * three stations to 2D coordinates in a plane.  distances in this plane should
 * very closely match distances on the curved earth.
 *
 * our method for conversion is simple.  we pick one point on the earth to
 * become the origin of the plane.  any other point on the earth is converted
 * to a point on the plane relative to that point.  more specifically, given
 * some other point on the earth, to find its point on the plane we just need
 * to find it's (x,y) coordinates.  We take x to be the distance that the point is 
 * to the east of the origin point and we take y to be the distance that the point 
 * north of the origin point. Thus, we compute these geodesic distances east/west
 * and north/south and use them to find our (x,y) coordinates.
 *
 * This process can clearly be inverted.
 * However, in order to go backwards we need to know the lengths of a degree of 
 * longitude or latitude through a given point on the earth (which depends on the
 * point's latitude).
 *
 * The conversions are done by
 *      earth_to_relative_2d_position
 * and  relative_2d_to_earth_position
 *
 */


/* given that refpoint will be the origin, convert thispoint to it's 2d position */
void earth_to_relative_2d_position(point2d_t *relative_2d_position, earthpos_t *thispoint, earthpos_t *refpoint)
{
	double deltax,deltay;

	/* to get deltax we construct the point directly to the east/west of the refpoint
	 * which lies directly below/above the point we are mapping */
	earthpos_t temp;
	temp.lat_deg = refpoint->lat_deg;
	temp.lat_min = refpoint->lat_min;
	temp.lng_deg = thispoint->lng_deg;
	temp.lng_min = thispoint->lng_min;

	deltax = earth_distance(&temp,refpoint);

	double refpoint_lng  = deg2full(refpoint->lng_deg,refpoint->lng_min);
	double thispoint_lng = deg2full(thispoint->lng_deg,thispoint->lng_min);
	if (refpoint_lng > thispoint_lng)
		deltax = -1.0*deltax;
	
	/* to get deltay we construct the point directly to the north/south of the refpoint
	 * which lies directly to the east/west of the point we are mapping */
	temp.lat_deg = thispoint->lat_deg;
	temp.lat_min = thispoint->lat_min;
	temp.lng_deg = refpoint->lng_deg;
	temp.lng_min = refpoint->lng_min;

	deltay = earth_distance(&temp,refpoint);

	double refpoint_lat  = deg2full(refpoint->lat_deg,refpoint->lat_min);
	double thispoint_lat = deg2full(thispoint->lat_deg,thispoint->lat_min);
	if (refpoint_lat > thispoint_lat)
		deltay = -1.0*deltay;
		

	relative_2d_position->x = deltax;
	relative_2d_position->y = deltay;
}

/* given that ref is the point on earth corresponding to the origin of the plane,
 * convert the plane position `relative' to it's point on earth. */
void relative_2d_to_earth_position(earthpos_t *result, point2d_t *relative, earthpos_t *ref)
{
	/*
	double ref_lat_rad = deg2rad(ref->lat_deg,ref->lat_min);
	double deg;
	deg = relative->y / degree_of_latitude(ref_lat_rad);
	result->lat_deg = ref->lat_deg + sgn(deg)*floor(fabs(deg));
	result->lat_min = fabs(ref->lat_min + 60*(deg - result->lat_deg+ref->lat_deg));


	deg = relative->x / degree_of_longitude(ref_lat_rad);
	result->lng_deg = ref->lng_deg + sgn(deg)*floor(fabs(deg));
	result->lng_min = fabs(ref->lng_min + 60*(deg - result->lng_deg+ref->lng_deg));
	*/
	double ref_lat_rad = deg2rad(ref->lat_deg,ref->lat_min);
	

	double refdeg, deg, resdeg;
        refdeg = deg2full(ref->lat_deg,ref->lat_min);
	deg    = relative->y / degree_of_latitude(ref_lat_rad);
	resdeg = refdeg + deg;

	result->lat_deg = sgn(resdeg)*floor(fabs(resdeg));
	result->lat_min = 60*fabs(resdeg - result->lat_deg);


	refdeg = deg2full(ref->lng_deg,ref->lng_min);
	deg = relative->x / degree_of_longitude(ref_lat_rad);
	resdeg = refdeg + deg;

	result->lng_deg = sgn(resdeg)*floor(fabs(resdeg));
	result->lng_min = 60*fabs(resdeg - result->lng_deg);
}


/* given the TDOAs for each pair of stations, and the locations of the stations on the earth,
 * we need to estimate a point which could have produced those TDOAs.  Finding such a point
 * is really a question of analytic geometry. Note however, that since the TDOAs will not be
 * correct (they are just estimates) they may describe a physically impossible situation.
 * That is, it is likely that there is _no_ point on earth which would produce the given TDOAs.
 * We are after an estimate.  We can measure how good a point is simply by computing the 
 * distance from it to each of the stations which gives the TDOAs that would be produced from
 * that point.  We can then compare these TDOAs with the given TDOAs.
 *
 * If we cannot find a point whose TDOAs are close to the ones given, then it is good evidence
 * that the given TDOAs are inconsistent, which indicates that they have been estimated 
 * incorrectly.
 *
 * There are several ways that the geometric problem can be solved.  Note that it is really
 * the question of finding a point in the plane such that the differences of its distances
 * from three given points are prescribed values.
 *
 * Recall that given two points p1 and p2 the locus of points such that
 *               | x - p1 | - | x - p2 | = c
 * forms a hyperbola.  (The hyperbola has two sheets, depending on the order of p1 and p2
 * in the above equation, or equivalently, depending on the sign taken for c).   
 * Thus the (signed) TDOA between stations p1 and p2 specifies that the point we are after
 * lies on a specific sheet of the hyperbola
 *               | x - p1 | - | x - p2 | = TDOA*speedofsound
 *
 * Thus, since we have three TDOAs, we are looking for a point in the intersection of three
 * hyperbolas.  From this view, our discussion about the inconsistency of the TDOAs means
 * that this intersection could be empty or even contain multiple points. Most likely, it
 * will be empty.  So one approach to the solution is to search for the point that is 
 * closest to the three hyperbolas, say in a least squares sense.
 *
 * An easier approach is just to search along the hyperbolas looking for the point that
 * gives the nearest TDOAs.
 *
 *
 * There is also a completely different (in some sense dual) approach to the geometry.
 * Recall that a conic section is uniquely determined by its major axis, and three points
 * that it intersects.  That is, given three points, and a line, there is a unique conic
 * section (ellipse or hyperbola) which intersects the points and has the line as its
 * major axis.
 *
 * It turns out that we can define a certain line (in terms of given TDOAs) such that
 * the point which produces those TDOAs lies on this line.  Moreoever if we construct
 * the conic section with this line as major axis, which intersects the three stations,
 * we can prove using some analytic geometry that the point which produces those TDOAs
 * is one of the two focii of the conic section.
 *
 * Thus we can derive a close-form solution.  Unfortunately, it only works when the 
 * TDOAs are consistent.  That is, _given_ that the TDOAs all come from some point, 
 * we can prove that that point is one of the focii of the aforementioned conic
 * section.  However, if the TDOAs are inconsistent, then these focii need not
 * produce the TDOAs.
 *
 * Our approach will actually be to search both along the hyperbolas and along the
 * major axis of this conic section, optimizing the obvious fitness function.
 */

void locate_point(double TDOA_01_in_seconds,double TDOA_12_in_seconds,double TDOA_20_in_seconds,earthpos_t station_earthpos[],earthpos_t *location,double heuristics[])
{
	/* first we need to convert the station positions from points on earth to
	 * points on a 2D grid, as described above.*/
	point2d_t station_planepos[NUM_STATIONS];
	station_planepos[0].x = 0;  /* we will take station 0 to be the origin of the plane, */
	station_planepos[0].y = 0;
	/* and convert the other two stations to points relative to station 0 */
	earth_to_relative_2d_position(&station_planepos[1],&station_earthpos[1],&station_earthpos[0]);
	earth_to_relative_2d_position(&station_planepos[2],&station_earthpos[2],&station_earthpos[0]);
	dprintf(6,"Station 1 is at (%lf,%lf) relative to station 0\n",station_planepos[1].x,station_planepos[1].y);
	dprintf(6,"Station 2 is at (%lf,%lf) relative to station 0\n",station_planepos[2].x,station_planepos[2].y);
	dprintf(6,"Centre is at (%lf,%lf) relative to station 0\n",(station_planepos[1].x+station_planepos[2].x)/3.0,(station_planepos[1].y+station_planepos[2].y)/3.0);


	point2d_t location_in_plane;
	locate_point_in_plane(TDOA_01_in_seconds,TDOA_12_in_seconds,TDOA_20_in_seconds,station_planepos,&location_in_plane,heuristics);


	relative_2d_to_earth_position(location,&location_in_plane,&station_earthpos[0]);
	dprintf(5,"Best point in plane at (%lf,%lf) coordinates (%.10lf, %.10lf)\n",location_in_plane.x,location_in_plane.y,deg2full(location->lng_deg,location->lng_min),deg2full(location->lat_deg,location->lat_min));

//	earthpos_t current3d;
//	relative_2d_to_earth_position(&current3d,&current2d,&station_earthpos[0]);
	//printf("Error in space = %lf\n",earth_distance(&current3d,location));
	//printf("Dist between current3d and current = %lf\n",earth_distance(&current3d,current));

//heuristics[LOCATION_CONSISTENCY_HEUR] = badness;
	heuristics[LOCATION_LATDEG_HEUR] = location->lat_deg;
	heuristics[LOCATION_LATMIN_HEUR] = location->lat_min;
	heuristics[LOCATION_LNGDEG_HEUR] = location->lng_deg;
	heuristics[LOCATION_LNGMIN_HEUR] = location->lng_min;
}
void rotate2(point2d_t *p,double slope,point2d_t *origin)
{
	double d = sqrt(1+slope*slope);
	double newx = origin->x + (p->x - origin->x - slope*(p->y-origin->y))/d;
	double newy = origin->y + (slope*(p->x - origin->x) + p->y - origin->y)/d;
	p->x = newx;
	p->y = newy;
}
void flip(point2d_t *p)
{
	double temp = p->x;
	p->x = p->y;
	p->y = temp;
}
double fitnessfunc(point2d_t *p,double del01,double del12,double del20,point2d_t *stations)
{
	return fabs(del01 - dist2(p,&stations[0]) + dist2(p,&stations[1])) + fabs(del12 - dist2(p,&stations[1]) + dist2(p,&stations[2]))  + abs(del20 - dist2(p,&stations[2]) + dist2(p,&stations[0]));
}

void locate_point_in_plane(double TDOA_01,double TDOA_12,double TDOA_20,point2d_t stations[],point2d_t *location,double *heuristics)
{
	/* these are the (signed) differences in distance between the point we're after and the pairs of
	 * stations. */
	double del01, del12, del20;
	del01 = TDOA_01 * SPEED_OF_SOUND;
	del12 = TDOA_12 * SPEED_OF_SOUND;
	del20 = TDOA_20 * SPEED_OF_SOUND;

	double origdel01 = del01;
	double origdel12 = del12;
	double origdel20 = del20;
	double average = (del01+del12+del20)/3.0;
	del01 -= average;
	del12 -= average;
	del20 -= average;

	dprintf(5,"TDOA sum = %g\n",3*average);


	/*=====================================================
	 * first we compute the major axis of the conic section
	 *=====================================================*/
	double station0_nsqr, station1_nsqr, station2_nsqr;
	double A,B,C;

	station0_nsqr = normsquared2(stations[0]);
	station1_nsqr = normsquared2(stations[1]);
	station2_nsqr = normsquared2(stations[2]);

	A     = stations[0].x * del12 +
		stations[1].x * del20 +
		stations[2].x * del01;

	B     = stations[0].y * del12 +
		stations[1].y * del20 +
		stations[2].y * del01;

	C     = (del01*del12*del20   +
		 station0_nsqr*del12 +
		 station1_nsqr*del20 +
		 station2_nsqr*del01  )/2.0;

	/* The line is Ax + By = C */
	dprintf(10,"The line is %lf x + %lf y = %lf\n",A,B,C);

	/*======================================================================
	 * the equations are easier to deal with if the line of major axis
	 * is just one of the coordinate axes.  thus what we do is we
	 * work as if we have rotated everything to one of the coordinate axes
	 * and work there and then we will rotate back
	 *=====================================================================*/

	int flipped=0;
	/* if the slope M of the line is between [-1,1] then we rotate to the
	 * horizontal axis, otherwise we rotate to the vertical axis. Since
	 *    M=-A/B this is as follows */
	if ((-fabs(B) > -A) || (-A > fabs(B)))
	{
		// we should really just do a reflection and at the end reflect back
		//printf("We should rotate to vert. exiting...\n");	
		//exit(1);
		//
		flipped=1;
		double temp = A;
		A=B;
		B=temp;
		dprintf(10,"flipped\n");
	}
	else
	{
		dprintf(10,"not flipped\n");
	}

	/* now. the slpoe should be in [-1,1] and i believe this will force that
	 * the y-intercept can't be too high, because one of the stations has to lie above it*/
	assert( C < 1000000*fabs(B));

	double yintercept = C/B;

	/* now we translate the line downwards so that it intersects the origin */
	/* so we consider it to be Ax + By = 0 */
	/* we also need to move down the stations and then rotate them */

	// now we rotate to x-axis

	double M;
	M = -A/B;
	
	point2d_t p0 = stations[0];
	point2d_t p1 = stations[1];
	point2d_t p2 = stations[2];
	if (flipped)
	{
		flip(&p0);
		flip(&p1);
		flip(&p2);
	}
	p0.y -= yintercept;
	p1.y -= yintercept;
	p2.y -= yintercept;
	point2d_t origin;
	origin.x = 0;
	origin.y = 0;
	rotate2(&p0,-M,&origin);
	rotate2(&p1,-M,&origin);
	rotate2(&p2,-M,&origin);

	/* now we compute the the conic section from the rotated points */

	double U, V, W;

	U = ((p2.x - p1.x)*square_d(p0.y) +
             (p0.x - p2.x)*square_d(p1.y) +
	     (p1.x - p0.x)*square_d(p2.y))   /
		     (  p2.x*square_d(p1.x)
		      - p1.x*square_d(p2.x)
		      + p1.x*square_d(p0.x)
		      - p0.x*square_d(p1.x)
		      + p0.x*square_d(p2.x)
		      - p2.x*square_d(p0.x)
		     );

	V = (square_d(p2.y) - square_d(p1.y) + U*(square_d(p2.x) - square_d(p1.x))) / (p1.x - p2.x);

	W = -square_d(p0.y) - V*p0.x - U*square_d(p0.x);

	dprintf(10,"U=%lf, V=%lf, W=%lf\n",U,V,W);

	double asqr, bsqr;

	point2d_t rotated_centre;
	rotated_centre.x = -V/(2*U);
	rotated_centre.y = 0;

	asqr = square_d(rotated_centre.x) - W/U;
	bsqr = U*asqr;

	dprintf(10,"asqr = %lf, bsqr = %lf\n",asqr,bsqr);

	double dist_to_focus;
	if (bsqr < 0)
	{
		// we have a hyperbola
		dist_to_focus = sqrt(asqr-bsqr);
		dprintf(10,"Equation of hyperbola is (x- %.2f)^2/%.2f - y^2/%.2f = 1\n",rotated_centre.x,asqr,bsqr);
	}
	else
	{
		// we have an ellipse
		dprintf(10,"Equation of ellipse is (x- %.2f)^2/%.2f + y^2/%.2f = 1\n", rotated_centre.x,asqr,bsqr);
		if (asqr >= bsqr)
		{
			dprintf(10,"Major x is true");
			dist_to_focus = sqrt(asqr-bsqr);
		}
		else
		{
			dprintf(10,"Major x is false");
			dist_to_focus = sqrt(bsqr-asqr);
		}
	}
	dprintf(10,"Dist to focus = %.10lf\n",dist_to_focus);


	point2d_t focus1;
	point2d_t focus2;

/*	if (!majorx)
	{
		focus1.x = rotated_centre.x;
		focus1.y = dist_to_focus;
		rotate2(&focus1,M,&xintercept);

		focus2.x = rotated_centre.x;
		focus2.y = -dist_to_focus;
		rotate2(&focus2,M,&xintercept);	
	}
	else*/
	{
		focus1.x = rotated_centre.x+dist_to_focus;
		focus1.y = 0;
		rotate2(&focus1,M,&origin);

		focus2.x = rotated_centre.x-dist_to_focus;
		focus2.y = 0;
		rotate2(&focus2,M,&origin);
	}

	//now translate back up
	focus1.y += yintercept;
	focus2.y += yintercept;

	if (flipped)
	{
		flip(&focus1);
		flip(&focus2);
	}

	dprintf(10,"Focus 1 at (%.10lf,%.10lf)\n",focus1.x,focus1.y);
	dprintf(10,"Focus 2 at (%.10lf,%.10lf)\n",focus2.x,focus2.y);

	
	if (graphing >= 15)
	{
		char line[1024];
		sprintf(line,"python pyplotgeometry.py %lf %lf %lf %lf %lf %lf %lf %lf %lf > /dev/null\n",stations[0].x,stations[0].y,stations[1].x,stations[1].y,stations[2].x,stations[2].y,TDOA_01,TDOA_12,TDOA_20);
		printf("line = '%s'\n",line);
		system(line);
		printf("done\n");
	}


//	double fitness1 = fitnessfunc(&focus1,del01,del12,del20,stations);
//	double fitness2 = fitnessfunc(&focus2,del01,del12,del20,stations);
	dprintf(10,"Focus 1 has fitness %lf\n",fitnessfunc(&focus1,del01,del12,del20,stations));
	dprintf(10,"Focus 2 has fitness %lf\n",fitnessfunc(&focus2,del01,del12,del20,stations));
	double origfitness1 = fitnessfunc(&focus1,origdel01,origdel12,origdel20,stations);
	double origfitness2 = fitnessfunc(&focus2,origdel01,origdel12,origdel20,stations);
	dprintf(10,"Focus 1 has orig fitness %lf\n",origfitness1);
	dprintf(10,"Focus 2 has orig fitness %lf\n",origfitness2);

	if (origfitness1 < origfitness2)
	{
		memcpy(location,&focus1,sizeof *location);
	}	
	else
	{
		memcpy(location,&focus2,sizeof *location);
	}
}

#if 0

	if (A == 0 || B==0)
	{
		printf("geometry.c: The line of goodness is horizontal or vertical, not sure what to do, bailing.\n");
		exit(1);
	}

	// M is the slope
	M = -A/B;
	point2d_t xintercept;
	xintercept.x = C/A;
	xintercept.y = 0;

	point2d_t p0 = stations[0];
	point2d_t p1 = stations[1];
	point2d_t p2 = stations[2];

	rotate2(&p0,-M,&xintercept);
	rotate2(&p1,-M,&xintercept);
	rotate2(&p2,-M,&xintercept);

	double U, V, W;

	U = ((p2.x - p1.x)*square_d(p0.y) +
             (p0.x - p2.x)*square_d(p1.y) +
	     (p1.x - p0.x)*square_d(p2.y))   /
		     (  p2.x*square_d(p1.x)
		      - p1.x*square_d(p2.x)
		      + p1.x*square_d(p0.x)
		      - p0.x*square_d(p1.x)
		      + p0.x*square_d(p2.x)
		      - p2.x*square_d(p0.x)
		     );

	V = (square_d(p2.y) - square_d(p1.y) + U*(square_d(p2.x) - square_d(p1.x))) / (p1.x - p2.x);

	W = -square_d(p0.y) - V*p0.x - U*square_d(p0.x);

	dprintf(10,"U=%lf, V=%lf, W=%lf\n",U,V,W);


	double asqr, bsqr;

	point2d_t rotated_centre;
	rotated_centre.x = -V/(2*U);
	rotated_centre.y = 0;

	asqr = square_d(rotated_centre.x) - W/U;
	bsqr = U*asqr;

	dprintf(10,"asqr = %lf, bsqr = %lf\n",asqr,bsqr);

	double dist_to_focus;
	int majorx = 1;
	int hyper = 1;
	if (bsqr < 0)
	{
		// we have a hyperbola
		dist_to_focus = sqrt(asqr-bsqr);
		dprintf(10,"Equation of hyperbola is (x- %.2f)^2/%.2f - y^2/%.2f = 1\n",rotated_centre.x,asqr,bsqr);

	}
	else
	{
		hyper = 0;
		// we have an ellipse
		dprintf(10,"Equation of ellipse is (x- %.2f)^2/%.2f + y^2/%.2f = 1\n", rotated_centre.x,asqr,bsqr);
		if (asqr >= bsqr)
		{
			dprintf(10,"Major x is true");
			dist_to_focus = sqrt(asqr-bsqr);
		}
		else
		{
			dprintf(10,"Major x is false");
			dist_to_focus = sqrt(bsqr-asqr);
			majorx=0;
		}
		

	}
	dprintf(10,"Dist to focus = %.10lf\n",dist_to_focus);


	point2d_t focus1;
	point2d_t focus2;


/*	if (!majorx)
	{
		focus1.x = rotated_centre.x;
		focus1.y = dist_to_focus;
		rotate2(&focus1,M,&xintercept);

		focus2.x = rotated_centre.x;
		focus2.y = -dist_to_focus;
		rotate2(&focus2,M,&xintercept);	
	}
	else*/
	{
		focus1.x = rotated_centre.x+dist_to_focus;
		focus1.y = 0;
		rotate2(&focus1,M,&xintercept);

		focus2.x = rotated_centre.x-dist_to_focus;
		focus2.y = 0;
		rotate2(&focus2,M,&xintercept);
	}

	dprintf(10,"Focus 1 at (%.10lf,%.10lf)\n",focus1.x,focus1.y);
	dprintf(10,"Focus 2 at (%.10lf,%.10lf)\n",focus2.x,focus2.y);

	dprintf(10,"Focus 1 has fitness %lf\n",fitnessfunc(&focus1,del01,del12,del20,stations));
	dprintf(10,"Focus 2 has fitness %lf\n",fitnessfunc(&focus2,del01,del12,del20,stations));

	if (hyper)
	{
	dprintf(10,"Hyper\n");
	}
	else if (majorx)
	{
		dprintf(10,"Ellip, Major\n");
	}
	else
	{
		dprintf(10,"Ellip, Minor\n");
	}
	dprintf(10,"A/C=%lf, B/C=%lf, U=%lf, V=%lf, W=%lf\n",A/C,B/C,U,V,W);

	searchforit(&focus1,&focus2,500,location,del01,del12,del20,stations);
	double bestval = fitnessfunc(bestpoint,del01,del12,del20,stations);

	/* now let's search the hyperbolas */
	point2d_t *hyperpoints; int num;
	hyperpoints = construct_hyperbola(&stations[1],&stations[2],del12, &num);
	for (int i=0; i<num; i++)
	{
		double val = fitnessfunc(&hyperpoints[i],del01,del12,del20,stations);
		//	printf(" %lf (%lf,%lf)\n",val,hyperpoints[i].x,hyperpoints[i].y);
		if (val < bestval)
		{
			dprintf(10,"Got a better one on hyperbola: %lf -> %lf\n",bestval,val);
			bestval = val;
			bestpoint->x = hyperpoints[i].x;
			bestpoint->y = hyperpoints[i].y;
		}
	}

	free(hyperpoints);
	hyperpoints = construct_hyperbola(&stations[2],&stations[0],del20, &num);
	for (int i=0; i<num; i++)
	{
		double val = fitnessfunc(&hyperpoints[i],del01,del12,del20,stations);
		if (val < bestval)
		{
			dprintf(10,"Got a better one on hyperbola: %lf -> %lf\n",bestval,val);
			bestval = val;
			bestpoint->x = hyperpoints[i].x;
			bestpoint->y = hyperpoints[i].y;
		}
	}
	free(hyperpoints);
	hyperpoints = construct_hyperbola(&stations[0],&stations[1],del01, &num);
	for (int i=0; i<num; i++)
	{
		double val = fitnessfunc(&hyperpoints[i],del01,del12,del20,stations);
		if (val < bestval)
		{
			dprintf(10,"Got a better one on hyperbola: %lf -> %lf\n",bestval,val);
			bestval = val;
			bestpoint->x = hyperpoints[i].x;
			bestpoint->y = hyperpoints[i].y;
		}
	}
	free(hyperpoints);


	dprintf(10,"Best found by search is at (%lf,%lf) with badness of %lf\n",bestpoint->x,bestpoint->y,fitnessfunc(bestpoint,del01,del12,del20,stations));
	*badness = fitnessfunc(bestpoint,del01,del12,del20,stations);

	if (graphing >= 5)
	{
		char line[1024];
		sprintf(line,"python pyplot.py %lf %lf %lf %lf %lf %lf %lf %lf %lf > /dev/null\n",stations[0].x,stations[0].y,stations[1].x,stations[1].y,stations[2].x,stations[2].y,TDOA_01,TDOA_12,TDOA_20);
		if (system(line) != 0)
		{
			printf("Running pyplot.py failed\n");
		}
		else
		{
			dprintf(10,"Ran pyplot.py\n");
		}
	}

}
#endif

#if 0
void pinpoint_via_tdoa(double TDOA_01,double TDOA_12,double TDOA_20,point2d_t stations[],point2d_t *bestpoint)
{
	double del01, del12, del20;
	del01 = TDOA_01 * SPEED_OF_SOUND;
	del12 = TDOA_12 * SPEED_OF_SOUND;
	del20 = TDOA_20 * SPEED_OF_SOUND;

	double station0_nsqr, station1_nsqr, station2_nsqr;
	double A,B,C;
	double M;

	station0_nsqr = normsquared2(stations[0]);
	station1_nsqr = normsquared2(stations[1]);
	station2_nsqr = normsquared2(stations[2]);

	A     = stations[0].x * del12 +
		stations[1].x * del20 +
		stations[2].x * del01;

	B     = stations[0].y * del12 +
		stations[1].y * del20 +
		stations[2].y * del01;

	C     = (del01*del12*del20   +
		 station0_nsqr*del12 +
		 station1_nsqr*del20 +
		 station2_nsqr*del01  )/2.0;

	/* The line is Ax + By = C */
	dprintf(10,"The line is %lf x + %lf y = %lf\n",A,B,C);

	if (A == 0 || B==0)
	{
		printf("geometry.c: The line of goodness is horizontal or vertical, not sure what to do, bailing.\n");
		exit(1);
	}

	// M is the slope
	M = -A/B;
	point2d_t xintercept;
	xintercept.x = C/A;
	xintercept.y = 0;

	point2d_t p0 = stations[0];
	point2d_t p1 = stations[1];
	point2d_t p2 = stations[2];

	rotate2(&p0,-M,&xintercept);
	rotate2(&p1,-M,&xintercept);
	rotate2(&p2,-M,&xintercept);

	double U, V, W;

	U = ((p2.x - p1.x)*square_d(p0.y) +
             (p0.x - p2.x)*square_d(p1.y) +
	     (p1.x - p0.x)*square_d(p2.y))   /
		     (  p2.x*square_d(p1.x)
		      - p1.x*square_d(p2.x)
		      + p1.x*square_d(p0.x)
		      - p0.x*square_d(p1.x)
		      + p0.x*square_d(p2.x)
		      - p2.x*square_d(p0.x)
		     );

	V = (square_d(p2.y) - square_d(p1.y) + U*(square_d(p2.x) - square_d(p1.x))) / (p1.x - p2.x);

	W = -square_d(p0.y) - V*p0.x - U*square_d(p0.x);

	dprintf(10,"U=%lf, V=%lf, W=%lf\n",U,V,W);


	double asqr, bsqr;

	point2d_t rotated_centre;
	rotated_centre.x = -V/(2*U);
	rotated_centre.y = 0;

	asqr = square_d(rotated_centre.x) - W/U;
	bsqr = U*asqr;

	dprintf(10,"asqr = %lf, bsqr = %lf\n",asqr,bsqr);

	double dist_to_focus;
	int majorx = 1;
	int hyper = 1;
	if (bsqr < 0)
	{
		// we have a hyperbola
		dist_to_focus = sqrt(asqr-bsqr);
		dprintf(10,"Equation of hyperbola is (x- %.2f)^2/%.2f - y^2/%.2f = 1\n",rotated_centre.x,asqr,bsqr);

	}
	else
	{
		hyper = 0;
		// we have an ellipse
		dprintf(10,"Equation of ellipse is (x- %.2f)^2/%.2f + y^2/%.2f = 1\n", rotated_centre.x,asqr,bsqr);
		if (asqr >= bsqr)
		{
			dprintf(10,"Major x is true");
			dist_to_focus = sqrt(asqr-bsqr);
		}
		else
		{
			dprintf(10,"Major x is false");
			dist_to_focus = sqrt(bsqr-asqr);
			majorx=0;
		}
		

	}
	dprintf(10,"Dist to focus = %.10lf\n",dist_to_focus);


	point2d_t focus1;
	point2d_t focus2;


/*	if (!majorx)
	{
		focus1.x = rotated_centre.x;
		focus1.y = dist_to_focus;
		rotate2(&focus1,M,&xintercept);

		focus2.x = rotated_centre.x;
		focus2.y = -dist_to_focus;
		rotate2(&focus2,M,&xintercept);	
	}
	else*/
	{
		focus1.x = rotated_centre.x+dist_to_focus;
		focus1.y = 0;
		rotate2(&focus1,M,&xintercept);

		focus2.x = rotated_centre.x-dist_to_focus;
		focus2.y = 0;
		rotate2(&focus2,M,&xintercept);
	}

	dprintf(10,"Focus 1 at (%.10lf,%.10lf)\n",focus1.x,focus1.y);
	dprintf(10,"Focus 2 at (%.10lf,%.10lf)\n",focus2.x,focus2.y);

	dprintf(10,"Focus 1 has fitness %lf\n",fitnessfunc(&focus1,del01,del12,del20,stations));
	dprintf(10,"Focus 2 has fitness %lf\n",fitnessfunc(&focus2,del01,del12,del20,stations));

	if (hyper)
	{
	dprintf(10,"Hyper\n");
	}
	else if (majorx)
	{
		dprintf(10,"Ellip, Major\n");
	}
	else
	{
		dprintf(10,"Ellip, Minor\n");
	}
	dprintf(10,"A/C=%lf, B/C=%lf, U=%lf, V=%lf, W=%lf\n",A/C,B/C,U,V,W);

	searchforit(&focus1,&focus2,500,bestpoint,del01,del12,del20,stations);
	double bestval = fitnessfunc(bestpoint,del01,del12,del20,stations);

	/* now let's search the hyperbolas */
	point2d_t *hyperpoints; int num;
	hyperpoints = construct_hyperbola(&stations[1],&stations[2],del12, &num);
	for (int i=0; i<num; i++)
	{
		double val = fitnessfunc(&hyperpoints[i],del01,del12,del20,stations);
		//	printf(" %lf (%lf,%lf)\n",val,hyperpoints[i].x,hyperpoints[i].y);
		if (val < bestval)
		{
			dprintf(10,"Got a better one on hyperbola: %lf -> %lf\n",bestval,val);
			bestval = val;
			bestpoint->x = hyperpoints[i].x;
			bestpoint->y = hyperpoints[i].y;
		}
	}

	free(hyperpoints);
	hyperpoints = construct_hyperbola(&stations[2],&stations[0],del20, &num);
	for (int i=0; i<num; i++)
	{
		double val = fitnessfunc(&hyperpoints[i],del01,del12,del20,stations);
		if (val < bestval)
		{
			dprintf(10,"Got a better one on hyperbola: %lf -> %lf\n",bestval,val);
			bestval = val;
			bestpoint->x = hyperpoints[i].x;
			bestpoint->y = hyperpoints[i].y;
		}
	}
	free(hyperpoints);
	hyperpoints = construct_hyperbola(&stations[0],&stations[1],del01, &num);
	for (int i=0; i<num; i++)
	{
		double val = fitnessfunc(&hyperpoints[i],del01,del12,del20,stations);
		if (val < bestval)
		{
			dprintf(10,"Got a better one on hyperbola: %lf -> %lf\n",bestval,val);
			bestval = val;
			bestpoint->x = hyperpoints[i].x;
			bestpoint->y = hyperpoints[i].y;
		}
	}
	free(hyperpoints);


	dprintf(10,"Best found by search is at (%lf,%lf) with badness of %lf\n",bestpoint->x,bestpoint->y,fitnessfunc(bestpoint,del01,del12,del20,stations));

	if (graphing >= 5)
	{
		char line[1024];
		sprintf(line,"python pyplot.py %lf %lf %lf %lf %lf %lf %lf %lf %lf > /dev/null\n",stations[0].x,stations[0].y,stations[1].x,stations[1].y,stations[2].x,stations[2].y,TDOA_01,TDOA_12,TDOA_20);
		if (system(line) != 0)
		{
			printf("Running pyplot.py failed\n");
		}
		else
		{
			dprintf(10,"Ran pyplot.py\n");
		}
	}

}
void rotate90(point2d_t *p)
{
	double newx = -p->y;
	double newy = p->x;

	p->x = newx;
	p->y = newy;
}

void rotate270(point2d_t *p)
{
	double newx = p->y;
	double newy = -p->x;

	p->x = newx;
	p->y = newy;
}

void rotate2_(point2d_t *p,double slope)
{
	double d = sqrt(1+slope*slope);
	double newx = (p->x - slope*(p->y))/d;
	double newy = (slope*(p->x) + p->y)/d;
	p->x = newx;
	p->y = newy;
}



void searchforit(point2d_t *startpoint, point2d_t *endpoint, double granularity, point2d_t *bestpoint, double del01, double del12, double del20, point2d_t *stations)
{

	double stepsizex = (endpoint->x - startpoint->x)/granularity;
	double stepsizey = (endpoint->y - startpoint->y)/granularity;
	bestpoint->x = startpoint->x;
	bestpoint->y = startpoint->y;
	double bestval = fitnessfunc(bestpoint,del01,del12,del20,stations);
	dprintf(10,"Starting with %lf at (%lf,%lf)\n",bestval,bestpoint->x,bestpoint->y);
	point2d_t current;
	current.x = startpoint->x;
	current.y = startpoint->y;
	for (int i = 0; i<= granularity; i++)
	{
		current.x = current.x + stepsizex;
		current.y = current.y + stepsizey;

		double fitness = fitnessfunc(&current,del01,del12,del20,stations);
		if (fitness < bestval)
		{
			bestpoint->x = current.x;
			bestpoint->y = current.y;
			bestval = fitness;
			dprintf(15,"New best of %lf at (%lf,%lf)\n",bestval,bestpoint->x,bestpoint->y);
		}
	}

}

void rotate(point2d_t *point, double theta)
{
	double costheta = cos(theta);
	double sintheta = sin(theta);

	double x = costheta*(point->x) - sintheta*(point->y);
	double y = sintheta*(point->x) + costheta*(point->y);

	point->x = x;
	point->y = y;
}

void pivot_rotate2(point2d_t *point, double theta, point2d_t *pivot)
{
// point -> R(point - pivot) + pivot = Rpoint - Rpivot + pivot

	double costheta = cos(theta);
	double sintheta = sin(theta);

	double x = pivot->x + costheta*(point->x-pivot->x) - sintheta*(point->y-pivot->y);
	double y = pivot->y + sintheta*(point->x-pivot->x) + costheta*(point->y-pivot->y);

	point->x = x;
	point->y = y;
}
point2d_t *construct_hyperbola(point2d_t *focus1, point2d_t *focus2, double constant_difference, int *num_)
{
	double a = constant_difference / 2.0;
	double c = dist2(focus1,focus2) / 2.0;
	double asqr = a*a;
	double bsqr = c*c - asqr;

	if (bsqr < 0)
	{
		printf("bsqr = %lf is negative!\n",bsqr);
		*num_ = 0;
		return NULL;
	}

	double granularity = 300.0;
	double startx, stopx;
	if (constant_difference >= 0)
	{
		startx = fabs(a);
		stopx = 2*fabs(a);
	}
	else
	{
		startx = -2*fabs(a);
		stopx = -fabs(a);
	}
	double xdelta = (stopx - startx) / granularity;

	int num = floor(granularity);
	point2d_t *points = salloc(2*num*sizeof(point2d_t));
	double x = startx;
	for (int i=0; i<num; i++)
	{
		double val = sqrt(bsqr*( square_d(x/a) - 1));
		points[i].x = x;
		points[i].y = val;
		points[i+num].x =x;
		points[i+num].y = -val;

		x += xdelta;
		//printf("(%lf,%lf) and (%lf,%lf)     , ",x,val,x,-val);
	}


	double M = (focus2->y - focus1->y) / (focus2->x-focus1->x);
	num = 2*num; //BEREN
	/*
	double theta;

	if (focus2->x >= focus1->x)
	{
		if (focus2->y >= focus1->y)
		{
			theta = atan(-M);
		}
		else
		{
			theta = -atan(-M);
		}
	}	
	else
	{
		if (focus2->y >= focus1->y)
		{
			theta = PI - atan(-M);
		}
		else
		{
			theta = -(PI - atan(-M));
		}
	}*/
//	printf("Focuii: (%lf,%lf), (%lf,%lf)\n",focus1->x,focus1->y,focus2->x,focus2->y);
	if  (focus2->x < focus1->x)
	{
		if (focus2->y >= focus1->y)
		{
			M = -(focus2->x - focus1->x)/(focus2->y - focus1->y);
			for (int i=0; i<num; i++)
			{
				rotate2_(&points[i],M);
				rotate90(&points[i]);
			}
		}
		else
		{
			M = (focus2->x - focus1->x)/(focus2->y - focus1->y);
			for (int i=0; i<num; i++)
			{
				rotate2_(&points[i],-M);
				rotate270(&points[i]);
			}
		}
	}
	else
	{
		for (int i=0; i<num; i++)
		{
			rotate2_(&points[i],M);
		}
	}

	double midx = 0.5*(focus1->x + focus2->x);
	double midy = 0.5*(focus1->y + focus2->y);
	for (int i=0; i<num; i++)
	{
//		rotate(&points[i],theta);
		
		points[i].x = points[i].x + midx;
		points[i].y = points[i].y + midy;
	}	


	*num_ = num;
	return points;
}

#endif
