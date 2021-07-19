#ifndef canidae_value_h

#define canidae_value_h

#include "common.h"

typedef enum {
    NUM_TYPE,
    BOOL_TYPE,
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

#define NUM_AS_VAL(n) ((value) {NUM_TYPE, n})

void init_value_array(value_array *arr);
void write_to_value_array(value_array *arr, value val);
void destroy_value_array(value_array *arr);
void print_value(value val);

#endif