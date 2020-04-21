/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "optimizer/StripMiner.hpp"

#include <algorithm>
#include <stdint.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/NodeUtils.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/InductionVariable.hpp"
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Structure.hpp"

#define OPT_DETAILS "O^O STRIP MINER: "

TR_StripMiner::TR_StripMiner(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager),
     _nodesInCFG(0),
     _endTree(NULL),
     _loopInfos(manager->trMemory()),
     _origBlockMapper(NULL),
     _mainBlockMapper(NULL),
     _preBlockMapper(NULL),
     _postBlockMapper(NULL),
     _residualBlockMapper(NULL)
   {}

bool TR_StripMiner::shouldPerform()
   {
   //FIXME: only enabled when creating arraylets (for now)
   if (!comp()->generateArraylets())
      {
      if (trace())
         traceMsg(comp(), "Not enabled in non-rtj mode.\n");
      return false;
      }
   else if (comp()->getOption(TR_DisableStripMining))
      {
      if (trace())
         traceMsg(comp(), "Option is not enabled -- returning from strip mining.\n");
      return false;
      }

   if (!comp()->mayHaveLoops())
      {
      if (trace())
         traceMsg(comp(), "Method does not have loops -- returning from strip mining.\n");
      return false;
      }

   return true;
   }

int32_t TR_StripMiner::perform()
   {
   if (trace())
      traceMsg(comp(), "Processing method: %s\n", comp()->signature());

   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure();
   _nodesInCFG = _cfg->getNextNodeNumber();

   _endTree = comp()->getMethodSymbol()->getLastTreeTop();
   _loopInfos.init();

   // From here, down, stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
      {
      traceMsg(comp(), "Starting StripMining\n");
      comp()->dumpMethodTrees("Before strip mining");
      ///getDebug()->print(comp()->getOutFile(), _cfg);
      }

   // Collect and analyze information about loops
   collectLoops(_rootStructure);
   dumpOptDetails(comp(), "Loop analysis completed...\n");

   // Strip mine if there are loop candidates
   if (!_loopInfos.isEmpty())
      {
      _cfg->setStructure(NULL);

      /* Initialize block mappers */
      intptr_t size = _nodesInCFG * sizeof(TR::Block *);
      _origBlockMapper = (TR::Block **) trMemory()->allocateStackMemory(size);
      memset(_origBlockMapper, 0, size);
      _mainBlockMapper = (TR::Block **) trMemory()->allocateStackMemory(size);
      memset(_mainBlockMapper, 0, size);
      _preBlockMapper = (TR::Block **) trMemory()->allocateStackMemory(size);
      memset(_preBlockMapper, 0, size);
      _postBlockMapper = (TR::Block **) trMemory()->allocateStackMemory(size);
      memset(_postBlockMapper, 0, size);
      _residualBlockMapper = (TR::Block **) trMemory()->allocateStackMemory(size);
      memset(_residualBlockMapper, 0, size);
      _offsetBlockMapper = (TR::Block **) trMemory()->allocateStackMemory(size);
      memset(_offsetBlockMapper, 0, size);

      for (TR::CFGNode *node = _cfg->getFirstNode(); node; node = node->getNext())
         {
         TR::Block *block = toBlock(node);
         if (block && node->getNumber() >= 0)
            _origBlockMapper[node->getNumber()] = block;
         }

      //find leaves
      findLeavesInList();

      // strip mining
      transformLoops();
      }
   else
      dumpOptDetails(comp(), "Strip mining completed: no loops found\n");

   return 0;
   }

void TR_StripMiner::collectLoops(TR_Structure *str)
   {
   TR_RegionStructure *region = str->asRegion();

   if (region == NULL)
      return;

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getCurrent(); node; node = it.getNext())
      collectLoops(node->getStructure());

   if (region->isNaturalLoop())
      {
      if (trace())
         traceMsg(comp(), "<analyzeLoops loop=%d addr=%p>\n", region->getNumber(), region);
      // Check for loop pre-header
      //
      TR::Block *preHeader = getLoopPreHeader(str);
      if (preHeader == NULL)
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> no pre-header\n", region->getNumber());
         return;
         }

      // Check for loop test
      //
      TR::Block *loopTest = getLoopTest(str, preHeader);
      if (loopTest == NULL)
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> no loop test block\n", region->getNumber());
         return;
         }

      // Check if there is a primary induction variable
      //
      TR_PrimaryInductionVariable *piv = region->getPrimaryInductionVariable();
      if (piv == NULL)
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> no primary induction variable\n",
                    region->getNumber());
         return;
         }

      TR::Node *entryValue = piv->getEntryValue();

      // Check to see if there is only 1 back edge (the other edge is from pre-header
      //
      if (region->getEntryBlock()->getPredecessors().size() != 2)
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> more than 1 back edge\n",
                    region->getNumber());
         return;
         }

      //FIXME: no support for loops with catch blocks
      //
      TR_ScratchList<TR::Block> blocksInLoop(trMemory());
      region->getBlocks(&blocksInLoop);
      ListIterator<TR::Block> bIt(&blocksInLoop);
      for (TR::Block *b = bIt.getFirst(); b; b = bIt.getNext())
         {
         if (b->hasExceptionPredecessors()) // catch
            {
            if (trace())
               traceMsg(comp(), "\tReject loop %d ==> block_%d has exception predecessors\n",
                        region->getNumber(), b->getNumber());
            return;
            }
         if (b->hasExceptionSuccessors()) // try
            {
            if (trace())
               traceMsg(comp(), "\tReject loop %d ==> block_%d has exception successors\n",
                        region->getNumber(), b->getNumber());
            return;
            }
         }

      // Save loop info
      //
      LoopInfo *li = (LoopInfo *) trMemory()->allocateStackMemory(sizeof(LoopInfo));
      li->_region = region;
      li->_regionNum = region->getNumber();
      li->_arrayDataSize = 0;
      li->_increasing = piv->getDeltaOnBackEdge() > 0;
      TR::Node *branch = loopTest->getLastRealTreeTop()->getNode();
      li->_branchToExit =
         !region->contains(branch->getBranchDestination()->getNode()->getBlock()->getStructureOf(),
                           region->getParent());
      li->_canMoveAsyncCheck = true;
      li->_needOffsetLoop = false;
      li->_preOffset = 0;
      li->_postOffset = 0;
      li->_offset = -1;
      if (entryValue && entryValue->getOpCode().isLoadConst())
         {
         if (entryValue->getType().isInt32())
            li->_offset = entryValue->getInt();
         else
            li->_offset = entryValue->getLongInt();
         }
      // FIXME: really, we need a pre-loop to make i % strip_length = 0
      // add that functionality here
      /*else
         li->_offset = -1;
       */

      li->_stripLen = DEFAULT_STRIP_LENGTH;
      li->_preHeader = preHeader;
      li->_loopTest = loopTest;
      li->_piv = piv;
      li->_asyncTree = NULL;
      TR::Region &scratchRegion = trMemory()->currentStackRegion();
      li->_mainParentsOfLoads.setRegion(scratchRegion);
      li->_mainParentsOfLoads.init();
      li->_mainParentsOfStores.setRegion(scratchRegion);
      li->_mainParentsOfStores.init();
      li->_residualParentsOfLoads.setRegion(scratchRegion);
      li->_residualParentsOfLoads.init();
      li->_residualParentsOfStores.setRegion(scratchRegion);
      li->_residualParentsOfStores.init();
      li->_edgesToRemove.setRegion(scratchRegion);
      li->_edgesToRemove.init();

      examineLoop(li, mainLoop, false);

      //check if there are multiple stores of the induction variable.
      //Reject if the stores are incremental. i.e. i=i+1; i=i+1... In that case the deltaOnBackEdge is 2.
      // It is ok in case where if the stores are in different paths of the code. Then the value is equal to the deltaOnBackEdge
      //Looking only on Main loop

      if (checkIfIncrementalIncreasesOfPIV(li))
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> multiple store of induction variable were found\n", region->getNumber());
         return;

         }

      // Check if there are array accesses of more than one data size
      //
      if (comp()->generateArraylets() && li->_arrayDataSize <= 0)
         {
         if (trace())
            {
            if (li->_arrayDataSize < 0)
               traceMsg(comp(), "\tReject loop %d ==> array accesses of more than one data size\n",
                       region->getNumber());
            else
               traceMsg(comp(), "\tReject loop %d ==> no array accesses found\n",
                       region->getNumber());
            }
         return;
         }

      // Determine the loop strip length
      //
      if (comp()->generateArraylets())
         li->_stripLen = comp()->fe()->getArrayletMask(li->_arrayDataSize) + 1;

      // Don't strip mine loops that need offsetLoop
      //
      //If entry value is known, we need to be at the start of the leaf for increasing loops,
      // and for end of the leaf for decreasing loops so that no offset loop will be needed.
      //If no entry value is known (-1) then we need offset loop.
      if ( (li->_offset==-1 ) ||
           ((li->_increasing && ((li->_offset & (li->_stripLen-1)) != 0)) ||
            (!li->_increasing && (li->_offset & (li->_stripLen-1)) != li->_stripLen-1)))
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> needs a offsetLoop - cannot deal with this now\n", region->getNumber());
         //printf("reject %s: need offset loop %d\n",comp()->signature(),li->_offset);fflush(stdout);
         return;
         }

      // If the iterationCount of the loop is known and if the
      // trip count is less than the length of a leaf, its not
      // worth to mine this loop
      //
      int32_t iterCount = piv->getIterationCount();
      if (trace())
         traceMsg(comp(), "\titerationCount = %d stripLength = %d\n", iterCount, li->_stripLen);
      if (iterCount != -1)
         {
         if (iterCount < li->_stripLen)
            {
            if (trace())
               traceMsg(comp(), "\tReject loop %d ==> iteration count is less than the strip length\n",
                          region->getNumber());
            return;
            }
         }


      //Temp reject loops with post offsets

      if ((li->_preOffset!=0) ||
	(li->_postOffset !=0))
         {
         if (trace())
            traceMsg(comp(),
                    "\tReject loop %d ==> pre offset = %d, post offsets = %d\n",
                    region->getNumber(), li->_preOffset, li->_postOffset);
         //printf("reject %s: pre %d post %d offset\n",comp()->signature(),li->_preOffset, li->_postOffset);fflush(stdout);
         return;
         }


      // Check if pre and post offsets can be handled
      //
      if (li->_preOffset + li->_postOffset >= li->_stripLen)
         {
         if (trace())
            traceMsg(comp(),
                    "\tReject loop %d ==> pre offset = %d, post offsets = %d, strip length = %d\n",
                    region->getNumber(), li->_preOffset, li->_postOffset, li->_stripLen);
         return;
         }

      // Check if there is a call in the loop
      //
      if (!li->_canMoveAsyncCheck)
         {
         if (trace())
            traceMsg(comp(), "\tReject loop %d ==> calls present\n",
                    region->getNumber());
         return;
         }

      // Otherwise, add loop info to the queue
      //
      _loopInfos.add(li);
      if (trace())
         {
         traceMsg(comp(), "\tSuccess => adding candidate loop %d to the queue\n", region->getNumber());
         traceMsg(comp(), "\t\tpre-header = %d, loop test = %d, primary induction variable = %d\n",
               preHeader->getNumber(), loopTest->getNumber(),
               piv->getSymRef()->getReferenceNumber());
         traceMsg(comp(), "\t\tpre-offset = %d, post-offset = %d, offset = %d, strip length = %d\n",
                 li->_preOffset, li->_postOffset, li->_offset, li->_stripLen);
         traceMsg(comp(), "\t\tarray data size = %d, step = %d need-offset-loop = %d\n", li->_arrayDataSize,
                 piv->getDeltaOnBackEdge(), (li->_offset < 0));
         }
      }
   else
      {
      if (trace())
         traceMsg(comp(), "\tReject region %d ==> not a natural loop\n", region->getNumber());
      }
   }

/** actual strip mining starts here
 *
 * Here's whats going to happen:
 *
 * * assuming i is the piv and j is the new iv with N being the loop limit
 * * and loop contains array accesses A[i+x] A[i+y]

                         +--------+       +--------+
                         | overflo|       |  orig  |
                         | test   ------->| loop   |
                         +---+----+       +---+----+
                             |
                         +--------+
                         | prehdr |
                ;------->|        |
                ;        +---+----+
                ;            |      temp = i+x
                ;        +---+----+                pre-loop
                ;      ,>|        |
               ,      (  | i < N  |  -> exit
            ,-'        `-+---+----+
          ,'       i < temp  |
         ;               +---+----+
         ;              .| strip  |.        (i + stripLen - x - y) < N
        /             .' | test   |''`-.
       /    false  .-'   +---+----+    `. true
      ;      +---.'---+              +---`-.--+
      |i->j  | residue|              | main   |  change i -> j and iterate
      : upto | loop   |              | loop   |     upto (stripLen - x - y)
       \ N   +---`.---+              +---.'---+
        \          `.    +--------+   .'
         \           `-. | i=i+j  | .'
          \             `.  i<N   :' -> exit
           \             +---+----+
            `.               |      temp = i+y
              '-.        +---+----+                post-loop
                 `---.   |        |
                      ;  + i < N  |< -> exit
                      ;  +---+----. )
                      i < temp     '


*/

void TR_StripMiner::transformLoops()
   {
   ListIterator<LoopInfo> it(&_loopInfos);
   intptr_t size = _nodesInCFG * sizeof(TR::Block *);
   for (LoopInfo *li = it.getFirst(); li; li = it.getNext())
      {
      if (li != NULL &&
          performTransformation(comp(), "%sTransforming loop %d\n", OPT_DETAILS, li->_regionNum))
         {
         // Clear out the block mappers
         //
         memset(_mainBlockMapper, 0, size);
         memset(_preBlockMapper, 0, size);
         memset(_postBlockMapper, 0, size);
         memset(_residualBlockMapper, 0, size);
         memset(_offsetBlockMapper, 0, size);

         // Duplicate the loops first
         //
         duplicateLoop(li, offsetLoop);
         duplicateLoop(li, preLoop);
         duplicateLoop(li, mainLoop);
         duplicateLoop(li, residualLoop);
         duplicateLoop(li, postLoop);

         TR_RegionStructure *loop = li->_region;
         TR_ScratchList<TR::Block> blocksInLoop(trMemory());
         loop->getBlocks(&blocksInLoop);
         // Fix up everything
         //
         transformLoop(li);
         ListIterator<TR::Block> bIt(&blocksInLoop);
         TR::Block *block = NULL;
         for (block = bIt.getFirst(); block; block = bIt.getNext())
            block->setFrequency((int32_t) ((float) block->getFrequency()/10));

         if (trace())
            traceMsg(comp(), "Done transforming loop %d\n", li->_regionNum);
         //printf("\n--Strip Mining in -- %s start %d post %d pre %d increasing %d\n", comp()->signature(), li->_offset, li->_postOffset, li->_preOffset, li->_increasing);
         }
      }
   }

void TR_StripMiner::duplicateLoop(LoopInfo *li, TR_ClonedLoopType type)
   {
   TR::Block **blockMapper = NULL;
   bool isMainOrResidual = true;
   switch (type)
      {
      case offsetLoop:
         // check if offset already starts at an arraylet
         // offset could be -1 in which case the offset loop
         // will position the stripmined loop at the start of
         // an arraylet
         //
         if (!comp()->generateArraylets() ||
             ((li->_offset!=-1) &&
              ((li->_increasing && ((li->_offset & (li->_stripLen-1)) == 0)) ||
               (!li->_increasing && (li->_offset & (li->_stripLen-1)) == li->_stripLen-1))))

            return;

         blockMapper = _offsetBlockMapper;
         if (trace())
            traceMsg(comp(), "\tcreating start offset loop: loop %d...\n", li->_regionNum);
         li->_needOffsetLoop = true;
         isMainOrResidual = false;
         break;
      case preLoop:
         if (li->_preOffset <= 0)
            return;
         blockMapper = _preBlockMapper;
         if (trace())
            traceMsg(comp(), "\tcreating pre loop: loop %d...\n", li->_regionNum);
         isMainOrResidual = false;
         break;
      case mainLoop:
         blockMapper = _mainBlockMapper;
         if (trace())
            traceMsg(comp(), "\tcreating main loop: loop %d...\n", li->_regionNum);
         break;
      case postLoop:
         if (li->_postOffset <= 0)
            return;
         blockMapper = _postBlockMapper;
         if (trace())
            traceMsg(comp(), "\tcreating post loop: loop %d...\n", li->_regionNum);
         isMainOrResidual = false;
         break;
      case residualLoop:
         blockMapper = _residualBlockMapper;
         if (trace())
            traceMsg(comp(), "\tcreating residual loop: loop %d...\n", li->_regionNum);
         break;
      default:
         if (trace())
            traceMsg(comp(), "\tcannot recognize loop type: %d, abort duplication...\n", type);
         return;
      }

   // used for debugging the start position loop
   static char *pEnv = feGetEnv("TR_noStripMineStart");
   if (pEnv &&
         (type == offsetLoop && li->_needOffsetLoop))
      {
      traceMsg(comp(), "loop %d needs a start offset loop %d\n", li->_regionNum, li->_needOffsetLoop);
      li->_needOffsetLoop = false;
      return;
      }

   // Clone the loop
   //
   TR_RegionStructure *loop = li->_region;
   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   loop->getBlocks(&blocksInLoop);
   blocksInLoop.add(li->_preHeader);

   TR_BlockCloner cloner(_cfg, true);
   ListIterator<TR::Block> bIt(&blocksInLoop);
   TR::Block *block = NULL;
   for (block = bIt.getFirst(); block; block = bIt.getNext())
      {
      if (block->getNumber() < _nodesInCFG)
         {
         TR::Block *clonedBlock = cloner.cloneBlocks(block, block);

         // Append the clone to the end of method
         //
         _endTree->join(clonedBlock->getEntry());
         clonedBlock->getExit()->setNextTreeTop(NULL);
         _endTree = clonedBlock->getExit();

         // Record the orig -> clone mapping
         //
         blockMapper[block->getNumber()] = clonedBlock;

         /*if (trace())
            {
            traceMsg(comp(), "Cloned block (%d): %d\n",
                  block->getNumber(), blockMapper[block->getNumber()]->getNumber());
            }
          */
         }
      }

   //  Fix the clone's edges except the exit edges
   //
   for (block = bIt.getFirst(); block; block = bIt.getNext())
      {
      TR::Block *clonedBlock = blockMapper[block->getNumber()];
      if (trace())
         traceMsg(comp(), "\tprocessing edges for: block [%d] => cloned block [%d]\n",
                 block->getNumber(), clonedBlock->getNumber());

      if (block == li->_loopTest && !isMainOrResidual)
         {
         if (trace())
            traceMsg(comp(), "\tskipping edge from non-main non-residual loop test block [%d]\n",
                    block->getNumber());
         continue;
         }

      //skip exit edges. Those will be fixed later.
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         TR::Block *dest = toBlock((*edge)->getTo());
         TR::Block *clonedDest = blockMapper[dest->getNumber()];

         if (trace())
            traceMsg(comp(), "\t   for edge: [%d] => [%d]\n", clonedBlock->getNumber(),
                    clonedDest ? clonedDest->getNumber() : dest->getNumber());

         if (clonedDest)
            redirect(clonedBlock, dest, clonedDest);
         else
            if (trace())
               traceMsg(comp(), "\t   skipping exit edge to [%d]\n", dest->getNumber());
         }
      }

   if (trace())
      {
      comp()->dumpMethodTrees("stripMining: trees after loop duplication");
      ///comp()->getDebug()->print(comp()->getOutFile(), _cfg);
      }
   }

void TR_StripMiner::transformLoop(LoopInfo *li)
   {
   TR_RegionStructure *loop = li->_region;
   TR::Block *ph = li->_preHeader;
   TR::TreeTop *phEntry = ph->getEntry();
   TR::Node *phNode = phEntry->getNode();
   TR::Block *lt = li->_loopTest;
   TR::TreeTop *ltTree = lt->getLastRealTreeTop();
   intptr_t stripLength = li->_stripLen;

   TR::Block *clonedPh = _mainBlockMapper[ph->getNumber()];
   TR::TreeTop *clonedPhEntry = clonedPh->getEntry();
   TR::Node *clonedPhNode = clonedPhEntry->getNode();
   TR::Block *clonedLt = _mainBlockMapper[lt->getNumber()];
   TR::TreeTop *clonedLtTree = clonedLt->getLastRealTreeTop();

   bool isInt32 = ltTree->getNode()->getOpCode().isBooleanCompare() && // isCompare only returns true for signed compare -- a better name would be isCompareArith()
      ltTree->getNode()->getSecondChild()->getDataType() == TR::Int32;

   // Check to see if overflow test is needed
   bool testOverflow = true;
   TR::Node *limitNode = ltTree->getNode()->getSecondChild();

   if (limitNode->getOpCode().isLoadConst())
      {
      intptr_t limit = limitNode->getType().isInt32() ?
                        limitNode->getInt() : limitNode->getLongInt();
      if ((li->_increasing && limit < limit + li->_stripLen) ||
          (!li->_increasing && limit > limit - li->_stripLen))
          {
          if (trace())
             traceMsg(comp(), "No overflow test for %s loop %d: limit = %ld, stripLength = %ld\n",
                     li->_increasing ? "increasing" : "decreasing",
                     loop->getNumber(), limit, li->_stripLen);
          testOverflow = false;
          }
      }

   // If overflow test is needed, create an if block to test:
   // N > TR::getMaxSigned<TR::Int32>() - stripLength or N < TR::getMinSigned<TR::Int32>() + stripLength
   TR::Block *ifOverBlock = NULL;
   if (testOverflow)
      {
      ifOverBlock = TR::Block::createEmptyBlock(phNode, comp(), ph->getFrequency(), ph);
      TR::ILOpCodes opCode;
      TR::Node *overflowNode;
      int64_t constVal;
      if (li->_increasing)
         {
         opCode = isInt32 ? TR::ificmpgt : TR::iflcmpgt;
         constVal = (TR::getMaxSigned<TR::Int32>() - stripLength);
         }
      else
         {
         opCode = isInt32 ? TR::ificmplt : TR::iflcmplt;
         constVal = (TR::getMinSigned<TR::Int32>() + stripLength);
         }
      overflowNode = isInt32 ? TR::Node::iconst(phNode, (int32_t)constVal) :
                               TR::Node::lconst(phNode, constVal);

      TR::Node *ifOverNode = TR::Node::createif(opCode, limitNode->duplicateTree(), overflowNode, phEntry);
      TR::TreeTop *ifOverTree = TR::TreeTop::create(comp(), ifOverNode);
      ifOverBlock->append(ifOverTree);
      _cfg->addNode(ifOverBlock);
      _endTree->join(ifOverBlock->getEntry());
      ifOverBlock->getExit()->setNextTreeTop(NULL);
      _endTree = ifOverBlock->getExit();
      if (trace())
         traceMsg(comp(), "\tcreating overflow test [%p] in block [%d]\n", ifOverNode, ifOverBlock->getNumber());

      // Point the original loop pre-header's predecessor to the if over block
      if (trace())
         traceMsg(comp(), "\tfixing predecessor edges of original loop pre-header [%d] :\n",
               ph->getNumber());

      for (auto edge = ph->getPredecessors().begin(); edge != ph->getPredecessors().end(); ++edge)
         {
         TR::Block *fromBlock = toBlock((*edge)->getFrom());

         if (trace())
            traceMsg(comp(), "\t   for edge: [%d] => [%d]\n", fromBlock->getNumber(), ph->getNumber());

         redirect(fromBlock, ph, ifOverBlock);
         li->_edgesToRemove.add(*edge);
         }

      // Add an edge: if over -> pre-header
      //
      if (trace())
         traceMsg(comp(), "\t   adding edge: overflow test block [%d] => original pre-header [%d]\n",
               ifOverBlock->getNumber(), ph->getNumber());
      _cfg->addEdge(ifOverBlock, ph);
      }

   // Create an empty block as an outer loop header
   //
   TR::Block *outerHeader = TR::Block::createEmptyBlock(clonedPhNode, comp(),
                                                      clonedPh->getFrequency(), clonedPh);
   _cfg->addNode(outerHeader);
   _endTree->join(outerHeader->getEntry());
   outerHeader->getExit()->setNextTreeTop(NULL);
   _endTree = outerHeader->getExit();
   outerHeader->setFrequency((int32_t) ((float) outerHeader->getFrequency()/10));

   // If we need an offset loop to get to the beginning of a leaf
   if (li->_needOffsetLoop)
      {
      TR::Block *offsetLoopTest = createStartOffsetLoop(li, outerHeader);
      TR::Block *offsetLoopPh = _offsetBlockMapper[ph->getNumber()];

      if (testOverflow)
         {
         // Add a goto block to transfer control to offsetLoop
         TR::Block *gotoBlock = createGotoBlock(ifOverBlock, offsetLoopPh);
         gotoBlock->setFrequency((int32_t) ((float) gotoBlock->getFrequency()/10));
         if (trace())
            {
            traceMsg(comp(),
                  "\t   adding edges: overflow test block [%d] => goto [%d]; " \
                  "goto [%d] => start position loop [%d]\n",
                  ifOverBlock->getNumber(), gotoBlock->getNumber(),
                  gotoBlock->getNumber(), offsetLoopPh->getNumber());
            }
         }
      else
         {
         // Point the original loop pre-header's predecessor to offsetLoop
         if (trace())
            traceMsg(comp(), "\tfixing edges: preds of pre-header [%d] -> offset loop [%d]\n",
                  ph->getNumber(), offsetLoopPh->getNumber());

         for (auto edge = ph->getPredecessors().begin(); edge != ph->getPredecessors().end(); ++edge)
            {
            TR::Block *fromBlock = toBlock((*edge)->getFrom());

            if (trace())
               traceMsg(comp(), "\t   for edge: [%d] => [%d]\n", fromBlock->getNumber(), ph->getNumber());

            redirect(fromBlock, ph, offsetLoopPh);
            li->_edgesToRemove.add(*edge);
            }
         }
      }
   else
      {
      if (testOverflow)
         {
         // Add an edge: if over -> outer header
         //
         if (trace())
            traceMsg(comp(), "\t   adding edge: overflow test block [%d] => outer pre-header [%d]\n",
                  ifOverBlock->getNumber(), outerHeader->getNumber());
         _cfg->addEdge(ifOverBlock, outerHeader);
         }
      else
         {
         // Point the original loop pre-header's predecessor to outer header
         if (trace())
            traceMsg(comp(), "\tfixing edges: preds of pre-header [%d] -> outer header [%d]\n",
                  ph->getNumber(), outerHeader->getNumber());

         for (auto edge = ph->getPredecessors().begin(); edge != ph->getPredecessors().end(); ++edge)
            {
            TR::Block *fromBlock = toBlock((*edge)->getFrom());

            if (trace())
               traceMsg(comp(), "\t   for edge: [%d] => [%d]\n", fromBlock->getNumber(), ph->getNumber());

            redirect(fromBlock, ph, outerHeader);
            li->_edgesToRemove.add(*edge);
            }
         }
      }

   ////////////////////////////////////////
   // Create a main strip mined loop
   ///////////////////////////////////////

   if (trace())
      traceMsg(comp(), "<stripMining loop=%d outer header=%d>\n", li->_regionNum,
              outerHeader->getNumber());

   TR::Block *branchBlock = stripMineLoop(li, outerHeader);


   //////////////////////////////////////////
   ////add pre/post loops if needed  and fix up
      ////////////////////////////////////
   // If there is a pre-offset, create a goto block to the preloop pre-header
   //
   if (li->_preOffset > 0)
      {
      TR::Block *pre = _preBlockMapper[ph->getNumber()];
      TR::Block *preLt = _preBlockMapper[lt->getNumber()];

      TR::Block *gotoBlock = createGotoBlock(outerHeader, pre);
      gotoBlock->setFrequency((int32_t) ((float) gotoBlock->getFrequency()/10));
      if (trace())
         {
         traceMsg(comp(),
               "\t   adding edges: outer header [%d] => goto [%d]; " \
               "goto [%d] => preloop pre-header [%d]\n",
               outerHeader->getNumber(), gotoBlock->getNumber(),
               gotoBlock->getNumber(), pre->getNumber());
         }

      /* Create and insert an if block: i < i + pre-offset */
      TR::Block *ifBlock = createLoopTest(li, preLoop);

      /* Fix the edges */
      if (trace())
         traceMsg(comp(), "\t   fixing preloop loop test successor edges:\n");

      for (auto edge = lt->getSuccessors().begin(); edge != lt->getSuccessors().end(); ++edge)
         {
         TR::Block *fromBlock = _preBlockMapper[(*edge)->getFrom()->getNumber()];
         TR::Block *dest = toBlock((*edge)->getTo());
         TR::Block *clonedDest = _preBlockMapper[dest->getNumber()];

         if (clonedDest)
            {
            TR::Block *oldDest = li->_branchToExit ? clonedDest : dest;
            /* preloop's loop test -> if block */
            redirect(fromBlock, oldDest, ifBlock);
            /* if block -> cloned destination */
            redirect(ifBlock, oldDest, clonedDest);
            }
         else
            {
            /* preloop's loop test -> exit block */
            redirect(fromBlock, dest, dest);
            /* if block -> cloned pre-header */
            redirect(ifBlock, dest, branchBlock);
            }
         }
      }
   else
      {
      /* Create a goto block to the cloned loop's pre-header */
      TR::Block *gotoBlock = createGotoBlock(outerHeader, branchBlock);
      if (trace())
         traceMsg(comp(),
               "\t   adding edges: outer header [%d] => goto [%d]; " \
               "goto [%d] => branch [%d]\n",
               outerHeader->getNumber(), gotoBlock->getNumber(),
               gotoBlock->getNumber(), branchBlock->getNumber());
      }

   /* If there is a post-offset, redirect cloned loop's exit edges to the postloop pre-header */
   if (li->_postOffset > 0)
      {
      TR::Block *post = _postBlockMapper[ph->getNumber()];
      TR::Block *postLt = _postBlockMapper[lt->getNumber()];

      /* Create and insert an if block: i < i + post-offset */
      TR::Block *ifBlock = createLoopTest(li, postLoop);

      /* Fix the edges */
      if (trace())
         traceMsg(comp(), "\t   fixing postloop loop test successor edges:\n");

      for (auto edge = lt->getSuccessors().begin(); edge != lt->getSuccessors().end(); ++edge)
         {
         TR::Block *fromBlock = _postBlockMapper[(*edge)->getFrom()->getNumber()];
         TR::Block *dest = toBlock((*edge)->getTo());
         TR::Block *clonedDest = _postBlockMapper[dest->getNumber()];

         if (clonedDest)
            {
            TR::Block *oldDest = li->_branchToExit ? clonedDest : dest;
            /* postloop's loop test -> if block */
            redirect(fromBlock, oldDest, ifBlock);
            /* if block -> cloned destination */
            redirect(ifBlock, oldDest, clonedDest);
            }
         else
            {
            /* postloop's loop test -> exit block */
            redirect(fromBlock, dest, dest);
            /* if block -> outer header */
            redirect(ifBlock, dest, outerHeader);
            }
         }
      }

   /* Remove all unwanted edges */
   ListIterator<TR::CFGEdge> eIt(&li->_edgesToRemove);
   TR::CFGEdge *edge;
   for (edge = eIt.getFirst(); edge; edge = eIt.getNext())
      {
      _cfg->removeEdge(edge);
      if (trace())
         traceMsg(comp(), "\t   removing edge: [%d] => [%d]\n", edge->getFrom()->getNumber(),
                 edge->getTo()->getNumber());
      }

   if (trace())
      {
      comp()->dumpMethodTrees("stripMining: trees after loop transformation");
      ///comp()->getDebug()->print(comp()->getOutFile(), _cfg);
      }
   }

TR::Block *TR_StripMiner::createStartOffsetLoop(LoopInfo *li, TR::Block *outerHeader)
   {
   // create a new block which contains the new loop test
   // (i & stripLen) > 0
   // this positions the start value of the iv at the beginning of an arraylet
   //
   TR::Block *ltBlock = _offsetBlockMapper[li->_loopTest->getNumber()];
   TR::Node *ltNode = ltBlock->getLastRealTreeTop()->getNode();

   TR::Block *newLtBlock = TR::Block::createEmptyBlock(ltNode, comp(), ltBlock->getFrequency(), ltBlock);
   TR::Node *newLtNode = ltNode->duplicateTree();
   newLtBlock->append(TR::TreeTop::create(comp(), newLtNode));
   _endTree->join(newLtBlock->getEntry());
   newLtBlock->getExit()->setNextTreeTop(NULL);
   _endTree = newLtBlock->getExit();
   _cfg->addNode(newLtBlock);

   bool isInt32 = li->_piv->getSymRef()->getSymbol()->getType().isInt32();

   // modify the looptest as
   // ifXcmple --> exit
   //    xand
   //       xload
   //       stripLen-1
   //    zeroNode
   //
   TR::Node *iNode = TR::Node::createLoad(newLtNode, li->_piv->getSymRef());
   TR::ILOpCodes iandOp = isInt32 ? TR::iand : TR::land;
   TR::Node *stripLenNode = isInt32 ? TR::Node::iconst(newLtNode, li->_stripLen-1) :
                                     TR::Node::lconst(newLtNode, (int64_t)(li->_stripLen-1));
   TR::Node *iAndNode = TR::Node::create(iandOp, 2, iNode, stripLenNode);

   TR::Node *zeroNode = isInt32 ? TR::Node::iconst(newLtNode, 0) :
                                 TR::Node::lconst(newLtNode, (int64_t)0);

   newLtNode->getFirstChild()->recursivelyDecReferenceCount();
   newLtNode->getSecondChild()->recursivelyDecReferenceCount();

   newLtNode->setAndIncChild(0, iAndNode);
   newLtNode->setAndIncChild(1, zeroNode);
   TR::Node::recreate(newLtNode, isInt32 ? TR::ificmple : TR::iflcmple);

   // now fixup the edges
   //
   TR::Block *oldDest = NULL;
   TR::Block *origExit = NULL;
   if (li->_branchToExit)
      {
      // backedge is the fall-through
      //
      oldDest = _offsetBlockMapper[li->_loopTest->getNextBlock()->getNumber()];
      origExit = ltNode->getBranchDestination()->getNode()->getBlock();
      }
   else
      {
      // backedge is the dest
      //
      oldDest = ltNode->getBranchDestination()->getNode()->getBlock();
      origExit = li->_loopTest->getNextBlock();
      }

   // add an edge from the loop-test block to the exit
   //
   if (trace())
      traceMsg(comp(), "\t   adding edge: test block [%d] => exit [%d]\n",
              ltBlock->getNumber(), origExit->getNumber());
   ///_cfg->addEdge(ltBlock, origExit);
   redirect(ltBlock, li->_branchToExit ? origExit : NULL, origExit);


   for (auto e = ltBlock->getSuccessors().begin(); e != ltBlock->getSuccessors().end(); ++e)
      if ((*e)->getTo()->getNumber() == oldDest->getNumber())
         {
         li->_edgesToRemove.add(*e);
         break;
         }
   redirect(ltBlock, oldDest, newLtBlock);
   // NULL here will guarantee a goto block
   //
   redirect(newLtBlock, NULL, oldDest);

   // now add an edge from the new block to the outerheader
   //
   newLtNode->setBranchDestination(outerHeader->getEntry());
   if (trace())
      traceMsg(comp(), "\t   adding edge: new test block [%d] => outer pre-header [%d]\n",
              newLtBlock->getNumber(), outerHeader->getNumber());
   _cfg->addEdge(newLtBlock, outerHeader);
   if (trace())
      traceMsg(comp(), "\t created a new block [%d] to position at arraylet with test [%p]\n", newLtBlock->getNumber(), newLtNode);
   return newLtBlock;
   }


TR::Block *TR_StripMiner::stripMineLoop(LoopInfo *li, TR::Block *outerHeader)
   {
   TR_RegionStructure *loop = li->_region;
   TR::Block *ph = li->_preHeader;
   TR::TreeTop *phEntry = ph->getEntry();
   TR::Node *phNode = phEntry->getNode();
   TR::Block *lt = li->_loopTest;
   TR::TreeTop *ltTree = lt->getLastRealTreeTop();
   intptr_t stripLength = li->_stripLen;

   TR::Block *mainPh = _mainBlockMapper[ph->getNumber()];
   TR::TreeTop *mainPhEntry = mainPh->getEntry();
   TR::Node *mainPhNode = mainPhEntry->getNode();
   TR::Block *mainLt = _mainBlockMapper[lt->getNumber()];
   TR::TreeTop *mainLtTree = mainLt->getLastRealTreeTop();

   TR::Block *residualPh = _residualBlockMapper[ph->getNumber()];
   TR::TreeTop *residualPhEntry = residualPh->getEntry();
   TR::Node *residualPhNode = residualPhEntry->getNode();
   TR::Block *residualLt = _residualBlockMapper[lt->getNumber()];
   TR::TreeTop *residualLtTree = residualLt->getLastRealTreeTop();
   bool isInt32 = li->_piv->getSymRef()->getSymbol()->getType().isInt32();


   //////////////////////////////////////
   //create striplength test
   //This checks if we can perform a whole leaf at once
   //If yes, go to main else go to residue
   /////////////////////////////////////////
   /* Create a branch block to test:
    *    (i + stripLength - pre - post) < N  for increasing loops
    *    (i - (stripLength - pre - post)) > N for decreasing loops
    */
   TR::Block *branchBlock = TR::Block::createEmptyBlock(phNode, comp(), ph->getFrequency(), ph);
   TR::Node *iNode = TR::Node::createLoad(phNode, li->_piv->getSymRef());
   int64_t constVal;
   if ((li->_preOffset > 0) && (li->_postOffset > 0))
      constVal = (stripLength - li->_preOffset - li->_postOffset);
   else
      constVal = stripLength;


   TR::Node *constNode = isInt32 ? TR::Node::iconst(phNode,   (int32_t)(constVal-1)) :
      TR::Node::lconst(phNode, constVal-1);

   TR::ILOpCodes updateOpCode = li->_increasing ? (isInt32 ? TR::iadd : TR::ladd) : (isInt32 ? TR::isub : TR::lsub);
   TR::Node *iAddNode = TR::Node::create(updateOpCode, 2, iNode, constNode);

   TR::Node *limitNode = NULL;
   limitNode = ltTree->getNode()->getSecondChild()->duplicateTree();

   bool isCmpLong = (limitNode->getDataType() == TR::Int64);

   // I think only signed types can legally reach here
   if (isCmpLong && isInt32)
      {
      iAddNode = TR::Node::create(TR::i2l, 1, iAddNode);
      }

   TR::ILOpCodes cmpOpCode;
   if (TR::ILOpCode::isNotEqualCmp(ltTree->getNode()->getOpCodeValue()) ||
         TR::ILOpCode::isEqualCmp(ltTree->getNode()->getOpCodeValue()))
      {
      // li->_branchToExit is false for ne
      // li->_branchToExit is true  for eq
      if (li->_increasing)
         cmpOpCode = isCmpLong ? TR::iflcmplt : TR::ificmplt;
      else
         cmpOpCode = isCmpLong ? TR::iflcmpgt : TR::ificmpgt;
      }
   else
      cmpOpCode = li->_branchToExit ?
            ltTree->getNode()->getOpCode().getOpCodeForReverseBranch() :
            ltTree->getNode()->getOpCodeValue();


   TR::Node *ifNode = TR::Node::createif(cmpOpCode, iAddNode, limitNode, mainPhEntry);
   TR::TreeTop *ifTree = TR::TreeTop::create(comp(), ifNode);
   branchBlock->append(ifTree);
   _cfg->addNode(branchBlock);
   _endTree->join(branchBlock->getEntry());
   branchBlock->getExit()->setNextTreeTop(NULL);
   _endTree = branchBlock->getExit();
   if (trace())
      traceMsg(comp(), "\tcreating striplength test [%p] in outer loop block [%d]\n", ifNode, branchBlock->getNumber());

   /* Add an edge: if -> main pre-header */
   if (trace())
      traceMsg(comp(), "\t   adding edge: branch [%d] => main pre-header [%d]\n",
              branchBlock->getNumber(), mainPh->getNumber());
   _cfg->addEdge(branchBlock, mainPh);

   /* Create a goto block to the residual pre-header */
   TR::Block *gotoBlock = createGotoBlock(branchBlock, residualPh);
   if (trace())
      {
      traceMsg(comp(),
            "\t   adding edges: branch [%d] => goto [%d]; " \
               "goto [%d] => residual pre-header [%d]\n",
            branchBlock->getNumber(), gotoBlock->getNumber(),
               gotoBlock->getNumber(), residualPh->getNumber());
      }

   ///////////////////////////////////////////////////

   /* Create a new induction variable for the strip mined loops */
   TR::SymbolReference *newCounter = comp()->getSymRefTab()->createTemporary(
                                          comp()->getMethodSymbol(), isInt32 ? TR::Int32 : TR::Int64);
   TR::Node *zeroNode = isInt32 ? TR::Node::iconst(mainPhNode, 0) : TR::Node::lconst(mainPhNode, 0);
   TR::Node *storeNode = TR::Node::createStore(newCounter, zeroNode);
   TR::TreeTop *storeTT = TR::TreeTop::create(comp(), storeNode);
   mainPh->prepend(storeTT);
   TR::Node *zeroNodeResid = isInt32 ? TR::Node::iconst(residualPhNode, 0) : TR::Node::lconst(residualPhNode, 0);
   TR::Node *storeNodeResid = TR::Node::createStore(newCounter, zeroNodeResid);
   TR::TreeTop *storeTTResid = TR::TreeTop::create(comp(), storeNodeResid);
   residualPh->prepend(storeTTResid);

   //////////////////////////////
   //creating the ifContinueBlock
   //////////////////////////////
   /* Clone the duplicated loop test before any changes are made */
   TR_BlockCloner cloner(_cfg, true, false);
   TR::Block *ifContinueBlock = cloner.cloneBlocks(mainLt, mainLt);

   /* Update the induciton variable value before the comparison */
   iNode = iNode->duplicateTree();
   TR::Node *jNode = TR::Node::createLoad(phNode, newCounter);
   iAddNode = TR::Node::create(updateOpCode, 2, iNode, jNode);
   TR::Node *iStoreNode = TR::Node::createStore(li->_piv->getSymRef(), iAddNode);
   TR::TreeTop *iStoreTree = TR::TreeTop::create(comp(), iStoreNode);


   // retain the loopTest and remove the rest of the trees from this block
   //
   TR::TreeTop *cmpTree = ifContinueBlock->getLastRealTreeTop();
   ifContinueBlock->getEntry()->join(ifContinueBlock->getExit());

   // change the loop test to load i
   //
   cmpTree->getNode()->getFirstChild()->recursivelyDecReferenceCount();
   TR::Node *newLoopLimit = cmpTree->getNode()->getSecondChild()->duplicateTree();

   // add i2l if necessary
   //
   TR::Node *newINode = iNode->duplicateTree();
   if (isInt32 && isCmpLong)
      newINode = TR::Node::create(TR::i2l, 1, newINode);
   else if (!isInt32 && !isCmpLong)
      newINode = TR::Node::create(TR::l2i, 1, newINode);

   cmpTree->getNode()->setAndIncChild(0, newINode);
   cmpTree->getNode()->getSecondChild()->recursivelyDecReferenceCount();
   cmpTree->getNode()->setAndIncChild(1, newLoopLimit);
   ifContinueBlock->append(cmpTree);

   // attach the store
   ifContinueBlock->prepend(iStoreTree);
   _endTree->join(ifContinueBlock->getEntry());
   ifContinueBlock->getExit()->setNextTreeTop(NULL);
   _endTree = ifContinueBlock->getExit();

   if (trace())
      traceMsg(comp(), "\t   created a new block [%d] to store i = i + j and test if i < N\n",
              ifContinueBlock->getNumber());

   ///////////////////////////////////////////////////
   // Fix the edges for the loop tests
   ////////////////////////////////////////////////
   //fix up edges to/from  ifContinue
   for (auto edge = lt->getSuccessors().begin(); edge != lt->getSuccessors().end(); ++edge)
      {
      TR::Block *fromMainBlock = _mainBlockMapper[(*edge)->getFrom()->getNumber()];
      TR::Block *fromResidualBlock = _residualBlockMapper[(*edge)->getFrom()->getNumber()];
      TR::Block *dest = toBlock((*edge)->getTo());
      TR::Block *clonedMainDest = _mainBlockMapper[dest->getNumber()];

      if (clonedMainDest)
         {
         /* if continue block -> next loop's pre-header */
         TR::Block *newDest = li->_postOffset > 0 ? _postBlockMapper[ph->getNumber()] : outerHeader;
         redirect(ifContinueBlock, clonedMainDest, newDest);
         }
      else
         {
         /* loop test -> if block */
         redirect(fromMainBlock, dest, ifContinueBlock);
         redirect(fromResidualBlock, dest, ifContinueBlock);
         /* if block -> exit block */
         redirect(ifContinueBlock, dest, dest);
         }
      }

   ////////////////////////////////////////
   //fixing loop tests in main and residue
   ////////////////////////////////////////

   //fixing main loop test

   /* For main loop, change loop test condition to j < stripLength - pre - post */
   //Since we have in our hand the stripLength-1-pre-post
   //test j <= stripLength-1 - pre - post
   if (li->_branchToExit)
         cmpOpCode = isInt32 ? TR::ificmpgt : TR::iflcmpgt;
   else
         cmpOpCode = isInt32 ? TR::ificmple : TR::iflcmple;

   //constNode is L-1-pre-post
   jNode = jNode->duplicateTree();
   constNode = constNode->duplicateTree();
   TR::Node::recreate(mainLtTree->getNode(), cmpOpCode);
   mainLtTree->getNode()->getChild(0)->recursivelyDecReferenceCount();
   mainLtTree->getNode()->setAndIncChild(0, jNode);
   mainLtTree->getNode()->getChild(1)->recursivelyDecReferenceCount();
   mainLtTree->getNode()->setAndIncChild(1, constNode);
   if (trace())
      traceMsg(comp(), "\t   changed main loop test [%p] to j < (strip length - pre - post)\n", mainLtTree->getNode());




   /* For both main and residual loops, replace old induction variable in the inner loop */
   traceMsg(comp(), "\t   replacing original induction variable symRef [%d]\n", li->_piv->getSymRef()->getReferenceNumber());
   examineLoop(li, mainLoop, true);
   examineLoop(li, residualLoop, true);

   TR::Node *newConst = isInt32 ? TR::Node::iconst(phNode, li->_piv->getDeltaOnBackEdge()) :
                                 TR::Node::lconst(phNode, (int64_t)li->_piv->getDeltaOnBackEdge());
   if (!li->_mainParentsOfLoads.isEmpty() || !li->_mainParentsOfStores.isEmpty())
      replaceLoopPivs(li, updateOpCode, newConst, newCounter, mainLoop);
   if (!li->_residualParentsOfLoads.isEmpty() || !li->_residualParentsOfStores.isEmpty())
      replaceLoopPivs(li, updateOpCode, newConst, newCounter, residualLoop);


   //fixing residue loop test
   /* For residual loop, change loop test condition j < N - i */
   iNode = iNode->duplicateTree();
   jNode = jNode->duplicateTree();
   limitNode = ltTree->getNode()->getSecondChild()->duplicateTree();
   if (isInt32 && isCmpLong)
      {
      iNode = TR::Node::create(TR::i2l, 1, iNode);
      jNode = TR::Node::create(TR::i2l, 1, jNode);
      }
   else if (!isInt32 && !isCmpLong)
      {
      iNode = TR::Node::create(TR::l2i, 1, iNode);
      jNode = TR::Node::create(TR::l2i, 1, jNode);
      }
   TR::ILOpCodes subOp = isCmpLong ? TR::lsub : TR::isub;

   if (li->_increasing)
      iAddNode = TR::Node::create(subOp, 2, limitNode, iNode);
   else
      iAddNode = TR::Node::create(subOp, 2, iNode, limitNode);

   residualLtTree->getNode()->getChild(0)->recursivelyDecReferenceCount();
   residualLtTree->getNode()->setAndIncChild(0, jNode);
   residualLtTree->getNode()->getChild(1)->recursivelyDecReferenceCount();
   residualLtTree->getNode()->setAndIncChild(1, iAddNode);

   if (trace())
      traceMsg(comp(), "\t   changed residual loop test [%p] to j < (N - i)\n", residualLtTree->getNode());



   //fixing exit edges other than loop test
   //For each block with exit edges, fix each exit edge.
   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> bIt(&blocksInLoop);
   TR::Block *block = NULL;
   ListIterator<TR::CFGEdge> eItExit(&loop->getExitEdges());
   TR::CFGEdge *exitEdge = NULL;
   TR::Block *dest = NULL;
   intptr_t freq;
   for (block = bIt.getFirst(); block; block = bIt.getNext())
      {
      if ((block != lt) && (block->getNumber() < _nodesInCFG))
         {
         for (exitEdge = eItExit.getFirst(); exitEdge; exitEdge = eItExit.getNext())
            {
            if (exitEdge->getFrom()->getNumber() == block->getNumber())
               {
               //found from block. Now look for dest block
               for (auto e = block->getSuccessors().begin(); e != block->getSuccessors().end(); ++e)
                  {
                  if ((*e)->getTo()->getNumber() == exitEdge->getTo()->getNumber())
                     {
                     dest = toBlock((*e)->getTo());
                     break;
                     }

                  }
               freq = (block->getFrequency() < dest->getFrequency()) ?
                    block->getFrequency() : dest->getFrequency();

               TR::Block *fromMainBlock = _mainBlockMapper[block->getNumber()];
               TR::Block *fromResidualBlock = _residualBlockMapper[block->getNumber()];
               TR::Block *updateBeforeExitBlock = TR::Block::createEmptyBlock(dest->getEntry()->getNode(), comp(), freq, block);
               _cfg->addNode(updateBeforeExitBlock);
               _endTree->join(updateBeforeExitBlock->getEntry());
               updateBeforeExitBlock->getExit()->setNextTreeTop(NULL);
               _endTree = updateBeforeExitBlock->getExit();


               /* Update the induciton variable value before the comparison */
               iNode = iNode->duplicateTree();
               TR::Node *jNodeForUpdate = TR::Node::createLoad(phNode, newCounter);
               iAddNode = TR::Node::create(updateOpCode, 2, iNode, jNodeForUpdate);
               TR::TreeTop *iStoreTree = TR::TreeTop::create(comp(), TR::Node::createStore(li->_piv->getSymRef(), iAddNode));
               updateBeforeExitBlock->prepend(iStoreTree);
               TR::Node *gotoNode = TR::Node::create(dest->getEntry()->getNode(), TR::Goto, 0, dest->getEntry());
               TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
               updateBeforeExitBlock->append(gotoTree);

               redirect(fromMainBlock, dest,updateBeforeExitBlock);
               redirect(fromResidualBlock, dest,updateBeforeExitBlock);
               _cfg->addEdge(updateBeforeExitBlock, dest);
               }
            }
         }
      }

   /////////////////////////////////////////////////////////////


   /* Move asynccheck to outer loop header */
   if (li->_canMoveAsyncCheck)
      {
      if (li->_asyncTree != NULL)
         {
         if (trace())
            traceMsg(comp(), "\t   moved asynccheck tree [%p] to block [%d]\n",
                    li->_asyncTree, outerHeader->getNumber());
         li->_asyncTree->getPrevTreeTop()->join(li->_asyncTree->getNextTreeTop());
         li->_asyncTree->join(outerHeader->getEntry()->getNextTreeTop());
         outerHeader->getEntry()->join(li->_asyncTree);
         }
      }

   return branchBlock;
   }

TR::Block *TR_StripMiner::createGotoBlock(TR::Block *source, TR::Block *dest)
   {
   TR::TreeTop *destEntry = dest->getEntry();
   intptr_t freq = (source->getFrequency() < dest->getFrequency()) ?
                    source->getFrequency() : dest->getFrequency();

   TR::Block *gotoBlock = TR::Block::createEmptyBlock(destEntry->getNode(), comp(), freq, source);
   TR::Node *gotoNode = TR::Node::create(destEntry->getNextTreeTop()->getNode(),
                                       TR::Goto, 0, destEntry);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
   gotoBlock->append(gotoTree);
   _cfg->addNode(gotoBlock);

   /* Put the goto block in the right place in the tree */
   TR::TreeTop *gotoEntry = gotoBlock->getEntry();
   TR::TreeTop *gotoExit = gotoBlock->getExit();
   if (source->getNextBlock())
      gotoExit->join(source->getNextBlock()->getEntry());
   else
      {
      gotoExit->setNextTreeTop(NULL);
      _endTree = gotoExit;
      }
   source->getExit()->join(gotoEntry);

   /* Add edges: source -> goto; goto -> dest */
   _cfg->addEdge(source, gotoBlock);
   _cfg->addEdge(gotoBlock, dest);

   return gotoBlock;
   }

void TR_StripMiner::redirect(TR::Block *source, TR::Block *oldDest, TR::Block *newDest)
   {
   TR::Node *branchNode = source->getExit()->getPrevRealTreeTop()->getNode();
   bool found = false;

   if (branchNode->getOpCode().isSwitch())
      {
      for (intptr_t i = branchNode->getCaseIndexUpperBound() - 1; i > 0; --i)
         {
         if (branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock() == oldDest)
            {
            if (trace())
               traceMsg(comp(), "\t      fixing switch statement: [%d] => [%d]\n", source->getNumber(),
                     newDest->getNumber());
            branchNode->getChild(i)->setBranchDestination(newDest->getEntry());
            found = true;
            }
         }
      }
   else if(branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren())
      {
      for (intptr_t i = 0 ; i< branchNode->getNumChildren() - 1; ++i)
         {
         if (branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock() == oldDest)
            {
            if (trace())
               traceMsg(comp(), "\t      fixing switch statement: [%d] => [%d]\n", source->getNumber(),
                     newDest->getNumber());
            branchNode->getChild(i)->setBranchDestination(newDest->getEntry());
            found = true;
            }
         }

      }
   else if ((branchNode->getOpCode().isBranch() || branchNode->getOpCode().isGoto() ) && (branchNode->getBranchDestination()->getNode()->getBlock() == oldDest))
     {
         if (trace())
            traceMsg(comp(), "\t      fixing branch/goto statement: [%d] => [%d]\n", source->getNumber(),
                  newDest->getNumber());
         branchNode->setBranchDestination(newDest->getEntry());
         found = true;
         }
   else if (branchNode->getOpCode().isBranch() && source->getNextBlock() == newDest)
     {

       if (trace())
          traceMsg(comp(), "\t      skipping edge: [%d] => [%d], already exist\n", source->getNumber(), newDest->getNumber());
       found = true;
     }

   if (found)
      {
      /* Add an edge if the destination has been found and changed */
      _cfg->addEdge(source, newDest);
      if (trace())
         traceMsg(comp(), "\t      adding edge: [%d] => [%d]\n", source->getNumber(),
               newDest->getNumber());
      }
   else
      {
      /* Create a goto block to the new destination */
      TR::Block *gotoBlock = createGotoBlock(source, newDest);
      if (trace())
         {
         traceMsg(comp(),
               "\t      adding edges: source [%d] => goto [%d]; goto [%d] => new dest [%d]\n",
               source->getNumber(), gotoBlock->getNumber(),
               gotoBlock->getNumber(), newDest->getNumber());
         }
      }
   }

TR::Block *TR_StripMiner::createLoopTest(LoopInfo *li, TR_ClonedLoopType type)
   {
   TR::Block *ph = li->_preHeader;
   TR::TreeTop *phEntry = ph->getEntry();
   TR::Node *phNode = phEntry->getNode();
   TR::Block *lt = li->_loopTest;
   TR::TreeTop *ltTree = lt->getLastRealTreeTop();
   TR::Block **blockMapper = type == preLoop ? _preBlockMapper : _postBlockMapper;
   TR::Block *clonedPh = blockMapper[ph->getNumber()];
   TR::TreeTop *clonedPhEntry = clonedPh->getEntry();
   TR::Node *clonedPhNode = clonedPhEntry->getNode();
   TR::Block *clonedLt = blockMapper[lt->getNumber()];
   intptr_t offset = type == preLoop ? li->_preOffset : li->_postOffset;

   bool isInt32 = li->_piv->getSymRef()->getSymbol()->getType().isInt32();

   /* Adding temp = i + offset in pre-header */
   if (trace())
      traceMsg(comp(), "\t Adding temp = i + %s-offset in block [%d]\n",
              type == preLoop ? "pre" : "post", clonedPh->getNumber());

   TR::SymbolReference *tempSymRef = comp()->getSymRefTab()->createTemporary(
                                       comp()->getMethodSymbol(), isInt32 ? TR::Int32 : TR::Int64);
   TR::Node *iNode = TR::Node::createLoad(phNode, li->_piv->getSymRef());
   TR::Node *offsetNode = isInt32 ? TR::Node::iconst(clonedPhNode, offset) :
                                   TR::Node::lconst(clonedPhNode, (int64_t)offset);
   TR::ILOpCodes opCode = li->_increasing ? (isInt32 ? TR::iadd : TR::ladd) : (isInt32 ? TR::isub : TR::lsub);
   TR::Node *storeNode = TR::Node::createStore(tempSymRef,
                                             TR::Node::create(opCode, 2, iNode, offsetNode));
   TR::TreeTop *storeTree = TR::TreeTop::create(comp(), storeNode);
   clonedPh->prepend(storeTree);

   /* Create a new block to test if i < temp */
   TR_BlockCloner cloner(_cfg, true, false);
   TR::Block *ifBlock = cloner.cloneBlocks(clonedLt, clonedLt);
   TR::TreeTop *ifTree = ifBlock->getLastRealTreeTop();
   TR::Node *newLimitNode = TR::Node::createLoad(clonedPhNode, tempSymRef);
   TR::Node *ivLoad = ifTree->getNode()->getChild(0)->duplicateTree();
   ifTree->getNode()->getChild(0)->recursivelyDecReferenceCount();
   ifTree->getNode()->getChild(1)->recursivelyDecReferenceCount();
   ifTree->getNode()->setAndIncChild(0, ivLoad);
   ifTree->getNode()->setAndIncChild(1, newLimitNode);
   ifBlock->getEntry()->join(ifBlock->getExit());
   ifBlock->append(ifTree);
   _endTree->join(ifBlock->getEntry());
   ifBlock->getExit()->setNextTreeTop(NULL);
   _endTree = ifBlock->getExit();

   if (trace())
      traceMsg(comp(), "\t created a new block [%d] to test if i < temp [%p]\n", ifBlock->getNumber(), ifTree->getNode());

   return ifBlock;
   }

// examine loop to find all references to the piv
// these will be replaced with the new iv of the strip mined loop
//
void TR_StripMiner::examineLoop(LoopInfo *li, TR_ClonedLoopType type, bool checkClone)
   {
   if (trace())
      traceMsg(comp(), "   analyze %s loop %d...\n", type == mainLoop ? "main" : "residual",
              li->_regionNum);

   TR_RegionStructure *loop = li->_region;
   TR::SymbolReference *oldCounter = li->_piv->getSymRef();
   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   loop->getBlocks(&blocksInLoop);

   ListIterator<TR::Block> bIt(&blocksInLoop);
   TR::Block *block = NULL;
   intptr_t visitCount = comp()->incVisitCount();
   for (block = bIt.getFirst(); block; block = bIt.getNext())
      {
      if (checkClone)
         block = type == mainLoop ?
                 _mainBlockMapper[block->getNumber()] :
                 _residualBlockMapper[block->getNumber()];

      TR::TreeTop *currentTree = block->getEntry();
      TR::TreeTop *exitTree = block->getExit();

      while (currentTree != exitTree)
         {
         /* This loop cannot be strip mined, return */
         if (comp()->generateArraylets() &&
               (!li->_canMoveAsyncCheck || li->_arrayDataSize < 0))
            return;

         /* Check the cloned node against asynccheck */
         TR::Node *node = currentTree->getNode();
         if (checkClone && li->_canMoveAsyncCheck && node->getOpCodeValue() == TR::asynccheck &&
             li->_asyncTree == NULL)
            {
            if (trace())
               traceMsg(comp(), "      found asynccheck [%p] in block [%d]\n",
                       currentTree, block->getNumber());
            li->_asyncTree = currentTree;
            }

         if (node->getNumChildren() > 0)
            examineNode(li, node, node, oldCounter, visitCount, type, checkClone, -1);
         currentTree = currentTree->getNextTreeTop();
         }
      }
   }

void TR_StripMiner::examineNode(LoopInfo *li, TR::Node *parent, TR::Node *node,
      TR::SymbolReference *oldSymRef, vcount_t visitCount, TR_ClonedLoopType type,
      bool checkClone, int32_t childNum)
   {

   // Check the cloned node's symbol reference
   //
   if (checkClone && node->getSymbolReference() == oldSymRef)
      {
      if (node->getOpCode().isLoad())
         {
         bool foundLoad = (parent->getChild(childNum) == node);
         if (foundLoad)
            {
            if (trace())
               traceMsg(comp(), "      adding node [%p] to load list parent: [%p], childNum: %d\n", node, parent, childNum);

            type == mainLoop ?
               li->_mainParentsOfLoads.add(new (trStackMemory()) TR_ParentOfChildNode(parent, childNum)) :
               li->_residualParentsOfLoads.add(new (trStackMemory()) TR_ParentOfChildNode(parent, childNum));
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "      adding node [%p] store list parent: [%p]\n", node, parent);

         type == mainLoop ?
            li->_mainParentsOfStores.add(new (trStackMemory()) TR_ParentOfChildNode(parent, -1)) :
            li->_residualParentsOfStores.add(new (trStackMemory()) TR_ParentOfChildNode(parent, -1));
         }
      }

   // If we have seen this node before, we are done
   // Otherwise, set visit count
   //
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   /* Check to see if there are array accesses of more than one data type size */
   if (!checkClone)
      {
      /* If the loop contains cases we cannot handle, return immediately */
      if (comp()->generateArraylets() &&
            (li->_arrayDataSize < 0 || li->_preOffset < 0 || li->_postOffset < 0))
         return;

      TR::Symbol *symbol = NULL;
      bool foundArrayAccess = false;
      if (node->getOpCode().hasSymbolReference())
         symbol = node->getSymbol();

      if (symbol &&
          (symbol->isArrayShadowSymbol() ))//|| symbol->isArrayletShadowSymbol()))
         {
         if (node->getFirstChild()->getOpCode().isArrayRef())
            node = node->getFirstChild()->getFirstChild();
         else
            node = node->getFirstChild();
         if ( node->getSymbol() && node->getSymbol()->isArrayletShadowSymbol())
            {
            /* Pattern match for array access offsets
               iaload <arraylet>                                iaload <arraylet>
                  aiadd                                            aladd
                     aload <array>                                    aload <array>
                     isub/iadd                                        lsub/ladd
                        imul/ishl                                        lmul/lshl
                           ishr/idiv                                        i2l <-- optional
                              iload/iadd/isub                                  ...
                              iconst
                           iconst
                        iconst
             */
            if (node->getOpCodeValue() == TR::aloadi)
               {
               if (node->getFirstChild()->getOpCode().isArrayRef())
                  {
                  TR::Node *secondChild = node->getFirstChild()->getSecondChild();
                  if (secondChild->getOpCode().isAdd() || secondChild->getOpCode().isSub())
                     {
                     if (secondChild->getFirstChild()->getOpCode().isMul() ||
                         secondChild->getFirstChild()->getOpCode().isLeftShift())
                        {
                        TR::Node *firstChild = secondChild->getFirstChild()->getFirstChild();
                        if (firstChild->getOpCodeValue() == TR::i2l)
                           firstChild = firstChild->getFirstChild();

                        if (firstChild->getOpCode().isRightShift() ||
                            firstChild->getOpCode().isDiv())
                           {
                           TR::Node *accessNode = firstChild->getFirstChild();
                           if ((accessNode->getOpCodeValue() == TR::iload) &&
                               (accessNode->getSymbolReference() == li->_piv->getSymRef()))
                              {
                              foundArrayAccess = true;
                              if (trace())
                                 traceMsg(comp(), "Node %p accesses array with no pre/post offset\n", node);
                              }
                           else
                              {
                              if (accessNode->getOpCodeValue() != TR::iload)
                                 {
                                 if ((accessNode->getOpCode().isAdd() ||
                                      accessNode->getOpCode().isSub()) &&
                                     accessNode->getFirstChild()->getSymbolReference() == oldSymRef &&
                                     accessNode->getSecondChild()->getOpCodeValue() == TR::iconst)
                                    {
                                    foundArrayAccess = true;
                                    intptr_t offset = accessNode->getOpCodeValue() == TR::iadd ?
                                       accessNode->getSecondChild()->getInt() :
                                       accessNode->getSecondChild()->getInt() * -1;
                                    if (li->_increasing)
                                       {
                                       if (offset < 0)
                                          li->_preOffset = std::max(li->_preOffset, -1 * offset);
                                       else if (offset > 0)
                                          li->_postOffset = std::max(li->_postOffset, offset);
                                       }
                                    else
                                       {
                                       if (offset > 0)
                                          li->_preOffset = std::max(li->_preOffset, offset);
                                       else
                                          li->_postOffset = std::max(li->_postOffset, -1 * offset);
                                       }
                                    if (trace())
                                       traceMsg(comp(), "Node %p has pre-offset: %d, post-offset: %d\n",
                                               node, li->_preOffset, li->_postOffset);
                                    }
                                 else
                                    {
                                    bool foundPiv = findPivInSimpleForm(accessNode,oldSymRef);
                                    if (foundPiv)
                                       {
                                       foundArrayAccess = true;
                                       li->_preOffset = 0;
                                       li->_postOffset = 0;
                                       if (trace())
                                          traceMsg(comp(), "Found an iload/iadd/isub tree for node %p\n",
                                                  node);
                                       }
                                    else
                                       {
                                       li->_preOffset = -1;
                                       li->_postOffset = -1;
                                       if (trace())
                                          traceMsg(comp(), "No iload/iadd/isub tree found for node %p\n",
                                                  node);
                                       }
                                    }
                                 }

                              else  //if iload
                                 {
                                 if (trace())
                                    traceMsg(comp(), "Found iload %p which is not piv ... skipping it\n",
                                            node);


                                 }
                              }
                           }
                        else
                           {
                           if (trace())
                              traceMsg(comp(), "Node %p does not have an ishr/idiv tree\n", node);
                           }
                        }
                     }
                  }
               }
            }
         if (foundArrayAccess)
            {
            TR::DataType type = symbol->getDataType();
            intptr_t dataSize = TR::Symbol::convertTypeToSize(type);
            if (comp()->useCompressedPointers() && (type == TR::Address))
               dataSize = TR::Compiler->om.sizeofReferenceField();
            if (li->_arrayDataSize != 0 && li->_arrayDataSize != dataSize)
               li->_arrayDataSize = -1;
            else
               li->_arrayDataSize = dataSize;
            }
         }
      }

   /* Walk its children */
   for (intptr_t i = 0; i < node->getNumChildren(); ++i)
      {
      /* This loop cannot be strip mined, return */
      if (comp()->generateArraylets() &&
            (!li->_canMoveAsyncCheck || li->_arrayDataSize < 0))
         return;

      TR::Node *child = node->getChild(i);
      examineNode(li, node, child, oldSymRef, visitCount, type, checkClone, i);
      }
   }

TR::Node * findIndexChild(TR::Node *node, TR::SymbolReference *newSymRef)
   {
   if (node->getOpCode().isLoad() &&
         (node->getSymbolReference() == newSymRef))
      return node;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *found = findIndexChild(node->getChild(i), newSymRef);
      if (found)
         return found;
      }

   return NULL;
   }

void TR_StripMiner::replaceLoopPivs(LoopInfo *li, TR::ILOpCodes newOpCode, TR::Node *newConst,
                                    TR::SymbolReference *newSymRef, TR_ClonedLoopType type)
   {
   TR::SymbolReference *origSymRef = li->_piv->getSymRef();
   ListIterator<TR_ParentOfChildNode> it;
   TR_ParentOfChildNode *parent = NULL;
   TR_ScratchList<TR_Pair<TR::Node, TR::Node> > loadMapper(trMemory());
   TR_ScratchList<TR_Pair<TR::Node, TR::Node> > storeMapper(trMemory());

   if (trace())
      traceMsg(comp(), "   replacing Pivs in %s loop %d...\n", type == mainLoop ? "main" : "residual",
              li->_regionNum);

   /* Process list of loads */
   type == mainLoop ?  it.set(&li->_mainParentsOfLoads) : it.set(&li->_residualParentsOfLoads);
   TR::Node *iNode = TR::Node::createLoad(newConst, origSymRef);

   for (parent = it.getFirst(); parent != NULL; parent = it.getNext())
      {
      TR::Node *parentNode = parent->getParent();
      intptr_t childNum = parent->getChildNumber();
      ListIterator<TR_Pair<TR::Node, TR::Node> > lit(&loadMapper);
      TR_Pair<TR::Node, TR::Node> *loadPair = NULL;
      TR::Node *loadNode = NULL;

#if 1

      bool skipNode = false;
      intptr_t value;
      if ((parentNode->getOpCode().isRightShift() || parentNode->getOpCode().isDiv()) &&
          parentNode->getSecondChild()->getOpCode().isLoadConst())
         {
         value = (parentNode->getSecondChild()->getType().isInt32())?
            parentNode->getSecondChild()->getInt(): parentNode->getSecondChild()->getLongInt();
         if (value == comp()->fe()->getArraySpineShift(li->_arrayDataSize))
            skipNode = true;
         }
      else if ( parentNode->getOpCode().isAnd() &&
                   parentNode->getSecondChild()->getOpCode().isLoadConst())
         {
         value = (parentNode->getSecondChild()->getType().isInt32())?
            parentNode->getSecondChild()->getInt(): parentNode->getSecondChild()->getLongInt();
         if (value == comp()->fe()->getArrayletMask(li->_arrayDataSize))
            skipNode = true;

         }

      if (skipNode)
         parentNode->getChild(childNum)->setSymbolReference(newSymRef);
      else

         {
#endif


         for (loadPair = lit.getFirst(); loadPair != NULL; loadPair = lit.getNext())
            {
            if (loadPair->getKey() == parentNode->getChild(childNum))
               {
               loadNode = loadPair->getValue();
               break;
               }
            }

         if (loadNode == NULL)
            {
            //loadNode = newLoad->duplicateTree();
            TR::Node *a = parentNode->getChild(childNum);
            a->setSymbolReference(newSymRef);
            TR::Node *b = iNode->duplicateTree();
            if (!li->_increasing)
               {
               TR::Node * t = a;
               a = b;
               b = t;
               }

            loadNode = TR::Node::create(newOpCode, 2, a, b);
            loadNode->setCannotOverflow(true);
            loadPair = new (comp()->trStackMemory()) TR_Pair<TR::Node, TR::Node> (parentNode->getChild(childNum), loadNode);
            loadMapper.add(loadPair);
            }
#if 1
         }

      //fix shr/and
      if ((parentNode->getOpCode().isRightShift() || parentNode->getOpCode().isDiv()) && skipNode)
         {
         if (trace())
            traceMsg(comp(), "      replacing load [%p] with new load [%p]: parent [%p], childNum %d\n",
                    parentNode->getChild(childNum), iNode, parentNode, childNum);
         parentNode->getChild(childNum)->decReferenceCount();
         parentNode->setAndIncChild(childNum, iNode->duplicateTree());
         }
      else if ( parentNode->getOpCode().isAnd() && skipNode)
         {
         if (trace())
            traceMsg(comp(), "    changing and node parent  [%p] with new load [%p]\n",
                    parentNode, parentNode->getChild(0));
         }
      else
         {
         if (trace())
            traceMsg(comp(), "      replacing load [%p] with new load [%p]: parent [%p], childNum %d\n",
                    parentNode->getChild(childNum), loadNode, parentNode, childNum);
         parentNode->getChild(childNum)->decReferenceCount();
         parentNode->setAndIncChild(childNum, loadNode);
         }

#endif
   }

/* Process list of stores */
   type == mainLoop ? it.set(&li->_mainParentsOfStores) : it.set(&li->_residualParentsOfStores);
   for (parent = it.getFirst(); parent != NULL; parent = it.getNext())
      {
      TR::Node *node = parent->getParent();

      ListIterator<TR_Pair<TR::Node, TR::Node> > sit(&storeMapper);
      TR_Pair<TR::Node, TR::Node> *storePair = NULL;
      TR::Node *storeNode = NULL;
      for (storePair = sit.getFirst(); storePair != NULL; storePair = sit.getNext())
         {
         if (storePair->getKey() == node)
            {
            storeNode = storePair->getValue();
            break;
            }
         }

      if (storeNode == NULL)
         {
         //storeNode = newStore->duplicateTree();
         TR::Node *index = findIndexChild(node->getFirstChild(), newSymRef);
         storeNode = TR::Node::create(newOpCode, 2, index, newConst->duplicateTree());
         storePair = new (comp()->trStackMemory()) TR_Pair<TR::Node, TR::Node> (node, storeNode);
         storeMapper.add(storePair);
         }

      if (trace())
         traceMsg(comp(), "      replacing store [%p] with new store [%p]: new symref [%d]\n",
                 node, storeNode, newSymRef->getReferenceNumber());

      node->setSymbolReference(newSymRef);
      node->getChild(0)->recursivelyDecReferenceCount();
      node->setAndIncChild(0, storeNode);
      }
   }

TR::Block *TR_StripMiner::getLoopPreHeader(TR_Structure *str)
   {
   TR_RegionStructure *region = str->asRegion();
   TR::Block *headerBlock = region->getEntryBlock();

   TR::Block *pBlock = NULL;
   for (auto e = headerBlock->getPredecessors().begin(); e != headerBlock->getPredecessors().end(); ++e)
      {
      TR::Block *from = toBlock((*e)->getFrom());
      if (from->getStructureOf()->isLoopInvariantBlock())
         {
         pBlock = from;
         break;
         }
      }

   return pBlock;
   }

TR::Block *TR_StripMiner::getLoopTest(TR_Structure *str, TR::Block *preHeader)
   {
   TR_RegionStructure *region = str->asRegion();
   TR::Block *headerBlock = region->getEntryBlock();
   TR::Block *loopTest = NULL;

   for (auto e = headerBlock->getPredecessors().begin(); e != headerBlock->getPredecessors().end(); ++e)
      {
      TR::Block *from = toBlock((*e)->getFrom());
      if (from != preHeader)
         {
         loopTest = from;
         break;
         }
      }

   //FIXME: this is conservative, we can do better by actually finding
   // the loop exit test by looking at the exit edges
   //
   if (loopTest &&
         !loopTest->getLastRealTreeTop()->getNode()->getOpCode().isBooleanCompare())
      {
      if (trace())
         traceMsg(comp(), "loop %d: no loop test found on backedge\n", region->getNumber());
      loopTest = NULL;
      }
   //FIXME: if the test is eq/ne, make sure the loop increment is 1/-1
   //
   if (loopTest)
      {
      if (TR::ILOpCode::isNotEqualCmp(loopTest->getLastRealTreeTop()->getNode()->getOpCodeValue()) ||
            TR::ILOpCode::isEqualCmp(loopTest->getLastRealTreeTop()->getNode()->getOpCodeValue()))
         {
         if (trace())
            traceMsg(comp(), "loop %d: found loop with eq/ne test condition\n", region->getNumber());
         loopTest = NULL;
         // check for increment here
         }
      }

   return loopTest;
   }


bool TR_StripMiner::checkIfIncrementalIncreasesOfPIV(LoopInfo *li)
   {
   if (trace())
      traceMsg(comp(), "   looking for stores in original loop %d...\n",  li->_regionNum);

   TR_RegionStructure *loop = li->_region;
   TR::SymbolReference *oldCounter = li->_piv->getSymRef();
   bool isInt32 = li->_piv->getSymRef()->getSymbol()->getType().isInt32();
   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   loop->getBlocks(&blocksInLoop);

   ListIterator<TR::Block> bIt(&blocksInLoop);
   TR::Block *block = NULL;
   intptr_t visitCount = comp()->incVisitCount();
   int32_t pivIncInStore;
   for (block = bIt.getFirst(); block; block = bIt.getNext())
      {
      TR::TreeTop *currentTree = block->getEntry();
      TR::TreeTop *exitTree = block->getExit();

      while (currentTree != exitTree)
         {
         TR::Node *node = currentTree->getNode();

         if (node->getOpCode().isStore() && node->getSymbolReference()==oldCounter)
            {
            if (((node->getFirstChild()->getOpCode().isSub()) || (node->getFirstChild()->getOpCode().isAdd())) &&
                node->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
               {

               if (isInt32)
                  pivIncInStore = node->getFirstChild()->getSecondChild()->getInt();
               else
                  pivIncInStore = node->getFirstChild()->getSecondChild()->getLongInt();

               if (node->getFirstChild()->getOpCode().isSub())
                  pivIncInStore= -pivIncInStore;

               if (pivIncInStore  != li->_piv->getDeltaOnBackEdge())
                  {
                  if (trace())
                     traceMsg(comp(), "\t loop %d ==> Found a store to induction variable with increment different than deltaObBackEdge\n", li->_region->getNumber());
                  return true;

                  }
               }
            }
         currentTree = currentTree->getNextTreeTop();
         }
      }
   return false;
   }


void TR_StripMiner::findLeavesInList()
   {
   TR_ScratchList<LoopInfo> loopInfos(comp()->trMemory());
   loopInfos.init();

   ListIterator<LoopInfo> itl(&_loopInfos);
   for (LoopInfo *lInfo = itl.getFirst(); lInfo; lInfo = itl.getNext())
      {
      loopInfos.add(lInfo);
      }

   _loopInfos.deleteAll();
   ListIterator<LoopInfo> it(&loopInfos);
   LoopInfo *li = it.getFirst();
   LoopInfo *currentLeaf = li;
   _loopInfos.add(currentLeaf);

   //start from second
   for (li=it.getNext(); li; li = it.getNext())
      {
      if (!(li->_region->contains(currentLeaf->_region, li->_region->getParent())))
         {
         currentLeaf = li;
         _loopInfos.add(currentLeaf);
         }
      }
   return;
   }

bool TR_StripMiner::findPivInSimpleForm(TR::Node *node, TR::SymbolReference *pivSym)
   {
   while (node->getOpCode().isAdd() || node->getOpCode().isSub())
      {
      if (node->getSecondChild()->getOpCode().isLoadConst())
         node = node->getFirstChild();
      else
         break;
      }

   if (node->getOpCode().hasSymbolReference() )
      {
      if (( node->getOpCode().getOpCodeValue() == TR::iload) &&
          (node->getSymbolReference() == pivSym))

         return true;
      else
         return false;
      }

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      if (findPivInSimpleForm(node->getChild(i),pivSym))
         return true;
      }

   return false;
   }

const char *
TR_StripMiner::optDetailString() const throw()
   {
   return "O^O STRIP MINER: ";
   }
