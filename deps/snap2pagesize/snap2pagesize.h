#ifndef SNAP2PAGESIZE_H
#define SNAP2PAGESIZE_H

/** Set ptr to a multiple of the system's pagesize.
 * The pointer is always adjusted backwards, similiar to floor().
 * @param ptr The pointer to be snapped.
 * @param len Increased by the size of the snap adjustment. */
void snap2pagesize(void** ptr, size_t *len);

#endif /* SNAP2PAGESIZE_H */
