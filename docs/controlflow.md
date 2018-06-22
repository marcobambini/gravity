## Control flow

Gravity provides a variety of control flow statements. Control flow is used to determine which chunks of code are executed and how many times. Branching statements and expressions decide whether or not to execute some code and looping ones execute something more than once.

### If statement
It is often useful to execute different pieces of code based on certain conditions. You might want to run an extra piece of code when an error occurs, or to display a message when a value becomes too high or too low. To do this, you make parts of your code conditional. In its simplest form, the if statement has a single if condition. It executes a set of statements only if that condition is true:
```swift
			var counter = 30;
			if (counter <= 10) {
				// do something here
			}
```

The if statement can provide an alternative set of statements, known as an else clause, for situations when the if condition is false. These statements are indicated by the else keyword:
```swift
			var counter = 30;
			if (counter <= 10) {
				// do something here
			} else {
				// do something else here
			}
```

More complex if statement:
```swift
			var counter = 30;
			if (counter <= 10) {
				// do something here
			} else if (counter <= 20) {
				// do something else here
			} else {
				// do something else here
			}
```
