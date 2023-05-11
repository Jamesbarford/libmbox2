/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sys/time.h>

#include <stddef.h>

#include "mbox-timing.h"

void
mboxTimerStart(struct timeval *timer)
{
    gettimeofday(timer, NULL);
}

double
mboxTimerEnd(struct timeval *timer)
{
    struct timeval end;
    gettimeofday(&end, NULL);

    return (end.tv_sec - timer->tv_sec) * 1000.0 +
            (end.tv_usec - timer->tv_usec) / 1000.0;
}
