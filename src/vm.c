#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "object.h"
#include "compiler.h"
#include "memory.h"

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

static bool is_falsey(Value val){
    return (IS_NIL(val) || (IS_BOOL(val) && !val.as.boolean));
}

static void concat_string(VM* vm, char* a, size_t size_a, char* b, size_t size_b){
    size_t length = size_a + size_b;
    char* chars = ALLOCATE(char, length+1);
    memcpy(chars, a, size_a);
    memcpy(chars + size_a, b, size_b);
    chars[length] = '\0';
    ObjString* result = take_string(chars, length);
    push(vm, OBJ_VAL(result));
}

static void to_string(char* s, size_t size, Value val){
    switch(val.type){
        case VAL_NUM: snprintf(s, size, "%g", val.as.number); break;
        case VAL_BOOL: strncpy(s, val.as.boolean ? "true" : "false", size); break;
        case VAL_NIL:  strncpy(s, "(nil)", size); break;
        default: break;
    }
}

static void concatenate(VM* vm, Value a, Value b){
    if (IS_STRING(a) && IS_STRING(b)){
        concat_string(vm, AS_CSTRING(a), AS_STRING(a)->length, AS_CSTRING(b), AS_STRING(b)->length);
        return;
    }
    if (!IS_STRING(a)){
        char str_a[100];
        to_string(str_a, 100, a);
        concat_string(vm, str_a, strlen(str_a), AS_CSTRING(b), AS_STRING(b)->length);
        return;
    }
    if (!IS_STRING(b)){
        char str_b[100];
        to_string(str_b, 100, b);
        concat_string(vm, AS_CSTRING(a), AS_STRING(a)->length, str_b, strlen(str_b));
        return;
    }
}

#define BINARY_OP(val_type, op)                                     \
    do {                                                            \
        if (!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){          \
            run_time_error(vm, "Operands must be numbers");         \
            return INTERPRET_RUNTIME_ERR;                           \
        }                                                           \
        double b = pop(vm).as.number;                               \
        double a = pop(vm).as.number;                               \
        push(vm, val_type(a op b));                                 \
    } while (0)                                                     \

#define EQUALS(not)                                                 \
    do {                                                            \
        Value b = pop(vm);                                          \
        Value a = pop(vm);                                          \
        push(vm, BOOL_VAL(not values_equal(a, b)));                 \
    } while (0)                                                     \

static InterpreterResult run(VM* vm){
    static void* dispatch_table[] = {
        &&op_add, &&op_sub, &&op_mul, &&op_div, &&op_not, &&op_negate, 
        &&op_const_long, &&op_const, &&op_nil, &&op_true, &&op_false,
        &&op_equal, &&op_not_equal, &&op_less, &&op_less_equal, &&op_greater, &&op_greater_equal,
        &&op_return
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
            if ((int)(vm->sp - vm->stack) > 0){
                print_value(pop(vm));
                printf("\n");
            }
            return INTERPRET_OK;
        }
        op_const:;{
            Value constant = READ_CONSTANT(READ_BYTE());
            push(vm, constant);
        } NEXT();
        op_const_long:;{
            size_t const_index = vm->ip[0] | vm->ip[1] << 8 | vm->ip[2] << 16;
            Value constant = READ_CONSTANT(const_index);
            push(vm, constant);
            vm->ip+=3;
        } NEXT();
        op_nil:; push(vm, NIL_VAL); NEXT();
        op_true:; push(vm, BOOL_VAL(true)); NEXT();
        op_false:; push(vm, BOOL_VAL(false)); NEXT();
        op_negate:;{
            if (!IS_NUM(peek(vm, 0))){
                run_time_error(vm, "operand must be a number");
                return INTERPRET_RUNTIME_ERR;
            }
            (vm->sp-1)->as.number *= -1; // access top of stack directly and negate its value
        } NEXT();
        op_not:; {
            *(vm->sp-1) = BOOL_VAL(is_falsey(*(vm->sp-1)));
        } NEXT();
        op_add:;{
            Value b = pop(vm);
            Value a = pop(vm);
            if (IS_NUM(a) && IS_NUM(b)) push(vm, NUM_VAL(a.as.number + b.as.number));
            else if (IS_STRING(a) || IS_STRING(b)) concatenate(vm, a, b);
            else {
                run_time_error(vm, "Unreachable add operation");
                return INTERPRET_RUNTIME_ERR;
            }
        } NEXT();
        op_sub:;            BINARY_OP(NUM_VAL, -); NEXT();
        op_mul:;            BINARY_OP(NUM_VAL, *); NEXT();
        op_div:;            BINARY_OP(NUM_VAL, /); NEXT();
        op_equal:;          EQUALS(); NEXT();
        op_not_equal:;      EQUALS(!); NEXT();
        op_less:;           BINARY_OP(BOOL_VAL, <); NEXT();
        op_less_equal:;     BINARY_OP(BOOL_VAL, <=); NEXT();
        op_greater:;        BINARY_OP(BOOL_VAL, >); NEXT();
        op_greater_equal:;  BINARY_OP(BOOL_VAL, >=); NEXT();
        
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