/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "mbox-buf.h"
#include "mbox-date.h"
#include "mbox-logger.h"
#include "mbox-redblacktree.h"

#define date_fmt_1 "%a, %d %b %Y %H:%M:%S %z"
/* We will use this format in the emails and remove the %c%c%c,<space> for
 * the day, we do not need that for a unix timestamp */
#define date_fmt_2 "%d %b %Y %H:%M:%S %z"
/* Format of the date in a from line */
#define date_fmt_3 "%a %b %d %H:%M:%S %z %Y"

void
printDateStruct(struct mboxDate *d)
{
    printf("wd:%d md:%d M:%d Y:%d %d:%d:%d", d->tm_wday, d->tm_mday, d->tm_mon,
            d->tm_year, d->tm_hour, d->tm_min, d->tm_sec);
    if (d->tm_zone_diff) {
        printf(" %d", d->tm_zone_diff);
    }
    printf("\n");
}

typedef struct dateTest {
    char *date;
    char *fmt;
    unsigned long stamp;
} dateTest;

dateTest dateTests[] = {
    { "Mon, 27 Feb 2023 07:30:00 +0000 (UTC)", date_fmt_1, 1677483000000 },
    { "Mon, 27 Feb 2023 14:37:33 -0800", date_fmt_1, 1677537453000 },
    { "Fri, 25 Feb 2023 05:30:47 -0500 (EST)", date_fmt_1, 1677321047000 },
    { "Mon, 27 Feb 2023 19:36:54 -0600 (CST)", date_fmt_1, 1677548214000 },
    { "Mon, 27 Feb 2023 19:36:54", date_fmt_1, 1677526614000 },
    { "27 Feb 2023 19:36:54", date_fmt_2, 1677526614000 },
    { "Fri Feb 24 15:13:20 +0000 2023", date_fmt_3, 1677251600000 },
};

void
dateTestSuite(void)
{
    struct mboxDate d;
    int passed = 0;
    int total = (sizeof(dateTests) / sizeof(dateTests[0]));

    for (int i = 0; i < total; ++i) {
        dateTest t = dateTests[i];
        int ok = mboxDateStringToStruct(t.date, t.fmt, &d);
        if (!ok) {
            printf("[%d] Failed to parse: ", i);
            printf("%s %s %lu\n", t.date, t.fmt, t.stamp);
            printDateStruct(&d);
        }
        passed += ok;
        unsigned long a1 = mboxDateStructToUnix(&d) * 1000;
        if (a1 != t.stamp) {
            printf("[%d]Expected %lu to equal: %lu\n", i, a1, t.stamp);
            exit(1);
        } else {
            passed++;
        }
    }

    printf("DATE TEST SUITE: dateStringToStruct & dateStructToUnix -- "
           "passed:%d of:%d\n",
            passed, total * 2);
    if (passed != total * 2) {
        printf("DATE TEST SUITE: FAILED\n");
        exit(1);
    }
}

typedef struct mboxBufContainTest {
    char *string;
    char *pattern;
    int should_match;
} mboxBufContainTest;

mboxBufContainTest strContains[] = {
    { "--68760429edd956d2400d1396f4e6c6371100c4ae3610a9940d6b2ac0ec37--",
            "68760429edd956d2400d1396f4e6c6371100c4ae3610a9940d6b2ac0ec37", 1 },
    { "--cee6855870433e9118d4d70cc3592d5e37d74eb38f5fed35863c2038a349--",
            "68760429edd956d2400d1396f4e6c6371100c4ae3610a9940d6b2ac0ec37", 0 },
    { "--cee6855870433e9118d4d70cc3592d5e37d74eb38f5fed35863c2038a349--",
            "--"
            "68760429edd956d2400d1396f4e6c6371100c4ae3610a9940d6b2ac0ec37-"
            "-",
            0 },
    { "Hello world", "Hello", 1 }

};

static void
mboxBufTestSuite(void)
{
    int total = static_sizeof(strContains);
    int passed = 0;

    for (int i = 0; i < total; ++i) {
        int a = -1;
        mboxBufContainTest *t = &strContains[i];
        unsigned int len = strlen(t->string);
        unsigned int pattern_len = strlen(t->pattern);

        mboxBuf *buf = mboxBufAlloc(len + 10);
        mboxBufCatLen(buf, t->string, len);

        a = mboxBufContainsPattern(buf, (mboxChar *)t->pattern, pattern_len);
        if (t->should_match) {
            if (a >= 0) {
                passed++;
            } else {
                printf("%s => %s\n", buf->data, t->pattern);
            }
        } else {
            if (a < 0) {
                passed++;
            } else {
                printf("%s => %s\n", buf->data, t->pattern);
            }
        }

        mboxBufRelease(buf);
    }

    printf("MOBX BUF TEST SUITE: mboxBufContainsPattern --  passed:%d of:%d\n",
            passed, total);
}

static void
mboxBufTestWrite(void)
{
    mboxBuf *s = mboxBufAlloc(100);
    mboxChar *w = (mboxChar *)"hello world!";
    mboxChar *w2 = (mboxChar *)"good bye";
    unsigned int wlen = strlen((char *)w);
    unsigned int w2len = strlen((char *)w2);
    int passed = 0;
    int total = 2;

    mboxBufWrite(s, w, wlen);
    if (strncmp((char *)s->data, "hello world!", wlen) == 0 && s->len == wlen) {
        passed++;
    } else {
        printf("expected: %s to equal: %s\n", s->data, w);
        printf("%zu %u\n", s->len, wlen);
    }

    mboxBufWrite(s, w2, w2len);
    if (strncmp((char *)s->data, "good bye", w2len) == 0 && s->len == w2len) {
        passed++;
    } else {
        printf("expected: %s to equal: %s\n", s->data, w);
        printf("%zu %u\n", s->len, wlen);
    }
    mboxBufRelease(s);
    printf("MBOX BUF TEST SUITE: mboxBufWrite --  passed:%d of:%d\n", passed,
            total);
}

int
main(void)
{
    dateTestSuite();
    mboxBufTestSuite();
    mboxBufTestWrite();
}
