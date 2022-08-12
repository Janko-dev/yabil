CC = gcc
CFLAGS = -Wall -Wextra -g 
SRC = src/
IN = $(SRC)table.c $(SRC)compiler.c $(SRC)lexer.c $(SRC)vm.c $(SRC)object.c $(SRC)value.c $(SRC)debug.c $(SRC)memory.c $(SRC)chunk.c $(SRC)main.c
OUT = yabil

make: $(IN)
	$(CC) $(IN) -o $(OUT) $(CFLAGS)