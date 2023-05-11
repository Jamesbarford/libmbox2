#ifndef __LIST_H
#define __LIST_H

#include <pthread.h>
#include <stddef.h>
/* doubly circular linked list */

typedef int listFindCallback(void *data, void *needle);
typedef void listFreeData(void *data);
typedef int listCmp(void *data, void *needle);

typedef struct lNode {
    void *data;
    struct lNode *next;
    struct lNode *prev;
} lNode;

typedef struct list {
    size_t len;
    lNode *root;
    pthread_mutex_t lock; /* for if we want a thread safe list we can use this
                     with the thread safe variants of the functions below */
    listFreeData *freedata;
    listCmp *compare;
} list;

list *listNew(void);

void *listFind(list *l, void *search_data, listFindCallback *compare);
void listAddHead(list *l, void *val);
void listAddTail(list *l, void *val);
void *listRemoveHead(list *l);
void *listRemoveTail(list *l);
void listQSort(list *l);

/* TS being thread safe */
void *listTSFind(list *l, void *search_data, listFindCallback *compare);
void listTSAddHead(list *l, void *val);
void listTSAddTail(list *l, void *val);
void *listTSRemoveHead(list *l);
void *listTSRemoveTail(list *l);
size_t listTSLength(list *l);

void listRelease(list *l);

#endif
