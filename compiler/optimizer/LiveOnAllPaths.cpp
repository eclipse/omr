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

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "optimizer/DataFlowAnalysis.hpp"

class TR_BlockStructure;
class TR_Structure;
namespace TR { class Optimizer; }

TR_DataFlowAnalysis::Kind TR_LiveOnAllPaths::getKind()
   {
   return LiveOnAllPaths;
   }

TR_LiveOnAllPaths *TR_LiveOnAllPaths::asLiveOnAllPaths()
   {
   return this;
   }

bool TR_LiveOnAllPaths::supportsGenAndKillSets()
   {
   return true;
   }

int32_t TR_LiveOnAllPaths::getNumberOfBits()
   {
   return _liveVariableInfo->numLocals();
   }

void TR_LiveOnAllPaths::analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *)
   {
   }

TR_LiveOnAllPaths::TR_LiveOnAllPaths(TR::Compilation *comp,
                                     TR::Optimizer *optimizer,
                                     TR_Structure               *rootStructure,
                                     TR_LiveVariableInformation *liveVariableInfo,
                                     bool                        splitLongs,
                                     bool                        includeParms)
   : TR_BackwardIntersectionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, comp->getOption(TR_TraceLiveness))
   {
   if (trace())
      traceMsg(comp, "Starting LiveOnAllPaths analysis\n");

   int32_t i;

   if (comp->getVisitCount() > 8000)
      comp->resetVisitCounts(1);

   if (liveVariableInfo == NULL)
      _liveVariableInfo = new (trStackMemory()) TR_LiveVariableInformation(comp, optimizer, rootStructure, splitLongs, includeParms);
   else
      _liveVariableInfo = liveVariableInfo;

   if (_liveVariableInfo->numLocals() == 0)
      return; // Nothing to do if there are no locals

   // Allocate the block info before setting the stack mark - it will be used by
   // the caller
   //
   initializeBlockInfo();

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   performAnalysis(rootStructure, false);

   if (trace())
      {
      for (i = 1; i < _numberOfNodes; ++i)
         {
         if (_blockAnalysisInfo[i])
            {
            traceMsg(comp, "\nLiveOnAllPaths variables for block_%d: ",i);
            _blockAnalysisInfo[i]->print(comp);
            }
         }
      traceMsg(comp, "\nEnding LiveOnAllPaths analysis\n");
      }
   } // scope of the stack memory region

   }

bool TR_LiveOnAllPaths::postInitializationProcessing()
   {
   if (trace())
      {
      int32_t i;
      for (i = 1; i < _numberOfNodes; ++i)
         {
         traceMsg(comp(), "\nGen and kill sets for block_%d: ",i);
         if (_regularGenSetInfo[i])
            {
            traceMsg(comp(), " gen set ");
            _regularGenSetInfo[i]->print(comp());
            }
         if (_regularKillSetInfo[i])
            {
            traceMsg(comp(), " kill set ");
            _regularKillSetInfo[i]->print(comp());
            }
         if (_exceptionGenSetInfo[i])
            {
            traceMsg(comp(), " exception gen set ");
            _exceptionGenSetInfo[i]->print(comp());
            }
         if (_exceptionKillSetInfo[i])
            {
            traceMsg(comp(), " exception kill set ");
            _exceptionKillSetInfo[i]->print(comp());
            }
         }
      }
   return true;
   }

void TR_LiveOnAllPaths::initializeGenAndKillSetInfo()
   {
   _liveVariableInfo->initializeGenAndKillSetInfo(_regularGenSetInfo, _regularKillSetInfo, _exceptionGenSetInfo, _exceptionKillSetInfo);
   }

void TR_LiveOnAllPaths::analyzeTreeTopsInBlockStructure(TR_BlockStructure *blockStructure)
   {
   TR_ASSERT(false, "LiveOnAllPaths should use gen and kill sets");
   }
