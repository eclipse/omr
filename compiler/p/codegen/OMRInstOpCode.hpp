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

#ifndef OMR_POWER_INSTOPCODE_INCL
#define OMR_POWER_INSTOPCODE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTOPCODE_CONNECTOR
#define OMR_INSTOPCODE_CONNECTOR
namespace OMR { namespace Power { class InstOpCode; } }
namespace OMR { typedef OMR::Power::InstOpCode InstOpCodeConnector; }
#else
#error OMR::Power::InstOpCode expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRInstOpCode.hpp"

#include "codegen/PPCOpsDefines.hpp"

typedef uint32_t TR_PPCOpCodeBinaryEntry;
extern const char * ppcOpCodeToNameMap[][2];

namespace OMR
{

namespace Power
{

class InstOpCode: public OMR::InstOpCode
   {
   protected:

   InstOpCode():  OMR::InstOpCode(bad)  {}
   InstOpCode(Mnemonic m): OMR::InstOpCode(m)  {}

   public:

   static const uint32_t                  properties[PPCNumOpCodes];
   static const TR_PPCOpCodeBinaryEntry   binaryEncodings[PPCNumOpCodes];

   bool isRecordForm() {return (properties[_mnemonic] & PPCOpProp_IsRecordForm)!=0;}

        bool hasRecordForm() {return (properties[_mnemonic] & PPCOpProp_HasRecordForm)!=0;}

        bool singleFPOp() {return (properties[_mnemonic] & PPCOpProp_SingleFP)!=0;}

        bool doubleFPOp() {return (properties[_mnemonic] & PPCOpProp_DoubleFP)!=0;}

        bool gprOp() {return (properties[_mnemonic] & (PPCOpProp_DoubleFP | PPCOpProp_SingleFP))==0;}

        bool fprOp() {return (properties[_mnemonic] & (PPCOpProp_DoubleFP | PPCOpProp_SingleFP))!=0;}

        bool useAlternateFormat() {return (properties[_mnemonic] & PPCOpProp_AltFormat)!=0;}

        bool useAlternateFormatx() {return (properties[_mnemonic] & PPCOpProp_AltFormatx)!=0;}

        bool readsCarryFlag() {return (properties[_mnemonic] & PPCOpProp_ReadsCarryFlag)!=0;}

        bool setsCarryFlag() {return (properties[_mnemonic] & PPCOpProp_SetsCarryFlag)!=0;}

        bool setsOverflowFlag() {return (properties[_mnemonic] & PPCOpProp_SetsOverflowFlag)!=0;}

        bool usesCountRegister() {return (properties[_mnemonic] & PPCOpProp_UsesCtr)!=0;}

        bool setsCountRegister() {return (properties[_mnemonic] & PPCOpProp_SetsCtr)!=0;}

        bool isBranchOp() {return (properties[_mnemonic] & PPCOpProp_BranchOp)!=0;}

        bool isLoad() {return (properties[_mnemonic] & PPCOpProp_IsLoad)!=0;}

        bool isStore() {return (properties[_mnemonic] & PPCOpProp_IsStore)!=0;}

        bool isRegCopy() {return (properties[_mnemonic] & PPCOpProp_IsRegCopy)!=0;}

        bool isUpdate() {return (properties[_mnemonic] & PPCOpProp_UpdateForm)!=0;}

        bool isDoubleWord() {return (properties[_mnemonic] & PPCOpProp_DWord)!=0;}

        bool isCall() {return _mnemonic==bl;}

        bool isTrap() {return (properties[_mnemonic] & PPCOpProp_Trap)!=0;}

        bool isTMAbort() {return (properties[_mnemonic] & PPCOpProp_TMAbort)!=0;}

        bool isFloat() {return (properties[_mnemonic] & (PPCOpProp_SingleFP|PPCOpProp_DoubleFP))!=0;}

        bool isVMX() {return (properties[_mnemonic] & PPCOpProp_IsVMX)!=0;}

        bool isVSX() {return (properties[_mnemonic] & PPCOpProp_IsVSX)!=0;}

        bool usesTarget() {return (properties[_mnemonic] & PPCOpProp_UsesTarget)!=0;}

        bool useMaskEnd() {return (properties[_mnemonic] & PPCOpProp_UseMaskEnd)!=0;}

        bool isRotateOrShift() {return (properties[_mnemonic] & PPCOpProp_IsRotateOrShift)!=0;}

        bool isCompare() {return (properties[_mnemonic] & PPCOpProp_CompareOp)!=0;}

        bool readsFPSCR() {return (properties[_mnemonic] & PPCOpProp_ReadsFPSCR)!=0;}

        bool setsFPSCR() {return (properties[_mnemonic] & PPCOpProp_SetsFPSCR)!=0;}

        bool isSyncSideEffectFree() {return (properties[_mnemonic] & PPCOpProp_SyncSideEffectFree)!=0;}

        bool offsetRequiresWordAlignment() { return (properties[_mnemonic] & PPCOpProp_OffsetRequiresWordAlignment)!=0;}

        bool setsCTR() {return (properties[_mnemonic] & PPCOpProp_SetsCtr)!=0;}

        bool usesCTR() {return (properties[_mnemonic] & PPCOpProp_UsesCtr)!=0;}

        bool isLongRunningFPOp() {return _mnemonic==fdiv  ||
                                         _mnemonic==fdivs ||
                                         _mnemonic==fsqrt ||
                                         _mnemonic==fsqrts;}

        bool isFXMult() {return _mnemonic==mullw  ||
                                _mnemonic==mulli  ||
                                _mnemonic==mulhw  ||
                                _mnemonic==mulhd  ||
                                _mnemonic==mulhwu ||
                                _mnemonic==mulhdu;}

        bool isAdmin() {return _mnemonic==ret      ||
                               _mnemonic==fence    ||
                               _mnemonic==depend   ||
                               _mnemonic==proc     ||
                               _mnemonic==assocreg ||
                               _mnemonic==dd;}

        static const TR_PPCOpCodeBinaryEntry getOpCodeBinaryEncoding(Mnemonic opCode)
           {return binaryEncodings[opCode];}
        const TR_PPCOpCodeBinaryEntry getOpCodeBinaryEncoding()
           {return getOpCodeBinaryEncoding(_mnemonic);}

        uint8_t *copyBinaryToBuffer(uint8_t *cursor)
           {
           *(uint32_t *)cursor = *(uint32_t *)&binaryEncodings[_mnemonic];
           return cursor;
           }
   };
}
}
#endif
