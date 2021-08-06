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

#define AS_ARRAY(v) ((object_array*)AS_OBJ(v))
#define AS_STRING(v) ((object_string*)AS_OBJ(v))
#define AS_CSTRING(v) (((object_string*)AS_OBJ(v))->chars)
#define AS_FUNCTION(v) ((object_function*)AS_OBJ(v))
#define AS_NATIVE(v) (((object_native*)AS_OBJ(v))->function)

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_FUNCTION,
    OBJ_NATIVE,
} object_type;

typedef value (*native_function)(uint8_t argc, value *argv);

struct object {
    object_type type;
    struct object *next;
};

struct object_native {
    object obj;
    native_function function;
};

struct object_function {
    object obj;
    uint8_t arity;
    segment seg;
    object_string *name;
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

object_native *new_native(VM *vm, native_function function);
object_function *new_function(VM *vm);
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