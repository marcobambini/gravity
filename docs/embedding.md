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

To keep the code as simple as possible I skipped any error check condition that would be required. The bare minimum C code to embed the above Gravity code would be:
```c
#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"

const char *source_code = " \
func sum (a, b) {   \
    return a + b    \
}   \
    \
func mul (a, b) {   \
    return a * b    \
}   \
    \
func main () {   \
    var a = 10   \
    var b = 20   \
    return sum(a, b) + mul(a, b)   \
}   \
";

// a very simple report error callback function
void report_error (gravity_vm *vm, error_type_t error_type, const char *message,
                   error_desc_t error_desc, void *xdata) {
    printf("%s\n", message);
    exit(0);
}

int main (void) {
    // setup a delegate struct (much more options are available)
    gravity_delegate_t delegate = {.error_callback = report_error};
    
    // allocate a new compiler
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    
    // compile Gravity source code into a closure (bytecode)
    gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, strlen(source_code), 0, true, true);
    
    // allocate a new Gravity VM
    gravity_vm *vm = gravity_vm_new(&delegate);
    
    // transfer memory from the compiler (front-end) to the VM (back-end)
    gravity_compiler_transfer(compiler, vm);
    
    // once the memory has been transferred, you can get rid of the front-end
    gravity_compiler_free(compiler);
    
    // execute main closure
    if (gravity_vm_runmain(vm, closure)) {
        // retrieve returned result
        gravity_value_t result = gravity_vm_result(vm);
	
        // dump result to a C string and print it to stdout
        char buffer[512];
        gravity_value_dump(vm, result, buffer, sizeof(buffer));
        printf("RESULT: %s\n", buffer);
    }
    
    // free VM and core libraries (implicitly allocated by the VM)
    gravity_vm_free(vm);
    gravity_core_free();
    
    return 0;
}

```

To load and execute a `myfile` Gravity source code from disk the required changes would be minimum:
```c
int main (void) {
    size_t size = 0;
    const char *source_code = file_read("myfile.gravity", &size);
    
    ...
    
    // compile Gravity source code into bytecode (embedded into a closure)
    // notice the change of the is_static bool parameter to false
    gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, size, 0, false, true);
    
    ...
}
```

To directly execute the `mul` Gravity function and pass some parameter from C to Gravity some minor changes need to be performed:
```c
#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"

const char *source_code = " \
func sum (a, b) {   \
    return a + b    \
}   \
    \
func mul (a, b) {   \
    return a * b    \
}   \
";

int main (void) {
    // setup a delegate struct
    gravity_delegate_t delegate = {.error_callback = report_error};
    
    // allocate a new compiler
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    
    // compile Gravity source code into bytecode
    gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, strlen(source_code), 0, true, true);
    
    // allocate a new Gravity VM
    gravity_vm *vm = gravity_vm_new(&delegate);
    
    // transfer memory from the compiler (front-end) to the VM (back-end)
    gravity_compiler_transfer(compiler, vm);
    
    // once the memory has been transferred, you can get rid of the front-end
    gravity_compiler_free(compiler);
    
    // load closure into VM
    gravity_vm_loadclosure(vm, closure);
    
    // lookup a reference to the mul closure into the Gravity VM
    gravity_value_t mul_function = gravity_vm_getvalue(vm, "mul", (uint32_t)strlen("mul"));
    if (!VALUE_ISA_CLOSURE(mul_function)) {
        printf("Unable to find mul function into Gravity VM.\n");
        return -1;
    }
    
    // convert function to closure
    gravity_closure_t *mul_closure = VALUE_AS_CLOSURE(mul_function);
    
    // prepare parameters
    gravity_value_t p1 = VALUE_FROM_INT(30);
    gravity_value_t p2 = VALUE_FROM_INT(40);
    gravity_value_t params[] = {p1, p2};
    
    // execute mul closure
    if (gravity_vm_runclosure (vm, mul_closure, VALUE_FROM_NULL, params, 2)) {
        // retrieve returned result
        gravity_value_t result = gravity_vm_result(vm);
    
        // dump result to a C string and print it to stdout
        char buffer[512];
        gravity_value_dump(vm, result, buffer, sizeof(buffer));
        printf("RESULT: %s\n", buffer);
    }
    
    // free VM and core libraries (implicitly allocated by the VM)
    gravity_vm_free(vm);
    gravity_core_free();
    
    return 0;
}
```
