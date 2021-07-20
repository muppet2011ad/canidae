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
    uint8_t constant = s->bytecode[offset+1];
    printf("%-16s %5d '", name, constant);
    print_value(s->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static size_t constant_long_instruction(const char *name, segment *s, int offset) {
    uint32_t constant = (s->bytecode[offset+1] << 16) + (s->bytecode[offset+2] << 8) + (s->bytecode[offset+3]);
    printf("%-16s %5d '", name, constant);
    print_value(s->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

size_t dissassemble_instruction(segment *s, size_t offset) {
    printf("%08lu ", offset);
    if (offset > 0 && s->lines[offset] == s->lines[offset-1]) {
        printf("   | ");
    }
    else {
        printf("%4d ", s->lines[offset]);
    }
    uint8_t instruction = s->bytecode[offset];
    switch (instruction) {
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simple_instruction("OP_DIVIDE", offset);
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", s, offset);
        case OP_CONSTANT_LONG:
            return constant_long_instruction("OP_CONSTANT_LONG", s, offset);
        default:
            fprintf(stderr, "Unrecognised opcode %d.\n", instruction);
            return s->len;
    }
}