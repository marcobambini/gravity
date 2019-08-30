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
