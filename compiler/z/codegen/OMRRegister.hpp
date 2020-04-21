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

#ifndef OMR_Z_REGISTER_INCL
#define OMR_Z_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_CONNECTOR
#define OMR_REGISTER_CONNECTOR
namespace OMR { namespace Z { class Register; } }
namespace OMR { typedef OMR::Z::Register RegisterConnector; }
#else
#error OMR::Z::Register expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegister.hpp"

#include "il/Node.hpp"
#include "infra/Flags.hpp"

class TR_LiveRegisterInfo;
class TR_OpaquePseudoRegister;
class TR_PseudoRegister;
namespace TR { class MemoryReference; }
namespace TR { class Register; }
template <class T> class TR_Queue;

namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE Register: public OMR::Register
   {
   protected:

   Register(uint32_t f=0);
   Register(TR_RegisterKinds rk);
   Register(TR_RegisterKinds rk, uint16_t ar);

   public:

   /*
    * Getters/Setters
    */

   TR_LiveRegisterInfo *getLiveRegisterInfo()                       {return _liveRegisterInfo._liveRegister;}
   TR_LiveRegisterInfo *setLiveRegisterInfo(TR_LiveRegisterInfo *p) {return (_liveRegisterInfo._liveRegister = p);}

   uint32_t getInterference()           {return _liveRegisterInfo._interference;}
   uint32_t setInterference(uint32_t i) {return (_liveRegisterInfo._interference = i);}

   TR::MemoryReference *getMemRef() { return _memRef; }
   void setMemRef(TR::MemoryReference *memRef) { _memRef = memRef; }

   /*
    * Methods for manipulating flags
    */

   bool isUsedInMemRef()                    {return _flags.testAny(IsUsedInMemRef);}
   void setIsUsedInMemRef(bool b = true)    {_flags.set(IsUsedInMemRef, b);}

   bool is64BitReg();
   void setIs64BitReg(bool b = true);

   bool isDependencySet()    {return _flags.testAny(DependencySet);}
   void setDependencySet(bool v) {if (v) _flags.set(DependencySet);}

   bool alreadySignExtended()           {return _flags.testAny(AlreadySignExtended);}
   void setAlreadySignExtended()        {_flags.set(AlreadySignExtended);}
   void resetAlreadySignExtended()      {_flags.reset(AlreadySignExtended);}
   
   /*
    * Overriding Base Class Implementation of these methods
    */
   void setPlaceholderReg();

   ncount_t decFutureUseCount(ncount_t fuc=1);

   bool containsCollectedReference();
   void setContainsCollectedReference();

   /*
    * Methods specialized in derived classes
    */

   virtual bool usesRegister(TR::Register* reg);  //ppc may duplicate this
   virtual bool usesAnyRegister(TR::Register* reg);

   /*
    * Pseudo and Opaque Registers
    */
   virtual TR_OpaquePseudoRegister  *getOpaquePseudoRegister() {return NULL;}
   virtual TR_PseudoRegister  *getPseudoRegister() { return NULL;}

   private:

   enum
      {
         IsUsedInMemRef                = 0x0800, // 390 cannot associate GPR0 to regs used in memrefs
         Is64Bit                       = 0x0002, // 390 flag indicates that this Register contained a 64-bit value
         DependencySet                 = 0x0200,  // 390 flag, post dependancy was assigned

         AlreadySignExtended           = 0x1000, // determine whether i2l should be nops
      };

   //Both x and z have this field, but power has own specialization, may move to base
   union
      {
      TR_LiveRegisterInfo *_liveRegister; // Live register entry representing this register
      uint32_t             _interference; // Real registers that interfere with this register
      } _liveRegisterInfo;

   // Both x and z have this, but power doesn't, so duplicating in both x and z
   TR::MemoryReference *_memRef;
   };
}
}

#endif /* OMR_Z_REGISTER_INCL */
