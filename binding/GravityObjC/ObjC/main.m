//
//  main.c
//  GravityObjC
//
//  Created by Marco Bambini on 07/02/21.
//

#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"
#include "gravity_vmmacros.h"
#include "gravity_objc.h"
#include "console.h"

int main (void) {
    // setup delegate
    gravity_delegate_t delegate = {
        .error_callback = report_error,
    };
    
    // read source file from the shared top level directory
    size_t size = 0;
    const char *path = current_filepath(__FILE__, "main.gravity");
    const char *source_code = file_read(path, &size);
    if (!source_code) return -1;
    
    // compile source code into bytecode
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, size, 0, false, true);
    
    // transfer bytecode into a newly created VM
    gravity_vm *vm = gravity_vm_new(&delegate);
    gravity_compiler_transfer(compiler, vm);
    gravity_compiler_free(compiler);
    if (!closure) goto cleanup;
    
    // register ObjC bridge into VM
    objc_register(vm);
    
    // run VM and print result
    if (gravity_vm_runmain(vm, closure)) {
        gravity_value_t result = gravity_vm_result(vm);
        double t = gravity_vm_time(vm);
        
        // print result as string
        gravity_value_t s = convert_value2string(vm, result);
        printf("RESULT: %s (%.4f ms)\n\n", VALUE_AS_CSTRING(s), t);
    }
    
cleanup:
    gravity_vm_free(vm);
    gravity_core_free();
    
    return 0;
}
