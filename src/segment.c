#include <stdlib.h>
#include <stdio.h>
#include "segment.h"
#include "common.h"
#include "memory.h"

#define UINT24_MAX 16777215

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

void write_n_bytes_to_segment(segment *s, uint8_t *bytes, size_t num_bytes, uint32_t line) {
    for (size_t i = 0; i < num_bytes; i++) {
        write_to_segment(s, bytes[i], line);
    }
}

uint32_t write_constant_to_segment(segment *s, value val, uint32_t line) { // TODO: make inclusion of instruction optional
    uint32_t c = (uint32_t) add_constant(s, val);
    if (c > UINT8_MAX) {
        if (c > UINT24_MAX) {
            fprintf(stderr,"Exceeding max number of constants (%d), cannot add constant.", UINT24_MAX); // Add proper error handling
        }
        else {
            write_to_segment(s, OP_CONSTANT_LONG, line);
            uint8_t bytes[3] = {c >> 16, c >> 8, c};
            write_n_bytes_to_segment(s, bytes, 3, line);
        }
    } else {
        write_to_segment(s, OP_CONSTANT, line);
        write_to_segment(s, (uint8_t) c, line);
    }
    return c;
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