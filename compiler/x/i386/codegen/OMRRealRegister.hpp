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

#ifndef OMR_X86_I386_REAL_REGISTER_INCL
#define OMR_X86_I386_REAL_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR
namespace OMR { namespace X86 { namespace i386 { class RealRegister; } } }
namespace OMR { typedef OMR::X86::i386::RealRegister RealRegisterConnector; }
#else
#error OMR::X86::i386::RealRegister expected to be a primary connector, but a OMR connector is already defined
#endif

#include "x/codegen/OMRRealRegister.hpp"

#include <stdint.h>                       // for uint8_t, uint16_t, etc
#include "codegen/RegisterConstants.hpp"  // for TR_RegisterKinds

namespace TR { class CodeGenerator; }

namespace OMR
{

namespace X86
{

namespace i386
{

class OMR_EXTENSIBLE RealRegister : public OMR::X86::RealRegister
   {
   protected:

   RealRegister(TR::CodeGenerator *cg) : OMR::X86::RealRegister(cg) {}

   RealRegister(TR_RegisterKinds   rk,
                uint16_t           w,
                RegState           s,
                RegNum             ri,
                RegMask            m,
                TR::CodeGenerator *cg) :
      OMR::X86::RealRegister(rk, w, s, ri, m, cg) {}

   public:

   static RegNum mmIndex(uint8_t r)
      {
      switch(r)
         {
         case 0:
            return OMR::RealRegister::mm0;
         case 1:
            return OMR::RealRegister::mm1;
         case 2:
            return OMR::RealRegister::mm2;
         case 3:
            return OMR::RealRegister::mm3;
         case 4:
            return OMR::RealRegister::mm4;
         case 5:
            return OMR::RealRegister::mm5;
         case 6:
            return OMR::RealRegister::mm6;
         case 7:
            return OMR::RealRegister::mm7;
         default:
            TR_ASSERT(false, "mmIndex is only valid for registers mm0 to mm7");
            return OMR::RealRegister::NoReg;
         }
      }

   static RegNum xmmIndex(uint8_t r)
      {
      switch(r)
         {
         case 0:
            return OMR::RealRegister::xmm0;
         case 1:
            return OMR::RealRegister::xmm1;
         case 2:
            return OMR::RealRegister::xmm2;
         case 3:
            return OMR::RealRegister::xmm3;
         case 4:
            return OMR::RealRegister::xmm4;
         case 5:
            return OMR::RealRegister::xmm5;
         case 6:
            return OMR::RealRegister::xmm6;
         case 7:
            return OMR::RealRegister::xmm7;
         default:
            TR_ASSERT(false, "xmmIndex is only valid for registers xmm0 to xmm7");
            return OMR::RealRegister::NoReg;
         }
      }

   static RegMask gprMask(RegNum idx)
      {
      switch(idx)
         {
         case OMR::RealRegister::NoReg:
            return OMR::RealRegister::noRegMask;
         case OMR::RealRegister::eax:
            return OMR::RealRegister::eaxMask;
         case OMR::RealRegister::ebx:
            return OMR::RealRegister::ebxMask;
         case OMR::RealRegister::ecx:
            return OMR::RealRegister::ecxMask;
         case OMR::RealRegister::edx:
            return OMR::RealRegister::edxMask;
         case OMR::RealRegister::edi:
            return OMR::RealRegister::ediMask;
         case OMR::RealRegister::esi:
            return OMR::RealRegister::esiMask;
         case OMR::RealRegister::ebp:
            return OMR::RealRegister::ebpMask;
         case OMR::RealRegister::esp:
            return OMR::RealRegister::espMask;
         default:
            TR_ASSERT(false, "gprMask is only valid for registers eax to esp");
            return OMR::RealRegister::noRegMask;
         }
      }

   static RegMask fprMask(RegNum idx)
      {
      switch(idx)
         {
         case OMR::RealRegister::NoReg:
            return OMR::RealRegister::noRegMask;
         case OMR::RealRegister::st0:
            return OMR::RealRegister::st0Mask;
         case OMR::RealRegister::st1:
            return OMR::RealRegister::st1Mask;
         case OMR::RealRegister::st2:
            return OMR::RealRegister::st2Mask;
         case OMR::RealRegister::st3:
            return OMR::RealRegister::st3Mask;
         case OMR::RealRegister::st4:
            return OMR::RealRegister::st4Mask;
         case OMR::RealRegister::st5:
            return OMR::RealRegister::st5Mask;
         case OMR::RealRegister::st6:
            return OMR::RealRegister::st6Mask;
         case OMR::RealRegister::st7:
            return OMR::RealRegister::st7Mask;
         default:
            TR_ASSERT(false, "fprMask is only valid for registers st0 to st7");
            return OMR::RealRegister::noRegMask;
         }
      }

   static RegMask mmrMask(RegNum idx)
      {
      switch(idx)
         {
         case OMR::RealRegister::NoReg:
            return OMR::RealRegister::noRegMask;
         case OMR::RealRegister::mm0:
            return OMR::RealRegister::mm0Mask;
         case OMR::RealRegister::mm1:
            return OMR::RealRegister::mm1Mask;
         case OMR::RealRegister::mm2:
            return OMR::RealRegister::mm2Mask;
         case OMR::RealRegister::mm3:
            return OMR::RealRegister::mm3Mask;
         case OMR::RealRegister::mm4:
            return OMR::RealRegister::mm4Mask;
         case OMR::RealRegister::mm5:
            return OMR::RealRegister::mm5Mask;
         case OMR::RealRegister::mm6:
            return OMR::RealRegister::mm6Mask;
         case OMR::RealRegister::mm7:
            return OMR::RealRegister::mm7Mask;
         default:
            TR_ASSERT(false, "mmrMask is only valid for registers mm0 to mm7");
            return OMR::RealRegister::noRegMask;
         }
      }

   static RegMask xmmrMask(RegNum idx)
      {
      switch(idx)
         {
         case OMR::RealRegister::NoReg:
            return OMR::RealRegister::noRegMask;
         case OMR::RealRegister::xmm0:
            return OMR::RealRegister::xmm0Mask;
         case OMR::RealRegister::xmm1:
            return OMR::RealRegister::xmm1Mask;
         case OMR::RealRegister::xmm2:
            return OMR::RealRegister::xmm2Mask;
         case OMR::RealRegister::xmm3:
            return OMR::RealRegister::xmm3Mask;
         case OMR::RealRegister::xmm4:
            return OMR::RealRegister::xmm4Mask;
         case OMR::RealRegister::xmm5:
            return OMR::RealRegister::xmm5Mask;
         case OMR::RealRegister::xmm6:
            return OMR::RealRegister::xmm6Mask;
         case OMR::RealRegister::xmm7:
            return OMR::RealRegister::xmm7Mask;
         default:
            TR_ASSERT(false, "xmmrMask is only valid for registers xmm0 to xmm7");
            return OMR::RealRegister::noRegMask;
         }
      }


   void setRegisterNumber() { TR_ASSERT(0, "X86 RealRegister doesn't have setRegisterNumber() implementation"); }

   void setRegisterFieldInOpcode(uint8_t *opcodeByte)
      {
      *opcodeByte |= _fullRegisterBinaryEncodings[_registerNumber].id; // reg field is in bits 0-2 of opcode
      }

   void setRegisterFieldInModRM(uint8_t *modRMByte)
      {
      *modRMByte |= _fullRegisterBinaryEncodings[_registerNumber].id << 3; // reg field is in bits 3-5 of ModRM byte
      }

   void setRMRegisterFieldInModRM(uint8_t *modRMByte)
      {
      *modRMByte |= _fullRegisterBinaryEncodings[_registerNumber].id; // RM field is in bits 0-2 of ModRM byte
      }

   void setBaseRegisterFieldInSIB(uint8_t *SIBByte)
      {
      *SIBByte |= _fullRegisterBinaryEncodings[_registerNumber].id; // base register field is in bits 0-2 of SIB byte
      }

   void setIndexRegisterFieldInSIB(uint8_t *SIBByte)
      {
      *SIBByte |= _fullRegisterBinaryEncodings[_registerNumber].id << 3; // index register field is in bits 3-5 SIB byte
      }

   uint8_t rexBits(uint8_t rxbBits, bool isByteOperand) { return 0; }

   // (AMD64: see x86-64 Architecture Programmer's Manual, Volume 3, section 1.7.2.
   bool needsDisp() { return _fullRegisterBinaryEncodings[_registerNumber].needsDisp; }
   bool needsSIB()  { return _fullRegisterBinaryEncodings[_registerNumber].needsSIB;  }

   private:

   // TODO: Consider making this back into a plain old byte for consistency with other platforms.
   static const struct TR_RegisterBinaryEncoding _fullRegisterBinaryEncodings[NumRegisters];
   };

}

}

}

#endif
