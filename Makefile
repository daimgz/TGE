CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -O0
INCLUDES = -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = libtge.a

.PHONY: all clean test examples games bench fuzz valgrind check_no_malloc

all: $(TARGET)

$(TARGET): $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c include/tge/tge.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

TEST_BINS = $(patsubst %.c,%,$(wildcard tests/test_*.c))

test: $(TARGET) $(TEST_BINS) tests/check_no_malloc
	./tests/check_no_malloc
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

tests/check_no_malloc: tests/check_no_malloc.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc $< -L. -ltge -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -o $@

check_no_malloc: tests/check_no_malloc
	./tests/check_no_malloc

examples: $(TARGET) examples/min/00_runtime_only examples/min/01_draw_text examples/min/02_input_keys examples/min/03_timer examples/min/04_colors examples/min/05_resize examples/min/06_mouse

examples/min/%: examples/min/%.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ltge -lm -o $@

games: $(TARGET) examples/games/01_snake

examples/games/01_snake: examples/games/01_snake.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) examples/games/01_snake.c -L. -ltge -lm -o $@

bench: $(TARGET) benchmarks/bench_renderer benchmarks/bench_canvas_fill benchmarks/bench_draw_line
	./benchmarks/bench_renderer
	./benchmarks/bench_canvas_fill
	./benchmarks/bench_draw_line

benchmarks/%: benchmarks/%.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc $< -L. -ltge -lm -o $@

fuzz: $(TARGET) fuzz/fuzz_parser
	./fuzz/fuzz_parser

fuzz/fuzz_parser: fuzz/fuzz_parser.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc fuzz/fuzz_parser.c -L. -ltge -o $@

valgrind: test
	@for t in $(TEST_BINS); do \
		printf "  CHECK $$t\n"; \
		valgrind --error-exitcode=1 --leak-check=full --quiet ./$$t >/dev/null || exit 1; \
	done
	@echo "valgrind: no memory errors or leaks"
	@echo "  (requires glibc debug symbols, e.g. libc6-dbg on Debian/Ubuntu)"

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f $(TEST_BINS)
	rm -f tests/check_no_malloc
	rm -f fuzz/fuzz_parser
	rm -f benchmarks/bench_renderer benchmarks/bench_canvas_fill benchmarks/bench_draw_line
	rm -f examples/min/00_runtime_only examples/min/01_draw_text examples/min/02_input_keys examples/min/03_timer examples/min/04_colors examples/min/05_resize examples/min/06_mouse
	rm -f examples/games/01_snake
