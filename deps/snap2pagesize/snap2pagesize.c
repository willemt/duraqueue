#include <unistd.h>
#include <stdint.h>

void snap2pagesize(void** ptr, size_t *len)
{
    size_t pagesize_clamp = (uintptr_t)*ptr % getpagesize();
    *len += pagesize_clamp;
    *ptr -= pagesize_clamp;
}
