#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "value.h"
#include "object.h"

void init_value_array(value_array *arr) {
    arr->values = NULL;
    arr->capacity = 0;
    arr->len = 0;
}

void write_to_value_array(VM *vm, value_array *arr, value val) {
    if (arr->len >= arr->capacity) {
        size_t oldc = arr->capacity;
        arr->capacity = GROW_CAPACITY(oldc);
        arr->values = GROW_ARRAY(vm, value, arr->values, oldc, arr->capacity);
    }
    arr->values[arr->len] = val;
    arr->len++;
}

void destroy_value_array(VM *vm, value_array *arr) {
    FREE_ARRAY(vm, value, arr->values, arr->capacity);
    init_value_array(arr);
}

void print_value(value val) {
    char bool_strings[2][6]= {"false", "true"};
    switch (val.type) {
        case NUM_TYPE:
            printf("%g", AS_NUMBER(val));
            break;
        case BOOL_TYPE:;
            uint8_t b = AS_BOOL(val);
            printf("%s", bool_strings[b]);
            break;
        case NULL_TYPE:
            printf("null"); break;
        case OBJ_TYPE: print_object(val); break;
    }
}

uint8_t value_equality(value a, value b) {
    if (a.type != b.type) return 0;
    switch (a.type) {
        case BOOL_TYPE: return AS_BOOL(a) == AS_BOOL(b);
        case NULL_TYPE: return 1;
        case NUM_TYPE: return AS_NUMBER(a) == AS_NUMBER(b);
        case OBJ_TYPE: {
            if (GET_OBJ_TYPE(a) == GET_OBJ_TYPE(b)) {
                switch (GET_OBJ_TYPE(a)) {
                    case OBJ_ARRAY:{
                        return array_equality(AS_ARRAY(a), AS_ARRAY(b));
                    }
                    default: return AS_OBJ(a) == AS_OBJ(b);
                }
            }
            else return 0;
            
        }
        default: return 0;
    }
}