#ifndef _VM_H
#define _VM_H

#include "chunk.h"
#include "value.h"

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
} VM;

extern Obj* objects;

void init_VM(VM* vm);
void free_VM(VM* vm);

InterpreterResult interpret(VM* vm, const char* source);
void push(VM* vm, Value val);
Value pop(VM* vm);

#endif //_VM_H