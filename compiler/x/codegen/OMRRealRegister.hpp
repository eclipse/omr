/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef OMR_X86_REAL_REGISTER_INCL
#define OMR_X86_REAL_REGISTER_INCL

#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR
namespace OMR { namespace X86 { class RealRegister; } }
namespace OMR { typedef OMR::X86::RealRegister RealRegisterConnector; }
#endif

#include "compiler/codegen/OMRRealRegister.hpp"

#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class RealRegister; }

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE RealRegister : public OMR::RealRegister
   {
   protected:

   RealRegister(TR::CodeGenerator *cg) : OMR::RealRegister(cg, NoReg) {}

   RealRegister(TR_RegisterKinds   rk,
                uint16_t           w,
                RegState           s,
                RegNum             ri,
                RegMask            m,
                TR::CodeGenerator *cg) :
      OMR::RealRegister(rk, w, s, (uint16_t)ri, ri, m, cg) {}

   public:

   // Constants for AMD64 Rex prefix calculations
   // TODO:AMD64: Consider putting this in an AMD64Register subclass
   //
   enum { REX=0x40, REX_W=0x08, REX_R=0x04, REX_X=0x02, REX_B=0x01 };

   static TR::RealRegister *regMaskToRealRegister(TR_RegisterMask mask, TR_RegisterKinds rk, TR::CodeGenerator *cg);
   static TR_RegisterMask getAvailableRegistersMask(TR_RegisterKinds rk);
   static RegMask getRealRegisterMask(TR_RegisterKinds rk, RegNum idx);
   using OMR::RealRegister::getRealRegisterMask;  // for getting the _registerMask member

   protected:

   // TODO:AMD64: Consider making this back into a plain old byte for consistency with other platforms.
   struct TR_RegisterBinaryEncoding
      {
      unsigned char id:3;              // 3-bit register identifier used in ModRM and SIB bytes
      unsigned char needsRexPlusRXB:1; // Always need a Rex prefix with the R, X, or B bit set
      unsigned char needsRexForByte:1; // Needs a Rex prefix when used as a byte register
      unsigned char needsDisp:1;       // Needs a displacement field (even if zero) when used as a base reg (ie. ebp/r13)
      unsigned char needsSIB:1;        // Needs a SIB byte when used as a base reg (ie. esp/r12)
      };
   };

}

}

#endif
