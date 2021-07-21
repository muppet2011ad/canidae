#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "value.h"
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
    uint32_t depth;
} local;

typedef struct {
    local locals[UINT16_MAX+1];
    uint16_t local_count;
    uint32_t scope_depth;
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

typedef void (*parse_func)(parser *p, VM *vm, uint8_t can_assign);

typedef struct {
    parse_func prefix;
    parse_func infix;
    precedence prec;
} parse_rule;

static parse_rule *get_rule(token_type type);
static void expression(parser *p, VM *vm);
static void declaration(parser *p, VM *vm);
static void statement(parser *p, VM *vm);
static uint32_t parse_variable(parser *p, VM *vm, const char *error_message);
static void parse_precedence(parser *p, VM *vm, precedence prec);
static uint32_t identifier_constant(parser *p, VM *vm, token *name);

segment *compiling_segment;

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

static void emit_return(parser *p) {
    emit_byte(p, OP_RETURN);
}

static void emit_constant(parser *p, value v) {
    uint32_t result = write_constant_to_segment(current_seg(), v, p->prev.line);
    if (result == UINT32_MAX) {
        error(p, "Too many constants in one chunk.");
    }
}

static void end_compiler(parser *p) {
    emit_return(p);
    #ifdef DEBUG_PRINT_CODE
        if (!p->had_error) {
            disassemble_segment(current_seg(), "code");
        }
    #endif
}

static void binary(parser *p, VM *vm, uint8_t can_assign) {
    token_type operator = p->prev.type;
    parse_rule *rule = get_rule(operator);
    parse_precedence(p, vm, rule->prec + 1);
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

static void literal(parser *p, VM *vm, uint8_t can_assign) {
    switch (p->prev.type) {
        case TOKEN_FALSE: emit_byte(p, OP_FALSE); break;
        case TOKEN_NULL: emit_byte(p, OP_NULL); break;
        case TOKEN_TRUE: emit_byte(p, OP_TRUE); break;
        default: return;
    }
}

static void number(parser *p, VM *vm, uint8_t can_assign) {
    double num = strtod(p->prev.start, NULL);
    emit_constant(p, NUMBER_VAL(num));
}

static void string(parser *p, VM *vm, uint8_t can_assign) {
    emit_constant(p, OBJ_VAL(copy_string(vm, p->prev.start + 1, p->prev.length -2)));
}

static void named_variable(parser *p, VM *vm, token name, uint8_t can_assign) {
    uint32_t arg = identifier_constant(p, vm, &name);
    if (can_assign && match(p, TOKEN_EQUAL)) {
        expression(p, vm);
        uint8_t bytes[4] = {OP_SET_GLOBAL, arg >> 16, arg >> 8, arg};
        write_n_bytes_to_segment(current_seg(), bytes, 4, p->prev.line);

    } else {
        uint8_t bytes[4] = {OP_GET_GLOBAL, arg >> 16, arg >> 8, arg};
        write_n_bytes_to_segment(current_seg(), bytes, 4, p->prev.line);
    }
}

static void variable(parser *p, VM *vm, uint8_t can_assign) {
    named_variable(p, vm, p->prev, can_assign);
}

static void unary(parser *p, VM *vm, uint8_t can_assign) {
    token_type operator = p->prev.type;
    parse_precedence(p, vm, PREC_UNARY);
    switch (operator) {
        case TOKEN_MINUS: emit_byte(p, OP_NEGATE); break;
        case TOKEN_BANG: emit_byte(p, OP_NOT); break;
        default: return;
    }
}

static void grouping(parser *p, VM *vm, uint8_t can_assign) {
    expression(p, vm);
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void expression(parser *p, VM *vm) {
    parse_precedence(p, vm, PREC_ASSIGNMENT);
}

static void define_variable(parser *p, uint32_t global) {
    uint8_t bytes[4] = {OP_DEFINE_GLOBAL, global >> 16, global >> 8, global};
    write_n_bytes_to_segment(current_seg(), bytes, 4, p->prev.line);
}

static void var_declaration(parser *p, VM *vm) {
    uint32_t global = parse_variable(p, vm, "Expect variable name.");

    if (match(p, TOKEN_EQUAL)) {
        expression(p, vm);
    } else {
        emit_byte(p, OP_NULL);
    }
    consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration,");
    define_variable(p, global);
}

static void print_statement(parser *p, VM *vm) {
    expression(p, vm);
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

static void declaration(parser *p, VM *vm) {
    if (match(p, TOKEN_LET)) {
        var_declaration(p, vm);
    }
    else {
        statement(p, vm);
    }

    if (p->panic) synchronise(p);
}

static void statement(parser *p, VM *vm) {
    if (match(p, TOKEN_PRINT)) {
        print_statement(p, vm);
    } else {
        expression(p, vm);
        consume(p, TOKEN_SEMICOLON, "Expect ';' after expression.");
        emit_byte(p, OP_POP);
    }
}

parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
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
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUNCTION] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NULL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
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

static void parse_precedence(parser *p, VM *vm, precedence prec) {
    advance(p);
    parse_func prefix_rule = get_rule(p->prev.type)->prefix;
    if (prefix_rule == NULL) {
        error(p, "Expect expression.");
        return;
    }

    uint8_t can_assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(p, vm, can_assign);

    while(prec <= get_rule(p->current.type)->prec) {
        advance(p);
        parse_func infix_rule = get_rule(p->prev.type)->infix;
        infix_rule(p, vm, can_assign);
    }

    if (can_assign && match(p, TOKEN_EQUAL)) {
        error(p, "Invalid assignment target.");
    }
}

static uint32_t identifier_constant(parser *p, VM *vm, token *name) {
    // Extra code so that new constants aren't required for every use of a variable
    segment *seg = current_seg();
    for (uint32_t i = 0; i < seg->constants.len; i++) {
        if (IS_STRING(seg->constants.values[i])) {
            char *const_string = AS_CSTRING(seg->constants.values[i]);
            uint32_t len = AS_STRING(seg->constants.values[i])->length;
            if (name->length == len && memcmp(const_string, name->start, len) == 0) {
                if (i < UINT8_MAX) {
                    emit_2_bytes(p, OP_CONSTANT, i);
                    return i;
                }
                else {
                    uint8_t bytes[4] = {OP_CONSTANT_LONG, i >> 16, i >> 8, i};
                    emit_bytes(p, bytes, 4);
                    return i;
                }
            }
        }
    }
    return write_constant_to_segment(current_seg(), OBJ_VAL(copy_string(vm, name->start, name->length)), name->line);
}

static uint32_t parse_variable(parser *p, VM *vm, const char *error_message) {
    consume(p, TOKEN_IDENTIFIER, error_message);
    return identifier_constant(p, vm, &p->prev);
}

static parse_rule *get_rule(token_type type) {
    return &rules[type];
}

int compile(const char* source, segment *seg, VM *vm) {
    scanner s;
    parser p;
    compiling_segment = seg;
    init_scanner(&s, source);
    init_parser(&p, &s);
    advance(&p);
    while (!match(&p, TOKEN_EOF)) {
        declaration(&p, vm);
    }
    consume(&p, TOKEN_EOF, "Expect end of expression.");
    end_compiler(&p);
    return !p.had_error;
}
