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

#include "codegen/LiveRegister.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"

void
TR_LiveRegisters::moveRegToList(TR_LiveRegisters* from, TR_LiveRegisters* to, TR::Register *reg)
   {
   from->removeRegisterFromLiveList(reg);
   reg->getLiveRegisterInfo()->addToList(to->_head);
   to->_numLiveRegisters++;
   }

TR_LiveRegisterInfo *
TR_LiveRegisters::addRegister(TR::Register *reg, bool updateInterferences)
   {
   TR_RegisterKinds rk = reg->getKind();

   TR_LiveRegisterInfo *entry;

   // Create a new live register entry
   //
   if (_pool)
      {
      entry = _pool;
      entry->removeFromList(_pool);
      }
   else
      entry = new (comp()->trHeapMemory()) TR_LiveRegisterInfo(comp());

   entry->initialize(reg);

   reg->setLiveRegisterInfo(entry);

   // Add the entry to the live register chain
   //
   entry->addToList(_head);
   reg->setIsLive();
   _numLiveRegisters++;

#ifdef DEBUG
   if (debug("traceLiveRegisters"))
      {
      diagnostic("Add live register %p for %p while processing node %p, kind %d\n",
                  entry, reg, cg()->getCurrentEvaluationTreeTop()->getNode(), reg->getKind());
      }
#endif

   return entry;
   }

TR_LiveRegisterInfo *
TR_LiveRegisters::addRegisterPair(TR::RegisterPair *reg)
   {
   TR_LiveRegisterInfo *entry = addRegister(reg);
   if (!reg->getLowOrder()->isLive())
      addRegister(reg->getLowOrder());
   if (!reg->getHighOrder()->isLive())
      addRegister(reg->getHighOrder());

   // Don't count the register pair entry in the number of live registers,
   // since it won't contribute to register pressure
   //
   _numLiveRegisters--;

   return entry;
   }

void
TR_LiveRegisters::setAssociation(TR::Register *reg, TR::RealRegister *realReg)
   {
   if (!reg->isLive())
      return;

   TR_LiveRegisterInfo *liveReg = reg->getLiveRegisterInfo();
   TR_RegisterMask      realRegMask = realReg->getRealRegisterMask();

#if defined(TR_TARGET_POWER)
   uint64_t realMask = (uint64_t)realRegMask;
   if (TR_VSX_SCALAR == reg->getKind() || TR_VSX_VECTOR == reg->getKind())
      {
      if (realReg->isNewVSR())
         {
         realMask = realMask << 32;
         }
      }
   liveReg->setAssociation(realMask, comp());
#else
   liveReg->setAssociation(realRegMask, comp());
#endif
   // The real register interferes with all current live virtual registers.
   // Also, all current live virtual registers are now not associated with this
   // real register.
   //
   for (TR_LiveRegisterInfo *p = _head; p; p = p->getNext())
      {
      if (p != liveReg)
         p->addInterference(liveReg->getAssociation());
      }
   }

void
TR_LiveRegisters::setByteRegisterAssociation(TR::Register *reg)
   {
   TR_LiveRegisterInfo *liveReg = reg->getLiveRegisterInfo();

   if (reg->isLive())
      liveReg->addByteRegisterAssociation();

   // Mark all current live virtual registers as interfering with a byte
   // register.
   //
   for (TR_LiveRegisterInfo *p = _head; p; p = p->getNext())
      {
      if (p != liveReg)
         p->addByteRegisterInterference();
      }
   }

void
TR_LiveRegisters::stopUsingRegister(TR::Register *reg)
   {
   if (reg->isLive())
      {
      if (reg->getLiveRegisterInfo()->getNodeCount() == 0)
         registerIsDead(reg);
      }
   }

void
TR_LiveRegisters::registerIsDead(TR::Register *reg, bool updateInterferences)
   {
   if (!reg->isLive())
      return;

   // Remove the live register info from the live register chain.
   //
   TR_LiveRegisterInfo *liveReg = reg->getLiveRegisterInfo();
   liveReg->removeFromList(_head);
   _numLiveRegisters--;

   TR::RegisterPair *regPair = reg->getRegisterPair();
   if (regPair)
      {
      // For a register pair, it may be the case that we've killed one of its components after its
      // last reference to reduce interferences.  Hence, we can relax the liveness constraint for
      // register pair components.
      //
      if (regPair->getLowOrder()->isLive() && (regPair->getLowOrder()->getLiveRegisterInfo()->getNodeCount() == 0))
         {
         if (cg()->getLiveRegisters(regPair->getLowOrder()->getKind()))
            cg()->getLiveRegisters(regPair->getLowOrder()->getKind())->registerIsDead(regPair->getLowOrder(), updateInterferences);
         else
            registerIsDead(regPair->getLowOrder(), updateInterferences);
         }

      if (regPair->getHighOrder()->isLive() && (regPair->getHighOrder()->getLiveRegisterInfo()->getNodeCount() == 0))
         {
         if (cg()->getLiveRegisters(regPair->getHighOrder()->getKind()))
            cg()->getLiveRegisters(regPair->getHighOrder()->getKind())->registerIsDead(regPair->getHighOrder(), updateInterferences);
         else
            registerIsDead(regPair->getHighOrder(), updateInterferences);
         }

      // Don't count the register pair entry in the number of live registers,
      // since it won't contribute to register pressure
      //
      _numLiveRegisters++;
      }
   else
      {
      reg->setInterference(liveReg->getInterference());

      // If this register is associated with a real register, the real register
      // interferes with all current live virtual registers
      //
      if (liveReg->getAssociation())
         {
         for (TR_LiveRegisterInfo *p = _head; p; p = p->getNext())
            p->addInterference(liveReg->getAssociation());
         }
      }

   reg->resetIsLive();
#ifndef TR_TARGET_POWER
   if (!TR::isJ9())
      {
      reg->setStartOfRangeNode(NULL);
      reg->setStartOfRange(NULL);
      }
#endif

   if (debug("traceLiveRegisters"))
      {
#ifdef TR_TARGET_POWER
      uint32_t lowerBits = (uint32_t)(reg->getInterference() & 0xFFFFFFFF);
      uint32_t highBits  = (uint32_t)(reg->getInterference() >> 32);
      diagnostic("Remove live register %p, interference = %08x%08x, real reg = %08x, kind %d\n",
                  reg, highBits, lowerBits, liveReg->getAssociation(), reg->getKind());
#else
      diagnostic("Remove live register %p, interference = %08x, real reg = %08x\n",
                  reg, reg->getInterference(), liveReg->getAssociation());
#endif
      }

   // Return this live register info to the pool
   //
   liveReg->addToList(_pool);
   }

void
TR_LiveRegisters::removeRegisterFromLiveList(TR::Register *reg)
   {
   if (!reg->isLive())
      return;

   TR_ASSERT(!reg->getRegisterPair(), "not expecting a register pair: %p", reg);

   TR_LiveRegisterInfo *liveReg = reg->getLiveRegisterInfo();
   liveReg->removeFromList(_head);
   _numLiveRegisters--;
   }
