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

#ifdef OMRZTPF
#define __TPF_DO_NOT_MAP_ATOE_REMOVE
#endif

#include "optimizer/RegisterCandidate.hpp"

#include <algorithm>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "cs2/hashtab.h"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Checklist.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "ras/Debug.hpp"


//TODO:GRA: if we are going to have two versions of GRA one with Meta Data and one without then we can do someting here
// for different compilation level then we can have two version of this, one using CPIndex and one  (i.e. with MetaData)
// using getReferenceNumber
#define GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(S) (S)->getReferenceNumber()
#define CANDIDATE_FOR_SYMREF_SIZE (comp()->getSymRefCount()+1)

#define OPT_DETAILS "O^O GLOBAL REGISTER ASSIGNER: "

//static TR_BitVector *blocksVisited = NULL;

// Duplicated in GlobalRegisterAllocator.cpp. TODO try and factor this out
#define keepAllStores false

TR::GlobalSet::GlobalSet(TR::Compilation * comp, TR::Region &region)
   :_region(region),
    _blocksPerAuto((RefMapComparator()),(RefMapAllocator(region))),
    _EMPTY(region),
    _collected(false),
    _comp(comp)
   {
   }

void TR::GlobalSet::collectBlocks()
   {
   TR::StackMemoryRegion stackMemoryRegion(*_comp->trMemory());

   TR_BitVector references(0, _comp->trMemory(), stackAlloc);
   TR_BitVectorIterator bvi(references);
   TR::NodeChecklist visited(_comp);

   for (TR::CFGNode *node = _comp->getFlowGraph()->getFirstNode(); node; node = node->getNext())
      {
      TR::Block *block = toBlock(node);
      if (!block)
         continue;

      // Collect all autos/parms used in this block
      references.empty();
      visited.remove(visited);
      for (TR::TreeTop * tt = block->getEntry(); tt && tt != block->getExit(); tt = tt->getNextTreeTop())
         collectReferencedAutoSymRefs(tt->getNode(), references, visited);

      // Set this block as referencing the collected autos/params
      // Also set any blocks that extend this one
      bvi.setBitVector(references);
      while (bvi.hasMoreElements())
         {
         uint32_t symRefNum = bvi.getNextElement();
         auto lookup = _blocksPerAuto.find(symRefNum);
         if (lookup != _blocksPerAuto.end())
            lookup->second->set(block->getNumber());
         else
            {
            TR_BitVector *blocks = new (_region) TR_BitVector(_region);
            blocks->set(block->getNumber());
            _blocksPerAuto[symRefNum] = blocks;
            }
         }
      }

   _collected = true;
   }

void TR::GlobalSet::collectReferencedAutoSymRefs(TR::Node *node, TR_BitVector &referencedAutoSymRefs, TR::NodeChecklist &visited)
   {
   if (visited.contains(node))
      return;
   visited.add(node);

   if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isAutoOrParm())
      referencedAutoSymRefs.set(node->getSymbolReference()->getReferenceNumber());

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      collectReferencedAutoSymRefs(node->getChild(i), referencedAutoSymRefs, visited);
   }

// Duplicated in GlobalRegisterAllocator.cpp. TODO try and factor this out
static bool dontAssignInColdBlocks(TR::Compilation *comp) { return comp->getMethodHotness() >= hot; }

// Duplicated in GlobalRegisterAllocator.cpp. TODO try and factor this out
// For both switch/table instructions and igoto instructions, the
// same sort of processing has to be done for each successor block.
// These classes are meant to allow that work to be independent of
// the way that the successor blocks are actually identified.

int32_t TR_RegisterCandidates::_candidateTypeWeights[TR_NumRegisterCandidateTypes] =
   {
   1,        // TR_InductionVariableCandidate
   1 // 10        // TR_PRECandidate
   };

TR_RegisterCandidate::TR_RegisterCandidate(TR::SymbolReference * sr, TR::Region &r)
   : _symRef(sr), _splitSymRef(NULL), _restoreSymRef(NULL), _lowRegNumber(-1), _highRegNumber(-1),
     _liveOnEntry(), _liveOnExit(), _originalLiveOnEntry(),
     _reprioritized(0),
     _blocks(r),
     _loopExitBlocks(r),
     _stores(r),
     _loopsWithHoles(r),
     _mostRecentValue(NULL),
     _lastLoad(NULL)
   {
   }

TR::DataType
TR_RegisterCandidate::getType()
   {
   return getSymbol()->getType();
   }
TR::DataType
TR_RegisterCandidate::getDataType()
   {
   TR::Symbol *rcSymbol = getSymbol();
   TR::DataType dtype = rcSymbol->getDataType();

   return dtype;
   }

TR_RegisterKinds
TR_RegisterCandidate::getRegisterKinds()
  {
  TR::DataType dt = getDataType();
  if(dt == TR::Float
     || dt == TR::Double
#ifdef J9_PROJECT_SPECIFIC
     || dt == TR::DecimalFloat
     || dt == TR::DecimalDouble
     || dt == TR::DecimalLongDouble
#endif
     )
    return TR_FPR;
  else if (dt.isVector())
    return TR_VRF;
  else
    return TR_GPR;
  }

void TR_RegisterCandidate::addAllBlocksInStructure(TR_Structure *structure, TR::Compilation *comp, const char *description, vcount_t count, bool recursiveCall)
   {
   if (!recursiveCall)
      count = comp->incVisitCount();

   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();

      addBlock(block, 0);

      if (description)
         traceMsg(comp, "\nAdded %s #%d (symRef %p) as global reg candidate in block_%d\n", description, getSymbolReference()->getReferenceNumber(), getSymbolReference(), block->getNumber());
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;

      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         addAllBlocksInStructure(node->getStructure(), comp, description, count, true);
      }
   }









TR_RegisterCandidates::TR_RegisterCandidates(TR::Compilation *comp)
  : _compilation(comp), _trMemory(comp->trMemory()), _candidateRegion(_trMemory->heapMemoryRegion()),
    _referencedAutoSymRefsInBlock(NULL)
   {
   _candidateForSymRefs = 0;
   }

 TR_RegisterCandidate *TR_RegisterCandidates::newCandidate(TR::SymbolReference *ref) {
   return new (_candidateRegion) TR_RegisterCandidate(ref, _candidateRegion);
 }

TR_RegisterCandidate *
TR_RegisterCandidates::findOrCreate(TR::SymbolReference * symRef)
   {
   TR_RegisterCandidate * rc;
   if ((rc = find(symRef)))
      {
      if (_candidateForSymRefs)
         {
         (*_candidateForSymRefs)[GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(symRef)] = rc;
         }

      return rc;
      }
   _candidates.add(rc = newCandidate(symRef));
   if (_candidateForSymRefs)
      (*_candidateForSymRefs)[GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(symRef)] = rc;

   TR_ASSERT(comp()->cg()->considerTypeForGRA(symRef),"should create a registerCandidate for symRef #%d with datatype %s\n",symRef->getReferenceNumber(),rc->getDataType().toString());

   return rc;
   }

TR_RegisterCandidate *
TR_RegisterCandidates::find(TR::SymbolReference * symRef)
   {
   if (symRef->getSymbol()->isAutoOrParm())
      {
      TR_RegisterCandidate* rc= (_candidateForSymRefs) ? (*_candidateForSymRefs)[GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(symRef)] : NULL;
      // two symrefs can point to same symbols so it is possible that we have an rc for symref->getSymbol but not have it added to the cache
      if(rc==NULL)
         {
         rc = find(symRef->getSymbol());
         if (_candidateForSymRefs)
            (*_candidateForSymRefs)[GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(symRef)] = rc;
         }
         return rc;
      }
   else
      return NULL;
   }


TR_RegisterCandidate *
TR_RegisterCandidates::find(TR::Symbol* sym)
   {
   TR_RegisterCandidate * rc;
   for (rc = _candidates.getFirst(); rc; rc = rc->getNext())
      if (rc->getSymbolReference()->getSymbol() == sym)
         return rc;

   return NULL;
   }




bool
TR_RegisterCandidate::find(TR::Block * b)
   {
   return _blocks.find(b->getNumber());
   }

void
TR_RegisterCandidate::addBlock(TR::Block * block, int32_t numberOfLoadsAndStores)
   {
   _blocks.incNumberOfLoadsAndStores(block->getNumber(), numberOfLoadsAndStores);
   }

int32_t
TR_RegisterCandidate::removeBlock(TR::Block * block)
   {
   if (find(block))
      {
      int32_t numLoadsAndStores = _blocks.getNumberOfLoadsAndStores(block->getNumber());
      _blocks.removeBlock(block->getNumber());
      return numLoadsAndStores;
      }
   return 0;
   }


void TR_RegisterCandidate::removeLoopExitBlock(TR::Block *b)
   {
   _loopExitBlocks.remove(b);
   }


void TR_RegisterCandidate::addLoopExitBlock(TR::Block *b)
   {
   if (!_loopExitBlocks.find(b))
      _loopExitBlocks.add(b);
   }


bool TR_RegisterCandidate::hasLoopExitBlock(TR::Block * b)
   {
   return (_loopExitBlocks.find(b) != 0);
   }


int32_t
TR_RegisterCandidate::countNumberOfLoadsAndStoresInBlocks(List<TR::Block> *blocksInLoop)
   {
   int32_t numLoadsAndStores = 0;
   ListIterator<TR::Block> blocksIt(blocksInLoop);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
          numLoadsAndStores += _blocks.getNumberOfLoadsAndStores(nextBlock->getNumber());
      }
   return numLoadsAndStores;
   }

#ifdef TRIM_ASSIGNED_CANDIDATES
// The following are methods for maintaining info about number of candidate's loads
// and stores per loop (vs. per block)
TR_RegisterCandidate::LoopInfo *
TR_RegisterCandidate::find(TR_Structure * l)
   {
   for (LoopInfo * li = _loops.getFirst(); li; li = li->getNext())
      if (li->_loop == l)
         return li;
   return 0;
   }

void
TR_RegisterCandidate::printLoopInfo(TR::Compilation *comp)
   {
   for (LoopInfo * li = _loops.getFirst(); li; li = li->getNext())
      {
      traceMsg(comp, "Candidate #%d has %d loads and stores in loop %d\n",
                     getSymbolReference()->getReferenceNumber(),
                     li->_numberOfLoadsAndStores,
                     li->_loop->getNumber());
      }
   }

void
TR_RegisterCandidate::addLoop(TR_Structure * loop, int32_t numberOfLoadsAndStores, TR_Memory * m)
   {
   LoopInfo * li = find(loop);
   if (li)
      li->_numberOfLoadsAndStores += numberOfLoadsAndStores;
   else
      _loops.add(new (m->trHeapMemory()) LoopInfo(loop, numberOfLoadsAndStores));
   }


// Reduce block info into loop info by counting loads and stores in loops
void
TR_RegisterCandidate::countNumberOfLoadsAndStoresInLoops(TR_Memory * m)
   {
   for (BlockInfo * bi = _blocks.getFirst(); bi; bi = bi->getNext())
      {
      TR_BlockStructure *blockStructure = bi->_block->getStructureOf();

      if (blockStructure)
         {
         TR_Structure *containingLoop = blockStructure->getContainingLoop();
         if (containingLoop)
            addLoop(containingLoop, bi->_numberOfLoadsAndStores, m);
         }
      }
   }

int32_t
TR_RegisterCandidate::getNumberOfLoadsAndStoresInLoop(TR_Structure *loop)
   {
   LoopInfo * li = find(loop);
   if (li)
      return li->_numberOfLoadsAndStores;
   else
      return 0;
   }

bool isInnerLoop(TR::Compilation *comp, TR_Structure *loop)
   {
   List<TR::Block> blocksInLoop(comp->trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> blocksIt(&blocksInLoop);
   TR::Block *nextBlock;
   blocksIt.reset();
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      if (nextBlock->getStructureOf()->getContainingLoop() != loop)
         return false;
      }
   return true;
   }

void
TR_RegisterCandidate::removeAssignedCandidateFromLoop(TR::Compilation *comp, TR_Structure *loop, int32_t regNum, TR_BitVector *liveOnEntryUsage, TR_BitVector *liveOnExitUsage, bool trace)
   {
   List<TR::Block> blocksInLoop(comp->trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> blocksIt(&blocksInLoop);
   TR::Block *nextBlock;
   blocksIt.reset();
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      int32_t blockNumber = nextBlock->getNumber();
      _liveOnEntry.reset(blockNumber);
      _liveOnExit.reset(blockNumber);
      liveOnEntryUsage->reset(blockNumber);
      liveOnExitUsage->reset(blockNumber);

      if (trace)
         traceMsg(comp, "Removed candidate #%d from block_%d entry and exit\n",
                        getSymbolReference()->getReferenceNumber(), blockNumber);


      nextBlock->getGlobalRegisters(comp)[regNum].setRegisterCandidateOnEntry(NULL);
      nextBlock->getGlobalRegisters(comp)[regNum].setRegisterCandidateOnExit(NULL);

       // handle predecessors
      for (auto pred = nextBlock->getPredecessors().begin(); pred != nextBlock->getPredecessors().end(); ++pred)
         {
         TR::Block *block = toBlock((*pred)->getFrom());
         _liveOnExit.reset(block->getNumber());
         liveOnExitUsage->reset(block->getNumber());
         block->getGlobalRegisters(comp)[regNum].setRegisterCandidateOnExit(NULL);
         }

       // handle successors
      for (auto succ = nextBlock->getSuccessors().begin(); succ != nextBlock->getSuccessors().end(); ++succ)
         {
         TR::Block *block = toBlock((*succ)->getTo());
         _liveOnEntry.reset(block->getNumber());
         liveOnEntryUsage->reset(block->getNumber());
         block->getGlobalRegisters(comp)[regNum].setRegisterCandidateOnEntry(NULL);
         }

       }
   }
#endif

bool
TR_RegisterCandidate::symbolIsLive(TR::Block * block)
   {
   TR_BitVector * liveLocals = block->getLiveLocals();
   if (!liveLocals)
      return true; // can be a new block

   if (!getSymbol()->getAutoSymbol())
      {
      return true;
      }

   if (!getSymbol()->isAutoOrParm())
      {
      return true;
      }

   TR::RegisterMappedSymbol * regSym = getSymbol()->getRegisterMappedSymbol();
   return liveLocals->get(regSym->getLiveLocalIndex()) != 0;
   }


bool findStoreNearEndOfBlock(TR::Block *block, TR::SymbolReference *ref)
   {
   TR::Node * node = block->getLastRealTreeTop()->getNode();

   if (node->getOpCode().isStore())
      return (node->getSymbolReference() == ref);
   else if (block->getLastRealTreeTop()->getPrevTreeTop())
      {
      node = block->getLastRealTreeTop()->getPrevTreeTop()->getNode();
      return  (node->getOpCode().isStore() && (node->getSymbolReference()) == ref);
      }
   return false;
   }

bool findLoad(TR::Node *node, TR::SymbolReference *ref)
   {
   if (node->getOpCode().isLoad() &&
       node->getSymbolReference() == ref)
     return true;

   return false; // simplify for now to avoid timeout in SPEC

   for (int i = 0; i < node->getNumChildren(); i++)
      {
      if (findLoad(node->getChild(i), ref))
         return true;
      }
   return false;
   }

bool findLoadNearStartOfBlock(TR::Block *block, TR::SymbolReference *ref)
   {
   // vcount_t visitCount = comp->incVisitCount();
   return findLoad(block->getFirstRealTreeTop()->getNode(), ref);

   }

bool TR_RegisterCandidates::aliasesPreventAllocation(TR::Compilation *comp, TR::SymbolReference *symRef)
  {

  if (!symRef->getSymbol()->isAutoOrParm() && !(TR::Compiler->target.cpu.isZ() && TR::Compiler->target.isLinux()) ) return true;

  TR::SparseBitVector use_def_aliases(comp->allocator());
  symRef->getUseDefAliases(false).getAliases(use_def_aliases);
  if (!use_def_aliases.IsZero())
    {
    use_def_aliases[symRef->getReferenceNumber()] = false;
    if (!use_def_aliases.IsZero())
      return true;
    }

  return false;
  }

void
TR_RegisterCandidate::setWeight(TR::Block * * blocks, int32_t *blockStructureWeight, TR::Compilation *comp,
                                TR_Array<int32_t> & blockGPRCount,
                                TR_Array<int32_t> & blockFPRCount,
                                TR_Array<int32_t> & blockVRFCount,
                                TR_BitVector *referencedBlocks, TR_Array<TR::Block *>& startOfExtendedBBForBB,
                                TR_BitVector &firstBlocks,
                                TR_BitVector &isExtensionOfPreviousBlock)
   {
   LexicalTimer t("setWeight", comp->phaseTimer());

   TR::CFG * cfg = comp->getFlowGraph();
   int32_t numberOfBlocks = cfg->getNextNodeNumber();
   _liveOnEntry.init(numberOfBlocks, comp->trMemory(), stackAlloc, growable);
   _liveOnExit.init(numberOfBlocks, comp->trMemory(), stackAlloc, growable);
   _originalLiveOnEntry.init(numberOfBlocks, comp->trMemory(), stackAlloc, growable);

   TR::CodeGenerator * cg = comp->cg();
   BlockInfo::iterator itr = _blocks.getIterator();
   while (itr.hasMoreElements())
      {
      int32_t blockNumber = itr.getNextElement();
      TR_ASSERT(blockNumber < cfg->getNextNodeNumber(), "Overflow on candidate BB numbers");
      TR::Block * b = blocks[blockNumber];
      if (!b) continue;

      TR_ASSERT(blockNumber == b->getNumber(),"blocks[x]->getNumber() != x");
      TR_ASSERT((blockNumber < cfg->getNextNodeNumber()) && (blocks[blockNumber] == b),"blockNumber is wrong");

      int32_t blockWeight = _blocks.getNumberOfLoadsAndStores(blockNumber);
      bool firstBlock = firstBlocks.isSet(blockNumber);
      if ((firstBlock && isAllBlocks() && cg->getSupportsGlRegDepOnFirstBlock()) ||
           (!firstBlock && (((blockWeight == 0) && hasLoopExitBlock(b)) || symbolIsLive(b))))
         _liveOnEntry.set(blockNumber);

      TR_BlockStructure * blockStructure = b->getStructureOf();
      int32_t blockFreq = 1;
      if (blockStructure)
         {
         blockFreq = blockStructureWeight[blockNumber];
         }

      TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
      TR::Block * extendedBlock = startOfExtendedBBForBB[blockNumber];
      int32_t extendedBlockFreq = 1;
      TR_BlockStructure *extendedBlockStructure = extendedBlock->getStructureOf();
      if (extendedBlockStructure)
         {
         extendedBlockFreq = blockStructureWeight[extendedBlock->getNumber()];
         }

       // We should not mark a candidate as being referenced in the extended block if the
       // only references to it are in the portion of the extended block that is outside a loop
       // This is important as we cannot recognize that a candidate is not referenced at all in a loop
       // otherwise. We see a 'false' use in the extended basic block that started inside the loop because
       // of the portion of the extended basic block outside the loop.
       //
      if (extendedBlockFreq <= blockFreq)
         {
         if ((uint32_t)blockWeight > _blocks.getNumberOfLoadsAndStores(extendedBlock->getNumber()))
            _blocks.setNumberOfLoadsAndStores(extendedBlock->getNumber(), blockWeight);
         }
      }

   _originalLiveOnEntry |= _liveOnEntry;

   processLiveOnEntryBlocks(blocks, blockStructureWeight, comp,blockGPRCount,blockFPRCount, blockVRFCount, referencedBlocks, startOfExtendedBBForBB);

   }


void
TR_RegisterCandidate::recalculateWeight(TR::Block * * blocks, int32_t *blockStructureWeight, TR::Compilation *comp,
                                        TR_Array<int32_t> & blockGPRCount,
                                        TR_Array<int32_t> & blockFPRCount,
                                        TR_Array<int32_t> & blockVRFCount,
                                        TR_BitVector *referencedBlocks, TR_Array<TR::Block *>& startOfExtendedBBForBB)
   {
   getBlocksLiveOnExit().empty();
   _originalLiveOnEntry = _liveOnEntry;
   processLiveOnEntryBlocks(blocks, blockStructureWeight, comp,blockGPRCount,blockFPRCount, blockVRFCount, referencedBlocks, startOfExtendedBBForBB);
   }

// Look at exit live count of predecessor, and return true if there is too much
// register pressue
bool
TR_RegisterCandidate::prevBlockTooRegisterConstrained(TR::Compilation *comp, TR::Block *block,
                                                      TR_Array<int32_t> & blockGPRCount,
                                                      TR_Array<int32_t> & blockFPRCount,
                                                      TR_Array<int32_t> & blockVRFCount)
{
  static char * doit = feGetEnv("TR_SkipIfPrevBlockConstrained");
  int32_t blockNum = block->getNumber();
  if (doit != NULL) return true;
  if (_liveOnEntry.get(blockNum))
     {
     int32_t gprCount=0,fprCount=0,vrfCount=0;
     gprCount = blockGPRCount[blockNum];
     fprCount = blockFPRCount[blockNum];
     vrfCount = blockVRFCount[blockNum];
     //printf("block_%d: gpr:%d fpr:%d vrf:%d\n",blockNum,gprCount,fprCount,vrfCount);

     for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
        {
        TR::Block * pred = toBlock((*e)->getFrom());
        if (pred == comp->getFlowGraph()->getStart())
           continue;
        TR::Node * node = pred->getLastRealTreeTop()->getNode();
        //printf("looking at block_%d's  previous node %p\n",blockNum,node);
        int32_t maxGPRs= comp->cg()->getMaximumNumberOfGPRsAllowedAcrossEdge(pred);
        int32_t maxFPRs= comp->cg()->getMaximumNumberOfFPRsAllowedAcrossEdge(node);
        int32_t maxVRFs= comp->cg()->getMaximumNumberOfVRFsAllowedAcrossEdge(node);

        //dumpOptDetails(comp, " pred %d actual gprs %d max gprs %d ", pred->getNumber(), gprCount, maxGPRs);

        if (maxFPRs <= fprCount || maxGPRs <= gprCount || maxVRFs <= vrfCount) return true;
        }
    }
  //printf(".....no constraints\n");
  return false;
}


bool TR_RegisterCandidate::rcNeeds2Regs(TR::Compilation *comp)
   {
   if(getType().isAggregate())
      {
      if ( (TR::Compiler->target.is32Bit() && !comp->cg()->use64BitRegsOn32Bit() && getSymbol()->getSize() > 4) || (getSymbol()->getSize() > 8 ) )
         return true;
      else
         return false;
      }
   else
      return ((getType().isInt64() && TR::Compiler->target.is32Bit() && !comp->cg()->use64BitRegsOn32Bit())
#ifdef J9_PROJECT_SPECIFIC
              || getType().isLongDouble()
#endif
              );
   }

bool
TR_RegisterCandidate::hasSameGlobalRegisterNumberAs(TR::Node *node, TR::Compilation *comp)
   {
   if (node->requiresRegisterPair(comp))
      return (getLowGlobalRegisterNumber() == node->getLowGlobalRegisterNumber())
          && (getHighGlobalRegisterNumber() == node->getHighGlobalRegisterNumber());
   else
      return getGlobalRegisterNumber() == node->getGlobalRegisterNumber();
   }


void
TR_RegisterCandidate::processLiveOnEntryBlocks(TR::Block * * blocks, int32_t *blockStructureWeight, TR::Compilation *comp,
                                               TR_Array<int32_t> & blockGPRCount,
                                               TR_Array<int32_t> & blockFPRCount,
                                               TR_Array<int32_t> & blockVRFCount,
                                               TR_BitVector *referencedBlocks,
                                               TR_Array<TR::Block *>& startOfExtendedBBForBB, bool callToRemoveUnusedLoops)
   {
   LexicalTimer t("processLiveOnEntryBlocks", comp->phaseTimer());

   TR::SymbolReference *symRef = getSymbolReference();
   TR::Symbol * sym = getSymbolReference()->getSymbol();
   bool trace = comp->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);

   bool removeUnusedLoops = callToRemoveUnusedLoops;
   bool useProfilingFrequencies = comp->getUsesBlockFrequencyInGRA();

   double freqRatio;
   if(useProfilingFrequencies)
      freqRatio = 0.7;
   else
      freqRatio = 1.0;


   if (comp->cg()->areAssignableGPRsScarce())
      removeUnusedLoops = true;

   if (trace)
      dumpOptDetails(comp, "\nWeighing candidate #%d\n", getSymbolReference()->getReferenceNumber());

   // If the candidate is marked to be live on entry but is not live on
   // entry in any of its predecessors and is not referenced in any of its
   // predecessor then remove the block from the _liveOnEntry bitVector
   //
   referencedBlocks->empty();
   vcount_t visitCount = comp->incVisitCount();
   TR_BitVectorIterator bvi;
   bool lookForBlocksToRemove = !isAllBlocks();
   // TODO take this all out
   // used for MetaData that are live on entry in blocks, just to mark that they can escape via return but there is no real loads
   // so we do not want such live on entry blocks to contribute to weigh of the candidate
   // TR_BitVector *ignoreLiveOnEntryBlocks = new (comp->trHeapMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory());
   while (lookForBlocksToRemove)
      {
      lookForBlocksToRemove = false;
      bvi.setBitVector(_liveOnEntry);
      while (bvi.hasMoreElements())
         {
         int32_t blockNumber = bvi.getNextElement();

         if (trace)
            dumpOptDetails(comp, "Examining original live-on-entry block_%d with loads and stores = %d\n", blockNumber, _blocks.getNumberOfLoadsAndStores(blockNumber));

         TR::Block * block = blocks[blockNumber];
         bool referencedInPred = false;
         for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
            {
            TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
            if (_liveOnEntry.get(startOfExtendedBBForBB[toBlock((*e)->getFrom())->getNumber()]->getNumber()))
            //if (_liveOnEntry.get(toBlock(e->getFrom())->getNumber()))
               { referencedInPred = true; break; }
            }
         if (!referencedInPred)
            for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
               //if (toBlock(e->getFrom())->startOfExtendedBlock()->findFirstReference(sym, true))
               {
               TR::Block *predBlock = toBlock((*e)->getFrom());
               TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
               TR::Block *parentPredBlock = startOfExtendedBBForBB[predBlock->getNumber()];
               while (parentPredBlock)
                  {
                  if (parentPredBlock->getVisitCount() < visitCount)
                     {
                     parentPredBlock->setVisitCount(visitCount);
                     if (parentPredBlock->findFirstReference(sym, visitCount))
                        {referencedBlocks->set(parentPredBlock->getNumber()); referencedInPred = true; break;}
                     }
                  else
                     {
                     if (referencedBlocks->get(parentPredBlock->getNumber()))
                        {
                        referencedInPred = true;
                        break;
                        }
                     }

                  // Fixed by Nakaike
                  // if (predBlock && !predBlock->isExtensionOfPreviousBlock())
                  if (parentPredBlock == predBlock ||
                      (parentPredBlock->getNextBlock() && ( parentPredBlock->getNextBlock() == block || !parentPredBlock->getNextBlock()->isExtensionOfPreviousBlock())))
                     break;
                  parentPredBlock = parentPredBlock->getNextBlock();
                  }

               if (referencedInPred)
                  break;
               }
         if (!referencedInPred)
            {
            if (trace)
               dumpOptDetails(comp, "Not referenced in pred, re-setting block_%d \n", blockNumber);

            _liveOnEntry.reset(blockNumber);
            lookForBlocksToRemove = true;
            }
         }
      }

   lookForBlocksToRemove = true;
   // If the number of loads and stores for a block is 0 and the candidate
   // is not live on exit then remove the block from the _liveOnEntry bitVector
   //
   if (lookForBlocksToRemove && trace)
      traceMsg(comp, "Trim live ranges upward:\n");

   while (lookForBlocksToRemove)
      {
      lookForBlocksToRemove = false;
      bvi.setBitVector(_liveOnEntry);
      while (bvi.hasMoreElements())
         {
         int32_t blockNumber = bvi.getNextElement();

         if (trace)
            traceMsg(comp, "\tExamining live-on-entry block_%d with loads and stores = %d\n", blockNumber, _blocks.getNumberOfLoadsAndStores(blockNumber));

         if (_blocks.getNumberOfLoadsAndStores(blockNumber) == 0)
            {
            TR::Block * block = blocks[blockNumber];
            bool liveOnExit = false;
             for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
               if (_liveOnEntry.get((*succ)->getTo()->getNumber()))
                  { liveOnExit = true; break; }

            if (!liveOnExit && block->isCold())
               {
               for (auto pred = block->getPredecessors().begin(); pred != block->getPredecessors().end(); ++pred)
                  if (!(*pred)->getFrom()->asBlock()->isCold())
                     {
                     liveOnExit = true;
                     break;
                     }
               }

            if (!liveOnExit)
               {
               if (trace)
                 traceMsg(comp, "\tNot referenced and not live on exit, re-setting block %d \n", blockNumber);
               _liveOnEntry.reset(blockNumber);
               lookForBlocksToRemove = true;
               }
            }
         }
      }

   // Calculate the max frequency of any block as per structure
   // (one could use some profiling here too)
   //
   int32_t maxFrequency = 1;
   bvi.setBitVector(_liveOnEntry);
   while (bvi.hasMoreElements())
     {
      int32_t blockNumber = bvi.getNextElement();
      if (trace)
         dumpOptDetails(comp, "Examining live-on-entry block_%d with loads and stores = %d to find maxFrequency\n", blockNumber, _blocks.getNumberOfLoadsAndStores(blockNumber));
      TR::Block * block = blocks[blockNumber];
      TR_BlockStructure *blockStructure = block->getStructureOf();
      int32_t blockWeight = 1;

      if (blockStructure &&
          (!dontAssignInColdBlocks(comp) || !block->isCold()))
           {
           blockWeight = blockStructureWeight[block->getNumber()];
           // blockStructure->calculateFrequencyOfExecution(&blockWeight);
           if (blockWeight > maxFrequency)
              maxFrequency = blockWeight;
           }
      }

   int32_t currMaxFrequency = maxFrequency;

   if (!getLoopsWithHoles().isEmpty())
      removeUnusedLoops = true;   // try to remove even if weight is not negative yet

   // For every loop nesting depth, figure out how many blocks
   // really use the candidate; discard blocks which don't use the candidate
   // iff the containing loop does not use the candidate at all. Basically
   // the entire loop will be discarded from the blocks list for the
   // candidate if the loop does not use the candidate.
   //
   TR_BitVector *resetLiveOnEntry = new (comp->trHeapMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory()); // Added by Nakaike
   TR_BitVector *resetLiveOnExit = new (comp->trHeapMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory());        // Added by Nakaike
   if ((maxFrequency > 1) &&
       removeUnusedLoops)
      {
      lookForBlocksToRemove = true;
      int32_t frequency = 1;

      if (comp->getMethodHotness() <= warm &&
          getLoopsWithHoles().isEmpty())
         frequency = maxFrequency;   // to save compile time

      while (lookForBlocksToRemove)
         {
         lookForBlocksToRemove = false;
         int32_t numBlocksAtThisFrequency = 0;
         int32_t numUnreferencedBlocksAtThisFrequency = 0;

         if (1) // to calculate numUnreferencedBlocksAtThisFrequency for setting maxFrequency later
            {
            // Maybe there is some value in figuring out the ratio of unutilized blocks
            // before we decide to discard any; experiment more with that; in that case use the
            // commented out portion below
            //
            bvi.setBitVector(_liveOnEntry);
            while (bvi.hasMoreElements())
               {
               int32_t blockNumber = bvi.getNextElement();
               TR::Block * block = blocks[blockNumber];
               TR_BlockStructure *blockStructure = block->getStructureOf();
               int32_t blockWeight = 1;

               if (blockStructure)
                  {
                  // blockStructure->calculateFrequencyOfExecution(&blockWeight);
                  TR_ASSERT(blockNumber == block->getNumber(),"blocks[x]->getNumber() != x");
                  blockWeight = blockStructureWeight[blockNumber];
                  }

               if (blockWeight == frequency)
                  {
                  if (_blocks.getNumberOfLoadsAndStores(blockNumber) == 0)
                     {
                     numUnreferencedBlocksAtThisFrequency++;
                     break;
                     }
                   }
               }
            }

         if (trace)
            traceMsg(comp, "At frequency %d for %d numBlocks %d numUnreferencedBlocks %d\n", frequency, getSymbolReference()->getReferenceNumber(), numBlocksAtThisFrequency, numUnreferencedBlocksAtThisFrequency);

         TR_ScratchList<TR_Structure> unreferencedLoops(comp->trMemory());
         TR_ScratchList<TR_Structure> referencedLoops(comp->trMemory());
         if ((numUnreferencedBlocksAtThisFrequency > 0) &&
             (comp->getOption(TR_EnableRangeSplittingGRA) ||  // move this check above ?
              (comp->cg()->areAssignableGPRsScarce())))
            {
            bool seenBlockAtThisFrequency = false;
            bvi.setBitVector(_liveOnEntry);
            while (bvi.hasMoreElements())
               {
               int32_t blockNumber = bvi.getNextElement();
               TR::Block * block = blocks[blockNumber];
               TR_BlockStructure *blockStructure = block->getStructureOf();
               int32_t blockWeight = 1;

               if (blockStructure)
                  {
                  blockWeight = blockStructureWeight[block->getNumber()];
                  }

               if (blockWeight == frequency)
                  {
                  // If this is a block where the candidate is unused
                  // try to see if the nearest containing loops requires it
                  // at some point. If not, then it is worthless to have the
                  // blocks inside this loop in the live range for this candidate
                  // as the candidate is never referenced inside the loop
                  //
                  //
                  bool referencedInNearestLoop = true;
                  if (!getLoopsWithHoles().isEmpty())
                     {
                     TR_RegionStructure *parent = blockStructure->getParent()->asRegion();
                     while (parent &&
                            !parent->isNaturalLoop())
                        {
                        if (parent->getParent())
                           parent = parent->getParent()->asRegion();
                        else
                           {
                           parent = NULL;
                           break;
                           }
                        }

                     if (parent && getLoopsWithHoles().find(parent))
                        {
                        referencedInNearestLoop = false;
                        unreferencedLoops.add(parent);
                        }
                     }

                  if (!referencedInNearestLoop  &&
                      ((_blocks.getNumberOfLoadsAndStores(blockNumber) == 0) ||
                       (dontAssignInColdBlocks(comp) && block->isCold())  /*||
                                                                            block->isRare(comp) */ ))
                     {
                     referencedInNearestLoop = false;
                     TR_RegionStructure *parent = blockStructure->getParent()->asRegion();
                     while (parent &&
                            !parent->isNaturalLoop())
                        {
                        if (parent->getParent())
                           parent = parent->getParent()->asRegion();
                        else
                           {
                           parent = NULL;
                           break;
                           }
                        }

                     if (parent)
                        {
                        if (!unreferencedLoops.find(parent))
                           {
                           if (!referencedLoops.find(parent))
                              {
                              List<TR::Block> blocksInReferencedLoop(comp->trMemory());
                              parent->getBlocks(&blocksInReferencedLoop);
                              ListIterator<TR::Block> blocksIt(&blocksInReferencedLoop);
                              TR::Block *nextBlock;
                              blocksIt.reset();
                              for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
                                 {
                                 if (nextBlock->getEntry())
                                    {
                                    TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
                                    TR::Block *liveBlock = startOfExtendedBBForBB[nextBlock->getNumber()];
                                    if (_liveOnEntry.get(liveBlock->getNumber()))
                                       {
                                       if ((_blocks.getNumberOfLoadsAndStores(blockNumber) == 0) &&
                                            (!dontAssignInColdBlocks(comp) || !nextBlock->isCold()))
                                          {
                                          referencedInNearestLoop = true;
                                          referencedLoops.add(parent);
                                          break;
                                          }
                                       }
                                    }
                                 }
                              }
                           else
                              referencedInNearestLoop = true;
                           }
                        }

                     if (!referencedInNearestLoop)
                        {
                        bool shouldResetThisLoop = true;
                        if (parent &&
                            !unreferencedLoops.find(parent))
                           {
                           //unreferencedLoops.add(parent);
                           if (!comp->getOptimizer()->getResetExitsGRA())
                              comp->getOptimizer()->setResetExitsGRA(new (comp->trHeapMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory()));
                           else
                              comp->getOptimizer()->getResetExitsGRA()->empty();

                           int32_t loopFrequency = parent->getEntryBlock()->getFrequency();
                           ListIterator<TR::CFGEdge> ei(&parent->getExitEdges());
                           for (TR::CFGEdge *edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
                              {
                              int32_t toStructureNumber = edge->getTo()->getNumber();
                              int32_t fromFrequency = edge->getFrom()->asStructureSubGraphNode()->getStructure()->getEntryBlock()->getFrequency();
                              int32_t toFrequency = -1;

                              bool shouldResetExit = true;

                              if (toStructureNumber == comp->getFlowGraph()->getEnd()->getNumber())
                                {
                                // The exit block shouldn't be in any live ranges anyway,
                                // so putting it in the resetExits set is unnecessary at best.
                                //
                                shouldResetExit = false;
                                }

                              //
                              //if (fromFrequency > 0)
                              //   {
                              //   ListIterator<TR::CFGEdge> succIt(&(parent->getParent()->findSubNodeInRegion(parent->getNumber())->getSuccessors()));
                              //   TR::CFGEdge *succ;
                              //   TR_Structure *succStructure = NULL;
                              //   for (succ = succIt.getFirst(); succ != NULL; succ = succIt.getNext())
                              //       {
                              //       TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) succ->getTo();
                              //       if (succNode->getNumber() == toStructureNumber)
                              //          {
                              //          succStructure = succNode->getStructure();
                              //          break;
                              //          }
                              //       }
                              //
                              //
                              //   if (succStructure)
                              //      {
                              //      toFrequency = succStructure->getEntryBlock()->getFrequency();
                              //      if (4*toFrequency > fromFrequency)
                              //         shouldResetExit = false;
                              //      if (4*toFrequency > loopFrequency)
                              //         shouldResetThisLoop = false;
                              //      }
                              //   }

                              if (shouldResetExit)
                                 {
                                 comp->getOptimizer()->getResetExitsGRA()->set(toStructureNumber);
                                 //_liveOnEntry.reset(toStructureNumber);
                                 }
                              }
                           if (shouldResetThisLoop)
                              {
                              if (trace)
                                 {
                                 dumpOptDetails(comp, "\n1Loop %d does not use candidate, removing the following blocks from live ranges of candidate #%d \n", parent->getNumber(), getSymbolReference()->getReferenceNumber());
                                 comp->getOptimizer()->getResetExitsGRA()->print(comp);
                                 }
                              _liveOnEntry -= *comp->getOptimizer()->getResetExitsGRA();
                              *resetLiveOnEntry |= *comp->getOptimizer()->getResetExitsGRA(); // Added by Nakaike
                              }
                           }

                        if (shouldResetThisLoop)
                           {
                           if (trace)
                              dumpOptDetails(comp, "\n2Loop does not use candidate, removing block_%d from live ranges of candidate #%d \n", blockNumber, getSymbolReference()->getReferenceNumber());

                           unreferencedLoops.add(parent);  // should not be moved where exits are removed ?
                           _liveOnEntry.reset(blockNumber);
                           _liveOnExit.reset(blockNumber);
                           resetLiveOnEntry->set(blockNumber); // Added by Nakaike
                           resetLiveOnExit->set(blockNumber);   // Added by Nakaike
                           }
                        else
                           referencedLoops.add(parent);
                        //dumpOptDetails(comp, "Resetting liveOnEntry at block_%d frequency %d for %d\n", blockNumber, frequency, getSymbolReference()->getReferenceNumber());
                        //dumpOptDetails(comp, "loadsStores %d\n", (*_loadsAndStores)[blockNumber]);
                        }
                     }

                  if (_liveOnEntry.get(blockNumber))
                     currMaxFrequency = frequency;

                  }
               }
            }

         // Fixed by Nakaike
         if (numUnreferencedBlocksAtThisFrequency == 0)
            currMaxFrequency = frequency;

         frequency = frequency*10;
         if (frequency <= maxFrequency)
            lookForBlocksToRemove = true;

         }  // while lookForBlocksToRemove
      }

   maxFrequency = currMaxFrequency;

   if (callToRemoveUnusedLoops)
       getLoopsWithHoles().deleteAll();

   if (getSplitSymbolReference()) // Added by Nakaike
      {
      _liveOnEntry |= *resetLiveOnEntry;
      _liveOnExit |= *resetLiveOnExit;
      }

   int32_t origNumberOfBlocks = 0;
   int32_t origLoadsAndStores = 0;
   bool eliminatedBlocksForDebugging = false;
   bvi.setBitVector(_liveOnEntry);
   int32_t bnum;
   while (trace && bvi.hasMoreElements())
      {
      bnum = bvi.getNextElement();

      TR::Block * block = blocks[bnum];
      TR_BlockStructure *blockStructure = block->getStructureOf();
      int32_t blockWeight = 1;

      if (blockStructure)
         {
         // blockStructure->calculateFrequencyOfExecution(&blockWeight);
         TR_ASSERT(bnum == block->getNumber(),"blocks[x]->getNumber() != x");
         blockWeight = blockStructureWeight[bnum];
         }

      bool ignoreBlock = (dontAssignInColdBlocks(comp) && block->isCold()) || block->isOSRInduceBlock();
      if (!ignoreBlock && (blockWeight >= maxFrequency*freqRatio))
         ++origNumberOfBlocks;

      if (!ignoreBlock)
         origLoadsAndStores += _blocks.getNumberOfLoadsAndStores(bnum);

      if (trace && !performTransformation(comp, "%s Candidate %d live on entry at block_%d\n", OPT_DETAILS, getSymbolReference()->getReferenceNumber(), bnum))
         {
         _liveOnEntry.reset(bnum);
         eliminatedBlocksForDebugging = true;
         }
      }


   _weight = 0;
   int32_t loadsAndStores = 0, numberOfBlocks = 0;

   bvi.setBitVector(_liveOnEntry);
   while (bvi.hasMoreElements())
      {
      int32_t blockNumber = bvi.getNextElement();

      TR::Block * block = blocks[blockNumber];
      TR_BlockStructure *blockStructure = block->getStructureOf();
      int32_t blockWeight = 1;

      if (blockStructure)
         {
         // blockStructure->calculateFrequencyOfExecution(&blockWeight);
         blockWeight = blockStructureWeight[block->getNumber()];
         }

      bool ignoreBlock = (dontAssignInColdBlocks(comp) && block->isCold()) || block->isOSRInduceBlock();
      if (!ignoreBlock && (blockWeight >= maxFrequency*freqRatio || useProfilingFrequencies))
         {
         ++numberOfBlocks;
         }

      if (!ignoreBlock)
         loadsAndStores += _blocks.getNumberOfLoadsAndStores(blockNumber);



      if (trace)
         dumpOptDetails(comp, "Examining live-on-entry block_%d (blockWeight %d maxFrequency %d) numberOfBlocks %d loadsAndStores %d", block->getNumber(), blockWeight, maxFrequency, numberOfBlocks, loadsAndStores);

      for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
         {
         TR::Block * pred = toBlock((*e)->getFrom());
         if (pred == comp->getFlowGraph()->getStart())
            continue;
         //TR::Block * extendedPred = pred->startOfExtendedBlock();
         TR::Node * node = pred->getLastRealTreeTop()->getNode();

         TR_BlockStructure *predStructure = pred->getStructureOf();
         int32_t predWeight = 1;
         if (predStructure)
            {
            predWeight = blockStructureWeight[pred->getNumber()];
            // predStructure->calculateFrequencyOfExecution(&predWeight);
            }

         // If the preds can't handle so many candidates across the branch
         // do not make this candidate live on entry
         //
         // If the pred is inside a loop but this block is not, and if the pred
         // would load the value into the register, then this would introduce
         // a load from memory inside a loop to satisfy a block outside a loop.
         // Never allow this. The candidate will be marked as not live on entry
         // to the block outside the loop so it is not loaded inside the loop.
         //
         // One special case: if there is a store inside the extended basic block
         // containing pred, then leave the candidate marked live on entry since
         // we'd like to avoid doing the store itself.  If liveOnEntry is false
         // but there are loads or stores, then it must be a store in the block.
         //
         if (!comp->cg()->allowGlobalRegisterAcrossBranch(this, node)
//             || prevBlockTooRegisterConstrained(comp,block,blockGPRCount,blockFPRCount,blockVRFCount)
            )
            {
            if (trace)
               dumpOptDetails(comp, " (too constrained), is NOT live on entry because of pred %d ", pred->getNumber());
            _liveOnEntry.reset(blockNumber);
            break;
            }

         bool liveOnExitFromPred = false;
         bool someOtherSuccSeen = false;
         TR_ExtendedBlockSuccessorIterator ebsi(pred, comp->getFlowGraph());
         for (TR::Block * succBlock = ebsi.getFirst(); succBlock; succBlock = ebsi.getNext())
            {
            if (succBlock != block)
               {
               //if (trace)
               //   dumpOptDetails(comp, " (some other succ %d of pred %d) ", succBlock->getNumber(), pred->getNumber());
               someOtherSuccSeen = true;
               if (_liveOnEntry.get(succBlock->getNumber()))
                  { liveOnExitFromPred = true; break; }
               }
            }

         if (!someOtherSuccSeen)
            liveOnExitFromPred = true;

         if (((predWeight > blockWeight) &&
              !liveOnExitFromPred &&
              !_liveOnEntry.get(pred->getNumber())) &&
              _blocks.getNumberOfLoadsAndStores(startOfExtendedBBForBB[pred->getNumber()]->getNumber()) == 0 &&
              _blocks.getNumberOfLoadsAndStores(pred->getNumber()) == 0)
            {
            bool reset = true;
            if (useProfilingFrequencies && (pred->getFrequency() > 0) &&
                (4*block->getFrequency() > pred->getFrequency()))
               reset = false;

            if (reset)
               {
               if (trace)
                  dumpOptDetails(comp, " is NOT live on entry because of pred %d ", pred->getNumber());

               _liveOnEntry.reset(blockNumber);
               break;
               }
            }
         }

      if (_liveOnEntry.get(blockNumber))
         {
         if (trace)
            dumpOptDetails(comp, "is live on entry");

         TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
         TR::Block *startOfExtendedBlock = startOfExtendedBBForBB[block->getNumber()];
         for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
            {
            TR::Block * pred = toBlock((*e)->getFrom());
            if (pred == comp->getFlowGraph()->getStart())
               continue;

            TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
            TR::Block * extendedPred = startOfExtendedBBForBB[pred->getNumber()];

            if ((pred->getNextBlock() == block) &&
                (extendedPred == startOfExtendedBlock))
               {
               TR::Node *lastNode = pred->getLastRealTreeTop()->getNode();

               if(lastNode->getOpCodeValue() == TR::treetop)
                   lastNode = lastNode->getFirstChild();

               if (lastNode->getOpCode().isBranch())
                  {
                  if (lastNode->getBranchDestination() != startOfExtendedBlock->getEntry())
                     continue;
                  }
               else if (!lastNode->getOpCode().isJumpWithMultipleTargets())
                  continue;
               }

            //_liveOnExit.set(extendedPred->getNumber());
            _liveOnExit.set(pred->getNumber());

            if (!_liveOnEntry.get(pred->getNumber()) && (!dontAssignInColdBlocks(comp) || !pred->isCold()))
               {
               //++numberOfBlocks;  // TODO: try uncomment this
               loadsAndStores += _blocks.getNumberOfLoadsAndStores(pred->getNumber());
               if (trace)
                  dumpOptDetails(comp, "\n    Adding %d loads and stores to pred  block_%d ",
                                         _blocks.getNumberOfLoadsAndStores(pred->getNumber()), pred->getNumber());

               }
            }
         }
      if (trace)
        dumpOptDetails(comp, "\n");
      }

   extendLiveRangesForLiveOnExit(comp, blocks, startOfExtendedBBForBB);

   int32_t reduction = 0;

   //static TR_BitVector *seenBlocks;
   if (!comp->getOptimizer()->getSeenBlocksGRA())
      comp->getOptimizer()->setSeenBlocksGRA(new (comp->trHeapMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory()));
   else
      comp->getOptimizer()->getSeenBlocksGRA()->empty();


   bvi.setBitVector(_liveOnExit);
   while (bvi.hasMoreElements())
      {
      int32_t blockNumber = bvi.getNextElement();
      if (!_liveOnEntry.get(blockNumber) &&
         (_blocks.getNumberOfLoadsAndStores(blockNumber) > 0))
         {
         int32_t blockWeight = blockStructureWeight[blockNumber];
         loadsAndStores = loadsAndStores + blockWeight;

         if (trace)
            traceMsg(comp, "Increased weight of candidate %d by %d (new weight %d) because of live on exit from block_%d\n", getSymbolReference()->getReferenceNumber(), blockWeight, loadsAndStores, blockNumber);
         }
      }

   bvi.setBitVector(_liveOnEntry);
   while (bvi.hasMoreElements())
      {
      int32_t blockNumber = bvi.getNextElement();
      TR::Block * block = blocks[blockNumber];

      do {
         blockNumber = block->getNumber();
         if (_blocks.getNumberOfLoadsAndStores(blockNumber) > 0)
            {
            bool foundEndOfRange = false;
            for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
               {
               if (((*succ)->getTo() != block->getNextBlock()) &&
                   ((*succ)->getTo() != comp->getFlowGraph()->getEnd()))
                  {
                  if (!_liveOnEntry.get((*succ)->getTo()->getNumber()))
                     {
                     TR_BlockStructure *blockStructure = block->getStructureOf();
                     int32_t blockWeight = 1;
                     bool exclude = block->isCatchBlock();

                     if (!exclude && blockStructure && (/* !dontAssignInColdBlocks(comp) || */ (!block->isCold() && !(*succ)->getTo()->asBlock()->isCold())) && !comp->getOptimizer()->getSeenBlocksGRA()->get(block->getNumber()) &&
                         (// symbolIsLive(block) ||
                          symbolIsLive((*succ)->getTo()->asBlock())))
                        {
                        TR_ASSERT(blockNumber == block->getNumber(),"blocks[x]->getNumber() != x");
                        comp->getOptimizer()->getSeenBlocksGRA()->set(blockNumber);
                        // blockStructure->calculateFrequencyOfExecution(&blockWeight);
                        blockWeight = blockStructureWeight[blockNumber];
                        int32_t succWeight = blockStructureWeight[(*succ)->getTo()->getNumber()];
                        if (succWeight < blockWeight)
                           blockWeight = succWeight;

                        TR_Structure *containingLoop = blockStructure->getContainingLoop();
                        if (containingLoop)
                           addLoopWithHole(containingLoop);

                        //blockWeight = (blockWeight + succWeight)/2; // maybe the store will be inserted in a split block in between the 2 blocks

                        loadsAndStores = loadsAndStores - blockWeight;
                        reduction = reduction + blockWeight;
                        if (trace)
                           traceMsg(comp, "Reduced weight of candidate %d by %d (new weight %d) because of end in live range at block_%d succ %d\n", getSymbolReference()->getReferenceNumber(), blockWeight, loadsAndStores, blockNumber, (*succ)->getTo()->getNumber());
                        foundEndOfRange = true;
                        break;
                        }
                     }
                  }
               }

            if (foundEndOfRange || block == comp->getFlowGraph()->getEnd())
                break;
            }

         } while ((block = block->getNextBlock()) && block->isExtensionOfPreviousBlock());
      }


   comp->getOptimizer()->getSeenBlocksGRA()->empty();
   bvi.setBitVector(_liveOnExit);
   while (bvi.hasMoreElements())
      {
      int32_t blockNumber = bvi.getNextElement();
      TR::Block * block = blocks[blockNumber];
      TR::Block * extBlock = block;
      int32_t extBlockNumber = blockNumber;
      if (block != comp->getFlowGraph()->getStart())
         {
         TR_ASSERT(blockNumber == block->getNumber(),"blocks[x]->getNumber() != x");
         TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
         extBlock = startOfExtendedBBForBB[blockNumber];
         extBlockNumber = extBlock->getNumber();
         }

      if (!_liveOnEntry.get(extBlockNumber) &&
          (block != comp->getFlowGraph()->getStart()) &&
          (_blocks.getNumberOfLoadsAndStores(extBlockNumber) == 0))
          {
          //do {
             if ((_blocks.getNumberOfLoadsAndStores(extBlockNumber) == 0) &&
                 (_blocks.getNumberOfLoadsAndStores(blockNumber) == 0))
                {
                bool foundEndOfRange = false;
                for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end(); ++succ)
                   {
                   if ((*succ)->getTo() != block->getNextBlock())
                      {
                      if (_liveOnEntry.get((*succ)->getTo()->getNumber()))
                         {
                         TR_BlockStructure *blockStructure = block->getStructureOf();
                         int32_t blockWeight = 1;

                         bool exclude = block->isCatchBlock();

                         if (!exclude && blockStructure && (/* !dontAssignInColdBlocks(comp) || */ !block->isCold()) && !comp->getOptimizer()->getSeenBlocksGRA()->get(extBlockNumber))
                            {
                            TR_Structure *containingLoop = blockStructure->getContainingLoop();
                            if (containingLoop)
                               addLoopWithHole(containingLoop);

                            comp->getOptimizer()->getSeenBlocksGRA()->set(extBlockNumber);
                            // blockStructure->calculateFrequencyOfExecution(&blockWeight);
                            TR_ASSERT(blockNumber == block->getNumber(),"blocks[x]->getNumber() != x");
                            blockWeight = blockStructureWeight[blockNumber];
                            loadsAndStores = loadsAndStores - blockWeight;
                            reduction = reduction + blockWeight;
                            if (trace)
                               traceMsg(comp, "Reduced weight of candidate %d by %d (new weight %d) because of start of live range at block_%d succ %d\n", getSymbolReference()->getReferenceNumber(), blockWeight, loadsAndStores, blockNumber, (*succ)->getTo()->getNumber());
                            foundEndOfRange = true;
                            break;
                            }
                         }
                      }
                   }

                //if (foundEndOfRange)
                //  break;
               }

         // } while ((block = block->getNextBlock()) && block->isExtensionOfPreviousBlock());
         }
      }

   if ((numberOfBlocks == 0) && (loadsAndStores > 0))
      numberOfBlocks = 1;

   if (eliminatedBlocksForDebugging && origNumberOfBlocks)
      {
      numberOfBlocks = origNumberOfBlocks;
      loadsAndStores = origLoadsAndStores;
      }

   bool isPPS = false; // (comp->getOption(TR_EnableOSR) && getSymbol()->isAuto() && (getSymbolReference()->getCPIndex() < 0)); // This hurts in WAS performance; so it is disable until I can figure out the issue

   if (numberOfBlocks && isPPS)
     {
     loadsAndStores = INT_MAX - 1;
     _weight = INT_MAX - 1;
     if (trace)
        dumpOptDetails(comp, "For candidate #%d weight = %d loadsAndStores = %d numberOfBlocks = %d\n", getSymbolReference()->getReferenceNumber(), _weight, loadsAndStores, numberOfBlocks);
     }
   else if (numberOfBlocks)
      {
      if (rcNeeds2Regs(comp))
         {
         if (!isHighWordZero())
            {
            loadsAndStores = 2*loadsAndStores;
            }
         else
            {
            loadsAndStores--;
            }
         }

      if (!useProfilingFrequencies)
          loadsAndStores = std::min(INT_MAX / 10000, loadsAndStores);

      if (!comp->allocateAtThisOptLevel() &&
            getSymbolReference()->getSymbol()->isMethodMetaData())
         _weight = 0;
      else if (loadsAndStores > 0)
         {
         if (!useProfilingFrequencies)
            _weight = std::max(1, ( loadsAndStores * 10000) / (numberOfBlocks*numberOfBlocks));
         else
            _weight = std::max(1, (loadsAndStores) / (numberOfBlocks*numberOfBlocks));
         }
      else
         {
         if (trace)
             traceMsg(comp, "handle negative weight\n");

         if (!callToRemoveUnusedLoops)
            {
            getBlocksLiveOnExit().empty();
            _liveOnEntry |= _originalLiveOnEntry;
            processLiveOnEntryBlocks(blocks, blockStructureWeight, comp,
                                     blockGPRCount,
                                     blockFPRCount,
                                     blockVRFCount,
                                     referencedBlocks, startOfExtendedBBForBB, true);
            }
         else
            {
            _weight = 0;
            }
         }

      if (trace)
         dumpOptDetails(comp, "For candidate #%d weight = %d loadsAndStores = %d numberOfBlocks = %d\n", getSymbolReference()->getReferenceNumber(), _weight, loadsAndStores, numberOfBlocks);
      }
   else if (trace)
      dumpOptDetails(comp, "Final weight is 0 because there are no blocks\n");
     }

//
// If a candidate is live on entry to an extended block, it has to be live on exit from all its
// successors. Moreover, it will be live from the time it is first referenced in the successor
// till the corressponding exit
//
void
TR_RegisterCandidate::extendLiveRangesForLiveOnExit(TR::Compilation *comp, TR::Block **blocks, TR_Array<TR::Block *>& startOfExtendedBBForBB)
   {
   LexicalTimer t("extendLiveRangesForLiveOnExit", comp->phaseTimer());

   bool trace = comp->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);
   if (trace)
      traceMsg(comp, "Extending live ranges due to live on exits\n");

   TR_BitVector blocksVisited(comp->trMemory()->currentStackRegion());
   TR_BitVector *blocksReferencing = comp->getGlobalRegisterCandidates()->getBlocksReferencingSymRef(getSymbolReference()->getReferenceNumber());

   _liveOnExit.empty();  // this function will recalculate

   // TODO: move allocation of working bitvectors out
   //    don't visit the same ext block more than once
   //    save all liveOnEntry that were added
   TR_BitVectorIterator bvi(_liveOnEntry);
   while (bvi.hasMoreElements())
      {
      int32_t blockNumber = bvi.getNextElement();
      TR::Block * block = blocks[blockNumber];
      if (!block->isExtensionOfPreviousBlock())
         {
         for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
            {
            TR::Block * pred = toBlock((*e)->getFrom());
            if (pred == comp->getFlowGraph()->getStart())
               continue;

            if (blocksVisited.get(pred->getNumber()))
               continue;

            TR_ASSERT(comp->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
            TR::Block * extPred = startOfExtendedBBForBB[pred->getNumber()];

            //
            // Top-down pass through the extended block:
            //       find all blocks where candidate is available to be used on exit
            //
            TR::Block *currBlock = extPred;
            TR::Block *lastBlock;
            do
               {
               blocksVisited.set(currBlock->getNumber());
               lastBlock = currBlock;
               currBlock = currBlock->getNextBlock();
               } while (currBlock && currBlock->isExtensionOfPreviousBlock());

            //
            // Bottom-up pass through the extended block:
            //    liveOnExit = U <liveOnEntry of all successors>
            //    liveOnEntry |= liveOnExit & !<available in this block> & <available in prev block>
            currBlock = lastBlock;
            do
               {
               for (auto e = currBlock->getSuccessors().begin(); e != currBlock->getSuccessors().end(); ++e)
                  {
                  TR::Block *succ = toBlock((*e)->getTo());
                  if (_liveOnEntry.get(succ->getNumber()))
                     {
                     _liveOnExit.set(currBlock->getNumber());
                     break;
                     }
                  }

               if (!_liveOnEntry.get(currBlock->getNumber()) &&  // not set already
                  _liveOnExit.get(currBlock->getNumber()) &&
                  currBlock->isExtensionOfPreviousBlock() &&
                  blocksReferencing &&
                  blocksReferencing->get(currBlock->getPrevBlock()->getNumber()) &&
                  !blocksReferencing->get(currBlock->getNumber()))
                  {
                  if (trace)
                     traceMsg(comp, "\tFor candidate #%d, set live on block_%d entry due to live on exit\n", getSymbolReference()->getReferenceNumber(), currBlock->getNumber());

                  _liveOnEntry.set(currBlock->getNumber());
                  }

               if (currBlock->isExtensionOfPreviousBlock())
                  currBlock = currBlock->getPrevBlock();
               else
                  break;
               } while (currBlock);

            }
         }
      }
   }

void
TR_RegisterCandidates::lookForCandidates(
   TR::Node * node, TR::Symbol * entrySymbol, TR::Symbol * exitSymbol, bool & foundEntry, bool & foundExit)
   {
   if (node->getVisitCount() == _compilation->getVisitCount())
      return;
   node->setVisitCount(_compilation->getVisitCount());

   bool foundEntryBeforeLookingAtChildren = foundEntry;

   int32_t i;
   for (i = 0; i < node->getNumChildren(); ++i)
      lookForCandidates(node->getChild(i), entrySymbol, exitSymbol, foundEntry, foundExit);

   if (node->getOpCode().hasSymbolReference())
      {
      TR::Symbol * s = node->getSymbol();
      if (s == exitSymbol)
         {
         if (!foundExit && !foundEntryBeforeLookingAtChildren)
            foundEntry = false;

         foundExit = true;
         }
      else if (s == entrySymbol)
         {
         foundEntry = true;
         }
      }
   }

bool
TR_RegisterCandidates::candidatesOverlap(
   TR::Block * block, TR_RegisterCandidate * candidateOnEntry, TR_RegisterCandidate * candidateOnExit, bool trace)
   {
   LexicalTimer t("candidatesOverlap", comp()->phaseTimer());

   if (candidateOnEntry == NULL || candidateOnExit == NULL)
      return false;

   // The candidates overlap if we see a reference to the entry symbol after seeing a reference
   // to the exit symbol.
   // Note, since a RegStore will have to be inserted to switch from one register to another,
   // if both symbol are used in the same tree, they are overlapping regardless of which order
   // they are referenced in.
   //
   TR::Symbol * entrySymbol = candidateOnEntry->getSymbolReference()->getSymbol();
   TR::Symbol * exitSymbol = candidateOnExit->getSymbolReference()->getSymbol();

   TR::SymbolReference *entrySymRef = NULL;
   TR::SymbolReference *exitSymRef = NULL;
   bool seenExitSymbol = false;
   for (TR::TreeTop * tt = block->getFirstRealTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node * node = tt->getNode();
      if (node->getOpCodeValue() == TR::BBStart)  // Start of next block
         return false;

      bool entrySymbolInTree = false;
      bool exitSymbolInTree = false;
      //lookForCandidates(node, entrySymbol, exitSymbol, entrySymbolInTree, exitSymbolInTree);
      lookForCandidates(node, entrySymbol, exitSymbol, entrySymbolInTree, seenExitSymbol);
      seenExitSymbol |= exitSymbolInTree;

      if (node->getOpCodeValue() == TR::treetop) node = node->getFirstChild();


      if (entrySymbolInTree && seenExitSymbol)
         {
         if (trace)
            traceMsg(comp(), "Returning true in block_%d node %p entry cand %d exit cand %d\n", block->getNumber(), node, candidateOnEntry->getSymbolReference()->getReferenceNumber(), candidateOnExit->getSymbolReference()->getReferenceNumber());
         return true;
         }
      }

   return false;
   }


bool
TR_RegisterCandidates::prioritizeCandidate(
   TR_RegisterCandidate * newRC, TR_RegisterCandidate * & first)
   {
   LexicalTimer t("prioritizeCandidate", comp()->phaseTimer());

   if (newRC->getWeight() != 0)
      {
      TR_RegisterCandidate * rc = first, * prev = 0;
      for (; rc; prev = rc, rc = rc->getNext())
         {
         if (newRC->getWeight() > rc->getWeight())
            break;
         }
      if (prev) prev->setNext(newRC);
      else first = newRC;

      newRC->setNext(rc);
      return true;
      }
   return false;
   }

TR_RegisterCandidate *
TR_RegisterCandidates::reprioritizeCandidates(
                  TR_RegisterCandidate * first, TR::Block * * blocks, int32_t *blockStructureWeight, int32_t numberOfBlocks, TR::Block * fullBlock, TR::Compilation *comp,
                  bool reprioritizeFP, bool onlyReprioritizeLongs, TR_BitVector *referencedBlocks,
                  TR_Array<int32_t> & blockGPRCount, TR_Array<int32_t> & blockFPRCount, TR_Array<int32_t> & blockVRFCount, TR_BitVector *successorBits,
                  bool trace)
   {
   LexicalTimer t("reprioritizeCandidates", comp->phaseTimer());
   if (!successorBits)
      {
      successorBits = new (trStackMemory()) TR_BitVector(numberOfBlocks, trMemory(), stackAlloc, growable);
      //TR_BitVector successorBits(numberOfBlocks, trMemory(), stackAlloc, growable);

      TR_ExtendedBlockSuccessorIterator ebsi(fullBlock, comp->getFlowGraph());
      //TR_Array<int32_t> blockGPRCount(trMemory(), numberOfBlocks,true,stackAlloc);
      //TR_Array<int32_t> blockFPRCount(trMemory(), numberOfBlocks,true,stackAlloc);
      //TR_Array<int32_t> blockVRFCount(trMemory(), numberOfBlocks,true,stackAlloc);
      for (TR::Block * b = ebsi.getFirst(); b; b = ebsi.getNext())
         {
         successorBits->set(b->getNumber());
         }
      }

   TR_RegisterCandidate *pFirst = NULL;
   TR_RegisterCandidate *next;
   for (TR_RegisterCandidate * rc = first; rc; rc = next)
      {
      next = rc->getNext();
      bool isFPCandidate = false;
      if (rc->getDataType() == TR::Float
          || rc->getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
          || rc->getDataType() == TR::DecimalFloat
          || rc->getDataType() == TR::DecimalDouble
          || rc->getDataType() == TR::DecimalLongDouble
#endif
          )
        isFPCandidate = true;

      if (rc->getBlocksLiveOnEntry().intersects(*successorBits))
         {
         if ((!onlyReprioritizeLongs ||
              (rc->getType().isInt64() &&
               TR::Compiler->target.is32Bit())) &&
             ((reprioritizeFP && isFPCandidate) ||
              (!reprioritizeFP && !isFPCandidate)))
            {
            if (trace)
               {
               dumpOptDetails(comp, "\nBefore repriortization, removing the following blocks from live ranges of candidate #%d \n", rc->getSymbolReference()->getReferenceNumber());
               successorBits->print(comp);
               }

            rc->getBlocksLiveOnEntry() -= *successorBits;
            rc->recalculateWeight(blocks, blockStructureWeight, comp,blockGPRCount,blockFPRCount, blockVRFCount, referencedBlocks, _startOfExtendedBBForBB);
            }
         }

      prioritizeCandidate(rc, pFirst);
      }
   first = pFirst;

   return first;
   }


static void
scanPressureSimulatorCacheForConflicts(TR_RegisterCandidate *rc, TR_BitVector &blocks, TR_Array<TR_BitVector> &liveOnEntryConflicts, TR_Array<TR_BitVector> &liveOnExitConflicts, TR_Array<TR_BitVector> &entryExitConflicts, int32_t firstRegister, int32_t lastRegister, TR_BitVector *spilledRegs, TR::Compilation *comp, TR::Block * * all_blocks)
   {
   LexicalTimer t("scanPressureSimulatorCacheForConflicts", comp->phaseTimer());

   bool trace = comp->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);

   if(trace)
     {
     traceMsg(comp,"\tscanPressureSimulatorCacheForConflits: Candidate #%d\n",rc->getSymbolReference()->getReferenceNumber());
     traceMsg(comp,"\t blocks candidate is live: ");
     blocks.print(comp);
     traceMsg(comp, "\n");
     traceMsg(comp,"\t   adding conflicts as reg#: { [Entry|Exit](block#) [Entry|Exit](block#) ...}\n");
     }

   TR_BitVectorIterator blockIterator(blocks);
   while (blockIterator.hasMoreElements())
      {
      int32_t blockNum = blockIterator.getNextElement();
      spilledRegs->empty();
      comp->cg()->computeSimulatedSpilledRegs(spilledRegs, blockNum, rc->getSymbolReference());

      if(trace)
        {
        traceMsg(comp,"\t  Adding conflicts in block_%d\n",blockNum);
        traceMsg(comp,"\t   spilled regs: ");
        TR_BitVector tmpSpilledRegs(*spilledRegs);
        TR_BitVectorIterator ri(tmpSpilledRegs);
        while (ri.hasMoreElements())
         {
         int32_t r = ri.getNextElement();
         if (r < firstRegister ||  r > lastRegister)
           tmpSpilledRegs.reset(r);
         }
  tmpSpilledRegs.print(comp);
        traceMsg(comp, "\n");
        }

      TR_BitVectorIterator regIterator(*spilledRegs);
      while (regIterator.hasMoreElements())
         {
         int32_t regNum = regIterator.getNextElement();
         if (firstRegister <= regNum && regNum <= lastRegister)
            {
            TR_ASSERT(rc->getBlocksLiveOnEntry().get(blockNum) || rc->getBlocksLiveOnExit().get(blockNum), "Candidate should be live on entry or on exit\n");
            bool intervalStartsAtEBBEntry = false;

            if(trace)
              {
              traceMsg(comp,"\t    reg %d: {",regNum);
              }
            // Trim live ranges up
            TR::Block *curr = all_blocks[blockNum];
            TR_ASSERT(curr->getNumber() == blockNum,"blocks[x]->getNumber() != x");
            int32_t currBlockNum = blockNum;
            while (rc->getBlocksLiveOnEntry().get(currBlockNum))
               {

               liveOnEntryConflicts[regNum].set(currBlockNum);
               if(trace)
                 {
                 traceMsg(comp," Entry(%d)",currBlockNum);
                 }

               if (!curr->isExtensionOfPreviousBlock())
                  {
                  intervalStartsAtEBBEntry = true;
                  break;
                  }
               curr = curr->getPrevBlock();
               currBlockNum = curr ? curr->getNumber(): 0;

               if (!curr  || !rc->getBlocksLiveOnExit().get(currBlockNum))
                   break;

               liveOnExitConflicts[regNum].set(currBlockNum);
               if(trace)
                 {
                 traceMsg(comp," Exit(%d)",currBlockNum);
                 }
               }

            // Trim live ranges down
            curr = all_blocks[blockNum];
            currBlockNum = blockNum;
            while (rc->getBlocksLiveOnExit().get(currBlockNum))
               {
               if (!intervalStartsAtEBBEntry)
                  {
                  liveOnExitConflicts[regNum].set(currBlockNum);
                  if(trace)
                    {
                    traceMsg(comp," Exit(%d)",currBlockNum);
                    }
                  }

               curr = curr->getNextBlock();
               currBlockNum = curr ? curr->getNumber(): 0;

               if (!curr || !curr->isExtensionOfPreviousBlock() ||
                   !rc->getBlocksLiveOnEntry().get(currBlockNum))
                   break;

               liveOnEntryConflicts[regNum].set(currBlockNum);
               if(trace)
                 {
                 traceMsg(comp," Entry(%d)",currBlockNum);
                 }
               }

            if(trace)
              {
              traceMsg(comp," }\n");
              }
            }
         }
      }

   if (trace)
     traceMsg(comp, "\t Update conflicts on entry due to conflicts on predecessor exits:\n");

   int32_t i;
   for (i = firstRegister; i <= lastRegister; ++i)
      {
      if(trace)
         {
         traceMsg(comp,"\t    reg %d: {",i);
         }
      TR_BitVectorIterator bvi(liveOnExitConflicts[i]);
      while (bvi.hasMoreElements())
         {
         TR::Block *block = all_blocks[bvi.getNextElement()];
         for (auto e = block->getSuccessors().begin(); e != block->getSuccessors().end(); ++e)
            {
            TR::Block *succ = toBlock((*e)->getTo());

            if (trace)
               {
               if(!liveOnEntryConflicts[i].isSet(succ->getNumber()))
                  traceMsg(comp," Entry(%d)<-Exit(%d)",succ->getNumber(), block->getNumber());
               }

            liveOnEntryConflicts[i].set(succ->getNumber());
            }
         }
      if(trace)
         {
         traceMsg(comp," }\n");
         }
      // In WCode, we allow entry-exit conflicts
      if(trace)
         {
         traceMsg(comp,"\t    reg %d: {",i);
         }
      bvi.setBitVector(entryExitConflicts[i]);
      while (bvi.hasMoreElements())
         {
         TR::Block *block = all_blocks[bvi.getNextElement()];
         for (auto e = block->getSuccessors().begin(); e != block->getSuccessors().end(); ++e)
            {
            TR::Block *succ = toBlock((*e)->getTo());

            if (trace)
               if(!liveOnEntryConflicts[i].isSet(succ->getNumber()))
                  traceMsg(comp," Entry(%d)<-EntryExit(%d)",succ->getNumber(), block->getNumber());

            liveOnEntryConflicts[i].set(succ->getNumber());
            }
         }
      if(trace)
         {
         traceMsg(comp," }\n");
         }
      }

   if (trace)
      {
      traceMsg(comp, "\tDone scanning for conflicts\n");
      TR_BitVectorIterator regIterator(*spilledRegs);
      while (regIterator.hasMoreElements())
         {
         int32_t regNum = regIterator.getNextElement();
         if (firstRegister <= regNum && regNum <= lastRegister)
            {
            traceMsg(comp, "\tFor candidate #%d, on entry block conflicts with register %d ", rc->getSymbolReference()->getReferenceNumber(), regNum);
            liveOnEntryConflicts[regNum].print(comp);
            traceMsg(comp, "\n");
            traceMsg(comp, "\tFor candidate #%d, on exit block conflicts with register %d ", rc->getSymbolReference()->getReferenceNumber(), regNum);
            liveOnExitConflicts[regNum].print(comp);
            traceMsg(comp, "\n");
            }
         }
      }
   }


void TR_RegisterCandidates::collectCfgProperties(TR::Block **blocks, int32_t numberOfBlocks)
  {
   TR::CFG * cfg = comp()->getFlowGraph();


   // Collect firstBlock property. These are BBs that follow the procedure entry.
   _firstBlock.init(cfg->getNextNodeNumber(),_trMemory,stackAlloc,notGrowable);
   _firstBlock.empty();

   TR::CFGNode * start = cfg->getStart();

   for (auto e = start->getSuccessors().begin(); e != start->getSuccessors().end(); ++e)
     {
     TR::CFGNode * b = toBlock((*e)->getTo());
     _firstBlock.set(b->getNumber());
     }


   // Collect ExtensionofPreviousBlock property
   // This seems redundant as the property is a simple flag of the block; however, profiles have shown that
   // indexing blocks followed by extracting information from has significant data cache misses. I'm hopeful
   // that bundling this info together into a bit vector to be referenced repeatedly for each RegisterCandidate
   // will speed up compilation of large programs.
   int i;
   _isExtensionOfPreviousBlock.init(cfg->getNextNodeNumber(),_trMemory,stackAlloc,notGrowable);
   _isExtensionOfPreviousBlock.empty();
   for(i=0;i<numberOfBlocks;i++)
     {
     TR::Block *b = blocks[i];
     if(b != NULL && b->isExtensionOfPreviousBlock())
       _isExtensionOfPreviousBlock.set(b->getNumber());
     }

  }

static void assign_candidate_loop_trace_increment(TR::Compilation *comp, TR_RegisterCandidate * rc, unsigned count)
   {
   bool trace = comp->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);
   if (trace)
      {
      dumpOptDetails(comp, "%d: Inspected %d\n\t", count, rc->getSymbolReference()->getReferenceNumber());
      }
   }

bool
TR_RegisterCandidates::assign(TR::Block ** cfgBlocks, int32_t numberOfBlocks, int32_t & lowestNumber, int32_t & highestNumber)
   {
#if (defined(__IBMCPP__) || defined(__IBMC__)) && !defined(__ibmxl__)
   // __func__ is not defined for this function on XLC compilers (Notably XLC on Linux PPC and ZOS)
   static const char __func__[] = "TR_RegisterCandidates::assign";
#endif
   LexicalTimer t("assign", comp()->phaseTimer());
   //ReferenceTable ot(0, comp()->allocator());
   ReferenceTable ot((ReferenceTableComparator()), (ReferenceTableAllocator(comp()->trMemory()->currentStackRegion())));
   overlapTable = &ot;

   // Init scratch bitvectors for extendLiveRangesForLiveOnExit

   bool useProfilingFrequencies = comp()->getUsesBlockFrequencyInGRA();
   double freqRatio;
   if(useProfilingFrequencies)
      freqRatio = 0.7;
   else
      freqRatio = 1.0;

   bool enableVectorGRA = comp()->cg()->getSupportsVectorRegisters() && !comp()->getOption(TR_DisableVectorRegGRA);

   TR_BitVector catchBlocks(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, growable);
   TR_BitVector callBlocks(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, growable);
   TR_BitVector switchBlocks(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, growable);
   TR_BitVector temp(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, growable);
   TR_BitVector highWordZeroLongs(comp()->getSymRefCount(), trMemory(), stackAlloc, growable);

   TR_BitVector catchBlockLiveLocals(comp()->getSymRefCount(), trMemory(), stackAlloc, growable);
   TR_BitVector nonOSRCatchBlockLiveLocals(comp()->getSymRefCount(), trMemory(), stackAlloc, growable);

   TR_BitVector referencedBlocks(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, growable);
   highWordZeroLongs.setAll(comp()->getSymRefCount());
   int32_t *blockStructureWeight = (int32_t *)trMemory()->allocateStackMemory(comp()->getFlowGraph()->getNextNodeNumber()*sizeof(int32_t));
   memset(blockStructureWeight, 0, comp()->getFlowGraph()->getNextNodeNumber()*sizeof(int32_t));
   bool trace = comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);

   bool catchBlockLiveLocalsExist = false;

   // Visit entire method looking at top nodes recording properties such as blocks with multiple targets,
   // blocks with call, blocks that catch exceptions, and high word candidates
   int32_t blockNum = -1;
   TR::Block *block = NULL;
   bool isCallBlock = false;
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      isCallBlock = false;

      if(node->getOpCodeValue() == TR::treetop)
          node = node->getFirstChild();

      if (node->getOpCode().isStoreDirect())
         {
         if (node->getType().isInt64())
            {
            if (node->getSymbolReference()->getSymbol()->isAutoOrParm() || (0 && node->getSymbolReference()->getSymbol()->isMethodMetaData()))
               {
               if ((!node->getFirstChild()->isHighWordZero() &&
                    !node->getFirstChild()->getOpCode().isLoadConst()) ||
                   (node->getFirstChild()->getOpCode().isLoadConst() &&
                    (node->getFirstChild()->getLongInt() & (((uint64_t)0xffffffff)<<32))))
                 highWordZeroLongs.reset(node->getSymbolReference()->getReferenceNumber());
               }
            }
         else
            highWordZeroLongs.reset(node->getSymbolReference()->getReferenceNumber());
         }

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         if (!node->getBlock()->isExtensionOfPreviousBlock())
            blockNum = block->getNumber();

         TR_BlockStructure *blockStructure = block->getStructureOf();
         int32_t blockWeight = 1;
         if (blockStructure)
            {
            TR::Optimizer * optimizer = (TR::Optimizer *)comp()->getOptimizer();
            optimizer->getStaticFrequency(block, &blockWeight);
            blockStructureWeight[block->getNumber()] = blockWeight;
            }

         if (!block->getExceptionPredecessors().empty())
            {
            catchBlocks.set(blockNum);
            TR_BitVector * liveLocals = block->getLiveLocals();
            if (comp()->cg()->getLiveLocals() && liveLocals)
               {
               catchBlockLiveLocalsExist = true;
               catchBlockLiveLocals |= *liveLocals;
               if (!block->isOSRCatchBlock())
                  nonOSRCatchBlockLiveLocals |= *liveLocals;
               }
            }
         }

      if (node->getOpCode().isJumpWithMultipleTargets())
         switchBlocks.set(blockNum);

      if (node->getOpCode().isResolveCheck() ||
          node->getOpCode().isCall())
         isCallBlock = true;

      if (node->getOpCode().isTreeTop() &&
          (node->getNumChildren() > 0))
         {
         node = node->getFirstChild();
         if (node->getOpCode().isCall())
            isCallBlock = true;
         }

      if (  comp()->cg()->spillsFPRegistersAcrossCalls()
         && isCallBlock
         && (!dontAssignInColdBlocks(comp()) || !block->isCold())
         /* && !node->isTheVirtualCallNodeForAGuardedInlinedCall() */)
         callBlocks.set(blockNum);
      }

      // Mark candidates compatible with high word feature
   TR_BitVectorIterator bvi(highWordZeroLongs);
   int32_t nextLongCandidate;
   while (bvi.hasMoreElements())
      {
      nextLongCandidate = bvi.getNextElement();
      TR::SymbolReference *longVar = comp()->getSymRefTab()->getSymRef(nextLongCandidate);
      if (longVar &&
          longVar->getSymbol()->isAuto() &&
          longVar->getSymbol()->getType().isInt64())
         {
         TR_RegisterCandidate *registerCandidate = comp()->getGlobalRegisterCandidates()->find(longVar);
         if (registerCandidate)
            registerCandidate->setHighWordZero(true);
         }
      }

   bool globalFPAssignmentDone = false;
   highestNumber = -1;
   lowestNumber = INT_MAX;

   TR_RegisterCandidate * rc = _candidates.getFirst(), * first = 0, * next;
   if (rc == 0)
      return globalFPAssignmentDone;

   TR::CodeGenerator * cg = comp()->cg();
   TR::Block * * blocks = cfgBlocks;

   TR_Array<int32_t> numberOfGPRsLiveOnExit(trMemory(), numberOfBlocks, true, stackAlloc);
   TR_Array<int32_t> numberOfFPRsLiveOnExit(trMemory(), numberOfBlocks, true, stackAlloc);
   TR_Array<int32_t> numberOfVRFsLiveOnExit(trMemory(), numberOfBlocks, true, stackAlloc);
   TR_Array<int32_t> maxGPRsLiveOnExit(trMemory(), numberOfBlocks, true, stackAlloc);
   TR_Array<int32_t> maxFPRsLiveOnExit(trMemory(), numberOfBlocks, true, stackAlloc);
   TR_Array<int32_t> maxVRFsLiveOnExit(trMemory(), numberOfBlocks, true, stackAlloc);

   TR::Block * b = comp()->getStartBlock();

   do
      {
      int32_t blockNumber = b->getNumber();
      TR::Node * node = b->getLastRealTreeTop()->getNode();
      maxGPRsLiveOnExit[blockNumber] = cg->getMaximumNumberOfGPRsAllowedAcrossEdge(b);
      maxFPRsLiveOnExit[blockNumber] = cg->getMaximumNumberOfFPRsAllowedAcrossEdge(node);
      maxVRFsLiveOnExit[blockNumber] = cg->getMaximumNumberOfVRFsAllowedAcrossEdge(node);
      } while ((b = b->getNextBlock()));

   int32_t numCandidates = 0;
   TR_Array<int32_t> totalGPRCount(trMemory(), numberOfBlocks,true,stackAlloc);
   TR_Array<int32_t> totalFPRCount(trMemory(), numberOfBlocks,true,stackAlloc);
   TR_Array<int32_t> totalVRFCount(trMemory(), numberOfBlocks,true,stackAlloc);
   TR_Array<int32_t> totalGPRCountOnEntry(trMemory(), numberOfBlocks,true,stackAlloc);
   TR_Array<int32_t> totalFPRCountOnEntry(trMemory(), numberOfBlocks,true,stackAlloc);
   TR_Array<int32_t> totalVRFCountOnEntry(trMemory(), numberOfBlocks,true,stackAlloc);

   int32_t i;
   for (i = 0; i < numberOfBlocks; ++i)
      {
      totalGPRCount[i] = 0;
      totalFPRCount[i] = 0;
      totalVRFCount[i] = 0;
      totalGPRCountOnEntry[i] = 0;
      totalFPRCountOnEntry[i] = 0;
      totalVRFCountOnEntry[i] = 0;
      }

   // Collect properties of the CFG that are used in candidate heuristics but are invariant to the candidate
   // and only depend on the structure of the CFG. This will speed up computing the heuristics.
   collectCfgProperties(blocks,numberOfBlocks);

   // prioritize the register candidates
   //
   for (; rc; rc = next)
      {
      next = rc->getNext();

      rc->setWeight(blocks, blockStructureWeight, comp(), totalGPRCount,totalFPRCount, totalVRFCount, &referencedBlocks, _startOfExtendedBBForBB,
                    _firstBlock, _isExtensionOfPreviousBlock);
      numCandidates++;

      dumpOptDetails(comp(), "Weight of candidate (symRef #%d %s) is %d\n", rc->getSymbolReference()->getReferenceNumber(),
                       rc->getSymbolReference()->getSymbol()->isMethodMetaData() ? rc->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName(): "",
                       rc->getWeight());
      bool isFloat = (rc->getDataType() == TR::Float
                      || rc->getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                      || rc->getDataType() == TR::DecimalFloat
                      || rc->getDataType() == TR::DecimalDouble
                      || rc->getDataType() == TR::DecimalLongDouble
#endif
                      );
      bool isVector = rc->getDataType().isVector();
      bool needs2Regs = false;

      if (rc->rcNeeds2Regs(comp())) needs2Regs = true;

      TR_BitVector seenBlocks(numberOfBlocks, trMemory(), stackAlloc, growable);
      TR_BitVectorIterator bvi(rc->getBlocksLiveOnExit());
      while (bvi.hasMoreElements())
         {
         int32_t nextElement = bvi.getNextElement();
         TR::Block * b = blocks[nextElement];
         TR_ASSERT(nextElement == b->getNumber(),"blocks[x]->getNumber() != x");
           int32_t blockNum = nextElement;
         if (!seenBlocks.get(blockNum))
            {
            seenBlocks.set(blockNum);
            if (isFloat)
               {
               ++totalFPRCount[blockNum];
               if (needs2Regs)
                  ++totalFPRCount[blockNum];
               }
            else if (isVector)
               {
               ++totalVRFCount[blockNum];
               }
            else
               {
               ++totalGPRCount[blockNum];
               //dumpOptDetails(comp(), "Block %d has %d gprs live\n", blockNum, totalGPRCount[blockNum]);
               if (needs2Regs)
                  ++totalGPRCount[blockNum];
               }
            }
         }

      seenBlocks.empty();
      bvi.setBitVector(rc->getBlocksLiveOnEntry());
      while (bvi.hasMoreElements())
         {
         int32_t nextElement = bvi.getNextElement();
         TR::Block * b = blocks[nextElement];
         int32_t blockNum = b->getNumber();
         if (!seenBlocks.get(blockNum))
            {
            seenBlocks.set(blockNum);
            if (isFloat)
               {
               ++totalFPRCountOnEntry[blockNum];
               if (needs2Regs)
                  ++totalFPRCountOnEntry[blockNum];
               }
            else if (isVector)
               {
               ++totalVRFCountOnEntry[blockNum];
               }
            else
               {
               ++totalGPRCountOnEntry[blockNum];
               // dumpOptDetails(comp(), "Block %d has %d gprs live\n", blockNum, totalGPRCount[blockNum]);
               if (needs2Regs)
                  ++totalGPRCountOnEntry[blockNum];
               }
            }
         }

      prioritizeCandidate(rc, first);
      }

   if (trace)
      {
      traceMsg(comp(),"Prioritized list of candidates\n");
      for (rc = first; rc; rc = rc->getNext())
         {
         traceMsg(comp()," Candidate #%d (weight=%d)\n",rc->getSymbolReference()->getReferenceNumber(),rc->getWeight());
         }
      }



   // Reuse arrays for below
   if (true)
      {
      for(int i=0;i < numberOfBlocks;++i)
         {
         totalGPRCount[i] = 0;
         totalFPRCount[i] = 0;
         totalVRFCount[i] = 0;
         totalGPRCountOnEntry[i] = 0;
         totalFPRCountOnEntry[i] = 0;
         totalVRFCountOnEntry[i] = 0;
         }
      }

   int32_t limitedGRACandidateMax = comp()->getOptions()->getMaxLimitedGRACandidates();
   int32_t limitedGRACandidateCount = 0;

   uint8_t maxReprioritized = 0; // 0 seems to give better throughput and lower CPU than 1
   if (comp()->getMethodHotness() >= veryHot)
      {
      maxReprioritized = 8;
      }
   else if (comp()->getMethodHotness() >= hot)
      {
      maxReprioritized = 4;
      }

   int32_t numCands = 0;
   for (rc = first; rc; rc = next)
      {
      numCands++;
      next = rc->getNext();
      rc->setMaxReprioritized(maxReprioritized);
      }

   // initialize the registerUsage bit vectors
   //
   int32_t numberOfGlobalRegisters = cg->getNumberOfGlobalRegisters();
   _liveOnEntryUsage.init(trMemory(), numberOfGlobalRegisters, true, stackAlloc);
   _liveOnExitUsage.init(trMemory(), numberOfGlobalRegisters, true, stackAlloc);

   for (i = _liveOnEntryUsage.internalSize() - 1; i >= 0; --i)
      {
      _liveOnEntryUsage[i].init(numberOfBlocks, trMemory(), stackAlloc, growable);
      _liveOnExitUsage[i].init(numberOfBlocks, trMemory(), stackAlloc, growable);
      }
   cg->setUnavailableRegistersUsage(_liveOnEntryUsage, _liveOnExitUsage);

   _liveOnEntryConflicts.init(trMemory(), numberOfGlobalRegisters, true, stackAlloc);
   _liveOnExitConflicts.init(trMemory(), numberOfGlobalRegisters, true, stackAlloc);
   _entryExitConflicts.init(trMemory(), numberOfGlobalRegisters, true, stackAlloc);
   _exitEntryConflicts.init(trMemory(), numberOfGlobalRegisters, true, stackAlloc);
#ifdef TRIM_ASSIGNED_CANDIDATES
   _availableBlocks.init(trMemory(), numberOfGlobalRegisters, true, stackAlloc);
#endif
   for (i = 0; i < numberOfGlobalRegisters; i++)
      {
      _liveOnEntryConflicts[i].init(numberOfBlocks, trMemory(), stackAlloc, growable);
      _liveOnExitConflicts[i].init(numberOfBlocks, trMemory(), stackAlloc, growable);
      _entryExitConflicts[i].init(numberOfBlocks, trMemory(), stackAlloc, growable);
      _exitEntryConflicts[i].init(numberOfBlocks, trMemory(), stackAlloc, growable);
#ifdef TRIM_ASSIGNED_CANDIDATES
      _availableBlocks[i].init(numberOfBlocks, trMemory(), stackAlloc, growable);
#endif
      }

   TR_BitVector intersection(numberOfBlocks, trMemory(), stackAlloc, growable);

#ifdef TRIM_ASSIGNED_CANDIDATES
   // Count number of loads and stores in each loop
   // and create loop info for each candidate
   //
   _candidates.setFirst(0);
   _candidateForSymRefs->clear();
   for (rc = first; rc; rc = rc->getNext())
      {
      rc->countNumberOfLoadsAndStoresInLoops(trMemory());
      if (trace)
         rc->printLoopInfo(comp());
      }
#endif

   // assign the register candidates to global registers
   //
   _candidates.setFirst(0);
   _candidateForSymRefs->clear();
   unsigned iterationCount = 0;
   int32_t conflictingRegister = -1;
   int32_t numAssigns = 0;

   for (rc = first; rc; assign_candidate_loop_trace_increment(comp(), rc, iterationCount), rc = next)
      {
      next = rc->getNext();

      if (comp()->getOption(TR_EnableGRACostBenefitModel))
         {
         static const char * a = feGetEnv("TR_GRAWeightThreshold");
         int32_t weightThresholdFactor = a ? atoi(a) : 10;
         int32_t weightThreshold = first->getWeight()/weightThresholdFactor;
         static const char * b = feGetEnv("TR_GRANumThreshold");
         int32_t numCandsThresholdPercent = b ? atoi(b) : 80;
         float numCandsThresholdFactor = (float) ((float) numCandsThresholdPercent / (float) 100);

         numAssigns++;

         if ((rc->getWeight() < weightThreshold) && (((float) numAssigns) > (float) (numCandsThresholdFactor * (float) numCands)))
            {
            if(trace)
               traceMsg(comp(),"Leaving candidate because the compile time cost is not worth it\n");

            continue;
            }
         }

      if (((++iterationCount & 0xf) == 0))
         {
         if (comp()->compilationShouldBeInterrupted(GRA_ASSIGN_CONTEXT))
            {
            comp()->failCompilation<TR::CompilationInterrupted>("interrupted in GRA");
            }
         }

      TR::DataType type = rc->getType();
      TR::DataType dt = rc->getDataType();

      if (type.isInt64() && cg->getDisableLongGRA())
         {
         if (trace)
            traceMsg(comp(),"Leaving candidate because LongGRA is disabled and candidate is 64 bit\n");
         continue;
         }

      if (dt == TR::Aggregate && (1 || rc->getSymbolReference()->getSymbol()->getSize() > 8))
         {
         if (trace)
            traceMsg(comp(),"Leaving candidate because its an aggregate and > 64 bits\n");
         continue;
         }

      if ((!rc->getSymbolReference()->getSymbol()->isAutoOrParm())
           ||
           rc->getSymbolReference()->getSymbol()->holdsMonitoredObject())
         {
         if (trace)
            traceMsg(comp(),"Leaving candidate because it holdsMonitoredObject\n");
         continue; // todo: handle statics and fields?
         }

      // exclude symbols that can define other symbols
      if (aliasesPreventAllocation(comp(),rc->getSymbolReference()))
         {
         if (trace)
            traceMsg(comp(),"Leaving candidate because it has use_def_aliases\n");
         continue;
         }

      if (dt.isVector() && !comp()->cg()->hasGlobalVRF())
         {
         if (trace)
            traceMsg(comp(),"Leaving candidate because it has vector type but no global vector registers provided\n");
         TR_ASSERT(!TR::Compiler->target.cpu.isZ(),"ed : debug : Should never get here for vector GRA on z");
         continue;
         }

      // don't put this auto into a global register if it can be accessed from a catch clause
      //
      temp = rc->getBlocksLiveOnEntry();
      temp &= catchBlocks;

      if (((catchBlockLiveLocalsExist && rc->getSymbolReference()->getSymbol()->isAuto() && catchBlockLiveLocals.get(rc->getSymbolReference()->getSymbol()->getAutoSymbol()->getLiveLocalIndex())) ||
           ((!catchBlockLiveLocalsExist || !rc->getSymbolReference()->getSymbol()->isAuto()) && !rc->getSymbolReference()->getUseonlyAliases().isZero(comp()))) &&
          (!comp()->penalizePredsOfOSRCatchBlocksInGRA() ||
           ((catchBlockLiveLocalsExist && rc->getSymbolReference()->getSymbol()->isAuto() && nonOSRCatchBlockLiveLocals.get(rc->getSymbolReference()->getSymbol()->getAutoSymbol()->getLiveLocalIndex())) ||
            ((!catchBlockLiveLocalsExist || !rc->getSymbolReference()->getSymbol()->isAuto()) && !comp()->getSymRefTab()->aliasBuilder.hasUseonlyAliasesOnlyDueToOSRCatchBlocks(rc->getSymbolReference())))))
         //TODO:GRA Decide if this fix is OK -- without it on zemulator we would have a useonlyalias set for metadata and so we would set this propert
         // and then later on not get rid of the store ot metadata, and create a new store to global reg
         rc->setLiveAcrossExceptionEdge(true);

      if (!temp.isEmpty())
         {
         TR_BitVectorIterator bvi(rc->getBlocksLiveOnEntry());
         while (bvi.hasMoreElements())
            {
            int32_t nextElement = bvi.getNextElement();
            TR::Block * b = blocks[nextElement];
            if (!b->getExceptionPredecessors().empty())
               rc->getBlocksLiveOnEntry().reset(nextElement);
            }
         //continue;
         }

      bool isFloat = (dt == TR::Float
                      || dt == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                      || dt == TR::DecimalFloat
                      || dt == TR::DecimalDouble
                      || dt == TR::DecimalLongDouble
#endif
                      );
      bool isVector = dt.isVector();
      int32_t firstRegister, lastRegister;
      
      if (isFloat)
         {
           if (debug("disableGlobalFPRs")
               || cg->getDisableFpGRA()
#ifdef J9_PROJECT_SPECIFIC
               || (dt == TR::DecimalLongDouble)
#endif
               )
              {
              continue;
              }

         // Cannot keep FP values on FP stack across a switch on IA32
         //
         if (!comp()->cg()->getSupportsJavaFloatSemantics())
            {
            temp = rc->getBlocksLiveOnEntry();
            temp |= rc->getBlocksLiveOnExit();
            temp &= switchBlocks;
            if (!temp.isEmpty())
               {
               //printf("Discarding FP candidate in %s\n", comp()->getCurrentMethod()->signature());
               continue;
               }
            }

         if (0 && comp()->cg()->spillsFPRegistersAcrossCalls())
            {
            temp = rc->getBlocksLiveOnEntry();
            temp &= rc->getBlocksLiveOnExit();
            temp &= callBlocks;
            if (!temp.isEmpty())
               {
               //dumpOptDetails(comp(), "Discarding FP candidate %d\n", rc->getSymbolReference()->getReferenceNumber());
               //printf("Discarding FP candidate in %s\n", comp()->getCurrentMethod()->signature());
               continue;
               }
            }

         firstRegister = cg->getFirstGlobalFPR(), lastRegister = cg->getLastGlobalFPR();
         }
      else if (isVector)
         {
         firstRegister = cg->getFirstGlobalVRF(), lastRegister = cg->getLastGlobalVRF();
         }
      else
         {
         firstRegister = cg->getFirstGlobalGPR(), lastRegister = cg->getLastGlobalGPR();
         }

      // check to see if max registers across branches is exceeded.
      // Done in descending order so we allocate the hot ones first
      if (true)
         {
         bool needs2Regs = false;
         if (rc->rcNeeds2Regs(comp())) needs2Regs = true;

         int32_t blockNum;
         bool sufficientRegisters=true;
         TR_BitVectorIterator bvi(rc->getBlocksLiveOnExit());

         if (trace)
            dumpOptDetails(comp(), "%d: Inspecting %d %s\t(weight %d)\n\t", iterationCount, rc->getSymbolReference()->getReferenceNumber(), rc->getSymbolReference()->getSymbol()->isParm() ? "parm ":"", rc->getWeight());

         while (bvi.hasMoreElements())
            {
            int32_t nextElement = bvi.getNextElement();
            TR_ASSERT(blocks[nextElement]->getNumber() == nextElement,"blocks[x]->getNumber() is not x");
            blockNum = nextElement;

            //dumpOptDetails(comp(), "For block_%d maxGPRs %d maxFPRs %d ext block_%d maxGPRs %d maxFPRs %d maxVRFs %d\n", b->getNumber(), maxGPRsLiveOnExit[b->getNumber()], maxFPRsLiveOnExit[b->getNumber()],  blockNum, maxGPRsLiveOnExit[blockNum], maxFPRsLiveOnExit[blockNum], maxFPRsLiveOnExit[blockNum]);
            //dumpOptDetails(comp(), "For block_%d numGPRs %d numFPRs %d ext block_%d numGPRs %d numFPRs %d maxVRFs %d\n", b->getNumber(), totalGPRCount[b->getNumber()], totalFPRCount[b->getNumber()],  blockNum, totalGPRCount[blockNum], totalFPRCount[blockNum], totalVRFCount[blockNum]);

            if (isFloat)
               {
               if (totalFPRCount[blockNum] >= (maxFPRsLiveOnExit[blockNum] - (needs2Regs ? 1 : 0)))
                  {
                  if (trace)
                     dumpOptDetails(comp(), "FPR count exceeded at some exit out of extended block_%d: %d >= %d\n",blockNum,totalFPRCount[blockNum],
                                     maxFPRsLiveOnExit[blockNum]);


                  bool resetExit = true;
                  bool hasMultiWayBranch = false;

                  TR::TreeTop *exitTree = blocks[blockNum]->getExit(); //Entry()->getExtendedBlockExitTreeTop();
                  TR::TreeTop *cursorExit;
                  TR::Block *cursorBlock = blocks[blockNum];
                  TR::Node * cursorNode = cursorBlock->getLastRealTreeTop()->getNode();
                  if(cursorNode->getOpCodeValue() == TR::treetop)
                     cursorNode = cursorNode->getFirstChild();

                  if (cursorNode->getOpCode().isJumpWithMultipleTargets())
                     {
                     hasMultiWayBranch = true;
                     }

                  TR::Block *problematicBlock = blocks[blockNum];
                  TR_SuccessorIterator ebsi(problematicBlock);
                  for (TR::CFGEdge * e = ebsi.getFirst(); e; e = ebsi.getNext())
                     {
                     TR::Block *b = e->getTo()->asBlock();
                     if (hasMultiWayBranch || (totalFPRCountOnEntry[b->getNumber()] >=  (maxFPRsLiveOnExit[blockNum] - (needs2Regs ? 1 : 0))))
                        {
                        if (trace)
                           dumpOptDetails(comp(), "FPR count exceeded : resetting live on entry block_%d (because of extended block_%d)\n", b->getNumber(), blockNum);
                        rc->getBlocksLiveOnEntry().reset(b->getNumber());
                        TR::Block *cursorBlock = problematicBlock;
                        if (trace)
                           dumpOptDetails(comp(), "FPR count exceeded : resetting live on exit block_%d)\n", cursorBlock->getNumber());

                        rc->getBlocksLiveOnExit().reset(cursorBlock->getNumber());
                        for (auto succ = cursorBlock->getSuccessors().begin(); succ != cursorBlock->getSuccessors().end(); ++succ)
                            {
                            rc->getBlocksLiveOnEntry().reset((*succ)->getTo()->getNumber());
                            }
                        }
                     }

                  //sufficientRegisters=false;
                  //break;
                  }
               //++totalFPRCount[blockNum];
               }
            if (isVector)
               {
               if (totalVRFCount[blockNum] >= maxVRFsLiveOnExit[blockNum])
                  {
                  if (trace)
                     dumpOptDetails(comp(), "VRF count exceeded at some exit out of extended block_%d: %d >= %d\n", blockNum, totalVRFCount[blockNum], maxVRFsLiveOnExit[blockNum]);

                  bool resetExit = true;
                  bool hasMultiWayBranch = false;

                  TR::TreeTop *exitTree = blocks[blockNum]->getExit();
                  TR::TreeTop *cursorExit;
                  TR::Block *cursorBlock = blocks[blockNum];
                  TR::Node * cursorNode = cursorBlock->getLastRealTreeTop()->getNode();
                  if (cursorNode->getOpCodeValue() == TR::treetop)
                     cursorNode = cursorNode->getFirstChild();

                  if (cursorNode->getOpCode().isJumpWithMultipleTargets())
                     hasMultiWayBranch = true;

                  TR::Block *problematicBlock = blocks[blockNum];
                  TR_SuccessorIterator ebsi(problematicBlock);
                  for (TR::CFGEdge * e = ebsi.getFirst(); e; e = ebsi.getNext())
                     {
                     TR::Block *b = e->getTo()->asBlock();
                     if (hasMultiWayBranch || (totalVRFCountOnEntry[b->getNumber()] >= maxVRFsLiveOnExit[blockNum]))
                        {
                        if (trace)
                           dumpOptDetails(comp(), "VRF count exceeded : resetting live on entry block_%d (because of extended block_%d)\n", b->getNumber(), blockNum);
                        rc->getBlocksLiveOnEntry().reset(b->getNumber());

                        TR::Block *cursorBlock = problematicBlock;
                        if (trace)
                           dumpOptDetails(comp(), "VRF count exceeded : resetting live on exit block_%d)\n", cursorBlock->getNumber());

                        rc->getBlocksLiveOnExit().reset(cursorBlock->getNumber());
                        for (auto succ = cursorBlock->getSuccessors().begin(); succ != cursorBlock->getSuccessors().end(); ++succ)
                            rc->getBlocksLiveOnEntry().reset((*succ)->getTo()->getNumber());
                        }
                     }
                  }
               }
            else
               {
               if (totalGPRCount[blockNum] >= (maxGPRsLiveOnExit[blockNum] - (needs2Regs ? 1 : 0)))
                  {
                  if (trace)
                     dumpOptDetails(comp(), "GPR count exceeded at some exit out of extended block_%d: %d >= %d\n",blockNum,totalGPRCount[blockNum],
                                  maxGPRsLiveOnExit[blockNum]);

                  bool hasMultiWayBranch = false;

                  TR::TreeTop *exitTree = blocks[blockNum]->getExit(); //Entry()->getExtendedBlockExitTreeTop();
                  TR::TreeTop *cursorExit;
                  TR::Block *cursorBlock = blocks[blockNum];
                  TR::Node * cursorNode = cursorBlock->getLastRealTreeTop()->getNode();
                  if (cursorNode->getOpCodeValue() == TR::treetop)
                     cursorNode = cursorNode->getFirstChild();

                  if (cursorNode->getOpCode().isJumpWithMultipleTargets())
                     {
                     hasMultiWayBranch = true;
                     }

                  TR::Block *problematicBlock = blocks[blockNum];
                  TR_SuccessorIterator ebsi(problematicBlock);
                  for (TR::CFGEdge * e = ebsi.getFirst(); e; e = ebsi.getNext())
                     {
                     TR::Block *b = e->getTo()->asBlock();
                     if (hasMultiWayBranch || (totalGPRCountOnEntry[b->getNumber()] >=  (maxGPRsLiveOnExit[blockNum] - (needs2Regs ? 1 : 0))))
                        {
                        if (trace)
                           dumpOptDetails(comp(), "GPR count exceeded : resetting live on entry block_%d (because of extended block_%d)\n", b->getNumber(), blockNum);
                        rc->getBlocksLiveOnEntry().reset(b->getNumber());
                        TR::Block *cursorBlock = problematicBlock;
                        if (trace)
                            dumpOptDetails(comp(), "GPR count exceeded : resetting live on exit block_%d)\n", cursorBlock->getNumber());

                        rc->getBlocksLiveOnExit().reset(cursorBlock->getNumber());
                        for (auto succ = cursorBlock->getSuccessors().begin(); succ != cursorBlock->getSuccessors().end(); ++succ)
                            {
                            rc->getBlocksLiveOnEntry().reset((*succ)->getTo()->getNumber());
                            }
                        }
                     }

                  //sufficientRegisters=false;
                  //break;
                  }
               //++totalGPRCount[blockNum];
               }
            }
         if (!sufficientRegisters)
            continue;
         }

      if (trace) // Feel this would be useful by default
         {
         dumpOptDetails(comp(), "\nFor candidate %d : trials left %d weight : %d\n", rc->getSymbolReference()->getReferenceNumber(), rc->getReprioritized(), rc->getWeight());
         dumpOptDetails(comp(), "live on entry : ");
         rc->getBlocksLiveOnEntry().print(comp());
         dumpOptDetails(comp(), "\n");
         dumpOptDetails(comp(), "live on exit : ");
         rc->getBlocksLiveOnExit().print(comp());
         dumpOptDetails(comp(), "\n");
         }
      //
      // Compute available registers
      //
      TR_BitVector availableRegisters(lastRegister+1, trMemory(), stackAlloc);
      computeAvailableRegisters(rc, firstRegister, lastRegister, blocks, &availableRegisters);
      if (trace)
         {
         traceMsg(comp(), "available registers : ");
         availableRegisters.print(comp());
         traceMsg(comp(), "\n");
         }
      TR::CodeGenerator * cg = comp()->cg();
      cg->removeUnavailableRegisters(rc, blocks, availableRegisters);

      static const char * vrbVecGRA = feGetEnv("TR_traceVectorGRA");

      if (enableVectorGRA)
         {
         for (int i = firstRegister; i <= lastRegister; ++i)
            {
            if (vrbVecGRA && (cg->isGlobalFPR(i) || cg->isGlobalVRF(i)) && cg->isAliasedGRN(i))
               {
               dumpOptDetails(comp(),"ed: alias for %d %s %d %s\t",
                     i,
                     cg->getDebug()->getGlobalRegisterName(i),
                     cg->getOverlappedAliasForGRN(i),
                     cg->getDebug()->getGlobalRegisterName(cg->getOverlappedAliasForGRN(i)));
               dumpOptDetails(comp(),"ffpr %d lfpr %d fopf %d lopr %d fvrf %d lvrf %d fovrf %d lovrf %d\n",
                     cg->getFirstGlobalFPR(),
                     cg->getLastGlobalFPR(),
                     cg->getFirstOverlappedGlobalFPR(),
                     cg->getLastOverlappedGlobalFPR(),
                     cg->getFirstGlobalVRF(),
                     cg->getLastGlobalVRF(),
                     cg->getFirstOverlappedGlobalVRF(),
                     cg->getLastOverlappedGlobalVRF());
               }
            }
         // Remove the aliases for all used FPRs (or first 16 VRFs)
         for (int i = firstRegister; i <= lastRegister; ++i)
            {
            if (vrbVecGRA && (cg->isGlobalFPR(i) || cg->isGlobalVRF(i)) && cg->isAliasedGRN(i))
               {
               dumpOptDetails(comp(),"ed: rc sym ref: %d\t", rc->getSymbolReference()->getReferenceNumber());
               dumpOptDetails(comp(),"i %d\t", i);
               dumpOptDetails(comp(),"set? %d\t", !availableRegisters.isSet(i));
               dumpOptDetails(comp(),"alias set? %d\t", !availableRegisters.isSet(cg->getOverlappedAliasForGRN(i)));
               dumpOptDetails(comp(),"aliased grn? %d\t", cg->isAliasedGRN(i));
               dumpOptDetails(comp(), "avail (check is set?):\n");
               availableRegisters.print(comp());
               dumpOptDetails(comp(), "\n");
               }
            if ((cg->isGlobalFPR(i) || cg->isGlobalVRF(i)) && cg->isAliasedGRN(i) &&
                  (/*!availableRegisters.isSet(i) */!availableRegisters.isSet(cg->getOverlappedAliasForGRN(i))))
               {
               TR_GlobalRegisterNumber overlappedReg = cg->getOverlappedAliasForGRN(i);
               if (trace)
                  {
                  traceMsg(comp(),
                        "removing %s (%d) from available list since it is aliased with %s (%d)\n",
                        cg->getDebug()->getGlobalRegisterName(i),i,
                        cg->getDebug()->getGlobalRegisterName(overlappedReg),overlappedReg);
                  }
               availableRegisters.reset(overlappedReg);
               availableRegisters.reset(i);
               }
            }
         }

      if (trace) // Feel this would be useful by default
         {
         dumpOptDetails(comp(), "available registers : ");
         availableRegisters.print(comp());
         dumpOptDetails(comp(), "\n");
         }


      bool needs2Regs = false;
      if (rc->rcNeeds2Regs(comp())) needs2Regs = true;

      TR_GlobalRegisterNumber otherRegisterNumber = 0;
      TR_GlobalRegisterNumber highRegisterNumber = -1;

      TR_GlobalRegisterNumber registerNumber;
      {
      LexicalTimer t("pickRegister", comp()->phaseTimer());
      registerNumber = cg->pickRegister(rc, blocks, availableRegisters, otherRegisterNumber, &_candidates);
      if (needs2Regs && (registerNumber > -1))
         {
         otherRegisterNumber = 1;
         availableRegisters.reset(registerNumber);
         highRegisterNumber = cg->pickRegister(rc, blocks, availableRegisters, otherRegisterNumber, &_candidates);
         availableRegisters.set(registerNumber);
         }
      }

     if ((registerNumber == -1) ||
         (needs2Regs &&
          (highRegisterNumber == -1)))
        {
        if (!rc->canBeReprioritized()) // only reprioritize a certain number of times based on hotness
           {
           if (trace)
              traceMsg(comp(), "Can't reprioritize anymore\n");

           }
        // If there's no available registers then attempt to remove some of the blocks from the
        // candidate to avoid conflicts with other candidates
        //
        else
           {
           // find the global register with the fewest conflicts
           //
           conflictingRegister = -1;
           int32_t secondConflictingRegister = -1;
           int32_t numberOfConflicts = -1;
           int32_t smallestMaxFrequency = -1;
           int32_t maxClearStructures = -1; // largest number of clear structures of the unavailable registers

           intersection.empty();
           List<TR_Structure> regStructureUses(trMemory());   // list of deepest loops where a register is currently live
           List<TR_Structure> candStructureUses(trMemory());  // list of loops where this candidate is currently live

           // loop over the blocks where this candidate is live, and add the loops that contain those blocks to candStructureUses
           TR_BitVectorIterator bvi(rc->getBlocksLiveOnEntry());
           while (bvi.hasMoreElements())
              {
              int32_t blockNumber = bvi.getNextElement();
              TR::Block * block = blocks[blockNumber];
              TR_BlockStructure *blockStructure = block->getStructureOf();

              if (blockStructure)
                 {
                 TR_Structure *containingLoop = blockStructure->getContainingLoop();
                 if (containingLoop && !candStructureUses.find(containingLoop))
                    candStructureUses.add(containingLoop);
                 }
              }

           // We'll use the _liveOnEntryConflicts array element to maintain the
           // list of block numbers to remove from the candidate's liveOnEntry list.

           const bool useRegisterPressureInfo = !comp()->getOption(TR_DisableRegisterPressureSimulation) && cg->hasRegisterPressureInfo() && (!trace || performTransformation(comp(), "%s Including register pressure info for trimming live range of #%d\n", OPT_DETAILS, rc->getSymbolReference()->getReferenceNumber()));
           if (useRegisterPressureInfo)
              {
              // Add blocks the register pressure simulator found to be problematic.
              //
              TR_BitVector spilledRegs(cg->getNumberOfGlobalRegisters(), trMemory(), stackAlloc);
              TR_BitVector liveOnEntryOrExit(numberOfBlocks, trMemory(), stackAlloc);
              liveOnEntryOrExit = rc->getBlocksLiveOnEntry();
              liveOnEntryOrExit |= rc->getBlocksLiveOnExit();
              scanPressureSimulatorCacheForConflicts(rc, liveOnEntryOrExit, _liveOnEntryConflicts, _liveOnExitConflicts, _entryExitConflicts, firstRegister, lastRegister, &spilledRegs, comp(), blocks);
              }

           for (i = firstRegister; i <= lastRegister; ++i)
              {
              if (trace)
                 {
                 traceMsg(comp(),"Searching for blocks/structures with max frequency for reg: %d\n",i);
                 }
              if (!useRegisterPressureInfo && availableRegisters.get(i))
                 {
                 // Without register pressure information, we have no idea
                 // what to do to make pickRegister pick an availableRegister,
                 // so we must give up on them.
                 //
                 continue;
                 }

              _liveOnEntryConflicts[i] |= _exitEntryConflicts[i];
#ifdef TRIM_ASSIGNED_CANDIDATES
              _availableBlocks[i].empty();
#endif

        // This code section moved to scanPressureSimulatorCacheForConflicts

              if (trace)
                 {
                 traceMsg(comp(), "Final live on entry conflicts with register : %d\n", i);
                 _liveOnEntryConflicts[i].print(comp());
                 traceMsg(comp(), "\n");
                 }

              int32_t numBlocksAtMaxFrequency = 0;
              int32_t maxFrequency = 0;
              int32_t maxBlock = -1;

              if (!needs2Regs)
                 {
                 bvi.setBitVector(_liveOnEntryConflicts[i]);
                 while (bvi.hasMoreElements())
                    {
                    int32_t blockNumber = bvi.getNextElement();
                    TR::Block * block = blocks[blockNumber];
                    TR_BlockStructure *blockStructure = block->getStructureOf();
                    int32_t blockWeight = 1;
                    TR_BitVector *autosInBlock = getBlocksReferencingSymRef(rc->getSymbolReference()->getReferenceNumber());

                    if (blockStructure)
                       {
                       TR_Structure *containingLoop = blockStructure->getContainingLoop();
#ifdef TRIM_ASSIGNED_CANDIDATES
                       if (containingLoop && isInnerLoop(comp(), containingLoop))
                          {
                          TR_RegisterCandidate * regCand = block->getGlobalRegisters(comp())[i].getRegisterCandidateOnEntry();

                          if (regCand)
                             {
                             int32_t currCandNumLoadsAndStores = rc->getNumberOfLoadsAndStoresInLoop(containingLoop);
                             int32_t regCandNumLoadsAndStores = regCand->getNumberOfLoadsAndStoresInLoop(containingLoop);
                             if (regCandNumLoadsAndStores == 0 &&
                                 currCandNumLoadsAndStores != 0)
                                {
                                _availableBlocks[i].set(blockNumber);
                                }
                             }
                          }
#endif
                       if (autosInBlock && autosInBlock->get(block->getNumber()))
                          {
                          // blockWeight = blockStructureWeight[block->getNumber()];
                          blockWeight = rc->_blocks.getNumberOfLoadsAndStores(block->getNumber());

                          if (blockWeight > maxFrequency)
                             {
                             maxFrequency = blockWeight;
                             numBlocksAtMaxFrequency = 1;
                             maxBlock = i;

                             // collect all loops with blocks at this frequency where this register is live
                             regStructureUses.deleteAll();
                             regStructureUses.add(containingLoop);
                             }
                          else if (blockWeight >= maxFrequency*freqRatio)
                             {
                             numBlocksAtMaxFrequency++;

                             // collect loops with blocks at this frequency where this register is live
                             if (!regStructureUses.find(containingLoop))
                                regStructureUses.add(containingLoop);
                             }
                          }
                       }
                    }

                 if (trace)
                   {
                   traceMsg(comp(), "block_%d has max freq (%d)\n", maxBlock, maxFrequency);
                   }

                 if (numBlocksAtMaxFrequency == 0)
                    numBlocksAtMaxFrequency = _liveOnEntryConflicts[i].elementCount();
                 }

              // figure out in how many of our candidate's structures
              // could we allocate this register ("clear" structures)
              // i.e. how many structures are in candStructureUses but not in regStructureUses
              int32_t numClearStructures = 0;
              ListIterator<TR_Structure> si(&candStructureUses);
              for (TR_Structure *s=si.getFirst(); s != NULL; s=si.getNext())
                 {
                 if (!regStructureUses.find(s))
                    numClearStructures++;
                 }
#ifdef TRIM_ASSIGNED_CANDIDATES
              if (!_availableBlocks[i].isEmpty())
                 {
                 numClearStructures++; // TODO: improve this
                 }
#endif
              if (needs2Regs)
                 {
                 int32_t j;
                 for (j = firstRegister; j < i; ++j)
                    if (!availableRegisters.get(j))
                       {
                       // use the _liveOnEntryConflicts array element to maintain the list of block
                       // numbers to remove from the candidate's liveOnEntry list.
                       //
                       intersection = _liveOnEntryConflicts[i];
                       intersection &= _liveOnEntryConflicts[j];

                       bvi.setBitVector(intersection);
                       while (bvi.hasMoreElements())
                          {
                          int32_t blockNumber = bvi.getNextElement();
                          TR::Block * block = blocks[blockNumber];
                          TR_BlockStructure *blockStructure = block->getStructureOf();
                          int32_t blockWeight = 1;

                          if (blockStructure)
                             {
                             // blockWeight = blockStructureWeight[block->getNumber()];
                             blockWeight = rc->_blocks.getNumberOfLoadsAndStores(block->getNumber());
                             if (blockWeight > maxFrequency)
                                {
                                maxFrequency = blockWeight;
                                numBlocksAtMaxFrequency = 1;
                                }
                             else if (blockWeight >= maxFrequency*freqRatio)
                                numBlocksAtMaxFrequency++;
                             }
                          }

                       if (numBlocksAtMaxFrequency == 0)
                          numBlocksAtMaxFrequency = intersection.elementCount();

                       int32_t intersectionElemCount = intersection.elementCount();
                       if (conflictingRegister != -1)
                          {
                          intersection = _liveOnEntryConflicts[conflictingRegister];
                          if (secondConflictingRegister != -1)
                             intersection &= _liveOnEntryConflicts[secondConflictingRegister];
                          }

                       if (conflictingRegister == -1 ||
                           ((numBlocksAtMaxFrequency < numberOfConflicts) ||
                            ((numBlocksAtMaxFrequency == numberOfConflicts) && (intersectionElemCount < intersection.elementCount()))))
                          {
                          conflictingRegister = i;
                          secondConflictingRegister = j;
                          numberOfConflicts = numBlocksAtMaxFrequency;
                          }
                       }
                 }
              else
                 {
                 // first check for smaller maximum frequency, then try to get as many clear
                 //    structures as possible...finally, look at the number of conflicts
                 //
                 if (trace)
                    {
                    traceMsg(comp(), "Register : %d\n", i);
                    traceMsg(comp(), "max freq %d; smallest max freq %d\n", maxFrequency, smallestMaxFrequency);
                    traceMsg(comp(), "num clear structures %d; max clear structures %d\n", numClearStructures, maxClearStructures);
                    traceMsg(comp(), "num blocks at max freq %d;  min num conflicts %d\n", numBlocksAtMaxFrequency, numberOfConflicts);
                    }

                 if (conflictingRegister == -1 ||
                     maxFrequency < smallestMaxFrequency ||
                     ((maxFrequency == smallestMaxFrequency) &&
                      ((numClearStructures > maxClearStructures) ||
                       ((numClearStructures == maxClearStructures) &&
                        ((numBlocksAtMaxFrequency < numberOfConflicts) ||
                         ((numBlocksAtMaxFrequency == numberOfConflicts) && (_liveOnEntryConflicts[i].elementCount() < _liveOnEntryConflicts[conflictingRegister].elementCount())))))))
                    {
                    smallestMaxFrequency = maxFrequency;
                    maxClearStructures = numClearStructures;
                    numberOfConflicts = numBlocksAtMaxFrequency;
                    conflictingRegister = i;
                    }
                 }
              }

           if (conflictingRegister != -1)
              {
              //uint32_t originalWeight = rc->getWeight();

              //
              // Trim candidate's range by removing _liveOnEntryConflicts
              //
              intersection = _liveOnEntryConflicts[conflictingRegister];
              if (rc->rcNeeds2Regs(comp()))
                 intersection &= _liveOnEntryConflicts[secondConflictingRegister];

              if (trace)
                 {
                 dumpOptDetails(comp(), "\nDue to conflicts, removing the following blocks from live on entry ranges of candidate #%d conflicting register %d\n", rc->getSymbolReference()->getReferenceNumber(), conflictingRegister);
                 intersection.print(comp());
                 }

#ifdef TRIM_ASSIGNED_CANDIDATES
              TR_BitVector blocksToBeFreed(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc, growable);
              blocksToBeFreed  = _availableBlocks[conflictingRegister];
              blocksToBeFreed &= intersection;
              intersection -= blocksToBeFreed;

              if (!blocksToBeFreed.isEmpty())
                 {
                 if (trace)
                    {
                    traceMsg(comp(), "\nBlocks to be freed for candidate #%d: ", rc->getSymbolReference()->getReferenceNumber());
                    blocksToBeFreed.print(comp());
                    traceMsg(comp(),"\n");
                    }

                 bvi.setBitVector(blocksToBeFreed);
                 while (bvi.hasMoreElements())
                    {
                    int32_t blockNumber = bvi.getNextElement();
                    TR::Block *block = blocks[blockNumber];
                    TR_Structure *loop = block->getStructureOf()->getContainingLoop();
                    TR_RegisterCandidate *cand = block->getGlobalRegisters(comp())[conflictingRegister].getRegisterCandidateOnEntry();
                    if (cand)
                       {
                       cand->removeAssignedCandidateFromLoop(comp(), loop, conflictingRegister,
                             &_liveOnEntryUsage[conflictingRegister],
                             &_liveOnExitUsage[conflictingRegister],trace);
                       }
                    }
                 }
#endif

              rc->getBlocksLiveOnEntry() -= intersection;

              rc->recalculateWeight(blocks, blockStructureWeight, comp(),totalGPRCount,totalFPRCount, totalVRFCount, &referencedBlocks, _startOfExtendedBBForBB);
              //if (rc->getWeight() >= originalWeight)
                 {
#if 1
                 prioritizeCandidate(rc, next);
#else
                 next = rc;  // experimenting
#endif
                 rc->setReprioritized();
                 }
              if (trace)
                 {
                 TR_RegisterCandidate *rc2;
                 traceMsg(comp(),"Prioritized list of candidates\n");
                 for (rc2 = next; rc2; rc2 = rc2->getNext())
                    {
                    traceMsg(comp()," Candidate #%d (weight=%d)\n",rc2->getSymbolReference()->getReferenceNumber(),rc2->getWeight());
                    }
                 }
              }
           }

        continue;
        }

     if (needs2Regs)
        {
        if (!performTransformation(comp(), "%s assign auto #%d to low reg %d and high reg %d (%s)\n", OPT_DETAILS, rc->getSymbolReference()->getReferenceNumber(), registerNumber, highRegisterNumber, comp()->getDebug()? comp()->getDebug()->getGlobalRegisterName(highRegisterNumber):"?"))
           continue;
        }
     else
        {
        if (!performTransformation(comp(), "%s assign auto #%d %s to reg %d (%s)\n", OPT_DETAILS,
                                                                                  rc->getSymbolReference()->getReferenceNumber(),
                                                                                  rc->getSymbolReference()->getSymbol()->isMethodMetaData() ? rc->getSymbolReference()->getSymbol()->castToMethodMetaDataSymbol()->getName(): "",
                                                                                  registerNumber,
                                                                                  comp()->getDebug()? comp()->getDebug()->getGlobalRegisterName(registerNumber):"?"))
            continue;
        }

   TR_BitVector seenBlocks(numberOfBlocks, trMemory(), stackAlloc, growable);
   TR_BitVectorIterator exitBlocksIt(rc->getBlocksLiveOnExit());
   while (exitBlocksIt.hasMoreElements())
      {
      int32_t nextElement = exitBlocksIt.getNextElement();
      TR_ASSERT(blocks[nextElement]->getNumber() == nextElement,"blocks[x]->getNumber() is not x");
      int32_t blockNum = nextElement;
      if (!seenBlocks.get(blockNum))
         {
         seenBlocks.set(blockNum);
         if (isFloat)
            {
            ++totalFPRCount[blockNum];
            if (needs2Regs)
               ++totalFPRCount[blockNum];
            }
         else if (isVector)
            {
            ++totalVRFCount[blockNum];
            }
         else
            {
            ++totalGPRCount[blockNum];
            if (needs2Regs)
               ++totalGPRCount[blockNum];
            }
         }
      }

   seenBlocks.empty();
   TR_BitVectorIterator entryBlocksIt(rc->getBlocksLiveOnEntry());
   while (entryBlocksIt.hasMoreElements())
      {
      int32_t nextElement = entryBlocksIt.getNextElement();
      TR_ASSERT(blocks[nextElement]->getNumber() == nextElement, "blocks[x]->getNumber() is not x");
      TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");
      int32_t blockNum = _startOfExtendedBBForBB[nextElement]->getNumber();
      if (!seenBlocks.get(blockNum))
         {
         seenBlocks.set(blockNum);
         if (isFloat)
            {
            ++totalFPRCountOnEntry[blockNum];
            if (needs2Regs)
               ++totalFPRCountOnEntry[blockNum];
            }
         if (isVector)
            {
            ++totalVRFCountOnEntry[blockNum];
            }
         else
            {
            ++totalGPRCountOnEntry[blockNum];
            if (needs2Regs)
               ++totalGPRCountOnEntry[blockNum];
            }
         }
      }


     if (isFloat)
        globalFPAssignmentDone = true;

     _candidates.add(rc);
     TR_ASSERT(rc->getSymbolReference()->getSymbol()->isAutoOrParm()
           , "expecting auto or parm");
     (*_candidateForSymRefs)[GET_INDEX_FOR_CANDIDATE_FOR_SYMREF(rc->getSymbolReference())] = rc;

     if (needs2Regs)
        {
        rc->setLowGlobalRegisterNumber(registerNumber);
        rc->setHighGlobalRegisterNumber(highRegisterNumber);
        }
     else
        rc->setGlobalRegisterNumber(registerNumber);

     rc->setIs8BitGlobalGPR(cg->is8BitGlobalGPR(registerNumber));


     // Commented out because the low 8 bits of a long global reg are determined
     // only the low word reg and only whether the low word global reg has an 8 bit
     // counterpart is important
     //
     //rc->setIs8BitGlobalGPR(cg->is8BitGlobalGPR(highRegisterNumber));

     int32_t numRegs = 1;
     if (registerNumber > highestNumber)
        highestNumber = registerNumber;
     if (registerNumber < lowestNumber)
        lowestNumber = registerNumber;

     if (needs2Regs)
        {
          //printf("Assigned long in %s\n", comp()->getCurrentMethod(_compilation->getCurrentMethod()));
        if (highRegisterNumber > highestNumber)
           highestNumber = highRegisterNumber;
        if (highRegisterNumber < lowestNumber)
           lowestNumber = highRegisterNumber;
        numRegs = 2;
        }

     TR_BitVectorIterator bvi(rc->getBlocksLiveOnEntry());
     while (bvi.hasMoreElements())
        {
        int32_t nextElement = bvi.getNextElement();
        if (trace)
           traceMsg(comp(), "Register %d in block_%d is candidate %d on entry\n", registerNumber, nextElement, rc->getSymbolReference()->getReferenceNumber());

        blocks[nextElement]->getGlobalRegisters(comp())[registerNumber].setRegisterCandidateOnEntry(rc);
        if (needs2Regs)
           blocks[nextElement]->getGlobalRegisters(comp())[highRegisterNumber].setRegisterCandidateOnEntry(rc);
        }

     bvi.setBitVector(rc->getBlocksLiveOnExit());
     while (bvi.hasMoreElements())
        {
        int32_t blockNumber = bvi.getNextElement();
        TR_ASSERT(blocks[blockNumber]->getNumber() == blockNumber,"blocks[x]->getNumber() is not x");
        TR::Block * b = blocks[blockNumber];
        TR::Block * extBlock = _startOfExtendedBBForBB[blockNumber];
        TR_ASSERT(comp()->getOptimizer()->cachedExtendedBBInfoValid(), "Incorrect value in _startOfExtendedBBForBB");

        if (trace)
           traceMsg(comp(), "Register %d in block_%d is candidate %d on exit\n", registerNumber, blockNumber, rc->getSymbolReference()->getReferenceNumber());

        if (b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnExit() != rc)
           {
           if ((b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnExit() != NULL) &&
               (b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnExit() != b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnEntry()) &&
               (rc != b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnEntry()))
              {
              traceMsg(comp(), "Candidate %d instead of candidate %d is required in register %d on exit out of block_%d\n", b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnExit()->getSymbolReference()->getReferenceNumber(), rc->getSymbolReference()->getReferenceNumber(), registerNumber, blockNumber);
              TR_ASSERT(0, "Candidate is required on exit out of block\n");
              }
           else if ((rc != b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnEntry()) ||
                    (b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnExit() == NULL))
              {
                //traceMsg(comp(), "block_%d reg %d cand %d\n", b->getNumber(), registerNumber, rc->getSymbolReference()->getReferenceNumber());
              b->getGlobalRegisters(comp())[registerNumber].setRegisterCandidateOnExit(rc);
              //traceMsg(comp(), "block_%d reg %d cand %d\n", b->getNumber(), registerNumber, b->getGlobalRegisters(comp())[registerNumber].getRegisterCandidateOnExit()->getSymbolReference()->getReferenceNumber());
              }
           }

        if (needs2Regs)
           {
           if (b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnExit() != rc)
             {
             if ((b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnExit() != NULL) &&
                 (b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnExit() != b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnEntry()) &&
                 (rc != b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnEntry()))
                 {
                 traceMsg(comp(), "Candidate %d instead of candidate %d is required in register %d on exit out of block_%d\n", b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnExit()->getSymbolReference()->getReferenceNumber(), rc->getSymbolReference()->getReferenceNumber(), highRegisterNumber, blockNumber);
                 TR_ASSERT(0, "Candidate is required on exit out of block\n");
                 }
              else if ((rc != b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnEntry()) ||
                       (b->getGlobalRegisters(comp())[highRegisterNumber].getRegisterCandidateOnExit() == NULL))
                 b->getGlobalRegisters(comp())[highRegisterNumber].setRegisterCandidateOnExit(rc);
              }
           }

        if (isFloat)
           {
           if (++numberOfFPRsLiveOnExit[blockNumber] == maxFPRsLiveOnExit[blockNumber])
              {
              //static TR_BitVector *successorBits;
              if (!comp()->getOptimizer()->getSuccessorBitsGRA())
                 comp()->getOptimizer()->setSuccessorBitsGRA(new (trHeapMemory()) TR_BitVector(numberOfBlocks, trMemory(), heapAlloc, growable));
              else
                 comp()->getOptimizer()->getSuccessorBitsGRA()->empty();

              TR_ExtendedBlockSuccessorIterator ebsi(b, comp()->getFlowGraph());
              for (TR::Block * succBlock = ebsi.getFirst(); succBlock; succBlock = ebsi.getNext())
                 {
                 if (totalFPRCountOnEntry[succBlock->getNumber()] == maxFPRsLiveOnExit[blockNumber])
                    comp()->getOptimizer()->getSuccessorBitsGRA()->set(succBlock->getNumber());
                 }

              next = reprioritizeCandidates(next, blocks, blockStructureWeight, numberOfBlocks, b, comp(),
                                            true, false, &referencedBlocks, totalGPRCount, totalFPRCount, totalVRFCount,
                                            comp()->getOptimizer()->getSuccessorBitsGRA(), trace);
              }
           }
        if (isVector)
           {
           if (++numberOfVRFsLiveOnExit[blockNumber] == maxVRFsLiveOnExit[blockNumber])
              {
              if (!comp()->getOptimizer()->getSuccessorBitsGRA())
                 comp()->getOptimizer()->setSuccessorBitsGRA(new (trHeapMemory()) TR_BitVector(numberOfBlocks, trMemory(), heapAlloc, growable));
              else
                 comp()->getOptimizer()->getSuccessorBitsGRA()->empty();

              TR_ExtendedBlockSuccessorIterator ebsi(b, comp()->getFlowGraph());
              for (TR::Block * succBlock = ebsi.getFirst(); succBlock; succBlock = ebsi.getNext())
                 {
                 if (totalVRFCountOnEntry[succBlock->getNumber()] == maxVRFsLiveOnExit[blockNumber])
                    comp()->getOptimizer()->getSuccessorBitsGRA()->set(succBlock->getNumber());
                 }

              next = reprioritizeCandidates(next, blocks, blockStructureWeight, numberOfBlocks, b, comp(), false, false, &referencedBlocks, totalGPRCount, totalFPRCount, totalVRFCount,
                                            comp()->getOptimizer()->getSuccessorBitsGRA(), trace);
              }
           }
        else
           {
           numberOfGPRsLiveOnExit[blockNumber] = numberOfGPRsLiveOnExit[blockNumber] + numRegs;
           if (numberOfGPRsLiveOnExit[blockNumber] == maxGPRsLiveOnExit[blockNumber]) // all reg cands need to be reprioritized
              {
              //static TR_BitVector *successorBits;
              if (!comp()->getOptimizer()->getSuccessorBitsGRA())
                 comp()->getOptimizer()->setSuccessorBitsGRA(new (trHeapMemory()) TR_BitVector(numberOfBlocks, trMemory(), heapAlloc, growable));
              else
                 comp()->getOptimizer()->getSuccessorBitsGRA()->empty();

              TR_ExtendedBlockSuccessorIterator ebsi(b, comp()->getFlowGraph());
              for (TR::Block * succBlock = ebsi.getFirst(); succBlock; succBlock = ebsi.getNext())
                 {
                 if (totalGPRCountOnEntry[succBlock->getNumber()] == maxGPRsLiveOnExit[blockNumber])
                    comp()->getOptimizer()->getSuccessorBitsGRA()->set(succBlock->getNumber());
                 }

              next = reprioritizeCandidates(next, blocks, blockStructureWeight, numberOfBlocks, b, comp(),
                                            false, false, &referencedBlocks, totalGPRCount, totalFPRCount, totalVRFCount,
                                            comp()->getOptimizer()->getSuccessorBitsGRA(), trace);
              }
           else if (numberOfGPRsLiveOnExit[blockNumber] == (maxGPRsLiveOnExit[blockNumber] - 1)) // only long cands need to be reprioritized
              {
              //static TR_BitVector *successorBits;
              if (!comp()->getOptimizer()->getSuccessorBitsGRA())
                 comp()->getOptimizer()->setSuccessorBitsGRA(new (trHeapMemory()) TR_BitVector(numberOfBlocks, trMemory(), heapAlloc, growable));
              else
                 comp()->getOptimizer()->getSuccessorBitsGRA()->empty();

              TR_ExtendedBlockSuccessorIterator ebsi(b, comp()->getFlowGraph());
              for (TR::Block * succBlock = ebsi.getFirst(); succBlock; succBlock = ebsi.getNext())
                 {
                 if (totalGPRCountOnEntry[succBlock->getNumber()] == (maxGPRsLiveOnExit[blockNumber] - 1))
                    comp()->getOptimizer()->getSuccessorBitsGRA()->set(succBlock->getNumber());
                 }

              next = reprioritizeCandidates(next, blocks, blockStructureWeight, numberOfBlocks, b, comp(),
                                            false, true, &referencedBlocks, totalGPRCount, totalFPRCount, totalVRFCount,
                                            comp()->getOptimizer()->getSuccessorBitsGRA(), trace);
              }
           }
        }

     _liveOnEntryUsage[registerNumber] |= rc->getBlocksLiveOnEntry();

     if (trace)
        {
        traceMsg(comp(), "After assigning candidate %d real register %d, entry usage : \n", rc->getSymbolReference()->getReferenceNumber(), registerNumber);
        _liveOnEntryUsage[registerNumber].print(comp());
        traceMsg(comp(), "\n");
        }

     _liveOnExitUsage[registerNumber] |= rc->getBlocksLiveOnExit();

     if (trace)
        {
        traceMsg(comp(), "After assigning candidate %d real register %d, exit usage : \n", rc->getSymbolReference()->getReferenceNumber(), registerNumber);
        _liveOnExitUsage[registerNumber].print(comp());
        traceMsg(comp(), "\n");
        }

     if (needs2Regs)
        {
        _liveOnEntryUsage[highRegisterNumber] |= rc->getBlocksLiveOnEntry();

        if (trace)
           {
           traceMsg(comp(), "After assigning candidate %d real register %d, entry usage : \n", rc->getSymbolReference()->getReferenceNumber(), highRegisterNumber);
           _liveOnEntryUsage[highRegisterNumber].print(comp());
           traceMsg(comp(), "\n");
           }

        _liveOnExitUsage[highRegisterNumber] |= rc->getBlocksLiveOnExit();

        if (trace)
           {
           traceMsg(comp(), "After assigning candidate %d real register %d, exit usage : \n", rc->getSymbolReference()->getReferenceNumber(), highRegisterNumber);
           _liveOnEntryUsage[highRegisterNumber].print(comp());
           traceMsg(comp(), "\n");
           }
        }
      }

   return globalFPAssignmentDone;
   }


void  ComputeOverlaps(TR::Node *node,
                      TR::Compilation *comp,
                      TR_RegisterCandidates::Coordinates &overlaps,
                      uint32_t &seqno)
   {
   if (node->getVisitCount() == comp->getVisitCount())
      return;
   node->setVisitCount(comp->getVisitCount());

   int32_t i;
   for (i = 0; i < node->getNumChildren(); ++i)
      ComputeOverlaps(node->getChild(i), comp, overlaps, seqno);

   if (node->getOpCode().hasSymbolReference())
      {
      seqno+=1;
      uint32_t ref = node->getSymbolReference()->getReferenceNumber();

      TR_RegisterCandidates::Coordinates::iterator itr = overlaps.find(ref);
      if (itr != overlaps.end())
         itr->second.last = seqno;
      else
         {
         overlaps.insert(std::make_pair(ref, TR_RegisterCandidates::coordinates(seqno, seqno)));
         }
      }


   }

void  ComputeOverlaps(TR::Block *block,
                      TR::Compilation *comp,
                      TR_RegisterCandidates::Coordinates &overlaps) {
  uint32_t seqno = 0;
  comp->incVisitCount();
  TR::TreeTop *lt = block->getEntry()->getExtendedBlockExitTreeTop();

  int32_t t = 0;
  for (TR::TreeTop * tt = block->getFirstRealTreeTop(); tt; tt = tt->getNextTreeTop()){
     ComputeOverlaps(tt->getNode(), comp, overlaps, seqno);
  t += 1;
  if (tt == lt) break;
  }
}


void
TR_RegisterCandidates::computeAvailableRegisters(TR_RegisterCandidate *rc, int32_t firstRegister, int32_t lastRegister, TR::Block * * blocks, TR_BitVector *availableRegisters)
   {
   LexicalTimer t("compute available registers", comp()->phaseTimer());
   bool trace = comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);
   int8_t i;
   for (i = firstRegister; i <= lastRegister; ++i)
      {
      TR_BitVector &liveOnEntryConflicts = _liveOnEntryConflicts[i];
      liveOnEntryConflicts = _liveOnEntryUsage[i];
      liveOnEntryConflicts &= rc->getBlocksLiveOnEntry();

      TR_BitVector &liveOnExitConflicts = _liveOnExitConflicts[i];
      liveOnExitConflicts = _liveOnExitUsage[i];
      liveOnExitConflicts &= rc->getBlocksLiveOnExit();

      TR_BitVector &entryExitConflicts = _entryExitConflicts[i];
      entryExitConflicts = _liveOnEntryUsage[i];
      entryExitConflicts &= rc->getBlocksLiveOnExit();

      TR_BitVector &exitEntryConflicts = _exitEntryConflicts[i];
      exitEntryConflicts = _liveOnExitUsage[i];
      exitEntryConflicts &= rc->getBlocksLiveOnEntry();

      comp()->incOrResetVisitCount();

      TR_BitVectorIterator bvi(entryExitConflicts);
      while (bvi.hasMoreElements())
         {
         int32_t blockNumber = bvi.getNextElement();
         TR::Block * b = blocks[blockNumber];

         auto overlapLookup = overlapTable->find(blockNumber);
         Coordinates *overlaps = NULL;
         if (overlapLookup == overlapTable->end())
            {
            overlaps = new (comp()->trMemory()->currentStackRegion()) Coordinates((CoordinatesComparator()), (CoordinatesAllocator(comp()->trMemory()->currentStackRegion())));
            (*overlapTable)[blockNumber] = overlaps;
            ComputeOverlaps(b, comp(), *overlaps);
            }
         else
            {
            overlaps = overlapLookup->second;
            }
         CS2::HashIndex hi1, hi2;

         TR_RegisterCandidate *rc1 = b->getGlobalRegisters(comp())[i].getRegisterCandidateOnEntry();
         if (rc1)
            {
            Coordinates::iterator rc1Itr = overlaps->find(rc1->getSymbolReference()->getReferenceNumber());
            Coordinates::iterator rcItr = overlaps->find(rc->getSymbolReference()->getReferenceNumber());

            if (rc1Itr == overlaps->end() ||
                rcItr == overlaps->end() ||
                rc1Itr->second.last < rcItr->second.first)
               {
               entryExitConflicts.reset(blockNumber);
               }
            }
         }

      comp()->incVisitCount();
      bvi.setBitVector(exitEntryConflicts);
      while (bvi.hasMoreElements())
         {
         int32_t blockNumber = bvi.getNextElement();
         TR::Block * b = blocks[blockNumber];

         auto overlapLookup = overlapTable->find(blockNumber);
         Coordinates *overlaps = NULL;
         if (overlapLookup == overlapTable->end())
            {
            overlaps = new (comp()->trMemory()->currentStackRegion()) Coordinates((CoordinatesComparator()), (CoordinatesAllocator(comp()->trMemory()->currentStackRegion())));
            (*overlapTable)[blockNumber] = overlaps;
            ComputeOverlaps(b, comp(), *overlaps);
            }
         else
            {
            overlaps = overlapLookup->second;
            }

         TR_RegisterCandidate *rc2 = b->getGlobalRegisters(comp())[i].getRegisterCandidateOnExit();
         if (rc2)
            {
            Coordinates::iterator rcItr = overlaps->find(rc->getSymbolReference()->getReferenceNumber());
            Coordinates::iterator rc2Itr = overlaps->find(rc2->getSymbolReference()->getReferenceNumber());
            if (rcItr == overlaps->end() ||
                rc2Itr == overlaps->end()  ||
                rcItr->second.last < rc2Itr->second.first)
               {
               exitEntryConflicts.reset(blockNumber);
               }
            }
         }

      comp()->incVisitCount();

      // Set parameter conflicts in the Entry block: TODO multiple entries
      int32_t entryBlockNumber = comp()->getStartTree()->getNode()->getBlock()->getNumber();
      TR::Symbol *rcSymbol = rc->getSymbolReference()->getSymbol();
      if (rcSymbol->isParm() && rc->getBlocksLiveOnEntry().get(entryBlockNumber))
         {
         int8_t lri = rcSymbol->getParmSymbol()->getLinkageRegisterIndex();
         if (lri >= 0)
            {
            TR_GlobalRegisterNumber parmReg = comp()->cg()->getLinkageGlobalRegisterNumber(lri, rcSymbol->getDataType());
            TR_BitVector *linkageRegisters = comp()->cg()->getGlobalRegisters(TR_linkageSpill, TR_System);

            bvi.setBitVector(*linkageRegisters);
            while (bvi.hasMoreElements())
               {
               int32_t reg = bvi.getNextElement();
               if (reg != parmReg)
                  _liveOnEntryConflicts[reg].set(entryBlockNumber);
               }
            }
         }

      if (trace)
         {
         traceMsg(comp(), "For candidate %d real register %d : \n", rc->getSymbolReference()->getReferenceNumber(), i);
         traceMsg(comp(), "live on entry conflicts : ");
         liveOnEntryConflicts.print(comp());
         traceMsg(comp(), "\nlive on exit conflicts : ");
         liveOnExitConflicts.print(comp());
         traceMsg(comp(), "\nentry exit conflicts : ");
         entryExitConflicts.print(comp());
         traceMsg(comp(), "\nexit entry conflicts : ");
         exitEntryConflicts.print(comp());
         traceMsg(comp(), "\n");
         }

      if (liveOnEntryConflicts.isEmpty() && liveOnExitConflicts.isEmpty() &&
          exitEntryConflicts.isEmpty() &&
          entryExitConflicts.isEmpty() &&
          comp()->cg()->isGlobalRegisterAvailable(i, rc->getDataType()))
         {
         availableRegisters->set(i);
         }
      }
   }
