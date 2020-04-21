/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef TR_GLOBALVALUEPROPAGATION_INCL
#define TR_GLOBALVALUEPROPAGATION_INCL

#include "optimizer/ValuePropagation.hpp"

namespace TR { class CFGNode; }
class TR_StructureSubGraphNode;
class TR_BitVector;

namespace TR {

/** \class GlobalValuePropagation
 *
 *  \brief An optimization that propagates value constraints within a block.

 *  The global value propagation optimization does the same thing as local
 *  value propagation does except that global value propagation is conducted
 *  across blocks.
 *
 * \note
 *  The class is not intented to be extended, ValuePropagation is the core
 *  engine and home to extension points of language specific optimizations
 */
class GlobalValuePropagation : public TR::ValuePropagation
   {
   public:

   GlobalValuePropagation(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR::GlobalValuePropagation(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:

   void determineConstraints();
   void processStructure(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processAcyclicRegion(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processNaturalLoop(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processImproperLoop(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processRegionSubgraph(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop, bool isNaturalLoop);
   void processRegionNode(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void processBlock(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool insideLoop);
   void getImproperRegionStores(TR_StructureSubGraphNode *node, ValueConstraints &stores);
   bool buildInputConstraints(TR::CFGNode *node);
   void propagateOutputConstraints(TR_StructureSubGraphNode *node, bool lastTimeThrough, bool isNaturalLoop, List<TR::CFGEdge> &outEdges1, List<TR::CFGEdge> *outEdges2);
   TR_BitVector *mergeDefinedOnAllPaths(TR_StructureSubGraphNode *node);
   // Blocks not included in this set will be skipped for speed.
   // NULL bitvector means the info is unavailable, and all blocks should be processed.
   //
   TR_BitVector *_blocksToProcess;

   };
}
#endif
