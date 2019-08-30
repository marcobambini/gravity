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

A range is expecially useful in for loops:
```swift
	for (var i in 1...10) {
		// repeat for 10 times (with i from 1 to 10)
	}
```
