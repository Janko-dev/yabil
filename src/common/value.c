#include <stdio.h> 
#include <string.h> 
#include "../core/memory.h"
#include "value.h"
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
#ifdef NAN_BOXING
    if (IS_NUM(a) && IS_NUM(b)){
        return AS_NUM(a) == AS_NUM(b);
    }
    return a == b;
#else
    if (a.type != b.type) return false;
    switch (a.type){
        case VAL_BOOL:  return a.as.boolean == b.as.boolean; 
        case VAL_NIL:   return true; 
        case VAL_NUM:   return a.as.number == b.as.number; 
        case VAL_OBJ:   return a.as.obj == b.as.obj; 
        default:        return false;
    }
#endif
}

void print_value(Value val){
#ifdef NAN_BOXING
    if (IS_BOOL(val)){
        printf(AS_BOOL(val) ? "true" : "false");
    } else if (IS_NIL(val)){
        printf("nil");
    } else if (IS_NUM(val)){
        printf("%g", AS_NUM(val));
    } else if (IS_OBJ(val)){
        print_obj(val);
    }
#else
    switch(val.type){
        case VAL_NUM:  printf("%g", val.as.number); break;
        case VAL_NIL:  printf("nil"); break;
        case VAL_BOOL: printf(val.as.boolean ? "true" : "false"); break;
        case VAL_OBJ:  print_obj(val); break;
    }
#endif
}


void print_value_array(ValueArray* arr){
    printf("======== Value array ========");
    for (size_t i = 0; i < arr->count; i++){
        print_value(arr->values[i]);
        printf("\n");
    }
    printf("============================");
}