
/**
 * Copyright (c) 2014, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "arrayqueue.h"

void aqueue_init(arrayqueue_t* me, size_t size, size_t m_size)
{
    me->m_size = m_size;
    me->size = size;
    me->count = 0;
    me->back = me->front = 0;
}

void* aqueue_array(arrayqueue_t* me)
{
    return (void*)me + sizeof(arrayqueue_t);
}

arrayqueue_t* aqueue_new(size_t size, size_t m_size)
{
    arrayqueue_t *me = malloc(aqueue_sizeof(size, m_size));

    if (!me)
        return NULL;

    aqueue_init(me, size, m_size);

    return me;
}

size_t aqueue_sizeof(size_t size, size_t m_size)
{
    return sizeof(arrayqueue_t) + size * m_size;
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

    arrayqueue_t *new = malloc(aqueue_sizeof(me->size * 2, me->m_size));

    size_t ii, jj;
    for (ii = 0, jj = me->front; ii < me->count; ii++, jj++)
    {
        if (jj == me->size)
            jj = 0;
        memcpy(aqueue_array(new) + ii * me->m_size, aqueue_array(me) + jj * me->m_size,
               me->m_size);
    }

    new->m_size = me->m_size;
    new->size = me->size * 2;
    new->front = 0;
    new->count = new->back = me->count;
    free(me);
    return new;
}

void *aqueue_peek(arrayqueue_t * me)
{
    if (aqueue_is_empty(me))
        return NULL;
    return aqueue_array(me) + me->front * me->m_size;
}

int aqueue_poll(arrayqueue_t * me)
{
    if (aqueue_is_empty(me))
        return -1;

    me->front++;
    if (me->size == me->front)
        me->front = 0;
    me->count--;
    return 0;
}

int aqueue_offerensure(arrayqueue_t **me_ptr, void *item)
{
    if (NULL == (*me_ptr = __ensurecapacity(*me_ptr)))
        return -1;

    arrayqueue_t* me = *me_ptr;

    memcpy(aqueue_array(me) + me->back * me->m_size, item, me->m_size);
    me->count++;
    me->back++;

    if (!aqueue_is_full(me) && me->size == me->back)
        me->back = 0;
    return 0;
}

int aqueue_offer(arrayqueue_t *me, void *item)
{
    if (aqueue_is_full(me))
        return -1;

    memcpy(aqueue_array(me) + me->back * me->m_size, item, me->m_size);
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
    return aqueue_array(me) + ((me->front + idx) % me->size) * me->m_size;
}

int aqueue_iter_has_next(arrayqueue_t* me, arrayqueue_iter_t* iter)
{
    if (iter->current == (int)me->back)
        return 0;
    return 1;
}

void *aqueue_iter_next(arrayqueue_t* me, arrayqueue_iter_t* iter)
{
    if (!aqueue_iter_has_next(me, iter))
        return NULL;

    if (iter->current == (int)me->size)
        iter->current = 0;

    void* item = aqueue_array(me) + iter->current * me->m_size;

    iter->current++;

    return item;
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

    void* val = aqueue_array(me) + iter->current * me->m_size;

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
