#include "i.h"

char *result_file;


double minwithindex(double *array, int N, index_t *index) {
	*index = 0;
	double best = array[0];
	for (index_t i=1; i<N; i++)
	{
		if (array[i] < best)
		{
			best = array[i];
			*index = i;
		}
	}
	return best;
}
double maxwithindex(double *array, int N, index_t *index)
{
	*index = 0;
	double best = array[0];
	for (index_t i=1; i<N; i++)
	{
		if (array[i] > best)
		{
			best = array[i];
			*index = i;
		}
	}
	return best;
}

double max(double *array, int N)
{
	double best = array[0];
	for (index_t i=1; i<N; i++)
	{
		if (array[i] > best)
		{
			best = array[i];
		}
	}
	return best;

}
double min(double *array, int N)
{
	double best = array[0];
	for (index_t i=1; i<N; i++)
	{
		if (array[i] < best)
		{
			best = array[i];
		}
	}
	return best;

}
double median(double *array, int N)
{
	return 0.5*(array[N/2] + array[(N-1)/2]);
}


int cmp_double(const void *p1, const void *p2)
{
	double d1 = *((double *)p1);
	double d2 = *((double *)p2);

	if (d1 == d2)
		return 0;
	return (d1 > d2) ? 1:-1;
}


/* very lame way of getting the number of digits */

int num_digits(int x)
{
	x = sgn(x)*x;
	char str[1024];
	sprintf(str,"%d",x);
	return strlen(str);
}

/*int top_digits(int x,int topnum)
{
	char str[1024];
	sprintf(str,"%d",x);
	int numdigits = strlen(str);
	for (int i=topnum; i<numdigits; i++)
	{
		str[i] = '0';
	}
	int result;
	sscanf(str,"%d",&result);
	return result;
}*/

/* returns the number obtained by turning the `numtodrop'
 * least significant digits of x to zero 
 * e.g. zero_least_sig_digits(12345,2) gives 12300 */
 
int zero_leastsig_digits(int x,int numtodrop)
{
	int sign=sgn(x);
	x = sign*x;
	char str[1024];
	sprintf(str,"%d",x);
	int numdigits = strlen(str);
	int topnum = numdigits-numtodrop;
	for (int i=topnum; i<numdigits; i++)
	{
		str[i] = '0';
	}
	int result;
	sscanf(str,"%d",&result);
	return sign*result;
}

/* converts a time string into our time format 
 * (which is just unix epoch time).  they can specify the time
 * as realworld time (e.g. 07:16:23.111) or as epoch time.
 * with epoch time they don't need to give the full epoch though,
 * they can just give some of the last digits.
 *
 * these inputs are turned into the correct epoch time by using
 * the base date which is obtained after we've scanned the data
 * dirs.  so this shouldn't be called until base_date and
 * base_epoch have been setup.
 */
 
double parse_time(char *timestr)
{
	assert(base_date);
	int hour, minute;
	double second;
	if (sscanf(timestr,"%d:%d:%lf",&hour,&minute,&second)==3)
	{
		// time was specified in realworld time
		struct tm tm;
		memcpy(&tm,base_date,sizeof(struct tm));
		tm.tm_sec = floor(second);
		tm.tm_min = minute;
		tm.tm_hour = hour;
		time_t epoch_time = mktime(&tm);
		return epoch_time+(second-floor(second));
	}
	else
	{
		// time was specified in epoch time
		second = atof(timestr);
		// it may have only specified some last digits though
		int numdigits = num_digits(floor(second));
		// however, it could have some forward zeros that need
		// to be accounted for
		numdigits += strspn(timestr,"0");
		double fulltime = zero_leastsig_digits(base_epoch,numdigits) + second;	
		return fulltime;
	}
}	

/* prints a date string to fp for our time epoch_timestamp */
void fprint_date(FILE *fp,double epoch_timestamp)
{
	double fraction = epoch_timestamp - floor(epoch_timestamp);
	time_t epoch = floor(epoch_timestamp);

	struct tm* thistime = localtime(&epoch);
	char date_str[128];
	strftime(date_str,128,"%F %H:%M",thistime);
	if (fraction == 0)
	{
		fprintf(fp,"%s:%2d",date_str,thistime->tm_sec);
	}
	else if (thistime->tm_sec > 9)
	{
		fprintf(fp,"%s:%g",date_str,thistime->tm_sec+fraction);
	}
	else
	{
		fprintf(fp,"%s:0%g",date_str,thistime->tm_sec+fraction);
	}
}

/* format the latitude and longitude position nicely */
void fprint_location(FILE *fp, earthpos_t *location)
{
	char lat_char = location->lat_deg >= 0 ? 'N' : 'S';
	char lng_char = location->lng_deg >= 0 ? 'E' : 'W';
	fprintf(fp,"%c%g %.5lf %c%g %.5lf",
			lat_char,fabs(location->lat_deg),fabs(location->lat_min),
			lng_char,fabs(location->lng_deg),fabs(location->lng_min));
}

int geochar_sgn(char c)
{
	if (c == 'N')
		return 1;
	else if (c == 'S')
		return -1;
	else if (c == 'E')
		return 1;
	else if (c == 'W')
		return -1;

	assert(0);
}


void set_earthpos(earthpos_t *pos,double lat_deg,double lat_min,double lng_deg,double lng_min)
{
	pos->lat_deg = lat_deg;
	pos->lat_min = fabs(lat_min);
	pos->lng_deg = lng_deg;
	pos->lng_min = fabs(lng_min);
}

double deg2rad(double degree,double minutes)
{
	degree = degree + sgn(degree)*minutes/60.0;
	return PI*degree/180.0;
}

double deg2full(double deg,double min)
{
	return deg+sgn(deg)*min/60.0;
}

FILE *fp_result=NULL;
int result_count=0;
void write_result(double time,double length,earthpos_t *location,double uncertainty)
{
	if (!fp_result)
	{
		fp_result = fopen(result_file,"w");
		if (!fp_result)
		{
			printf("Failed to open result file %s\n",result_file);
			return;
		}
	}
	fprintf(fp_result,"Result %d:\n",result_count);
	fprintf(fp_result,"            Time: ");
	fprint_date(fp_result,time);
	fputc('\n',fp_result);
	fprintf(fp_result,"          Length: %g seconds\n",length);
	fprintf(fp_result,"        Location: ");
	fprint_location(fp_result,location);
	fputc('\n',fp_result);
	fprintf(fp_result,"     Uncertainty: %lf\n",uncertainty);

	result_count++;
}

// __attribute__ ((const))
double sgn(double x) 
{
	if (x>=0)
		return 1.0;
	else
		return -1.0;
}


//__attribute__ ((const)) 
double
square_d(double x) {
        return x * x;
}

