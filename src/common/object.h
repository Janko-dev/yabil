#ifndef _OBJECT_H
#define _OBJECT_H

#include "common.h"
#include "value.h"
#include "../core/chunk.h"

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjArray {
    Obj obj;
    ValueArray elements;
};

typedef struct {
    Obj obj;
    size_t arity;
    Chunk chunk;
    size_t upvalue_count;
    ObjString* name;
} ObjFunction;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int32_t upvalue_count;
} ObjClosure;

typedef NativeResult (*NativeFn)(int arg_count, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
    size_t arity;
} ObjNative;

struct ObjString {
    Obj obj;
    size_t length;
    uint32_t hash;
    char chars[];
};

static inline bool is_obj_type(Value value, ObjType type){
    return value.type == VAL_OBJ && value.as.obj->type == type;
}

#define OBJ_TYPE(value) ((value).as.obj->type)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_ARRAY(value) is_obj_type(value, OBJ_ARRAY)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)

#define AS_STRING(value) ((ObjString*)(value).as.obj)
#define AS_CSTRING(value) (((ObjString*)(value).as.obj)->chars)
#define AS_ARRAY(value) ((ObjArray*)(value).as.obj)
#define AS_FUNCTION(value) ((ObjFunction*)(value).as.obj)
#define AS_NATIVE_FN(value) (((ObjNative*)(value).as.obj)->function)
#define AS_NATIVE(value) ((ObjNative*)(value).as.obj)
#define AS_CLOSURE(value) ((ObjClosure*)(value).as.obj)

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);
void print_obj(Value value);

ObjArray* take_array();

ObjFunction* new_function();
ObjUpvalue* new_upvalue(Value* slot);
ObjNative* new_native(NativeFn function, size_t arity);
ObjClosure* new_closure(ObjFunction* function);

#endif //_OBJECT_H
