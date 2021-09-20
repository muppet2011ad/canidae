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
#include "stdlib_canidae.h"
#include "type_conversions.h"

static void reset_stack(VM *vm) {
    vm->stack_ptr = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
}

void stacktrace(VM *vm) {
    object_exception *exception = vm->exception_stack;
    char *error_strings[8] = {"NameError", "TypeError", "ValueError", "ImportError", "ArgumentError", "RecursionError", "MemoryError", "IndexError"};
    fprintf(stderr, "[%s] ", error_strings[exception->type]);
    fprintf(stderr, exception->message->chars);
    fprintf(stderr, " [line %lu]", exception->line);
    fputs("\nRaised at:\n", stderr);

    for (int32_t i = vm->frame_count - 1; i >= 0; i--) {
        call_frame *frame = &vm->frames[i];
        object_function *function = frame->closure->function;
        size_t instruction = frame->ip - function->seg.bytecode - 1;
        fprintf(stderr, "\t[line %u] in ", function->seg.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        }
        else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    while (exception->next != NULL) {
        exception = exception->next;
        fprintf(stderr, "\nError was encountered during the handling of the following error:\n\t[%s] %s [line %lu]\n", error_strings[exception->type], exception->message->chars, exception->line);
    }

    reset_stack(vm);
}

uint8_t runtime_error(VM *vm, error_type type, const char *format, ...) { // Returns whether or not the exception is handled (1 is handled, 0 is unhandled)
    va_list args;
    va_list args_2;
    va_start(args, format);
    va_copy(args_2, args);
    long len = vsnprintf(NULL, 0, format, args);
    char *error_message = ALLOCATE(vm, char, len + 1);
    vsnprintf(error_message, len + 1, format, args_2);
    
    call_frame *frame = vm->active_frame;
    object_function *function = frame->closure->function;

    size_t instruction = frame->ip - function->seg.bytecode - 1;
    disable_gc(vm);
    object_exception *exception = new_exception(vm, take_string(vm, error_message, len), type, function->seg.lines[instruction]);
    enable_gc(vm);
    va_end(args);

    return raise(vm, exception);    
}

uint8_t raise(VM *vm, object_exception *exception) { // Returns whether or not the exception is handled (1 is handled, 0 is unhandled)
    if (exception != vm->exception_stack) {
        exception->next = vm->exception_stack;
        vm->exception_stack = exception;
    }

    exception_catch *catcher = vm->catch_stack;
    while (catcher != NULL) {
        uint8_t matched = 0;
        if (catcher->num_errors == 0) break;
        else {
            for (uint8_t i = 0; i < catcher->num_errors; i++) {
                if (AS_ERROR_TYPE(catcher->catching_errors[i]) == exception->type) {
                    matched = 1;
                    break;
                }
            }
            if (matched) break;
        }
        // If we get to here, this catch statement isn't catching this exception, so we pop it and move onto the next
        exception_catch *old = catcher;
        catcher = catcher->next;
        free(old);
    }

    if (catcher == NULL) { // Code to check for except statements will go here
        stacktrace(vm);
        return 0;
    }
    else {
        vm->frame_count = catcher->frame_at_try;
        vm->active_frame = &vm->frames[vm->frame_count-1];
        vm->stack_ptr -= (STACK_LEN(vm) - catcher->stack_size_at_try);
        vm->active_frame->ip = &vm->active_frame->closure->function->seg.bytecode[catcher->catch_address];
        push(vm, OBJ_VAL(exception));
        vm->catch_stack = catcher->next;
        free(catcher);
        return 1;
    }
}

void define_native(VM *vm, const char *name, native_function function) {
    push(vm, OBJ_VAL(copy_string(vm, name, strlen(name))));
    push(vm, OBJ_VAL(new_native(vm, function)));
    hashmap_set(&vm->globals, vm, AS_STRING(vm->stack[0]), vm->stack[1]);
    popn(vm, 2);
}

void define_native_global(VM *vm, const char *name, value val) {
    hashmap_set(&vm->globals, vm, copy_string(vm, name, strlen(name)), val);
}

void init_VM(VM *vm) {
    vm->source_path = NULL;
    vm->stack = calloc(STACK_INITIAL, sizeof(value));
    if (vm->stack == NULL) {
        fprintf(stderr, "Out of memory.");
    }
    vm->long_instruction = 0;
    vm->gc_allowed = 0;
    vm->grey_capacity = 0;
    vm->grey_count = 0;
    vm->grey_stack = NULL;
    vm->bytes_allocated = STACK_INITIAL*sizeof(value); // Include initial stack allocation in heap allocation
    vm->gc_threshold = GC_THRESHOLD_INITIAL; // Arbitrary threshold
    vm->stack_capacity = STACK_INITIAL;
    vm->objects = NULL;
    vm->owns_strings = 1;
    init_hashmap(&vm->strings);
    init_hashmap(&vm->globals);
    reset_stack(vm);
    define_stdlib(vm);
    vm->exception_stack = NULL;
    vm->catch_stack = NULL;
    vm->init_string = NULL;
    vm->str_string = NULL;
    vm->num_string = NULL;
    vm->bool_string = NULL;
    vm->add_string = NULL;
    vm->sub_string = NULL;
    vm->mult_string = NULL;
    vm->div_string = NULL;
    vm->pow_string = NULL;
    vm->len_string = NULL;
    vm->message_string = NULL;
    vm->type_string = NULL;
    vm->init_string = copy_string(vm, "__init__", 8);
    vm->str_string = copy_string(vm, "__str__", 7);
    vm->num_string = copy_string(vm, "__num__", 7);
    vm->bool_string = copy_string(vm, "__bool__", 8);
    vm->add_string = copy_string(vm, "__add__", 7);
    vm->sub_string = copy_string(vm, "__sub__", 7);
    vm->mult_string = copy_string(vm, "__mul__", 7);
    vm->div_string = copy_string(vm, "__div__", 7);
    vm->pow_string = copy_string(vm, "__pow__", 7);
    vm->len_string = copy_string(vm, "__len__", 7);
    vm->message_string = copy_string(vm, "message", 7);
    vm->type_string = copy_string(vm, "type", 4);
}

void destroy_VM(VM *vm) {
    destroy_hashmap(&vm->strings, vm);
    destroy_hashmap(&vm->globals, vm);
    free_objects(vm, vm->objects);
    free(vm->stack);
    free(vm->grey_stack);
    vm->init_string = NULL;
}

void merge_VM_heaps(VM *primary, VM *secondary) {
    // Used in importing - ownership of objects in the secondary VM is transferred to the primary VM, making it responsible for memory management of those objects.
    // Allows an import statement to move objects over to the main VM before getting rid of the VM used to process the import.
    // Assumes the secondary VM is using the same strings table as the primary (necessary for string interning to work) and therefore doesn't need to be freed
    object *insert_point = primary->objects;
    object *next = primary->objects->next;
    while (next != NULL) {
        insert_point = next;
        next = next->next; // Get to end of objects list for primary VM
    }
    insert_point->next = secondary->objects;
    secondary->objects = NULL;
    // Now we need to calculate the memory usage of these objects
    size_t secondary_memory_usage = secondary->bytes_allocated - (secondary->stack_capacity * sizeof(value)) - (secondary->globals.capacity * sizeof(kv_pair)) - (secondary->strings.capacity * sizeof(kv_pair));
    primary->bytes_allocated += secondary_memory_usage;

    // Now free everything in the secondary VM
    free(secondary->stack);
    free(secondary->grey_stack);
    free(secondary->source_path);
    destroy_hashmap(&secondary->globals, secondary);
    secondary->init_string = NULL;
}

void enable_gc(VM *vm) {
    vm->gc_allowed = 1;
}

void disable_gc(VM *vm) {
    vm->gc_allowed = 0;
}

void resize_stack(VM *vm, size_t target_size) {
    disable_gc(vm);
    size_t oldc = vm->stack_capacity;
    size_t len = STACK_LEN(vm);
    vm->stack_capacity = target_size;
    value *stack_old = vm->stack;
    vm->stack = GROW_ARRAY(vm, value, vm->stack, oldc, vm->stack_capacity);
    vm->stack_ptr = &(vm->stack[len]);
    if (vm->stack != stack_old) { // If realloc has moved the stack, we need to update all pointers to the stack in upvalues and call frames
        for (size_t i = 0; i < vm->frame_count; i++) { // Moves stack frames to meet the new stack size
            call_frame *frame = &vm->frames[i];
            frame->slots = &(vm->stack[frame->slot_offset]);
        }
        object_upvalue *upval = vm->open_upvalues;
        while (upval != NULL) {
            if (&upval->closed != upval->location) { // If not closed, location must be on the stack
                upval->location = vm->stack + (upval->location - stack_old); // Calculate new pointer to location on stack
            }
            upval = upval->next; // Move onto the next upvalue
        }
    }
    enable_gc(vm);
}

void push(VM *vm, value val) {
    if (STACK_LEN(vm) >= vm->stack_capacity) {
        resize_stack(vm, GROW_CAPACITY(vm->stack_capacity));
    }
    *vm->stack_ptr = val;
    vm->stack_ptr++;
}

value pop(VM *vm) {
    vm->stack_ptr--;
    return *vm->stack_ptr;
}

value popn(VM *vm, size_t n) {
    vm->stack_ptr += -n;
    return *vm->stack_ptr;
}

static FILE* resolve_import_path(VM *vm, char *path, char **resolved_path) {
    // Attempts different locations to open the source file that we're importing from:
    // 1. Current working directory
    // 2. Directory containing the script file performing the import
    // 3. ... if I come up with anything else it will go here
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        if (vm->source_path != NULL) {
            size_t final_index = strlen(vm->source_path);
            #ifdef __unix__ // Horrible conditional compilation so it plays nice with Windows
                while (vm->source_path[final_index] != '/' && final_index != 0) final_index--;
            #endif
            #ifdef _WIN32
                while ((vm->source_path[final_index] != '/' || vm->source_path[final_index] != '\\') && final_index != 0) final_index--;
            #endif
            #ifdef _WIN64
                while ((vm->source_path[final_index] != '/' || vm->source_path[final_index] != '\\') && final_index != 0) final_index--;
            #endif
            if (final_index != 0) {
                char *final_path = malloc(final_index+strlen(path)+2); // Allocate memory to store the final path we're attempting
                memcpy(final_path, vm->source_path, final_index+1);
                memcpy(&final_path[final_index+1], path, strlen(path)+1); // Copy in the directory of the script followed by the final specified
                f = fopen(final_path, "rb");
                if (f != NULL) {
                    *resolved_path = final_path;
                    return f;
                }
                else {
                    return NULL;
                }
            }
            else {
                return NULL;
            }
        }
        else {
            return NULL;
        }
    }
    else {
        *resolved_path = path;
        return f;
    }
}

static char *read_import_file(VM *vm, char *path, char **script_path) {
    FILE *f = resolve_import_path(vm, path, script_path);
    if (f == NULL) {
        runtime_error(vm, IMPORT_ERROR, "Could not open file '%s'.", path);
        return NULL;
    }
    fseek(f, 0L, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    char *buffer = malloc(file_size+1);
    if (buffer == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(74);
    }
    size_t bytes_read_actual = fread(buffer, sizeof(char), file_size, f);
    if (bytes_read_actual < file_size) {
        runtime_error(vm, IMPORT_ERROR, "Error reading source file on import of '%s'.", path);
        return NULL;
    }
    buffer[bytes_read_actual] = '\0';
    fclose(f);
    return buffer;
}

static interpret_result import(VM *vm, object_string *filename, object_string *namespace_name) {
    char *final_path = NULL;
    char *source = read_import_file(vm, AS_CSTRING(OBJ_VAL(filename)), &final_path);
    if (source == NULL) return INTERPRET_RUNTIME_ERROR;
    VM *temp_vm = malloc(sizeof(VM));
    init_VM(temp_vm); // Create a VM to interpret our imported code
    temp_vm->source_path = final_path;
    destroy_hashmap(&temp_vm->strings, temp_vm);
    temp_vm->strings = vm->strings;
    temp_vm->owns_strings = 0;

    interpret_result status = interpret(temp_vm, source); // Run the imported module in this VM
    free(source);
    switch (status) {
        case INTERPRET_COMPILE_ERROR:
            fprintf(stderr, "Failed to compile module '%s'.", AS_CSTRING(OBJ_VAL(filename)));
            break;
        case INTERPRET_RUNTIME_ERROR:
            fprintf(stderr, "Error in module '%s'.", AS_CSTRING(OBJ_VAL(filename)));
            break;
        case INTERPRET_OK: {
            disable_gc(vm);
            object_namespace *namespace = new_namespace(vm, namespace_name, &temp_vm->globals);
            push(vm, OBJ_VAL(namespace));
            merge_VM_heaps(vm, temp_vm);
            vm->strings = temp_vm->strings;
            enable_gc(vm);
            break;
        }
    }
    free(temp_vm);
    return status;
}

static value peek(VM *vm, int distance) {
    return vm->stack_ptr[-1 - distance];
}

static uint8_t call(VM *vm, object_closure *closure, uint8_t argc) {
    if (argc != closure->function->arity) {
        return runtime_error(vm, ARGUMENT_ERROR, "Function '%s' expects %u arguments (got %u).", closure->function->name->chars, closure->function->arity, argc);
    }
    if (vm->frame_count == FRAMES_MAX) {
        return runtime_error(vm, RECURSION_ERROR, "Exceeded max call depth (%u).", FRAMES_MAX);
    }
    call_frame *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->seg.bytecode;
    frame->slots = vm->stack_ptr - argc - 1;
    frame->slot_offset = STACK_LEN(vm) - argc -1;
    return 1;
}

static uint8_t call_value(VM *vm, value callee, uint8_t argc) {
    if (IS_OBJ(callee)) {
        switch(GET_OBJ_TYPE(callee)) {
            case OBJ_CLOSURE: {
                return call(vm, AS_CLOSURE(callee), argc);
            }
            case OBJ_NATIVE: {
                native_function native = AS_NATIVE(callee);
                disable_gc(vm); // Disables gc during native function in case native function doesn't have gc-safe (i.e. push objects to stack) code
                value result = native(vm, argc, vm->stack_ptr - argc);
                enable_gc(vm); // Re-enables afterwards
                FREE(vm, char, ALLOCATE(vm, char, 1)); // Code triggers gc after function call if we've reached the threshold - the whole block essentially defers gc until after the native function
                if (IS_NATIVE_ERROR(result)) return 0;
                if (IS_HANDLED_NATIVE_ERROR(result)) return 1;
                vm->stack_ptr -= argc + 1;
                push(vm, result);
                return 1;
            }
            case OBJ_CLASS: {
                object_class *class_ = AS_CLASS(callee);
                vm->stack_ptr[-argc - 1] = OBJ_VAL(new_instance(vm, class_));
                value initialiser;
                if(hashmap_get(&class_->methods, vm->init_string, &initialiser)) {
                    return call(vm, AS_CLOSURE(initialiser), argc);
                } else if (argc != 0) {
                    return runtime_error(vm, ARGUMENT_ERROR, "Expected 0 arguments (got %u).", argc);
                }
                return 1;
            }
            case OBJ_BOUND_METHOD: {
                object_bound_method *bound = AS_BOUND_METHOD(callee);
                vm->stack_ptr[-argc - 1] = bound->receiver;
                return call(vm, bound->method, argc);
            }
            default: {
                return runtime_error(vm, TYPE_ERROR, "Can only call functions.");
            }
        }
    }
    return runtime_error(vm, TYPE_ERROR, "Can only call functions.");
}

static uint8_t invoke_from_class(VM *vm, object_class *class_, object_string *name, uint8_t argc) {
    value method;
    if (!hashmap_get(&class_->methods, name, &method)) {
        return runtime_error(vm, NAME_ERROR, "Undefined property '%s'.", name->chars);
    }
    return call(vm, AS_CLOSURE(method), argc);
}

static uint8_t invoke(VM *vm, object_string *name, uint8_t argc) {
    value receiver = peek(vm, argc);

    if (IS_NAMESPACE(receiver)) {
        object_namespace *namespace = AS_NAMESPACE(receiver);
        value v;
        if (hashmap_get(&namespace->values, name, &v)) {
            vm->stack_ptr[-argc - 1] = v;
            return call_value(vm, v, argc);
        }
        else {
            return runtime_error(vm, NAME_ERROR, "Could not find '%s' in namespace '%s'.", name->chars, namespace->name->chars);
        }
    }

    if (!IS_INSTANCE(receiver)) {
        return runtime_error(vm, TYPE_ERROR, "Only instances and namespaces have methods or functions.");
    }

    object_instance *instance = AS_INSTANCE(receiver);

    value v;
    if (hashmap_get(&instance->fields, name, &v)) {
        vm->stack_ptr[-argc - 1] = v;
        return call_value(vm, v, argc);
    }

    return invoke_from_class(vm, instance->class_, name, argc);
}

static uint8_t bind_method(VM *vm, object_class *class_, object_string *name, uint8_t keep_ref) {
    value method;
    if (!hashmap_get(&class_->methods, name, &method)) {
        return 0;
    }

    object_bound_method *bound = new_bound_method(vm, peek(vm, 0), AS_CLOSURE(method));
    if (!keep_ref) pop(vm);
    push(vm, OBJ_VAL(bound));
    return 1;
}

static object_upvalue *capture_upvalue(VM *vm, value *local) {
    object_upvalue *prev_upval = NULL;
    object_upvalue *upval = vm->open_upvalues;
    while (upval != NULL && upval->location > local) {
        prev_upval = upval;
        upval = upval->next;
    }
    if (upval != NULL && upval->location == local) {
        return upval;
    }

    object_upvalue *created_upvalue = new_upvalue(vm, local);
    created_upvalue->next = NULL;
    if (prev_upval == NULL) {
        vm->open_upvalues = created_upvalue;
    }
    else {
        prev_upval->next = created_upvalue;
    }
    return created_upvalue;
}

static void close_upvalues(VM *vm, value *last) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        object_upvalue *upval = vm->open_upvalues;
        upval->closed = *upval->location;
        upval->location = &upval->closed;
        vm->open_upvalues = upval->next;
    }
}

static void define_method(VM *vm, object_string *name) {
    value method = peek(vm, 0);
    object_class *class_ = AS_CLASS(peek(vm, 1));
    hashmap_set(&class_->methods, vm, name, method);
    pop(vm);
}

uint8_t is_falsey(value v) {
    return IS_NULL(v) || (IS_BOOL(v) && !AS_BOOL(v)) || (IS_NUMBER(v) && AS_NUMBER(v) == 0) || (IS_STRING(v) && AS_STRING(v)->length == 0) || (IS_ARRAY(v) && AS_ARRAY(v)->arr.len == 0) || IS_UNDEFINED(v);
}

static uint8_t concatenate(VM *vm) {
    switch (GET_OBJ_TYPE(peek(vm, 0))) {
        case OBJ_STRING: {
            object_string *b = AS_STRING(peek(vm, 0));
            object_string *a = AS_STRING(peek(vm, 1));
            size_t len = a->length + b->length;
            if (len < a->length) {
                runtime_error(vm, MEMORY_ERROR, "String concatenation results in string larger than max string size.");
                return INTERPRET_RUNTIME_ERROR;
            }
            char *chars = ALLOCATE(vm, char, len + 1);
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
                runtime_error(vm, MEMORY_ERROR, "Array concatenation results in array larger than max array size.");
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

static uint8_t convert_type(VM *vm, value converter(VM*, value), object_string *override_function) {
    value v = peek(vm, 0);
    uint8_t is_instance = IS_INSTANCE(v);
    uint8_t overridden = 0;
    if (is_instance) {
        object_instance *instance = AS_INSTANCE(v);
        value method;
        if (hashmap_get(&instance->class_->methods, override_function, &method)) {
            return call(vm, AS_CLOSURE(method), 0);
        }
    }
    if (!is_instance || !overridden) {
        value converted = converter(vm, v);
        if (IS_NATIVE_ERROR(converted)) return 0;
        pop(vm);
        push(vm, converted);
        return 1;
    }
    return 0;
}

static uint8_t vm_array_get(VM *vm, uint8_t keep_ref) {
    switch (GET_OBJ_TYPE(peek(vm, 1))) {
        case OBJ_ARRAY: {
            value index = peek(vm, 0);
            object_array *array = AS_ARRAY(peek(vm, 1));
            if (!IS_NUMBER(index)) {
                return runtime_error(vm, TYPE_ERROR, "Expected number as array index.");
            }
            if (AS_NUMBER(index) < 0) index.as.number += array->arr.len;
            if (AS_NUMBER(index) < 0) {
                return runtime_error(vm, INDEX_ERROR, "Index is less than min index of array (-%lu).", array->arr.len);
            }
            if (AS_NUMBER(index) > SIZE_MAX) {
                return runtime_error(vm, INDEX_ERROR, "Index exceeds maximum possible index value (%lu).", SIZE_MAX);
            }
            size_t index_int = (size_t) AS_NUMBER(index);
            if (index_int >= array->arr.len) {
                return runtime_error(vm, INDEX_ERROR, "Array index %lu exceeds max index of array (%lu).", index_int, array->arr.len-1);
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
                return runtime_error(vm, TYPE_ERROR, "Expected number as array index.");
            }
            if (AS_NUMBER(index) < 0) index.as.number += string->length;
            if (AS_NUMBER(index) < 0) {
                return runtime_error(vm, INDEX_ERROR, "Index is less than min index of string (-%lu).", string->length);
            }
            if (AS_NUMBER(index) > SIZE_MAX) {
                return runtime_error(vm, INDEX_ERROR, "Index exceeds maximum possible index value (%lu).", SIZE_MAX);
            }
            size_t index_int = (size_t) AS_NUMBER(index);
            if (index_int >= string->length) {
                return runtime_error(vm, INDEX_ERROR, "Index %lu exceeds max index of string (%lu).", index_int, string->length-1);
            }
            object_string *result = copy_string(vm, &string->chars[index_int], 1);
            popn(vm, 2);
            push(vm, OBJ_VAL(result));
            break;
        }
        default: {
            return runtime_error(vm, TYPE_ERROR, "Attempt to index value that is not a string or an array.");
        }
    }
    return 1;
}

static interpret_result run(VM *vm) {
    vm->active_frame = &vm->frames[vm->frame_count - 1];
    #define READ_BYTE() (*vm->active_frame->ip++)
    #define READ_CONSTANT() (vm->active_frame->closure->function->seg.constants.values[READ_BYTE()])    
    #define READ_UINT24() ((uint32_t) READ_BYTE() << 16) + ((uint32_t) READ_BYTE() << 8) + ((uint32_t) READ_BYTE())
    #define READ_UINT40() ((uint64_t) READ_BYTE() << 32) + ((uint64_t) READ_BYTE() << 24) + ((uint64_t) READ_BYTE() << 16) + ((uint64_t) READ_BYTE() << 8) + ((uint64_t) READ_BYTE())
    #define READ_UINT48() ((uint64_t) READ_BYTE() << 40) + ((uint64_t) READ_BYTE() << 32) + ((uint64_t) READ_BYTE() << 24) + ((uint64_t) READ_BYTE() << 16) + ((uint64_t) READ_BYTE() << 8) + ((uint64_t) READ_BYTE())
    #define READ_UINT56() ((uint64_t) READ_BYTE() << 48) + ((uint64_t) READ_BYTE() << 40) + ((uint64_t) READ_BYTE() << 32) + ((uint64_t) READ_BYTE() << 24) + ((uint64_t) READ_BYTE() << 16) + ((uint64_t) READ_BYTE() << 8) + ((uint64_t) READ_BYTE())
    #define READ_CONSTANT_LONG() (vm->active_frame->closure->function->seg.constants.values[READ_UINT24()])
    #define READ_STRING(constant) AS_STRING(constant)
    #define BINARY_OP(type, op, override) \
        do { \
            if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
                uint8_t overriden = 0; \
                if (IS_INSTANCE(peek(vm, 1))) { \
                    object_instance *instance = AS_INSTANCE(peek(vm, 1)); \
                    value v; \
                    if (hashmap_get(&instance->class_->methods, override, &v)) { \
                        overriden = call(vm, AS_CLOSURE(v), 1); \
                        vm->active_frame = &vm->frames[vm->frame_count-1]; \
                    } \
                } \
                if (!overriden) { \
                    if(!runtime_error(vm, TYPE_ERROR, "Unsupported operands for binary operation.")) return INTERPRET_RUNTIME_ERROR; \
                } \
            } \
            else { \
                double b = AS_NUMBER(pop(vm)); \
                double a = AS_NUMBER(pop(vm)); \
                push(vm, type(a op b)); \
            } \
        } while (0)

    #define READ_VARIABLE_ARG() \
        (vm->long_instruction ? ({vm->long_instruction = 0; READ_UINT24();}) : READ_BYTE())

    #define READ_VARIABLE_CONST() \
        (vm->long_instruction ? ({vm->long_instruction = 0; READ_CONSTANT_LONG();}) : READ_CONSTANT())

    #define BINARY_COMPARISON(t, op) \
        do { \
            value b = peek(vm, 0); \
            value a = peek(vm, 1); \
            if (a.type != b.type) { \
                if (!runtime_error(vm, TYPE_ERROR, "Cannot perform comparison on values of different type.")) return INTERPRET_RUNTIME_ERROR; \
                continue; \
            } \
            switch (a.type) { \
                case NUM_TYPE: BINARY_OP(t, op, NULL); break; \
                case OBJ_TYPE: { \
                    if (GET_OBJ_TYPE(a) != GET_OBJ_TYPE(b)) { \
                        if(!runtime_error(vm, TYPE_ERROR, "Cannot perform comparison on objects of different type.")) return INTERPRET_RUNTIME_ERROR; \
                        continue; \
                    } \
                    switch (GET_OBJ_TYPE(a)) { \
                        case OBJ_STRING: { \
                            popn(vm, 2); \
                            push(vm, BOOL_VAL(string_comparison(AS_STRING(a), AS_STRING(b)) op 0)); \
                            break; \
                        } \
                        default: { \
                            if(!runtime_error(vm, TYPE_ERROR, "Unsupported type for comparison operator")) return INTERPRET_RUNTIME_ERROR; \
                            continue; \
                        } \
                    } \
                    break; \
                } \
                default: { \
                    if(!runtime_error(vm, TYPE_ERROR, "Unsupported type for comparison operator")) return INTERPRET_RUNTIME_ERROR; \
                    continue; \
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
            dissassemble_instruction(&vm->active_frame->closure->function->seg, (size_t) (vm->active_frame->ip - vm->active_frame->closure->function->seg.bytecode));
        #endif
        switch (instruction = READ_BYTE()) {
            case OP_RETURN:{
                value result = pop(vm);
                close_upvalues(vm, vm->active_frame->slots);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }
                vm->stack_ptr = vm->active_frame->slots;
                push(vm, result);
                vm->active_frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0))) {
                    if(!runtime_error(vm, TYPE_ERROR, "Operand must be a number.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                (vm->stack_ptr-1)->as.number = -(vm->stack_ptr-1)->as.number;
                break;
            case OP_ADD: {
                if ((IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) || (IS_ARRAY(peek(vm, 0)) && IS_ARRAY(peek(vm, 1)))) {
                    if(concatenate(vm) == INTERPRET_RUNTIME_ERROR) return INTERPRET_RUNTIME_ERROR;
                }
                else {
                    BINARY_OP(NUMBER_VAL, +, vm->add_string);
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -, vm->sub_string); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *, vm->mult_string); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /, vm->div_string); break;
            case OP_POWER: {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
                    uint8_t overriden = 0;
                    if (IS_INSTANCE(peek(vm, 1))) {
                        object_instance *instance = AS_INSTANCE(peek(vm, 1));
                        value v;
                        if (hashmap_get(&instance->class_->methods, vm->pow_string, &v)) {
                            overriden = call(vm, AS_CLOSURE(v), 1);
                            vm->active_frame = &vm->frames[vm->frame_count-1];
                        }
                    }
                    if (!overriden) {
                        if(!runtime_error(vm, TYPE_ERROR, "Unsupported operands for binary operation.")) return INTERPRET_RUNTIME_ERROR;
                        continue;
                    }
                }
                else {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(pow(a, b)));
                }
                break;
            }
            case OP_UNDEFINED: push(vm, UNDEFINED_VAL); break;
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
                if(!vm_array_get(vm, 0)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_ARRAY_GET_KEEP_REF: {
                if(!vm_array_get(vm, 1)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_ARRAY_SET: {
                value new_value = peek(vm, 0);
                value index = peek(vm, 1);
                if (!IS_ARRAY(peek(vm, 2))) {
                    if(!runtime_error(vm, TYPE_ERROR, "Attempt to set at index of non-array value.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                object_array *array = AS_ARRAY(peek(vm, 2));
                if (!IS_NUMBER(index)) {
                    if(!runtime_error(vm, TYPE_ERROR, "Expected number as array index.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                if (AS_NUMBER(index) > SIZE_MAX) {
                    if(!runtime_error(vm, INDEX_ERROR, "Index exceeds maximum possible index value (%lu).", SIZE_MAX)) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                if (AS_NUMBER(index) < 0) index.as.number += array->arr.len;
                if (AS_NUMBER(index) < 0) {
                    if(!runtime_error(vm, INDEX_ERROR, "Index is less than min index of string (-%lu).", array->arr.len)) return INTERPRET_RUNTIME_ERROR;
                    continue;
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
            case OP_CLOSE_UPVALUE: {
                close_upvalues(vm, vm->stack_ptr - 1);
                pop(vm);
                break;
            }
            case OP_LONG: {
                vm->long_instruction = 1;
                break;
            }
            case OP_INHERIT: {
                value superclass = peek(vm, 1);
                object_class *subclass = AS_CLASS(peek(vm, 0));
                if (!IS_CLASS(superclass)) {
                    if(!runtime_error(vm, TYPE_ERROR, "Can only inherit from class.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                hashmap_copy_all(vm, &AS_CLASS(superclass)->methods, &subclass->methods);
                pop(vm); // Pops subclass
                break;
            }
            case OP_TYPEOF: {
                value v = peek(vm, 0);
                switch (v.type) {
                    case NULL_TYPE: pop(vm); push(vm, NULL_VAL); break;
                    case NUM_TYPE: pop(vm); push(vm, TYPE_VAL(TYPEOF_NUM)); break;
                    case BOOL_TYPE: pop(vm); push(vm, TYPE_VAL(TYPEOF_BOOL)); break;
                    case UNDEFINED_TYPE: pop(vm); push(vm, UNDEFINED_VAL); break;
                    case OBJ_TYPE: {
                        switch (GET_OBJ_TYPE(v)) {
                            case OBJ_STRING: pop(vm); push(vm, TYPE_VAL(TYPEOF_STRING)); break;
                            case OBJ_ARRAY: pop(vm); push(vm, TYPE_VAL(TYPEOF_ARRAY)); break;
                            case OBJ_CLASS: pop(vm); push(vm, TYPE_VAL(TYPEOF_CLASS)); break;
                            case OBJ_FUNCTION:
                            case OBJ_BOUND_METHOD:
                            case OBJ_CLOSURE:
                                pop(vm); push(vm, TYPE_VAL(TYPEOF_FUNCTION)); break;
                            case OBJ_NAMESPACE: pop(vm); push(vm, TYPE_VAL(TYPEOF_NAMESPACE)); break;
                            case OBJ_INSTANCE: {
                                object_instance *instance = AS_INSTANCE(v);
                                pop(vm);
                                push(vm, OBJ_VAL(instance->class_));
                                break;
                            }
                            default:
                                if(!runtime_error(vm, TYPE_ERROR, "Unsupported type for 'typeof'.")) return INTERPRET_RUNTIME_ERROR;
                                continue;
                        }
                        break;
                    }
                    default:
                        if(!runtime_error(vm, TYPE_ERROR, "Unsupported type for 'typeof'.")) return INTERPRET_RUNTIME_ERROR;
                        continue;
                }
                break;
            }
            case OP_LEN: {
                value v = peek(vm, 0);
                uint8_t result = 0;
                if (v.type == OBJ_TYPE) {
                    switch (GET_OBJ_TYPE(v)) {
                        case OBJ_STRING: {
                            size_t len = AS_STRING(v)->length;
                            pop(vm);
                            push(vm, NUMBER_VAL((double) len));
                            result = 1;
                            break;
                        }
                        case OBJ_ARRAY: {
                            size_t len = AS_ARRAY(v)->arr.len;
                            pop(vm);
                            push(vm, NUMBER_VAL((double) len));
                            result = 1;
                            break;
                        }
                        case OBJ_INSTANCE: {
                            object_instance *instance = AS_INSTANCE(v);
                            value v;
                            if (hashmap_get(&instance->class_->methods, vm->len_string, &v)) {
                                result = call(vm, AS_CLOSURE(v), 0);
                                vm->active_frame = &vm->frames[vm->frame_count-1];
                            }
                        }
                        default: break;
                    }
                }
                if (!result) {
                    if(!runtime_error(vm, TYPE_ERROR, "Unsupported type for 'len' operator.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                break;
            }
            case OP_UNREGISTER_CATCH: {
                exception_catch *old = vm->catch_stack;
                if (old) {
                    vm->catch_stack = old->next;
                    free(old);
                }
                break;
            }
            case OP_MARK_ERRORS_HANDLED: {
                vm->exception_stack = NULL;
                break;
            }
            case OP_RAISE: {
                value val = pop(vm);
                if (!IS_EXCEPTION(val)) {
                    if (!runtime_error(vm, TYPE_ERROR, "Can only raise exceptions.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                object_exception *exception = AS_EXCEPTION(val);
                if(!raise(vm, exception)) return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_IMPORT: {
                object_string *namespace_name = READ_STRING(READ_VARIABLE_CONST());
                object_string *filename = AS_STRING(peek(vm, 0));
                interpret_result result = import(vm, filename, namespace_name);
                vm->stack_ptr[-2] = vm->stack_ptr[-1];
                pop(vm);                
                if (result != INTERPRET_OK) return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_CONSTANT: {
                push(vm, READ_VARIABLE_CONST());
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
                vm->active_frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_PUSH_TYPEOF: {
                uint8_t arg = READ_BYTE();
                push(vm, TYPE_VAL(arg));
                break;
            }
            case OP_CONV_TYPE: {
                uint8_t arg = READ_BYTE();
                uint8_t result = 0;
                disable_gc(vm);
                switch (arg) {
                    case TYPEOF_NUM: {
                        result = convert_type(vm, to_num, vm->num_string);
                        break;
                    }
                    case TYPEOF_STRING: {
                        result = convert_type(vm, to_str, vm->str_string);
                        break;
                    }
                    case TYPEOF_BOOL: {
                        result = convert_type(vm, to_bool, vm->bool_string);
                        break;
                    }
                    default: break; // Should be unreachable
                }
                enable_gc(vm);
                if (!result) return INTERPRET_RUNTIME_ERROR;
                vm->active_frame = &vm->frames[vm->frame_count-1];
                break;
            }
            case OP_DEFINE_GLOBAL: {
                object_string *name = READ_STRING(READ_VARIABLE_CONST());
                hashmap_set(&vm->globals, vm, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                object_string *name = READ_STRING(READ_VARIABLE_CONST());
                value val;
                if (!hashmap_get(&vm->globals, name, &val)) {
                    if(!runtime_error(vm, NAME_ERROR, "Undefined variable '%s'.", name->chars)) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                push(vm, val);
                break;
            }
            case OP_SET_GLOBAL: {
                object_string *name = READ_STRING(READ_VARIABLE_CONST());
                if(hashmap_set(&vm->globals, vm, name, peek(vm, 0))) {
                    hashmap_delete(&vm->globals, name);
                    if(!runtime_error(vm, NAME_ERROR, "Undefined variable '%s'.", name->chars)) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                break;
            }
            case OP_GET_LOCAL: {
                push(vm, vm->active_frame->slots[READ_VARIABLE_ARG()]);
                break;
            }
            case OP_SET_LOCAL: {
                vm->active_frame->slots[READ_VARIABLE_ARG()] = peek(vm, 0);
                break;
            }
            case OP_GET_UPVALUE: {
                push(vm, *vm->active_frame->closure->upvalues[READ_VARIABLE_ARG()]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                *vm->active_frame->closure->upvalues[READ_VARIABLE_ARG()]->location = peek(vm, 0);
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint64_t offset = READ_UINT40();
                if (is_falsey(peek(vm, 0))) vm->active_frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint64_t offset = READ_UINT40();
                if (!is_falsey(peek(vm, 0))) vm->active_frame->ip += offset;
                break;
            }
            case OP_JUMP: {
                uint64_t offset = READ_UINT40();
                vm->active_frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint64_t offset = READ_UINT40();
                vm->active_frame->ip -= offset;
                break;
            }
            case OP_CLOSURE: {
                object_function *function = AS_FUNCTION(READ_CONSTANT_LONG());
                object_closure *closure = new_closure(vm, function);
                push(vm, OBJ_VAL(closure));
                for (uint32_t i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint32_t index = READ_UINT24();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(vm, vm->active_frame->slots + index);
                    }
                    else {
                        closure->upvalues[i] = vm->active_frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLASS: {
                push(vm, OBJ_VAL(new_class(vm, READ_STRING(READ_VARIABLE_CONST()))));
                break;
            }
            case OP_GET_PROPERTY: {
                value obj = peek(vm, 0);
                if (!(IS_INSTANCE(obj) || IS_NAMESPACE(obj) || IS_EXCEPTION(obj))) {
                    if(!runtime_error(vm, TYPE_ERROR, "Only instances, namespaces and exceptions have properties.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                if (IS_INSTANCE(obj)) {
                    object_instance *instance = AS_INSTANCE(peek(vm, 0));
                    object_string *name = READ_STRING(READ_VARIABLE_CONST());

                    value v;
                    if (hashmap_get(&instance->fields, name, &v)) {
                        pop(vm);
                        push(vm, v);
                        break;
                    }
                    if (!bind_method(vm, instance->class_, name, 1)){
                        push(vm, UNDEFINED_VAL); // Take JS approach of using undefined for non-existent properties
                        break;
                    }
                }
                else if (IS_NAMESPACE(obj)) {
                    object_namespace *namespace = AS_NAMESPACE(obj);
                    object_string *name = READ_STRING(READ_VARIABLE_CONST());
                    value v;
                    if (hashmap_get(&namespace->values, name, &v)) {
                        pop(vm);
                        push(vm, v);
                        break;
                    }
                    else {
                        if(!runtime_error(vm, NAME_ERROR, "Could not find '%s' in namespace '%s'.", name->chars, namespace->name->chars)) return INTERPRET_RUNTIME_ERROR;
                        continue;
                    }
                }
                else if (IS_EXCEPTION(obj)) {
                    object_exception *exception = AS_EXCEPTION(obj);
                    object_string *name = READ_STRING(READ_VARIABLE_CONST());
                    if (name == vm->message_string) {
                        pop(vm);
                        push(vm, OBJ_VAL(exception->message));
                    }
                    else if (name == vm->type_string) {
                        pop(vm);
                        push(vm, ERROR_TYPE_VAL(exception->type));
                    }
                    else {
                        if (!runtime_error(vm, NAME_ERROR, "Exceptions do not have property '%s'.", name->chars)) return INTERPRET_RUNTIME_ERROR;
                        continue;
                    }
                }

                break;
            }
            case OP_GET_PROPERTY_KEEP_REF: {
                value obj = peek(vm, 0);
                if (!(IS_INSTANCE(obj) || IS_NAMESPACE(obj) || IS_EXCEPTION(obj))) {
                    if(!runtime_error(vm, TYPE_ERROR, "Only instances, namespaces and exceptions have properties.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                if (IS_INSTANCE(obj)) {
                    object_instance *instance = AS_INSTANCE(peek(vm, 0));
                    object_string *name = READ_STRING(READ_VARIABLE_CONST());

                    value v;
                    if (hashmap_get(&instance->fields, name, &v)) {
                        push(vm, v);
                        break;
                    }
                    if (!bind_method(vm, instance->class_, name, 1)){
                        push(vm, UNDEFINED_VAL); // Take JS approach of using undefined for non-existent properties
                        break;
                    }
                }
                else if (IS_NAMESPACE(obj)) {
                    object_namespace *namespace = AS_NAMESPACE(obj);
                    object_string *name = READ_STRING(READ_VARIABLE_CONST());
                    value v;
                    if (hashmap_get(&namespace->values, name, &v)) {
                        push(vm, v);
                        break;
                    }
                    else {
                        if(!runtime_error(vm, NAME_ERROR, "Could not find '%s' in namespace '%s'.", name->chars, namespace->name->chars)) return INTERPRET_RUNTIME_ERROR;
                        continue;
                    }
                }
                else if (IS_EXCEPTION(obj)) {
                    object_exception *exception = AS_EXCEPTION(obj);
                    object_string *name = READ_STRING(READ_VARIABLE_CONST());
                    if (name == vm->message_string) {
                        push(vm, OBJ_VAL(exception->message));
                    }
                    else if (name == vm->type_string) {
                        push(vm, ERROR_TYPE_VAL(exception->type));
                    }
                    else {
                        if (!runtime_error(vm, NAME_ERROR, "Exceptions do not have property '%s'.", name->chars)) return INTERPRET_RUNTIME_ERROR;
                        continue;
                    }
                }

                break;
            }
            case OP_SET_PROPERTY: {
                value obj = peek(vm, 1);
                if (IS_EXCEPTION(obj)) {
                    if(!runtime_error(vm, TYPE_ERROR, "Properties of exceptions cannot be set.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                if (!(IS_INSTANCE(obj) || IS_NAMESPACE(obj))) {
                    if(!runtime_error(vm, TYPE_ERROR, "Only instances and namespaces have fields.")) return INTERPRET_RUNTIME_ERROR;
                    continue;
                }
                hashmap *h; // Instance fields or namespace values goes here
                if (IS_INSTANCE(obj)) h = &AS_INSTANCE(obj)->fields;
                else if (IS_NAMESPACE(obj)) h = &AS_NAMESPACE(obj)->values;
                hashmap_set(h, vm, READ_STRING(READ_VARIABLE_CONST()), peek(vm, 0));
                value v = pop(vm);
                pop(vm);
                push(vm, v);
                break;
            }
            case OP_METHOD: {
                define_method(vm, READ_STRING(READ_VARIABLE_CONST()));
                break;
            }
            case OP_INVOKE: {
                object_string *method = READ_STRING(READ_VARIABLE_CONST());
                uint8_t argc = READ_BYTE();
                if (!invoke(vm, method, argc)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm->active_frame = &vm->frames[vm->frame_count - 1];
                break;
            }
            case OP_GET_SUPER: {
                object_string *name = READ_STRING(READ_VARIABLE_CONST());
                object_class *superclass = AS_CLASS(pop(vm));
                if (!bind_method(vm, superclass, name, 0)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_INVOKE_SUPER: {
                object_string *method = READ_STRING(READ_VARIABLE_CONST());
                uint8_t argc = READ_BYTE();
                object_class *superclass = AS_CLASS(pop(vm));
                if (!invoke_from_class(vm, superclass, method, argc)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm->active_frame = &vm->frames[vm->frame_count-1];
                break;
            }
            case OP_REGISTER_CATCH: {
                uint8_t had_error = 0;
                uint8_t num_errors = READ_BYTE();
                size_t catch_addr = READ_UINT48();
                exception_catch *catcher = ALLOCATE(vm, exception_catch, 1);
                for (uint8_t i = 0; i < num_errors; i++) {
                    value val = pop(vm);
                    if (!IS_ERROR_TYPE(val)) {
                        if (!runtime_error(vm, TYPE_ERROR, "Expected an error type in catch statement.")) return INTERPRET_RUNTIME_ERROR;
                        had_error = 1;
                        break;
                    }
                    else catcher->catching_errors[i] = val;
                }
                if (had_error) continue;
                catcher->catch_address = catch_addr;
                catcher->frame_at_try = vm->frame_count;
                catcher->num_errors = num_errors;
                catcher->stack_size_at_try = STACK_LEN(vm);
                catcher->next = vm->catch_stack;
                vm->catch_stack = catcher;
                break;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef READ_CONSTANT_LONG
    #undef READ_UINT24
    #undef READ_UINT40
    #undef READ_UINT48
    #undef READ_UINT56
    #undef READ_STRING
    #undef BINARY_COMPARISON
    #undef BINARY_OP
    #undef READ_VARIABLE_ARG
    #undef READ_VARIABLE_CONST
}

interpret_result interpret(VM *vm, const char *source) {
    object_function *function = compile(source, vm);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(vm, OBJ_VAL(function));
    object_closure *closure = new_closure(vm, function);
    pop(vm);
    push(vm, OBJ_VAL(closure));
    call(vm, closure, 0);

    return run(vm);
}