## Syntax

**Gravity** syntax is designed to be familiar to people coming from C-like languages like Javascript, Swift, C++, C# and many more. We started working on this new language a year before Apple announced Swift and we were happily surprised to discovered how similar both syntax appear.
>In Gravity semicolon separator **;** is completely optional.


What Gravity code looks like:
```swift
	class Rectangle {
		// instance variables
		var width;
		var height;

		// instance method
		func area() {
			return width*height;
		}

		// constructor
		func init(w, h) {
			width = w;
			height = h;
		}
	}

	func main() {
		// initialize a new Rectangle object
		var r = Rectangle(20, 10);

		// return value is 20*10 = 200
		return r.area();
	}
```

### Comments
Gravity supports both line comments:
```swift
	// This is a line comment
```
and block comments:
```swift
	/*
		This
		is
		a
		multi-line
		comment
	*/
```
While Gravity uses C-Style comments, Gravity still supports the common "#!" shebang to tell your shell what program to execute the file with. However, the shebang must be on the first line of the file in order to use it in this way:
```swift
	#!/path/to/gravity

	func main() {
		System.print("Execute as: path/to/file.gravity");
		System.print("Instead of: gravity path/to/file.gravity");
	}
```

### Include
To statically include a gravity file from within another gravity file you can use the #include statement.
Please note that in the current version, the path to the included file is relative to the location that the gravity executable was run.
```swift
	// adder.gravity
	func add(x, y) {
		return x+y;
	}
```
```swift
	// main.gravity
	#include "adder.gravity"

	func main() {
		System.print("5+4=" + add(5,4));
	}
```

### Import
While #include is used to include file at compilation time, the import statement enables you to load modules at runtime.
Import is under active development and it will be available pretty soon.

### Reserved Keywords
Like many other programming languages Gravity has some reserved keywords that assume a very specific meaning in the context of the source code:
```swift
	if in or is for var and not func else true enum case null
	file lazy super false break while class const event _func
	_args struct repeat switch return public static extern
	import module default private continue internal undefined
```

### Identifiers
Identifiers represent a naming rule used to identify objects inside your source code. Gravity is a case-sensitive language. Identifiers start with a letter or underscore and may contain letters, digits, and underscores (function identifiers can be any of the [built-in operators](operators.md) in order to override a default behaviour):
```swift
	a
	_thisIsValid
	Hello_World
	foo123
	BYE_BYE
```

### Blocks and Scope
Every named identifier introduced in some portion of the source code is introduced in a scope. The scope is the largest part of the source code in which that identifier is valid. The names declared by a declaration are introduced into a specific scope based on the context of the declaration. For instance, local variable declarations introduce the name into the block scope, whereas class member variable declarations introduce the name into class scope.<br><br>There are three scopes defined: **block scope**, **class scope** and **file scope**. Names declared in the block scope become visible immediately after its completed declarator.  This means you cannot refer to a name within the block scope until after it has been fully declared. Names declared in the file and class scopes become visible immediately upon executing the starting statement of the script. This means you can refer to a name within the file or class scopes before it has been fully declared.<br><br>These are all valid scopes:
```swift
	// file scope can refer to a name
	// before it has been fully declared
	func f1() {
	    return f2();
	}
	func f2() {
	    return 42;
	}

	// block scope can be nested and
	// can hide other local variables
	func f3() {
		var a = 10;
		if (a > 0) {
			var a = 20;
		}
		// 10 is returned here
		return a;
	}
```
