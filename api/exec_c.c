//
//  Sample code to demonstrate how to execute C code from Gravity
//  Created by Marco Bambini on 12/03/2017.
//

#include <stdio.h>
#include <math.h>

#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"

#define CLASS_NAME    "Math"

// gravity func source code
// Math is declared as extern because it will be later defined in C
static const char *source =    " extern var Math;                   \
                                func main() {                       \
                                    var pi = Math.pi;               \
                                    var n1 = Math.log(pi);          \
                                    var n2 = Math.pow(pi,2.12);     \
                                    return n1 + n2;                 \
                                }";

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

// MARK: -

static bool math_pi (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (args, nargs)
    gravity_vm_setslot(vm, VALUE_FROM_FLOAT(3.1315f), rindex);
    return true;
}

static bool math_log (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // missing parameters check here
    // 1. number of args
    // 2. args type

    // assuming arg of type float (in a real example there should be a conversion if not float)
    gravity_float_t n = VALUE_AS_FLOAT(args[1]);

    // gravity can be compiled with FLOAT as 32 or 64 bit
    #if GRAVITY_ENABLE_DOUBLE
    gravity_float_t result = (gravity_float_t)log(n);
    #else
    gravity_float_t result = (gravity_float_t)logf(n);
    #endif

    gravity_vm_setslot(vm, VALUE_FROM_FLOAT(result), rindex);
    return true;
}

static bool math_pow (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // missing parameters check here
    // 1. number of args
    // 2. args type

    // assuming arg 1 of type float (in a real example there should be a conversion if not float)
    gravity_float_t n1 = VALUE_AS_FLOAT(args[1]);

    // assuming arg 2 of type float (in a real example there should be a conversion if not float)
    gravity_float_t n2 = VALUE_AS_FLOAT(args[2]);

    double result = pow((double)n1, (double)n2);

    gravity_vm_setslot(vm, VALUE_FROM_FLOAT((gravity_float_t)result), rindex);
    return true;
}

// MARK: -

static void create_math_class (gravity_vm *vm) {
    // create a new class (a pair of classes since we are creating a class and its meta-class)
    gravity_class_t *c = gravity_class_new_pair(NULL, CLASS_NAME, NULL, 0, 0);

    // we want to register properties and methods callback to its meta-class
    // so user can access Math.property and Math.method without the need to instantiate it

    // get its meta-class
    gravity_class_t *meta = gravity_class_get_meta(c);

    // start binding methods and properties (special methods) to the meta class

    // *** LOG METHOD ***
    // 1. create a gravity_function_t from the c function
    gravity_function_t *logf = gravity_function_new_internal(NULL, NULL, math_log, 0);

    // 2. create a closure from the gravity_function_t
    gravity_closure_t *logc = gravity_closure_new(NULL, logf);

    // 3. bind closure VALUE to meta class
    gravity_class_bind(meta, "log", VALUE_FROM_OBJECT(logc));

    // *** POW METHOD ***
    // 1. create a gravity_function_t from the c function
    gravity_function_t *powf = gravity_function_new_internal(NULL, NULL, math_pow, 0);

    // 2. create a closure from the gravity_function_t
    gravity_closure_t *powc = gravity_closure_new(NULL, powf);

    // 3. bind closure VALUE to meta class
    gravity_class_bind(meta, "pow", VALUE_FROM_OBJECT(powc));

    // *** PI PROPERTY (getter only) ***
    // 1. create a gravity_function_t from the c function
    gravity_function_t *pif = gravity_function_new_internal(NULL, NULL, math_pi, 0);

    // 2. create a closure from the gravity_function_t
    gravity_closure_t *pi_getter = gravity_closure_new(NULL, pif);

    // 3. create a new special function to represents getter and setter (NULL in this case)
    gravity_function_t *f = gravity_function_new_special(vm, NULL, GRAVITY_COMPUTED_INDEX, pi_getter, NULL);

    // 4. create a closure for the special function
    gravity_closure_t *closure_property = gravity_closure_new(NULL, f);

    // 5. bind closure VALUE to meta class
    gravity_class_bind(meta, "pi", VALUE_FROM_OBJECT(closure_property));

    // LAST STEP
    // register newly defined C class into Gravity VM
    gravity_vm_setvalue(vm, CLASS_NAME, VALUE_FROM_OBJECT(c));
}

// MARK: -

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

    // create a new math class with methods and properties and register it to the VM
    create_math_class(vm);

    // expected result: 12.387436
    // pi = 3.1415
    // n1 = log(pi) => 1.1447
    // n2 = pow(pi, 2.12) => 11.3221

    // Math class is now available from Gravity code so we can start excuting previously compiled closure
    if (gravity_vm_runmain(vm, closure)) {
        gravity_value_t result = gravity_vm_result(vm);
        double t = gravity_vm_time(vm);

        char buffer[512];
        gravity_value_dump(vm, result, buffer, sizeof(buffer));
        printf("RESULT: %s (in %.4f ms)\n\n", buffer, t);
    }

    // our Math C class was not exposed to the GC (we passed NULL as vm parameter) so we would need to manually free it here
    // free class and its methods here

    // free vm and base classes
    if (vm) gravity_vm_free(vm);
    gravity_core_free();
}
