[![Build Status](https://travis-ci.org/marcobambini/gravity.svg?branch=master)](https://travis-ci.org/marcobambini/gravity)

<p align="center" >
<img src="https://raw.githubusercontent.com/marcobambini/gravity/master/docs/assets/images/logo-gravity.png" height="74px" alt="Gravity Programming Language" title="Gravity Programming Language">
</p>

**Gravity** is a powerful, dynamically typed, lightweight, embeddable programming language written in C without any external dependencies (except for stdlib). It is a class-based concurrent scripting language with modern <a href="https://github.com/apple/swift">Swift</a>-like syntax.

**Gravity** supports procedural programming, object-oriented programming, functional programming and data-driven programming. Thanks to special built-in methods, it can also be used as a prototype-based programming language.

**Gravity** has been developed from scratch for the <a href="http://creolabs.com" target="_blank">Creo</a> project in order to offer an easy way to write portable code for the iOS and Android platforms. It is written in portable C code that can be compiled on any platform using a C99 compiler. The VM code is about 4K lines long, the multipass compiler code is about 7K lines and the shared code is about 3K lines long. The compiler and virtual machine combined add less than 200KB to the executable on a 64 bit system.

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
* dynamic typing
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
* **optional semicolons**

## Special thanks
Gravity was supported by a couple of open source projects. The inspiration for closures comes from the elegant <a href="http://www.lua.org" target="_blank">Lua</a> programming language; specifically from the document <a href="http://www.cs.tufts.edu/~nr/cs257/archive/roberto-ierusalimschy/closures-draft.pdf">Closures in Lua</a>. For fibers, upvalues handling and some parts of the garbage collector, my gratitude goes to <a href="http://journal.stuffwithstuff.com" target="_blank">Bob Nystrom</a> and his excellent <a href="https://github.com/munificent/wren">Wren</a> programming language. A very special thanks should also go to my friend **Andrea Donetti** who helped me debugging and testing various aspects of the language.

## Documentation
The <a href="https://marcobambini.github.io/gravity/#/README">Getting Started</a> page is a guide for downloading and compiling the language. There is also a more extensive <a href="http://gravity-lang.org">language documentation</a>. Official [wiki](https://github.com/marcobambini/gravity/wiki) is used to collect related projects and tools.

## Community
Seems like a good idea to make a group chat for people to discuss about Gravity.<br> [![Join the chat at https://gitter.im/gravity-lang/](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/gravity-lang/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

## Contributing
Contributions to Gravity are welcomed and encouraged!<br>
More information are available in the official [CONTRIBUTING](CONTRIBUTING.md) file.
* <a href="https://github.com/marcobambini/gravity/issues/new">Open an issue</a>:
	* if you need help
	* if you find a bug
	* if you have a feature request
	* to ask a general question
* <a href="https://github.com/marcobambini/gravity/pulls">Submit a pull request</a>:
	* if you want to contribute

## License
Gravity is available under the permissive MIT license.
