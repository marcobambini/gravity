COMPILER_DIR = ../src/compiler/
RUNTIME_DIR = ../src/runtime/
SHARED_DIR = ../src/shared/
UTILS_DIR = ../src/utils/
EXEC_GRAVITY_SRC = exec_gravity.c

SRC = $(wildcard $(COMPILER_DIR)*.c) \
      $(wildcard $(RUNTIME_DIR)/*.c) \
      $(wildcard $(SHARED_DIR)/*.c) \
      $(wildcard $(UTILS_DIR)/*.c)

INCLUDE = -I$(COMPILER_DIR) -I$(RUNTIME_DIR) -I$(SHARED_DIR) -I$(UTILS_DIR)
CFLAGS = $(INCLUDE) -O2 -std=gnu99 -fgnu89-inline
OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
	# Windows
	LDFLAGS = -lm -lShlwapi
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		# MacOS
		LDFLAGS = -lm 
	else
		# Linux
		LDFLAGS = -lm -lrt
	endif
endif

all: exec_gravity

exec_gravity:	$(OBJ) $(EXEC_GRAVITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean exec_gravity

clean:
	rm -f $(OBJ) exec_gravity
