//
//  gravity_core.c
//  gravity
//
//  Created by Marco Bambini on 10/01/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#include <inttypes.h>
#include <math.h>
#include <time.h>
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_value.h"
#include "gravity_debug.h"
#include "gravity_opcodes.h"
#include "gravity_macros.h"
#include "gravity_memory.h"
#include "date_class.h"


static bool date_inited = false;		// initialize global classes just once
static uint32_t refcount = 0;			// protect deallocation of global classes

gravity_class_t *gravity_class_date;

#define SETMETA_INITED(c)						gravity_class_get_meta(c)->is_inited = true
#define GET_VALUE(_idx)							args[_idx]
#define RETURN_VALUE(_v,_i)						do {gravity_vm_setslot(vm, _v, _i); return true;} while(0)
#define RETURN_CLOSURE(_v,_i)					do {gravity_vm_setslot(vm, _v, _i); return false;} while(0)
#define RETURN_FIBER()							return false
#define RETURN_NOVALUE()						return true
#define RETURN_ERROR(...)						do {																		\
													char buffer[4096];														\
													snprintf(buffer, sizeof(buffer), __VA_ARGS__);							\
													gravity_fiber_seterror(gravity_vm_fiber(vm), (const char *) buffer);	\
													gravity_vm_setslot(vm, VALUE_FROM_NULL, rindex);						\
													return false;															\
												} while(0)

#define DECLARE_1VARIABLE(_v,_idx)				register gravity_value_t _v = GET_VALUE(_idx)
#define DECLARE_2VARIABLES(_v1,_v2,_idx1,_idx2) DECLARE_1VARIABLE(_v1,_idx1);DECLARE_1VARIABLE(_v2,_idx2)

#define CHECK_VALID(_v, _msg)					if (VALUE_ISA_NOTVALID(_v)) RETURN_ERROR(_msg)
#define INTERNAL_CONVERT_FLOAT(_v)				_v = convert_value2float(vm,_v); CHECK_VALID(_v, "Unable to convert object to Float")
#define INTERNAL_CONVERT_BOOL(_v)				_v = convert_value2bool(vm,_v); CHECK_VALID(_v, "Unable to convert object to Bool")
#define INTERNAL_CONVERT_INT(_v)				_v = convert_value2int(vm,_v); CHECK_VALID(_v, "Unable to convert object to Int")
#define INTERNAL_CONVERT_STRING(_v)				_v = convert_value2string(vm,_v); CHECK_VALID(_v, "Unable to convert object to String")

#define NEW_FUNCTION(_fptr)						(gravity_function_new_internal(NULL, NULL, _fptr, 0))
#define NEW_CLOSURE_VALUE(_fptr)				((gravity_value_t){	.isa = gravity_class_closure,		\
																	.p = (gravity_object_t *)gravity_closure_new(NULL, NEW_FUNCTION(_fptr))})

#define FUNCTION_ISA_SPECIAL(_f)				(OBJECT_ISA_FUNCTION(_f) && (_f->tag == EXEC_TYPE_SPECIAL))
#define FUNCTION_ISA_DEFAULT_GETTER(_f)			((_f->index < GRAVITY_COMPUTED_INDEX) && (_f->special[EXEC_TYPE_SPECIAL_GETTER] == NULL))
#define FUNCTION_ISA_DEFAULT_SETTER(_f)			((_f->index < GRAVITY_COMPUTED_INDEX) && (_f->special[EXEC_TYPE_SPECIAL_SETTER] == NULL))
#define FUNCTION_ISA_GETTER(_f)					(_f->special[EXEC_TYPE_SPECIAL_GETTER] != NULL)
#define FUNCTION_ISA_SETTER(_f)					(_f->special[EXEC_TYPE_SPECIAL_SETTER] != NULL)
#define FUNCTION_ISA_BRIDGED(_f)				(_f->index == GRAVITY_BRIDGE_INDEX)

// MARK: - Date -

static bool date_time (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	int t = (int)time(NULL);
	RETURN_VALUE(VALUE_FROM_INT(t), rindex);
}

static bool date_second (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	int tm = localtime(&t)->tm_sec;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_minute (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	int tm = localtime(&t)->tm_min;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_hour (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	int tm = localtime(&t)->tm_hour;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_month_day (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	int tm = localtime(&t)->tm_mday;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_month (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	// Plus 1 because tm_mon is a base 0 number
	int tm = localtime(&t)->tm_mon + 1;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_year (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	// Plus 1900 because tm_year is the number of years since 1900
	int tm = localtime(&t)->tm_year + 1900;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_week_day (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	// Plus 1 because tm_wday is a base 0 number
	int tm = localtime(&t)->tm_wday + 1;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_year_day (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	// Plus 1 because tm_yday is a base 0 number
	int tm = localtime(&t)->tm_yday + 1;
	RETURN_VALUE(VALUE_FROM_INT(tm), rindex);
}

static bool date_daylight_savings (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	time_t t = time(NULL);
	int tm = localtime(&t)->tm_isdst;
	RETURN_VALUE(VALUE_FROM_BOOL(tm), rindex);
}


static void gravity_date_init (void) {
	// this function must be executed ONCE
	if (date_inited) return;
	date_inited = true;
	
	mem_check(false);
	
	// DATE class
	gravity_class_date = gravity_class_new_pair(NULL, GRAVITY_CLASS_DATE_NAME, NULL, 0, 0);
	gravity_class_t *date_meta = gravity_class_get_meta(gravity_class_date);
	gravity_class_bind(date_meta, GRAVITY_DATE_TIME_NAME, NEW_CLOSURE_VALUE(date_time));
	gravity_class_bind(date_meta, GRAVITY_DATE_SECOND_NAME, NEW_CLOSURE_VALUE(date_second));
	gravity_class_bind(date_meta, GRAVITY_DATE_MINUTE_NAME, NEW_CLOSURE_VALUE(date_minute));
	gravity_class_bind(date_meta, GRAVITY_DATE_HOUR_NAME, NEW_CLOSURE_VALUE(date_hour));
	gravity_class_bind(date_meta, GRAVITY_DATE_MONTH_DAY_NAME, NEW_CLOSURE_VALUE(date_month_day));
	gravity_class_bind(date_meta, GRAVITY_DATE_MONTH_NAME, NEW_CLOSURE_VALUE(date_month));
	gravity_class_bind(date_meta, GRAVITY_DATE_YEAR_NAME, NEW_CLOSURE_VALUE(date_year));
	gravity_class_bind(date_meta, GRAVITY_DATE_WEEK_DAY_NAME, NEW_CLOSURE_VALUE(date_week_day));
	gravity_class_bind(date_meta, GRAVITY_DATE_YEAR_DAY_NAME, NEW_CLOSURE_VALUE(date_year_day));
	gravity_class_bind(date_meta, GRAVITY_DATE_DAYLIGHT_SAVINGS_NAME, NEW_CLOSURE_VALUE(date_daylight_savings));
	
	// INIT META
	SETMETA_INITED(gravity_class_date);

	mem_check(true);
}

void gravity_date_free (void) {
	if (!date_inited) return;
	
	// check if others VM are still running
	if (--refcount != 0) return;
	
	// this function should never be called
	// it is just called when we need to internally check for memory leaks
	
	mem_check(false);
	
	// before freeing the meta class we need to remove entries with duplicated functions
	gravity_class_t *date_meta = gravity_class_get_meta(gravity_class_date);
	gravity_class_free_core(NULL, date_meta);
	gravity_class_free_core(NULL, gravity_class_date);
	
	gravity_class_date = NULL;

	core_inited = false;
}

void gravity_date_register (gravity_vm *vm) {
	gravity_date_init();
	++refcount;
	if (!vm) return;
	
	// register core classes inside VM
	if (gravity_vm_ismini(vm)) return;
	gravity_vm_setvalue(vm, GRAVITY_CLASS_DATE_NAME, VALUE_FROM_OBJECT(gravity_class_date));
}

