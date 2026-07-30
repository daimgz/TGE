CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -O0
INCLUDES = -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = libtge.a

.PHONY: all clean test examples bench fuzz

all: $(TARGET)

$(TARGET): $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c include/tge/tge.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TARGET) tests/test_unit
	./tests/test_unit

tests/test_unit: tests/test_unit.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_unit.c -L. -ltge -o $@

examples: $(TARGET) examples/min/00_runtime_only examples/min/01_draw_text

examples/min/00_runtime_only: examples/min/00_runtime_only.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) examples/min/00_runtime_only.c -L. -ltge -o $@

examples/min/01_draw_text: examples/min/01_draw_text.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) examples/min/01_draw_text.c -L. -ltge -o $@

bench: $(TARGET) benchmarks/bench_renderer
	./benchmarks/bench_renderer

benchmarks/bench_renderer: benchmarks/bench_renderer.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) benchmarks/bench_renderer.c -L. -ltge -o $@

fuzz: $(TARGET) fuzz/fuzz_parser
	./fuzz/fuzz_parser

fuzz/fuzz_parser: fuzz/fuzz_parser.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) fuzz/fuzz_parser.c -L. -ltge -o $@

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f tests/test_unit
	rm -f fuzz/fuzz_parser
	rm -f benchmarks/bench_renderer
	rm -f examples/min/00_runtime_only examples/min/01_draw_text
