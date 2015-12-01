#ifndef ARRAYqueue_H
#define ARRAYqueue_H

typedef struct
{
    size_t m_size;      /* size of member */
    size_t size;        /* size of array */
    size_t count;       /* the amount of items in the array */
    size_t front, back; /* position of the queue */
} arrayqueue_t;

typedef struct
{
    int current;
} arrayqueue_iter_t;

/**
 * Create a new data structure and initialise it
 *
 * malloc()s space
 *
 * @param[in] size Initial size of queue
 * @return initialised queue */
arrayqueue_t* aqueue_new(size_t size, size_t m_size);

/**
 * Create a new data structure and initialise it.
 *
 * No malloc()s are performed.
 *
 * @param[in] size Initial size of queue
 * @return initialised queue */
void aqueue_init(arrayqueue_t* qu, size_t size, size_t m_size);

/**
 * @return number of bytes needed for a queue of this size. */
size_t aqueue_sizeof(size_t size, size_t m_size);

/**
 * Is the queue empty?
 *
 * @return 1 if empty; otherwise 0 */
int aqueue_is_empty(const arrayqueue_t * qu);

/**
 * @return oldest item in this queue. */
void *aqueue_peek(arrayqueue_t * qu);

/**
 * Remove oldest item from queue.
 *
 * @return 0 on sucess; -1 on failure */
int aqueue_poll(arrayqueue_t * qu);

/**
 * Add item
 *
 * Ensures that the queue can hold the item.
 *
 * NOTE:
 *  malloc() possibly called.
 *  The queue pointer will be changed if the queu needs to be enlarged.
 *
 * @param[in/out] qu_ptr Pointer to the queue. Changed when queue is enlarged.
 * @param[in] item The item to be added
 * @return 0 on success; -1 on failure */
int aqueue_offerensure(arrayqueue_t ** qu_ptr, void *item);

/**
 * Add item
 *
 * An error will occur if there isn't enough space for this item.
 *
 * NOTE:
 *  no malloc()s called.
 *
 * @param[in] item The item to be added
 * @return 0 on success; -1 on error */
int aqueue_offer(arrayqueue_t * qu, void *item);

/**
 * Empty the queue */
void aqueue_empty(arrayqueue_t * qu);

void aqueue_free(arrayqueue_t * qu);

/**
 * @return number of items */
int aqueue_count(const arrayqueue_t * qu);

int aqueue_size(const arrayqueue_t * qu);

int aqueue_iter_has_next(arrayqueue_t* qu, arrayqueue_iter_t* iter);

void *aqueue_iter_next(arrayqueue_t* qu, arrayqueue_iter_t* iter);

int aqueue_iter_has_next_reverse(arrayqueue_t* qu, arrayqueue_iter_t* iter);

void *aqueue_iter_next_reverse(arrayqueue_t* qu, arrayqueue_iter_t* iter);

void aqueue_iter_reverse(arrayqueue_t* qu, arrayqueue_iter_t* iter);

void aqueue_iter(arrayqueue_t * qu, arrayqueue_iter_t * iter);

#endif /* ARRAYqueue_H */
