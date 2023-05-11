/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbox-buf.h"
#include "mbox-common-headers.h"
#include "mbox-date.h"
#include "mbox-io.h"
#include "mbox-logger.h"
#include "mbox-msg.h"
#include "mbox-parser.h"
#include "mbox-redblacktree.h"

static mboxMsgLite *
mboxMsgLiteNew(void)
{
    mboxMsgLite *m = (mboxMsgLite *)malloc(sizeof(mboxMsgLite));
    m->end = m->start = 0;
    m->unix_timestamp = 0;
    m->date = NULL;
    m->from = NULL;
    m->msg_id = NULL;
    m->subject = NULL;
    m->preview = NULL;
    return m;
}

void
mboxMsgLitePrint(mboxMsgLite *m)
{
    if (m == NULL) {
        printf("NULL\n");
        return;
    }

    printf("ID: %s\n", (char *)(m->msg_id ? (char *)m->msg_id->data : "(NIL)"));
    printf("From: %s\n", (char *)(m->from ? (char *)m->from->data : "(NIL)"));
    printf("Subject: %s\n",
            (char *)(m->subject ? (char *)m->subject->data : "(NIL)"));
    printf("Date: %s\n", (char *)(m->date ? (char *)m->date->data : "(NIL)"));
    printf("Start offset: %zu, End offset: %zu\n", m->start, m->end);
    printf("********************* PREVIEW *********************\n");
    printf("%s\n", (char *)(m->preview ? (char *)m->preview->data : "(NIL)"));
    printf("*-------------------------------------------------------------------*\n\n");
}

void
mboxMsgLiteClear(mboxMsgLite *m)
{
    if (m) {
        m->start = m->end = m->unix_timestamp = 0;
        free(m->date);
        free(m->from);
        free(m->msg_id);
        free(m->preview);
        free(m->subject);
    }
}

void
mboxMsgLiteRelease(mboxMsgLite *m)
{
    if (m) {
        mboxMsgLiteClear(m);
        free(m);
    }
}

mboxMsgLite *
mboxMsgLiteFromBuffer(mboxBuf *buf, ssize_t start_offset, ssize_t end_offset)
{
    struct mboxDate d;

    mboxRBTree *headers = mboxParseEmailHeaders(buf);
    mboxBuf *from = mboxHeadersGet(headers, MBOX_HEADER_FROM);
    mboxBuf *subject = mboxHeadersGet(headers, MBOX_HEADER_SUBJECT);
    mboxBuf *date = mboxHeadersGet(headers, MBOX_HEADER_DATE);
    mboxBuf *msg_id = mboxHeadersGet(headers, MBOX_HEADER_MSG_ID);
    mboxBuf *preview = mboxBufDupRaw(buf->data + buf->offset,
            MBOX_BUF_PREVIEW_LEN, MBOX_BUF_PREVIEW_LEN);
    mboxBuf *from_line = mboxHeadersGet(headers, MBOX_HEADER_FROM_LINE);

    long unix_timestamp = 0;
    mboxMsgLite *msg = malloc(sizeof(mboxMsgLite));

    if (date) {
        mboxDateStringToStruct((char *)date->data, MBOX_DATE_FORMAT, &d);
        if (d.tm_hour != -1) {
            unix_timestamp = mboxDateStructToUnix(&d);
        }
    }

    if (from == NULL) {
        // loggerDebug("FROM %.*s\n", 20, ctx->buf->data);
        mboxRBTreePrintKeysAsString(headers);
    }

    msg->msg_id = mboxBufMaybeDup(msg_id);
    msg->from = mboxBufMaybeDup(from);
    msg->subject = mboxBufMaybeDup(subject);
    msg->date = mboxBufMaybeDup(date);
    msg->from_line = mboxBufMaybeDup(from_line);
    msg->preview = preview;
    msg->unix_timestamp = unix_timestamp;
    msg->start = start_offset;
    msg->end = end_offset;

    /* Clean everything up */
    mboxRBTreeRelease(headers);
    return msg;
}

mboxMsgLite *
mboxMsgLiteCreate(mboxIOMsg *ctx)
{
    mboxMsgLite *msg = mboxMsgLiteFromBuffer(ctx->buf, ctx->start_offset,
            ctx->end_offset);
    mboxBufRelease(ctx->buf);
    free(ctx);
    return msg;
}

int
mboxMsgListCompareByDate(void *d1, void *d2)
{
    mboxMsgLite *m1 = (mboxMsgLite *)d1;
    mboxMsgLite *m2 = (mboxMsgLite *)d2;

    if (m1->unix_timestamp == -1 || m2->unix_timestamp == -1) {
        return -1;
    }

    if (m1->unix_timestamp < m2->unix_timestamp) {
        return -1;
    } else if (m1->unix_timestamp > m2->unix_timestamp) {
        return 1;
    }
    return 0;
}

int
mboxMsgListCompareByFrom(void *d1, void *d2)
{
    mboxMsgLite *m1 = (mboxMsgLite *)d1;
    mboxMsgLite *m2 = (mboxMsgLite *)d2;

    if (m1->from == NULL || m2->from == NULL) {
        return -1;
    }

    size_t len = m1->from->len < m2->from->len ? m1->from->len : m2->from->len;

    return mboxBufNCaseCmp(m1->from, m2->from, len);
}

void
mboxMsgListSortByDate(mboxList *l)
{
    mboxListQSort(l, mboxMsgListCompareByDate);
}

void
mboxMsgListSortByFrom(mboxList *l)
{
    mboxListQSort(l, mboxMsgListCompareByFrom);
}

mboxList *
mboxMsgListFilterBySender(mboxList *l, char *sender)
{
    if (l->len == 0) {
        return NULL;
    }
    mboxList *filtered = mboxListNew();
    mboxLNode *n = l->root;
    mboxMsgLite *msg = n->data;
    size_t len = strlen(sender);
    size_t count = l->len;
    mboxBuf *from = NULL;
    int *table = mboxBufComputePrefixTable((mboxChar *)sender, len);

    do {
        from = msg->from;

        if (mboxBufContainsCasePatternWithTable(from, table, (mboxChar *)sender,
                    len) != -1) {
            mboxListAddHead(filtered, msg);
        }
        n = n->next;
        msg = n->data;
        count--;
    } while (count > 0);
    free(table);

    return filtered;
}
