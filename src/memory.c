#include <stdlib.h>
#include "memory.h"

void* reallocate(void* pointer, size_t old_size, size_t new_size){
    (void) old_size;
    if (new_size == 0){
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    if (result == NULL) exit(1);
    return result;
}

void free_object(Obj* object){
    switch (object->type){
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length+1);
            FREE(ObjString, object);
        }
    }
}

void free_objects(){
    Obj* obj = objects;
    while (obj != NULL){
        Obj* next = obj->next;
        free_object(obj);
        obj = next;
    }
}