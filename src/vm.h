#ifndef canidae_vm_h

#define canidae_vm_h

#include "segment.h"

#define STACK_MAX 256

typedef struct {
    segment *s;
    uint8_t *ip;
    value stack[STACK_MAX];
    value *stack_ptr;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} interpret_result;

void init_VM(VM *vm);
void destroy_VM(VM *vm);
interpret_result interpret(VM *vm, segment *s);
static interpret_result run(VM *vm);
void push(VM *vm, value val);
value pop(VM *vm);

#endif