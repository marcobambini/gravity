//
//  gravity_delegate.h
//  gravity
//
//  Created by Marco Bambini on 09/12/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_DELEGATE__
#define __GRAVITY_DELEGATE__

#include <stdint.h>
#include "gravity_value.h"

// error type and code definitions
typedef enum {
    GRAVITY_ERROR_NONE = 0,
    GRAVITY_ERROR_SYNTAX,
    GRAVITY_ERROR_SEMANTIC,
    GRAVITY_ERROR_RUNTIME,
    GRAVITY_ERROR_IO,
    GRAVITY_WARNING,
} error_type_t;

typedef struct {
    uint32_t        lineno;
    uint32_t        colno;
    uint32_t        fileid;
    uint32_t        offset;
} error_desc_t;

#define ERROR_DESC_NONE     (error_desc_t){0,0,0,0}

typedef void                (*gravity_log_callback)    (gravity_vm *vm, const char *message, void *xdata);
typedef void                (*gravity_log_clear) (gravity_vm *vm, void *xdata);
typedef void                (*gravity_error_callback) (gravity_vm *vm, error_type_t error_type, const char *description, error_desc_t error_desc, void *xdata);
typedef void                (*gravity_unittest_callback) (gravity_vm *vm, error_type_t error_type, const char *desc, const char *note, gravity_value_t value, int32_t row, int32_t col, void *xdata);
typedef void                (*gravity_parser_callback) (void *token, void *xdata);
typedef void                (*gravity_type_callback) (void *token, const char *type, void *xdata);
typedef const char*         (*gravity_precode_callback) (void *xdata);
typedef const char*			(*gravity_loadfile_callback) (const char *file, size_t *size, uint32_t *fileid, void *xdata, bool *is_static);
typedef const char*         (*gravity_filename_callback) (uint32_t fileid, void *xdata);
typedef const char**        (*gravity_optclass_callback) (void *xdata);

typedef bool                (*gravity_bridge_initinstance) (gravity_vm *vm, void *xdata, gravity_value_t ctx, gravity_instance_t *instance, gravity_value_t args[], int16_t nargs);
typedef bool                (*gravity_bridge_setvalue) (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, gravity_value_t value);
typedef bool                (*gravity_bridge_getvalue) (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, uint32_t vindex);
typedef bool                (*gravity_bridge_setundef) (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, gravity_value_t value);
typedef bool                (*gravity_bridge_getundef) (gravity_vm *vm, void *xdata, gravity_value_t target, const char *key, uint32_t vindex);
typedef bool                (*gravity_bridge_execute)  (gravity_vm *vm, void *xdata, gravity_value_t ctx, gravity_value_t args[], int16_t nargs, uint32_t vindex);
typedef bool                (*gravity_bridge_equals) (gravity_vm *vm, void *obj1, void *obj2);
typedef const char*         (*gravity_bridge_string) (gravity_vm *vm, void *xdata, uint32_t *len);
typedef void*               (*gravity_bridge_clone)  (gravity_vm *vm, void *xdata);
typedef uint32_t            (*gravity_bridge_size) (gravity_vm *vm, gravity_object_t *obj);
typedef void                (*gravity_bridge_free) (gravity_vm *vm, gravity_object_t *obj);
typedef void                (*gravity_bridge_blacken) (gravity_vm *vm, void *xdata);

typedef struct {
    // user data
    void                        *xdata;                 // optional user data transparently passed between callbacks
    bool                        report_null_errors;     // by default messages sent to null objects are silently ignored (if this flag is false)
    bool                        disable_gccheck_1;      // memory allocations are protected so it could be useful to automatically check gc when enabled is restored
    
    // callbacks
    gravity_log_callback        log_callback;           // log reporting callback
    gravity_log_clear           log_clear;              // log reset callback
    gravity_error_callback      error_callback;         // error reporting callback
    gravity_unittest_callback   unittest_callback;      // special unit test callback
    gravity_parser_callback     parser_callback;        // lexer callback used for syntax highlight
    gravity_type_callback       type_callback;          // callback used to bind a token with a declared type
    gravity_precode_callback    precode_callback;       // called at parse time in order to give the opportunity to add custom source code
    gravity_loadfile_callback   loadfile_callback;      // callback to give the opportunity to load a file from an import statement
    gravity_filename_callback   filename_callback;      // called while reporting an error in order to be able to convert a fileid to a real filename
    gravity_optclass_callback   optional_classes;       // optional classes to be exposed to the semantic checker as extern (to be later registered)

    // bridge
    gravity_bridge_initinstance bridge_initinstance;    // init class
    gravity_bridge_setvalue     bridge_setvalue;        // setter
    gravity_bridge_getvalue     bridge_getvalue;        // getter
    gravity_bridge_setundef     bridge_setundef;        // setter not found
    gravity_bridge_getundef     bridge_getundef;        // getter not found
    gravity_bridge_execute      bridge_execute;         // execute a method/function
    gravity_bridge_blacken      bridge_blacken;         // blacken obj to be GC friend
    gravity_bridge_string       bridge_string;          // instance string conversion
    gravity_bridge_equals       bridge_equals;          // check if two objects are equals
    gravity_bridge_clone        bridge_clone;           // clone
    gravity_bridge_size         bridge_size;            // size of obj
    gravity_bridge_free         bridge_free;            // free obj
} gravity_delegate_t;

#endif
