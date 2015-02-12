
/**
 * Copyright (c) 2014, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "arrayqueue.h"

void aqueue_init(arrayqueue_t* me, unsigned int size)
{
    me->size = size;
    me->count = 0;
    me->back = me->front = 0;
}

arrayqueue_t* aqueue_new(unsigned int size)
{
    arrayqueue_t *me = malloc(aqueue_sizeof(size));

    if (!me)
        return NULL;

    aqueue_init(me, size);

    return me;
}

size_t aqueue_sizeof(unsigned int size)
{
    return sizeof(arrayqueue_t) + size * sizeof(void *);
}

int aqueue_is_empty(const arrayqueue_t * me)
{
    return 0 == me->count;
}

int aqueue_is_full(arrayqueue_t * me)
{
    return me->size == me->count;
}

static arrayqueue_t* __ensurecapacity(arrayqueue_t * me)
{
    if (me->count < me->size)
        return me;

    arrayqueue_t *new = malloc(aqueue_sizeof(me->size * 2));

    int ii, jj;
    for (ii = 0, jj = me->front; ii < me->count; ii++, jj++)
    {
        if (jj == me->size)
            jj = 0;
        new->array[ii] = me->array[jj];
    }

    new->size = me->size * 2;
    new->front = 0;
    new->count = new->back = me->count;
    free(me);
    return new;
}

void *aqueue_peek(const arrayqueue_t * me)
{
    if (aqueue_is_empty(me))
        return NULL;
    return ((void**)me->array)[me->front];
}

void *aqueue_peektail(const arrayqueue_t * me)
{
    if (aqueue_is_empty(me))
        return NULL;

    int pos = me->back - 1;

    /* don't forget about wrapping */
    if (pos < 0)
        pos = me->size - 1;

    return ((void**)me->array)[pos];
}

void *aqueue_poll(arrayqueue_t * me)
{
    if (aqueue_is_empty(me))
        return NULL;

    void *elem = me->array[me->front];

    me->front++;
    if (me->size == me->front)
        me->front = 0;
    me->count--;

    return elem;
}

void *aqueue_polltail(arrayqueue_t * me)
{
    if (aqueue_is_empty(me))
        return NULL;

    me->back--;
    me->count--;
    if (-1 == me->back)
        me->back = me->size;

    return me->array[me->back];
}

int aqueue_offerensure(arrayqueue_t **me, void *item)
{
    if (NULL == (*me = __ensurecapacity(*me)))
        return -1;

    ((const void**)(*me)->array)[(*me)->back] = item;
    (*me)->count++;
    (*me)->back++;

    if (!aqueue_is_full(*me) && (*me)->size == (*me)->back)
        (*me)->back = 0;
    return 0;
}

int aqueue_offer(arrayqueue_t *me, void *item)
{
    if (aqueue_is_full(me))
        return -1;

    ((const void**)me->array)[me->back] = item;
    me->count++;
    me->back++;

    if (me->size == me->back)
        me->back = 0;
    return 0;
}

void aqueue_empty(arrayqueue_t * me)
{
    me->front = me->back = me->count = 0;
}

void aqueue_free(arrayqueue_t * me)
{
    free(me);
}

int aqueue_count(const arrayqueue_t * me)
{
    return me->count;
}

int aqueue_size(const arrayqueue_t * me)
{
    return me->count;
}

void* aqueue_get_from_idx(arrayqueue_t * me, int idx)
{
    return me->array[(me->front + idx) % me->size];
}

int aqueue_iter_has_next(arrayqueue_t* me, arrayqueue_iter_t* iter)
{
    if (iter->current == me->back)
        return 0;
    return 1;
}

void *aqueue_iter_next(arrayqueue_t* me, arrayqueue_iter_t* iter)
{
    if (!aqueue_iter_has_next(me, iter))
        return NULL;

    if (iter->current == me->size)
        iter->current = 0;

    return (void*)me->array[iter->current++];
}

int aqueue_iter_has_next_reverse(arrayqueue_t* me,
                                 arrayqueue_iter_t* iter)
{
    int end = me->front - 1;

    if (end < 0)
        end = me->size - 1;

    if (iter->current == end)
        return 0;

    return 1;
}

void *aqueue_iter_next_reverse(arrayqueue_t* me,
                               arrayqueue_iter_t* iter)
{
    if (!aqueue_iter_has_next_reverse(me, iter))
        return NULL;

    void *val = (void*)me->array[iter->current];

    iter->current--;
    if (iter->current < 0)
        iter->current = me->size - 1;
    return val;
}

void aqueue_iter_reverse(arrayqueue_t* me, arrayqueue_iter_t* iter)
{
    iter->current = me->back - 1;
    if (iter->current < 0)
        iter->current = me->size - 1;
}

void aqueue_iter(arrayqueue_t * me, arrayqueue_iter_t * iter)
{
    iter->current = me->front;
}
