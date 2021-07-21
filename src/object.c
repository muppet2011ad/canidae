#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm ,type, obj_type) \
    (type*) allocate_object(vm, sizeof(type), obj_type);

static object *allocate_object(VM* vm, size_t size, object_type type) {
    object *obj = reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm->objects;
    vm->objects = obj;
    return obj;
}

static object_string *allocate_string(VM *vm, char *chars, uint32_t length, uint32_t hash) {
    object_string *string = ALLOCATE_OBJ(vm, object_string, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    hashmap_set(&vm->strings, string, NULL_VAL);
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

object_string *take_string(VM *vm, char *chars, uint32_t length) {
    uint32_t hash = hash_string(chars, length);
    object_string *interned = hashmap_find_string(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return allocate_string(vm, chars, length, hash);
}

object_string *copy_string(VM *vm, const char *chars, uint32_t length) {
    uint32_t hash = hash_string(chars, length);
    object_string *interned = hashmap_find_string(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;
    char *new_string = ALLOCATE(char, length + 1);
    memcpy(new_string, chars, length);
    new_string[length] = '\0';
    return allocate_string(vm, new_string, length, hash);
}

void print_object(value v) {
    switch (GET_OBJ_TYPE(v)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(v));
            break;
    }
}