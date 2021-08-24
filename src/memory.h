#ifndef canidae_memory_h

#define canidae_memory_h

#include "common.h"
#include "object.h"

#define GROW_CAPACITY(c) \
    ((c) < 8 ? 8 : (c) * 2)

#define GROW_ARRAY(vm, t, ptr, oldc, newc) \
    (t*)reallocate(vm, ptr, sizeof(t) * (oldc), sizeof(t)*(newc))

#define FREE_ARRAY(vm, t, ptr, oldc) \
    reallocate(vm, ptr, sizeof(t)*oldc, 0)

#define FREE(vm, t, ptr) reallocate(vm, ptr, sizeof(t), 0)

#define ALLOCATE(vm, type, count) \
    (type*)reallocate(vm, NULL, 0, sizeof(type) * (count))

void *reallocate(VM *vm, void *ptr, size_t old_size, size_t new_size);
void collect_garbage(VM *vm);
void free_objects(VM *vm, object *objects);

#endif