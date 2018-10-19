//
//  gravity_compiler.c
//  gravity
//
//  Created by Marco Bambini on 29/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_compiler.h"
#include "gravity_parser.h"
#include "gravity_token.h"
#include "gravity_utils.h"
#include "gravity_semacheck1.h"
#include "gravity_semacheck2.h"
#include "gravity_optimizer.h"
#include "gravity_codegen.h"
#include "gravity_array.h"
#include "gravity_hash.h"
#include "gravity_core.h"

struct gravity_compiler_t {
    gravity_parser_t        *parser;
    gravity_delegate_t      *delegate;
    cstring_r               *storage;
    gravity_vm              *vm;
    gnode_t                 *ast;
    void_r                  *objects;
};

static void internal_vm_transfer (gravity_vm *vm, gravity_object_t *obj) {
    gravity_compiler_t *compiler = (gravity_compiler_t *)gravity_vm_getdata(vm);
    marray_push(void*, *compiler->objects, obj);
}

static void internal_free_class (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused (hashtable, data)

    // sanity checks
    if (!VALUE_ISA_FUNCTION(value)) return;
    if (!VALUE_ISA_STRING(key)) return;

    // check for special function
    gravity_function_t *f = VALUE_AS_FUNCTION(value);
    if (f->tag == EXEC_TYPE_SPECIAL) {
        if (f->special[0]) gravity_function_free(NULL, (gravity_function_t *)f->special[0]);
        if (f->special[1]) gravity_function_free(NULL, (gravity_function_t *)f->special[1]);
    }

    // a super special init constructor is a string that begins with $init AND it is longer than strlen($init)
    gravity_string_t *s = VALUE_AS_STRING(key);
    bool is_super_function = ((s->len > 5) && (string_casencmp(s->s, CLASS_INTERNAL_INIT_NAME, 5) == 0));
    if (!is_super_function) gravity_function_free(NULL, VALUE_AS_FUNCTION(value));
}

static void internal_vm_cleanup (gravity_vm *vm) {
    gravity_compiler_t *compiler = (gravity_compiler_t *)gravity_vm_getdata(vm);
    size_t count = marray_size(*compiler->objects);
    for (size_t i=0; i<count; ++i) {
        gravity_object_t *obj = marray_pop(*compiler->objects);
        if (OBJECT_ISA_CLASS(obj)) {
            gravity_class_t *c = (gravity_class_t *)obj;
            gravity_hash_iterate(c->htable, internal_free_class, NULL);
        }
        gravity_object_free(vm, obj);
    }
}

// MARK: -

gravity_compiler_t *gravity_compiler_create (gravity_delegate_t *delegate) {
    gravity_compiler_t *compiler = mem_alloc(NULL, sizeof(gravity_compiler_t));
    if (!compiler) return NULL;

    compiler->ast = NULL;
    compiler->objects = void_array_create();
    compiler->delegate = delegate;
    return compiler;
}

static void gravity_compiler_reset (gravity_compiler_t *compiler, bool free_core) {
    // free memory for array of strings storage
    if (compiler->storage) {
        cstring_array_each(compiler->storage, {mem_free((void *)val);});
        gnode_array_free(compiler->storage);
    }

    // first ast then parser, don't change the release order
    if (compiler->ast) gnode_free(compiler->ast);
    if (compiler->parser) gravity_parser_free(compiler->parser);

    // at the end free mini VM and objects array
    if (compiler->vm) gravity_vm_free(compiler->vm);
    if (compiler->objects) {
        marray_destroy(*compiler->objects);
        mem_free((void*)compiler->objects);
    }

    // feel free to free core if someone requires it
    if (free_core) gravity_core_free();

    // reset internal pointers
    compiler->vm = NULL;
    compiler->ast = NULL;
    compiler->parser = NULL;
    compiler->objects = NULL;
    compiler->storage = NULL;
}

void gravity_compiler_free (gravity_compiler_t *compiler) {
    gravity_compiler_reset(compiler, true);
    mem_free(compiler);
}

gnode_t *gravity_compiler_ast (gravity_compiler_t *compiler) {
    return compiler->ast;
}

void gravity_compiler_transfer(gravity_compiler_t *compiler, gravity_vm *vm) {
    if (!compiler->objects) return;

    // transfer each object from compiler mini VM to exec VM
    gravity_gc_setenabled(vm, false);
    size_t count = marray_size(*compiler->objects);
    for (size_t i=0; i<count; ++i) {
        gravity_object_t *obj = marray_pop(*compiler->objects);
        gravity_vm_transfer(vm, obj);
        if (!OBJECT_ISA_CLOSURE(obj)) continue;

        // $moduleinit closure needs to be explicitly initialized
        gravity_closure_t *closure = (gravity_closure_t *)obj;
        if ((closure->f->identifier) && strcmp(closure->f->identifier, INITMODULE_NAME) == 0) {
            // code is here because it does not make sense to add this overhead (that needs to be executed only once)
            // inside the gravity_vm_transfer callback which is called for each allocated object inside the VM
            gravity_vm_initmodule(vm, closure->f);
        }
    }

    gravity_gc_setenabled(vm, true);
}

// MARK: -

gravity_closure_t *gravity_compiler_run (gravity_compiler_t *compiler, const char *source, size_t len, uint32_t fileid, bool is_static, bool add_debug) {
    if ((source == NULL) || (len == 0)) return NULL;

    // CHECK cleanup first
    if (compiler->ast) gnode_free(compiler->ast);
    if (!compiler->objects) compiler->objects = void_array_create();

    // CODEGEN requires a mini vm in order to be able to handle garbage collector
    compiler->vm = gravity_vm_newmini();
    gravity_vm_setdata(compiler->vm, (void *)compiler);
    gravity_vm_set_callbacks(compiler->vm, internal_vm_transfer, internal_vm_cleanup);
    gravity_core_register(compiler->vm);

    // STEP 0: CREATE PARSER
    compiler->parser = gravity_parser_create(source, len, fileid, is_static);
    if (!compiler->parser) return NULL;

    // STEP 1: SYNTAX CHECK
    compiler->ast = gravity_parser_run(compiler->parser, compiler->delegate);
    if (!compiler->ast) goto abort_compilation;
    gravity_parser_free(compiler->parser);
    compiler->parser = NULL;

    // STEP 2a: SEMANTIC CHECK (NON-LOCAL DECLARATIONS)
    bool b1 = gravity_semacheck1(compiler->ast, compiler->delegate);
    if (!b1) goto abort_compilation;

    // STEP 2b: SEMANTIC CHECK (LOCAL DECLARATIONS)
    bool b2 = gravity_semacheck2(compiler->ast, compiler->delegate);
    if (!b2) goto abort_compilation;

    // STEP 3: INTERMEDIATE CODE GENERATION (stack based VM)
	gravity_function_t *f = gravity_codegen(compiler->ast, compiler->delegate, compiler->vm, add_debug);
    if (!f) goto abort_compilation;

    // STEP 4: CODE GENERATION (register based VM)
	f = gravity_optimizer(f, add_debug);
    if (f) return gravity_closure_new(compiler->vm, f);

abort_compilation:
    gravity_compiler_reset(compiler, false);
    return NULL;
}

json_t *gravity_compiler_serialize (gravity_compiler_t *compiler, gravity_closure_t *closure) {
    #pragma unused(compiler)
    if (!closure) return NULL;

    json_t *json = json_new();
    json_begin_object(json, NULL);

    gravity_function_serialize(closure->f, json);

    json_end_object(json);
    return json;
}

bool gravity_compiler_serialize_infile (gravity_compiler_t *compiler, gravity_closure_t *closure, const char *path) {
    if (!closure) return false;
    json_t *json = gravity_compiler_serialize(compiler, closure);
    if (!json) return false;
    
    json_write_file(json, path);
    json_free(json);
    return true;
}
