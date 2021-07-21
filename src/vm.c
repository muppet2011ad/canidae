#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"

static void reset_stack(VM *vm) {
    vm->stack_ptr = vm->stack;
}

static void runtime_error(VM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm->ip - vm->s->bytecode - 1;
    uint32_t line = vm->s->lines[instruction];
    fprintf(stderr, "[line %u] in script\n", line);
    reset_stack(vm);
}

void init_VM(VM *vm) {
    vm->stack = calloc(STACK_INITIAL, sizeof(value));
    if (vm->stack == NULL) {
        fprintf(stderr, "Out of memory.");
    }
    vm->stack_len = 0;
    vm->stack_capacity = STACK_INITIAL;
    vm->objects = NULL;
    reset_stack(vm);
}

void destroy_VM(VM *vm) {
    free_objects(vm->objects);
    free(vm->stack);
}

void push(VM *vm, value val) {
    if (vm->stack_len >= vm->stack_capacity) {
        size_t old_capacity = vm->stack_capacity;
        size_t ptr_diff = ((size_t) vm->stack_ptr - (size_t) vm->stack)/sizeof(value);
        vm->stack_capacity = GROW_CAPACITY(old_capacity);
        vm->stack = GROW_ARRAY(value, vm->stack, old_capacity, vm->stack_capacity);
        vm->stack_ptr = vm->stack + ptr_diff;
    }
    *vm->stack_ptr = val;
    vm->stack_ptr++;
    vm->stack_len++;
}

value pop(VM *vm) {
    vm->stack_ptr--;
    vm->stack_len--;
    return *vm->stack_ptr;
}

static value peek(VM *vm, int distance) {
    return vm->stack_ptr[-1 - distance];
}

static uint8_t is_falsey(value v) {
    return IS_NULL(v) || (IS_BOOL(v) && !AS_BOOL(v)) || (IS_NUMBER(v) && AS_NUMBER(v) == 0); // TODO: Make zero-length strings/arrays falsey when implemted
}

static void concatenate(VM *vm) {
    object_string *b = AS_STRING(pop(vm));
    object_string *a = AS_STRING(pop(vm));
    uint32_t len = a->length + b->length;
    char *chars = ALLOCATE(char, len + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[len] = '\0';

    object_string *result = take_string(&vm->objects, chars, len);
    push(vm, OBJ_VAL(result));
}

static interpret_result run(VM *vm) {
    #define READ_BYTE() (*vm->ip++)
    #define READ_CONSTANT() (vm->s->constants.values[READ_BYTE()])    
    #define READ_CONSTANT_LONG() (vm->s->constants.values[((uint32_t) READ_BYTE() << 16) + ((uint32_t) READ_BYTE() << 8) + ((uint32_t) READ_BYTE())])
    #define BINARY_OP(type, op) \
        do { \
            if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
                runtime_error(vm, "Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            double b = AS_NUMBER(pop(vm)); \
            double a = AS_NUMBER(pop(vm)); \
            push(vm, type(a op b)); \
        } while (0)


    for (;;) {
        uint8_t instruction;
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (value *v = vm->stack; v < vm->stack_ptr; v++) {
                printf("[");
                print_value(*v);
                printf("]");
            }
            printf("\n");
            dissassemble_instruction(vm->s, (size_t) (vm->ip - vm->s->bytecode));
        #endif
        switch (instruction = READ_BYTE()) {
            case OP_RETURN:
                print_value(pop(vm));
                printf("\n");
                return INTERPRET_OK;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtime_error(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                (vm->stack_ptr-1)->as.number = -(vm->stack_ptr-1)->as.number;
                break;
            case OP_ADD: {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenate(vm);
                }
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                } else {
                    runtime_error(vm, "Operands must be two numbers or two strings");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_POWER:;
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(pow(a, b)));
                break;
            case OP_NULL: push(vm, NULL_VAL); break;
            case OP_TRUE: push(vm, BOOL_VAL(1)); break;
            case OP_FALSE: push(vm, BOOL_VAL(0)); break;
            case OP_NOT: push(vm, BOOL_VAL(is_falsey(pop(vm)))); break;
            case OP_EQUAL: {
                value b = pop(vm);
                value a = pop(vm);
                push(vm, BOOL_VAL(value_equality(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_LESS_EQUAL: BINARY_OP(BOOL_VAL, <=); break;
            case OP_CONSTANT: {
                value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                value constant = READ_CONSTANT_LONG();
                push(vm, constant);
                break;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef READ_CONSTANT_LONG
    #undef BINARY_OP
}

interpret_result interpret(VM *vm, const char *source) {
    segment seg;
    init_segment(&seg);

    if (!compile(source, &seg, &vm->objects)) {
        destroy_segment(&seg);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->s = &seg;
    vm->ip = vm->s->bytecode;

    interpret_result result = run(vm);

    destroy_segment(&seg);
    
    return result;
    //return run(vm);
}