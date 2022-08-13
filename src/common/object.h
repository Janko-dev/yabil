#ifndef _OBJECT_H
#define _OBJECT_H

#include "common.h"
#include "value.h"

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

// var x = [2, 6, 1] + [2];
// var y = ["string", true, nil, 8.22];
// y[0] = 
struct ObjArray {
    Obj obj;
    ValueArray elements;
};

struct ObjString {
    Obj obj;
    size_t length;
    uint32_t hash;
    char chars[];
};

static inline bool is_obj_type(Value value, ObjType type){
    return value.type == VAL_OBJ && value.as.obj->type == type;
}

#define OBJ_TYPE(value) (value.as.obj->type)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_ARRAY(value) is_obj_type(value, OBJ_ARRAY)

#define AS_STRING(value) ((ObjString*)value.as.obj)
#define AS_CSTRING(value) (((ObjString*)value.as.obj)->chars)

#define AS_ARRAY(value) ((ObjArray*)value.as.obj)

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);
void print_obj(Value value);

ObjArray* take_array();


#endif //_OBJECT_H
