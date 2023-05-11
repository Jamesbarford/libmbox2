/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOX_ARRAY_H
#define __MBOX_ARRAY_H

#include <stddef.h>
#include <stdlib.h>

typedef struct mboxArray {
    size_t size;
    size_t capacity;
    size_t memsize;
    void *entries;
} mboxArray;

#define mboxArrayGet(v, idx) \
    ((idx) < (v)->size ? (v)->entries + (v)->memsize * (idx) : NULL)

mboxArray *mboxArrayNew(size_t memsize, size_t capacity);
void mboxArraySet(mboxArray *a, size_t idx, void *value);
void mboxArrayPush(mboxArray *a, void *value);
void mboxArraySort(mboxArray *a, int (*cmp)(const void *e1, const void *e2));

#endif
