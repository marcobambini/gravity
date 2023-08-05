COMPILER_DIR = src/compiler/
RUNTIME_DIR = src/runtime/
SHARED_DIR = src/shared/
UTILS_DIR = src/utils/
OPT_DIR = src/optionals/
GRAVITY_SRC = src/cli/gravity.c
EXAMPLE_SRC = examples/example.c

CC ?= gcc
SRC = $(wildcard $(COMPILER_DIR)*.c) \
      $(wildcard $(RUNTIME_DIR)*.c) \
      $(wildcard $(SHARED_DIR)*.c) \
      $(wildcard $(UTILS_DIR)*.c) \
      $(wildcard $(OPT_DIR)*.c)

INCLUDE = -I$(COMPILER_DIR) -I$(RUNTIME_DIR) -I$(SHARED_DIR) -I$(UTILS_DIR) -I$(OPT_DIR)
OBJ = $(SRC:.c=.o)

CFLAGS = $(INCLUDE) -fPIC -DBUILD_GRAVITY_API
ifneq ($(CC),tcc)
    CFLAGS += -std=gnu99 -fgnu89-inline
endif


ifeq ($(OS),Windows_NT)
	# Windows
	LIBTARGET = gravity.dll
	LDFLAGS = -lShlwapi
	ifeq ($(CC),tcc)
		LDFLAGS += -lucrtbase
		EXEXT = .exe
	else
		LDFLAGS += -lm
	endif
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

all: gravity$(EXEXT)

gravity$(EXEXT):	$(OBJ) $(GRAVITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example$(EXEXT):	$(OBJ) $(EXAMPLE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

lib: gravity$(EXEXT)
	$(CC) -shared -o $(LIBTARGET) $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) gravity gravity.def gravity.exe example example.def example.exe libgravity.so gravity.dll
	
.PHONY: all clean gravity$(EXEXT) example$(EXEXT)
