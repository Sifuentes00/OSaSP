CC = gcc
CFLAGS = -W -Wall -D_POSIX_C_SOURCE=200809L -Wextra -std=c11 -pedantic -Wno-unused -Werror

DEBUG = build/debug
RELEASE = build/release
OUT_DIR = $(DEBUG)

vpath %.c src
vpath %.h src

ifeq ($(MODE), release)
  CFLAGS = -O2 -std=c11 -pedantic -Wall -Wextra -Werror -D_DEFAULT_SOURCE
  OUT_DIR = $(RELEASE)
else
  CFLAGS += -g
endif

# Находим все .c файлы в папке src
SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(OUT_DIR)/%.o,$(SOURCES))

EXEC = $(OUT_DIR)/prod_cons

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ -lrt

$(OUT_DIR)/%.o : src/%.c $(OUT_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

.PHONY: run
run: $(EXEC)
	./$(EXEC)

.PHONY: clean
clean:
	rm -rf $(DEBUG) $(RELEASE)

