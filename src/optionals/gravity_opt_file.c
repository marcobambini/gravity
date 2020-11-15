//
//  gravity_opt_file.c
//  gravity
//
//  Created by Marco Bambini on 15/11/2020.
//  Copyright © 2020 Creolabs. All rights reserved.
//

#include "gravity_vm.h"
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_utils.h"
#include "gravity_macros.h"
#include "gravity_vmmacros.h"
#include "gravity_opt_file.h"

static gravity_class_t              *gravity_class_file = NULL;
static uint32_t                     refcount = 0;

// MARK: - Implementation -

/*
     GRAVITY EXAMPLE
     ==============
    
     func main() {
         var target_file = "FULL_PATH_TO_A_TEXT_FILE_HERE";
         var target_folder = "FULL_PATH_TO_A_FOLDER_HERE";
 
         // FILE TEST
         var size = File.size(target_file);
         var exists = File.exists(target_file);
         var is_dir = File.is_directory(target_file);
         var data = File.read(target_file);
         
         System.print("File: " + target_file);
         System.print("Size: " + size);
         System.print("Exists: " + exists);
         System.print("Is Directory: " + is_dir);
         System.print("Data: " + data);
         
         // FOLDER TEST
         func closure (file_name, full_path, is_directory) {
             if (is_directory) {
                 System.print("+ \(file_name)");
             } else {
                 System.print("    \(file_name)");
             }
         }
         
         var recursive = true;
         var n = File.directory_scan(target_folder, recursive, closure);
         
         // return the number of file processed
         return n;
     }
 
 */

static bool internal_file_size (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 1 parameter of type string is required
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    int64_t size = file_size((const char *)path);
    RETURN_VALUE(VALUE_FROM_INT(size), rindex);
}

static bool internal_file_exists (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 1 parameter of type string is required
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool exists = file_exists((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(exists), rindex);
}

static bool internal_file_delete (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 1 parameter of type string is required
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool result = file_delete((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static bool internal_file_read (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 1 parameter of type string is required
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    size_t len = 0;
    char *buffer = file_read((const char *)path, &len);
    if (!buffer) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    gravity_value_t string = VALUE_FROM_STRING(vm, buffer, (uint32_t)len);
    RETURN_VALUE(string, rindex);
}

static bool internal_file_write (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 2 parameters of type string are required
    if (nargs != 3 && !VALUE_ISA_STRING(args[1]) && !VALUE_ISA_STRING(args[2])) {
        RETURN_ERROR("A path parameter of type String and a String parameter are required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    char *buffer = VALUE_AS_STRING(args[2])->s;
    size_t len = (size_t)VALUE_AS_STRING(args[2])->len;
    bool result = file_write((const char *)path, buffer, len);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static bool internal_file_buildpath (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 2 parameters of type string are required
    if (nargs != 3 && !VALUE_ISA_STRING(args[1]) && !VALUE_ISA_STRING(args[2])) {
        RETURN_ERROR("A file and path parameters of type String are required.");
    }
    
    char *file = VALUE_AS_STRING(args[1])->s;
    char *path = VALUE_AS_STRING(args[2])->s;
    char *result = file_buildpath((const char *)file, (const char *)path);
    
    if (!result) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    gravity_value_t string = VALUE_FROM_STRING(vm, result, (uint32_t)strlen(result));
    RETURN_VALUE(string, rindex);
}

static bool internal_file_is_directory (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 1 parameter of type string is required
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool result = is_directory((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static bool internal_file_directory_create (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // 1 parameter of type string is required
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool result = directory_create((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static void scan_directory (gravity_vm *vm, char *path, bool recursive, gravity_closure_t *closure, gravity_int_t *n, bool isdir) {
    DIRREF dir = directory_init ((const char *)path);
    if (!dir) return;
    
    if (isdir) {
        // report directory name
        char *name = file_name_frompath(path);
        gravity_value_t p1 = VALUE_FROM_CSTRING(vm, name);
        mem_free(name);
        gravity_value_t p2 = VALUE_FROM_CSTRING(vm, path);
        gravity_value_t p3 = VALUE_FROM_BOOL(true);
        gravity_value_t params[] = {p1, p2, p3};
        
        gravity_vm_runclosure(vm, closure, VALUE_FROM_NULL, params, 3);
        if (n) *n = *n + 1;
    }
    
    #ifdef WIN32
    char buffer[MAX_PATH];
    #else
    char *buffer = NULL;
    #endif

    const char *target_file;
    while ((target_file = directory_read_extend(dir, buffer))) {
        char *full_path = file_buildpath(target_file, path);
        if (recursive && (is_directory(full_path))) {
            scan_directory(vm, full_path, recursive, closure, n, true);
            continue;
        }
        
        // call user closure with target_file and full_path
        gravity_value_t p1 = VALUE_FROM_CSTRING(vm, target_file);
        gravity_value_t p2 = VALUE_FROM_CSTRING(vm, full_path);
        gravity_value_t p3 = VALUE_FROM_BOOL(false);
        gravity_value_t params[] = {p1, p2, p3};
        mem_free(full_path);
        
        gravity_vm_runclosure(vm, closure, VALUE_FROM_NULL, params, 3);
        if (n) *n = *n + 1;
    }
}

static bool internal_file_directory_scan (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    char *path = NULL;
    
    // check for minimum params
    if (nargs < 3) {
        RETURN_ERROR("A path and a closure parameter are required.");
    }
    
    // first parameter of type string is required
    if (!VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    } else {
        path = VALUE_AS_STRING(args[1])->s;
    }
    
    // optional bool 2nd parameter
    int nindex = 2;
    bool recursive = true;
    if (VALUE_ISA_BOOL(args[2])) {
        recursive = VALUE_AS_BOOL(args[2]);
        nindex = 3;
    }
    
    if (!VALUE_ISA_CLOSURE(args[nindex])) {
        RETURN_ERROR("A closure parameter is required.");
    }
    
    // extract closure
    gravity_closure_t *closure = VALUE_AS_CLOSURE(args[nindex]);
    gravity_int_t n = 0;
    
    // do not report directory name in the first scan
    scan_directory(vm, path, recursive, closure, &n, false);
    
    /*
     func closure (var filename, var full_path) {
        Console.write(filename);
     }
     
     var skipdot = true;
     var recursive = true;
     File.directory_scan("/Users/marco/Desktop/", true, recursive, closure);
     */
    
    RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

// MARK: - Internals -

static void create_optional_class (void) {
    gravity_class_file = gravity_class_new_pair(NULL, GRAVITY_CLASS_FILE_NAME, NULL, 0, 0);
    gravity_class_t *meta = gravity_class_get_meta(gravity_class_file);

    gravity_class_bind(meta, "size", NEW_CLOSURE_VALUE(internal_file_size));
    gravity_class_bind(meta, "exists", NEW_CLOSURE_VALUE(internal_file_exists));
    gravity_class_bind(meta, "delete", NEW_CLOSURE_VALUE(internal_file_delete));
    gravity_class_bind(meta, "read", NEW_CLOSURE_VALUE(internal_file_read));
    gravity_class_bind(meta, "write", NEW_CLOSURE_VALUE(internal_file_write));
    gravity_class_bind(meta, "buildpath", NEW_CLOSURE_VALUE(internal_file_buildpath));
    gravity_class_bind(meta, "is_directory", NEW_CLOSURE_VALUE(internal_file_is_directory));
    gravity_class_bind(meta, "directory_create", NEW_CLOSURE_VALUE(internal_file_directory_create));
    gravity_class_bind(meta, "directory_scan", NEW_CLOSURE_VALUE(internal_file_directory_scan));

    SETMETA_INITED(gravity_class_file);
}

// MARK: - Commons -

bool gravity_isfile_class (gravity_class_t *c) {
    return (c == gravity_class_file);
}

const char *gravity_file_name (void) {
    return GRAVITY_CLASS_FILE_NAME;
}

void gravity_file_register (gravity_vm *vm) {
    if (!gravity_class_file) create_optional_class();
    ++refcount;

    if (!vm || gravity_vm_ismini(vm)) return;
    gravity_vm_setvalue(vm, GRAVITY_CLASS_FILE_NAME, VALUE_FROM_OBJECT(gravity_class_file));
}

void gravity_file_free (void) {
    if (!gravity_class_file) return;
    if (--refcount) return;

    gravity_class_t *meta = gravity_class_get_meta(gravity_class_file);
    gravity_class_free_core(NULL, meta);
    gravity_class_free_core(NULL, gravity_class_file);

    gravity_class_file = NULL;
}
