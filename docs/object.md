### Object

Object is the root class of every object inside Gravity. Through the Object class, objects inherit a basic interface to the runtime system and the ability to behave as Gravity objects.

All the built-in Gravity type are built from the base Object class and when you declare a new Class in Gravity without a super class then it is set by default to Object.

The new [Introspection](introspection.md) feature is built on top of the base Object class (so any other Class automatically inherits that feature).
