//
//  gravity_value.h
//  gravity
//
//  Created by Marco Bambini on 11/12/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_VALUES__
#define __GRAVITY_VALUES__

#include "gravity_memory.h"
#include "gravity_utils.h"
#include "gravity_array.h"
#include "gravity_json.h"
#include "debug_macros.h"

// Gravity is a dynamically typed language so a variable (gravity_value_t) can hold a value of any type.

// The representation of values in a dynamically typed language is very important since it can lead to a big
// difference in terms of performance. Such representation has several constraints:
// - fast access
// - must represent several kind of values
// - be able to cope with the garbage collector
// - low memory overhead (when allocating a lot of small values)

// In modern 64bit processor with OS that always returns aligned allocated memory blocks that means that each ptr is 8 bytes.
// That means that passing a value as an argument or storing it involves copying these bytes around (requiring 2/4 machine words).
// Values are not pointers but structures.

// The built-in types for booleans, numbers, floats, null, undefs are unboxed: their value is stored directly into gravity_value_t.
// Other types like classes, instances, functions, lists, and strings are all reference types. They are stored on the heap and
// the gravity_value_t just stores a pointer to it.

// So each value is a pointer to a FIXED size block of memory (16 bytes). Having all values of the same size greatly reduce the complexity
// of a memory pool and since allocating a large amount of values is very common is a dynamically typed language like Gravity.
// In a future update I could introduce NaN tagging and squeeze value size to 8 bytes (that would mean nearly double performance).

// Internal settings to set integer and float size.
// Default is to have both int and float as 64bit.

// In a 64bit OS:
// sizeof(float)        => 4 bytes
// sizeof(double)       => 8 bytes
// sizeof(void*)        => 8 bytes
// sizeof(int64_t)      => 8 bytes
//
// sizeof various structs in a 64bit OS:
// STRUCT                       BYTES
// ======                       =====
// gravity_function_t           104
// gravity_value_t              16
// gravity_upvalue_t            56
// gravity_closure_t            40
// gravity_list_t               48
// gravity_map_t                32
// gravity_callframe_t          48
// gravity_fiber_t              112
// gravity_class_t              88
// gravity_module_t             40
// gravity_instance_t           40
// gravity_string_t             48
// gravity_range_t              40

#ifdef __cplusplus
extern "C" {
#endif

#define GRAVITY_VERSION						"0.8.1"     // git tag 0.8.1
#define GRAVITY_VERSION_NUMBER				0x000801    // git push --tags
#define GRAVITY_BUILD_DATE                  __DATE__

#ifndef GRAVITY_ENABLE_DOUBLE
#define GRAVITY_ENABLE_DOUBLE               1           // if 1 enable gravity_float_t to be a double (instead of a float)
#endif

#ifndef GRAVITY_ENABLE_INT64
#define GRAVITY_ENABLE_INT64                1           // if 1 enable gravity_int_t to be a 64bit int (instead of a 32bit int)
#endif

#ifndef GRAVITY_COMPUTED_GOTO
#define GRAVITY_COMPUTED_GOTO               1           // if 1 enable faster computed goto (instead of switch) for compilers that support it
#endif

#ifndef GRAVITY_NULL_SILENT
#define GRAVITY_NULL_SILENT                 1           // if 1 then messages sent to null does not produce any runtime error
#endif

#ifndef GRAVITY_MAP_DOTSUGAR
#define GRAVITY_MAP_DOTSUGAR                1           // if 1 then map objects can be accessed with both map[key] and map.key
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#undef GRAVITY_COMPUTED_GOTO
#define GRAVITY_COMPUTED_GOTO               0           // MSVC does not support computed goto (supported if using clang on Windows)
#endif

#define MAIN_FUNCTION                       "main"
#define ITERATOR_INIT_FUNCTION              "iterate"
#define ITERATOR_NEXT_FUNCTION              "next"
#define INITMODULE_NAME                     "$moduleinit"
#define CLASS_INTERNAL_INIT_NAME            "$init"
#define CLASS_CONSTRUCTOR_NAME              "init"
#define CLASS_DESTRUCTOR_NAME               "deinit"
#define SELF_PARAMETER_NAME                 "self"
#define OUTER_IVAR_NAME                     "outer"
#define GETTER_FUNCTION_NAME                "get"
#define SETTER_FUNCTION_NAME                "set"
#define SETTER_PARAMETER_NAME               "value"

#define GLOBALS_DEFAULT_SLOT                4096
#define CPOOL_INDEX_MAX                     4096        // 2^12
#define CPOOL_VALUE_SUPER                   CPOOL_INDEX_MAX+1
#define CPOOL_VALUE_NULL                    CPOOL_INDEX_MAX+2
#define CPOOL_VALUE_UNDEFINED               CPOOL_INDEX_MAX+3
#define CPOOL_VALUE_ARGUMENTS               CPOOL_INDEX_MAX+4
#define CPOOL_VALUE_TRUE                    CPOOL_INDEX_MAX+5
#define CPOOL_VALUE_FALSE                   CPOOL_INDEX_MAX+6
#define CPOOL_VALUE_FUNC                    CPOOL_INDEX_MAX+7

#define MAX_INSTRUCTION_OPCODE              64              // 2^6
#define MAX_REGISTERS                       256             // 2^8
#define MAX_LOCALS                          200             // maximum number of local variables
#define MAX_UPVALUES                        200             // maximum number of upvalues
#define MAX_INLINE_INT                      131072          // 32 - 6 (OPCODE) - 8 (register) - 1 bit sign = 17
#define MAX_FIELDSxFLUSH                    64              // used in list/map serialization
#define MAX_IVARS                           768             // 2^10 - 2^8
#define MAX_ALLOCATION                      4194304         // 1024 * 1024 * 4 (about 4 millions entry)
#define MAX_CCALLS                          100             // default maximum number of nested C calls
#define MAX_MEMORY_BLOCK                    157286400       // 150MB

#define DEFAULT_CONTEXT_SIZE                256             // default VM context entries (can grow)
#define DEFAULT_MINSTRING_SIZE              32              // minimum string allocation size
#define DEFAULT_MINSTACK_SIZE               256             // sizeof(gravity_value_t) * 256     = 16 * 256 => 4 KB
#define DEFAULT_MINCFRAME_SIZE              32              // sizeof(gravity_callframe_t) * 48  = 32 * 48 => 1.5 KB
#define DEFAULT_CG_THRESHOLD                5*1024*1024     // 5MB
#define DEFAULT_CG_MINTHRESHOLD             1024*1024       // 1MB
#define DEFAULT_CG_RATIO                    0.5             // 50%

#define MAXNUM(a,b)                         ((a) > (b) ? a : b)
#define MINNUM(a,b)                         ((a) < (b) ? a : b)
#define EPSILON                             0.000001
#define MIN_LIST_RESIZE                     12              // value used when a List is resized

#define GRAVITY_DATA_REGISTER               UINT32_MAX
#define GRAVITY_FIBER_REGISTER              UINT32_MAX-1
#define GRAVITY_MSG_REGISTER                UINT32_MAX-2

#define GRAVITY_BRIDGE_INDEX                UINT16_MAX
#define GRAVITY_COMPUTED_INDEX              UINT16_MAX-1

//DLL export/import support for Windows
#if !defined(GRAVITY_API) && defined(_WIN32) && defined(BUILD_GRAVITY_API)
  #define GRAVITY_API __declspec(dllexport)
#else
  #define GRAVITY_API
#endif

// MARK: - STRUCT -

// FLOAT_MAX_DECIMALS FROM https://stackoverflow.com/questions/13542944/how-many-significant-digits-have-floats-and-doubles-in-java
#if GRAVITY_ENABLE_DOUBLE
typedef double                              gravity_float_t;
#define GRAVITY_FLOAT_MAX                   DBL_MAX
#define GRAVITY_FLOAT_MIN                   DBL_MIN
#define FLOAT_MAX_DECIMALS                  16
#define FLOAT_EPSILON                       0.00001
#else
typedef float                               gravity_float_t;
#define GRAVITY_FLOAT_MAX                   FLT_MAX
#define GRAVITY_FLOAT_MIN                   FLT_MIN
#define FLOAT_MAX_DECIMALS                  7
#define FLOAT_EPSILON                       0.00001
#endif

#if GRAVITY_ENABLE_INT64
typedef int64_t                             gravity_int_t;
#define GRAVITY_INT_MAX                     9223372036854775807
#define GRAVITY_INT_MIN                     (-GRAVITY_INT_MAX-1LL)
#else
typedef int32_t                             gravity_int_t;
#define GRAVITY_INT_MAX                     2147483647
#define GRAVITY_INT_MIN                     -2147483648
#endif

// Forward references (an object ptr is just its isa pointer)
typedef struct gravity_class_s              gravity_class_t;
typedef struct gravity_class_s              gravity_object_t;

// Everything inside Gravity VM is a gravity_value_t struct
typedef struct {
    gravity_class_t         *isa;           // EVERY object must have an ISA pointer (8 bytes on a 64bit system)
    union {                                 // union takes 8 bytes on a 64bit system
        gravity_int_t       n;              // integer slot
        gravity_float_t     f;              // float/double slot
        gravity_object_t    *p;             // ptr to object slot
    };
} gravity_value_t;

// All VM shares the same foundation classes
extern gravity_class_t *gravity_class_object;
extern gravity_class_t *gravity_class_bool;
extern gravity_class_t *gravity_class_null;
extern gravity_class_t *gravity_class_int;
extern gravity_class_t *gravity_class_float;
extern gravity_class_t *gravity_class_function;
extern gravity_class_t *gravity_class_closure;
extern gravity_class_t *gravity_class_fiber;
extern gravity_class_t *gravity_class_class;
extern gravity_class_t *gravity_class_string;
extern gravity_class_t *gravity_class_instance;
extern gravity_class_t *gravity_class_list;
extern gravity_class_t *gravity_class_map;
extern gravity_class_t *gravity_class_module;
extern gravity_class_t *gravity_class_range;
extern gravity_class_t *gravity_class_upvalue;

typedef marray_t(gravity_value_t)        gravity_value_r;   // array of values
#ifndef GRAVITY_HASH_DEFINED
#define GRAVITY_HASH_DEFINED
typedef struct gravity_hash_t            gravity_hash_t;    // forward declaration
#endif

#ifndef GRAVITY_VM_DEFINED
#define GRAVITY_VM_DEFINED
typedef struct gravity_vm                gravity_vm;        // vm is an opaque data type
#endif

typedef bool (*gravity_c_internal)(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex);
typedef uint32_t (*gravity_gc_callback)(gravity_vm *vm, gravity_object_t *obj);

typedef enum {
    EXEC_TYPE_SPECIAL_GETTER = 0,       // index inside special gravity_function_t union to represent getter func
    EXEC_TYPE_SPECIAL_SETTER = 1,       // index inside special gravity_function_t union to represent setter func
} gravity_special_index;

typedef enum {
    EXEC_TYPE_NATIVE,           // native gravity code (can change stack)
    EXEC_TYPE_INTERNAL,         // c internal code (can change stack)
    EXEC_TYPE_BRIDGED,          // external code to be executed by delegate (can change stack)
    EXEC_TYPE_SPECIAL           // special execution like getter and setter (can be NATIVE, INTERNAL)
} gravity_exec_type;

typedef struct gravity_gc_s {
    bool                    isdark;         // flag to check if object is reachable
    bool                    visited;        // flag to check if object has already been counted in memory size
    gravity_gc_callback     free;           // free callback
    gravity_gc_callback     size;           // size callback
    gravity_gc_callback     blacken;        // blacken callback
    gravity_object_t        *next;          // to track next object in the linked list
} gravity_gc_t;

typedef struct {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    void                    *xdata;         // extra bridged data
    const char              *identifier;    // function name
    uint16_t                nparams;        // number of formal parameters
    uint16_t                nlocals;        // number of local variables
    uint16_t                ntemps;         // number of temporary values used
    uint16_t                nupvalues;      // number of up values (if any)
    gravity_exec_type       tag;            // can be EXEC_TYPE_NATIVE (default), EXEC_TYPE_INTERNAL, EXEC_TYPE_BRIDGED or EXEC_TYPE_SPECIAL
    union {
        // tag == EXEC_TYPE_NATIVE
        struct {
            gravity_value_r cpool;          // constant pool
            gravity_value_r pvalue;         // default param value
            gravity_value_r pname;          // param names
            uint32_t        ninsts;         // number of instructions in the bytecode
            uint32_t        *bytecode;      // bytecode as array of 32bit values
            uint32_t        *lineno;        // debug: line number <-> current instruction relation
            float           purity;         // experimental value
            bool            useargs;        // flag set by the compiler to optimize the creation of the arguments array only if needed
        };

        // tag == EXEC_TYPE_INTERNAL
        gravity_c_internal  internal;       // function callback

        // tag == EXEC_TYPE_SPECIAL
        struct {
            uint16_t        index;          // property index to speed-up default getter and setter
            void            *special[2];    // getter/setter functions
        };
    };
} gravity_function_t;

typedef struct upvalue_s {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector
    
    gravity_value_t         *value;         // ptr to open value on the stack or to closed value on this struct
    gravity_value_t         closed;         // copy of the value once has been closed
    struct upvalue_s        *next;          // ptr to the next open upvalue
} gravity_upvalue_t;

typedef struct gravity_closure_s {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    gravity_vm              *vm;            // vm bound to this closure (useful when executed from a bridge)
    gravity_function_t      *f;             // function prototype
    gravity_object_t        *context;       // context where the closure has been created (or object bound by the user)
    gravity_upvalue_t       **upvalue;      // upvalue array
    uint32_t                refcount;       // bridge language sometimes needs to protect closures from GC
} gravity_closure_t;

typedef struct {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    gravity_value_r         array;          // dynamic array of values
} gravity_list_t;

typedef struct {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    gravity_hash_t          *hash;          // hash table
} gravity_map_t;

// Call frame used for function call
typedef struct {
    uint32_t                *ip;            // instruction pointer
    uint32_t                dest;           // destination register that will receive result
    uint16_t                nargs;          // number of effective arguments passed to the function
    gravity_list_t          *args;          // implicit special _args array
    gravity_closure_t       *closure;       // closure being executed
    gravity_value_t         *stackstart;    // first stack slot used by this call frame (receiver, plus parameters, locals and temporaries)
    bool                    outloop;        // special case for events or native code executed from C that must be executed separately
} gravity_callframe_t;

typedef enum {
    FIBER_NEVER_EXECUTED = 0,
    FIBER_ABORTED_WITH_ERROR = 1,
    FIBER_TERMINATED = 2,
    FIBER_RUNNING = 3,
    FIBER_TRYING = 4
} gravity_fiber_status;
    
// Fiber is the core executable model
typedef struct fiber_s {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    gravity_value_t         *stack;         // stack buffer (grown as needed and it holds locals and temps)
    gravity_value_t         *stacktop;      // current stack ptr
    uint32_t                stackalloc;     // number of allocated values

    gravity_callframe_t     *frames;        // callframes buffer (grown as needed but never shrinks)
    uint32_t                nframes;        // number of frames currently in use
    uint32_t                framesalloc;    // number of allocated frames

    gravity_upvalue_t       *upvalues;      // linked list used to keep track of open upvalues

    char                    *error;         // runtime error message
    bool                    trying;         // set when the try flag is set by the user
    struct fiber_s          *caller;        // optional caller fiber
    gravity_value_t         result;         // end result of the fiber
    
    gravity_fiber_status    status;         // Fiber status (see enum)
    nanotime_t              lasttime;       // last time Fiber has been called
    gravity_float_t         timewait;       // used in yieldTime
    gravity_float_t         elapsedtime;    // time passed since last execution
} gravity_fiber_t;

typedef struct gravity_class_s {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    gravity_class_t         *objclass;      // meta class
    const char              *identifier;    // class name
    bool                    has_outer;      // flag used to automatically set ivar 0 to outer class (if any)
    bool                    is_struct;      // flag to mark class as a struct
    bool                    is_inited;      // flag used to mark already init meta-classes (to be improved)
    bool                    unused;         // unused padding byte
    void                    *xdata;         // extra bridged data
    struct gravity_class_s  *superclass;    // reference to the super class
    const char              *superlook;     // when a superclass is set to extern a runtime lookup must be performed
    gravity_hash_t          *htable;        // hash table
    uint32_t                nivars;         // number of instance variables
	//gravity_value_r			inames;			    // ivar names
    gravity_value_t         *ivars;         // static variables
} gravity_class_s;

typedef struct {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    const char              *identifier;    // module name
    gravity_hash_t          *htable;        // hash table
} gravity_module_t;

typedef struct {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    gravity_class_t         *objclass;      // real instance class
    void                    *xdata;         // extra bridged data
    gravity_value_t         *ivars;         // instance variables
} gravity_instance_t;

typedef struct {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    char                    *s;             // pointer to NULL terminated string
    uint32_t                hash;           // string hash (type to be keept in sync with gravity_hash_size_t)
    uint32_t                len;            // actual string length
    uint32_t                alloc;          // bytes allocated for string
} gravity_string_t;

typedef struct {
    gravity_class_t         *isa;           // to be an object
    gravity_gc_t            gc;             // to be collectable by the garbage collector

    gravity_int_t           from;           // range start
    gravity_int_t           to;             // range end
} gravity_range_t;

typedef void (*code_dump_function) (void *code);
typedef marray_t(gravity_function_t*)   gravity_function_r;     // array of functions
typedef marray_t(gravity_class_t*)      gravity_class_r;        // array of classes
typedef marray_t(gravity_object_t*)     gravity_object_r;       // array of objects

// MARK: - MODULE -
GRAVITY_API gravity_module_t    *gravity_module_new (gravity_vm *vm, const char *identifier);
GRAVITY_API void                gravity_module_free (gravity_vm *vm, gravity_module_t *m);
GRAVITY_API void                gravity_module_blacken (gravity_vm *vm, gravity_module_t *m);
GRAVITY_API uint32_t            gravity_module_size (gravity_vm *vm, gravity_module_t *m);

// MARK: - FUNCTION -
GRAVITY_API gravity_function_t  *gravity_function_new (gravity_vm *vm, const char *identifier, uint16_t nparams, uint16_t nlocals, uint16_t ntemps, void *code);
GRAVITY_API gravity_function_t  *gravity_function_new_internal (gravity_vm *vm, const char *identifier, gravity_c_internal exec, uint16_t nparams);
GRAVITY_API gravity_function_t  *gravity_function_new_special (gravity_vm *vm, const char *identifier, uint16_t index, void *getter, void *setter);
GRAVITY_API gravity_function_t  *gravity_function_new_bridged (gravity_vm *vm, const char *identifier, void *xdata);
GRAVITY_API uint16_t            gravity_function_cpool_add (gravity_vm *vm, gravity_function_t *f, gravity_value_t v);
GRAVITY_API gravity_value_t     gravity_function_cpool_get (gravity_function_t *f, uint16_t i);
GRAVITY_API void                gravity_function_dump (gravity_function_t *f, code_dump_function codef);
GRAVITY_API void                gravity_function_setouter (gravity_function_t *f, gravity_object_t *outer);
GRAVITY_API void                gravity_function_setxdata (gravity_function_t *f, void *xdata);
GRAVITY_API gravity_list_t      *gravity_function_params_get (gravity_vm *vm, gravity_function_t *f);
GRAVITY_API void                gravity_function_serialize (gravity_function_t *f, json_t *json);
GRAVITY_API uint32_t            *gravity_bytecode_deserialize (const char *buffer, size_t len, uint32_t *ninst);
GRAVITY_API gravity_function_t  *gravity_function_deserialize (gravity_vm *vm, json_value *json);
GRAVITY_API void                gravity_function_free (gravity_vm *vm, gravity_function_t *f);
GRAVITY_API void                gravity_function_blacken (gravity_vm *vm, gravity_function_t *f);
GRAVITY_API uint32_t            gravity_function_size (gravity_vm *vm, gravity_function_t *f);

// MARK: - CLOSURE -
GRAVITY_API gravity_closure_t   *gravity_closure_new (gravity_vm *vm, gravity_function_t *f);
GRAVITY_API void                gravity_closure_free (gravity_vm *vm, gravity_closure_t *closure);
GRAVITY_API uint32_t            gravity_closure_size (gravity_vm *vm, gravity_closure_t *closure);
GRAVITY_API void                gravity_closure_inc_refcount (gravity_vm *vm, gravity_closure_t *closure);
GRAVITY_API void                gravity_closure_dec_refcount (gravity_vm *vm, gravity_closure_t *closure);
GRAVITY_API void                gravity_closure_blacken (gravity_vm *vm, gravity_closure_t *closure);

// MARK: - UPVALUE -
GRAVITY_API gravity_upvalue_t   *gravity_upvalue_new (gravity_vm *vm, gravity_value_t *value);
GRAVITY_API uint32_t            gravity_upvalue_size (gravity_vm *vm, gravity_upvalue_t *upvalue);
GRAVITY_API void                gravity_upvalue_blacken (gravity_vm *vm, gravity_upvalue_t *upvalue);
GRAVITY_API void                gravity_upvalue_free(gravity_vm *vm, gravity_upvalue_t *upvalue);

// MARK: - CLASS -
GRAVITY_API void                gravity_class_bind (gravity_class_t *c, const char *key, gravity_value_t value);
GRAVITY_API gravity_class_t     *gravity_class_getsuper (gravity_class_t *c);
GRAVITY_API bool                gravity_class_grow (gravity_class_t *c, uint32_t n);
GRAVITY_API bool                gravity_class_setsuper (gravity_class_t *subclass, gravity_class_t *superclass);
GRAVITY_API bool                gravity_class_setsuper_extern (gravity_class_t *baseclass, const char *identifier);
GRAVITY_API gravity_class_t     *gravity_class_new_single (gravity_vm *vm, const char *identifier, uint32_t nfields);
GRAVITY_API gravity_class_t     *gravity_class_new_pair (gravity_vm *vm, const char *identifier, gravity_class_t *superclass, uint32_t nivar, uint32_t nsvar);
GRAVITY_API gravity_class_t     *gravity_class_get_meta (gravity_class_t *c);
GRAVITY_API bool                gravity_class_is_meta (gravity_class_t *c);
GRAVITY_API bool                gravity_class_is_anon (gravity_class_t *c);
GRAVITY_API uint32_t            gravity_class_count_ivars (gravity_class_t *c);
GRAVITY_API void                gravity_class_dump (gravity_class_t *c);
GRAVITY_API void                gravity_class_setxdata (gravity_class_t *c, void *xdata);
GRAVITY_API int16_t             gravity_class_add_ivar (gravity_class_t *c, const char *identifier);
GRAVITY_API void                gravity_class_serialize (gravity_class_t *c, json_t *json);
GRAVITY_API gravity_class_t     *gravity_class_deserialize (gravity_vm *vm, json_value *json);
GRAVITY_API void                gravity_class_free (gravity_vm *vm, gravity_class_t *c);
GRAVITY_API void                gravity_class_free_core (gravity_vm *vm, gravity_class_t *c);
GRAVITY_API gravity_object_t    *gravity_class_lookup (gravity_class_t *c, gravity_value_t key);
GRAVITY_API gravity_closure_t   *gravity_class_lookup_closure (gravity_class_t *c, gravity_value_t key);
GRAVITY_API gravity_closure_t   *gravity_class_lookup_constructor (gravity_class_t *c, uint32_t nparams);
GRAVITY_API gravity_class_t     *gravity_class_lookup_class_identifier (gravity_class_t *c, const char *identifier);
GRAVITY_API void                gravity_class_blacken (gravity_vm *vm, gravity_class_t *c);
GRAVITY_API uint32_t            gravity_class_size (gravity_vm *vm, gravity_class_t *c);

// MARK: - FIBER -
GRAVITY_API gravity_fiber_t     *gravity_fiber_new (gravity_vm *vm, gravity_closure_t *closure, uint32_t nstack, uint32_t nframes);
GRAVITY_API void                gravity_fiber_reassign (gravity_fiber_t *fiber, gravity_closure_t *closure, uint16_t nargs);
GRAVITY_API void                gravity_fiber_seterror (gravity_fiber_t *fiber, const char *error);
GRAVITY_API void                gravity_fiber_reset (gravity_fiber_t *fiber);
GRAVITY_API void                gravity_fiber_free (gravity_vm *vm, gravity_fiber_t *fiber);
GRAVITY_API void                gravity_fiber_blacken (gravity_vm *vm, gravity_fiber_t *fiber);
GRAVITY_API uint32_t            gravity_fiber_size (gravity_vm *vm, gravity_fiber_t *fiber);

// MARK: - INSTANCE -
GRAVITY_API gravity_instance_t  *gravity_instance_new (gravity_vm *vm, gravity_class_t *c);
GRAVITY_API gravity_instance_t  *gravity_instance_clone (gravity_vm *vm, gravity_instance_t *src_instance);
GRAVITY_API void                gravity_instance_setivar (gravity_instance_t *instance, uint32_t idx, gravity_value_t value);
GRAVITY_API void                gravity_instance_setxdata (gravity_instance_t *i, void *xdata);
GRAVITY_API void                gravity_instance_free (gravity_vm *vm, gravity_instance_t *i);
GRAVITY_API gravity_closure_t   *gravity_instance_lookup_event (gravity_instance_t *i, const char *name);
GRAVITY_API gravity_value_t     gravity_instance_lookup_property (gravity_vm *vm, gravity_instance_t *i, gravity_value_t key);
GRAVITY_API void                gravity_instance_blacken (gravity_vm *vm, gravity_instance_t *i);
GRAVITY_API uint32_t            gravity_instance_size (gravity_vm *vm, gravity_instance_t *i);
GRAVITY_API void                gravity_instance_serialize (gravity_instance_t *i, json_t *json);
GRAVITY_API bool                gravity_instance_isstruct (gravity_instance_t *i);

// MARK: - VALUE -
GRAVITY_API bool                gravity_value_equals (gravity_value_t v1, gravity_value_t v2);
GRAVITY_API bool                gravity_value_vm_equals (gravity_vm *vm, gravity_value_t v1, gravity_value_t v2);
GRAVITY_API uint32_t            gravity_value_hash (gravity_value_t value);
GRAVITY_API gravity_class_t     *gravity_value_getclass (gravity_value_t v);
GRAVITY_API gravity_class_t     *gravity_value_getsuper (gravity_value_t v);
GRAVITY_API void                gravity_value_free (gravity_vm *vm, gravity_value_t v);
GRAVITY_API void                gravity_value_serialize (const char *key, gravity_value_t v, json_t *json);
GRAVITY_API void                gravity_value_dump (gravity_vm *vm, gravity_value_t v, char *buffer, uint16_t len);
GRAVITY_API bool                gravity_value_isobject (gravity_value_t v);
GRAVITY_API void                *gravity_value_xdata (gravity_value_t value);
GRAVITY_API const char          *gravity_value_name (gravity_value_t value);
GRAVITY_API void                gravity_value_blacken (gravity_vm *vm, gravity_value_t v);
GRAVITY_API uint32_t            gravity_value_size (gravity_vm *vm, gravity_value_t v);

GRAVITY_API gravity_value_t     gravity_value_from_error(const char* msg);
GRAVITY_API gravity_value_t     gravity_value_from_object(void *obj);
GRAVITY_API gravity_value_t     gravity_value_from_int(gravity_int_t n);
GRAVITY_API gravity_value_t     gravity_value_from_float(gravity_float_t f);
GRAVITY_API gravity_value_t     gravity_value_from_null(void);
GRAVITY_API gravity_value_t     gravity_value_from_undefined(void);
GRAVITY_API gravity_value_t     gravity_value_from_bool(bool b);

// MARK: - OBJECT -
GRAVITY_API void                gravity_object_serialize (gravity_object_t *obj, json_t *json);
GRAVITY_API gravity_object_t    *gravity_object_deserialize (gravity_vm *vm, json_value *entry);
GRAVITY_API void                gravity_object_free (gravity_vm *vm, gravity_object_t *obj);
GRAVITY_API void                gravity_object_blacken (gravity_vm *vm, gravity_object_t *obj);
GRAVITY_API uint32_t            gravity_object_size (gravity_vm *vm, gravity_object_t *obj);
GRAVITY_API const char          *gravity_object_debug (gravity_object_t *obj, bool is_free);

// MARK: - LIST -
GRAVITY_API gravity_list_t      *gravity_list_new (gravity_vm *vm, uint32_t n);
GRAVITY_API gravity_list_t      *gravity_list_from_array (gravity_vm *vm, uint32_t n, gravity_value_t *p);
GRAVITY_API void                gravity_list_free (gravity_vm *vm, gravity_list_t *list);
GRAVITY_API void                gravity_list_append_list (gravity_vm *vm, gravity_list_t *list1, gravity_list_t *list2);
GRAVITY_API void                gravity_list_blacken (gravity_vm *vm, gravity_list_t *list);
GRAVITY_API uint32_t            gravity_list_size (gravity_vm *vm, gravity_list_t *list);

// MARK: - MAP -
GRAVITY_API gravity_map_t       *gravity_map_new (gravity_vm *vm, uint32_t n);
GRAVITY_API void                gravity_map_free (gravity_vm *vm, gravity_map_t *map);
GRAVITY_API void                gravity_map_append_map (gravity_vm *vm, gravity_map_t *map1, gravity_map_t *map2);
GRAVITY_API void                gravity_map_insert (gravity_vm *vm, gravity_map_t *map, gravity_value_t key, gravity_value_t value);
GRAVITY_API void                gravity_map_blacken (gravity_vm *vm, gravity_map_t *map);
GRAVITY_API uint32_t            gravity_map_size (gravity_vm *vm, gravity_map_t *map);

// MARK: - RANGE -
GRAVITY_API gravity_range_t     *gravity_range_new (gravity_vm *vm, gravity_int_t from, gravity_int_t to, bool inclusive);
GRAVITY_API void                gravity_range_free (gravity_vm *vm, gravity_range_t *range);
GRAVITY_API void                gravity_range_blacken (gravity_vm *vm, gravity_range_t *range);
GRAVITY_API uint32_t            gravity_range_size (gravity_vm *vm, gravity_range_t *range);
GRAVITY_API void                gravity_range_serialize (gravity_range_t *r, json_t *json);
GRAVITY_API gravity_range_t     *gravity_range_deserialize (gravity_vm *vm, json_value *json);

/// MARK: - STRING -
GRAVITY_API gravity_value_t     gravity_string_to_value (gravity_vm *vm, const char *s, uint32_t len);
GRAVITY_API gravity_string_t    *gravity_string_new (gravity_vm *vm, char *s, uint32_t len, uint32_t alloc);
GRAVITY_API void				gravity_string_set(gravity_string_t *obj, char *s, uint32_t len);
GRAVITY_API void                gravity_string_free (gravity_vm *vm, gravity_string_t *value);
GRAVITY_API void                gravity_string_blacken (gravity_vm *vm, gravity_string_t *string);
GRAVITY_API uint32_t            gravity_string_size (gravity_vm *vm, gravity_string_t *string);

// MARK: - CALLBACKS -
// HASH FREE CALLBACK FUNCTION
GRAVITY_API void                gravity_hash_keyvaluefree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data);
GRAVITY_API void                gravity_hash_keyfree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data);
GRAVITY_API void                gravity_hash_valuefree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data);
GRAVITY_API void                gravity_hash_finteralfree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data);

#ifdef __cplusplus
}
#endif

#endif
