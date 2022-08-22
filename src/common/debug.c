#include <stdio.h>
#include "debug.h"
#include "value.h"
#include "object.h"

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

static size_t long_instruction(const char* name, Chunk* chunk, size_t offset){
    size_t const_index = chunk->code[offset+1] |
                         chunk->code[offset+2] << 8 |
                         chunk->code[offset+3] << 16;
    printf("%-16s %4d\n", name, const_index);
    return offset+4;
}

static size_t jump_instruction(const char* name, int sign, Chunk* chunk, size_t offset){
    int jump = chunk->code[offset+1] |
               chunk->code[offset+2] << 8 |
               chunk->code[offset+3] << 16;
    printf("%-16s %4d -> %d\n", name, offset, offset + 1 + sign * jump);
    return offset+4;
}

static size_t closure_instruction(const char* name, Chunk* chunk, size_t constant, size_t offset){
    printf("%-16s %4d ", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("\n");
    ObjFunction* fn = AS_FUNCTION(chunk->constants.values[constant]);
    // printf("\tsizeof upvalues: %d \n", fn->upvalue_count);
    for (size_t i = 0; i < fn->upvalue_count; i++){
        int is_local = chunk->code[offset++];
        size_t index = chunk->code[offset] |
                       chunk->code[offset + 1] << 8 |
                       chunk->code[offset + 2] << 16;
        offset += 3;
        printf("%04d    |                     %s %d\n", 
                offset, is_local ? "local" : "upvalue", index);
    } 
    return offset;
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
        case OP_RETURN:                     return simple_instruction("OP_RETURN", offset);
        case OP_CONSTANT:                   return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:              return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_NEGATE:                     return simple_instruction("OP_NEGATE", offset);
        case OP_NOT:                        return simple_instruction("OP_NOT", offset);
        case OP_NIL:                        return simple_instruction("OP_NIL", offset);
        case OP_TRUE:                       return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:                      return simple_instruction("OP_FALSE", offset);
        case OP_ADD:                        return simple_instruction("OP_ADD", offset);
        case OP_SUB:                        return simple_instruction("OP_SUB", offset);
        case OP_MUL:                        return simple_instruction("OP_MUL", offset);
        case OP_DIV:                        return simple_instruction("OP_DIV", offset);
        case OP_MOD:                        return simple_instruction("OP_MOD", offset);
        case OP_NOT_EQUAL:                  return simple_instruction("OP_NOT_EQUAL", offset);         
        case OP_EQUAL:                      return simple_instruction("OP_EQUAL", offset);      
        case OP_LESS:                       return simple_instruction("OP_LESS", offset);      
        case OP_LESS_EQUAL:                 return simple_instruction("OP_LESS_EQUAL", offset);      
        case OP_GREATER:                    return simple_instruction("OP_GREATER", offset);      
        case OP_GREATER_EQUAL:              return simple_instruction("OP_GREATER_EQUAL", offset);  
         
        case OP_PRINT:                      return simple_instruction("OP_PRINT", offset);
        case OP_POP:                        return simple_instruction("OP_POP", offset); 
        case OP_POPN:                       return long_instruction("OP_POPN", chunk, offset);
        case OP_DEFINE_GLOBAL:              return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset); 
        case OP_DEFINE_GLOBAL_LONG:         return long_instruction("OP_DEFINE_GLOBAL_LONG", chunk, offset); 
        case OP_GET_GLOBAL:                 return constant_instruction("OP_GET_GLOBAL", chunk, offset); 
        case OP_GET_GLOBAL_LONG:            return long_instruction("OP_GET_GLOBAL_LONG", chunk, offset);     
        case OP_SET_GLOBAL:                 return constant_instruction("OP_SET_GLOBAL", chunk, offset); 
        case OP_SET_GLOBAL_LONG:            return long_instruction("OP_SET_GLOBAL_LONG", chunk, offset);
        case OP_GET_LOCAL:                  return long_instruction("OP_GET_LOCAL", chunk, offset); 
        case OP_SET_LOCAL:                  return long_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_UPVALUE:                return long_instruction("OP_GET_UPVALUE", chunk, offset); 
        case OP_SET_UPVALUE:                return long_instruction("OP_SET_UPVALUE", chunk, offset);
        case OP_ARRAY:                      return constant_instruction("OP_ARRAY", chunk, offset); 
        case OP_ARRAY_LONG:                 return long_instruction("OP_ARRAY_LONG", chunk, offset);
        case OP_GET_INDEX:                  return simple_instruction("OP_GET_INDEX", offset);
        case OP_SET_INDEX:                  return simple_instruction("OP_SET_INDEX", offset);
        case OP_JUMP_IF_FALSE:              return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP:                       return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_LOOP:                       return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:                       return constant_instruction("OP_CALL", chunk, offset);
        case OP_CLOSE_UPVALUE:              return simple_instruction("OP_CLOSE_UPVALUE", offset);
        case OP_CLOSURE:{
            offset++;
            uint8_t constant = chunk->code[offset++];
            return closure_instruction("OP_CLOSURE", chunk, constant, offset);
        }
        case OP_CLOSURE_LONG:{
            offset++;
            int32_t constant = chunk->code[offset] | 
                               chunk->code[offset+1] << 8 | 
                               chunk->code[offset+2] << 16;
            offset+=3;
            return closure_instruction("OP_CLOSURE_LONG", chunk, constant, offset);
        }
        case OP_CLASS:                      return constant_instruction("OP_CLASS", chunk, offset);
        default:
            printf("Unknown opcode %d\n", ins);
            return offset+1;
    }
}
