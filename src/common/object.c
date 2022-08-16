#include <string.h>
#include <stdio.h>
#include "../core/memory.h"
#include "../core/vm.h"
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

static Obj* alloc_obj(size_t size, ObjType type){
    Obj* object  = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static ObjString* allocate_string(char* chars, size_t length, uint32_t hash){
    ObjString* string = (ObjString*)alloc_obj(sizeof(ObjString) + sizeof(char) * length + 1, OBJ_STRING);
    string->length = length;
    strncpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->hash = hash;
    table_set(&vm.strings, string, NIL_VAL);
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
        case OBJ_FUNCTION: {
            if (AS_FUNCTION(value)->name == NULL) {
                printf("<Script>");
                return;
            }
            printf("<fn %s>", AS_FUNCTION(value)->name->chars);
        } break;
        case OBJ_NATIVE: {
            printf("<native fn>");
        } break;
    }
}
