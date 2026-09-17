CC     ?= cc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

all: $(OBJ)

%.o: %.c src/ash.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) ash

.PHONY: all clean
