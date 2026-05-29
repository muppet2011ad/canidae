#ifndef canidae_compiler_h

#define canidae_compiler_h

#include "vm.h"
#include "object.h"

object_function *compile(const char* source, VM *vm);
object_function *compile_module(const char* source, VM *vm);

#endif