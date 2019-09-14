//
//  gravity_ircode.h
//  gravity
//
//  Created by Marco Bambini on 06/11/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_IRCODE__
#define __GRAVITY_IRCODE__

// References:
// https://www.usenix.org/legacy/events/vee05/full_papers/p153-yunhe.pdf
// http://www.lua.org/doc/jucs05.pdf
//
// In a stack-based VM, a local variable is accessed using an index, and the operand stack is accessed via the stack pointer.
// In a register-based VM both the local variables and operand stack can be considered as virtual registers for the method.
// There is a simple mapping from stack locations to register numbers, because the height and contents of the VM operand stack
// are known at any point in a program.
//
// All values on the operand stack can be considered as temporary variables (registers) for a method and therefore are short-lived.
// Their scope of life is between the instructions that push them onto the operand stack and the instruction that consumes
// the value on the operand stack. On the other hand, local variables (also registers) are long-lived and their life scope is
// the time of method execution.

#include "debug_macros.h"
#include "gravity_opcodes.h"
#include "gravity_array.h"

#define REGISTER_ERROR    UINT32_MAX

typedef enum {
        NO_TAG = 0,
        INT_TAG,
        DOUBLE_TAG,
        LABEL_TAG,
        SKIP_TAG,
        RANGE_INCLUDE_TAG,
        RANGE_EXCLUDE_TAG,
        PRAGMA_MOVE_OPTIMIZATION
} optag_t;

typedef struct {
    opcode_t    op;
    optag_t     tag;
    int32_t     p1;
    int32_t     p2;
    int32_t     p3;
    union {
        double      d;  //    tag is DOUBLE_TAG
        int64_t     n;  //    tag is INT_TAG
    };
    uint32_t    lineno;     //  debug info
} inst_t;

typedef struct ircode_t ircode_t;

ircode_t    *ircode_create (uint16_t nlocals);
void        ircode_free (ircode_t *code);
uint32_t    ircode_count (ircode_t *code);
uint32_t    ircode_ntemps (ircode_t *code);
inst_t      *ircode_get (ircode_t *code, uint32_t index);
void        ircode_dump (void *code);
void        ircode_push_context (ircode_t *code);
void        ircode_pop_context (ircode_t *code);
bool        ircode_iserror (ircode_t *code);
void        ircode_patch_init (ircode_t *code, uint16_t index);

uint32_t    ircode_newlabel (ircode_t *code);
void        ircode_setlabel_true (ircode_t *code, uint32_t nlabel);
void        ircode_setlabel_false (ircode_t *code, uint32_t nlabel);
void        ircode_setlabel_check (ircode_t *code, uint32_t nlabel);
void        ircode_unsetlabel_true (ircode_t *code);
void        ircode_unsetlabel_false (ircode_t *code);
void        ircode_unsetlabel_check (ircode_t *code);
uint32_t    ircode_getlabel_true (ircode_t *code);
uint32_t    ircode_getlabel_false (ircode_t *code);
uint32_t    ircode_getlabel_check (ircode_t *code);
void		ircode_marklabel (ircode_t *code, uint32_t nlabel, uint32_t lineno);

void        inst_setskip (inst_t *inst);
uint8_t     opcode_numop (opcode_t op);

void		ircode_pragma (ircode_t *code, optag_t tag, uint32_t value, uint32_t lineno);
void		ircode_add (ircode_t *code, opcode_t op, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t lineno);
void		ircode_add_tag (ircode_t *code, opcode_t op, uint32_t p1, uint32_t p2, uint32_t p3, optag_t tag, uint32_t lineno);
void		ircode_add_array (ircode_t *code, opcode_t op, uint32_t p1, uint32_t p2, uint32_t p3, uint32_r r, uint32_t lineno);
void		ircode_add_double (ircode_t *code, double d, uint32_t lineno);
void		ircode_add_int (ircode_t *code, int64_t n, uint32_t lineno);
void		ircode_add_constant (ircode_t *code, uint32_t index, uint32_t lineno);
void		ircode_add_skip (ircode_t *code, uint32_t lineno);
void        ircode_set_index (uint32_t index, ircode_t *code, opcode_t op, uint32_t p1, uint32_t p2, uint32_t p3);
void        ircode_add_check (ircode_t *code);

// IMPORTANT NOTE
//
// The following functions can return REGISTER_ERROR and so an error check is mandatory
// ircode_register_pop
// ircode_register_pop_context_protect
// ircode_register_last
//
// The following functions can return 0 if no temp registers are available
// ircode_register_push_temp
//

bool        ircode_register_istemp (ircode_t *code, uint32_t n);
uint32_t    ircode_register_push_temp (ircode_t *code);
uint32_t    ircode_register_push_temp_protected (ircode_t *code);
uint32_t    ircode_register_push (ircode_t *code, uint32_t nreg);
uint32_t    ircode_register_pop (ircode_t *code);
uint32_t    ircode_register_first_temp_available (ircode_t *code);
uint32_t    ircode_register_pop_context_protect (ircode_t *code, bool protect);
bool        ircode_register_protect_outside_context (ircode_t *code, uint32_t nreg);
void        ircode_register_protect_in_context (ircode_t *code, uint32_t nreg);
uint32_t    ircode_register_last (ircode_t *code);
uint32_t    ircode_register_count (ircode_t *code);
void        ircode_register_clear (ircode_t *code, uint32_t nreg);
void        ircode_register_set (ircode_t *code, uint32_t nreg);
void        ircode_register_dump (ircode_t *code);

void        ircode_register_temp_protect (ircode_t *code, uint32_t nreg);
void        ircode_register_temp_unprotect (ircode_t *code, uint32_t nreg);
void        ircode_register_temps_clear (ircode_t *code);

#endif
