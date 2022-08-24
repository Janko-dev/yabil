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

static NativeResult native_len(int arg_count, Value* args){
    UNUSED(arg_count);
    if (IS_ARRAY(*args)){
        return NATIVE_SUCC(NUM_VAL(AS_ARRAY(*args)->elements.count));
    } else if (IS_STRING(*args)){
        return NATIVE_SUCC(NUM_VAL(strlen(AS_CSTRING(*args))));
    } else {
        run_time_error("length function can only be used on arrays and strings");
        return NATIVE_ERROR();
    }
}

static NativeResult native_clock(int arg_count, Value* args){
    UNUSED(arg_count); UNUSED(args);
    return NATIVE_SUCC(NUM_VAL((double)clock() / CLOCKS_PER_SEC));
}

static NativeResult native_stdin(int arg_count, Value* args){
    UNUSED(arg_count); UNUSED(args);
    char* s = NULL;
    size_t count = 0;
    size_t cap = 0;
    char c;
    while((c = fgetc(stdin)) != '\n'){
        if (cap < count + 1){
            size_t old_cap = cap;
            cap = GROW_CAP(cap);
            s = GROW_ARRAY(char, s, old_cap, cap);
        }
        s[count++] = c;
    }
    NativeResult res = NATIVE_SUCC(OBJ_VAL(copy_string(s, count)));
    FREE(char, s);
    return res;
}

static NativeResult native_sqrt(int arg_count, Value* args){
    UNUSED(arg_count);
    if (!IS_NUM(*args)){
        run_time_error("Argument to native 'sqrt()' function expected to be a number value");
        return NATIVE_ERROR();
    }
    return NATIVE_SUCC(NUM_VAL(sqrt((*args).as.number)));
}

static void reset_stack(){
    vm.sp = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
}

void init_VM(){
    reset_stack();
    vm.objects = NULL;

    vm.gray_cap = 0;
    vm.gray_count = 0;
    vm.gray_stack = NULL;
    vm.bytes_allocated = 0;
    vm.next_GC = 1024*1024;

    init_table(&vm.globals);
    init_table(&vm.strings);

    // vm.init_string = NULL;
    // vm.init_string = copy_string("init", 4);

    define_native("clock", native_clock, 0);
    define_native("sqrt", native_sqrt, 1);
    define_native("input", native_stdin, 0);
    define_native("len", native_len, 1);
}

void free_VM(){
    free_table(&vm.globals);
    free_table(&vm.strings);
    // vm.init_string = NULL;
    free_objects();
}

static void run_time_error(const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    if (vm.frame_count == 0){
        reset_stack();
        return;
    }

    for (int i = vm.frame_count - 1; i >= 0; i--){
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t line = get_line(&frame->closure->function->chunk.lines, (size_t)(frame->ip - frame->closure->function->chunk.code - 1));
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
    pop();
    pop();
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
        pop();
        pop();
        push(a);
        return;
    }

    if (!IS_ARRAY(a) && IS_ARRAY(b)){
        write_value_array(&AS_ARRAY(b)->elements, NIL_VAL);
        for (size_t i = AS_ARRAY(b)->elements.count - 1; i > 0; i--){
            AS_ARRAY(b)->elements.values[i] = AS_ARRAY(b)->elements.values[i-1];
        }
        AS_ARRAY(b)->elements.values[0] = a;
        pop();
        pop();
        push(b);
        return;
    }

    if (IS_ARRAY(a) && !IS_ARRAY(b)){
        write_value_array(&AS_ARRAY(a)->elements, b);
        pop();
        pop();
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

static bool call(ObjClosure* closure, uint8_t arg_count){
    if (arg_count != closure->function->arity){
        printf("DEBUG: %zu\n", closure->function->arity);
        run_time_error("Expected %d arguments but got %d", closure->function->arity, arg_count);
        return false;
    }

    if (vm.frame_count >= FRAMES_MAX){
        run_time_error("Stack overflow error");
        return false;
    }
    
    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.sp - arg_count - 1;
    return true;
}

static bool call_value(Value callee, uint8_t arg_count){
    if (IS_OBJ(callee)){
        switch(OBJ_TYPE(callee)){
            case OBJ_CLOSURE: return call(AS_CLOSURE(callee), arg_count);
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
            case OBJ_CLASS: {
                ObjClass* class_obj = AS_CLASS(callee);
                vm.sp[-arg_count - 1] = OBJ_VAL(new_instance(class_obj));
                
                if (class_obj->init != NULL){
                    return call(class_obj->init, arg_count);
                } else if (arg_count != 0){
                    run_time_error("Expected 0 arguments but got %d arguments", arg_count);
                    return false;
                }
                // Value initializer;
                // if (table_get(&class_obj->methods, vm.init_string, &initializer)){
                //     return call(AS_CLOSURE(initializer), arg_count);
                // } else if (arg_count != 0){
                //     run_time_error("Expected 0 arguments but got %d arguments", arg_count);
                //     return false;
                // }
                return true;
            } break;
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND(callee);
                vm.sp[-arg_count - 1] = bound->receiver;
                return call(bound->method, arg_count);
            }
            default: break;
        }
    }
    run_time_error("Can only call functions and classes");
    return false;
}

static bool invoke_from_class(ObjClass* class_obj, ObjString* name, size_t arg_count){
    if (class_obj->init != NULL && class_obj->init->function->name == name){
        return call(class_obj->init, arg_count);
    }
    
    Value method;
    if (!table_get(&class_obj->methods, name, &method)){
        run_time_error("Undefined property '%s'", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), arg_count);
}

static ObjUpvalue* capture_upvalue(Value* local){
    ObjUpvalue* prev_upval = NULL;
    ObjUpvalue* upvalue = vm.open_upvalues;
    while(upvalue != NULL && upvalue->location > local){
        prev_upval = upvalue;
        upvalue = upvalue->next;
    }
    if (upvalue != NULL && upvalue->location == local){
        return upvalue;
    }
    ObjUpvalue* created_upvalue = new_upvalue(local);
    created_upvalue->next = upvalue;
    if (prev_upval == NULL){
        vm.open_upvalues = created_upvalue;
    } else {
        prev_upval->next = created_upvalue;
    }
    return created_upvalue;
}

static void close_upvalues(Value* last){
    while(vm.open_upvalues != NULL &&
          vm.open_upvalues->location >= last)
    {
        ObjUpvalue* upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

static void define_method(ObjString* name){
    Value method = peek(0);
    ObjClass* class_obj = AS_CLASS(peek(1));
    if (name->length == 4 && memcmp(name->chars, "init", 4) == 0){
        class_obj->init = AS_CLOSURE(method);
    } else {
        table_set(&class_obj->methods, name, method);
    }
    pop();
}

static bool bind_method(ObjClass* class_obj, ObjString* name){
    Value method;
    if (!table_get(&class_obj->methods, name, &method)){
        run_time_error("Undefined property '%s'", name->chars);
        return false;
    }
    ObjBoundMethod* bound = new_bound_method(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

static bool invoke(ObjString* name, size_t arg_count){
    Value receiver = peek(arg_count);
    if (!IS_INSTANCE(receiver)){
        run_time_error("Only instances have methods");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(receiver);
    Value value;
    if (table_get(&instance->fields, name, &value)){
        vm.sp[-arg_count-1] = value;
        return call_value(value, arg_count);
    }
    return invoke_from_class(instance->instance_of, name, arg_count);
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
    #define READ_CONSTANT(index) (frame->closure->function->chunk.constants.values[index])
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
            // printf("offset: %zu\n", (size_t)(frame->ip - frame->closure->function->chunk.code));
            disassemble_instruction(&frame->closure->function->chunk, (size_t)(frame->ip - frame->closure->function->chunk.code));
            // table_print(&vm.globals, "Globals");
#endif //DEBUG_TRACE_EXECUTION 
            DISPATCH();

        op_return:;{
            Value result = pop();
            close_upvalues(frame->slots);
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
            Value b = peek(0);
            Value a = peek(1);
            if (IS_NUM(a) && IS_NUM(b)) {
                pop();
                pop();
                push(NUM_VAL(a.as.number + b.as.number));
            } else if (IS_STRING(a) || IS_STRING(b)) concatenate(a, b);
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
            push(OBJ_VAL(name));
            table_set(&vm.globals, name, peek(1));
            pop();
            pop();
        } NEXT();
        op_define_global_long:; {
            size_t const_index = READ_3_BYTES();
            ObjString* name = AS_STRING(READ_CONSTANT(const_index));
            push(OBJ_VAL(name));
            table_set(&vm.globals, name, peek(1));
            pop();
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
        op_set_upvalue:;{
            size_t slot = READ_3_BYTES();
            *frame->closure->upvalues[slot]->location = peek(0);
            frame->ip+=3;
        } NEXT();
        op_get_upvalue:;{
            size_t slot = READ_3_BYTES();
            push(*frame->closure->upvalues[slot]->location);
            frame->ip+=3;
        } NEXT();
        op_array:; {
            size_t count = READ_BYTE();
            ObjArray* arr = take_array(); 
            push(OBJ_VAL(arr));
            for (size_t i = 0; i < count; i++){
                write_value_array(&arr->elements, peek(count-i));
            }
            pop();
            for (size_t i = 0; i < count; i++) pop();
            push(OBJ_VAL(arr));
        } NEXT();
        op_array_long:; {
            size_t count = READ_3_BYTES();
            ObjArray* arr = take_array();
            push(OBJ_VAL(arr));
            for (size_t i = 0; i < count; i++){
                write_value_array(&arr->elements, peek(count-i));
            }
            pop();
            for (size_t i = 0; i < count; i++) pop();
            push(OBJ_VAL(arr));
            frame->ip+=3;
        } NEXT();
        op_get_index:;{
            if (IS_NUM(peek(0))) {
                if (rintf(peek(0).as.number) != peek(0).as.number){
                    run_time_error("Index must evaluate to integer number");
                    return INTERPRET_RUNTIME_ERR;
                }
                if (!IS_ARRAY(peek(1)) && !IS_STRING(peek(1))){
                    run_time_error("Can only index into Array object or String literal");
                    return INTERPRET_RUNTIME_ERR;
                }
                Value index = pop();
                Value array = pop();
                if (IS_ARRAY(array)) push(AS_ARRAY(array)->elements.values[(size_t)index.as.number % AS_ARRAY(array)->elements.count]);
                else push(OBJ_VAL(copy_string(AS_CSTRING(array) + (int)index.as.number, 1))); 
            } else if (IS_STRING(peek(0))){
                if (!IS_INSTANCE(peek(1))){
                    run_time_error("Can only get field of instance");
                    return INTERPRET_RUNTIME_ERR;
                }
                ObjString* index_str = AS_STRING(pop());
                ObjInstance* instance = AS_INSTANCE(pop());
                Value val;
                if (table_get(&instance->fields, index_str, &val)){
                    push(val);
                } else {
                    run_time_error("Undefined property '%s'", index_str->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
            } else {
                run_time_error("Undefined indexing operation");
                return INTERPRET_RUNTIME_ERR;
            }
        } NEXT();
        op_set_index:;{
            if (IS_NUM(peek(1))) {
                if (rintf(peek(1).as.number) != peek(1).as.number){
                    run_time_error("Index must evaluate to integer number");
                    return INTERPRET_RUNTIME_ERR;
                }
                if (!IS_ARRAY(peek(2)) && !IS_STRING(peek(2))){
                    run_time_error("Can only index into Array object or String literal");
                    return INTERPRET_RUNTIME_ERR;
                }
                Value new_val = pop();
                Value index = pop();
                if (IS_ARRAY(peek(0))){
                    AS_ARRAY(peek(0))->elements.values[(size_t)index.as.number % AS_ARRAY(peek(0))->elements.count] = new_val;
                } else if (IS_STRING(new_val) && AS_STRING(new_val)->length == 1){
                    AS_CSTRING(peek(0))[(size_t)index.as.number % AS_STRING(peek(0))->length] = AS_CSTRING(new_val)[0];
                } else {
                    run_time_error("Can only assign characters to indices of strings");
                    return INTERPRET_RUNTIME_ERR;
                }
            } else if (IS_STRING(peek(1))){
                if (!IS_INSTANCE(peek(2))){
                    run_time_error("Can only set field of instance");
                    return INTERPRET_RUNTIME_ERR;
                }
                table_set(&AS_INSTANCE(peek(2))->fields, AS_STRING(peek(1)), peek(0));
                Value new_val = pop();
                pop();
                pop();
                push(new_val);
            } else {
                run_time_error("Undefined indexing operation");
                return INTERPRET_RUNTIME_ERR;
            }
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
        op_closure:;{
            ObjFunction* function = AS_FUNCTION(READ_CONSTANT(READ_BYTE()));
            push(OBJ_VAL(function));
            ObjClosure* closure = new_closure(function);
            pop();
            push(OBJ_VAL(closure));
            for (int32_t i = 0; i < closure->upvalue_count; i++){
                uint8_t is_local = READ_BYTE();
                size_t index = READ_3_BYTES();
                frame->ip+=3;
                if (is_local){
                    closure->upvalues[i] = capture_upvalue(frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
        } NEXT();
        op_closure_long:;{
            ObjFunction* function = AS_FUNCTION(READ_CONSTANT(READ_3_BYTES()));
            push(OBJ_VAL(function));
            ObjClosure* closure = new_closure(function);
            pop();
            push(OBJ_VAL(closure));
            frame->ip+=3;
            for (int32_t i = 0; i < closure->upvalue_count; i++){
                uint8_t is_local = READ_BYTE();
                size_t index = READ_3_BYTES();
                frame->ip+=3;
                if (is_local){
                    closure->upvalues[i] = capture_upvalue(frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
        } NEXT();
        op_close_upvalue:;{
            close_upvalues(vm.sp - 1);
            pop();
        } NEXT();
        op_class:; {
            ObjClass* class_obj = new_class(AS_STRING(READ_CONSTANT(READ_BYTE())));  
            push(OBJ_VAL(class_obj));
            
        } NEXT();
        op_method:; {
            define_method(AS_STRING(READ_CONSTANT(READ_BYTE())));
        } NEXT();
        op_get_prop:;{
            if (!IS_INSTANCE(peek(0))){
                run_time_error("Only instances have properties");
                return INTERPRET_RUNTIME_ERR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(0));
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            Value value;
            if (table_get(&instance->fields, name, &value)){
                pop();
                push(value);
            } else if (!bind_method(instance->instance_of, name)){
                return INTERPRET_RUNTIME_ERR;
            }
        } NEXT();
        op_get_prop_long:;{
            if (!IS_INSTANCE(peek(0))){
                run_time_error("Only instances have properties");
                return INTERPRET_RUNTIME_ERR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(0));
            ObjString* name = AS_STRING(READ_CONSTANT(READ_3_BYTES()));
            frame->ip+=3;
            Value value;
            if (table_get(&instance->fields, name, &value)){
                pop();
                push(value);
            } else {
                run_time_error("Undefined property '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
        } NEXT();
        op_set_prop:;{
            if (!IS_INSTANCE(peek(1))){
                run_time_error("Only properties of instances can be set to a value");
                return INTERPRET_RUNTIME_ERR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(1));
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            table_set(&instance->fields, name, peek(0));
            Value value = pop();
            pop();
            push(value);
        } NEXT();
        op_set_prop_long:;{
            if (!IS_INSTANCE(peek(1))){
                run_time_error("Only properties of instances can be set to a value");
                return INTERPRET_RUNTIME_ERR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(1));
            ObjString* name = AS_STRING(READ_CONSTANT(READ_3_BYTES()));
            frame->ip+=3;
            table_set(&instance->fields, name, peek(0));
            Value value = pop();
            pop();
            push(value);
        } NEXT();
        op_invoke:;{
            ObjString* method = AS_STRING(READ_CONSTANT(READ_BYTE()));
            size_t arg_count = READ_BYTE();
            if (!invoke(method, arg_count)){
                return INTERPRET_RUNTIME_ERR;
            }
            frame = &vm.frames[vm.frame_count - 1];
        } NEXT();
        op_inherit:;{
            Value superclass = peek(1);
            if (!IS_CLASS(superclass)){
                run_time_error("Can only inherit from another class");
                return INTERPRET_RUNTIME_ERR;
            }
            ObjClass* subclass = AS_CLASS(peek(0));
            table_add_all(&AS_CLASS(superclass)->methods, &subclass->methods);
            pop();
        } NEXT();
        op_get_super:;{
            ObjString* name = AS_STRING(READ_CONSTANT(READ_BYTE()));
            ObjClass* super_class = AS_CLASS(pop());
            if (!bind_method(super_class, name)){
                return INTERPRET_RUNTIME_ERR;
            }
        } NEXT();
        op_super_invoke:;{
            ObjString* method = AS_STRING(READ_CONSTANT(READ_BYTE()));
            size_t arg_count = READ_BYTE();
            ObjClass* super_class = AS_CLASS(pop());
            if (!invoke_from_class(super_class, method, arg_count)){
                return INTERPRET_RUNTIME_ERR;
            }
            frame = &vm.frames[vm.frame_count - 1];
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
    ObjClosure* closure = new_closure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);
    
    return run();
}