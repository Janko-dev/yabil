#include <stdlib.h>
#include <string.h>

#include "../core/memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

void init_table(Table* table){
    table->count = 0;
    table->cap = 0;
    table->entries = NULL;
}

void free_table(Table* table){
    FREE_ARRAY(Entry, table->entries, table->cap);
    init_table(table);
}

static Entry* find_entry(Entry* entries, size_t cap, ObjString* key){
    uint32_t index = key->hash % cap;
    Entry* tombstone = NULL;
    while(1){
        Entry* entry = (entries + index);
        if (entry->key == NULL){
            if (IS_NIL(entry->value)){
                // empty entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                // tombstone found
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key || entry->key == NULL)
            return entry;
        index = (index+1) % cap;
    }
}

static void adjust_capacity(Table* table, size_t cap){
    Entry* entries = ALLOCATE(Entry, cap);
    for (size_t i = 0; i < cap; i++){
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }
    table->count = 0;
    // iterate through old entries
    for (size_t i = 0; i < table->cap; i++){
        Entry* entry = table->entries + i;
        if (entry->key == NULL) continue; // skip unused entries
        Entry* dest = find_entry(entries, cap, entry->key); // find new entry in new array based on existing key
        dest->key = entry->key; // copy old to new 
        dest->value = entry->value;
        table->count++;
    }
    FREE_ARRAY(Entry, table->entries, table->cap); // free old entries
    table->entries = entries; // assign new entries
    table->cap = cap;
}

bool table_set(Table* table, ObjString* key, Value value){
    if (table->count + 1 > table->cap * TABLE_MAX_LOAD){
        adjust_capacity(table, GROW_CAP(table->cap));
    }
    Entry* entry = find_entry(table->entries, table->cap, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value)) table->count++;
    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool table_get(Table* table, ObjString* key, Value* value){
    if (table->count == 0) return false;
    Entry* entry = find_entry(table->entries, table->cap, key);
    if (entry->key == NULL) return false;
    *value = entry->value;
    return true;
}

bool table_delete(Table* table, ObjString* key){
    if (table->count == 0) return false;
    Entry* entry = find_entry(table->entries, table->cap, key);
    if (entry->key == NULL) return false;
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_add_all(Table* from, Table* to){
    for (size_t i = 0; i < from->cap; i++){
        Entry* entry = from->entries + i;
        if (entry->key != NULL){
            table_set(to, entry->key, entry->value);
        }
    }
}

ObjString* table_find_string(Table* table, const char* chars, size_t length, uint32_t hash){
    if (table->count == 0) return NULL;
    uint32_t index = hash % table->cap;
    while(1){
        Entry* entry = table->entries + index;
        if (entry->key == NULL){
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
                   entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0)
        {
            return entry->key;
        }
        index = (index+1) % table->cap;
    }
}