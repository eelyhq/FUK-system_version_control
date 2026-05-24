# ==========================================
# Compiler and Linker Configuration
# ==========================================
CC       := gcc
CFLAGS   := -Wall -Wextra -std=gnu99 -O2 -Iinclude
LDFLAGS  :=
LDLIBS   := -lz -lcrypto  # Links zlib (-lz) and crypto (-lcrypto) for hashing

# ==========================================
# Directories and Targets
# ==========================================
SRC_DIR  := src
OBJ_DIR  := build
TARGET   := fuk

# ==========================================
# File Discovery
# ==========================================
# Automatically finds all .c files in src/ and its subfolders
SRCS     := $(wildcard $(SRC_DIR)/*.c) \
            $(wildcard $(SRC_DIR)/commands/*.c) \
            $(wildcard $(SRC_DIR)/utils/*.c)

# Maps each .c file to a corresponding .o file in the build directory
# e.g., src/commands/commit.c -> build/commands/commit.o
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# ==========================================
# Build Rules
# ==========================================
.PHONY: all clean fclean re

# Default target
all: $(TARGET)

# Linking Rule: Combines object files into the final executable
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Build successful!"

# Compilation Rule: Converts .c files into .o files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up only compiled object files
clean:
	@echo "Cleaning object files..."
	rm -rf $(OBJ_DIR)

# Clean up object files AND the executable
fclean: clean
	@echo "Cleaning executable..."
	rm -f $(TARGET)

# Rebuild the entire project from scratch
re: fclean all