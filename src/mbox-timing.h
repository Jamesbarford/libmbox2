/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __TIMING_H
#define __TIMING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void mboxTimerStart(struct timeval *timer);
double mboxTimerEnd(struct timeval *timer);

#ifdef __cplusplus
}
#endif

#endif
