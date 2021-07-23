#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassemble_segment(segment *s, const char *name) {
    printf("==== %s ====\n", name);
    for (size_t offset = 0; offset < s->len;) {
        offset = dissassemble_instruction(s, offset);
    }
}

static size_t simple_instruction(const char *name, size_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

static size_t raw_byte_instruction(const char *name, segment *s, size_t offset) {
    uint8_t byte = s->bytecode[offset+1];
    printf("%-16s %5u\n", name, byte);
    return offset + 2;
}

static size_t three_byte_instruction(const char *name, segment *s, size_t offset) {
    uint32_t arg = ((uint32_t) s->bytecode[offset+1] << 16) + ((uint32_t) s->bytecode[offset+2] << 8) + ((uint32_t) s->bytecode[offset+3]);
    printf("%-16s %5u\n", name, arg);
    return offset + 4;
}

static size_t jump_instruction(const char *name, segment *s, int sign, size_t offset) {
    uint64_t jump = ((uint64_t) s->bytecode[offset+1] << 32) + ((uint64_t) s->bytecode[offset+2] << 24) + ((uint64_t) s->bytecode[offset+3] << 16) + ((uint64_t) s->bytecode[offset+4] << 8) + ((uint64_t) s->bytecode[offset+5]);
    printf("%-16s %4lu -> %lu\n", name, offset, offset + 6 + sign*jump);
    return offset + 6;
}

static size_t seven_byte_instrction(const char *name, segment *s, size_t offset) {
    uint64_t arg = ((uint64_t) s->bytecode[offset+1] << 48) + ((uint64_t) s->bytecode[offset+2] << 40) + ((uint64_t) s->bytecode[offset+3] << 32) + ((uint64_t) s->bytecode[offset+4] << 24) + ((uint64_t) s->bytecode[offset+5] << 16) + ((uint64_t) s->bytecode[offset+5] << 8) + ((uint64_t) s->bytecode[offset+6] << 8) + ((uint64_t) s->bytecode[offset+7]);
    printf("%-16s %5lu\n", name, arg);
    return offset + 8;
}

static size_t constant_instruction(const char *name, segment *s, size_t offset) {
    uint8_t constant = s->bytecode[offset+1];
    printf("%-16s %5u '", name, constant);
    print_value(s->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static size_t constant_long_instruction(const char *name, segment *s, size_t offset) {
    uint32_t constant = (s->bytecode[offset+1] << 16) + (s->bytecode[offset+2] << 8) + (s->bytecode[offset+3]);
    printf("%-16s %5u '", name, constant);
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
        case OP_POWER:
            return simple_instruction("OP_POWER", offset);
        case OP_NULL:
            return simple_instruction("OP_NULL", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_EQUAL:
            return simple_instruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simple_instruction("OP_GREATER", offset);
        case OP_GREATER_EQUAL:
            return simple_instruction("OP_GREATER_EQUAL", offset);
        case OP_LESS:
            return simple_instruction("OP_LESS", offset);
        case OP_LESS_EQUAL:
            return simple_instruction("OP_LESS_EQUAL", offset);
        case OP_PRINT:
            return simple_instruction("OP_PRINT", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_MAKE_ARRAY:
            return simple_instruction("OP_MAKE_ARRAY", offset);
        case OP_ARRAY_GET:
            return simple_instruction("OP_ARRAY_GET", offset);
        case OP_ARRAY_SET:
            return simple_instruction("OP_ARRAY_SET", offset);
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", s, offset);
        case OP_POPN:
            return raw_byte_instruction("OP_POPN", s, offset);
        case OP_DEFINE_GLOBAL:
            return constant_long_instruction("OP_DEFINE_GLOBAL", s, offset);
        case OP_GET_GLOBAL:
            return constant_long_instruction("OP_GET_GLOBAL", s, offset);
        case OP_SET_GLOBAL:
            return constant_long_instruction("OP_SET_GLOBAL", s, offset);
        case OP_GET_LOCAL:
            return three_byte_instruction("OP_GET_LOCAL", s, offset);
        case OP_SET_LOCAL:
            return three_byte_instruction("OP_SET_LOCAL", s, offset);
        case OP_CONSTANT_LONG:
            return constant_long_instruction("OP_CONSTANT_LONG", s, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", s, 1, offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", s, 1, offset);
        default:
            fprintf(stderr, "Unrecognised opcode %d.\n", instruction);
            return s->len;
    }
}