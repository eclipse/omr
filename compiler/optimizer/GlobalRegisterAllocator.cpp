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

#include "optimizer/GlobalRegisterAllocator.hpp"

#include <stdint.h>                                 // for int8_t, int32_t, etc
#include <string.h>                                 // for NULL, memset, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd, etc
#include "codegen/Linkage.hpp"                      // for Linkage
#include "codegen/Machine.hpp"                      // for Machine
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"              // for TR::Options, etc
#include "cs2/bitvectr.h"
#include "cs2/hashtab.h"                            // for HashTable, etc
#include "cs2/sparsrbit.h"                          // for ASparseBitVector, etc
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                               // for POINTER_PRINTF_FORMAT
#include "env/ObjectModel.hpp"                      // for ObjectModel
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                         // for SparseBitVector, etc
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                             // for ILOpCode, etc
#include "il/Node.hpp"                              // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference, etc
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode, etc
#include "il/symbol/AutomaticSymbol.hpp"            // for AutomaticSymbol
#include "il/symbol/ParameterSymbol.hpp"            // for ParameterSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"                          // for TR_Array
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/Link.hpp"                           // for TR_LinkHead
#include "infra/List.hpp"                           // for ListIterator, etc
#include "infra/CfgEdge.hpp"                        // for CFGEdge
#include "infra/CfgNode.hpp"                        // for CFGNode
#include "optimizer/Optimization.hpp"               // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"                  // for Optimizer
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"              // for TransformUtil
#include "optimizer/DataFlowAnalysis.hpp"           // for TR_Liveness
#include "optimizer/UseDefInfo.hpp"                 // for TR_UseDefInfo, etc
#include "ras/Debug.hpp"                            // for TR_DebugBase


#define GRA_COMPLEXITY_LIMIT 1000000000

static bool isHot(TR::Compilation *comp) { return comp->getMethodHotness() >= hot; }

#define HAVE_DIFFERENT_MSB_TO_LSB_OFFSETS(r1,r2) \
   ((((r1)->getHostByteOffset() + (r1)->getSize()) - ((r2)->getHostByteOffset() + (r2)->getSize())) != 0)

static bool dontAssignInColdBlocks(TR::Compilation *comp) { return comp->getMethodHotness() >= hot; }

#define keepAllStores false

//TODO:GRA: if we are going to have two versions of GRA one with Meta Data and one without then we can do someting here
// for different compilation level then we can have two version of this, one using CPIndex and one  (i.e. with MetaData)
// using getReferenceNumber
#define GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(S) (S)->getReferenceNumber()
#define CANDIDATE_FOR_SYMREF_SIZE (comp()->getSymRefCount()+1)

//static TR_BitVector *resetExits;
//static TR_BitVector *seenBlocks;
//static TR_BitVector *successorBits;

#define OPT_DETAILS "O^O GLOBAL REGISTER ASSIGNER: "


// For both switch/table instructions and igoto instructions, the
// same sort of processing has to be done for each successor block.
// These classes are meant to allow that work to be independent of
// the way that the successor blocks are actually identified.
class switchSuccessorIterator;
class SuccessorIterator
   {
   public:
   virtual TR::Block *getFirstSuccessor() = 0;
   virtual TR::Block *getNextSuccessor() = 0;
   virtual switchSuccessorIterator *asSwitchSuccessor() { return NULL; }
   };
class switchSuccessorIterator : public SuccessorIterator
   {
   public:
   TR_ALLOC(TR_Memory::RegisterCandidates)
   switchSuccessorIterator(TR::Node *node) : node_(node), i_(node_->getCaseIndexUpperBound()) { }
   switchSuccessorIterator *asSwitchSuccessor() { return this; }
   TR::Block *getFirstSuccessor() { i_ = node_->getCaseIndexUpperBound(); return getNextSuccessor(); }
   TR::Block *getNextSuccessor()
      {
      for (i_ = (i_ > 0) ? i_ - 1 : 0;
           i_ > 0 && !node_->getChild((int32_t)i_)->getOpCode().isCase();
           --i_);
      if (i_ == 0)
         {
         return NULL;
         }
      else
         {
         return node_->getChild((int32_t)i_)->getBranchDestination()->getNode()->getBlock();
         }
      }
   TR::Node *getCaseNode()
      {
      TR_ASSERT(i_ > 0 && i_ < node_->getCaseIndexUpperBound(), "getCaseNode not called on valid successor");
      return node_->getChild((int32_t)i_);
      }
   private:
   TR::Node *            node_;
   intptr_t             i_;
   //TR::Compilation *     comp_;
   };
class multipleJumpSuccessorIterator : public SuccessorIterator
   {
   public:
   TR_ALLOC(TR_Memory::RegisterCandidates)
   multipleJumpSuccessorIterator(TR::Block * currBlock)
      {
      _list = &currBlock->getSuccessors();
      _iterator = _list->begin();
      }
   TR::Block *getFirstSuccessor()
      {
      _iterator = _list->begin();
      return getNextSuccessor_();
      }
   TR::Block *getNextSuccessor()
      {
      if (_iterator != _list->end())
         ++_iterator;
      return getNextSuccessor_();
      }
   private:
   TR::Block *getNextSuccessor_()
      {
      if (_iterator == _list->end())
         return NULL;
      else
         return (*_iterator)->getTo()->asBlock();
      }
   TR::CFGEdgeList::iterator _iterator;
   TR::CFGEdgeList* _list;
   };


///////////////////////////////////////////////////////////////////
// TR_GlobalRegisterAllocator
///////////////////////////////////////////////////////////////////

TR_GlobalRegisterAllocator::TR_GlobalRegisterAllocator(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _pairedSymbols(manager->trMemory()),
     _allocatedPerformCellSymRefs(manager->allocator()),
     _performCellCount(0),
     _newBlocks(manager->trMemory()),
     _nodesToBeChecked(manager->trMemory()),
     _nodeTriplesToBeChecked(manager->trMemory()),
     _rejectedSignExtNodes(manager->trMemory()),
     _storeSymRef(NULL),
     _seenInternalMethods(NULL),
     _osrCatchSucc(NULL),
     _defIndexToTempMap(manager->allocator())
   {}

void TR_GlobalRegisterAllocator::populateSymRefNodes(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().hasSymbolReference())
      _nodesForSymRefs[node->getSymbolReference()->getReferenceNumber()] = node;

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      populateSymRefNodes(child, visitCount);
      }
   }

/**
 * The cg considerTypeForGRA calls below are the functional tests (i.e. avoid BCD and some aggregates)
 * The allocateFor checks are for tuning for scalability when running in limited GRA mode
 */
bool TR_GlobalRegisterAllocator::allocateForSymRef(TR::SymbolReference *symRef)
   {
   return true;
   }

bool TR_GlobalRegisterAllocator::isSymRefAvailable(TR::SymbolReference *symRef)
   {
   if (!comp()->cg()->considerTypeForGRA(symRef))
      return false;

   return allocateForSymRef(symRef);
   }

/**
 * For the findLoops case consider the number of blocks too
 */
bool TR_GlobalRegisterAllocator::isSymRefAvailable(TR::SymbolReference *symRef, List<TR::Block> *blocksInLoop)
   {
   if (!comp()->cg()->considerTypeForGRA(symRef))
      return false;

   bool loopHasOneBlock = blocksInLoop && (blocksInLoop->getListHead()->getNextElement() == NULL);

   return true;
   }

bool TR_GlobalRegisterAllocator::allocateForType(TR::DataType dt)
   {
   return true;
   }

bool TR_GlobalRegisterAllocator::isNodeAvailable(TR::Node *node)
   {
   if (!comp()->cg()->considerTypeForGRA(node))
      return false;

   return allocateForType(node->getDataType());
   }

bool TR_GlobalRegisterAllocator::isTypeAvailable(TR::SymbolReference *symref)
   {
   if (!comp()->cg()->considerTypeForGRA(symref))
      return false;

   return allocateForType(symref->getSymbol()->getDataType());
   }

static void changeHeapBaseConstToLoad(TR::Compilation *comp, TR::SymbolReference * &autoSymRef, TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if ((node->getOpCodeValue() == TR::lconst) &&
       (node->getLongInt() == TR::Compiler->vm.heapBaseAddress()))
      {
      if (!autoSymRef)
         {
         autoSymRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), node->getDataType());
         TR::TreeTop *startTree = comp->getStartTree();
         TR::TreeTop *nextTree = startTree->getNextTreeTop();

         TR::Node *lconstNode = TR::Node::create(node, TR::lconst, 0, 0);
         lconstNode->setLongInt(node->getLongInt());
         TR::Node *storeNode = TR::Node::createWithSymRef(TR::lstore, 1, 1, lconstNode, autoSymRef);
         TR::TreeTop *tt = TR::TreeTop::create(comp, storeNode);
         startTree->join(tt);
         tt->join(nextTree);
         }

      TR::Node::recreate(node, TR::lload);
      node->setSymbolReference(autoSymRef);
      }

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      changeHeapBaseConstToLoad(comp, autoSymRef, child, visitCount);
      }
   }

void
TR_GlobalRegisterAllocator::visitNodeForDataType(TR::Node *node)
   {
   if(node->getVisitCount() >= comp()->getVisitCount())
      return;
   node->setVisitCount(comp()->getVisitCount());

   //visit children

   for(int32_t i = 0 ; i < node->getNumChildren() ; i++)
      {
      visitNodeForDataType(node->getChild(i));
      }


   if(!node->getOpCode().hasSymbolReference())
      return;


   if(node->getDataType() != node->getSymbol()->getDataType() && node->getSymbol()->getDataType() == TR::Aggregate)
      {
      comp()->cg()->addSymbolAndDataTypeToMap(node->getSymbol(),node->getDataType());
      }
   }

void
TR_GlobalRegisterAllocator::walkTreesAndCollectSymbolDataTypes()
   {
   comp()->incOrResetVisitCount();

   for (TR::TreeTop * tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      visitNodeForDataType(tt->getNode());
      }
   }

int32_t
TR_GlobalRegisterAllocator::perform()
   {
   LexicalTimer t("TR_GlobalRegisterAllocator::perform", comp()->phaseTimer());

   if (comp()->hasLargeNumberOfLoops())
      {
      return 0;
      }

   comp()->cg()->setGRACompleted();  // means "full GRA"

   if (comp()->getOption(TR_MimicInterpreterFrameShape) && ((!comp()->getOption(TR_EnableOSR) && !comp()->getOption(TR_FSDGRA)) || comp()->getJittedMethodSymbol()->sharesStackSlots(comp())))
      return 1;

   if (comp()->isGPUCompilation())
      return 1;


   walkTreesAndCollectSymbolDataTypes();

   comp()->getOptimizer()->setResetExitsGRA(0);
   comp()->getOptimizer()->setSeenBlocksGRA(0);
   comp()->getOptimizer()->setSuccessorBitsGRA(0);

   if (comp()->useCompressedPointers() &&
       comp()->cg()->materializesHeapBase() &&
       TR::Compiler->vm.heapBaseAddress() != 0)
      {
      TR::SymbolReference *autoSymRef = NULL;
      vcount_t visitCount = comp()->incVisitCount();
      for (TR::TreeTop * tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
         {
         changeHeapBaseConstToLoad(comp(), autoSymRef, tt->getNode(), visitCount);
         }
      if (autoSymRef)
         comp()->getSymRefTab()->aliasBuilder.createAliasInfo();
      }

   _storeSymRef = NULL;
   _rejectedSignExtNodes.deleteAll();

   bool globalFPAssignmentDone = false;
   _appendBlock = 0;

   TR::CFG * cfg = comp()->getFlowGraph();
   TR::Block * *cfgBlocks = cfg->createArrayOfBlocks();
   int32_t numberOfBlocks = cfg->getNextNodeNumber();

   TR_RegisterCandidates * candidates = comp()->getGlobalRegisterCandidates();
   candidates->_candidateForSymRefs = new (trStackMemory()) TR_RegisterCandidates::SymRefCandidateMap((TR_RegisterCandidates::SymRefCandidateMapComparator()), (TR_RegisterCandidates::SymRefCandidateMapAllocator(trMemory()->currentStackRegion())));
   TR_RegisterCandidate *rc = candidates->getFirst();
   for (; rc ; rc = rc->getNext())
      (*candidates->_candidateForSymRefs)[GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(rc->getSymbolReference())] = rc;

   candidates->_startOfExtendedBBForBB.init(trMemory(),
                                            (uint32_t)(comp()->getFlowGraph()->getNextNodeNumber() * sizeof(TR::Block *) * 1.5),
                                            false, stackAlloc);
   TR::Block * lastStartOfExtendedBB = comp()->getStartBlock();
   for (TR::Block * b = lastStartOfExtendedBB; b; b = b->getNextBlock())
      {
      lastStartOfExtendedBB = b->isExtensionOfPreviousBlock() ? lastStartOfExtendedBB : b;
      candidates->_startOfExtendedBBForBB[b->getNumber()] = lastStartOfExtendedBB;
      }

   comp()->getOptimizer()->setCachedExtendedBBInfoValid(true);

   if (cg()->getSupportsGlRegDeps() && !debug("disableGRA") && cg()->prepareForGRA())
      {
      static char *useFreqs = feGetEnv("TR_GRA_UseProfilingFrequencies");
      if (useFreqs)
        comp()->setUsesBlockFrequencyInGRA();

      TR_BitVector *liveVars = NULL;
      if (!cg()->getLiveLocals())
         {
         int32_t numLocals = 0;
         TR::AutomaticSymbol *a;
         ListIterator<TR::AutomaticSymbol> locals(&comp()->getMethodSymbol()->getAutomaticList());
         for (a = locals.getFirst(); a != NULL; a = locals.getNext())
            ++numLocals;

         if (comp()->getOption(TR_EnableAggressiveLiveness))
            {
            TR::ParameterSymbol *p;
            ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
            for (p = parms.getFirst(); p != NULL; p = parms.getNext())
               ++numLocals;
            }

         ListIterator<TR::RegisterMappedSymbol> methodMetaDataSymbols(&comp()->getMethodSymbol()->getMethodMetaDataList());
         if (comp()->allocateAtThisOptLevel())
            {
            for (auto mds = methodMetaDataSymbols.getFirst(); mds != NULL; mds = methodMetaDataSymbols.getNext())
               {
               TR_ASSERT(mds->isMethodMetaData(), "Should be method meta data");
               ++numLocals;
               }
            }

         const uint64_t MAX_BITVECTOR_MEMORY_USAGE = 1000000000;
         uint64_t bitvectorMemoryUsage = numLocals * comp()->getFlowGraph()->getNextNodeNumber();

         if (
             numLocals > 0 && (!trace() || performTransformation(comp(), "%s Performing liveness for Global Register Allocator\n", OPT_DETAILS)))
            {
            // Perform liveness analysis
            //
            TR_Liveness liveLocals(comp(), optimizer(), comp()->getFlowGraph()->getStructure(),
               false, NULL, false, comp()->getOption(TR_EnableAggressiveLiveness));
            if (comp()->getVisitCount() > HIGH_VISIT_COUNT)
               {
               comp()->resetVisitCounts(1);
               }

            for (TR::CFGNode *cfgNode = comp()->getFlowGraph()->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
                {
                TR::Block *block     = toBlock(cfgNode);
                int32_t blockNum    = block->getNumber();
                if (blockNum > 0 && liveLocals._blockAnalysisInfo[blockNum])
                   {
                   liveVars = new (trHeapMemory()) TR_BitVector(numLocals, trMemory());
                   *liveVars = *liveLocals._blockAnalysisInfo[blockNum];
                   block->setLiveLocals(liveVars);
                   }
                }

            // Make sure the code generator knows there are live locals for blocks, and
            // create a bit vector of the correct size for it.
            //
            liveVars = new (trHeapMemory()) TR_BitVector(numLocals, trMemory());
            cg()->setLiveLocals(liveVars);
            }
         }


      if (trace())
         comp()->dumpMethodTrees("Trees before tactical global register allocator", comp()->getMethodSymbol());

      _nodesToBeChecked.deleteAll();
      _nodeTriplesToBeChecked.deleteAll();
      _rejectedSignExtNodes.deleteAll();
      _candidatesNeedingSignExtension = NULL;
      _candidatesSignExtendedInThisLoop = NULL;

      _temp = NULL;
      _origSymRefCount = comp()->getSymRefCount();
      _temp2 = new (trStackMemory()) TR_BitVector(_origSymRefCount, trMemory(), stackAlloc);


      if (TR::Compiler->target.is64Bit() &&
          optimizer()->getUseDefInfo())
         {
         _temp = new (trStackMemory()) TR_BitVector(optimizer()->getUseDefInfo()->getNumDefNodes(), trMemory(), stackAlloc);
         _candidatesNeedingSignExtension = new (trStackMemory()) TR_BitVector(_origSymRefCount, trMemory(), stackAlloc);
         _candidatesSignExtendedInThisLoop = new (trStackMemory()) TR_BitVector(_origSymRefCount, trMemory(), stackAlloc);
         }

      candidates->getReferencedAutoSymRefs(comp()->trMemory()->currentStackRegion());
      if (!comp()->mayHaveLoops() || cg()->considerAllAutosAsTacticalGlobalRegisterCandidates())
         offerAllAutosAndRegisterParmAsCandidates(cfgBlocks, numberOfBlocks);
      else
         offerAllFPAutosAndParmsAsCandidates(cfgBlocks, numberOfBlocks);

      _registerCandidates = new (trStackMemory()) SymRefCandidateMap((SymRefCandidateMapComparator()), SymRefCandidateMapAllocator(trMemory()->currentStackRegion()));

      _candidates = comp()->getGlobalRegisterCandidates();

      for (TR_RegisterCandidate * rc = _candidates->getFirst(); rc; rc = rc->getNext())
         {
         (*_registerCandidates)[rc->getSymbolReference()->getReferenceNumber()] = rc;
         }

      findIfThenRegisterCandidates();

      findLoopAutoRegisterCandidates();

      if (comp()->getOptions()->realTimeGC() &&
          comp()->compilationShouldBeInterrupted(GRA_AFTER_FIND_LOOP_AUTO_CONTEXT))
         {
         comp()->failCompilation<TR::CompilationInterrupted>("interrupted during GRA");
         }

      bool canAffordAssignment = true;
      if (!comp()->getOption(TR_ProcessHugeMethods))
         {
         int32_t numCands = 0;
         for (TR_RegisterCandidate * rc = _candidates->getFirst(); rc; rc = rc->getNext())
            numCands++;

         int32_t hotnessFactor = 1;
         if (comp()->getMethodHotness() >= scorching)
            hotnessFactor = 4;
         else if (comp()->getMethodHotness() >= hot)
            hotnessFactor = 2;

         // Use double here so we don't need to worry about overflow
         //
         double complexityEstimate = comp()->getFlowGraph()->getNumberOfNodes() * (double)numCands * numCands;
         if (complexityEstimate / hotnessFactor > (double)GRA_COMPLEXITY_LIMIT)
            canAffordAssignment = false;
         }

      _valueModifiedSymRefs = new (trStackMemory()) TR_BitVector(_origSymRefCount, trMemory(), stackAlloc);
      TR_BitVector splitSymRefs(_origSymRefCount, trMemory(), stackAlloc);
      TR_BitVector nonSplittingCopyStored(_origSymRefCount, trMemory(), stackAlloc);

      //
      // Assign registers to candidates
      //
      if (canAffordAssignment)
         {
         globalFPAssignmentDone = _candidates->assign(cfgBlocks, numberOfBlocks, _firstGlobalRegisterNumber, _lastGlobalRegisterNumber);

         if (_lastGlobalRegisterNumber > -1)
            {
            _visitCount = comp()->incVisitCount();
            _gotoCreated = false;

            _signExtAdjustmentReqd = new (trStackMemory()) TR_BitVector(_lastGlobalRegisterNumber+1, trMemory(), stackAlloc);
            _signExtAdjustmentNotReqd = new (trStackMemory()) TR_BitVector(_lastGlobalRegisterNumber+1, trMemory(), stackAlloc);
            _signExtDifference = new (trStackMemory()) TR_BitVector(_lastGlobalRegisterNumber+1, trMemory(), stackAlloc);

            //
            // Transform IL
            //
            for (TR::TreeTop * tt = comp()->getStartTree(); tt; tt = tt->getExtendedBlockExitTreeTop()->getNextTreeTop())
               transformBlock(tt);

            TR::Node *node;
            ListIterator<TR::Node> nodesIt(&_nodesToBeChecked);
            for (node = nodesIt.getFirst(); node != NULL; node = nodesIt.getNext())
               {
               if (node->skipSignExtension() &&
                   (node->getOpCode().isLoadVarDirect() ||
                    (((node->getOpCodeValue() == TR::iadd) || (node->getOpCodeValue() == TR::isub)) &&
                     node->getFirstChild()->getOpCode().isLoadVarDirect() &&
                     node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
                     node->getSecondChild()->getOpCode().isLoadConst())))
                  node->setSkipSignExtension(false);
               }

            TR_NodeTriple *nodeTriple;
            ListIterator<TR_NodeTriple> nodeTriplesIt(&_nodeTriplesToBeChecked);
            for (nodeTriple = nodeTriplesIt.getFirst(); nodeTriple != NULL; nodeTriple = nodeTriplesIt.getNext())
               {
               if (trace())
                  traceMsg(comp(), "node1 %p node2 %p node3 %p \n", nodeTriple->_node1, nodeTriple->_node2, nodeTriple->_node3);

               TR::Node *node3 = NULL;
               if ((nodeTriple->_node3->getOpCodeValue() == TR::istore) || (nodeTriple->_node3->getOpCodeValue() == TR::iRegStore))
                  node3 = nodeTriple->_node3;
               else
                  node3 = nodeTriple->_node3->getChild(nodeTriple->_childNum);

               if ((nodeTriple->_node2->getOpCodeValue() == TR::iRegStore) &&
                   ((node3->getOpCodeValue() == TR::iRegStore) ||
                    (node3->getOpCodeValue() == TR::iRegLoad)) &&
                   (nodeTriple->_node2->getGlobalRegisterNumber() == node3->getGlobalRegisterNumber()))
                  {
                  //nodeTriple->_node1->setRegisterNumber(nodeTriple->_node2->getGlobalRegisterNumber());
                  }
               else
                  {
                  if (trace())
                     traceMsg(comp(), "Resetting property on %p\n", nodeTriple->_node1->getFirstChild());
                  nodeTriple->_node1->getChild(nodeTriple->_childNum)->setSkipSignExtension(false);
                  }
               }

            }

         bool mayHaveDeadStore = false;
         for (TR_RegisterCandidate * rc = _candidates->getFirst(); rc; rc = rc->getNext())
            {
            (*_registerCandidates)[rc->getSymbolReference()->getReferenceNumber()] = rc;
            TR::SymbolReference *splitSymRef = rc->getSplitSymbolReference();
            if (splitSymRef)
               {
               splitSymRefs.set(splitSymRef->getReferenceNumber());
               }

            ListIterator<TR::TreeTop> stores(&rc->getStores());
            TR::TreeTop * store = stores.getFirst();
            if (!rc->getValueModified())
               for (; store; store = stores.getNext())
                  {
                  TR::TransformUtil::removeTree(comp(), store);
                  }
           else
              {
              _valueModifiedSymRefs->set(rc->getSymbolReference()->getReferenceNumber());
              if (store)
                 mayHaveDeadStore = true;
              }
            }

         if (mayHaveDeadStore)
            {
            requestOpt(OMR::isolatedStoreGroup);
            requestOpt(OMR::globalDeadStoreElimination);
            if (_gotoCreated)
               requestOpt(OMR::redundantGotoElimination);
            }

         // Post processing to remove redundant stores for live-range splitting.
         //
         if (!splitSymRefs.isEmpty())
            {
            bool trace = comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);

            // If a candidate is modified, and it has a restoreSymbolReference,
            // then that one will be modified too (by the "restore" instruction
            // after the loop).  Hence, we propagage the modified info out of
            // every level of the loop nest.
            //
            for (TR_RegisterCandidate * rc = _candidates->getFirst(); rc; rc = rc->getNext())
               {
               TR::SymbolReference *outerSymRef = rc->getRestoreSymbolReference();
               while (outerSymRef)
                  {
                  TR_RegisterCandidate *outerRc = (*_registerCandidates)[outerSymRef->getReferenceNumber()];
                  if (!outerRc)
                     break;
                  if (_valueModifiedSymRefs->get(rc->getSymbolReference()->getReferenceNumber()))
                     _valueModifiedSymRefs->set(outerRc->getSymbolReference()->getReferenceNumber());
                  if (outerSymRef == rc->getSplitSymbolReference())
                     break;
                  outerSymRef = outerRc->getRestoreSymbolReference();
                  }
               }

            // Propagate the information about 'valueModified'.
            // 'valueModified' flag is not set at the stores (copies) for live-range splitting.
            // If source and destination locals of a copy receive registers, in other words, a register-register copy exists,
            // and a value is modified in one of locals, another local must be indentified as 'valueModified'.
            //
            if (trace)
               traceMsg(comp(), "\nPropagating value modified information\n");
            List<TR::TreeTop>        storesFromRegisters(trMemory());
            TR::TreeTop *tt = NULL, *nextTreeTop = NULL;
            for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
               {
               TR::Node *node = tt->getNode();
               if (!node->getOpCode().isStore() &&
                  (node->getNumChildren() > 0))
                  node = node->getFirstChild();

               if (isSplittingCopy(tt->getNode()))
                  {
                  TR::Node *store = tt->getNode(), *load = tt->getNode()->getFirstChild();
                  if (store->getOpCode().isStoreDirect() && load->getOpCode().isLoadReg()
                     && !(*_registerCandidates)[store->getSymbolReference()->getReferenceNumber()]->extendedLiveRange())
                     {
                     storesFromRegisters.add(tt);
                     }
                  }
               else if (node->getOpCode().isStoreDirect() && node->getSymbolReference()->getSymbol()->isAutoOrParm())
                  nonSplittingCopyStored.set(node->getSymbolReference()->getReferenceNumber());
               }

               // Restore original symbols. This can reduce the size of the stack area.
               // In addition, dead store elimination can remove redundant stores (copies)
               // where both of operands (source and destination) do not receive any register.
               //
               if (performTransformation(comp(), "%s Restoring original symbols from live range splitter\n", OPT_DETAILS))
                  {
                  vcount_t visitCount = comp()->incVisitCount();
                  for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
                     restoreOriginalSymbol(tt->getNode(), visitCount);
                  }

               // Remove a store generated at the end of the live range when a value is not modified in the live range.
               // If a value is not modified in a local which receives a register, the stores from the register is redundant.
               //
               if (trace)
                  traceMsg(comp(), "\nRemoving redundant stores\n");
               ListIterator<TR::TreeTop>        itr(&storesFromRegisters);
               for (tt = itr.getFirst(); tt; tt = itr.getNext())
                  {
                  if (!_valueModifiedSymRefs->isSet(tt->getNode()->getFirstChild()->getRegLoadStoreSymbolReference()->getReferenceNumber()) &&
                     !nonSplittingCopyStored.isSet(tt->getNode()->getFirstChild()->getRegLoadStoreSymbolReference()->getReferenceNumber()))
                     {
                     if (trace) traceMsg(comp(), "Remove a redundant store %p\n", tt->getNode());
                     TR::TransformUtil::removeTree(comp(), tt);
                     }
                  }

            }
         }
      }

   cg()->setLiveLocals(NULL);
   optimizer()->setUseDefInfo(NULL);
   optimizer()->setValueNumberInfo(NULL);

   TR::Block * block = comp()->getStartTree()->getNode()->getBlock();
   for (; block; block = block->getNextBlock())
      block->clearGlobalRegisters();

   candidates->releaseCandidates();

   _allocatedPerformCellSymRefs.Clear();

   return 1; // actual cost
   }


bool
TR_GlobalRegisterAllocator::isSplittingCopy(TR::Node *node)
   {
   bool trace = comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);

   // Check whether or not this store is a copy for live-range splitting
   if ((node->getOpCode().isStoreDirect() || node->getOpCode().isStoreReg()) &&
       (node->getFirstChild()->getOpCode().isLoadVarDirect() || node->getFirstChild()->getOpCode().isLoadReg()))
      {
      if (trace) traceMsg(comp(), "Finding a copy at node %p\n", node);
      TR::SymbolReference *storeSymRef = node->getSymbolReferenceOfAnyType();
      TR::SymbolReference *loadSymRef  = node->getFirstChild()->getSymbolReferenceOfAnyType();
      if (storeSymRef && loadSymRef && storeSymRef != loadSymRef)
         {
         TR_RegisterCandidate *storeRc = (*_registerCandidates)[storeSymRef->getReferenceNumber()];
         TR_RegisterCandidate *loadRc = (*_registerCandidates)[loadSymRef->getReferenceNumber()];
         TR::SymbolReference *origStoreSymRef = storeRc ? storeRc->getSplitSymbolReference() : NULL;
         TR::SymbolReference *origLoadSymRef = loadRc ? loadRc->getSplitSymbolReference() : NULL;
         if ((origStoreSymRef && origLoadSymRef && origStoreSymRef == origLoadSymRef) ||
             (origStoreSymRef && !origLoadSymRef && origStoreSymRef == loadSymRef) ||
             (!origStoreSymRef && origLoadSymRef && storeSymRef == origLoadSymRef))
            {
            //if (trace) traceMsg(comp(), "Found a copy %p\n", node);
            return true;
            }
         }
      }

   return false;
   }

void
TR_GlobalRegisterAllocator::restoreOriginalSymbol(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      restoreOriginalSymbol(node->getChild(i), visitCount);

   bool trace = comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);

   if (node->getOpCode().hasSymbolReference() || node->getOpCode().isLoadReg() || node->getOpCode().isStoreReg())
      {
      if (node->getSymbolReferenceOfAnyType())
         {
         int32_t symRefNum = node->getSymbolReferenceOfAnyType()->getReferenceNumber();
         TR_RegisterCandidate *rc = (*_registerCandidates)[symRefNum];
         TR::SymbolReference *origSymRef = rc ? rc->getRestoreSymbolReference() : NULL;
         bool foundChangeSymRef = false;
         bool setValueModified = false;
         TR::SymbolReference *changeSymRef = rc ? rc->getSplitSymbolReference() : NULL;
         while (origSymRef &&
                (origSymRef != rc->getSplitSymbolReference()))
            {
            TR_RegisterCandidate *origRc = (*_registerCandidates)[origSymRef->getReferenceNumber()];

            if (setValueModified)
               _valueModifiedSymRefs->set(origRc->getSymbolReference()->getReferenceNumber());

            if (!origRc ||
                 origRc->getValueModified() ||
                 origRc->extendedLiveRange())
               {
               if (!foundChangeSymRef)
                  {
                    if (origRc &&
                      !origRc->getValueModified() &&
                      origRc->getRestoreSymbolReference())
                    {
                    _valueModifiedSymRefs->set(origRc->getSymbolReference()->getReferenceNumber());
                    setValueModified = true;
                    }

                  foundChangeSymRef = true;
                  changeSymRef = origSymRef;
                  }
               }

            origSymRef = origRc->getRestoreSymbolReference();
            }

         TR_RegisterCandidate *oldRc = origSymRef ? (*_registerCandidates)[origSymRef->getReferenceNumber()] : 0;
         if (oldRc && oldRc->extendedLiveRange())
            {
            _valueModifiedSymRefs->set(oldRc->getSymbolReference()->getReferenceNumber());
            changeSymRef = NULL;
            }

         if (rc && !rc->extendedLiveRange() && changeSymRef)
            {
            if (trace) traceMsg(comp(), "Restore an original symbol #%d from #%d at %p\n", changeSymRef->getReferenceNumber(), symRefNum, node);
            if(node->getOpCode().isLoadReg() || node->getOpCode().isStoreReg())
               node->setRegLoadStoreSymbolReference(changeSymRef);
            else
               node->setSymbolReference(changeSymRef);
            }
         else
            _valueModifiedSymRefs->set(symRefNum);
         }
      else if (trace)
         traceMsg(comp(), "Node %p has no symbol\n", node);
      }
   }


/**
 * Transforms extended block
 * @param tt Is a bbStart
 */
void
TR_GlobalRegisterAllocator::transformBlock(TR::TreeTop * tt)
   {
   TR::Node * bbStart = tt->getNode();
   TR::Block * block = bbStart->getBlock();
   TR::Block * origBlock = block;

   // Find out if there are any symbols that are in registers on entry and/or exit
   //
   TR_Array<TR_GlobalRegister> & registers = block->getGlobalRegisters(comp());
   bool found = false;
   int32_t i;
   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR::Block * b = block;
      while (b)
         {
         TR_Array<TR_GlobalRegister> & curRegisters = b->getGlobalRegisters(comp());
         if (curRegisters[i].getRegisterCandidateOnEntry())
            found = true;
         if (curRegisters[i].getRegisterCandidateOnExit())
            found = true;

          b = b->getNextBlock();
          if (b && !b->isExtensionOfPreviousBlock())
             break;
          }
      }

   if (!found)
      {
      bbStart->setVisitCount(_visitCount);
      return;
      }

   _newBlocks.deleteAll();
   _signExtAdjustmentReqd->empty();
   _signExtAdjustmentNotReqd->empty();
   _signExtDifference->empty();

   //
   // Walk the tree changing all load/stores of register candidates to RegLoads and RegStores.
   // Also add RegLoads at branch points for registers live across the branch.
   //
   _storesInBlockInfo.setFirst(0);
   TR::Node * node = tt->getNode();
   TR_Array<TR_GlobalRegister> * curRegisters = NULL;
   TR_NodeMappings extBlockNodeMapping;

   do
      {
      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         curRegisters = &block->getGlobalRegisters(comp());

         // Mark all symbols that are in registers on entry and/or exit to this BB
         //
         for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
            {
            if ((*curRegisters)[i].getRegisterCandidateOnEntry())
               {
               (*curRegisters)[i].getRegisterCandidateOnEntry()->getSymbolReference()->getSymbol()->setIsInGlobalRegister(true);
               }

            if ((*curRegisters)[i].getRegisterCandidateOnExit())
               {
               (*curRegisters)[i].getRegisterCandidateOnExit()->getSymbolReference()->getSymbol()->setIsInGlobalRegister(true);
               }
            }
         }
      else if (node->getOpCodeValue() == TR::BBEnd)
         {
         block = node->getBlock();
         curRegisters = &block->getGlobalRegisters(comp());

         // Reset all tagged symbols for this BB
         //
         for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
            {
            if ((*curRegisters)[i].getRegisterCandidateOnEntry())
               {
               (*curRegisters)[i].getRegisterCandidateOnEntry()->getSymbolReference()->getSymbol()->setIsInGlobalRegister(false);
               }

            if ((*curRegisters)[i].getRegisterCandidateOnExit())
               {
               (*curRegisters)[i].getRegisterCandidateOnExit()->getSymbolReference()->getSymbol()->setIsInGlobalRegister(false);
               }
            }
         }

      transformNode(node, 0, 0, tt, block, *curRegisters, &extBlockNodeMapping);
      }
   while ((tt = tt->getNextTreeTop()) &&
          (node = tt->getNode(), node->getOpCodeValue() != TR::BBStart || node->getBlock()->isExtensionOfPreviousBlock()));

   *_signExtDifference = *_signExtAdjustmentNotReqd;
   *_signExtDifference &= *_signExtAdjustmentReqd;
   if (!_signExtDifference->isEmpty())
      {
      tt = origBlock->getEntry();
      node = tt->getNode();
      do
         {
         if (node->getOpCodeValue() == TR::treetop)
            node = node->getFirstChild();

         if (node->getOpCodeValue() == TR::iRegStore)
            {
            if (_signExtDifference->get(node->getGlobalRegisterNumber()))
               node->setNeedsSignExtension(true);
            }
         }
      while ((tt = tt->getNextTreeTop()) &&
             (node = tt->getNode(), node->getOpCodeValue() != TR::BBStart || node->getBlock()->isExtensionOfPreviousBlock()));
      }

   if (block == _appendBlock)
      _appendBlock = NULL;

   }


TR::Node *
TR_GlobalRegisterAllocator::resolveTypeMismatch(TR::DataType oldType, TR::Node *newNode)
   {
   return resolveTypeMismatch(oldType, NULL, newNode);
   }

TR::Node *
TR_GlobalRegisterAllocator::resolveTypeMismatch(TR::Node *oldNode, TR::Node *newNode)
   {
   return resolveTypeMismatch(TR::NoType, oldNode, newNode);
   }

TR::Node *
TR_GlobalRegisterAllocator::resolveTypeMismatch(TR::DataType inputOldType, TR::Node *oldNode, TR::Node *newNode)
   {
   return newNode;
   }

static void setAutoContainsRegisterValue(TR_RegisterCandidate *rc, TR_Array<TR_GlobalRegister> * extRegisters, int32_t i, TR::Compilation *comp)
   {
   bool needs2Regs = false;
   if (rc->rcNeeds2Regs(comp))
      needs2Regs = true;

   (*extRegisters)[i].setAutoContainsRegisterValue(true);

   if (needs2Regs)
      {
      int32_t highRegNum = rc->getHighGlobalRegisterNumber();
      if (i == highRegNum)
         {
         int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
         (*extRegisters)[lowRegNum].setAutoContainsRegisterValue(true);
         }
      else
         {
         (*extRegisters)[highRegNum].setAutoContainsRegisterValue(true);
         }
      }
   }

void
TR_GlobalRegisterAllocator::transformNode(
   TR::Node * node, TR::Node * parent, int32_t childIndex, TR::TreeTop * tt, TR::Block * & block, TR_Array<TR_GlobalRegister> & registers, TR_NodeMappings *extBlockNodeMapping)
   {
   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   TR_Array<TR_GlobalRegister> * extRegisters = &(_candidates->_startOfExtendedBBForBB[block->getNumber()]->getGlobalRegisters(comp()));

   if (node->getVisitCount() == _visitCount)
      return;
   node->setVisitCount(_visitCount);
   TR::Node* origStoreToMetaData = NULL;
   TR::Node* origLoadFromMetaData = NULL;
   TR::TreeTop* origLoadFromMetaDataPrevTT = NULL;

   TR::ILOpCode opcode = node->getOpCode();
   if (opcode.getOpCodeValue() == TR::table) // (opcode.isSwitch() && debug("putGlRegDepOnSwitch"))
      {
      transformNode(node->getFirstChild(), node, 0, tt, block, registers, extBlockNodeMapping);
      transformMultiWayBranch(tt, node, block, registers);
      return;
      }

   int32_t i;
   for (i = 0; i < node->getNumChildren(); ++i)
      transformNode(node->getChild(i), node, i, tt, block, registers, extBlockNodeMapping);

   bool transformDone = false;


   if (node->getOpCode().isJumpWithMultipleTargets() && !node->getOpCode().isSwitch() && node->getOpCodeValue() != TR::tstart)
      transformMultiWayBranch(tt, node, block, registers, transformDone);
   else if (opcode.isBranch())
      {
      transformBlockExit(tt, node, block, registers, node->getBranchDestination()->getNode()->getBlock());
      }
   else if (opcode.getOpCodeValue() == TR::BBStart)
      {
      block = node->getBlock();
      addCandidateReloadsToEntry(tt, *extRegisters, block);
      if (!block->isExtensionOfPreviousBlock())
         addRegLoadsToEntry(tt, registers, block);

      _osrCatchSucc = NULL;
      addStoresForCatchBlockLoads(tt, *extRegisters, block);
      }
   else if (opcode.getOpCodeValue() == TR::BBEnd)
      {
      TR::TreeTop * nextTT = tt->getNextTreeTop();
      TR::TreeTop * prevTT = tt->getPrevRealTreeTop();
      TR::Block * nextBlock = nextTT ? nextTT->getNode()->getBlock() : 0;

      while (prevTT && prevTT->getNode()->getOpCode().isStoreReg())
         prevTT = prevTT->getPrevTreeTop();

      TR::Node *ttNode = prevTT->getNode();

      if(ttNode->getOpCodeValue() == TR::treetop)
            ttNode = ttNode->getFirstChild();

      if (nextBlock && !nextBlock->isExtensionOfPreviousBlock() && block->hasSuccessor(nextBlock) && !ttNode->getOpCode().isJumpWithMultipleTargets() )
         {
         transformBlockExit(tt, node, block, registers, nextBlock);
         }
      }

   else if (opcode.isLoadVar() && isNodeAvailable(node))
      {
      TR::Symbol * symbol = node->getSymbolReference()->getSymbol();

      bool changeNode = true;
      TR::Node * value = NULL;
      TR_GlobalRegister *gr = NULL;
      //traceMsg(comp(), "Node %p symbol %p tag %d\n", node, symbol, symbol->isInGlobalRegister());

      changeNode = false;
      value = extBlockNodeMapping->getTo(node);
      if (value)
         {
         changeNode = true;
         }
      else if (symbol->isInGlobalRegister())
         {
         gr = getGlobalRegister(symbol, registers, block);
         if (gr)
           changeNode = true; // symbol is will be put into register right before exit
         }
      else
         {
         TR_GlobalRegister *ptrToGr = getGlobalRegisterWithoutChangingCurrentCandidate(symbol, registers, block);
         if (ptrToGr)
            {
            TR_RegisterCandidate * rc = ptrToGr->getCurrentRegisterCandidate();
            if (rc && (rc->getSymbolReference()->getSymbol() == symbol))
               {
               if (ptrToGr->getValue() &&
                   !ptrToGr->getAutoContainsRegisterValue())
                  ptrToGr->createStoreFromRegister(comp()->getVisitCount(), tt->getPrevTreeTop(), 1, comp(), true);
               ptrToGr->setAutoContainsRegisterValue(true);
               ptrToGr->setValue(0);

               bool needs2Regs = false;
               if (ptrToGr->getCurrentRegisterCandidate() &&
                   ptrToGr->getCurrentRegisterCandidate()->rcNeeds2Regs(comp()))
                  needs2Regs = true;

               if (needs2Regs)
                  {
                  TR_RegisterCandidate *currRC = ptrToGr->getCurrentRegisterCandidate();
                  int32_t highRegNum = currRC->getHighGlobalRegisterNumber();
                  if (ptrToGr == &((*extRegisters)[highRegNum]))
                     {
                     TR_GlobalRegister *grLow = &((*extRegisters)[currRC->getLowGlobalRegisterNumber()]);
                     if (grLow->getCurrentRegisterCandidate() == currRC)
                        {
                        grLow->setAutoContainsRegisterValue(true);
                        grLow->setValue(0);
                        }
                     }
                  else
                     {
                     TR_GlobalRegister *grHigh = &((*extRegisters)[currRC->getHighGlobalRegisterNumber()]);
                     if (grHigh->getCurrentRegisterCandidate() == currRC)
                        {
                        grHigh->setAutoContainsRegisterValue(true);
                        grHigh->setValue(0);
                        }
                     }
                  }
               }
            }
         }


      if (changeNode)
         {
         dumpOptDetails(comp(), "%s change load var [%p] of %s symRef#%d to load reg\n",
                        OPT_DETAILS,
                        node,
                        node->getSymbolReference()->getSymbol()->isMethodMetaData() ? node->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName() : "",
                        node->getSymbolReference()->getReferenceNumber());

         TR_ASSERT(parent, "shouldn't all loads have a TR::Node as a parent?");

         node->setVisitCount(_visitCount - 1);

         if (!value)
            {
            value = gr->getValue();
            if (comp()->cg()->needToAvoidCommoningInGRA() && value &&
                (!value->getOpCode().isLoadReg() && !value->getOpCode().isLoadVar()) &&
                 (symbol->getDataType() == TR::Float
                  || symbol->getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                  || symbol->getDataType() == TR::DecimalFloat
                  || symbol->getDataType() == TR::DecimalDouble
                  || symbol->getDataType() == TR::DecimalLongDouble
#endif
                  ) &&
                (!parent->getOpCode().isStore() &&
                 (parent->getOpCodeValue() != TR::fRegStore) &&
                 (parent->getOpCodeValue() != TR::dRegStore) &&
                 (parent->getOpCodeValue() != TR::GlRegDeps) &&
                 (parent->getOpCodeValue() != TR::PassThrough)))

               {

               StoresInBlockInfo *storeInfo = findRegInStoreInfo(gr);
               TR_ASSERT(storeInfo, "Must have a store before a load in this block\n");

               if (!storeInfo->_origStoreExists)
                  {
                  TR::TreeTop *regStoreTT = storeInfo->_lastStore;
                  TR::Node *regStoreNode =  regStoreTT->getNode();
                  TR_RegisterCandidate * rc = gr->getCurrentRegisterCandidate();
                  TR_ASSERT(rc, "must have a candidate\n");
                  TR::Node * store = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(rc->getDataType()), 1, 1, regStoreNode->getFirstChild(),rc->getSymbolReference());

                  if (regStoreNode->needsSignExtension())
                     store->setNeedsSignExtension(true);

                  TR::TreeTop *prevTree = regStoreTT->getPrevTreeTop();
                  TR::TreeTop *storeTreeTop = TR::TreeTop::create(comp(), prevTree, store);
                  storeInfo->_origStoreExists = true;
                  }
               dumpOptDetails(comp(), "%s did not change load var [%p] of symRef#%d to load reg because float/double issues\n",
                              OPT_DETAILS, node, node->getSymbolReference()->getReferenceNumber());
               return;

               }

            dumpOptDetails(comp(), "%s change load var [%p] of %s symRef#%d to load reg\n",
                           OPT_DETAILS,
                           node,
                           node->getSymbolReference()->getSymbol()->isMethodMetaData() ? node->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName() : "",
                           node->getSymbolReference()->getReferenceNumber());
            if (!value)
               {
               if (parent->getOpCode().isStore() &&
                   (parent->getSymbolReference() == node->getSymbolReference()))
                  {
                  TR_RegisterCandidate * rc = gr->getCurrentRegisterCandidate();
                  TR::SymbolReference *symRef = node->getSymbolReference();
                  if (!rc->is8BitGlobalGPR())
                     node->setIsInvalid8BitGlobalRegister(true);
                  node->setVisitCount(_visitCount);
                  value = node;
                  gr->setAutoContainsRegisterValue(true);
                  gr->setValue(value);
                  }
               else
                  {
                  TR::TreeTop *prevTree = tt->getPrevTreeTop();
                  TR::Node* loadVarNode = node->getSymbolReference()->getSymbol()->isMethodMetaData() &&
                                          node->getSymbolReference() != node->getSymbolReference() ? 0 : node;
                  value = gr->createStoreToRegister(prevTree, loadVarNode, _visitCount, comp(), this);
                  origLoadFromMetaData = 0 ? TR::Node::copy(node) : NULL;
                  origLoadFromMetaDataPrevTT = tt->getPrevTreeTop();
                  dumpOptDetails(comp(), "%s change load var [%p] of %s symRef#%d to load reg\n",
                                 OPT_DETAILS,
                                 node,
                                 node->getSymbolReference()->getSymbol()->isMethodMetaData() ? node->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName() : "",
                                 node->getSymbolReference()->getReferenceNumber());
                  }

               bool needs2Regs = false;
               if (gr->getCurrentRegisterCandidate() &&
                   gr->getCurrentRegisterCandidate()->rcNeeds2Regs(comp()))
                  needs2Regs = true;

               if (needs2Regs)
                  {
                  TR_RegisterCandidate *currRC = gr->getCurrentRegisterCandidate();
                  int32_t highRegNum = currRC->getHighGlobalRegisterNumber();
                  if (gr == &((*extRegisters)[highRegNum]))
                     {
                     TR_GlobalRegister *grLow = &((*extRegisters)[currRC->getLowGlobalRegisterNumber()]);
                     if (grLow->getCurrentRegisterCandidate() == currRC)
                        grLow->setAutoContainsRegisterValue(true);
                     }
                  else
                     {
                     TR_GlobalRegister *grHigh = &((*extRegisters)[currRC->getHighGlobalRegisterNumber()]);
                     if (grHigh->getCurrentRegisterCandidate() == currRC)
                        grHigh->setAutoContainsRegisterValue(true);
                     }
                  }

               if (node->isDontMoveUnderBranch())
                  {
                  //dumpOptDetails(comp(), "propagating dontMoveUnderBranch flag from original load node [%p] to new load node [%p]\n", node, value);
                  value->setIsDontMoveUnderBranch(true);
                  }
               }

            if (node->getReferenceCount() > 1)
               {
               extBlockNodeMapping->add(node, value, trMemory());

               TR_RegisterCandidate *rc = gr->getCurrentRegisterCandidate();
               if (rc->rcNeeds2Regs(comp()))
                  {
                  int32_t highRegNum = rc->getHighGlobalRegisterNumber();
                  if (i == highRegNum)
                     {
                     int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
                     TR_GlobalRegister & grLow = (*extRegisters)[lowRegNum];
                     grLow.setLastRefTreeTop(tt);
                     }
                  else
                     {
                     TR_GlobalRegister & grHigh = (*extRegisters)[highRegNum];
                     grHigh.setLastRefTreeTop(tt);
                     }
                  }
               }
            }

         if (value->getOpCode().isLoadReg() &&
               !(parent->getOpCode().getOpCodeValue()==TR::GlRegDeps) &&
               !value->isSeenRealReference())
            {
            //dumpOptDetails(comp(), " changed flags on node [%p] %s\n",node, node->getOpCode().getName());
            value->setFlags(node->getFlags());
            value->setSeenRealReference(true);
            value->copyByteCodeInfo(node);
            }

         if (node->skipSignExtension())
            value->setSkipSignExtension(true);

         TR::Node *oldValue = parent->getChild(childIndex);

         value = resolveTypeMismatch(oldValue, value);
         parent->setChild(childIndex, value);
         value->incReferenceCount();

         if (comp()->getOption(TR_CheckGRA)
            && performTransformation(comp(), "%s CheckGRA inserting BNDCHK to verify %s node %p\n", OPT_DETAILS, value->getOpCode().getName(), value))
            {
            TR::Node *zeroIfEqual = TR::Node::create(comp()->il.opCodeForCompareNotEquals(oldValue->getDataType()), 2, oldValue, value);
            TR::TreeTop::create(comp(), tt->getPrevTreeTop(),
               TR::Node::createWithSymRef(TR::BNDCHK, 2, 2,
                  TR::Node::iconst(oldValue, 1), // pretend we have an array of size=1 so only zeroIfEqual==0 is acceptable
                  zeroIfEqual,
                  comp()->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp()->getJittedMethodSymbol())));
            }

         oldValue->recursivelyDecReferenceCount();
         if ((oldValue->getReferenceCount() == 0) &&
             (oldValue->getOpCodeValue() == TR::iload) &&
             (value->getOpCodeValue() == TR::iRegLoad))
            {
            TR::Node::recreate(oldValue, TR::iRegLoad);
            oldValue->setGlobalRegisterNumber(value->getGlobalRegisterNumber());
            }

         if (value->getOpCode().isLoadReg())
            {
            TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
            TR_Array<TR_GlobalRegister> & extRegisters = _candidates->_startOfExtendedBBForBB[block->getNumber()]->getGlobalRegisters(comp());
            extRegisters[value->getGlobalRegisterNumber()].setLastRefTreeTop(tt);
            }
         }
      }
   else if (opcode.getOpCodeValue() == TR::loadaddr)
      {
      TR::Symbol * symbol = node->getSymbolReference()->getSymbol();
      if (symbol->isInGlobalRegister())
         {
         TR_GlobalRegister * gr = getGlobalRegister(symbol, registers, block);
         if (gr && gr->getValue() && !gr->getAutoContainsRegisterValue())
            {
            gr->createStoreFromRegister(_visitCount, tt->getPrevTreeTop(), -1, comp());
            bool needs2Regs = false;
            TR_RegisterCandidate *rc = gr->getCurrentRegisterCandidate();
            if (rc->rcNeeds2Regs(comp()))
               needs2Regs = true;
            if (needs2Regs)
               {
               int32_t highRegNum = rc->getHighGlobalRegisterNumber();
               if (i == highRegNum)
                  {
                  int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
                  (*extRegisters)[lowRegNum].setAutoContainsRegisterValue(true);
                  }
               else
                  {
                  (*extRegisters)[highRegNum].setAutoContainsRegisterValue(true);
                  }
               }
            }
         }
      else
         {
         TR_GlobalRegister *ptrToGr = getGlobalRegisterWithoutChangingCurrentCandidate(symbol, registers, block);
         if (ptrToGr)
            {
            TR_RegisterCandidate * rc = ptrToGr->getCurrentRegisterCandidate();
            if (rc && (rc->getSymbolReference()->getSymbol() == symbol))
               {
               if (ptrToGr->getValue() && !ptrToGr->getAutoContainsRegisterValue())
                  {
                  ptrToGr->createStoreFromRegister(comp()->getVisitCount(), tt->getPrevTreeTop(), 1, comp(), true);
                  }
               ptrToGr->setAutoContainsRegisterValue(true);
               ptrToGr->setValue(0);
               }
            }
         }

      }
   else if (opcode.isStore() && isNodeAvailable(node))
      {
      TR::SymbolReference * origSymRef = node->getSymbolReference();
      TR::SymbolReference * symRef = origSymRef;
      TR::Symbol * symbol = symRef->getSymbol();
      //traceMsg(comp(), "Store node %p\n", node);
      TR_GlobalRegister * gr;

      if (symbol->isInGlobalRegister() &&
          (gr = getGlobalRegister(symbol, registers, block)))
         {
           //traceMsg(comp(), "Store node %p sym is tagged\n", node);
         bool needs2Regs = false;
         TR_RegisterCandidate *rc = gr->getCurrentRegisterCandidate();
         if (rc->rcNeeds2Regs(comp()))
            needs2Regs = true;

         bool fpStore = false;
         if (symRef->getSymbol()->getDataType() == TR::Float
             || symRef->getSymbol()->getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
             || symRef->getSymbol()->getDataType() == TR::DecimalFloat
             || symRef->getSymbol()->getDataType() == TR::DecimalDouble
             || symRef->getSymbol()->getDataType() == TR::DecimalLongDouble
#endif
             )
            fpStore = true;

         // If this is an FP store, then the correct precision adjustment will
         // be done on IA32 by the global analysis GlobalFPStoreReloadOpt; so we
         // do not need to keep the store around for ANY FP store.
         // Also if it is a 'normal' int or address or long store, we do not need
         // to keep the store.
         //
         // However if storeCanBeRemoved returns false for (some special) stores created
         // by other optimization passes (like escape analysis), then for non FP stores
         // (FP stores are handled correct as explained above because we will insert store/reload
         // pairs wherever required based on GlobalFPStoreReloadOpt), we must keep the store intact.
         // Another case when we must keep the store to the stack location intact is if
         // it is a pinning array for an internal pointer, as the pinning array for an
         // internal pointer must always be present on the stack.
         //
         //
         if (!fpStore && !symRef->storeCanBeRemoved()
             || (comp()->getOption(TR_MimicInterpreterFrameShape) && comp()->getOption(TR_FSDGRA)))
            {
            gr->setValue(0);
            if (needs2Regs)
               {
               int32_t highRegNum = rc->getHighGlobalRegisterNumber();
               if (i == highRegNum)
                  {
                  int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
                  (*extRegisters)[lowRegNum].setValue(0);
                  }
               else
                  (*extRegisters)[highRegNum].setValue(0);
               }
            }
         else
            {
            TR_RegisterCandidate * rc = gr->getCurrentRegisterCandidate();
            dumpOptDetails(comp(), "%s change store var [%p] %s #%d to store reg\n", OPT_DETAILS, node, rc->getSymbolReference()->getSymbol()->isMethodMetaData() ? rc->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName():"",rc->getSymbolReference()->getReferenceNumber());

            bool avoidDuplicateFPStackValues = false;
            if (fpStore &&
                !comp()->cg()->getSupportsJavaFloatSemantics())
               {
               int32_t i;
               for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
                  {
                  TR::Block * b = block;
                  while (b)
                     {
                     TR_Array<TR_GlobalRegister> & curRegisters = b->getGlobalRegisters(comp());

                     if (curRegisters[i].getRegisterCandidateOnExit() &&
                         (curRegisters[i].getRegisterCandidateOnExit() != rc))
                        {
                        if ((*extRegisters)[i].getValue() == node->getFirstChild())
                           {
                           //gr.setValue(0);
                           (*extRegisters)[rc->getGlobalRegisterNumber()].setValue(0);
                           //gr.setAutoContainsRegisterValue(false);
                           //rc->setValueModified(true);
                           //gr.setLastRefTreeTop(0);
                           avoidDuplicateFPStackValues = true;
                           break;
                           }
                        }

                     b = b->getNextBlock();
                     if (b && !b->isExtensionOfPreviousBlock())
                        break;
                     }

                  if (avoidDuplicateFPStackValues)
                     break;
                  }
               }

            if (avoidDuplicateFPStackValues)
               return;
            StoresInBlockInfo *storeInfo = NULL;
            if (comp()->cg()->needToAvoidCommoningInGRA() && fpStore)
               {

               storeInfo = findRegInStoreInfo(gr);
               if (!storeInfo)
                  {
                  storeInfo = new (trStackMemory()) StoresInBlockInfo;
                  _storesInBlockInfo.add(storeInfo);
                  }
               storeInfo->_gr = gr;
               storeInfo->_lastStore = tt;
               storeInfo->_origStoreExists = false;
               }

            bool storeIntact = false;
            TR::Node *newValueNode = NULL;

            comp()->setCurrentBlock(block);

            if (_osrCatchSucc
                || symRef->getSymbol()->dontEliminateStores(comp())
                || keepAllStores
                || rc->isLiveAcrossExceptionEdge()
                || (comp()->getOption(TR_CheckGRA)
                  && performTransformation(comp(), "%s CheckGRA keeping %s node %p\n", OPT_DETAILS, node->getOpCode().getName(), node)))
               {
               storeIntact = true;
               // Do not eliminate the original store in this case.
               // Pinning arrays must be accessible from the stack for GC to work correctly.
               // Candidates live across exception edges must have the correct value
               // in the temp so that the value can be reloaded in code reachable from
               // the catch block. Note that we ensure the candidate is not live on entry
               // in the ctach block. This ensures that it will be re-loaded into the global
               // register from the stack, prior to its use.
               //
               TR::Node * child = node->getFirstChild();
               TR::Node *newNode = NULL;
                  {
                  TR::DataType regStoreType = node->getDataType();
                  newNode = TR::Node::create(comp()->il.opCodeForRegisterStore(regStoreType), 1, child);
                  newNode->setRegLoadStoreSymbolReference(symRef);
                  }
               if (node->needsSignExtension())
                  {
                  newNode->setNeedsSignExtension(true);
                  setSignExtensionRequired(true, rc->getGlobalRegisterNumber());
                  }
               else
                  setSignExtensionNotRequired(true, rc->getGlobalRegisterNumber());

               TR::TreeTop *prevTree = tt->getPrevTreeTop();
               TR::TreeTop *newStore = TR::TreeTop::create(comp(), prevTree, newNode);
               node = newNode;
               if (comp()->cg()->needToAvoidCommoningInGRA() && fpStore)
                  storeInfo->_origStoreExists = true;
               }
            else
               {
               origStoreToMetaData = NULL;
                  {
                  TR::DataType regStoreType = node->getDataType();
                  TR::Node::recreate(node, comp()->il.opCodeForRegisterStore(regStoreType));
                  }

               if (node->needsSignExtension())
                  {
                  setSignExtensionRequired(true, rc->getGlobalRegisterNumber());
                  }
               else
                  setSignExtensionNotRequired(true, rc->getGlobalRegisterNumber());
               }

            if (node->requiresRegisterPair(comp()))
               {
               node->setLowGlobalRegisterNumber(rc->getLowGlobalRegisterNumber());
               node->setHighGlobalRegisterNumber(rc->getHighGlobalRegisterNumber());
               }
            else
               node->setGlobalRegisterNumber(rc->getGlobalRegisterNumber());

            if (!gr->getCurrentRegisterCandidate()->is8BitGlobalGPR())
               node->getFirstChild()->setIsInvalid8BitGlobalRegister(true);
            gr->setValue(newValueNode ? newValueNode : node->getFirstChild());
            //traceMsg(comp(), "Store node first child %p\n", node->getFirstChild());
            if (!storeIntact)
               gr->setAutoContainsRegisterValue(false);
            //if (!isSplittingCopy(node))
               rc->setValueModified(true);
            //rc->setValueModified(true);
            gr->setLastRefTreeTop(tt);

            if (needs2Regs)
               {
               int32_t highRegNum = rc->getHighGlobalRegisterNumber();
               if (i == highRegNum)
                  {
                  int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
                  (*extRegisters)[lowRegNum].setValue(newValueNode ? newValueNode : node->getFirstChild());
                  if (!storeIntact)
                     (*extRegisters)[lowRegNum].setAutoContainsRegisterValue(false);
                  (*extRegisters)[lowRegNum].setLastRefTreeTop(tt);
                  }
               else
                  {
                  (*extRegisters)[highRegNum].setValue(newValueNode ? newValueNode : node->getFirstChild());
                  if (!storeIntact)
                     (*extRegisters)[highRegNum].setAutoContainsRegisterValue(false);
                  (*extRegisters)[highRegNum].setLastRefTreeTop(tt);
                  }
               }
            }
         }
      else
         {
         TR_GlobalRegister *ptrToGr = getGlobalRegisterWithoutChangingCurrentCandidate(symbol, registers, block);

         //traceMsg(comp(), "ptrToGr %p node %p mapping %d\n", ptrToGr, node, ptrToGr ? ptrToGr->getMappings().getTo(node) : 0);
         if (ptrToGr)
            {
            //printf("Ignoring store\n"); fflush(stdout);
            //traceMsg(comp(), "Ignoring store %p\n", node);
            TR_RegisterCandidate * rc = ptrToGr->getCurrentRegisterCandidate();
            if (rc && (rc->getSymbolReference()->getSymbol() == symbol))
               {
               if (rc->getSymbolReference()->getSymbol()->isMethodMetaData() && rc->getSymbolReference() != origSymRef && ptrToGr->getValue() &&
                   !ptrToGr->getAutoContainsRegisterValue()  )
                  ptrToGr->createStoreFromRegister(comp()->getVisitCount(), tt->getPrevTreeTop(), 1, comp(), true);
               ptrToGr->setAutoContainsRegisterValue(true);
               ptrToGr->setValue(0);

              bool needs2Regs = false;
               if (ptrToGr->getCurrentRegisterCandidate() &&
                   ptrToGr->getCurrentRegisterCandidate()->rcNeeds2Regs(comp()))
                  needs2Regs = true;

               if (needs2Regs)
                  {
                  TR_RegisterCandidate *currRC = ptrToGr->getCurrentRegisterCandidate();
                  int32_t highRegNum = currRC->getHighGlobalRegisterNumber();
                  if (ptrToGr == &((*extRegisters)[highRegNum]))
                     {
                     TR_GlobalRegister *grLow = &((*extRegisters)[currRC->getLowGlobalRegisterNumber()]);
                     if (grLow->getCurrentRegisterCandidate() == currRC)
                        {
                        grLow->setAutoContainsRegisterValue(true);
                        grLow->setValue(0);
                        }
                     }
                  else
                     {
                     TR_GlobalRegister *grHigh = &((*extRegisters)[currRC->getHighGlobalRegisterNumber()]);
                     if (grHigh->getCurrentRegisterCandidate() == currRC)
                        {
                        grHigh->setAutoContainsRegisterValue(true);
                        grHigh->setValue(0);
                        }
                     }
                  }
               }
            }
         }
      }
   }

/**
 * This routine is used for switch op-codes and for igoto op-codes
 */
void
TR_GlobalRegisterAllocator::transformMultiWayBranch(
   TR::TreeTop * exitTreeTop, TR::Node * node, TR::Block * block, TR_Array<TR_GlobalRegister> & registers, bool regStarTransformDone)
   {
   TR_Array<TR::Node *> regDepNodes(trMemory(), _lastGlobalRegisterNumber + 1, true, stackAlloc);;

   SuccessorIterator *si;
   if (node->getOpCode().isSwitch())
      {
      si = new (comp()->trStackMemory()) switchSuccessorIterator(node);
      }
   else
      {
      if(node->getOpCodeValue() == TR::treetop)
          node = node->getFirstChild();
      TR_ASSERT(node->getOpCode().isJumpWithMultipleTargets(),"transform multi-way branch called with unknown op-code");
      si = new (comp()->trStackMemory()) multipleJumpSuccessorIterator(block);
      }

   for (TR::Block * successorBlock = si->getFirstSuccessor();
        successorBlock != NULL;
        successorBlock = si->getNextSuccessor())
      {
      TR::Node * caseNode;
      if (node->getOpCode().isSwitch())
         {
         caseNode = si->asSwitchSuccessor()->getCaseNode();
         }
      else
         {
         caseNode = node;
         }
      if(successorBlock->isExtensionOfPreviousBlock() == false)
         {
         prepareForBlockExit(exitTreeTop, caseNode, block, registers, successorBlock, regDepNodes);
         }
      }

   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");


   if (node->getOpCode().getOpCodeValue() != TR::tstart)
      {
      TR::Node *exitNode = node->getOpCode().isSwitch() ? node->getSecondChild() : node;

      if (block->getNextBlock() && !block->getNextBlock()->isExtensionOfPreviousBlock() && block->hasSuccessor(block->getNextBlock()))
         {
         if  (!regStarTransformDone)
            {
            addGlRegDepToExit(regDepNodes, exitNode, _candidates->_startOfExtendedBBForBB[block->getNumber()]->getGlobalRegisters(comp()), block);
            }
         exitNode = block->getExit()->getNode();
        }

      addGlRegDepToExit(regDepNodes, exitNode, _candidates->_startOfExtendedBBForBB[block->getNumber()]->getGlobalRegisters(comp()), block);
      }
   }


void
TR_GlobalRegisterAllocator::transformBlockExit(
   TR::TreeTop * exitTreeTop, TR::Node * exitNode, TR::Block * block,
   TR_Array<TR_GlobalRegister> & registers, TR::Block * successorBlock)
   {
   TR_Array<TR::Node *> regDepNodes(trMemory(), _lastGlobalRegisterNumber + 1, true, stackAlloc);;

   prepareForBlockExit(exitTreeTop, exitNode, block, registers, successorBlock, regDepNodes);

   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   addGlRegDepToExit(regDepNodes, exitNode, _candidates->_startOfExtendedBBForBB[block->getNumber()]->getGlobalRegisters(comp()), block);
   }

/**
 * The method below is used to record the GlobalRegister to be reloaded into register
 * is successors of the passed block should we be not able to reload it in the current block
 * for reasons such as stack not being available
 */
bool TR_GlobalRegisterAllocator::markCandidateForReloadInSuccessors(int32_t i, TR_GlobalRegister *extReg, TR_GlobalRegister *reg, TR::Block *block, bool traceIt)
   {
   bool result = false;    // Success or no?
   TR::Block *nextBlock = NULL;
   TR_RegisterCandidate *rc = extReg->getCurrentRegisterCandidate();
   TR_RegisterCandidate *nextRc = NULL;

   if (traceIt) traceMsg(comp(),"TR_GlobalRegisterAllocator::markCandidateForReloadInSuccessors block=%d GlobalReg=(%d,symRef=#%d)\n",block->getNumber(),i,rc->getSymbolReference()->getReferenceNumber());
   // Alread visited this block?
   if (reg->isUnavailable())
      return(reg->isUnavailableResolved());

   reg->setUnavailable();

   // If it is not live on exit then nothing to do
   if (reg->getRegisterCandidateOnExit() != rc && !block->getNextBlock()->isExtensionOfPreviousBlock())
      return true;

   if (traceIt) traceMsg(comp(),"TR_GlobalRegisterAllocator::markCandidateForReloadInSuccessors checking extensions\n");
   // First visit all extensions of this block before doing any loops
   nextBlock = block->getNextBlock();
   if (nextBlock && nextBlock->isExtensionOfPreviousBlock())
      {
      if (traceIt) traceMsg(comp(),"TR_GlobalRegisterAllocator::markCandidateForReloadInSuccessors nextBlock=%d\n",nextBlock->getNumber());
      TR_Array<TR_GlobalRegister> & nextRegisters = nextBlock->getGlobalRegisters(comp());
      TR_GlobalRegister &nextGr = nextRegisters[i];
      nextRc = nextGr.getRegisterCandidateOnEntry();
      if (nextRc && nextRc != rc) // Not live anymore so leave. Should be caught on previous test on live on exit
         {
         if (traceIt) traceMsg(comp(), "  not live on entry. Ok here.\n");
         reg->setUnavailableResolved();
         return true;
         }

      // Continue the search if we still restricted from reloading it
      nextGr.setReloadRegisterCandidateOnEntry(rc);
      if (traceIt) traceMsg(comp(), "  block_%d marked to reload candidate #%d\n", nextBlock->getNumber(),rc->getSymbolReference()->getReferenceNumber());
      reg->setUnavailableResolved();
      return true; // Yes I managed to set reload on Entry for this successor path
      }
   else
      {
      if (traceIt) traceMsg(comp(),"TR_GlobalRegisterAllocator::markCandidateForReloadInSuccessors next block is not extension\n");
      // Multiple successors must visit all and set them as needing a reload on entry
      for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
         {
         nextBlock = (*succ)->getTo()->asBlock();

         TR_Array<TR_GlobalRegister> & nextRegisters = nextBlock->getGlobalRegisters(comp());
         TR_GlobalRegister &nextGr = nextRegisters[i];
         nextRc = nextGr.getRegisterCandidateOnEntry();

         if (nextRc == NULL || nextRc != rc)// not live on entry in successor, continue
            continue;

         // Continue the search if we still restricted from reloading it
         nextGr.setReloadRegisterCandidateOnEntry(rc);
         if (traceIt) traceMsg(comp(), "  block_%d marked to reload candidate #%d\n", nextBlock->getNumber(),rc->getSymbolReference()->getReferenceNumber());
         reg->setUnavailableResolved();
         result = true; // Yes I managed to set reload on Entry for this successor path
         }
     }
   reg->setUnavailableResolved(result);
   return result;
   }

void
TR_GlobalRegisterAllocator::reloadNonRegStarVariables(TR::TreeTop *tt, TR::Node *node, TR::Block *block, bool traceIt)
   {
   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   TR_Array<TR_GlobalRegister> & extRegisters = block->startOfExtendedBlock()->getGlobalRegisters(comp());

   TR::SparseBitVector killedAliases(comp()->allocator());
   node->mayKill(true).getAliases(killedAliases);
   }

void
TR_GlobalRegisterAllocator::addCandidateReloadsToEntry(TR::TreeTop * bbStartTT, TR_Array<TR_GlobalRegister> & registers, TR::Block *block)
   {
   TR::Node * bbStart = bbStartTT->getNode();
   int32_t i;
   TR_Array<TR_GlobalRegister> & currRegisters = block->getGlobalRegisters(comp());

   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      if (currRegisters[i].getReloadRegisterCandidateOnEntry())
         {
         TR_RegisterCandidate *candidateOnEntry = currRegisters[i].getRegisterCandidateOnEntry();
         currRegisters[i].setCurrentRegisterCandidate(candidateOnEntry, 0, 0, i, comp());
         currRegisters[i].createStoreToRegister(bbStartTT, NULL, _visitCount, comp(), this);
         }
      }
   }

void
TR_GlobalRegisterAllocator::addStoresForCatchBlockLoads(TR::TreeTop *appendPoint, TR_Array<TR_GlobalRegister> &registers, TR::Block *throwingBlock)
   {
   if (!throwingBlock->hasExceptionSuccessors() || !comp()->penalizePredsOfOSRCatchBlocksInGRA())
      return;

   // For most catch block loads, we currently just don't remove any candidate stores,
   // so there's no need to emit stores before taking the exception edge because
   // the auto always has the right value.
   // For OSR, however, we are particularly interested in eliminating those stores,
   // so now when we reach a block with an outgoing OSR exception edge, we must
   // emit the stores.
   //
   _osrCatchSucc = NULL;
   for (auto nextEdge = throwingBlock->getExceptionSuccessors().begin(); (nextEdge != throwingBlock->getExceptionSuccessors().end()) && !_osrCatchSucc; ++nextEdge)
      {
      TR::CFGNode *succ = (*nextEdge)->getTo();
      if (succ->asBlock()->isOSRCatchBlock())
         {
         _osrCatchSucc = succ->asBlock();
         if (trace())
            traceMsg(comp(), "           addStoresForCatchBlockLoads([%p], block_%d) found OSR catch block_%d\n", appendPoint->getNode(), throwingBlock->getNumber(), _osrCatchSucc->getNumber());
         }
      }

   if (!_osrCatchSucc)
      return;

   for (int32_t i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR_RegisterCandidate *rc = registers[i].getCurrentRegisterCandidate();

      if (_osrCatchSucc && rc && !rc->isLiveAcrossExceptionEdge() &&
          rc->symbolIsLive(_osrCatchSucc) && registers[i].getValue())
         {
         if (!registers[i].getAutoContainsRegisterValue())
            {
            registers[i].createStoreFromRegister(comp()->getVisitCount(), appendPoint, 1, comp(), true);
            setAutoContainsRegisterValue(rc, &registers, i, comp());
            registers[i].setLastRefTreeTop(appendPoint->getNextTreeTop());
            }

         registers[i].setValue(0);
         }
      }
   }

void
TR_GlobalRegisterAllocator::addRegLoadsToEntry(TR::TreeTop * bbStartTT, TR_Array<TR_GlobalRegister> & registers, TR::Block *block)
   {
   TR::Node * bbStart = bbStartTT->getNode();
   comp()->setCurrentBlock(bbStartTT->getEnclosingBlock());
   TR_Array<TR_GlobalRegister> & currRegisters = block->getGlobalRegisters(comp());
   int32_t numLoads = 0;
   TR_ScratchList<TR_RegisterCandidate> seenCandidates(trMemory());
   int32_t i;
   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR_RegisterCandidate * rc = registers[i].getRegisterCandidateOnEntry();
      if (rc && !seenCandidates.find(rc) &&
          !currRegisters[i].getReloadRegisterCandidateOnEntry() && !currRegisters[i].isUnavailable())
         {
         seenCandidates.add(rc);
         ++numLoads;
         }
      }

   if (!numLoads)
      return;

   seenCandidates.deleteAll();

   // Traditional tactical GRA creates GlRegDeps with RegLoads
   TR::Node * regDepNode = TR::Node::create(bbStart, TR::GlRegDeps, numLoads);
   dumpOptDetails(comp(), "%s create TR::GlRegDeps [%p] on BBStart [%p]\n", OPT_DETAILS, regDepNode, bbStart);
   numLoads = 0;

   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR_RegisterCandidate *candidateOnEntry = registers[i].getRegisterCandidateOnEntry();

      registers[i].setCurrentRegisterCandidate(candidateOnEntry, 0, 0, i, comp());
      if (!currRegisters[i].getReloadRegisterCandidateOnEntry() && !currRegisters[i].isUnavailable())
         {
         if (candidateOnEntry && !seenCandidates.find(candidateOnEntry))
            {
            seenCandidates.add(candidateOnEntry);
            regDepNode->setAndIncChild(numLoads++, registers[i].createLoadFromRegister(bbStart, comp()));
            }
         registers[i].setLastRefTreeTop(bbStartTT);
         }
      }

   bbStart->setAndIncChild(0, regDepNode);
   bbStart->setNumChildren(1);
   }

void
TR_GlobalRegisterAllocator::addGlRegDepToExit(
   TR_Array<TR::Node *> & regDepNodes, TR::Node * exitNode, TR_Array<TR_GlobalRegister> & registers, TR::Block *block)
   {
   int32_t numLoads = 0, i;

   TR_ScratchList<TR_RegisterCandidate> seenCandidates(trMemory());
   TR_Array<TR_GlobalRegister> & currRegisters = block->getGlobalRegisters(comp());
   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      if (regDepNodes[i])
         {
         TR_RegisterCandidate *candidate = registers[i].getCurrentRegisterCandidate();
         //traceMsg(comp(), "real reg %d exit node %p reg dep node %p candidate %d\n", i, exitNode, regDepNodes[i], (candidate ? candidate->getSymbolReference()->getReferenceNumber() : 0));
         if (candidate && !seenCandidates.find(candidate) && !currRegisters[i].isUnavailable())
            {
            seenCandidates.add(candidate);
            ++numLoads;
            }
         }
      }

   seenCandidates.deleteAll();

   if (numLoads)
      {
      TR::Node * glRegDep = TR::Node::create(exitNode, TR::GlRegDeps, numLoads);
      for (numLoads = 0, i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
         {
         if (regDepNodes[i])
            {
            TR_RegisterCandidate *candidate = registers[i].getCurrentRegisterCandidate();
            if (candidate && !seenCandidates.find(candidate) && !currRegisters[i].isUnavailable())
               {
               seenCandidates.add(candidate);
               glRegDep->setAndIncChild(numLoads++, regDepNodes[i]);
               }
            }
         }

      exitNode->setAndIncChild(exitNode->getNumChildren(), glRegDep);
      exitNode->setNumChildren(exitNode->getNumChildren() + 1);

      dumpOptDetails(comp(), "%s create TR::GlRegDeps [" POINTER_PRINTF_FORMAT "] on exit node [" POINTER_PRINTF_FORMAT "]\n", OPT_DETAILS, glRegDep, exitNode);
      }
   }

TR::TreeTop *
TR_GlobalRegisterAllocator::findPrevTreeTop(
   TR::TreeTop * & exitTreeTop, TR::Node * & exitNode, TR::Block * block, TR::Block * successorBlock)
   {
   if (exitNode->getOpCode().getOpCodeValue() == TR::BBEnd)
      {
      exitTreeTop = extendBlock(block, successorBlock)->getExit();
      exitNode = exitTreeTop->getNode();
      }
   return exitTreeTop->getPrevTreeTop();
   }

void
TR_GlobalRegisterAllocator::prepareForBlockExit(
   TR::TreeTop * & exitTreeTop, TR::Node * & exitNode, TR::Block * block,
   TR_Array<TR_GlobalRegister> & registers, TR::Block * successorBlock,
   TR_Array<TR::Node *> & regDepNodes)
   {
   TR::Block * originalSuccessorBlock = successorBlock;
   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   TR_Array<TR_GlobalRegister> & extRegisters = _candidates->_startOfExtendedBBForBB[block->getNumber()]->getGlobalRegisters(comp());

   TR::TreeTop * prevTreeTop = 0;
   TR_ScratchList<TR_RegisterCandidate> seenCurrCandidates(trMemory()), seenExitCandidates(trMemory());

   int32_t i;
   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR_GlobalRegister * gr = &registers[i];
      TR_GlobalRegister * extgr = &extRegisters[i];
      bool currCandidateAlreadySeen = false, exitCandidateAlreadySeen = false;

      TR_RegisterCandidate *candidate = gr->getRegisterCandidateOnExit();
      if (successorBlock->getGlobalRegisters(comp())[i].getRegisterCandidateOnEntry() != gr->getRegisterCandidateOnExit())
         candidate = extgr->getCurrentRegisterCandidate();
      if (candidate &&
          !seenExitCandidates.find(candidate))
         seenExitCandidates.add(candidate);
      else if (candidate)
         exitCandidateAlreadySeen = true; // only true for longs

      candidate = extgr->getCurrentRegisterCandidate();
      if (candidate &&
          !seenCurrCandidates.find(candidate))
         seenCurrCandidates.add(candidate);
      else if (candidate)
         currCandidateAlreadySeen = true; // only true for longs

      /////dumpOptDetails(comp(), "i == %d extgr->getValue %x autoContainsRegisterValue %d\n", i, extgr->getValue(), extgr->getAutoContainsRegisterValue());
      //
      // handle end of live range
      //

      if (extgr->getValue())
         {
         bool mustBeLiveAcrossAllPaths = (extgr->getValue()->getOpCode().isFloatingPoint() &&
                                          !cg()->getSupportsJavaFloatSemantics());

         if ((!extgr->getAutoContainsRegisterValue() || mustBeLiveAcrossAllPaths) &&
             !registerIsLiveAcrossEdge(exitTreeTop, exitNode, block, extgr, successorBlock, i))
            {
            TR_RegisterCandidate * rc = extgr->getCurrentRegisterCandidate();
            TR::CFGEdge *succ;
            bool liveOnSomeSucc = true;
            if (liveOnSomeSucc &&
                !extgr->getAutoContainsRegisterValue() &&
                !currCandidateAlreadySeen)
               {
               if ((block->getSuccessors().size() == 1) || rc != gr->getRegisterCandidateOnExit())
                  {
                  extgr->createStoreFromRegister(_visitCount, extgr->optimalPlacementForStore(block, comp()), i, comp());
                  }
               else
                  {
                  if (!prevTreeTop)
                     prevTreeTop = findPrevTreeTop(exitTreeTop, exitNode, block, successorBlock);
                  extgr->createStoreFromRegister(_visitCount, prevTreeTop, i, comp());
                  }

                bool needs2Regs = false;
                if (rc->rcNeeds2Regs(comp()))
                   needs2Regs = true;
                if (needs2Regs)
                   {
                   int32_t highRegNum = rc->getHighGlobalRegisterNumber();
                   if (i == highRegNum)
                     {
                     int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
                     if (extRegisters[lowRegNum].getCurrentRegisterCandidate() == rc)
                        extRegisters[lowRegNum].setAutoContainsRegisterValue(true);
                     }
                  else
                     {
                     if (extRegisters[highRegNum].getCurrentRegisterCandidate() == rc)
                        extRegisters[highRegNum].setAutoContainsRegisterValue(true);
                     }
                  }
               }
            if (mustBeLiveAcrossAllPaths)
               extgr->setValue(0); // must ensure the stack is cleared across the edge
            }
         }

      // create reg dep nodes
      //
      // note: successorBlock may have changed during call to registerIsLiveAcrossEdge
      //
      TR_RegisterCandidate * successorRC = successorBlock->getGlobalRegisters(comp())[i].getRegisterCandidateOnEntry();
      /////dumpOptDetails(comp(), "i = %d successorRC %x regDepNodes %x autoContainsRegisterValue %d\n", i, successorRC, regDepNodes[i], extgr->getAutoContainsRegisterValue());
      //traceMsg(comp(), "exit node %p succ RC %p reg dep nodes %p\n", exitNode, successorRC, regDepNodes[i]);
      if (successorRC && !regDepNodes[i] &&
          !gr->isUnavailable())   // ensure the global reg is currently available)
         {
            // the check commented out is not enough for longs, for example:
            // low register can be ok but high register can hold another candidate
            //
            // if (extgr->getCurrentRegisterCandidate() != successorRC)
            {
            extgr->setCurrentRegisterCandidate(gr->getRegisterCandidateOnExit(), _visitCount, exitTreeTop->getEnclosingBlock(), i, comp());

            TR_RegisterCandidate *currRC = extgr->getCurrentRegisterCandidate();
            if (currRC && currRC->rcNeeds2Regs(comp()))
               {
               int32_t highRegNum = currRC->getHighGlobalRegisterNumber();
               if (i == highRegNum)
                  {
                  TR_GlobalRegister *grLow = &extRegisters[currRC->getLowGlobalRegisterNumber()];
                  grLow->setCurrentRegisterCandidate(gr->getRegisterCandidateOnExit(), _visitCount, exitTreeTop->getEnclosingBlock(), currRC->getLowGlobalRegisterNumber(), comp());
                  //grLow->copyCurrentRegisterCandidate(gr);
                  }
               else
                  {
                  TR_GlobalRegister *grHigh = &extRegisters[currRC->getHighGlobalRegisterNumber()];
                  grHigh->setCurrentRegisterCandidate(gr->getRegisterCandidateOnExit(), _visitCount, exitTreeTop->getEnclosingBlock(), currRC->getHighGlobalRegisterNumber(), comp());
                  //grHigh->copyCurrentRegisterCandidate(gr);
                  }
               }

            if (extgr->getCurrentRegisterCandidate() != successorRC)
               {
               traceMsg(comp(), "block_%d successorBlock %d exitNode %p\n", block->getNumber(), successorBlock->getNumber(), exitNode);

               if (extgr->getCurrentRegisterCandidate())
                  traceMsg(comp(), "reg %d current candidate %d\n", i, extgr->getCurrentRegisterCandidate()->getSymbolReference()->getReferenceNumber());
               else
                  traceMsg(comp(), "reg %d current candidate null\n", i);

               if (gr->getRegisterCandidateOnExit())
                  traceMsg(comp(), "reg %d exit candidate %d\n", i, gr->getRegisterCandidateOnExit()->getSymbolReference()->getReferenceNumber());
               else
                  traceMsg(comp(), "reg %d current candidate null\n", i);

               if (successorRC)
                  traceMsg(comp(), "reg %d successorRC %d\n", i, successorRC->getSymbolReference()->getReferenceNumber());
               else
                  traceMsg(comp(), "reg %d successorRC null\n", i);
               }

               TR_ASSERT(extgr->getCurrentRegisterCandidate() == successorRC,  "Candidate on exit does not match candidate on successor's entry");
            }

         TR::Node * value = extgr->getValue();
         if (!extgr->getValue() &&
             !exitCandidateAlreadySeen)
            {
            if (!prevTreeTop)
               prevTreeTop = findPrevTreeTop(exitTreeTop, exitNode, block, successorBlock);

            value = extgr->createStoreToRegister(prevTreeTop, NULL, _visitCount, comp(), this);
            bool needs2Regs = false;
            if (extgr->getCurrentRegisterCandidate() &&
                extgr->getCurrentRegisterCandidate()->rcNeeds2Regs(comp()))
               needs2Regs = true;

            if (needs2Regs)
               {
               TR_RegisterCandidate *currRC = extgr->getCurrentRegisterCandidate();
               int32_t highRegNum = currRC->getHighGlobalRegisterNumber();
               if (i == highRegNum)
                  {
                  TR_GlobalRegister *grLow = &extRegisters[currRC->getLowGlobalRegisterNumber()];
                  if (grLow->getCurrentRegisterCandidate() == currRC)
                     grLow->setAutoContainsRegisterValue(true);
                  }
               else
                  {
                  TR_GlobalRegister *grHigh = &extRegisters[currRC->getHighGlobalRegisterNumber()];
                  if (grHigh->getCurrentRegisterCandidate() == currRC)
                     grHigh->setAutoContainsRegisterValue(true);
                  }
               }
            }

         if (!exitCandidateAlreadySeen &&
             !(value->getOpCode().isLoadReg() && extgr->getCurrentRegisterCandidate()->hasSameGlobalRegisterNumberAs(value, comp())))
            {
            TR::Node *actualValue = value;
            value = TR::Node::create(TR::PassThrough, 1, value);

            if (actualValue->requiresRegisterPair(comp()))
               {
               value->setLowGlobalRegisterNumber(extgr->getCurrentRegisterCandidate()->getLowGlobalRegisterNumber());
               value->setHighGlobalRegisterNumber(extgr->getCurrentRegisterCandidate()->getHighGlobalRegisterNumber());

               TR_RegisterCandidate *currRC = extgr->getCurrentRegisterCandidate();
               if (currRC &&
                   currRC->getType().isInt64())
                  {
                  int32_t highRegNum = currRC->getHighGlobalRegisterNumber();
                  if (i == highRegNum)
                     {
                     regDepNodes[extgr->getCurrentRegisterCandidate()->getLowGlobalRegisterNumber()] = value;
                     TR_GlobalRegister * grLow = &extRegisters[extgr->getCurrentRegisterCandidate()->getLowGlobalRegisterNumber()];
                     grLow->setLastRefTreeTop(exitTreeTop);
                     grLow->setValue(actualValue);
                     }
                  else
                     {
                     regDepNodes[extgr->getCurrentRegisterCandidate()->getHighGlobalRegisterNumber()] = value;
                     TR_GlobalRegister * grHigh = &extRegisters[extgr->getCurrentRegisterCandidate()->getHighGlobalRegisterNumber()];
                     grHigh->setLastRefTreeTop(exitTreeTop);
                     grHigh->setValue(actualValue);
                     }
                  }
               }
            else
               value->setGlobalRegisterNumber(i);
            }

         regDepNodes[i] = value;
         /////dumpOptDetails(comp(), "exitNode %x i = %d value %x\n", exitNode, i, value);
         extgr->setLastRefTreeTop(exitTreeTop);
         }
      }

   if ((successorBlock != originalSuccessorBlock) &&
       (successorBlock->getPredecessors().size() == 1) &&
       (successorBlock->getExit()->getPrevTreeTop()->getNode()->getOpCodeValue() != TR::Goto))
      {
      // isSingleton test above is to ensure we only reach here once;
      // a single new successorBlock could be shared by more than 1 pred
      // if they branch to the same originalSuccessorBlock and we
      // do not want this code to be run more than once for any new
      // successor block
      //
      TR::Block * sb = successorBlock;
      sb->getEntry()->getNode()->setVisitCount(_visitCount);
      addRegLoadsToEntry(sb->getEntry(), sb->getGlobalRegisters(comp()),sb);

      TR::TreeTop *succExit = sb->getExit();
      if (succExit->getPrevTreeTop()->getNode()->getOpCodeValue() == TR::Goto)
         succExit = succExit->getPrevTreeTop();

      transformBlockExit(succExit, succExit->getNode(), sb, sb->getGlobalRegisters(comp()), originalSuccessorBlock);
      }
   }

bool
TR_GlobalRegisterAllocator::registerIsLiveAcrossEdge(
   TR::TreeTop * exitTreeTop, TR::Node * exitNode, TR::Block * block,
   TR_GlobalRegister * extgr, TR::Block * & successorBlock, int32_t i)
   {
   TR_Array<TR_GlobalRegister> & registers = block->getGlobalRegisters(comp());
   TR_GlobalRegister * gr = &registers[i];

   TR_RegisterCandidate * rc = extgr->getCurrentRegisterCandidate();
   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   TR::Block *extBlock = _candidates->_startOfExtendedBBForBB[block->getNumber()];

   TR_GlobalRegister * successorRegister = &successorBlock->getGlobalRegisters(comp())[i];
   if (rc == successorRegister->getRegisterCandidateOnEntry())
      return true;


   if (successorBlock && !successorBlock->getEntry())
      return false;

   // It may happen that all predecessors have this candidate in register on exit.
   // If there are more than one predecessor and their frequency is higher than that of this block,
   // it's better to extend live range into successor
   TR::CFGEdgeList &predecessors = successorBlock->getPredecessors();
   if (!predecessors.empty() && !(predecessors.size() == 1) &&
       successorBlock->getEntry()->getNode()->getVisitCount() != comp()->getVisitCount())
      {
      bool liveOnAllPredExits = true;
      int32_t allPredFreq = 0;
      for (auto pred = predecessors.begin(); pred != predecessors.end(); ++pred)
         {
         if (!rc->getBlocksLiveOnExit().get((*pred)->getFrom()->getNumber()) ||
            toBlock((*pred)->getFrom())->getEntry()->getNode()->getVisitCount() == comp()->getVisitCount())
            {
            liveOnAllPredExits = false;
            break;
            }
         else
            {
            allPredFreq += toBlock((*pred)->getFrom())->getFrequency();
            }
         }
      if (liveOnAllPredExits && allPredFreq >= successorBlock->getFrequency())
         {
         if (trace())
            traceMsg(comp(), "Extended live range of #%d into successor since candidate is available in register on all predecessor's exits: block=%d succ=%d allPredFreq=%d succFreq=%d\n",
                            rc->getSymbolReference()->getReferenceNumber(),
                            block->getNumber(), successorBlock->getNumber(),
                            allPredFreq, successorBlock->getFrequency());

         TR_ASSERT(!successorRegister->getRegisterCandidateOnEntry(), "Should not have candidate on entry");
         successorRegister->setRegisterCandidateOnEntry(rc);
         return true;
         }
      }


   // If the register isn't marked as being live across the branch then without some effort we're
   // going to slow down the fall-thru path by inserting a store from the register back into the auto
   // before the branch and/or reloading the float value (on ia32) at the next use in the fall thru.
   //
   // If the target of the branch has only one predecessor then we can prepend a block to the branch
   // target, add the store there and change the branch target to be the new block.
   //
   // If the target of the branch has multiple predecessors and the fallthru path in known to be
   // hotter than the branch target then we can append a new block, change the branch to target
   // the new block, end the live range in the new block, and add a goto to the original block.
   //

   int32_t refNum = extgr->getCurrentRegisterCandidate()->getSymbolReference()->getReferenceNumber();

   if (!exitNode->getOpCode().isBranch() || !cg()->allowGlobalRegisterAcrossBranch(rc, exitNode))
      return false;

   bool needs2Regs = rc->rcNeeds2Regs(comp());

   // todo: see if it is really referenced on the fall thru
   //
   bool isReferencedInFallThru = (rc == gr->getRegisterCandidateOnExit());
   if (!isReferencedInFallThru)
      return false;

   if (needs2Regs)
      {
      int32_t highRegNum = rc->getHighGlobalRegisterNumber();
      if (i == highRegNum)
         {
         int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
         if (extBlock->getGlobalRegisters(comp())[lowRegNum].getCurrentRegisterCandidate() != rc)
            return false;
         }
      else
         {
         if (extBlock->getGlobalRegisters(comp())[highRegNum].getCurrentRegisterCandidate() != rc)
            return false;
         }
      }

   int32_t numRegistersLiveOnSuccessor = numberOfRegistersLiveOnEntry(successorBlock->getGlobalRegisters(comp()), true);
   if (numRegistersLiveOnSuccessor + (needs2Regs ? 2 : 1) > cg()->getMaximumNumberOfGPRsAllowedAcrossEdge(block))
      return false;

   if (!rc->symbolIsLive(successorBlock)
       && !rc->symbolIsLive(block))
      return false;

   TR::TreeTop * ttBeforeSuccessor = successorBlock->getEntry()->getPrevTreeTop();
   bool keepingItAliveAcrossEdgeRequiresExtraGoto =
      !ttBeforeSuccessor || ttBeforeSuccessor->getNode()->getBlock()->hasSuccessor(successorBlock);

   TR_YesNoMaybe branchIsHotter = TR_no;
      // Try to decide based on cold flag
      //
      branchIsHotter = TR_maybe;
      if (successorBlock->isCold() && block->getNextBlock() && !block->getNextBlock()->isCold())
         branchIsHotter = TR_no;
      else if (!successorBlock->isCold() && block->getNextBlock() && block->getNextBlock()->isCold())
         branchIsHotter = TR_yes;

   //Try to decide based on program structure
   //
   if (branchIsHotter == TR_maybe)
      {
      TR_BlockStructure *nextStructure = NULL;
      if (block->getNextBlock())
         nextStructure = block->getNextBlock()->getStructureOf();

      int32_t nextWeight = 1;
      if (nextStructure)
         {
         TR::Optimizer * optimizer = (TR::Optimizer *)comp()->getOptimizer();
         optimizer->getStaticFrequency(block->getNextBlock(), &nextWeight);
         }

      TR_BlockStructure *blockStructure = block->getStructureOf();
      int32_t blockWeight = 1;
      if (blockStructure)
         {
         TR::Optimizer * optimizer = (TR::Optimizer *)comp()->getOptimizer();
         optimizer->getStaticFrequency(block, &blockWeight);
         }

      TR_BlockStructure *succStructure = successorBlock->getStructureOf();
      int32_t succWeight = 1;
      if (succStructure)
         {
         TR::Optimizer * optimizer = (TR::Optimizer *)comp()->getOptimizer();
         optimizer->getStaticFrequency(successorBlock, &succWeight);
         }

      if (nextWeight > succWeight)
         branchIsHotter = TR_no;
      else if (nextWeight < succWeight)
         branchIsHotter = TR_yes;
      }

   // Try to decide based on profiling frequency
   //
   if (branchIsHotter == TR_maybe)
      {
      if (block->getNextBlock())
         {
         if (block->getNextBlock()->getFrequency() > successorBlock->getFrequency())
            {
            if (((successorBlock->getFrequency() == 0) ||
                 ((successorBlock->getFrequency() > 0) &&
                  ((block->getNextBlock()->getFrequency()*100/(successorBlock->getFrequency())) > 130))))
               branchIsHotter = TR_no;
            }
         else if (block->getNextBlock()->getFrequency() < successorBlock->getFrequency())
            {
            if (((block->getNextBlock()->getFrequency() == 0) ||
                 ((block->getNextBlock()->getFrequency() > 0) &&
                  ((successorBlock->getFrequency()*100/(block->getNextBlock()->getFrequency())) > 130))))
               branchIsHotter = TR_yes;
            }
         }
      }

   if (!successorBlock->getExceptionPredecessors().empty())
      return false;

   if (!keepingItAliveAcrossEdgeRequiresExtraGoto)
      {
      // If an extra branch isn't required to keep the register alive across the edge then
      // it should make sense to keep it alive across the edge unless doing so introduces an
      // extra pop on the branch successor and the branch is the hotter path.
      //
      if (branchIsHotter == TR_yes && extgr->getAutoContainsRegisterValue())
         return false;


      if (successorRegister->getRegisterCandidateOnEntry() != 0 ||
          successorBlock->getEntry()->getNode()->getVisitCount() == _visitCount ||
          !(successorBlock->getPredecessors().size() == 1) ||
          (successorRegister->getRegisterCandidateOnExit() &&
           (successorRegister->getRegisterCandidateOnExit() != rc)))
         {
         TR::Block * newBlock = createNewSuccessorBlock(block, successorBlock, exitTreeTop, exitNode, rc);
         if (trace())
            traceMsg(comp(), "Creating new successor block_%d\n", newBlock->getNumber());
         if (!newBlock->getEntry()->getPrevTreeTop())
            {
            ttBeforeSuccessor->join(newBlock->getEntry());
            newBlock->getExit()->join(successorBlock->getEntry());
            }
         successorBlock = newBlock;
         }


      if (trace())
         traceMsg(comp(), "Setting candidate %d (real reg %d) on entry to succ block_%d\n", rc->getSymbolReference()->getReferenceNumber(), i, successorBlock->getNumber());
      successorBlock->getGlobalRegisters(comp())[i].setRegisterCandidateOnEntry(rc);
      rc->setExtendedLiveRange(true);
      /////dumpOptDetails(comp(), "i = %d successorRC %x\n", i, successorBlock->getGlobalRegisters(comp())[i].getRegisterCandidateOnEntry());
      if (needs2Regs)
         {
         int32_t highRegNum = rc->getHighGlobalRegisterNumber();
         if (i == highRegNum)
            {
            int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
            successorBlock->getGlobalRegisters(comp())[lowRegNum].setRegisterCandidateOnEntry(rc);
            rc->setExtendedLiveRange(true);
            if (trace())
               traceMsg(comp(), "Setting candidate %d (real reg %d) on entry to succ block_%d\n", rc->getSymbolReference()->getReferenceNumber(), lowRegNum, successorBlock->getNumber());
            /////dumpOptDetails(comp(), "lowRegNum = %d successorRC %x\n", lowRegNum, successorBlock->getGlobalRegisters(comp())[lowRegNum].getRegisterCandidateOnEntry());
            }
         else
            {
            successorBlock->getGlobalRegisters(comp())[highRegNum].setRegisterCandidateOnEntry(rc);
            rc->setExtendedLiveRange(true);
            //////dumpOptDetails(comp(), "highRegNum = %d successorRC %x\n", highRegNum, successorBlock->getGlobalRegisters(comp())[highRegNum].getRegisterCandidateOnEntry());
            if (trace())
               traceMsg(comp(), "Setting candidate %d (real reg %d) on entry to succ block_%d\n", rc->getSymbolReference()->getReferenceNumber(), highRegNum, successorBlock->getNumber());
            }
         }

      return true;
      }

   // If keeping the register live across the branch requires an extra goto on the
   // branch path then only do this if the fallthru path is hotter.
   //
   if (branchIsHotter == TR_no)
      {

      TR::Block * newBlock = createNewSuccessorBlock(block, successorBlock, exitTreeTop, exitNode, rc);

      if (trace())
         traceMsg(comp(), "Creating new block_%d\n", newBlock->getNumber());

      if (!newBlock->getEntry()->getPrevTreeTop())
         {
         newBlock->append(TR::TreeTop::create(comp(),
                                             TR::Node::create(exitNode, TR::Goto, 0, successorBlock->getEntry())));

         // FIXME: appending all the goto blocks clumped up together, (near) the end of the method
         // is far from ideal.  We should try to find the first break in the fall through chain from
         // 'block' onwards, and place it there instead so that the ichace is not contaminated
         //
         appendGotoBlock(newBlock, block);
         }

      newBlock->getGlobalRegisters(comp())[i].setRegisterCandidateOnEntry(rc);
      rc->setExtendedLiveRange(true);

      if (trace())
         traceMsg(comp(), "Setting candidate %d (real reg %d) on entry to new block_%d\n", rc->getSymbolReference()->getReferenceNumber(), i, newBlock->getNumber());

      if (needs2Regs)
         {
         int32_t highRegNum = rc->getHighGlobalRegisterNumber();
         if (i == highRegNum)
            {
            int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
            newBlock->getGlobalRegisters(comp())[lowRegNum].setRegisterCandidateOnEntry(rc);
            rc->setExtendedLiveRange(true);
            if (trace())
               traceMsg(comp(), "Setting candidate %d (real reg %d) on entry to new block_%d\n", rc->getSymbolReference()->getReferenceNumber(), lowRegNum, newBlock->getNumber());
            }
         else
            {
            newBlock->getGlobalRegisters(comp())[highRegNum].setRegisterCandidateOnEntry(rc);
            rc->setExtendedLiveRange(true);
            if (trace())
               traceMsg(comp(), "Setting candidate %d (real reg %d) on entry to new block_%d\n", rc->getSymbolReference()->getReferenceNumber(), highRegNum, newBlock->getNumber());
            }
         }

      successorBlock = newBlock;
      return true;
      }

   return false;
   }

TR::Block *
TR_GlobalRegisterAllocator::extendBlock(TR::Block * block, TR::Block * successorBlock)
   {
   TR::Block * newBlock = createBlock(block, successorBlock);
   newBlock->getEntry()->getNode()->setVisitCount(_visitCount);
   newBlock->setIsExtensionOfPreviousBlock();
   _candidates->_startOfExtendedBBForBB[newBlock->getNumber()] = _candidates->_startOfExtendedBBForBB[block->getNumber()];
   block->getExit()->join(newBlock->getEntry());
   newBlock->getExit()->join(successorBlock->getEntry());
   comp()->getOptimizer()->setCachedExtendedBBInfoValid(true);
   return newBlock;
   }

TR::Block *
TR_GlobalRegisterAllocator::createNewSuccessorBlock(
   TR::Block * block, TR::Block * successorBlock, TR::TreeTop * exitTreeTop, TR::Node * exitNode, TR_RegisterCandidate * rc)
   {
   TR_Array<TR_GlobalRegister> & successorRegisters = successorBlock->getGlobalRegisters(comp());
   TR_Array<TR_GlobalRegister> & blockRegisters = block->getGlobalRegisters(comp());
   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   TR_Array<TR_GlobalRegister> & blockRegistersEBB = _candidates->_startOfExtendedBBForBB[block->getNumber()]->getGlobalRegisters(comp());

   TR::Block *newBlock = NULL;

   if (!_newBlocks.isEmpty())
      {
      ListIterator<TR::Block> newBlocksIt(&_newBlocks);
      TR::Block *nextNewBlock = newBlocksIt.getFirst();
      for (nextNewBlock = newBlocksIt.getFirst(); nextNewBlock != NULL; nextNewBlock = newBlocksIt.getNext())
         {
         bool newBlockCanBeReused = true;
         TR::Block *predBlock = nextNewBlock->getPredecessors().front()->getFrom()->asBlock();
         if (!predBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch() ||
             !block->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
            newBlockCanBeReused = false;

         if (newBlockCanBeReused)
            {
            TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
            TR_Array<TR_GlobalRegister> & predRegistersEBB = _candidates->_startOfExtendedBBForBB[predBlock->getNumber()]->getGlobalRegisters(comp());
            TR_Array<TR_GlobalRegister> & predRegisters = predBlock->getGlobalRegisters(comp());

            //printf("Considering next block_%d for reuse\n", nextNewBlock->getNumber());
            if ((nextNewBlock->getSuccessors().size() == 1) &&
                nextNewBlock->getSuccessors().front()->getTo() == successorBlock)
               {
               TR_Array<TR_GlobalRegister> & nextNewRegisters = nextNewBlock->getGlobalRegisters(comp());
               int32_t j;
               for (j = _firstGlobalRegisterNumber; j <= _lastGlobalRegisterNumber; ++j)
                  {
                  if (successorRegisters[j].getRegisterCandidateOnEntry() != nextNewRegisters[j].getRegisterCandidateOnExit())
                     {
                     newBlockCanBeReused = false;
                     break;
                     }
                  if ((blockRegisters[j].getRegisterCandidateOnExit() != predRegisters[j].getRegisterCandidateOnExit()) ||
                      (blockRegistersEBB[j].getCurrentRegisterCandidate() != predRegisters[j].getRegisterCandidateOnExit()) ||
                      (blockRegisters[j].getRegisterCandidateOnExit() != nextNewRegisters[j].getRegisterCandidateOnEntry()) ||
                      (blockRegistersEBB[j].getCurrentRegisterCandidate() != nextNewRegisters[j].getRegisterCandidateOnEntry()))
                     {
                     newBlockCanBeReused = false;
                     break;
                    }
                  }
               int32_t numRegistersLiveOnNewSuccessor = numberOfRegistersLiveOnEntry(nextNewBlock->getGlobalRegisters(comp()), true);
               bool needs2Regs = rc->rcNeeds2Regs(comp());
               if (numRegistersLiveOnNewSuccessor + (needs2Regs? 2 : 1) > comp()->cg()->getMaximumNumberOfGPRsAllowedAcrossEdge(block))
                  {
                  if (trace())
                        traceMsg(comp(), "numRegistersLiveOnNewSuccessor %d on nextNewBlock %d > comp()->cg()->getMaximumNumberOfGPRsAllowedAcrossEdge(block_%d) %d\n",numRegistersLiveOnNewSuccessor, nextNewBlock->getNumber(), block->getNumber(),comp()->cg()->getMaximumNumberOfGPRsAllowedAcrossEdge(block));
                  newBlockCanBeReused = false;
                  }
               }
            else
               newBlockCanBeReused = false;
            }

         if (newBlockCanBeReused)
            {
            newBlock = nextNewBlock;
            break;
            }
         }
      }


   if (!newBlock)
      {
      newBlock = createBlock(block, successorBlock);
      _newBlocks.add(newBlock);
      TR_Array<TR_GlobalRegister> & newRegisters = newBlock->getGlobalRegisters(comp());

      int32_t j;
      for (j = _firstGlobalRegisterNumber; j <= _lastGlobalRegisterNumber; ++j)
         {
         if (successorRegisters[j].getRegisterCandidateOnEntry())
            {
            //traceMsg(comp(), "cand on exit %p\n", blockRegisters[j].getRegisterCandidateOnExit());
            //traceMsg(comp(), "succ on enter %p\n", successorRegisters[j].getRegisterCandidateOnEntry());
            //traceMsg(comp(), "cand on ebb enter %p\n", blockRegistersEBB[j].getRegisterCandidateOnEntry());

            TR_ASSERT(!blockRegisters[j].getRegisterCandidateOnExit() ||
               (blockRegisters[j].getRegisterCandidateOnExit() == successorRegisters[j].getRegisterCandidateOnEntry()) ||
               (blockRegisters[j].getRegisterCandidateOnEntry() == successorRegisters[j].getRegisterCandidateOnEntry()), "Candidates are different in a global register across a CFG edge\n");

            successorRegisters[j].getRegisterCandidateOnEntry()->setExtendedLiveRange(true);
            newRegisters[j].setRegisterCandidateOnEntry(successorRegisters[j].getRegisterCandidateOnEntry());
            newRegisters[j].setRegisterCandidateOnExit(successorRegisters[j].getRegisterCandidateOnEntry());
            }
         }
      }
   else
      {
      TR::CFG * cfg = comp()->getFlowGraph();
      cfg->addEdge(block, newBlock);
      cfg->removeEdge(block, successorBlock);
      }
   TR::Node *exitTTNode = exitTreeTop->getNode();

   if(exitTTNode->getOpCodeValue() == TR::treetop)
       exitTTNode = exitTTNode->getFirstChild();

   if (exitTreeTop->getNode()->getOpCode().isSwitch())
      {
      int32_t i;
      for (i = exitTreeTop->getNode()->getCaseIndexUpperBound() - 1; i > 0; --i)
         {
         TR::Node * child = exitTreeTop->getNode()->getChild(i);
         if (child->getBranchDestination()->getNode()->getBlock() == successorBlock)
            child->setBranchDestination(newBlock->getEntry());
         }
      }
   else if (exitTreeTop->getNode()->getOpCode().isJumpWithMultipleTargets() && !exitTreeTop->getNode()->getOpCode().isSwitch())
      {
      int32_t i=0;
      for (i = 0 ; i < exitTreeTop->getNode()->getNumChildren()-1 ; i++)
         {
         TR::Node * child = exitTreeTop->getNode()->getChild(i);
         if (child->getBranchDestination()->getNode()->getBlock() == successorBlock)
            child->setBranchDestination(newBlock->getEntry());
         }
      }
   else if (!exitTTNode->getOpCode().isJumpWithMultipleTargets())
      exitNode->setBranchDestination(newBlock->getEntry());

   return newBlock;
   }

TR::Block *
TR_GlobalRegisterAllocator::createBlock(TR::Block * block, TR::Block * successorBlock)
   {
   //dumpOptDetails(comp(), "successorBlock %d freq %d\n", successorBlock->getNumber(), successorBlock->getFrequency());
   TR::Block * newBlock = TR::Block::createEmptyBlock(block->getExit()->getNode(), comp(), block->getFrequency(), block);
   newBlock->getExit()->getNode()->setVisitCount(_visitCount);

   if (block->isCold() || successorBlock->isCold())
      {
      newBlock->setIsCold();
      if (block->isSuperCold() || successorBlock->isSuperCold())
         newBlock->setIsSuperCold();
      newBlock->setFrequency(TR::Block::getMinColdFrequency(block, successorBlock));
      }

   TR::CFG * cfg = comp()->getFlowGraph();
   cfg->addNode(newBlock, block->getCommonParentStructureIfExists(successorBlock, cfg));

   cfg->addEdge(block, newBlock);
   cfg->addEdge(newBlock, successorBlock);

   TR_SuccessorIterator ei(block);
   for (TR::CFGEdge * edge = ei.getFirst(); edge != NULL; edge = ei.getNext())
     if ((edge->getTo() == successorBlock) && (edge->getFrequency() >= 0))
            newBlock->setFrequency(edge->getFrequency());

   cfg->removeEdge(block, successorBlock);

   _candidates->_startOfExtendedBBForBB[newBlock->getNumber()] = newBlock;

   if (_candidates->_startOfExtendedBBForBB[successorBlock->getNumber()] == block &&
       block != successorBlock)
      _candidates->_startOfExtendedBBForBB[successorBlock->getNumber()] = newBlock;

   return newBlock;
   }

int32_t
TR_GlobalRegisterAllocator::numberOfRegistersLiveOnEntry(TR_Array<TR_GlobalRegister> & registers, bool countMachineRegs)
   {
   int32_t numLoads = 0;
   TR_ScratchList<TR_RegisterCandidate> seenCandidates(trMemory());
   int32_t i;
   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR_RegisterCandidate * rc = registers[i].getRegisterCandidateOnEntry();
      if (rc && !seenCandidates.find(rc) && !registers[i].isUnavailable())
         {
         seenCandidates.add(rc);
         ++numLoads;
         if (countMachineRegs && rc->rcNeeds2Regs(comp()))
            ++numLoads;
         }
      }

   return numLoads;
   }

TR_GlobalRegister *
TR_GlobalRegisterAllocator::getGlobalRegisterWithoutChangingCurrentCandidate(TR::Symbol * symbol, TR_Array<TR_GlobalRegister> & registers, TR::Block * block)
   {
   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   TR::Block *startOfExtendedBlock = _candidates->_startOfExtendedBBForBB[block->getNumber()];
   TR_Array<TR_GlobalRegister> & extRegisters = startOfExtendedBlock->getGlobalRegisters(comp());
   int32_t i;
   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR_GlobalRegister * gr = &registers[i];
      TR_GlobalRegister * extgr = &extRegisters[i];

      TR_RegisterCandidate * firstRc = gr->getRegisterCandidateOnEntry();
      TR_RegisterCandidate * secondRc = gr->getRegisterCandidateOnExit();
      TR_RegisterCandidate *rc = firstRc;

      if (!firstRc || firstRc->getSymbolReference()->getSymbol() != symbol)
         {
         rc = secondRc;
         if (!secondRc || secondRc->getSymbolReference()->getSymbol() != symbol)
            {
            TR_RegisterCandidate * thirdRc = extgr->getCurrentRegisterCandidate();
            rc = thirdRc;

            if (!thirdRc || thirdRc->getSymbolReference()->getSymbol() != symbol)
               continue;
            else
              TR_ASSERT(!symbol->isInGlobalRegister(), "Current candidate is not live on entry and not live on exit from basic block\n");

            }
         }
      return extgr;
      }

   return NULL;
   }


TR_GlobalRegister *
TR_GlobalRegisterAllocator::getGlobalRegister(TR::Symbol * symbol, TR_Array<TR_GlobalRegister> & registers, TR::Block * block)
   {
   TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
   TR::Block *startOfExtendedBlock = _candidates->_startOfExtendedBBForBB[block->getNumber()];
   TR_Array<TR_GlobalRegister> & extRegisters = startOfExtendedBlock->getGlobalRegisters(comp());
   int32_t i;
   for (i = _firstGlobalRegisterNumber; i <= _lastGlobalRegisterNumber; ++i)
      {
      TR_GlobalRegister *gr = &registers[i];
      TR_GlobalRegister *extgr = &extRegisters[i];

      TR_RegisterCandidate * firstRc = gr->getRegisterCandidateOnEntry();
      TR_RegisterCandidate * secondRc = gr->getRegisterCandidateOnExit();
      TR_RegisterCandidate *rc = firstRc;

      if (!firstRc || firstRc->getSymbolReference()->getSymbol() != symbol)
         {
         rc = secondRc;
         if (!secondRc || secondRc->getSymbolReference()->getSymbol() != symbol)
            {
            TR_RegisterCandidate * thirdRc = extgr->getCurrentRegisterCandidate();
            rc = thirdRc;

            if (!thirdRc || thirdRc->getSymbolReference()->getSymbol() != symbol)
               continue;
            else
              TR_ASSERT(false, "Current cand is not live on entry and not live on exit from basic block\n");
            }
         }
      extgr->setCurrentRegisterCandidate(rc, _visitCount, block, i, comp());

      if (rc->rcNeeds2Regs(comp()))
         {
         int32_t highRegNum = rc->getHighGlobalRegisterNumber();
         if (i == highRegNum)
            {
            int32_t lowRegNum = rc->getLowGlobalRegisterNumber();
            TR_GlobalRegister *grLow = &extRegisters[lowRegNum];
            grLow->setCurrentRegisterCandidate(rc, _visitCount, block, lowRegNum, comp());
            //grLow.copyCurrentRegisterCandidate(gr);
            }
         else
            {
            TR_GlobalRegister *grHigh = &extRegisters[highRegNum];
            grHigh->setCurrentRegisterCandidate(rc, _visitCount, block, highRegNum, comp());
            //grHigh.copyCurrentRegisterCandidate(gr);
            }
         }

      return extgr;
      }

   TR_ASSERT(0, "couldn't find the global register");
   return NULL;
   }



//////////////////////////////////////////
// TR_GlobalRegister
//////////////////////////////////////////

bool
TR_GlobalRegister::getAutoContainsRegisterValue()
   {
   return _autoContainsRegisterValue;
   // &&
   //      (!getValue()->getOpCode().isFloatingPoint() ||
   //       comp()->cg()->getSupportsJavaFloatSemantics()));
   }


void
TR_GlobalRegister::setCurrentRegisterCandidate(
   TR_RegisterCandidate * rc, vcount_t visitCount, TR::Block * currentBlock, int32_t i, TR::Compilation *comp, bool resetOtherHalfOfLong)
   {
   if (_rcCurrent != rc)
      {
      bool liveOnSomeSucc = true;
      if (_rcCurrent && getValue() && !getAutoContainsRegisterValue())
         {
         if (liveOnSomeSucc)
            createStoreFromRegister(visitCount, optimalPlacementForStore(currentBlock, comp), i, comp);

         }
      if (_rcCurrent)
         _rcCurrent->getSymbolReference()->getSymbol()->setIsInGlobalRegister(false);



      if (resetOtherHalfOfLong && _rcCurrent != NULL && _rcCurrent->rcNeeds2Regs(comp))
         {
       //  TR_ASSERT(currentBlock->startOfExtendedBlock() == _candidates->_startOfExtendedBBForBB[currentBlock->getNumber()], "Incorrect value in _startOfExtendedBBForBB");
         TR::Block *startOfExtendedBlock = currentBlock->startOfExtendedBlock();
         TR_Array<TR_GlobalRegister> & extRegisters = startOfExtendedBlock->getGlobalRegisters(comp);

         int32_t highRegNum = _rcCurrent->getHighGlobalRegisterNumber();
         if (i == highRegNum)
            {
            int32_t lowRegNum = _rcCurrent->getLowGlobalRegisterNumber();
            TR_GlobalRegister *grLow = &extRegisters[lowRegNum];
            grLow->setCurrentRegisterCandidate(NULL, visitCount, currentBlock, lowRegNum, comp, false);
            //grLow.copyCurrentRegisterCandidate(gr);
            }
         else
            {
            TR_GlobalRegister *grHigh = &extRegisters[highRegNum];
            grHigh->setCurrentRegisterCandidate(NULL, visitCount, currentBlock, highRegNum, comp, false);
            //grHigh.copyCurrentRegisterCandidate(gr);
            }
         }

      _rcCurrent = rc;
      setValue(0); //regardless of whether or not we created a store we must setValue to NULL, else we can use the wrong value

      if (currentBlock) comp->setCurrentBlock(currentBlock);
      if (rc && (rc->getSymbol()->dontEliminateStores(comp)
                 || keepAllStores
                 || rc->isLiveAcrossExceptionEdge()))
         setAutoContainsRegisterValue(true);
      else
         setAutoContainsRegisterValue(false);
      }
   }


void
TR_GlobalRegister::copyCurrentRegisterCandidate(TR_GlobalRegister *gr)
   {
   _rcCurrent = gr->getCurrentRegisterCandidate();
   //_rcOnExit = gr->getRegisterCandidateOnExit();
   //_rcOnEntry = gr->getRegisterCandidateOnEntry();
   setValue(gr->getValue());
   //setLastRefTreeTop(gr->getLastRefTreeTop());
   //_mappings = gr->getMappings();
   setAutoContainsRegisterValue(gr->getAutoContainsRegisterValue());
   }



TR::TreeTop *
TR_GlobalRegister::optimalPlacementForStore(TR::Block * currentBlock, TR::Compilation *comp)
   {
   bool traceGRA = comp->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);
   if (traceGRA)
      traceMsg(comp, "           optimalPlacementForStore([%p], block_%d)\n", getValue(), currentBlock->getNumber());

   TR::TreeTop * lastRefTreeTop = getLastRefTreeTop();

   TR::Block * lastRefBlock = lastRefTreeTop->getEnclosingBlock();

   // if lastRefTreeTop is the last tree in the previous block,
   // the store should be in the current block
   if (lastRefBlock != currentBlock)
      {
      TR::Node * lastRefNode = lastRefTreeTop->getNode();
      if(lastRefNode->getOpCodeValue() == TR::treetop)
          lastRefNode = lastRefNode->getFirstChild();

      TR::ILOpCode &prevOpCode = lastRefTreeTop->getNode()->getOpCode();
      if (prevOpCode.isBranch() ||
          prevOpCode.isJumpWithMultipleTargets() ||
          prevOpCode.isReturn() ||
          prevOpCode.getOpCodeValue() == TR::athrow ||
          prevOpCode.getOpCodeValue() == TR::BBEnd)
         {
         lastRefTreeTop = lastRefTreeTop->getNextTreeTop();
         }

         if (lastRefTreeTop->getNode()->getOpCodeValue() == TR::BBEnd)
            lastRefTreeTop = lastRefTreeTop->getNextTreeTop();
      }

   if (lastRefBlock == currentBlock)
      {
      if (traceGRA)
         traceMsg(comp, "           - lastRefBlock == currentBlock: returning [%p]\n", lastRefTreeTop->getNode());
      return lastRefTreeTop;
      }

   int32_t lastRefFreq = 1, currentFreq = 1;
   if (!lastRefBlock->getStructureOf() || !currentBlock->getStructureOf())
      {
      if (traceGRA)
         traceMsg(comp, "           - Structure info missing: returning [%p]\n", lastRefTreeTop->getNode());
      return lastRefTreeTop;
      }

   TR::Optimizer * optimizer = (TR::Optimizer *)comp->getOptimizer();

   optimizer->getStaticFrequency(lastRefBlock, &lastRefFreq);
   optimizer->getStaticFrequency(currentBlock, &currentFreq);

   if (lastRefFreq <= currentFreq)  // used to be ==
      {
      if (traceGRA)
         traceMsg(comp, "           - Frequency is low enough: returning [%p]\n", lastRefTreeTop->getNode());
      return lastRefTreeTop;
      }

   for (TR::Block * b = lastRefBlock->getNextBlock(); b; b = b->getNextBlock())
      {
      if (b != currentBlock)
         {
         int32_t freq = 1;
         optimizer->getStaticFrequency(b, &freq);
         if (freq > currentFreq)
            continue;
         }
      if (traceGRA)
         traceMsg(comp, "           - Found a suitable block: returning [%p]\n", b->getEntry()->getNode());
      return b->getEntry();
      }

   TR_ASSERT(0, "optimalPlacementForStore didn't find optimal placement");
   return 0;
   }

TR::Node *
TR_GlobalRegister::createLoadFromRegister(TR::Node * n, TR::Compilation *comp)
   {
   TR_RegisterCandidate * rc = getCurrentRegisterCandidate();
   TR::DataType dt = rc->getDataType();
   if (dt == TR::Aggregate)
      {
      switch (rc->getSymbol()->getSize())
         {
            case 1: dt = TR::Int8; break;
            case 2: dt = TR::Int16; break;
            case 4: dt = TR::Int32; break;
            case 8: dt = TR::Int64; break;
         }
      }

   TR::Node *load;
   load = TR::Node::create(n, comp->il.opCodeForRegisterLoad(dt));
   load->setRegLoadStoreSymbolReference(rc->getSymbolReference());

   if (load->requiresRegisterPair(comp))
      {
      load->setLowGlobalRegisterNumber(rc->getLowGlobalRegisterNumber());
      load->setHighGlobalRegisterNumber(rc->getHighGlobalRegisterNumber());
      }
   else
      load->setGlobalRegisterNumber(rc->getGlobalRegisterNumber());
   if (!rc->is8BitGlobalGPR())
      load->setIsInvalid8BitGlobalRegister(true);
   setValue(load);
   if (load->requiresRegisterPair(comp))
      dumpOptDetails(comp, "%s create load [%p] from Register %d (low word) and Register %d (high word)\n", OPT_DETAILS, load, rc->getLowGlobalRegisterNumber(), rc->getHighGlobalRegisterNumber());
   else
      dumpOptDetails(comp, "%s create load [%p] %s from Register %d\n", OPT_DETAILS, load, rc->getSymbolReference()->getSymbol()->isMethodMetaData() ? rc->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName():"", rc->getGlobalRegisterNumber());
   return load;
   }

TR::Node *
TR_GlobalRegister::createStoreToRegister(TR::TreeTop * prevTreeTop, TR::Node *node, vcount_t visitCount, TR::Compilation *comp, TR_GlobalRegisterAllocator *gra)
   {
   TR::Node * n = prevTreeTop->getNode();
   TR_RegisterCandidate * rc = getCurrentRegisterCandidate();
   TR::Node * load = NULL;

   TR::DataType dt = rc->getDataType();
   if (dt == TR::Aggregate)
      {
      switch (rc->getSymbol()->getSize())
         {
         case 1: dt = TR::Int8; break;
         case 2: dt = TR::Int16; break;
         case 4: dt = TR::Int32; break;
         case 8: dt = TR::Int64; break;
         default:
            TR_ASSERT(false,"unsupported size %d for aggregate in GRA\n",rc->getSymbol()->getSize());
            break;
         }
      }

   if (node)
      {
      load = node;
      }
   else
      {
      load = TR::Node::createWithSymRef(n, comp->il.opCodeForDirectLoad(dt), 0, rc->getSymbolReference());
      }

   load = gra->resolveTypeMismatch(dt, load);

   TR::Node *store;
   store = TR::Node::create(comp->il.opCodeForRegisterStore(dt), 1, load);
   store->setRegLoadStoreSymbolReference(rc->getSymbolReference());

   bool enableSignExtGRA = false; // enable for other platforms later
   static char *doit = feGetEnv("TR_SIGNEXTGRA");
   if (NULL != doit)
      enableSignExtGRA = true;

   if (TR::Compiler->target.cpu.isZ())
      {
      enableSignExtGRA = true;
      static char *doit2 = feGetEnv("TR_NSIGNEXTGRA");
      if (NULL != doit2)
         enableSignExtGRA = false;
      }

   if (TR::Compiler->target.is64Bit() &&
       (store->getOpCodeValue() == TR::iRegStore) &&
       gra->candidateCouldNeedSignExtension(rc->getSymbolReference()->getReferenceNumber()) &&
       enableSignExtGRA)
      {
      store->setNeedsSignExtension(true);
      }

   if (store->requiresRegisterPair(comp))
      {
      store->setLowGlobalRegisterNumber(rc->getLowGlobalRegisterNumber());
      store->setHighGlobalRegisterNumber(rc->getHighGlobalRegisterNumber());
      }
   else
      store->setGlobalRegisterNumber(rc->getGlobalRegisterNumber());

   if (store->needsSignExtension())
      gra->setSignExtensionRequired(true, rc->getGlobalRegisterNumber());
   else
      gra->setSignExtensionNotRequired(true, rc->getGlobalRegisterNumber());

   TR::TreeTop *tt = TR::TreeTop::create(comp, prevTreeTop, store);
   load->setVisitCount(visitCount);
   TR::Symbol *sym = rc->getSymbolReference()->getSymbol();
   if (!rc->is8BitGlobalGPR())
      load->setIsInvalid8BitGlobalRegister(true);
   setValue(load);
   setAutoContainsRegisterValue(true);

   if (store->requiresRegisterPair(comp))
      dumpOptDetails(comp, "%s create store [%p] of symRef#%d to Register %d (low word) and Register %d (high word)\n", OPT_DETAILS, store, rc->getSymbolReference()->getReferenceNumber(), rc->getLowGlobalRegisterNumber(), rc->getHighGlobalRegisterNumber());
   else
      dumpOptDetails(comp, "%s create store [%p] of %s symRef#%d to Register %d\n", OPT_DETAILS, store,
            rc->getSymbolReference()->getSymbol()->isMethodMetaData() ? rc->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName():"", rc->getSymbolReference()->getReferenceNumber(), rc->getGlobalRegisterNumber());

   return load;
   }

TR::Node *
TR_GlobalRegister::createStoreFromRegister(vcount_t visitCount, TR::TreeTop * prevTreeTop, int32_t i, TR::Compilation *comp, bool storeUnconditionally)
   {
   if (!storeUnconditionally)
      {
      TR_ASSERT(!getAutoContainsRegisterValue(), "don't need to store the register back into the auto");
      TR_ASSERT(getLastRefTreeTop(), "expected getLastRefTreeTop to be nonzero");
      }
   else
      TR_ASSERT(prevTreeTop, "expected prevTreeTop to be nonzero");

   if (!prevTreeTop)
      prevTreeTop = getLastRefTreeTop();

   TR::Node *prevNode = prevTreeTop->getNode();
   if ((prevNode->getOpCodeValue() == TR::NULLCHK) ||
       (prevNode->getOpCodeValue() == TR::treetop))
      prevNode = prevNode->getFirstChild();

   TR::ILOpCode &prevOpCode = prevNode->getOpCode();
   if (prevOpCode.isBranch() ||
       prevOpCode.isJumpWithMultipleTargets() ||
       prevOpCode.isReturn() ||
       prevOpCode.getOpCodeValue() == TR::athrow ||
       prevOpCode.getOpCodeValue() == TR::BBEnd)
      prevTreeTop = prevTreeTop->getPrevTreeTop();

   TR_RegisterCandidate * rc = getCurrentRegisterCandidate();
   TR::Node *node = getValue();
   TR::Node * store = TR::Node::createWithSymRef(comp->il.opCodeForDirectStore(rc->getDataType()), 1, 1, node, rc->getSymbolReference());
   store->setVisitCount(visitCount);
   rc->addStore(TR::TreeTop::create(comp, prevTreeTop, store));
   setAutoContainsRegisterValue(true);
   rc->setExtendedLiveRange(true);

   if (i != -1)
      {
      if (store->requiresRegisterPair(comp))
         dumpOptDetails(comp, "%s create store [%p] from Register %d (low word) and Register %d (high word)\n", OPT_DETAILS, store, rc->getLowGlobalRegisterNumber(), rc->getHighGlobalRegisterNumber());
      else
         dumpOptDetails(comp, "%s create store [%p] from Register %d for %s #%d\n", OPT_DETAILS, store,
               rc->getGlobalRegisterNumber(), rc->getSymbolReference()->getSymbol()->isMethodMetaData() ? rc->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName():"",rc->getSymbolReference()->getReferenceNumber());
      }

   return store;
   }

void
TR_GlobalRegisterAllocator::findIfThenRegisterCandidates()
   {
   LexicalTimer t("TR_GlobalRegisterAllocator::findIfThenRegisterCandidates", comp()->phaseTimer());

   TR_ScratchList<TR_RegisterCandidate> registerCandidates(trMemory());
   TR::CFG * cfg = comp()->getFlowGraph();

   TR::ResolvedMethodSymbol              *methodSymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>   paramIterator(&(methodSymbol->getParameterList()));
   ListIterator<TR::AutomaticSymbol>   autoIterator(&(methodSymbol->getAutomaticList()));
   TR::ParameterSymbol      *paramCursor = paramIterator.getFirst();
   TR::AutomaticSymbol      *autoCursor = autoIterator.getFirst();

   // This first part isn't really looking for If-Then-Else candidates.
   // It is visiting all parameters and locals and ensuring that the candidate has a BlockInfo entry if it
   // is live in any block.
   if(!debug("oldIfThen"))
     {
     LexicalTimer t("newFindIfThen", comp()->phaseTimer());

     // First create a bit vector of auto and parm symbols
     // Guess at size of bit vector to use by getting first block's LiveLocals bitvector and check its size
     TR_BitVector *guess = toBlock(cfg->getFirstNode())->getLiveLocals();
     int32_t guessSize = 1024;
     if(guess && guess->numChunks()*BITS_IN_CHUNK > guessSize)
       guessSize = guess->numChunks()*BITS_IN_CHUNK;

     TR_BitVector autoAndParmLiveLocalIndex(guessSize, trMemory(), stackAlloc, growable);
     TR_Array<TR_RegisterCandidate*> registerCandidateByIndex(trMemory(), guessSize, false, stackAlloc);
     autoAndParmLiveLocalIndex.empty();
     int32_t i;
     while (paramCursor != NULL)
       {
       if (paramCursor->isReferencedParameter())
         {
         TR_RegisterCandidate *rc = comp()->getGlobalRegisterCandidates()->find(paramCursor);
         if (!rc)
           {
           paramCursor = paramIterator.getNext();
           continue;
           }
         i = paramCursor->getLiveLocalIndex();
         autoAndParmLiveLocalIndex.set(i);
         registerCandidateByIndex[i] = rc;
         }
       paramCursor = paramIterator.getNext();
       }
     while (autoCursor != NULL)
       {
       TR_RegisterCandidate *rc = comp()->getGlobalRegisterCandidates()->find(autoCursor);
       if (!rc)
         {
         autoCursor = autoIterator.getNext();
         continue;
         }
         i = autoCursor->getLiveLocalIndex();
         autoAndParmLiveLocalIndex.set(i);
         registerCandidateByIndex[i] = rc;
         autoCursor = autoIterator.getNext();
       }

     // Now visit all blocks and intersect each blocks LiveLocals with autoAndParmLiveLocalIndex.
     // For each intersected bit ensure BlockInfo exists for the candidate and initialize it
     // to zero NumberOfLoadsAndStores if it does not exist
     guessSize = autoAndParmLiveLocalIndex.numChunks()*BITS_IN_CHUNK;
     TR_BitVector intersection(guessSize, trMemory(), stackAlloc, growable);
     for (TR::CFGNode * block = cfg->getFirstNode(); block; block = block->getNext())
       {
       TR_BitVector * liveLocals = toBlock(block)->getLiveLocals();
       if (cg()->getLiveLocals() && liveLocals)
         {
         if (block != comp()->getStartBlock() &&
             block != cfg->getStart() &&
             block != cfg->getEnd())
           {
           intersection = autoAndParmLiveLocalIndex;
           intersection &= *liveLocals;
           TR_BitVectorIterator bvi(intersection);
           while (bvi.hasMoreElements())
             {
             int32_t autoOrParm = bvi.getNextElement();
             TR_RegisterCandidate *rc=registerCandidateByIndex[autoOrParm];
             if(!rc->_blocks.find(block->getNumber()))
                rc->_blocks.setNumberOfLoadsAndStores(toBlock(block)->getNumber(), 0);
             }
           }
         }
       }
     }
   else
     {
     LexicalTimer t("oldFindIfThen", comp()->phaseTimer());
   while (paramCursor != NULL)
      {
      if (paramCursor->isReferencedParameter())
         {
         TR_RegisterCandidate *rc = comp()->getGlobalRegisterCandidates()->find(paramCursor);
         if (!rc)
            {
            paramCursor = paramIterator.getNext();
            continue;
            }

         for (TR::CFGNode * block = cfg->getFirstNode(); block; block = block->getNext())
            {
            TR_BitVector * liveLocals = toBlock(block)->getLiveLocals();
            if (cg()->getLiveLocals() &&
                liveLocals && !rc->_blocks.find(block->getNumber()))
               {
               if (block != comp()->getStartBlock() &&
                  block != cfg->getStart() &&
                  block != cfg->getEnd() &&
                  liveLocals->get(paramCursor->getLiveLocalIndex()))
                  {
                  rc->_blocks.setNumberOfLoadsAndStores(toBlock(block)->getNumber(), 0);
                  }
               }
            }
         }

      paramCursor = paramIterator.getNext();
      }

   while (autoCursor != NULL)
      {
      //if (paramCursor->isReferencedParameter())
         {
         TR_RegisterCandidate *rc = comp()->getGlobalRegisterCandidates()->find(autoCursor);
         if (!rc)
            {
            autoCursor = autoIterator.getNext();
            continue;
            }

         for (TR::CFGNode * block = cfg->getFirstNode(); block; block = block->getNext())
            {
            TR_BitVector * liveLocals = toBlock(block)->getLiveLocals();
            if (cg()->getLiveLocals() &&
                liveLocals && !rc->_blocks.find(block->getNumber()))
               {
               if (block != comp()->getStartBlock() &&
                   block != cfg->getStart() &&
                   block != cfg->getEnd() &&
                   liveLocals->get(autoCursor->getLiveLocalIndex()))
                  {
                  rc->_blocks.setNumberOfLoadsAndStores(toBlock(block)->getNumber(), 0);
                  }
               }
            }
         }

      autoCursor = autoIterator.getNext();
      }
     }

   for (TR::CFGNode * block = cfg->getFirstNode(); block; block = block->getNext())
      {
      TR::CFGEdgeList& edges = block->getSuccessors();
      TR::Block *currBlock = toBlock(block);
      if ((edges.size() == 2) && currBlock->getExit())
         {
         TR::Block * block1 = toBlock(edges.front()->getTo());
         TR::Block * block2 = toBlock((*(++edges.begin()))->getTo());

         TR::Block *mergeBlock1 = NULL, *mergeBlock2 = NULL;
         if (block1->getSuccessors().size() == 1)
            mergeBlock1 = toBlock(block1->getSuccessors().front()->getTo());

         if (mergeBlock1)
            {
            if (block2->hasSuccessor(mergeBlock1))
               mergeBlock2 = mergeBlock1;
            }
         else
            {
            if (block2->getSuccessors().size() == 1)
               mergeBlock2 = toBlock(block2->getSuccessors().front()->getTo());

            if (mergeBlock2)
               {
               if (block1->hasSuccessor(mergeBlock2))
                  mergeBlock1 = mergeBlock2;
               }
            }


         //if (block1->getSuccessors().size() == 1) && block2->getSuccessors().size() == 1))
            {
              //TR::Block * mergeBlock1 = toBlock(block1->getSuccessors().front()->getTo());
              //TR::Block * mergeBlock2 = toBlock(block2->getSuccessors().front()->getTo());
            if (mergeBlock1 &&
                mergeBlock1 == mergeBlock2 &&
                mergeBlock1 != cfg->getEnd())
               {
               for (TR::TreeTop * tt1 = block1->getEntry(); tt1 != block1->getExit(); tt1 = tt1->getNextTreeTop())
                  {
                  TR::Node * storeNode1 = tt1->getNode()->getStoreNode();
                  if (storeNode1 && storeNode1->getOpCode().isStoreDirect())
                     {
                     TR_RegisterCandidate * rc = 0;
                     TR::SymbolReference * symRef = tt1->getNode()->getSymbolReference();
                     if ((symRef->getSymbol()->isAutoOrParm() ) &&
                          isSymRefAvailable(symRef))
                        for (TR::TreeTop * tt2 = block2->getEntry(); tt2 != block2->getExit(); tt2 = tt2->getNextTreeTop())
                           {
                           TR::Node * storeNode2 = tt2->getNode()->getStoreNode();
                           if (storeNode2 && storeNode2->getOpCode().isStoreDirect() && storeNode2->getSymbolReference() == symRef)
                              {
                              rc = comp()->getGlobalRegisterCandidates()->findOrCreate(symRef);
                              break;
                              }
                           }
                     if (rc)
                        {
                        int32_t weight = 3;
                        if (mergeBlock1->findFirstReference(symRef->getSymbol(), comp()->incVisitCount()))
                           {
                           if (mergeBlock1->getStructureOf())
                              optimizer()->getStaticFrequency(mergeBlock1, &weight);

                           rc->addBlock(mergeBlock1, weight);
                           }
                        if (toBlock(block)->findFirstReference(symRef->getSymbol(), comp()->incVisitCount()))
                           {
                           weight = 1;
                           if (toBlock(block)->getStructureOf())
                              optimizer()->getStaticFrequency(toBlock(block), &weight);

                           rc->addBlock(block1, weight);
                           rc->addBlock(block2, weight);
                           }
                        }
                     }
                  }
               }
            }


         TR::Node *virtualGuard = currBlock->getLastRealTreeTop()->getNode();
         if (virtualGuard->isTheVirtualGuardForAGuardedInlinedCall())
            {
            TR::Block *branchBlock = virtualGuard->getBranchDestination()->getNode()->getBlock();
            TR::TreeTop *tt1 = currBlock->getEntry();
            for (;tt1 != currBlock->getExit(); tt1 = tt1->getNextTreeTop())
               {
               TR::Node * storeNode1 = tt1->getNode()->getStoreNode();
               if (storeNode1 && storeNode1->getOpCode().isStoreDirect())
                  {
                  TR_RegisterCandidate * rc = 0;
                  TR::SymbolReference * symRef = tt1->getNode()->getSymbolReference();
                  if ((symRef->getSymbol()->isAutoOrParm() ) &&
                      isSymRefAvailable(symRef))
                      {
                      rc = comp()->getGlobalRegisterCandidates()->findOrCreate(symRef);
                      }

                  if (rc)
                     {
                     int32_t weight = 1;
                     if (branchBlock->findFirstReference(symRef->getSymbol(), comp()->incVisitCount()))
                        {
                        if (branchBlock->getStructureOf())
                           optimizer()->getStaticFrequency(branchBlock, &weight);
                        //printf("Adding symRef %d in block_%d\n", symRef->getReferenceNumber(), branchBlock->getNumber());
                        rc->addBlock(branchBlock, weight);
                        }
                     }
                  }
               }
            }
         }
      }
   }

void TR_GlobalRegisterAllocator::offerAllAutosAndRegisterParmAsCandidates(TR::Block **cfgBlocks, int32_t numberOfNodes, bool onlySelectedCandidates)
   {
   LexicalTimer t("TR_GlobalRegisterAllocator::offerAllAutosAndRegisterParmsAsCandidates", comp()->phaseTimer());

   TR::ResolvedMethodSymbol            *methodSymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>   paramIterator(&(methodSymbol->getParameterList()));
   TR::CFG                             *cfg = comp()->getFlowGraph();
   TR::CFGNode                         *node;
   TR::Block                           *block, *startBlock=toBlock(cfg->getStart()), *endBlock=toBlock(cfg->getEnd());
   int32_t                             symRefCount  = comp()->getSymRefCount();
   TR::SymbolReferenceTable            *symRefTab   = comp()->getSymRefTab();
   TR::SymbolReference                 *symRef;
   TR::Symbol                          *sym;
   TR_RegisterCandidates               *candidates = comp()->getGlobalRegisterCandidates();


   // Interested blocks consist of all blocks except for entry, exit and exception handlers
   TR_BitVector interestedBlocks(numberOfNodes, comp()->trMemory()->currentStackRegion());
   TR_BitVector tmp(numberOfNodes, comp()->trMemory()->currentStackRegion());
   for (node = cfg->getFirstNode(); node != NULL; node = node->getNext())
      {
      block = toBlock(node);
      if (block == startBlock || block == endBlock || (!block->getExceptionPredecessors().empty()) || !cfgBlocks[block->getNumber()])
         continue;
      interestedBlocks.set(block->getNumber());
      }

   //
   // Offer parameters first
   //
   for (TR::ParameterSymbol *paramCursor = paramIterator.getFirst(); paramCursor != NULL; paramCursor = paramIterator.getNext())
      {
      symRef = methodSymbol->getParmSymRef(paramCursor->getSlot());

      if ((paramCursor->isReferencedParameter() && isTypeAvailable(symRef)) &&
          !onlySelectedCandidates)
         {
         if (!isSymRefAvailable(symRef))
            continue;

         int32_t symRefNumber = symRef->getReferenceNumber();

         // Check there is an interested block that references the symref
         tmp.empty();
         tmp |= *candidates->getBlocksReferencingSymRef(symRefNumber);
         tmp &= interestedBlocks;
         if (tmp.isEmpty())
            continue;

         TR_RegisterCandidate *rc = comp()->getGlobalRegisterCandidates()->findOrCreate(symRef);

         // All interested blocks will be candidates
         rc->_blocks.getCandidateBlocks() |= interestedBlocks;

         // Increment the number of loads and stores for all candidate blocks
         // that also reference the symref
         TR_BitVectorIterator bvi(tmp);
         while (bvi.hasMoreElements())
            rc->_blocks.incNumberOfLoadsAndStores(bvi.getNextElement(), 1);

         if (paramCursor->getLinkageRegisterIndex() >= 0)
            rc->addAllBlocks();
         }
      }

   //
   // Offer all autos now
   //
   for (int32_t symRefNumber = symRefTab->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
      {
      symRef = symRefTab->getSymRef(symRefNumber);
      if (symRef && isSymRefAvailable(symRef))
         {
         sym = symRef->getSymbol();
         if (sym)
            {
            if (candidates->aliasesPreventAllocation(comp(),symRef))
               {
               if (comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator))
                  traceMsg(comp(),"Leaving candidate #%d because it has use_def_aliases\n",symRefNumber);
               continue;
               }

            if ((sym->isAuto() &&
                methodSymbol->getAutomaticList().find(sym->castToAutoSymbol()) &&
                !onlySelectedCandidates))
               {

               int32_t symRefNumber = symRef->getReferenceNumber();

               // Check there is an interested block that references the symref
               tmp.empty();
               tmp |= *candidates->getBlocksReferencingSymRef(symRefNumber);
               tmp &= interestedBlocks;
               if (tmp.isEmpty())
                  continue;

               TR_RegisterCandidate *rc = comp()->getGlobalRegisterCandidates()->findOrCreate(symRef);
               if (sym->isMethodMetaData() && rc && rc->initialBlocksWeightComputed())
                  continue;

               // All interested blocks will be candidates
               rc->_blocks.getCandidateBlocks() |= interestedBlocks;

               // Increment the number of loads and stores for all candidate blocks
               // that also reference the symref
               TR_BitVectorIterator bvi(tmp);
               while (bvi.hasMoreElements())
                  rc->_blocks.incNumberOfLoadsAndStores(bvi.getNextElement(), 1);

               rc->setInitialBlocksWeightComputed(true);
               }
            }
         }
      }
   }



void TR_GlobalRegisterAllocator::offerAllFPAutosAndParmsAsCandidates(TR::Block * *cfgBlocks, int32_t numberOfNodes)
   {
   LexicalTimer t("TR_GlobalRegisterAllocator::offerAllFPAutosAndParmsAsCandidates", comp()->phaseTimer());

   TR::CFG                  *cfg = comp()->getFlowGraph();
   TR::CFGNode              *node;
   TR::Block                *block, *startBlock=toBlock(cfg->getStart()), *endBlock=toBlock(cfg->getEnd());
   int32_t                  symRefCount = comp()->getSymRefCount();
   TR::SymbolReferenceTable *symRefTab   = comp()->getSymRefTab();
   TR::SymbolReference      *symRef;
   TR::Symbol               *sym;
   TR::ResolvedMethodSymbol *methodSymbol = comp()->getJittedMethodSymbol();
   TR_RegisterCandidates    *candidates = comp()->getGlobalRegisterCandidates();

   // Interested blocks consist of all blocks except for entry, exit and exception handlers
   TR_BitVector interestedBlocks(numberOfNodes, comp()->trMemory()->currentStackRegion());
   TR_BitVector tmp(numberOfNodes, comp()->trMemory()->currentStackRegion());
   for (node = cfg->getFirstNode(); node != NULL; node = node->getNext())
      {
      block = toBlock(node);
      if (block == startBlock || block == endBlock || (!block->getExceptionPredecessors().empty()) || !cfgBlocks[block->getNumber()])
         continue;

      interestedBlocks.set(block->getNumber());
      }

   //
   // Offer all FP autos now
   //
   for (int32_t symRefNumber = symRefTab->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
      {
      symRef = symRefTab->getSymRef(symRefNumber);
      if (symRef)
         {
         sym = symRef->getSymbol();
         if (sym)
            {

            if ((sym->getDataType() == TR::Float
                 || sym->getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                 || sym->getDataType() == TR::DecimalFloat
                 || sym->getDataType() == TR::DecimalDouble
                 || sym->getDataType() == TR::DecimalLongDouble
#endif
                 ) &&
                 isTypeAvailable(symRef) &&
                ((sym->isAuto() && methodSymbol->getAutomaticList().find(sym->castToAutoSymbol())) ||
                 (sym->isParm() && methodSymbol->getParameterList().find(sym->castToParmSymbol()) && sym->isReferencedParameter())))
               {

               int32_t symRefNumber = symRef->getReferenceNumber();

               // Check there is an interested block that references the symref
               tmp.empty();
               tmp |= *candidates->getBlocksReferencingSymRef(symRefNumber);
               tmp &= interestedBlocks;
               if (tmp.isEmpty())
                  continue;

               TR_RegisterCandidate *rc = comp()->getGlobalRegisterCandidates()->findOrCreate(symRef);

               // All interested blocks will be candidates
               rc->_blocks.getCandidateBlocks() |= interestedBlocks;

               // Increment the number of loads and stores for all candidate blocks
               // that also reference the symref
               TR_BitVectorIterator bvi(tmp);
               while (bvi.hasMoreElements())
                  rc->_blocks.incNumberOfLoadsAndStores(bvi.getNextElement(), 1);
               }
            }
         }
      }
   }

void
TR_GlobalRegisterAllocator::findLoopAutoRegisterCandidates()
   {
   LexicalTimer t("TR_GlobalRegisterAllocator::findLoopAutoRegisterCandidates", comp()->phaseTimer());
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR::CFG * cfg = comp()->getFlowGraph();
   vcount_t visitCount = comp()->incVisitCount();
   TR_Structure *rootStructure = comp()->getFlowGraph()->getStructure();
   SymRefCandidateMap * registerCandidates = new (trStackMemory()) SymRefCandidateMap((SymRefCandidateMapComparator()), SymRefCandidateMapAllocator(trMemory()->currentStackRegion()));
   findLoopsAndCorrespondingAutos(NULL, visitCount, *registerCandidates);
   }

TR_GlobalRegisterAllocator::BlockInfo &
TR_GlobalRegisterAllocator::blockInfo(int32_t i)
   {
   TR_ASSERT(i != -1, "block number has not been assigned");
   TR_ASSERT(_blockInfo, "blockInfo has not been set");
   return _blockInfo[i];
   }

void
TR_GlobalRegisterAllocator::findLoopsAndCorrespondingAutos(TR_StructureSubGraphNode *structureNode, vcount_t visitCount, SymRefCandidateMap &registerCandidates)
   {
   TR_Structure *structure;
   if (structureNode)
      structure = structureNode->getStructure();
   else
      structure = comp()->getFlowGraph()->getStructure();

   if (structure->asRegion())
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *subNode;
      TR_Structure             *subStruct = NULL;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         subStruct = subNode->getStructure();
         findLoopsAndCorrespondingAutos(subNode, visitCount, registerCandidates);
         }

      if (!regionStructure->isAcyclic() && structureNode)
         {
         TR_ScratchList<TR::Block> blocksInLoop(trMemory());
         regionStructure->getBlocks(&blocksInLoop);

         TR_BitVector assignedAutosInCurrentLoop(_origSymRefCount, trMemory(), stackAlloc);

         TR_BitVector *symsThatShouldNotBeAssignedInCurrentLoop = NULL;
         TR_BitVector *symsThatShouldBeAssignedInCurrentLoop = NULL;

         bool excludeInvariantsEnabled = comp()->cg()->excludeInvariantsFromGRAEnabled() &&
                                        (!comp()->getOptions()->getOption(TR_DisableRXusage));

         if (excludeInvariantsEnabled)
            {
            symsThatShouldNotBeAssignedInCurrentLoop = new (trStackMemory()) TR_BitVector(_origSymRefCount, trMemory(), stackAlloc);
            symsThatShouldNotBeAssignedInCurrentLoop->setAll(_origSymRefCount);
            symsThatShouldBeAssignedInCurrentLoop = new (trStackMemory()) TR_BitVector(_origSymRefCount, trMemory(), stackAlloc);
            }

         ListIterator<TR::Block> blocksIt(&blocksInLoop);
         TR::Block *nextBlock;
         bool hasCatchBlock = false;
         for (nextBlock = blocksIt.getFirst(); nextBlock; nextBlock=blocksIt.getNext())
            {
            if (nextBlock->isCatchBlock())
               hasCatchBlock = true;
            }

         TR_BitVector *oldCandidatesSignExtendedInThisLoop = NULL;
         if (_candidatesSignExtendedInThisLoop)
            {
            oldCandidatesSignExtendedInThisLoop = new (trStackMemory()) TR_BitVector(_origSymRefCount, trMemory(), stackAlloc);
            *oldCandidatesSignExtendedInThisLoop = *_candidatesSignExtendedInThisLoop;
            _candidatesSignExtendedInThisLoop->empty();
            }

         for (nextBlock = blocksIt.getFirst(); nextBlock; nextBlock=blocksIt.getNext())
            {
            if (nextBlock->getVisitCount() != visitCount)
               {
               static uint32_t numIter = 0;
               if (((++numIter) & 0x3f)==0 && comp()->compilationShouldBeInterrupted(GRA_FIND_LOOPS_AND_CORRESPONDING_AUTOS_BLOCK_CONTEXT))
                  {
                  comp()->failCompilation<TR::CompilationInterrupted>("interrupted in GRA-findLoopsAndCorrspondingAuto-block");
                  }
               nextBlock->setVisitCount(visitCount);
               int32_t executionFrequency = 1;
               if (nextBlock->getStructureOf())
                  optimizer()->getStaticFrequency(nextBlock, &executionFrequency);

               TR::TreeTop *currentTree = nextBlock->getEntry();
               TR::TreeTop *exitTree = nextBlock->getExit();
               while (currentTree != exitTree)
                  {
                  TR::Node *currentNode = currentTree->getNode();
                  TR::Node *arrayAccess = NULL;
                  markAutosUsedIn(currentNode, NULL, NULL, &arrayAccess, nextBlock, &blocksInLoop, visitCount, executionFrequency, registerCandidates, &assignedAutosInCurrentLoop, symsThatShouldNotBeAssignedInCurrentLoop, hasCatchBlock);
                  currentTree = currentTree->getNextRealTreeTop();
                  }
               }
            }

         if (oldCandidatesSignExtendedInThisLoop)
            *_candidatesSignExtendedInThisLoop = *oldCandidatesSignExtendedInThisLoop;

         for (auto succ = structureNode->getSuccessors().begin(); succ != structureNode->getSuccessors().end(); ++succ)
            {
            TR_Structure *exitStructure = (*succ)->getTo()->asStructureSubGraphNode()->getStructure();
            TR::Block *exitBlock = NULL;
            if (exitStructure)
               exitBlock = exitStructure->getEntryBlock();

            // Turning off this code below. Reason : extending the live range till the
            // exit blocks may lead to slowdowns if the loop (whose exit we are extending
            // the live range to) was actually run for only a few iterations and
            // the outer loop was in fact long running. Making the candidate live on entry
            // to an exit out of the inner loop may cause it to be loaded up into the register
            // on some hot path in the outer loop and this extra load could degrade performance.
            // This scenario actually occurred in db shell sort, where the innermost loop is
            // String compareTo code which does not run too long, but the outer loops
            // are very hot.
            //
            if (0 && exitBlock &&
                (exitBlock != comp()->getFlowGraph()->getEnd()))
               {
               TR_BitVectorIterator bvi(assignedAutosInCurrentLoop);
               while (bvi.hasMoreElements())
                  {
                  int32_t nextCandidate = bvi.getNextElement();
                  //dumpOptDetails(comp(), "For loop %d exit block_%d candidate %d\n", structureNode->getNumber(), exitBlock->getNumber(), nextCandidate);
                  TR_RegisterCandidate *rc = registerCandidates[nextCandidate];
                  rc->addBlock(exitBlock, 0);
                  rc->addLoopExitBlock(exitBlock);
                  }
               }
            }

         if (symsThatShouldNotBeAssignedInCurrentLoop)
            {
            *symsThatShouldNotBeAssignedInCurrentLoop &= assignedAutosInCurrentLoop;

            TR_BitVectorIterator bvi(*symsThatShouldNotBeAssignedInCurrentLoop);
            while (bvi.hasMoreElements())
               {
               int32_t nextCandidate = bvi.getNextElement();
               if (!symsThatShouldBeAssignedInCurrentLoop->get(nextCandidate))
                  {
                  *_temp2 = *symsThatShouldBeAssignedInCurrentLoop;
                  TR_RegisterCandidate *rc = registerCandidates[nextCandidate];
                  TR::SymbolReference *symRef = rc->getSymbolReference();
                  ListIterator<TR_PairedSymbols> pairs(&_pairedSymbols);
                  bool doNotRemoveThisCandidate = false;
                  TR_PairedSymbols *p;
                  for (p = pairs.getFirst(); p; p = pairs.getNext())
                     {
                     TR::SymbolReference *otherSymRef = NULL;
                     if (p->_symRef1 == symRef)
                        otherSymRef = p->_symRef2;
                     else if (p->_symRef2 == symRef)
                        otherSymRef = p->_symRef1;

                     if (otherSymRef && symsThatShouldNotBeAssignedInCurrentLoop->get(otherSymRef->getReferenceNumber()))
                        {
                        TR_RegisterCandidate *otherCandidate = registerCandidates[otherSymRef->getReferenceNumber()];
                        if (otherCandidate->countNumberOfLoadsAndStoresInBlocks(&blocksInLoop) < rc->countNumberOfLoadsAndStoresInBlocks(&blocksInLoop))
                           symsThatShouldBeAssignedInCurrentLoop->set(otherSymRef->getReferenceNumber());
                        else
                           doNotRemoveThisCandidate = true;
                        }
                     }

                  if (!doNotRemoveThisCandidate)
                     {
                    //printf("Removing candidate in %s\n", _compilation->getCurrentMethod()->signature());
                     blocksIt.reset();
                     for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
                        rc->removeBlock(nextBlock);

                     for (auto succ = structureNode->getSuccessors().begin(); succ != structureNode->getSuccessors().end(); ++succ)
                        {
                        TR_Structure *exitStructure = (*succ)->getTo()->asStructureSubGraphNode()->getStructure();
                        TR::Block *exitBlock = NULL;
                        if (exitStructure)
                           exitBlock = exitStructure->getEntryBlock();
                        if (exitBlock &&
                            (exitBlock != comp()->getFlowGraph()->getEnd()))
                           {
                           rc->removeBlock(exitBlock);
                           rc->removeLoopExitBlock(exitBlock);
                           }
                        }
                     }
                  else
                     *symsThatShouldBeAssignedInCurrentLoop = *_temp2;
                  }
               }
            }
         }
      }
   }


bool TR_GlobalRegisterAllocator::isDependentStore(TR::Node *node, const TR_UseDefInfo::BitVector &defs, TR::SymbolReference *symRef, bool *seenLoad)
   {
   if (node->getOpCode().isLoadVar())
      {
      if (symRef->getSymbol() == node->getSymbolReference()->getSymbol())
         {
         *seenLoad = true;
         int32_t useIndex = node->getUseDefIndex();
         TR_UseDefInfo::BitVector childDefs(comp()->allocator());
         if (optimizer()->getUseDefInfo()->getUseDef(childDefs, useIndex))
            {
            TR_UseDefInfo::BitVector temp(comp()->allocator());
            temp = childDefs;
            temp -= defs;
            if (!temp.IsZero())
               return false;
            }
         }
      else
         return false;
      }

  int32_t i;
  for (i=0;i<node->getNumChildren();i++)
     {
     if (!isDependentStore(node->getChild(i), defs, symRef, seenLoad))
       return false;
     }

  return true;
  }



void
TR_GlobalRegisterAllocator::markAutosUsedIn(
   TR::Node               *node,
   TR::Node               *parent,
   TR::Node               *grandParent,
   TR::Node              **currentArrayAccess,
   TR::Block              *block,
   List<TR::Block>        *blocksInLoop,
   vcount_t               visitCount,
   int32_t                executionFrequency,
   SymRefCandidateMap    &registerCandidates,
   TR_BitVector          *assignedAutosInCurrentLoop,
   TR_BitVector          *symsThatShouldNotBeAssignedInCurrentLoop,
   bool                   hasCatchBlock)
   {
   bool enableSignExtGRA = false; // enable for other platforms later
   bool enableNewSignExtGRA = false;

   static char *doit = feGetEnv("TR_SIGNEXTGRA");
   if (NULL != doit)
      enableSignExtGRA = true;

   if (TR::Compiler->target.cpu.isZ())
      {
      enableSignExtGRA = true;
      static char *doit2 = feGetEnv("TR_NSIGNEXTGRA");
      if (NULL != doit2)
         enableSignExtGRA = false;

      static char *doit3 = feGetEnv("TR_NEWSIGNEXTGRA");
      if (NULL != doit3)
         enableNewSignExtGRA = true;
      }

   TR::Node *origNode = NULL;
   TR::Node *prevArrayAccess = NULL;
   if (enableNewSignExtGRA)
      {
      prevArrayAccess = *currentArrayAccess;

      if (node->getOpCode().hasSymbolReference() &&
          (node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()))
         {
         TR::Node *child = node->getFirstChild();
         if (child->getOpCodeValue() == TR::aladd)
            {
            TR::Node *offsetChild = child->getSecondChild();
            if (((offsetChild->getOpCodeValue() == TR::ladd) || (offsetChild->getOpCodeValue() == TR::lsub)) &&
                (offsetChild->getSecondChild()->getOpCodeValue() == TR::lconst))
               offsetChild = offsetChild->getFirstChild();

            int32_t sizeOfElem;
            if (node->getDataType() == TR::Address)
               sizeOfElem = TR::Compiler->om.sizeofReferenceField();
            else
               sizeOfElem = node->getOpCode().getSize();

            if ((offsetChild->getOpCodeValue() == TR::lmul) &&
                (offsetChild->getSecondChild()->getOpCodeValue() == TR::lconst) &&
                (sizeOfElem == offsetChild->getSecondChild()->getLongInt()))
               {
               TR::Node *longIndexNode = offsetChild->getFirstChild();
               if (longIndexNode->getOpCodeValue() == TR::i2l)
                  {
                  *currentArrayAccess = longIndexNode;
                  }
               }
            else if ((offsetChild->getOpCodeValue() == TR::i2l) &&
                     (sizeOfElem == 1))
               {
               *currentArrayAccess = offsetChild;
               }
            }
         }

      bool canSkip = false;
      if (0 && ((node->getOpCode().isLoadVarDirect() && node->getSymbolReference()->getSymbol()->isAuto()) ||
          (((node->getOpCodeValue() == TR::iadd) || (node->getOpCodeValue() == TR::isub)) &&
            node->getFirstChild()->getOpCode().isLoadVarDirect() &&
            node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
            node->getSecondChild()->getOpCode().isLoadConst() /* &&
                                                                 ((node->getSecondChild()->getInt() <= 32767) && (node->getSecondChild()->getInt() >= -32767)) */ )))
         {
         if (!_rejectedSignExtNodes.find(node))
            canSkip = true;
         }

      if (!canSkip &&
          ((node->getOpCodeValue() == TR::iadd) || (node->getOpCodeValue() == TR::isub) ||
           (node->getOpCodeValue() == TR::iand) || (node->getOpCodeValue() == TR::iloadi) || (node->getOpCodeValue() == TR::ixor)) ||
          ((node->getOpCodeValue() == TR::iload) /* && !node->getSymbolReference()->getSymbol()->isAutoOrParm() */))
         {
         TR::Node *storeNode = NULL;
         TR::Node *loadNode = NULL;
         if (*currentArrayAccess)
            {
            if (TR::Compiler->target.is64Bit() &&
                !_rejectedSignExtNodes.find(node))
               {
               if ((node->getOpCodeValue() == TR::iload) &&
                   (node->getSymbolReference()->getSymbol()->isAutoOrParm()
                   ))
                  {
                  loadNode = node;
                  canSkip = true;
                  }
               else if (false)
                  {
                  TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
                  TR::TreeTop *cursorTree = _candidates->_startOfExtendedBBForBB[block->getNumber()]->getEntry()->getNextTreeTop();
                  while (cursorTree)
                     {
                     TR::Node *cursorNode = cursorTree->getNode();
                     if (cursorNode->getOpCode().isStoreDirect() &&
                         cursorNode->getSymbolReference()->getSymbol()->isAuto() &&
                         (cursorNode->getFirstChild() == node))
                        {
                        storeNode = cursorNode;
                        canSkip = true;
                        break;
                        }

                      if (cursorNode->getOpCodeValue() == TR::BBStart)
                         {
                         if (!cursorNode->getBlock()->isExtensionOfPreviousBlock())
                            break;
                         }

                      cursorTree = cursorTree->getNextTreeTop();
                     }
                  }
               }
            }

         if (canSkip)
            {
            TR_BlockStructure *blockStructure = block->getStructureOf();
            if (comp()->getFlowGraph()->getStructure() &&
                blockStructure)
               {
               TR_Structure *containingLoop = blockStructure->getContainingLoop();

               if (containingLoop)
                  {
                  TR::Block *loopInvariantBlock = NULL;
                  TR_RegionStructure *parentStructure = containingLoop->getParent()->asRegion();
                  TR_StructureSubGraphNode *subNode = NULL;
                  TR_RegionStructure::Cursor si(*parentStructure);
                  for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
                     {
                     if (subNode->getNumber() == containingLoop->getNumber())
                        break;
                     }

                  if ((subNode->getPredecessors().size() == 1))
                     {
                     TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
                     if (loopInvariantNode->getStructure()->asBlock() &&
                         loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
                        loopInvariantBlock = loopInvariantNode->getStructure()->asBlock()->getBlock();
                     }

                  if (loopInvariantBlock)
                     {
                     TR::SymbolReference *storeSymRef = NULL;
                     if (loadNode)
                        storeSymRef = loadNode->getSymbolReference();
                     else
                        storeSymRef = storeNode->getSymbolReference();

                     TR::Node *signExtStore = NULL;

                     if (_candidatesSignExtendedInThisLoop->get(storeSymRef->getReferenceNumber()))
                        {
                        TR::TreeTop *cursorTree = loopInvariantBlock->getExit();
                        while (cursorTree)
                           {
                           TR::Node *cursorNode = cursorTree->getNode();
                           if ((cursorNode->getOpCodeValue() == comp()->il.opCodeForDirectStore(node->getDataType())) &&
                               (cursorNode->getSymbolReference() == storeSymRef))
                              {
                              if (cursorNode->getFirstChild()->getOpCode().isLoadVar() &&
                                  (cursorNode->getFirstChild()->getSymbolReference() == cursorNode->getSymbolReference()) &&
                                  cursorNode->needsSignExtension())
                                 {
                                 signExtStore = cursorNode;
                                 break;
                                 }
                              }

                           if (cursorNode->getOpCodeValue() == TR::BBStart)
                              break;

                           cursorTree = cursorTree->getPrevTreeTop();
                           }
                        }
                     else
                        {
                        signExtStore = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(node->getDataType()), 1, 1,
                                                             TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(storeSymRef->getSymbol()->getDataType()), 0, storeSymRef),
                                                             storeSymRef);

                        TR::TreeTop *initTree = TR::TreeTop::create(comp(), signExtStore, 0, 0);

                        TR::TreeTop *placeHolderTree = loopInvariantBlock->getLastRealTreeTop();
                        TR::Node *placeHolderNode = placeHolderTree->getNode();
                        if (placeHolderNode->getOpCode().isResolveOrNullCheck() ||
                           (placeHolderNode->getOpCodeValue() == TR::treetop))
                           placeHolderNode = placeHolderNode->getFirstChild();

                        TR::ILOpCode &placeHolderOpCode = placeHolderNode->getOpCode();

                        if (!placeHolderOpCode.isBranch() &&
                            !placeHolderOpCode.isJumpWithMultipleTargets() &&
                            !placeHolderOpCode.isReturn() &&
                            (placeHolderOpCode.getOpCodeValue() != TR::athrow))
                            placeHolderTree = loopInvariantBlock->getExit();

                        TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();
                        prevTree->join(initTree);
                        initTree->join(placeHolderTree);
                        signExtStore->setNeedsSignExtension(true);
                        }

                     TR_ASSERT(signExtStore, "Store for sign extension must be found or created in loop pre-header\n");

                     if (trace())
                        {
                        traceMsg(comp(), "Skip %p sign ext store %p load %p store %p\n", node, signExtStore, loadNode, storeNode);
                        traceMsg(comp(), "loop invariant block_%d loop %p\n", loopInvariantBlock->getNumber(), containingLoop);
                        }

                     node->setSkipSignExtension(true);
                     int32_t childNum = -1;
                     if (parent)
                        {
                        int32_t i = 0;
                        while (i < parent->getNumChildren())
                           {
                           if (parent->getChild(i) == node)
                              {
                              childNum = i;
                              break;
                              }
                           i++;
                           }
                        }

                     TR_NodeTriple *nodeTriple = new (comp()->trHeapMemory()) TR_NodeTriple(parent, signExtStore, (loadNode ? parent : storeNode), childNum);                                    _nodeTriplesToBeChecked.add(nodeTriple);
                     _candidatesNeedingSignExtension->set(storeSymRef->getReferenceNumber());
                     _candidatesSignExtendedInThisLoop->set(storeSymRef->getReferenceNumber());
                     }
                  }
               }

            canSkip = false;
            }
         }

      origNode = node;
      if (0 && *currentArrayAccess &&
          canSkip)
         {
         TR_UseDefInfo *info = optimizer()->getUseDefInfo();
         if (TR::Compiler->target.is64Bit() && info &&
             (parent->getOpCodeValue() == TR::i2l) /* && node->isNonNegative() */ &&
             (*currentArrayAccess == parent) &&
             enableSignExtGRA)
            {
             TR_BlockStructure *blockStructure = block->getStructureOf();
            node->setSkipSignExtension(true);
            if (!node->getOpCode().isLoadVarDirect())
               node = node->getFirstChild();

            _nodesToBeChecked.add(origNode);

            _candidatesNeedingSignExtension->set(node->getSymbolReference()->getReferenceNumber());
            //_candidatesSignExtendedInThisLoop->set(node->getSymbolReference()->getReferenceNumber());

            int32_t useIndex = node->getUseDefIndex();
            TR_UseDefInfo::BitVector defs(comp()->allocator());
            if (info->getUseDef(defs, useIndex))
               {
               TR_UseDefInfo::BitVector::Cursor cursor(defs);
               for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                  {
                  int32_t defIndex = info->getFirstDefIndex() + (int32_t) cursor;
                  if (defIndex < info->getFirstRealDefIndex()) // def is  unseen--can't mark this node as skippable
                     {
                     origNode->setSkipSignExtension(false);
                     break;
                     }
                  TR::Node *defNode = info->getNode(defIndex);
                  if (defNode->getOpCode().isStore())
                     {
                     TR::Symbol *sym = defNode->getSymbolReference()->getSymbol();
                     TR_ASSERT((sym == node->getSymbolReference()->getSymbol()), "Symbols must match between a use and a def\n");

                     if (sym->isAuto())
                        {
                        bool seenLoad = false;
                        bool dependentStore = isDependentStore(defNode, defs, node->getSymbolReference(), &seenLoad);
                        if (dependentStore &&
                            defNode->getFirstChild()->isNonNegative() &&
                            seenLoad)
                           {
                           //printf("Skip sign extension at def node %p in %s\n", defNode, comp->getCurrentMethod()->signature());
                           TR::TreeTop *defTree = info->getTreeTop(defIndex);
                           TR::Block *defBlock = defTree->getEnclosingBlock();
                           TR_BlockStructure *defBlockStructure = defBlock->getStructureOf();
                           if (comp()->getFlowGraph()->getStructure() &&
                               blockStructure && defBlockStructure &&
                               (blockStructure->getContainingLoop() == defBlockStructure->getContainingLoop()))
                              continue;
                           }
                           defNode->setNeedsSignExtension(true);
                        }
                      }
                  }
               }
            }
         }

      if (TR::Compiler->target.is64Bit() &&
          parent &&
          (parent->getOpCodeValue() == TR::i2l) &&
          ((*currentArrayAccess != parent) &&
           (grandParent->getOpCodeValue() != TR::l2i)) &&
          node->skipSignExtension())
         {
         _rejectedSignExtNodes.add(node);
         node->setSkipSignExtension(false);
         }
      }

   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (enableNewSignExtGRA)
      node = origNode;

   if (!enableNewSignExtGRA && /* !hasCatchBlock && */ node->getOpCode().isLoadVarDirect() &&
         (node->getSymbolReference()->getSymbol()->isAuto() ))
      {
      TR_UseDefInfo *info = optimizer()->getUseDefInfo();
      if (TR::Compiler->target.is64Bit() && info &&
          (parent->getOpCodeValue() == TR::i2l) && node->isNonNegative() && enableSignExtGRA)
         {
         node->setSkipSignExtension(true);
         _candidatesNeedingSignExtension->set(node->getSymbolReference()->getReferenceNumber());

         TR_BlockStructure *blockStructure = block->getStructureOf();
         //printf("Skip sign extension at node %p in %s\n", node, comp->getCurrentMethod()->signature());
         int32_t useIndex = node->getUseDefIndex();
         TR_UseDefInfo::BitVector defs(comp()->allocator());
         if (info->getUseDef(defs, useIndex))
            {
            TR_UseDefInfo::BitVector::Cursor cursor(defs);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t defIndex = info->getFirstDefIndex() + (int32_t) cursor;
               if (defIndex < info->getFirstRealDefIndex()) // def is  unseen--can't mark this node as skippable
                  {
                  node->setSkipSignExtension(false);
                  //printf("Parm def, so cannot skip sign extension at node %p in %s\n",
                  //         node, comp->getCurrentMethod()->signature());
                  break;
                  }
               TR::Node *defNode = info->getNode(defIndex);
               if (defNode->getOpCode().isStore())
                  {
                  TR::Symbol *sym = defNode->getSymbolReference()->getSymbol();
                  // TR_ASSERT((sym == node->getSymbolReference()->getSymbol()), "Symbols must match between a use and a def\n");
                  // Comment out the above assume because:
                  //   - The assume came from JAVA where there could not be aliasing with autos
                  //   - If there is an alias causing a different sym then we will not have a candidate anyways so it does not matter
                  //   - avoid extra computation if symbols do not match by adding extra condition below.
                  if (sym == node->getSymbolReference()->getSymbol() &&
                      (sym->isAuto() ))
                     {
                     bool seenLoad = false;
                     bool dependentStore = isDependentStore(defNode, defs, node->getSymbolReference(), &seenLoad);
                     if (dependentStore &&
                         defNode->getFirstChild()->isNonNegative() &&
                         seenLoad)
                        {
                        //printf("Skip sign extension at def node %p in %s\n", defNode, comp->getCurrentMethod()->signature());
                        TR::TreeTop *defTree = info->getTreeTop(defIndex);
                        TR::Block *defBlock = defTree->getEnclosingBlock();
                        TR_BlockStructure *defBlockStructure = defBlock->getStructureOf();
                        if (comp()->getFlowGraph()->getStructure() &&
                            blockStructure && defBlockStructure &&
                            (blockStructure->getContainingLoop() == defBlockStructure->getContainingLoop()))
                           continue;
                        }

                     if (!defNode->getFirstChild()->isNonNegative())
                        {
                        node->setSkipSignExtension(false);
                        break;
                        }
                     else
                        defNode->setNeedsSignExtension(true);
                     }
                  }
               }
            }
         }
      }

   if (node->getOpCode().isLoadVarDirect() || node->getOpCode().isStoreDirect())
      {
      TR::CFG *cfg = comp()->getFlowGraph();

      TR::SymbolReference *symRef = node->getSymbolReference();
      if ((symRef->getSymbol()->isAutoOrParm() ||
           (0) && symRef->getSymbol()->getDataType() != TR::NoType)
          && isSymRefAvailable(symRef, blocksInLoop))
         {
         TR_RegisterCandidate *rc = registerCandidates[symRef->getReferenceNumber()];
         if (!rc)
            {
            registerCandidates[symRef->getReferenceNumber()] = comp()->getGlobalRegisterCandidates()->findOrCreate(symRef);
            rc = registerCandidates[symRef->getReferenceNumber()];
            }

         if (!assignedAutosInCurrentLoop->get(symRef->getReferenceNumber()))
            {
            assignedAutosInCurrentLoop->set(symRef->getReferenceNumber());
            ListIterator<TR::Block> blocksIt(blocksInLoop);
            TR::Block *nextBlock;
            for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
               {
               if (!rc->hasBlock(nextBlock))
                  {
                  if (nextBlock != cfg->getStart())
                     rc->addBlock(nextBlock, 0);
                  }
               }
            }

         //dumpOptDetails(comp(), "Adding %d numLoadsStores to candidate %d in block_%d\n", executionFrequency, rc->getSymbolReference()->getReferenceNumber(), block->getNumber());

         if (block != cfg->getStart())
            {
            // We should not consider the cost of the copies for live-range splitting,
            // because this cost is meaningful only when one of operand (source or destination) receives a register.
            // Note: Although we need to re-calculate the cost when one of operand receives a register,
            // we do not do so currently.
            //
            if ((node->getOpCode().isStoreDirect() && isSplittingCopy(node)) ||
                (node->getOpCode().isLoadVarDirect() && parent && parent->getOpCode().isStoreDirect() && isSplittingCopy(parent)))
               {
               rc->addBlock(block, 0);
               }
            else
               {
               if (TR::Compiler->target.cpu.isZ() &&
                   rc->getSymbolReference()->getSymbol()->getDataType() == TR::Address &&
                   parent &&
                   (((parent->getOpCode().isStoreIndirect() ||
                     parent->getOpCode().isLoadIndirect()) &&
                     parent->getFirstChild() == node) ||
                    (parent->getOpCode().isAdd() &&
                     grandParent &&
                     (grandParent->getOpCode().isStoreIndirect() ||
                     grandParent->getOpCode().isLoadIndirect()))))
                  {
                  rc->addBlock(block, executionFrequency*10);
                  if (trace())
                      dumpOptDetails(comp(), "Increased weight of candidate #%d in block_%d to reduce AGI\n", rc->getSymbolReference()->getReferenceNumber(), block->getNumber());
                  }
               else
                  {
                  rc->addBlock(block, executionFrequency);
                  }
               }
            }

         if (((node->getReferenceCount() > 1) ||
              node->getOpCode().isStoreDirect()) &&
             symsThatShouldNotBeAssignedInCurrentLoop)
            symsThatShouldNotBeAssignedInCurrentLoop->reset(symRef->getReferenceNumber());

         if (symsThatShouldNotBeAssignedInCurrentLoop &&
             node->getOpCode().isLoadVarDirect())
            {
            if (parent->getNumChildren() == 2)
               {
               if (parent->getSecondChild()->getOpCode().isLoadConst())
                  symsThatShouldNotBeAssignedInCurrentLoop->reset(symRef->getReferenceNumber());
               else
                  {
                  TR::Node *otherChild;
                  if (parent->getFirstChild() == node)
                     otherChild = parent->getSecondChild();
                  else
                     otherChild = parent->getFirstChild();

                  if (otherChild->getOpCode().isLoadVarDirect() &&
                      (otherChild->getSymbolReference()->getSymbol()->isAutoOrParm())
                      )
                     findOrCreatePairedSymbols(symRef, otherChild->getSymbolReference());
                  }
               }
            }
         }
      }

   int32_t childNum;
   for (childNum=0;childNum<node->getNumChildren();childNum++)
      {
      TR::Node *oldArrayAccess = *currentArrayAccess;
      markAutosUsedIn(node->getChild(childNum), node, parent, currentArrayAccess, block, blocksInLoop, visitCount, executionFrequency, registerCandidates, assignedAutosInCurrentLoop, symsThatShouldNotBeAssignedInCurrentLoop, hasCatchBlock);
      *currentArrayAccess = oldArrayAccess;
      }

  TR::ILOpCode &opCode = node->getOpCode();
  TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

  if (symsThatShouldNotBeAssignedInCurrentLoop &&
       assignedAutosInCurrentLoop &&
       ((opCode.hasSymbolReference() &&
         opCode.isIndirect()) ||
        (opCode.isArrayLength()) ||
        !(opCode.isAdd() || opCode.isSub() || opCode.isMul() ||
          opCode.isBooleanCompare() || opCode.isNullCheck() || opCode.isBndCheck())))
      findSymsUsedInIndirectAccesses(node, symsThatShouldNotBeAssignedInCurrentLoop, assignedAutosInCurrentLoop, true);

   if (enableNewSignExtGRA && node->getOpCode().hasSymbolReference() &&
       (node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()))
      *currentArrayAccess = prevArrayAccess;

   }


void TR_GlobalRegisterAllocator::signExtendAllDefNodes(TR::Node *defNode, List<TR::Node> *defNodes)
   {
   LexicalTimer t("TR_GlobalRegisterAllocator::signExtendAllDefNodes", comp()->phaseTimer());

   defNodes->add(defNode);
   TR::Node *childOfDefNode = defNode->getFirstChild();
   if (((childOfDefNode->getOpCodeValue() == TR::iadd) || (childOfDefNode->getOpCodeValue() == TR::isub)) &&
       childOfDefNode->getFirstChild()->getOpCode().isLoadVarDirect() &&
       childOfDefNode->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
       childOfDefNode->getSecondChild()->getOpCode().isLoadConst() &&
      ((childOfDefNode->getSecondChild()->getInt() <= 32767) && (childOfDefNode->getSecondChild()->getInt() >= -32767)))
      {
      int32_t useIndex;
      if ((childOfDefNode->getOpCodeValue() == TR::iadd) || (childOfDefNode->getOpCodeValue() == TR::isub))
         {
         useIndex = childOfDefNode->getFirstChild()->getUseDefIndex();
         if (((childOfDefNode->getOpCodeValue() == TR::iadd) &&
              (childOfDefNode->getSecondChild()->getInt() < 0)) ||
             ((childOfDefNode->getOpCodeValue() == TR::isub) &&
              (childOfDefNode->getSecondChild()->getInt() > 0)))
            defNode->setNeedsSignExtension(true);
         }
      else
         {
         defNode->setNeedsSignExtension(true);
         useIndex = childOfDefNode->getUseDefIndex();
         }

      TR_UseDefInfo *info = optimizer()->getUseDefInfo();
      TR_UseDefInfo::BitVector defs(comp()->allocator());
      if (info->getUseDef(defs, useIndex))
         {
         TR_UseDefInfo::BitVector::Cursor cursor(defs);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t defIndex = info->getFirstDefIndex() + (int32_t) cursor;
            if (defIndex < info->getFirstRealDefIndex()) continue;
            TR::Node *nextDefNode = info->getNode(defIndex);
            if (nextDefNode->getOpCode().isStore() &&
                !defNodes->find(nextDefNode))
               signExtendAllDefNodes(nextDefNode, defNodes);
            }
         }
      }
   else
      defNode->setNeedsSignExtension(true);
   }


void
TR_GlobalRegisterAllocator::findSymsUsedInIndirectAccesses(TR::Node *node, TR_BitVector *symsThatShouldNotBeAssignedInCurrentLoop, TR_BitVector *assignedAutosInCurrentLoop, bool examineChildren)
   {
   if (symsThatShouldNotBeAssignedInCurrentLoop &&
       node->getOpCode().hasSymbolReference() &&
       (node->getSymbolReference()->getSymbol()->isAutoOrParm())
       )
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      symsThatShouldNotBeAssignedInCurrentLoop->reset(symRef->getReferenceNumber());
      }

   *_temp2 = *symsThatShouldNotBeAssignedInCurrentLoop;
   *_temp2 &= *assignedAutosInCurrentLoop;

   if (examineChildren && (node->getNumChildren() > 0) && !_temp2->isEmpty())
      {
      TR::Node *child = node->getFirstChild();
      if (child->getOpCode().isArrayRef())
         node = child;

      int32_t i;
      for (i = 0; i < node->getNumChildren(); ++i)
         findSymsUsedInIndirectAccesses(node->getChild(i), symsThatShouldNotBeAssignedInCurrentLoop, assignedAutosInCurrentLoop, false);
      }
   }


TR_PairedSymbols * TR_GlobalRegisterAllocator::findOrCreatePairedSymbols(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2)
   {
   TR_PairedSymbols *pairedSymbols = findPairedSymbols(symRef1, symRef2);
   if (!pairedSymbols)
      {
      pairedSymbols = new (trStackMemory()) TR_PairedSymbols(symRef1, symRef2);
      _pairedSymbols.add(pairedSymbols);
      }
   return pairedSymbols;
   }


TR_PairedSymbols * TR_GlobalRegisterAllocator::findPairedSymbols(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2)
   {
   ListIterator<TR_PairedSymbols> pairs(&_pairedSymbols);
   TR_PairedSymbols *p;
   for (p = pairs.getFirst(); p; p = pairs.getNext())
      {
      if (((p->_symRef1 == symRef1) &&
           (p->_symRef2 == symRef2)) ||
          ((p->_symRef1 == symRef2) &&
           (p->_symRef2 == symRef1)))
         return p;
      }
   return NULL;
   }


/**
 * @return A block that is a good choice to append new blocks to
 * It is guaranteed that this block does not fall through to the next block,
 * so adding a block after this block is okay.
 */
TR::Block *
TR_GlobalRegisterAllocator::getAppendBlock(TR::Block *block)
   {
   if (_appendBlock)
      return _appendBlock;

   TR::Block *inBlock = block;

   // Find the last block that is not a cold block, and does not fall into the next block, or
   // if none such, the first cold block that does not fall into the next
   //
   TR::Block *prevBlock = block->getPrevBlock();;
   if (block->isCold())
      return _appendBlock = comp()->getMethodSymbol()->getLastTreeTop()->getNode()->getBlock();

   for (; block != NULL; prevBlock = block, block = block->getNextBlock())
      {
      if (block != inBlock &&
          prevBlock &&
          !prevBlock->hasSuccessor(block))
         return _appendBlock = prevBlock;
      }

   // Return the last block of the method
   //
   return _appendBlock = prevBlock;
   }

void
TR_GlobalRegisterAllocator::appendGotoBlock(TR::Block *gotoBlock, TR::Block *curBlock)
   {
   TR::Block *appendBlock = getAppendBlock(curBlock);
   TR::Block *nextBlock   = appendBlock->getNextBlock();

   appendBlock->getExit()->join(gotoBlock->getEntry());
   if (nextBlock)
      gotoBlock->getExit()->join(nextBlock->getEntry());
   _appendBlock = gotoBlock;
   }


StoresInBlockInfo *TR_GlobalRegisterAllocator::findRegInStoreInfo(TR_GlobalRegister *gr)
   {
   StoresInBlockInfo *nextStoreInfo;
   for (nextStoreInfo = _storesInBlockInfo.getFirst(); nextStoreInfo; nextStoreInfo=nextStoreInfo->getNext())
      {
      if (nextStoreInfo->_gr == gr)
            return nextStoreInfo;
      }
   return NULL;
   }

void
TR_GlobalRegisterAllocator::findLoopsAndCreateAutosForSignExt(TR_StructureSubGraphNode *structureNode, vcount_t visitCount)
   {
   TR_Structure *structure;
   if (structureNode)
      structure = structureNode->getStructure();
   else
      structure = comp()->getFlowGraph()->getStructure();

   if (structure->asRegion())
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *subNode;
      TR_Structure             *subStruct = NULL;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         subStruct = subNode->getStructure();
         findLoopsAndCreateAutosForSignExt(subNode, visitCount);
         }

      if (!regionStructure->isAcyclic() && structureNode)
         {
         TR_ScratchList<TR::Block> blocksInLoop(trMemory());
         regionStructure->getBlocks(&blocksInLoop);

         visitCount = comp()->incVisitCount();
         _storeSymRef = NULL;

         ListIterator<TR::Block> blocksIt(&blocksInLoop);
         TR::Block *nextBlock;
         for (nextBlock = blocksIt.getFirst(); nextBlock; nextBlock=blocksIt.getNext())
            {
            if (nextBlock->getVisitCount() != visitCount)
               {
               nextBlock->setVisitCount(visitCount);
               int32_t executionFrequency = 1;
               if (nextBlock->getStructureOf())
                  optimizer()->getStaticFrequency(nextBlock, &executionFrequency);

               TR::TreeTop *currentTree = nextBlock->getEntry();
               TR::TreeTop *exitTree = nextBlock->getExit();
               while (currentTree != exitTree)
                  {
                  TR::Node *currentNode = currentTree->getNode();
                  TR::Node *arrayAccess = NULL;
                  createStoresForSignExt(currentNode, NULL, NULL, currentTree, &arrayAccess, nextBlock, &blocksInLoop, visitCount, false);
                  currentTree = currentTree->getNextRealTreeTop();
                  }
               }
            }
         }
      }
   }

void
TR_GlobalRegisterAllocator::createStoresForSignExt(
   TR::Node *node, TR::Node *parent, TR::Node *grandParent, TR::TreeTop *treetop, TR::Node **currentArrayAccess, TR::Block * block, List<TR::Block> *blocksInLoop,
   vcount_t visitCount, bool hasCatchBlock)
   {
   LexicalTimer t("TR_GlobalRegisterAllocator::createStoresForSignExt", comp()->phaseTimer());

   bool enableSignExtGRA = false; // enable for other platforms later
   bool enableNewSignExtGRA = false;

   static char *doit = feGetEnv("TR_SIGNEXTGRA");
   if (NULL != doit)
      enableSignExtGRA = true;

   if (TR::Compiler->target.cpu.isZ())
      {
      enableSignExtGRA = true;
      static char *doit2 = feGetEnv("TR_NSIGNEXTGRA");
      if (NULL != doit2)
         enableSignExtGRA = false;

      static char *doit3 = feGetEnv("TR_NEWSIGNEXTGRA");
      if (NULL != doit3)
         enableNewSignExtGRA = true;
      }

   TR::Node *prevArrayAccess = NULL;
   if (enableNewSignExtGRA)
      {
      prevArrayAccess = *currentArrayAccess;

      if (node->getOpCode().hasSymbolReference() &&
          (node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()))
         {
         TR::Node *child = node->getFirstChild();
         if (child->getOpCodeValue() == TR::aladd)
            {
            TR::Node *offsetChild = child->getSecondChild();
            if (((offsetChild->getOpCodeValue() == TR::ladd) || (offsetChild->getOpCodeValue() == TR::lsub)) &&
                (offsetChild->getSecondChild()->getOpCodeValue() == TR::lconst))
               offsetChild = offsetChild->getFirstChild();

            int32_t sizeOfElem;
            if (node->getDataType() == TR::Address)
               sizeOfElem = TR::Compiler->om.sizeofReferenceField();
            else
               sizeOfElem = node->getOpCode().getSize();

            if ((offsetChild->getOpCodeValue() == TR::lmul) &&
                (offsetChild->getSecondChild()->getOpCodeValue() == TR::lconst) &&
                (sizeOfElem == offsetChild->getSecondChild()->getLongInt()))
               {
               TR::Node *longIndexNode = offsetChild->getFirstChild();
               if (longIndexNode->getOpCodeValue() == TR::i2l)
                  *currentArrayAccess = longIndexNode;
               }
            else if ((offsetChild->getOpCodeValue() == TR::i2l) &&
                     (node->getOpCode().getSize() == 1))
               {
               *currentArrayAccess = offsetChild;
               }
            }
         }

      bool needStoreAndItExists = false;

      if (!needStoreAndItExists &&
          ((node->getOpCodeValue() == TR::iadd) || (node->getOpCodeValue() == TR::isub) ||
           (node->getOpCodeValue() == TR::iand) || (node->getOpCodeValue() == TR::iloadi) || (node->getOpCodeValue() == TR::ixor) ||
           ((node->getOpCodeValue() == TR::iload) && !node->getSymbolReference()->getSymbol()->isAutoOrParm())
           ))
         {
         TR::Node *storeNode = NULL;
         if (*currentArrayAccess)
            {
            if (TR::Compiler->target.is64Bit() &&
                (parent->getOpCodeValue() == TR::i2l) /* && node->isNonNegative() */ &&
                (*currentArrayAccess == parent) &&
                !_rejectedSignExtNodes.find(node))
               {
               TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
               TR::TreeTop *cursorTree = _candidates->_startOfExtendedBBForBB[block->getNumber()]->getEntry()->getNextTreeTop();
               while (cursorTree)
                  {
                  TR::Node *cursorNode = cursorTree->getNode();
                  if (cursorNode->getOpCode().isStoreDirect() &&
                      cursorNode->getSymbolReference()->getSymbol()->isAuto() &&
                      (cursorNode->getFirstChild() == node))
                     {
                     storeNode = cursorNode;
                     needStoreAndItExists = true;
                     break;
                     }

                   if (cursorNode->getOpCodeValue() == TR::BBStart)
                      {
                      if (!cursorNode->getBlock()->isExtensionOfPreviousBlock())
                         break;
                      }

                   cursorTree = cursorTree->getNextTreeTop();
                   }

               if (!needStoreAndItExists)
                  {
                  TR_BlockStructure *blockStructure = block->getStructureOf();
                  if (comp()->getFlowGraph()->getStructure() &&
                      blockStructure)
                     {
                     TR_Structure *containingLoop = blockStructure->getContainingLoop();

                     if (containingLoop)
                        {
                        TR::Block *loopInvariantBlock = NULL;
                        TR_RegionStructure *parentStructure = containingLoop->getParent()->asRegion();
                        TR_StructureSubGraphNode *subNode = NULL;
                        TR_RegionStructure::Cursor si(*parentStructure);
                        for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
                           {
                           if (subNode->getNumber() == containingLoop->getNumber())
                              break;
                           }

                        if ((subNode->getPredecessors().size() == 1))
                           {
                           TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
                           if (loopInvariantNode->getStructure()->asBlock() &&
                               loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
                              loopInvariantBlock = loopInvariantNode->getStructure()->asBlock()->getBlock();
                           }

                        if (loopInvariantBlock)
                           {
                           if (!_storeSymRef)
                              _storeSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), node->getDataType());
                           TR::Node *signExtStore = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(node->getDataType()), 1, 1,
                                                                   node,
                                                                   _storeSymRef);

                           TR::TreeTop *initTree = TR::TreeTop::create(comp(), signExtStore, 0, 0);
                           TR::TreeTop *prevTree = treetop->getPrevTreeTop();
                           prevTree->join(initTree);
                           initTree->join(treetop);
                           cg()->setLiveLocals(NULL);
                           }
                        }
                     }
                  }
               }
            }
         }

      if (TR::Compiler->target.is64Bit() &&
          parent &&
          (parent->getOpCodeValue() == TR::i2l) &&
          ((*currentArrayAccess != parent) &&
           (grandParent->getOpCodeValue() != TR::l2i)) &&
          node->skipSignExtension())
         {
         _rejectedSignExtNodes.add(node);
         }
      }

   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   int32_t childNum;
   for (childNum=0;childNum<node->getNumChildren();childNum++)
      createStoresForSignExt(node->getChild(childNum), node, parent, treetop, currentArrayAccess, block, blocksInLoop, visitCount, hasCatchBlock);

   if (enableNewSignExtGRA && node->getOpCode().hasSymbolReference() &&
       (node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()))
      *currentArrayAccess = prevArrayAccess;
   }

const char *
TR_GlobalRegisterAllocator::optDetailString() const throw()
   {
   return "O^O GLOBAL REGISTER ASSIGNER: ";
   }

TR_LiveRangeSplitter::TR_LiveRangeSplitter(TR::OptimizationManager *manager)
   : TR::Optimization(manager), _splitBlocks(manager->trMemory()), _changedSomething(false), _origSymRefs(NULL)
   {}

int32_t TR_LiveRangeSplitter::perform()
   {
   if (!comp()->getOptions()->getOption(TR_EnableRangeSplittingGRA))
      return 0;

   if (!cg()->prepareForGRA())
      return 0;

   if (comp()->hasLargeNumberOfLoops())
      {
      return 0;
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   splitLiveRanges();
   }

   return 1;
   }

/**
 * @todo FIXME: Do live range splitting before GRA pass so that the use/def, liveness etc. are
 * computed after we have created the new (split) variables
 */
void TR_LiveRangeSplitter::splitLiveRanges()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _changedSomething = false;

   TR_BitVector *liveVars = NULL;
   if (!cg()->getLiveLocals())
      {
      //printf("Computing liveness in %s\n", comp()->getCurrentMethod()->signature());
      int32_t numLocals = 0;
      TR::AutomaticSymbol *p;
      ListIterator<TR::AutomaticSymbol> locals(&comp()->getMethodSymbol()->getAutomaticList());
      for (p = locals.getFirst(); p != NULL; p = locals.getNext())
         ++numLocals;

      if (numLocals > 0 && (!trace() || performTransformation(comp(), "%s Performing liveness for Global Register Allocator\n", OPT_DETAILS)))
         {
         // Perform liveness analysis
         //
         TR_Liveness liveLocals(comp(), optimizer(), comp()->getFlowGraph()->getStructure());
         if (comp()->getVisitCount() > HIGH_VISIT_COUNT)
            {
            comp()->resetVisitCounts(1);
            }

         for (TR::CFGNode *cfgNode = comp()->getFlowGraph()->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
            {
            TR::Block *block     = toBlock(cfgNode);
            int32_t blockNum    = block->getNumber();
            if (blockNum > 0 && liveLocals._blockAnalysisInfo[blockNum])
               {
               liveVars = new (trHeapMemory()) TR_BitVector(numLocals, trMemory());
               *liveVars = *liveLocals._blockAnalysisInfo[blockNum];
               block->setLiveLocals(liveVars);
               }
            }

         // Make sure the code generator knows there are live locals for blocks, and
         // create a bit vector of the correct size for it.
         //
         liveVars = new (trHeapMemory()) TR_BitVector(numLocals, trMemory());
         cg()->setLiveLocals(liveVars);
         }
      }

   if (trace())
      comp()->dumpMethodTrees("Trees before live range splitter ", comp()->getMethodSymbol());

   //_origSymRefs = (TR::SymbolReference **)trMemory()->allocateStackMemory(comp()->getSymRefCount()*sizeof(TR::SymbolReference *));
   //memset(_origSymRefs, 0, comp()->getSymRefCount()*sizeof(TR::SymbolReference *));
   _origSymRefs = NULL;
   _origSymRefCount = 0;

   TR::CFG * cfg = comp()->getFlowGraph();
   splitLiveRanges(NULL);

   if (_changedSomething)
      cg()->setLiveLocals(NULL);

   }


static bool canSplit(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   if (symRef->getSymbol()->getDataType() == TR::Aggregate)
      return false;

   bool addr_taken = false;


   // FIXME : refine below condition some more to allow these kinds of autos/parms
   //
   if (symRef->getSymbol()->isAutoOrParm() &&
       !addr_taken &&
       !symRef->getSymbol()->isInternalPointer() &&
       !symRef->getSymbol()->dontEliminateStores(comp) &&
       !symRef->getSymbol()->isLocalObject() &&
       symRef->getUseonlyAliases().isZero(comp))
      return true;

   return false;
   }


void
TR_LiveRangeSplitter::splitLiveRanges(TR_StructureSubGraphNode *structureNode)
   {
   TR::SymbolReference **oldOrigSymRefs = _origSymRefs;
   int32_t oldSymRefCount = _origSymRefCount;

   TR_Structure *structure;
   if (structureNode)
      structure = structureNode->getStructure();
   else
      structure = comp()->getFlowGraph()->getStructure();

   if (structure->asRegion())
      {
      TR_RegionStructure *regionStructure = structure->asRegion();

      if (regionStructure->isNaturalLoop() &&
          structureNode)
         {
         TR::Block *loopInvariantBlock = NULL;

         if (!dontAssignInColdBlocks(comp()) || !regionStructure->getEntryBlock()->isCold())
            {
            if ((structureNode->getPredecessors().size() == 1))
               {
               TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(structureNode->getPredecessors().front()->getFrom());
               if (loopInvariantNode->getStructure()->asBlock() &&
                  loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
                  loopInvariantBlock = loopInvariantNode->getStructure()->asBlock()->getBlock();
               }
            }

         // FIXME : turn on loop canonicalization before GRA
         //

         if (loopInvariantBlock)
            {
            TR_ScratchList<TR::Block> blocksInLoop(trMemory());
            regionStructure->getBlocks(&blocksInLoop);

            _splitBlocks.deleteAll();
            _numberOfGPRs = 0;
            _numberOfFPRs = 0;

            SymRefCandidateMap *registerCandidates = new (trStackMemory()) SymRefCandidateMap((SymRefCandidateMapComparator()), SymRefCandidateMapAllocator(trMemory()->currentStackRegion()));

            TR_BitVector *replacedAutosInCurrentLoop = new (trStackMemory()) TR_BitVector(comp()->getSymRefCount(), trMemory(), stackAlloc);
            TR_BitVector *autosThatCannotBeReplacedInCurrentLoop = new (trStackMemory()) TR_BitVector(comp()->getSymRefCount(), trMemory(), stackAlloc);
            _storedSymRefs = new (trStackMemory()) TR_BitVector(comp()->getSymRefCount(), trMemory(), stackAlloc);

            TR_SymRefCandidatePair **correspondingSymRefs = (TR_SymRefCandidatePair **)trMemory()->allocateStackMemory(comp()->getSymRefCount()*sizeof(TR_SymRefCandidatePair *));
            memset(correspondingSymRefs, 0, comp()->getSymRefCount()*sizeof(TR_SymRefCandidatePair *));

            int32_t symRefCount = comp()->getSymRefCount();

            TR_ScratchList<TR::Block> loopExitBlocks(trMemory());
            regionStructure->collectExitBlocks(&loopExitBlocks);

            TR_BitVector *exitsFromCurrentLoop = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
            for (auto succ = structureNode->getSuccessors().begin(); succ != structureNode->getSuccessors().end(); ++succ)
               exitsFromCurrentLoop->set((*succ)->getTo()->getNumber());

            TR_ScratchList<TR::Block> exitBlocks(trMemory());
            ListIterator<TR::Block> blocksIt(&loopExitBlocks);
            TR::Block *nextBlock;
            for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
               {
               for (auto succ = nextBlock->getSuccessors().begin(); succ != nextBlock->getSuccessors().end(); ++succ)
                  {
                  if (exitsFromCurrentLoop->get((*succ)->getTo()->getNumber()))
                     exitBlocks.add((*succ)->getTo()->asBlock());
                  }
               }

            vcount_t visitCount = comp()->incVisitCount();
            blocksIt.set(&blocksInLoop);
            for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
               {
               if (nextBlock->getVisitCount() != visitCount)
                  {
                  nextBlock->setVisitCount(visitCount);
                  int32_t executionFrequency = 1;
                  if (nextBlock->getStructureOf())
                     optimizer()->getStaticFrequency(nextBlock, &executionFrequency);

                  TR::TreeTop *currentTree = nextBlock->getEntry();
                  TR::TreeTop *exitTree = nextBlock->getExit();
                  while (currentTree != exitTree)
                     {
                     TR::Node *currentNode = currentTree->getNode();
                     replaceAutosUsedIn(currentTree, currentNode, NULL, nextBlock, &blocksInLoop, &exitBlocks, visitCount, executionFrequency, *registerCandidates, correspondingSymRefs, replacedAutosInCurrentLoop, autosThatCannotBeReplacedInCurrentLoop, structureNode, loopInvariantBlock);
                     currentTree = currentTree->getNextRealTreeTop();
                     }
                  }
               }

            TR::SymbolReference **prevOrigSymRefs = oldOrigSymRefs;
            int32_t prevSymRefCount = oldSymRefCount;
            ///if (trace())
            traceMsg(comp(), "Trying to split unused locals in loop %d with starting sym ref count %d\n", structureNode->getNumber(), symRefCount);

            int32_t i = 0;
            while ((i < symRefCount) && (prevSymRefCount == 0))
               {
               TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(i);

               TR::SymbolReference *origSymRef = NULL; //prevOrigSymRefs[i];
               if (i < prevSymRefCount)
                  origSymRef = prevOrigSymRefs[i];

               if (!origSymRef)
                  origSymRef = symRef;

               if (origSymRef && trace())
                  traceMsg(comp(), "orig sym %p (#%d)\n", origSymRef->getSymbol(), origSymRef->getReferenceNumber());

               bool candidateIsLiveOnExit = false;
               if (origSymRef &&
                   origSymRef->getSymbol()->isAutoOrParm() &&
                   comp()->cg()->considerTypeForGRA(origSymRef) &&
                   canSplit(symRef, comp()) &&
                   !replacedAutosInCurrentLoop->get(i) &&
                   !autosThatCannotBeReplacedInCurrentLoop->get(i))
                  {
                  TR_RegisterCandidate *rc = (*registerCandidates)[origSymRef->getReferenceNumber()];
                  if (!rc)
                     {
                     rc = comp()->getGlobalRegisterCandidates()->find(origSymRef);
                     (*registerCandidates)[origSymRef->getReferenceNumber()] = rc;
                     }

                  if (trace() && origSymRef->getSymbol()->getAutoSymbol())
                     traceMsg(comp(), "2 orig sym %p %d live index %d\n", origSymRef->getSymbol(), origSymRef->getReferenceNumber(), origSymRef->getSymbol()->getAutoSymbol()->getLiveLocalIndex());
                  //if (rc)
                     {
                     ListIterator<TR::Block> exitBlocksIt(&exitBlocks);
                     TR::Block *exitBlock;
                     for (exitBlock = exitBlocksIt.getCurrent(); exitBlock; exitBlock=exitBlocksIt.getNext())
                        {
                        TR_BitVector * liveLocals = exitBlock->getLiveLocals();
                        if (liveLocals &&
                            origSymRef->getSymbol()->getAutoSymbol() &&
                            liveLocals->get(origSymRef->getSymbol()->getAutoSymbol()->getLiveLocalIndex()))
                        //if (rc->symbolIsLive(exitBlock))
                           {
                           candidateIsLiveOnExit = true;
                           break;
                           }
                        }
                     }
                  }

               if (candidateIsLiveOnExit)
                  {
                  TR::DataType dt = symRef->getSymbol()->getDataType();
                  bool isFloat = (dt == TR::Float
                                  || dt == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                                  || dt == TR::DecimalFloat
                                  || dt == TR::DecimalDouble
                                  || dt == TR::DecimalLongDouble
#endif
                                  );
                  int32_t numRegsForCandidate = 1;
                  if ((symRef->getSymbol()->getType().isInt64() && TR::Compiler->target.is32Bit())
#ifdef J9_PROJECT_SPECIFIC
                      || symRef->getSymbol()->getType().isLongDouble()
#endif
                      )
                     numRegsForCandidate = 2;

                  if (((!isFloat && (_numberOfGPRs + numRegsForCandidate) <=  comp()->cg()->getNumberOfGlobalGPRs() - 2) ||
                       (isFloat && (_numberOfFPRs + numRegsForCandidate) <=  comp()->cg()->getNumberOfGlobalFPRs() - 2)) &&
                      performTransformation(comp(), "%s replace auto #%d in loop %d (%p)\n", OPT_DETAILS, symRef->getReferenceNumber(), structureNode->getNumber(), structureNode))
                     {
                     _changedSomething = true;

                     //printf("Splitting live vars in %s\n", comp()->signature()); fflush(stdout);

                     if (isFloat)
                        _numberOfFPRs = _numberOfFPRs + numRegsForCandidate;
                     else
                        _numberOfGPRs = _numberOfGPRs + numRegsForCandidate;

                     TR_SymRefCandidatePair *correspondingSymRefCandidate = splitAndFixPreHeader(symRef, correspondingSymRefs, loopInvariantBlock, loopInvariantBlock->getEntry()->getNode());
                     TR::SymbolReference *correspondingSymRef = correspondingSymRefCandidate->_symRef;
                     //////printf("Splitting sym ref %d with new sym ref %d in method %s\n", symRef->getReferenceNumber(), correspondingSymRef->getReferenceNumber(), comp()->signature()); fflush(stdout);
                     fixExitsAfterSplit(symRef, correspondingSymRefCandidate, correspondingSymRefs, loopInvariantBlock, &blocksInLoop, loopInvariantBlock->getEntry()->getNode(), *registerCandidates, structureNode, replacedAutosInCurrentLoop, origSymRef);
                     }
                  }

               i++;
               }

            prevOrigSymRefs = _origSymRefs;
            prevSymRefCount = _origSymRefCount;
            _origSymRefCount = comp()->getSymRefCount(); // _origSymRefCount was always zero becasue there was no definition
            _origSymRefs = (TR::SymbolReference **)trMemory()->allocateStackMemory(comp()->getSymRefCount()*sizeof(TR::SymbolReference *));
            memset(_origSymRefs, 0, comp()->getSymRefCount()*sizeof(TR::SymbolReference *));
            if (prevOrigSymRefs)
               {
               int32_t i = 0;
               while (i < prevSymRefCount)
                  {
                  _origSymRefs[i] = prevOrigSymRefs[i];
                  i++;
                  }
               }

            i = 0;
            while (i < symRefCount)
               {
               if (correspondingSymRefs[i])
                  _origSymRefs[correspondingSymRefs[i]->_symRef->getReferenceNumber()] =
                                                        _origSymRefs[i] ? _origSymRefs[i] : comp()->getSymRefTab()->getSymRef(i);
               i++;
               }
            }
         else
            traceMsg(comp(), " loop %d (%p) is skipped because loop pre-header was not found \n", regionStructure->getNumber(), regionStructure);
         }

      TR_StructureSubGraphNode *subNode;
      TR_Structure             *subStruct = NULL;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         subStruct = subNode->getStructure();
         splitLiveRanges(subNode);
         }
      }

   _origSymRefs = oldOrigSymRefs;
   _origSymRefCount = oldSymRefCount;
   }

void
TR_LiveRangeSplitter::replaceAutosUsedIn(
   TR::TreeTop *currentTree, TR::Node *node, TR::Node *parent, TR::Block * block, List<TR::Block> *blocksInLoop, List<TR::Block> *exitBlocks,
   vcount_t visitCount, int32_t executionFrequency,
   SymRefCandidateMap &registerCandidates, TR_SymRefCandidatePair **correspondingSymRefs, TR_BitVector *replacedAutosInCurrentLoop, TR_BitVector *autosThatCannotBeReplacedInCurrentLoop,
   TR_StructureSubGraphNode *loop, TR::Block *loopInvariantBlock)
   {

   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isLoadVarDirect() || (node->getOpCodeValue() == TR::loadaddr) || node->getOpCode().isStoreDirect())
      {
      TR::CFG *cfg = comp()->getFlowGraph();

      TR::SymbolReference *symRef = node->getSymbolReference();
      comp()->setCurrentBlock(block);
      if (comp()->cg()->considerTypeForGRA(symRef) &&
          canSplit(symRef, comp()) &&
         !autosThatCannotBeReplacedInCurrentLoop->get(symRef->getReferenceNumber()))
         {
         TR::SymbolReference *correspondingSymRef = NULL;
         TR_SymRefCandidatePair *correspondingSymRefCandidate = correspondingSymRefs[symRef->getReferenceNumber()];
         TR::SymbolReference *origSymRef = symRef;
         if (!correspondingSymRefCandidate)
            {
            TR::DataType dt = symRef->getSymbol()->getDataType();
            bool isFloat = (dt == TR::Float
                            || dt == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                            || dt == TR::DecimalFloat
                            || dt == TR::DecimalDouble
                            || dt == TR::DecimalLongDouble
#endif
                            );
            int32_t numRegsForCandidate = 1;
            if (node->requiresRegisterPair(comp()))
               numRegsForCandidate = 2;

            bool candidateIsLiveOnExit = false;
            bool candidateIsLiveOnEntry = true;
            if (symRef->getReferenceNumber() < _origSymRefCount)
               {
               origSymRef = _origSymRefs[symRef->getReferenceNumber()];
               if (!origSymRef)
                  origSymRef = symRef;
               }

            if (origSymRef)
               {
               TR_RegisterCandidate *rc = registerCandidates[origSymRef->getReferenceNumber()];
               if (!rc)
                  {
                  rc = comp()->getGlobalRegisterCandidates()->find(origSymRef);
                  if (!rc)
                     {
                     static char *dontCreateCandidatesJustForSplitter = feGetEnv("TR_dontCreateCandidatesJustForSplitter");
                     if (  !dontCreateCandidatesJustForSplitter
                        && performTransformation(comp(), "%s live range splitter creating candidate for #%d in loop %d\n", OPT_DETAILS, origSymRef->getReferenceNumber(), loop->getNumber()))
                        {
                        rc = comp()->getGlobalRegisterCandidates()->findOrCreate(origSymRef);
                        rc->addAllBlocksInStructure(loop->getStructure(), comp(), trace()?"range splitter candidate":NULL);
                        }
                     }
                  registerCandidates[origSymRef->getReferenceNumber()] = rc;
                  }

               if (rc)
                  {
                  ListIterator<TR::Block> exitBlocksIt(exitBlocks);
                  TR::Block *exitBlock;
                  for (exitBlock = exitBlocksIt.getCurrent(); exitBlock; exitBlock=exitBlocksIt.getNext())
                     {
                     if (rc->symbolIsLive(exitBlock))
                        {
                        candidateIsLiveOnExit = true;
                        break;
                        }
                     }

                  if (!rc->symbolIsLive(loop->getStructure()->getEntryBlock()))
                     candidateIsLiveOnEntry = false;
                  }
               else
                  candidateIsLiveOnEntry = false;
               }

             // FIXME : enable condition below
             //
             if (((!isFloat && (_numberOfGPRs + numRegsForCandidate) <=  comp()->cg()->getNumberOfGlobalGPRs() - 2) ||
                  (isFloat && (_numberOfFPRs + numRegsForCandidate) <=  comp()->cg()->getNumberOfGlobalFPRs() - 2)) &&
                 /* candidateIsLiveOnExit && */  candidateIsLiveOnEntry &&
                performTransformation(comp(), "%s replace auto #%d in loop %d (%p)\n", OPT_DETAILS, symRef->getReferenceNumber(), loop->getNumber(), loop))
               {
               _changedSomething = true;

               //printf("Splitting live vars in %s\n", comp()->signature()); fflush(stdout);

               if (isFloat)
                  _numberOfFPRs = _numberOfFPRs + numRegsForCandidate;
               else
                  _numberOfGPRs = _numberOfGPRs + numRegsForCandidate;

               correspondingSymRefCandidate = splitAndFixPreHeader(symRef, correspondingSymRefs, loopInvariantBlock, node);
               correspondingSymRef = correspondingSymRefCandidate->_symRef;
               }
             else
               autosThatCannotBeReplacedInCurrentLoop->set(symRef->getReferenceNumber());
            }
         else
            correspondingSymRef = correspondingSymRefCandidate->_symRef;

         if (correspondingSymRef)
            {
            static const char *dontReplaceStores = feGetEnv("TR_disableReplacingOfStores");

            if (!node->getOpCode().isStoreDirect() ||
                (!dontReplaceStores ||
                 performTransformation(comp(), "%s --- going to replace auto #%d by auto #%d on %s node %p --- \n", OPT_DETAILS, symRef->getReferenceNumber(), correspondingSymRef->getReferenceNumber(), node->getOpCode().getName(), node)))
               {
         //traceMsg(comp(), " --- replaced auto #%d by auto #%d on %s node %p --- \n", symRef->getReferenceNumber(), correspondingSymRef->getReferenceNumber(), node->getOpCode().getName(), node);
               node->setSymbolReference(correspondingSymRef);
               }
            else if (node->getOpCode().isStoreDirect())
               {
               TR::Node *storeNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(correspondingSymRef->getSymbol()->getDataType()), 1, 1,
                                                   node->getFirstChild(),
                                                   correspondingSymRef);
               traceMsg(comp(), " --- created a store to auto #%d adjacent to existing store to auto #%d at %s node %p --- \n", correspondingSymRef->getReferenceNumber(), symRef->getReferenceNumber(), node->getOpCode().getName(), node);
               storeNode->setVisitCount(visitCount);
               TR::TreeTop *storeTree = TR::TreeTop::create(comp(), storeNode, 0, 0);
               TR::TreeTop *prevTree = currentTree->getPrevTreeTop();
               prevTree->join(storeTree);
               storeTree->join(currentTree);
               }

            fixExitsAfterSplit(symRef, correspondingSymRefCandidate, correspondingSymRefs, loopInvariantBlock, blocksInLoop, node, registerCandidates, loop, replacedAutosInCurrentLoop, origSymRef);
            }
         }
      }

   int32_t childNum;
   for (childNum=0;childNum<node->getNumChildren();childNum++)
      replaceAutosUsedIn(currentTree, node->getChild(childNum), node, block, blocksInLoop, exitBlocks, visitCount, executionFrequency, registerCandidates, correspondingSymRefs, replacedAutosInCurrentLoop, autosThatCannotBeReplacedInCurrentLoop, loop, loopInvariantBlock);
   }


TR_SymRefCandidatePair *
TR_LiveRangeSplitter::splitAndFixPreHeader(TR::SymbolReference *symRef, TR_SymRefCandidatePair **correspondingSymRefs, TR::Block *loopInvariantBlock, TR::Node *node)
   {

   TR::SymbolReference *correspondingSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), symRef->getSymbol()->getDataType(), symRef->getSymbol()->isPinningArrayPointer());

   if (symRef->getSymbol()->isNotCollected() &&
       correspondingSymRef->getSymbol()->isCollectedReference())
      correspondingSymRef->getSymbol()->setNotCollected();
   if (symRef->isFromLiteralPool())
      correspondingSymRef->setFromLiteralPool();

   //_cg->setLiveLocals(0);
   optimizer()->setUseDefInfo(NULL);
   optimizer()->setValueNumberInfo(NULL);
   optimizer()->setAliasSetsAreValid(false);
   requestOpt(OMR::globalCopyPropagation);
   requestOpt(OMR::globalDeadStoreGroup);

   TR_SymRefCandidatePair *correspondingSymRefCandidate = new (trStackMemory()) TR_SymRefCandidatePair(correspondingSymRef, 0);
   correspondingSymRefs[symRef->getReferenceNumber()] = correspondingSymRefCandidate;

   //lay down store in loop pre-header and in loop exits
   //
   dumpOptDetails(comp(), " place initialization of auto #%d by auto #%d in loop pre-header block_%d\n", correspondingSymRef->getReferenceNumber(), symRef->getReferenceNumber(), loopInvariantBlock->getNumber());
   appendStoreToBlock(correspondingSymRef, symRef, loopInvariantBlock, node);

   return correspondingSymRefCandidate;
   }


void
TR_LiveRangeSplitter::fixExitsAfterSplit(TR::SymbolReference *symRef, TR_SymRefCandidatePair *correspondingSymRefCandidate, TR_SymRefCandidatePair **correspondingSymRefs, TR::Block *loopInvariantBlock, List<TR::Block> *blocksInLoop, TR::Node *node, SymRefCandidateMap &registerCandidates, TR_StructureSubGraphNode *loop, TR_BitVector *replacedAutosInCurrentLoop, TR::SymbolReference *origSymRef)
   {

   TR::SymbolReference *correspondingSymRef = correspondingSymRefCandidate->_symRef;

   if (correspondingSymRef)
      {
      static const char *dontReplaceStores = feGetEnv("TR_disableReplacingOfStores");

      if (/* node->getOpCode().isStoreDirect() && */ !_storedSymRefs->get(symRef->getReferenceNumber()))
         {
         _storedSymRefs->set(symRef->getReferenceNumber());
         placeStoresInLoopExits(node, loop, blocksInLoop, symRef, correspondingSymRef);
         }

      if (!replacedAutosInCurrentLoop->get(symRef->getReferenceNumber()))
         {
         replacedAutosInCurrentLoop->set(symRef->getReferenceNumber());

         TR_RegisterCandidate *rc = registerCandidates[symRef->getReferenceNumber()];
         if (!rc)
            {
            rc = comp()->getGlobalRegisterCandidates()->find(symRef);
            registerCandidates[symRef->getReferenceNumber()] = rc;
            }

          TR_RegisterCandidate *correspondingRc = correspondingSymRefCandidate->_rc;

          if (rc && !correspondingRc)
            {
            correspondingRc = comp()->getGlobalRegisterCandidates()->findOrCreate(correspondingSymRef);
            correspondingRc->setSplitSymbolReference(origSymRef);
            correspondingRc->setRestoreSymbolReference(symRef);
            correspondingSymRefCandidate->_rc = correspondingRc;

            TR_BitVector *blocksInInnerLoop = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
            ListIterator<TR::Block> blocksIt(blocksInLoop);
            TR::Block *nextBlock;
            for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
               {
               if (rc->hasBlock(nextBlock))
                  {
                  int32_t numLoadsAndStores = rc->removeBlock(nextBlock);
                  correspondingRc->addBlock(nextBlock, numLoadsAndStores);
                  }
               blocksInInnerLoop->set(nextBlock->getNumber());
               }

            correspondingRc->addBlock(loopInvariantBlock, 1);

            TR_Structure *parentOfLoop = loop->getStructure()->getContainingLoop();
            if (parentOfLoop)
               {
               TR_ScratchList<TR::Block> blocksInParent(trMemory());
               parentOfLoop->getBlocks(&blocksInParent);
               ListIterator<TR::Block> blocksIt(&blocksInParent);
               TR::Block *nextBlock;
               for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
                  {
                  if (!blocksInInnerLoop->get(nextBlock->getNumber()))
                     {
         if (trace())
                        traceMsg(comp(), "Adding original candidate #%d in block_%d in outer loop %d (%p)\n", rc->getSymbolReference()->getReferenceNumber(), nextBlock->getNumber(), parentOfLoop->getNumber(), parentOfLoop);
                     rc->addBlock(nextBlock, 0);
                     }
                  }
               }
            }
         }
      }
   }

void
TR_LiveRangeSplitter::appendStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node)
   {
   TR::Node *initNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(storeSymRef->getSymbol()->getDataType()), 1, 1,
                                       TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(loadSymRef->getSymbol()->getDataType()), 0, loadSymRef),
                                       storeSymRef);

   if (trace())
      dumpOptDetails(comp(), "creating store node %p\n", initNode);

   TR::TreeTop *initTree = TR::TreeTop::create(comp(), initNode, 0, 0);


   TR::TreeTop *placeHolderTree = block->getLastRealTreeTop();
   TR::Node *placeHolderNode = placeHolderTree->getNode();
   if (placeHolderNode->getOpCode().isResolveOrNullCheck() ||
       (placeHolderNode->getOpCodeValue() == TR::treetop))
      placeHolderNode = placeHolderNode->getFirstChild();

   TR::ILOpCode &placeHolderOpCode = placeHolderNode->getOpCode();

   if (!placeHolderOpCode.isBranch() &&
       !placeHolderOpCode.isJumpWithMultipleTargets() &&
       !placeHolderOpCode.isReturn() &&
       (placeHolderOpCode.getOpCodeValue() != TR::athrow))
      placeHolderTree = block->getExit();

   TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();

   TR::TreeTop *placeHolderTreeForLoad = prevTree;

   while (placeHolderTreeForLoad &&
          placeHolderTreeForLoad->getNode()->getOpCode().isStore())
      {
      if ((placeHolderTreeForLoad->getNode()->getSymbolReference() == loadSymRef) ||
          (placeHolderTreeForLoad->getNode()->getSymbolReference()->sharesSymbol() &&
           placeHolderTreeForLoad->getNode()->getSymbolReference()->getUseDefAliases().contains(loadSymRef, comp())))
         break;
      placeHolderTreeForLoad = placeHolderTreeForLoad->getPrevTreeTop();
      }

   prevTree->join(initTree);
   initTree->join(placeHolderTree);

   if (placeHolderTreeForLoad != prevTree)
      {
      TR::Node *ttNode = TR::Node::create(TR::treetop, 1, initNode->getFirstChild());
      if (trace())
         dumpOptDetails(comp(), "creating treetop node %p\n", ttNode);
      TR::TreeTop *ttTree = TR::TreeTop::create(comp(), ttNode, 0, 0);
      TR::TreeTop *nextTree = placeHolderTreeForLoad->getNextTreeTop();
      placeHolderTreeForLoad->join(ttTree);
      ttTree->join(nextTree);
      }
   }

void
TR_LiveRangeSplitter::prependStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node)
   {
   for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCode().isStoreDirect() && node->getFirstChild()->getOpCode().isLoadVarDirect())
         {
         TR::SymbolReference *storeSymRef2 = node->getSymbolReference();
         TR::SymbolReference *loadSymRef2 = node->getFirstChild()->getSymbolReference();
         if (storeSymRef2 && loadSymRef2 && storeSymRef == storeSymRef2 && loadSymRef == loadSymRef2)
            return;
         }
      }

   TR::Node *initNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(storeSymRef->getSymbol()->getDataType()), 1, 1,
                                       TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(loadSymRef->getSymbol()->getDataType()), 0, loadSymRef),
                                       storeSymRef);

   if (trace())
      dumpOptDetails(comp(), "creating store node %p\n", initNode);

   TR::TreeTop *initTree = TR::TreeTop::create(comp(), initNode, 0, 0);

   TR::TreeTop *placeHolderTree = block->getEntry();
   TR::TreeTop *nextTree = placeHolderTree->getNextTreeTop();

   TR::TreeTop *placeHolderTreeForStore = nextTree;
   bool treetopsFound = false;
   while (placeHolderTreeForStore &&
          (placeHolderTreeForStore->getNode()->getOpCodeValue() == TR::treetop))
      {
      treetopsFound = true;
      TR::Node *ttChild = placeHolderTreeForStore->getNode()->getFirstChild();
      if (!ttChild->getOpCode().hasSymbolReference() ||
          !ttChild->getSymbolReference()->getSymbol()->isAutoOrParm() ||
          (ttChild->getSymbolReference() == storeSymRef) ||
          (ttChild->getSymbolReference()->sharesSymbol() && ttChild->getSymbolReference()->getUseDefAliases().contains(storeSymRef, comp())))
         break;
      placeHolderTreeForStore = placeHolderTreeForStore->getNextTreeTop();
      }

   if (!treetopsFound ||
       (placeHolderTreeForStore != nextTree))
      {
      TR::Node *ttNode = TR::Node::create(TR::treetop, 1, initNode->getFirstChild());
      if (trace())
         dumpOptDetails(comp(), "creating treetop node %p\n", ttNode);
      TR::TreeTop *ttTree = TR::TreeTop::create(comp(), ttNode, 0, 0);
      TR::TreeTop *prevTree = placeHolderTreeForStore->getPrevTreeTop();
      prevTree->join(ttTree);
      ttTree->join(placeHolderTreeForStore);
      placeHolderTree = ttTree;
      nextTree = placeHolderTreeForStore;
      }

   placeHolderTree->join(initTree);
   initTree->join(nextTree);
   }

void
TR_LiveRangeSplitter::placeStoresInLoopExits(TR::Node *node, TR_StructureSubGraphNode *loop, List<TR::Block> *blocksInLoop, TR::SymbolReference *orig, TR::SymbolReference *replacement)
   {
   //traceMsg(comp(), " place initialization of auto #%d by auto #%d in loop pre-header block_%d\n", replacement->getReferenceNumber(), orig->getReferenceNumber(), loopInvariantBlock->getNumber());
   //appendStoreToBlock(replacement, orig, loopInvariantBlock, node);

   TR_ScratchList<TR::Block> loopExitBlocks(trMemory());
   loop->getStructure()->collectExitBlocks(&loopExitBlocks);

   TR_BitVector *exitsFromCurrentLoop = new (trStackMemory()) TR_BitVector(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
   for (auto succ = loop->getSuccessors().begin(); succ != loop->getSuccessors().end(); ++succ)
      exitsFromCurrentLoop->set((*succ)->getTo()->getNumber());

   TR_ScratchList<TR::Block> exitBlocks(trMemory());
   ListIterator<TR::Block> blocksIt(&loopExitBlocks);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      for (auto succ = nextBlock->getSuccessors().begin(); succ != nextBlock->getSuccessors().end(); ++succ)
         {
         if (exitsFromCurrentLoop->get((*succ)->getTo()->getNumber()))
            exitBlocks.add((*succ)->getTo()->asBlock());
         }
      }

   ListIterator<TR::Block> exitBlocksIt(&exitBlocks);
   TR::Block *exitBlock;
   for (exitBlock = exitBlocksIt.getCurrent(); exitBlock; exitBlock=exitBlocksIt.getNext())
      {
      if ((exitBlock != comp()->getFlowGraph()->getEnd()) &&
          exitsFromCurrentLoop->get(exitBlock->getNumber()))
         {
         exitsFromCurrentLoop->reset(exitBlock->getNumber());
         if (!(exitBlock->getPredecessors().size() == 1))
            {
            TR_ScratchList<TR::CFGEdge> edgesFromLoop(trMemory());
            for (auto nextEdge = exitBlock->getPredecessors().begin(); nextEdge != exitBlock->getPredecessors().end(); ++nextEdge)
               {
               TR::Block *pred = (*nextEdge)->getFrom()->asBlock();
               if (blocksInLoop->find(pred))
                  edgesFromLoop.add(*nextEdge);
               }

            TR::CFGEdge *nextEdge;
            ListIterator<TR::CFGEdge> ei(&edgesFromLoop);
            for (nextEdge = ei.getFirst(); nextEdge; nextEdge = ei.getNext())
               {
               TR::Block *pred = nextEdge->getFrom()->asBlock();
               TR::Block *splitBlock = NULL;
               if (pred->getLastRealTreeTop()->getNode()->getOpCode().isBranch() &&
                   !_splitBlocks.isEmpty())
                  {
                  ListIterator<TR::Block> splitBlocksIt(&_splitBlocks);
                  TR::Block *nextSplitBlock = splitBlocksIt.getFirst();
                  for (nextSplitBlock = splitBlocksIt.getFirst(); nextSplitBlock != NULL; nextSplitBlock = splitBlocksIt.getNext())
                     {
                     bool exitBlockIsASplitBlock = false;
                     bool exitBlockIsFallThrough = false;
                     if (nextSplitBlock == exitBlock)
                        exitBlockIsASplitBlock = true;
                     else if (pred->getNextBlock() == exitBlock)
                        exitBlockIsFallThrough = true;

                     if (exitBlockIsASplitBlock ||
                         ((nextSplitBlock->getSuccessors().size() == 1) &&
                          nextSplitBlock->getSuccessors().front()->getTo() == exitBlock))
                        {
                        splitBlock = nextSplitBlock;
                        if (!exitBlockIsASplitBlock)
                           {
                           TR::CFG * cfg = comp()->getFlowGraph();
                           cfg->addEdge(pred, splitBlock);
                           cfg->removeEdge(pred, exitBlock);
                           if (pred->getLastRealTreeTop()->getNode()->getBranchDestination() == exitBlock->getEntry())
                              pred->getLastRealTreeTop()->getNode()->setBranchDestination(splitBlock->getEntry());
                           if (exitBlockIsFallThrough)
                              {
                              TR::TreeTop *prevTree = splitBlock->getEntry()->getPrevTreeTop();
                              TR::TreeTop *nextTree = splitBlock->getExit()->getNextTreeTop();
                              prevTree->join(nextTree);
                              pred->getExit()->join(splitBlock->getEntry());
                              splitBlock->getExit()->join(exitBlock->getEntry());
                              if (splitBlock->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto)
                                 {
                                 prevTree = splitBlock->getLastRealTreeTop()->getPrevTreeTop();
                                 prevTree->join(splitBlock->getExit());
                                 }
                              }
                           }

                        dumpOptDetails(comp(), " place store-back of auto #%d using auto #%d in (existing) loop exit block_%d between pred %d and exit block_%d\n", orig->getReferenceNumber(), replacement->getReferenceNumber(), splitBlock->getNumber(), pred->getNumber(), exitBlock->getNumber());
                        break;
                        }
                     }
                  }

               if (!splitBlock)
                  {
                  splitBlock = pred->splitEdge(pred, exitBlock, comp());
                  _splitBlocks.add(splitBlock);
                  dumpOptDetails(comp(), " place store-back of auto #%d using auto #%d in (new) loop exit block_%d between pred %d and exit block_%d\n", orig->getReferenceNumber(), replacement->getReferenceNumber(), splitBlock->getNumber(), pred->getNumber(), exitBlock->getNumber());
                  }

               prependStoreToBlock(orig, replacement, splitBlock, node);
               }
            }
         else
            {
            dumpOptDetails(comp(), " place store-back of auto #%d using auto #%d in loop exit block_%d \n", orig->getReferenceNumber(), replacement->getReferenceNumber(), exitBlock->getNumber());
            prependStoreToBlock(orig, replacement, exitBlock, node);
            }

         //rc->removeBlock(exitBlock);
         //rc->removeLoopExitBlock(exitBlock);
         }
      }

   // FIXME : enable when candidates with use-only aliases (i.e. those
   // that are used in code from catch blocks) are also considered for
   // live range splitting
   //
   /*
   succIt.set(&(loop->getExceptionSuccessors()));
   for (succ = succIt.getFirst(); succ != NULL; succ = succIt.getNext())
      {
      TR_Structure *exitStructure = succ->getTo()->asStructureSubGraphNode()->getStructure();
      TR::Block *exitBlock = NULL;
      if (exitStructure)
         exitBlock = exitStructure->getEntryBlock();
      if (exitBlock &&
          (exitBlock != comp()->getFlowGraph()->getEnd()))
         {
         //rc->removeBlock(exitBlock);
         //rc->removeLoopExitBlock(exitBlock);
         }
      }
   */
   }

const char *
TR_LiveRangeSplitter::optDetailString() const throw()
   {
   return "O^O LIVE RANGE SPLITTER: ";
   }
