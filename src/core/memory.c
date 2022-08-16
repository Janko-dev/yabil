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
            FREE(ObjString, object);
        } break;
        case OBJ_ARRAY: {
            free_value_array(&((ObjArray*)object)->elements);
            FREE(ObjArray, object);
        } break;
        case OBJ_FUNCTION: {
            free_chunk(&((ObjFunction*)object)->chunk);
            FREE(ObjFunction, object);
        } break;
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
        } break;
    }
}

void free_objects(){
    Obj* obj = vm.objects;
    while (obj != NULL){
        Obj* next = obj->next;
        free_object(obj);
        obj = next;
    }
}