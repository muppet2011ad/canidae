#ifndef canidae_vm_h

#define canidae_vm_h

#include "segment.h"
#include "hashmap.h"

#define STACK_INITIAL 64
#define FRAMES_MAX 1024
#define GC_THRESHOLD_INITIAL 512 * 1024

typedef struct {
    object_closure *closure;
    uint8_t *ip;
    value *slots;
    size_t slot_offset;
} call_frame;

typedef struct VM {
    char *source_path;
    call_frame frames[FRAMES_MAX];
    uint16_t frame_count;
    value *stack;
    size_t stack_capacity;
    value *stack_ptr;
    hashmap strings;
    hashmap globals;
    object_string *init_string;
    object_string *str_string;
    object_string *num_string;
    object_string *bool_string;
    object_string *add_string;
    object_string *sub_string;
    object_string *mult_string;
    object_string *div_string;
    object_string *pow_string;
    object_string *len_string;
    uint8_t owns_strings; // Secondary VMs don't own their strings table so have to leave it free
    uint8_t long_instruction;
    uint8_t gc_allowed;
    uint64_t grey_capacity;
    long grey_count;
    object **grey_stack;
    size_t bytes_allocated;
    size_t gc_threshold;
    object_upvalue *open_upvalues;
    object_exception *exception_stack;
    object *objects;
} VM;

#define STACK_LEN(vm) (vm->stack_ptr - vm->stack)

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
value popn(VM *vm, size_t n);
void define_native(VM *vm, const char *name, value (*function)(VM *vm, uint8_t argc, value *argv) );
uint8_t runtime_error(VM *vm, error_type type, const char *format, ...);
uint8_t is_falsey(value v);
void enable_gc(VM *vm);
void disable_gc(VM *vm);
void resize_stack(VM *vm, size_t target_size);

#endif