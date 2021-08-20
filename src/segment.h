#ifndef canidae_segment_h
#define canidae_segment_h

#include "common.h"
#include "value.h"

#define UINT24_MAX 16777215
#define UINT40_MAX 1099511627775
#define UINT56_MAX 72057594037927935

#define JUMP_OFFSET_LEN 5

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
    OP_MAKE_ARRAY,
    OP_ARRAY_GET,
    OP_ARRAY_GET_KEEP_REF,
    OP_ARRAY_SET,
    // One-byte operand
    OP_CONSTANT,
    OP_POPN,
    OP_CALL,
    // Three-byte operand 
    OP_CONSTANT_LONG,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    // Five-byte operand
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_JUMP,
    OP_LOOP,
    // Seven-byte operand
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