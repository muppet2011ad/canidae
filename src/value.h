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
    NATIVE_ERROR_TYPE, // Used only by native functions to signal a runtime error so the VM knows to stop
} value_type;

typedef struct object object;
typedef struct object_string object_string;
typedef struct object_array object_array;
typedef struct object_function object_function;
typedef struct object_closure object_closure;
typedef struct object_upvalue object_upvalue;
typedef struct object_native object_native;
typedef struct object_class object_class;
typedef struct object_instance object_instance;

typedef struct {
    value_type type;
    union data {
        double number;
        uint8_t boolean;
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
#define OBJ_VAL(o) ((value) {OBJ_TYPE, {.obj = (object*)o}})
#define NATIVE_ERROR_VAL ((value) {NATIVE_ERROR_TYPE, {.number = 0}})

#define AS_NUMBER(v) ((v).as.number)
#define AS_BOOL(v) ((v).as.boolean)
#define AS_OBJ(v) ((v).as.obj)

#define IS_NUMBER(v) ((v).type == NUM_TYPE)
#define IS_BOOL(v) ((v).type == BOOL_TYPE)
#define IS_NULL(v) ((v).type == NULL_TYPE)
#define IS_OBJ(v) ((v).type == OBJ_TYPE)
#define IS_UNDEFINED(v) ((v).type == UNDEFINED_TYPE)
#define IS_NATIVE_ERROR(v) ((v).type == NATIVE_ERROR_TYPE)

void init_value_array(value_array *arr);
void write_to_value_array(VM *vm, value_array *arr, value val);
void destroy_value_array(VM *vm, value_array *arr);
void print_value(value val);
uint8_t value_equality(value a, value b);

#endif