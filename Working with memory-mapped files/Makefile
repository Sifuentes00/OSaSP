CC = gcc
CFLAGS = -W -Wall -D_POSIX_C_SOURCE=200809L -Wextra -std=c11 -pedantic -Wno-unused-parameter -Wno-unused-variable -Werror
LDFLAGS = -lpthread

DEBUG = build/debug
RELEASE = build/release
OUT_DIR = $(DEBUG)

# Находим все .c файлы в папке src
SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(OUT_DIR)/%.o,$(SOURCES))

# Определяем исполняемые файлы
GEN_BIN = $(OUT_DIR)/gen
VIEW_BIN = $(OUT_DIR)/view
SORT_BIN = $(OUT_DIR)/sort_index

ifeq ($(MODE), release)
  CFLAGS = -O2 -std=c11 -pedantic -Wall -Wextra -Werror -D_DEFAULT_SOURCE
  OUT_DIR = $(RELEASE)
else
  CFLAGS += -g
endif

# Добавляем vpath для поиска .c и .h файлов в src
vpath %.c src
vpath %.h src

all: $(GEN_BIN) $(VIEW_BIN) $(SORT_BIN)

$(GEN_BIN): $(OUT_DIR)/gen.o
	$(CC) $(CFLAGS) $^ -o $@

$(VIEW_BIN): $(OUT_DIR)/view.o
	$(CC) $(CFLAGS) $^ -o $@

$(SORT_BIN): $(OUT_DIR)/sort_index.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OUT_DIR)/%.o : src/%.c $(OUT_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

.PHONY: all clean run

run: all
	$(GEN_BIN) input.dat 1024
	$(SORT_BIN) 1024 4 2 input.dat
	$(VIEW_BIN) input.dat

clean:
	rm -rf $(DEBUG) $(RELEASE)

