CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra
TARGET   := main
SRCS     := main.cpp Demos.cpp containers/visual/Tui.cpp

.PHONY: all run test clean doxygen

all: $(TARGET)

$(TARGET): $(SRCS) $(shell find . -name "*.h")
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: all
	./$(TARGET)

test: all
	./$(TARGET)

doxygen:
	mkdir -p build/doxygen
	doxygen Doxyfile

clean:
	rm -f $(TARGET)
