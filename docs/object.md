### Object

Object is the root class of every object inside Gravity. Through the Object class, objects inherit a basic interface to the runtime system and the ability to behave as Gravity objects.

All the built-in Gravity type are built from the base Object class and when you declare a new Class in Gravity without a super class then it is set by default to Object.

The new [Introspection](introspection.md) feature is built on top of the base Object class (so any other Class automatically inherits that feature).

## Built-in types

Gravity has some built-in types that extend and overrides methods and classes from the base Object class:
* [Int](int.md)
* [Float](float.md)
* [String](string.md)
* [Bool](bool.md)
* [Null](null.md)
* [Class](class.md)
* [Function](func.md)
* [Fiber](fiber.md)
* Instance
* [List](list.md)
* [Map](map.md)
* [Range](range.md)
* [Function](func.md)
* [Closure](closure.md)
* [Fiber](fiber.md)

## Special internal methods
Some methods has a very special meaning, for example by implementing the **exec** method your class is able to be executable via the object() notation. By implementing the **loadat/storeat** method your object can be accessed via the subscript shortcut object[i]. The **load/store** method is internally used to implement the dot notation (object.property).

The Class class overrides the exec method in order to implement Object instantiation and initialization:
```swift
  var foo = Foo()
  // means execute the exec method of the Foo class
```

All these special methods are implemented by the base Object class in order to provide most the basic functionalities that an user expects for a modern object oriented programming language.
