#ifndef canidae_stdlib_arrays_h
#define canidae_stdlib_arrays_h

#include "object.h"

value array_push_native(VM *vm, value receiver, uint8_t argc, value *argv);
value array_pop_native(VM *vm, value receiver, uint8_t argc, value *argv);
value array_contains_native(VM *vm, value receiver, uint8_t argc, value *argv);

#endif
