### Int

In most dynamically typed programming language both Integers and Float are internally represented by a C double value.

In a modern 64bit system, this implementation leads to some issue because some integer values cannot be correctly represented by a double value (for more details please read [Storage of integer values in double](https://www.viva64.com/en/l/0018/)).

In Gravity Int and Float are internally represented by two different types to mitigate rounding errors.

An Int represents a 64 bit signed number (can optionally be compiled as 32 bit signed number):
```swift
	var a = 123;        // decimal
	var b = 0xFF;       // hexadecimal
	var c = 0O7777;     // octal
	var d = 0B0101;     // binary

	var e = Int.random(1, 10) // returns a random int between 1 and 10 inclusive

	var f = 30.radians // returns the result of converting 30 degrees to radians
	var f = 3.degrees  // returns the result of converting 3 radians to degrees
```

An Int can also be used as a convenient way to execute loops:
```swift
	5.loop() {
		System.print("Hello World");
	}
	// result
	// Hello World
	// Hello World
	// Hello World
	// Hello World
	// Hello World
```

The Int class exposes a min/max property used to know at runtime lower/upper bound values:
```swift
	var min = Int.min;    // -9223372036854775808 in 64bit systems
	var max = Int.max;    //  9223372036854775807 in 64bit systems
```
