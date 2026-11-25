CXX      := g++
CXXFLAGS := -std=c++23
TARGET   := main
SRCS     := main.cpp Demos.cpp
HEADERS  := $(wildcard *.h containers/*.h utilities/*.h)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

excel: excel.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) excel.cpp -o excel

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) excel
