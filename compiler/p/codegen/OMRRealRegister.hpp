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

#ifndef OMR_Power_REAL_REGISTER_INCL
#define OMR_Power_REAL_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR
namespace OMR { namespace Power { class RealRegister; } }
namespace OMR { typedef OMR::Power::RealRegister RealRegisterConnector; }
#else
#error OMR::Power::RealRegister expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRealRegister.hpp"

#include <stdint.h>
#include "codegen/RegisterConstants.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class RealRegister; }

namespace OMR
{

namespace Power
{

class OMR_EXTENSIBLE RealRegister : public OMR::RealRegister
   {
   protected:

   RealRegister(TR::CodeGenerator *cg) : OMR::RealRegister(cg, NoReg) {}

   RealRegister(TR_RegisterKinds rk, uint16_t  w, RegState s, RegNum rn, RegMask m, TR::CodeGenerator * cg) :
          OMR::RealRegister(rk, w, s, (uint16_t)0, rn, m, cg), _useVSR(false) {}

   public:

   typedef enum  {
      pos_RT     = 21,
      pos_RS     = 21,
      pos_TO     = 12,
      pos_RA     = 16,
      pos_RB     = 11,
      pos_RC     = 6,
      pos_SH     = 11,
      pos_BF     = 23,
      pos_BI     = 18,
      pos_XBI    = 7,
      pos_MBE    = 5,
      pos_FRD    = 21,
      pos_FRA    = 16,
      pos_FRC    = 6,
      pos_FRB    = 11,
      pos_TX     = 0,
      pos_SX     = 0,
      pos_BX     = 1,
      pos_AX     = 2,
      pos_CX     = 3
   } PPCOperandPosition;

   enum CRCC {
      //Condition Register Condition Code (CRCC)
      CRCC_LT = 0x0,    // less than
      CRCC_GT = 0x1,    // greater than
      CRCC_EQ = 0x2,    // equal
      CRCC_SO = 0x3,    // summary overflow
      CRCC_FU = CRCC_SO // floating-point unordered
   };

   bool getUseVSR() { return _useVSR; }
   void setUseVSR(bool value) { _useVSR = value; }
   bool isNewVSR() { return _registerNumber >= vsr32 && _registerNumber <= LastVSR; }

   bool isConditionRegister() { return ((_registerNumber>=FirstCCR) && (_registerNumber<=LastCCR))?true:false; }

   void setRegisterFieldRT(uint32_t *instruction);

   void setRegisterFieldRS(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RS;
      }

   void setRegisterFieldBI(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_BI;
      }

   void setRegisterFieldRA(uint32_t *instruction);

   void setRegisterFieldRB(uint32_t *instruction);

   void setRegisterFieldRC(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RC;
      }

   void setRegisterFieldFRA(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_FRA;
      }

   void setRegisterFieldFRB(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_FRB;
      }

   void setRegisterFieldFRC(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_FRC;
      }

   void setRegisterFieldFRD(uint32_t *instruction)
      {
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_FRD;
      }

   void setRegisterFieldXS(uint32_t *instruction);
   void setRegisterFieldXT(uint32_t *instruction);
   void setRegisterFieldXA(uint32_t *instruction);
   void setRegisterFieldXB(uint32_t *instruction);
   void setRegisterFieldXC(uint32_t *instruction);

   static TR::RealRegister *regMaskToRealRegister(TR_RegisterMask mask, TR_RegisterKinds rk, TR::CodeGenerator *cg);
   static TR_RegisterMask getAvailableRegistersMask(TR_RegisterKinds rk);

   private:

   bool _useVSR;
   static const uint8_t fullRegBinaryEncodings[NumRegisters];

   };

}

}

#endif
