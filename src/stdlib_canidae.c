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

static value str_native(VM *vm, uint8_t argc, value *args) { // Converts value to string - should be given name "str"
    if (argc != 1) {
        runtime_error(vm, "Function 'str' expects 1 argument (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    switch (args[0].type) {
        case NUM_TYPE: {
            double number = AS_NUMBER(args[0]);
            int len = snprintf(NULL, 0, "%g", number);
            char *result = ALLOCATE(vm, char, len + 1);
            snprintf(result, len + 1, "%g", number);
            return OBJ_VAL(take_string(vm, result, len));
        }
        case NULL_TYPE: {
            return OBJ_VAL(copy_string(vm, "null", 4));
        }
        case UNDEFINED_TYPE: {
            return OBJ_VAL(copy_string(vm, "undefined", 9));
        }
        case BOOL_TYPE: {
            return OBJ_VAL(copy_string(vm, args[0].as.boolean ? "true" : "false", args[0].as.boolean ? 4 : 5));
        }
        case OBJ_TYPE: {
            #define FN_TO_STRING(function) \
                long len = snprintf(NULL, 0, "<function %s>", function->name->chars); \
                char *result = ALLOCATE(vm, char, len + 1); \
                snprintf(result, len + 1, "<function %s>", function->name->chars); \
                return OBJ_VAL(take_string(vm, result, len));
            switch (GET_OBJ_TYPE(args[0])) {
                case OBJ_STRING: {
                    return args[0];
                }
                case OBJ_NATIVE: {
                    return OBJ_VAL(copy_string(vm, "<native function>", 17));
                }
                case OBJ_FUNCTION: {
                    FN_TO_STRING(AS_FUNCTION(args[0]));
                }
                case OBJ_CLOSURE: {
                    FN_TO_STRING(AS_CLOSURE(args[0])->function);
                }
                case OBJ_ARRAY: {
                    #define APPEND_VAL(i) \
                        object_string *item_as_str = AS_STRING(str_native(vm, 1, &(array->arr.values[i]))); \
                        size_t newlen = len + item_as_str->length + 2; \
                        if (newlen > capacity) { \
                            size_t oldc = capacity; \
                            capacity = GROW_CAPACITY(oldc); \
                            result = GROW_ARRAY(vm, char, result, oldc, capacity); \
                        } \
                        strcat(result, item_as_str->chars);
                    object_array *array = AS_ARRAY(args[0]);
                    size_t capacity = 2;
                    size_t len = 2;
                    char *result = ALLOCATE(vm, char, capacity);
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
                    #undef APPEND_VAL
                }
                case OBJ_CLASS: {
                    object_class *class_ = AS_CLASS(args[0]);
                    long len = snprintf(NULL, 0, "<class %s>", class_->name->chars);
                    char *result = ALLOCATE(vm, char, len + 1);
                    snprintf(result, len + 1, "<class %s>", class_->name->chars);
                    return OBJ_VAL(take_string(vm, result, len));
                }
                case OBJ_INSTANCE: {
                    object_instance *instance = AS_INSTANCE(args[0]);
                    long len = snprintf(NULL, 0, "<%s instance at %p>", instance->class_->name->chars, (void*) AS_OBJ(args[0]));
                    char *result = ALLOCATE(vm, char, len + 1);
                    snprintf(result, len + 1, "<%s instance at %p>", instance->class_->name->chars, (void*) AS_OBJ(args[0]));
                    return OBJ_VAL(take_string(vm, result, len));
                }
                case OBJ_BOUND_METHOD: {
                    FN_TO_STRING(AS_BOUND_METHOD(args[0])->method->function);
                }
                case OBJ_NAMESPACE: {
                    object_namespace *namespace = AS_NAMESPACE(args[0]);
                    long len = snprintf(NULL, 0, "<namespace %s>", namespace->name->chars);
                    char *result = ALLOCATE(vm, char, len + 1);
                    snprintf(result, len + 1, "<namespace %s>", namespace->name->chars);
                    return OBJ_VAL(take_string(vm, result, len));
                }
                default:
                    runtime_error(vm, "Unprintable object type (how did you even access this?)");
                    return NATIVE_ERROR_VAL;
            }
            #undef FN_TO_STRING
            break;
        }
    }
    runtime_error(vm, "Failed to convert value to string.");
    return NATIVE_ERROR_VAL;
}

static void print_args(VM *vm, uint8_t argc, value *args) { // Helper function to print values
    for (uint8_t i = 0; i < argc; i++) {
        printf(AS_CSTRING(str_native(vm, 1, &args[i])));
    }
}

static value num_native(VM *vm, uint8_t argc, value *args) { // Converts to number, should be given name "num"
    if (argc != 1) {
        runtime_error(vm, "Function 'num' expects 1 argument (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    value v = args[0];
    switch (v.type) {
        case NUM_TYPE: return v;
        case BOOL_TYPE: return NUMBER_VAL(v.as.boolean);
        case NULL_TYPE: return NUMBER_VAL(0);
        case OBJ_TYPE:
            switch (GET_OBJ_TYPE(v)) {
                case OBJ_STRING: {
                    char *string = AS_CSTRING(v);
                    double conv = atof(string);
                    if (conv != 0 || (strspn(string, "0.") == strlen(string) && strchr(string, '.') == strrchr(string, '.'))) {
                        return NUMBER_VAL(conv);
                    }
                    else {
                        runtime_error(vm, "Could not convert string '%s' to number.");
                        return NATIVE_ERROR_VAL;
                    }
                }
                default: {
                    runtime_error(vm, "Invalid type for conversion to number.");
                    return NATIVE_ERROR_VAL;
                }
            }
        default:
            runtime_error(vm, "Failed to convert value to number.");
            return NATIVE_ERROR_VAL;
    }
}

static value int_native(VM *vm, uint8_t argc, value *args) { // Converts to number then rounds, should be given name "int"
    if (argc != 1) {
        runtime_error(vm, "Function 'int' expects 1 argument (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    return NUMBER_VAL(round(AS_NUMBER(num_native(vm, 1, &args[0]))));
}

static value bool_native(VM *vm, uint8_t argc, value *args) { // Evaluates whether its arg is truthy or falsey, should be given name "bool"
    if (argc != 1) {
        runtime_error(vm, "Function 'bool' expects 1 argument (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    return BOOL_VAL(!is_falsey(args[0]));
}

static value len_native(VM *vm, uint8_t argc, value *args) { // Gets length of array or string, should be given name "len"
    if (argc != 1) {
        runtime_error(vm, "Function 'len' expects 1 argument (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    if (args[0].type != OBJ_TYPE) {
        runtime_error(vm, "Invalid type to get length.");
        return NATIVE_ERROR_VAL;
    }
    switch(GET_OBJ_TYPE(args[0])) {
        case OBJ_ARRAY: return NUMBER_VAL(AS_ARRAY(args[0])->arr.len);
        case OBJ_STRING: return NUMBER_VAL(AS_STRING(args[0])->length);
        default: {
            runtime_error(vm, "Invalid type to get length.");
            return NATIVE_ERROR_VAL;
        }
    }
}

static value read_line(VM *vm, FILE *f) { // Helper function to read a line as an obj_string, not intended to be a directly accessible part of the lib
    size_t capacity = 0;
    size_t len = 0;
    char *s = ALLOCATE(vm, char, 8);
    char c;
    while (EOF != (c = fgetc(f)) && c != '\r' && c != '\n') {
        if (capacity == len) {
            size_t oldc = capacity;
            capacity = GROW_CAPACITY(capacity);
            s = GROW_ARRAY(vm, char, s, oldc, capacity);
        } 
        s[len++] = c;
    }
    s[len++] = '\0';
    value canidae_string = OBJ_VAL(copy_string(vm, s, len));
    free(s);
    return canidae_string;
}

static value input(VM *vm, uint8_t argc, value *args) {
    print_args(vm, argc, args);
    value line = read_line(vm, stdin);
    return line;
}

static value clock_native(VM *vm, uint8_t argc, value *args) {
    if (argc != 0) {
        runtime_error(vm, "Function 'clock' expects 0 arguments (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    return NUMBER_VAL(((double)clock()/CLOCKS_PER_SEC));
}

static value print_native(VM *vm, uint8_t argc, value *args) {
    print_args(vm, argc, args);
    puts("");
    return NULL_VAL;
}

void define_stdlib(VM *vm) {
    define_native(vm, "clock", clock_native);
    define_native(vm, "str", str_native);
    define_native(vm, "num", num_native);
    define_native(vm, "int", int_native);
    define_native(vm, "bool", bool_native);
    define_native(vm, "len", len_native);
    define_native(vm, "println", print_native);
    define_native(vm, "input", input);
}