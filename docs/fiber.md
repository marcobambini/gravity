## Fiber
Fibers are user-space threads without a scheduler; a Fiber can yield and resume its execution from the place it has exited. A Fibers (or coroutine as called in other languages) are special functions that can be interrupted at any time by the user.<br><br>When a conventional function is invoked, execution begins at the start, and once a function exits, it is finished. By contrast, Fibers can exit by calling other Fibers, which may later return to the point where they were invoked in the original coroutine:

```swift
	func main() {
		var fiber = Fiber.create({
			System.print("fiber 1");
			Fiber.yield()
			System.print("fiber 2");
		});

		System.print("main 1");
		fiber.call()
		System.print("main 2");
		fiber.call()
		System.print("main 3");
	}
	// Output:
	// main 1
	// fiber 1
	// main 2
	// fiber 2
	// main 3
```

A Fiber is created with `create`:

```swift
	Fiber.create( {
		System.print("\(self) is the current fiber")
	})
```
and executed till the next `yield` with `fiber.call()`

```swift
var closure = {
    System.print("1")
    Fiber.yield()

    System.print("2")
    Fiber.yield()

    System.print("3")
    Fiber.yield()

    System.print("Done")
}

var fiber = Fiber.create(closure)

fiber.call()
// prints 1

fiber.call()
// prints 2

fiber.call()
// prints 3

fiber.call()
// prints Done

System.print(fiber.isDone())
// prints true
```

There are 2 types of yield:
1. `Fiber.yield()` it returns the controll to the function calling `call()`
2. `Fiber.yieldWaitTime(seconds)` it returns the controll to the function calling `call()` and also store the current time internally.

The later enable a call check of the total time in seconds passed since last `call()`. If the time amount is not enough the call is void and the fiber is not entered.

Example:

To implement a function that do some stuff every second, like a timer, a way is to use `Fiber.yieldWaitTime(seconds)`

```swift
var fiber = Fiber.create({
  var keepGoing = true
  while (keepGoing) {
    keepGoing = doSomeStuff()
    Console.write("Waiting")
    Fiber.yieldWaitTime(1.0)
    Console.write("Elapsed time: \(self.elapsedTime())")
  }
})

...
// Note: this strict loop is just for reference, not a real case.
while (!fiber.isDone()) {
  fiber.call()
}

```
