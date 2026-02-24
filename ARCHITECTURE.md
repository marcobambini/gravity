# Gravity Language Architecture

This document provides a detailed technical description of the Gravity programming language implementation, covering the full compilation pipeline, runtime virtual machine, type system, garbage collector, embedding API, and supporting infrastructure.

## Table of Contents

- [1. High-Level Overview](#1-high-level-overview)
- [2. Compilation Pipeline](#2-compilation-pipeline)
  - [2.1 Lexer](#21-lexer)
  - [2.2 Parser](#22-parser)
  - [2.3 Abstract Syntax Tree (AST)](#23-abstract-syntax-tree-ast)
  - [2.4 Semantic Analysis — Pass 1](#24-semantic-analysis--pass-1)
  - [2.5 Semantic Analysis — Pass 2](#25-semantic-analysis--pass-2)
  - [2.6 Code Generation (AST → IR)](#26-code-generation-ast--ir)
  - [2.7 IR Representation](#27-ir-representation)
  - [2.8 Optimizer & Bytecode Emission](#28-optimizer--bytecode-emission)
- [3. Runtime Virtual Machine](#3-runtime-virtual-machine)
  - [3.1 VM Structure](#31-vm-structure)
  - [3.2 Instruction Dispatch](#32-instruction-dispatch)
  - [3.3 Instruction Set](#33-instruction-set)
  - [3.4 Instruction Encoding](#34-instruction-encoding)
  - [3.5 Stack and Call Frames](#35-stack-and-call-frames)
  - [3.6 Fiber (Coroutine) Model](#36-fiber-coroutine-model)
  - [3.7 Execution Flow](#37-execution-flow)
  - [3.8 Fast-Path Optimizations](#38-fast-path-optimizations)
- [4. Value System and Type Hierarchy](#4-value-system-and-type-hierarchy)
  - [4.1 Value Representation](#41-value-representation)
  - [4.2 Object Header and GC Metadata](#42-object-header-and-gc-metadata)
  - [4.3 Built-in Types](#43-built-in-types)
  - [4.4 Functions and Closures](#44-functions-and-closures)
  - [4.5 Upvalues](#45-upvalues)
  - [4.6 Classes and Instances](#46-classes-and-instances)
- [5. Garbage Collector](#5-garbage-collector)
- [6. Core Data Structures](#6-core-data-structures)
  - [6.1 Hash Table](#61-hash-table)
  - [6.2 Dynamic Array](#62-dynamic-array)
  - [6.3 Memory Management](#63-memory-management)
- [7. Optional Modules](#7-optional-modules)
  - [7.1 Registration Pattern](#71-registration-pattern)
  - [7.2 Math Module](#72-math-module)
  - [7.3 File Module](#73-file-module)
  - [7.4 JSON Module](#74-json-module)
  - [7.5 ENV Module](#75-env-module)
- [8. Embedding API](#8-embedding-api)
  - [8.1 Compiler API](#81-compiler-api)
  - [8.2 VM API](#82-vm-api)
  - [8.3 Delegate Pattern](#83-delegate-pattern)
  - [8.4 Bridging](#84-bridging)
- [9. Utilities](#9-utilities)
- [10. CLI](#10-cli)
- [11. Build System](#11-build-system)
- [12. Test Infrastructure](#12-test-infrastructure)

---

## 1. High-Level Overview

Gravity is a dynamically typed, embeddable programming language written in portable C99 with zero external dependencies (only stdlib). It features Swift-like syntax and supports procedural, object-oriented, functional, and prototype-based programming paradigms.

The implementation follows a classic multi-pass compiler architecture that produces register-based bytecode executed by a stack-based virtual machine with coroutine (fiber) support.

### Source Layout

```
src/
├── cli/            CLI entry point (gravity.c)
├── compiler/       Lexer, parser, AST, semantic analysis, IR, optimizer, codegen
├── runtime/        Virtual machine (gravity_vm), built-in types (gravity_core)
├── shared/         Value representation, opcodes, hash table, dynamic array, memory/GC
├── optionals/      Optional modules: Math, File, JSON, ENV
└── utils/          Debug disassembler, JSON serialization, file I/O, UTF-8 utilities
```

### Full Pipeline

```
Source Code
    │
    ▼
┌──────────┐
│  Lexer   │  Character stream → Token stream
└────┬─────┘
     ▼
┌──────────┐
│  Parser  │  Token stream → Abstract Syntax Tree
└────┬─────┘
     ▼
┌──────────────┐
│ Semacheck 1  │  Gather non-local declarations into symbol tables
└──────┬───────┘
       ▼
┌──────────────┐
│ Semacheck 2  │  Resolve identifiers, detect upvalues, validate scopes
└──────┬───────┘
       ▼
┌──────────────┐
│   Codegen    │  AST → IR instructions (virtual registers)
└──────┬───────┘
       ▼
┌──────────────┐
│  Optimizer   │  Constant folding, dead code elimination, label resolution
└──────┬───────┘
       ▼
┌──────────────┐
│  Bytecode    │  Packed 32-bit instruction words
└──────┬───────┘
       ▼
┌──────────────┐
│     VM       │  Register-based execution with computed goto dispatch
└──────────────┘
```

The compiler entry point (`gravity_compiler_run` in `gravity_compiler.c`) orchestrates this pipeline: it creates a mini VM for GC during compilation, runs the parser to produce an AST, applies both semantic passes, generates IR code, optimizes it into final bytecode, and returns a `gravity_closure_t` ready for execution.

---

## 2. Compilation Pipeline

### 2.1 Lexer

**Files:** `src/compiler/gravity_lexer.c`, `src/compiler/gravity_lexer.h`

The lexer is a zero-allocation streaming tokenizer that scans source code character-by-character, producing tokens without copying or modifying the input buffer. Token values are pointers into the original source string.

#### Lexer State

```c
struct gravity_lexer_t {
    const char *buffer;           // source buffer (not owned)
    uint32_t    offset;           // current byte offset
    uint32_t    position;         // current character position (UTF-8 aware)
    uint32_t    length;           // buffer length in bytes
    uint32_t    lineno;           // 1-based line number
    uint32_t    colno;            // 0-based column number
    uint32_t    fileid;           // source file identifier
    gtoken_s    token;            // current token
    bool        peeking;         // in peek mode
    gravity_delegate_t *delegate; // error callback
};
```

#### Token Structure

```c
struct gtoken_s {
    gtoken_t    type;      // token type (enum)
    uint32_t    lineno;    // line number
    uint32_t    colno;     // column at end of token
    uint32_t    position;  // byte offset of first character
    uint32_t    bytes;     // length in bytes
    uint32_t    length;    // length in UTF-8 characters
    uint32_t    fileid;    // source file ID
    gbuiltin_t  builtin;   // builtin identifier (__LINE__, __FILE__, etc.)
    const char *value;     // pointer into source buffer (NOT null-terminated)
};
```

#### Token Categories (~80 total)

| Category | Count | Examples |
|----------|-------|---------|
| General | 8 | `EOF`, `ERROR`, `COMMENT`, `STRING`, `NUMBER`, `IDENTIFIER`, `SPECIAL`, `MACRO` |
| Keywords | 36 | `func`, `class`, `var`, `const`, `if`, `else`, `for`, `while`, `return`, `import`, `enum`, `switch`, `true`, `false`, `null`, `undefined`, `super`, `isa`, ... |
| Operators | 36 | `+`, `-`, `*`, `/`, `%`, `&`, `\|`, `^`, `~`, `<<`, `>>`, `<`, `<=`, `==`, `!=`, `===`, `!==`, `~=`, `&&`, `\|\|`, `=`, `+=`, `..<`, `...`, ... |
| Punctuators | 10 | `(`, `)`, `[`, `]`, `{`, `}`, `;`, `:`, `.`, `,` |

#### Key Features

- **UTF-8 support:** Tracks both byte offset and character position separately using `utf8_charbytes()`. Handles 1–4 byte sequences.
- **Number literals:** Decimal, hexadecimal (`0x`), binary (`0b`), octal (`0o`), floating-point with scientific notation (`1.25e-2`). Uses state-machine with lookahead.
- **String literals:** Single (`'`) and double (`"`) quoted strings with backslash escapes. Multi-line strings tracked across line boundaries.
- **String interpolation:** Detected as `LITERAL_STRING_INTERPOLATED` for `"text \(expr)"` syntax.
- **Nested block comments:** `/* ... /* ... */ ... */` with a nesting depth counter.
- **Builtin identifiers:** `__LINE__`, `__FILE__`, `__COLUMN__`, `__CLASS__`, `__FUNC__` resolved during lexing.
- **Line separators:** CR, LF, CR+LF, NEL (U+0085), LS (U+2028).

---

### 2.2 Parser

**Files:** `src/compiler/gravity_parser.c`, `src/compiler/gravity_parser.h`

The parser uses a **Pratt parser** (top-down operator precedence) to build an Abstract Syntax Tree. It supports a lexer stack for `#include` directives and maintains a declaration scope stack for context tracking.

#### Parser State

```c
struct gravity_parser_t {
    lexer_r              *lexer;        // stack of lexers (for includes)
    gnode_r              *declarations; // declaration scope stack
    gnode_r              *statements;   // statement list being built
    gravity_delegate_t   *delegate;     // error callbacks
    uint32_t              nerrors;      // accumulated error count
    uint32_t              unique_id;    // unique identifier counter
    uint32_t              depth;        // statement nesting depth
    uint32_t              expr_depth;   // expression nesting depth
};
```

#### Precedence Levels

```
PREC_LOWEST       =   0
PREC_ASSIGN       =  90    =  +=  -=  *=  /=  %=  <<=  >>=  &=  |=  ^=
PREC_TERNARY      = 100    ?:
PREC_LOGICAL_OR   = 110    ||
PREC_LOGICAL_AND  = 120    &&
PREC_COMPARISON   = 130    <  <=  >  >=  ==  !=  ===  !==  ~=
PREC_ISA          = 132    is
PREC_RANGE        = 135    ..<  ...
PREC_TERM         = 140    +  -  |  ^
PREC_FACTOR       = 150    *  /  %  &
PREC_SHIFT        = 160    <<  >>
PREC_UNARY        = 170    +  -  !  ~
PREC_CALL         = 200    .  (  [
```

Each grammar rule carries a prefix handler, infix handler, precedence level, and a right-associativity flag:

```c
typedef struct {
    parse_func  prefix;      // prefix expression handler (or NULL)
    parse_func  infix;       // infix expression handler (or NULL)
    prec_level  precedence;  // binding power
    const char *name;        // operator name for diagnostics
    bool        right;       // right-associative
} grammar_rule;
```

#### Statement Types

- Compound statements (blocks)
- Variable/constant declarations (`var`, `const`, with optional type annotations and initialization)
- Function declarations (with parameters, default values)
- Class declarations (with inheritance, access modifiers, struct flag)
- Enum declarations
- Module declarations
- Control flow: `if`/`else`, `switch`/`case`/`default`, `for`, `while`, `repeat`
- Jump statements: `break`, `continue`, `return`
- Expression statements
- Empty statements

#### Error Recovery

- **One error per line:** Suppresses cascading errors from the same source line.
- **Token synchronization:** `parse_skip_until()` advances to a recovery point (e.g., next statement boundary).
- **Recursion limits:** `MAX_RECURSION_DEPTH = 1000` for statements, `MAX_EXPRESSION_DEPTH = 512` for expressions.

---

### 2.3 Abstract Syntax Tree (AST)

**Files:** `src/compiler/gravity_ast.c`, `src/compiler/gravity_ast.h`

The AST uses a **non-uniform node design** — each node type has its own struct, but all share a common base for dispatch. A visitor pattern (`gvisitor_t`) is used for all tree traversals.

#### Node Types (21 total)

**Statements (7):**

| Node | Purpose |
|------|---------|
| `NODE_LIST_STAT` | Root/global statement list |
| `NODE_COMPOUND_STAT` | Block with local scope and symbol table |
| `NODE_LABEL_STAT` | Switch case/default label |
| `NODE_FLOW_STAT` | `if`/`else`, `switch`, ternary |
| `NODE_JUMP_STAT` | `break`, `continue`, `return` |
| `NODE_LOOP_STAT` | `while`, `repeat`, `for` loops |
| `NODE_EMPTY_STAT` | Empty statement |

**Declarations (6):**

| Node | Purpose |
|------|---------|
| `NODE_ENUM_DECL` | Enumeration definition |
| `NODE_FUNCTION_DECL` | Function (with params, defaults, upvalue list) |
| `NODE_VARIABLE_DECL` | Variable/constant declaration group |
| `NODE_CLASS_DECL` | Class (with superclass, protocols, ivar counts) |
| `NODE_MODULE_DECL` | Module definition |
| `NODE_VARIABLE` | Individual variable within a declaration |

**Expressions (8):**

| Node | Purpose |
|------|---------|
| `NODE_BINARY_EXPR` | Binary operations |
| `NODE_UNARY_EXPR` | Unary operations |
| `NODE_FILE_EXPR` | `__FILE__` constant |
| `NODE_LIST_EXPR` | Array/map literals |
| `NODE_LITERAL_EXPR` | Numbers, strings, booleans |
| `NODE_IDENTIFIER_EXPR` | Variable references |
| `NODE_KEYWORD_EXPR` | `true`, `false`, `null`, `undefined`, `super` |
| `NODE_POSTFIX_EXPR` | Calls, subscripts, property access (with subtypes) |

#### Base Node

```c
typedef struct {
    gnode_n   tag;            // node type discriminant
    uint32_t  refcount;       // reference counting for shared nodes
    uint32_t  block_length;   // byte length (for autocompletion)
    gtoken_s  token;          // source location
    bool      is_assignment;  // assignment target flag
    void     *decl;           // enclosing declaration
} gnode_t;
```

#### Location Tracking

After semantic analysis, each identifier is annotated with a resolved location:

```c
typedef enum {
    LOCATION_LOCAL,             // local variable
    LOCATION_GLOBAL,            // global variable
    LOCATION_UPVALUE,           // closure upvalue
    LOCATION_CLASS_IVAR_SAME,   // instance variable (same class)
    LOCATION_CLASS_IVAR_OUTER   // instance variable (outer class)
} gnode_location_type;

typedef struct {
    gnode_location_type type;
    uint16_t index;             // symbol index
    uint16_t nup;               // upvalue or outer index
} gnode_location_t;
```

#### Visitor Pattern

```c
typedef struct gvisitor {
    uint32_t nerr;
    void    *data;              // visitor-specific state
    void    *delegate;          // error callback delegate

    // 22 callbacks — one per node type, plus pre/post hooks
    void (*visit_pre)(visitor, node);
    void (*visit_post)(visitor, node);
    void (*visit_list_stmt)(visitor, node);
    void (*visit_compound_stmt)(visitor, node);
    void (*visit_function_decl)(visitor, node);
    // ... one for each AST node type
} gvisitor_t;
```

The dispatch function `gvisit()` calls `visit_pre`, then the node-specific callback based on `node->tag`, then `visit_post`.

---

### 2.4 Semantic Analysis — Pass 1

**File:** `src/compiler/gravity_semacheck1.c`

The first semantic pass gathers all **non-local declarations** into symbol tables, enabling forward references. It does not perform full name resolution or type checking.

#### What It Does

1. Creates symbol tables for each scope (global, class, module, enum).
2. Inserts function, class, enum, module, and variable declarations.
3. Reports duplicate declaration errors.
4. Assigns instance variable indices for class members.
5. Applies name mangling for static class members (prefixed with `"$"`).

#### Symbol Table

```c
struct symboltable_t {
    ghash_r    *stack;    // stack of hash tables (nested scopes)
    uint16_t    count1;   // local variable counter
    uint16_t    count2;   // instance variable counter
    uint16_t    count3;   // static variable counter
    symtable_tag tag;     // GLOBAL, FUNC, CLASS, MODULE, or ENUM
};
```

This pass enables forward references — a function can call another function declared later in the same scope:

```swift
func foo() { return bar(); }
func bar() { return 42; }
```

---

### 2.5 Semantic Analysis — Pass 2

**File:** `src/compiler/gravity_semacheck2.c`

The second semantic pass validates all identifiers within function bodies, resolves variable references, and detects closure upvalues.

#### What It Does

1. Validates all identifier references (reports "undefined variable" errors).
2. Resolves each identifier to its declaration and sets the `location` field.
3. Detects upvalue usage and builds upvalue lists for closures.
4. Validates declaration nesting constraints.
5. Checks `break`/`continue` appear only inside loops.
6. Validates module declarations are at global scope.

#### Identifier Lookup Order

The lookup traverses the declaration stack from innermost to outermost:

1. Local scope (current compound statement)
2. Enclosing function scopes
3. Enclosing class scopes (including superclass hierarchy)
4. Module scope
5. Global scope

#### Declaration Nesting Rules

What can be declared inside each construct:

```
         │   func    var    enum   class   module
-------------------------------------------------
func     │   YES     YES    NO     YES     YES
var      │   YES     NO     NO     YES     YES
enum     │   YES     NO     NO     YES     YES
class    │   YES     NO     NO     YES     YES
module   │   NO      NO     NO     NO      NO
-------------------------------------------------
```

---

### 2.6 Code Generation (AST → IR)

**File:** `src/compiler/gravity_codegen.c`

The code generator walks the AST using the visitor pattern and emits IR instructions with virtual registers. It maintains a context stack of functions and classes being compiled.

```c
struct codegen_t {
    gravity_object_r  context;    // stack of functions/classes
    gnode_class_r     superfix;   // superclass resolution stack
    uint32_t          lasterror;  // last error line
    gravity_vm       *vm;         // mini VM for GC during codegen
};
```

#### Key Responsibilities

- **Operator mapping:** Converts token operators to opcodes (e.g., `TOK_OP_ADD` → `ADD`).
- **Implicit self:** Inserts self parameter for instance methods.
- **Super calls:** Emits `LOADS` instruction for superclass method lookup.
- **Collection literals:** `LISTNEW`/`MAPNEW` + `SETLIST` instructions.
- **Range literals:** `RANGENEW` with inclusive/exclusive flag.
- **String interpolation:** Converts `"text \(expr)"` into string concatenation operations.
- **Closures:** `CLOSURE` instruction references the function in the constant pool; `CLOSE` releases upvalues when scope exits.

---

### 2.7 IR Representation

**Files:** `src/compiler/gravity_ircode.c`, `src/compiler/gravity_ircode.h`

The IR is a flat sequence of instructions with virtual registers, acting as the bridge between the AST and final packed bytecode.

#### IR Instruction

```c
typedef struct {
    opcode_t  op;              // operation code
    optag_t   tag;             // metadata tag
    int32_t   p1, p2, p3;     // operand parameters
    union {
        double  d;             // embedded float constant (DOUBLE_TAG)
        int64_t n;             // embedded int constant (INT_TAG)
    };
    uint32_t  lineno;          // source line for debug info
} inst_t;
```

#### Instruction Tags

| Tag | Meaning |
|-----|---------|
| `NO_TAG` | Normal instruction |
| `INT_TAG` | Carries an embedded integer literal |
| `DOUBLE_TAG` | Carries an embedded float literal |
| `LABEL_TAG` | Label marker (resolved to offset by optimizer) |
| `SKIP_TAG` | Dead instruction (removed by optimizer) |
| `RANGE_INCLUDE_TAG` | Inclusive range flag |
| `RANGE_EXCLUDE_TAG` | Exclusive range flag |
| `PRAGMA_MOVE_OPTIMIZATION` | Hint for move elimination |

#### Register Allocation

The IR uses a bitmask-based register allocator (256 registers max = 32 bytes of bitmask):

- **Local registers** `[0 .. nlocals-1]`: Reserved for parameters and local variables.
- **Temp registers** `[nlocals .. 255]`: Allocated/freed for expression evaluation.
- Register 0 is always reserved.

Key operations:
- `ircode_register_push_temp()` — allocate the next free temp register.
- `ircode_register_pop()` — free the most recently allocated temp register.
- `ircode_register_first_temp_available()` — find first free temp slot.

#### Label Management

Three separate label stacks manage control flow:
- `label_true` — target for true branch of conditionals.
- `label_false` — target for false branch.
- `label_check` — target for loop checks and safety guards.

---

### 2.8 Optimizer & Bytecode Emission

**Files:** `src/compiler/gravity_optimizer.c`, `src/compiler/gravity_optimizer.h`

The optimizer is the final compilation stage. It converts IR instructions into packed 32-bit bytecodes, resolves labels, and applies peephole optimizations.

#### Optimizations Performed

1. **Constant folding:** Arithmetic on constant operands evaluated at compile time.
   ```
   LOADI r1, 5 ; LOADI r2, 3 ; ADD r0, r1, r2  →  LOADI r0, 8
   ```

2. **Dead code elimination:** Unreachable instructions after unconditional jumps/returns are marked `SKIP` and removed.

3. **Move elimination:** Redundant `MOVE` instructions are detected via `PRAGMA_MOVE_OPTIMIZATION` hints and removed when safe.

4. **Label resolution:** Symbolic labels are mapped to concrete instruction offsets.

#### 32-Bit Instruction Encoding

```
Standard (3 operands):  [ opcode:6 | A:8 | B:8 | C:10 ]
LOADI (immediate):      [ opcode:6 | A:8 | sign:1 | N:17 ]
JUMP (offset):          [ opcode:6 | N:26 ]
```

---

## 3. Runtime Virtual Machine

### 3.1 VM Structure

**Files:** `src/runtime/gravity_vm.c`, `src/runtime/gravity_vm.h`

The VM is an opaque struct (`gravity_vm`) with the following key components:

```c
struct gravity_vm {
    // Execution
    gravity_fiber_t     *fiber;       // current fiber (coroutine)
    gravity_hash_t      *context;     // global variable table
    gravity_delegate_t  *delegate;    // runtime delegate
    uint32_t             pc;          // program counter
    bool                 aborted;     // runtime error flag

    // Recursion limits
    uint32_t             maxccalls;   // max nested C calls (default: 100)
    uint32_t             nccalls;     // current C call depth
    gravity_int_t        maxrecursion;// max recursive depth (0 = unlimited)

    // Garbage collector
    int32_t              gcenabled;   // reference-counted enable flag
    gravity_object_t    *gchead;      // linked list of all GC objects
    gravity_object_r     graylist;    // mark phase gray list
    gravity_object_r     gctemp;      // temporary GC-protected objects
    gravity_int_t        memallocated;// total allocated memory
    gravity_int_t        gcthreshold; // GC trigger threshold (default: 5MB)
    gravity_int_t        gcminthreshold; // minimum threshold (default: 1MB)
    gravity_float_t      gcratio;     // threshold growth ratio (default: 0.5)

    // Callbacks
    vm_transfer_cb       transfer;    // object allocation hook
    vm_cleanup_cb        cleanup;     // VM cleanup hook
    vm_filter_cb         filter;      // selective cleanup filter
};
```

An internal operator name cache (`cache[GRAVITY_VTABLE_SIZE]`) holds pre-computed strings for operator method names (`"+"`, `"-"`, `"*"`, etc.) to avoid repeated allocations during dispatch.

---

### 3.2 Instruction Dispatch

**File:** `src/runtime/gravity_vmmacros.h`

The VM uses **computed goto** for instruction dispatch (GCC/Clang), falling back to a `switch` statement on MSVC:

```c
// Computed goto (GCC/Clang):
#define DISPATCH()  goto *dispatchTable[OPCODE_GET_OPCODE(*ip)]

// Switch fallback (MSVC):
#define INTERPRET_LOOP  switch (OPCODE_GET_OPCODE(*ip))
#define CASE_CODE(x)    case x:
```

Computed goto provides O(1) dispatch with no branch prediction overhead. Each opcode is a label address stored in a static table, and `DISPATCH()` performs an indirect jump.

Key macros in the dispatch loop:

| Macro | Purpose |
|-------|---------|
| `OPCODE_GET_OPCODE(inst)` | Extract 6-bit opcode |
| `OPCODE_GET_ONE8bit_ONE18bit(inst, A, N)` | Decode register + immediate |
| `OPCODE_GET_THREE8bit(inst, A, B, C)` | Decode three register operands |
| `LOAD_FRAME()` | Synchronize local variables from fiber state |
| `STORE_FRAME()` | Save local variables back to fiber state |
| `PUSH_FRAME(closure, stackstart, dest, nargs)` | Create a new call frame |
| `FN_COUNTREG(f, nargs)` | Compute register window size: `max(nparams, nargs) + nlocals + ntemps` |

---

### 3.3 Instruction Set

The VM implements **56 opcodes** (6-bit opcode field supports up to 64):

#### General (5)

| Opcode | Description |
|--------|-------------|
| `RET0` | Return null |
| `HALT` | Stop VM execution |
| `NOP` | No operation |
| `RET` | Return value from register |
| `CALL` | Call function/closure |

#### Load/Store (13)

| Opcode | Semantics |
|--------|-----------|
| `LOAD` | `R(A) = R(B)[R(C)]` — property access |
| `LOADAT` | `R(A) = R(B)[R(C)]` — subscript access |
| `LOADS` | Super property access |
| `LOADK` | `R(A) = K(Bx)` — load constant from pool |
| `LOADG` | `R(A) = G[K(Bx)]` — load global |
| `LOADI` | `R(A) = N` — load inline integer |
| `LOADU` | `R(A) = U(B)` — load upvalue |
| `MOVE` | `R(A) = R(B)` — register copy |
| `STORE` | `R(B)[R(C)] = R(A)` — property write |
| `STOREAT` | `R(B)[R(C)] = R(A)` — subscript write |
| `STOREG` | `G[K(Bx)] = R(A)` — store global |
| `STOREU` | `U(B) = R(A)` — store upvalue |

#### Jump (2)

| Opcode | Semantics |
|--------|-----------|
| `JUMP` | Unconditional jump (26-bit signed offset) |
| `JUMPF` | Jump if false (18-bit signed offset) |

#### Arithmetic & Logic (19)

| Opcode | Operation |
|--------|-----------|
| `ADD`, `SUB`, `MUL`, `DIV`, `REM` | Arithmetic |
| `AND`, `OR` | Logical and/or |
| `LT`, `GT`, `LEQ`, `GEQ` | Ordered comparison |
| `EQ`, `NEQ` | Equality |
| `EQQ`, `NEQQ` | Strict equality (identity) |
| `ISA` | Instance-of check |
| `MATCH` | Pattern match (`~=`) |
| `NEG`, `NOT` | Unary negation/logical not |

#### Bitwise (6)

| Opcode | Operation |
|--------|-----------|
| `LSHIFT`, `RSHIFT` | Bit shifts |
| `BAND`, `BOR`, `BXOR` | Bitwise and/or/xor |
| `BNOT` | Bitwise complement |

#### Collections (4)

| Opcode | Semantics |
|--------|-----------|
| `MAPNEW` | `R(A) = new Map(B)` |
| `LISTNEW` | `R(A) = new List(B)` |
| `RANGENEW` | `R(A) = new Range(B, C, flag)` |
| `SETLIST` | Populate list/map from register range |

#### Closures (2)

| Opcode | Semantics |
|--------|-----------|
| `CLOSURE` | Create closure from function constant |
| `CLOSE` | Close open upvalues at register level |

#### Special (1)

| Opcode | Semantics |
|--------|-----------|
| `CHECK` | Clone struct value (enforces value semantics) |

#### Operator Vtable

Each class defines operator methods via a vtable indexed by `GRAVITY_VTABLE_INDEX`:

```c
typedef enum {
    GRAVITY_ADD_INDEX,    // "+"
    GRAVITY_SUB_INDEX,    // "-"
    GRAVITY_MUL_INDEX,    // "*"
    GRAVITY_DIV_INDEX,    // "/"
    // ... one for each overloadable operator
    GRAVITY_EXEC_INDEX    // "()" — call
} GRAVITY_VTABLE_INDEX;
```

---

### 3.4 Instruction Encoding

All instructions are 32 bits wide with varying field layouts:

```
Standard 3-operand:   [ opcode:6 ][ A:8 ][ B:8 ][ C:10 ]
Immediate (LOADI):    [ opcode:6 ][ A:8 ][ sign:1 ][ N:17 ]
Jump (JUMP):          [ opcode:6 ][ N:26 ]
```

Operand extraction uses bit shifts and masks:

```c
#define OPCODE_GET_OPCODE(v)                ((v >> 26) & 0x3F)
#define OPCODE_GET_THREE8bit(v, A, B, C)    A = (v >> 18) & 0xFF; \
                                            B = (v >> 10) & 0xFF; \
                                            C = v & 0x3FF;
#define OPCODE_GET_ONE8bit_ONE18bit(v, A, N) A = (v >> 18) & 0xFF; \
                                             N = v & 0x3FFFF;
```

---

### 3.5 Stack and Call Frames

#### Call Frame

```c
typedef struct {
    uint32_t           *ip;          // instruction pointer
    uint32_t            dest;        // destination register for return value
    uint16_t            nargs;       // actual argument count
    gravity_list_t     *args;        // implicit _args array (if needed)
    gravity_closure_t  *closure;     // closure being executed
    gravity_value_t    *stackstart;  // first stack slot of this frame
    bool                outloop;     // set when called from gravity_vm_runclosure
} gravity_callframe_t;
```

#### Stack Layout Per Frame

```
stackstart[0]          = self (implicit first parameter)
stackstart[1..n]       = explicit parameters
stackstart[n+1..m]     = local variables
stackstart[m+1..p]     = temporary values
```

#### Sliding Register Window

When a `CALL` instruction executes, the register window for the callee starts at `r2+1` (where `r2` is the callable register). This sliding window design minimizes value copying between frames:

```
Caller:     [ ... | self | arg1 | arg2 | ... ]
                    ↑
                    rwin = r2 + 1 → callee's stackstart
Callee:     [ self | arg1 | arg2 | locals... | temps... ]
```

The stack grows on demand (power-of-2 reallocation). When the stack is reallocated, all frame pointers are adjusted to maintain consistency. The stack never shrinks.

---

### 3.6 Fiber (Coroutine) Model

**Fibers** are Gravity's concurrency primitive. Each fiber has its own stack and call frame array, enabling cooperative multitasking.

```c
typedef struct {
    gravity_class_t    *isa;
    gravity_gc_t        gc;

    // Stack
    gravity_value_t    *stack;        // value stack buffer
    gravity_value_t    *stacktop;     // current stack pointer
    uint32_t            stackalloc;   // allocated capacity

    // Call frames
    gravity_callframe_t *frames;      // frame buffer
    uint32_t             nframes;     // frames in use
    uint32_t             framesalloc; // allocated capacity

    // Closures
    gravity_upvalue_t   *upvalues;    // open upvalue linked list

    // Status
    gravity_fiber_status status;      // NEVER_EXECUTED, RUNNING, ABORTED, TERMINATED, TRYING
    char                *error;       // error message
    bool                 trying;      // inside try block
    gravity_fiber_t     *caller;      // parent fiber
    gravity_value_t      result;      // final result

    // Timing (for yield with timeout)
    nanotime_t           lasttime;
    gravity_float_t      timewait;
    gravity_float_t      elapsedtime;
} gravity_fiber_t;
```

Fiber status values: `FIBER_NEVER_EXECUTED`, `FIBER_RUNNING`, `FIBER_ABORTED_WITH_ERROR`, `FIBER_TERMINATED`, `FIBER_TRYING`.

---

### 3.7 Execution Flow

#### `gravity_vm_exec` — Main Bytecode Loop

```c
bool gravity_vm_exec(gravity_vm *vm) {
    DECLARE_DISPATCH_TABLE;
    // Load fiber, frame, function, stackstart, ip, bytecode ...

    while (1) {
        INTERPRET_LOOP {
            CASE_CODE(ADD): {
                // 1. Decode operands
                // 2. Check fast path (inline int/float arithmetic)
                // 3. Fallback: look up "+" method on r2's class
                // 4. Call method, store result
                DISPATCH();
            }
            CASE_CODE(CALL): {
                // 1. Decode: r1=dest, r2=callable, r3=nargs
                // 2. Compute register window: rwin = r2 + 1
                // 3. Resolve closure (directly or via "exec" method)
                // 4. Push frame, fill defaults for missing args
                // 5. Dispatch by type:
                //    - NATIVE: PUSH_FRAME, continue loop
                //    - INTERNAL: call C function directly
                //    - BRIDGED: call delegate->bridge_execute
                DISPATCH();
            }
            CASE_CODE(RET): {
                // 1. Pop frame
                // 2. Close open upvalues
                // 3. If outloop flag → return to gravity_vm_runclosure
                // 4. Else → continue with caller frame
                DISPATCH();
            }
            // ... 53 more opcodes
        }
    }
}
```

#### `gravity_vm_runclosure` — External Entry Point

Called from the embedding API or internally to invoke a specific closure:

1. Validate VM is not aborted.
2. Set up stack window and parameters.
3. Dispatch by function type:
   - **Native:** Increment `nccalls`, call `gravity_vm_exec()`, decrement.
   - **Internal:** Call C function pointer directly.
   - **Bridged:** Call delegate `bridge_execute` callback.
4. Restore frame pointers and adjust stack top.

---

### 3.8 Fast-Path Optimizations

- **Inline arithmetic:** When both operands are `Int` or `Float`, arithmetic is computed directly without method lookup.
- **Jump fusion:** Compare instructions (e.g., `EQ`, `LT`) peek ahead for a following `JUMPF`. If found, the compare and jump are fused into a single operation.
- **Register window:** The sliding register window avoids copying arguments between caller and callee.
- **Computed goto:** O(1) instruction dispatch with no branch prediction overhead.
- **Pre-allocated frames:** Call frames and stack space are pre-allocated and reused.

---

## 4. Value System and Type Hierarchy

### 4.1 Value Representation

**Files:** `src/shared/gravity_value.h`, `src/shared/gravity_value.c`

Gravity uses a **16-byte tagged union** for all values (not NaN-boxing):

```c
typedef struct {
    gravity_class_t *isa;       // 8 bytes: type tag (pointer to class)
    union {                     // 8 bytes: payload
        gravity_int_t    n;     //   integer value
        gravity_float_t  f;     //   float/double value
        gravity_object_t *p;    //   pointer to heap object
    };
} gravity_value_t;
```

The `isa` pointer serves double duty: it identifies the type and provides the method lookup table. Special sentinel values:
- **Null:** `isa = NULL`, `n = 0`
- **Undefined:** `isa = NULL`, `n = 1`

**Unboxed types** (value stored directly in the union): `Bool`, `Int`, `Float`, `Null`, `Undefined`.

**Boxed types** (pointer to heap-allocated object): `String`, `List`, `Map`, `Class`, `Instance`, `Closure`, `Function`, `Range`, `Fiber`, `Upvalue`.

---

### 4.2 Object Header and GC Metadata

All heap-allocated objects share a common header:

```c
typedef struct gravity_object_s {
    gravity_class_t *isa;       // class pointer (method dispatch)
    gravity_gc_t     gc;        // GC metadata
} gravity_object_t;

typedef struct {
    bool                isdark;   // marked during GC
    bool                visited;  // prevents double-counting in size calc
    gravity_object_t   *next;     // intrusive linked list (GC object chain)
    gravity_gc_callback free;     // destructor callback
    gravity_gc_callback size;     // size reporting callback
    gravity_gc_callback blacken;  // mark-children callback
} gravity_gc_t;
```

Every heap object is linked into the VM's GC chain via `gc.next`. The three callbacks (`free`, `size`, `blacken`) implement type-specific GC behavior without virtual dispatch overhead.

---

### 4.3 Built-in Types

The runtime registers these built-in classes (in `gravity_core.c`):

| Class | Behavior |
|-------|----------|
| `gravity_class_int` | 64-bit integer, arithmetic operators, bitwise ops |
| `gravity_class_float` | IEEE 754 double, arithmetic operators |
| `gravity_class_bool` | Boolean, logical operators |
| `gravity_class_null` | Null singleton |
| `gravity_class_string` | Immutable UTF-8 string, concatenation, methods |
| `gravity_class_object` | Base class (all types inherit from this) |
| `gravity_class_function` | Function prototype |
| `gravity_class_closure` | Closure (function + captured environment) |
| `gravity_class_fiber` | Fiber (coroutine) |
| `gravity_class_class` | Metaclass |
| `gravity_class_instance` | User-defined class instance |
| `gravity_class_list` | Dynamic array |
| `gravity_class_map` | Hash map |
| `gravity_class_range` | Integer range (inclusive or exclusive) |
| `gravity_class_upvalue` | Captured variable reference |

Each class binds operator methods and instance methods. For example, `gravity_class_int` binds `"+"`, `"-"`, `"*"`, etc. as well as methods like `loop()`, `random()`, and conversion operators.

---

### 4.4 Functions and Closures

#### Function Prototype

```c
typedef struct {
    gravity_class_t  *isa;
    gravity_gc_t      gc;

    const char       *identifier;     // function name
    uint16_t          nparams;        // formal parameters (including self)
    uint16_t          nlocals;        // local variables
    uint16_t          ntemps;         // temporary registers
    uint16_t          nupvalues;      // captured variables
    gravity_exec_type tag;            // execution type

    union {
        // EXEC_TYPE_NATIVE (compiled Gravity code):
        struct {
            gravity_value_r  cpool;     // constant pool
            gravity_value_r  pvalue;    // default parameter values
            gravity_value_r  pname;     // parameter names
            uint32_t         ninsts;    // instruction count
            uint32_t        *bytecode;  // packed 32-bit instructions
            uint32_t        *lineno;    // line number mapping (debug)
            bool             useargs;   // needs implicit _args array
        };

        // EXEC_TYPE_INTERNAL (C callback):
        gravity_c_internal   internal;  // bool (*)(vm, args, nargs, rindex)

        // EXEC_TYPE_SPECIAL (computed property):
        struct {
            uint16_t  index;           // property index
            void     *special[2];      // [0]=getter, [1]=setter
        };
    };
} gravity_function_t;
```

Execution types:
- `EXEC_TYPE_NATIVE` — compiled Gravity bytecode.
- `EXEC_TYPE_INTERNAL` — C function callback with signature `bool (*)(gravity_vm*, gravity_value_t*, uint16_t, uint32_t)`.
- `EXEC_TYPE_BRIDGED` — external bridge, executed via delegate callback.
- `EXEC_TYPE_SPECIAL` — getter/setter computed property.

#### Closure

```c
typedef struct {
    gravity_class_t    *isa;
    gravity_gc_t        gc;
    gravity_vm         *vm;           // owning VM
    gravity_function_t *f;            // function prototype (shared)
    gravity_object_t   *context;      // captured self reference
    gravity_upvalue_t **upvalue;      // captured upvalue array
    uint32_t            refcount;     // bridge reference counting
} gravity_closure_t;
```

Multiple closures can share the same function prototype while having different captured environments.

---

### 4.5 Upvalues

Upvalues implement Lua-style **open/closed** variable capture:

```c
typedef struct upvalue_s {
    gravity_class_t  *isa;
    gravity_gc_t      gc;
    gravity_value_t  *value;     // points to stack slot (open) or self->closed (closed)
    gravity_value_t   closed;    // storage when variable leaves scope
    struct upvalue_s *next;      // linked list (ordered by stack position)
} gravity_upvalue_t;
```

- **Open upvalue:** `value` points to a live stack slot. The fiber maintains a linked list of open upvalues ordered by descending stack address.
- **Closed upvalue:** When the enclosing function returns, the captured value is copied from the stack into `closed`, and `value` is repointed to `&self->closed`.

The `CLOSE` instruction walks the open upvalue list and closes any upvalues at or above a given register level.

---

### 4.6 Classes and Instances

#### Class

```c
typedef struct {
    gravity_class_t  *isa;           // metaclass
    gravity_gc_t      gc;

    gravity_class_t  *objclass;      // metaclass reference
    const char       *identifier;    // class name
    bool              has_outer;     // has outer class ivar
    bool              is_struct;     // value semantics (copy on assignment)
    bool              is_inited;     // metaclass initialized
    void             *xdata;         // bridge extension data

    gravity_class_t  *superclass;    // parent class
    const char       *superlook;     // extern superclass name (lazy binding)
    gravity_hash_t   *htable;        // method/property hash table

    uint32_t          nivars;        // instance variable count
    gravity_value_r   inames;        // ivar names (debug)
    gravity_value_t  *ivars;         // static (class) variables
} gravity_class_t;
```

Method resolution traverses the superclass chain. Methods and computed properties are stored in the class hash table.

#### Instance

```c
typedef struct {
    gravity_class_t  *isa;
    gravity_gc_t      gc;
    gravity_class_t  *objclass;      // actual class
    void             *xdata;         // bridge extension data
    gravity_value_t  *ivars;         // instance variable array (indexed by position)
} gravity_instance_t;
```

Instance variables are stored in a flat array indexed by position (set during semacheck1), providing O(1) access.

---

## 5. Garbage Collector

**Location:** `src/runtime/gravity_vm.c`

Gravity uses a **tri-color mark-and-sweep** garbage collector.

### Mark Phase

1. Mark temporary protected objects (in `vm->gctemp`).
2. Mark the current fiber as a root.
3. Mark all globals in the context hash table.
4. Process the gray list: for each gray object, call its `blacken` callback to mark all referenced objects.
5. Repeat until the gray list is empty.

### Sweep Phase

1. Walk the `vm->gchead` linked list.
2. For each object **not** marked (`!isdark`): call its `free` callback and remove it from the chain.
3. For each marked object: clear the `isdark` flag for the next cycle.

### GC Triggers

- **Automatic:** When `memallocated >= gcthreshold` during `gravity_gc_transfer` (object allocation).
- **Manual:** `gravity_gc_start(vm)`.
- **Stress test:** Every allocation (when compiled with `GRAVITY_GC_STRESSTEST`).

### Dynamic Threshold Adjustment

After each collection:
```
new_threshold = memallocated + (memallocated * gcratio / 100)
if (new_threshold < minthreshold) new_threshold = minthreshold
if (new_threshold < original)     new_threshold = original
```

Default values: `gcthreshold = 5MB`, `gcminthreshold = 1MB`, `gcratio = 0.5 (50%)`.

### GC-Safe Coding Pattern

The enable flag is reference-counted, allowing nested disable/enable calls:

```c
gravity_gc_setenabled(vm, false);  // disable GC (increments counter)
// ... allocate objects safely ...
gravity_gc_setenabled(vm, true);   // re-enable (decrements counter)
```

Temporary objects can be protected from collection:

```c
gravity_gc_temppush(vm, object);   // protect
// ... use object ...
gravity_gc_temppop(vm);            // unprotect
```

---

## 6. Core Data Structures

### 6.1 Hash Table

**Files:** `src/shared/gravity_hash.c`, `src/shared/gravity_hash.h`

A chained hash table used for symbol tables, class method lookup, global variables, and the `Map` type.

```c
typedef struct hash_node_s {
    uint32_t             hash;    // cached hash value
    gravity_value_t      key;
    gravity_value_t      value;
    struct hash_node_s  *next;    // collision chain
} hash_node_t;

struct gravity_hash_t {
    uint32_t              size;        // bucket count
    uint32_t              count;       // entry count
    hash_node_t         **nodes;       // bucket array
    gravity_hash_compute_fn  compute_fn;  // hash function
    gravity_hash_isequal_fn  isequal_fn;  // equality function
    gravity_hash_iterate_fn  free_fn;     // entry cleanup callback
    void                    *data;        // callback context
};
```

| Property | Value |
|----------|-------|
| Hash function | Murmur3-32 (seed 5381) |
| Collision resolution | Chaining (linked list per bucket) |
| Load factor | 0.75 |
| Growth strategy | Double bucket count on resize |
| Initial size | 32 buckets |
| Max entries | 2^30 |

Hash function variants: `gravity_hash_compute_buffer()` for strings, `gravity_hash_compute_int()` for integers, `gravity_hash_compute_float()` for floats.

---

### 6.2 Dynamic Array

**File:** `src/shared/gravity_array.h`

A macro-based generic dynamic array:

```c
#define marray_t(type)  struct { size_t n, m; type *p; }
//                               count  capacity  data
```

| Macro | Purpose |
|-------|---------|
| `marray_init(v)` | Initialize to zero |
| `marray_push(T, v, x)` | Append (doubles capacity if needed) |
| `marray_pop(v)` | Remove and return last element |
| `marray_get(v, i)` | Access element by index |
| `marray_size(v)` | Current element count |
| `marray_max(v)` | Current capacity |
| `marray_resize(T, v, n)` | Extend capacity to at least `n` |
| `marray_destroy(v)` | Free backing memory |

Growth strategy: double capacity on each reallocation.

---

### 6.3 Memory Management

**Files:** `src/shared/gravity_memory.h`, `src/shared/gravity_memory.c`

Production mode provides thin wrappers around `malloc`/`realloc`/`free` with max block size enforcement (`MAX_MEMORY_BLOCK = 150MB`).

Debug mode (`GRAVITY_MEMORY_DEBUG`) adds:
- Tracking of every allocation with call stack.
- Detection of double-free and use-after-free.
- Leak reporting on shutdown.

All allocations go through `mem_alloc()`, which integrates with the VM's `memallocated` counter for GC threshold tracking.

---

## 7. Optional Modules

### 7.1 Registration Pattern

**File:** `src/optionals/gravity_optionals.h`

Each optional module follows the same pattern:

1. Compile-time guard (`#ifndef GRAVITY_INCLUDE_MATH` / `#define GRAVITY_INCLUDE_MATH`).
2. Macro wrappers that become no-ops when disabled.
3. Singleton class with reference counting.
4. Static methods bound to the metaclass.
5. Registration: `gravity_vm_setvalue(vm, name, class)`.

```c
// Typical module lifecycle:
static gravity_class_t *gravity_class_math = NULL;
static uint32_t refcount = 0;

void gravity_math_register(gravity_vm *vm) {
    if (!gravity_class_math) create_optional_class();
    ++refcount;
    gravity_vm_setvalue(vm, "Math", VALUE_FROM_OBJECT(gravity_class_math));
}

void gravity_math_free(void) {
    if (--refcount) return;   // wait for all VMs to unregister
    // destroy class ...
}
```

Computed properties (read-only constants) use a getter-only closure:

```c
gravity_closure_t *closure = computed_property_create(NULL, NEW_FUNCTION(getter), NULL);
gravity_class_bind(meta, "PI", VALUE_FROM_OBJECT(closure));
```

---

### 7.2 Math Module

**File:** `src/optionals/gravity_opt_math.c` — Class name: `"Math"`

**Methods (23):**

| Category | Functions |
|----------|-----------|
| Trigonometric | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` |
| Rounding | `ceil`, `floor`, `round` (with optional precision) |
| Logarithmic | `log`, `log10`, `logx` (custom base) |
| Algebraic | `abs`, `sqrt`, `cbrt`, `xrt` (nth root), `pow`, `exp` |
| Combinatorial | `gcf`, `lcm` |
| Interpolation | `lerp` |
| Comparison | `min`, `max` (variadic) |
| Random | `random()`, `random(max)`, `random(min, max)` |

**Constants (8):** `PI`, `E`, `LN2`, `LN10`, `LOG2E`, `LOG10E`, `SQRT2`, `SQRT1_2`.

Random number generator: LFSR258 (64-bit) or LFSR113 (32-bit), seeded with `nanotime()` on first call.

---

### 7.3 File Module

**File:** `src/optionals/gravity_opt_file.c` — Class name: `"File"`

Uses a custom `gravity_file_t` struct wrapping a `FILE*` pointer, with GC integration for automatic cleanup.

**Class (static) methods:** `size`, `exists`, `delete`, `read`, `write`, `buildpath`, `is_directory`, `directory_create`, `directory_scan`.

**Instance methods:** `open` (factory), `read`, `write`, `seek`, `eof`, `error`, `flush`, `close`.

The `directory_scan` method accepts a closure callback invoked for each entry with `(filename, fullpath, isdir)`.

---

### 7.4 JSON Module

**File:** `src/optionals/gravity_opt_json.c` — Class name: `"JSON"`

Two static methods:
- `stringify(value)` — Serialize any Gravity value to a JSON string. Handles nested structures, escapes special characters, uses heap allocation for strings >4KB.
- `parse(jsonString)` — Deserialize a JSON string into nested Gravity lists and maps. Returns `null` for invalid JSON.

---

### 7.5 ENV Module

**File:** `src/optionals/gravity_opt_env.c` — Class name: `"ENV"`

**Methods:** `get(key)`, `set(key, value)`, `keys()`.

**Properties:** `argc` (read-only), `argv` (read-only list).

Supports map-access syntax: `ENV["PATH"]` via overloaded load/store-at handlers. Cross-platform: uses `_putenv_s` on Windows, `setenv` on Unix.

---

## 8. Embedding API

### 8.1 Compiler API

**File:** `src/compiler/gravity_compiler.h`

```c
gravity_compiler_t  *gravity_compiler_create(gravity_delegate_t *delegate);
gravity_closure_t   *gravity_compiler_run(compiler, source, len, fileid, is_static, add_debug);
gnode_t             *gravity_compiler_ast(compiler);
void                 gravity_compiler_transfer(compiler, vm);  // move objects to VM's GC
void                 gravity_compiler_free(compiler);
```

Serialization for ahead-of-time compilation:
```c
json_t *gravity_compiler_serialize(compiler, closure);
bool    gravity_compiler_serialize_infile(compiler, closure, path);
```

---

### 8.2 VM API

**File:** `src/runtime/gravity_vm.h`

```c
// Lifecycle
gravity_vm          *gravity_vm_new(gravity_delegate_t *delegate);
gravity_vm          *gravity_vm_newmini(void);        // lightweight (no optionals)
void                 gravity_vm_free(vm);
void                 gravity_vm_reset(vm);

// Execution
bool                 gravity_vm_runmain(vm, closure);
bool                 gravity_vm_runclosure(vm, closure, sender, params, nparams);
gravity_value_t      gravity_vm_result(vm);

// Globals
void                 gravity_vm_setvalue(vm, key, value);
gravity_value_t      gravity_vm_getvalue(vm, key, keylen);
gravity_value_t      gravity_vm_lookup(vm, key);

// Memory & GC
void                 gravity_vm_transfer(vm, object);
void                 gravity_gc_start(vm);
void                 gravity_gc_setenabled(vm, enabled);
void                 gravity_gc_setvalues(vm, threshold, minthreshold, ratio);

// Bytecode loading
gravity_closure_t   *gravity_vm_loadfile(vm, path);
gravity_closure_t   *gravity_vm_loadbuffer(vm, buffer, len);

// Optional modules
void                 gravity_opt_register(vm);
void                 gravity_opt_free(void);
```

---

### 8.3 Delegate Pattern

**File:** `src/shared/gravity_delegate.h`

The delegate is a struct of function pointers used for all communication between the compiler/VM and the host application:

```c
typedef struct {
    // Error handling
    gravity_error_callback      error_callback;       // syntax, semantic, runtime errors

    // Compiler hooks
    gravity_loadfile_callback   loadfile_callback;    // resolve import paths
    gravity_filename_callback   filename_callback;    // map fileid → filename
    gravity_precode_callback    precode_callback;     // inject code at parse time
    gravity_parser_callback     parser_callback;      // syntax highlighting hook
    gravity_type_callback       type_callback;        // bind type annotations

    // Logging
    gravity_log_callback        log_callback;
    gravity_log_clear           log_clear;

    // Bridge (C interop)
    gravity_bridge_initinstance bridge_initinstance;
    gravity_bridge_execute      bridge_execute;
    gravity_bridge_blacken      bridge_blacken;
    gravity_bridge_equals       bridge_equals;
    gravity_bridge_clone        bridge_clone;
    gravity_bridge_size         bridge_size;
    gravity_bridge_free         bridge_free;
    gravity_bridge_getvalue     bridge_getvalue;
    gravity_bridge_setvalue     bridge_setvalue;

    // Testing
    gravity_unittest_callback   unittest_callback;
} gravity_delegate_t;
```

Error types: `GRAVITY_ERROR_SYNTAX`, `GRAVITY_ERROR_SEMANTIC`, `GRAVITY_ERROR_RUNTIME`, `GRAVITY_ERROR_IO`, `GRAVITY_WARNING`.

---

### 8.4 Bridging

Gravity supports binding external (C, Objective-C, Swift) objects through the bridge delegate callbacks:

- `EXEC_TYPE_BRIDGED` functions are dispatched via `delegate->bridge_execute`.
- Instance creation goes through `delegate->bridge_initinstance`.
- Property access uses `bridge_getvalue` / `bridge_setvalue`.
- Objects store host-side data in the `xdata` pointer present on classes, instances, and functions.

#### Typical Embedding Usage

```c
// 1. Create compiler
gravity_delegate_t delegate = {.error_callback = report_error};
gravity_compiler_t *compiler = gravity_compiler_create(&delegate);

// 2. Compile
gravity_closure_t *closure = gravity_compiler_run(
    compiler, source, strlen(source), 0, true, true);

// 3. Create VM and transfer ownership
gravity_vm *vm = gravity_vm_new(&delegate);
gravity_compiler_transfer(compiler, vm);
gravity_compiler_free(compiler);

// 4. Execute
if (gravity_vm_runmain(vm, closure)) {
    gravity_value_t result = gravity_vm_result(vm);
    // ... use result ...
}

// 5. Cleanup
gravity_vm_free(vm);
gravity_core_free();
```

---

## 9. Utilities

### Debug / Disassembler

**Files:** `src/utils/gravity_debug.c`, `src/utils/gravity_debug.h`

- `opcode_name(opcode_t)` — maps opcode enum to mnemonic string.
- `opcode_constname(int)` — maps constant pool indices to names (`SUPER`, `NULL`, `UNDEFINED`, `TRUE`, `FALSE`, etc.).
- `gravity_disassemble()` — full bytecode disassembler; outputs human-readable assembly with line numbers and decoded operands.

### JSON Serialization

**Files:** `src/utils/gravity_json.c`, `src/utils/gravity_json.h`

Two components:
- **Serializer:** `json_t` object with hierarchical `json_add_*()`, `json_begin/end_array()`, `json_begin/end_object()` functions. Used by the compiler to serialize bytecode to JSON.
- **Parser:** Third-party JSON parser (`json_parse()`) that produces a `json_value` tree. Used by `gravity_vm_loadfile` to deserialize compiled bytecode.

### File I/O and Platform Utilities

**Files:** `src/utils/gravity_utils.c`, `src/utils/gravity_utils.h`

- High-resolution timer: `nanotime()` (platform-specific: `mach_absolute_time` on macOS, `clock_gettime` on Linux, `QueryPerformanceCounter` on Windows).
- File operations: `file_read`, `file_write`, `file_exists`, `file_delete`, `file_size`, `file_buildpath`.
- Directory operations: `directory_create`, `directory_init`, `directory_read`, `is_directory`.
- String utilities: `string_dup`, `string_replace`, `string_reverse`.
- UTF-8: `utf8_charbytes`, `utf8_encode`, `utf8_len`, `utf8_nbytes`, `utf8_reverse`.
- Number parsing: `number_from_bin`, `number_from_hex`, `number_from_oct`.

---

## 10. CLI

**File:** `src/cli/gravity.c`

### Operation Modes

| Flag | Mode | Description |
|------|------|-------------|
| *(filename)* | `OP_COMPILE_RUN` | Compile and execute in one pass |
| `-c file` | `OP_COMPILE` | Compile to bytecode file (default: `gravity.json`) |
| `-x file` | `OP_RUN` | Execute precompiled JSON bytecode |
| `-i 'code'` | `OP_INLINE_RUN` | Compile and execute inline string (wrapped in `func main() { ... }`) |
| `-t folder` | `OP_UNITTEST` | Run unit tests recursively |
| `-o file` | — | Specify output filename |
| `-q` | — | Quiet mode (suppress result and timing) |

The CLI sets up a `gravity_delegate_t` with `error_callback` and `loadfile_callback` (for `import` resolution), then drives the compiler and VM through the standard embedding API.

---

## 11. Build System

**File:** `Makefile`

### Targets

| Target | Output |
|--------|--------|
| `make` | `gravity` CLI executable |
| `make mode=debug` | Debug build (`-g -O0 -DDEBUG`) |
| `make lib` | Shared library (`libgravity.dylib` / `.so` / `.dll`) |
| `make example` | C embedding API example |
| `make clean` | Remove all build artifacts |

### Compiler Flags

```
-std=gnu99 -fgnu89-inline -fPIC -DBUILD_GRAVITY_API
-O2                     (release)
-g -O0 -DDEBUG          (debug)
```

### Platform Detection

- **macOS:** `libgravity.dylib`
- **Linux/BSD:** `libgravity.so`, links `-lm`
- **Windows:** `gravity.dll`, links `Shlwapi`

### Dependencies

- C99-compatible compiler
- Standard C library (including `math.h`)
- Platform headers (`dirent.h`, `sys/time.h`, or Windows equivalents)
- No external library dependencies

---

## 12. Test Infrastructure

### Test Format

Unit tests are individual `.gravity` files in `test/unittest/`. Each test declares expected results in a metadata block:

```swift
#unittest {
    name: "Test description";
    result: expected_value;
};

func main() {
    // test logic
    return actual_value;
}
```

The test runner compiles and executes each file, then compares the return value of `main()` against the declared `result`.

### Test Metadata Fields

| Field | Purpose |
|-------|---------|
| `name` | Human-readable test description |
| `result` | Expected return value (compared with `==`) |
| `expected_error` | Expected error type (for negative tests) |
| `expected_row` | Expected error line number |
| `expected_col` | Expected error column number |

### Running Tests

```bash
./gravity -t test/unittest/              # run all tests
./test/unittest/run_all.sh               # run with timeouts (used by CI)
./gravity test/unittest/test_file.gravity # run a single test
```

### Test Organization

Tests are organized by category in subdirectories: compiler phases, language features, built-in types, optional modules, edge cases, and bug regressions. The runner recursively scans the target directory, skips any `/disabled/` subdirectories, and applies fuzzy comparison for tests under `/fuzzy/`.

CI runs: `make && test/unittest/run_all.sh`
