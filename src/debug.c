#include <stdio.h>

#include "debug.h"

void disassemble_segment(segment *s, const char *name) {
    printf("==== %s ====\n", name);
    for (size_t offset = 0; offset < s->len;) {
        offset = dissassemble_instruction(s, offset);
    }
}

static size_t simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

size_t dissassemble_instruction(segment *s, size_t offset) {
    printf("%08lu ", offset);
    uint8_t instruction = s->bytecode[offset];
    switch (instruction) {
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        default:
            fprintf(stderr, "Unrecognised opcode %d in bytecode, halting dissassembly.\n", instruction);
            return s->len;
    }
}