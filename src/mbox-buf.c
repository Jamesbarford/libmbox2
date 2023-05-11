/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbox-buf.h"
#include "mbox-logger.h"

mboxBuf *
mboxBufAlloc(size_t capacity)
{
    mboxBuf *buf = malloc(sizeof(mboxBuf));
    buf->capacity = capacity + 10;
    buf->len = 0;
    buf->offset = 0;
    buf->data = malloc(sizeof(mboxChar) * buf->capacity);
    return buf;
}

void
mboxBufRelease(mboxBuf *buf)
{
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

mboxChar *
mboxBufGetData(mboxBuf *buf)
{
    return buf->data;
}

size_t
mboxBufGetOffset(mboxBuf *buf)
{
    return buf->offset;
}

void
mboxBufSetOffset(mboxBuf *buf, size_t offset)
{
    buf->offset = offset;
}

void
mboxBufSetLen(mboxBuf *buf, size_t len)
{
    buf->len = len;
    buf->data[len] = '\0';
}

size_t
mboxBufLen(mboxBuf *buf)
{
    return buf->len;
}

void
mboxBufSetCapacity(mboxBuf *buf, size_t capacity)
{
    buf->capacity = capacity;
}

size_t
mboxBufCapacity(mboxBuf *buf)
{
    return buf->capacity;
}

/* At is relative to the current internal offset */
mboxChar
mboxBufGetCharAt(mboxBuf *buf, size_t at)
{
    if (buf->offset + at > buf->len) {
        return '\0';
    }

    return buf->data[buf->offset + at];
}

mboxChar
mboxBufGetChar(mboxBuf *buf)
{
    if (buf->offset > buf->len) {
        return '\0';
    }

    return buf->data[buf->offset];
}

/* At is relative to the current internal offset */
int
mboxBufMatchCharAt(mboxBuf *buf, mboxChar ch, size_t at)
{
    if (at + buf->offset > buf->len) {
        return 0;
    }
    return buf->data[buf->offset + at] == ch;
}

int
mboxBufMatchChar(mboxBuf *buf, mboxChar ch)
{
    return buf->data[buf->offset] == ch;
}

void
mboxBufAdvance(mboxBuf *buf)
{
    buf->offset++;
}

void
mboxBufAdvanceBy(mboxBuf *buf, size_t by)
{
    buf->offset += by;
}

void
mboxBufRewindBy(mboxBuf *buf, size_t by)
{
    buf->offset -= by;
}

int
mboxBufWouldOverflow(mboxBuf *buf, size_t additional)
{
    return buf->offset + additional > buf->len;
}

/* Grow the capacity of the string buffer by `additional` space */
int
mboxBufExtendBuffer(mboxBuf *buf, unsigned int additional)
{
    size_t new_capacity = buf->capacity + additional;
    if (new_capacity <= buf->capacity) {
        return -1;
    }

    mboxChar *_str = buf->data;
    mboxChar *tmp = (mboxChar *)realloc(_str, new_capacity);

    if (tmp == NULL) {
        return 0;
    }
    buf->data = tmp;

    mboxBufSetCapacity(buf, new_capacity);

    return 1;
}

/* Only extend the buffer if the additional space required would overspill the
 * current allocated capacity of the buffer */
int
mboxBufExtendBufferIfNeeded(mboxBuf *buf, size_t additional)
{
    if ((buf->len + 1 + additional) >= buf->capacity) {
        return mboxBufExtendBuffer(buf, additional > 256 ? additional : 256);
    }
    return 0;
}

void
mboxBufToLowerCase(mboxBuf *buf)
{
    for (size_t i = 0; i < buf->len; ++i) {
        buf->data[i] = tolower(buf->data[i]);
    }
}

void
mboxBufToUpperCase(mboxBuf *buf)
{
    for (size_t i = 0; i < buf->len; ++i) {
        buf->data[i] = toupper(buf->data[i]);
    }
}

void
mboxBufPutChar(mboxBuf *buf, mboxChar ch)
{
    mboxBufExtendBufferIfNeeded(buf, 100);
    buf->data[buf->len] = ch;
    buf->data[buf->len + 1] = '\0';
    buf->len++;
}

mboxChar
mboxBufUnPutChar(mboxBuf *buf)
{
    if (buf->len == 0) {
        return '\0';
    }

    mboxChar ch = buf->data[buf->len - 2];
    buf->len--;
    buf->data[buf->len] = '\0';
    return ch;
}

int
mboxStrCmp(mboxBuf *b1, mboxChar *s)
{
    return strcmp((char *)b1->data, (char *)s);
}

/* DANGER: This assumes the both buffers at a minimum have `size` bytes in their
 * data */
int
mboxBufNCmp(mboxBuf *b1, mboxBuf *b2, size_t size)
{
    return memcmp(b1->data, b2->data, size);
}

int
mboxBufNCaseCmp(mboxBuf *b1, mboxBuf *b2, size_t size)
{
    return strncasecmp((char *)b1->data, (char *)b2->data, size);
}

int
mboxBufStrNCmp(mboxBuf *b1, mboxChar *s, size_t len)
{
    return strncmp((char *)b1->data, (char *)s, len);
}

int
mboxBufStrNCaseCmp(mboxBuf *b1, mboxChar *s, size_t len)
{
    return strncasecmp((char *)b1->data, (char *)s, len);
}

int
mboxBufCmp(mboxBuf *b1, mboxBuf *b2)
{
    size_t l1 = b1->len;
    size_t l2 = b2->len;
    size_t min = l1 < l2 ? l1 : l2;
    return memcmp(b1->data, b2->data, min);
}

int
mboxBufCaseCmp(mboxBuf *b1, mboxBuf *b2)
{
    size_t l1 = b1->len;
    size_t l2 = b2->len;
    size_t min = l1 < l2 ? l1 : l2;
    return strncasecmp((char *)b1->data, (char *)b2->data, min);
}

void
mboxBufSlice(mboxBuf *buf, size_t from, size_t to, size_t size)
{
    memmove(buf->data + to, buf->data + from, size);
    buf->data[size] = '\0';
    buf->len = size;
}

mboxBuf *
mboxBufDupRaw(mboxChar *s, size_t len, size_t capacity)
{
    mboxBuf *dupe = mboxBufAlloc(capacity);
    memcpy(dupe->data, s, len);
    mboxBufSetLen(dupe, len);
    mboxBufSetCapacity(dupe, capacity);
    return dupe;
}

/* We duplicate a lot of strings sometimes they are null */
mboxBuf *
mboxBufMaybeDup(mboxBuf *buf)
{
    if (buf) {
        return mboxBufDup(buf);
    }
    return NULL;
}

mboxBuf *
mboxBufDup(mboxBuf *buf)
{
    mboxBuf *dupe = mboxBufAlloc(buf->len);
    memcpy(dupe->data, buf->data, buf->len);
    mboxBufSetLen(dupe, buf->len);
    mboxBufSetCapacity(dupe, buf->len);
    return dupe;
}

size_t
mboxBufWrite(mboxBuf *buf, mboxChar *s, size_t len)
{
    mboxBufExtendBufferIfNeeded(buf, len);
    memcpy(buf->data, s, len);
    buf->len = len;
    buf->data[buf->len] = '\0';
    return len;
}

void
mboxBufCatLen(mboxBuf *buf, const void *d, size_t len)
{
    mboxBufExtendBufferIfNeeded(buf, len);
    memcpy(buf->data + buf->len, d, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
}

void
mboxBufCatPrintf(mboxBuf *b, const char *fmt, ...)
{
    va_list ap, copy;
    va_start(ap, fmt);

    /* Probably big enough */
    int min_len = 512;
    int bufferlen = strlen(fmt) * 3;
    bufferlen = bufferlen > min_len ? bufferlen : min_len;
    char *buf = (char *)malloc(sizeof(char) * bufferlen);

    while (1) {
        buf[bufferlen - 2] = '\0';
        va_copy(copy, ap);
        vsnprintf(buf, bufferlen, fmt, ap);
        va_end(copy);
        if (buf[bufferlen - 2] != '\0') {
            free(buf);
            bufferlen *= 2;
            buf = malloc(bufferlen);
            if (buf == NULL) {
                return;
            }
            continue;
        }
        break;
    }

    mboxBufCatLen(b, buf, strlen(buf));
    free(buf);
    va_end(ap);
}

mboxBuf *
mboxBufEscapeString(mboxBuf *buf)
{
    mboxBuf *outstr = mboxBufAlloc(buf->capacity);
    mboxChar *ptr = buf->data;

    if (buf == NULL) {
        return NULL;
    }

    while (*ptr) {
        if (*ptr > 31 && *ptr != '\"' && *ptr != '\\') {
            mboxBufPutChar(outstr, *ptr);
        } else {
            mboxBufPutChar(outstr, '\\');
            switch (*ptr) {
            case '\\':
                mboxBufPutChar(outstr, '\\');
                break;
            case '\"':
                mboxBufPutChar(outstr, '\"');
                break;
            case '\b':
                mboxBufPutChar(outstr, 'b');
                break;
            case '\n':
                mboxBufPutChar(outstr, 'n');
                break;
            case '\t':
                mboxBufPutChar(outstr, 't');
                break;
            case '\v':
                mboxBufPutChar(outstr, 'v');
                break;
            case '\f':
                mboxBufPutChar(outstr, 'f');
                break;
            case '\r':
                mboxBufPutChar(outstr, 'r');
                break;
            default:
                mboxBufCatPrintf(outstr, "u%04x", (unsigned int)*ptr);
                break;
            }
        }
        ++ptr;
    }
    return outstr;
}

int *
mboxBufComputePrefixTable(mboxChar *pattern, size_t patternlen)
{
    int *table = (int *)malloc(sizeof(int) * patternlen);
    table[0] = 0;
    int k = 0;

    for (size_t i = 1; i < patternlen; ++i) {
        while (k > 0 && pattern[k] != pattern[i]) {
            k = table[k - 1];
        }
        if (pattern[k] == pattern[i]) {
            k++;
        }
        table[i] = k;
    }
    return table;
}

int
mboxBufContainsPatternWithTable(mboxBuf *buf, int *table, mboxChar *pattern,
        size_t patternlen)
{
    mboxChar *_buf = buf->data;
    int retval = -1;

    if (patternlen == 0) {
        return retval;
    }

    size_t q = 0;
    for (size_t i = 0; i < buf->len; ++i) {
        if (_buf[i] == '\0') {
            break;
        }
        while (q > 0 && pattern[q] != _buf[i]) {
            q = table[q - 1];
        }
        if (pattern[q] == _buf[i]) {
            q++;
        }

        if (q == patternlen) {
            retval = i - patternlen + 1;
            break;
        }
    }

    return retval;
}

/* Implementation of KMP Matcher algorithm from introduction to algorithms */
int
mboxBufContainsPattern(mboxBuf *buf, mboxChar *pattern, size_t patternlen)
{
    int retval = -1;
    int *table = mboxBufComputePrefixTable(pattern, patternlen);
    retval = mboxBufContainsPatternWithTable(buf, table, pattern, patternlen);
    free(table);
    return retval;
}

int
mboxBufContainsCasePatternWithTable(mboxBuf *buf, int *table, mboxChar *pattern,
        size_t patternlen)
{
    size_t q = 0;
    mboxChar *_buf = buf->data;
    int retval = -1;

    if (patternlen == 0) {
        return retval;
    }

    for (size_t i = 0; i < buf->len; ++i) {
        if (_buf[i] == '\0') {
            break;
        }
        while (q > 0 && tolower(pattern[q]) != tolower(_buf[i])) {
            q = table[q - 1];
        }
        if (tolower(pattern[q]) == tolower(_buf[i])) {
            q++;
        }

        if (q == patternlen) {
            retval = i - patternlen + 1;
            break;
        }
    }

    return retval;
}

int
mboxBufContainsCasePattern(mboxBuf *buf, mboxChar *pattern, size_t patternlen)
{
    int retval = -1;
    int *table = mboxBufComputePrefixTable(pattern, patternlen);
    retval = mboxBufContainsCasePatternWithTable(buf, table, pattern,
            patternlen);
    free(table);
    return retval;
}

void
mboxBufArrayRelease(mboxBuf **arr, int count)
{
    if (arr) {
        for (int i = 0; i < count; ++i) {
            mboxBufRelease(arr[i]);
        }
        free(arr);
    }
}

/**
 * Split into cstrings on delimiter
 */
mboxBuf **
mboxBufSplit(mboxChar *to_split, mboxChar delimiter, int *count)
{
    mboxBuf **outArr, **tmp;
    mboxChar *ptr;
    int arrsize, memslot;
    long start, end;

    if (*to_split == delimiter) {
        to_split++;
    }

    memslot = 5;
    ptr = to_split;
    *count = 0;
    start = end = 0;
    arrsize = 0;

    if ((outArr = malloc(sizeof(char) * memslot)) == NULL)
        return NULL;

    while (*ptr != '\0') {
        if (memslot < arrsize + 5) {
            memslot *= 5;
            if ((tmp = (mboxBuf **)realloc(outArr,
                         sizeof(mboxBuf) * memslot)) == NULL)
                goto error;
            outArr = tmp;
        }

        if (*ptr == delimiter) {
            outArr[arrsize] = mboxBufDupRaw(to_split + start, end - start,
                    end - start);
            ptr++;
            arrsize++;
            start = end + 1;
            end++;
            continue;
        }

        end++;
        ptr++;
    }

    outArr[arrsize] = mboxBufDupRaw(to_split + start, end - start, end - start);
    arrsize++;
    *count = arrsize;

    return outArr;

error:
    mboxBufArrayRelease(outArr, arrsize);
    return NULL;
}

int
mboxBufIsMimeEncoded(mboxBuf *buf)
{
    const char *prefix = "=?utf-8?Q?";
    const char *suffix = "?=";

    return strncmp(prefix, (char *)buf->data, 10) == 0 &&
            strncmp((char *)buf->data + (buf->len - 2), suffix, 2) == 0;
}

static int
hexToInt(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static void
decodeMimeEncoded(mboxChar *str, mboxChar *out, size_t *outlen)
{
    size_t len = 0;
    mboxChar *in = str;

    while (*in) {
        if (*in == '=') {
            int hi = hexToInt(*(in + 1));
            int lo = hexToInt(*(in + 2));

            if (hi != -1 && lo != -1) {
                out[len] = (hi << 4) | lo;
                in += 3;
            } else {
                out[len] = *in++;
            }
            len++;
        } else {
            out[len] = *in++;
            len++;
        }
    }
    *outlen = len;
}

/* Decodes the string inplace mutating the string passed in. Will set the new
 * length on the string */
void
mboxBufDecodeMimeEncodedInplace(mboxBuf *buf)
{
    size_t outlen = 0;
    mboxBufSlice(buf, 10, 0, buf->len - 10 - 2);
    decodeMimeEncoded(buf->data, buf->data, &outlen);
    mboxBufSetLen(buf, outlen);
}

/* Allocates a new string for the decoded version */
mboxBuf *
mboxBufDecodeMimeEncoded(mboxBuf *buf)
{
    int has_alloced = 0;
    size_t outlen = 0;
    size_t len = buf->len;
    mboxBuf *out = mboxBufAlloc(512);
    mboxChar buffer[2048];
    mboxChar *tmp = NULL;

    if (len < sizeof(buffer)) {
        tmp = buffer;
    } else {
        has_alloced++;
        tmp = (mboxChar *)malloc(sizeof(mboxChar) * len);
    }

    memcpy(tmp, buf + 10, len - 10 - 2);
    tmp[len - 10 - 2] = '\0';

    decodeMimeEncoded(tmp, out->data, &outlen);
    mboxBufSetLen(out, outlen);

    if (has_alloced) {
        free(tmp);
    }
    return out;
}
