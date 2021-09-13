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
    if (argc != 0) {
        runtime_error(vm, "Function 'input' expects 1 argument (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    if (!IS_STRING(args[0])) {
        runtime_error(vm, "Function 'input' expects a string.");
        return NATIVE_ERROR_VAL;
    }
    print_value(args[0]);
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

void define_stdlib(VM *vm) {
    define_native(vm, "clock", clock_native);
    define_native(vm, "len", len_native);
    define_native(vm, "input", input);
}