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

#ifndef OMR_Z_REAL_REGISTER_INCL
#define OMR_Z_REAL_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR
namespace OMR { namespace Z { class RealRegister; } }
namespace OMR { typedef OMR::Z::RealRegister RealRegisterConnector; }
#else
#error OMR::Z::RealRegister expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRealRegister.hpp"

#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "infra/Flags.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class RealRegister; }

namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE RealRegister : public OMR::RealRegister
   {
   protected:

   RealRegister(TR::CodeGenerator *cg);
   RealRegister(TR_RegisterKinds, uint16_t, RegState, RegNum, RegMask, TR::CodeGenerator *);

   public:
      
   // methods for manipulating flags
   bool getModified() {return _modified.testAny(isModified);}
   bool setModified(bool b)  { (b)? _modified.set(isModified) : _modified.reset(isModified);  return b; }
   
   bool setHasBeenAssignedInMethod(bool b); // derived from base class



   // member methods that call static methods which set register field
   void setBaseRegisterField(uint32_t *instruction);
   void setIndexRegisterField(uint32_t *instruction);
   void setRegisterField(uint32_t *instruction);
   void setRegisterField(uint32_t *instruction, int32_t nibbleIndex); // nibbleIndex-> rightmost nibble=0, leftmost nibble=7
   void setRegister1Field(uint32_t *instruction);
   void setRegister2Field(uint32_t *instruction);
   void setRegister3Field(uint32_t *instruction);
   void setRegister4Field(uint32_t *instruction);

   // static methods for setting register field
   static void setRegisterRXBField(uint32_t *instruction, RegNum reg, int index);

   static void setBaseRegisterField(uint32_t *instruction,RegNum reg);
   static void setIndexRegisterField(uint32_t *instruction,RegNum reg);
   static void setRegisterField(uint32_t *instruction,RegNum reg);
   static void setRegisterField(uint32_t *instruction, int32_t nibbleIndex,RegNum reg);

   static void setRegister1Field(uint32_t *instruction,RegNum reg);
   static void setRegister2Field(uint32_t *instruction,RegNum reg);
   static void setRegister3Field(uint32_t *instruction,RegNum reg);
   static void setRegister4Field(uint32_t *instruction,RegNum reg);

   // static methods for checking register type
   static bool isPseudoRealReg(RegNum reg);
   static bool isRealReg(RegNum reg);
   static bool isGPR(RegNum reg);
   static bool isFPR(RegNum reg);
   static bool isVRF(RegNum reg);

   private:

   enum
      {
      isModified      = 0x01,
      };
   
   flags8_t        _modified;
   static const uint8_t _fullRegBinaryEncodings[NumRegisters];

   };

}

}


#endif
