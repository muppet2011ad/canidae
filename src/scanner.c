#include <stdio.h>
#include <string.h>
#include "common.h"
#include "scanner.h"

void init_scanner(scanner *s, const char *source) {
    s->start = source;
    s->current = source;
    s->line = 1;
}

static int is_at_end(scanner *s) {
    return *s->current == '\0';
}

static token make_token(scanner *s, token_type type) {
    token t;
    t.type = type;
    t.start = s->start;
    t.length = (s->current - s->start);
    t.line = s->line;
    return t;
}

static token error_token(scanner *s, const char *message) {
    token t;
    t.type = TOKEN_ERROR;
    t.start = message;
    t.length = strlen(message);
    t.line = s->line;
    return t;
}

static char peek(scanner *s) {
    return *s->current;
}

static char peek_next(scanner *s) {
    if (is_at_end(s)) return '\0';
    return s->current[1];
}

static char advance(scanner *s) {
    s->current++;
    return s->current[-1];
}

static int match(scanner *s, char expected) {
    if (is_at_end(s)) return 0;
    if (*s->current != expected) return 0;
    s->current++;
    return 1;
}

static void skip_whitespace(scanner *s) {
    for (;;) {
        char c = peek(s);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(s);
                break;
            case '\n':
                s->line++;
                advance(s);
                break;
            case '/':
                if (peek_next(s) == '/') {
                    while (peek(s) != '\n' && !is_at_end(s)) advance(s);
                }
                else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static token number(scanner *s) {
    while (is_digit(peek(s))) advance(s);
    if (peek(s) == '.' && is_digit(peek_next(s))) {
        advance(s);
        while (is_digit(peek(s))) advance(s);
    }
    return make_token(s, TOKEN_NUMBER);
}

static token_type check_keyword(scanner *s, uint32_t start, uint32_t length, const char *rest, token_type type) {
    if (s->current - s->start == start + length && memcmp(s->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static token_type identifier_type(scanner *s) {
    switch (s->start[0]) {
        case 'a':
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'n': return check_keyword(s, 2, 1, "d", TOKEN_AND);
                    case 'r': return check_keyword(s, 2, 3, "ray", TOKEN_ARRAY);
                    case 's': return check_keyword(s, 2, 0, "", TOKEN_AS);
                }
                break;
            } 
            break;
        case 'b': 
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'r': return check_keyword(s, 2, 3, "eak", TOKEN_BREAK);
                    case 'o': return check_keyword(s, 2, 2, "ol", TOKEN_BOOL);
                }
            }
            break;
        
        case 'c': 
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'l': return check_keyword(s, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': {
                        if (s->current - s->start > 2) {
                            switch (s->start[2]) {
                                case 'n': {
                                    if (s->current - s->start > 3) {
                                        switch (s->start[3]) {
                                            case 's': return check_keyword(s, 4, 2, "st", TOKEN_CONST);
                                            case 't': return check_keyword(s, 4, 4, "inue", TOKEN_CONTINUE);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;
        case 'd': return check_keyword(s, 1, 1, "o", TOKEN_DO);
        case 'e': return check_keyword(s, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'a': return check_keyword(s, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(s, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(s, 2, 6, "nction", TOKEN_FUNCTION);
                }
            }
            break;
        case 'i': 
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'f': return check_keyword(s, 2, 0, "", TOKEN_IF);
                    case 'm': return check_keyword(s, 2, 4, "port", TOKEN_IMPORT);
                    case 'n': return check_keyword(s, 2, 6, "herits", TOKEN_INHERITS);
                }
                break;
            }
            break;
        case 'l': return check_keyword(s, 1, 2, "et", TOKEN_LET);
        case 'n': 
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'a': return check_keyword(s, 2, 7, "mespace", TOKEN_NAMESPACE);
                    case 'u':
                        if (s->current - s->start > 2) {
                            switch (s->start[2]) {
                                case 'l': return check_keyword(s, 3, 1, "l", TOKEN_NULL);
                                case 'm': return check_keyword(s, 3, 0, "", TOKEN_NUM);
                            }
                        }
                        break;
                }
            }
            break;
        case 'o': return check_keyword(s, 1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(s, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(s, 1, 5, "eturn", TOKEN_RETURN);
        case 's': 
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'u': return check_keyword(s, 2, 3, "per", TOKEN_SUPER);
                    case 't': return check_keyword(s, 2, 1, "r", TOKEN_STR);
                }
            }
            break;
        case 't':
            if (s->current - s->start > 1) {
                switch (s->start[1]) {
                    case 'h': {
                        if (s->current - s->start > 2) {
                            switch (s->start[2]) {
                                case 'i': return check_keyword(s, 3, 1, "s", TOKEN_THIS);
                                case 'e': return check_keyword(s, 3, 1, "n", TOKEN_THEN);
                            }
                        }
                    }
                    case 'r': return check_keyword(s, 2, 2, "ue", TOKEN_TRUE);
                    case 'y': return check_keyword(s, 2, 4, "peof", TOKEN_TYPEOF);
                }
            }
            break;
        case 'u': return check_keyword(s, 1, 8, "ndefined", TOKEN_UNDEFINED);
        case 'w': return check_keyword(s, 1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

static token identifier(scanner *s) {
    while (is_alpha(peek(s)) || is_digit(peek(s))) advance(s);
    return make_token(s, identifier_type(s));
}

static token string(scanner *s) {
    while (peek(s) != '"' && !is_at_end(s)) {
        if (peek(s) == '\n') s->line++;
        advance(s);
    }
    if (is_at_end(s)) {
        return error_token(s, "Unterminated string.");
    }

    advance(s);
    return make_token(s, TOKEN_STRING);
}

token scan_token(scanner *s) {
    skip_whitespace(s);
    s->start = s->current;
    if (is_at_end(s)) return make_token(s, TOKEN_EOF);

    char c = advance(s);
    if (is_alpha(c)) return identifier(s);
    if (is_digit(c)) return number(s);


    switch (c) {
        case '(': return make_token(s, TOKEN_LEFT_PAREN);
        case ')': return make_token(s, TOKEN_RIGHT_PAREN);
        case '{': return make_token(s, TOKEN_LEFT_BRACE);
        case '}': return make_token(s, TOKEN_RIGHT_BRACE);
        case '[': return make_token(s, TOKEN_LEFT_SQR);
        case ']': return make_token(s, TOKEN_RIGHT_SQR);
        case ';': return make_token(s, TOKEN_SEMICOLON);
        case ',': return make_token(s, TOKEN_COMMA);
        case '.': return make_token(s, TOKEN_DOT);
        case '-': 
            return make_token(s, match(s, '=') ? TOKEN_MINUS_EQUAL : (match(s, '-') ? TOKEN_MINUS_MINUS : TOKEN_MINUS));
        case '+': 
            return make_token(s, match(s, '=') ? TOKEN_PLUS_EQUAL : (match(s, '+') ? TOKEN_PLUS_PLUS : TOKEN_PLUS));
        case '/': 
            return make_token(s, match(s, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '*': 
            return make_token(s, match(s, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
        case '^': 
            return make_token(s, match(s, '=') ? TOKEN_CARET_EQUAL : TOKEN_CARET);
        case '!':
            return make_token(s, match(s, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(s, match(s, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return make_token(s, match(s, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return make_token(s, match(s, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string(s);

    }

    return error_token(s, "Unexpected character.");
}