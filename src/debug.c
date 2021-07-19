#include <stdio.h>

#include "debug.h"
#include "value.h"

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

static size_t constant_instruction(const char *name, segment *s, int offset) {
    uint16_t constant = *(uint16_t*) &(s->bytecode[offset+1]);
    printf("%-16s %5d '", name, constant);
    print_value(s->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

size_t dissassemble_instruction(segment *s, size_t offset) {
    printf("%08lu ", offset);
    if (offset > 0 && s->lines[offset] == s->lines[offset-1]) {
        printf("    | ");
    }
    else {
        printf("%4d ", s->lines[offset]);
    }
    uint8_t instruction = s->bytecode[offset];
    switch (instruction) {
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", s, offset);
        default:
            fprintf(stderr, "Unrecognised opcode %d in bytecode, halting dissassembly.\n", instruction);
            return s->len;
    }
}