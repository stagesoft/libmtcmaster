#Compiler, compiler flags and linker flags
CXX = g++
CXXFLAGS = -Wall -Werror -Wextra -std=c++17 -O3 -fPIC -g -I /usr/include/rtmidi
LDFLAGS =  -fsanitize=address
LBLIBS = -lpthread -lrtmidi

# Version ids
MAJOR := 0
MINOR := 1
VERSION := $(MAJOR).$(MINOR)

NAME := mtcmaster
SRC := $(wildcard *.cpp)
INC := $(wildcard *.h)
OBJ := $(SRC:.cpp=.o)

.PHONY: clean

all: lib

lib: lib$(NAME).so

lib$(NAME).so: lib$(NAME).so.$(VERSION)
	ln -sf lib$(NAME).so.$(MAJOR).$(MINOR) lib$(NAME).so

lib$(NAME).so.$(VERSION): $(OBJ)
	$(CXX) -shared -Wl,-soname,lib$(NAME).so.$(MAJOR) $(OBJ) -o $@ $(LBLIBS)

clean:
	rm -rf $(OBJ)

install: lib$(NAME).so.$(VERSION)
	sudo cp lib$(NAME).so.$(VERSION) /usr/lib/
	sudo ln -sf /usr/lib/lib$(NAME).so.$(VERSION) /usr/lib/lib$(NAME).so
	sudo ln -sf /usr/lib/lib$(NAME).so.$(VERSION) /usr/lib/lib$(NAME).so.$(MAJOR)