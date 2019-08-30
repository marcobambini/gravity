## List

Lists (or arrays) are simple sequence of objects, their size is dynamic and their index starts always at 0. They provide fast random access to their elements. You can create a list by placing a sequence of comma-separated expressions inside square brackets:

```swift
	var r = [1, 2, "Hello", 3.1415, true];

	// list has a count property
	var n = r.count;	// n is 5
```

### Accessing items
You can access an element from a list by calling the subscript operator [] on it with the index of the element you want. Like most languages, indices start at 0:
			
```swift
	var names = ["Mark", "Andrew", "Paul", "Ross", "Frank", "Max"];
	names[0];	// "Mark"
	names[2];	// "Paul"
```

Negative indices count backwards from the end:
```swift
	var names = ["Mark", "Andrew", "Paul", "Ross", "Frank", "Max"];
	names[-1];	// "Max"
	names[-2];	// "Frank"
```

### Iterating items
The subscript operator works well for finding values when you know the key you’re looking for, but sometimes you want to see everything that’s in the list. Since the List class implements the iterator method, you can easily use it in a for loop:
```swift
	var people = ["Mark", "Andrew", "Paul", "Ross", "Frank", "Max"];
	for (var name in people) {
		System.print("Current name is " + name);
	}
```

### Adding items
A List instance can be expanded by setting an index that is greater than the current size of the list:
```swift
	var list = [10,20,30,40,50];
	list[30] = 22;	// list contains now 31 elements (index 0...30)
```

### List as a stack
The List class implements the push/pop methods as a convenient way to treat a list as a stack:
```swift
	var list = [10,20,30,40,50];
	list.push(100);		// add 100 to the list
	var v1 = list.pop();	// pop 100
	var v2 = list.pop();	// pop 50
```

### List Contains
The List class implements the contains methods as a convenient way to check for the existence of a value in a list:
```swift
	var list = [1, 2, "Hello", 3.1415, true];
	return list.contains(3.1415); // Returns: true
```

### List Join
The List class implements the join method as a convenient way to interpret a list as a string:
```swift
	var list = [1,2,3,4,5];
	list.join(" + "); // Becomes: "1 + 2 + 3 + 4 + 5"
```

### List Map
The List class implements the map method as a convenient way to create a new list using the current values of a list in some defined way:
			
```swift
  var numbers = [1,2,3,4,5,6,7,8,9,10]

  var squared = numbers.map(func(num) {
    return num*num
  })
  // squared is now equal to [1,4,9,16,25,36,49,64,81,100]
```

### List Filter
The List class implements the filter method as a convenient way to create a new list that contains the elements of the original list which passed a specified test:
			
```swift
  var numbers = [1,2,3,4,5,6,7,8,9,10]

  var even = numbers.map(func(num) {
    return !(num % 2)
  })
  // even is now equal to [2,4,6,8,10]
```

### List Reduce
The List class implements the reduce method as a convenient way to create a new list reduces a list to a single value based on a provided callback:
			
```swift
  var numbers = [1,2,3,4,5,6,7,8,9,10]

  var sum = numbers.reduce(0, func(num1, num2) {
    return num1+num2
  })
  // sum is now equal to 55
```

### List Sort
The List class implements the sort method as a convenient way to sort its items. By default, the sort() method sorts the values as strings (or numbers) in alphabetical (for strings) and ascending order:
			
```swift
  var fruits = ["Banana", "Orange", "Apple", "Mango"];
  fruits.sort();
  // fruits is now [Apple,Banana,Mango,Orange]
  
  var numbers = [10, 3.14, 82, 1, 7];
  numbers.sort();
  // numbers is now [1,3.14,7,10,82]
  
  // if you need to customize the sort algorithm you can provide a closure
  func compare (a, b) {
    return (a < b);
  }
  
  var fruits = ["Banana", "Orange", "Apple", "Mango"];
  fruits.sort(compare);
  // fruits is now [Orange,Mango,Banana,Apple]
  
```
