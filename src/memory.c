#include <stdlib.h>
#include <stdio.h>
#include "memory.h"
#include "value.h"
#include "object.h"

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

static void free_object(object *obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            object_string *string = (object_string*) obj;
            FREE_ARRAY(char, string->chars, string->length+1);
            FREE(object_string, obj);
            break;
        }
    }
}

void free_objects(object *objects) {
    object *obj = objects;
    while (obj != NULL) {
        object *next = obj->next;
        free_object(obj);
        obj = next;
    }
}