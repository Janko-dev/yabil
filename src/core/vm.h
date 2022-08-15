#ifndef _VM_H
#define _VM_H

#include "chunk.h"
#include "../common/value.h"
#include "../common/table.h"

#define STACK_MAX 1024

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERR,
    INTERPRET_RUNTIME_ERR,
} InterpreterResult;

typedef struct {
    Chunk* chunk;               // chunk of compiled byte-code instructions that will be executed 
    uint8_t* ip;                // instruction pointer
    Value stack[STACK_MAX];     // stack of Values
    Value* sp;                  // stack pointer
    Table globals;              // hashtable of global variables
    Table strings;              // hashtable of strings (used for interning strings)
    Obj* objects;               // linked list of all heap allocated objects 
} VM;

extern VM vm;

void init_VM();
void free_VM();

InterpreterResult interpret(const char* source);
void push(Value val);
Value pop();

#endif //_VM_H