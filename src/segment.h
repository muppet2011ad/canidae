#ifndef canidae_segment_h
#define canidae_segment_h

#include "common.h"

typedef enum {
    OP_RETURN,
    OP_CONSTANT,
} opcode;

typedef struct {
    size_t len;
    size_t capacity;
    uint8_t *bytecode;
} segment;

void init_segment(segment *s);
void write_to_segment(segment *s, uint8_t byte);
void destroy_segment(segment *s);

#endif