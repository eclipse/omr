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

#ifndef TR_PPCOPSDEFINES_INCL
#define TR_PPCOPSDEFINES_INCL

#define PPCOpProp_HasRecordForm     0x00000001
#define PPCOpProp_SetsCarryFlag     0x00000002
#define PPCOpProp_SetsOverflowFlag  0x00000004
#define PPCOpProp_ReadsCarryFlag    0x00000008
#define PPCOpProp_TMAbort           0x00000010
#define PPCOpProp_BranchOp          0x00000040
#define PPCOpProp_CRLogical         0x00000080
#define PPCOpProp_DoubleFP          0x00000100
#define PPCOpProp_SingleFP          0x00000200
#define PPCOpProp_UpdateForm        0x00000400
#define PPCOpProp_AltFormat         0x00000800  // use alternate instruction format
#define PPCOpProp_AltFormatx        0x00001000  // use alternate instruction format
#define PPCOpProp_IsRecordForm      0x00002000
#define PPCOpProp_IsLoad            0x00004000
#define PPCOpProp_IsStore           0x00008000
#define PPCOpProp_IsRegCopy         0x00010000
#define PPCOpProp_Trap              0x00020000
#define PPCOpProp_SetsCtr           0x00040000
#define PPCOpProp_UsesCtr           0x00080000
#define PPCOpProp_DWord             0x00100000
#define PPCOpProp_UseMaskEnd        0x00200000  // ME or MB should be encoded
#define PPCOpProp_IsSync            0x00400000
#define PPCOpProp_IsRotateOrShift   0x00800000
#define PPCOpProp_CompareOp         0x01000000
#define PPCOpProp_SetsFPSCR         0x02000000
#define PPCOpProp_ReadsFPSCR        0x04000000
#define PPCOpProp_OffsetRequiresWordAlignment 0x08000000

#define PPCOpProp_BranchLikelyMask            0x00600000
#define PPCOpProp_BranchUnlikelyMask          0x00400000
#define PPCOpProp_BranchLikelyMaskCtr         0x01200000
#define PPCOpProp_BranchUnlikelyMaskCtr       0x01000000
#define PPCOpProp_IsBranchCTR                 0x04000000
#define PPCOpProp_BranchLikely                1
#define PPCOpProp_BranchUnlikely              0

#define PPCOpProp_LoadReserveAtomicUpdate     0x00000000
#define PPCOpProp_LoadReserveExclusiveAccess  0x00000001
#define PPCOpProp_NoHint                      0xFFFFFFFF

#define PPCOpProp_IsVMX                       0x08000000
#define PPCOpProp_SyncSideEffectFree          0x10000000 // syncs redundant if seperated by these types of ops
#define PPCOpProp_IsVSX                       0x20000000
#define PPCOpProp_UsesTarget                  0x40000000

#if defined(TR_TARGET_64BIT)
#define Op_st        std
#define Op_stu       stdu
#define Op_stx       stdx
#define Op_stux      stdux
#define Op_load      ld
#define Op_loadu     ldu
#define Op_loadx     ldx
#define Op_loadux    ldux
#define Op_cmp       cmp8
#define Op_cmpi      cmpi8
#define Op_cmpl      cmpl8
#define Op_cmpli     cmpli8
#define Op_larx      ldarx
#define Op_stcx_r    stdcx_r
#else
#define Op_st        stw
#define Op_stu       stwu
#define Op_stx       stwx
#define Op_stux      stwux
#define Op_load      lwz
#define Op_loadu     lwzu
#define Op_loadx     lwzx
#define Op_loadux    lwzux
#define Op_cmp       cmp4
#define Op_cmpi      cmpi4
#define Op_cmpl      cmpl4
#define Op_cmpli     cmpli4
#define Op_larx      lwarx
#define Op_stcx_r    stwcx_r
#endif

#endif //ifndef TR_PPCOPSDEFINES_INCL
