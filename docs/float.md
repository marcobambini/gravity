### Float

In most dynamically typed programming language both Integers and Float are internally represented by a C double value.

In a modern 64bit system, this implementation leads to some issue because some integer values cannot be correctly represented by a double value (for more details please read [Storage of integer values in double](https://www.viva64.com/en/l/0018/)).

In Gravity Int and Float are internally represented by two different types to mitigate rounding errors.

An Float represents a 64 bit floating point number (can optionally be compiled as 32 bit floating point number):
```swift
	var a = 3.1415;   // float
	var b = 1.25e2;   // scientific notation

	var f = 30.5.radians  // returns the result of converting 30.5 degrees to radians
	var f = 3.14.degrees. // returns the result of converting 3.14 radians to degrees
```

The Float class exposes also a min/max property used to know at runtime lower/upper bound values:
```swift
	var min = Float.min;    // 2.22507e-308 in 64bit systems
	var max = Float.max;    // 1.79769e+308 in 64bit systems
```

Other useful methods:
```swift
	var f = 3.1415;       // float
	var f1 = f.ceil();    // result is 4 (ceil computes the smallest integer value not less than f)
	var f2 = f.round();   // result is 3 (round computes the nearest integer value to f)
	var f3 = f.floor();   // result is 3 (floor computes the largest integer value not greater than f)
```
