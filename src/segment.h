#ifndef canidae_segment_h
#define canidae_segment_h

#include "common.h"
#include "value.h"

#define UINT24_MAX 16777215

typedef enum {
    // No operand
    OP_RETURN,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_POWER,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_PRINT,
    OP_POP,
    // One-byte operand
    OP_CONSTANT,
    // Three-byte operand 
    OP_CONSTANT_LONG,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
} opcode;

typedef struct {
    size_t len;
    size_t capacity;
    uint8_t *bytecode;
    uint32_t *lines;
    value_array constants;
} segment;

void init_segment(segment *s);
void write_to_segment(segment *s, uint8_t byte, uint32_t line);
void write_n_bytes_to_segment(segment *s, uint8_t *bytes, size_t num_bytes, uint32_t line);
uint32_t write_constant_to_segment(segment *s, value val, uint32_t line);
void destroy_segment(segment *s);
size_t add_constant(segment *s, value val);

#endif