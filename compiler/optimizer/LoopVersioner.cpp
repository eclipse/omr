/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "optimizer/LoopVersioner.hpp"

#include <algorithm>                               // for std::max, etc
#include <stdint.h>                                // for int32_t, int64_t, etc
#include <stdio.h>                                 // for printf
#include <stdlib.h>                                // for atoi, atof
#include <string.h>                                // for NULL, memset, etc
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator
#include "codegen/FrontEnd.hpp"                    // for TR_FrontEnd, etc
#include "compile/Compilation.hpp"                 // for Compilation
#include "compile/Method.hpp"                      // for TR_Method
#include "compile/ResolvedMethod.hpp"              // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"                // for TR_VirtualGuard
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"             // for TR::Options, etc
#include "cs2/bitvectr.h"
#include "cs2/llistof.h"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"                     // for ObjectModel
#include "env/PersistentInfo.hpp"                  // for PersistentInfo
#include "env/StackMemoryRegion.hpp"               // for TR::StackMemoryRegion
#include "env/TRMemory.hpp"                        // for TR_Memory, etc
#include "env/jittypes.h"                          // for TR_ByteCodeInfo, etc
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                            // for Block, toBlock, etc
#include "il/DataTypes.hpp"                        // for DataTypes::Int32, etc
#include "il/ILOpCodes.hpp"                        // for ILOpCodes::iconst, etc
#include "il/ILOps.hpp"                            // for ILOpCode, etc
#include "il/Node.hpp"                             // for Node, etc
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                           // for Symbol
#include "il/SymbolReference.hpp"                  // for SymbolReference, etc
#include "il/TreeTop.hpp"                          // for TreeTop
#include "il/TreeTop_inlines.hpp"                  // for TreeTop::getNode, etc
#include "il/symbol/AutomaticSymbol.hpp"           // for AutomaticSymbol
#include "il/symbol/MethodSymbol.hpp"              // for MethodSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"              // for StaticSymbol
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/BitVector.hpp"                     // for TR_BitVector, etc
#include "infra/Cfg.hpp"                           // for CFG, etc
#include "infra/Link.hpp"                          // for TR_LinkHead, etc
#include "infra/List.hpp"                          // for List, ListElement, etc
#include "infra/CfgEdge.hpp"                       // for CFGEdge
#include "infra/CfgNode.hpp"                       // for CFGNode
#include "optimizer/Dominators.hpp"                // for TR_PostDominators
#include "optimizer/LocalAnalysis.hpp"             // for TR_LocalAnalysis
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/Optimization.hpp"              // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"                 // for Optimizer, etc
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/UseDefInfo.hpp"                // for TR_UseDefInfo, etc
#include "optimizer/ValueNumberInfo.hpp"
#include "optimizer/VPConstraint.hpp"              // for TR::VPConstraint
#include "ras/Debug.hpp"                           // for TR_DebugBase
#include "ras/DebugCounter.hpp"                    // for TR::DebugCounter, etc

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
   _checksInDupHeader(trMemory())
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

   return performWithoutDominators();
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
   bool aggressive = TR::Options::getCmdLineOptions()->getOption(TR_EnableAggressiveLoopVersioning);
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

      bool aggressive = TR::Options::getCmdLineOptions()->getOption(TR_EnableAggressiveLoopVersioning);

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
      TR_ScratchList<TR::TreeTop> iwrtbarTrees(trMemory());
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
         continue;

      _loopConditionInvariant = false;

      bool discontinue = false;
      bool   nullChecksMayBeEliminated = detectChecksToBeEliminated(naturalLoop, &nullCheckedReferences, &nullCheckTrees, &numIndirections,
                                                                  &boundCheckTrees, &spineCheckTrees, &numDimensions, &conditionalTrees, &divCheckTrees, &iwrtbarTrees,
                                                                  &checkCastTrees, &arrayStoreCheckTrees, &specializedInvariantNodes, invariantNodes,
                                                                  &invariantTranslationNodesList, discontinue);

      if (discontinue) continue;

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

      bool iwrtBarsWillBeEliminated = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         iwrtBarsWillBeEliminated = detectInvariantIwrtbars(&iwrtbarTrees);
      //else
      //   iwrtBarTrees.deleteAll();

      SharedSparseBitVector reverseBranchInLoops(comp()->allocator());
      bool containsNonInlineGuard = false;
      bool checkCastTreesWillBeEliminated = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         checkCastTreesWillBeEliminated = detectInvariantTrees(naturalLoop, &checkCastTrees, false, &containsNonInlineGuard, reverseBranchInLoops);
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
      containsNonInlineGuard = false;
      if (!shouldOnlySpecializeLoops() && !refineAliases())
         {
         // default hotness threshold
         TR_Hotness hotnessThreshold = hot;
         if (TR::Options::getCmdLineOptions()->getOption(TR_EnableAggressiveLoopVersioning))
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
             iwrtBarsWillBeEliminated  ||
             checkCastTreesWillBeEliminated ||
             arrayStoreCheckTreesWillBeEliminated ||
             specializedNodesWillBeEliminated ||
             invariantNodesWillBeEliminated ||
             _nonInlineGuardConditionalsWillNotBeEliminated ||
             _loopTransferDone)
            {
            conditionalsWillBeEliminated = detectInvariantTrees(naturalLoop, &conditionalTrees, true, &containsNonInlineGuard, reverseBranchInLoops);
            }
         else
            {
            conditionalsWillBeEliminated = detectInvariantTrees(naturalLoop, &conditionalTrees, false, &containsNonInlineGuard, reverseBranchInLoops);
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
           iwrtBarsWillBeEliminated  ||
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
         versionNaturalLoop(naturalLoop, &nullCheckedReferences, &nullCheckTrees, &boundCheckTrees, &spineCheckTrees, &conditionalTrees, &divCheckTrees, &iwrtbarTrees, &checkCastTrees, &arrayStoreCheckTrees, &specializedInvariantNodes, invariantNodes, &invariantTranslationNodesList, &whileLoops, &clonedInnerWhileLoops, skipAsyncCheckRemoval, reverseBranchInLoops);
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
            TR_RegisterCandidate *inductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(inductionVar);
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
               TR_RegisterCandidate *inductionCandidate = comp()->getGlobalRegisterCandidates()->findOrCreate(inductionSymRef);
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
      }

   ////if (!_virtualGuardPairs.isEmpty())
   if (!_virtualGuardInfo.isEmpty())
      {
      bool disableLT = TR::Options::getCmdLineOptions()->getOption(TR_DisableLoopTransfer);
      if (!disableLT)
         performLoopTransfer();
      }

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

   return 1; // actual cost
   }


void TR_LoopVersioner::performLoopTransfer()
   {
   // Only do the transfer if there are no unguarded calls left
   // in the loop (should be checked here)
   //
   ////if (trace())
   ////   traceMsg(comp(), "Loop transfer in %s with size %d\n", comp()->signature(), _virtualGuardInfo.getSize());

   dumpOptDetails(comp(), "Loop transfer in %s with size %d\n", comp()->signature(), _virtualGuardInfo.getSize());

   TR_ScratchList<TR::Node> processedVirtualGuards(trMemory());
   TR::CFG *cfg = comp()->getFlowGraph();
   VirtualGuardInfo *vgInfo = NULL;
   bool somethingChanged = false;
   ///int32_t count = 0;
   for (vgInfo = _virtualGuardInfo.getFirst(); vgInfo; vgInfo = vgInfo->getNext())
      {
      ListIterator<VirtualGuardPair> guardsIt(&vgInfo->_virtualGuardPairs);
      VirtualGuardPair *nextVirtualGuard;
      int32_t changedTargets = 0;
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
            bool useOldLoopTransfer = comp()->getOption(TR_DisableNewLoopTransfer);
            //FIXME: disable the new loop transfer
	    if (!useOldLoopTransfer)
               {
               bool doIt = true;
               if (doIt)
                  {
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
            else
               {
               bool doIt = true;

               if (doIt && performTransformation(comp(), "%sLoop transfer for guard %p in loop %d\n", OPT_DETAILS_LOOP_VERSIONER, hotGuard, vgInfo->_loopEntry->getNumber()))
                  {
                  comp()->setLoopTransferDone();
                  ///printf("Loop transfer in %s at %s\n", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()));
                  // prevent processing of a guard more than once
                  // (could happen if the guard is in a deeply nested loop
                  //
                  if (!processedVirtualGuards.find(hotGuard))
                     {
                     nextVirtualGuard->_isGuarded = true;
                     processedVirtualGuards.add(hotGuard);
                     changedTargets++;
                     }
                  }
               }
            ///printf("Loop transfer in %s\n", comp()->signature());
            }
         }

      if (changedTargets > 0)
         {
         // set up loop transfer
         if (trace())
            comp()->dumpMethodTrees("trees before loop transfer\n");
         fixupVirtualGuardTargets(vgInfo);
         somethingChanged = true;
         }
      }
   if (somethingChanged)
      {
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      optimizer()->setAliasSetsAreValid(false);
      }
   }

static TR::Block *findBlockInCFG(TR::CFG *cfg, int32_t num)
   {
   for (TR::CFGNode *node = cfg->getFirstNode(); node; node = node->getNext())
      {
      TR::Block *b = toBlock(node);
      if (b->getNumber() == num)
         return b;
      }
   return NULL;
   }

void TR_LoopVersioner::fixupVirtualGuardTargets(VirtualGuardInfo *vgInfo)
   {
   // Find the last treetop in the method
   //
   TR::TreeTop *treeTop, *endTree = NULL;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      treeTop = treeTop->getNode()->getBlock()->getExit();
      endTree = treeTop;
      }

   int32_t numNodes = comp()->getFlowGraph()->getNextNodeNumber();
   VGLoopInfo **distinctTargets = (VGLoopInfo **)trMemory()->allocateStackMemory(numNodes*sizeof(VGLoopInfo *));
   memset(distinctTargets, 0, numNodes*sizeof(VGLoopInfo *));

   // container for innerloop selectors
   //
   TR_LinkHead<LoopTemps> selectorTemps;
   selectorTemps.setFirst(NULL);

   TR_Structure *rootStructure = _cfg->getStructure();
   _cfg->setStructure(NULL);
   // create the switch block by cloning
   // the cold loop header and fixing up the relevant edges
   //
   TR::Block *switchBlock = vgInfo->_coldLoopEntryBlock;
   TR::Block *clonedHeader = createClonedHeader(switchBlock, &endTree);

   // rip out the trees
   //
   switchBlock->getEntry()->join(switchBlock->getExit());

   ListIterator<VirtualGuardPair> vgIt(&vgInfo->_virtualGuardPairs);
   VirtualGuardPair *nextVG;
   int32_t caseKonst = 0;
   bool innerLoop = false;
   for (nextVG = vgIt.getFirst(); nextVG; nextVG = vgIt.getNext())
      {
      if (nextVG->_isGuarded)
         {
         if (nextVG->_isInsideInnerLoop)
            {
            int32_t num = nextVG->_coldGuardLoopEntryBlock->getNumber();

            if (!distinctTargets[num])
               {
               distinctTargets[num] = (VGLoopInfo *) trMemory()->allocateStackMemory(sizeof(VGLoopInfo));
               memset(distinctTargets[num], 0, sizeof(VGLoopInfo));
               }
            distinctTargets[num]->_count++;
            }
         else
            caseKonst++;
         if (nextVG->_coldGuardBlock == switchBlock)
            nextVG->_coldGuardBlock = clonedHeader;
         }
      }

   // create the temp symref to hold a distinct value
   // on the failing path of each virtual guard
   //
   TR::SymbolReference *vgTempSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);

   int32_t numDistinct = 0;
   int32_t i;
   for (i = 0; i < numNodes; i++)
      if (distinctTargets[i] && distinctTargets[i]->_count != 0)
         {
         // locate the invariant block for the inner loop containing
         // the coldGuard
         //
         TR::Block *coldGuardLoopEntry = findBlockInCFG(_cfg, i);
         TR::Block *invariantBlock = distinctTargets[i]->_coldGuardLoopInvariantBlock;
         if (!invariantBlock)
            {
            _cfg->setStructure(rootStructure);
            for (auto e = coldGuardLoopEntry->getPredecessors().begin(); e != coldGuardLoopEntry->getPredecessors().end(); ++e)
               {
               TR::Block *from = toBlock((*e)->getFrom());
               if (from == clonedHeader)
                  continue;
               if (from->getStructureOf()->isLoopInvariantBlock())
                  {
                  invariantBlock = from;
                  break;
                  }
               }
            ////TR_ASSERT(invariantBlock, "Invariant block in loop is NULL\n");
            if (!invariantBlock)
               {
               dumpOptDetails(comp(), "invariant block in loop [%d] is NULL\n", coldGuardLoopEntry->getNumber());
               invariantBlock = createEmptyGoto(NULL, coldGuardLoopEntry, endTree);
               endTree = invariantBlock->getExit();
               _cfg->addNode(invariantBlock);
               // fixup the predecessor edges
               //
               TR_ScratchList<TR::CFGEdge> removedEdges(trMemory());
               for (auto e = coldGuardLoopEntry->getPredecessors().begin(); e != coldGuardLoopEntry->getPredecessors().end(); ++e)
                  {
                  TR::Block *from = toBlock((*e)->getFrom());
                  // ignore backedges
                  //
                  if (from == clonedHeader)
                     continue;
                  TR_Structure *blockStructure = from->getStructureOf();
                  TR_RegionStructure *region = coldGuardLoopEntry->getStructureOf()->getParent()->asRegion();
                  if (region->contains(blockStructure, region->getParent()))
                     continue;

                  removedEdges.add(*e);
                  }

               _cfg->setStructure(NULL);
               _cfg->addEdge(TR::CFGEdge::createEdge(invariantBlock,  coldGuardLoopEntry, trMemory()));

               // now remove the edges
               //
               ListIterator<TR::CFGEdge> eIt;
               eIt.set(&removedEdges);
               for (TR::CFGEdge* e = eIt.getFirst(); e; e = eIt.getNext())
                  {
                  TR::Block *from = toBlock(e->getFrom());

                  TR::Node *branchNode = from->getLastRealTreeTop()->getNode();
                  if (from->getNextBlock() == coldGuardLoopEntry)
                     {
                     if (branchNode->getOpCode().isBranch())
                        {
                        // last tree ends in branch, fix destination
                        //
                        if (branchNode->getBranchDestination()->getNode()->getBlock() == coldGuardLoopEntry)
                           {
                           branchNode->setBranchDestination(invariantBlock->getEntry());
                           _cfg->addEdge(TR::CFGEdge::createEdge(from,  invariantBlock, trMemory()));
                           }
                        else
                           {
                           TR::Block *gotoBlock = createEmptyGoto(NULL, invariantBlock, NULL);
                           TR::TreeTop *fromExitTT = from->getExit();
                           TR::TreeTop *nextTT = fromExitTT->getNextTreeTop();
                           fromExitTT->join(gotoBlock->getEntry());
                           gotoBlock->getExit()->join(nextTT);
                           _cfg->addNode(gotoBlock);
                           _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  invariantBlock, trMemory()));
                           _cfg->addEdge(TR::CFGEdge::createEdge(from,  gotoBlock, trMemory()));
                           dumpOptDetails(comp(), "created goto block_%d to invariant block\n", gotoBlock->getNumber());
                           }
                        }
                     else
                        {
                        TR::Node *gotoNode = TR::Node::create(branchNode, TR::Goto, 0, invariantBlock->getEntry());
                        TR::TreeTop *gotoTT = TR::TreeTop::create(comp(), gotoNode);
                        from->append(gotoTT);
                        _cfg->addEdge(TR::CFGEdge::createEdge(from,  invariantBlock, trMemory()));
                        }
                     }
                  else
                     {
                     // last tree ends in branch, fix destination
                     //
                     branchNode->setBranchDestination(invariantBlock->getEntry());
                     _cfg->addEdge(TR::CFGEdge::createEdge(from,  invariantBlock, trMemory()));
                     }

                  _cfg->removeEdge(e);
                  }

               dumpOptDetails(comp(), "created invariant block [%d] for loop [%d]\n", invariantBlock->getNumber(), coldGuardLoopEntry->getNumber());
               }
            _cfg->setStructure(NULL);
            distinctTargets[i]->_coldGuardLoopInvariantBlock = invariantBlock;
            }

         TR::Node *node = coldGuardLoopEntry->getFirstRealTreeTop()->getNode();

         TR::SymbolReference *loopTempSymRef = distinctTargets[i]->_loopTempSymRef;
         if (!loopTempSymRef)
            {
            loopTempSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
            distinctTargets[i]->_loopTempSymRef = loopTempSymRef;
            // reset the selector
            //
            clonedHeader->prepend(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::istore, 1, 1,
                                                     TR::Node::create(node, TR::iconst, 0, distinctTargets[i]->_count), loopTempSymRef)));
            // record the temp and count so that
            // the temp is properly initialized on each
            // failing path of a virtual guard
            //
            LoopTemps *tempInfo = new (trStackMemory()) LoopTemps();
            tempInfo->_symRef = loopTempSymRef;
            tempInfo->_maxValue = distinctTargets[i]->_count;
            selectorTemps.add(tempInfo);
            }

         TR::Node *loadTemp = TR::Node::createWithSymRef(node, TR::iload, 0, loopTempSymRef);
         TR::Node *subNode = TR::Node::create(TR::isub, 2,
                                                    TR::Node::create(node, TR::iconst, 0, distinctTargets[i]->_count), loadTemp);
         TR::Node *storeTemp = TR::Node::createWithSymRef(TR::istore, 1, 1, subNode, loopTempSymRef);
         invariantBlock->prepend(TR::TreeTop::create(comp(), storeTemp));

         TR::Block *innerSwitchBlock = coldGuardLoopEntry;
         TR::Block *cloneBlock = createClonedHeader(innerSwitchBlock, &endTree);
         TR::Node *innerSwitchNode = createSwitchNode(cloneBlock, loopTempSymRef, distinctTargets[i]->_count);
         innerSwitchBlock->getEntry()->join(innerSwitchBlock->getExit());
         innerSwitchBlock->prepend(TR::TreeTop::create(comp(), innerSwitchNode));
         numDistinct++;
         distinctTargets[i]->_targetNum = -1;
         // reset the temp on entry to the loop
         //
         cloneBlock->prepend(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::istore, 1, 1,
                                                TR::Node::create(innerSwitchNode, TR::iconst, 0, 0), loopTempSymRef)));
         for (nextVG = vgIt.getFirst(); nextVG; nextVG = vgIt.getNext())
            {
            if (nextVG->_isGuarded)
               {
               if (nextVG->_coldGuardBlock == innerSwitchBlock)
                  nextVG->_coldGuardBlock = cloneBlock;
               }
            }
         }

   TR::Node *switchNode = createSwitchNode(clonedHeader, vgTempSymRef, caseKonst+numDistinct);
   switchBlock->prepend(TR::TreeTop::create(comp(), switchNode));

   // store the default value of the
   // temp in the loop invariant block
   //
   TR::Node *defaultStore = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                           TR::Node::create(switchNode, TR::iconst, 0, 0), vgTempSymRef);
   vgInfo->_coldLoopInvariantBlock->prepend(TR::TreeTop::create(comp(), defaultStore));

   // reset the temp on entry to the loop
   //
   clonedHeader->prepend(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::istore, 1, 1,
                                                                    TR::Node::create(switchNode, TR::iconst, 0, 0), vgTempSymRef)));


   vgIt.reset();
   for (nextVG = vgIt.getFirst(), caseKonst = 0; nextVG; nextVG = vgIt.getNext())
      {
      ///getDebug()->printIRTrees(comp()->getOutFile(), "after processing IR trees:", comp()->getMethodSymbol());

      if (nextVG->_isGuarded)
         {
         TR::Block *hotGuardBlock = nextVG->_hotGuardBlock;
         TR::Block *coldGuardBlock = nextVG->_coldGuardBlock;

         if (!findBlockInCFG(_cfg, hotGuardBlock->getNumber()))
            continue;

         // prepare to change the branch destination of the hot guard
         //
         TR::Node *hotGuardNode = hotGuardBlock->getLastRealTreeTop()->getNode();
         TR::Node *coldGuardNode = coldGuardBlock->getLastRealTreeTop()->getNode();
         TR::Block *oldDestBlock = hotGuardNode->getBranchDestination()->getNode()->getBlock();
         TR::CFGEdge *oldDestEdge = hotGuardBlock->getEdge(oldDestBlock);

         // create the gotoblock and store value into the temp
         //
         TR::Block *gotoBlock = createEmptyGoto(hotGuardBlock, switchBlock, endTree);
         endTree = gotoBlock->getExit();
         _cfg->addNode(gotoBlock);
         _cfg->addEdge(TR::CFGEdge::createEdge(hotGuardBlock,  gotoBlock, trMemory()));
         _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  switchBlock, trMemory()));

         hotGuardNode->setBranchDestination(gotoBlock->getEntry());
         _cfg->removeEdge(oldDestEdge);

         // fixup the inner switch block if required
         //
         TR::Node *storeTemp = NULL;
         TR::TreeTop *branchDest = coldGuardNode->getBranchDestination();

         if (nextVG->_isInsideInnerLoop)
            {
            int32_t loopNum = nextVG->_coldGuardLoopEntryBlock->getNumber();
            if (distinctTargets[loopNum]->_targetNum < 0)
               distinctTargets[loopNum]->_targetNum = ++caseKonst;
            storeTemp = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                  TR::Node::create(hotGuardNode, TR::iconst, 0, distinctTargets[loopNum]->_targetNum), vgTempSymRef);
            int32_t vgNum = --(distinctTargets[loopNum]->_count);
            TR::Node *vgNumTemp = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                 TR::Node::create(hotGuardNode, TR::iconst, 0, vgNum), distinctTargets[loopNum]->_loopTempSymRef);

            // reset any other temps that might have been created
            //
            for (LoopTemps *l = selectorTemps.getFirst(); l; l = l->getNext())
               {
               if (l->_symRef == distinctTargets[loopNum]->_loopTempSymRef)
                  continue;
               TR::Node *resetNode = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                                    TR::Node::create(hotGuardNode, TR::iconst, 0, l->_maxValue),
                                                    l->_symRef);
               gotoBlock->prepend(TR::TreeTop::create(comp(), resetNode));
               }

            // properly initialize this temp along the path
            //
            gotoBlock->prepend(TR::TreeTop::create(comp(), vgNumTemp));


            TR::Node *innerSwitchNode = nextVG->_coldGuardLoopEntryBlock->getFirstRealTreeTop()->getNode();
            int32_t indx = innerSwitchNode->getCaseIndexUpperBound() - vgNum;
            TR::Node *innerCaseNode = innerSwitchNode->getChild(indx-1);
            innerCaseNode->setCaseConstant(indx-2);
            if (coldGuardBlock == nextVG->_coldGuardLoopEntryBlock)
               {
               coldGuardBlock = nextVG->_coldGuardLoopEntryBlock->getNextBlock();
               coldGuardNode = coldGuardBlock->getLastRealTreeTop()->getNode();
               }
            // fix the targets of the switch node
            // iff they point to the default
            //
            TR::TreeTop *defaultTT = innerSwitchNode->getSecondChild()->getBranchDestination();
            if (innerCaseNode->getBranchDestination() == defaultTT)
               innerCaseNode->setBranchDestination(coldGuardNode->getBranchDestination());
            TR::Block *callBlock = coldGuardNode->getBranchDestination()->getNode()->getBlock();

            bool addIt = true;
            for (auto e = nextVG->_coldGuardLoopEntryBlock->getSuccessors().begin(); e != nextVG->_coldGuardLoopEntryBlock->getSuccessors().end()
                                ; ++e)
               {
               if ((*e)->getTo() == callBlock)
                  {
                  addIt = false;
                  break;
                  }
               }

            if (addIt)
               _cfg->addEdge(TR::CFGEdge::createEdge(nextVG->_coldGuardLoopEntryBlock,  callBlock, trMemory()));

            branchDest = distinctTargets[loopNum]->_coldGuardLoopInvariantBlock->getEntry();
            //reset the lookup flag
            //
            TR::Node *resetTemp = vgNumTemp->duplicateTree();
            resetTemp->getFirstChild()->setInt(0);
            callBlock->prepend(TR::TreeTop::create(comp(), resetTemp));
            }
         else
            {
            storeTemp = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                        TR::Node::create(hotGuardNode, TR::iconst, 0, ++caseKonst), vgTempSymRef);

            // reset any other temps that might have been created
            //
            for (LoopTemps *l = selectorTemps.getFirst(); l; l = l->getNext())
               {
               TR::Node *resetNode = TR::Node::createWithSymRef(TR::istore, 1, 1,
                                                    TR::Node::create(hotGuardNode, TR::iconst, 0, l->_maxValue),
                                                    l->_symRef);
               gotoBlock->prepend(TR::TreeTop::create(comp(), resetNode));
               }
            }

         gotoBlock->prepend(TR::TreeTop::create(comp(), storeTemp));

         // reset the lookup flag in the block
         // containing the call
         //
         TR::Block *callBlock = coldGuardNode->getBranchDestination()->getNode()->getBlock();
         TR::Node *resetTemp = storeTemp->duplicateTree();
         resetTemp->getFirstChild()->setInt(0);
         callBlock->prepend(TR::TreeTop::create(comp(), resetTemp));

         // fixup the targets of the switch and add edges
         //
         TR::Node *caseNode = switchNode->getChild(caseKonst+1);
         if (caseNode->getBranchDestination() == clonedHeader->getEntry())
            caseNode->setBranchDestination(branchDest);

         bool addIt = true;
         TR::Block *tgtBlock = branchDest->getNode()->getBlock();
         for (auto e = switchBlock->getSuccessors().begin(); e != switchBlock->getSuccessors().end(); ++e)
            {
            if ((*e)->getTo() == tgtBlock)
               {
               addIt = false;
               break;
               }
            }

         if (addIt)
            _cfg->addEdge(TR::CFGEdge::createEdge(switchBlock,  tgtBlock, trMemory()));
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
          (node->getData()->getSymbolReference()->getSymbol()->isAuto() &&
          isDependentOnInvariant(node->getData()) ||
          node->getData()->getOpCode().isLoadIndirect() &&
          !_seenDefinedSymbolReferences->get(node->getData()->getSymbolReference()->getReferenceNumber()) &&
          node->getData()->getFirstChild()->getOpCode().hasSymbolReference() &&
          node->getData()->getFirstChild()->getSymbolReference()->getSymbol()->isAuto() &&
          isDependentOnInvariant(node->getData()->getFirstChild())))
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
         vcount_t visitCount;
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

            if (inlinedCallIndex < comp()->getNumInlinedCallSites())
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


bool TR_LoopVersioner::detectInvariantTrees(TR_RegionStructure *whileLoop, List<TR::TreeTop> *trees, bool onlyDetectHighlyBiasedBranches, bool *containsNonInlineGuard, SharedSparseBitVector &reverseBranchInLoops)
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

      bool highlyBiasedBranch = false;
      //static char *enableHBB = feGetEnv("TR_enableHighlyBiasedBranch");
      if (node->getOpCode().isIf() && !node->isTheVirtualGuardForAGuardedInlinedCall())
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
      else if (!highlyBiasedBranch && comp()->hasIntStreamForEach() && node->getOpCode().isIf())
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

         if (onlyDetectHighlyBiasedBranches && (thisChild || nextRealNode) &&
             node->isTheVirtualGuardForAGuardedInlinedCall() && !node->isHCRGuard())
            {
            if (!guardInfo)
               guardInfo = comp()->findVirtualGuardInfo(node);
            //TR::Node *thisChild = NULL;
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
                  //if (node->isNonoverriddenGuard())
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
                  /*
                  else
                     {
                     visitCount = comp()->incVisitCount();
                     for (int32_t childNum=0;childNum < node->getNumChildren(); childNum++)
                        {
                        if (!isExprInvariant(node->getChild(childNum), visitCount))
                          {
                          isTreeInvariant = false;
                          break;
                          }
                        }
                     }
                  */
                  }
               }
            }
         else if (!node->isHCRGuard())
            {
            for (int32_t childNum=0;childNum < node->getNumChildren(); childNum++)
               {
               if (!isExprInvariant(node->getChild(childNum)))
                  {
                  if(node->isVersionableIfWithMaxExpr() || node->isVersionableIfWithMinExpr())
                     {
                     TR::Node *child = node->getChild(childNum);

                     while (child->getOpCode().isAdd() || child->getOpCode().isSub() || child->getOpCode().isMul())
                        {
                        if (child->getSecondChild()->getOpCode().isLoadConst())
                           child = child->getFirstChild();
                        else
                           {
                           bool isSecondChildInvariant = isExprInvariant(child->getSecondChild());
                           if (isSecondChildInvariant)
                              child = child->getFirstChild();
                           else
                              {
                              bool isFirstChildInvariant = isExprInvariant(child->getFirstChild());
                              if (isFirstChildInvariant)
                                 child = child->getSecondChild();
                              else
                                 {
                                 child= NULL;
                                 break;
                                 }
                              }
                           }
                        }

                     if(!child || !child->getOpCode().isLoadVarDirect())
                        {
                        isTreeInvariant = false;
                        break;
                        }
                     else
                        {
                        TR_InductionVariable *v;
                        bool isInductionVar=false;
                        int32_t symRefNum = child->getSymbolReference()->getReferenceNumber();
                        ListElement<int32_t> *versionableInductionVar = _versionableInductionVariables.getListHead();
                        while (versionableInductionVar)
                           {
                           dumpOptDetails(comp(), "Versionable induction var in if %d\n", *(versionableInductionVar->getData()));
                           if (symRefNum == *(versionableInductionVar->getData()))
                              {
                              isInductionVar = true;
                              break;
                              }
                           versionableInductionVar = versionableInductionVar->getNextElement();
                           }

                        if(!isInductionVar) //&& !isExprInvariant(child,comp()->incVisitCount()))
                           {
                           isTreeInvariant = false;
                           break;
                           }
                        }
                     }
                  else
                     {
                     isTreeInvariant = false;
                     break;
                     }
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
   if(cursor<useDefInfo->getFirstRealDefIndex())
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
   bool &indVarOccursAsSecondChildOfSub)
   {
   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   if (!useDefInfo)
      return NULL;

   uint16_t useIndex = useNode->getUseDefIndex();
   if (!useIndex || !useDefInfo->isUseIndex(useIndex))
      return NULL;

   TR_UseDefInfo::BitVector defs(comp()->allocator());
   if (useDefInfo->getUseDef(defs, useIndex) &&
       (defs.PopulationCount() == 1) &&
       _writtenExactlyOnce.ValueAt(useNode->getSymbolReference()->getReferenceNumber()))
      {
      TR_UseDefInfo::BitVector::Cursor cursor(defs);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t defIndex = cursor;
         if (defIndex < useDefInfo->getFirstRealDefIndex())
            return NULL;

         TR::Node *defNode = useDefInfo->getNode(defIndex);
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
            return child;
         }
      }

   return NULL;
   }

bool TR_LoopVersioner::isVersionableArrayAccess(TR::Node * indexChild)
   {

   bool mulNodeFound=false;
   bool addNodeFound=false;
   bool goodAccess=true;
   vcount_t visitCount;
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
      TR::Node *node = nextTree->getData()->getNode();

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
      //dumpOptDetails(comp(), "maxBound %p (invariant %d) for BNDCHK node %p loop condition invariant %d\n", node->getFirstChild(), isMaxBoundInvariant, node, _loopConditionInvariant);
      bool isLoopDrivingInductionVariable = false;
      bool isDerivedInductionVariable = false;
      if (isMaxBoundInvariant)
         {
         indexChild =
            (node->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? node->getChild(3) : node->getSecondChild();

         isIndexInvariant = isExprInvariant(indexChild);
         if (!isIndexInvariant &&
             _loopConditionInvariant &&
             (node->getOpCodeValue() == TR::BNDCHK || node->getOpCodeValue() == TR::BNDCHKwithSpineCHK))
            {
            if (indexChild->getOpCode().hasSymbolReference())
               indexSymRef = indexChild->getSymbolReference();
            else
               {
               bool goodAccess = isVersionableArrayAccess(indexChild);
               while (indexChild->getOpCode().isAdd() || indexChild->getOpCode().isSub() || indexChild->getOpCode().isAnd() ||
                      indexChild->getOpCode().isMul())
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


                 if (!isInductionVariable /* &&
                    !strncmp(comp()->signature(), "banshee/scan/encoding/Latin1EncodingSupport.load", 48) */)
                  {
                  if (_loopTestTree &&
                      (_loopTestTree->getNode()->getNumChildren() > 1) &&
                      ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmplt) ||
                       (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpgt) ||
                       (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpge) ||
                       (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmple)))
                     {
                     TR::Symbol *symbolInCompare = NULL;
                     TR::Node *childInCompare = _loopTestTree->getNode()->getFirstChild();
                     while (childInCompare->getOpCode().isAdd() || childInCompare->getOpCode().isSub())
                        {
                        if (childInCompare->getSecondChild()->getOpCode().isLoadConst())
                           childInCompare = childInCompare->getFirstChild();
                        else
                           break;
                        }


                   if (childInCompare->getOpCode().hasSymbolReference())
                      {
                      symbolInCompare = childInCompare->getSymbolReference()->getSymbol();
                      if (!symbolInCompare->isAutoOrParm())
                         symbolInCompare = NULL;
                      }

                     if (symbolInCompare)
                        {
                        TR_InductionVariable *v;
                        for (v = _currentNaturalLoop->getFirstInductionVariable(); v; v = v->getNext())
                           {
                           if ((v->getLocal() == indexSymRef->getSymbol()) &&
                               (v->getLocal() == symbolInCompare) &&
                               (v->getLocal()->getDataType() == TR::Int32) &&
                               (v->isSigned()))
                              {
                              if ((v->getIncr()->getLowInt() == v->getIncr()->getHighInt()) &&
                                  (v->getIncr()->getLowInt() > 0))
                                 _additionInfo->set(symRefNum);
                              isLoopDrivingInductionVariable = true;
                              isInductionVariable = true;
                              break;
                              }
                           }
                        }
                     }
                  }

               if (!isInductionVariable)
                  {
                  bool isIndexChildMultiplied=false;
                  TR::Node *mulNode=NULL;
                  TR::Node *strideNode=NULL;
                  bool indVarOccursAsSecondChildOfSub=false;
                  indexChild = isDependentOnInductionVariable(
                     indexChild,
                     changedIndexSymRefAtSomePoint,
                     isIndexChildMultiplied,
                     mulNode,
                     strideNode,
                     indVarOccursAsSecondChildOfSub);
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
                     }
                  }
               }
            }
         }


      if (isInductionVariable && indexSymRef)
         {
         if (!boundCheckUsesUnchangedValue(nextTree->getData(), indexChild, indexSymRef, _currentNaturalLoop))
            {
            //isInductionVariable = false;
            }
         else
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
         _checksInDupHeader.find(nextTree->getData()))
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


bool TR_LoopVersioner::detectInvariantIwrtbars(List<TR::TreeTop> *iwrtbarTrees)
   {

   if (!iwrtbarTrees->getListHead())
      return false;

#ifdef J9_PROJECT_SPECIFIC
   if (comp()->getOptions()->isVariableHeapBaseForBarrierRange0())
      {
      iwrtbarTrees->deleteAll();
      return false;
      }

   uintptrj_t nurseryBase, nurseryTop;
   comp()->fej9()->getNurserySpaceBounds(&nurseryBase, &nurseryTop);
   //printf("nursery base %p nursery top %p\n", nurseryBase, nurseryTop);
   if ((nurseryBase == 0) || (nurseryTop == 0))
      {
      iwrtbarTrees->deleteAll();
      return false;
      }

   uintptrj_t stackCompareValue = comp()->getOptions()->getHeapBase();
   if (stackCompareValue == 0)
     {
     iwrtbarTrees->deleteAll();
     return false;
     }

   bool foundInvariantChecks = false;
   ListElement<TR::TreeTop> *nextTree = iwrtbarTrees->getListHead();
   ListElement<TR::TreeTop> *prevTree = NULL;

   for (;nextTree;)
      {
      TR::Node *node = nextTree->getData()->getNode();
      bool isBaseInvariant = false;
      if (node->getOpCodeValue() != TR::wrtbari)
        node = node->getFirstChild();

      if (trace())
         traceMsg(comp(), "base invariant 0 in %p\n", node);

      if (node->getOpCodeValue() == TR::wrtbari)
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
            traceMsg(comp(), "Non invariant iwrtbar %p (%s)\n", node, node->getOpCode().getName());

         if (prevTree)
            {
            prevTree->setNextElement(nextTree->getNextElement());
            }
         else
            {
            iwrtbarTrees->setListHead(nextTree->getNextElement());
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Invariant iwrtbar %p (%s)\n", node, node->getOpCode().getName());
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

      // CAREFUL: This should probably be a whitelist of side-effect free operators. If we move effectful stuff, things
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

	bool TR_LoopVersioner::isExprInvariantRecursive(TR::Node *node, bool ignoreHeapificationStore)
   {
   if (_visitedNodes.isSet(node->getGlobalIndex()))
      return true;

   _visitedNodes.set(node->getGlobalIndex()) ;


   TR::ILOpCode &opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      if ((_seenDefinedSymbolReferences->get(symReference->getReferenceNumber()) &&
           (!ignoreHeapificationStore ||
            _writtenAndNotJustForHeapification->get(symReference->getReferenceNumber()))) ||
          !opCodeIsHoistable(node, comp()) /* ||
         (!symReference->getSymbol()->isAutoOrParm() && (comp()->getMethodHotness() <= warm) &&
         !comp()->getSymRefTab()->isImmutable(symReference)) */)
         return false;
      }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      {
      if (!isExprInvariantRecursive(node->getChild(i)))
         return false;
      }

   return true;
   }


bool TR_LoopVersioner::hasWrtbarBeenSeen(List<TR::TreeTop> *iwrtbarTrees, TR::Node *iwrtbarNode)
   {
   ListElement<TR::TreeTop> *nextTree = iwrtbarTrees->getListHead();
   for (;nextTree;)
      {
      TR::Node *node = nextTree->getData()->getNode();

      if (node->getOpCodeValue() != TR::wrtbari)
        node = node->getFirstChild();

      if (trace())
         traceMsg(comp(), "base invariant 0 in %p\n", node);

      if (node->getOpCodeValue() == TR::wrtbari)
         {
         if (node == iwrtbarNode)
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
   bool disableLoopCodeRatioCheck = feGetEnv("TR_DisableLoopCodeRatioCheck") != NULL;
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
            char *s = classNameToSignature(method->getMethod()->classNameChars(), len, comp);
            TR_OpaqueClassBlock *classOfMethod = comp->fe()->getClassFromSignature(s, len, owningMethod, true);
            traceMsg(comp, "Found profiled gaurd %p is on interface %s\n", guardNode, TR::Compiler->cls.classNameChars(comp, classOfMethod, len));
            }
         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "interfaceGuardCheck/(%s)", comp->signature()));
         int32_t *treeTopCounts = computeCallsiteCounts(loopBlocks, comp);
         float loopCodeRatio = (float)treeTopCounts[guardNode->getInlinedSiteIndex() + 2] / (float)treeTopCounts[0];
         traceMsg(comp, "  Loop code ratio %d / %d = %.2f\n", treeTopCounts[guardNode->getInlinedSiteIndex() + 2], treeTopCounts[0], loopCodeRatio);
         if (disableLoopCodeRatioCheck || loopCodeRatio < 0.25)
            {
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
   if (node->isProfiledGuard())
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



bool TR_LoopVersioner::detectChecksToBeEliminated(TR_RegionStructure *whileLoop, List<TR::Node> *nullCheckedReferences, List<TR::TreeTop> *nullCheckTrees, List<int32_t> *numIndirections, List<TR::TreeTop> *boundCheckTrees, List<TR::TreeTop> *spineCheckTrees, List<int32_t> *numDimensions, List<TR::TreeTop> *conditionalTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *iwrtbarTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *specializedInvariantNodes, List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodesList, bool &discontinue)
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
         bool aggressive = TR::Options::getCmdLineOptions()->getOption(TR_EnableAggressiveLoopVersioning);

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
            if (trace() || comp()->getOptions()->getOption(TR_CountOptTransformations))
               {
               for (TR::TreeTop *tt = entryTree; isUnimportant && tt != exitTree; tt = tt->getNextTreeTop())
                  {
                  TR::Node *ttNode = tt->getNode();
                  TR::ILOpCode &op = ttNode->getOpCode();
                  if ( op.isCheck() || op.isCheckCast() ||
                       op.isBranch() && ttNode->getNumChildren() >= 2 && (!tt->getNode()->getFirstChild()->getOpCode().isLoadConst() || !tt->getNode()->getSecondChild()->getOpCode().isLoadConst()))
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

         bool disableLT = TR::Options::getCmdLineOptions()->getOption(TR_DisableLoopTransfer);
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
             else if (currentOpCode.isBndCheck())
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
               TR_ASSERT(currentNode->getSecondChild()->getOpCodeValue() == TR::loadaddr, "second child should be LOADADDR");
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
            else if (currentOpCode.getOpCodeValue() == TR::ArrayStoreCHK)
               {
               if (trace())
                  traceMsg(comp(), "Array store check %p\n", currentTree->getNode());

               arrayStoreCheckTrees->add(currentTree);

               if (dupOfThisBlockAlreadyExecutedBeforeLoop)
             _checksInDupHeader.add(currentTree);
               }

            //SPECpower Work
            if ( comp()->getOptions()->getGcMode() == TR_WrtbarCardMarkAndOldCheck ||
                 comp()->getOptions()->getGcMode() == TR_WrtbarOldCheck)
               {
               TR::Node *possibleIwrtbarNode = currentTree->getNode();
               if ((currentOpCode.getOpCodeValue() != TR::wrtbari) &&
                  (possibleIwrtbarNode->getNumChildren() > 0))
                  possibleIwrtbarNode = possibleIwrtbarNode->getFirstChild();

               if ((possibleIwrtbarNode->getOpCodeValue() == TR::wrtbari) &&
                  !possibleIwrtbarNode->skipWrtBar() &&
                   !hasWrtbarBeenSeen(iwrtbarTrees, possibleIwrtbarNode))
                  {
                  if (trace())
                     traceMsg(comp(), "iwrtbar %p\n", currentTree->getNode());

                  iwrtbarTrees->add(currentTree);

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
      TR_NodeKillAliasSetInterface defAliases = defNode->mayKill();
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

   static char *profileLongParms = feGetEnv("TR_ProfileLongParms");
   if (TR_LocalAnalysis::isSupportedOpCode(node->getOpCode(), comp()))
      {
      if (node->getType().isInt32() ||
          (node->getType().isInt64() &&
           node->getOpCode().isLoadVar() &&
           node->getSymbolReference()->getSymbol()->isAutoOrParm()))
          {
          if (profileLongParms &&
              comp()->getMethodHotness() == hot &&
              comp()->getRecompilationInfo())
             {
             if (node->getType().isInt64())
                {
                if (node->getSymbolReference()->getSymbol()->isParm())
                   {
                   // Switch this compile to profiling compilation in case a
                   // potential opportunity is seen for specializing long parms (which are
                   // usually invariant) is seen
                   //
                   optimizer()->switchToProfiling();
                   //printf("Profiling longs in %s from LVE\n", comp()->signature());
                   }
                }
             }

          if (/* comp()->getRecompilationInfo() && */ collectProfiledExprs &&
              ((!node->getType().isInt64()) ||
               (profileLongParms /* && (valueInfo->getTopValue() == 0) */)))
             {
#ifdef J9_PROJECT_SPECIFIC
             TR_ValueInfo *valueInfo = static_cast<TR_ValueInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(node, comp(), ValueInfo));
             if (valueInfo)
                {
                if ((valueInfo->getTopProbability() > MIN_PROFILED_FREQUENCY) &&
                    (valueInfo->getTotalFrequency() > 0) &&
                    !_containsCall &&
                    !node->getByteCodeInfo().doNotProfile() && !node->isNodeCreatedByPRE() &&
                    // Only collect nodes from unspecialized blocks to avoid specializing the same nodes twice
                    !block->isSpecialized())
                   {
                   if ((!node->getType().isInt64()) ||
                       (profileLongParms && (valueInfo->getTopValue() == 0)))
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


void TR_LoopVersioner::versionNaturalLoop(TR_RegionStructure *whileLoop, List<TR::Node> *nullCheckedReferences, List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *boundCheckTrees, List<TR::TreeTop> *spineCheckTrees, List<TR::TreeTop> *conditionalTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *iwrtbarTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *specializedNodes, List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodesList, List<TR_Structure> *innerWhileLoops, List<TR_Structure> *clonedInnerWhileLoops, bool skipVersioningAsynchk, SharedSparseBitVector &reverseBranchInLoops)
   {
   if (!performTransformation(comp(), "%sVersioning natural loop %d\n", OPT_DETAILS_LOOP_VERSIONER, whileLoop->getNumber()))
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
      if (whileLoop->getNumber() == treeTop->getNode()->getBlock()->getNumber())
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
      bool disableLT = TR::Options::getCmdLineOptions()->getOption(TR_DisableLoopTransfer);
      if (!disableLT &&
          lastTree->getOpCode().isIf() &&
          blocksInWhileLoop.find(lastTree->getBranchDestination()->getNode()->getBlock()) &&
          lastTree->isTheVirtualGuardForAGuardedInlinedCall() &&
          isBranchSuitableToDoLoopTransfer(&blocksInWhileLoop, lastTree, comp()))
         {
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
         ///_virtualGuardPair.add(virtualGuardPair);
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

               TR_BlockStructure *newGotoBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newGotoBlock->getNumber(), newGotoBlock);
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

      if (!refineAliases() && _canPredictIters && comp()->getProfilingMode() != JitProfiling &&
          performTransformation(comp(), "%s Creating test outside loop for deciding if async check is required\n", OPT_DETAILS_LOOP_VERSIONER))
         {
         TR::Node *lowerBound = _loopTestTree->getNode()->getFirstChild()->duplicateTreeForCodeMotion();
         TR::Node *upperBound = _loopTestTree->getNode()->getSecondChild()->duplicateTreeForCodeMotion();
         TR::Node *loopLimit = isIncreasing ?
            TR::Node::create(TR::isub, 2, upperBound, lowerBound) :
            TR::Node::create(TR::isub, 2, lowerBound, upperBound);

         collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, loopLimit, &comparisonTrees, clonedLoopInvariantBlock, comp()->incVisitCount());
         TR::Node *nextComparisonNode = TR::Node::createif(comparisonOpCode, loopLimit, TR::Node::create(loopLimit, TR::iconst, 0, profiledLoopLimit), clonedLoopInvariantBlock->getEntry());
         nextComparisonNode->setIsMaxLoopIterationGuard(true);

         if (trace())
            {
            traceMsg(comp(), "Removing async check %p\n", _asyncCheckTree->getNode());
            }

         comp()->setLoopWasVersionedWrtAsyncChecks(true);
         comparisonTrees.add(nextComparisonNode);
         dumpOptDetails(comp(), "The node %p has been created for testing if async check is required\n", nextComparisonNode);

         if (!comp()->getOption(TR_DisableAsyncCheckVersioning))
            {
            TR::TreeTop *prevTree = _asyncCheckTree->getPrevTreeTop();
            TR::TreeTop *nextTree = _asyncCheckTree->getNextTreeTop();
            prevTree->join(nextTree);
            }

         whileLoop->getEntryBlock()->getStructureOf()->setIsEntryOfShortRunningLoop();
         if (trace())
            traceMsg(comp(), "Marked block %p with entry %p\n", whileLoop->getEntryBlock(), whileLoop->getEntryBlock()->getEntry()->getNode());
         }
      }

   // Construct the tests for invariant expressions that need
   // to be null checked.

   // default hotness threshold
   TR_Hotness hotnessThreshold = hot;

   // If aggressive loop versioning is requested, don't call buildNullCheckComparisonsTree based on hotness
   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableAggressiveLoopVersioning))
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
         buildNullCheckComparisonsTree(nullCheckedReferences, nullCheckTrees, boundCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, clonedLoopInvariantBlock);
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
      buildBoundCheckComparisonsTree(nullCheckTrees, boundCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, spineCheckTrees, &comparisonTrees, clonedLoopInvariantBlock, reverseBranch);
      }

   // Construct the tests for invariant or induction var expressions that need
   // to be bounds checked.
   //
   if (shouldOnlySpecializeLoops() && !spineCheckTrees->isEmpty())
      {
      buildSpineCheckComparisonsTree(nullCheckTrees, spineCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, clonedLoopInvariantBlock);
      }


   // Construct the tests for invariant expressions that need
   // to be div checked.
   //
   if (!divCheckTrees->isEmpty() &&
      !shouldOnlySpecializeLoops())
      {
      buildDivCheckComparisonsTree(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, clonedLoopInvariantBlock);
      }

   //Construct the tests for invariant expressions that need to be checked for write barriers.
   //
   if (!iwrtbarTrees->isEmpty() &&
      !shouldOnlySpecializeLoops())
      {
      buildIwrtbarComparisonsTree(iwrtbarTrees, nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, clonedLoopInvariantBlock);
      }


   // Construct specialization tests
   //
   if (!specializedNodes->isEmpty())
      {
      TR::SymbolReference **symRefs = (TR::SymbolReference **)trMemory()->allocateStackMemory(comp()->getSymRefCount()*sizeof(TR::SymbolReference *));
      memset(symRefs, 0, comp()->getSymRefCount()*sizeof(TR::SymbolReference *));

      //printf("Reached here in %s\n", comp()->signature());
      bool specializedLongs = buildSpecializationTree(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, specializedNodes, clonedLoopInvariantBlock, invariantBlock, symRefs);

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
      }


   // Construct trees for refined alias version
   if(refineAliases())
      buildAliasRefinementComparisonTrees(nullCheckTrees,divCheckTrees, checkCastTrees, arrayStoreCheckTrees,&comparisonTrees,clonedLoopInvariantBlock);

   // Construct the tests for invariant conditionals.
   //
   if (!conditionalTrees->isEmpty())
      buildConditionalTree(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, conditionalTrees, &comparisonTrees, clonedLoopInvariantBlock, reverseBranchInLoops);

   // Construct the tests for invariant expressions that need
   // to be cast.
   //
   if (!checkCastTrees->isEmpty())
      {
      buildCheckCastComparisonsTree(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, clonedLoopInvariantBlock);

      ListElement<TR::TreeTop> *nextTree = checkCastTrees->getListHead();
      while (nextTree)
         {
         TR::TreeTop *checkCastTree = nextTree->getData();
         TR::Node *checkCastNode = checkCastTree->getNode();

         TR::TreeTop *prevTreeTop = checkCastTree->getPrevTreeTop();
         TR::TreeTop *nextTreeTop = checkCastTree->getNextTreeTop();
         TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, checkCastNode->getFirstChild()), NULL, NULL);
         TR::TreeTop *secondNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, checkCastNode->getSecondChild()), NULL, NULL);
         prevTreeTop->join(firstNewTree);
         firstNewTree->join(secondNewTree);
         secondNewTree->join(nextTreeTop);
         checkCastNode->recursivelyDecReferenceCount();

         nextTree = nextTree->getNextElement();
         }
      }

   if (!arrayStoreCheckTrees->isEmpty())
      {
      buildArrayStoreCheckComparisonsTree(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, clonedLoopInvariantBlock);

      ListElement<TR::TreeTop> *nextTree = arrayStoreCheckTrees->getListHead();
      while (nextTree)
         {
         TR::TreeTop *arrayStoreCheckTree = nextTree->getData();
         TR::Node *arrayStoreCheckNode = arrayStoreCheckTree->getNode();

         TR::TreeTop *prevTreeTop = arrayStoreCheckTree->getPrevTreeTop();
         TR::TreeTop *nextTreeTop = arrayStoreCheckTree->getNextTreeTop();
         TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, arrayStoreCheckNode->getFirstChild()), NULL, NULL);
         TR::Node *child = arrayStoreCheckNode->getFirstChild();
         if (child->getOpCodeValue() == TR::wrtbari && comp()->getOptions()->getGcMode() == TR_WrtbarNone &&
            performTransformation(comp(), "%sChanging iwrtbar node [%p] to an iastore\n", OPT_DETAILS_LOOP_VERSIONER, child))
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
         if (child->getOpCodeValue() == TR::wrtbari && comp()->getOptions()->getGcMode() == TR_WrtbarNone &&
             performTransformation(comp(), "%sChanging iwrtbar node [%p] to an iastore\n", OPT_DETAILS_LOOP_VERSIONER, child))
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

      nextTree = nextTree->getNextElement();
      }
   }

   // Construct tests for invariant expressions
   //
   if (invariantNodes && !invariantNodes->isEmpty())
      {
      //printf("Reached here in %s\n", comp()->signature());
      buildLoopInvariantTree(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, &comparisonTrees, invariantNodes, /*invariantTranslationNodesList*/ NULL, clonedLoopInvariantBlock, invariantBlock);
      invariantNodes->deleteAll();
      }

   if (comparisonTrees.isEmpty() &&
      _containsGuard && performTransformation(comp(), "%s Creating test outside loop for checking if virtual guard is required\n", OPT_DETAILS_LOOP_VERSIONER))
      {
      TR::Node *constNode = TR::Node::create(blockHeadNode, TR::iconst, 0, 0);
      TR::Node *nextComparisonNode = TR::Node::createif(TR::ificmpne, constNode, constNode, clonedLoopInvariantBlock->getEntry());
      comparisonTrees.add(nextComparisonNode);
      dumpOptDetails(comp(), "The node %p has been created for testing if virtual guard is required\n", nextComparisonNode);
      }

   // If all yield points have been removed from the loop but OSR guards remain,
   // they can be versioned out. Currently, this code assumes all OSR guards
   // will be patched by the same runtime assumptions, so only one OSR guard is added
   // to branch to the slow loop.
   //
   bool safeToRemoveOSRGuards = false;
   bool seenOSRGuards = false;
   bool safeToRemoveHCRGuards = false;
   TR_ScratchList<TR::TreeTop> hcrGuards(trMemory());
   static char *disableLoopOSR = feGetEnv("TR_DisableOSRGuardLoopVersioner");
   static char *disableLoopHCR = feGetEnv("TR_DisableHCRGuardLoopVersioner");
   if (comp()->getHCRMode() == TR::osr && disableLoopOSR == NULL)
      {
      safeToRemoveOSRGuards = true;
      ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
      TR::Node *osrGuard = NULL;
      for (TR::Block *block = blocksIt.getCurrent(); safeToRemoveOSRGuards && block; block = blocksIt.getNext())
         {
         for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
            {
            if (comp()->isPotentialOSRPoint(tt->getNode(), NULL, true))
               {
               safeToRemoveOSRGuards = false;
               break;
               }
            else if (tt->getNode()->isOSRGuard())
               {
               osrGuard = tt->getNode();
               seenOSRGuards = true;
               }
            }
         }
      if (seenOSRGuards && safeToRemoveOSRGuards)
         {
         if (performTransformation(comp(), "%sCreate versioned OSRGuard\n", OPT_DETAILS_LOOP_VERSIONER))
            {
            TR_ASSERT(osrGuard, "should have found an OSR guard to version");

            // Duplicate the OSR guard tree
            TR::Node *guard = osrGuard->duplicateTree();
            if (trace())
               traceMsg(comp(), "OSRGuard n%dn has been created to guard against method invalidation\n", guard->getGlobalIndex());
            guard->setBranchDestination(clonedLoopInvariantBlock->getEntry());
            comparisonTrees.add(guard);
            }
         else
            {
            safeToRemoveOSRGuards = false;
            }
         }
      }
   if (comp()->getHCRMode() != TR::none && disableLoopHCR == NULL)
      {
      safeToRemoveHCRGuards = true;
      ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
      for (TR::Block *block = blocksIt.getCurrent(); safeToRemoveHCRGuards && block; block = blocksIt.getNext())
         {
         for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
            {
            if (tt->getNode()->isHCRGuard())
               {
               hcrGuards.add(tt);
               }
            else
               {
               // Identify virtual call nodes for the taken side of HCR guards
               bool isVirtualCallForHCR = (tt->getNode()->getOpCodeValue() == TR::treetop || tt->getNode()->getOpCode().isCheck())
                  && tt->getNode()->getFirstChild()->isTheVirtualCallNodeForAGuardedInlinedCall()
                  && block->getPredecessors().size() == 1
                  && block->getPredecessors().front()->getFrom()->asBlock()->getLastRealTreeTop()->getNode()->isHCRGuard();

               if ((tt->getNode()->canGCandReturn() || tt->getNode()->canGCandExcept()) && !isVirtualCallForHCR)
                  {
                  safeToRemoveHCRGuards = false;
                  break;
                  }
               }
            }
         }
      if (safeToRemoveHCRGuards && !hcrGuards.isEmpty())
         {
         ListIterator<TR::TreeTop> guardIt(&hcrGuards);
         for (TR::TreeTop *tt = guardIt.getCurrent(); tt; tt = guardIt.getNext())
             {
             if (performTransformation(comp(), "%sCreated versioned HCRGuard for guard n%dn\n", OPT_DETAILS_LOOP_VERSIONER, tt->getNode()->getGlobalIndex()))
                {
                TR::Node *guard = tt->getNode()->duplicateTree();
                guard->setBranchDestination(clonedLoopInvariantBlock->getEntry());
                comparisonTrees.add(guard);
                }
             else
                {
                safeToRemoveHCRGuards = false;
                }
             }
         }
      }

   // Due to RAS changes to make each loop version test a transformation, disableOptTransformations or lastOptTransformationIndex can now potentially remove all the tests above the 2 versioned loops.  When there are two versions of the loop, it is necessary that there be at least one test at the top.  Therefore, the following is required to ensure that a test is created.
   if (comparisonTrees.isEmpty())
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

      if (firstComparisonNode)
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
         TR_BlockStructure *newGotoBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), newGotoBlock->getNumber(), newGotoBlock);
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

      // hookup the anchor node if needed
      //
      if (comp()->useCompressedPointers())
         {
         for (int32_t i = 0; i < actualComparisonNode->getNumChildren(); i++)
            {
            TR::Node *objectRef = actualComparisonNode->getChild(i);
            bool shouldBeCompressed = false;
            if (objectRef->getOpCode().isLoadIndirect() &&
                  objectRef->getDataType() == TR::Address &&
                  TR::TransformUtil::fieldShouldBeCompressed(objectRef, comp()))
               {
               shouldBeCompressed = true;
               }
            else if (objectRef->getOpCode().isArrayLength() &&
                        objectRef->getFirstChild()->getOpCode().isLoadIndirect() &&
                        TR::TransformUtil::fieldShouldBeCompressed(objectRef->getFirstChild(), comp()))
               {
               objectRef = objectRef->getFirstChild();
               shouldBeCompressed = true;
               }
            if (shouldBeCompressed)
               {
               TR::TreeTop *translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(objectRef), NULL, NULL);
               comparisonBlock->prepend(translateTT);
               }
            }
         }

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
      const char *debugCounter = TR::DebugCounter::debugCounterName(comp(), "loopVersioner.fail/(%s)/%s/origin=block_%d", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()), currentBlock->getNumber());
      if (nextComparisonBlock)
         {
         _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock, nextComparisonBlock->getData(), trMemory()));
         _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock, currCriticalEdgeBlock->getData(), trMemory()));
         _cfg->addEdge(TR::CFGEdge::createEdge(currCriticalEdgeBlock->getData(), clonedLoopInvariantBlock, trMemory()));
         TR::DebugCounter::prependDebugCounter(comp(), debugCounter, currCriticalEdgeBlock->getData()->getEntry()->getNextTreeTop());
         currCriticalEdgeBlock = currCriticalEdgeBlock->getNextElement();
         }
      else
         {
         _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  invariantBlock, trMemory()));
         _cfg->addEdge(TR::CFGEdge::createEdge(currentBlock,  clonedLoopInvariantBlock, trMemory()));
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
   TR_BlockStructure *clonedInvariantBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), clonedLoopInvariantBlock->getNumber(), clonedLoopInvariantBlock);
   clonedInvariantBlockStructure->setCreatedByVersioning(true);

   if (!_neitherLoopCold)
   clonedInnerWhileLoops->deleteAll();
   clonedInvariantBlockStructure->setAsLoopInvariantBlock(true);
   TR_RegionStructure *parentStructure = whileLoop->getParent()->asRegion();
   TR_RegionStructure *properRegion = new (_cfg->structureRegion()) TR_RegionStructure(comp(), chooserBlock->getNumber());
   parentStructure->replacePart(invariantBlockStructure, properRegion);

   TR_StructureSubGraphNode *clonedWhileNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(clonedWhileLoop);
   TR_StructureSubGraphNode *whileNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(whileLoop);
   TR_StructureSubGraphNode *invariantNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(invariantBlockStructure);
   TR_StructureSubGraphNode *clonedInvariantNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(clonedInvariantBlockStructure);

   properRegion->addSubNode(whileNode);
   properRegion->addSubNode(clonedWhileNode);
   properRegion->addSubNode(invariantNode);
   properRegion->addSubNode(clonedInvariantNode);

   TR_StructureSubGraphNode *prevComparisonNode = NULL;
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

      TR_BlockStructure *comparisonBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), actualComparisonBlock->getNumber(), actualComparisonBlock);
      comparisonBlockStructure->setCreatedByVersioning(true);
      TR_StructureSubGraphNode *comparisonNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(comparisonBlockStructure);
      properRegion->addSubNode(comparisonNode);

      if (prevComparisonNode)
         TR::CFGEdge::createEdge(prevComparisonNode,  comparisonNode, trMemory());
      else
         properRegion->setEntry(comparisonNode);

      //TR::CFGEdge::createEdge(comparisonNode,  clonedInvariantNode, trMemory());
      prevComparisonNode = comparisonNode;
      currComparisonBlock = currComparisonBlock->getNextElement();

      if (currComparisonBlock)
         {
         TR_StructureSubGraphNode *criticalEdgeNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(currCriticalEdgeBlock->getData()->getStructureOf());
         properRegion->addSubNode(criticalEdgeNode);
         TR::CFGEdge::createEdge(prevComparisonNode,  criticalEdgeNode, trMemory());
         TR::CFGEdge::createEdge(criticalEdgeNode,  clonedInvariantNode, trMemory());
         currCriticalEdgeBlock = currCriticalEdgeBlock->getNextElement();
         }
      else
         TR::CFGEdge::createEdge(prevComparisonNode,  clonedInvariantNode, trMemory());
      }

   TR::CFGEdge::createEdge(prevComparisonNode,  invariantNode, trMemory());
   TR::CFGEdge::createEdge(invariantNode,  whileNode, trMemory());
   TR::CFGEdge::createEdge(clonedInvariantNode,  clonedWhileNode, trMemory());

   // Since the new proper region replaced the original loop invariant
   // block in the parent structure, the successor of the loop
   // invariant block strcuture (the natural loop originally) must now  be removed
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
            (*changedSuccEdge)->setFrom(properNode);

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
      TR_StructureSubGraphNode *newGotoBlockNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(newGotoBlockStructure);
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
         properRegion->addExitEdge(whileNode, node->getNumber(), isExceptionEdge);
         seenExitNodes.set(node->getNumber());
         }
      }

   // Patch up the cloned while loop edges properly
   //
   seenExitNodes.empty();
   ei.set(&clonedWhileLoop->getExitEdges());
   for (exitEdge = ei.getCurrent(); exitEdge; exitEdge = ei.getNext())
      {
      TR_StructureSubGraphNode *node = toStructureSubGraphNode(exitEdge->getTo());
      if (!seenExitNodes.get(node->getNumber()))
         {
         bool isExceptionEdge = false;
         if (exitEdge->getFrom()->hasExceptionSuccessor(node))
            isExceptionEdge = true;
         properRegion->addExitEdge(clonedWhileNode, node->getNumber(), isExceptionEdge);
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

   // If OSR guards have been versioned out of the loop, walk the
   // original loop and remove the guards and their branch edges
   //
   if (seenOSRGuards && safeToRemoveOSRGuards)
      {
      ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
       for (TR::Block *block = blocksIt.getCurrent(); block; block = blocksIt.getNext())
          {
          TR::TreeTop *lastRealTT = block->getLastRealTreeTop();
          if (lastRealTT
              && lastRealTT->getNode()->isOSRGuard()
              && performTransformation(comp(), "%sRemove OSR guard n%dn from hot loop\n", OPT_DETAILS_LOOP_VERSIONER, lastRealTT->getNode()->getGlobalIndex()))
             {
             block->removeBranch(comp());
             }
          }
      }
   if (safeToRemoveHCRGuards && !hcrGuards.isEmpty())
      {
      ListIterator<TR::Block> blocksIt(&blocksInWhileLoop);
      for (TR::Block *block = blocksIt.getCurrent(); block; block = blocksIt.getNext())
         {
         TR::TreeTop *lastRealTT = block->getLastRealTreeTop();
         if (lastRealTT
             && lastRealTT->getNode()->isHCRGuard()
             && performTransformation(comp(), "%sRemove HCR guard n%dn from hot loop\n", OPT_DETAILS_LOOP_VERSIONER, lastRealTT->getNode()->getGlobalIndex()))
            {
            block->removeBranch(comp());
            }
         }
      }

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after this versioning");
      //traceMsg(comp(), "Structure after versioning whileLoop : %d\n", whileLoop->getNumber());
      //if (comp()->getFlowGraph()->getStructure())
         //comp()->getFlowGraph()->getStructure()->print(comp()->getOutFile(), 6);
      }
   }

void TR_LoopVersioner::buildNullCheckComparisonsTree(List<TR::Node> *nullCheckedReferences, List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *boundCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock)
   {
   ListElement<TR::Node> *nextNode = nullCheckedReferences->getListHead();
   ListElement<TR::TreeTop> *nextTree = nullCheckTrees->getListHead();

   for (; nextNode;)
      {
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
                  dupInvariantNullCheckReference= invariantNullCheckReference->duplicateTreeForCodeMotion();
                  TR::Node *oldInvariantNullCheckReference = nextNode->getData();
                  nextTree->getData()->getNode()->setNullCheckReference(dupInvariantNullCheckReference);
                  oldInvariantNullCheckReference->recursivelyDecReferenceCount();
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

      vcount_t visitCount = comp()->incVisitCount();
      if (!nodeToBeNullChkd)
         nodeToBeNullChkd = nextNode->getData();
      ///collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, nextNode->getData(), comparisonTrees, exitGotoBlock, visitCount);
      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, nodeToBeNullChkd, comparisonTrees, exitGotoBlock, visitCount);

      if (performTransformation(comp(), "%s Creating test outside loop for checking if %p is null\n", OPT_DETAILS_LOOP_VERSIONER, nextNode->getData()))
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

         //TR::Node *nextComparisonNode = TR::Node::createif(TR::ifacmpeq, duplicateNullCheckReference, TR::Node::create(duplicateNullCheckReference, TR::aconst, 0, 0), exitGotoBlock->getEntry());
         TR::Node *nextComparisonNode = TR::Node::createif(TR::ifacmpeq, duplicateNullCheckReference, TR::Node::aconst(duplicateNullCheckReference, 0), exitGotoBlock->getEntry());
         comparisonTrees->add(nextComparisonNode);

         dumpOptDetails(comp(), "The node %p has been created for testing if null check is required\n", nextComparisonNode);

         if (nextTree->getData()->getNode()->getOpCodeValue() == TR::NULLCHK)
            TR::Node::recreate(nextTree->getData()->getNode(), TR::treetop);
         else if (nextTree->getData()->getNode()->getOpCodeValue() == TR::ResolveAndNULLCHK)
            TR::Node::recreate(nextTree->getData()->getNode(), TR::ResolveCHK);

         if (trace())
            {
            traceMsg(comp(), "Doing check for null check reference %p\n", nextNode->getData());
            traceMsg(comp(), "Adjusting tree %p\n", nextTree->getData()->getNode());
            }
         }
      nextNode = nextNode->getNextElement();
      nextTree = nextTree->getNextElement();
      }
   }


void TR_LoopVersioner::buildIwrtbarComparisonsTree(List<TR::TreeTop> *iwrtbarTrees, List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock)
   {
#ifdef J9_PROJECT_SPECIFIC
   ListElement<TR::TreeTop> *nextTree = iwrtbarTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *iwrtbarTree = nextTree->getData();
      TR::Node *iwrtbarNode = iwrtbarTree->getNode();
      if (iwrtbarNode->getOpCodeValue() != TR::wrtbari)
        iwrtbarNode = iwrtbarNode->getFirstChild();

      //traceMsg(comp(), "iwrtbar node %p\n", iwrtbarNode);

      //vcount_t visitCount = comp()->incVisitCount();
      //collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, divCheckNode->getFirstChild()->getSecondChild(), comparisonTrees, exitGotoBlock, visitCount);

      if (performTransformation(comp(), "%s Creating test outside loop for checking if %p is iwrtbar is required\n", OPT_DETAILS_LOOP_VERSIONER, iwrtbarNode))
         {
         TR::Node *duplicateBase = iwrtbarNode->getLastChild()->duplicateTreeForCodeMotion();
         TR::Node *ifNode, *ifNode1, *ifNode2;

         bool isX86 = false;

#ifdef TR_TARGET_X86
         isX86 = true;
#endif

         // If we can guarantee a fixed tenure size we can hardcode size constants
         bool isVariableHeapBase = comp()->getOptions()->isVariableHeapBaseForBarrierRange0();
         bool isVariableHeapSize = comp()->getOptions()->isVariableHeapSizeForBarrierRange0();

         TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

         if (!isX86 && (isVariableHeapBase || isVariableHeapSize))
            {
            ifNode1 =  TR::Node::create(TR::acmpge, 2, duplicateBase, TR::Node::createWithSymRef(TR::aload, 0, comp()->getSymRefTab()->findOrCreateThreadLowTenureAddressSymbolRef()));
            }
         else
            {
            ifNode1 =  TR::Node::create(TR::acmpge, 2, duplicateBase, TR::Node::create(duplicateBase, TR::aconst, 0, fej9->getLowTenureAddress()));
            }

         //comparisonTrees->add(ifNode);

         dumpOptDetails(comp(), "1 The node %p has been created for testing if iwrtbar is required\n", ifNode1);

         duplicateBase = iwrtbarNode->getLastChild()->duplicateTreeForCodeMotion();

         if (!isX86 && (isVariableHeapBase || isVariableHeapSize))
            {
            ifNode2 =  TR::Node::create(TR::acmplt, 2, duplicateBase, TR::Node::createWithSymRef(TR::aload, 0, comp()->getSymRefTab()->findOrCreateThreadHighTenureAddressSymbolRef()));
            }
         else
            {
            ifNode2 =  TR::Node::create(TR::acmplt, 2, duplicateBase, TR::Node::create(duplicateBase, TR::aconst, 0, fej9->getHighTenureAddress()));
            }

         ifNode =  TR::Node::createif(TR::ificmpne, TR::Node::create(TR::iand, 2, ifNode1, ifNode2), TR::Node::create(duplicateBase, TR::iconst, 0, 0), exitGotoBlock->getEntry());

         comparisonTrees->add(ifNode);

         dumpOptDetails(comp(), "2 The node %p has been created for testing if iwrtbar is required\n", ifNode2);


         //printf("Found opportunity for skipping wrtbar %p in %s at freq %d\n", iwrtbarNode, comp()->signature(), iwrtbarTree->getEnclosingBlock()->getFrequency()); fflush(stdout);

         iwrtbarNode->setSkipWrtBar(true);
         }

      nextTree = nextTree->getNextElement();
      }
#endif
   }


void TR_LoopVersioner::buildDivCheckComparisonsTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock)
   {
   ListElement<TR::TreeTop> *nextTree = divCheckTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *divCheckTree = nextTree->getData();
      TR::Node *divCheckNode = divCheckTree->getNode();
      vcount_t visitCount = comp()->incVisitCount();
      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, divCheckNode->getFirstChild()->getSecondChild(), comparisonTrees, exitGotoBlock, visitCount);

      if (performTransformation(comp(), "%s Creating test outside loop for checking if %p is divide by zero\n", OPT_DETAILS_LOOP_VERSIONER, divCheckNode))
         {
         TR::Node *duplicateDivisor = divCheckNode->getFirstChild()->getSecondChild()->duplicateTreeForCodeMotion();
         TR::Node *ifNode;
         if (duplicateDivisor->getType().isInt64())
            ifNode =  TR::Node::createif(TR::iflcmpeq, duplicateDivisor, TR::Node::create(duplicateDivisor, TR::lconst, 0, 0), exitGotoBlock->getEntry());
         else
            ifNode =  TR::Node::createif(TR::ificmpeq, duplicateDivisor, TR::Node::create(duplicateDivisor, TR::iconst, 0, 0), exitGotoBlock->getEntry());
         comparisonTrees->add(ifNode);
         dumpOptDetails(comp(), "The node %p has been created for testing if div check is required\n", ifNode);
         }
      TR::Node::recreate(divCheckNode, TR::treetop);
      nextTree = nextTree->getNextElement();
      }
   }




void TR_LoopVersioner::buildCheckCastComparisonsTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock)

   {
   ListElement<TR::TreeTop> *nextTree = checkCastTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *checkCastTree = nextTree->getData();
      TR::Node *checkCastNode = checkCastTree->getNode();
      vcount_t visitCount = comp()->incVisitCount();
      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, checkCastNode, comparisonTrees, exitGotoBlock, visitCount);
      nextTree = nextTree->getNextElement();
      }
   }



void TR_LoopVersioner::buildArrayStoreCheckComparisonsTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock)
   {
   ListElement<TR::TreeTop> *nextTree = arrayStoreCheckTrees->getListHead();
   while (nextTree)
      {
      if (!performTransformation(comp(), "%s Creating test outside loop for checking if %p is casted\n", OPT_DETAILS_LOOP_VERSIONER, nextTree->getData()->getNode()))
	 {
         nextTree = nextTree->getNextElement();
         continue;
	 }


      TR::TreeTop *arrayStoreCheckTree = nextTree->getData();
      TR::Node *arrayStoreCheckNode = arrayStoreCheckTree->getNode();
      vcount_t visitCount = comp()->incVisitCount();

      // this is a general fix
      // NOTE: since buildArrayStoreCheckComparisonTree only checks part
      // of the arrayStoreCheckNode for invariancy, we should only call
      // collectExprToBeChecked on the same subtree to avoid versioning for
      // other part of the tree that may be loop variant

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
      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, childOfAddressNode, comparisonTrees, exitGotoBlock, visitCount);
      visitCount = comp()->incVisitCount();
      //collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, arrayNode, comparisonTrees, exitGotoBlock, visitCount);


      TR::Node *duplicateSrcArray = arrayNode->duplicateTreeForCodeMotion();
      TR::Node *duplicateClassPtr = TR::Node::createWithSymRef(TR::aloadi, 1, 1, duplicateSrcArray, comp()->getSymRefTab()->findOrCreateVftSymbolRef());

      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, duplicateClassPtr, comparisonTrees, exitGotoBlock, visitCount);

      TR::Node *duplicateCheckedValue = childOfAddressNode->duplicateTreeForCodeMotion();

     TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, duplicateCheckedValue,  duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
     TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(arrayStoreCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());
     comparisonTrees->add(ificmpeqNode);
     dumpOptDetails(comp(), "The node %p has been created for testing if arraystorecheck is required\n", ificmpeqNode);

      nextTree = nextTree->getNextElement();
      }
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



bool TR_LoopVersioner::buildLoopInvariantTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees,
                                               List<TR::TreeTop> *checkCastTrees,
                                               List<TR::TreeTop> *arrayStoreCheckTrees,
                                               List<TR::Node> *comparisonTrees,
                                               List<TR_NodeParentSymRef> *invariantNodes,
                                               List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodesList,
                                               TR::Block *exitGotoBlock, TR::Block *loopInvariantBlock)
   {

   if (comp()->getJittedMethodSymbol() && // avoid NULL pointer on non-Wcode builds
       comp()->getJittedMethodSymbol()->isNoTemps())
       {
       dumpOptDetails(comp(), "buildLoopInvariantTree not safe to perform when NOTEMPS enabled\n");
       return false;
       }
   TR::TreeTop *placeHolderTree = loopInvariantBlock->getLastRealTreeTop();
   TR::Node *placeHolderNode = placeHolderTree->getNode();
   int dumped = 0;

   TR::ILOpCode &placeHolderOpCode = placeHolderNode->getOpCode();
   if (placeHolderOpCode.isBranch() ||
       placeHolderOpCode.isJumpWithMultipleTargets() ||
       placeHolderOpCode.isReturn() ||
       placeHolderOpCode.getOpCodeValue() == TR::athrow)
      {
      }
   else
      placeHolderTree = loopInvariantBlock->getExit();

   TR::TreeTop *treeBeforePlaceHolderTree = placeHolderTree->getPrevTreeTop();

   ListElement<TR_NodeParentSymRef> *nextInvariantNode = invariantNodes->getListHead();
   while (nextInvariantNode)
      {
      // Heuristic: Pulling out really small trees doesn't help reduce
      // the amount of computation much, and just increases register pressure.
      //
      // Don't do it. Instead, count the total number of nodes in a tree, and
      // only pull it out if there are 4 or more nodes. This number is arbitrary,
      // and could probably be tweaked.

      TR::Node *invariantNode = nextInvariantNode->getData()->_node;
      if (nodeSize(invariantNode) < 4)
         {
         if (trace())
            traceMsg(comp(), "skipping undersized tree %p\n", nextInvariantNode->getData()->_node);
         nextInvariantNode = nextInvariantNode->getNextElement();
         continue;
         }
      if (nextInvariantNode->getData()->_symRef)
         {
         nextInvariantNode = nextInvariantNode->getNextElement();
         continue;
         }
      //invariant expressions might be the part of checks we already versioned.
      //if buildXXX already removed the nodes representing the expressions
      //ICM will assert while decrementing their ref counts which might already be 0s
      if (nextInvariantNode->getData()->_node->getReferenceCount() < 1 || nextInvariantNode->getData()->_parent->getReferenceCount()<1)
         {
         if (trace())
            traceMsg(comp(), "skipping node %p or its parent %p which were removed as a part of a versioned check (e.g. count < 1)\n", nextInvariantNode->getData()->_node, nextInvariantNode->getData()->_parent);
         nextInvariantNode = nextInvariantNode->getNextElement();
         continue;
         }

      vcount_t visitCount = comp()->incVisitCount();
      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, invariantNode, comparisonTrees, exitGotoBlock, visitCount);

      //printf("Creating test for loop invariant expression %p in in block_%d %s\n", invariantNode, loopInvariantBlock->getNumber(), comp()->signature());

      if (performTransformation(comp(), "%s Creating store outside the loop for loop invariant expression %p\n", OPT_DETAILS_LOOP_VERSIONER, invariantNode))
         {
         TR::Node *duplicateNode = invariantNode->duplicateTree();

         TR::DataType dataType = invariantNode->getDataType();
         TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType);

         if (invariantNode->getOpCode().hasSymbolReference() && invariantNode->getSymbolReference()->getSymbol()->isNotCollected())
            newSymbolReference->getSymbol()->setNotCollected();

         if (comp()->useCompressedPointers())
            {
            if (duplicateNode->getOpCode().isLoadIndirect() &&
                  duplicateNode->getDataType() == TR::Address &&
                  TR::TransformUtil::fieldShouldBeCompressed(duplicateNode, comp()))
               {
               TR::TreeTop *translateTT = TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(duplicateNode), NULL, NULL);
               treeBeforePlaceHolderTree->join(translateTT);
               translateTT->join(placeHolderTree);
               treeBeforePlaceHolderTree = translateTT;
               }
            }

         TR::Node *storeForInvariantNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(duplicateNode->getDataType()), 1, 1, duplicateNode, newSymbolReference);
         TR::TreeTop *storeForInvariantTree = TR::TreeTop::create(comp(), storeForInvariantNode, 0, 0);
         nextInvariantNode->getData()->_symRef = newSymbolReference;

         treeBeforePlaceHolderTree->join(storeForInvariantTree);
         storeForInvariantTree->join(placeHolderTree);
         treeBeforePlaceHolderTree = storeForInvariantTree;

         //printf("Creating store %p outside the loop for loop invariant expression %p in in block_%d %s\n", storeForInvariantNode, invariantNode, loopInvariantBlock->getNumber(), comp()->signature());
         //fflush(stdout);

         ListElement<TR_NodeParentSymRef> *otherInvariantNodeElem = nextInvariantNode->getNextElement();
         ListElement<TR_NodeParentSymRef> *prevOtherInvariantNodeElem = nextInvariantNode;
         for (;otherInvariantNodeElem;)
            {
            TR::Node *otherInvariantNode = otherInvariantNodeElem->getData()->_node;

            if (!otherInvariantNodeElem->getData()->_symRef &&
                optimizer()->areNodesEquivalent(otherInvariantNode, invariantNode))
               {
               visitCount = comp()->incVisitCount();
               if (optimizer()->areSyntacticallyEquivalent(otherInvariantNode, invariantNode, visitCount))
                  {
                  otherInvariantNodeElem->getData()->_symRef = newSymbolReference;

                  /*
                  int32_t childNum;
                  for (childNum=0;childNum<otherInvariantNode->getNumChildren(); childNum++)
                     otherInvariantNode->getChild(childNum)->recursivelyDecReferenceCount();
                  otherInvariantNode->setNumChildren(0);
                  TR::Node::recreate(otherInvariantNode, comp()->il.opCodeForDirectLoad(invariantNode->getDataType()));
                  otherInvariantNode->setSymbolReference(newSymbolReference);
                  if (otherInvariantNodeElem->getData()->_parent &&
                      otherInvariantNodeElem->getData()->_parent->getOpCode().isNullCheck())
                     TR::Node::recreate(otherInvariantNodeElem->getData()->_parent, TR::treetop);

                  if (prevOtherInvariantNodeElem)
                     prevOtherInvariantNodeElem->setNextElement(otherInvariantNodeElem->getNextElement());
                  otherInvariantNodeElem = otherInvariantNodeElem->getNextElement();
                  continue;
                  */
                  }
               }

            prevOtherInvariantNodeElem = otherInvariantNodeElem;
            otherInvariantNodeElem = otherInvariantNodeElem->getNextElement();
            }

         /*
         int32_t childNum;
         for (childNum=0;childNum<invariantNode->getNumChildren(); childNum++)
            invariantNode->getChild(childNum)->recursivelyDecReferenceCount();
         invariantNode->setNumChildren(0);
         TR::Node::recreate(invariantNode, comp()->il.opCodeForDirectLoad(invariantNode->getDataType()));
         invariantNode->setSymbolReference(newSymbolReference);

         if (nextInvariantNode->getData()->_parent &&
             nextInvariantNode->getData()->_parent->getOpCode().isNullCheck())
             TR::Node::recreate(nextInvariantNode->getData()->_parent, TR::treetop);
         */

         optimizer()->setRequestOptimization(OMR::globalValuePropagation, true);
         optimizer()->setUseDefInfo(NULL);
         optimizer()->setValueNumberInfo(NULL);
         optimizer()->setAliasSetsAreValid(false);
         }

      nextInvariantNode = nextInvariantNode->getNextElement();
      }

   nextInvariantNode = invariantNodes->getListHead();
   while (nextInvariantNode)
      {
      if (nextInvariantNode->getData()->_symRef)
         {
         if (nextInvariantNode->getData()->_parent &&
            nextInvariantNode->getData()->_parent->getOpCode().isNullCheck())
            TR::Node::recreate(nextInvariantNode->getData()->_parent, TR::treetop);

         //invariant expressions might be the part of checks we already versioned.
         //if buildXXX already removed the nodes representing the expressions
         //ICM will assert while decrementing their ref counts which might already be 0s
         if (nextInvariantNode->getData()->_node->getReferenceCount() > 0 && nextInvariantNode->getData()->_parent->getReferenceCount()> 0)
            {
            TR::Node *invariantNode = nextInvariantNode->getData()->_node;
            int32_t childNum;

            if(trace())
               traceMsg(comp(), "Replacing node %p with a load of symref %p\n", invariantNode, nextInvariantNode->getData()->_symRef);
            for (childNum=0;childNum<invariantNode->getNumChildren(); childNum++)
               invariantNode->getChild(childNum)->recursivelyDecReferenceCount();
            invariantNode->setNumChildren(0);
            TR::Node::recreate(invariantNode, comp()->il.opCodeForDirectLoad(invariantNode->getDataType()));
            invariantNode->setSymbolReference(nextInvariantNode->getData()->_symRef);
            }
         else
            {
            if (trace())
               traceMsg(comp(), "skipping node %p or its parent %p which were removed as a part of a versioned check (e.g. count < 1)\n", nextInvariantNode->getData()->_node, nextInvariantNode->getData()->_parent);
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
                                               TR::Block *exitGotoBlock, TR::Block *loopInvariantBlock,
                                               TR::SymbolReference **symRefs)
   {
   if (!comp()->getRecompilationInfo())
     return false;

   if (comp()->getJittedMethodSymbol() && // avoid NULL pointer on non-Wcode builds
       comp()->getJittedMethodSymbol()->isNoTemps())
      {
      dumpOptDetails(comp(), "buildSpecializationTree not safe to perform when NOTEMPS enabled\n");
      return false;
      }

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

      vcount_t visitCount = comp()->incVisitCount();
      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, nodeToBeChecked, comparisonTrees, exitGotoBlock, visitCount);

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
            comparisonNode = TR::Node::createif(TR::iflcmpne, highInt, TR::Node::create(duplicateNode, TR::lconst, 0, 0), exitGotoBlock->getEntry());
            }
         else
            {
            comparisonNode = TR::Node::createif(TR::ificmpne, duplicateNode, TR::Node::create(duplicateNode, TR::iconst, 0, value), exitGotoBlock->getEntry());
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

void TR_LoopVersioner::buildConditionalTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::TreeTop> *conditionalTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock, SharedSparseBitVector &reverseBranchInLoops)
   {
   ListElement<TR::TreeTop> *nextTree = conditionalTrees->getListHead();
   while (nextTree)
      {
      TR::TreeTop *conditionalTree = nextTree->getData();
      TR::Node *conditionalNode = conditionalTree->getNode();

      TR::Node *dupThisChild = NULL;

      if (conditionalNode->isTheVirtualGuardForAGuardedInlinedCall() &&
          !conditionalNode->isNonoverriddenGuard()  && !conditionalNode->isHCRGuard() && !conditionalNode->isBreakpointGuard())
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

         if (nextRealNode && !thisChild)
            {
            if (nextRealNode->getOpCode().isCallIndirect())
               thisChild = nextRealNode->getSecondChild();
            else
               thisChild = nextRealNode->getFirstChild();
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
                dupThisChild = isDependentOnInvariant(thisChild);
                if (dupThisChild)
                   {
                   if (!guardInfo)
                      guardInfo = comp()->findVirtualGuardInfo(conditionalNode);
                   TR::Node *invariantThisChild = NULL;
                   if (guardInfo->getTestType() == TR_VftTest)
                      {
                      invariantThisChild = conditionalNode->getFirstChild()->getFirstChild();
                      conditionalNode->getFirstChild()->setAndIncChild(0, dupThisChild->duplicateTree());
                      }
                   else
                      {
                      TR_ASSERT((guardInfo->getTestType() == TR_MethodTest), "Unknown guard type @ node kind %d test %d %p\n", conditionalNode, guardInfo->getKind(), guardInfo->getTestType());
                      invariantThisChild = conditionalNode->getFirstChild()->getFirstChild()->getFirstChild();
                      conditionalNode->getFirstChild()->getFirstChild()->setAndIncChild(0, dupThisChild->duplicateTree());
                      }

                   invariantThisChild->recursivelyDecReferenceCount();
                   }
                else
                   TR_ASSERT(0, "Cannot version with respect to non-invariant guard\n");
                }
             else
                TR_ASSERT(0, "Cannot version with respect to non-invariant guard\n");
             }
         }

      //if (includeComparisonTree)
         {
         vcount_t visitCount = comp()->incVisitCount();
         collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, conditionalNode, comparisonTrees, exitGotoBlock, visitCount);

         if (performTransformation(comp(), "%s Creating test outside loop for checking if %p is conditional\n", OPT_DETAILS_LOOP_VERSIONER, conditionalNode))
            {
            bool changeConditionalToUnconditionalInBothVersions = false;

            //
            // We pulled out from both on first conditional and all the subsequent conditionals we only pull from fast path
            // This exposed a bug where slow path had an unconditional that removed a valid path.
            // Fix is only allowing unconditioning in both if there is single if conditional tree.
            //
            if ((_conditionalTree == conditionalNode) && (conditionalTrees->getSize() == 1))
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
            if(reverseBranchInLoops[conditionalNode->getGlobalIndex()])
               {
               if (trace()) traceMsg(comp(), "Branch reversed for conditional node [%p], destination Frequency %d, fallThrough frequency %d\n", conditionalNode,destBlock->getFrequency(), fallThroughBlock->getFrequency());
               reverseBranch = true;
               }
            bool isInductionVar=false;
            TR::Node *duplicateComparisonNode,* duplicateIndexNode;
            int32_t indexChildIndex=-1;
            bool replaceIndexWithExpr=false;
            if(conditionalNode->isVersionableIfWithMaxExpr() || conditionalNode->isVersionableIfWithMinExpr())
               {
               int32_t loopDrivingInductionVariable = -1;
               int32_t childIndex = -1;
               TR::Node *child = NULL;
               TR::Node *parent=conditionalNode;
               bool isAddition=false;
               bool indVarOccursAsSecondChildOfSub = false;
               int indexReferenceCount=0;
               TR::Node *indexNode=NULL;

               for (int32_t childNum=0;childNum < conditionalNode->getNumChildren(); childNum++)
                  {
                  child=conditionalNode->getChild(childNum);
                  //TR::Node *indexNode=NULL;
                  if (!isExprInvariant(conditionalNode->getChild(childNum)))
                     {
                     if (trace()) traceMsg(comp(), "Versioning if conditional node [%p]\n", conditionalNode);
                     while (child->getOpCode().isAdd() || child->getOpCode().isSub() || child->getOpCode().isMul())
                        {
                        if (child->getSecondChild()->getOpCode().isLoadConst())
                           {
                           parent=child;
                           child = child->getFirstChild();
                           childIndex=0;
                           }
                        else
                           {
                           bool isSecondChildInvariant = isExprInvariant(child->getSecondChild());
                           if (isSecondChildInvariant)
                              {
                              parent=child;
                              child = child->getFirstChild();
                              childIndex=0;
                              }
                            else
                              {
                              bool isFirstChildInvariant = isExprInvariant(child->getFirstChild());
                              if (isFirstChildInvariant)
                                 {
                                 parent=child;
                                 child = child->getSecondChild();
                                 childIndex=1;
                                 }
                              else
                                 {
                                 child= NULL;
                                 break;
                                 }
                              }
                           }
                        }

                     if(!child || !child->getOpCode().hasSymbolReference())
                        {
                        break;
                        }
                     else
                        {
                        int32_t symRefNum = child->getSymbolReference()->getReferenceNumber();
                        ListElement<int32_t> *versionableInductionVar = _versionableInductionVariables.getListHead();
                        while (versionableInductionVar)
                           {
                           loopDrivingInductionVariable = *(versionableInductionVar->getData());
                           if (symRefNum == *(versionableInductionVar->getData()))
                              {
                              if (_additionInfo->get(symRefNum))
                                 isAddition = true;
                              isInductionVar = true;
                              indexNode=child;
                              indexChildIndex=childNum;
                              break;
                              }
                           versionableInductionVar = versionableInductionVar->getNextElement();
                           }
                        if(!isInductionVar)
                           {
                           break;
                           }
                        }
                      if(isInductionVar)
                         indexReferenceCount++;
                     }
                  }

               if(isInductionVar &&
                 ((conditionalNode->isVersionableIfWithMaxExpr() && isAddition) ||
                  (conditionalNode->isVersionableIfWithMinExpr() && !isAddition) ))
                  {
                  replaceIndexWithExpr=true;
                  //replace ind var with max value;
                  TR::SymbolReference *loopDrivingSymRef = indexNode->getSymbolReference();

                  TR::Node * loopLimit =NULL;
                  TR::Node * range = NULL;
                  TR::Node *entryNode = TR::Node::createLoad(conditionalNode, loopDrivingSymRef);
                  TR::Node *storeNode = _storeTrees[indexNode->getSymbolReference()->getReferenceNumber()]->getNode();
                  //int32_t exitValue = exitValue = storeNode->getFirstChild()->getSecondChild()->getInt();

                  loopLimit = _loopTestTree->getNode()->getSecondChild()->duplicateTree(comp());
                  if(isAddition)
                     {
                     range = TR::Node::create(TR::isub, 2, loopLimit,entryNode);
                     }
                  else
                     {
                     range = TR::Node::create(TR::isub, 2, entryNode, loopLimit);
                     }
                  TR::Node *numIterations = NULL;
                  int32_t additiveConstantInStore = 0;
                  TR::Node *valueChild = storeNode->getFirstChild();
                  while (valueChild->getOpCode().isAdd() || valueChild->getOpCode().isSub())
                     {
                     if (valueChild->getOpCode().isAdd())
                        additiveConstantInStore = additiveConstantInStore + valueChild->getSecondChild()->getInt();
                     else
                        additiveConstantInStore = additiveConstantInStore - valueChild->getSecondChild()->getInt();
                     valueChild = valueChild->getFirstChild();
                     }


                  int32_t incrementJ = additiveConstantInStore;
                  int32_t incrementI = incrementJ;

                  if (incrementI < 0)
                     incrementI = -incrementI;
                  if (incrementJ < 0)
                     incrementJ = -incrementJ;
                  TR::Node *incrNode = TR::Node::create(parent, TR::iconst, 0, incrementI);
                  TR::Node *incrJNode = TR::Node::create(parent, TR::iconst, 0, incrementJ);
                  TR::Node *zeroNode = TR::Node::create(parent, TR::iconst, 0, 0);
                  numIterations = TR::Node::create(TR::idiv, 2, range, incrNode);
                  TR::Node *remNode = TR::Node::create(TR::irem, 2, range, incrNode);
                  TR::Node *ceilingNode = TR::Node::create(TR::icmpne, 2, remNode, zeroNode);
                  numIterations = TR::Node::create(TR::iadd, 2, numIterations, ceilingNode);

                  incrementJ = additiveConstantInStore;
                  traceMsg(comp(),"tracing incrementJ 2: %d \n",incrementJ);
                  int32_t condValue = 0;
                  switch (_loopTestTree->getNode()->getOpCodeValue())
                     {
                     case TR::ificmpge:
                        condValue = 1;
                        break;
                     case TR::ificmple:
                        condValue = 1;
                        break;
                     case TR::ificmplt:
                        incrementJ = incrementJ - 1;
                        break;
                     case TR::ificmpgt:
                        incrementJ = incrementJ + 1;
                        break;
                     default:
                     	break;
                     }

                  TR::Node *extraIter = TR::Node::create(TR::icmpeq, 2, remNode, zeroNode);
                  extraIter = TR::Node::create(TR::iand, 2, extraIter,
                  TR::Node::create(parent, TR::iconst, 0, condValue));
                  numIterations = TR::Node::create(TR::iadd, 2, numIterations, extraIter);
                  numIterations = TR::Node::create(TR::imul, 2, numIterations, incrJNode);

                  TR::Node *adjustMaxValue=NULL;
                  if(isAddition)
                     adjustMaxValue = TR::Node::create(parent, TR::iconst, 0, incrementJ+1);
                  else
                     adjustMaxValue = TR::Node::create(parent, TR::iconst, 0, incrementJ);

                  TR::Node *maxValue = NULL;
                  TR::ILOpCodes addOp;
                  if(isAddition)
                     addOp=TR::iadd;
                  else
                     addOp=TR::isub;
                  entryNode = TR::Node::createLoad(conditionalNode, loopDrivingSymRef);

                  maxValue = TR::Node::create(addOp, 2, entryNode, numIterations);
                  maxValue = TR::Node::create(TR::isub, 2, maxValue, adjustMaxValue);

                  TR_ASSERT(parent, "Parent shouldn't be null\n");

                  TR_ASSERT(indexChildIndex>=0, "Index child index should be valid at this point\n");
                  duplicateIndexNode = conditionalNode->getChild(indexChildIndex)->duplicateTree(comp());

                  int visitCount = comp()->incVisitCount();
                  int32_t indexSymRefNum = loopDrivingSymRef->getReferenceNumber();
                  if(duplicateIndexNode->getOpCode().isLoad() &&
                     duplicateIndexNode->getSymbolReference()->getReferenceNumber()==indexSymRefNum)
                     duplicateIndexNode=maxValue;
                  else
                     replaceInductionVariable(conditionalNode, duplicateIndexNode, indexChildIndex, indexSymRefNum, maxValue, visitCount);
                  }
               }

            if(replaceIndexWithExpr)
               {
               bool indexNodeOccursAsSecondChild=indexChildIndex==1;
               if(indexNodeOccursAsSecondChild)
                  {
                  if (!reverseBranch)
                     duplicateComparisonNode = TR::Node::createif(conditionalNode->getOpCodeValue(), conditionalNode->getFirstChild()->duplicateTree(comp()),duplicateIndexNode ,exitGotoBlock->getEntry());
                  else
                     duplicateComparisonNode = TR::Node::createif(conditionalNode->getOpCode().getOpCodeForReverseBranch(), conditionalNode->getFirstChild()->duplicateTree(comp()), duplicateIndexNode, exitGotoBlock->getEntry());
                  }
               else
                  {
                  if (!reverseBranch)
                     duplicateComparisonNode = TR::Node::createif(conditionalNode->getOpCodeValue(),duplicateIndexNode, conditionalNode->getSecondChild()->duplicateTree(comp()), exitGotoBlock->getEntry());
                  else
                     duplicateComparisonNode = TR::Node::createif(conditionalNode->getOpCode().getOpCodeForReverseBranch(), duplicateIndexNode, conditionalNode->getSecondChild()->duplicateTree(comp()), exitGotoBlock->getEntry());
                  }
               }
            else
               {
               if (!reverseBranch)
                  duplicateComparisonNode = TR::Node::createif(conditionalNode->getOpCodeValue(), conditionalNode->getFirstChild()->duplicateTree(comp()), conditionalNode->getSecondChild()->duplicateTree(comp()), exitGotoBlock->getEntry());
               else
                  duplicateComparisonNode = TR::Node::createif(conditionalNode->getOpCode().getOpCodeForReverseBranch(), conditionalNode->getFirstChild()->duplicateTree(comp()), conditionalNode->getSecondChild()->duplicateTree(comp()), exitGotoBlock->getEntry());
               }

            comparisonTrees->add(duplicateComparisonNode);
            dumpOptDetails(comp(), "The node %p has been created for testing if conditional check is required\n", duplicateComparisonNode);
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

            if (duplicateComparisonNode->isMaxLoopIterationGuard())
               {
               duplicateComparisonNode->setIsMaxLoopIterationGuard(false); //for the outer loop its not a maxloop itr guard anymore!
               }

            if (conditionalNode->isTheVirtualGuardForAGuardedInlinedCall())
               {
               TR::Node *callNode = conditionalNode->getVirtualCallNodeForGuard();
               if (callNode)
                  {
                  callNode->resetIsTheVirtualCallNodeForAGuardedInlinedCall();
                  _guardedCalls.add(callNode);
                  }
               }

            TR::Node *constNode = TR::Node::create(conditionalNode, TR::iconst, 0, 0);

            conditionalNode->getFirstChild()->recursivelyDecReferenceCount();
            conditionalNode->setChild(0, constNode);
            constNode->incReferenceCount();

            conditionalNode->getSecondChild()->recursivelyDecReferenceCount();

            if (!reverseBranch)
               constNode = TR::Node::create(conditionalNode, TR::iconst, 0, 1);

            conditionalNode->setChild(1, constNode);
            constNode->incReferenceCount();

            TR::Node::recreate(conditionalNode, TR::ificmpeq);
            conditionalNode->resetIsTheVirtualGuardForAGuardedInlinedCall();

            if (changeConditionalToUnconditionalInBothVersions)
               {
               if (_duplicateConditionalTree->isTheVirtualGuardForAGuardedInlinedCall())
                  {
                  TR::Node *callNode = _duplicateConditionalTree->getVirtualCallNodeForGuard();
                  if (callNode)
                    callNode->resetIsTheVirtualCallNodeForAGuardedInlinedCall();
                  }

               TR::Node *constNode = TR::Node::create(conditionalNode, TR::iconst, 0, 0);

               _duplicateConditionalTree->getFirstChild()->recursivelyDecReferenceCount();
               _duplicateConditionalTree->setChild(0, constNode);
               constNode->incReferenceCount();

               _duplicateConditionalTree->getSecondChild()->recursivelyDecReferenceCount();

               if (!reverseBranch)
                  constNode = TR::Node::create(conditionalNode, TR::iconst, 0, 1);

               _duplicateConditionalTree->setChild(1, constNode);
               constNode->incReferenceCount();

               TR::Node::recreate(_duplicateConditionalTree, TR::ificmpne);
               _duplicateConditionalTree->resetIsTheVirtualGuardForAGuardedInlinedCall();
               }
            }
         }

      nextTree = nextTree->getNextElement();
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

void TR_LoopVersioner::buildSpineCheckComparisonsTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *spineCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock)
   {
   ListElement<TR::TreeTop> *nextTree = spineCheckTrees->getListHead();

   bool isAddition;
   while (nextTree)
      {
      TR::Node *spineCheckNode = nextTree->getData()->getNode();
      TR::Node *arrayBase = spineCheckNode->getChild(1);
      vcount_t visitCount = comp()->incVisitCount();
      bool performSpineCheck = false;

      if (performTransformation(comp(), "%s Creating test outside loop for checking if %p has spine\n", OPT_DETAILS_LOOP_VERSIONER, spineCheckNode))
         {
         performSpineCheck = true;
         TR::Node *contigArrayLength = TR::Node::create(TR::contigarraylength, 1, arrayBase->duplicateTreeForCodeMotion());
         TR::Node *nextComparisonNode = TR::Node::createif(TR::ificmpne, contigArrayLength, TR::Node::create(spineCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());

         comparisonTrees->add(nextComparisonNode);
         if (trace())
            traceMsg(comp(), "Spine versioning -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
         }

      if (performSpineCheck)
         {
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

         bool is64BitTarget = TR::Compiler->target.is64Bit() ? true : false;
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


         TR::TreeTop *prevTree = nextTree->getData()->getPrevTreeTop();
         TR::TreeTop *succTree = nextTree->getData()->getNextTreeTop();

         if (comp()->useCompressedPointers())
            {
            prevTree->join(compressTree);
            compressTree->join(firstNewTree);
            }
         else
            prevTree->join(firstNewTree);

         firstNewTree->join(succTree);
         } //if perform

     nextTree = nextTree->getNextElement();
     }
   }


void TR_LoopVersioner::buildBoundCheckComparisonsTree(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *boundCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, List<TR::TreeTop> *spineCheckTrees, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock, bool reverseBranch)
   {
   ListElement<TR::TreeTop> *nextTree = boundCheckTrees->getListHead();
   bool isAddition;

   while (nextTree)
      {
      isAddition = false;
      TR::Node *boundCheckNode = nextTree->getData()->getNode();

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
                  arrayLengthNode->setAndIncChild(0, dupInvariantArrayObject);
                  arrayObject->recursivelyDecReferenceCount();
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
                  {
                  TR::Node *dupInvariantArrayLen = invariantArrayLen->duplicateTree();
                  boundCheckNode->setAndIncChild(boundChildIndex, dupInvariantArrayLen);
                  arrayLengthNode->recursivelyDecReferenceCount();
                  }
               else
                  TR_ASSERT(0, "Should not be considering bound check for versioning\n");
               }
            else
               TR_ASSERT(0, "Should not be considering bound check for versioning\n");
            }
         }

      vcount_t visitCount = comp()->incVisitCount(); //@TODO: unsafe API/use pattern
      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, boundCheckNode, comparisonTrees, exitGotoBlock, visitCount);
      TR::Node *nextComparisonNode;


      int32_t indexChildIndex = (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 3 : 1;
      TR::Node *indexNode = boundCheckNode->getChild(indexChildIndex);

      bool isIndexInvariant = isExprInvariant(indexNode);

#if 0
      // Create the spine test against the base array.
      //
      if (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK)
         {
         TR::Node *contigArrayLength = arrayLengthNode->duplicateTree();

		 /*  // this can create an uninitialized load that used to be in the loop after an invariant store,
         if (boundCheckNode->getChild(2)->getOpCodeValue() == TR::contigarraylength)
            {
            contigArrayLength = boundCheckNode->getChild(2)->duplicateTree();
            dumpOptDetails(comp(), "Re-using existing contigarraylength %p for spine check\n", contigArrayLength);
            }
         else
            {
            contigArrayLength =
               TR::Node::create(TR::contigarraylength, 1, boundCheckNode->getChild(1)->duplicateTree());
            dumpOptDetails(comp(), "Fabricating contigarraylength %p for spine check\n", contigArrayLength);
            }
         */

         nextComparisonNode = TR::Node::createif(
            TR::ificmpeq,
            contigArrayLength,
            TR::Node::createWithSymRef(boundCheckNode, TR::iconst, 1, 0, 0),
            exitGotoBlock->getEntry());

         if (trace())
            traceMsg(comp(), "Creating spine check comparison %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
         comparisonTrees->add(nextComparisonNode);
         dumpOptDetails(comp(), "The node %p has been created to spine check\n", nextComparisonNode);
         }
#endif

      if (isIndexInvariant)
         {
         bool performFirstBoundCheck = false;
         if (performTransformation(comp(), "%s Creating test outside loop for checking if %p exceeds bounds\n", OPT_DETAILS_LOOP_VERSIONER, boundCheckNode))
            {
            performFirstBoundCheck = true;
            nextComparisonNode = TR::Node::createif(TR::ificmplt, indexNode->duplicateTreeForCodeMotion(), TR::Node::create(boundCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());

            if (trace())
               traceMsg(comp(), "Index invariant in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

	    if (comp()->requiresSpineChecks())
   		findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());

            comparisonTrees->add(nextComparisonNode);
            dumpOptDetails(comp(), "The node %p has been created for testing if exceed bounds\n", nextComparisonNode);
            }

         bool performSecondBoundCheck = false;
         if (performTransformation(comp(), "%s Creating test outside loop for checking if %p exceeds bounds\n", OPT_DETAILS_LOOP_VERSIONER, boundCheckNode))
            {
            performSecondBoundCheck = true;
            if (boundCheckNode->getOpCodeValue() == TR::BNDCHK || boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK)
               nextComparisonNode = TR::Node::createif(TR::ificmpge, indexNode->duplicateTreeForCodeMotion(), arrayLengthNode->duplicateTreeForCodeMotion(), exitGotoBlock->getEntry());
            else
               nextComparisonNode = TR::Node::createif(TR::ificmpgt, indexNode->duplicateTreeForCodeMotion(), arrayLengthNode->duplicateTreeForCodeMotion(), exitGotoBlock->getEntry());

            if (trace())
               traceMsg(comp(), "Index invariant in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

            if (comp()->requiresSpineChecks())
               findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());

           comparisonTrees->add(nextComparisonNode);
            dumpOptDetails(comp(), "The node %p has been created for testing if exceed bounds\n", nextComparisonNode);
            }

        if (performFirstBoundCheck && performSecondBoundCheck)
            {

            TR::TreeTop *prevTree = nextTree->getData()->getPrevTreeTop();
            TR::TreeTop *succTree = nextTree->getData()->getNextTreeTop();


            if (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK)
               {

		       TR::Node::recreate(boundCheckNode, TR::SpineCHK);
		       boundCheckNode->getChild(2)->recursivelyDecReferenceCount();
	           boundCheckNode->setAndIncChild(2, boundCheckNode->getChild(3));
		       boundCheckNode->getChild(3)->recursivelyDecReferenceCount();
	           boundCheckNode->setNumChildren(3);
	  	       //printf("adding spiencheck from versioning bndchk\n");fflush(stdout);
 spineCheckTrees->add(nextTree->getData());
               }
            else
               {
               TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, boundCheckNode->getChild(boundChildIndex)), NULL, NULL);
               TR::TreeTop *secondNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, boundCheckNode->getChild(indexChildIndex)), NULL, NULL);
               prevTree->join(firstNewTree);
               firstNewTree->join(secondNewTree);
               secondNewTree->join(succTree);
               }

            if (boundCheckNode->getOpCodeValue() != TR::SpineCHK)
               boundCheckNode->recursivelyDecReferenceCount();


            if (trace())
               traceMsg(comp(), "Adjusting tree %p\n", nextTree->getData()->getNode());
            }
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
               indexChild = isDependentOnInductionVariable(
                  indexChild,
                  changedIndexSymRefAtSomePoint,
                  isIndexChildMultiplied,
                  mulNode,
                  strideNode,
                  indVarOccursAsSecondChildOfSub);

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

         bool performFirstBoundCheck = performTransformation(comp(), "%s Creating test outside loop for checking if %p exceeds bounds\n", OPT_DETAILS_LOOP_VERSIONER, boundCheckNode);
         bool performSecondBoundCheck = performTransformation(comp(), "%s Creating test outside loop for checking if %p exceeds bounds\n", OPT_DETAILS_LOOP_VERSIONER, boundCheckNode);

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

         if (performFirstBoundCheck)
            {
            if (isSpecialInductionVariable)
               {
               //printf("Found an opportunity for special versioning in method %s\n", comp()->signature());
               nextComparisonNode = TR::Node::createif(TR::ificmplt, boundCheckNode->getChild(indexChildIndex)->duplicateTreeForCodeMotion(), TR::Node::create(boundCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());
               if (trace())
                  traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
               }
            else
               {
               TR::Node *duplicateIndex = boundCheckNode->getChild(indexChildIndex)->duplicateTreeForCodeMotion();

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
                        visitCount = comp()->incVisitCount();
                        replaceInductionVariable(NULL, duplicateIndex, -1, origIndexSymRef->getReferenceNumber(), storeRhs, visitCount);
                        }
                     else
                        duplicateIndex = storeRhs; //storeRhs;
                     }
                  }

               if ((isAddition && !indVarOccursAsSecondChildOfSub) ||
                  (!isAddition && indVarOccursAsSecondChildOfSub))
                  {
                  nextComparisonNode = TR::Node::createif(TR::ificmplt, duplicateIndex, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());
                  nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
                  if (trace())
                     traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
                  }
               else
                  {
                  int32_t boundChildIndex = (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 2 : 0;
                  nextComparisonNode = TR::Node::createif(TR::ificmpge, duplicateIndex, boundCheckNode->getChild(boundChildIndex)->duplicateTreeForCodeMotion(), exitGotoBlock->getEntry());
                  nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                  if (trace())
                     traceMsg(comp(), "Induction variable subed in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
                  }
               }

            if (comp()->requiresSpineChecks())
               findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());
            comparisonTrees->add(nextComparisonNode);
            dumpOptDetails(comp(), "1: The node %p has been created for testing if exceed bounds\n", nextComparisonNode);
            }

         TR::Node *loopLimit = NULL;
         if (performSecondBoundCheck)
            {
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

               if (incrementI < 0)
                  incrementI = -incrementI;
               if (incrementJ < 0)
                  incrementJ = -incrementJ;
               TR::Node *incrNode = TR::Node::create(boundCheckNode, TR::iconst, 0, incrementI);
               TR::Node *incrJNode = TR::Node::create(boundCheckNode, TR::iconst, 0, incrementJ);
               TR::Node *zeroNode = TR::Node::create(boundCheckNode, TR::iconst, 0, 0);
               numIterations = TR::Node::create(TR::idiv, 2, range, incrNode);
               TR::Node *remNode = TR::Node::create(TR::irem, 2, range, incrNode);
               TR::Node *ceilingNode = TR::Node::create(TR::icmpne, 2, remNode, zeroNode);
               numIterations = TR::Node::create(TR::iadd, 2, numIterations, ceilingNode);

               incrementJ = additiveConstantInStore;
               int32_t condValue = 0;
               bool adjustForDerivedIV = false;
               switch (_loopTestTree->getNode()->getOpCodeValue())
                  {
                  case TR::ificmpge:
                     if (reverseBranch) // actually <
                        {
                        if (isLoopDrivingInductionVariable)
                           incrementJ = incrementJ - 1;
                        else //isDerivedInductionVariable
                           adjustForDerivedIV = true;
                        }
                     else
                        condValue = 1;
                     break;
                  case TR::ificmple:
                     if (reverseBranch) // actually >
                        {
                        if (isLoopDrivingInductionVariable)
                           incrementJ = incrementJ + 1;
                        else //isDerivedInductionVariable
                           adjustForDerivedIV = true;
                        }
                     else
                        condValue = 1;
                     break;
                  case TR::ificmplt:
                     if (reverseBranch) // actually >=
                        condValue = 1;
                     else
                        {
                        if (isLoopDrivingInductionVariable)
                           incrementJ = incrementJ - 1;
                        else //isDerivedInductionVariable
                           adjustForDerivedIV = true;
                        }
                     break;
                  case TR::ificmpgt:
                     if (reverseBranch) // actually <=
                        condValue = 1;
                     else
                        {
                        if (isLoopDrivingInductionVariable)
                           incrementJ = incrementJ + 1;
                        else //isDerivedInductionVariable
                           adjustForDerivedIV = true;
                        }
                     break;
                  default:
                  	break;
                  }

               if (adjustForDerivedIV)
                  {
                  if (isAddition)
                     incrementJ = incrementJ - 1;
                  else
                     incrementJ = incrementJ + 1;
                  }

               TR::Node *extraIter = TR::Node::create(TR::icmpeq, 2, remNode, zeroNode);
               extraIter = TR::Node::create(TR::iand, 2, extraIter,
                                           TR::Node::create(boundCheckNode, TR::iconst, 0, condValue));
               numIterations = TR::Node::create(TR::iadd, 2, numIterations, extraIter);
               numIterations = TR::Node::create(TR::imul, 2, numIterations, incrJNode);
               TR::Node *adjustMaxValue;
               if(isIndexChildMultiplied)
                  adjustMaxValue = TR::Node::create(boundCheckNode, TR::iconst, 0, incrementJ+1);
               else
                  adjustMaxValue = TR::Node::create(boundCheckNode, TR::iconst, 0, incrementJ);
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
                   entryNode = TR::Node::createLoad(boundCheckNode, origIndexSymRef);

               maxValue = TR::Node::create(addOp, 2, entryNode, numIterations);

               maxValue = TR::Node::create(TR::isub, 2, maxValue, adjustMaxValue);
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

            int32_t boundChildIndex = (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 2 : 0;
            nextComparisonNode = NULL;
            if (isSpecialInductionVariable)
               {
               nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, boundCheckNode->getChild(indexChildIndex)->duplicateTree(), correctCheckNode->duplicateTree(), exitGotoBlock->getEntry());
               if (trace())
                  traceMsg(comp(), "Special Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

	       if (comp()->requiresSpineChecks())
        	  findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());

               comparisonTrees->add(nextComparisonNode);

               nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, correctCheckNode, boundCheckNode->getChild(boundChildIndex)->duplicateTree(), exitGotoBlock->getEntry());
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
                     if (isLoopDrivingInductionVariable)
                        {
                        if ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmple) ||
                            (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpgt))
                            nextComparisonNode = TR::Node::createif(TR::ifiucmpge, correctCheckNode, boundCheckNode->getChild(boundChildIndex)->duplicateTree(), exitGotoBlock->getEntry());
                        }

                     if (!nextComparisonNode)
                        {
                        if(isIndexChildMultiplied)
                           nextComparisonNode = TR::Node::createif(TR::ifiucmpge, correctCheckNode, boundCheckNode->getChild(boundChildIndex)->duplicateTree(), exitGotoBlock->getEntry());
                        else
                           nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, correctCheckNode, boundCheckNode->getChild(boundChildIndex)->duplicateTree(), exitGotoBlock->getEntry());
                        }
                        nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                     }
                  else
                     {
                     if (isLoopDrivingInductionVariable)
                        {
                        if ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmplt) ||
                            (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpge))
                            nextComparisonNode = TR::Node::createif(TR::ificmple, correctCheckNode, TR::Node::create(boundCheckNode, TR::iconst, 0, -1), exitGotoBlock->getEntry());
                        }

                     if (!nextComparisonNode)
                        nextComparisonNode = TR::Node::createif(TR::ificmplt, correctCheckNode, TR::Node::create(boundCheckNode, TR::iconst, 0, -1), exitGotoBlock->getEntry());

                     nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
                     }

                  if (trace())
                     traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
                  }
               else
                  {
                  if (!indVarOccursAsSecondChildOfSub)
                     {
                     if (isLoopDrivingInductionVariable)
                        {
                        if ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmplt) ||
                            (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpge))
                           nextComparisonNode = TR::Node::createif(TR::ificmple, correctCheckNode, TR::Node::create(boundCheckNode, TR::iconst, 0, -1), exitGotoBlock->getEntry());
                        }

                     if (!nextComparisonNode)
                        nextComparisonNode = TR::Node::createif(TR::ificmplt, correctCheckNode, TR::Node::create(boundCheckNode, TR::iconst, 0, -1), exitGotoBlock->getEntry());

                     nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
                     }
                  else
                     {
                     if (isLoopDrivingInductionVariable)
                        {
                        if ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmple) ||
                            (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpgt))
                            nextComparisonNode = TR::Node::createif(TR::ifiucmpge, correctCheckNode, boundCheckNode->getChild(boundChildIndex)->duplicateTree(), exitGotoBlock->getEntry());
                        }

                     if (!nextComparisonNode)
                        {
                        if(isIndexChildMultiplied)
                           nextComparisonNode = TR::Node::createif(TR::ifiucmpge, correctCheckNode, boundCheckNode->getChild(boundChildIndex)->duplicateTree(), exitGotoBlock->getEntry());
                        else
                           nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, correctCheckNode, boundCheckNode->getChild(boundChildIndex)->duplicateTree(), exitGotoBlock->getEntry());
                        }

                     nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                     }

                  if (trace())
                     traceMsg(comp(), "Induction variable subed in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());
                  }
               }

            if (comp()->requiresSpineChecks())
              findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());

	    comparisonTrees->add(nextComparisonNode);
            dumpOptDetails(comp(), "2: The node %p has been created for testing if exceed bounds\n", nextComparisonNode);

            if (0 && complexButPredictableForm)
               {
               TR::Node *loadNode = TR::Node::createWithSymRef(boundCheckNode, TR::iload, 0, indexSymRef);
               TR::Node *storeConstNode = TR::Node::create(boundCheckNode, TR::iconst, 0, additiveConstantInStore);
               TR::Node *iremNode = TR::Node::create(TR::irem, 2, loadNode, storeConstNode);
               TR::Node *comparedConstNode = TR::Node::create(boundCheckNode, TR::iconst, 0, additiveConstantInStore - additiveConstantInIndex);
               nextComparisonNode = TR::Node::createif(TR::ificmpge, iremNode, comparedConstNode, exitGotoBlock->getEntry());

               if (comp()->requiresSpineChecks())
	          findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());
               comparisonTrees->add(nextComparisonNode);
               }


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
                        visitCount = comp()->incVisitCount();
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
                        nextComparisonNode = TR::Node::createif(TR::ificmpgt, firstChild, secondChild, exitGotoBlock->getEntry());
                     else
                        nextComparisonNode = TR::Node::createif(TR::ificmpge, firstChild, secondChild, exitGotoBlock->getEntry());

                     nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                     }
                  else
                     {
                     if ((_loopTestTree->getNode()->getOpCodeValue() == TR::ificmpge) ||
                         (_loopTestTree->getNode()->getOpCodeValue() == TR::ificmplt))
                        nextComparisonNode = TR::Node::createif(TR::ificmplt, firstChild, secondChild, exitGotoBlock->getEntry());
                     else
                        nextComparisonNode = TR::Node::createif(TR::ificmple, firstChild, secondChild, exitGotoBlock->getEntry());

                     nextComparisonNode->setIsVersionableIfWithMinExpr(comp());
                     }

                  if (trace())
                     traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

                  dumpOptDetails(comp(), "3: The node %p has been created for testing if exceed bounds\n", nextComparisonNode);

	               if (comp()->requiresSpineChecks())
                     findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());
                  comparisonTrees->add(nextComparisonNode);
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
                     visitCount = comp()->incVisitCount();
                     replaceInductionVariable(NULL, duplicateMulNode, -1, indexSymRef->getReferenceNumber(), loopLimit->duplicateTree(), visitCount);
                     visitCount = comp()->incVisitCount();
                     replaceInductionVariable(NULL, duplicateMulHNode, -1, indexSymRef->getReferenceNumber(), loopLimit->duplicateTree(), visitCount);
                     }

                  traceMsg(comp(), " node : %p Loop limit %p )\n",nextComparisonNode,loopLimit);
                  //If its negative
                  nextComparisonNode = TR::Node::createif(TR::ificmplt, duplicateMulNode, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());
                  nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                  if (comp()->requiresSpineChecks())
                     findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());

                  comparisonTrees->add(nextComparisonNode);
                  if (trace())
                     traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

                  TR::Node::recreate(duplicateMulHNode,TR::imulh);
                  nextComparisonNode = TR::Node::createif(TR::ifiucmpgt, duplicateMulHNode, TR::Node::create(boundCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());
                  nextComparisonNode->setIsVersionableIfWithMaxExpr(comp());
                  if (comp()->requiresSpineChecks())
                     findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());
                  comparisonTrees->add(nextComparisonNode);
                  if (trace())
                     traceMsg(comp(), "Induction variable added in each iter -> Creating %p (%s)\n", nextComparisonNode, nextComparisonNode->getOpCode().getName());

                  //Adding multiplicative factor greater than zero check for multiplicative BNDCHKS a.i+b; a>0
                  nextComparisonNode = TR::Node::createif(TR::ificmple, strideNode->duplicateTree(), TR::Node::create(boundCheckNode, TR::iconst, 0, 0), exitGotoBlock->getEntry());
                  if (comp()->requiresSpineChecks())
                     findAndReplaceContigArrayLen(NULL, nextComparisonNode ,comp()->incVisitCount());
                  comparisonTrees->add(nextComparisonNode);
                  }
               }
            }

         if (performFirstBoundCheck && performSecondBoundCheck)
            {
            TR::TreeTop *prevTree = nextTree->getData()->getPrevTreeTop();
            TR::TreeTop *succTree = nextTree->getData()->getNextTreeTop();

            if (boundCheckNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK)
               {
			TR::Node::recreate(boundCheckNode, TR::SpineCHK);
			boundCheckNode->getChild(2)->recursivelyDecReferenceCount();
	        boundCheckNode->setAndIncChild(2, boundCheckNode->getChild(3));
			boundCheckNode->getChild(3)->recursivelyDecReferenceCount();
	        boundCheckNode->setNumChildren(3);
			//printf("adding spiencheck from versioning bndchk\n");fflush(stdout);
 spineCheckTrees->add(nextTree->getData());


               }
            else
               {
               TR::TreeTop *firstNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, boundCheckNode->getChild(boundChildIndex)), NULL, NULL);
               TR::TreeTop *secondNewTree = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, boundCheckNode->getChild(indexChildIndex)), NULL, NULL);
               prevTree->join(firstNewTree);
               firstNewTree->join(secondNewTree);
               secondNewTree->join(succTree);
               }

            if(boundCheckNode->getOpCodeValue() != TR::SpineCHK)
               boundCheckNode->recursivelyDecReferenceCount();

            if (trace())
               traceMsg(comp(), "Adjusting tree %p\n", nextTree->getData()->getNode());
            }
         }
      nextTree = nextTree->getNextElement();
      }
   }

void TR_LoopVersioner::collectAllExpressionsToBeChecked(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, TR::Node *node, List<TR::Node> *comparisonTrees, TR::Block *exitGotoBlock, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      // Do not process the load or store child of a BNDCHKwithSpineCHK node because we are
      // already generating check trees for it as part of the topmost bound check node.
      //
      if (node->getOpCodeValue() == TR::BNDCHKwithSpineCHK && i == 0)
         continue;

      collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, node->getChild(i), comparisonTrees, exitGotoBlock, visitCount);
      }

   // If this is an indirect access
   //
   if (node->isInternalPointer() ||
       (((node->getOpCode().isIndirect() && node->getOpCode().hasSymbolReference() && !node->getSymbolReference()->getSymbol()->isStatic()) || node->getOpCode().isArrayLength()) &&
        !node->getFirstChild()->isInternalPointer()))
      {
      if (!node->getFirstChild()->isThisPointer() && performTransformation(comp(), "%s Creating test outside loop for checking if %p is null\n", OPT_DETAILS_LOOP_VERSIONER, node->getFirstChild()))
         {
         ListElement<TR::TreeTop> *nullCheckTree = nullCheckTrees->getListHead();

         // Null check the pointer being dereferenced; also
         // get rid of the corresponding TR::NULLCHK if it exists
         //
         bool eliminatedNullCheck = false;
         while (nullCheckTree)
            {
            TR::Node *nullCheckNode = nullCheckTree->getData()->getNode();
            if (nullCheckNode->getOpCode().isNullCheck())
               {
               if (nullCheckNode->getNullCheckReference() == node->getFirstChild())
                  {
                  if (nullCheckTree->getData()->getNode()->getOpCodeValue() == TR::NULLCHK)
                     TR::Node::recreate(nullCheckTree->getData()->getNode(), TR::treetop);
                  else if (nullCheckTree->getData()->getNode()->getOpCodeValue() == TR::ResolveAndNULLCHK)
                     TR::Node::recreate(nullCheckTree->getData()->getNode(), TR::ResolveCHK);
                  eliminatedNullCheck = true;
                  break;
                  }
               }

              nullCheckTree = nullCheckTree->getNextElement();
            }

         if (!eliminatedNullCheck)
            {
            nullCheckTree = _checksInDupHeader.getListHead();
            while (nullCheckTree)
               {
               TR::Node *nullCheckNode = nullCheckTree->getData()->getNode();
               if (nullCheckNode->getOpCode().isNullCheck())
                  {
                  if (nullCheckNode->getNullCheckReference() == node->getFirstChild())
                     {
                     if (nullCheckTree->getData()->getNode()->getOpCodeValue() == TR::NULLCHK)
                        TR::Node::recreate(nullCheckTree->getData()->getNode(), TR::treetop);
                     else if (nullCheckTree->getData()->getNode()->getOpCodeValue() == TR::ResolveAndNULLCHK)
                        TR::Node::recreate(nullCheckTree->getData()->getNode(), TR::ResolveCHK);
                     eliminatedNullCheck = true;
                     break;
                     }
                  }
               nullCheckTree = nullCheckTree->getNextElement();
               }
            }


         TR::Node *duplicateNullCheckReference = node->getFirstChild()->duplicateTreeForCodeMotion();
         TR::Node *ifacmpeqNode =  TR::Node::createif(TR::ifacmpeq, duplicateNullCheckReference, TR::Node::aconst(node, 0),  exitGotoBlock->getEntry());
         comparisonTrees->add(ifacmpeqNode);
         dumpOptDetails(comp(), "The node %p has been created for testing if null check is required\n", ifacmpeqNode);
         }

      TR::Node *firstChild = node->getFirstChild();
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
               // At this point it would be difficult to prevent the
               // transformation responsible for hoisting this load.
               traceMsg(
                  comp(),
                  "failed to find class from field #%d\n",
                  otherSymRef->getReferenceNumber());
               comp()->failCompilation<TR::CompilationException>(
                  "failed to find class from field during versioning");
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
            dumpOptDetails(comp(), "%s Creating test outside loop for checking if %p is of the correct type\n", OPT_DETAILS_LOOP_VERSIONER, node->getFirstChild());
            TR::Node *duplicateCheckedValue = node->getFirstChild()->duplicateTreeForCodeMotion();
            TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, duplicateCheckedValue,  duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
            TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(node, TR::iconst, 0, 0), exitGotoBlock->getEntry());
            comparisonTrees->add(ificmpeqNode);
            dumpOptDetails(comp(), "The node %p has been created for testing if object is of the right type\n", ificmpeqNode);
            //printf("The node %p has been created for testing if object is of the right type in %s\n", ificmpeqNode, comp()->signature());
            //fflush(stdout);
            }
         else if (testIsArray)
            {
#ifdef J9_PROJECT_SPECIFIC
            dumpOptDetails(comp(), "%s Creating test outside loop for checking if %p is of array type\n", OPT_DETAILS_LOOP_VERSIONER, node->getFirstChild());
            TR::Node *duplicateCheckedValue = node->getFirstChild()->duplicateTreeForCodeMotion();
            TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1, duplicateCheckedValue, comp()->getSymRefTab()->findOrCreateVftSymbolRef());
            //TR::Node *componentTypeLoad = TR::Node::create(TR::aloadi, 1, vftLoad, comp()->getSymRefTab()->findOrCreateArrayComponentTypeSymbolRef());
            TR::Node *classFlag = NULL;
            if (TR::Compiler->target.is32Bit())
               {
               classFlag = TR::Node::createWithSymRef(TR::iloadi, 1, 1, vftLoad, comp()->getSymRefTab()->findOrCreateClassAndDepthFlagsSymbolRef());
               }
            else
               {
               classFlag = TR::Node::createWithSymRef(TR::lloadi, 1, 1, vftLoad, comp()->getSymRefTab()->findOrCreateClassAndDepthFlagsSymbolRef());
               classFlag = TR::Node::create(TR::l2i, 1, classFlag);
               }
            TR::Node *andConstNode = TR::Node::create(classFlag, TR::iconst, 0, TR::Compiler->cls.flagValueForArrayCheck(comp()));
            TR::Node * andNode   = TR::Node::create(TR::iand, 2, classFlag, andConstNode);
            TR::Node *cmp = TR::Node::createif(TR::ificmpne, andNode, andConstNode, exitGotoBlock->getEntry());
            comparisonTrees->add(cmp);
            dumpOptDetails(comp(), "The node %p has been created for testing if object is of array type\n", cmp);
            //printf("The node %p has been created for testing if object is of array type in %s\n", cmp, comp()->signature());
            //fflush(stdout);
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
  else if (node->getOpCode().isIndirect() && node->getFirstChild()->isInternalPointer() && performTransformation(comp(), "%s Creating test outside loop for checking if %p requires bound check\n", OPT_DETAILS_LOOP_VERSIONER, node))
      {
      // This is an array access; so we need to insert explicit
      // checks to mimic the bounds check for the access.
      //
      TR::Node *offset = node->getFirstChild()->getSecondChild();
      TR::Node *childInRequiredForm = NULL;
      TR::Node *duplicateIndex = NULL;

      int32_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
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

      TR::DataType type = offset->getType();

      // compute the right shift width
      //
      int32_t dataWidth = TR::Symbol::convertTypeToSize(node->getDataType());
      if (comp()->useCompressedPointers() &&
            node->getDataType() == TR::Address)
         dataWidth = TR::Compiler->om.sizeofReferenceField();
      int32_t shiftWidth = TR::TransformUtil::convertWidthToShift(dataWidth);

      if (childInRequiredForm)
         {
         if (childInRequiredForm->getOpCodeValue() == TR::ishl || childInRequiredForm->getOpCodeValue() == TR::lshl)
            {
            if (childInRequiredForm->getSecondChild()->getOpCode().isLoadConst() &&
               (childInRequiredForm->getSecondChild()->getInt() == shiftWidth))
               duplicateIndex = childInRequiredForm->getFirstChild()->duplicateTreeForCodeMotion();
            }
         else if (childInRequiredForm->getOpCodeValue() == TR::imul || childInRequiredForm->getOpCodeValue() == TR::lmul)
            {
            if (childInRequiredForm->getSecondChild()->getOpCode().isLoadConst() &&
               (childInRequiredForm->getSecondChild()->getInt() == dataWidth))
               duplicateIndex = childInRequiredForm->getFirstChild()->duplicateTreeForCodeMotion();
            }
         }


     if (!duplicateIndex)
         {
         if (!childInRequiredForm)
            {
            TR::Node *constNode = TR::Node::create(node, type.isInt32() ? TR::iconst : TR::lconst, 0, 0);
            duplicateIndex = TR::Node::create(type.isInt32() ? TR::isub : TR::lsub, 2, offset->duplicateTreeForCodeMotion(), constNode);
            if (type.isInt64())
               constNode->setLongInt((int64_t)headerSize);
            else
               constNode->setInt((int64_t)headerSize);
            }
         else
            duplicateIndex = childInRequiredForm->duplicateTreeForCodeMotion();


         duplicateIndex = TR::Node::create(type.isInt32() ? TR::iushr : TR::lushr, 2, duplicateIndex,
                                          TR::Node::create(node, TR::iconst, 0, shiftWidth));
         }

      TR::Node *duplicateBase = node->getFirstChild()->getFirstChild()->duplicateTreeForCodeMotion();
      TR::Node *arrayLengthNode = TR::Node::create(TR::arraylength, 1, duplicateBase);

      arrayLengthNode->setArrayStride(dataWidth);
      if (type.isInt64())
         arrayLengthNode = TR::Node::create(TR::i2l, 1, arrayLengthNode);

      TR::Node *ificmpgeNode = TR::Node::createif(type.isInt32() ? TR::ificmpge : TR::iflcmpge, duplicateIndex, arrayLengthNode, exitGotoBlock->getEntry());
      comparisonTrees->add(ificmpgeNode);
      TR::Node *constNode = 0;
      if(type.isInt32())
         constNode = TR::Node::create(arrayLengthNode, TR::iconst , 0, 0);
      else
         constNode = TR::Node::create(arrayLengthNode, TR::lconst, 0, 0);
      // Note: duplicateIndex was already duplicated for code motion, we don't need to duplicate the duplicate for code motion
      TR::Node *ificmpltNode = TR::Node::createif(type.isInt32() ? TR::ificmplt : TR::iflcmplt, duplicateIndex->duplicateTree(), constNode, exitGotoBlock->getEntry());
      if (type.isInt64())
         ificmpltNode->getSecondChild()->setLongInt(0);
      comparisonTrees->add(ificmpltNode);
      dumpOptDetails(comp(), "The node %p has been created for testing if bounds check is required\n", ificmpgeNode);
      dumpOptDetails(comp(), "The node %p has been created for testing if bounds check is required\n", ificmpltNode);
      }
   else if (((node->getOpCodeValue() == TR::idiv) || (node->getOpCodeValue() == TR::irem) || (node->getOpCodeValue() == TR::lrem) || (node->getOpCodeValue() == TR::ldiv)) && performTransformation(comp(), "%s Creating test outside loop for checking if %p is divide by zero\n", OPT_DETAILS_LOOP_VERSIONER))
      {
      // This is a divide; so we need to insert explicit checks
      // to mimic the div check.
      //
      // Get rid of the coresponding TR::DIVCHK node if there was one
      //
      ListElement<TR::TreeTop> *divCheckTree = divCheckTrees->getListHead();
      bool eliminatedDivCheck = false;
      while (divCheckTree)
         {
         TR::Node *divCheckNode = divCheckTree->getData()->getNode();
         if (divCheckNode->getOpCodeValue() == TR::DIVCHK)
            {
            if (divCheckNode->getFirstChild() == node)
               {
               TR::Node::recreate(divCheckTree->getData()->getNode(), TR::treetop);
               eliminatedDivCheck = true;
               break;
               }
            }

         divCheckTree = divCheckTree->getNextElement();
         }

     if (!eliminatedDivCheck)
        {
        divCheckTree = _checksInDupHeader.getListHead();
        while (divCheckTree)
           {
           TR::Node *divCheckNode = divCheckTree->getData()->getNode();
           if (divCheckNode->getOpCodeValue() == TR::DIVCHK)
              {
              if (divCheckNode->getFirstChild() == node)
                 {
                 TR::Node::recreate(divCheckTree->getData()->getNode(), TR::treetop);
                 break;
                 }
              }

           divCheckTree = divCheckTree->getNextElement();
           }
   }


      TR::Node *duplicateDivisor = node->getSecondChild()->duplicateTreeForCodeMotion();
      TR::Node *ifNode;
      if (duplicateDivisor->getType().isInt64())
         ifNode =  TR::Node::createif(TR::iflcmpeq, duplicateDivisor, TR::Node::create(node, TR::lconst, 0, 0), exitGotoBlock->getEntry());
      else
         ifNode =  TR::Node::createif(TR::ificmpeq, duplicateDivisor, TR::Node::create(node, TR::iconst, 0, 0), exitGotoBlock->getEntry());
      comparisonTrees->add(ifNode);
      dumpOptDetails(comp(), "The node %p has been created for testing if div check is required\n", ifNode);
      }
   else if (node->getOpCode().isCheckCast()) //(node->getOpCodeValue() == TR::checkcast)
      {
      // This is a checkcast node; so we need to insert the
      // corresponding instanceof check.
      //
      ListElement<TR::TreeTop> *checkCastTree = checkCastTrees->getListHead();
      while (checkCastTree)
         {
         TR::Node *checkCastNode = checkCastTree->getData()->getNode();
         if (checkCastNode->getOpCode().isCheckCast()) //(checkCastNode->getOpCodeValue() == TR::checkcast)
            {
            if ((checkCastNode == node) && performTransformation(comp(), "%s Creating test outside loop for checking if %p is casted\n", OPT_DETAILS_LOOP_VERSIONER, node))
               {
               TR::Node *duplicateClassPtr = node->getSecondChild()->duplicateTreeForCodeMotion();
               TR::Node *duplicateCheckedValue = node->getFirstChild()->duplicateTreeForCodeMotion();
               if (node->getOpCodeValue() == TR::checkcastAndNULLCHK)
                  {
                  TR::Node *duplicateNullCheckReference = node->getFirstChild()->duplicateTreeForCodeMotion();
                  TR::Node *ifacmpeqNode =  TR::Node::createif(TR::ifacmpeq, duplicateNullCheckReference, TR::Node::aconst(node, 0),  exitGotoBlock->getEntry());
                  comparisonTrees->add(ifacmpeqNode);
                  dumpOptDetails(comp(), "The node %p has been created for testing if null check is required\n", ifacmpeqNode);
                  }

              TR::Node *instanceofNode = TR::Node::createWithSymRef(TR::instanceof, 2, 2, duplicateCheckedValue,  duplicateClassPtr, comp()->getSymRefTab()->findOrCreateInstanceOfSymbolRef(comp()->getMethodSymbol()));
               TR::Node *ificmpeqNode =  TR::Node::createif(TR::ificmpeq, instanceofNode, TR::Node::create(node, TR::iconst, 0, 0), exitGotoBlock->getEntry());
               comparisonTrees->add(ificmpeqNode);
               dumpOptDetails(comp(), "The node %p has been created for testing if checkcast is required\n", ificmpeqNode);
               break;
               }
            }

         checkCastTree = checkCastTree->getNextElement();
         }
      }
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

bool TR_LoopVersioner::boundCheckUsesUnchangedValue(TR::TreeTop *bndCheckTree, TR::Node *node, TR::SymbolReference *symRef, TR_RegionStructure *loopStructure)
   {
   TR::Block *currentBlock = bndCheckTree->getEnclosingBlock();
   TR::Block *entryBlock = loopStructure->asRegion()->getEntryBlock();
   // needAuxiliaryWalk flag is not needed whenever the walk of the trees
   // ends at the bndCheckTree (refer to 138095 for some notes)
   //
   if (currentBlock == entryBlock)
      {
      if (!findStore(currentBlock->getEntry(), bndCheckTree, node, symRef))
         return true;
      else
         return false;
      }

   if (currentBlock == _loopTestBlock)
      {
      if (findStore(bndCheckTree, currentBlock->getExit(), node, symRef, true))
         return true;
      else
         return false;
      }

   if ((currentBlock->getSuccessors().size() == 1) &&
       (currentBlock->getSuccessors().front()->getTo() == _loopTestBlock))
      {
      if (findStore(bndCheckTree, currentBlock->getExit(), node, symRef, true) ||
          findStore(_loopTestBlock->getEntry(), _loopTestBlock->getExit(), node, symRef, true))
         return true;
      else
         return false;
      }

   // check for a straight-line path to the current block from the entry block
   //
   if (entryBlock->getSuccessors().size() == 1)
      {
      TR::Block *succBlock = entryBlock->getSuccessors().front()->getTo()->asBlock();
      bool searchForStore = false;
      if (currentBlock == succBlock)
         searchForStore = true;

      if (!searchForStore)
         {
         // skip over goto blocks if any
         //
         if ((succBlock->getSuccessors().size() == 1) &&
               succBlock->getSuccessors().front()->getTo() == currentBlock)
            {
            if (!findStore(succBlock->getEntry(), succBlock->getExit(), node, symRef))
               searchForStore = true;
            }
         }

      if (searchForStore)
         {
         if (!findStore(currentBlock->getEntry(), bndCheckTree, node, symRef))
            return true;
         }
      }

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   loopStructure->getBlocks(&blocksInRegion);

   bool flag = true;
   TR::Block *cursorBlock = currentBlock;
   TR_ScratchList<TR::Block> innerLoopEntries(trMemory());
   while (flag)
      {
      flag = false;
      // check if cursorBlock is the entry of an inner loop or an improper region
      // if it is the start of an improper region, exit the walk
      // if it is the start of an inner loop, add to the list. if already added,
      // this means that the block is being analyzed for the second time, and the
      // walk needs to terminate ; otherwise there is a chance of an infinite loop
      //
      TR_Structure *cursorStruct = cursorBlock->getStructureOf();
      TR_RegionStructure *parent = cursorStruct->getParent()->asRegion();
      if (parent && (parent != loopStructure))
         {
         if (parent->isNaturalLoop()) // inner loop detected
            {
            if (cursorBlock == parent->getEntryBlock())
               {
               if (!innerLoopEntries.find(cursorBlock))
                  innerLoopEntries.add(cursorBlock);
               else
                  break; // terminate
               }
            }
         else if (!parent->isAcyclic()) // improper
            break;
         }

      if (cursorBlock == currentBlock)
         {
         if (findStore(bndCheckTree, cursorBlock->getExit(), node, symRef, true))
            return true;
         }
      else
         {
         if (findStore(cursorBlock->getEntry(), cursorBlock->getExit(), node, symRef))
            return true;
         }

      if ((cursorBlock == _loopTestBlock) ||
          (cursorBlock == entryBlock))
         break;

      TR::Block *onlySuccInsideLoop = NULL;
      for (auto succ = cursorBlock->getSuccessors().begin(); succ != cursorBlock->getSuccessors().end(); ++succ)
         {
         TR::Block *succBlock = (*succ)->getTo()->asBlock();
         if (blocksInRegion.find(succBlock))
            {
            if (!onlySuccInsideLoop)
               onlySuccInsideLoop = succBlock;
            else
               {
               onlySuccInsideLoop = NULL;
               break;
               }
            }
         }
      if (onlySuccInsideLoop)
         {
         cursorBlock = onlySuccInsideLoop;
         flag = true;
         }
      }



   if ((cursorBlock == _loopTestBlock) ||
       (cursorBlock == entryBlock))
      return false;

   flag = true;
   cursorBlock = currentBlock;
   innerLoopEntries.deleteAll();
   while (flag)
      {
      flag = false;

      TR_Structure *cursorStruct = cursorBlock->getStructureOf();
      TR_RegionStructure *parent = cursorStruct->getParent()->asRegion();
      if (parent && (parent != loopStructure))
         {
         if (parent->isNaturalLoop()) // inner loop detected
            {
            if (cursorBlock == parent->getEntryBlock())
               {
               if (!innerLoopEntries.find(cursorBlock))
                  innerLoopEntries.add(cursorBlock);
               else
                  break; // terminate
               }
            }
         else if (!parent->isAcyclic()) // improper
            break;
         }

      if (cursorBlock == currentBlock)
         {
         if (findStore(cursorBlock->getEntry(), bndCheckTree, node, symRef))
            return false;
         }
      else
         {
         if (findStore(cursorBlock->getEntry(), cursorBlock->getExit(), node, symRef))
            return false;
         }

      if ((cursorBlock == _loopTestBlock) ||
          (cursorBlock == entryBlock))
         break;

      TR::Block *predInsideLoop = NULL;
      for (auto pred = cursorBlock->getPredecessors().begin(); pred != cursorBlock->getPredecessors().end(); ++pred)
         {
         TR::Block *predBlock = (*pred)->getFrom()->asBlock();
         if (blocksInRegion.find(predBlock))
            {
            if (!predInsideLoop)
               predInsideLoop = predBlock;
            else
               {
               predInsideLoop = NULL;
               break;
               }
            }
         }

      if (predInsideLoop)
         {
         TR::Block *onlySuccInsideLoop = NULL;
         for (auto succ = predInsideLoop->getSuccessors().begin(); succ != predInsideLoop->getSuccessors().end(); ++succ)
            {
            TR::Block *succBlock = (*succ)->getTo()->asBlock();
            if (blocksInRegion.find(succBlock))
               {
               if (!onlySuccInsideLoop)
                  onlySuccInsideLoop = succBlock;
               else
                  {
                  onlySuccInsideLoop = NULL;
                  break;
                  }
               }
            }
         if (!onlySuccInsideLoop)
            predInsideLoop = NULL;
         }

      if (predInsideLoop)
         {
         cursorBlock = predInsideLoop;
         flag = true;
         }
      }

   if ((cursorBlock == _loopTestBlock) ||
       (cursorBlock == entryBlock))
      return true;

   TR::TreeTop *storeTree = _storeTrees[symRef->getReferenceNumber()];
   if (storeTree)
      {
      TR::Block *storeBlock = storeTree->getEnclosingBlock();
      bool atEntry = false;
      if (blockIsAlwaysExecutedInLoop(storeBlock, loopStructure, &atEntry))
         {
         if (!atEntry)
            return true;
         else
            return false;
         }
      }

   return false;
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

   if ((nodeCount/(MAX_SIZE_INCREASE_FACTOR/hotnessFactor)) > (_origNodeCount/nodeCountFactor))
      {
      traceMsg(comp(), "Failing node count %d orig %d factor %d\n", nodeCount, _origNodeCount, nodeCountFactor);
      return -2;
      }

   if ((comp()->getFlowGraph()->getNodes().getSize()/(MAX_SIZE_INCREASE_FACTOR/hotnessFactor)) > (_origBlockCount/blockCountFactor))
      {
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
                   //dumpOptDetails(comp(), "1Store of %d in reqd form %d\n", nextInductionVariableNumber, storeInRequiredForm);
                   if (storeInRequiredForm)
                      {
                      TR::Block *currentBlock = _storeTrees[nextInductionVariableNumber]->getEnclosingBlock();
                      if (!blockIsAlwaysExecutedInLoop(currentBlock, regionStructure))
                         storeInRequiredForm = false;
                      if (currentBlock->getStructureOf()->getContainingLoop() != regionStructure)
                         storeInRequiredForm = false;
                      }
                   //dumpOptDetails(comp(), "2Store of %d in reqd form %d\n", nextInductionVariableNumber, storeInRequiredForm);
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
