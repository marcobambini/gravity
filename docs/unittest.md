## Unit Test
Unit testing is so important (expecially for a programming language) that Gravity has built-in unit testing capabilities. What a user needs to do is setup some delegate C methods in order to be able to correctly setup a unit-test. Gravity already has a unit-test executable so in order to add your code to it you need to create a .gravity file using the special **#unittest** preprocessor macro:

```swift
#unittest {
	name: "A simple add operation.";
	error: NONE;
	result: 33;
};

func main() {
	var a = 11;
	var b = 22;
	return a + b;
}
```


> A unit test can also be written in order to be able to check for syntax, semantic and runtime errors. For syntax and semantic errors the user can also specify row and column of the expected generated error.
