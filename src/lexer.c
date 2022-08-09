#include <stdio.h>
#include <string.h>
#include "lexer.h"

void init_lexer(Lexer* lexer, const char* source){
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
}

Token create_token(Lexer* lexer, TokenType type){
    return (Token){
        .type = type,
        .start = lexer->start,
        .length = (size_t)(lexer->current-lexer->start),
        .line = lexer->line
    };
}

Token error_token(Lexer* lexer, const char* message){
    return (Token){
        .type = TOKEN_ERROR,
        .start = message,
        .length = strlen(message),
        .line = lexer->line
    };
}

static bool is_at_end(Lexer* lexer){
    return *lexer->current == '\0';
}

static char advance(Lexer* lexer){
    return *lexer->current++;
}

static bool match(Lexer* lexer, char expected){
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    return true;
}

static void skip_whitespace(Lexer* lexer){
    for (;;){
        char c = *lexer->current;
        switch(c){
            case ' ':
            case '\r':
            case '\t': advance(lexer); break;
            case '/': {
                if (!is_at_end(lexer) && lexer->current[1] == '/'){
                    // single line comment
                    for (; !is_at_end(lexer) || *lexer->current != '\n'; lexer->current++);
                    advance(lexer);
                } else if (!is_at_end(lexer) && lexer->current[1] == '*'){
                    // multi line comment
                    for (; *lexer->current != '*' && lexer->current[1] != '/' || !is_at_end(lexer); lexer->current++){
                        if (*lexer->current == '\n') lexer->line++;
                    }
                    advance(lexer);
                    advance(lexer);
                } else return;
            } break;
            case '\n': lexer->line++; advance(lexer); break;
            default: return;
        }
    }
}

Token scan_token(Lexer* lexer){
    skip_whitespace(lexer);

    lexer->start = lexer->current;
    if(is_at_end(lexer)) return create_token(lexer, TOKEN_EOF);

    char c = advance(lexer);
    switch (c){
        case '(': return create_token(lexer, TOKEN_LEFT_PAREN);
        case ')': return create_token(lexer, TOKEN_RIGHT_PAREN);
        case '{': return create_token(lexer, TOKEN_LEFT_BRACE);
        case '}': return create_token(lexer, TOKEN_RIGHT_BRACE);
        case ';': return create_token(lexer, TOKEN_SEMICOLON);
        case ',': return create_token(lexer, TOKEN_COMMA);
        case '.': return create_token(lexer, TOKEN_DOT);
        case '-': return create_token(lexer, TOKEN_MINUS);
        case '+': return create_token(lexer, TOKEN_PLUS);
        case '*': return create_token(lexer, TOKEN_STAR);
        case '/': return create_token(lexer, TOKEN_SLASH);
        case '?': return create_token(lexer, TOKEN_QMARK);
        case ':': return create_token(lexer, TOKEN_COLON);

        case '!': return create_token(lexer, match(lexer, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG); break;
        case '=': return create_token(lexer, match(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL); break;
        case '<': return create_token(lexer, match(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS); break;
        case '>': return create_token(lexer, match(lexer, '=') ? TOKEN_GREATER_EQUAL: TOKEN_GREATER); break;    
    }

    return error_token(lexer, "Unexpected character");
}