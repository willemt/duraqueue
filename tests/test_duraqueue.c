#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "CuTest.h"

#include "duraqueue.h"

void Testdqueue_new_is_empty(CuTest * tc)
{
    remove("tmp.queue");

    void *wqu = dqueuew_open("tmp.queue", 1 << 13);
    CuAssertTrue(tc, dqueue_is_empty(wqu));
    CuAssertTrue(tc, 0 == dqueue_usedspace(wqu));
    dqueue_free(wqu);
}

void Testdqueue_new_must_be_multiple_of_32(CuTest * tc)
{
    remove("tmp.queue");

    void *wqu = dqueuew_open("tmp.queue", 65);
    CuAssertTrue(tc, NULL == wqu);
}

void Testdqueue_is_not_empty_after_offer(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_offer(qu, "abcd", 4);
    CuAssertTrue(tc, !dqueue_is_empty(qu));
}

void Testdqueue_is_empty_after_poll(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_offer(qu, "abcd", 4);
    dqueue_poll(qu);
    CuAssertTrue(tc, dqueue_is_empty(qu));
}

void Testdqueue_spaceused_is_nonzero_after_offer(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_offer(qu, "abcd", 4);
    CuAssertTrue(tc, 0 != dqueue_usedspace(qu));
}

void Testdqueue_spaceused_is_zero_after_poll(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_offer(qu, "abcd", 4);
    dqueue_poll(qu);
    CuAssertTrue(tc, 0 == dqueue_usedspace(qu));
}

void Testdqueue_is_empty_after_2polls(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_offer(qu, "abcd", 4);
    dqueue_offer(qu, "abcd", 4);
    dqueue_poll(qu);
    CuAssertTrue(tc, !dqueue_is_empty(qu));
    dqueue_poll(qu);
    CuAssertTrue(tc, dqueue_is_empty(qu));
}

// FIXME
#if 0
void Txestdqueue_cant_poll_nonexistant(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_poll(qu);
}
#endif

void Testdqueue_offer_adds_new_item(CuTest * tc)
{
    remove("tmp.queue");

    char *item = "testitem";
    void *wqu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_offer(wqu, item, 8);
    CuAssertTrue(tc, 1 == dqueue_count(wqu));
    dqueue_free(wqu);
}

void Testdqueue_offer_adds_three_items(CuTest * tc)
{
    remove("tmp.queue");

    char **item = malloc(sizeof(char*) * 3);
    item[0] = strdup("testitem");
    item[1] = strdup("anotheritem");
    item[2] = strdup("anotheritem");
    void *wqu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_offer(wqu, item[0], strlen(item[0]));
    dqueue_offer(wqu, item[1], strlen(item[1]));
    dqueue_offer(wqu, item[2], strlen(item[2]));
    CuAssertTrue(tc, 3 == dqueue_count(wqu));

    void *rqu = dqueuer_open("tmp.queue");
    CuAssertTrue(tc, NULL != rqu);
    CuAssertTrue(tc, 3 == dqueue_count(rqu));
    dqueue_free(wqu);
}

void Testdqueue_writer_can_reopen(CuTest * tc)
{
    remove("tmp.queue");

    void *wqu;
    char **item = malloc(sizeof(char*) * 3);
    item[0] = strdup("testitem");
    item[1] = strdup("anotheritem");
    item[2] = strdup("anotheritem");

    wqu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_offer(wqu, item[0], strlen(item[0]));
    dqueue_offer(wqu, item[1], strlen(item[1]));
    CuAssertTrue(tc, 2 == dqueue_count(wqu));
    dqueue_free(wqu);

    wqu = dqueuew_open("tmp.queue", 1 << 13);
    CuAssertTrue(tc, NULL != wqu);
    dqueue_offer(wqu, item[2], strlen(item[2]));
    CuAssertTrue(tc, 3 == dqueue_count(wqu));
    dqueue_free(wqu);
}

void Testdqueue_cannot_offer_over_capacity(CuTest * tc)
{
    remove("tmp.queue");

    char *item = malloc(2048);
    void *wqu = dqueuew_open("tmp.queue", 1 << 13);
    int i;

    for (i = 0; i < 2048; i++)
        item[i] = 'c';

    CuAssertTrue(tc, -1 == dqueue_offer(wqu, item, 1 << 13));
    CuAssertTrue(tc, 0 == dqueue_count(wqu));
    dqueue_free(wqu);
}

void Testdqueue_cant_offer_if_not_enough_space_full(CuTest * tc)
{
    remove("tmp.queue");
    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    CuAssertTrue(tc, -1 == dqueue_offer(qu, "1000", 1 << 17));
}

void Testdqueue_can_offer_over_boundary(CuTest * tc)
{
    remove("tmp.queue");

    char *item = malloc(4096);
    char *item2 = malloc(6000);
    void *wqu = dqueuew_open("tmp.queue", 1 << 13);
    int i;

    for (i = 0; i < 4096; i++)
        item[i] = 'a';

    for (i = 0; i < 6000; i++)
        item2[i] = 'b';

    CuAssertTrue(tc, 0 == dqueue_offer(wqu, item, 4096));
    CuAssertTrue(tc, 0 == dqueue_poll(wqu));
    CuAssertTrue(tc, 0 == dqueue_offer(wqu, item2, 6000));
    CuAssertTrue(tc, 1 == dqueue_count(wqu));
    dqueue_free(wqu);
}

