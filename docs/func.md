## Function

Functions are first class objects like [Int](types.md) or [String](types.md) and can be stored in local variables (even in [Lists](list.md) or [Maps](map.md)), passed as function parameters or returned by a function. Functions can be implemented in Gravity or in a [native language](api.md) with calling conventions compatible with ANSI C.<br><br>Functions are called by value. This means that foo(1) calls the function which is the value of the variable foo. Calling a value that is not a function (or does not implement the exec method) will raise a runtime error.
```swift
	func main() {
		var a = 10;
		var b = 20;
		return a + b;
	}
```

```swift
	func f1() {
		return 10;
	}

	func f2() {
		return f1;
	}

	func main() {
		// a is now function f2
		var a = f2;

		// b is now the return value of f2 which is function f1
		var b = a();

		// return value is f1() which is 10
		return b();

		// above code is equivalent to
		return f2()();
	}
```

### Function parameters
Functions aren’t very useful if you can’t pass values to them so you can provide a parameter list in the function declaration. Gravity performs no check on the number of parameters so you can call a function providing more or less parameters.
```swift
	func sum(a, b) {
		return a + b;
	}

	// execute the sum function
	// and returns 30 as result
	sum(10,20);
```

If a function is called with missing arguments (less than declared), the missing values are set to **undefined**.
```swift
	// sum modified to take in account missing arguments
	func sum(a, b) {
		// equivalent to if (a == undefined) a = 30;
		if (!a) a = 30;

		// equivalent to if (b == undefined) b = 50;
		if (!b) b = 50;

		return a + b;
	}

	// execute the sum function without any argument
	// a has a 30 default value and b has a 50 default value
	// return value is 80
	sum();
```

If a function is called with more arguments (more than declared), the additional arguments can be accessed using the **_args** array.
```swift
	// sum modified to accept a variable number of arguments
	func sum() {
		var tot = 0;
		for (var i in 0..<_args.count) {
			tot += _args[i];
		}
		return tot;
	}

	// execute the sum function with a variable number
	// of arguments returns 550 as result
	sum(10,20,30,40,50,60,70,80,90,100);
```

### Recursion
Function recursion is fully supported in Gravity (current function can be accessed using the _func reserved keyword):
```swift
	func fibonacci (n) {
		if (n<2) return n;
		// could be written as return _func(n-2) + _func(n-1)
		return fibonacci(n-2) + fibonacci(n-1);
 	}

	func main() {
		return fibonacci(20);
 	}
```

### Returning values
A function without a return statement returns **null** by default. You can explicitly return a value using a return statement.
