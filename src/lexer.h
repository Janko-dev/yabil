#ifndef _LEXER_H
#define _LEXER_H

#include "common.h"

typedef enum {
    // single-character tokens
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    TOKEN_QMARK, TOKEN_COLON,
    
    // one or two character tokens
    TOKEN_BANG, TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,

    // literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

    // keywords
    TOKEN_AND, TOKEN_OR, TOKEN_PRINT, TOKEN_IF, TOKEN_ELSE, TOKEN_TRUE, TOKEN_FALSE, TOKEN_NIL,
    TOKEN_FOR, TOKEN_WHILE, TOKEN_FUN, TOKEN_RETURN, TOKEN_CLASS, TOKEN_SUPER, TOKEN_THIS, TOKEN_VAR,

    TOKEN_ERROR, TOKEN_EOF, 
} TokenType;


typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    size_t line;
    const char* err_msg;
    size_t err_len;
} Token;

typedef struct {
    const char* start;
    const char* current;
    const char* source;
    size_t line;
} Lexer;

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;


void init_lexer(Lexer* lexer, const char* source);
Token scan_token(Lexer* lexer);

#endif //_LEXER_H
