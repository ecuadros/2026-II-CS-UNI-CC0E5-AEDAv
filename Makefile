CXX      := g++
CXXFLAGS := -std=c++23 -Icontainers
TARGET   := main
SRCS     := main.cpp Demos.cpp
HDRS 	 := containers/vector.h containers/GeneralIterator.h foreach.h types.h Demos.h

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
