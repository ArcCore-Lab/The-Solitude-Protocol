CC = gcc
CFLAGS = -Wall -Wextra -g -I./include
LDFLAGS = -lpthread

# Structure of directories
SRC_DIR = src
INCLUDE_DIR = include
LIB_DIR = lib
TEST_DIR = tests
OBJ_DIR = build/obj
BIN_DIR = build/bin
LIB_BUILD_DIR = build/lib

# Find all project subdirectories (each directory under src is considered a project)
PROJECTS = $(wildcard $(SRC_DIR)/*)
PROJECTS := $(filter-out $(SRC_DIR)/%,%,$(foreach p,$(PROJECTS),$(if $(wildcard $(p)/*.c),$(notdir $(p)))))

# Library files
LIB_SOURCES = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJECTS = $(patsubst $(LIB_DIR)/%.c,$(OBJ_DIR)/lib/%.o,$(LIB_SOURCES))
COMMON_LIB = $(LIB_BUILD_DIR)/libcommon.a

# Generate targets for each project
TARGETS = $(foreach proj,$(PROJECTS),$(BIN_DIR)/$(proj))

# Default target
all: $(COMMON_LIB) $(TARGETS)
    @echo "✓ All projects compiled, sir"

# Create necessary directories
$(OBJ_DIR) $(BIN_DIR) $(LIB_BUILD_DIR) $(OBJ_DIR)/lib:
    @mkdir -p $@

# Compile library files
$(OBJ_DIR)/lib/%.o: $(LIB_DIR)/%.c | $(OBJ_DIR)/lib
    @$(CC) $(CFLAGS) -c $< -o $@
    @echo "  ✓ Compiled library file: $(notdir $<)"

# Generate static library
$(COMMON_LIB): $(LIB_OBJECTS) | $(LIB_BUILD_DIR)
    @ar rcs $@ $^
    @echo "✓ Static library generated: $@"

# Generate compilation rules for each project
define PROJECT_TEMPLATE
$(BIN_DIR)/$(1): $$(OBJ_DIR)/$(1)/main.o $$(OBJ_DIR)/$(1)/%.o $$(COMMON_LIB) | $(BIN_DIR)
    @$$(CC) $$(CFLAGS) -o $$@ $$^ $$(LDFLAGS)
    @echo "✓ Project compiled: $(1)"
$(OBJ_DIR)/$(1)/%.o: $(SRC_DIR)/$(1)/%.c | $(OBJ_DIR)/$(1)
    @$$(CC) $$(CFLAGS) -c $$< -o $$@

$(OBJ_DIR)/$(1):
    @mkdir -p $$@
endef

# Apply compilation rules for all projects
$(foreach proj,$(PROJECTS),$(eval $(call PROJECT_TEMPLATE,$(proj))))

# Clean build files
clean:
    @rm -rf build
    @echo "✓ Build files cleaned, sir"

# List available projects
list:
    @echo "Available projects:"
    @$(foreach proj,$(PROJECTS),echo "  - $(proj)";)

# Compile a single project (usage: make project-Day01)
project-%: $(BIN_DIR)/%
    @echo "✓ Project $* compiled"

.PHONY: all clean list project-%