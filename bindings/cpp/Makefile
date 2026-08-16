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
TEST2    := tests/test_sokoban
TEST3    := tests/test_pacman
TEST4    := tests/test_minesweeper

EXAMPLE  := examples/snake
EXAMPLE2 := examples/sokoban
EXAMPLE3 := examples/pacman
EXAMPLE4 := examples/minesweeper
EXAMPLE5 := examples/tetris/tetris

TEST  := tests/test_snake
TEST2 := tests/test_sokoban
TEST3 := tests/test_pacman
TEST4 := tests/test_minesweeper
TEST5 := tests/test_tetris

.PHONY: all clean test
all: $(EXAMPLE) $(EXAMPLE2) $(EXAMPLE3) $(EXAMPLE4) $(EXAMPLE5)

$(EXAMPLE): examples/snake.cpp examples/snake_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(EXAMPLE2): examples/sokoban.cpp examples/sokoban_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(EXAMPLE3): examples/pacman.cpp examples/pacman_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(EXAMPLE4): examples/minesweeper.cpp examples/minesweeper_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(EXAMPLE5): examples/tetris/tetris.cpp examples/tetris/tetris_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

test: $(TEST) $(TEST2) $(TEST3) $(TEST4) $(TEST5)
	./$(TEST)
	./$(TEST2)
	./$(TEST3)
	./$(TEST4)
	./$(TEST5)

$(TEST): tests/test_snake.cpp examples/snake_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(TEST2): tests/test_sokoban.cpp examples/sokoban_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(TEST3): tests/test_pacman.cpp examples/pacman_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(TEST4): tests/test_minesweeper.cpp examples/minesweeper_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(TEST5): tests/test_tetris.cpp examples/tetris/tetris_game.hpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $(TEST_INC) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(LIBTGE) $(LIBEXTRA):
	$(MAKE) -C $(ROOT) $(notdir $@)

clean:
	rm -f $(EXAMPLE) $(EXAMPLE2) $(EXAMPLE3) $(EXAMPLE4) $(EXAMPLE5) \
	      $(TEST) $(TEST2) $(TEST3) $(TEST4) $(TEST5)
