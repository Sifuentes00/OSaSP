# Makefile

# Tools
CC      := gcc
CD      := gdb
CT      := valgrind

# Standards & flags
C_STANDARD      := c2x
C_EXTRA_FLAGS   := -Wno-unused
C_COMMON_FLAGS  := -std=$(C_STANDARD) -pedantic -W -Wall -Wextra $(C_EXTRA_FLAGS)
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
debug: CFLAGS:=$(C_DEBUG_FLAGS)
debug: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/generation.c -o $(BUILD_DIR)/generation
	$(CC) $(CFLAGS) $(SRC_DIR)/editor.c     -o $(BUILD_DIR)/editor

# Build release versions
release: CFLAGS:=$(C_RELEASE_FLAGS)
release: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/generation.c -o $(BUILD_DIR)/generation
	$(CC) $(CFLAGS) $(SRC_DIR)/editor.c     -o $(BUILD_DIR)/editor

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run the editor on sample data (after debug build)
run: debug
	@echo "Running generation and editor..."
	$(BUILD_DIR)/generation data.bin 20
	$(BUILD_DIR)/editor data.bin

# Start editor under debugger
debug-run: debug
	$(CD) $(BUILD_DIR)/editor

# Memory check with Valgrind (release build)
test: release
	@echo "Generating test data..."
	@$(BUILD_DIR)/generation test_data.bin 10
	$(CT) --leak-check=full --show-leak-kinds=all --track-origins=yes $(BUILD_DIR)/editor test_data.bin
	@rm -f test_data.bin

# Clean
clean:
	rm -rf $(BUILD_DIR)

