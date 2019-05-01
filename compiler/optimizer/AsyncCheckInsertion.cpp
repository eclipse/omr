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

#include "optimizer/AsyncCheckInsertion.hpp"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"


#define NUMBER_OF_NODES_IN_LARGE_METHOD 2000



// Add an async check into a block - MUST be at block entry
//
void TR_AsyncCheckInsertion::insertAsyncCheck(TR::Block *block, TR::Compilation *comp, const char *counterPrefix)
   {
   TR::TreeTop *lastTree = block->getLastRealTreeTop();
   TR::TreeTop *asyncTree =
      TR::TreeTop::create(comp,
         TR::Node::createWithSymRef(lastTree->getNode(), TR::asynccheck, 0,
            comp->getSymRefTab()->findOrCreateAsyncCheckSymbolRef(comp->getMethodSymbol())));


   if (lastTree->getNode()->getOpCode().isReturn())
      {
      TR::TreeTop *prevTree = lastTree->getPrevTreeTop();
      prevTree->join(asyncTree);
      asyncTree->join(lastTree);
      }
   else
      {
      TR::TreeTop *nextTree = block->getEntry()->getNextTreeTop();
      block->getEntry()->join(asyncTree);
      asyncTree->join(nextTree);
      }

   const char * const name = TR::DebugCounter::debugCounterName(comp,
      "asynccheck.insert/%s/(%s)/%s/block_%d",
      counterPrefix,
      comp->signature(),
      comp->getHotnessName(),
      block->getNumber());
   TR::DebugCounter::prependDebugCounter(comp, name, asyncTree->getNextTreeTop());
   }

int32_t TR_AsyncCheckInsertion::insertReturnAsyncChecks(TR::Optimization *opt, const char *counterPrefix)
   {
   TR::Compilation * const comp = opt->comp();
   if (opt->trace())
      traceMsg(comp, "Inserting return asyncchecks (%s)\n", counterPrefix);

   int numAsyncChecksInserted = 0;
   for (TR::TreeTop *treeTop = comp->getStartTree();
        treeTop;
        /* nothing */ )
      {
      TR::Block *block = treeTop->getNode()->getBlock();
      if (block->getLastRealTreeTop()->getNode()->getOpCode().isReturn()
          && performTransformation(comp,
               "%sInserting return asynccheck (%s) in block_%d\n",
               opt->optDetailString(),
               counterPrefix,
               block->getNumber()))
         {
         insertAsyncCheck(block, comp, counterPrefix);
         numAsyncChecksInserted++;
         }

      treeTop = block->getExit()->getNextRealTreeTop();
      }
   return numAsyncChecksInserted;
   }

TR_AsyncCheckInsertion::TR_AsyncCheckInsertion(TR::OptimizationManager *manager) : TR::Optimization(manager)
   {
   }

bool TR_AsyncCheckInsertion::shouldPerform()
   {
   // Don't run when profiling
   //
   if (comp()->getProfilingMode() == JitProfiling || comp()->generateArraylets())
      return false;

   // It is not safe to add an asynccheck under involuntary OSR
   // as a transition may have to occur at the added point and the
   // required infrastructure may not exist
   //
   if (comp()->getOption(TR_EnableOSR) && comp()->getOSRMode() == TR::involuntaryOSR)
      return false;

   // This only helps when the method may be recompiled later due to sampling.
   //
   return comp()->getMethodHotness() != scorching
      && comp()->getRecompilationInfo() != NULL
#ifdef J9_PROJECT_SPECIFIC
      && comp()->getRecompilationInfo()->useSampling()
#endif
      && comp()->getRecompilationInfo()->shouldBeCompiledAgain();
   }

int32_t TR_AsyncCheckInsertion::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // If this is a large acyclic method - add a yield point at each return from this method
   // so that sampling will realize that we are actually in this method.
   //
   static const char *p;
   static uint32_t numNodesInLargeMethod = (p = feGetEnv("TR_LargeMethodNodes")) ? atoi(p) : NUMBER_OF_NODES_IN_LARGE_METHOD;
   const bool largeAcyclicMethod =
      !comp()->mayHaveLoops() && comp()->getNodeCount() > numNodesInLargeMethod;

   // If this method has loops whose asyncchecks were versioned out, it may
   // still spend a significant amount of time in each invocation without
   // yielding. In this case, insert yield points before returns whenever there
   // is a sufficiently frequent block somewhere in the method.
   //
   bool loopyMethodWithVersionedAsyncChecks = false;
   if (!largeAcyclicMethod && comp()->getLoopWasVersionedWrtAsyncChecks())
      {
      // The max (normalized) block frequency is fixed, but very frequent
      // blocks push down the frequency of method entry.
      int32_t entry = comp()->getStartTree()->getNode()->getBlock()->getFrequency();
      int32_t limit = comp()->getOptions()->getLoopyAsyncCheckInsertionMaxEntryFreq();
      loopyMethodWithVersionedAsyncChecks = 0 <= entry && entry <= limit;
      }

   if (largeAcyclicMethod || loopyMethodWithVersionedAsyncChecks)
      {
      const char * counterPrefix = largeAcyclicMethod ? "acyclic" : "loopy";
      int32_t numAsyncChecksInserted = insertReturnAsyncChecks(this, counterPrefix);
      if (trace())
         traceMsg(comp(), "Inserted %d async checks\n", numAsyncChecksInserted);
      return 1;
      }

   return 0;
   }

const char *
TR_AsyncCheckInsertion::optDetailString() const throw()
   {
   return "O^O ASYNC CHECK INSERTION: ";
   }
