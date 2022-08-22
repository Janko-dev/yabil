#include <stdlib.h>
#include <stdio.h>
#include "memory.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "../common/debug.h"
#include "../common/object.h"
#endif //DEBUG_LOG_GC


void* reallocate(void* pointer, size_t old_size, size_t new_size){
    vm.bytes_allocated += new_size - old_size;
    if (new_size > old_size){
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#else //DEBUG_STRESS_GC
        if (vm.bytes_allocated > vm.next_GC){
            collect_garbage();
        }
#endif
    }
    
    if (new_size == 0){
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    if (result == NULL) {
        fprintf(stderr, "Allocation failed\n");
        exit(1);
    }
    return result;
}

void free_object(Obj* object){
#ifdef DEBUG_LOG_GC
    printf("%p free type %s ", (void*)object, obj_type_tostring(object->type));
    print_value(OBJ_VAL(object));
    printf("\n");
#endif //DEBUG_LOG_GC
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
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE(ObjClosure, object);
        } break;
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, object);
        } break;
        case OBJ_CLASS: {
            FREE(ObjClass, object);
        } break;
        case OBJ_INSTANCE: {
            free_table(&((ObjInstance*)object)->fields);
            FREE(ObjInstance, object);
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
    free(vm.gray_stack);
}

void mark_object(Obj* object){
    if (object == NULL) return;
    if (object->is_marked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark type %s ", (void*)object, obj_type_tostring(object->type));
    print_value(OBJ_VAL(object));
    printf("\n");
#endif //DEBUG_LOG_GC
    object->is_marked = true;

    if (vm.gray_count + 1 > vm.gray_cap){
        vm.gray_cap = GROW_CAP(vm.gray_cap);
        vm.gray_stack = (Obj**)realloc(vm.gray_stack, sizeof(Obj*) * vm.gray_cap);
        if (vm.gray_stack == NULL) {
            fprintf(stderr, "Couldn't realloc GC stack\n");
            exit(1);
        }
    }
    vm.gray_stack[vm.gray_count++] = object;
}

void mark_value(Value value){
    if (IS_OBJ(value)) mark_object(value.as.obj);
}

static void mark_roots(){
    // mark stack
    for (Value* slot = vm.stack; slot < vm.sp; slot++){
        mark_value(*slot);
    }

    // closures
    for (size_t i = 0; i < vm.frame_count; i++){
        mark_object((Obj*)vm.frames[i].closure);
    }

    // open upvalues
    for (ObjUpvalue* upval = vm.open_upvalues; upval != NULL; upval = upval->next){
        mark_object((Obj*)upval);
    }

    // globals
    table_mark(&vm.globals);
    // compiler objects
    mark_compiler_roots();
}

static void mark_array(ValueArray* array){
    for (size_t i = 0; i < array->count; i++){
        mark_value(array->values[i]);
    }
}

static void blacken_object(Obj* object){
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    print_value((OBJ_VAL(object)));
    printf("\n");
#endif //DEBUG_LOG_GC  
    switch (object->type){
        case OBJ_NATIVE: case OBJ_STRING: break;
        case OBJ_UPVALUE: mark_value(((ObjUpvalue*)object)->closed); break;
        case OBJ_FUNCTION:{
            ObjFunction* function = (ObjFunction*)object;
            mark_object((Obj*)function->name);
            mark_array(&function->chunk.constants);
        } break;
        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)object;
            mark_array(&arr->elements);
        } break;
        case OBJ_CLASS: {
            ObjClass* class_obj = (ObjClass*)object;
            mark_object((Obj*)class_obj->name);
        } break;
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            mark_object((Obj*)closure->function);
            for (int i = 0; i < closure->upvalue_count; i++){
                mark_object((Obj*)closure->upvalues[i]);
            }
        } break;
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            mark_object((Obj*)instance->instance_of);
            table_mark(&instance->fields);
        } break;
    }
}

static void trace_references(){
    while(vm.gray_count > 0){
        Obj* object = vm.gray_stack[--vm.gray_count];
        blacken_object(object);
    }
}

static void sweep(){
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while(object != NULL){
        if (object->is_marked){
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL){
                previous->next = object;
            } else {
                vm.objects = object;
            }
            free_object(unreached);
        }
    }
}

void collect_garbage(){
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytes_allocated;
#endif //DEBUG_LOG_GC

    mark_roots();
    trace_references();
    table_remove_white_marked_obj(&vm.strings);
    sweep();

    vm.next_GC = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm.bytes_allocated, before, vm.bytes_allocated, vm.next_GC);
#endif //DEBUG_LOG_GC
}