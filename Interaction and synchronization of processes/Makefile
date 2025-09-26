CC = gcc
CFLAGS = -std=c11 -g2 -ggdb -pedantic -W -Wall -Wextra -Wno-unused-parameter

.SUFFIXES:   
.SUFFIXES: .c .o

DEBUG   = ./build/debug
RELEASE = ./build/release
OUT_DIR = $(DEBUG)
vpath %.c src
vpath %.h src
vpath %.o build/debug

ifeq ($(MODE), release)
  CFLAGS = -std=c11 -pedantic -W -Wall -Wextra -Wno-unused-parameter -Werror
  OUT_DIR = $(RELEASE)
  vpath %.o build/release
endif

parent_objects = $(OUT_DIR)/parent.o
child_objects = $(OUT_DIR)/child.o   

parent = $(OUT_DIR)/parent
child = $(OUT_DIR)/child

all: $(parent) $(child)

$(parent): $(parent_objects)
	$(CC) $(CFLAGS) $(parent_objects) -o $@

$(child): $(child_objects)
	$(CC) $(CFLAGS) $(child_objects) -o $@

$(OUT_DIR)/%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean 
clean:
	@rm -rf $(DEBUG)/* $(RELEASE)/* $(parent) $(child)

