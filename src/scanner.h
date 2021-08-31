#ifndef canidae_scanner_h

#define canidae_scanner_h

typedef enum {
    // Single char tokens
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE, TOKEN_LEFT_SQR, TOKEN_RIGHT_SQR, TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS, TOKEN_SEMICOLON,
    TOKEN_SLASH, TOKEN_STAR, TOKEN_CARET, 
    // Single/double char tokens
    TOKEN_BANG, TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL, TOKEN_CARET_EQUAL, TOKEN_PLUS_PLUS, TOKEN_MINUS_MINUS,
    // Literally literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // Keywords
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR, TOKEN_FUNCTION, TOKEN_IF, TOKEN_THEN, TOKEN_NULL, TOKEN_OR, TOKEN_PRINT, TOKEN_RETURN,
    TOKEN_SUPER, TOKEN_THIS, TOKEN_TRUE, TOKEN_LET, TOKEN_CONST, TOKEN_WHILE, TOKEN_DO, TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_UNDEFINED,
    // Control
    TOKEN_ERROR, TOKEN_EOF,
} token_type;

typedef struct {
    const char *start;
    const char *current;
    uint32_t line;
} scanner;

typedef struct {
    token_type type;
    const char *start;
    uint16_t length;
    uint32_t line;
} token;

void init_scanner(scanner *s, const char *source);
token scan_token(scanner *s);

#endif