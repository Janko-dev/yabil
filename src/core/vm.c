#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include "../common/common.h"
#include "../common/debug.h"
#include "../common/object.h"
#include "vm.h"
#include "compiler.h"
#include "memory.h"

VM vm;

static void define_native(const char* name, NativeFn function, size_t arity);
static void run_time_error(const char* fmt, ...);

static NativeResult native_clock(int arg_count, Value* args){
    UNUSED(arg_count); UNUSED(args);
    return NATIVE_SUCC(NUM_VAL((double)clock() / CLOCKS_PER_SEC));
}

static NativeResult native_sqrt(int arg_count, Value* args){
    UNUSED(arg_count); UNUSED(args);
    if (!IS_NUM(*args)){
        run_time_error("Argument to native 'sqrt()' function expected to be a number value");
        return NATIVE_ERROR();
    }
    return NATIVE_SUCC(NUM_VAL(sqrt((*args).as.number)));
}

static void reset_stack(){
    vm.sp = vm.stack;
    vm.frame_count = 0;
}

void init_VM(){
    reset_stack();
    vm.objects = NULL;
    init_table(&vm.globals);
    init_table(&vm.strings);

    define_native("clock", native_clock, 0);
    define_native("sqrt", native_sqrt, 1);
}

void free_VM(){
    free_table(&vm.globals);
    free_table(&vm.strings);
    free_objects();
}

static void run_time_error(const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    for (int i = vm.frame_count - 1; i >= 0; i--){
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        size_t line = get_line(&frame->function->chunk.lines, (size_t)(frame->ip - frame->function->chunk.code - 1));
        fprintf(stderr, "[line %d] in ", line);
        if (function->name == NULL){
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    reset_stack();
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

static void define_native(const char* name, NativeFn function, size_t arity){
    push(OBJ_VAL(copy_string(name, strlen(name))));
    push(OBJ_VAL(new_native(function, arity)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

static void concatenate(Value a, Value b){
    if (IS_STRING(a) && IS_STRING(b)){
        concat_string(AS_CSTRING(a), AS_STRING(a)->length, AS_CSTRING(b), AS_STRING(b)->length);
        return;
    }

    if (IS_ARRAY(a) && IS_ARRAY(b)){
        for (size_t i = 0; i < AS_ARRAY(b)->elements.count; i++){
            write_value_array(&AS_ARRAY(a)->elements, AS_ARRAY(b)->elements.values[i]);
        }
        push(a);
        return;
    }

    if (!IS_ARRAY(a) && IS_ARRAY(b)){
        write_value_array(&AS_ARRAY(b)->elements, NIL_VAL);
        for (size_t i = AS_ARRAY(b)->elements.count - 1; i > 0; i--){
            AS_ARRAY(b)->elements.values[i] = AS_ARRAY(b)->elements.values[i-1];
        }
        AS_ARRAY(b)->elements.values[0] = a;
        push(b);
        return;
    }

    if (IS_ARRAY(a) && !IS_ARRAY(b)){
        write_value_array(&AS_ARRAY(a)->elements, b);
        push(a);
        return;
    }

    if (!IS_STRING(a) && IS_STRING(b)){
        char str_a[100];
        to_string(str_a, 100, a);
        concat_string(str_a, strlen(str_a), AS_CSTRING(b), AS_STRING(b)->length);
        return;
    }
    if (!IS_STRING(b) && IS_STRING(a)){
        char str_b[100];
        to_string(str_b, 100, b);
        concat_string(AS_CSTRING(a), AS_STRING(a)->length, str_b, strlen(str_b));
        return;
    }
}

static bool call(ObjFunction* function, uint8_t arg_count){
    if (arg_count != function->arity){
        run_time_error("Expected %d arguments but got %d", function->arity, arg_count);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX){
        run_time_error("Stack overflow error");
        return false;
    }
    
    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.sp - arg_count - 1;
    return true;
}

static bool call_value(Value callee, uint8_t arg_count){
    if (IS_OBJ(callee)){
        switch(OBJ_TYPE(callee)){
            case OBJ_FUNCTION: return call(AS_FUNCTION(callee), arg_count);
            case OBJ_NATIVE: {
                if (arg_count != AS_NATIVE(callee)->arity){
                    run_time_error("Expected %d arguments but got %d", AS_NATIVE(callee)->arity, arg_count);
                    return false;
                }
                NativeFn native = AS_NATIVE_FN(callee);
                NativeResult res = native(arg_count, vm.sp - arg_count);
                if (!res.success) return false;
                vm.sp -= arg_count + 1;
                push(res.result);
                return true;
            } break;
            default: break;
        }
    }
    run_time_error("Can only call functions and classes");
    return false;
}

#define MODULO_OP()                                                 \
    do {                                                            \
        if (!IS_NUM(peek(0)) || !IS_NUM(peek(1))){                  \
            run_time_error("Operands must be numbers");             \
            return INTERPRET_RUNTIME_ERR;                           \
        }                                                           \
        double b = pop().as.number;                                 \
        double a = pop().as.number;                                 \
        push(NUM_VAL((int)a % (int)b));                             \
    } while (0)                                                     \

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
    CallFrame* frame = &vm.frames[vm.frame_count-1];
    static void* dispatch_table[] = {
        #define OPCODE(name) &&name,
        #include "opcodes.h"
        #undef OPCODE
    };
    #define READ_BYTE() (*(frame->ip++))
    #define DISPATCH() goto *dispatch_table[READ_BYTE()]
    #define NEXT() goto start
    #define READ_CONSTANT(index) (frame->function->chunk.constants.values[index])
    #define READ_3_BYTES() (frame->ip[0] | frame->ip[1] << 8 | frame->ip[2] << 16)

#ifdef DEBUG_TRACE_EXECUTION
    printf("\n=== Debug instructions execution ===\n");
#endif //DEBUG_TRACE_EXECUTION
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
            // printf("IP: %d --> %p\n", *vm.ip, vm.ip);
            disassemble_instruction(&frame->function->chunk, (size_t)(frame->ip - frame->function->chunk.code));
#endif //DEBUG_TRACE_EXECUTION 
            DISPATCH();

        op_return:;{
            Value result = pop();
            vm.frame_count--;
            if (vm.frame_count == 0){
                pop();
                return INTERPRET_OK; 
            }
            vm.sp = frame->slots;
            push(result);
            frame = &vm.frames[vm.frame_count - 1];
        } NEXT();
        op_const:;{
            Value constant = READ_CONSTANT(READ_BYTE());
            push(constant);
        } NEXT();
        op_const_long:;{
            size_t const_index = READ_3_BYTES();
            Value constant = READ_CONSTANT(const_index);
            push(constant);
            frame->ip+=3;
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
            else if (IS_ARRAY(a) || IS_ARRAY(b)) concatenate(a, b);
            else {
                run_time_error("undefined add operation");
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
        op_mod:;            MODULO_OP(); NEXT();
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
        op_popn:; {
            size_t num = READ_3_BYTES();
            frame->ip+=3;
            for (size_t i = 0; i < num; i++) pop();
        } NEXT();
        op_define_global:; {
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            table_set(&vm.globals, name, peek(0));
            pop();
        } NEXT();
        op_define_global_long:; {
            size_t const_index = READ_3_BYTES();
            ObjString* name = AS_STRING(READ_CONSTANT(const_index));
            table_set(&vm.globals, name, peek(0));
            pop();
            frame->ip+=3;
        } NEXT();
        op_get_global:;{
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            Value value;
            if (!table_get(&vm.globals, name, &value)){
                run_time_error("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
            push(value);
        } NEXT();
        op_get_global_long:;{
            size_t const_index = READ_3_BYTES();
            ObjString* name = AS_STRING(READ_CONSTANT(const_index));
            Value value;
            if (!table_get(&vm.globals, name, &value)){
                run_time_error("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
            push(value);
            frame->ip+=3;
        } NEXT();
        op_set_global:;{
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            if (table_set(&vm.globals, name, peek(0))){ // is_new_key in hash, so undefined variable
                table_delete(&vm.globals, name);
                run_time_error("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
        } NEXT();
        op_set_global_long:;{
            size_t const_index = READ_3_BYTES();
            ObjString* name = AS_STRING(READ_CONSTANT(const_index));
            if (table_set(&vm.globals, name, peek(0))){ // is_new_key in hash, so undefined variable
                table_delete(&vm.globals, name);
                run_time_error("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
            frame->ip+=3;
        } NEXT();
        op_get_local:;{
            size_t slot = READ_3_BYTES();
            push(frame->slots[slot]);
            frame->ip+=3;
        } NEXT();
        op_set_local:;{
            size_t slot = READ_3_BYTES();
            frame->slots[slot] = peek(0);
            frame->ip+=3;
        } NEXT();
        op_array:; {
            size_t count = READ_BYTE();
            ObjArray* arr = take_array(); 
            for (size_t i = 0; i < count; i++){
                write_value_array(&arr->elements, peek(count-i-1));
            }
            for (size_t i = 0; i < count; i++) pop();
            push(OBJ_VAL(arr));
        } NEXT();
        op_array_long:; {
            size_t count = READ_3_BYTES();
            ObjArray* arr = take_array(); 
            for (size_t i = 0; i < count; i++){
                write_value_array(&arr->elements, peek(count-i-1));
            }
            for (size_t i = 0; i < count; i++) pop();
            push(OBJ_VAL(arr));
            frame->ip+=3;
        } NEXT();
        op_get_index:;{
            if (!IS_NUM(peek(0)) && rintf(peek(0).as.number) == peek(0).as.number){
                run_time_error("Index must be an integer number");
                return INTERPRET_RUNTIME_ERR;
            }
            if (!IS_ARRAY(peek(1))){
                run_time_error("Can only index into Array object");
                return INTERPRET_RUNTIME_ERR;
            }
            Value index = pop();
            Value array = pop();
            push(AS_ARRAY(array)->elements.values[(size_t)index.as.number % AS_ARRAY(array)->elements.count]);
        } NEXT();
        op_jump:; {
            size_t jmp_amt = READ_3_BYTES();
            frame->ip += jmp_amt;
        } NEXT();
        op_loop:; {
            size_t jmp_amt = READ_3_BYTES();
            frame->ip -= jmp_amt;
        } NEXT();
        op_jump_if_false:;{
            size_t jmp_amt = READ_3_BYTES();
            if (is_falsey(peek(0))) frame->ip += jmp_amt;
            else frame->ip+=3;
        } NEXT();
        op_call:;{
            uint8_t arg_count = READ_BYTE();
            if (!call_value(peek(arg_count), arg_count)){
                return INTERPRET_RUNTIME_ERR;
            }
            frame = &vm.frames[vm.frame_count-1];
        } NEXT();
    }
    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef DISPATCH
    #undef NEXT
    #undef READ_3_BYTES
}

InterpreterResult interpret(const char* source){
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERR;

    push(OBJ_VAL(function));
    call(function, 0);
    
    return run();
    // printf("=== hashtable ===\n");
    // for (size_t i = 0; i < vm.strings.cap; i++){
    //     if (vm.strings.entries[i].key) printf("%s\n", vm.strings.entries[i].key->chars);
    //     else printf("(null)\n");
    // }
}