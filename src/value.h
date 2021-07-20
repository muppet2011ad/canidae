#ifndef canidae_value_h

#define canidae_value_h

#include "common.h"

typedef enum {
    NUM_TYPE,
    BOOL_TYPE,
    NULL_TYPE,
    OBJ_TYPE,
} value_type;

typedef struct {
    value_type type;
    union data {
        double number;
        uint8_t boolean;
        // Object would go here but I'm not typedeffing that yet
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

#define AS_NUMBER(v) ((v).as.number)
#define AS_BOOL(v) ((v).as.boolean)

#define IS_NUMBER(v) ((v).type == NUM_TYPE)
#define IS_BOOL(v) ((v).type == BOOL_TYPE)
#define IS_NULL(v) ((v).type == NULL_TYPE)
#define IS_OBJ(v) ((v).type == OBJ_TYPE)

void init_value_array(value_array *arr);
void write_to_value_array(value_array *arr, value val);
void destroy_value_array(value_array *arr);
void print_value(value val);

#endif