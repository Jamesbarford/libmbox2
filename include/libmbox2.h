/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __LIBMBOX2_H
#define __LIBMBOX2_H

#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A handle on the mbox file, internally this is a bit of a monster */
typedef struct mbox mbox;
typedef unsigned char mboxChar;

typedef struct _mboxMsgLite mboxMsgLite;

typedef int mboxListFindCallback(void *data, void *needle);
typedef void mboxListFreeData(void *data);
typedef int mboxListCmp(void *data, void *needle);

enum MboxListType {
    MBOX_LIST,
    MBOX_LIST_TS,
};

typedef struct mboxLNode {
    void *data;
    struct mboxLNode *next;
    struct mboxLNode *prev;
} mboxLNode;

typedef struct mboxList {
    size_t len;
    mboxLNode *root;
    enum MboxListType type;
    pthread_mutex_t lock; /* for if we want a thread safe list we can use this
                     with the thread safe variants of the functions below */
    mboxListFreeData *freedata;
    mboxListCmp *compare;
} mboxList;

typedef struct mboxBuf {
    mboxChar *data;
    size_t offset;
    size_t len;
    size_t capacity;
} mboxBuf;

/* There is so much noise in the file that this should help cut it down,
 * it is a stipped down version of the message */
struct _mboxMsgLite {
    mboxBuf *msg_id;     /* Unique message id */
    mboxBuf *from;       /* Who message is from */
    mboxBuf *subject;    /* What message is about */
    mboxBuf *date;       /* When it was sent */
    mboxBuf *preview;    /* 420 character or less preview of the email */
    mboxBuf *from_line;  /* The line commencing 'From xxx xxxx' */
    long unix_timestamp; /* Unix timetamp in milliseconds for when message was
                            sent */
    /* These are so we can find them in the file quickly by using fseek and
     * friends */
    size_t start; /* The start of this offset in the file */
    size_t end;   /* The end of the offset in the file */
};

mboxList *mboxListNew(void);
mboxList *mboxListTSNew(void);

void *mboxListFind(mboxList *l, void *search_data,
        mboxListFindCallback *compare);
void mboxListAddHead(mboxList *l, void *val);
void mboxListAddTail(mboxList *l, void *val);
void *mboxListRemoveHead(mboxList *l);
void *mboxListRemoveTail(mboxList *l);
void mboxListQSort(mboxList *l);

/* TS being thread safe */
void *mboxListTSFind(mboxList *l, void *search_data,
        mboxListFindCallback *compare);
void mboxListTSAddHead(mboxList *l, void *val);
void mboxListTSAddTail(mboxList *l, void *val);
void *mboxListTSRemoveHead(mboxList *l);
void *mboxListTSRemoveTail(mboxList *l);

void mboxListRelease(mboxList *l);

void mboxMsgListSortByDate(mboxList *msglist);
void mboxMsgListSortBySender(mboxList *msglist);
mboxList *mboxMsgListFilterBySender(mboxList *l, char *sender);

mbox *mboxReadOpen(char *file_path, int perms);

void mboxMsgLitePrint(mboxMsgLite *m);

mboxList *mboxParse(mbox *m, size_t thread_count);
void mboxRelease(mbox *m);

/* Save a linked list of lite messages to a simple file format that will allow
 * quickly loading in all of the messages again without having to scan the raw
 * mbox file to find all off the messages
 *
 * 0 3000\r\n // this is what is persisted to a file
 * */
int mboxIdxSave(char *filename, mboxList *l);

mboxList *mboxIdxLoad(char *idxfile, char *mboxfile, unsigned int thread_count);
#ifdef __cplusplus
}
#endif

#endif
