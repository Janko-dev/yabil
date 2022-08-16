#include <stdio.h>
#include <string.h>
#include "lexer.h"

void init_lexer(Lexer* lexer, const char* source){
    lexer->start = source;
    lexer->current = source;
    lexer->source = source;
    lexer->line = 1;
}

Token create_token(Lexer* lexer, TokenType type){
    return (Token){
        .type = type,
        .start = lexer->start,
        .length = (size_t)(lexer->current-lexer->start),
        .line = lexer->line,
        .err_msg = NULL,
        .err_len = 0
    };
}

Token error_token(Lexer* lexer, const char* message){
    return (Token){
        .type = TOKEN_ERROR,
        .start = lexer->start,
        .length = (size_t)(lexer->current-lexer->start),
        .line = lexer->line,
        .err_msg = message,
        .err_len = strlen(message)
    };
}

static bool is_at_end(Lexer* lexer){
    return (*lexer->current) == '\0';
}

static char advance(Lexer* lexer){
    return *lexer->current++;
}

static bool is_digit(char c){
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c){
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c == '_');
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
                    for (; *lexer->current != '\n' && !is_at_end(lexer); advance(lexer));
                } else if (!is_at_end(lexer) && lexer->current[1] == '*'){
                    // multi line comment
                    advance(lexer);
                    advance(lexer);
                    while(!is_at_end(lexer) && *lexer->current != '*' && lexer->current[1] != '/') advance(lexer);
                    advance(lexer);
                    advance(lexer);
                } else return;
            } break;
            case '\n': lexer->line++; advance(lexer); break;
            default: return;
        }
    }
}

Token create_string(Lexer* lexer){
    while(!is_at_end(lexer) && *lexer->current != '"'){
        if (*lexer->current == '\n') return error_token(lexer, "Unterminated string literal");
        advance(lexer);
    }

    if (is_at_end(lexer)) return error_token(lexer, "Unterminated string literal");
    
    advance(lexer);
    return create_token(lexer, TOKEN_STRING);
} 

Token create_digit(Lexer* lexer){
    while(is_digit(*lexer->current)) advance(lexer);
    if (*lexer->current == '.'){
        advance(lexer); //consume dot '.'
        while(is_digit(*lexer->current)) advance(lexer);
    }
    return create_token(lexer, TOKEN_NUMBER);
} 

TokenType match_keyword(Lexer* lexer, size_t start, size_t len, const char* word, TokenType keyword){
    if ((size_t)(lexer->current - lexer->start) == start + len && 
        memcmp(lexer->start + start, word, len) == 0) 
            return keyword; 
    return TOKEN_IDENTIFIER;
}

TokenType get_keyword(Lexer* lexer){
    switch(*lexer->start){
        case 'a': return match_keyword(lexer, 1, 2, "nd",   TOKEN_AND);
        case 'c': return match_keyword(lexer, 1, 4, "lass", TOKEN_CLASS);
        case 'e': return match_keyword(lexer, 1, 3, "lse",  TOKEN_ELSE);
        case 'i': return match_keyword(lexer, 1, 1, "f",    TOKEN_IF);
        case 'n': return match_keyword(lexer, 1, 2, "il",   TOKEN_NIL);
        case 'o': return match_keyword(lexer, 1, 1, "r",    TOKEN_OR);
        case 'p': return match_keyword(lexer, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return match_keyword(lexer, 1, 5, "eturn",TOKEN_RETURN);
        case 's': return match_keyword(lexer, 1, 4, "uper", TOKEN_SUPER);
        case 'v': return match_keyword(lexer, 1, 2, "ar",   TOKEN_VAR);
        case 'w': return match_keyword(lexer, 1, 4, "hile", TOKEN_WHILE);

        case 'f': {
            if (lexer->current - lexer->start <= 1) return TOKEN_IDENTIFIER;
            switch(lexer->start[1]){
                case 'a': return match_keyword(lexer, 2, 3, "lse", TOKEN_FALSE);
                case 'o': return match_keyword(lexer, 2, 1, "r",   TOKEN_FOR);
                case 'u': return match_keyword(lexer, 2, 1, "n",   TOKEN_FUN);
            }
        } break;
        case 't': {
            if (lexer->current - lexer->start <= 1) return TOKEN_IDENTIFIER;
            switch(lexer->start[1]){
                case 'h': return match_keyword(lexer, 2, 2, "is",  TOKEN_THIS);
                case 'r': return match_keyword(lexer, 2, 2, "ue",  TOKEN_TRUE);
            }
        } break;
    }
    return TOKEN_IDENTIFIER;
}

Token create_identifier(Lexer* lexer){
    while(is_alpha(*lexer->current) || is_digit(*lexer->current)) advance(lexer);
    return create_token(lexer, get_keyword(lexer));
}

Token scan_token(Lexer* lexer){

    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if(is_at_end(lexer)) return create_token(lexer, TOKEN_EOF);

    char c = advance(lexer);

    if (is_digit(c)) return create_digit(lexer);
    if (is_alpha(c)) return create_identifier(lexer);
    
    switch (c){
        case '(': return create_token(lexer, TOKEN_LEFT_PAREN);
        case ')': return create_token(lexer, TOKEN_RIGHT_PAREN);
        case '{': return create_token(lexer, TOKEN_LEFT_BRACE);
        case '}': return create_token(lexer, TOKEN_RIGHT_BRACE);
        case '[': return create_token(lexer, TOKEN_LEFT_BRACKET);
        case ']': return create_token(lexer, TOKEN_RIGHT_BRACKET);
        case ';': return create_token(lexer, TOKEN_SEMICOLON);
        case ',': return create_token(lexer, TOKEN_COMMA);
        case '.': return create_token(lexer, TOKEN_DOT);
        case '-': return create_token(lexer, TOKEN_MINUS);
        case '+': return create_token(lexer, TOKEN_PLUS);
        case '*': return create_token(lexer, TOKEN_STAR);
        case '/': return create_token(lexer, TOKEN_SLASH);
        case '?': return create_token(lexer, TOKEN_QMARK);
        case ':': return create_token(lexer, TOKEN_COLON);
        case '%': return create_token(lexer, TOKEN_MOD);

        case '!': return create_token(lexer, match(lexer, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return create_token(lexer, match(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL); 
        case '<': return create_token(lexer, match(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS); 
        case '>': return create_token(lexer, match(lexer, '=') ? TOKEN_GREATER_EQUAL: TOKEN_GREATER);

        case '"': return create_string(lexer); 
    }

    return error_token(lexer, "Unexpected character");
}