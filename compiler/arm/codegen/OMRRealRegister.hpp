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

#ifndef OMR_ARM_REAL_REGISTER_INCL
#define OMR_ARM_REAL_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR
   namespace OMR { namespace ARM { class RealRegister; } }
   namespace OMR { typedef OMR::ARM::RealRegister RealRegisterConnector; }
#else
   #error OMR::ARM::RealRegister expected to be a primary connector, but a OMR connector is already defined
#endif

// currently TR doesn't have a generic component for real register, so derive from OMR directly
#include "compiler/codegen/OMRRealRegister.hpp"


namespace OMR
{

namespace ARM
{

class OMR_EXTENSIBLE RealRegister : public OMR::RealRegister
   {
   protected:

   RealRegister(TR::CodeGenerator *cg) : OMR::RealRegister(cg, NoReg) {}

   RealRegister(TR_RegisterKinds rk, uint16_t  w, RegState s, RegNum rn, RegMask m, TR::CodeGenerator * cg) :
          OMR::RealRegister(rk, w, s, (uint16_t)0, rn, m, cg) /* , _useVSR(false) */ {}

   public:

   typedef enum  {
      pos_RD     = 12,  // ARM
      pos_RN     = 16,  // ARM
      pos_RM     = 0,   // ARM
      pos_RS     = 8,   // ARM
      pos_RT     = 21,  // ARM
      pos_D      = 22,  // ARM
      pos_N      = 7,   // ARM
      pos_M      = 5    // ARM
   } ARMOperandPosition;

   void setRegisterFieldRS(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RS;
      }

   void setRegisterFieldRT(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RT;
      }

   void setRegisterFieldRD(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RD;
      }

   void setRegisterFieldRN(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RN;
      }

   void setRegisterFieldRM(uint32_t *instruction)
      {
           *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RM;
      }

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   void setRegisterFieldSD(uint32_t *instruction)
      {
           *instruction |= (((fullRegBinaryEncodings[_registerNumber] >> 1) << pos_RD) |
                            ((fullRegBinaryEncodings[_registerNumber] & 1) << pos_D)) ;
      }
   void setRegisterFieldSM(uint32_t *instruction)
      {
           *instruction |= (((fullRegBinaryEncodings[_registerNumber] >> 1) << pos_RM) |
                            ((fullRegBinaryEncodings[_registerNumber] & 1) << pos_M)) ;
      }
   void setRegisterFieldSN(uint32_t *instruction)
      {
           *instruction |= (((fullRegBinaryEncodings[_registerNumber] >> 1) << pos_RN) |
                            ((fullRegBinaryEncodings[_registerNumber] & 1) << pos_N)) ;
      }
#endif

   private:

   static const uint8_t fullRegBinaryEncodings[NumRegisters];

   };

}

}

#endif
