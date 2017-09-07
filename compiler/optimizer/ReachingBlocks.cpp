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

#include <stdint.h>                                 // for int32_t
#include "env/StackMemoryRegion.hpp"
#include "compile/Compilation.hpp"                  // for Compilation
#include "env/TRMemory.hpp"
#include "infra/BitVector.hpp"                      // for TR_BitVector
#include "infra/Cfg.hpp"                            // for CFG
#include "optimizer/DataFlowAnalysis.hpp"

class TR_BlockStructure;
class TR_Structure;
namespace TR { class Optimizer; }

TR_DataFlowAnalysis::Kind TR_ReachingBlocks::getKind()
   {
   return ReachingBlocks;
   }


bool TR_ReachingBlocks::supportsGenAndKillSets()
   {
   return true;
   }


TR_ReachingBlocks::TR_ReachingBlocks(TR::Compilation *comp, TR::Optimizer *optimizer, bool trace)
   : TR_UnionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, trace)
   {
   _numberOfBlocks = comp->getFlowGraph()->getNextNodeNumber();
   }

int32_t TR_ReachingBlocks::perform()
   {
   // Allocate the block info before setting the stack mark - it will be used by
   // the caller
   //
   initializeBlockInfo();

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR_Structure *rootStructure = comp()->getFlowGraph()->getStructure();
   performAnalysis(rootStructure, false);
   } // scope of the stack memory region

   return 10; // actual cost
   }


int32_t TR_ReachingBlocks::getNumberOfBits()
   {
   return _numberOfBlocks;
   }


void TR_ReachingBlocks::analyzeBlockZeroStructure(TR_BlockStructure *blockStructure)
   {
   // Initialize the analysis info by making the initial parameter and field
   // definitions reach the method entry
   //
   //_blockInfo[0]->Empty();
   }


void TR_ReachingBlocks::initializeGenAndKillSetInfo()
   {
   // For each block in the CFG build the gen and kill set for this analysis.
   // Go in treetop order, which guarantees that we see the correct (i.e. first)
   // evaluation point for each node.
   //
   int32_t   blockNum;

   for (blockNum = 0; blockNum < _numberOfBlocks; blockNum++)
      {
      _regularGenSetInfo[blockNum] = new (trStackMemory()) TR_BitVector(getNumberOfBits(),trMemory(), stackAlloc);
      _regularGenSetInfo[blockNum]->set(blockNum);
      _exceptionGenSetInfo[blockNum] = new (trStackMemory()) TR_BitVector(getNumberOfBits(),trMemory(), stackAlloc);
      _exceptionGenSetInfo[blockNum]->set(blockNum);
      }
   }
