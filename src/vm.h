#ifndef canidae_vm_h

#define canidae_vm_h

#include "segment.h"

#define STACK_INITIAL 256

typedef struct {
    segment *s;
    uint8_t *ip;
    value *stack;
    size_t stack_len;
    size_t stack_capacity;
    value *stack_ptr;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} interpret_result;

void init_VM(VM *vm);
void destroy_VM(VM *vm);
interpret_result interpret(VM *vm, const char *source);
void push(VM *vm, value val);
value pop(VM *vm);

#endif