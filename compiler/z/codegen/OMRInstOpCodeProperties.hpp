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
 * This file will be included within a static table.  Only comments and
 * definitions are permitted.
 */


   {
   /* .mnemonic    = */ OMR::InstOpCode::ASSOCREGS,
   /* .name        = */ "ASSOCREGS",
   /* .description = */ "Register Association",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BAD,
   /* .name        = */ "BAD",
   /* .description = */ "Bad Opcode",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BREAK,
   /* .name        = */ "BREAK",
   /* .description = */ "Breakpoint (debugger)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DC,
   /* .name        = */ "DC",
   /* .description = */ "DC",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ DC_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DC2,
   /* .name        = */ "DC2",
   /* .description = */ "DC2",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ DC_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DCB,
   /* .name        = */ "DCB",
   /* .description = */ "Debug Counter Bump",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DEPEND,
   /* .name        = */ "DEPEND",
   /* .description = */ "Someplace to hang dependencies",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FENCE,
   /* .name        = */ "FENCE",
   /* .description = */ "Fence",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LABEL,
   /* .name        = */ "LABEL",
   /* .description = */ "Destination of a jump",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LHHR,
   /* .name        = */ "LHHR",
   /* .description = */ "Load (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LHLR,
   /* .name        = */ "LHLR",
   /* .description = */ "Load (High <- Low)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLCHHR,
   /* .name        = */ "LLCHHR",
   /* .description = */ "Load Logical Character (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLCHLR,
   /* .name        = */ "LLCHLR",
   /* .description = */ "Load Logical Character (High <- low)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLCLHR,
   /* .name        = */ "LLCLHR",
   /* .description = */ "Load Logical Character (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLHFR,
   /* .name        = */ "LLHFR",
   /* .description = */ "Load (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLHHHR,
   /* .name        = */ "LLHHHR",
   /* .description = */ "Load Logical Halfword (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLHHLR,
   /* .name        = */ "LLHHLR",
   /* .description = */ "Load Logical Halfword (High <- low)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLHLHR,
   /* .name        = */ "LLHLHR",
   /* .description = */ "Load Logical Halfword (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NHHR,
   /* .name        = */ "NHHR",
   /* .description = */ "AND High (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NHLR,
   /* .name        = */ "NHLR",
   /* .description = */ "AND High (High <- Low)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NLHR,
   /* .name        = */ "NLHR",
   /* .description = */ "AND High (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OHHR,
   /* .name        = */ "OHHR",
   /* .description = */ "OR High (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OHLR,
   /* .name        = */ "OHLR",
   /* .description = */ "OR High (High <- Low)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OLHR,
   /* .name        = */ "OLHR",
   /* .description = */ "OR High (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PROC,
   /* .name        = */ "PROC",
   /* .description = */ "Entry to the method",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RET,
   /* .name        = */ "RET",
   /* .description = */ "Return",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLLHH,
   /* .name        = */ "SLLHH",
   /* .description = */ "Shift Left Logical (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLLLH,
   /* .name        = */ "SLLLH",
   /* .description = */ "Shift Left Logical (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRLHH,
   /* .name        = */ "SRLHH",
   /* .description = */ "Shift Right Logical (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRLLH,
   /* .name        = */ "SRLLH",
   /* .description = */ "Shift Right Logical (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VGNOP,
   /* .name        = */ "VGNOP",
   /* .description = */ "ValueGuardNOP",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XHHR,
   /* .name        = */ "XHHR",
   /* .description = */ "Exclusive OR High (High <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XHLR,
   /* .name        = */ "XHLR",
   /* .description = */ "Exclusive OR High (High <- Low)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XLHR,
   /* .name        = */ "XLHR",
   /* .description = */ "Exclusive OR High (Low <- High)",
   /* .opcode[0]   = */ 0x00,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SrcHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::A,
   /* .name        = */ "A",
   /* .description = */ "ADD (32)",
   /* .opcode[0]   = */ 0x5A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AD,
   /* .name        = */ "AD",
   /* .description = */ "ADD NORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x6A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ADB,
   /* .name        = */ "ADB",
   /* .description = */ "ADD (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x1A,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ADBR,
   /* .name        = */ "ADBR",
   /* .description = */ "ADD (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x1A,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ADR,
   /* .name        = */ "ADR",
   /* .description = */ "ADD NORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x2A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AE,
   /* .name        = */ "AE",
   /* .description = */ "ADD NORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x7A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AEB,
   /* .name        = */ "AEB",
   /* .description = */ "ADD (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AEBR,
   /* .name        = */ "AEBR",
   /* .description = */ "ADD (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AER,
   /* .name        = */ "AER",
   /* .description = */ "ADD NORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x3A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGFR,
   /* .name        = */ "AGFR",
   /* .description = */ "ADD (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x18,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGHI,
   /* .name        = */ "AGHI",
   /* .description = */ "ADD HALFWORD IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGR,
   /* .name        = */ "AGR",
   /* .description = */ "ADD (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AH,
   /* .name        = */ "AH",
   /* .description = */ "ADD HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0x4A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AHI,
   /* .name        = */ "AHI",
   /* .description = */ "ADD HALFWORD IMMEDIATE (32 <- 16)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AL,
   /* .name        = */ "AL",
   /* .description = */ "ADD LOGICAL (32)",
   /* .opcode[0]   = */ 0x5E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALCGR,
   /* .name        = */ "ALCGR",
   /* .description = */ "ADD LOGICAL WITH CARRY (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x88,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALCR,
   /* .name        = */ "ALCR",
   /* .description = */ "ADD LOGICAL WITH CARRY (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x98,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALGFR,
   /* .name        = */ "ALGFR",
   /* .description = */ "ADD LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x1A,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALGR,
   /* .name        = */ "ALGR",
   /* .description = */ "ADD LOGICAL (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALR,
   /* .name        = */ "ALR",
   /* .description = */ "ADD LOGICAL (32)",
   /* .opcode[0]   = */ 0x1E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AP,
   /* .name        = */ "AP",
   /* .description = */ "ADD DECIMAL",
   /* .opcode[0]   = */ 0xFA,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AR,
   /* .name        = */ "AR",
   /* .description = */ "ADD (32)",
   /* .opcode[0]   = */ 0x1A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AU,
   /* .name        = */ "AU",
   /* .description = */ "ADD UNNORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x7E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AUR,
   /* .name        = */ "AUR",
   /* .description = */ "ADD UNNORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x3E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AW,
   /* .name        = */ "AW",
   /* .description = */ "ADD UNNORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x6E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AWR,
   /* .name        = */ "AWR",
   /* .description = */ "ADD UNNORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x2E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AXBR,
   /* .name        = */ "AXBR",
   /* .description = */ "ADD (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x4A,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AXR,
   /* .name        = */ "AXR",
   /* .description = */ "ADD NORMALIZED (extended HFP)",
   /* .opcode[0]   = */ 0x36,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BAKR,
   /* .name        = */ "BAKR",
   /* .description = */ "BRANCH AND STACK",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x40,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BAL,
   /* .name        = */ "BAL",
   /* .description = */ "BRANCH AND LINK",
   /* .opcode[0]   = */ 0x45,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BALR,
   /* .name        = */ "BALR",
   /* .description = */ "BRANCH AND LINK",
   /* .opcode[0]   = */ 0x05,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BAS,
   /* .name        = */ "BAS",
   /* .description = */ "BRANCH AND SAVE",
   /* .opcode[0]   = */ 0x4D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BASR,
   /* .name        = */ "BASR",
   /* .description = */ "BRANCH AND SAVE",
   /* .opcode[0]   = */ 0x0D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BASSM,
   /* .name        = */ "BASSM",
   /* .description = */ "BRANCH AND SAVE AND SET MODE",
   /* .opcode[0]   = */ 0x0C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BC,
   /* .name        = */ "BC",
   /* .description = */ "BRANCH ON CONDITION",
   /* .opcode[0]   = */ 0x47,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BCR,
   /* .name        = */ "BCR",
   /* .description = */ "BRANCH ON CONDITION",
   /* .opcode[0]   = */ 0x07,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_ReadsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BCT,
   /* .name        = */ "BCT",
   /* .description = */ "BRANCH ON COUNT (32)",
   /* .opcode[0]   = */ 0x46,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BCTGR,
   /* .name        = */ "BCTGR",
   /* .description = */ "BRANCH ON COUNT (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x46,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BCTR,
   /* .name        = */ "BCTR",
   /* .description = */ "BRANCH ON COUNT (32)",
   /* .opcode[0]   = */ 0x06,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRAS,
   /* .name        = */ "BRAS",
   /* .description = */ "BRANCH RELATIVE AND SAVE",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RIb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRASL,
   /* .name        = */ "BRASL",
   /* .description = */ "BRANCH RELATIVE AND SAVE LONG",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRC,
   /* .name        = */ "BRC",
   /* .description = */ "BRANCH RELATIVE ON CONDITION",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RIc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_ReadsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRCL,
   /* .name        = */ "BRCL",
   /* .description = */ "BRANCH RELATIVE ON CONDITION LONG",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RILc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_ReadsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRCT,
   /* .name        = */ "BRCT",
   /* .description = */ "BRANCH RELATIVE ON COUNT (32)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RIb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRCTG,
   /* .name        = */ "BRCTG",
   /* .description = */ "BRANCH RELATIVE ON COUNT (64)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ RIb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRXH,
   /* .name        = */ "BRXH",
   /* .description = */ "BRANCH RELATIVE ON INDEX HIGH (32)",
   /* .opcode[0]   = */ 0x84,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_UsesTarget |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRXHG,
   /* .name        = */ "BRXHG",
   /* .description = */ "BRANCH RELATIVE ON INDEX HIGH (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x44,
   /* .format      = */ RIEe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_UsesTarget |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRXLE,
   /* .name        = */ "BRXLE",
   /* .description = */ "BRANCH RELATIVE ON INDEX LOW OR EQ. (32)",
   /* .opcode[0]   = */ 0x85,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRXLG,
   /* .name        = */ "BRXLG",
   /* .description = */ "BRANCH RELATIVE ON INDEX LOW OR EQ. (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x45,
   /* .format      = */ RIEe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BSM,
   /* .name        = */ "BSM",
   /* .description = */ "BRANCH AND SET MODE",
   /* .opcode[0]   = */ 0x0B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_IsCall |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BXH,
   /* .name        = */ "BXH",
   /* .description = */ "BRANCH ON INDEX HIGH (32)",
   /* .opcode[0]   = */ 0x86,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BXLE,
   /* .name        = */ "BXLE",
   /* .description = */ "BRANCH ON INDEX LOW OR EQUAL (32)",
   /* .opcode[0]   = */ 0x87,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::C,
   /* .name        = */ "C",
   /* .description = */ "COMPARE (32)",
   /* .opcode[0]   = */ 0x59,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CD,
   /* .name        = */ "CD",
   /* .description = */ "COMPARE (long HFP)",
   /* .opcode[0]   = */ 0x69,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDB,
   /* .name        = */ "CDB",
   /* .description = */ "COMPARE (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x19,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDBR,
   /* .name        = */ "CDBR",
   /* .description = */ "COMPARE (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x19,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDFBR,
   /* .name        = */ "CDFBR",
   /* .description = */ "CONVERT FROM FIXED (32 to long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x95,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDFR,
   /* .name        = */ "CDFR",
   /* .description = */ "CONVERT FROM FIXED (32 to long HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xB5,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDGBR,
   /* .name        = */ "CDGBR",
   /* .description = */ "CONVERT FROM FIXED (64 to long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA5,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDGR,
   /* .name        = */ "CDGR",
   /* .description = */ "CONVERT FROM FIXED (64 to long HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xC5,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDR,
   /* .name        = */ "CDR",
   /* .description = */ "COMPARE (long HFP)",
   /* .opcode[0]   = */ 0x29,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDS,
   /* .name        = */ "CDS",
   /* .description = */ "COMPARE DOUBLE AND SWAP (32)",
   /* .opcode[0]   = */ 0xBB,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CE,
   /* .name        = */ "CE",
   /* .description = */ "COMPARE (short HFP)",
   /* .opcode[0]   = */ 0x79,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEB,
   /* .name        = */ "CEB",
   /* .description = */ "COMPARE (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEBR,
   /* .name        = */ "CEBR",
   /* .description = */ "COMPARE (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEFBR,
   /* .name        = */ "CEFBR",
   /* .description = */ "CONVERT FROM FIXED (32 to short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x94,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEFR,
   /* .name        = */ "CEFR",
   /* .description = */ "CONVERT FROM FIXED (32 to short HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xB4,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEGBR,
   /* .name        = */ "CEGBR",
   /* .description = */ "CONVERT FROM FIXED (64 to short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA4,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEGR,
   /* .name        = */ "CEGR",
   /* .description = */ "CONVERT FROM FIXED (64 to short HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xC4,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CER,
   /* .name        = */ "CER",
   /* .description = */ "COMPARE (short HFP)",
   /* .opcode[0]   = */ 0x39,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFC,
   /* .name        = */ "CFC",
   /* .description = */ "COMPARE AND FORM CODEWORD",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x1A,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFDBR,
   /* .name        = */ "CFDBR",
   /* .description = */ "CONVERT TO FIXED (long BFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x99,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFDR,
   /* .name        = */ "CFDR",
   /* .description = */ "CONVERT TO FIXED (long HFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xB9,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFEBR,
   /* .name        = */ "CFEBR",
   /* .description = */ "CONVERT TO FIXED (short BFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x98,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFER,
   /* .name        = */ "CFER",
   /* .description = */ "CONVERT TO FIXED (short HFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xB8,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFXBR,
   /* .name        = */ "CFXBR",
   /* .description = */ "CONVERT TO FIXED (extended BFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x9A,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFXR,
   /* .name        = */ "CFXR",
   /* .description = */ "CONVERT TO FIXED (extended HFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xBA,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGDBR,
   /* .name        = */ "CGDBR",
   /* .description = */ "CONVERT TO FIXED (long BFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA9,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGDR,
   /* .name        = */ "CGDR",
   /* .description = */ "CONVERT TO FIXED (long HFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xC9,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGEBR,
   /* .name        = */ "CGEBR",
   /* .description = */ "CONVERT TO FIXED (short BFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA8,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGER,
   /* .name        = */ "CGER",
   /* .description = */ "CONVERT TO FIXED (short HFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xC8,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGFR,
   /* .name        = */ "CGFR",
   /* .description = */ "COMPARE (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x30,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGHI,
   /* .name        = */ "CGHI",
   /* .description = */ "COMPARE HALFWORD IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGR,
   /* .name        = */ "CGR",
   /* .description = */ "COMPARE (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x20,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGXBR,
   /* .name        = */ "CGXBR",
   /* .description = */ "CONVERT TO FIXED (extended BFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xAA,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGXR,
   /* .name        = */ "CGXR",
   /* .description = */ "CONVERT TO FIXED (extended HFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xCA,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CH,
   /* .name        = */ "CH",
   /* .description = */ "COMPARE HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0x49,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHI,
   /* .name        = */ "CHI",
   /* .description = */ "COMPARE HALFWORD IMMEDIATE (32 <- 16)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CKSM,
   /* .name        = */ "CKSM",
   /* .description = */ "CHECKSUM",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x41,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesRegPairForSource |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CL,
   /* .name        = */ "CL",
   /* .description = */ "COMPARE LOGICAL (32)",
   /* .opcode[0]   = */ 0x55,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLC,
   /* .name        = */ "CLC",
   /* .description = */ "COMPARE LOGICAL (character)",
   /* .opcode[0]   = */ 0xD5,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_HasTwoMemoryReferences
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLCL,
   /* .name        = */ "CLCL",
   /* .description = */ "COMPARE LOGICAL LONG",
   /* .opcode[0]   = */ 0x0F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLCLE,
   /* .name        = */ "CLCLE",
   /* .description = */ "COMPARE LOGICAL LONG EXTENDED",
   /* .opcode[0]   = */ 0xA9,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGFR,
   /* .name        = */ "CLGFR",
   /* .description = */ "COMPARE LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x31,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGR,
   /* .name        = */ "CLGR",
   /* .description = */ "COMPARE LOGICAL (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x21,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLI,
   /* .name        = */ "CLI",
   /* .description = */ "COMPARE LOGICAL (immediate)",
   /* .opcode[0]   = */ 0x95,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLM,
   /* .name        = */ "CLM",
   /* .description = */ "COMPARE LOGICAL CHAR. UNDER MASK (low)",
   /* .opcode[0]   = */ 0xBD,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLR,
   /* .name        = */ "CLR",
   /* .description = */ "COMPARE LOGICAL (32)",
   /* .opcode[0]   = */ 0x15,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLST,
   /* .name        = */ "CLST",
   /* .description = */ "COMPARE LOGICAL STRING",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x5D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CP,
   /* .name        = */ "CP",
   /* .description = */ "COMPARE DECIMAL",
   /* .opcode[0]   = */ 0xF9,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_HasTwoMemoryReferences
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CPYA,
   /* .name        = */ "CPYA",
   /* .description = */ "COPY ACCESS",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x4D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CR,
   /* .name        = */ "CR",
   /* .description = */ "COMPARE (32)",
   /* .opcode[0]   = */ 0x19,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CS,
   /* .name        = */ "CS",
   /* .description = */ "COMPARE AND SWAP (32)",
   /* .opcode[0]   = */ 0xBA,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CSCH,
   /* .name        = */ "CSCH",
   /* .description = */ "CLEAR SUBCHANNEL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x30,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CUSE,
   /* .name        = */ "CUSE",
   /* .description = */ "COMPARE UNTIL SUBSTRING EQUAL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x57,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CVB,
   /* .name        = */ "CVB",
   /* .description = */ "CONVERT TO BINARY (32)",
   /* .opcode[0]   = */ 0x4F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CVD,
   /* .name        = */ "CVD",
   /* .description = */ "CONVERT TO DECIMAL (32)",
   /* .opcode[0]   = */ 0x4E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXBR,
   /* .name        = */ "CXBR",
   /* .description = */ "COMPARE (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x49,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXFBR,
   /* .name        = */ "CXFBR",
   /* .description = */ "CONVERT FROM FIXED (32 to extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x96,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXFR,
   /* .name        = */ "CXFR",
   /* .description = */ "CONVERT FROM FIXED (32 to extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xB6,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXGBR,
   /* .name        = */ "CXGBR",
   /* .description = */ "CONVERT FROM FIXED (64 to extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA6,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXGR,
   /* .name        = */ "CXGR",
   /* .description = */ "CONVERT FROM FIXED (64 to extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xC6,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXR,
   /* .name        = */ "CXR",
   /* .description = */ "COMPARE (extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x69,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::D,
   /* .name        = */ "D",
   /* .description = */ "DIVIDE (32 <- 64)",
   /* .opcode[0]   = */ 0x5D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DD,
   /* .name        = */ "DD",
   /* .description = */ "DIVIDE (long HFP)",
   /* .opcode[0]   = */ 0x6D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DDB,
   /* .name        = */ "DDB",
   /* .description = */ "DIVIDE (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x1D,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DDBR,
   /* .name        = */ "DDBR",
   /* .description = */ "DIVIDE (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x1D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DDR,
   /* .name        = */ "DDR",
   /* .description = */ "DIVIDE (long HFP)",
   /* .opcode[0]   = */ 0x2D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DE,
   /* .name        = */ "DE",
   /* .description = */ "DIVIDE (short HFP)",
   /* .opcode[0]   = */ 0x7D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DEB,
   /* .name        = */ "DEB",
   /* .description = */ "DIVIDE (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DEBR,
   /* .name        = */ "DEBR",
   /* .description = */ "DIVIDE (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DER,
   /* .name        = */ "DER",
   /* .description = */ "DIVIDE (short HFP)",
   /* .opcode[0]   = */ 0x3D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DIDBR,
   /* .name        = */ "DIDBR",
   /* .description = */ "DIVIDE TO INTEGER (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x5B,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DIEBR,
   /* .name        = */ "DIEBR",
   /* .description = */ "DIVIDE TO INTEGER (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x53,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DLGR,
   /* .name        = */ "DLGR",
   /* .description = */ "DIVIDE LOGICAL (64 <- 128)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x87,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DLR,
   /* .name        = */ "DLR",
   /* .description = */ "DIVIDE LOGICAL (32 <- 64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x97,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DP,
   /* .name        = */ "DP",
   /* .description = */ "DIVIDE DECIMAL",
   /* .opcode[0]   = */ 0xFD,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DR,
   /* .name        = */ "DR",
   /* .description = */ "DIVIDE (32 <- 64)",
   /* .opcode[0]   = */ 0x1D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DSGFR,
   /* .name        = */ "DSGFR",
   /* .description = */ "DIVIDE SINGLE (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x1D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DSGR,
   /* .name        = */ "DSGR",
   /* .description = */ "DIVIDE SINGLE (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DXBR,
   /* .name        = */ "DXBR",
   /* .description = */ "DIVIDE (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x4D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DXR,
   /* .name        = */ "DXR",
   /* .description = */ "DIVIDE (extended HFP)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x2D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EAR,
   /* .name        = */ "EAR",
   /* .description = */ "EXTRACT ACCESS",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x4F,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ED,
   /* .name        = */ "ED",
   /* .description = */ "EDIT",
   /* .opcode[0]   = */ 0xDE,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EDMK,
   /* .name        = */ "EDMK",
   /* .description = */ "EDIT AND MARK",
   /* .opcode[0]   = */ 0xDF,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlySetsGPR1 |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EFPC,
   /* .name        = */ "EFPC",
   /* .description = */ "EXTRACT FPC",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x8C,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_ReadsFPC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EPAR,
   /* .name        = */ "EPAR",
   /* .description = */ "EXTRACT PRIMARY ASN",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x26,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EPSW,
   /* .name        = */ "EPSW",
   /* .description = */ "EXTRACT PSW",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x8D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EREG,
   /* .name        = */ "EREG",
   /* .description = */ "EXTRACT STACKED REGISTERS (32)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x49,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EREGG,
   /* .name        = */ "EREGG",
   /* .description = */ "EXTRACT STACKED REGISTERS (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ESAR,
   /* .name        = */ "ESAR",
   /* .description = */ "EXTRACT SECONDARY ASN",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x27,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ESEA,
   /* .name        = */ "ESEA",
   /* .description = */ "EXTRACT AND SET EXTENDED AUTHORITY",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x9D,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ESTA,
   /* .name        = */ "ESTA",
   /* .description = */ "EXTRACT STACKED STATE",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x4A,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EX,
   /* .name        = */ "EX",
   /* .description = */ "EXECUTE",
   /* .opcode[0]   = */ 0x44,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIDBR,
   /* .name        = */ "FIDBR",
   /* .description = */ "LOAD FP INTEGER (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x5F,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIDR,
   /* .name        = */ "FIDR",
   /* .description = */ "LOAD FP INTEGER (long HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x7F,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIEBR,
   /* .name        = */ "FIEBR",
   /* .description = */ "LOAD FP INTEGER (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x57,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIER,
   /* .name        = */ "FIER",
   /* .description = */ "LOAD FP INTEGER (short HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x77,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIXBR,
   /* .name        = */ "FIXBR",
   /* .description = */ "LOAD FP INTEGER (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x47,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIXR,
   /* .name        = */ "FIXR",
   /* .description = */ "LOAD FP INTEGER (extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x67,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::HDR,
   /* .name        = */ "HDR",
   /* .description = */ "HALVE (long HFP)",
   /* .opcode[0]   = */ 0x24,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::HER,
   /* .name        = */ "HER",
   /* .description = */ "HALVE (short HFP)",
   /* .opcode[0]   = */ 0x34,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::HSCH,
   /* .name        = */ "HSCH",
   /* .description = */ "HALT SUBCHANNEL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x31,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IAC,
   /* .name        = */ "IAC",
   /* .description = */ "INSERT ADDRESS SPACE CONTROL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x24,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IC,
   /* .name        = */ "IC",
   /* .description = */ "INSERT CHARACTER",
   /* .opcode[0]   = */ 0x43,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ICM,
   /* .name        = */ "ICM",
   /* .description = */ "INSERT CHARACTERS UNDER MASK (low)",
   /* .opcode[0]   = */ 0xBF,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IIHH,
   /* .name        = */ "IIHH",
   /* .description = */ "INSERT IMMEDIATE (high high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IIHL,
   /* .name        = */ "IIHL",
   /* .description = */ "INSERT IMMEDIATE (high low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IILH,
   /* .name        = */ "IILH",
   /* .description = */ "INSERT IMMEDIATE (low high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IILL,
   /* .name        = */ "IILL",
   /* .description = */ "INSERT IMMEDIATE (low low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IPK,
   /* .name        = */ "IPK",
   /* .description = */ "INSERT PSW KEY",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlySetsGPR2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IPM,
   /* .name        = */ "IPM",
   /* .description = */ "INSERT PROGRAM MASK",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x22,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ISKE,
   /* .name        = */ "ISKE",
   /* .description = */ "INSERT STORAGE KEY EXTENDED",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x29,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IVSK,
   /* .name        = */ "IVSK",
   /* .description = */ "INSERT VIRTUAL STORAGE KEY",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x23,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::L,
   /* .name        = */ "L",
   /* .description = */ "LOAD (32)",
   /* .opcode[0]   = */ 0x58,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LA,
   /* .name        = */ "LA",
   /* .description = */ "LOAD ADDRESS",
   /* .opcode[0]   = */ 0x41,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAE,
   /* .name        = */ "LAE",
   /* .description = */ "LOAD ADDRESS EXTENDED",
   /* .opcode[0]   = */ 0x51,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAM,
   /* .name        = */ "LAM",
   /* .description = */ "LOAD ACCESS MULTIPLE",
   /* .opcode[0]   = */ 0x9A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LARL,
   /* .name        = */ "LARL",
   /* .description = */ "LOAD ADDRESS RELATIVE LONG",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCDBR,
   /* .name        = */ "LCDBR",
   /* .description = */ "LOAD COMPLEMENT (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x13,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCDR,
   /* .name        = */ "LCDR",
   /* .description = */ "LOAD COMPLEMENT (long HFP)",
   /* .opcode[0]   = */ 0x23,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCEBR,
   /* .name        = */ "LCEBR",
   /* .description = */ "LOAD COMPLEMENT (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCER,
   /* .name        = */ "LCER",
   /* .description = */ "LOAD COMPLEMENT (short HFP)",
   /* .opcode[0]   = */ 0x33,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCGFR,
   /* .name        = */ "LCGFR",
   /* .description = */ "LOAD COMPLEMENT (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x13,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCGR,
   /* .name        = */ "LCGR",
   /* .description = */ "LOAD COMPLEMENT (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCR,
   /* .name        = */ "LCR",
   /* .description = */ "LOAD COMPLEMENT (32)",
   /* .opcode[0]   = */ 0x13,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCTL,
   /* .name        = */ "LCTL",
   /* .description = */ "LOAD CONTROL (32)",
   /* .opcode[0]   = */ 0xB7,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCXBR,
   /* .name        = */ "LCXBR",
   /* .description = */ "LOAD COMPLEMENT (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x43,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCXR,
   /* .name        = */ "LCXR",
   /* .description = */ "LOAD COMPLEMENT (extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x63,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LD,
   /* .name        = */ "LD",
   /* .description = */ "LOAD (long)",
   /* .opcode[0]   = */ 0x68,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_DoubleFP |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDE,
   /* .name        = */ "LDE",
   /* .description = */ "LOAD LENGTHENED (short to long HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x24,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDEB,
   /* .name        = */ "LDEB",
   /* .description = */ "LOAD LENGTHENED (short to long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDEBR,
   /* .name        = */ "LDEBR",
   /* .description = */ "LOAD LENGTHENED (short to long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDER,
   /* .name        = */ "LDER",
   /* .description = */ "LOAD LENGTHENED (short to long HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x24,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDR,
   /* .name        = */ "LDR",
   /* .description = */ "LOAD (long)",
   /* .opcode[0]   = */ 0x28,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDXBR,
   /* .name        = */ "LDXBR",
   /* .description = */ "LOAD ROUNDED (extended to long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x45,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDXR,
   /* .name        = */ "LDXR",
   /* .description = */ "LOAD ROUNDED (extended to long HFP)",
   /* .opcode[0]   = */ 0x25,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LE,
   /* .name        = */ "LE",
   /* .description = */ "LOAD (short)",
   /* .opcode[0]   = */ 0x78,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SingleFP |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LEDBR,
   /* .name        = */ "LEDBR",
   /* .description = */ "LOAD ROUNDED (long to short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x44,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LEDR,
   /* .name        = */ "LEDR",
   /* .description = */ "LOAD ROUNDED (long to short HFP)",
   /* .opcode[0]   = */ 0x35,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LER,
   /* .name        = */ "LER",
   /* .description = */ "LOAD (short)",
   /* .opcode[0]   = */ 0x38,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LEXBR,
   /* .name        = */ "LEXBR",
   /* .description = */ "LOAD ROUNDED (extended to short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x46,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LEXR,
   /* .name        = */ "LEXR",
   /* .description = */ "LOAD ROUNDED (extended to short HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x66,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LFPC,
   /* .name        = */ "LFPC",
   /* .description = */ "LOAD FPC",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x9D,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGFR,
   /* .name        = */ "LGFR",
   /* .description = */ "LOAD (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x14,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGHI,
   /* .name        = */ "LGHI",
   /* .description = */ "LOAD HALFWORD IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGR,
   /* .name        = */ "LGR",
   /* .description = */ "LOAD (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LH,
   /* .name        = */ "LH",
   /* .description = */ "LOAD HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0x48,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LHI,
   /* .name        = */ "LHI",
   /* .description = */ "LOAD HALFWORD IMMEDIATE (32) <- 16",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGFR,
   /* .name        = */ "LLGFR",
   /* .description = */ "LOAD LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x16,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGTR,
   /* .name        = */ "LLGTR",
   /* .description = */ "LOAD LOGICAL THIRTY ONE BITS (64 <- 31)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x17,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLIHH,
   /* .name        = */ "LLIHH",
   /* .description = */ "LOAD LOGICAL IMMEDIATE (high high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLIHL,
   /* .name        = */ "LLIHL",
   /* .description = */ "LOAD LOGICAL IMMEDIATE (high low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLILH,
   /* .name        = */ "LLILH",
   /* .description = */ "LOAD LOGICAL IMMEDIATE (low high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLILL,
   /* .name        = */ "LLILL",
   /* .description = */ "LOAD LOGICAL IMMEDIATE (low low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LM,
   /* .name        = */ "LM",
   /* .description = */ "LOAD MULTIPLE (32)",
   /* .opcode[0]   = */ 0x98,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNDBR,
   /* .name        = */ "LNDBR",
   /* .description = */ "LOAD NEGATIVE (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x11,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNDR,
   /* .name        = */ "LNDR",
   /* .description = */ "LOAD NEGATIVE (long HFP)",
   /* .opcode[0]   = */ 0x21,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNEBR,
   /* .name        = */ "LNEBR",
   /* .description = */ "LOAD NEGATIVE (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNER,
   /* .name        = */ "LNER",
   /* .description = */ "LOAD NEGATIVE (short HFP)",
   /* .opcode[0]   = */ 0x31,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNGFR,
   /* .name        = */ "LNGFR",
   /* .description = */ "LOAD NEGATIVE (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x11,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_Is32To64Bit |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNGR,
   /* .name        = */ "LNGR",
   /* .description = */ "LOAD NEGATIVE (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNR,
   /* .name        = */ "LNR",
   /* .description = */ "LOAD NEGATIVE (32)",
   /* .opcode[0]   = */ 0x11,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNXBR,
   /* .name        = */ "LNXBR",
   /* .description = */ "LOAD NEGATIVE (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x41,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNXR,
   /* .name        = */ "LNXR",
   /* .description = */ "LOAD NEGATIVE (extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x61,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPDBR,
   /* .name        = */ "LPDBR",
   /* .description = */ "LOAD POSITIVE (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x10,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPDR,
   /* .name        = */ "LPDR",
   /* .description = */ "LOAD POSITIVE (long HFP)",
   /* .opcode[0]   = */ 0x20,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPEBR,
   /* .name        = */ "LPEBR",
   /* .description = */ "LOAD POSITIVE (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPER,
   /* .name        = */ "LPER",
   /* .description = */ "LOAD POSITIVE (short HFP)",
   /* .opcode[0]   = */ 0x30,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPGFR,
   /* .name        = */ "LPGFR",
   /* .description = */ "LOAD POSITIVE (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x10,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPGR,
   /* .name        = */ "LPGR",
   /* .description = */ "LOAD POSITIVE (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPR,
   /* .name        = */ "LPR",
   /* .description = */ "LOAD POSITIVE (32)",
   /* .opcode[0]   = */ 0x10,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPSW,
   /* .name        = */ "LPSW",
   /* .description = */ "LOAD PSW",
   /* .opcode[0]   = */ 0x82,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPSWE,
   /* .name        = */ "LPSWE",
   /* .description = */ "LOAD PSW EXTENDED",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xB2,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPXBR,
   /* .name        = */ "LPXBR",
   /* .description = */ "LOAD POSITIVE (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x40,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPXR,
   /* .name        = */ "LPXR",
   /* .description = */ "LOAD POSITIVE (extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x60,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LR,
   /* .name        = */ "LR",
   /* .description = */ "LOAD (32)",
   /* .opcode[0]   = */ 0x18,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRA,
   /* .name        = */ "LRA",
   /* .description = */ "LOAD REAL ADDRESS (32)",
   /* .opcode[0]   = */ 0xB1,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_LongDispSupported |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRVGR,
   /* .name        = */ "LRVGR",
   /* .description = */ "LOAD REVERSED (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRVR,
   /* .name        = */ "LRVR",
   /* .description = */ "LOAD REVERSED (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x1F,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTDBR,
   /* .name        = */ "LTDBR",
   /* .description = */ "LOAD AND TEST (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x12,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTDR,
   /* .name        = */ "LTDR",
   /* .description = */ "LOAD AND TEST (long HFP)",
   /* .opcode[0]   = */ 0x22,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTEBR,
   /* .name        = */ "LTEBR",
   /* .description = */ "LOAD AND TEST (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTER,
   /* .name        = */ "LTER",
   /* .description = */ "LOAD AND TEST (short HFP)",
   /* .opcode[0]   = */ 0x32,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTGFR,
   /* .name        = */ "LTGFR",
   /* .description = */ "LOAD AND TEST (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x12,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTGR,
   /* .name        = */ "LTGR",
   /* .description = */ "LOAD AND TEST (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTR,
   /* .name        = */ "LTR",
   /* .description = */ "LOAD AND TEST (32)",
   /* .opcode[0]   = */ 0x12,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTXBR,
   /* .name        = */ "LTXBR",
   /* .description = */ "LOAD AND TEST (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x42,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTXR,
   /* .name        = */ "LTXR",
   /* .description = */ "LOAD AND TEST (extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x62,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LURA,
   /* .name        = */ "LURA",
   /* .description = */ "LOAD USING REAL ADDRESS (32)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x4B,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LURAG,
   /* .name        = */ "LURAG",
   /* .description = */ "LOAD USING REAL ADDRESS (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXD,
   /* .name        = */ "LXD",
   /* .description = */ "LOAD LENGTHENED (long to extended HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x25,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXDB,
   /* .name        = */ "LXDB",
   /* .description = */ "LOAD LENGTHENED (long to extended BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_DoubleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXDBR,
   /* .name        = */ "LXDBR",
   /* .description = */ "LOAD LENGTHENED (long to extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXDR,
   /* .name        = */ "LXDR",
   /* .description = */ "LOAD LENGTHENED (long to extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x25,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXE,
   /* .name        = */ "LXE",
   /* .description = */ "LOAD LENGTHENED (short to extended HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x26,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXEB,
   /* .name        = */ "LXEB",
   /* .description = */ "LOAD LENGTHENED (short to extended BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXEBR,
   /* .name        = */ "LXEBR",
   /* .description = */ "LOAD LENGTHENED (short to extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXER,
   /* .name        = */ "LXER",
   /* .description = */ "LOAD LENGTHENED (short to extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x26,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXR,
   /* .name        = */ "LXR",
   /* .description = */ "LOAD (extended)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x65,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LZDR,
   /* .name        = */ "LZDR",
   /* .description = */ "LOAD ZERO (long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x75,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LZER,
   /* .name        = */ "LZER",
   /* .description = */ "LOAD ZERO (short)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x74,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LZXR,
   /* .name        = */ "LZXR",
   /* .description = */ "LOAD ZERO (extended)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x76,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::M,
   /* .name        = */ "M",
   /* .description = */ "MULTIPLY (64 <- 32)",
   /* .opcode[0]   = */ 0x5C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MADB,
   /* .name        = */ "MADB",
   /* .description = */ "MULTIPLY AND ADD (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x1E,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MADBR,
   /* .name        = */ "MADBR",
   /* .description = */ "MULTIPLY AND ADD (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x1E,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAEB,
   /* .name        = */ "MAEB",
   /* .description = */ "MULTIPLY AND ADD (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAEBR,
   /* .name        = */ "MAEBR",
   /* .description = */ "MULTIPLY AND ADD (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MD,
   /* .name        = */ "MD",
   /* .description = */ "MULTIPLY (long HFP)",
   /* .opcode[0]   = */ 0x6C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MDB,
   /* .name        = */ "MDB",
   /* .description = */ "MULTIPLY (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x1C,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MDBR,
   /* .name        = */ "MDBR",
   /* .description = */ "MULTIPLY (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x1C,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MDE,
   /* .name        = */ "MDE",
   /* .description = */ "MULTIPLY (short to long HFP)",
   /* .opcode[0]   = */ 0x7C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MDER,
   /* .name        = */ "MDER",
   /* .description = */ "MULTIPLY (short to long HFP)",
   /* .opcode[0]   = */ 0x3C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MDR,
   /* .name        = */ "MDR",
   /* .description = */ "MULTIPLY (long HFP)",
   /* .opcode[0]   = */ 0x2C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MEE,
   /* .name        = */ "MEE",
   /* .description = */ "MULTIPLY (short HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x37,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MEEB,
   /* .name        = */ "MEEB",
   /* .description = */ "MULTIPLY (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x17,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MEEBR,
   /* .name        = */ "MEEBR",
   /* .description = */ "MULTIPLY (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x17,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MEER,
   /* .name        = */ "MEER",
   /* .description = */ "MULTIPLY (short HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x37,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MGHI,
   /* .name        = */ "MGHI",
   /* .description = */ "MULTIPLY HALFWORD IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MH,
   /* .name        = */ "MH",
   /* .description = */ "MULTIPLY HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0x4C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MHI,
   /* .name        = */ "MHI",
   /* .description = */ "MULTIPLY HALFWORD IMMEDIATE (32 <- 16)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MLGR,
   /* .name        = */ "MLGR",
   /* .description = */ "MULTIPLY LOGICAL (128 <- 64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x86,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MLR,
   /* .name        = */ "MLR",
   /* .description = */ "MULTIPLY LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x96,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MP,
   /* .name        = */ "MP",
   /* .description = */ "MULTIPLY DECIMAL",
   /* .opcode[0]   = */ 0xFC,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MR,
   /* .name        = */ "MR",
   /* .description = */ "MULTIPLY (64 <- 32)",
   /* .opcode[0]   = */ 0x1C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MS,
   /* .name        = */ "MS",
   /* .description = */ "MULTIPLY SINGLE (32)",
   /* .opcode[0]   = */ 0x71,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSCH,
   /* .name        = */ "MSCH",
   /* .description = */ "MODIFY SUBCHANNEL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x32,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSDB,
   /* .name        = */ "MSDB",
   /* .description = */ "MULTIPLY AND SUBTRACT (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x1F,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSDBR,
   /* .name        = */ "MSDBR",
   /* .description = */ "MULTIPLY AND SUBTRACT (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x1F,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSEB,
   /* .name        = */ "MSEB",
   /* .description = */ "MULTIPLY AND SUBTRACT (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSEBR,
   /* .name        = */ "MSEBR",
   /* .description = */ "MULTIPLY AND SUBTRACT (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSGFR,
   /* .name        = */ "MSGFR",
   /* .description = */ "MULTIPLY SINGLE (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x1C,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSGR,
   /* .name        = */ "MSGR",
   /* .description = */ "MULTIPLY SINGLE (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSR,
   /* .name        = */ "MSR",
   /* .description = */ "MULTIPLY SINGLE (32)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x52,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSTA,
   /* .name        = */ "MSTA",
   /* .description = */ "MODIFY STACKED STATE",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x47,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVC,
   /* .name        = */ "MVC",
   /* .description = */ "MOVE (character)",
   /* .opcode[0]   = */ 0xD2,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCDK,
   /* .name        = */ "MVCDK",
   /* .description = */ "MOVE WITH DESTINATION KEY",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ SSE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCK,
   /* .name        = */ "MVCK",
   /* .description = */ "MOVE WITH KEY",
   /* .opcode[0]   = */ 0xD9,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCL,
   /* .name        = */ "MVCL",
   /* .description = */ "MOVE LONG",
   /* .opcode[0]   = */ 0x0E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCLE,
   /* .name        = */ "MVCLE",
   /* .description = */ "MOVE LONG EXTENDED",
   /* .opcode[0]   = */ 0xA8,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCP,
   /* .name        = */ "MVCP",
   /* .description = */ "MOVE TO PRIMARY",
   /* .opcode[0]   = */ 0xDA,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCS,
   /* .name        = */ "MVCS",
   /* .description = */ "MOVE TO SECONDARY",
   /* .opcode[0]   = */ 0xDB,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCSK,
   /* .name        = */ "MVCSK",
   /* .description = */ "MOVE WITH SOURCE KEY",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ SSE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVI,
   /* .name        = */ "MVI",
   /* .description = */ "MOVE (immediate)",
   /* .opcode[0]   = */ 0x92,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVN,
   /* .name        = */ "MVN",
   /* .description = */ "MOVE NUMERICS",
   /* .opcode[0]   = */ 0xD1,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVO,
   /* .name        = */ "MVO",
   /* .description = */ "MOVE WITH OFFSET",
   /* .opcode[0]   = */ 0xF1,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVST,
   /* .name        = */ "MVST",
   /* .description = */ "MOVE STRING",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x55,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsStore |
                        S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVZ,
   /* .name        = */ "MVZ",
   /* .description = */ "MOVE ZONES",
   /* .opcode[0]   = */ 0xD3,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MXBR,
   /* .name        = */ "MXBR",
   /* .description = */ "MULTIPLY (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x4C,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MXD,
   /* .name        = */ "MXD",
   /* .description = */ "MULTIPLY (long to extended HFP)",
   /* .opcode[0]   = */ 0x67,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MXDR,
   /* .name        = */ "MXDR",
   /* .description = */ "MULTIPLY (long to extended HFP)",
   /* .opcode[0]   = */ 0x27,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MXR,
   /* .name        = */ "MXR",
   /* .description = */ "MULTIPLY (extended HFP)",
   /* .opcode[0]   = */ 0x26,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::N,
   /* .name        = */ "N",
   /* .description = */ "AND (32)",
   /* .opcode[0]   = */ 0x54,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NC,
   /* .name        = */ "NC",
   /* .description = */ "AND (character)",
   /* .opcode[0]   = */ 0xD4,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NGR,
   /* .name        = */ "NGR",
   /* .description = */ "AND (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x80,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NI,
   /* .name        = */ "NI",
   /* .description = */ "AND (immediate)",
   /* .opcode[0]   = */ 0x94,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsStore |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NIHH,
   /* .name        = */ "NIHH",
   /* .description = */ "AND IMMEDIATE (high high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NIHL,
   /* .name        = */ "NIHL",
   /* .description = */ "AND IMMEDIATE (high low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NILH,
   /* .name        = */ "NILH",
   /* .description = */ "AND IMMEDIATE (low high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NILL,
   /* .name        = */ "NILL",
   /* .description = */ "AND IMMEDIATE (low low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NOP,
   /* .name        = */ "NOP",
   /* .description = */ "No-Op",
   /* .opcode[0]   = */ 0x07,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ PSEUDO,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NR,
   /* .name        = */ "NR",
   /* .description = */ "AND (32)",
   /* .opcode[0]   = */ 0x14,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::O,
   /* .name        = */ "O",
   /* .description = */ "OR (32)",
   /* .opcode[0]   = */ 0x56,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OC,
   /* .name        = */ "OC",
   /* .description = */ "OR (character)",
   /* .opcode[0]   = */ 0xD6,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OGR,
   /* .name        = */ "OGR",
   /* .description = */ "OR (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x81,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OI,
   /* .name        = */ "OI",
   /* .description = */ "OR (immediate)",
   /* .opcode[0]   = */ 0x96,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsStore |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OIHH,
   /* .name        = */ "OIHH",
   /* .description = */ "OR IMMEDIATE (high high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OIHL,
   /* .name        = */ "OIHL",
   /* .description = */ "OR IMMEDIATE (high low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OILH,
   /* .name        = */ "OILH",
   /* .description = */ "OR IMMEDIATE (low high)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OILL,
   /* .name        = */ "OILL",
   /* .description = */ "OR IMMEDIATE (low low)",
   /* .opcode[0]   = */ 0xA5,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OR,
   /* .name        = */ "OR",
   /* .description = */ "OR (32)",
   /* .opcode[0]   = */ 0x16,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PACK,
   /* .name        = */ "PACK",
   /* .description = */ "PACK",
   /* .opcode[0]   = */ 0xF2,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PALB,
   /* .name        = */ "PALB",
   /* .description = */ "PURGE ALB",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x48,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PC,
   /* .name        = */ "PC",
   /* .description = */ "PROGRAM CALL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x18,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_ImplicitlySetsGPR3 |
                        S390OpProp_ImplicitlySetsGPR4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PKA,
   /* .name        = */ "PKA",
   /* .description = */ "PACK ASCII",
   /* .opcode[0]   = */ 0xE9,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PKU,
   /* .name        = */ "PKU",
   /* .description = */ "PACK UNICODE",
   /* .opcode[0]   = */ 0xE1,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PLO,
   /* .name        = */ "PLO",
   /* .description = */ "PERFORM LOCKED OPERATION",
   /* .opcode[0]   = */ 0xEE,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsOperand4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PR,
   /* .name        = */ "PR",
   /* .description = */ "PROGRAM RETURN",
   /* .opcode[0]   = */ 0x01,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PT,
   /* .name        = */ "PT",
   /* .description = */ "PROGRAM TRANSFER",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x28,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RCHP,
   /* .name        = */ "RCHP",
   /* .description = */ "RESET CHANNEL PATH",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x3B,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RSCH,
   /* .name        = */ "RSCH",
   /* .description = */ "RESUME SUBCHANNEL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x38,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::S,
   /* .name        = */ "S",
   /* .description = */ "SUBTRACT (32)",
   /* .opcode[0]   = */ 0x5B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SAC,
   /* .name        = */ "SAC",
   /* .description = */ "SET ADDRESS SPACE CONTROL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x19,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SAL,
   /* .name        = */ "SAL",
   /* .description = */ "SET ADDRESS LIMIT",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x37,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SAM24,
   /* .name        = */ "SAM24",
   /* .description = */ "SET ADDRESSING MODE (24)",
   /* .opcode[0]   = */ 0x01,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SAM31,
   /* .name        = */ "SAM31",
   /* .description = */ "SET ADDRESSING MODE (31)",
   /* .opcode[0]   = */ 0x01,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SAM64,
   /* .name        = */ "SAM64",
   /* .description = */ "SET ADDRESSING MODE (64)",
   /* .opcode[0]   = */ 0x01,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SAR,
   /* .name        = */ "SAR",
   /* .description = */ "SET ACCESS",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x4E,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SCHM,
   /* .name        = */ "SCHM",
   /* .description = */ "SET CHANNEL MONITOR",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x3C,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SCK,
   /* .name        = */ "SCK",
   /* .description = */ "SET CLOCK",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SCKC,
   /* .name        = */ "SCKC",
   /* .description = */ "SET CLOCK COMPARATOR",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SD,
   /* .name        = */ "SD",
   /* .description = */ "SUBTRACT NORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x6B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SDB,
   /* .name        = */ "SDB",
   /* .description = */ "SUBTRACT (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x1B,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SDBR,
   /* .name        = */ "SDBR",
   /* .description = */ "SUBTRACT (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x1B,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SDR,
   /* .name        = */ "SDR",
   /* .description = */ "SUBTRACT NORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x2B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SE,
   /* .name        = */ "SE",
   /* .description = */ "SUBTRACT NORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x7B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SEB,
   /* .name        = */ "SEB",
   /* .description = */ "SUBTRACT (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SEBR,
   /* .name        = */ "SEBR",
   /* .description = */ "SUBTRACT (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SER,
   /* .name        = */ "SER",
   /* .description = */ "SUBTRACT NORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x3B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SFPC,
   /* .name        = */ "SFPC",
   /* .description = */ "SET FPC",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x84,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsFPC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SGFR,
   /* .name        = */ "SGFR",
   /* .description = */ "SUBTRACT (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x19,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SGR,
   /* .name        = */ "SGR",
   /* .description = */ "SUBTRACT (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SH,
   /* .name        = */ "SH",
   /* .description = */ "SUBTRACT HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0x4B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SIGP,
   /* .name        = */ "SIGP",
   /* .description = */ "SIGNAL PROCESSOR",
   /* .opcode[0]   = */ 0xAE,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SL,
   /* .name        = */ "SL",
   /* .description = */ "SUBTRACT LOGICAL (32)",
   /* .opcode[0]   = */ 0x5F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLA,
   /* .name        = */ "SLA",
   /* .description = */ "SHIFT LEFT SINGLE (32)",
   /* .opcode[0]   = */ 0x8B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLBGR,
   /* .name        = */ "SLBGR",
   /* .description = */ "SUBTRACT LOGICAL WITH BORROW (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x89,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLBR,
   /* .name        = */ "SLBR",
   /* .description = */ "SUBTRACT LOGICAL WITH BORROW (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x99,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLDA,
   /* .name        = */ "SLDA",
   /* .description = */ "SHIFT LEFT DOUBLE (64)",
   /* .opcode[0]   = */ 0x8F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLDL,
   /* .name        = */ "SLDL",
   /* .description = */ "SHIFT LEFT DOUBLE LOGICAL (64)",
   /* .opcode[0]   = */ 0x8D,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLGFR,
   /* .name        = */ "SLGFR",
   /* .description = */ "SUBTRACT LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x1B,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLGR,
   /* .name        = */ "SLGR",
   /* .description = */ "SUBTRACT LOGICAL (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLL,
   /* .name        = */ "SLL",
   /* .description = */ "SHIFT LEFT SINGLE LOGICAL (32)",
   /* .opcode[0]   = */ 0x89,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLR,
   /* .name        = */ "SLR",
   /* .description = */ "SUBTRACT LOGICAL (32)",
   /* .opcode[0]   = */ 0x1F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SP,
   /* .name        = */ "SP",
   /* .description = */ "SUBTRACT DECIMAL",
   /* .opcode[0]   = */ 0xFB,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SPKA,
   /* .name        = */ "SPKA",
   /* .description = */ "SET PSW KEY FROM ADDRESS",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SPM,
   /* .name        = */ "SPM",
   /* .description = */ "SET PROGRAM MASK",
   /* .opcode[0]   = */ 0x04,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SPT,
   /* .name        = */ "SPT",
   /* .description = */ "SET CPU TIMER",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SPX,
   /* .name        = */ "SPX",
   /* .description = */ "SET PREFIX",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x10,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQD,
   /* .name        = */ "SQD",
   /* .description = */ "SQUARE ROOT (long HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x35,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQDB,
   /* .name        = */ "SQDB",
   /* .description = */ "SQUARE ROOT (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x15,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQDBR,
   /* .name        = */ "SQDBR",
   /* .description = */ "SQUARE ROOT (long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x15,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQDR,
   /* .name        = */ "SQDR",
   /* .description = */ "SQUARE ROOT (long HFP)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x44,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQE,
   /* .name        = */ "SQE",
   /* .description = */ "SQUARE ROOT (short HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x34,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQEB,
   /* .name        = */ "SQEB",
   /* .description = */ "SQUARE ROOT (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x14,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQEBR,
   /* .name        = */ "SQEBR",
   /* .description = */ "SQUARE ROOT (short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x14,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQER,
   /* .name        = */ "SQER",
   /* .description = */ "SQUARE ROOT (short HFP)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x45,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQXBR,
   /* .name        = */ "SQXBR",
   /* .description = */ "SQUARE ROOT (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x16,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SQXR,
   /* .name        = */ "SQXR",
   /* .description = */ "SQUARE ROOT (extended HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x36,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SR,
   /* .name        = */ "SR",
   /* .description = */ "SUBTRACT (32)",
   /* .opcode[0]   = */ 0x1B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRA,
   /* .name        = */ "SRA",
   /* .description = */ "SHIFT RIGHT SINGLE (32)",
   /* .opcode[0]   = */ 0x8A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRDA,
   /* .name        = */ "SRDA",
   /* .description = */ "SHIFT RIGHT DOUBLE (64)",
   /* .opcode[0]   = */ 0x8E,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRDL,
   /* .name        = */ "SRDL",
   /* .description = */ "SHIFT RIGHT DOUBLE LOGICAL (64)",
   /* .opcode[0]   = */ 0x8C,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRL,
   /* .name        = */ "SRL",
   /* .description = */ "SHIFT RIGHT SINGLE LOGICAL (32)",
   /* .opcode[0]   = */ 0x88,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRNM,
   /* .name        = */ "SRNM",
   /* .description = */ "SET BFP ROUNDING MODE (2 bit)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x99,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRP,
   /* .name        = */ "SRP",
   /* .description = */ "SHIFT AND ROUND DECIMAL",
   /* .opcode[0]   = */ 0xF0,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRST,
   /* .name        = */ "SRST",
   /* .description = */ "SEARCH STRING",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x5E,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SSAR,
   /* .name        = */ "SSAR",
   /* .description = */ "SET SECONDARY ASN",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x25,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SSCH,
   /* .name        = */ "SSCH",
   /* .description = */ "START SUBCHANNEL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x33,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SSM,
   /* .name        = */ "SSM",
   /* .description = */ "SET SYSTEM MASK",
   /* .opcode[0]   = */ 0x80,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ST,
   /* .name        = */ "ST",
   /* .description = */ "STORE (32)",
   /* .opcode[0]   = */ 0x50,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STAM,
   /* .name        = */ "STAM",
   /* .description = */ "STORE ACCESS MULTIPLE",
   /* .opcode[0]   = */ 0x9B,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STAP,
   /* .name        = */ "STAP",
   /* .description = */ "STORE CPU ADDRESS",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x12,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STC,
   /* .name        = */ "STC",
   /* .description = */ "STORE CHARACTER",
   /* .opcode[0]   = */ 0x42,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCK,
   /* .name        = */ "STCK",
   /* .description = */ "STORE CLOCK",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCKC,
   /* .name        = */ "STCKC",
   /* .description = */ "STORE CLOCK COMPARATOR",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCKE,
   /* .name        = */ "STCKE",
   /* .description = */ "STORE CLOCK EXTENDED",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x78,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCM,
   /* .name        = */ "STCM",
   /* .description = */ "STORE CHARACTERS UNDER MASK (low)",
   /* .opcode[0]   = */ 0xBE,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCPS,
   /* .name        = */ "STCPS",
   /* .description = */ "STORE CHANNEL PATH STATUS",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x3A,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCRW,
   /* .name        = */ "STCRW",
   /* .description = */ "STORE CHANNEL REPORT WORD",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x39,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCTL,
   /* .name        = */ "STCTL",
   /* .description = */ "STORE CONTROL (32)",
   /* .opcode[0]   = */ 0xB6,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STD,
   /* .name        = */ "STD",
   /* .description = */ "STORE (long)",
   /* .opcode[0]   = */ 0x60,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SingleFP |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STE,
   /* .name        = */ "STE",
   /* .description = */ "STORE (short)",
   /* .opcode[0]   = */ 0x70,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_DoubleFP |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STFPC,
   /* .name        = */ "STFPC",
   /* .description = */ "STORE FPC",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x9C,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STH,
   /* .name        = */ "STH",
   /* .description = */ "STORE HALFWORD (16)",
   /* .opcode[0]   = */ 0x40,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STIDP,
   /* .name        = */ "STIDP",
   /* .description = */ "STORE CPU ID",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STM,
   /* .name        = */ "STM",
   /* .description = */ "STORE MULTIPLE (32)",
   /* .opcode[0]   = */ 0x90,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STNSM,
   /* .name        = */ "STNSM",
   /* .description = */ "STORE THEN AND SYSTEM MASK",
   /* .opcode[0]   = */ 0xAC,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STOSM,
   /* .name        = */ "STOSM",
   /* .description = */ "STORE THEN OR SYSTEM MASK",
   /* .opcode[0]   = */ 0xAD,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STPT,
   /* .name        = */ "STPT",
   /* .description = */ "STORE CPU TIMER",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STPX,
   /* .name        = */ "STPX",
   /* .description = */ "STORE PREFIX",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x11,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STRAG,
   /* .name        = */ "STRAG",
   /* .description = */ "STORE REAL ADDRESS",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ SSE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is64Bit |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STSCH,
   /* .name        = */ "STSCH",
   /* .description = */ "STORE SUBCHANNEL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x34,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STURA,
   /* .name        = */ "STURA",
   /* .description = */ "STORE USING REAL ADDRESS (32)",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x46,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STURG,
   /* .name        = */ "STURG",
   /* .description = */ "STORE USING REAL ADDRESS (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x25,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SU,
   /* .name        = */ "SU",
   /* .description = */ "SUBTRACT UNNORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x7F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SUR,
   /* .name        = */ "SUR",
   /* .description = */ "SUBTRACT UNNORMALIZED (short HFP)",
   /* .opcode[0]   = */ 0x3F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SVC,
   /* .name        = */ "SVC",
   /* .description = */ "SUPERVISOR CALL",
   /* .opcode[0]   = */ 0x0A,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ I_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SW,
   /* .name        = */ "SW",
   /* .description = */ "SUBTRACT UNNORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x6F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SWR,
   /* .name        = */ "SWR",
   /* .description = */ "SUBTRACT UNNORMALIZED (long HFP)",
   /* .opcode[0]   = */ 0x2F,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SXBR,
   /* .name        = */ "SXBR",
   /* .description = */ "SUBTRACT (extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x4B,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SXR,
   /* .name        = */ "SXR",
   /* .description = */ "SUBTRACT NORMALIZED (extended HFP)",
   /* .opcode[0]   = */ 0x37,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TAM,
   /* .name        = */ "TAM",
   /* .description = */ "TEST ADDRESSING MODE",
   /* .opcode[0]   = */ 0x01,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TAR,
   /* .name        = */ "TAR",
   /* .description = */ "TEST ACCESS",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x4C,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TBDR,
   /* .name        = */ "TBDR",
   /* .description = */ "CONVERT HFP TO BFP (long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x51,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TBEDR,
   /* .name        = */ "TBEDR",
   /* .description = */ "CONVERT HFP TO BFP (long to short)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x50,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TCDB,
   /* .name        = */ "TCDB",
   /* .description = */ "TEST DATA CLASS (long BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x11,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TCEB,
   /* .name        = */ "TCEB",
   /* .description = */ "TEST DATA CLASS (short BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x10,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TCXB,
   /* .name        = */ "TCXB",
   /* .description = */ "TEST DATA CLASS (extended BFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x12,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::THDER,
   /* .name        = */ "THDER",
   /* .description = */ "CONVERT BFP TO HFP (short to long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x58,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::THDR,
   /* .name        = */ "THDR",
   /* .description = */ "CONVERT BFP TO HFP (long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x59,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TM,
   /* .name        = */ "TM",
   /* .description = */ "TEST UNDER MASK",
   /* .opcode[0]   = */ 0x91,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TMH,
   /* .name        = */ "TMH",
   /* .description = */ "TEST UNDER MASK HIGH",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TMHH,
   /* .name        = */ "TMHH",
   /* .description = */ "TEST UNDER MASK (high high)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TMHL,
   /* .name        = */ "TMHL",
   /* .description = */ "TEST UNDER MASK (high low)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TML,
   /* .name        = */ "TML",
   /* .description = */ "TEST UNDER MASK LOW",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TMLH,
   /* .name        = */ "TMLH",
   /* .description = */ "TEST UNDER MASK (low high)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TMLL,
   /* .name        = */ "TMLL",
   /* .description = */ "TEST UNDER MASK (low low)",
   /* .opcode[0]   = */ 0xA7,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TP,
   /* .name        = */ "TP",
   /* .description = */ "TEST DECIMAL",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xC0,
   /* .format      = */ RSLa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TPI,
   /* .name        = */ "TPI",
   /* .description = */ "TEST PENDING INTERRUPTION",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x36,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TPROT,
   /* .name        = */ "TPROT",
   /* .description = */ "TEST PROTECTION",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ SSE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TR,
   /* .name        = */ "TR",
   /* .description = */ "TRANSLATE",
   /* .opcode[0]   = */ 0xDC,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TRE,
   /* .name        = */ "TRE",
   /* .description = */ "TRANSLATE EXTENDED",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xA5,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TRT,
   /* .name        = */ "TRT",
   /* .description = */ "TRANSLATE AND TEST",
   /* .opcode[0]   = */ 0xDD,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlySetsGPR2 |
                        S390OpProp_ImplicitlySetsGPR1 |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TS,
   /* .name        = */ "TS",
   /* .description = */ "TEST AND SET",
   /* .opcode[0]   = */ 0x93,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TSCH,
   /* .name        = */ "TSCH",
   /* .description = */ "TEST SUBCHANNEL",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x35,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_SetsCC |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::UNPK,
   /* .name        = */ "UNPK",
   /* .description = */ "UNPACK",
   /* .opcode[0]   = */ 0xF3,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::UNPKA,
   /* .name        = */ "UNPKA",
   /* .description = */ "UNPACK ASCII",
   /* .opcode[0]   = */ 0xEA,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::UNPKU,
   /* .name        = */ "UNPKU",
   /* .description = */ "UNPACK UNICODE",
   /* .opcode[0]   = */ 0xE2,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_SetsCC |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::UPT,
   /* .name        = */ "UPT",
   /* .description = */ "UPDATE TREE",
   /* .opcode[0]   = */ 0x01,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlySetsGPR3 |
                        S390OpProp_ImplicitlySetsGPR5 |
                        S390OpProp_ImplicitlySetsGPR2 |
                        S390OpProp_ImplicitlySetsGPR1 |
                        S390OpProp_ImplicitlySetsGPR0 |
                        S390OpProp_ImplicitlyUsesGPR2 |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::X,
   /* .name        = */ "X",
   /* .description = */ "EXCLUSIVE OR (32)",
   /* .opcode[0]   = */ 0x57,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RXa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XC,
   /* .name        = */ "XC",
   /* .description = */ "EXCLUSIVE OR (character)",
   /* .opcode[0]   = */ 0xD7,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XGR,
   /* .name        = */ "XGR",
   /* .description = */ "EXCLUSIVE OR (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x82,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XI,
   /* .name        = */ "XI",
   /* .description = */ "EXCLUSIVE OR (immediate)",
   /* .opcode[0]   = */ 0x97,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XR,
   /* .name        = */ "XR",
   /* .description = */ "EXCLUSIVE OR (32)",
   /* .opcode[0]   = */ 0x17,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RR_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ZAP,
   /* .name        = */ "ZAP",
   /* .description = */ "ZERO AND ADD",
   /* .opcode[0]   = */ 0xF8,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z900,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AG,
   /* .name        = */ "AG",
   /* .description = */ "ADD (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGF,
   /* .name        = */ "AGF",
   /* .description = */ "ADD (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x18,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AHY,
   /* .name        = */ "AHY",
   /* .description = */ "ADD HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x7A,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALC,
   /* .name        = */ "ALC",
   /* .description = */ "ADD LOGICAL WITH CARRY (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x98,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALCG,
   /* .name        = */ "ALCG",
   /* .description = */ "ADD LOGICAL WITH CARRY (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x88,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALG,
   /* .name        = */ "ALG",
   /* .description = */ "ADD LOGICAL (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALGF,
   /* .name        = */ "ALGF",
   /* .description = */ "ADD LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x1A,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALY,
   /* .name        = */ "ALY",
   /* .description = */ "ADD LOGICAL (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x5E,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AY,
   /* .name        = */ "AY",
   /* .description = */ "ADD (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x5A,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BCTG,
   /* .name        = */ "BCTG",
   /* .description = */ "BRANCH ON COUNT (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x46,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BXHG,
   /* .name        = */ "BXHG",
   /* .description = */ "BRANCH ON INDEX HIGH (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x44,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BXLEG,
   /* .name        = */ "BXLEG",
   /* .description = */ "BRANCH ON INDEX LOW OR EQUAL (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x45,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDSG,
   /* .name        = */ "CDSG",
   /* .description = */ "COMPARE DOUBLE AND SWAP (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x3E,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDSY,
   /* .name        = */ "CDSY",
   /* .description = */ "COMPARE DOUBLE AND SWAP (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x31,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CG,
   /* .name        = */ "CG",
   /* .description = */ "COMPARE (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x20,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGF,
   /* .name        = */ "CGF",
   /* .description = */ "COMPARE (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x30,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHY,
   /* .name        = */ "CHY",
   /* .description = */ "COMPARE HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x79,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLCLU,
   /* .name        = */ "CLCLU",
   /* .description = */ "COMPARE LOGICAL LONG UNICODE",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x8F,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLG,
   /* .name        = */ "CLG",
   /* .description = */ "COMPARE LOGICAL (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x21,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGF,
   /* .name        = */ "CLGF",
   /* .description = */ "COMPARE LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x31,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLIY,
   /* .name        = */ "CLIY",
   /* .description = */ "COMPARE LOGICAL (immediate)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x55,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLMH,
   /* .name        = */ "CLMH",
   /* .description = */ "COMPARE LOGICAL CHAR. UNDER MASK (high)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x20,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLMY,
   /* .name        = */ "CLMY",
   /* .description = */ "COMPARE LOGICAL CHAR. UNDER MASK (low)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x21,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLY,
   /* .name        = */ "CLY",
   /* .description = */ "COMPARE LOGICAL (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x55,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CSG,
   /* .name        = */ "CSG",
   /* .description = */ "COMPARE AND SWAP (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x30,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CSY,
   /* .name        = */ "CSY",
   /* .description = */ "COMPARE AND SWAP (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x14,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CVBG,
   /* .name        = */ "CVBG",
   /* .description = */ "CONVERT TO BINARY (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CVBY,
   /* .name        = */ "CVBY",
   /* .description = */ "CONVERT TO BINARY (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CVDG,
   /* .name        = */ "CVDG",
   /* .description = */ "CONVERT TO DECIMAL (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x2E,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CVDY,
   /* .name        = */ "CVDY",
   /* .description = */ "CONVERT TO DECIMAL (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x26,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CY,
   /* .name        = */ "CY",
   /* .description = */ "COMPARE (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x59,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DL,
   /* .name        = */ "DL",
   /* .description = */ "DIVIDE LOGICAL (32 <- 64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x97,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DSG,
   /* .name        = */ "DSG",
   /* .description = */ "DIVIDE SINGLE (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DSGF,
   /* .name        = */ "DSGF",
   /* .description = */ "DIVIDE SINGLE (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x1D,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ICMH,
   /* .name        = */ "ICMH",
   /* .description = */ "INSERT CHARACTERS UNDER MASK (high)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x80,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ICMY,
   /* .name        = */ "ICMY",
   /* .description = */ "INSERT CHARACTERS UNDER MASK (low)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x81,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ICY,
   /* .name        = */ "ICY",
   /* .description = */ "INSERT CHARACTER",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x73,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KIMD,
   /* .name        = */ "KIMD",
   /* .description = */ "COMPUTE INTERMEDIATE MESSAGE DIGEST",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x3E,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KLMD,
   /* .name        = */ "KLMD",
   /* .description = */ "COMPUTE LAST MESSAGE DIGEST",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x3F,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KM,
   /* .name        = */ "KM",
   /* .description = */ "CIPHER MESSAGE",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x2E,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KMAC,
   /* .name        = */ "KMAC",
   /* .description = */ "COMPUTE MESSAGE AUTHENTICATION CODE",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x1E,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KMC,
   /* .name        = */ "KMC",
   /* .description = */ "CIPHER MESSAGE WITH CHAINING",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x2F,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAMY,
   /* .name        = */ "LAMY",
   /* .description = */ "LOAD ACCESS MULTIPLE",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x9A,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAY,
   /* .name        = */ "LAY",
   /* .description = */ "LOAD ADDRESS",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x71,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LB,
   /* .name        = */ "LB",
   /* .description = */ "LOAD BYTE (32 <- 8)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x76,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCTLG,
   /* .name        = */ "LCTLG",
   /* .description = */ "LOAD CONTROL (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x2F,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDY,
   /* .name        = */ "LDY",
   /* .description = */ "LOAD (long)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x65,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LEY,
   /* .name        = */ "LEY",
   /* .description = */ "LOAD (short)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x64,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LG,
   /* .name        = */ "LG",
   /* .description = */ "LOAD (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGB,
   /* .name        = */ "LGB",
   /* .description = */ "LOAD BYTE (64 <- 8)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x77,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGF,
   /* .name        = */ "LGF",
   /* .description = */ "LOAD (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x14,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32To64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGH,
   /* .name        = */ "LGH",
   /* .description = */ "LOAD HALFWORD (64 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x15,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LHY,
   /* .name        = */ "LHY",
   /* .description = */ "LOAD HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x78,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGC,
   /* .name        = */ "LLGC",
   /* .description = */ "LOAD LOGICAL CHARACTER (64 <- 8)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x90,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGF,
   /* .name        = */ "LLGF",
   /* .description = */ "LOAD LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x16,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGH,
   /* .name        = */ "LLGH",
   /* .description = */ "LOAD LOGICAL HALFWORD (64 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x91,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGT,
   /* .name        = */ "LLGT",
   /* .description = */ "LOAD LOGICAL THIRTY ONE BITS (64 <- 31)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x17,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LMG,
   /* .name        = */ "LMG",
   /* .description = */ "LOAD MULTIPLE (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LMH,
   /* .name        = */ "LMH",
   /* .description = */ "LOAD MULTIPLE HIGH (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x96,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LMY,
   /* .name        = */ "LMY",
   /* .description = */ "LOAD MULTIPLE (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x98,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPQ,
   /* .name        = */ "LPQ",
   /* .description = */ "LOAD PAIR FROM QUADWORD (64&64 <- 128)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x8F,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRAG,
   /* .name        = */ "LRAG",
   /* .description = */ "LOAD REAL ADDRESS (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRAY,
   /* .name        = */ "LRAY",
   /* .description = */ "LOAD REAL ADDRESS (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x13,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_LongDispSupported |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRV,
   /* .name        = */ "LRV",
   /* .description = */ "LOAD REVERSED (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x1E,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRVG,
   /* .name        = */ "LRVG",
   /* .description = */ "LOAD REVERSED (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRVH,
   /* .name        = */ "LRVH",
   /* .description = */ "LOAD REVERSED (16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x1F,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LY,
   /* .name        = */ "LY",
   /* .description = */ "LOAD (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x58,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAD,
   /* .name        = */ "MAD",
   /* .description = */ "MULTIPLY AND ADD (long HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x3E,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MADR,
   /* .name        = */ "MADR",
   /* .description = */ "MULTIPLY AND ADD (long HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x3E,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAE,
   /* .name        = */ "MAE",
   /* .description = */ "MULTIPLY AND ADD (short HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x2E,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAER,
   /* .name        = */ "MAER",
   /* .description = */ "MULTIPLY AND ADD (short HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x2E,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MLG,
   /* .name        = */ "MLG",
   /* .description = */ "MULTIPLY LOGICAL (128 <- 64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x86,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSD,
   /* .name        = */ "MSD",
   /* .description = */ "MULTIPLY AND SUBTRACT (long HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x3F,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSDR,
   /* .name        = */ "MSDR",
   /* .description = */ "MULTIPLY AND SUBTRACT (long HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x3F,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSE,
   /* .name        = */ "MSE",
   /* .description = */ "MULTIPLY AND SUBTRACT (short HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x2F,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSER,
   /* .name        = */ "MSER",
   /* .description = */ "MULTIPLY AND SUBTRACT (short HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x2F,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSG,
   /* .name        = */ "MSG",
   /* .description = */ "MULTIPLY SINGLE (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSGF,
   /* .name        = */ "MSGF",
   /* .description = */ "MULTIPLY SINGLE (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x1C,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSY,
   /* .name        = */ "MSY",
   /* .description = */ "MULTIPLY SINGLE (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x51,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCLU,
   /* .name        = */ "MVCLU",
   /* .description = */ "MOVE LONG UNICODE",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x8E,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVIY,
   /* .name        = */ "MVIY",
   /* .description = */ "MOVE (immediate)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x52,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NG,
   /* .name        = */ "NG",
   /* .description = */ "AND (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x80,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NIY,
   /* .name        = */ "NIY",
   /* .description = */ "AND (immediate)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x54,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsStore |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NY,
   /* .name        = */ "NY",
   /* .description = */ "AND (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x54,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OG,
   /* .name        = */ "OG",
   /* .description = */ "OR (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x81,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OIY,
   /* .name        = */ "OIY",
   /* .description = */ "OR (immediate)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x56,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsStore |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OY,
   /* .name        = */ "OY",
   /* .description = */ "OR (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x56,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RLL,
   /* .name        = */ "RLL",
   /* .description = */ "ROTATE LEFT SINGLE LOGICAL (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x1D,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RLLG,
   /* .name        = */ "RLLG",
   /* .description = */ "ROTATE LEFT SINGLE LOGICAL (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x1C,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SG,
   /* .name        = */ "SG",
   /* .description = */ "SUBTRACT (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SGF,
   /* .name        = */ "SGF",
   /* .description = */ "SUBTRACT (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x19,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SHY,
   /* .name        = */ "SHY",
   /* .description = */ "SUBTRACT HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x7B,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLAG,
   /* .name        = */ "SLAG",
   /* .description = */ "SHIFT LEFT SINGLE (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLB,
   /* .name        = */ "SLB",
   /* .description = */ "SUBTRACT LOGICAL WITH BORROW (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x99,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLBG,
   /* .name        = */ "SLBG",
   /* .description = */ "SUBTRACT LOGICAL WITH BORROW (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x89,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLG,
   /* .name        = */ "SLG",
   /* .description = */ "SUBTRACT LOGICAL (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLGF,
   /* .name        = */ "SLGF",
   /* .description = */ "SUBTRACT LOGICAL (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x1B,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLLG,
   /* .name        = */ "SLLG",
   /* .description = */ "SHIFT LEFT SINGLE LOGICAL (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLY,
   /* .name        = */ "SLY",
   /* .description = */ "SUBTRACT LOGICAL (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x5F,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRAG,
   /* .name        = */ "SRAG",
   /* .description = */ "SHIFT RIGHT SINGLE (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRLG,
   /* .name        = */ "SRLG",
   /* .description = */ "SHIFT RIGHT SINGLE LOGICAL (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STAMY,
   /* .name        = */ "STAMY",
   /* .description = */ "STORE ACCESS MULTIPLE",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x9B,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCMH,
   /* .name        = */ "STCMH",
   /* .description = */ "STORE CHARACTERS UNDER MASK (high)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x2C,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCMY,
   /* .name        = */ "STCMY",
   /* .description = */ "STORE CHARACTERS UNDER MASK (low)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x2D,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCTG,
   /* .name        = */ "STCTG",
   /* .description = */ "STORE CONTROL (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x25,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCY,
   /* .name        = */ "STCY",
   /* .description = */ "STORE CHARACTER",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x72,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STDY,
   /* .name        = */ "STDY",
   /* .description = */ "STORE (long)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x67,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STEY,
   /* .name        = */ "STEY",
   /* .description = */ "STORE (short)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x66,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STG,
   /* .name        = */ "STG",
   /* .description = */ "STORE (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x24,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STHY,
   /* .name        = */ "STHY",
   /* .description = */ "STORE HALFWORD (16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x70,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STMG,
   /* .name        = */ "STMG",
   /* .description = */ "STORE MULTIPLE (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x24,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STMH,
   /* .name        = */ "STMH",
   /* .description = */ "STORE MULTIPLE HIGH (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x26,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STMY,
   /* .name        = */ "STMY",
   /* .description = */ "STORE MULTIPLE (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x90,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STPQ,
   /* .name        = */ "STPQ",
   /* .description = */ "STORE PAIR TO QUADWORD",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x8E,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STRV,
   /* .name        = */ "STRV",
   /* .description = */ "STORE REVERSED (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x3E,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STRVG,
   /* .name        = */ "STRVG",
   /* .description = */ "STORE REVERSED (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x2F,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STRVH,
   /* .name        = */ "STRVH",
   /* .description = */ "STORE REVERSED (16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x3F,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STY,
   /* .name        = */ "STY",
   /* .description = */ "STORE (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x50,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SY,
   /* .name        = */ "SY",
   /* .description = */ "SUBTRACT (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x5B,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TMY,
   /* .name        = */ "TMY",
   /* .description = */ "TEST UNDER MASK",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x51,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XG,
   /* .name        = */ "XG",
   /* .description = */ "EXCLUSIVE OR (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x82,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XIY,
   /* .name        = */ "XIY",
   /* .description = */ "EXCLUSIVE OR (immediate)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x57,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XY,
   /* .name        = */ "XY",
   /* .description = */ "EXCLUSIVE OR (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x57,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z990,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ADTR,
   /* .name        = */ "ADTR",
   /* .description = */ "ADD (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD2,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AFI,
   /* .name        = */ "AFI",
   /* .description = */ "ADD IMMEDIATE (32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGFI,
   /* .name        = */ "AGFI",
   /* .description = */ "ADD IMMEDIATE (64 <- 32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALFI,
   /* .name        = */ "ALFI",
   /* .description = */ "ADD LOGICAL IMMEDIATE (32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALGFI,
   /* .name        = */ "ALGFI",
   /* .description = */ "ADD LOGICAL IMMEDIATE (64 <- 32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AXTR,
   /* .name        = */ "AXTR",
   /* .description = */ "ADD (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xDA,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDGTR,
   /* .name        = */ "CDGTR",
   /* .description = */ "CONVERT FROM FIXED (64 to long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF1,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDSTR,
   /* .name        = */ "CDSTR",
   /* .description = */ "CONVERT FROM SIGNED PACKED (64 to long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF3,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDTR,
   /* .name        = */ "CDTR",
   /* .description = */ "COMPARE (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE4,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDUTR,
   /* .name        = */ "CDUTR",
   /* .description = */ "CONVERT FROM UNSIGNED PACKED (64 to long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF2,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEDTR,
   /* .name        = */ "CEDTR",
   /* .description = */ "COMPARE BIASED EXPONENT (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF4,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CEXTR,
   /* .name        = */ "CEXTR",
   /* .description = */ "COMPARE BIASED EXPONENT (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xFC,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_UsesRegPairForSource
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CFI,
   /* .name        = */ "CFI",
   /* .description = */ "COMPARE IMMEDIATE (32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsExtendedImmediate
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGDTR,
   /* .name        = */ "CGDTR",
   /* .description = */ "CONVERT TO FIXED (long DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE1,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGFI,
   /* .name        = */ "CGFI",
   /* .description = */ "COMPARE IMMEDIATE (64 <- 32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsExtendedImmediate
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGXTR,
   /* .name        = */ "CGXTR",
   /* .description = */ "CONVERT TO FIXED (extended DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE9,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLFI,
   /* .name        = */ "CLFI",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE (32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGFI,
   /* .name        = */ "CLGFI",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE (64 <- 32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CPSDR,
   /* .name        = */ "CPSDR",
   /* .description = */ "COPY SIGN (long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x72,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_DoubleFP
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CSDTR,
   /* .name        = */ "CSDTR",
   /* .description = */ "CONVERT TO SIGNED PACKED (long DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE3,
   /* .format      = */ RRFd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CSXTR,
   /* .name        = */ "CSXTR",
   /* .description = */ "CONVERT TO SIGNED PACKED (extended DFP to 128)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xEB,
   /* .format      = */ RRFd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CU14,
   /* .name        = */ "CU14",
   /* .description = */ "CONVERT UTF-8 TO UTF-32",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xB0,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CU24,
   /* .name        = */ "CU24",
   /* .description = */ "CONVERT UTF-16 TO UTF-32",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xB1,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CU41,
   /* .name        = */ "CU41",
   /* .description = */ "CONVERT UTF-32 TO UTF-8",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xB2,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CU42,
   /* .name        = */ "CU42",
   /* .description = */ "CONVERT UTF-32 TO UTF-16",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xB3,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CUDTR,
   /* .name        = */ "CUDTR",
   /* .description = */ "CONVERT TO UNSIGNED PACKED (long DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE2,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CUTFU,
   /* .name        = */ "CUTFU",
   /* .description = */ "CONVERT UTF-8 TO UNICODE",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xA7,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CUUTF,
   /* .name        = */ "CUUTF",
   /* .description = */ "CONVERT UNICODE TO UTF-8",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xA6,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CUXTR,
   /* .name        = */ "CUXTR",
   /* .description = */ "CONVERT TO UNSIGNED PACKED (extended DFP to 128)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xEA,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXGTR,
   /* .name        = */ "CXGTR",
   /* .description = */ "CONVERT FROM FIXED (64 to extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF9,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXSTR,
   /* .name        = */ "CXSTR",
   /* .description = */ "CONVERT FROM SIGNED PACKED (128 to extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xFB,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXTR,
   /* .name        = */ "CXTR",
   /* .description = */ "COMPARE (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xEC,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXUTR,
   /* .name        = */ "CXUTR",
   /* .description = */ "CONVERT FROM UNSIGNED PACKED (128 to ext. DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xFA,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DDTR,
   /* .name        = */ "DDTR",
   /* .description = */ "DIVIDE (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD1,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsFPC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::DXTR,
   /* .name        = */ "DXTR",
   /* .description = */ "DIVIDE (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD9,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsFPC |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EEDTR,
   /* .name        = */ "EEDTR",
   /* .description = */ "EXTRACT BIASED EXPONENT (long DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE5,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EEXTR,
   /* .name        = */ "EEXTR",
   /* .description = */ "EXTRACT BIASED EXPONENT (extended DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xED,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ESDTR,
   /* .name        = */ "ESDTR",
   /* .description = */ "EXTRACT SIGNIFICANCE (long DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE7,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ESXTR,
   /* .name        = */ "ESXTR",
   /* .description = */ "EXTRACT SIGNIFICANCE (extended DFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xEF,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIDTR,
   /* .name        = */ "FIDTR",
   /* .description = */ "LOAD FP INTEGER (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD7,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FIXTR,
   /* .name        = */ "FIXTR",
   /* .description = */ "LOAD FP INTEGER (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xDF,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::FLOGR,
   /* .name        = */ "FLOGR",
   /* .description = */ "FIND LEFTMOST ONE",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x83,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IEDTR,
   /* .name        = */ "IEDTR",
   /* .description = */ "INSERT BIASED EXPONENT (64 to long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF6,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IEXTR,
   /* .name        = */ "IEXTR",
   /* .description = */ "INSERT BIASED EXPONENT (64 to extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xFE,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IIHF,
   /* .name        = */ "IIHF",
   /* .description = */ "INSERT IMMEDIATE (high)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::IILF,
   /* .name        = */ "IILF",
   /* .description = */ "INSERT IMMEDIATE (low)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KDTR,
   /* .name        = */ "KDTR",
   /* .description = */ "COMPARE AND SIGNAL (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE0,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KXTR,
   /* .name        = */ "KXTR",
   /* .description = */ "COMPARE AND SIGNAL (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xE8,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LBR,
   /* .name        = */ "LBR",
   /* .description = */ "LOAD BYTE (32 <- 8)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x26,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCDFR,
   /* .name        = */ "LCDFR",
   /* .description = */ "LOAD COMPLEMENT (long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x73,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDETR,
   /* .name        = */ "LDETR",
   /* .description = */ "LOAD LENGTHENED (short to long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD4,
   /* .format      = */ RRFd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDGR,
   /* .name        = */ "LDGR",
   /* .description = */ "LOAD FPR FROM GR (64 to long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xC1,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LDXTR,
   /* .name        = */ "LDXTR",
   /* .description = */ "LOAD ROUNDED (extended to long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xDD,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LEDTR,
   /* .name        = */ "LEDTR",
   /* .description = */ "LOAD ROUNDED (long to short DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD5,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGBR,
   /* .name        = */ "LGBR",
   /* .description = */ "LOAD BYTE (64 <- 8)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGDR,
   /* .name        = */ "LGDR",
   /* .description = */ "LOAD GR FROM FPR (long to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xCD,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_DoubleFP |
                        S390OpProp_SingleFP |
                        S390OpProp_DoubleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGFI,
   /* .name        = */ "LGFI",
   /* .description = */ "LOAD IMMEDIATE (64 <- 32)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGHR,
   /* .name        = */ "LGHR",
   /* .description = */ "LOAD HALFWORD (64 <- 16)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LHR,
   /* .name        = */ "LHR",
   /* .description = */ "LOAD HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x27,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLC,
   /* .name        = */ "LLC",
   /* .description = */ "LOAD LOGICAL CHARACTER (32 <- 8)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x94,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLCR,
   /* .name        = */ "LLCR",
   /* .description = */ "LOAD LOGICAL CHARACTER (32 <- 8)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x94,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGCR,
   /* .name        = */ "LLGCR",
   /* .description = */ "LOAD LOGICAL CHARACTER (64 <- 8)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x84,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGHR,
   /* .name        = */ "LLGHR",
   /* .description = */ "LOAD LOGICAL HALFWORD (64 <- 16)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x85,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLH,
   /* .name        = */ "LLH",
   /* .description = */ "LOAD LOGICAL HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x95,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLHR,
   /* .name        = */ "LLHR",
   /* .description = */ "LOAD LOGICAL HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x95,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLIHF,
   /* .name        = */ "LLIHF",
   /* .description = */ "LOAD LOGICAL IMMEDIATE (high)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLILF,
   /* .name        = */ "LLILF",
   /* .description = */ "LOAD LOGICAL IMMEDIATE (low)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LNDFR,
   /* .name        = */ "LNDFR",
   /* .description = */ "LOAD NEGATIVE (long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x71,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPDFR,
   /* .name        = */ "LPDFR",
   /* .description = */ "LOAD POSITIVE (long)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x70,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LT,
   /* .name        = */ "LT",
   /* .description = */ "LOAD AND TEST (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x12,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTDTR,
   /* .name        = */ "LTDTR",
   /* .description = */ "LOAD AND TEST (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD6,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTG,
   /* .name        = */ "LTG",
   /* .description = */ "LOAD AND TEST (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTXTR,
   /* .name        = */ "LTXTR",
   /* .description = */ "LOAD AND TEST (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xDE,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LXDTR,
   /* .name        = */ "LXDTR",
   /* .description = */ "LOAD LENGTHENED (long to extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xDC,
   /* .format      = */ RRFd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAY,
   /* .name        = */ "MAY",
   /* .description = */ "MULTIPLY & ADD UNNORMALIZED (long to ext. HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x3A,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAYH,
   /* .name        = */ "MAYH",
   /* .description = */ "MULTIPLY AND ADD UNNRM. (long to ext. high HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x3C,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAYHR,
   /* .name        = */ "MAYHR",
   /* .description = */ "MULTIPLY AND ADD UNNRM. (long to ext. high HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x3C,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAYL,
   /* .name        = */ "MAYL",
   /* .description = */ "MULTIPLY AND ADD UNNRM. (long to ext. low HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x38,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAYLR,
   /* .name        = */ "MAYLR",
   /* .description = */ "MULTIPLY AND ADD UNNRM. (long to ext. low HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x38,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MAYR,
   /* .name        = */ "MAYR",
   /* .description = */ "MULTIPLY & ADD UNNORMALIZED (long to ext. HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x3A,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MDTR,
   /* .name        = */ "MDTR",
   /* .description = */ "MULTIPLY (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD0,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MXTR,
   /* .name        = */ "MXTR",
   /* .description = */ "MULTIPLY (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD8,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MY,
   /* .name        = */ "MY",
   /* .description = */ "MULTIPLY UNNORMALIZED (long to ext. HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x3B,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MYH,
   /* .name        = */ "MYH",
   /* .description = */ "MULTIPLY UNNORM. (long to ext. high HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x3D,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MYHR,
   /* .name        = */ "MYHR",
   /* .description = */ "MULTIPLY UNNORM. (long to ext. high HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x3D,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MYL,
   /* .name        = */ "MYL",
   /* .description = */ "MULTIPLY UNNORM. (long to ext. low HFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x39,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MYLR,
   /* .name        = */ "MYLR",
   /* .description = */ "MULTIPLY UNNORM. (long to ext. low HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x39,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MYR,
   /* .name        = */ "MYR",
   /* .description = */ "MULTIPLY UNNORMALIZED (long to ext. HFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x3B,
   /* .format      = */ RRD_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NIHF,
   /* .name        = */ "NIHF",
   /* .description = */ "AND IMMEDIATE (high)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NILF,
   /* .name        = */ "NILF",
   /* .description = */ "AND IMMEDIATE (low)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OIHF,
   /* .name        = */ "OIHF",
   /* .description = */ "OR IMMEDIATE (high)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OILF,
   /* .name        = */ "OILF",
   /* .description = */ "OR IMMEDIATE (low)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PFPO,
   /* .name        = */ "PFPO",
   /* .description = */ "PERFORM FLOATING-POINT OPERATION",
   /* .opcode[0]   = */ 0x01,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ E_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_ImplicitlyUsesGPR0
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::QADTR,
   /* .name        = */ "QADTR",
   /* .description = */ "QUANTIZE (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF5,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsFPC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::QAXTR,
   /* .name        = */ "QAXTR",
   /* .description = */ "QUANTIZE (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xFD,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsFPC |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RRDTR,
   /* .name        = */ "RRDTR",
   /* .description = */ "REROUND (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xF7,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RRXTR,
   /* .name        = */ "RRXTR",
   /* .description = */ "REROUND (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xFF,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SDTR,
   /* .name        = */ "SDTR",
   /* .description = */ "SUBTRACT (long DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xD3,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SFASR,
   /* .name        = */ "SFASR",
   /* .description = */ "SET FPC AND SIGNAL",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x85,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsFPC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLDT,
   /* .name        = */ "SLDT",
   /* .description = */ "SHIFT SIGNIFICAND LEFT (long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x40,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLFI,
   /* .name        = */ "SLFI",
   /* .description = */ "SUBTRACT LOGICAL IMMEDIATE (32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLGFI,
   /* .name        = */ "SLGFI",
   /* .description = */ "SUBTRACT LOGICAL IMMEDIATE (64 <- 32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLXT,
   /* .name        = */ "SLXT",
   /* .description = */ "SHIFT SIGNIFICAND LEFT (extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x48,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRDT,
   /* .name        = */ "SRDT",
   /* .description = */ "SHIFT SIGNIFICAND RIGHT (long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x41,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRNMT,
   /* .name        = */ "SRNMT",
   /* .description = */ "SET DFP ROUNDING MODE",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xB9,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRSTU,
   /* .name        = */ "SRSTU",
   /* .description = */ "SEARCH STRING UNICODE",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xBE,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRXT,
   /* .name        = */ "SRXT",
   /* .description = */ "SHIFT SIGNIFICAND RIGHT (extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x49,
   /* .format      = */ RXF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesTarget |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SSKE,
   /* .name        = */ "SSKE",
   /* .description = */ "SET STORAGE KEY EXTENDED",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x2B,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCKF,
   /* .name        = */ "STCKF",
   /* .description = */ "STORE CLOCK FAST",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0x7C,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SXTR,
   /* .name        = */ "SXTR",
   /* .description = */ "SUBTRACT (extended DFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xDB,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TDCDT,
   /* .name        = */ "TDCDT",
   /* .description = */ "TEST DATA CLASS (long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x54,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TDCET,
   /* .name        = */ "TDCET",
   /* .description = */ "TEST DATA CLASS (short DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x50,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TDCXT,
   /* .name        = */ "TDCXT",
   /* .description = */ "TEST DATA CLASS (extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x58,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TDGDT,
   /* .name        = */ "TDGDT",
   /* .description = */ "TEST DATA GROUP (long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x55,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TDGET,
   /* .name        = */ "TDGET",
   /* .description = */ "TEST DATA GROUP (short DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x51,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TDGXT,
   /* .name        = */ "TDGXT",
   /* .description = */ "TEST DATA GROUP (extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0x59,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TROO,
   /* .name        = */ "TROO",
   /* .description = */ "TRANSLATE ONE TO ONE",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x93,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TROT,
   /* .name        = */ "TROT",
   /* .description = */ "TRANSLATE ONE TO TWO",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x92,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TRTO,
   /* .name        = */ "TRTO",
   /* .description = */ "TRANSLATE TWO TO ONE",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x91,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TRTR,
   /* .name        = */ "TRTR",
   /* .description = */ "TRANSLATE AND TEST REVERSE",
   /* .opcode[0]   = */ 0xD0,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR2 |
                        S390OpProp_ImplicitlySetsGPR2 |
                        S390OpProp_ImplicitlySetsGPR1 |
                        S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TRTT,
   /* .name        = */ "TRTT",
   /* .description = */ "TRANSLATE TWO TO TWO",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x90,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XIHF,
   /* .name        = */ "XIHF",
   /* .description = */ "EXCLUSIVE OR IMMEDIATE (high)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XILF,
   /* .name        = */ "XILF",
   /* .description = */ "EXCLUSIVE OR IMMEDIATE (low)",
   /* .opcode[0]   = */ 0xC0,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z9,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGSI,
   /* .name        = */ "AGSI",
   /* .description = */ "ADD IMMEDIATE (64 <- 8)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x7A,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALGSI,
   /* .name        = */ "ALGSI",
   /* .description = */ "ADD LOGICAL WITH SIGNED IMMEDIATE (64 <- 8)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x7E,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALSI,
   /* .name        = */ "ALSI",
   /* .description = */ "ADD LOGICAL WITH SIGNED IMMEDIATE (32 <- 8)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x6E,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ASI,
   /* .name        = */ "ASI",
   /* .description = */ "ADD IMMEDIATE (32 <- 8)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x6A,
   /* .format      = */ SIY_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGFRL,
   /* .name        = */ "CGFRL",
   /* .description = */ "COMPARE RELATIVE LONG (64 <- 32)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32To64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGHRL,
   /* .name        = */ "CGHRL",
   /* .description = */ "COMPARE HALFWORD RELATIVE LONG (64 <- 16)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGHSI,
   /* .name        = */ "CGHSI",
   /* .description = */ "COMPARE HALFWORD IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x58,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGIB,
   /* .name        = */ "CGIB",
   /* .description = */ "COMPARE IMMEDIATE AND BRANCH (64 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xFC,
   /* .format      = */ RIS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGIJ,
   /* .name        = */ "CGIJ",
   /* .description = */ "COMPARE IMMEDIATE AND BRANCH RELATIVE (64 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x7C,
   /* .format      = */ RIEc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGIT,
   /* .name        = */ "CGIT",
   /* .description = */ "COMPARE IMMEDIATE AND TRAP (64 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x70,
   /* .format      = */ RIEa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGRB,
   /* .name        = */ "CGRB",
   /* .description = */ "COMPARE AND BRANCH (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xE4,
   /* .format      = */ RRS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGRJ,
   /* .name        = */ "CGRJ",
   /* .description = */ "COMPARE AND BRANCH RELATIVE (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x64,
   /* .format      = */ RIEb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGRL,
   /* .name        = */ "CGRL",
   /* .description = */ "COMPARE RELATIVE LONG (64)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CGRT,
   /* .name        = */ "CGRT",
   /* .description = */ "COMPARE AND TRAP (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x60,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHHSI,
   /* .name        = */ "CHHSI",
   /* .description = */ "COMPARE HALFWORD IMMEDIATE (16 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x54,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHRL,
   /* .name        = */ "CHRL",
   /* .description = */ "COMPARE HALFWORD RELATIVE LONG (32 <- 16)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHSI,
   /* .name        = */ "CHSI",
   /* .description = */ "COMPARE HALFWORD IMMEDIATE (32 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x5C,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CIB,
   /* .name        = */ "CIB",
   /* .description = */ "COMPARE IMMEDIATE AND BRANCH (32 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xFE,
   /* .format      = */ RIS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CIJ,
   /* .name        = */ "CIJ",
   /* .description = */ "COMPARE IMMEDIATE AND BRANCH RELATIVE (32 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x7E,
   /* .format      = */ RIEc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CIT,
   /* .name        = */ "CIT",
   /* .description = */ "COMPARE IMMEDIATE AND TRAP (32 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x72,
   /* .format      = */ RIEa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLFHSI,
   /* .name        = */ "CLFHSI",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE (32 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x5D,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLFIT,
   /* .name        = */ "CLFIT",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE AND TRAP (32 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x73,
   /* .format      = */ RIEa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGFRL,
   /* .name        = */ "CLGFRL",
   /* .description = */ "COMPARE LOGICAL RELATIVE LONG (64 <- 32)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGHRL,
   /* .name        = */ "CLGHRL",
   /* .description = */ "COMPARE LOGICAL RELATIVE LONG (64 <- 16)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGHSI,
   /* .name        = */ "CLGHSI",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x59,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGIB,
   /* .name        = */ "CLGIB",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE AND BRANCH (64 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xFD,
   /* .format      = */ RIS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGIJ,
   /* .name        = */ "CLGIJ",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE AND BRANCH RELATIVE (64 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x7D,
   /* .format      = */ RIEc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGIT,
   /* .name        = */ "CLGIT",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE AND TRAP (64 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x71,
   /* .format      = */ RIEa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGRB,
   /* .name        = */ "CLGRB",
   /* .description = */ "COMPARE LOGICAL AND BRANCH (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xE5,
   /* .format      = */ RRS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGRJ,
   /* .name        = */ "CLGRJ",
   /* .description = */ "COMPARE LOGICAL AND BRANCH RELATIVE (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x65,
   /* .format      = */ RIEb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGRL,
   /* .name        = */ "CLGRL",
   /* .description = */ "COMPARE LOGICAL RELATIVE LONG (64)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGRT,
   /* .name        = */ "CLGRT",
   /* .description = */ "COMPARE LOGICAL AND TRAP (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x61,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLHHSI,
   /* .name        = */ "CLHHSI",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE (16 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x55,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLHRL,
   /* .name        = */ "CLHRL",
   /* .description = */ "COMPARE LOGICAL RELATIVE LONG (32 <- 16)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLIB,
   /* .name        = */ "CLIB",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE AND BRANCH (32 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xFF,
   /* .format      = */ RIS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLIJ,
   /* .name        = */ "CLIJ",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE AND BRANCH RELATIVE (32 <- 8)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x7F,
   /* .format      = */ RIEc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLRB,
   /* .name        = */ "CLRB",
   /* .description = */ "COMPARE LOGICAL AND BRANCH (32)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xF7,
   /* .format      = */ RRS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLRJ,
   /* .name        = */ "CLRJ",
   /* .description = */ "COMPARE LOGICAL AND BRANCH RELATIVE (32)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x77,
   /* .format      = */ RIEb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLRL,
   /* .name        = */ "CLRL",
   /* .description = */ "COMPARE LOGICAL RELATIVE LONG (32)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLRT,
   /* .name        = */ "CLRT",
   /* .description = */ "COMPARE LOGICAL AND TRAP (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x73,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CRB,
   /* .name        = */ "CRB",
   /* .description = */ "COMPARE AND BRANCH (32)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xF6,
   /* .format      = */ RRS_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CRJ,
   /* .name        = */ "CRJ",
   /* .description = */ "COMPARE AND BRANCH RELATIVE (32)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x76,
   /* .format      = */ RIEb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |

                        // At binary encoding we may determine the branch distance is too large at which point we have
                        // no choice but to generate a compare and branch instruction pair
                        S390OpProp_SetsCC |
                        S390OpProp_BranchOp
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CRL,
   /* .name        = */ "CRL",
   /* .description = */ "COMPARE RELATIVE LONG (32)",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CRT,
   /* .name        = */ "CRT",
   /* .description = */ "COMPARE AND TRAP (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x72,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ECAG,
   /* .name        = */ "ECAG",
   /* .description = */ "EXTRACT CPU ATTRIBUTE",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x4C,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::EXRL,
   /* .name        = */ "EXRL",
   /* .description = */ "EXECUTE RELATIVE LONG",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAEY,
   /* .name        = */ "LAEY",
   /* .description = */ "LOAD ADDRESS EXTENDED",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x75,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGFRL,
   /* .name        = */ "LGFRL",
   /* .description = */ "LOAD RELATIVE LONG (64 <- 32)",
   /* .opcode[0]   = */ 0xC4,
   /* .opcode[1]   = */ 0x0C,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGRL,
   /* .name        = */ "LGRL",
   /* .description = */ "LOAD RELATIVE LONG (64)",
   /* .opcode[0]   = */ 0xC4,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGFRL,
   /* .name        = */ "LLGFRL",
   /* .description = */ "LOAD LOGICAL RELATIVE LONG (64 <- 32)",
   /* .opcode[0]   = */ 0xC4,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LRL,
   /* .name        = */ "LRL",
   /* .description = */ "LOAD RELATIVE LONG (32)",
   /* .opcode[0]   = */ 0xC4,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LTGF,
   /* .name        = */ "LTGF",
   /* .description = */ "LOAD AND TEST (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x32,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is32To64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MC,
   /* .name        = */ "MC",
   /* .description = */ "MONITOR CALL",
   /* .opcode[0]   = */ 0xAF,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MFY,
   /* .name        = */ "MFY",
   /* .description = */ "MULTIPLY (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x5C,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MHY,
   /* .name        = */ "MHY",
   /* .description = */ "MULTIPLY HALFWORD (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x7C,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSFI,
   /* .name        = */ "MSFI",
   /* .description = */ "MULTIPLY SINGLE IMMEDIATE (32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSGFI,
   /* .name        = */ "MSGFI",
   /* .description = */ "MULTIPLY SINGLE IMMEDIATE (64 <- 32)",
   /* .opcode[0]   = */ 0xC2,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVGHI,
   /* .name        = */ "MVGHI",
   /* .description = */ "MOVE (64 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x48,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVHHI,
   /* .name        = */ "MVHHI",
   /* .description = */ "MOVE (16 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x44,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVHI,
   /* .name        = */ "MVHI",
   /* .description = */ "MOVE (32 <- 16)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x4C,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PFD,
   /* .name        = */ "PFD",
   /* .description = */ "PREFETCH DATA",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x36,
   /* .format      = */ RXYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PFDRL,
   /* .name        = */ "PFDRL",
   /* .description = */ "PREFETCH DATA RELATIVE LONG",
   /* .opcode[0]   = */ 0xC6,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ RILc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RISBG,
   /* .name        = */ "RISBG",
   /* .description = */ "ROTATE THEN INSERT SELECTED BITS (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x55,
   /* .format      = */ RIEf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RNSBG,
   /* .name        = */ "RNSBG",
   /* .description = */ "ROTATE THEN AND SELECTED BITS (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x54,
   /* .format      = */ RIEf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ROSBG,
   /* .name        = */ "ROSBG",
   /* .description = */ "ROTATE THEN OR SELECTED BITS (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x56,
   /* .format      = */ RIEf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RXSBG,
   /* .name        = */ "RXSBG",
   /* .description = */ "ROTATE THEN EXCLUSIVE OR SELECT. BITS (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x57,
   /* .format      = */ RIEf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STGRL,
   /* .name        = */ "STGRL",
   /* .description = */ "STORE RELATIVE LONG (64)",
   /* .opcode[0]   = */ 0xC4,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is64Bit
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STRL,
   /* .name        = */ "STRL",
   /* .description = */ "STORE RELATIVE LONG (32)",
   /* .opcode[0]   = */ 0xC4,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TRTE,
   /* .name        = */ "TRTE",
   /* .description = */ "TRANSLATE AND TEST EXTENDED",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xBF,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TRTRE,
   /* .name        = */ "TRTRE",
   /* .description = */ "TRANSLATE AND TEST REVERSE EXTENDED",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xBD,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z10,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_ImplicitlyUsesGPR1 |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGHIK,
   /* .name        = */ "AGHIK",
   /* .description = */ "ADD IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xD9,
   /* .format      = */ RIEd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGRK,
   /* .name        = */ "AGRK",
   /* .description = */ "ADD (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE8,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AHHHR,
   /* .name        = */ "AHHHR",
   /* .description = */ "ADD HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xC8,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_Src2HW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AHHLR,
   /* .name        = */ "AHHLR",
   /* .description = */ "ADD HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xD8,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AHIK,
   /* .name        = */ "AHIK",
   /* .description = */ "ADD IMMEDIATE (32 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xD8,
   /* .format      = */ RIEd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AIH,
   /* .name        = */ "AIH",
   /* .description = */ "ADD IMMEDIATE HIGH (32)",
   /* .opcode[0]   = */ 0xCC,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALGHSIK,
   /* .name        = */ "ALGHSIK",
   /* .description = */ "ADD LOGICAL WITH SIGNED IMMEDIATE (64 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xDB,
   /* .format      = */ RIEd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALGRK,
   /* .name        = */ "ALGRK",
   /* .description = */ "ADD LOGICAL (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xEA,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALHHHR,
   /* .name        = */ "ALHHHR",
   /* .description = */ "ADD LOGICAL HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xCA,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_Src2HW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALHHLR,
   /* .name        = */ "ALHHLR",
   /* .description = */ "ADD LOGICAL HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xDA,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALHSIK,
   /* .name        = */ "ALHSIK",
   /* .description = */ "ADD LOGICAL WITH SIGNED IMMEDIATE (32 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0xDA,
   /* .format      = */ RIEd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALRK,
   /* .name        = */ "ALRK",
   /* .description = */ "ADD LOGICAL (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xFA,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALSIH,
   /* .name        = */ "ALSIH",
   /* .description = */ "ADD LOGICAL WITH SIGNED IMMEDIATE HIGH (32)",
   /* .opcode[0]   = */ 0xCC,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ALSIHN,
   /* .name        = */ "ALSIHN",
   /* .description = */ "ADD LOGICAL WITH SIGNED IMMEDIATE HIGH (32)",
   /* .opcode[0]   = */ 0xCC,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ARK,
   /* .name        = */ "ARK",
   /* .description = */ "ADD (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF8,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BRCTH,
   /* .name        = */ "BRCTH",
   /* .description = */ "BRANCH RELATIVE ON COUNT HIGH (32)",
   /* .opcode[0]   = */ 0xCC,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ RILb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_BranchOp |
                        S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDLFBR,
   /* .name        = */ "CDLFBR",
   /* .description = */ "CONVERT FROM LOGICAL (32 to long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x91,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDLGBR,
   /* .name        = */ "CDLGBR",
   /* .description = */ "CONVERT FROM LOGICAL (64 to long BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA1,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CELFBR,
   /* .name        = */ "CELFBR",
   /* .description = */ "CONVERT FROM LOGICAL (32 to short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x90,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SingleFP |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CELGBR,
   /* .name        = */ "CELGBR",
   /* .description = */ "CONVERT FROM LOGICAL (64 to short BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA0,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHF,
   /* .name        = */ "CHF",
   /* .description = */ "COMPARE HIGH (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xCD,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHHR,
   /* .name        = */ "CHHR",
   /* .description = */ "COMPARE HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xCD,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CHLR,
   /* .name        = */ "CHLR",
   /* .description = */ "COMPARE HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xDD,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CIH,
   /* .name        = */ "CIH",
   /* .description = */ "COMPARE IMMEDIATE HIGH (32)",
   /* .opcode[0]   = */ 0xCC,
   /* .opcode[1]   = */ 0x0D,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLFDBR,
   /* .name        = */ "CLFDBR",
   /* .description = */ "CONVERT TO LOGICAL (long BFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x9D,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLFEBR,
   /* .name        = */ "CLFEBR",
   /* .description = */ "CONVERT TO LOGICAL (short BFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x9C,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLFXBR,
   /* .name        = */ "CLFXBR",
   /* .description = */ "CONVERT TO LOGICAL (extended BFP to 32)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x9E,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGDBR,
   /* .name        = */ "CLGDBR",
   /* .description = */ "CONVERT TO LOGICAL (long BFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xAD,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGEBR,
   /* .name        = */ "CLGEBR",
   /* .description = */ "CONVERT TO LOGICAL (short BFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xAC,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SingleFP |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGXBR,
   /* .name        = */ "CLGXBR",
   /* .description = */ "CONVERT TO LOGICAL (extended BFP to 64)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xAE,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLHF,
   /* .name        = */ "CLHF",
   /* .description = */ "COMPARE LOGICAL HIGH (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xCF,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLHHR,
   /* .name        = */ "CLHHR",
   /* .description = */ "COMPARE LOGICAL HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xCF,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLHLR,
   /* .name        = */ "CLHLR",
   /* .description = */ "COMPARE LOGICAL HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xDF,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLIH,
   /* .name        = */ "CLIH",
   /* .description = */ "COMPARE LOGICAL IMMEDIATE HIGH (32)",
   /* .opcode[0]   = */ 0xCC,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ RILa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsCompare |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsCompareFlag |
                        S390OpProp_IsExtendedImmediate |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXLFBR,
   /* .name        = */ "CXLFBR",
   /* .description = */ "CONVERT FROM LOGICAL (32 to extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0x92,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXLGBR,
   /* .name        = */ "CXLGBR",
   /* .description = */ "CONVERT FROM LOGICAL (64 to extended BFP)",
   /* .opcode[0]   = */ 0xB3,
   /* .opcode[1]   = */ 0xA2,
   /* .format      = */ RRFe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KMCTR,
   /* .name        = */ "KMCTR",
   /* .description = */ "CIPHER MESSAGE WITH COUNTER",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x2D,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KMF,
   /* .name        = */ "KMF",
   /* .description = */ "CIPHER MESSAGE WITH CIPHER FEEDBACK",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x2A,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KMO,
   /* .name        = */ "KMO",
   /* .description = */ "CIPHER MESSAGE WITH OUTPUT FEEDBACK",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x2B,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesRegPairForSource |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAA,
   /* .name        = */ "LAA",
   /* .description = */ "LOAD AND ADD (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xF8,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAAG,
   /* .name        = */ "LAAG",
   /* .description = */ "LOAD AND ADD (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE8,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAAL,
   /* .name        = */ "LAAL",
   /* .description = */ "LOAD AND ADD LOGICAL (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xFA,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAALG,
   /* .name        = */ "LAALG",
   /* .description = */ "LOAD AND ADD LOGICAL (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xEA,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAN,
   /* .name        = */ "LAN",
   /* .description = */ "LOAD AND AND (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xF4,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LANG,
   /* .name        = */ "LANG",
   /* .description = */ "LOAD AND AND (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE4,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAO,
   /* .name        = */ "LAO",
   /* .description = */ "LOAD AND OR (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xF6,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAOG,
   /* .name        = */ "LAOG",
   /* .description = */ "LOAD AND OR (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE6,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAX,
   /* .name        = */ "LAX",
   /* .description = */ "LOAD AND EXCLUSIVE OR (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xF7,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAXG,
   /* .name        = */ "LAXG",
   /* .description = */ "LOAD AND EXCLUSIVE OR (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE7,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LBH,
   /* .name        = */ "LBH",
   /* .description = */ "LOAD BYTE HIGH (32 <- 8)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xC0,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LFH,
   /* .name        = */ "LFH",
   /* .description = */ "LOAD HIGH (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xCA,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LHH,
   /* .name        = */ "LHH",
   /* .description = */ "LOAD HALFWORD HIGH (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xC4,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLCH,
   /* .name        = */ "LLCH",
   /* .description = */ "LOAD LOGICAL CHARACTER HIGH (32 <- 8)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xC2,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLHH,
   /* .name        = */ "LLHH",
   /* .description = */ "LOAD LOGICAL HALFWORD HIGH (32 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xC6,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOC,
   /* .name        = */ "LOC",
   /* .description = */ "LOAD ON CONDITION (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xF2,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_LongDispSupported |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCG,
   /* .name        = */ "LOCG",
   /* .description = */ "LOAD ON CONDITION (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE2,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_LongDispSupported |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCGR,
   /* .name        = */ "LOCGR",
   /* .description = */ "LOAD ON CONDITION (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE2,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCR,
   /* .name        = */ "LOCR",
   /* .description = */ "LOAD ON CONDITION (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF2,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPD,
   /* .name        = */ "LPD",
   /* .description = */ "LOAD PAIR DISJOINT (32)",
   /* .opcode[0]   = */ 0xC8,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ SSF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LPDG,
   /* .name        = */ "LPDG",
   /* .description = */ "LOAD PAIR DISJOINT (64)",
   /* .opcode[0]   = */ 0xC8,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ SSF_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_HasTwoMemoryReferences |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NGRK,
   /* .name        = */ "NGRK",
   /* .description = */ "AND (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE4,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NRK,
   /* .name        = */ "NRK",
   /* .description = */ "AND (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF4,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OGRK,
   /* .name        = */ "OGRK",
   /* .description = */ "OR (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE6,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ORK,
   /* .name        = */ "ORK",
   /* .description = */ "OR (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF6,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::POPCNT,
   /* .name        = */ "POPCNT",
   /* .description = */ "POPULATION COUNT",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE1,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RISBHG,
   /* .name        = */ "RISBHG",
   /* .description = */ "ROTATE THEN INSERT SELECTED BITS HIGH (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x5D,
   /* .format      = */ RIEf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RISBLG,
   /* .name        = */ "RISBLG",
   /* .description = */ "ROTATE THEN INSERT SELECTED BITS LOW (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x51,
   /* .format      = */ RIEf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SGRK,
   /* .name        = */ "SGRK",
   /* .description = */ "SUBTRACT (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE9,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SHHHR,
   /* .name        = */ "SHHHR",
   /* .description = */ "SUBTRACT HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xC9,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SHHLR,
   /* .name        = */ "SHHLR",
   /* .description = */ "SUBTRACT HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xD9,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLAK,
   /* .name        = */ "SLAK",
   /* .description = */ "SHIFT LEFT SINGLE (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xDD,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLGRK,
   /* .name        = */ "SLGRK",
   /* .description = */ "SUBTRACT LOGICAL (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xEB,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLHHHR,
   /* .name        = */ "SLHHHR",
   /* .description = */ "SUBTRACT LOGICAL HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xCB,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLHHLR,
   /* .name        = */ "SLHHLR",
   /* .description = */ "SUBTRACT LOGICAL HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xDB,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLLK,
   /* .name        = */ "SLLK",
   /* .description = */ "SHIFT LEFT SINGLE LOGICAL (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xDF,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SLRK,
   /* .name        = */ "SLRK",
   /* .description = */ "SUBTRACT LOGICAL (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xFB,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRAK,
   /* .name        = */ "SRAK",
   /* .description = */ "SHIFT RIGHT SINGLE (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xDC,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRK,
   /* .name        = */ "SRK",
   /* .description = */ "SUBTRACT (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF9,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SRLK,
   /* .name        = */ "SRLK",
   /* .description = */ "SHIFT RIGHT SINGLE LOGICAL (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xDE,
   /* .format      = */ RSYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STCH,
   /* .name        = */ "STCH",
   /* .description = */ "STORE CHARACTER HIGH (8)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xC3,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STFH,
   /* .name        = */ "STFH",
   /* .description = */ "STORE HIGH (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xCB,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STHH,
   /* .name        = */ "STHH",
   /* .description = */ "STORE HALFWORD HIGH (16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xC7,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STOC,
   /* .name        = */ "STOC",
   /* .description = */ "STORE ON CONDITION (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xF3,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is32Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STOCG,
   /* .name        = */ "STOCG",
   /* .description = */ "STORE ON CONDITION (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE3,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_Is64Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XGRK,
   /* .name        = */ "XGRK",
   /* .description = */ "EXCLUSIVE OR (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE7,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XRK,
   /* .name        = */ "XRK",
   /* .description = */ "EXCLUSIVE OR (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF7,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z196,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BPP,
   /* .name        = */ "BPP",
   /* .description = */ "BRANCH PREDICTION PRELOAD",
   /* .opcode[0]   = */ 0xC7,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ SMI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BPRP,
   /* .name        = */ "BPRP",
   /* .description = */ "BRANCH PREDICTION RELATIVE PRELOAD",
   /* .opcode[0]   = */ 0xC5,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ MII_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDZT,
   /* .name        = */ "CDZT",
   /* .description = */ "CONVERT FROM ZONED (to long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xAA,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLGT,
   /* .name        = */ "CLGT",
   /* .description = */ "COMPARE LOGICAL AND TRAP (64)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x2B,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_Trap |
                        S390OpProp_IsLoad
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CLT,
   /* .name        = */ "CLT",
   /* .description = */ "COMPARE LOGICAL AND TRAP (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0x23,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsCompare |
                        S390OpProp_IsLoad |
                        S390OpProp_Trap
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXZT,
   /* .name        = */ "CXZT",
   /* .description = */ "CONVERT FROM ZONED (to extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xAB,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CZDT,
   /* .name        = */ "CZDT",
   /* .description = */ "CONVERT TO ZONED (from long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xA8,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CZXT,
   /* .name        = */ "CZXT",
   /* .description = */ "CONVERT TO ZONED (from extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xA9,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ETND,
   /* .name        = */ "ETND",
   /* .description = */ "EXTRACT TRANSACTION NESTING DEPTH",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xEC,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LAT,
   /* .name        = */ "LAT",
   /* .description = */ "LOAD AND TRAP (32L <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x9F,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_Trap |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LFHAT,
   /* .name        = */ "LFHAT",
   /* .description = */ "LOAD HIGH AND TRAP (32H <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0xC8,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Trap |
                        S390OpProp_LongDispSupported |
                        S390OpProp_TargetHW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGAT,
   /* .name        = */ "LGAT",
   /* .description = */ "LOAD AND TRAP (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x85,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_Trap |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGFAT,
   /* .name        = */ "LLGFAT",
   /* .description = */ "LOAD LOGICAL AND TRAP (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x9D,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_Trap |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGTAT,
   /* .name        = */ "LLGTAT",
   /* .description = */ "LOAD LOGICAL THIRTY ONE BITS AND TRAP (64 <- 31)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x9C,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_Trap |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NIAI,
   /* .name        = */ "NIAI",
   /* .description = */ "NEXT INSTRUCTION ACCESS INTENT",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xFA,
   /* .format      = */ IE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NTSTG,
   /* .name        = */ "NTSTG",
   /* .description = */ "NONTRANSACTIONAL STORE (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x25,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsStore |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PPA,
   /* .name        = */ "PPA",
   /* .description = */ "PERFORM PROCESSOR ASSIST",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xE8,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RIEMIT,
   /* .name        = */ "RIEMIT",
   /* .description = */ "RUNTIME INSTRUMENTATION EMIT",
   /* .opcode[0]   = */ 0xAA,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RINEXT,
   /* .name        = */ "RINEXT",
   /* .description = */ "RUNTIME INSTRUMENTATION NEXT",
   /* .opcode[0]   = */ 0xAA,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_None
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RIOFF,
   /* .name        = */ "RIOFF",
   /* .description = */ "RUNTIME INSTRUMENTATION OFF",
   /* .opcode[0]   = */ 0xAA,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RION,
   /* .name        = */ "RION",
   /* .description = */ "RUNTIME INSTRUMENTATION ON",
   /* .opcode[0]   = */ 0xAA,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ RIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::RISBGN,
   /* .name        = */ "RISBGN",
   /* .description = */ "ROTATE THEN INSERT SELECTED BITS (64)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x59,
   /* .format      = */ RIEf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TABORT,
   /* .name        = */ "TABORT",
   /* .description = */ "TRANSACTION ABORT",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xFC,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TBEGIN,
   /* .name        = */ "TBEGIN",
   /* .description = */ "TRANSACTION BEGIN (nonconstrained)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x60,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TBEGINC,
   /* .name        = */ "TBEGINC",
   /* .description = */ "TRANSACTION BEGIN (constrained)",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x61,
   /* .format      = */ SIL_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::TEND,
   /* .name        = */ "TEND",
   /* .description = */ "TRANSACTION END",
   /* .opcode[0]   = */ 0xB2,
   /* .opcode[1]   = */ 0xF8,
   /* .format      = */ S_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::zEC12,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CDPT,
   /* .name        = */ "CDPT",
   /* .description = */ "CONVERT FROM PACKED (to long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xAE,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CPDT,
   /* .name        = */ "CPDT",
   /* .description = */ "CONVERT TO PACKED (from long DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xAC,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CPXT,
   /* .name        = */ "CPXT",
   /* .description = */ "CONVERT TO PACKED (from extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xAD,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsStore |
                        S390OpProp_SetsOperand2 |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::CXPT,
   /* .name        = */ "CXPT",
   /* .description = */ "CONVERT FROM PACKED (to extended DFP)",
   /* .opcode[0]   = */ 0xED,
   /* .opcode[1]   = */ 0xAF,
   /* .format      = */ RSLb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_DoubleFP |
                        S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesRegPairForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LCBB,
   /* .name        = */ "LCBB",
   /* .description = */ "LOAD COUNT TO BLOCK BOUNDARY",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x27,
   /* .format      = */ RXE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLZRGF,
   /* .name        = */ "LLZRGF",
   /* .description = */ "LOAD LOGICAL AND ZERO RIGHTMOST BYTE (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x3A,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCFH,
   /* .name        = */ "LOCFH",
   /* .description = */ "LOAD HIGH ON CONDITION (32)",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE0,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_ReadsCC |
                        S390OpProp_LongDispSupported |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCFHR,
   /* .name        = */ "LOCFHR",
   /* .description = */ "LOAD HIGH ON CONDITION (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE0,
   /* .format      = */ RRFc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCGHI,
   /* .name        = */ "LOCGHI",
   /* .description = */ "LOAD HALFWORD IMMEDIATE ON CONDITION (64 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x46,
   /* .format      = */ RIEg_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCHHI,
   /* .name        = */ "LOCHHI",
   /* .description = */ "LOAD HALFWORD HIGH IMMEDIATE ON CONDITION (32 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x4E,
   /* .format      = */ RIEg_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_TargetHW |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LOCHI,
   /* .name        = */ "LOCHI",
   /* .description = */ "LOAD HALFWORD IMMEDIATE ON CONDITION (32 <- 16)",
   /* .opcode[0]   = */ 0xEC,
   /* .opcode[1]   = */ 0x42,
   /* .format      = */ RIEg_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LZRF,
   /* .name        = */ "LZRF",
   /* .description = */ "LOAD AND ZERO RIGHTMOST BYTE (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x3B,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LZRG,
   /* .name        = */ "LZRG",
   /* .description = */ "LOAD AND ZERO RIGHTMOST BYTE (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x2A,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::PRNO,
   /* .name        = */ "PRNO",
   /* .description = */ "PERFORM RANDOM NUMBER OPERATION",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x3C,
   /* .format      = */ RRE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STOCFH,
   /* .name        = */ "STOCFH",
   /* .description = */ "STORE HIGH ON CONDITION",
   /* .opcode[0]   = */ 0xEB,
   /* .opcode[1]   = */ 0xE1,
   /* .format      = */ RSYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_TargetHW |
                        S390OpProp_ReadsCC |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VA,
   /* .name        = */ "VA",
   /* .description = */ "VECTOR ADD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF3,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VAC,
   /* .name        = */ "VAC",
   /* .description = */ "VECTOR ADD WITH CARRY",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xBB,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VACC,
   /* .name        = */ "VACC",
   /* .description = */ "VECTOR ADD COMPUTE CARRY",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF1,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VACCC,
   /* .name        = */ "VACCC",
   /* .description = */ "VECTOR ADD WITH CARRY COMPUTE CARRY",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xB9,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VAVG,
   /* .name        = */ "VAVG",
   /* .description = */ "VECTOR AVERAGE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF2,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VAVGL,
   /* .name        = */ "VAVGL",
   /* .description = */ "VECTOR AVERAGE LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF0,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCDG,
   /* .name        = */ "VCDG",
   /* .description = */ "VECTOR FP CONVERT FROM FIXED 64-BIT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC3,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCDLG,
   /* .name        = */ "VCDLG",
   /* .description = */ "VECTOR FP CONVERT FROM LOGICAL 64-BIT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC1,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCEQ,
   /* .name        = */ "VCEQ",
   /* .description = */ "VECTOR COMPARE EQUAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF8,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCGD,
   /* .name        = */ "VCGD",
   /* .description = */ "VECTOR FP CONVERT TO FIXED 64-BIT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC2,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCH,
   /* .name        = */ "VCH",
   /* .description = */ "VECTOR COMPARE HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xFB,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCHL,
   /* .name        = */ "VCHL",
   /* .description = */ "VECTOR COMPARE HIGH LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF9,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCKSM,
   /* .name        = */ "VCKSM",
   /* .description = */ "VECTOR CHECKSUM",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x66,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCLGD,
   /* .name        = */ "VCLGD",
   /* .description = */ "VECTOR FP CONVERT TO LOGICAL 64-BIT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC0,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCLZ,
   /* .name        = */ "VCLZ",
   /* .description = */ "VECTOR COUNT LEADING ZEROS",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x53,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCTZ,
   /* .name        = */ "VCTZ",
   /* .description = */ "VECTOR COUNT TRAILING ZEROS",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x52,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VEC,
   /* .name        = */ "VEC",
   /* .description = */ "VECTOR ELEMENT COMPARE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xDB,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VECL,
   /* .name        = */ "VECL",
   /* .description = */ "VECTOR ELEMENT COMPARE LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xD9,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VERIM,
   /* .name        = */ "VERIM",
   /* .description = */ "VECTOR ELEMENT ROTATE AND INSERT UNDER MASK",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x72,
   /* .format      = */ VRId_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VERLL,
   /* .name        = */ "VERLL",
   /* .description = */ "VECTOR ELEMENT ROTATE LEFT LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x33,
   /* .format      = */ VRSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VERLLV,
   /* .name        = */ "VERLLV",
   /* .description = */ "VECTOR ELEMENT ROTATE LEFT LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x73,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VESL,
   /* .name        = */ "VESL",
   /* .description = */ "VECTOR ELEMENT SHIFT LEFT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x30,
   /* .format      = */ VRSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VESLV,
   /* .name        = */ "VESLV",
   /* .description = */ "VECTOR ELEMENT SHIFT LEFT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x70,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VESRA,
   /* .name        = */ "VESRA",
   /* .description = */ "VECTOR ELEMENT SHIFT RIGHT ARITHMETIC",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x3A,
   /* .format      = */ VRSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VESRAV,
   /* .name        = */ "VESRAV",
   /* .description = */ "VECTOR ELEMENT SHIFT RIGHT ARITHMETIC",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x7A,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VESRL,
   /* .name        = */ "VESRL",
   /* .description = */ "VECTOR ELEMENT SHIFT RIGHT LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x38,
   /* .format      = */ VRSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VESRLV,
   /* .name        = */ "VESRLV",
   /* .description = */ "VECTOR ELEMENT SHIFT RIGHT LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x78,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFA,
   /* .name        = */ "VFA",
   /* .description = */ "VECTOR FP ADD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xE3,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFAE,
   /* .name        = */ "VFAE",
   /* .description = */ "VECTOR FIND ANY ELEMENT EQUAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x82,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorStringOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFCE,
   /* .name        = */ "VFCE",
   /* .description = */ "VECTOR FP COMPARE EQUAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xE8,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFCH,
   /* .name        = */ "VFCH",
   /* .description = */ "VECTOR FP COMPARE HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xEB,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFCHE,
   /* .name        = */ "VFCHE",
   /* .description = */ "VECTOR FP COMPARE HIGH OR EQUAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xEA,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFD,
   /* .name        = */ "VFD",
   /* .description = */ "VECTOR FP DIVIDE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xE5,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFEE,
   /* .name        = */ "VFEE",
   /* .description = */ "VECTOR FIND ELEMENT EQUAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x80,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorStringOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFENE,
   /* .name        = */ "VFENE",
   /* .description = */ "VECTOR FIND ELEMENT NOT EQUAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x81,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorStringOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFI,
   /* .name        = */ "VFI",
   /* .description = */ "VECTOR LOAD FP INTEGER",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC7,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFM,
   /* .name        = */ "VFM",
   /* .description = */ "VECTOR FP MULTIPLY",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xE7,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFMA,
   /* .name        = */ "VFMA",
   /* .description = */ "VECTOR FP MULTIPLY AND ADD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x8F,
   /* .format      = */ VRRe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFMS,
   /* .name        = */ "VFMS",
   /* .description = */ "VECTOR FP MULTIPLY AND SUBTRACT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x8E,
   /* .format      = */ VRRe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFPSO,
   /* .name        = */ "VFPSO",
   /* .description = */ "VECTOR FP PERFORM SIGN OPERATION",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xCC,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFS,
   /* .name        = */ "VFS",
   /* .description = */ "VECTOR FP SUBTRACT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xE2,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFSQ,
   /* .name        = */ "VFSQ",
   /* .description = */ "VECTOR FP SQUARE ROOT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xCE,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFTCI,
   /* .name        = */ "VFTCI",
   /* .description = */ "VECTOR FP TEST DATA CLASS IMMEDIATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x4A,
   /* .format      = */ VRIe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VGBM,
   /* .name        = */ "VGBM",
   /* .description = */ "VECTOR GENERATE BYTE MASK",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x44,
   /* .format      = */ VRIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VGEF,
   /* .name        = */ "VGEF",
   /* .description = */ "VECTOR GATHER ELEMENT (32)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x13,
   /* .format      = */ VRV_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VGEG,
   /* .name        = */ "VGEG",
   /* .description = */ "VECTOR GATHER ELEMENT (64)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x12,
   /* .format      = */ VRV_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VGFM,
   /* .name        = */ "VGFM",
   /* .description = */ "VECTOR GALOIS FIELD MULTIPLY SUM",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xB4,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VGFMA,
   /* .name        = */ "VGFMA",
   /* .description = */ "VECTOR GALOIS FIELD MULTIPLY SUM AND ACCUMULATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xBC,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VGM,
   /* .name        = */ "VGM",
   /* .description = */ "VECTOR GENERATE MASK",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x46,
   /* .format      = */ VRIb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VISTR,
   /* .name        = */ "VISTR",
   /* .description = */ "VECTOR ISOLATE STRING",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x5C,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorStringOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VL,
   /* .name        = */ "VL",
   /* .description = */ "VECTOR LOAD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLBB,
   /* .name        = */ "VLBB",
   /* .description = */ "VECTOR LOAD TO BLOCK BOUNDARY",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLC,
   /* .name        = */ "VLC",
   /* .description = */ "VECTOR LOAD COMPLEMENT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xDE,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLDE,
   /* .name        = */ "VLDE",
   /* .description = */ "vector floating-point load lengthened",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC4,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEB,
   /* .name        = */ "VLEB",
   /* .description = */ "VECTOR LOAD ELEMENT (8)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x00,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLED,
   /* .name        = */ "VLED",
   /* .description = */ "vector floating-point load rounded",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC5,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEF,
   /* .name        = */ "VLEF",
   /* .description = */ "VECTOR LOAD ELEMENT (32)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEG,
   /* .name        = */ "VLEG",
   /* .description = */ "VECTOR LOAD ELEMENT (64)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEH,
   /* .name        = */ "VLEH",
   /* .description = */ "VECTOR LOAD ELEMENT (16)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEIB,
   /* .name        = */ "VLEIB",
   /* .description = */ "VECTOR LOAD ELEMENT IMMEDIATE (8)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x40,
   /* .format      = */ VRIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEIF,
   /* .name        = */ "VLEIF",
   /* .description = */ "VECTOR LOAD ELEMENT IMMEDIATE (32)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x43,
   /* .format      = */ VRIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEIG,
   /* .name        = */ "VLEIG",
   /* .description = */ "VECTOR LOAD ELEMENT IMMEDIATE (64)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x42,
   /* .format      = */ VRIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEIH,
   /* .name        = */ "VLEIH",
   /* .description = */ "VECTOR LOAD ELEMENT IMMEDIATE (16)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x41,
   /* .format      = */ VRIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLGV,
   /* .name        = */ "VLGV",
   /* .description = */ "VECTOR LOAD GR FROM VR ELEMENT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x21,
   /* .format      = */ VRSc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_Is64Bit |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLL,
   /* .name        = */ "VLL",
   /* .description = */ "VECTOR LOAD WITH LENGTH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x37,
   /* .format      = */ VRSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLLEZ,
   /* .name        = */ "VLLEZ",
   /* .description = */ "VECTOR LOAD LOGICAL ELEMENT AND ZERO",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLM,
   /* .name        = */ "VLM",
   /* .description = */ "VECTOR LOAD MULTIPLE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x36,
   /* .format      = */ VRSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesRegRangeForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsOperand2
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLP,
   /* .name        = */ "VLP",
   /* .description = */ "VECTOR LOAD POSITIVE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xDF,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLR,
   /* .name        = */ "VLR",
   /* .description = */ "VECTOR LOAD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x56,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLREP,
   /* .name        = */ "VLREP",
   /* .description = */ "VECTOR LOAD AND REPLICATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLVG,
   /* .name        = */ "VLVG",
   /* .description = */ "VECTOR LOAD VR ELEMENT FROM GR",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x22,
   /* .format      = */ VRSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_Is64Bit |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLVGP,
   /* .name        = */ "VLVGP",
   /* .description = */ "VECTOR LOAD VR FROM GRS DISJOINT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x62,
   /* .format      = */ VRRf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_Is64Bit
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMAE,
   /* .name        = */ "VMAE",
   /* .description = */ "VECTOR MULTIPLY AND ADD EVEN",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xAE,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMAH,
   /* .name        = */ "VMAH",
   /* .description = */ "VECTOR MULTIPLY AND ADD HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xAB,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMAL,
   /* .name        = */ "VMAL",
   /* .description = */ "VECTOR MULTIPLY AND ADD LOW",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xAA,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMALE,
   /* .name        = */ "VMALE",
   /* .description = */ "VECTOR MULTIPLY AND ADD LOGICAL EVEN",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xAC,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMALH,
   /* .name        = */ "VMALH",
   /* .description = */ "VECTOR MULTIPLY AND ADD LOGICAL HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA9,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMALO,
   /* .name        = */ "VMALO",
   /* .description = */ "VECTOR MULTIPLY AND ADD LOGICAL ODD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xAD,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMAO,
   /* .name        = */ "VMAO",
   /* .description = */ "VECTOR MULTIPLY AND ADD ODD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xAF,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VME,
   /* .name        = */ "VME",
   /* .description = */ "VECTOR MULTIPLY EVEN",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA6,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMH,
   /* .name        = */ "VMH",
   /* .description = */ "VECTOR MULTIPLY HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA3,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VML,
   /* .name        = */ "VML",
   /* .description = */ "VECTOR MULTIPLY LOW",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA2,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMLE,
   /* .name        = */ "VMLE",
   /* .description = */ "VECTOR MULTIPLY LOGICAL EVEN",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA4,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMLH,
   /* .name        = */ "VMLH",
   /* .description = */ "VECTOR MULTIPLY LOGICAL HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA1,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMLO,
   /* .name        = */ "VMLO",
   /* .description = */ "VECTOR MULTIPLY LOGICAL ODD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA5,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMN,
   /* .name        = */ "VMN",
   /* .description = */ "VECTOR MINIMUM",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xFE,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMNL,
   /* .name        = */ "VMNL",
   /* .description = */ "VECTOR MINIMUM LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xFC,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMO,
   /* .name        = */ "VMO",
   /* .description = */ "VECTOR MULTIPLY ODD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xA7,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMRH,
   /* .name        = */ "VMRH",
   /* .description = */ "VECTOR MERGE HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x61,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMRL,
   /* .name        = */ "VMRL",
   /* .description = */ "VECTOR MERGE LOW",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x60,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMX,
   /* .name        = */ "VMX",
   /* .description = */ "VECTOR MAXIMUM",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xFF,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMXL,
   /* .name        = */ "VMXL",
   /* .description = */ "VECTOR MAXIMUM LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xFD,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VN,
   /* .name        = */ "VN",
   /* .description = */ "VECTOR AND",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x68,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VNC,
   /* .name        = */ "VNC",
   /* .description = */ "VECTOR AND WITH COMPLEMENT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x69,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VNO,
   /* .name        = */ "VNO",
   /* .description = */ "VECTOR NOR",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x6B,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VO,
   /* .name        = */ "VO",
   /* .description = */ "VECTOR OR",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x6A,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPDI,
   /* .name        = */ "VPDI",
   /* .description = */ "VECTOR PERMUTE DOUBLEWORD IMMEDIATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x84,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPERM,
   /* .name        = */ "VPERM",
   /* .description = */ "VECTOR PERMUTE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x8C,
   /* .format      = */ VRRe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPK,
   /* .name        = */ "VPK",
   /* .description = */ "VECTOR PACK",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x94,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPKLS,
   /* .name        = */ "VPKLS",
   /* .description = */ "VECTOR PACK LOGICAL SATURATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x95,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPKS,
   /* .name        = */ "VPKS",
   /* .description = */ "VECTOR PACK SATURATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x97,
   /* .format      = */ VRRb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPOPCT,
   /* .name        = */ "VPOPCT",
   /* .description = */ "VECTOR POPULATION COUNT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x50,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VREP,
   /* .name        = */ "VREP",
   /* .description = */ "VECTOR REPLICATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x4D,
   /* .format      = */ VRIc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VREPI,
   /* .name        = */ "VREPI",
   /* .description = */ "VECTOR REPLICATE IMMEDIATE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x45,
   /* .format      = */ VRIa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VS,
   /* .name        = */ "VS",
   /* .description = */ "VECTOR SUBTRACT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF7,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSBCBI,
   /* .name        = */ "VSBCBI",
   /* .description = */ "VECTOR SUBTRACT WITH BORROW COMPUTE BORROW INDICATION",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xBD,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSBI,
   /* .name        = */ "VSBI",
   /* .description = */ "VECTOR SUBTRACT WITH BORROW INDICATION",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xBF,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSCBI,
   /* .name        = */ "VSCBI",
   /* .description = */ "VECTOR SUBTRACT COMPUTE BORROW INDICATION",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xF5,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSCEF,
   /* .name        = */ "VSCEF",
   /* .description = */ "VECTOR SCATTER ELEMENT (32)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x1B,
   /* .format      = */ VRV_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSCEG,
   /* .name        = */ "VSCEG",
   /* .description = */ "VECTOR SCATTER ELEMENT (64)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x1A,
   /* .format      = */ VRV_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSEG,
   /* .name        = */ "VSEG",
   /* .description = */ "VECTOR SIGN EXTEND TO DOUBLEWORD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x5F,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSEL,
   /* .name        = */ "VSEL",
   /* .description = */ "VECTOR SELECT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x8D,
   /* .format      = */ VRRe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSL,
   /* .name        = */ "VSL",
   /* .description = */ "VECTOR SHIFT LEFT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x74,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSLB,
   /* .name        = */ "VSLB",
   /* .description = */ "VECTOR SHIFT LEFT BY BYTE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x75,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSLDB,
   /* .name        = */ "VSLDB",
   /* .description = */ "VECTOR SHIFT LEFT DOUBLE BY BYTE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x77,
   /* .format      = */ VRId_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSRA,
   /* .name        = */ "VSRA",
   /* .description = */ "VECTOR SHIFT RIGHT ARITHMETIC",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x7E,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSRAB,
   /* .name        = */ "VSRAB",
   /* .description = */ "VECTOR SHIFT RIGHT ARITHMETIC BY BYTE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x7F,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSRL,
   /* .name        = */ "VSRL",
   /* .description = */ "VECTOR SHIFT RIGHT LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x7C,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSRLB,
   /* .name        = */ "VSRLB",
   /* .description = */ "VECTOR SHIFT RIGHT LOGICAL BY BYTE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x7D,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VST,
   /* .name        = */ "VST",
   /* .description = */ "VECTOR STORE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTEB,
   /* .name        = */ "VSTEB",
   /* .description = */ "VECTOR STORE ELEMENT (8)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x08,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTEF,
   /* .name        = */ "VSTEF",
   /* .description = */ "VECTOR STORE ELEMENT (32)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTEG,
   /* .name        = */ "VSTEG",
   /* .description = */ "VECTOR STORE ELEMENT (64)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTEH,
   /* .name        = */ "VSTEH",
   /* .description = */ "VECTOR STORE ELEMENT (16)",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTL,
   /* .name        = */ "VSTL",
   /* .description = */ "VECTOR STORE WITH LENGTH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x3F,
   /* .format      = */ VRSb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTM,
   /* .name        = */ "VSTM",
   /* .description = */ "VECTOR STORE MULTIPLE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x3E,
   /* .format      = */ VRSa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesRegRangeForTarget
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTRC,
   /* .name        = */ "VSTRC",
   /* .description = */ "VECTOR STRING RANGE COMPARE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x8A,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorStringOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSUM,
   /* .name        = */ "VSUM",
   /* .description = */ "VECTOR SUM ACROSS WORD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x64,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSUMG,
   /* .name        = */ "VSUMG",
   /* .description = */ "VECTOR SUM ACROSS DOUBLEWORD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x65,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSUMQ,
   /* .name        = */ "VSUMQ",
   /* .description = */ "VECTOR SUM ACROSS QUADWORD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x67,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VTM,
   /* .name        = */ "VTM",
   /* .description = */ "VECTOR TEST UNDER MASK",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xD8,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VUPH,
   /* .name        = */ "VUPH",
   /* .description = */ "VECTOR UNPACK HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xD7,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VUPL,
   /* .name        = */ "VUPL",
   /* .description = */ "VECTOR UNPACK LOW",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xD6,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VUPLH,
   /* .name        = */ "VUPLH",
   /* .description = */ "VECTOR UNPACK LOGICAL HIGH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xD5,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VUPLL,
   /* .name        = */ "VUPLL",
   /* .description = */ "VECTOR UNPACK LOGICAL LOW",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xD4,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VX,
   /* .name        = */ "VX",
   /* .description = */ "VECTOR EXCLUSIVE OR",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x6D,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::WFC,
   /* .name        = */ "WFC",
   /* .description = */ "VECTOR FP COMPARE SCALAR",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xCB,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::WFK,
   /* .name        = */ "WFK",
   /* .description = */ "VECTOR FP COMPARE AND SIGNAL SCALAR",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xCA,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z13,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_SetsCC |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AGH,
   /* .name        = */ "AGH",
   /* .description = */ "ADD HALFWORD (64 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x38,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsSignFlag |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::BIC,
   /* .name        = */ "BIC",
   /* .description = */ "BRANCH INDIRECT ON CONDITION",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x47,
   /* .format      = */ RXYb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_BranchOp |
                        S390OpProp_ReadsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::KMA,
   /* .name        = */ "KMA",
   /* .description = */ "CIPHER MESSAGE WITH AUTHENTICATION",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x29,
   /* .format      = */ RRFb_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGG,
   /* .name        = */ "LGG",
   /* .description = */ "LOAD GUARDED (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x4C,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_BranchOp |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LGSC,
   /* .name        = */ "LGSC",
   /* .description = */ "LOAD GUARDED STORAGE CONTROLS",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x4D,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::LLGFSG,
   /* .name        = */ "LLGFSG",
   /* .description = */ "LOAD LOGICAL AND SHIFT GUARDED (64 <- 32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x48,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::Unknown,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_Is64Bit |
                        S390OpProp_BranchOp |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MG,
   /* .name        = */ "MG",
   /* .description = */ "MULTIPLY (128 <- 64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x84,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MGH,
   /* .name        = */ "MGH",
   /* .description = */ "MULTIPLY HALFWORD (64 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x3C,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MGRK,
   /* .name        = */ "MGRK",
   /* .description = */ "MULTIPLY (128 <- 64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xEC,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_UsesRegPairForTarget |
                        S390OpProp_SetsOperand3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSC,
   /* .name        = */ "MSC",
   /* .description = */ "MULTIPLY SINGLE (32)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x53,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSGC,
   /* .name        = */ "MSGC",
   /* .description = */ "MULTIPLY SINGLE (64)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x83,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_UsesTarget |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSGRKC,
   /* .name        = */ "MSGRKC",
   /* .description = */ "MULTIPLY SINGLE (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xED,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MSRKC,
   /* .name        = */ "MSRKC",
   /* .description = */ "MULTIPLY SINGLE (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xFD,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsOperand3 |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SGH,
   /* .name        = */ "SGH",
   /* .description = */ "SUBTRACT HALFWORD (64 <- 16)",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x39,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_IsLoad |
                        S390OpProp_LongDispSupported |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsOverflowFlag |
                        S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::STGSC,
   /* .name        = */ "STGSC",
   /* .description = */ "STORE GUARDED STORAGE CONTROLS",
   /* .opcode[0]   = */ 0xE3,
   /* .opcode[1]   = */ 0x49,
   /* .format      = */ RXYa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_LongDispSupported
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VAP,
   /* .name        = */ "VAP",
   /* .description = */ "VECTOR ADD DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x71,
   /* .format      = */ VRIf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VBPERM,
   /* .name        = */ "VBPERM",
   /* .description = */ "VECTOR BIT PERMUTE",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x85,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCP,
   /* .name        = */ "VCP",
   /* .description = */ "VECTOR COMPARE DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x77,
   /* .format      = */ VRRh_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCVB,
   /* .name        = */ "VCVB",
   /* .description = */ "VECTOR CONVERT TO BINARY",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x50,
   /* .format      = */ VRRi_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCVBG,
   /* .name        = */ "VCVBG",
   /* .description = */ "VECTOR CONVERT TO BINARY",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x52,
   /* .format      = */ VRRi_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCVD,
   /* .name        = */ "VCVD",
   /* .description = */ "VECTOR CONVERT TO DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x58,
   /* .format      = */ VRIi_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VCVDG,
   /* .name        = */ "VCVDG",
   /* .description = */ "VECTOR CONVERT TO DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x5A,
   /* .format      = */ VRIi_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM4
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VDP,
   /* .name        = */ "VDP",
   /* .description = */ "VECTOR DIVIDE DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x7A,
   /* .format      = */ VRIf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFLL,
   /* .name        = */ "VFLL",
   /* .description = */ "VECTOR FP LOAD LENGTHENED",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC4,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFLR,
   /* .name        = */ "VFLR",
   /* .description = */ "VECTOR FP LOAD ROUNDED",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xC5,
   /* .format      = */ VRRa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFMAX,
   /* .name        = */ "VFMAX",
   /* .description = */ "VECTOR FP MAXIMUM",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xEF,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFMIN,
   /* .name        = */ "VFMIN",
   /* .description = */ "VECTOR FP MINIMUM",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xEE,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM4 |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFNMA,
   /* .name        = */ "VFNMA",
   /* .description = */ "VECTOR FP NEGATIVE MULTIPLY AND ADD",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x9F,
   /* .format      = */ VRRe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VFNMS,
   /* .name        = */ "VFNMS",
   /* .description = */ "VECTOR FP NEGATIVE MULTIPLY AND SUBTRACT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x9E,
   /* .format      = */ VRRe_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6 |
                        S390OpProp_VectorFPOp |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLIP,
   /* .name        = */ "VLIP",
   /* .description = */ "VECTOR LOAD IMMEDIATE DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x49,
   /* .format      = */ VRIh_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLRL,
   /* .name        = */ "VLRL",
   /* .description = */ "VECTOR LOAD RIGHTMOST WITH LENGTH",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x35,
   /* .format      = */ VSI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLRLR,
   /* .name        = */ "VLRLR",
   /* .description = */ "VECTOR LOAD RIGHTMOST WITH LENGTH",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x37,
   /* .format      = */ VRSd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMP,
   /* .name        = */ "VMP",
   /* .description = */ "VECTOR MULTIPLY DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x78,
   /* .format      = */ VRIf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMSL,
   /* .name        = */ "VMSL",
   /* .description = */ "VECTOR MULTIPLY SUM LOGICAL",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0xB8,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5 |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VMSP,
   /* .name        = */ "VMSP",
   /* .description = */ "VECTOR MULTIPLY AND SHIFT DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x79,
   /* .format      = */ VRIf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VNN,
   /* .name        = */ "VNN",
   /* .description = */ "VECTOR NAND",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x6E,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VNX,
   /* .name        = */ "VNX",
   /* .description = */ "VECTOR NOT EXCLUSIVE OR",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x6C,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VOC,
   /* .name        = */ "VOC",
   /* .description = */ "VECTOR OR WITH COMPLEMENT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x6F,
   /* .format      = */ VRRc_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPKZ,
   /* .name        = */ "VPKZ",
   /* .description = */ "VECTOR PACK ZONED",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x34,
   /* .format      = */ VSI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VPSOP,
   /* .name        = */ "VPSOP",
   /* .description = */ "VECTOR PERFORM SIGN OPERATION DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x5B,
   /* .format      = */ VRIg_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VRP,
   /* .name        = */ "VRP",
   /* .description = */ "VECTOR REMAINDER DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x7B,
   /* .format      = */ VRIf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSDP,
   /* .name        = */ "VSDP",
   /* .description = */ "VECTOR SHIFT AND DIVIDE DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x7E,
   /* .format      = */ VRIf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSP,
   /* .name        = */ "VSP",
   /* .description = */ "VECTOR SUBTRACT DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x73,
   /* .format      = */ VRIf_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSRP,
   /* .name        = */ "VSRP",
   /* .description = */ "VECTOR SHIFT AND ROUND DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x59,
   /* .format      = */ VRIg_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM5
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTRL,
   /* .name        = */ "VSTRL",
   /* .description = */ "VECTOR STORE RIGHTMOST WITH LENGTH",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x3D,
   /* .format      = */ VSI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTRLR,
   /* .name        = */ "VSTRLR",
   /* .description = */ "VECTOR STORE RIGHTMOST WITH LENGTH",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x3F,
   /* .format      = */ VRSd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VTP,
   /* .name        = */ "VTP",
   /* .description = */ "VECTOR TEST DECIMAL",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x5F,
   /* .format      = */ VRRg_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_SetsCC
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VUPKZ,
   /* .name        = */ "VUPKZ",
   /* .description = */ "VECTOR UNPACK ZONED",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x3C,
   /* .format      = */ VSI_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z14,
   /* .properties  = */ S390OpProp_IsStore
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::MVCRL,
   /* .name        = */ "MVCRL",
   /* .description = */ "MOVE RIGHT TO LEFT",
   /* .opcode[0]   = */ 0xE5,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ SSE_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_ImplicitlyUsesGPR0 |
                        S390OpProp_IsLoad |
                        S390OpProp_IsStore |
                        S390OpProp_HasTwoMemoryReferences
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NCGRK,
   /* .name        = */ "NCGRK",
   /* .description = */ "AND WITH COMPLEMENT (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE5,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NCRK,
   /* .name        = */ "NCRK",
   /* .description = */ "AND WITH COMPLEMENT (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF5,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NNGRK,
   /* .name        = */ "NNGRK",
   /* .description = */ "NAND (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x64,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NNRK,
   /* .name        = */ "NNRK",
   /* .description = */ "NAND (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x74,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NOGRK,
   /* .name        = */ "NOGRK",
   /* .description = */ "NOR (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x66,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NORK,
   /* .name        = */ "NORK",
   /* .description = */ "NOR (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x76,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NXGRK,
   /* .name        = */ "NXGRK",
   /* .description = */ "NOR (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x67,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::NXRK,
   /* .name        = */ "NXRK",
   /* .description = */ "NOT EXCLUSIVE OR (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x77,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OCGRK,
   /* .name        = */ "OCGRK",
   /* .description = */ "OR WITH COMPLEMENT (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x65,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OCRK,
   /* .name        = */ "OCRK",
   /* .description = */ "OR WITH COMPLEMENT (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0x75,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_SetsCC |
                        S390OpProp_SetsZeroFlag |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SELGR,
   /* .name        = */ "SELGR",
   /* .description = */ "SELECT (64)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xE3,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is64Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SELHHHR,
   /* .name        = */ "SELHHHR",
   /* .description = */ "SELECT HIGH (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xC0,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsRegCopy |
                        S390OpProp_TargetHW |
                        S390OpProp_SrcHW |
                        S390OpProp_Src2HW |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::SELR,
   /* .name        = */ "SELR",
   /* .description = */ "SELECT (32)",
   /* .opcode[0]   = */ 0xB9,
   /* .opcode[1]   = */ 0xF0,
   /* .format      = */ RRFa_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_Is32Bit |
                        S390OpProp_ReadsCC |
                        S390OpProp_IsRegCopy |
                        S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLBR,
   /* .name        = */ "VLBR",
   /* .description = */ "VECTOR LOAD BYTE REVERSED ELEMENTS",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x06,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLBRREP,
   /* .name        = */ "VLBRREP",
   /* .description = */ "VECTOR LOAD BYTE REVERSED ELEMENT AND REPLICATE",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x05,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEBRF,
   /* .name        = */ "VLEBRF",
   /* .description = */ "VECTOR LOAD BYTE REVERSED ELEMENT (32)",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x03,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEBRG,
   /* .name        = */ "VLEBRF",
   /* .description = */ "VECTOR LOAD BYTE REVERSED ELEMENT (64)",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x02,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLEBRH,
   /* .name        = */ "VLEBRH",
   /* .description = */ "VECTOR LOAD BYTE REVERSED ELEMENT (16)",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x01,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_UsesTarget |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLER,
   /* .name        = */ "VLER",
   /* .description = */ "VECTOR LOAD ELEMENTS REVERSED",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x07,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VLLEBRZ,
   /* .name        = */ "VLLEBRZ",
   /* .description = */ "VECTOR LOAD BYTE REVERSED ELEMENT AND ZERO",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x04,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsLoad |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_UsesM3 |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTBR,
   /* .name        = */ "VSTBR",
   /* .description = */ "VECTOR STORE BYTE REVERSED ELEMENTS",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x0E,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3 |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTEBRF,
   /* .name        = */ "VSTEBRF",
   /* .description = */ "VECTOR STORE BYTE REVERSED ELEMENT (32)",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x0B,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTEBRG,
   /* .name        = */ "VSTEBRG",
   /* .description = */ "VECTOR STORE BYTE REVERSED ELEMENT (64)",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x0A,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTEBRH,
   /* .name        = */ "VSTEBRH",
   /* .description = */ "VECTOR STORE BYTE REVERSED ELEMENT (16)",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x09,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTER,
   /* .name        = */ "VSTER",
   /* .description = */ "VECTOR STORE ELEMENTS REVERSED",
   /* .opcode[0]   = */ 0xE6,
   /* .opcode[1]   = */ 0x0F,
   /* .format      = */ VRX_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_IsStore |
                        S390OpProp_UsesM3 |
                        S390OpProp_HasExtendedMnemonic
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSLD,
   /* .name        = */ "VSLD",
   /* .description = */ "VECTOR SHIFT LEFT DOUBLE BY BIT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x86,
   /* .format      = */ VRId_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSRD,
   /* .name        = */ "VSRD",
   /* .description = */ "VECTOR SHIFT RIGHT DOUBLE BY BIT",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x87,
   /* .format      = */ VRId_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_SetsOperand1
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::VSTRS,
   /* .name        = */ "VSTRS",
   /* .description = */ "VECTOR STRING SEARCH",
   /* .opcode[0]   = */ 0xE7,
   /* .opcode[1]   = */ 0x8B,
   /* .format      = */ VRRd_FORMAT,
   /* .minimumALS  = */ CPU::Architecture::z15,
   /* .properties  = */ S390OpProp_SetsCC |
                        S390OpProp_SetsOperand1 |
                        S390OpProp_VectorStringOp |
                        S390OpProp_UsesM5 |
                        S390OpProp_UsesM6
   },
