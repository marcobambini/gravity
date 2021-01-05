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

// MARK: Instance -

typedef struct {
    gravity_class_t         *isa;   // to be an object
    gravity_gc_t            gc;     // to be collectable by the garbage collector
    FILE                    *file;  // real FILE instance
} gravity_file_t;

#define VALUE_AS_FILE(x)    ((gravity_file_t *)VALUE_AS_OBJECT(x))

static uint32_t gravity_ifile_free (gravity_vm *vm, gravity_object_t *object) {
    UNUSED_PARAM(vm);
    
    gravity_file_t *instance = (gravity_file_t *)object;
    DEBUG_FREE("FREE %s", gravity_object_debug(object, true));
    if (instance->file) fclose(instance->file);
    mem_free((void *)object);
    
    return 0;
}

static gravity_file_t *gravity_ifile_new (gravity_vm *vm, FILE *f) {
    if (!f) return NULL;
    
    gravity_file_t *instance = (gravity_file_t *)mem_alloc(NULL, sizeof(gravity_file_t));
    if (!instance) return NULL;
    
    instance->isa = gravity_class_file;
    instance->gc.free = gravity_ifile_free;
    instance->file = f;

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) instance);
    return instance;
}

// MARK: - Implementation -

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
    
    RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

// MARK: -

static bool internal_file_open (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // var file = File.open("path_to_file", "mode")
    
    /*
     mode is a string:
        r or rb: Open file for reading.
        w or wb: Truncate to zero length or create file for writing.
        a or ab: Append; open or create file for writing at end-of-file.
        r+ or rb+ or r+b: Open file for update (reading and writing).
        w+ or wb+ or w+b: Truncate to zero length or create file for update.
        a+ or ab+ or a+b: Append; open or create file for update, writing at end-of-file.
     */
    
    // 1 parameter of type string is required
    if (nargs > 1 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    char *path = VALUE_AS_STRING(args[1])->s;
    
    char *mode = "r";
    if (nargs > 2 && VALUE_ISA_STRING(args[2])) {
        mode = VALUE_AS_STRING(args[2])->s;
    }
    
    FILE *file = fopen(path, mode);
    if (file == NULL) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    gravity_file_t *instance = gravity_ifile_new(vm, file);
    if (instance == NULL) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    RETURN_VALUE(VALUE_FROM_OBJECT((gravity_object_t*) instance), rindex);
}

static bool internal_file_iread (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // var data = file.read(N)
    
    // 1 parameter of type int is required
    if (nargs < 1 && (!VALUE_ISA_INT(args[1]) && !VALUE_ISA_STRING(args[1]))) {
        RETURN_ERROR("A parameter of type Int or String is required.");
    }
    
    gravity_file_t *instance = VALUE_AS_FILE(args[0]);
    gravity_int_t n = 256;
    gravity_string_t *str = NULL;
    size_t nread = 0;
    
    if (VALUE_ISA_INT(args[1])) n = VALUE_AS_INT(args[1]);
    else str = VALUE_AS_STRING(args[1]);
    
    char *buffer = (char *)mem_alloc(NULL, n);
    if (!buffer) {
        RETURN_ERROR("Not enought memory to allocate required buffer.");
    }
    
    // args[1] was a number so read up-to n characters
    if (str == NULL) {
        nread = fread(buffer, (size_t)n, 1, instance->file);
    } else {
        // read up-until s character was found (or EOF)
        // taking in account buffer b resizing
        // algorithm modified from: http://cvsweb.netbsd.org/bsdweb.cgi/~checkout~/pkgsrc/pkgtools/libnbcompat/files/getdelim.c
        
        int delimiter = (int)str->s[0];
        char *ptr, *eptr;
        
        for (ptr = buffer, eptr = buffer + n; ++nread;) {
            int c = fgetc(instance->file);
            if ((c == -1) || feof(instance->file)) break;
            
            *ptr++ = c;
            if (c == delimiter) break;
            
            if (ptr + 2 >= eptr) {
                char *nbuf;
                size_t nbufsiz = n * 2;
                ssize_t d = ptr - buffer;
                if ((nbuf = mem_realloc(NULL, buffer, nbufsiz)) == NULL) break;
                
                buffer = nbuf;
                n = nbufsiz;
                eptr = nbuf + nbufsiz;
                ptr = nbuf + d;
            }
        }
        // NULL terminate the string
        buffer[nread] = 0;
    }
    
    gravity_string_t *result = gravity_string_new(vm, buffer, (uint32_t)nread, (uint32_t)n);
    RETURN_VALUE(VALUE_FROM_OBJECT(result), rindex);
}

static bool internal_file_iwrite (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // var written = file.write(data)
    
    // 1 parameter of type int is required
    if (nargs < 1 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A parameter of type String is required.");
    }
    
    gravity_file_t *instance = VALUE_AS_FILE(args[0]);
    gravity_string_t *data = VALUE_AS_STRING(args[1]);
    
    size_t nwritten = fwrite(data->s, data->len, 1, instance->file);
    RETURN_VALUE(VALUE_FROM_INT(nwritten), rindex);
}

static bool internal_file_iseek (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // var result = file.seek(offset, whence)
    
    // 2 parameters of type int are required
    if (nargs != 3 && !VALUE_ISA_INT(args[1]) && !VALUE_ISA_INT(args[2])) {
        RETURN_ERROR("An offset parameter of type Int and a whence parameter of type Int are required.");
    }
    
    gravity_file_t *instance = VALUE_AS_FILE(args[0]);
    gravity_int_t offset = VALUE_AS_INT(args[1]);
    gravity_int_t whence = VALUE_AS_INT(args[2]);
    
    int result = fseek(instance->file, (long)offset, (int)whence);
    RETURN_VALUE(VALUE_FROM_INT(result), rindex);
}

static bool internal_file_ierror (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // var error = file.error();
    
    UNUSED_PARAM(nargs);
    gravity_file_t *instance = VALUE_AS_FILE(args[0]);
    int result = ferror(instance->file);
    RETURN_VALUE(VALUE_FROM_INT(result), rindex);
}

static bool internal_file_iflush (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // var error = file.flush();
    
    UNUSED_PARAM(nargs);
    gravity_file_t *instance = VALUE_AS_FILE(args[0]);
    int result = fflush(instance->file);
    RETURN_VALUE(VALUE_FROM_INT(result), rindex);
}

static bool internal_file_ieof (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // var isEOF = file.eof()
    UNUSED_PARAM(nargs);
    gravity_file_t *instance = VALUE_AS_FILE(args[0]);
    int result = feof(instance->file);
    RETURN_VALUE(VALUE_FROM_BOOL(result != 0), rindex);
}

static bool internal_file_iclose (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    UNUSED_PARAM(nargs);
    
    // var bool = file.close()
    gravity_file_t *instance = VALUE_AS_FILE(args[0]);
    
    bool result = false;
    if (instance->file) {
        fclose(instance->file);
        instance->file = NULL;
        result = true;
    }
    
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}


// MARK: - Internals -

static void create_optional_class (void) {
    gravity_class_file = gravity_class_new_pair(NULL, GRAVITY_CLASS_FILE_NAME, NULL, 0, 0);
    gravity_class_t *meta = gravity_class_get_meta(gravity_class_file);
    
    // class methods
    gravity_class_bind(meta, "size", NEW_CLOSURE_VALUE(internal_file_size));
    gravity_class_bind(meta, "exists", NEW_CLOSURE_VALUE(internal_file_exists));
    gravity_class_bind(meta, "delete", NEW_CLOSURE_VALUE(internal_file_delete));
    gravity_class_bind(meta, "read", NEW_CLOSURE_VALUE(internal_file_read));
    gravity_class_bind(meta, "write", NEW_CLOSURE_VALUE(internal_file_write));
    gravity_class_bind(meta, "buildpath", NEW_CLOSURE_VALUE(internal_file_buildpath));
    gravity_class_bind(meta, "is_directory", NEW_CLOSURE_VALUE(internal_file_is_directory));
    gravity_class_bind(meta, "directory_create", NEW_CLOSURE_VALUE(internal_file_directory_create));
    gravity_class_bind(meta, "directory_scan", NEW_CLOSURE_VALUE(internal_file_directory_scan));
    
    // instance methods
    gravity_class_bind(meta, "open", NEW_CLOSURE_VALUE(internal_file_open));
    gravity_class_bind(gravity_class_file, "read", NEW_CLOSURE_VALUE(internal_file_iread));
    gravity_class_bind(gravity_class_file, "write", NEW_CLOSURE_VALUE(internal_file_iwrite));
    gravity_class_bind(gravity_class_file, "seek", NEW_CLOSURE_VALUE(internal_file_iseek));
    gravity_class_bind(gravity_class_file, "eof", NEW_CLOSURE_VALUE(internal_file_ieof));
    gravity_class_bind(gravity_class_file, "error", NEW_CLOSURE_VALUE(internal_file_ierror));
    gravity_class_bind(gravity_class_file, "flush", NEW_CLOSURE_VALUE(internal_file_iflush));
    gravity_class_bind(gravity_class_file, "close", NEW_CLOSURE_VALUE(internal_file_iclose));

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
