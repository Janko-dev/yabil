#ifndef _VM_H
#define _VM_H

#include "chunk.h"
#include "value.h"
#include "table.h"

#define STACK_MAX 256

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERR,
    INTERPRET_RUNTIME_ERR,
} InterpreterResult;

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* sp;
    Table strings; 
    Obj* objects;
} VM;

extern VM vm;

void init_VM();
void free_VM();

InterpreterResult interpret(const char* source);
void push(Value val);
Value pop();

#endif //_VM_H