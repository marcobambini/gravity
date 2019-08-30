## Enum

Enums defines a common type for a group of related values. If you are familiar with C, you will know that C enumerations assign related names to a set of integer values. Enums in Gravity are much more flexible and enable you to assign any literal value (Int, Float, String, Bool):

```swift
	enum state {
		nothing,                // default to 0
		active,                 // default to 1
		inactive,               // default to 2
		undetermined = 666,
		error                   // 667
	}

	enum math {
		pi = 3.141592,
		e = 2.718281,
		goldratio = 1.618033
	}

	enum company {
		ceo = "Gauss",
		cto = "Eurel",
		cfo = "Nostradamus"
	}

	enum mixed {
		one = "Hello World",
		two = 3.1415,
		three = 666,
		four = true
	}

	func main() {
		var a = state.active;    // a = 1
		var b = math.pi;         // b = 3.1415
		var c = company.ceo;     // c = "Gauss"
		var d = mixed.four;      // d = true
	}
```

> Enum is a static operator, which means that at compile time the real value of the enum item is automatically replaced by Gravity.
