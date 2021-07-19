#include "common.h"
#include "segment.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    segment s;
    init_segment(&s);
    //value v;
    //v.type = NUM_TYPE;
    //v.as.number = 1.2;
    //write_constant_to_segment(&s, v, 1);
    for (int i = 0; i < 500; i++) {
        value v;
        v.type = NUM_TYPE;
        v.as.number = i;
        write_constant_to_segment(&s, v, i);
    }
    value b;
    b.type = BOOL_TYPE;
    b.as.boolean = 1;
    write_constant_to_segment(&s, b, 500);
    write_to_segment(&s, OP_RETURN, 2);
    disassemble_segment(&s, "test chunk");
    destroy_segment(&s);
    return 0;
}