# Minimal C++ probe of the TGE C API (Fase 4 ergonomics sonda).
# Builds only a header-only `tge::` wrapper + one example; links the existing
# C libraries (libtge.a + libtge-extra.a). It does NOT depend on, nor feed,
# any other language binding.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Iinclude -I../../include -I../..
ROOT     ?= ../..
LIBTGE   := $(ROOT)/libtge.a
LIBEXTRA := $(ROOT)/libtge-extra.a

# Extra include dirs for the headless test: the mock backend (ROOT/tests) and
# the internal runtime header (ROOT/src), plus the example dir for snake_game.hpp.
TEST_INC := -I$(ROOT)/tests -I$(ROOT)/src -Iexamples
TEST     := tests/test_snake

EXAMPLE := examples/snake

.PHONY: all clean test
all: $(EXAMPLE)

$(EXAMPLE): examples/snake.cpp examples/snake_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

test: $(TEST)
	./$(TEST)

$(TEST): tests/test_snake.cpp examples/snake_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(LIBTGE) $(LIBEXTRA):
	$(MAKE) -C $(ROOT) $(notdir $@)

clean:
	rm -f $(EXAMPLE) $(TEST)
