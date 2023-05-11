/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbox-buf.h"
#include "mbox-common-headers.h"
#include "mbox-io.h"
#include "mbox-logger.h"
#include "mbox-parser.h"

#define MBOX_MESSAGE_RETRIES (5)

#define MBOX_CONTENT_TYPE_MULTIPART "multipart/alternative"

#define isLine(ch) ((ch) == '\r' || (ch) == '\n')

/* For when a message has multiple boundaries */
typedef struct mboxMultiMsg {
    mboxBuf *body;       /* Body of the message section with no headers */
    mboxRBTree *headers; /* headers for this section */
} mboxMultiMsg;

void
mboxParserCtxInit(mboxParserCtx *ctx, int id, int readfd, size_t file_size)
{
    ctx->id = id;
    ctx->parsed = 0;
    ctx->err = MBOX_IO_OK;
    ctx->ioctx = mboxIONew(readfd, file_size);
}

/* Create a new context */
mboxParserCtx *
mboxParserCtxNew(int id, int readfd, size_t file_size)
{
    mboxParserCtx *ctx = (mboxParserCtx *)malloc(sizeof(mboxParserCtx));
    mboxParserCtxInit(ctx, id, readfd, file_size);
    return ctx;
}

void
mboxParserMsgCtxRelease(mboxParserMsgCtx *ctx)
{
    if (ctx) {
        ctx->err = 0;
        mboxBufRelease(ctx->buf);
        mboxBufRelease(ctx->end_boundary);
        mboxRBTreeRelease(ctx->headers);
    }
}

/* Detemine if from our current offset if we are  looking at the boundary */
static inline int
mboxParseIsBoundary(mboxBuf *ctxbuf, mboxBuf *boundary)
{
    return strncmp((char *)ctxbuf->data + ctxbuf->offset,
                   (char *)boundary->data, boundary->len) == 0;
}

/* An email can be chopped up into bits and this is the mark */
static mboxBuf *
parseBoundaryMark(mboxBuf *content_type)
{
    mboxBuf *boundary = NULL;

    while (!mboxBufMatchChar(content_type, 'b') &&
            !mboxBufMatchChar(content_type, '\0')) {
        mboxBufAdvance(content_type);
    }

    if (mboxBufMatchChar(content_type, '\0')) {
        return NULL;
    }

    while (!mboxBufMatchChar(content_type, '=') &&
            !mboxBufMatchChar(content_type, '\0')) {
        mboxBufAdvance(content_type);
    }

    if (mboxBufMatchChar(content_type, '\0')) {
        return NULL;
    }

    boundary = mboxBufAlloc(200);
    /* move passed '=' */
    mboxBufAdvance(content_type);
    if (mboxBufMatchChar(content_type, '"')) {
        mboxBufAdvance(content_type);

        while (!mboxBufMatchChar(content_type, '"')) {
            mboxBufPutChar(boundary, mboxBufGetChar(content_type));
            mboxBufAdvance(content_type);
        }
    } else {
        while (!mboxBufMatchChar(content_type, '\0')) {
            if (!mboxBufMatchChar(content_type, '\r')) {
                mboxBufPutChar(boundary, mboxBufGetChar(content_type));
            }
            mboxBufAdvance(content_type);
        }
    }

    return boundary;
}

/* Parse both the name of the sender and the email address:
 * Can be in the following formats:
 * 1. Hacker Noon <support@hackernoon.com>
 * 2. "Hacker Noon" <support@hackernoon.com>
 * 3. <support@hackernoon.com>
 * 4. =?utf-8?Q?Hacker=20Noon?= <support@hackernoon.com>
 * */
void
mboxParseFrom(mboxBuf *buf, mboxBuf **name, mboxBuf **email)
{
    int has_name = 1;

    while (isspace(mboxBufGetChar(buf))) {
        mboxBufAdvance(buf);
    }

    /* We have no name, this can legitimately happen */
    if (mboxBufMatchChar(buf, '<')) {
        mboxBufAdvance(buf);
        has_name = 0;
    }

    if (has_name) {
        *name = mboxBufAlloc(100);
        if (mboxBufMatchChar(buf, '"')) {
            mboxBufAdvance(buf);
        }

        while (!mboxBufMatchChar(buf, '<') && !mboxBufMatchChar(buf, '"') &&
                !mboxBufMatchChar(buf, '\0')) {
            if (mboxBufMatchCharAt(buf, '<', 1) && mboxBufMatchChar(buf, ' ')) {
                break;
            }
            mboxBufPutChar(*name, mboxBufGetChar(buf));
            mboxBufAdvance(buf);
        }

        while (!mboxBufMatchChar(buf, '<')) {
            mboxBufAdvance(buf);
        }
        mboxBufAdvance(buf);

        if (mboxBufIsMimeEncoded(*name)) {
            mboxBufDecodeMimeEncodedInplace(*name);
        }
    }

    *email = mboxBufAlloc(100);
    while (!mboxBufMatchChar(buf, '<') && !mboxBufMatchChar(buf, '\0')) {
        mboxBufPutChar(*email, mboxBufGetChar(buf));
        mboxBufAdvance(buf);
    }
}

/* Function will scan a line to see if we are at the end of a message
 * - `MBOX_MSG_EOF` not enough in the buffer to do the comparison
 * - `MBOX_MSG_NO_MATCH` no match for boundary
 * - `MBOX_MSG_EOM` is end of message
 * - `MBOX_MSG_MATCH` matched but not the end of the message
 * */
static int
mboxIsEOM(mboxBuf *buf, mboxBuf *boundary)
{
    if (mboxBufMatchChar(buf, '-') && mboxBufMatchCharAt(buf, '-', 1)) {
        if (mboxBufWouldOverflow(buf, 3 + mboxBufLen(boundary))) {
            /* Ran out of buffer to read */
            return MBOX_MSG_EOF;
        }

        int contains = strncmp((char *)buf->data + buf->offset + 2,
                               (char *)boundary->data, boundary->len) == 0;

        if (contains == 1) {
            if (buf->data[buf->offset + 2 + boundary->len] == '-' &&
                    buf->data[buf->offset + 3 + boundary->len]) {

                /* Advance passed the '--' */
                buf->offset += 4 + boundary->len;
                /* Is the end of the message */
                return MBOX_MSG_EOM;
            }
            /* Not the end of the message but there was a match */
            buf->offset += 2 + boundary->len;
            return MBOX_MSG_MATCH;
        }
    }

    return MBOX_MSG_NO_MATCH;
}

static int
mboxBufMatchFromLine(mboxBuf *buf)
{
    return buf->data[buf->offset] == 'F' && buf->data[buf->offset + 1] == 'r' &&
            buf->data[buf->offset + 2] == 'o' &&
            buf->data[buf->offset + 3] == 'm' &&
            buf->data[buf->offset + 4] == ' ';
}

/* Can be both used by the io parser and the parser with a full message in the
 * buffer. IO Parser has to use it to be able to determine where the boundary is
 */
mboxRBTree *
mboxParseEmailHeaders(mboxBuf *buf)
{
    mboxBuf *key = mboxBufAlloc(3000);
    mboxBuf *value = mboxBufAlloc(3000);
    mboxRBTree *headers = rbTreeNew((rbFreeKey *)mboxBufRelease,
            (rbFreeValue *)mboxBufRelease, (rbCompareKey *)mboxBufCaseCmp);
    int run = 1;

    while (isLine(buf->data[buf->offset])) {
        buf->offset++;
    }

    while (run && buf->offset < buf->len) {
        /* We don't want to parse this header */
        if (mboxBufMatchFromLine(buf)) {
            while (buf->data[buf->offset] != '\n') {
                if (buf->data[buf->offset] != '\r') {
                    mboxBufPutChar(key, buf->data[buf->offset]);
                }
                buf->offset++;
            }
            buf->offset++;
            mboxBuf *k = mboxBufDupRaw((mboxChar *)"__FROM_LINE__", 13, 14);
            mboxRBTreeInsert(headers, k, mboxBufDup(key));
        }

        /* Why couldn't all the parsing be this simple :( */
        while (buf->data[buf->offset] != ':') {
            mboxBufPutChar(key, buf->data[buf->offset++]);
        }

        /* Move past ': ' */
        buf->offset += 2;

        while (1) {
            while (buf->data[buf->offset] != '\n') {
                char ch = buf->data[buf->offset];
                /* There _shouldn't_ be '\r' but there seem to be */
                if (ch != '\r' && ch != '\t') {
                    mboxBufPutChar(value, ch);
                }
                buf->offset++;
            }
            buf->offset++;
            if (mboxBufMatchChar(buf, ' ') || mboxBufMatchChar(buf, '\t')) {
                /* Keep parsing */
                continue;
                /* Finished parsing all headers */
            } else {
                if (mboxBufIsMimeEncoded(value)) {
                    mboxBuf *decoded = mboxBufDecodeMimeEncoded(value);
                    mboxRBTreeInsert(headers, mboxBufDup(key), decoded);
                } else {
                    mboxRBTreeInsert(headers, mboxBufDup(key),
                            mboxBufDup(value));
                }

                mboxBufSlice(key, 0, 0, 0);
                mboxBufSlice(value, 0, 0, 0);

                if (isLine(mboxBufGetChar(buf))) {
                    while (isLine(mboxBufGetChar(buf))) {
                        mboxBufAdvance(buf);
                    }
                    run = 0;
                }

                /* Finished parsing the value */
                break;
            }
        }
    }
    mboxBufRelease(key);
    mboxBufRelease(value);
    return headers;
}

/* Go from current position which should be just after parsing the headers
 * untill the next boundary */
static mboxBuf *
mboxParseMessageBody(mboxParserMsgCtx *ctx, mboxBuf *boundary, int *msgstatus)
{
    size_t boundary_len = mboxBufLen(boundary);
    size_t start = mboxBufGetOffset(ctx->buf);
    size_t bodylen = 0;
    mboxBuf *buf = ctx->buf;
    mboxBuf *body = NULL;

    /* Go from here to the next boundary and return */
    while (buf->offset < buf->len) {
        while (buf->data[buf->offset] != '-' &&
                buf->data[buf->offset + 1] != boundary->data[0]) {
            buf->offset++;
        }
        buf->offset++;

        int isboundary = mboxParseIsBoundary(ctx->buf, boundary);

        if (isboundary) {
            /* Move back to '--' this sets things up for the next parse and
             * means we won't get the `--` in the body */
            bodylen = buf->offset - 2 - start;

            body = mboxBufDupRaw(buf->data + start, bodylen, bodylen + 10);
            buf->offset += boundary_len;

            if (buf->data[buf->offset] == '-' &&
                    buf->data[buf->offset + 1] == '-') {
                /* Move to the end of the message '--<boundary>--' */
                buf->offset += 4;
                *msgstatus = MBOX_MSG_EOM;
            } else {
                buf->offset -= boundary_len + 2;
                *msgstatus = MBOX_MSG_MATCH;
            }
            break;
        }
    }

    if (buf->offset + 1 == buf->len) {
        *msgstatus = MBOX_MSG_EOF;
    }
    return body;
}

/* With this what we really want to do is emit a string which is the part
 * between the most recently parsed header and the next boundary mark up
 * until the terminal boundary mark.
 *
 * Caller needs to free the string and headers on `*alt`
 */
static int
mboxParseMulitpartAlternative(mboxParserMsgCtx *ctx, mboxBuf *boundary,
        mboxMultiMsg *alt)
{
    /* Assumes:
     * - Boundary is the mark of new messages as well as what will be the
     *   terminator
     * - There are going to be `n` number of messages using this boundary
     * - Where with each match we will need to parse some headers
     * */
    int run = 1;
    int retval = 0;
    mboxBuf *buf = ctx->buf;

    while (run) {
        while (!mboxBufMatchChar(ctx->buf, '-')) {
            mboxBufAdvance(ctx->buf);
        }

        retval = mboxIsEOM(ctx->buf, boundary);

        switch (retval) {
        case MBOX_MSG_EOF:
            run = 0;
            break;

        case MBOX_MSG_NO_MATCH:
            buf->offset++;
            continue;

        case MBOX_MSG_EOM:
        case MBOX_MSG_MATCH:
            alt->headers = mboxParseEmailHeaders(buf);
            alt->body = mboxParseMessageBody(ctx, boundary, &retval);
            run = 0;
            break;
        }
    }
    return retval;
}

static void
mboxParserParseMessage(mboxParserMsgCtx *ctx)
{
    size_t len = mboxBufLen(ctx->buf);

    /* For now we just want:
     * - text/plain (so we can read the message)
     * - store in redbacktree the:
     *   - headers in the body
     *   - name of attachments
     *   - alternative content types in the body */
    mboxBufSetOffset(ctx->buf, 0);
    while (mboxBufGetOffset(ctx->buf) < len) {
        while (!mboxBufMatchChar(ctx->buf, '-')) {
            mboxBufAdvance(ctx->buf);
        }

        int is_eom = mboxIsEOM(ctx->buf, ctx->end_boundary);

        switch (is_eom) {
        case MBOX_MSG_NO_MATCH:
            mboxBufAdvance(ctx->buf);
            continue;
        case MBOX_MSG_EOM:
            return;
        case MBOX_MSG_MATCH:
            while (isLine(mboxBufGetChar(ctx->buf))) {
                mboxBufAdvance(ctx->buf);
            }
            /* we can parse some headers, this will advance the offset to
             * just passed these headers too */
            mboxRBTree *rb = mboxParseEmailHeaders(ctx->buf);
            if (!rb) {
                break;
            }

            mboxBuf *content_type = mboxHeadersGet(rb,
                    MBOX_HEADER_CONTENT_TYPE);
            mboxBuf *transfer_encoding = mboxHeadersGet(rb,
                    MBOX_HEADER_CONTENT_TRANSFER_ENCODING);

            if (transfer_encoding != NULL) {
                mboxBufToLowerCase(transfer_encoding);
                loggerDebug("transfer encoding: %s\n", transfer_encoding->data);
            }

            int multipart_idx = 0;

            /* We are looking at a plain text email */
            if (mboxBufStrNCaseCmp(content_type, (mboxChar *)"text/plain",
                        10) == 0) {
                printf("plain text\n");
            }
            /* The multipart email is a tricky one, it contains the boundary
             * for the next content type ideally we want to zip straight to
             * that offset.
             *
             * This is what I'm trying to do with the `cstrContainsPattern`
             * call but the index returned seems to be part of the way
             * through the string which is not really useful */
            else if ((multipart_idx = mboxBufContainsPattern(content_type,
                              (mboxChar *)MBOX_CONTENT_TYPE_MULTIPART,
                              static_sizeof(MBOX_CONTENT_TYPE_MULTIPART) -
                                      1)) != -1) {

                mboxBuf *boundary_mark = parseBoundaryMark(content_type);

                if (!boundary_mark) {
                    loggerPanic("NO BOUNDARY\n");
                }

                mboxMultiMsg alt;

                int match_status = mboxParseMulitpartAlternative(ctx,
                        boundary_mark, &alt);

                while (match_status == MBOX_MSG_MATCH) {
                    /* Do something?? */
                    mboxRBTreeRelease(alt.headers);
                    mboxBufRelease(alt.body);
                    match_status = mboxParseMulitpartAlternative(ctx,
                            boundary_mark, &alt);
                }

                switch (match_status) {
                case MBOX_MSG_EOM:
                    /* Do something */
                    mboxRBTreeRelease(alt.headers);
                    mboxBufRelease(alt.body);
                    break;
                case MBOX_MSG_EOF:
                    loggerPanic("Should not have got MBOX_MSG_EOF\n");
                case MBOX_MSG_NO_MATCH:
                    loggerPanic("Should not have got MBOX_MSG_NO_MATCH\n");
                }
                mboxBufRelease(boundary_mark);
            }

            mboxRBTreeRelease(rb);
            break;
        }
    }
}

/* This seesm to be more robust looking for \nFrom */
static int
mboxIsFromLine(mboxChar *s)
{
    if (s[0] == '\n' && s[1] == 'F' && s[2] == 'r' && s[3] == 'o' &&
            s[4] == 'm' && s[5] == ' ') {
        return 1;
    }
    return 0;
}

/* Go backwards until pattern 'From ' */
void
mboxParserCtxSeekStart(mboxParserCtx *ctx)
{
    mboxIOCtx *ioctx = ctx->ioctx;
    mboxBuf *buf = ioctx->buf;
    size_t idx = 0;
    ssize_t jumpsize = 0;
    ssize_t rbytes = 0;
    ssize_t offset = ioctx->file_offset;
    int found_offset = 0;

    jumpsize = MBOX_IO_READ_SIZE;

    while (1) {

        offset = offset - jumpsize < 0 ? 0 : offset - jumpsize;
        rbytes = mboxIORead(ioctx, MBOX_IO_READ_SIZE, 0, offset);

        if (rbytes == 0 || ioctx->err != 0) {
            break;
        }

        for (idx = 0; idx < buf->len; ++idx) {
            if (ioctx->file_offset != 0) {
                if (buf->data[idx] == '\n' && buf->data[idx + 1] == 'F' &&
                        buf->data[idx + 2] == 'r' &&
                        buf->data[idx + 3] == 'o' &&
                        buf->data[idx + 4] == 'm' &&
                        buf->data[idx + 5] == ' ') {
                    ioctx->start_offset = offset + idx;
                    found_offset = 1;
                    break;
                }
            } else {
                if (buf->data[idx] == 'F' && buf->data[idx + 1] == 'r' &&
                        buf->data[idx + 2] == 'o' &&
                        buf->data[idx + 3] == 'm' &&
                        buf->data[idx + 4] == ' ') {
                    ioctx->start_offset = offset + idx;
                    found_offset = 1;
                    break;
                }
            }
        }

        if (found_offset) {
            break;
        }
    }
    mboxBufSetLen(buf, 0);
    buf->offset = 0;
    ioctx->file_offset = ioctx->start_offset;
}

/* TODO: this is the think we need to optimise the hell out of */
int
mboxParserCtxSeekNextFromLine(mboxParserCtx *ctx)
{
    mboxIOCtx *ioctx = ctx->ioctx;
    mboxBuf *buf = ioctx->buf;
    ssize_t rbytes = 0;

    while (1) {
        /* Go one line at a time */
        while (buf->offset < buf->len) {
            if (buf->data[buf->offset] == '\n') {
                break;
            }
            buf->offset++;
        }

        if (mboxIsFromLine(buf->data + buf->offset) == 1) {
            buf->offset++;
            return 1;
        }

        if (buf->offset + 10 >= buf->len) {
            if (ioctx->file_offset > ioctx->end_offset) {
                return 0;
            }

            rbytes = mboxIORead(ioctx, MBOX_IO_READ_SIZE, buf->len,
                    ioctx->file_offset);

            if (rbytes == 0 && ioctx->err == MBOX_IO_EOF) {
                printf("REACHED MBOX_IO_EOF\n");
                return 1;
            }
            /* We have an error */
            else if (ioctx->err != 0) {
                return ioctx->err;
            }
            ioctx->file_offset += rbytes;
            buf->offset--;
        }

        /* Move passed the '\n' we are looking at */
        buf->offset++;
    }
    return 0;
}

/**
 * We need to find the terminal boundary marker which indicates a full email
 * message. This is the best way I can think of doing it at the moment. Will
 * set the length of the internal buffer to the length of the message.
 * Returns:
 *   0 -> not yet a full message
 *   1 -> we have a full message
 *
 * Each of the blocks below is a read and we are wanting to find 'boundary'
 * which demonstates the problem. The buffer is the bottom block
 *                       v
 * [......] [.......] [.......] <- reads
 * [..........................] <- buffer
 */
static int
mboxBufCanParseHeaders(mboxBuf *buf)
{
    size_t len = buf->len;
    mboxChar c1, c2, c3;

    while (buf->offset < len) {
        /* Advance until we meet either a '\n' or a '\0', a '\0' indicates we
         * have run out of buffer to read */
        while (buf->data[buf->offset] != '\n' &&
                buf->data[buf->offset] != '\0') {
            buf->offset++;
        }

        /* No point in continuing */
        if (buf->data[buf->offset] == '\0' ||
                buf->data[buf->offset + 1] == '\0') {
            return 0;
        }

        c1 = buf->data[buf->offset];
        c2 = buf->data[buf->offset + 1];
        c3 = buf->data[buf->offset + 2];

        /* finding two consecutive '\n' characters denotes the end of the
         * headers section we also do the same check for the presence of '\r'
         * too which seems to exist in gmail */
        if (c1 == '\n' && c2 == '\n') {
            buf->offset += 2;
            return 1;
        } else if (c1 == '\n' && c2 == '\r' && c3 == '\n') {
            buf->offset += 3;
            return 1;
        }
        buf->offset++;
    }
    return 0;
}

/* From a 'From ' line find the end of the headers section reading in more of
 * the file if required */
int
mboxParserCtxSeekHeaders(mboxParserCtx *ctx)
{
    mboxIOCtx *ioctx = ctx->ioctx;
    mboxBuf *buf = ioctx->buf;
    ssize_t rbytes = 0;

    while (!mboxBufCanParseHeaders(buf)) {
        if (buf->data[0] == '\0' || buf->offset + 1 >= buf->len) {
            rbytes = mboxIORead(ioctx, MBOX_IO_READ_SIZE, buf->len,
                    ioctx->file_offset);

            if (rbytes == 0 || ioctx->err != 0) {
                return 0;
            }
            ioctx->file_offset += rbytes;
        }
    }
    return 1;
}

/* Scan from current From line up to but not including the next 'F' */
mboxIOMsg *
mboxParserCtxGetNextMessage(mboxParserCtx *ctx)
{
    mboxIOCtx *ioctx = ctx->ioctx;
    mboxBuf *buf = ioctx->buf;
    mboxBuf *full_msg = NULL;
    mboxIOMsg *msg = NULL;
    size_t msg_start = ioctx->offset;
    size_t msg_end = 0;
    size_t newlen = buf->len - buf->offset;

    /**
     * Move the buffer along, we can drop what we have parsed and move what
     * we haven't parsed to the start of the buffer.
     *
     * Then set the length of the buffer, the offset to zero (so we process
     * what is in the buffer)
     */
    if (newlen != 0) {
        mboxBufSlice(buf, buf->offset, 0, newlen);
    }

    /* Prepare the buffer for the next parse */
    mboxBufSetLen(buf, newlen);
    mboxBufSetOffset(buf, 0);

    /* It's faster to scan for \n\n than it is to search for the from line,
     * so may as well keep this */
    if (mboxParserCtxSeekHeaders(ctx) != 1) {
        return NULL;
    }

    /* Find next from line denoting the next message */
    if (mboxParserCtxSeekNextFromLine(ctx) != 1) {
        return NULL;
    }

    ioctx->offset += buf->offset;
    msg_end = ioctx->offset;

    if (ioctx->offset >= ioctx->end_offset || ioctx->err == MBOX_IO_EOF) {
        ctx->err = MBOX_PARSE_DONE;
    }

    full_msg = mboxBufDupRaw(buf->data, buf->offset, buf->offset);
    msg = malloc(sizeof(mboxIOMsg));
    msg->buf = full_msg;
    msg->start_offset = msg_start;
    msg->end_offset = msg_end;

    ctx->parsed++;

    return msg;
}

void
mboxParserCtxRelease(mboxParserCtx *ctx)
{
    if (ctx) {
        mboxIOCtx *ioctx = ctx->ioctx;
        mboxBufRelease(ioctx->buf);
        free(ctx);
    }
}
