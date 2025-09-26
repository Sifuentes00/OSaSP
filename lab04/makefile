CC = gcc
CFLAGS = -O2 -std=c11 -pedantic -Wall -Wextra -Werror -D_DEFAULT_SOURCE

DEBUG = build/debug
RELEASE = build/release
OUT_DIR = $(DEBUG)

vpath %.c src
vpath %.h src

ifeq ($(MODE), release)
  CFLAGS = -O2 -std=c11 -pedantic -Wall -Wextra -Werror -D_DEFAULT_SOURCE
  OUT_DIR = $(RELEASE)
endif

SOURCE_BASES = app consumer_functions system_management shared_buffer_types producer_functions
OBJECTS = $(patsubst %,$(OUT_DIR)/%.o,$(SOURCE_BASES))

EXEC = $(OUT_DIR)/ipc

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ -lrt

$(OUT_DIR)/%.o : %.c $(OUT_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

.PHONY: run
run: $(EXEC)
	./$(EXEC)

.PHONY: clean
clean:
	rm -rf $(DEBUG) $(RELEASE)
