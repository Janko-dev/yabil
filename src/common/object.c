#include <string.h>
#include <stdio.h>
#include "../core/memory.h"
#include "../core/vm.h"
#include "../core/lexer.h"
#include "object.h"
#include "value.h"
#include "table.h"

// hashing algorithm: FNV-1a
static uint32_t hash_string(const char* key, size_t length){
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; i++){
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

#ifdef DEBUG_LOG_GC
const char* obj_type_tostring(ObjType type){
    switch (type){
        case OBJ_STRING: return "OBJ_STRING"; 
        case OBJ_ARRAY: return "OBJ_ARRAY"; 
        case OBJ_FUNCTION: return "OBJ_FUNCTION"; 
        case OBJ_NATIVE: return "OBJ_NATIVE"; 
        case OBJ_CLOSURE: return "OBJ_CLOSURE"; 
        case OBJ_UPVALUE: return "OBJ_UPVALUE"; 
        case OBJ_CLASS: return "OBJ_CLASS"; 
        case OBJ_INSTANCE: return "OBJ_INSTANCE";
        default: return "";
    }
}
#endif //DEBUG_LOG_GC

static Obj* alloc_obj(size_t size, ObjType type){
    Obj* object  = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    object->is_marked = false;
    vm.objects = object;
#ifdef DEBUG_LOG_GC
    printf("%p allocate %d bytes for type '%s'\n", (void*)object, size, obj_type_tostring(type));
#endif //DEBUG_LOG_GC
    return object;
}

extern Parser parser;

static ObjString* allocate_string(char* chars, size_t length, uint32_t hash){
    ObjString* string = (ObjString*)alloc_obj(sizeof(ObjString) + sizeof(char) * length + 1, OBJ_STRING);
    string->length = length;
    strncpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->hash = hash;
    push(OBJ_VAL(string)); // push string on stack so GC doesn't clean
    table_set(&vm.strings, string, NIL_VAL);
    pop(); // pop string from stack
    return string;
}

ObjString* copy_string(const char* chars, size_t length){
    uint32_t hash = hash_string(chars, length);
    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    char* heap_chars = ALLOCATE(char, length+1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(heap_chars, length, hash);
}

ObjString* take_string(char* chars, size_t length){
    uint32_t hash = hash_string(chars, length);
    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length);
        return interned;
    }
    return allocate_string(chars, length, hash);
}

ObjArray* take_array(){
    ObjArray* array = (ObjArray*)alloc_obj(sizeof(ObjArray), OBJ_ARRAY);
    init_value_array(&array->elements);
    return array;
}

ObjFunction* new_function(){
    ObjFunction* func = (ObjFunction*)alloc_obj(sizeof(ObjFunction), OBJ_FUNCTION);
    func->arity = 0;
    func->upvalue_count = 0;
    func->name = NULL;
    init_chunk(&func->chunk);
    return func;
}

ObjNative* new_native(NativeFn function, size_t arity){
    ObjNative* native = (ObjNative*)alloc_obj(sizeof(ObjNative), OBJ_NATIVE);
    native->function = function;
    native->arity = arity;
    return native;
}

ObjClosure* new_closure(ObjFunction* function){
    ObjUpvalue** upvalues = (ObjUpvalue**)reallocate(NULL, 0, sizeof(ObjUpvalue*) * function->upvalue_count);
    for (size_t i = 0; i < function->upvalue_count; i++) upvalues[i] = NULL;
    ObjClosure* closure = (ObjClosure*)alloc_obj(sizeof(ObjClosure), OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

ObjUpvalue* new_upvalue(Value* slot){
    ObjUpvalue* upval = (ObjUpvalue*)alloc_obj(sizeof(ObjUpvalue), OBJ_UPVALUE);
    upval->location = slot;
    upval->next = NULL;
    upval->closed = NIL_VAL;
    return upval;
}

ObjClass* new_class(ObjString* name){
    ObjClass* class_obj = (ObjClass*)alloc_obj(sizeof(ObjClass), OBJ_CLASS);
    class_obj->name = name;
    class_obj->init = NULL;
    init_table(&class_obj->methods);
    return class_obj;
}

ObjInstance* new_instance(ObjClass* instance_of){
    ObjInstance* instance = (ObjInstance*)alloc_obj(sizeof(ObjInstance), OBJ_INSTANCE);
    instance->instance_of = instance_of;
    init_table(&instance->fields);
    return instance;
}

ObjBoundMethod* new_bound_method(Value receiver, ObjClosure* method){
    ObjBoundMethod* bound = (ObjBoundMethod*)alloc_obj(sizeof(ObjBoundMethod), OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

static void print_function(ObjFunction* fn){
    if (fn->name == NULL) {
        printf("<Script>");
        return;
    }
    printf("<fn %s>", fn->name->chars);
}

void print_obj(Value value){
    switch (OBJ_TYPE(value)){
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_ARRAY: {
            printf("[ ");
            for (size_t i = 0; i < AS_ARRAY(value)->elements.count; i++){
                print_value(AS_ARRAY(value)->elements.values[i]);
                if (i != AS_ARRAY(value)->elements.count - 1) printf(", ");
            }
            printf(" ]");
        } break;
        case OBJ_FUNCTION: print_function(AS_FUNCTION(value)); break;
        case OBJ_NATIVE: {
            printf("<native fn>");
        } break;
        case OBJ_CLOSURE: print_function(AS_CLOSURE(value)->function); break;
        case OBJ_UPVALUE: printf("Upvalue"); break;
        case OBJ_CLASS: printf("<Class %s>", AS_CLASS(value)->name->chars); break;
        case OBJ_INSTANCE: printf("<instance of %s>", AS_INSTANCE(value)->instance_of->name->chars); break;
        case OBJ_BOUND_METHOD: print_function(AS_BOUND(value)->method->function); break;
    }
}
