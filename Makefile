CC     ?= cc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

all: ash

ash: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

build/%.o: src/%.c src/ash.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

format:
	clang-format -i src/*.c src/*.h

clean:
	rm -rf build ash

.PHONY: all format clean
