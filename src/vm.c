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
    vm->frame_count = 0;
}

static void runtime_error(VM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int32_t i = vm->frame_count - 1; i >= 0; i--) {
        call_frame *frame = &vm->frames[i];
        object_function *function = frame->function;
        size_t instruction = frame->ip - function->seg.bytecode - 1;
        fprintf(stderr, "[line %u] in ", function->seg.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        }
        else {
            fprintf(stderr, "%s()n", function->name->chars);
        }
    }

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
    init_hashmap(&vm->strings);
    init_hashmap(&vm->globals);
    reset_stack(vm);
}

void destroy_VM(VM *vm) {
    destroy_hashmap(&vm->strings);
    destroy_hashmap(&vm->globals);
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

value popn(VM *vm, size_t n) {
    vm->stack_ptr -= n;
    vm->stack_len -= n;
    return *vm->stack_ptr;
}

static value peek(VM *vm, int distance) {
    return vm->stack_ptr[-1 - distance];
}

static uint8_t call(VM *vm, object_function *function, uint8_t argc) {
    if (argc != function->arity) {
        runtime_error(vm, "Function '%s' expects %u arguments (got %u).", function->name->chars, function->arity, argc);
        return 0;
    }
    if (vm->frame_count == FRAMES_MAX) {
        runtime_error(vm, "Exceeded max call depth (%u).", FRAMES_MAX);
        return 0;
    }
    call_frame *frame = &vm->frames[vm->frame_count++];
    frame->function = function;
    frame->ip = function->seg.bytecode;
    frame->slots = vm->stack_ptr - argc - 1;
    return 1;
}

static uint8_t call_value(VM *vm, value callee, uint8_t argc) {
    if (IS_OBJ(callee)) {
        if (IS_FUNCTION(callee)) {
            return call(vm, AS_FUNCTION(callee), argc);
        }
    }
    runtime_error(vm, "Can only call functions.");
    return 0;
}

static uint8_t is_falsey(value v) {
    return IS_NULL(v) || (IS_BOOL(v) && !AS_BOOL(v)) || (IS_NUMBER(v) && AS_NUMBER(v) == 0) || (IS_STRING(v) && AS_STRING(v)->length == 0) || (IS_ARRAY(v) && AS_ARRAY(v)->arr.len == 0);
}

static uint8_t concatenate(VM *vm) {
    switch (GET_OBJ_TYPE(peek(vm, 0))) {
        case OBJ_STRING: {
            object_string *b = AS_STRING(peek(vm, 0));
            object_string *a = AS_STRING(peek(vm, 1));
            size_t len = a->length + b->length;
            if (len < a->length) {
                runtime_error(vm, "String concatenation results in string larger than max string size.");
                return INTERPRET_RUNTIME_ERROR;
            }
            char *chars = ALLOCATE(char, len + 1);
            memcpy(chars, a->chars, a->length);
            memcpy(chars + a->length, b->chars, b->length);
            chars[len] = '\0';
            object_string *result = take_string(vm, chars, len);
            popn(vm, 2);
            push(vm, OBJ_VAL(result));
            break;
        }
        case OBJ_ARRAY: {
            object_array *b = AS_ARRAY(peek(vm, 0));
            object_array *a = AS_ARRAY(peek(vm, 1));
            size_t len = a->arr.len + b->arr.len;
            if (len < a->arr.len) {
                runtime_error(vm, "Array concatenation results in array larger than max array size.");
                return INTERPRET_RUNTIME_ERROR;
            }
            value *values = calloc(len, sizeof(value));
            memcpy(values, a->arr.values, a->arr.len*sizeof(value));
            memcpy(values + a->arr.len, b->arr.values, b->arr.len*sizeof(value));
            object_array *array = allocate_array(vm, values, len);
            popn(vm, 2);
            push(vm, OBJ_VAL(array));
            break;
        }
        default: break; // Unreachable
    }
    return INTERPRET_OK;
}

static uint8_t vm_array_get(VM *vm, uint8_t keep_ref) {
    switch (GET_OBJ_TYPE(peek(vm, 1))) {
        case OBJ_ARRAY: {
            value index = peek(vm, 0);
            object_array *array = AS_ARRAY(peek(vm, 1));
            if (!IS_NUMBER(index)) {
                runtime_error(vm, "Expected number as array index.");
                return INTERPRET_RUNTIME_ERROR;
            }
            if (AS_NUMBER(index) < 0) index.as.number += array->arr.len;
            if (AS_NUMBER(index) < 0) {
                runtime_error(vm, "Index is less than min index of array (-%lu).", array->arr.len);
                return INTERPRET_RUNTIME_ERROR;
            }
            if (AS_NUMBER(index) > SIZE_MAX) {
                runtime_error(vm, "Index exceeds maximum possible index value (%lu).", SIZE_MAX);
                return INTERPRET_RUNTIME_ERROR;
            }
            size_t index_int = (size_t) AS_NUMBER(index);
            if (index_int >= array->arr.len) {
                runtime_error(vm, "Array index %lu exceeds max index of array (%lu).", index_int, array->arr.len-1);
                return INTERPRET_RUNTIME_ERROR;
            }
            value at_index = array->arr.values[index_int];
            if(!keep_ref) popn(vm, 2);
            push(vm, at_index);
            break;
        }
        case OBJ_STRING: {
            value index = peek(vm, 0);
            object_string *string = AS_STRING(peek(vm, 1));
            if (!IS_NUMBER(index)) {
                runtime_error(vm, "Expected number as array index.");
                return INTERPRET_RUNTIME_ERROR;
            }
            if (AS_NUMBER(index) < 0) index.as.number += string->length;
            if (AS_NUMBER(index) < 0) {
                runtime_error(vm, "Index is less than min index of string (-%lu).", string->length);
                return INTERPRET_RUNTIME_ERROR;
            }
            if (AS_NUMBER(index) > SIZE_MAX) {
                runtime_error(vm, "Index exceeds maximum possible index value (%lu).", SIZE_MAX);
                return INTERPRET_RUNTIME_ERROR;
            }
            size_t index_int = (size_t) AS_NUMBER(index);
            if (index_int >= string->length) {
                runtime_error(vm, "Index %lu exceeds max index of string (%lu).", index_int, string->length-1);
                return INTERPRET_RUNTIME_ERROR;
            }
            object_string *result = copy_string(vm, &string->chars[index_int], 1);
            popn(vm, 2);
            push(vm, OBJ_VAL(result));
            break;
        }
        default: {
            runtime_error(vm, "Attempt to index value that is not a string or an array.");
            return INTERPRET_RUNTIME_ERROR;
        }
    }
    return INTERPRET_OK;
}

static interpret_result run(VM *vm) {
    call_frame *frame = &vm->frames[vm->frame_count - 1];
    #define READ_BYTE() (*frame->ip++)
    #define READ_CONSTANT() (frame->function->seg.constants.values[READ_BYTE()])    
    #define READ_UINT24() ((uint32_t) READ_BYTE() << 16) + ((uint32_t) READ_BYTE() << 8) + ((uint32_t) READ_BYTE())
    #define READ_UINT40() ((uint64_t) READ_BYTE() << 32) + ((uint64_t) READ_BYTE() << 24) + ((uint64_t) READ_BYTE() << 16) + ((uint64_t) READ_BYTE() << 8) + ((uint64_t) READ_BYTE())
    #define READ_UINT56() ((uint64_t) READ_BYTE() << 48) + ((uint64_t) READ_BYTE() << 40) + ((uint64_t) READ_BYTE() << 32) + ((uint64_t) READ_BYTE() << 24) + ((uint64_t) READ_BYTE() << 16) + ((uint64_t) READ_BYTE() << 8) + ((uint64_t) READ_BYTE())
    #define READ_CONSTANT_LONG() (frame->function->seg.constants.values[READ_UINT24()])
    #define READ_STRING() AS_STRING(READ_CONSTANT_LONG())
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

    #define BINARY_COMPARISON(t, op) \
        do { \
            value b = peek(vm, 0); \
            value a = peek(vm, 1); \
            if (a.type != b.type) { \
                runtime_error(vm, "Cannot perform comparison on values of different type."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            switch (a.type) { \
                case NUM_TYPE: BINARY_OP(t, op); break; \
                case OBJ_TYPE: { \
                    if (GET_OBJ_TYPE(a) != GET_OBJ_TYPE(b)) { \
                        runtime_error(vm, "Cannot perform comparison on objects of different type."); \
                        return INTERPRET_RUNTIME_ERROR; \
                    } \
                    switch (GET_OBJ_TYPE(a)) { \
                        case OBJ_STRING: { \
                            popn(vm, 2); \
                            push(vm, BOOL_VAL(string_comparison(AS_STRING(a), AS_STRING(b)) op 0)); \
                            break; \
                        } \
                        default: { \
                            runtime_error(vm, "Unsupported type for comparison operator"); \
                            return INTERPRET_RUNTIME_ERROR; \
                        } \
                    } \
                    break; \
                } \
                default: { \
                    runtime_error(vm, "Unsupported type for comparison operator"); \
                    return INTERPRET_RUNTIME_ERROR; \
                } \
            } \
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
            dissassemble_instruction(&frame->function->seg, (size_t) (frame->ip - frame->function->seg.bytecode));
        #endif
        switch (instruction = READ_BYTE()) {
            case OP_RETURN:{
                value result = pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }
                vm->stack_ptr = frame->slots;
                push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtime_error(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                (vm->stack_ptr-1)->as.number = -(vm->stack_ptr-1)->as.number;
                break;
            case OP_ADD: {
                if ((IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) || (IS_ARRAY(peek(vm, 0)) && IS_ARRAY(peek(vm, 1)))) {
                    if(concatenate(vm) == INTERPRET_RUNTIME_ERROR) return INTERPRET_RUNTIME_ERROR;
                }
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                } else {
                    runtime_error(vm, "Operands must be two numbers, strings or arrays.");
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
            case OP_GREATER: BINARY_COMPARISON(BOOL_VAL, >); break;
            case OP_GREATER_EQUAL: BINARY_COMPARISON(BOOL_VAL, >=); break;
            case OP_LESS: BINARY_COMPARISON(BOOL_VAL, <); break;
            case OP_LESS_EQUAL: BINARY_COMPARISON(BOOL_VAL, <=); break;
            case OP_PRINT: {
                print_value(pop(vm));
                printf("\n");
                break;
            }
            case OP_POP: pop(vm); break;
            case OP_ARRAY_GET: {
                if(vm_array_get(vm, 0) == INTERPRET_RUNTIME_ERROR) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_ARRAY_GET_KEEP_REF: {
                if(vm_array_get(vm, 1) == INTERPRET_RUNTIME_ERROR) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_ARRAY_SET: {
                value new_value = peek(vm, 0);
                value index = peek(vm, 1);
                if (!IS_ARRAY(peek(vm, 2))) {
                    runtime_error(vm, "Attempt to set at index of non-array value.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                object_array *array = AS_ARRAY(peek(vm, 2));
                if (!IS_NUMBER(index)) {
                    runtime_error(vm, "Expected number as array index.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (AS_NUMBER(index) > SIZE_MAX) {
                    runtime_error(vm, "Index exceeds maximum possible index value (%lu).", SIZE_MAX);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (AS_NUMBER(index) < 0) index.as.number += array->arr.len;
                if (AS_NUMBER(index) < 0) {
                    runtime_error(vm, "Index is less than min index of string (-%lu).", array->arr.len);
                    return INTERPRET_RUNTIME_ERROR;
                }
                size_t index_int = (size_t) AS_NUMBER(index);
                array_set(vm, array, index_int, new_value);
                pop(vm);
                pop(vm);
                break;
            }
            case OP_MAKE_ARRAY: {
                size_t arr_size = (size_t) AS_NUMBER(pop(vm));
                if (arr_size == 0) {
                    push(vm, OBJ_VAL(allocate_array(vm, NULL, 0)));
                    break;
                }
                value *values = calloc(arr_size, sizeof(value));
                for (long i = arr_size-1; i >= 0; i--) {
                    values[i] = peek(vm, arr_size-1-i);
                }
                object_array *array = allocate_array(vm, values, arr_size);
                popn(vm, arr_size);
                push(vm, OBJ_VAL(array));
                break;
            }
            case OP_CONSTANT: {
                value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_POPN: {
                uint8_t n = READ_BYTE();
                popn(vm, n);
                break;
            }
            case OP_CALL: {
                uint8_t argc = READ_BYTE();
                if (!call_value(vm, peek(vm, argc), argc)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_DEFINE_GLOBAL: {
                object_string *name = READ_STRING();
                hashmap_set(&vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                object_string *name = READ_STRING();
                value val;
                if (!hashmap_get(&vm->globals, name, &val)) {
                    runtime_error(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, val);
                break;
            }
            case OP_SET_GLOBAL: {
                object_string *name = READ_STRING();
                if(hashmap_set(&vm->globals, name, peek(vm, 0))) {
                    hashmap_delete(&vm->globals, name);
                    runtime_error(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint32_t arg = READ_UINT24();
                push(vm, frame->slots[arg]);
                break;
            }
            case OP_SET_LOCAL: {
                uint32_t arg = READ_UINT24();
                frame->slots[arg] = peek(vm, 0);
                break;
            }
            case OP_CONSTANT_LONG: {
                value constant = READ_CONSTANT_LONG();
                push(vm, constant);
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint64_t offset = READ_UINT40();
                if (is_falsey(peek(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint64_t offset = READ_UINT40();
                if (!is_falsey(peek(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_JUMP: {
                uint64_t offset = READ_UINT40();
                frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint64_t offset = READ_UINT40();
                frame->ip -= offset;
                break;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef READ_CONSTANT_LONG
    #undef READ_UINT24
    #undef READ_UINT40
    #undef READ_UINT56
    #undef READ_STRING
    #undef BINARY_COMPARISON
    #undef BINARY_OP
}

interpret_result interpret(VM *vm, const char *source) {
    object_function *function = compile(source, vm);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(vm, OBJ_VAL(function));
    call(vm, function, 0);

    return run(vm);
}