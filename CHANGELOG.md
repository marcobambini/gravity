# Changelog

All notable changes to Gravity are documented in this file.

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
