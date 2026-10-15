# ==============================================================================
#  Mini RTOS - cooperative event-driven scheduler
# ==============================================================================
#  make                 build the kernel library and the demo (debug)
#  make run             build and run the demo
#  make test            build and run the unit tests
#  make BUILD=release   optimised build without assertions
#  make SANITIZE=1      AddressSanitizer + UBSan (gcc/clang on Linux/macOS)
#  make DEFINES=-DOS_CFG_MAX_TASKS=32   override a kernel option
#  make check           static analysis (cppcheck)
#  make format          format the sources (clang-format)
#  make clean           remove build/
#  make V=1             show full compiler command lines
# ==============================================================================

# ---- toolchain ---------------------------------------------------------------
ifeq ($(origin CC),default)
CC := gcc
endif
AR ?= ar

# ---- configuration -----------------------------------------------------------
BUILD    ?= debug
SANITIZE ?= 0
V        ?= 0

BUILD_DIR := build/$(BUILD)
OBJ_DIR   := $(BUILD_DIR)/obj
LIB_DIR   := $(BUILD_DIR)/lib
BIN_DIR   := $(BUILD_DIR)/bin

ifeq ($(OS),Windows_NT)
EXE      := .exe
DEFINES  += -D__USE_MINGW_ANSI_STDIO=1
LDLIBS   += -lwinmm
else
EXE      :=
endif

ifeq ($(V),1)
Q :=
else
Q := @
endif

# ---- sources -----------------------------------------------------------------
CORE_SRCS      := $(wildcard src/core/*.c)
HOST_PORT_SRCS := $(wildcard src/port/host/*.c)
SIM_PORT_SRCS  := $(wildcard src/port/sim/*.c)
DEMO_SRCS      := $(wildcard examples/demo/*.c)
TEST_SRCS      := $(wildcard tests/*.c)

to_obj = $(patsubst %.c,$(OBJ_DIR)/%.o,$(1))

CORE_OBJS      := $(call to_obj,$(CORE_SRCS))
HOST_PORT_OBJS := $(call to_obj,$(HOST_PORT_SRCS))
SIM_PORT_OBJS  := $(call to_obj,$(SIM_PORT_SRCS))
DEMO_OBJS      := $(call to_obj,$(DEMO_SRCS))
TEST_OBJS      := $(call to_obj,$(TEST_SRCS))
ALL_OBJS       := $(CORE_OBJS) $(HOST_PORT_OBJS) $(SIM_PORT_OBJS) $(DEMO_OBJS) $(TEST_OBJS)

LIB      := $(LIB_DIR)/libminirtos.a
DEMO_BIN := $(BIN_DIR)/demo$(EXE)
TEST_BIN := $(BIN_DIR)/unit_tests$(EXE)

# ---- flags -------------------------------------------------------------------
WARNINGS := -Wall -Wextra -Wpedantic -Werror \
            -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wundef \
            -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wwrite-strings

ifeq ($(BUILD),release)
OPTIMIZE := -O2 -DNDEBUG
else
OPTIMIZE := -O0 -g3
endif

ifeq ($(SANITIZE),1)
SANITIZERS := -fsanitize=address,undefined -fno-omit-frame-pointer
endif

INCLUDES := -Iinclude
CPPFLAGS += -MMD -MP $(DEFINES)
CFLAGS   += -std=c11 $(WARNINGS) $(OPTIMIZE) $(SANITIZERS)
LDFLAGS  += $(SANITIZERS)

# Private kernel headers are visible to the kernel and to white-box tests only.
$(CORE_OBJS): INCLUDES += -Isrc/core
$(TEST_OBJS): INCLUDES += -Isrc/core -Isrc/port/sim

# ---- targets -----------------------------------------------------------------
.PHONY: all lib demo run test check format clean help

all: lib demo

lib: $(LIB)

demo: $(DEMO_BIN)

run: $(DEMO_BIN)
	./$(DEMO_BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(LIB): $(CORE_OBJS)
	@mkdir -p $(@D)
	@echo "  AR      $@"
	$(Q)$(AR) rcs $@ $^

$(DEMO_BIN): $(DEMO_OBJS) $(HOST_PORT_OBJS) $(LIB)
	@mkdir -p $(@D)
	@echo "  LD      $@"
	$(Q)$(CC) $(LDFLAGS) $(DEMO_OBJS) $(HOST_PORT_OBJS) $(LIB) $(LDLIBS) -o $@

$(TEST_BIN): $(TEST_OBJS) $(SIM_PORT_OBJS) $(LIB)
	@mkdir -p $(@D)
	@echo "  LD      $@"
	$(Q)$(CC) $(LDFLAGS) $(TEST_OBJS) $(SIM_PORT_OBJS) $(LIB) $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "  CC      $<"
	$(Q)$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -c $< -o $@

check:
	cppcheck --enable=warning,style,performance,portability --std=c11 \
	    --inline-suppr --suppress=missingIncludeSystem --error-exitcode=1 \
	    -Iinclude -Isrc/core -Isrc/port/sim src examples tests

format:
	clang-format -i $(wildcard include/*.h src/*/*.[ch] src/port/*/*.[ch] \
	    examples/*/*.[ch] tests/*.[ch])

clean:
	rm -rf build

help:
	@sed -n '2,15p' Makefile

-include $(ALL_OBJS:.o=.d)
