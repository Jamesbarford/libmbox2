/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */

/* Code is pretty much verbatim from introduction to algorithms, the ordered
 * property of the tree is useful too us and as headers are at max about 30 the
 * speed difference between using this tree and a list for insertions was
 * negligable however for lookup speeds the redblack tree blew it out the
 * water, it's trival to swap this out for the 'mboxList' if you want to. */
#include <stdio.h>
#include <stdlib.h>

#include "mbox-buf.h"
#include "mbox-redblacktree.h"

static rbNode RB_SENTINAL[1];

mboxRBTree *
rbTreeNew(rbFreeKey *freekey, rbFreeValue *freevalue, rbCompareKey *keycmp)
{
    mboxRBTree *t = malloc(sizeof(mboxRBTree));
    t->keycmp = keycmp;
    t->freevalue = freevalue;
    t->freekey = freekey;
    t->root = RB_SENTINAL;
    t->size = 0;
    RB_SENTINAL->color = RB_BLACK;
    RB_SENTINAL->left = NULL;
    RB_SENTINAL->right = NULL;
    RB_SENTINAL->key = NULL;
    RB_SENTINAL->value = NULL;
    return t;
}

static rbNode *
rbNodeNew(void *key, void *value)
{
    rbNode *n = malloc(sizeof(rbNode));
    n->key = key;
    n->value = value;
    n->color = RB_RED;
    n->right = n->left = RB_SENTINAL;
    n->parent = NULL;
    return n;
}

static void
rbNodeRelease(mboxRBTree *t, rbNode *n)
{
    if (n && n != RB_SENTINAL) {
        if (t->freekey && n->key) {
            t->freekey(n->key);
        }

        if (t->freevalue && n->value) {
            t->freevalue(n->value);
        }

        free(n);
    }
}

static void
rbLeftRotate(mboxRBTree *t, rbNode *x)
{
    rbNode *y = x->right;
    x->right = y->left;
    if (y->left != RB_SENTINAL) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == NULL) {
        t->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

static void
rbRightRotate(mboxRBTree *t, rbNode *y)
{
    rbNode *x = y->left;
    y->left = x->right;
    if (x->right != RB_SENTINAL) {
        x->right->parent = y;
    }
    x->parent = y->parent;
    if (y->parent == NULL) {
        t->root = x;
    } else if (y == y->parent->right) {
        y->parent->right = x;
    } else {
        y->parent->left = x;
    }
    x->right = y;
    y->parent = x;
}

static void
rbInsertFixup(mboxRBTree *t, rbNode *z)
{
    while (z->parent->color == RB_RED) {
        rbNode *y;
        if (z->parent == z->parent->parent->left) {
            y = z->parent->parent->right;
            if (y->color == RB_RED) {
                y->color = RB_BLACK;
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rbLeftRotate(t, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rbRightRotate(t, z->parent->parent);
            }
        } else {
            y = z->parent->parent->left;
            if (y->color == RB_RED) {
                y->color = RB_BLACK;
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rbRightRotate(t, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rbLeftRotate(t, z->parent->parent);
            }
        }
        if (z == t->root) {
            break;
        }
    }
    t->root->color = RB_BLACK;
}

void
mboxRBTreeInsert(mboxRBTree *t, void *key, void *value)
{
    rbNode *x = t->root;
    rbNode *y = NULL;
    rbNode *z = NULL;
    int cmp = 0;

    while (x != RB_SENTINAL) {
        y = x;
        cmp = t->keycmp(key, x->key);

        if (cmp < 0) {
            x = x->left;
        } else if (cmp > 0) {
            x = x->right;
        } else {
            /* equal -> no dupes in this tree */
            return;
        }
    }

    z = rbNodeNew(key, value);
    z->parent = y;
    if (y == NULL) {
        t->root = z;
    } else if (t->keycmp(z->key, y->key) < 0) {
        y->left = z;
    } else {
        y->right = z;
    }
    t->size++;

    if (z->parent == NULL) {
        z->color = RB_BLACK;
        return;
    }

    if (z->parent->parent == NULL) {
        return;
    }

    rbInsertFixup(t, z);
}

static rbNode *
rbNodeFind(mboxRBTree *t, rbNode *n, void *key)
{
    rbNode *cur = n;
    int cmp = 0;

    while (cur && cur != RB_SENTINAL) {
        cmp = t->keycmp(key, cur->key);
        if (cmp == 0) {
            return cur;
        } else if (cmp < 0) {
            cur = cur->left;
        } else {
            cur = cur->right;
        }
    }

    return NULL;
}

void *
mboxRBTreeGet(mboxRBTree *t, void *key)
{
    rbNode *n = rbNodeFind(t, t->root, key);
    return n && n != RB_SENTINAL ? n->value : NULL;
}

int
mboxRBTreeHas(mboxRBTree *t, void *key)
{
    rbNode *n = rbNodeFind(t, t->root, key);
    return n != NULL;
}

static void
rbTransplant(mboxRBTree *t, rbNode *u, rbNode *v)
{
    if (u->parent == NULL) {
        t->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

static rbNode *
rbNodeMinimum(rbNode *n)
{
    while (n->left) {
        n = n->left;
    }
    return n;
}

static void
mboxRBTreeFixDelete(mboxRBTree *t, rbNode *x)
{
    rbNode *w = NULL;
    while (x != NULL && x != t->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            w = x->parent->right;
            if (w->color == RB_RED) {
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rbLeftRotate(t, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->right->color == RB_BLACK) {
                    w->left->color = RB_BLACK;
                    w->color = RB_RED;
                    rbRightRotate(t, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->right->color = RB_BLACK;
                rbLeftRotate(t, x->parent);
                x = t->root;
            }
        } else {
            w = x->parent->left;
            if (w->color == RB_RED) {
                w->color = RB_RED;
                x->parent->color = RB_BLACK;
                rbRightRotate(t, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == RB_BLACK && w->left->color == RB_BLACK) {
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->left->color == RB_BLACK) {
                    w->right->color = RB_BLACK;
                    w->color = RB_RED;
                    rbLeftRotate(t, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->left->color = RB_BLACK;
                rbRightRotate(t, x->parent);
                x = t->root;
            }
        }
    }
    x->color = RB_BLACK;
}

void
mboxRBTreeDelete(mboxRBTree *t, void *key)
{
    rbNode *x = NULL;
    rbNode *y = NULL;
    rbNode *z = NULL;
    rbNode *cur = t->root;
    int cmp = 0;
    int original_color = -1;

    while (cur) {
        cmp = t->keycmp(key, cur->key);
        if (cmp == 0) {
            z = cur;
        } else if (cmp > 0) {
            cur = cur->right;
        } else {
            cur = cur->left;
        }
    }

    if (z == NULL) {
        return;
    }

    y = z;
    original_color = y->color;

    if (z->left == NULL) {
        x = z->right;
        rbTransplant(t, z, z->right);
    } else if (z->right == NULL) {
        x = z->left;
        rbTransplant(t, z, z->left);
    } else {
        y = rbNodeMinimum(z->right);
        original_color = y->color;
        x = y->right;
        if (y != z->right) {
            rbTransplant(t, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        } else {
            x->parent = y;
        }
        rbTransplant(t, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    if (original_color == RB_BLACK) {
        mboxRBTreeFixDelete(t, x);
    }

    if (z) {
        rbNodeRelease(t, z);
    }

    t->size--;
}

static void
rbNodeFree(mboxRBTree *t, rbNode *n)
{
    if (n && n != RB_SENTINAL) {
        rbNodeFree(t, n->left);
        rbNodeFree(t, n->right);
        rbNodeRelease(t, n);
    }
}

void
mboxRBTreeClear(mboxRBTree *t)
{
    if (t && t->size != 0) {
        rbNodeFree(t, t->root);
        t->size = 0;
        t->root = RB_SENTINAL;
    }
}

void
mboxRBTreeRelease(mboxRBTree *t)
{
    if (t) {
        mboxRBTreeClear(t);
        free(t);
    }
}

void
printString(void *s)
{
    printf("%s", ((mboxBuf *)s)->data);
}

static void
mboxRBTreePrintHelper(rbNode *n, void (*print)(void *))
{
    if (n && n != RB_SENTINAL) {
        mboxRBTreePrintHelper(n->left, print);
        print(n->key);
        printf(" => ");
        print(n->value);
        printf("\n");
        mboxRBTreePrintHelper(n->right, print);
    }
}

static void
mboxRBTreePrintKeyHelper(rbNode *n, void (*print)(void *))
{
    if (n != RB_SENTINAL) {
        mboxRBTreePrintKeyHelper(n->left, print);
        print(n->key);
        printf("\n");
        mboxRBTreePrintKeyHelper(n->right, print);
    }
}

void
mboxRBTreePrintAsStrings(mboxRBTree *t)
{
    mboxRBTreePrintHelper(t->root, printString);
}

void
mboxRBTreePrintKeysAsString(mboxRBTree *t)
{
    mboxRBTreePrintKeyHelper(t->root, printString);
}

static void
rbForEachHelper(rbNode *n, rbCallback *cb, void *closure)
{
    if (n && n != RB_SENTINAL) {
        rbForEachHelper(n->left, cb, closure);
        cb(n->key, n->value, closure);
        rbForEachHelper(n->right, cb, closure);
    }
}

/* Iterate over all of the values in the tree, the closure generic can capture
 * any return value we may want to capture */
void
mboxRBTreeForEach(mboxRBTree *t, rbCallback *cb, void *closure)
{
    rbForEachHelper(t->root, cb, closure);
}
