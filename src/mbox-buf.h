/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOX_BUF_H
#define __MBOX_BUF_H

#include <stddef.h>

#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned char mboxChar;
typedef struct mboxBuf mboxBuf;

typedef struct mboxBuf {
    mboxChar *data;
    size_t offset;
    size_t len;
    size_t capacity;
} mboxBuf;

mboxBuf *mboxBufAlloc(size_t capacity);
void mboxBufRelease(mboxBuf *buf);

mboxChar *mboxBufGetData(mboxBuf *buf);
void mboxBufSetLen(mboxBuf *buf, size_t len);
size_t mboxBufLen(mboxBuf *buf);
size_t mboxBufGetOffset(mboxBuf *buf);
void mboxBufSetOffset(mboxBuf *buf, size_t offset);
mboxChar *mboxBufGetData(mboxBuf *buf);
mboxChar mboxBufGetChar(mboxBuf *buf);
mboxChar mboxBufGetCharAt(mboxBuf *buf, size_t at);
void mboxBufAdvance(mboxBuf *buf);
void mboxBufAdvanceBy(mboxBuf *buf, size_t by);
void mboxBufRewindBy(mboxBuf *buf, size_t by);
int mboxBufWouldOverflow(mboxBuf *buf, size_t additional);
int mboxBufMatchChar(mboxBuf *buf, mboxChar ch);
int mboxBufMatchCharAt(mboxBuf *buf, mboxChar ch, size_t at);
void mboxBufSetCapacity(mboxBuf *buf, size_t capacity);
size_t mboxBufCapacity(mboxBuf *buf);
int mboxBufExtendBuffer(mboxBuf *buf, unsigned int additional);
int mboxBufExtendBufferIfNeeded(mboxBuf *buf, size_t additional);
void mboxBufToLowerCase(mboxBuf *buf);
void mboxBufToUpperCase(mboxBuf *buf);
void mboxBufPutChar(mboxBuf *buf, mboxChar ch);
mboxChar mboxBufUnPutChar(mboxBuf *buf);
int mboxStrCmp(mboxBuf *b1, mboxChar *s);
int mboxBufNCmp(mboxBuf *b1, mboxBuf *b2, size_t size);
int mboxBufNCaseCmp(mboxBuf *b1, mboxBuf *b2, size_t size);
int mboxBufStrNCmp(mboxBuf *b1, mboxChar *s, size_t len);
int mboxBufStrNCaseCmp(mboxBuf *b1, mboxChar *s, size_t len);
int mboxBufCmp(mboxBuf *b1, mboxBuf *b2);
int mboxBufCaseCmp(mboxBuf *b1, mboxBuf *b2);
void mboxBufSlice(mboxBuf *buf, size_t from, size_t to, size_t size);
mboxBuf *mboxBufDupRaw(mboxChar *s, size_t len, size_t capacity);
mboxBuf *mboxBufDup(mboxBuf *buf);
mboxBuf *mboxBufMaybeDup(mboxBuf *buf);
size_t mboxBufWrite(mboxBuf *buf, mboxChar *s, size_t len);

void mboxBufCatLen(mboxBuf *buf, const void *d, size_t len);
void mboxBufCatPrintf(mboxBuf *b, const char *fmt, ...);
mboxBuf *mboxBufEscapeString(mboxBuf *buf);

int *mboxBufComputePrefixTable(mboxChar *pattern, size_t patternlen);
int mboxBufContainsPatternWithTable(mboxBuf *buf, int *table, mboxChar *pattern,
        size_t patternlen);
int mboxBufContainsCasePatternWithTable(mboxBuf *buf, int *table,
        mboxChar *pattern, size_t patternlen);
int mboxBufContainsPattern(mboxBuf *buf, mboxChar *pattern, size_t patternlen);
int mboxBufContainsCasePattern(mboxBuf *buf, mboxChar *pattern,
        size_t patternlen);

void mboxBufArrayRelease(mboxBuf **arr, int count);
mboxBuf **mboxBufSplit(mboxChar *to_split, mboxChar delimiter, int *count);
int mboxBufIsMimeEncoded(mboxBuf *buf);
void mboxBufDecodeMimeEncodedInplace(mboxBuf *buf);
mboxBuf *mboxBufDecodeMimeEncoded(mboxBuf *buf);

#ifdef __cplusplus
}
#endif
#endif
