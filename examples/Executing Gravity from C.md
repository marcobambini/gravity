## Executing Gravity from C

Gravity has a clear separation between the compiler (the frontend) and the VM (the backend). So it is possibile to have the VM running without the compiler. The compiler can produce a bytecode executable file (JSON based) that can run inside the VM without the need to be recompiled.

```c
#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"

#define SOURCE	"func main() {var a = 10; var b=20; return a + b}"

// error reporting callback called by both compiler or VM
static void report_error (gravity_vm *vm, error_type_t error_type,
                          const char *description, error_desc_t error_desc, void *xdata) {
    printf("%s\n", description);
    exit(0);
}

int main (void) {
    // configure a VM delegate
    gravity_delegate_t delegate = {.error_callback = report_error};
    
    // compile Gravity source code into bytecode
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    gravity_closure_t *closure = gravity_compiler_run(compiler, SOURCE, strlen(SOURCE), 0, true, true);
    
    // sanity check on compiled source
    if (!closure) {
        // an error occurred while compiling source code and it has already been reported by the report_error callback
        gravity_compiler_free(compiler);
        return 1;
    }
    
    // create a new VM
    gravity_vm *vm = gravity_vm_new(&delegate);
    
    // transfer objects owned by the compiler to the VM (so they can be part of the GC)
    gravity_compiler_transfer(compiler, vm);
    
    // compiler can now be freed
    gravity_compiler_free(compiler);
    
    // run main closure inside Gravity bytecode
    if (gravity_vm_runmain(vm, closure)) {
        // print result (INT) 30 in this simple example
        gravity_value_t result = gravity_vm_result(vm);
        gravity_value_dump(vm, result, NULL, 0);
    }
    
    // free VM memory and core libraries
    gravity_vm_free(vm);
	gravity_core_free();
    
    return 0;
}
```





## Extending Gravity with C

Gravity can be extended at runtime using C API. The right step to proceed is usually to create a new class with methods and properties and then register that class inside the VM.

