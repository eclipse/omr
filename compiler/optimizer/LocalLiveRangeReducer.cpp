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

#include "optimizer/LocalLiveRangeReducer.hpp"

#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "ras/Debug.hpp"

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
#include "env/StackMemoryRegion.hpp"
#endif

#define OPT_DETAILS "O^O LOCAL LIVE RANGE REDUCTION: "

TR_LocalLiveRangeReduction::TR_LocalLiveRangeReduction(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _movedTreesList(trMemory()), _depPairList(trMemory())
   {}

void TR_LocalLiveRangeReduction::prePerformOnBlocks()
   {
   comp()->incVisitCount();
    int32_t symRefCount = comp()->getSymRefCount();
   _temp = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
   }

void TR_LocalLiveRangeReduction::postPerformOnBlocks()
   {
   comp()->incVisitCount();
   }

int32_t TR_LocalLiveRangeReduction::perform()
   {
   if (TR::Compiler->target.cpu.isZ())
      return false;

   TR::TreeTop * exitTT, * nextTT;
   TR::Block *b;
   TR::TreeTop * tt;

   //calculate number of TreeTops in each bb (or extended bb)
   for (tt = comp()->getStartTree(); tt; tt = nextTT)
      {
      TR::StackMemoryRegion stackMemoryRegion(*trMemory());

      TR::Node *node = tt->getNode();
      b = node->getBlock();
      exitTT = b->getExit();
      _numTreeTops = b->getNumberOfRealTreeTops()+2; //include both BBStart/BBend

      //support for extended blocks
      while ((nextTT = exitTT->getNextTreeTop()) && (b = nextTT->getNode()->getBlock(), b->isExtensionOfPreviousBlock()))
         {

         _numTreeTops += b->getNumberOfRealTreeTops()+2;
         exitTT = b->getExit();
         }

      _treesRefInfoArray = (TR_TreeRefInfo**)trMemory()->allocateStackMemory(_numTreeTops*sizeof(TR_TreeRefInfo*));
      memset(_treesRefInfoArray, 0, _numTreeTops*sizeof(TR_TreeRefInfo*));
      _movedTreesList.deleteAll();
      _depPairList.deleteAll();
      transformExtendedBlock(tt,exitTT->getNextTreeTop());
      }

   if (trace())
      traceMsg(comp(), "\nEnding LocalLiveRangeReducer\n");

   return 2;
   }


//exitTree is the next treeTop after the relevant range of TreeTops (start of next block)
bool TR_LocalLiveRangeReduction::transformExtendedBlock(TR::TreeTop *entryTree, TR::TreeTop *exitTree)
   {
   TR::TreeTop *currentTree = exitTree;

   if (!performTransformation(comp(), "%sBlock %d\n", OPT_DETAILS, entryTree->getNode()->getBlock()->getNumber()))
      return false;

   //Gather information for each tree regarding its first/mid/last-ref-nodes.
   //And populate list of TreesRefInfo
   collectInfo(entryTree, exitTree);

   /******************* pass 1 ********************************************/
   //Move "interesting trees" down as close as possible to the last ref node.
   for (int32_t i =0; i< _numTreeTops; )
      {
      bool movedFlag=false;
      TR_TreeRefInfo *currentTree = _treesRefInfoArray[i];

      if (isNeedToBeInvestigated(currentTree))
         movedFlag = investigateAndMove(currentTree,1);

      if (!movedFlag)
         i++;
      }

   //if none moved return;
   if (_movedTreesList.isEmpty())
      return true;

   //Update DependenciesList after pass 1.
   //Remove dependencies which are not of interest (i.e. anchor didn't move).
   updateDepList();

   //if in all pairs the anchor didn't move, no need for second pass.
   if (_depPairList.isEmpty())
      return true;

   _movedTreesList.deleteAll();
   /******************* pass 2 ********************************************/
   //Try to move trees that were blcok by a tree that later has been moved.
   //Note that since the list adds elements to the head, we can iterate the list, and by this to cover chain of moves (the later one will be in front)
   //e.g. if a blocked by b and then b moved after c. The list will be b-c;a-b

   ListIterator<DepPair> listIt(&_depPairList);
   for (DepPair * depPair = listIt.getFirst(); depPair != NULL; depPair = listIt.getNext())
      {
      TR_TreeRefInfo *treeToMove = depPair->getDep();
      if (isNeedToBeInvestigated(treeToMove))
         investigateAndMove(treeToMove,2);
      }

   return true;
   }

//---------------------------- collecting ref info at the beginning -----------------------------------------
void TR_LocalLiveRangeReduction::collectInfo(TR::TreeTop *entryTree,TR::TreeTop *exitTree)
   {

   TR::TreeTop *currentTree = entryTree;
   TR_TreeRefInfo *treeRefInfo;
   int32_t i = 0;
   int32_t maxRefCount = 0;
   vcount_t visitCount = comp()->getVisitCount();

   while (!(currentTree == exitTree))
      {
      treeRefInfo = new (trStackMemory()) TR_TreeRefInfo(currentTree, trMemory());
      collectRefInfo(treeRefInfo, currentTree->getNode(),visitCount,&maxRefCount);
      _treesRefInfoArray[i++] = treeRefInfo;
      initPotentialDeps(treeRefInfo);
      treeRefInfo->resetSyms();
      populatePotentialDeps(treeRefInfo,treeRefInfo->getTreeTop()->getNode());
      currentTree = currentTree->getNextTreeTop();
      }

   comp()->setVisitCount(visitCount+maxRefCount);

   }


void  TR_LocalLiveRangeReduction::collectRefInfo(TR_TreeRefInfo *treeRefInfo,TR::Node *node,vcount_t compVisitCount, int32_t *maxRefCount)
   {
   vcount_t visitCount = node->getVisitCount();
   if (node->getReferenceCount() > 1)
      {
      if (node->getReferenceCount()> *maxRefCount)
         *maxRefCount = node->getReferenceCount();

      if (visitCount >= compVisitCount) //already visited
         {
         node->incVisitCount() ;
         if (visitCount+1 == (compVisitCount+node->getReferenceCount()-1))
            treeRefInfo-> getLastRefNodesList()->add(node);
         else
            treeRefInfo-> getMidRefNodesList()->add(node);
         return;          //don't recurse on children of => nodes
         }
      else
         {
         treeRefInfo->getFirstRefNodesList()->add(node);
         node->setVisitCount(compVisitCount);
         }

      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      collectRefInfo(treeRefInfo, child, compVisitCount,maxRefCount);
      }

   }


// -------------------------------         pass 1          ----------------------------------------------------------

bool TR_LocalLiveRangeReduction::investigateAndMove(TR_TreeRefInfo *treeRefInfo,int32_t passNumber)
   {

   if (isWorthMoving(treeRefInfo))                 //worth moving
      {
      TR_TreeRefInfo *anchor = findLocationToMove(treeRefInfo);
      if (!moveTreeBefore(treeRefInfo,anchor,passNumber))  //Move tree before anchor
         return false;

      if(passNumber==1)
         {
         _movedTreesList.add(treeRefInfo);
         addDepPair(treeRefInfo, anchor);          //update Dependencies
         }

      return true; //moved
      }

   return false;
   }

//
bool TR_LocalLiveRangeReduction::isWorthMoving(TR_TreeRefInfo *tree)
   {
   bool usesRegisterPairsForLongs = cg()->usesRegisterPairsForLongs();
   int32_t numFirstRefNodesFloat=0;
   int32_t numFirstRefNodesInt=0;
   int32_t numLastRefNodesFloat=0;
   int32_t numLastRefNodesInt=0;
   TR::Node *node;


   //check first references
   ListIterator<TR::Node> listIt(tree->getFirstRefNodesList());
   for ( node = listIt.getFirst(); node != NULL; node = listIt.getNext())
      {
      TR::ILOpCode &opCode = node->getOpCode();
      if (opCode.isFloatingPoint())
         numFirstRefNodesFloat++;
      else
         {
         //all integers, signed and unsined

         if (opCode.isLong()&& usesRegisterPairsForLongs)
            numFirstRefNodesInt+=2;
         else
            numFirstRefNodesInt++;
         }
      }
   //check last references
   listIt.set(tree->getLastRefNodesList());
   for ( node = listIt.getFirst(); node != NULL; node = listIt.getNext())
      {
      TR::ILOpCode &opCode = node->getOpCode();
      if (opCode.isFloatingPoint())
         numLastRefNodesFloat++;
      else
         {
         //all integers, signed and unsined
         if (opCode.isLong()&& usesRegisterPairsForLongs)
            numLastRefNodesInt+=2;
         else
            numLastRefNodesInt++;
         }
      }


   if (((numLastRefNodesInt < numFirstRefNodesInt) &&
        (numLastRefNodesFloat <= numFirstRefNodesFloat)) ||
       ((numLastRefNodesFloat < numFirstRefNodesFloat) &&
        (numLastRefNodesInt <= numFirstRefNodesInt)))
      return true;

   return false;
   }

//Returns the location before which needs to move the tree.
TR_TreeRefInfo * TR_LocalLiveRangeReduction::findLocationToMove(TR_TreeRefInfo *treeToMove)
   {

   int32_t i = getIndexInArray(treeToMove)+1;
   //looping recursivly to find if any tree has a constraint

   TR_TreeRefInfo *currentTree;
   for (;i<_numTreeTops; i++)
      {
      currentTree = _treesRefInfoArray[i];
      TR::ILOpCode &opCode =currentTree->getTreeTop()->getNode()->getOpCode();
      if (opCode.isBranch() || opCode.isReturn() || opCode.isGoto() || opCode.isJumpWithMultipleTargets())
         return currentTree;

      //support for extended blocks  - don't move beyond BBEnd
      //note - can be more specific about things that can move across.
      if (opCode.getOpCodeValue() == TR::BBEnd)
         return currentTree;

      if (isAnyDataConstraint(currentTree,treeToMove) ||
          isAnySymInDefinedOrUsedBy(currentTree,currentTree->getTreeTop()->getNode(),treeToMove) ||
          (matchFirstOrMidToLastRef(currentTree,treeToMove)))
         return currentTree;
      }

   return NULL;
   }


void TR_LocalLiveRangeReduction::initPotentialDeps(TR_TreeRefInfo *tree)
   {
   int32_t symRefCount = comp()->getSymRefCount();
   if (tree->getDefSym() == NULL)
      tree->setDefSym(new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc));
   if (tree->getUseSym() == NULL)
      tree->setUseSym(new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc));
   }


void TR_LocalLiveRangeReduction::populatePotentialDeps(TR_TreeRefInfo *treeRefInfo,TR::Node *node)
   {
   TR::ILOpCode &opCode = node->getOpCode();
   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t symRefNum = symRef->getReferenceNumber();

      //set defSym - all symbols that might be written

      if (opCode.isCall() || opCode.isResolveCheck()|| opCode.isStore() || node->mightHaveVolatileSymbolReference())
         {

         bool isCallDirect = false;
         if (node->getOpCode().isCallDirect())
            isCallDirect = true;

         if (!symRef->getUseDefAliases(isCallDirect).isZero(comp()))
            {
            TR::SparseBitVector useDefAliases(comp()->allocator());
            symRef->getUseDefAliases(isCallDirect).getAliases(useDefAliases);
            TR::SparseBitVector::Cursor aliasCursor(useDefAliases);
            for (aliasCursor.SetToFirstOne(); aliasCursor.Valid(); aliasCursor.SetToNextOne())
               {
               int32_t nextAlias = aliasCursor;
               treeRefInfo->getDefSym()->set(nextAlias);
               }
            }

         if (opCode.isStore())
            treeRefInfo->getDefSym()->set(symRefNum);
         }
      //set useSym - all symbols that are used
      if (opCode.canRaiseException())
         {
         TR::SparseBitVector useAliases(comp()->allocator());
         symRef->getUseonlyAliases().getAliases(useAliases);
            {
            TR::SparseBitVector::Cursor aliasesCursor(useAliases);
            for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
               {
               int32_t nextAlias = aliasesCursor;
               treeRefInfo->getUseSym()->set(nextAlias);
               }
            }
         }
      if (opCode.isLoadVar() || (opCode.getOpCodeValue() == TR::loadaddr))
         {
         treeRefInfo->getUseSym()->set(symRefNum);
         }

      }
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);

      //don't recurse over references (nodes which are not the first reference)
      //
      if (child->getReferenceCount()==1 || treeRefInfo->getFirstRefNodesList()->find(child))
         populatePotentialDeps(treeRefInfo,child );
      }
   return;
   }


bool TR_LocalLiveRangeReduction::isAnyDataConstraint(TR_TreeRefInfo *currentTreeRefInfo, TR_TreeRefInfo *movingTreeRefInfo )
   {
      //check if there is a symbol used or written by the moving tree and written by the current
      *_temp = *movingTreeRefInfo->getUseSym();
      *_temp |= *movingTreeRefInfo->getDefSym();
      *_temp &= *currentTreeRefInfo->getDefSym();
      if (!_temp->isEmpty())
         return true;

      //check if there is a symbol written by the moving tree and used by current
      *_temp = *movingTreeRefInfo->getDefSym();
      *_temp &= *currentTreeRefInfo->getUseSym();
      if (!_temp->isEmpty())
         return true;

      return false;
   }

static bool mayBeObjectHeaderStore(TR::Node *node, TR_FrontEnd *fe)
   {
   node = node->getStoreNode();
   TR_ASSERT(fe->getObjectHeaderSizeInBytes() <= TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), "assertion failure");
   if (node && node->getSymbol()->getOffset() < TR::Compiler->om.contiguousArrayHeaderSizeInBytes())
      return true;
   else
      return false;
   }

static bool nodeMaybeMonitor(TR::Node *node)
   {
   if ((node->getOpCodeValue() == TR::monent) ||
         (node->getOpCodeValue() == TR::monexit) ||
         (node->getOpCode().isStore() && node->getSymbol()->holdsMonitoredObject()))
      return true;
   return false;
   }


// Returns true if there is any constraint to the move
bool TR_LocalLiveRangeReduction::isAnySymInDefinedOrUsedBy(TR_TreeRefInfo *currentTreeRefInfo, TR::Node *currentNode, TR_TreeRefInfo *movingTreeRefInfo )
   {
   TR::Node *movingNode = movingTreeRefInfo->getTreeTop()->getNode();
   // ignore anchors
   //
   if (movingNode->getOpCode().isAnchor())
      movingNode = movingNode->getFirstChild();

   TR::ILOpCode &opCode = currentNode->getOpCode();

   ////if ((opCode.getOpCodeValue() == TR::monent) || (opCode.getOpCodeValue() == TR::monexit))
   if (nodeMaybeMonitor(currentNode))
      {
      if (trace())
    	 traceMsg(comp(),"cannot move %p beyond monitor %p\n",movingNode,currentNode);
      return true;
      }

   // Don't move gc points or things across gc points
   //
   if (movingNode->canGCandReturn() ||
         currentNode->canGCandReturn())
      {
      if (trace())
         traceMsg(comp(), "cannot move gc points %p past %p\n", movingNode, currentNode);
      return true;
      }

   // Don't move checks or calls at all
   //
   if (containsCallOrCheck(movingTreeRefInfo,movingNode))
      {
      if (trace())
    	   traceMsg(comp(),"cannot move check or call %s\n", getDebug()->getName(movingNode));
      return true;
      }

   // Don't move object header store past a GC point
   //
   if ((currentNode->getOpCode().isWrtBar() || currentNode->canCauseGC()) && mayBeObjectHeaderStore(movingNode, fe()))
      {
      if (trace())
    	   traceMsg(comp(),"cannot move possible object header store %s past GC point %s\n", getDebug()->getName(movingNode), getDebug()->getName(currentNode));
      return true;
      }

   if (TR::Compiler->target.cpu.isPower() && opCode.getOpCodeValue() == TR::allocationFence)
      {
      // Can't move allocations past flushes
      if (movingNode->getOpCodeValue() == TR::treetop &&
          movingNode->getFirstChild()->getOpCode().isNew() &&
          (currentNode->getAllocation() == NULL ||
           currentNode->getAllocation() == movingNode->getFirstChild()))
         {
         if (trace())
            {
            traceMsg(comp(),"cannot move %p beyond flush %p - ", movingNode, currentNode);
            if (currentNode->getAllocation() == NULL)
               traceMsg(comp(),"(flush with null allocation)\n");
            else
               traceMsg(comp(),"(flush for allocation %p)\n", currentNode->getAllocation());
            }
         return true;
         }

      // Can't move certain stores past flushes
      // Exclude all indirect stores, they may be for stack allocs, in which case the flush is needed at least as a scheduling barrier
      // Direct stores to autos and parms are the only safe candidates
      if (movingNode->getOpCode().isStoreIndirect() ||
          (movingNode->getOpCode().isStoreDirect() && !movingNode->getSymbol()->isParm() && !movingNode->getSymbol()->isAuto()))
         {
         if (trace())
            traceMsg(comp(),"cannot move %p beyond flush %p - (flush for possible stack alloc)", movingNode, currentNode);
         return true;
         }
      }

   for (int32_t i = 0; i < currentNode->getNumChildren(); i++)
      {
      TR::Node *child = currentNode->getChild(i);

      //Any node that has side effects (like call and newarrya) cannot be evaluated in the middle of the tree.
      if (movingTreeRefInfo->getFirstRefNodesList()->find(child))
         {
         //for calls and unresolve symbol that are not under check

         if (child->exceptionsRaised() ||
             (child->getOpCode().hasSymbolReference() && child->getSymbolReference()->isUnresolved()))
    	    {
    	    if (trace())
    	       traceMsg(comp(),"cannot move %p beyond %p - cannot change evaluation point of %p\n ",movingNode,currentTreeRefInfo->getTreeTop()->getNode(),child);
            return true;
    	    }

         else if(movingNode->getOpCode().isStore())
            {
            TR::SymbolReference *stSymRef = movingNode->getSymbolReference();
            int32_t stSymRefNum = stSymRef->getReferenceNumber();
            //TR::SymbolReference *stSymRef = movingNode->getSymbolReference();
            int32_t numHelperSymbols = comp()->getSymRefTab()->getNumHelperSymbols();
            if ((comp()->getSymRefTab()->isNonHelper(stSymRefNum, TR::SymbolReferenceTable::vftSymbol))||
                (comp()->getSymRefTab()->isNonHelper(stSymRefNum, TR::SymbolReferenceTable::contiguousArraySizeSymbol))||
                (comp()->getSymRefTab()->isNonHelper(stSymRefNum, TR::SymbolReferenceTable::discontiguousArraySizeSymbol))||
                (stSymRef == comp()->getSymRefTab()->findHeaderFlagsSymbolRef())||
                (stSymRef->getSymbol() == comp()->getSymRefTab()->findGenericIntShadowSymbol()))

               return true;
            }

         else if (movingNode->getOpCode().isResolveOrNullCheck())
            {
    	    if (trace())
    	       traceMsg(comp(),"cannot move %p beyond %p - node %p under ResolveOrNullCheck",movingNode,currentTreeRefInfo->getTreeTop()->getNode(),currentNode);
            return true;
            }

    	 else if (TR::Compiler->target.is64Bit() &&
    		  movingNode->getOpCode().isBndCheck() &&
    		  ((opCode.getOpCodeValue() == TR::i2l) || (opCode.getOpCodeValue() == TR::iu2l)) &&
    		  !child->isNonNegative())
    	    {
    	    if (trace())
    	       traceMsg(comp(),"cannot move %p beyond %p - changing the eval point of %p will casue extra cg instruction ",movingNode,currentTreeRefInfo->getTreeTop()->getNode(),currentNode);
    	    return true;
    	    }
         }

      //don't recurse over nodes each are not the first reference
      if (child->getReferenceCount()==1 || currentTreeRefInfo->getFirstRefNodesList()->find(child))
         {
         if (isAnySymInDefinedOrUsedBy(currentTreeRefInfo, child, movingTreeRefInfo ))
            return true;
         }
      }

   return false;
   }


bool TR_LocalLiveRangeReduction::moveTreeBefore(TR_TreeRefInfo *treeToMove,TR_TreeRefInfo *anchor,int32_t passNumber)
   {
   TR::TreeTop *treeToMoveTT = treeToMove->getTreeTop();
   TR::TreeTop *anchorTT = anchor->getTreeTop();
   if (treeToMoveTT->getNextRealTreeTop() == anchorTT)
      {
      addDepPair(treeToMove, anchor);
      return false;
      }

   if (!performTransformation(comp(), "%sPass %d: moving tree [%p] before Tree %p\n", OPT_DETAILS, passNumber, treeToMoveTT->getNode(),anchorTT->getNode()))
      return false;

   //   printf("Moving [%p] before Tree %p\n",  treeToMoveTT->getNode(),anchorTT->getNode());


   //changing location in block
   TR::TreeTop *origPrevTree = treeToMoveTT->getPrevTreeTop();
   TR::TreeTop *origNextTree = treeToMoveTT->getNextTreeTop();
   origPrevTree->setNextTreeTop(origNextTree);
   origNextTree->setPrevTreeTop(origPrevTree);
   TR::TreeTop *prevTree = anchorTT->getPrevTreeTop();
   anchorTT->setPrevTreeTop(treeToMoveTT);
   treeToMoveTT->setNextTreeTop(anchorTT);
   treeToMoveTT->setPrevTreeTop(prevTree);
   prevTree->setNextTreeTop(treeToMoveTT);

   //UPDATE REFINFO
   //find locations of treeTops in TreeTopsRefInfo array
   //startIndex points to the currentTree that has moved
   //endIndex points to the treeTop after which we moved the tree (nextTree)

   int32_t startIndex = getIndexInArray(treeToMove);
   int32_t endIndex = getIndexInArray(anchor)-1;
   int32_t i=0;
   for ( i = startIndex+1; i<= endIndex ; i++)
      {
      TR_TreeRefInfo *currentTreeRefInfo = _treesRefInfoArray[i];
      List<TR::Node> *firstList = currentTreeRefInfo->getFirstRefNodesList();
      List<TR::Node> *midList = currentTreeRefInfo->getMidRefNodesList();
      List<TR::Node> *lastList = currentTreeRefInfo->getLastRefNodesList();
      List<TR::Node> *M_firstList = treeToMove->getFirstRefNodesList();
      List<TR::Node> *M_midList = treeToMove->getMidRefNodesList();
      List<TR::Node> *M_lastList = treeToMove->getLastRefNodesList();

      if (trace())
    	 {
    	 traceMsg(comp(),"Before move:\n");
    	 printRefInfo(treeToMove);
    	 printRefInfo(currentTreeRefInfo);
    	 }

      updateRefInfo(treeToMove->getTreeTop()->getNode(), currentTreeRefInfo, treeToMove , false);
      treeToMove->resetSyms();
      currentTreeRefInfo->resetSyms();
      populatePotentialDeps(currentTreeRefInfo,currentTreeRefInfo->getTreeTop()->getNode());
      populatePotentialDeps(treeToMove,treeToMove->getTreeTop()->getNode());

      if (trace())
    	 {
    	 traceMsg(comp(),"After move:\n");
    	 printRefInfo(treeToMove);
    	 printRefInfo(currentTreeRefInfo);
    	 traceMsg(comp(),"------------------------\n");
    	 }
      }

   TR_TreeRefInfo *temp = _treesRefInfoArray[startIndex];
   for (i = startIndex; i< endIndex ; i++)
      {
      _treesRefInfoArray[i] = _treesRefInfoArray[i+1];
      }

   _treesRefInfoArray[endIndex]=temp;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   if (!(comp()->getOption(TR_EnableParanoidOptCheck) || debug("paranoidOptCheck")))
      return true;

   //verifier
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   vcount_t visitCount = comp()->getVisitCount();
   int32_t maxRefCount = 0;
   TR::TreeTop *tt;
   TR_TreeRefInfo **treesRefInfoArrayTemp = (TR_TreeRefInfo**)trMemory()->allocateStackMemory(_numTreeTops*sizeof(TR_TreeRefInfo*));
   memset(treesRefInfoArrayTemp, 0, _numTreeTops*sizeof(TR_TreeRefInfo*));
   TR_TreeRefInfo *treeRefInfoTemp;


   //collect info
   for ( int32_t i  = 0; i<_numTreeTops-1; i++)
      {
      tt =_treesRefInfoArray[i]->getTreeTop();
      treeRefInfoTemp = new (trStackMemory()) TR_TreeRefInfo(tt, trMemory());
      collectRefInfo(treeRefInfoTemp, tt->getNode(),visitCount,&maxRefCount);
      treesRefInfoArrayTemp[i] = treeRefInfoTemp;
      }

   comp()->setVisitCount(visitCount+maxRefCount);

   for ( int32_t i  = 0; i<_numTreeTops-1; i++)
      {
      if (!verifyRefInfo(treesRefInfoArrayTemp[i]->getFirstRefNodesList(),_treesRefInfoArray[i]->getFirstRefNodesList()))
    	 {
    	 printOnVerifyError(_treesRefInfoArray[i],treesRefInfoArrayTemp[i]);
    	 TR_ASSERT(0,"fail to verify firstRefNodesList for %p\n",_treesRefInfoArray[i]->getTreeTop()->getNode());
    	 }

      if (!verifyRefInfo(treesRefInfoArrayTemp[i]->getMidRefNodesList(),_treesRefInfoArray[i]->getMidRefNodesList()))
    	 {
    	 printOnVerifyError(_treesRefInfoArray[i],treesRefInfoArrayTemp[i]);
    	 TR_ASSERT(0,"fail to verify midRefNodesList for %p\n",_treesRefInfoArray[i]->getTreeTop()->getNode());
    	 }

      if (!verifyRefInfo(treesRefInfoArrayTemp[i]->getLastRefNodesList(),_treesRefInfoArray[i]->getLastRefNodesList()))
    	 {
    	 printOnVerifyError(_treesRefInfoArray[i],treesRefInfoArrayTemp[i]);
    	 TR_ASSERT(0,"fail to verify lastRefNodesList for %p\n",_treesRefInfoArray[i]->getTreeTop()->getNode());
    	 }


       }
   } // scope of the stack memory region

#endif
   return true;
   }

//add pair into dependant list
void  TR_LocalLiveRangeReduction::addDepPair(TR_TreeRefInfo *dep,TR_TreeRefInfo *anchor)
   {
   DepPair *depPair = new (trStackMemory()) DepPair(dep,anchor);
   _depPairList.add(depPair);
   }

void TR_LocalLiveRangeReduction::updateDepList()
   {
   //at this point we are sure the list is not empty, cause we know some trees as moved.

   //first check the head, remove from head as needed
   ListElement<DepPair> * leHead=_depPairList.getListHead();
   while(leHead && !_movedTreesList.find(leHead->getData()->getAnchor()))
      {
      _depPairList.popHead();
      leHead=_depPairList.getListHead();
      }
   if (_depPairList.isEmpty())
      return;
   ListElement<DepPair> * le=leHead;
   ListElement<DepPair> * leNext=le->getNextElement();
   for (;leNext;)
      {
      if (!_movedTreesList.find(leNext->getData()->getAnchor()))
         _depPairList.removeNext(le);
      else
         le=leNext;

      leNext=le->getNextElement();
      }


   }

//--------------------------------     helper functions    ----------------------------------------------------------

//Test if the tree needs investigation.
//We are only concerned about trees which have children of type Last-Ref-Node.

bool TR_LocalLiveRangeReduction::isNeedToBeInvestigated(TR_TreeRefInfo *treeRefInfo)
   {

   //   TR::TreeTop *treeTop = treeRefInfo->getTreeTop();
   TR::Node *node = treeRefInfo->getTreeTop()->getNode();
   TR::ILOpCode &opCode = node->getOpCode();

   if (opCode.isBranch() || opCode.isReturn() || opCode.isGoto() || opCode.isJumpWithMultipleTargets() ||
       opCode.getOpCodeValue() == TR::BBStart || opCode.getOpCodeValue() == TR::BBEnd)
      return false;

   if (opCode.getOpCodeValue() == TR::treetop || opCode.isResolveOrNullCheck())
      node  = node->getFirstChild();

   //node might have changed
   /*if ((node->getOpCodeValue() == TR::monent) ||
       (node->getOpCodeValue() == TR::monexit)||
       (node->getOpCodeValue() == TR::athrow)) */
   if (nodeMaybeMonitor(node) ||(node->getOpCodeValue() == TR::athrow))
      return false;

  /********************************************************************/
  /*Need to add support for this :stop before loadReg to same register*/
   if (node->getOpCode().isStoreReg())
      return false;
  /*******************************************************************/
   if (_movedTreesList.find(treeRefInfo))
      return false;

   if (treeRefInfo->getFirstRefNodesList()->getSize()!=0)
      return true;

   return false;

   }


//returns true if there is first reference of a call or check
bool TR_LocalLiveRangeReduction::containsCallOrCheck(TR_TreeRefInfo *treeRefInfo, TR::Node *node)
   {
   if ((node->getOpCode().isCall() &&
        (node->getReferenceCount()==1 || treeRefInfo->getFirstRefNodesList()->find(node))) ||
       node->getOpCode().isCheck())
      {
      return true;
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (child->getReferenceCount()==1 || treeRefInfo->getFirstRefNodesList()->find(child))
         return containsCallOrCheck(treeRefInfo, child);
      }
   return false;
   }


int32_t TR_LocalLiveRangeReduction::getIndexInArray(TR_TreeRefInfo *treeRefInfo)
   {
   for ( int32_t i  = _numTreeTops-1; i>=0;  i--)
      {
      if( treeRefInfo == _treesRefInfoArray[i])
         return i;
      }
   return -1;
   }


//When moving trees down, this will tell us where to stop -
//If there are several nodes with "first reference" in the tree to move, stop before the first tree which contain a loast reference to one of those nodes.
bool TR_LocalLiveRangeReduction::matchFirstOrMidToLastRef(TR_TreeRefInfo *tree, TR_TreeRefInfo *treeToMove)
   {
   ListIterator<TR::Node> listIt(tree->getLastRefNodesList());
   for (TR::Node *node = listIt.getFirst(); node != NULL; node = listIt.getNext())
      {
      TR::Node *node1;
      ListIterator<TR::Node> listIt1(treeToMove->getMidRefNodesList());
      for (node1 = listIt1.getFirst(); node1 != NULL; node1 = listIt1.getNext())
         {
         if (node == node1)
            return true;
         }
      listIt1.set(treeToMove->getFirstRefNodesList());
      for (node1 = listIt1.getFirst(); node1 != NULL; node1 = listIt1.getNext())
         {
         if (node == node1)
            return true;
         }

      }
   return false;
   }

void TR_LocalLiveRangeReduction::updateRefInfo(TR::Node *n, TR_TreeRefInfo *currentTreeRefInfo, TR_TreeRefInfo *movingTreeRefInfo, bool underEval)
   {
    List<TR::Node> *C_firstList = currentTreeRefInfo->getFirstRefNodesList();
    List<TR::Node> *C_midList = currentTreeRefInfo->getMidRefNodesList();
    List<TR::Node> *C_lastList = currentTreeRefInfo->getLastRefNodesList();
    List<TR::Node> *M_firstList = movingTreeRefInfo->getFirstRefNodesList();
    List<TR::Node> *M_midList = movingTreeRefInfo->getMidRefNodesList();
    List<TR::Node> *M_lastList = movingTreeRefInfo->getLastRefNodesList();
    bool newEval = false;
    bool needToRecurse = false;
    if (n->getReferenceCount() <=1)
       needToRecurse = true;
    if (M_firstList->find(n))
        {
    	needToRecurse = true;
    	 if (underEval)
    	    {
    	    M_firstList->remove(n);
    	    C_firstList->add(n);
    	    newEval = true;
    	    if (M_midList->find(n) && C_lastList->find(n) )
    	       {
    	       M_midList->remove(n);
    	       M_lastList->add(n);
    	       C_lastList->remove(n);
    	       C_midList->add(n);
    	       }
    	    }
    	 else if (C_lastList->find(n) )

    	    {
    	    M_firstList->remove(n);
    	    M_lastList->add(n);
    	    C_lastList->remove(n);
    	    C_firstList->add(n);
    	    newEval = true;
    	    }
    	 else if (C_midList->find(n) )
    	    {
            M_firstList->remove(n);
            M_midList->add(n);
            C_midList->remove(n);
            C_firstList->add(n);
            newEval = true;
    	    }
    	 }

      else if (M_midList->find(n))
    	 {
    	 if (C_firstList->find(n) || C_midList->find(n) || C_lastList->find(n))
    	    {
    	    if (underEval)
    	       {
    	       M_midList->remove(n);
    	       C_midList->add(n);
    	       }
    	    if (C_lastList->find(n) && ((underEval && M_midList->find(n)) || !underEval))
    	       {
    	       M_midList->remove(n);
    	       M_lastList->add(n);
    	       C_lastList->remove(n);
    	       C_midList->add(n);
    	       }
    	    }
    	 else if (underEval)
    	    {
    	    if (M_firstList->find(n))
    	       {
    	       M_firstList->remove(n);
    	       C_firstList->add(n);
    	       newEval = true;
    	       }
    	    else
    	       {
    	       M_midList->remove(n);
    	       C_midList->add(n);
    	       }
    	    }
    	 }
      else if (M_lastList->find(n) && underEval)
    	 {
    	 if (C_midList->find(n) || C_lastList->find(n)) //caused by prev updates
    	    {
    	    M_lastList->remove(n);

    	    if (M_midList->find(n))
    	       {
    	       M_midList->remove(n);
    	       M_lastList->add(n);
    	       C_midList->add(n);
    	       }
    	    else
    	       C_lastList->add(n);

    	    }
    	 else
    	    {
    	    if (M_firstList->find(n))
    	       {
    	       M_firstList->remove(n);
    	       C_firstList->add(n);
    	       newEval = true;
    	       }
    	    else if (M_midList->find(n))
    	       {
    	       M_midList->remove(n);
    	       C_midList->add(n);
    	       }
    	    else
    	       {
    	       M_lastList->remove(n);
    	       C_lastList->add(n);
    	       }
    	    }
    	 }
      else
    	 newEval=underEval;
    if (needToRecurse)
       {
        for (int32_t i = 0; i < n->getNumChildren(); i++)
           {
           TR::Node *child = n->getChild(i);
           if (newEval)
        	  updateRefInfo(child, currentTreeRefInfo, movingTreeRefInfo, true);
           else
        	  updateRefInfo(child, currentTreeRefInfo, movingTreeRefInfo, false);
           }
       }
   }

void TR_LocalLiveRangeReduction::printRefInfo(TR_TreeRefInfo *treeRefInfo)
   {
   if (trace())
      {
      TR::Node *n;
      ListIterator<TR::Node> lit(treeRefInfo->getFirstRefNodesList());
      traceMsg(comp(),"[%p]:F={",treeRefInfo->getTreeTop()->getNode());
      for (n = lit.getFirst(); n != NULL; n = lit.getNext())
    	 traceMsg(comp(),"%p  ",n);

      traceMsg(comp(),"},M={");
      lit.set(treeRefInfo->getMidRefNodesList());
      for (n = lit.getFirst(); n != NULL; n = lit.getNext())
    	 traceMsg(comp(),"%p  ",n);

      traceMsg(comp(),"},L={");
      lit.set(treeRefInfo->getLastRefNodesList());
      for (n = lit.getFirst(); n != NULL; n = lit.getNext())
    	 traceMsg(comp(),"%p  ",n);

      traceMsg(comp(),"}\n");

      if (treeRefInfo->getUseSym() && treeRefInfo->getDefSym())
    	 {
    	 traceMsg(comp(),"[%p]:use = ",treeRefInfo->getTreeTop()->getNode());
    	 treeRefInfo->getUseSym()->print(comp());

    	 traceMsg(comp(),"  def = ");
    	 treeRefInfo->getDefSym()->print(comp());
    	 traceMsg(comp(),"\n");
    	 }
      }
   }
bool TR_LocalLiveRangeReduction::verifyRefInfo(List<TR::Node> *verifier,List<TR::Node> *refList)
   {

   ListIterator<TR::Node> listIt(refList);
   TR::Node *node = NULL;
   for ( node = listIt.getFirst(); node != NULL; node = listIt.getNext())
      {
      if (verifier->find(node))
    	 verifier->remove(node);
      else
    	 {
    	 if (trace())
    	    traceMsg(comp(),"LocalLiveRangeReduction:node %p should not have beed in the List\n",node);
    	 return false;
    	 }
      }

   if (!verifier->isEmpty())
      {
      if (trace())
    	 traceMsg(comp(),"LocalLiveRangeReduction: there are nodes that should have been in the List\n");
      return false;
      }
   return true;

   }

void TR_LocalLiveRangeReduction::printOnVerifyError(TR_TreeRefInfo *optRefInfo,TR_TreeRefInfo *verifier )
   {
   if (trace())
      {
      traceMsg(comp(),"from opt:");
      printRefInfo(optRefInfo);
      traceMsg(comp(),"verifyer:");
      printRefInfo(verifier);
      comp()->dumpMethodTrees("For verifying\n");
      comp()->incVisitCount();
      }
   }

const char *
TR_LocalLiveRangeReduction::optDetailString() const throw()
   {
   return "O^O LOCAL LIVE RANGE REDUCTION: ";
   }
