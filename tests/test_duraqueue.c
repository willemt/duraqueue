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
    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, "abcd", 4);
    dqueue_txn_commit(&txn);
    CuAssertTrue(tc, !dqueue_is_empty(qu));
}

void Testdqueue_is_empty_after_poll(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, "abcd", 4);
    dqueue_txn_commit(&txn);
    dqueue_poll(qu);
    CuAssertTrue(tc, dqueue_is_empty(qu));
}

void Testdqueue_spaceused_is_nonzero_after_offer(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, "abcd", 4);
    dqueue_txn_commit(&txn);
    CuAssertTrue(tc, 0 != dqueue_usedspace(qu));
}

void Testdqueue_spaceused_is_zero_after_poll(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);
    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, "abcd", 4);
    dqueue_txn_commit(&txn);
    dqueue_poll(qu);
    CuAssertTrue(tc, 0 == dqueue_usedspace(qu));
}

void Testdqueue_is_empty_after_2polls(CuTest * tc)
{
    remove("tmp.queue");

    void *qu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, "abcd", 4);
    dqueue_offer(&txn, "abcd", 4);
    dqueue_txn_commit(&txn);

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

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item, 8);
    dqueue_txn_commit(&txn);

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

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[0], strlen(item[0]));
    dqueue_offer(&txn, item[1], strlen(item[1]));
    dqueue_offer(&txn, item[2], strlen(item[2]));
    dqueue_txn_commit(&txn);

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
    item[2] = strdup("1234567890");

    wqu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[0], strlen(item[0]));
    dqueue_offer(&txn, item[1], strlen(item[1]));
    dqueue_txn_commit(&txn);
    CuAssertTrue(tc, 2 == dqueue_count(wqu));
    dqueue_free(wqu);

    wqu = dqueuew_open("tmp.queue", 1 << 13);
    CuAssertTrue(tc, NULL != wqu);

    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[2], strlen(item[2]));
    dqueue_txn_commit(&txn);
    CuAssertTrue(tc, 3 == dqueue_count(wqu));
    dqueue_free(wqu);
}

void Testdqueue_reader_can_reopen(CuTest * tc)
{
    remove("tmp.queue");

    void *wqu;
    char **item = malloc(sizeof(char*) * 3);
    item[0] = strdup("testitem");
    item[1] = strdup("anotheritem");
    item[2] = strdup("1234567890");

    wqu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[0], strlen(item[0]));
    dqueue_offer(&txn, item[1], strlen(item[1]));
    dqueue_offer(&txn, item[2], strlen(item[2]));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 3 == dqueue_count(wqu));
    dqueue_free(wqu);

    void *qu  = dqueuer_open("tmp.queue");
    CuAssertTrue(tc, NULL != wqu);

    size_t len;
    char *data;

    /* item 1 is ok */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item[0], data, len));
    CuAssertTrue(tc, strlen(item[0]) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    /* item 2 is ok */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item[1], data, len));
    CuAssertTrue(tc, strlen(item[1]) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    /* item 3 is ok */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item[2], data, len));
    CuAssertTrue(tc, strlen(item[2]) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    dqueue_free(qu);
}

void Testdqueue_reader_can_reopen2(CuTest * tc)
{
    remove("tmp.queue");

    void *wqu;
    char **item = malloc(sizeof(char*) * 3);
    item[0] = strdup("testitem_____");
    item[1] = strdup("anotheritem__");
    item[2] = strdup("1234567890___");

    wqu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[0], strlen(item[0]));
    dqueue_offer(&txn, item[1], strlen(item[1]));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 0 == dqueue_poll(wqu));

    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[2], strlen(item[2]));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 2 == dqueue_count(wqu));
    dqueue_free(wqu);

    void *qu  = dqueuer_open("tmp.queue");
    CuAssertTrue(tc, NULL != wqu);

    size_t len;
    char *data;

    /* item 2 is ok */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item[1], data, len));
    CuAssertTrue(tc, strlen(item[1]) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    /* item 3 is ok */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item[2], data, len));
    CuAssertTrue(tc, strlen(item[2]) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    dqueue_free(qu);
}

void Testdqueue_reader_can_reopen3(CuTest * tc)
{
    remove("tmp.queue");

    void *wqu;
    char **item = malloc(sizeof(char*) * 3);
    item[0] = strdup("testitem_____");
    item[1] = strdup("anotheritem__");
    item[2] = strdup("1234567890___");

    wqu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[0], strlen(item[0]));
    dqueue_offer(&txn, item[1], strlen(item[1]));
    dqueue_txn_commit(&txn);

    /* reopen here */
    wqu = dqueuew_open("tmp.queue", 1 << 13);
    CuAssertTrue(tc, 0 == dqueue_poll(wqu));

    dqueue_txn_begin(wqu, &txn);
    dqueue_offer(&txn, item[2], strlen(item[2]));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 2 == dqueue_count(wqu));
    dqueue_free(wqu);

    void *qu  = dqueuer_open("tmp.queue");
    CuAssertTrue(tc, NULL != wqu);

    size_t len;
    char *data;

    /* item 2 is ok */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item[1], data, len));
    CuAssertTrue(tc, strlen(item[1]) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    /* item 3 is ok */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item[2], data, len));
    CuAssertTrue(tc, strlen(item[2]) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    dqueue_free(qu);
}

void Testdqueue_cannot_offer_over_capacity(CuTest * tc)
{
    remove("tmp.queue");

    char *item = malloc(2048);
    void *wqu = dqueuew_open("tmp.queue", 1 << 13);
    int i;

    for (i = 0; i < 2048; i++)
        item[i] = 'c';

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    CuAssertTrue(tc, -1 == dqueue_offer(&txn, item, 1 << 13));
    dqueue_txn_commit(&txn);
    CuAssertTrue(tc, 0 == dqueue_count(wqu));
    dqueue_free(wqu);
}

void Testdqueue_cant_offer_if_not_enough_space_full(CuTest * tc)
{
    remove("tmp.queue");
    void *qu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    CuAssertTrue(tc, -1 == dqueue_offer(&txn, "1000", 1 << 17));
    dqueue_txn_commit(&txn);
    dqueue_free(qu);
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

    dqueue_txn_t txn;
    dqueue_txn_begin(wqu, &txn);
    CuAssertTrue(tc, 0 == dqueue_offer(&txn, item, 4096));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 0 == dqueue_poll(wqu));

    dqueue_txn_begin(wqu, &txn);
    CuAssertTrue(tc, 0 == dqueue_offer(&txn, item2, 6000));
    dqueue_txn_commit(&txn);
    CuAssertTrue(tc, 1 == dqueue_count(wqu));
    dqueue_free(wqu);
}

void Testdqueue_peek_gets_head(CuTest * tc)
{
    remove("tmp.queue");

    char *item = "testitem";
    char *item2 = "testitem2";
    void *qu = dqueuew_open("tmp.queue", 1 << 13);

    size_t len;
    char *data;

    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, item, strlen(item));
    dqueue_offer(&txn, item2, strlen(item2));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item, data, len));
    CuAssertTrue(tc, strlen(item) == len);
    dqueue_free(qu);
}

void Testdqueue_cant_poll_with_no_contents(CuTest * tc)
{
    remove("tmp.queue");

    char *item = "testitem";
    void *qu = dqueuew_open("tmp.queue", 1 << 13);

    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, item, strlen(item));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 0 == dqueue_poll(qu));
    CuAssertTrue(tc, 0 == dqueue_count(qu));
    dqueue_free(qu);
}

void Testdqueue_offer_and_poll_item(CuTest * tc)
{
    remove("tmp.queue");

    char *item = "testitem";
    void *qu = dqueuew_open("tmp.queue", 1 << 13);

    size_t len;
    char *data;

    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, item, strlen(item));
    dqueue_txn_commit(&txn);

    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item, data, len));
    CuAssertTrue(tc, strlen(item) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));
    dqueue_free(qu);
}

void Testdqueue_fifo(CuTest * tc)
{
    remove("tmp.queue");

    char *item = "testitem", *item2 = "testitem2";
    void *qu = dqueuew_open("tmp.queue", 1 << 13);

    char* data;
    size_t len;

    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, item, strlen(item));
    dqueue_offer(&txn, item2, strlen(item2));
    dqueue_txn_commit(&txn);

    /* item 1 */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item, data, len));
    CuAssertTrue(tc, strlen(item) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    /* item 2 */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item2, data, len));
    CuAssertTrue(tc, strlen(item2) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    dqueue_free(qu);
}

void Testdqueue_poll_offer_past_boundary(CuTest * tc)
{
    remove("tmp.queue");

    char *item1 = malloc(5000);
    char *item2 = malloc(3000);
    char *item3 = malloc(3000);
    void *qu = dqueuew_open("tmp.queue", 1 << 13);

    int i;

    for (i = 0; i < 5000; i++)
        item1[i] = 'a';
    for (i = 0; i < 3000; i++)
        item2[i] = 'b';
    for (i = 0; i < 3000; i++)
        item3[i] = 'c';

    char* data;
    size_t len;

    dqueue_txn_t txn;
    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, item1, 5000);
    dqueue_offer(&txn, item2, 3000);
    dqueue_txn_commit(&txn);

    /* poll item 1 */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item1, data, len));
    CuAssertTrue(tc, strlen(item1) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    /* peek item 2 */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item2, data, len));
    CuAssertTrue(tc, strlen(item2) == len);

    dqueue_txn_begin(qu, &txn);
    dqueue_offer(&txn, item3, 3000);
    dqueue_txn_commit(&txn);

    /* poll item 2 */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item2, data, len));
    CuAssertTrue(tc, strlen(item2) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    /* poll item 3 */
    CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
    CuAssertTrue(tc, 0 == strncmp(item3, data, len));
    CuAssertTrue(tc, strlen(item3) == len);
    CuAssertTrue(tc, 0 == dqueue_poll(qu));

    dqueue_free(qu);
}

void Testdqueue_offer_1024_items(CuTest * tc)
{
    remove("tmp.queue");

    char *items = malloc(1024);
    void *qu = dqueuew_open("tmp.queue", 1 << 17);
    unsigned int i;
    dqueue_txn_t txn;

    /* offer 1024 items */
    for (i = 0; i < 1024; i++)
    {
        items[i] = i % 255;
        dqueue_txn_begin(qu, &txn);
        CuAssertTrue(tc, 0 == dqueue_offer(&txn, &items[i], 1));
        dqueue_txn_commit(&txn);
    }
    CuAssertTrue(tc, 1024 == dqueue_count(qu));

    /* ensure we can read all 1024 items */
    for (i = 0; i < 1024; i++)
    {
        char *data;
        unsigned long len;
        CuAssertTrue(tc, 0 == dqueue_peek(qu, &data, &len));
        CuAssertTrue(tc, 1 == len);
        /* printf("%d %d\n", i % 255, (unsigned char)data[0]); */
        CuAssertTrue(tc, i % 255 == (unsigned char)data[0]);
        CuAssertTrue(tc, 0 == dqueue_poll(qu));
    }

    dqueue_free(qu);
}
