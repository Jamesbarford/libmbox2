/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MACROS_H
#define __MACROS_H

#ifdef __cplusplus
extern "C" {
#endif

#define static_sizeof(x) (sizeof(x) / sizeof(x[0]))

#ifdef __cplusplus
}
#endif


#endif
