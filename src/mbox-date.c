/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mbox-date.h"

static const char *const days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri",
    "Sat" };
static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* strptime wanted a header that I didn't want to include, so here we have a
 * home rolled version. For our use case tests are in tests.c */
int
mboxDateStringToStruct(char *strdate, char *format, struct mboxDate *t)
{
    /* inintialise the date struct's field to -1 */
    memset(t, -1, sizeof(struct mboxDate));
    /* This is for when we pass the string to strtoll */
    char *endptr = strdate;

    while (*strdate && *format) {
        if (*format == '%') {
            format++;
            switch (*format) {
            case 'a':
                for (int i = 0; i < 7; ++i) {
                    if (strncasecmp(strdate, days[i], 3) == 0) {
                        strdate += 3;
                        t->tm_wday = i;
                        break;
                    }
                }
                if (t->tm_wday == -1) {
                    return 0;
                }
                break;

            case 'b':
                for (int i = 0; i < 12; ++i) {
                    if (strncasecmp(strdate, months[i], 3) == 0) {
                        strdate += 2;
                        t->tm_mon = i;
                        break;
                    }
                }
                if (t->tm_mon == -1) {
                    return 0;
                }
                break;

            case 'd':
                t->tm_mday = strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate += 2;
                break;

            case 'm':
                t->tm_mon = strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate = endptr;
                strdate += 1;
                break;

            case 'Y':
                t->tm_year = (int)strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate = endptr;
                strdate += 1;
                break;

            case 'H':
                t->tm_hour = strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate = endptr;
                strdate += 1;
                break;

            case 'M':
                t->tm_min = strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate = endptr;
                strdate += 1;
                break;

            case 'p':
                if (strncasecmp(strdate, "AM", 2) == 0) {
                    if (t->tm_hour >= 12) {
                        t->tm_hour -= 12;
                    }
                } else if (strncasecmp(strdate, "PM", 2) == 0) {
                    t->tm_hour += 12;
                }
                t->tm_mon = strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate += 1;
                break;

            case 'S':
                t->tm_sec = strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate = endptr;
                strdate += 1;
                break;

            case 'z':
                t->tm_zone_diff = strtol(strdate, &endptr, 10);
                if (strdate == endptr) {
                    return 0;
                }
                strdate = endptr;
                strdate += 1;
                break;

            default:
                break;
            }
        }

        /* This means we parsed a number and it was the end of the string
         * That can be considered a successful parse for us. We can always
         * check if the field was populated by ensuring it is not -1 */
        if (*endptr == '\0') {
            break;
        }

        if (*format == *strdate) {
            strdate++;
        }
        format++;
    }

    return 1;
}

/**
 * Produce a unix timestamp in seconds from a date struct.
 */
long
mboxDateStructToUnix(struct mboxDate *d)
{
    struct tm t;
    /* I've tried passing in mktime((struct tm *)d) and it did not work
     * so we set all the values on a struct tm from the struct date */
    memset(&t, 0, sizeof(struct tm));
    t.tm_year = d->tm_year - 1900;
    t.tm_mon = d->tm_mon;
    t.tm_mday = d->tm_mday;
    t.tm_hour = d->tm_hour;
    t.tm_min = d->tm_min;
    t.tm_sec = d->tm_sec;

    long timestamp = (long)mktime((struct tm *)&t);

    /* Adjust for the time zone if it is not 0 and not -1, -1 signifying it is
     * not set */
    if (d->tm_zone_diff && d->tm_zone_diff != -1) {
        int hour_diff = abs(d->tm_zone_diff) / 100;
        int min_diff = abs(d->tm_zone_diff) % 100;
        if (d->tm_zone_diff < -1) {
            timestamp += hour_diff * 3600;
            timestamp += min_diff * 60;
        } else {
            timestamp -= hour_diff * 3600;
            timestamp -= min_diff * 60;
        }
    }
    return timestamp;
}
