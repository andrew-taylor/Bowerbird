#include "i.h"

/** 
 * @file schedule.c
 * @brief This file contains methods to do with scheduling.
 *
 * This includes checking if it's before or after a given
 * time, or a given offset from sunset/sunrise
 */

#define D2R(DEGREES)		((DEGREES) * M_PI / 180.0)
#define R2D(RADS)			((RADS) * 180.0 / M_PI)

#define T_COEFF1			0.9856
#define MEAN_OFFSET			3.289
#define SIN_MEAN_COEFF		1.916
#define SIN_2MEAN_COEFF		0.020
#define TRUE_LONG_OFFSET	282.634
#define TRUE_LONG_COEFF		0.91764
#define SIN_TRUE_LONG_COEFF	0.39782
#define ZENITH				90.5
#define T_COEFF2			0.06571
#define LOCAL_OFFSET		6.622

/** Returns the time of day in seconds (0-86399) */
int 
getTimeOfDay(void)
{
	struct tm *local = _getLocalTime();
	int time = _getSecondsSinceMidnight(local);
	free(local);
	return time;
}


/** Returns the day of the year */
int 
getDayOfYear(void)
{
	struct tm *local = _getLocalTime();
	int time = _getDayOfYear(local);
	free(local);
	return time;
}


/** Returns the sunrise time in seconds since midnight 
 * @param day_of_year The day of the year we want the sunrise time for
 * @param latitude The latitude in degrees
 * @param longitude The longitude in degrees
 * @param utc_offset The UTC offset in hours
 */
int
getSunriseTime(int day_of_year, double latitude, double longitude, double utc_offset)
{
	return _getSunTime(DAY_TIME_SUNRISE, day_of_year, latitude, longitude, utc_offset);
}


/** Returns the sunset time in seconds since midnight 
 * @param day_of_year The day of the year we want the sunset time for
 * @param latitude The latitude in degrees
 * @param longitude The longitude in degrees
 * @param utc_offset The UTC offset in hours
 */
int
getSunsetTime(int day_of_year, double latitude, double longitude, double utc_offset)
{
	return _getSunTime(DAY_TIME_SUNSET, day_of_year, latitude, longitude, utc_offset);
}


/** @brief Returns the time of day relative to the given event
 * @param event The event we're comparing the time to
 * @param latitude[in] The latitude in degrees
 * @param longitude[in] The longitude in degrees
 * @return The number of seconds until (negative) or since the event
 * @retval INT_MIN The given event type is unknown;
 *
 * Calculates the number of seconds relative to the given event 
 * (midnight, sunrise, or sunset). Negative values mean the event has not
 * happened yet. Positive values are the length of time since it happened.
 */
int
getRelativeTimeOfDay(DayTimeEvent event, double latitude, double longitude, double utc_offset)
{
	struct tm *local = _getLocalTime();
	int time = _getSecondsSinceMidnight(local);

	int relative_time;
	switch(event) {
		case DAY_TIME_MIDNIGHT:
			relative_time = time;
			break;
		case DAY_TIME_SUNRISE:
		case DAY_TIME_SUNSET:
			relative_time = time - _getSunTime(event, _getDayOfYear(local), latitude, longitude, utc_offset);
			break;
		default:
			dp(0, "Unknown event type %d\n", event);
			relative_time = INT_MIN;
	}

	free(local);
	return relative_time;
}


/** Private helper function to calculate the time when the sun will rise 
 * or set on the given day.
 * @param event Sunrise or sunset
 * @param day_of_year The day of the year we want the sunrise/set time for
 * @param latitude The latitude in degrees
 * @param longitude The longitude in degrees
 * @param utc_offset The UTC offset in hours
 *
 * Note: calculations derived from SunTime class found on CodeProject
 * Note: these angles would be better to be kept as radians all the time
 * 		 but that's gonna be tricky, so I'll do it later.
 */
int
_getSunTime(DayTimeEvent event, int day_of_year, double latitude, double longitude, double utc_offset)
{
	// number of hours away from GMT
	double long_offset = 24 * longitude / 360;

	// sorry, don't know what this one represents ("appr. time")
	double t;
	switch(event) {
		case DAY_TIME_SUNRISE:
			t = day_of_year + (6 - long_offset) / 24;
			break;
		case DAY_TIME_SUNSET:
			t = day_of_year + (18 - long_offset) / 24;
			break;
		default:
			dp(0, "bad event type %d\n", event);
			return INT_MIN;
	}

	// mean anomaly
	double mean = (T_COEFF1 * t) - MEAN_OFFSET;

	// true longitude
	double true_long = bound(mean + SIN_MEAN_COEFF * sin(D2R(mean))
			+ SIN_2MEAN_COEFF * sin(D2R(2 * mean)) + TRUE_LONG_OFFSET, 0, 360);

	// right ascension
	double right_asc = bound(R2D(atan(TRUE_LONG_COEFF * tan(D2R(true_long)))), 
			0, 360);

	// adjust quadrant of right ascension
	double base = floor(true_long / 90) * 90;
	right_asc = base + fmod(right_asc, 90.0);
	
	// convert right ascension to an hour offset
	double right_asc_offset = 24 * right_asc / 360;
	
	// more unknown temporaries
	double sinDec = SIN_TRUE_LONG_COEFF * sin(D2R(true_long));
	double cosDec = cos(asin(sinDec));

	// local hour angle
	double cos_hour = (cos(D2R(ZENITH)) - sinDec * sin(D2R(latitude)))
			/ (cosDec * cos(D2R(latitude)));

	double hour;
	switch(event) {
		case DAY_TIME_SUNRISE:
			hour = 24 * (1 - R2D(acos(cos_hour)) / 360);
			break;
		case DAY_TIME_SUNSET:
			hour = 24 * R2D(acos(cos_hour)) / 360;
			break;
		default:
			dp(0, "bad event type %d\n", event);
			return INT_MIN;
	}

	// local time
	double local_hour = hour + right_asc_offset - T_COEFF2 * t - LOCAL_OFFSET;

	// universal time
	double universal = bound(local_hour - long_offset + utc_offset, 0, 24);

	int seconds = (int)(universal * 3600);
/*	printf("N%d, lH%f, t%f, M%f, L%f, RA%f, cH%f, H%f, T%f, UT%f, s%d\n",
			day_of_year, long_offset, t, mean, true_long, right_asc, 
			cos_hour, hour, local_hour, universal, seconds);
*/
	// time in seconds
	return seconds;
}


/**
 * Private helper function to get the local time in a struct tm
 * @warning Remember to free the struct when you're done with it!
 */
struct tm *
_getLocalTime(void)
{
	time_t t = time(NULL);
	return localtime(&t);
}


/**
 * Private helper function to convert tm struct to number of seconds
 * since midnight.
 */
int
_getSecondsSinceMidnight(struct tm *local)
{
	return (local->tm_hour * 60 * 60) 
			+ (local->tm_min * 60)
			+ local->tm_sec;
}


/**
 * Private helper function to convert tm struct to number of seconds
 * since midnight.
 */
int
_getDayOfYear(struct tm *local)
{
	return local->tm_yday;
}
