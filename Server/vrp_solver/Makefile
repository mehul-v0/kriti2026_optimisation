# VRP Solver Makefile
# Compiles the C++ VRP solver that replicates OR-Tools algorithms

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra

# For Windows with MinGW
ifeq ($(OS),Windows_NT)
    EXE = vrp_solver.exe
    RM = del /Q
else
    EXE = vrp_solver
    RM = rm -f
endif

# Source files
SRCS = vrp_solver.cpp
HEADERS = vrp_types.h vrp_utils.h vrp_construction.h vrp_local_search.h vrp_gls.h vrp_parser.h

# Build target
all: $(EXE)

$(EXE): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(EXE) $(SRCS)

# Debug build
debug: CXXFLAGS = -std=c++17 -g -Wall -Wextra -DDEBUG
debug: $(EXE)

# Clean
clean:
	$(RM) $(EXE)

# Test run
test: $(EXE)
	./$(EXE) ../cpp_input.txt output.json 30

.PHONY: all clean debug test
