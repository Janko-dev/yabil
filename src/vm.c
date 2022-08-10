#include <stdio.h>
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

static void reset_stack(VM* vm){
    vm->sp = vm->stack;
}

void init_VM(VM* vm){
    reset_stack(vm);
}

void free_VM(VM* vm){
    (void)vm;
}

void push(VM* vm, Value val){
    *(vm->sp++) = val;
}

Value pop(VM* vm){
    return *(--vm->sp);
}

#define BINARY_OP(op) \
    do { \
        double b = pop(vm); \
        double a = pop(vm); \
        push(vm, a op b); \
    } while (0) \

static InterpreterResult run(VM* vm){
    static void* dispatch_table[] = {
        &&op_add, &&op_sub, &&op_mul, &&op_div, &&op_negate, 
        &&op_const_long, &&op_const, &&op_return
    };
    #define READ_BYTE() (*vm->ip++)
    #define DISPATCH() goto *dispatch_table[READ_BYTE()]
    #define NEXT() goto start
    #define READ_CONSTANT(index) (vm->chunk->constants.values[index])
    
    for (;;){
        start:;
#ifdef DEBUG_TRACE_EXECUTION
            printf("      ");
            for (Value* slot = vm->stack; slot < vm->sp; slot++){
                printf("[");
                print_value(*slot);
                printf("]");
            }
            printf("\n");
            disassemble_instruction(vm->chunk, (size_t)(vm->ip - vm->chunk->code));
#endif //DEBUG_TRACE_EXECUTION 
            DISPATCH();

        op_return:;{
            print_value(pop(vm));
            printf("\n");
            return INTERPRET_OK;
        }
        op_const:;{
            Value constant = READ_CONSTANT(READ_BYTE());
            push(vm, constant);
            NEXT();
        }
        op_const_long:;{
            size_t const_index = vm->ip[0] | vm->ip[1] << 8 | vm->ip[2] << 16;
            Value constant = READ_CONSTANT(const_index);
            push(vm, constant);
            vm->ip+=3;
            NEXT();
        }
        op_negate:;{
            *(vm->sp-1) *= -1;
            // push(vm, -pop(vm));
            NEXT();
        }
        op_add:; BINARY_OP(+); NEXT();
        op_sub:; BINARY_OP(-); NEXT();
        op_mul:; BINARY_OP(*); NEXT();
        op_div:; BINARY_OP(/); NEXT();

    }
    #undef READ_BYTE
    #undef DISPATCH
    #undef READ_CONSTANT
    #undef NEXT
}

InterpreterResult interpret(VM* vm, const char* source){
    Chunk chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)){
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERR;
    }

    vm->chunk = &chunk;
    vm->ip = chunk.code;

    InterpreterResult result = run(vm);
    free_chunk(&chunk);
    return result;
}