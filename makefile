#=============================================
# Configuration and target swapping
#=============================================

# Default to debug if no configuration 
CONFIG ?= debug

# Directory configuration
SRC_DIR   := src
BUILD_DIR := build/$(CONFIG)
MOD_DIR   := $(BUILD_DIR)/modules
OBJ_DIR   := $(BUILD_DIR)/obj
DOC_DIR   := docs
COMPDB_DIR := $(BUILD_DIR)/compdb

# Cross-platform utilities
RM := rm -rf
MKDIR := mkdir -p

# Binary outputs
TARGET := build/aoc_worker_$(CONFIG)
DOXYFILE := docs/Doxyfile
COMPDB_FILE := compile_commands.json

#=============================================
# Toolchain & hardware-aware compilation flags
#=============================================

CXX := clang++
BASE_FLAGS := -std=c++23 -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic

ifeq ($(CONFIG), release)
	# High optimization release
	CXXFLAGS := $(BASE_FLAGS) -O3 -march=native -DNDEBUG
else ifeq ($(CONFIG), instrument)
	# Sanitizer & Instrumentation matrix (bounds, memory, UB)
	# Note: Banned raw new/delete means ASan tracks your OS file reads and Arena bounds
	CXXFLAGS := $(BASE_FLAGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-position -DBUILD_INSTRUMENT
else
	# Default Debug Target
	CONFIG   := debug
	CXXFLAGS := $(BASE_FLAGS) -O0 -g -DBUILD_DEBUG
endif

# Clang module configuration flags
MOD_FLAGS := -fprebuilt-module-path=$(MOD_DIR)
MJFLAGS   = $(if $(GENERATE_COMPDB),-MJ $(COMPDB_DIR)/$(subst /,_,$(patsubst $(OBJ_DIR)/%,%,$@)).json)

# =====================================================
# Dynamic Wildcard Ingestion & Automated File Layout
# =====================================================

# Ingest base engineering files from src/base/
BASE_SRCS   := $(wildcard $(SRC_DIR)/base/core_*.cppm)

# Ingest nested puzzle paths: matches any year directory (y2015-y2025) and day files
PUZZLE_SRCS := $(wildcard $(SRC_DIR)/puzzles/y*/day*.cppm)
MAIN_SRCS   := $(wildcard $(SRC_DIR)/main.cpp)

# Map discovered source paths directly into isolated build object targets
BASE_OBJS   := $(patsubst $(SRC_DIR)/base/%.cppm,$(OBJ_DIR)/base/%.o,$(BASE_SRCS))
PUZZLE_OBJS := $(foreach src,$(PUZZLE_SRCS),$(patsubst $(SRC_DIR)/puzzles/%,$(OBJ_DIR)/puzzles/%,$(patsubst %.cppm,%.o,$(src))))
MAIN_OBJ    := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(MAIN_SRCS))

ALL_OBJS    := $(BASE_OBJS) $(PUZZLE_OBJS) $(MAIN_OBJ)
MODULE_SRCS := $(BASE_SRCS) $(PUZZLE_SRCS)

# Dynamic Module File Flag Generator:
# Extracts module names from filenames and points Clang to their active .pcm caches
LINK_MOD_FLAGS := $(foreach src,$(BASE_SRCS),-fmodule-file=$(basename $(notdir $(src)))=$(MOD_DIR)/$(basename $(notdir $(src))).pcm) \
                  $(foreach src,$(PUZZLE_SRCS),-fmodule-file=$(basename $(notdir $(src)))=$(MOD_DIR)/$(basename $(notdir $(src))).pcm)

# Automatic module dependency extraction from `import <module>;` statements.
# Maps imported module names to local module object targets, so adding modules
# normally requires no makefile edits.
module_obj_from_name = $(patsubst $(SRC_DIR)/%.cppm,$(OBJ_DIR)/%.o,$(firstword $(filter %/$(1).cppm,$(MODULE_SRCS))))
imports_of_src = $(shell sed -n 's/^[[:space:]]*import[[:space:]]\([A-Za-z0-9_]*\)[[:space:]]*;.*/\1/p' $(1))
module_import_obj_deps = $(strip $(foreach m,$(call imports_of_src,$(1)),$(call module_obj_from_name,$(m))))

$(foreach src,$(MODULE_SRCS),$(eval $(patsubst $(SRC_DIR)/%.cppm,$(OBJ_DIR)/%.o,$(src)): $(call module_import_obj_deps,$(src))))

#====================================
# Phase execution and meta rules 
#====================================

.PHONY: all clean docs clean_all bear

all: $(TARGET)

# The compilation database target: rebuilds with Clang -MJ fragments so
# module interface units are recorded correctly in compile_commands.json.
bear:
	@$(RM) $(COMPDB_FILE)
	@$(RM) $(BUILD_DIR)
	@$(MKDIR) $(COMPDB_DIR)
	@$(MAKE) CONFIG=$(CONFIG) GENERATE_COMPDB=1 all
	@{ printf '[\n'; find $(COMPDB_DIR) -type f -name '*.json' | sort | xargs -r cat; } | sed '$$s/,$$//' > $(COMPDB_FILE)
	@printf '\n]\n' >> $(COMPDB_FILE)
	
# final link
$(TARGET): $(ALL_OBJS)
	@$(MKDIR) build
	$(CXX) $(CXXFLAGS) $(ALL_OBJS) -o $(TARGET)

# rule A: Automatically compile core utilities inside src/base
$(OBJ_DIR)/base/core_%.o: $(SRC_DIR)/base/core_%.cppm
	@$(MKDIR) $(MOD_DIR) $(OBJ_DIR)/base $(if $(GENERATE_COMPDB),$(COMPDB_DIR))
	$(CXX) $(CXXFLAGS) $(MOD_FLAGS) $(LINK_MOD_FLAGS) $(MJFLAGS) -fmodule-output=$(MOD_DIR)/core_$*.pcm -c $< -o $@

# rule B: Automatically compile multi-year nested Puzzles
# Matches objects like build/debug/obj/puzzles/y2020/day01.o
$(OBJ_DIR)/puzzles/y%/day%.o: $(SRC_DIR)/puzzles/y%/day%.cppm $(BASE_OBJS)
	@$(MKDIR) $(MOD_DIR) $(OBJ_DIR)/puzzles/y$* $(if $(GENERATE_COMPDB),$(COMPDB_DIR))
	$(CXX) $(CXXFLAGS) $(MOD_FLAGS) $(LINK_MOD_FLAGS) $(MJFLAGS) -fmodule-output=$(MOD_DIR)/day$*.pcm -c $< -o $@

# rule C: The Application Entry Point Execution Block
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.cpp $(BASE_OBJS) $(PUZZLE_OBJS)
	@$(MKDIR) $(OBJ_DIR) $(if $(GENERATE_COMPDB),$(COMPDB_DIR))
	$(CXX) $(CXXFLAGS) $(LINK_MOD_FLAGS) $(MJFLAGS) -c $< -o $@

# ===============================
# Tooling & Cleanup Operations
# ===============================
docs:
	@$(MKDIR) $(DOC_DIR)
	doxygen $(DOXYFILE)

clean:
	$(RM) build/$(CONFIG)
	$(RM) $(TARGET)

clean_all:
	$(RM) build
	$(RM) $(COMPDB_FILE)