//
//  gravity_vm.h
//  gravity
//
//  Created by Marco Bambini on 11/11/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_VM__
#define __GRAVITY_VM__

#include "gravity_delegate.h"
#include "gravity_value.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GRAVITY_VM_GCENABLED            "gcEnabled"
#define GRAVITY_VM_GCMINTHRESHOLD       "gcMinThreshold"
#define GRAVITY_VM_GCTHRESHOLD          "gcThreshold"
#define GRAVITY_VM_GCRATIO              "gcRatio"
#define GRAVITY_VM_MAXCALLS             "maxCCalls"
#define GRAVITY_VM_MAXBLOCK             "maxBlock"
#define GRAVITY_VM_MAXRECURSION         "maxRecursionDepth"

typedef bool (*vm_filter_cb) (gravity_object_t *obj);
typedef void (*vm_transfer_cb) (gravity_vm *vm, gravity_object_t *obj);
typedef void (*vm_cleanup_cb) (gravity_vm *vm);

GRAVITY_API gravity_vm          *gravity_vm_new (gravity_delegate_t *delegate);
GRAVITY_API gravity_vm          *gravity_vm_newmini (void);
GRAVITY_API void                gravity_vm_set_callbacks (gravity_vm *vm, vm_transfer_cb vm_transfer, vm_cleanup_cb vm_cleanup);
GRAVITY_API void                gravity_vm_free (gravity_vm *vm);
GRAVITY_API void                gravity_vm_reset (gravity_vm *vm);
GRAVITY_API bool                gravity_vm_runclosure (gravity_vm *vm, gravity_closure_t *closure, gravity_value_t sender, gravity_value_t params[], uint16_t nparams);
GRAVITY_API bool                gravity_vm_runmain (gravity_vm *vm, gravity_closure_t *closure);
GRAVITY_API void                gravity_vm_loadclosure (gravity_vm *vm, gravity_closure_t *closure);
GRAVITY_API void                gravity_vm_setvalue (gravity_vm *vm, const char *key, gravity_value_t value);
GRAVITY_API gravity_value_t     gravity_vm_lookup (gravity_vm *vm, gravity_value_t key);
GRAVITY_API gravity_value_t     gravity_vm_getvalue (gravity_vm *vm, const char *key, uint32_t keylen);
GRAVITY_API double              gravity_vm_time (gravity_vm *vm);
GRAVITY_API gravity_value_t     gravity_vm_result (gravity_vm *vm);
GRAVITY_API gravity_delegate_t  *gravity_vm_delegate (gravity_vm *vm);
GRAVITY_API gravity_fiber_t     *gravity_vm_fiber (gravity_vm *vm);
GRAVITY_API void                gravity_vm_setfiber(gravity_vm* vm, gravity_fiber_t *fiber);
GRAVITY_API void                gravity_vm_seterror (gravity_vm *vm, const char *format, ...);
GRAVITY_API void                gravity_vm_seterror_string (gravity_vm* vm, const char *s);
GRAVITY_API bool                gravity_vm_ismini (gravity_vm *vm);
GRAVITY_API gravity_value_t     gravity_vm_keyindex (gravity_vm *vm, uint32_t index);
GRAVITY_API bool                gravity_vm_isaborted (gravity_vm *vm);
GRAVITY_API void                gravity_vm_setaborted (gravity_vm *vm);
GRAVITY_API gravity_closure_t   *gravity_vm_getclosure (gravity_vm *vm);

GRAVITY_API void                gravity_gray_value (gravity_vm* vm, gravity_value_t v);
GRAVITY_API void                gravity_gray_object (gravity_vm* vm, gravity_object_t *obj);
GRAVITY_API void                gravity_gc_start (gravity_vm* vm);
GRAVITY_API void                gravity_gc_setenabled (gravity_vm* vm, bool enabled);
GRAVITY_API void                gravity_gc_temppush (gravity_vm *vm, gravity_object_t *obj);
GRAVITY_API void                gravity_gc_temppop (gravity_vm *vm);
GRAVITY_API void                gravity_gc_tempnull (gravity_vm *vm, gravity_object_t *obj);
GRAVITY_API void                gravity_gc_setvalues (gravity_vm *vm, gravity_int_t threshold, gravity_int_t minthreshold, gravity_float_t ratio);
    
GRAVITY_API void                gravity_vm_transfer (gravity_vm* vm, gravity_object_t *obj);
GRAVITY_API void                gravity_vm_cleanup (gravity_vm* vm);
GRAVITY_API void                gravity_vm_filter (gravity_vm* vm, vm_filter_cb cleanup_filter);

GRAVITY_API gravity_closure_t   *gravity_vm_loadfile (gravity_vm *vm, const char *path);
GRAVITY_API gravity_closure_t   *gravity_vm_loadbuffer (gravity_vm *vm, const char *buffer, size_t len);
GRAVITY_API void                gravity_vm_initmodule (gravity_vm *vm, gravity_function_t *f);

GRAVITY_API gravity_closure_t   *gravity_vm_fastlookup (gravity_vm *vm, gravity_class_t *c, int index);
GRAVITY_API void                gravity_vm_setslot (gravity_vm *vm, gravity_value_t value, uint32_t index);
GRAVITY_API gravity_value_t     gravity_vm_getslot (gravity_vm *vm, uint32_t index);
GRAVITY_API void                gravity_vm_setdata (gravity_vm *vm, void *data);
GRAVITY_API void                *gravity_vm_getdata (gravity_vm *vm);
GRAVITY_API void                gravity_vm_memupdate (gravity_vm *vm, gravity_int_t value);
GRAVITY_API gravity_int_t       gravity_vm_maxmemblock (gravity_vm *vm);

GRAVITY_API gravity_value_t     gravity_vm_get (gravity_vm *vm, const char *key);
GRAVITY_API bool                gravity_vm_set (gravity_vm *vm, const char *key, gravity_value_t value);
GRAVITY_API char                *gravity_vm_anonymous (gravity_vm *vm);

GRAVITY_API bool                gravity_isopt_class (gravity_class_t *c);
GRAVITY_API void                gravity_opt_register (gravity_vm *vm);
GRAVITY_API void                gravity_opt_free (void);

#ifdef __cplusplus
}
#endif

#endif
