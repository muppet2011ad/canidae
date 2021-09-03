#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "value.h"
#include "memory.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    token current;
    token prev;
    uint8_t had_error;
    uint8_t panic;
    scanner *s;
} parser;

typedef struct {
    token name;
    long depth;
    uint8_t is_captured;
} local;

typedef struct {
    uint32_t index;
    uint8_t is_local;
} upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
    TYPE_METHOD,
    TYPE_INITIALISER,
} function_type;

typedef struct {
    uint8_t sentinel;
    size_t continue_addr;
    long scope_depth;
    size_t *breaks;
    size_t num_breaks;
    size_t break_capacity;
    size_t *continues; // Only stores do-while continues that are actually jumps forward
    size_t num_continues;
    size_t continue_capacity;
} loop;

typedef struct class_compiler {
    struct class_compiler *enclosing;
} class_compiler;

typedef struct compiler {
    local *locals;
    long local_count;
    uint32_t local_capacity;
    long scope_depth;
    loop *loops;
    long num_loops;
    size_t loop_capacity;
    object_function *function;
    function_type type;
    struct compiler *enclosing;
    uint32_t upvalue_count;
    uint32_t upvalue_capacity;
    upvalue *upvalues;
    class_compiler *current_class;
} compiler;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_POWER,
    PREC_CALL,
    PREC_PRIMARY
} precedence;

typedef void (*parse_func)(parser *p, compiler *c, VM *vm, uint8_t can_assign);

typedef struct {
    parse_func prefix;
    parse_func infix;
    precedence prec;
} parse_rule;

static parse_rule *get_rule(token_type type);
static void expression(parser *p, compiler *c, VM *vm);
static void declaration(parser *p, compiler *c, VM *vm);
static void statement(parser *p, compiler *c, VM *vm);
static uint32_t parse_variable(parser *p, compiler *c, VM *vm, const char *error_message);
static void parse_precedence(parser *p, compiler *c, VM *vm, precedence prec);
static uint32_t identifier_constant(parser *p, compiler *c, VM *vm, token *name);
static long resolve_local(parser *p, compiler *c, token *name);
static void mark_initialised(compiler *c);
static void push_loop_stack(compiler *c, size_t cont_addr, long scope_depth, uint8_t sentinel);
static void pop_loop_stack(compiler *c);
static loop *peek_loop_stack(compiler *c);
static void push_loop_sentinel(compiler *c);
static size_t get_continue_addr(compiler *c);
static uint8_t is_in_loop(compiler *c);
static void add_break(compiler *c, size_t break_addr);
static void add_continue(compiler *c, size_t cont_addr);
static void free_loop(loop l);
static void add_local(parser *p, compiler *c, token name);
static long resolve_upvalue(parser *p, compiler *c, token *name);
static void define_variable(parser *p, compiler *c, uint32_t global);
static uint8_t argument_list(parser *p, compiler *c, VM *vm);
static void declare_variable(parser *p, compiler *c);
static uint8_t handle_assignment(parser *p, compiler *c, VM *vm, uint8_t var_or_arr, uint32_t arg, opcode get_op, opcode set_op);

static void init_compiler(parser *p, compiler *c, VM *vm, function_type type, token *id, uint8_t make_function) {
    c->local_count = 0;
    c->local_capacity = 0;
    c->scope_depth = 0;
    c->loop_capacity = 0;
    c->num_loops = 0;
    c->locals = NULL;
    c->loops = NULL;
    c->function = NULL;
    c->enclosing = NULL;
    c->type = type;
    c->upvalue_capacity = 0;
    c->upvalue_count = 0;
    c->upvalues = NULL;
    c->current_class = NULL;
    if (make_function) c->function = new_function(vm);
    token t; // This section of adding a sentinel local gets a bit more complicated because we have to allocate the locals array
    t.start = "";
    t.length = 0;
    add_local(p, c, t);
    local *l = &c->locals[c->local_count-1];
    l->depth = 0;
    l->is_captured = 0;
    if (type == TYPE_METHOD || type == TYPE_INITIALISER) {
        l->name.start = "this";
        l->name.length = 4;
    }
    if (type != TYPE_SCRIPT) {
        c->function->name = copy_string(vm, p->prev.start, p->prev.length);
    }
}

static void destroy_compiler(compiler *c, VM *vm) {
    FREE_ARRAY(NULL, local, c->locals, c->local_capacity);
    for (size_t i = 0; i < c->num_loops; i++) {
        free_loop(c->loops[i]);
    }
    FREE_ARRAY(NULL, loop, c->loops, c->loop_capacity);
    FREE_ARRAY(NULL, upvalue, c->upvalues, c->upvalue_capacity);
    init_compiler(NULL, c, vm, TYPE_SCRIPT, NULL, 0);
    free(c->locals);
}

static segment *current_seg(compiler *c) {
    return &c->function->seg;
}

static void init_parser(parser *p, scanner *s) {
    p->s = s;
    p->had_error = 0;
    p->panic = 0;
}

static void error_at(parser *p, token *t, const char *message) {
    if (p->panic) return;
    p->panic = 1;
    fprintf(stderr, "[line %u] Error", t->line);
    if (t->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    }
    else if (t->type == TOKEN_ERROR) {

    }
    else {
        fprintf(stderr, " at '%.*s'", t->length, t->start);
    }

    fprintf(stderr, ": %s\n", message);
    p->had_error = 1;
}

static void error(parser *p, const char *message) {
    error_at(p, &p->prev, message);
}

static void error_at_current(parser *p, const char *message) {
    error_at(p, &p->current, message);
}

static uint32_t make_constant(parser *p, compiler *c, value val) {
    size_t constant = add_constant(current_seg(c), val);
    if (constant > UINT24_MAX) {
        error(p, "Too many constants in one segment.");
        return 0;
    }
    return (uint32_t) constant;
}

static void advance(parser *p) {
    p->prev = p->current;

    for (;;) {
        p->current = scan_token(p->s);
        if (p->current.type != TOKEN_ERROR) break;
        error_at_current(p, p->current.start);
    }
}

static void consume(parser *p, token_type type, const char *message) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error_at_current(p, message);
}

static uint8_t check(parser *p, token_type type) {
    return p->current.type == type;
}

static uint8_t match(parser *p, token_type type) {
    if (!check(p, type)) return 0;
    advance(p);
    return 1;
}

static void emit_byte(parser *p, compiler *c, uint8_t byte) {
    write_to_segment(current_seg(c), byte, p->prev.line);
}

static void emit_2_bytes(parser *p, compiler *c, uint8_t x, uint8_t y) {
    emit_byte(p, c, x);
    emit_byte(p, c, y);
}

static void emit_bytes(parser *p, compiler *c, uint8_t *bytes, size_t n) {
    write_n_bytes_to_segment(current_seg(c), bytes, n, p->prev.line);
}

static uint8_t emit_variable_length_instruction(parser *p, compiler *c, opcode op, uint32_t operand) {
    if (operand <= UINT8_MAX) {
        emit_2_bytes(p, c, op, operand);
        return 1;
    }
    else if (operand <= UINT24_MAX) {
        uint8_t bytes[5] = {OP_LONG, op, operand >> 16, operand >> 8, operand};
        emit_bytes(p, c, bytes, 5);
        return 1;
    }
    else {
        return 0;
    }
}

static size_t emit_jump(parser *p, compiler *c, uint8_t instruction) {
    uint8_t bytes[6] = {instruction, 0xff, 0xff, 0xff, 0xff, 0xff};
    emit_bytes(p, c, bytes, 6);
    return current_seg(c)->len - JUMP_OFFSET_LEN;
}

static void emit_loop(parser *p, compiler *c, size_t loop_start) {
    size_t offset = current_seg(c)->len - loop_start + JUMP_OFFSET_LEN + 1;
    if (offset > UINT40_MAX) {
        error(p, "Too much code to jump over.");
    }
    uint8_t bytes[6] = {OP_LOOP, (uint8_t) (offset >> 32), (uint8_t) (offset >> 24), (uint8_t) (offset >> 16), (uint8_t) (offset >> 8), (uint8_t) offset};
    emit_bytes(p, c, bytes, 6);
}

static void emit_return(parser *p, compiler *c) {
    if (c->type == TYPE_INITIALISER) {
        emit_variable_length_instruction(p, c, OP_GET_LOCAL, 0);
    }
    else {
        emit_byte(p, c, OP_NULL);
    }
    emit_byte(p, c, OP_RETURN);
}

static void emit_constant(parser *p, compiler *c, value v) {
    uint32_t constant = add_constant(current_seg(c), v);
    if (!emit_variable_length_instruction(p, c, OP_CONSTANT, constant)) {
        error(p, "Too many constants in one chunk.");
    }
}

static void patch_jump(parser *p, compiler *c, size_t offset) {
    long jump = current_seg(c)->len - offset - JUMP_OFFSET_LEN;
    if (jump > UINT40_MAX) {
        error(p, "Too much code to jump over.");
    }

    current_seg(c)->bytecode[offset] = (uint8_t) (jump >> 32);
    current_seg(c)->bytecode[offset+1] = (uint8_t) (jump >> 24);
    current_seg(c)->bytecode[offset+2] = (uint8_t) (jump >> 16);
    current_seg(c)->bytecode[offset+3] = (uint8_t) (jump >> 8);
    current_seg(c)->bytecode[offset+4] = (uint8_t) jump;
}

static void patch_breaks(parser *p, compiler *c) {
    loop *l = peek_loop_stack(c);
    for (size_t i = 0; i < l->num_breaks; i++) {
        patch_jump(p, c, l->breaks[i]);
    }
}

static void patch_continues(parser *p, compiler *c) {
    loop *l = peek_loop_stack(c);
    for (size_t i = 0; i < l->num_continues; i++) {
        patch_jump(p, c, l->continues[i]);
    }
}

static void exit_loop_scope(parser *p, compiler *c) {
    loop *l = peek_loop_stack(c);
    long to_pop = 0;
    long local_count = c->local_count;
    while (local_count > 0 && c->locals[local_count-1].depth > l->scope_depth) {
        to_pop++;
        local_count--;
    }
    for (; to_pop > 0; to_pop-=255) {
        if (to_pop >= 255) {
            emit_2_bytes(p, c, OP_POPN, 255);
        }
        else {
            emit_2_bytes(p, c, OP_POPN, to_pop);
        }
    }
}

static object_function *end_compiler(parser *p, compiler *c) {
    emit_return(p, c);
    object_function *f = c->function;
    f->upvalue_count = c->upvalue_count;
    #ifdef DEBUG_PRINT_CODE
        if (!p->had_error) {
            disassemble_segment(current_seg(c), f->name != NULL ? f->name->chars : "<script>");
        }
    #endif
    return f;
}

static void begin_scope(compiler *c) {
    c->scope_depth++;
}

static void end_scope(parser *p, compiler *c) {
    c->scope_depth--;

    while (c->local_count > 0 && c->locals[c->local_count-1].depth > c->scope_depth) {
        if (c->locals[c->local_count-1].is_captured) {
            emit_byte(p, c, OP_CLOSE_UPVALUE);
            c->local_count--;
            continue;
        }
        long to_pop = 0;
        while (c->local_count > 0 && c->locals[c->local_count-1].depth > c->scope_depth && !c->locals[c->local_count-1].is_captured) {
            to_pop++;
            c->local_count--;
        }
        for (; to_pop > 0; to_pop-=255) {
            if (to_pop >= 255) {
                emit_2_bytes(p, c, OP_POPN, 255);
            }
            else {
                emit_2_bytes(p, c, OP_POPN, to_pop);
            }
        }
    }
}

static void binary(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    token_type operator = p->prev.type;
    parse_rule *rule = get_rule(operator);
    parse_precedence(p, c, vm, rule->prec + 1);
    switch (operator) {
        case TOKEN_PLUS: emit_byte(p, c, OP_ADD); break;
        case TOKEN_MINUS: emit_byte(p, c, OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(p, c, OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(p, c, OP_DIVIDE); break;
        case TOKEN_CARET: emit_byte(p, c, OP_POWER); break;
        case TOKEN_BANG_EQUAL: emit_2_bytes(p, c, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emit_byte(p, c, OP_EQUAL); break;
        case TOKEN_GREATER: emit_byte(p, c, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_byte(p, c, OP_GREATER_EQUAL); break;
        case TOKEN_LESS: emit_byte(p, c, OP_LESS); break;
        case TOKEN_LESS_EQUAL: emit_byte(p, c, OP_LESS_EQUAL); break;
        default: return;
    }
}

static void call(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    uint8_t argc = argument_list(p, c, vm);
    emit_2_bytes(p, c, OP_CALL, argc);
}

static void dot(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    consume(p, TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint32_t property_name = identifier_constant(p, c, vm, &p->prev);

    uint8_t assigned = 0;
    if (can_assign) {
        if (!check(p, TOKEN_EQUAL)) {
            assigned |= handle_assignment(p, c, vm, 0, property_name, OP_GET_PROPERTY_KEEP_REF, OP_SET_PROPERTY);
        }
        else {
            assigned |= handle_assignment(p, c, vm, 0, property_name, OP_GET_PROPERTY, OP_SET_PROPERTY);
        }
    } 
    if (!assigned) {
        if (match(p, TOKEN_LEFT_PAREN)) {
            uint8_t argc = argument_list(p, c, vm);
            emit_variable_length_instruction(p, c, OP_INVOKE, property_name);
            emit_byte(p, c, argc);
        }
        else {
            emit_variable_length_instruction(p, c, OP_GET_PROPERTY, property_name);
        }
    }
}

static void literal(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    switch (p->prev.type) {
        case TOKEN_FALSE: emit_byte(p, c, OP_FALSE); break;
        case TOKEN_NULL: emit_byte(p, c, OP_NULL); break;
        case TOKEN_TRUE: emit_byte(p, c, OP_TRUE); break;
        case TOKEN_UNDEFINED: emit_byte(p, c, OP_UNDEFINED); break;
        default: return;
    }
}

static void number(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    double num = strtod(p->prev.start, NULL);
    emit_constant(p, c, NUMBER_VAL(num));
}

static void string(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    emit_constant(p, c, OBJ_VAL(copy_string(vm, p->prev.start + 1, p->prev.length -2)));
}

static void assign_with_op(parser *p, compiler *c, VM *vm, uint8_t var_or_arr, uint32_t arg, opcode get_op, opcode set_op, opcode op, uint8_t can_eval_expr) {
    if (var_or_arr) {
        emit_byte(p, c, get_op);
    }
    else {
        emit_variable_length_instruction(p, c, get_op, arg);
    }
    if (can_eval_expr) {
        expression(p, c, vm);
    } else {
        emit_constant(p, c, NUMBER_VAL(1));
    }
    emit_byte(p, c, op);
    if (var_or_arr) {
        emit_byte(p, c, set_op);
    }
    else {
        emit_variable_length_instruction(p, c, set_op, arg);
    }
}

static uint8_t handle_assignment(parser *p, compiler *c, VM *vm, uint8_t var_or_arr, uint32_t arg, opcode get_op, opcode set_op) {
    if (match(p, TOKEN_EQUAL)) {
        expression(p, c, vm);
        if (var_or_arr) {
            emit_byte(p, c, set_op);
        } 
        else {
            emit_variable_length_instruction(p, c, set_op, arg);
        }
        return 1;
    }
    else if (match(p, TOKEN_PLUS_EQUAL)) {
        assign_with_op(p, c, vm, var_or_arr, arg, get_op, set_op, OP_ADD, 1);
        return 1;
    }
    else if (match(p, TOKEN_MINUS_EQUAL)) {
        assign_with_op(p, c, vm, var_or_arr, arg, get_op, set_op, OP_SUBTRACT, 1);
        return 1;
    }
    else if (match(p, TOKEN_STAR_EQUAL)) {
        assign_with_op(p, c, vm, var_or_arr, arg, get_op, set_op, OP_MULTIPLY, 1);
        return 1;
    }
    else if (match(p, TOKEN_SLASH_EQUAL)) {
        assign_with_op(p, c, vm, var_or_arr, arg, get_op, set_op, OP_DIVIDE, 1);
        return 1;
    }
    else if (match(p, TOKEN_CARET_EQUAL)) {
        assign_with_op(p, c, vm, var_or_arr, arg, get_op, set_op, OP_POWER, 1);
        return 1;
    }
    else if (match(p, TOKEN_PLUS_PLUS)) {
        assign_with_op(p, c, vm, var_or_arr, arg, get_op, set_op, OP_ADD, 0);
        return 1;
    }
    else if (match(p, TOKEN_MINUS_MINUS)) {
        assign_with_op(p, c, vm, var_or_arr, arg, get_op, set_op, OP_SUBTRACT, 0);
        return 1;
    }
    return 0;
}

static void named_variable(parser *p, compiler *c, VM *vm, token name, uint8_t can_assign) {
    uint8_t get_op, set_op;
    long arg_long = resolve_local(p, c, &name);
    uint32_t arg = (uint32_t) arg_long;
    if (arg_long != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else if ((arg_long = resolve_upvalue(p, c, &name)) != -1) {
        arg = (uint32_t) arg_long;
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    }
    else {
        arg = identifier_constant(p, c, vm, &name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }
    uint8_t assigned = 0;
    if (can_assign) {
        assigned = handle_assignment(p, c, vm, 0, arg, get_op, set_op);
    }
    if (!assigned) {
        emit_variable_length_instruction(p, c, get_op, arg);
    }
}

static void variable(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    named_variable(p, c, vm, p->prev, can_assign);
}

static void this_(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    if (c->current_class == NULL) {
        error(p, "Can't use 'this' outside of a class.");
        return;
    }
    variable(p, c, vm, 0);
}

static void unary(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    token_type operator = p->prev.type;
    parse_precedence(p, c, vm, PREC_UNARY);
    switch (operator) {
        case TOKEN_MINUS: emit_byte(p, c, OP_NEGATE); break;
        case TOKEN_BANG: emit_byte(p, c, OP_NOT); break;
        default: return;
    }
}

static void grouping(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    expression(p, c, vm);
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void expression(parser *p, compiler *c, VM *vm) {
    parse_precedence(p, c, vm, PREC_ASSIGNMENT);
}

static void expression_statement(parser *p, compiler *c, VM *vm) {
    expression(p, c, vm);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(p, c, OP_POP);
}

static void if_statement(parser *p, compiler *c, VM *vm) {
    expression(p, c, vm);
    consume(p, TOKEN_THEN, "Expect 'then' after condition.");
    size_t then_jump = emit_jump(p, c, OP_JUMP_IF_FALSE);
    emit_byte(p, c, OP_POP);
    statement(p, c, vm);

    size_t else_jump = emit_jump(p, c, OP_JUMP);

    patch_jump(p, c, then_jump);
    emit_byte(p, c, OP_POP);

    if (match(p, TOKEN_ELSE)) {
        statement(p, c, vm);
    }
    patch_jump(p, c, else_jump);
}

static void while_statement(parser *p, compiler *c, VM *vm) {
    size_t loop_start = current_seg(c)->len;
    push_loop_stack(c, loop_start, c->scope_depth, 0);
    expression(p, c, vm);
    consume(p, TOKEN_DO, "Expect 'do' after loop condition.");

    size_t skip_loop_jump = emit_jump(p, c, OP_JUMP_IF_FALSE);
    emit_byte(p, c, OP_POP);
    statement(p, c, vm);
    emit_loop(p, c, loop_start);
    patch_jump(p, c, skip_loop_jump);
    emit_byte(p, c, OP_POP);
    patch_breaks(p, c);
    pop_loop_stack(c);
}

static void do_statement(parser *p, compiler *c, VM *vm) {
    size_t loop_start = current_seg(c)->len;
    push_loop_stack(c, SIZE_MAX, c->scope_depth, 0);
    statement(p, c, vm);
    consume(p, TOKEN_WHILE, "Expect 'while' after do loop body.");
    patch_continues(p, c);
    expression(p, c, vm);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    size_t exit_jump = emit_jump(p, c, OP_JUMP_IF_FALSE);
    emit_byte(p, c, OP_POP);
    emit_loop(p, c, loop_start);
    patch_jump(p, c, exit_jump);
    emit_byte(p, c, OP_POP);
    patch_breaks(p, c);
    pop_loop_stack(c);
}

static void continue_statement(parser *p, compiler *c, VM *vm) {
    if (!is_in_loop(c)) {
        error(p, "Unexpected 'break' outside of loop body.");
        return;
    }
    exit_loop_scope(p, c);
    if (get_continue_addr(c) == SIZE_MAX) {
        add_continue(c, emit_jump(p, c, OP_JUMP));
    }
    else {
        emit_loop(p, c, get_continue_addr(c));
    }
    consume(p, TOKEN_SEMICOLON, "Expected ';' at end of break statement.");
}

static void break_statement(parser *p, compiler *c, VM *vm) {
    if (!is_in_loop(c)) {
        error(p, "Unexpected 'break' outside of loop body.");
        return;
    }
    exit_loop_scope(p, c);
    add_break(c, emit_jump(p, c, OP_JUMP));
    consume(p, TOKEN_SEMICOLON, "Expected ';' at end of break statement.");
}

static void array_dec(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    size_t nmeb = 0;
    while (!check(p, TOKEN_RIGHT_SQR) && !check(p, TOKEN_EOF)) {
        expression(p, c, vm);
        nmeb++;
        if (!check(p, TOKEN_RIGHT_SQR)) {
            consume(p, TOKEN_COMMA, "Expect ',' to separate array elements.");
        }
        if (nmeb + 1 == 0) {
            error(p, "Too many elements in array.");
        }
    }
    consume(p, TOKEN_RIGHT_SQR, "Expect ']' after array.");
    emit_constant(p, c, NUMBER_VAL((double) nmeb));
    emit_byte(p, c, OP_MAKE_ARRAY);
}

static void array_index(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    if (check(p, TOKEN_RIGHT_BRACE) || check(p, TOKEN_EOF)) {
        error(p, "Expected array index.");
    }
    expression(p, c, vm);
    consume(p, TOKEN_RIGHT_SQR, "Expect ']' after array index.");
    uint8_t assigned = 0;
    if (can_assign) {
        if (!check(p, TOKEN_EQUAL)) {
            assigned |= handle_assignment(p, c, vm, 1, -1, OP_ARRAY_GET_KEEP_REF, OP_ARRAY_SET);
        }
        else {
            assigned |= handle_assignment(p, c, vm, 1, -1, OP_ARRAY_GET, OP_ARRAY_SET);
        }
    } 
    if (!assigned) {
        emit_byte(p, c, OP_ARRAY_GET);
    }
}

static void block(parser *p, compiler *c, VM *vm) {
    while (!check(p, TOKEN_RIGHT_BRACE) && !check(p, TOKEN_EOF)) {
        declaration(p, c, vm);
    }
    consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(parser *p, compiler *c, VM *vm, function_type type) {
    compiler function_compiler;
    init_compiler(p, &function_compiler, vm, type, &p->prev, 1);
    function_compiler.enclosing = c;
    function_compiler.current_class = c->current_class;
    begin_scope(&function_compiler);
    mark_initialised(&function_compiler);
    consume(p, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(p, TOKEN_RIGHT_PAREN)) {
        do {
            function_compiler.function->arity++;
            if (function_compiler.function->arity == 0) {
                error_at_current(p, "Can't have more than 255 parameters.");
            }
            uint32_t constant = parse_variable(p, &function_compiler, vm, "Expect parameter name.");
            define_variable(p, &function_compiler, constant);
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(p, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(p, &function_compiler, vm);
    object_function *function = end_compiler(p, &function_compiler);
    uint32_t constant = make_constant(p, c, OBJ_VAL(function));
    uint8_t bytes[4] = {OP_CLOSURE, constant >> 16, constant >> 8, constant};
    emit_bytes(p, c, bytes, 4);
    for (uint32_t i = 0; i < function->upvalue_count; i++) {
        upvalue upval = function_compiler.upvalues[i];
        uint8_t bytes[4] = {upval.is_local ? 1 : 0, upval.index << 16, upval.index << 8, upval.index};
        emit_bytes(p, c, bytes, 4);
    }
    destroy_compiler(&function_compiler, vm);
}

static void method(parser *p, compiler *c, VM *vm) {
    consume(p, TOKEN_FUNCTION, "Expect function declaration in class.");
    consume(p, TOKEN_IDENTIFIER, "Expect method name.");
    uint32_t method_name = identifier_constant(p, c, vm, &p->prev);

    function_type type = TYPE_METHOD;

    if (p->prev.length == 8 && memcmp(p->prev.start, "__init__", 8) == 0) type = TYPE_INITIALISER;

    function(p, c, vm, type);
    emit_variable_length_instruction(p, c, OP_METHOD, method_name);
}

static void class_declaration(parser *p, compiler *c, VM *vm) {
    consume(p, TOKEN_IDENTIFIER, "Expect class name.");
    token class_name = p->prev;
    uint32_t class_name_constant = identifier_constant(p, c, vm, &p->prev);
    declare_variable(p, c);

    emit_variable_length_instruction(p, c, OP_CLASS, class_name_constant);
    define_variable(p, c, class_name_constant);

    class_compiler class_c;
    class_c.enclosing = c->current_class;
    c->current_class = &class_c;

    named_variable(p, c, vm, class_name, 0);

    consume(p, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(p, TOKEN_RIGHT_BRACE) && !check(p, TOKEN_EOF)) {
        method(p, c, vm);
    }
    consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emit_byte(p, c, OP_POP);
    c->current_class = c->current_class->enclosing;
}

static void function_declaration(parser *p, compiler *c, VM *vm) {
    uint32_t global = parse_variable(p, c, vm, "Expect function name.");
    mark_initialised(c);
    function(p, c, vm, TYPE_FUNCTION);
    define_variable(p, c, global);
}

static void define_variable(parser *p, compiler *c, uint32_t global) {
    if (c->scope_depth > 0) {
        mark_initialised(c);
        return;
    }
    emit_variable_length_instruction(p, c, OP_DEFINE_GLOBAL, global);
}

static uint8_t argument_list(parser *p, compiler *c, VM *vm) {
    uint8_t argc = 0;
    if (!check(p, TOKEN_RIGHT_PAREN)) {
        do {
            if (argc == 255) error(p, "Can't have more than 255 arguments.");
            expression(p, c, vm);
            argc++;
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argc;
}

static void and_(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    size_t end_jump = emit_jump(p, c, OP_JUMP_IF_FALSE);
    emit_byte(p, c, OP_POP);
    parse_precedence(p, c, vm, PREC_AND);
    patch_jump(p, c, end_jump);
}

static void or_(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    size_t else_jump = emit_jump(p, c, OP_JUMP_IF_TRUE);
    emit_byte(p, c, OP_POP);
    parse_precedence(p, c, vm, PREC_OR);
    patch_jump(p, c, else_jump);
}

static void var_declaration(parser *p, compiler *c, VM *vm) {
    uint32_t global = parse_variable(p, c, vm, "Expect variable name.");

    if (match(p, TOKEN_EQUAL)) {
        expression(p, c, vm);
    } else {
        emit_byte(p, c, OP_NULL);
    }
    if (match(p, TOKEN_LEFT_SQR)) {
        error(p, "Cannot declare array member as variable.");
    }
    consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration,");
    define_variable(p, c, global);
}

static void for_statement(parser *p, compiler *c, VM *vm) {
    begin_scope(c);
    if (match(p, TOKEN_SEMICOLON)) {
        // No initialiser
    }
    else if (match(p, TOKEN_LET)) {
        var_declaration(p, c, vm);
    }
    else {
        expression_statement(p, c, vm);
    }

    size_t loop_start = current_seg(c)->len;
    long exit_jump = -1;
    if (!match(p, TOKEN_SEMICOLON)) {
        expression(p, c, vm); // Evaluate condition
        consume(p, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exit_jump = emit_jump(p, c, OP_JUMP_IF_FALSE); // Emit jump to leave loop
        emit_byte(p, c, OP_POP); // Pop result of condition expression
    }
    
    if (!match(p, TOKEN_DO)) {
        size_t body_jump = emit_jump(p, c, OP_JUMP);
        size_t increment_start = current_seg(c)->len;
        expression(p, c, vm);
        emit_byte(p, c, OP_POP);
        consume(p, TOKEN_DO, "Expect 'do' after for clauses.");
        emit_loop(p, c, loop_start);
        loop_start = increment_start;
        patch_jump(p, c, body_jump);
    }

    push_loop_stack(c, loop_start, c->scope_depth, 0);

    statement(p, c, vm);
    emit_loop(p, c, loop_start);
    if (exit_jump != -1) {
        patch_jump(p, c, (size_t) exit_jump);
        emit_byte(p, c, OP_POP);
    }
    patch_breaks(p, c);
    pop_loop_stack(c);
    end_scope(p, c);
}


static void print_statement(parser *p, compiler *c, VM *vm) {
    expression(p, c, vm);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(p, c, OP_PRINT);
}

static void return_statement(parser *p, compiler *c, VM *vm) {
    if (c->type == TYPE_SCRIPT) {
        error(p, "Can't return outside of a function.");
    }
    if (match(p, TOKEN_SEMICOLON)) {
        emit_return(p, c);
    }
    else {
        if (c->type == TYPE_INITIALISER) {
            error(p, "Can't return a value from an initialiser.");
        }
        expression(p, c, vm);
        consume(p, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(p, c, OP_RETURN);
    }
}

static void synchronise(parser *p) {
    p->panic = 0;

    while (p->current.type != TOKEN_EOF) {
        if (p->prev.type == TOKEN_SEMICOLON) return;
        switch (p->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUNCTION:
            case TOKEN_LET:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:;
        }
        advance(p);
    }
}

static void declaration(parser *p, compiler *c, VM *vm) {
    if (match(p, TOKEN_CLASS)) {
        class_declaration(p, c, vm);
    }
    else if (match(p, TOKEN_FUNCTION)) {
        function_declaration(p, c, vm);
    }
    else if (match(p, TOKEN_LET)) {
        var_declaration(p, c, vm);
    }
    else {
        statement(p, c, vm);
    }

    if (p->panic) synchronise(p);
}

static void statement(parser *p, compiler *c, VM *vm) {
    if (match(p, TOKEN_PRINT)) {
        print_statement(p, c, vm);
    } 
    else if (match(p, TOKEN_LEFT_BRACE)) {
        begin_scope(c);
        block(p, c, vm);
        end_scope(p, c);

    } 
    else if (match(p, TOKEN_WHILE)) {
        while_statement(p, c, vm);
    }
    else if (match(p, TOKEN_FOR)) {
        for_statement(p, c, vm);
    }
    else if (match(p, TOKEN_DO)) {
        do_statement(p, c, vm);
    }
    else if (match(p, TOKEN_IF)) {
        if_statement(p, c, vm);
    } 
    else if (match(p, TOKEN_RETURN)) {
        return_statement(p, c, vm);
    }
    else if (match(p, TOKEN_CONTINUE)) {
        continue_statement(p, c, vm);
    }
    else if (match(p, TOKEN_BREAK)) {
        break_statement(p, c, vm);
    }
    else {
        expression_statement(p, c, vm);
    }
}

parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_SQR] = {array_dec, array_index, PREC_PRIMARY},
    [TOKEN_RIGHT_SQR] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_CARET] = {NULL, binary, PREC_POWER},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUNCTION] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NULL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONST] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_UNDEFINED] = {literal, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parse_precedence(parser *p, compiler *c, VM *vm, precedence prec) {
    advance(p);
    parse_func prefix_rule = get_rule(p->prev.type)->prefix;
    if (prefix_rule == NULL) {
        error(p, "Expect expression.");
        return;
    }

    uint8_t can_assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(p, c, vm, can_assign);

    while(prec <= get_rule(p->current.type)->prec) {
        advance(p);
        parse_func infix_rule = get_rule(p->prev.type)->infix;
        infix_rule(p, c, vm, can_assign);
    }

    if (can_assign && match(p, TOKEN_EQUAL)) {
        error(p, "Invalid assignment target.");
    }
}

static uint32_t identifier_constant(parser *p, compiler *c, VM *vm, token *name) {
    // Extra code so that new constants aren't required for every use of a variable
    segment *seg = current_seg(c);
    for (uint32_t i = 0; i < seg->constants.len; i++) {
        if (IS_STRING(seg->constants.values[i])) {
            char *const_string = AS_CSTRING(seg->constants.values[i]);
            size_t len = AS_STRING(seg->constants.values[i])->length;
            if (name->length == len && memcmp(const_string, name->start, len) == 0) {
               return i;
            }
        }
    }
    return make_constant(p, c, OBJ_VAL(copy_string(vm, name->start, name->length)));
}

static uint8_t identifiers_equal(token *a, token *b) {
    return a->length == b->length && memcmp(a->start, b->start, a->length) == 0;
}

static long resolve_local(parser *p, compiler *c, token *name) {
    for (long i = c->local_count - 1; i >= 0; i--) {
        local *l = &c->locals[i];
        if (identifiers_equal(name, &l->name)) {
            if (l->depth == -1) {
                error(p, "Can't read local variable in its own initaliser.");
            }
            return i;
        }
    }

    return -1;
}

static long add_upvalue(parser *p, compiler *c, uint32_t index, uint8_t is_local) {
    if (c->upvalue_count == UINT24_MAX) {
        error(p, "Too many closure variables in function.");
        return 0;
    }
    if (c->upvalue_count >= c->upvalue_capacity) {
        uint32_t oldc = c->upvalue_capacity;
        c->upvalue_capacity = GROW_CAPACITY(oldc);
        c->upvalues = GROW_ARRAY(NULL, upvalue, c->upvalues, oldc, c->upvalue_capacity);
    }
    for (uint32_t i = 0; i < c->upvalue_count; i++) {
        upvalue *upval = &c->upvalues[i];
        if (upval->index == index && upval->is_local == is_local) {
            return i;
        }
    }
    c->upvalues[c->upvalue_count].is_local = is_local;
    c->upvalues[c->upvalue_count].index = index;
    return c->upvalue_count++;
}

static long resolve_upvalue(parser *p, compiler *c, token *name) {
    if (c->enclosing == NULL) return -1;
    long local  = resolve_local(p, c->enclosing, name);
    if (local != -1) {
        c->enclosing->locals[local].is_captured = 1;
        return add_upvalue(p, c, (uint32_t) local, 1);
    }
    long upval = resolve_upvalue(p, c->enclosing, name);
    if (upval != -1) {
        return add_upvalue(p, c, (uint32_t) upval, 0);
    }

    return -1;
}

static void add_local(parser *p, compiler *c, token name) {
    // Extra code for allowing more locals
    if (c->local_count > UINT24_MAX) {
        error(p, "Too many local variables in function.");
        return;
    }
    if (c->local_count >= c->local_capacity) {
        uint32_t oldc = c->local_capacity;
        c->local_capacity = GROW_CAPACITY(oldc);
        c->locals = GROW_ARRAY(NULL, local, c->locals, oldc, c->local_capacity);
    }
    local *l = &c->locals[c->local_count++];
    l->name = name;
    l->depth = -1;
    l->is_captured = 0;
}

static void push_loop_stack(compiler *c, size_t cont_addr, long scope_depth, uint8_t sentinel) {
    if (c->num_loops >= c->loop_capacity) {
        size_t oldc = c->loop_capacity;
        c->loop_capacity = GROW_CAPACITY(oldc);
        c->loops = GROW_ARRAY(NULL, loop, c->loops, oldc, c->loop_capacity);
    }
    loop l;
    l.continue_addr = cont_addr;
    l.scope_depth = scope_depth;
    l.sentinel = sentinel;
    l.breaks = NULL;
    l.break_capacity = 0;
    l.num_breaks = 0;
    l.continues = NULL;
    l.continue_capacity = 0;
    l.num_continues = 0;
    c->loops[c->num_loops] = l;
    c->num_loops++;
}

static void pop_loop_stack(compiler *c) {
    c->num_loops--;
    free_loop(c->loops[c->num_loops]);
}

static loop *peek_loop_stack(compiler *c) {
    return &c->loops[c->num_loops-1];
}

static void push_loop_sentinel(compiler *c) {
    push_loop_stack(c, 0, 0, 1);
}

static size_t get_continue_addr(compiler *c) {
    loop l = *peek_loop_stack(c);
    if (l.sentinel == 1) {
        return SIZE_MAX;
    }
    return l.continue_addr;

}

static uint8_t is_in_loop(compiler *c) {
    return c->num_loops > 0 && !peek_loop_stack(c)->sentinel;
}

static void add_break(compiler *c, size_t break_addr) {
    loop *l = peek_loop_stack(c);
    if (l->num_breaks >= l->break_capacity) {
        size_t oldc = l->break_capacity;
        l->break_capacity = GROW_CAPACITY(oldc);
        l->breaks = GROW_ARRAY(NULL, size_t, l->breaks, oldc, l->break_capacity);
    }
    l->breaks[l->num_breaks] = break_addr;
    l->num_breaks++;
}

static void add_continue(compiler *c, size_t cont_addr) {
    loop *l = peek_loop_stack(c);
    if (l->num_continues >= l->continue_capacity) {
        size_t oldc = l->continue_capacity;
        l->continue_capacity = GROW_CAPACITY(oldc);
        l->continues = GROW_ARRAY(NULL, size_t, l->continues, oldc, l->continue_capacity);
    }
    l->continues[l->num_continues] = cont_addr;
    l->num_continues++;
}

static void free_loop(loop l) {
    FREE_ARRAY(NULL, size_t, l.breaks, l.break_capacity);
    FREE_ARRAY(NULL, size_t, l.continues, l.continue_capacity);
}

static void declare_variable(parser *p, compiler *c) {
    if (c->scope_depth == 0) return;
    token *name = &p->prev;
    for (long i = (long) c->local_count - 1; i >= 0; i--) {
        local *l = &c->locals[i];
        if (l->depth != -1 && l->depth < c->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &l->name)) {
            error(p, "Already a variable with this name in this scope.");
        }
    }
    add_local(p, c, *name);
}

static uint32_t parse_variable(parser *p, compiler *c, VM *vm, const char *error_message) {
    consume(p, TOKEN_IDENTIFIER, error_message);

    declare_variable(p, c);
    if (c->scope_depth > 0) return 0;

    return identifier_constant(p, c, vm, &p->prev);
}

static void mark_initialised(compiler *c) {
    if (c->scope_depth == 0) return;
    c->locals[c->local_count - 1].depth = c->scope_depth;
}

static parse_rule *get_rule(token_type type) {
    return &rules[type];
}

object_function *compile(const char* source, VM *vm) {
    scanner s;
    parser p;
    compiler c;
    init_scanner(&s, source);
    init_parser(&p, &s);
    init_compiler(&p, &c, vm, TYPE_SCRIPT, NULL, 1);
    disable_gc(vm);
    advance(&p);
    while (!match(&p, TOKEN_EOF)) {
        declaration(&p, &c, vm);
    }
    consume(&p, TOKEN_EOF, "Expect end of expression.");
    object_function *f = end_compiler(&p, &c);
    destroy_compiler(&c, vm);
    enable_gc(vm);
    return p.had_error ? NULL : f;
}
