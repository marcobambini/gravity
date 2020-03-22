//
//  main.cpp
//  GravityCpp
//
//  Created by Marco Bambini on 26/12/2018.
//  Copyright © 2018 Creolabs. All rights reserved.
//

#include "gravity_compiler.h"
#include "gravity_core.h"
#include "gravity_vm.h"
#include "gravity_macros.h"
#include "gravity_vmmacros.h"
#include "gravity_opcodes.h"
#include <iostream>
using namespace std;

// MARK: - C++ code -

class Rectangle {
public:
    double length;
    double height;
    
    // Constructor
    Rectangle(double l = 2.0, double h = 2.0) {
        cout << "Rectangle constructor called." << endl;
        length = l;
        height = h;
    }
    
    virtual ~Rectangle() {
        cout << "Rectangle destructor called." << endl;
    }
    
    // Methods
    double Area() {
        return length * height;
    }
    
    void Test(double p1, int32_t p2, string p3) {
        cout << "Rectangle test: " << p1 << p2 << p3 << endl;
    }
};

// MARK: - Gravity Bridge -

static bool rect_create (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // check for optional parameters here (if you need to process a more complex constructor)
    
    // self parameter is the rect_class create in register_cpp_classes
    gravity_class_t *c = (gravity_class_t *)GET_VALUE(0).p;
    
    // create Gravity instance and set its class to c
    gravity_instance_t *instance = gravity_instance_new(vm, c);
    
    // allocate a cpp instance of the Rectangle class on the heap
    Rectangle *r = new Rectangle();
    
    // set cpp instance and xdata of the gravity instance (for later used in the rect_area and rect_test functions)
    gravity_instance_setxdata(instance, r);
    
    // return instance
    RETURN_VALUE(VALUE_FROM_OBJECT(instance), rindex);
}

static bool rect_area (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // get self object which is the instance created in rect_create function
    gravity_instance_t *instance = (gravity_instance_t *)GET_VALUE(0).p;
    
    // get xdata (which is a cpp Rectangle instance)
    Rectangle *r = (Rectangle *)instance->xdata;
    
    // invoke the Area method
    double d = r->Area();
    
    RETURN_VALUE(VALUE_FROM_FLOAT(d), rindex);
}

static bool rect_test (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // get self object which is the instance created in rect_create function
    gravity_instance_t *instance = (gravity_instance_t *)GET_VALUE(0).p;
    
    // get xdata (which is a cpp Rectangle instance)
    Rectangle *r = (Rectangle *)instance->xdata;
    
    // invoke the Test method with dummy parameters
    // the right way to proceed here would be to check for nargs first
    // and then check if each parameter is of the right type
    // and then bind parameters to the cpp call (using the std::bind method)
    r->Test(3.0, 89, "rect_test");
    
    RETURN_NOVALUE();
}

static void object_free (gravity_vm *vm, gravity_object_t *obj) {
    gravity_instance_t *instance = (gravity_instance_t *)obj;
    
    // get xdata (which is a cpp Rectangle instance)
    Rectangle *r = (Rectangle *)instance->xdata;
    
    // explicitly free memory
    delete r;
}

void register_cpp_classes (gravity_vm *vm) {
    // create Rectangle class
    gravity_class_t *rect_class = gravity_class_new_pair(vm, "Rectangle", NULL, 0, 0);
    gravity_class_t *rect_class_meta = gravity_class_get_meta(rect_class);
    
    gravity_class_bind(rect_class_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(rect_create));
    gravity_class_bind(rect_class, "area", NEW_CLOSURE_VALUE(rect_area));
    gravity_class_bind(rect_class, "test", NEW_CLOSURE_VALUE(rect_test));
    
    // register Rectangle class inside VM
    gravity_vm_setvalue(vm, "Rectangle", VALUE_FROM_OBJECT(rect_class));
}

// MARK: - Main -

void report_error (gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
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
    cout << "Gravity version " << GRAVITY_VERSION << endl << endl;
    
    const char source_code[] =  "extern var Rectangle;"
                                "extern var Triangle;"
                                "func main() {"
                                "var r = Rectangle();"
                                "System.print(r.area());"
                                "r.test(1.0, 32, \"Hello\");"
                                "return 1;"
                                "}";
    
    // setup delegate
    gravity_delegate_t delegate = {
        .error_callback = report_error,
        .bridge_free = object_free
    };
    
    // setup compiler
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    
    // compile source code
    gravity_closure_t *closure = gravity_compiler_run(compiler, source_code, strlen(source_code), 0, true, true);
    if (!closure) return -1; // syntax/semantic error
    
    // setup Gravity VM
    gravity_vm *vm = gravity_vm_new(&delegate);
    
    // transfer memory from compiler to VM
    gravity_compiler_transfer(compiler, vm);
    
    // cleanup compiler
    gravity_compiler_free(compiler);
    
    // register my C++ classes inside Gravity VM
    register_cpp_classes(vm);
    
    // run main func
    if (gravity_vm_runmain(vm, closure)) {
        // print result to stdout
        gravity_value_t result = gravity_vm_result(vm);
        gravity_value_dump(vm, result, NULL, 0);
    }
    
    // cleanup
    gravity_vm_free(vm);
    gravity_core_free();
    
    return 0;
}
