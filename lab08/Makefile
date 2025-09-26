# Makefile

# Tools
CC      := gcc
CD      := gdb
CT      := valgrind

# Standards & flags
C_STANDARD      := c2x
C_EXTRA_FLAGS   := -Wno-unused
C_COMMON_FLAGS  := -std=$(C_STANDARD) -pedantic -W -Wall -Wextra $(C_EXTRA_FLAGS) -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
C_DEBUG_FLAGS   := $(C_COMMON_FLAGS) -O1 -g -ggdb
C_RELEASE_FLAGS := $(C_COMMON_FLAGS) -Werror -O2

# Directories
BUILD_DIR := build
SRC_DIR   := src

# Phony targets
.PHONY: all debug release run debug-run test clean

# Default: build both debug and release
all: debug release

# Build debug versions
debug: CFLAGS := $(C_DEBUG_FLAGS)
debug: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/server.c -lpthread -o $(BUILD_DIR)/myserver
	$(CC) $(CFLAGS) $(SRC_DIR)/client.c       -o $(BUILD_DIR)/myclient

# Build release versions
release: CFLAGS := $(C_RELEASE_FLAGS)
release: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/server.c -lpthread -o $(BUILD_DIR)/myserver
	$(CC) $(CFLAGS) $(SRC_DIR)/client.c       -o $(BUILD_DIR)/myclient

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run server and client (after debug build)
run: debug
	@echo "Running server and client..."
	$(BUILD_DIR)/myserver ./root_dir 12345 & \
	sleep 1; \
	$(BUILD_DIR)/myclient 127.0.0.1 12345

# Start server under debugger
debug-run: debug
	$(CD) $(BUILD_DIR)/myserver

# Memory check with Valgrind (release build)
test: release
	@echo "Testing server under Valgrind..."
	$(CT) --leak-check=full --show-leak-kinds=all --track-origins=yes $(BUILD_DIR)/myserver ./root_dir 12345

# Clean
clean:
	rm -rf $(BUILD_DIR)

