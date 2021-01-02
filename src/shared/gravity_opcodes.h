//
//  gravity_opcodes.h
//  gravity
//
//  Created by Marco Bambini on 24/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_OPCODES__
#define __GRAVITY_OPCODES__

/*
        Big-endian vs Little-endian machines

        ARM architecture runs both little & big endianess, but the android, iOS, and windows phone platforms run little endian.
        95% of modern desktop computers are little-endian.
        All x86 desktops (which is nearly all desktops with the demise of the PowerPC-based Macs several years ago) are little-endian.
        It's probably actually a lot more than 95% nowadays. PowerPC was the only non-x86 architecture that has been popular for desktop
        computers in the last 20 years and Apple abandoned it in favor of x86.
        Sparc, Alpha, and Itanium did exist, but they were all very rare in the desktop market.
 */

/*
        Instructions are 32bit in length

        // 2 registers and 1 register/constant
        +------------------------------------+
        |  OP  |   Ax   |   Bx   |    Cx/K   |
        +------------------------------------+

        // instructions with no parameters
        +------------------------------------+
        |  OP  |0                            |
        +------------------------------------+

        // unconditional JUMP
        +------------------------------------+
        |  OP  |             N1              |
        +------------------------------------+

        // LOADI and JUMPF
        +------------------------------------+
        |  OP  |   Ax   |S|       N2         |
        +------------------------------------+

        OP   =>  6 bits
        Ax   =>  8 bits
        Bx   =>  8 bits
        Cx/K =>  8/10 bits
        S    =>  1 bit
        N1   =>  26 bits
        N2   =>  17 bits
 */

typedef enum {

    //      ***********************************************************************************************************
    //      56 OPCODE INSTRUCTIONS (for a register based virtual machine)
    //      opcode is a 6 bit value so at maximum 2^6 = 64 opcodes can be declared
    //      ************************************************************************************************************
    //
    //      MNEMONIC        PARAMETERS          DESCRIPTION                                 OPERATION
    //      --------        ----------          ------------------------------------        ----------------------------
    //
                                                //  *** GENERAL COMMANDS (5) ***
            RET0 = 0,       //  NONE            //  return nothing from a function          MUST BE THE FIRST OPCODE (because an implicit 0 is added
                                                //                                          as a safeguard at the end of any bytecode
            HALT,           //  NONE            //  stop VM execution
            NOP,            //  NONE            //  NOP                                     http://en.wikipedia.org/wiki/NOP
            RET,            //  A               //  return from a function                  R(-1) = R(A)
            CALL,           //  A, B, C         //  call a function                         R(A) = B(C0...Cn) B is callable object and C is num args

                                                //  *** LOAD/STORE OPERATIONS (11) ***
            LOAD,           //  A, B, C         //  load C from B and store in A            R(A) = R(B)[C]
            LOADS,          //  A, B, C         //  load C from B and store in A            R(A) = R(B)[C] (super variant)
            LOADAT,         //  A, B, C         //  load C from B and store in A            R(A) = R(B)[C]
            LOADK,          //  A, B            //  load constant into register             R(A) = K(B)
            LOADG,          //  A, B            //  load global into register               R(A) = G[K(B)]
            LOADI,          //  A, B            //  load integer into register              R(A) = I
            LOADU,          //  A, B            //  load upvalue into register              R(A) = U(B)
            MOVE,           //  A, B            //  move registers                          R(A) = R(B)
            STORE,          //  A, B, C         //  store A into R(B)[C]                    R(B)[C] = R(A)
            STOREAT,        //  A, B, C         //  store A into R(B)[C]                    R(B)[C] = R(A)
            STOREG,         //  A, B            //  store global                            G[K(B)] = R(A)
            STOREU,         //  A, B            //  store upvalue                           U(B) = R(A)

                                                //  *** JUMP OPERATIONS (3) ***
            JUMP,           //  A               //  unconditional jump                      PC += A
            JUMPF,          //  A, B            //  jump if R(A) is false                   (R(A) == 0)    ? PC += B : 0
            SWITCH,         //                  //  switch statement

                                                //  *** MATH OPERATIONS (19) ***
            ADD,            //  A, B, C         //  add operation                           R(A) = R(B) + R(C)
            SUB,            //  A, B, C         //  sub operation                           R(A) = R(B) - R(C)
            DIV,            //  A, B, C         //  div operation                           R(A) = R(B) / R(C)
            MUL,            //  A, B, C         //  mul operation                           R(A) = R(B) * R(C)
            REM,            //  A, B, C         //  rem operation                           R(A) = R(B) % R(C)
            AND,            //  A, B, C         //  and operation                           R(A) = R(B) && R(C)
            OR,             //  A, B, C         //  or operation                            R(A) = R(B) || R(C)
            LT,             //  A, B, C         //  < comparison                            R(A) = R(B) < R(C)
            GT,             //  A, B, C         //  > comparison                            R(A) = R(B) > R(C)
            EQ,             //  A, B, C         //  == comparison                           R(A) = R(B) == R(C)
            LEQ,            //  A, B, C         //  <= comparison                           R(A) = R(B) <= R(C)
            GEQ,            //  A, B, C         //  >= comparison                           R(A) = R(B) >= R(C)
            NEQ,            //  A, B, C         //  != comparison                           R(A) = R(B) != R(C)
            EQQ,            //  A, B, C         //  === comparison                          R(A) = R(B) === R(C)
            NEQQ,           //  A, B, C         //  !== comparison                          R(A) = R(B) !== R(C)
            ISA,            //  A, B, C         //  isa comparison                          R(A) = R(A).class == R(B).class
            MATCH,          //  A, B, C         //  =~ pattern match                        R(A) = R(B) =~ R(C)
            NEG,            //  A, B            //  neg operation                           R(A) = -R(B)
            NOT,            //  A, B            //  not operation                           R(A) = !R(B)

                                                //  *** BIT OPERATIONS (6) ***
            LSHIFT,         //  A, B, C         //  shift left                              R(A) = R(B) << R(C)
            RSHIFT,         //  A, B, C         //  shift right                             R(A) = R(B) >> R(C)
            BAND,           //  A, B, C         //  bit and                                 R(A) = R(B) & R(C)
            BOR,            //  A, B, C         //  bit or                                  R(A) = R(B) | R(C)
            BXOR,           //  A, B, C         //  bit xor                                 R(A) = R(B) ^ R(C)
            BNOT,           //  A, B            //  bit not                                 R(A) = ~R(B)

                                                //  *** ARRAY/MAP/RANGE OPERATIONS (4) ***
            MAPNEW,         //  A, B            //  create a new map                        R(A) = Alloc a MAP(B)
            LISTNEW,        //  A, B            //  create a new array                      R(A) = Alloc a LIST(B)
            RANGENEW,       //  A, B, C, f      //  create a new range                      R(A) = Alloc a RANGE(B,C) f flag tells if B inclusive or exclusive
            SETLIST,        //  A, B, C         //  set list/map items

                                                //  *** CLOSURES (2) ***
            CLOSURE,        //  A, B            //  create a new closure                    R(A) = closure(K(B))
            CLOSE,          //  A               //  close all upvalues from R(A)

                                                //  *** UNUSED (6) ***
            CHECK,          //  A               //  checkpoint for structs                  R(A) = R(A).clone (if A is a struct)
            RESERVED2,      //                  //  reserved for future use
            RESERVED3,      //                  //  reserved for future use
            RESERVED4,      //                  //  reserved for future use
            RESERVED5,      //                  //  reserved for future use
            RESERVED6       //                  //  reserved for future use
} opcode_t;

#define GRAVITY_LATEST_OPCODE           RESERVED6    // used in some debug code so it is very useful to define the latest opcode here

typedef enum {
    GRAVITY_NOTFOUND_INDEX              = 0,
    GRAVITY_ADD_INDEX,
    GRAVITY_SUB_INDEX,
    GRAVITY_DIV_INDEX,
    GRAVITY_MUL_INDEX,
    GRAVITY_REM_INDEX,
    GRAVITY_AND_INDEX,
    GRAVITY_OR_INDEX,
    GRAVITY_CMP_INDEX,
    GRAVITY_EQQ_INDEX,
    GRAVITY_IS_INDEX,
    GRAVITY_MATCH_INDEX,
    GRAVITY_NEG_INDEX,
    GRAVITY_NOT_INDEX,
    GRAVITY_LSHIFT_INDEX,
    GRAVITY_RSHIFT_INDEX,
    GRAVITY_BAND_INDEX,
    GRAVITY_BOR_INDEX,
    GRAVITY_BXOR_INDEX,
    GRAVITY_BNOT_INDEX,
    GRAVITY_LOAD_INDEX,
    GRAVITY_LOADS_INDEX,
    GRAVITY_LOADAT_INDEX,
    GRAVITY_STORE_INDEX,
    GRAVITY_STOREAT_INDEX,
    GRAVITY_INT_INDEX,
    GRAVITY_FLOAT_INDEX,
    GRAVITY_BOOL_INDEX,
    GRAVITY_STRING_INDEX,
    GRAVITY_EXEC_INDEX,
    GRAVITY_VTABLE_SIZE                 // MUST BE LAST ENTRY IN THIS ENUM
} GRAVITY_VTABLE_INDEX;

#define GRAVITY_OPERATOR_ADD_NAME       "+"
#define GRAVITY_OPERATOR_SUB_NAME       "-"
#define GRAVITY_OPERATOR_DIV_NAME       "/"
#define GRAVITY_OPERATOR_MUL_NAME       "*"
#define GRAVITY_OPERATOR_REM_NAME       "%"
#define GRAVITY_OPERATOR_AND_NAME       "&&"
#define GRAVITY_OPERATOR_OR_NAME        "||"
#define GRAVITY_OPERATOR_CMP_NAME       "=="
#define GRAVITY_OPERATOR_EQQ_NAME       "==="
#define GRAVITY_OPERATOR_NEQQ_NAME      "!=="
#define GRAVITY_OPERATOR_IS_NAME        "is"
#define GRAVITY_OPERATOR_MATCH_NAME     "=~"
#define GRAVITY_OPERATOR_NEG_NAME       "neg"
#define GRAVITY_OPERATOR_NOT_NAME        "!"
#define GRAVITY_OPERATOR_LSHIFT_NAME    "<<"
#define GRAVITY_OPERATOR_RSHIFT_NAME    ">>"
#define GRAVITY_OPERATOR_BAND_NAME      "&"
#define GRAVITY_OPERATOR_BOR_NAME       "|"
#define GRAVITY_OPERATOR_BXOR_NAME      "^"
#define GRAVITY_OPERATOR_BNOT_NAME      "~"
#define GRAVITY_INTERNAL_LOAD_NAME      "load"
#define GRAVITY_INTERNAL_LOADS_NAME     "loads"
#define GRAVITY_INTERNAL_STORE_NAME     "store"
#define GRAVITY_INTERNAL_LOADAT_NAME    "loadat"
#define GRAVITY_INTERNAL_STOREAT_NAME   "storeat"
#define GRAVITY_INTERNAL_NOTFOUND_NAME  "notfound"
#define GRAVITY_INTERNAL_EXEC_NAME      "exec"
#define GRAVITY_INTERNAL_LOOP_NAME      "loop"

#define GRAVITY_CLASS_INT_NAME          "Int"
#define GRAVITY_CLASS_FLOAT_NAME        "Float"
#define GRAVITY_CLASS_BOOL_NAME         "Bool"
#define GRAVITY_CLASS_STRING_NAME       "String"
#define GRAVITY_CLASS_OBJECT_NAME       "Object"
#define GRAVITY_CLASS_CLASS_NAME        "Class"
#define GRAVITY_CLASS_NULL_NAME         "Null"
#define GRAVITY_CLASS_FUNCTION_NAME     "Func"
#define GRAVITY_CLASS_FIBER_NAME        "Fiber"
#define GRAVITY_CLASS_INSTANCE_NAME     "Instance"
#define GRAVITY_CLASS_CLOSURE_NAME      "Closure"
#define GRAVITY_CLASS_LIST_NAME         "List"
#define GRAVITY_CLASS_MAP_NAME          "Map"
#define GRAVITY_CLASS_RANGE_NAME        "Range"
#define GRAVITY_CLASS_UPVALUE_NAME      "Upvalue"

#define GRAVITY_CLASS_SYSTEM_NAME       "System"
#define GRAVITY_SYSTEM_PRINT_NAME       "print"
#define GRAVITY_SYSTEM_PUT_NAME         "put"
#define GRAVITY_SYSTEM_INPUT_NAME       "input"
#define GRAVITY_SYSTEM_NANOTIME_NAME    "nanotime"

#define GRAVITY_TOCLASS_NAME            "toClass"
#define GRAVITY_TOSTRING_NAME           "toString"
#define GRAVITY_TOINT_NAME              "toInt"
#define GRAVITY_TOFLOAT_NAME            "toFloat"
#define GRAVITY_TOBOOL_NAME             "toBool"

#endif
