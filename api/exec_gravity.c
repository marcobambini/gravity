//
//  Sample code to demonstrate how to execute Gravity code from C
//  Created by Marco Bambini on 06/03/2017.
//

#include <stdio.h>
#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"

// gravity func source code
static const char *source = "func add (a, b) {return a + b;}; \
                             func mul (a, b) {return a * b;};";

// error callback
static void report_error (gravity_vm *vm, error_type_t error_type, const char *message,
                          error_desc_t error_desc, void *xdata) {
    #pragma unused(vm, xdata)
    const char *type = "N/A";
    switch (error_type) {
        case GRAVITY_ERROR_NONE: type = "NONE"; break;
        case GRAVITY_ERROR_SYNTAX: type = "SYNTAX"; break;
        case GRAVITY_ERROR_SEMANTIC: type = "SEMANTIC"; break;
        case GRAVITY_ERROR_RUNTIME: type = "RUNTIME"; break;
        case GRAVITY_WARNING: type = "WARNING"; break;
        case GRAVITY_ERROR_IO: type = "I/O"; break;
    }

    if (error_type == GRAVITY_ERROR_RUNTIME) printf("RUNTIME ERROR: ");
    else printf("%s ERROR on %d (%d,%d): ", type, error_desc.fileid, error_desc.lineno, error_desc.colno);
    printf("%s\n", message);
}


int main(int argc, const char * argv[]) {

    // setup a minimal delegate
    gravity_delegate_t delegate = {
        .error_callback = report_error
    };

    // compile source into a closure
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    gravity_closure_t *closure = gravity_compiler_run(compiler, source, strlen(source), 0, true);
    if (!closure) return -1;

    // setup a new VM and a new fiber
    gravity_vm *vm = gravity_vm_new(&delegate);

    // transfer memory from compiler to VM and then free compiler
    gravity_compiler_transfer(compiler, vm);
    gravity_compiler_free(compiler);

    // load closure into VM context
    gravity_vm_loadclosure(vm, closure);

    // create parameters (that must be boxed) for the closure
    gravity_value_t n1 = VALUE_FROM_INT(30);
    gravity_value_t n2 = VALUE_FROM_INT(50);
    gravity_value_t params[2] = {n1, n2};

    // lookup add closure
    gravity_value_t add = gravity_vm_getvalue(vm, "add", strlen("add"));
    if (!VALUE_ISA_CLOSURE(add)) return -2;

    // execute add closure and print result
    if (gravity_vm_runclosure(vm, VALUE_AS_CLOSURE(add), add, params, 2)) {
        gravity_value_t result = gravity_vm_result(vm);
        printf("add result ");
        gravity_value_dump(vm, result, NULL, 0);
    }

    // lookup mul closure
    gravity_value_t mul = gravity_vm_getvalue(vm, "mul", strlen("mul"));
    if (!VALUE_ISA_CLOSURE(mul)) return -3;

    // execute mul closure and print result
    if (gravity_vm_runclosure(vm, VALUE_AS_CLOSURE(mul), mul, params, 2)) {
        gravity_value_t result = gravity_vm_result(vm);
        printf("mul result ");
        gravity_value_dump(vm, result, NULL, 0);
    }

    // free vm and core classes
    gravity_vm_free(vm);
    gravity_core_free();

    return 0;
}
