#include <string.h>
#include <stdio.h>
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static Obj* alloc_obj(size_t size, ObjType type){
    Obj* object  = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

static ObjString* allocate_string(char* chars, size_t length){
    ObjString* string = (ObjString*)alloc_obj(sizeof(ObjString), OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString* copy_string(const char* chars, size_t length){
    char* heap_chars = ALLOCATE(char, length+1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(heap_chars, length);
}

ObjString* take_string(char* chars, size_t length){
    return allocate_string(chars, length);
}

void print_obj(Value value){
    switch (OBJ_TYPE(value)){
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}
