#include <stdlib.h>
#include <stdio.h>
#include "memory.h"

void *reallocate(void *ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    if (result == NULL) {
        fprintf(stderr, "Failed to allocate memory, exiting...\n");
        exit(1);
    }
    return result;
}