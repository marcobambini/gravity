## Class

Every value in Gravity is an object, and every object is an instance of a class. Classes define an object's behavior and state. Behavior is defined by methods which live in the class. Every object of the same class supports the same methods. State is defined in fields, whose values are stored in each instance.<br><br>Like [functions](func.md) a **Class is a first class object**, that means that it can be stored in local variables (even in [Lists](list.md) or [Maps](map.md)), passed as a function parameter or returned by a function. Gravity supports **nested classes** and **single inheritance**.

### Defining a class
Like most programming languages the class keyword is used to declare a new class:
```swift
	class Italy {
	}
```

### Instantiate a class
A class in gravity can be instantiated by simply executing it (without the new keyword):
```swift
	var instance = Italy();
```

### Methods
Functions declared inside a class are called methods and are used to add behaviors to objects that belong to a specific class:
```swift
	class Italy {
		func print() {
			System.print("Hello from Italy");
		}
	}
```

### Properties
Variables declared inside a class are called properties and are used to add states to objects that belong to a specific class:
```swift
	class Italy {
		var population = 60656000;
		var area = 301340; // in km2

		func density() {
			return population/area;
		}
	}

	func main() {
		var it = Italy();
		return it.density();	// returns 201
	}
```

### Class methods and properties
A class method (or property) is a method (or property) that operates on class objects rather than instances of the class. In Gravity you can specify a class method (or property) using the static keyword:
```swift
	class Italy {
		static var population = 60656000;
		static var area = 301340; // in km2

		static func density() {
			return population/area;
		}
	}

	func main() {
		return Italy.density();
	}

```

### Getters and Setters:
As a convenient way to execute some code when a property is read or written, Gravity fully support custom getters and setters:
```swift
	class foo {
		private var _a = 12;
		var a {
			set {_a = value * 100;} // value is default parameter name
			get {return _a/2;}
		};
		var b {
			// in this case b is a write-only property
			set (newb) {_a = newb * 50;}	// parameter name can be specified
		};
	}

	func main() {
		var f = foo();
		f.a = 14;       // 14*100 = 1400
		return f.a;     // 1400/2 = 700
	}
```

### Adding methods at runtime:
Sometimes you need to add methods at runtime to a particular instance, this is far more efficient than subclassing and in many cases it could be a decision than can be applied only at runtime. Gravity provides a convenient **bind** method specifically developed to manage this feature:
```swift
	class foo {
		func f1() {System.print("Hello from f1");}
	}

	func main() {
		var obj = foo();
		obj.f1();	// Output: Hello from f1

		// add a new f2 method to obj instance
		obj.bind("f2", {System.print("Hello from f2");});
		obj.f2();	// Output: Hello from f2

		// replace f1 method
		obj.bind("f1", {System.print("Hello from f1 new");});
		obj.f1();	// Output: Hello from f1 new

		// with unbind you can remove an existing method
		obj.unbind("f2");
		obj.f2();	// RUNTIME ERROR: Unable to find f2
	}
```

### Nested classes:
There are many cases where nested classes can lead to more readable and maintainable code, for example as a way of logically grouping classes that are only used in one place:
```swift
	class Database {
		public var query;

		class RecordSet {
			public var sql;

			public func run() {
				if (!sql) return 0;
				System.print(sql);
				return sql.length();
			}

			func init() {
				System.print("RecordSet init called");
			}
		}

		func init() {
			System.print("Database init called");
			query = RecordSet();
		}
	}

	func main() {
		var db = Database();
		db.query.sql = "Hello World from Gravity!";
		return db.query.run();
	}
```

### Access specifiers
The public and private keywords can be used to restrict access to specific parts of code.
