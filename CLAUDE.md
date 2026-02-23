# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Gravity is a dynamically typed, embeddable programming language written in portable C99 with no external dependencies (except stdlib). It features Swift-like syntax and supports procedural, OOP, functional, and prototype-based programming paradigms. Originally developed for the Creo project for cross-platform iOS/Android scripting.

## Build Commands

```bash
make                    # Build the gravity CLI executable
make mode=debug         # Debug build (-g -O0 -DDEBUG)
make lib                # Build shared library (libgravity.dylib/so/dll)
make example            # Build the C API example
make clean              # Clean all build artifacts
```

Compiler flags: `-std=gnu99 -fgnu89-inline -fPIC -DBUILD_GRAVITY_API`

## Testing

```bash
./gravity -t test/unittest              # Run all unit tests
./test/unittest/run_all.sh              # Run all tests via shell script (with timeouts)
./gravity test/unittest/somefile.gravity # Run a single test file
./gravity -c test.gravity               # Compile only (produces gravity.json)
./gravity -x gravity.json               # Execute compiled bytecode
./gravity -i 'print("hello")'           # Inline execution
```

CI runs: `make && test/unittest/run_all.sh`

## Architecture

The codebase follows a multi-pass compiler pipeline feeding into a bytecode VM:

**Source → Lexer → Parser → AST → Semantic Check (2 passes) → IR → Optimizer → Bytecode → VM**

### Source Layout

- **`src/compiler/`** — Multi-pass compiler: lexer, parser, AST, two semantic analysis passes (`semacheck1`, `semacheck2`), IR code generation, optimizer, and final codegen
- **`src/runtime/`** — Stack-based virtual machine (`gravity_vm`), built-in classes/functions (`gravity_core`), VM execution macros
- **`src/shared/`** — Value representation and type system (`gravity_value`), hash table, dynamic array, memory management/GC, opcode definitions
- **`src/utils/`** — Debug utilities, JSON serialization, file I/O helpers
- **`src/optionals/`** — Optional modules (math, file, json, env) registered via `gravity_opt_register()`
- **`src/cli/gravity.c`** — CLI entry point

### Key Design Patterns

- The compiler uses an AST visitor pattern (`gravity_visitor`) for tree traversal
- The VM is stack-based with a mark-and-sweep garbage collector
- Optional modules are self-contained and registered at runtime
- The embedding API uses a delegate pattern (`gravity_delegate_t`) for callbacks (errors, logging, etc.)

### Embedding API

Core API in `src/runtime/` and example usage in `examples/example.c`:
- `gravity_compiler_create/run` — compile source to closures
- `gravity_vm_new/runmain/runclosure` — create VM and execute code
- `gravity_vm_loadfile/loadbuffer` — load from file or memory

## Code Style

- Private functions are `static` and don't use the `gravity_` prefix
- Public API functions use the `gravity_` prefix
- Unit tests are individual `.gravity` source files in `test/unittest/`, organized by compiler phase and feature area
