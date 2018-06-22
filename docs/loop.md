## Loop

### While loop</h4>
A while loop performs a set of statements until a condition becomes false. These kind of loops are best used when the number of iterations is not known before the first iteration begins.
```swift
	func main() {
		var i = 0;

		while (i < 50000) {
			i += 1;
		}

		return i;
	}
```

### Repeat-while loop</h4>
The other variation of the while loop, known as the repeat-while loop, performs a single pass through the loop block first, before considering the loop’s condition. It then continues to repeat the loop until the condition is false.
			
```swift
	func main() {
		var i = 0;

		repeat {
			i += 1;
		} while (i < 50000);

		return i;
	}
```

### For loop</h4>
You can access an element from a list by calling the subscript operator [] on it with the index of the element you want. As in most languages, indices start at zero:
```swift
	var count = 0;
	for (var i in 0...40) {
		count += i;
	}
	return count;
```
The for in loop can be used over any object that supports iteration, such as [Lists](list.md), Strings or [Maps](map.md).

### Loop method</h4>
Performing a loop is very common operation in any programming language, so Gravity adds a very convenient way to run a loop by adding a special loop method to some classes (Int, Range, List, String and Map) that accepts a [closure](closure.md) as parameter:
```swift
	func main() {
		4.loop({System.print("Hello World");});
	}
	// Output:
	// Hello World
	// Hello World
	// Hello World
	// Hello World
```

If we need to access the current index of the loop we can just rewrite the closure:
```swift
	func main() {
		var target = 5;
		target.loop(func (value){System.print("Hello World " + value);});
	}
	// Output:
	// Hello World 0
	// Hello World 1
	// Hello World 2
	// Hello World 3
	// Hello World 4
```
Loop within a [Range](types.md):
```swift
	func main() {
		var target = 0...4;
		target.loop(func (value){System.print("Hello World " + value);});
	}
	// also in reverse order
	func main() {
		var target = 4...0;
		target.loop(func (value){System.print("Hello World " + value);});
	}
```

Loop within a [Lists](list.md):
```swift
	func main() {
		var target = [10,20,30,40,50,60,70,80,90];
		target.loop(func (value){System.print("Hello World " + value);});
	}
```

Loop within a String:
```swift
        func main() {
                var s = "abcdefghijklmnopqrstuvwxyz";

                var vowels = ""
                s.loop(func (c) {
                        if (c == "a" or
                            c == "e" or
                            c == "i" or
                            c == "o" or
                            c == "u") {
                                vowels += c;
                        }
                })
                System.print(vowels)  // aeiou
        }
```
			
Loop within a [Maps](map.md) where the key is passed as closure argument (please note that key order is not preserved):
```swift
	func main() {
		var target = ["key1":10,"key2":20,"key3":30,"key4":40];
		target.loop(func (key){System.print(key);});
	}
	// Output:
	// key1
	// key2
	// key4
	// key3
```