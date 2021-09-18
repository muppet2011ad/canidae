#ifndef canidae_segment_h
#define canidae_segment_h

#include "common.h"
#include "value.h"

#define UINT24_MAX 16777215
#define UINT40_MAX 1099511627775
#define UINT48_MAX 281474976710655
#define UINT56_MAX 72057594037927935

#define JUMP_OFFSET_LEN 5

typedef enum {
    // No operand
    OP_RETURN,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_POWER,
    OP_UNDEFINED,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_PRINT,
    OP_POP,
    OP_MAKE_ARRAY,
    OP_ARRAY_GET,
    OP_ARRAY_GET_KEEP_REF,
    OP_ARRAY_SET,
    OP_CLOSE_UPVALUE,
    OP_LONG,
    OP_INHERIT,
    OP_TYPEOF,
    OP_LEN,
    OP_UNREGISTER_CATCH,
    OP_MARK_ERRORS_HANDLED,
    // One-byte operand
    OP_POPN,
    OP_CALL,
    OP_PUSH_TYPEOF,
    OP_CONV_TYPE,
    // Three-byte operand 
    // Five-byte operand
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_JUMP,
    OP_LOOP,
    // Variable-length operand
    OP_CLOSURE,
    // Variable-length via OP_LONG (one byte without, three bytes with)
    OP_CONSTANT,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLASS,
    OP_GET_PROPERTY,
    OP_GET_PROPERTY_KEEP_REF,
    OP_SET_PROPERTY,
    OP_METHOD,
    OP_INVOKE, // Variable length with an extra byte for the number of arguments
    OP_GET_SUPER,
    OP_INVOKE_SUPER, // Variable length with an extra byte for the number of arguments
    OP_IMPORT,
    // One-byte operand followed by 6 byte jump address
    OP_REGISTER_CATCH,
} opcode;

typedef struct {
    size_t len;
    size_t capacity;
    uint8_t *bytecode;
    uint32_t *lines;
    value_array constants;
} segment;

void init_segment(segment *s);
void write_to_segment(segment *s, uint8_t byte, uint32_t line);
void write_n_bytes_to_segment(segment *s, uint8_t *bytes, size_t num_bytes, uint32_t line);
void destroy_segment(segment *s);
size_t add_constant(segment *s, value val);

#endif