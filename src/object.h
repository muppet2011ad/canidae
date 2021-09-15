#ifndef canidae_object_h

#define canidae_object_h

#include "common.h"
#include "value.h"
#include "segment.h"
#include "vm.h"

#define GET_OBJ_TYPE(v) (AS_OBJ(v)->type)
#define IS_STRING(v) is_obj_type(v, OBJ_STRING)
#define IS_ARRAY(v) is_obj_type(v, OBJ_ARRAY)
#define IS_FUNCTION(v) is_obj_type(v, OBJ_FUNCTION)
#define IS_NATIVE(v) is_obj_type(v, OBJ_NATIVE)
#define IS_CLOSURE(v) is_obj_type(v, OBJ_CLOSURE)
#define IS_CLASS(v) is_obj_type(v, OBJ_CLASS)
#define IS_INSTANCE(v) is_obj_type(v, OBJ_INSTANCE)
#define IS_BOUND_METHOD(v) is_obj_type(v, OBJ_BOUND_METHOD)
#define IS_NAMESPACE(v) is_obj_type(v, OBJ_NAMESPACE)
#define IS_EXCEPTION(v) is_obj_type(v, OBJ_EXCEPTION)

#define AS_ARRAY(v) ((object_array*)AS_OBJ(v))
#define AS_STRING(v) ((object_string*)AS_OBJ(v))
#define AS_CSTRING(v) (((object_string*)AS_OBJ(v))->chars)
#define AS_FUNCTION(v) ((object_function*)AS_OBJ(v))
#define AS_NATIVE(v) (((object_native*)AS_OBJ(v))->function)
#define AS_CLOSURE(v) ((object_closure*)AS_OBJ(v))
#define AS_CLASS(v) ((object_class*)AS_OBJ(v))
#define AS_INSTANCE(v) ((object_instance*) AS_OBJ(v))
#define AS_BOUND_METHOD(v) ((object_bound_method*) AS_OBJ(v))
#define AS_NAMESPACE(v) ((object_namespace*) AS_OBJ(v))
#define AS_EXCEPTION(v) ((object_exception*) AS_OBJ(v))

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_NAMESPACE,
    OBJ_EXCEPTION,
} object_type;

typedef value (*native_function)(VM *vm, uint8_t argc, value *argv);

struct object {
    object_type type;
    uint8_t is_marked;
    struct object *next;
};

struct object_native {
    object obj;
    native_function function;
};

struct object_function {
    object obj;
    uint8_t arity;
    uint32_t upvalue_count;
    segment seg;
    object_string *name;
};

struct object_closure {
    object obj;
    object_function *function;
    object_upvalue **upvalues;
    uint32_t upvalue_count;
};

struct object_upvalue {
    object obj;
    value *location;
    value closed;
    object_upvalue *next;
};

struct object_string {
    object obj;
    size_t length;
    char *chars;
    uint32_t hash;
};

struct object_array {
    object obj;
    value_array arr;
};

struct object_class {
    object obj;
    object_string *name;
    hashmap methods;
};

struct object_instance {
    object obj;
    object_class *class_;
    hashmap fields;
};

struct object_bound_method {
    object obj;
    value receiver;
    object_closure *method;
};

struct object_namespace {
    object obj;
    object_string *name;
    hashmap values;
};

struct object_exception {
    object obj;
    object_string *message;
    error_type type;
    object_exception *next;
};

object_native *new_native(VM *vm, native_function function);
object_function *new_function(VM *vm);
object_closure *new_closure(VM *vm, object_function *function);
object_upvalue *new_upvalue(VM *vm, value *slot);
object_class *new_class(VM *vm, object_string *name);
object_instance *new_instance(VM *vm, object_class *class_);
object_bound_method *new_bound_method(VM *vm, value receiver, object_closure *method);
object_namespace *new_namespace(VM *vm, object_string *name, hashmap *source);
object_exception *new_exception(VM *vm, object_string *message, error_type type);
object_string *take_string(VM *vm, char *chars, size_t length);
object_string *copy_string(VM *vm, const char *chars, size_t length);
object_array *allocate_array(VM *vm, value *values, size_t length);
void array_set(VM *vm, object_array *arr, size_t index, value val);
value array_get(VM *vm, object_array *arr, size_t index);
uint8_t array_equality(object_array *a, object_array *b);
int8_t string_comparison(object_string *a, object_string *b);
void print_object(value v);

static inline uint8_t is_obj_type(value v, object_type type) {
    return IS_OBJ(v) && AS_OBJ(v)->type == type;
}

#endif