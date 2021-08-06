#include "vm.h"
#include "time.h"
#include "stdlib_canidae.h"

static value clock_native(VM *vm, uint8_t argc, value *args) {
    if (argc != 0) {
        runtime_error(vm, "Function 'clock' expects 0 arguments (got %u).", argc);
        return NATIVE_ERROR_VAL;
    }
    return NUMBER_VAL(((double)clock()/CLOCKS_PER_SEC));
}

void define_stdlib(VM *vm) {
    define_native(vm, "clock", clock_native);
}