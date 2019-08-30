## System
System class is a class registered in every Gravity VM that offers some useful methods and properties.

### Print methods
```swift
	func main() {
		// print to stdout and add a newline character
		System.print("Hello World");

		// print to stdout without any newline character appended
		System.put("Hello World");
	}
```

### Garbage collector methods
Gravity automatically manages memory for you using a tri-colour marking garbage collector, using the System class the user has the ability to change some of its settings and even disable it when certain performance critical tasks need to be performed:
```swift
	func main() {
		// disable GC
		System.gcEnabled = false;

		// ratio used during automatic recomputation of the new gcthreshold value
		var ratio = System.gcRatio;

		// minimum GC threshold size
		var minthreshold = System.gcMinThreshold;

		// memory required to trigger a GC
		var threshold = System.gcThreshold;

		// enable GC
		System.gcEnabled = true;
	}
```

### Time related methods
There are times where it could be really useful to easily measure how much time is spent in a given task:
```swift
	func main() {
		var t1 = System.nanotime();
		perform_my_task();
		var t2 = System.nanotime();

		// return elapsed time in ms
		return ((t2-t1) / 1000000.0);
	}
```

### System Exit
There are times where it could be useful to have main() return an
				error code back to your shell:
```swift
	func main() {
		foo(); // Do something useful
		System.exit(5);
	}
```
In your terminal, you can now reference the return code:
```bash
	# Returns: 5
	$ echo $?
```
