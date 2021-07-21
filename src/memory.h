#ifndef canidae_memory_h

#define canidae_memory_h

#include "common.h"
#include "object.h"

#define GROW_CAPACITY(c) \
    ((c) < 8 ? 8 : (c) * 2)

#define GROW_ARRAY(t, ptr, oldc, newc) \
    (t*)reallocate(ptr, sizeof(t) * (oldc), sizeof(t)*(newc))

#define FREE_ARRAY(t, ptr, oldc) \
    reallocate(ptr, sizeof(t)*oldc, 0)

#define FREE(t, ptr) reallocate(ptr, sizeof(t), 0)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

void *reallocate(void *ptr, size_t old_size, size_t new_size);
void free_objects(object *objects);

#endif