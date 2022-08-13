#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../common/common.h"
#include "../common/debug.h"
#include "../common/object.h"
#include "vm.h"
#include "compiler.h"
#include "memory.h"

VM vm;

static void run_time_error(const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    size_t line = get_line(&vm.chunk->lines, (size_t)(vm.ip - vm.chunk->code - 1));
    fprintf(stderr, "\n[line %d] in script\n", line);
}

static void reset_stack(){
    vm.sp = vm.stack;
}

void init_VM(){
    reset_stack();
    vm.objects = NULL;
    init_table(&vm.globals);
    init_table(&vm.strings);
}

void free_VM(){
    free_table(&vm.globals);
    free_table(&vm.strings);
    free_objects();
}

void push(Value val){
    *(vm.sp++) = val;
}

Value pop(){
    return *(--vm.sp);
}

Value peek(int dist){
    return vm.sp[-1 - dist];
}

static bool is_falsey(Value val){
    return (IS_NIL(val) || (IS_BOOL(val) && !val.as.boolean));
}

static void concat_string(char* a, size_t size_a, char* b, size_t size_b){
    size_t length = size_a + size_b;
    char* chars = ALLOCATE(char, length+1);
    memcpy(chars, a, size_a);
    memcpy(chars + size_a, b, size_b);
    chars[length] = '\0';
    ObjString* result = take_string(chars, length);
    push(OBJ_VAL(result));
}

static void to_string(char* s, size_t size, Value val){
    switch(val.type){
        case VAL_NUM: snprintf(s, size, "%g", val.as.number); break;
        case VAL_BOOL: strncpy(s, val.as.boolean ? "true" : "false", size); break;
        case VAL_NIL:  strncpy(s, "(nil)", size); break;
        default: break;
    }
}

static void concatenate(Value a, Value b){
    if (IS_STRING(a) && IS_STRING(b)){
        concat_string(AS_CSTRING(a), AS_STRING(a)->length, AS_CSTRING(b), AS_STRING(b)->length);
        return;
    }
    if (!IS_STRING(a)){
        char str_a[100];
        to_string(str_a, 100, a);
        concat_string(str_a, strlen(str_a), AS_CSTRING(b), AS_STRING(b)->length);
        return;
    }
    if (!IS_STRING(b)){
        char str_b[100];
        to_string(str_b, 100, b);
        concat_string(AS_CSTRING(a), AS_STRING(a)->length, str_b, strlen(str_b));
        return;
    }
}

#define BINARY_OP(val_type, op)                                     \
    do {                                                            \
        if (!IS_NUM(peek(0)) || !IS_NUM(peek(1))){                  \
            run_time_error("Operands must be numbers");             \
            return INTERPRET_RUNTIME_ERR;                           \
        }                                                           \
        double b = pop().as.number;                                 \
        double a = pop().as.number;                                 \
        push(val_type(a op b));                                     \
    } while (0)                                                     \

#define EQUALS(not)                                                 \
    do {                                                            \
        Value b = pop();                                            \
        Value a = pop();                                            \
        push(BOOL_VAL(not values_equal(a, b)));                     \
    } while (0)                                                     \

static InterpreterResult run(){
    static void* dispatch_table[] = {
        #define OPCODE(name) &&name,
        #include "opcodes.h"
        #undef OPCODE
    };
    #define READ_BYTE() (*vm.ip++)
    #define DISPATCH() goto *dispatch_table[READ_BYTE()]
    #define NEXT() goto start
    #define READ_CONSTANT(index) (vm.chunk->constants.values[index])
    
    for (;;){
        start:;
#ifdef DEBUG_TRACE_EXECUTION
            printf("      ");
            for (Value* slot = vm.stack; slot < vm.sp; slot++){
                printf("[");
                print_value(*slot);
                printf("]");
            }
            printf("\n");
            disassemble_instruction(vm.chunk, (size_t)(vm.ip - vm.chunk->code));
#endif //DEBUG_TRACE_EXECUTION 
            DISPATCH();

        op_return:;{
            return INTERPRET_OK;
        }
        op_const:;{
            Value constant = READ_CONSTANT(READ_BYTE());
            push(constant);
        } NEXT();
        op_const_long:;{
            size_t const_index = vm.ip[0] | vm.ip[1] << 8 | vm.ip[2] << 16;
            Value constant = READ_CONSTANT(const_index);
            push(constant);
            vm.ip+=3;
        } NEXT();
        op_nil:; push(NIL_VAL); NEXT();
        op_true:; push(BOOL_VAL(true)); NEXT();
        op_false:; push(BOOL_VAL(false)); NEXT();
        op_negate:;{
            if (!IS_NUM(peek(0))){
                run_time_error("operand must be a number");
                return INTERPRET_RUNTIME_ERR;
            }
            (vm.sp-1)->as.number *= -1; // access top of stack directly and negate its value
        } NEXT();
        op_not:; {
            *(vm.sp-1) = BOOL_VAL(is_falsey(*(vm.sp-1)));
        } NEXT();
        op_add:;{
            Value b = pop();
            Value a = pop();
            if (IS_NUM(a) && IS_NUM(b)) push(NUM_VAL(a.as.number + b.as.number));
            else if (IS_STRING(a) || IS_STRING(b)) concatenate(a, b);
            else {
                run_time_error("Unreachable add operation");
                return INTERPRET_RUNTIME_ERR;
            }
        } NEXT();
        op_div:;{
            if (IS_NUM(peek(0)) && peek(0).as.number == 0){
                run_time_error("Divide by 0 error");
                return INTERPRET_RUNTIME_ERR;
            }
            BINARY_OP(NUM_VAL, /);
        } NEXT();
        op_sub:;            BINARY_OP(NUM_VAL, -); NEXT();
        op_mul:;            BINARY_OP(NUM_VAL, *); NEXT();
        op_equal:;          EQUALS(); NEXT();
        op_not_equal:;      EQUALS(!); NEXT();
        op_less:;           BINARY_OP(BOOL_VAL, <); NEXT();
        op_less_equal:;     BINARY_OP(BOOL_VAL, <=); NEXT();
        op_greater:;        BINARY_OP(BOOL_VAL, >); NEXT();
        op_greater_equal:;  BINARY_OP(BOOL_VAL, >=); NEXT();
        op_print:; {
            print_value(pop());
            printf("\n");
        } NEXT();
        op_pop:; pop(); NEXT();
        op_define_global:; {
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            table_set(&vm.globals, name, peek(0));
            pop();
        } NEXT();
        op_define_global_long:; {
            size_t const_index = vm.ip[0] | vm.ip[1] << 8 | vm.ip[2] << 16;
            ObjString* name = AS_STRING(READ_CONSTANT(const_index));
            table_set(&vm.globals, name, peek(0));
            pop();
            vm.ip+=3;
        } NEXT();
        op_get_global:;{
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            Value value;
            if (!table_get(&vm.globals, name, &value)){
                run_time_error("Undefined variable '%s'", name->chars);
            }
            push(value);
        } NEXT();
        op_get_global_long:;{
            size_t const_index = vm.ip[0] | vm.ip[1] << 8 | vm.ip[2] << 16;
            ObjString* name = AS_STRING(READ_CONSTANT(const_index));
            Value value;
            if (!table_get(&vm.globals, name, &value)){
                run_time_error("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
            push(value);
            vm.ip+=3;
        } NEXT();
    }
    #undef READ_BYTE
    #undef DISPATCH
    #undef READ_CONSTANT
    #undef NEXT
}

InterpreterResult interpret(const char* source){
    Chunk chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)){
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERR;
    }

    vm.chunk = &chunk;
    vm.ip = chunk.code;
    
    InterpreterResult result = run();
    // printf("=== hashtable ===\n");
    // for (size_t i = 0; i < vm.strings.cap; i++){
    //     if (vm.strings.entries[i].key) printf("%s\n", vm.strings.entries[i].key->chars);
    //     else printf("(null)\n");
    // }
    free_chunk(&chunk);
    return result;
}