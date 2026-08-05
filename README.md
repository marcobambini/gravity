<p align="center" >
<img src="https://raw.githubusercontent.com/marcobambini/gravity/master/docs/assets/images/logo-gravity.png" height="74px" alt="Gravity Programming Language" title="Gravity Programming Language">
</p>

**Gravity** is a powerful, dynamically typed, lightweight, embeddable programming language written in C without any external dependencies (except for stdlib). It is a class-based concurrent scripting language with modern <a href="https://github.com/apple/swift">Swift</a>-like syntax.

**Gravity** supports procedural programming, object-oriented programming, functional programming, and data-driven programming. Thanks to special built-in methods, it can also be used as a prototype-based programming language.

**Gravity** has been developed from scratch for the <a href="http://creolabs.com" target="_blank">Creo</a> project in order to offer an easy way to write portable code for the iOS and Android platforms. It is written in portable C code that can be compiled on any platform using a C99 compiler. The VM code is about 6.5K lines long, the multipass compiler code is about 10K lines and the shared code is about 4.7K lines long. The compiler and virtual machine combined add less than 200KB to the executable on a 64-bit system.

## What Gravity code looks like

```swift
class Vector {
	// instance variables
	var x = 0;
	var y = 0;
	var z = 0;

	// constructor
	func init (a = 0, b = 0, c = 0) {
		x = a; y = b; z = c;
	}

	// instance method (built-in operator overriding)
	func + (v) {
		if (v is Int) return Vector(x+v, y+v, z+v);
		else if (v is Vector) return Vector(x+v.x, y+v.y, z+v.z);
		return null;
	}

	// instance method (built-in String conversion overriding)
	func String() {
	        // string interpolation support
		return "[\(x),\(y),\(z)]";
	}
}

func main() {
	// initialize a new vector object
	var v1 = Vector(1,2,3);
	
	// initialize a new vector object
	var v2 = Vector(4,5,6);
	
	// call + function in the vector object
	var v3 = v1 + v2;
	
	// returns string "[1,2,3] + [4,5,6] = [5,7,9]"
    	return "\(v1) + \(v2) = \(v3)";
 }
 ```

## Features
* multipass compiler with optimizer
* dynamic typing
* classes and inheritance
* higher-order functions and classes
* lexical scoping
* coroutines (via fibers)
* nested classes
* closures
* garbage collection (mark-and-sweep)
* operator overriding
* string interpolation
* enums, modules, and structs (value types)
* switch/case and ranges
* optional modules (Math, File, JSON, ENV)
* powerful embedding API with bridging support
* built-in unit tests
* built-in JSON serializer/deserializer
* **optional semicolons**

## Building

**Make (Linux / macOS / BSD)**
```bash
make                    # Build the gravity CLI executable
make mode=debug         # Debug build with symbols
make lib                # Build shared library (libgravity.dylib/so/dll)
make staticlib          # Build static library (libgravity.a)
make example            # Build the C embedding API example
make clean              # Clean all build artifacts
```

**CMake (cross-platform, including Windows)**
```bash
cmake -B build
cmake --build build
# Optionally disable the CLI and build the library only:
cmake -B build -DBUILD_CLI=OFF
cmake --build build
```

Requires a C99 compiler. No external dependencies.

## Usage

```bash
./gravity file.gravity                  # Compile and execute a source file
./gravity -c file.gravity               # Compile to bytecode (outputs gravity.g)
./gravity -o out.json -c file.gravity   # Compile to a specific output file
./gravity -x gravity.g                  # Execute precompiled bytecode
./gravity -i 'return 2 + 3'             # Execute inline code
./gravity -t test/unittest              # Run unit tests
```

## Testing

```bash
./gravity -t test/unittest              # Run all unit tests via the VM
./test/unittest/run_all.sh              # Run all unit tests via shell script (with per-test timeouts)
./gravity test/unittest/somefile.gravity # Run a single test file
```

The `test/` directory also contains `fuzzy/` (randomised fuzzing inputs) and `infiniteloop/` (tests that must terminate with a runtime error rather than hang).

## Project Structure

```
src/
├── cli/            Command-line interface
├── compiler/       Lexer, parser, AST, semantic analysis, IR, optimizer, codegen
├── runtime/        Stack-based VM, built-in types and core methods
├── shared/         Value representation, opcodes, hash table, array, memory/GC
├── optionals/      Optional modules: Math, File, JSON, ENV
└── utils/          Debug disassembler, JSON serialization, file I/O, UTF-8
```

For a comprehensive technical deep-dive into the implementation, see [ARCHITECTURE.md](ARCHITECTURE.md).

## Embedding API

Gravity is designed to be embedded inside a host application. The complete API lives in `src/runtime/gravity_vm.h` and `src/compiler/gravity_compiler.h`. A minimal example:

```c
#include "gravity_compiler.h"
#include "gravity_core.h"
#include "gravity_vm.h"

static void report_error(gravity_vm *vm, error_type_t type,
                         const char *description, error_desc_t desc, void *xdata) {
    printf("%s\n", description);
}

int main(void) {
    const char *source = "func main() { return 6 * 7; }";

    gravity_delegate_t delegate = {.error_callback = report_error};

    // compile
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    gravity_closure_t *closure   = gravity_compiler_run(compiler, source, strlen(source), 0, true, true);

    // create VM and transfer compiler-owned objects into it
    gravity_vm *vm = gravity_vm_new(&delegate);
    gravity_compiler_transfer(compiler, vm);
    gravity_compiler_free(compiler);

    // execute and read result
    if (gravity_vm_runmain(vm, closure)) {
        gravity_value_t result = gravity_vm_result(vm);
        gravity_value_dump(vm, result, NULL, 0);  // prints: 42
    }

    gravity_vm_free(vm);
    gravity_core_free();
    return 0;
}
```

See [`examples/example.c`](examples/example.c) and the [embedding documentation](https://gravity-lang.org) for the full bridging API.

## Special thanks
Gravity was supported by a couple of open-source projects. The inspiration for closures comes from the elegant <a href="http://www.lua.org" target="_blank">Lua</a> programming language; specifically from the document <a href="http://www.cs.tufts.edu/~nr/cs257/archive/roberto-ierusalimschy/closures-draft.pdf">Closures in Lua</a>. For fibers, upvalues handling and some parts of the garbage collector, my gratitude goes to <a href="http://journal.stuffwithstuff.com" target="_blank">Bob Nystrom</a> and his excellent <a href="https://github.com/munificent/wren">Wren</a> programming language. A very special thanks should also go to my friend **Andrea Donetti** who helped me debugging and testing various aspects of the language.

## Documentation
The <a href="https://marcobambini.github.io/gravity/#/README">Getting Started</a> page is a guide for downloading and compiling the language. There is also a more extensive <a href="https://gravity-lang.org">language documentation</a>. Official [wiki](https://github.com/marcobambini/gravity/wiki) is used to collect related projects and tools. For implementation internals, see the [Architecture Document](ARCHITECTURE.md).

## Where Gravity is used
* Gravity is the core language built into Creo (https://creolabs.com)
* Gravity is the scripting language for the Untold game engine (https://youtu.be/OGrWq8jpK14?t=58)

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a summary of changes across versions.

## Community

[![GitHub Discussions](https://img.shields.io/badge/discussions-GitHub-blue)](https://github.com/marcobambini/gravity/discussions)

Questions, ideas, and general discussion are welcome in [GitHub Discussions](https://github.com/marcobambini/gravity/discussions).

## Contributing
Contributions to Gravity are welcomed and encouraged!<br>
More information is available in the official [CONTRIBUTING](CONTRIBUTING.md) file.
* <a href="https://github.com/marcobambini/gravity/issues/new">Open an issue</a>:
	* if you need help
	* if you find a bug
	* if you have a feature request
	* to ask a general question
* <a href="https://github.com/marcobambini/gravity/pulls">Submit a pull request</a>:
	* if you want to contribute

## License
Gravity is available under the permissive MIT license.
