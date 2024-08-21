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

#if defined(J9ZOS390)
#pragma csect(CODE,"TRCGBase#C")
#pragma csect(STATIC,"TRCGBase#S")
#pragma csect(TEST,"TRCGBase#T")
#endif

#include "codegen/CodeGenerator.hpp"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/RegisterUsage.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/StorageInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "compile/Compilation.hpp"
#include "compile/OSRData.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitmanip.h"
#include "cs2/hashtab.h"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
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
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/HashTab.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "infra/Checklist.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "ras/Delimiter.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "stdarg.h"
#include "OMR/Bytes.hpp"

namespace TR { class Optimizer; }
namespace TR { class RegisterDependencyConditions; }

OMR::TreeEvaluatorFunctionPointerTable OMR::CodeGenerator::_nodeToInstrEvaluators;

OMR::TreeEvaluatorFunctionPointer
OMR::TreeEvaluatorFunctionPointerTable::table[] =
   {
#define OPCODE_MACRO(\
   opcode, \
   name, \
   prop1, \
   prop2, \
   prop3, \
   prop4, \
   dataType, \
   typeProps, \
   childProps, \
   swapChildrenOpcode, \
   reverseBranchOpcode, \
   boolCompareOpcode, \
   ifCompareOpcode, \
   ...) TR::TreeEvaluator::opcode ## Evaluator,

   TR::TreeEvaluator::BadILOpEvaluator,

#include "il/Opcodes.enum"
#undef OPCODE_MACRO

#define VECTOR_OPERATION_MACRO(\
   operation, \
   name, \
   prop1, \
   prop2, \
   prop3, \
   prop4, \
   dataType, \
   typeProps, \
   childProps, \
   swapChildrenOpcode, \
   reverseBranchOpcode, \
   boolCompareOpcode, \
   ifCompareOpcode, \
   ...) TR::TreeEvaluator::operation ## Evaluator,

   TR::TreeEvaluator::BadILOpEvaluator,

#include "il/VectorOperations.enum"
#undef VECTOR_OPERATION_MACRO
   };


void OMR::TreeEvaluatorFunctionPointerTable::checkTableSize()
   {
   static_assert((TR::NumScalarIlOps + TR::NumVectorOperations) ==
              (sizeof(table) / sizeof(table[0])),
              "OMR::TreeEvaluatorFunctionPointerTable::table is not the correct size");
   }


#define OPT_DETAILS "O^O CODE GENERATION: "


TR::Instruction *
OMR::CodeGenerator::generateNop(TR::Node * node, TR::Instruction *instruction, TR_NOPKind nopKind)
    { TR_ASSERT(0, "shouldn't get here"); return NULL;}


TR::CodeGenerator *
OMR::CodeGenerator::create(TR::Compilation *comp)
   {
   TR::CodeGenerator *cg = new (comp->trHeapMemory()) TR::CodeGenerator(comp);
   cg->initialize();
   return cg;
   }


OMR::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
      _compilation(comp),
      _trMemory(comp->trMemory()),
      _liveLocals(0),
      _currentEvaluationTreeTop(0),
      _currentEvaluationBlock(0),
      _prePrologueSize(0),
      _implicitExceptionPoint(0),
      _localsThatAreStored(NULL),
      _numLocalsWhenStoreAnalysisWasDone(-1),
      _symRefTab(comp->getSymRefTab()),
      _vmThreadRegister(NULL),
      _stackAtlas(NULL),
      _methodStackMap(NULL),
      _binaryBufferStart(NULL),
      _binaryBufferCursor(NULL),
      _largestOutgoingArgSize(0),
      _warmCodeEnd(NULL),
      _coldCodeStart(NULL),
      _estimatedWarmCodeLength(0),
      _estimatedColdCodeLength(0),
      _estimatedCodeLength(0),
      _estimatedSnippetStart(0),
      _accumulatedInstructionLengthError(0),
      _registerSaveDescription(0),
      _extendedToInt64GlobalRegisters(comp->allocator()),
      _liveButMaybeUnreferencedLocals(NULL),
      _assignedGlobalRegisters(NULL),
      _aheadOfTimeCompile(NULL),
      _globalRegisterTable(NULL),
      _currentGRABlockLiveOutSet(NULL),
      _localsIG(NULL),
      _lastGlobalGPR(0),
      _firstGlobalFPR(0),
      _lastGlobalFPR(0),
      _firstOverlappedGlobalFPR(0),
      _lastOverlappedGlobalFPR(0),
      _last8BitGlobalGPR(0),
      _globalGPRPartitionLimit(0),
      _globalFPRPartitionLimit(0),
      _firstInstruction(NULL),
      _appendInstruction(NULL),
      _lastWarmInstruction(NULL),
      _firstGlobalVRF(-1),
      _lastGlobalVRF(-1),
      _firstOverlappedGlobalVRF(-1),
      _lastOverlappedGlobalVRF(-1),
      _overlapOffsetBetweenFPRandVRFgrns(0),
      _supportedLiveRegisterKinds(0),
      _blocksWithCalls(NULL),
      _codeCache(0),
      _committedToCodeCache(false),
      _codeCacheSwitched(false),
      _blockRegisterPressureCache(NULL),
      _simulatedNodeStates(NULL),
      _availableSpillTemps(getTypedAllocator<TR::SymbolReference*>(comp->allocator())),
      _counterBlocks(getTypedAllocator<TR::Block*>(comp->allocator())),
      _liveReferenceList(getTypedAllocator<TR_LiveReference*>(comp->allocator())),
      _snippetList(getTypedAllocator<TR::Snippet*>(comp->allocator())),
      _registerArray(comp->trMemory()),
      _spill4FreeList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _spill8FreeList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _spill16FreeList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _spill32FreeList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _spill64FreeList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _internalPointerSpillFreeList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _firstTimeLiveOOLRegisterList(NULL),
      _spilledRegisterList(NULL),
      _afterRA(false),
      _referencedRegistersList(NULL),
      _variableSizeSymRefPendingFreeList(getTypedAllocator<TR::SymbolReference*>(comp->allocator())),
      _variableSizeSymRefFreeList(getTypedAllocator<TR::SymbolReference*>(comp->allocator())),
      _variableSizeSymRefAllocList(getTypedAllocator<TR::SymbolReference*>(comp->allocator())),
      _accumulatorNodeUsage(0),
      _collectedSpillList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _allSpillList(getTypedAllocator<TR_BackingStore*>(comp->allocator())),
      _relocationList(getTypedAllocator<TR::Relocation*>(comp->allocator())),
      _externalRelocationList(getTypedAllocator<TR::Relocation*>(comp->allocator())),
      _staticRelocationList(comp->allocator()),
      _preJitMethodEntrySize(0),
      _jitMethodEntryPaddingSize(0),
      _lastInstructionBeforeCurrentEvaluationTreeTop(NULL),
      _unlatchedRegisterList(NULL),
      _indentation(2),
      _currentBlock(NULL),
      _realVMThreadRegister(NULL),
      _internalControlFlowNestingDepth(0),
      _internalControlFlowSafeNestingDepth(0),
      _stackOfArtificiallyInflatedNodes(comp->trMemory(), 16),
      _stackOfMemoryReferencesCreatedDuringEvaluation(comp->trMemory(), 16),
      randomizer(comp),
      _outOfLineColdPathNestedDepth(0),
      _codeGenPhase(self()),
      _symbolDataTypeMap(comp->allocator()),
      _lmmdFailed(false),
      _objectFormat(NULL),
      _snippetsToBePatchedOnClassUnload(getTypedAllocator<TR::Snippet*>(comp->allocator())),
      _methodSnippetsToBePatchedOnClassUnload(getTypedAllocator<TR::Snippet*>(comp->allocator())),
      _snippetsToBePatchedOnClassRedefinition(getTypedAllocator<TR::Snippet*>(comp->allocator()))
   {
   }


void
OMR::CodeGenerator::initialize()
   {
   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = self()->comp();

   _machine = new (cg->trHeapMemory()) TR::Machine(cg);

   _disableInternalPointers = comp->getOption(TR_MimicInterpreterFrameShape) ||
                              comp->getOptions()->realTimeGC() ||
                              comp->getOption(TR_DisableInternalPointers);

   uintptr_t maxSize = TR::Compiler->vm.getOverflowSafeAllocSize(comp);
   int32_t i;

   for (i = 0; i < NumRegisterKinds; ++i)
      {
      _liveRegisters[i] = NULL;
      _liveRealRegisters[i] = 0;
      }

   for (i = 0 ; i < TR_NumLinkages; ++i)
      _linkages[i] = NULL;

   _maxObjectSizeGuaranteedNotToOverflow = static_cast<uint32_t>((maxSize > UINT_MAX) ? UINT_MAX : maxSize);

   if (comp->getDebug())
      {
      comp->getDebug()->resetDebugData();
      }

   cg->setIsLeafMethod();

   cg->createObjectFormat();
   }

TR_StackMemory
OMR::CodeGenerator::trStackMemory()
   {
   return self()->trMemory();
   }

TR_FrontEnd *
OMR::CodeGenerator::fe()
   {
   return self()->comp()->fe();
   }

TR_Debug *
OMR::CodeGenerator::getDebug()
   {
   return self()->comp()->getDebug();
   }


/*
 * generateCodeFromIL
 *
 * Translate IL trees into executable machine instructions for the target
 * architecture.  This method provides te default functionality for generating
 * code.  It should be overridden to provide customized functionality.
 *
 * Returns: true if successful, false otherwise
 */

bool
OMR::CodeGenerator::generateCodeFromIL()
   {
   return false;
   }

void OMR::CodeGenerator::findLastWarmBlock()
   {
   TR::Compilation *comp = self()->comp();
   TR::TreeTop * tt;
   TR::Node * node;

   TR::Block * block = NULL;
   TR::Block * firstColdBlock = NULL, * firstColdExtendedBlock = NULL;
   int32_t numColdBlocks = 0, numNonOutlinedColdBlocks = 0;

   for (tt = comp->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      node = tt->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();

         // If this is the first cold block, remember where the warm blocks ended.
         // If it is a warm block and cold blocks have already been found, they
         // are treated as warm.
         //
         if (block->isCold())
            {
            if (!firstColdBlock)
               firstColdBlock = block;

            numColdBlocks++;
            }
         else if (firstColdBlock)
            {
            firstColdBlock = NULL;
            firstColdExtendedBlock = NULL;
            numNonOutlinedColdBlocks = numColdBlocks;
            }

         if (!block->isExtensionOfPreviousBlock())
            {
            if (firstColdBlock &&
                !firstColdExtendedBlock)
               {
               if (!block->getPrevBlock() ||
                   !block->getPrevBlock()->canFallThroughToNextBlock())
                  {
                  firstColdExtendedBlock = block;
                  }
               else
                  {
                  firstColdBlock = NULL;
                  numNonOutlinedColdBlocks = numColdBlocks;
                  }
               }
            }
         }
      }

   // Mark the split point between warm and cold blocks, so they can be
   // allocated in different code sections.
   //
   TR::Block *lastWarmBlock;

   if (firstColdExtendedBlock)
      {
      lastWarmBlock = firstColdExtendedBlock->getPrevBlock();

      if (!lastWarmBlock)
         {
         // All the blocks are cold: insert a new goto block ahead of real blocks
         //
         lastWarmBlock = comp->insertNewFirstBlock();
         }
      }
   else
      {
      // All the blocks are warm - the split point is between the last
      // instruction and the snippets.
      //
      lastWarmBlock = block;
      }

   lastWarmBlock->setIsLastWarmBlock();

   if (comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "%s Last warm block is block_%d\n", SPLIT_WARM_COLD_STRING, lastWarmBlock->getNumber());

      if (numColdBlocks > 0)
         traceMsg(comp, "%s Moved to cold code cache %d out of %d cold blocks (%d%%)\n",
                           SPLIT_WARM_COLD_STRING,
                           numColdBlocks - numNonOutlinedColdBlocks,
                           numColdBlocks,
                           (numColdBlocks - numNonOutlinedColdBlocks)*100/numColdBlocks);
      }

   // If the last tree in the last warm block is not a TR_goto, insert a goto tree
   // at the end of the block.
   // If there is a following block the goto will branch to it so that when the
   // code is split any fall-through will go to the right place.
   // If there is no following block the goto will branch to the first block; in
   // this case the goto should never be reached, it is there only to
   // make sure that the instruction following the last real treetop will be in
   // warm code, so if it is a helper call (e.g. for a throw) the return address
   // is in this method's code.
   //
   if (lastWarmBlock->getNumberOfRealTreeTops() == 0)
      tt = lastWarmBlock->getEntry();
   else
      tt = lastWarmBlock->getLastRealTreeTop();

   node = tt->getNode();

   if (!(node->getOpCode().isGoto() ||
         node->getOpCode().isJumpWithMultipleTargets() ||
         node->getOpCode().isReturn()))
      {
      // Find the block to be branched to
      //
      TR::TreeTop * targetTreeTop = lastWarmBlock->getExit()->getNextTreeTop();

      if (targetTreeTop)
         // Branch to following block. Make sure it is not marked as an
         // extension block so that it will get a label generated.
         //
         targetTreeTop->getNode()->getBlock()->setIsExtensionOfPreviousBlock(false);
      else
         // Branch to the first block. This will not be marked as an extension
         // block.
         //
         targetTreeTop = comp->getStartBlock()->getEntry();

      // Generate the goto and insert it into the end of the last warm block.
      //
      TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp, TR::Node::create(node, TR::Goto, 0, targetTreeTop));

      // Move reg deps from BBEnd to goto
      //
      TR::Node *bbEnd = lastWarmBlock->getExit()->getNode();

      if (bbEnd->getNumChildren() > 0)
         {
         TR::Node *glRegDeps = bbEnd->getChild(0);

         gotoTreeTop->getNode()->setNumChildren(1);
         gotoTreeTop->getNode()->setChild(0, glRegDeps);

         bbEnd->setChild(0,NULL);
         bbEnd->setNumChildren(0);
         }

      tt->insertAfter(gotoTreeTop);
      }
   }

void OMR::CodeGenerator::lowerTrees()
   {

   TR::Delimiter d(self()->comp(), self()->comp()->getOption(TR_TraceCG), "LowerTrees");

   // Go through the trees and lower any nodes that need to be lowered. This
   // involves a call to the VM to replace the trees with other trees.
   //

   // During this loop, more trees may be added after the current tree
   //
   // NOTE: when changing, please be aware that during the following tree walk some trees
   // might be removed (look at lowerTreeIfNeeded)
   //

   // traverse to lower the trees
   // visitCount should not be incremented until it finishes
   //

   self()->preLowerTrees();

   TR::TreeTop * tt;
   TR::Node * node;

   vcount_t visitCount = self()->comp()->incVisitCount();

   for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      node = tt->getNode();

      TR_ASSERT(node->getVisitCount() != visitCount, "Code Gen: error in lowering trees");
      TR_ASSERT(node->getReferenceCount() == 0, "Code Gen: error in lowering trees");

      self()->lowerTreesPreTreeTopVisit(tt, visitCount);

      // First lower the children
      //
      self()->lowerTreesWalk(node, tt, visitCount);

      self()->lowerTreeIfNeeded(node, 0, 0, tt);


      self()->lowerTreesPostTreeTopVisit(tt, visitCount);

      }

   self()->postLowerTrees();
   }

void OMR::CodeGenerator::postLowerTrees()
   {
   if (comp()->getOption(TR_SplitWarmAndColdBlocks) &&
       !comp()->compileRelocatableCode())
      {
      self()->findLastWarmBlock();
      }
   }

void
OMR::CodeGenerator::lowerTreesWalk(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {

   parent->setVisitCount(visitCount);

   self()->lowerTreesPreChildrenVisit(parent, treeTop, visitCount);

   // Go through the subtrees and lower any nodes that need to be lowered. This
   // involves a call to the VM to replace the trees with other trees.
   //
   for (int32_t childCount = 0; childCount < parent->getNumChildren(); childCount++)
      {
      TR::Node *child = parent->getChild(childCount);

      // If the subtree needs to be lowered, call the VM to lower it
      //
      if (child->getVisitCount() != visitCount)
         {
         self()->lowerTreesWalk(child, treeTop, visitCount);
         self()->lowerTreeIfNeeded(child, childCount, parent, treeTop);
         }
      }

   self()->lowerTreesPostChildrenVisit(parent, treeTop, visitCount);

   }


void
OMR::CodeGenerator::lowerTreesPropagateBlockToNode(TR::Node *node)
   {

   if (self()->comp()->getFlowGraph()->getNextNodeNumber() < SHRT_MAX)
      {
      node->setLocalIndex(self()->getCurrentBlock()->getNumber());
      }
   else
      {
      node->setLocalIndex(-1);
      }

   }


void
OMR::CodeGenerator::preLowerTrees()
   {
   int32_t symRefCount = self()->comp()->getSymRefCount();
   _localsThatAreStored = new (self()->comp()->trHeapMemory()) TR_BitVector(symRefCount, self()->comp()->trMemory(), heapAlloc);
   _numLocalsWhenStoreAnalysisWasDone = symRefCount;
   }


void
OMR::CodeGenerator::lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount)
   {
   TR::Node *node = tt->getNode();

   if (node->getOpCodeValue() == TR::BBStart)
      {
      TR::Block *block = node->getBlock();
      self()->setCurrentBlock(block);
      }

   }


void
OMR::CodeGenerator::lowerTreesPostTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount)
   {
   }


void
OMR::CodeGenerator::lowerTreesPostChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {
   }


void
OMR::CodeGenerator::setUpForInstructionSelection()
  {
   self()->comp()->incVisitCount();

   // prepareNodeForInstructionSelection is called during a separate walk of the treetops because
   // the _register and _label fields are unioned members of a node.  prepareNodeForInstructionSelection
   // zeros the _register field while the second for loop sets label fields on destination nodes.
   //
   TR::TreeTop * tt=NULL, *prev = NULL;


   for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      self()->prepareNodeForInstructionSelection(tt->getNode());
      }


   for (tt = self()->comp()->getStartTree(); tt; prev=tt, tt = tt->getNextTreeTop())
      {
      TR::Node * node = tt->getNode();

      if ((node->getOpCodeValue() == TR::treetop) ||
          node->getOpCode().isAnchor() ||
          node->getOpCode().isCheck())
         {
         node = node->getFirstChild();
         }

      TR::ILOpCode & opcode = node->getOpCode();

      if (opcode.getOpCodeValue() == TR::BBStart)
         {
         self()->setCurrentBlock(node->getBlock());
         }
      else if (opcode.isLoadVarOrStore())
         {
         TR::Symbol * sym = node->getSymbol();
         TR::AutomaticSymbol *local = sym->getAutoSymbol();
         if (local)
            {
            local->incReferenceCount();
            }
         }
      else if (opcode.isBranch())
         {
         if (node->getBranchDestination()->getNode()->getLabel() == NULL)
            {
            // need to get the label type from the target block for RAS
            TR::LabelSymbol * label =
                TR::LabelSymbol::create(self()->trHeapMemory(),self(),node->getBranchDestination()->getNode()->getBlock());

            node->getBranchDestination()->getNode()->setLabel(label);

            }
         }
      else if (opcode.isSwitch())
         {
         uint16_t upperBound = node->getCaseIndexUpperBound();
         for (int i = 1; i < upperBound; ++i)
            {
            if (node->getChild(i)->getBranchDestination()->getNode()->getLabel() == NULL)
               {
               TR::LabelSymbol *label = generateLabelSymbol(self());
               node->getChild(i)->getBranchDestination()->getNode()->setLabel(label);
               }
            }
         }
      else if (opcode.isCall() || opcode.getOpCodeValue() == TR::arraycopy)
         {
         self()->setUpStackSizeForCallNode(node);
         }

      }

  }

/**
 * \brief The uncommon-call-constant-node pass is an extra pre-codegen pass
 * before instruction selection. It aims to avoid the situation where
 * a constant integer is made alive across calls and, thereby, lower the preserved register
 * pressure caused due to this.
 *
 * \details
 *
 * \verbatim
 * Take z/Architecture as an example, we could run into the following situation where constant
 * integer 16 is loaded to a preserved register GPR6, which is then loaded to GPR2 as call
 * parameter. The GPR6 will be preserved by the callee and can incur extra register shuffling.
 *
 * LHI     GPR6,0x10           # immediate value in preserved register
 * LGR     GPR2,GPR6           # load parameter from preserved register again
 * LG      GPR14,#609 -216(GPR7)
 * LGHI    GPR0,0xff28
 * BASR    GPR14,GPR14         # call virtual function, which will unnecessarily preserve GPR6.
 * LGR     GPR2,GPR6           # load parameter from preserved register again for the next use
 * ...
 *
 * \endverbatim
 *
 * Loading integer constants as immediate values [i.e. LHI GPR2, 0x10] is preferred.
 * The ranges for immediate value loading on different platforms are governed by the following
 * platform-specific query:
 *
 * \verbatim
 *   bool materializesLargeConstants()
 *   bool shouldValueBeInACommonedNode(int64_t value)
 * \endverbatim
 *
*/
void
OMR::CodeGenerator::uncommonCallConstNodes()
   {
   TR::Compilation* comp = self()->comp();
   if(comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "Performing uncommon call constant nodes\n");
      }

   TR::NodeChecklist checklist(comp);

   for (TR::TreeTop* tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
       TR::Node* node = tt->getNode();
       if(node->getNumChildren() >= 1 &&
               node->getFirstChild()->getOpCode().isFunctionCall())
          {
          TR::Node* callNode = node->getFirstChild();
          if(checklist.contains(callNode))
             {
             if(comp->getOption(TR_TraceCG))
                {
                traceMsg(comp, "Skipping previously visited call node %d\n", callNode->getGlobalIndex());
                }
             continue;
             }

          checklist.add(callNode);

          for(uint32_t i = 0; i < callNode->getNumChildren(); ++i)
             {
             TR::Node* paramNode = callNode->getChild(i);
             if(paramNode->getReferenceCount() > 1 &&
                     paramNode->getOpCode().isLoadConst() &&
                     !self()->isMaterialized(paramNode))
                {
                if(self()->comp()->getOption(TR_TraceCG))
                   {
                   traceMsg(comp, "Uncommon const node %X [n%dn]\n",
                            paramNode, paramNode->getGlobalIndex());
                   }

                TR::Node* newConstNode = TR::Node::create(paramNode->getOpCodeValue(), 0);
                newConstNode->setConstValue(paramNode->getConstValue());
                callNode->setAndIncChild(i, newConstNode);
                paramNode->decReferenceCount();
                }
             }
          }
      }
   }

void
OMR::CodeGenerator::doInstructionSelection()
   {
   TR::Compilation *comp = self()->comp();

   // Set default value for pre-prologue size
   //
   self()->setPrePrologueSize(0);

   if (comp->getOption(TR_TraceCG))
      {
      diagnostic("\n<selection>");
      }

   if (comp->getOption(TR_TraceCG) || debug("traceGRA"))
      {
      self()->getDebug()->setupToDumpTreesAndInstructions("Performing Instruction Selection");
      }

   self()->beginInstructionSelection();

   TR_BitVector *liveLocals = self()->getLiveLocals();
   TR_BitVector nodeChecklistBeforeDump(comp->getNodeCount(), self()->trMemory(), stackAlloc, growable);

   for (TR::TreeTop *tt = comp->getStartTree(); tt; tt = self()->getCurrentEvaluationTreeTop()->getNextTreeTop())
      {
      TR::Instruction *prevInstr = self()->getAppendInstruction();
      TR::Node *node = tt->getNode();
      TR::ILOpCodes opCode = node->getOpCodeValue();

      TR::Node *firstChild = node->getNumChildren() > 0 ? node->getFirstChild() : 0;

      if (opCode == TR::BBStart)
         {
         TR::Block *block = node->getBlock();
         self()->setCurrentEvaluationBlock(block);

         if (!block->isExtensionOfPreviousBlock())
            {
            // If we are keeping track of live locals, set up the live locals for
            // this block
            //
            if (liveLocals)
               {
               if (block->getLiveLocals())
                  {
                  liveLocals = new (self()->trHeapMemory()) TR_BitVector(*block->getLiveLocals());
                  }
               else
                  {
                  liveLocals = new (self()->trHeapMemory()) TR_BitVector(*liveLocals);
                  liveLocals->empty();
                  }
               }
            }

         if (self()->getDebug())
            {
            self()->getDebug()->roundAddressEnumerationCounters();
            }

#if DEBUG
         // Verify that we are only being more conservative by inheriting the live
         // local information from the previous block.
         //
         else if (liveLocals && debug("checkBlockEntryLiveLocals"))
            {
            TR_BitVector *extendedBlockLocals = new (self()->trStackMemory()) TR_BitVector(*(block->getLiveLocals()));
            *extendedBlockLocals -= *(liveLocals);

            TR_ASSERT(extendedBlockLocals->isEmpty(),
                   "Live local information is *less* pessimistic!\n");
            }
#endif
         }

      self()->setLiveLocals(liveLocals);

      if (comp->getOption(TR_TraceCG) || debug("traceGRA"))
         {
         // any evaluator that handles multiple trees will need to dump
         // the others
         self()->getDebug()->saveNodeChecklist(nodeChecklistBeforeDump);
         self()->getDebug()->dumpSingleTreeWithInstrs(tt, NULL, true, false, true, true);
         traceMsg(comp, "\n------------------------------\n");
         }

      self()->setCurrentEvaluationTreeTop(tt);
      self()->setImplicitExceptionPoint(NULL);

      // -----------------------------------------------------------------------
      // Node evaluation
      //
      self()->evaluate(node);

      if (comp->getOption(TR_TraceCG) || debug("traceGRA"))
         {
         TR::Instruction *lastInstr = self()->getAppendInstruction();
         tt->setLastInstruction(lastInstr == prevInstr ? 0 : lastInstr);
         }

      if (liveLocals)
         {
         TR::AutomaticSymbol * liveSym = 0;
         if (debug("checkBlockEntryLiveLocals"))
            {
            // Check for a store into a local.
            // If so, this local becomes live at this point.
            //
            if (node->getOpCode().isStore())
               {
               liveSym = node->getSymbol()->getAutoSymbol();
               }

            // Check for a loadaddr of a local object.
            // If so, this local object becomes live at this point.
            //
            else if (opCode == TR::treetop)
               {
               if (firstChild->getOpCodeValue() == TR::loadaddr)
                  {
                  liveSym = firstChild->getSymbol()->getLocalObjectSymbol();
                  }
               }

            if (liveSym && liveSym->getLiveLocalIndex() == (uint16_t)-1)
               {
               liveSym = NULL;
               }
            }
         else
            {
            // Check for a store into a collected local reference.
            // If so, this local becomes live at this point.
            //
            if (opCode == TR::astore)
               {
               liveSym = node->getSymbol()->getAutoSymbol();
               }

            // Check for a loadaddr of a local object containing collected references.
            // If so, this local object becomes live at this point.
            //
            else if (opCode == TR::treetop)
               {
               if (firstChild->getOpCodeValue() == TR::loadaddr)
                  {
                  liveSym = firstChild->getSymbol()->getLocalObjectSymbol();
                  }
               }

            if (liveSym && !liveSym->isCollectedReference())
               {
               liveSym = NULL;
               }
            }

         if (liveSym)
            {
            liveLocals = new (self()->trHeapMemory()) TR_BitVector(*liveLocals);
            liveLocals->set(liveSym->getLiveLocalIndex());
            }
         }

      if (comp->getOption(TR_TraceCG) || debug("traceGRA"))
         {
         self()->getDebug()->restoreNodeChecklist(nodeChecklistBeforeDump);
         if (tt == self()->getCurrentEvaluationTreeTop())
            {
            traceMsg(comp, "------------------------------\n");
            self()->getDebug()->dumpSingleTreeWithInstrs(tt, prevInstr->getNext(), true, true, true, false);
            }
         else
            {
            // dump all the trees that the evaluator handled
            traceMsg(comp, "------------------------------");
            for (TR::TreeTop *dumptt = tt; dumptt != self()->getCurrentEvaluationTreeTop()->getNextTreeTop(); dumptt = dumptt->getNextTreeTop())
               {
               traceMsg(comp, "\n");
               self()->getDebug()->dumpSingleTreeWithInstrs(dumptt, NULL, true, false, true, false);
               }

            // all instructions are on the tt tree
            self()->getDebug()->dumpSingleTreeWithInstrs(tt, prevInstr->getNext(), false, true, false, false);
            }

         trfflush(comp->getOutFile());
         }
      }

#if defined(TR_TARGET_S390)
   // Virtual function insertInstructionPrefetches is implemented only for s390 platform,
   // for all other platforms the function is empty
   //
   self()->insertInstructionPrefetches();
#endif

   if (comp->getOption(TR_TraceCG) || debug("traceGRA"))
      {
      comp->incVisitCount();
      }

   if (self()->getDebug())
      {
      self()->getDebug()->roundAddressEnumerationCounters();
      }

   self()->endInstructionSelection();

   if (comp->getOption(TR_TraceCG))
      {
      diagnostic("</selection>\n");
      }
   }

void
OMR::CodeGenerator::doRegisterAssignment(TR_RegisterKinds kindsToAssign)
   {
   TR::Instruction *prevInstr = NULL;
   TR::Instruction *currInstr = self()->getAppendInstruction();

   if (!self()->isOutOfLineColdPath())
      {
      auto *firstTimeLiveOOLRegisterList = new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::Register*>(self()->comp()->allocator()));
      self()->setFirstTimeLiveOOLRegisterList(firstTimeLiveOOLRegisterList);

      auto *spilledRegisterList = new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::CFGEdge*>(self()->comp()->allocator()));
      self()->setSpilledRegisterList(spilledRegisterList);
      }

   if (self()->getDebug())
      {
      self()->getDebug()->startTracingRegisterAssignment();
      }

   while (currInstr)
      {
      prevInstr = currInstr->getPrev();

      self()->tracePreRAInstruction(currInstr);

      if (currInstr->getNode()->getOpCodeValue() == TR::BBEnd)
         {
         self()->comp()->setCurrentBlock(currInstr->getNode()->getBlock());
         }

      // Main register assignment procedure
      currInstr->assignRegisters(TR_GPR);

      if (currInstr->isLabel())
         {
         if (currInstr->getLabelSymbol() != NULL)
            {
            if (currInstr->getLabelSymbol()->isStartInternalControlFlow())
               {
               self()->decInternalControlFlowNestingDepth();
               }
            if (currInstr->getLabelSymbol()->isEndInternalControlFlow())
               {
               self()->incInternalControlFlowNestingDepth();
               }
            }
         }

      self()->freeUnlatchedRegisters();
      self()->buildGCMapsForInstructionAndSnippet(currInstr);

      self()->tracePostRAInstruction(currInstr);

      currInstr = prevInstr;
      }

   _afterRA = true;

   if (self()->getDebug())
      {
      self()->getDebug()->stopTracingRegisterAssignment();
      }
   }

bool
OMR::CodeGenerator::use64BitRegsOn32Bit()
   {
#ifdef TR_TARGET_S390
   return self()->comp()->target().is32Bit();
#else
   return false;
#endif // TR_TARGET_S390
   }

TR_PersistentMemory *
OMR::CodeGenerator::trPersistentMemory()
   {
   return self()->trMemory()->trPersistentMemory();
   }

void
OMR::CodeGenerator::addSymbolAndDataTypeToMap(TR::Symbol *symbol, TR::DataType dt)
   {
   _symbolDataTypeMap.Add(symbol,dt);
   }
TR::DataType
OMR::CodeGenerator::getDataTypeFromSymbolMap(TR::Symbol *symbol)
   {
   CS2::HashIndex hi;

   TR::DataType dt = TR::NoType;

   if(_symbolDataTypeMap.Locate(symbol,hi))
      {
      dt = _symbolDataTypeMap[hi];
      }

   return dt;
   }

OMR::CodeGenerator::TR_RegisterPressureSummary *
OMR::CodeGenerator::calculateRegisterPressure()
   {
   return NULL;
   }

void OMR::CodeGenerator::startUsingRegister(TR::Register *reg)
   {
   if (reg != NULL && _liveRegisters[reg->getKind()]) _liveRegisters[reg->getKind()]->addRegister(reg);
   }

void OMR::CodeGenerator::stopUsingRegister(TR::Register *reg)
   {
   if (reg != NULL && _liveRegisters[reg->getKind()]) _liveRegisters[reg->getKind()]->stopUsingRegister(reg);
   }

bool
OMR::CodeGenerator::isRegisterClobberable(TR::Register *reg, uint16_t count)
   {
   if (reg == NULL) return false;

   return (!reg->isLive() ||
           (reg->getLiveRegisterInfo() &&
            reg->getLiveRegisterInfo()->getNodeCount() <= count));
   }

bool
OMR::CodeGenerator::canClobberNodesRegister(
      TR::Node *node,
      uint16_t count,
      TR_ClobberEvalData *data,
      bool ignoreRefCount)
   {
   if (!ignoreRefCount && node->getReferenceCount() > count) return false;

   if (self()->useClobberEvaluate()) return true;

   TR::Register *reg = node->getRegister();
   TR::RegisterPair *regPair = reg->getRegisterPair();

   TR_ASSERT(reg != NULL, "Node should have been evaluated\n");

   if (!regPair)
      {
      bool clobber = self()->isRegisterClobberable(reg, count);
      if (data && clobber)
         {
         data->setClobberLowWord();
         }
      return clobber;
      }
   else
      {
      bool clobberHigh = false;
      bool clobberLow = false;

      if(self()->isRegisterClobberable(regPair->getHighOrder(), count))
         {
         clobberHigh = true;
         if (data)
            {
            data->setClobberHighWord();
            }
         }
      if (self()->isRegisterClobberable(regPair->getLowOrder(), count))
         {
         clobberLow = true;
         if (data)
            {
            data->setClobberLowWord();
            }
         }
      return clobberHigh && clobberLow;
      }
   }

bool
OMR::CodeGenerator::getSupportsTLE()
   {
   return self()->getSupportsTM();
   }

/**
 * Query whether [bsil]bitpermute is supported
 *
 * \return True if the opcodes are supported in codegen
 */
bool
OMR::CodeGenerator::getSupportsBitPermute()
   {
   return false;
   }

bool
OMR::CodeGenerator::getSupportsConstantOffsetInAddressing(int64_t value)
   {
   return self()->getSupportsConstantOffsetInAddressing();
   }

void
OMR::CodeGenerator::toggleIsInOOLSection()
   {
   _flags4.set(IsInOOLSection, !self()->getIsInOOLSection());
   }

bool OMR::CodeGenerator::traceBCDCodeGen()
   {
   return self()->comp()->getOption(TR_TraceCG);
   }

void OMR::CodeGenerator::traceBCDEntry(const char *str, TR::Node *node)
   {
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"EVAL: %s 0x%p - start\n",str,node);
   }

void OMR::CodeGenerator::traceBCDExit(const char *str, TR::Node *node)
   {
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"EVAL: %s 0x%p - end\n",str,node);
   }

bool OMR::CodeGenerator::traceSimulateTreeEvaluation()
   {
   return self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator) && !self()->comp()->getOption(TR_TerseRegisterPressureTrace);
   }

bool OMR::CodeGenerator::terseSimulateTreeEvaluation()
   {
   return self()->comp()->getOptions()->trace(OMR::tacticalGlobalRegisterAllocator);
   }


TR::SymbolReference *OMR::CodeGenerator::getSymRef(TR_RuntimeHelper h) { return _symRefTab->getSymRef(h); }


TR::Linkage *
OMR::CodeGenerator::getLinkage(TR_LinkageConventions lc)
   {
   if (lc == TR_None)
      return NULL;
   else
      return _linkages[lc] ? _linkages[lc] : self()->createLinkage(lc);
   }

void OMR::CodeGenerator::initializeLinkage()
   {
   TR::Linkage * linkage = NULL;
   // Allow the project/GlobalCompilationInfo to set the body linkage
   // Expect to call once during initialization
   //
   linkage = self()->createLinkageForCompilation();
   linkage = linkage ? linkage : self()->createLinkage(self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   self()->setLinkage(linkage);
   }

TR::Linkage *OMR::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

bool
OMR::CodeGenerator::mulDecompositionCostIsJustified(int numOfOperations, char bitPosition[], char operationType[], int64_t value)
   {
   if (self()->comp()->getOptions()->trace(OMR::treeSimplification))
      {
      if (numOfOperations <= 3)
         traceMsg(self()->comp(), "MulDecomp cost is justified\n");
      else
         traceMsg(self()->comp(), "MulDecomp cost is too high. numCycle=%i(max:3)\n", numOfOperations);
      }
   return numOfOperations <= 3 && numOfOperations != 0;
   }

uint8_t *
OMR::CodeGenerator::getCodeStart()
   {
   return _binaryBufferStart + self()->getPrePrologueSize() + _jitMethodEntryPaddingSize;
   }

uint32_t
OMR::CodeGenerator::getCodeLength() // cast explicitly
   {
   return (uint32_t)(self()->getCodeEnd() - self()->getCodeStart());
   }

bool
OMR::CodeGenerator::needRelocationsForLookupEvaluationData()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::needClassAndMethodPointerRelocations()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::needRelocationsForStatics()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::needRelocationsForBodyInfoData()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::needRelocationsForPersistentInfoData()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::needRelocationsForPersistentProfileInfoData()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::needRelocationsForCurrentMethodPC()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::needRelocationsForHelpers()
   {
   return self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::isGlobalVRF(TR_GlobalRegisterNumber n)
   {
   return self()->hasGlobalVRF() &&
          n >= self()->getFirstGlobalVRF() &&
          n <= self()->getLastGlobalVRF();
   }

bool
OMR::CodeGenerator::supportsMergingGuards()
   {
   return !self()->comp()->getOption(TR_DisableOSRGuardMerging) &&
            self()->getSupportsVirtualGuardNOPing() &&
            self()->comp()->performVirtualGuardNOPing() &&
            !self()->comp()->compileRelocatableCode();
   }

bool
OMR::CodeGenerator::isGlobalFPR(TR_GlobalRegisterNumber n)
   {
   return !self()->isGlobalGPR(n);
   }

TR_BitVector *
OMR::CodeGenerator::getBlocksWithCalls()
   {
   if (!_blocksWithCalls)
      self()->computeBlocksWithCalls();
   return _blocksWithCalls;
   }

bool OMR::CodeGenerator::profiledPointersRequireRelocation() { return self()->comp()->compileRelocatableCode(); }

bool OMR::CodeGenerator::supportsInternalPointers()
   {
   if (_disableInternalPointers)
      return false;

   return self()->internalPointerSupportImplemented();
   }

uint16_t
OMR::CodeGenerator::getNumberOfGlobalRegisters()
   {
   if (self()->hasGlobalVRF())
      return _lastGlobalVRF + 1;
   else
      return _lastGlobalFPR + 1;
   }

int32_t OMR::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Block *block)
   {
   TR::Node *node = block->getLastRealTreeTop()->getNode();
   return self()->getMaximumNumberOfGPRsAllowedAcrossEdge(node);
   }

TR::Register *OMR::CodeGenerator::allocateCollectedReferenceRegister()
   {
   TR::Register * temp = self()->allocateRegister();
   temp->setContainsCollectedReference();
   return temp;
   }

TR::Register *OMR::CodeGenerator::allocateSinglePrecisionRegister(TR_RegisterKinds rk)
   {
   TR::Register * temp = self()->allocateRegister(rk);
   temp->setIsSinglePrecision();
   return temp;
   }

void OMR::CodeGenerator::apply8BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   { *(int8_t *)cursor += (int8_t)(intptr_t)label->getCodeLocation(); }
void OMR::CodeGenerator::apply12BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label, bool isCheckDisp)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply12BitLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   { *(int16_t *)cursor += (int16_t)(intptr_t)label->getCodeLocation(); }
void OMR::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label,int8_t d, bool isInstrOffset)
   { *(int16_t *)cursor += (int16_t)(intptr_t)label->getCodeLocation(); }
void OMR::CodeGenerator::apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply24BitLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply32BitLabelRelativeRelocation"); }

void OMR::CodeGenerator::addSnippet(TR::Snippet *s)
   {
   _snippetList.push_back(s);
   }

void OMR::CodeGenerator::setCurrentBlock(TR::Block *b)
   {
   _currentBlock = b;
   self()->comp()->setCurrentBlock(b);
   }


bool
OMR::CodeGenerator::useClobberEvaluate()
   {
#if defined(TR_HOST_S390) || defined(TR_HOST_POWER)
   if(!self()->comp()->getOption(TR_DisableEnhancedClobberEval))
      return false;
#endif
   return true;
   }


bool
OMR::CodeGenerator::opCodeIsNoOp(TR::ILOpCode &opCode)
   {
   if (TR::ILOpCode::isOpCodeAnImplicitNoOp(opCode))
      return true;

   return self()->opCodeIsNoOpOnThisPlatform(opCode);
   }

void OMR::CodeGenerator::traceRAInstruction(TR::Instruction *instr)
   {
   const static char * traceEveryInstruction = feGetEnv("TR_traceEveryInstructionDuringRA");
   if (self()->getDebug())
      self()->getDebug()->traceRegisterAssignment(instr, true, traceEveryInstruction ? true : false);
   }

void OMR::CodeGenerator::tracePreRAInstruction(TR::Instruction *instr)
   {
   if (self()->getDebug())
      self()->getDebug()->traceRegisterAssignment(instr, false);
   }

void OMR::CodeGenerator::tracePostRAInstruction(TR::Instruction *instr)
   {
   if (self()->getDebug())
      self()->getDebug()->traceRegisterAssignment(instr, false, true);
   }

void OMR::CodeGenerator::traceRegAssigned(TR::Register *virtReg, TR::Register *realReg)
   {
   if (self()->getDebug())
      self()->getDebug()->traceRegisterAssigned(_regAssignFlags, virtReg, realReg);
   }

void OMR::CodeGenerator::traceRegAssigned(TR::Register *virtReg, TR::Register *realReg, TR_RegisterAssignmentFlags flags)
   {
   if (self()->getDebug())
      self()->getDebug()->traceRegisterAssigned(flags, virtReg, realReg);
   }

void OMR::CodeGenerator::traceRegFreed(TR::Register *virtReg, TR::Register *realReg)
   {
   if (self()->getDebug())
      self()->getDebug()->traceRegisterFreed(virtReg, realReg);
   }

void OMR::CodeGenerator::traceRegInterference(TR::Register *virtReg, TR::Register *interferingVirtual, int32_t distance)
   {
   if (self()->getDebug())
      self()->getDebug()->traceRegisterInterference(virtReg, interferingVirtual, distance);
   }

void OMR::CodeGenerator::traceRegWeight(TR::Register *realReg, uint32_t weight)
   {
   if (self()->getDebug())
      self()->getDebug()->traceRegisterWeight(realReg, weight);
   }

void OMR::CodeGenerator::traceRegisterAssignment(const char *format, ...)
   {
   if (self()->getDebug())
      {
      va_list args;
      va_start(args, format);
      self()->getDebug()->traceRegisterAssignment(format, args);
      va_end(args);
      }
   }

bool
OMR::CodeGenerator::enableRefinedAliasSets()
   {
   if (self()->comp()->getOption(TR_EnableHCR))
      {
      return false;  // Methods can change, so remembered alias info is unreliable
      }

   return
      _enabledFlags.testAny(EnableRefinedAliasSets) &&
      !self()->comp()->getOption(TR_DisableRefinedAliases);
   }








void
OMR::CodeGenerator::generateCode()
   {
   LexicalTimer t("Code Generation", self()->comp()->phaseTimer());
   TR::LexicalMemProfiler mp("Code Generation", self()->comp()->phaseMemProfiler());
   LexicalXmlTag cgMsg(self());

   self()->getCodeGeneratorPhase().performAll();
   }


void
OMR::CodeGenerator::removeUnusedLocals()
   {
   if (self()->comp()->getOption(TR_MimicInterpreterFrameShape))
      return;


   self()->comp()->getMethodSymbol()->removeUnusedLocals();
   }

bool OMR::CodeGenerator::areAssignableGPRsScarce()
   {
   int32_t threshold = 13;
   static char *c1 = feGetEnv("TR_ScarceGPRsThreshold");
   if (c1)
      threshold = atoi(c1);
      return (self()->getMaximumNumbersOfAssignableGPRs() <= threshold);
   }


// returns true iff, based on opcodes and offsets if two store/load/address nodes are definitely disjoint (i.e. guaranteed not to overlap)
// any combination of stores,loads and address nodes are allowed as node1 and node2
// The return tells how node1 and node2 is overlapped (TR_StorageOverlapKind)
TR_StorageOverlapKind
OMR::CodeGenerator::storageMayOverlap(TR::Node *node1, size_t length1, TR::Node *node2, size_t length2)
   {
   if ((node2->getOpCode().isLoadVarOrStore() || node2->getType().isAddress()) && // node1 is usually an always valid store so check node2 first
       (node1->getOpCode().isLoadVarOrStore() || node1->getType().isAddress()))
      {
      TR_StorageInfo node1Info = TR_StorageInfo(node1, length1, self()->comp());
      TR_StorageInfo node2Info = TR_StorageInfo(node2, length2, self()->comp());

      return node1Info.mayOverlapWith(&node2Info);
      }
   else
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"overlap=true : node1 %s (%p) and/or node2 %s (%p) are not valid load/store/address nodes\n",
            node1->getOpCode().getName(),node1,node2->getOpCode().getName(),node2);

      return TR_MayOverlap;
      }
   }


int32_t
OMR::CodeGenerator::arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget)
   {
   return TR::CodeGenerator::defaultArrayTranslateMinimumNumberOfIterations("arrayTranslateMinimumNumberOfElements");
   }

int32_t
OMR::CodeGenerator::arrayTranslateAndTestMinimumNumberOfIterations()
   {
   return TR::CodeGenerator::defaultArrayTranslateMinimumNumberOfIterations("arrayTranslateAndTestMinimumNumberOfIterations");
   }

int32_t
OMR::CodeGenerator::defaultArrayTranslateMinimumNumberOfIterations(const char *methodName)
   {
   if (TR::CodeGenerator::useOldArrayTranslateMinimumNumberOfIterations())
      return INT_MAX;
   TR_ASSERT(false, "%s not implemented, platform codegen should choose an appropriate value", methodName);
   return 10001;
   }


// For platforms with overlapping vector and floating point registers
//
bool
OMR::CodeGenerator::isAliasedGRN(TR_GlobalRegisterNumber n)
     {
     TR_ASSERT(self()->isGlobalVRF(n) || self()->isGlobalFPR(n),
               "Global register number is of incorrect type. Current type: %d (%s)", n, self()->getDebug()->getGlobalRegisterName(n));
     return n >= _firstOverlappedGlobalFPR && n <= _lastOverlappedGlobalFPR; // _lastOverlappedFPR includes _lastOverlappingVector
     }

TR_GlobalRegisterNumber
OMR::CodeGenerator::getOverlappedAliasForGRN(TR_GlobalRegisterNumber n)
   {
   TR_ASSERT(self()->isAliasedGRN(n),
             "Can only get alias from supported types. Current grn : %d (%s)", n, self()->getDebug()->getGlobalRegisterName(n));
   TR_ASSERT(self()->getOverlapOffsetBetweenAliasedGRNs() > 0, "Overlap offset must be set before invoking this method");

   TR_GlobalRegisterNumber aliasedGrn = -1;

   // This is tricky.
   // An example table from a platform with overlapping register types
   // (f|l)      ( |o)              (f|v)
   //  f = first    o = overlapping  f = float
   //  l = last                      v = vector
   // ff 19   lf 50   fof 19  lof 50
   // fv 19   lv 66   fov 35  lov 50

   if (n >= _firstOverlappedGlobalFPR && n < _firstOverlappedGlobalVRF)     // n is a a float grn but not a vector grn
      {
      aliasedGrn = n + self()->getOverlapOffsetBetweenAliasedGRNs();
      TR_ASSERT(self()->isGlobalVRF(aliasedGrn),"Aliased FPR %d did not give an a valid VRF %d", n, aliasedGrn);
      }
   else if (n >= _firstOverlappedGlobalVRF && n <= _lastOverlappedGlobalVRF) // n is a pure overlapped vector grn
      {
      aliasedGrn = n - self()->getOverlapOffsetBetweenAliasedGRNs();
      TR_ASSERT(self()->isGlobalVRF(aliasedGrn),"Aliased VRF %d did not give an a valid FPR %d", n, aliasedGrn);
      }
   else
      {
      TR_ASSERT(false, "Unreachable");
      }

   return aliasedGrn;
   }



bool
OMR::CodeGenerator::isSupportedAdd(TR::Node *addr)
   {
   if (addr->getOpCode().isAdd() && (addr->getType().isAddress() || addr->getType().isInt32() || addr->getType().isIntegral()))
      return true;
   else
      return false;
   }


bool OMR::CodeGenerator::uniqueAddressOccurrence(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop)
   {
   bool duringEvaluation = self()->getCodeGeneratorPhase() > TR::CodeGenPhase::SetupForInstructionSelectionPhase;
   if (!addressesUnderSameTreeTop) return false;

   if (duringEvaluation)
      {
      if (addr1->getRegister() == NULL &&
          addr2->getRegister() == NULL)
         {
         return true;
         }
      }
   else
      {
      if (addr1->getReferenceCount() <= 1 && // refCounts may be zero when called during ilgen and the nodes are freshly popped
          addr2->getReferenceCount() <= 1)
         {
         return true;

         }
      }
   return false;
   }


bool
OMR::CodeGenerator::nodeMatches(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop)
   {
   bool foundMatch = false;
   if (addr1 == addr2)
      {
      foundMatch = true;
      }
   else if (addr1->getOpCodeValue() == TR::loadaddr && addr2->getOpCodeValue() == TR::loadaddr &&
            addr1->getSymbolReference() == addr2->getSymbolReference())
      {
      foundMatch = true;
      }
   else if (addr1->getType().isIntegral() && addr1->getOpCode().isLoadConst() &&
            addr2->getType().isIntegral() && addr2->getOpCode().isLoadConst() &&
            addr1->get64bitIntegralValue() == addr2->get64bitIntegralValue())
      {
      foundMatch = true;
      }
   else if (addressesUnderSameTreeTop &&
            addr1->getOpCodeValue() == TR::i2a && addr2->getOpCodeValue() == TR::i2a &&
            addr1->getFirstChild()->getOpCode().isLoadVarDirect() &&
            addr1->getFirstChild()->getSymbolReference()->getSymbol()->isMethodMetaData() &&
            addr1->getFirstChild()->getOpCodeValue() == addr2->getFirstChild()->getOpCodeValue() &&
            addr1->getFirstChild()->getSize() == addr2->getFirstChild()->getSize() &&
            addr1->getFirstChild()->getSymbolReference() == addr2->getFirstChild()->getSymbolReference())
      {
      // i2a
      //    iload _GPR13
      foundMatch = true;
      }
   else if (self()->uniqueAddressOccurrence(addr1, addr2, addressesUnderSameTreeTop))
      {
      TR::ILOpCode op1 = addr1->getOpCode();
      TR::ILOpCode op2 = addr2->getOpCode();
      if (op1.getOpCodeValue() == op2.getOpCodeValue() &&
            op1.isLoadVar() && op1.getDataType() == TR::Address &&
               addr1->getSymbolReference() == addr2->getSymbolReference())
         {
         // aload, ardbar etc
         if (op1.isLoadDirect())
            foundMatch = true;
         // aloadi, ardbari etc
         else if (op1.isLoadIndirect() && self()->nodeMatches(addr1->getFirstChild(), addr2->getFirstChild(),addressesUnderSameTreeTop))
            foundMatch = true;
         }
      }

   return foundMatch;
   }

bool
OMR::CodeGenerator::additionsMatch(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop)
   {
   bool foundMatch = false;
   TR::Node *addr1ChildOne = addr1->getFirstChild();
   TR::Node *addr2ChildOne = addr2->getFirstChild();
   TR::Node *addr1ChildTwo = addr1->getSecondChild();
   TR::Node *addr2ChildTwo = addr2->getSecondChild();
   if (self()->nodeMatches(addr1ChildOne, addr2ChildOne, addressesUnderSameTreeTop))
      {
      if (self()->nodeMatches(addr1ChildTwo, addr2ChildTwo, addressesUnderSameTreeTop))
         {
         foundMatch = true;
         }
      else if (self()->isSupportedAdd(addr1ChildTwo) && self()->isSupportedAdd(addr2ChildTwo) &&
               self()->additionsMatch(addr1ChildTwo, addr2ChildTwo, addressesUnderSameTreeTop))
         {
         // aladd
         //    loadaddr
         //    ladd
         //       x
         //       y
         //
         // aladd
         //    ==>loadaddr
         //    ladd
         //       ==>x
         //       ==>y
         foundMatch = true;
         }
      }
   else if (self()->nodeMatches(addr1ChildTwo, addr2ChildTwo, addressesUnderSameTreeTop) &&
            self()->isSupportedAdd(addr1ChildOne) && self()->isSupportedAdd(addr2ChildOne) &&
            self()->additionsMatch(addr1ChildOne, addr2ChildOne, addressesUnderSameTreeTop))
      {
      // aiadd
      //    aiadd                   // addr1ChildOne
      //       aload cached_static
      //       iconst x
      //    iconst y                // addr1ChildTwo

      // aiadd
      //    aiadd                   // addr2ChildOne
      //       aload cached_static
      //       iconst x
      //    iconst y                // addr2ChildTwo
      foundMatch = true;
      }
   else if (self()->isSupportedAdd(addr1ChildOne) &&
            addr1ChildOne->getFirstChild() == addr2ChildOne &&
            addr1ChildOne->getSecondChild()->getOpCode().isLoadConst() &&
            addr1ChildTwo->getOpCode().isLoadConst() &&
            addr2ChildTwo->getOpCode().isLoadConst())
      {
      // aladd
      //    aladd       addr1ChildOne
      //       aload
      //       lconst x
      //    lconst y
      //
      // aladd
      //    ==>aload     addr2ChildOne
      //    lconst x+y

      int64_t offsetOne = addr1ChildOne->getSecondChild()->get64bitIntegralValue() + addr1ChildTwo->get64bitIntegralValue();
      int64_t offsetTwo = addr2ChildTwo->get64bitIntegralValue();
/*
      traceMsg(comp(),"adding offsets from %p and %p (%lld + %lld) and %p (%lld) (found base match %s %p)\n",
         addr1ChildOne->getSecondChild(),addr1ChildTwo,
         addr1ChildOne->getSecondChild()->get64bitIntegralValue(),addr1ChildTwo->get64bitIntegralValue(),
         addr2ChildTwo,addr2ChildTwo->get64bitIntegralValue(),
         addr2ChildOne->getOpCode().getName(),addr2ChildOne);
*/
      if (offsetOne == offsetTwo)
         {
         foundMatch = true;
         }
      }
   return foundMatch;
   }

bool
OMR::CodeGenerator::addressesMatch(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop)
   {
   bool foundMatch = false;

   if (self()->nodeMatches(addr1, addr2, addressesUnderSameTreeTop))
      {
      foundMatch = true;
      }
   else
      {
      if (self()->isSupportedAdd(addr1) && self()->isSupportedAdd(addr2))
         {
         if (self()->additionsMatch(addr1, addr2, addressesUnderSameTreeTop))
            {
            foundMatch = true;
            }
         else if (addr1->getFirstChild() && addr2->getFirstChild() &&
                  self()->isSupportedAdd(addr1->getFirstChild()) && self()->isSupportedAdd(addr2->getFirstChild()))
            {
            bool firstChildrenMatch = self()->additionsMatch(addr1->getFirstChild(), addr2->getFirstChild(), addressesUnderSameTreeTop);
            bool secondChildrenMatch = false;

            if (firstChildrenMatch &&
                addr1->getSecondChild() && addr2->getSecondChild() &&
                self()->isSupportedAdd(addr1->getSecondChild()) && self()->isSupportedAdd(addr2->getSecondChild()))
               {
               secondChildrenMatch = self()->additionsMatch(addr1->getSecondChild(), addr2->getSecondChild(), addressesUnderSameTreeTop);
               }

            foundMatch = firstChildrenMatch && secondChildrenMatch;
            }
         }
      }

   if (!foundMatch && addressesUnderSameTreeTop)
      {
      bool duringEvaluation = self()->getCodeGeneratorPhase() > TR::CodeGenPhase::SetupForInstructionSelectionPhase;
      // if the caller asserts the addresses are under the same treetop (so no intervening kills are possible)
      //  and one of the mutually exclusive tests below are true then addresses will also match
      // 1) before evaluation : refCounts of both nodes are 1 (a more refined test, that requires scanning ahead in the block, is if both are first references)
      // 2) during evaluation : registers are both NULL (i.e. both first ref)

      if (self()->isSupportedAdd(addr1) && self()->isSupportedAdd(addr2) &&
          self()->nodeMatches(addr1->getSecondChild(), addr2->getSecondChild(), addressesUnderSameTreeTop))
         {
         // for the case where the iaload itself is below an addition, match the additions first
         // aiadd
         //    iaload
         //       aload
         //    iconst
         addr1 = addr1->getFirstChild();
         addr2 = addr2->getFirstChild();
#ifdef INPSECT_SUPPORT
         if (traceInspect())
            traceMsg(comp(),"\t\tfound possibly matching additions : update addr1 to %s (%p), addr2 to %s (%p) and continue matching\n",
               addr1->getOpCode().getName(),addr1,addr2->getOpCode().getName(),addr2);
#endif
         }

      if (addr1->getOpCodeValue() == TR::aloadi &&
          addr2->getOpCodeValue() == TR::aloadi &&
          addr1->getSymbolReference() == addr2->getSymbolReference() &&
          self()->addressesMatch(addr1->getFirstChild(), addr2->getFirstChild()) &&
          self()->uniqueAddressOccurrence(addr1, addr2, addressesUnderSameTreeTop))
         {
         foundMatch = true;
         }
      }

   return foundMatch;
   }


void
OMR::CodeGenerator::reserveCodeCache()
   {
   int32_t numReserved = 0;
   int32_t compThreadID = 0;

   _codeCache = TR::CodeCacheManager::instance()->reserveCodeCache(false, 0, compThreadID, &numReserved);

   if (!_codeCache) // Cannot reserve a cache; all are used
      {
      TR::Compilation *comp = self()->comp();

      // We may reach this point if all code caches have been used up.
      // If some code caches have some space but cannot be used because they are reserved
      // we will throw an exception in the call to TR::CodeCacheManager::reserveCodeCache
      //
      if (comp->compileRelocatableCode())
         {
         comp->failCompilation<TR::RecoverableCodeCacheError>("Cannot reserve code cache");
         }

      comp->failCompilation<TR::CodeCacheError>("Cannot reserve code cache");
      }
   }

uint8_t *
OMR::CodeGenerator::allocateCodeMemoryInner(
      uint32_t warmCodeSizeInBytes,
      uint32_t coldCodeSizeInBytes,
      uint8_t **coldCode,
      bool isMethodHeaderNeeded)
   {
   TR::CodeCache *codeCache = self()->getCodeCache();

   TR_ASSERT(codeCache->isReserved(), "Code cache should have been reserved.");

   uint8_t *warmCode = TR::CodeCacheManager::instance()->allocateCodeMemory(
         warmCodeSizeInBytes,
         coldCodeSizeInBytes,
         &codeCache,
         coldCode,
         false,
         isMethodHeaderNeeded);

   if (codeCache != self()->getCodeCache())
      {
      // Either we didn't get a code cache, or the one we got should be reserved
      TR_ASSERT(!codeCache || codeCache->isReserved(), "Substitute code cache isn't marked as reserved");
      self()->comp()->setRelocatableMethodCodeStart(warmCode);
      self()->switchCodeCacheTo(codeCache);
      }

   if (warmCode == NULL)
      {
      TR::Compilation *comp = self()->comp();

      if (TR::CodeCacheManager::instance()->codeCacheFull())
         {
         comp->failCompilation<TR::CodeCacheError>("Code Cache Full");
         }
      else
         {
         comp->failCompilation<TR::RecoverableCodeCacheError>("Failed to allocate code memory");
         }
      }

   TR_ASSERT( !((warmCodeSizeInBytes && !warmCode) || (coldCodeSizeInBytes && !coldCode)), "Allocation failed but didn't throw an exception");

   return warmCode;
   }

uint8_t *
OMR::CodeGenerator::allocateCodeMemory(uint32_t warmCodeSizeInBytes, uint32_t coldCodeSizeInBytes, uint8_t **coldCode, bool isMethodHeaderNeeded)
   {
   uint8_t *warmCode;
   warmCode = self()->allocateCodeMemoryInner(warmCodeSizeInBytes, coldCodeSizeInBytes, coldCode, isMethodHeaderNeeded);

   if (self()->getCodeGeneratorPhase() == TR::CodeGenPhase::BinaryEncodingPhase)
      {
      self()->commitToCodeCache();
      }

   TR_ASSERT( !((warmCodeSizeInBytes && !warmCode) || (coldCodeSizeInBytes && !coldCode)), "Allocation failed but didn't throw an exception");
   return warmCode;
   }

uint8_t *
OMR::CodeGenerator::allocateCodeMemory(uint32_t codeSizeInBytes, bool isCold, bool isMethodHeaderNeeded)
   {
   uint8_t *coldCode;
   if (isCold)
      {
      self()->allocateCodeMemory(0, codeSizeInBytes, &coldCode, isMethodHeaderNeeded);
      return coldCode;
      }
   return self()->allocateCodeMemory(codeSizeInBytes, 0, &coldCode, isMethodHeaderNeeded);
   }

void
OMR::CodeGenerator::trimCodeMemoryToActualSize()
   {
   uint8_t *bufferStart = self()->getBinaryBufferStart();
   size_t actualCodeLengthInBytes = self()->getCodeEnd() - bufferStart;

   self()->getCodeCache()->trimCodeMemoryAllocation(bufferStart, actualCodeLengthInBytes);
   }

bool
OMR::CodeGenerator::convertMultiplyToShift(TR::Node * node)
   {
   // See if the multiply node can be strength-reduced to a shift.
   // If the multiplier is a negative constant, create a shift as if the constant
   // was positive and the caller will add the appropriate negation.
   // Return true if strength reduction was possible.

   TR::Node *secondChild = node->getSecondChild();
   TR_ASSERT(node->getOpCode().isMul(), "error in strength reduction");

   // Get the constant value
   //
   if (!secondChild->getOpCode().isLoadConst())
      return false;

   // See if the second child is a power of two
   //
   int32_t multiplier = 0;
   int32_t shiftAmount = 0;
   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      int64_t longMultiplier = secondChild->getLongInt();
      if (longMultiplier == 0)
         return false;  // Can't handle this case
      if (longMultiplier < 0)
         longMultiplier = -longMultiplier;
      int32_t longHigh = longMultiplier >> 32;
      int32_t longLow = longMultiplier & 0xFFFFFFFF;
      if (longHigh == 0)
         multiplier = longLow;
      else if (longLow == 0)
         {
         multiplier = longHigh;
         shiftAmount = 32;
         }
      else
         return false;
      }
   else
      {
      multiplier = secondChild->get32bitIntegralValue();
      if (multiplier == 0)
         return false;  // Can't handle this case
      if (multiplier < 0)
         multiplier = -multiplier;
      }



   if (!isNonNegativePowerOf2(multiplier) && multiplier != TR::getMinSigned<TR::Int32>())
      return false;

   // Calculate the shift amount
   //
   while ((multiplier = ((uint32_t)multiplier) >> 1))
      ++shiftAmount;

   // Change the multiply into a shift.  We must create a new node for the new
   // constant as it might be referenced in a snippet.
   //
   self()->decReferenceCount(secondChild);
   secondChild = TR::Node::create(secondChild, TR::iconst, 0);
   node->setAndIncChild(1, secondChild);

   if (node->getOpCodeValue() == TR::imul)
      TR::Node::recreate(node, TR::ishl);
   else if (node->getOpCodeValue() == TR::smul)
      TR::Node::recreate(node, TR::sshl);
   else if (node->getOpCodeValue() == TR::bmul)
      TR::Node::recreate(node, TR::bshl);
   else
      {
      TR::Node::recreate(node, TR::lshl);
      TR::Node::recreate(secondChild, TR::iconst);
      }
   secondChild->setInt(shiftAmount);
   return true;
   }


bool
OMR::CodeGenerator::isMemoryUpdate(TR::Node *node)
   {
   if (self()->comp()->getOption(TR_DisableDirectMemoryOps))
      return false;

   // See if the given store node can be represented by a direct operation on the
   // node's memory location.
   // If so return true.
   //
   TR::Node *valueChild = (node->getOpCode().isIndirect()) ? node->getSecondChild() : node->getFirstChild();

   // Don't bother if the calculation has already been done or is needed later.
   //
   if (valueChild->getRegister() || valueChild->getReferenceCount() > 1)
      return false;

   if (valueChild->getNumChildren() != 2)
      return false;

   // Multiplies on x86 do not have direct memory forms.  This check is done here
   // rather than in the caller because this function can have the unexpected side
   // effect of swapping children of commutative operations in order to expose the
   // opportunity.
   //
   if (self()->comp()->target().cpu.isX86() && valueChild->getOpCode().isMul())
      {
      return false;
      }

   // See if one of the operands of the value child is a load of the same memory
   // location being stored.
   // Note that if the load has already been stored in a register we don't know
   // if it represents the latest value in the memory location so we can't do
   // a memory operation.
   //
   int32_t loadIndex;
   for (loadIndex = 0; loadIndex <= 1; loadIndex++)
      {
      TR::Node *loadNode = valueChild->getChild(loadIndex);
      if (loadNode->getRegister())
         continue;

      // See if this is a load of the same symbol as the store
      //
      if (!loadNode->getOpCode().isLoadVar() ||
          loadNode->getSymbol() != node->getSymbol() ||
          loadNode->getSymbolReference()->getOffset() != node->getSymbolReference()->getOffset())
         {
         continue;
         }

      // If the load and store are direct, they match.
      // If they are indirect, they match if they use the same base node
      //
      if (!node->getOpCode().isIndirect())
         break;
      else
         {
         if (node->getFirstChild() == loadNode->getFirstChild())
            break;
         // make sure the address nodes matches (aladd, etc.)
         if (self()->addressesMatch(node->getFirstChild(), loadNode->getFirstChild(), true))
            break;
         }
      }

   // If the load node was not found this can't be a memory increment
   //
   if (loadIndex > 1)
      return false;

   // The load node may be the first or second child of the value child.
   // If it is the second, this can only be a memory increment if the operands
   // can be swapped
   //
   if (loadIndex > 0)
      {
      if (!valueChild->getOpCode().isCommutative())
         return false;
      valueChild->swapChildren();
      }

   return true;
   }

void
OMR::CodeGenerator::processRelocations()
   {
   //
   auto iterator = _relocationList.begin();
   while(iterator != _relocationList.end())
      {
      // Traverse the non-AOT/non-external labels first
      (*iterator)->apply(self());
      ++iterator;
      }
   }

#if defined(TR_HOST_ARM)
void armCodeSync(uint8_t *start, uint32_t size);
#elif defined(TR_HOST_ARM64)
void arm64CodeSync(uint8_t *start, uint32_t size);
#elif defined(TR_HOST_POWER)
void ppcCodeSync(uint8_t *start, uint32_t size);
#else
   // S390, IA32 and AMD64 do not need code sync?
#endif

void
OMR::CodeGenerator::syncCode(uint8_t *start, uint32_t size)
   {
#if defined(TR_HOST_ARM)
   armCodeSync(start, size);
#elif defined(TR_HOST_ARM64)
   arm64CodeSync(start, size);
#elif defined(TR_HOST_POWER)
   ppcCodeSync(start, size);
#else
   // S390, IA32 and AMD64 do not need code sync?
#endif
   }

void
OMR::CodeGenerator::compute32BitMagicValues(
      int32_t  d,
      int32_t *m,
      int32_t *s)
   {
   int p, first, last, mid;
   unsigned int ad, anc, delta, q1, r1, q2, r2, t;
   const unsigned int two31 = 2147483648u;

   // Cache some common denominators and their magic values.  The key values in this
   // array MUST be in numerically increasing order for the binary search to work.
   #define NUM_32BIT_MAGIC_VALUES 11
   static uint32_t div32BitMagicValues[NUM_32BIT_MAGIC_VALUES][3] =

   //     Denominator   Magic Value   Shift

      { {           3,   0x55555556,      0 },
        {           5,   0x66666667,      1 },
        {           7,   0x92492493,      2 },
        {           9,   0x38e38e39,      1 },
        {          10,   0x66666667,      2 },
        {          12,   0x2aaaaaab,      1 },
        {          24,   0x2aaaaaab,      2 },
        {          60,   0x88888889,      5 },
        {         100,   0x51eb851f,      5 },
        {        1000,   0x10624dd3,      6 },
        {        3600,   0x91a2b3c5,     11 } };

   // Quick check if 'd' is cached.
   first = 0;
   last  = NUM_32BIT_MAGIC_VALUES-1;
   while (first <= last)
      {
      mid = (first + last) / 2;
      if (div32BitMagicValues[mid][0] == d)
         {
         *m = div32BitMagicValues[mid][1];
         *s = div32BitMagicValues[mid][2];
         return;
         }
      else if (unsigned(d) > div32BitMagicValues[mid][0])
         {
         first = mid+1;
         }
      else
         {
         last = mid-1;
         }
      }

   // Values for 'd' were not found.  Compute them.
   ad = abs(d);
   t = two31 + ((unsigned int)d >> 31);
   anc = t - 1 - t%ad;
   p = 31;
   q1 = two31/anc;
   r1 = two31 - q1*anc;
   q2 = two31/ad;
   r2 = two31 - q2*ad;

   do
      {
      p = p + 1;
      q1 = 2*q1;
      r1 = 2*r1;
      if (r1 >= anc)
         {
         q1 = q1 + 1;
         r1 = r1 - anc;
         }

      q2 = 2*q2;
      r2 = 2*r2;
      if (r2 >= ad)
         {
         q2 = q2 + 1;
         r2 = r2 - ad;
         }

      delta = ad - r2;
      } while (q1 < delta || (q1 == delta && r1 == 0));

   *m = q2 + 1;
   if (d<0) *m = -(*m);
   *s = p - 32;
   }


void
OMR::CodeGenerator::compute64BitMagicValues(
      int64_t  d,
      int64_t *m,
      int64_t *s)
   {
   int p, first, last, mid;
   uint64_t ad, anc, delta, q1, r1, q2, r2, t;
   const uint64_t two63 = (uint64_t)1<<63;

   // Cache some common denominators and their magic values.  The key values in this
   // array MUST be in numerically increasing order for the binary search to work.

   #define NUM_64BIT_MAGIC_VALUES 6
   static int64_t div64BitMagicValues[NUM_64BIT_MAGIC_VALUES][3] =
   //     Denominator                     Magic Value   Shift

      { {           3, CONSTANT64(0x5555555555555556),      0 },
        {           5, CONSTANT64(0x6666666666666667),      1 },
        {           7, CONSTANT64(0x4924924924924925),      1 },
        {           9, CONSTANT64(0x1c71c71c71c71c72),      0 },
        {          10, CONSTANT64(0x6666666666666667),      2 },
        {          12, CONSTANT64(0x2aaaaaaaaaaaaaab),      1 } };

   // Quick check if 'd' is cached.
   first = 0;
   last  = NUM_64BIT_MAGIC_VALUES-1;
   while (first <= last)
      {
      mid = (first + last) / 2;
      if (div64BitMagicValues[mid][0] == d)
         {
         *m = div64BitMagicValues[mid][1];
         *s = div64BitMagicValues[mid][2];
         return;
         }
      else if (d > div64BitMagicValues[mid][0])
         {
         first = mid+1;
         }
      else
         {
         last = mid-1;
         }
      }

   // Values for 'd' were not found: compute them.

   // Compiler independent 64-bit absolute value.
   ad = (d >= (int64_t)0) ? d : ((int64_t)0-d);

   t = two63 + ((uint64_t)d >> 63);
   anc = t - 1 - t%ad;
   p = 63;
   q1 = two63/anc;
   r1 = two63 - q1*anc;
   q2 = two63/ad;
   r2 = two63 - q2*ad;

   do
      {
      p = p + 1;
      q1 = 2*q1;
      r1 = 2*r1;
      if (r1 >= anc)
         {
         q1 = q1 + 1;
         r1 = r1 - anc;
         }

      q2 = 2*q2;
      r2 = 2*r2;
      if (r2 >= ad)
         {
         q2 = q2 + 1;
         r2 = r2 - ad;
         }

      delta = ad - r2;
      } while (q1 < delta || (q1 == delta && r1 == 0));

   *m = q2 + 1;
   if (d<0) *m = -(*m);
   *s = p - 64;
   }


uint64_t
OMR::CodeGenerator::computeUnsigned64BitMagicValues(uint64_t d, int32_t* s, int32_t* a)
   {
   int p;
   uint64_t m; /* magic number */
   uint64_t nc, delta, q1, r1, q2, r2;

   *a = 0; /* initialize "add" indicator */
   nc = -1 - ((~d) + 1)%d;
   p = 63; /* initialize p */
   q1 = 0x8000000000000000ull/nc; /* initialize 2**p/nc */
   r1 = 0x8000000000000000ull- q1*nc; /* initialize rem(2**p,nc) */
   q2 = 0x7fffffffffffffffull/d; /* initialize (2**p - 1)/d */
   r2 = 0x7fffffffffffffffull - q2*d; /* initialize rem(2**p -1, d) */

loop:
   p = p + 1;
   if (r1 >= nc - r1)
      {	             /* update quotient and */
      q1 = 2*q1 + 1; /* remainder of 2**p/nc */
      r1 = 2*r1 - nc;
      }
   else
      {
      q1 = 2*q1;
      r1 = 2*r1;
      }

   if (r2 + 1 >= d - r2)
      {	/* update quotient and */
        /* remainder of (2**p - 1)/d */
      if (q2 >= 0x7fffffffffffffffull) *a = 1;
      q2 = 2*q2 + 1;
      r2 = 2*r2 + 1 - d;
      }
   else
      {
      if (q2 >= 0x8000000000000000ull) *a = 1;
      q2 = 2*q2;
      r2 = 2*r2 + 1;
      }

   delta = d - 1 - r2;
   if (p < 128 && (q1 < delta || (q1==delta && r1==0))) goto loop;

   m = q2 + 1;
   *s = p - 64;
   return m; /* return magic number */
   }


uint8_t *
OMR::CodeGenerator::alignBinaryBufferCursor()
   {
   uint32_t boundary = self()->getJitMethodEntryAlignmentBoundary();

   TR_ASSERT_FATAL(boundary > 0, "JIT method entry alignment boundary (%d) definition is violated", boundary);

   // Align cursor to boundary as long as it meets the threshold
   if (self()->supportsJitMethodEntryAlignment() && boundary > 1)
      {
      uint32_t offset = self()->getPreJitMethodEntrySize();

      uint8_t* alignedBinaryBufferCursor = _binaryBufferCursor;
      alignedBinaryBufferCursor += offset;
      alignedBinaryBufferCursor = reinterpret_cast<uint8_t*>(OMR::align(reinterpret_cast<size_t>(alignedBinaryBufferCursor), boundary));

      TR_ASSERT_FATAL(OMR::aligned(reinterpret_cast<size_t>(alignedBinaryBufferCursor), boundary),
         "alignedBinaryBufferCursor [%p] is not aligned to the specified boundary (%d)", alignedBinaryBufferCursor, boundary);

      alignedBinaryBufferCursor -= offset;
      _binaryBufferCursor = alignedBinaryBufferCursor;
      self()->setJitMethodEntryPaddingSize(static_cast<uint32_t>(_binaryBufferCursor - _binaryBufferStart));
      memset(_binaryBufferStart, 0, self()->getJitMethodEntryPaddingSize());
      }

   return _binaryBufferCursor;
   }

bool
OMR::CodeGenerator::supportsJitMethodEntryAlignment()
   {
   return true;
   }

uint32_t
OMR::CodeGenerator::getJitMethodEntryAlignmentBoundary()
   {
   return 1;
   }

int32_t
OMR::CodeGenerator::setEstimatedLocationsForSnippetLabels(int32_t estimatedSnippetStart)
   {
   self()->setEstimatedSnippetStart(estimatedSnippetStart);

   for (auto iterator = _snippetList.begin(); iterator != _snippetList.end(); ++iterator)
      {
      (*iterator)->setEstimatedCodeLocation(estimatedSnippetStart);
      estimatedSnippetStart += (*iterator)->getLength(estimatedSnippetStart);
      }

   if (self()->hasDataSnippets())
      {
      estimatedSnippetStart = self()->setEstimatedLocationsForDataSnippetLabels(estimatedSnippetStart);
      }

   return estimatedSnippetStart;
   }

uint8_t *
OMR::CodeGenerator::emitSnippets()
   {
   uint8_t *codeOffset;
   uint8_t *retVal;

   omrthread_jit_write_protect_disable();

   for (auto iterator = _snippetList.begin(); iterator != _snippetList.end(); ++iterator)
      {
      codeOffset = (*iterator)->emitSnippet();
      if (codeOffset != NULL)
         {
         TR_ASSERT((*iterator)->getLength(static_cast<int32_t>(self()->getBinaryBufferCursor()-self()->getBinaryBufferStart())) + self()->getBinaryBufferCursor() >= codeOffset,
                 "%s length estimate must be conservatively large (snippet @ " POINTER_PRINTF_FORMAT ", estimate=%d, actual=%d)",
                 self()->getDebug()->getName(*iterator), *iterator,
                 (*iterator)->getLength(static_cast<int32_t>(self()->getBinaryBufferCursor()-self()->getBinaryBufferStart())),
                 codeOffset - self()->getBinaryBufferCursor());
         self()->setBinaryBufferCursor(codeOffset);
         }
      }

   retVal = self()->getBinaryBufferCursor();

   // Emit constant data snippets last.
   //
   if (self()->hasDataSnippets())
      {
      self()->emitDataSnippets();
      }

   omrthread_jit_write_protect_enable();

   return retVal;
   }

TR::LabelSymbol *
OMR::CodeGenerator::lookUpSnippet(int32_t snippetKind, TR::SymbolReference *symRef)
   {
   for (auto iterator = _snippetList.begin(); iterator != _snippetList.end(); ++iterator)
      {
      if (self()->isSnippetMatched(*iterator, snippetKind, symRef))
         return (*iterator)->getSnippetLabel();
      }
   return NULL;
   }

TR::SymbolReference *
OMR::CodeGenerator::allocateLocalTemp(TR::DataType dt, bool isInternalPointer)
   {
   //
   TR::AutomaticSymbol * temp;
   if (isInternalPointer)
      {
      temp = TR::AutomaticSymbol::createInternalPointer(self()->trHeapMemory(),
                                                       dt,
                                                       TR::Symbol::convertTypeToSize(dt),
                                                       self()->fe());
      }
   else
      {
      temp = TR::AutomaticSymbol::create(self()->trHeapMemory(),dt,TR::Symbol::convertTypeToSize(dt));
      }
   self()->comp()->getMethodSymbol()->addAutomatic(temp);
   return new (self()->trHeapMemory()) TR::SymbolReference(self()->comp()->getSymRefTab(), temp);
   }


int32_t
OMR::CodeGenerator::getFirstBit(TR_BitVector &bv)
   {
   TR_BitVectorIterator bvi(bv);
   while (bvi.hasMoreElements())
      {
      int32_t nextElmnt = bvi.getNextElement();

      if (self()->getGlobalRegister((TR_GlobalRegisterNumber)nextElmnt)!=(uint32_t)(-1))
         {
         return nextElmnt;
         }
      }
   return -1;
   }


bool
OMR::CodeGenerator::treeContainsCall(TR::TreeTop * treeTop)
   {
   TR::Node      * node = treeTop->getNode();
   TR::ILOpCodes  l1OpCode = node->getOpCodeValue();

   // Cover instanceOf and those NEW opCodes
   if (l1OpCode == TR::treetop || l1OpCode == TR::ificmpeq || l1OpCode == TR::ificmpne)
      l1OpCode = node->getFirstChild()->getOpCodeValue();

   if (l1OpCode == TR::monent          ||
       l1OpCode == TR::monexit         ||
       l1OpCode == TR::checkcast       ||
       l1OpCode == TR::instanceof      ||
       l1OpCode == TR::ArrayStoreCHK   ||
       l1OpCode == TR::MethodEnterHook ||
       l1OpCode == TR::MethodExitHook  ||
       l1OpCode == TR::New             ||
       l1OpCode == TR::newarray        ||
       l1OpCode == TR::anewarray       ||
       l1OpCode == TR::multianewarray)
      return true;

   return(node->getNumChildren()!=0 && node->getFirstChild()->getOpCode().isCall() &&
          node->getFirstChild()->getOpCodeValue()!=TR::arraycopy);
   }

void
OMR::CodeGenerator::computeBlocksWithCalls()
   {
   uint32_t         bcount = self()->comp()->getFlowGraph()->getNextNodeNumber();
   TR_BitVector     bvec;
   TR::TreeTop      *pTree, *exitTree;
   TR::Block        *block;
   uint32_t         bnum, btemp;

   _blocksWithCalls = new (self()->trHeapMemory()) TR_BitVector(bcount, self()->trMemory());
   bvec.init(bcount, self()->trMemory());

   for (pTree=self()->comp()->getStartTree(); pTree!=NULL; pTree=exitTree->getNextTreeTop())
      {
      block = pTree->getNode()->getBlock();
      exitTree = block->getExit();
      bnum = block->getNumber();
      TR_ASSERT(bnum<bcount, "Block index must be less than the total number of blocks.\n");

      while (pTree != exitTree)
         {
         if (TR::CodeGenerator::treeContainsCall(pTree))
            {
            bvec.set(bnum);
            break;
            }
         pTree = pTree->getNextTreeTop();
         }
      if (pTree==exitTree && TR::CodeGenerator::treeContainsCall(pTree))
         {
         bvec.set(bnum);
         }
      }

   for (pTree=self()->comp()->getStartTree(); pTree!=NULL; pTree=exitTree->getNextTreeTop())
      {
      block = pTree->getNode()->getBlock();
      exitTree = block->getExit();
      bnum = block->getNumber();

      TR::Block *eblock = block->startOfExtendedBlock();
      btemp = eblock->getNumber();
      while (!bvec.isSet(btemp))
         {
         eblock = eblock->getNextBlock();
         if (eblock==NULL || !eblock->isExtensionOfPreviousBlock())
            break;
         btemp = eblock->getNumber();
         }
      if (bvec.isSet(btemp))
         _blocksWithCalls->set(bnum);
      }

   }


/**
From codegen/CodeGenerator.cpp to make the size of instructions to patch smarter.
**/

bool OMR::CodeGenerator::areMergeableGuards(TR::Instruction *earlierGuard, TR::Instruction *laterGuard)
   {
   return    earlierGuard->isMergeableGuard()
          && laterGuard->isMergeableGuard()
          && earlierGuard->getNode()->getBranchDestination()
             == laterGuard->getNode()->getBranchDestination()
          && (!earlierGuard->getNode()->isStopTheWorldGuard() || laterGuard->getNode()->isStopTheWorldGuard());
   }

TR::Instruction *OMR::CodeGenerator::getVirtualGuardForPatching(TR::Instruction *vgnop)
   {
   TR_ASSERT(vgnop->isVirtualGuardNOPInstruction(),
      "getGuardForPatching called with non VirtualGuardNOPInstruction [%p] - this only works for guards!", vgnop);

   if (!vgnop->isMergeableGuard())
      return vgnop;

   // If there are no previous instructions the instruction must be the patch point
   // as there is nothing to merge with
   if (!vgnop->getPrev())
      return vgnop;

   // Guard merging is only done when the guard trees are consecutive in treetop order
   // only separated by BBStart and BBEnd trees Skip back to the BBStart since we need
   // to get the block for vgnop
   if (vgnop->getPrev()->getNode()->getOpCodeValue() != TR::BBStart)
      return vgnop;

   // there could be local RA generated reg-reg movs between guards so we resort to checking the
   // trees and making sure that the guards are in treetop order
   // we only merge blocks in the same extended blocks - virtual guard head merger will
   // arrange for this to happen when possible for guards
   TR::Instruction *toReturn = vgnop;
   TR::Block *extendedBlockStart = vgnop->getPrev()->getNode()->getBlock()->startOfExtendedBlock();
   for (TR::Instruction *prevI = vgnop->getPrev(); prevI; prevI = prevI->getPrev())
      {
      if (prevI->isVirtualGuardNOPInstruction())
         {
         if (self()->areMergeableGuards(prevI, vgnop))
            {
            toReturn = prevI;
            }
         else
            {
            break;
            }
         }
      else
         {
         if (prevI->isMergeableGuard() &&
             prevI->getNode()->getBranchDestination() == vgnop->getNode()->getBranchDestination())
            {
            // instruction tied to an acceptable guard so do nothing and continue
            }
         else if ((prevI->getNode()->getOpCodeValue() == TR::BBStart ||
                   prevI->getNode()->getOpCodeValue() == TR::BBEnd) &&
                  prevI->getNode()->getBlock()->startOfExtendedBlock() == extendedBlockStart)
            {
            // we are in the same extension and haven't found any problematic instructions yet
            // so keep scanning
            }
         else
            {
            break;
            }
         }
      }
   if (toReturn != vgnop)
      {
      TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "guardMerge/(%s)", self()->comp()->signature()));
      if (self()->comp()->getOption(TR_TraceCG))
         traceMsg(self()->comp(), "vgnop instruction [%p] begins scanning for patch instructions for mergeable guard [%p]\n", vgnop, toReturn);
      }
   return toReturn;
   }

TR::Instruction
*OMR::CodeGenerator::getInstructionToBePatched(TR::Instruction *vgnop)
   {
   TR::Instruction   * nextI;
   TR::Node          *firstBBEnd = NULL;

   for (nextI=self()->getVirtualGuardForPatching(vgnop)->getNext(); nextI!=NULL; nextI=nextI->getNext())
      {
      if (nextI->isVirtualGuardNOPInstruction())
         {
         if (!self()->areMergeableGuards(vgnop, nextI))
            return NULL;
         continue;
         }

      if (nextI->isPatchBarrier(self()) || self()->comp()->isPICSite(nextI))
         return NULL;

      if (nextI->getEstimatedBinaryLength() > 0)
         return nextI;

      TR::Node * node = nextI->getNode();
      if (node == NULL)
         return NULL;
      if (node->getOpCodeValue()==TR::BBEnd)
         {
         if (firstBBEnd == NULL)
            firstBBEnd = node;
         else if (firstBBEnd!=node &&
                  (node->getBlock()->getNextBlock()==NULL ||
                   !node->getBlock()->getNextBlock()->isExtensionOfPreviousBlock()))
            return NULL;
         }
      if (node->getOpCodeValue()==TR::BBStart && firstBBEnd!=NULL && !node->getBlock()->isExtensionOfPreviousBlock())
         {
         return NULL;
         }
      }
   return NULL;
   }


int32_t
OMR::CodeGenerator::sizeOfInstructionToBePatched(TR::Instruction *vgnop)
   {
   TR::Instruction *instToBePatched = self()->getInstructionToBePatched(vgnop);
   if (instToBePatched)
      return instToBePatched->getBinaryLengthLowerBound();
   else
        return 0;
   }

int32_t
OMR::CodeGenerator::sizeOfInstructionToBePatchedHCRGuard(TR::Instruction *vgnop)
   {
   TR::Instruction   *nextI;
   TR::Node          *firstBBEnd = NULL;
   int32_t             accumulatedSize = 0;

   for (nextI=self()->getInstructionToBePatched(vgnop); nextI!=NULL; nextI=nextI->getNext())
      {
      if (nextI->isVirtualGuardNOPInstruction())
         {
         if (!self()->areMergeableGuards(vgnop, nextI))
            break;
         continue;
         }

      if (nextI->isPatchBarrier(self()) || self()->comp()->isPICSite(nextI))
         break;

      accumulatedSize += nextI->getBinaryLengthLowerBound();

      if (accumulatedSize > nextI->getMaxPatchableInstructionLength())
         break;

      TR::Node * node = nextI->getNode();
      if (node == NULL)
         break;

      if (node->getOpCodeValue()==TR::BBEnd)
         {
         if (firstBBEnd == NULL)
            firstBBEnd = node;
         else if (firstBBEnd!=node &&
                 (node->getBlock()->getNextBlock()==NULL ||
                  !node->getBlock()->getNextBlock()->isExtensionOfPreviousBlock()))
            break;
         }

      if (node->getOpCodeValue()==TR::BBStart && firstBBEnd!=NULL && !node->getBlock()->isExtensionOfPreviousBlock())
         {
         break;
         }
      }

   return accumulatedSize;
   }

#ifdef DEBUG
void
OMR::CodeGenerator::shutdown(TR_FrontEnd *fe, TR::FILE *logFile)
   {
   }
#endif


#if !(defined(TR_HOST_POWER) && (defined(__IBMC__) || defined(__IBMCPP__) || defined(__ibmxl__)))

int32_t leadingZeroes (int32_t inputWord)
   {
   uint32_t byteMask, bitCount;

   byteMask = 0xFFul << 24;  // a byte mask high order justified

   // find first non-zero byte
   for (bitCount = 0; bitCount < 32; bitCount += 8)
      {
      uint8_t byteValue;
      uint32_t testWord;

      testWord = inputWord & byteMask;
      if (testWord != 0)
         {
         byteValue = testWord >> (24 - bitCount);
         return bitCount + CS2::BitManipulator::LeadingZeroes(byteValue);
         }
      byteMask >>= 8;
      }

   return 32;
   }

int32_t leadingZeroes (int64_t inputWord)
   {
   uint64_t byteMask;
   uint32_t bitCount;

   byteMask = (uint64_t)0xFF << 56;  // a byte mask high order justified

   // find first non-zero byte
   for (bitCount = 0; bitCount < 64; bitCount += 8)
      {
      uint8_t  byteValue;
      uint64_t testWord;

      testWord = inputWord & byteMask;
      if (testWord != 0)
         {
         byteValue = static_cast<uint8_t>(testWord >> (56 - bitCount));
         return bitCount + CS2::BitManipulator::LeadingZeroes(byteValue);
         }
      byteMask >>= 8;
      }

   return 64;
   }
#endif



// determine if this node, a constant load, will be loaded into a register
// Various code generators can override for their specific instruction set
bool
OMR::CodeGenerator::isMaterialized(TR::Node * node)
   {
   if(!node->getOpCode().isLoadConst()) return false;

   if (node->getOpCode().isFloat() || node->getOpCode().isDouble())
      {
      // assume by default that instruction set
      // doesn't support fp/dbl constants in instructions,
      // and always cheaper to keep in register than load
      // from contant pool
      return true;
      }

   int64_t value;
   if (node->getOpCodeValue() == TR::iconst)
      value = (int64_t) node->getInt();
   else if (node->getOpCodeValue() == TR::lconst)
      value = node->getLongInt();
   else
      return false;

   return self()->shouldValueBeInACommonedNode(value);
   }

bool
OMR::CodeGenerator::canNullChkBeImplicit(TR::Node *node)
   {
   return self()->canNullChkBeImplicit(node, false);
   }

bool
OMR::CodeGenerator::canNullChkBeImplicit(TR::Node *node, bool doChecks)
   {
   if (self()->comp()->getOption(TR_DisableTraps))
      return false;

   if (!doChecks)
      return true;

   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCode &opCode = firstChild->getOpCode();

   if (opCode.isLoadVar() || (self()->comp()->target().is64Bit() && opCode.getOpCodeValue() == TR::l2i))
      {
      TR::SymbolReference *symRef = NULL;

      if (opCode.getOpCodeValue() == TR::l2i)
         symRef = firstChild->getFirstChild()->getSymbolReference();
      else
         symRef = firstChild->getSymbolReference();

      if (symRef &&
            symRef->getSymbol()->getOffset() + symRef->getOffset() < self()->getNumberBytesReadInaccessible())
         return true;
      }
   else if (opCode.isStore())
      {
      TR::SymbolReference *symRef = firstChild->getSymbolReference();
      if (symRef &&
            symRef->getSymbol()->getOffset() + symRef->getOffset() < self()->getNumberBytesWriteInaccessible())
         return true;
      }
   else if (opCode.isCall() &&
               opCode.isIndirect() &&
               self()->getNumberBytesReadInaccessible() > TR::Compiler->om.offsetOfObjectVftField())
      {
      return true;
      }
   else if (opCode.getOpCodeValue() == TR::iushr &&
            self()->getNumberBytesReadInaccessible() > self()->fe()->getOffsetOfContiguousArraySizeField())
      {
      return true;
      }
   return false;
   }

bool OMR::CodeGenerator::isILOpCodeSupported(TR::ILOpCodes o)
   {
	return (_nodeToInstrEvaluators[o] != TR::TreeEvaluator::unImpOpEvaluator) &&
	      (_nodeToInstrEvaluators[o] != TR::TreeEvaluator::badILOpEvaluator);
   }

void OMR::CodeGenerator::freeUnlatchedRegisters()
   {
   for (int i = 0; _unlatchedRegisterList[i]; i++)
      {
      // a register on the unlatched list may have changed state since
      // it was added to the list ==> need to check state again
      if (_unlatchedRegisterList[i]->getState() == TR::RealRegister::Unlatched)
         {
         _unlatchedRegisterList[i]->setState(TR::RealRegister::Free);
         _unlatchedRegisterList[i]->setAssignedRegister(NULL);
         }
      }
   _unlatchedRegisterList[0] = 0; // mark that the list is empty now
   }

TR::Instruction *
OMR::CodeGenerator::generateDebugCounter(const char *name, int32_t delta, int8_t fidelity)
   {
   return self()->generateDebugCounter(NULL, name, delta, fidelity);
   }

TR::Instruction *
OMR::CodeGenerator::generateDebugCounter(const char *name, TR::Register *deltaReg, int8_t fidelity)
   {
   return self()->generateDebugCounter(NULL, name, deltaReg, fidelity);
   }

TR::Instruction *OMR::CodeGenerator::generateDebugCounter(TR::Instruction *cursor, const char *name, int32_t delta, int8_t fidelity, int32_t staticDelta)
   {
   if (!cursor)
      cursor = self()->getAppendInstruction();
   if (!self()->comp()->getOptions()->enableDebugCounters())
      return cursor;
   if (delta == 0)
      return cursor;
   TR::DebugCounterAggregation *aggregatedCounters = self()->comp()->getPersistentInfo()->getDynamicCounters()->createAggregation(self()->comp(), name);
   aggregatedCounters->aggregateStandardCounters(self()->comp(), cursor->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
      return cursor;

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()) && !aggregatedCounters->initializeReloData(self()->comp(), delta, fidelity, staticDelta))
      return cursor;

   TR::SymbolReference *symref = aggregatedCounters->getBumpCountSymRef(self()->comp());

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()))
      self()->comp()->mapStaticAddressToCounter(symref, aggregatedCounters);

   return self()->generateDebugCounterBump(cursor, aggregatedCounters, 1, NULL);
   }

TR::Instruction *OMR::CodeGenerator::generateDebugCounter(TR::Instruction *cursor, const char *name, TR::Register *deltaReg, int8_t fidelity, int32_t staticDelta)
   {
   if (!cursor)
      cursor = self()->getAppendInstruction();
   if (!self()->comp()->getOptions()->enableDebugCounters())
      return cursor;
   // We won't do any aggregation (histogram buckets, bytecode breakdown, etc.) if we're getting the delta from a register
   TR::DebugCounter *counter = TR::DebugCounter::getDebugCounter(self()->comp(), name, fidelity, staticDelta);
   if (!counter)
      return cursor;

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()) && !counter->initializeReloData(self()->comp(), 0, fidelity, staticDelta))
      return cursor;

   TR::SymbolReference *symref = counter->getBumpCountSymRef(self()->comp());

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()))
      self()->comp()->mapStaticAddressToCounter(symref, counter);

   return self()->generateDebugCounterBump(cursor, counter, deltaReg, NULL);
   }

TR::Instruction *OMR::CodeGenerator::generateDebugCounter(const char *name, TR::RegisterDependencyConditions &cond, int32_t delta, int8_t fidelity, int32_t staticDelta, TR::Instruction *cursor)
   {
   if (!cursor)
      cursor = self()->getAppendInstruction();
   if (!self()->comp()->getOptions()->enableDebugCounters())
      return cursor;
   if (delta == 0)
      return cursor;
   TR::DebugCounterAggregation *aggregatedCounters = self()->comp()->getPersistentInfo()->getDynamicCounters()->createAggregation(self()->comp(), name);
   aggregatedCounters->aggregateStandardCounters(self()->comp(), cursor->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
      return cursor;

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()) && !aggregatedCounters->initializeReloData(self()->comp(), delta, fidelity, staticDelta))
      return cursor;

   TR::SymbolReference *symref = aggregatedCounters->getBumpCountSymRef(self()->comp());

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()))
      self()->comp()->mapStaticAddressToCounter(symref, aggregatedCounters);

   return self()->generateDebugCounterBump(cursor, aggregatedCounters, 1, &cond);
   }

TR::Instruction *OMR::CodeGenerator::generateDebugCounter(const char *name, TR::Register *deltaReg, TR::RegisterDependencyConditions &cond, int8_t fidelity, int32_t staticDelta, TR::Instruction *cursor)
   {
   if (!cursor)
      cursor = self()->getAppendInstruction();
   if (!self()->comp()->getOptions()->enableDebugCounters())
      return cursor;
   // We won't do any aggregation (histogram buckets, bytecode breakdown, etc.) if we're getting the delta from a register
   TR::DebugCounter *counter = TR::DebugCounter::getDebugCounter(self()->comp(), name, fidelity, staticDelta);
   if (!counter)
      return cursor;

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()) && !counter->initializeReloData(self()->comp(), 0, fidelity, staticDelta))
      return cursor;

   TR::SymbolReference *symref = counter->getBumpCountSymRef(self()->comp());

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()))
      self()->comp()->mapStaticAddressToCounter(symref, counter);

   return self()->generateDebugCounterBump(cursor, counter, deltaReg, &cond);
   }

TR::Instruction *OMR::CodeGenerator::generateDebugCounter(const char *name, TR_ScratchRegisterManager &srm, int32_t delta, int8_t fidelity, int32_t staticDelta, TR::Instruction *cursor)
   {
   if (!cursor)
      cursor = self()->getAppendInstruction();
   if (!self()->comp()->getOptions()->enableDebugCounters())
      return cursor;
   if (delta == 0)
      return cursor;
   TR::DebugCounterAggregation *aggregatedCounters = self()->comp()->getPersistentInfo()->getDynamicCounters()->createAggregation(self()->comp(), name);
   aggregatedCounters->aggregateStandardCounters(self()->comp(), cursor->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
      return cursor;

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()) && !aggregatedCounters->initializeReloData(self()->comp(), delta, fidelity, staticDelta))
      return cursor;

   TR::SymbolReference *symref = aggregatedCounters->getBumpCountSymRef(self()->comp());

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()))
      self()->comp()->mapStaticAddressToCounter(symref, aggregatedCounters);

   return self()->generateDebugCounterBump(cursor, aggregatedCounters, 1, srm);
   }

TR::Instruction *OMR::CodeGenerator::generateDebugCounter(const char *name, TR::Register *deltaReg, TR_ScratchRegisterManager &srm, int8_t fidelity, int32_t staticDelta, TR::Instruction *cursor)
   {
   if (!cursor)
      cursor = self()->getAppendInstruction();
   if (!self()->comp()->getOptions()->enableDebugCounters())
      return cursor;
   // We won't do any aggregation (histogram buckets, bytecode breakdown, etc.) if we're getting the delta from a register
   TR::DebugCounter *counter = TR::DebugCounter::getDebugCounter(self()->comp(), name, fidelity, staticDelta);
   if (!counter)
      return cursor;

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()) && !counter->initializeReloData(self()->comp(), 0, fidelity, staticDelta))
      return cursor;

   TR::SymbolReference *symref = counter->getBumpCountSymRef(self()->comp());

   if (TR::DebugCounter::relocatableDebugCounter(self()->comp()))
      self()->comp()->mapStaticAddressToCounter(symref, counter);

   return self()->generateDebugCounterBump(cursor, counter, deltaReg, srm);
   }

// Records the use of a single virtual register.
//
void OMR::CodeGenerator::recordSingleRegisterUse(TR::Register *reg)
   {
   for (auto iter = self()->getReferencedRegisterList()->begin(), end = self()->getReferencedRegisterList()->end(); iter != end; ++iter)
      {
      if ((*iter)->virtReg == reg)
         {
         (*iter)->useCount++;
         return;
         }
      }

   OMR::RegisterUsage *ru = new (self()->trHeapMemory()) OMR::RegisterUsage(reg, 1);
   self()->getReferencedRegisterList()->push_front(ru);
   }

void OMR::CodeGenerator::startRecordingRegisterUsage()
   {
   self()->setReferencedRegisterList(new (self()->trHeapMemory()) TR::list<OMR::RegisterUsage*>(getTypedAllocator<OMR::RegisterUsage*>(self()->comp()->allocator())));
   self()->setEnableRegisterUsageTracking();
   }

TR::list<OMR::RegisterUsage*> *OMR::CodeGenerator::stopRecordingRegisterUsage()
   {
   self()->resetEnableRegisterUsageTracking();
   return self()->getReferencedRegisterList();
   }


void OMR::CodeGenerator::addRelocation(TR::Relocation *r)
   {
   if (r->isExternalRelocation())
      {
      TR_ASSERT(false, "Cannot use addRelocation to add an AOT relocation. Please use addExternalRelocation here");
      }
   else
      {
      _relocationList.push_front(r);
      }
   }

void OMR::CodeGenerator::addExternalRelocation(TR::Relocation *r, const char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node, TR::ExternalRelocationPositionRequest where)
   {
   TR_ASSERT(generatingFileName, "External relocation location has improper NULL filename specified");
   if (self()->comp()->compileRelocatableCode())
      {
      TR::RelocationDebugInfo *genData = new(self()->trHeapMemory()) TR::RelocationDebugInfo;
      genData->file = generatingFileName;
      genData->line = generatingLineNumber;
      genData->node = node;
      self()->addExternalRelocation(r, genData, where);
      }
   }

void OMR::CodeGenerator::addExternalRelocation(TR::Relocation *r, TR::RelocationDebugInfo* info, TR::ExternalRelocationPositionRequest where)
   {
   if (self()->comp()->compileRelocatableCode())
      {
      TR_ASSERT(info, "External relocation location does not have associated debug information");
      r->setDebugInfo(info);
      switch (where)
         {
         case TR::ExternalRelocationAtFront:
            _externalRelocationList.push_front(r);
            break;

         case TR::ExternalRelocationAtBack:
            _externalRelocationList.push_back(r);
            break;

         default:
            TR_ASSERT_FATAL(
               false,
               "invalid TR::ExternalRelocationPositionRequest %d",
               where);
            break;
         }
      }
   }

void
OMR::CodeGenerator::addStaticRelocation(const TR::StaticRelocation &relocation)
   {
   _staticRelocationList.push_back(relocation);
   }

intptr_t OMR::CodeGenerator::hiValue(intptr_t address)
   {
   if (self()->comp()->compileRelocatableCode()) // We don't want to store values using HI_VALUE at compile time, otherwise, we do this a 2nd time when we relocate (and new value is based on old one)
      return address >> 16;
   else
      return HI_VALUE(address);
   }


void OMR::CodeGenerator::addToOSRTable(TR::Instruction *instr)
   {
   self()->comp()->getOSRCompilationData()->addInstruction(instr);
   }

void OMR::CodeGenerator::addToOSRTable(int32_t instructionPC, TR_ByteCodeInfo &bcInfo)
   {
   self()->comp()->getOSRCompilationData()->addInstruction(instructionPC, bcInfo);
   }






void OMR::CodeGenerator::addAllocatedRegisterPair(TR::RegisterPair * temp)
   {
   uint32_t idx = _registerArray.add(temp);
   temp->setIndex(idx);
   if (temp->getLowOrder()->getKind() == temp->getHighOrder()->getKind())
      {
      if (_liveRegisters[temp->getKind()])
         _liveRegisters[temp->getKind()]->addRegisterPair(temp);
      }
   else // for AR:GPR pairs and beyond
        {
      if (_liveRegisters[temp->getKind()])
         {
         _liveRegisters[temp->getKind()]->addRegister(temp);
         // Don't count the register pair entry in the number of live registers,
         // since it won't contribute to register pressure
         _liveRegisters[temp->getKind()]->decNumberOfLiveRegisters();
         }
      if (!temp->getLowOrder()->isLive())
         _liveRegisters[temp->getLowOrder()->getKind()]->addRegister(temp->getLowOrder());
      if (!temp->getHighOrder()->isLive())
         _liveRegisters[temp->getHighOrder()->getKind()]->addRegister(temp->getHighOrder());

        }
   }

void OMR::CodeGenerator::addAllocatedRegister(TR::Register * temp)
   {
   uint32_t idx = _registerArray.add(temp);
   temp->setIndex(idx);
   self()->startUsingRegister(temp);
   }

TR::RegisterPair * OMR::CodeGenerator::allocateRegisterPair(TR::Register * lo, TR::Register * ho)
   {
   TR::RegisterPair *temp =  new (self()->trHeapMemory()) TR::RegisterPair(lo, ho);
   self()->addAllocatedRegisterPair(temp);
   return temp;
   }

TR::RegisterPair * OMR::CodeGenerator::allocateSinglePrecisionRegisterPair(TR::Register * lo, TR::Register * ho)
   {
   TR::RegisterPair *temp = new (self()->trHeapMemory()) TR::RegisterPair(TR_FPR);
   temp->setLowOrder(lo, self());
   temp->setHighOrder(ho, self());
   self()->addAllocatedRegisterPair(temp);
   return temp;
   }

TR::RegisterPair * OMR::CodeGenerator::allocateFloatingPointRegisterPair(TR::Register * lo, TR::Register * ho)
   {
   TR::RegisterPair * temp = new (self()->trHeapMemory()) TR::RegisterPair(lo, ho);
   temp->setKind(TR_FPR);
   self()->addAllocatedRegisterPair(temp);
   return temp;
   }

// Z
bool OMR::CodeGenerator::AddArtificiallyInflatedNodeToStack(TR::Node * n)
   {
   for (uint32_t i = 0; i < _stackOfArtificiallyInflatedNodes.size(); i++)
      {
      TR::Node * existingNode = _stackOfArtificiallyInflatedNodes[i];

      if (existingNode == n)
         {
         return false;
         }
      }
   _stackOfArtificiallyInflatedNodes.push(n);
   return true;
   }

bool
OMR::CodeGenerator::constantAddressesCanChangeSize(TR::Node *node)
   {
   if (!self()->comp()->compileRelocatableCode() || self()->comp()->target().is32Bit() || node==NULL)
      return false;

   if (node->getOpCodeValue() == TR::aconst && (node->isClassPointerConstant() || node->isMethodPointerConstant()))
      return true;

   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *symbol = symRef->getSymbol();
      if (symbol)
         {
         if (node->getOpCodeValue() == TR::loadaddr && node->getSymbolReference()->getSymbol()->isClassObject())
            return true;
         }
      }

   return false;
   }


// Z
bool
OMR::CodeGenerator::isInMemoryInstructionCandidate(TR::Node * node)
   {
   // can we use an in-memory only instruction to evaluate
   //  the value child in a given tree?
   //
   // Examples:
   //
   // ibstore
   //   addr
   //   bor/band/bxor
   //     ibload
   //       =>addr
   //     bconst
   //
   // On Z systems, this can use an OI instruction, which is an SI form instruction.

   if (node->getOpCode().isStore() && node->getOpCode().isIndirect())
      {
      }
   else
      {
      return false;
      }

   TR::Node * valueChild = node->getChild(1);

   // callers of this function should make sure that the opcode on valueChild corresponds
   // to a valid in-memory instruction on the target instruction set

   if (valueChild->getNumChildren() == 0)
      {
      return false;
      }

   // whatever is under the valueChild has to be indirect too, and its address must match
   // the store's address

   if (valueChild->getFirstChild()->getOpCode().isLoadVar() &&
       valueChild->getFirstChild()->getOpCode().isIndirect() &&
       self()->addressesMatch(node->getChild(0), valueChild->getFirstChild()->getFirstChild(), true))
      {
      }
   else
      {
      return false;
      }

   // in-memory instruction use only applies if ref count == 1 and the value exists
   // only in memory (not loaded into reg). this restriction applies to:
   //
   // 1) address under store
   // 2) valueChild
   // 3) address under valueChild

   if (node->getFirstChild()->getReferenceCount() == 1 &&
       node->getFirstChild()->getRegister() == NULL &&
       valueChild->getReferenceCount() == 1 &&
       valueChild->getRegister() == NULL &&
       valueChild->getFirstChild()->getReferenceCount() == 1 &&
       valueChild->getFirstChild()->getRegister() == NULL)
      {
      }
   else
      {
      return false;
      }

   return true;
   }

TR::Linkage *
OMR::CodeGenerator::createLinkageForCompilation()
   {
   return self()->getLinkage(TR_System);
   }


TR::TreeTop *
OMR::CodeGenerator::lowerTree(TR::Node *root, TR::TreeTop *tt)
   {
   TR_ASSERT(0, "Unimplemented lowerTree() called for an opcode that needs to be lowered");
   return NULL;
   }

/*
 * insertPrefetchIfNecessary
 *
 * This method can be used to emit a prefetch instruction. By default it does nothing. It can be overwritten
 * to set the conditions for the generation of the prefetch instruction.
 */
void
OMR::CodeGenerator::insertPrefetchIfNecessary(TR::Node *node, TR::Register *targetRegister)
   {
   return;
   }


void
OMR::CodeGenerator::switchCodeCacheTo(TR::CodeCache *newCodeCache)
   {
   TR::CodeCache *oldCodeCache = self()->getCodeCache();

   TR_ASSERT(oldCodeCache != newCodeCache, "Attempting to switch to the currently held code cache");

   self()->setCodeCache(newCodeCache);
   self()->setCodeCacheSwitched(true);

   if (self()->committedToCodeCache() || !newCodeCache)
      {
      TR::Compilation *comp = self()->comp();

      if (newCodeCache)
         {
         comp->failCompilation<TR::RecoverableCodeCacheError>("Already committed to current code cache");
         }

      comp->failCompilation<TR::CodeCacheError>("Already committed to current code cache");
      }

   // If the old CodeCache had pre-loaded code, the current compilation may have
   // initialized it and will therefore depend on it.  The new CodeCache must be
   // initialized as well.
   //
   if (oldCodeCache->isCCPreLoadedCodeInitialized())
      {
      newCodeCache->getCCPreLoadedCodeAddress(TR_numCCPreLoadedCode, self());
      }

   }


void
OMR::CodeGenerator::redoTrampolineReservationIfNecessary(TR::Instruction *callInstr, TR::SymbolReference *instructionSymRef)
   {
   TR_ASSERT_FATAL(instructionSymRef, "Expecting instruction to have a SymbolReference");

   /**
    * Determine the callee symbol reference based on the target of the call instruction
    */
   TR::LabelSymbol *labelSymbol = instructionSymRef->getSymbol()->getLabelSymbol();
   TR::SymbolReference *calleeSymRef = NULL;

   if (labelSymbol == NULL)
      {
      calleeSymRef = instructionSymRef;
      }
   else if (callInstr->getNode() != NULL)
      {
      calleeSymRef = callInstr->getNode()->getSymbolReference();
      }

   TR_ASSERT_FATAL(calleeSymRef != NULL, "Missing possible re-reservation for trampolines");

   if (calleeSymRef->getReferenceNumber() >= TR_numRuntimeHelpers)
      {
      self()->fe()->reserveTrampolineIfNecessary(self()->comp(), calleeSymRef, true);
      }
   }

uint32_t
OMR::CodeGenerator::getCodeSnippetsSize()
   {
   uint32_t codeSnippetsSize = 0;

   TR::list<TR::Snippet*> snippetList = self()->getSnippetList();

   for (auto snippets = snippetList.begin(); snippets != snippetList.end(); ++snippets)
      codeSnippetsSize += (*snippets)->getLength(0);

   return codeSnippetsSize;
   }

void
OMR::CodeGenerator::getMethodStats(MethodStats &methodStats)
   {
   auto &codeSize = methodStats.codeSize;
   auto &warmBlocks = methodStats.warmBlocks;
   auto &coldBlocks = methodStats.coldBlocks;
   auto &prologue = methodStats.prologue;
   auto &snippets = methodStats.snippets;
   auto &outOfLine = methodStats.outOfLine;
   auto &unaccounted = methodStats.unaccounted;
   auto &blocksInColdCache = methodStats.blocksInColdCache;
   auto &overestimateInColdCache = methodStats.overestimateInColdCache;

   // init
   codeSize = 0;
   warmBlocks = 0;
   coldBlocks = 0;
   prologue = 0;
   snippets = 0;
   outOfLine = 0;
   unaccounted = 0;
   blocksInColdCache = 0;
   overestimateInColdCache = self()->getEstimatedColdLength() - self()->getColdCodeLength();

   uint32_t allBlocks = 0;
   uint32_t sizeBeforeFirstBlock = 0;
   bool firstBlock = true;
   bool insideColdCache = false;
   uint32_t cold_frequence_size[NUMBER_BLOCK_FREQUENCIES] = {0};

   codeSize = static_cast<uint32_t>(self()->getCodeEnd() - self()->getCodeStart());

   if (self()->getLastWarmInstruction())
      codeSize = codeSize - static_cast<uint32_t>(self()->getColdCodeStart() - self()->getWarmCodeEnd());

   for (TR::TreeTop *tt = self()->comp()->getMethodSymbol()->getFirstTreeTop(); tt ; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();

      TR::Block *block;
      uint8_t *startCursor, *endCursor;

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         startCursor = block->getFirstInstruction()->getBinaryEncoding();
         endCursor = block->getLastInstruction()->getBinaryEncoding();

         uint32_t blockSize = static_cast<uint32_t>(endCursor - startCursor);
         allBlocks += blockSize;

         if (block->isCold())
            {
            coldBlocks += blockSize;
            int32_t freq = block->getFrequency();

            if (freq >= 0 && freq < NUMBER_BLOCK_FREQUENCIES)
               cold_frequence_size[freq] += blockSize;
            }

         if (insideColdCache)
            blocksInColdCache += blockSize;

         if (firstBlock)
            {
            sizeBeforeFirstBlock = (uint32_t)(startCursor - self()->getCodeStart());
            firstBlock = false;
            }

         if (block->isLastWarmBlock())
            insideColdCache = true;
         }
      }

   warmBlocks = allBlocks - coldBlocks;
   snippets = self()->getCodeSnippetsSize() + self()->getDataSnippetsSize();
   outOfLine = self()->getOutOfLineCodeSize();
   unaccounted = codeSize - allBlocks - sizeBeforeFirstBlock - outOfLine - snippets;
   prologue = sizeBeforeFirstBlock;

   if (self()->comp()->getOption(TR_TraceCG))
      {
      uint32_t known_cold_blocks = 0;
      for (int i = 0; i < NUMBER_BLOCK_FREQUENCIES; i++)
         {
         traceMsg(self()->comp(), "FOOTPRINT: COLD BLOCK TYPE: %s = %d\n", OMR::CFG::blockFrequencyNames[i],
                                  cold_frequence_size[i]);

         known_cold_blocks += cold_frequence_size[i];
         }
      traceMsg(self()->comp(), "FOOTPRINT: COLD BLOCK TYPE: OTHER = %d\n", coldBlocks - known_cold_blocks);
      }
   }
