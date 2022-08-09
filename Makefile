CC = gcc
CFLAGS = -Wall -Wextra -g 
SRC = src/
IN = $(SRC)vm.c $(SRC)value.c $(SRC)debug.c $(SRC)memory.c $(SRC)chunk.c $(SRC)main.c
OUT = yabil

make: $(IN)
	$(CC) $(IN) -o $(OUT) $(CFLAGS)