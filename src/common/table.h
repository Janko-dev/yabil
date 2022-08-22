#ifndef _TABLE_H
#define _TABLE_H

#include "common.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    size_t count;
    size_t cap;
    Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);
bool table_set(Table* table, ObjString* key, Value value);
bool table_get(Table* table, ObjString* key, Value* value);
bool table_delete(Table* table, ObjString* key);
void table_add_all(Table* from, Table* to);
void table_print(Table* table, const char* name);

void table_mark(Table* table);
void table_remove_white_marked_obj(Table* table);

ObjString* table_find_string(Table* table, const char* chars, size_t length, uint32_t hash);

#endif //_TABLE_H