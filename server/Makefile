CXX = g++
# Default: native = maximum performance (used when building locally or on Hetzner via Coolify)
# Docker on Hetzner CCX13: -march=native compiles for AMD EPYC = optimal
# Override from Dockerfile if building on unknown CPU: make MARCH=x86-64-v2
MARCH ?= native
CXXFLAGS = -std=c++17 -O3 -march=$(MARCH) -flto -DNDEBUG -I./src
# -O3: Aggressive optimization (30-50% faster than -O2)
# -march=native: Use CPU-specific instructions (SSE4, AVX, etc.)
# -flto: Link-time optimization (cross-file inlining)
# -DNDEBUG: Disable assert() overhead

LDFLAGS = -lpthread -flto

# Source directory
SRCDIR = src

TARGET = vrp_solver_custom
SRC = $(SRCDIR)/vrp_solver_custom.cpp

HEADERS = $(SRCDIR)/vrp_types.h $(SRCDIR)/vrp_utils.h $(SRCDIR)/vrp_parser.h $(SRCDIR)/vrp_constraints.h \
          $(SRCDIR)/vrp_construction.h $(SRCDIR)/vrp_single_trip_construction.h \
          $(SRCDIR)/vrp_savings_construction.h \
          $(SRCDIR)/vrp_validators.h $(SRCDIR)/vrp_local_search.h $(SRCDIR)/vrp_alns.h $(SRCDIR)/vrp_output.h

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)
	@echo "Built with -O3 -march=native -flto for maximum performance"

debug: $(SRC) $(HEADERS)
	$(CXX) -std=c++17 -g -O0 -I./src -o $(TARGET)_debug $(SRC) -lpthread

clean:
	rm -f $(TARGET) $(TARGET)_debug $(TARGET).exe $(TARGET)_debug.exe

.PHONY: all debug clean
