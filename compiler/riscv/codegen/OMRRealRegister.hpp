/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef OMR_RV_REAL_REGISTER_INCL
#define OMR_RV_REAL_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR
   namespace OMR { namespace RV { class RealRegister; } }
   namespace OMR { typedef OMR::RV::RealRegister RealRegisterConnector; }
#else
   #error OMR::RV::RealRegister expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRRealRegister.hpp"
#include "infra/Annotations.hpp"
#include "infra/Assert.hpp"


namespace TR { class CodeGenerator; }


namespace OMR
{

namespace RV
{

class OMR_EXTENSIBLE RealRegister : public OMR::RealRegister
   {
   protected:

   /**
    * @param[in] cg : the TR::CodeGenerator object
    */
   RealRegister(TR::CodeGenerator *cg) : OMR::RealRegister(cg, NoReg) {}

   /**
    * @param[in] rk : kind of real register from TR_RegisterKinds
    * @param[in] w : register weight
    * @param[in] s : register state from RegState
    * @param[in] rn : register number from RegNum
    * @param[in] m : register mask from RegMask
    * @param[in] cg : the TR::CodeGenerator object
    */
   RealRegister(TR_RegisterKinds rk, uint16_t w, RegState s, RegNum rn, RegMask m, TR::CodeGenerator *cg) :
          OMR::RealRegister(rk, w, s, (uint16_t)0, rn, m, cg) {}

   public:

   /**
    * @brief Return binary encoding of the register
    * @return: register binary encoding
    */
   uint32_t binaryRegCode()
      {
      return (uint32_t)fullRegBinaryEncodings[_registerNumber];
      }

   private:

   static const uint8_t fullRegBinaryEncodings[NumRegisters];

   };

}

}

#endif
