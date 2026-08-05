COMPILER_DIR = src/compiler/
RUNTIME_DIR = src/runtime/
SHARED_DIR = src/shared/
UTILS_DIR = src/utils/
OPT_DIR = src/optionals/
GRAVITY_SRC = src/cli/gravity.c
EXAMPLE_SRC = examples/example.c
JSONTEST_SRC = test/loadbuffer/json_bounds.c

CC ?= gcc
SRC = $(wildcard $(COMPILER_DIR)*.c) \
      $(wildcard $(RUNTIME_DIR)*.c) \
      $(wildcard $(SHARED_DIR)*.c) \
      $(wildcard $(UTILS_DIR)*.c) \
      $(wildcard $(OPT_DIR)*.c)

INCLUDE = -I$(COMPILER_DIR) -I$(RUNTIME_DIR) -I$(SHARED_DIR) -I$(UTILS_DIR) -I$(OPT_DIR)
CFLAGS = $(INCLUDE) -std=gnu99 -fgnu89-inline -fPIC -DBUILD_GRAVITY_API -MMD
OBJ = $(SRC:.c=.o)
GRAVITY_OBJ = $(GRAVITY_SRC:.c=.o)
EXAMPLE_OBJ = $(EXAMPLE_SRC:.c=.o)
JSONTEST_OBJ = $(JSONTEST_SRC:.c=.o)
DEP = $(OBJ:.o=.d) $(GRAVITY_OBJ:.o=.d) $(EXAMPLE_OBJ:.o=.d) $(JSONTEST_OBJ:.o=.d)
	
# the static library has the same name everywhere, only the shared one is platform specific
SLIBTARGET = libgravity.a

ifeq ($(OS),Windows_NT)
	# Windows
	LIBTARGET = gravity.dll
	LDFLAGS = -lm -lShlwapi
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		# MacOS
		LIBTARGET = libgravity.dylib
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),OpenBSD)
		# OpenBSD
		LIBTARGET = libgravity.so
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),FreeBSD)
		# FreeBSD
		LIBTARGET = libgravity.so
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),NetBSD)
		# NetBSD
		LIBTARGET = libgravity.so
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),DragonFly)
		# DragonFly
		LIBTARGET = libgravity.so
		LDFLAGS = -lm
	else
		# Linux
		LIBTARGET = libgravity.so
		LDFLAGS = -lm -lrt
	endif
endif

ifeq ($(mode),debug)
	CFLAGS += -g -O0 -DDEBUG
else
	CFLAGS += -O2
endif

all: gravity

gravity: $(OBJ) $(GRAVITY_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example: $(OBJ) $(EXAMPLE_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# bounds tests for the JSON scanner, see test/loadbuffer/json_bounds.c.
# Build it with a sanitizer to catch out of bounds reads:
#   make jsontest CC="clang -fsanitize=address,undefined"
jsontest: $(OBJ) $(JSONTEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

lib: $(OBJ)
	$(CC) -shared -o $(LIBTARGET) $(OBJ) $(LDFLAGS)

# static counterpart of lib: same objects, so the CLI entry point is left out here too
staticlib: $(OBJ)
	$(AR) rcs $(SLIBTARGET) $(OBJ)

clean:
	rm -f $(OBJ) $(GRAVITY_OBJ) $(EXAMPLE_OBJ) $(JSONTEST_OBJ) $(DEP) gravity example jsontest libgravity.dylib libgravity.so $(SLIBTARGET) gravity.dll

.PHONY: all clean lib staticlib

-include $(DEP)
