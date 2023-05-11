/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOX_H
#define __MBOX_H

#include <stddef.h>

#include "mbox-list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A handle on the mbox file, internally this is a bit of a monster */
typedef struct mbox mbox;

mbox *mboxReadOpen(char *file_path, int perms);

mboxList *mboxParse(mbox *m, size_t thread_count);
void mboxRelease(mbox *m);

#ifdef __cplusplus
}
#endif

#endif
