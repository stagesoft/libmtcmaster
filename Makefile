CXXFLAGS := -std=c++11 -fPIC -O3 -g -Wall -Werror -I /usr/include/rtmidi 
CCFLAGS := -std=c++11 -fPIC -O3 -g -Wall -Werror 
LDFLAGS :=
CC := gcc
LIBS = -L$(LIB) 

LIB = ../lib
MAJOR := 0
MINOR := 1
NAME := mtcmaster
VERSION := $(MAJOR).$(MINOR)

SOURCES = $(shell echo *.cpp)
HEADERS = $(shell echo *.h)
OBJECTS=$(SOURCES:.cpp=.o)



lib: lib$(NAME).so


test: $(NAME)_test
	LD_LIBRARY_PATH=. ./$(NAME)_test

$(NAME)_test: lib$(NAME).so
	$(CC) $(NAME)_test.cpp -o $@ -L. -l$(NAME)

lib$(NAME).so: lib$(NAME).so.$(VERSION)
	ln -s lib$(NAME).so.$(MAJOR) lib$(NAME).so

lib$(NAME).so.$(VERSION): $(OBJECTS)
	$(CC) -shared -Wl,-soname,lib$(NAME).so.$(MAJOR) $(OBJECTS) -o $@ -lpthread -lrtmidi

clean:
	$(RM) $(NAME)_test *.o *.so*
