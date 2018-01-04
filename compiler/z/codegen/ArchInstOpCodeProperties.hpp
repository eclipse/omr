/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

/*
 * This file will be included within a static table.  Only comments and
 * definitions are permitted.
 */


      // BAD
   S390OpProp_None,

      // A
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // ADB
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // ADBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // ADTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // AEB
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // AEBR
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // AFI
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // AG
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // AGF
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // AGFI
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // AGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // AGHI
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsCC,

      // AGHIK
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // AGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // AGRK
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // AH
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsCC,

      // AHHHR
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_Src2HW |
   S390OpProp_SetsOperand1,

      // AHHLR
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_Src2LW |
   S390OpProp_SetsOperand1,

      // AHI
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsCC,

      // AHIK
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // AHY
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsCC,

      // AIH
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // AL
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // ALC
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // ALCG
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // ALCGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // ALCR
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // ALFI
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // ALG
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // ALGF
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // ALGFI
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // ALGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // ALGHSIK
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // ALGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // ALGRK
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // ALHHHR
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_Src2HW |
   S390OpProp_SetsOperand1,

      // ALHHLR
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_Src2LW |
   S390OpProp_SetsOperand1,

      // ALHSIK
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // ALR
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // ALRK
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // ALSIH
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // ALSIHN
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // ALY
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // AR
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // ARK
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // AXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // AY
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // BAL
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // BALR
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_SetsOperand1,

      // BAS
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // BASR
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_SetsOperand1,

      // BC
   S390OpProp_BranchOp |
   S390OpProp_ReadsCC |
   S390OpProp_IsLoad,

      // BCR
   S390OpProp_BranchOp |
   S390OpProp_ReadsCC,

      // BCT
   S390OpProp_BranchOp |
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // BCTG
   S390OpProp_BranchOp |
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // BCTGR
   S390OpProp_BranchOp |
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // BCTR
   S390OpProp_BranchOp |
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // BPP
   S390OpProp_None,

      // BPRP
   S390OpProp_None,

      // BRAS
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_SetsOperand1,

      // BRASL
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_SetsOperand1,

      // BRC
   S390OpProp_BranchOp |
   S390OpProp_ReadsCC,

      // BRCL
   S390OpProp_BranchOp |
   S390OpProp_ReadsCC,

      // BRCT
   S390OpProp_BranchOp |
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // BRCTG
   S390OpProp_BranchOp |
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // BRCTH
   S390OpProp_BranchOp |
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // BRXH
   S390OpProp_BranchOp |
   S390OpProp_UsesTarget |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // BRXHG
   S390OpProp_BranchOp |
   S390OpProp_UsesTarget |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // BRXLE
   S390OpProp_BranchOp |
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // BRXLG
   S390OpProp_BranchOp |
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // BXH
   S390OpProp_BranchOp |
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // BXHG
   S390OpProp_BranchOp |
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // BXLE
   S390OpProp_BranchOp |
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // BXLEG
   S390OpProp_BranchOp |
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // C
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CDB
   S390OpProp_DoubleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CDBR
   S390OpProp_DoubleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CDFBR
   S390OpProp_DoubleFP |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // CDGBR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CDGTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CDLFBR
   S390OpProp_DoubleFP |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // CDLGBR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_SetsOperand1,

      // CDSTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CDUTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CEB
   S390OpProp_SingleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CEBR
   S390OpProp_SingleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CEFBR
   S390OpProp_Is32Bit |
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // CEGBR
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CEDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CELFBR
   S390OpProp_Is32Bit |
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // CELGBR
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CEXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_UsesRegPairForSource,

      // CFDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // CFEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // CFI
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsExtendedImmediate,

      // CG
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CGDBR
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CGDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CGEBR
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CGF
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CGFI
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsExtendedImmediate,

      // CGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesTarget,

      // CGFRB
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CGFRJ
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CGFRT
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CGHI
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CGIB
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CGIJ
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp |
   S390OpProp_SetsCC,

      // CGIT
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CGR
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesTarget,

      // CGRB
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CGRJ
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp |
   S390OpProp_SetsCC,

      // CGRT
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CGXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1,

      // CH
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CHF
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW,

      // CHHR
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW,

      // CHI
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CHLR
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW,

      // CHY
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CIB
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CIH
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_TargetHW,

      // CIJ
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp |
   S390OpProp_SetsCC,

      // CIT
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CL
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesTarget,

      // CLC
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_HasTwoMemoryReferences,

      // CLCL
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_UsesTarget,

      // CLCLE
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_UsesTarget,

      // CLCLU
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_UsesTarget,

      // CLFDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // CLFEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // CLFI
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CLFIT
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CLG
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CLGDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CLGEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CLGF
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CLGFI
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CLGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesTarget,

      // CLGFRB
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLGFRJ
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLGFRT
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CLGIB
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLGIJ
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLGIT
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CLGR
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesTarget,

      // CLGRB
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLGRJ
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLGRT
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CLGT
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap |
   S390OpProp_IsLoad,

      // CLHF
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW,

      // CLHHR
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW,

      // CLHLR
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW,

      // CLIY
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CLI
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CLIB
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLIH
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_TargetHW,

      // CLIJ
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLM
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CLMH
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW,

      // CLMY
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // CLR
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesTarget,

      // CLRB
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLRJ
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CLRT
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CLT
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_IsLoad |
   S390OpProp_Trap,

      // CLY
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesTarget,

      // CPYA
   S390OpProp_SetsOperand1,

      // CR
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesTarget,

      // CRB
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp,

      // CRJ
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_BranchOp |
   S390OpProp_SetsCC,

      // CRT
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_Trap,

      // CS
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsCC,

      // CSY
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsCC,

      // CSG
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsCC,

      // CSDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CSXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CUDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // CUXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CVD
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand2,

      // CVDY
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand2,

      // CVDG
   S390OpProp_Is64Bit |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand2,

      // CXGTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CXSTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget,

      // CXUTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CY
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // D
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DIAG
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // DDB
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // DDBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // DDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsFPC |
   S390OpProp_SetsOperand1,

      // DEB
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // DEBR
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // DIDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand3,

      // DIDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsCC |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand3,

      // DIEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsCC |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand3,

      // DL
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DLGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DLR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DSG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DSGF
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DSGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DSGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsFPC |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // EAR
   S390OpProp_SetsOperand1,

      // ECAG
   S390OpProp_None,

      // EEDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // EEXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1,

      // EFPC
   S390OpProp_UsesTarget |
   S390OpProp_ReadsFPC |
   S390OpProp_SetsOperand1,

      // EPSW
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // ESDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // ESXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1,

      // ETND
   S390OpProp_None,

      // EX
   S390OpProp_IsStore |
   S390OpProp_SetsCC,

      // EXRL
   S390OpProp_IsStore |
   S390OpProp_SetsCC,

      // FIDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // FIDTR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // FIEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // FIXTR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // FLOGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // IC
   S390OpProp_TargetLW |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // ICM
   S390OpProp_TargetLW |
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // ICMH
   S390OpProp_TargetHW |
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // ICMY
   S390OpProp_TargetLW |
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // ICY
   S390OpProp_TargetLW |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // IEDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // IEXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // IIHH
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // IIHL
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // IIHF
   S390OpProp_TargetHW |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // IILF
   S390OpProp_TargetLW |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // IILH
   S390OpProp_TargetLW |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // IILL
   S390OpProp_TargetLW |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // IPM
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // KDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // KIMD
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KLMD
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KM
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KMAC
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KMC
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KMCTR
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KMF
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KMO
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // KXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget,

      // L
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LA
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAA
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAAG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAAL
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAALG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAM
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // LAMY
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // LAN
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LANG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAO
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAOG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LARL
   S390OpProp_SetsOperand1,

      // LAT
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_Trap |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAX
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAXG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAY
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LB
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LBH
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // LBR
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // LCDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LCDFR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LCEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LCGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // LCGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // LCR
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // LD
   S390OpProp_IsLoad |
   S390OpProp_DoubleFP |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LDEB
   S390OpProp_IsLoad |
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LDEBR
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LDETR
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LDXTR
   S390OpProp_DoubleFP |
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LDGR
   S390OpProp_DoubleFP |
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LDR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LDY
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LE
   S390OpProp_IsLoad |
   S390OpProp_SingleFP |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LEDBR
   S390OpProp_SetsOperand1,

      // LEDTR
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LER
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // LEY
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LFH
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // LFHAT
   S390OpProp_IsLoad |
   S390OpProp_Trap |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // LG
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LGAT
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_Trap |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LGB
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LGBR
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LGDR
   S390OpProp_Is64Bit |
   S390OpProp_DoubleFP |
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LGF
   S390OpProp_IsLoad |
   S390OpProp_Is32To64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LGFI
   S390OpProp_Is32To64Bit |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // LGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LGH
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LGHI
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LGHR
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LGR
   S390OpProp_Is64Bit |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LGRL
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // LGFRL
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

   // LLGFRL
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // LH
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LHH
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // LHI
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // LHR
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // LHY
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLC
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLCH
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // LLCR
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // LLGC
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLGCR
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LLGF
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLGFAT
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_Trap |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsOperand1,

      // LLGH
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLGHR
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LLGT
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLGTAT
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_Trap |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLGTR
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // LLH
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LLHH
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // LLHR
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // LLIHH
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LLIHL
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LLIHF
   S390OpProp_Is64Bit |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // LLILF
   S390OpProp_Is64Bit |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // LLILH
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LLILL
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LLZRGF
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_LongDispSupported,

      // LM
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // LMG
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // LMY
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // LNDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LNDFR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LNEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LNGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // LNR
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // LOC
   S390OpProp_Is32Bit |
   S390OpProp_ReadsCC |
   S390OpProp_LongDispSupported |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCFH
   S390OpProp_TargetHW |
   S390OpProp_ReadsCC |
   S390OpProp_LongDispSupported |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCFHR
   S390OpProp_TargetHW |
   S390OpProp_ReadsCC |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCG
   S390OpProp_Is64Bit |
   S390OpProp_ReadsCC |
   S390OpProp_LongDispSupported |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCGHI
   S390OpProp_Is64Bit |
   S390OpProp_ReadsCC |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCGR
   S390OpProp_Is64Bit |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCHHI
   S390OpProp_TargetHW |
   S390OpProp_ReadsCC |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCHI
   S390OpProp_Is32Bit |
   S390OpProp_ReadsCC |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LOCR
   S390OpProp_Is32Bit |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // LPD
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // LPDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LPDFR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LPDG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // LPEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LPGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // LPGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // LPQ
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LPR
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // LR
   S390OpProp_Is32Bit |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LRIC
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // LRL
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // LRV
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LRVG
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LRVGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // LRVH
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LRVR
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand1,

      // LT
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LTDBR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LTDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LTEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LTG
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LTGF
   S390OpProp_IsLoad |
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LTGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LTGR
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LTR
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LTXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsRegCopy |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXDTR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LY
   S390OpProp_IsLoad |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LZDR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LZER
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // LZRF
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_LongDispSupported,

      // LZRG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_LongDispSupported,

      // M
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MFY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MADB
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MADBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MAEB
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MAEBR
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MDB
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MDBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // MEEB
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MEEBR
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MGHI
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MH
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MHY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MHI
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MLG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MLGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MLR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MRIC
   S390OpProp_SetsCC,

      // MS
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MSY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MSDB
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MSDBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MSEB
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MSEBR
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MSG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MSGF
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MSGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MSGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MSR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MVC
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVCL
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_UsesTarget,

      // MVGHI
   S390OpProp_IsStore,

      // MVHHI
   S390OpProp_IsStore,

      // MVHI
   S390OpProp_IsStore,

      // MVI
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MVIY
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // MXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // N
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // NC
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // NG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // NGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // NGRK
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // NI
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsStore |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // NIAI
   S390OpProp_None,

      // NIHF
   S390OpProp_TargetHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // NIHH
   S390OpProp_TargetHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // NIHL
   S390OpProp_TargetHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // NILF
   S390OpProp_TargetLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // NILH
   S390OpProp_TargetLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // NILL
   S390OpProp_TargetLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // NIY
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsStore |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // NR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // NRK
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // NTSTG
   S390OpProp_Is64Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported,

      // NY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // O
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // OC
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // OG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // OGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // OGRK
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // OI
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsStore |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // OIHF
   S390OpProp_TargetHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // OIHH
   S390OpProp_TargetHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // OIHL
   S390OpProp_TargetHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // OILF
   S390OpProp_TargetLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // OILH
   S390OpProp_TargetLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // OILL
   S390OpProp_TargetLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // OIY
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsStore |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // OR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // ORK
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // OY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // PFD
   S390OpProp_LongDispSupported,

      // PFPO
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR0,

      // POPCNT
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // PPA
   S390OpProp_UsesTarget,

      // QADTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsFPC |
   S390OpProp_SetsOperand1,

      // QAXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsFPC |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,


      // RISBG
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // RISBGN
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // RISBHG
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SetsOperand1,

      // RISBLG
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesTarget |
   S390OpProp_TargetLW |
   S390OpProp_SetsOperand1,

      // RLL
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // RLLG
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // RNSBG
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // ROSBG
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // RRDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // RRXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // RXSBG
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // S
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SAR
   S390OpProp_SetsOperand1,

      // SDB
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SDBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SDTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SEB
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SEBR
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SFASR
   S390OpProp_UsesTarget |
   S390OpProp_SetsFPC,

      // SFPC
   S390OpProp_UsesTarget |
   S390OpProp_SetsFPC,

      // SG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SGF
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SGRK
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SH
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SHHHR
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SHHLR
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SHY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SL
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SLA
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SLAG
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SLAK
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SLB
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // SLBG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // SLBGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // SLBR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_ReadsCC |
   S390OpProp_SetsOperand1,

      // SLDA
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SLDL
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SLDT
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // SLXT
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SLFI
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // SLG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SLGF
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SLGFI
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // SLGFR
   S390OpProp_Is32To64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // SLGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // SLGRK
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // SLHHHR
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // SLHHLR
   S390OpProp_UsesTarget |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // SLL
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // SLLG
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SLLK
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SLR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // SLRK
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // SLY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SQDB
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SQDBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // SQEB
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SQEBR
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // SR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SRA
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SRAG
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SRAK
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SRDA
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SRDL
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SRDT
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // SRXT
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SRL
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // SRLG
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SRLK
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // SRK
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // SRST
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand2,

      // SRSTU
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand2,

      // SRNMT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit,

      // ST
   S390OpProp_IsStore |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand2,

      // STAM
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand3,

      // STAMY
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand3,

      // STC
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand2,

      // STCH
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW,

      // STCK
   S390OpProp_SetsCC |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STCKE
   S390OpProp_SetsCC |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STCKF
   S390OpProp_SetsCC |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STCM
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand3,

      // STCMH
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_SetsOperand3,

      // STCMY
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand3,

      // STCY
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand2,

      // STD
   S390OpProp_IsStore |
   S390OpProp_SingleFP |
   S390OpProp_LongDispSupported,

      // STDY
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported,

      // STE
   S390OpProp_IsStore |
   S390OpProp_DoubleFP |
   S390OpProp_LongDispSupported,

      // STEY
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported,

      // STFH
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW,

      // STG
   S390OpProp_IsStore |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand2,

      // STGRL
   S390OpProp_IsStore |
   S390OpProp_Is64Bit,

      // STH
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand2,

      // STHH
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_TargetHW,

      // STHY
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand2,

      // STM
   S390OpProp_IsStore |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand3,

      // STMG
   S390OpProp_IsStore |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand3,

      // STMY
   S390OpProp_IsStore |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand3,

      // STOC
   S390OpProp_IsStore |
   S390OpProp_Is32Bit |
   S390OpProp_ReadsCC |
   S390OpProp_LongDispSupported,

      // STOCFH
   S390OpProp_IsStore |
   S390OpProp_TargetHW |
   S390OpProp_ReadsCC |
   S390OpProp_LongDispSupported,

      // STOCG
   S390OpProp_IsStore |
   S390OpProp_Is64Bit |
   S390OpProp_ReadsCC |
   S390OpProp_LongDispSupported,

      // STPQ
   S390OpProp_Is64Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget,

      // STRIC
   S390OpProp_IsStore |
   S390OpProp_SetsCC,

      // STRL
   S390OpProp_IsStore |
   S390OpProp_Is32Bit,

      // STRV
   S390OpProp_IsStore |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported,

      // STRVG
   S390OpProp_IsStore |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported,

      // STRVH
   S390OpProp_Is32Bit |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported,

      // STY
   S390OpProp_IsStore |
   S390OpProp_Is32Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand2,

      // SXTR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // TABORT
   S390OpProp_SetsCC |
   S390OpProp_IsStore,

      // TBEGIN
   S390OpProp_SetsCC |
   S390OpProp_IsStore,

      // TBEGINC
   S390OpProp_SetsCC,

      // TCDB
   S390OpProp_DoubleFP |
   S390OpProp_SetsCC,

      // TCDT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsCC,

      // TDCDT
   S390OpProp_DoubleFP |
   S390OpProp_SetsCC,

      // TCEB
   S390OpProp_SingleFP |
   S390OpProp_SetsCC,

      // TDCET
   S390OpProp_SingleFP |
   S390OpProp_SetsCC,

      // TDCXT
   S390OpProp_DoubleFP |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget,

      // TDGDT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsCC,

      // TDGET
   S390OpProp_SingleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsCC,

      // TDGXT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget,

      // TEND
   S390OpProp_SetsCC,

      // TM
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // TMLL
   S390OpProp_SetsCC |
   S390OpProp_TargetLW,

      // TMLH
   S390OpProp_SetsCC |
   S390OpProp_TargetLW,

      // TMHL
   S390OpProp_SetsCC |
   S390OpProp_TargetHW,

      // TMHH
   S390OpProp_SetsCC |
   S390OpProp_TargetHW,

      // TMY
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

      // TR
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // TRE
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1,

      // TRIC
   S390OpProp_SetsCC,

      // TROO
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // TROT
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // TRT
   S390OpProp_ImplicitlySetsGPR2 |
   S390OpProp_ImplicitlySetsGPR1 |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_IsLoad,

      // TRTO
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // TRTT
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // TRTE
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_SetsOperand1,

      // TRTRE
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_SetsOperand1,

      // TS
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // X
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // XC
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // XG
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // XGR
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // XGRK
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // XI
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // XIHF
   S390OpProp_TargetHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // XILF
   S390OpProp_TargetLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // XIY
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // XR
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // XRK
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // XY
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // ASI
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_IsStore,

      // AGSI
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_IsStore,

      // ALSI
   S390OpProp_Is32Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_IsStore,

      // ALGSI
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_IsStore,

      // CRL
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CGRL
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CGFRL
   S390OpProp_Is32To64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CHHSI
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CHSI
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CGHSI
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CHHRL
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate,

      // CHRL
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate,

      // CGHRL
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate,

      // CLHHSI
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CLFHSI
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CLGHSI
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CLRL
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CLGRL
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CLGFRL
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CLHHRL
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CLHRL
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // CLGHRL
   S390OpProp_Is64Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_UsesTarget,

      // MSFI
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // MSGFI
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsExtendedImmediate |
   S390OpProp_SetsOperand1,

      // AP
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // BASSM
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_SetsOperand1,

      // BAKR
   S390OpProp_BranchOp |
   S390OpProp_IsCall,

      // BSM
   S390OpProp_BranchOp |
   S390OpProp_IsCall |
   S390OpProp_SetsOperand1,

      // CDS
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsCC,

      // CDSY
   S390OpProp_Is32Bit |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsCC,

      // CDSG
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsCC,

      // CFC
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR2,

      // CLST
   S390OpProp_SetsCC |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // CKSM
   S390OpProp_UsesRegPairForSource |
   S390OpProp_IsLoad |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // CP
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad |
   S390OpProp_HasTwoMemoryReferences,

      // CPSDR
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP,

      // CSCH
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1,

      // CUUTF
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // CUTFU
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // CU14
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsCC,

      // CU24
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsCC,

      // CU41
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsCC,

      // CU42
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsCC,

      // CUSE
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_ImplicitlyUsesGPR1,

      // CVB
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // CVBY
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // CVBG
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1,

      // DP
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // ED
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // EDMK
   S390OpProp_ImplicitlySetsGPR1 |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // ESAR
   S390OpProp_SetsOperand1,

      // EPAR
   S390OpProp_SetsOperand1,

      // EREG
   S390OpProp_Is32Bit |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // EREGG
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // ESTA
   S390OpProp_SetsCC |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // ESEA
   S390OpProp_SetsOperand1,

      // HSCH
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1,

      // IAC
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // IPK
   S390OpProp_ImplicitlySetsGPR2,

      // IVSK
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // ISKE
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // LAE
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LAEY
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

      // LCBB
   S390OpProp_IsLoad |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // LCTL
   S390OpProp_Is32Bit |
   S390OpProp_IsLoad,

      // LCTLG
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad,

      // LFPC
   S390OpProp_IsLoad,

      // LMH
   S390OpProp_IsLoad |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // LNGFR
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_Is32To64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LPSW
   S390OpProp_IsLoad |
   S390OpProp_SetsCC,

      // LPSWE
   S390OpProp_IsLoad |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // LRA
   S390OpProp_LongDispSupported |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // LRAG
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // LRAY
   S390OpProp_LongDispSupported |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // LURA
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // LURAG
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MC
   S390OpProp_IsStore,

      // MP
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesTarget |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MSCH
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1,

      // MSTA
   S390OpProp_UsesRegPairForTarget,

      // MVCDK
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVCSK
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVCK
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVCLU
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_UsesTarget,

      // MVCLE
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_UsesTarget,

      // MVCP
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVCS
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVN
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVO
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVZ
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // MVST
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_IsStore |
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // PACK
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // PALB
   S390OpProp_None,

      // PC
   S390OpProp_ImplicitlySetsGPR3 |
   S390OpProp_ImplicitlySetsGPR4,

      // PLO
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0 |
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2 |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsOperand4,

      // PKA
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // PKU
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // PR
   S390OpProp_None,

      // PT
   S390OpProp_None,

      // PFDRL
   S390OpProp_None,

      // RCHP
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1,

      // RSCH
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1,

      // SAC
   S390OpProp_None,

      // SAL
   S390OpProp_ImplicitlyUsesGPR1,

      // SCHM
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR2,

      // SCK
   S390OpProp_SetsCC |
   S390OpProp_IsLoad,

      // SCKC
   S390OpProp_IsLoad,

      // SIGP
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SP
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // SPKA
   S390OpProp_None,

      // SPM
   S390OpProp_SetsCC,

      // SPT
   S390OpProp_IsLoad,

      // SPX
   S390OpProp_IsLoad,

      // SRP
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // SRNM
   S390OpProp_None,

      // SSAR
   S390OpProp_None,

      // SSCH
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1,

      // SSKE
   S390OpProp_SetsCC,

      // SSM
   S390OpProp_IsLoad,

      // STAP
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STCKC
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STCPS
   S390OpProp_SetsCC |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STCRW
   S390OpProp_SetsCC |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STCTL
   S390OpProp_IsStore |
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand3,

      // STCTG
   S390OpProp_IsStore |
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand3,

      // STFPC
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STIDP
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STMH
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand3,

      // STNSM
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STOSM
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STPT
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STPX
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // STRAG
   S390OpProp_IsStore |
   S390OpProp_Is64Bit |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // STSCH
   S390OpProp_IsStore |
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_SetsOperand1,

      // STURA
   S390OpProp_IsStore,

      // STURG
   S390OpProp_IsStore,

      // SVC
   S390OpProp_None,

      // TAR
   S390OpProp_SetsCC,

      // TML
   S390OpProp_SetsCC,

      // TMH
   S390OpProp_SetsCC,

      // TP
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // TPI
   S390OpProp_SetsCC |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // TRTR
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR2 |
   S390OpProp_ImplicitlySetsGPR2 |
   S390OpProp_ImplicitlySetsGPR1 |
   S390OpProp_SetsCC |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences,

      // TPROT
   S390OpProp_IsLoad |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences,

      // TSCH
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_SetsCC |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand1,

      // UNPK
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // UNPKA
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // UNPKU
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_SetsCC |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // UPT
   S390OpProp_SetsCC |
   S390OpProp_ImplicitlySetsGPR3 |
   S390OpProp_ImplicitlySetsGPR5 |
   S390OpProp_ImplicitlySetsGPR2 |
   S390OpProp_ImplicitlySetsGPR1 |
   S390OpProp_ImplicitlySetsGPR0 |
   S390OpProp_ImplicitlyUsesGPR2 |
   S390OpProp_ImplicitlyUsesGPR1 |
   S390OpProp_ImplicitlyUsesGPR0,

      // ZAP
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore |
   S390OpProp_HasTwoMemoryReferences |
   S390OpProp_SetsOperand1,

      // AXBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // CFXBR
   S390OpProp_Is32Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CGXBR
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CLFXBR
   S390OpProp_Is32Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CLGXBR
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CXBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CXFBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CXGBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CXLFBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CXLGBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // DXBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // FIXBR
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LTXBR
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsRegCopy |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LCXBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LDXBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LEXBR
   S390OpProp_SingleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LNXBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LPXBR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXDB
   S390OpProp_IsLoad |
   S390OpProp_DoubleFP |
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXDBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXEB
   S390OpProp_IsLoad |
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXEBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LZXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MXBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // SQXBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // SXBR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // TCXB
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsCC,

      // AXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesTarget,

      // ADR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // AD
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // AER
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // AE
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // AWR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // AW
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // AUR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // AU
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // CXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CDR
   S390OpProp_DoubleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CD
   S390OpProp_DoubleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CER
   S390OpProp_SingleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag,

      // CE
   S390OpProp_SingleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_IsLoad,

      // CEFR
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // CDFR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // CXFR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CEGR
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // CDGR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // CXGR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // CFER
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CFDR
   S390OpProp_Is32Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CFXR
   S390OpProp_Is32Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CGER
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CGDR
   S390OpProp_Is64Bit |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // CGXR
   S390OpProp_Is64Bit |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsOperand1,

      // DDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // DD
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // DER
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // DE
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // DXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // FIER
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // FIDR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // FIXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // HDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // HER
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // LTDR
   S390OpProp_DoubleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_SetsOperand1,

      // LTER
   S390OpProp_SingleFP |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_SetsOperand1,

      // LTXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsCompare |
   S390OpProp_SetsCompareFlag |
   S390OpProp_SetsOperand1,

      // LCDR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LCER
   S390OpProp_SingleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LCXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LNDR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LNER
   S390OpProp_SingleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LNXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LPDR
   S390OpProp_DoubleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LPER
   S390OpProp_SingleFP |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LPXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // LEDR
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // LDXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1,

      // LEXR
   S390OpProp_SingleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_SetsOperand1,

      // LDER
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LXDR
   S390OpProp_DoubleFP |
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXER
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LDE
   S390OpProp_IsLoad |
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // LXD
   S390OpProp_IsLoad |
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // LXE
   S390OpProp_IsLoad |
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MEER
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MEE
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MD
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MXDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MXD
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MDER
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MDE
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MAER
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MADR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MAE
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MAD
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MSER
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MSDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MSE
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MSD
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MAYR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MAYHR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MAYLR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MAY
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MAYH
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MAYL
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MYR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1,

      // MYHR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MYLR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // MY
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MYH
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // MYL
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SQER
   S390OpProp_SingleFP |
   S390OpProp_SetsOperand1,

      // SQDR
   S390OpProp_DoubleFP |
   S390OpProp_SetsOperand1,

      // SQXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget,

      // SQE
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SQD
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SXR
   S390OpProp_DoubleFP |
   S390OpProp_UsesRegPairForSource |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SER
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SD
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SE
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // SWR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SW
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SUR
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // SU
   S390OpProp_SingleFP |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOperand1,

      // TBDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // TBEDR
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // THDER
   S390OpProp_SingleFP |
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // THDR
   S390OpProp_DoubleFP |
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // SAM24
   S390OpProp_None,

      // SAM31
   S390OpProp_None,

      // SAM64
   S390OpProp_None,

      // TAM
   S390OpProp_None,

      // CDZT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // CXZT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesRegPairForTarget,

      // CZDT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand2 |
   S390OpProp_SetsSignFlag,

      // CZXT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand2 |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForTarget,

      // CDPT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // CXPT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1 |
   S390OpProp_UsesRegPairForTarget,

      // CPDT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand2 |
   S390OpProp_SetsSignFlag,

      // CPXT
   S390OpProp_DoubleFP |
   S390OpProp_Is64Bit |
   S390OpProp_IsStore |
   S390OpProp_SetsOperand2 |
   S390OpProp_SetsSignFlag |
   S390OpProp_UsesRegPairForTarget,

      // LHHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LHLR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LLHFR
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LLHHHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LLHHLR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LLHLHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LLCHHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LLCHLR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // LLCLHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // SLLHH
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // SLLLH
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // SRLHH
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // SRLLH
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // NHHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // NHLR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // NLHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // XHHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // XHLR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // XLHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // OHHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // OHLR
   S390OpProp_Is32Bit |
   S390OpProp_TargetHW |
   S390OpProp_SrcLW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // OLHR
   S390OpProp_Is32Bit |
   S390OpProp_TargetLW |
   S390OpProp_SrcHW |
   S390OpProp_UsesTarget |
   S390OpProp_SetsZeroFlag |
   S390OpProp_SetsOperand1,

      // VGEF
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VGEG
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VGBM
   S390OpProp_SetsOperand1,

      // VGM
   S390OpProp_SetsOperand1,

      // VL
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // VLR
   S390OpProp_IsRegCopy |
   S390OpProp_SetsOperand1,

      // VLREP
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // VLEB
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLEH
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLEF
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLEG
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLEIB
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLEIH
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLEIF
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLEIG
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VLGV
   S390OpProp_SetsOperand1 |
   S390OpProp_Is64Bit,

      // VLLEZ
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // VLM
   S390OpProp_IsLoad |
   S390OpProp_UsesRegRangeForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOperand2,

      // VLBB
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // VLVG
   S390OpProp_IsLoad |
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_Is64Bit,

      // VLVGP
   S390OpProp_SetsOperand1 |
   S390OpProp_Is64Bit,

      // VLL
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

      // VMRH
   S390OpProp_SetsOperand1,

      // VMRL
   S390OpProp_SetsOperand1,

      // VPK
   S390OpProp_SetsOperand1,

      // VPKS
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsCC,

      // VPKLS
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsCC,

      // VPERM
   S390OpProp_SetsOperand1,

      // VPDI
   S390OpProp_SetsOperand1,

      // VREP
   S390OpProp_SetsOperand1,

      // VREPI
   S390OpProp_SetsOperand1,

      // VSCEF
   S390OpProp_IsStore,

      // VSCEG
   S390OpProp_IsStore,

      // VSEL
   S390OpProp_SetsOperand1,

      // VSEG
   S390OpProp_SetsOperand1,

      // VST
   S390OpProp_IsStore,

      // VSTEB
   S390OpProp_IsStore,

      // VSTEH
   S390OpProp_IsStore,

      // VSTEF
   S390OpProp_IsStore,

      // VSTEG
   S390OpProp_IsStore,

      // VSTM
   S390OpProp_IsStore |
   S390OpProp_UsesRegRangeForTarget,

      // VSTL
   S390OpProp_IsStore,

      // VUPH
   S390OpProp_SetsOperand1,

      // VUPLH
   S390OpProp_SetsOperand1,

      // VUPL
   S390OpProp_SetsOperand1,

      // VUPLL
   S390OpProp_SetsOperand1,

      // VA
   S390OpProp_SetsOperand1,

      // VACC
   S390OpProp_SetsOperand1,

      // VAC
   S390OpProp_SetsOperand1,

      // VACCC
   S390OpProp_SetsOperand1,

      // VN
   S390OpProp_SetsOperand1,

      // VNC
   S390OpProp_SetsOperand1,

      // VAVG
   S390OpProp_SetsOperand1,

      // VAVGL
   S390OpProp_SetsOperand1,

      // VCKSM
   S390OpProp_SetsOperand1,

      // VEC
   S390OpProp_SetsCC,

      // VECL
   S390OpProp_SetsCC,

      // VCEQ
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VCH
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VCHL
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VCLZ
   S390OpProp_SetsOperand1,

      // VCTZ
   S390OpProp_SetsOperand1,

      // VX
   S390OpProp_SetsOperand1,

      // VGFM
   S390OpProp_SetsOperand1,

      // VGFMA
   S390OpProp_SetsOperand1,

      // VLC
   S390OpProp_SetsOperand1,

      // VLP
   S390OpProp_SetsOperand1,

      // VMX
   S390OpProp_SetsOperand1,

      // VMXL
   S390OpProp_SetsOperand1,

      // VMN
   S390OpProp_SetsOperand1,

      // VMNL
   S390OpProp_SetsOperand1,

      // VMAL
   S390OpProp_SetsOperand1,

      // VMAH
   S390OpProp_SetsOperand1,

      // VMALH
   S390OpProp_SetsOperand1,

      // VMAE
   S390OpProp_SetsOperand1,

      // VMALE
   S390OpProp_SetsOperand1,

      // VMAO
   S390OpProp_SetsOperand1,

      // VMALO
   S390OpProp_SetsOperand1,

      // VMH
   S390OpProp_SetsOperand1,

      // VMLH
   S390OpProp_SetsOperand1,

      // VML
   S390OpProp_SetsOperand1,

      // VME
   S390OpProp_SetsOperand1,

      // VMLE
   S390OpProp_SetsOperand1,

      // VMO
   S390OpProp_SetsOperand1,

      // VMLO
   S390OpProp_SetsOperand1,

      // VNO
   S390OpProp_SetsOperand1,

      // VO
   S390OpProp_SetsOperand1,

      // VPOPCT
   S390OpProp_SetsOperand1,

      // VERLLV
   S390OpProp_SetsOperand1,

      // VERLL
   S390OpProp_SetsOperand1,

      // VERIM
   S390OpProp_UsesTarget |
   S390OpProp_SetsOperand1,

      // VESLV
   S390OpProp_SetsOperand1,

      // VESL
   S390OpProp_SetsOperand1,

      // VESRAV
   S390OpProp_SetsOperand1,

      // VESRA
   S390OpProp_SetsOperand1,

      // VESRLV
   S390OpProp_SetsOperand1,

      // VESRL
   S390OpProp_SetsOperand1,

      // VSL
   S390OpProp_SetsOperand1,

      // VSLB
   S390OpProp_SetsOperand1,

      // VSLDB
   S390OpProp_SetsOperand1,

      // VSRA
   S390OpProp_SetsOperand1,

      // VSRAB
   S390OpProp_SetsOperand1,

      // VSRL
   S390OpProp_SetsOperand1,

      // VSRLB
   S390OpProp_SetsOperand1,

      // VS
   S390OpProp_SetsOperand1,

      // VSCBI
   S390OpProp_SetsOperand1,

      // VSBI
   S390OpProp_SetsOperand1,

      // VSBCBI
   S390OpProp_SetsOperand1,

      // VSUMG
   S390OpProp_SetsOperand1,

      // VSUMQ
   S390OpProp_SetsOperand1,

      // VSUM
   S390OpProp_SetsOperand1,

      // VTM
   S390OpProp_SetsCC,

      // VFAE
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VFEE
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VFENE
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VISTR
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VSTRC
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VFA
   S390OpProp_SetsOperand1,

      // WFC
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC,

      // WFK
   S390OpProp_UsesTarget |
   S390OpProp_SetsCC,

      // VFCE
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VFCH
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VFCHE
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // VCDG
   S390OpProp_SetsOperand1,

      // VCDLG
   S390OpProp_SetsOperand1,

      // VCGD
   S390OpProp_SetsOperand1,

      // VCLGD
   S390OpProp_SetsOperand1,

      // VFD
   S390OpProp_SetsOperand1,

      // VFI
   S390OpProp_SetsOperand1,

      // VLDE
   S390OpProp_SetsOperand1,

      // VLED
   S390OpProp_SetsOperand1,

      // VFM
   S390OpProp_SetsOperand1,

      // VFMA
   S390OpProp_SetsOperand1,

      // VFMS
   S390OpProp_SetsOperand1,

      // VFPSO
   S390OpProp_SetsOperand1,

      // VFSQ
   S390OpProp_SetsOperand1,

      // VFS
   S390OpProp_SetsOperand1,

      // VFTCI
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

      // RIEMIT
   S390OpProp_UsesTarget,

      // RIOFF
   S390OpProp_SetsCC,

      // RION
   S390OpProp_SetsCC,

      // RINEXT
   S390OpProp_None,

      // ASSOCREGS
   S390OpProp_None,

      // DEPEND
   S390OpProp_None,

      // DS
   S390OpProp_None,

      // FENCE
   S390OpProp_None,

      // SCHEDFENCE
   S390OpProp_None,

      // PROC
   S390OpProp_None,

      // RET
   S390OpProp_None,

      // DIRECTIVE
   S390OpProp_None,

      // WRTBAR
   S390OpProp_None,

      // XPCALLDESC
   S390OpProp_None,

      // DC
   S390OpProp_None,

      // DC2
   S390OpProp_None,

      // VGNOP
   S390OpProp_None,

      // NOP
   S390OpProp_None,

      // ASM
   S390OpProp_None,

      // LABEL
   S390OpProp_None,

      // TAILCALL
   S390OpProp_IsCall,

      // DCB
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_IsLoad |
   S390OpProp_IsStore,

   // VLRLR
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

   // VLRL
   S390OpProp_IsLoad |
   S390OpProp_SetsOperand1,

   // VSTRLR
   S390OpProp_IsStore,

   // VSTRL
   S390OpProp_IsStore,

   // VAP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VCP
   S390OpProp_SetsCC,

   // VCVB
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VCVBG
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VCVD
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VCVDG
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VDP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VLIP
   S390OpProp_SetsOperand1,

   // VMP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VMSP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VPKZ
   S390OpProp_SetsOperand1,

   // VPSOP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VRP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VSDP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VSRP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VSP
   S390OpProp_SetsCC |
   S390OpProp_SetsOperand1,

   // VTP
   S390OpProp_SetsCC,

   // VUPKZ
   S390OpProp_IsStore,

   // VBPERM
   S390OpProp_SetsOperand1,

   // VNX
   S390OpProp_SetsOperand1,

   // VMSL
   S390OpProp_SetsOperand1,

   // VNN
   S390OpProp_SetsOperand1,

   // VOC
   S390OpProp_SetsOperand1,

   // VFLL
   S390OpProp_SetsOperand1,

   // VFLR
   S390OpProp_SetsOperand1,

   // VFMAX
   S390OpProp_SetsOperand1,

   // VFMIN
   S390OpProp_SetsOperand1,

   // VFNMA
   S390OpProp_SetsOperand1,

   // VFNMS
   S390OpProp_SetsOperand1,

   // KMGCM
   S390OpProp_SetsCC,

   // PRNO
   S390OpProp_None,

   // LGG
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |

   // Note:
   //
   // Though the instruction itself does not set CC we mark it as such because the condition code following a
   // guarded load is undefined because we cannot be certain whether or not the guard will be raised. As such
   // we must stay conservative with the condition code and model the instruction as if it sets the condition
   // code.
   S390OpProp_SetsCC,

   // LLGFSG
   S390OpProp_IsLoad |
   S390OpProp_Is64Bit |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |

   // Note: See note for LGG regarding condition codes and guarded load instructions
   S390OpProp_SetsCC,

   // LGSC
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported,

   // STGSC
   S390OpProp_IsStore |
   S390OpProp_LongDispSupported,

   // AGH
   S390OpProp_Is64Bit |
   S390OpProp_SetsSignFlag |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsCC,

   // BIC
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_BranchOp |
   S390OpProp_ReadsCC,

   // MG
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand1 |
   S390OpProp_LongDispSupported,

   // MGRK
   S390OpProp_UsesRegPairForTarget |
   S390OpProp_SetsOperand3,

   // MGH
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1,

   // MSC
   S390OpProp_Is32Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsCC,

   // MSRKC
   S390OpProp_Is32Bit |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsCC,

   // MSGC
   S390OpProp_Is64Bit |
   S390OpProp_UsesTarget |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOperand1 |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsCC,

   // MSGRKC
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand3 |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsCC,

   // SGH
   S390OpProp_Is64Bit |
   S390OpProp_SetsOperand1 |
   S390OpProp_IsLoad |
   S390OpProp_LongDispSupported |
   S390OpProp_SetsOverflowFlag |
   S390OpProp_SetsCC,
