#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "duraqueue.h"
#include "arrayqueue.h"

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
    unsigned int id;
} item_t;

#define ITEM_METADATA_SIZE sizeof(header_t) + sizeof(header_t)

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
        int ret, start_pos;
        header_t h;

        start_pos = pos;

        /* 1. read first header */
        ret = read(me->fd, &h, sizeof(header_t));
        if (ret < (int)sizeof(header_t))
            return -1;
        pos += sizeof(header_t);

        /* check header is ok */
        if (h.type != HEADER || 0 != memcmp(h.stamp, STAMP, strlen(STAMP)))
            continue;

        unsigned int check_id = h.id;

        /* 2. read 2nd header */
        /* put on sizeof(header_t) offset */
        size_t offset = sizeof(header_t) - ntohl(h.len) % sizeof(header_t);
        ret = lseek(me->fd, ntohl(h.len) + offset, SEEK_CUR);
        pos += ntohl(h.len);

        ret = read(me->fd, &h, sizeof(header_t));
        if (ret < (int)sizeof(header_t))
        {
            perror("couldn't read file\n");
            return -1;
        }
        pos += sizeof(header_t);

        if (h.id != check_id || ntohl(h.type) != FOOTER || 0 !=
            memcmp(h.stamp, STAMP, strlen(STAMP)))
            continue;

        /* found a valid queue item */

        item_t* item = malloc(sizeof(item_t));
        item->pos = start_pos;
        item->pos = h.len + ITEM_METADATA_SIZE;
        item->id = h.id;
        arrayqueue_offer(me->items, item);

        h.id = ntohl(h.id);
    }

    if (0 == arrayqueue_count(me->items))
        return 0;

    /* get lowest */
    unsigned int lowest_id = UINT_MAX;
    arrayqueue_iterator_t iter;
    for (arrayqueue_iterator(me->items, &iter);
         arrayqueue_iterator_has_next(me->items, &iter); )
    {
        item_t* item = arrayqueue_iterator_next(me->items, &iter);
        if (item->id < lowest_id)
        {
            lowest_id = item->id;
            me->head = item->pos;
        }
    }

    void* stowaway = arrayqueue_new(16);

    /* put lowest at front of queue */
    while (1)
    {
        item_t* item = arrayqueue_peek(me->items);
        if (item->id == lowest_id)
            break;
        arrayqueue_offer(stowaway, arrayqueue_poll(me->items));
    }

    /* empty out stowaway */
    while (!arrayqueue_is_empty(stowaway))
        arrayqueue_offer(me->items, arrayqueue_poll(stowaway));

    arrayqueue_free(stowaway);

    /* get tail info */
    item_t* tail = arrayqueue_peek(me->items);
    me->tail = tail->pos + tail->len;

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
    me->items = arrayqueue_new(16);
    me->head = me->tail = 0;

    __load(me);

    lseek(me->fd, me->head, SEEK_SET);

    me->data = __open_mmap(me->fd);
    __create_buffer_mirror(me->fd, me->data, me->size);


    return me;
}

static int __create_stub_file(unsigned int fd, size_t size)
{
    int ret;

    ret = lseek(fd, size, SEEK_SET);
    if (-1 == ret)
    {
        perror("couldn't seek\n");
        return -1;
    }

    ret = write(fd, "\0", 1);
    if (-1 == ret)
    {
        perror("Couldn't write to file\n");
        return -1;
    }

    ret = lseek(fd, 0, SEEK_SET);
    if (-1 == ret)
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
    me->head = me->tail = 0;
    me->items = arrayqueue_new(16);

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

        /* create a stub file */
        int ret = __create_stub_file(me->fd, max_size);
        if (-1 == ret)
            return NULL;

        me->size = max_size;
    }

    me->data = __open_mmap(me->fd);
    //__create_buffer_mirror(me->fd, me->data, max_size);

    return me;
}

unsigned int dqueue_count(dqueue_t* me)
{
    return arrayqueue_count(me->items);
}

//unsigned int dqueue_bytes_used(dqueue_t* me)
//{
//    return me->inuse;
//}

int dqueue_offer(dqueue_t* me, const char* buf, size_t len)
{
    header_t h;

    size_t padding_required = sizeof(header_t) - len % sizeof(header_t);
    size_t space_required = ITEM_METADATA_SIZE + len + padding_required;

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
    me->tail += len + padding_required;
    memcpy(me->data + me->tail, &h, sizeof(header_t));

    /* durability */
    int ret = msync(me->data, space_required, MS_SYNC);
    if (-1 == ret)
    {
        perror("Couldn't fsync file\n");
        return -1;
    }

    me->tail += sizeof(header_t);

//    me->inuse += space_required;
    item_t* item = malloc(sizeof(item_t));
    item->pos = start;
    item->len = space_required;
    item->id = me->item_id++;
    arrayqueue_offer(me->items, item);
    return 0;
}

int dqueue_poll(dqueue_t * me)
{
    header_t h;

    memset(&h, 0, sizeof(header_t));
    memcpy(me->data + me->head, &h, sizeof(header_t));

    // TODO: replace with fdatasync()
    int ret = msync(me->data, sizeof(header_t), MS_SYNC);
    if (-1 == ret)
    {
        perror("Couldn't fsync file\n");
        return -1;
    }

    item_t* item = arrayqueue_poll(me->items);
    me->head += item->len;

    free(item);
    if (me->size < me->head)
        me->head %= me->size;

    return 0;
}

#if 0
int dqueue_peek(dqueue_t * me, const char* path, char* data, size_t len)
{
    return 0;
}
#endif

int dqueue_is_empty(dqueue_t * me)
{
    return arrayqueue_count(me->items) == 0;
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
