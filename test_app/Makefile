#Compiler, compiler flags and linker flags
CXX = g++
CXXFLAGS = -Wall -Werror -Wextra -std=c++17 -O3 -g -I . -I ..
LDFLAGS =  -fsanitize=address -L../
LBLIBS = -lmtcmaster

# Version ids

NAME := mtcmaster_test_app
SRC := $(wildcard *.cpp)
INC := $(wildcard *.h)
OBJ := $(SRC:.cpp=.o)

.PHONY: clean

all: app

app: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LBLIBS) $(LDFLAGS)

clean:
	rm -rf $(OBJ) $(NAME)
