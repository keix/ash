CC     ?= cc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

all: $(OBJ)

build/%.o: src/%.c src/ash.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build ash

.PHONY: all clean
