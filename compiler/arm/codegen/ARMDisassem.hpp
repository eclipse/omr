/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef INCL_ARMDISASSEM
#define INCL_ARMDISASSEM

#include <stdlib.h>

typedef unsigned int uint32_t;
typedef signed int int32_t;

/* minimum sizes (in number of chars) for buffers */
#define MIN_mbuffer 11
#define MIN_ibuffer 80


void disassemble(int32_t* instrPtr, char* mBuf, char* iBuf);


typedef struct
   {
   unsigned shifter_operand: 12;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned S:                1;
   unsigned opcode:           4;
   unsigned groupOpcode:      3;
   unsigned cond:             4;
   } ARMInstruction;

typedef struct
   {
   unsigned:                  4;
   unsigned bit4:             1;
   unsigned bit5:             1;
   unsigned bit6:             1;
   unsigned bit7:             1;
   unsigned:                  8;
   unsigned bit16:            1;
   unsigned bit17:            1;
   unsigned bit18:            1;
   unsigned bit19:            1;
   unsigned bit20:            1;
   unsigned bit21:            1;
   unsigned bit22:            1;
   unsigned bits23_24:        2;
   unsigned bit25:            1;
   unsigned:                  6;
   } InstructionBits;

typedef struct
   {
   signed offset:            24;
   unsigned L:                1;
   unsigned groupOpcode:      3;
   unsigned cond:             4;
   } BranchInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned:                  1;
   unsigned L:                1;
   unsigned:                  2;
   unsigned SBO:             12;
   unsigned:                  8;
   unsigned cond:             4;
   } BranchExchangeInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned shifter:          8;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned S:                1;
   unsigned opcode:           4;
   unsigned:                  3;
   unsigned cond:             4;
   } DataProcessingInstruction;

typedef struct
   {
   unsigned imm8:             8;
   unsigned rotate_imm:       4;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned S:                1;
   unsigned opcode:           4;
   unsigned:                  3;
   unsigned cond:             4;
   } DataProcessingImmedInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned:                  1;
   unsigned shift:            2;
   unsigned:                  1;
   unsigned Rs:               4;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned S:                1;
   unsigned opcode:           4;
   unsigned:                  3;
   unsigned cond:             4;
   } DataProcessingRegShiftInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned:                  1;
   unsigned shift:            2;
   unsigned shift_imm:        5;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned S:                1;
   unsigned opcode:           4;
   unsigned:                  3;
   unsigned cond:             4;
   } DataProcessingImmShiftInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned groupOpcode:      4;
   unsigned SBO1:             4;
   unsigned Rd:               4;
   unsigned SBO2:            12;
   unsigned cond:             4;
   } MiscellaneousInstruction;

typedef struct
   {
   unsigned immLo4:           4;  //  Rm for the register form
   unsigned immHi4:           4;
   unsigned rotate_imm:       4;
   unsigned Rd:               4;
   unsigned c:                1;
   unsigned x:                1;
   unsigned s:                1;
   unsigned f:                1;
   unsigned:                  1;
   unsigned to:               1;
   unsigned R:                1;
   unsigned:                  2;
   unsigned immForm:          1;
   unsigned:                  2;
   unsigned cond:             4;
   } MSRInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned:                  4;
   unsigned SBZ:              4;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned:                  2;
   unsigned B:                1;
   unsigned:                  5;
   unsigned cond:             4;
   } SwapInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned:                  4;
   unsigned Rs:               4;
   unsigned Rn:               4;
   unsigned Rd:               4;
   unsigned S:                1;
   unsigned A:                1;
   unsigned U:                1;
   unsigned L:                1;
   unsigned:                  4;
   unsigned cond:             4;
   } MultiplyInstruction;

typedef struct
   {
   unsigned Rm:               4;
   unsigned:                  1;
   unsigned shift:            2;
   unsigned shift_imm:        5;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned L:                1;
   unsigned W:                1;
   unsigned:                  1;
   unsigned U:                1;
   unsigned P:                1;
   unsigned:                  3;
   unsigned cond:             4;
   } LoadStoreInstruction;

typedef struct
   {
   unsigned immed:           12;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned:                  8;
   unsigned cond:             4;
   } LoadStoreImmInstruction;

typedef struct
   {
   unsigned immL:             4;
   unsigned:                  4;
   unsigned immH:             4;
   unsigned Rd:               4;
   unsigned Rn:               4;
   unsigned:                  8;
   unsigned cond:             4;
   } LoadStoreImmMiscInstruction;

typedef struct
   {
   unsigned register_list:   16;
   unsigned Rn:               4;
   unsigned L:                1;
   unsigned W:                1;
   unsigned S:                1;
   unsigned U:                1;
   unsigned P:                1;
   unsigned:                  3;
   unsigned cond:             4;
   } MultipleLoadStoreInstruction;

typedef struct
   {
   unsigned offset8:          8;
   unsigned cp:               4;
   unsigned CR:               4;
   unsigned Rn:               4;
   unsigned L:                1;
   unsigned W:                1;
   unsigned N:                1;
   unsigned U:                1;
   unsigned P:                1;
   unsigned:                  3;
   unsigned cond:             4;
   } CoprocessorLoadStoreInstruction;

typedef struct
   {
   unsigned immed4:           4;
   unsigned:                  4;
   unsigned immed12:         12;
   unsigned:                  8;
   unsigned cond:             4;
   } BreakpointInstruction;

typedef struct
   {
   unsigned Fm:               4;
   unsigned xfer:             1;
   unsigned M:                1;
   unsigned s:                1;
   unsigned N:                1;
   unsigned cp:               4;
   unsigned Fd:               4;
   unsigned Fn:               4;
   unsigned r:                1;
   unsigned q:   	      1;
   unsigned D:                1;
   unsigned p:                1;
   unsigned:                  4;
   unsigned cond:             4;
   } VFPInstruction;

static const char* condName[] = {"eq","ne","hs","lo","mi","pl","vs","vc","hi","ls","ge","lt","gt","le","","nv"};

#endif
