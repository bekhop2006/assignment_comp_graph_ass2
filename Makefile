CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Isrc
LDFLAGS  ?= -framework GLUT -framework OpenGL

COMMON = src/common.cpp src/common.h
BINS   = interaction cube

.PHONY: all clean run

all: $(BINS)

interaction: src/interaction.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o $@ src/interaction.cpp src/common.cpp $(LDFLAGS)

cube: src/cube.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o $@ src/cube.cpp src/common.cpp $(LDFLAGS)

clean:
	rm -f $(BINS) *.ppm

run: all
	./interaction
	./cube
