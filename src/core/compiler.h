#ifndef _COMPILER_H
#define _COMPILER_H

#include "../common/common.h"
#include "lexer.h"
#include "vm.h"

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_TERNARY,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(bool can_assign);
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int32_t depth;
} Local;

typedef struct {
    Local locals[UINT24_COUNT];
    int32_t local_count;
    int32_t scope_depth;
} Compiler;

bool compile(const char* source, Chunk* chunk);

#endif //_COMPILER_H