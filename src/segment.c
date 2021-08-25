#include <stdlib.h>
#include <stdio.h>
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
        s->bytecode = GROW_ARRAY(NULL, uint8_t, s->bytecode, old_capacity, s->capacity);
        s->lines = GROW_ARRAY(NULL, uint32_t, s->lines, old_capacity, s->capacity);
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

void destroy_segment(segment *s) {
    FREE_ARRAY(NULL, uint8_t, s->bytecode, s->capacity);
    FREE_ARRAY(NULL, uint32_t, s->lines, s->capacity);
    destroy_value_array(NULL, &s->constants);
    init_segment(s);
}

size_t add_constant(segment *s, value val) {
    size_t id = 0;
    for (; id < s->constants.len; id++) {
        if (value_equality(s->constants.values[id], val)) {
            return id;
        }
    }
    write_to_value_array(NULL, &s->constants, val);
    return id;
}