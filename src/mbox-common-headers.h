/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOX_COMMON_HEADER_H
#define __MBOX_COMMON_HEADER_H

#include "mbox-buf.h"
#include "mbox-redblacktree.h"


#ifdef __cplusplus
extern "C" {
#endif

#define MBOX_HEADER_FROM_LINE (0)
#define MBOX_HEADER_CONTENT_TYPE (1)
#define MBOX_HEADER_CONTENT_TRANSFER_ENCODING (2)
#define MBOX_HEADER_FROM (3)
#define MBOX_HEADER_DATE (4)
#define MBOX_HEADER_GMAIL_LABELS (5)
#define MBOX_HEADER_SUBJECT (6)
#define MBOX_HEADER_MSG_ID (7)

mboxBuf *mboxHeadersGet(mboxRBTree *headers, int header);
mboxChar *mboxHeaderGetString(int header);

#ifdef __cplusplus
}
#endif

#endif
