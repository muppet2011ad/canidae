#ifndef canidae_value_h

#define canidae_value_h

#include "common.h"

typedef struct VM VM;

typedef enum {
    NULL_TYPE,
    NUM_TYPE,
    BOOL_TYPE,
    OBJ_TYPE,
    UNDEFINED_TYPE,
    TYPE_TYPE,
    ERROR_TYPE,
    NATIVE_ERROR_TYPE, // Used only by native functions to signal a runtime error so the VM knows to stop
} value_type;

typedef enum {
    TYPEOF_NUM,
    TYPEOF_BOOL,
    TYPEOF_STRING,
    TYPEOF_ARRAY,
    TYPEOF_CLASS,
    TYPEOF_FUNCTION,
    TYPEOF_NAMESPACE,
} typeofs;

typedef enum {
    NAME_ERROR,
    TYPE_ERROR,
    VALUE_ERROR,
    IMPORT_ERROR,
    ARGUMENT_ERROR,
    RECURSION_ERROR,
    MEMORY_ERROR,
    INDEX_ERROR,
} error_type;

typedef struct object object;
typedef struct object_string object_string;
typedef struct object_array object_array;
typedef struct object_function object_function;
typedef struct object_closure object_closure;
typedef struct object_upvalue object_upvalue;
typedef struct object_native object_native;
typedef struct object_class object_class;
typedef struct object_instance object_instance;
typedef struct object_bound_method object_bound_method;
typedef struct object_namespace object_namespace;
typedef struct object_exception object_exception;

typedef struct {
    value_type type;
    union data {
        double number;
        uint8_t boolean;
        typeofs type;
        error_type err;
        object *obj;
    } as;

} value;

typedef struct {
    size_t len;
    size_t capacity;
    value *values;
} value_array;

#define NUMBER_VAL(n) ((value) {NUM_TYPE, {.number = n}})
#define BOOL_VAL(n) ((value) {BOOL_TYPE, {.boolean = n}})
#define NULL_VAL ((value) {NULL_TYPE, {.number = 0}})
#define UNDEFINED_VAL ((value) {UNDEFINED_TYPE, {.number = 0}})
#define TYPE_VAL(t) ((value) {TYPE_TYPE, {.type = t}})
#define ERROR_TYPE_VAL(t) ((value) {ERROR_TYPE, {.err = t}})
#define OBJ_VAL(o) ((value) {OBJ_TYPE, {.obj = (object*)o}})
#define NATIVE_ERROR_VAL ((value) {NATIVE_ERROR_TYPE, {.number = 0}})

#define AS_NUMBER(v) ((v).as.number)
#define AS_BOOL(v) ((v).as.boolean)
#define AS_TYPE(v) ((v).as.type)
#define AS_ERROR_TYPE(v) ((v).as.err)
#define AS_OBJ(v) ((v).as.obj)

#define IS_NUMBER(v) ((v).type == NUM_TYPE)
#define IS_BOOL(v) ((v).type == BOOL_TYPE)
#define IS_NULL(v) ((v).type == NULL_TYPE)
#define IS_OBJ(v) ((v).type == OBJ_TYPE)
#define IS_UNDEFINED(v) ((v).type == UNDEFINED_TYPE)
#define IS_TYPE_TYPE(v) ((v).type == TYPE_TYPE)
#define IS_ERROR_TYPE(v) ((v).type == ERROR_TYPE)
#define IS_NATIVE_ERROR(v) ((v).type == NATIVE_ERROR_TYPE)

void init_value_array(value_array *arr);
void write_to_value_array(VM *vm, value_array *arr, value val);
void destroy_value_array(VM *vm, value_array *arr);
void print_value(value val);
uint8_t value_equality(value a, value b);

#endif