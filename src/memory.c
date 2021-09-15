#include <stdlib.h>
#include <stdio.h>
#include "memory.h"
#include "value.h"
#include "object.h"
#include "debug.h"
#include "hashmap.h"

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(VM *vm, void *ptr, size_t old_size, size_t new_size) {
    if (vm != NULL) {
        vm->bytes_allocated += new_size - old_size;
    }
    #ifdef DEBUG_STRESS_GC
        if (vm != NULL && new_size > old_size) collect_garbage(vm);
    #else
        if (vm != NULL && vm->gc_allowed && vm->bytes_allocated > vm->gc_threshold && new_size > old_size) {
            collect_garbage(vm);
        }
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

void mark_object(VM *vm, object *obj) {
    if (obj == NULL || obj->is_marked) return;
    #ifdef DEBUG_LOG_GC
        printf("%p mark ", (void*)obj);
        print_value(OBJ_VAL(obj));
        printf("\n");
    #endif
    obj->is_marked = 1;

    if (vm->grey_capacity < vm->grey_count + 1) {
        vm->grey_capacity = GROW_CAPACITY(vm->grey_capacity);
        vm->grey_stack = (object**)realloc(vm->grey_stack, sizeof(object*) * vm->grey_capacity);
        if (vm->grey_stack == NULL) exit(1);
    }

    vm->grey_stack[vm->grey_count++] = obj;
}

void mark_value(VM *vm, value val) {
    if (IS_OBJ(val)) mark_object(vm, AS_OBJ(val));
}

static void free_object(VM *vm, object *obj) {
    #ifdef DEBUG_LOG_GC
        printf("%p free type %d", (void*) obj, obj->type);
        //print_value(OBJ_VAL(obj));
        printf("\n");
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
        case OBJ_CLASS: {
            object_class *class_ = (object_class*) obj;
            destroy_hashmap(&class_->methods, vm);
            FREE(vm, object_class, obj);
            break;
        }
        case OBJ_INSTANCE: {
            object_instance *instance = (object_instance*) obj;
            destroy_hashmap(&instance->fields, vm);
            FREE(vm, object_instance, obj);
            break;
        }
        case OBJ_BOUND_METHOD: {
            FREE(vm, object_bound_method, obj);
            break;
        }
        case OBJ_NAMESPACE: {
            object_namespace *namespace = (object_namespace*) obj;
            destroy_hashmap(&namespace->values, vm);
            FREE(vm, object_namespace, obj);
            break;
        }
    }
}

static void mark_roots(VM *vm) {
    for (value *slot = vm->stack; slot < vm->stack_ptr; slot++) { // Mark stack
        mark_value(vm, *slot);
    }
    mark_hashmap(vm, &vm->globals); // Mark global variables

    for (uint16_t i = 0; i < vm->frame_count; i++) { // Mark closures on call stack
        mark_object(vm, (object*)vm->frames[i].closure);
    }

    for (object_upvalue *upval = vm->open_upvalues; upval != NULL; upval = upval->next) { // Mark upvalues
        mark_object(vm, (object*)upval);
    }

    mark_object(vm, (object*)vm->init_string);
    mark_object(vm, (object*)vm->str_string);
    mark_object(vm, (object*)vm->num_string);
    mark_object(vm, (object*)vm->bool_string);
    mark_object(vm, (object*)vm->add_string);
    mark_object(vm, (object*)vm->sub_string);
    mark_object(vm, (object*)vm->mult_string);
    mark_object(vm, (object*)vm->div_string);
    mark_object(vm, (object*)vm->pow_string);
    mark_object(vm, (object*)vm->len_string);

    mark_object(vm, (object*)vm->exception_stack);
}

static void mark_array(VM *vm, value_array *arr) {
    for (size_t i = 0; i < arr->len; i++) {
        mark_value(vm, arr->values[i]);
    }
}

static void blacken_object(VM *vm, object *obj) {
    #ifdef DEBUG_LOG_GC
        printf("%p blacken ", (void*)obj);
        print_value(OBJ_VAL(obj));
        printf("\n");
    #endif
    switch (obj->type) {
        case OBJ_UPVALUE:
            mark_value(vm, ((object_upvalue*)obj)->closed);
            break;
        case OBJ_FUNCTION: {
            object_function *function = (object_function*) obj;
            mark_object(vm, (object*) function->name);
            mark_array(vm, &function->seg.constants);
            break;
        }
        case OBJ_CLOSURE: {
            object_closure *closure = (object_closure*) obj;
            mark_object(vm, (object*) closure->function);
            for (uint32_t i = 0; i < closure->upvalue_count; i++) {
                mark_object(vm, (object*) closure->upvalues[i]);
            }
            break;
        }
        case OBJ_ARRAY: {
            object_array *array = (object_array*) obj;
            mark_array(vm, &array->arr);
            break;
        }
        case OBJ_CLASS: {
            object_class *class_ = (object_class*) obj;
            mark_object(vm, (object*) class_->name);
            mark_hashmap(vm, &class_->methods);
            break;
        }
        case OBJ_INSTANCE: {
            object_instance *instance = (object_instance*) obj;
            mark_object(vm, (object*)instance->class_);
            mark_hashmap(vm, &instance->fields);
            break;
        }
        case OBJ_BOUND_METHOD: {
            object_bound_method *bound = (object_bound_method*) obj;
            mark_value(vm, bound->receiver);
            mark_object(vm, (object*) bound->method);
            break;
        }
        case OBJ_NAMESPACE: {
            object_namespace *namespace = (object_namespace*) obj;
            mark_object(vm, (object*) namespace->name);
            mark_hashmap(vm, &namespace->values);
            break;
        }
        case OBJ_EXCEPTION: {
            object_exception *exception = (object_exception*) obj;
            mark_object(vm, (object*) exception->message);
            mark_object(vm, (object*) exception->next);
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void trace_references(VM *vm) {
    while (vm->grey_count > 0) {
        object *obj = vm->grey_stack[--vm->grey_count];
        blacken_object(vm, obj);
    }
}

static void sweep(VM *vm) {
    object *prev = NULL;
    object *obj = vm->objects;
    while (obj != NULL) {
        if (obj->is_marked) {
            obj->is_marked = 0;
            prev = obj;
            obj = obj->next;
        }
        else {
            object *unmarked = obj;
            obj = obj->next;
            if (prev != NULL) prev->next = obj;
            else vm->objects = obj;
            free_object(vm, unmarked);
        }
    }
}

void collect_garbage(VM *vm) {
    if (!vm->gc_allowed) {
        return;
    }
    #ifdef DEBUG_LOG_GC
        printf("-- gc begin\n");
        size_t before = vm->bytes_allocated;
    #endif

    // Perform gc on heap values
    mark_roots(vm);
    trace_references(vm);
    if (vm->owns_strings) hashmap_remove_white(vm, &vm->strings);
    sweep(vm);

    // Consider shrinking stack if it's particularly oversized
    size_t stack_length = STACK_LEN(vm);
    if (vm->stack_capacity >= STACK_INITIAL*2 && stack_length*4 < vm->stack_capacity) {
        size_t oldc = vm->stack_capacity;
        resize_stack(vm, vm->stack_capacity/2);
        #ifdef DEBUG_LOG_GC
            printf("shrunk VM stack by %zu bytes (from %zu to %zu)\n", (oldc - vm->stack_capacity)*sizeof(value), oldc*sizeof(value), vm->stack_capacity*sizeof(value));
        #endif
    }

    vm->gc_threshold = vm->bytes_allocated * GC_HEAP_GROW_FACTOR;

    #ifdef DEBUG_LOG_GC
        printf("-- gc end\n");
        printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm->bytes_allocated, before, vm->bytes_allocated, vm->gc_threshold);
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