//
//  gravity_vmmacros.h
//  gravity
//
//  Created by Marco Bambini on 08/10/15.
//  Copyright © 2015 Creolabs. All rights reserved.
//

#ifndef __GRAVITY_VMMACROS__
#define __GRAVITY_VMMACROS__

// Assertions add significant overhead, so are only enabled in debug builds.
#if 1
#define ASSERT(condition, message)						do { \
															if (!(condition)) { \
																fprintf(stderr, "[%s:%d] Assert failed in %s(): %s\n", \
																__FILE__, __LINE__, __func__, message); \
																abort(); \
															} \
														} \
														while(0)
#else
#define ASSERT(condition, message)
#endif

#if 0
#define DEBUG_CALL(s, f)								printf("%s %s\n", s, f->identifier)
#else
#define DEBUG_CALL(s, f)
#endif

// signed operation decoding (OPCODE_GET_ONE8bit_SIGN_ONE17bit) from my question on stackoverflow
// http://stackoverflow.com/questions/37054769/optimize-a-bit-decoding-operation-in-c?noredirect=1#comment61673505_37054769

#define OPCODE_GET_OPCODE(op)							((op >> 26) & 0x3F)
#define OPCODE_GET_ONE8bit_FLAG_ONE17bit(op,r1,f,n)		r1 = (op >> 18) & 0xFF; f = (op >> 17) & 0x01; n = (int32_t)(op & 0x1FFFF)
#define OPCODE_GET_ONE8bit_SIGN_ONE17bit(op,r1,n)		r1 = (op >> 18) & 0xFF; n = ((int32_t)(op & 0x1FFFF) - (int32_t)(op & 0x20000))
#define OPCODE_GET_TWO8bit_ONE10bit(op,r1,r2,r3)		r1 = (op >> 18) & 0xFF; r2 = (op >> 10) & 0xFF; r3 = (op & 0x3FF)
#define OPCODE_GET_ONE8bit(op,r1)						r1 = (op >> 18) & 0xFF;
#define OPCODE_GET_SIGN_ONE25bit(op, n)					n = ((op >> 25) & 0x01) ? -(op & 0x1FFFFFF) : (op & 0x1FFFFFF)
#define OPCODE_GET_ONE8bit_ONE18bit(op,r1,n)			r1 = (op >> 18) & 0xFF; n = (op & 0x3FFFF)
#define OPCODE_GET_LAST18bit(op,n)						n = (op & 0x3FFFF)
#define OPCODE_GET_ONE26bit(op, n)						n = (op & 0x3FFFFFF)
#define OPCODE_GET_ONE8bit_ONE10bit(op,r1,r3)			r1 = (op >> 18) & 0xFF; r3 = (op & 0x3FF)
#define OPCODE_GET_THREE8bit(op,r1,r2,r3)				OPCODE_GET_TWO8bit_ONE10bit(op,r1,r2,r3)
#define OPCODE_GET_FOUR8bit(op,r1,r2,r3,r4)				r1 = (op >> 24) & 0xFF; r2 = (op >> 16) & 0xFF; r3 = (op >> 8) & 0xFF; r4 = (op & 0xFF)
#define OPCODE_GET_THREE8bit_ONE2bit(op,r1,r2,r3,r4)	r1 = (op >> 18) & 0xFF; r2 = (op >> 10) & 0xFF; r3 = (op >> 2) & 0xFF; r4 = (op & 0x03)

#define GRAVITY_VM_DEGUB								0
#define GRAVITY_VM_STATS								0
#define GRAVITY_GC_STATS								0
#define GRAVITY_GC_STRESSTEST							0
#define GRAVITY_GC_DEBUG								0
#define GRAVITY_STACK_DEBUG								0
#define GRAVITY_VM_PREEMPTION							1

#if GRAVITY_STACK_DEBUG
#define DEBUG_STACK()									gravity_stack_dump(fiber)
#else
#define DEBUG_STACK()
#endif

#if GRAVITY_GC_DEBUG
#define DEBUG_GC(...)									printf(__VA_ARGS__);printf("\n");fflush(stdout)
#else
#define DEBUG_GC(...)
#endif

#if GRAVITY_VM_DEGUB
#define DEBUG_VM(...)									DEBUG_STACK();printf("%06u\t",vm->pc); printf(__VA_ARGS__);printf("\n");fflush(stdout)
#define DEBUG_VM_NOCR(...)								DEBUG_STACK();printf("%06u\t",vm->pc); printf(__VA_ARGS__);fflush(stdout)
#define DEBUG_VM_RAW(...)								printf(__VA_ARGS__);fflush(stdout)
#define INC_PC											++vm->pc;
#else
#define DEBUG_VM(...)
#define DEBUG_VM_NOCR(...)
#define DEBUG_VM_RAW(...)
#define INC_PC
#endif

#if GRAVITY_VM_STATS
#define RESET_STATS(_vm)								bzero(_vm->nstat, sizeof(_vm->nstat)); bzero(_vm->tstat, sizeof(_vm->tstat))
#define PRINT_STATS(_vm)								gravity_vm_stats(_vm)
#define START_MICROBENCH(_vm)							_vm->t = nanotime()
#define UPDATE_STATS(_vm,_op)							++_vm->nstat[_op]; _vm->tstat[_op] += millitime(_vm->t, nanotime())
#define STAT_FRAMES_REALLOCATED(_vm)					++_vm->nfrealloc
#define STAT_STACK_REALLOCATED(_vm)						++_vm->nsrealloc
#else
#define RESET_STATS(_vm)
#define PRINT_STATS(_vm)
#define START_MICROBENCH(_vm)
#define UPDATE_STATS(_vm,_op)
#define STAT_FRAMES_REALLOCATED(_vm)
#define STAT_STACK_REALLOCATED(_vm)
#endif

#define RUNTIME_ERROR(...)								do {																\
															report_runtime_error(vm, GRAVITY_ERROR_RUNTIME, __VA_ARGS__);	\
															return false;													\
														} while (0)

#define RUNTIME_FIBER_ERROR(_err)						RUNTIME_ERROR("%s",_err)

#define RUNTIME_WARNING(...)							do {																\
															report_runtime_error(vm, GRAVITY_WARNING, __VA_ARGS__);			\
														} while (0)

#define SETVALUE_BOOL(idx, x)							stackstart[idx]=VALUE_FROM_BOOL(x)
#define SETVALUE_INT(idx, x)							stackstart[idx]=VALUE_FROM_INT(x)
#define SETVALUE_FLOAT(idx, x)							stackstart[idx]=VALUE_FROM_FLOAT(x)
#define SETVALUE_NULL(idx)								stackstart[idx]=VALUE_FROM_NULL
#define SETVALUE(idx, x)								stackstart[idx]=x
#define GETVALUE_INT(v)									v.n
#define GETVALUE_FLOAT(v)								v.f
#define STACK_GET(idx)									stackstart[idx]
// macro the count number of registers needed by the _f function which is the sum of local variables, temp variables and formal parameters
#define FN_COUNTREG(_f,_nargs)							(MAXNUM(_f->nparams,_nargs) + _f->nlocals + _f->ntemps)

#if GRAVITY_VM_PREEMPTION
#define MAYBE_PREEMPT                                   if (preempt_cb && preempt_cb(vm, vm->delegate->xdata)) goto DO_PREEMPT;
#else
#define MAYBE_PREEMPT
#endif
#if GRAVITY_COMPUTED_GOTO
#define DECLARE_DISPATCH_TABLE		static void* dispatchTable[] = {								\
									&&RET0,			&&HALT,			&&NOP,			&&RET,			\
									&&CALL,			&&LOAD,			&&LOADS,		&&LOADAT,		\
									&&LOADK,		&&LOADG,		&&LOADI,		&&LOADU,		\
									&&MOVE,			&&STORE,		&&STOREAT,		&&STOREG,		\
									&&STOREU,		&&JUMP,			&&JUMPF,		&&SWITCH,		\
									&&ADD,			&&SUB,			&&DIV,			&&MUL,			\
									&&REM,			&&AND,			&&OR,			&&LT,			\
									&&GT,			&&EQ,			&&LEQ,			&&GEQ,			\
									&&NEQ,			&&EQQ,			&&NEQQ,			&&ISA,			\
									&&MATCH,		&&NEG,			&&NOT,			&&LSHIFT,		\
									&&RSHIFT,		&&BAND,			&&BOR,			&&BXOR,			\
									&&BNOT,			&&MAPNEW,		&&LISTNEW,		&&RANGENEW,		\
									&&SETLIST,		&&CLOSURE,		&&CLOSE,		&&RESERVED1,	\
									&&RESERVED2,	&&RESERVED3,	&&RESERVED4,	&&RESERVED5,	\
									&&RESERVED6														};
#define INTERPRET_LOOP				DISPATCH();
#define CASE_CODE(name)				START_MICROBENCH(vm); name
#if GRAVITY_VM_STATS
#define DISPATCH()					DEBUG_STACK();MAYBE_PREEMPT;INC_PC;inst = *ip++;op = (opcode_t)OPCODE_GET_OPCODE(inst);UPDATE_STATS(vm,op);goto *dispatchTable[op];
#else
#define DISPATCH()					DEBUG_STACK();MAYBE_PREEMPT;INC_PC;inst = *ip++;goto *dispatchTable[op = (opcode_t)OPCODE_GET_OPCODE(inst)];
#endif
#else
#define DECLARE_DISPATCH_TABLE
#define INTERPRET_LOOP				inst = *ip++;op = (opcode_t)OPCODE_GET_OPCODE(inst);UPDATE_STATS(op);switch (op)
#define CASE_CODE(name)				case name
#define DISPATCH()					break
#endif

#define INIT_PARAMS(n)				for (uint32_t i=n; i<func->nparams; ++i)															\
									stackstart[i] = VALUE_FROM_UNDEFINED;

#define STORE_FRAME()				frame->ip = ip

#define LOAD_FRAME()				frame = &fiber->frames[fiber->nframes - 1];															\
									stackstart = frame->stackstart;																		\
									ip = frame->ip;																						\
									func = frame->closure->f;																			\
									DEBUG_VM_RAW("******\tEXEC %s (%p) ******\n", func->identifier, func);

#define PUSH_FRAME(_c,_n,_r,_p)		gravity_callframe_t *cframe = gravity_new_callframe(vm, fiber);										\
									cframe->closure = _c;																				\
									cframe->stackstart = _n;																			\
									cframe->ip = _c->f->bytecode;																		\
									cframe->dest = _r;																					\
									cframe->nargs = _p;																					\
									cframe->outloop = false;																			\
									cframe->args = (_c->f->useargs) ? gravity_list_from_array(vm, _p-1, _n+1) : NULL;					\

#define SYNC_STACKTOP(_c,_n)		if (_c->f->tag != EXEC_TYPE_NATIVE) fiber->stacktop -= _n
#define SETFRAME_OUTLOOP(cframe)	(cframe)->outloop = true

#define COMPUTE_JUMP(value)			(func->bytecode + (value))

// FAST MATH MACROS
#define FMATH_BIN_INT(_r1,_v2,_v3,_OP)				do {SETVALUE(_r1, VALUE_FROM_INT(_v2 _OP _v3)); DISPATCH();} while(0)
#define FMATH_BIN_FLOAT(_r1,_v2,_v3,_OP)			do {SETVALUE(_r1, VALUE_FROM_FLOAT(_v2 _OP _v3)); DISPATCH();} while(0)
#define FMATH_BIN_BOOL(_r1,_v2,_v3,_OP)				do {SETVALUE(_r1, VALUE_FROM_BOOL(_v2 _OP _v3)); DISPATCH();} while(0)

#define DEFINE_STACK_VARIABLE(_v,_r)				register gravity_value_t _v = STACK_GET(_r)
#define DEFINE_INDEX_VARIABLE(_v,_r)				register gravity_value_t _v = (_r < MAX_REGISTERS) ? STACK_GET(_r) : VALUE_FROM_INT(_r-MAX_REGISTERS)

#define NO_CHECK
#define CHECK_ZERO(_v)								if ((VALUE_ISA_INT(_v) && (_v.n == 0)) || (VALUE_ISA_FLOAT(_v) && (_v.f == 0.0)))			\
													RUNTIME_ERROR("Division by 0 error.")

#define CHECK_FAST_BINARY_BOOL(r1,r2,r3,v2,v3,OP)	DEFINE_STACK_VARIABLE(v2,r2);																\
													DEFINE_STACK_VARIABLE(v3,r3);																\
													if (VALUE_ISA_BOOL(v2) && VALUE_ISA_BOOL(v3)) FMATH_BIN_BOOL(r1, v2.n, v3.n, OP)

#define CHECK_FAST_UNARY_BOOL(r1,r2,v2,OP)			DEFINE_STACK_VARIABLE(v2,r2);																\
													if (VALUE_ISA_BOOL(v2)) {SETVALUE(r1, VALUE_FROM_BOOL(OP v2.n)); DISPATCH();}

// fast math only for INT and FLOAT
#define CHECK_FAST_BINARY_MATH(r1,r2,r3,v2,v3,OP,_CHECK)																						\
													DEFINE_STACK_VARIABLE(v2,r2);																\
													DEFINE_STACK_VARIABLE(v3,r3);																\
													_CHECK;																						\
													if (VALUE_ISA_INT(v2)) {																	\
														if (VALUE_ISA_INT(v3)) FMATH_BIN_INT(r1, v2.n, v3.n, OP);								\
														if (VALUE_ISA_FLOAT(v3)) FMATH_BIN_FLOAT(r1, v2.n, v3.f, OP);							\
													} else if (VALUE_ISA_FLOAT(v2)) {															\
														if (VALUE_ISA_FLOAT(v3)) FMATH_BIN_FLOAT(r1, v2.f, v3.f, OP);							\
														if (VALUE_ISA_INT(v3)) FMATH_BIN_FLOAT(r1, v2.f, v3.n, OP);								\
													}

#define CHECK_FAST_UNARY_MATH(r1,r2,v2,OP)			DEFINE_STACK_VARIABLE(v2,r2);	\
													if (VALUE_ISA_INT(v2)) {SETVALUE(r1, VALUE_FROM_INT(OP v2.n)); DISPATCH();}	\
													if (VALUE_ISA_FLOAT(v2)) {SETVALUE(r1, VALUE_FROM_FLOAT(OP v2.f)); DISPATCH();}


#define CHECK_FAST_BINARY_REM(r1,r2,r3,v2,v3)		DEFINE_STACK_VARIABLE(v2,r2);																\
													DEFINE_STACK_VARIABLE(v3,r3);																\
													if (VALUE_ISA_INT(v2) && VALUE_ISA_INT(v3)) FMATH_BIN_INT(r1, v2.n, v3.n, %)

#define CHECK_FAST_BINARY_BIT(r1,r2,r3,v2,v3,OP)	DEFINE_STACK_VARIABLE(v2,r2);																\
													DEFINE_STACK_VARIABLE(v3,r3);																\
													if (VALUE_ISA_INT(v2) && VALUE_ISA_INT(v3)) FMATH_BIN_INT(r1, v2.n, v3.n, OP)

#define CHECK_FAST_BINBOOL_BIT(r1,v2,v3,OP)			if (VALUE_ISA_BOOL(v2) && VALUE_ISA_BOOL(v3)) FMATH_BIN_BOOL(r1, v2.n, v3.n, OP)

#define DECODE_BINARY_OPERATION(r1,r2,r3)			OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);	\
													DEBUG_VM("%s %d %d %d", opcode_name(op), r1, r2, r3)

#define PREPARE_FUNC_CALLN(_c,_i,_w,_N)				gravity_closure_t *_c = (gravity_closure_t *)gravity_class_lookup_closure(gravity_value_getclass(v2), cache[_i]); \
													if (!_c || !_c->f) RUNTIME_ERROR("Unable to perform operator %s on object", opcode_name(op));	\
													uint32_t _w = FN_COUNTREG(func, frame->nargs);													\
													uint32_t _rneed = FN_COUNTREG(_c->f, _N);														\
													gravity_check_stack(vm, fiber, _rneed, &stackstart)

#define PREPARE_FUNC_CALL1(_c,_v1,_i,_w)			PREPARE_FUNC_CALLN(_c,_i,_w,1);															\
													SETVALUE(_w, _v1)


#define PREPARE_FUNC_CALL2(_c,_v1,_v2,_i,_w)		PREPARE_FUNC_CALLN(_c,_i,_w,2);															\
													SETVALUE(_w, _v1);																		\
													SETVALUE(_w+1, _v2)

#define PREPARE_FUNC_CALL3(_c,_v1,_v2,_v3,_i,_w)	PREPARE_FUNC_CALLN(_c,_i,_w,3);															\
													SETVALUE(_w, _v1);																		\
													SETVALUE(_w+1, _v2);																	\
													SETVALUE(_w+2, _v3)


#define CALL_FUNC(_name,_c,r1,nargs,rwin)			STORE_FRAME();																			\
													execute_op_##_name:																		\
													switch(_c->f->tag) {																	\
													case EXEC_TYPE_NATIVE: {																\
														PUSH_FRAME(_c, &stackstart[rwin], r1, nargs);										\
													} break;																				\
													case EXEC_TYPE_INTERNAL: {																\
														if (!_c->f->internal(vm, &stackstart[rwin], nargs, r1)) {							\
															if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {											\
																closure = VALUE_AS_CLOSURE(STACK_GET(r1));									\
																SETVALUE(r1, VALUE_FROM_NULL);												\
																goto execute_op_##_name;													\
															}																				\
															fiber = vm->fiber;																\
															if (fiber == NULL) return true;													\
															if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);							\
														}																					\
													} break;																				\
													case EXEC_TYPE_BRIDGED:	{																\
														ASSERT(delegate->bridge_execute, "bridge_execute delegate callback is mandatory");	\
														if (!delegate->bridge_execute(vm, _c->f->xdata, &stackstart[rwin], nargs, r1)) {	\
															if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);							\
														}																					\
													} break;																				\
													case EXEC_TYPE_SPECIAL:																	\
														RUNTIME_ERROR("Unable to handle a special function in current context");			\
														break;																				\
													}																						\
													LOAD_FRAME();																			\
													SYNC_STACKTOP(_c, _rneed)

#endif
