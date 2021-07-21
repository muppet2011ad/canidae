#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(objects_list ,type, obj_type) \
    (type*) allocate_object(objects_list, sizeof(type), obj_type);

static object *allocate_object(object **objects, size_t size, object_type type) {
    object *obj = reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = *objects;
    *objects = obj;
    return obj;
}

static object_string *allocate_string(object **objects, char *chars, uint32_t length, uint32_t hash) {
    object_string *string = ALLOCATE_OBJ(objects, object_string, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    return string;
}

static uint32_t hash_string(const char *key, uint32_t length) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

object_string *take_string(object **objects, char *chars, uint32_t length) {
    uint32_t hash = hash_string(chars, length);
    return allocate_string(objects, chars, length, hash);
}

object_string *copy_string(object **objects, const char *chars, uint32_t length) {
    char *new_string = ALLOCATE(char, length + 1);
    memcpy(new_string, chars, length);
    uint32_t hash = hash_string(chars, length);
    new_string[length] = '\0';
    return allocate_string(objects, new_string, length, hash);
}

void print_object(value v) {
    switch (GET_OBJ_TYPE(v)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(v));
            break;
    }
}