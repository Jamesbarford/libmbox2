/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MSG_H
#define __MSG_H

#include <sys/types.h>

#include <stddef.h>

#include "mbox-buf.h"
#include "mbox-list.h"

#ifdef __cplusplus
extern "C" {
#endif
#define MBOX_BUF_PREVIEW_LEN (420)

typedef struct _mboxMsgLite mboxMsgLite;

typedef enum {
    MBOX_PTR_FULL_MESSAGE_OFFSET = 0,
    MBOX_PTR_HEADER_OFFSET,
    MBOX_PTR_TEXT_OFFSET,
    MBOX_PTR_ATTACHMENT_OFFSET,
    MBOS_PTR_MSG_LITE,
} MboxPtrType;

typedef struct mboxIOMsg {
    mboxBuf *buf; /* Full message from from line to the start of the next one */
    size_t start_offset; /* Where the message starts in the file */
    size_t end_offset;   /* Where the message ends in the file */
} mboxIOMsg;

/* There is so much noise in the file that this should help cut it down,
 * it is a stipped down version of the message */
struct _mboxMsgLite {
    mboxBuf *msg_id;     /* Unique message id */
    mboxBuf *from;       /* Who message is from */
    mboxBuf *subject;    /* What message is about */
    mboxBuf *date;       /* When it was sent */
    mboxBuf *preview;    /* 420 character or less preview of the email */
    mboxBuf *from_line;  /* The line commencing 'From xxx xxxx' */
    long unix_timestamp; /* Unix timetamp in milliseconds for when message was
                            sent */
    /* These are so we can find them in the file quickly by using fseek and
     * friends */
    size_t start; /* The start of this offset in the file */
    size_t end;   /* The end of the offset in the file */
};

mboxMsgLite *mboxMsgLiteFromBuffer(mboxBuf *buf, ssize_t start_offset,
        ssize_t end_offset);
mboxMsgLite *mboxMsgLiteCreate(mboxIOMsg *ctx);
void mboxMsgLitePrint(mboxMsgLite *m);
void mboxMsgLiteRelease(mboxMsgLite *m);
void mboxMsgListSortByDate(mboxList *msglist);
void mboxMsgListSortBySender(mboxList *msglist);
mboxList *mboxMsgListFilterBySender(mboxList *l, char *sender);

#ifdef __cplusplus
}
#endif

#endif
