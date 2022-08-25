#ifndef _OBJECT_H
#define _OBJECT_H

#include "common.h"
#include "value.h"
#include "table.h"
#include "../core/chunk.h"

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked;
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

typedef struct {
    Obj obj;
    ObjString* name;
    ObjClosure* init;
    Table methods;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* instance_of;
    Table fields;
} ObjInstance;

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

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

static inline bool is_obj_type(Value value, ObjType type){
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_ARRAY(value) is_obj_type(value, OBJ_ARRAY)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_CLASS(value) is_obj_type(value, OBJ_CLASS)
#define IS_INSTANCE(value) is_obj_type(value, OBJ_INSTANCE)
#define IS_BOUND(value) is_obj_type(value, OBJ_BOUND_METHOD)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_ARRAY(value) ((ObjArray*)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE_FN(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_NATIVE(value) ((ObjNative*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))
#define AS_BOUND(value) ((ObjBoundMethod*)AS_OBJ(value))

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);
ObjArray* take_array();

ObjFunction* new_function();
ObjUpvalue* new_upvalue(Value* slot);
ObjNative* new_native(NativeFn function, size_t arity);
ObjClosure* new_closure(ObjFunction* function);
ObjClass* new_class(ObjString* name);
ObjInstance* new_instance(ObjClass* instance_of);
ObjBoundMethod* new_bound_method(Value receiver, ObjClosure* method);

void print_obj(Value value);

#ifdef DEBUG_LOG_GC
const char* obj_type_tostring(ObjType type);
#endif

#endif //_OBJECT_H
