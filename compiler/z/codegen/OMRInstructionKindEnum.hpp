/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/*
 * This file will be included within an enum.  Only comments and enumerator
 * definitions are permitted.
 */

IsNotExtended,
IsLabel,           ///< Is Label
IsBranch,          ///< Is simple branch
IsBranchOnCount,   ///< Is branch on count (BRCT)
IsBranchOnIndex,   ///< Is indexed branch (BRXLE, BRXH)
IsAnnot,           ///< Is Annotation Instruction
IsPseudo,          ///< Is Pseudo instruction
IsVirtualGuardNOP, ///< Is virtual guard NOP
IsImm,             ///< Target is immediate e.g., DC 0xffff
   IsImmSnippet,
   IsImmSym,
IsImm2Byte,        ///< Target is 2 byte immediate e.g., DC 0xff
IsReg,             ///< Target is register
   IsRR,           ///< Is RR instruction eg. LR Rt,Rs
   IsRRF,          ///< Is RRF instruction eg. IEDTR R1,R3,R2
   IsRRF2,         ///< Is RR instruction eg. DIDBR Rt,Rs3,Rs2,M4
   IsRRF3,         ///< Is RR instruction eg. MSDBR Rt,Rs2,Rs
   IsRRF4,         ///< Is RR instruction eg. LDETR Rt,Rs,M4
   IsRRF5,         ///< Is RR instruction eg. LEDTR Rt,Rs,M3,M4
   IsRRD,          ///< Is RRD instruction eg. MSDBR/MSEBR/MSER/MSDR R1,R3,R2
   IsRRR,          ///< Is RRR instruction eg. AHHHR, Rt, Rs, Rs2
   IsRI,           ///< Is RI instruction eg. CHI  R1, I2
   IsRS,           ///< Is RS instruction eg. SRL R1,D2(B2)
   IsRSY,          ///< Is RSY instruction eg. STM R1,R2,-D2(B2)
   IsRX,           ///< Is RX instruction eg. A R1,D2(X2,B2)
   IsRXE,          ///< Is RX instruction eg. A R1,D2(X2,B2)
   IsRXY,          ///< Is RXY instruction eg. A R1,-D2(X2,B2)
   IsRXYb,         ///< Is RXY-b instruction eg. PFD M1,D2(X2,B2)
   IsRXF,          ///< Is RXF instruction eg. MSDB R1,R3,D2(X2,B2)
   IsRIL,          ///< Is RIL instructions eg. LARL R1, I2
   IsRRE,          ///< Is RRE instructions eg. TRxx R1,R2
   IsRRS,          ///< Is RRS instruction eg. CRB R1,R2,M3,D4(B4)
   IsRIE,          ///< Is RIE instruction eg. CRJ R1,R2,M3,I4
   IsRIS,          ///< Is RIS instruction
   IsSMI,          ///< Is SMI instruction eg BPP M1,RI2,D3(B3)
   IsMII,
   IsVRIa,         ///< Is VRI (Vector Register-and-Immediate) Operation and extended opCode field
   IsVRIb,
   IsVRIc,
   IsVRId,
   IsVRIe,
   IsVRIf,
   IsVRIg,
   IsVRIh,
   IsVRIi,
   IsVRRa,         ///< Is VRR (Vector Register-and-Register) Operation and extended opCode field
   IsVRRb,
   IsVRRc,
   IsVRRd,
   IsVRRe,
   IsVRRf,
   IsVRRg,
   IsVRRh,
   IsVRRi,
   IsVRSa,         ///< Is VRS (Vector Register-and-Storage) Operation and extended opCode field
   IsVRSb,
   IsVRSc,
   IsVRSd,
   IsVRV,          ///< Is VRV (Vector Register-and-Vector-Index) Operation and extended opCode field
   IsVRX,          ///< Is VRX (register-and-index-storage) instruction eg VL V1,D2(X2 ,B2)
   IsVSI,          ///< Is VSI (register-and-storage) Operation and extended opCode field
IsMem,             ///< Target is memory storage
   IsRSL,          ///< Is RSL instruction eg. TP D1(L1,B1)
   IsRSLb,         ///< Is RSLb instruction eg. CDZT R1,D2(L2,B2),M3
   IsS,            ///< Is S  instruction
   IsSI,           ///< Is SI instruction
   IsSIY,          ///< Is SIY instruction
   IsSSF,          ///< Is SSF Instruction eg. LPD R3,D1(B1),D2(B2)
   IsMemMem,
      IsSS1,       ///< Is SS1 instruction eg. XC D2(L,B1),D2(B2)
      IsSS2,       ///< Is SS2 instruction eg, PACK D1(L1,B1),D2(L2,B2)
      IsSS4,       ///< Is SS4 instruction eg. MVCS D1(R1,B1),D2(B2),R3
      IsSSE,       ///< Is SSE Instruction eg. MVCDK D1(B1),D2(B2)
   IsSIL,          ///< Is SIL instruction
IsE,               ///< is E instruction, i.e. SAM24, SAM31, SAM64
IsI,               ///< is I instruction, i.e. SVC
IsIE,              ///< is IE instruction, i.e. NIAI
IsOpCodeOnly,      ///< This is used for many instructions which only require the opcode
IsNOP,
IsAlignmentNop,
