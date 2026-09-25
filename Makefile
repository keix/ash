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

test: ash
	@fail=0; \
	for t in test/*.fs; do \
	  ./ash < $$t 2>&1 | diff -u $${t%.fs}.expected - || fail=1; \
	done; \
	if [ $$fail = 0 ]; then echo "all tests passed"; else exit 1; fi

format:
	clang-format -i src/*.c src/*.h

clean:
	rm -rf build ash

.PHONY: all test format clean
