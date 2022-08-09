#include <stdio.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char** argv){
    (void)argc; (void)argv;

    VM vm;
    init_VM(&vm);

    Chunk chunk;
    init_chunk(&chunk);
    
    write_chunk(&chunk, OP_CONSTANT_LONG, 2);
    write_constant(&chunk, 1, 2);
    write_chunk(&chunk, OP_CONSTANT_LONG, 2);
    write_constant(&chunk, 2, 2);
    write_chunk(&chunk, OP_CONSTANT_LONG, 2);
    write_constant(&chunk, 3, 2);
    
    write_chunk(&chunk, OP_MUL, 2);

    write_chunk(&chunk, OP_CONSTANT_LONG, 2);
    write_constant(&chunk, 4, 2);
    write_chunk(&chunk, OP_CONSTANT_LONG, 2);
    write_constant(&chunk, 5, 2);
    write_chunk(&chunk, OP_NEGATE, 2);
    write_chunk(&chunk, OP_DIV, 2);
    write_chunk(&chunk, OP_SUB, 2);
    write_chunk(&chunk, OP_ADD, 2);

    write_chunk(&chunk, OP_RETURN, 3);
    
    disassemble_chunk(&chunk, "test chunk");
    interpret(&vm, &chunk);

    free_VM(&vm);
    free_chunk(&chunk);

    return 0;
}