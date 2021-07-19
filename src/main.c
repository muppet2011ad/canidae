#include "common.h"
#include "segment.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    segment s;
    init_segment(&s);
    write_to_segment(&s, OP_RETURN);
    disassemble_segment(&s, "test chunk");
    destroy_segment(&s);
    return 0;
}