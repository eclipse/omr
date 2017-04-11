/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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

#include "optimizer/PrefetchInsertion.hpp"

#include <stddef.h>                              // for NULL
#include <stdint.h>                              // for int64_t, int32_t
#include "codegen/CodeGenerator.hpp"             // for CodeGenerator
#include "codegen/FrontEnd.hpp"                  // for feGetEnv, etc
#include "compile/Compilation.hpp"               // for Compilation
#include "compile/SymbolReferenceTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"                   // for ObjectModel
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                      // for TR_Memory
#include "env/jittypes.h"                        // for intptrj_t
#include "il/Block.hpp"                          // for Block
#include "il/DataTypes.hpp"                      // for DataTypes::Int32
#include "il/ILOpCodes.hpp"                      // for ILOpCodes::iconst, etc
#include "il/ILOps.hpp"                          // for ILOpCode
#include "il/Node.hpp"                           // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                         // for Symbol
#include "il/TreeTop.hpp"                        // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "infra/Cfg.hpp"                         // for CFG
#include "infra/List.hpp"                        // for ListIterator, etc
#include "optimizer/InductionVariable.hpp"
#include "optimizer/LoopCanonicalizer.hpp"       // for TR_LoopTransformer
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"     // for OptimizationManager
#include "optimizer/Structure.hpp"               // for TR_RegionStructure, etc

namespace TR { class SymbolReference; }

#define OPT_DETAILS "O^O PREFETCH INSERTION: "

TR_PrefetchInsertion::TR_PrefetchInsertion(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager),
     _arrayAccessInfos(manager->trMemory())
   {}

int32_t TR_PrefetchInsertion::perform()
   {
   if (comp()->requiresSpineChecks())
      {
      // see 172692
      if (trace())
         traceMsg(comp(), "Spine checks required for array element accesses -- returning from prefetch insertion.\n");
      return 0;
      }

   if (!comp()->mayHaveLoops())
      {
      if (trace())
         traceMsg(comp(), "Method does not have loops -- returning from prefetch insertion.\n");
      return 0;
      }

   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure();

   _arrayAccessInfos.init();

   // From here, down, stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
      {
      traceMsg(comp(), "Starting Prefetch Insertion\n");
      comp()->dumpMethodTrees("Before prefetch insertion");
      }

   // Collect and analyze information about loops
   collectLoops(_rootStructure);
   dumpOptDetails(comp(), "Loop analysis completed...\n");

   if (!_arrayAccessInfos.isEmpty())
      insertPrefetchInstructions();
   else
      dumpOptDetails(comp(), "Prefetch insertion completed: no qualifying loops found\n");

   return 0;
   }


void TR_PrefetchInsertion::collectLoops(TR_Structure *str)
   {
   TR_RegionStructure *region = str->asRegion();

   if (region == NULL)
      return;

   if (region->isNaturalLoop())
      {
      if (trace())
         traceMsg(comp(), "<Analyzing outer loop=%d addr=%p>\n", region->getNumber(), region);

      // Check if there is an induction variable
      //
      if (region->getPrimaryInductionVariable() != NULL || !region->getBasicInductionVariables().isEmpty())
         {
         if (!region->getEntryBlock()->isCold())
            {
            examineLoop(region);
            return;
            }
         else
            {
            if (trace())
               traceMsg(comp(), "\tReject loop %d ==> cold loop\n", region->getNumber());
            return;
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> no basic induction variable\n", region->getNumber());
         }
      }

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getCurrent(); node; node = it.getNext())
      collectLoops(node->getStructure());

   }


static bool identicalSubTrees(TR::Node *node1, TR::Node *node2)
   {
   if (node1 == node2)
      return true;

   if (node1->getOpCodeValue() != node2->getOpCodeValue())
      return false;

   if (node1->getOpCodeValue() == TR::iconst &&
       node1->getInt() != node2->getInt())
      return false;

   if (node1->getOpCodeValue() == TR::lconst &&
       node1->getLongInt() != node2->getLongInt())
      return false;

   if (node1->getOpCode().isLoadVar() &&
       node1->getSymbolReference() != node2->getSymbolReference())
      return false;
   
   if (node1->getNumChildren() != node2->getNumChildren())
      return false;
   

   for (int32_t i = 0; i < node1->getNumChildren(); i++)
      if (!identicalSubTrees(node1->getChild(i), node2->getChild(i)))
         return false;

   return true;
   }


void TR_PrefetchInsertion::insertPrefetchInstructions()
   {
   ListIterator<ArrayAccessInfo> it(&_arrayAccessInfos);
   for (ArrayAccessInfo *aai = it.getFirst(); aai; aai = it.getNext())
      {
      TR::Block * block = aai->_treeTop->getEnclosingBlock();
      TR_Structure * loop = block->getStructureOf()->getContainingLoop();
      bool skip = false;
      ListIterator<ArrayAccessInfo> it1(&_arrayAccessInfos);
      for (ArrayAccessInfo *aai1 = it1.getFirst(); !skip && aai1 != aai; aai1 = it1.getNext())
         {
         TR::Block * block1 = aai1->_treeTop->getEnclosingBlock();
         if (block == block1 && identicalSubTrees(aai->_addressNode, aai1->_addressNode))
            skip = true;
         }
      for (ArrayAccessInfo *aai1 = it1.getNext(); !skip && aai1; aai1 = it1.getNext())
         {
         TR::Block * block1 = aai1->_treeTop->getEnclosingBlock();
         TR_Structure * loop1 = block1->getStructureOf()->getContainingLoop();
         if (loop == loop1 && block != block1 && identicalSubTrees(aai->_addressNode, aai1->_addressNode))
            skip = true;
         }
      if (!skip && performTransformation(comp(), "%sInserting prefetch for array access %p in block_%d\n",
                                           OPT_DETAILS, aai->_aaNode, aai->_treeTop->getEnclosingBlock()->getNumber()))
         {
         // Enable tree simplification on this block to normalize any address computation so we can produce an optimal
         // instruction sequence during instruction selection
         requestOpt(OMR::treeSimplification, true, aai->_treeTop->getEnclosingBlock());

         TR::Node *prefetchNode, *addressNode, *deltaNode, *offsetNode, *sizeNode, *typeNode;

         // First child -- base address
         deltaNode = createDeltaNode(aai->_addressNode->getSecondChild()->getFirstChild(), aai->_bivNode,
                                     aai->_biv->getDeltaOnBackEdge());
         addressNode = TR::Node::createWithSymRef(TR::aloadi, 1, 1,
            TR::Node::create(aai->_addressNode->getOpCodeValue(), 2, aai->_addressNode, deltaNode),
            aai->_aaNode->getSymbolReference());

         addressNode->getFirstChild()->setIsInternalPointer(true);

         // Second child -- offset
         offsetNode = TR::Node::iconst(addressNode, 0);

         // Third child -- size
         sizeNode = TR::Node::iconst(addressNode, 1);

         // Fourth child -- type
         // Better default prefetch type for all platforms is Load,
         // except for 390, where it is Store when a store is found in the loop.
         //

         bool foundStore = false;
#if defined(TR_TARGET_S390)
         //Quick search to identify it this is a store prefetch (if possible)
         //Walk all blocks in the loop and search for storei whose first child includes a[i].
         TR_ScratchList<TR::Block> blocksInLoop(trMemory());
         aai->_treeTop->getEnclosingBlock()->getStructureOf()->getContainingLoop()->getBlocks(&blocksInLoop);
         ListIterator<TR::Block> bIt(&blocksInLoop);
         for (TR::Block *blk = bIt.getFirst(); blk && !foundStore; blk = bIt.getNext())
            {
            TR::TreeTop *currentTree = blk->startOfExtendedBlock()->getEntry();
            TR::TreeTop *exitTree = currentTree->getExtendedBlockExitTreeTop();

            while ((currentTree != exitTree) && !foundStore)
               {
               TR::Node *node = currentTree->getNode();
               if (node->getOpCode().isStoreIndirect())
                  {
            	  // loopup the a[i];
            	   if ((node->getFirstChild() == aai->_aaNode) ||
                       (node->getSymbol()->isArrayShadowSymbol() && node->getFirstChild()->getOpCode().isArrayRef() &&
                        node->getFirstChild()->getFirstChild()== aai->_aaNode))
                      foundStore = true;
                  }
               currentTree = currentTree->getNextTreeTop();
               }
            }
#endif

         static char * disablePrefetchStore = feGetEnv("TR_DISABLEPrefetchStore");
         typeNode = TR::Node::iconst(addressNode, (TR::Compiler->target.cpu.isZ() && foundStore && !disablePrefetchStore) ?
                                                            (int32_t)PrefetchStore :
                                                            (int32_t)PrefetchLoad);

         prefetchNode = TR::Node::createWithSymRef(TR::Prefetch, 4, 4,
                                        addressNode, offsetNode, sizeNode, typeNode,
                                        comp()->getSymRefTab()->findOrCreatePrefetchSymbol());

         TR::TreeTop *treeTop = aai->_treeTop;
         if (treeTop->getNode()->getOpCode().isBranch())
            treeTop = TR::TreeTop::create(comp(), treeTop->getPrevTreeTop(),
                                         TR::Node::create(TR::treetop, 1,
                                                         treeTop->getNode()->getFirstChild()));
         else if (treeTop->getNode()->canGCandReturn())
            treeTop = treeTop->getPrevTreeTop();

         TR::TreeTop::create(comp(), treeTop, prefetchNode);
         }
      }
   }


TR::Node *TR_PrefetchInsertion::createDeltaNode(TR::Node *node, TR::Node *pivNode, int32_t deltaOnBackEdge)
   {
   if (node == pivNode)
      {
      if (pivNode->getDataType() == TR::Int32)
         return TR::Node::iconst(pivNode, deltaOnBackEdge);
      else
         return TR::Node::lconst(pivNode, deltaOnBackEdge);
      }

   if (node->getNumChildren() == 0)
      return node;

   TR::Node *newNode;

   if(node->getOpCode().hasSymbolReference())
      newNode = TR::Node::createWithSymRef(node, node->getOpCodeValue(), node->getNumChildren(), node->getSymbolReference());
   else
      newNode = TR::Node::create(node, node->getOpCodeValue(), node->getNumChildren());

   for (intptrj_t i = 0; i < node->getNumChildren(); i++)
      {
      newNode->setAndIncChild(i, createDeltaNode(node->getChild(i), pivNode, deltaOnBackEdge));
      }
   return newNode;
   }


// Examine loop to find all candidates for prefetch insertion.
//
void TR_PrefetchInsertion::examineLoop(TR_RegionStructure *loop)
   {
   intptrj_t visitCount = comp()->incVisitCount();

   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> bIt(&blocksInLoop);
   for (TR::Block *block = bIt.getFirst(); block; block = bIt.getNext())
      {
      TR::TreeTop *currentTree = block->startOfExtendedBlock()->getEntry();
      TR::TreeTop *exitTree = currentTree->getExtendedBlockExitTreeTop();

      while (currentTree != exitTree)
         {
         TR::Node *node = currentTree->getNode();
         if (node->getNumChildren() > 0)
            examineNode(currentTree, block, node, visitCount, loop);
         currentTree = currentTree->getNextTreeTop();
         }
      }
   }


void TR_PrefetchInsertion::examineNode(TR::TreeTop *treeTop, TR::Block *block, TR::Node *node, intptrj_t visitCount, TR_RegionStructure *loop)
   {
   // If we have seen this node before, we are done
   // Otherwise, set visit count
   //
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   TR::Symbol *symbol = NULL;
   if (node->getOpCode().hasSymbolReference())
      symbol = node->getSymbol();

   if (symbol && symbol->isArrayShadowSymbol() && node->getOpCodeValue() == TR::aloadi && node->getFirstChild()->getOpCode().isArrayRef())
      {
      /* Pattern match for reference array accesses
         iaload                                           iaload
            aiadd                                            aladd
               aload <array> / iaload                           aload <array> / iaload
               isub/iadd                                        lsub/ladd
                  (imul/ishl)                                      (lmul/lshl)
                     iload/iadd/isub/imul                             i2l    or   lload/ladd/lsub/lmul
                     iconst                                              iload/iadd/isub/imul
                  iconst                                              lconst/iconst
                                                                   lconst
       */
      int64_t addConst = 0;
      int64_t mulConst = 1;
      int64_t mulConstBytes = 1;
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = firstChild->getSecondChild();
      if ((secondChild->getOpCode().isAdd() || secondChild->getOpCode().isSub()) &&
          secondChild->getSecondChild()->getOpCode().isLoadConst())
         {
         firstChild = secondChild->getFirstChild();

         if ((firstChild->getOpCode().isMul() ||
              firstChild->getOpCode().isLeftShift()) &&
             firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
            if (firstChild->getSecondChild()->getOpCodeValue() == TR::iconst)
               mulConstBytes = (int64_t)((firstChild->getOpCode().isMul()) ?
                                  firstChild->getSecondChild()->getInt() :
                                  2 << firstChild->getSecondChild()->getInt());
            else
               mulConstBytes = firstChild->getOpCode().isMul() ?
                                  firstChild->getSecondChild()->getLongInt() :
                                  2 << firstChild->getSecondChild()->getLongInt();

            firstChild = firstChild->getFirstChild();
            }

         if (firstChild->getOpCode().isConversion())
            {
            firstChild = firstChild->getFirstChild();
            }

         if ((firstChild->getOpCode().isAdd() ||
              firstChild->getOpCode().isSub()) &&
             firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
            if (firstChild->getSecondChild()->getOpCodeValue() == TR::iconst)
               addConst = (int64_t)((firstChild->getOpCode().isAdd()) ?
                             (int64_t)firstChild->getSecondChild()->getInt() :
                             -firstChild->getSecondChild()->getInt());
            else
               addConst = firstChild->getOpCode().isAdd() ?
                             firstChild->getSecondChild()->getLongInt() :
                             -firstChild->getSecondChild()->getLongInt();

            firstChild = firstChild->getFirstChild();
            }

         if ((firstChild->getOpCode().isMul() ||
              firstChild->getOpCode().isLeftShift()) &&
             firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
            if (firstChild->getSecondChild()->getOpCodeValue() == TR::iconst)
               mulConst = (int64_t)((firstChild->getOpCode().isMul()) ?
                             firstChild->getSecondChild()->getInt() :
                             2 << firstChild->getSecondChild()->getInt());
            else
               mulConst = firstChild->getOpCode().isMul() ?
                             firstChild->getSecondChild()->getLongInt() :
                             2 << firstChild->getSecondChild()->getLongInt();

            firstChild = firstChild->getFirstChild();
            }

         TR_PrimaryInductionVariable *closestPIV = NULL;  // argument loop is the most outer loop
         TR_BasicInductionVariable *biv = NULL;  // argument loop is the most outer loop
         if (firstChild->getOpCode().isLoadDirect() &&
             ((closestPIV=getClosestPIV(block)) && (firstChild->getOpCode().hasSymbolReference() && firstChild->getSymbolReference() == closestPIV->getSymRef()) ||
             closestPIV == NULL && isBIV(firstChild->getSymbolReference(), block, biv)))
            {
            if (closestPIV)
               biv = closestPIV;

            int64_t stepInBytes = mulConstBytes * (mulConst * (int64_t)biv->getDeltaOnBackEdge() + addConst);
            TR_Structure *loop1= treeTop->getEnclosingBlock()->getStructureOf()->getContainingLoop();
            bool isTreetopInLoop = (loop1 && (loop1->asRegion() == loop)) ? true : false;
            if (isTreetopInLoop && (stepInBytes > 0 &&  stepInBytes <= TR::Compiler->vm.heapTailPaddingSizeInBytes() ||
                stepInBytes < 0 && -stepInBytes <= TR::Compiler->om.contiguousArrayHeaderSizeInBytes()))
               {
               // Save array access info
               //
               ArrayAccessInfo *aai = (ArrayAccessInfo *) trMemory()->allocateStackMemory(sizeof(ArrayAccessInfo));
               aai->_treeTop = treeTop;
               aai->_aaNode = node;
               aai->_addressNode = node->getFirstChild();
               aai->_bivNode = firstChild;
               aai->_biv = biv;
               _arrayAccessInfos.add(aai);

               if (trace())
                  traceMsg(comp(), "Found array access: node %p, access address node %p, biv node %p\n",
                          node, aai->_addressNode, aai->_bivNode);
               return;
               }
            }
         }
      }

   /* Walk its children */
   for (intptrj_t i = 0; i < node->getNumChildren(); i++)
      {
      examineNode(treeTop, block, node->getChild(i), visitCount, loop);
      }
   }


TR_PrimaryInductionVariable *TR_PrefetchInsertion::getClosestPIV(TR::Block *block)
   {
   TR_Structure *loop = block->getStructureOf()->getContainingLoop();
   if (loop && loop->asRegion())
      return loop->asRegion()->getPrimaryInductionVariable();
   return NULL;
   }

bool TR_PrefetchInsertion::isBIV(TR::SymbolReference* symRef, TR::Block *block, TR_BasicInductionVariable* &biv)
   {
   List<TR_BasicInductionVariable> *bivs;
   TR_Structure *loop = block->getStructureOf()->getContainingLoop();
   if (loop && loop->asRegion())
      bivs = &loop->asRegion()->getBasicInductionVariables();
   else
      return false;

   ListIterator<TR_BasicInductionVariable> it(bivs);
   for (biv = it.getFirst(); biv; biv = it.getNext())
      {
      if (biv->getSymRef() == symRef)
         return true;
      }

   return false;
   }
