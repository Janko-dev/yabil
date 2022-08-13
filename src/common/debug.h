#ifndef _DEBUG_H
#define _DEBUG_H

#include "../core/chunk.h"

void disassemble_chunk(Chunk* chunk, const char* name);
size_t disassemble_instruction(Chunk* chunk, size_t offset);

#endif //_DEBUG_H
