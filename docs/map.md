## Map

Maps are associative containers implemented as pairs each of which maps a key to a value. You can create a map by placing a series of comma-separated entries inside square brackets. Each entry is a key and a value separated by a colon:

```swift
	// create a new map with 4 entries
	var d = ["Mark":1, "Andrew":2, "Paul":3, "Ross":4];

	// map has a count property
	var n = d.count;	// n is 4

	// create an empty map
	var map = [:];
```

### Looking up values
You can access an element from a list by calling the subscript operator [] on it with the key of the element you want:
			
```swift
	var names = ["Mark":1, "Andrew":2, "Paul":3, "Ross":4];
	names["Mark"];      // 1
	names["Andrew"];    // 2
```

### Iterating items
The subscript operator works well for finding values when you know the key you’re looking for, but sometimes you want to see everything that’s in the map. Since the Map class implements the iterator method (through the keys method), you can easily use it in a for loop:
```swift
	var people = ["Mark":1, "Andrew":2, "Paul":3, "Ross":4];
	for (var name in people.keys()) {
		System.print("Current name is " + name);
	}
```

### Adding items
An item can be added to a map by simply setting a key/value:
```swift
	var people = ["Mark":1, "Andrew":2, "Paul":3, "Ross":4];
	people["Kiara"] = 5;	// people now contains the "Kiara" key with value 5
```

### Removing items
The remove method has been added to the map class as a conveniente way to remove keys:
```swift
	var people = ["Mark":1, "Andrew":2, "Paul":3, "Ross":4];
	people.remove("Paul");
	people.remove("Ross");
	return people.count; // 2 is returned in this case
```

### Retrieving keys
The keys method has been added to the map class as a conveniente way to get access to all keys:
```swift
	var people = ["Mark":1, "Andrew":2, "Paul":3, "Ross":4];
	return people.keys; // ["Mark", "Andrew", "Paul", "Ross"] is returned in this case
```

### Checking for a key
To check if a key has been added to a map you can use the he hasKey method:
```swift
	var people = ["Mark":1, "Andrew":2, "Paul":3, "Ross":4];
	return people.hasKey("Max"); // false is returned in this case because the "Max" key is not in the people map
```
