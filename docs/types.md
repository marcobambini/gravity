## Values and Types

Gravity is a dynamically typed language so variables do not have a type, although they refer to a value that does have a type. In Gravity everything is an object (with methods you can call and instance variables you can use). Basic built-in types are of class [Object](object.md), [Int](int.md), [Float](float.md), [String](string.md), [Bool](bool.md), [Null](null.md), [Class](class.md), [Function](func.md), [Fiber](fiber.md), Instance, [List](list.md), [Map](map.md) and [Range](range.md).

### Manifest Typing

Gravity supports manifest typing, so you can specify which type is associated with an Object with a syntax like:
```swift
var s:String = "Hello World";
var n:Int = 100;
```

It can also be used in func parameters:
```swift
func sum (a:Int, b: Int) {
	return a + b;
}
```

> Manifest typing is currently used in autocompletion feature, full static type checking will be introduced in a future release.
