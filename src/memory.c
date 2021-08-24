#include <stdlib.h>
#include <stdio.h>
#include "memory.h"
#include "value.h"
#include "object.h"
#include "debug.h"

void *reallocate(VM *vm, void *ptr, size_t old_size, size_t new_size) {
    #ifdef DEBUG_STRESS_GC
        if (vm != NULL && new_size > old_size) collect_garbage(vm);
    #endif
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

void mark_object(object *obj) {
    if (obj == NULL) return;
    #ifdef DEBUG_LOG_GC
        printf("%p mark ", (void*)obj);
        print_value(OBJ_VAL(obj));
        printf("\n");
    #endif
    obj->is_marked = 1;
}

void mark_value(value val) {
    if (IS_OBJ(val)) mark_object(AS_OBJ(val));
}

static void free_object(VM *vm, object *obj) {
    #ifdef DEBUG_LOG_GC
        printf("%p free type %d\n", (void*) obj, obj->type);
    #endif
    switch (obj->type) {
        case OBJ_STRING: {
            object_string *string = (object_string*) obj;
            FREE_ARRAY(vm, char, string->chars, string->length+1);
            FREE(vm, object_string, obj);
            break;
        }
        case OBJ_ARRAY:{
            object_array *array = (object_array*) obj;
            destroy_value_array(vm, &array->arr);
            FREE(vm, object_array, obj);
            break;
        }
        case OBJ_FUNCTION: {
            object_function *f = (object_function*)obj;
            destroy_segment(&f->seg);
            FREE(vm, object_function, obj);
            break;
        }
        case OBJ_NATIVE: {
            FREE(vm, object_native, obj);
            break;
        }
        case OBJ_CLOSURE: {
            object_closure *closure = (object_closure*) obj;
            FREE_ARRAY(vm, object_upvalue*, closure->upvalues, closure->upvalue_count);
            FREE(vm, object_closure, obj);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(vm, object_upvalue, obj);
            break;
        }
    }
}

static void mark_roots(VM *vm) {
    for (value *slot = vm->stack; slot < vm->stack_ptr; slot++) { // Mark stack
        mark_value(*slot);
    }
    mark_hashmap(&vm->globals); // Mark global variables
    mark_hashmap(&vm->strings); // Mark interned strings

    for (uint16_t i = 0; i < vm->frame_count; i++) { // Mark closures on call stack
        mark_object((object*)vm->frames[i].closure);
    }

    for (object_upvalue *upval = vm->open_upvalues; upval != NULL; upval = upval->next) { // Mark upvalues
        mark_object((object*)upval);
    }
}

void collect_garbage(VM *vm) {
    if (!vm->gc_allowed) {
        return;
    }
    #ifdef DEBUG_LOG_GC
        printf("-- gc begin\n");
    #endif

    mark_roots(vm);

    #ifdef DEBUG_LOG_GC
        printf("-- gc end\n");
    #endif
}

void free_objects(VM *vm, object *objects) {
    object *obj = objects;
    while (obj != NULL) {
        object *next = obj->next;
        free_object(vm, obj);
        obj = next;
    }
}