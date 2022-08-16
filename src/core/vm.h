#ifndef _VM_H
#define _VM_H

#include "chunk.h"
#include "../common/value.h"
#include "../common/table.h"
#include "../common/object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_MAX)

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERR,
    INTERPRET_RUNTIME_ERR,
} InterpreterResult;

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];     // stack of function calls that get executed
    size_t frame_count;               // number of call frames currently on the stack
    Value stack[STACK_MAX];           // stack of Values
    Value* sp;                        // stack pointer
    Table globals;                    // hashtable of global variables
    Table strings;                    // hashtable of strings (used for interning strings)
    Obj* objects;                     // linked list of all heap allocated objects 
} VM;

extern VM vm;

void init_VM();
void free_VM();

InterpreterResult interpret(const char* source);
void push(Value val);
Value pop();

#endif //_VM_H