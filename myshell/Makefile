# MyShell Makefile
CXX ?= g++
CXXFLAGS := -std=gnu++17 -Wall -Wextra -O2 -pthread
LDFLAGS := -pthread
INCLUDES := -Iinclude

# Enable readline if READLINE=1
ifeq ($(READLINE),1)
    CXXFLAGS += -DHAVE_READLINE
    LDFLAGS  += -lreadline
endif

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
BIN := myshell

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

run: $(BIN)
	./$(BIN)

.PHONY: all clean run
