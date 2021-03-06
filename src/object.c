#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm ,type, obj_type) \
    (type*) allocate_object(vm, sizeof(type), obj_type);

static object *allocate_object(VM* vm, size_t size, object_type type) {
    object *obj = reallocate(vm, NULL, 0, size);
    obj->is_marked = 0;
    obj->type = type;
    obj->next = vm->objects;
    vm->objects = obj;

    #ifdef DEBUG_LOG_GC
        printf("%p allocate %zu for %d\n", (void*) obj, size, type);
    #endif

    return obj;
}

object_function *new_function(VM *vm) {
    object_function *f = ALLOCATE_OBJ(vm, object_function, OBJ_FUNCTION);
    f->arity = 0;
    f->name = NULL;
    init_segment(&f->seg);
    return f;
}

object_closure *new_closure(VM *vm, object_function *function) {
    object_upvalue **upvalues = ALLOCATE(vm, object_upvalue*, function->upvalue_count);
    for (uint32_t i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }
    object_closure *closure = ALLOCATE_OBJ(vm, object_closure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

object_native *new_native(VM *vm, native_function function) {
    object_native *n = ALLOCATE_OBJ(vm, object_native, OBJ_NATIVE);
    n->function = function;
    return n;
}

object_upvalue *new_upvalue(VM *vm, value *slot) {
    object_upvalue *upvalue = ALLOCATE_OBJ(vm, object_upvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NULL_VAL;
    upvalue->next = NULL;
    return upvalue;
}

object_class *new_class(VM *vm, object_string *name) {
    object_class *class_ = ALLOCATE_OBJ(vm, object_class, OBJ_CLASS);
    class_->name = name;
    init_hashmap(&class_->methods);
    return class_;
}

object_instance *new_instance(VM *vm, object_class *class_) {
    object_instance *instance = ALLOCATE_OBJ(vm, object_instance, OBJ_INSTANCE);
    instance->class_ = class_;
    init_hashmap(&instance->fields);
    return instance;
}

object_bound_method *new_bound_method(VM *vm, value receiver, object_closure *method) {
    object_bound_method *bound = ALLOCATE_OBJ(vm, object_bound_method, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

object_namespace *new_namespace(VM *vm, object_string *name, hashmap *source) {
    object_namespace *namespace = ALLOCATE_OBJ(vm, object_namespace, OBJ_NAMESPACE);
    namespace->name = name;
    init_hashmap(&namespace->values);
    hashmap_copy_all(vm, source, &namespace->values);
    return namespace;
}

object_exception *new_exception(VM *vm, object_string *message, error_type type, size_t line) {
    push(vm, OBJ_VAL(message));
    object_exception *exception = ALLOCATE_OBJ(vm, object_exception, OBJ_EXCEPTION);
    pop(vm);
    exception->message = message;
    exception->type = type;
    exception->next = NULL;
    exception->line = line;
    return exception;
}

static object_string *allocate_string(VM *vm, char *chars, size_t length, uint32_t hash) {
    object_string *string = ALLOCATE_OBJ(vm, object_string, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    push(vm, OBJ_VAL(string));
    hashmap_set(&vm->strings, vm, string, NULL_VAL);
    pop(vm);
    return string;
}

static uint32_t hash_string(const char *key, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

object_string *take_string(VM *vm, char *chars, size_t length) {
    uint32_t hash = hash_string(chars, length);
    object_string *interned = hashmap_find_string(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(NULL, char, chars, length + 1);
        return interned;
    }
    return allocate_string(vm, chars, length, hash);
}

object_string *copy_string(VM *vm, const char *chars, size_t length) {
    uint32_t hash = hash_string(chars, length);
    object_string *interned = hashmap_find_string(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;
    char *new_string = ALLOCATE(vm, char, length + 1);
    memcpy(new_string, chars, length);
    new_string[length] = '\0';
    return allocate_string(vm, new_string, length, hash);
}

static void print_function(object_function *f) {
    if (f->name == NULL) {
        printf("<script>");
    }
    else {
        printf("<function %s>", f->name->chars);
    }
}

object_array *allocate_array(VM *vm, value *values, size_t length) {
    object_array *array = ALLOCATE_OBJ(vm, object_array, OBJ_ARRAY);
    push(vm, OBJ_VAL(array));
    init_value_array(&array->arr);
    array->arr.values = values;
    array->arr.len = length;
    // Inefficient code to get the next power of two but what can you do
    size_t pow = 1;
    while (pow < array->arr.len) {
        pow *= 2;
    }
    array->arr.values = GROW_ARRAY(vm, value, array->arr.values, length, pow);
    array->arr.capacity = pow;
    pop(vm);
    return array;
}

void array_set(VM *vm, object_array *arr, size_t index, value val) {
    // Following section is really slow and horrible code that probably could do with optimisation
    while (index >= arr->arr.capacity) {
        size_t oldc = arr->arr.capacity;
        arr->arr.capacity = GROW_CAPACITY(arr->arr.capacity);
        arr->arr.values = GROW_ARRAY(vm, value, arr->arr.values, oldc, arr->arr.capacity);
        for (size_t i = oldc; i < arr->arr.capacity; i++) {
            arr->arr.values[i] = NULL_VAL;
        }
    }
    arr->arr.values[index] = val;
    if (arr->arr.len < index+1) {
        arr->arr.len = index+1;
    }
}

int8_t string_comparison(object_string *a, object_string *b) {
    if (a == b) return 0;
    size_t str_len;
    uint8_t a_is_shorter = a->length < b->length;
    if (a_is_shorter) str_len = a->length;
    else str_len = b->length;
    int8_t result = strncmp(a->chars, b->chars, str_len);
    if (result == 0) {
        if (a_is_shorter) return -1;
        else return 1; 
    }
    return result;
}

uint8_t array_equality(object_array *a, object_array *b) {
    if (a->arr.len == b->arr.len) {
        for (size_t i = 0; i < a->arr.len; i++) {
            if (!value_equality(a->arr.values[i], b->arr.values[i])) return 0;
        }
        return 1;
    }
    else return 0;
}

void print_object(value v) {
    switch (GET_OBJ_TYPE(v)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(v));
            break;
        case OBJ_ARRAY:
            printf("[");
            object_array *array = AS_ARRAY(v);
            if (array->arr.len != 0) {
                for (size_t i = 0; i < array->arr.len-1; i++) {
                    print_value(array->arr.values[i]);
                    printf(", ");
                }
                print_value(array->arr.values[array->arr.len-1]);
            }
            printf("]");
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(v));
            break;
        case OBJ_NATIVE:
            printf("<native function>");
            break;
        case OBJ_CLOSURE:
            print_function(AS_CLOSURE(v)->function);
            break;
        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(v)->name->chars);
            break;
        case OBJ_INSTANCE:
            printf("<%s instance at %p>", AS_INSTANCE(v)->class_->name->chars, (void*) AS_OBJ(v));
            break;
        case OBJ_BOUND_METHOD:
            print_function(AS_BOUND_METHOD(v)->method->function);
            break;
        case OBJ_NAMESPACE:
            printf("<namespace %s>", AS_NAMESPACE(v)->name->chars);
            break;
        case OBJ_EXCEPTION:{
            char *error_strings[8] = {"NameError", "TypeError", "ValueError", "ImportError", "ArgumentError", "RecursionError", "MemoryError", "IndexError"};
            printf("<exception %s>", error_strings[AS_EXCEPTION(v)->type]);
            break;
        }
        default:
            break;
    }
}