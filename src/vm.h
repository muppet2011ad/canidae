#ifndef canidae_vm_h

#define canidae_vm_h

#include "segment.h"
#include "hashmap.h"

#define STACK_INITIAL 256
#define FRAMES_MAX 1024

typedef struct {
    object_function *function;
    uint8_t *ip;
    value *slots;
} call_frame;

typedef struct {
    call_frame frames[FRAMES_MAX];
    uint16_t frame_count;
    value *stack;
    size_t stack_len;
    size_t stack_capacity;
    value *stack_ptr;
    hashmap strings;
    hashmap globals;
    object *objects;
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