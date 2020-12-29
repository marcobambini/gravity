//
//  gravity_vm.c
//  gravity
//
//  Created by Marco Bambini on 11/11/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_hash.h"
#include "gravity_json.h"
#include "gravity_array.h"
#include "gravity_debug.h"
#include "gravity_macros.h"
#include "gravity_vm.h"
#include "gravity_core.h"
#include "gravity_opcodes.h"
#include "gravity_memory.h"
#include "gravity_vmmacros.h"
#include "gravity_optionals.h"

// MARK: Internals -
static void gravity_gc_cleanup (gravity_vm *vm);
static void gravity_gc_transfer (gravity_vm *vm, gravity_object_t *obj);
static bool vm_set_superclass (gravity_vm *vm, gravity_object_t *obj);
static void gravity_gc_transform (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t *value, void *data);

// Internal cache to speed up common operations lookup
static uint32_t cache_refcount = 0;
static gravity_value_t cache[GRAVITY_VTABLE_SIZE];

// Used in order to guarantee a NON NULL delegate (so I can speedup comparisons)
static gravity_delegate_t empty_delegate;

// Opaque VM struct
struct gravity_vm {
    gravity_hash_t      *context;                       // context hash table
    gravity_delegate_t  *delegate;                      // registered runtime delegate
    gravity_fiber_t     *fiber;                         // current fiber
    void                *data;                          // custom data optionally set by the user
    uint32_t            pc;                             // program counter
    double              time;                           // useful timer for the main function
    bool                aborted;                        // set when VM has generated a runtime error
    uint32_t            maxccalls;                      // maximum number of nested c calls
    uint32_t            nccalls;                        // current number of nested c calls

    // recursion
    gravity_int_t       maxrecursion;                   // maximum recursive depth
    gravity_int_t       recursioncount;                 // recursion counter
    
    // anonymous names
    uint32_t            nanon;                          // counter for anonymous classes (used in object_bind)
    char                temp[64];                       // temporary buffer used for anonymous names generator

    // callbacks
    vm_transfer_cb      transfer;                       // function called each time a gravity_object_t is allocated
    vm_cleanup_cb       cleanup;                        // function called when VM must be cleaned-up
    vm_filter_cb        filter;                         // function called to filter objects in the cleanup process

    // garbage collector
    int32_t             gcenabled;                      // flag to enable/disable garbage collector (was bool but it is now reference counted)
    gravity_int_t       memallocated;                   // total number of allocated memory
    gravity_int_t       maxmemblock;                    // maximum block memory size allowed to allocate
    gravity_object_t    *gchead;                        // head of garbage collected objects
    gravity_int_t       gcminthreshold;                 // minimum GC threshold size to avoid spending too much time in GC
    gravity_int_t       gcthreshold;                    // memory required to trigger a GC
    gravity_int_t       gcthreshold_original;           // gcthreshold is dynamically re-computed so I msut save the original value somewhere
    gravity_float_t     gcratio;                        // ratio used in automatic recomputation of the new gcthreshold value
    gravity_int_t       gccount;                        // number of objects into GC
    gravity_object_r    graylist;                       // array of collected objects while GC is in process (gray list)
    gravity_object_r    gctemp;                         // array of temp objects that need to be saved from GC

    // internal stats fields
    #if GRAVITY_VM_STATS
    uint32_t            nfrealloc;                      // to check how many frames reallocation occurred
    uint32_t            nsrealloc;                      // to check how many stack reallocation occurred
    uint32_t            nstat[GRAVITY_LATEST_OPCODE];   // internal used to collect opcode usage stats
    double              tstat[GRAVITY_LATEST_OPCODE];   // internal used to collect microbenchmarks
    nanotime_t          t;                              // internal timer
    #endif
};

// MARK: - Debug -

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static void gravity_stack_dump (gravity_fiber_t *fiber) {
    uint32_t index = 0;
    for (gravity_value_t *stack = fiber->stack; stack < fiber->stacktop; ++stack) {
        printf("[%05d]\t", index++);
        if (!stack->isa) {printf("\n"); continue;}
        gravity_value_dump(NULL, *stack, NULL, 0);
    }
    if (index) printf("\n\n");
}

static void gravity_callframe_dump (gravity_fiber_t *fiber) {
    printf("===========================\n");
    printf("CALL FRAME\n");
    printf("===========================\n");
    for (uint32_t i = 0; i < fiber->nframes; ++i) {
        gravity_callframe_t *frame = &fiber->frames[i];
        const char *fname = (frame->closure->f->identifier) ? frame->closure->f->identifier : "N/A";
        gravity_value_t self_value = frame->stackstart[0];
        char buffer[256];
        gravity_value_dump(NULL, self_value, buffer, sizeof(buffer));
        printf("[%03d]\t%s\t(%s)\n", i, fname, buffer);
    }
}

static uint32_t gravity_vm_lineno (gravity_vm *vm) {
    // get current fiber
    gravity_fiber_t *fiber = vm->fiber;
    
    // get current call frame
    gravity_callframe_t *frame = (fiber->nframes) ? &fiber->frames[fiber->nframes-1] : NULL;
    if (!frame) return 0;
    
    // get current executing function
    gravity_function_t *func = (frame->closure) ? frame->closure->f : NULL;
    if (!func) return 0;
    
    // sanity check about function type and included debug information
    if (func->tag == EXEC_TYPE_NATIVE && func->lineno) {
        uint32_t nindex = 0;
        if (frame->ip > func->bytecode) {
            // -1 because frame->ip points to the next instruction to execute
            nindex = (uint32_t)(frame->ip - func->bytecode) - 1;
        }
        
        return func->lineno[nindex];
    }
    
    return 0;
}


#pragma clang diagnostic pop

// MARK: -

static void report_runtime_error (gravity_vm *vm, error_type_t error_type, const char *format, ...) {
    char        buffer[1024];
    va_list        arg;

    if (vm->aborted) return;
    vm->aborted = true;

    if (format) {
        va_start (arg, format);
        vsnprintf(buffer, sizeof(buffer), format, arg);
        va_end (arg);
    }

    gravity_error_callback error_cb = ((gravity_delegate_t *)vm->delegate)->error_callback;
    if (error_cb) {
        uint32_t lineno = gravity_vm_lineno(vm);
        error_desc_t edesc = (error_desc_t){lineno, 0, 0, 0};
        void *data = ((gravity_delegate_t *)vm->delegate)->xdata;
		error_cb(vm, error_type, buffer, edesc, data);
    } else {
        printf("%s\n", buffer);
        fflush(stdout);
    }
    
    // gravity_callframe_dump(vm->fiber);
}

static void gravity_cache_setup (void) {
    ++cache_refcount;

    // NULL here because I do not want them to be in the GC
    mem_check(false);
    cache[GRAVITY_NOTFOUND_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_INTERNAL_NOTFOUND_NAME);
    cache[GRAVITY_ADD_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_ADD_NAME);
    cache[GRAVITY_SUB_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_SUB_NAME);
    cache[GRAVITY_DIV_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_DIV_NAME);
    cache[GRAVITY_MUL_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_MUL_NAME);
    cache[GRAVITY_REM_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_REM_NAME);
    cache[GRAVITY_AND_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_AND_NAME);
    cache[GRAVITY_OR_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_OR_NAME);
    cache[GRAVITY_CMP_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_CMP_NAME);
    cache[GRAVITY_EQQ_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_EQQ_NAME);
    cache[GRAVITY_IS_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_IS_NAME);
    cache[GRAVITY_MATCH_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_MATCH_NAME);
    cache[GRAVITY_NEG_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_NEG_NAME);
    cache[GRAVITY_NOT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_NOT_NAME);
    cache[GRAVITY_LSHIFT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_LSHIFT_NAME);
    cache[GRAVITY_RSHIFT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_RSHIFT_NAME);
    cache[GRAVITY_BAND_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_BAND_NAME);
    cache[GRAVITY_BOR_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_BOR_NAME);
    cache[GRAVITY_BXOR_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_BXOR_NAME);
    cache[GRAVITY_BNOT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_OPERATOR_BNOT_NAME);
    cache[GRAVITY_LOAD_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_INTERNAL_LOAD_NAME);
    cache[GRAVITY_LOADS_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_INTERNAL_LOADS_NAME);
    cache[GRAVITY_LOADAT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_INTERNAL_LOADAT_NAME);
    cache[GRAVITY_STORE_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_INTERNAL_STORE_NAME);
    cache[GRAVITY_STOREAT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_INTERNAL_STOREAT_NAME);
    cache[GRAVITY_INT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_CLASS_INT_NAME);
    cache[GRAVITY_FLOAT_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_CLASS_FLOAT_NAME);
    cache[GRAVITY_BOOL_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_CLASS_BOOL_NAME);
    cache[GRAVITY_STRING_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_CLASS_STRING_NAME);
    cache[GRAVITY_EXEC_INDEX] = VALUE_FROM_CSTRING(NULL, GRAVITY_INTERNAL_EXEC_NAME);
    mem_check(true);
}

static void gravity_cache_free (void) {
    --cache_refcount;
    if (cache_refcount > 0) return;

    mem_check(false);
    for (uint32_t index = 0; index <GRAVITY_VTABLE_SIZE; ++index) {
        gravity_value_free(NULL, cache[index]);
    }
    mem_check(true);
}

gravity_value_t gravity_vm_keyindex (gravity_vm *vm, uint32_t index) {
    #pragma unused (vm)
    return cache[index];
}

static inline gravity_callframe_t *gravity_new_callframe (gravity_vm *vm, gravity_fiber_t *fiber) {
    #pragma unused(vm)

    // check if there are enough slots in the call frame and optionally create new cframes
    if (fiber->framesalloc - fiber->nframes < 1) {
        uint32_t new_size = fiber->framesalloc * 2;
        void *ptr = mem_realloc(NULL, fiber->frames, sizeof(gravity_callframe_t) * new_size);
        if (!ptr) {
            // frames reallocation failed means that there is a very high probability to be into an infinite loop
            report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, "Infinite loop detected. Current execution must be aborted.");
            return NULL;
        }
        fiber->frames = (gravity_callframe_t *)ptr;
        fiber->framesalloc = new_size;
        STAT_FRAMES_REALLOCATED(vm);
    }

    // update frames counter
    ++fiber->nframes;

    // return first available cframe (-1 because I just updated the frames counter)
    return &fiber->frames[fiber->nframes - 1];
}

static inline bool gravity_check_stack (gravity_vm *vm, gravity_fiber_t *fiber, uint32_t stacktopdelta, gravity_value_t **stackstart) {
    #pragma unused(vm)
	if (stacktopdelta == 0) return true;
	
    // update stacktop pointer before a call
    fiber->stacktop += stacktopdelta;
	
    // check stack size
	uint32_t stack_size = (uint32_t)(fiber->stacktop - fiber->stack);
    uint32_t stack_needed = MAXNUM(stack_size, DEFAULT_MINSTACK_SIZE);
    if (fiber->stackalloc >= stack_needed) return true;
    gravity_value_t *old_stack = fiber->stack;

    // perform stack reallocation (power_of2_ceil returns 0 if argument is bigger than 2^31)
    uint32_t new_size = power_of2_ceil(fiber->stackalloc + stack_needed);
    bool size_condition = (new_size && (uint64_t)new_size >= (uint64_t)(fiber->stackalloc + stack_needed) && ((sizeof(gravity_value_t) * new_size) < SIZE_MAX));
    void *ptr = (size_condition) ? mem_realloc(NULL, fiber->stack, sizeof(gravity_value_t) * new_size) : NULL;
    if (!ptr) {
        // restore stacktop to previous state
        fiber->stacktop -= stacktopdelta;

        // stack reallocation failed means that there is a very high probability to be into an infinite loop
        // so return false and let the calling function (vm_exec) raise a runtime error
        return false;
    }
    
    fiber->stack = (gravity_value_t *)ptr;
    fiber->stackalloc = new_size;
    STAT_STACK_REALLOCATED(vm);

    // check if reallocation moved the stack
    if (fiber->stack == old_stack) return true;

    // re-compute ptr offset
    ptrdiff_t offset = (ptrdiff_t)(fiber->stack - old_stack);

    // adjust stack pointer for each call frame
    for (uint32_t i=0; i < fiber->nframes; ++i) {
        fiber->frames[i].stackstart += offset;
    }

    // adjust upvalues ptr offset
    gravity_upvalue_t *upvalue = fiber->upvalues;
    while (upvalue) {
        upvalue->value += offset;
        upvalue = upvalue->next;
    }

    // adjust fiber stack pointer
    fiber->stacktop += offset;

    // stack is changed so update currently used stackstart
    *stackstart += offset;

    return true;
}

static gravity_upvalue_t *gravity_capture_upvalue (gravity_vm *vm, gravity_fiber_t *fiber, gravity_value_t *value) {
    // closures and upvalues implementation inspired by Lua and Wren
     // fiber->upvalues list must be ORDERED by the level of the corresponding variables in the stack starting from top

    // if upvalues is empty then create it
    if (!fiber->upvalues) {
        fiber->upvalues = gravity_upvalue_new(vm, value);
        return fiber->upvalues;
    }

    // scan list looking for upvalue first (keeping track of the order)
    gravity_upvalue_t *prevupvalue = NULL;
    gravity_upvalue_t *upvalue = fiber->upvalues;
    while (upvalue && upvalue->value > value) {
        prevupvalue = upvalue;
        upvalue = upvalue->next;
    }

    // if upvalue found then re-use it
    if (upvalue != NULL && upvalue->value == value) return upvalue;

    // upvalue not found in list so creates a new one and add it in list ORDERED
    gravity_upvalue_t *newvalue = gravity_upvalue_new(vm, value);
    if (prevupvalue == NULL) fiber->upvalues = newvalue;
    else prevupvalue->next = newvalue;

    // returns newly created upvalue
    newvalue->next = upvalue;
    
    return newvalue;
}

static void gravity_close_upvalues (gravity_fiber_t *fiber, gravity_value_t *level) {
    while (fiber->upvalues != NULL && fiber->upvalues->value >= level) {
        gravity_upvalue_t *upvalue = fiber->upvalues;

        // move the value into the upvalue itself and point the upvalue to it
        upvalue->closed = *upvalue->value;
        upvalue->value = &upvalue->closed;

        // remove it from the open upvalue list
        fiber->upvalues = upvalue->next;
    }
}

static void gravity_vm_loadclass (gravity_vm *vm, gravity_class_t *c) {
    // convert func to closure for class and for its meta class
    gravity_hash_transform(c->htable, gravity_gc_transform, (void *)vm);
    gravity_class_t *meta = gravity_class_get_meta(c);
    gravity_hash_transform(meta->htable, gravity_gc_transform, (void *)vm);
}

// MARK: - Optionals -

void gravity_opt_register (gravity_vm *vm) {
    GRAVITY_MATH_REGISTER(vm);
    GRAVITY_ENV_REGISTER(vm);
    GRAVITY_JSON_REGISTER(vm);
    GRAVITY_FILE_REGISTER(vm);
}

void gravity_opt_free() {
    GRAVITY_MATH_FREE();
    GRAVITY_ENV_FREE();
    GRAVITY_JSON_FREE();
    GRAVITY_FILE_FREE();
}

bool gravity_isopt_class (gravity_class_t *c) {
    return (GRAVITY_ISMATH_CLASS(c)) || (GRAVITY_ISENV_CLASS(c) || (GRAVITY_ISJSON_CLASS(c)) || (GRAVITY_ISFILE_CLASS(c)));
}

// MARK: - MAIN EXECUTION -

static bool gravity_vm_exec (gravity_vm *vm) {
    DECLARE_DISPATCH_TABLE;

    gravity_fiber_t         *fiber = vm->fiber;         // current fiber
    gravity_delegate_t      *delegate = vm->delegate;   // current delegate
    gravity_callframe_t     *frame;                     // current executing frame
    gravity_function_t      *func;                      // current executing function
    gravity_value_t         *stackstart;                // SP => stack pointer
    register uint32_t       *ip;                        // IP => instruction pointer
    register uint32_t       inst;                       // IR => instruction register
    register opcode_t       op;                         // OP => opcode register

    // load current callframe
    LOAD_FRAME();
    DEBUG_CALL("Executing", func);

    // sanity check
    if ((ip == NULL) || (!func->bytecode) || (func->ninsts == 0)) return true;

    DEBUG_STACK();

    while (1) {
        INTERPRET_LOOP {

            // MARK: - OPCODES -
            // MARK: NOP
            CASE_CODE(NOP): {
                DEBUG_VM("NOP");
                DISPATCH();
            }

            // MARK: MOVE
            CASE_CODE(MOVE): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
                DEBUG_VM("MOVE %d %d", r1, r2);

                SETVALUE(r1, STACK_GET(r2));
                DISPATCH();
            }
            
            // MARK: LOADS
            CASE_CODE(LOADS): {
                OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
                DEBUG_VM("LOADS %d %d %d", r1, r2, r3);
                
                // r1 result
                // r2 superclass to lookup (target implicit to self)
                // r3 key
                
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_INDEX_VARIABLE(v3,r3);
                
                gravity_value_t target = STACK_GET(0);
                gravity_class_t *target_class = gravity_value_getclass(target);
                gravity_class_t *super_target = gravity_class_lookup_class_identifier(target_class, VALUE_AS_CSTRING(v2));
                if (!super_target) RUNTIME_ERROR("Unable to find superclass %s in self object", VALUE_AS_CSTRING(v2));
                
                gravity_object_t *result = gravity_class_lookup(super_target, v3);
                if (!result) RUNTIME_ERROR("Unable to find %s in superclass %s", VALUE_AS_CSTRING(v3), (super_target->identifier) ? super_target->identifier : "N/A");
                
                SETVALUE(r1, VALUE_FROM_OBJECT(result));
                DISPATCH();
            }

            // MARK: LOAD
            // MARK: LOADAT
            CASE_CODE(LOAD):
            CASE_CODE(LOADAT):{
                OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
                DEBUG_VM("%s %d %d %d", (op == LOAD) ? "LOAD" : ((op == LOADAT) ? "LOADAT" : "LOADS"), r1, r2, r3);

                // r1 result
                // r2 target
                // r3 key

                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_INDEX_VARIABLE(v3,r3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, (op == LOAD) ? GRAVITY_LOAD_INDEX : ((op == LOADAT) ? GRAVITY_LOADAT_INDEX : GRAVITY_LOADS_INDEX), rwin);

                // save currently executing fiber (that can change!)
                gravity_fiber_t *current_fiber = fiber;
                
                // call closure (do not use a macro here because we want to handle both the bridged and special cases)
                STORE_FRAME();
                execute_load_function:
                switch(closure->f->tag) {
                    case EXEC_TYPE_NATIVE: {
                        // invalidate current_fiber because it does not need to be in sync in this case
                        current_fiber = NULL;
                        PUSH_FRAME(closure, &stackstart[rwin], r1, 2);
                    } break;

                    case EXEC_TYPE_INTERNAL: {
                        // backup register r1 because it can be overwrite to return a closure
                        gravity_value_t r1copy = STACK_GET(r1);
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = closure->f->internal(vm, &stackstart[rwin], 2, r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (vm->aborted) return false;

                            // check for special getter trick
                            if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {
                                closure = VALUE_AS_CLOSURE(STACK_GET(r1));
                                SETVALUE(r1, r1copy);
                                goto execute_load_function;
                            }

                            // check for special fiber error
                            fiber = vm->fiber;
                            if (fiber == NULL) return true;
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_BRIDGED: {
                        DEBUG_ASSERT(delegate->bridge_getvalue, "bridge_getvalue delegate callback is mandatory");
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = delegate->bridge_getvalue(vm, closure->f->xdata, v2, VALUE_AS_CSTRING(v3), r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_SPECIAL: {
                        if (!closure->f->special[EXEC_TYPE_SPECIAL_GETTER]) RUNTIME_ERROR("Missing special getter function for property %s", VALUE_AS_CSTRING(v3));
                        closure = closure->f->special[EXEC_TYPE_SPECIAL_GETTER];
                        goto execute_load_function;
                    } break;
                }
                LOAD_FRAME();
                SYNC_STACKTOP(current_fiber, fiber, stacktopdelta);

                // continue execution
                DISPATCH();
            }

            // MARK: LOADI
            CASE_CODE(LOADI): {
                OPCODE_GET_ONE8bit_SIGN_ONE17bit(inst, const uint32_t r1, const int32_t value);
                DEBUG_VM("LOADI %d %d", r1, value);

                SETVALUE_INT(r1, value);
                DISPATCH();
            }

            // MARK: LOADK
            CASE_CODE(LOADK): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t index);
                DEBUG_VM("LOADK %d %d", r1, index);

                // constant pool case
                if (index < marray_size(func->cpool)) {
                    gravity_value_t v = gravity_function_cpool_get(func, index);
                    SETVALUE(r1, v);
                    DISPATCH();
                }

                // special value case
                switch (index) {
                    case CPOOL_VALUE_SUPER: {
                        // get super class from STACK_GET(0) which is self
                        gravity_class_t *super = gravity_value_getsuper(STACK_GET(0));
                        SETVALUE(r1, (super) ? VALUE_FROM_OBJECT(super) : VALUE_FROM_NULL);
                    } break;
                    case CPOOL_VALUE_ARGUMENTS: SETVALUE(r1, (frame->args) ? VALUE_FROM_OBJECT(frame->args) : VALUE_FROM_NULL); break;
                    case CPOOL_VALUE_NULL: SETVALUE(r1, VALUE_FROM_NULL); break;
                    case CPOOL_VALUE_UNDEFINED: SETVALUE(r1, VALUE_FROM_UNDEFINED); break;
                    case CPOOL_VALUE_TRUE: SETVALUE(r1, VALUE_FROM_TRUE); break;
                    case CPOOL_VALUE_FALSE: SETVALUE(r1, VALUE_FROM_FALSE); break;
                    case CPOOL_VALUE_FUNC: SETVALUE(r1, VALUE_FROM_OBJECT(frame->closure)); break;
                    default: RUNTIME_ERROR("Unknown LOADK index"); break;
                }
                DISPATCH();
            }

            // MARK: LOADG
            CASE_CODE(LOADG): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, uint32_t r1, int32_t index);
                DEBUG_VM("LOADG %d %d", r1, index);

                gravity_value_t key = gravity_function_cpool_get(func, index);
                gravity_value_t *v = gravity_hash_lookup(vm->context, key);
                if (!v) RUNTIME_ERROR("Unable to find object %s", VALUE_AS_CSTRING(key));

                SETVALUE(r1, *v);
                DISPATCH();
            }

            // MARK: LOADU
            CASE_CODE(LOADU): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
                DEBUG_VM("LOADU %d %d", r1, r2);

                gravity_upvalue_t *upvalue = frame->closure->upvalue[r2];
                SETVALUE(r1, *upvalue->value);
                DISPATCH();
            }

            // MARK: STORE
            // MARK: STOREAT
            CASE_CODE(STORE):
            CASE_CODE(STOREAT):{
                OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
                DEBUG_VM("%s %d %d %d", (op == STORE) ? "STORE" : "STOREAT", r1, r2,r3);

                // r1 value
                // r2 target
                // r3 key

                DEFINE_STACK_VARIABLE(v1,r1);
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_INDEX_VARIABLE(v3,r3);

                // prepare function call
                PREPARE_FUNC_CALL3(closure, v2, v3, v1, (op == STORE) ? GRAVITY_STORE_INDEX : GRAVITY_STOREAT_INDEX, rwin);

                // save currently executing fiber (that can change!)
                gravity_fiber_t *current_fiber = fiber;
                
                // in case of computed property what happens is that EXEC_TYPE_INTERNAL returns false
                // and the initial closure variable is overwritten by the closure to be executed returned
                // in the r1 registed. The goto execute_store_function take cares of re-executing the code
                // with the updated closure (that contains the computed property code) but the r1 register
                // must be re-set with its initial value backed-up in the r1copy variable
                bool reset_r1 = false;
                
                // call function f (do not use a macro here because we want to handle both the bridged and special cases)
                STORE_FRAME();
                execute_store_function:
                switch(closure->f->tag) {
                    case EXEC_TYPE_NATIVE: {
                        // invalidate current_fiber because it does not need to be in sync in this case
                        current_fiber = NULL;
                        if (reset_r1) {SETVALUE(rwin+1, v1); reset_r1 = false;}
                        // was r1 but it was incorrect, pass r3 as the destination register because I am sure it is a
                        // dummy temp (and unused) register that can be safely set to NULL
                        PUSH_FRAME(closure, &stackstart[rwin], r3, 2);
                    } break;

                    case EXEC_TYPE_INTERNAL: {
                        // backup register r1 because it can be overwrite to return a closure
                        gravity_value_t r1copy = STACK_GET(r1);
                        if (reset_r1) {SETVALUE(rwin+1, r1copy);  reset_r1 = false;}
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = closure->f->internal(vm, &stackstart[rwin], 2, r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (vm->aborted) return false;

                            // check for special setter trick
                            if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {
                                closure = VALUE_AS_CLOSURE(STACK_GET(r1));
                                SETVALUE(r1, r1copy);
                                reset_r1 = true;
                                goto execute_store_function;
                            }

                            // check for special fiber error
                            fiber = vm->fiber;
                            if (fiber == NULL) return true;
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_BRIDGED: {
                        DEBUG_ASSERT(delegate->bridge_setvalue, "bridge_setvalue delegate callback is mandatory");
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = delegate->bridge_setvalue(vm, closure->f->xdata, v2, VALUE_AS_CSTRING(v3), v1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_SPECIAL: {
                        if (!closure->f->special[EXEC_TYPE_SPECIAL_SETTER]) RUNTIME_ERROR("Missing special setter function for property %s", VALUE_AS_CSTRING(v3));
                        closure = closure->f->special[EXEC_TYPE_SPECIAL_SETTER];
                        goto execute_store_function;
                    } break;
                }
                LOAD_FRAME();
                SYNC_STACKTOP(current_fiber, fiber, stacktopdelta);

                // continue execution
                DISPATCH();
            }

            // MARK: STOREG
            CASE_CODE(STOREG): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, uint32_t r1, int32_t index);
                DEBUG_VM("STOREG %d %d", r1, index);

                gravity_value_t key = gravity_function_cpool_get(func, index);
                gravity_value_t v = STACK_GET(r1);
                gravity_hash_insert(vm->context, key, v);
                DISPATCH();
            }

            // MARK: STOREU
            CASE_CODE(STOREU): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
                DEBUG_VM("STOREU %d %d", r1, r2);

                gravity_upvalue_t *upvalue = frame->closure->upvalue[r2];
                *upvalue->value = STACK_GET(r1);
                DISPATCH();
            }

            // MARK: - EQUALITY
            CASE_CODE(EQQ):
            CASE_CODE(NEQQ): {
                // decode operation
                DECODE_BINARY_OPERATION(r1,r2,r3);

                // get registers
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_STACK_VARIABLE(v3,r3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_EQQ_INDEX, rwin);

                // call function f
                CALL_FUNC(EQQ, closure, r1, 2, rwin);

                // get result of the EQQ selector
                register gravity_int_t result = STACK_GET(r1).n;

                // save result (NEQQ is not EQQ)
                SETVALUE_BOOL(r1, (op == EQQ) ? result : !result);

                DISPATCH();
            }

            CASE_CODE(ISA):
            CASE_CODE(MATCH): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // get registers
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_STACK_VARIABLE(v3,r3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, (op == ISA) ? GRAVITY_IS_INDEX : GRAVITY_MATCH_INDEX, rwin);

                // call function f
                CALL_FUNC(ISA, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: COMPARISON
            CASE_CODE(LT):
            CASE_CODE(GT):
            CASE_CODE(EQ):
            CASE_CODE(LEQ):
            CASE_CODE(GEQ):
            CASE_CODE(NEQ): {

                // decode operation
                DECODE_BINARY_OPERATION(r1,r2,r3);

                // check fast comparison only if both values are boolean OR if one of them is undefined
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_STACK_VARIABLE(v3,r3);
                if ((VALUE_ISA_BOOL(v2) && (VALUE_ISA_BOOL(v3))) || (VALUE_ISA_UNDEFINED(v2) || (VALUE_ISA_UNDEFINED(v3)))) {
                    register gravity_int_t eq_result = (v2.isa == v3.isa) && (v2.n == v3.n);
                    SETVALUE(r1, VALUE_FROM_BOOL((op == EQ) ? eq_result : !eq_result));
                    DISPATCH();
                } else if (VALUE_ISA_INT(v2) && VALUE_ISA_INT(v3)) {
                    // INT optimization especially useful in loops
                    if (v2.n == v3.n) SETVALUE(r1, VALUE_FROM_INT(0));
                    else SETVALUE(r1, VALUE_FROM_INT((v2.n > v3.n) ? 1 : -1));
                } else {
                    // prepare function call for binary operation
                    PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_CMP_INDEX, rwin);

                    // call function f
                    CALL_FUNC(CMP, closure, r1, 2, rwin);
                }

                // compare returns 0 if v1 and v2 are equals, 1 if v1>v2 and -1 if v1<v2
                register gravity_int_t result = STACK_GET(r1).n;

                switch(op) {
                    case LT: SETVALUE_BOOL(r1, result < 0); break;
                    case GT: SETVALUE_BOOL(r1, result > 0); break;
                    case EQ: SETVALUE_BOOL(r1, result == 0); break;
                    case LEQ: SETVALUE_BOOL(r1, result <= 0); break;
                    case GEQ: SETVALUE_BOOL(r1, result >= 0); break;
                    case NEQ: SETVALUE_BOOL(r1, result != 0); break;
                    default: assert(0);
                }

                // optimize the case where after a comparison there is a JUMPF instruction (usually in a loop)
                uint32_t inext = *ip++;
                if ((STACK_GET(r1).n == 0) && (OPCODE_GET_OPCODE(inext) == JUMPF)) {
                    OPCODE_GET_LAST18bit(inext, int32_t value);
                    DEBUG_VM("JUMPF %d %d", (int)result, value);
                    ip = COMPUTE_JUMP(value); // JUMP is an absolute value
                    DISPATCH();
                }

                // JUMPF not executed so I need to go back in instruction pointer
                --ip;

                // continue execution
                DISPATCH();
            }

            // MARK: - BIT OPERATORS -
            // MARK: LSHIFT
            CASE_CODE(LSHIFT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, <<);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_LSHIFT_INDEX, rwin);

                // call function f
                CALL_FUNC(LSHIFT, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: RSHIFT
            CASE_CODE(RSHIFT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, >>);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_RSHIFT_INDEX, rwin);

                // call function f
                CALL_FUNC(RSHIFT, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: BAND
            CASE_CODE(BAND): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, &);
                CHECK_FAST_BINBOOL_BIT(r1, v2, v3, &);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_BAND_INDEX, rwin);

                // call function f
                CALL_FUNC(BAND, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: BOR
            CASE_CODE(BOR): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, |);
                CHECK_FAST_BINBOOL_BIT(r1, v2, v3, |);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_BOR_INDEX, rwin);

                // call function f
                CALL_FUNC(BOR, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: BXOR
            CASE_CODE(BXOR):{
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, ^);
                CHECK_FAST_BINBOOL_BIT(r1, v2, v3, ^);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_BXOR_INDEX, rwin);

                // call function f
                CALL_FUNC(BXOR, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: - BINARY OPERATORS -
            // MARK: ADD
            CASE_CODE(ADD): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast math operation first (only in case of int and float)
                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, +, NO_CHECK);

                // fast math operation cannot be performed so let's try with a regular call
                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_ADD_INDEX, rwin);

                // call function f
                CALL_FUNC(ADD, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: SUB
            CASE_CODE(SUB): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast math operation first (only in case of int and float)
                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, -, NO_CHECK);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_SUB_INDEX, rwin);

                // call function f
                CALL_FUNC(SUB, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: DIV
            CASE_CODE(DIV): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast math operation first  (only in case of int and float)
                // a special check macro is added in order to check for divide by zero cases
                #if defined(__clang__)
                    #pragma clang diagnostic push
                    #pragma clang diagnostic ignored "-Wdivision-by-zero"
                #elif defined(__GNUC__)
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wdiv-by-zero"
				#elif defined(_MSC_VER)
					#pragma warning (push)
					#pragma warning (disable: 4723)
                #endif
                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, /, CHECK_ZERO(v3));
                #if defined(__clang__)
                    #pragma clang diagnostic pop
                #elif defined(__GNUC__)
                    #pragma GCC diagnostic pop
				#elif defined(_MSC_VER)
					#pragma warning (pop)
                #endif

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_DIV_INDEX, rwin);

                // call function f
                CALL_FUNC(DIV, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: MUL
            CASE_CODE(MUL): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast math operation first (only in case of int and float)
                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, *, NO_CHECK);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_MUL_INDEX, rwin);

                // call function f
                CALL_FUNC(MUL, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: REM
            CASE_CODE(REM): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast math operation first (only in case of int and float)
                CHECK_FAST_BINARY_REM(r1, r2, r3, v2, v3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_REM_INDEX, rwin);

                // call function f
                CALL_FUNC(REM, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: AND
            CASE_CODE(AND): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bool operation first (only if both are bool)
                CHECK_FAST_BINARY_BOOL(r1, r2, r3, v2, v3, &&);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_AND_INDEX, rwin);

                // call function f
                CALL_FUNC(AND, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: OR
            CASE_CODE(OR): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bool operation first (only if both are bool)
                CHECK_FAST_BINARY_BOOL(r1, r2, r3, v2, v3, ||);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, GRAVITY_OR_INDEX, rwin);

                // call function f
                CALL_FUNC(OR, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: - UNARY OPERATORS -
            // MARK: NEG
            CASE_CODE(NEG): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);
                #pragma unused(r3)

                // check fast bool operation first (only if it is int or float)
                CHECK_FAST_UNARY_MATH(r1, r2, v2, -);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL1(closure, v2, GRAVITY_NEG_INDEX, rwin);

                // call function f
                CALL_FUNC(NEG, closure, r1, 1, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: NOT
            CASE_CODE(NOT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);
                #pragma unused(r3)

                // check fast bool operation first  (only if it is bool)
                CHECK_FAST_UNARY_BOOL(r1, r2, v2, !);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL1(closure, v2, GRAVITY_NOT_INDEX, rwin);

                // call function f
                CALL_FUNC(NOT, closure, r1, 1, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: BNOT
            CASE_CODE(BNOT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);
                #pragma unused(r3)

                // check fast int operation first
                DEFINE_STACK_VARIABLE(v2,r2);
                // ~v2.n == -v2.n-1
                if (VALUE_ISA_INT(v2)) {SETVALUE(r1, VALUE_FROM_INT(~v2.n)); DISPATCH();}

                // prepare function call for binary operation
                PREPARE_FUNC_CALL1(closure, v2, GRAVITY_BNOT_INDEX, rwin);

                // call function f
                CALL_FUNC(BNOT, closure, r1, 1, rwin);

                // continue execution
                DISPATCH();
            }

            // MARK: -
            // MARK: JUMPF
            CASE_CODE(JUMPF): {
                // JUMPF like JUMP is an absolute value
                OPCODE_GET_ONE8bit_FLAG_ONE17bit(inst, const uint32_t r1, const uint32_t flag, const int32_t value);
                DEBUG_VM("JUMPF %d %d (flag %d)", r1, value, flag);

                if (flag) {
                    // if flag is set then ONLY boolean values must be checked (compiler must guarantee this condition)
                    // this is necessary in a FOR loop over numeric values (with an iterator) where otherwise number 0
                    // could be computed as a false condition
                    if ((VALUE_ISA_BOOL(STACK_GET(r1))) && (GETVALUE_INT(STACK_GET(r1)) == 0)) ip = COMPUTE_JUMP(value);
                    DISPATCH();
                }

                // no flag set so convert r1 to BOOL
                DEFINE_STACK_VARIABLE(v1, r1);

                // common NULL/UNDEFINED/BOOL/INT/FLOAT/STRING cases
                // NULL/UNDEFINED   =>    no check
                // BOOL/INT         =>    check n
                // FLOAT            =>    check f
                // STRING           =>    check len
                if (VALUE_ISA_NULL(v1) || (VALUE_ISA_UNDEFINED(v1))) {ip = COMPUTE_JUMP(value);}
                else if (VALUE_ISA_BOOL(v1) || (VALUE_ISA_INT(v1))) {if (GETVALUE_INT(v1) == 0) ip = COMPUTE_JUMP(value);}
                else if (VALUE_ISA_FLOAT(v1)) {if (GETVALUE_FLOAT(v1) == 0.0) ip = COMPUTE_JUMP(value);}
                else if (VALUE_ISA_STRING(v1)) {if (VALUE_AS_STRING(v1)->len == 0) ip = COMPUTE_JUMP(value);}
                else {
                    // no common case so check if it implements the Bool command
                    gravity_closure_t *closure = (gravity_closure_t *)gravity_class_lookup_closure(gravity_value_getclass(v1), cache[GRAVITY_BOOL_INDEX]);

                    // if no closure is found then object is considered TRUE
                    if (closure) {
                        // prepare func call
                        uint32_t rwin = FN_COUNTREG(func, frame->nargs);
                        uint32_t _rneed = FN_COUNTREG(closure->f, 1);
						uint32_t stacktopdelta = (uint32_t)MAXNUM(stackstart + rwin + _rneed - fiber->stacktop, 0);
                        if (!gravity_check_stack(vm, fiber, stacktopdelta, &stackstart)) {
                            RUNTIME_ERROR("Infinite loop detected. Current execution must be aborted.");
                        }
                        SETVALUE(rwin, v1);

                        // call func and check result
                        CALL_FUNC(JUMPF, closure, rwin, 2, rwin);
                        gravity_int_t result = STACK_GET(rwin).n;

                        // re-compute ip ONLY if false
                        if (!result) ip = COMPUTE_JUMP(value);
                    }
                }
                DISPATCH();
            }

            // MARK: JUMP
            CASE_CODE(JUMP): {
                OPCODE_GET_ONE26bit(inst, const uint32_t value);
                DEBUG_VM("JUMP %d", value);

                ip = COMPUTE_JUMP(value); // JUMP is an absolute value
                DISPATCH();
            }

            // MARK: CALL
            CASE_CODE(CALL): {
                // CALL A B C => R(A) = B(B+1...B+C)
                OPCODE_GET_THREE8bit(inst, const uint32_t r1, const uint32_t r2, register uint32_t r3);
                DEBUG_VM("CALL %d %d %d", r1, r2, r3);

                DEBUG_STACK();

                // r1 is the destination register
                // r2 is the register which contains the callable object
                // r3 is the number of arguments (nparams)

                // sliding register window as in:
                // https://the-ravi-programming-language.readthedocs.io/en/latest/lua-parser.html#sliding-register-window-by-mike-pall
                const uint32_t rwin = r2 + 1;

                // object to call
                gravity_value_t v = STACK_GET(r2);

                // closure to call
                gravity_closure_t *closure = NULL;

                if (VALUE_ISA_CLOSURE(v)) {
                    closure = VALUE_AS_CLOSURE(v);
                } else {
                    // check for exec closure inside object
                    closure = (gravity_closure_t *)gravity_class_lookup_closure(gravity_value_getclass(v), cache[GRAVITY_EXEC_INDEX]);
                }

                // sanity check
                if (!closure) RUNTIME_ERROR("Unable to call object (in function %s)", func->identifier);

                // check stack size
                uint32_t _rneed = FN_COUNTREG(closure->f, r3);
				uint32_t stacktopdelta = (uint32_t)MAXNUM(stackstart + rwin + _rneed - fiber->stacktop, 0);
                if (!gravity_check_stack(vm, fiber, stacktopdelta, &stackstart)) {
                    RUNTIME_ERROR("Infinite loop detected. Current execution must be aborted.");
                }

                // if less arguments are passed then fill the holes with UNDEFINED values
                while (r3 < closure->f->nparams) {
                    SETVALUE(rwin+r3, VALUE_FROM_UNDEFINED);
                    ++r3;
                }
                
                if (VALUE_ISA_CLASS(v)) {
                    // set self as class for class_exec()
                    SETVALUE(rwin, v);
                } else if (closure->context) {
                    // check for closure autocaptured self context (or custom self set by the user)
                    SETVALUE(rwin, VALUE_FROM_OBJECT(closure->context));
                }
                
                DEBUG_STACK();

                // save currently executing fiber (that can change!)
                gravity_fiber_t *current_fiber = fiber;
                
                // execute function
                STORE_FRAME();
                execute_call_function:
                switch(closure->f->tag) {
                    case EXEC_TYPE_NATIVE: {
                        // invalidate current_fiber because it does not need to be synced in this case
                        current_fiber = NULL;
                        // support for default arg values
                        if (marray_size(closure->f->pvalue)) {
                            uint32_t n = 1; // from 1 in order to skip self implicit argument
                            while (n < closure->f->nparams) {
                                if (VALUE_ISA_UNDEFINED(STACK_GET(rwin+n))) SETVALUE(rwin+n, marray_get(closure->f->pvalue, n-1));
                                ++n;
                            }
                        }
                        PUSH_FRAME(closure, &stackstart[rwin], r1, r3);
                        
                        // max depth recursion check
                        if (vm->maxrecursion != 0) {
                            if (func != closure->f) vm->recursioncount = 0;
                            else if (++vm->recursioncount >= vm->maxrecursion) {
                                const char *identifier = (func->identifier) ? func->identifier : "anon";
                                RUNTIME_ERROR("Max recursion depth exceeded for func %s (limit is set to %d)", identifier , vm->maxrecursion);
                                break;
                            }
                        }
                    } break;

                    case EXEC_TYPE_INTERNAL: {
                        // backup register r1 because it can be overwrite to return a closure
                        gravity_value_t r1copy = STACK_GET(r1);
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = closure->f->internal(vm, &stackstart[rwin], r3, r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (vm->aborted) return false;

                            // check for special getter trick
                            if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {
                                closure = VALUE_AS_CLOSURE(STACK_GET(r1));
                                SETVALUE(r1, r1copy);
                                goto execute_call_function;
                            }

                            // reset current fiber that could be changed during the call
                            fiber = vm->fiber;
                            
                            // check for special fiber error
                            if (fiber == NULL) return true;
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_BRIDGED: {
                        bool result;
                        BEGIN_TRUST_USERCODE(vm);
                        if (VALUE_ISA_CLASS(v)) {
                            DEBUG_ASSERT(delegate->bridge_initinstance, "bridge_initinstance delegate callback is mandatory");
                            gravity_instance_t *instance = (gravity_instance_t *)VALUE_AS_OBJECT(stackstart[rwin]);
                            result = delegate->bridge_initinstance(vm, closure->f->xdata, STACK_GET(0), instance, &stackstart[rwin], r3);
                            SETVALUE(r1, VALUE_FROM_OBJECT(instance));
                        } else {
                            DEBUG_ASSERT(delegate->bridge_execute, "bridge_execute delegate callback is mandatory");
                            // starting from version 0.4.4 we pass context object to execute in order to give the opportunity to pass it as self parameter to closures
                            result = delegate->bridge_execute(vm, closure->f->xdata, STACK_GET(0), &stackstart[rwin], r3, r1);
                        }
                        END_TRUST_USERCODE(vm);
                        if (!result && fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                    } break;

                    case EXEC_TYPE_SPECIAL:
                        RUNTIME_ERROR("Unable to handle a special function in current context");
                        break;
                }
                
                LOAD_FRAME();
                SYNC_STACKTOP(current_fiber, fiber, stacktopdelta);

                DISPATCH();
            }

            // MARK: RET
            // MARK: RET0
            CASE_CODE(RET0):
            CASE_CODE(RET): {
                gravity_value_t result;

                if (op == RET0) {
                    DEBUG_VM("RET0");
                    result = VALUE_FROM_NULL;
                } else {
                    OPCODE_GET_ONE8bit(inst, const uint32_t r1);
                    DEBUG_VM("RET %d", r1);
                    result = STACK_GET(r1);
                }

                // sanity check
                DEBUG_ASSERT(fiber->nframes > 0, "Number of active frames cannot be 0.");

                // POP frame
                --fiber->nframes;

                // close open upvalues (if any)
                gravity_close_upvalues(fiber, stackstart);

                // check if it was a gravity_vm_runclosure execution
                if (frame->outloop) {
                    fiber->result = result;
                    return true;
                }

                // retrieve destination register
                uint32_t dest = fiber->frames[fiber->nframes].dest;
                if (fiber->nframes == 0) {
                    if (fiber->caller == NULL) {fiber->result = result; return true;}
                    fiber = fiber->caller;
                    vm->fiber = fiber;
                } else {
                    // recompute stacktop based on last executed frame
					gravity_callframe_t *lastframe = &fiber->frames[fiber->nframes-1];
                    fiber->stacktop = lastframe->stackstart + FN_COUNTREG(lastframe->closure->f, lastframe->nargs);
                }

                LOAD_FRAME();
                DEBUG_CALL("Resuming", func);
                SETVALUE(dest, result);

                DISPATCH();
            }

            // MARK: HALT
            CASE_CODE(HALT): {
                DEBUG_VM("HALT");
                return true;
            }

            // MARK: SWITCH
            CASE_CODE(SWITCH): {
                DEBUG_ASSERT(0, "To be implemented");
                DISPATCH();
            }

            // MARK: -
            // MARK: MAPNEW
            CASE_CODE(MAPNEW): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t n);
                DEBUG_VM("MAPNEW %d %d", r1, n);

                gravity_map_t *map = gravity_map_new(vm, n);
                SETVALUE(r1, VALUE_FROM_OBJECT(map));
                DISPATCH();
            }

            // MARK: LISTNEW
            CASE_CODE(LISTNEW): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t n);
                DEBUG_VM("LISTNEW %d %d", r1, n);

                gravity_list_t *list = gravity_list_new(vm, n);
                SETVALUE(r1, VALUE_FROM_OBJECT(list));
                DISPATCH();
            }

            // MARK: RANGENEW
            CASE_CODE(RANGENEW): {
                OPCODE_GET_THREE8bit_ONE2bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3, const bool flag);
                DEBUG_VM("RANGENEW %d %d %d (flag %d)", r1, r2, r3, flag);

                // sanity check
                if ((!VALUE_ISA_INT(STACK_GET(r2))) || (!VALUE_ISA_INT(STACK_GET(r3))))
                    RUNTIME_ERROR("Unable to build Range from a non Int value");

                gravity_range_t *range = gravity_range_new(vm, VALUE_AS_INT(STACK_GET(r2)), VALUE_AS_INT(STACK_GET(r3)), !flag);
                SETVALUE(r1, VALUE_FROM_OBJECT(range));
                DISPATCH();
            }

            // MARK: SETLIST
            CASE_CODE(SETLIST): {
                OPCODE_GET_TWO8bit_ONE10bit(inst, uint32_t r1, uint32_t r2, const uint32_t r3);
                DEBUG_VM("SETLIST %d %d", r1, r2);

                // since this code is produced by the compiler I can trust the fact that if v1 is not a map
                // then it is an array and nothing else
                gravity_value_t v1 = STACK_GET(r1);
                bool v1_is_map = VALUE_ISA_MAP(v1);

                // r2 == 0 is an optimization, it means that list/map is composed all of literals
                // and can be filled using the constant pool (r3 in this case represents an index inside cpool)
                if (r2 == 0) {
                    gravity_value_t v2 = gravity_function_cpool_get(func, r3);
                    if (v1_is_map) {
                        gravity_map_t *map = VALUE_AS_MAP(v1);
                        gravity_map_append_map(vm, map, VALUE_AS_MAP(v2));
                    } else {
                        gravity_list_t *list = VALUE_AS_LIST(v1);
                        gravity_list_append_list(vm, list, VALUE_AS_LIST(v2));
                    }
                    DISPATCH();
                }

                if (v1_is_map) {
                    gravity_map_t *map = VALUE_AS_MAP(v1);
                    while (r2) {
                        gravity_value_t key = STACK_GET(++r1);
                        if (!VALUE_ISA_STRING(key)) RUNTIME_ERROR("Unable to build Map from a non String key");
                        gravity_value_t value = STACK_GET(++r1);
                        gravity_hash_insert(map->hash, key, value);
                        --r2;
                    }
                } else {
                    gravity_list_t *list = VALUE_AS_LIST(v1);
                    while (r2) {
                        marray_push(gravity_value_t, list->array, STACK_GET(++r1));
                        --r2;
                    }
                }
                DISPATCH();
            }

            // MARK: -
            // MARK: CLOSURE
            CASE_CODE(CLOSURE): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t index);
                DEBUG_VM("CLOSURE %d %d", r1, index);

                // get function prototype from cpool
                gravity_value_t v = gravity_function_cpool_get(func, index);
                if (!VALUE_ISA_FUNCTION(v)) RUNTIME_ERROR("Unable to create a closure from a non function object.");
                gravity_function_t *f = VALUE_AS_FUNCTION(v);
                
                gravity_gc_setenabled(vm, false);
                
                // create closure (outside GC)
                gravity_closure_t *closure = gravity_closure_new(vm, f);

                // save current context (only if class or instance)
                if ((VALUE_ISA_CLASS(STACK_GET(0))) || (VALUE_ISA_INSTANCE(STACK_GET(0))))
                    closure->context = VALUE_AS_OBJECT(STACK_GET(0)) ;
                
                // loop for each upvalue setup instruction
                for (uint16_t i=0; i<f->nupvalues; ++i) {
                    // code is generated by the compiler so op must be MOVE
                    inst = *ip++;
                    op = (opcode_t)OPCODE_GET_OPCODE(inst);
                    OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t p1, const uint32_t p2);

                    // p2 can be 1 (means that upvalue is in the current call frame) or 0 (means that upvalue is in the upvalue list of the caller)
                    if (op != MOVE) RUNTIME_ERROR("Wrong OPCODE in CLOSURE statement");
                    closure->upvalue[i] = (p2) ? gravity_capture_upvalue (vm, fiber, &stackstart[p1]) : frame->closure->upvalue[p1];
                }
                
                SETVALUE(r1, VALUE_FROM_OBJECT(closure));
                gravity_gc_setenabled(vm, true);
                DISPATCH();
            }

            // MARK: CLOSE
            CASE_CODE(CLOSE): {
                OPCODE_GET_ONE8bit(inst, const uint32_t r1);
                DEBUG_VM("CLOSE %d", r1);

                // close open upvalues (if any) starting from R1
                gravity_close_upvalues(fiber, &stackstart[r1]);
                DISPATCH();
            }
            
            // MARK: CHECK
            CASE_CODE(CHECK): {
                OPCODE_GET_ONE8bit(inst, const uint32_t r1);
                DEBUG_VM("CHECK %d", r1);
                
                gravity_value_t value = STACK_GET(r1);
                if (VALUE_ISA_INSTANCE(value) && (gravity_instance_isstruct(VALUE_AS_INSTANCE(value)))) {
                    gravity_instance_t *instance = gravity_instance_clone(vm, VALUE_AS_INSTANCE(value));
                    SETVALUE(r1, VALUE_FROM_OBJECT(instance));
                }
                
                DISPATCH();
            }
            
            // MARK: - RESERVED
            CASE_CODE(RESERVED2):
            CASE_CODE(RESERVED3):
            CASE_CODE(RESERVED4):
            CASE_CODE(RESERVED5):
            CASE_CODE(RESERVED6):{
                RUNTIME_ERROR("Opcode not implemented in this VM version.");
                DISPATCH();
            }
        }

        // MARK: -

        INC_PC;
    };

    return true;
}

gravity_vm *gravity_vm_new (gravity_delegate_t *delegate) {
    gravity_core_init();
    
    gravity_vm *vm = mem_alloc(NULL, sizeof(gravity_vm));
    if (!vm) return NULL;

    // setup default callbacks
    vm->transfer = gravity_gc_transfer;
    vm->cleanup = gravity_gc_cleanup;

    // allocate default fiber
    vm->fiber = gravity_fiber_new(vm, NULL, 0, 0);
    vm->maxccalls = MAX_CCALLS;
    vm->maxrecursion = 0; // default is no limit

    vm->pc = 0;
    vm->delegate = (delegate) ? delegate : &empty_delegate;
    vm->context = gravity_hash_create(DEFAULT_CONTEXT_SIZE, gravity_value_hash, gravity_value_equals, NULL, NULL);

    // garbage collector
    gravity_gc_setenabled(vm, true);
    gravity_gc_setvalues(vm, DEFAULT_CG_THRESHOLD, DEFAULT_CG_MINTHRESHOLD, DEFAULT_CG_RATIO);
    vm->memallocated = 0;
    vm->maxmemblock = MAX_MEMORY_BLOCK;
    marray_init(vm->graylist);
    marray_init(vm->gctemp);

    // init base and core
    gravity_core_register(vm);
    gravity_cache_setup();

    RESET_STATS(vm);
    return vm;
}

gravity_vm *gravity_vm_newmini (void) {
    gravity_core_init();
    gravity_vm *vm = mem_alloc(NULL, sizeof(gravity_vm));
    return vm;
}

void gravity_vm_free (gravity_vm *vm) {
    if (!vm) return;

    if (vm->context) gravity_cache_free();
    gravity_vm_cleanup(vm);
    if (vm->context) gravity_hash_free(vm->context);
    marray_destroy(vm->gctemp);
    marray_destroy(vm->graylist);
    mem_free(vm);
}

inline gravity_value_t gravity_vm_lookup (gravity_vm *vm, gravity_value_t key) {
    gravity_value_t *value = gravity_hash_lookup(vm->context, key);
    return (value) ? *value : VALUE_NOT_VALID;
}

inline gravity_closure_t *gravity_vm_fastlookup (gravity_vm *vm, gravity_class_t *c, int index) {
    #pragma unused(vm)
    return (gravity_closure_t *)gravity_class_lookup_closure(c, cache[index]);
}

inline gravity_value_t gravity_vm_getvalue (gravity_vm *vm, const char *key, uint32_t keylen) {
    STATICVALUE_FROM_STRING(k, key, keylen);
    return gravity_vm_lookup(vm, k);
}

inline void gravity_vm_setvalue (gravity_vm *vm, const char *key, gravity_value_t value) {
    gravity_hash_insert(vm->context, VALUE_FROM_CSTRING(vm, key), value);
}

double gravity_vm_time (gravity_vm *vm) {
    return vm->time;
}

gravity_value_t gravity_vm_result (gravity_vm *vm) {
    gravity_value_t result = vm->fiber->result;
    vm->fiber->result = VALUE_FROM_NULL;
    return result;
}

gravity_delegate_t *gravity_vm_delegate (gravity_vm *vm) {
    return vm->delegate;
}

gravity_fiber_t *gravity_vm_fiber (gravity_vm* vm) {
    return vm->fiber;
}

void gravity_vm_setfiber(gravity_vm* vm, gravity_fiber_t *fiber) {
    vm->fiber = fiber;
}

void gravity_vm_seterror (gravity_vm* vm, const char *format, ...) {
    gravity_fiber_t *fiber = vm->fiber;

    if (fiber->error) mem_free(fiber->error);
    size_t err_size = 2048;
    fiber->error = mem_alloc(NULL, err_size);

    va_list arg;
    va_start (arg, format);
    vsnprintf(fiber->error, err_size, (format) ? format : "%s", arg);
    va_end (arg);
}

void gravity_vm_seterror_string (gravity_vm* vm, const char *s) {
    gravity_fiber_t *fiber = vm->fiber;
    if (fiber->error) mem_free(fiber->error);
    fiber->error = (char *)string_dup(s);
}

#if GRAVITY_VM_STATS
static void gravity_vm_stats (gravity_vm *vm) {
    printf("\n=======================================================\n");
    printf("                   GRAVITY VM STATS\n");
    printf("=======================================================\n");
    printf("%12s %10s %10s %20s\n", "OPCODE", "USAGE", "MEAN", "MICROBENCH (ms)");
    printf("=======================================================\n");

    double total = 0.0;
    for (uint32_t i=0; i<GRAVITY_LATEST_OPCODE; ++i) {
        if (vm->nstat[i]) {
            total += vm->tstat[i];
        }
    }

    for (uint32_t i=0; i<GRAVITY_LATEST_OPCODE; ++i) {
        if (vm->nstat[i]) {
            uint32_t n = vm->nstat[i];
            double d = vm->tstat[i];
            double m = d / (double)n;
            double p = (d * 100) / total;
            printf("%12s %*d %*.4f %*.4f (%.2f%%)\n", opcode_name((opcode_t)i), 10, n, 11, m, 10, d, p);
        }
    }
    printf("=======================================================\n");
    printf("# Frames reallocs: %d (%d)\n", vm->nfrealloc, vm->fiber->framesalloc);
    printf("# Stack  reallocs: %d (%d)\n", vm->nsrealloc, vm->fiber->stackalloc);
    printf("=======================================================\n");
}
#endif

void gravity_vm_loadclosure (gravity_vm *vm, gravity_closure_t *closure) {
    // closure MUST BE $moduleinit so sanity check here
    if (string_cmp(closure->f->identifier, INITMODULE_NAME) != 0) return;

    // re-use main fiber
    gravity_fiber_reassign(vm->fiber, closure, 0);

    // execute $moduleinit in order to initialize VM
    gravity_vm_exec(vm);
}

bool gravity_vm_runclosure (gravity_vm *vm, gravity_closure_t *closure, gravity_value_t sender, gravity_value_t params[], uint16_t nparams) {
    if (!vm || !closure || vm->aborted) return false;

    // do not waste cycles on empty functions
    gravity_function_t *f = closure->f;
    if (f && (f->tag == EXEC_TYPE_NATIVE) && ((!f->bytecode) || (f->ninsts == 0))) return true;

    // current execution fiber
    gravity_fiber_t     *fiber = vm->fiber;
    gravity_value_t     *stackstart = NULL;
    uint32_t            rwin = 0;
	uint32_t			stacktopdelta = 0;
    
    // current frame and current instruction pointer
    gravity_callframe_t *frame;
    uint32_t            *ip;
	
    DEBUG_STACK();

    // self value is default to the context where the closure has been created (or set by the user)
    gravity_value_t selfvalue;

    // MSVC bug: designated initializer was prematurely evaluated
    if (closure->context) selfvalue = VALUE_FROM_OBJECT(closure->context);
    else selfvalue = sender;
    
    // we need a way to give user the ability to access the sender value from a closure
    
    // if fiber->nframes is not zero it means that this event has been recursively called
    // from somewhere inside the main function so we need to protect and correctly setup
    // the new activation frame
    if (fiber->nframes) {
        // current call frame
        frame = &fiber->frames[fiber->nframes - 1];

        // current top of the stack
        stackstart = frame->stackstart;

        // current instruction pointer
        ip = frame->ip;

        // compute register window
        rwin = FN_COUNTREG(frame->closure->f, frame->nargs);

        // check stack size
        uint32_t _rneed = FN_COUNTREG(f,nparams+1);
		stacktopdelta = (uint32_t)MAXNUM(stackstart + rwin + _rneed - vm->fiber->stacktop, 0);
        if (!gravity_check_stack(vm, vm->fiber, stacktopdelta, &stackstart)) {
            RUNTIME_ERROR("Infinite loop detected. Current execution must be aborted.");
        }
		
        // setup params (first param is self)
        SETVALUE(rwin, selfvalue);
        for (uint16_t i=0; i<nparams; ++i) {
            SETVALUE(rwin+i+1, params[i]);
        }

        STORE_FRAME();
        PUSH_FRAME(closure, &stackstart[rwin], rwin, nparams+1);
        SETFRAME_OUTLOOP(cframe);
    } else {
        // there are no execution frames when called outside main function
        gravity_fiber_reassign(vm->fiber, closure, nparams+1);
        stackstart = vm->fiber->stack;
		stacktopdelta = FN_COUNTREG(closure->f, nparams+1);
		
        // setup params (first param is self)
        SETVALUE(rwin, selfvalue);
        for (uint16_t i=0; i<nparams; ++i) {
            SETVALUE(rwin+i+1, params[i]);
        }

        // check if closure uses the special _args instruction
        frame = &fiber->frames[0];
        ip = frame->ip;
        frame->args = (USE_ARGS(closure)) ? gravity_list_from_array(vm, nparams, &stackstart[rwin]+1) : NULL;
    }

    // f can be native, internal or bridge because this function
    // is called also by convert_value2string
    // for example in Creo:
    // var mData = Data();
    // Console.write("data: " + mData);
    // mData.String is a bridged objc method
    DEBUG_STACK();

    bool result = false;
    switch (f->tag) {
        case EXEC_TYPE_NATIVE:
            ++vm->nccalls;
            if (vm->nccalls > vm->maxccalls) RUNTIME_ERROR("Maximum number of nested C calls reached (%d).", vm->maxccalls);
            result = gravity_vm_exec(vm);
            --vm->nccalls;
            break;

        case EXEC_TYPE_INTERNAL:
            BEGIN_TRUST_USERCODE(vm);
            result = f->internal(vm, &stackstart[rwin], nparams, GRAVITY_FIBER_REGISTER);
            END_TRUST_USERCODE(vm);
            break;

        case EXEC_TYPE_BRIDGED:
            if (vm->delegate->bridge_execute) {
                BEGIN_TRUST_USERCODE(vm);
                result = vm->delegate->bridge_execute(vm, f->xdata, selfvalue, &stackstart[rwin], nparams, GRAVITY_FIBER_REGISTER);
                END_TRUST_USERCODE(vm);
            }
            break;

        case EXEC_TYPE_SPECIAL:
            result = false;
            break;
    }
    
    if (fiber == vm->fiber) {
        // fix pointers ONLY if fiber remains the same
        if (f->tag != EXEC_TYPE_NATIVE) --fiber->nframes;
        fiber->stacktop -= stacktopdelta;
    }
	
    DEBUG_STACK();
    return result;
}

bool gravity_vm_runmain (gravity_vm *vm, gravity_closure_t *closure) {
    // first load closure into vm
    if (closure) gravity_vm_loadclosure(vm, closure);

    // lookup main function
    gravity_value_t main = gravity_vm_getvalue(vm, MAIN_FUNCTION, (uint32_t)strlen(MAIN_FUNCTION));
    if (!VALUE_ISA_CLOSURE(main)) {
        report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, "%s", "Unable to find main function.");
        return false;
    }

    // re-use main fiber
    gravity_closure_t *main_closure = VALUE_AS_CLOSURE(main);
    gravity_fiber_reassign(vm->fiber, main_closure, 0);

    // execute main function
    RESET_STATS(vm);
    nanotime_t tstart = nanotime();
    bool result = gravity_vm_exec(vm);
    nanotime_t tend = nanotime();
    vm->time = millitime(tstart, tend);

    PRINT_STATS(vm);
    return result;
}

void gravity_vm_reset (gravity_vm *vm) {
    if (!vm || !vm->fiber) return;
    gravity_fiber_reset(vm->fiber);
}

// MARK: - User -

gravity_closure_t *gravity_vm_getclosure (gravity_vm *vm) {
    if (!vm || !vm->fiber) return NULL;
    if (!vm->fiber->nframes) return NULL;
    if (vm->aborted) return NULL;

    gravity_callframe_t *frame = &(vm->fiber->frames[vm->fiber->nframes-1]);
    return frame->closure;
}

void gravity_vm_setslot (gravity_vm *vm, gravity_value_t value, uint32_t index) {
    if (vm->aborted) return;
    if (index == GRAVITY_FIBER_REGISTER) {
        vm->fiber->result = value;
        return;
    }

    gravity_callframe_t *frame = &(vm->fiber->frames[vm->fiber->nframes-1]);
    frame->stackstart[index] = value;
}

gravity_value_t gravity_vm_getslot (gravity_vm *vm, uint32_t index) {
    gravity_callframe_t *frame = &(vm->fiber->frames[vm->fiber->nframes-1]);
    return frame->stackstart[index];
}

void gravity_vm_setdata (gravity_vm *vm, void *data) {
    vm->data = data;
}

void *gravity_vm_getdata (gravity_vm *vm) {
    return vm->data;
}

void gravity_vm_set_callbacks (gravity_vm *vm, vm_transfer_cb vm_transfer, vm_cleanup_cb vm_cleanup) {
    vm->transfer = vm_transfer;
    vm->cleanup = vm_cleanup;
}

void gravity_vm_transfer (gravity_vm *vm, gravity_object_t *obj) {
    if (vm->transfer) vm->transfer(vm, obj);
}

void gravity_vm_cleanup (gravity_vm *vm) {
    if (vm->cleanup) vm->cleanup(vm);
}

void gravity_vm_filter (gravity_vm *vm, vm_filter_cb cleanup_filter) {
    vm->filter = cleanup_filter;
}

bool gravity_vm_ismini (gravity_vm *vm) {
    return (vm->context == NULL);
}

bool gravity_vm_isaborted (gravity_vm *vm) {
    if (!vm) return true;
    return vm->aborted;
}

void gravity_vm_setaborted (gravity_vm *vm) {
    vm->aborted = true;
}

char *gravity_vm_anonymous (gravity_vm *vm) {
    snprintf(vm->temp, sizeof(vm->temp), "%sanon%d", GRAVITY_VM_ANONYMOUS_PREFIX, ++vm->nanon);
    return vm->temp;
}

void gravity_vm_memupdate (gravity_vm *vm, gravity_int_t value) {
    vm->memallocated += value;
}

gravity_int_t gravity_vm_maxmemblock (gravity_vm *vm) {
    return vm->maxmemblock;
}

// MARK: - Get/Set Internal Settings -

gravity_value_t gravity_vm_get (gravity_vm *vm, const char *key) {
    if (key) {
        if (strcmp(key, GRAVITY_VM_GCENABLED) == 0) return VALUE_FROM_INT(vm->gcenabled);
        if (strcmp(key, GRAVITY_VM_GCMINTHRESHOLD) == 0) return VALUE_FROM_INT(vm->gcminthreshold);
        if (strcmp(key, GRAVITY_VM_GCTHRESHOLD) == 0) return VALUE_FROM_INT(vm->gcthreshold);
        if (strcmp(key, GRAVITY_VM_GCRATIO) == 0) return VALUE_FROM_FLOAT(vm->gcratio);
        if (strcmp(key, GRAVITY_VM_MAXCALLS) == 0) return VALUE_FROM_INT(vm->maxccalls);
        if (strcmp(key, GRAVITY_VM_MAXBLOCK) == 0) return VALUE_FROM_INT(vm->maxmemblock);
        if (strcmp(key, GRAVITY_VM_MAXRECURSION) == 0) return VALUE_FROM_INT(vm->maxrecursion);
    }
    return VALUE_FROM_NULL;
}

bool gravity_vm_set (gravity_vm *vm, const char *key, gravity_value_t value) {
    if (key) {
        if ((strcmp(key, GRAVITY_VM_GCENABLED) == 0) && VALUE_ISA_BOOL(value)) {VALUE_AS_BOOL(value) ? ++vm->gcenabled : --vm->gcenabled ; return true;}
        if ((strcmp(key, GRAVITY_VM_GCMINTHRESHOLD) == 0) && VALUE_ISA_INT(value)) {vm->gcminthreshold = VALUE_AS_INT(value); return true;}
        if ((strcmp(key, GRAVITY_VM_GCTHRESHOLD) == 0) && VALUE_ISA_INT(value)) {vm->gcthreshold = VALUE_AS_INT(value); return true;}
        if ((strcmp(key, GRAVITY_VM_GCRATIO) == 0) && VALUE_ISA_FLOAT(value)) {vm->gcratio = VALUE_AS_FLOAT(value); return true;}
        if ((strcmp(key, GRAVITY_VM_MAXCALLS) == 0) && VALUE_ISA_INT(value)) {vm->maxccalls = (uint32_t)VALUE_AS_INT(value); return true;}
        if ((strcmp(key, GRAVITY_VM_MAXBLOCK) == 0) && VALUE_ISA_INT(value)) {vm->maxmemblock = (uint32_t)VALUE_AS_INT(value); return true;}
        if ((strcmp(key, GRAVITY_VM_MAXRECURSION) == 0) && VALUE_ISA_INT(value)) {vm->maxrecursion = (uint32_t)VALUE_AS_INT(value); return true;}
    }
    return false;
}


// MARK: - Deserializer -

static bool real_set_superclass (gravity_vm *vm, gravity_class_t *c, gravity_value_t key, const char *supername) {
    // 1. LOOKUP in current stack hierarchy
    STATICVALUE_FROM_STRING(superkey, supername, strlen(supername));
    void_r *stack = (void_r *)vm->data;
    size_t n = marray_size(*stack);
    for (size_t i=0; i<n; i++) {
        gravity_object_t *obj = marray_get(*stack, i);
        // if object is a CLASS then lookup hash table
        if (OBJECT_ISA_CLASS(obj)) {
            gravity_class_t *c2 = (gravity_class_t *)gravity_class_lookup((gravity_class_t *)obj, superkey);
            if ((c2) && (OBJECT_ISA_CLASS(c2))) {
                mem_free(supername);
                if (!gravity_class_setsuper(c, c2)) goto error_max_ivar;
                return true;
            }
        }
        // if object is a FUNCTION then iterate the constant pool
        else if (OBJECT_ISA_FUNCTION(obj)) {
            gravity_function_t *f = (gravity_function_t *)obj;
            if (f->tag == EXEC_TYPE_NATIVE) {
                size_t count = marray_size(f->cpool);
                for (size_t j=0; j<count; j++) {
                    gravity_value_t v = marray_get(f->cpool, j);
                    if (VALUE_ISA_CLASS(v)) {
                        gravity_class_t *c2 = VALUE_AS_CLASS(v);
                        if (strcmp(c2->identifier, supername) == 0) {
                            mem_free(supername);
                            if (!gravity_class_setsuper(c, c2)) goto error_max_ivar;
                            return true;
                        }
                    }
                }
            }
        }
    }

    // 2. not found in stack hierarchy so LOOKUP in VM
    gravity_value_t v = gravity_vm_lookup(vm, superkey);
    if (VALUE_ISA_CLASS(v)) {
        mem_free(supername);
        if (!gravity_class_setsuper(c, VALUE_AS_CLASS(v))) goto error_max_ivar;
        return true;
    }

    report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, "Unable to find superclass %s of class %s.", supername, VALUE_AS_CSTRING(key));
    mem_free(supername);
    return false;

error_max_ivar:
    report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, "Maximum number of allowed ivars (%d) reached for class %s.", MAX_IVARS, VALUE_AS_CSTRING(key));
    return false;
}

static void vm_set_superclass_callback (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(hashtable)
    gravity_vm *vm = (gravity_vm *)data;

    if (VALUE_ISA_FUNCTION(value)) vm_set_superclass(vm, VALUE_AS_OBJECT(value));
    if (!VALUE_ISA_CLASS(value)) return;

    // process class
    gravity_class_t *c = VALUE_AS_CLASS(value);
    DEBUG_DESERIALIZE("set_superclass_callback for class %s", c->identifier);

    const char *supername = c->xdata;
    c->xdata = NULL;
    if (supername) {
        if (!real_set_superclass(vm, c, key, supername)) return;
    }

    gravity_hash_iterate(c->htable, vm_set_superclass_callback, vm);
}

static bool vm_set_superclass (gravity_vm *vm, gravity_object_t *obj) {
    void_r *stack = (void_r *)vm->data;
    marray_push(void*, *stack, obj);

    if (OBJECT_ISA_CLASS(obj)) {
        // CLASS case: process class and its hash table
        gravity_class_t *c = (gravity_class_t *)obj;
        DEBUG_DESERIALIZE("set_superclass for class %s", c->identifier);

        STATICVALUE_FROM_STRING(key, c->identifier, strlen(c->identifier));
        const char *supername = c->xdata;
        c->xdata = NULL;
        if (supername) real_set_superclass(vm, c, key, supername);
        gravity_hash_iterate(c->htable, vm_set_superclass_callback, vm);

    } else if (OBJECT_ISA_FUNCTION(obj)) {
        // FUNCTION case: scan constant pool and recursively call fixsuper

        gravity_function_t *f = (gravity_function_t *)obj;
        if (f->tag == EXEC_TYPE_NATIVE) {
            size_t n = marray_size(f->cpool);
            for (size_t i=0; i<n; i++) {
                gravity_value_t v = marray_get(f->cpool, i);
                if (VALUE_ISA_FUNCTION(v)) vm_set_superclass(vm, (gravity_object_t *)VALUE_AS_FUNCTION(v));
                else if (VALUE_ISA_CLASS(v)) vm_set_superclass(vm, (gravity_object_t *)VALUE_AS_CLASS(v));
            }
        }
    } else {
        report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, "%s", "Unable to recognize object type.");
        return false;
    }

    marray_pop(*stack);
    return true;
}

gravity_closure_t *gravity_vm_loadfile (gravity_vm *vm, const char *path) {
    size_t len;
    const char *buffer = file_read(path, &len);
    if (!buffer) return NULL;

    gravity_closure_t *closure = gravity_vm_loadbuffer(vm, buffer, len);

    mem_free(buffer);
    return closure;
}

gravity_closure_t *gravity_vm_loadbuffer (gravity_vm *vm, const char *buffer, size_t len) {
    // state buffer for further processing super classes
    void_r objects;
    marray_init(objects);

    // start json parsing
    json_value *json = json_parse (buffer, len);
    if (!json) goto abort_load;
    if (json->type != json_object) goto abort_load;

    // disable GC while deserializing objects
    gravity_gc_setenabled(vm, false);

    // scan loop
    gravity_closure_t *closure = NULL;
    uint32_t n = json->u.object.length;
    for (uint32_t i=0; i<n; ++i) {
        json_value *entry = json->u.object.values[i].value;
        if (entry->u.object.length == 0) continue;

        // each entry must be an object
        if (entry->type != json_object) goto abort_load;

        gravity_object_t *obj = gravity_object_deserialize(vm, entry);
        if (!obj) goto abort_load;

        // save object for further processing
        marray_push(void*, objects, obj);

        // add a sanity check here: obj can be a function or a class, nothing else

        // transform every function to a closure
        if (OBJECT_ISA_FUNCTION(obj)) {
            gravity_function_t *f = (gravity_function_t *)obj;
            const char *identifier = f->identifier;
            gravity_closure_t *cl = gravity_closure_new(vm, f);
            if (string_casencmp(identifier, INITMODULE_NAME, strlen(identifier)) == 0) {
                closure = cl;
            } else {
                gravity_vm_setvalue(vm, identifier, VALUE_FROM_OBJECT(cl));
            }
        }
    }
    json_value_free(json);
    json = NULL;

    // fix superclass(es)
    size_t count = marray_size(objects);
    if (count) {
        void *saved = vm->data;

        // prepare stack to help resolve nested super classes
        void_r stack;
        marray_init(stack);
        vm->data = (void *)&stack;

        // loop of each processed object
        for (size_t i=0; i<count; ++i) {
            gravity_object_t *obj = (gravity_object_t *)marray_get(objects, i);
            if (!vm_set_superclass(vm, obj)) goto abort_super;
        }

        marray_destroy(stack);
        marray_destroy(objects);
        vm->data = saved;
    }

    gravity_gc_setenabled(vm, true);
    return closure;

abort_load:
    report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, "%s", "Unable to parse JSON executable file.");

abort_super:
    marray_destroy(objects);
    if (json) json_value_free(json);
    gravity_gc_setenabled(vm, true);
    return NULL;
}

// MARK: - Garbage Collector -

void gravity_gray_object (gravity_vm *vm, gravity_object_t *obj) {
    if (!obj) return;

    // avoid recursion if object has already been visited
    if (obj->gc.isdark) return;
    DEBUG_GC("GRAY %s", gravity_object_debug(obj, false));

    // object has been reached
    obj->gc.isdark = true;

    // add to marked array
    marray_push(gravity_object_t *, vm->graylist, obj);
}

void gravity_gray_value (gravity_vm *vm, gravity_value_t v) {
    if (gravity_value_isobject(v)) gravity_gray_object(vm, (gravity_object_t *)v.p);
}

static void gravity_gray_hash (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused (hashtable)
    gravity_vm *vm = (gravity_vm*)data;
    gravity_gray_value(vm, key);
    gravity_gray_value(vm, value);
}

// MARK: -

void gravity_gc_setvalues (gravity_vm *vm, gravity_int_t threshold, gravity_int_t minthreshold, gravity_float_t ratio) {
    vm->gcminthreshold = (minthreshold) ? minthreshold : DEFAULT_CG_MINTHRESHOLD;
    vm->gcthreshold = (threshold) ? threshold : DEFAULT_CG_THRESHOLD;
    vm->gcratio = (ratio) ? ratio : DEFAULT_CG_RATIO;
    vm->gcthreshold_original = vm->gcthreshold;
}

static void gravity_gc_transform (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t *value, void *data) {
    #pragma unused (hashtable)

    gravity_vm *vm = (gravity_vm *)data;
    gravity_object_t *obj = VALUE_AS_OBJECT(*value);

    if (OBJECT_ISA_FUNCTION(obj)) {
        gravity_function_t *f = (gravity_function_t *)obj;
        if (f->tag == EXEC_TYPE_SPECIAL) {
            if (f->special[0]) {
                gravity_gc_transfer(vm, (gravity_object_t *)f->special[0]);
                f->special[0] = gravity_closure_new(vm, f->special[0]);
            }
            if (f->special[1]) {
                gravity_gc_transfer(vm, (gravity_object_t *)f->special[1]);
                f->special[1] = gravity_closure_new(vm, f->special[1]);
            }
        } else if (f->tag == EXEC_TYPE_NATIVE) {
            gravity_vm_initmodule(vm, f);
        }

        bool is_super_function = false;
        if (VALUE_ISA_STRING(key)) {
            gravity_string_t *s = VALUE_AS_STRING(key);
            // looking for a string that begins with $init AND it is longer than strlen($init)
            is_super_function = ((s->len > 5) && (string_casencmp(s->s, CLASS_INTERNAL_INIT_NAME, 5) == 0));
        }

        gravity_closure_t *closure = gravity_closure_new(vm, f);
        *value = VALUE_FROM_OBJECT((gravity_object_t *)closure);
        if (!is_super_function) gravity_gc_transfer(vm, obj);
    } else if (OBJECT_ISA_CLASS(obj)) {
        gravity_class_t *c = (gravity_class_t *)obj;
        gravity_vm_loadclass(vm, c);
    } else {
        // should never reach this point
        assert(0);
    }
}

void gravity_vm_initmodule (gravity_vm *vm, gravity_function_t *f) {
    size_t n = marray_size(f->cpool);
    for (size_t i=0; i<n; i++) {
        gravity_value_t v = marray_get(f->cpool, i);
        if (VALUE_ISA_CLASS(v)) {
            gravity_class_t *c =  VALUE_AS_CLASS(v);
            gravity_vm_loadclass(vm, c);
        }
        else if (VALUE_ISA_FUNCTION(v)) {
            gravity_vm_initmodule(vm, VALUE_AS_FUNCTION(v));
        }
    }
}

static void gravity_gc_transfer_object (gravity_vm *vm, gravity_object_t *obj) {
    DEBUG_GC("GC TRANSFER %s", gravity_object_debug(obj, false));
    ++vm->gccount;
    obj->gc.next = vm->gchead;
    vm->gchead = obj;
}

static void gravity_gc_check (gravity_vm *vm) {
	if (vm->memallocated >= vm->gcthreshold) gravity_gc_start(vm);
}

static void gravity_gc_transfer (gravity_vm *vm, gravity_object_t *obj) {
    if (vm->gcenabled > 0) {
        #if GRAVITY_GC_STRESSTEST
        #if 0
        // check if ptr is already in the list
        gravity_object_t **ptr = &vm->gchead;
        while (*ptr) {
            if (obj == *ptr) {
                printf("Object %s already GC!\n", gravity_object_debug(obj, false));
                assert(0);
            }
            ptr = &(*ptr)->gc.next;
        }
        #endif
        gravity_gc_start(vm);
        #else
        if (vm->memallocated >= vm->gcthreshold) gravity_gc_start(vm);
        #endif
    }

    gravity_gc_transfer_object(vm, obj);
    gravity_vm_memupdate(vm, gravity_object_size(vm, obj));
}

static void gravity_gc_sweep (gravity_vm *vm) {
    gravity_object_t **obj = &vm->gchead;
    while (*obj) {
        if (!(*obj)->gc.isdark) {
            // object is unreachable so remove from the list and free it
            gravity_object_t *unreached = *obj;
            *obj = unreached->gc.next;
            gravity_object_free(vm, unreached);
            --vm->gccount;
        } else {
            // object was reached so unmark it for the next GC and move on to the next
            (*obj)->gc.isdark = false;
            obj = &(*obj)->gc.next;
        }
    }
}

void gravity_gc_start (gravity_vm *vm) {
    if (!vm->fiber) return;

    #if GRAVITY_GC_STATS
    gravity_int_t membefore = vm->memallocated;
    nanotime_t tstart = nanotime();
    #endif

    // reset memory counter
    vm->memallocated = 0;

    // mark GC saved temp objects
    for (uint32_t i=0; i<marray_size(vm->gctemp); ++i) {
        gravity_object_t *obj = marray_get(vm->gctemp, i);
        gravity_gray_object(vm, obj);
    }

    // mark all reachable objects starting from current fiber
    gravity_gray_object(vm, (gravity_object_t *)vm->fiber);

    // mark globals
    gravity_hash_iterate(vm->context, gravity_gray_hash, (void*)vm);

    // root has been grayed so recursively scan reachable objects
    while (marray_size(vm->graylist)) {
        gravity_object_t *obj = marray_pop(vm->graylist);
        gravity_object_blacken(vm, obj);
    }

    // check unreachable objects (collect white objects)
    gravity_gc_sweep(vm);

    // dynamically update gcthreshold
    vm->gcthreshold = (gravity_int_t)(vm->memallocated + (vm->memallocated * vm->gcratio / 100));
    if (vm->gcthreshold < vm->gcminthreshold) vm->gcthreshold = vm->gcminthreshold;
    
    // this line prevents GC to run more than needed
    if (vm->gcthreshold < vm->gcthreshold_original) vm->gcthreshold = vm->gcthreshold_original;
    
    #if GRAVITY_GC_STATS
    nanotime_t tend = nanotime();
    double gctime = millitime(tstart, tend);
    printf("GC %lu before, %lu after (%lu collected - %lu objects), next at %lu. Took %.2fms.\n",
           (unsigned long)membefore,
           (unsigned long)vm->memallocated,
           (membefore > vm->memallocated) ? (unsigned long)(membefore - vm->memallocated) : 0,
           (unsigned long)vm->gccount,
           (unsigned long)vm->gcthreshold,
           gctime);
    #endif
}

static void gravity_gc_cleanup (gravity_vm *vm) {
    if (!vm->gchead) return;

    if (vm->filter) {
        // filter case
        vm_filter_cb filter = vm->filter;

        // we need to remove freed objects from the linked list
        // so we need a pointer to the previous object in order
        // to be able to point it to obj's next ptr
        //
        //         +--------+      +--------+      +--------+
        //     --> |  prev  |  --> |   obj  |  --> |  next  |  -->
        //         +--------+      +--------+      +--------+
        //             |                               ^
        //             |                               |
        //             +-------------------------------+

        gravity_object_t *obj = vm->gchead;
        gravity_object_t *prev = NULL;

        while (obj) {
            if (!filter(obj)) {
                prev = obj;
                obj = obj->gc.next;
                continue;
            }

            // REMOVE obj from the linked list
            gravity_object_t *next = obj->gc.next;
            if (!prev) vm->gchead = next;
            else prev->gc.next = next;

            gravity_object_free(vm, obj);
            --vm->gccount;
            obj = next;
        }
        return;
    }

    // no filter so free all GC objects
    gravity_object_t *obj = vm->gchead;
    while (obj) {
        gravity_object_t *next = obj->gc.next;
        gravity_object_free(vm, obj);
        --vm->gccount;
        obj = next;
    }
    vm->gchead = NULL;

    // free all temporary allocated objects
    while (marray_size(vm->gctemp)) {
        gravity_object_t *tobj = marray_pop(vm->gctemp);
        gravity_object_free(vm, tobj);
    }
}

void gravity_gc_setenabled (gravity_vm *vm, bool enabled) {
    if (!vm) return;
    (enabled) ? ++vm->gcenabled : --vm->gcenabled ;
    
	if (vm->gcenabled > 0 && (!vm->delegate->disable_gccheck_1)) gravity_gc_check(vm);
}

void gravity_gc_temppush (gravity_vm *vm, gravity_object_t *obj) {
    marray_push(gravity_object_t *, vm->gctemp, obj);
}

void gravity_gc_temppop (gravity_vm *vm) {
    marray_pop(vm->gctemp);
}

void gravity_gc_tempnull (gravity_vm *vm, gravity_object_t *obj) {
    for (uint32_t i=0; i<marray_size(vm->gctemp); ++i) {
        gravity_object_t *tobj = marray_get(vm->gctemp, i);
        if (tobj == obj) {
            marray_setnull(vm->gctemp, i);
            break;
        }
    }
}
