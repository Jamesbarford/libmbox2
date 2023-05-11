/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mbox-logger.h"
#include "mbox-memory.h"

#define MEM_BLOCK_SIZE 8
#define MEM_SIZE_INFO (sizeof(uint32_t) + 1)

/**
 * Store how many blocks worth of data are allocated
 * within in this pointer */
static void
mboxMemPoolSetBlockCount(void *ptr, unsigned int len)
{
    unsigned char *_ptr = (unsigned char *)ptr;
    _ptr -= MEM_SIZE_INFO;
    _ptr[0] = (len >> 24) & 0xFF;
    _ptr[1] = (len >> 16) & 0xFF;
    _ptr[2] = (len >> 8) & 0xFF;
    _ptr[3] = len & 0xFF;
    (_ptr)[4] = '\0';
    _ptr += MEM_SIZE_INFO;
}

/**
 * Retrieve how many blocks are allocated within this piece of
 * memory */
static uint32_t
mboxMemPoolGetBlockCount(void *ptr)
{
    unsigned char *_ptr = (unsigned char *)ptr;
    _ptr -= MEM_SIZE_INFO;

    return ((uint32_t)_ptr[0] << 24) | ((uint32_t)_ptr[1] << 16) |
            ((uint32_t)_ptr[2] << 8) | (uint32_t)_ptr[3];
}

mboxMemPool *
mboxMemPoolNew(size_t block_count)
{
    mboxMemPool *p = malloc(sizeof(mboxMemPool));
    p->data = malloc(block_count * MEM_BLOCK_SIZE);
    p->blocks = block_count;
    p->allocated = 0;
    p->bitset = calloc((block_count + 31) / 32, sizeof(uint32_t));
    return p;
}

void *
mboxMemPoolAlloc(mboxMemPool *p, size_t size)
{
    unsigned int block_count = (size + MEM_SIZE_INFO + MEM_BLOCK_SIZE - 1) /
            MEM_BLOCK_SIZE;
    uint32_t mask = 0xFFFFFFFFU >> (32 - block_count);

    for (int i = 0; i < p->blocks; ++i) {
        /* Check that the blocks are free */
        if ((p->bitset[i / 32] & (mask << (i & (32 - 1)))) == 0) {
            for (int j = i; j < i + block_count; ++j) {
                /* mark as allocated */
                p->bitset[j / 32] |= (1UL << (j & (32 - 1)));
            }
            /* Store how many blocks we've allocated */
            p->allocated += block_count;
            /* Return pointer to the first byte of free memory */
            void *ptr = (void *)p->data + MEM_SIZE_INFO + i * MEM_BLOCK_SIZE;
            /* Set how many blocks were used in the pointer */
            mboxMemPoolSetBlockCount(ptr, block_count);
            return ptr;
        }
    }

    loggerDebug("out of mem\n");
    return NULL;
}

void
mboxMemPoolFree(mboxMemPool *p, void *ptr)
{
    /* Index of memory block */
    uint32_t len = mboxMemPoolGetBlockCount(ptr);
    mboxMemPoolSetBlockCount(ptr, 0);
    p->allocated -= len;

    /* Clear all bits from the bit set */
    for (int i = len - 1; i >= 0; --i) {
        int idx = (((char *)ptr - (char *)p->data) / MEM_BLOCK_SIZE) + i;
        int bit = idx & (32 - 1);
        int word = idx / 32;
        p->bitset[word] &= ~(1UL << bit);
    }
}

void
mboxMemPoolRelease(mboxMemPool *p)
{
    free(p->data);
    free(p);
}

void
mboxMemPoolPrint(mboxMemPool *p)
{
    for (int i = 0; i < p->blocks; ++i) {
        if ((i & (32 - 1)) == 0) {
            printf("0x%08x ", p->bitset[i / 32]);
        }

        printf("%d", (p->bitset[i / 32] >> (i & (32 - 1))) & 1);
        if ((i & (32 - 1)) == 31) {
            printf("\n");
        }
    }
}

uint32_t
mboxMemPoolMemCount(mboxMemPool *p)
{
    return p->allocated * MEM_BLOCK_SIZE;
}
