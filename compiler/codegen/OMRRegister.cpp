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

#include "codegen/Register.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/RealRegister.hpp"
#include "compile/Compilation.hpp"
#include "ras/Debug.hpp"

OMR::Register::Register(uint32_t f):
   _flags(f),
   _backingStorage(NULL),
   _pinningArrayPointer(NULL),
   _assignedRegister(NULL),
   _siblingRegister(NULL),
   _startOfRange(NULL),
   _endOfRange(NULL),
   _startOfRangeNode(NULL),
   _totalUseCount(0),
   _futureUseCount(0),
   _outOfLineUseCount(0),
   _association(0),
   _kind(TR_GPR),
   _index(0)
   {}

OMR::Register::Register(TR_RegisterKinds rk):
   _flags(0),
   _backingStorage(NULL),
   _pinningArrayPointer(NULL),
   _assignedRegister(NULL),
   _siblingRegister(NULL),
   _startOfRange(NULL),
   _endOfRange(NULL),
   _startOfRangeNode(NULL),
   _totalUseCount(0),
   _futureUseCount(0),
   _outOfLineUseCount(0),
   _association(0),
   _kind(rk),
   _index(0)
   {}

OMR::Register::Register(TR_RegisterKinds rk, uint16_t ar):
   _flags(0),
   _backingStorage(NULL),
   _pinningArrayPointer(NULL),
   _assignedRegister(NULL),
   _siblingRegister(NULL),
   _startOfRange(NULL),
   _endOfRange(NULL),
   _startOfRangeNode(NULL),
   _totalUseCount(0),
   _futureUseCount(0),
   _outOfLineUseCount(0),
   _association(ar),
   _kind(rk),
   _index(0)
   {}

TR::Register*
OMR::Register::self()
    {
    return static_cast<TR::Register*>(this);
    }

ncount_t
OMR::Register::getTotalUseCount()
   {
   TR_SSR_ASSERT();
   return _totalUseCount;
   }

ncount_t
OMR::Register::setTotalUseCount(ncount_t tuc)
   {
   TR_SSR_ASSERT();
   return (_totalUseCount = tuc);
   }

ncount_t
OMR::Register::incTotalUseCount(ncount_t tuc)
   {
   TR_SSR_ASSERT();
   TR_MAX_USECOUNT_ASSERT(tuc);
   return (_totalUseCount += tuc);
   }

ncount_t
OMR::Register::decTotalUseCount(ncount_t tuc)
   {
   TR_SSR_ASSERT();
   return (_totalUseCount -= tuc);
   }

TR::RealRegister *
OMR::Register::getAssignedRealRegister()
   {
   return self()->getAssignedRegister() ? self()->getAssignedRegister()->getRealRegister() : NULL;
   }

TR::Register *
OMR::Register::getRegister()
   {
   return self(); //return subclass ptr
   }

void
OMR::Register::block()
   {
   if (self()->getAssignedRegister() != NULL)
      {
      TR::RealRegister *assignedReg = self()->getAssignedRegister()->getRealRegister();
      if (assignedReg != NULL)
         {
         if (assignedReg->getState() == TR::RealRegister::Assigned)
            {
            assignedReg->setState(TR::RealRegister::Blocked);
            }
         }
      }
   }


void
OMR::Register::unblock()
   {
   if (self()->getAssignedRegister() != NULL)
      {
      TR::RealRegister *assignedReg = self()->getAssignedRegister()->getRealRegister();
      if (assignedReg != NULL)
         {
         if (assignedReg->getState() == TR::RealRegister::Blocked)
            {
            assignedReg->setState(TR::RealRegister::Assigned, self()->isPlaceholderReg());
            }
         }
      }
   }


const char *
OMR::Register::getRegisterName(TR::Compilation *comp, TR_RegisterSizes size)
   {
   if (comp->getDebug())
      return comp->getDebug()->getName(self(), size);
   return "unknown";
   }


const char *
OMR::Register::getRegisterKindName(TR::Compilation *comp, TR_RegisterKinds rk)
   {
   if (comp->getDebug())
      return comp->getDebug()->getRegisterKindName(rk);
   return "??R";
   }

void
OMR::Register::setContainsInternalPointer()
    {
    TR_ASSERT(!self()->containsCollectedReference(), "assertion failure");
    _flags.set(ContainsInternalPointer);
    }

#if defined(DEBUG)
void
OMR::Register::print(TR::Compilation *comp, TR::FILE *pOutFile, TR_RegisterSizes size)
   {
   comp->getDebug()->print(pOutFile, self(), size);
   }
#endif


#if defined(DEBUG)
const char *
TR_RematerializationInfo::toString(TR::Compilation *comp)
   {
   return comp->getDebug()->toString(this);
   }
#endif
