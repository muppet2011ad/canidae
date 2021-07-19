#ifndef canidae_memory_h

#define canidae_memory_h

#include "common.h"

#define GROW_CAPACITY(c) \
    ((c) < 8 ? 8 : (c) * 2)

#define GROW_ARRAY(t, ptr, oldc, newc) \
    (t*)reallocate(ptr, sizeof(t) * (oldc), sizeof(t)*(newc))

#define FREE_ARRAY(t, ptr, oldc) \
    reallocate(ptr, sizeof(t)*oldc, 0)

void *reallocate(void *ptr, size_t old_size, size_t new_size);

#endif