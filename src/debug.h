#ifndef canidae_debug_h

#define canidae_debug_h

#include "segment.h"

void disassemble_segment(segment *s, const char *name);
size_t dissassemble_instruction(segment *s, size_t offset);

#endif