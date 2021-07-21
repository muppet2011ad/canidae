#ifndef canidae_compiler_h

#define canidae_compiler_h

#include "vm.h"

int compile(const char* source, segment *seg, object **objects);

#endif