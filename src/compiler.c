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
} local;

typedef struct {
    local *locals;
    long local_count;
    uint32_t local_capacity;
    long scope_depth;
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

segment *compiling_segment;

static void init_compiler(compiler *c) {
    c->local_count = 0;
    c->local_capacity = 0;
    c->scope_depth = 0;
    c->locals = NULL;
}

static void destroy_compiler(compiler *c) {
    FREE_ARRAY(local, c->locals, c->local_capacity);
    init_compiler(c);
}

static segment *current_seg() {
    return compiling_segment;
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

static uint32_t make_constant(parser *p, value val) {
    size_t constant = add_constant(current_seg(), val);
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

static void emit_byte(parser *p, uint8_t byte) {
    write_to_segment(current_seg(), byte, p->prev.line);
}

static void emit_2_bytes(parser *p, uint8_t x, uint8_t y) {
    emit_byte(p, x);
    emit_byte(p, y);
}

static void emit_bytes(parser *p, uint8_t *bytes, size_t n) {
    write_n_bytes_to_segment(current_seg(), bytes, n, p->prev.line);
}

static size_t emit_jump(parser *p, uint8_t instruction) {
    uint8_t bytes[6] = {instruction, 0xff, 0xff, 0xff, 0xff, 0xff};
    emit_bytes(p, bytes, 6);
    return current_seg()->len - JUMP_OFFSET_LEN;
}

static void emit_loop(parser *p, size_t loop_start) {
    size_t offset = current_seg()->len - loop_start + JUMP_OFFSET_LEN + 1;
    if (offset > UINT40_MAX) {
        error(p, "Too much code to jump over.");
    }
    uint8_t bytes[6] = {OP_LOOP, (uint8_t) (offset >> 32), (uint8_t) (offset >> 24), (uint8_t) (offset >> 16), (uint8_t) (offset >> 8), (uint8_t) offset};
    emit_bytes(p, bytes, 6);
}

static void emit_return(parser *p) {
    emit_byte(p, OP_RETURN);
}

static void emit_constant(parser *p, value v) {
    uint32_t result = write_constant_to_segment(current_seg(), v, p->prev.line);
    if (result == UINT32_MAX) {
        error(p, "Too many constants in one chunk.");
    }
}

static void patch_jump(parser *p, size_t offset) {
    long jump = current_seg()->len - offset - JUMP_OFFSET_LEN;
    if (jump > UINT40_MAX) {
        error(p, "Too much code to jump over.");
    }

    current_seg()->bytecode[offset] = (uint8_t) (jump >> 32);
    current_seg()->bytecode[offset+1] = (uint8_t) (jump >> 24);
    current_seg()->bytecode[offset+2] = (uint8_t) (jump >> 16);
    current_seg()->bytecode[offset+3] = (uint8_t) (jump >> 8);
    current_seg()->bytecode[offset+4] = (uint8_t) jump;
}

static void end_compiler(parser *p) {
    emit_return(p);
    #ifdef DEBUG_PRINT_CODE
        if (!p->had_error) {
            disassemble_segment(current_seg(), "code");
        }
    #endif
}

static void begin_scope(compiler *c) {
    c->scope_depth++;
}

static void end_scope(parser *p, compiler *c) {
    c->scope_depth--;

    long to_pop = 0;
    while (c->local_count > 0 && c->locals[c->local_count-1].depth > c->scope_depth) {
        to_pop++;
        c->local_count--;
    }
    for (; to_pop > 0; to_pop-=255) {
        if (to_pop >= 255) {
            emit_2_bytes(p, OP_POPN, 255);
        }
        else {
            emit_2_bytes(p, OP_POPN, to_pop);
        }
    }
}

static void binary(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    token_type operator = p->prev.type;
    parse_rule *rule = get_rule(operator);
    parse_precedence(p, c, vm, rule->prec + 1);
    switch (operator) {
        case TOKEN_PLUS: emit_byte(p, OP_ADD); break;
        case TOKEN_MINUS: emit_byte(p, OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(p, OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(p, OP_DIVIDE); break;
        case TOKEN_CARET: emit_byte(p, OP_POWER); break;
        case TOKEN_BANG_EQUAL: emit_2_bytes(p, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emit_byte(p, OP_EQUAL); break;
        case TOKEN_GREATER: emit_byte(p, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_byte(p, OP_GREATER_EQUAL); break;
        case TOKEN_LESS: emit_byte(p, OP_LESS); break;
        case TOKEN_LESS_EQUAL: emit_byte(p, OP_LESS_EQUAL); break;
        default: return;
    }

}

static void literal(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    switch (p->prev.type) {
        case TOKEN_FALSE: emit_byte(p, OP_FALSE); break;
        case TOKEN_NULL: emit_byte(p, OP_NULL); break;
        case TOKEN_TRUE: emit_byte(p, OP_TRUE); break;
        default: return;
    }
}

static void number(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    double num = strtod(p->prev.start, NULL);
    emit_constant(p, NUMBER_VAL(num));
}

static void string(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    emit_constant(p, OBJ_VAL(copy_string(vm, p->prev.start + 1, p->prev.length -2)));
}

static void named_variable(parser *p, compiler *c, VM *vm, token name, uint8_t can_assign) {
    uint8_t get_op, set_op;
    long arg_long = resolve_local(p, c, &name);
    uint32_t arg = (uint32_t) arg_long;
    if (arg_long != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else {
        arg = identifier_constant(p, c, vm, &name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }
    if (can_assign && match(p, TOKEN_EQUAL)) {
        expression(p, c, vm);
        uint8_t bytes[4] = {set_op, arg >> 16, arg >> 8, arg};
        write_n_bytes_to_segment(current_seg(), bytes, 4, p->prev.line);

    } else {
        uint8_t bytes[4] = {get_op, arg >> 16, arg >> 8, arg};
        write_n_bytes_to_segment(current_seg(), bytes, 4, p->prev.line);
    }
}

static void variable(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    named_variable(p, c, vm, p->prev, can_assign);
}

static void unary(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    token_type operator = p->prev.type;
    parse_precedence(p, c, vm, PREC_UNARY);
    switch (operator) {
        case TOKEN_MINUS: emit_byte(p, OP_NEGATE); break;
        case TOKEN_BANG: emit_byte(p, OP_NOT); break;
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
    emit_byte(p, OP_POP);
}

static void if_statement(parser *p, compiler *c, VM *vm) {
    expression(p, c, vm);
    consume(p, TOKEN_THEN, "Expect 'then' after condition.");
    size_t then_jump = emit_jump(p, OP_JUMP_IF_FALSE);
    emit_byte(p, OP_POP);
    statement(p, c, vm);

    size_t else_jump = emit_jump(p, OP_JUMP);

    patch_jump(p, then_jump);
    emit_byte(p, OP_POP);

    if (match(p, TOKEN_ELSE)) {
        statement(p, c, vm);
    }
    patch_jump(p, else_jump);
}

static void while_statement(parser *p, compiler *c, VM *vm) {
    size_t loop_start = current_seg()->len;
    expression(p, c, vm);
    consume(p, TOKEN_DO, "Expect 'do' after loop condition.");

    size_t skip_loop_jump = emit_jump(p, OP_JUMP_IF_FALSE);
    emit_byte(p, OP_POP);
    statement(p, c, vm);
    emit_loop(p, loop_start);
    patch_jump(p, skip_loop_jump);
    emit_byte(p, OP_POP);
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
    emit_constant(p, NUMBER_VAL((double) nmeb));
    emit_byte(p, OP_MAKE_ARRAY);
}

static void array_index(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    if (check(p, TOKEN_RIGHT_BRACE) || check(p, TOKEN_EOF)) {
        error(p, "Expected array index.");
    }
    expression(p, c, vm);
    consume(p, TOKEN_RIGHT_SQR, "Expect ']' after array index.");
    if (can_assign && match(p, TOKEN_EQUAL)) {
        expression(p, c, vm);
        emit_byte(p, OP_ARRAY_SET);
    } else {
        emit_byte(p, OP_ARRAY_GET);
    }
}

static void block(parser *p, compiler *c, VM *vm) {
    while (!check(p, TOKEN_RIGHT_BRACE) && !check(p, TOKEN_EOF)) {
        declaration(p, c, vm);
    }
    consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void define_variable(parser *p, compiler *c, uint32_t global) {
    if (c->scope_depth > 0) {
        mark_initialised(c);
        return;
    }
    uint8_t bytes[4] = {OP_DEFINE_GLOBAL, global >> 16, global >> 8, global};
    write_n_bytes_to_segment(current_seg(), bytes, 4, p->prev.line);
}

static void and_(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    size_t end_jump = emit_jump(p, OP_JUMP_IF_FALSE);
    emit_byte(p, OP_POP);
    parse_precedence(p, c, vm, PREC_AND);
    patch_jump(p, end_jump);
}

static void or_(parser *p, compiler *c, VM *vm, uint8_t can_assign) {
    size_t else_jump = emit_jump(p, OP_JUMP_IF_TRUE);
    emit_byte(p, OP_POP);
    parse_precedence(p, c, vm, PREC_OR);
    patch_jump(p, else_jump);
}

static void var_declaration(parser *p, compiler *c, VM *vm) {
    uint32_t global = parse_variable(p, c, vm, "Expect variable name.");

    if (match(p, TOKEN_EQUAL)) {
        expression(p, c, vm);
    } else {
        emit_byte(p, OP_NULL);
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

    size_t loop_start = current_seg()->len;
    long exit_jump = -1;
    if (!match(p, TOKEN_SEMICOLON)) {
        expression(p, c, vm); // Evaluate condition
        consume(p, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exit_jump = emit_jump(p, OP_JUMP_IF_FALSE); // Emit jump to leave loop
        emit_byte(p, OP_POP); // Pop result of condition expression
    }
    
    if (!match(p, TOKEN_DO)) {
        size_t body_jump = emit_jump(p, OP_JUMP);
        size_t increment_start = current_seg()->len;
        expression(p, c, vm);
        emit_byte(p, OP_POP);
        consume(p, TOKEN_DO, "Expect 'do' after for clauses.");
        emit_loop(p, loop_start);
        loop_start = increment_start;
        patch_jump(p, body_jump);
    }

    statement(p, c, vm);
    emit_loop(p, loop_start);
    if (exit_jump != -1) {
        patch_jump(p, (size_t) exit_jump);
        emit_byte(p, OP_POP);
    }
    end_scope(p, c);
}


static void print_statement(parser *p, compiler *c, VM *vm) {
    expression(p, c, vm);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(p, OP_PRINT);
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
    if (match(p, TOKEN_LET)) {
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
    else if (match(p, TOKEN_IF)) {
        if_statement(p, c, vm);
    } 
    else {
        expression_statement(p, c, vm);
    }
}

parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_SQR] = {array_dec, array_index, PREC_PRIMARY},
    [TOKEN_RIGHT_SQR] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
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
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONST] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
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
    segment *seg = current_seg();
    for (uint32_t i = 0; i < seg->constants.len; i++) {
        if (IS_STRING(seg->constants.values[i])) {
            char *const_string = AS_CSTRING(seg->constants.values[i]);
            size_t len = AS_STRING(seg->constants.values[i])->length;
            if (name->length == len && memcmp(const_string, name->start, len) == 0) {
               return i;
            }
        }
    }
    return make_constant(p, OBJ_VAL(copy_string(vm, name->start, name->length)));
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

static void add_local(parser *p, compiler *c, token name) {
    // Extra code for allowing more locals
    if (c->local_count > UINT24_MAX) {
        error(p, "Too many local variables in function.");
        return;
    }
    if (c->local_count >= c->local_capacity) {
        uint32_t oldc = c->local_capacity;
        c->local_capacity = GROW_CAPACITY(oldc);
        c->locals = GROW_ARRAY(local, c->locals, oldc, c->local_capacity);
    }
    local *l = &c->locals[c->local_count++];
    l->name = name;
    l->depth = -1;
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
    c->locals[c->local_count - 1].depth = c->scope_depth;
}

static parse_rule *get_rule(token_type type) {
    return &rules[type];
}

int compile(const char* source, segment *seg, VM *vm) {
    scanner s;
    parser p;
    compiler c;
    compiling_segment = seg;
    init_scanner(&s, source);
    init_parser(&p, &s);
    init_compiler(&c);
    advance(&p);
    while (!match(&p, TOKEN_EOF)) {
        declaration(&p, &c, vm);
    }
    consume(&p, TOKEN_EOF, "Expect end of expression.");
    end_compiler(&p);
    destroy_compiler(&c);
    return !p.had_error;
}
