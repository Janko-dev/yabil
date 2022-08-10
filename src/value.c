#include <stdio.h> 
#include "value.h"
#include "memory.h"

void init_value_array(ValueArray* val_array){
    val_array->cap = 0;
    val_array->count = 0;
    val_array->values = NULL;
}

void free_value_array(ValueArray* val_array){
    FREE_ARRAY(Value, val_array->values, val_array->cap);
    init_value_array(val_array);
}

void write_value_array(ValueArray* val_array, Value value){
    if (val_array->cap < val_array->count + 1){
        size_t old_cap = val_array->cap;
        val_array->cap = GROW_CAP(val_array->cap);
        val_array->values = GROW_ARRAY(Value, val_array->values, old_cap, val_array->cap);
    }

    val_array->values[val_array->count++] = value;
}

void print_value(Value val){
    printf("%g", val.as.number);
}