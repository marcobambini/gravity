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
