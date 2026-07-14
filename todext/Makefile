CXX      	:= g++
CXXFLAGS	:= -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET   	:= build/todext
PREFIX      ?= $(HOME)/.local/bin

.PHONY: all clean install

all: $(TARGET)

$(TARGET): todext.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)

clean:
	rm -rf build
