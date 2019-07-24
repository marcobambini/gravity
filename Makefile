COMPILER_DIR = src/compiler/
RUNTIME_DIR = src/runtime/
SHARED_DIR = src/shared/
UTILS_DIR = src/utils/
OPT_DIR = src/optionals/
GRAVITY_SRC = src/cli/gravity.c

CC ?= gcc
SRC = $(wildcard $(COMPILER_DIR)*.c) \
      $(wildcard $(RUNTIME_DIR)/*.c) \
      $(wildcard $(SHARED_DIR)/*.c) \
      $(wildcard $(UTILS_DIR)/*.c) \
      $(wildcard $(OPT_DIR)/*.c)

INCLUDE = -I$(COMPILER_DIR) -I$(RUNTIME_DIR) -I$(SHARED_DIR) -I$(UTILS_DIR) -I$(OPT_DIR)
CFLAGS = $(INCLUDE) -std=gnu99 -fgnu89-inline -fPIC -DBUILD_GRAVITY_API
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
		CFLAGS += -D_WITH_GETLINE
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),FreeBSD)
		# FreeBSD
		CFLAGS += -D_WITH_GETLINE
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),NetBSD)
		# NetBSD
		CFLAGS += -D_WITH_GETLINE
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),DragonFly)
		# DragonFly
		CFLAGS += -D_WITH_GETLINE
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

gravity:	$(OBJ) $(GRAVITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean gravity

src/compiler/gravity_ast.o: src/compiler/gravity_ast.c src/compiler/gravity_ast.h src/utils/gravity_json.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_array.h src/compiler/gravity_visitor.h src/shared/gravity_hash.h src/compiler/gravity_symboltable.h src/shared/gravity_value.h src/compiler/gravity_token.h
src/compiler/gravity_codegen.o: src/compiler/gravity_codegen.c src/compiler/gravity_ast.h src/utils/gravity_json.h src/compiler/gravity_optimizer.h src/compiler/gravity_ircode.h src/shared/gravity_opcodes.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/shared/gravity_array.h src/compiler/gravity_visitor.h src/shared/gravity_hash.h src/compiler/gravity_symboltable.h src/shared/gravity_value.h src/compiler/gravity_token.h src/shared/gravity_delegate.h src/compiler/gravity_codegen.h src/shared/gravity_memory.h
src/compiler/gravity_compiler.o: src/compiler/gravity_compiler.c src/compiler/gravity_parser.h src/utils/gravity_json.h src/compiler/gravity_optimizer.h src/compiler/gravity_semacheck2.h src/compiler/gravity_ast.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/runtime/gravity_core.h src/shared/gravity_array.h src/compiler/gravity_compiler.h src/shared/gravity_hash.h src/shared/gravity_value.h src/compiler/gravity_semacheck1.h src/compiler/gravity_token.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h src/compiler/gravity_codegen.h src/shared/gravity_memory.h
src/compiler/gravity_ircode.o: src/runtime/gravity_debug.h src/utils/gravity_json.h src/compiler/gravity_ircode.h src/utils/gravity_debug.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_array.h src/shared/gravity_value.h src/shared/gravity_opcodes.h src/compiler/gravity_ircode.c
src/compiler/gravity_lexer.o: src/compiler/gravity_lexer.c src/utils/gravity_json.h src/compiler/gravity_lexer.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_array.h src/shared/gravity_value.h src/compiler/gravity_token.h src/shared/gravity_delegate.h
src/compiler/gravity_optimizer.o: src/compiler/gravity_optimizer.c src/utils/gravity_json.h src/compiler/gravity_optimizer.h src/compiler/gravity_ircode.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/shared/gravity_opcodes.h src/shared/gravity_hash.h src/shared/gravity_value.h src/shared/gravity_array.h
src/compiler/gravity_parser.o: src/compiler/gravity_parser.c src/optionals/gravity_math.h src/compiler/gravity_parser.h src/utils/gravity_json.h src/compiler/gravity_lexer.h src/optionals/gravity_optionals.h src/compiler/gravity_ast.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/runtime/gravity_core.h src/shared/gravity_array.h src/compiler/gravity_compiler.h src/shared/gravity_hash.h src/compiler/gravity_symboltable.h src/shared/gravity_value.h src/optionals/gravity_env.h src/compiler/gravity_token.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h
src/compiler/gravity_semacheck1.o: src/compiler/gravity_semacheck1.c src/utils/gravity_json.h src/compiler/gravity_ast.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_array.h src/compiler/gravity_visitor.h src/compiler/gravity_compiler.h src/compiler/gravity_symboltable.h src/shared/gravity_value.h src/compiler/gravity_semacheck1.h src/compiler/gravity_token.h src/shared/gravity_delegate.h
src/compiler/gravity_semacheck2.o: src/compiler/gravity_semacheck2.c src/utils/gravity_json.h src/compiler/gravity_semacheck2.h src/compiler/gravity_ast.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/runtime/gravity_core.h src/shared/gravity_array.h src/compiler/gravity_visitor.h src/compiler/gravity_compiler.h src/compiler/gravity_symboltable.h src/shared/gravity_value.h src/compiler/gravity_token.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h
src/compiler/gravity_symboltable.o: src/compiler/gravity_symboltable.c src/utils/gravity_json.h src/compiler/gravity_ast.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/shared/gravity_array.h src/shared/gravity_hash.h src/compiler/gravity_symboltable.h src/shared/gravity_value.h src/compiler/gravity_token.h src/shared/gravity_memory.h
src/compiler/gravity_token.o: src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/compiler/gravity_token.h src/compiler/gravity_token.c
src/compiler/gravity_visitor.o: src/compiler/gravity_visitor.c src/compiler/gravity_ast.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/shared/gravity_array.h src/compiler/gravity_visitor.h src/compiler/gravity_token.h
src/optionals//gravity_env.o: src/runtime/gravity_debug.h src/utils/gravity_json.h src/utils/gravity_debug.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/runtime/gravity_core.h src/shared/gravity_array.h src/runtime/gravity_vm.h src/shared/gravity_hash.h src/shared/gravity_value.h src/optionals/gravity_env.h src/optionals/gravity_env.c src/shared/gravity_opcodes.h src/shared/gravity_delegate.h src/runtime/gravity_vmmacros.h src/shared/gravity_memory.h
src/optionals//gravity_math.o: src/optionals/gravity_math.h src/optionals/gravity_math.c src/utils/gravity_json.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/runtime/gravity_core.h src/shared/gravity_array.h src/shared/gravity_hash.h src/shared/gravity_value.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h src/runtime/gravity_vmmacros.h src/shared/gravity_memory.h
src/runtime//gravity_vm.o: src/runtime/gravity_debug.h src/optionals/gravity_math.h src/utils/gravity_json.h src/optionals/gravity_optionals.h src/utils/gravity_debug.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/runtime/gravity_core.h src/shared/gravity_array.h src/runtime/gravity_vm.c src/shared/gravity_hash.h src/shared/gravity_value.h src/optionals/gravity_env.h src/shared/gravity_opcodes.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h src/runtime/gravity_vmmacros.h src/shared/gravity_memory.h
src/runtime//gravity_core.o: src/runtime/gravity_debug.h src/utils/gravity_json.h src/utils/gravity_debug.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/runtime/gravity_core.h src/shared/gravity_array.h src/runtime/gravity_core.c src/shared/gravity_hash.h src/shared/gravity_value.h src/shared/gravity_opcodes.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h src/runtime/gravity_vmmacros.h src/shared/gravity_memory.h
src/shared//gravity_hash.o: src/utils/gravity_json.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/shared/gravity_array.h src/shared/gravity_hash.c src/shared/gravity_hash.h src/shared/gravity_value.h src/shared/gravity_memory.h
src/shared//gravity_value.o: src/utils/gravity_json.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_macros.h src/runtime/gravity_core.h src/shared/gravity_array.h src/shared/gravity_hash.h src/shared/gravity_value.c src/shared/gravity_value.h src/shared/gravity_opcodes.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h src/runtime/gravity_vmmacros.h src/shared/gravity_memory.h
src/shared//gravity_memory.o: src/utils/gravity_json.h src/shared/gravity_memory.h src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_array.h src/shared/gravity_value.h src/runtime/gravity_vm.h src/shared/gravity_delegate.h src/shared/gravity_memory.c
src/utils//gravity_json.o: src/utils/gravity_json.c src/utils/gravity_json.h src/utils/gravity_utils.h src/shared/gravity_memory.h src/utils/gravity_json.c src/utils/gravity_json.h src/utils/gravity_utils.h src/shared/gravity_memory.h
src/utils//gravity_debug.o: src/utils/gravity_json.h src/utils/gravity_debug.h src/utils/gravity_debug.c src/compiler/debug_macros.h src/utils/gravity_utils.h src/shared/gravity_array.h src/shared/gravity_value.h src/shared/gravity_opcodes.h src/runtime/gravity_vmmacros.h src/shared/gravity_memory.h
src/utils//gravity_utils.o: src/utils/gravity_utils.c src/utils/gravity_utils.h src/shared/gravity_memory.h

lib: gravity
	$(CC) -shared -o $(LIBTARGET) $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) gravity libgravity.so gravity.dll
