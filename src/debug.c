#include <stdio.h>
#include "debug.h"
#include "value.h"

void disassemble_chunk(Chunk* chunk, const char* name){
    printf("=== %s ===\n", name);
    for (size_t offset = 0; offset < chunk->count;){
        offset = disassemble_instruction(chunk, offset);
    }
}

static size_t simple_instruction(const char* name, size_t offset){
    printf("%s\n", name);
    return offset+1;
}

static size_t constant_instruction(const char* name, Chunk* chunk, size_t offset){
    uint8_t constant = chunk->code[offset+1];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset+2;
}

static size_t constant_long_instruction(const char* name, Chunk* chunk, size_t offset){
    size_t const_index = chunk->code[offset+1] |
                         chunk->code[offset+2] << 8 |
                         chunk->code[offset+3] << 16;
    printf("%-16s %4d '", name, const_index);
    print_value(chunk->constants.values[const_index]);
    printf("'\n");
    return offset+4;
}

size_t disassemble_instruction(Chunk* chunk, size_t offset){
    printf("%04d ", offset);
    size_t line_num = get_line(&chunk->lines, offset);
    if (offset > 0 && line_num == get_line(&chunk->lines, offset-1)){
        printf("   | ");
    } else {
        printf("%4d ", line_num);
    }
    uint8_t ins = chunk->code[offset];
    switch(ins){
        case OP_RETURN: return simple_instruction("OP_RETURN", offset);
        case OP_CONSTANT: return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG: return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_NEGATE: return simple_instruction("OP_NEGATE", offset);
        case OP_ADD: return simple_instruction("OP_ADD", offset);
        case OP_SUB: return simple_instruction("OP_SUB", offset);
        case OP_MUL: return simple_instruction("OP_MUL", offset);
        case OP_DIV: return simple_instruction("OP_DIV", offset);
        
        default:
            printf("Unknown opcode %d\n", ins);
            return offset+1;
    }
}
