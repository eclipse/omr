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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"TRZRegBase#C")
#pragma csect(STATIC,"TRZRegtBase#S")
#pragma csect(TEST,"TRZRegBase#T")

#include <stddef.h>                       // for NULL
#include <stdint.h>                       // for uint16_t, uint32_t
#include "codegen/Register.hpp"           // for RegisterExt, Register, etc
#include "codegen/RegisterConstants.hpp"  // for TR_RegisterKinds, etc
#include "infra/Flags.hpp"                // for flags32_t

OMR::Z::Register::Register(uint32_t f): OMR::Register(f)
   { _liveRegisterInfo._liveRegister = NULL; _memRef=NULL;}

OMR::Z::Register::Register(TR_RegisterKinds rk): OMR::Register(rk)
   { _liveRegisterInfo._liveRegister = NULL; _memRef=NULL;}

OMR::Z::Register::Register(TR_RegisterKinds rk, uint16_t ar): OMR::Register(rk,ar)
   { _liveRegisterInfo._liveRegister = NULL; _memRef=NULL;}


bool
OMR::Z::Register::isArGprPair()
   {
   if (self()->getRegisterPair() &&
         self()->getLowOrder()->getKind() == TR_AR &&
         (self()->getHighOrder()->getKind() == TR_GPR || self()->getHighOrder()->getKind() == TR_GPR64))
   return true;

   return false;
   }

bool
OMR::Z::Register::usesRegister(TR::Register* reg)
   {
   return (reg == self())? true : false;
   }

bool OMR::Z::Register::usesAnyRegister(TR::Register *reg)
   {
   if (self()->getRegisterPair())
      {
      if (reg->getRegisterPair())
         return (reg == self());
      else
         return (self()->getHighOrder()->usesRegister(reg) || self()->getLowOrder()->usesRegister(reg));
      }
   else
      {
      if (reg->getRegisterPair())
         return (self()->usesRegister(reg->getHighOrder()) || self()->usesRegister(reg->getLowOrder()));
      else
         return (reg == self());
      }
   }

TR::Register *
OMR::Z::Register::getARofArGprPair()
   {
   return self()->getLowOrder();
   }

TR::Register *
OMR::Z::Register::getGPRofArGprPair()
   {
   if (self()->isArGprPair()) return self()->getHighOrder(); else return self();
   }


void
OMR::Z::Register::setPlaceholderReg()
   {
#if defined(TR_TARGET_64BIT)
   self()->setIs64BitReg(true);
#endif
   OMR::Register::setPlaceholderReg();
   }

bool
OMR::Z::Register::assignToHPR()
   {
   return (!(self()->is64BitReg()) && _flags.testAny(AssignToHPR));
   }

bool
OMR::Z::Register::assignToGPR()
   {
   return (!(self()->is64BitReg()) && !_flags.testAny(AssignToHPR));
   }

bool
OMR::Z::Register::isHighWordUpgradable()
   {
      return (!_flags.testAny(IsNotHighWordUpgradable) &&
               !self()->isUsedInMemRef() &&
               !self()->containsCollectedReference() &&
               !self()->containsInternalPointer() &&
               !self()->getRegisterPair() &&
               (self()->getKind() == TR_GPR) &&
               !self()->is64BitReg());
   }

ncount_t
OMR::Z::Register::decFutureUseCount(ncount_t fuc)
   {
   self()->resetNotUsedInThisBB();
   return OMR::Register::decFutureUseCount(fuc);
   }

bool
OMR::Z::Register::containsCollectedReference()
   {
   return OMR::Register::containsCollectedReference();
   }


void
OMR::Z::Register::setContainsCollectedReference()
   {
   _flags.set(ContainsCollectedReference);
   }

#if   defined(TR_TARGET_64BIT)
   bool OMR::Z::Register::is64BitReg()
         {return (_flags.testAny(Is64Bit) ||
                  self()->containsCollectedReference() ||
                  self()->containsInternalPointer());}
#else
   bool OMR::Z::Register::is64BitReg()  {return _flags.testAny(Is64Bit);}
#endif
