#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

void run_REPL(VM* vm){
    char line[1024];
    for(;;){
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)){
            printf("\n");
            break;
        }
        interpret(line);
    }
}

static char* read_file(const char* file_path){
    FILE* source_file = fopen(file_path, "rb");
    if (source_file == NULL){
        fprintf(stderr, "Could not open file [%s]\n", file_path);
        exit(74);
    }

    fseek(source_file, 0L, SEEK_END);
    size_t size = ftell(source_file);
    fseek(source_file, 0L, SEEK_SET);
    
    char* source = (char*)malloc(size + 1);
    if (source == NULL){
        fprintf(stderr, "Could not allocate buffer [%s]\n", file_path);
        exit(74);
    }
    size_t bytes_read = fread(source, sizeof(char), size, source_file);
    if(bytes_read < size){
        fprintf(stderr, "Could not read file [%s]\n", file_path);
        exit(74);
    }
    source[bytes_read] = '\0';

    fclose(source_file);
    return source;
}

void run_file(VM* vm, const char* file_path){
    char* source = read_file(file_path);
    InterpreterResult result = interpret(vm, source);
    free(source);
    if (result == INTERPRET_COMPILE_ERR) exit(65);
    if (result == INTERPRET_RUNTIME_ERR) exit(65);
}

int main(int argc, const char** argv){
    
    VM vm;
    init_VM(&vm);

    if (argc == 1) run_REPL(&vm);
    else if (argc == 2) run_file(&vm, argv[1]);
    else {
        fprintf(stderr, "Usage: yabil [path]\n");
        exit(64);
    }
    
    free_VM(&vm);
    return 0;
}