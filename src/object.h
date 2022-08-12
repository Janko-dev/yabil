#ifndef _OBJECT_H
#define _OBJECT_H

#include "common.h"
#include "value.h"

typedef enum {
    OBJ_STRING
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    size_t length;
    char chars[];
};

static inline bool is_obj_type(Value value, ObjType type){
    return value.type == VAL_OBJ && value.as.obj->type == type;
}

#define OBJ_TYPE(value) (value.as.obj->type)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString*)value.as.obj)
#define AS_CSTRING(value) (((ObjString*)value.as.obj)->chars)

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);
void print_obj(Value value);


#endif //_OBJECT_H
