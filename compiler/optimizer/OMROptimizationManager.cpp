/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "optimizer/OptimizationManager.hpp"
#include "optimizer/OptimizationManager_inlines.hpp"

#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "ras/ILValidator.hpp"
#include "ras/ILValidationStrategies.hpp"
#include "env/CompilerEnv.hpp"

struct OptimizationStrategy;

OMR::OptimizationManager::OptimizationManager(TR::Optimizer *o, OptimizationFactory factory, OMR::Optimizations optNum, const OptimizationStrategy *groupOfOpts)
      : _optimizer(o),
        _factory(factory),
        _id(optNum),
        _groupOfOpts(groupOfOpts),
        _numPassesCompleted(0),
        _optData(NULL),
        _optPolicy(NULL),
        _enabled(!o->comp()->isDisabled(optNum)),
        _requested(false),
        _requestedBlocks(o->trMemory()),
        _trace(o->comp()->getOptions()->trace(optNum))
   {
   if (_id < OMR::Optimizations::numGroups)
      TR_ASSERT_SAFE_FATAL(_id < OMR::Optimizations::numGroups, "The optimization id requested (%d) is too high", _id);

   // set flags if necessary
   switch (self()->id())
      {
      case OMR::inlining:
         _flags.set(doesNotRequireAliasSets | canAddSymbolReference | verifyTrees | verifyBlocks | checkTheCFG | requiresAccurateNodeCount);
         break;
      case OMR::trivialInlining:
         _flags.set(doesNotRequireAliasSets | canAddSymbolReference | verifyTrees | verifyBlocks | checkTheCFG | requiresAccurateNodeCount);
         break;
      case OMR::CFGSimplification:
         _flags.set(verifyTrees | verifyBlocks | checkTheCFG | supportsIlGenOptLevel);
         break;
      case OMR::basicBlockExtension:
         _flags.set(requiresStructure);
         break;
      case OMR::treeSimplification:
         _flags.set(verifyTrees | verifyBlocks | checkTheCFG);
         if (self()->comp()->getOption(TR_EnableReassociation))
            self()->setRequiresStructure(true);
         break;
      case OMR::loopCanonicalization:
         _flags.set(requiresStructure | checkStructure | dumpStructure | requiresAccurateNodeCount);
         break;
      case OMR::loopVersioner:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         if (self()->comp()->getMethodHotness() >= hot)
            _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs | requiresLocalsValueNumbering);
         break;
      case OMR::loopReduction:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         break;
      case OMR::loopReplicator:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         break;
      case OMR::globalValuePropagation:
         _flags.set(requiresStructure | checkStructure | dumpStructure |
                    requiresLocalsUseDefInfo | requiresLocalsValueNumbering);
         break;
      case OMR::partialRedundancyElimination:
         _flags.set(requiresStructure | canAddSymbolReference);
         break;
      case OMR::globalCopyPropagation:
         _flags.set(requiresStructure | requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs);
         break;
      case OMR::globalDeadStoreElimination:
         _flags.set(requiresStructure);
         _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs);
         break;
      case OMR::deadTreesElimination:
         break;
      case OMR::tacticalGlobalRegisterAllocator:
         _flags.set(requiresStructure);
         if (self()->comp()->getMethodHotness() >= hot && o->comp()->target().is64Bit())
            _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs);
         break;
      case OMR::loopInversion:
         _flags.set(requiresStructure);
         break;
      case OMR::fieldPrivatization:
         _flags.set(requiresStructure);
         break;
      case OMR::catchBlockRemoval:
         _flags.set(verifyTrees | verifyBlocks | checkTheCFG);
         break;
      case OMR::generalLoopUnroller:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         break;
      case OMR::redundantAsyncCheckRemoval:
         _flags.set(requiresStructure);
         break;
      case OMR::virtualGuardTailSplitter:
         _flags.set(checkTheCFG);
         break;
      case OMR::expressionsSimplification:
         _flags.set(requiresStructure);
         break;
      case OMR::blockSplitter:
         _flags.set(requiresStructure);
         break;
      case OMR::invariantArgumentPreexistence:
         _flags.set(doesNotRequireAliasSets | verifyTrees | supportsIlGenOptLevel);
         break;
      case OMR::compactLocals:
         _flags.set(requiresStructure);
         break;
      case OMR::coldBlockMarker:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::coldBlockOutlining:
         _flags.set(doesNotRequireAliasSets);
         break;
      case OMR::basicBlockPeepHole:
         _flags.set(requiresStructure);
         break;
      case OMR::innerPreexistence:
         _flags.set(requiresLocalsUseDefInfo | requiresLocalsValueNumbering);
         break;
      case OMR::basicBlockOrdering:
         _flags.set(requiresStructure);
         break;
      case OMR::loopSpecializer:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         break;
      case OMR::generalStoreSinking:
         _flags.set(requiresStructure);
         break;
      case OMR::inductionVariableAnalysis:
         _flags.set(requiresStructure | checkStructure);
         break;
      case OMR::reorderArrayIndexExpr:
         _flags.set(requiresStructure | checkStructure);
         break;
      case OMR::liveRangeSplitter:
         _flags.set(requiresStructure);
         break;
      case OMR::loopStrider:
         _flags.set(requiresStructure);
         // get UseDefInfo for sign-extension elimination on 64-bit
         if (o->comp()->target().is64Bit())
            _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs);
         break;
      case OMR::profiledNodeVersioning:
         _flags.set(doesNotRequireAliasSets);
         break;
      case OMR::stripMining:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         break;
      case OMR::osrDefAnalysis:
         if (self()->comp()->getOption(TR_DisableOSRSharedSlots))
            _flags.set(doesNotRequireAliasSets | doesNotRequireTreeDumps | supportsIlGenOptLevel);
         else
            _flags.set(doesNotRequireAliasSets | /* requiresStructure | */ supportsIlGenOptLevel);
         break;
      case OMR::blockShuffling:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::osrLiveRangeAnalysis:
         _flags.set(doesNotRequireAliasSets | supportsIlGenOptLevel);
         break;
      case OMR::redundantInductionVarElimination:
         _flags.set(requiresStructure | checkStructure | dumpStructure);
         break;
      case OMR::trivialBlockExtension:
         self()->setSupportsIlGenOptLevel(true);
         _flags.set(doesNotRequireAliasSets);
         break;
      case OMR::virtualGuardHeadMerger:
         _flags.set(doesNotRequireAliasSets);
         break;
      default:
         // do nothing
         break;
      }
   }

bool OMR::OptimizationManager::requested(TR::Block *block)
   {
   if (block == NULL) return _requested;
   return _requestedBlocks.find(block);
   }

void OMR::OptimizationManager::setRequested(bool requested, TR::Block *block)
   {
   _requested = requested;

   if (requested && self()->optimizer()->canRunBlockByBlockOptimizations())
      {
      if (block)
         {
         block = block->startOfExtendedBlock();
         if (!_requestedBlocks.find(block))
            {
            _requestedBlocks.add(block);
            if (self()->id() == loopVersionerGroup)
               self()->optimizer()->setRequestOptimization(lastLoopVersionerGroup, requested, block);
            }
         }
      else
         {
         // If the dummy start block is in the list, then that is deemed
         // to be an indication that adding all blocks into this list
         // was done before
         if (!_requestedBlocks.find(toBlock(self()->comp()->getFlowGraph()->getStart())))
            // Add the start node at the head of the list so that the above
            // test is fast; this will enable repeated calls to add all blocks
            // to be handled efficiently as we won't iterate through blocks each time
            _requestedBlocks.add(toBlock(self()->comp()->getFlowGraph()->getStart()));
         if (self()->id() == loopVersionerGroup)
            self()->optimizer()->setRequestOptimization(lastLoopVersionerGroup, requested, toBlock(self()->comp()->getFlowGraph()->getStart()));
         }
      }
   else if (requested == false)
      {
      _requestedBlocks.deleteAll();
      }
   }


void OMR::OptimizationManager::performChecks()
   {
#if !defined(DISABLE_CFG_CHECK)
   LexicalTimer t("CFG_CHECK", self()->comp()->phaseTimer());
   TR::Compilation::CompilationPhaseScope performChecks(self()->comp());
   self()->comp()->reportAnalysisPhase(TR::Optimizer::PERFORMING_CHECKS);
   // From here, down, stack memory allocations will die when the function returns.
   TR::StackMemoryRegion stackMemoryRegion(*(self()->trMemory()));
   if (self()->getVerifyTrees() || self()->comp()->getOption(TR_EnableParanoidOptCheck) || debug("paranoidOptCheck"))
      {
      if (self()->comp()->getOption(TR_UseILValidator))
         {
         self()->comp()->validateIL(TR::postILgenValidation);
         }
      else
         {
         self()->comp()->verifyTrees(self()->comp()->getMethodSymbol());
         }
      }

   if (self()->getVerifyBlocks() || self()->comp()->getOption(TR_EnableParanoidOptCheck) || debug("paranoidOptCheck"))
      self()->comp()->verifyBlocks(self()->comp()->getMethodSymbol());

   if (self()->getCheckTheCFG() || self()->comp()->getOption(TR_EnableParanoidOptCheck) || debug("paranoidOptCheck"))
      self()->comp()->verifyCFG(self()->comp()->getMethodSymbol());

   if (self()->getCheckStructure() || self()->comp()->getOption(TR_EnableParanoidOptCheck) || debug("paranoidOptCheck"))
      self()->optimizer()->doStructureChecks();
#endif
   }
