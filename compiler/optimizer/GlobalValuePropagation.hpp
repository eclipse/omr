/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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
   // Blocks not included in this set will be skipped for speed.
   // NULL bitvector means the info is unavailable, and all blocks should be processed.
   //
   TR_BitVector *_blocksToProcess;

   };
}
#endif
