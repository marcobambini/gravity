# Changelog

All notable changes to Gravity are documented in this file.

## [Unreleased]

### Fixed
- **Heap buffer overflow in `list_storeat` when growing the list fails** — storing past the end of a list reallocates the backing array, but `marray_resize` leaves both the pointer and the capacity untouched when the `realloc` fails, so the existing `if (!list->array.p)` guard never fired: the old, smaller buffer is still there and still non-NULL. The count was then set to the requested index and the fill loop wrote well past the end of the allocation. The check now tests the capacity actually obtained, and the out-of-memory case is reported as `Not enough memory to resize List.` as intended. Reachable from a script: `x[4444444444444444444] = 0` asks for a single multi-gigabyte allocation.

### Changed
- The sanitizer CI job caps a single allocation at 1 GB (`max_allocation_size_mb`) so the pathological allocations in `test/fuzzy` fail cleanly instead of pushing the runner into the OOM killer, and pins `abort_on_error` so a sanitizer finding arrives as a signal on every platform rather than as the bare exit code 1 the runtime defaults to on Linux. The fuzzing step also scans the output for sanitizer reports, which the exit code alone does not reliably convey.

---

## [0.9.8] - 2026-08-05

Security and memory-safety release. Every issue below was found by external
reporters fuzzing the compiler and the bytecode loader, and each fix ships with
a regression test.

### Fixed
- **NULL dereference in `gravity_vm_loadbuffer`** — a serialized function object without an `identifier` field, such as `{"x":{"type":"function"}}`, reached `strlen(NULL)` and crashed the process. The loader now validates the structure of every JSON executable before using it: the root and each entry must be objects, the identifier must be present exactly once and be a string, and unknown object types are rejected. Malformed input is reported as a load error instead of crashing (issue #444).
- **Signed 64-bit integer overflow in `json_parse_ex`** — the integer and exponent accumulators multiplied by 10 per digit with no range check, so any literal longer than 19 significant digits overflowed. Signed overflow is undefined behaviour: the parser stored a wrapped value, and builds compiled with `-fsanitize=undefined` trapped with SIGILL. Both accumulators are now range-checked and over-long literals are rejected (issue #447).
- **Pointer-arithmetic overflow in the JSON scan loop** — `for (state.ptr = json; ; ++state.ptr)` incremented unconditionally, so input that ended while the scanner was still inside a string or comment advanced the pointer past one-past-the-end, which is undefined behaviour. The loop now stops at the end of the buffer regardless of scanner state (issue #448).
- **Heap out-of-bounds read in `parse_number_expression`** — the `0x`/`0b`/`0o` prefix check read `value[1]` without confirming the token was at least two bytes, so a source file whose last token was a bare `0` read one byte past the buffer. The prefix is only inspected when the token is long enough (issue #446).
- **Compiler crash (SIGFPE) folding a floating-point remainder** — `optimize_const_instruction` folded `%` by truncating both operands to `int64_t`, so any divisor with `0 < |divisor| < 1` became an integer division by zero and killed the compiler on `1 % 0.5`. Float remainder is now folded with `remainder()`, matching `operator_float_rem`, and mixed Int/Float remainders are left to the runtime because REM dispatches on the class of the left operand. This also fixes a silent wrong answer: `5.5 % 2.0` folded to `1` where the VM evaluates `-0.5` (issue #443).
- **Undefined behaviour in Int arithmetic** — Gravity Ints wrap on overflow, but the wrap was performed on signed operands in the VM fast path, in the `operator_int_*` methods and in the constant folder, which is undefined in C and traps under `-fsanitize=undefined`. All three paths now go through new `GRAVITY_INT_ADD/SUB/MUL/NEG/DIV/REM` helpers that compute on the unsigned counterpart. The helpers also handle `GRAVITY_INT_MIN op -1`, which on x86 faults in `idiv` rather than merely wrapping (issue #443).
- **Optional classes never released** — `gravity_core_free` decremented the refcount of the optional classes without the matching balance, so `Math`, `File`, `JSON` and `ENV` were leaked by every embedder that created and destroyed a VM (issue #442).
- **Core reference leaked by every `gravity_compiler_run`** — the compiler took a reference to the core classes on each run and never released it, so the count never returned to zero and the core was never torn down (issue #442).
- **Double free of the inline source buffer** — `gravity -i` passed its heap-allocated wrapper source to `gravity_compiler_run` with `is_static` false, which hands the buffer to the lexer; the lexer freed it in `parser_run` and the CLI freed the same pointer again on the way out, aborting every inline run under a hardened allocator.

### Added
- `test/loadbuffer/` — a suite of malformed JSON executables that must each be rejected as a load error without crashing, plus `json_bounds.c` (`make jsontest`), 60 bounds checks that drive the JSON scanner directly. Run with `test/loadbuffer/run_all.sh`.
- A GitHub Actions workflow building with gcc and clang on Linux and macOS, and a second job that builds with `-fsanitize=address,undefined` and runs the unit tests, the fuzzing corpus and the loader tests through it.

### Changed
- Version bumped to **0.9.8** (`GRAVITY_VERSION`, `GRAVITY_VERSION_NUMBER`).
- The usage text now prints the real default output file name, `gravity.g`; `README.md` and `CLAUDE.md` documented a stale `gravity.json`.

---

## [0.9.7] - 2026-04-14

### Fixed
- **Float precision loss in JSON bytecode serialization** — float constants were written with `%f` (6 decimal places), silently rounding small values like `-0.000000004` to zero and causing `RUNTIME ERROR: Unknown LOADK index` on the `-c`/`-x` (compile + execute bytecode) path. Switched to `%.17g` for full IEEE 754 double round-trip precision (issue #420).
- **Float constant deduplication in cpool** — `gravity_function_cpool_add` used the epsilon-based `gravity_value_equals` (EPSILON = 1e-6) to detect duplicate constants, incorrectly merging distinct small floats into a single pool entry. The cpool now uses exact bit-level comparison for float values (issue #420).
- **`gravity_optionals.h` unconditionally defined all optional-module guards** — the `#ifndef GRAVITY_INCLUDE_*` blocks always defined every guard, making it impossible to exclude modules at compile time. The guards are now left undefined by default; embedders define only the modules they need. The Gravity CLI and runtime define all four (issue #426).
- **Makefile dependency errors** — four issues: `gravity` and `example` were incorrectly listed as `.PHONY` targets (causing unconditional rebuilds); `lib` depended on the `gravity` executable instead of just `$(OBJ)`; `gravity.c` and `example.c` were compiled only during the link step so `-MMD` never generated `.d` header-dependency files for them; `make clean` did not remove `libgravity.dylib` on macOS (issue #413).
- **`run_all.sh` portability** — the test runner used GNU `timeout` which is not available on macOS. The script now auto-detects `timeout`, `gtimeout` (Homebrew coreutils), or falls back to a pure-bash kill-watcher.

### Changed
- Version bumped to **0.9.7** (`GRAVITY_VERSION`, `GRAVITY_VERSION_NUMBER`).

---

## [0.9.6] - 2026-04-14

### Fixed
- **Stack overflow now produces a clean runtime error** instead of a hard crash (SIGSEGV). Infinite recursion and pathological call depths are caught by a configurable stack size limit before the process runs out of memory.
- **Class `$init` chain infinite recursion** — parent-class `$init` helpers (`$init2`, `$init3`, …) previously used a dynamic name lookup against `self`, which resolved to the wrong (overriding) function when called from a subclass, producing infinite recursion. The compiler now emits a direct static closure reference (`LOADK`) so dispatch is always to the correct ancestor function.
- **Fiber stack growth in `gravity_fiber_reassign`** — a large register-window allocation in `$moduleinit` could cause the initial stack pointer to overshoot `DEFAULT_MINSTACK_SIZE` (256 slots), leaving `stacktop` pointing into unallocated memory (issue #437).
- **`gravity_opt_free` double-free** — optional module cleanup now checks the reference count before freeing (PR #436).

### Added
- `GRAVITY_VM_MAXSTACK` — runtime-configurable maximum fiber stack size (default 1 048 576 slots / 16 MB). Readable and writable via `gravity_vm_get` / `gravity_vm_set` with key `"maxStack"`.
- Re-enabled two previously disabled tests (`heap.gravity`, `loop1.gravity`) — both now pass with the new OOM error reporting.

### Changed
- Version bumped to **0.9.6** (`GRAVITY_VERSION`, `GRAVITY_VERSION_NUMBER`).

---

## [0.9.5] - 2024

### Fixed
- Numerous memory leaks and use-after-free errors throughout the compiler and runtime.
- Memory safety improvements across GC, value handling, and object lifecycle.
- Clang build compatibility (PR #435).

### Changed
- Documentation updates: ARCHITECTURE.md rewritten; README refreshed.

---

## [0.9.0] - 2023

### Fixed
- Several memory leaks plugged across the compiler pipeline.
- Missing Makefile dependencies (PR #431).
- `File.read()` now returns `null` when zero characters are read.
- Replaced unsafe `printf` calls with `snprintf`.
- Fixed issue #394.

### Added
- New unit test for leak-related regression coverage.

---

## [0.8.5] - 2022

### Fixed
- Setter issue that affected several unit tests.
- File read size mismatch due to line-ending differences on Windows (PR #365).
- Compilation failure introduced by `O_BINARY` on non-Windows platforms.

### Added
- Unit tests integrated into CI (PR #378).
- BSD shared-object build support; removed `WITH_GETLINE` (PR #375).

---

## [0.8.4] - 2022

### Fixed
- Issue #379.
- Hash table header organisation (PR #369).

---

## [0.8.3] - 2021

### Fixed
- Regression introduced by lazy-loading of superclasses in 0.8.2.

---

## [0.8.2] - 2021

### Added
- Lazy loading of extern superclasses at runtime.
- Preliminary support for instance `deinit` (destructor).
- `System.input()` (PR #342).
- `ENV.argc` / `ENV.argv` properties (PR #343).
- ObjC binding example.
- C++ binding example.
- Ternary expression and `switch` statement codegen (PR #336).
- Optional `File` class (cross-platform).
- `gravity_instance_lookup_real_property` helper.
- `gravity_config.h` for platform-specific configuration (PR #301).
- Improved CMake: supports CLI, shared lib, and static lib targets (PR #299).

### Fixed
- Inner class constructor returning wrong instance.
- Computed property (setter) bug.
- Sign-conversion and char-type warnings flagged by sanitizers.
- `stat` return-value check.
- `uint32_t`-to-`char` conversion in `utf8_encode`.
- `size_t` comparison against negative value in `file_read`.
- Various Windows / MSVC compatibility fixes.
- Implicit `long`-to-`double` conversion warning under Clang.
- Emscripten include-guard fix.

### Changed
- Improved superclass type checking in the semantic analyser.
- Improved error handling and detection (0.8.0).
- `File.eof` renamed from `isEOF`.

---

## [0.7.9] - 2020

### Fixed
- Improved error handling in the VM and runtime.

### Added
- `vm` back-reference stored on `gravity_closure_t`.
- Computed-goto support for Clang on Windows.
- `DISPATCH_INNER` macro for `do/while(0)` loops without computed goto.
- `xdata` parameter on `delegate->optional_classes` (PR #307).

---

## [0.7.8] - 2020

### Added
- Preliminary `Struct` support.
- `bind` method fix; unit test added.

### Fixed
- Possible GC issue (unit test added).

---

## [0.7.7] - 2020

### Fixed
- `super` keyword resolution issue; unit test added.

---

## [0.7.6] - 2020

### Changed
- Optional classes renamed for consistency.

### Fixed
- Setter unwanted side effect.

---

## [0.7.5] - 2020

### Improved
- `float`/`double` to `String` conversion accuracy.
- Various core methods.

---

## [0.7.4] - 2020

### Fixed
- `String.length` is now UTF-8 aware; `String.bytes` added (unit test added).
- Function returning address of local variable on Windows (`directory_read`).
- Division-by-zero warning suppression in GCC.
- Const output-buffer issue on Windows (`WideCharToMultiByte`).

### Added
- More BSD targets in `make` and CMake.

---

## [0.7.0] - 2019

### Added
- `String.split` and string iteration are now Unicode-aware; unit test added.
- Support for local `enum` declarations; unit test added.

### Fixed
- Local class declarations.
- Superclass resolution edge cases.
- `continue` keyword inside `for` loops.
- `self` parameter in complex postfix expressions.
- Comparison between different object types no longer raises a spurious runtime error.

---

## [0.6.x] - 2018–2019

Series of incremental releases adding language features (closures, ranges, maps, lists, optional modules) and fixing compiler and runtime issues. See git history for per-commit details.

---

## [0.5.x] - 2017–2018

Initial public release series establishing the core language, VM, and compiler pipeline.
