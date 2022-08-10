#include <stdio.h>
#include <stdarg.h>
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"

static void run_time_error(VM* vm, const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    size_t line = get_line(&vm->chunk->lines, (size_t)(vm->ip - vm->chunk->code - 1));
    fprintf(stderr, "\n[line %d] in script\n", line);
}

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

Value peek(VM* vm, int dist){
    return vm->sp[-1 - dist];
}

#define BINARY_OP(val_type, op) \
    do { \
        if (!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){ \
            run_time_error(vm, "Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERR; \
        } \
        double b = pop(vm).as.number; \
        double a = pop(vm).as.number; \
        push(vm, val_type(a op b)); \
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
            if (!IS_NUM(peek(vm, 0))){
                run_time_error(vm, "operand must be a number");
                return INTERPRET_RUNTIME_ERR;
            }
            (vm->sp-1)->as.number *= -1;
            // push(vm, -pop(vm));
            NEXT();
        }
        op_add:; BINARY_OP(NUM_VAL, +); NEXT();
        op_sub:; BINARY_OP(NUM_VAL, -); NEXT();
        op_mul:; BINARY_OP(NUM_VAL, *); NEXT();
        op_div:; BINARY_OP(NUM_VAL, /); NEXT();

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