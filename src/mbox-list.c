/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "mbox-list.h"

static mboxLNode *
mboxLNodeNew(void *data)
{
    mboxLNode *n = (mboxLNode *)malloc(sizeof(mboxLNode));
    n->data = data;
    n->next = n->prev = NULL;
    return n;
}

mboxList *
mboxListNew(void)
{
    mboxList *l = (mboxList *)malloc(sizeof(mboxList));
    l->len = 0;
    l->root = NULL;
    l->freedata = NULL;
    pthread_mutex_init(&l->lock, NULL);
    return l;
}

void
mboxListAddHead(mboxList *l, void *data)
{
    mboxLNode *n = mboxLNodeNew(data);

    if (l->root == NULL) {
        l->root = n;
        n->next = n->prev = n;
    } else {
        mboxLNode *tail = l->root->prev;
        mboxLNode *head = l->root;

        n->next = head;
        n->prev = tail;

        tail->next = n;
        head->prev = n;

        l->root = n;
    }

    l->len++;
}

void
mboxListAddTail(mboxList *l, void *data)
{
    mboxLNode *n = mboxLNodeNew(data);

    if (l->root == NULL) {
        l->root = n;
        n->next = n->prev = n;
    } else {
        mboxLNode *tail = l->root->prev;
        mboxLNode *head = l->root;

        n->next = head;
        n->prev = tail;

        tail->next = n;
        head->prev = n;
    }

    l->len++;
}

void *
mboxListRemoveHead(mboxList *l)
{
    if (l->len == 0 || l->root == NULL) {
        return NULL;
    }
    mboxLNode *head = l->root;
    void *val = head->data;

    if (l->len == 1) {
        l->root = NULL;
    } else {
        mboxLNode *tail = l->root->prev;
        l->root = l->root->next;
        tail->next = l->root;
        l->root->prev = tail;
    }

    free(head);
    l->len--;
    return val;
}

void *
mboxListRemoveTail(mboxList *l)
{
    if (l->len == 0) {
        return NULL;
    }

    mboxLNode *tail = l->root->prev;
    void *val = tail->data;

    if (l->len == 1) {
        l->root = NULL;
    } else {
        mboxLNode *new_tail = tail->prev;
        l->root->prev = new_tail;
        new_tail->next = l->root;
    }

    free(tail);
    l->len--;
    return val;
}

void *
mboxListFind(mboxList *l, void *search_data, mboxListFindCallback *compare)
{
    if (l->len == 0) {
        return NULL;
    }

    mboxLNode *current = l->root;
    size_t count = l->len;

    do {
        if (compare(current->data, search_data)) {
            return current->data;
        }

        current = current->next;
        count--;
    } while (count > 0);

    return NULL;
}

mboxList *
mboxListTSNew(void)
{
    mboxList *l = mboxListNew();
    pthread_mutex_init(&l->lock, NULL);
    return l;
}

void *
mboxListTSFind(mboxList *l, void *search_data, mboxListFindCallback *compare)
{
    void *retval = NULL;
    pthread_mutex_lock(&l->lock);
    retval = mboxListFind(l, search_data, compare);
    pthread_mutex_unlock(&l->lock);
    return retval;
}

void
mboxListTSAddHead(mboxList *l, void *val)
{
    pthread_mutex_lock(&l->lock);
    mboxListAddHead(l, val);
    pthread_mutex_unlock(&l->lock);
}

void
mboxListTSAddTail(mboxList *l, void *val)
{
    pthread_mutex_lock(&l->lock);
    mboxListAddTail(l, val);
    pthread_mutex_unlock(&l->lock);
}

void *
mboxListTSRemoveHead(mboxList *l)
{
    void *retval = NULL;
    pthread_mutex_lock(&l->lock);
    retval = mboxListRemoveHead(l);
    pthread_mutex_unlock(&l->lock);

    return retval;
}

void *
mboxListTSRemoveTail(mboxList *l)
{
    void *retval = NULL;
    pthread_mutex_lock(&l->lock);
    retval = mboxListRemoveTail(l);
    pthread_mutex_unlock(&l->lock);

    return retval;
}

static mboxLNode *
mboxListQSortPivot(mboxList *l, mboxLNode *low, mboxLNode *high,
        mboxListCmp *compare)
{
    void *pivot = high->data;
    mboxLNode *prev = low->prev;
    mboxLNode *current = NULL;

    for (current = low; current != high; current = current->next) {
        if (compare(current->data, pivot) <= 0) {
            prev = (prev == NULL) ? low : prev->next;
            void *tmp = prev->data;
            prev->data = current->data;
            current->data = tmp;
        }
    }
    prev = (prev == NULL) ? low : prev->next;
    void *tmp = prev->data;
    prev->data = current->data;
    current->data = tmp;

    return prev;
}

static void
mboxQSortHelper(mboxList *l, mboxLNode *low, mboxLNode *high, int count,
        mboxListCmp *compare)
{
    if (low == high || (count == 1 && low == high->next)) {
        return;
    }

    mboxLNode *pivot = mboxListQSortPivot(l, low, high, compare);
    mboxQSortHelper(l, low, pivot->prev, 1, compare);
    mboxQSortHelper(l, pivot->next, high, 1, compare);
}

/* Quick sort the list */
void
mboxListQSort(mboxList *l, mboxListCmp *compare)
{
    pthread_mutex_lock(&l->lock);
    if (l->len <= 1 || compare == NULL) {
        return;
    }
    mboxQSortHelper(l, l->root, l->root->prev, 0, compare);
    pthread_mutex_unlock(&l->lock);
}

/* Print a list like an array */
void
mboxListPrint(mboxList *l, mboxListPrintCallback *print)
{
    mboxLNode *n = l->root;
    size_t count = l->len;

    printf("[");
    do {
        print(n->data);
        if (count - 1 != 0) {
            printf(", ");
        } else {
            printf(" ");
        }
        n = n->next;
        count--;
    } while (count);
    printf("]\n");
}

void
mboxListRelease(mboxList *l)
{
    if (l) {
        while (l->len > 0) {
            void *data = mboxListRemoveHead(l);
            if (data && l->freedata) {
                l->freedata(data);
            }
        }
        pthread_mutex_destroy(&l->lock);
        free(l);
    }
}

/* Plonk aux onto the tail of main O(1) fast and unsophisticated, just how I
 * like cars*/
mboxList *
mboxListAppendListTail(mboxList *main, mboxList *aux)
{
    if (aux->len == 0) {
        return main;
    }
    mboxLNode *aux_head = aux->root;

    if (main->len == 0) {
        main->root = aux_head;
    } else {
        mboxLNode *tail = main->root->prev;
        mboxLNode *head = main->root;
        mboxLNode *aux_tail = aux_head->prev;

        aux_head->prev = tail;
        tail->next = aux_head;

        head->prev = aux_tail;
        aux_tail->next = head;
    }

    main->len += aux->len;
    pthread_mutex_destroy(&aux->lock);
    aux->len = 0;
    free(aux);

    return main;
}
