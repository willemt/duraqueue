#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <assert.h>

/* for ntohl */
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "duraqueue.h"
#include "arrayqueue.h"
#include "snap2pagesize.h"

#define STAMP "xxxxxxxxxxxxxxxx"
#define HEADER 0
#define FOOTER 1

typedef struct
{
    char stamp[16];           /* 16 */
    unsigned int type;        /* 4 */
    unsigned int id;          /* 4 */
    unsigned int len;         /* 4 */
    unsigned int padding1;    /* 4 */
} header_t;

typedef struct
{
    unsigned int pos;
    unsigned int len;
    unsigned int space_used;
    unsigned int id;
} item_t;

#define ITEM_METADATA_SIZE sizeof(header_t) + sizeof(header_t)

static size_t __padding_required(size_t len)
{
    return sizeof(header_t) - (len % sizeof(header_t));
}

int dqueue_free(dqueue_t* me)
{
    free(me);
    return 0;
}

static unsigned int __get_maxsize(int fd)
{
    unsigned int max_size = lseek(fd, 0, SEEK_END);
    /* make sure its a multiple */
    max_size -= max_size % sizeof(header_t);
    lseek(fd, 0, SEEK_SET);
    return max_size;
}

static int __load(dqueue_t* me)
{
    unsigned int pos = 0;

    while (pos < me->size)
    {
        int e, start_pos;
        header_t h;

        start_pos = pos;

        /* 1. read first header */
        e = read(me->fd, &h, sizeof(header_t));
        if (e < (int)sizeof(header_t))
            return -1;
        pos += sizeof(header_t);

        /* check header is ok */
        if (h.type != HEADER || 0 != memcmp(h.stamp, STAMP, strlen(STAMP)))
            continue;

        unsigned int check_id = h.id;

        /* 2. read 2nd header */
        /* put on sizeof(header_t) offset */
        size_t offset = sizeof(header_t) - ntohl(h.len) % sizeof(header_t);
        e = lseek(me->fd, ntohl(h.len) + offset, SEEK_CUR);
        pos += ntohl(h.len);

        e = read(me->fd, &h, sizeof(header_t));
        if (e < (int)sizeof(header_t))
        {
            perror("couldn't read file\n");
            return -1;
        }

        pos += __padding_required(ntohl(h.len));
        pos += sizeof(header_t);

        if (h.id != check_id || ntohl(h.type) != FOOTER || 0 !=
            memcmp(h.stamp, STAMP, strlen(STAMP)))
            continue;

        /* found a valid queue item */

        item_t item;
        item.pos = start_pos;
        item.len = ntohl(h.len);
        item.space_used = ntohl(h.len) + ITEM_METADATA_SIZE +
                           __padding_required(ntohl(h.len));
        item.id = h.id;
        aqueue_offerensure((void*)&me->items, &item);

        h.id = ntohl(h.id);
    }

    if (0 == aqueue_count(me->items))
        return 0;

    /* get lowest */
    unsigned int lowest_id = UINT_MAX;
    unsigned int highest_id = 0;
    arrayqueue_iter_t iter;
    for (aqueue_iter(me->items, &iter);
         aqueue_iter_has_next(me->items, &iter); )
    {
        item_t* item = aqueue_iter_next(me->items, &iter);
        if (item->id < lowest_id)
        {
            lowest_id = item->id;
            me->head = item->pos;
        }

        /* set tail */
        if (highest_id < item->id)
        {
            highest_id = item->id;
            me->tail = item->pos + item->len + ITEM_METADATA_SIZE +
                       __padding_required(item->len);
            me->item_id = item->id + 1;
        }
    }

    arrayqueue_t* stowaway = aqueue_new(16, sizeof(item_t));

    /* put lowest at front of queue */
    while (!aqueue_is_empty(me->items))
    {
        item_t* item = aqueue_peek(me->items);
        if (item->id == lowest_id)
            break;
        aqueue_offerensure(&stowaway, aqueue_peek(me->items));
        aqueue_poll(me->items);
    }

    /* empty out stowaway */
    while (!aqueue_is_empty(stowaway))
    {
        aqueue_offerensure((void*)&me->items, aqueue_peek(stowaway));
        aqueue_poll(stowaway);
    }

    aqueue_free(stowaway);

    return 0;
}

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

static void __create_buffer_mirror(int fd, char* data, unsigned int size)
{
    char* r = mmap(data + size, size, PROT_READ | PROT_WRITE,
                   MAP_FIXED | MAP_SHARED, fd, 0);
    if (r != data + size)
        handle_error("mmap mirror");
}

/**
 * We want to interface with the file using mmap */
static char* __open_mmap(int fd)
{
    /* obtain file size */
    struct stat sb;

    if (fstat(fd, &sb) == -1)
        handle_error("fstat");

    char *addr;
    addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        handle_error("mmap");

    return addr;
}

dqueue_t* dqueuer_open(const char* path)
{
    dqueue_t* me = calloc(1, sizeof(dqueue_t));

    if (access(path, F_OK) != 0)
    {
        perror("file doesn't exist\n");
        return NULL;
    }

    me->fd = open(path, O_RDWR | O_SYNC);
    if (me->fd == -1)
    {
        perror("couldn't open file\n");
        return NULL;
    }

    me->size = __get_maxsize(me->fd);
    me->items = aqueue_new(16, sizeof(item_t));
    me->head = me->tail = 0;

    __load(me);

    lseek(me->fd, me->head, SEEK_SET);

    me->data = __open_mmap(me->fd);
    __create_buffer_mirror(me->fd, me->data, me->size);

    return me;
}

static int __create_stub_file(unsigned int fd, size_t size)
{
    int e;

    e = lseek(fd, size, SEEK_SET);
    if (-1 == e)
    {
        perror("couldn't seek\n");
        return -1;
    }

    e = write(fd, "\0", 1);
    if (-1 == e)
    {
        perror("Couldn't write to file\n");
        return -1;
    }

    e = lseek(fd, 0, SEEK_SET);
    if (-1 == e)
    {
        perror("Couldn't seek to start of file\n");
        return -1;
    }

    return 0;
}

dqueue_t* dqueuew_open(const char* path, size_t max_size)
{
    if (max_size % sizeof(header_t) != 0)
        return NULL;

    dqueue_t* me = calloc(1, sizeof(dqueue_t));
    me->items = aqueue_new(16, sizeof(item_t));
    me->head = me->tail = 0;

    if (access(path, F_OK ) != -1)
    {
        me->fd = open(path, O_RDWR | O_SYNC);
        me->size = __get_maxsize(me->fd);
        __load(me);
    }
    else
    {
        me->fd = open(path, O_RDWR | O_SYNC | O_CREAT, 0777);
        if (-1 == me->fd)
        {
            perror("couldn't open file\n");
            return NULL;
        }

        int e = __create_stub_file(me->fd, max_size);
        if (-1 == e)
            return NULL;

        me->size = max_size;
    }

    me->data = __open_mmap(me->fd);
    __create_buffer_mirror(me->fd, me->data, max_size);

    return me;
}

unsigned int dqueue_count(dqueue_t* me)
{
    return aqueue_count(me->items);
}

int dqueue_offer(dqueue_txn_t* txn, const char* buf, const size_t len)
{
    dqueue_t* me = txn->dq;
    header_t h;

    size_t space_required = ITEM_METADATA_SIZE + len + __padding_required(len);

    if (me->size < dqueue_usedspace(me) + space_required)
        return -1;

    int start = me->tail;

    /* 1. write header */
    memcpy(h.stamp, STAMP, strlen(STAMP));
    h.type = htonl(HEADER);
    h.len = htonl(len);
    h.id = htonl(me->item_id);
    memcpy(me->data + me->tail, &h, sizeof(header_t));
    me->tail += sizeof(header_t);

    /* 2. write data */
    memcpy(me->data + me->tail, buf, len);

    /* 3. write footer */
    h.type = htonl(FOOTER);
    me->tail += len + __padding_required(len);
    memcpy(me->data + me->tail, &h, sizeof(header_t));

    txn->end += space_required;

    me->tail += sizeof(header_t);

    /* FIXME: use pool */
    item_t item;
    item.pos = start;
    item.len = len;
    item.space_used = space_required;
    item.id = me->item_id++;
    aqueue_offerensure((void*)&me->items, &item);
    return 0;
}

int dqueue_poll(dqueue_t * me)
{
    header_t h;

    memset(&h, 0, sizeof(header_t));
    memcpy(me->data + me->head, &h, sizeof(header_t));

    void* start = me->data + me->head;
    size_t len = sizeof(header_t);
    snap2pagesize(&start, &len);

    // TODO: replace with fdatasync()
    int e = msync(start, len, MS_SYNC | MS_INVALIDATE);
    if (-1 == e)
    {
        perror("Couldn't fsync file\n");
        return -1;
    }

    item_t* item = aqueue_peek(me->items);
    me->head += item->space_used;
    aqueue_poll(me->items);

    if (me->size < me->head)
        me->head %= me->size;

    return 0;
}

int dqueue_peek(dqueue_t* me, char** data, size_t* len)
{
    item_t* item = aqueue_peek(me->items);
    if (!item)
        return -1;

    *data = me->data + item->pos + sizeof(header_t);
    *len = item->len;
    return 0;
}

int dqueue_is_empty(dqueue_t * me)
{
    return aqueue_count(me->items) == 0;
}

int dqueue_size(const dqueue_t *me)
{
    return me->size;
}

int dqueue_usedspace(const dqueue_t *me)
{
    if (me->head <= me->tail)
        return me->tail - me->head;
    else
        return dqueue_size(me) - (me->head - me->tail);
}

int dqueue_unusedspace(const dqueue_t *me)
{
    return dqueue_size(me) - dqueue_usedspace(me);
}

int dqueue_txn_begin(dqueue_t* me, dqueue_txn_t* txn)
{
    memset(txn, 0, sizeof(dqueue_txn_t));
    txn->start = txn->end = me->data + me->tail; 
    txn->dq = me;
    return 0;
}

int dqueue_txn_commit(dqueue_txn_t* txn)
{
    dqueue_t* me = txn->dq; 

    if (txn->end == txn->start)
        return -1;

    void* start = txn->start;
    size_t len = txn->end - txn->start;
    snap2pagesize(&start, &len);

    /* durability */
    int e = msync(start, len, MS_SYNC | MS_INVALIDATE);
    if (-1 == e)
    {
        perror("Couldn't fsync file\n");
        assert(0);
        return -1;
    }

    return 0;
}
