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
#include "il/Block.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/LocalAnalysis.hpp"

namespace TR { class Node; }
namespace TR { class Optimizer; }

// #define MAX_BLOCKS_FOR_STACK_ALLOCATION 16

// This file contains an implementation of Isolatedness which
// is the fifth global bit vector analyses used by PRE. Isolatedness
// attempts to avoid redundant copy statements generated sometimes
// as a result of the optimal placewherever possible.
//
//

TR_DataFlowAnalysis::Kind TR_Isolatedness::getKind()
   {
   return Isolatedness;
   }

TR_Isolatedness *TR_Isolatedness::asIsolatedness()
   {
   return this;
   }


int32_t TR_Isolatedness::getNumberOfBits()
   {
   return _latestness->getNumberOfBits();
   }





TR_Isolatedness::TR_Isolatedness(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure, bool trace)
   : TR_BackwardIntersectionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, trace)
   {
   _latestness = new (comp->allocator()) TR_Latestness(comp, optimizer, rootStructure, trace);
   _supportedNodesAsArray = _latestness->_supportedNodesAsArray;
   //_temp = NULL;

   /*
   if (trace())
      traceMsg("Starting Isolatedness\n");

   performAnalysis(rootStructure, false);

   if (trace())
      traceMsg("\nEnding Isolatedness\n");
   */
   }

bool TR_Isolatedness::postInitializationProcessing()
   {
/*
   _outSetInfo = (TR_BitVector **)jitStackAlloc(_numberOfNodes*sizeof(TR_BitVector *));
   memset(_outSetInfo, 0, _numberOfNodes*sizeof(TR_BitVector *));

   for (int32_t i = 0; i<_numberOfNodes; i++)
      _outSetInfo[i] = new (trStackMemory()) TR_BitVector(_numberOfBits, stackAlloc, trMemory());
*/
   return true;
   }




#if 0
// Overrides the implementation in the superclass as this analysis
// is slightly different from conventional bit vector analyses.
// It uses the results from local analyses instead of examining
// each tree top for effects on the input bit vector at that tree top.
// This analysis has a trivial analyzeNode(...) method as a result.
//
//
void TR_Isolatedness::analyzeNode(TR::Node *, uint16_t, TR_BlockStructure *, ContainerType *)
   {
   }

void TR_Isolatedness::analyzeTreeTopsInBlockStructure(TR_BlockStructure *blockStructure)
   {

   TR::Block *block = blockStructure->getBlock();
   TR::TreeTop *currentTree = block->getExit();
   TR::TreeTop *entryTree = block->getEntry();
   bool notSeenTreeWithChecks = true;

   copyFromInto(_regularInfo, _outSetInfo[blockStructure->getNumber()]);

   _containsExceptionTreeTop = false;
   while (!(currentTree == entryTree))
      {
      if (notSeenTreeWithChecks)
         {
         bool currentTreeHasChecks = treeHasChecks(currentTree);
         if (currentTreeHasChecks)
            {
            notSeenTreeWithChecks = false;
            _containsExceptionTreeTop = true;
            compose(_regularInfo, _exceptionInfo);
            compose(_outSetInfo[blockStructure->getNumber()], _exceptionInfo);
            }
         }
      else
         break;

      if (!(currentTree == entryTree))
         currentTree = currentTree->getPrevTreeTop();
      }

   // NOTE : Difference1 from Muchnick here
   // To account for the fact that expressions that are repeated in the block
   // are NOT isolated
   //
   /////////*_regularInfo -= *(_latestness->_delayedness->_earliestness->_globalAnticipatability->_localTransparency.getRepeatAnalysisInfo(blockStructure->getBlock()->getNumber()));
   //
   // NOTE : Difference1 from Muchnick over

   copyFromInto(_regularInfo, _outSetInfo[blockStructure->getNumber()]);

   if (_temp == NULL)
      allocateContainer(&_temp);

   // NOTE : Difference2 from Muchnick here
   // To prevent isolated information of expressions that are not locally
   // transparent from flowing up the flow graph and corrupting the flow
   // information of blocks higher up in the flow graph
   //
   // negation.setAll(_numberOfBits);
   // negation -= *(_latestness->_delayedness->_earliestness->_globalAnticipatability->_localTransparency._info[blockStructure->getBlock()->getNumber()]._analysisInfo);
   // *(_info[blockStructure->getNumber()]._analysisInfo) -= negation;
   //
   // Second version of the difference
   //
   *_regularInfo &= *(_latestness->_delayedness->_earliestness->_globalAnticipatability->_localTransparency.getAnalysisInfo(blockStructure->getBlock()->getNumber()));
   //
   // NOTE : Difference2 from Muchnick over

   if (block != comp()->getFlowGraph()->getEnd())
      {
      _temp->setAll(_numberOfBits);
      *_temp -= *(_latestness->_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability.getAnalysisInfo(blockStructure->getBlock()->getNumber()));

      if (trace())
         _latestness->_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability.getAnalysisInfo(blockStructure->getBlock()->getNumber())->print(comp());

      *_regularInfo &= *_temp;
      *_regularInfo |= *(_latestness->_inSetInfo[blockStructure->getNumber()]);


      if (trace())
         {
         /////traceMsg("\nIn Set of Block : %d\n", blockStructure->getNumber());
         /////_regularInfo->print(comp()->getOutFile());
         }
      }
   }
#endif
