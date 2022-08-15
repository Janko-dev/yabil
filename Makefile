CC = gcc
CFLAGS = -Wall -Wextra -g -lm
SRC = src/
CORE = $(SRC)core/
COMMON = $(SRC)common/

INPUT_CORE = $(CORE)compiler.c $(CORE)lexer.c $(CORE)vm.c $(CORE)memory.c $(CORE)chunk.c
INPUT_COMMON = $(COMMON)table.c $(COMMON)object.c $(COMMON)value.c $(COMMON)debug.c
IN = $(INPUT_COMMON) $(INPUT_CORE) $(SRC)main.c
OUT = yabil

make: $(IN)
	$(CC) $(IN) -o $(OUT) $(CFLAGS)