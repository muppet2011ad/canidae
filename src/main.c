#include "common.h"
#include "segment.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
    VM vm;
    init_VM(&vm);
    segment s;
    init_segment(&s);
    write_constant_to_segment(&s, NUM_AS_VAL(1.2), 1);
    write_constant_to_segment(&s, NUM_AS_VAL(3.4), 1);
    write_to_segment(&s, OP_ADD, 1);
    write_constant_to_segment(&s, NUM_AS_VAL(5.6), 1);
    write_to_segment(&s, OP_DIVIDE, 1);
    write_to_segment(&s, OP_NEGATE, 1);
    write_to_segment(&s, OP_RETURN, 2);
    //disassemble_segment(&s, "test chunk");

    interpret(&vm, &s);

    destroy_VM(&vm);
    destroy_segment(&s);
    return 0;
}