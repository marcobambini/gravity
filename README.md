<p align="center" >
<img src="https://raw.githubusercontent.com/marcobambini/gravity/master/docs/images/logo.png" height="74px" alt="Gravity Programming Language" title="Gravity Programming Language">
</p>

**Gravity** is a powerful, dynamically typed, lightweight, embeddable programming language written in C without any external dependency (except stdlib). It is a class based concurrent scripting language with a modern <a href="https://github.com/apple/swift">Swift</a> like syntax.

**Gravity** supports procedural programming, object-oriented programming, functional programming and data-driven programming. Thanks to built-in bind method it can also be used as a prototype-based programming language.

## How Gravity code looks like

```swift
class Vector {
	// instance variables
	var x=0;
	var y=0;
	var z=0;
	
	// constructor
	func init (a, b, c) {
		if (!a) a = 0;
		if (!b) b = 0;
		if (!c) c = 0;
		x = a; y = b; z = c;
	}
	
	// instance method (built-in operator overriding)
	func + (v) {
		if (v isa Int) return Vector(x+v, y+v, z+v);
		else if (v isa Vector) return Vector(x+v.x, y+v.y, z+v.z);
		return null;
	}
  	
	// instance method (built-in String conversion overriding)
	func String() {
		return "[" + x.String() + "," + y.String() + "," + z.String() + "]";
	}
}

func main() {
	// initialize a new vector object
	var v1 = Vector(1,2,3);
	// initialize a new vector object
	var v2 = Vector(4,5,6);
	// call + function in the vector object
	var v3 = v1 + v2;
	// returns string "[5,7,9]"
	return v3.String();
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
* garbage collector
* powerful embedding api
* built-in unit test
* built-in JSON serializer/deserializer

## Credits
**Gravity** has been developed from scratch for the <a href="http://creolabs.com" target="_blank">Creo</a> project in order to offer an easy way to write portable code for the iOS and Android platforms. For closures implementation inspirations come from the elegant <a href="http://www.lua.org" target="_blank">Lua</a> programming language. For fibers and some portions of the garbage collector my gratitude should go to <a href="http://journal.stuffwithstuff.com" target="_blank">Bob Nystrom</a> and his excellent <a href="https://github.com/munificent/wren">Wren</a> programming language.

## Documentation

## Author
My name is Marco Bambini and I am <a href="http://creolabs.com">Creolabs</a> founder. If you want to know more about me my personal homepage is <a href="http://marcobambini.com">http://marcobambini.com</a>.
