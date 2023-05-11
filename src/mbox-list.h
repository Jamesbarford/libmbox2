/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __LIST_H
#define __LIST_H

#include <pthread.h>
#include <stddef.h>
/* doubly circular linked list */

#ifdef __cplusplus
extern "C" {
#endif

typedef int mboxListFindCallback(void *data, void *needle);
typedef void mboxListFreeData(void *data);
typedef int mboxListCmp(void *data, void *needle);
typedef void mboxListPrintCallback(void *data);

typedef struct mboxLNode {
    void *data;
    struct mboxLNode *next;
    struct mboxLNode *prev;
} mboxLNode;

typedef struct mboxList {
    size_t len;
    mboxLNode *root;
    pthread_mutex_t lock; /* for if we want a thread safe list we can use this
                     with the thread safe variants of the functions below */
    mboxListFreeData *freedata;
} mboxList;

#define mboxListSetFreedata(l, fn) ((l)->freedata = (fn))

mboxList *mboxListNew(void);
mboxList *mboxListTSNew(void);

void *mboxListFind(mboxList *l, void *search_data,
        mboxListFindCallback *compare);
void mboxListAddHead(mboxList *l, void *val);
void mboxListAddTail(mboxList *l, void *val);
void *mboxListRemoveHead(mboxList *l);
void *mboxListRemoveTail(mboxList *l);
void mboxListQSort(mboxList *l, mboxListCmp *compare);
void mboxListPrint(mboxList *l, mboxListPrintCallback *print);

/* TS being thread safe */
void *mboxListTSFind(mboxList *l, void *search_data,
        mboxListFindCallback *compare);
void mboxListTSAddHead(mboxList *l, void *val);
void mboxListTSAddTail(mboxList *l, void *val);
void *mboxListTSRemoveHead(mboxList *l);
void *mboxListTSRemoveTail(mboxList *l);

void mboxListRelease(mboxList *l);
mboxList *mboxListAppendListTail(mboxList *main, mboxList *aux);

#ifdef __cplusplus
}
#endif

#endif
