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

#include "control/OptimizationPlan.hpp"

#include <stdio.h>
#include "env/TRMemory.hpp"
#include "infra/Monitor.hpp"

TR_OptimizationPlan* TR_OptimizationPlan::_pool = 0;
unsigned long        TR_OptimizationPlan::_totalNumAllocatedPlans = 0;
unsigned long        TR_OptimizationPlan::_poolSize = 0;
TR::Monitor *TR_OptimizationPlan::_optimizationPlanMonitor = 0;
unsigned long        TR_OptimizationPlan::_numAllocOp = 0;
unsigned long        TR_OptimizationPlan::_numFreeOp = 0;


//----------------------------- freeOptimizationPlan --------------------------
// Puts the optimization plan back into the pool. If the pool of available
// entries is too large we free half the entries (by calling jitPersistentFree)
// Operations on the pool are protected by a monitor
//-----------------------------------------------------------------------------
void TR_OptimizationPlan::freeOptimizationPlan(TR_OptimizationPlan *plan)
   {
   _numFreeOp++; // statistics
   if (!plan->getIsStackAllocated())
      {
      TR_OptimizationPlan *listToBeFreed = NULL;
      _optimizationPlanMonitor->enter();
      TR_ASSERT(plan->inUse(), "plan %p must be in use\n", plan);
      plan->setInUse(false); // RAS feature
      plan->_next = _pool;
      _pool = plan;
      _poolSize++;
      // there is the danger of freeing an entry twice
      TR_ASSERT(_totalNumAllocatedPlans >= _poolSize, "_totalNumAllocatedPlans should be greater than _poolSize");
      // if too many entries in the pool, free some
      //
      if (_poolSize > POOL_THRESHOLD)
         {
         do {
            TR_OptimizationPlan *p = _pool;
            _pool = _pool->_next;
            p->_next = listToBeFreed;
            listToBeFreed = p;
            _poolSize--;
            _totalNumAllocatedPlans--;
            TR_ASSERT(_totalNumAllocatedPlans >= _poolSize, "Error: _totalNumAllocatedPlans=%d poolSize=%d\n", _totalNumAllocatedPlans, _poolSize);
            }while(_poolSize > POOL_THRESHOLD/2);
         }

      _optimizationPlanMonitor->exit();
      if (listToBeFreed)
         {
         do
            {
            TR_OptimizationPlan *p = listToBeFreed;
            listToBeFreed = listToBeFreed->_next;
            jitPersistentFree(p);
            }while(listToBeFreed);
         }
      }
   }


//---------------------------------- operator new --------------------------
// The overloaded operator new first tries to get an available optimization
// plan from the pool. If no entries are in the pool, we allocate a new
// one using jitPersitentAlloc
//--------------------------------------------------------------------------
void* TR_OptimizationPlan::operator new (size_t size) NOTHROW
   {
   // try to get an entry from the pool
   TR_OptimizationPlan *plan = 0;
   _numAllocOp++; // statistics, no need for lock

   _optimizationPlanMonitor->enter();
   if (_pool)
      {
      plan = _pool;
      _pool = _pool->_next;
      _poolSize--;
      TR_ASSERT(_poolSize >= 0, "_poolSize cannot become negative");
      TR_ASSERT(!plan->inUse(), "new plan must not be in use\n");
      _optimizationPlanMonitor->exit();
      }
   else // pool is empty; allocate a new entry
      {
      TR_ASSERT(_poolSize==0, "Some error regarding optimization plan accounting\n");
      _totalNumAllocatedPlans++; // anticipate the allocation
      // No need to keep the optimizationPlanMonitor locked while
      // we call jitPersistentAlloc
      //
      _optimizationPlanMonitor->exit();
      plan = (TR_OptimizationPlan*)jitPersistentAlloc(size);
      }
   // TODO: generate a message if we cannot allocate memory for the plan
   return (void*)plan;
   }


//--------------------------------- freeEntirePool ----------------------------
// Deallocates all entries in the pool calling freePersistentFree on them
// Returns the number of plans still in use somewhere in the system. This should
// be normally 0 (it may be a small number if some thread tries to perform a
// compilation while the shutdown procedure has been initiated
//-----------------------------------------------------------------------------
int32_t TR_OptimizationPlan::freeEntirePool()
   {
   int32_t remainingPlans;
   _optimizationPlanMonitor->enter();
   while (_pool)
      {
      TR_OptimizationPlan *plan = _pool;
      _pool = _pool->_next;
      jitPersistentFree(plan);
      _poolSize--;
      _totalNumAllocatedPlans--;
      }
   TR_ASSERT(_poolSize==0, "Some error regarding optimization plan accounting\n");
   if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL1)
      fprintf(stderr, "TR_OptimizationPlan allocations=%lu releases=%lu\n", _numAllocOp, _numFreeOp);
   remainingPlans = _totalNumAllocatedPlans;
   _optimizationPlanMonitor->exit();
   return remainingPlans;
   }
