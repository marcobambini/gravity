//
//  gravity_debug.c
//  gravity
//
//  Created by Marco Bambini on 01/04/16.
//  Copyright (c) 2016 CreoLabs. All rights reserved.
//

#include <assert.h>
#include "gravity_value.h"
#include "gravity_vmmacros.h"
#include "gravity_debug.h"

const char *opcode_constname (int n) {
	switch (n) {
		case CPOOL_VALUE_SUPER: return "SUPER";
		case CPOOL_VALUE_NULL: return "NULL";
		case CPOOL_VALUE_UNDEFINED: return "UNDEFINED";
		case CPOOL_VALUE_ARGUMENTS: return "ARGUMENTS";
		case CPOOL_VALUE_TRUE: return "TRUE";
		case CPOOL_VALUE_FALSE: return "FALSE";
		case CPOOL_VALUE_FUNC: return "FUNC";
	}
	
	assert(0);
	return "N/A";
}

const char *opcode_name (opcode_t op) {
	static const char *optable[] = {
		"RET0", "HALT", "NOP", "RET", "CALL", "LOAD", "LOADS", "LOADAT",
		"LOADK", "LOADG", "LOADI", "LOADU", "MOVE", "STORE", "STOREAT",
		"STOREG", "STOREU", "JUMP", "JUMPF", "SWITCH", "ADD", "SUB", "DIV",
		"MUL", "REM", "AND", "OR", "LT", "GT", "EQ", "LEQ", "GEQ", "NEQ",
		"EQQ", "NEQQ", "IS", "MATCH", "NEG", "NOT", "LSHIFT", "RSHIFT", "BAND",
		"BOR", "BXOR", "BNOT", "MAPNEW", "LISTNEW", "RANGENEW", "SETLIST",
		"CLOSURE", "CLOSE", "RESERVED1", "RESERVED2", "RESERVED3", "RESERVED4",
		"RESERVED5", "RESERVED6"};
	return optable[op];
}

#define DUMP_VM(buffer, bindex, ...)					bindex += snprintf(&buffer[bindex], balloc-bindex, "%06u\t", pc);	\
														bindex += snprintf(&buffer[bindex], balloc-bindex, __VA_ARGS__);		\
														bindex += snprintf(&buffer[bindex], balloc-bindex, "\n");

#define DUMP_VM_NOCR(buffer, bindex, ...)				bindex += snprintf(&buffer[bindex], balloc-bindex, "%06u\t", pc);	\
														bindex += snprintf(&buffer[bindex], balloc-bindex, __VA_ARGS__);

#define DUMP_VM_RAW(buffer, bindex, ...)				bindex += snprintf(&buffer[bindex], balloc-bindex, __VA_ARGS__);

const char *gravity_disassemble (const char *bcode, uint32_t blen, bool deserialize) {
	uint32_t	*ip = NULL;
	uint32_t	pc = 0, inst = 0, ninsts = 0;
	opcode_t	op;
	
	const int	rowlen = 256;
	uint32_t	bindex = 0;
	uint32_t	balloc = 0;
	char		*buffer = NULL;
	
	if (deserialize) {
		// decode textual buffer to real bytecode
		ip = gravity_bytecode_deserialize(bcode, blen, &ninsts);
		if ((ip == NULL) || (ninsts == 0)) goto abort_disassemble;
	} else {
		ip = (uint32_t *)bcode;
		ninsts = blen;
	}
		
	// allocate a buffer big enought to fit all disassembled bytecode
	// I assume that each instruction (each row) will be 256 chars long
	balloc = ninsts * rowlen;
	buffer = mem_alloc(balloc);
	if (!buffer) goto abort_disassemble;
		
	// conversion loop
	while (pc < ninsts) {
		inst = *ip++;
		op = (opcode_t)OPCODE_GET_OPCODE(inst);
		
		switch (op) {
			case NOP: {
				DUMP_VM(buffer, bindex, "NOP");
				break;
			}
				
			case MOVE: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
				DUMP_VM(buffer, bindex, "MOVE %d %d", r1, r2);
				break;
			}
				
			case LOADI: {
				//OPCODE_GET_ONE8bit_ONE18bit(inst, uint32_t r1, int32_t value); if no support for signed int
				OPCODE_GET_ONE8bit_SIGN_ONE17bit(inst, const uint32_t r1, const int32_t value);
				DUMP_VM(buffer, bindex, "LOADI %d %d", r1, value);
				break;
			}
				
			case LOADK: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t index);
				if (index < CPOOL_INDEX_MAX) {
					DUMP_VM(buffer, bindex, "LOADK %d %d", r1, index);
				} else {
					const char *special;
					switch (index) {
						case CPOOL_VALUE_SUPER: special = "SUPER"; break;
						case CPOOL_VALUE_NULL: special = "NULL"; break;
						case CPOOL_VALUE_UNDEFINED: special = "UNDEFINED"; break;
						case CPOOL_VALUE_ARGUMENTS: special = "ARGUMENTS"; break;
						case CPOOL_VALUE_TRUE: special = "TRUE"; break;
						case CPOOL_VALUE_FALSE: special = "FALSE"; break;
						case CPOOL_VALUE_FUNC: special = "_FUNC"; break;
						default: ASSERT(0, "Invalid index in LOADK opcode"); break;
					}
					DUMP_VM(buffer, bindex, "LOADK %d %s", r1, special);
				}
				break;
			}
				
			case LOADG: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, uint32_t r1, int32_t index);
				DUMP_VM(buffer, bindex, "LOADG %d %d", r1, index);
				break;
			}
			
			case LOAD:
			case LOADS:
			case LOADAT: {
				OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
				DUMP_VM(buffer, bindex, "%s %d %d %d", (op == LOAD) ? "LOAD" : "LOADAT", r1, r2, r3);
				break;
			}
				
			case LOADU: {
				ASSERT(0, "To be implemented");
				break;
			}
				
			case STOREG: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, uint32_t r1, int32_t index);
				DUMP_VM(buffer, bindex, "STOREG %d %d", r1, index);
				break;
			}
				
			case STOREU: {
				ASSERT(0, "To be implemented");
				break;
			}
				
			case STORE:
			case STOREAT: {
				OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
				DUMP_VM(buffer, bindex, "%s %d %d %d", (op == STORE) ? "STORE" : "STOREAT", r1, r2, r3);
				break;
			}
			
			case EQQ:
			case NEQQ:
			case ISA:
			case MATCH:
			case LT:
			case GT:
			case EQ:
			case LEQ:
			case GEQ:
			case NEQ: {
				OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
				DUMP_VM(buffer, bindex, "%s %d %d %d", opcode_name(op), r1, r2, r3);
				break;
			}
				
			// binary operators
			case LSHIFT:
			case RSHIFT:
			case BAND:
			case BOR:
			case BXOR:
			case ADD:
			case SUB:
			case DIV:
			case MUL:
			case REM:
			case AND:
			case OR: {
				OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
				DUMP_VM(buffer, bindex, "%s %d %d %d", opcode_name(op), r1, r2, r3);
				break;
			}
				
			// unary operators
			case BNOT:
			case NEG:
			case NOT: {
				OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
				#pragma unused(r3)
				DUMP_VM(buffer, bindex, "%s %d %d %d", opcode_name(op), r1, r2, r3);
				break;
			}
			
			case RANGENEW: {
				OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
				DUMP_VM(buffer, bindex, "%s %d %d %d", opcode_name(op), r1, r2, r3);
				break;
			}
			
			case JUMPF: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const int32_t value);
				DUMP_VM(buffer, bindex, "JUMPF %d %d", r1, value);
				break;
			}
				
			case JUMP: {
				OPCODE_GET_ONE26bit(inst, const uint32_t value);
				DUMP_VM(buffer, bindex, "JUMP %d", value);
				break;
			}
				
			case CALL: {
				// CALL A B C => R(A) = B(C0... CN)
				OPCODE_GET_THREE8bit(inst, const uint32_t r1, const uint32_t r2, uint32_t r3);
				DUMP_VM(buffer, bindex, "CALL %d %d %d", r1, r2, r3);
				break;
			}
				
			case RET0:
			case RET: {
				if (op == RET0) {
					DUMP_VM(buffer, bindex, "RET0");
				} else {
					OPCODE_GET_ONE8bit(inst, const uint32_t r1);
					DUMP_VM(buffer, bindex, "RET %d", r1);
				}
				break;
			}
				
			case HALT: {
				DUMP_VM(buffer, bindex, "HALT");
				break;
			}
				
			case SWITCH: {
				ASSERT(0, "To be implemented");
				break;
			}
				
			case MAPNEW: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t n);
				DUMP_VM(buffer, bindex, "MAPNEW %d %d", r1, n);
				break;
			}
				
			case LISTNEW: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t n);
				DUMP_VM(buffer, bindex, "LISTNEW %d %d", r1, n);
				break;
			}
				
			case SETLIST: {
				OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, uint32_t r2, const uint32_t r3);
				#pragma unused(r3)
				DUMP_VM(buffer, bindex, "SETLIST %d %d", r1, r2);
				break;
			}
			
			case CLOSURE: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
				DUMP_VM(buffer, bindex, "CLOSURE %d %d", r1, r2);
				break;
			}
				
			case CLOSE: {
				OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
				#pragma unused(r2)
				DUMP_VM(buffer, bindex, "CLOSE %d", r1);
				break;
			}
				
			case RESERVED1:
			case RESERVED2:
			case RESERVED3:
			case RESERVED4:
			case RESERVED5:
			case RESERVED6: {
				DUMP_VM(buffer, bindex, "RESERVED");
				break;
			}
		}
		
		++pc;
	}
	
	return buffer;
	
abort_disassemble:
	if (ip && deserialize) mem_free(ip);
	if (buffer) mem_free(buffer);
	return NULL;
}
