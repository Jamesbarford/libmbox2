/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOX_PARSER_H
#define __MBOX_PARSER_H

#include <sys/types.h>

#include <stddef.h>

#include "mbox-buf.h"
#include "mbox-io.h"
#include "mbox-msg.h"
#include "mbox-redblacktree.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBOX_MSG_EOF (-1)
#define MBOX_MSG_EMPTY (0)
#define MBOX_MSG_OK (1)
#define MBOX_MSG_ERR (2)
#define MBOX_MSG_NO_MATCH (3)
#define MBOX_MSG_EOM (4)
#define MBOX_MSG_MATCH (5)

#define MBOX_ENCODING_7BIT (1)
#define MBOX_ENCODING_8BIT (2)
#define MBOX_ENCODING_BASE64 (3)
#define MBOX_ENCODING_BINARY (4)
#define MBOX_ENCODING_QUOTED_PRINTABLE (5)

#define MBOX_PARSE_DONE (1)

typedef enum {
    MBOX_ERR_EOF = -1,
    MBOX_ERR_NO_FILE = 0,
} MboxErrno;

typedef struct mboxParserMsgCtx {
    mboxBuf *buf;          /* The buffer containing a full MBOX message */
    mboxBuf *end_boundary; /* The string marking the end of the message */
    int err;               /* Error if we encountered any while parsing */
    mboxRBTree *headers;   /* All of the headers in the message, TODO: should we
                              put these on the context ?*/
} mboxParserMsgCtx;

typedef struct mboxParserCtx {
    int id;           /* Id of the context */
    int err;          /* Error code '0' is all good */
    size_t parsed;    /* How many messages this context has passed*/
    mboxIOCtx *ioctx; /* File context that we are parsing */
} mboxParserCtx;

/* Initialise context, not needed if new was used to create the context */
void mboxParserCtxInit(mboxParserCtx *ctx, int id, int readfd,
        size_t file_size);

/* Instantiate a new context */
mboxParserCtx *mboxParserCtxNew(int id, int fd, size_t file_size);

/* Read some data from somewhere in the file */
ssize_t mboxParserCtxRead(mboxParserCtx *ctx, size_t size, size_t buf_offset,
        ssize_t file_offset);

/* Parse the email headers to a redblack tree */
mboxRBTree *mboxParseEmailHeaders(mboxBuf *buf);

/* Find the starting 'From' pattern */
void mboxParserCtxSeekStart(mboxParserCtx *ctx);

/* Free a context */
void mboxParserCtxRelease(mboxParserCtx *ctx);

/* Will return a mboxIOMsg containing a full message ready to be parsed */
mboxIOMsg *mboxParserCtxGetNextMessage(mboxParserCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif
