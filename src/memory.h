#ifndef _MEMORY_H
#define _MEMORY_H

#include "common.h"
#include "object.h"
#include "vm.h"

#define GROW_CAP(cap) ((cap) < 8 ? 8 : (cap) * 2)
#define GROW_ARRAY(type, ptr, old_count, new_count) \
    (type*)reallocate(ptr, sizeof(type) * (old_count), sizeof(type) * (new_count))\
    
#define FREE_ARRAY(type, ptr, cap) \
    reallocate(ptr, sizeof(type) * (cap), 0)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count)) \

#define FREE(type, ptr) reallocate(ptr, sizeof(type), 0)

void* reallocate(void* pointer, size_t old_size, size_t new_size);
void free_objects();

#endif //_MEMORY_H

