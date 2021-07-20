#include <stdio.h>

#include "memory.h"
#include "value.h"

void init_value_array(value_array *arr) {
    arr->values = NULL;
    arr->capacity = 0;
    arr->len = 0;
}

void write_to_value_array(value_array *arr, value val) {
    if (arr->len >= arr->capacity) {
        size_t oldc = arr->capacity;
        arr->capacity = GROW_CAPACITY(oldc);
        arr->values = GROW_ARRAY(value, arr->values, oldc, arr->capacity);
    }
    arr->values[arr->len] = val;
    arr->len++;
}

void destroy_value_array(value_array *arr) {
    FREE_ARRAY(value, arr->values, arr->capacity);
    init_value_array(arr);
}

void print_value(value val) {
    char bool_strings[2][6]= {"false", "true"};
    switch (val.type) {
        case NUM_TYPE:
            printf("%g", AS_NUMBER(val));
            break;
        case BOOL_TYPE:;
            uint8_t b = AS_NUMBER(val);
            printf("%s", bool_strings[b]);
            break;
        case OBJ_TYPE:
            break;
    }
}