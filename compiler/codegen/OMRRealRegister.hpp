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

#ifndef OMR_REAL_REGISTER_INCL
#define OMR_REAL_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR
namespace OMR { class RealRegister; }
namespace OMR { typedef OMR::RealRegister RealRegisterConnector; }
#endif

#include <stdint.h>                         // for uint16_t, int32_t
#include "codegen/Register.hpp"             // for Register
#include "codegen/RegisterConstants.hpp"    // for TR_RegisterKinds, etc
#include "infra/Flags.hpp"                  // for flags8_t

namespace TR { class CodeGenerator; }
namespace TR { class RealRegister; }

namespace OMR
{

class OMR_EXTENSIBLE RealRegister : public TR::Register
   {

   public:

   TR::RealRegister* self();

   typedef enum
      {
      Free      = 0,
      Unlatched = 1,
      Assigned  = 2,
      Blocked   = 3,
      Locked    = 4
      } RegState;

   typedef enum
      {
      #include "codegen/RealRegisterEnum.hpp"

      } RegNum;

   typedef enum
      {
      #include "codegen/RealRegisterMaskEnum.hpp"

      } RegMask;


   protected:

   RealRegister(TR::CodeGenerator *cg, RegNum n);
   RealRegister(TR_RegisterKinds, uint16_t, RegState, uint16_t, RegNum, RegMask, TR::CodeGenerator *);


   public:
   /*
    * Getters/Setters
    */
   uint16_t getWeight() {return _weight;}
   uint16_t setWeight(uint16_t w) { return (_weight = w); }

   RegState getState() {return _state;}
   RegState setState(RegState s, bool assignedToDummy=false); //can not overwrite locked reg
   void resetState(RegState s) {_state = s;} // only call this if overwriting a locked register

   TR::Register *setAssignedRegister(TR::Register *r);

   bool getHasBeenAssignedInMethod()  { return _realRegFlags.testAny(isAssigned);  }
   bool setHasBeenAssignedInMethod(bool b);

   bool getIsFreeOnExit()  { return _realRegFlags.testAny(isFreeOnExit);  }
   void setIsFreeOnExit(bool b=true) { _realRegFlags.set(isFreeOnExit, b); }

   bool getIsAssignedOnce()  { return _realRegFlags.testAny(isAssignedOnce);  }
   void setIsAssignedOnce(bool b=true) { _realRegFlags.set(isAssignedOnce, b); }

   bool getIsAssignedMoreThanOnce()  { return _realRegFlags.testAny(isAssignedMoreThanOnce);  }
   void setIsAssignedMoreThanOnce(bool b=true) { _realRegFlags.set(isAssignedMoreThanOnce, b); }

   bool getIsSpillExtendedOutOfLoop()  { return _realRegFlags.testAny(isSpillExtendedOutOfLoop);  }
   void setIsSpillExtendedOutOfLoop(bool b=true) { _realRegFlags.set(isSpillExtendedOutOfLoop, b); }

   RegMask getRealRegisterMask()       {return _registerMask;}
   RegMask setRealRegisterMask(RegMask m) { return _registerMask = m;}

   RegNum getRegisterNumber() {return _registerNumber;}
   RegNum setRegisterNumber(RegNum rn) {return _registerNumber = rn;}


   /*
    * Other methods specialized in this derived class
    */
   virtual void block();
   virtual void unblock();

   virtual TR::Register     *getRegister();
   virtual TR::RealRegister *getRealRegister();

   static TR_RegisterMask getAvailableRegistersMask(TR_RegisterKinds rk) { return 0; }
   static TR::RealRegister *regMaskToRealRegister(TR_RegisterMask mask, TR_RegisterKinds rk, TR::CodeGenerator *cg) { return NULL; }

   static int32_t getBitPosInMask(TR_RegisterMask mask);

   protected:
   flags8_t        _realRegFlags;
   RegNum _registerNumber;

   private:

   enum
      {
      isAssigned                = 0x01,  // Implies 32-bit reg on 32-bit platform, 64-bit reg on 64-bit platform
      isFreeOnExit              = 0x04,  // Was register free on exit of current inner loop
      isAssignedOnce            = 0x08,  // Was the register assigned only once inside the current loop
      isAssignedMoreThanOnce    = 0x10,  // Was the register assigned more then once inside the current loop
      isSpillExtendedOutOfLoop  = 0x20,  // Was the register load from spill extended to loop pre-entry
      };

   uint16_t        _weight;
   RegState       _state;

   RegMask _registerMask;
   TR::CodeGenerator *_cg;
   };

}

#endif
