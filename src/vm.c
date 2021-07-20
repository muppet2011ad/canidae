#include <stdio.h>
#include <math.h>
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

static void resetStack(VM *vm) {
    vm->stack_ptr = vm->stack;
}

void init_VM(VM *vm) {
    resetStack(vm);
}

void destroy_VM(VM *vm) {

}

void push(VM *vm, value val) {
    *vm->stack_ptr = val;
    vm->stack_ptr++;
}

value pop(VM *vm) {
    vm->stack_ptr--;
    return *vm->stack_ptr;
}

static interpret_result run(VM *vm) {
    #define READ_BYTE() (*vm->ip++)
    #define READ_CONSTANT() (vm->s->constants.values[READ_BYTE()])    
    #define READ_CONSTANT_LONG() (vm->s->constants.values[((uint32_t) READ_BYTE() << 16) + ((uint32_t) READ_BYTE() << 8) + ((uint32_t) READ_BYTE())])
    #define BINARY_OP(op) \
        do { \
            double b = AS_NUMBER(pop(vm)); \
            double a = AS_NUMBER(pop(vm)); \
            push(vm, NUMBER_VAL(a op b)); \
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
                (vm->stack_ptr-1)->as.number = -(vm->stack_ptr-1)->as.number; // TODO: error handling
                break;
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
            case OP_POWER:
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(pow(a, b)));
                break;
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

    if (!compile(source, &seg)) {
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