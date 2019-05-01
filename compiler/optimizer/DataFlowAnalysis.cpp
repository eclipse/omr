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

#include "optimizer/DataFlowAnalysis.hpp"

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimizer.hpp"

class TR_ExceptionCheckMotion;
class TR_RedundantExpressionAdjustment;

// This file contains some of the general purpose helper functions
// that may be of use for any dataflow analysis.
//
//
#ifdef DEBUG
static char *analysisNames[] =
   { "ReachingDefinitions",
     "AvailableExpressions",
     "GlobalAnticipatability",
     "Earliestness",
     "Delayedness",
     "Latestness",
     "Isolatedness",
     "Liveness",
     "BasicDFSetAnalysis",
     "ForwardDFSetAnalysis",
     "UnionDFSetAnalysis",
     "IntersectionDFSetAnalysis",
     "BackwardDFSetAnalysis",
     "BackwardUnionDFSetAnalysis",
     "BackwardIntersectionDFSetAnalysis",
     "ExceptionCheckMotion",
     "RedundantExpressionAdjustment",
     "FlowSensitiveEscapeAnalysis",
     "LiveOnAllPaths",
     "DSALiveness",
     "GPRLiveness",
   };

char* TR_DataFlowAnalysis::getAnalysisName() { return analysisNames[this->getKind()]; }

#endif

TR_ExceptionCheckMotion           *TR_DataFlowAnalysis::asExceptionCheckMotion()
   {return NULL;}
TR_RedundantExpressionAdjustment *TR_DataFlowAnalysis::asRedundantExpressionAdjustment()
   {return NULL;}
//TR_BitVectorAnalysis             *TR_DataFlowAnalysis::asBitVectorAnalysis()
//   {return NULL;}
//TR_ForwardBitVectorAnalysis      *TR_DataFlowAnalysis::asForwardBitVectorAnalysis()
//   {return NULL;}
//TR_UnionBitVectorAnalysis        *TR_DataFlowAnalysis::asUnionBitVectorAnalysis()
//   {return NULL;}
//TR_IntersectionBitVectorAnalysis  *TR_DataFlowAnalysis::asIntersectionBitVectorAnalysis()
//   {return NULL;}
//TR_BackwardBitVectorAnalysis      *TR_DataFlowAnalysis::asBackwardBitVectorAnalysis()
//   {return NULL;}
//TR_BackwardUnionBitVectorAnalysis    *TR_DataFlowAnalysis::asBackwardUnionBitVectorAnalysis()
//   {return NULL;}
//TR_BackwardIntersectionBitVectorAnalysis *TR_DataFlowAnalysis::asBackwardIntersectionBitVectorAnalysis()
//   {return NULL;}
TR_GlobalAnticipatability           *TR_DataFlowAnalysis::asGlobalAnticipatability()
   {return NULL;}
TR_Earliestness                     *TR_DataFlowAnalysis::asEarliestness()
   {return NULL;}
TR_Delayedness                      *TR_DataFlowAnalysis::asDelayedness()
   {return NULL;}
TR_Latestness                       *TR_DataFlowAnalysis::asLatestness()
   {return NULL;}
TR_Isolatedness                     *TR_DataFlowAnalysis::asIsolatedness()
   {return NULL;}
TR_Liveness                         *TR_DataFlowAnalysis::asLiveness()
   {return NULL;}
TR_LiveOnAllPaths                   *TR_DataFlowAnalysis::asLiveOnAllPaths()
   {return NULL;}
TR_FlowSensitiveEscapeAnalysis      *TR_DataFlowAnalysis::asFlowSensitiveEscapeAnalysis()
   {return NULL;}

void TR_DataFlowAnalysis::addToAnalysisQueue(TR_StructureSubGraphNode *node, uint8_t changedSets)
   {
   _analysisQueue.add(node);
   uint8_t *changedValue = (uint8_t *)trMemory()->allocateStackMemory(sizeof(uint8_t));
   *changedValue = changedSets;
   _changedSetsQueue.add(changedValue);
   }


void TR_DataFlowAnalysis::removeHeadFromAnalysisQueue()
   {
   _analysisQueue.setListHead(_analysisQueue.getListHead()->getNextElement());
   _changedSetsQueue.setListHead(_changedSetsQueue.getListHead()->getNextElement());
   }




bool TR_DataFlowAnalysis::isSameAsOrAliasedWith(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2)
   {
   if (symRef1->getReferenceNumber() == symRef2->getReferenceNumber())
      return true;

   if (symRef1->getUseDefAliases().contains(symRef2, comp()))
      return true;

   return false;
   }






bool TR_DataFlowAnalysis::collectCallSymbolReferencesInTreeInto(TR::Node *node, List<TR::SymbolReference> *callSymbolReferencesInTree)
   {
   bool flag = false;

   // diagnostic("OpCode being examined : %s\n",node->getOpCode().getName());

   if (node->getOpCode().isCall())
      {
      callSymbolReferencesInTree->add(node->getSymbolReference());
      flag = true;
      }

   for (int32_t i = 0;i < node->getNumChildren();i++)
      {
      bool childFlag = collectCallSymbolReferencesInTreeInto(node->getChild(i), callSymbolReferencesInTree);
      if (childFlag)
         flag = true;
      }

   return flag;
   }





bool TR_DataFlowAnalysis::collectAllSymbolReferencesInTreeInto(TR::Node *node, List<TR::SymbolReference> *allSymbolReferencesInTree)
   {
   bool flag = false;

   // diagnostic("OpCode being examined : %s\n",node->getOpCode().getName());

   if (node->getOpCode().hasSymbolReference())
      {
      allSymbolReferencesInTree->add(node->getSymbolReference());
      flag = true;
      }

   for (int32_t i = 0;i < node->getNumChildren();i++)
      {
      bool childFlag = collectAllSymbolReferencesInTreeInto(node->getChild(i), allSymbolReferencesInTree);
      if (childFlag)
         flag = true;
      }

   return flag;
   }






bool TR_DataFlowAnalysis::areSyntacticallyEquivalent(TR::Node *node1, TR::Node *node2)
   {
   // diagnostic("SyntacticallyEquivalent \n");

   if (!comp()->getOptimizer()->areNodesEquivalent(node1,node2))
      return false;

   if (!(node1->getNumChildren() == node2->getNumChildren()))
      return false;

   //if (node1 == node2)
   //   return true;

   for (int32_t i = 0;i < node1->getNumChildren();i++)
      {
      if (!areSyntacticallyEquivalent(node1->getChild(i), node2->getChild(i)))
         return false;
      }

   return true;
   }
