#include "vm.h"
#include "object.h"
#include "value.h"
#include "memory.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stdlib_canidae.h"

static value str_native(VM *vm, uint8_t argc, value *args) {
    if (argc != 1) {
        runtime_error(vm, "Function 'str' expects 1 argument (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    switch (args[0].type) {
        case NUM_TYPE: {
            double number = AS_NUMBER(args[0]);
            int len = snprintf(NULL, 0, "%g", number);
            char *result = malloc(len+1);
            snprintf(result, len + 1, "%g", number);
            return OBJ_VAL(take_string(vm, result, len));
        }
        case NULL_TYPE: {
            return OBJ_VAL(copy_string(vm, "null", 4));
        }
        case BOOL_TYPE: {
            return OBJ_VAL(copy_string(vm, args[0].as.boolean ? "true" : "false", args[0].as.boolean ? 4 : 5));
        }
        case OBJ_TYPE: {
            switch (GET_OBJ_TYPE(args[0])) {
                case OBJ_STRING: {
                    return args[0];
                }
                case OBJ_NATIVE: {
                    return OBJ_VAL(copy_string(vm, "<native function>", 17));
                }
                case OBJ_FUNCTION: {
                    int len = snprintf(NULL, 0, "<function %s>", AS_FUNCTION(args[0])->name);
                    char *result = malloc(len+1);
                    snprintf(result, len + 1, "<function %s>", AS_FUNCTION(args[0])->name);
                    return OBJ_VAL(take_string(vm, result, len));;
                }
                case OBJ_ARRAY: {
                    #define APPEND_VAL(i) \
                        object_string *item_as_str = AS_STRING(str_native(vm, 1, &(array->arr.values[i]))); \
                        size_t newlen = len + item_as_str->length + 2; \
                        if (newlen > capacity) { \
                            size_t oldc = capacity; \
                            capacity = GROW_CAPACITY(oldc); \
                            result = realloc(result, capacity); \
                        } \
                        strcat(result, item_as_str->chars);
                    object_array *array = AS_ARRAY(args[0]);
                    size_t capacity = 2;
                    size_t len = 2;
                    char *result = malloc(capacity);
                    strcpy(result, "[");
                    for (size_t i = 0; i < array->arr.len-1; i++) {
                        APPEND_VAL(i);
                        strcat(result, ", ");
                        len = newlen;
                    }
                    APPEND_VAL(array->arr.len-1);
                    if (capacity < len + 2) result = realloc(result, len + 2);
                    strcat(result, "]\0");
                    return OBJ_VAL(take_string(vm, result, len));
                }
            }
            break;
        }
    }
    runtime_error(vm, "Failed to convert value to string.");
    return NATIVE_ERROR_VAL;
}

static value clock_native(VM *vm, uint8_t argc, value *args) {
    if (argc != 0) {
        runtime_error(vm, "Function 'clock' expects 0 arguments (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    return NUMBER_VAL(((double)clock()/CLOCKS_PER_SEC));
}

void define_stdlib(VM *vm) {
    define_native(vm, "clock", clock_native);
    define_native(vm, "str", str_native);
}