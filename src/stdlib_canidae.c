#include "vm.h"
#include "time.h"
#include "stdlib_canidae.h"

static value clock_native(uint8_t argc, value *args) {
    return NUMBER_VAL(((double)clock()/CLOCKS_PER_SEC));
}

void define_stdlib(VM *vm) {
    define_native(vm, "clock", clock_native);
}