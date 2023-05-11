/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __DATE_H
#define __DATE_H

#ifdef __cplusplus
extern "C" {
#endif

#define MBOX_DATE_FORMAT "%d %b %Y %H:%M:%S %z"
#define MBOX_DATE_FORMAT_FROM "%a %b %d %H:%M:%S %z %Y"

/* This is verbtim of time struct with added tm_zone_diff */
struct mboxDate {
    int tm_sec;       /* Seconds.	[0-60] (1 leap second) */
    int tm_min;       /* Minutes.	[0-59] */
    int tm_hour;      /* Hours.	[0-23] */
    int tm_mday;      /* Day.		[1-31] */
    int tm_mon;       /* Month.	[0-11] */
    int tm_year;      /* Year	- 1900.  */
    int tm_wday;      /* Day of week.	[0-6] */
    int tm_yday;      /* Days in year.[0-365]	*/
    int tm_isdst;     /* DST.		[-1/0/1]*/
    int tm_zone_diff; /* Time difference from current timezone */
};

int mboxDateStringToStruct(char *strdate, char *format, struct mboxDate *t);
long mboxDateStructToUnix(struct mboxDate *d);
#ifdef __cplusplus
}
#endif

#endif
