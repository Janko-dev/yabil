#include <stdio.h> 
#include <string.h> 
#include "value.h"
#include "memory.h"
#include "object.h"

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

bool values_equal(Value a, Value b){
    if (a.type != b.type) return false;
    switch (a.type){
        case VAL_BOOL:  return a.as.boolean == b.as.boolean; 
        case VAL_NIL:   return true; 
        case VAL_NUM:   return a.as.number == b.as.number; 
        case VAL_OBJ:   return a.as.obj == b.as.obj; 
        default:        return false;
    }
}

void print_value(Value val){
    switch(val.type){
        case VAL_NUM:  printf("%g", val.as.number); break;
        case VAL_NIL:  printf("nil"); break;
        case VAL_BOOL: printf(val.as.boolean ? "true" : "false"); break;
        case VAL_OBJ:  print_obj(val); break;
    }
    
}