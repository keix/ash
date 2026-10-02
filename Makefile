CC     ?= cc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -pedantic

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

all: ash

ash: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

build/%.o: src/%.c src/ash.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

FORTH_TESTS := tests/stack.fs tests/arithmetic.fs tests/numeric.fs \
               tests/exceptions.fs tests/input.fs \
               tests/memory.fs tests/control.fs tests/compiler.fs

TESTOBJ := $(filter-out build/main.o,$(OBJ))

test: test-c test-forth test-jit test-session

test-jit: ash
	@out=$$(./ash tests/jit-on.fs tests/tester.fs $(FORTH_TESTS) tests/summary.fs 2>&1); \
	echo "$$out" | tail -1; \
	echo "$$out" | grep -q "all forth tests passed" && echo "jit mode passed"

test-c: all
	@fail=0; \
	for t in tests/c/*.c; do \
	  bin=build/$$(basename $${t%.c}); \
	  $(CC) $(CFLAGS) -Isrc -o $$bin $$t $(TESTOBJ) && $$bin || fail=1; \
	done; \
	if [ $$fail = 0 ]; then echo "c tests passed"; else exit 1; fi

test-forth: ash
	@out=$$(./ash tests/tester.fs $(FORTH_TESTS) tests/summary.fs 2>&1); \
	echo "$$out"; \
	echo "$$out" | grep -q "all forth tests passed"

test-session: ash
	@fail=0; \
	for t in tests/session/*.fs; do \
	  ./ash < $$t 2>&1 | diff -u $${t%.fs}.expected - || fail=1; \
	done; \
	if [ $$fail = 0 ]; then echo "session tests passed"; else exit 1; fi

test-gforth:
	gforth tests/tester.fs $(FORTH_TESTS) tests/summary.fs -e bye

ANS_SUITE ?= ../forth-standard-test-suite/src

test-ans: ash
	@out=$$(./ash $(ANS_SUITE)/prelimtest.fth 2>&1); \
	echo "$$out" | grep -q "^0 tests failed" || { echo "$$out"; exit 1; }; \
	echo "ans preliminary tests passed"; \
	out=$$(./ash $(ANS_SUITE)/tester.fr $(ANS_SUITE)/core.fr $(ANS_SUITE)/coreplustest.fth < /dev/null 2>&1); \
	if echo "$$out" | grep -E "INCORRECT|WRONG NUMBER|undefined word"; then exit 1; fi; \
	echo "ans core and coreplus tests passed"

format:
	clang-format -i src/*.c src/*.h tests/c/*.c

clean:
	rm -rf build ash

.PHONY: all test test-c test-forth test-jit test-session test-gforth test-ans format clean
