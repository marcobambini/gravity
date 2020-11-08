## Embedding

Gravity can be easily embedded into any C/C++ code. Suppose to have the following Gravity code:
```swift
func sum (a, b) {
    return a + b
}

func mul (a, b) {
    return a * b
}

func main () {
    var a = 10
    var b = 20
    return sum(a, b) + mul(a, b)
}
```

The bare minimum C code to embed Gravity would be:
```C

const char *source_code = "
func sum (a, b) {
    return a + b
}

func mul (a, b) {
    return a * b
}

func main () {
    var a = 10
    var b = 20
    return sum(a, b) + mul(a, b)
}
";

// a very simple report error callback function
void report_error (error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
    printf("%s\n", message);
    exit(0);
}

void main (void) {
    // setup a delegate struct
    gravity_delegate_t delegate = {
        .error_callback = report_error
    };
    
    // allocate a new compiler
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    
    // compile Gravity source code into bytecode (embedded into a closure)
    gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, strlen(source_code), 0, true, true);
    
    // allocate a new Gravity VM
    gravity_vm *vm = gravity_vm_new(&delegate);
    
    // transfer memory from the compiler (front-end) to the VM (back-end)
    gravity_compiler_transfer(compiler, vm);
    
    // once memory has been trasferred, you can get rid of the front-end
    gravity_compiler_free(compiler);
    
    // execute closure
    if (gravity_vm_runmain(vm, closure)) {
        // retrieve returned result
        gravity_value_t result = gravity_vm_result(vm);
	
	// print result to stdout
	printf("RESULT: %s", t, VALUE_AS_CSTRING(result));
    }
    
    // free VM and core libraries (implicitly allocated by the VM)
    gravity_vm_free(vm);
    gravity_core_free();
}

```
