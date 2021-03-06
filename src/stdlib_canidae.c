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
        if(!runtime_error(vm, ARGUMENT_ERROR, "Function 'input' expects 1 argument (got %u).", argc)) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    if (!IS_STRING(args[0])) {
        if(!runtime_error(vm, TYPE_ERROR, "Function 'input' expects a string.")) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    print_value(args[0]);
    value line = read_line(vm, stdin);
    return line;
}

static value clock_native(VM *vm, uint8_t argc, value *args) {
    if (argc != 0) {
        if(!runtime_error(vm, ARGUMENT_ERROR, "Function 'clock' expects 0 arguments (got %u).", argc)) return NATIVE_ERROR_VAL;
        return NULL_VAL;
    }
    return NUMBER_VAL(((double)clock()/CLOCKS_PER_SEC));
}

static value exception_native(VM *vm, uint8_t argc, value *args) {
    // Starts with checking the arguments to make sure we can continue
    if (argc != 2) {
        if(!runtime_error(vm, ARGUMENT_ERROR, "Function 'exception' expects 2 arguments (got %u).", argc)) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    if (!IS_ERROR_TYPE(args[0])) {
        if(!runtime_error(vm, TYPE_ERROR, "Function 'exception' expects its first argument is an error type.")) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    if (!IS_STRING(args[1])) {
        if(!runtime_error(vm, TYPE_ERROR, "Function 'exception' expects its second argument is a string.")) return NATIVE_ERROR_VAL;
        return HANDLED_NATIVE_ERROR_VAL;
    }
    call_frame *frame = vm->active_frame;
    object_function *function = frame->closure->function;
    size_t instruction = frame->ip - function->seg.bytecode - 1;

    object_exception *exception = new_exception(vm, AS_STRING(args[1]), AS_ERROR_TYPE(args[0]), function->seg.lines[instruction]);
    return OBJ_VAL(exception);
}

void define_stdlib(VM *vm) {
    disable_gc(vm);
    define_native(vm, "clock", clock_native);
    define_native(vm, "input", input);
    char *error_strings[8] = {"NameError", "TypeError", "ValueError", "ImportError", "ArgumentError", "RecursionError", "MemoryError", "IndexError"};
    for (int i = 0; i < 8; i++) {
        define_native_global(vm, error_strings[i], ERROR_TYPE_VAL(i));
    }
    define_native(vm, "exception", exception_native);
    enable_gc(vm);
}