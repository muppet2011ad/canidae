#ifndef canidae_segment_h
#define canidae_segment_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_RETURN,
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
void write_constant_to_segment(segment *s, value val, uint32_t line);
void destroy_segment(segment *s);
size_t add_constant(segment *s, value val);

#endif