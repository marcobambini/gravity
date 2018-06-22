## API

Gravity can be extended at runtime using C API. The right step to proceed is usually to create a new class, then add methods and properties to it and finally register that class inside the VM.
```c
	// report error callback function
	void report_error (error_type_t error_type, const char *message,
	                   error_desc_t error_desc, void *xdata) {
		printf("%s\n", message);
		exit(0);
	}

	// function to be executed inside Gravity VM
	bool my_function (gravity_vm *vm, gravity_value_t *args,
	                  uint16_t nargs, uint32_t rindex) {
		// do something useful here
	}

	// Configure VM delegate
	gravity_delegate_t delegate = {.error_callback = report_error};

	// Create a new VM
	gravity_vm *vm = gravity_vm_new(&delegate);

	// Create a new class
	gravity_class_t *c = gravity_class_new_pair (vm, "MyClass", NULL, 0, 0);

	// Allocate and bind closures to the newly created class
	gravity_closure_t *closure = gravity_closure_new(vm, my_function);
	gravity_class_bind(c, "myfunc", VALUE_FROM_OBJECT(closure));

	// Register class inside VM
	gravity_vm_setvalue(vm, "MyClass", VALUE_FROM_OBJECT(c));
```

Using the above C code a "MyClass" class has been registered inside the VM and ready to be used by Gravity:
```swift
	func main() {
		// allocate a new class
		var foo = MyClass();

		// execute the myfunc C function
		foo.myfunc();
	}
```

### Execute Gravity code from C
```c
```

### Bridge API
Gravity C API offers much more flexibility using the delegate bridge API.
TO DO: more information here.
TO DO: post objc bridge.
