/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/ScratchRegisterManager.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"

TR_ScratchRegisterManager::TR_ScratchRegisterManager(int32_t capacity, TR::CodeGenerator *cg) :
   _capacity(capacity),
   _cg(cg),
   _msrList(cg->comp()->trMemory()),
   _cursor(0)
   {}

bool TR_ScratchRegisterManager::donateScratchRegister(TR::Register *reg)
   {
   if (_cursor >= _capacity)
      return false;

   TR_ManagedScratchRegister *msr = new (_cg->trHeapMemory()) TR_ManagedScratchRegister(reg, msrDonated);
   _msrList.add(msr);
   _cursor++;
   return true;
   }

bool TR_ScratchRegisterManager::reclaimScratchRegister(TR::Register *reg)
   {

   if (reg == NULL)
      {
      return false;
      }

   ListIterator<TR_ManagedScratchRegister> iterator(&_msrList);
   TR_ManagedScratchRegister *msr = iterator.getFirst();

   while (msr)
      {
      if (msr->_reg == reg)
         {
         msr->_state &= ~msrAllocated;
         return true;
         }

      msr = iterator.getNext();
      }

   return false;
   }


TR::Register *TR_ScratchRegisterManager::findOrCreateScratchRegister(TR_RegisterKinds rk)
   {
   // Check for free registers in the list.  If there are none then create
   // one if there is enough room.
   //
   ListIterator<TR_ManagedScratchRegister> iterator(&_msrList);
   TR_ManagedScratchRegister *msr = iterator.getFirst();

   while (msr)
      {
      if (msr->_reg->getKind() == rk && !(msr->_state & msrAllocated))
         {
         msr->_state |= msrAllocated;
         return msr->_reg;
         }

      msr = iterator.getNext();
      }

   if (_cursor >= _capacity)
      {
      TR_ASSERT_FATAL(false, "ERROR: cannot allocate any more scratch registers");
      return NULL;
      }

   TR::Register *reg = _cg->allocateRegister(rk);
   msr = new (_cg->trHeapMemory()) TR_ManagedScratchRegister(reg, msrAllocated);
   _msrList.add(msr);
   _cursor++;
   return reg;
   }


void TR_ScratchRegisterManager::addScratchRegistersToDependencyList(
   TR::RegisterDependencyConditions *deps)
   {
   ListIterator<TR_ManagedScratchRegister> iterator(&_msrList);
   TR_ManagedScratchRegister *msr = iterator.getFirst();

   while (msr)
      {
      deps->unionNoRegPostCondition(msr->_reg, _cg);
      msr = iterator.getNext();
      }
   }

// Terminate the live range of non-donated registers.
//
void TR_ScratchRegisterManager::stopUsingRegisters()
   {
   ListIterator<TR_ManagedScratchRegister> iterator(&_msrList);
   TR_ManagedScratchRegister *msr = iterator.getFirst();

   while (msr)
      {
      if (!(msr->_state & msrDonated))
         {
         _cg->stopUsingRegister(msr->_reg);
         }

      msr = iterator.getNext();
      }
   }
