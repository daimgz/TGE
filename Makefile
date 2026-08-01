CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -O0
INCLUDES = -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = libtge.a

.PHONY: all clean test examples games bench fuzz

all: $(TARGET)

$(TARGET): $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c include/tge/tge.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

TEST_BINS = $(patsubst %.c,%,$(wildcard tests/test_*.c))

test: $(TARGET) $(TEST_BINS)
	@ok=0; fail=0; \
	for t in $(TEST_BINS); do \
		printf "  RUN   $$t\n"; \
		./$$t; \
		r=$$?; \
		if [ $$r -eq 0 ]; then ok=$$((ok+1)); \
		else fail=$$((fail+1)); fi; \
	done; \
	printf "$$ok suites passed, $$fail suites failed\n"; \
	[ $$fail -eq 0 ]

tests/test_%: tests/test_%.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc $< -L. -ltge -lm -o $@

examples: $(TARGET) examples/min/00_runtime_only examples/min/01_draw_text

examples/min/00_runtime_only: examples/min/00_runtime_only.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) examples/min/00_runtime_only.c -L. -ltge -lm -o $@

examples/min/01_draw_text: examples/min/01_draw_text.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) examples/min/01_draw_text.c -L. -ltge -lm -o $@

games: $(TARGET) examples/games/01_snake

examples/games/01_snake: examples/games/01_snake.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) examples/games/01_snake.c -L. -ltge -lm -o $@

bench: $(TARGET) benchmarks/bench_renderer
	./benchmarks/bench_renderer

benchmarks/bench_renderer: benchmarks/bench_renderer.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) benchmarks/bench_renderer.c -L. -ltge -lm -o $@

fuzz: $(TARGET) fuzz/fuzz_parser
	./fuzz/fuzz_parser

fuzz/fuzz_parser: fuzz/fuzz_parser.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc fuzz/fuzz_parser.c -L. -ltge -o $@

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f $(TEST_BINS)
	rm -f fuzz/fuzz_parser
	rm -f benchmarks/bench_renderer
	rm -f examples/min/00_runtime_only examples/min/01_draw_text
	rm -f examples/games/01_snake
