#include <stdlib.h>
#include "segment.h"
#include "common.h"
#include "memory.h"

void init_segment(segment *s) {
    s->len = 0;
    s->capacity = 0;
    s->bytecode = NULL;
    s->lines = NULL;
    init_value_array(&s->constants);
}

void write_to_segment(segment *s, uint8_t byte, uint32_t line) {
    if (s->len >= s->capacity) {
        size_t old_capacity = s->capacity;
        s->capacity = GROW_CAPACITY(old_capacity);
        s->bytecode = GROW_ARRAY(uint8_t, s->bytecode, old_capacity, s->capacity);
        s->lines = GROW_ARRAY(uint32_t, s->lines, old_capacity, s->capacity);
    }
    s->bytecode[s->len] = byte;
    s->lines[s->len] = line;
    s->len++;
}

void write_two_bytes_to_segment(segment *s, uint16_t bytes, uint32_t line) {
    uint8_t low = (uint8_t) bytes;
    uint8_t high = (uint8_t) (bytes >> 8);
    write_to_segment(s, high, line);
    write_to_segment(s, low, line);
}

void destroy_segment(segment *s) {
    FREE_ARRAY(uint8_t, s->bytecode, s->capacity);
    FREE_ARRAY(uint32_t, s->lines, s->capacity);
    destroy_value_array(&s->constants);
    init_segment(s);
}

size_t add_constant(segment *s, value val) {
    write_to_value_array(&s->constants, val);
    return s->constants.len - 1;
}