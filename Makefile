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
CFLAGS = $(INCLUDE) -O2
OBJ = $(SRC:.c=.o)
LDFLAGS = 

all: unittest gravity

unittest:	$(OBJ) $(UNITTEST_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

gravity:	$(OBJ) $(GRAVITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean unittest gravity

clean:	
	rm -f $(OBJ) unittest gravity
