/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "optimizer/LoopVersioner.hpp"

#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "cs2/llistof.h"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "infra/ILWalk.hpp"
#include "infra/String.hpp"
#include "optimizer/Dominators.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "optimizer/VPConstraint.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "runtime/J9ValueProfiler.hpp"
#include "runtime/J9Profiler.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#endif

#define DEFAULT_LOOP_LIMIT 100000000


#define MAX_SIZE_INCREASE_FACTOR 8
#define MAX_BLOCKS_THRESHOLD 200
#define MAX_NESTING_DEPTH 8
#define NORMAL_BLOCK_COUNT 100
#define NORMAL_NODE_COUNT 1000

//#define MIN_PROFILED_FREQUENCY (.75f)

#define OPT_DETAILS_LOOP_VERSIONER "O^O LOOP VERSIONER: "

const int HIGH_FREQ_THRESHOLD = 5000;

/*
void printNode(TR::Node *node, TR_BitVector &visitedNodes, int32_t indentation)
   {
   if (visitedNodes.isSet(node->getGlobalIndex()))
      {
      traceMsg(comp(), "\t\t\t%p %5d %*s ==>%s\n",  node,
              node->getLocalIndex(), indentation, " ",
              comp()->getDebug()->getName(node->getOpCode()));
      }
   else
      {
      traceMsg(comp(), "\t\t\t%p %5d %*s %s %s\n", node,
              node->getLocalIndex(),
              indentation, " ", comp()->getDebug()->getName(node->getOpCode()),
              node->getOpCode().hasSymbolReference() ?
              comp()->getDebug()->getName(node->getSymbolReference()): "");

      //comp()->getDebug()->printNodeInfo(comp()->getDebug()->getOutFile(), node);

      visitedNodes.set(node->getGlobalIndex());

      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         {
         printNode(node->getChild(i), visitedNodes, indentation+2);
         }
      }
   }
   */

TR_LoopSpecializer::TR_LoopSpecializer(TR::OptimizationManager *manager)
   : TR_LoopVersioner(manager, true)
   {}

const char *
TR_LoopSpecializer::optDetailString() const throw()
   {
   return "O^O LOOP SPECIALIZER: ";
   }

TR_LoopVersioner::TR_LoopVersioner(TR::OptimizationManager *manager, bool onlySpecialize, bool refineAliases)
   : TR_LoopTransformer(manager),
   _versionableInductionVariables(trMemory()), _specialVersionableInductionVariables(trMemory()),
   _derivedVersionableInductionVariables(trMemory()),
   ////_virtualGuardPairs(trMemory()),
   _guardedCalls(trMemory()),
     _refineLoopAliases(refineAliases),_addressingTooComplicated(false),
   _visitedNodes(comp()->getNodeCount() /*an estimate*/, trMemory(), heapAlloc, growable),
   _checksInDupHeader(trMemory()),
   _exitGotoTarget(NULL),
   _curLoop(NULL),
   _invalidateAliasSets(false)
   {
   _nonInlineGuardConditionalsWillNotBeEliminated = false;
   setOnlySpecializeLoops(onlySpecialize);
   }

int32_t TR_LoopVersioner::perform()
   {
   if (!comp()->mayHaveLoops() || optimizer()->optsThatCanCreateLoopsDisabled())
      return 0;

   _postDominators = NULL;

   return performWithoutDominators();
   }

int32_t TR_LoopVersioner::performWithDominators()
   {
   if (trace())
      traceMsg(comp(), "Building Control Dependencies\n");

   TR_PostDominators postDominators(comp());
   if (postDominators.isValid())
      {
      postDominators.findControlDependents();
      _postDominators = &postDominators;
      }
   else
      {
         printf("WARNING: method may have infinite loops\n");
      }
   auto result = performWithoutDominators();

   /* Local variable postDominators is about to go out of scope.
    * NULL this field to prevent illegal access to a stack
    * allocated object.
    */
   _postDominators = NULL;

   return result;
   }


bool TR_LoopVersioner::loopIsWorthVersioning(TR_RegionStructure *naturalLoop)
   {
   TR::Block *entryBlock = naturalLoop->getEntryBlock();

   if (entryBlock->isCold())
      {
      if (trace()) traceMsg(comp(), "loopIsWorthVersioning returning false for cold block\n");
      return false;
      }

   // if aggressive loop versioner flag is set then run at any optlevel, otherwise only at warm or less
   bool aggressive = comp()->getOption(TR_EnableAggressiveLoopVersioning);
   if (aggressive || comp()->getMethodHotness() <= warm)
      {
      if (naturalLoop->getParent())
         {
         TR_StructureSubGraphNode *loopNode = naturalLoop->getParent()->findNodeInHierarchy(naturalLoop->getNumber());
         if (loopNode && (loopNode->getPredecessors().size() == 1))
            {
            TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(loopNode->getPredecessors().front()->getFrom());
            if (loopInvariantNode->getStructure()->asBlock() &&
                loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
               {
               TR::Block *loopInvariantBlock = loopInvariantNode->getStructure()->asBlock()->getBlock();
               static const char *unimportantLoopCountStr = feGetEnv("TR_UnimportantLoopCountThreshold");
               int32_t unimportantLoopCountThreshold = unimportantLoopCountStr ? atoi(unimportantLoopCountStr) : 2;

               if ((unimportantLoopCountThreshold*loopInvariantBlock->getFrequency()) > entryBlock->getFrequency()) // loop does not even run twice
                  {
                  if (trace()) traceMsg(comp(), "loopIsWorthVersioning returning false based on LoopCountThreshold\n");
                  return false;
                  }
               }
            }
         }

      bool aggressive = comp()->getOption(TR_EnableAggressiveLoopVersioning);

      int32_t lvBlockFreqCutoff;
      static const char * b = feGetEnv("TR_LoopVersionerFreqCutoff");
      if (b)
         {
         lvBlockFreqCutoff = atoi(b);
         }
      else if (aggressive)
         {
         lvBlockFreqCutoff = 500;
         }
      else
         {
         lvBlockFreqCutoff = 5000;
         }

      if (trace()) traceMsg(comp(), "lvBlockFreqCutoff=%d\n", lvBlockFreqCutoff);

      if (entryBlock->getFrequency() < lvBlockFreqCutoff)
         {
         if (trace()) traceMsg(comp(), "loopIsWorthVersioning returning false based on lvBlockFreqCutoff\n");
         return false;
         }
      }

   if (trace()) traceMsg(comp(), "loopIsWorthVersioning returning true\n");
   return true;
   }



int32_t TR_LoopVersioner::performWithoutDominators()
   {
   _seenDefinedSymbolReferences = NULL;
   _additionInfo = NULL;
   _nullCheckReference = NULL;
   _conditionalTree = NULL;
   _duplicateConditionalTree = NULL;
   _currentNaturalLoop = NULL;

   _versionableInductionVariables.deleteAll();
   _specialVersionableInductionVariables.deleteAll();
   _derivedVersionableInductionVariables.deleteAll();
   ////_virtualGuardPairs.deleteAll();
   _virtualGuardInfo.setFirst(NULL);
   _containsGuard = false;
   _containsUnguardedCall = false;
   _counter = 0;
   _inNullCheckReference = 0,
   _containsCall = 0;
   _neitherLoopCold = 0;
   _loopConditionInvariant = 0;
   _guardedCalls.deleteAll();
   _loopTransferDone = false;

   _origNodeCount = comp()->getAccurateNodeCount();
   _origBlockCount = comp()->getFlowGraph()->getNodes().getSize();

   _doingVersioning = true;
   bool somethingChanged = false;

   // From this point on stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   initAdditionalDataStructures();

   TR_ScratchList<TR::CFGNode> newEmptyBlocks(trMemory());
   TR_ScratchList<TR::CFGNode> newEmptyExceptionBlocks(trMemory());
   _cfg = comp()->getFlowGraph();
   if (trace())
      {
      traceMsg(comp(), "Starting LoopVersioning\n");
      traceMsg(comp(), "\nCFG before loop versioning:\n");
      getDebug()->print(comp()->getOutFile(), _cfg);
      }

   // printTrees();

   TR_ScratchList<TR_Structure> whileLoops(trMemory());

   createWhileLoopsList(&whileLoops);

   ListIterator<TR_Structure> whileLoopsIt(&whileLoops);

   if (whileLoops.isEmpty())
     {
     return false;
     }

   TR_ScratchList<TR_Structure> innerWhileLoops(trMemory());
   TR_ScratchList<TR_Structure> *curLoops = NULL;
   int32_t curWeight = 10;
   curLoops = &whileLoops;
   bool needToPruneMore;
   if (!comp()->getOption(TR_ProcessHugeMethods) &&
       (whileLoops.getSize() > HIGH_LOOP_COUNT/2))
      {
      needToPruneMore = true;
      while (needToPruneMore)
         {
         TR_Structure *nextWhileLoop;
         ListElement<TR_Structure> *prevLoop = NULL;
         ListIterator<TR_Structure> whileLoopsIt2(&whileLoops);
         for (nextWhileLoop = whileLoopsIt2.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt2.getNext())
            {
            TR_RegionStructure *naturalLoop = nextWhileLoop->asRegion();
            TR::Block *entryBlock = naturalLoop->getEntryBlock();
            if (!entryBlock->isCold())
               {
               int32_t weight = 1;
               entryBlock->getStructureOf()->calculateFrequencyOfExecution(&weight);
               if (weight > curWeight)
                  prevLoop = innerWhileLoops.addAfter(naturalLoop, prevLoop);
               }
            }
         if (innerWhileLoops.getSize() > HIGH_LOOP_COUNT/2)
            {
            curWeight = curWeight*10;
            innerWhileLoops.deleteAll();
            }
         else
            needToPruneMore = false;
         }
      curLoops = &innerWhileLoops;
      }

   bool enableSpecialization = false;

   TR_Structure *nextWhileLoop;

   // _startOfHeader originally points to the last tree in the method;
   // this variable is used as a placeholder to keep track of where
   // trees can be moved (sometimes required for efficiency purposes
   // to minimize gotos in the critical 'hot' path between the loop body
   // and the new duplicate header that drives the new Do While loop.
   //
   _startOfHeader = comp()->getMethodSymbol()->getLastTreeTop();
   _seenDefinedSymbolReferences = new (trStackMemory()) TR_BitVector(comp()->getSymRefCount(), trMemory(), stackAlloc, growable);
   _writtenAndNotJustForHeapification = new (trStackMemory()) TR_BitVector(comp()->getSymRefCount(), trMemory(), stackAlloc, growable);
   _additionInfo = new (trStackMemory()) TR_BitVector(comp()->getSymRefCount(), trMemory(), stackAlloc, growable);

   _whileIndex = 0;
   _counter = 0;
   _guardedCalls.deleteAll();
   for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt.getNext())
      {
      // Can be used for debugging; to stop a particular loop from being
      // and see if the problem persists or not.
      //
      //if (_counter == 1)
      //   break;
      //else
      //   _counter++;

      TR_RegionStructure *naturalLoop = nextWhileLoop->asRegion();

      // HCR guard versioning removes edges from the CFG immediately (unlike
      // versioning of regular conditionals/virtual guards, which leaves a
      // trivially foldable conditional tree in place). The removal of
      // particular edges can change loops into acyclic regions, so skip any
      // region that is no longer a loop.
      //
      // (OSR guard versioning also removes edges immediately, but the targets
      // of those edges are not within any loop, so their removal can't change
      // loops into acyclic regions.)
      //
      if (!naturalLoop->isNaturalLoop())
         continue;

      TR::Block *entryBlock = naturalLoop->getEntryBlock();
      if (!loopIsWorthVersioning(naturalLoop))
         continue;

      _requiresAdditionalCheckForIncrement = false;
      _neitherLoopCold = false;
      _containsGuard = false;
      _containsUnguardedCall = false;
      _loopTransferDone = false;
      _unchangedValueUsedInBndCheck = new(trStackMemory()) TR_BitVector(comp()->getNodeCount(), trMemory(), stackAlloc);

      _checksInDupHeader.deleteAll();

      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"Loop versioner, expecting natural loop");

      if (trace())
         comp()->dumpMethodTrees("Trees after this versioning");

      TR::Region curLoopMemRegion(stackMemoryRegion);
      CurLoop curLoop(comp(), curLoopMemRegion, naturalLoop);
      _curLoop = &curLoop;

      TR_ScratchList<TR::Node> nullCheckedReferences(trMemory());
      TR_ScratchList<TR::TreeTop> nullCheckTrees(trMemory());
      TR_ScratchList<int32_t> numIndirections(trMemory());
      TR_ScratchList<TR::TreeTop> boundCheckTrees(trMemory());
      TR_ScratchList<TR::TreeTop> spineCheckTrees(trMemory());
      TR_ScratchList<int32_t> numDimensions(trMemory());
      TR_ScratchList<TR::TreeTop> conditionalTrees(trMemory());
      TR_ScratchList<TR::TreeTop> divCheckTrees(trMemory());
      TR_ScratchList<TR::TreeTop> checkCastTrees(trMemory());
      TR_ScratchList<TR::TreeTop> arrayStoreCheckTrees(trMemory());
      TR_ScratchList<TR::TreeTop> awrtbariTrees(trMemory());
      TR_ScratchList<TR::Node> specializedInvariantNodes(trMemory());
      TR_ScratchList<TR_NodeParentSymRef> invariantNodesList(trMemory());
      TR_ScratchList<TR_NodeParentSymRefWeightTuple> invariantTranslationNodesList(trMemory());
      TR_ScratchList<TR::Node> arrayAccesses(trMemory());
      TR_ScratchList<TR_NodeParentBlockTuple> arrayLoadCandidates(trMemory());
      TR_ScratchList<TR_NodeParentBlockTuple> arrayMemberLoadCandidates(trMemory());
      TR_BitVector disqualifiedRefinementCandidates(1,trMemory(),stackAlloc,growable);
      _disqualifiedRefinementCandidates = & disqualifiedRefinementCandidates;
      _arrayLoadCandidates = &arrayLoadCandidates;
      _arrayAccesses = &arrayAccesses;
      _arrayMemberLoadCandidates = &arrayMemberLoadCandidates;
      _currentNaturalLoop = naturalLoop;

      List<TR_NodeParentSymRef> *invariantNodes = &invariantNodesList;

      if (comp()->getOption(TR_DisableInvariantCodeMotion) || shouldOnlySpecializeLoops())
         invariantNodes = NULL;

      if (refineAliases())
         {
         naturalLoop->resetInvariance();
         naturalLoop->computeInvariantExpressions();
         }

      _seenDefinedSymbolReferences->empty();
      _writtenAndNotJustForHeapification->empty();
      _additionInfo->empty();
      _asyncCheckTree = NULL;

      // Initialize induction variable information
      //
      _numberOfTreesInLoop = 0;
      _versionableInductionVariables.deleteAll();
      _specialVersionableInductionVariables.deleteAll();
      _derivedVersionableInductionVariables.deleteAll();

      if(detectCanonicalizedPredictableLoops(naturalLoop, NULL, -1)<0)  //if there was a problem detecting, move to next iteration of the loop
         {
         _curLoop = NULL;
         continue;
         }

      _loopConditionInvariant = false;

      bool discontinue = false;
      bool   nullChecksMayBeEliminated = detectChecksToBeEliminated(naturalLoop, &nullCheckedReferences, &nullCheckTrees, &numIndirections,
                                                                  &boundCheckTrees, &spineCheckTrees, &numDimensions, &conditionalTrees, &divCheckTrees, &awrtbariTrees,
                                                                  &checkCastTrees, &arrayStoreCheckTrees, &specializedInvariantNodes, invariantNodes,
                                                                  &invariantTranslationNodesList, discontinue);

      if (discontinue)
         {
         _curLoop = NULL;
         continue;
         }

      if (_loopTestTree)
         {
         if ((_loopTestTree->getNode()->getNumChildren() > 1) &&
             isExprInvariant(_loopTestTree->getNode()->getSecondChild()))
           {
           if (trace())
              traceMsg(comp(), "Limit in loop test tree %p is invariant\n", _loopTestTree->getNode()->getSecondChild());
           _loopConditionInvariant = true;
           }
         else
           _asyncCheckTree = NULL;
         }
      else
 	 _asyncCheckTree = NULL;

      bool nullChecksWillBeEliminated = false;

      if (nullChecksMayBeEliminated &&
            !refineAliases() &&
            !shouldOnlySpecializeLoops())
         nullChecksWillBeEliminated = detectInvariantChecks(&nullCheckedReferences, &nullCheckTrees);
      else
         {
         //nullCheckTrees.deleteAll();
         //nullCheckedReferences.deleteAll();
         }
      bool specializedNodesWillBeEliminated = false;
      if(!refineAliases())
         specializedNodesWillBeEliminated = detectInvariantSpecializedExprs(&specializedInvariantNodes);
      else
         specializedInvariantNodes.deleteAll();

      bool skipAsyncCheckRemoval = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         {
         specializedNodesWillBeEliminated = false;
         if (!specializedInvariantNodes.isEmpty())
            {
            enableSpecialization = true;
            skipAsyncCheckRemoval = true;
            }
         specializedInvariantNodes.deleteAll();
         }

      bool invariantNodesWillBeEliminated = false;
      if (!refineAliases() && invariantNodes && !invariantNodes->isEmpty())
         invariantNodesWillBeEliminated = detectInvariantNodes(invariantNodes, &invariantTranslationNodesList);
      else
         {
         invariantTranslationNodesList.deleteAll();
         invariantNodesList.deleteAll();
         }

      bool boundChecksWillBeEliminated = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         boundChecksWillBeEliminated = detectInvariantBoundChecks(&boundCheckTrees);
      else
         boundCheckTrees.deleteAll();

      bool spineChecksWillBeEliminated = false;

      if (false && shouldOnlySpecializeLoops() && comp()->requiresSpineChecks() && !refineAliases())
         spineChecksWillBeEliminated = detectInvariantSpineChecks(&spineCheckTrees);
      else
         spineCheckTrees.deleteAll();


      bool divChecksWillBeEliminated = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         divChecksWillBeEliminated = detectInvariantDivChecks(&divCheckTrees);
      //else
      //   divCheckTrees.deleteAll();

      bool awrtBarisWillBeEliminated = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         awrtBarisWillBeEliminated = detectInvariantAwrtbaris(&awrtbariTrees);
      else
         awrtbariTrees.deleteAll();

      SharedSparseBitVector reverseBranchInLoops(comp()->allocator());
      bool checkCastTreesWillBeEliminated = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         checkCastTreesWillBeEliminated = detectInvariantCheckCasts(&checkCastTrees);
      else
         checkCastTrees.deleteAll();


      bool arrayStoreCheckTreesWillBeEliminated = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         arrayStoreCheckTreesWillBeEliminated = detectInvariantArrayStoreChecks(&arrayStoreCheckTrees);
      else
         arrayStoreCheckTrees.deleteAll();


      bool conditionalsWillBeEliminated = false;

      bool versionToRefineAliases = false;
      if(refineAliases())
         versionToRefineAliases = processArrayAliasCandidates();


      _conditionalTree = NULL;
      _duplicateConditionalTree = NULL;
      bool containsNonInlineGuard = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         {
         // default hotness threshold
         TR_Hotness hotnessThreshold = hot;
         if (comp()->getOption(TR_EnableAggressiveLoopVersioning))
            {
            if (trace()) traceMsg(comp(), "aggressiveLoopVersioning: raising hotnessThreshold for conditionalsWillBeEliminated\n");
            hotnessThreshold = maxHotness; // threshold which can't be matched by the > operator
            }

         if (( (debug("nullCheckVersion") ||
                comp()->cg()->performsChecksExplicitly() ||
                (comp()->getMethodHotness() > hotnessThreshold)
               ) && nullChecksWillBeEliminated
             ) ||
             boundChecksWillBeEliminated ||
             divChecksWillBeEliminated  ||
             awrtBarisWillBeEliminated  ||
             checkCastTreesWillBeEliminated ||
             arrayStoreCheckTreesWillBeEliminated ||
             specializedNodesWillBeEliminated ||
             invariantNodesWillBeEliminated ||
             _nonInlineGuardConditionalsWillNotBeEliminated ||
             _loopTransferDone)
            {
            conditionalsWillBeEliminated = detectInvariantConditionals(
               naturalLoop,
               &conditionalTrees,
               true,
               &containsNonInlineGuard,
               reverseBranchInLoops);
            }
         else
            {
            conditionalsWillBeEliminated = detectInvariantConditionals(
               naturalLoop,
               &conditionalTrees,
               false,
               &containsNonInlineGuard,
               reverseBranchInLoops);

            if (containsNonInlineGuard)
               {
               _neitherLoopCold = true;
               _conditionalTree = conditionalTrees.getListHead()->getData()->getNode();
               }
            }
         }
      else
         {
         conditionalTrees.deleteAll();
         _neitherLoopCold = true;
         }


      List<TR_Structure> clonedInnerWhileLoops(trMemory());
      bool versionedThisLoop = false;
      if ((versionToRefineAliases || !refineAliases()) &&
          (((debug("nullCheckVersion") || comp()->cg()->performsChecksExplicitly() ||
             (comp()->getMethodHotness() >= veryHot)) &&
            (naturalLoop->containsOnlyAcyclicRegions() || !_loopTransferDone) &&
            nullChecksWillBeEliminated) ||
            (_asyncCheckTree &&
             shouldOnlySpecializeLoops()) ||
           boundChecksWillBeEliminated ||
           spineChecksWillBeEliminated ||
           conditionalsWillBeEliminated ||
           divChecksWillBeEliminated ||
           awrtBarisWillBeEliminated  ||
           checkCastTreesWillBeEliminated ||
           arrayStoreCheckTreesWillBeEliminated ||
           specializedNodesWillBeEliminated ||
           invariantNodesWillBeEliminated ||
           versionToRefineAliases ||
           _containsGuard))
         {
         //if (shouldOnlySpecializeLoops())
         //   printf("Reached here for %s\n", comp()->signature());
         somethingChanged = true;
         versionedThisLoop = true;
         versionNaturalLoop(naturalLoop, &nullCheckedReferences, &nullCheckTrees, &boundCheckTrees, &spineCheckTrees, &conditionalTrees, &divCheckTrees, &awrtbariTrees, &checkCastTrees, &arrayStoreCheckTrees, &specializedInvariantNodes, invariantNodes, &invariantTranslationNodesList, &whileLoops, &clonedInnerWhileLoops, skipAsyncCheckRemoval, reverseBranchInLoops);
         _exitGotoTarget = NULL;
         }

      if (versionedThisLoop)
         {
         int32_t symRefCount = comp()->getSymRefCount();
         TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

         if(refineAliases())
            refineArrayAliases(naturalLoop);

         TR_BitVector *inductionVars = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
         ListElement<int32_t> *versionableInductionVar = _versionableInductionVariables.getListHead();
         while (versionableInductionVar)
            {
            TR::SymbolReference *inductionVar = comp()->getSymRefTab()->getSymRef(*(versionableInductionVar->getData()));
            //printf("Induction variable %d in loop %d in register for %s\n", inductionVar->getReferenceNumber(), naturalLoop->getNumber(), comp()->signature());
            inductionVars->set(inductionVar->getReferenceNumber());
            TR::RegisterCandidate *inductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(inductionVar);
            //dumpOptDetails(comp(), "%s Adding auto %d as a global register candidate in loop %d\n", OPT_DETAILS_LOOP_VERSIONER, inductionCandidate->getSymbolReference()->getReferenceNumber(), naturalLoop->getNumber());
            inductionCandidate->addAllBlocksInStructure(naturalLoop, comp(), trace()?"auto":NULL);
            versionableInductionVar = versionableInductionVar->getNextElement();
            }

         TR_InductionVariable *v;
         int32_t symRefNumber;
         for (v = naturalLoop->getFirstInductionVariable(); v; v = v->getNext())
            {
            TR::SymbolReference *symRef = NULL;
            TR::SymbolReference *inductionSymRef = NULL;
            for (symRefNumber = symRefTab->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
               {
               symRef = symRefTab->getSymRef(symRefNumber);
               if (symRef)
                  {
                  TR::Symbol *sym = symRef->getSymbol();
                  if (v->getLocal() == sym)
                     {
                     inductionSymRef = symRef;
                     break;
                     }
                  }
               }


             if (inductionSymRef && !inductionVars->get(inductionSymRef->getReferenceNumber()))
               {
               //printf("CP Induction variable %d in natural loop %d in register for %s\n", inductionSymRef->getReferenceNumber(), naturalLoop->getNumber(), comp()->signature());
               TR::RegisterCandidate *inductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(inductionSymRef);
               //dumpOptDetails(comp(), "%s Adding auto %d as a global register candidate in loop %d\n", OPT_DETAILS_LOOP_VERSIONER, inductionCandidate->getSymbolReference()->getReferenceNumber(), naturalLoop->getNumber());
               inductionCandidate->addAllBlocksInStructure(naturalLoop, comp(), trace()?"auto":NULL);
               }
            }


         whileLoops.setListHead(whileLoopsIt.getCurrentElement());
         clonedInnerWhileLoops.popHead();
         ListIterator<TR_Structure> clonedInnerWhileLoopsIt(&clonedInnerWhileLoops);
         TR_Structure *clonedInnerWhileLoop = clonedInnerWhileLoopsIt.getFirst();
         ListElement<TR_Structure> *prevClonedInnerLoop = whileLoops.getListHead();
         for (; clonedInnerWhileLoop != NULL; clonedInnerWhileLoop = clonedInnerWhileLoopsIt.getNext())
            {
            prevClonedInnerLoop = whileLoops.addAfter(clonedInnerWhileLoop, prevClonedInnerLoop);
            }
         whileLoopsIt.set(&whileLoops);

         if (!shouldOnlySpecializeLoops() && !refineAliases() && comp()->requiresSpineChecks())
            {
            if (!spineCheckTrees.isEmpty())
               {
               enableSpecialization = true;
               skipAsyncCheckRemoval = true;
               }
            }
         }

      _curLoop = NULL;
      }

   // If there are any entries in _virtualGuardInfo, they were taken into
   // account in the above analyses (i.e. their cold calls were assumed to have
   // been removed from the loop after transformation), so at this point loop
   // transfer is mandatory.
   if (!_virtualGuardInfo.isEmpty())
      performLoopTransfer();

   // Use/def info and value number info are now bad.
   //
   optimizer()->setUseDefInfo(NULL);
   optimizer()->setValueNumberInfo(NULL);

   _nonInlineGuardConditionalsWillNotBeEliminated = true;

   if (!shouldOnlySpecializeLoops() &&
       enableSpecialization)
      {
      //setOnlySpecializeLoops(true);
      requestOpt(OMR::loopSpecializerGroup);
      }
   else if (shouldOnlySpecializeLoops())
      {
      //setOnlySpecializeLoops(false);
      requestOpt(OMR::deadTreesElimination);
      requestOpt(OMR::treeSimplification);
      }

   if (somethingChanged)
      requestOpt(OMR::andSimplification);

   if (trace())
      {
      traceMsg(comp(), "\nCFG after loop versioning:\n");
      getDebug()->print(comp()->getOutFile(), _cfg);
      traceMsg(comp(), "Ending LoopVersioner\n");
      }

   //comp()->dumpMethodTrees("Trees after this versioning");

   if (_invalidateAliasSets)
      optimizer()->setAliasSetsAreValid(false);

   return 1; // actual cost
   }


void TR_LoopVersioner::performLoopTransfer()
   {
   // Only do the transfer if there are no unguarded calls left
   // in the loop (should be checked here)
   //

   dumpOptDetails(comp(), "Loop transfer in %s with size %d\n", comp()->signature(), _virtualGuardInfo.getSize());

   TR_ScratchList<TR::Node> processedVirtualGuards(trMemory());
   TR::CFG *cfg = comp()->getFlowGraph();
   VirtualGuardInfo *vgInfo = NULL;
   for (vgInfo = _virtualGuardInfo.getFirst(); vgInfo; vgInfo = vgInfo->getNext())
      {
      ListIterator<VirtualGuardPair> guardsIt(&vgInfo->_virtualGuardPairs);
      VirtualGuardPair *nextVirtualGuard;
      for (nextVirtualGuard = guardsIt.getFirst(); nextVirtualGuard != NULL; nextVirtualGuard = guardsIt.getNext())
         {
         TR::Block *hotGuardBlock = nextVirtualGuard->_hotGuardBlock;
         TR::Block *coldGuardBlock = nextVirtualGuard->_coldGuardBlock;
         TR::TreeTop *hotGuardTree = hotGuardBlock->getLastRealTreeTop();
         TR::TreeTop *coldGuardTree = coldGuardBlock->getLastRealTreeTop();
         TR::Node *hotGuard = hotGuardTree->getNode();
         TR::Node *coldGuard = coldGuardTree->getNode();
         if (hotGuard->getOpCode().isIf() &&
             hotGuard->isTheVirtualGuardForAGuardedInlinedCall() &&
             coldGuard->getOpCode().isIf() &&
             coldGuard->isTheVirtualGuardForAGuardedInlinedCall())
            {
            // Can set up loop transfer now
            //
            cfg->setStructure(NULL);
            hotGuardBlock->changeBranchDestination(coldGuard->getBranchDestination(), cfg);
            dumpOptDetails(comp(), "loop transfer, changed target of guard [%p] in [%d] to [%d]\n", hotGuard, hotGuardBlock->getNumber(), coldGuard->getBranchDestination()->getNode()->getBlock()->getNumber());
            const char *debugCounter = TR::DebugCounter::debugCounterName(comp(), "loopVersioner.transfer/(%s)/%s/origin=block_%d", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()), hotGuardBlock->getNumber());
            if (comp()->getOptions()->dynamicDebugCounterIsEnabled(debugCounter))
               {
               TR::Block *newBlock = hotGuardBlock->splitEdge(hotGuardBlock, coldGuard->getBranchDestination()->getNode()->getBlock(), comp(), NULL, false);
               TR::DebugCounter::prependDebugCounter(comp(), debugCounter, newBlock->getEntry()->getNextTreeTop());
               }
            }
         }
      }
   }

// create the switch block containing the node
// and return the cloned header of the loop
//
TR::Block *TR_LoopVersioner::createClonedHeader(TR::Block *origHeader, TR::TreeTop **endTree)
   {
   TR_BlockCloner cloner(_cfg, true);
   TR::Block *clonedHeader = cloner.cloneBlocks(origHeader, origHeader);
   clonedHeader->setFrequency(origHeader->getFrequency());
   if (origHeader->getNextBlock())
      clonedHeader->getExit()->join(origHeader->getNextBlock()->getEntry());
   else
      {
      // update the endTree
      clonedHeader->getExit()->setNextTreeTop(NULL);
      *endTree = clonedHeader->getExit();
      }
   origHeader->getExit()->join(clonedHeader->getEntry());

   TR_ScratchList<TR::CFGEdge> removedEdges(trMemory());
   // fixup the successors
   //
   for (auto e = origHeader->getSuccessors().begin(); e != origHeader->getSuccessors().end(); ++e)
      {
      removedEdges.add(*e);
      TR::Block *dest = toBlock((*e)->getTo());
      _cfg->addEdge(TR::CFGEdge::createEdge(clonedHeader,  dest, trMemory()));
      }

   for (auto e = origHeader->getExceptionSuccessors().begin(); e != origHeader->getExceptionSuccessors().end(); ++e)
      {
      removedEdges.add(*e);
      TR::Block *dest = toBlock((*e)->getTo());
      _cfg->addEdge(TR::CFGEdge::createExceptionEdge(clonedHeader, dest,trMemory()));
      }
   _cfg->addEdge(TR::CFGEdge::createEdge(origHeader,  clonedHeader, trMemory()));

   // now remove the edges
   //
   ListIterator<TR::CFGEdge> sIt;
   sIt.set(&removedEdges);
   for (TR::CFGEdge* e = sIt.getFirst(); e; e = sIt.getNext())
      _cfg->removeEdge(e);

   return clonedHeader;
   }

TR::Node *TR_LoopVersioner::createSwitchNode(TR::Block *clonedHeader, TR::SymbolReference *tempSymRef, int32_t numCase)
   {
   TR::Node *switchNode = TR::Node::create(clonedHeader->getFirstRealTreeTop()->getNode(), TR::lookup, numCase+2);
   TR::Node *loadTemp = TR::Node::createWithSymRef(switchNode, TR::iload, 0, tempSymRef);
   switchNode->setAndIncChild(0, loadTemp);

   for (int32_t i = 0; i < numCase+1; i++)
      {
      TR::Node *caseNode = TR::Node::createCase(switchNode, clonedHeader->getEntry(), i);
      switchNode->setAndIncChild(i+1, caseNode);
      }
   return switchNode;
   }

TR::Block * TR_LoopVersioner::createEmptyGoto(TR::Block *source, TR::Block *dest, TR::TreeTop *endTree)
   {
   // create an empty goto block
   // its the caller's responsibility to add this block to the cfg
   // and create appropriate edges
   //
   TR::TreeTop *destEntry = dest->getEntry();
   TR::Block *gotoBlock = TR::Block::createEmptyBlock(destEntry->getNode(), comp(), dest->getFrequency(), dest);
   gotoBlock->setIsSpecialized(dest->isSpecialized());
   TR::TreeTop *gotoEntry = gotoBlock->getEntry();
   TR::TreeTop *gotoExit = gotoBlock->getExit();
   TR::Node *gotoNode = TR::Node::create(destEntry->getNextTreeTop()->getNode(), TR::Goto, 0, destEntry);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
   gotoEntry->join(gotoTree);
   gotoTree->join(gotoExit);
   // paste the block in the trees
   // if endTree is null, caller will
   // take care of pasting
   //
   if (endTree)
      {
      endTree->join(gotoEntry);
      gotoExit->setNextTreeTop(NULL);
      }
   gotoEntry->getNode()->setBlock(gotoBlock);
   gotoExit->getNode()->setBlock(gotoBlock);
   return gotoBlock;
   }



bool TR_LoopVersioner::detectInvariantChecks(List<TR::Node> *nullCheckedReferences, List<TR::TreeTop> *nullCheckTrees)
   {
   bool foundInvariantChecks = false;
   ListElement<TR::Node> *node, *prevNode = NULL;
   ListElement<TR::TreeTop> *nextTree = nullCheckTrees->getListHead();
   ListElement<TR::TreeTop> *prevTree = NULL;

   for (node = nullCheckedReferences->getListHead(); node != NULL;)
      {
      bool isNullCheckReferenceInvariant = isExprInvariant(node->getData());

      if (!isNullCheckReferenceInvariant &&
          node->getData()->getOpCode().hasSymbolReference() &&
          ((node->getData()->getSymbolReference()->getSymbol()->isAuto() && isDependentOnInvariant(node->getData())) ||
           (node->getData()->getOpCode().isLoadIndirect() &&
            !_seenDefinedSymbolReferences->get(node->getData()->getSymbolReference()->getReferenceNumber()) &&
            node->getData()->getFirstChild()->getOpCode().hasSymbolReference() &&
            node->getData()->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
            isDependentOnInvariant(node->getData()->getFirstChild()))))
         isNullCheckReferenceInvariant = true;

      if (!isNullCheckReferenceInvariant ||
          _checksInDupHeader.find(nextTree->getData()))
         {
         if (trace())
            traceMsg(comp(), "Non invariant Null check reference %p (%s)\n", node->getData(), node->getData()->getOpCode().getName());

         if (prevNode)
            {
            prevNode->setNextElement(node->getNextElement());
            prevTree->setNextElement(nextTree->getNextElement());
            }
         else
            {
            nullCheckedReferences->setListHead(node->getNextElement());
            nullCheckTrees->setListHead(nextTree->getNextElement());
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Invariant Null check reference %p (%s)\n", node->getData(), node->getData()->getOpCode().getName());
         foundInvariantChecks = true;
         prevNode = node;
         prevTree = nextTree;
         }

      node = node->getNextElement();
      nextTree = nextTree->getNextElement();
      }


   return foundInvariantChecks;
   }



bool TR_LoopVersioner::detectInvariantArrayStoreChecks(List<TR::TreeTop> *arrayStoreCheckTrees)
   {
   bool foundInvariantChecks = false;

   ListElement<TR::TreeTop> *treetop, *prevTreetop = NULL;
   for (treetop = arrayStoreCheckTrees->getListHead(); treetop != NULL;)
      {
      TR::Node *childNode = treetop->getData()->getNode()->getFirstChild();
      TR::Node *arrayNode = NULL;
      TR::Node *valueNode = NULL;
      if (childNode->getOpCode().isWrtBar())
         {
         int32_t lastChild = childNode->getNumChildren()-1;
         arrayNode = childNode->getChild(lastChild);
         valueNode = childNode->getChild(lastChild-1);
         }

      bool sourceInvariant = false;
      if (arrayNode && valueNode)
         {
         //visitCount = comp()->incVisitCount();
         //sourceInvariant = isExprInvariant(valueNode, visitCount);

         if (!sourceInvariant)
            {
            if (valueNode->getOpCode().hasSymbolReference() &&
                valueNode->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
               {
               TR::Node *addressNode = valueNode->getFirstChild();
               if (addressNode->getOpCode().isArrayRef())
                  {
                  TR::Node *childOfAddressNode = addressNode->getFirstChild();
                  if (!childOfAddressNode->isInternalPointer() &&
                      (!childOfAddressNode->getOpCode().hasSymbolReference() ||
                       (!childOfAddressNode->getSymbolReference()->getSymbol()->isInternalPointerAuto() &&
                        !childOfAddressNode->getSymbolReference()->getSymbol()->isNotCollected())))
                     {
                     sourceInvariant = isExprInvariant(childOfAddressNode);
                     }
                  }
               }
              }

         if (sourceInvariant)
            {
            bool targetInvariant = isExprInvariant(arrayNode);
            if (!targetInvariant ||
                _checksInDupHeader.find(treetop->getData()))
               {
               if (trace())
                  traceMsg(comp(), "Non invariant Array store check %p (%s)\n", treetop->getData()->getNode(), treetop->getData()->getNode()->getOpCode().getName());

               if (prevTreetop)
                  prevTreetop->setNextElement(treetop->getNextElement());
               else
                  arrayStoreCheckTrees->setListHead(treetop->getNextElement());
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "Invariant Array store check %p (%s)\n", treetop->getData()->getNode(), treetop->getData()->getNode()->getOpCode().getName());
               foundInvariantChecks = true;
               prevTreetop = treetop;
               }
            }
         else
            {
            if (trace())
                  traceMsg(comp(), "Non invariant Specialized expr %p (%s)\n", treetop->getData()->getNode(), treetop->getData()->getNode()->getOpCode().getName());

            if (prevTreetop)
               prevTreetop->setNextElement(treetop->getNextElement());
            else
               arrayStoreCheckTrees->setListHead(treetop->getNextElement());
            }
         }

      treetop = treetop->getNextElement();
      }

   return foundInvariantChecks;
   }


bool TR_LoopVersioner::detectInvariantSpecializedExprs(List<TR::Node> *profiledExprs)
   {
   bool foundInvariantExprs = false;
   ListElement<TR::Node> *node, *prevNode = NULL;

   for (node = profiledExprs->getListHead(); node != NULL;)
      {
      bool isProfiledExprInvariant = isExprInvariant(node->getData());
      if (!isProfiledExprInvariant &&
          node->getData()->getOpCode().hasSymbolReference() &&
          node->getData()->getSymbolReference()->getSymbol()->isAuto() &&
          isDependentOnInvariant(node->getData()))
         isProfiledExprInvariant = true;

      if (!isProfiledExprInvariant)
         {
         if (trace())
            traceMsg(comp(), "Non invariant Specialized expr %p (%s)\n", node->getData(), node->getData()->getOpCode().getName());

         if (prevNode)
            prevNode->setNextElement(node->getNextElement());
         else
            profiledExprs->setListHead(node->getNextElement());
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Invariant Specialized expr %p (%s)\n", node->getData(), node->getData()->getOpCode().getName());
         foundInvariantExprs = true;
         prevNode = node;
         }

      node = node->getNextElement();
      }

   return foundInvariantExprs;
   }



TR::Node* nodeTreeGetFirstOpCode(TR::Node *n, TR::ILOpCodes op)
   {
   if(n->getOpCodeValue() == op)
      return n;
   for (int32_t i = 0; i < n->getNumChildren(); ++i)
      {
      TR::Node *childNode = nodeTreeGetFirstOpCode(n->getChild(i), op);
      if(childNode)
         return childNode;
      }
   return NULL;
   }


bool TR_LoopVersioner::detectInvariantNodes(List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodes)
   {
   bool foundInvariantNodes = false;
   ListElement<TR_NodeParentSymRef> *nextNode = invariantNodes->getListHead();
   ListElement<TR_NodeParentSymRef> *prevNode = NULL;

   for (;nextNode;)
      {
      TR::Node *node = nextNode->getData()->_node;
      TR::Node *parent = nextNode->getData()->_parent;
      if (trace()) traceMsg(comp(), "Looking at node %p parent %p\n\n", node, nextNode->getData()->_parent);
      bool isNodeInvariant = isExprInvariant(node);
      // while not technically true, computeCC and overflow compares need to have their children intact and so they can't be marked as invariant
      // a better solution is really needed
      bool removeElement = false;
      if (isNodeInvariant)
         foundInvariantNodes = true;
      else
         removeElement = true;

      if (removeElement)
         {
         if (trace())
            traceMsg(comp(), "Non invariant expr %p (%s)\n", node, node->getOpCode().getName());

         if (prevNode)
            prevNode->setNextElement(nextNode->getNextElement());
         else
            invariantNodes->setListHead(nextNode->getNextElement());
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Invariant expr %p (%s)\n", node, node->getOpCode().getName());

         prevNode = nextNode;
         }

      nextNode = nextNode->getNextElement();
      }

   return foundInvariantNodes;
   }


TR::Node *TR_LoopVersioner::findCallNodeInBlockForGuard(TR::Node *node)
   {
   TR::TreeTop *tt = node->getBranchDestination();

   while (tt)
      {
      TR::Node *ttNode = tt->getNode();

      if (ttNode->getOpCodeValue() == TR::BBEnd)
         break;

      if (ttNode->getOpCode().isTreeTop() && (ttNode->getNumChildren() > 0))
         ttNode = ttNode->getFirstChild();

      if (ttNode->getOpCode().isCall())
         {
         if (ttNode->getNumChildren() > 0)
            {
            uint32_t bcIndex = ttNode->getByteCodeIndex();
            int16_t callerIndex = ttNode->getInlinedSiteIndex();
            int16_t inlinedCallIndex = node->getInlinedSiteIndex();

            if (unsigned(inlinedCallIndex) < comp()->getNumInlinedCallSites())
               {
               TR_InlinedCallSite & ics = comp()->getInlinedCallSite(inlinedCallIndex);
               if ((ics._byteCodeInfo.getByteCodeIndex() == bcIndex) &&
                   (ics._byteCodeInfo.getCallerIndex() == callerIndex))
                  {
                  //printf("Located tree in new routine\n");
                  //if (tt != node->getBranchDestination()->getNextTreeTop())
                  //   printf("Located tree in new routine by jumping over some trees\n");
                  //fflush(stdout);
                  return ttNode;
                  }
               }
            }

         return NULL;
         }

      tt = tt->getNextTreeTop();
      }

   return NULL;
   }

bool TR_LoopVersioner::detectInvariantCheckCasts(List<TR::TreeTop> *trees)
   {
   bool foundInvariantTrees = false;
   ListElement<TR::TreeTop> *curTreesElem = trees->getListHead();
   ListElement<TR::TreeTop> *prevTreesElem = NULL;
   while (curTreesElem != NULL)
      {
      ListElement<TR::TreeTop> *nextTreesElem = curTreesElem->getNextElement();
      TR::Node *node = curTreesElem->getData()->getNode();
      TR_ASSERT_FATAL_WITH_NODE(
         node, node->getOpCode().isCheckCast(), "expected a checkcast");

      if (areAllChildrenInvariant(node))
         {
         foundInvariantTrees = true;
         prevTreesElem = curTreesElem;
         if (trace())
            {
            traceMsg(
               comp(),
               "Invariant checkcast n%un [%p]\n",
               node->getGlobalIndex(),
               node);
            }
         }
      else
         {
         // Because curTreesElem is removed, prevTreesElem remains unchanged
         if (prevTreesElem != NULL)
            prevTreesElem->setNextElement(nextTreesElem);
         else
            trees->setListHead(nextTreesElem);

         if (trace())
            {
            traceMsg(
               comp(),
               "Non-invariant checkcast n%un %p\n",
               node->getGlobalIndex(),
               node);
            }
         }

      curTreesElem = nextTreesElem;
      }

   return foundInvariantTrees;
   }

/**
 * \brief Determine whether it is possible to version \p ifNode w.r.t. an
 * extremum of a non-invariant function of an IV.
 *
 * \param[in]  ifNode The conditional node to analyze
 * \param[out] out    The results of analyzing the conditional node, if
 *                    versioning is possible.
 *
 * \return true when versioning is possible, false otherwise.
 */
bool TR_LoopVersioner::isVersionableIfWithExtremum(
   TR::TreeTop *ifTree, ExtremumConditional *out)
   {
   TR::Node *ifNode = ifTree->getNode();
   ExtremumConditional excond = {};
   excond.varyingChildIndex = -1; // invalid
   *out = excond;

   bool withMin = ifNode->isVersionableIfWithMinExpr();
   bool withMax = ifNode->isVersionableIfWithMaxExpr();
   if (!withMin && !withMax)
      return false;

   TR_ASSERT_FATAL_WITH_NODE(
      ifNode, !withMin || !withMax, "both min and max flags are set");

   static const bool enable = feGetEnv("TR_dontVersionIfWithExtremum") == NULL;
   if (!enable)
      return false;

   TR_ASSERT_FATAL_WITH_NODE(
      ifNode, ifNode->getNumChildren() == 2, "unexpected number of children");

   if (!ifNode->getChild(0)->getDataType().isIntegral()
       || ifNode->getChild(0)->getDataType() != ifNode->getChild(1)->getDataType())
      return false;

   bool invariant0 = isExprInvariant(ifNode->getChild(0));
   bool invariant1 = isExprInvariant(ifNode->getChild(1));
   if (invariant0 == invariant1)
      return false;

   excond.varyingChildIndex = (int32_t)invariant0;
   TR::Node *varyingChild = ifNode->getChild(excond.varyingChildIndex);

   TR::Node *inner = varyingChild;
   bool increasing = true; // Is varyingChild increasing as a function of the IV?
   while (inner->getOpCode().isAdd()
          || inner->getOpCode().isSub()
          || inner->getOpCode().isMul()
          || inner->getOpCode().isNeg())
      {
      // Overflowing operations do not necessarily preserve ordering in the way
      // that we expect. NOTE: cannotOverflow means that the compiler has ruled
      // out *signed* overflow. We have no flag that indicates the absence of
      // unsigned overflow, so this is a signed analysis, and versioning of
      // unsigned comparisons requires extra care on the part of the caller.
      if (!inner->cannotOverflow())
         return false;

      int32_t nextChildIndex = -1;
      if (inner->getOpCode().isNeg())
         nextChildIndex = 0;
      else if (isExprInvariant(inner->getChild(1)))
         nextChildIndex = 0;
      else if (isExprInvariant(inner->getChild(0)))
         nextChildIndex = 1;
      else
         return false;

      if (inner->getOpCode().isNeg()
          || (inner->getOpCode().isSub() && nextChildIndex == 1))
         {
         increasing = !increasing;
         }
      else if (inner->getOpCode().isMul())
         {
         // Find the sign of the multiplier and determine whether we have to
         // toggle increasing.
         //
         // NOTE: The sign only needs to be correct for the cases where the
         // multiplier is nonzero. If we happen to multiply by zero - either a
         // constant zero (unlikely), or a loop-invariant value known to be
         // nonnegative or nonpositive that happens to be zero at runtime -
         // then varyingChild will actually have the same value on every loop
         // iteration. In particular, the initial and final values will be
         // identical. So we can accept the possibility of a zero here to relax
         // the requirements for non-constant multipliers, i.e. we can accept
         // a node that isNonNegative() rather than require isPositive().
         //
         TR::Node *multiplier = inner->getChild(1 - nextChildIndex);
         bool flip = false;
         if (multiplier->getOpCode().isLoadConst())
            flip = multiplier->get64bitIntegralValue() < 0;
         else if (multiplier->isNonNegative())
            flip = false;
         else if (multiplier->isNonPositive())
            flip = true;
         else
            return false; // cannot determine the sign

         if (flip)
            increasing = !increasing;
         }

      inner = inner->getChild(nextChildIndex);
      }

   if (!inner->getOpCode().isLoadVarDirect())
      return false;

   excond.ivLoad = inner;
   excond.iv = inner->getSymbolReference();
   int32_t symRefNum = excond.iv->getReferenceNumber();
   auto *usableIV = _versionableInductionVariables.getListHead();
   while (usableIV != NULL && *usableIV->getData() != symRefNum)
      usableIV = usableIV->getNextElement();

   if (usableIV == NULL)
      return false;

   // The conditional branch is in a suitable form. Now check all additional
   // conditions that are required for versioning.

   TR::Node *ivUpdate = _storeTrees[symRefNum]->getNode();
   TR::Node *newIvValue = ivUpdate->getChild(0);

   // It should be perfectly possible to deal with 64-bit, but for now we only
   // do 32-bit IVs, e.g. the final value calculation is all 32-bit.
   if (!newIvValue->getType().isInt32())
      return false;

   // Find the IV step.
   TR::ILOpCodes incOp = newIvValue->getOpCodeValue();
   if (incOp != TR::iadd && incOp != TR::isub)
      return false;

   if (!newIvValue->cannotOverflow())
      return false; // can't predict the extremum

   // Make sure that the LHS of the add/sub is a load of the same IV. This
   // should be guaranteed already by isStoreInRequiredForm(), but check anyway
   // just to be safe.
   TR::Node *oldIvLoad = newIvValue->getChild(0);
   if (oldIvLoad->getOpCodeValue() != TR::iload
       || oldIvLoad->getSymbolReference() != ivUpdate->getSymbolReference())
      return false;

   TR::Node *stepNode = newIvValue->getChild(1);
   if (stepNode->getOpCodeValue() != TR::iconst)
      return false;

   excond.step = stepNode->getInt();
   if (incOp == TR::isub)
      {
      if (excond.step == TR::getMinSigned<TR::Int32>())
         return false;

      excond.step = -excond.step;
      }

   if (excond.step == 0)
      return false; // unimportant case

   // Determine whether the extremum is the final value.
   bool exprIncreasingWithIters = increasing == (excond.step > 0);
   excond.extremumIsFinalValue =
      ifNode->isVersionableIfWithMaxExpr() == exprIncreasingWithIters;

   if (excond.extremumIsFinalValue || ifNode->getOpCode().isUnsigned())
      {
      // The final value is needed, so make sure that we can predict it.
      // The LHS of the loop test comparison must be either the old/unchanged
      // or new/updated value of the IV for the iteration. (We could probably
      // generalize to old IV + const, where the new IV is just old IV + step.)
      TR::Node *loopTest = _loopTestTree->getNode();
      TR::Node *lhs = loopTest->getChild(0);
      if (lhs == newIvValue)
         {
         excond.loopTest.usesUpdatedIv = true;
         }
      else if (lhs->getOpCodeValue() == TR::iload
          && lhs->getSymbolReference() == excond.iv)
         {
         excond.loopTest.usesUpdatedIv = ivLoadSeesUpdatedValue(lhs, _loopTestTree);
         }
      else
         {
         // Could pretty easily also allow e.g. (iadd (iload iv) (iconst step))
         // and treat is as the new value, as long as the IV load sees the old,
         // but don't bother for now.
         return false;
         }

      // The loop limit must be invariant.
      if (!isExprInvariant(loopTest->getChild(1)))
         return false;

      // Find the condition for remaining in the loop. This is needed because
      // the loop test may be testing the opposite condition, i.e. the jump
      // target may be the exit rather than the loop header.
      TR::ILOpCode testOp = loopTest->getOpCode();
      TR::Block *loopHeader = _curLoop->_loop->getEntryBlock();
      TR::Block *target = loopTest->getBranchDestination()->getNode()->getBlock();
      if (target != loopHeader)
         {
         TR_ASSERT_FATAL_WITH_NODE(
            loopTest,
            !_curLoop->_loop->contains(target->getStructureOf()),
            "loop test targets neither the header nor an exit");

         testOp = testOp.getOpCodeForReverseBranch();
         }

      excond.loopTest.keepLoopingCmpOp = testOp;

      // The direction of the loop test must agree with the sign of step, i.e.
      // adding step to the IV must bring us closer to failing the loop test.
      if (!testOp.isCompareForOrder())
         return false;

      if ((excond.step > 0) == testOp.isCompareTrueIfGreater())
         return false;
      }

   // Make sure there is an unambiguous cold side.
   bool takenCold =
      ifNode->getBranchDestination()->getNode()->getBlock()->isCold();
   bool fallthroughCold =
      ifTree->getEnclosingBlock()->getNextBlock()->isCold();

   if (takenCold == fallthroughCold)
      return false;

   excond.reverseBranch = fallthroughCold;

   // Success!
   if (trace())
      {
      traceMsg(
         comp(),
         "Conditional n%un [%p] is versionable based on an extremum of child %d: "
         "n%un [%p], derived from IV #%d\n",
         ifNode->getGlobalIndex(),
         ifNode,
         excond.varyingChildIndex,
         varyingChild->getGlobalIndex(),
         varyingChild,
         *usableIV->getData());
      }

   *out = excond;
   return true;
   }

bool TR_LoopVersioner::detectInvariantConditionals(
   TR_RegionStructure *whileLoop,
   List<TR::TreeTop> *trees,
   bool onlyDetectHighlyBiasedBranches,
   bool *containsNonInlineGuard,
   SharedSparseBitVector &reverseBranchInLoops)
   {
   bool foundInvariantTrees = false;
   ListElement<TR::TreeTop> *nextTree = trees->getListHead();
   ListElement<TR::TreeTop> *prevTree = NULL;
   TR::TreeTop *onlyNonInlineGuardConditional = NULL;


   TR_ScratchList<TR::Block> blocksInWhileLoop(trMemory());
   whileLoop->getBlocks(&blocksInWhileLoop);

   for (;nextTree;)
      {
      TR::Node *node = nextTree->getData()->getNode();
      if (trace())
         traceMsg(comp(), "guard node %p %d\n", node, onlyDetectHighlyBiasedBranches);

      TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isIf(), "expected if");

      bool highlyBiasedBranch = false;
      //static char *enableHBB = feGetEnv("TR_enableHighlyBiasedBranch");
      if (!node->isTheVirtualGuardForAGuardedInlinedCall())
         {
         TR::Block *nextBlock=nextTree->getData()->getEnclosingBlock();
         TR::Block *targetBlock=node->getBranchDestination()->getNode()->getBlock();

         if (nextBlock->getEdge(targetBlock)->getFrequency() <= MAX_COLD_BLOCK_COUNT+1 &&
             nextBlock->getFrequency() >= HIGH_FREQ_THRESHOLD &&
            static_cast<double> (nextBlock->getNextBlock()->getFrequency())/nextBlock->getFrequency() >= 0.8)
            {
            if (trace())
               traceMsg(comp(), "node %p is highly biased\n", node);
            highlyBiasedBranch = true;
            }
         else if(node->isVersionableIfWithMaxExpr() || node->isVersionableIfWithMinExpr() || node->isMaxLoopIterationGuard())
            {
            if (trace())
               traceMsg(comp(), "node %p is versionable If\n", node);
            highlyBiasedBranch = true;
            }
#ifdef J9_PROJECT_SPECIFIC
         else if (comp()->hasIntStreamForEach() &&
                ((targetBlock->isCold() && !nextBlock->getNextBlock()->isCold()) || (!targetBlock->isCold() && nextBlock->getNextBlock()->isCold())))
            {
            highlyBiasedBranch = true;
            }
#endif
         }

      bool includeComparisonTree = true;
      TR::Node *nextRealNode = NULL;
      TR::Node *thisChild = NULL;
      TR::Node *nodeToBeChecked = NULL;

      TR_VirtualGuard *guardInfo = NULL;

      if (onlyDetectHighlyBiasedBranches)
         {
         if (node->isTheVirtualGuardForAGuardedInlinedCall())
            {
            includeComparisonTree = false;

            guardInfo = comp()->findVirtualGuardInfo(node);
            if (guardInfo->getTestType() == TR_VftTest)
               {
               if (node->getFirstChild()->getNumChildren() > 0)
                  {
                  thisChild = node->getFirstChild()->getFirstChild();
                  includeComparisonTree = true;
                  }
               else
                  nodeToBeChecked = node->getFirstChild();
               }
            else if (guardInfo->getTestType() == TR_MethodTest)
               {
               if ((node->getFirstChild()->getNumChildren() > 0) &&
                   (node->getFirstChild()->getFirstChild()->getNumChildren() > 0))
                  {
                  thisChild = node->getFirstChild()->getFirstChild()->getFirstChild();
                  includeComparisonTree = true;
                  }
               else if (node->getFirstChild()->getNumChildren() > 0)
                  nodeToBeChecked = node->getFirstChild()->getFirstChild();
               else
                  nodeToBeChecked = node->getFirstChild();
               }

            if (!includeComparisonTree)
               {
               TR::Node *callNode = findCallNodeInBlockForGuard(node);
               if (callNode && blocksInWhileLoop.find(node->getBranchDestination()->getEnclosingBlock()))
                  {
                  nextRealNode = callNode;
                  includeComparisonTree = true;
                  }
               }
            }
         else if (!highlyBiasedBranch)
            includeComparisonTree = false;
         }
      else if (highlyBiasedBranch || node->isTheVirtualGuardForAGuardedInlinedCall())
         {
	      if (node->isTheVirtualGuardForAGuardedInlinedCall())
	         {
            includeComparisonTree = false;

            guardInfo = comp()->findVirtualGuardInfo(node);
            if (guardInfo->getTestType() == TR_VftTest)
               {
               if (node->getFirstChild()->getNumChildren() > 0)
                  {
                  thisChild = node->getFirstChild()->getFirstChild();
                  includeComparisonTree = true;
                  }
               else
                  nodeToBeChecked = node->getFirstChild();
               }
            else if (guardInfo->getTestType() == TR_MethodTest)
               {
               if ((node->getFirstChild()->getNumChildren() > 0) &&
                   (node->getFirstChild()->getFirstChild()->getNumChildren() > 0))
                  {
                  thisChild = node->getFirstChild()->getFirstChild()->getFirstChild();
                  includeComparisonTree = true;
                  }
               else if (node->getFirstChild()->getNumChildren() > 0)
                  nodeToBeChecked = node->getFirstChild()->getFirstChild();
               else
                  nodeToBeChecked = node->getFirstChild();
               }

            if (!includeComparisonTree)
               {
               TR::Node *callNode = findCallNodeInBlockForGuard(node);
               if (callNode && blocksInWhileLoop.find(node->getBranchDestination()->getEnclosingBlock()))
                  {
                  nextRealNode = callNode;
                  includeComparisonTree = true;
                  }
               }
	         }

         if (includeComparisonTree)
            {
            onlyDetectHighlyBiasedBranches = true;
            if (onlyNonInlineGuardConditional)
               {
               ListElement<TR::TreeTop> *nextTempTree = trees->getListHead();
               ListElement<TR::TreeTop> *prevTempTree = NULL;
               for (;nextTempTree != nextTree; nextTempTree=nextTempTree->getNextElement())
                  {
                  if (nextTempTree->getData() == onlyNonInlineGuardConditional)
                     {
                     if (prevTempTree)
                        {
                        prevTempTree->setNextElement(nextTempTree->getNextElement());
                        }
                     else
                        {
                        trees->setListHead(nextTempTree->getNextElement());
                        }

                     if (nextTempTree == prevTree)
                        prevTree = prevTempTree;

                     foundInvariantTrees = false;
                     *containsNonInlineGuard = false;
                     break;
                     }
                  prevTempTree = nextTempTree;
                  }
               }
            }
         }
#ifdef J9_PROJECT_SPECIFIC
      else if (!highlyBiasedBranch && comp()->hasIntStreamForEach())
         {
         TR::Block *nextBlock=nextTree->getData()->getEnclosingBlock();
         TR::Block *targetBlock=node->getBranchDestination()->getNode()->getBlock();
         if((targetBlock->isCold() && nextBlock->getNextBlock()->isCold()) || (!targetBlock->isCold() && !nextBlock->getNextBlock()->isCold()))
            includeComparisonTree = false;
         }
#endif
      bool isTreeInvariant = false;
      if (includeComparisonTree)
         {
         isTreeInvariant = true;

         if (onlyDetectHighlyBiasedBranches
             && (thisChild || nextRealNode)
             && node->isTheVirtualGuardForAGuardedInlinedCall()
             && !node->isOSRGuard()
             && !node->isHCRGuard()
             && !node->isDirectMethodGuard()
             && !node->isBreakpointGuard())
            {
            if (!guardInfo)
               guardInfo = comp()->findVirtualGuardInfo(node);

            if (nextRealNode && !thisChild)
               {
               if (nextRealNode->getOpCode().isCallIndirect())
                  thisChild = nextRealNode->getSecondChild();
               else
                  thisChild = nextRealNode->getFirstChild();
               }

            if (guardInfo->getKind() == TR_DummyGuard ||
                !guardInfo->getInnerAssumptions().isEmpty())  // inner assumption may have receivers other than 'thisChild'
               isTreeInvariant = false;
            else
               {
               bool ignoreHeapificationStore = false;
               if (thisChild->getOpCode().hasSymbolReference() &&
                   thisChild->getSymbolReference()->getSymbol()->isAutoOrParm())
                  ignoreHeapificationStore = true;

               bool b = isExprInvariant(thisChild, ignoreHeapificationStore);
               if (!b)
                  {
                  if (!(thisChild->getOpCode().hasSymbolReference() &&
                        thisChild->getSymbolReference()->getSymbol()->isAuto() &&
                        isDependentOnInvariant(thisChild)))
                     {
                     isTreeInvariant = false;
                     }
                  else
                     {
                     if (!guardInfo)
                        guardInfo = comp()->findVirtualGuardInfo(node);

                     if (guardInfo->getTestType() == TR_VftTest)
                        {
                        if (node->getFirstChild()->getNumChildren() == 0)
                           isTreeInvariant = false;
                        }
                     else if (guardInfo->getTestType() == TR_MethodTest)
                        {
                        if ((node->getFirstChild()->getNumChildren() == 0) ||
                            (node->getFirstChild()->getFirstChild()->getNumChildren() == 0))
                           isTreeInvariant = false;
                        }
                     }
                  }
               }
            }
         else if (!node->isOSRGuard()
                  && !node->isHCRGuard()
                  && !node->isDirectMethodGuard()
                  && !node->isBreakpointGuard())
            {
            isTreeInvariant = areAllChildrenInvariant(node);
            if (!isTreeInvariant)
               {
               ExtremumConditional excond;
               if (isVersionableIfWithExtremum(nextTree->getData(), &excond))
                  {
                  isTreeInvariant = true;
                  if (excond.reverseBranch)
                     reverseBranchInLoops[node->getGlobalIndex()] = true;
                  }
               }
            }
         else
            isTreeInvariant = false;
         }

      if (isTreeInvariant && nodeToBeChecked)
         {
         isTreeInvariant = isExprInvariant(nodeToBeChecked, false);
         }

      if (!isTreeInvariant)
         {
         if (trace())
            {
            traceMsg(comp(), "Non invariant tree %p (%s)\n", node, node->getOpCode().getName());
            }
         if (prevTree)
            {
            prevTree->setNextElement(nextTree->getNextElement());
            }
         else
            {
            trees->setListHead(nextTree->getNextElement());
            }
         }
      else
         {
	 //if (highlyBiasedBranch)
	 //   printf("Detected a highly biased invariant branch in %s\n", comp()->signature());

         if (onlyDetectHighlyBiasedBranches ||
             (!node->getOpCode().isBranch()) ||
             (!onlyNonInlineGuardConditional))
             {
             if (trace())
                traceMsg(comp(), "Invariant tree %p (%s)\n", node, node->getOpCode().getName());

             foundInvariantTrees = true;
             prevTree = nextTree;
             if (!onlyDetectHighlyBiasedBranches &&
                 node->getOpCode().isBranch())
                {
		onlyNonInlineGuardConditional = nextTree->getData();
                *containsNonInlineGuard = true;
                }
             }
         else
            {
            if (!onlyDetectHighlyBiasedBranches &&
                 node->getOpCode().isBranch())
               {
               TR::Node *removedNode = NULL;

	       if (prevTree)
                  {
                  int32_t b1 = prevTree->getData()->getEnclosingBlock()->getNumber();
                  int32_t b2 = nextTree->getData()->getEnclosingBlock()->getNumber();
                  if (!_postDominators ||
                      _postDominators->numberOfBlocksControlled(b1) > _postDominators->numberOfBlocksControlled(b2))
                     {
                     prevTree->setNextElement(nextTree->getNextElement());
                     removedNode = nextTree->getData()->getNode();
                     }
                  else
		     {
                     if (trace())
                        traceMsg(comp(), "Keeping invariant branch  %p (%s) in block_%d\n", node, node->getOpCode().getName(), nextTree->getData()->getEnclosingBlock()->getNumber());
                     trees->setListHead(nextTree);
                     removedNode = prevTree->getData()->getNode();
                     prevTree = nextTree;

                     }
                  }
               else
                  {
                  trees->setListHead(nextTree->getNextElement());
                  }

               if (trace())
                   traceMsg(comp(), "Discarded invariant branch  %p (%s) \n", removedNode, removedNode->getOpCode().getName());

               }
            }
         }

      nextTree = nextTree->getNextElement();
      }

   return foundInvariantTrees;
   }



TR::Node *TR_LoopVersioner::isDependentOnInvariant(TR::Node *useNode)
   {
   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   if (!useDefInfo)
      return NULL;

   uint16_t useIndex = useNode->getUseDefIndex();
   if (!useIndex || !useDefInfo->isUseIndex(useIndex))
      return NULL;

   TR_UseDefInfo::BitVector defs(comp()->allocator());
   bool isNonZero = useDefInfo->getUseDef(defs, useIndex);
   TR_UseDefInfo::BitVector::Cursor cursor(defs);
   cursor.SetToFirstOne();

   //int32_t defnIndex=cursor;
   if(cursor<unsigned(useDefInfo->getFirstRealDefIndex()))
      return NULL;

   TR_ValueNumberInfo *valueNumberInfo = optimizer()->getValueNumberInfo();
   int32_t firstDefValueNumber = valueNumberInfo->getValueNumber(useDefInfo->getNode(cursor));
   if (trace())
      traceMsg(comp(),"Definition Counts for node [%p] is %d inside isDependentOnInvariant \n", useNode,defs.PopulationCount());
   TR::Node *childNode = NULL;
   if (isNonZero) //&&
       //(defs.PopulationCount() == 1) &&
       //_writtenExactlyOnce.ValueAt(useNode->getSymbolReference()->getReferenceNumber()))
      {
      TR_UseDefInfo::BitVector::Cursor cursor(defs);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t defIndex = cursor;
         if (defIndex < useDefInfo->getFirstRealDefIndex())
            return NULL;

         TR::Node *defNode = useDefInfo->getNode(defIndex);
         int32_t valueNumber = valueNumberInfo->getValueNumber(defNode);
         if(trace())
            traceMsg(comp(),"Definition node [%p] value number %d and firstValueNumber %d  \n", defNode,valueNumber,firstDefValueNumber);

         if(valueNumber != firstDefValueNumber)
            return NULL;

         TR::Node *child = defNode->getFirstChild();
         bool isChildInvariant = isExprInvariant(child);

         if (!isChildInvariant)
            return NULL;

         if (child &&
             child->getOpCode().hasSymbolReference())
            childNode = child;
         }
      }

   return childNode;
   }


TR::Node *TR_LoopVersioner::isDependentOnInductionVariable(
   TR::Node *useNode,
   bool noArithmeticAllowed,
   bool &isIndexChildMultiplied,
   TR::Node * &mulNode,
   TR::Node * &strideNode,
   bool &indVarOccursAsSecondChildOfSub,
   TR::TreeTop * &outDefTree)
   {
   outDefTree = NULL;

   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   if (!useDefInfo)
      return NULL;

   uint16_t useIndex = useNode->getUseDefIndex();
   if (!useIndex || !useDefInfo->isUseIndex(useIndex))
      return NULL;

   int32_t useSymRefNum = useNode->getSymbolReference()->getReferenceNumber();

   TR_UseDefInfo::BitVector defs(comp()->allocator());
   if (useDefInfo->getUseDef(defs, useIndex) &&
       (defs.PopulationCount() == 1) &&
       _writtenExactlyOnce.ValueAt(useSymRefNum))
      {
      TR_UseDefInfo::BitVector::Cursor cursor(defs);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t defIndex = cursor;
         if (defIndex < useDefInfo->getFirstRealDefIndex())
            return NULL;

         TR::Node *defNode = useDefInfo->getNode(defIndex);
         TR::TreeTop *defTree = _storeTrees[useSymRefNum];
         TR_ASSERT_FATAL_WITH_NODE(
            useNode,
            defNode == defTree->getNode(),
            "n%un [%p] def discrepancy: use-def n%un [%p] vs. _storeTrees n%un [%p]",
            useNode->getGlobalIndex(),
            useNode,
            defNode->getGlobalIndex(),
            defNode,
            defTree->getNode()->getGlobalIndex(),
            defTree->getNode());

         TR::Node *child = defNode->getFirstChild();
         bool goodAccess = isVersionableArrayAccess(child);
         while (!noArithmeticAllowed && (child->getOpCode().isAdd() || child->getOpCode().isSub() || child->getOpCode().isMul()))
            {
            if(child->getOpCode().isMul())
               {
               isIndexChildMultiplied=true;
               mulNode=child;
               }
            if (child->getSecondChild()->getOpCode().isLoadConst())
               {
               if(isIndexChildMultiplied)
                  strideNode=child->getSecondChild();
               child = child->getFirstChild();
               }
            else
               {
               bool isSecondChildInvariant = isExprInvariant(child->getSecondChild());
               if (isSecondChildInvariant)
                  {
                  if(isIndexChildMultiplied)
                     strideNode=child->getSecondChild();
                  child = child->getFirstChild();
                  }
               else
                  {
                  bool isFirstChildInvariant = isExprInvariant(child->getFirstChild());
                  if (isFirstChildInvariant)
                     {
                     if (child->getOpCode().isSub())
                        indVarOccursAsSecondChildOfSub = !indVarOccursAsSecondChildOfSub;

                     if(isIndexChildMultiplied)
                         strideNode=child->getFirstChild();
                     child = child->getSecondChild();
                     }
                  else
                     return NULL;
                  }
               }
            }

         if (child &&
             child->getOpCode().hasSymbolReference() && goodAccess)
            {
            outDefTree = defTree;
            return child;
            }
         }
      }

   return NULL;
   }

bool TR_LoopVersioner::isVersionableArrayAccess(TR::Node * indexChild)
   {

   bool mulNodeFound=false;
   bool addNodeFound=false;
   bool goodAccess=true;
   if(!indexChild->getOpCode().hasSymbolReference())
      {
      while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub() ||
             indexChild->getOpCode().isMul())
         {
         if(indexChild->getOpCode().isSub())
           goodAccess = false;
         else if(indexChild->getOpCode().isMul())
            {
            if(mulNodeFound)
              goodAccess= false;
            mulNodeFound=true;
            }
         else if(indexChild->getOpCode().isAdd())
            {
            if(addNodeFound)
               goodAccess= false;
             addNodeFound=true;
            }
         if (indexChild->getSecondChild()->getOpCode().isLoadConst())
            {
            indexChild = indexChild->getFirstChild();
            }
         else
            {
            bool isSecondChildInvariant = isExprInvariant(indexChild->getSecondChild());
            if (isSecondChildInvariant)
               {
               indexChild = indexChild->getFirstChild();
               }
            else
               {
               bool isFirstChildInvariant = isExprInvariant(indexChild->getFirstChild());
               if (isFirstChildInvariant)
                  {
                  indexChild = indexChild->getSecondChild();
                  }
               else
                  return false;
               }
            }
        }
     }
   if(mulNodeFound)
      return goodAccess;

   return true;
   }


bool TR_LoopVersioner::detectInvariantBoundChecks(List<TR::TreeTop> *boundCheckTrees)
   {
   bool foundInvariantChecks = false;
   ListElement<TR::TreeTop> *nextTree = boundCheckTrees->getListHead();
   ListElement<TR::TreeTop> *prevTree = NULL;

   for (;nextTree;)
      {
      TR::TreeTop *checkTree = nextTree->getData();
      TR::Node *node = checkTree->getNode();

      TR::Node *boundNode =
         (node->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? node->getChild(2) : node->getFirstChild();

      bool isInductionVariable = false, isIndexInvariant = false;

      bool isMaxBoundInvariant = isExprInvariant(boundNode);
      if (!isMaxBoundInvariant)
         {
         if (boundNode->getOpCode().isArrayLength())
            {
            TR::Node *arrayObject = boundNode->getFirstChild();
            if (arrayObject->getOpCode().hasSymbolReference() &&
                arrayObject->getSymbolReference()->getSymbol()->isAuto() &&
                isDependentOnInvariant(arrayObject))
               isMaxBoundInvariant = true;
            }
         else
            {
            if (boundNode->getOpCode().hasSymbolReference() &&
                boundNode->getSymbolReference()->getSymbol()->isAuto() &&
                isDependentOnInvariant(boundNode))
               isMaxBoundInvariant = true;
            }
         }

      TR::SymbolReference *indexSymRef = NULL;
      TR::Node *indexChild = NULL;
      TR::TreeTop *occurrenceTree = NULL;
      bool isLoopDrivingInductionVariable = false;
      bool isDerivedInductionVariable = false;
      bool isSpecialIv = false;
      if (isMaxBoundInvariant)
         {
         occurrenceTree = checkTree;
         indexChild =
            (node->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? node->getChild(3) : node->getSecondChild();

         isIndexInvariant = isExprInvariant(indexChild);
         if (!isIndexInvariant &&
             _loopConditionInvariant &&
             (node->getOpCodeValue() == TR::BNDCHK || node->getOpCodeValue() == TR::BNDCHKwithSpineCHK))
            {
            bool isIndexChildMultiplied = false;
            if (indexChild->getOpCode().hasSymbolReference())
               indexSymRef = indexChild->getSymbolReference();
            else
               {
               bool goodAccess = isVersionableArrayAccess(indexChild);
               while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub() || indexChild->getOpCode().isAnd() ||
                      indexChild->getOpCode().isMul())
                  {
                  if (indexChild->getOpCode().isMul())
                     isIndexChildMultiplied = true;

                  if (indexChild->getSecondChild()->getOpCode().isLoadConst())
                     indexChild = indexChild->getFirstChild();
                  else
                     {
                     bool isSecondChildInvariant = isExprInvariant(indexChild->getSecondChild());
                     if (isSecondChildInvariant)
                        indexChild = indexChild->getFirstChild();
                     else
                        {
                        bool isFirstChildInvariant = isExprInvariant(indexChild->getFirstChild());
                        if (isFirstChildInvariant)
                           indexChild = indexChild->getSecondChild();
                        else
                           break;
                        }
                     }
                  }

               //dumpOptDetails(comp(), "indexChild %p for BNDCHK node %p\n", indexChild, node);

               if (indexChild &&
                   indexChild->getOpCode().hasSymbolReference() && goodAccess)
                  indexSymRef = indexChild->getSymbolReference();
               }

            //dumpOptDetails(comp(), "BNDCHK node %p\n", node);
            bool changedIndexSymRef = true;
            bool changedIndexSymRefAtSomePoint = false;
            while (indexSymRef &&
                   changedIndexSymRef)
               {
               changedIndexSymRef = false;
               int32_t symRefNum = indexSymRef->getReferenceNumber();
               ListElement<int32_t> *versionableInductionVar = _versionableInductionVariables.getListHead();
               while (versionableInductionVar)
                  {
                  //dumpOptDetails(comp(), "Versionable induction var %d\n", *(versionableInductionVar->getData()));
                  if (symRefNum == *(versionableInductionVar->getData()))
                     {
                     isInductionVariable = true;
                     isLoopDrivingInductionVariable = true;
                     break;
                     }
                  versionableInductionVar = versionableInductionVar->getNextElement();
                  }

               if (!isInductionVariable)
                  {
                  versionableInductionVar = _specialVersionableInductionVariables.getListHead();
                  while (versionableInductionVar)
                     {
                     if (symRefNum == *(versionableInductionVar->getData()))
                        {
                        isInductionVariable = true;
                        isSpecialIv = true;
                        break;
                        }
                     versionableInductionVar = versionableInductionVar->getNextElement();
                     }
                  }

               if (!isInductionVariable &&
                   !_versionableInductionVariables.isEmpty())
                  {
                  versionableInductionVar = _derivedVersionableInductionVariables.getListHead();
                  while (versionableInductionVar)
                     {

                     if (symRefNum == *(versionableInductionVar->getData()))
                        {
                        isDerivedInductionVariable = true;
                        isInductionVariable = true;
                        break;
                        }
                     versionableInductionVar = versionableInductionVar->getNextElement();
                     }
                  }

               if (!isInductionVariable)
                  {
                  if (isIndexChildMultiplied)
                     {
                     // Don't look through temps beneath a multiplication. The
                     // versioning test to rule out overflow in multiplication
                     // would generate a load of the temporary, which before
                     // entering the loop has a value unrelated to its value in
                     // the index expression and might even be uninitialized.
                     break;
                     }

                  TR::Node *mulNode=NULL;
                  TR::Node *strideNode=NULL;
                  bool indVarOccursAsSecondChildOfSub=false;
                  TR::TreeTop *defTree = NULL;
                  indexChild = isDependentOnInductionVariable(
                     indexChild,
                     changedIndexSymRefAtSomePoint,
                     isIndexChildMultiplied,
                     mulNode,
                     strideNode,
                     indVarOccursAsSecondChildOfSub,
                     defTree);
                  if (!indexChild ||
                      !indexChild->getOpCode().hasSymbolReference() ||
                      !indexChild->getSymbolReference()->getSymbol()->isAutoOrParm() ||
                      (indexChild->getSymbolReference()->getReferenceNumber() == indexSymRef->getReferenceNumber()))
                     {
                     break;
                     }
                  else
                     {
                     indexSymRef = indexChild->getSymbolReference();
                     changedIndexSymRef = true;
                     changedIndexSymRefAtSomePoint = true;
                     occurrenceTree = defTree;
                     }
                  }
               }
            }
         }

      if (isInductionVariable && indexSymRef)
         {
         if (!isSpecialIv && !ivLoadSeesUpdatedValue(indexChild, occurrenceTree))
            _unchangedValueUsedInBndCheck->set(node->getGlobalIndex());

         // check for multiple loop exits when versioning a derived iv
         //
         if (isDerivedInductionVariable && !_hasPredictableExits->get(indexSymRef->getReferenceNumber()))
            {
            isInductionVariable = false;
            isIndexInvariant = false;
            ///traceMsg(comp(), "marking node %p as non-invariant\n", node);
            }
         }

      if (isIndexInvariant &&
         _checksInDupHeader.find(checkTree))
         isIndexInvariant = false;

      if (!isIndexInvariant &&
          !isInductionVariable)
         {
         if (trace())
            traceMsg(comp(), "Non invariant Bound check reference %p (%s)\n", node, node->getOpCode().getName());
         if (prevTree)
            {
            prevTree->setNextElement(nextTree->getNextElement());
            }
         else
            {
            boundCheckTrees->setListHead(nextTree->getNextElement());
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Invariant Bound check reference %p (%s)\n", node, node->getOpCode().getName());
         foundInvariantChecks = true;
         prevTree = nextTree;
         }

      nextTree = nextTree->getNextElement();
      }

   return foundInvariantChecks;
   }


bool TR_LoopVersioner::detectInvariantSpineChecks(List<TR::TreeTop> *spineCheckTrees)
   {
   bool foundInvariantChecks = false;
   ListElement<TR::TreeTop> *nextTree = spineCheckTrees->getListHead();
   ListElement<TR::TreeTop> *prevTree = NULL;

   for (;nextTree;)
      {
      TR::Node *node = nextTree->getData()->getNode();
      TR::Node *arrayObject = node->getChild(1);
      bool isArrayInvariant = isExprInvariant(arrayObject);
//printf("isArrayInvariant is %d\n",isArrayInvariant);fflush(stdout);
      if (!isArrayInvariant)
         {
	if (arrayObject->getOpCode().hasSymbolReference() &&
           arrayObject->getSymbolReference()->getSymbol()->isAuto() &&
            isDependentOnInvariant(arrayObject))
           isArrayInvariant = true;
   	}

        if (!isArrayInvariant)
          {
//printf("Found A not invariant\n");fflush(stdout);
         if (trace())
            traceMsg(comp(), "Non invariant Spine check reference %p (%s)\n", node, node->getOpCode().getName());
         if (prevTree)
            {
            prevTree->setNextElement(nextTree->getNextElement());
            }
         else
            {
            spineCheckTrees->setListHead(nextTree->getNextElement());
            }
         }
      else
         {
//printf("Found A invariant\n");fflush(stdout);
         if (trace())
            traceMsg(comp(), "Invariant Spine check reference %p (%s)\n", node, node->getOpCode().getName());
         foundInvariantChecks = true;
         prevTree = nextTree;
         }

      nextTree = nextTree->getNextElement();


	}


   return foundInvariantChecks;
   }



bool TR_LoopVersioner::detectInvariantDivChecks(List<TR::TreeTop> *divideCheckTrees)
   {
   bool foundInvariantChecks = false;
   ListElement<TR::TreeTop> *nextTree = divideCheckTrees->getListHead();
   ListElement<TR::TreeTop> *prevTree = NULL;

   for (;nextTree;)
      {
      TR::Node *node = nextTree->getData()->getNode();
      bool isInductionVariable = false, isDivisorInvariant = false;
      if ((node->getFirstChild()->getOpCodeValue() == TR::idiv) ||
          (node->getFirstChild()->getOpCodeValue() == TR::irem) ||
          (node->getFirstChild()->getOpCodeValue() == TR::ldiv) ||
          (node->getFirstChild()->getOpCodeValue() == TR::lrem))
         {
         TR::Node *divisorChild = node->getFirstChild()->getSecondChild();

         isDivisorInvariant = isExprInvariant(divisorChild);
         if (isDivisorInvariant)
            {
            if (_checksInDupHeader.find(nextTree->getData()))
               isDivisorInvariant = false;
            }

         if (!isDivisorInvariant)
            {
            /*
            TR::SymbolReference *indexSymRef = NULL;
            if (divisorChild->getOpCode().hasSymbolReference())
               indexSymRef = divisorChild->getSymbolReference();
            else
               {
               while ((divisorChild->getOpCode().isAdd() || divisorChild->getOpCode().isSub()) &&
                      divisorChild->getSecondChild()->getOpCode().isLoadConst())
                  divisorChild = divisorChild->getFirstChild();

               if (divisorChild &&
                   divisorChild->getOpCode().hasSymbolReference())
                  indexSymRef = divisorChild->getSymbolReference();
               }

            if (indexSymRef)
               {
               int32_t symRefNum = indexSymRef->getReferenceNumber();
               ListElement<int32_t> *versionableInductionVar = _versionableInductionVariables.getListHead();
               while (versionableInductionVar)
                  {
                  if (symRefNum == *(versionableInductionVar->getData()))
                     {
                     isInductionVariable = true;
                     break;
                     }
                  versionableInductionVar = versionableInductionVar->getNextElement();
                  }
               }
            */
            }
         }


      if (!isDivisorInvariant &&
          !isInductionVariable)
         {
         if (trace())
            traceMsg(comp(), "Non invariant Div check reference %p (%s)\n", node, node->getOpCode().getName());

         if (prevTree)
            {
            prevTree->setNextElement(nextTree->getNextElement());
            }
         else
            {
            divideCheckTrees->setListHead(nextTree->getNextElement());
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Invariant Div check reference %p (%s)\n", node, node->getOpCode().getName());
         foundInvariantChecks = true;
         prevTree = nextTree;
         }

      nextTree = nextTree->getNextElement();
      }

   return foundInvariantChecks;
   }

bool TR_LoopVersioner::isDependentOnAllocation(TR::Node *useNode, int32_t recursionDepth)
   {
   if (recursionDepth < 0)
      return false;

   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   if (!useDefInfo)
      return false;

   uint16_t useIndex = useNode->getUseDefIndex();
   if (!useIndex || !useDefInfo->isUseIndex(useIndex))
      return false;


   TR_UseDefInfo::BitVector defs(comp()->allocator());
   if (useDefInfo->getUseDef(defs, useIndex))
      {
      bool pointsToNew = false;
      bool pointsToNonNew = false;

      TR_UseDefInfo::BitVector::Cursor cursor(defs);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t defIndex = cursor;
         if (defIndex == 0)
            return false;

         TR::Node *defNode = useDefInfo->getNode(defIndex);
         TR::Node *child = defNode->getFirstChild();
         if (trace())
            {
            traceMsg(comp(), "use %p child %p def %p rec %d\n", useNode, child, defNode, recursionDepth);
            traceMsg(comp(), "new %d non new %d\n", pointsToNew, pointsToNonNew);
            }

        bool heapificationStore = defNode->getOpCodeValue() == TR::astore && defNode->isHeapificationStore();

         if (heapificationStore ||
             (child->getOpCode().isNew()) ||
             ((child->getOpCodeValue() == TR::loadaddr) &&
              child->getSymbol()->isLocalObject()))
            pointsToNew = true;
         else
            {
            if (recursionDepth > 0)
               {
               bool isChildInvariant = isExprInvariant(child, true);
               if (isDependentOnAllocation(child, recursionDepth-1))
                  pointsToNew = true;
               }
            else
               pointsToNew = false;
            }

         if (trace())
            traceMsg(comp(), "new %d non new %d\n", pointsToNew, pointsToNonNew);

         if (!pointsToNew)
            {
            TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);
            TR::Block *defBlock = defTree->getEnclosingBlock();
            if (!defBlock->isCold() && (defBlock->getFrequency() > 6))
               {
               pointsToNonNew = true;
               break;
               }
            }
         }

      if (trace())
         traceMsg(comp(), "final new %d non new %d\n", pointsToNew, pointsToNonNew);

      if (!pointsToNew || pointsToNonNew)
         return false;
      return true;
      }

   return false;
   }


bool TR_LoopVersioner::detectInvariantAwrtbaris(List<TR::TreeTop> *awrtbariTrees)
   {

   if (!awrtbariTrees->getListHead())
      return false;

#ifdef J9_PROJECT_SPECIFIC
   if (comp()->getOptions()->isVariableHeapBaseForBarrierRange0())
      {
      awrtbariTrees->deleteAll();
      return false;
      }

   uintptr_t nurseryBase, nurseryTop;
   comp()->fej9()->getNurserySpaceBounds(&nurseryBase, &nurseryTop);
   //printf("nursery base %p nursery top %p\n", nurseryBase, nurseryTop);
   if ((nurseryBase == 0) || (nurseryTop == 0))
      {
      awrtbariTrees->deleteAll();
      return false;
      }

   uintptr_t stackCompareValue = comp()->getOptions()->getHeapBase();
   if (stackCompareValue == 0)
     {
     awrtbariTrees->deleteAll();
     return false;
     }

   bool foundInvariantChecks = false;
   ListElement<TR::TreeTop> *nextTree = awrtbariTrees->getListHead();
   ListElement<TR::TreeTop> *prevTree = NULL;

   for (;nextTree;)
      {
      TR::Node *node = nextTree->getData()->getNode();
      bool isBaseInvariant = false;
      if (node->getOpCodeValue() != TR::awrtbari)
        node = node->getFirstChild();

      if (trace())
         traceMsg(comp(), "base invariant 0 in %p\n", node);

      if (node->getOpCodeValue() == TR::awrtbari)
         {
         if (trace())
            traceMsg(comp(), "base invariant 1 in %p\n", node);
         //printf("base invariant 1 in %s\n", comp()->signature());
         TR::Node *baseChild = node->getLastChild();
         if (baseChild->getOpCode().hasSymbolReference() &&
             (baseChild->getOpCodeValue() == TR::aload) &&
             baseChild->getSymbol()->isAutoOrParm())
            {
            isBaseInvariant = isExprInvariant(baseChild, true);
            if (trace())
               traceMsg(comp(), "base invariant 11 in %p inv %d\n", node, isBaseInvariant);

            if (isBaseInvariant)
               {
               if (_checksInDupHeader.find(nextTree->getData()))
                  {
                  isBaseInvariant = false;
                  }
               else
                  {
                  if (trace())
                     traceMsg(comp(), "base invariant 0 in %p\n", baseChild);
                  //
                  // recursionDepth cannot be changed to > 1 because isExprInvariant used inside
                  // isDependentOnAllocation may not give the right answer if the expr is outside the loop
                  // isExprInvariant is relevant mostly in the context of a loop is my worry
                  //
                  isBaseInvariant = isDependentOnAllocation(baseChild, 1);
                  }
               }
            }
         else
           isBaseInvariant = false;
         }


      if (!isBaseInvariant)
         {
         if (trace())
            traceMsg(comp(), "Non invariant awrtbari %p (%s)\n", node, node->getOpCode().getName());

         if (prevTree)
            {
            prevTree->setNextElement(nextTree->getNextElement());
            }
         else
            {
            awrtbariTrees->setListHead(nextTree->getNextElement());
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Invariant awrtbari %p (%s)\n", node, node->getOpCode().getName());
         foundInvariantChecks = true;
         prevTree = nextTree;
         }

      nextTree = nextTree->getNextElement();
      }

   return foundInvariantChecks;
#else
   return false;
#endif

   }

bool opCodeIsHoistable(TR::Node *node, TR::Compilation *comp)
   {
      bool hasEffects = 0;
      TR::ILOpCode &opCode = node->getOpCode();
      TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
      TR::SymbolReference *symReference = node->getSymbolReference();

      // CAREFUL: This should probably be an allowlist of side-effect free operators. If we move effectful stuff, things
      // may explode. Boom is bad.
      hasEffects = (opCode.isCall() || opCodeValue == TR::New || opCodeValue == TR::newarray || opCodeValue == TR::anewarray ||
                    opCodeValue == TR::multianewarray) || symReference->isUnresolved() ||
                    symReference->getSymbol()->isLocalObject() || (symReference->getSymbol()->isArrayShadowSymbol() && comp->requiresSpineChecks());
      return !hasEffects;
   }

bool TR_LoopVersioner::isExprInvariant(TR::Node *node, bool ignoreHeapificationStore)
   {
   _visitedNodes.empty();
   return isExprInvariantRecursive(node, ignoreHeapificationStore);
   }

bool TR_LoopVersioner::areAllChildrenInvariant(TR::Node *node, bool ignoreHeapificationStore)
   {
   _visitedNodes.empty();
   return areAllChildrenInvariantRecursive(node, ignoreHeapificationStore);
   }

bool TR_LoopVersioner::isExprInvariantRecursive(TR::Node *node, bool ignoreHeapificationStore)
   {
   static const bool paranoid = feGetEnv("TR_paranoidVersioning") != NULL;

   // Do not attempt to privatize BCD type nodes because doing so can result in creating direct loads of BCD types which are currently
   // not handled correctly within Java. BCDCHK IL will attempt to recreate a call to the original Java API for the corresponding BCD
   // IL and it needs to be able to materialize the node representing the original byte array object. This is not possible if the byte
   // array is stored on the stack.
#ifdef J9_PROJECT_SPECIFIC
   if (node->getType().isBCD())
      return false;
#endif

   if (paranoid && requiresPrivatization(node))
      return false;

   if (_visitedNodes.isSet(node->getGlobalIndex()))
      return true;

   _visitedNodes.set(node->getGlobalIndex()) ;

   TR::ILOpCode &opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      if (suppressInvarianceAndPrivatization(symReference))
         return false;

      if ((_seenDefinedSymbolReferences->get(symReference->getReferenceNumber()) &&
           (!ignoreHeapificationStore ||
            _writtenAndNotJustForHeapification->get(symReference->getReferenceNumber()))) ||
          !opCodeIsHoistable(node, comp()) /* ||
         (!symReference->getSymbol()->isAutoOrParm() && (comp()->getMethodHotness() <= warm) &&
         !comp()->getSymRefTab()->isImmutable(symReference)) */)
         return false;
      }

   return areAllChildrenInvariantRecursive(node, ignoreHeapificationStore);
   }

bool TR_LoopVersioner::areAllChildrenInvariantRecursive(TR::Node *node, bool ignoreHeapificationStore)
   {
   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      {
      if (!isExprInvariantRecursive(node->getChild(i), ignoreHeapificationStore))
         return false;
      }

   return true;
   }

bool TR_LoopVersioner::hasWrtbarBeenSeen(List<TR::TreeTop> *awrtbariTrees, TR::Node *awrtbariNode)
   {
   ListElement<TR::TreeTop> *nextTree = awrtbariTrees->getListHead();
   for (;nextTree;)
      {
      TR::Node *node = nextTree->getData()->getNode();

      if (node->getOpCodeValue() != TR::awrtbari)
        node = node->getFirstChild();

      if (trace())
         traceMsg(comp(), "base invariant 0 in %p\n", node);

      if (node->getOpCodeValue() == TR::awrtbari)
         {
         if (node == awrtbariNode)
           return true;
         }

      nextTree = nextTree->getNextElement();
      }

   return false;
   }


bool TR_LoopVersioner::canPredictIters(TR_RegionStructure* whileLoop,
   const TR_ScratchList<TR::Block>& blocksInWhileLoop,
   bool& isIncreasing, TR::SymbolReference*& firstChildSymRef)
   {
   bool returnValue = false;
   isIncreasing = false;

   if (((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmple) ||
         (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmplt)))
      {
      returnValue = true;
      isIncreasing = blocksInWhileLoop.find(_loopTestTree->getNode()->getBranchDestination()->getNode()->getBlock());
      }

   if (((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpge) ||
         (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpgt)))
      {
      returnValue = true;
      isIncreasing = !blocksInWhileLoop.find(_loopTestTree->getNode()->getBranchDestination()->getNode()->getBlock());
      }

   if (!whileLoop->containsOnlyAcyclicRegions())
      returnValue = false;

   firstChildSymRef = NULL;
   if (returnValue)
      {
      returnValue = false;
      TR::Node *firstChild = _loopTestTree->getNode()->getFirstChild();

      if (firstChild->getOpCode().hasSymbolReference())
         firstChildSymRef = firstChild->getSymbolReference();
      else
         {
         if ((firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub()) &&
            firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
            firstChild = firstChild->getFirstChild();
            }

         if (firstChild &&
            firstChild->getOpCode().hasSymbolReference())
            firstChildSymRef = firstChild->getSymbolReference();
         }

      if (firstChildSymRef)
         {
         int32_t loopDrivingInductionVar = -1;
         if (!_versionableInductionVariables.isEmpty())
            loopDrivingInductionVar = *(_versionableInductionVariables.getListHead()->getData());
         if (firstChildSymRef->getReferenceNumber() == loopDrivingInductionVar)
            {
            bool isLoopDrivingAddition = false;
            if (_additionInfo->get(loopDrivingInductionVar))
               isLoopDrivingAddition = true;

            if ((isLoopDrivingAddition && isIncreasing) ||
               (!isLoopDrivingAddition && !isIncreasing))
               returnValue = true;
            }
         }
      }
   return returnValue;
   }

int32_t *computeCallsiteCounts(TR_ScratchList<TR::Block> *loopBlocks, TR::Compilation *comp)
   {
   int32_t inlinedCount = comp->getNumInlinedCallSites();
   int32_t *callIndexCounts = (int32_t*)comp->trMemory()->allocateHeapMemory(sizeof(int32_t) * (inlinedCount + 1));
   for (int32_t i = 0; i < inlinedCount + 1; ++i)
      {
      callIndexCounts[i] = 0;
      }

   // compute the count for each inlined method
      ListIterator<TR::Block> loopItr(loopBlocks);
   int32_t treeTopCount = 0;
   const bool countAllocationFenceNodes = comp->getOption(TR_EnableLoopVersionerCountAllocationFences);
   for (TR::Block *block = loopItr.getCurrent(); block; block=loopItr.getNext())
      {
      for (OMR::TreeTop *tt = block->getFirstRealTreeTop(); tt != block->getExit(); tt = tt->getNextTreeTop())
         {
         // Do not count allocation fences on PPC (or other weak memory platforms) because
         // it throws the treetop count relative to non-weak memory platforms where we may
         // have tuned our thresholds
         if (!countAllocationFenceNodes && (tt->getNode()->getOpCodeValue() == TR::allocationFence)) { continue; }
         int32_t callIndex = tt->getNode()->getInlinedSiteIndex();
         callIndexCounts[callIndex + 1]++;
         treeTopCount++;
         }
      }

   // now compute the transitive closure - each method's count is for the method itself and all the
   // methods it inlines
   int32_t *transitiveCounts = (int32_t*)comp->trMemory()->allocateHeapMemory(sizeof(int32_t) * (inlinedCount + 2));
   for (int32_t i = 0; i < inlinedCount + 2; ++i)
      {
      transitiveCounts[i] = 0;
      }

   for (int32_t i = 0; i < inlinedCount; ++i)
      {
      int32_t callIndex = i;
      while (callIndex > -1)
         {
         transitiveCounts[callIndex + 2] += callIndexCounts[i + 1];
         callIndex = comp->getInlinedCallSite(callIndex)._byteCodeInfo.getCallerIndex();
         }
      transitiveCounts[1] += callIndexCounts[i + 1];
      }
   transitiveCounts[0] = treeTopCount;
   return transitiveCounts;
   }

bool TR_LoopVersioner::checkProfiledGuardSuitability(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *guardNode, TR::SymbolReference *callSymRef, TR::Compilation *comp)
   {
   static const bool disableLoopCodeRatioCheck = feGetEnv("TR_DisableLoopCodeRatioCheck") != NULL;
   bool risky = false;
   if (comp->getMethodHotness() >= hot && callSymRef)
      {
      if (callSymRef->getSymbol()
          && callSymRef->getSymbol()->castToMethodSymbol()
          && callSymRef->getSymbol()->castToMethodSymbol()->isInterface()
#ifdef J9_PROJECT_SPECIFIC
          && comp->fej9()->maybeHighlyPolymorphic(comp, callSymRef->getOwningMethod(comp), callSymRef->getCPIndex(), callSymRef->getSymbol()->castToMethodSymbol()->getMethod())
#endif
          )
         {
         if (trace())
            {
            TR::MethodSymbol *method = callSymRef->getSymbol()->castToMethodSymbol();
            TR_ResolvedMethod *owningMethod = callSymRef->getOwningMethod(comp);
            int32_t len = method->getMethod()->classNameLength();
            char *s = TR::Compiler->cls.classNameToSignature(method->getMethod()->classNameChars(), len, comp);
            TR_OpaqueClassBlock *classOfMethod = comp->fe()->getClassFromSignature(s, len, owningMethod, true);
            traceMsg(comp, "Found profiled gaurd %p is on interface %s\n", guardNode, TR::Compiler->cls.classNameChars(comp, classOfMethod, len));
            }
         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "interfaceGuardCheck/(%s)", comp->signature()));
         int32_t *treeTopCounts = computeCallsiteCounts(loopBlocks, comp);
         float loopCodeRatio = (float)treeTopCounts[guardNode->getInlinedSiteIndex() + 2] / (float)treeTopCounts[0];
         if (trace())
            traceMsg(comp, "  Loop code ratio %d / %d = %.2f\n", treeTopCounts[guardNode->getInlinedSiteIndex() + 2], treeTopCounts[0], loopCodeRatio);
         if (disableLoopCodeRatioCheck || loopCodeRatio < 0.25)
            {
            if (trace())
               traceMsg(comp, "Skipping versioning of profiled guard %p because we found more than 2 JIT'd implementors at warm or above and the loop code ratio is too low\n", guardNode);
            risky = true;
            TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "profiledVersioning/unsuitableForVersioning/interfaceGuard/(%s)/bci=%d.%d", comp->signature(), guardNode->getByteCodeInfo().getCallerIndex(), guardNode->getByteCodeInfo().getByteCodeIndex()));
            }
         }
      else if (comp->getInlinedResolvedMethod(guardNode->getByteCodeInfo().getCallerIndex())->isSubjectToPhaseChange(comp))
        {
        if (trace())
           {
           traceMsg(comp, "Found profiled guard %p is for a method subject to phase change - skipping versioning\n", guardNode);
           }
        risky = true;
        }
      }
   return !risky;
   }
bool TR_LoopVersioner::isBranchSuitableToVersion(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *node, TR::Compilation *comp)
   {
   bool suitableForVersioning = true;

   float profiledGuardProbabilityThreshold = 0.98f;
   static char *profiledGuardProbabilityThresholdStr = feGetEnv("TR_ProfiledGuardVersioningThreshold");
   static char *disableProfiledGuardVersioning = feGetEnv("TR_DisableProfiledGuardVersioning");
   if (profiledGuardProbabilityThresholdStr)
      {
      profiledGuardProbabilityThreshold = ((float)atof(profiledGuardProbabilityThresholdStr));
      }

#ifdef J9_PROJECT_SPECIFIC
   if (node->isProfiledGuard()
       && !node->getBranchDestination()->getNode()->getBlock()->isCold())
       {
       bool isGetFieldCacheCounts = !strncmp(comp->signature(),"org/apache/solr/request/SimpleFacets.getFieldCacheCounts(Lorg/apache/solr/search/SolrIndexSearcher;Lorg/apache/solr/search/DocSet;Ljava/lang/String;IIIZLjava/lang/String;Ljava/lang/String;)Lorg/apache/solr/common/util/NamedList;",60);

       TR_InlinedCallSite &ics = comp->getInlinedCallSite(node->getByteCodeInfo().getCallerIndex());
       if (isGetFieldCacheCounts || disableProfiledGuardVersioning)
          {
          suitableForVersioning = false;
          }
       else if (comp->getInlinedCallerSymRef(node->getByteCodeInfo().getCallerIndex()))
          {
          TR_AddressInfo *valueInfo = static_cast<TR_AddressInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(ics._byteCodeInfo, comp, AddressInfo));

          if (valueInfo)
             {
             if (trace())
               traceMsg(comp, "Profiled guard probability %.2f for guard %p\n", valueInfo->getTopProbability(), node);
             if (valueInfo->getTopProbability() >= profiledGuardProbabilityThreshold)
                {
                suitableForVersioning = checkProfiledGuardSuitability(loopBlocks, node, comp->getInlinedCallerSymRef(node->getByteCodeInfo().getCallerIndex()), comp);
                }
             else
                {
                suitableForVersioning = false;
                }
             if (suitableForVersioning)
                {
                TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "profiledVersioning/suitableForVersioning/probability=%d", ((int32_t)(valueInfo->getTopProbability() * 100))));
                }
             else
                {
                TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "profiledVersioning/unsuitableForVersioning/probability=%d", ((int32_t)(valueInfo->getTopProbability() * 100))));
                }
             }
          else
             {
             TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "profiledVersioning/unsuitableForVersioning/noinfo"));
             suitableForVersioning = false;
             }
          }
       else
          {
          traceMsg(comp,"No callNode found for guard %p\n", node);
          }
       }
#endif

   return suitableForVersioning;
   }


bool TR_LoopVersioner::isBranchSuitableToDoLoopTransfer(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *node, TR::Compilation *comp)
   {
   if (comp->getMethodHotness() <= warm || comp->isProfilingCompilation())
      return false;

   return isBranchSuitableToVersion(loopBlocks, node, comp);
   }



bool TR_LoopVersioner::detectChecksToBeEliminated(TR_RegionStructure *whileLoop, List<TR::Node> *nullCheckedReferences, List<TR::TreeTop> *nullCheckTrees, List<int32_t> *numIndirections, List<TR::TreeTop> *boundCheckTrees, List<TR::TreeTop> *spineCheckTrees, List<int32_t> *numDimensions, List<TR::TreeTop> *conditionalTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *awrtbariTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *specializedInvariantNodes, List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodesList, bool &discontinue)
   {
   bool foundPotentialChecks = false;
   int32_t warmBranchCount = 0;

   TR_ScratchList<TR::Block> blocksInWhileLoop(trMemory());
   whileLoop->getBlocks(&blocksInWhileLoop);
   int32_t loop_size = blocksInWhileLoop.getSize();

   // Blocks are listed in order of a traversal over the tree of regions, i.e.
   // they are grouped by region. As we iterate, maintain the current innermost
   // region, and current innermost containing loop. The hottest intervening
   // loop header only has to be recomputed when the innermost loop changes.
   TR_RegionStructure *rememberedInnermostRegion = NULL;
   TR_RegionStructure *rememberedInnermostLoop = NULL;
   TR::Block *rememberedHotLoopHeader = NULL;

   ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      TR::TreeTop *entryTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();

      bool dupOfThisBlockAlreadyExecutedBeforeLoop = false;
      TR_BlockStructure *nextBlockStructure = nextBlock->getStructureOf();
      if (nextBlockStructure &&
          nextBlockStructure->wasHeaderOfCanonicalizedLoop() &&
          nextBlock->hasSuccessor(whileLoop->getEntryBlock()))
         dupOfThisBlockAlreadyExecutedBeforeLoop = true;

      bool isUnimportant = false;
      if (nextBlock->isCold())
         {
         isUnimportant = true;
         }
      else
         {
         static char *unimportantFrequencyRatioStr = feGetEnv("TR_loopVersionerControlUnimportantFrequencyRatio");
         int32_t unimportantFrequencyRatio = unimportantFrequencyRatioStr ? atoi(unimportantFrequencyRatioStr) : 20;
         int16_t blockFrequency = nextBlock->getFrequency();

         // If the aggressive loop versioning flag is set, only do the unimportant block test
         // at lower optlevels
         bool aggressive = comp()->getOption(TR_EnableAggressiveLoopVersioning);

         static const bool useOuterHeader =
            feGetEnv("TR_loopVersionerUseOuterHeaderForImportance") != NULL;

         TR::Block *hotBlock = NULL;
         if (useOuterHeader)
            {
            hotBlock = whileLoop->getEntryBlock();
            }
         else
            {
            // Find the hottest intervening loop header.
            TR_BlockStructure * const bstruct = nextBlock->getStructureOf();
            TR_RegionStructure * const region = bstruct->getParent();

            hotBlock = rememberedHotLoopHeader;
            if (region != rememberedInnermostRegion)
               {
               rememberedInnermostRegion = region;
               TR_RegionStructure *loop = bstruct->getContainingLoop();
               if (loop != rememberedInnermostLoop)
                  {
                  rememberedInnermostLoop = loop;
                  hotBlock = whileLoop->getEntryBlock();
                  while (loop != whileLoop)
                     {
                     TR::Block * const innerHeader = loop->getEntryBlock();
                     if (innerHeader->getFrequency() > hotBlock->getFrequency())
                        hotBlock = innerHeader;

                     loop = loop->getContainingLoop();
                     }

                  rememberedHotLoopHeader = hotBlock;
                  }
               }
            }

         const int16_t loopFrequency = hotBlock->getFrequency();
         if ( (!aggressive || comp()->getMethodHotness() <= warm)
              && blockFrequency >= 0 // frequency must be valid
              && blockFrequency < loopFrequency / unimportantFrequencyRatio
              && performTransformation(comp(), "%sDisregard unimportant block_%d frequency %d < %d from block_%d in loop %d\n",
                 OPT_DETAILS_LOOP_VERSIONER, nextBlock->getNumber(), blockFrequency, loopFrequency, hotBlock->getNumber(), whileLoop->getNumber())
            )
            {
            isUnimportant = true;
            if (trace() || comp()->getOption(TR_CountOptTransformations))
               {
               for (TR::TreeTop *tt = entryTree; isUnimportant && tt != exitTree; tt = tt->getNextTreeTop())
                  {
                  TR::Node *ttNode = tt->getNode();
                  TR::ILOpCode &op = ttNode->getOpCode();
                  if ( op.isCheck() || op.isCheckCast() ||
                       (op.isBranch() && ttNode->getNumChildren() >= 2 && (!tt->getNode()->getFirstChild()->getOpCode().isLoadConst() || !tt->getNode()->getSecondChild()->getOpCode().isLoadConst())))
                     {
                     if (!performTransformation(comp(), "%s ...disregarding %s n%dn\n", OPT_DETAILS_LOOP_VERSIONER, ttNode->getOpCode().getName(), ttNode->getGlobalIndex()))
                        isUnimportant = false;
                     }
                  }
               }
            }
         }

      bool collectProfiledExprs = true;
      if(isUnimportant)
         collectProfiledExprs = false;

      recordCurrentBlock(nextBlock); // set for detectAliasRefinementOpportunties

      TR::TreeTop *currentTree = entryTree->getNextTreeTop();
      while (!(currentTree == exitTree))
         {
         TR::Node *currentNode = currentTree->getNode();
         TR::ILOpCode currentOpCode = currentNode->getOpCode();

         _containsCall = false;
         _nullCheckReference = NULL;
         _inNullCheckReference = false;

         if (currentOpCode.isNullCheck())
            _nullCheckReference = currentNode->getNullCheckReference();

         if (currentOpCode.getOpCodeValue() == TR::asynccheck)
            {
            _asyncCheckTree = currentTree;

            if (_loopTestTree &&
            (_loopTestTree->getNode()->getNumChildren() > 1) &&
               _asyncCheckTree &&
               shouldOnlySpecializeLoops())
               {
               bool isIncreasing;
               TR::SymbolReference* firstChildSymRef;
               bool _canPredictIters = canPredictIters(whileLoop, blocksInWhileLoop, isIncreasing, firstChildSymRef);

               if (_canPredictIters)
                  {
                  int32_t numIters = whileLoop->getEntryBlock()->getFrequency();
                  //if (numIters > 0.90*comp()->getRecompilationInfo()->getMaxBlockCount())
                  if (numIters < 0.90*(MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT))
                     _asyncCheckTree = NULL;
                  }
               else
                  _asyncCheckTree = NULL;
               }
            }
         vcount_t visitCount = comp()->incVisitCount(); //@TODO: unsafe API/use pattern
         updateDefinitionsAndCollectProfiledExprs(NULL, currentNode, visitCount, specializedInvariantNodes, invariantNodes, invariantTranslationNodesList, NULL, collectProfiledExprs, nextBlock, warmBranchCount);

         bool disableLT = comp()->getOption(TR_DisableLoopTransfer);
         if (!disableLT &&
             currentNode->isTheVirtualGuardForAGuardedInlinedCall() &&
             blocksInWhileLoop.find(currentNode->getBranchDestination()->getNode()->getBlock()))
            {
            _loopTransferDone = true;
            if (!_containsUnguardedCall)
               _containsGuard = true;
            }


         if ((currentNode->getNumChildren() > 0) &&
             currentNode->getFirstChild()->getOpCode().isCall())
            {
            if (!currentNode->getFirstChild()->isTheVirtualCallNodeForAGuardedInlinedCall())
               {
               _containsUnguardedCall = true;
               _containsGuard = false;
               }
            else if(currentNode->getFirstChild()->isTheVirtualCallNodeForAGuardedInlinedCall())
               {
               TR::CFG *cfg = comp()->getFlowGraph();
               TR::Block *guard = nextBlock->findVirtualGuardBlock(cfg);
               if(!guard)
                  {
                  //printf("\t\tnode %p (nextblock = %p) is hte virtual call node for a guarded inlined call without a guard.\n",currentNode,nextBlock);
                  _containsUnguardedCall = true;
                  _containsGuard = false;
                  }
               }
            }

         if (!isUnimportant &&
             !_containsCall && !refineAliases())
            {
            if (currentOpCode.isNullCheck())
               {
               if (trace())
                  traceMsg(comp(), "Null check reference %p (%s)\n", _nullCheckReference, _nullCheckReference->getOpCode().getName());

               nullCheckedReferences->add(_nullCheckReference);
               nullCheckTrees->add(currentTree);

               if (dupOfThisBlockAlreadyExecutedBeforeLoop)
             _checksInDupHeader.add(currentTree);

               foundPotentialChecks = true;
               }
             else if (currentOpCode.isBndCheck()
                      && currentOpCode.getOpCodeValue() != TR::arraytranslateAndTest)
               {
               if (trace())
                  traceMsg(comp(), "Bound check %p\n", currentTree->getNode());

               boundCheckTrees->add(currentTree);

               if (dupOfThisBlockAlreadyExecutedBeforeLoop)
                  _checksInDupHeader.add(currentTree);

               foundPotentialChecks = true;
               }
             else if (currentOpCode.getOpCodeValue() == TR::SpineCHK)
               {
               if (trace())
                  traceMsg(comp(), "Spine check %p\n", currentTree->getNode());
               spineCheckTrees->add(currentTree);

               if (dupOfThisBlockAlreadyExecutedBeforeLoop)
                  _checksInDupHeader.add(currentTree);
               foundPotentialChecks = true;
               }
             else if (currentOpCode.isBranch() &&
                     isBranchSuitableToVersion(&blocksInWhileLoop, currentNode, comp()) &&
                     (currentOpCode.getOpCodeValue() != TR::Goto))
               {
               if(isConditionalTreeCandidateForElimination(currentTree))
                  {
                  bool postDominatesEntry = false;
                  if (_postDominators)
                     {
                     postDominatesEntry = (_postDominators->dominates(nextBlock, whileLoop->getEntryBlock()) != 0);
                     if (postDominatesEntry)
                        {
                        dumpOptDetails(comp(), "   block_%d post dominates loop entry\n", nextBlock->getNumber());
                        nextBlock->setIsPRECandidate(true);
                        }

                     if (nextBlock->getFrequency() == whileLoop->getEntryBlock()->getFrequency())
                        dumpOptDetails(comp(), "   the same frequency as entry block\n");
                     }

                  if (trace())
                     {
                     traceMsg(comp(), "Conditional %p \n", currentTree->getNode());
                     if (_postDominators)
                        {
                        traceMsg(comp(), "    controls %d out of %d blocks\n",
                                        _postDominators->numberOfBlocksControlled(nextBlock->getNumber()),
                                        loop_size);

                        traceMsg(comp(), "    post dominates loop entry: %s\n",
                                         postDominatesEntry ? "yes" : "no");
                        }
                     }
                  conditionalTrees->add(currentTree);
                  }
               }
            else if (currentOpCode.getOpCodeValue() == TR::DIVCHK)
               {
               if (trace())
                  traceMsg(comp(), "DIVCHK %p\n", currentTree->getNode());

               divCheckTrees->add(currentTree);

               if (dupOfThisBlockAlreadyExecutedBeforeLoop)
             _checksInDupHeader.add(currentTree);
               }
            else if (currentOpCode.isCheckCast()) // (currentOpCode.getOpCodeValue() == TR::checkcast)
               {
               if (currentNode->getSecondChild()->getOpCodeValue() == TR::loadaddr)
	          {
                  void* clazz = currentNode->getSecondChild()->
                     getSymbolReference()->getSymbol()->getStaticSymbol()->getStaticAddress();
                  if (!comp()->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock*)clazz, comp()->getCurrentMethod()))
                     {
                     checkCastTrees->add(currentTree);

                     if (dupOfThisBlockAlreadyExecutedBeforeLoop)
                        _checksInDupHeader.add(currentTree);
                     }
                  else
                     {
                     if (trace()) //if we move the checkcast before the inlined body instanceof might fail because the class might not exist in the original code
                         traceMsg(comp(), "Class for Checkcast %p is not loaded by the same classloader as the compiled method\n", currentTree->getNode());
                     }
                  }
               else
                  {
                  checkCastTrees->add(currentTree);
                  if (dupOfThisBlockAlreadyExecutedBeforeLoop)
                     _checksInDupHeader.add(currentTree);
                  }
               }
            else if (currentOpCode.getOpCodeValue() == TR::ArrayStoreCHK)
               {
               if (trace())
                  traceMsg(comp(), "Array store check %p\n", currentTree->getNode());

               arrayStoreCheckTrees->add(currentTree);

               if (dupOfThisBlockAlreadyExecutedBeforeLoop)
             _checksInDupHeader.add(currentTree);
               }

            //SPECpower Work
            if (TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_cardmark_and_oldcheck ||
                TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_oldcheck)
               {
               TR::Node *possibleAwrtbariNode = currentTree->getNode();
               if ((currentOpCode.getOpCodeValue() != TR::awrtbari) &&
                  (possibleAwrtbariNode->getNumChildren() > 0))
                  possibleAwrtbariNode = possibleAwrtbariNode->getFirstChild();

               if ((possibleAwrtbariNode->getOpCodeValue() == TR::awrtbari) &&
                  !possibleAwrtbariNode->skipWrtBar() &&
                   !hasWrtbarBeenSeen(awrtbariTrees, possibleAwrtbariNode))
                  {
                  if (trace())
                     traceMsg(comp(), "awrtbari %p\n", currentTree->getNode());

                  awrtbariTrees->add(currentTree);

                  if (dupOfThisBlockAlreadyExecutedBeforeLoop)
                     _checksInDupHeader.add(currentTree);
                  }
               }
            }

         currentTree = currentTree->getNextTreeTop();
         }
      }

   return foundPotentialChecks;
   }

void TR_LoopVersioner::updateDefinitionsAndCollectProfiledExprs(TR::Node *parent, TR::Node *node, vcount_t visitCount, List<TR::Node> *profiledNodes,
                                                                List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodesList,
                                                                TR::Node *lastFault, bool collectProfiledExprs, TR::Block *block, int32_t warmBranchCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   // There will be no definitions or profiled expressions
   // beneath a nopable guard
   if (node->isNopableInlineGuard() || node->isOSRGuard())
      return;

   if(node->getOpCode().isIndirect() && refineAliases())
      collectArrayAliasCandidates(node,visitCount);

   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = node->getOpCodeValue();

   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      if (symReference->getSymbol()->isArrayShadowSymbol())
         _arrayAccesses->add(node);

      if (node->mightHaveVolatileSymbolReference())
         {
         if (symReference->sharesSymbol())
           {
           symReference->getUseDefAliases().getAliasesAndUnionWith(*_seenDefinedSymbolReferences);
           }
         else
            _seenDefinedSymbolReferences->set(symReference->getReferenceNumber());
         }
      }

   TR::Node *defNode = node;
   if (opCode.isResolveCheck())
      defNode = node->getFirstChild();

   if (defNode->isTheVirtualCallNodeForAGuardedInlinedCall() &&
       block->findVirtualGuardBlock(comp()->getFlowGraph()))
      defNode = NULL;   // old code was setting node's symref anyway, is it really needed?

   if (_guardedCalls.find(node))
      defNode = NULL;

   if (defNode)
      {
      TR_UseDefAliasSetInterface defAliases = defNode->mayKill();
      defAliases.getAliasesAndUnionWith(*_seenDefinedSymbolReferences);

      if (opCode.isStore())
         {
         bool heapificationStore = node->getOpCodeValue() == TR::astore && node->isHeapificationStore();
         if (!heapificationStore && _writtenAndNotJustForHeapification)
            defAliases.getAliasesAndUnionWith(*_writtenAndNotJustForHeapification); // TODO: avoid getting the same aliases twice
         }
      }

   //dumpOptDetails(comp(), "After examining node %p\n", node);
   //dumpOptDetails(comp(), "seenDefined is : "); _seenDefinedSymbolReferences->print(comp());
   if (node == _nullCheckReference)
      _inNullCheckReference = true;

   opCodeValue = opCode.getOpCodeValue();
   if (_inNullCheckReference || !_nullCheckReference)
      {
       if ((opCode.isCall()) || (opCodeValue == TR::New) || (opCodeValue == TR::newarray) || (opCodeValue == TR::anewarray) || (opCodeValue == TR::multianewarray))
         _containsCall = true;

      if (node->hasUnresolvedSymbolReference())
         _containsCall = true;

	if (_inNullCheckReference && comp()->requiresSpineChecks() &&
          node->getOpCode().hasSymbolReference() &&
          node->getSymbol()->isArrayShadowSymbol())
         _containsCall = true;
      }

   if (TR_LocalAnalysis::isSupportedOpCode(node->getOpCode(), comp()))
      {
      if (node->getType().isInt32() ||
          (node->getType().isInt64() &&
           node->getOpCode().isLoadVar() &&
           node->getSymbolReference()->getSymbol()->isAutoOrParm()))
          {
          if (collectProfiledExprs && !node->getType().isInt64())
             {
#ifdef J9_PROJECT_SPECIFIC
             TR_ValueInfo *valueInfo = static_cast<TR_ValueInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(node, comp(), ValueInfo));
             if (valueInfo)
                {
                if ((valueInfo->getTopProbability() > MIN_PROFILED_FREQUENCY) &&
                    (valueInfo->getTotalFrequency() > 0) &&
                    !_containsCall &&
                    !node->getByteCodeInfo().doNotProfile() && node->canChkNodeCreatedByPRE() && !node->isNodeCreatedByPRE() &&
                    // Only collect nodes from unspecialized blocks to avoid specializing the same nodes twice
                    !block->isSpecialized())
                   {
                   if (!node->getType().isInt64())
                      {

                     // Zero length contiguous array lengths mean further profiling is necessary
                     // to find the true array length.
                     //
                     if (comp()->requiresSpineChecks() && node->getOpCodeValue() == TR::contigarraylength && valueInfo->getTopValue() == 0)
                        {
                        dumpOptDetails(comp(), "From value profiling, NOT USING inconclusive contigarraylength node %p with value 0 freq %d total freq %d\n",
                           node, ((int32_t) (valueInfo->getTopProbability()*((float) valueInfo->getTotalFrequency()))), valueInfo->getTotalFrequency());
                        }
                     else
                        {
                        // Use the information for the long value if
                        // it can be changed to an int (high int value == 0)
                        //
                        /////if (debug("traceLVE"))
                        dumpOptDetails(comp(), "From value profiling, node %p has value %d freq %d total freq %d\n", node, valueInfo->getTopValue(), ((int32_t) (valueInfo->getTopProbability()*((float) valueInfo->getTotalFrequency()))), valueInfo->getTotalFrequency());
                        profiledNodes->add(node);
                        }

                      }
                   }
                }
#endif
             }
          }

      if (invariantNodes && collectProfiledExprs && !_containsCall &&
            ((node->getOpCode().getSize() >= 4) ||
             (opCodeValue == TR::computeCC)
             ) &&
            TR_LocalAnalysis::isSupportedNode(node, comp(), NULL) &&
            ((node->getOpCode().isLoadVar() &&
              !node->getSymbolReference()->getSymbol()->isAutoOrParm()) ||
             !node->getOpCode().hasSymbolReference()))
          {
          if (trace()) traceMsg(comp(), "Added invariant node %p %s\n", node, node->getOpCode().getName());
          invariantNodes->add(new (trStackMemory()) TR_NodeParentSymRef(node, parent, NULL));
          }
       }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      updateDefinitionsAndCollectProfiledExprs(node, node->getChild(i), visitCount, profiledNodes, invariantNodes, invariantTranslationNodesList, lastFault, collectProfiledExprs, block, warmBranchCount);


   if (node == _nullCheckReference)
     _inNullCheckReference = false;
   }

static void traceCannot(
   const char *what, TR::Node *culprit, TR::Compilation *comp)
   {
   dumpOptDetails(
      comp,
      "Cannot %s due to n%un [%p]\n",
      what,
      culprit->getGlobalIndex(),
      culprit);
   }

void TR_LoopVersioner::versionNaturalLoop(TR_RegionStructure *whileLoop, List<TR::Node> *nullCheckedReferences, List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *boundCheckTrees, List<TR::TreeTop> *spineCheckTrees, List<TR::TreeTop> *conditionalTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *awrtbariTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *specializedNodes, List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodesList, List<TR_Structure> *innerWhileLoops, List<TR_Structure> *clonedInnerWhileLoops, bool skipVersioningAsynchk, SharedSparseBitVector &reverseBranchInLoops)
   {
   const int loopNum = whileLoop->getNumber();
   if (!performTransformation(comp(), "%sVersioning natural loop %d\n", OPT_DETAILS_LOOP_VERSIONER, loopNum))
      return;

   TR_ScratchList<TR::Block> blocksInWhileLoop(trMemory());
   whileLoop->getBlocks(&blocksInWhileLoop);

   int32_t numNodes = comp()->getFlowGraph()->getNextNodeNumber();
   TR::Block **correspondingBlocks = (TR::Block **)trMemory()->allocateStackMemory(numNodes*sizeof(TR::Block *));
   memset(correspondingBlocks, 0, numNodes*sizeof(TR::Block *));

   TR::Block *blockAtHeadOfLoop = NULL;
   TR::TreeTop *endTree = NULL;
   //
   // Find the last treetop so that we can append blocks to the end
   //
   TR::TreeTop *treeTop;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      if (loopNum == treeTop->getNode()->getBlock()->getNumber())
         blockAtHeadOfLoop = treeTop->getNode()->getBlock();

      treeTop = treeTop->getNode()->getBlock()->getExit();
      endTree = treeTop;
      }

   VirtualGuardInfo *vgInfo = NULL;

   // Clone blocks and trees
   //
   vcount_t visitCount = comp()->incVisitCount();
   ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      TR::TreeTop *entryTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();
      TR::Node *entryNode = entryTree->getNode();

      TR::Block *nextClonedBlock = TR::Block::createEmptyBlock(entryNode, comp(), nextBlock->getFrequency(), nextBlock);
      nextClonedBlock->setIsSpecialized(nextBlock->isSpecialized());
      //
      // Set slow version flag so subsequent passes like unrolling (and even inlining)
      // to ignore 'cold' portions of code (like the slow version of
      // the loop).
      //
      if (!_neitherLoopCold ||
         nextBlock->isCold())
         {
         nextClonedBlock->setIsCold();
         if (nextBlock->isSuperCold())
            nextClonedBlock->setIsSuperCold();
         int32_t frequency = VERSIONED_COLD_BLOCK_COUNT;
         if (nextBlock->isCold())
            {
            int32_t nextBlockFreq = nextBlock->getFrequency();
            if (frequency > nextBlockFreq)
               frequency = nextBlockFreq;
            }
         nextClonedBlock->setFrequency(frequency);
         }
      else if (_neitherLoopCold &&
               shouldOnlySpecializeLoops())
         {
         //nextClonedBlock->setIsRare();
         // Set the flag on blocks in the slow version of the loop, so that in the analysis phase, we don't collect these blocks
         nextClonedBlock->setIsSpecialized();
         if (nextBlock->getFrequency() < 0)
            nextClonedBlock->setFrequency(nextBlock->getFrequency());
         else
            {
            int32_t specializedBlockFrequency = TR::Block::getScaledSpecializedFrequency(nextBlock->getFrequency());
            if (nextBlock->isCold())
               specializedBlockFrequency = nextBlock->getFrequency();
            else if (specializedBlockFrequency <= MAX_COLD_BLOCK_COUNT)
               specializedBlockFrequency = MAX_COLD_BLOCK_COUNT+1;
            nextClonedBlock->setFrequency(specializedBlockFrequency);
            }
         }
      if (nextBlock->isSynchronizedHandler())
         nextClonedBlock->setIsSynchronizedHandler();

      _cfg->addNode(nextClonedBlock);

      TR::TreeTop *newEntryTree = nextClonedBlock->getEntry();
      TR::TreeTop *newExitTree = nextClonedBlock->getExit();

      // Duplicate the trees in the original blocks.
      // Note that the parent/child links for
      // multiply referenced nodes are also the same as in the original
      // block.
      //
      TR::TreeTop *currentTree = entryTree->getNextTreeTop();
      TR::TreeTop *newPrevTree = newEntryTree;
      TR_ScratchList<TR::Node> seenNodes(trMemory()), duplicateNodes(trMemory());

      while (!(currentTree == exitTree))
         {
         TR::Node *currentNode = currentTree->getNode();
         TR::ILOpCode currentOpCode = currentNode->getOpCode();

         TR::Node *newCurrentNode = TR::Node::copy(currentTree->getNode());
         currentTree->getNode()->setVisitCount(visitCount);
         duplicateNodes.add(newCurrentNode);
         seenNodes.add(currentTree->getNode());

         for (int32_t childNum=0;childNum < currentTree->getNode()->getNumChildren(); childNum++)
            {
            TR::Node *duplicateChildNode = duplicateExact(currentTree->getNode()->getChild(childNum), &seenNodes, &duplicateNodes);
            newCurrentNode->setChild(childNum, duplicateChildNode);
            }

         if (currentNode == _conditionalTree)
            _duplicateConditionalTree = newCurrentNode;

         TR::TreeTop *newCurrentTree = TR::TreeTop::create(comp(), newCurrentNode, NULL, NULL);
         newCurrentTree->join(newExitTree);
         newPrevTree->join(newCurrentTree);

         currentTree = currentTree->getNextTreeTop();
         newPrevTree = newCurrentTree;
         }

      TR::Node *lastTree = nextBlock->getLastRealTreeTop()->getNode();
      bool disableLT = comp()->getOption(TR_DisableLoopTransfer);
      if (!disableLT
         && lastTree->getOpCode().isIf()
         && blocksInWhileLoop.find(lastTree->getBranchDestination()->getNode()->getBlock())
         && lastTree->isTheVirtualGuardForAGuardedInlinedCall()
         && isBranchSuitableToDoLoopTransfer(&blocksInWhileLoop, lastTree, comp())
         && performTransformation(
               comp(),
               "%s Adding loop transfer candidate (VirtualGuardPair) for n%un [%p]\n",
               OPT_DETAILS_LOOP_VERSIONER,
               lastTree->getGlobalIndex(),
               lastTree))
         {
         dumpOptDetails(
            comp(),
            "hotGuardBlock %d coldGuardBlock %d\n",
            nextBlock->getNumber(),
            nextClonedBlock->getNumber());

         // create the virtual guard info for this loop
         //
         if (!vgInfo)
            {
            vgInfo = new (trStackMemory()) VirtualGuardInfo(comp());
            vgInfo->_loopEntry = blockAtHeadOfLoop;
            _virtualGuardInfo.add(vgInfo);
            }

         VirtualGuardPair *virtualGuardPair = (VirtualGuardPair *) trMemory()->allocateStackMemory(sizeof(VirtualGuardPair));
         virtualGuardPair->_hotGuardBlock = nextBlock;
         virtualGuardPair->_coldGuardBlock = nextClonedBlock;
         if (trace())
            traceMsg(comp(), "virtualGuardPair at guard node %p hotGuardBlock %d coldGuardBlock %d\n", nextBlock->getLastRealTreeTop()->getNode(), nextBlock->getNumber(), nextClonedBlock->getNumber());
         virtualGuardPair->_isGuarded = false;
         // check if the virtual guard is in an inner loop
         //
         TR_RegionStructure *parent = nextBlock->getStructureOf()->getParent()->asRegion();
         if ((parent != whileLoop) &&
               (parent && parent->isNaturalLoop()))
            {
            TR::Block *entry = parent->getEntryBlock();
            virtualGuardPair->_coldGuardLoopEntryBlock = entry;
            virtualGuardPair->_isInsideInnerLoop = true;
            }
         else
            virtualGuardPair->_isInsideInnerLoop = false;

         vgInfo->_virtualGuardPairs.add(virtualGuardPair);

         // Since we've decided to do loop transfer (assuming that we don't
         // version this guard first), the taken side of the guard will
         // definitely be absent from the loop after all transformations have
         // been done, so it should be ignored when searching the loop for GC
         // points or calls.
         _curLoop->_definitelyRemovableNodes.add(lastTree);
         _curLoop->_optimisticallyRemovableNodes.add(lastTree);
         }

      correspondingBlocks[nextBlock->getNumber()] = nextClonedBlock;
      }

   // fixup the coldguard's loop entry
   //
   if (vgInfo)
      {
      ListIterator<VirtualGuardPair> vgIt(&vgInfo->_virtualGuardPairs);
      for (VirtualGuardPair *vg = vgIt.getFirst(); vg; vg = vgIt.getNext())
         {
         if (vg->_isInsideInnerLoop)
            vg->_coldGuardLoopEntryBlock = correspondingBlocks[vg->_coldGuardLoopEntryBlock->getNumber()];
         }
      }

   // Append the new cloned blocks (non versioned loop)
   // to the end of the method in the same tree order as the
   // corresponding blocks in the original loop. This
   // should preserve fall throughs where possible.
   //
   TR::TreeTop *stopTree = endTree;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Block *block = treeTop->getNode()->getBlock();
      TR::Block *clonedBlock = correspondingBlocks[block->getNumber()];
      if (clonedBlock)
         {
         TR::TreeTop *newEntryTree = clonedBlock->getEntry();
         TR::TreeTop *newExitTree = clonedBlock->getExit();
         endTree->join(newEntryTree);
         newExitTree->setNextTreeTop(NULL);
         endTree = newExitTree;
         }

      treeTop = treeTop->getNode()->getBlock()->getExit();
      if (treeTop == stopTree)
         break;
      }


   // Start adjusting the CFG now; set structure to NULL
   // before doing this to avoid generalized structure
   // repair. We will repair structure ourselves later on.
   //
   _cfg->setStructure(NULL);

   TR_ScratchList<TR_BlockStructure> newGotoBlockStructures(trMemory());
   blocksIt.reset();
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      // Iterate over the original blocks being cloned and examine
      // successors for each blocks trying to then fix up the CFG
      // edges for correspoding blocks
      //
      for (auto edge = nextBlock->getSuccessors().begin(); edge != nextBlock->getSuccessors().end(); ++edge)
         {
         TR::Block *succ = toBlock((*edge)->getTo());
         TR::Block *nextClonedBlock = correspondingBlocks[nextBlock->getNumber()];
         TR::Block *clonedSucc = correspondingBlocks[succ->getNumber()];

         if (clonedSucc)
            {
            // If this successor also has a cloned counterpart,
            // i.e. it belongs to the loop being cloned.
            //
            TR::CFGEdge *e = TR::CFGEdge::createEdge(nextClonedBlock,  clonedSucc, trMemory());
            _cfg->addEdge(e);

            if (_neitherLoopCold)
               {
               if (!shouldOnlySpecializeLoops())
                  e->setFrequency((*edge)->getFrequency());
               else
                  {
                  int32_t specializedBlockFrequency = TR::Block::getScaledSpecializedFrequency((*edge)->getFrequency());

                  e->setFrequency(specializedBlockFrequency);
                  }
               }

            nextClonedBlock->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp(), succ->getEntry(), clonedSucc->getEntry());
            }
         else
            {
            // This successor does not belong to the loop being cloned;
            // so there is no cloned counterpart for it. If the block
            // branches explicitly to its successor, then we can simply
            // add an edge from the cloned block to the successor; else
            // if it was fall through then we have to add a new goto block
            // which would branch explicitly to the successor.
            //
            TR::Node *lastNode = nextClonedBlock->getLastRealTreeTop()->getNode();
            bool callWithException = false;
            if (lastNode->getNumChildren() > 0)
               {
               if (lastNode->getFirstChild()->getOpCodeValue() == TR::athrow)
                  lastNode = lastNode->getFirstChild();
               TR::ILOpCode& childOpCode = lastNode->getFirstChild()->getOpCode();
               if (childOpCode.isCall() && childOpCode.isJumpWithMultipleTargets())
                  {
                  callWithException = true;
                  }
               }
            TR::ILOpCode &lastOpCode = lastNode->getOpCode();

            bool fallsThrough = false;
            if (!(lastOpCode.isBranch() || (lastOpCode.isJumpWithMultipleTargets() && lastOpCode.hasBranchChildren()) || lastOpCode.isReturn() || (lastOpCode.getOpCodeValue() == TR::athrow) || callWithException))
               fallsThrough = true;
            else if (lastOpCode.isBranch())
               {
               if (lastNode->getBranchDestination() != succ->getEntry())
                  fallsThrough = true;
               }

            if (!fallsThrough)
               _cfg->addEdge(TR::CFGEdge::createEdge(nextClonedBlock,  succ, trMemory()));
            else
               {
               TR::Block *newGotoBlock = TR::Block::createEmptyBlock(lastNode, comp(), (*edge)->getFrequency(), nextClonedBlock);
               newGotoBlock->setIsSpecialized(nextClonedBlock->isSpecialized());
               _cfg->addNode(newGotoBlock);

               if (trace())
                  traceMsg(comp(), "Creating new goto block : %d for node %p\n", newGotoBlock->getNumber(), lastNode);

               TR::TreeTop *gotoBlockEntryTree = newGotoBlock->getEntry();
               TR::TreeTop *gotoBlockExitTree = newGotoBlock->getExit();
               TR::Node *gotoNode =  TR::Node::create(lastNode, TR::Goto);
               TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
               TR_ASSERT(succ->getEntry(), "Entry tree of succ is NULL\n");
               gotoNode->setBranchDestination(succ->getEntry());
               gotoBlockEntryTree->join(gotoTree);
               gotoTree->join(gotoBlockExitTree);
               TR::TreeTop *clonedExit = nextClonedBlock->getExit();
               TR::TreeTop *treeAfterClonedExit = clonedExit->getNextTreeTop();
               clonedExit->join(gotoBlockEntryTree);
               gotoBlockExitTree->join(treeAfterClonedExit);
               if (endTree == clonedExit)
                  endTree = gotoBlockExitTree;

               TR::CFGEdge *e1 = TR::CFGEdge::createEdge(nextClonedBlock,  newGotoBlock, trMemory());
               _cfg->addEdge(e1);
               TR::CFGEdge *e2 = TR::CFGEdge::createEdge(newGotoBlock,  succ, trMemory());
               _cfg->addEdge(e2);
               if (_neitherLoopCold)
                  {
                  if (!shouldOnlySpecializeLoops())
                     {
                     e1->setFrequency((*edge)->getFrequency());
                     e2->setFrequency((*edge)->getFrequency());
                     }
                  else
                     {
                     int32_t specializedBlockFrequency = TR::Block::getScaledSpecializedFrequency((*edge)->getFrequency());

                     e1->setFrequency(specializedBlockFrequency);
                     e2->setFrequency(specializedBlockFrequency);
                     }
                  }

               TR_BlockStructure *newGotoBlockStructure = new (_cfg->structureMemoryRegion()) TR_BlockStructure(comp(), newGotoBlock->getNumber(), newGotoBlock);
               newGotoBlockStructure->setCreatedByVersioning(true);
               if (!_neitherLoopCold)
                  {
                  newGotoBlock->setIsCold();
                  newGotoBlock->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
                  }
               else if (_neitherLoopCold &&
                        shouldOnlySpecializeLoops())
                  {
                  //newGotoBlock->setIsRare();
                  if ((*edge)->getFrequency() < 0)
                     newGotoBlock->setFrequency((*edge)->getFrequency());
                  else
                     {
                     int32_t specializedBlockFrequency = TR::Block::getScaledSpecializedFrequency((*edge)->getFrequency());
                     if ((*edge)->getFrequency() <= MAX_COLD_BLOCK_COUNT)
                        specializedBlockFrequency = (*edge)->getFrequency();
                     else if (specializedBlockFrequency <= MAX_COLD_BLOCK_COUNT)
                        specializedBlockFrequency = MAX_COLD_BLOCK_COUNT+1;
                     newGotoBlock->setFrequency(specializedBlockFrequency);
                     }
                  }
               newGotoBlockStructures.add(newGotoBlockStructure);
               }
            }
         }


      // Adjust exception edges for the new cloned blocks
      //
      for (auto edge = nextBlock->getExceptionSuccessors().begin(); edge != nextBlock->getExceptionSuccessors().end(); ++edge)
         {
         TR::Block *succ = toBlock((*edge)->getTo());
         TR::Block *nextClonedBlock = correspondingBlocks[nextBlock->getNumber()];
         TR::Block *clonedSucc = correspondingBlocks[succ->getNumber()];

         if (clonedSucc)
            {
            _cfg->addEdge(TR::CFGEdge::createExceptionEdge(nextClonedBlock, clonedSucc,trMemory()));
            }
         else
            {
            _cfg->addEdge(TR::CFGEdge::createExceptionEdge(nextClonedBlock, succ,trMemory()));
            }
         }
      }


   // Locate the loop invariant block for the original loop
   //
   TR::Block *invariantBlock = NULL;
   for (auto nextPred = blockAtHeadOfLoop->getPredecessors().begin(); nextPred != blockAtHeadOfLoop->getPredecessors().end(); ++nextPred)
      {
      TR::CFGNode *nextNode = (*nextPred)->getFrom();
      if (!correspondingBlocks[nextNode->getNumber()])
         {
         invariantBlock = toBlock(nextNode);
         break;
         }
      }


   // Create a new loop invariant block for the cloned loop
   //
   TR::Block *blockAtHeadOfClonedLoop = correspondingBlocks[blockAtHeadOfLoop->getNumber()];
   TR::TreeTop * blockHeadTreeTop = blockAtHeadOfClonedLoop->getEntry();
   TR::Node * blockHeadNode = blockHeadTreeTop->getNode();
   TR::Block *clonedLoopInvariantBlock = TR::Block::createEmptyBlock(invariantBlock->getEntry()->getNode(), comp(), invariantBlock->getFrequency(), invariantBlock);
   clonedLoopInvariantBlock->setIsSpecialized(invariantBlock->isSpecialized());
   _cfg->addNode(clonedLoopInvariantBlock);
   TR::TreeTop *clonedInvariantEntryTree = clonedLoopInvariantBlock->getEntry();
   TR::TreeTop *clonedInvariantExitTree = clonedLoopInvariantBlock->getExit();
   TR::Node *gotoNode =  TR::Node::create(blockHeadNode, TR::Goto, 0, blockHeadTreeTop);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
   clonedInvariantEntryTree->join(gotoTree);
   gotoTree->join(clonedInvariantExitTree);

   _exitGotoTarget = clonedLoopInvariantBlock->getEntry();

   if (vgInfo)
      {
      vgInfo->_coldLoopEntryBlock = blockAtHeadOfClonedLoop;
      vgInfo->_coldLoopInvariantBlock = clonedLoopInvariantBlock;
      }

   // --------------------------------------------------------------------------------
   //
   // Now construct each comparison test that needs to be
   // done as part of this versioning.
   //
   // The first one is to check if the loop driving induction
   // variable is incremented not a by a constt but by a loop invariant
   // local, then is the local greater than zero. This is reqd to
   // guarantee the direction in which the value of the induction variable
   // is moving in.
   //
   // --------------------------------------------------------------------------------

   TR_ScratchList<TR::Node> comparisonTrees(trMemory());

   if (_requiresAdditionalCheckForIncrement  && performTransformation(comp(), "%s Creating test outside loop for checking if loop driving induction variable is incremented by a loop invariant that is greater than 0\n", OPT_DETAILS_LOOP_VERSIONER))
      {
      if (_constNode->getType().isInt32())
         {
         TR::Node *nextComparisonNode = TR::Node::createif(TR::ificmple, _constNode, TR::Node::create(blockHeadNode, TR::iconst, 0, 0), clonedLoopInvariantBlock->getEntry());
         dumpOptDetails(comp(), "Node %p has been created testing if increment check is required\n", nextComparisonNode);
         comparisonTrees.add(nextComparisonNode);
         }
      else if (_constNode->getType().isInt64())
         {
         TR::Node *nextComparisonNode = TR::Node::createif(TR::iflcmple, _constNode, TR::Node::create(blockHeadNode, TR::lconst, 0, 0), clonedLoopInvariantBlock->getEntry());
         dumpOptDetails(comp(), "Node %p has been created testing if increment check is required\n", nextComparisonNode);
         comparisonTrees.add(nextComparisonNode);
         }
      else
         {
         TR_ASSERT(0, "Unable to create test outside of loop for checking if iv update increment is greater than 0 due to unknown type\n");
         }
      }

   if (_loopConditionInvariant && _asyncCheckTree)
      {
      bool isIncreasing;
      TR::SymbolReference* firstChildSymRef;
      bool _canPredictIters = canPredictIters(whileLoop, blocksInWhileLoop, isIncreasing, firstChildSymRef);

      if (_numberOfTreesInLoop == 0)
         {
         TR_ASSERT(0, "Loop must have at least one tree in it\n");
         _numberOfTreesInLoop = 1;
         }

      TR::Node *loopLimit = _loopTestTree->getNode()->getSecondChild();
      int32_t profiledLoopLimit = DEFAULT_LOOP_LIMIT/_numberOfTreesInLoop;
      TR::ILOpCodes comparisonOpCode = TR::ificmpgt;

      if (loopLimit->getType().isInt32() &&
         !loopLimit->getByteCodeInfo().doNotProfile())
         {
#ifdef J9_PROJECT_SPECIFIC
         TR_ValueInfo *valueInfo = static_cast<TR_ValueInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(loopLimit, comp(), ValueInfo));
         if (valueInfo)
            {
            if ( valueInfo->getTotalFrequency() )
               {
               dumpOptDetails(comp(), "From value profiling, loop test node %p has value %d freq %d total freq %d, and max value %d\n", loopLimit, valueInfo->getTopValue(), (int32_t) (valueInfo->getTopProbability() * valueInfo->getTotalFrequency()), valueInfo->getTotalFrequency(), valueInfo->getMaxValue());

               //if (shouldOnlySpecializeLoops())
                  {

                  // if we estimate loop count to mostly be over
                  // the static limit, do not version based on async
                  // checks--leave them in, so we can still run the
                  // version without bounds checks
                  TR_ScratchList<TR_ExtraValueInfo> valuesSortedByFrequency(trMemory());
                  valueInfo->getSortedList(comp(), &valuesSortedByFrequency);
                  ListIterator<TR_ExtraValueInfo> sortedValuesIt(&valuesSortedByFrequency);

                  float totalFreq = (float) valueInfo->getTotalFrequency();
                  float valuedFreq = 0;

                  for (TR_ExtraValueInfo *profiledInfo = sortedValuesIt.getFirst(); profiledInfo != NULL; profiledInfo = sortedValuesIt.getNext())
                     {

                     if ( profiledInfo->_value < profiledLoopLimit )
                        valuedFreq += (float) profiledInfo->_value;

                     }

                  if ( valueInfo->getMaxValue() > profiledLoopLimit && valuedFreq / totalFreq < 0.95f )
                     {
                     dumpOptDetails(comp(), "Maximum profiled value of %d exceeds profiled loop limit of %d, and value profiled iterations below limit account for only %5.3f of iterations.  Delaying asyncCheck removal for node %p\n", valueInfo->getMaxValue(), profiledLoopLimit, valuedFreq / totalFreq, loopLimit);
                     _canPredictIters = false;
                     }
                  }
               }
            else
               {
               //NO Value Profiling Info
               int32_t numIters = whileLoop->getEntryBlock()->getFrequency();
               //if (numIters > 0.90*comp()->getRecompilationInfo()->getMaxBlockCount())
               if (numIters > 0.90*(MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT))
                  _canPredictIters = false;
               }
            }
         //compjazz 108308 disallow versioning of async checks for long running GPU Loops when forced to compile at scorching without jit value info.
         else if(comp()->getOptions()->getEnableGPU(TR_EnableGPU) && comp()->hasIntStreamForEach() && comp()->getMethodHotness() == scorching)
            {
            _canPredictIters = false;
            }
#endif
         }

      if (_canPredictIters && firstChildSymRef)
         {
         TR_InductionVariable *v = whileLoop->findMatchingIV(firstChildSymRef);
         if (v && v->getEntry() && v->getExit())
            {
            TR::VPConstraint *entryVal = v->getEntry();
            TR::VPConstraint *exitVal = v->getExit();
            TR::VPConstraint *incrVal = v->getIncr();
            if (entryVal->asIntConstraint() &&
               exitVal->asIntConstraint() &&
               incrVal->asIntConstraint())
               {
               int64_t minDelta;
               if (incrVal->getLowInt() > 0)
                  minDelta = (int64_t)exitVal->getLowInt() - entryVal->getHighInt();
               else if (incrVal->getHighInt() < 0)
                  minDelta = (int64_t)entryVal->getLowInt() - exitVal->getHighInt();
               else
                  minDelta = TR::getMinSigned<TR::Int64>();
               if (minDelta > profiledLoopLimit)
                  _canPredictIters = false;
               }
            }
         }

      if (shouldOnlySpecializeLoops())
         {
         int32_t numIters = whileLoop->getEntryBlock()->getFrequency();
         if (numIters < 0.90*(MAX_BLOCK_COUNT+MAX_COLD_BLOCK_COUNT))
            _canPredictIters = false;
         }

      // `_duplicateConditionalTree` is set only when `_conditionalTree` is set. `_conditionalTree`
      // is set only when`_neitherLoopCold` is true. Therefore, if `_duplicateConditionalTree` exists,
      // the loops are unbiased. When the loops are unbiased, do not version asynch check
      // so that the conditional in both loops can be folded away.
      bool asyncCheckVersioningOK = _duplicateConditionalTree ? false : true;

      if (asyncCheckVersioningOK &&
          !comp()->getOption(TR_DisableAsyncCheckVersioning) &&
          !refineAliases() && _canPredictIters && comp()->getProfilingMode() != JitProfiling &&
          performTransformation(comp(), "%s Creating test outside loop for deciding if async check is required\n", OPT_DETAILS_LOOP_VERSIONER))
         {
         TR::Node *lowerBound = _loopTestTree->getNode()->getFirstChild()->duplicateTreeForCodeMotion();
         TR::Node *upperBound = _loopTestTree->getNode()->getSecondChild()->duplicateTreeForCodeMotion();
         TR::Node *loopLimit = isIncreasing ?
            TR::Node::create(TR::isub, 2, upperBound, lowerBound) :
            TR::Node::create(TR::isub, 2, lowerBound, upperBound);

         TR::Node *nextComparisonNode = TR::Node::createif(comparisonOpCode, loopLimit, TR::Node::create(loopLimit, TR::iconst, 0, profiledLoopLimit), clonedLoopInvariantBlock->getEntry());
         nextComparisonNode->setIsMaxLoopIterationGuard(true);

         LoopEntryPrep *prep =
            createLoopEntryPrep(LoopEntryPrep::TEST, nextComparisonNode);

         if (prep != NULL)
            {
            nodeWillBeRemovedIfPossible(_asyncCheckTree->getNode(), prep);
            _curLoop->_loopImprovements.push_back(
               new (_curLoop->_memRegion) RemoveAsyncCheck(
                  this,
                  prep,
                  _asyncCheckTree));
            }
         }
      }

   // Construct the tests for invariant or induction var expressions that need
   // to be bounds checked.
   //
   if (!boundCheckTrees->isEmpty())
      {
      bool reverseBranch = false;
      if (_loopConditionInvariant &&
         !blocksInWhileLoop.find(_loopTestTree->getNode()->getBranchDestination()->getNode()->getBlock()))
         reverseBranch = true;
      buildBoundCheckComparisonsTree(boundCheckTrees, spineCheckTrees, reverseBranch);
      }

   // Construct the tests for invariant or induction var expressions that need
   // to be bounds checked.
   //
   if (shouldOnlySpecializeLoops() && !spineCheckTrees->isEmpty())
      {
      buildSpineCheckComparisonsTree(spineCheckTrees);
      }


   // Construct the tests for invariant expressions that need
   // to be div checked.
   //
   if (!divCheckTrees->isEmpty() &&
      !shouldOnlySpecializeLoops())
      {
      buildDivCheckComparisonsTree(divCheckTrees);
      }

   // Construct specialization tests
   //
   if (!specializedNodes->isEmpty())
      {
      TR::SymbolReference **symRefs = (TR::SymbolReference **)trMemory()->allocateStackMemory(comp()->getSymRefCount()*sizeof(TR::SymbolReference *));
      memset(symRefs, 0, comp()->getSymRefCount()*sizeof(TR::SymbolReference *));

      //printf("Reached here in %s\n", comp()->signature());
      bool specializedLongs = buildSpecializationTree(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, specializedNodes, invariantBlock, symRefs);

      if (specializedLongs)
         {
         ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
         TR::Block *nextBlock;
         for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
            {
            vcount_t visitCount = comp()->incVisitCount();
            TR::TreeTop *cursor = nextBlock->getEntry();
            while (cursor != nextBlock->getExit())
               {
               convertSpecializedLongsToInts(cursor->getNode(), visitCount, symRefs);
               cursor = cursor->getNextTreeTop();
               }
            }
         }

      // Ensure that substitution for specialization leaves behind no malformed
      // NULLCHK or DIVCHK nodes. These checks used to be eagerly (and
      // generally incorrectly) removed by collectAllExpressionsToBeChecked().
      //
      // TODO: Specialization should be changed to defer its transformations,
      // using the privatization analysis to determine whether it's okay to
      // stop re-reading certain data each iteration. Substitution of constant
      // values should probably be integrated with substitution of loads of
      // privatization temps.
      //
      ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
      for (TR::Block *block = blocksIt.getCurrent(); block != NULL; block = blocksIt.getNext())
         {
         TR::TreeTop *entry = block->getEntry();
         TR::TreeTop *exit = block->getExit();
         for (TR::TreeTopIterator it(entry, comp()); !it.isAt(exit); it.stepForward())
            {
            TR::TreeTop *tt = it.currentTree();
            TR::Node *node = tt->getNode();
            TR::ILOpCode op = node->getOpCode();
            if (op.isNullCheck() || op.getOpCodeValue() == TR::DIVCHK)
               {
               TR::ILOpCodes childOp = node->getChild(0)->getOpCodeValue();
               if (childOp == TR::iconst || childOp == TR::iu2l)
                  {
                  dumpOptDetails(
                     comp(),
                     "Removing check n%un [%p] because child has been specialized\n",
                     node->getGlobalIndex(),
                     node);

                  TR::Node::recreate(node, TR::treetop);
                  }
               }
            }
         }
      }


   // Construct trees for refined alias version
   if(refineAliases())
      buildAliasRefinementComparisonTrees(nullCheckTrees,divCheckTrees, checkCastTrees, arrayStoreCheckTrees,&comparisonTrees,clonedLoopInvariantBlock);

   // Construct the tests for invariant conditionals.
   //
   if (!conditionalTrees->isEmpty())
      buildConditionalTree(conditionalTrees, reverseBranchInLoops);

   // Construct the tests for invariant expressions that need
   // to be cast.
   //
   if (!checkCastTrees->isEmpty())
      buildCheckCastComparisonsTree(checkCastTrees);

   if (!arrayStoreCheckTrees->isEmpty())
      buildArrayStoreCheckComparisonsTree(arrayStoreCheckTrees);

   // Construct tests for invariant expressions
   //
   if (invariantNodes && !invariantNodes->isEmpty())
      {
      //printf("Reached here in %s\n", comp()->signature());
      buildLoopInvariantTree(invariantNodes);
      invariantNodes->deleteAll();
      }

   // Construct the tests for invariant expressions that need
   // to be null checked.

   // default hotness threshold
   TR_Hotness hotnessThreshold = hot;

   // If aggressive loop versioning is requested, don't call buildNullCheckComparisonsTree based on hotness
   if (comp()->getOption(TR_EnableAggressiveLoopVersioning))
      {
      if (trace()) traceMsg(comp(), "aggressiveLoopVersioning: raising hotnessThreshold for buildNullCheckComparisonsTree\n");
      hotnessThreshold = maxHotness; // threshold which can't be matched by the > operator
      }

   if (comp()->cg()->performsChecksExplicitly() ||
       (comp()->getMethodHotness() > hotnessThreshold))
      {
      if (!nullCheckTrees->isEmpty() &&
          !shouldOnlySpecializeLoops())
         {
         buildNullCheckComparisonsTree(nullCheckedReferences, nullCheckTrees);
         }
      }
   else
      {
      // Find null checks for which some other prep has already created a
      // versioning test as a dependency. If the versioning test can be
      // emitted, we might as well remove the null check.
      ListElement<TR::TreeTop> *head = nullCheckTrees->getListHead();
      for (ListElement<TR::TreeTop> *elt = head; elt != NULL; elt = elt->getNextElement())
         {
         TR::Node *check = elt->getData()->getNode();

         // A NULLCHK might have been specialized away, but will still appear in nullCheckTrees
         // Skip such trees
         if (!check->getOpCode().isNullCheck())
            {
            TR_ASSERT_FATAL(check->getOpCodeValue() == TR::treetop, "Unexpected opcode for n%dn [%p]\n", check->getGlobalIndex(), check);
            continue;
            }

         TR::Node *refNode = check->getNullCheckReference();
         const Expr *refExpr = findCanonicalExpr(refNode);
         if (refExpr == NULL)
            continue;

         auto nullTestEntry = _curLoop->_nullTestPreps.find(refExpr);
         if (nullTestEntry == _curLoop->_nullTestPreps.end())
            continue;

         LoopEntryPrep *prep = nullTestEntry->second;
         if (!performTransformation(
               comp(),
               "%sOpportunistically attempting to eliminate null check n%un [%p] using existing prep %p\n",
               optDetailString(),
               check->getGlobalIndex(),
               check,
               prep))
            {
            continue;
            }

         nodeWillBeRemovedIfPossible(check, prep);
         _curLoop->_loopImprovements.push_back(
            new (_curLoop->_memRegion) RemoveNullCheck(this, prep, check));
         }
      }

   TR_ASSERT_FATAL(
       _curLoop->_optimisticallyRemovableNodes.contains(_curLoop->_definitelyRemovableNodes),
       "loop %d: privatization should only allow more versioning",
       loopNum);

   TR_ASSERT_FATAL(
       _curLoop->_guardsRemovableWithPrivAndHCR.contains(_curLoop->_guardsRemovableWithHCR),
       "loop %d: privatization should only allow more merged HCR guard versioning",
       loopNum);

   TR_ASSERT_FATAL(
       _curLoop->_guardsRemovableWithPrivAndOSR.contains(_curLoop->_guardsRemovableWithOSR),
       "loop %d: privatization should only allow more merged OSR guard versioning",
       loopNum);

   // Detect the presence of HCR and/or OSR guards in the loop.
   TR_ScratchList<TR::TreeTop> hcrGuards(trMemory());
   TR_ScratchList<TR::TreeTop> osrGuards(trMemory());
   TR::NodeChecklist hcrGuardsSet(comp());
   TR::NodeChecklist osrGuardsSet(comp());
   // Search scope
      {
      TR::NodeChecklist noNodesRemoved(comp()); // search the loop as-is
      LoopBodySearch search(
         comp(),
         _curLoop->_memRegion,
         whileLoop,
         &noNodesRemoved,
         &_curLoop->_takenBranches);

      for (; search.hasTreeTop(); search.advance())
         {
         TR::TreeTop *tt = search.currentTreeTop();
         TR::Node *ttNode = tt->getNode();
         if (ttNode->isHCRGuard())
            {
            hcrGuards.add(tt);
            hcrGuardsSet.add(ttNode);
            }
         else if (ttNode->isOSRGuard())
            {
            osrGuards.add(tt);
            osrGuardsSet.add(ttNode);
            }
         }
      }

   // Do a fixed-point computation to find the maximal set of versioning
   // operations that can be safely performed. Initially, we'll consider
   // allowing all versioning operations. Then until the analysis converges, we
   // will tentatively assume that the types of versioning currently under
   // consideration are safe and look at the operations that would still remain
   // in the hot loop, some of which may prevent some versioning operations
   // that we hoped would be possible.

   // TODO: Handle breakpoint and method enter/exit hook guards?

   static const bool disableLoopHCR =
      feGetEnv("TR_DisableHCRGuardLoopVersioner") != NULL;
   static const bool disableLoopOSR =
      feGetEnv("TR_DisableOSRGuardLoopVersioner") != NULL;
   static const bool disableWrtbarVersion =
      feGetEnv("TR_disableWrtbarVersion") != NULL;

   _curLoop->_privatizationOK = _curLoop->_privatizationsRequested;

   _curLoop->_hcrGuardVersioningOK =
      (!hcrGuards.isEmpty() || !_curLoop->_guardsRemovableWithPrivAndHCR.isEmpty())
      && !disableLoopHCR;

   _curLoop->_osrGuardVersioningOK =
      (!osrGuards.isEmpty() || !_curLoop->_guardsRemovableWithPrivAndOSR.isEmpty())
      && !disableLoopOSR;

   bool awrtbariVersioningOK =
      !awrtbariTrees->isEmpty()
      && !disableWrtbarVersion
      && !shouldOnlySpecializeLoops()
      && !refineAliases()
      && !_curLoop->_foldConditionalInDuplicatedLoop;

   bool alreadyAskedPermission = false; // permission to transform

   while (_curLoop->_privatizationOK
          || _curLoop->_hcrGuardVersioningOK
          || _curLoop->_osrGuardVersioningOK
          || awrtbariVersioningOK)
      {
      // Search the part of the loop body that would remain *after removing
      // everything allowed by the current tentative assumptions*.
      TR::NodeChecklist removedNodes(comp());

      if (_curLoop->_privatizationOK)
         removedNodes.add(_curLoop->_optimisticallyRemovableNodes);
      else
         removedNodes.add(_curLoop->_definitelyRemovableNodes);

      if (_curLoop->_hcrGuardVersioningOK)
         {
         removedNodes.add(hcrGuardsSet);
         if (_curLoop->_privatizationOK)
            removedNodes.add(_curLoop->_guardsRemovableWithPrivAndHCR);
         else
            removedNodes.add(_curLoop->_guardsRemovableWithHCR);
         }

      if (_curLoop->_osrGuardVersioningOK)
         {
         removedNodes.add(osrGuardsSet);
         if (_curLoop->_privatizationOK)
            removedNodes.add(_curLoop->_guardsRemovableWithPrivAndOSR);
         else
            removedNodes.add(_curLoop->_guardsRemovableWithOSR);
         }

      dumpOptDetails(
         comp(),
         "Trial versioning with:%s%s%s%s\n",
         _curLoop->_privatizationOK ? " privatization" : "",
         _curLoop->_hcrGuardVersioningOK ? " hcrGuards" : "",
         _curLoop->_osrGuardVersioningOK ? " osrGuards" : "",
         awrtbariVersioningOK ? " (awrtbari)" : "");

      bool newPrivatizationOK = _curLoop->_privatizationOK;
      bool newHCRGuardVersioningOK = _curLoop->_hcrGuardVersioningOK;
      bool newOSRGuardVersioningOK = _curLoop->_osrGuardVersioningOK;
      bool newAwrtbariVersioningOK = awrtbariVersioningOK;

      LoopBodySearch search(
         comp(),
         _curLoop->_memRegion,
         whileLoop,
         &removedNodes,
         &_curLoop->_takenBranches);

      for (; search.hasTreeTop(); search.advance())
         {
         TR::TreeTop *tt = search.currentTreeTop();
         TR::Node *ttNode = tt->getNode();
         if (removedNodes.contains(ttNode))
            continue;

         if (ttNode->getOpCodeValue() == TR::treetop
             || ttNode->getOpCode().isNullCheck()
             || ttNode->getOpCode().isResolveCheck())
            {
            TR::Node *child = ttNode->getChild(0);
            if (child->getOpCode().isFunctionCall())
               {
               newPrivatizationOK = false;
               if (_curLoop->_privatizationOK)
                  traceCannot("privatize", child, comp());
               }
            }

         // If the VM is configured to allow for OSR-HCR, then HCR invalidation
         // can only happen at a subset of GC points even in compilations using
         // traditional HCR. We could try to take advantage of that here, but
         // so far no attempt is made to do so.

         if (comp()->getHCRMode() == TR::osr
             && comp()->isPotentialOSRPoint(ttNode, NULL, true))
            {
            newHCRGuardVersioningOK = false;
            newOSRGuardVersioningOK = false;
            if (_curLoop->_hcrGuardVersioningOK)
               traceCannot("version HCR guards", ttNode, comp());
            if (_curLoop->_osrGuardVersioningOK)
               traceCannot("version OSR guards", ttNode, comp());
            }

         bool canGcAndStayInLoop = ttNode->canGCandReturn();
         if (!canGcAndStayInLoop && ttNode->canGCandExcept())
            {
            TR::Block *block = search.currentBlock();
            TR::CFGEdgeList &excSuccs = block->getExceptionSuccessors();
            for (auto it = excSuccs.begin(); it != excSuccs.end(); ++it)
               {
               TR::Block *handler = (*it)->getTo()->asBlock();
               if (whileLoop->contains(handler->getStructureOf()))
                  {
                  canGcAndStayInLoop = true;
                  break;
                  }
               }
            }

         if (canGcAndStayInLoop)
            {
            if (comp()->getHCRMode() == TR::traditional)
               {
               newHCRGuardVersioningOK = false;
               if (_curLoop->_hcrGuardVersioningOK)
                  traceCannot("version HCR guards", ttNode, comp());
               }

            newAwrtbariVersioningOK = false;
            if (awrtbariVersioningOK)
               traceCannot("version awrtbari", ttNode, comp());
            }
         }

      // Update awrtbariVersioningOK unconditionally here, since it is purely
      // an output, not an input, of the analysis, so changes to it are not
      // relevant to convergence.
      if (awrtbariVersioningOK && !newAwrtbariVersioningOK)
         dumpOptDetails(comp(), "No awrtbari versioning in loop %d\n", loopNum);

      awrtbariVersioningOK = newAwrtbariVersioningOK;

      // If nothing has changed, then the analysis has converged! It's possible
      // to version with the current settings, and nothing remaining in the hot
      // loop will interfere with that versioning.
      if (newPrivatizationOK == _curLoop->_privatizationOK
          && newHCRGuardVersioningOK == _curLoop->_hcrGuardVersioningOK
          && newOSRGuardVersioningOK == _curLoop->_osrGuardVersioningOK)
         {
         // By checking performTransformation() here, we avoid asking about
         // transformations that wouldn't have been possible anyway.
         //
         // Don't performTransformation() for privatization. Privatizations are
         // part of transformations that were already optional earlier on.
         //
         // Don't performTransformation() for write barrier versioning here.
         // That can be done separately for each barrier.
         //
         if (alreadyAskedPermission)
            break;

         // Permission to version HCR/OSR guards applies to all HCR/OSR guards
         // (resp.), since whenever this analysis determines that they are safe
         // to version, it does so under the assumption that all of them will
         // be versioned.
         alreadyAskedPermission = true;

         if (newHCRGuardVersioningOK)
            {
            newHCRGuardVersioningOK = performTransformation(
               comp(), "%sVersioning HCR guards\n", OPT_DETAILS_LOOP_VERSIONER);
            }

         if (newOSRGuardVersioningOK)
            {
            newOSRGuardVersioningOK = performTransformation(
               comp(), "%sVersioning OSR guards\n", OPT_DETAILS_LOOP_VERSIONER);
            }

         if (newHCRGuardVersioningOK == _curLoop->_hcrGuardVersioningOK
             && newOSRGuardVersioningOK == _curLoop->_osrGuardVersioningOK)
            break;
         }

      // Discovered that we cannot do at least one of privatization, HCR guard
      // versioning, and OSR guard versioning. Relax the tentative assumptions
      // and re-analyze.
      if (_curLoop->_privatizationOK && !newPrivatizationOK)
         dumpOptDetails(comp(), "No privatization in loop %d\n", loopNum);
      if (_curLoop->_hcrGuardVersioningOK && !newHCRGuardVersioningOK)
         dumpOptDetails(comp(), "No HCR guard versioning in loop %d\n", loopNum);
      if (_curLoop->_osrGuardVersioningOK && !newOSRGuardVersioningOK)
         dumpOptDetails(comp(), "No OSR guard versioning in loop %d\n", loopNum);

      _curLoop->_privatizationOK = newPrivatizationOK;
      _curLoop->_hcrGuardVersioningOK = newHCRGuardVersioningOK;
      _curLoop->_osrGuardVersioningOK = newOSRGuardVersioningOK;
      }

   // At this point, all of the transformations allowed by _privatizationOK,
   // etc. are MANDATORY, since they have been determined to be safe under the
   // assumption that all of them will be done.
   //
   // However, write barrier versioning is (exceptionally) still optional
   // because no other transformations depend on it.

   if (_curLoop->_hcrGuardVersioningOK)
      {
      ListIterator<TR::TreeTop> guardIt(&hcrGuards);
      for (TR::TreeTop *tt = guardIt.getCurrent(); tt; tt = guardIt.getNext())
         {
         dumpOptDetails(
            comp(),
            "Creating versioned HCRGuard for guard n%dn\n",
            tt->getNode()->getGlobalIndex());

         TR::Node *guard = tt->getNode()->duplicateTree();
         guard->setBranchDestination(clonedLoopInvariantBlock->getEntry());
         comparisonTrees.add(guard);

         bool reverseBranch = false, origLoop = true;
         FoldConditional fold(this, NULL, tt->getNode(), reverseBranch, origLoop);
         fold.improveLoop();
         }
      }

   if (_curLoop->_osrGuardVersioningOK)
      {
      // All OSR guards will be patched by the same runtime assumptions, so only
      // one OSR guard is added to branch to the slow loop. This is only needed
      // if there are no virtual guards with merged OSR guards to be versioned,
      // but it's more straightforward (and harmless) to generate it whenever
      // there was originally a standalone OSR guard in the loop.
      if (!osrGuards.isEmpty())
         {
         TR::Node *osrGuard = osrGuards.getListHead()->getData()->getNode();
         TR::Node *guard = osrGuard->duplicateTree();
         if (trace())
            traceMsg(comp(), "OSRGuard n%dn has been created to guard against method invalidation\n", guard->getGlobalIndex());

         guard->setBranchDestination(clonedLoopInvariantBlock->getEntry());
         comparisonTrees.add(guard);
         }

      ListIterator<TR::TreeTop> guardIt(&osrGuards);
      for (TR::TreeTop *tt = guardIt.getCurrent(); tt; tt = guardIt.getNext())
         {
         bool reverseBranch = false, origLoop = true;
         FoldConditional fold(this, NULL, tt->getNode(), reverseBranch, origLoop);
         fold.improveLoop();
         }
      }

   if (awrtbariVersioningOK)
      buildAwrtbariComparisonsTree(awrtbariTrees);

   // For each loop improvement that is still possible, emit its loop entry
   // prep and transform the loop. NB. These improvements are mandatory now.
   auto improvementsBegin = _curLoop->_loopImprovements.begin();
   auto improvementsEnd = _curLoop->_loopImprovements.end();
   for (auto it = improvementsBegin; it != improvementsEnd; ++it)
      {
      LoopImprovement *improvement = *it;
      LoopEntryPrep *prep = improvement->_prep;
      if ((!prep->_requiresPrivatization || _curLoop->_privatizationOK)
          && (!prep->_expr->mergedWithHCRGuard() || _curLoop->_hcrGuardVersioningOK)
          && (!prep->_expr->mergedWithOSRGuard() || _curLoop->_osrGuardVersioningOK))
         {
         emitPrep(prep, &comparisonTrees);
         improvement->improveLoop();
         }
      }

   // Substitute in loads of temps for expressions that have been privatized
   // throughout the loop.
   if (_curLoop->_privatizationsRequested && !_curLoop->_privTemps.empty())
      {
      // Since checks and conditionals have already been modified, both
      // removedNodes and takenBranches can be empty.
      TR::NodeChecklist empty(comp());
      TR::NodeChecklist *removedNodes = &empty;
      TR::NodeChecklist *takenBranches = &empty;
      LoopBodySearch search(
         comp(),
         _curLoop->_memRegion,
         whileLoop,
         removedNodes,
         takenBranches);

      TR::NodeChecklist visited(comp());
      for (; search.hasTreeTop(); search.advance())
         {
         TR::TreeTop *tt = search.currentTreeTop();
         TR::Node *node = tt->getNode();
         substitutePrivTemps(tt, node, &visited);

         TR::ILOpCode op = node->getOpCode();
         if (op.isNullCheck() || op.getOpCodeValue() == TR::DIVCHK)
            {
            TR::Node *child = node->getChild(0);
            if (child->getOpCode().isLoadDirect())
               {
               dumpOptDetails(
                  comp(),
                  "Removing check n%un [%p] because child has been privatized\n",
                  node->getGlobalIndex(),
                  node);

               TR::Node::recreate(node, TR::treetop);
               }
            }
         }
      }

   // Due to RAS changes to make each loop version test a transformation, disableOptTransformations or lastOptTransformationIndex can now potentially remove all the tests above the 2 versioned loops.  When there are two versions of the loop, it is necessary that there be at least one test at the top.  Therefore, the following is required to ensure that a test is created.
   size_t comparisonTreesCount = comparisonTrees.getSize();
   size_t privatizationCount = _curLoop->_privTemps.size();
   TR_ASSERT_FATAL(
      privatizationCount <= comparisonTreesCount,
      "more privatizations (%d) than entries in comparisonTrees (%d)",
      privatizationCount,
      comparisonTreesCount);

   size_t testCount = comparisonTreesCount - privatizationCount;
   if (testCount == 0)
      {
      TR::Node *constNode = TR::Node::create(blockHeadNode, TR::iconst, 0, 0);
      TR::Node *nextComparisonNode = TR::Node::createif(TR::ificmpne, constNode, constNode, clonedLoopInvariantBlock->getEntry());
      comparisonTrees.add(nextComparisonNode);
      dumpOptDetails(comp(), "The comparison tree is empty.  The node %p has been created because a test is required when there are two versions of the loop.\n", nextComparisonNode);
      }

   //if (debugBoundCheck)
   //   {
   //   TR::TreeTop *debugBoundCheckTree = TR::TreeTop::create(comp(), debugBoundCheck, NULL, NULL);
   //   clonedInvariantEntryTree->join(debugBoundCheckTree);
   //   debugBoundCheckTree->join(gotoTree);
   //   }


   // Add each test accumalated earlier into a block of its own
   // with a critical edge splitting goto block into the trees and
   // the CFG.
   //
   TR::Block *chooserBlock = NULL;
   ///////TR::Block *lastComparisonBlock = NULL;
   ListElement<TR::Node> *comparisonNode = comparisonTrees.getListHead();
   TR_ScratchList<TR::Block> comparisonBlocks(trMemory()), criticalEdgeBlocks(trMemory());
   TR::TreeTop *insertionPoint = invariantBlock->getEntry();
   TR::TreeTop *treeBeforeInsertionPoint = insertionPoint->getPrevTreeTop();

   bool firstComparisonNode = true;
   while (comparisonNode)
      {
      TR::Node *actualComparisonNode = comparisonNode->getData();
      TR::TreeTop *comparisonTree = TR::TreeTop::create(comp(), actualComparisonNode, NULL, NULL);
      TR::Block *comparisonBlock = TR::Block::createEmptyBlock(invariantBlock->getEntry()->getNode(), comp(), invariantBlock->getFrequency(), invariantBlock);
      comparisonBlock->setIsSpecialized(invariantBlock->isSpecialized());

      if (actualComparisonNode->getOpCode().isStore())
         {
         // No critical edge splitting necessary.
         }
      else if (firstComparisonNode)
         {
         firstComparisonNode = false;
         //////TR::Node::recreate(actualComparisonNode, actualComparisonNode->getOpCode().getOpCodeForReverseBranch());
         //////actualComparisonNode->setBranchDestination(invariantBlock->getEntry());
         //////lastComparisonBlock = comparisonBlock;
         }
      else
         {
         TR::Block *newGotoBlock = TR::Block::createEmptyBlock(invariantBlock->getEntry()->getNode(), comp(), 0, comparisonBlock);
         newGotoBlock->setIsSpecialized(comparisonBlock->isSpecialized());
         _cfg->addNode(newGotoBlock);

         if (trace())
            traceMsg(comp(), "Creating new goto block : %d for node %p\n", newGotoBlock->getNumber(), actualComparisonNode);

         actualComparisonNode->setBranchDestination(newGotoBlock->getEntry());
         TR::TreeTop *gotoBlockEntryTree = newGotoBlock->getEntry();
         TR::TreeTop *gotoBlockExitTree = newGotoBlock->getExit();
         TR::Node *gotoNode =  TR::Node::create(actualComparisonNode, TR::Goto);
         TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
         gotoNode->setBranchDestination(clonedLoopInvariantBlock->getEntry());
         gotoBlockEntryTree->join(gotoTree);
         gotoTree->join(gotoBlockExitTree);
         endTree->join(gotoBlockEntryTree);
         endTree = gotoBlockExitTree;
         //_cfg->addEdge(TR::CFGEdge::createEdge(comparisonBlock,  newGotoBlock, trMemory()));
         //_cfg->addEdge(TR::CFGEdge::createEdge(newGotoBlock,  clonedLoopInvariantBlock, trMemory()));
         TR_BlockStructure *newGotoBlockStructure = new (_cfg->structureMemoryRegion()) TR_BlockStructure(comp(), newGotoBlock->getNumber(), newGotoBlock);
         newGotoBlockStructure->setCreatedByVersioning(true);
         if (!_neitherLoopCold)
            {
            newGotoBlock->setIsCold();
            newGotoBlock->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
            }
         else if (_neitherLoopCold &&
                  shouldOnlySpecializeLoops())
            {
            //newGotoBlock->setIsRare();
            if (comparisonBlock->getFrequency() < 0)
               newGotoBlock->setFrequency(comparisonBlock->getFrequency());
            else
               {
               int32_t specializedBlockFrequency = TR::Block::getScaledSpecializedFrequency(comparisonBlock->getFrequency());
               if (comparisonBlock->isCold())
                  specializedBlockFrequency = comparisonBlock->getFrequency();
               else if (specializedBlockFrequency <= MAX_COLD_BLOCK_COUNT)
                  specializedBlockFrequency = MAX_COLD_BLOCK_COUNT+1;
               newGotoBlock->setFrequency(specializedBlockFrequency);
               }
            }
         criticalEdgeBlocks.add(newGotoBlock);
         }

      TR::TreeTop *comparisonEntryTree = comparisonBlock->getEntry();
      TR::TreeTop *comparisonExitTree = comparisonBlock->getExit();
      comparisonEntryTree->join(comparisonTree);
      comparisonTree->join(comparisonExitTree);

      comparisonExitTree->join(insertionPoint);

      if (treeBeforeInsertionPoint)
         treeBeforeInsertionPoint->join(comparisonEntryTree);
      else
         comp()->setStartTree(comparisonEntryTree);

      chooserBlock = comparisonBlock;
      comparisonBlocks.add(comparisonBlock);
      insertionPoint = comparisonEntryTree;
      comparisonNode = comparisonNode->getNextElement();
      }


   // Adjust the predecessors of the original loop invariant block
   // so that they now branch to the first block containing the
   // (first) versioning test.
   //
   while (!invariantBlock->getPredecessors().empty())
      {
      TR::CFGEdge * const nextPred = invariantBlock->getPredecessors().front();
      invariantBlock->getPredecessors().pop_front();
      nextPred->setTo(chooserBlock);
      TR::Block * const nextPredBlock = toBlock(nextPred->getFrom());
      if (nextPredBlock != _cfg->getStart())
         {
         TR::TreeTop *lastTreeInPred = nextPredBlock->getLastRealTreeTop();

         if (lastTreeInPred)
            lastTreeInPred->adjustBranchOrSwitchTreeTop(comp(), invariantBlock->getEntry(), chooserBlock->getEntry());
         }
      }


   //////TR::TreeTop *lastComparisonExit = lastComparisonBlock->getExit();
   //////TR::TreeTop *nextTreeAfterLastComparisonExit = lastComparisonExit->getNextTreeTop();
   //////lastComparisonExit->join(clonedInvariantEntryTree);
   //////clonedInvariantExitTree->join(nextTreeAfterLastComparisonExit);
   //
   TR::TreeTop *lastComparisonExit = invariantBlock->getExit();
   TR::TreeTop *nextTreeAfterLastComparisonExit = lastComparisonExit->getNextTreeTop();
   lastComparisonExit->join(clonedInvariantEntryTree);
   clonedInvariantExitTree->join(nextTreeAfterLastComparisonExit);
   TR::TreeTop *previousTree = lastComparisonExit->getPrevRealTreeTop();

   if (previousTree->getNode()->getOpCodeValue() != TR::Goto)
      {
      TR::Node *gotoNode =  TR::Node::create(previousTree->getNode(), TR::Goto);
      TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
      gotoNode->setBranchDestination(toBlock(invariantBlock->getSuccessors().front()->getTo())->getEntry());
      previousTree->join(gotoTree);
      gotoTree->join(lastComparisonExit);
      }

   if(!_neitherLoopCold ||
      invariantBlock->isCold())
      {
      clonedLoopInvariantBlock->setIsCold();
      if (invariantBlock->isSuperCold())
         clonedLoopInvariantBlock->setIsSuperCold();
      int32_t frequency = VERSIONED_COLD_BLOCK_COUNT;
      if (clonedLoopInvariantBlock->isCold())
         {
         int32_t blockFreq = invariantBlock->getFrequency();
         if (frequency > blockFreq)
            frequency = blockFreq;
         }
      clonedLoopInvariantBlock->setFrequency(frequency);
      }
   else if (_neitherLoopCold &&
            shouldOnlySpecializeLoops())
      {
      //clonedLoopInvariantBlock->setIsRare();
      if (invariantBlock->getFrequency() < 0)
         clonedLoopInvariantBlock->setFrequency(invariantBlock->getFrequency());
      else
         clonedLoopInvariantBlock->setFrequency(TR::Block::getScaledSpecializedFrequency(invariantBlock->getFrequency()));
      }

   TR_ByteCodeInfo &bcInfo = clonedLoopInvariantBlock->getEntry()->getNode()->getByteCodeInfo();
   TR::DebugCounter::prependDebugCounter(comp(),
   TR::DebugCounter::debugCounterName(comp(), "loopVersioner.coldLoop/%d/(%s)/%s/bcinfo=%d.%d", blockAtHeadOfLoop->getFrequency(), comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()), bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex()),
   clonedLoopInvariantBlock->getEntry()->getNextTreeTop(), 1, TR::DebugCounter::Free);

   // Add CFG edges correct for the newly created comparison test
   // blocks and the corresponding critical edge splitting blocks.
   //
   ListElement<TR::Block> *currComparisonBlock = comparisonBlocks.getListHead();
   ListElement<TR::Block> *currCriticalEdgeBlock = criticalEdgeBlocks.getListHead();

   while (currComparisonBlock)
      {
      TR::Block *currentBlock = currComparisonBlock->getData();
      _cfg->addNode(currentBlock);
      ListElement<TR::Block> *nextComparisonBlock = currComparisonBlock->getNextElement();
      bool isTest =
         !currentBlock->getLastRealTreeTop()->getNode()->getOpCode().isStore();
      const char *debugCounter = NULL;
      if (isTest)
         debugCounter = TR::DebugCounter::debugCounterName(comp(), "loopVersioner.fail/(%s)/%s/origin=block_%d", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()), currentBlock->getNumber());

      if (nextComparisonBlock)
         _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock, nextComparisonBlock->getData(), trMemory()));
      else
         _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  invariantBlock, trMemory()));

      if (isTest)
         {
         if (currCriticalEdgeBlock == NULL)
            {
            _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  clonedLoopInvariantBlock, trMemory()));
            }
         else
            {
            _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock, currCriticalEdgeBlock->getData(), trMemory()));
            _cfg->addEdge(TR::CFGEdge::createEdge(currCriticalEdgeBlock->getData(), clonedLoopInvariantBlock, trMemory()));
            TR::DebugCounter::prependDebugCounter(comp(), debugCounter, currCriticalEdgeBlock->getData()->getEntry()->getNextTreeTop());
            currCriticalEdgeBlock = currCriticalEdgeBlock->getNextElement();
            }
         }

      currComparisonBlock = nextComparisonBlock;
      }

   _cfg->addEdge(TR::CFGEdge::createEdge(clonedLoopInvariantBlock,  blockAtHeadOfClonedLoop, trMemory()));
   _cfg->setStructure(_rootStructure);

   // Done with trees and CFG changes
   //
   // Now modify the structure appropriately
   //
   // We will essentially create an acyclic region to replace the original loop
   // invariant block structure and natural loop structure. This acyclic region
   // will contain all the versioning test blocks, critical edge splitting blocks,
   // and other goto blocks created earlier as simple block structures. We will
   // clone the original natural loop structure and make these natural loop
   // structures successors of some of the block structures mentioned above
   // as appropriate.
   //
   TR_StructureSubGraphNode **correspondingSubNodes = (TR_StructureSubGraphNode **)trMemory()->allocateStackMemory(numNodes*sizeof(TR_StructureSubGraphNode *));
   memset(correspondingSubNodes, 0, numNodes*sizeof(TR_StructureSubGraphNode *));

   TR_RegionStructure *clonedWhileLoop = whileLoop->cloneStructure(correspondingBlocks, correspondingSubNodes, innerWhileLoops, clonedInnerWhileLoops)->asRegion();
   clonedWhileLoop->cloneStructureEdges(correspondingBlocks);
   clonedWhileLoop->setVersionedLoop(whileLoop);
   whileLoop->setVersionedLoop(clonedWhileLoop);

   TR_BlockStructure *invariantBlockStructure = invariantBlock->getStructureOf();
   TR_BlockStructure *clonedInvariantBlockStructure = new (_cfg->structureMemoryRegion()) TR_BlockStructure(comp(), clonedLoopInvariantBlock->getNumber(), clonedLoopInvariantBlock);
   clonedInvariantBlockStructure->setCreatedByVersioning(true);

   if (!_neitherLoopCold)
      clonedInnerWhileLoops->deleteAll();
   clonedInvariantBlockStructure->setAsLoopInvariantBlock(true);
   TR_RegionStructure *parentStructure = whileLoop->getParent()->asRegion();
   TR_RegionStructure *properRegion = new (_cfg->structureMemoryRegion()) TR_RegionStructure(comp(), chooserBlock->getNumber());
   parentStructure->replacePart(invariantBlockStructure, properRegion);

   TR_StructureSubGraphNode *clonedWhileNode = new (_cfg->structureMemoryRegion()) TR_StructureSubGraphNode(clonedWhileLoop);
   TR_StructureSubGraphNode *whileNode = new (_cfg->structureMemoryRegion()) TR_StructureSubGraphNode(whileLoop);
   TR_StructureSubGraphNode *invariantNode = new (_cfg->structureMemoryRegion()) TR_StructureSubGraphNode(invariantBlockStructure);
   TR_StructureSubGraphNode *clonedInvariantNode = new (_cfg->structureMemoryRegion()) TR_StructureSubGraphNode(clonedInvariantBlockStructure);

   properRegion->addSubNode(whileNode);
   properRegion->addSubNode(clonedWhileNode);
   properRegion->addSubNode(invariantNode);
   properRegion->addSubNode(clonedInvariantNode);

   TR_StructureSubGraphNode *prevComparisonNode = NULL;
   TR_StructureSubGraphNode *regionEntryNode = NULL;
   currComparisonBlock = comparisonBlocks.getListHead();
   currCriticalEdgeBlock = criticalEdgeBlocks.getListHead();

   // Create block structures for the comparison test blocks and
   // add them to the new acyclic region. Each one will have
   // a corresponding critical edge splitting block and we will
   // also create/add these block structures simultaneously.
   //
   while (currComparisonBlock)
      {
      TR::Block *actualComparisonBlock = currComparisonBlock->getData();
      bool isTest =
         !actualComparisonBlock->getLastRealTreeTop()->getNode()->getOpCode().isStore();

      TR_BlockStructure *comparisonBlockStructure = new (_cfg->structureMemoryRegion()) TR_BlockStructure(comp(), actualComparisonBlock->getNumber(), actualComparisonBlock);
      comparisonBlockStructure->setCreatedByVersioning(true);
      TR_StructureSubGraphNode *comparisonNode = new (_cfg->structureMemoryRegion()) TR_StructureSubGraphNode(comparisonBlockStructure);
      properRegion->addSubNode(comparisonNode);

      if (prevComparisonNode)
         TR::CFGEdge::createEdge(prevComparisonNode,  comparisonNode, trMemory());
      else
         {
         regionEntryNode = comparisonNode;
         properRegion->setEntry(regionEntryNode);
         }

      //TR::CFGEdge::createEdge(comparisonNode,  clonedInvariantNode, trMemory());
      prevComparisonNode = comparisonNode;
      currComparisonBlock = currComparisonBlock->getNextElement();

      if (isTest)
         {
         if (currCriticalEdgeBlock != NULL)
            {
            TR_StructureSubGraphNode *criticalEdgeNode = new (_cfg->structureMemoryRegion()) TR_StructureSubGraphNode(currCriticalEdgeBlock->getData()->getStructureOf());
            properRegion->addSubNode(criticalEdgeNode);
            TR::CFGEdge::createEdge(prevComparisonNode,  criticalEdgeNode, trMemory());
            TR::CFGEdge::createEdge(criticalEdgeNode,  clonedInvariantNode, trMemory());
            currCriticalEdgeBlock = currCriticalEdgeBlock->getNextElement();
            }
         else
            TR::CFGEdge::createEdge(prevComparisonNode,  clonedInvariantNode, trMemory());
         }
      }

   TR::CFGEdge::createEdge(prevComparisonNode,  invariantNode, trMemory());
   TR::CFGEdge::createEdge(invariantNode,  whileNode, trMemory());
   TR::CFGEdge::createEdge(clonedInvariantNode,  clonedWhileNode, trMemory());

   // Since the new proper region replaced the original loop invariant
   // block in the parent structure, the successor of the loop
   // invariant block structure (the natural loop originally) must now be removed
   // from the parent structure as the natural loop would be moved into the proper
   // region.
   //
   TR_StructureSubGraphNode *properNode = NULL;
   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      if (subStruct == properRegion)
         {
         properNode = subNode;
         TR::CFGEdge *succEdge = subNode->getSuccessors().front();
         TR_StructureSubGraphNode *succNode = toStructureSubGraphNode(succEdge->getTo());
         succNode->removePredecessor(succEdge);
         subNode->removeSuccessor(succEdge);

         for (auto changedSuccEdge = succNode->getSuccessors().begin(); changedSuccEdge != succNode->getSuccessors().end(); ++changedSuccEdge)
            {
            //
            // Watch for any edge from the original loop to the invariant
            // block - it will now refer to the proper region node, and
            // must be discarded from the list of predecessors of the
            // proper region node in the context of the parent structure, as a
            // subgraph node must not have an edge to itself
            if ((*changedSuccEdge)->getTo() != properNode)
               {
               (*changedSuccEdge)->setFrom(properNode);
               }
            else
               {
               properNode->removePredecessor(*changedSuccEdge);
               }
            }

         for (auto changedSuccEdge = succNode->getExceptionSuccessors().begin(); changedSuccEdge != succNode->getExceptionSuccessors().end(); ++changedSuccEdge)
            (*changedSuccEdge)->setExceptionFrom(properNode);

         parentStructure->removeSubNode(succNode);
         succNode->getStructure()->setParent(properRegion);
         break;
         }
      }

   // Set the edges correctly in the parent structure now; the
   // exit edges must have the proper node as the from node
   // now instead of the natural loop node.
   //
   ListIterator<TR::CFGEdge> ei(&parentStructure->getExitEdges());
   TR::CFGEdge *exitEdge;
   for (exitEdge = ei.getCurrent();  exitEdge != NULL; exitEdge = ei.getNext())
      {
      TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(exitEdge->getFrom());
      TR_Structure *fromStruct = fromNode->getStructure();
      int32_t toNum = exitEdge->getTo()->getNumber();

      if (fromStruct == whileLoop)
         {
         // See if it is a regular edge or an exception edge.
         //
         auto succEdge = fromNode->getExceptionSuccessors().begin();
         for (; succEdge != fromNode->getExceptionSuccessors().end(); ++succEdge)
            {
            if ((*succEdge)->getTo()->getNumber() == toNum)
               break;
            }

         //toStructureSubGraphNode(exitEdge->getFrom())->setStructure(properRegion);
         if (succEdge != fromNode->getExceptionSuccessors().end())
            {
            exitEdge->setExceptionFrom(properNode);
            //properNode->addExceptionSuccessor(exitEdge);
            }
         else
            {
            exitEdge->setFrom(properNode);
            //properNode->addSuccessor(exitEdge);
            }
         }
      }


   // Add these goto block structures required for fixing up the
   // fall throughs properly.
   //
   ListIterator<TR_BlockStructure> newGotoBlockStructuresIt(&newGotoBlockStructures);
   TR_BlockStructure *newGotoBlockStructure;
   for (newGotoBlockStructure = newGotoBlockStructuresIt.getCurrent(); newGotoBlockStructure; newGotoBlockStructure = newGotoBlockStructuresIt.getNext())
      {
      TR_StructureSubGraphNode *newGotoBlockNode = new (_cfg->structureMemoryRegion()) TR_StructureSubGraphNode(newGotoBlockStructure);
      properRegion->addSubNode(newGotoBlockNode);
      TR::CFGEdge::createEdge(clonedWhileNode,  newGotoBlockNode, trMemory());
      }

   // Add appropriate exit edges into the new proper region based
   // on the original loop's exit edges.
   //
   TR_BitVector seenExitNodes(numNodes, trMemory());
   ei.set(&whileLoop->getExitEdges());
   for (exitEdge = ei.getCurrent(); exitEdge; exitEdge = ei.getNext())
      {
      TR_StructureSubGraphNode *node = toStructureSubGraphNode(exitEdge->getTo());
      if (!seenExitNodes.get(node->getNumber()))
         {
         bool isExceptionEdge = false;
         if (exitEdge->getFrom()->hasExceptionSuccessor(node))
            isExceptionEdge = true;

         // Check whether this exit is from the while loop to the new region
         // If it is, it should not be added as an exit from the region itself,
         // but rather as an edge from the while loop to the start of the new
         // region
         if (node->getNumber() == regionEntryNode->getNumber())
            {
            TR::CFGEdge::createEdge(whileNode, regionEntryNode, trMemory());
            }
         else
            {
            properRegion->addExitEdge(whileNode, node->getNumber(), isExceptionEdge);
            }
         seenExitNodes.set(node->getNumber());
         }
      }

   // If the original while loop exited to the loop invariant block, ensure the
   // structure of the cloned version of the loop exits to the new proper region
   ei.set(&clonedWhileLoop->getExitEdges());
   for (exitEdge = ei.getCurrent(); exitEdge; exitEdge = ei.getNext())
      {
      if (exitEdge->getTo()->getNumber() == invariantBlockStructure->getNumber())
         {
         clonedWhileLoop->replaceExitPart(invariantBlockStructure->getNumber(), properRegion->getNumber());
         break;
         }
      }

   // Patch up the cloned while loop edges properly
   //
   seenExitNodes.empty();
   ei.reset();
   for (exitEdge = ei.getCurrent(); exitEdge; exitEdge = ei.getNext())
      {
      TR_StructureSubGraphNode *node = toStructureSubGraphNode(exitEdge->getTo());
      if (!seenExitNodes.get(node->getNumber()))
         {
         bool isExceptionEdge = false;
         if (exitEdge->getFrom()->hasExceptionSuccessor(node))
            isExceptionEdge = true;

         // Check whether this exit is from the while loop to the new region
         // If it is, it should not be added as an exit from the region itself,
         // but rather as an edge from the while loop to the start of the new
         // region
         if (node->getNumber() == regionEntryNode->getNumber())
            {
            TR::CFGEdge::createEdge(clonedWhileNode, regionEntryNode, trMemory());
            }
         else
            {
            properRegion->addExitEdge(clonedWhileNode, node->getNumber(), isExceptionEdge);
            }
         seenExitNodes.set(node->getNumber());
         }
      }

   newGotoBlockStructuresIt.reset();
   for (newGotoBlockStructure = newGotoBlockStructuresIt.getCurrent(); newGotoBlockStructure; newGotoBlockStructure = newGotoBlockStructuresIt.getNext())
      {
      TR::Block *predBlock = toBlock(newGotoBlockStructure->getBlock()->getPredecessors().front()->getFrom());
      clonedWhileLoop->addExternalEdge(predBlock->getStructureOf(), newGotoBlockStructure->getNumber(), false);
      }

   newGotoBlockStructuresIt.reset();
   for (newGotoBlockStructure = newGotoBlockStructuresIt.getCurrent(); newGotoBlockStructure; newGotoBlockStructure = newGotoBlockStructuresIt.getNext())
      {
      TR::Block *predBlock = toBlock(newGotoBlockStructure->getBlock()->getPredecessors().front()->getFrom());
      TR::Block *succBlock = toBlock(newGotoBlockStructure->getBlock()->getSuccessors().front()->getTo());
      properRegion->addExternalEdge(newGotoBlockStructure, succBlock->getStructureOf()->getNumber(), false);
      properRegion->removeExternalEdgeTo(predBlock->getStructureOf(), succBlock->getStructureOf()->getNumber());
      }

   if (trace())
      comp()->dumpMethodTrees("Trees after this versioning");
   }

void TR_LoopVersioner::RemoveAsyncCheck::improveLoop()
   {
   TR::Node *asyncCheckNode = _asyncCheckTree->getNode();
   dumpOptDetails(
      comp(),
      "Removing asynccheck n%un [%p]\n",
      asyncCheckNode->getGlobalIndex(),
      asyncCheckNode);

   comp()->setLoopWasVersionedWrtAsyncChecks(true);
   TR::TreeTop *prevTree = _asyncCheckTree->getPrevTreeTop();
   TR::TreeTop *nextTree = _asyncCheckTree->getNextTreeTop();
   prevTree->join(nextTree);

   TR_RegionStructure *whileLoop = _versioner->_currentNaturalLoop;
   whileLoop->getEntryBlock()->getStructureOf()->setIsEntryOfShortRunningLoop();
   if (_versioner->trace())
      {
      traceMsg(
         comp(),
         "Marked block %p with entry %p\n",
         whileLoop->getEntryBlock(),
         whileLoop->getEntryBlock()->getEntry()->getNode());
      }
   }

void TR_LoopVersioner::buildNullCheckComparisonsTree(
   List<TR::Node> *nullCheckedReferences,
   List<TR::TreeTop> *nullCheckTrees)
   {
   ListElement<TR::Node> *nextNode = nullCheckedReferences->getListHead();
   ListElement<TR::TreeTop> *nextTree = nullCheckTrees->getListHead();

   for (; nextNode; nextNode = nextNode->getNextElement(), nextTree = nextTree->getNextElement())
      {
      // A NULLCHK might have been specialized away, but will still appear in nullCheckTrees
      // Skip such trees
      TR::Node *nullChkNode = nextTree->getData()->getNode();

      if (!nullChkNode->getOpCode().isNullCheck())
         {
         TR_ASSERT_FATAL(nullChkNode->getOpCodeValue() == TR::treetop, "Unexpected opcode for n%dn [%p]\n", nullChkNode->getGlobalIndex(), nullChkNode);
         continue;
         }

      bool isNextNodeInvariant = isExprInvariant(nextNode->getData());
      TR::Node *nodeToBeNullChkd = NULL;
      bool isDependent = false;
      bool nullCheckReferenceisField=false;
      if (!isNextNodeInvariant)
         {
         bool nullCheckReferenceisField = nextNode->getData()->getOpCode().isLoadIndirect() &&
              nextNode->getData()->getOpCode().hasSymbolReference() &&
              !_seenDefinedSymbolReferences->get(nextNode->getData()->getSymbolReference()->getReferenceNumber()) &&
              nextNode->getData()->getFirstChild()->getOpCode().hasSymbolReference() &&
              nextNode->getData()->getFirstChild()->getSymbolReference()->getSymbol()->isAuto();

         if ((nextNode->getData()->getOpCode().hasSymbolReference() &&
             nextNode->getData()->getSymbolReference()->getSymbol()->isAuto()) ||
             nullCheckReferenceisField)
            {
            TR::Node *invariantNullCheckReference = isDependentOnInvariant(nextNode->getData());

            if(!invariantNullCheckReference && nullCheckReferenceisField)
                 invariantNullCheckReference =  isDependentOnInvariant(nextNode->getData()->getFirstChild());

            if (invariantNullCheckReference)
               {
               TR::Node *dupInvariantNullCheckReference;
               if(nullCheckReferenceisField)
                  {
                  dupInvariantNullCheckReference=nextNode->getData()->duplicateTreeForCodeMotion();
                  dupInvariantNullCheckReference->setAndIncChild(0,invariantNullCheckReference->duplicateTreeForCodeMotion());
                  nodeToBeNullChkd = dupInvariantNullCheckReference;
                  }
               else
                  {
                  nodeToBeNullChkd = invariantNullCheckReference;
                  }

               isDependent = true;
               }
            else
               TR_ASSERT(0, "Should not be considering null check for versioning\n");
            }
         else
            TR_ASSERT(0, "Should not be considering null check for versioning\n");
         }

      if (!nodeToBeNullChkd)
         nodeToBeNullChkd = nextNode->getData();

      if (performTransformation(
            comp(),
            "%s Creating test outside loop for checking if n%un [%p] is null at n%un [%p]\n",
            OPT_DETAILS_LOOP_VERSIONER,
            nextNode->getData()->getGlobalIndex(),
            nextNode->getData(),
            nextTree->getData()->getNode()->getGlobalIndex(),
            nextTree->getData()->getNode()))
         {
         ///TR::Node *duplicateNullCheckReference = nextNode->getData()->duplicateTree();
         TR::Node *duplicateNullCheckReference = NULL;
         if (isDependent)
            {
            if(nullCheckReferenceisField)
               duplicateNullCheckReference = nodeToBeNullChkd;
            else
               duplicateNullCheckReference = nodeToBeNullChkd->duplicateTreeForCodeMotion();
            }
         else
            duplicateNullCheckReference = nextNode->getData()->duplicateTreeForCodeMotion();

         //TR::Node *nextComparisonNode = TR::Node::createif(TR::ifacmpeq, duplicateNullCheckReference, TR::Node::create(duplicateNullCheckReference, TR::aconst, 0, 0), _exitGotoTarget);
         TR::Node *nextComparisonNode = TR::Node::createif(TR::ifacmpeq, duplicateNullCheckReference, TR::Node::aconst(duplicateNullCheckReference, 0), _exitGotoTarget);
         LoopEntryPrep *prep =
            createLoopEntryPrep(LoopEntryPrep::TEST, nextComparisonNode);
         if (prep != NULL)
            {
            TR::Node *checkNode = nextTree->getData()->getNode();
            nodeWillBeRemovedIfPossible(checkNode, prep);
            _curLoop->_loopImprovements.push_back(
               new (_curLoop->_memRegion) RemoveNullCheck(this, prep, checkNode));
            }
         }
      }
   }

void TR_LoopVersioner::RemoveNullCheck::improveLoop()
   {
   dumpOptDetails(
      comp(),
      "Removing null check n%un [%p]\n",
      _nullCheckNode->getGlobalIndex(),
      _nullCheckNode);

   if (_nullCheckNode->getOpCodeValue() == TR::NULLCHK)
      TR::Node::recreate(_nullCheckNode, TR::treetop);
   else if (_nullCheckNode->getOpCodeValue() == TR::ResolveAndNULLCHK)
      TR::Node::recreate(_nullCheckNode, TR::ResolveCHK);
   else
      TR_ASSERT_FATAL(false, "unexpected opcode");
   }

void TR_LoopVersioner::buildAwrtbariComparisonsTree(List<TR::TreeTop> *awrtbariTrees)
   {
#ifdef J9_PROJECT_SPECIFIC
   ListElement<TR::TreeTop> *nextTree = awrtbariTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *awrtbariTree = nextTree->getData();
      TR::Node *awrtbariNode = awrtbariTree->getNode();
      if (awrtbariNode->getOpCodeValue() != TR::awrtbari)
        awrtbariNode = awrtbariNode->getFirstChild();

      if (performTransformation(
            comp(),
            "%s Creating test outside loop for checking if n%un [%p] requires a write barrier\n",
            OPT_DETAILS_LOOP_VERSIONER,
            awrtbariNode->getGlobalIndex(),
            awrtbariNode))
         {
         TR::Node *duplicateBase = awrtbariNode->getLastChild()->duplicateTreeForCodeMotion();
         TR::Node *ifNode, *ifNode1, *ifNode2;

         // If we can guarantee a fixed tenure size we can hardcode size constants
         bool isVariableHeapBase = comp()->getOptions()->isVariableHeapBaseForBarrierRange0();
         bool isVariableHeapSize = comp()->getOptions()->isVariableHeapSizeForBarrierRange0();

         TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

         if (isVariableHeapBase || isVariableHeapSize)
            {
            ifNode1 =  TR::Node::create(TR::acmpge, 2, duplicateBase, TR::Node::createWithSymRef(TR::aload, 0, comp()->getSymRefTab()->findOrCreateThreadLowTenureAddressSymbolRef()));
            }
         else
            {
            ifNode1 =  TR::Node::create(TR::acmpge, 2, duplicateBase, TR::Node::aconst(duplicateBase, fej9->getLowTenureAddress()));
            }

         duplicateBase = awrtbariNode->getLastChild()->duplicateTreeForCodeMotion();

         if (isVariableHeapBase || isVariableHeapSize)
            {
            ifNode2 =  TR::Node::create(TR::acmplt, 2, duplicateBase, TR::Node::createWithSymRef(TR::aload, 0, comp()->getSymRefTab()->findOrCreateThreadHighTenureAddressSymbolRef()));
            }
         else
            {
            ifNode2 =  TR::Node::create(TR::acmplt, 2, duplicateBase, TR::Node::aconst(duplicateBase, fej9->getHighTenureAddress()));
            }

         ifNode = TR::Node::createif(TR::ificmpne, TR::Node::create(TR::iand, 2, ifNode1, ifNode2), TR::Node::create(duplicateBase, TR::iconst, 0, 0), _exitGotoTarget);

         LoopEntryPrep *prep = createLoopEntryPrep(LoopEntryPrep::TEST, ifNode);
         if (prep != NULL)
            {
            // nodeWillBeRemovedIfPossible() doesn't apply here.
            _curLoop->_loopImprovements.push_back(
               new (_curLoop->_memRegion) RemoveWriteBarrier(
                  this,
                  prep,
                  awrtbariNode));
            }
         }

      nextTree = nextTree->getNextElement();
      }
#endif
   }

void TR_LoopVersioner::RemoveWriteBarrier::improveLoop()
   {
   dumpOptDetails(
      comp(),
      "Removing write barrier n%un [%p]\n",
      _awrtbariNode->getGlobalIndex(),
      _awrtbariNode);

   TR_ASSERT_FATAL(_awrtbariNode->getOpCodeValue() == TR::awrtbari, "unexpected opcode");
   _awrtbariNode->setSkipWrtBar(true);
   }

void TR_LoopVersioner::buildDivCheckComparisonsTree(List<TR::TreeTop> *divCheckTrees)
   {
   ListElement<TR::TreeTop> *nextTree = divCheckTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *divCheckTree = nextTree->getData();
      TR::Node *divCheckNode = divCheckTree->getNode();

      if (performTransformation(
            comp(),
            "%s Creating test outside loop for checking if n%un [%p] is divide by zero\n",
            OPT_DETAILS_LOOP_VERSIONER,
            divCheckNode->getGlobalIndex(),
            divCheckNode))
         {
         TR::Node *duplicateDivisor = divCheckNode->getFirstChild()->getSecondChild()->duplicateTreeForCodeMotion();
         TR::Node *ifNode;
         if (duplicateDivisor->getType().isInt64())
            ifNode =  TR::Node::createif(TR::iflcmpeq, duplicateDivisor, TR::Node::create(duplicateDivisor, TR::lconst, 0, 0), _exitGotoTarget);
         else
            ifNode =  TR::Node::createif(TR::ificmpeq, duplicateDivisor, TR::Node::create(duplicateDivisor, TR::iconst, 0, 0), _exitGotoTarget);

         LoopEntryPrep *prep = createLoopEntryPrep(LoopEntryPrep::TEST, ifNode);
         if (prep != NULL)
            {
            nodeWillBeRemovedIfPossible(divCheckNode, prep);
            _curLoop->_loopImprovements.push_back(
               new (_curLoop->_memRegion) RemoveDivCheck(this, prep, divCheckNode));
            }
         }

      nextTree = nextTree->getNextElement();
      }
   }

void TR_LoopVersioner::RemoveDivCheck::improveLoop()
   {
   dumpOptDetails(
      comp(),
      "Removing div check n%un [%p]\n",
      _divCheckNode->getGlobalIndex(),
      _divCheckNode);

   TR_ASSERT_FATAL(_divCheckNode->getOpCodeValue() == TR::DIVCHK, "unexpected opcode");
   TR::Node::recreate(_divCheckNode, TR::treetop);
   }

void TR_LoopVersioner::buildCheckCastComparisonsTree(List<TR::TreeTop> *checkCastTrees)
   {
   ListElement<TR::TreeTop> *nextTree = checkCastTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *checkCastTree = nextTree->getData();
      TR::Node *checkCastNode = checkCastTree->getNode();

      if (!performTransformation(
            comp(),
            "%s Creating test outside loop for checking that checkcast n%un [%p] passes\n",
            OPT_DETAILS_LOOP_VERSIONER,
            checkCastNode->getGlobalIndex(),
            checkCastNode))
         {
         nextTree = nextTree->getNextElement();
         continue;
         }

      TR::Node *duplicateClassPtr = checkCastNode->getSecondChild()->duplicateTreeForCodeMotion();
      TR::Node *duplicateCheckedValue = checkCastNode->getFirstChild()->duplicateTreeForCodeMotion();
      TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, duplicateCheckedValue, duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
      TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(checkCastNode, TR::iconst, 0, 0), _exitGotoTarget);

      LoopEntryPrep *prep = createLoopEntryPrep(LoopEntryPrep::TEST, ificmpeqNode);
      if (prep != NULL)
         {
         nodeWillBeRemovedIfPossible(checkCastNode, prep);
         _curLoop->_loopImprovements.push_back(
            new (_curLoop->_memRegion) RemoveCheckCast(this, prep, checkCastTree));
         }

      nextTree = nextTree->getNextElement();
      }
   }

void TR_LoopVersioner::RemoveCheckCast::improveLoop()
   {
   TR::Node *checkCastNode = _checkCastTree->getNode();
   dumpOptDetails(
      comp(),
      "Removing checkcast n%un [%p]\n",
      checkCastNode->getGlobalIndex(),
      checkCastNode);

   TR_ASSERT_FATAL(checkCastNode->getOpCode().isCheckCast(), "unexpected opcode");

   TR::TreeTop *prevTreeTop = _checkCastTree->getPrevTreeTop();
   TR::TreeTop *nextTreeTop = _checkCastTree->getNextTreeTop();
   TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, checkCastNode->getFirstChild()), NULL, NULL);
   TR::TreeTop *secondNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, checkCastNode->getSecondChild()), NULL, NULL);

   prevTreeTop->join(firstNewTree);
   firstNewTree->join(secondNewTree);
   secondNewTree->join(nextTreeTop);
   checkCastNode->recursivelyDecReferenceCount();
   }

void TR_LoopVersioner::buildArrayStoreCheckComparisonsTree(List<TR::TreeTop> *arrayStoreCheckTrees)
   {
   ListElement<TR::TreeTop> *nextTree = arrayStoreCheckTrees->getListHead();
   while (nextTree)
      {
      if (!performTransformation(
            comp(),
            "%s Creating test outside loop for checking if n%un [%p] is casted\n",
            OPT_DETAILS_LOOP_VERSIONER,
            nextTree->getData()->getNode()->getGlobalIndex(),
            nextTree->getData()->getNode()))
	 {
         nextTree = nextTree->getNextElement();
         continue;
	 }


      TR::TreeTop *arrayStoreCheckTree = nextTree->getData();
      TR::Node *arrayStoreCheckNode = arrayStoreCheckTree->getNode();

      TR::Node *childNode = arrayStoreCheckNode->getFirstChild();
      TR::Node *arrayNode = NULL;
      TR::Node *valueNode = NULL;
      if (childNode->getOpCode().isWrtBar())
         {
         int32_t lastChild = childNode->getNumChildren()-1;
         arrayNode = childNode->getChild(lastChild);
         valueNode = childNode->getChild(lastChild-1);
         }

      TR_ASSERT(arrayNode && valueNode, "unexpect node for ArrayStoreCheck");
      TR::Node *addressNode = valueNode->getFirstChild();
      TR_ASSERT(addressNode->getOpCode().isArrayRef(), "expecting array ref for addressNode");
      TR::Node *childOfAddressNode = addressNode->getFirstChild();

      TR::Node *duplicateSrcArray = arrayNode->duplicateTreeForCodeMotion();
      TR::Node *duplicateClassPtr = TR::Node::createWithSymRef(TR::aloadi, 1, 1, duplicateSrcArray, comp()->getSymRefTab()->findOrCreateVftSymbolRef());
      TR::Node *duplicateCheckedValue = childOfAddressNode->duplicateTreeForCodeMotion();

      TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, duplicateCheckedValue,  duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
      TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(arrayStoreCheckNode, TR::iconst, 0, 0), _exitGotoTarget);

      LoopEntryPrep *prep = createLoopEntryPrep(LoopEntryPrep::TEST, ificmpeqNode);
      if (prep != NULL)
         {
         nodeWillBeRemovedIfPossible(arrayStoreCheckNode, prep);
         _curLoop->_loopImprovements.push_back(
            new (_curLoop->_memRegion) RemoveArrayStoreCheck(
               this,
               prep,
               arrayStoreCheckTree));
         }

      nextTree = nextTree->getNextElement();
      }
   }

void TR_LoopVersioner::RemoveArrayStoreCheck::improveLoop()
   {
   TR::Node *arrayStoreCheckNode = _arrayStoreCheckTree->getNode();
   dumpOptDetails(
      comp(),
      "Removing array store check n%un [%p]\n",
      arrayStoreCheckNode->getGlobalIndex(),
      arrayStoreCheckNode);

   TR_ASSERT_FATAL(arrayStoreCheckNode->getOpCodeValue() == TR::ArrayStoreCHK, "unexpected opcode");

   TR::TreeTop *prevTreeTop = _arrayStoreCheckTree->getPrevTreeTop();
   TR::TreeTop *nextTreeTop = _arrayStoreCheckTree->getNextTreeTop();
   TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, arrayStoreCheckNode->getFirstChild()), NULL, NULL);
   TR::Node *child = arrayStoreCheckNode->getFirstChild();
   if (child->getOpCodeValue() == TR::awrtbari && TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_none &&
      performTransformation(comp(), "%sChanging awrtbari node [%p] to an iastore\n", OPT_DETAILS_LOOP_VERSIONER, child))
      {
      TR::Node::recreate(child, TR::astorei);
      child->getChild(2)->recursivelyDecReferenceCount();
      child->setNumChildren(2);
      }

   TR::TreeTop *secondNewTree = NULL;
   if (arrayStoreCheckNode->getNumChildren() > 1)
      {
      TR_ASSERT((arrayStoreCheckNode->getNumChildren() == 2), "Unknown array store check tree\n");
      secondNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, arrayStoreCheckNode->getSecondChild()), NULL, NULL);
      child = arrayStoreCheckNode->getSecondChild();
      if (child->getOpCodeValue() == TR::awrtbari && TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_none &&
          performTransformation(comp(), "%sChanging awrtbari node [%p] to an iastore\n", OPT_DETAILS_LOOP_VERSIONER, child))
         {
         TR::Node::recreate(child, TR::astorei);
         child->getChild(2)->recursivelyDecReferenceCount();
         child->setNumChildren(2);
         }
      }

   prevTreeTop->join(firstNewTree);
   if (secondNewTree)
      {
      firstNewTree->join(secondNewTree);
      secondNewTree->join(nextTreeTop);
      }
   else
      firstNewTree->join(nextTreeTop);

   arrayStoreCheckNode->recursivelyDecReferenceCount();
   }

void TR_LoopVersioner::convertSpecializedLongsToInts(TR::Node *node, vcount_t visitCount, TR::SymbolReference **symRefs)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if ((node->getType().isInt64()) &&
       node->getOpCode().isLoadVar())
      {
      TR::SymbolReference *tempSymRef = symRefs[node->getSymbolReference()->getReferenceNumber()];
      if (tempSymRef)
         {
         TR::Node::recreate(node, TR::iu2l);
         TR::Node *newLoad = TR::Node::createWithSymRef(node, TR::iload, 0, tempSymRef);
         node->setNumChildren(1);
         node->setAndIncChild(0, newLoad);
         //printf("Replacing long by int in %s\n", comp()->signature());
         }
      }

   int32_t childNum;
   for (childNum=0;childNum < node->getNumChildren(); childNum++)
     convertSpecializedLongsToInts(node->getChild(childNum), visitCount, symRefs);
   }



int nodeSize(TR::Node *n)
{
   int sz;

   sz = 0;
   for (int32_t i = 0; i < n->getNumChildren(); ++i)
      sz += nodeSize(n->getChild(i));
   return sz + 1;
}


bool nodeTreeContainsOpCode(TR::Node *n, TR::ILOpCodes op)
   {
   if(n->getOpCodeValue() == op)
      return true;
   for (int32_t i = 0; i < n->getNumChildren(); ++i)
      {
      if(nodeTreeContainsOpCode(n->getChild(i), op))
         return true;
      }
   return false;
   }



bool TR_LoopVersioner::buildLoopInvariantTree(List<TR_NodeParentSymRef> *invariantNodes)
   {
   TR::NodeChecklist seen(comp());

   ListElement<TR_NodeParentSymRef> *nextInvariantNode = invariantNodes->getListHead();
   while (nextInvariantNode)
      {
      TR::Node *invariantNode = nextInvariantNode->getData()->_node;
      if (seen.contains(invariantNode))
         {
         nextInvariantNode = nextInvariantNode->getNextElement();
         continue;
         }

      seen.add(invariantNode);

      // Heuristic: Pulling out really small trees doesn't help reduce
      // the amount of computation much, and just increases register pressure.
      //
      // Don't do it. Instead, count the total number of nodes in a tree, and
      // only pull it out if there are 4 or more nodes. This number is arbitrary,
      // and could probably be tweaked.

      if (nodeSize(invariantNode) < 4)
         {
         if (trace())
            traceMsg(comp(), "skipping undersized tree %p\n", nextInvariantNode->getData()->_node);
         nextInvariantNode = nextInvariantNode->getNextElement();
         continue;
         }

      if (performTransformation(
            comp(),
            "%s Attempting to hoist n%un [%p] out of the loop\n",
            OPT_DETAILS_LOOP_VERSIONER,
            invariantNode->getGlobalIndex(),
            invariantNode))
         {
         LoopEntryPrep *prep = createLoopEntryPrep(
            LoopEntryPrep::PRIVATIZE,
            invariantNode->duplicateTree());

         if (prep == NULL)
            {
            dumpOptDetails(
               comp(),
               "failed to privatize n%un [%p]\n",
               invariantNode->getGlobalIndex(),
               invariantNode);
            }
         else
            {
            _curLoop->_loopImprovements.push_back(
               new (_curLoop->_memRegion) Hoist(this, prep));
            }
         }

      nextInvariantNode = nextInvariantNode->getNextElement();
      }

   return true;
   }





bool TR_LoopVersioner::buildSpecializationTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees,
                                               List<TR::TreeTop> *checkCastTrees,
                                               List<TR::TreeTop> *arrayStoreCheckTrees,
                                               List<TR::Node> *comparisonTrees,
                                               List<TR::Node> *specializedNodes,
                                               TR::Block *loopInvariantBlock,
                                               TR::SymbolReference **symRefs)
   {
   if (!comp()->getRecompilationInfo())
     return false;

   bool specializedLong = false;

   ListElement<TR::Node> *nextSpecializedNode = specializedNodes->getListHead();
   while (nextSpecializedNode)
      {
      TR::Node *specializedNode = nextSpecializedNode->getData();
      TR::Node *dupSpecializedNode = NULL;

      bool isNextNodeInvariant = isExprInvariant(specializedNode);
      if (!isNextNodeInvariant)
         {
         if (specializedNode->getOpCode().hasSymbolReference() &&
             specializedNode->getSymbolReference()->getSymbol()->isAuto())
            {
            TR::Node *invariantSpecializedNode = isDependentOnInvariant(specializedNode);
            if (invariantSpecializedNode)
               dupSpecializedNode = invariantSpecializedNode->duplicateTreeForCodeMotion();
            else
               TR_ASSERT(0, "Should not be considering profiled expr for versioning\n");
            }
         else
            TR_ASSERT(0, "Should not be considering profiled expr for versioning\n");
         }

      TR::Node *nodeToBeChecked = specializedNode;
      if (dupSpecializedNode)
         nodeToBeChecked = dupSpecializedNode;

      collectAllExpressionsToBeChecked(nodeToBeChecked, comparisonTrees);

#ifdef J9_PROJECT_SPECIFIC
      if (performTransformation(comp(), "%s Creating test outside loop for checking if %p is value profiled\n", OPT_DETAILS_LOOP_VERSIONER, specializedNode))
         {
         TR::Node *duplicateNode = dupSpecializedNode;
         if (!duplicateNode)
            duplicateNode = specializedNode->duplicateTreeForCodeMotion();

         TR_ValueInfo *valueInfo = static_cast<TR_ValueInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(specializedNode, comp(), ValueInfo));
         int32_t value = valueInfo->getTopValue();

         TR::Node *comparisonNode = NULL;
         if (specializedNode->getType().isInt64())
            {
            TR::Node *highInt = TR::Node::create(TR::land, 2, duplicateNode, TR::Node::create(duplicateNode, TR::lconst, 0));
            highInt->getSecondChild()->setLongInt(((uint64_t)0xffffffff)<<32);
            comparisonNode = TR::Node::createif(TR::iflcmpne, highInt, TR::Node::create(duplicateNode, TR::lconst, 0, 0), _exitGotoTarget);
            }
         else
            {
            comparisonNode = TR::Node::createif(TR::ificmpne, duplicateNode, TR::Node::create(duplicateNode, TR::iconst, 0, value), _exitGotoTarget);
            }
         comparisonTrees->add(comparisonNode);
         dumpOptDetails(comp(), "The node %p has been created for testing if value profiling check is required\n", comparisonNode);

         int32_t childNum;
         for (childNum=0;childNum<specializedNode->getNumChildren(); childNum++)
            specializedNode->getChild(childNum)->recursivelyDecReferenceCount();

         if (specializedNode->getType().isInt64())
            {
            if (specializedNode->getOpCode().isLoadVar())
               {
               TR::SymbolReference *tempSymRef = symRefs[specializedNode->getSymbolReference()->getReferenceNumber()];
               if (!tempSymRef)
                  {
                  specializedLong = true;
                  tempSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
                  symRefs[specializedNode->getSymbolReference()->getReferenceNumber()] = tempSymRef;
                  TR::Node *newStore = TR::Node::createWithSymRef(TR::istore, 1, 1, TR::Node::create(TR::l2i, 1, specializedNode->duplicateTree()), tempSymRef);
                  TR::TreeTop *entryTree = loopInvariantBlock->getEntry();
                  loopInvariantBlock->prepend(TR::TreeTop::create(comp(), newStore));
                  }

               TR::Node::recreate(specializedNode, TR::iu2l);
               TR::Node *newLoad = TR::Node::createWithSymRef(specializedNode, TR::iload, 0, tempSymRef);
               specializedNode->setNumChildren(1);
               specializedNode->setAndIncChild(0, newLoad);

               //printf("Replacing long by int in %s\n", comp()->signature());
               }
            else
               TR_ASSERT(0, "Unexpected long opcode being profiled/specialized in loop\n");
            }
         else
            {
            TR::Node::recreate(specializedNode, TR::iconst);
            specializedNode->setNumChildren(0);
            specializedNode->setInt(value);
            }
         }
#endif
      nextSpecializedNode = nextSpecializedNode->getNextElement();
      }

   return specializedLong;
   }


static void cleanseIntegralNodeFlagsInSubtree(TR::Compilation *comp, TR::Node *node)
   {
   dumpOptDetails(comp, "%sCleansing Integer/Unsigned related flags under node n%in subtree\n",
         OPT_DETAILS_LOOP_VERSIONER, node->getGlobalIndex());

   TR::deque<TR::Node*, TR::Region&> queue(comp->trMemory()->currentStackRegion());
   queue.push_back(node);
   TR_BitVector seen(comp->trMemory()->currentStackRegion());
   while (!queue.empty())
      {
      TR::Node *tmp = queue.front();
      queue.pop_front();
      if (seen.get(tmp->getGlobalIndex()))
         continue;
      seen.set(tmp->getGlobalIndex());
      auto &op = tmp->getOpCode();
      if (op.isInteger() || op.isUnsigned())
         {
         tmp->setIsZero(false);
         tmp->setIsNonZero(false);
         tmp->setIsNonNegative(false);
         tmp->setIsNonPositive(false);
         }
      for (int i = 0; i < tmp->getNumChildren(); i++)
         {
         TR::Node *child = tmp->getChild(i);
         if (!seen.get(child->getGlobalIndex()))
            queue.push_back(child);
         }
      }
   }

void TR_LoopVersioner::buildConditionalTree(
   List<TR::TreeTop> *conditionalTrees,
   SharedSparseBitVector &reverseBranchInLoops)
   {
   ListElement<TR::TreeTop> *nextTree = conditionalTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *conditionalTree = nextTree->getData();
      TR::Node *conditionalNode = conditionalTree->getNode();
      TR::Node *origConditionalNode = conditionalNode;

      LoopEntryPrep *prep = NULL;
      bool chainPrep = false;

      if (conditionalNode->isTheVirtualGuardForAGuardedInlinedCall()
          && !conditionalNode->isOSRGuard()
          && !conditionalNode->isHCRGuard()
          && !conditionalNode->isDirectMethodGuard()
          && !conditionalNode->isBreakpointGuard())
         {

         bool searchReqd = true;
         TR::Node *nextRealNode = NULL;
         TR::Node *thisChild = NULL;
         TR::Node *nodeToBeChecked = NULL;

         TR_VirtualGuard *guardInfo = comp()->findVirtualGuardInfo(conditionalNode);
         if (guardInfo->getTestType() == TR_VftTest)
            {
            if (conditionalNode->getFirstChild()->getNumChildren() > 0)
               {
               thisChild = conditionalNode->getFirstChild()->getFirstChild();
               searchReqd = false;
               }
            else
               nodeToBeChecked = conditionalNode->getFirstChild();
            }
         else if (guardInfo->getTestType() == TR_MethodTest)
            {
            if ((conditionalNode->getFirstChild()->getNumChildren() > 0) &&
                (conditionalNode->getFirstChild()->getFirstChild()->getNumChildren() > 0))
               {
               thisChild = conditionalNode->getFirstChild()->getFirstChild()->getFirstChild();
               searchReqd = false;
               }
            else if (conditionalNode->getFirstChild()->getNumChildren() > 0)
               nodeToBeChecked = conditionalNode->getFirstChild()->getFirstChild();
            else
               nodeToBeChecked = conditionalNode->getFirstChild();
            }

         if (searchReqd)
            nextRealNode = findCallNodeInBlockForGuard(conditionalNode);

         // The receiver of an inlined call guarded by conditionalNode, if that
         // receiver does not occur within the guard tree, e.g. as part of VFT
         // test or method test.
         TR::Node *unmentionedReceiver = NULL;

         if (nextRealNode && !thisChild)
            {
            if (nextRealNode->getOpCode().isCallIndirect())
               thisChild = nextRealNode->getSecondChild();
            else
               thisChild = nextRealNode->getFirstChild();

            unmentionedReceiver = thisChild;
            }

         bool ignoreHeapificationStore = false;
         if (thisChild->getOpCode().hasSymbolReference() &&
             thisChild->getSymbolReference()->getSymbol()->isAutoOrParm())
            ignoreHeapificationStore = true;

         bool b = isExprInvariant(thisChild, ignoreHeapificationStore);
         if (!b && !nodeToBeChecked)
            {
            if (thisChild->getOpCode().hasSymbolReference() &&
                thisChild->getSymbolReference()->getSymbol()->isAuto())
                {
                TR::Node *dupThisChild = isDependentOnInvariant(thisChild);
                if (dupThisChild)
                   {
                   if (!guardInfo)
                      guardInfo = comp()->findVirtualGuardInfo(conditionalNode);


                   TR::Node *invariantThisChild = NULL;

                   if (guardInfo->getTestType() == TR_VftTest)
                      {
                      copyOnWriteNode(origConditionalNode, &conditionalNode);
                      invariantThisChild = conditionalNode->getFirstChild()->getFirstChild();
                      conditionalNode->getFirstChild()->setAndIncChild(0, dupThisChild->duplicateTree());
                      }
                   else if (guardInfo->getTestType() == TR_MethodTest)
                      {
                      copyOnWriteNode(origConditionalNode, &conditionalNode);
                      invariantThisChild = conditionalNode->getFirstChild()->getFirstChild()->getFirstChild();
                      conditionalNode->getFirstChild()->getFirstChild()->setAndIncChild(0, dupThisChild->duplicateTree());
                      }
                   else
                      {
                      unmentionedReceiver = dupThisChild;
                      }

                   if (invariantThisChild != NULL)
                      invariantThisChild->recursivelyDecReferenceCount();
                   }
                else
                   TR_ASSERT(0, "Cannot version with respect to non-invariant guard\n");
                }
             else
                TR_ASSERT(0, "Cannot version with respect to non-invariant guard\n");
             }

         if (unmentionedReceiver != NULL
             && requiresPrivatization(unmentionedReceiver))
            {
            chainPrep = true;
            prep =
               createLoopEntryPrep(LoopEntryPrep::PRIVATIZE, unmentionedReceiver);
            }
         }

      if (performTransformation(
            comp(),
            "%s Creating test outside loop for checking if n%un [%p] is conditional\n",
            OPT_DETAILS_LOOP_VERSIONER,
            conditionalNode->getGlobalIndex(),
            conditionalNode))
         {
         bool changeConditionalToUnconditionalInBothVersions = false;

         //
         // We pulled out from both on first conditional and all the subsequent conditionals we only pull from fast path
         // This exposed a bug where slow path had an unconditional that removed a valid path.
         // Fix is only allowing unconditioning in both if there is single if conditional tree.
         //
         if ((_conditionalTree == origConditionalNode) && (conditionalTrees->getSize() == 1))
            changeConditionalToUnconditionalInBothVersions = true;

         if (trace())
            traceMsg(comp(), "changeConditionalToUnconditionalInBothVersions %d\n", changeConditionalToUnconditionalInBothVersions);

         bool reverseBranch = false;

         // Special case for inlining tests that are invariant
         //
         //bool doNotReverseBranch = false;
         //if (conditionalNode->isTheVirtualGuardForAGuardedInlinedCall())
         //   {
         //    doNotReverseBranch = true;
         //    }

         TR::TreeTop *dest = conditionalNode->getBranchDestination();
         TR::Block *destBlock = dest ? dest->getNode()->getBlock() : NULL;
         TR::Block *fallThroughBlock = conditionalTree->getEnclosingBlock()->getNextBlock();

         if (trace()) traceMsg(comp(), "Frequency Test for conditional node [%p], destination Frequency %d, fallThrough frequency %d\n", conditionalNode,destBlock->getFrequency(), fallThroughBlock->getFrequency());
         if(reverseBranchInLoops[origConditionalNode->getGlobalIndex()])
            {
            if (trace()) traceMsg(comp(), "Branch reversed for conditional node [%p], destination Frequency %d, fallThrough frequency %d\n", conditionalNode,destBlock->getFrequency(), fallThroughBlock->getFrequency());
            reverseBranch = true;
            }

         TR::Node *duplicateComparisonNode = NULL, *duplicateIndexNode = NULL;
         int32_t indexChildIndex = -1;
         bool replaceIndexWithExpr=false;
         ExtremumConditional excond;
         if (isVersionableIfWithExtremum(conditionalTree, &excond))
            {
            indexChildIndex = excond.varyingChildIndex;
            replaceIndexWithExpr = excond.extremumIsFinalValue;

            // The final value is also needed in order to correctly version
            // unsigned comparisons.
            if (excond.extremumIsFinalValue
                || conditionalNode->getOpCode().isUnsigned())
               {
               chainPrep = true;

               // Ensure that the initial IV value would pass the loop test. If
               // not, then the loop is likely not to enter a second iteration,
               // and the number of iterations becomes difficult to predict.
               TR::Node *loopTest = _loopTestTree->getNode();
               TR::Node *loopLimit = loopTest->getChild(1)->duplicateTree();
               TR::Node *initialIv =
                  TR::Node::createLoad(conditionalNode, excond.iv);

               TR::ILOpCodes exitCmpOp =
                  excond.loopTest.keepLoopingCmpOp.getOpCodeForReverseBranch();

               TR::Node *preLoopTest = TR::Node::createif(
                  exitCmpOp, initialIv, loopLimit, _exitGotoTarget);

               prep = createLoopEntryPrep(LoopEntryPrep::TEST, preLoopTest);

               // Because isVersionableIfWithExtremum has checked that the loop
               // test comparison agrees in the expected way with the IV step
               // value, and because we have an earlier versioning test that
               // guarantees that the initial IV value would pass the loop
               // test, the true difference here has the same sign as the step
               // (or it's zero). However, it's possible for the subtraction to
               // overflow. Generate another versioning test to guard against
               // the possilibity of overflow.
               TR::Node *range = TR::Node::create(
                  conditionalNode, TR::isub, 2, loopLimit, initialIv);

               TR::ILOpCodes wrongSignCmpOp =
                  excond.step > 0 ? TR::ificmplt : TR::ificmpgt;

               TR::Node *zero = TR::Node::iconst(conditionalNode, 0);
               TR::Node *overflowTest =
                  TR::Node::createif(wrongSignCmpOp, range, zero, _exitGotoTarget);

               prep = createChainedLoopEntryPrep(
                  LoopEntryPrep::TEST, overflowTest, prep);

               // Now we can predict the final value of the IV. Really, this is
               // the most extreme possible final value, since in the presence
               // of other loop exits, we might exit in an earlier iteration.
               //
               // To predict the final value, consider an idealized IV which is
               // zero on entry to the loop, and increases by one just before
               // the loop test. (Because we have _loopTestTree, we know that
               // there is only one back-edge, and it originates from the loop
               // test.) So on entry to each iteration, its value is the number
               // of iterations that have already completed since the most
               // recent entry into the loop. Let J be this idealized IV, let i
               // be the real IV (excond.iv), and let A be the initial value of
               // i on loop entry. Then at the start of each iteration,
               //
               //    i = A + step * J.
               //
               // This equation also holds at the loop test, with the caveat
               // that a loop test comparing (old i + step) vs. L for some loop
               // limit L should be considered instead to compare new i vs. L.
               //
               // This equation holds over modular/machine integers, and futher,
               // since the IV update does not (signed) overflow, it also holds
               // over true integers if we consider the signed interpretations.
               //
               // Let J' be last value of J that passes the loop test, i.e. the
               // value of J at the start of the last iteration. Since the
               // direction of the loop test comparison (keepLoopingCmpOp)
               // agrees with the sign of step, we need to predict the final
               // value only in the following cases:
               //
               //    case: step > 0, loop test (i < L).
               //
               //       J' = max { J | A + step * J < L }
               //          = max { J | J < (L - A) / step }
               //          = ceil((L - A) / step) - 1
               //
               //    case: step > 0, loop test (i <= L).
               //
               //       J' = max { J | A + step * J <= L }
               //          = max { J | J <= (L - A) / step }
               //          = floor((L - A) / step)
               //
               //    case: step < 0, loop test (i > L).
               //
               //       J' = max { J | A + step * J > L }
               //          = max { J | J < (L - A) / step }
               //          = ceil((L - A) / step) - 1
               //
               //    case: step < 0, loop test (i >= L).
               //
               //       J' = max { J | A + step * J >= L }
               //          = max { J | J <= (L - A) / step }
               //          = floor((L - A) / step)
               //
               // The first versioning test above (preLoopTest) ensures that
               // the true difference L - A is zero or has the same sign as
               // step (again because the loop test comparison is in the
               // expected direction), and the second (overflowTest) ensures
               // that it's possible to compute L - A without overflow. So then
               // (L - A) / step >= 0, and an integer division that rounds
               // toward 0 will produce the floor, i.e.
               //
               //    idiv((L - A), step) = floor((L - A) / step),
               //
               // as required for non-strict loop test comparisons. Then for
               // the case of a strict comparison, note that
               //
               //      floor((L - A) / step) - icmpeq((L - A) % step, 0)
               //    = ceil((L - A) / step) - 1,
               //
               // which is easy to verify by considering the cases where step
               // does and does not divide evenly into L - A.
               //
               TR::Node *step = TR::Node::iconst(conditionalNode, excond.step);
               TR::Node *finalJ = // J'
                  TR::Node::create(conditionalNode, TR::idiv, 2, range, step);

               if (!excond.loopTest.keepLoopingCmpOp.isCompareTrueIfEqual()) // strict
                  {
                  TR::Node *rem =
                     TR::Node::create(conditionalNode, TR::irem, 2, range, step);
                  finalJ = TR::Node::create(
                     conditionalNode,
                     TR::isub,
                     2,
                     finalJ,
                     TR::Node::create(conditionalNode, TR::icmpeq, 2, rem, zero));
                  }

               if (!excond.loopTest.usesUpdatedIv)
                  {
                  // When the loop test uses the old value of the IV (from the
                  // beginning of the iteration), it delays failure of the loop
                  // test by one iteration, so add an extra 1 to J'.
                  finalJ = TR::Node::create(
                     conditionalNode,
                     TR::isub,
                     2,
                     finalJ,
                     TR::Node::iconst(conditionalNode, -1));
                  }

               // Find the value of i at conditionalNode in the last (possible)
               // iteration. Start with the value at the beginning of that
               // iteration, which is A + step * J'.
               TR::Node *finalIv = TR::Node::create(
                  conditionalNode,
                  TR::iadd,
                  2,
                  initialIv,
                  TR::Node::create(conditionalNode, TR::imul, 2, finalJ, step));

               if (ivLoadSeesUpdatedValue(excond.ivLoad, conditionalTree))
                  {
                  // When conditionalNode uses the updated value, the value at
                  // conditionalNode will be the iteration-initial value + step.
                  finalIv = TR::Node::create(
                     conditionalNode, TR::iadd, 2, finalIv, step);
                  }

               // Determine the overall expression to replace the varying child
               // of conditionalNode in the loop test. This is the same as the
               // varying child, but with finalIv substituted where originally
               // we had a load of the IV.
               TR_ASSERT_FATAL_WITH_NODE(
                  conditionalNode,
                  indexChildIndex >= 0,
                  "Index child index should be valid at this point");

               duplicateIndexNode = conditionalNode->getChild(indexChildIndex)->duplicateTree();

               int visitCount = comp()->incVisitCount();
               int32_t indexSymRefNum = excond.iv->getReferenceNumber();
               if(duplicateIndexNode->getOpCode().isLoad() &&
                  duplicateIndexNode->getSymbolReference()->getReferenceNumber()==indexSymRefNum)
                  {
                  duplicateIndexNode = finalIv;
                  }
               else
                  {
                  copyOnWriteNode(origConditionalNode, &conditionalNode);
                  replaceInductionVariable(conditionalNode, duplicateIndexNode, indexChildIndex, indexSymRefNum, finalIv, visitCount);
                  }
               }

            // Until now the extremum analysis has all been signed - see the
            // comment about overflow in isVersionableIfWithExtremum() - but
            // unsigned overflow can cause a problem for unsigned comparisons.
            //
            // Rather than try to rule out unsigned overflow directly, we can
            // reason about the range of possible signed values of the varying
            // expression.
            //
            // The initial and final values of the varying expression are its
            // signed min and signed max (not necessarily respectively). If
            // they have the same sign, then the varying expression would also
            // have that same sign on every iteration. In that case, the signs
            // of the comparison operands are both invariant. If the signs are
            // the same, then the comparison acts like a signed comparison, and
            // the usual versioning test works in the same way as in the signed
            // case. If OTOH the signs are different, then the comparison
            // result will be the same in every iteration, since negative
            // values always compare unsigned-greater-than nonnegative values,
            // so conditionalNode always agrees with the usual versioning test.
            //
            // So for unsigned comparisons, generate an extra versioning test
            // to make sure that the initial and final values both have the
            // same sign.
            //
            if (conditionalNode->getOpCode().isUnsigned())
               {
               dumpOptDetails(
                  comp(),
                  "Generating initial/final sign test for unsigned comparison\n");

               TR::Node *initial = conditionalNode->getChild(indexChildIndex);
               TR::Node *final = duplicateIndexNode;
               if (!replaceIndexWithExpr)
                  duplicateIndexNode = NULL;

               TR::Node *xorNode = TR::Node::create(
                  conditionalNode, TR::ixor, 2, initial->duplicateTree(), final);

               TR::Node *signTestNode = TR::Node::createif(
                  TR::ificmplt,
                  xorNode,
                  TR::Node::iconst(conditionalNode, 0),
                  _exitGotoTarget);

               prep = createChainedLoopEntryPrep(
                  LoopEntryPrep::TEST, signTestNode, prep);
               }
            }

         TR::Node *dupChildren[2] = {};
         for (int32_t i = 0; i < 2; i++)
            {
            if (replaceIndexWithExpr && i == indexChildIndex)
               dupChildren[i] = duplicateIndexNode;
            else
               dupChildren[i] = conditionalNode->getChild(i)->duplicateTree();
            }

         TR::ILOpCodes dupIfOp = conditionalNode->getOpCodeValue();
         if (reverseBranch)
            dupIfOp = TR::ILOpCode(dupIfOp).getOpCodeForReverseBranch();

         duplicateComparisonNode = TR::Node::createif(
            dupIfOp, dupChildren[0], dupChildren[1], _exitGotoTarget);

         if (duplicateComparisonNode->getFirstChild()->getOpCodeValue() == TR::instanceof)
            {
            duplicateComparisonNode->getFirstChild()->getFirstChild()->setIsNull(false);
            duplicateComparisonNode->getFirstChild()->getFirstChild()->setIsNonNull(false);
            }
         else
            {
            if (duplicateComparisonNode->getFirstChild()->getOpCodeValue() != TR::loadaddr)
               {
               duplicateComparisonNode->getFirstChild()->setIsNull(false);
               duplicateComparisonNode->getFirstChild()->setIsNonNull(false);
               }
            if (duplicateComparisonNode->getSecondChild()->getOpCodeValue() != TR::loadaddr)
               {
               duplicateComparisonNode->getSecondChild()->setIsNull(false);
               duplicateComparisonNode->getSecondChild()->setIsNonNull(false);
               }
            }

         duplicateComparisonNode->copyByteCodeInfo(conditionalNode);
         duplicateComparisonNode->setFlags(conditionalNode->getFlags());
         cleanseIntegralNodeFlagsInSubtree(comp(), duplicateComparisonNode);

         if (conditionalNode->isTheVirtualGuardForAGuardedInlinedCall())
            {
            conditionalNode->copyVirtualGuardInfoTo(duplicateComparisonNode, comp());

            // duplicateComparisonNode won't be put into the trees, so comp
            // shouldn't track its guard info. The guard info still needs to be
            // attached to the node though. If the prep is emitted, we'll make
            // a new copy of this tree (via the intermediate Expr) and insert
            // it ahead of the loop. That copy needs guard info, and so the
            // Expr does too, and createLoopEntryPrep()/makeCanonicalExpr()
            // will pick it up from duplicateComparisonNode.
            comp()->removeVirtualGuard(duplicateComparisonNode->virtualGuardInfo());
            }

         if (duplicateComparisonNode->isMaxLoopIterationGuard())
            {
            duplicateComparisonNode->setIsMaxLoopIterationGuard(false); //for the outer loop its not a maxloop itr guard anymore!
            }

         if (chainPrep)
            {
            prep = createChainedLoopEntryPrep(
               LoopEntryPrep::TEST, duplicateComparisonNode, prep);
            }
         else
            {
            TR_ASSERT_FATAL_WITH_NODE(
               conditionalNode, prep == NULL, "unexpected prereq LoopEntryPrep");
            prep = createLoopEntryPrep(LoopEntryPrep::TEST, duplicateComparisonNode);
            }

         if (prep != NULL)
            {
            nodeWillBeRemovedIfPossible(origConditionalNode, prep);
            if (reverseBranch)
               _curLoop->_takenBranches.add(origConditionalNode);

            _curLoop->_loopImprovements.push_back(
               new (_curLoop->_memRegion) FoldConditional(
                  this,
                  prep,
                  origConditionalNode,
                  reverseBranch,
                  /* original = */ true));

            // We don't (and generally can't) privatize in the slow loop, so
            // when the condition requires privatization, it isn't
            // necessarily invariant in the slow loop.
            if (changeConditionalToUnconditionalInBothVersions
                && !prep->_requiresPrivatization)
               {
               // This conditional node is outside the hot loop, so
               // nodeWillBeRemovedIfPossible() is unnecessary, and
               // _takenBranches is irrelevant.
               _curLoop->_loopImprovements.push_back(
                  new (_curLoop->_memRegion) FoldConditional(
                     this,
                     prep,
                     _duplicateConditionalTree,
                     reverseBranch,
                     /* original = */ false));

               _curLoop->_foldConditionalInDuplicatedLoop = true;
               }
            }
         }

      nextTree = nextTree->getNextElement();
      }
   }

void TR_LoopVersioner::FoldConditional::improveLoop()
   {
   dumpOptDetails(
      comp(),
      "Folding conditional n%un [%p]\n",
      _conditionalNode->getGlobalIndex(),
      _conditionalNode);

   if (_conditionalNode->isTheVirtualGuardForAGuardedInlinedCall())
      {
      TR::Node *callNode = _conditionalNode->getVirtualCallNodeForGuard();
      if (callNode)
         {
         callNode->resetIsTheVirtualCallNodeForAGuardedInlinedCall();
         if (_original)
            _versioner->_guardedCalls.add(callNode);
         }
      }

   TR::Node *constNode = TR::Node::create(_conditionalNode, TR::iconst, 0, 0);

   _conditionalNode->getFirstChild()->recursivelyDecReferenceCount();
   _conditionalNode->setChild(0, constNode);
   constNode->incReferenceCount();

   _conditionalNode->getSecondChild()->recursivelyDecReferenceCount();

   if (!_reverseBranch)
      constNode = TR::Node::create(_conditionalNode, TR::iconst, 0, 1);

   _conditionalNode->setChild(1, constNode);
   constNode->incReferenceCount();

   TR::Node::recreate(_conditionalNode, _original ? TR::ificmpeq : TR::ificmpne);
   _conditionalNode->setVirtualGuardInfo(NULL, comp());
   }

void TR_LoopVersioner::copyOnWriteNode(TR::Node *original, TR::Node **current)
   {
   if (*current != original)
      return; // already copied

   // Don't use duplicateTreeForCodeMotion(); *current is a stand-in for the
   // original node, except that it can be safely mutated, so it should have
   // all of the same flags, etc.
   *current = original->duplicateTree();

   // Later calls to dumpOptDetails() may refer to these nodes, so show this
   // output even without trace().
   if (comp()->getOutFile() != NULL && (trace() || comp()->getOption(TR_TraceOptDetails)))
      {
      comp()->getDebug()->clearNodeChecklist();
      dumpOptDetails(comp(), "Copy on write:\n\toriginal node:\n");
      comp()->getDebug()->printWithFixedPrefix(comp()->getOutFile(), original, 1, true, false, "\t\t");
      dumpOptDetails(comp(), "\n\tduplicate node:\n");
      comp()->getDebug()->printWithFixedPrefix(comp()->getOutFile(), *current, 1, true, false, "\t\t");
      dumpOptDetails(comp(), "\n");
      }
   }

bool TR_LoopVersioner::replaceInductionVariable(TR::Node *parent, TR::Node *node, int childNum, int loopDrivingInductionVar, TR::Node *loopLimit, int visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);
   if (node->getOpCode().hasSymbolReference())
      {
      if (node->getSymbolReference()->getReferenceNumber() == loopDrivingInductionVar)
         {
         parent->setAndIncChild(childNum, loopLimit);
         return true;
         }
      }
   int i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      if (replaceInductionVariable(node, node->getChild(i), i, loopDrivingInductionVar, loopLimit, visitCount))
         return true;
      }
   return false;
   }

/**
 * \brief Determine whether \p ivLoad sees the updated IV value.
 *
 * \param ivLoad         The load in question. It must load a versionable IV.
 * \param occurrenceTree A tree in which \p ivLoad occurs.
 * \return true if the IV is updated before \p ivLoad, and false otherwise.
 */
bool TR_LoopVersioner::ivLoadSeesUpdatedValue(
   TR::Node *ivLoad, TR::TreeTop *occurrenceTree)
   {
   TR_ASSERT_FATAL_WITH_NODE(
      ivLoad, ivLoad->getOpCode().isLoadVarDirect(), "expected a direct load");

   TR::SymbolReference *iv = ivLoad->getSymbolReference();
   TR_ASSERT_FATAL_WITH_NODE(
      ivLoad, iv->getSymbol()->isAutoOrParm(), "expected an auto");

   bool foundOccurrence = false;
   for (TR::PostorderNodeIterator it(occurrenceTree, comp());
        it.isAt(occurrenceTree);
        it.stepForward())
      {
      if (it.currentNode() == ivLoad)
         {
         foundOccurrence = true;
         break;
         }
      }

   TR_ASSERT_FATAL_WITH_NODE(
      ivLoad,
      foundOccurrence,
      "expected node to occur beneath n%un [%p]",
      occurrenceTree->getNode()->getGlobalIndex(),
      occurrenceTree->getNode());

   List<int32_t> *ivLists[] =
      {
      &_versionableInductionVariables,
      &_derivedVersionableInductionVariables
      };

   int32_t ivNum = iv->getReferenceNumber();
   bool isIV = false;
   for (size_t i = 0; i < sizeof(ivLists) / sizeof(ivLists[0]) && !isIV; i++)
      {
      List<int32_t> *ivList = ivLists[i];
      auto *elem = ivList->getListHead();
      for (; elem != NULL; elem = elem->getNextElement())
         {
         if (*elem->getData() == ivNum)
            {
            isIV = true;
            break;
            }
         }
      }

   TR_ASSERT_FATAL_WITH_NODE(ivLoad, isIV, "expected a primary or derived IV");

   TR::TreeTop *ivUpdateTree = _storeTrees[ivNum];
   TR::Block *ivUpdateBlock = ivUpdateTree->getEnclosingBlock();
   const TR::BlockChecklist *priorBlocks = NULL;
   bool updateAlwaysExecuted =
      blockIsAlwaysExecutedInLoop(ivUpdateBlock, _curLoop->_loop, &priorBlocks);

   TR_ASSERT_FATAL(
      updateAlwaysExecuted, "expected IV #%d to be updated every iteration", ivNum);

   TR::Block *loadBlock = occurrenceTree->getEnclosingBlock();
   if (priorBlocks->contains(loadBlock))
      return false; // the load is in a block that runs before the update
   else if (loadBlock != ivUpdateBlock)
      return true; // the load is in a block that runs after the update

   // The load is in the same block as the IV update. Determine which is
   // evaluated first.
   TR::Node *ivUpdate = ivUpdateTree->getNode();
   TR::TreeTop *entry = ivUpdateBlock->getEntry();
   TR::TreeTop *exit = ivUpdateBlock->getExit();
   for (TR::PostorderNodeIterator it(entry, comp()); !it.isAt(exit); it.stepForward())
      {
      TR::Node *node = it.currentNode();
      if (node == ivLoad)
         return false; // IV load is evaluated first, so it sees the old value
      else if (node == ivUpdate)
         return true; // IV update is evaluated first, so load sees the new value
      }

   TR_ASSERT_FATAL_WITH_NODE(ivLoad, false, "failed to distinguish old/new value");
   return false; // unreachable, but may look reachable to some build compiler(s)
   }

void TR_LoopVersioner::buildSpineCheckComparisonsTree(List<TR::TreeTop> *spineCheckTrees)
   {
   ListElement<TR::TreeTop> *nextTree = spineCheckTrees->getListHead();

   while (nextTree)
      {
      // NOTE: It could be a BNDCHKwithSpineCHK, since at this point no bound
      // checks have been removed yet. Only the array base is used, and it's at
      // the same position for both SpineCHK and BNDCHKwithSpineCHK.
      TR::Node *spineCheckNode = nextTree->getData()->getNode();
      TR::Node *arrayBase = spineCheckNode->getChild(1);
      vcount_t visitCount = comp()->incVisitCount();

      if (performTransformation(
            comp(),
            "%s Creating test outside loop for checking if n%un [%p] has spine\n",
            OPT_DETAILS_LOOP_VERSIONER,
            spineCheckNode->getGlobalIndex(),
            spineCheckNode))
         {
         TR::Node *contigArrayLength = TR::Node::create(TR::contigarraylength, 1, arrayBase->duplicateTreeForCodeMotion());
         TR::Node *nextComparisonNode = TR::Node::createif(TR::ificmpne, contigArrayLength, TR::Node::create(spineCheckNode, TR::iconst, 0, 0), _exitGotoTarget);

         // In case of BNDCHKwithSpineCHK, make this prep depend on the
         // one for removing the bound check. Otherwise we could fail to
         // remove the bound check, but "succeed" in removing the spine check,
         // and the spine check transformation would find an unexpected tree.
         LoopEntryPrep *prep = NULL;
         TR::ILOpCodes op = spineCheckNode->getOpCodeValue();
         if (op == TR::SpineCHK)
            {
            prep = createLoopEntryPrep(LoopEntryPrep::TEST, nextComparisonNode);
            }
         else
            {
            TR_ASSERT_FATAL(
               op == TR::BNDCHKwithSpineCHK,
               "expected either SpineCHK or BNDCHKwithSpineCHK, got %s",
               TR::ILOpCode(op).getName());

            auto prereqEntry =
               _curLoop->_boundCheckPrepsWithSpineChecks.find(spineCheckNode);

            TR_ASSERT_FATAL(
               prereqEntry != _curLoop->_boundCheckPrepsWithSpineChecks.end(),
               "missing prep for removal of bound check from BNDCHKwithSpineCHK n%un [%p]",
               spineCheckNode->getGlobalIndex(),
               spineCheckNode);

            LoopEntryPrep *prereq = prereqEntry->second;
            prep = createChainedLoopEntryPrep(
               LoopEntryPrep::TEST,
               nextComparisonNode,
               prereq);
            }

         if (prep != NULL)
            {
            nodeWillBeRemovedIfPossible(spineCheckNode, prep);
            _curLoop->_loopImprovements.push_back(
               new (_curLoop->_memRegion) RemoveSpineCheck(
                  this,
                  prep,
                  nextTree->getData()));
            }
         }

      nextTree = nextTree->getNextElement();
      }
   }

void TR_LoopVersioner::RemoveSpineCheck::improveLoop()
   {
   TR::Node *spineCheckNode = _spineCheckTree->getNode();
   dumpOptDetails(
      comp(),
      "Removing spine check n%un [%p]\n",
      spineCheckNode->getGlobalIndex(),
      spineCheckNode);

   TR_ASSERT_FATAL(spineCheckNode->getOpCodeValue() == TR::SpineCHK, "unexpected opcode");

   TR::Node *arrayBase = spineCheckNode->getChild(1);

   //fixup node to have arraylets.
   // aiadd
   //   iaload
   //       aiadd
   //          aload a
   //          iadd
   //             ishl
   //                 ishr
   //                    i
   //                    spineShift
   //                 shift
   //             hdrsize
   //    ishl
   //       iand
   //          iconst mask
   //          i
   //       iconst strideShift

   bool is64BitTarget = comp()->target().is64Bit() ? true : false;
   TR::DataType type =  spineCheckNode->getChild(0)->getDataType();
   uint32_t elementSize = TR::Symbol::convertTypeToSize(type);
   if (comp()->useCompressedPointers() && (type == TR::Address))
      elementSize = TR::Compiler->om.sizeofReferenceField();

   TR::Node* hdrSize = NULL;
   if (is64BitTarget)
      {
      hdrSize = TR::Node::create(spineCheckNode, TR::lconst);
      hdrSize->setLongInt((int64_t)TR::Compiler->om.discontiguousArrayHeaderSizeInBytes());
      }
   else
      hdrSize = TR::Node::create(spineCheckNode, TR::iconst, 0, (int32_t)TR::Compiler->om.discontiguousArrayHeaderSizeInBytes());


   int32_t shift = TR::TransformUtil::convertWidthToShift(TR::Compiler->om.sizeofReferenceField());
   TR::Node *shiftNode = is64BitTarget ? TR::Node::lconst(spineCheckNode, (int64_t)shift) :
                                        TR::Node::iconst(spineCheckNode, shift);

   int32_t strideShift = TR::TransformUtil::convertWidthToShift(elementSize);
   TR::Node *strideShiftNode = NULL;
   if (strideShift)
      strideShiftNode = is64BitTarget ? TR::Node::lconst(spineCheckNode, (int64_t)strideShift) :
                                        TR::Node::iconst(spineCheckNode, strideShift);

   int32_t arraySpineShift = fe()->getArraySpineShift(elementSize);
   TR::Node *spineShiftNode = is64BitTarget ? TR::Node::lconst(spineCheckNode, (int64_t)arraySpineShift) :
                                             TR::Node::iconst(spineCheckNode, arraySpineShift);

   TR::Node *spineIndex = spineCheckNode->getChild(2);
   if (is64BitTarget && spineIndex->getType().isInt32())
      spineIndex = TR::Node::create(TR::i2l, 1, spineIndex);

   TR::Node *node = NULL;

   node = TR::Node::create(is64BitTarget ? TR::lshr : TR::ishr, 2, spineIndex, spineShiftNode);
   node = TR::Node::create(is64BitTarget ? TR::lshl : TR::ishl, 2, node, shiftNode);
   node = TR::Node::create(is64BitTarget ? TR::ladd : TR::iadd, 2, node, hdrSize);
   node = TR::Node::create(is64BitTarget ? TR::aladd : TR::aiadd, 2, arrayBase, node);
   node = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node, comp()->getSymRefTab()->findOrCreateArrayletShadowSymbolRef(type));

   TR::Node *leafOffset = NULL;

   if (is64BitTarget)
      {
      leafOffset = TR::Node::create(spineCheckNode , TR::lconst);
      leafOffset->setLongInt((int64_t) fe()->getArrayletMask(elementSize));
      }
   else
      {
      leafOffset = TR::Node::create(spineCheckNode, TR::iconst, 0, (int32_t)fe()->getArrayletMask(elementSize));
      }

   TR::Node *nodeLeafOffset = TR::Node::create(is64BitTarget ? TR::land : TR::iand, 2, leafOffset, spineIndex);

   if (strideShiftNode)
      {
      nodeLeafOffset = TR::Node::create(is64BitTarget ? TR::lshl : TR::ishl, 2, nodeLeafOffset, strideShiftNode);
      }

   node = TR::Node::create(is64BitTarget ? TR::aladd : TR::aiadd, 2, node, nodeLeafOffset);
   TR::TreeTop *compressTree = NULL;
   if (comp()->useCompressedPointers())
      {
      compressTree = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(node->getFirstChild()),NULL,NULL);
      }

   TR::Node *arrayTT = spineCheckNode; //->getFirstChild();


   if (arrayTT->getFirstChild()->getOpCode().hasSymbolReference() &&
       arrayTT->getFirstChild()->getSymbol()->isArrayShadowSymbol())
      {
      arrayTT = arrayTT->getFirstChild();
      arrayTT->getChild(0)->recursivelyDecReferenceCount();
      arrayTT->setAndIncChild(0,node);
      }
   else
      arrayTT = node;

   if (!spineCheckNode->getFirstChild()->getOpCode().isStore())
      {
      arrayTT = TR::Node::create(TR::treetop, 1, arrayTT);
      spineCheckNode->getFirstChild()->recursivelyDecReferenceCount();
      }
   else
      arrayTT->setReferenceCount(0);


   TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), arrayTT, NULL, NULL);
   spineCheckNode->getChild(1)->recursivelyDecReferenceCount();
   spineCheckNode->getChild(2)->recursivelyDecReferenceCount();

   TR::TreeTop *prevTree = _spineCheckTree->getPrevTreeTop();
   TR::TreeTop *succTree = _spineCheckTree->getNextTreeTop();

   if (comp()->useCompressedPointers())
      {
      prevTree->join(compressTree);
      compressTree->join(firstNewTree);
      }
   else
      prevTree->join(firstNewTree);

   firstNewTree->join(succTree);
   }

void TR_LoopVersioner::buildBoundCheckComparisonsTree(
   List<TR::TreeTop> *boundCheckTrees,
   List<TR::TreeTop> *spineCheckTrees,
   bool reverseBranch)
   {
   ListElement<TR::TreeTop> *nextTree = boundCheckTrees->getListHead();
   bool isAddition;

   while (nextTree)
      {
      isAddition = false;
      TR::TreeTop *boundCheckTree = nextTree->getData();
      TR::Node *boundCheckNode = boundCheckTree->getNode();

      int32_t boundChildIndex = (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 2 : 0;
      TR::Node *arrayLengthNode = boundCheckNode->getChild(boundChildIndex);

      bool isMaxBoundInvariant = isExprInvariant(arrayLengthNode);
      if (!isMaxBoundInvariant)
         {
         if (arrayLengthNode->getOpCode().isArrayLength())
            {
            TR::Node *arrayObject = arrayLengthNode->getFirstChild();
            if (arrayObject->getOpCode().hasSymbolReference() &&
                arrayObject->getSymbolReference()->getSymbol()->isAuto())
               {
               TR::Node *invariantArrayObject = isDependentOnInvariant(arrayObject);
               if (invariantArrayObject)
                  {
                  TR::Node *dupInvariantArrayObject = invariantArrayObject->duplicateTree();
                  arrayLengthNode = TR::Node::create(
                     arrayLengthNode,
                     arrayLengthNode->getOpCodeValue(),
                     1,
                     dupInvariantArrayObject);
                  }
               else
                  TR_ASSERT(0, "Should not be considering bound check for versioning\n");
               }
            else
               TR_ASSERT(0, "Should not be considering bound check for versioning\n");
            }
         else
            {
            if (arrayLengthNode->getOpCode().hasSymbolReference() &&
                arrayLengthNode->getSymbolReference()->getSymbol()->isAuto())
               {
               TR::Node *invariantArrayLen = isDependentOnInvariant(arrayLengthNode);
               if (invariantArrayLen)
                  arrayLengthNode = invariantArrayLen->duplicateTree();
               else
                  TR_ASSERT(0, "Should not be considering bound check for versioning\n");
               }
            else
               TR_ASSERT(0, "Should not be considering bound check for versioning\n");
            }
         }

      int32_t indexChildIndex = (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 3 : 1;
      TR::Node *indexNode = boundCheckNode->getChild(indexChildIndex);

      TR::Node *nextComparisonNode;

      bool isIndexInvariant = isExprInvariant(indexNode);

      bool performBoundCheck = performTransformation(
         comp(),
         "%s Creating test outside loop for checking if n%un [%p] exceeds bounds\n",
         OPT_DETAILS_LOOP_VERSIONER,
         boundCheckNode->getGlobalIndex(),
         boundCheckNode);

      if (!performBoundCheck)
         {
         nextTree = nextTree->getNextElement();
         continue;
         }

      if (isIndexInvariant)
         {
         nextComparisonNode = TR::Node::createif(TR::ificmplt, indexNode->duplicateTreeForCodeMotion(), TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);

         if (trace())
            traceMsg(comp(), "Index invariant in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

         if (comp()->requiresSpineChecks())
             findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

         LoopEntryPrep *prep =
            createLoopEntryPrep(LoopEntryPrep::TEST, nextComparisonNode);

         if (boundCheckNode->getOpCodeValue() == TR::BNDCHK || boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK)
            nextComparisonNode = TR::Node::createif(TR::ificmpge, indexNode->duplicateTreeForCodeMotion(), arrayLengthNode->duplicateTreeForCodeMotion(), _exitGotoTarget);
         else
            nextComparisonNode = TR::Node::createif(TR::ificmpgt, indexNode->duplicateTreeForCodeMotion(), arrayLengthNode->duplicateTreeForCodeMotion(), _exitGotoTarget);

         if (trace())
            traceMsg(comp(), "Index invariant in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

         if (comp()->requiresSpineChecks())
            findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

         prep = createChainedLoopEntryPrep(
            LoopEntryPrep::TEST,
            nextComparisonNode,
            prep);

         if (prep != NULL)
            createRemoveBoundCheck(boundCheckTree, prep, spineCheckTrees);
         }
      else
         {
         bool isIndexChildMultiplied=false;
         bool indVarOccursAsSecondChildOfSub = false;

         int32_t indexChildIndex = (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 3 : 1;
         TR::Node *indexChild = boundCheckNode->getChild(indexChildIndex);
         TR::Node *mulNode=NULL;
         TR::Node *strideNode=NULL;

         TR::SymbolReference *indexSymRef = NULL;
         int32_t indexSymRefNum = -1;
         if (indexChild->getOpCode().hasSymbolReference())
            indexSymRef = indexChild->getSymbolReference();
         else
            {
            while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub() || indexChild->getOpCode().isAnd() ||
                   indexChild->getOpCode().isMul())
               {
               if(indexChild->getOpCode().isMul())
                  {
                  isIndexChildMultiplied=true;
                  mulNode=indexChild;
                  }
               if (indexChild->getSecondChild()->getOpCode().isLoadConst())
                  {
                  if(isIndexChildMultiplied)
                     strideNode=indexChild->getSecondChild();
                  indexChild = indexChild->getFirstChild();
                  }
               else
                  {
                  while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub() || indexChild->getOpCode().isAnd() ||
                         indexChild->getOpCode().isMul())
                     {
                     if(indexChild->getOpCode().isMul())
                        {
                        isIndexChildMultiplied=true;
                        mulNode=indexChild;
                        }
                     dumpOptDetails(comp(), "indexChild %p\n", indexChild);
                     if (indexChild->getSecondChild()->getOpCode().isLoadConst())
                        {
                        if(isIndexChildMultiplied)
                           strideNode=indexChild->getSecondChild();
                        indexChild = indexChild->getFirstChild();
                        }
                     else
                        {
                        bool isSecondChildInvariant = isExprInvariant(indexChild->getSecondChild());
                        if (isSecondChildInvariant)
                           {
                           if(isIndexChildMultiplied)
                              strideNode=indexChild->getSecondChild();
                           indexChild = indexChild->getFirstChild();
                           }
                        else
                           {
                           bool isFirstChildInvariant = isExprInvariant(indexChild->getFirstChild());
                           if (isFirstChildInvariant)
                              {
                              if (indexChild->getOpCode().isSub())
                                 {
                                 indVarOccursAsSecondChildOfSub = !indVarOccursAsSecondChildOfSub;
                                 }
                              if(isIndexChildMultiplied)
                                 strideNode=indexChild->getFirstChild();
                              indexChild = indexChild->getSecondChild();
                              }
                           else
                              break;
                           }
                        }
                     }
                  }
               }

            if (indexChild &&
                indexChild->getOpCode().hasSymbolReference())
                indexSymRef = indexChild->getSymbolReference();
            }

         TR::SymbolReference *origIndexSymRef = NULL;
         if (indexSymRef)
            origIndexSymRef = indexSymRef;

         bool isSpecialInductionVariable = false;
         bool isLoopDrivingInductionVariable = false;
         bool isDerivedInductionVariable = false;
         int32_t loopDrivingInductionVariable = -1;
         bool isLoopDrivingAddition = false;
         bool changedIndexSymRef = true;
         bool changedIndexSymRefAtSomePoint = false;
         int32_t changedIndexSymRefNumForStoreRhs = -1;

         int32_t exitValue = -1;
         while (indexSymRef &&
                changedIndexSymRef)
            {
            changedIndexSymRef = false;
            int32_t indexSymRefNum = indexSymRef->getReferenceNumber();
            ListElement<int32_t> *versionableInductionVar = _versionableInductionVariables.getListHead();
            bool foundInductionVariable = false;
            while (versionableInductionVar)
               {
               loopDrivingInductionVariable = *(versionableInductionVar->getData());
               if (_additionInfo->get(loopDrivingInductionVariable))
                  isLoopDrivingAddition = true;

               if (indexSymRefNum == *(versionableInductionVar->getData()))
                  {
                  if (_additionInfo->get(indexSymRefNum))
                     isAddition = true;
                  foundInductionVariable = true;
                  isLoopDrivingInductionVariable = true;
                  break;
                  }

               versionableInductionVar = versionableInductionVar->getNextElement();
               }


            if (!foundInductionVariable)
               {
               versionableInductionVar = _specialVersionableInductionVariables.getListHead();
               while (versionableInductionVar)
                  {
                  if (indexSymRefNum == *(versionableInductionVar->getData()))
                     {
                     isSpecialInductionVariable = true;
                     exitValue = _storeTrees[indexSymRefNum]->getNode()->getFirstChild()->getSecondChild()->getInt();
                     foundInductionVariable = true;
                     break;
                     }
                  versionableInductionVar = versionableInductionVar->getNextElement();
                  }
               }

            if (!foundInductionVariable)
               {
               versionableInductionVar = _derivedVersionableInductionVariables.getListHead();
               while (versionableInductionVar)
                  {
                  if (indexSymRefNum == *(versionableInductionVar->getData()))
                     {
                       //printf("Eliminating a BNDCHK on derived induction variable in %s\n", comp()->signature());
                     isDerivedInductionVariable = true;
                     if (_additionInfo->get(indexSymRefNum))
                        isAddition = true;
                     //exitValue = _storeTrees[indexSymRefNum]->getNode()->getFirstChild()->getSecondChild()->getInt();
                     foundInductionVariable = true;
                     break;
                     }
                  versionableInductionVar = versionableInductionVar->getNextElement();
                  }
               }

            if ((isDerivedInductionVariable && (loopDrivingInductionVariable == -1)) ||
                 !foundInductionVariable)
               {
               TR::SymbolReference *symRefInCompare = NULL;
               if (isDerivedInductionVariable && (loopDrivingInductionVariable == -1))
                  {
                  if (_loopTestTree &&
                      (_loopTestTree->getNode()->getNumChildren() > 1))
                     {
                     TR::Node *childInCompare = _loopTestTree->getNode()->getFirstChild();
                     while (childInCompare->getOpCode().isAdd() || childInCompare->getOpCode().isSub())
                        {
                        if (childInCompare->getSecondChild()->getOpCode().isLoadConst())
                           childInCompare = childInCompare->getFirstChild();
                        else
                           break;
                        }

                     if (childInCompare->getOpCode().hasSymbolReference())
                        symRefInCompare = childInCompare->getSymbolReference();
                     }


                  TR_InductionVariable *v = _currentNaturalLoop->findMatchingIV(symRefInCompare);
                  if (v)
                     {
                     if (_additionInfo->get(symRefInCompare->getReferenceNumber()))
                        {
                        isLoopDrivingAddition = true;
                        }
                     loopDrivingInductionVariable = symRefInCompare->getReferenceNumber();
                     //printf("Reached here in method %s\n", comp()->signature());
                     }
                  }
               else
                  {
                  TR_InductionVariable *v = _currentNaturalLoop->findMatchingIV(indexSymRef);
                  if (v)
                     {
                     if (_additionInfo->get(indexSymRefNum))
                        {
                        isAddition = true;
                        isLoopDrivingAddition = true;
                        //exitValue = exitVal->getHighInt();
                        }
                     //else
                     //   exitValue = exitVal->getLowInt();
                     loopDrivingInductionVariable = indexSymRefNum;
                     isLoopDrivingInductionVariable = true;
                     foundInductionVariable = true;
                     //printf("Reached here in method %s\n", comp()->signature());
                     }
                  }
               }

            if (!foundInductionVariable)
               {
               TR::TreeTop *defTree = NULL;
               indexChild = isDependentOnInductionVariable(
                  indexChild,
                  changedIndexSymRefAtSomePoint,
                  isIndexChildMultiplied,
                  mulNode,
                  strideNode,
                  indVarOccursAsSecondChildOfSub,
                  defTree);

               if (!indexChild ||
                   !indexChild->getOpCode().hasSymbolReference() ||
                   !indexChild->getSymbolReference()->getSymbol()->isAutoOrParm() ||
                   (indexChild->getSymbolReference()->getReferenceNumber() == indexSymRefNum))
                  {
                  break;
                  }
               else
                  {
                  indexSymRef = indexChild->getSymbolReference();
                  indexSymRefNum = indexSymRef->getReferenceNumber();
                  if (changedIndexSymRefAtSomePoint)
                     changedIndexSymRefNumForStoreRhs = indexSymRefNum;
                  //foundInductionVariable = true;
                  changedIndexSymRef = true;
                  changedIndexSymRefAtSomePoint = true;
                  }
               }
            }

         LoopEntryPrep *prep = NULL;

         bool complexButPredictableForm = true;
         if (!isAddition)
            complexButPredictableForm = false;

         int32_t additiveConstantInStore = 0;
         int32_t additiveConstantInIndex = 0;

         TR::Node *storeNode = _storeTrees[indexChild->getSymbolReference()->getReferenceNumber()]->getNode();
         if (storeNode->getDataType() != TR::Int32)
           complexButPredictableForm = false;

         TR::Node *valueChild = storeNode->getFirstChild();
         if (isInverseConversions(valueChild))
            valueChild = valueChild->getFirstChild()->getFirstChild();

         while (valueChild->getOpCode().isAdd() || valueChild->getOpCode().isSub())
            {
            if (valueChild->getSecondChild()->getOpCode().isLoadConst())
               {
               if (valueChild->getOpCode().isAdd())
                  additiveConstantInStore = additiveConstantInStore + valueChild->getSecondChild()->getInt();
               else
                  additiveConstantInStore = additiveConstantInStore - valueChild->getSecondChild()->getInt();
               valueChild = valueChild->getFirstChild();
               }
            else
               {
               complexButPredictableForm = false;
               break;
               //while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub() || indexChild->getOpCode().isAnd())
               //   {
               //   if (indexChild->getSecondChild()->getOpCode().isLoadConst())
               //      indexChild = indexChild->getFirstChild();
               //   else
               //      {
               //        visitCount = comp()->incVisitCount();
               //        bool isSecondChildInvariant = isExprInvariant(indexChild->getSecondChild(), visitCount);
               //        if (isSecondChildInvariant)
               //       indexChild = indexChild->getFirstChild();
               //        else
               //       {
               //           visitCount = comp()->incVisitCount();
               //           bool isFirstChildInvariant = isExprInvariant(indexChild->getFirstChild(), visitCount);
               //       if (isFirstChildInvariant)
               //              indexChild = indexChild->getSecondChild();
               //       else
               //      break;
               //           }
               //        }
               //   }
               }
            }

         if (!valueChild->getOpCode().hasSymbolReference() ||
             (valueChild->getSymbolReference()->getReferenceNumber() != indexChild->getSymbolReference()->getReferenceNumber()))
            complexButPredictableForm = false;

         if ((additiveConstantInStore == 1) || (additiveConstantInStore == -1))
            complexButPredictableForm = false;

         if (complexButPredictableForm)
            {
            TR::Node *cursorChild = boundCheckNode->getChild(indexChildIndex);
            while (cursorChild->getOpCode().isAdd() || cursorChild->getOpCode().isSub())
               {
               if (cursorChild->getSecondChild()->getOpCode().isLoadConst())
                  {
                  if (cursorChild->getOpCode().isAdd())
                     additiveConstantInIndex = additiveConstantInIndex + cursorChild->getSecondChild()->getInt();
                  else
                     additiveConstantInIndex = additiveConstantInIndex - cursorChild->getSecondChild()->getInt();
                  cursorChild = cursorChild->getFirstChild();
                  }
               else
                  {
                  complexButPredictableForm = false;
                  break;
                  }
               }

            if (cursorChild != indexChild)
               complexButPredictableForm = false;

            if (additiveConstantInIndex == 0)
               complexButPredictableForm = false;
            }

         if (isSpecialInductionVariable)
            {
            //printf("Found an opportunity for special versioning in method %s\n", comp()->signature());
            nextComparisonNode = TR::Node::createif(TR::ificmplt, boundCheckNode->getChild(indexChildIndex)->duplicateTreeForCodeMotion(), TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);
            if (trace())
               traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
            }
         else
            {
            TR::Node *duplicateIndex = boundCheckNode->getChild(indexChildIndex)->duplicateTreeForCodeMotion();

            // TODO: Always consult _unchangedValueUsedInBndCheck even when we
            // changedIndexSymRefAtSomePoint? It may be that the check here for
            // changedIndexSymRefAtSomePoint is trying to be conservative for
            // substitutions via isDependentOnInductionVariable() because they
            // used to interfere with the correct determination of the value in
            // _unchangedValueUsedInBndCheck.
            if ( changedIndexSymRefAtSomePoint || !_unchangedValueUsedInBndCheck->get(boundCheckNode->getGlobalIndex()))
               {
               if (_storeTrees[origIndexSymRef->getReferenceNumber()])
                  {
                  TR::Node *storeNode = _storeTrees[origIndexSymRef->getReferenceNumber()]->getNode();
                  TR::Node *storeRhs = storeNode->getFirstChild()->duplicateTree();
                  TR::Node *replacementLoad = TR::Node::createWithSymRef(storeNode, comp()->il.opCodeForDirectLoad(storeNode->getDataType()), 0, indexSymRef);

                  if (!storeRhs->getOpCode().isLoad())
                               {
                     int visitCount = comp()->incVisitCount();
                     replaceInductionVariable(NULL, storeRhs, -1, changedIndexSymRefNumForStoreRhs, replacementLoad, visitCount);
                                    }
                  else
                               storeRhs = replacementLoad;

                  //printf("Changing test in %s\n", comp()->signature());
                  if (!duplicateIndex->getOpCode().isLoad())
                     {
                     vcount_t visitCount = comp()->incVisitCount();
                     replaceInductionVariable(NULL, duplicateIndex, -1, origIndexSymRef->getReferenceNumber(), storeRhs, visitCount);
                     }
                  else
                     duplicateIndex = storeRhs; //storeRhs;
                  }
               }

            if ((isAddition && !indVarOccursAsSecondChildOfSub) ||
               (!isAddition && indVarOccursAsSecondChildOfSub))
               {
               nextComparisonNode = TR::Node::createif(TR::ificmplt, duplicateIndex, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);
               nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
               if (trace())
                  traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
               }
            else
               {
               nextComparisonNode = TR::Node::createif(TR::ificmpge, duplicateIndex, arrayLengthNode->duplicateTreeForCodeMotion(), _exitGotoTarget);
               nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
               if (trace())
                  traceMsg(comp(), "Induction variable subed in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
               }
            }

         if (comp()->requiresSpineChecks())
            findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

         prep = createLoopEntryPrep(LoopEntryPrep::TEST, nextComparisonNode);
         dumpOptDetails(
            comp(),
            "1: Prep %p has been created for testing if exceed bounds\n",
            prep);

         TR::Node *loopLimit = NULL;
         if (isLoopDrivingInductionVariable ||
             isDerivedInductionVariable)
            loopLimit = _loopTestTree->getNode()->getSecondChild()->duplicateTree();
         else
            loopLimit = TR::Node::create(boundCheckNode, TR::iconst, 0, exitValue);

         TR::Node *duplicateIndex = boundCheckNode->getChild(indexChildIndex)->duplicateTree();
         if (changedIndexSymRefAtSomePoint ||
             (!_unchangedValueUsedInBndCheck->get(boundCheckNode->getGlobalIndex()) &&
              !isSpecialInductionVariable))
            {
            if (_storeTrees[origIndexSymRef->getReferenceNumber()])
               {
               //printf("Changing test in %s\n", comp()->signature());
               TR::Node *storeNode = _storeTrees[origIndexSymRef->getReferenceNumber()]->getNode();
               TR::Node *storeRhs = storeNode->getFirstChild()->duplicateTree();
               TR::Node *replacementLoad = TR::Node::createWithSymRef(storeNode, comp()->il.opCodeForDirectLoad(storeNode->getDataType()), 0, indexSymRef);


               if (!storeRhs->getOpCode().isLoad())
                            {
                  int visitCount = comp()->incVisitCount();
                  replaceInductionVariable(NULL, storeRhs, -1, changedIndexSymRefNumForStoreRhs, replacementLoad, visitCount);
                            }
               else
                          storeRhs = replacementLoad;
               if (!duplicateIndex->getOpCode().isLoad())
                  {
                  int visitCount = comp()->incVisitCount();
                  replaceInductionVariable(NULL, duplicateIndex, -1, origIndexSymRef->getReferenceNumber(), storeRhs, visitCount);
                  }
               else
                  duplicateIndex = storeRhs;
               //dumpOptDetails(comp(), "dup index = %p\n", duplicateIndex);
               }
            }

         // change the loopLimit to reflect the
         // first illegal value of the iv.
         // handle both primary and derived ivs.
         //
         // to find the first illegal value, find
         // the last legal value of an iv:
         //
         // a) primary iv -
         // let primary iv be == i
         // for an increasing loop
         // maxValuei = [Ei + ceiling((N-Ei)/incr(i))*incr(i)] - incr(i)
         //
         // for a decreasing loop
         // maxValuei = [Ei - ceiling((Ei-N)/incr(i))*incr(i)] + incr(i)
         //
         // where  Ei = entry value of i
         //        N  = exit value in loop test
         //        incr(i) = increment of i
         //        ceiling((N-Ei)/incr(i)) = t (number of times
         //                                the loop is executed)
         // however for <= or >= loops
         // iif ((N-Ei)%incr(i) == 0)
         //     t = t + 1
         // in which case, the last legal value :
         // maxValuei = [Ei + (ceiling((N-Ei)/incr(i))+1)*incr(i)] - incr(i)
         //
         // so, for lt,gt loops, first illegal value is then:
         // loopLimit = maxValuei+1
         // loopLimit = maxValuei-1
         //
         // but for le,ge loops, the loopLimit will remain as
         // the last legal value:
         // loopLimit = maxValuei
         //
         // b) derived iv
         // let primary be == i and derived == j
         //
         // maxValuej = [Ej +/- ceiling((N-Ei)/incr(i))*incr(j)] -/+ incr(j) +/- 1
         //
         //
         //    isub
         //       iadd/isub
         //          iload Ei/Ej
         //          imul
         //             iadd
         //                iadd
         //                   idiv
         //                      isub           <-- SUB
         //                         iload N/Ei
         //                         iload Ei/N
         //                      iconst incr    <-- INCR
         //                   icmpne
         //                      irem
         //                         ==>SUB
         //                         ==>INCR
         //                      iconst 0
         //                iand
         //                   icmpeq
         //                      ==>IREM
         //                      ==>CONST0
         //                   iconst COND
         //            ==>INCR
         //       iconst adjustmentFactor
         //
         if (isLoopDrivingInductionVariable || isDerivedInductionVariable)
            {
            TR::SymbolReference *loopDrivingSymRef = indexSymRef;
            if (isDerivedInductionVariable)
               loopDrivingSymRef = comp()->getSymRefTab()->getSymRef(loopDrivingInductionVariable);

            TR::Node *entryNode = TR::Node::createLoad(boundCheckNode, loopDrivingSymRef);
            TR::Node *numIterations = NULL;
            TR::Node *range = NULL;
            if (isLoopDrivingAddition)
               range = TR::Node::create(TR::isub, 2, loopLimit, entryNode);
            else
               range = TR::Node::create(TR::isub, 2, entryNode, loopLimit);
            int32_t incrementJ = additiveConstantInStore;
            int32_t incrementI = 0;
            // find the increment of the primary iv
            //
            if (isDerivedInductionVariable)
               {
               TR::Node *indexChild = _storeTrees[loopDrivingInductionVariable]->getNode()->getFirstChild();
               if (isInverseConversions(indexChild))
                  indexChild = indexChild->getFirstChild()->getFirstChild();

               while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub())
                  {
                  if (indexChild->getSecondChild()->getOpCode().isLoadConst())
                     {
                     if (indexChild->getOpCode().isAdd())
                        incrementI = incrementI + indexChild->getSecondChild()->getInt();
                     else
                        incrementI = incrementI - indexChild->getSecondChild()->getInt();
                     indexChild = indexChild->getFirstChild();
                     }
                  else
                     break;
                  }
               }
            else
               incrementI = incrementJ;

            bool isIncreasingI = incrementI > 0; // check before taking abs just below

            if (incrementI < 0)
               incrementI = -incrementI;
            if (incrementJ < 0)
               incrementJ = -incrementJ;
            TR::Node *incrNode = TR::Node::create(boundCheckNode, TR::iconst, 0, incrementI);
            TR::Node *incrJNode = TR::Node::create(boundCheckNode, TR::iconst, 0, incrementJ);
            TR::Node *zeroNode = TR::Node::create(boundCheckNode, TR::iconst, 0, 0);
            numIterations = TR::Node::create(TR::idiv, 2, range, incrNode);

            TR::ILOpCode stayInLoopOp = _loopTestTree->getNode()->getOpCode();
            if (reverseBranch)
               stayInLoopOp = stayInLoopOp.getOpCodeForReverseBranch();

            if (isIncreasingI == stayInLoopOp.isCompareTrueIfGreater())
               {
               dumpOptDetails(
                  comp(),
                  "Abandon versioning bound check because while condition %s "
                  "is incompatible with %s loop driving IV\n",
                  stayInLoopOp.getName(),
                  isIncreasingI ? "an increasing" : "a decreasing");

               nextTree = nextTree->getNextElement();
               continue;
               }

            bool strict = !stayInLoopOp.isCompareTrueIfEqual();
            if (strict)
               {
               TR::Node *remNode = TR::Node::create(TR::irem, 2, range, incrNode);
               TR::Node *ceilingNode = TR::Node::create(TR::icmpne, 2, remNode, zeroNode);
               numIterations = TR::Node::create(TR::iadd, 2, numIterations, ceilingNode);
               }

            numIterations = TR::Node::create(TR::imul, 2, numIterations, incrJNode);

            TR::Node *maxValue = NULL;
            TR::ILOpCodes addOp = TR::isub;
            if (isLoopDrivingInductionVariable)
               {
               if (isLoopDrivingAddition)
                  addOp = TR::iadd;
               }
            else //isDerivedInductionVariable
               {
               if (isAddition)
                  addOp = TR::iadd;
               }

            if (isLoopDrivingInductionVariable)
                entryNode = TR::Node::createLoad(boundCheckNode, loopDrivingSymRef);
            else
                entryNode = TR::Node::createLoad(boundCheckNode, indexSymRef);

            maxValue = TR::Node::create(addOp, 2, entryNode, numIterations);

            if (strict)
               {
               TR::Node *adjustMaxValue = TR::Node::create(boundCheckNode, TR::iconst, 0, additiveConstantInStore);
               maxValue = TR::Node::create(TR::isub, 2, maxValue, adjustMaxValue);
               }

            if (trace())
               {
               traceMsg(comp(), "%s: reverseBranch %d stayInLoopOp %s incrNode n%dn numIterations n%dn maxValue n%dn loopLimit n%dn loopDrivingInductionVariable n%dn\n", __FUNCTION__,
                  reverseBranch, stayInLoopOp.getName(), incrNode->getGlobalIndex(), numIterations->getGlobalIndex(),
                  maxValue->getGlobalIndex(), loopLimit->getGlobalIndex(), _storeTrees[loopDrivingInductionVariable]->getNode()->getFirstChild()->getGlobalIndex());
               }
            /*
             * Loop test op code: <= or <
             *   - (limit + step) should be greater than or equal to the limit, otherwise outside of the representable range
             *
             * Loop test op code: >= or >
             *   - (limit - step) should be less than or equal to the limit, otherwise outside of the representable range
             */
            if (!_storeTrees[loopDrivingInductionVariable]->getNode()->getFirstChild()->cannotOverflow())
               {
               TR::Node *overflowComparisonNode = NULL;

               // stayInLoopOp already considers whether or not the branch is reversed
               if ((stayInLoopOp.getOpCodeValue() == TR::ificmple) || (stayInLoopOp.getOpCodeValue() == TR::ificmplt)) // <=, <
                  {
                  TR::Node *overLimit = TR::Node::create(TR::iadd, 2, loopLimit, incrNode);
                  overflowComparisonNode = TR::Node::createif(TR::ificmplt, overLimit, loopLimit, _exitGotoTarget);
                  }
               else // >=, >
                  {
                  TR::Node *overLimit = TR::Node::create(TR::isub, 2, loopLimit, incrNode);
                  overflowComparisonNode = TR::Node::createif(TR::ificmpgt, overLimit, loopLimit, _exitGotoTarget);
                  }

               if (comp()->requiresSpineChecks())
                  findAndReplaceContigArrayLen(NULL, overflowComparisonNode, comp()->incVisitCount());

               prep = createChainedLoopEntryPrep(
                  LoopEntryPrep::TEST,
                  overflowComparisonNode,
                  prep);

               dumpOptDetails(
                  comp(),
                  "Prep %p has been created for testing if exceed bounds\n",
                  prep);
               }

            loopLimit = maxValue;
            dumpOptDetails(comp(), "loopLimit has been adjusted to %p\n", loopLimit);
            }


         TR::Node *correctCheckNode = NULL;
         if (!duplicateIndex->getOpCode().isLoad())
            {
            int visitCount = comp()->incVisitCount();
            int32_t indexSymRefNum = indexSymRef->getReferenceNumber();
            /*
            if (isDerivedInductionVariable)
               indexSymRefNum = origIndexSymRef->getReferenceNumber();
            else
               indexSymRefNum = loopDrivingInductionVariable;
            */
            replaceInductionVariable(NULL, duplicateIndex, -1, indexSymRefNum, loopLimit, visitCount);
            correctCheckNode = duplicateIndex;
            }
         else
            correctCheckNode = loopLimit;

         nextComparisonNode = NULL;
         if (isSpecialInductionVariable)
            {
            nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, boundCheckNode->getChild(indexChildIndex)->duplicateTree(), correctCheckNode->duplicateTree(), _exitGotoTarget);
            if (trace())
               traceMsg(comp(), "Special Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

            if (comp()->requiresSpineChecks())
               findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

            prep = createChainedLoopEntryPrep(
               LoopEntryPrep::TEST,
               nextComparisonNode,
               prep);

            nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, correctCheckNode, arrayLengthNode->duplicateTree(), _exitGotoTarget);
            if (trace())
               traceMsg(comp(), "Special Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
            }
         else
            {
            //if ((isAddition && !indVarOccursAsSecondChildOfSub) ||
            //    (!isAddition && indVarOccursAsSecondChildOfSub))
            if (isAddition)
               {
               if (!indVarOccursAsSecondChildOfSub)
                  {
                  nextComparisonNode = TR::Node::createif(TR::ifiucmpge, correctCheckNode, arrayLengthNode->duplicateTree(), _exitGotoTarget);
                  nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                  }
               else
                  {
                  nextComparisonNode = TR::Node::createif(TR::ificmplt, correctCheckNode, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);
                  nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
                  }

               if (trace())
                  traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
               }
            else
               {
               if (!indVarOccursAsSecondChildOfSub)
                  {
                  nextComparisonNode = TR::Node::createif(TR::ificmplt, correctCheckNode, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);
                  nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
                  }
               else
                  {
                  nextComparisonNode = TR::Node::createif(TR::ifiucmpge, correctCheckNode, arrayLengthNode->duplicateTree(), _exitGotoTarget);
                  nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                  }

               if (trace())
                  traceMsg(comp(), "Induction variable subed in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
               }
            }

         if (comp()->requiresSpineChecks())
           findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

         prep = createChainedLoopEntryPrep(
            LoopEntryPrep::TEST,
            nextComparisonNode,
            prep);

         dumpOptDetails(
            comp(),
            "2: Prep %p has been created for testing if exceed bounds\n",
            prep);

         if (!isSpecialInductionVariable)
            {
            TR::Node *firstChild = boundCheckNode->getChild(indexChildIndex)->duplicateTree();
            if ( changedIndexSymRefAtSomePoint || !_unchangedValueUsedInBndCheck->get(boundCheckNode->getGlobalIndex()))
               {
               if (_storeTrees[origIndexSymRef->getReferenceNumber()])
                  {
                  TR::Node *storeNode = _storeTrees[origIndexSymRef->getReferenceNumber()]->getNode();
                  TR::Node *storeRhs = storeNode->getFirstChild()->duplicateTree();
                  TR::Node *replacementLoad = TR::Node::createWithSymRef(storeNode, comp()->il.opCodeForDirectLoad(storeNode->getDataType()), 0, indexSymRef);
                  //printf("Changing test in %s\n", comp()->signature());
                  if (!storeRhs->getOpCode().isLoad())
                     {
                     int visitCount = comp()->incVisitCount();
                     replaceInductionVariable(NULL, storeRhs, -1, origIndexSymRef->getReferenceNumber(), replacementLoad, visitCount);
                     }
                  else
                     storeRhs = replacementLoad;

                  if (!firstChild->getOpCode().isLoad())
                     {
                     vcount_t visitCount = comp()->incVisitCount();
                     replaceInductionVariable(NULL, firstChild, -1, origIndexSymRef->getReferenceNumber(), storeRhs, visitCount);
                     }
                  else
                     firstChild = storeRhs;
                  }
               }
            if(!isIndexChildMultiplied)
               {
               TR::Node *secondChild = correctCheckNode->duplicateTree();
               if (indVarOccursAsSecondChildOfSub)
                  {
                  TR::Node *temp = firstChild;
                  firstChild = secondChild;
                  secondChild = temp;
                  }

               if (isAddition)
                  {
                  if ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmple) ||
                      (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpgt))
                     nextComparisonNode = TR::Node::createif(TR::ificmpgt, firstChild, secondChild, _exitGotoTarget);
                  else
                     nextComparisonNode = TR::Node::createif(TR::ificmpge, firstChild, secondChild, _exitGotoTarget);

                  nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                  }
               else
                  {
                  if ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpge) ||
                      (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmplt))
                     nextComparisonNode = TR::Node::createif(TR::ificmplt, firstChild, secondChild, _exitGotoTarget);
                  else
                     nextComparisonNode = TR::Node::createif(TR::ificmple, firstChild, secondChild, _exitGotoTarget);

                  nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
                  }

               if (trace())
                  traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

               if (comp()->requiresSpineChecks())
                  findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

               prep = createChainedLoopEntryPrep(
                  LoopEntryPrep::TEST,
                  nextComparisonNode,
                  prep);

               dumpOptDetails(
                  comp(),
                  "3: Prep %p has been created for testing if exceed bounds\n",
                  prep);
               }
            else
               {
               //creating an OverFlow check
               TR::Node *duplicateMulNode = mulNode->duplicateTree();
               TR::Node *duplicateMulHNode = mulNode->duplicateTree();
               if((isAddition && !indVarOccursAsSecondChildOfSub) ||
                 (!isAddition && indVarOccursAsSecondChildOfSub))
                  {
                  traceMsg(comp(), " Its addition indexsymref %d, duplicateMulNode %p \n",indexSymRef->getReferenceNumber(),duplicateMulNode);
                  vcount_t visitCount = comp()->incVisitCount();
                  replaceInductionVariable(NULL, duplicateMulNode, -1, indexSymRef->getReferenceNumber(), loopLimit->duplicateTree(), visitCount);
                  visitCount = comp()->incVisitCount();
                  replaceInductionVariable(NULL, duplicateMulHNode, -1, indexSymRef->getReferenceNumber(), loopLimit->duplicateTree(), visitCount);
                  }

               traceMsg(comp(), " node : %p Loop limit %p )\n",nextComparisonNode,loopLimit);
               //If its negative
               nextComparisonNode = TR::Node::createif(TR::ificmplt, duplicateMulNode, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);
               nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
               if (comp()->requiresSpineChecks())
                  findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

               prep = createChainedLoopEntryPrep(
                  LoopEntryPrep::TEST,
                  nextComparisonNode,
                  prep);

               if (trace())
                  traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

               TR::Node::recreate(duplicateMulHNode,TR::imulh);
               nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, duplicateMulHNode, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);
               nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
               if (comp()->requiresSpineChecks())
                  findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

               prep = createChainedLoopEntryPrep(
                  LoopEntryPrep::TEST,
                  nextComparisonNode,
                  prep);

               if (trace())
                  traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

               //Adding multiplicative factor greater than zero check for multiplicative BNDCHKS a.i+b; a>0
               nextComparisonNode = TR::Node::createif(TR::ificmple, strideNode->duplicateTree(), TR::Node::create(boundCheckNode, TR::iconst, 0, 0), _exitGotoTarget);
               if (comp()->requiresSpineChecks())
                  findAndReplaceContigArrayLen(NULL, nextComparisonNode, comp()->incVisitCount());

               prep = createChainedLoopEntryPrep(
                  LoopEntryPrep::TEST,
                  nextComparisonNode,
                  prep);
               }
            }

         if (prep != NULL)
            createRemoveBoundCheck(boundCheckTree, prep, spineCheckTrees);
         }
      nextTree = nextTree->getNextElement();
      }
   }

void TR_LoopVersioner::createRemoveBoundCheck(
   TR::TreeTop *boundCheckTree,
   LoopEntryPrep *prep,
   List<TR::TreeTop> *spineCheckTrees)
   {
   _curLoop->_loopImprovements.push_back(
      new (_curLoop->_memRegion) RemoveBoundCheck(
         this,
         prep,
         boundCheckTree));

   TR::Node *boundCheckNode = boundCheckTree->getNode();
   TR::ILOpCodes op = boundCheckNode->getOpCodeValue();
   if (op == TR::BNDCHK || op == TR::ArrayCopyBNDCHK)
      {
      // Bound check only, which if prep is allowed will be completely removed.
      //
      // Note that the condition for BNDCHK is stronger than the one for
      // ArrayCopyBNDCHK. It's safe to remove an ArrayCopyBNDCHK based on
      // versioning tests that would guarantee BNDCHK, though such tests may be
      // more conservative than necessary.
      nodeWillBeRemovedIfPossible(boundCheckNode, prep);
      }
   else
      {
      TR_ASSERT_FATAL(
         op == TR::BNDCHKwithSpineCHK,
         "expected BNDCHK, ArrayCopyBNDCHK, or BNDCHKwithSpineCHK, but got %s",
         TR::ILOpCode(op).getName());

      // After eliminating the bound check part of BNDCHKwithSpineCHK, it will
      // still be SpineCHK, so nodeWillBeRemovedIfPossible() is inappropriate
      // here. By adding the tree to spineCheckTrees, an attempt will be made
      // to remove the spine check as well, and spine check removal can call
      // nodeWillBeRemovedIfPossible().
      spineCheckTrees->add(boundCheckTree);

      // The spine check should only be removed if the bound check is
      // successfully removed first. Remember this prep so that the prep for
      // spine check removal can depend on it.
      auto insertResult =
         _curLoop->_boundCheckPrepsWithSpineChecks.insert(
            std::make_pair(boundCheckNode, prep));

      bool insertSucceeded = insertResult.second;
      NodePrepMap::iterator entryPreventingInsertion = insertResult.first;
      TR_ASSERT_FATAL(
         insertSucceeded,
         "multiple preps %p and %p for removing bound check n%un [%p]",
         entryPreventingInsertion->second,
         prep,
         boundCheckNode->getGlobalIndex(),
         boundCheckNode);
      }
   }

void TR_LoopVersioner::RemoveBoundCheck::improveLoop()
   {
   TR::Node *boundCheckNode = _boundCheckTree->getNode();
   dumpOptDetails(
      comp(),
      "Removing bound check n%un [%p]\n",
      boundCheckNode->getGlobalIndex(),
      boundCheckNode);

   TR_ASSERT_FATAL(boundCheckNode->getOpCode().isBndCheck(), "unexpected opcode");

   if (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK)
      {
      TR::Node::recreate(boundCheckNode, TR::SpineCHK);
      TR::Node *contigLen = boundCheckNode->getChild(2);
      TR::Node *anchor = TR::Node::create(contigLen, TR::treetop, 1, contigLen);
      _boundCheckTree->insertBefore(TR::TreeTop::create(comp(), anchor));
      contigLen->recursivelyDecReferenceCount();
      boundCheckNode->setAndIncChild(2, boundCheckNode->getChild(3));
      boundCheckNode->getChild(3)->recursivelyDecReferenceCount();
      boundCheckNode->setNumChildren(3);
      }
   else
      {
      TR::TreeTop *prevTree = _boundCheckTree->getPrevTreeTop();
      TR::TreeTop *succTree = _boundCheckTree->getNextTreeTop();
      TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, boundCheckNode->getChild(0)), NULL, NULL);
      TR::TreeTop *secondNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, boundCheckNode->getChild(1)), NULL, NULL);
      prevTree->join(firstNewTree);
      firstNewTree->join(secondNewTree);
      secondNewTree->join(succTree);
      boundCheckNode->recursivelyDecReferenceCount();
      }
   }

/**
 * \brief Emit versioning tests to check that \p node is safe to evaluate.
 *
 * \deprecated Many of the parameters are unused. Prefer
 * collectAllExpressionsToBeChecked(TR::Node*, List<TR::Node>*).
 * This overload remains for compatibility with downstream projects.
 *
 * \param nullCheckTrees Unused
 * \param divCheckTrees Unused
 * \param checkCastTrees Unused
 * \param arrayStoreCheckTrees Unused
 * \param node The node that should be made safe to evaluate
 * \param comparisonTrees The list of all versioning tests for the loop
 * \param exitGotoBlock Unused in favor of _exitGotoTarget
 * \param visitCount Unused
 */
void TR_LoopVersioner::collectAllExpressionsToBeChecked(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, TR::Node *node, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock, vcount_t visitCount)
   {
   collectAllExpressionsToBeChecked(node, comparisonTrees);
   }

/**
 * \brief Emit versioning tests to check that \p node is safe to evaluate.
 *
 * This is used to allow generating a copy of \p node as part of a versioning
 * test, which otherwise would be evaluated in a context where the safety
 * conditions (e.g. that the child of an indirect load is non-null) are not
 * known to hold.
 *
 * \deprecated This method generates versioning tests immediately instead of
 * deferring them. The only reason to do so is because the caller is also
 * generating versioning tests immediately. Such versioning tests cannot rely
 * on the ability to do privatization.
 *
 * \param node The node that should be made safe to evaluate
 * \param comparisonTrees The list of all versioning tests for the loop
 *
 * \see unsafelyEmitAllTests()
 */
void TR_LoopVersioner::collectAllExpressionsToBeChecked(TR::Node *node, List<TR::Node> *comparisonTrees)
   {
   // At this point versioner itself should never call this method.
   TR_ASSERT_FATAL(
      shouldOnlySpecializeLoops() || refineAliases(),
      "versioner itself called collectAllExpressionsToBeChecked() for loop %d",
      _curLoop->_loop->getNumber());

   // Some callers pass an original tree instead of a duplicate. Including
   // original nodes in the safety tests generated by depsForLoopEntryPrep()
   // incorrectly increases the refcount of those original nodes. To prevent
   // this, copy defensively here.
   node = node->duplicateTreeForCodeMotion();

   // Because node will no longer appear verbatim in the trees, print it to
   // the log so that other dumpOptDetails() messages that refer to its
   // descendants make sense.
   bool optDetails =
      comp()->getOutFile() != NULL
      && (trace() || comp()->getOption(TR_TraceOptDetails));

   if (optDetails)
      {
      dumpOptDetails(comp(), "collectAllExpressionsToBeChecked on tree:\n");
      comp()->getDebug()->clearNodeChecklist();
      comp()->getDebug()->printWithFixedPrefix(
         comp()->getOutFile(),
         node,
         1,
         true,
         false,
         "\t\t");
      traceMsg(comp(), "\n");
      }

   TR::NodeChecklist visited(comp());
   TR::list<LoopEntryPrep*, TR::Region&> deps(_curLoop->_memRegion);
   if (!depsForLoopEntryPrep(node, &deps, &visited, true))
      {
      comp()->failCompilation<TR::CompilationException>(
         "failed to generate safety tests");
      }

   unsafelyEmitAllTests(deps, comparisonTrees);
   }

/**
 * \brief Determine whether \p node has a stable value when re-evaluated, or
 * whether privatization is needed to force the value to remain stable.
 *
 * Only the evaluation of \p node itself is considered here. Its children (and
 * generally descendants) are assumed to have stable values, possibly because
 * they themselves will be privatized.
 *
 * **For debugging purposes only**, the \c TR_nothingRequiresPrivatizationInVersioner
 * environment variable can be set to cause this method to assume that
 * privatization is unnecessary.
 *
 * \param node The node in question
 * \return true if privatization is required
 *
 * \see suppressInvarianceAndPrivatization()
 */
bool TR_LoopVersioner::requiresPrivatization(TR::Node *node)
   {
   static const bool nothingRequiresPrivatization =
      feGetEnv("TR_nothingRequiresPrivatizationInVersioner") != NULL;

   if (nothingRequiresPrivatization)
      {
      // NB. It's tempting to think that turning off privatization for
      // everything this way would work for single-threaded programs, but that
      // isn't the case. The loop may still contain cold calls that could
      // overwrite values observed in the versioning tests. See the logic for
      // TR_assumeSingleThreadedVersioning in emitPrep().
      return false;
      }

   if (!node->getOpCode().hasSymbolReference())
      return false;

   if (node->isDataAddrPointer())
      return false;

   if (node->getOpCodeValue() == TR::loadaddr || node->getOpCode().isTreeTop())
      return false;

   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol *sym = symRef->getSymbol();
   if (sym->isAutoOrParm())
      return false;

   TR::SymbolReferenceTable *srTab = comp()->getSymRefTab();
   TR::ResolvedMethodSymbol *curMethod = comp()->getMethodSymbol();
   if (symRef == srTab->findOrCreateInstanceOfSymbolRef(curMethod))
      return false;

   switch (symRef->getReferenceNumber() - srTab->getNumHelperSymbols())
      {
      case TR::SymbolReferenceTable::vftSymbol:
      case TR::SymbolReferenceTable::javaLangClassFromClassSymbol:
      case TR::SymbolReferenceTable::classFromJavaLangClassSymbol:
         return false;

      default:
         break;
      }

   if (srTab->isVtableEntrySymbolRef(symRef))
      return false;

   if (suppressInvarianceAndPrivatization(symRef))
      return false;

   return true;
   }

/**
 * \brief Determine whether to prevent accesses to \p symRef from being
 * considered invariant and getting privatized, even if it doesn't appear to be
 * modified in the loop.
 *
 * This is useful for values that are not explicitly written in IL but may be
 * set by the runtime system, e.g. the current exception, and for values for
 * which stores from other threads should be observed (without imposing any
 * other memory ordering effect).
 *
 * \param symRef The symbol reference for the operation in question
 *
 * \return true if node should be considered non-invariant and shouldn't be
 * privatized
 *
 * \see isExprInvariant()
 * \see requiresPrivatization()
 */
bool TR_LoopVersioner::suppressInvarianceAndPrivatization(TR::SymbolReference *symRef)
   {
   // Be sensitive to other threads' updates to the method overridden bit.
   if (symRef->isOverriddenBitAddress())
      return true;

   // Some nop guards contain code that tests the value of a fake static from
   // which we can't actually load at runtime. We should avoid privatizing any
   // such static for two reasons:
   //
   // 1. Semantically, loads should be sensitive to updates from other threads.
   // 2. If a load is separated from a guard, it may be evaluated.
   //
   // For a static at address 0, it's safe to return true because if it's not a
   // fake static for a nop guard, it will simply be considered non-invariant
   // and so it won't be hoisted.
   //
   // This catches TR_DirectMethodGuard.
   //
   TR::Symbol *sym = symRef->getSymbol();
   if (sym->isStatic() && sym->getStaticSymbol()->getStaticAddress() == 0)
      return true;

   TR::SymbolReferenceTable *srTab = comp()->getSymRefTab();
   switch (symRef->getReferenceNumber() - srTab->getNumHelperSymbols())
      {
      // The current exception is updated by the VM on throw.
      case TR::SymbolReferenceTable::excpSymbol:
         return true;

      // Class flags are mutable.
      case TR::SymbolReferenceTable::isClassDepthAndFlagsSymbol:
         return true;

#ifdef J9_PROJECT_SPECIFIC
      // These may be updated by the garbage collector at GC points.
      case TR::SymbolReferenceTable::lowTenureAddressSymbol:
      case TR::SymbolReferenceTable::highTenureAddressSymbol:
         return true;

      // The VM updates this with a tag when there is a breakpoint.
      case TR::SymbolReferenceTable::j9methodConstantPoolSymbol:
         return true;

      // TODO: Recognize the statics loaded for method enter and exit hook
      // tests. The only consequence of failing to recognize them here is that
      // if hooks are disabled before entering the loop, but become enabled
      // during loop execution, they will be skipped within the loop until the
      // loop exits. This is a preexisting problem, as these statics were
      // already considered to be invariant and the hook tests could already be
      // versioned.
#endif

      default:
         break;
      }

   return false;
   }

void TR_LoopVersioner::dumpOptDetailsCreatingTest(
   const char *description,
   TR::Node *node)
   {
   dumpOptDetails(
      comp(),
      "Creating %s test for n%un [%p]\n",
      description,
      node->getGlobalIndex(),
      node);
   }

void TR_LoopVersioner::dumpOptDetailsFailedToCreateTest(
   const char *description,
   TR::Node *node)
   {
   dumpOptDetails(
      comp(),
      "Failed to create %s test for n%un [%p]\n",
      description,
      node->getGlobalIndex(),
      node);
   }

/**
 * \brief Create and add to \p deps dependencies for a LoopEntryPrep whose
 * expression is based on \p node.
 *
 * Identify descendants of node that need to be privatized to guarantee a
 * stable value, or that need safety conditions to be checked before
 * evaluation (e.g. that the child of an indirect load is non-null), and create
 * the appropriate LoopEntryPrep instances. An effort is made not to copy
 * transitive dependencies directly into \p deps.
 *
 * \param node A subtree belonging to the LoopEntryPrep that owns \p deps
 * \param deps The list to which to add dependencies
 * \param visited The visited set corresponding to \p deps
 * \param canPrivatizeRootNode True if \p node can be privatized. Should be
 * false only when computing dependencies for a privatization of \p node, to
 * avoid circularity.
 *
 * \return true on success, false on failure
 */
bool TR_LoopVersioner::depsForLoopEntryPrep(
   TR::Node *node,
   TR::list<LoopEntryPrep*, TR::Region&> *deps,
   TR::NodeChecklist *visited,
   bool canPrivatizeRootNode)
   {
   if (visited->contains(node))
      return true;

   visited->add(node);

   if (canPrivatizeRootNode && requiresPrivatization(node))
      return addLoopEntryPrepDep(LoopEntryPrep::PRIVATIZE, node, deps, visited) != NULL;

   // If this is an indirect access
   //
   if (node->isInternalPointer() ||
         (((node->getOpCode().isIndirect() && node->getOpCode().hasSymbolReference() &&
         !node->getSymbolReference()->getSymbol()->isStatic()) ||
         node->getOpCode().isArrayLength()) && !node->getFirstChild()->isInternalPointer()))
      {
      TR::Node *firstChild = node->getFirstChild();
      if (!firstChild->isThisPointer())
         {
         // if we are dealing with dataAddr load use the child for null check.
         // We can reach here either with dataAddr load or with parent of dataAddr load.
         if (firstChild->isDataAddrPointer())
            firstChild = firstChild->getFirstChild(); // Array base is the child of dataAddr load

         dumpOptDetailsCreatingTest("null", firstChild);
         TR::Node *ifacmpeqNode = TR::Node::createif(TR::ifacmpeq, firstChild, TR::Node::aconst(node, 0), _exitGotoTarget);
         LoopEntryPrep *nullTestPrep =
            addLoopEntryPrepDep(LoopEntryPrep::TEST, ifacmpeqNode, deps, visited);

         if (nullTestPrep == NULL)
            {
            dumpOptDetailsFailedToCreateTest("null", firstChild);
            return false;
            }

         const Expr *refExpr = nullTestPrep->_expr->_children[0];
         _curLoop->_nullTestPreps.insert(std::make_pair(refExpr, nullTestPrep));
         }

      bool instanceOfReqd = true;
      TR_OpaqueClassBlock *otherClassObject = NULL;
      TR::Node *duplicateClassPtr = NULL;
      bool testIsArray = false;
      if (node->isInternalPointer() &&
          (firstChild->isInternalPointer() ||
           (firstChild->getOpCode().hasSymbolReference() &&
            firstChild->getSymbolReference()->getSymbol()->isAuto() &&
            firstChild->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer())))
         instanceOfReqd = false;
      else if (firstChild->getOpCode().hasSymbolReference())
         {
         TR::SymbolReference *symRef = firstChild->getSymbolReference();
         int32_t len;
         const char *sig = symRef->getTypeSignature(len);

         if (node->isInternalPointer() || node->getOpCode().isArrayLength())
            {
            if (sig && (len > 0) && (sig[0] == '['))
               instanceOfReqd = false;
            else
               testIsArray = true;
            }
         else if (node->getOpCode().hasSymbolReference() &&
                  !node->getSymbolReference()->isUnresolved())
            {
            TR::SymbolReference *otherSymRef = node->getSymbolReference();

            TR_OpaqueClassBlock *cl = NULL;
            if (sig && (len > 0))
               {
               // Currently it's not possible to use this class to eliminate
               // the type check in AOT. That would require a verification
               // record to check the subtyping relationship.
               cl = fe()->getClassFromSignature(sig, len, symRef->getOwningMethod(comp()));
               }

            int32_t otherLen;
            char *otherSig = otherSymRef->getOwningMethod(comp())->classNameOfFieldOrStatic(otherSymRef->getCPIndex(), otherLen);
            dumpOptDetails(comp(), "For node %p len %d other len %d sig %p other sig %p\n", node, len, otherLen, sig, otherSig);
            TR::ResolvedMethodSymbol *owningMethodSym = otherSymRef->getOwningMethodSymbol(comp());
            int32_t classCPI = 0;
            int32_t fieldCPI = otherSymRef->getCPIndex();
            instanceOfReqd = false;
            if (otherSymRef->getSymbol()->isShadow() && fieldCPI >= 0)
               {
               TR_ResolvedMethod *owningMethod = owningMethodSym->getResolvedMethod();
               classCPI = owningMethod->classCPIndexOfFieldOrStatic(fieldCPI);
               bool aotOK = true;
               otherClassObject = owningMethod->getClassFromConstantPool(comp(), classCPI, aotOK);
               instanceOfReqd = true;
               }
#ifdef J9_PROJECT_SPECIFIC
            else
               {
               switch (otherSymRef->getReferenceNumber() - comp()->getSymRefTab()->getNumHelperSymbols())
                  {
                  case TR::SymbolReferenceTable::classFromJavaLangClassSymbol:
                  case TR::SymbolReferenceTable::classFromJavaLangClassAsPrimitiveSymbol:
                     {
                     bool aotOK = true;
                     otherClassObject = comp()->getClassClassPointer(aotOK);
                     classCPI = -1;
                     instanceOfReqd = true;
                     break;
                     }
                  }
               }
#endif

            if (!instanceOfReqd)
               {
               // nothing to do
               }
            else if (otherClassObject == NULL)
               {
               // A type test against the class that's expected to have the
               // field is mandatory but the class pointer is not forthcoming.
               dumpOptDetails(
                  comp(),
                  "Failed to find class from field #%d\n",
                  otherSymRef->getReferenceNumber());
               return false;
               }
            else if (cl != NULL && fe()->isInstanceOf(cl, otherClassObject, true) == TR_yes)
               {
               instanceOfReqd = false;
               }
            else
               {
               TR::SymbolReference *otherClassSymRef =
                  comp()->getSymRefTab()->findOrCreateClassSymbol(
                     owningMethodSym,
                     classCPI,
                     otherClassObject);

               duplicateClassPtr = TR::Node::createWithSymRef(
                  node,
                  TR::loadaddr,
                  0,
                  otherClassSymRef);
               }
            }
         }

      if (instanceOfReqd)
         {
         if (otherClassObject)
            {
            dumpOptDetailsCreatingTest("type", firstChild);
            TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, firstChild, duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
            TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(node, TR::iconst, 0, 0), _exitGotoTarget);
            if (addLoopEntryPrepDep(LoopEntryPrep::TEST, ificmpeqNode, deps, visited) == NULL)
               {
               dumpOptDetailsFailedToCreateTest("type", firstChild);
               return false;
               }
            }
         else if (testIsArray)
            {
#ifdef J9_PROJECT_SPECIFIC
            dumpOptDetailsCreatingTest("array type", firstChild);

            TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1, firstChild, comp()->getSymRefTab()->findOrCreateVftSymbolRef());
            //TR::Node *componentTypeLoad = TR::Node::create(TR::aloadi, 1, vftLoad, comp()->getSymRefTab()->findOrCreateArrayComponentTypeSymbolRef());
            TR::Node *classFlag = comp()->fej9()->testIsClassArrayType(vftLoad);
            TR::Node *zeroNode = TR::Node::iconst(classFlag, 0);
            TR::Node *cmp = TR::Node::createif(TR::ificmpeq, classFlag, zeroNode, _exitGotoTarget);
            if (addLoopEntryPrepDep(LoopEntryPrep::TEST, cmp, deps, visited) == NULL)
               {
               dumpOptDetailsFailedToCreateTest("array type", firstChild);
               return false;
               }
#endif
            }
         else
            {
            //TR_ASSERT(((node->getSymbolReference() == comp()->getSymRefTab()->findVftSymbolRef()) || comp()->getSymRefTab()->findVtableEntrySymbolRef(node->getSymbolReference())), "Not enough information to emit the instanceof test that is reqd\n");
            //dumpOptDetails(comp(), "otherClassObject is NULL in node %p\n", node);
            //printf("otherClassObject is NULL in %s\n", comp()->signature());
            //fflush(stdout);
            }
         }
      }
  else if (node->getOpCode().isIndirect() && node->getFirstChild()->isInternalPointer())
      {
      dumpOptDetailsCreatingTest("bounds", node);

      // This is an array access; so we need to insert explicit
      // checks to mimic the bounds check for the access.
      //
      TR::Node *childInRequiredForm = NULL;
      TR::Node *indexNode = NULL;
      TR::DataType type = TR::NoType;

      // compute the right shift width
      int32_t dataWidth = TR::Symbol::convertTypeToSize(node->getDataType());
      if (comp()->useCompressedPointers() && node->getDataType() == TR::Address)
         dataWidth = TR::Compiler->om.sizeofReferenceField();

      // Holds the array node, or the dataAddr node for off heap
      TR::Node *arrayBaseAddressNode = NULL;

      if (node->getFirstChild()->isDataAddrPointer())
         {
         // This protects the case when off heap allocation is enabled and array
         // access is for the first element in the array, eliminating offset tree.
         // *loadi <== node
         //    aloadi dataAddr_ptr (dataAddrPointer sharedMemory )
         //       aload objt_ptr

         // If array access does not have offset tree, its accessing the first element using the dataAddrPointer
         arrayBaseAddressNode = node->getFirstChild();
         indexNode = TR::Node::iconst(node, 0);
         type = TR::Int32;
         }
      else
         {
         int32_t headerSize = static_cast<int32_t>(TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
         arrayBaseAddressNode = node->getFirstChild()->getFirstChild();
         TR::Node *offset = node->getFirstChild()->getSecondChild();
         type = offset->getType();

         /* childInRequiredForm is expected to be shift/multiply node. When
            off heap allocation is enabled and the sibling is dataAddr pointer
            node, instead of using objt_ptr + header_size we load address of first
            array element from dataAddr field in the header and add the offset. */
         if (arrayBaseAddressNode->isDataAddrPointer())
            {
            // This is what trees will look like with dataAddr load
            // aloadi < node
            //    aladd
            //       ==>aload dataAddr_ptr (dataAddrPointer sharedMemory )
            //       shl/mul < childInRequiredForm/offset
            childInRequiredForm = node->getFirstChild()->getSecondChild();
            }
         else
            {
            // aloadi < node
            //    aladd
            //       ==>aload objt_ptr
            //       add < offset
            //          shl/mul < childInRequiredForm
            //          const array_header
            static struct temps
               {
               TR::ILOpCodes addOp;
               TR::ILOpCodes constOp;
               int32_t k;
               }
            a[] =
               {{TR::iadd, TR::iconst,  headerSize},
               {TR::isub, TR::iconst, -headerSize},
               {TR::ladd, TR::lconst,  headerSize},
               {TR::lsub, TR::lconst, -headerSize}};
            for (int32_t index = sizeof(a)/sizeof(temps) - 1; index >= 0; --index)
               {
               if (offset->getOpCodeValue() == a[index].addOp &&
                  offset->getSecondChild()->getOpCodeValue() == a[index].constOp &&
                  offset->getSecondChild()->getInt() == a[index].k)
                  {
                  childInRequiredForm = offset->getFirstChild();
                  break;
                  }
               }
            }

         int32_t shiftWidth = TR::TransformUtil::convertWidthToShift(dataWidth);

         if (childInRequiredForm)
            {
            if (childInRequiredForm->getOpCodeValue() == TR::ishl || childInRequiredForm->getOpCodeValue() == TR::lshl)
               {
               if (childInRequiredForm->getSecondChild()->getOpCode().isLoadConst() &&
                  (childInRequiredForm->getSecondChild()->getInt() == shiftWidth))
                  indexNode = childInRequiredForm->getFirstChild();
               }
            else if (childInRequiredForm->getOpCodeValue() == TR::imul || childInRequiredForm->getOpCodeValue() == TR::lmul)
               {
               if (childInRequiredForm->getSecondChild()->getOpCode().isLoadConst() &&
                  (childInRequiredForm->getSecondChild()->getInt() == dataWidth))
                  indexNode = childInRequiredForm->getFirstChild();
               }
            }

         if (!indexNode)
            {
            if (!childInRequiredForm)
               {
               // We don't need to subtract the header size when we are using dataAddr pointer node
               // because we already have a pointer to the first element in the array.
               // For Reference see Walker.cpp:calculateElementAddressInContiguousArray
               if (arrayBaseAddressNode->isDataAddrPointer())
                  indexNode = offset;
               else
                  {
                  TR::Node *constNode = TR::Node::create(node, type.isInt32() ? TR::iconst : TR::lconst, 0, 0);
                  indexNode = TR::Node::create(type.isInt32() ? TR::isub : TR::lsub, 2, offset, constNode);
                  if (type.isInt64())
                     constNode->setLongInt((int64_t)headerSize);
                  else
                     constNode->setInt((int64_t)headerSize);
                  }
               }
            else
               indexNode = childInRequiredForm;


            indexNode = TR::Node::create(type.isInt32() ? TR::iushr : TR::lushr, 2, indexNode,
                                             TR::Node::create(node, TR::iconst, 0, shiftWidth));
            }
         }

      TR::Node *base = arrayBaseAddressNode;
      if (base->isDataAddrPointer())
         base = base->getFirstChild();
      TR::Node *arrayLengthNode = TR::Node::create(TR::arraylength, 1, base);

      arrayLengthNode->setArrayStride(dataWidth);
      if (type.isInt64())
         arrayLengthNode = TR::Node::create(TR::i2l, 1, arrayLengthNode);

      TR::Node *ificmpgeNode = TR::Node::createif(type.isInt32() ? TR::ificmpge : TR::iflcmpge, indexNode, arrayLengthNode, _exitGotoTarget);
      if (addLoopEntryPrepDep(LoopEntryPrep::TEST, ificmpgeNode, deps, visited) == NULL)
         {
         dumpOptDetailsFailedToCreateTest("part 1 of bounds", node);
         return false;
         }

      TR::Node *constNode = 0;
      if(type.isInt32())
         constNode = TR::Node::create(arrayLengthNode, TR::iconst , 0, 0);
      else
         constNode = TR::Node::create(arrayLengthNode, TR::lconst, 0, 0);

      TR::Node *ificmpltNode = TR::Node::createif(type.isInt32() ? TR::ificmplt : TR::iflcmplt, indexNode, constNode, _exitGotoTarget);
      if (type.isInt64())
         ificmpltNode->getSecondChild()->setLongInt(0);

      if (addLoopEntryPrepDep(LoopEntryPrep::TEST, ificmpltNode, deps, visited) == NULL)
         {
         dumpOptDetailsFailedToCreateTest("part 2 of bounds", node);
         return false;
         }
      }
   else if (((node->getOpCodeValue() == TR::idiv) || (node->getOpCodeValue() == TR::irem) || (node->getOpCodeValue() == TR::lrem) || (node->getOpCodeValue() == TR::ldiv)))
      {
      // This is a divide; so we need to insert explicit checks
      // to mimic the div check.
      //
      TR::Node *divisor = node->getSecondChild();
      bool divisorIsNonzeroConstant =
         (divisor->getOpCodeValue() == TR::lconst && divisor->getLongInt() != 0)
         || (divisor->getOpCodeValue() == TR::iconst && divisor->getInt() != 0);

      if (!divisorIsNonzeroConstant)
         {
         dumpOptDetailsCreatingTest("divide-by-zero", node);

         TR::Node *ifNode;
         if (divisor->getType().isInt64())
            ifNode = TR::Node::createif(TR::iflcmpeq, divisor, TR::Node::create(node, TR::lconst, 0, 0), _exitGotoTarget);
         else
            ifNode = TR::Node::createif(TR::ificmpeq, divisor, TR::Node::create(node, TR::iconst, 0, 0), _exitGotoTarget);

         if (addLoopEntryPrepDep(LoopEntryPrep::TEST, ifNode, deps, visited) == NULL)
            {
            dumpOptDetailsFailedToCreateTest("divide-by-zero", node);
            return false;
            }
         }
      }
   else if (node->getOpCode().isCheckCast()) //(node->getOpCodeValue() == TR::checkcast)
      {
      TR_ASSERT_FATAL(
         false,
         "depsForLoopEntryPrep: n%un is a checkcast",
         node->getGlobalIndex());
      }

   // Get any further prerequisite LoopEntryPreps beyond those required for the
   // ones already in deps. Doing this last avoids copying transitive dependencies
   // into deps.
   for (int i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (!depsForLoopEntryPrep(child, deps, visited, true))
         return false;
      }

   return true;
   }

/**
 * \brief Create a LoopEntryPrep and add it as a dependency to \p deps.
 *
 * \param kind The kind of LoopEntryPrep to create
 * \param node The node specifying the expression of the LoopEntryPrep
 * \param deps The dependency list to which to add the resulting LoopEntryPrep
 * \param visited The visited set corresponding to \p deps
 * \return the created LoopEntryPrep, or null on failure
 *
 * \see depsForLoopEntryPrep()
 * \see createLoopEntryPrep()
 */
TR_LoopVersioner::LoopEntryPrep *TR_LoopVersioner::addLoopEntryPrepDep(
   LoopEntryPrep::Kind kind,
   TR::Node *node,
   TR::list<LoopEntryPrep*, TR::Region&> *deps,
   TR::NodeChecklist *visited)
   {
   // The checklist passed to createLoopEntryPrep() here must be fresh. It's
   // fine to propagate visited nodes up to dependents, but not down to
   // dependencies, because propagating down could cause a dependency to miss
   // one of *its* (transitive) dependencies that had already been seen in a
   // sibling.
   TR::NodeChecklist innerVisited(comp());
   LoopEntryPrep *prep = createLoopEntryPrep(kind, node, &innerVisited);
   if (prep == NULL)
      return NULL;

   deps->push_back(prep);
   visited->add(innerVisited);
   return prep;
   }

TR::Node *TR_LoopVersioner::findLoad(TR::Node *node, TR::SymbolReference *symRef, vcount_t origVisitCount)
   {
   if (node->getVisitCount() >= origVisitCount)
      return NULL;

   node->setVisitCount(origVisitCount);

   if (node->getOpCode().hasSymbolReference() &&
       (node->getSymbolReference() == symRef))
      return node;

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      {
      TR::Node *load = findLoad(node->getChild(i), symRef, origVisitCount);
      if (load)
         return load;
      }

   return NULL;
   }

bool TR_LoopVersioner::findStore(TR::TreeTop *start, TR::TreeTop *end, TR::Node *node, TR::SymbolReference *symRef, bool needAuxiliaryWalk, bool lastTimeThrough)
   {
   bool foundLoad = false;
   TR::TreeTop *cursorTree = start;
   vcount_t origVisitCount = comp()->incVisitCount();
   // walk the trees looking for a store of
   // the index-child symRef
   while (cursorTree != end)
      {
      TR::Node *cursorNode = cursorTree->getNode()->getStoreNode();
      if (cursorNode)
         {
         if (cursorNode->getSymbolReference() == symRef)
            {
            TR::Node *indexChild = cursorNode->getFirstChild();
            while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub() || indexChild->getOpCode().isAnd())
               {
               if (indexChild->getSecondChild()->getOpCode().isLoadConst())
                  indexChild = indexChild->getFirstChild();
               else
                  {
                  bool isSecondChildInvariant = isExprInvariant(indexChild->getSecondChild());
                  if (isSecondChildInvariant)
                     indexChild = indexChild->getFirstChild();
                  else
                     {
                     bool isFirstChildInvariant = isExprInvariant(indexChild->getFirstChild());
                     if (isFirstChildInvariant)
                        indexChild = indexChild->getSecondChild();
                     else
                        {
                        break;
                        }
                     }
                  }
               }

            // a store of the index-child symRef was found;
            // check if the load of this store and the index-child
            // are the same. if they are, then the bndchk uses the unchanged
            // value; however check if a load of this symRef was found in
            // the lastTimeThrough walk before asserting that a store was
            // found
            //
            if (!needAuxiliaryWalk && (indexChild == node))
               return false;
            else if (lastTimeThrough && foundLoad)
               return false;
            else
               return true;
            }
         }

      // donot return prematurely during
      // the lastTimeThrough walk when just a load of the index-child
      // symRef is found. it must be ensured that a store of the index-child
      // symRef is found in the same block. if no such store is found,
      // then the index-child must have been changed in some other block
      //
      if (!needAuxiliaryWalk &&
            findLoad(cursorTree->getNode(), symRef, origVisitCount) == node)
         {
         if (lastTimeThrough)
            foundLoad = true;
         else
            return false;
         }

      cursorTree = cursorTree->getNextTreeTop();
      }

   // a store for the index-symRef was not found in this walk;
   // walk from the beginning of the block to the bndchk looking
   // for a store for the index-symRef which has a load commoned with the bndchk
   // if such a store is found, then the bndchk uses the unchanged value
   //
   if (needAuxiliaryWalk)
      {
      // lastTimeThrough the block
      if (!findStore(start->getEnclosingBlock()->getEntry(), start, node, symRef, false, true))
         return true;
      else
         return false;
      }
   else
      {
      // a corresponding store for the index-child symRef was not found
      // if its the lastTimeThrough (this implies that the block being walked
      // is at the end of the loop) and just a load was found with no
      // store, then the index-child must have been changed in some other block
      //
      if (lastTimeThrough) // foundLoad
         return true;
      else
         return false;
      }
   }

int32_t TR_LoopVersioner::detectCanonicalizedPredictableLoops(TR_Structure *loopStructure, TR_BitVector **optSetInfo, int32_t bitVectorSize)
   {
   int32_t retval = 1;
   if (!loopStructure->getParent())
      return -3;

   TR_ScratchList<TR::Block> blocksInWhileLoop(trMemory());
   loopStructure->getBlocks(&blocksInWhileLoop);
   int32_t loop_size = blocksInWhileLoop.getSize();
   int32_t hotnessFactor = 1;
   if (comp()->getMethodHotness() < warm)
      hotnessFactor = 2;

   if (loop_size > (MAX_BLOCKS_THRESHOLD/hotnessFactor))
      return -2;

   int32_t normalNodeCount = (2*NORMAL_NODE_COUNT)/hotnessFactor;
   int32_t normalBlockCount = (2*NORMAL_BLOCK_COUNT)/hotnessFactor;

   int32_t nodeCountFactor = std::min(8, std::max(1, (_origNodeCount/normalNodeCount)));
   int32_t blockCountFactor = std::min(8, std::max(1, (_origBlockCount/normalBlockCount)));

   ncount_t nodeCount;
   nodeCount = comp()->getAccurateNodeCount();

   if ((nodeCount/(MAX_SIZE_INCREASE_FACTOR/hotnessFactor)) > unsigned((_origNodeCount/nodeCountFactor)))
      {
      if (trace())
         traceMsg(comp(), "Failing node count %d orig %d factor %d\n", nodeCount, _origNodeCount, nodeCountFactor);
      return -2;
      }

   if ((comp()->getFlowGraph()->getNodes().getSize()/(MAX_SIZE_INCREASE_FACTOR/hotnessFactor)) > (_origBlockCount/blockCountFactor))
      {
      if (trace())
         traceMsg(comp(), "Failing block count %d orig %d factor %d\n", comp()->getFlowGraph()->getNodes().getSize(), _origBlockCount, blockCountFactor);
      return -2;
      }

   int32_t currentNestingDepth = 0;
   int32_t currentMaxNestingDepth = 0;
   if (loopStructure->getMaxNestingDepth(&currentNestingDepth, &currentMaxNestingDepth) > (MAX_NESTING_DEPTH/hotnessFactor))
      return -2;


   TR_RegionStructure *regionStructure = loopStructure->asRegion();
   TR_BlockStructure *loopInvariantBlock = NULL;

   TR_RegionStructure *parentStructure = regionStructure->getParent()->asRegion();
   TR_StructureSubGraphNode *subNode = NULL;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getNumber() == loopStructure->getNumber())
         break;
      }

   if (subNode->getPredecessors().size() == 1)
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock() &&
          loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
         loopInvariantBlock = loopInvariantNode->getStructure()->asBlock();
      }


   if (loopInvariantBlock)
       {
       /////if (isInvariantLoop)
          {
          int32_t symRefCount = comp()->getSymRefCount();

          initializeSymbolsWrittenAndReadExactlyOnce(symRefCount, notGrowable);
          _hasPredictableExits = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
          _hasPredictableExits->setAll(symRefCount);
          //_writtenAndNotJustForHeapification = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);

          if (trace())
             traceMsg(comp(), "\nChecking loop %d for predictability\n", loopStructure->getNumber());

          _isAddition = false;
          _loopTestTree = NULL;
          int32_t isPredictableLoop = checkLoopForPredictability(loopStructure, loopInvariantBlock->getBlock(), NULL);

          if ((isPredictableLoop > 0) &&
              ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmplt) ||
               (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpgt) ||
               (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpge) ||
               (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmple)))
             {
             if (trace())
                {
                dumpOptDetails(comp(), "\nDetected a predictable loop %d\n", loopStructure->getNumber());
                dumpOptDetails(comp(), "Possible new induction variable candidates :\n");
                comp()->getDebug()->print(comp()->getOutFile(), &_writtenExactlyOnce);
                dumpOptDetails(comp(), "\n");
                }

             TR_ScratchList<TR::TreeTop> predictableComputations(trMemory());

             TR::SparseBitVector::Cursor cursor(_writtenExactlyOnce);
             bool flushDerivedInductionVariables=false;
             for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                {
                int32_t nextInductionVariableNumber = cursor;
                bool storeInRequiredForm = true;

                /////if (nextInductionVariableNumber != _loopDrivingInductionVar)
                _isAddition = false;
                _loadUsedInLoopIncrement = NULL;
                /////_loadUsedInNewLoopIncrement = NULL;
                _requiresAdditionalCheckForIncrement = false;
                storeInRequiredForm = isStoreInRequiredForm(nextInductionVariableNumber, loopStructure);
                //dumpOptDetails(comp(), "Store of %d in reqd form %d\n", nextInductionVariableNumber, storeInRequiredForm);

                if (_isAddition)
                   _additionInfo->set(nextInductionVariableNumber);

                if (storeInRequiredForm)
                   {
                   TR::Block *currentBlock = _storeTrees[nextInductionVariableNumber]->getEnclosingBlock();
                   if (!blockIsAlwaysExecutedInLoop(currentBlock, regionStructure))
                      storeInRequiredForm = false;
                   else if (currentBlock->getStructureOf()->getContainingCyclicRegion() != regionStructure)
                      storeInRequiredForm = false;
                   }

                if (storeInRequiredForm /* && !_cannotBeEliminated->get(nextInductionVariableNumber) */)
                   {
                   int32_t *versionableInductionVariable = (int32_t *)trMemory()->allocateStackMemory(sizeof(int32_t));
                   *versionableInductionVariable = nextInductionVariableNumber;
                   _derivedVersionableInductionVariables.add(versionableInductionVariable);
                   TR::Node *loopTestNode = _loopTestTree->getNode();

                   if ((loopTestNode->getFirstChild()->getOpCode().hasSymbolReference() &&
							nextInductionVariableNumber == loopTestNode->getFirstChild()->getSymbolReference()->getReferenceNumber()) ||
						loopTestNode->getFirstChild() == _storeTrees[nextInductionVariableNumber]->getNode()->getFirstChild())

                      {
                      versionableInductionVariable = _derivedVersionableInductionVariables.popHead();
                      _versionableInductionVariables.add(versionableInductionVariable);

                      if(_requiresAdditionalCheckForIncrement)
                         flushDerivedInductionVariables=true;

                      if (trace())
                           traceMsg(comp(), "Version loop : %d with respect to induction variable %d\n", loopStructure->getNumber(), nextInductionVariableNumber);
                      }
                   }
                else if (isStoreInSpecialForm(nextInductionVariableNumber, loopStructure))
                   {
                   int32_t *versionableInductionVariable = (int32_t *)trMemory()->allocateStackMemory(sizeof(int32_t));
                   *versionableInductionVariable = nextInductionVariableNumber;
                   _specialVersionableInductionVariables.add(versionableInductionVariable);

                   if (trace())
                        traceMsg(comp(), "Version loop : %d with respect to induction variable %d\n", loopStructure->getNumber(), nextInductionVariableNumber);
                   }
                }

             //clearing the derived inductionVariables when primary induction variable had a non-const increment.
              if(flushDerivedInductionVariables)
                 _derivedVersionableInductionVariables.deleteAll();

             // look at the exit edges to see if there are multiple loop tests
             //
             if (loopStructure->asRegion())
                {
                TR_ScratchList<TR::Block> exitBlocks(trMemory());
                loopStructure->collectExitBlocks(&exitBlocks);
                // _hasPredictableExits = true
                //
                ListIterator<TR::Block> eb(&exitBlocks);
                for (TR::Block *b = eb.getFirst(); b; b = eb.getNext())
                   {
                   if (b == _loopTestBlock)
                      continue;

                   // now looking at the block that exits the loop but
                   // its not the loop test, see if it involves an iv
                   //
                   TR::Node *ifNode = b->getLastRealTreeTop()->getNode();
                   if (ifNode->getOpCode().isIf() && !ifNode->getOpCode().isCompBranchOnly())
                      {
                      TR::SymbolReference *ivSymRef = NULL;
                      TR::Node *ivNode = ifNode->getFirstChild();
                      if (ivNode->getOpCode().hasSymbolReference())
                         ivSymRef = ivNode->getSymbolReference();
                      else
                         {
                         if ((ivNode->getOpCode().isAdd() || ivNode->getOpCode().isSub()) &&
                               ivNode->getSecondChild()->getOpCode().isLoadConst())
                            ivNode = ivNode->getFirstChild();
                         if (ivNode &&
                               ivNode->getOpCode().hasSymbolReference())
                            ivSymRef = ivNode->getSymbolReference();
                         }

                      bool foundInductionVariable = false;
                      if (ivSymRef)
                         {
                         ListElement<int32_t> *v = _derivedVersionableInductionVariables.getListHead();
                         while (v)
                            {
                            if (*(v->getData()) == ivSymRef->getReferenceNumber())
                               {
                               foundInductionVariable = true;
                               break;
                               }
                            v = v->getNextElement();
                            }
                         }

                      // loop has more than one loop exit based on an
                      // inductionvariable. this means that we cannot
                      // predict which loop exit is taken if we're trying
                      // to version for example
                      //
                      if (foundInductionVariable)
                         _hasPredictableExits->reset(ivSymRef->getReferenceNumber());
                      }
                   }
                }
             }
          }
       }
   else
      {
      retval = -1;
      }
   return retval;
   }



bool TR_LoopVersioner::isStoreInSpecialForm(int32_t symRefNum, TR_Structure *loopStructure)
   {
   if (!comp()->getSymRefTab()->getSymRef(symRefNum)->getSymbol()->isAutoOrParm())
      return false;
   TR::Node *storeNode = _storeTrees[symRefNum]->getNode();
   if (!storeNode->getType().isInt32())
      return false;

   if (storeNode->getFirstChild()->getOpCode().isAnd() &&
       storeNode->getFirstChild()->getSecondChild()->getOpCode().isLoadConst() &&
       (storeNode->getFirstChild()->getSecondChild()->getInt() > 0) &&
       (storeNode->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::iload) &&
       (storeNode->getFirstChild()->getFirstChild()->getSymbolReference()->getReferenceNumber() == storeNode->getSymbolReference()->getReferenceNumber()))
      return true;

   return false;
   }






bool TR_LoopVersioner::isStoreInRequiredForm(int32_t symRefNum, TR_Structure *loopStructure)
   {
   /* [101214] calls too can kill symRefs via loadaddr */
   if (symRefNum != 0 && _allKilledSymRefs[symRefNum] == true)
      return false;

   if (!comp()->getSymRefTab()->getSymRef(symRefNum)->getSymbol()->isAutoOrParm())
      return false;

   TR::Node *storeNode = _storeTrees[symRefNum]->getNode();
   if (!storeNode->getType().isInt32() && !storeNode->getType().isInt64())
      return false;

   TR::Node *addNode = storeNode->getFirstChild();
   if (isInverseConversions(addNode))
      addNode = addNode->getFirstChild()->getFirstChild();

   _constNode = containsOnlyInductionVariableAndAdditiveConstant(addNode, symRefNum);
   if (_constNode == NULL)
      return false;

   TR::Node *secondChild = _constNode;
   if (!secondChild->getOpCode().isLoadConst() && (secondChild->getType().isInt32() || secondChild->getType().isInt64()))
      {
      // This case is currently not handled correctly:
      // 1. Versioning tests think the step is zero, and they divide by zero.
      // 2. The initial versioning test (that checks the sign of the variable
      // loaded by _constNode) always checks that the step is positive, even if
      // it should be negative in order to agree with the direction of the loop
      // test comparison.
      static const bool allowVariableStep =
         feGetEnv("TR_loopVersionerAllowVariableStep") != NULL;
      if (!allowVariableStep)
         return false;

      // This requires isAutoOrParm() because other threads can write to statics
      if (secondChild->getOpCode().isLoadVarDirect()
          && secondChild->getSymbol()->isAutoOrParm())
         {
         int32_t timesWritten = 0;
         if (!isSymbolReferenceWrittenNumberOfTimesInStructure(loopStructure, secondChild->getSymbolReference()->getReferenceNumber(), &timesWritten, 0))
            return false;
         _requiresAdditionalCheckForIncrement = true;
         }
      else
         return false;
      }
   else
      {
      if (((secondChild->getType().isInt32()) && (secondChild->getInt() < 0)) ||
          ((secondChild->getType().isInt64()) && (secondChild->getLongInt() < 0)))
         {
         if (_isAddition)
            _isAddition = false;
         else
            _isAddition = true;
         }
      }

   _constNode = _constNode->duplicateTree();
   _constNode->setReferenceCount(0);

   _loopDrivingInductionVar = symRefNum;
   _insertionTreeTop = _storeTrees[symRefNum];

   return true;
   }


bool TR_LoopVersioner::processArrayAliasCandidates()
   {
   TR_ASSERT(!refineAliases(),"something wrong");
   return false;
}
void TR_LoopVersioner::collectArrayAliasCandidates(TR::Node *node,vcount_t visitCount){ TR_ASSERT(!refineAliases(),"something wrong");}
void TR_LoopVersioner::buildAliasRefinementComparisonTrees(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, TR_ScratchList<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock){TR_ASSERT(!refineAliases(),"something wrong");}
void TR_LoopVersioner::initAdditionalDataStructures(){}
void TR_LoopVersioner::refineArrayAliases(TR_RegionStructure *r) {TR_ASSERT(!refineAliases(),"something wrong");}

void TR_LoopVersioner::findAndReplaceContigArrayLen(TR::Node *parent, TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   if ((node->getOpCodeValue() == TR::contigarraylength))  //future: || (node->getOpCodeValue() == TR::discontigarraylength))
	  TR::Node::recreate(node, TR::arraylength);


   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      findAndReplaceContigArrayLen(node, node->getChild(i), visitCount);

   }


bool TR_LoopVersioner::isInverseConversions(TR::Node* node)
   {
   if (node->getOpCode().isConversion() &&
      node->getFirstChild()->getOpCode().isConversion() &&
      (node->isNonNegative() || node->isNonPositive()))
      {
      TR::Node *conversion1 = node;
      TR::Node *conversion2 = node->getFirstChild();
      if ((conversion1->getOpCodeValue() == TR::s2i && conversion2->getOpCodeValue() == TR::i2s) ||
          (conversion1->getOpCodeValue() == TR::b2i && conversion2->getOpCodeValue() == TR::i2b) || // FIXME: why are we ignoring bu2i/i2b here?
          (conversion1->getOpCodeValue() == TR::su2i && conversion2->getOpCodeValue() == TR::i2s))
         return true;
      }
   return false;
   }

const char *
TR_LoopVersioner::optDetailString() const throw()
   {
   return "O^O LOOP VERSIONER: ";
   }

/**
 * \brief Initialize \p expr based on \p node.
 *
 * The children are set to null, and populating them is the caller's
 * responsibility, because different callers have different requirements.
 *
 * \param expr The Expr to initialize
 * \param node The node based on which to initialize \p expr
 * \param onlySearching True when \p expr will only be used as a key for
 * lookup, and won't be inserted into CurLoop::_exprTable
 * \return true on success, false if \p node is unrepresentable
 */
bool TR_LoopVersioner::initExprFromNode(Expr *expr, TR::Node *node, bool onlySearching)
   {
   // Reject nodes that can't be properly represented.
#ifdef J9_PROJECT_SPECIFIC
   if (node->getOpCode().isConversionWithFraction())
      return false;
#endif

   if (node->getNumChildren() > Expr::MAX_CHILDREN)
      return false;

   // Reject unrecognized nop-able guards, which may contain unevaluable nodes.
   if (node->isNopableInlineGuard() && !guardOkForExpr(node, onlySearching))
      return false;

   // Initialize _op.
   expr->_op = node->getOpCode();

   // Initialize _constValue, _symRef, or _guard as appropriate.
   expr->_constValue = 0;
   if (expr->_op.isLoadConst())
      {
      expr->_constValue = node->getConstValue();
      }
   else if (expr->_op.hasSymbolReference())
      {
      // Loads from the same original symref should intern to the same Expr.
      expr->_symRef = node->getSymbolReference()->getOriginalUnimprovedSymRef(comp());
      }
   else if (expr->_op.isIf())
      {
      expr->_guard = node->virtualGuardInfo();
      TR_ASSERT_FATAL(
         node->getBranchDestination() == _exitGotoTarget,
         "versioning test n%un [%p] does not target _exitGotoTarget",
         node->getGlobalIndex(),
         node);
      }

   // Initialize _mandatoryFlags. Any flag not preserved here may be cleared.
   // IMPORTANT: Only flags that affect the semantics of a node should be
   // included in _mandatoryFlags. Otherwise irrelevant flags set on original
   // nodes in the loop will prevent substitution of privatization temps.
   expr->_mandatoryFlags = flags32_t(0);
   if (expr->_op.getOpCodeValue() == TR::aconst)
      {
      flags32_t origFlags = node->getFlags();
      bool isClassPointerConstant = node->isClassPointerConstant();
      bool isMethodPointerConstant = node->isMethodPointerConstant();
      node->setFlags(flags32_t(0));
      node->setIsClassPointerConstant(isClassPointerConstant);
      node->setIsMethodPointerConstant(isMethodPointerConstant);
      expr->_mandatoryFlags = node->getFlags();
      node->setFlags(origFlags);
      }

   // Clear _children. Proper values here are the caller's responsibility.
   for (int i = 0; i < Expr::MAX_CHILDREN; i++)
      expr->_children[i] = NULL;

   // Set advisory info.
   expr->_bci = node->getByteCodeInfo();
   expr->_flags = node->getFlags();

   // _flags must be a superset of _mandatoryFlags.
   int32_t allFlags = expr->_flags.getValue();
   int32_t mandatoryFlags = expr->_mandatoryFlags.getValue();
   TR_ASSERT_FATAL(
      (allFlags & mandatoryFlags) == mandatoryFlags,
      "setting _flags 0x%x would fail to preserve _mandatoryFlags 0x%x\n",
      allFlags,
      mandatoryFlags);

   return true;
   }

// needle both begins and ends with a comma
static bool containsCommaSeparated(const char *haystack, const char *needle)
   {
   int haystackLen = (int)strlen(haystack);
   int needleLen = (int)strlen(needle);

   if (haystackLen < needleLen - 2)
      return false;
   else if (haystackLen == needleLen - 2)
      return strncmp(haystack, needle + 1, needleLen - 2) == 0;
   else if (strncmp(haystack, needle + 1, needleLen - 1) == 0)
      return true;
   else if (strncmp(haystack + haystackLen - needleLen + 1, needle, needleLen - 1) == 0)
      return true;
   else
      return strstr(haystack, needle) != NULL;
   }

/**
 * \brief Determine whether to allow the nop-able guard \p node to be
 * represented as an Expr.
 *
 * Nop-able guards may contain unevaluable nodes, the values of which versioner
 * must be sure not to privatize. Check whether it is known to be safe to
 * version the guard kind/test combination found in \p node.
 *
 * The result of this function can be controlled using the environment variables
 * \c TR_allowGuardForVersioning and \c TR_forbidGuardForVersioning to
 * respectively allow and forbid guard kind/test combinations. Each is a
 * comma-separated list of kind:test pairs, where the kind and test are both
 * specified as the integer values of the TR_VirtualGuardKind and
 * TR_VirtualGuardTestType, e.g. to forbid TR_AbstractGuard:TR_MethodTest and
 * TR_HierarchyGuard:TR_VftTest, <tt>TR_forbidGuardForVersioning=5:2,6:1</tt>
 * (subject to rearrangement of the enum entries). Integers are used because
 * names currently require a TR_Debug object. No leading + or 0, or leading or
 * trailing whitespace is accepted.
 *
 * \param node The nop-able guard node
 * \param onlySearching True when node is not being copied into CurLoop::_exprTable
 * \return true if the guard is allowed, or else false to prevent versioning
 *
 * \see suppressInvarianceAndPrivatization()
 * \see makeCanonicalExpr()
 */
bool TR_LoopVersioner::guardOkForExpr(TR::Node *node, bool onlySearching)
   {
   TR_VirtualGuard *guard = comp()->findVirtualGuardInfo(node);
   TR_VirtualGuardKind kind = guard->getKind();
   TR_VirtualGuardTestType test = guard->getTestType();

   if (trace())
      {
      traceMsg(
         comp(),
         "guardOkForExpr? %s:%s\n",
         comp()->getDebug()->getVirtualGuardKindName(kind),
         comp()->getDebug()->getVirtualGuardTestTypeName(test));
      }

   static const char * const allowEnv = feGetEnv("TR_allowGuardForVersioning");
   static const char * const forbidEnv = feGetEnv("TR_forbidGuardForVersioning");
   if (allowEnv != NULL || forbidEnv != NULL)
      {
      char needle[32];
      TR::snprintfNoTrunc(needle, sizeof (needle), ",%d:%d,", (int)kind, (int)test);

      if (allowEnv != NULL && containsCommaSeparated(allowEnv, needle))
         return true;
      else if (forbidEnv != NULL && containsCommaSeparated(forbidEnv, needle))
         return false;
      }

   switch (kind)
      {
      // A guard type may be allowed here only after determining whether it
      // has unevaluable nodes, and if it does, ensuring that those nodes
      // are detected by suppressInvarianceAndPrivatization(), so that they
      // will not be privatized.

      case TR_InterfaceGuard:
      case TR_AbstractGuard:
         return test == TR_MethodTest;

      case TR_HierarchyGuard:
         return test == TR_VftTest || test == TR_MethodTest;

      case TR_NonoverriddenGuard:
         return test == TR_NonoverriddenTest || test == TR_VftTest;

      case TR_DirectMethodGuard:
         return test == TR_NonoverriddenTest;

      // These guard types are versionable, but they require special
      // handling. They should not go through makeCanonicalExpr().
      case TR_HCRGuard:
         TR_ASSERT_FATAL(
            onlySearching,
            "guardOkForExpr: should not intern HCR guard n%un [%p]",
            node->getGlobalIndex(),
            node);
         return false;

      case TR_OSRGuard:
         TR_ASSERT_FATAL(
            onlySearching,
            "guardOkForExpr: should not intern OSR guard n%un [%p]",
            node->getGlobalIndex(),
            node);
         return false;

      // Never versioned, only holds inner assumptions.
      case TR_DummyGuard:
         TR_ASSERT_FATAL(
            onlySearching,
            "guardOkForExpr: should not intern dummy guard n%un [%p]",
            node->getGlobalIndex(),
            node);
         return false;

      // Never versioned (for now).
      case TR_BreakpointGuard:
         TR_ASSERT_FATAL(
            onlySearching,
            "guardOkForExpr: should not intern breakpoint guard n%un [%p]",
            node->getGlobalIndex(),
            node);
         return false;

      // These kinds have not yet been checked. Conservatively disallow
      // versioning for now.
      case TR_SideEffectGuard:
      case TR_MutableCallSiteTargetGuard:
      case TR_MethodEnterExitGuard:
      case TR_InnerGuard:
      case TR_ArrayStoreCheckGuard:
         return false;

      default:
         // Alert developers adding a new guard kind to the potential
         // performance problem caused by refusing to version. This can be
         // fixed by opting in or out just above.
         //
         // This assert will also fire in case guard is somehow not nop-able.
         //
         TR_ASSERT_FATAL(
            false,
            "guardOkForExpr: n%un [%p]: unrecognized nop-able guard kind %d",
            node->getGlobalIndex(),
            node,
            (int)kind);
      }

   return false;
   }

/**
 * \brief Intern \p node as an Expr.
 *
 * The resulting Expr is effectively a copy of \p node, but nonetheless \p node
 * and its descendants must \em not be mutated in-place after interning, since
 * CurLoop::_nodeToExpr is updated to remember the correspondence between each
 * node in the subtree and its respective Expr, and modifications to any of
 * these nodes could invalidate the associations.
 *
 * While it is not necessarily incorrect for \p node for to be unrepresentable,
 * it is unexpected and likely to cause a performance problem. So in case
 * \p node \em is unrepresentable, this method increments a static debug
 * counter matching <tt>{loopVersioner.unrepresentable*}</tt>, and optionally
 * fails an assertion when enabled using \c ASSERT_REPRESENTABLE_IN_VERSIONER
 * or \c TR_assertRepresentableInVersioner.
 *
 * \p node The node to be represented as an Expr
 * \return the pointer to the canonical Expr instance corresponding to \p node,
 * or null if \p node is unrepresentable
 */
const TR_LoopVersioner::Expr *TR_LoopVersioner::makeCanonicalExpr(TR::Node *node)
   {
   auto cached = _curLoop->_nodeToExpr.find(node);
   if (cached != _curLoop->_nodeToExpr.end())
      return cached->second;

   static const bool assertRepresentableEnv =
      feGetEnv("TR_assertRepresentableInVersioner") != NULL;

   Expr expr;
   bool onlySearching = false; // expr will be inserted into CurLoop::_exprTable
   if (!initExprFromNode(&expr, node, onlySearching))
      {
      dumpOptDetails(
         comp(),
         "n%un [%p] is unrepresentable\n",
         node->getGlobalIndex(),
         node);

#ifdef ASSERT_REPRESENTABLE_IN_VERSIONER
      bool shouldAssert = true;
#else
      bool shouldAssert = assertRepresentableEnv;
#endif
      if (shouldAssert)
         {
         if (node->isNopableInlineGuard())
            {
            TR_VirtualGuard *guard = comp()->findVirtualGuardInfo(node);
            TR_ASSERT_FATAL(
               false,
               "n%un [%p] is unrepresentable guard kind=%d, test=%d",
               node->getGlobalIndex(),
               node,
               (int)guard->getKind(),
               (int)guard->getTestType());
            }
         else
            {
            TR_ASSERT_FATAL(
               false,
               "n%un [%p] is unrepresentable",
               node->getGlobalIndex(),
               node);
            }
         }

      TR::DebugCounter::incStaticDebugCounter(
         comp(),
         TR::DebugCounter::debugCounterName(
            comp(),
            "loopVersioner.unrepresentable/(%s)/%s/loop=%d/n%un",
            comp()->signature(),
            comp()->getHotnessName(comp()->getMethodHotness()),
            _curLoop->_loop->getNumber(),
            node->getGlobalIndex()));

      return NULL;
      }

   for (int i = 0; i < node->getNumChildren(); i++)
      {
      const Expr *child = makeCanonicalExpr(node->getChild(i));
      if (child == NULL)
         return NULL;

      expr._children[i] = child;
      }

   const Expr *result = NULL;
   auto existing = _curLoop->_exprTable.find(expr);
   if (existing != _curLoop->_exprTable.end())
      {
      result = existing->second;
      }
   else
      {
      result = new (_curLoop->_memRegion) Expr(expr);
      _curLoop->_exprTable.insert(std::make_pair(expr, result));
      }

   if (trace())
      {
      traceMsg(
         comp(),
         "Canonical n%un [%p] is expr %p\n",
         node->getGlobalIndex(),
         node,
         result);
      }

   _curLoop->_nodeToExpr.insert(std::make_pair(node, result));
   return result;
   }

/**
 * \brief Find the canonical Expr corresponding to \p node, if any.
 *
 * \p node The node whose Expr representation is requested
 * \return the pointer to the canonical Expr instance corresponding to \p node,
 * or null if no such Expr has been created
 */
const TR_LoopVersioner::Expr *TR_LoopVersioner::findCanonicalExpr(TR::Node *node)
   {
   auto cached = _curLoop->_nodeToExpr.find(node);
   if (cached != _curLoop->_nodeToExpr.end())
      return cached->second;

   TR::Node *defRHS = NULL;
   if (node->getOpCode().isLoadVarDirect()
       && node->getSymbol()->isAutoOrParm()
       && !isExprInvariant(node))
      defRHS = isDependentOnInvariant(node);

   const Expr *result = NULL;
   if (defRHS != NULL)
      {
      result = findCanonicalExpr(defRHS);
      if (result == NULL)
         return NULL;
      }
   else
      {
      Expr expr;
      bool onlySearching = true;
      if (!initExprFromNode(&expr, node, onlySearching))
         return NULL;

      for (int i = 0; i < node->getNumChildren(); i++)
         {
         const Expr *child = findCanonicalExpr(node->getChild(i));
         if (child == NULL)
            return NULL;

         expr._children[i] = child;
         }

      auto existing = _curLoop->_exprTable.find(expr);
      if (existing == _curLoop->_exprTable.end())
         return NULL;

      result = existing->second;
      }

   if (trace())
      {
      traceMsg(
         comp(),
         "findCanonicalExpr: Canonical n%un [%p] is expr %p\n",
         node->getGlobalIndex(),
         node,
         result);
      }

   _curLoop->_nodeToExpr.insert(std::make_pair(node, result));
   return result;
   }

/**
 * \brief Find occurrences of privatized expressions beneath \p node and
 * transmute them into loads of the temps into which the values have been
 * stored.
 *
 * Any child node removed from a parent as a result of this process is anchored
 * before \p tt.
 *
 * \p tt The tree where \p node (if it is unvisited) is evaluated
 * \p node The subtree within which to substitute loads of privatization temps
 * \p visited The set of nodes within which substitution has already been performed
 * \return the pointer to the canonical Expr instance corresponding to \p node,
 * or null if no such Expr has been created
 */
const TR_LoopVersioner::Expr *TR_LoopVersioner::substitutePrivTemps(
   TR::TreeTop *tt,
   TR::Node *node,
   TR::NodeChecklist *visited)
   {
   if (visited->contains(node))
      {
      auto cached = _curLoop->_nodeToExpr.find(node);
      if (cached == _curLoop->_nodeToExpr.end())
         return NULL;
      else
         return cached->second;
      }

   visited->add(node);

   TR::Node *defRHS = NULL;
   if (node->getOpCode().isLoadVarDirect()
       && node->getSymbol()->isAutoOrParm()
       && !isExprInvariant(node))
      defRHS = isDependentOnInvariant(node);

   const Expr *canonicalExpr = NULL;
   if (defRHS != NULL)
      {
      // Because tt isn't the point of evaluation of defRHS, use
      // findCanonicalExpr() instead of recursing here to ensure nodes beneath
      // defRHS are anchored correctly.
      canonicalExpr = findCanonicalExpr(defRHS);
      if (canonicalExpr == NULL)
         return NULL;
      }
   else
      {
      // Nodes whose opcodes satisfy isTreeTop() don't produce a value and
      // won't be privatized. Check isTreeTop() here to avoid passing them into
      // initExprFromNode() to exempt any conditional nodes encountered here
      // from the assertion on the branch target for versioning tests.
      Expr expr;
      bool onlySearching = true;
      bool rootMayBePrivatized =
         !node->getOpCode().isTreeTop()
         && initExprFromNode(&expr, node, onlySearching);

      for (int i = 0; i < node->getNumChildren(); i++)
         {
         const Expr *child = substitutePrivTemps(tt, node->getChild(i), visited);
         if (rootMayBePrivatized)
            {
            if (child == NULL)
               rootMayBePrivatized = false;
            else
               expr._children[i] = child;
            }
         }

      if (!rootMayBePrivatized)
         return NULL;

      auto exprEntry = _curLoop->_exprTable.find(expr);
      if (exprEntry == _curLoop->_exprTable.end())
         return NULL;

      canonicalExpr = exprEntry->second;
      }

   if (trace())
      {
      traceMsg(
         comp(),
         "substitutePrivTemps: Canonical n%un [%p] is expr %p\n",
         node->getGlobalIndex(),
         node,
         canonicalExpr);
      }

   auto insertResult =
      _curLoop->_nodeToExpr.insert(std::make_pair(node, canonicalExpr));

   TR_ASSERT_FATAL(
      insertResult.second || insertResult.first->second == canonicalExpr,
      "failed to insert n%un [%p] into _nodeToExpr",
      node->getGlobalIndex(),
      node);

   auto tempEntry = _curLoop->_privTemps.find(canonicalExpr);
   if (tempEntry == _curLoop->_privTemps.end())
      return canonicalExpr;

   // NB. Once the entry has been put into _privTemps, this transformation is
   // required.
   TR::SymbolReference *temp = tempEntry->second._symRef;
   dumpOptDetails(
      comp(),
      "Transforming n%un [%p] into a load of privatization temp #%d\n",
      node->getGlobalIndex(),
      node,
      temp->getReferenceNumber());

   anchorChildren(node, tt);
   node->removeAllChildren();
   node->setUseDefIndex(0);
   node->setFlags(0);

   TR::DataType type = tempEntry->second._type;
   if (type == TR::Int8 || type == TR::Int16)
      {
      TR::Node::recreate(node, type == TR::Int8 ? TR::i2b : TR::i2s);
      node->setNumChildren(1);
      node->setAndIncChild(0, TR::Node::createWithSymRef(node, TR::iload, 0, temp));
      }
   else
      {
      TR::ILOpCodes op = comp()->il.opCodeForDirectLoad(type);
      TR::Node::recreateWithSymRef(node, op, temp);
      }

   return canonicalExpr;
   }

/**
 * \brief Generate a node from \p expr.
 *
 * Repeated subexpressions will be commoned. If \p expr or any subexpression
 * has been privatized (i.e. if the privatization has been emitted), then the
 * outermost privatized subexpressions will be generated as loads of their
 * respective temps.
 *
 * \param expr The expression to emit
 * \return the generated node
 *
 * \see emitPrep()
 */
TR::Node *TR_LoopVersioner::emitExpr(const Expr *expr)
   {
   TR::Region emitExprRegion(comp()->trMemory()->currentStackRegion());
   EmitExprMemo memo(std::less<const Expr*>(), emitExprRegion);
   return emitExpr(expr, memo);
   }

/// Implementation of emitExpr(const Expr*)
TR::Node *TR_LoopVersioner::emitExpr(const Expr *expr, EmitExprMemo &memo)
   {
   auto existing = memo.find(expr);
   if (existing != memo.end())
      return existing->second;

   auto tempEntry = _curLoop->_privTemps.find(expr);
   if (tempEntry != _curLoop->_privTemps.end())
      {
      TR::SymbolReference *temp = tempEntry->second._symRef;
      TR::Node *node = TR::Node::createLoad(temp);
      node->setByteCodeInfo(expr->_bci);

      TR::DataType type = tempEntry->second._type;
      if (type == TR::Int8)
         node = TR::Node::create(node, TR::i2b, 1, node);
      else if (type == TR::Int16)
         node = TR::Node::create(node, TR::i2s, 1, node);

      if (trace())
         {
         traceMsg(
            comp(),
            "Emitted expr %p as privatized temp #%d load n%un [%p]\n",
            expr,
            temp->getReferenceNumber(),
            node->getGlobalIndex(),
            node);
         }

      memo.insert(std::make_pair(expr, node));
      return node;
      }

   int numChildren = 0;
   while (numChildren < Expr::MAX_CHILDREN && expr->_children[numChildren] != NULL)
      numChildren++;

   TR::Node *children[Expr::MAX_CHILDREN] = {0};
   for (int i = 0; i < numChildren; i++)
      children[i] = emitExpr(expr->_children[i], memo);

   TR::Node *node = NULL;
   TR::ILOpCode op = expr->_op;
   TR::ILOpCodes opVal = op.getOpCodeValue();
   if (!op.isLoadConst() && op.hasSymbolReference())
      {
      node = TR::Node::createWithSymRef(opVal, numChildren, expr->_symRef);
      setAndIncChildren(node, numChildren, children);
      }
   else if (op.isIf())
      {
      TR_ASSERT_FATAL(numChildren == 2, "expected if %p to have 2 children", expr);
      node = TR::Node::createif(opVal, children[0], children[1], _exitGotoTarget);
      if (expr->_guard != NULL)
         new (comp()->trHeapMemory()) TR_VirtualGuard(expr->_guard, node, comp());
      }
   else
      {
      node = TR::Node::create(opVal, numChildren);
      setAndIncChildren(node, numChildren, children);
      }

   if (op.isLoadConst())
      node->setConstValue(expr->_constValue);

   node->setByteCodeInfo(expr->_bci);
   node->setFlags(expr->_flags);

   if (trace())
      {
      traceMsg(
         comp(),
         "Emitted expr %p as n%un [%p]\n",
         expr,
         node->getGlobalIndex(),
         node);
      }

   memo.insert(std::make_pair(expr, node));
   return node;
   }

/**
 * \brief Generate a tree for \p prep and add it to \p comparisonTrees.
 *
 * If \p prep is a versioning test, its expression specifies the entire tree.
 *
 * If on the other hand it is a privatization, the expression specifies only
 * the value to be privatized, and the tree is a store of this value into a
 * fresh temp \c t. After emitting the privatization of an Expr \c e,
 * emitExpr(const Expr*) will generate a load of \c t instead of
 * rematerializing \c e, so that occurrences of \c e in later preparations will
 * all have the same value. Occurrences within the loop are substituted later.
 *
 * Preparations are emitted in post-order, so all of the dependencies of
 * \p prep are emitted before \p prep itself. No instance is emitted more than
 * once.
 *
 * **For debugging purposes only**, the \c TR_assumeSingleThreadedVersioning
 * environment variable can be set to cause this method to skip privatization.
 *
 * \param prep The preparation to emit
 * \param comparisonTrees The list of all versioning test and privatization
 * trees for the loop
 *
 * \see emitExpr(const Expr*)
 * \see substitutePrivTemps(const Expr*)
 */
void TR_LoopVersioner::emitPrep(LoopEntryPrep *prep, List<TR::Node> *comparisonTrees)
   {
   TR_ASSERT_FATAL(
      !prep->_requiresPrivatization || _curLoop->_privatizationOK,
      "should not be emitting prep %p because it requires privatization",
      prep);

   if (prep->_emitted)
      return;

   prep->_emitted = true;

   for (auto it = prep->_deps.begin(); it != prep->_deps.end(); ++it)
      emitPrep(*it, comparisonTrees);

   if (prep->_kind == LoopEntryPrep::TEST)
      {
      TR::Node *node = emitExpr(prep->_expr);
      comparisonTrees->add(node);
      dumpOptDetails(
         comp(),
         "Emitted prep %p as n%un [%p]\n",
         prep,
         node->getGlobalIndex(),
         node);
      }
   else
      {
      TR_ASSERT_FATAL(
         prep->_kind == LoopEntryPrep::PRIVATIZE,
         "prep %p has unrecognized kind %d\n",
         prep,
         prep->_kind);

      // If in the future it is desirable not to privatize within a
      // single-threaded system, the way to do it is to simply ignore
      // privatizations here. If privatizations are allowed, then there will be
      // no calls remaining in the loop, and the only possible writers will be
      // other threads (of which there are none). If OTOH privatizations are
      // not allowed, then any dependent LoopEntryPreps and LoopImprovements
      // must be skipped, just as in the multi-threaded case.
      //
      // TR_assumeSingleThreadedVersioning is only for debugging multi-threaded
      // systems such as OpenJ9. If/when a single-threaded system wishes to
      // stop privatization, some other mechanism should be used here to
      // determine whether we are doing threading.
      //
      static const bool singleThreaded =
         feGetEnv("TR_assumeSingleThreadedVersioning") != NULL;

      if (singleThreaded)
         return;

      // Actually privatize now.
      TR::Node *value = emitExpr(prep->_expr);
      TR::DataType nodeType = value->getDataType();

      // Privatization of internal pointers is unhandled. Existing internal
      // pointer temps are already autos, so they will not be privatized. For
      // performance we may hoist expressions containing pointer arithmetic
      // (aladd, aiadd), but not expressions that do pointer arithmetic at the
      // root, because those opcodes do not satisfy isSupportedForPRE().
      //
      // If in the future we want to store the results of pointer arithmetic,
      // it should be possible to do so by making the temp an internal pointer
      // temp with an associated pinning array temp, and setting both temps.
      //
      TR_ASSERT_FATAL(
         !value->isInternalPointer(),
         "prep %p attempting to privatize an internal pointer",
         prep);

      // OpenJ9 does not allow narrow integer-typed temps, requiring values to
      // be extended to 32-bit before storing them to the stack.
      TR::DataType tempType = nodeType;
      if (tempType == TR::Int8 || tempType == TR::Int16)
         tempType = TR::Int32;

      TR::SymbolReference *temp = comp()->getSymRefTab()->createTemporary(
         comp()->getMethodSymbol(),
         tempType);

      if (value->getDataType() == TR::Address && value->isNotCollected())
         temp->getSymbol()->setNotCollected();

      auto insertResult = _curLoop->_privTemps.insert(
         std::make_pair(prep->_expr, PrivTemp(temp, nodeType)));

      TR_ASSERT_FATAL(
         insertResult.second,
         "_privTemps insert failed for expr %p",
         prep->_expr);

      // Extend when nodeType is a narrow integer, since tempType is Int32.
      if (nodeType == TR::Int8)
         value = TR::Node::create(value, TR::b2i, 1, value);
      else if (nodeType == TR::Int16)
         value = TR::Node::create(value, TR::s2i, 1, value);

      TR::Node *store = TR::Node::createStore(value, temp, value);
      comparisonTrees->add(store);

      // Don't invalidate use-def info, etc. quite yet. It's still needed
      // during substitutePrivTemps() (for isDependentOnInvariant()). Use-def
      // info and value number info are always invalidated at the end of
      // versioner's optimization pass, but remember to invalidate alias sets
      // as well.
      _invalidateAliasSets = true;

      optimizer()->setRequestOptimization(OMR::globalValuePropagation, true);

      dumpOptDetails(
         comp(),
         "Emitted prep %p as n%un [%p] storing to temp #%d\n",
         prep,
         store->getGlobalIndex(),
         store,
         temp->getReferenceNumber());
      }
   }

/**
 * \brief Emit all versioning tests in \p preps and its elements' transitive
 * dependencies, with no privatization.
 *
 * \deprecated This is useful only for collectAllExpressionsToBeChecked().
 *
 * \param preps The list of preparations from which to emit versioning tests
 * \param comparisonTrees The list of all versioning test trees for this loop
 */
void TR_LoopVersioner::unsafelyEmitAllTests(
   const TR::list<LoopEntryPrep*, TR::Region&> &preps,
   List<TR::Node> *comparisonTrees)
   {
   for (auto it = preps.begin(); it != preps.end(); ++it)
      {
      LoopEntryPrep *prep = *it;
      if (prep->_unsafelyEmitted)
         continue;

      prep->_unsafelyEmitted = true;

      unsafelyEmitAllTests(prep->_deps, comparisonTrees);

      if (prep->_kind == LoopEntryPrep::TEST)
         {
         TR::Node *node = emitExpr(prep->_expr);
         comparisonTrees->add(node);

         dumpOptDetails(
            comp(),
            "Unsafely emitted prep %p as n%un [%p]\n",
            prep,
            node->getGlobalIndex(),
            node);

         if (!prep->_requiresPrivatization)
            {
            prep->_emitted = true;
            dumpOptDetails(
               comp(),
               "This prep happens to be safe (no privatization required)\n");
            }
         }
      }
   }

void TR_LoopVersioner::setAndIncChildren(TR::Node *node, int n, TR::Node **children)
   {
   for (int i = 0; i < n; i++)
      node->setAndIncChild(i, children[i]);
   }

/**
 * \brief Commit to removing \p node if \p prep can be emitted.
 *
 * This commitment is trusted by the analysis that determines whether
 * privatization is safe, so at this point the removal of \p node becomes
 * **mandatory** if it is at all possible.
 *
 * The node must be a root-level node, typically a check. If it's a conditional
 * branch, then the analysis will assume that it unconditionally falls through,
 * unless it is also present in CurLoop::_takenBranches, in which case the
 * analysis will assume that it is unconditionally taken.
 *
 * \param node The node to be removed
 * \param prep The preparation upon which the removal of \p node is predicated
 *
 * \see LoopBodySearch
 */
void TR_LoopVersioner::nodeWillBeRemovedIfPossible(
   TR::Node *node,
   LoopEntryPrep *prep)
   {
   TR::NodeChecklist *removableWithPriv = &_curLoop->_optimisticallyRemovableNodes;
   TR::NodeChecklist *removableWithoutPriv = &_curLoop->_definitelyRemovableNodes;
   if (prep->_expr->mergedWithHCRGuard())
      {
      removableWithPriv = &_curLoop->_guardsRemovableWithPrivAndHCR;
      removableWithoutPriv = &_curLoop->_guardsRemovableWithHCR;
      }
   else if (prep->_expr->mergedWithOSRGuard())
      {
      removableWithPriv = &_curLoop->_guardsRemovableWithPrivAndOSR;
      removableWithoutPriv = &_curLoop->_guardsRemovableWithOSR;
      }

   removableWithPriv->add(node);
   if (!prep->_requiresPrivatization)
      removableWithoutPriv->add(node);
   }

/**
 * \brief Compare two \ref Expr "Exprs" for ordering.
 *
 * Comparison ignores the "best-effort" fields Expr::_bci and Expr::_flags.
 *
 * \param lhs The first Expr to compare
 * \param rhs The second Expr to compare
 * \return true if \p lhs is less than \p rhs
 */
bool TR_LoopVersioner::Expr::operator<(const Expr &rhs) const
   {
   const Expr &lhs = *this;

   if ((int)lhs._op.getOpCodeValue() < (int)rhs._op.getOpCodeValue())
      return true;
   else if ((int)lhs._op.getOpCodeValue() > (int)rhs._op.getOpCodeValue())
      return false;

   if (lhs._op.isLoadConst())
      {
      if (lhs._constValue < rhs._constValue)
         return true;
      else if (lhs._constValue > rhs._constValue)
         return false;
      }
   else if (lhs._op.hasSymbolReference())
      {
      std::less<TR::SymbolReference*> symRefLt;
      if (symRefLt(lhs._symRef, rhs._symRef))
         return true;
      else if (symRefLt(rhs._symRef, lhs._symRef))
         return false;
      }
   else if (lhs._op.isIf() && lhs._guard != rhs._guard)
      {
      if (lhs._guard == NULL)
         return true; // Non-guards compare less than guards.
      else if (rhs._guard == NULL)
         return false;
      else if (lhs._guard->getKind() < rhs._guard->getKind())
         return true;
      else if (lhs._guard->getKind() > rhs._guard->getKind())
         return false;
      else if (lhs._guard->getTestType() < rhs._guard->getTestType())
         return true;
      else if (lhs._guard->getTestType() > rhs._guard->getTestType())
         return false;
      }

   if (lhs._mandatoryFlags.getValue() < rhs._mandatoryFlags.getValue())
      return true;
   else if (lhs._mandatoryFlags.getValue() > rhs._mandatoryFlags.getValue())
      return false;

   for (int i = 0; i < Expr::MAX_CHILDREN; i++)
      {
      std::less<const Expr*> exprPtrLt;
      if (exprPtrLt(lhs._children[i], rhs._children[i]))
         return true;
      else if (exprPtrLt(rhs._children[i], lhs._children[i]))
         return false;
      }

   return false;
   }

/**
 * \brief Create a new LoopEntryPrep.
 *
 * For versioning tests, \p node should be the entire conditional tree. For
 * privatizations, it should be just the node whose value is to be privatized.
 *
 * This process can fail, either because \p node is not representable as an
 * Expr, or because of a problem generating prerequisite safety tests, so
 * **callers must be prepared to abandon the desired transformation in case of
 * failure**.
 *
 * If a LoopEntryPrep has already been created with the same kind and with an
 * Expr that corresponds to \p node, then the existing instance is returned.
 *
 * \param kind The kind of LoopEntryPrep to create.
 * \param node The node to be converted into the Expr of the LoopEntryPrep.
 * \param visited The visited set. Do not specify outside of addLoopEntryPrepDep()
 * \param prev A previous preparation to depend on. Do not specify outside of
 * createChainedLoopEntryPrep().
 *
 * \return the (new or existing) LoopEntryPrep, or null on failure
 */
TR_LoopVersioner::LoopEntryPrep *TR_LoopVersioner::createLoopEntryPrep(
   LoopEntryPrep::Kind kind,
   TR::Node *node,
   TR::NodeChecklist *visited,
   LoopEntryPrep *prev)
   {
   bool optDetails =
      comp()->getOutFile() != NULL
      && (trace() || comp()->getOption(TR_TraceOptDetails));

   if (visited == NULL)
      {
      // This is the top-level call. Ensure that node's flags have been reset
      // for code motion.
      node->resetFlagsAndPropertiesForCodeMotion();
      }

   // Because node will no longer appear verbatim in the trees, print it to
   // the log so that other dumpOptDetails() messages that refer to its
   // descendants make sense.
   if (optDetails)
      {
      const char *kindName = kind == LoopEntryPrep::PRIVATIZE ? "PRIVATIZE" : "TEST";
      if (prev == NULL)
         dumpOptDetails(comp(), "Creating %s prep for tree:\n", kindName);
      else
         dumpOptDetails(comp(), "Creating %s prep for tree with prev=%p:\n", kindName, prev);

      if (visited == NULL)
         comp()->getDebug()->clearNodeChecklist();

      comp()->getDebug()->printWithFixedPrefix(
         comp()->getOutFile(),
         node,
         1,
         true,
         false,
         "\t\t");

      traceMsg(comp(), "\n");
      }

   const Expr *expr = makeCanonicalExpr(node);
   if (expr == NULL)
      return NULL;

   PrepKey key(kind, expr, prev);
   auto existing = _curLoop->_prepTable.find(key);
   if (existing != _curLoop->_prepTable.end())
      {
      if (visited != NULL)
         {
         // This is happening as part of depsForLoopEntryPrep(). Update the
         // visited set as though the subtree rooted at node were put through
         // the normal recursive processing, so that reusing the existing
         // LoopEntryPrep doesn't affect the outer dependency list.
         visitSubtree(node, visited);
         }

      dumpOptDetails(
         comp(),
         "Using existing prep %p for n%un [%p]\n",
         existing->second,
         node->getGlobalIndex(),
         node);

      return existing->second;
      }

   LoopEntryPrep *prep =
      new (_curLoop->_memRegion) LoopEntryPrep(kind, expr, _curLoop->_memRegion);

   bool ok = false;
   bool isPrivatization = kind == LoopEntryPrep::PRIVATIZE;
   bool canPrivatizeRootNode = !isPrivatization;
   if (isPrivatization)
      _curLoop->_privatizationsRequested = true;

   if (prev != NULL)
      prep->_deps.push_back(prev);

   if (visited != NULL)
      {
      ok = depsForLoopEntryPrep(node, &prep->_deps, visited, canPrivatizeRootNode);
      }
   else
      {
      TR::NodeChecklist myVisited(comp());
      ok = depsForLoopEntryPrep(node, &prep->_deps, &myVisited, canPrivatizeRootNode);
      }

   if (!ok)
      {
      dumpOptDetails(
         comp(),
         "Failed to create prep for n%un [%p]\n",
         node->getGlobalIndex(),
         node);

      return NULL;
      }

   // Even if this LoopEntryPrep is a PRIVATIZE, the privatization may not be
   // required per se. For instance, this may be an attempt to hoist an
   // expression that does not observe state that could be written by other
   // threads, e.g. (8 + 4 * i2l(k)), where k is a loop-invariant auto.
   //
   // Set _requiresPrivatization only when correctness actually relies on it.
   //
   prep->_requiresPrivatization = isPrivatization && requiresPrivatization(node);

   if (!prep->_requiresPrivatization)
      {
      for (auto it = prep->_deps.begin(); it != prep->_deps.end(); ++it)
         {
         if ((*it)->_requiresPrivatization)
            {
            prep->_requiresPrivatization = true;
            break;
            }
         }
      }

   if (optDetails)
      {
      dumpOptDetails(
         comp(),
         "Prep for n%un [%p] is prep %p %s expr %p%s, deps: ",
         node->getGlobalIndex(),
         node,
         prep,
         prep->_kind == LoopEntryPrep::PRIVATIZE ? "PRIVATIZE" : "TEST",
         prep->_expr,
         prep->_requiresPrivatization ? " (requires privatization)" : "");

      if (prep->_deps.empty())
         {
         traceMsg(comp(), "none\n");
         }
      else
         {
         auto it = prep->_deps.begin();
         traceMsg(comp(), "%p", *it);
         while (++it != prep->_deps.end())
            traceMsg(comp(), ", %p", *it);
         traceMsg(comp(), "\n");
         }
      }

   _curLoop->_prepTable.insert(std::make_pair(key, prep));
   return prep;
   }

/**
 * \brief Create a LoopEntryPrep that depends on \p prev.
 *
 * This can be used to create a series of multiple preparations all required
 * for a single transformation, e.g.
 *
 *     LoopEntryPrep *prep = createLoopEntryPrep(LoopEntryPrep::TEST, test0);
 *     prep = createChainedLoopEntryPrep(LoopEntryPrep::TEST, test1, prep);
 *     prep = createChainedLoopEntryPrep(LoopEntryPrep::TEST, test2, prep);
 *     if (prep != NULL) {
 *         // success
 *     }
 *
 * This method fails when \p prev is null, so that LoopEntryPrep creation
 * failure cascades, and callers need only check the final result.
 *
 * \param kind The kind of LoopEntryPrep to create.
 * \param node The node to be converted into the Expr of the LoopEntryPrep.
 * \param prev The previous preparation to depend on. If null, no new
 * preparation is created.
 *
 * \return the created LoopEntryPrep, or null on failure
 *
 * \see createLoopEntryPrep()
 */
TR_LoopVersioner::LoopEntryPrep *TR_LoopVersioner::createChainedLoopEntryPrep(
   LoopEntryPrep::Kind kind,
   TR::Node *node,
   LoopEntryPrep *prev)
   {
   if (prev == NULL)
      return NULL;

   return createLoopEntryPrep(kind, node, NULL, prev);
   }

bool TR_LoopVersioner::PrepKey::operator<(const PrepKey &rhs) const
   {
   const PrepKey &lhs = *this;

   if ((int)lhs._kind < (int)rhs._kind)
      return true;
   else if ((int)lhs._kind > (int)rhs._kind)
      return false;

   std::less<const Expr*> ltExpr;
   if (ltExpr(lhs._expr, rhs._expr))
      return true;
   else if (ltExpr(rhs._expr, lhs._expr))
      return false;

   std::less<LoopEntryPrep*> ltPrep;
   if (ltPrep(lhs._prev, rhs._prev))
      return true;
   else if (ltPrep(rhs._prev, lhs._prev))
      return false;

   return false;
   }

void TR_LoopVersioner::visitSubtree(TR::Node *node, TR::NodeChecklist *visited)
   {
   if (visited->contains(node))
      return;

   visited->add(node);
   for (int i = 0; i < node->getNumChildren(); i++)
      visitSubtree(node->getChild(i), visited);
   }

/**
 * \brief Construct this CurLoop.
 *
 * This should be done once for each loop considered by versioner.
 *
 * \param comp The compilation object
 * \param memRegion A memory region local to the consideration of this loop
 * \param loop The structure of the loop
 */
TR_LoopVersioner::CurLoop::CurLoop(
   TR::Compilation *comp,
   TR::Region &memRegion,
   TR_RegionStructure *loop)
   : _memRegion(memRegion)
   , _loop(loop)
   , _exprTable(std::less<Expr>(), memRegion)
   , _nodeToExpr(std::less<TR::Node*>(), memRegion)
   , _prepTable(std::less<PrepKey>(), memRegion)
   , _nullTestPreps(std::less<const Expr*>(), memRegion)
   , _boundCheckPrepsWithSpineChecks(std::less<TR::Node*>(), memRegion)
   , _definitelyRemovableNodes(comp)
   , _optimisticallyRemovableNodes(comp)
   , _guardsRemovableWithHCR(comp)
   , _guardsRemovableWithPrivAndHCR(comp)
   , _guardsRemovableWithOSR(comp)
   , _guardsRemovableWithPrivAndOSR(comp)
   , _takenBranches(comp)
   , _loopImprovements(memRegion)
   , _privTemps(std::less<const Expr*>(), memRegion)
   , _privatizationsRequested(false)
   , _privatizationOK(false)
   , _hcrGuardVersioningOK(false)
   , _osrGuardVersioningOK(false)
   , _foldConditionalInDuplicatedLoop(false)
   {}

/**
 * \brief Construct this LoopBodySearch and start iterating.
 *
 * The first tree will be the \c BBStart of the loop header.
 *
 * \param comp The current compilation
 * \param memRegion The region in which to allocate the search queue
 * \param loop The loop structure
 * \param removedNodes The set of nodes to ignore/assume removed
 * \param takenBranches The subset of \p removedNodes that should be assumed to
 * be taken if they are branches
 */
TR_LoopVersioner::LoopBodySearch::LoopBodySearch(
   TR::Compilation *comp,
   TR::Region &memRegion,
   TR_RegionStructure *loop,
   TR::NodeChecklist *removedNodes,
   TR::NodeChecklist *takenBranches)
   : _loop(loop)
   , _removedNodes(removedNodes)
   , _takenBranches(takenBranches)
   , _queue(memRegion)
   , _alreadyEnqueuedBlocks(comp)
   , _currentBlock(loop->getEntryBlock())
   , _currentTreeTop(_currentBlock->getEntry())
   , _blockHasExceptionPoint(false)
   {
   _alreadyEnqueuedBlocks.add(_currentBlock);
   }

/**
 * \brief Move to the next tree in the loop.
 *
 * The search must not have already terminated.
 */
void TR_LoopVersioner::LoopBodySearch::advance()
   {
   TR_ASSERT_FATAL(_currentTreeTop != NULL, "Search has already terminated");
   if (_currentTreeTop != _currentBlock->getExit())
      {
      _currentTreeTop = _currentTreeTop->getNextTreeTop();
      TR::Node *node = _currentTreeTop->getNode();
      if (!_removedNodes->contains(node) && node->canGCandExcept())
         _blockHasExceptionPoint = true;
      }
   else
      {
      enqueueReachableSuccessorsInLoop();

      // Move to the next block in the queue.
      if (!_queue.empty())
         {
         _currentBlock = _queue.front();
         _queue.pop_front();
         _currentTreeTop = _currentBlock->getEntry();
         _blockHasExceptionPoint = false;
         }
      else
         {
         _currentBlock = NULL;
         _currentTreeTop = NULL;
         }
      }
   }

void TR_LoopVersioner::LoopBodySearch::enqueueReachableSuccessorsInLoop()
   {
   TR::Node *lastNode = _currentBlock->getLastRealTreeTop()->getNode();
   if (!lastNode->getOpCode().isIf() || !isBranchConstant(lastNode))
      enqueueReachableSuccessorsInLoopFrom(_currentBlock->getSuccessors());
   else if (isConstantBranchTaken(lastNode))
      enqueueBlockIfInLoop(lastNode->getBranchDestination());
   else
      enqueueBlockIfInLoop(_currentBlock->getExit()->getNextTreeTop());

   if (_blockHasExceptionPoint)
      enqueueReachableSuccessorsInLoopFrom(_currentBlock->getExceptionSuccessors());
   }

bool TR_LoopVersioner::LoopBodySearch::isBranchConstant(TR::Node *ifNode)
   {
   if (_removedNodes->contains(ifNode))
      return true;

   // Detect branches that have already been transformed in the current pass of
   // versioner.
   TR::ILOpCodes op = ifNode->getOpCodeValue();
   if (op != TR::ificmpeq && op != TR::ificmpne)
      return false;

   TR::Node *lhs = ifNode->getChild(0);
   TR::Node *rhs = ifNode->getChild(1);
   return lhs->getOpCodeValue() == TR::iconst && rhs->getOpCodeValue() == TR::iconst;
   }

bool TR_LoopVersioner::LoopBodySearch::isConstantBranchTaken(TR::Node *ifNode)
   {
   TR_ASSERT_FATAL(
      isBranchConstant(ifNode),
      "unexpected branch n%un",
      ifNode->getGlobalIndex());

   if (_removedNodes->contains(ifNode))
      return _takenBranches->contains(ifNode);

   // Handle branches that have already been transformed in the current pass of
   // versioner.
   bool branchOnEqual = ifNode->getOpCodeValue() == TR::ificmpeq;
   TR::Node *lhs = ifNode->getChild(0);
   TR::Node *rhs = ifNode->getChild(1);
   return (lhs->getInt() == rhs->getInt()) == branchOnEqual;
   }

void TR_LoopVersioner::LoopBodySearch::enqueueReachableSuccessorsInLoopFrom(
   TR::CFGEdgeList &outgoingEdges)
   {
   for (auto it = outgoingEdges.begin(); it != outgoingEdges.end(); ++it)
      enqueueBlockIfInLoop((*it)->getTo()->asBlock());
   }

void TR_LoopVersioner::LoopBodySearch::enqueueBlockIfInLoop(TR::TreeTop *entry)
   {
   enqueueBlockIfInLoop(entry->getNode()->getBlock());
   }

void TR_LoopVersioner::LoopBodySearch::enqueueBlockIfInLoop(TR::Block *block)
   {
   if (!_alreadyEnqueuedBlocks.contains(block)
       && _loop->contains(block->getStructureOf()))
      {
      _queue.push_back(block);
      _alreadyEnqueuedBlocks.add(block);
      }
   }
