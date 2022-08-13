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

void print_obj(Value value){
    switch (OBJ_TYPE(value)){
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}
