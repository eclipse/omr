/*******************************************************************************
 * Copyright IBM Corp. and others 2023
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_X86OPCODETABLE_HPP
#define OMR_X86OPCODETABLE_HPP

#include <cstdint>
#include "codegen/InstOpCode.hpp"

enum ArithmeticOps : uint32_t
   {
   ArithmeticInvalid,
   BinaryArithmeticAdd,
   BinaryArithmeticSub,
   BinaryArithmeticMul,
   BinaryArithmeticDiv,
   BinaryArithmeticAnd,
   BinaryArithmeticOr,
   BinaryArithmeticXor,
   BinaryArithmeticMin,
   BinaryArithmeticMax,
   BinaryLogicalShiftLeft,
   BinaryLogicalShiftRight,
   BinaryArithmeticShiftRight,
   BinaryRotateLeft,
   BinaryRotateRight,
   NumBinaryArithmeticOps,
   UnaryArithmeticAbs,
   UnaryArithmeticSqrt,
   UnaryLeadingZeroCnt,
   LastOp,
   NumUnaryArithmeticOps = LastOp - NumBinaryArithmeticOps + 1
   };

// TODO: Truncate the table
static const TR::InstOpCode::Mnemonic BinaryArithmeticOpCodesForReg[NumBinaryArithmeticOps][TR::NumOMRTypes] =
   {
   //                NoType                         Int8,                          Int16,                         Int32,                         Int64,                         Float,                         Double,                         Address,                        Aggregate
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticInvalid
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::ADDSSRegReg,   TR::InstOpCode::ADDSDRegReg,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticAdd
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::SUBSSRegReg,   TR::InstOpCode::SUBSDRegReg,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticSub
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::MULSSRegReg,   TR::InstOpCode::MULSDRegReg,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticMul
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::DIVSSRegReg,   TR::InstOpCode::DIVSDRegReg,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticDiv
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticAnd
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticOr,
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticXor
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticMin
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticMax
   };

// TODO: Truncate the table
static const TR::InstOpCode::Mnemonic BinaryArithmeticOpCodesForMem[NumBinaryArithmeticOps][TR::NumOMRTypes] =
   {
   //                NoType                         Int8,                          Int16,                         Int32,                         Int64,                         Float,                         Double,                         Address,                        Aggregate
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticInvalid
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::ADDSSRegMem,   TR::InstOpCode::ADDSDRegMem,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticAdd
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::SUBSSRegMem,   TR::InstOpCode::SUBSDRegMem,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticSub
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::MULSSRegMem,   TR::InstOpCode::MULSDRegMem,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticMul
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::DIVSSRegMem,   TR::InstOpCode::DIVSDRegMem,    TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticDiv
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticAnd
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticOr,
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticXor
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticMin
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,            TR::InstOpCode::bad,            TR::InstOpCode::bad           }, // BinaryArithmeticMax
   };

static const TR::InstOpCode::Mnemonic VectorBinaryArithmeticOpCodesForReg[NumBinaryArithmeticOps][TR::NumVectorElementTypes] =
   {
   //                Int8,                          Int16,                         Int32,                         Int64,                         Float,                         Double
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticInvalid
   { TR::InstOpCode::PADDBRegReg,      TR::InstOpCode::PADDWRegReg,      TR::InstOpCode::PADDDRegReg,      TR::InstOpCode::PADDQRegReg,      TR::InstOpCode::ADDPSRegReg,      TR::InstOpCode::ADDPDRegReg   }, // BinaryArithmeticAdd
   { TR::InstOpCode::PSUBBRegReg,      TR::InstOpCode::PSUBWRegReg,      TR::InstOpCode::PSUBDRegReg,      TR::InstOpCode::PSUBQRegReg,      TR::InstOpCode::SUBPSRegReg,      TR::InstOpCode::SUBPDRegReg   }, // BinaryArithmeticSub
   { TR::InstOpCode::bad,              TR::InstOpCode::PMULLWRegReg,     TR::InstOpCode::PMULLDRegReg,     TR::InstOpCode::bad,              TR::InstOpCode::MULPSRegReg,      TR::InstOpCode::MULPDRegReg   }, // BinaryArithmeticMul
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::DIVPSRegReg,      TR::InstOpCode::DIVPDRegReg   }, // BinaryArithmeticDiv
   { TR::InstOpCode::PANDRegReg,       TR::InstOpCode::PANDRegReg,       TR::InstOpCode::PANDRegReg,       TR::InstOpCode::PANDRegReg,       TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticAnd
   { TR::InstOpCode::PORRegReg,        TR::InstOpCode::PORRegReg,        TR::InstOpCode::PORRegReg,        TR::InstOpCode::PORRegReg,        TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticOr,
   { TR::InstOpCode::PXORRegReg,       TR::InstOpCode::PXORRegReg,       TR::InstOpCode::PXORRegReg,       TR::InstOpCode::PXORRegReg,       TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticXor
   { TR::InstOpCode::PMINSBRegReg,     TR::InstOpCode::PMINSWRegReg,     TR::InstOpCode::PMINSDRegReg,     TR::InstOpCode::PMINSQRegReg,     TR::InstOpCode::MINPSRegReg,      TR::InstOpCode::MINPDRegReg   }, // BinaryArithmeticMin
   { TR::InstOpCode::PMAXSBRegReg,     TR::InstOpCode::PMAXSWRegReg,     TR::InstOpCode::PMAXSDRegReg,     TR::InstOpCode::PMAXSQRegReg,     TR::InstOpCode::MAXPSRegReg,      TR::InstOpCode::MAXPDRegReg   }, // BinaryArithmeticMax
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryLogicalShiftLeft
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryLogicalShiftRight
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticShiftRight
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryRotateLeft
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }  // BinaryRotateRight
   };

static const TR::InstOpCode::Mnemonic VectorBinaryArithmeticOpCodesForMem[NumBinaryArithmeticOps][TR::NumVectorElementTypes] =
   {
   //                Int8,                          Int16,                         Int32,                         Int64,                         Float,                         Double
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticInvalid
   { TR::InstOpCode::PADDBRegMem,      TR::InstOpCode::PADDWRegMem,      TR::InstOpCode::PADDDRegMem,      TR::InstOpCode::PADDQRegMem,      TR::InstOpCode::ADDPSRegMem,      TR::InstOpCode::ADDPDRegMem   }, // BinaryArithmeticAdd
   { TR::InstOpCode::PSUBBRegMem,      TR::InstOpCode::PSUBWRegMem,      TR::InstOpCode::PSUBDRegMem,      TR::InstOpCode::PSUBQRegMem,      TR::InstOpCode::SUBPSRegMem,      TR::InstOpCode::SUBPDRegMem   }, // BinaryArithmeticSub
   { TR::InstOpCode::bad,              TR::InstOpCode::PMULLWRegMem,     TR::InstOpCode::PMULLDRegMem,     TR::InstOpCode::bad,              TR::InstOpCode::MULPSRegMem,      TR::InstOpCode::MULPDRegMem   }, // BinaryArithmeticMul
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::DIVPSRegMem,      TR::InstOpCode::DIVPDRegMem   }, // BinaryArithmeticDiv
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::PANDRegMem,       TR::InstOpCode::PANDRegMem,       TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticAnd
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::PORRegMem,        TR::InstOpCode::PORRegMem,        TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticOr,
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::PXORRegMem,       TR::InstOpCode::PXORRegMem,       TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticXor
   { TR::InstOpCode::PMINSBRegMem,     TR::InstOpCode::PMINSWRegMem,     TR::InstOpCode::PMINSDRegMem,     TR::InstOpCode::PMINSQRegMem,     TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticMin
   { TR::InstOpCode::PMAXSBRegMem,     TR::InstOpCode::PMAXSWRegMem,     TR::InstOpCode::PMAXSDRegMem,     TR::InstOpCode::PMAXSQRegMem,     TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticMax
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryLogicalShiftLeft
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryLogicalShiftRight
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryArithmeticShiftRight
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }, // BinaryRotateLeft
   { TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad,              TR::InstOpCode::bad           }  // BinaryRotateRight
   };

static const TR::InstOpCode::Mnemonic VectorUnaryArithmeticOpCodesForReg[NumUnaryArithmeticOps][TR::NumVectorElementTypes] =
   {
   //                Int8,                          Int16,                         Int32,                         Int64,                         Float,                         Double
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,                TR::InstOpCode::bad,                TR::InstOpCode::bad,           TR::InstOpCode::bad          }, // UnaryArithmeticInvalid,
   { TR::InstOpCode::PABSBRegReg,   TR::InstOpCode::PABSWRegReg,   TR::InstOpCode::PABSDRegReg,        TR::InstOpCode::bad,                TR::InstOpCode::bad,           TR::InstOpCode::bad          }, // UnaryArithmeticAbs,
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,                TR::InstOpCode::bad,                TR::InstOpCode::SQRTPSRegReg,  TR::InstOpCode::SQRTPDRegReg }, // UnaryArithmeticSqrt,
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::VPLZCNTDRegMaskReg, TR::InstOpCode::VPLZCNTQRegMaskReg, TR::InstOpCode::bad,           TR::InstOpCode::bad          }  // UnaryLeadingZeroCnt,
   };

static const TR::InstOpCode::Mnemonic VectorUnaryArithmeticOpCodesForMem[NumUnaryArithmeticOps][TR::NumVectorElementTypes] =
   {
   //                Int8,                          Int16,                         Int32,                         Int64,                         Float,                         Double
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,                TR::InstOpCode::bad,                TR::InstOpCode::bad,           TR::InstOpCode::bad           }, // UnaryArithmeticInvalid,
   { TR::InstOpCode::PABSBRegMem,   TR::InstOpCode::PABSWRegMem,   TR::InstOpCode::PABSDRegMem,        TR::InstOpCode::bad,                TR::InstOpCode::bad,           TR::InstOpCode::bad           }, // UnaryArithmeticAbs,
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::bad,                TR::InstOpCode::bad,                TR::InstOpCode::VSQRTPSRegMem, TR::InstOpCode::VSQRTPDRegMem }, // UnaryArithmeticSqrt,
   { TR::InstOpCode::bad,           TR::InstOpCode::bad,           TR::InstOpCode::VPLZCNTDRegMaskMem, TR::InstOpCode::VPLZCNTQRegMaskMem, TR::InstOpCode::bad,           TR::InstOpCode::bad           }  // UnaryLeadingZeroCnt,
   };

#endif
