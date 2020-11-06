//
//  gravity_optimizer.c
//  gravity
//
//  Created by Marco Bambini on 24/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

// Some optimizations taken from: http://www.compileroptimizations.com/

#include "gravity_hash.h"
#include "gravity_optimizer.h"
#include "gravity_opcodes.h"
#include "gravity_ircode.h"
#include "gravity_utils.h"
#include "gravity_value.h"

#define IS_MOVE(inst)               ((inst) && (inst->op == MOVE))
#define IS_RET(inst)                ((inst) && (inst->op == RET))
#define IS_NEG(inst)                ((inst) && (inst->op == NEG))
#define IS_NUM(inst)                ((inst) && (inst->op == LOADI))
#define IS_MATH(inst)               ((inst) && (inst->op >= ADD) && (inst->op <= REM))
#define IS_SKIP(inst)               (inst->tag == SKIP_TAG)
#define IS_LABEL(inst)              (inst->tag == LABEL_TAG)
#define IS_NOTNULL(inst)            (inst)
#define IS_PRAGMA_MOVE_OPT(inst)    ((inst) && (inst->tag == PRAGMA_MOVE_OPTIMIZATION))

// http://www.mathsisfun.com/binary-decimal-hexadecimal-converter.html
#define OPCODE_SET(op,code)                             op = (code & 0x3F) << 26
#define OPCODE_SET_TWO8bit_ONE10bit(op,code,a,b,c)      op = (code & 0x3F) << 26; op += (a & 0xFF) << 18; op += (b & 0xFF) << 10; op += (c & 0x3FF)
#define OPCODE_SET_FOUR8bit(op,a,b,c,d)                 op = (a & 0xFF) << 24; op += (b & 0xFF) << 16; op += (c & 0xFF) << 8; op += (d & 0xFF)
#define OPCODE_SET_ONE8bit_SIGN_ONE17bit(op,code,a,s,n) op = (code & 0x3F) << 26; op += (a & 0xFF) << 18; op += (s & 0x01) << 17; op += (n & 0x1FFFF)
#define OPCODE_SET_SIGN_ONE25bit(op,code,s,a)           op = (code & 0x3F) << 26; op += (s & 0x01) << 25; op += (a & 0x1FFFFFF)
#define OPCODE_SET_ONE8bit_ONE18bit(op,code,a,n)        op = (code & 0x3F) << 26; op += (a & 0xFF) << 18; op += (n & 0x3FFFF)
#define OPCODE_SET_ONE26bit(op,code,a)                  op = (code & 0x3F) << 26; op += (a & 0x3FFFFFF)
#define OPCODE_SET_THREE8bit(op,code,a,b,c)             OPCODE_SET_TWO8bit_ONE10bit(op,code,a,b,c)
#define OPCODE_SET_ONE8bit_ONE10bit(op,code,a,b)        OPCODE_SET_TWO8bit_ONE10bit(op,code,a,0,b)
#define OPCODE_SET_ONE8bit(op,code,a)                   OPCODE_SET_TWO8bit_ONE10bit(op,code,a,0,0)
#define OPCODE_SET_THREE8bit_ONE2bit(op,code,a,b,c,f)   op =(code & 0x3F)<<26; op+=(a & 0xFF)<<18; op+=(b & 0xFF)<<10; op+=(c & 0xFF)<<2; op+=(f & 0x03)

// MARK: -

static bool hash_isequal (gravity_value_t v1, gravity_value_t v2) {
    return (v1.n == v2.n);
}

static uint32_t hash_compute (gravity_value_t v) {
    return gravity_hash_compute_int(v.n);
}

static void finalize_function (gravity_function_t *f, bool add_debug) {
    ircode_t        *code = (ircode_t *)f->bytecode;
    uint32_t        ninst = 0, count = ircode_count(code);
    uint32_t        notpure = 0;
    uint32_t        *bytecode = NULL;
    uint32_t        *lineno = NULL;
    gravity_hash_t    *labels = gravity_hash_create(0, hash_compute, hash_isequal, NULL, NULL);

    // determine how big bytecode buffer must be
    // and collect all LABEL instructions
    for (uint32_t i=0; i<count; ++i) {
        inst_t *inst = ircode_get(code, i);
        if (IS_SKIP(inst)) continue;
        if (IS_PRAGMA_MOVE_OPT(inst)) continue;
        if (IS_LABEL(inst)) {
            // insert key inst->p1 into hash table labels with value ninst (next instruction)
            gravity_hash_insert(labels, VALUE_FROM_INT(inst->p1), VALUE_FROM_INT(ninst));
            continue;
        }
        ++ninst;
    }

    // +1 is just a trick so the VM switch loop terminates with an implicit RET0 instruction (RET0 has opcode 0)
    f->ninsts = ninst;
    bytecode = (uint32_t *)mem_alloc(NULL, (ninst+1) * sizeof(uint32_t));
    if (add_debug) lineno = (uint32_t *)mem_alloc(NULL, (ninst+1) * sizeof(uint32_t));
    assert(bytecode);

    uint32_t j=0;
    for (uint32_t i=0; i<count; ++i) {
        inst_t *inst = ircode_get(code, i);
        if (IS_SKIP(inst)) continue;
        if (IS_LABEL(inst)) continue;
        if (IS_PRAGMA_MOVE_OPT(inst)) continue;

        uint32_t op = 0x0;
        switch (inst->op) {
            case HALT:
            case RET0:
            case NOP:
                OPCODE_SET(op, inst->op);
                break;

            case LOAD:
            case STORE:
                ++notpure;    // not sure here
            case LOADS:
            case LOADAT:
            case STOREAT:
            case EQQ:
            case NEQQ:
            case ISA:
            case MATCH:
            case LSHIFT:
            case RSHIFT:
            case BOR:
            case BAND:
            case BNOT:
            case BXOR:
            case ADD:
            case SUB:
            case DIV:
            case MUL:
            case REM:
            case AND:
            case OR:
            case LT:
            case GT:
            case EQ:
            case LEQ:
            case GEQ:
            case NEQ:
            case NEG:
            case NOT:
                OPCODE_SET_TWO8bit_ONE10bit(op, inst->op, inst->p1, inst->p2, inst->p3);
                break;

            case LOADI:
                OPCODE_SET_ONE8bit_SIGN_ONE17bit(op, inst->op, inst->p1, (inst->n < 0) ? 1 : 0, inst->n);
                break;

            case JUMPF: {
                gravity_value_t *v = gravity_hash_lookup(labels, VALUE_FROM_INT(inst->p2));
                assert(v); // key MUST exists!
                uint32_t njump = (uint32_t)v->n;
                uint32_t bflag = inst->p3;
                OPCODE_SET_ONE8bit_SIGN_ONE17bit(op, inst->op, inst->p1, bflag, njump);
                //OPCODE_SET_ONE8bit_ONE18bit(op, inst->op, inst->p1, njump);
                break;
            }

            case RET:
                OPCODE_SET_ONE8bit(op, inst->op, inst->p1);
                break;

            case JUMP: {
                gravity_value_t *v = gravity_hash_lookup(labels, VALUE_FROM_INT(inst->p1));
                assert(v); // key MUST exists!
                uint32_t njump = (uint32_t)v->n;
                OPCODE_SET_ONE26bit(op, inst->op, njump);
                break;
            }

            case LOADG:
            case STOREG:
                ++notpure;
            case MOVE:
            case LOADK:
                OPCODE_SET_ONE8bit_ONE18bit(op, inst->op, inst->p1, inst->p2);
                break;

            case CALL:
                OPCODE_SET_TWO8bit_ONE10bit(op, inst->op, inst->p1, inst->p2, inst->p3);
                break;

            case SETLIST:
                OPCODE_SET_TWO8bit_ONE10bit(op, inst->op, inst->p1, inst->p2, inst->p3);
                break;

            case LOADU:
            case STOREU:
                ++notpure;
                OPCODE_SET_ONE8bit_ONE18bit(op, inst->op, inst->p1, inst->p2);
                break;

            case RANGENEW: {
                uint8_t flag = (inst->tag == RANGE_INCLUDE_TAG) ? 0 : 1;
                OPCODE_SET_THREE8bit_ONE2bit(op, inst->op, inst->p1, inst->p2, inst->p3, flag);
                break;
            }
            case MAPNEW:
            case LISTNEW:
                OPCODE_SET_ONE8bit_ONE18bit(op, inst->op, inst->p1, inst->p2);
                break;

            case SWITCH:
                assert(0);
                break;

            case CLOSURE:
            case CLOSE:
                OPCODE_SET_ONE8bit_ONE18bit(op, inst->op, inst->p1, inst->p2);
                break;

            case CHECK:
                OPCODE_SET_ONE8bit_ONE18bit(op, inst->op, inst->p1, inst->p2);
                break;
                
            case RESERVED2:
            case RESERVED3:
            case RESERVED4:
            case RESERVED5:
            case RESERVED6:
                assert(0);
                break;
        }

        // add debug information
        if (add_debug) lineno[j] = inst->lineno;
        
        // store encoded instruction
        bytecode[j++] = op;
    }

    ircode_free(code);
    gravity_hash_free(labels);

    f->bytecode = bytecode;
    f->lineno = lineno;
    f->purity = (notpure == 0) ? 1.0f : ((float)(notpure * 100) / (float)ninst) / 100.0f;
}

// MARK: -

inline static bool pop1_instruction (ircode_t *code, uint32_t index, inst_t **inst1) {
    *inst1 = NULL;

    for (int32_t i=index-1; i>=0; --i) {
        inst_t *inst = ircode_get(code, i);
        if ((inst != NULL) && (inst->tag != SKIP_TAG)) {
            *inst1 = inst;
            return true;
        }
    }

    return false;
}

inline static bool pop2_instructions (ircode_t *code, uint32_t index, inst_t **inst1, inst_t **inst2) {
    *inst1 = NULL;
    *inst2 = NULL;

    for (int32_t i=index-1; i>=0; --i) {
        inst_t *inst = ircode_get(code, i);
        if ((inst != NULL) && (inst->tag != SKIP_TAG)) {
            if (*inst1 == NULL) *inst1 = inst;
            else if (*inst2 == NULL) {
                *inst2 = inst;
                return true;
            }
        }
    }

    return false;
}

inline static inst_t *current_instruction (ircode_t *code, uint32_t i) {
    while (1) {
        inst_t *inst = ircode_get(code, i);
        if (inst == NULL) return NULL;
        if (inst->tag != SKIP_TAG) return inst;
        ++i;
    }

    return NULL;
}

// MARK: -

static bool optimize_const_instruction (inst_t *inst, inst_t *inst1, inst_t *inst2) {
    if (!inst2) return false;
    
    // select type algorithm:
    // two numeric types are supported here, int64 or double
    // if types are equals then set the first one
    // if types are not equals then set to double
    optag_t type;
    double  d = 0.0, d1 = 0.0, d2 = 0.0;
    int64_t n = 0, n1 = 0, n2 = 0;

    // compute types
    if (inst1->tag == inst2->tag) type = inst1->tag;
    else type = DOUBLE_TAG;

    // check registers
    // code like:
    //      var i = 13;
    //      return 20 + i*100;
    // produces the following bytecode
    //      00000    LOADI 2 13
    //      00001    MOVE 1 2
    //      00002    LOADI 2 20
    //      00003    LOADI 3 100
    //      00004    MUL 3 1 3
    //      00005    ADD 2 2 3
    // inst points to a MATH instruction but registers are not the same as the LOADI instructions
    // so no optimizations must be performed
    if (!(inst->p2 == inst1->p1 && inst->p3 == inst2->p2)) return false;
    
    // compute operands
    if (type == DOUBLE_TAG) {
        d1 = (inst1->tag == INT_TAG) ? (double)inst1->n : inst1->d;
        d2 = (inst2->tag == INT_TAG) ? (double)inst2->n : inst2->d;
    } else {
        n1 = (inst1->tag == INT_TAG) ? inst1->n : (int64_t)inst1->d;
        n2 = (inst2->tag == INT_TAG) ? inst2->n : (int64_t)inst2->d;
    }

    // perform operation
    switch (inst->op) {
        case ADD:
            if (type == DOUBLE_TAG) d = d1 + d2;
            else n = n1 + n2;
            break;

        case SUB:
            if (type == DOUBLE_TAG) d = d1 - d2;
            else n = n1 - n2;
            break;

        case MUL:
            if (type == DOUBLE_TAG) d = d1 * d2;
            else n = n1 * n2;
            break;

        case DIV:
            // don't optimize in case of division by 0
            if ((int64_t)d2 == 0) return false;
            if (type == DOUBLE_TAG) d = d1 / d2;
            else n = n1 / n2;
            break;

        case REM:
            if ((int64_t)d2 == 0) return false;
            if (type == DOUBLE_TAG) d = (double)((int64_t)d1 % (int64_t)d2);
            else n = n1 % n2;
            break;

        default:
            assert(0);
    }

    // adjust IRCODE
    inst_setskip(inst1);
    inst_setskip(inst2);

    // convert an ADD instruction to a LOADI instruction
    // ADD A B C    => R(A) = R(B) + R(C)
    // LOADI A B    => R(A) = N
    inst->op = LOADI;
    inst->tag = type;
    inst->p2 = inst->p3 = 0;
    if (type == DOUBLE_TAG) inst->d = d;
    else inst->n = n;

    return true;
}

static bool optimize_neg_instruction (ircode_t *code, inst_t *inst, uint32_t i) {
    inst_t *inst1 = NULL;
    pop1_instruction(code, i, &inst1);
    if ((inst1 == NULL) || (inst1->op != LOADI) || (inst1->p1 != inst->p2)) return false;
    if (!ircode_register_istemp(code, inst1->p1)) return false;

    if (inst1->tag == INT_TAG) {
        int64_t n = inst1->n;
        if (n > 131072) return false;
        inst1->p1 = inst->p2;
        inst1->n = -(int64_t)n;
    } else if (inst1->tag == DOUBLE_TAG) {
        double d = inst1->d;
        inst1->p1 = inst->p2;
        inst1->d = -d;
    } else {
        return false;
    }
	
    inst_setskip(inst);
    return true;
}

static bool optimize_math_instruction (ircode_t *code, inst_t *inst, uint32_t i) {
    uint8_t count = opcode_numop(inst->op) - 1;
    inst_t  *inst1 = NULL, *inst2 = NULL;
    bool    flag = false;

    if (count == 2) {
        pop2_instructions(code, i, &inst2, &inst1);
        if (IS_NUM(inst1) && IS_NUM(inst2)) flag = optimize_const_instruction(inst, inst1, inst2);

        // process inst2
        if (IS_MOVE(inst2)) {
            bool b1 = ircode_register_istemp(code, inst->p3);
            bool b2 = ((inst2) && ircode_register_istemp(code, inst2->p1));
            if ((b1) && (b2) && (inst->p3 == inst2->p1)) {
                inst->p3 = inst2->p2;
                inst_setskip(inst2);
                flag = true;
            }
        }

        // process inst1
        if (IS_MOVE(inst1)) {
            bool b1 = ircode_register_istemp(code, inst->p2);
            bool b2 = ((inst1) && ircode_register_istemp(code, inst1->p1));
            if ((b1) && (b2) && (inst->p2 == inst1->p1)) {
                inst->p2 = inst1->p2;
                inst_setskip(inst1);
                flag = true;
            }
        }

    }
    else {
        pop1_instruction(code, i, &inst1);
        if (IS_NUM(inst1)) flag = optimize_const_instruction(inst, inst1, NULL);
    }

    return flag;
}

static bool optimize_move_instruction (ircode_t *code, inst_t *inst, uint32_t i) {
    inst_t *inst1 = NULL;
    pop1_instruction(code, i, &inst1);
    if (inst1 == NULL) return false;
    if ((inst1->op != LOADI) && (inst1->op != LOADG) && (inst1->op != LOADK)) return false;

    bool b1 = ircode_register_istemp(code, inst->p2);
    bool b2 = ((inst1) && ircode_register_istemp(code, inst1->p1));

    if ((b1) && (b2) && (inst->p2 == inst1->p1)) {
        inst1->p1 = inst->p1;
        inst_setskip(inst);
        return true;
    }

    return false;
}

static bool optimize_return_instruction (ircode_t *code, inst_t *inst, uint32_t i) {
    inst_t *inst1 = NULL;
    pop1_instruction(code, i, &inst1);

    if (!ircode_register_istemp(code, inst->p1)) return false;
    if ((IS_MOVE(inst1)) && (inst->p1 == inst1->p1)) {
        inst->p1 = inst1->p2;
        inst_setskip(inst1);
        return true;
    }

    return false;
}

static bool optimize_num_instruction (inst_t *inst, gravity_function_t *f) {

    // double values always added to constant pool
    bool add_cpool = (inst->tag == DOUBLE_TAG);

    // LOADI is a 32bit instruction
    // 32 - 6 (OPCODE) - 8 (register) - 1 bit sign = 17
    // range is from MAX_INLINE_INT-1 to MAX_INLINE_INT
    // so max/min values are in the range -(2^17)-1/+2^17
    // 2^17 = 131072 (MAX_INLINE_INT)
    if (inst->tag == INT_TAG) {
        int64_t n = inst->n;
        add_cpool = ((n < -MAX_INLINE_INT + 1) || (n > MAX_INLINE_INT));
    }

    if (add_cpool) {
        uint16_t index = 0;
        if (inst->tag == INT_TAG) {
            int64_t n = inst->n;
            index = gravity_function_cpool_add(NULL, f, VALUE_FROM_INT(n));
        } else {
            // always add floating point values as double in constant pool (then VM will be configured to interpret it as float or double)
            index = gravity_function_cpool_add(NULL, f, VALUE_FROM_FLOAT(inst->d));
        }

        // replace LOADI with a LOADK instruction
        inst->op = LOADK;
        inst->p2 = index;
        inst->tag = NO_TAG;
    }

    return true;
}

// MARK: -

gravity_function_t *gravity_optimizer(gravity_function_t *f, bool add_debug) {
    if (f->bytecode == NULL) return f;

    ircode_t    *code = (ircode_t *)f->bytecode;
    uint32_t    count = ircode_count(code);
    bool        optimizer = true;

    f->ntemps = ircode_ntemps(code);

    loop_neg:
    for (uint32_t i=0; i<count; ++i) {
        inst_t *inst = current_instruction(code, i);
        if (IS_NEG(inst)) {
            bool b = optimize_neg_instruction (code, inst, i);
            if (b) goto loop_neg;
        }
    }

    loop_math:
    for (uint32_t i=0; i<count; ++i) {
        inst_t *inst = current_instruction(code, i);
        if (IS_MATH(inst)) {
            bool b = optimize_math_instruction (code, inst, i);
            if (b) goto loop_math;
        }
    }

    loop_move:
    optimizer = true;
    for (uint32_t i=0; i<count; ++i) {
        inst_t *inst = current_instruction(code, i);
        if (IS_PRAGMA_MOVE_OPT(inst)) optimizer = (bool)inst->p1;
        if (optimizer && IS_MOVE(inst)) {
            bool b = optimize_move_instruction (code, inst, i);
            if (b) goto loop_move;
        }
    }

    loop_ret:
    for (uint32_t i=0; i<count; ++i) {
        inst_t *inst = current_instruction(code, i);
        if (IS_RET(inst)) {
            bool b = optimize_return_instruction (code, inst, i);
            if (b) goto loop_ret;
        }
    }

    for (uint32_t i=0; i<count; ++i) {
        inst_t *inst = current_instruction(code, i);
        if (IS_NUM(inst)) optimize_num_instruction (inst, f);
    }

    // dump optimized version
    #if GRAVITY_BYTECODE_DEBUG
    gravity_function_dump(f, ircode_dump);
    #endif

    // finalize function
	finalize_function(f, add_debug);

    return f;
}
