UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
CXX      := /opt/homebrew/opt/llvm/bin/clang++
LDFLAGS  := -L/opt/homebrew/opt/llvm/lib -L/opt/homebrew/opt/gsl/lib -Wl,-rpath,/opt/homebrew/opt/llvm/lib -lstdc++ -lm -lgsl
else
CXX      := g+ # linux
LDFLAGS := -L/usr/local/lib/ -lstdc++ -lm -lgsl # linux
endif

ASAN_OPTIONS :== detect_leaks=1
#CXXFLAGS := -Wall -Wextra -Ofast -fsanitize=address -g -fopenmp #-ffast-math -march=native -funroll-loops 
#CXXFLAGS := -Wall -Wextra -Ofast -march=native // no openmp
CXXFLAGS := -Wall -Wextra -Ofast -fopenmp  # -march=native

# uses random number generators from GNU Scientific Library - install using "brew install gsl"
BUILD    := .
OBJ_DIR  := $(BUILD)/tmp
APP_DIR  := $(BUILD)/bin
TARGET   := mcts
INCLUDE  := -Iinclude/ -I/usr/local/include/ -I/opt/homebrew/include/ -I/opt/homebrew/opt/gsl/include
SRC      := $(wildcard mcts/*.cpp) 

OBJECTS  := $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEPENDENCIES \
         := $(OBJECTS:.o=.d)

all: build $(APP_DIR)/$(TARGET)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -MMD -o $@

$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(APP_DIR)/$(TARGET) $^ $(LDFLAGS)

-include $(DEPENDENCIES)

.PHONY: all build clean debug release info

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all

release: CXXFLAGS += -O2
release: all

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*
