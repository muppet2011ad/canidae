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

value to_str(VM *vm, value arg) {
    switch (arg.type) {
        case NUM_TYPE: {
            double number = AS_NUMBER(arg);
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
            return OBJ_VAL(copy_string(vm, arg.as.boolean ? "true" : "false", arg.as.boolean ? 4 : 5));
        }
        case TYPE_TYPE: {
            char type_strings[7][10] = {"num", "bool", "str", "array", "class", "function", "namespace"};
            long len = snprintf(NULL, 0, "<type %s>", type_strings[arg.as.type]);
            char *result = ALLOCATE(vm, char, len + 1);
            snprintf(result, len + 1, "<type %s>", type_strings[arg.as.type]);
            return OBJ_VAL(take_string(vm, result, len));
        }
        case OBJ_TYPE: {
            #define FN_TO_STRING(function) \
                long len = snprintf(NULL, 0, "<function %s>", function->name->chars); \
                char *result = ALLOCATE(vm, char, len + 1); \
                snprintf(result, len + 1, "<function %s>", function->name->chars); \
                return OBJ_VAL(take_string(vm, result, len));
            switch (GET_OBJ_TYPE(arg)) {
                case OBJ_STRING: {
                    return arg;
                }
                case OBJ_NATIVE: {
                    return OBJ_VAL(copy_string(vm, "<native function>", 17));
                }
                case OBJ_FUNCTION: {
                    FN_TO_STRING(AS_FUNCTION(arg));
                }
                case OBJ_CLOSURE: {
                    FN_TO_STRING(AS_CLOSURE(arg)->function);
                }
                case OBJ_ARRAY: {
                    #define APPEND_VAL(i) \
                        object_string *item_as_str = AS_STRING(to_str(vm, (array->arr.values[i]))); \
                        size_t newlen = len + item_as_str->length + 2; \
                        if (newlen > capacity) { \
                            size_t oldc = capacity; \
                            capacity = GROW_CAPACITY(oldc); \
                            result = GROW_ARRAY(vm, char, result, oldc, capacity); \
                        } \
                        strcat(result, item_as_str->chars);
                    object_array *array = AS_ARRAY(arg);
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
                    object_class *class_ = AS_CLASS(arg);
                    long len = snprintf(NULL, 0, "<class %s>", class_->name->chars);
                    char *result = ALLOCATE(vm, char, len + 1);
                    snprintf(result, len + 1, "<class %s>", class_->name->chars);
                    return OBJ_VAL(take_string(vm, result, len));
                }
                case OBJ_INSTANCE: {
                    object_instance *instance = AS_INSTANCE(arg);
                    long len = snprintf(NULL, 0, "<%s instance at %p>", instance->class_->name->chars, (void*) AS_OBJ(arg));
                    char *result = ALLOCATE(vm, char, len + 1);
                    snprintf(result, len + 1, "<%s instance at %p>", instance->class_->name->chars, (void*) AS_OBJ(arg));
                    return OBJ_VAL(take_string(vm, result, len));
                }
                case OBJ_BOUND_METHOD: {
                    FN_TO_STRING(AS_BOUND_METHOD(arg)->method->function);
                }
                case OBJ_NAMESPACE: {
                    object_namespace *namespace = AS_NAMESPACE(arg);
                    long len = snprintf(NULL, 0, "<namespace %s>", namespace->name->chars);
                    char *result = ALLOCATE(vm, char, len + 1);
                    snprintf(result, len + 1, "<namespace %s>", namespace->name->chars);
                    return OBJ_VAL(take_string(vm, result, len));
                }
                case OBJ_EXCEPTION: {
                    char *error_strings[8] = {"NameError", "TypeError", "ValueError", "ImportError", "ArgumentError", "RecursionError", "MemoryError", "IndexError"};
                    long len =  snprintf(NULL, 0, "<exception %s>", error_strings[AS_EXCEPTION(arg)->type]);
                    char *result = ALLOCATE(vm, char, len + 1);
                    snprintf(result, len + 1, "<exception %s>", error_strings[AS_EXCEPTION(arg)->type]);
                    return OBJ_VAL(take_string(vm, result, len));
                }
                default:
                    if(!runtime_error(vm, TYPE_ERROR, "Unprintable object type (how did you even access this?)")) return NATIVE_ERROR_VAL;
                    return NULL_VAL;
            }
            #undef FN_TO_STRING
            break;
        }
    }
    if(!runtime_error(vm, TYPE_ERROR, "Failed to convert value to string.")) return NATIVE_ERROR_VAL;
    return NULL_VAL;
}

value to_num(VM *vm, value arg) {
    value v = arg;
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
                        if(!runtime_error(vm, VALUE_ERROR, "Could not convert string '%s' to number.")) return NATIVE_ERROR_VAL;
                        return NULL_VAL;
                    }
                }
                default: {
                    if(!runtime_error(vm, TYPE_ERROR, "Invalid type for conversion to number.")) return NATIVE_ERROR_VAL;
                    return NULL_VAL;
                }
            }
        default:
            if(!runtime_error(vm, TYPE_ERROR, "Failed to convert value to number.")) return NATIVE_ERROR_VAL;
            return NULL_VAL;
    }
}

value to_bool(VM *vm, value arg) {
    return BOOL_VAL(!is_falsey(arg));
}