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

#ifndef OMR_X86_REGISTER_INCL
#define OMR_X86_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_CONNECTOR
#define OMR_REGISTER_CONNECTOR
namespace OMR { namespace X86 { class Register; } }
namespace OMR { typedef OMR::X86::Register RegisterConnector; }
#else
#error OMR::X86::Register expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegister.hpp"

#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

class TR_LiveRegisterInfo;
class TR_RematerializationInfo;
namespace TR { class MemoryReference; }

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE Register: public OMR::Register
   {
   protected:

   Register(uint32_t f=0): OMR::Register(f), _memRef(NULL),_rematerializationInfo(NULL) {_liveRegisterInfo._liveRegister = NULL;}
   Register(TR_RegisterKinds rk): OMR::Register(rk), _memRef(NULL), _rematerializationInfo(NULL) {_liveRegisterInfo._liveRegister = NULL;}
   Register(TR_RegisterKinds rk, uint16_t ar): OMR::Register(rk, ar), _memRef(NULL), _rematerializationInfo(NULL) {_liveRegisterInfo._liveRegister = NULL;}


   public:

   /*
    * Getter/setters
    */
   TR_LiveRegisterInfo *getLiveRegisterInfo()                       {return _liveRegisterInfo._liveRegister;}
   TR_LiveRegisterInfo *setLiveRegisterInfo(TR_LiveRegisterInfo *p) {return (_liveRegisterInfo._liveRegister = p);}

   TR_RematerializationInfo *getRematerializationInfo() { return _rematerializationInfo; }
   TR_RematerializationInfo *setRematerializationInfo(TR_RematerializationInfo *d) { return (_rematerializationInfo = d); }

   TR::MemoryReference *getMemRef() { return _memRef; }
   void setMemRef(TR::MemoryReference *memRef) { _memRef = memRef; }

   uint32_t getInterference()           {return _liveRegisterInfo._interference;}
   uint32_t setInterference(uint32_t i) {return (_liveRegisterInfo._interference = i);}


   /*
    * Method for manipulating flags
    */
   bool needsPrecisionAdjustment()      {return _flags.testAny(NeedsPrecisionAdjustment);}
   void setNeedsPrecisionAdjustment();
   void resetNeedsPrecisionAdjustment() {_flags.reset(NeedsPrecisionAdjustment);}

   bool mayNeedPrecisionAdjustment()      {return _flags.testAny(MayNeedPrecisionAdjustment);}
   void setMayNeedPrecisionAdjustment()   {_flags.set(MayNeedPrecisionAdjustment);}
   void resetMayNeedPrecisionAdjustment();

   bool hasBetterSpillPlacement()          {return _flags.testAny(HasBetterSpillPlacement);}
   void setHasBetterSpillPlacement(bool v) {if (v) _flags.set(HasBetterSpillPlacement); else _flags.reset(HasBetterSpillPlacement);}

   bool isAssignedAsByteRegister()    {return _flags.testAny(ByteRegisterAssigned);}
   void setAssignedAsByteRegister(bool v) {if (v) _flags.set(ByteRegisterAssigned); else _flags.reset(ByteRegisterAssigned);}

   bool isDiscardable()                     {return _flags.testAny(IsDiscardable);}
   void setIsDiscardable()                  {_flags.set(IsDiscardable);}
   void resetIsDiscardable()                {_flags.reset(IsDiscardable);}

   bool needsLazyClobbering()                 {return _flags.testAny(NeedsLazyClobbering);}
   void setNeedsLazyClobbering(bool b = true) {_flags.set(NeedsLazyClobbering, b);}

   bool areUpperBitsZero()                  {return _flags.testAny(UpperBitsAreZero);}
   void setUpperBitsAreZero(bool b = true)  {_flags.set(UpperBitsAreZero, b);}

   bool isSpilledToSecondHalf()                 {return _flags.testAny(SpilledToSecondHalf);}
   void setIsSpilledToSecondHalf(bool b = true) {_flags.set(SpilledToSecondHalf, b);}


   private:

   enum
      {
      NeedsPrecisionAdjustment      = 0x0002, // IA32 flag that indicates that a store/reload of
                                              // the value in this register is required before
                                              // it can be used in further computations. Required
                                              // for single-precision FP registers (unless method
                                              // is in single precision mode).
      MayNeedPrecisionAdjustment    = 0x0004, // IA32 flag that indicates that a store/reload of
                                              // the value in this register may be required before
                                              // it can be used in an FP-strict expression.
      NeedsLazyClobbering           = 0x0800, // X86 service releases only (well, originally the intent): node refcount = 1 is not enough to determine that this can be clobbered; must also check register node count
      UpperBitsAreZero              = 0x2000, // AMD64 many 32bit operations clear the upper bits
      HasBetterSpillPlacement       = 0x0100,
      SpilledToSecondHalf           = 0x4000, // Spilled at an offset starting at the middle of the spill slot
      IsDiscardable                 = 0x0020, // Register is currently discardable
      ByteRegisterAssigned          = 0x0200,
      };

   //Both x and z have this field, but power has own specialization, may move to base
   union
      {
      TR_LiveRegisterInfo *_liveRegister; // Live register entry representing this register
      uint32_t             _interference; // Real registers that interfere with this register
      } _liveRegisterInfo;

   // rematerialization information for this register
   TR_RematerializationInfo *_rematerializationInfo;

   // Both x and z have this, but power doesn't, so duplicating in both x and z
   TR::MemoryReference *_memRef;

   };

}

}

#endif /* OMR_X86_REGISTER_INCL */
