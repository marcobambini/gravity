## Extending Gravity

Gravity can be extended at runtime using the C API (please read the [Embedding](https://marcobambini.github.io/gravity/#/embedding) section before proceeding).

Three steps are required:
1. Create a new class
2. Add methods and properties to the class
3. Register this class inside the VM

In this simple example we'll create a "Foo" class with a "sum" method in C and then we'll execute it from Gravity.
```c
#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"
#include "gravity_vmmacros.h"

// notice the usage of the extern clause to tell compiler (the front-end) that the Foo object will be registered later by the back-end (the VM)
const char *source_code = " \
extern var Foo; \
func main () {   \
    var c = Foo();  \
    return c.sum(30, 40);   \
}   \
";

static bool sum (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // SKIPPED: check nargs (must be 3 because arg[0] is self)
    gravity_value_t v1 = GET_VALUE(1);
    gravity_value_t v2 = GET_VALUE(2);
    
    // SKIPPED: check that both v1 and v2 are int numbers
    RETURN_VALUE(VALUE_FROM_INT(v1.n + v2.n), rindex);
}

void setup_foo (gravity_vm *vm) {
    // create a new Foo class
    gravity_class_t *c = gravity_class_new_pair (vm, "Foo", NULL, 0, 0);
    
    // allocate and bind bar closure to the newly created class
    gravity_class_bind(c, "sum", NEW_CLOSURE_VALUE(sum));
    
    // register class c inside VM
    gravity_vm_setvalue(vm, "Foo", VALUE_FROM_OBJECT(c));
}

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
    
    // once memory has been trasferred, you can get rid of the front-end
    gravity_compiler_free(compiler);
    
    // register my Foo class inside Gravity VM
    setup_foo(vm);
    
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
For more examples see the file gravity_core.c in the src/runtime/ directory. Most of the Gravity classes are built using these same APIs.
