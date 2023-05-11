/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __REDBLACK_H
#define __REDBLACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
/* -int less than 0 equal +int greater than */
typedef int rbCompareKey(void *, void *);
typedef void rbFreeKey(void *);
typedef void rbFreeValue(void *);
typedef void rbCallback(void *key, void *value, void *closure);

enum RbColor {
    RB_RED,
    RB_BLACK,
};

typedef struct rbNode {
    void *key;
    void *value;
    enum RbColor color;
    struct rbNode *left;
    struct rbNode *right;
    struct rbNode *parent;
} rbNode;

typedef struct mboxRBTree {
    rbNode *root;
    size_t size;
    rbFreeKey *freekey;
    rbFreeValue *freevalue;
    rbCompareKey *keycmp;
} mboxRBTree;

mboxRBTree *rbTreeNew(rbFreeKey *freekey, rbFreeValue *freevalue,
        rbCompareKey *keycmp);
void mboxRBTreeInsert(mboxRBTree *t, void *key, void *value);
void mboxRBTreeDelete(mboxRBTree *t, void *key);
void mboxRBTreeClear(mboxRBTree *t);
void mboxRBTreeRelease(mboxRBTree *t);
int mboxRBTreeHas(mboxRBTree *t, void *);
void *mboxRBTreeGet(mboxRBTree *t, void *);
void mboxRBTreePrintAsStrings(mboxRBTree *t);
void mboxRBTreePrintKeysAsString(mboxRBTree *t);
void mboxRBTreeForEach(mboxRBTree *t, rbCallback *cb, void *closure);

#ifdef __cplusplus
}
#endif

#endif
