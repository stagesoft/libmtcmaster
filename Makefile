#Compiler, compiler flags and linker flags
CXX = g++
CXXFLAGS = -Wall -Werror -Wextra -std=c++17 -O3 -fPIC -g -I /usr/include/rtmidi
LDFLAGS =  -fsanitize=address
LBLIBS = -lpthread -lrtmidi

#PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(prefix),)
    prefix := /usr/local
endif

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
	install -d $(DESTDIR)$(prefix)/lib/
	install -m 644 lib$(NAME).so.$(VERSION) $(DESTDIR)$(prefix)/lib/
	ln -sf $(DESTDIR)$(prefix)/lib/lib$(NAME).so.$(VERSION) $(DESTDIR)$(prefix)/lib/lib$(NAME).so
	ln -sf $(DESTDIR)$(prefix)/lib/lib$(NAME).so.$(VERSION) $(DESTDIR)$(prefix)/lib/lib$(NAME).so.$(MAJOR)

