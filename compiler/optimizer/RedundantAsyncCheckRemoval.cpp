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

#include "optimizer/RedundantAsyncCheckRemoval.hpp"

#include <algorithm>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/AsyncCheckInsertion.hpp"

#define OPT_DETAILS "O^O REDUNDANT ASYNC CHECK REMOVAL: "
#define SHORT_RUNNING_LOOP 1
#define SHORT_RUNNING_LOOP_BOUND 20000
#define NUMBER_OF_NODES_IN_LARGE_METHOD 2000
#define NUMBER_OF_NODES_IN_LARGE_WARM_ACYCLIC_METHOD 200

// The following macro(s) works on StructureSubGraphNodes
#define GET_ASYNC_INFO(x)  ((AsyncInfo*)(((x)->getStructure())->getAnalysisInfo()))

// Find a loop that this regoin is contained in
//
static TR_RegionStructure *getOuterLoop(TR_RegionStructure *region)
   {
   if (region->getParent() == 0)
      return 0;
   TR_RegionStructure *parent = region->getParent()->asRegion();
   if (parent->isNaturalLoop())
      return parent;
   return getOuterLoop(parent);
   }

static TR_RegionStructure *getOuterImproperRegion(TR_Structure *region)
   {
   if (region->getParent() == 0)
      return 0;
   TR_RegionStructure *parent = region->getParent()->asRegion();
   if (parent->containsInternalCycles())
      return parent;
   return getOuterImproperRegion(parent);
   }


static inline int32_t ceiling(int32_t numer, int32_t denom)
   {
   return (numer % denom == 0) ?
      numer / denom :
      numer / denom + 1;
   }


TR_RedundantAsyncCheckRemoval::TR_RedundantAsyncCheckRemoval(TR::OptimizationManager *manager)
   : TR::Optimization(manager), _ancestors(trMemory())
   {}

bool TR_RedundantAsyncCheckRemoval::shouldPerform()
   {
   // Don't run when profiling
   //
   if (comp()->getProfilingMode() == JitProfiling || comp()->generateArraylets())
      return false;


   // It is not safe to add an asynccheck under involuntary OSR
   // as a transition may have to occur at the added point and the
   // required infrastructure may not exist
   //
   if (comp()->getOption(TR_EnableOSR) && comp()->getOSRMode() == TR::involuntaryOSR)
      return false;

   return true;
   }

int32_t TR_RedundantAsyncCheckRemoval::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
      comp()->dumpMethodTrees("Before analysis:");

   _asyncCheckInCurrentLoop = false;
   _numAsyncChecksInserted  = 0;
   _foundShortRunningLoops  = false;
   _cfg = comp()->getFlowGraph();

   // If this is a large acyclic method - add a yield point at each return from this method
   // so that sampling will realize that we are actually in this method.
   //
   // Under (voluntary) OSR, it's safe to insert asynccheck immediately before
   // return, but not necessarily elsewhere.
   if (comp()->getMethodHotness() <= warm
       || !comp()->mayHaveLoops()
       || comp()->getOption(TR_EnableOSR))
      {
      static const char *p;
      static uint32_t numNodesInLargeMethod     = (p = feGetEnv("TR_LargeMethodNodes"))     ? atoi(p) : NUMBER_OF_NODES_IN_LARGE_METHOD;
      if (!p && 0)
         {
         if (comp()->getMethodHotness() <= warm &&
             !comp()->mayHaveLoops())
            numNodesInLargeMethod = NUMBER_OF_NODES_IN_LARGE_WARM_ACYCLIC_METHOD;
         }

      if ((uint32_t) comp()->getNodeCount() > numNodesInLargeMethod ||
          comp()->getLoopWasVersionedWrtAsyncChecks())
         {
         _numAsyncChecksInserted += TR_AsyncCheckInsertion::insertReturnAsyncChecks(this,
            "redundantAsyncCheckRemoval/returns");
         }

      return 1;
      }

   initialize(_cfg->getStructure());

   comp()->incVisitCount();
   int32_t count = perform(_cfg->getStructure());

   // If all the loops are short running in the method, and we have
   // resulted in the removal of all async checks - then sampling might
   // be effected.  Insert async checks on method exits.
   //
   if ((comp()->getMethodHotness() < scorching) &&	// make sure we don't add unnecessary checks at scorching
       (comp()->getLoopWasVersionedWrtAsyncChecks() ||
       (_numAsyncChecksInserted == 0 && _foundShortRunningLoops &&
        comp()->getRecompilationInfo() &&
#ifdef J9_PROJECT_SPECIFIC
        comp()->getRecompilationInfo()->useSampling() &&
#endif
        comp()->getRecompilationInfo()->shouldBeCompiledAgain())))
      {
      _numAsyncChecksInserted += TR_AsyncCheckInsertion::insertReturnAsyncChecks(this,
         "redundantAsyncCheckRemoval/returns");
      }

   if (trace())
      comp()->dumpMethodTrees("After analysis:");

   return count;
   }



int32_t TR_RedundantAsyncCheckRemoval::perform(TR_Structure *str, bool insideImproperRegion)
   {
   TR_RegionStructure *region = str->asRegion();
   if (region == 0)
      return processBlockStructure(str->asBlock());

   bool origAsyncCheckFlag = _asyncCheckInCurrentLoop;

   if (region->containsInternalCycles())
      {
      int32_t retValue = processImproperRegion(region);
      if (origAsyncCheckFlag)
         _asyncCheckInCurrentLoop = true;
      return retValue;
      }

   bool asyncCheckFlag = false;
   if (region->isNaturalLoop())
      _asyncCheckInCurrentLoop = false;

   TR_RegionStructure::Cursor it(*region);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node != 0; node = it.getNext())
      {
      perform(node->getStructure());
      if (_asyncCheckInCurrentLoop)
         asyncCheckFlag = true;

      if (trace())
         traceMsg(comp(), "sub node %d flag %d\n", node->getNumber(), asyncCheckFlag);

      if (region->isNaturalLoop())
         _asyncCheckInCurrentLoop = false;
      }

   if (region->isNaturalLoop())
      {
      if (trace())
         traceMsg(comp(), "region %d flag %d\n", region->getNumber(), asyncCheckFlag);
      _asyncCheckInCurrentLoop = asyncCheckFlag;
      int32_t retValue = processNaturalLoop(region, insideImproperRegion);
      if (asyncCheckFlag || origAsyncCheckFlag)
         _asyncCheckInCurrentLoop = true;
      else
         _asyncCheckInCurrentLoop = false;
      return retValue;
      }

   _asyncCheckInCurrentLoop = asyncCheckFlag;
   int32_t retValue = processAcyclicRegion(region);
   if (asyncCheckFlag || origAsyncCheckFlag)
      _asyncCheckInCurrentLoop = true;
   else
      _asyncCheckInCurrentLoop = false;
   return retValue;
   }



// Initializes the AsyncInfo analysis objects for each of the structures,
// ignoring any improper regoins
//
void TR_RedundantAsyncCheckRemoval::initialize(TR_Structure *str)
   {
   AsyncInfo *info = new (trStackMemory()) AsyncInfo(trMemory());
   info->setVisitMarker(0);
   str->setAnalysisInfo(info);

   TR_RegionStructure *region = str->asRegion();
   if (region == 0)
      return;
   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
      initialize(node->getStructure());
   }

int32_t TR_RedundantAsyncCheckRemoval::processBlockStructure(TR_BlockStructure *b)
   {
   // We remove all explicit async checks from the block.  All calls other than INL
   // calls cause a yield point as well.  We must ignore unresolved calls as well
   // because they might resolve to INL calls.
   //
   // 20020816: Return Paths as implicit yield points:
   // Base Case: If the return is in "main", it is safe to omit the yield
   //   point on returning paths.
   // Induction Step: Assume that the caller has correct yield points in it (it is safe).
   //   If you are returning to the caller, you need not do a yield, since you know
   //   that the caller will do it.
   //
   // By Induction, on the call depth, omitting yield points on paths that return is safe.
   //

   TR::Block *block = b->getBlock();
   TR::TreeTop *treeTop;
   TR::TreeTop *prev;
   bool hasAYieldPoint = false;
   bool hadAYieldPoint = false;
   AsyncInfo *info = (AsyncInfo *)b->getAnalysisInfo();
   TR::TreeTop *firstBlockEntry = block->startOfExtendedBlock()->getEntry();
   TR::Block *curBlock = block;
   TR::TreeTop *stopTree = NULL;
   for (treeTop = firstBlockEntry; treeTop != block->getExit(); treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         stopTree = NULL;
         curBlock = node->getBlock();

         TR_RegionStructure *outerImproperRegion = getOuterImproperRegion(curBlock->getStructureOf());
         TR_Structure * outerLoop = curBlock->getStructureOf()->getContainingLoop();

         if ((node->getVisitCount() == comp()->getVisitCount()) ||
             (outerImproperRegion && (!outerLoop || outerLoop->contains(outerImproperRegion))) ||
             (curBlock->getStructureOf()->getContainingLoop() != b->getContainingLoop()))
            {
            stopTree = node->getBlock()->getExit()->getPrevTreeTop();
            //continue;
            }

         node->setVisitCount(comp()->getVisitCount());
         }

      if (info->canHaveAYieldPoint() &&
          containsImplicitInternalPointer(node))
         {
         // Mark all extendees as 'mustNotHaveAYieldPoint'
         //
         // Sometimes CSE creates implicit internal pointers in portions of code
         // where there were no gc-points before.  We cannot add a async check is such
         // regions of code - since it will invalidate that optimization.  Instead
         // we recognize portions of extended basic blocks as "grey-areas" where its
         // going to be dangerous to put a new async check.
         //
         markExtendees(curBlock, false);
         }


      if (!stopTree)
         {
         switch (node->getOpCodeValue())
            {
            case TR::asynccheck:
               // Remove the async check
               //
               _asyncCheckInCurrentLoop = true;
               if (trace())
                  traceMsg(comp(), "removing async check from block_%d\n", b->getNumber());
               if (performTransformation(comp(), "%sremoving async check from block_%d\n", OPT_DETAILS, b->getNumber()))
                  {
                  prev = treeTop->getPrevTreeTop();
                  optimizer()->prepareForTreeRemoval(treeTop);
                  TR::TransformUtil::removeTree(comp(), treeTop);
                  treeTop = prev;
                  }
               hadAYieldPoint = true;
               break;

            case TR::treetop:
            case TR::NULLCHK:
            case TR::ResolveCHK:
            case TR::ResolveAndNULLCHK:
               if (node->getFirstChild()->getOpCode().isCall() && !node->getFirstChild()->getSymbolReference()->isUnresolved())
                  {
                  if (callDoesAnImplicitAsyncCheck(node->getFirstChild()))
                     hasAYieldPoint = true;
                  }
               break;

            default:
               if (node->getOpCode().isReturn())
                  hasAYieldPoint = true;
               break;
            }
         }
      }

   // If we cannot have a yield point here because of a implicit internal pointer
   // seen earlier in the extended basic block, then the internal pointer must go
   // dead before the end of the block, since by definition, it cannot be live
   // across a gc point.  Any extendees of this block can have a asynccheck
   // put into them
   //
   ////if ((hasAYieldPoint || hadAYieldPoint) && !info->canHaveAYieldPoint())
   ////   markExtendees(block, true);

   if (hasAYieldPoint)
      info->setHardYieldPoint();

   return 0;
   }

bool TR_RedundantAsyncCheckRemoval::callDoesAnImplicitAsyncCheck(TR::Node *callNode)
   {
   TR::SymbolReference *symRef = callNode->getSymbolReference();
   TR::MethodSymbol    *symbol = callNode->getSymbol()->castToMethodSymbol();
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

   if (symbol->isVMInternalNative() || symbol->isJITInternalNative())
      return false;

   // This covers jitProfileValue, singlePrecisionSQRT and currentTimeMaxPrecision
   if (symbol->isHelper())
      return false;

#ifdef J9_PROJECT_SPECIFIC
   if ((symbol->getRecognizedMethod()==TR::java_lang_Math_sqrt)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_StrictMath_sqrt)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_max_I) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_min_I) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_max_L) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_min_L) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_max_D) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_min_D) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_max_F) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_min_F) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_L) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_D) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_F) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_I) ||
       (symbol->getRecognizedMethod()==TR::java_lang_System_hiresClockImpl)         ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_numberOfLeadingZeros)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_Long_numberOfLeadingZeros)     ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_numberOfTrailingZeros) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Long_numberOfTrailingZeros) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_highestOneBit) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Long_reverseBytes)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_Short_reverseBytes)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_reverseBytes)  ||

       (symbol->getRecognizedMethod()==TR::java_lang_Long_highestOneBit) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_rotateLeft) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Long_rotateLeft) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_rotateRight) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Long_rotateRight)
       )
       return false;

   // Beginning in Java9 we use the same enum values for both the
   // sun.misc.Unsafe wrappers and the jdk.internal.misc.Unsafe JNI methods, so
   // for these we need to do an additional test to check if they are the native
   // versions or not.
   if (symbol->isNative() &&
       ((symbol->getRecognizedMethod()==TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z) ||
       (symbol->getRecognizedMethod()==TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z) ||
       (symbol->getRecognizedMethod()==TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z))
      )
      return false;
#endif
   return true;
   }

int32_t TR_RedundantAsyncCheckRemoval::processAcyclicRegion(TR_RegionStructure *region)
   {
   // We do not need to look inside an acyclic region, if it is
   // not inside a loop.
   //
   if (getOuterLoop(region) == 0)
      return 0;

   _ancestors.deleteAll();

   comp()->incVisitCount();
   computeCoverageInfo(region->getEntry(), region->getEntry());

   if (GET_ASYNC_INFO(region->getEntry())->getCoverage() != FullyCovered)
      {
      comp()->incVisitCount();
      TR_RegionStructure::Cursor it(*region);
      TR_StructureSubGraphNode *node;
      for (node = it.getFirst(); node; node = it.getNext())
         {
         AsyncInfo *info = GET_ASYNC_INFO(node);
         if (info->hasYieldPoint())
            markAncestors(node, region->getEntry());
            }
      ListIterator<TR_StructureSubGraphNode> ait(&_ancestors);
      if (!_ancestors.isEmpty())
         {
         for (node = ait.getFirst(); node; node = ait.getNext())
            {
            getNearestAncestors(node, node, region->getEntry());
            }

         while ((node = findSmallestAncestor()) != 0)
            {
            insertAsyncCheckOnSubTree(node, region->getEntry());
            }

         }
      }

   AsyncInfo *info = (AsyncInfo *)region->getAnalysisInfo();
   AsyncInfo *entryInfo = GET_ASYNC_INFO(region->getEntry());

   // At this point, an ac region can either be not covered or
   // fully covered.  It may /seem/ partially covered because of
   // exit successors.  For such cases:
   //
   if (entryInfo->getCoverage() == PartiallyCovered)
      entryInfo->setCoverage(FullyCovered);

   info->setCoverage(entryInfo->getCoverage());

   return 0;
   }

// Recursively propagates coverage information about nodes in a loop or acylic region
//
void TR_RedundantAsyncCheckRemoval::computeCoverageInfo(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return;

   node->setVisitCount(comp()->getVisitCount());
   AsyncInfo *info = GET_ASYNC_INFO(node);

   if (!info->hasYieldPoint())
      {
      // This node is not FullyCovered, if all it's successors are covered.
      // If it is covered on some paths then it is PartirallyCovered
      // If it is not covered on any path, then it is NotCovered.
      //
      bool coveredOnSomePaths = false;
      bool notCoveredOnSomePaths = false;
      bool hasRealSuccessors = false;
      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());

         // If there is an exit edge from node, we are not covered on that path
         //
         if (!succ->getStructure())
            notCoveredOnSomePaths = true;
         else
            {
            hasRealSuccessors = true;
            if (succ == entry)
               notCoveredOnSomePaths = true;
            else
               {
               computeCoverageInfo(succ, entry);
               switch (GET_ASYNC_INFO(succ)->getCoverage())
                  {
                  case NotCovered:
                     notCoveredOnSomePaths = true;
                     break;
                  case PartiallyCovered:
                     coveredOnSomePaths = true;
                     notCoveredOnSomePaths = true;
                     break;
                  case FullyCovered:
                  coveredOnSomePaths = true;
                  break;
                  }
               }
            }
         }

      if (hasRealSuccessors)
         {
         if (coveredOnSomePaths && notCoveredOnSomePaths)
            info->setCoverage(PartiallyCovered);
         else if (coveredOnSomePaths && !notCoveredOnSomePaths)
            info->setCoverage(FullyCovered);
         else if (!coveredOnSomePaths && notCoveredOnSomePaths)
            info->setCoverage(NotCovered);
         else
            info->setCoverage(NotCovered);
         }
      else
         info->setCoverage(NotCovered);
      }

   if (trace())
      traceMsg(comp(), "for node: %d coverage: %d\n", node->getNumber(), info->getCoverage());
   }

bool TR_RedundantAsyncCheckRemoval::isMaxLoopIterationGuardedLoop(TR_RegionStructure *loop)
   {
   // FIXME: this is much easier if dominator trees are available

   TR_RegionStructure *parent = loop->getParent()->asRegion();
   TR_StructureSubGraphNode *node = parent->findSubNodeInRegion(loop->getNumber());
   if (!(node->getPredecessors().size() == 1))
      return false;
   TR_StructureSubGraphNode *pred = node->getPredecessors().front()->getFrom()
      ->asStructureSubGraphNode();

   // If parent is an improper region, give up
   //
   if (parent->containsInternalCycles())
      return false;

   // The following is a rather intricate structure walk, and requires some explanation
   // we start at the node representing the loop in the parent region.  What we want to do
   // is to find a dominating if-block that contains the max-loop-iteration guard for this
   // loop.  Traditionally, the guard is not the parent region, but infact there are several
   // other versioning guards intervening between the loop and the iter guard.  Each of
   // these guards forms its own acyclic region, nesting the inner guards.
   //
   // We start the search in the parent region, walking the predecessors of the loop region.
   // By definition there must exist an acyclic path between the loop and the guard.  Ignore
   // backedges and just walk the preds.  There cannot be a region structure en route
   // since that would imply that there was some control flow not typical of guarded versioned
   // loops.  If we hit the entry of the parent region, we start the same process in the
   // parent of the parent region, until we hit the start-node, which implies failure.
   //
   // Each node en route must have exactly one predecessor, otherwise we have control flow
   // not typical of a versioned loop.
   //
   // The walk below uses a queue for traversal just to look like a canonical backwards
   // reverse post order walk, however there is never more than one element in the queue.
   // NOTE: there is no use of visit counts in this loop, a node can never be visited twice
   // in the following walk
   //
   TR_Queue<TR_StructureSubGraphNode> q(trMemory());
   q.enqueue(pred);
   while (!q.isEmpty())
      {
      TR_StructureSubGraphNode *cursor = q.dequeue();

      // cannot have a region between the guard and the actual loop
      //
      if (cursor->getStructure()->asRegion())
         return false;

      // Check the cursor block
      //
      TR::Block *block = cursor->getStructure()->asBlock()->getBlock();
      if (block == _cfg->getStart()->asBlock())
         return false;

      TR::TreeTop *tt = block->getLastRealTreeTop();
      if (tt->getNode()->isMaxLoopIterationGuard())
         return true;

      // if we have hit the entry of the parent, we need to start looking in the
      // parent's parent
      //
      TR_RegionStructure *cursorParent = cursor->getStructure()->getParent()->asRegion();
      if (cursor == cursorParent->getEntry())
         {
         // if the parent was a loop, we know already that the guard was not found
         // (cannot be improper, otherwise we would never have enqueued it)
         //
         if (cursorParent->isNaturalLoop())
            return false;

         // if we hit the start of the cfg, we have failed
         //
         if (!cursorParent->getParent())
            return false;
         cursorParent = cursorParent->getParent()->asRegion();

         // If the parent is improper, give up
         //
         if (cursorParent->containsInternalCycles())
            return false;

         cursor = cursorParent->findSubNodeInRegion(cursor->getNumber());
         }

      // Enqueue the predecessors of the cursor
      // there must be one unique predecessor
      //
      if (!(cursor->getPredecessors().size() == 1))
         return false;
      TR_StructureSubGraphNode *pred = cursor->getPredecessors().front()->getFrom()
         ->asStructureSubGraphNode();
      q.enqueue(pred);
      }

   return false;
   }

bool
TR_RedundantAsyncCheckRemoval::containsImplicitInternalPointer(TR::Node *node)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return false;
   node->setVisitCount(comp()->getVisitCount());

   bool result = false;
   if (node->getOpCode().isArrayRef() && node->getReferenceCount() > 1 &&
       (!comp()->cg()->supportsInternalPointers() || !node->isInternalPointer() || !node->getPinningArrayPointer()))
      {
      result = true;
      }
   else
      {
      for (int32_t c = node->getNumChildren()-1; c >= 0; --c)
         {
         if (containsImplicitInternalPointer(node->getChild(c)))
            {
            result = true;
            break;
            }
         }
      }
   if (trace())
      traceMsg(comp(), "    containsImplicitInternalPointer(%p) = %s\n", node, result?"true":"false");
   return result;
   }

void
TR_RedundantAsyncCheckRemoval::markExtendees(TR::Block *block, bool canHaveAYieldPoint)
   {
   for (TR::Block *cursor = block->getNextBlock();
        cursor && cursor->isExtensionOfPreviousBlock();
        cursor = cursor->getNextBlock())
      {
      TR_BlockStructure *s = cursor->getStructureOf();
      AsyncInfo *ai = ((AsyncInfo*)s->getAnalysisInfo());
      if (trace())
         {
         traceMsg(comp(), "    block_%d canHaveAYieldPoint %s -> %s\n",
            cursor->getNumber(),
            ai->canHaveAYieldPoint()? "true":"false",
            canHaveAYieldPoint?       "true":"false");
         }
      ai->setCanHaveAYieldPoint(canHaveAYieldPoint);
      }
   }

#define FIND_LOOP_ITERATIONS(Type,Name,NAME)   \
Type incr = static_cast<Type>(incrVal->getLow##Name());  \
Type in, out, iters, diff; \
if (incr == 0) continue; \
if (entryVal && exitVal && entryVal->as##Name##Const() && exitVal->as##Name##Const()) \
  { \
  in = static_cast<Type>(entryVal->getLow##Name()); \
  out = static_cast<Type>(exitVal->getLow##Name()); \
  } \
else if (entryVal && entryVal->as##Name##Const()) \
  { \
  Type lo, hi; \
  if (exitVal) \
    { \
    lo = static_cast<Type>(exitVal->getLow##Name()); \
    hi = static_cast<Type>(exitVal->getHigh##Name()); \
    } \
  else \
    { \
    lo = static_cast<Type>(TR::getMinSigned<NAME>()); \
    hi = static_cast<Type>(TR::getMaxSigned<NAME>()); \
    } \
  in = static_cast<Type>(entryVal->getLow##Name()); \
  if (incr > 0 && in < lo) \
    out = lo; \
  else if (incr < 0 && in > hi) \
    out = hi; \
  else \
    continue; \
  } \
else \
  continue; \
diff = in-out; \
if (diff == static_cast<Type>(TR::getMinSigned<NAME>())) \
   continue; \
iters = (diff < 0) ? -diff/incr : diff/-incr;


int32_t TR_RedundantAsyncCheckRemoval::estimateLoopIterations(TR_RegionStructure *loop)
   {
   int32_t estimate = INT_MAX;

   TR_InductionVariable *indVar;
   for (indVar = loop->getFirstInductionVariable();
        indVar != 0;
        indVar = indVar->getNext())
      {
      TR::VPConstraint *entryVal = indVar->getEntry();
      TR::VPConstraint *exitVal  = indVar->getExit();
      TR::VPConstraint *incrVal  = indVar->getIncr();

      if (incrVal->asLongConst() || (entryVal && entryVal->asLongConst()) || (exitVal && exitVal->asLongConst()))
         {
         FIND_LOOP_ITERATIONS(int64_t,Long,TR::Int64);

         if (iters < estimate)
            estimate = (int32_t) iters;
         }
      else
         {
         FIND_LOOP_ITERATIONS(int32_t,Int,TR::Int32);

         if (iters < estimate)
            estimate = iters;
         }
      }

   if (isMaxLoopIterationGuardedLoop(loop))
      {
      return SHORT_RUNNING_LOOP;
      }

   if (loop->getFirstInductionVariable() == 0)
      {
      TR_LoopEstimator est(comp()->getFlowGraph(), loop, trace());
      estimate = est.estimateLoopIterationsUpperBound();
      }

   return estimate;
   }

int32_t TR_RedundantAsyncCheckRemoval::findShallowestCommonCaller(int32_t callSiteIndex1, int32_t callSiteIndex2)
   {
   while ((callSiteIndex1 != -1) && (callSiteIndex1 != -1) && (callSiteIndex1 != callSiteIndex2))
      {
      if (callSiteIndex1 > callSiteIndex2)
	 callSiteIndex1 = comp()->getInlinedCallSite(callSiteIndex1)._byteCodeInfo.getCallerIndex();
      else
	 callSiteIndex2 = comp()->getInlinedCallSite(callSiteIndex2)._byteCodeInfo.getCallerIndex();
      }
   if (callSiteIndex1 == callSiteIndex2)
      return callSiteIndex1;
   else
      return -1;
   }


bool TR_RedundantAsyncCheckRemoval::originatesFromShortRunningMethod(TR_RegionStructure* region)
   {

   // Loops originating from java/lang/String methods are assumed to be short running unless
   // they contain loops that are inlined from methods outsides the above classes.
   // The following is an algorithm to detect such loops. In the algorithm, we assume that if all branches of all basic blocks of a loop
   // have the same owning method, then that loop originates from that method.

   //First build a list of branches in all basic blocks of the loop (region)
   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   region->getBlocks(&blocksInRegion);
   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR_ScratchList<TR::Node> branches(trMemory());
   for (TR::Block * block = blocksIt.getCurrent(); block; block=blocksIt.getNext())
      {
      TR::TreeTop* lastTreeTop = block->getLastRealTreeTop();
      if (lastTreeTop != block->getEntry() && lastTreeTop->getNode()->getOpCode().isBranch())
	 branches.add(lastTreeTop->getNode());
      }

   TR_ASSERT(!branches.isEmpty(), "there are no branches in this loop!\n");
   if (branches.isEmpty())
     return false;

   ListIterator<TR::Node> branchNodeIt(&branches);
   int32_t commonCaller = branchNodeIt.getFirst()->getByteCodeInfo().getCallerIndex();
   for (TR::Node* branchNode = branchNodeIt.getNext(); branchNode; branchNode = branchNodeIt.getNext())
      {
      commonCaller = findShallowestCommonCaller(commonCaller,
						branchNode->getByteCodeInfo().getCallerIndex());
      }

   while ((commonCaller != -1) && !comp()->isShortRunningMethod(commonCaller))
      commonCaller = comp()->getInlinedCallSite(commonCaller)._byteCodeInfo.getCallerIndex();
   if (commonCaller == -1)
      return false;

   //Check all the branches. Either their origin must be the
   //same as the common ancestor, or they must be called by the
   //same origin through short-running or loopless methods.
   for (TR::Node* branchNode = branchNodeIt.getFirst(); branchNode; branchNode = branchNodeIt.getNext())
      {
      int32_t callerIndex = branchNode->getByteCodeInfo().getCallerIndex();
      bool callerMatch = false;
      while (callerIndex != -1)
	 {
	 if (callerIndex == commonCaller)
	    {
	    callerMatch = true;
	    break;
	    }
	 TR_InlinedCallSite &ics = comp()->getInlinedCallSite(callerIndex);
	 if (!comp()->isShortRunningMethod(callerIndex) &&
	     TR::Compiler->mtd.hasBackwardBranches(ics._methodInfo))
	    break;
	 //set callerIndex to its caller
	 callerIndex = comp()->getInlinedCallSite(callerIndex)._byteCodeInfo.getCallerIndex();
	 }
      if (!callerMatch)
	 return false;
      }
   return true;
   }


bool TR_RedundantAsyncCheckRemoval::hasEarlyExit(TR_RegionStructure *region)
   {
   ListIterator<TR::CFGEdge> eit2(&region->getExitEdges());
   for (TR::CFGEdge *edge2 = eit2.getCurrent(); edge2 != 0; edge2 = eit2.getNext())
      {
      TR_StructureSubGraphNode *pred2 = edge2->getFrom()->asStructureSubGraphNode();
      bool earlyExit = true;
      for (auto edge = region->getEntry()->getPredecessors().begin(); edge != region->getEntry()->getPredecessors().end();
    		  ++edge)
         {
         if (pred2 == (*edge)->getFrom())
            {
            if (trace())
               {
               traceMsg(comp(), "pred2 = %d\n", pred2 ? pred2->getNumber() : -1);
               traceMsg(comp(), "edge->getFrom = %d\n", (*edge)->getFrom() ? (*edge)->getFrom()->getNumber() : -1);
               }

            earlyExit = false;
            break;
            }
         }

       if (earlyExit)
          {
          if (trace())
             traceMsg(comp(), "found earlyExit in region %d \n", region->getNumber());
          return true;
	  }
      }

   return false;
   }


int32_t TR_RedundantAsyncCheckRemoval::processNaturalLoop(TR_RegionStructure *region, bool isInsideImproperRegion)
   {
   if (trace())
      traceMsg(comp(), "==> Forward Processing natural loop %d\n", region->getNumber());
   bool isShortRunning = false;
   bool needsForwardAnalysis = true;

   if (!isInsideImproperRegion)
      {
      // There is no need to put async checks in loops created by tail recursion
      // elimination.  The reasoning is that if the loop were long-running, we
      // would have resulted in a stack overflow to begin with.
      //
      TR::Block *entryBlock = region->getEntryBlock();
      for (auto edge = entryBlock->getPredecessors().begin(); (edge != entryBlock->getPredecessors().end())&& !isShortRunning; ++edge)
         {
         if ((*edge)->getCreatedByTailRecursionElimination())
            {
            isShortRunning = true;
            if (trace())
               traceMsg(comp(), "Loop %d was created by TailRecursionElim.  Skipping\n", region->getNumber());
            }
         }

      if ((comp()->getMethodHotness() == scorching) && originatesFromShortRunningMethod(region))
            {
            isShortRunning = true;
	 if (trace())
	    traceMsg(comp(), "Loop %d originates from a trusted method, and therefore, is tagged as short running. Skipping\n", region->getNumber());
         }

      // Async check did not exist in this loop at beginning of RACR
      // This implies that loop versioning eliminated the asynccheck
      // from the loop based on short running guard.
      //
      if (!_asyncCheckInCurrentLoop)
         {
         isShortRunning = true;
         if (trace())
            traceMsg(comp(), "Loop %d is a Short running loop. Skipping\n", region->getNumber());
         }

      // Spill Loops generated by the General Loop Unroller do not need any async checks
      // either since they are guaranteed to be short running
      //
      if (entryBlock->getStructureOf()->isEntryOfShortRunningLoop())
         {
         isShortRunning = true;
         if (trace())
            traceMsg(comp(), "Loop %d is a Short running loop. Skipping\n", region->getNumber());
         }

      if (!isShortRunning && (estimateLoopIterations(region) < SHORT_RUNNING_LOOP_BOUND))
         {
         isShortRunning = true;
         if (trace())
            traceMsg(comp(), "Loop %d is short running. Skipping\n", region->getNumber());
         }
      }

   bool cannotClaimFullCoverage = false;
   if (!isShortRunning)
      {
      _ancestors.deleteAll();

      // First of all, compute coverage information for all subnodes
      //
      comp()->incVisitCount();
      computeCoverageInfo(region->getEntry(), region->getEntry());

      // If the entry is Fully Covered, we do not need to perform the analysis
      //
      if (GET_ASYNC_INFO(region->getEntry())->getCoverage() == FullyCovered)
         {
         if (trace())
            traceMsg(comp(), "Region is completely covered.  No need to perform POSet analysis.\n");
         needsForwardAnalysis = false;
         }

      // Just for debugging right now, print out the debugging information.
      // Print nodes and their coverage information
      //
      TR_RegionStructure::Cursor it(*region);
      ListIterator<TR_StructureSubGraphNode> ait(&_ancestors);
      TR_StructureSubGraphNode *node;
#ifdef DEBUG
      for (node = it.getFirst(); node; node = it.getNext())
         {
         AsyncInfo *info = GET_ASYNC_INFO(node);
         if (trace())
            traceMsg(comp(), "Node %d, coverage: %d\n", node->getNumber(), info->getCoverage());
         }
#endif

      if (needsForwardAnalysis)
         {
         // Find all ancestor branch nodes that are candidates for fixing.
         //
         comp()->incVisitCount();
         it.reset();
         for (node = it.getFirst(); node; node = it.getNext())
            {
            AsyncInfo *info = GET_ASYNC_INFO(node);
            if (info->hasYieldPoint())
               markAncestors(node, region->getEntry());
            }

         // If there were no ancestors, then do something simple
         // We used to put an async check in the entry -- but thats not too good
         // A better fix is to place then on backedges
         // This would penalize triply nested loops that appear to be singly
         // nested loops to our structural analysis... but these should be rare
         //
         if (_ancestors.isEmpty())
            {
            bool placeInEntry = false;
            for (auto edge = region->getEntry()->getPredecessors().begin(); edge != region->getEntry()->getPredecessors().end(); ++edge)
               {
               TR_StructureSubGraphNode *pred = (*edge)->getFrom()->asStructureSubGraphNode();
               AsyncInfo *info = GET_ASYNC_INFO(pred);
               if (!info->canHaveAYieldPoint() ||
                   (pred->getStructure()->asRegion() && pred->getStructure()->asRegion()->isNaturalLoop() &&
                    (info->getCoverage() != FullyCovered)))
                  {
                  placeInEntry = true;
                  break;
                  }
               }

             if (GET_ASYNC_INFO(region->getEntry())->getCoverage() != NotCovered)
                placeInEntry = true;

             if (placeInEntry)
                {
                GET_ASYNC_INFO(region->getEntry())->setSoftYieldPoint();
                }
             else
                {
                TR_ASSERT(GET_ASYNC_INFO(region->getEntry())->getCoverage() == NotCovered,
                   "_ancestors cannot be empty if the loop is covered in any way");

                for (auto edge = region->getEntry()->getPredecessors().begin(); edge != region->getEntry()->getPredecessors().end(); ++edge)
                   {
                   TR_StructureSubGraphNode *pred = (*edge)->getFrom()->asStructureSubGraphNode();
                   GET_ASYNC_INFO(pred)->setSoftYieldPoint();
                   }
                }
            }
         else
            {
            // Contruct a partial ordering
            //

            for (node = ait.getFirst(); node; node = ait.getNext())
               {
               getNearestAncestors(node, node, region->getEntry());
               }

            if (trace())
               {
               // print out debugging information about the partial ordering
               ait.reset();
               for (node = ait.getFirst(); node; node = ait.getNext())
                  {
                  AsyncInfo *info = GET_ASYNC_INFO(node);
                  traceMsg(comp(), "-------------------------- NODE %d ----------------------\n", node->getNumber());
                  ListIterator<TR_StructureSubGraphNode> nit(&info->getChildren());
                  TR_StructureSubGraphNode *relative;
                  for (relative = nit.getFirst(); relative; relative = nit.getNext())
                     {
                     traceMsg(comp(), "child ----> %d\n", relative->getNumber());
                     }
                  nit.set(&info->getParents());
                  for (relative = nit.getFirst(); relative; relative = nit.getNext())
                     {
                     traceMsg(comp(), "parent ----> %d\n", relative->getNumber());
                     }
                  }
               }

            // Pick the smallest _ancestor, and fix it.
            //
            while ( (node = findSmallestAncestor()) != 0)
               {
               insertAsyncCheckOnSubTree(node, region->getEntry());

               // Print out some debugging info
               //
               if (trace())
                  {
                  traceMsg(comp(), "smallest is %d\n", node->getNumber());
                  TR_RegionStructure::Cursor it(*region);
                  TR_StructureSubGraphNode *n0de;
                  for (n0de = it.getFirst(); n0de; n0de = it.getNext())
                     {
                     AsyncInfo *info = GET_ASYNC_INFO(n0de);
                     traceMsg(comp(), "Node %d, coverage: %d\n", n0de->getNumber(), info->getCoverage());
                     }
                  traceMsg(comp(), "-----------------------------------------------------------\n");
                  }
               }

            } //else of if (_ancestor.is....)

         } // if (needsFo...)


      if (hasEarlyExit(region))
         {
         if (trace())
            traceMsg(comp(), "found earlyExit in region %d, so cannotClaimFullCoverage\n", region->getNumber());
         cannotClaimFullCoverage = true;
         }

      performRegionalBackwardAnalysis(region, false);

      TR_RegionStructure::Cursor rSubnodes(*region);
      for (node = rSubnodes.getFirst(); node; node = rSubnodes.getNext())
         {
         solidifySoftAsyncChecks(node);
         }

      if (!cannotClaimFullCoverage)
         {
         AsyncInfo *regionInfo = (AsyncInfo *)region->getAnalysisInfo();
         regionInfo->setCoverage(FullyCovered);
         regionInfo->setHardYieldPoint();
         }
      } // if (!isShortRunn...)
   else
      {
      if (!isInsideImproperRegion)
         _foundShortRunningLoops = true;
      }

   if (trace())
      traceMsg(comp(), "==> Finished processing region %d\n", region->getNumber());

   return 0;
   }

TR_StructureSubGraphNode *TR_RedundantAsyncCheckRemoval::findSmallestAncestor()
   {
   // Smallest node has nothing smaller than it.
   // Pick the node that has it's children list empty
   //
   ListIterator<TR_StructureSubGraphNode> it(&_ancestors);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node; node = it.getNext())
      {
      AsyncInfo *info = GET_ASYNC_INFO(node);
      if (info->getChildren().isEmpty())
         {
         // Remove it from each of it's parents
         ListIterator<TR_StructureSubGraphNode> pi(&info->getParents());
         TR_StructureSubGraphNode *parent;
         for (parent = it.getFirst(); parent; parent = it.getNext())
            {
            GET_ASYNC_INFO(parent)->getChildren().remove(node);
            }
         // remove it from _ancestors as well
         _ancestors.remove(node);
         return node;
         }
      }

   return 0;
   }

static TR_StructureSubGraphNode *findNodeInHierarchy(TR_RegionStructure *region, int32_t num)
   {
   if (!region) return 0;
   TR_RegionStructure::Cursor it(*region);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getNumber() == num)
         return node;
      }
   return findNodeInHierarchy(region->getParent()->asRegion(), num);
   }

void TR_RedundantAsyncCheckRemoval::insertAsyncCheckOnSubTree(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry)
   {
   AsyncInfo *info = GET_ASYNC_INFO(node);

   if (info->getCoverage() == FullyCovered)
      return;

   TR_RegionStructure *outerLoop = getOuterLoop(entry->getStructure()->getParent()->asRegion());

   // Cover all uncovered successors
   //
   for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
      {
      TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());

      if (succ == entry)
         continue;

      if (!succ->getStructure())
         {
         // If we are the outer most loop, then we can ignore the exit edges
         // because they cannot participate in any cycle.
         //
         if (outerLoop == 0)
            continue;

         TR_StructureSubGraphNode *realNode = findNodeInHierarchy(entry->getStructure()->getParent()->asRegion(),
                                                                  succ->getNumber());
         TR_BlockStructure *block = realNode->getStructure()->asBlock();
         if (block)
            {
            if (trace())
               traceMsg(comp(), "- added exit yield point in block_%d\n", block->getNumber());
            AsyncInfo *info = (AsyncInfo *)block->getAnalysisInfo();
            info->setSoftYieldPoint();
            }
         }
      else
         {
         AsyncInfo *succInfo = GET_ASYNC_INFO(succ);
         if (succInfo->getCoverage() != FullyCovered)
            {
            if (trace())
               {
               traceMsg(comp(), "--------------------------------------\n");
               traceMsg(comp(), "=======>Added asynccheck in %d<=======\n", succ->getNumber());
               traceMsg(comp(), "--------------------------------------\n");
               }
            succInfo->setSoftYieldPoint();
            }
         }
      }

   // Recompute coverage information
   //
   comp()->incVisitCount();
   computeCoverageInfo(entry, entry);
   }

void TR_RedundantAsyncCheckRemoval::getNearestAncestors(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *current, TR_StructureSubGraphNode *entry)
   {
   //   printf("node %d, current %d\n", node->getNumber(), current->getNumber());
   if (current == entry || node == entry)
      return;

   for (auto edge = current->getPredecessors().begin(); edge != current->getPredecessors().end(); ++edge)
      {
      TR_StructureSubGraphNode *pred = toStructureSubGraphNode((*edge)->getFrom());
      AsyncInfo *info = GET_ASYNC_INFO(pred);

      if (!info->isAlreadyVisited(node))
         {
         info->setVisitMarker(node);
         if (info->isAncestor())
            {
            AsyncInfo *nodeInfo = GET_ASYNC_INFO(node);
            // node < pred
            nodeInfo->addParent(pred);
            // pred > node
            info->addChild(node);
            }
         else
            {
            getNearestAncestors(node, pred, entry);
            }
         }
      }
   }


void TR_RedundantAsyncCheckRemoval::markAncestors(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry)
   {
   return;  // Disable it for performance. For more details, refer to https://github.com/eclipse/omr/pull/1138

   if (node == entry)
      return;

   if (node->getVisitCount() == comp()->getVisitCount())
      return;

   node->setVisitCount(comp()->getVisitCount());

   if (trace())
      traceMsg(comp(),"<===markAncestors start=== ssg node: %d, ssg entry: %d\n", node->getNumber(), entry->getNumber());

   for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
      {
      TR_StructureSubGraphNode *pred = toStructureSubGraphNode((*edge)->getFrom());

      AsyncInfo *info = GET_ASYNC_INFO(pred);

      // A node is a candidate ancestor if it a partially covered branch node
      // and is allowed to have a yield point in it.
      //
      // FIXME: this can be improved slightly - if its a block which can have a yield
      // point in it - and the next block cannot have a yield point in it - then mark
      // it even if it is not a branch node
      //
      if (info->getCoverage() == PartiallyCovered &&
          !(pred->getSuccessors().size() == 1) &&
          !info->isAncestor() &&
          info->canHaveAYieldPoint())
         {
         bool canMakeAncestor = true;
         for (auto succEdge = pred->getSuccessors().begin(); succEdge != pred->getSuccessors().end(); ++succEdge)
            {
            TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*succEdge)->getTo());
            if (!succ->getStructure() ||
                !GET_ASYNC_INFO(succ)->canHaveAYieldPoint())
	       {
	       canMakeAncestor = false;
               break;
	       }
            }

         if (canMakeAncestor)
	    {
            info->markAsAncestor();
            _ancestors.add(pred);
	    }
         }

      if (trace())
         traceMsg(comp(),"<===markAncestors recursion=== ssg pred: %d, ssg entry: %d\n", pred->getNumber(), entry->getNumber());

      markAncestors(pred, entry);
      }

   }


int32_t TR_RedundantAsyncCheckRemoval::processImproperRegion(TR_RegionStructure *region)
   {
   // We are conservative with improper regions.  We do not attempt to find an optimal
   // solution for an improper region, and leave it in its original form, hence being
   // conservatively correct.  However, any inner loops of the region are analyzed
   // normally: they execute an AC definitely, before and after the transformation, so
   // we should be correct.
   //
   TR_ScratchList<TR_RegionStructure> stack(trMemory());
   stack.add(region);
   while (!stack.isEmpty())
      {
      TR_RegionStructure *region = stack.popHead();

      if (region->isNaturalLoop())
         perform(region, true);
      else
         {

         TR_RegionStructure::Cursor it(*region);
         TR_StructureSubGraphNode *subNode;
         for (subNode = it.getFirst(); subNode; subNode = it.getNext())
            {
            TR_RegionStructure *str = subNode->getStructure()->asRegion();
            if (str)  stack.add(str);
            }
         }
      }

   AsyncInfo *info = (AsyncInfo *)region->getAnalysisInfo();
   info->setCoverage(FullyCovered);
   info->setHardYieldPoint();

   return 0;
   }



void TR_RedundantAsyncCheckRemoval::enqueueSinks(TR_RegionStructure *region, TR_Queue<TR_StructureSubGraphNode> *q, bool in)
   {
   if (region->isAcyclic())
      {
      // Any node with only exit successors is a sink
      //
      TR_RegionStructure::Cursor ni(*region);
      TR_StructureSubGraphNode *node;
      for (node = ni.getFirst(); node; node = ni.getNext())
         {
         bool seenInternalEdge = false;
         for (auto edge = node->getSuccessors().begin(); !seenInternalEdge && (edge != node->getSuccessors().end()); ++edge)
            {
            if (!region->isExitEdge(*edge))
               seenInternalEdge = true;
            }

         if (!seenInternalEdge)
            {
            q->enqueue(node);
            GET_ASYNC_INFO(node)->setReverseCoverageInfo(in);
            }
         }
      }
   else if (region->isNaturalLoop())
      {
      // All the predecessors of the entry node are the sinks.
      //
      for (auto edge = region->getEntry()->getPredecessors().begin(); edge != region->getEntry()->getPredecessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *pred = toStructureSubGraphNode((*edge)->getFrom());
         q->enqueue(pred);
         }
      }
   else
      TR_ASSERT(0, "sinks are not defined in an improper region.");
   }

// Performs a linear backwards AC redundancy analysis using a flow algorithm to BFS, with
// the only difference that we analyzed a node only after all its successors have been
// analyzed
//
bool TR_RedundantAsyncCheckRemoval::performRegionalBackwardAnalysis(TR_RegionStructure *region, bool inInfo)
   {
   bool earlyExit = hasEarlyExit(region);

   TR_Queue<TR_StructureSubGraphNode> *q = new (trHeapMemory()) TR_Queue<TR_StructureSubGraphNode>(trMemory());

   // Start off by enqueue-ing all sinks for the region.
   // In a natural loop, a sink is defined to be a node from which the backedge
   // originates.  In an acylic region, a sink is a node with only exit successors.
   //
   enqueueSinks(region, q, inInfo);

   // Mark all sub nodes as unvisited (white)
   //
   TR_RegionStructure::Cursor ni(*region);
   TR_StructureSubGraphNode *subNode;
   for (subNode = ni.getFirst(); subNode; subNode = ni.getNext())
      subNode->getStructure()->setAnalyzedStatus(false);

   if (trace())
      traceMsg(comp(), "<== Start processing region %d, in = %d\n", region->getNumber(), inInfo);

   while (!q->isEmpty())
      {
      TR_StructureSubGraphNode *node = q->dequeue();

      // If we have seen the node before, skip.
      if (node->getStructure()->hasBeenAnalyzedBefore())
         continue;

      // We can process a node only after all its successors have been analyzed
      //
      bool hasUnanalyzedChildren = false;
      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end() && !hasUnanalyzedChildren ; ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (!succ->getStructure() || succ == region->getEntry())
            continue;
         if (!succ->getStructure()->hasBeenAnalyzedBefore())
            hasUnanalyzedChildren = true;
         }
      if (hasUnanalyzedChildren)
         continue;

      // Enqueue all predecessors of this node
      //
      for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *pred = toStructureSubGraphNode((*edge)->getFrom());
         q->enqueue(pred);
         }

      // Compute in(n) = intersection over all successors S, out(S).
      //
      bool in = false;
      bool coveredOnSomePaths = false;
      bool notCoveredOnSomePaths = false;
      bool backedge = false;
      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end() && !notCoveredOnSomePaths; ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());

         // Ignore exit successors
         //
         if (!succ->getStructure())
            continue;

         // Backedge kills info
         //
         if (succ == region->getEntry())
            notCoveredOnSomePaths = backedge = true;
         else
            {
            TR_ASSERT(succ->getStructure()->hasBeenAnalyzedBefore(), "unvisited successor encountered");

            // intersection
            //
            if (GET_ASYNC_INFO(succ)->getReverseCoverageInfo() == false)
               notCoveredOnSomePaths = true;
            else
               coveredOnSomePaths = true;
            }
         }

      if (coveredOnSomePaths && !notCoveredOnSomePaths) //i.e covered on all paths
         in = true;

      // If N is an acyclic region, then investigate inside.
      //
      bool out = false;
      TR_RegionStructure *region = node->getStructure()->asRegion();
      if (region && region->isAcyclic())
         out = performRegionalBackwardAnalysis(node->getStructure()->asRegion(), in);
      else if (region && region->getAnalysisInfo() && ((AsyncInfo *)region->getAnalysisInfo())->hasYieldPoint() && GET_ASYNC_INFO(region->getEntry())->getReverseCoverageInfo())
         out = true; //natural loop (except for short running) or improper region

      // If this is a block, is covered on all in paths, and has a soft yield point,
      // we can remove the soft yield point.
      //
      AsyncInfo *info = GET_ASYNC_INFO(node);
      bool removedYieldPoint = false;
      if (!region && in == true && info->hasSoftYieldPoint())
         {
         // remove yield point
         //
         if (trace())
            traceMsg(comp(), "\t\tremoved yield point from node %d\n", node->getNumber());
         info->removeYieldPoint();
         removedYieldPoint = true;
         }

      out = out || (info->hasYieldPoint() && !info->hasSoftYieldPoint() && ((region && GET_ASYNC_INFO(region->getEntry())->getReverseCoverageInfo()) || !earlyExit)) || (in && !backedge && (!removedYieldPoint || !earlyExit));
      node->getStructure()->setAnalyzedStatus(true);  // mark it as visited (black)
      info->setReverseCoverageInfo(out);

      if (trace())
         traceMsg(comp(), "\tsubnode %d, in = %d, out = %d\n", node->getNumber(), in, out);
      }

   if (trace())
      traceMsg(comp(), "<== Finished processing region %d, out = %d\n", region->getNumber(),
                  GET_ASYNC_INFO(region->getEntry())->getReverseCoverageInfo());

   return GET_ASYNC_INFO(region->getEntry())->getReverseCoverageInfo();
   }




void TR_RedundantAsyncCheckRemoval::solidifySoftAsyncChecks(TR_StructureSubGraphNode *node)
   {
   TR_Structure *str = node->getStructure();
   TR_BlockStructure *b = str->asBlock();

   if (b)
      {
      if (GET_ASYNC_INFO(node)->hasSoftYieldPoint())
         {
         if (performTransformation(comp(), "%sinserted async check in block_%d\n", OPT_DETAILS, b->getNumber()))
            {
            TR::Block *block = b->getBlock();
            TR_AsyncCheckInsertion::insertAsyncCheck(block, comp(), "redundantAsyncCheckRemoval/solidify");
            _numAsyncChecksInserted++;
            }
         }
      }
   else
      {
      TR_RegionStructure *region = str->asRegion();

      // process acyclic regions only.
      //
      if (region->isAcyclic())
         {
         if (GET_ASYNC_INFO(node) && GET_ASYNC_INFO(node)->hasSoftYieldPoint())
            {
            TR::Block *entryBlock = region->getEntryBlock();
            if (performTransformation(comp(), "%sinserted async check in acyclic region entry block %d\n", OPT_DETAILS, entryBlock->getNumber()))
               {
               TR_AsyncCheckInsertion::insertAsyncCheck(entryBlock, comp(), "redundantAsyncCheckRemoval/solidify");
               _numAsyncChecksInserted++;
               }
            }

         TR_RegionStructure::Cursor it(*region);
         for (TR_StructureSubGraphNode *subNode = it.getFirst(); subNode; subNode = it.getNext())
            if (subNode->getStructure())
               solidifySoftAsyncChecks(subNode);
         }
      }

   }

const char *
TR_RedundantAsyncCheckRemoval::optDetailString() const throw()
   {
   return "O^O REDUNDANT ASYNC CHECK REMOVAL: ";
   }

// --------------------------------------------------------
// Loop Estimator
// --------------------------------------------------------

uint32_t TR_LoopEstimator::estimateLoopIterationsUpperBound()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t numSymRefs = comp()->getSymRefTab()->getNumSymRefs();

   _numBlocks  = _cfg->getNextNodeNumber();

   if (trace())
      traceMsg(comp(), "==> Begin Processing Loop %d for iteration estimate\n", _loop->getNumber());

   // BitVector marking interesing symbol references
   //
   TR_BitVector candidates (numSymRefs, comp()->trMemory(), stackAlloc);

   // A list of all the exit conditions in the loop
   //
   TR_ScratchList<ExitCondition> conditions(trMemory());

   uint16_t index = 0;
   ListIterator<TR::CFGEdge> eit(&_loop->getExitEdges());
   for (TR::CFGEdge *edge = eit.getCurrent(); edge != 0; edge = eit.getNext())
      {
      TR::SymbolReference *symRef;
      TR::ILOpCodes op;
      TR_ProgressionKind kind;
      int64_t limit;

      // If the edge is recognizable, store information about it.
      // If we ever find an edge that is not recognizable, we pessimistically
      // throw in the towel an leave.. Since we do not know the control flow
      // inside the loop,
      //
      if (isRecognizableExitEdge(edge, &op, &symRef, &kind, &limit))
         {
         uint32_t refNum = symRef->getReferenceNumber();

         // If this symbol is a new candidate
         //
         if (!candidates.isSet(refNum))
            {
            symRef->getSymbol()->setLocalIndex(index);
            candidates.set(refNum);
            index++;
            }

         // Add the condition block to the list of conditions that we know about.
         //
         conditions.add(new (trStackMemory()) ExitCondition(op, symRef, limit));

         if (trace())
            traceMsg(comp(), "found candidate symbol #%d (%d) in condition block_%d\n",
                        refNum, symRef->getSymbol()->getLocalIndex(), edge->getFrom()->getNumber());
         }
      else
         {
         // An exit condition that does not look recognizable disables
         // us from making any upper bound estimate
         //
         return static_cast<uint32_t>(TR::getMaxSigned<TR::Int32>());
         }
      }

   if (index == 0)
      {
      return static_cast<uint32_t>(TR::getMaxSigned<TR::Int32>());
      }


   _numCandidates = index;
   _blockInfo = getBlockInfoArray();
   IncrementInfo **loopIncrements = getIncrementInfoArray();

   // Get Loop Increments
   //
   getLoopIncrements(candidates, loopIncrements);


   // Find the symbol reference which suggestes the maximum number of loop
   // iterations.  An increaing loop may have two conditions, say (i>20) and (i>40).
   // Our analysis does not tell us which one will be executed, or when.
   // Therefore, we must pick i>40 in our decision about the loop iterations,
   // and divide it by the minimum increment per loop iteration of T.
   //
   // This generalizes to multiple variables controlling the loop, we must pick
   // the one with the highest bound.
   //
   // Remember: this is not constant propagation.  This is cheaper, hence the
   // estimates might be more conservative.
   //
   // If a symbol is incremening in a direction contradictory to its condition
   // eg. for (; i > 40; i++) (ie. 0 or infinite iterations), then we must use
   // the initial value.  If it runs 0 times, ignore this variable, and look
   // at the rest of the loop controlling variables, else, simply return TR::getMaxSigned<TR::Int32>()
   // as the number of iterations. (here we are being conservative as well,
   // there might be an exit edge that causes us to leave the loop early..)
   //
   int32_t estimate = -1;
   ListIterator<ExitCondition> it(&conditions);
   for (ExitCondition *cond = it.getCurrent(); cond; cond = it.getNext())
      {
      uint32_t refNum = cond->_local->getReferenceNumber();
      uint32_t refIndex = cond->_local->getSymbol()->getLocalIndex();

      // make sure this is still a candidate
      //
      if (candidates.isSet(refNum))
         {
         IncrementInfo *iinfo = loopIncrements[refIndex];

         if (!iinfo || iinfo->isUnknownValue())
            {
            candidates.reset(refNum);
            if (trace())
               traceMsg(comp(), "Symbol %d has unknown increment value\n", refIndex);
            continue;
            }

         EntryInfo     *einfo = getEntryValueForSymbol(cond->_local);

         if (einfo->isUnknownValue() && iinfo->getKind() != Geometric)
            {
            candidates.reset(refNum);
            if (trace())
               traceMsg(comp(), "Symbol %d has unknown entry value\n", refNum);
            }
         else
            {
            int32_t incr  = iinfo->_incr;
            int32_t opCode= cond->_opCode;

            if (iinfo->getKind() == Geometric)
               {
               // For geometric induction variables with a well formed loop, the number
               // of loop iterations in bounded by 32.  We don't really care about the
               // limit value and the entry values.
               //

               // Get rid of the increasing infinite loop (// FIXME: is this an infinite loop?)
               //
               if (incr > 0 && (opCode == TR::ificmplt || opCode == TR::ificmple))
                  estimate = static_cast<int32_t>(TR::getMaxSigned<TR::Int32>());

               // Get rid of the decreasing infinite loop
               else if (incr < 0 && (opCode == TR::ificmpgt || opCode == TR::ificmpge))
                  estimate = static_cast<int32_t>(TR::getMaxSigned<TR::Int32>());

               else
                  {
                  if (trace())
                     traceMsg(comp(), "found geometric induction variable symbol #%d\n", refNum);
                  if (estimate < 32)
                     estimate = 32;
                  }
               }
            else
               {
               int32_t in    = einfo->_val;
               int32_t lim   = static_cast<int32_t>(cond->_limit);

               // increasing infinite loops
               if (incr > 0 && (opCode == TR::ificmplt || opCode == TR::ificmple) && in > lim)
                  estimate = static_cast<int32_t>(TR::getMaxSigned<TR::Int32>());

               // decreasing infinite loops
               else if (incr < 0 && (opCode == TR::ificmpgt || opCode == TR::ificmpge) && in < lim)
                  estimate = static_cast<int32_t>(TR::getMaxSigned<TR::Int32>());

               // messed up induction variable info
               else if (incr == 0)
                  estimate = static_cast<int32_t>(TR::getMaxSigned<TR::Int32>());

               else
                  {
                  int32_t diff = in - lim;
                  int32_t iters = (diff < 0) ? ceiling(-diff, incr) : ceiling(diff, -incr);

                  // The above calculation is approximate, it is possible that the number of
                  // iterations is calculated to be a negative number, because of commoning.
                  // Ignore these cases. Usually the diff is off the actual exact value
                  // by a difference of incr.
                  //
                  if (iters < 0) iters = 0;

                  if (trace())
                     {
                     traceMsg(comp(), "loop iterations estimate based upon symbol #%d: %d\n", refNum, iters);
                     traceMsg(comp(), "in val = %d, out val = %d, incr = %d\n", in, lim, incr);
                     }
                  if (iters > estimate)
                     estimate = iters;
                  }
               }
            }
         }

      if (estimate == TR::getMaxSigned<TR::Int32>()) break;
      }

   if (estimate == -1)
      return static_cast<int32_t>(TR::getMaxSigned<TR::Int32>());


   /**
   if (estimate < TR::getMaxSigned<TR::Int32>())
      printf(">>found good estimate in loop %d (%d times) in %s\n", _loop->getNumber(), estimate,
             signature(comp()->getCurrentMethod()));
   **/


   return estimate;
   }

void TR_LoopEstimator::getLoopIncrements(TR_BitVector &candidates, IncrementInfo **loopIncrements)
   {
   uint32_t headerIndex = _loop->getNumber();

   // Mark all blocks that are contained inside the loop region
   //
   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   _loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> bit(&blocksInLoop);
   TR::Block *block;
   TR_BitVector localBlocks(_numBlocks, comp()->trMemory(), stackAlloc);
   for (block = bit.getFirst(); block; block = bit.getNext())
      localBlocks.set(block->getNumber());

   // Visit the blocks in breadth first order
   //
   vcount_t visitCount = comp()->incVisitCount();
   TR_Queue<TR::Block> queue(trMemory());
   queue.enqueue(_loop->getEntryBlock());
   while (!queue.isEmpty())
      {
      block = queue.dequeue();

      if (block->getVisitCount() == visitCount)
         continue;

      // Make sure the predecessors of this Block have been visited (except for entry)
      //
      if (block->getNumber() != headerIndex)
         {
         bool notDone = false;
         TR_PredecessorIterator pit(block);
         for (TR::CFGEdge *edge = pit.getFirst(); edge && !notDone; edge = pit.getNext())
            {
            TR::Block *pred = toBlock(edge->getFrom());
            if (pred->getVisitCount() != visitCount)
               notDone = true;
            }

         if (notDone) continue;
         }

      block->setVisitCount(visitCount);

      processBlock(block, candidates);

      TR_SuccessorIterator sit(block);
      for (TR::CFGEdge *edge = sit.getFirst(); edge; edge = sit.getNext())
         {
         TR::Block *succ = toBlock(edge->getTo());
         uint32_t succIndex = succ->getNumber();

         // If this is a back edge, merge with loop info
         //
         if (succIndex == headerIndex)
            mergeWithLoopIncrements(block, loopIncrements);

         else if (localBlocks.isSet(succIndex)) // ignore exit edges
            queue.enqueue(succ);
         }
      }
   }

void TR_LoopEstimator::processBlock(TR::Block *block, TR_BitVector &candidates)
   {
   // Initialize Block Info
   //
   int32_t blockIndex = block->getNumber();

   TR_ASSERT(_blockInfo[blockIndex] == 0, "block already visited");
   IncrementInfo **inSymbols = _blockInfo[blockIndex] = getIncrementInfoArray();

   // Merge with all the precessors blocks
   //
   if (blockIndex != _loop->getNumber())
      {
      TR_PredecessorIterator pit(block);
      for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
         {
         TR::Block *pred = toBlock(edge->getFrom());

         for (int i = _numCandidates - 1; i >= 0; --i)
            {
            IncrementInfo *otherSymbol = _blockInfo[pred->getNumber()][i];
            IncrementInfo *inSymbol = inSymbols[i];

            if (otherSymbol)
               {
               if (!inSymbol)
                  inSymbols[i] = new (trStackMemory()) IncrementInfo(otherSymbol);
               else
                  inSymbol->merge(otherSymbol);
               }
            }
         }
      }


   // Process the contents of the block
   //
   TR::TreeTop *exitTreeTop = block->getExit();
   for (TR::TreeTop *treeTop = block->getFirstRealTreeTop();
        treeTop != exitTreeTop;
        treeTop = treeTop->getNextRealTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCode().isStoreDirect())
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         uint32_t refNum = symRef->getReferenceNumber();
         uint32_t refIndex = symRef->getSymbol()->getLocalIndex();

         if (candidates.isSet(refNum))
            {
            IncrementInfo *inSymbol = inSymbols[refIndex];

            if (!inSymbol)
               inSymbol = inSymbols[refIndex] = new (trStackMemory()) IncrementInfo(0);

            int32_t increment;
            TR_ProgressionKind kind;
            if (isProgressionalStore(node, &kind, &increment))
               {
               if (kind == Arithmetic)
                  inSymbol->arithmeticIncrement(increment);
               else if (kind == Geometric)
                  inSymbol->geometricIncrement(increment);
               }
            else
               inSymbol->setUnknownValue();
            }
         }
      }
   }

void TR_LoopEstimator::mergeWithLoopIncrements(TR::Block *block, IncrementInfo **loopIncrements)
   {
   IncrementInfo **outSymbols = _blockInfo[block->getNumber()];
   TR_ASSERT(outSymbols, "block info array not initialized");

   for (int i = _numCandidates - 1; i >= 0; --i)
      {
      IncrementInfo *outSymbol  = outSymbols[i];
      IncrementInfo *loopSymbol = loopIncrements[i];

      if (outSymbol)
         {
         if (!loopSymbol)
            loopIncrements[i] = new (trStackMemory()) IncrementInfo(outSymbol);
         else
            loopSymbol->merge(outSymbol);
         }
      }
   }


void TR_LoopEstimator::IncrementInfo::merge(IncrementInfo *other)
   {
   if (other->isUnknownValue() ||
       (_kind == Arithmetic && other->_kind == Geometric) ||
       (_kind == Geometric && other->_kind == Arithmetic))
      _unknown = true;
   else if (!_unknown)
      {
      if (_kind == Identity)
         _kind = other->_kind;

      // if signs are opposite, generalize
      //
      if ((other->_incr >> 31) != (_incr >> 31))
         setUnknownValue();
      else
         {
         if (_incr > 0)
            _incr = std::min(_incr, other->_incr);
         else
            _incr = std::max(_incr, other->_incr);
         }
      }
   }


bool TR_LoopEstimator::isRecognizableExitEdge(TR::CFGEdge *edge, TR::ILOpCodes *op,
                                              TR::SymbolReference **ref, TR_ProgressionKind *prog, int64_t *limit)
   {
   TR_StructureSubGraphNode *subNode = toStructureSubGraphNode(edge->getFrom());

   if (subNode->getStructure()->asRegion())
      return false;

   TR::Block *block = subNode->getStructure()->asBlock()->getBlock();

   if (!block) return false;

   TR::TreeTop *branchTree = block->getLastRealTreeTop();
   TR::Node    *node = branchTree->getNode();
   TR::SymbolReference *symRef;
   TR::ILOpCode opCode;
   TR::ILOpCodes opValue;
   int64_t konst;

   if ( node->getOpCode().isJumpWithMultipleTargets())
      return false;

   if (!node->getOpCode().isBranch())
      {
      // if the block has a single predecessor, look at the predecessor instead
      //
      TR::CFGEdgeList &predList = subNode->getPredecessors();
      if ((predList.size() == 1) && subNode->getExceptionPredecessors().empty())
         return isRecognizableExitEdge(predList.front(),
                                       op, ref, prog, limit);
      return false;
      }

   opCode = node->getOpCode();
   opValue = opCode.getOpCodeValue();

   if (!(opValue == TR::ificmplt ||
         opValue == TR::ificmpge ||
         opValue == TR::ificmpgt ||
         opValue == TR::ificmple ))
       return false;  // FIXME: can do better for eq and ne cases.


   int32_t incr;
   TR_ProgressionKind kind;
   if (!getProgression(node->getFirstChild(), &symRef, &kind, &incr))
      return false;

   if (kind != Geometric)
      {
      TR::Node *secondChild = node->getSecondChild();
      if (!secondChild->getOpCode().isLoadConst())
         return false;

      konst = secondChild->getInt() - incr;
      }
   else
      {
      // For geometric series, it is good if we know the exit limit
      // but the number of loop iterations is bounded even when we don't the limiting var.
      //
      TR::Node *secondChild = node->getSecondChild();
      if (secondChild->getOpCode().isLoadConst())
         {
         konst = secondChild->getInt();
         konst = (incr > 0) ? konst << incr : konst >> -incr;
         }
      else
         {
         konst = (incr > 0) ? INT_MAX : 0;
         }
      }

   int32_t exitDestIndex = toStructureSubGraphNode(edge->getTo())->getNumber();
   int32_t nextBlockIndex = block->getNextBlock()->getNumber();
   bool branchToExit = (exitDestIndex != nextBlockIndex);

   if (branchToExit)
      {
      /* info is correct */
      }
   else
      {
      opValue = opCode.getOpCodeForReverseBranch();
      }

   *op = opValue;
   *ref = symRef;
   *limit = konst;
   *prog = kind;
   return true;
   }


TR_LoopEstimator::IncrementInfo ***TR_LoopEstimator::getBlockInfoArray()
   {
   IncrementInfo ***array = (IncrementInfo ***) trMemory()->allocateStackMemory(_numBlocks*sizeof(IncrementInfo **));
   memset(array, 0, _numBlocks * sizeof(IncrementInfo **));
   return array;
   }

TR_LoopEstimator::IncrementInfo **TR_LoopEstimator::getIncrementInfoArray()
   {
   IncrementInfo **array = (IncrementInfo **) trMemory()->allocateStackMemory(_numCandidates * sizeof(IncrementInfo *));
   memset(array, 0, _numCandidates * sizeof(IncrementInfo *));
   return array;
   }

TR_LoopEstimator::EntryInfo **TR_LoopEstimator::getEntryInfoArray()
   {
   EntryInfo **array = (EntryInfo **) trMemory()->allocateStackMemory(_numBlocks * sizeof(EntryInfo *));
   memset(array, 0, _numBlocks * sizeof(EntryInfo *));
   return array;
   }

// Returns true if the node is an increment of a symbol reference
//
//  istore #24  <--- same sym ref
//    iadd           |
//      iload #24 <--' (could be an iadd/isub subtree)
//      iconst 304
//
// The increment is stored in the location pointed to by incr
//
bool TR_LoopEstimator::isProgressionalStore(TR::Node *node, TR_ProgressionKind *kind, int32_t *incr)
   {
   TR::Node *cursor = node->getFirstChild();
   TR::SymbolReference *storeRef = node->getSymbolReference();

   // skip over conversion nodes
   //
   while (cursor->getOpCode().isConversion())
      cursor = cursor->getFirstChild();

   if (! (cursor->getOpCode().isAdd() || cursor->getOpCode().isSub() ||
          cursor->getOpCode().isLeftShift() || cursor->getOpCode().isRightShift()))
      return false;

   TR::SymbolReference *symRef;
   if (!getProgression(cursor, &symRef, kind, incr))
      return false;

   if (symRef != storeRef) // must be an increment
      return false;

   return true;
   }

// Finds the progession taking place on an induction variable.  Right now this method recognizes arithmetic
// progressions of all kinds and Geometric progressions where the multiplicative constant (r) is a power of 2 (shift).
//
// Returns true if it recognizes a progression taking place.  Returns the symbol reference of the induction
// variable in ref, the kind of progression in prog, and the progression constant in incr.
//
// For identity progression incr has no meaning.
// For arithmetic progressions, incr contains the increment value.
// For geometric progressions, incr represents the multiplicative constant. geometric progression.
// The multiplicative constant for the series (r) = 2^(incr).  Naturally this means
//   that the series is a shrinking series if incr is negative.
//
bool TR_LoopEstimator::getProgression(TR::Node *expr, TR::SymbolReference **ref, TR_ProgressionKind *prog, int32_t *incr)
   {
   int32_t konst;
   TR_ProgressionKind kind;

   TR::Node *secondChild = expr->getNumChildren() > 1 ? expr->getSecondChild() : 0;

   if (expr->getOpCode().isAdd() && secondChild->getOpCode().isLoadConst())
      {
      if (!getProgression(expr->getFirstChild(), ref, &kind, &konst))
         return false;
      if (kind == Geometric) return false;
      konst += secondChild->getInt();
      *prog = (incr == 0) ? Identity : Arithmetic;
      }
   else if (expr->getOpCode().isSub() && secondChild->getOpCode().isLoadConst())
      {
      if (!getProgression(expr->getFirstChild(), ref, &kind, &konst))
         return false;
      if (kind == Geometric) return false;
      konst -= secondChild->getInt();
      *prog = (incr == 0) ? Identity : Arithmetic;
      }
   else if (expr->getOpCode().isLeftShift() && secondChild->getOpCode().isLoadConst())
      {
      if (!getProgression(expr->getFirstChild(), ref, &kind, &konst))
         return false;
      if (kind == Arithmetic) return false;
      konst += secondChild->getInt();
      *prog = (incr == 0) ? Identity : Geometric;
      }
   else if (expr->getOpCode().isRightShift() && secondChild->getOpCode().isLoadConst())
      {
      if (!getProgression(expr->getFirstChild(), ref, &kind, &konst))
         return false;
      if (kind == Arithmetic) return false;
      konst -= secondChild->getInt();
      *prog = (incr == 0) ? Identity : Geometric;
      }
   else if (expr->getOpCode().isLoadDirect())
      {
      if (expr->getOpCode().hasSymbolReference())
         {
         if (!expr->getSymbolReference()->getSymbol()->isAutoOrParm())
            return false;
         *ref = expr->getSymbolReference();
         konst = 0;
         *prog = Identity;
         }
      else
         return false;
      }
   else if (expr->getOpCode().isConversion())
      return (getProgression(expr->getFirstChild(), ref, prog, incr));
   else
      return false;

   *incr = konst;
   return true;
   }

TR_LoopEstimator::EntryInfo *TR_LoopEstimator::getEntryValue(TR::Block *block, TR::SymbolReference *symRef, TR_BitVector &nodesDone, EntryInfo **entryInfos)
   {
   uint32_t blockIndex = block->getNumber();

   if (nodesDone.isSet(blockIndex))
      return entryInfos[blockIndex];
   nodesDone.set(blockIndex);

   EntryInfo *myInfo = 0;
   TR::TreeTop *entryTree = block->getEntry();

   if (!entryTree)
      {
      if (symRef->getSymbol()->isParm())
         {
         entryInfos[blockIndex] = myInfo = new (trStackMemory()) EntryInfo(); // parameters have unintialized values
         }
      else
         TR_ASSERT(entryTree, "unintialized local discovered.  Uninitialized locals cannot exist in Java");

      return myInfo;
      }

   for (TR::TreeTop *treeTop = block->getLastRealTreeTop();
        treeTop != entryTree;
        treeTop = treeTop->getPrevRealTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();

      if (node->getOpCode().isStoreDirect())
         {
         TR::SymbolReference *ref = node->getSymbolReference();
         if (ref->getReferenceNumber() == symRef->getReferenceNumber())
            {
            if (node->getFirstChild()->getOpCode().isLoadConst())
               entryInfos[blockIndex] = myInfo = new (trStackMemory()) EntryInfo(node->getFirstChild()->getInt());
            else
               entryInfos[blockIndex] = myInfo = new (trStackMemory()) EntryInfo(); // unknown value
            }
         }
      }

   if (myInfo == 0)
      {
      TR_PredecessorIterator pit(block);
      for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
         {
         TR::Block *pred = toBlock(edge->getFrom());
         EntryInfo *predInfo = getEntryValue(pred, symRef, nodesDone, entryInfos);

         if (predInfo)
            {
            if (!myInfo)
               entryInfos[blockIndex] = myInfo = new (trStackMemory()) EntryInfo(predInfo);
            else
               myInfo->merge(predInfo);
            }
         }
      }

   return myInfo;
   }

static bool internalEdge(TR_RegionStructure *loop, TR::Block *block)
   {
   TR_RegionStructure::Cursor it(*loop);
   for (TR_StructureSubGraphNode *node = it.getCurrent(); node; node = it.getNext())
      {
      TR_BlockStructure *b = node->getStructure()->asBlock();
      if (!b)
         {
         if (internalEdge(node->getStructure()->asRegion(), block))
            return true;
         }
      else
         {
         if (b->getBlock() == block) return true;
         }
      }
   return false;
   }

TR_LoopEstimator::EntryInfo *TR_LoopEstimator::getEntryValueForSymbol(TR::SymbolReference *symRef)
   {
   TR::Block *header = _loop->getEntryBlock();

   TR_BitVector nodesDone(_numBlocks, comp()->trMemory(), stackAlloc);
   nodesDone.set(header->getNumber());

   EntryInfo **entryInfos = getEntryInfoArray();
   EntryInfo *myInfo = 0;

   TR_PredecessorIterator pit(header);
   for (TR::CFGEdge *edge = pit.getFirst(); edge; edge = pit.getNext())
      {
      TR::Block *pred = toBlock(edge->getFrom());

      // ignore backedges
      //
      if (internalEdge(_loop, pred))
         continue;

      EntryInfo *predInfo = getEntryValue(pred, symRef, nodesDone, entryInfos);

      if (predInfo)
         {
         if (!myInfo)
            myInfo = predInfo;
         else
            myInfo->merge(predInfo);
         }
      else
         {
         if (!myInfo)
            TR_ASSERT(0, "Couldn't find store for entry value");
         }
      }

   return myInfo;
   }

void TR_LoopEstimator::EntryInfo::merge(EntryInfo *other)
   {
   if (other->_unknown)
      setUnknownValue();
   else if (!_unknown)
      if (other->_val != _val)
         setUnknownValue();
   }
