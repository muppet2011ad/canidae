#include "common.h"
#include "segment.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    segment s;
    init_segment(&s);
    value v;
    v.type = NUM_TYPE;
    v.as.number = 1.2;
    size_t constant = add_constant(&s, v);
    write_to_segment(&s, OP_CONSTANT, 1);
    write_two_bytes_to_segment(&s, constant, 1);
    write_to_segment(&s, OP_RETURN, 2);
    disassemble_segment(&s, "test chunk");
    destroy_segment(&s);
    return 0;
}