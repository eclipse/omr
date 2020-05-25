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
#include "env/Processors.hpp"

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

   /** \brief
    *     Defines various metadata of an instruction including the name, opcodes, format, the minimum architecture
    *     level set (ALS) which introduced the instruction, and the various properties which model the instruction
    *     in a way that the code generator understands.
    */
   struct OpCodeMetaData
      {
      /** \brief
       *     The instruction mnemonic.
       */
      OMR::InstOpCode::Mnemonic mnemonic;

      /** \brief
       *     The instruction mnemonic as defined by the Power ISA.
       */
      const char* name;

      /** \brief
       *     The instruction opcode with fields masked out by zeros.
       */
      uint32_t opcode;

      /** \brief
       *     The instruction format, defining the fields filled by the binary encoder.
       */
      PPCInstructionFormat format;

      /** \brief
       *     The minimum architecture level set (ALS) which introduced this instruction.
       */
      TR_Processor minimumALS;

      /** \brief
       *     The properties describing the behavior of this instruction to the codegen.
       */
      uint32_t properties;
      };

   static const OpCodeMetaData metadata[NumOpCodes];

   bool isRecordForm() {return (metadata[_mnemonic].properties & PPCOpProp_IsRecordForm)!=0;}
   bool hasRecordForm() {return (metadata[_mnemonic].properties & PPCOpProp_HasRecordForm)!=0;}

   bool singleFPOp() {return (metadata[_mnemonic].properties & PPCOpProp_SingleFP)!=0;}
   bool doubleFPOp() {return (metadata[_mnemonic].properties & PPCOpProp_DoubleFP)!=0;}
   bool gprOp() {return (metadata[_mnemonic].properties & (PPCOpProp_DoubleFP | PPCOpProp_SingleFP))==0;}
   bool fprOp() {return (metadata[_mnemonic].properties & (PPCOpProp_DoubleFP | PPCOpProp_SingleFP))!=0;}

   bool useAlternateFormat() {return (metadata[_mnemonic].properties & PPCOpProp_AltFormat)!=0;}
   bool useAlternateFormatx() {return (metadata[_mnemonic].properties & PPCOpProp_AltFormatx)!=0;}

   bool readsCarryFlag() {return (metadata[_mnemonic].properties & PPCOpProp_ReadsCarryFlag)!=0;}
   bool setsCarryFlag() {return (metadata[_mnemonic].properties & PPCOpProp_SetsCarryFlag)!=0;}

   bool setsOverflowFlag() {return (metadata[_mnemonic].properties & PPCOpProp_SetsOverflowFlag)!=0;}

   bool usesCountRegister() {return (metadata[_mnemonic].properties & PPCOpProp_UsesCtr)!=0;}
   bool setsCountRegister() {return (metadata[_mnemonic].properties & PPCOpProp_SetsCtr)!=0;}

   bool isBranchOp() {return (metadata[_mnemonic].properties & PPCOpProp_BranchOp)!=0;}
   bool isLoad() {return (metadata[_mnemonic].properties & PPCOpProp_IsLoad)!=0;}
   bool isStore() {return (metadata[_mnemonic].properties & PPCOpProp_IsStore)!=0;}
   bool isRegCopy() {return (metadata[_mnemonic].properties & PPCOpProp_IsRegCopy)!=0;}
   bool isUpdate() {return (metadata[_mnemonic].properties & PPCOpProp_UpdateForm)!=0;}
   bool isDoubleWord() {return (metadata[_mnemonic].properties & PPCOpProp_DWord)!=0;}
   bool isCall() {return _mnemonic==bl;}
   bool isTrap() {return (metadata[_mnemonic].properties & PPCOpProp_Trap)!=0;}
   bool isTMAbort() {return (metadata[_mnemonic].properties & PPCOpProp_TMAbort)!=0;}

   bool isFloat() {return (metadata[_mnemonic].properties & (PPCOpProp_SingleFP|PPCOpProp_DoubleFP))!=0;}
   bool isVMX() {return (metadata[_mnemonic].properties & PPCOpProp_IsVMX)!=0;}
   bool isVSX() {return (metadata[_mnemonic].properties & PPCOpProp_IsVSX)!=0;}

   bool usesTarget() {return (metadata[_mnemonic].properties & PPCOpProp_UsesTarget)!=0;}
   bool useMaskEnd() {return (metadata[_mnemonic].properties & PPCOpProp_UseMaskEnd)!=0;}

   bool isRotateOrShift() {return (metadata[_mnemonic].properties & PPCOpProp_IsRotateOrShift)!=0;}
   bool isCompare() {return (metadata[_mnemonic].properties & PPCOpProp_CompareOp)!=0;}

   bool readsFPSCR() {return (metadata[_mnemonic].properties & PPCOpProp_ReadsFPSCR)!=0;}
   bool setsFPSCR() {return (metadata[_mnemonic].properties & PPCOpProp_SetsFPSCR)!=0;}

   bool isSyncSideEffectFree() {return (metadata[_mnemonic].properties & PPCOpProp_SyncSideEffectFree)!=0;}

   bool offsetRequiresWordAlignment() { return (metadata[_mnemonic].properties & PPCOpProp_OffsetRequiresWordAlignment)!=0;}

   bool setsCTR() {return (metadata[_mnemonic].properties & PPCOpProp_SetsCtr)!=0;}
   bool usesCTR() {return (metadata[_mnemonic].properties & PPCOpProp_UsesCtr)!=0;}

   bool isCRLogical() {return (metadata[_mnemonic].properties & PPCOpProp_CRLogical)!=0;}

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
                          _mnemonic==proc     ||
                          _mnemonic==assocreg ||
                          _mnemonic==dd;}

   static const uint32_t getOpCodeBinaryEncoding(Mnemonic opCode)
      {return metadata[opCode].opcode;}
   const uint32_t getOpCodeBinaryEncoding()
      {return getOpCodeBinaryEncoding(_mnemonic);}

   const OpCodeMetaData& getMetaData() { return metadata[_mnemonic]; }
   PPCInstructionFormat getFormat() { return getMetaData().format; }
   const char* getMnemonicName() { return getMetaData().name; }

   int8_t getBinaryLength()
      {
      switch (getFormat())
         {
         case FORMAT_NONE:
            return 0;
         default:
            return 4;
         }
      }

   int8_t getMaxBinaryLength()
      {
      return getBinaryLength();
      }

   uint8_t *copyBinaryToBuffer(uint8_t *cursor)
      {
      switch (getFormat())
         {
         case FORMAT_NONE:
            break;
         default:
            *reinterpret_cast<uint32_t*>(cursor) = metadata[_mnemonic].opcode;
            break;
         }
      return cursor;
      }
   };
}
}
#endif
