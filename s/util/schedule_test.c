#include "i.h"

#define LATITUDE (-33.2166666)
#define LONGITUDE (150.8333333)
#define UTC_OFFSET (10)

int m_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

int
main(int argc, char** argv)
{
	int i, j, d = 1;

	for (i = 1; i <= 12; ++i) {
		printf("\t%s\n\trise\tset\n", months[i-1]);
		for (j = 1; j <= m_days[i-1]; ++j) {
			int rise = getSunriseTime(d, LATITUDE, LONGITUDE, UTC_OFFSET);
			int set = getSunsetTime(d, LATITUDE, LONGITUDE, UTC_OFFSET);
			printf("%2d\t%04d\t%04d\n", j,
					convertTimeToPrintable(rise),
					convertTimeToPrintable(set));
			d++;
		}
	}

	return 0;
}


int
convertTimeToPrintable(int seconds)
{
	int hours = seconds / 3600;
	int minutes = (seconds % 3600) / 60;

	return hours * 100 + minutes;
}
