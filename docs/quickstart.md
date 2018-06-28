## Getting started

### Install
To install Gravity, simply execute the commands given below. This should make two executables: **gravity**, the compiler itself and **unittest**, the test runner.
```bash
	git clone https://github.com/marcobambini/gravity.git
	cd gravity
	make
```

> If you want to access the gravity compiler globally just add it to your **PATH**.  
You can also use the **Xcode** project to create the gravity or unittest executables.

### Configure your editor
Programming is way more enjoyable when you have the right tools. That's why we've equipped several code editors with Gravity support. Just click on your favourite editor and configure it accordingly:
* [Visual Studio Code](https://github.com/Dohxis/vscode-gravity)
* [Atom](https://github.com/Tribex/atom-language-gravity)
* [vim](https://github.com/hallzy/gravity.vim)
* [BBEdit](https://github.com/marcobambini/bbedit-gravity)

### Command line
To view all possible flags you can run the command below:
```bash
	./gravity --help
```

To compile a gravity file to a exec.json executable:
```bash
	./gravity -c myfile.gravity -o exec.json
```

To execute a precompiled json executable file:
```bash
	./gravity -x exec.json
```

To directly execute a gravity file (without first serializing it to json):
```bash
	./gravity myfile.gravity
```
### Unit Tests
You can run [unit tests](unittest.md) by providing a path to a folder containing all test files:
```bash
	./gravity -t path_to_test_folder
```
This should produce output like:
	<img src="assets/images/unittest.png" width="666px" height="466px">

			
### Hello World
A simple <strong>Hello World</strong> code in Gravity looks like:
```swift
	func main() {
		System.print("Hello World!")
	}
```
