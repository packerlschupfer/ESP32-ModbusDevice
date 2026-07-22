# Makefile for ModbusDevice unit tests
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -I. -I../src -DUNIT_TEST
LDFLAGS = 

# Test source files
TEST_SOURCES = test_main.cpp \
               test_modbus_device.cpp \
               test_result_pattern.cpp \
               test_simple_modbus_device.cpp \
               test_queued_modbus_device.cpp

# Object files
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)

# Test executable
TEST_EXEC = run_tests

# Default target
all: $(TEST_EXEC)

# Build test executable
$(TEST_EXEC): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile test files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Run tests
test: $(TEST_EXEC)
	./$(TEST_EXEC)

# Clean build artifacts
clean:
	rm -f $(TEST_OBJECTS) $(TEST_EXEC)

# Phony targets
.PHONY: all test clean