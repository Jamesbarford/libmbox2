/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <string.h>

#include "mbox-array.h"

mboxArray *
mboxArrayNew(size_t memsize, size_t capacity)
{
    mboxArray *a = malloc(sizeof(mboxArray));
    a->size = 0;
    a->capacity = capacity;
    a->memsize = memsize;
    a->entries = calloc(capacity, memsize);
    return a;
}

void
mboxArraySet(mboxArray *a, size_t idx, void *value)
{
    memcpy((char *)a->entries + a->memsize * idx, value, a->memsize);
}

void
mboxArrayPush(mboxArray *a, void *value)
{
    memcpy((char *)a->entries + a->memsize * a->size, value, a->memsize);
    a->size++;
}

void
mboxArraySort(mboxArray *a, int (*cmp)(const void *e1, const void *e2))
{
    qsort(a->entries, a->size, a->memsize, cmp);
}
