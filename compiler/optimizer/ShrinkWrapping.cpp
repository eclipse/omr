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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "optimizer/ShrinkWrapping.hpp"

#include <stdint.h>                                 // for int32_t
#include <stdio.h>                                  // for NULL, fflush, etc
#include <string.h>                                 // for memset
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator
#include "codegen/FrontEnd.hpp"                     // for feGetEnv
#include "codegen/GCStackMap.hpp"                   // for TR_GCStackMap
#include "codegen/Instruction.hpp"                  // for Instruction
#include "codegen/Linkage.hpp"                      // for Linkage
#include "codegen/Snippet.hpp"                      // for Snippet
#include "compile/Compilation.hpp"                  // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/hashtab.h"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                             // for Block, toBlock
#include "il/DataTypes.hpp"                         // for TR_YesNoMaybe, etc
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode, etc
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Cfg.hpp"                            // for CFG, etc
#include "infra/Link.hpp"                           // for TR_LinkHead
#include "infra/List.hpp"                           // for ListIterator, etc
#include "infra/CfgEdge.hpp"                        // for CFGEdge
#include "infra/CfgNode.hpp"                        // for CFGNode
#include "optimizer/Optimization.hpp"               // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase

#define OPT_DETAILS "O^O SHRINKWRAPPING: "

TR_ShrinkWrap::TR_ShrinkWrap(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {
   _cfg = NULL;
   _traceSW = comp()->getOption(TR_TraceShrinkWrapping);
   _linkageUsesPushes = false;
   }

int32_t TR_ShrinkWrap::perform()
   {
   // only x86, ppc and s390 supported at the moment
   //
   if (!comp()->cg()->getSupportsShrinkWrapping())
      {
      if (_traceSW)
         traceMsg(comp(), "Platform does not support shrinkWrapping of registers\n");
      return 0;
      }

   _cfg = comp()->getFlowGraph();
   // compute the RUSE vectors at each basic block
   //
   if (_traceSW)
      traceMsg(comp(), "Going to start shrink wrapping of registers\n");

   // no shrink wrapping on CFG with internal cycles, as the
   // analysis is going to be conservative anyway
   //
   if (_cfg->getStructure()->markStructuresWithImproperRegions())
      {
      traceMsg(comp(), "CFG contains internal cycles, no shrink wrapping\n");
      return 0;
      }

   // FIXME: cannot handle switches for now
   // switches are an impediment for edge splitting
   // and there is no support yet to split the necessary
   // edges correctly
   //
   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      if (tt->getNode()->getOpCode().isJumpWithMultipleTargets())
         {
         // org/apache/xerces/dom/DeferredDocumentImpl.getNodeObject(I)Lorg/apache/xerces/dom/DeferredNode;
         // in xml.validation
         //
         traceMsg(comp(), "method contains switches, no shrink wrapping\n");
         return 0;
         }
      }

   if (_preservedRegs.isEmpty())
      {
      traceMsg(comp(), "No preserved registers used in this method, no shrink wrapping\n");
      comp()->cg()->setPreservedRegsInPrologue(NULL);
      return 0;
      }

   if (_traceSW && getDebug())
      {
      traceMsg(comp(), "CFG before shrinkwrapping :\n");
      getDebug()->print(comp()->getOutFile(), _cfg->getStructure(), 6);
      }


   // build up the RUSE vectors by checking which
   // registers are used at each instruction
   //
   analyzeInstructions();

   if (_traceSW)
      {
      traceMsg(comp(), "RUSE vectors: \n");
      for (int32_t i = 0; i < _numberOfNodes; i++)
         {
         traceMsg(comp(), "RUSE for block_%d : ", i);
         _registerUsageInfo[i]->print(comp());
         traceMsg(comp(), "\n");
         }
      }

   // first data-flow pass to
   // compute register anticipatability
   //
   TR_RegisterAnticipatability registerAnticipatability(comp(),
                                                        optimizer(),
                                                        _cfg->getStructure(),
                                                        _registerUsageInfo,
                                                        trace());

   // second data-flow pass to
   // compute register availability
   //
   TR_RegisterAvailability registerAvailability(comp(),
                                                optimizer(),
                                                _cfg->getStructure(),
                                                _registerUsageInfo,
                                                trace());

   // now compute the save and restore info
   // and use the results in placements
   //
   computeSaveRestoreSets(registerAnticipatability, registerAvailability);

   // now place the saves and restores of the registers
   // at various points as decided by the analysis
   //
   doPlacement(registerAnticipatability, registerAvailability);
   return 0;
   }

void TR_ShrinkWrap::prePerformOnBlocks()
   {
   _numberOfNodes = _cfg->getNextNodeNumber();

   _registerUsageInfo = (TR_BitVector **) trMemory()->allocateStackMemory(_numberOfNodes * sizeof(TR_BitVector *));
   _swBlockInfo = new (trStackMemory()) SWBlockInfo[_numberOfNodes];
   _cfgBlocks = _cfg->createArrayOfBlocks();

   // init the save/restore sets
   //
   _saveInfo = (TR_BitVector **) trMemory()->allocateStackMemory(_numberOfNodes * sizeof(TR_BitVector *));
   _restoreInfo = (TR_BitVector **) trMemory()->allocateStackMemory(_numberOfNodes * sizeof(TR_BitVector *));

   int32_t numRegs = TR::RealRegister::NumRegisters;
   for (int32_t i = 0; i < _numberOfNodes; i++)
      {
      _registerUsageInfo[i] = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
      _saveInfo[i] = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
      _restoreInfo[i] = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
      _swBlockInfo[i]._block = _cfgBlocks[i];
      }

   // init the bitvector to hold info about which preserved regs
   // are used in this method
   //
   _preservedRegsInMethod = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
   _preservedRegsInMethod->empty();
   _preservedRegsInLinkage = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
   _preservedRegsInLinkage->empty();

   // init the preservedRegs array to hold all the info
   //
   _preservedRegs.setFirst(NULL);
   TR_BitVector *p = new (trHeapMemory()) TR_BitVector(numRegs, trMemory(), heapAlloc);
   p->empty();
   comp()->cg()->setPreservedRegsInPrologue(p);

   // some aux data structures
   _returnBlocks.setFirst(NULL);
   _swEdgeInfo.setFirst(NULL);

   // init the array of preserved regs to provide a
   // mapping between regs and stack offsets
   //
   _mapRegsToStack = (int32_t *) trMemory()->allocateStackMemory(numRegs*sizeof(int32_t));
   memset(_mapRegsToStack, -1, (numRegs*sizeof(int32_t)));
   _linkageUsesPushes = comp()->cg()->getLinkage()->mapPreservedRegistersToStackOffsets(_mapRegsToStack, _numPreservedRegs, _preservedRegsInLinkage);

   if (_traceSW)
      traceMsg(comp(), "Mapping between preserved registers and stack offsets: \n");
   for (int32_t i = 0; i < numRegs; i++)
      {
      if (_mapRegsToStack[i] != -1)
         {
         if (_traceSW)
            traceMsg(comp(), "_mapRegsToStack[%d] = %d\n", i, _mapRegsToStack[i]);
         PreservedRegisterInfo *p = new (trStackMemory()) PreservedRegisterInfo;
         p->_index = i;
         p->_saveBlocks = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
         p->_restoreBlocks = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
         _preservedRegs.add(p);
         _preservedRegsInMethod->set(i);
         }
      }

   if (_traceSW)
      {
      traceMsg(comp(), "Preserved registers used in this linkage: ");
      _preservedRegsInLinkage->print(comp());
      traceMsg(comp(), "\n");
      traceMsg(comp(), "Preserved registers used in this method: { ");
      for (PreservedRegisterInfo *cur = _preservedRegs.getFirst(); cur; cur = cur->getNext())
         traceMsg(comp(), "%s ", comp()->getDebug()->getRealRegisterName(cur->_index-1));
      traceMsg(comp(), "}\n");
      traceMsg(comp(), "Corresponding bv: ");
      _preservedRegsInMethod->print(comp());
      traceMsg(comp(), "\n");
      }
   }

TR::Instruction *TR_ShrinkWrap::findJumpInstructionsInBlock (int32_t blockNum, TR::list<TR::Instruction*> *jmpInstrs)
   {
   //Start at the end of the block, and find the last jump instruction that is not to an out-of-line label
   //As we do not handle multi-target jumps in Shrink Wrapping, this is a jump to label marking the successor of this block
   TR::Instruction *current = _swBlockInfo[blockNum]._endInstr;

   while (current && current != _swBlockInfo[blockNum]._startInstr && ( !comp()->cg()->isBranchInstruction(current) || current->getLabelSymbol()->isStartOfColdInstructionStream() ) )
      current = current->getPrev();

   TR_ASSERT(current != _swBlockInfo[blockNum]._startInstr, "Current should not have been able to make it all the way back to the start instruction without encountering a jump\n");

   CS2::HashTable<TR::Instruction *, bool, TR::Allocator> setOfJumps(comp()->allocator());
   findJumpInstructionsInCodeRegion(_swBlockInfo[blockNum]._startInstr, _swBlockInfo[blockNum]._endInstr, setOfJumps);

   //Populate jmpInstrs from setOfJumps
   CS2::HashTable<TR::Instruction *, bool, TR::Allocator>::Cursor h(setOfJumps);
   for (h.SetToFirst(); h.Valid(); h.SetToNext())
      jmpInstrs->push_front(setOfJumps.KeyAt(h));

   return current;
   }

void TR_ShrinkWrap::findJumpInstructionsInCodeRegion (TR::Instruction *firstInstr, TR::Instruction *lastInstr, CS2::HashTable<TR::Instruction *, bool, TR::Allocator> & setOfJumps)
   {
   TR::Instruction *loc = lastInstr;
   while (loc != firstInstr)
      {
      if (comp()->cg()->isBranchInstruction(loc) && setOfJumps.Add(loc, true))
         {
         //In general, a snippet or out-of-line code segment could also have a jump that might need patching.
         //The recursive call below identifies these cases, and adds any jumps in snippets to the list.
         TR::Instruction *start, *end;
         if (comp()->cg()->isTargetSnippetOrOutOfLine(loc, &start, &end))
            {
            if (_traceSW)
               traceMsg(comp(), "Recursing over snippet jumped to by instruction %p", loc);

            findJumpInstructionsInCodeRegion(start, end, setOfJumps);
            }
         }

      loc = loc->getPrev();
      }
   }

void TR_ShrinkWrap::analyzeInstructions()
   {
   TR::Instruction *instr = comp()->cg()->getFirstInstruction();

   int32_t blockNum = comp()->getStartTree()->getEnclosingBlock()->getNumber(); // entry block
   if (_traceSW)
      traceMsg(comp(), "First block of method in IL: %d\n", blockNum);

   // process any glRegDeps on the parameters
   //
   comp()->cg()->processIncomingParameterUsage(_registerUsageInfo, blockNum);

   for (int32_t blockNum = 0; blockNum < _numberOfNodes; blockNum++)
      {
      if ((blockNum == _cfg->getEnd()->getNumber()) ||
            (blockNum == _cfg->getStart()->getNumber()))
         continue;

      TR::Block *curBlock = _swBlockInfo[blockNum]._block;
      if (!curBlock) continue;
      TR::Instruction *cur = curBlock->getFirstInstruction();
      TR::Instruction *end = curBlock->getLastInstruction();
      _swBlockInfo[blockNum]._startInstr = cur;
      _swBlockInfo[blockNum]._endInstr = end;

      while (cur != end)
         {
         int32_t isFence = -1; // 1 is BBStart ; 2 is BBEnd
         bool success = comp()->cg()->processInstruction(cur, _registerUsageInfo, blockNum, isFence, _traceSW);
         // look for registers that may be used in outlined sequences
         //
         if (comp()->cg()->isBranchInstruction(cur))
            processInstructionsInSnippet(cur, blockNum);
         cur = cur->getNext();
         }
      }

   // if a preserved register is used inside a loop,
   // propagate uses of it across all the blocks so that the
   // analysis does not shrinkWrap inside loops
   //
   for (int32_t i = 0; i < _numberOfNodes; i++)
      {
      if (_registerUsageInfo[i]->isEmpty())
         continue;

      ///traceMsg(comp(), "processing block_%d\n", i);
      TR::Block *curBlock = _swBlockInfo[i]._block;
      TR_Structure *outerMost = curBlock->getStructureOf()->getContainingLoop();
      TR_Structure *region = outerMost;
      ///if (region)
      ///   traceMsg(comp(), "found loop %d\n", region->getNumber());

      for (; outerMost && outerMost->asRegion()->isNaturalLoop(); outerMost = outerMost->getParent())
         region = outerMost;
      if (region)
         {
         ///traceMsg(comp(), "contained in %d\n", region->getNumber());

         TR_ScratchList<TR::Block> blocksInRegion(trMemory());
         region->asRegion()->getBlocks(&blocksInRegion);
         TR_BitVectorIterator rUseIt(*_registerUsageInfo[i]);
         while (rUseIt.hasMoreElements())
            {
            int32_t regIndex = rUseIt.getNextElement();
            if (comp()->cg()->isPreservedRegister(regIndex) != -1)
               {
               ListIterator<TR::Block> bIt(&blocksInRegion);
               for (TR::Block *b = bIt.getFirst(); b; b = bIt.getNext())
                  {
                  if (b->getNumber() == i)
                     continue;
                  ///if (_traceSW)
                  ///   traceMsg(comp(), "setting regIndex %d on block_%d\n", regIndex, b->getNumber());
                  _registerUsageInfo[b->getNumber()]->set(regIndex);
                  }
               }
            }
         }

#if 0
      //FIXME: this piece of code makes things very conservative.
      // i.e. since register usage info from a catch block (which is
      // generally cold) is propagated to its exception preds (which may
      // be on the hot paths), this means that there is no real benefit to
      // shrinkwrapping the registers that would have been used only in the
      // paths dominated by the catch block
      //
      if (!curBlock->getExceptionPredecessors().empty() &&
            !(curBlock->getExceptionPredecessors().size() == 1))
         {
         // catchblock having more than one predecessor
         // iterate through the exception predecessors and propagate the use info
         // to them. this is because we cannot split exception edges, so we need to
         // make sure that the regs are saved
         //
         for (auto e = curBlock->getExceptionPredecessors().begin(); e != curBlock->getExceptionPredecessors().end(); ++e)
            {
            TR::Block *predBlock = toBlock((*e)->getFrom());
            *_registerUsageInfo[predBlock->getNumber()] |= *_registerUsageInfo[curBlock->getNumber()];
            if (_traceSW)
               traceMsg(comp(), "pushing regusage info from catch block_%d to %d\n", curBlock->getNumber(), predBlock->getNumber());
            }
         }
#endif
      }

   // handle switches here. we will just propagate the use vectors
   // from the successor blocks into the preds to make sure that
   // the save/restore(s) span this region
   //
   //FIXME: enable this code below
#if 0
   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      if (tt->getNode()->getOpCode().isSwitch())
         {
         TR::Block *b = tt->getEnclosingBlock();
         for (auto e = b->getSuccessors().begin(); e != b->getSuccessors().end(); ++e)
            {
            TR::Block *succBlock = toBlock((*e)->getTo());
            for (auto e2 = succBlock->getPredecessors().begin(); e2 != succBlock->getPredecessors().end(); ++e2)
               {
               *_registerUsageInfo[(*e2)->getFrom()->getNumber()] |= *_registerUsageInfo[succBlock->getNumber()];
               }
            }
         }
      }
#endif

   // check the catch blocks in the methods. propagate uses from the
   // predecessors into the catch blocks
   //
   for (TR::CFGNode *node = _cfg->getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = toBlock(node);
      int32_t blockNum = block->getNumber();

      if (!block->getExceptionPredecessors().empty())
         {
         for (auto e = block->getExceptionPredecessors().begin(); e != block->getExceptionPredecessors().end(); ++e)
            {
            TR::Block *pred = toBlock((*e)->getFrom());
            *_registerUsageInfo[blockNum] |= *_registerUsageInfo[pred->getNumber()];
            }
         }
      }
   }

void TR_ShrinkWrap::processInstructionsInSnippet(TR::Instruction *instr, int32_t blockNum)
   {
   TR::Instruction *cur = NULL;
   TR::Instruction *end = NULL;
   if (comp()->cg()->isTargetSnippetOrOutOfLine(instr, &cur, &end))
      {
      while (cur != end)
         {
         int32_t isFence = -1;
         bool success = comp()->cg()->processInstruction(cur, _registerUsageInfo, blockNum, isFence, _traceSW);
         cur = cur->getNext();
         }
      }
   }

void TR_ShrinkWrap::computeSaveRestoreSets(TR_RegisterAnticipatability &registerAnticipatability, TR_RegisterAvailability &registerAvailability)
   {
   // For each register r, the save equation at basic block i would be: (REGS is set to 1s)
   //
   // SAV(i)          = [ RANTin(i) - RAVLin(i) ] & intersection j E pred(i) [ REGS - RANTin(j) ]
   // RESTORE(i)      = [ RAVLout(i) - RANTout(i) ] & intersection j E succ(i) [ REGS - RAVLout(j) ]
   //
   int32_t numRegs = TR::RealRegister::NumRegisters;
   TR_BitVector *regs = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
   regs->setAll(numRegs);

   // handle loops here
   // propagate the anticipatability from the loop entry block
   // to the predecessors (ignoring backedges), so the saves are
   // computed correctly. since we're just iterating over blocks and
   // not in CFG order, we need to first propagate before iterating
   // again to determine the save/restore solution
   // FIXME: this is now handled in doPlacement
   /*
   for (TR::CFGNode *node = _cfg->getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = toBlock(node);
      int32_t blockNum = block->getNumber();

      TR_Structure *loop = block->getStructureOf()->getContainingLoop();
      if (loop &&
            (loop->getEntryBlock() == block))
         {
         traceMsg(comp(), "   block_%d is loop entry\n", blockNum);
         TR_BitVector *entryAnticipatability = registerAnticipatability._blockAnalysisInfo[block->getNumber()];
         TR_PredecessorIterator edgeIt(block);
         for (TR::CFGEdge *e = edgeIt.getFirst(); e; e = edgeIt.getNext())
            {
            TR::Block *predBlock = toBlock(e->getFrom());
            if (!loop->contains(predBlock->getStructureOf(), loop->getParent()))
               // not a backedge, so propagate the anticipatability
               //
               *(registerAnticipatability._blockAnalysisInfo[predBlock->getNumber()]) |= *entryAnticipatability;
            }
         }
      }
   */

   // compute saveInfo now
   // iterate over all the preds of blocks and compute the resulting bitvectors
   // using the solutions for anticipatability and availability computed above
   //
   TR_BitVector *scratch = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
   TR_BitVector *temp = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
   for (TR::CFGNode *node = _cfg->getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = toBlock(node);
      int32_t blockNum = block->getNumber();

      // initialize the bitvectors
      //
      temp->empty();
      _saveInfo[blockNum]->empty();
      scratch->empty();
      *scratch |= *regs;
      *temp |= *regs;

      if (_traceSW)
         traceMsg(comp(), "processing block_%d\n", blockNum);

      /*
      traceMsg(comp(), "orig scratch is: \n");
      scratch->print(comp());
      traceMsg(comp(), "\n");
      */

      ///ListIterator<TR::CFGEdge> edgeIt(&block->getPredecessors());
      TR_PredecessorIterator edgeIt(block);
      bool hasPreds = false;
      for (TR::CFGEdge *e = edgeIt.getFirst(); e; e = edgeIt.getNext())
         {
         TR::Block *from = toBlock(e->getFrom());
         ///*scratch -= *(registerAnticipatability._blockAnalysisInfo[from->getNumber()]);
         *scratch &= *(registerAnticipatability._blockAnalysisInfo[from->getNumber()]);
         ///*scratch |= *(registerAnticipatability._blockAnalysisInfo[from->getNumber()]);
         hasPreds = true;
         }

      if (_traceSW)
         {
         traceMsg(comp(), "after preds scratch is: \n");
         scratch->print(comp());
         traceMsg(comp(), "\n");
         }

      if (hasPreds)
         {
         *temp -= *scratch;
         if (_traceSW)
            {
            traceMsg(comp(), "after negation preds scratch is: \n");
            temp->print(comp());
            traceMsg(comp(), "\n");
            }
         scratch->empty();
         *scratch |= *temp;
         }
      temp->empty();


      if (_traceSW)
         {
         traceMsg(comp(), "regant in: \n");
         (registerAnticipatability._blockAnalysisInfo[blockNum])->print(comp());
         traceMsg(comp(), "\n");

         traceMsg(comp(), "regavl in: \n");
         (registerAvailability._inSetInfo[blockNum])->print(comp());
         traceMsg(comp(), "\n");
         }

      *temp |= *(registerAnticipatability._blockAnalysisInfo[blockNum]);

      if (_traceSW)
         {
         traceMsg(comp(), "temp1: \n");
         temp->print(comp());
         traceMsg(comp(), "\n");
         }

      *temp -= *(registerAvailability._inSetInfo[blockNum]);

      if (_traceSW)
         {
         traceMsg(comp(), "temp2: \n");
         temp->print(comp());
         traceMsg(comp(), "\n");
         }

      *temp &= *scratch;

      if (_traceSW)
         {
         traceMsg(comp(), "temp3: \n");
         temp->print(comp());
         traceMsg(comp(), "\n");
         }

      *_saveInfo[blockNum] |= *temp;

      if (_traceSW)
         {
         traceMsg(comp(), "sav: \n");
         _saveInfo[blockNum]->print(comp());
         traceMsg(comp(), "\n");
         }
      }

   // now compute the RESTORE sets
   //
   for (TR::CFGNode *node = _cfg->getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = toBlock(node);
      int32_t blockNum = block->getNumber();

      // initialize the bitvectors
      //
      temp->empty();
      _restoreInfo[blockNum]->empty();
      scratch->empty();
      *scratch |= *regs;
      *temp |= *regs;

      if (_traceSW)
         traceMsg(comp(), "processing block_%d\n", blockNum);

      /*
      traceMsg(comp(), "orig scratch is: \n");
      scratch->print(comp());
      traceMsg(comp(), "\n");
      */

      ///ListIterator<TR::CFGEdge> edgeIt(&block->getSuccessors());
      TR_SuccessorIterator edgeIt(block);
      bool hasSuccs = false;
      for (TR::CFGEdge *e = edgeIt.getFirst(); e; e = edgeIt.getNext())
         {
         TR::Block *to = toBlock(e->getTo());
         *scratch &= *(registerAvailability._blockAnalysisInfo[to->getNumber()]);
         ///*scratch |= *(registerAvailability._blockAnalysisInfo[to->getNumber()]);
         hasSuccs = true;
         }

      if (_traceSW)
         {
         traceMsg(comp(), "after succs scratch is: \n");
         scratch->print(comp());
         traceMsg(comp(), "\n");
         }

      if (hasSuccs)
         {
         *temp -= *scratch;
         if (_traceSW)
            {
            traceMsg(comp(), "after negation succs scratch is: \n");
            temp->print(comp());
            traceMsg(comp(), "\n");
            }
         scratch->empty();
         *scratch |= *temp;
         }
      temp->empty();

      if (_traceSW)
         {
         traceMsg(comp(), "regant out: \n");
         (registerAnticipatability._outSetInfo[blockNum])->print(comp());
         traceMsg(comp(), "\n");

         traceMsg(comp(), "regavl out: \n");
         (registerAvailability._blockAnalysisInfo[blockNum])->print(comp());
         traceMsg(comp(), "\n");
         }

      *temp |= *(registerAvailability._blockAnalysisInfo[blockNum]);

      if (_traceSW)
         {
         traceMsg(comp(), "temp1: \n");
         temp->print(comp());
         traceMsg(comp(), "\n");
         }

      *temp -= *(registerAnticipatability._outSetInfo[blockNum]);

      if (_traceSW)
         {
         traceMsg(comp(), "temp2: \n");
         temp->print(comp());
         traceMsg(comp(), "\n");
         }

      *temp &= *scratch;

      if (_traceSW)
         {
         traceMsg(comp(), "temp3: \n");
         temp->print(comp());
         traceMsg(comp(), "\n");
         }

      *_restoreInfo[blockNum] |= *temp;

      if (_traceSW)
         {
         traceMsg(comp(), "restore: \n");
         _restoreInfo[blockNum]->print(comp());
         traceMsg(comp(), "\n");
         }
      }

   // propagate the restoreInfo of the end node into its predecessors
   //
   TR_PredecessorIterator edgeIt(_cfg->getEnd());
   for (TR::CFGEdge *e = edgeIt.getFirst(); e; e = edgeIt.getNext())
      {
      int32_t blockNum = e->getFrom()->getNumber();
      *_restoreInfo[blockNum] |= *_restoreInfo[_cfg->getEnd()->getNumber()];
      }

   // filter the save/restore sets
   //
   for (int32_t i = 0; i < _numberOfNodes; i++)
      {
      *_saveInfo[i] &= *_preservedRegsInLinkage;
      *_restoreInfo[i] &= *_preservedRegsInLinkage;
      }

   if (_traceSW)
      {
      traceMsg(comp(), "SAVE/RESTORE solution: \n");
      for (int32_t i = 0; i < _numberOfNodes; i++)
         {
         traceMsg(comp(), "SAVE for block_%d : ", i);
         _saveInfo[i]->print(comp());
         TR_BitVectorIterator bvIt(*_saveInfo[i]);
         traceMsg(comp(), " { ");
         while (bvIt.hasMoreElements())
            {
            int32_t index = bvIt.getNextElement();
            if (comp()->cg()->isPreservedRegister(index) != -1)
               traceMsg(comp(), "%s ", comp()->getDebug()->getRealRegisterName(index-1));
            }
         traceMsg(comp(), " }\n");
         }

      for (int32_t i = 0; i < _numberOfNodes; i++)
         {
         traceMsg(comp(), "RESTORE for block_%d : ", i);
         _restoreInfo[i]->print(comp());
         TR_BitVectorIterator bvIt(*_restoreInfo[i]);
         traceMsg(comp(), " { ");
         while (bvIt.hasMoreElements())
            {
            int32_t index = bvIt.getNextElement();
            if (comp()->cg()->isPreservedRegister(index) != -1)
               traceMsg(comp(), "%s ", comp()->getDebug()->getRealRegisterName(index-1));
            }
         traceMsg(comp(), " }\n");
         }
      }
   }

TR::Instruction *TR_ShrinkWrap::findReturnInBlock(int32_t blockNum)
   {
   // look for the return instruction
   //
   TR::Instruction *cursor = _swBlockInfo[blockNum]._endInstr;
   TR::Instruction *loc = cursor;
   bool foundFence = false;
   while (loc != _swBlockInfo[blockNum]._startInstr)
      {
      if (comp()->cg()->isFenceInstruction(loc) == 2) // BBEnd
         {
         foundFence = true;
         break;
         }
      loc = loc->getPrev();
      }

   TR_ASSERT(foundFence, "Looking for BBEnd fence, but actually found [%p]\n", loc);
   if (foundFence)
      {
      while (loc != _swBlockInfo[blockNum]._startInstr)
         {
         if (comp()->cg()->isReturnInstruction(loc))
            return loc; //loc is RET
         loc = loc->getPrev();
         }
      TR_ASSERT(0, "no return found in block_%d that should end in return\n", blockNum);
      }
   return NULL;
   }

static bool isPredecessor(TR::Block *pred, TR::Block *cur)
   {
   for (auto e = cur->getPredecessors().begin(); e != cur->getPredecessors().end(); ++e)
      {
      if (toBlock((*e)->getFrom()) == pred)
         return true;
      }
   return false;
   }

void TR_ShrinkWrap::doPlacement(TR_RegisterAnticipatability &registerAnticipatability, TR_RegisterAvailability &registerAvailability)
   {
   // iterate over each basic block
   // look at its SAVE/RESTORE set and save/restore each callee-saved register as needed
   //   - if block is inside loop, walk up the structure hierarchy finding the first parent
   //   not contained in a loop and do placements there
   //
   int32_t numRegs = TR::RealRegister::NumRegisters;
   int32_t cfgStartNum = _cfg->getStart()->getNumber();
   int32_t cfgEndNum = _cfg->getEnd()->getNumber();
   int32_t firstBlockNum = comp()->getStartTree()->getEnclosingBlock()->getNumber();

   TR_BitVector *visited = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
   TR_BitVector *blocksVisited = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
   TR_BitVector *splitBlocks = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);
   TR_BitVector *blocksSplitSoFar = new (trStackMemory()) TR_BitVector(_numberOfNodes, trMemory(), stackAlloc);

   // STEP1: generate the reverse postorder traversal of the
   // flow graph. this is used to create the register save
   // description bits at each basic block
   //
   _cfg->createTraversalOrder(true, stackAlloc);
   if (_traceSW)
      {
      traceMsg(comp(), "Forward traversal:\n");
      for (int32_t i = 0; i < _cfg->getForwardTraversalLength(); i++)
         traceMsg(comp(), "\t%d (%d)\n", _cfg->getForwardTraversalElement(i)->getNumber(),
                                       _cfg->getForwardTraversalElement(i)->getForwardTraversalIndex());
      }

   // STEP 2: populate the preservedRegsInfo data structures
   //
   for (int32_t blockNum = 0; blockNum < _numberOfNodes; blockNum++)
      {

      // collect all the return blocks
      //
      bool hasExceptionSuccs = false;
      TR_YesNoMaybe isReturnBlock = blockEndsInReturn(blockNum, hasExceptionSuccs);

      if (isReturnBlock == TR_yes)
         {
         ReturnBlockInfo  *r = new (trStackMemory()) ReturnBlockInfo;
         r->_blockNum = blockNum;
         r->_restoresRegisters = false;
         r->_numRestoredRegisters = 0;
         _returnBlocks.add(r);
         }

      // nothing to do
      //
      if (_saveInfo[blockNum]->isEmpty() &&
            _restoreInfo[blockNum]->isEmpty())
         continue;

      // the solution from the dummy exit have been propagated
      //
      if (blockNum == _cfg->getEnd()->getNumber())
         continue;

      // if this is the dummy entry or the first block, then just populate
      // the preservedRegsInPrologue bit vector and let the codegen do the
      // necessary saves in the prologue
      //
      if ((blockNum == firstBlockNum) ||
            (blockNum == cfgStartNum))
         {
         if (!_saveInfo[blockNum]->isEmpty())
            {
            TR_BitVector *p = comp()->cg()->getPreservedRegsInPrologue();
            *p |= *_saveInfo[blockNum];
            _saveInfo[blockNum]->empty();
            comp()->cg()->setPreservedRegsInPrologue(p);
            }
         }

      // do not save on blocks that do not end in return
      // but have edges to the exit block (e.g. blocks
      // that eventually throw an exception)
      //
      bool addInfo = true;
#if 0
      if (!_saveInfo[blockNum]->isEmpty() &&
            (isReturnBlock == TR_no))
         {
         if (_traceSW)
            traceMsg(comp(), "not saving registers in block [%d] that does not contain return\n", blockNum);
         addInfo = false;
         }
#endif

      if (addInfo)
         {
         TR_BitVectorIterator savIt(*_saveInfo[blockNum]);
         while (savIt.hasMoreElements())
            {
            int32_t regIndex = savIt.getNextElement();
            PreservedRegisterInfo *p = findPreservedRegisterInfo(regIndex);
            if (p)
               {
               p->_saveBlocks->set(blockNum);
               }
            }
         }

      addInfo = true;

      // do not restore on blocks that do not end in return
      // but have edges to the exit block (e.g. blocks
      // that eventually throw an exception)
      //
      if (!_restoreInfo[blockNum]->isEmpty() &&
            (isReturnBlock == TR_no))
         {
         if (_traceSW)
            traceMsg(comp(), "not restoring registers in block [%d] that does not contain return\n", blockNum);
         addInfo = false;
         }

      if (addInfo)
         {
         TR_BitVectorIterator resIt(*_restoreInfo[blockNum]);
         while (resIt.hasMoreElements())
            {
            int32_t regIndex = resIt.getNextElement();
            PreservedRegisterInfo *p = findPreservedRegisterInfo(regIndex);
            if (p)
               {
               p->_restoreBlocks->set(blockNum);
               }
            }
         }
      }

   // determine whether the usage of lastOptTransformationIndex (for debugging)
   // is going to affect the registers saved in the prologue. if this is the case,
   // we need to make sure the metadata is correct
   //
   bool shrinkWrapWillbeDone = false;
   if (1)
      {
      TR_BitVector *temp = comp()->cg()->getPreservedRegsInPrologue();
      *temp |= *_saveInfo[cfgStartNum];
      *temp |= *_saveInfo[firstBlockNum];
      comp()->cg()->setPreservedRegsInPrologue(temp);

      bool debugMode = false;
      for (PreservedRegisterInfo *p = _preservedRegs.getFirst(); p; p = p->getNext())
         {
         int32_t regIndex = p->_index;

         // nothing to do if reg really needs to be saved in prologue
         //
         if (comp()->cg()->getPreservedRegsInPrologue()->get(regIndex))
            continue;

         // if the save blocks of this register is empty, then it means
         // its getting saved in the prologue (can happen on x86-32)
         //
         if (p->_saveBlocks->isEmpty())
            {
            traceMsg(comp(), "         O^O Preserved reg %d has no save blocks -- saved in prologue\n", regIndex);
            comp()->cg()->getPreservedRegsInPrologue()->set(regIndex);
            continue;
            }


         // cycle through the restore blocks of this register
         // if any of the blocks on which this register needs to be
         // restored is in a loop, then conservatively avoid shrink wrapping
         // this register
         // FIXME: the right way to do this is to figure out if this block
         // is actually the block to which a(n inner) loop exits to and then restore the
         // register along that edge. this would mean either tweaking the regAvailability
         // sets and then doing the right checks in the restore part of STEP3 or just
         // special casing this case
         //
         if (1)
            {
            TR_BitVectorIterator resIt(*p->_restoreBlocks);
            bool found = false;
            while (resIt.hasMoreElements())
               {
               int32_t blockNum = resIt.getNextElement();
               TR::Block *curBlock = _swBlockInfo[blockNum]._block;

               TR::Instruction *cursor = _swBlockInfo[blockNum]._endInstr;
               if (curBlock->getStructureOf()->getContainingLoop())
                  {
                  traceMsg(comp(), "         O^O Register %d is going to be restored in a block_%d inside a loop, no shrink wrap\n", regIndex, curBlock->getNumber());
                  found = true;
                  break;
                  }

               // handle catch blocks that contain returns
               // since we cannot split exception edges, we
               // conservatively prevent shrink wrapping this register
               //
               else if (!curBlock->getExceptionSuccessors().empty())
                  {
                  for (auto e = curBlock->getExceptionSuccessors().begin(); e != curBlock->getExceptionSuccessors().end(); ++e)
                     {
                     int32_t catchBlockNum = (*e)->getTo()->getNumber();
                     TR::Block *catchBlock = toBlock((*e)->getTo());
                     ReturnBlockInfo *r = findReturnBlockInfo(catchBlockNum);
                     bool catchBlockHasSinglePred = (catchBlock->getExceptionPredecessors().size() == 1);
                     if (r &&
                           !registerAvailability._inSetInfo[catchBlockNum]->get(regIndex) &&
                           !registerAnticipatability._blockAnalysisInfo[catchBlockNum]->get(regIndex))
                        {
                        // if the catch block has a single predecessor, then we can
                        // just restore it on the exit
                        //
                        if (catchBlockHasSinglePred)
                           {
                           r->_restoresRegisters = true;
                           _restoreInfo[catchBlockNum]->set(regIndex);
                           }
                        else // no hope
                           {
                           traceMsg(comp(), "         O^O Register %d is going to be restored on a catch block_%d that contains a return and needs to be split\n", regIndex, catchBlockNum);
                           found = true;
                           break;
                           }
                        }
                     else if (!catchBlockHasSinglePred)
                        {
                        //FIXME: this is the case when a restore block has exception succs
                        // and the catch block has in turn multiple predecessors. in this case
                        // we cannot split this edge, so we currently have no way of restoring this
                        // register correctly (the register may be restored at the end of the block but
                        // that is not good enough if the exception happens to be taken from the middle of
                        // the block). conservatively mark this register as an untouchable
                        //
                        traceMsg(comp(), "         O^O Register %d is going to be restored in a block_%d that has exception edges\n", regIndex, blockNum);
                        found = true;
                        break;
                        }
                     }

                  if (!found)
                     {
                     bool hasExceptionSuccs = false;
                     TR_YesNoMaybe isReturnBlock = blockEndsInReturn(blockNum, hasExceptionSuccs);
                     if ((isReturnBlock == TR_maybe) && hasExceptionSuccs)
                        {
                        traceMsg(comp(), "         O^O Register %d is going to be restored in a block_%d that contains a throw and has exception successors\n", regIndex, blockNum);
                        found = true;
                        break;
                        }
                     }
                  }
               }

            if (found)
               {
               // conservatively mark this register as being saved/restored in
               // the prologue
               //
               comp()->cg()->getPreservedRegsInPrologue()->set(regIndex);
               continue;
               }


            // check if any of the save blocks are catch blocks
            // and if any of their preds need to be split. if this is
            // the case, then we conservatively designate this register
            // to be not shrinkwrappable (as we cannot split exception edges)
            //
            TR_BitVectorIterator savIt(*p->_saveBlocks);
            found = false;
            while (savIt.hasMoreElements())
               {
               int32_t blockNum = savIt.getNextElement();
               TR::Block *curBlock = _swBlockInfo[blockNum]._block;
               if (!curBlock->getExceptionPredecessors().empty())
                  {
                  bool anyPredsNeedSplit = false;
                  for (auto e = curBlock->getExceptionPredecessors().begin(); e != curBlock->getExceptionPredecessors().end(); ++e)
                     {
                     TR::Block *predBlock = toBlock((*e)->getFrom());
                     int32_t predBlockNum = predBlock->getNumber();
                     if (registerAnticipatability._outSetInfo[predBlockNum]->get(regIndex) ||
                           registerAvailability._blockAnalysisInfo[predBlockNum]->get(regIndex))
                        {
                        anyPredsNeedSplit = true;
                        if (_traceSW)
                           traceMsg(comp(), "         O^O Edge between predecessor block_%d of catch block_%d needs to be split\n", predBlockNum, blockNum);
                        break;
                        }
                     }

                  if (anyPredsNeedSplit)
                     {
                     traceMsg(comp(), "         O^O Register %d is going to be saved in a catch block_%d that needs to be split\n", regIndex, blockNum);
                     found = true;
                     break;
                     }
                  }
               }

            if (found)
               {
               // conservatively mark this register as being saved/restored in
               // the prologue
               //
               comp()->cg()->getPreservedRegsInPrologue()->set(regIndex);
               continue;
               }
            }

         bool doSave = performTransformation(comp(), "%s register %s [%d] at stack offset [%d]\n", OPT_DETAILS, comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, _mapRegsToStack[regIndex]);
         if (!doSave)
            {
            comp()->cg()->getPreservedRegsInPrologue()->set(regIndex);
            debugMode = true;
            }
         else
            shrinkWrapWillbeDone = true;
         }
      }

   // on platforms that use load/store multiple,
   // check the benefit of shrinkwrapping
   // basically, we don't want to break the store multiple group in
   // the prologue if only 2 registers out of the registers used in
   // the method are going to be shrinkwrapped. this will cause us to
   // break down the store multiple into individual stores thereby
   // creating more pressure on the store queue
   //
   if (shrinkWrapWillbeDone &&
         comp()->cg()->getUsesLoadStoreMultiple())
      {
      int32_t regsUsedInMethod = _preservedRegsInMethod->elementCount();
      int32_t regsNotShrinkWrapped = comp()->cg()->getPreservedRegsInPrologue()->elementCount();
      if ((regsUsedInMethod - regsNotShrinkWrapped) <= 2) // no benefit to breaking the store multiple in prologue
         {
         shrinkWrapWillbeDone = false;
         traceMsg(comp(), "No benefit to shrinkwrapping %d regs out of %d preserved regs used in method\n",
               regsUsedInMethod, regsNotShrinkWrapped);
         }
      }

   // check for work to do
   //
   if (!shrinkWrapWillbeDone)
      {
      traceMsg(comp(), "No register can be shrink wrapped.\n");
      if (_traceSW)
         {
         traceMsg(comp(), "These regs are saved (restored) in prologue (epilogue): ");
         comp()->cg()->getPreservedRegsInPrologue()->print(comp());
         traceMsg(comp(), "\n");
         }
      comp()->cg()->setPreservedRegsInPrologue(NULL);
      return;
      }

   // Found work to do!!
   //
   ///printf("done in %s\n", comp()->signature());fflush(stdout);

   // first build the bitvector of registers that are shrinkwrapped
   // this will be used in the stackwalker to walk the right slots
   //
   TR_BitVector *registersShrinkWrapped = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
   registersShrinkWrapped->empty();
   for (PreservedRegisterInfo *p = _preservedRegs.getFirst(); p; p = p->getNext())
      {
      int32_t regIndex = p->_index;
      if (!comp()->cg()->getPreservedRegsInPrologue()->get(regIndex))
         registersShrinkWrapped->set(regIndex);
      }

   // setup the lowestSavedReg info which is used in to emit the metadata
   // note: this is needed on x86 because the preserved regs are not necessarily
   // allocated in sequence on x86
   //
   comp()->cg()->computeRegisterSaveDescription(registersShrinkWrapped, true);

   // now setup the register save description bits at each basic
   // block. this is used to indicate which stack slots (containing
   // preserved regs) are live and need to be walked by the stack walker
   //
   TR_ScratchList<TR::CFGEdge> backEdges(trMemory());
   for (int32_t i = 0; i < _cfg->getForwardTraversalLength(); i++)
      {
      int32_t blockNum = _cfg->getForwardTraversalElement(i)->getNumber();
      visited->set(blockNum);

      TR::Block *curBlock = _swBlockInfo[blockNum]._block;

      // dont propagate for dummy entry
      //
      if (blockNum == cfgStartNum)
         continue;

      // all stack slots holding saved regs in this block need to go live
      //
      TR_BitVector *livePreservedRegs = curBlock->getRegisterSaveDescriptionBits();
      if (!livePreservedRegs)
         livePreservedRegs = new (trHeapMemory()) TR_BitVector(*_saveInfo[blockNum]);
      else
         *livePreservedRegs |= *_saveInfo[blockNum];

      // stack slots corresp. to regs saved in preds are no longer live
      // in this block ; ignore back edges
      //
      TR_PredecessorIterator predIt(curBlock);
      for (TR::CFGEdge *e = predIt.getFirst(); e; e = predIt.getNext())
         {
         TR::Block *pred = toBlock(e->getFrom());
         bool isBackEdge = false;
         if (pred->getForwardTraversalIndex() >= curBlock->getForwardTraversalIndex())
            {
            isBackEdge = true;
            backEdges.add(e);
            if (_traceSW)
               traceMsg(comp(), "ignoring backedge from [%d] to [%d]\n", pred->getNumber(), curBlock->getNumber());
            }

         if (!isBackEdge)
            {
            TR_ASSERT(visited->get(pred->getNumber()),
                    "computing register save description for node [%d] whose pred is not processed [%d]\n",
                    blockNum,
                    pred->getNumber());
            ///*livePreservedRegs -= *_restoreInfo[pred->getNumber()];
            TR_BitVectorIterator resIt(*_restoreInfo[pred->getNumber()]);
            while (resIt.hasMoreElements())
               {
               int32_t regIndex = resIt.getNextElement();
               if (!registerAvailability._inSetInfo[blockNum]->get(regIndex) &&
                     !registerAnticipatability._blockAnalysisInfo[blockNum]->get(regIndex))
                  livePreservedRegs->reset(regIndex);
               }
            }
         }

      // finally stack slots corresp. to regs that will be restored in this block are
      // still live until the end of the block along with the ones saved in the prologue
      //
      *livePreservedRegs |= *_restoreInfo[blockNum];
      *livePreservedRegs |= *comp()->cg()->getPreservedRegsInPrologue();
      // make sure this vector only contains bits corresponding to regs used in this
      // method. NOTE: we cannot get this info wrong, as the metadata needs to be
      // correct, otherwise the stack walker will end up populating the wrong info
      //
      *livePreservedRegs &= *_preservedRegsInMethod;
      if (_traceSW)
         {
         traceMsg(comp(), "livePreservedRegs for block[%d] : ", curBlock->getNumber());
         livePreservedRegs->print(comp());
         traceMsg(comp(), "\n");
         }
      curBlock->setRegisterSaveDescriptionBits(livePreservedRegs);
      markInstrsInBlock(blockNum);

      if (_traceSW)
         {
         traceMsg(comp(), "registerDesriptionBits for block [%d] : ", curBlock->getNumber());
         curBlock->getRegisterSaveDescriptionBits()->print(comp());
         traceMsg(comp(), "\n");
         }

      // push this info along all the succs of this block
      //
      TR_SuccessorIterator succIt(curBlock);
      for (TR::CFGEdge *e = succIt.getFirst(); e; e = succIt.getNext())
         {
         TR::Block *succ = toBlock(e->getTo());
         if (backEdges.find(e))
            {
            if (_traceSW)
               traceMsg(comp(), "not propagating info along backedge from [%d] to [%d]\n", curBlock->getNumber(), succ->getNumber());
            continue;
            }
         TR_BitVector *scratch = new (trHeapMemory()) TR_BitVector(*livePreservedRegs);
         succ->setRegisterSaveDescriptionBits(scratch);
         }
      }

   // STEP3: do the placements according to the saves/restores collected above
   // for each preserved register
   //
   if (_traceSW)
      {
      for (PreservedRegisterInfo *p = _preservedRegs.getFirst(); p; p = p->getNext())
         {
         traceMsg(comp(), "For preserved register: %d [%s]\n", p->_index, comp()->getDebug()->getRealRegisterName(p->_index-1));
         traceMsg(comp(), "   Save blocks: ");
         p->_saveBlocks->print(comp());
         if (p->_saveBlocks->isEmpty())
            traceMsg(comp(), "   register %d saved in prologue\n", p->_index);
         traceMsg(comp(), "\n");
         traceMsg(comp(), "   Restore blocks: ");
         p->_restoreBlocks->print(comp());
         traceMsg(comp(), "\n");
         }
      }

   bool isWrapped = false;
   blocksVisited->empty();

   // pre-process step
   // first split any loop entry blocks if required
   // the purpose of this code is to create a pre-header label
   // so that the loop entry block has a single predecessor (not counting
   // the backedge). the other predecessors are re-directed to the new label
   //
   for (PreservedRegisterInfo *p = _preservedRegs.getFirst(); p; p = p->getNext())
      {
      int32_t regIndex = p->_index;

      if (_traceSW)
         traceMsg(comp(), "      Pre-Processing save/restore for register: %d [%s]\n", regIndex, comp()->getDebug()->getRealRegisterName(regIndex-1));

      if (p->_saveBlocks->isEmpty() ||
            comp()->cg()->getPreservedRegsInPrologue()->get(regIndex))
         {
         // nothing to do
         //
         continue;
         }
      // do the saves and restores
      //
      TR_BitVectorIterator savIt(*p->_saveBlocks);
      while (savIt.hasMoreElements())
         {
         int32_t blockNum = savIt.getNextElement();
         TR::Block *curBlock = _swBlockInfo[blockNum]._block;
         bool isInLoop = curBlock->getStructureOf()->getContainingLoop() ? true : false;

         // if isInLoop, then create a new label for the entry block
         // since we should not be saving at the loop entry block (as the
         // saves should not be done every iteration -- its incorrect from
         // both a functional & a performance point of view
         //
         if (isInLoop &&
               !blocksVisited->get(blockNum))
            {
            blocksVisited->set(blockNum);
            // loop entry block has more than one predecessors
            //
            if (1)///!(curBlock->getPredecessors().size() == 1))
               {
               TR::Instruction *entry = _swBlockInfo[blockNum]._startInstr;
               // now look for the nop alignment so that the loop entry is
               // aligned correctly even after adding necessary saves
               //
               TR::Instruction *loc = entry;
               bool foundNop = false;
               while (!comp()->cg()->isFenceInstruction(loc))
                  {
                  if (comp()->cg()->isAlignmentInstruction(loc))
                     {
                     foundNop = true;
                     break;
                     }
                  loc = loc->getPrev();
                  }
               if (foundNop)
                  entry = loc;
               TR::Instruction *newEntry = comp()->cg()->splitBlockEntry(entry);
               _swBlockInfo[blockNum]._startInstr = newEntry;

               traceMsg(comp(), "         O^O Loop entry block_%d split at newentry %p\n", blockNum, newEntry);

               for (auto e = curBlock->getPredecessors().begin(); e != curBlock->getPredecessors().end(); ++e)
                  {
                  // ignore backedges
                  //
                  if (backEdges.find(*e))
                     continue;

                  TR::Block *predBlock = toBlock((*e)->getFrom());
                  int32_t predBlockNum = predBlock->getNumber();
                  bool isFallThrough = true;
                  TR::Node *branchNode = predBlock->getExit()->getPrevRealTreeTop()->getNode();
                  if (branchNode->getOpCode().isBranch())
                     {
                     if (branchNode->getBranchDestination()->getNode()->getBlock() == curBlock)
                        isFallThrough = false;
                     }

                  TR::Instruction *cur = _swBlockInfo[predBlockNum]._endInstr;
                  TR::list<TR::Instruction*> jmpInstrs(getTypedAllocator<TR::Instruction*>(TR::comp()->allocator()));
                  if (!isFallThrough)
                     {
                     cur = findJumpInstructionsInBlock(predBlockNum, &jmpInstrs);
                     }
                  // now re-direct any branches to the new split entry
                  //
                  comp()->cg()->splitEdge(cur, isFallThrough, false, newEntry, &jmpInstrs);
                  traceMsg(comp(), "         O^O Loop pred block_%d split at %p\n", predBlockNum, cur);
                  }
               }
            }
         }
      }

   splitBlocks->empty();
   blocksSplitSoFar->empty();
   for (PreservedRegisterInfo *p = _preservedRegs.getFirst(); p; p = p->getNext())
      {
      int32_t regIndex = p->_index;

      if (_traceSW)
         traceMsg(comp(), "      Processing saves for register: %d [%s]\n", regIndex, comp()->getDebug()->getRealRegisterName(regIndex-1));

      if (p->_saveBlocks->isEmpty())
         {
         // nothing to do
         //
         comp()->cg()->getPreservedRegsInPrologue()->set(regIndex);
         if (_traceSW)
            traceMsg(comp(), "      Nothing to do for register %d [%s]\n", regIndex, comp()->getDebug()->getRealRegisterName(regIndex-1));
         continue;
         }

      // don't bother shrink wrapping registers if they
      // have to be saved in the prologue according to the
      // analysis
      //
      if (comp()->cg()->getPreservedRegsInPrologue()->get(regIndex))
         {
         if (_traceSW)
            traceMsg(comp(), "      Not shrink wrapping register %d [%s] as it is saved in prologue\n", regIndex, comp()->getDebug()->getRealRegisterName(regIndex-1));
         continue;
         }

      /*
      bool doSave = performTransformation(comp(), "%sSaving register %s [%d] at stack offset [%d]\n", OPT_DETAILS, comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, _mapRegsToStack[regIndex]);

      // if performTransformation prevents this register from being shrinkWrapped,
      // save it in the prologue
      //
      if (!doSave)
         {
         comp()->cg()->getPreservedRegsInPrologue()->set(p->_index);
         // this register is not shrink wrapped
         //
         continue;
         }
      */

      // do the saves and then the restores
      // this is needed because on some platforms that support
      // load/store multiples we try to coalesce the individual stores/loads
      // back into groups. for the coalescing code to work correctly, we
      // shouldn't interleave the saves/restores
      //
      TR_BitVectorIterator savIt(*p->_saveBlocks);
      while (savIt.hasMoreElements())
         {
         int32_t blockNum = savIt.getNextElement();
         TR::Block *curBlock = _swBlockInfo[blockNum]._block;
         bool isInLoop = curBlock->getStructureOf()->getContainingLoop() ? true : false;

         // blockNum is the placement point for this save
         //
         if (_traceSW)
            traceMsg(comp(), "      save block [%d] is in loop [%s]\n", blockNum, isInLoop ? "yes" : "no");

         // if isInLoop, then create a new label for the entry block
         // since we should not be saving at the loop entry block (as the
         // saves should not be done every iteration -- its incorrect from
         // both a functional & a performance point of view
         //
#if 0
         if (isInLoop &&
               !blocksVisited->get(blockNum))
            {
            blocksVisited->set(blockNum);
            TR::Instruction *entry = _swBlockInfo[blockNum]._startInstr;
            TR::Instruction *newEntry = comp()->cg()->splitBlockEntry(entry);
            _swBlockInfo[blockNum]._startInstr = newEntry;
            traceMsg(comp(), "loop entry block_%d split at newentry %p\n", blockNum, newEntry);

            for (auto e = curBlock->getPredecessors().begin(); e != curBlock->getPredecessors().end(); ++e)
               {
               // ignore backedges
               //
               if (backEdges.find(*e))
                  continue;

               TR::Block *predBlock = toBlock((*e)->getFrom());
               int32_t predBlockNum = predBlock->getNumber();
               bool isFallThrough = true;
               TR::Node *branchNode = predBlock->getExit()->getPrevRealTreeTop()->getNode();
               if (branchNode->getOpCode().isBranch())
                  {
                  if (branchNode->getBranchDestination()->getNode()->getBlock() == curBlock)
                     isFallThrough = false;
                  }

               TR::Instruction *cur = _swBlockInfo[predBlockNum]._endInstr;
               TR::list<TR::Instruction*> jmpInstrs(getTypedAllocator<TR::Instruction*>(TR::comp()->allocator()));
               if (!isFallThrough)
                  {
                  // find the first jump instruction in the pred block
                  //
                  while (cur && !comp()->cg()->isBranchInstruction(cur))
                     cur = cur->getPrev();
                  // collect any remaining jmps in the block
                  //
                  TR::Instruction *loc = cur;
                  while (loc != _swBlockInfo[predBlockNum]._startInstr)
                     {
                     if (comp()->cg()->isBranchInstruction(loc))
                        jmpInstrs.push_front(loc);
                     loc = loc->getPrev();
                     }
                  }
               // now re-direct any branches to the new split entry
               //
               comp()->cg()->splitEdge(cur, isFallThrough, false, newEntry, &jmpInstrs);
               traceMsg(comp(), "pred block_%d split at %p\n", predBlockNum, cur);
               }
            }
#endif

         TR::Instruction *cursor = _swBlockInfo[blockNum]._startInstr;

         if (!curBlock->getExceptionPredecessors().empty())
            {
            // for catch blocks, make sure the start is the fence instruction
            //
            bool foundFence = false;
            TR::Instruction *loc = _swBlockInfo[blockNum]._startInstr;
            while (loc != _swBlockInfo[blockNum]._endInstr)
               {
               if (comp()->cg()->isFenceInstruction(loc) == 1) // BBStart
                  {
                  foundFence = true;
                  break;
                  }
               loc = loc->getNext();
               }

            if (foundFence)
               cursor = loc;

            bool doneSave = false;
            for (auto e = curBlock->getExceptionPredecessors().begin(); e != curBlock->getExceptionPredecessors().end(); ++e)
               {
               TR::Block *predBlock = toBlock((*e)->getFrom());
               if (!registerAnticipatability._outSetInfo[predBlock->getNumber()]->get(regIndex) &&
                     !registerAvailability._blockAnalysisInfo[predBlock->getNumber()]->get(regIndex) &&
                     !doneSave)
                  {
                  traceMsg(comp(), "         O^O Saving register %s [%d] at instr [%p] in catch block [%d] with stack offset [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, cursor, blockNum, _mapRegsToStack[regIndex]);
                  int32_t offset = _linkageUsesPushes ? -1 : _mapRegsToStack[regIndex];
                  comp()->cg()->getLinkage()->savePreservedRegister(cursor, regIndex, offset);
                  doneSave = true;
                  }

               if (!doneSave)
                  {
                  if (_traceSW)
                     traceMsg(comp(), "      register %s [%d] is used in some exception predecessor of catch block [%d], not saved\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, blockNum);
                  }
               }
            }
         else if (curBlock->getPredecessors().size() == 1)
            {
            TR::CFGEdge *e = curBlock->getPredecessors().front();

            TR::Block *predBlock = toBlock(e->getFrom());
            if (!registerAnticipatability._outSetInfo[predBlock->getNumber()]->get(regIndex))
               {
               traceMsg(comp(), "         O^O Saving register %s [%d] at instr [%p] block [%d] with stack offset [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, cursor, blockNum, _mapRegsToStack[regIndex]);
               int32_t offset = _linkageUsesPushes ? -1 : _mapRegsToStack[regIndex];
               comp()->cg()->getLinkage()->savePreservedRegister(cursor, regIndex, offset);
               if (!_swBlockInfo[blockNum]._savedRegs)
                  {
                  _swBlockInfo[blockNum]._savedRegs = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
                  _swBlockInfo[blockNum]._savedRegs->empty();
                  }
               _swBlockInfo[blockNum]._savedRegs->set(regIndex);
               }
            }
         else
            {
            // first determine if any predecessor edges need to be split
            // depending on whether the register is available on some path
            // into this block. If no preds need to be split, then we can
            // just do the save at the block entry
            // For loops, we always need to split the predecessor edges
            // because we should not save on the loop entry block
            //
            bool anyPredsNeedSplit = false;
            ///if (isInLoop) anyPredsNeedSplit = true;

            // iterate through the preds and determine if the reg needs to be
            // saved on the fall-through. if not, then we need to insert a
            // jmp so that we skip the save on the fall-through
            //
            bool fallThroughNeedsJump = true;
            if (1)
               {
               for (auto e = curBlock->getPredecessors().begin(); e != curBlock->getPredecessors().end(); ++e)
                  {
                  // ignore backedges in loops
                  //
                  if (isInLoop && backEdges.find(*e))
                     continue;

                  TR::Block *predBlock = toBlock((*e)->getFrom());
                  int32_t predBlockNum = predBlock->getNumber();
                  bool isFallThrough = true;
                  TR::Node *branchNode = predBlock->getExit()->getPrevRealTreeTop()->getNode();
                  if (branchNode->getOpCode().isBranch())
                     {
                     if (branchNode->getBranchDestination()->getNode()->getBlock() == curBlock)
                        isFallThrough = false;
                     }
                  if (isFallThrough)
                     {
                     if (!registerAnticipatability._outSetInfo[predBlockNum]->get(regIndex) &&
                           !registerAvailability._blockAnalysisInfo[predBlockNum]->get(regIndex))
                        fallThroughNeedsJump = false;
                     traceMsg(comp(), "         O^O Found fall-through block_%d needsJump %d anyPredsNeedSplit %d\n", predBlockNum, fallThroughNeedsJump, anyPredsNeedSplit);
                     }

                  if (!anyPredsNeedSplit)
                     {
                     if (registerAnticipatability._outSetInfo[predBlockNum]->get(regIndex) ||
                           registerAvailability._blockAnalysisInfo[predBlockNum]->get(regIndex))
                        {
                        anyPredsNeedSplit = true;
                        traceMsg(comp(), "         O^O PredBlock %d needsSplit\n", predBlockNum);
                        }
                     }
                  traceMsg(comp(), "         O^O Looked at pred %d anyPredsNeedSplit %d\n", predBlockNum, anyPredsNeedSplit);
                  }
               }

            TR::Instruction *newSplitLocation = NULL;
            for (auto e = curBlock->getPredecessors().begin(); e != curBlock->getPredecessors().end(); ++e)
               {
               bool doSaves = true;
               // ignore backedges in loops
               //
               if (isInLoop && backEdges.find(*e))
                  continue;

               TR::Block *predBlock = toBlock((*e)->getFrom());
               int32_t predBlockNum = predBlock->getNumber();
               bool isFallThrough = true;
               TR::Node *branchNode = predBlock->getExit()->getPrevRealTreeTop()->getNode();
               if (branchNode->getOpCode().isBranch())
                  {
                  if (branchNode->getBranchDestination()->getNode()->getBlock() == curBlock)
                     isFallThrough = false;
                  }

               if (!registerAnticipatability._outSetInfo[predBlockNum]->get(regIndex))
                  {
                  // need to split this edge and do a placement of the store
                  // If the reg happens to be available on exit of this predecessor
                  // then it means that it was used on some path into this block
                  // and thus need not be saved (as the save will write the wrong
                  // value into the stack)
                  //
                  if (!registerAvailability._blockAnalysisInfo[predBlockNum]->get(regIndex))
                     {
                     SWEdgeInfo *ei = findEdgeInfo(*e);

                     // check if the saves can be shared with another predecessor
                     //
                     // FIXME: the checks below are insufficient to decide whether
                     // saves can be shared between two edges. enable the code below
                     // when fixed
                     //
                     TR::Instruction *saveLocation = NULL;
                     // HACKIT
                     if (0 && !ei && splitBlocks->get(blockNum))
                        {
                        // compare the saveInfo on the preds to see if
                        // they can share a common save sequence
                        //
                        for (auto pred = curBlock->getPredecessors().begin(); pred != curBlock->getPredecessors().end(); ++pred)
                           {
                           int32_t pNum = (*pred)->getFrom()->getNumber();
                           if (pNum == predBlockNum)
                              continue;

                           SWEdgeInfo *pei = findEdgeInfo(*pred);
                           if (pei)
                              {
                              traceMsg(comp(), "         O^O Found pei for save predblock_%d\n", pNum);
                              TR_BitVector *temp1 = _saveInfo[predBlockNum];
                              *temp1 &= *_preservedRegsInLinkage;
                              TR_BitVector *temp2 = _saveInfo[blockNum];
                              *temp2 &= *_preservedRegsInLinkage;
                              *temp2 &= *temp1;
                              TR_BitVector *temp3 = registerAvailability._blockAnalysisInfo[predBlockNum];
                              *temp3 &= *_preservedRegsInLinkage;
                              TR_BitVector *temp4 = registerAvailability._blockAnalysisInfo[pNum];
                              *temp4 &= *_preservedRegsInLinkage;
                              traceMsg(comp(), "         O^O temp2 %d _restoreInfo[predBlockNum] %d _restoreInfo[pNum] %d isFallThrough %d\n", temp2->isEmpty(), _restoreInfo[predBlockNum]->isEmpty(), _restoreInfo[pNum]->isEmpty(), isFallThrough);
                              // make sure
                              // a) this register is not saved already in predBlock (temp2 being empty)
                              // b) this register is not being restored along edge from predBlockNum to block
                              // c) this register is not being restored along edge from pNum to block
                              //
                              if (temp2->isEmpty() &&
                                    (*temp3 == *temp4) &&
                                    _restoreInfo[predBlockNum]->isEmpty() &&
                                    _restoreInfo[pNum]->isEmpty() &&
                                    !isFallThrough)
                                 {
                                 traceMsg(comp(), "         O^O Can share split for saves between %d and %d\n", predBlockNum, pNum);
                                 ///fprintf(stderr, "did the stuff in %s\n", comp()->signature());fflush(stderr);
                                 ei = pei;
                                 saveLocation = pei->_restoreSplitLocation;
                                 break;
                                 }
                              }
                           }
                        }

                     // no info was found, so create one
                     //
                     if (!ei)
                        {
                        ei = new (trStackMemory()) SWEdgeInfo;
                        ei->_edge = *e;
                        ei->_saveSplitLocation = NULL;
                        ei->_restoreSplitLocation = saveLocation;
                        ei->_savedRegs = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
                        ei->_restoredRegs = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
                        ei->_savedRegs->empty();
                        ei->_restoredRegs->empty();
                        _swEdgeInfo.add(ei);
                        }

                     if (!ei->_restoreSplitLocation) //!ei->_saveSplitLocation)
                        {
                        traceMsg(comp(), "         O^O Going to split predBlock %d\n", predBlockNum);
                        // HACKIT
                        bool needsJump = true;
                        TR::Block *prevBlock = curBlock->getPrevBlock();
                        if (!isPredecessor(prevBlock, curBlock) && !blocksSplitSoFar->get(curBlock->getNumber()))
                           {
                           ///printf("found save suboptimality in %s\n", comp()->signature());fflush(stdout);
                           traceMsg(comp(), "         O^O No save jump needed between %d and %d\n", prevBlock->getNumber(), curBlock->getNumber());
                           needsJump = false;
                           }
                        cursor = _swBlockInfo[predBlockNum]._endInstr;
                        TR::list<TR::Instruction*> jmpInstrs(getTypedAllocator<TR::Instruction*>(TR::comp()->allocator()));
                        if (!isFallThrough)
                           {
                           cursor = findJumpInstructionsInBlock(predBlockNum, &jmpInstrs);
                           }
                        traceMsg(comp(), "         O^O Split cursor %p with newSplitLoc %p\n", cursor, newSplitLocation);
                        cursor = comp()->cg()->splitEdge(cursor, isFallThrough, needsJump, newSplitLocation, &jmpInstrs, true);
                        ///ei->_saveSplitLocation = cursor;
                        ei->_restoreSplitLocation = cursor;
                        splitBlocks->set(blockNum);
                        blocksSplitSoFar->set(blockNum);
                        }
                     else
                        {
                        ///cursor = ei->_saveSplitLocation;
                        cursor = ei->_restoreSplitLocation;
                        if (saveLocation && ei->_savedRegs->get(regIndex))
                           doSaves = false;
                        }

                     // prevent doing the same stores twice
                     //
                     if (doSaves)
                        {
                        traceMsg(comp(), "         O^O Saving register %s [%d] for block [%d] with instr [%p] at predecessor block (split) [%d] with stack offset [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, blockNum, cursor, predBlockNum, _mapRegsToStack[regIndex]);
                        int32_t offset = _linkageUsesPushes ? -1 : _mapRegsToStack[regIndex];
                        comp()->cg()->getLinkage()->savePreservedRegister(cursor, regIndex, offset);
                        ////doneSaves = true;
                        ei->_savedRegs->set(regIndex);
                        }

                     // saves were shared between preds, so fixup the branches if any to point
                     // to the new location
                     //
                     if (saveLocation)
                        {
                        TR::Instruction *newSaveLocation = cursor;
                        cursor = _swBlockInfo[predBlockNum]._endInstr;
                        TR::list<TR::Instruction*> jmpInstrs(getTypedAllocator<TR::Instruction*>(TR::comp()->allocator()));
                        if (!isFallThrough)
                           {
                           cursor = findJumpInstructionsInBlock(predBlockNum, &jmpInstrs);
                           }
                        cursor = comp()->cg()->splitEdge(cursor, isFallThrough, false, newSaveLocation, &jmpInstrs);
                        }
                     }
                  else
                     traceMsg(comp(), "         O^O Not saving register %s [%d] for block [%d] since its available in pred [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, blockNum, predBlockNum);
                  }
               }
            }

         isWrapped = true;
         }
      }

   // now do the restores
   //
   splitBlocks->empty();
   for (PreservedRegisterInfo *p = _preservedRegs.getFirst(); p; p = p->getNext())
      {
      int32_t regIndex = p->_index;

      if (_traceSW)
         traceMsg(comp(), "      Processing restores for register: %d [%s]\n", regIndex, comp()->getDebug()->getRealRegisterName(regIndex-1));

      if (p->_saveBlocks->isEmpty())
         {
         // nothing to do
         //
         comp()->cg()->getPreservedRegsInPrologue()->set(regIndex);
         if (_traceSW)
            traceMsg(comp(), "      Nothing to do for register %d [%s]\n", regIndex, comp()->getDebug()->getRealRegisterName(regIndex-1));
         continue;
         }

      // don't bother shrink wrapping registers if they
      // have to be saved in the prologue according to the
      // analysis
      //
      if (comp()->cg()->getPreservedRegsInPrologue()->get(regIndex))
         {
         if (_traceSW)
            traceMsg(comp(), "      Not shrink wrapping register %d [%s] as it is saved in prologue\n", regIndex, comp()->getDebug()->getRealRegisterName(regIndex-1));
         continue;
         }


      // intialize a bitvector to indicate whether
      // a register is already restored on entry to a block
      //
      visited->empty();
      TR_BitVectorIterator resIt(*p->_restoreBlocks);
      while (resIt.hasMoreElements())
         {
         int32_t blockNum = resIt.getNextElement();
         TR::Block *curBlock = _swBlockInfo[blockNum]._block;

         // this is the Fence instr corresp. to BBEnd
         //
         TR::Instruction *cursor = _swBlockInfo[blockNum]._endInstr;
         bool isInLoop = curBlock->getStructureOf()->getContainingLoop() ? true : false;

         // blockNum is the placement point for this restore
         //
         if (_traceSW)
            traceMsg(comp(), "      restore block [%d] is in loop [%s]\n", blockNum, isInLoop ? "yes" : "no");

         if (curBlock->getSuccessors().size() == 1)
            {
            bool doRestore = true;
            TR::CFGEdge *e = curBlock->getSuccessors().front();
            if (backEdges.find(e))
               {
               if (_traceSW)
                  traceMsg(comp(), "      not restoring along backedge [%d] -> [%d]\n", e->getFrom()->getNumber(), e->getTo()->getNumber());
               doRestore = false;
               }

            // if the register is available on the successor block, then
            // do not restore (except the exit block)
            //
            TR::Block *succBlock = toBlock(e->getTo());
            if (registerAvailability._inSetInfo[succBlock->getNumber()]->get(regIndex) &&
                  (succBlock != _cfg->getEnd()))
               {
               if (_traceSW)
                  traceMsg(comp(), "      not restoring along edge [%d] -> [%d] since reg is available\n", e->getFrom()->getNumber(), e->getTo()->getNumber());
               doRestore = false;
               }

            // block has a single successor, so its either a
            // fall-through block or a block that ends in a goto/return
            //
            if (doRestore)
               {
               bool isFallThrough = true;
               TR::Node *branchNode = curBlock->getExit()->getPrevRealTreeTop()->getNode();
               if (branchNode->getOpCode().isBranch()) // check for gotos
                  {
                  if (branchNode->getBranchDestination()->getNode()->getBlock() == succBlock)
                     isFallThrough = false;
                  }

               // look for returns or branches and place before those
               // first find the FENCE instruction
               //
               TR::Instruction *loc = cursor;
               bool foundFence = false;
               while (loc != _swBlockInfo[blockNum]._startInstr)
                  {
                  if (comp()->cg()->isFenceInstruction(loc) == 2) // BBEnd
                     {
                     foundFence = true;
                     break;
                     }
                  loc = loc->getPrev();
                  }

               if (foundFence)
                  {
                  // loc is the FENCE instruction, so place before this
                  ///cursor = loc->getPrev();
                  if (!isFallThrough) // find the branch instr if its a goto block
                     {
                     while (loc != _swBlockInfo[blockNum]._startInstr)
                        {
                        if (comp()->cg()->isBranchInstruction(loc))
                           {
                           cursor = loc->getPrev();
                           break;
                           }
                        loc = loc->getPrev();
                        }
                     }
                  else
                     {
                     cursor = loc->getPrev();
                     }
                  // remember the location for fall-through blocks so that
                  // we can use this as a starting point to coalesce these
                  // loads if required
                  _swBlockInfo[blockNum]._restoreLocation = loc;
                  /*
                  if (comp()->cg()->isReturnInstruction(loc) ||
                        comp()->cg()->isBranchInstruction(loc))
                     {
                     cursor = loc->getPrev();
                     if (comp()->cg()->isReturnInstruction(loc))
                        isReturn = true;
                     }
                  else
                     cursor = loc;
                  */
                  }

               traceMsg(comp(), "         O^O Restoring register %s [%d] at instr [%p] at end of block [%d] with stack offset [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, cursor, blockNum, _mapRegsToStack[regIndex]);
               // if its a return block, then we'll take care of the restores
               // at the end
               ReturnBlockInfo *r = findReturnBlockInfo(blockNum);
               if (r)
                  {
                  // on some platforms like ia32, we'd like to use the pop instruction
                  // to restore regs in blocks that contain the return instruction
                  // this has a dual purpose of the instr deallocating the frame as well
                  // as being compact
                  // FIXME: shrinkwrapping and using pushes is not compatible, so we wont
                  // use pushes on ia32
                  //
                  ///returnBlocks->set(blockNum);
                  r->_restoresRegisters = true;
                  }
               else
                  comp()->cg()->getLinkage()->restorePreservedRegister(cursor, regIndex, _mapRegsToStack[regIndex]);

               if (!_swBlockInfo[blockNum]._restoredRegs)
                  {
                  _swBlockInfo[blockNum]._restoredRegs = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
                  _swBlockInfo[blockNum]._restoredRegs->empty();
                  }
               _swBlockInfo[blockNum]._restoredRegs->set(regIndex);
               }
            }
         else
            {
            // this block has one or more successors and the register
            // needs to be restored. check if the successors have this
            // register in their availability inSet. if not, then the
            // edge from this block to the successor needs to be split
            // and then restored.
            //
            for (auto e = curBlock->getSuccessors().begin(); e != curBlock->getSuccessors().end(); ++e)
               {
               // dont restore along a backedge
               //
               if (backEdges.find(*e))
                  {
                  if (_traceSW)
                     traceMsg(comp(), "         O^O Not restoring along backedge [%d] -> [%d]\n", (*e)->getFrom()->getNumber(), (*e)->getTo()->getNumber());
                  continue;
                  }

               TR::Block *succBlock = toBlock((*e)->getTo());
               int32_t succBlockNum = succBlock->getNumber();
               bool isFallThrough = true;
               TR::Node *branchNode = curBlock->getExit()->getPrevRealTreeTop()->getNode();
               if (branchNode->getOpCode().isBranch())
                  {
                  if (branchNode->getBranchDestination()->getNode()->getBlock() == succBlock)
                     isFallThrough = false;
                  }

               if (!registerAvailability._inSetInfo[succBlockNum]->get(regIndex) &&
                     !registerAnticipatability._blockAnalysisInfo[succBlockNum]->get(regIndex))
                  {
                  if (!(succBlock->getPredecessors().size() == 1))
                     {
                     bool doRestore = true;
                     // need to split this edge and do the placement
                     // of the restore. remember the split location
                     // so that the edge is only split once
                     //
                     SWEdgeInfo  *ei = findEdgeInfo(*e);

                     // if no edge info is found, check if it can
                     // be shared with another edge with the same
                     // regs being restored
                     //
                     TR::Instruction *restoreLocation = NULL;
                     if (!ei && splitBlocks->get(succBlockNum))
                        {
                        // check if the block has been split before
                        // if so, then we can compare the restoreInfo
                        // on the preds to see if they can share a common
                        // restore sequence
                        //
                        for (auto pred = succBlock->getPredecessors().begin(); pred != succBlock->getPredecessors().end(); ++pred)
                           {
                           int32_t predBlockNum = (*pred)->getFrom()->getNumber();
                           if (predBlockNum == blockNum)
                              continue;

                           SWEdgeInfo *pei = findEdgeInfo(*pred);
                           if (pei)
                              {
                                 if (_traceSW)
                                    traceMsg(comp(), "found pei info for predblock_%d\n", predBlockNum);
                                 ///printf("found opportunity for new stuff in %s\n", comp()->signature());fflush(stdout);

                              // one of the predecessor edges has been split
                              // check if the restore info of this edge and
                              // the current block being analyzed is the same
                              // if so we can just share the restores
                              //
                              TR_BitVector *temp1 = _restoreInfo[blockNum];
                              *temp1 &= *_preservedRegsInLinkage;
                              TR_BitVector *temp2 = _restoreInfo[predBlockNum];
                              *temp2 &= *_preservedRegsInLinkage;
                              if (_traceSW)
                                 {
                                 traceMsg(comp(), "bv for block_%d and pred %d\n", blockNum, predBlockNum);
                                 temp1->print(comp());
                                 traceMsg(comp(), "\n");
                                 temp2->print(comp());
                                 traceMsg(comp(), "\n");
                                 }
                              //FIXME: org/w3c/tidy/PPrint.printTree
                              // can a fall-through that needs a restore also share?
                              // look at java/lang/ThreadGroup.remove(Ljava/lang/Thread;)V in jitt on x86-32
                              //
                              //HACKIT
                              if (_traceSW)
                                 traceMsg(comp(), "pei->restoreSplit %p\n", pei->_restoreSplitLocation);
                              if ((*temp1 == *temp2) &&
                                    pei->_savedRegs->isEmpty() &&
                                    !isFallThrough)
                                 {
                                 if (_traceSW)
                                    traceMsg(comp(), "no need to split between block_%d and %d\n", predBlockNum, blockNum);
                                 restoreLocation = pei->_restoreSplitLocation;
                                 ei = pei;
                                 // HACKIT
                                 if (0 && isFallThrough)
                                    {
                                    printf("doing the stuff in %s\n", comp()->signature());fflush(stdout);
                                    // if a fall-through is going to share, get rid of the preceeding jmp
                                    TR::Instruction *jmpInstr = pei->_restoreSplitLocation->getPrev();
                                    TR::Instruction *prev = jmpInstr->getPrev();
                                    prev->setNext(pei->_restoreSplitLocation);
                                    pei->_restoreSplitLocation->setPrev(prev);
                                    }
                                 break;
                                 }
                              }
                           }
                        }

                     // no info has been found and cannot be shared
                     // with another edge, so create a new one
                     //
                     if (!ei)
                        {
                        ei = new (trStackMemory()) SWEdgeInfo;
                        ei->_edge = (*e);
                        ei->_restoreSplitLocation = restoreLocation;
                        ei->_saveSplitLocation = NULL;
                        ei->_savedRegs = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
                        ei->_restoredRegs = new (trStackMemory()) TR_BitVector(numRegs, trMemory(), stackAlloc);
                        ei->_restoredRegs->empty();
                        ei->_savedRegs->empty();
                        _swEdgeInfo.add(ei);
                        }

                     if (!ei->_restoreSplitLocation)
                        {
                        // HACKIT
                        bool needsJump = true;
                        TR::Block *prevBlock = succBlock->getPrevBlock();
                        if (!isPredecessor(prevBlock, succBlock) && !blocksSplitSoFar->get(succBlockNum))
                           {
                           ///printf("found restore suboptimality in %s\n", comp()->signature());fflush(stdout);
                           if (_traceSW)
                              traceMsg(comp(), "no restore jump needed between %d and %d\n", prevBlock->getNumber(), succBlock->getNumber());
                           needsJump = false;
                           }

                        TR::list<TR::Instruction*> jmpInstrs(getTypedAllocator<TR::Instruction*>(TR::comp()->allocator()));
                        if (!isFallThrough)
                           {
                           cursor = findJumpInstructionsInBlock(blockNum, &jmpInstrs);
                           }
                        else
                           cursor = _swBlockInfo[blockNum]._endInstr;
                        TR_ASSERT(isFallThrough || comp()->cg()->isBranchInstruction(cursor), "cursor [%p] is not branch before split edge\n", cursor);
                        cursor = comp()->cg()->splitEdge(cursor, isFallThrough, needsJump, NULL, &jmpInstrs, true);
                        ei->_restoreSplitLocation = cursor;
                        splitBlocks->set(succBlockNum);
                        blocksSplitSoFar->set(succBlockNum);
                        }
                     else
                        {
                        // if this register has already been restored
                        // at this split, do not restore again
                        //
                        if (_traceSW)
                           traceMsg(comp(), "restoreLocation %p restored %d\n", restoreLocation, ei->_restoredRegs->get(regIndex));
                        if (restoreLocation && ei->_restoredRegs->get(regIndex))
                           {
                           doRestore = false;
                           }
                        cursor = ei->_restoreSplitLocation;
                        }

                     if (doRestore)
                        {
                        traceMsg(comp(), "         O^O Restoring register %s [%d] for block [%d] with instr [%p] at successor block (split) [%d] with stack offset [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, blockNum, cursor, succBlockNum, _mapRegsToStack[regIndex]);
                        comp()->cg()->getLinkage()->restorePreservedRegister(cursor, regIndex, _mapRegsToStack[regIndex]);
                        ei->_restoredRegs->set(regIndex);
                        }
                     ///else
                        {
                        traceMsg(comp(), "         O^O Not restoring register again %s [%d] for block [%d] at successor block (split) [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, blockNum, succBlockNum);
                        // make sure the jmp instruction in this block now points to the
                        // new restore location
                        //
                        if (!isFallThrough)
                           {
                           TR::Instruction *newSplitLocation = cursor;
                           cursor = _swBlockInfo[blockNum]._endInstr;
                           TR::list<TR::Instruction*> jmpInstrs(getTypedAllocator<TR::Instruction*>(TR::comp()->allocator()));

                           cursor = findJumpInstructionsInBlock(blockNum, &jmpInstrs);
                           cursor = comp()->cg()->splitEdge(cursor, isFallThrough, false, newSplitLocation, &jmpInstrs);
                           }
                        }
                     }
                  else // this is not possible according to the data flow solutions
                     {
                     cursor = _swBlockInfo[succBlockNum]._startInstr;
                     traceMsg(comp(), "         O^O Restoring register %s [%d] for block [%d] with instr [%p] at successor block [%d] with stack offset [%d]\n", comp()->getDebug()->getRealRegisterName(regIndex-1), regIndex, blockNum, cursor, succBlockNum, _mapRegsToStack[regIndex]);
                     comp()->cg()->getLinkage()->restorePreservedRegister(cursor, regIndex, _mapRegsToStack[regIndex]);
                     }
                  }
               else
                  ; // do nothing as the successor will take care of restoring the register
               }
            }
         isWrapped = true;
         }
      }

   // now iterate over the blocks that end in a return
   // and make sure we restore the required regs
   // NOTE: on ia32, the pop sequence must be in the
   // increasing order (because the pushes were done in
   // decreasing order)
   //
   if (1)
      {
#if 0
      // preserved regs used in this method
      int32_t numPreservedRegsInMethod = _preservedRegsInMethod->elementCount();
      // number of regs restored in epilogue
      int32_t numRegsRestored = 0;
      if (comp()->cg()->getPreservedRegsInPrologue())
         {
         TR_BitVector *temp = comp()->cg()->getPreservedRegsInPrologue();
         *temp &= *_preservedRegsInMethod;
         ///numRegsRestored = comp()->cg()->getPreservedRegsInPrologue()->elementCount();
         numRegsRestored = temp->elementCount();
         }

      traceMsg(comp(), "numPreservedRegsInMethod %d numRegsRestored %d\n", numPreservedRegsInMethod, numRegsRestored);
      int32_t allocSize = numPreservedRegsInMethod - numRegsRestored;
#endif

      for (ReturnBlockInfo *r = _returnBlocks.getFirst(); r; r = r->getNext())
         {
         int32_t blockNum = r->_blockNum;
         if (_traceSW)
            traceMsg(comp(), "fixing return block_%d\n", blockNum);

         // look for the return instruction
         //
         TR::Instruction *retInstr = findReturnInBlock(blockNum); // is RET

         if (r->_restoresRegisters)
            {
            TR::Instruction *cursor = retInstr->getPrev();
            _swBlockInfo[blockNum]._restoreLocation = cursor;

            // iterate over the restore info (which is in
            // increasing order of preserved regs)
            //
            TR_BitVectorIterator resIt(*_restoreInfo[blockNum]);
            while (resIt.hasMoreElements())
               {
               int32_t regIndex = resIt.getNextElement();
               if (findPreservedRegisterInfo(regIndex) &&
                     !comp()->cg()->getPreservedRegsInPrologue()->get(regIndex))
                  {
                  if (_traceSW)
                     traceMsg(comp(), "restoring register %d at %p\n", regIndex, cursor);

                  int32_t offset = _linkageUsesPushes ? -1 :_mapRegsToStack[regIndex];
                  comp()->cg()->getLinkage()->restorePreservedRegister(cursor, regIndex, offset);
                  r->_numRestoredRegisters++;
                  cursor = cursor->getNext();
                  }
               }
            }

#if 0
         // if the linkage has determined that the frame allocation size is 0,
         // then we need to explicity deallocate the frame before the returns
         // as it will not be done by the epilogue code (this can happen on ia32
         // for example, when no locals are used)
         //
         if (comp()->cg()->getLinkage()->needsFrameDeallocation())
            {
            int32_t tempSize = allocSize - r->_numRestoredRegisters;
            if (_traceSW)
               traceMsg(comp(), "adding instr to deallocate frame %d at %p\n", tempSize, retInstr);
            // add instructions to deallocate the frame
            //
            if (tempSize > 0)
               {
               ////comp()->cg()->getLinkage()->deallocateFrameIfNeeded(retInstr->getPrev(), tempSize);
               TR_Pair<TR::Instruction, int32_t> *retInfo = new (trHeapMemory()) TR_Pair<TR::Instruction, int32_t> (retInstr, (int32_t *)tempSize);
               comp()->cg()->getReturnInstructionsForShrinkWrapping().add(retInfo);
               }
            }
#endif
         }
      }

   // this bool is needed so that we can piggy back
   // on the registerSaveDescription field in the metadata
   // this field is then checked in the stackwalker code to
   // determine the right saveDescription at every pc
   //
   if (isWrapped)
      {
      comp()->cg()->setShrinkWrappingDone();
      ///printf("-- shrinkWrapping in %s --\n", comp()->signature());fflush(stdout);
      }

   // try to pack saves/restores by peepholing
   // the save/restore sequence
   //
   if (comp()->cg()->getUsesLoadStoreMultiple())
      composeSavesRestores();

   // print out the registers that need to be saved in the prologue
   //
   if (_traceSW &&
         comp()->cg()->getPreservedRegsInPrologue())
      {
      //comp()->cg()->getPreservedRegsInPrologue()->setAll(TR::RealRegister::NumRegisters);
      traceMsg(comp(), "These regs are saved (restored) in prologue (epilogue): ");
      comp()->cg()->getPreservedRegsInPrologue()->print(comp());
      traceMsg(comp(), "\n");
      }
   }

void TR_ShrinkWrap::composeSavesRestores()
   {
   static char *pEnv = feGetEnv("TR_pCompose");
   static char *pEnv2 = feGetEnv("TR_pComposeSplitSaves");
   static char *pEnv3 = feGetEnv("TR_pComposeSplitRestores");
   static char *pEnv4 = feGetEnv("TR_pComposeBlockSaves");
   static char *pEnv5 = feGetEnv("TR_pComposeBlockRestores");
   if (!pEnv)
      return;
   // cycle through all the split edges
   //
   for (SWEdgeInfo *cur = _swEdgeInfo.getFirst(); cur; cur = cur->getNext())
      {
      // along a single edge, if there are
      // saves and restores, then the restores
      // would have been placed before the saves
      // in sequence
      //
      // do the saves first. this is important because we need to skip
      // over the restores
      //
      if (pEnv2 && cur->_savedRegs)
         {
         traceMsg(comp(), "trying to use smg for saves along edge %d %d\n", cur->_edge->getFrom()->getNumber(), cur->_edge->getTo()->getNumber());
         TR::Instruction *loc = cur->_restoreSplitLocation;
         if (cur->_restoredRegs)
            {
            int32_t numRegs = cur->_restoredRegs->elementCount();
            for (int32_t i = 0; i < numRegs; i++) loc = loc->getNext();
            }
         findMultiples(cur->_savedRegs, loc, true);
         }

      // handle all the restores
      //
      if (pEnv3 && cur->_restoredRegs)
         {
         traceMsg(comp(), "trying to use lmg for restores along edge %d %d\n", cur->_edge->getFrom()->getNumber(), cur->_edge->getTo()->getNumber());
         findMultiples(cur->_restoredRegs, cur->_restoreSplitLocation, false);
         }
      }

   // handle all the stores/loads that were done at the beginning/end of blocks
   //
   for (int32_t i = 0; i < _numberOfNodes; i++)
      {
      if (pEnv4 && _swBlockInfo[i]._savedRegs)
         {
         traceMsg(comp(), "trying to use smg for saves in block_%d\n", i);
         findMultiples(_swBlockInfo[i]._savedRegs, _swBlockInfo[i]._startInstr, true);
         }
      if (pEnv5 && _swBlockInfo[i]._restoredRegs)
         {
         traceMsg(comp(), "trying to use lmg for restores in block_%d\n", i);
         bool forwardWalk = findReturnBlockInfo(i) ? true : false;
         if (!forwardWalk)
            findMultiples(_swBlockInfo[i]._restoredRegs, _swBlockInfo[i]._restoreLocation, false, forwardWalk);
         }
      }
   }

bool TR_ShrinkWrap::findMultiples(TR_BitVector *regs, TR::Instruction *startLocation, bool doSaves, bool forwardWalk)
   {
   int32_t numRegs = regs->elementCount();
   traceMsg(comp(), "find multiples startLocation %p numregs %d\n", startLocation, numRegs);
   traceMsg(comp(), "regs are: ");
   regs->print(comp());
   traceMsg(comp(), "\n");
   if (numRegs > 1)
      {
      // determine if we can use load/store multiple
      // the registers need to be in sequence
      //
      int32_t startIdx = -1;
      int32_t endIdx = -1;
      int32_t prevIdx = -1;
      TR::Instruction *loc = startLocation;
      TR::Instruction *lmgInstrs = loc;
      if (!forwardWalk)
         lmgInstrs = loc->getPrev();
      int32_t i = 0;

      TR_BitVectorIterator resIt(*regs);
      while (resIt.hasMoreElements())
         {
         int32_t curIdx = resIt.getNextElement();
         if (prevIdx != -1)
            {
            if (curIdx == prevIdx+1)
               {
               if (startIdx == -1) startIdx = prevIdx; // new pattern
               endIdx = curIdx;
               }
            else
               {
               // pattern broken, so insert a load/store multiple
               // for the regs upto this point and remove any
               // individual loads/stores
               //
               if (i > 1)
                  {
                  traceMsg(comp(), "found pattern at %p firstReg %d lastReg %d\n", lmgInstrs, startIdx, endIdx);
                  traceMsg(comp(), "startLocation %p offset %d\n", startLocation, _mapRegsToStack[startIdx]);
                  lmgInstrs = comp()->cg()->getLinkage()->composeSavesRestores(lmgInstrs, startIdx, endIdx, _mapRegsToStack[startIdx], numRegs, doSaves);
                  if (forwardWalk)
                     {
                     lmgInstrs->setNext(loc->getNext());
                     if (loc->getNext())
                        loc->getNext()->setPrev(lmgInstrs);
                     //lmgInstrs = lmgInstrs->getNext();
                     }
                  else
                     {
                     lmgInstrs->setPrev(loc->getPrev());
                     if (loc->getPrev())
                        loc->getPrev()->setNext(lmgInstrs);
                     lmgInstrs = lmgInstrs->getPrev();
                     }
                  }
               else
                  {
                  lmgInstrs = loc;
                  if (!forwardWalk)
                     lmgInstrs = loc->getPrev();
                  }
               startIdx = -1;
               endIdx = -1;
               i = 0;
               }
            }
         else
            startIdx = curIdx;
         i++;
         if (forwardWalk)
            loc = loc->getNext();
         else
            loc = loc->getPrev();
         prevIdx = curIdx;
         }

      // do all the remaining
      if (startIdx != -1)
         {
         traceMsg(comp(), "found remaining pattern at %p firstReg %d lastReg %d\n", lmgInstrs, startIdx, endIdx);
         traceMsg(comp(), "startLocation %p offset %d\n", startLocation, _mapRegsToStack[startIdx]);
         lmgInstrs = comp()->cg()->getLinkage()->composeSavesRestores(lmgInstrs, startIdx, endIdx, _mapRegsToStack[startIdx], numRegs, doSaves);
         if (forwardWalk)
            {
            lmgInstrs->setNext(loc->getNext());
            if (loc->getNext())
               loc->getNext()->setPrev(lmgInstrs);
            }
         else
            {
            lmgInstrs->setPrev(loc->getPrev());
            if (loc->getPrev())
               loc->getPrev()->setNext(lmgInstrs);
            }
         }
      return true;
      }
   return false;
   }


PreservedRegisterInfo *TR_ShrinkWrap::findPreservedRegisterInfo(int32_t regIndex)
   {
   for (PreservedRegisterInfo *cur = _preservedRegs.getFirst(); cur; cur = cur->getNext())
      {
      if (cur->_index == regIndex)
         return cur;
      }
   return NULL;
   }

ReturnBlockInfo *TR_ShrinkWrap::findReturnBlockInfo(int32_t blockNum)
   {
   for (ReturnBlockInfo *cur = _returnBlocks.getFirst(); cur; cur = cur->getNext())
      {
      if (cur->_blockNum == blockNum)
         return cur;
      }
   return NULL;
   }

SWEdgeInfo *TR_ShrinkWrap::findEdgeInfo(TR::CFGEdge *e)
   {
   for (SWEdgeInfo *cur = _swEdgeInfo.getFirst(); cur; cur = cur->getNext())
      {
      if (cur->_edge == e)
         return cur;
      }
   return NULL;
   }

int32_t TR_ShrinkWrap::updateMapWithRSD(TR::Instruction *instr, int32_t rsd)
   {
   int32_t value = -1;
   if (instr->needsGCMap())
      {
      TR_GCStackMap *map = instr->getGCMap();
      if (map)
         {
         ///traceMsg(comp(), "instr %p rsd %x map %p\n", instr, rsd, map);
         map->setRegisterSaveDescription(rsd);
         value = 0; // found some map
         }
      }

   if (instr->getSnippetForGC())
      {
      TR::Snippet * snippet = instr->getSnippetForGC();
      if (snippet && snippet->gcMap().isGCSafePoint() && snippet->gcMap().getStackMap())
         {
         TR_GCStackMap *map = snippet->gcMap().getStackMap();
         ///traceMsg(comp(), "instr %p rsd %x snippet map %p\n", instr, rsd, map);
         map->setRegisterSaveDescription(rsd);
         value = 1; // to indicate a snippet
         }
      }

   return value;
   }

void TR_ShrinkWrap::markInstrsInBlock(int32_t blockNum)
   {
   TR::Block *block = _swBlockInfo[blockNum]._block;

   int32_t rsd = comp()->cg()->computeRegisterSaveDescription(block->getRegisterSaveDescriptionBits());
   ///traceMsg(comp(), "got rsd %x for block_%d\n", rsd, blockNum);
   // build the registerSaveDescription on the instrs
   // in this block
   //
   TR::Instruction *cur = _swBlockInfo[blockNum]._startInstr;
   TR::Instruction *end = _swBlockInfo[blockNum]._endInstr;

   ///traceMsg(comp(), "start %p end %p\n", cur, end);
   while (cur != end)
      {
      int32_t mightHaveSnippet = updateMapWithRSD(cur, rsd);
      // check for branches to snippets
      //
      ///traceMsg(comp(), "cur %p mightHaveSnippet %d\n", cur, mightHaveSnippet);
      if ((mightHaveSnippet != 1) &&
            comp()->cg()->isBranchInstruction(cur))
         comp()->cg()->updateSnippetMapWithRSD(cur, rsd);

      cur = cur->getNext();
      }
   }

// if this block has an edge to the exit block but does not
// end in return
//
TR_YesNoMaybe TR_ShrinkWrap::blockEndsInReturn(int32_t blockNum, bool &hasExceptionSuccs)
   {
   TR::Block *exitBlock = toBlock(_cfg->getEnd());

   bool hasExitEdge = false;
   for (auto e = exitBlock->getPredecessors().begin(); e != exitBlock->getPredecessors().end(); ++e)
      if ((*e)->getFrom()->getNumber() == blockNum)
         {
         hasExitEdge = true;
         break;
         }

   if (!hasExitEdge)
      return TR_maybe;

   // this is a block which is exiting the method
   //
   TR::Instruction *start = _swBlockInfo[blockNum]._startInstr;
   TR::Instruction *end = _swBlockInfo[blockNum]._endInstr;

   while (end != start)
      {
      if (comp()->cg()->isReturnInstruction(end))
         return TR_yes;
      end = end->getPrev();
      }

   // could be a block that has exception successors
   //
   TR::Block *curBlock = _swBlockInfo[blockNum]._block;
   if (hasExitEdge)
      {
      if (!curBlock->getExceptionSuccessors().empty())
         {
         hasExceptionSuccs = true;
         return TR_maybe;
         }
      }

   return TR_no;
   }

const char *
TR_ShrinkWrap::optDetailString() const throw()
   {
   return "O^O SHRINK WRAPPING: ";
   }
