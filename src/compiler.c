#include <stdio.h>
#include "common.h"
#include "compiler.h"
#include "lexer.h"

void compile(VM* vm, const char* source){
    
    Lexer lexer;
    init_lexer(&lexer, source);

    size_t line = -1;
    for (;;){
        Token token = scan_token(&lexer);
        if(token.line != line){
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);
        if (token.type == TOKEN_EOF) break;
    }
}