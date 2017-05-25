COMPILER_DIR = src/compiler/
RUNTIME_DIR = src/runtime/
SHARED_DIR = src/shared/
UTILS_DIR = src/utils/
UNITTEST_SRC = src/cli/unittest.c
GRAVITY_SRC = src/cli/gravity.c

SRC = $(wildcard $(COMPILER_DIR)*.c) \
      $(wildcard $(RUNTIME_DIR)/*.c) \
      $(wildcard $(SHARED_DIR)/*.c) \
      $(wildcard $(UTILS_DIR)/*.c)

INCLUDE = -I$(COMPILER_DIR) -I$(RUNTIME_DIR) -I$(SHARED_DIR) -I$(UTILS_DIR)
CFLAGS = $(INCLUDE) -O2 -std=gnu99 -fgnu89-inline -fPIC -DBUILD_GRAVITY_API
OBJ = $(SRC:.c=.o)

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
		# LIBTARGET = libgravity.so (not used)
		LDFLAGS = -lm
	else
		# Linux
		LIBTARGET = libgravity.so
		LDFLAGS = -lm -lrt
	endif
endif

all: unittest gravity

unittest:	$(OBJ) $(UNITTEST_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

gravity:	$(OBJ) $(GRAVITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean unittest gravity

lib: gravity
	$(CC) -shared -o $(LIBTARGET) $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) unittest gravity libgravity.so gravity.dll
