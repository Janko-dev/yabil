#ifndef _CHUNK_H
#define _CHUNK_H

#include "../common/common.h"
#include "../common/value.h"

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NOT,
    OP_NEGATE,
    OP_CONSTANT_LONG,
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_PRINT,
    OP_POP,
    OP_POPN,
    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_GET_GLOBAL,
    OP_GET_GLOBAL_LONG,
    OP_SET_GLOBAL,
    OP_SET_GLOBAL_LONG,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_ARRAY,
    OP_ARRAY_LONG,
    OP_GET_INDEX,
    OP_RETURN,
} OpCode;

typedef struct {
    size_t line_num;
    size_t count;
} Line;

typedef struct {
    Line* lines_t;
    size_t count;
    size_t cap;
} LineArray;

typedef struct {
    size_t count;
    size_t cap;
    uint8_t* code;
    LineArray lines;
    ValueArray constants;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, size_t line);
size_t add_constant(Chunk* chunk, Value val);
void write_constant(Chunk* chunk, Value val, size_t line);

void init_lines(LineArray* lines);
void free_lines(LineArray* lines);
void write_lines(LineArray* lines, size_t line);
size_t get_line(LineArray* lines, size_t index_ins);
#endif //_CHUNK_H