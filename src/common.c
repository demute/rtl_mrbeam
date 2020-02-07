#include "common.h"

#include <sys/time.h>

double date_string_to_double (char* str)
{

    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_isdst = 1;
    time_t thetime;
    if (strptime (str, "%Y-%m-%d/%H:%M:%S", & tm) != NULL)
        thetime = mktime (&tm);
    else
        exit_error ("failed to extract time");
    return (double) thetime;
}

char* double_to_date_string (double uClock)
{
    long uClockSecs   = (long) uClock;
    double uClockFrac = uClock - (double) uClockSecs;
    time_t uc    = uClockSecs;
    struct tm* t = localtime (& uc);

    char *days[7] = {"(sun)", "(mon)", "(tue)", "(wed)", "(thu)", "(fri)", "(sat)"};
    char *day = days[t->tm_wday];

    static char buf[128];
    char fracbuf[32];
    strftime (buf, sizeof (buf), "%Y-%m-%d/%H:%M:%S", t);
    snprintf (fracbuf, sizeof (fracbuf), "%09.6f", uClockFrac);
    strcat (buf, & fracbuf[2]);
    strcat (buf, day);
    return buf;
}


double get_time (void)
{
    struct timeval t;
    gettimeofday (& t, 0);
    double timestamp = (double) t.tv_sec + (double) t.tv_usec / 1000000;
    return timestamp;
}
