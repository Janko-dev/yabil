#include <stdlib.h>
#include "chunk.h"
#include "memory.h"

void init_lines(LineArray* lines){
    lines->cap = 0;
    lines->count = 0;
    lines->lines_t = NULL;
}

void free_lines(LineArray* lines){
    FREE_ARRAY(Line, lines->lines_t, lines->cap);
    init_lines(lines);
}

void write_lines(LineArray* lines, size_t line){
    if (lines->cap < lines->count + 1){
        size_t old_cap = lines->cap;
        lines->cap = GROW_CAP(lines->cap);
        lines->lines_t = GROW_ARRAY(Line, lines->lines_t, old_cap, lines->cap);
    }

    if (lines->count == 0) {
        lines->lines_t[lines->count++] = (Line){
            .line_num=line,
            .count=1
        };
        return;
    }

    if (lines->lines_t[lines->count-1].line_num == line){
        lines->lines_t[lines->count-1].count++;
    } else {
        lines->lines_t[lines->count++] = (Line){
            .line_num=line,
            .count=1
        };
    }
}

size_t get_line(LineArray* lines, size_t index_ins){
    
    size_t index = 0;
    for (size_t i = 0; i < lines->count; i++){
        index += lines->lines_t[i].count;
        if (index_ins < index){
            return lines->lines_t[i].line_num;
        }
    }
    return 0;
}

void init_chunk(Chunk* chunk){
    chunk->cap = 0;
    chunk->count = 0;
    chunk->code = NULL;
    init_value_array(&chunk->constants);
    init_lines(&chunk->lines);
}

void free_chunk(Chunk* chunk){
    FREE_ARRAY(uint8_t, chunk->code, chunk->cap);
    free_value_array(&chunk->constants);
    free_lines(&chunk->lines);
    init_chunk(chunk);
}

void write_chunk(Chunk* chunk, uint8_t byte, size_t line){
    if (chunk->cap < chunk->count + 1){
        size_t old_cap = chunk->cap;
        chunk->cap = GROW_CAP(chunk->cap);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_cap, chunk->cap);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
    write_lines(&chunk->lines, line);
}

void write_constant(Chunk* chunk, Value val, size_t line){
    size_t const_index = add_constant(chunk, val);
    write_chunk(chunk, const_index, line);
    write_chunk(chunk, const_index >> 8, line);
    write_chunk(chunk, const_index >> 16, line);
}

size_t add_constant(Chunk* chunk, Value val){
    push(val);
    write_value_array(&chunk->constants, val);
    pop();
    return chunk->constants.count - 1;
}