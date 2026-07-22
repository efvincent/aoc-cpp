#=============================================
# Configuration and target swapping
#=============================================

.DEFAULT_GOAL := all

# Internal build mode selector (user-facing entry points are make targets).
MODE ?= debug

# Directory configuration
SRC_DIR   := src
BUILD_DIR := build/$(MODE)
MOD_DIR   := $(BUILD_DIR)/modules
OBJ_DIR   := $(BUILD_DIR)/obj
DOC_DIR   := docs
COMPDB_DIR := $(BUILD_DIR)/compdb

# Cross-platform utilities
RM := rm -rf
MKDIR := mkdir -p

# Binary outputs
TARGET := build/aoc_worker_$(MODE)
DOXYFILE := docs/Doxyfile
COMPDB_FILE := compile_commands.json

#=============================================
# Toolchain & hardware-aware compilation flags
#=============================================

CXX := clang++
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS := $(shell pkg-config --libs openssl 2>/dev/null)

BASE_FLAGS := -std=c++23 -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic $(OPENSSL_CFLAGS)

ifeq ($(MODE), release)
	# High optimization release
	CXXFLAGS := $(BASE_FLAGS) -O3 -march=native -DNDEBUG
else ifeq ($(MODE), lto)
	# High optimization release with full LTO
	CXXFLAGS := $(BASE_FLAGS) -O3 -march=native -DNDEBUG -flto=full
else ifeq ($(MODE), instrument)
	# Sanitizer & Instrumentation matrix (bounds, memory, UB)
	# Note: Banned raw new/delete means ASan tracks your OS file reads and Arena bounds
	CXXFLAGS := $(BASE_FLAGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DBUILD_INSTRUMENT
else
	# Default Debug Target
	MODE     := debug
	CXXFLAGS := $(BASE_FLAGS) -O0 -g -DBUILD_DEBUG
endif

# Clang module configuration flags
MOD_FLAGS := -fprebuilt-module-path=$(MOD_DIR)
MJFLAGS   = $(if $(GENERATE_COMPDB),-MJ $(COMPDB_DIR)/$(subst /,_,$(patsubst $(OBJ_DIR)/%,%,$@)).json)

# =====================================================
# Recursive source discovery for everything under src/
# =====================================================

CPPM_SRCS := $(shell find $(SRC_DIR) -type f -name '*.cppm' | sort)
CPP_SRCS  := $(shell find $(SRC_DIR) -type f -name '*.cpp'  | sort)

MODULE_SRCS := $(CPPM_SRCS)
MODULE_OBJS := $(patsubst $(SRC_DIR)/%.cppm,$(OBJ_DIR)/%.o,$(MODULE_SRCS))
CPP_OBJS    := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CPP_SRCS))

ALL_OBJS := $(MODULE_OBJS) $(CPP_OBJS)

# Point linker/consumers at every discovered prebuilt module.
LINK_MOD_FLAGS := $(foreach src,$(MODULE_SRCS),-fmodule-file=$(basename $(notdir $(src)))=$(MOD_DIR)/$(patsubst $(SRC_DIR)/%.cppm,%.pcm,$(src)))

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

.PHONY: all debug release lto instrument bench bench-debug bench-release bench-lto bench-instrument clean docs docs_pdf clean_all bear

N ?= 1000
ARGS ?= 2015 2

all: $(TARGET)

debug:
	@$(MAKE) MODE=debug all

release:
	@$(MAKE) MODE=release all

lto:
	@$(MAKE) MODE=lto all

instrument:
	@$(MAKE) MODE=instrument all

bench: $(MODE)
	@./bench_nanos.sh $(N) $(MODE) $(ARGS)

bench-debug: debug
	@./bench_nanos.sh $(N) debug $(ARGS)

bench-release: release
	@./bench_nanos.sh $(N) release $(ARGS)

bench-lto: lto
	@./bench_nanos.sh $(N) lto $(ARGS)

bench-instrument: instrument
	@./bench_nanos.sh $(N) instrument $(ARGS)

# The compilation database target: rebuilds with Clang -MJ fragments so
# module interface units are recorded correctly in compile_commands.json.
bear:
	@$(RM) $(COMPDB_FILE)
	@$(RM) $(BUILD_DIR)
	@$(MKDIR) $(COMPDB_DIR)
	@$(MAKE) MODE=$(MODE) GENERATE_COMPDB=1 all
	@{ printf '[\n'; find $(COMPDB_DIR) -type f -name '*.json' | sort | xargs -r cat; } | sed '$$s/,$$//' > $(COMPDB_FILE)
	@printf '\n]\n' >> $(COMPDB_FILE)
	
# final link
$(TARGET): $(ALL_OBJS)
	@$(MKDIR) build
	$(CXX) $(CXXFLAGS) $(ALL_OBJS) -o $(TARGET) $(OPENSSL_LIBS)

# rule A: Compile any module interface unit discovered under src/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cppm
	@$(MKDIR) $(dir $(MOD_DIR)/$*.pcm) $(@D) $(if $(GENERATE_COMPDB),$(COMPDB_DIR))
	$(CXX) $(CXXFLAGS) $(MOD_FLAGS) $(LINK_MOD_FLAGS) $(MJFLAGS) -fmodule-output=$(MOD_DIR)/$*.pcm -c $< -o $@

# rule B: Compile any translation unit discovered under src/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(MODULE_OBJS)
	@$(MKDIR) $(@D) $(if $(GENERATE_COMPDB),$(COMPDB_DIR))
	$(CXX) $(CXXFLAGS) $(LINK_MOD_FLAGS) $(MJFLAGS) -c $< -o $@

# ===============================
# Tooling & Cleanup Operations
# ===============================
docs:
	@$(MKDIR) $(DOC_DIR)
	doxygen $(DOXYFILE)

docs_pdf: docs
	@missing=0; \
	for pkg in listofitems.sty ulem.sty; do \
		if ! kpsewhich $$pkg >/dev/null 2>&1; then \
			echo "Missing TeX package file: $$pkg"; \
			missing=1; \
		fi; \
	done; \
	if [ $$missing -ne 0 ]; then \
		echo "Install required LaTeX packages (example: tlmgr --usermode install listofitems ulem)."; \
		exit 1; \
	fi
	@$(MAKE) -C $(DOC_DIR)/latex

clean:
	$(RM) build/$(MODE)
	$(RM) $(TARGET)

clean_all:
	$(RM) build
	$(RM) $(COMPDB_FILE)