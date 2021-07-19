#include <stdlib.h>
#include "segment.h"
#include "common.h"
#include "memory.h"

void init_segment(segment *s) {
    s->len = 0;
    s->capacity = 0;
    s->bytecode = NULL;
}

void write_to_segment(segment *s, uint8_t byte) {
    if (s->len >= s->capacity) {
        size_t old_capacity = s->capacity;
        s->capacity = GROW_CAPACITY(old_capacity);
        s->bytecode = GROW_ARRAY(uint8_t, s->bytecode, old_capacity, s->capacity);
    }
    s->bytecode[s->len] = byte;
    s->len++;
}

void destroy_segment(segment *s) {
    FREE_ARRAY(uint8_t, s->bytecode, s->capacity);
    init_segment(s);
}