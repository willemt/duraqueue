#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "duraqueue.h"

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

static int __load(int fd,
                  unsigned int max_size,
                  unsigned int *head,
                  unsigned int *count,
                  unsigned int *bytes_inuse)
{
    unsigned int lowest_id = UINT_MAX;
    unsigned int pos = 0;

    while (pos < max_size)
    {
        int ret;
        header_t h;

        /* 1. read first header */
        ret = read(fd, &h, sizeof(header_t));
        if (ret < (int)sizeof(header_t))
            break;
        pos += sizeof(header_t);

        if (h.type != HEADER || 0 != memcmp(h.stamp, STAMP, strlen(STAMP)))
            continue;

        unsigned int id = h.id;

        /* 2. read 2nd header */
        /* put on sizeof(header_t) offset */
        size_t offset = sizeof(header_t) - ntohl(h.len) % sizeof(header_t);
        ret = lseek(fd, ntohl(h.len) + offset, SEEK_CUR);
        pos += ntohl(h.len);

        ret = read(fd, &h, sizeof(header_t));
        if (ret < (int)sizeof(header_t))
        {
            perror("couldn't read file\n");
            return -1;
        }
        pos += sizeof(header_t);

        if (h.id != id || ntohl(h.type) != FOOTER || 0 !=
            memcmp(h.stamp, STAMP, strlen(STAMP)))
            continue;

        /* found a valid queue item */

        *count += 1;
        *bytes_inuse += ITEM_METADATA_SIZE + ntohl(h.len);

        h.id = ntohl(h.id);
        if (h.id < lowest_id)
        {
            lowest_id = h.id;
            *head = pos - ITEM_METADATA_SIZE - ntohl(h.len);
        }
    }

    return 0;
}

dqueue_t* dqueuer_open(const char* path)
{
    if (access(path, F_OK) != 0)
    {
        perror("file doesn't exist\n");
        return NULL;
    }

    int fd = open(path, O_RDWR | O_SYNC);
    if (fd == -1)
    {
        perror("couldn't open file\n");
        return NULL;
    }

    unsigned int max_size = __get_maxsize(fd);
    unsigned int head = 0;
    unsigned int count = 0;
    unsigned int bytes_inuse = 0;

    int ret = __load(fd, max_size, &head, &count, &bytes_inuse);
    if (-1 == ret)
        return NULL;

    dqueue_t* me = calloc(1, sizeof(dqueue_t));
    me->fd = fd;
    me->count = count;
    me->pos = head;
    me->inuse = bytes_inuse;
    me->max_size = __get_maxsize(fd);

    lseek(fd, head, SEEK_SET);

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

    int fd;
    unsigned int head = 0;
    unsigned int count = 0;
    unsigned int bytes_inuse = 0;

    if (access(path, F_OK ) != -1)
    {
        fd = open(path, O_RDWR | O_SYNC);
        max_size = __get_maxsize(fd);
        int ret = __load(fd, max_size, &head, &count, &bytes_inuse);
        if (-1 == ret)
            return NULL;
    }
    else
    {
        fd = open(path, O_RDWR | O_SYNC | O_CREAT, 0777);
        if (-1 == fd)
        {
            perror("couldn't open file\n");
            return NULL;
        }

        /* create a stub file */
        int ret = __create_stub_file(fd, max_size);
        if (-1 == ret)
            return NULL;
    }

    dqueue_t* me = calloc(1, sizeof(dqueue_t));
    me->fd = fd;
    me->count = count;
    me->max_size = max_size;
    me->inuse = bytes_inuse;
    return me;
}

unsigned int dqueue_count(dqueue_t* me)
{
    return me->count;
}

unsigned int dqueue_bytes_used(dqueue_t* me)
{
    return me->inuse;
}

int dqueue_offer(dqueue_t* me, const char* buf, size_t len)
{
    header_t h;

    size_t padding_required = sizeof(header_t) - len % sizeof(header_t);
    size_t space_required = ITEM_METADATA_SIZE + len + padding_required;
//    size_t space_unusable = me->max_size ITEM_METADATA_SIZE + len + padding_required;

    if (me->max_size < me->inuse + space_required)
        return -1;

    /* we don't want an item to have a header on the back and a footer on the
     * front of the "array". It prevents the reader from having clean single
     * reads */
//    if (me->max_size < me->pos + space_required)
//        me->pos = 0;

    int bytes;

    /* 1. write header */
    memcpy(h.stamp, STAMP, strlen(STAMP));
    h.type = htonl(HEADER);
    h.len = htonl(len);
    h.id = htonl(me->item_id);
    bytes = write(me->fd, &h, sizeof(header_t));
    if (bytes < (int)sizeof(header_t))
    {
        perror("Couldn't write to file\n");
        return -1;
    }

    /* 2. write data */
    bytes = write(me->fd, buf, len);
    if (bytes < (int)len)
    {
        perror("Couldn't write to file\n");
        return -1;
    }

    lseek(me->fd, padding_required, SEEK_CUR);

    /* 3. write footer */
    h.type = htonl(FOOTER);
    bytes = write(me->fd, &h, sizeof(header_t));
    if (bytes < (int)sizeof(header_t))
    {
        perror("Couldn't write to file\n");
        return -1;
    }

    /* durability */
    int ret = fsync(me->fd);
    if (-1 == ret)
    {
        perror("Couldn't fsync file\n");
        return -1;
    }

    me->inuse += space_required;
    me->count++;
    me->item_id++;
    return 0;
}

int dqueue_poll(dqueue_t * me)
{
    header_t h;

    memset(&h, 0, sizeof(header_t));

    ssize_t bytes;

    bytes = write(me->fd, &h, sizeof(header_t));
    if (bytes < (int)sizeof(header_t))
    {
        perror("Couldn't write to file\n");
        return -1;
    }

    // TODO: replace with fdatasync()
    int ret = fsync(me->fd);
    if (-1 == ret)
    {
        perror("Couldn't fsync file\n");
        return -1;
    }

    me->count--;

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
    return me->count == 0;
}
