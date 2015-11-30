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

typedef struct {
    void* start;
    void* end;

    /* how many items we've written */
    int n_items;

    dqueue_t *dq;
} dqueue_txn_t;

int dqueue_free(dqueue_t* me);

/**
 * Open a queue in the context of the reader (aka poller)
 *
 * WARNING: only one reader is allowed on a queue at anytime.
 *
 * @param[in] path The filename of the queue
 * @return 0 on success
 */
dqueue_t* dqueuer_open(const char* path);

/**
 * Open a queue in the context of the writer (aka offerer)
 *
 * Create queue if it doesn't exist
 *
 * WARNING: only one writer is allowed on a queue at anytime.
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
int dqueue_offer(dqueue_txn_t* txn, const char* buf, size_t len);

/**
 * @return 0 on successful read from disk
 */
int dqueue_poll(dqueue_t* me);

/**
 * @return 0 on successful read from disk
 */
int dqueue_peek(dqueue_t* me, char** data, size_t* len);

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

/**
 * Start a transaction for offering items to the queue.
 *
 * A Duraqueue transaction is more like a "batch" rather than a database
 * transaction. This is because Duraqueue transactions can not be aborted.
 * Transactions are purely available to commit changes to disk in a single
 * commit. This has massive performance benefits if you are offering multiple
 * items to the queue. Changes to the queue's file on disk COULD happen ANYTIME
 * once the dqueue_offer call is made.
 *
 * @param txn The transaction to be initialized
 * @return 0 on success
 */
int dqueue_txn_begin(dqueue_t* me, dqueue_txn_t* txn);

/**
 * Commit all offers to the queue.
 * Fsync the changes to disk
 * @param txn The transaction
 * @return 0 on success
 */
int dqueue_txn_commit(dqueue_txn_t* txn);

#endif /* DURAQUEUE_H */
