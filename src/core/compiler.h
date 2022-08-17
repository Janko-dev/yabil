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

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

typedef void (*ParseFn)(bool can_assign);
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int32_t depth;
    bool is_captured;
} Local;

typedef struct {
    int32_t index;
    bool is_local;
} Upvalue;


typedef struct Compiler Compiler;
struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* fn;
    FunctionType type;
    Local locals[UINT24_COUNT];
    int32_t local_count;
    int32_t scope_depth;
    Upvalue upvalues[UINT24_COUNT];
};

ObjFunction* compile(const char* source);

#endif //_COMPILER_H