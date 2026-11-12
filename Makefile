CXX      := g++
CXXFLAGS := -std=c++23
TARGET   := main
SRCS     := main.cpp Demos.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(wildcard *.h containers/*.h)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
