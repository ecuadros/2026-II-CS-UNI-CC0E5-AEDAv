CXX      := g++
CXXFLAGS := -std=c++23
TARGET   := main
SRCS     := main.cpp Demos.cpp
HDRS     := Demos.h foreach.h types.h containers/vector.h containers/GeneralIterator.h

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
