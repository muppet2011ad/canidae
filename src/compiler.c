#include <stdlib.h>
#include <stdio.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"
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
    PREC_CALL,
    PREC_PRIMARY
} precedence;

typedef void (*parse_func)(parser *p);

typedef struct {
    parse_func prefix;
    parse_func infix;
    precedence prec;
} parse_rule;

static parse_rule *get_rule(token_type type);
static void expression(parser *p);
static void parse_precedence(parser *p, precedence prec);

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
    fprintf(stderr, "[line %d] Error", t->line);
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

static void emit_byte(parser *p, uint8_t byte) {
    write_to_segment(current_seg(), byte, p->prev.line);
}

static void emit_bytes(parser *p, uint8_t *bytes, size_t n) {
    write_n_bytes_to_segment(current_seg(), bytes, n, p->prev.line);
}

static void emit_return(parser *p) {
    emit_byte(p, OP_RETURN);
}

static void emitConstant(parser *p, value v) {
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

static void binary(parser *p) {
    token_type operator = p->prev.type;
    parse_rule *rule = get_rule(operator);
    parse_precedence(p, rule->prec + 1);
    switch (operator) {
        case TOKEN_PLUS: emit_byte(p, OP_ADD); break;
        case TOKEN_MINUS: emit_byte(p, OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(p, OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(p, OP_DIVIDE); break;
        default: return;
    }

}

static void number(parser *p) {
    double num = strtod(p->prev.start, NULL);
    emitConstant(p, NUMBER_VAL(num));
}

static void unary(parser *p) {
    token_type operator = p->prev.type;
    parse_precedence(p, PREC_UNARY);
    switch (operator) {
        case TOKEN_MINUS: emit_byte(p, OP_NEGATE); break;
        default: return;
    }
}

static void grouping(parser *p) {
    expression(p);
    consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void expression(parser *p) {
    parse_precedence(p, PREC_ASSIGNMENT);
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
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUNCTION] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NULL] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONST] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parse_precedence(parser *p, precedence prec) {
    advance(p);
    parse_func prefix_rule = get_rule(p->prev.type)->prefix;
    if (prefix_rule == NULL) {
        error(p, "Expect expression.");
        return;
    }

    prefix_rule(p);

    while(prec <= get_rule(p->current.type)->prec) {
        advance(p);
        parse_func infix_rule = get_rule(p->prev.type)->infix;
        infix_rule(p);
    }
}

static parse_rule *get_rule(token_type type) {
    return &rules[type];
}

int compile(const char* source, segment *seg) {
    scanner s;
    parser p;
    compiling_segment = seg;
    init_scanner(&s, source);
    init_parser(&p, &s);
    advance(&p);
    expression(&p);
    consume(&p, TOKEN_EOF, "Expect end of expression.");
    end_compiler(&p);
    return !p.had_error;
}
