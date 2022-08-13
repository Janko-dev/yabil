#ifndef _VALUE_H
#define _VALUE_H

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjArray ObjArray;

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

typedef struct {
    size_t count;
    size_t cap;
    Value* values;
} ValueArray;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_NIL(value)  ((value).type == VAL_NIL)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

#define BOOL_VAL(value) ((Value){.type=VAL_BOOL, {.boolean=value}})
#define NUM_VAL(value)  ((Value){.type=VAL_NUM, {.number=value}})
#define NIL_VAL         ((Value){.type=VAL_NIL, {.number=0}})
#define OBJ_VAL(object) ((Value){.type=VAL_OBJ, {.obj=(Obj*)object}})

void init_value_array(ValueArray* val_array);
void free_value_array(ValueArray* val_array);
void write_value_array(ValueArray* val_array, Value value);

bool values_equal(Value a, Value b);

void print_value(Value val);

#endif //_VALUE_H