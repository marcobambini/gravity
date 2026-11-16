<p align="center">
<img src="assets/images/logo.png" height="90px" alt="Gravity Programming Language" title="Gravity Programming Language">
</p>

**Gravity** is a powerful, dynamically typed, lightweight, embeddable programming language written in C without any external dependencies (except for stdlib). It is a class-based concurrent scripting language with a modern [Swift](https://github.com/apple/swift) like syntax.

**Gravity** supports procedural programming, object-oriented programming, functional programming and data-driven programming. Thanks to special built-in methods, it can also be used as a prototype-based programming language.

**Gravity** has been developed from scratch for the [Creo](https://creolabs.com) project in order to offer an easy way to write portable code for the iOS and Android platforms. It is written in portable C code that can be compiled on any platform using a C99 compiler. The VM code is about 4K lines long, the multipass compiler code is about 7K lines and the shared code is about 3K lines long. The compiler and virtual machine combined, add less than 200KB to the executable on a 64 bit system.

> Comments in the C code make it easy to read and understand.

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
* multipass compiler
* dynamic typing (manifest typing coming soon)
* classes and inheritance
* higher order functions and classes
* lexical scoping
* coroutines (via fibers)
* nested classes
* closures
* garbage collection
* operator overriding
* powerful embedding api
* built-in unit tests
* built-in JSON serializer/deserializer
