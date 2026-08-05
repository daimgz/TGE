CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -O0
INCLUDES = -Iinclude -I.
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = libtge.a
EXTRA_SRC = $(wildcard tge-extra/*.c)
EXTRA_OBJ = $(EXTRA_SRC:.c=.o)
EXTRA_TARGET = libtge-extra.a

.PHONY: all clean test check_headers examples games bench fuzz valgrind check_no_malloc

all: $(TARGET) $(EXTRA_TARGET)

$(TARGET): $(OBJ)
	ar rcs $@ $^

$(EXTRA_TARGET): $(EXTRA_OBJ)
	ar rcs $@ $^

src/%.o: src/%.c include/tge/tge.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

tge-extra/%.o: tge-extra/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

TEST_BINS = $(patsubst %.c,%,$(wildcard tests/test_*.c))

test: $(TARGET) $(EXTRA_TARGET) $(TEST_BINS) tests/check_no_malloc check_headers
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

tests/test_%: tests/test_%.c $(TARGET) $(EXTRA_TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc $< -L. -ltge-extra -ltge -lm -o $@

tests/check_no_malloc: tests/check_no_malloc.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -Isrc $< -L. -ltge -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -o $@

check_no_malloc: tests/check_no_malloc
	./tests/check_no_malloc

check_headers: $(TARGET)
	@for h in include/tge/*.h; do \
		probe=$$(mktemp /tmp/tge_hdr_XXX.c); \
		printf '#include <tge/%s>\nint main(void){return 0;}\n' "$$(basename $$h)" > $$probe; \
		printf "  HC $$h\n"; \
		$(CC) $(CFLAGS) $(INCLUDES) $$probe -o /dev/null || { rm -f $$probe; exit 1; }; \
		rm -f $$probe; \
	done
	@for h in tge-extra/*.h; do \
		probe=$$(mktemp /tmp/tge_xhdr_XXX.c); \
		printf '#include <tge-extra/%s>\nint main(void){return 0;}\n' "$$(basename $$h)" > $$probe; \
		printf "  HC $$h\n"; \
		$(CC) $(CFLAGS) $(INCLUDES) $$probe -o /dev/null || { rm -f $$probe; exit 1; }; \
		rm -f $$probe; \
	done

examples: $(TARGET) examples/min/00_runtime_only examples/min/01_draw_text examples/min/02_input_keys examples/min/03_timer examples/min/04_colors examples/min/05_resize examples/min/06_mouse examples/min/07_extra_demo examples/min/08_grid_canvas

examples/min/%: examples/min/%.c $(TARGET) $(EXTRA_TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ltge-extra -ltge -lm -o $@

games: $(TARGET) $(EXTRA_TARGET) examples/games/01_snake examples/games/02_pong examples/games/03_tetris examples/games/04_space_invaders examples/games/05_swarm examples/games/06_snake_grid examples/games/07_breakout

examples/games/%: examples/games/%.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ltge -lm -o $@

# 01_snake (tge-extra utils) and 05_swarm / 06_snake_grid / 07_breakout
# (tge-extra consumers) link the extra library; the rest link the core only.
examples/games/01_snake: examples/games/01_snake.c $(TARGET) $(EXTRA_TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ltge-extra -ltge -lm -o $@

examples/games/05_swarm: examples/games/05_swarm.c $(TARGET) $(EXTRA_TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ltge-extra -ltge -lm -o $@

examples/games/06_snake_grid: examples/games/06_snake_grid.c $(TARGET) $(EXTRA_TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ltge-extra -ltge -lm -o $@

examples/games/07_breakout: examples/games/07_breakout.c $(TARGET) $(EXTRA_TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ltge-extra -ltge -lm -o $@

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
	rm -f $(OBJ) $(TARGET) $(EXTRA_OBJ) $(EXTRA_TARGET)
	rm -f $(TEST_BINS)
	rm -f tests/check_no_malloc
	rm -f fuzz/fuzz_parser
	rm -f benchmarks/bench_renderer benchmarks/bench_canvas_fill benchmarks/bench_draw_line
	rm -f examples/min/00_runtime_only examples/min/01_draw_text examples/min/02_input_keys examples/min/03_timer examples/min/04_colors examples/min/05_resize examples/min/06_mouse examples/min/07_extra_demo examples/min/08_grid_canvas
	rm -f examples/games/01_snake examples/games/02_pong examples/games/03_tetris examples/games/04_space_invaders examples/games/05_swarm examples/games/06_snake_grid examples/games/07_breakout
