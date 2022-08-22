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
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    Value stack[STACK_MAX];           // stack of Values
    Value* sp;                        // stack pointer
    CallFrame frames[FRAMES_MAX];     // stack of function calls that get executed
    size_t frame_count;               // number of call frames currently on the stack
    Table globals;                    // hashtable of global variables
    Table strings;                    // hashtable of strings (used for interning strings)
    ObjUpvalue* open_upvalues;        // linked list of all open upvalues
    Obj* objects;                     // linked list of all heap allocated objects
    size_t gray_count;                // count of gray colored object nodes
    size_t gray_cap;                  // capacity of gray colored object nodes
    Obj** gray_stack;                 // stack of gray colored object nodes used by GC
    size_t bytes_allocated;           // total of bytes that the VM has allocated
    size_t next_GC;                   // threshold to trigger next GC run     
} VM;

extern VM vm;

void init_VM();
void free_VM();

InterpreterResult interpret(const char* source);
void push(Value val);
Value pop();

#endif //_VM_H