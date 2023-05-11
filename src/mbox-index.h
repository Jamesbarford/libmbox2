/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOX_INDEX_H
#define __MBOX_INDEX_H

#include "mbox-list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Save a list of "mboxMsgLite" to a mbox-idx file */
int mboxIdxSave(char *filename, mboxList *l);

/* Load a list of mboxMsgLite from an mbox-idx file and its corresponding mbox
 * file */
mboxList *mboxIdxLoad(char *idxfile, char *mboxfile, unsigned int thread_count);

#ifdef __cplusplus
}
#endif

#endif
