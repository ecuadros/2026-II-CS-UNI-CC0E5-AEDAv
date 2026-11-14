CXX      := g++
CXXFLAGS := -std=c++23
TARGET   := main
SRCS     := main.cpp Demos.cpp

.PHONY: all run clean doxygen

all: $(TARGET)

$(TARGET): $(SRCS) $(wildcard *.h containers/*.h)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: all
	./$(TARGET)

doxygen:
	mkdir -p build/doxygen
	doxygen Doxyfile

clean:
	rm -f $(TARGET)
