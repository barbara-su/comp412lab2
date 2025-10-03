CC     = gcc
CFLAGS = -O3 -Wall -Wextra

# Source and object files
SRC = main.c scanner.c parser.c ir.c
OBJ = $(SRC:.c=.o)

# Target executable
TARGET = 412alloc

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

format:
	clang-format -i --style=file *.c *.h

clean:
	rm -f *.o $(TARGET) *~ core.*
