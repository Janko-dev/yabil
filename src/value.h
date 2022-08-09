#ifndef _VALUE_H
#define _VALUE_H

#include "common.h"

typedef double Value;

typedef struct {
    size_t count;
    size_t cap;
    Value* values;
} ValueArray;

void init_value_array(ValueArray* val_array);
void free_value_array(ValueArray* val_array);
void write_value_array(ValueArray* val_array, Value value);

void print_value(Value val);

#endif //_VALUE_H