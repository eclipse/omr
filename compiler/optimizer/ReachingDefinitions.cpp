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

#include <stddef.h>
#include <stdint.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/UseDefInfo.hpp"

class TR_BlockStructure;
class TR_Structure;
namespace TR { class Optimizer; }

TR_DataFlowAnalysis::Kind TR_ReachingDefinitions::getKind()
   {
   return ReachingDefinitions;
   }

bool TR_ReachingDefinitions::supportsGenAndKillSets()
   {
   return true;
   }


TR_ReachingDefinitions::TR_ReachingDefinitions(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, TR_UseDefInfo *useDefInfo, TR_UseDefInfo::AuxiliaryData &aux, bool trace)
   : TR_UnionBitVectorAnalysis(comp, cfg, optimizer, trace),
     _useDefInfo(useDefInfo),
     _aux(aux)
   {
   _traceRD = comp->getOption(TR_TraceUseDefs);
   }

int32_t TR_ReachingDefinitions::perform()
   {
   LexicalTimer tlex("reachingDefs_perform", comp()->phaseTimer());
   if (traceRD())
      traceMsg(comp(), "Starting ReachingDefinitions\n");

   // Allocate the block info, allowing the bit vectors to be allocated on the fly
   //
   initializeBlockInfo(false);

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR_Structure *rootStructure = _cfg->getStructure();
   performAnalysis(rootStructure, false);

   if (traceRD())
      traceMsg(comp(), "\nEnding ReachingDefinitions\n");

   } // scope of the stack memory region

   return 10; // actual cost
   }


int32_t TR_ReachingDefinitions::getNumberOfBits()
   {
   return _useDefInfo->getNumExpandedDefNodes();
   }


void TR_ReachingDefinitions::analyzeBlockZeroStructure(TR_BlockStructure *blockStructure)
   {
   // Initialize the analysis info by making the initial parameter and field
   // definitions reach the method entry
   //
   if (_useDefInfo->getNumExpandedDefsOnEntry())
      _regularInfo->setAll(_useDefInfo->getNumExpandedDefsOnEntry());
   if (!_blockAnalysisInfo[0])
      allocateBlockInfoContainer(&_blockAnalysisInfo[0], _regularInfo);
   copyFromInto(_regularInfo, _blockAnalysisInfo[0]);
   }




void TR_ReachingDefinitions::initializeGenAndKillSetInfo()
   {
   // For each block in the CFG build the gen and kill set for this analysis.
   // Go in treetop order, which guarantees that we see the correct (i.e. first)
   // evaluation point for each node.
   //
   TR::Block *block;
   int32_t   blockNum = 0;
   bool      seenException = false;
   TR_BitVector defsKilled(getNumberOfBits(), trMemory()->currentStackRegion());

   comp()->incVisitCount();
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         blockNum = block->getNumber();
         seenException  = false;
         if (traceRD())
            traceMsg(comp(), "\nNow generating gen and kill information for block_%d\n", blockNum);
         continue;
         }

#if DEBUG
      if (node->getOpCodeValue() == TR::BBEnd && traceRD())
         {
         traceMsg(comp(), "  Block %d:\n", blockNum);
         traceMsg(comp(), "     Gen set ");
         if (_regularGenSetInfo[blockNum])
            _regularGenSetInfo[blockNum]->print(comp());
         else
            traceMsg(comp(), "{}");
         traceMsg(comp(), "\n     Kill set ");
         if (_regularKillSetInfo[blockNum])
            _regularKillSetInfo[blockNum]->print(comp());
         else
            traceMsg(comp(), "{}");
         traceMsg(comp(), "\n     Exception Gen set ");
         if (_exceptionGenSetInfo[blockNum])
            _exceptionGenSetInfo[blockNum]->print(comp());
         else
            traceMsg(comp(), "{}");
         traceMsg(comp(), "\n     Exception Kill set ");
         if (_exceptionKillSetInfo[blockNum])
            _exceptionKillSetInfo[blockNum]->print(comp());
         else
            traceMsg(comp(), "{}");
         continue;
         }
#endif

      initializeGenAndKillSetInfoForNode(node, defsKilled, seenException, blockNum, NULL);

      if (!seenException && treeHasChecks(treeTop))
         seenException = true;
      }
   }


void TR_ReachingDefinitions::initializeGenAndKillSetInfoForNode(TR::Node *node, TR_BitVector &defsKilled, bool seenException, int32_t blockNum, TR::Node *parent)
   {
   // Update gen and kill info for nodes in this subtree
   //
   int32_t i;

   if (node->getVisitCount() == comp()->getVisitCount())
      return;
   node->setVisitCount(comp()->getVisitCount());

   // Process the children first
   //
   for (i = node->getNumChildren()-1; i >= 0; --i)
      {
      initializeGenAndKillSetInfoForNode(node->getChild(i), defsKilled, seenException, blockNum, node);
      }

   bool irrelevantStore = false;
   scount_t nodeIndex = node->getLocalIndex();
   if (nodeIndex <= 0)
      {
      if (node->getOpCode().isStore() &&
          node->getSymbol()->isAutoOrParm() &&
          node->storedValueIsIrrelevant())
         {
         irrelevantStore = true;
         }
      else
         return;
      }

   bool foundDefsToKill = false;
   int32_t numDefNodes = 0;
   defsKilled.empty();

   TR::ILOpCode &opCode = node->getOpCode();
   TR::SymbolReference *symRef;
   TR::Symbol *sym;
   uint16_t symIndex;
   uint32_t num_aliases;

   if (_useDefInfo->_useDefForRegs &&
        (opCode.isLoadReg() ||
       opCode.isStoreReg()))
      {
      sym = NULL;
      symRef = NULL;
      symIndex = _useDefInfo->getNumSymbols() + node->getGlobalRegisterNumber();
      num_aliases = 1;
      }
   else
      {
      symRef = node->getSymbolReference();
      sym = symRef->getSymbol();
      symIndex = symRef->getSymbol()->getLocalIndex();
      num_aliases = _useDefInfo->getNumAliases(symRef, _aux);
      }


   if (symIndex == NULL_USEDEF_SYMBOL_INDEX || node->getOpCode().isCall() || node->getOpCode().isFence() ||
       (parent && parent->getOpCode().isResolveCheck() && num_aliases > 1))
      {
      // A call or unresolved reference is a definition of all
      // symbols it is aliased with
      //
      numDefNodes = num_aliases;

      //for all symbols that are a mustdef of a call, kill defs of those symbols
      if (node->getOpCode().isCall())
         foundDefsToKill = false;
      }
   else if (irrelevantStore || _useDefInfo->isExpandedDefIndex(nodeIndex))
      {
      // DefOnly node defines all symbols it is aliased with
      // UseDef node(load) defines only the symbol itself
      //

      if (!irrelevantStore)
         {
         numDefNodes = num_aliases;
         numDefNodes = _useDefInfo->isExpandedUseDefIndex(nodeIndex) ? 1 : numDefNodes;

         if (!_useDefInfo->getDefsForSymbolIsZero(symIndex, _aux) &&
             (!sym ||
             (!sym->isShadow() &&
             !sym->isMethod())))
            {
            foundDefsToKill = true;
               // defsKilled ORed with defsForSymbol(symIndex);
           _useDefInfo->getDefsForSymbol(defsKilled, symIndex, _aux);
            }
         if (node->getOpCode().isStoreIndirect())
            {
            int32_t memSymIndex = _useDefInfo->getMemorySymbolIndex(node);
            if (memSymIndex != -1 &&
                !_useDefInfo->getDefsForSymbolIsZero(memSymIndex, _aux))
               {
               foundDefsToKill = true;
               // defsKilled ORed with defsForSymbol(symIndex);
               _useDefInfo->getDefsForSymbol(defsKilled, memSymIndex, _aux);
               }
            }
         }
      else if (!_useDefInfo->getDefsForSymbolIsZero(symIndex, _aux))
         {
         numDefNodes = 1;
         foundDefsToKill = true;
         // defsKilled ORed with defsForSymbol(symIndex);
         _useDefInfo->getDefsForSymbol(defsKilled, symIndex, _aux);
         }
      }
   else
      {
      numDefNodes = 0;
      }

   if (foundDefsToKill)
      {
      if (_regularKillSetInfo[blockNum] == NULL)
         allocateContainer(&_regularKillSetInfo[blockNum]);
      *_regularKillSetInfo[blockNum] |= defsKilled;
      if (!seenException)
         {
         if (_exceptionKillSetInfo[blockNum] == NULL)
            allocateContainer(&_exceptionKillSetInfo[blockNum]);
         *_exceptionKillSetInfo[blockNum] |= defsKilled;
         }
      }
   if (_regularGenSetInfo[blockNum] == NULL)
     allocateContainer(&_regularGenSetInfo[blockNum]);
   else if (foundDefsToKill)
      *_regularGenSetInfo[blockNum] -= defsKilled;

   if (_exceptionGenSetInfo[blockNum] == NULL)
      allocateContainer(&_exceptionGenSetInfo[blockNum]);
   else if (foundDefsToKill && !seenException)
      *_exceptionGenSetInfo[blockNum] -= defsKilled;

   if (!irrelevantStore)
      {
      for (i = 0; i < numDefNodes; ++i)
         {
         _regularGenSetInfo[blockNum]->set(nodeIndex+i);
         _exceptionGenSetInfo[blockNum]->set(nodeIndex+i);
         }
      }
   else // fake up the method entry def as the def index to "gen" to avoid a use without a def completely
      {
      _regularGenSetInfo[blockNum]->set(sym->getLocalIndex());
      _exceptionGenSetInfo[blockNum]->set(sym->getLocalIndex());
      }
   }
