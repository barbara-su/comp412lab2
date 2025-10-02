CC     = gcc
CFLAGS = -O3 -Wall -Wextra

# Source and object files
SRC = main.c scanner.c parser.c ir.c
OBJ = $(SRC:.c=.o)

# Targets
TARGET_FE     = 412fe
TARGET_ALLOC  = 412alloc

all: $(TARGET_FE) $(TARGET_ALLOC)

$(TARGET_FE): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_ALLOC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

format:
	clang-format -i --style=file *.c *.h

clean:
	rm -f *.o $(TARGET_FE) $(TARGET_ALLOC) *~ core.*