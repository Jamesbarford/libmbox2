/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include "mbox-buf.h"
#include "mbox-common-headers.h"
#include "mbox-redblacktree.h"

static const mboxBuf mboxExpectedHeaders[] = {
    [MBOX_HEADER_FROM_LINE] = { (mboxChar *)"__FROM_LINE__", 0, 13, 0 },
    [MBOX_HEADER_CONTENT_TYPE] = { (mboxChar *)"Content-Type", 0, 12, 0 },
    [MBOX_HEADER_CONTENT_TRANSFER_ENCODING] = { (mboxChar *)"Content-Transfer-Encoding",
            0, 25, 0 },
    [MBOX_HEADER_FROM] = { (mboxChar *)"From", 0, 4, 0 },
    [MBOX_HEADER_DATE] = { (mboxChar *)"Date", 0, 4, 0 },
    [MBOX_HEADER_GMAIL_LABELS] = { (mboxChar *)"X-Gmail-Labels", 0, 14, 0 },
    [MBOX_HEADER_SUBJECT] = { (mboxChar *)"Subject", 0, 8, 0 },
    [MBOX_HEADER_MSG_ID] = { (mboxChar *)"Message-ID", 0, 10, 0 },
};

/* Convenience for indexing the headers */
mboxBuf *
mboxHeadersGet(mboxRBTree *headers, int header)
{
    const mboxBuf *key = &mboxExpectedHeaders[header];
    return mboxRBTreeGet(headers, (mboxBuf *)key);
}

mboxChar *
mboxHeaderGetString(int header)
{
    const mboxBuf *key = &mboxExpectedHeaders[header];
    if (key) {
        return key->data;
    }
    return NULL;
}
