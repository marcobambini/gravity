//
//  gravity_vmmacros.h
//  gravity
//
//  Created by Marco Bambini on 08/10/15.
//  Copyright © 2015 Creolabs. All rights reserved.
//

#ifndef __GRAVITY_VMMACROS__
#define __GRAVITY_VMMACROS__

// MACROS used in VM

#if 0
#define DEBUG_CALL(s, f)                                printf("%s %s\n", s, f->identifier)
#else
#define DEBUG_CALL(s, f)
#endif

// signed operation decoding (OPCODE_GET_ONE8bit_SIGN_ONE17bit) from my question on stackoverflow
// http://stackoverflow.com/questions/37054769/optimize-a-bit-decoding-operation-in-c?noredirect=1#comment61673505_37054769

#define OPCODE_GET_OPCODE(op)                           ((op >> 26) & 0x3F)
#define OPCODE_GET_ONE8bit_FLAG_ONE17bit(op,r1,f,n)     r1 = (op >> 18) & 0xFF; f = (op >> 17) & 0x01; n = (int32_t)(op & 0x1FFFF)
#define OPCODE_GET_ONE8bit_SIGN_ONE17bit(op,r1,n)       r1 = (op >> 18) & 0xFF; n = ((int32_t)(op & 0x1FFFF) - (int32_t)(op & 0x20000))
#define OPCODE_GET_TWO8bit_ONE10bit(op,r1,r2,r3)        r1 = (op >> 18) & 0xFF; r2 = (op >> 10) & 0xFF; r3 = (op & 0x3FF)
#define OPCODE_GET_ONE8bit(op,r1)                       r1 = (op >> 18) & 0xFF;
#define OPCODE_GET_SIGN_ONE25bit(op, n)                 n = ((op >> 25) & 0x01) ? -(op & 0x1FFFFFF) : (op & 0x1FFFFFF)
#define OPCODE_GET_ONE8bit_ONE18bit(op,r1,n)            r1 = (op >> 18) & 0xFF; n = (op & 0x3FFFF)
#define OPCODE_GET_LAST18bit(op,n)                      n = (op & 0x3FFFF)
#define OPCODE_GET_ONE26bit(op, n)                      n = (op & 0x3FFFFFF)
#define OPCODE_GET_ONE8bit_ONE10bit(op,r1,r3)           r1 = (op >> 18) & 0xFF; r3 = (op & 0x3FF)
#define OPCODE_GET_THREE8bit(op,r1,r2,r3)               OPCODE_GET_TWO8bit_ONE10bit(op,r1,r2,r3)
#define OPCODE_GET_FOUR8bit(op,r1,r2,r3,r4)             r1 = (op >> 24) & 0xFF; r2 = (op >> 16) & 0xFF; r3 = (op >> 8) & 0xFF; r4 = (op & 0xFF)
#define OPCODE_GET_THREE8bit_ONE2bit(op,r1,r2,r3,r4)    r1 = (op >> 18) & 0xFF; r2 = (op >> 10) & 0xFF; r3 = (op >> 2) & 0xFF; r4 = (op & 0x03)

#define GRAVITY_VM_DEBUG                0               // print each VM instruction
#define GRAVITY_VM_STATS                0               // print VM related stats after each execution
#define GRAVITY_GC_STATS                0               // print useful stats each time GC runs
#define GRAVITY_GC_STRESSTEST           0               // force a GC run after each memory allocation
#define GRAVITY_GC_DEBUG                0               // print objects transferred and grayed
#define GRAVITY_STACK_DEBUG             0               // dump the stack at each CALL and in some other places
#define GRAVITY_TRUST_USERCODE          0               // set at 1 at your own risk!
                                                        // when 0 each time an internal or a bridge function is executed the GC is disabled
                                                        // in this way user does not have to completely understand how GC works under the hood

#if GRAVITY_STACK_DEBUG
#define DEBUG_STACK()                   gravity_stack_dump(fiber)
#else
#define DEBUG_STACK()
#endif

#if GRAVITY_GC_DEBUG
#define DEBUG_GC(...)                   printf(__VA_ARGS__);printf("\n");fflush(stdout)
#else
#define DEBUG_GC(...)
#endif

#if GRAVITY_VM_DEBUG
#define DEBUG_VM(...)                   DEBUG_STACK();printf("%06u\t",vm->pc); printf(__VA_ARGS__);printf("\n");fflush(stdout)
#define DEBUG_VM_NOCR(...)              DEBUG_STACK();printf("%06u\t",vm->pc); printf(__VA_ARGS__);fflush(stdout)
#define DEBUG_VM_RAW(...)               printf(__VA_ARGS__);fflush(stdout)
#define INC_PC                          ++vm->pc;
#else
#define DEBUG_VM(...)
#define DEBUG_VM_NOCR(...)
#define DEBUG_VM_RAW(...)
#define INC_PC
#endif

#if GRAVITY_TRUST_USERCODE
#define BEGIN_TRUST_USERCODE(_vm)
#define END_TRUST_USERCODE(_vm)
#else
#define BEGIN_TRUST_USERCODE(_vm)       gravity_gc_setenabled(_vm, false)
#define END_TRUST_USERCODE(_vm)         gravity_gc_setenabled(_vm, true)
#endif

#if GRAVITY_VM_STATS
#define RESET_STATS(_vm)                bzero(_vm->nstat, sizeof(_vm->nstat)); bzero(_vm->tstat, sizeof(_vm->tstat))
#define PRINT_STATS(_vm)                gravity_vm_stats(_vm)
#define START_MICROBENCH(_vm)           _vm->t = nanotime()
#define UPDATE_STATS(_vm,_op)           ++_vm->nstat[_op]; _vm->tstat[_op] += millitime(_vm->t, nanotime())
#define STAT_FRAMES_REALLOCATED(_vm)    ++_vm->nfrealloc
#define STAT_STACK_REALLOCATED(_vm)     ++_vm->nsrealloc
#else
#define RESET_STATS(_vm)
#define PRINT_STATS(_vm)
#define START_MICROBENCH(_vm)
#define UPDATE_STATS(_vm,_op)
#define STAT_FRAMES_REALLOCATED(_vm)
#define STAT_STACK_REALLOCATED(_vm)
#endif

// starting from version 0.6.3 a call to STORE_FRAME macro has been added in order to syncronize current IP
// for a better line number computation in case of runtime error (as a consequence ip and frame variables
// has been explicitly exposed in the gravity_vm_runclosure function and the infinite loop error message
// has been moved outside the gravity_check_stack function)
#define RUNTIME_ERROR(...)              do {                                                                \
                                            STORE_FRAME();                                                  \
                                            report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, __VA_ARGS__);   \
                                            return false;                                                   \
                                        } while (0)

#define RUNTIME_FIBER_ERROR(_err)       RUNTIME_ERROR("%s",_err)

#define RUNTIME_WARNING(...)            do {                                                            \
                                            report_runtime_error(vm, GRAVITY_WARNING, __VA_ARGS__);     \
                                        } while (0)

#define SETVALUE_BOOL(idx, x)           stackstart[idx]=VALUE_FROM_BOOL(x)
#define SETVALUE_INT(idx, x)            stackstart[idx]=VALUE_FROM_INT(x)
#define SETVALUE_FLOAT(idx, x)          stackstart[idx]=VALUE_FROM_FLOAT(x)
#define SETVALUE_NULL(idx)              stackstart[idx]=VALUE_FROM_NULL
#define SETVALUE(idx, x)                stackstart[idx]=x
#define GETVALUE_INT(v)                 v.n
#define GETVALUE_FLOAT(v)               v.f
#define STACK_GET(idx)                  stackstart[idx]
// macro the count number of registers needed by the _f function which is the sum of local variables, temp variables and formal parameters
#define FN_COUNTREG(_f,_nargs)          (MAXNUM(_f->nparams,_nargs) + _f->nlocals + _f->ntemps)

#if GRAVITY_COMPUTED_GOTO
#define DECLARE_DISPATCH_TABLE      static void* dispatchTable[] = {                                \
                                    &&RET0,         &&HALT,         &&NOP,          &&RET,          \
                                    &&CALL,         &&LOAD,         &&LOADS,        &&LOADAT,       \
                                    &&LOADK,        &&LOADG,        &&LOADI,        &&LOADU,        \
                                    &&MOVE,         &&STORE,        &&STOREAT,      &&STOREG,       \
                                    &&STOREU,       &&JUMP,         &&JUMPF,        &&SWITCH,       \
                                    &&ADD,          &&SUB,          &&DIV,          &&MUL,          \
                                    &&REM,          &&AND,          &&OR,           &&LT,           \
                                    &&GT,           &&EQ,           &&LEQ,          &&GEQ,          \
                                    &&NEQ,          &&EQQ,          &&NEQQ,         &&ISA,          \
                                    &&MATCH,        &&NEG,          &&NOT,          &&LSHIFT,       \
                                    &&RSHIFT,       &&BAND,         &&BOR,          &&BXOR,         \
                                    &&BNOT,         &&MAPNEW,       &&LISTNEW,      &&RANGENEW,     \
                                    &&SETLIST,      &&CLOSURE,      &&CLOSE,        &&CHECK,        \
                                    &&RESERVED2,    &&RESERVED3,    &&RESERVED4,    &&RESERVED5,    \
                                    &&RESERVED6                                                        };
#define INTERPRET_LOOP              DISPATCH();
#define CASE_CODE(name)             START_MICROBENCH(vm); name
#if GRAVITY_VM_STATS
#define DISPATCH()                  DEBUG_STACK();INC_PC;inst = *ip++;op = (opcode_t)OPCODE_GET_OPCODE(inst);UPDATE_STATS(vm,op);goto *dispatchTable[op];
#else
#define DISPATCH()                  DEBUG_STACK();INC_PC;inst = *ip++;goto *dispatchTable[op = (opcode_t)OPCODE_GET_OPCODE(inst)];
#endif
#define DISPATCH_INNER()            DISPATCH()
#else
#define DECLARE_DISPATCH_TABLE
#define INTERPRET_LOOP              vm_loop: inst = *ip++;op = (opcode_t)OPCODE_GET_OPCODE(inst);UPDATE_STATS(op); switch (op)
#define CASE_CODE(name)             case name
#define DISPATCH()                  break
#define DISPATCH_INNER()            goto vm_loop
#endif

#define INIT_PARAMS(n)              for (uint32_t i=n; i<func->nparams; ++i)            \
                                    stackstart[i] = VALUE_FROM_UNDEFINED;

#define STORE_FRAME()               frame->ip = ip

#define LOAD_FRAME()                if (vm->aborted) return false;                                                  \
                                    frame = &fiber->frames[fiber->nframes - 1];                                     \
                                    stackstart = frame->stackstart;                                                 \
                                    ip = frame->ip;                                                                 \
                                    func = frame->closure->f;                                                       \
                                    DEBUG_VM_RAW("******\tEXEC %s (%p) ******\n", func->identifier, func)

#define USE_ARGS(_c)                (_c->f->tag == EXEC_TYPE_NATIVE && _c->f->useargs)
#define PUSH_FRAME(_c,_s,_r,_n)     gravity_callframe_t *cframe = gravity_new_callframe(vm, fiber);                 \
                                    if (!cframe) return false;                                                      \
                                    cframe->closure = _c;                                                           \
                                    cframe->stackstart = _s;                                                        \
                                    cframe->ip = _c->f->bytecode;                                                   \
                                    cframe->dest = _r;                                                              \
                                    cframe->nargs = _n;                                                             \
                                    cframe->outloop = false;                                                        \
                                    cframe->args = (USE_ARGS(_c)) ? gravity_list_from_array(vm, _n-1, _s+1) : NULL

// SYNC_STACKTOP has been modified in version 0.5.8 (December 4th 2018)
// stack must be trashed ONLY in the fiber remains the same otherwise GC will collect stack values from a still active Fiber
#define SYNC_STACKTOP(_fiber_saved, _fiber,_n)      if (_fiber_saved && (_fiber_saved == _fiber)) _fiber_saved->stacktop -= _n
#define SETFRAME_OUTLOOP(cframe)                    (cframe)->outloop = true

#define COMPUTE_JUMP(value)                         (func->bytecode + (value))

// FAST MATH MACROS
#define FMATH_BIN_INT(_r1,_v2,_v3,_OP)              do {SETVALUE(_r1, VALUE_FROM_INT(_v2 _OP _v3)); DISPATCH_INNER();} while(0)
#define FMATH_BIN_FLOAT(_r1,_v2,_v3,_OP)            do {SETVALUE(_r1, VALUE_FROM_FLOAT(_v2 _OP _v3)); DISPATCH_INNER();} while(0)
#define FMATH_BIN_BOOL(_r1,_v2,_v3,_OP)             do {SETVALUE(_r1, VALUE_FROM_BOOL(_v2 _OP _v3)); DISPATCH_INNER();} while(0)

#define DEFINE_STACK_VARIABLE(_v,_r)                register gravity_value_t _v = STACK_GET(_r)
#define DEFINE_INDEX_VARIABLE(_v,_r)                register gravity_value_t _v = (_r < MAX_REGISTERS) ? STACK_GET(_r) : VALUE_FROM_INT(_r-MAX_REGISTERS)

#define NO_CHECK
#define CHECK_ZERO(_v)                              if ((VALUE_ISA_INT(_v) && (_v.n == 0)) || (VALUE_ISA_FLOAT(_v) && (_v.f == 0.0)) || (VALUE_ISA_NULL(_v))) \
                                                    RUNTIME_ERROR("Division by 0 error.")

#define CHECK_FAST_BINARY_BOOL(r1,r2,r3,v2,v3,OP)   DEFINE_STACK_VARIABLE(v2,r2);                                                           \
                                                    DEFINE_STACK_VARIABLE(v3,r3);                                                           \
                                                    if (VALUE_ISA_BOOL(v2) && VALUE_ISA_BOOL(v3)) FMATH_BIN_BOOL(r1, v2.n, v3.n, OP)

#define CHECK_FAST_UNARY_BOOL(r1,r2,v2,OP)          DEFINE_STACK_VARIABLE(v2,r2);                                                           \
                                                    if (VALUE_ISA_BOOL(v2)) {SETVALUE(r1, VALUE_FROM_BOOL(OP v2.n)); DISPATCH();}

// fast math only for INT and FLOAT
#define CHECK_FAST_BINARY_MATH(r1,r2,r3,v2,v3,OP,_CHECK)                                                                                                        \
                                                    DEFINE_STACK_VARIABLE(v2,r2);                                                                               \
                                                    DEFINE_STACK_VARIABLE(v3,r3);                                                                               \
                                                    _CHECK;                                                                                                     \
                                                    if (VALUE_ISA_INT(v2)) {                                                                                    \
                                                        if (VALUE_ISA_INT(v3)) FMATH_BIN_INT(r1, v2.n, v3.n, OP);                                               \
                                                        if (VALUE_ISA_FLOAT(v3)) FMATH_BIN_FLOAT(r1, v2.n, v3.f, OP);                                           \
                                                        if (VALUE_ISA_NULL(v3)) FMATH_BIN_INT(r1, v2.n, 0, OP);                                                 \
                                                        if (VALUE_ISA_STRING(v3)) RUNTIME_ERROR("Right operand must be a number (use the number() method).");   \
                                                    } else if (VALUE_ISA_FLOAT(v2)) {                                                                           \
                                                        if (VALUE_ISA_FLOAT(v3)) FMATH_BIN_FLOAT(r1, v2.f, v3.f, OP);                                           \
                                                        if (VALUE_ISA_INT(v3)) FMATH_BIN_FLOAT(r1, v2.f, v3.n, OP);                                             \
                                                        if (VALUE_ISA_NULL(v3)) FMATH_BIN_FLOAT(r1, v2.f, 0, OP);                                               \
                                                        if (VALUE_ISA_STRING(v3)) RUNTIME_ERROR("Right operand must be a number (use the number() method).");   \
                                                    }

#define CHECK_FAST_UNARY_MATH(r1,r2,v2,OP)          DEFINE_STACK_VARIABLE(v2,r2);                                                   \
                                                    if (VALUE_ISA_INT(v2)) {SETVALUE(r1, VALUE_FROM_INT(OP v2.n)); DISPATCH();}     \
                                                    if (VALUE_ISA_FLOAT(v2)) {SETVALUE(r1, VALUE_FROM_FLOAT(OP v2.f)); DISPATCH();}


#define CHECK_FAST_BINARY_REM(r1,r2,r3,v2,v3)       DEFINE_STACK_VARIABLE(v2,r2);                                                               \
                                                    DEFINE_STACK_VARIABLE(v3,r3);                                                               \
                                                    CHECK_ZERO(v3);                                                                             \
                                                    if (VALUE_ISA_INT(v2) && VALUE_ISA_INT(v3)) FMATH_BIN_INT(r1, v2.n, v3.n, %)

#define CHECK_FAST_BINARY_BIT(r1,r2,r3,v2,v3,OP)    DEFINE_STACK_VARIABLE(v2,r2);                                                               \
                                                    DEFINE_STACK_VARIABLE(v3,r3);                                                               \
                                                    if (VALUE_ISA_INT(v2) && VALUE_ISA_INT(v3)) FMATH_BIN_INT(r1, v2.n, v3.n, OP)

#define CHECK_FAST_BINBOOL_BIT(r1,v2,v3,OP)         if (VALUE_ISA_BOOL(v2) && VALUE_ISA_BOOL(v3)) FMATH_BIN_BOOL(r1, v2.n, v3.n, OP)

#define DECODE_BINARY_OPERATION(r1,r2,r3)           OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3); \
                                                    DEBUG_VM("%s %d %d %d", opcode_name(op), r1, r2, r3)

#define PREPARE_FUNC_CALLN(_c,_i,_w,_N)             gravity_closure_t *_c = (gravity_closure_t *)gravity_class_lookup_closure(gravity_value_getclass(v2), cache[_i]); \
                                                    if (!_c || !_c->f) RUNTIME_ERROR("Unable to perform operator %s on object", opcode_name(op));   \
                                                    uint32_t _w = FN_COUNTREG(func, frame->nargs); \
                                                    uint32_t _rneed = FN_COUNTREG(_c->f, _N);      \
													uint32_t stacktopdelta = (uint32_t)MAXNUM(stackstart + _w + _rneed - fiber->stacktop, 0); \
                                                    if (!gravity_check_stack(vm, fiber, stacktopdelta, &stackstart)) return false;              \
                                                    if (vm->aborted) return false

#define PREPARE_FUNC_CALL1(_c,_v1,_i,_w)            PREPARE_FUNC_CALLN(_c,_i,_w,1);         \
                                                    SETVALUE(_w, _v1)


#define PREPARE_FUNC_CALL2(_c,_v1,_v2,_i,_w)        PREPARE_FUNC_CALLN(_c,_i,_w,2);         \
                                                    SETVALUE(_w, _v1);                      \
                                                    SETVALUE(_w+1, _v2)

#define PREPARE_FUNC_CALL3(_c,_v1,_v2,_v3,_i,_w)    PREPARE_FUNC_CALLN(_c,_i,_w,3);         \
                                                    SETVALUE(_w, _v1);                      \
                                                    SETVALUE(_w+1, _v2);                    \
                                                    SETVALUE(_w+2, _v3)


#define CALL_FUNC(_name,_c,r1,nargs,rwin)           gravity_fiber_t *current_fiber = fiber;                             \
                                                    STORE_FRAME();                                                      \
                                                    execute_op_##_name:                                                 \
                                                    switch(_c->f->tag) {                                                \
                                                    case EXEC_TYPE_NATIVE: {                                            \
                                                        current_fiber = NULL;                                           \
                                                        PUSH_FRAME(_c, &stackstart[rwin], r1, nargs);                   \
                                                    } break;                                                            \
                                                    case EXEC_TYPE_INTERNAL: {                                          \
                                                        BEGIN_TRUST_USERCODE(vm);                                       \
                                                        bool result = _c->f->internal(vm, &stackstart[rwin], nargs, r1);\
                                                        END_TRUST_USERCODE(vm);                                         \
                                                        if (!result) {       \
                                                            if (vm->aborted) return false;                              \
                                                            if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {                     \
                                                                closure = VALUE_AS_CLOSURE(STACK_GET(r1));              \
                                                                SETVALUE(r1, VALUE_FROM_NULL);                          \
                                                                goto execute_op_##_name;                                \
                                                            }                                                           \
                                                            fiber = vm->fiber;                                          \
                                                            if (fiber == NULL) return true;                             \
                                                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);        \
                                                        }                                                               \
                                                    } break;                                                            \
                                                    case EXEC_TYPE_BRIDGED:    {                                        \
                                                        DEBUG_ASSERT(delegate->bridge_execute, "bridge_execute delegate callback is mandatory");        \
                                                        BEGIN_TRUST_USERCODE(vm);                                       \
                                                        bool result = delegate->bridge_execute(vm, _c->f->xdata, STACK_GET(0), &stackstart[rwin], nargs, r1); \
                                                        END_TRUST_USERCODE(vm);                                         \
                                                        if (!result) {                                                  \
                                                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);        \
                                                        }                                                               \
                                                    } break;                                                            \
                                                    case EXEC_TYPE_SPECIAL:                                             \
                                                        RUNTIME_ERROR("Unable to handle a special function in current context");    \
                                                        break;                                                          \
                                                    }                                                                   \
                                                    LOAD_FRAME();                                                       \
                                                    SYNC_STACKTOP(current_fiber, fiber, stacktopdelta)

// MACROS used in core and optionals
#define SETMETA_INITED(c)                           gravity_class_get_meta(c)->is_inited = true
#define GET_VALUE(_idx)                             args[_idx]
#define RETURN_VALUE(_v,_i)                         do {gravity_vm_setslot(vm, _v, _i); return true;} while(0)
#define RETURN_CLOSURE(_v,_i)                       do {gravity_vm_setslot(vm, _v, _i); return false;} while(0)
#define RETURN_FIBER()                              return false
#define RETURN_NOVALUE()                            return true
#define RETURN_ERROR(...)                           do {                                                                                    \
                                                        char _buffer[4096];                                                                 \
                                                        snprintf(_buffer, sizeof(_buffer), __VA_ARGS__);                                    \
                                                        gravity_fiber_seterror(gravity_vm_fiber(vm), (const char *) _buffer);               \
                                                        gravity_vm_setslot(vm, VALUE_FROM_NULL, rindex);                                    \
                                                        return false;                                                                       \
                                                    } while(0)
#define RETURN_ERROR_SIMPLE()                       do {                                                                                    \
                                                        gravity_vm_setslot(vm, VALUE_FROM_NULL, rindex);                                    \
                                                        return false;                                                                       \
                                                    } while(0)
#define CHECK_MEM_ALLOC(_ptr)                       if (!_ptr) RETURN_ERROR_SIMPLE();
#define DECLARE_1VARIABLE(_v,_idx)                  register gravity_value_t _v = GET_VALUE(_idx)
#define DECLARE_2VARIABLES(_v1,_v2,_idx1,_idx2)     DECLARE_1VARIABLE(_v1,_idx1);DECLARE_1VARIABLE(_v2,_idx2)

#define CHECK_VALID(_check,_v,_msg)                 if ((_check) && VALUE_ISA_NOTVALID(_v)) RETURN_ERROR(_msg)
#define INTERNAL_CONVERT_FLOAT(_v,_check)           _v = convert_value2float(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to Float")
#define INTERNAL_CONVERT_BOOL(_v,_check)            _v = convert_value2bool(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to Bool")
#define INTERNAL_CONVERT_INT(_v,_check)             _v = convert_value2int(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to Int")
#define INTERNAL_CONVERT_STRING(_v,_check)          _v = convert_value2string(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to String")

#define NEW_FUNCTION(_fptr)                         (gravity_function_new_internal(NULL, NULL, _fptr, 0))
#define NEW_CLOSURE_VALUE(_fptr)                    ((gravity_value_t){.isa = gravity_class_closure,.p = (gravity_object_t *)gravity_closure_new(NULL, NEW_FUNCTION(_fptr))})

#define FUNCTION_ISA_SPECIAL(_f)                    (OBJECT_ISA_FUNCTION(_f) && (_f->tag == EXEC_TYPE_SPECIAL))
#define FUNCTION_ISA_DEFAULT_GETTER(_f)             ((_f->index < GRAVITY_COMPUTED_INDEX) && (_f->special[EXEC_TYPE_SPECIAL_GETTER] == NULL))
#define FUNCTION_ISA_DEFAULT_SETTER(_f)             ((_f->index < GRAVITY_COMPUTED_INDEX) && (_f->special[EXEC_TYPE_SPECIAL_SETTER] == NULL))
#define FUNCTION_ISA_GETTER(_f)                     (_f->special[EXEC_TYPE_SPECIAL_GETTER] != NULL)
#define FUNCTION_ISA_SETTER(_f)                     (_f->special[EXEC_TYPE_SPECIAL_SETTER] != NULL)
#define FUNCTION_ISA_BRIDGED(_f)                    (_f->index == GRAVITY_BRIDGE_INDEX)

#endif
