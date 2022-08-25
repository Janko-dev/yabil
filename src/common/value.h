#ifndef _VALUE_H
#define _VALUE_H

#include "common.h"
#include <string.h>

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjArray ObjArray;

#ifdef NAN_BOXING

#define QNAN     ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define TAG_NIL   1 // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE  3 // 11

typedef uint64_t Value;

#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))

#define BOOL_VAL(value) ((value) ? TRUE_VAL : FALSE_VAL) 
#define NUM_VAL(num)    num_to_val(num)
#define OBJ_VAL(object) (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(object))

#define IS_NUM(value)  (((value) & QNAN) != QNAN)
#define IS_NIL(value)  ((value) == NIL_VAL)
#define IS_OBJ(value)  (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))
#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)

#define AS_NUM(value)   value_to_num(value)
#define AS_BOOL(value)  ((value) == TRUE_VAL)
#define AS_OBJ(value)   ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

static inline Value num_to_val(double num){
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

static inline double value_to_num(Value val){
    double num;
    memcpy(&num, &val, sizeof(Value));
    return num;
}

#else

typedef enum {
    VAL_NUM,
    VAL_BOOL,
    VAL_NIL,
    VAL_OBJ
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        bool boolean;
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_NIL(value)  ((value).type == VAL_NIL)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

#define BOOL_VAL(value) ((Value){.type=VAL_BOOL, {.boolean=value}})
#define NUM_VAL(value)  ((Value){.type=VAL_NUM, {.number=value}})
#define NIL_VAL         ((Value){.type=VAL_NIL, {.number=0}})
#define OBJ_VAL(object) ((Value){.type=VAL_OBJ, {.obj=(Obj*)(object)}})

#define AS_NUM(value)   ((value).as.number)
#define AS_BOOL(value)  ((value).as.boolean)
#define AS_OBJ(value)   ((value).as.obj)
#endif

typedef struct {
    size_t count;
    size_t cap;
    Value* values;
} ValueArray;

typedef struct {
    bool success;
    Value result;
} NativeResult;

#define NATIVE_SUCC(value) ((NativeResult){.success=true, .result=(value)})
#define NATIVE_ERROR() ((NativeResult){.success=false, .result=NIL_VAL})

void init_value_array(ValueArray* val_array);
void free_value_array(ValueArray* val_array);
void write_value_array(ValueArray* val_array, Value value);

bool values_equal(Value a, Value b);

void print_value(Value val);
void print_value_array(ValueArray* arr);

#endif //_VALUE_H