## Closure

Closures are self-contained blocks of functionality that can be passed around and used in your code. Closures can capture and store references to any constants and variables from the context in which they are defined. Closures can be nested and can be anonymous (without a name):

```swift
	func f1(a) {
		return func(b) {
			return a + b;
		}
	}

	func main() {
		var addTen = f1(10);
		return addTen(20);	// result is 30
	}
```

### Disassemble
A closure can be disassembled in order to reveal its bytecode:
```swift
	func sum (a,b) {
		return a + b;
	}

	func main() {
		System.print(sum.disassemble());
	}

	// Output:
	// 000000 ADD 3 1 2
	// 000001 RET 3
```
