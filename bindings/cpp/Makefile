# Minimal C++ probe of the TGE C API (Fase 4 ergonomics sonda).
# Builds only a header-only `tge::` wrapper + one example; links the existing
# C libraries (libtge.a + libtge-extra.a). It does NOT depend on, nor feed,
# any other language binding.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Iinclude -I../../include -I../..
ROOT     ?= ../..
LIBTGE   := $(ROOT)/libtge.a
LIBEXTRA := $(ROOT)/libtge-extra.a

EXAMPLE := examples/snake

.PHONY: all clean
all: $(EXAMPLE)

$(EXAMPLE): examples/snake.cpp $(LIBTGE) $(LIBEXTRA)
	$(CXX) $(CXXFLAGS) $< -L$(ROOT) -ltge-extra -ltge -lm -o $@

$(LIBTGE) $(LIBEXTRA):
	$(MAKE) -C $(ROOT) $(notdir $@)

clean:
	rm -f $(EXAMPLE)
