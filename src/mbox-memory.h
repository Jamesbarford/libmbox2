/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOX_MEMORY_POOL_H
#define __MBOX_MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>

typedef struct mboxMemPool {
    void *data;
    size_t blocks;
    size_t allocated;
    uint32_t *bitset;
} mboxMemPool;

mboxMemPool *mboxMemPoolNew(size_t block_count);
void *mboxMemPoolAlloc(mboxMemPool *p, size_t size);
void mboxMemPoolFree(mboxMemPool *p, void *ptr);
void mboxMemPoolRelease(mboxMemPool *p);
void mboxMemPoolPrint(mboxMemPool *p);

#endif
