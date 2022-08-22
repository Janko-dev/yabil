CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
LIBS = -lm
SRC = src/
CORE = $(SRC)core/
COMMON = $(SRC)common/
TEST = $(SRC)test/

INPUT_CORE = $(CORE)compiler.c $(CORE)lexer.c $(CORE)vm.c $(CORE)memory.c $(CORE)chunk.c
INPUT_COMMON = $(COMMON)table.c $(COMMON)object.c $(COMMON)value.c $(COMMON)debug.c
IN = $(INPUT_COMMON) $(INPUT_CORE) $(SRC)main.c
OUT = yabil

TESTER = $(TEST)main.c

make: $(IN)
	$(CC) $(IN) -o $(OUT) $(CFLAGS) $(LIBS)

run_test: $(OUT) $(TESTER)
	$(CC) $(TESTER) -o tester $(CFLAGS)
	./tester