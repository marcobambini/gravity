<p align="center" >
<img src="https://raw.githubusercontent.com/marcobambini/gravity/master/docs/images/logo.png" height="74px" alt="Gravity Programming Language" title="Gravity Programming Language">
</p>

**Gravity** is a powerful, dynamically typed, lightweight, embeddable programming language written in C without any external dependency (except stdlib). It is a class based concurrent scripting language with a modern <a href="https://github.com/apple/swift">Swift</a> like syntax.

**Gravity** supports procedural programming, object-oriented programming, functional programming and data-driven programming. Thanks to built-in bind methods it can also be used as a prototype-based programming language.

## Example

```swift
class Vector {
	public var x=0;
	public var y=0;
	public var z=0;
	
	public func init (a, b, c) {
		if (!a) a = 0;
		if (!b) b = 0;
		if (!c) c = 0;
		x = a; y = b; z = c;
	}
	
	public func + (v) {
		if (v isa Int) {
			return Vector(x+v, y+v, z+v);
		} else if (v isa Vector) {
			return Vector(x+v.x, y+v.y, z+v.z);
		}
		return null;
	}
  
  public func String() {
		return "[" + x.String() + "," + y.String() + "," + z.String() + "]";
	}
}

func main() {
  var v1 = Vector(1,2,3);
  var v2 = Vector(4,5,6);
  var v3 = v1 + v2;
  return v3.String();
 }
 ```

## Features

## To Do
* Add support for modules
* Code generation for ternary operator
* Code generation for switch statement
* Improve register allocation algorithm
* Improve parser error handling

## Internals

By design it has a clear distinction between the compiler itself (frontend and backend) and the runtime and the executable format is an highly portale JSON file (future version could include a binary representation).
