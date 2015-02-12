#ifndef DURAQUEUE_H
#define DURAQUEUE_H

typedef struct
{
    /* postition of head and tail */
    unsigned int head, tail;

    /* the underlying buffer */
    void* data;

    /* maximum size of buffer */
    size_t size;

    /* the mmap'd file's file descriptor */
    int fd;

    /* monotonic id for queue items */
    unsigned int item_id;

    /* metadata for each item in queue
     * ie. position, length, etc */
    void* items;
} dqueue_t;

int dqueue_free(dqueue_t* me);

/**
 * Open a queue in the context of the reader
 *
 * @param[in] path The filename of the queue
 * @return 0 on success
 */
dqueue_t* dqueuer_open(const char* path);

/**
 * Open a queue in the context of the writer
 * Create queue if it doesn't exist
 *
 * @param[in] path The filename of the queue
 * @param max_size Maximum size of queue in bytes
 *                 Ignored if queue already exists
 * @return 0 on success
 */
dqueue_t* dqueuew_open(const char* path, size_t max_size);

/**
 * @return number of items on queue
 */
unsigned int dqueue_count(dqueue_t* me);

/**
 * @return 0 on successful write to disk
 */
int dqueue_offer(dqueue_t* me, const char* buf, size_t len);

/**
 * @return 0 on successful read from disk
 */
int dqueue_poll(dqueue_t* me);

/**
 * @return 0 on successful read from disk
 */
int dqueue_peek(dqueue_t* me, const char* path, char* data, size_t len);

/**
 * @return 1 when queue is empty; 0 otherwise
 */
int dqueue_is_empty(dqueue_t* me);

int dqueue_size(const dqueue_t *me);

/**
 * @return number of bytes used (includes metadata)
 */
int dqueue_usedspace(const dqueue_t *me);

/**
 * @return number of bytes unused (including space for metadata)
 */
int dqueue_unusedspace(const dqueue_t *me);

#endif /* DURAQUEUE_H */