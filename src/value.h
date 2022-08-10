#ifndef _VALUE_H
#define _VALUE_H

#include "common.h"

typedef enum {
    VAL_NUM,
    VAL_BOOL,
    VAL_NIL
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        bool boolean;
    } as;
} Value;

typedef struct {
    size_t count;
    size_t cap;
    Value* values;
} ValueArray;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_NIL(Value)  ((value).type == VAL_NIL)

#define BOOL_VAL(value) ((Value){.type=VAL_BOOL, {.boolean=value}})
#define NUM_VAL(value)  ((Value){.type=VAL_NUM, {.number=value}})
#define NIL_VAL         ((Value){.type=VAL_NIL, {.number=0}})

void init_value_array(ValueArray* val_array);
void free_value_array(ValueArray* val_array);
void write_value_array(ValueArray* val_array, Value value);

void print_value(Value val);

#endif //_VALUE_H