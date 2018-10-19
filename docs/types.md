## Values and Types

Gravity is a dynamically typed language so variables do not have a type, although they refer to a value that does have a type. In Gravity everything is an object (with methods you can call and instance variables you can use). Basic intrinsic types are of class Object, Int, Float, String, Bool, Null, [Class](class.md), [Function](func.md), Fiber, Instance, [List](list.md), [Map](map.md) and Range.

### Object
Object is the root class of every object inside Gravity. Through the Object class, objects inherit a basic interface to the runtime system and the ability to behave as Gravity objects.

### Int
An Int represents a 64 bit (can optionally be compiled as 32 bit) signed number:
```swift
	var a = 123;        // decimal
	var b = 0xFF;       // hexadecimal
	var c = 0O7777;     // octal
	var d = 0B0101;     // binary

	var e = Int.random(1, 10) // returns a random int between 1 and 10 inclusive

	var f = 30.radians // returns the result of converting 30 degrees to radians
	var f = 3.degrees  // returns the result of converting 3 radians to degrees
```

### Float
A float represents a 32 bit (or better) floating point number:
```swift
	var a = 3.1415;		// float
	var b = 1.25e2;		// scientific notation

	var f = 30.5.radians // returns the result of converting 30.5 degrees to radians
	var f = 3.14.degrees // returns the result of converting 3.14 radians to degrees
```

### String
Strings are an immutable sequence of characters. String literals can be surrounded with double or single quotes.
```swift
	var a = "Hello World";  // double quotes
	var b = 'Hello World';  // single quotes

	// Strings have also a length property
	var n = b.length;       // n is now 11

	// Strings also have some built in methods
	n = b.index("Wor")      // n is now 6.

	n = b.count("l")        // n is now 3.
	n = b.count("llo")      // n is now 1.

	n = "A".repeat(10)      // n is now "AAAAAAAAAA"

	// upper() and lower() operate on the whole string when 0 arguments are passed
	n = b.upper()           // n is now "HELLO WORLD"
	n = b.lower()           // n is now "hello world"

	// upper() and lower() can both take multiple integer arguments
	n = b.upper(1, -1)     // n is now "HEllo WorlD"

	// split() is the opposite of a list join
	var string = "Roses are Red, Violets are Blue"
	var list = string.split(", ")
	list[0]                // "Roses are Red"
	list[1]                // "Violets are Blue"

	// Split can also be used to convert a string to a list type so that you can
	// call list specific methods
	var string = "abcdefghijklmnpqrstuvwxyz".split('')
	string.contains("o")   // false
	string.contains("s")   // true

	// You are also able to edit strings by character...
	b[0] = "Z"              // b is now "Zello World"
	b[1] = "abc"            // b is now "Zabco World"
	b[-7] = "QWERTY"        // b is now "ZabcQWERTYd"

	// and retrieve those characters
	n = a[6]                // n is now "W"
	n = a[-7]               // n is now "o"
	n = a[0...4]            // n is now "Hello"
	n = a[-5...-1]          // n is now "World"
	n = a[-5...10]          // n is now "World"
	n = a[-1...-5]          // n is now "dlroW"
	
	// replace o with i
	var str = "momo"
	str.replace("o", "i") // returns mimi
```

### Bool
The Bool data type can have only two values, they are the literals true and false. A Bool value expresses the validity of a condition (tells whether the condition is true or false).
```swift
	var a = true;
	var b = false;
```

### Null
It indicates the absence of a value. If you call a method that doesn’t return anything and get its returned value, you get null back. The null data type is also used to initialize uninitialized variables with a default value.
```swift
	class Newton {
		var mass = 10;
		var acceleration;
		func force() {
			return mass * acceleration;
		}
	}

	func f2() {
		var sir = Newton();
		// acceleration instance variable has no default value
		// so it is automatically set to null
		return sir.force();
	}

	func f1() {
		var a;
		// a is uninitialized so it has a default null value
		return a;
	}
```

### Range
A range is an object that represents a consecutive range of numbers. Syntax for this type has been directly inspired by Swift.
```swift
	// a represents a range with values 1,2,3
	var a = 1...3;

	// b represents a range with values 1,2
	var b = 1..<3;

	// Ranges have also a conveniente count property
	var n1 = a.count;	// n1 is now 3
	var n2 = b.count;	// n2 is now 2
```
