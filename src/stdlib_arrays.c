#include "stdlib_arrays.h"
#include "value.h"
#include "vm.h"

value array_push_native(VM *vm, value receiver, uint8_t argc, value *argv) {
    if (argc != 1) {
        if (!runtime_error(vm, ARGUMENT_ERROR, "push() takes 1 argument (got %u).", argc)) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    write_to_value_array(vm, &AS_ARRAY(receiver)->arr, argv[0]);
    return NULL_VAL;
}

value array_pop_native(VM *vm, value receiver, uint8_t argc, value *argv) {
    if (argc != 0) {
        if (!runtime_error(vm, ARGUMENT_ERROR, "pop() takes 0 arguments (got %u).", argc)) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    object_array *array = AS_ARRAY(receiver);
    if (array->arr.len == 0) {
        if (!runtime_error(vm, VALUE_ERROR, "Cannot pop from empty array.")) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    return array->arr.values[--array->arr.len];
}

value array_contains_native(VM *vm, value receiver, uint8_t argc, value *argv) {
    if (argc != 1) {
        if (!runtime_error(vm, ARGUMENT_ERROR, "contains() takes 1 argument (got %u).", argc)) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    object_array *array = AS_ARRAY(receiver);
    for (size_t i = 0; i < array->arr.len; i++) {
        if (value_equality(array->arr.values[i], argv[0])) return BOOL_VAL(1);
    }
    return BOOL_VAL(0);
}
