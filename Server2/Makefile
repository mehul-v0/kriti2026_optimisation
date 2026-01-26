CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -I.
LDFLAGS = -lm
TARGET = vrp_solver_custom
SOURCES = vrp_solver_custom.cpp
JSON_HEADER = json.hpp

all: $(JSON_HEADER) $(TARGET)

$(JSON_HEADER):
	@echo "Downloading nlohmann/json..."
	curl -o $(JSON_HEADER) https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Build complete: $(TARGET)"

clean:
	rm -f $(TARGET) *.json

distclean: clean
	rm -f $(JSON_HEADER)

run: $(TARGET)
	./$(TARGET) input.json

.PHONY: all clean distclean run
