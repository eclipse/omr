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

#pragma csect(CODE,"TRCGBase#C")
#pragma csect(STATIC,"TRCGBase#S")
#pragma csect(TEST,"TRCGBase#T")

#include "codegen/OMRCodeGenerator.hpp"

#include <limits.h>                                 // for UINT_MAX
#include <stdarg.h>                                 // for va_list
#include <stddef.h>                                 // for NULL, size_t
#include <stdint.h>                                 // for int64_t, etc
#include <stdio.h>                                  // for sprintf
#include <stdlib.h>                                 // for abs, atoi
#include <string.h>                                 // for memset, memcpy
#include <algorithm>                                // for std::find
#include "codegen/CodeGenPhase.hpp"                 // for CodeGenPhase
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd, etc
#include "codegen/Instruction.hpp"                  // for Instruction
#include "codegen/Linkage.hpp"                      // for Linkage
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"                      // for Machine
#include "codegen/RealRegister.hpp"                 // for RealRegister
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/RegisterUsage.hpp"                // for RegisterUsage
#include "codegen/Relocation.hpp"                   // for TR::Relocation, etc
#include "codegen/Snippet.hpp"                      // for Snippet
#include "codegen/StorageInfo.hpp"                  // for TR_StorageInfo, etc
#include "codegen/TreeEvaluator.hpp"                // for TreeEvaluator
#include "codegen/GCStackMap.hpp"                   // for GCStackMap
#include "codegen/GCStackAtlas.hpp"                   // for GCStackMap
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/OSRData.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/allocator.h"
#include "cs2/bitmanip.h"                           // for LeadingZeroes
#include "cs2/hashtab.h"                            // for HashTable, etc
#include "cs2/sparsrbit.h"                          // for operator<<
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                               // for IO
#include "env/PersistentInfo.hpp"                   // for PersistentInfo
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                         // for Allocator, etc
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for DataTypes, etc
#include "il/ILOpCodes.hpp"                         // for ILOpCodes, etc
#include "il/ILOps.hpp"                             // for ILOpCode, etc
#include "il/Node.hpp"                              // for Node, vcount_t
#include "il/NodePool.hpp"                          // for TR::NodePool
#include "il/Node_inlines.hpp"                      // for Node::getType, etc
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"                          // for TR_Array
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/Bit.hpp"                            // for isEven, etc
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/Flags.hpp"                          // for flags32_t, etc
#include "infra/HashTab.hpp"                        // for TR_HashTab, etc
#include "infra/Link.hpp"                           // for TR_LinkHead, etc
#include "infra/List.hpp"                           // for ListIterator, etc
#include "infra/Stack.hpp"                          // for TR_Stack
#include "infra/Checklist.hpp"                      // for TR::NodeCheckList
#include "infra/CfgEdge.hpp"                        // for CFGEdge
#include "infra/CfgNode.hpp"                        // for CFGNode
#include "optimizer/Optimizations.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "ras/DebugCounter.hpp"
#include "ras/Delimiter.hpp"                        // for Delimiter
#include "runtime/Runtime.hpp"                      // for HI_VALUE, etc
#include "runtime/CodeCacheExceptions.hpp"
#include "stdarg.h"                                 // for va_end, etc

namespace TR { class Optimizer; }
namespace TR { class RegisterDependencyConditions; }


#if defined(TR_TARGET_X86) || defined(TR_TARGET_POWER)
   #define butestEvaluator badILOpEvaluator
   #define sutestEvaluator badILOpEvaluator
   #define iucmpEvaluator badILOpEvaluator
   #define icmpEvaluator badILOpEvaluator
#endif
#ifdef TR_TARGET_X86  //x86 only
    #define bucmpEvaluator badILOpEvaluator
    #define bcmpEvaluator badILOpEvaluator
    #define sucmpEvaluator badILOpEvaluator
    #define scmpEvaluator badILOpEvaluator
#endif
#if defined(TR_TARGET_AMD64) || defined(TR_TARGET_POWER) //ppc and amd64
    #define zccAddSubEvaluator badILOpEvaluator
#endif

TR_TreeEvaluatorFunctionPointer
OMR::CodeGenerator::_nodeToInstrEvaluators[] =
   {
   #include "codegen/TreeEvaluatorTable.hpp"
   };


static_assert(TR::NumIlOps ==
              (sizeof(OMR::CodeGenerator::_nodeToInstrEvaluators) / sizeof(OMR::CodeGenerator::_nodeToInstrEvaluators[0])),
              "NodeToInstrEvaluators is not the correct size");

#define OPT_DETAILS "O^O CODE GENERATION: "

TR::Node* generatePoisonNode(TR::Compilation *comp, TR::Block *currentBlock, TR::SymbolReference *liveAutoSymRef)
   {

   bool poisoned = true;
   TR::Node *storeNode = NULL;

   if (liveAutoSymRef->getSymbol()->getType().isAddress())
       storeNode = TR::Node::createStore(liveAutoSymRef, TR::Node::aconst(currentBlock->getEntry()->getNode(), 0x0));
   else if (liveAutoSymRef->getSymbol()->getType().isInt64())
       storeNode = TR::Node::createStore(liveAutoSymRef, TR::Node::lconst(currentBlock->getEntry()->getNode(), 0xc1aed1e5));
   else if (liveAutoSymRef->getSymbol()->getType().isInt32())
       storeNode = TR::Node::createStore(liveAutoSymRef, TR::Node::iconst(currentBlock->getEntry()->getNode(), 0xc1aed1e5));
   else
         poisoned = false;

   if (comp->getOption(TR_TraceCG) && comp->getOption(TR_PoisonDeadSlots))
      {
      if (poisoned)
         traceMsg(comp, "POISON DEAD SLOTS --- Live local %d  from parent block %d going dead .... poisoning slot with node 0x%x .\n", liveAutoSymRef->getReferenceNumber() , currentBlock->getNumber(), storeNode);
      else
         traceMsg(comp, "POISON DEAD SLOTS --- Live local %d of unsupported type from parent block %d going dead .... poisoning skipped.\n", liveAutoSymRef->getReferenceNumber() , currentBlock->getNumber());
      }

   return storeNode;
   }

TR::Instruction *
OMR::CodeGenerator::generateNop(TR::Node * node, TR::Instruction *instruction, TR_NOPKind nopKind)
    { TR_ASSERT(0, "shouldn't get here"); return NULL;}

OMR::CodeGenerator::CodeGenerator() :
      _compilation(TR::comp()),
      _trMemory(_compilation->trMemory()),
      _liveLocals(0),
      _currentEvaluationTreeTop(0),
      _currentEvaluationBlock(0),
      _prePrologueSize(0),
      _implicitExceptionPoint(0),
      _localsThatAreStored(NULL),
      _numLocalsWhenStoreAnalysisWasDone(-1),
      _uncommmonedNodes(self()->comp()->trMemory(), stackAlloc),
      _ialoadUnneeded(self()->comp()->trMemory()),
     _symRefTab(self()->comp()->getSymRefTab()),
     _vmThreadRegister(NULL),
     _vmThreadSpillInstr(NULL),
     _stackAtlas(NULL),
     _methodStackMap(NULL),
     _binaryBufferStart(NULL),
     _binaryBufferCursor(NULL),
     _largestOutgoingArgSize(0),
     _estimatedCodeLength(0),
     _estimatedSnippetStart(0),
     _accumulatedInstructionLengthError(0),
     _registerSaveDescription(0),
     _extendedToInt64GlobalRegisters(self()->comp()->allocator()),
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
     _firstGlobalAR(0),
     _lastGlobalAR(0),
     _last8BitGlobalGPR(0),
     _globalGPRPartitionLimit(0),
     _globalFPRPartitionLimit(0),
     _firstInstruction(NULL),
     _appendInstruction(NULL),
     _firstGlobalVRF(-1),
     _lastGlobalVRF(-1),
     _firstOverlappedGlobalVRF(-1),
     _lastOverlappedGlobalVRF(-1),
     _overlapOffsetBetweenFPRandVRFgrns(0),
     _supportedLiveRegisterKinds(0),
     _monitorMapping(self()->comp()->trMemory(), 256),
     _blocksWithCalls(NULL),
     _codeCache(0),
     _committedToCodeCache(false),
     _dummyTempStorageRefNode(NULL),
     _blockRegisterPressureCache(NULL),
     _simulatedNodeStates(NULL),
     _availableSpillTemps(getTypedAllocator<TR::SymbolReference*>(TR::comp()->allocator())),
     _counterBlocks(getTypedAllocator<TR::Block*>(TR::comp()->allocator())),
     _compressedRefs(getTypedAllocator<TR::Node*>(TR::comp()->allocator())),
     _liveReferenceList(getTypedAllocator<TR_LiveReference*>(TR::comp()->allocator())),
     _snippetList(getTypedAllocator<TR::Snippet*>(TR::comp()->allocator())),
     _registerArray(self()->comp()->trMemory()),
     _spill4FreeList(getTypedAllocator<TR_BackingStore*>(TR::comp()->allocator())),
     _spill8FreeList(getTypedAllocator<TR_BackingStore*>(TR::comp()->allocator())),
     _spill16FreeList(getTypedAllocator<TR_BackingStore*>(TR::comp()->allocator())),
     _internalPointerSpillFreeList(getTypedAllocator<TR_BackingStore*>(TR::comp()->allocator())),
     _spilledRegisterList(NULL),
     _firstTimeLiveOOLRegisterList(NULL),
     _referencedRegistersList(NULL),
     _variableSizeSymRefPendingFreeList(getTypedAllocator<TR::SymbolReference*>(TR::comp()->allocator())),
     _variableSizeSymRefFreeList(getTypedAllocator<TR::SymbolReference*>(TR::comp()->allocator())),
     _variableSizeSymRefAllocList(getTypedAllocator<TR::SymbolReference*>(TR::comp()->allocator())),
     _accumulatorNodeUsage(0),
     _nodesUnderComputeCCList(getTypedAllocator<TR::Node*>(TR::comp()->allocator())),
     _nodesToUncommonList(getTypedAllocator<TR::Node*>(TR::comp()->allocator())),
     _nodesSpineCheckedList(getTypedAllocator<TR::Node*>(TR::comp()->allocator())),
     _collectedSpillList(getTypedAllocator<TR_BackingStore*>(TR::comp()->allocator())),
     _allSpillList(getTypedAllocator<TR_BackingStore*>(TR::comp()->allocator())),
     _relocationList(getTypedAllocator<TR::Relocation*>(TR::comp()->allocator())),
     _aotRelocationList(getTypedAllocator<TR::Relocation*>(TR::comp()->allocator())),
     _staticRelocationList(_compilation->allocator()),
     _breakPointList(getTypedAllocator<uint8_t*>(TR::comp()->allocator())),
     _jniCallSites(getTypedAllocator<TR_Pair<TR_ResolvedMethod,TR::Instruction> *>(TR::comp()->allocator())),
     _lowestSavedReg(0),
     _preJitMethodEntrySize(0),
     _jitMethodEntryPaddingSize(0),
     _lastInstructionBeforeCurrentEvaluationTreeTop(NULL),
     _unlatchedRegisterList(NULL),
     _indentation(2),
     _preservedRegsInPrologue(NULL),
     _currentBlock(NULL),
     _realVMThreadRegister(NULL),
     _internalControlFlowNestingDepth(0),
     _internalControlFlowSafeNestingDepth(0),
     _stackOfArtificiallyInflatedNodes(self()->comp() ? self()->comp()->trMemory() : 0, 16),
     _stackOfMemoryReferencesCreatedDuringEvaluation(self()->comp() ? self()->comp()->trMemory() : 0, 16),
     _afterRA(false),
     randomizer(self()->comp()),
     _outOfLineColdPathNestedDepth(0),
     _codeGenPhase(self()),
     _symbolDataTypeMap(self()->comp()->allocator()),
     _lmmdFailed(false)
   {
   _machine = new (self()->trHeapMemory()) TR::Machine(self());
   _disableInternalPointers = self()->comp()->getOption(TR_MimicInterpreterFrameShape) ||
                               self()->comp()->getOptions()->realTimeGC() ||
                               self()->comp()->getOptions()->getOption(TR_DisableInternalPointers);

   uintptrj_t maxSize = TR::Compiler->vm.getOverflowSafeAllocSize(self()->comp());
   int32_t i;

   for (i = 0; i < NumRegisterKinds; ++i)
      {
      _liveRegisters[i] = NULL;
      _liveRealRegisters[i] = 0;
      }

   for (i = 0 ; i < TR_NumLinkages; ++i)
      _linkages[i] = NULL;

   _maxObjectSizeGuaranteedNotToOverflow = (maxSize > UINT_MAX) ? UINT_MAX : maxSize;

   if (self()->comp()->getDebug())
      self()->comp()->getDebug()->resetDebugData();

   self()->setIsLeafMethod();
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

   preLowerTrees();

   TR::TreeTop * tt;
   TR::Node * node;

   vcount_t visitCount = self()->comp()->incVisitCount();

   for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      node = tt->getNode();

      TR_ASSERT(node->getVisitCount() != visitCount, "Code Gen: error in lowering trees");
      TR_ASSERT(node->getReferenceCount() == 0, "Code Gen: error in lowering trees");

      lowerTreesPreTreeTopVisit(tt, visitCount);


      // First lower the children
      //
      self()->lowerTreesWalk(node, tt, visitCount);

      // If the tree needs to be lowered, call the VM to lower it
      //
      lowerTreeIfNeeded(node, 0, 0, tt);


      lowerTreesPostTreeTopVisit(tt, visitCount);

      }

   self()->postLowerTrees();
   }


void
OMR::CodeGenerator::lowerTreesWalk(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {

   parent->setVisitCount(visitCount);

   lowerTreesPreChildrenVisit(parent, treeTop, visitCount);

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
         lowerTreeIfNeeded(child, childCount, parent, treeTop);
         }
      }

   lowerTreesPostChildrenVisit(parent, treeTop, visitCount);

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

   self()->setNextAvailableBlockIndex(comp->getFlowGraph()->getNextNodeNumber() + 1);

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

   beginInstructionSelection();

   {
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());

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
         self()->setCurrentBlockIndex(block->getNumber());

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
   } // scope of the stack memory region

   if (comp->getOption(TR_TraceCG) || debug("traceGRA"))
      {
      comp->incVisitCount();
      }

   if (self()->getDebug())
      {
      self()->getDebug()->roundAddressEnumerationCounters();
      }

   endInstructionSelection();

   if (comp->getOption(TR_TraceCG))
      {
      diagnostic("</selection>\n");
      }
   }

bool
OMR::CodeGenerator::use64BitRegsOn32Bit()
   {
#ifdef TR_TARGET_S390
   if (!(TR::Compiler->target.isZOS() || TR::Compiler->target.isLinux()))
      return false;
   else if (TR::Compiler->target.is64Bit())
      return false;
   else
      {
      bool longReg = self()->comp()->getOptions()->getOption(TR_Enable64BitRegsOn32Bit);
      bool longRegHeur = self()->comp()->getOptions()->getOption(TR_Enable64BitRegsOn32BitHeuristic);
      bool use64BitRegs = (longReg && !longRegHeur && self()->comp()->getJittedMethodSymbol()->mayHaveLongOps()) ||
                          (longReg && longRegHeur && self()->comp()->useLongRegAllocation());
      return use64BitRegs;
      }
#endif // TR_TARGET_S390
   return false;
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

void
OMR::CodeGenerator::setVMThreadRequired(bool v)
   {
   }

bool
OMR::CodeGenerator::getSupportsTLE()
   {
   return self()->getSupportsTM();
   }

/** \brief
 *     A query about whether ibyteswap is supported
 *
 *  \return
 *     True if ibyteswap is supported in codegen
 *
 *  \note
 *     Override the query on the platform where ibyteswap is implemented
 */
bool
OMR::CodeGenerator::getSupportsIbyteswap()
   {
   return false;
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
   return self()->comp()->getOptions()->getTraceCGOption(TR_TraceCGBinaryCodedDecimal);
   }

void OMR::CodeGenerator::traceBCDEntry(char *str, TR::Node *node)
   {
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"EVAL: %s 0x%p - start\n",str,node);
   }

void OMR::CodeGenerator::traceBCDExit(char *str, TR::Node *node)
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
      return _linkages[lc] ? _linkages[lc] : createLinkage(lc);
   }

void OMR::CodeGenerator::initializeLinkage()
   {
   TR::Linkage * linkage = NULL;
   // Allow the project/GlobalCompilationInfo to set the body linkage
   // Expect to call once during initialization
   //
   linkage = createLinkageForCompilation();
   linkage = linkage ? linkage : createLinkage(self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   self()->setLinkage(linkage);
   }

TR::Linkage *OMR::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR_ASSERT(0, "Unimplemented createLinkage");
   return NULL;
   }

bool
OMR::CodeGenerator::mulDecompositionCostIsJustified(int numOfOperations, char bitPosition[], char operationType[], int64_t value)
   {
   if (self()->comp()->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition))
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
OMR::CodeGenerator::isGlobalVRF(TR_GlobalRegisterNumber n)
   {
   return self()->hasGlobalVRF() &&
          n >= self()->getFirstGlobalVRF() &&
          n <= self()->getLastGlobalVRF();
   }

bool
OMR::CodeGenerator::isGlobalFPR(TR_GlobalRegisterNumber n)
   {
   return !self()->isGlobalGPR(n) && !self()->isGlobalHPR(n);
   }

TR_BitVector *
OMR::CodeGenerator::getBlocksWithCalls()
   {
   if (!_blocksWithCalls)
      self()->computeBlocksWithCalls();
   return _blocksWithCalls;
   }

bool OMR::CodeGenerator::profiledPointersRequireRelocation() { return self()->comp()->compileRelocatableCode(); }

bool OMR::CodeGenerator::needGuardSitesEvenWhenGuardRemoved() { return self()->comp()->compileRelocatableCode(); }

bool OMR::CodeGenerator::supportVMInternalNatives() { return !self()->comp()->compileRelocatableCode(); }

bool OMR::CodeGenerator::supportsNativeLongOperations() { return (TR::Compiler->target.is64Bit() || self()->use64BitRegsOn32Bit()); }

void
OMR::CodeGenerator::setVMThreadSpillInstruction(TR::Instruction *i)
   {
   if (_vmThreadSpillInstr == NULL)
      {
      _vmThreadSpillInstr = i;
      }
   else
      {
      _vmThreadSpillInstr = (TR::Instruction *)0xffffffff; // set to sentinel to indicate that
                                                          // more than one location is needed and
                                                          // should therefore simply spill in the prologue
      }
   }


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


#ifdef TR_HOST_S390
uint16_t OMR::CodeGenerator::getNumberOfGlobalGPRs()
   {
   if (self()->comp()->cg()->supportsHighWordFacility() && !self()->comp()->getOption(TR_DisableHighWordRA))
      {
      return _firstGlobalHPR;
      }
   return _lastGlobalGPR + 1;
   }
#endif

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

TR::Register *OMR::CodeGenerator::allocate64bitRegister()
   {
   TR::Register * temp = NULL;

   if (TR::Compiler->target.is64Bit())
      temp = self()->allocateRegister();
   else
      temp = self()->allocateRegister(TR_GPR64);

   return temp;
   }

void OMR::CodeGenerator::apply8BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   { *(int8_t *)cursor += (int8_t)(intptrj_t)label->getCodeLocation(); }
void OMR::CodeGenerator::apply12BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label, bool isCheckDisp)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply12BitLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   { *(int16_t *)cursor += (int16_t)(intptrj_t)label->getCodeLocation(); }
void OMR::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label,int8_t d, bool isInstrOffset)
   { *(int16_t *)cursor += (int16_t)(intptrj_t)label->getCodeLocation(); }
void OMR::CodeGenerator::apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply24BitLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply16BitLoadLabelRelativeRelocation(TR::Instruction *, TR::LabelSymbol *, TR::LabelSymbol *, int32_t)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply16BitLoadLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply32BitLoadLabelRelativeRelocation(TR::Instruction *, TR::LabelSymbol *, TR::LabelSymbol *, int32_t)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply32BitLoadLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply64BitLoadLabelRelativeRelocation(TR::Instruction *, TR::LabelSymbol *)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply64BitLoadLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply32BitLabelRelativeRelocation"); }
void OMR::CodeGenerator::apply32BitLabelTableRelocation(int32_t * cursor, TR::LabelSymbol *)
   { TR_ASSERT(0, "unexpected call to OMR::CodeGenerator::apply32BitLabelTableRelocation"); }

void OMR::CodeGenerator::addSnippet(TR::Snippet *s)
   {
   _snippetList.push_front(s);
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


bool OMR::CodeGenerator::getTraceRAOption(uint32_t mask){ return self()->comp()->getOptions()->getTraceRAOption(mask); }

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

// J9
//
TR::Node *
OMR::CodeGenerator::createOrFindClonedNode(TR::Node *node, int32_t numChildren)
   {
   TR_HashId index;
   if (!_uncommmonedNodes.locate(node->getGlobalIndex(), index))
      {
      // has not been uncommoned already, clone and store for later
      TR::Node *clone = TR::Node::copy(node, numChildren);
      _uncommmonedNodes.add(node->getGlobalIndex(), index, clone);
      node = clone;
      }
   else
      {
      // found previously cloned node
      node = (TR::Node *) _uncommmonedNodes.getData(index);
      }
   return node;
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
      if (addr1->getOpCodeValue() == TR::aload && addr2->getOpCodeValue() == TR::aload &&
               addr1->getSymbolReference() == addr2->getSymbolReference())
         {
         foundMatch = true;
         }
      else if (addr1->getOpCodeValue() == TR::aloadi && addr2->getOpCodeValue() == TR::aloadi &&
               addr1->getSymbolReference() == addr2->getSymbolReference() &&
               self()->nodeMatches(addr1->getFirstChild(), addr2->getFirstChild(),addressesUnderSameTreeTop))
         {
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
OMR::CodeGenerator::zeroOutAutoOnEdge(
      TR::SymbolReference *liveAutoSymRef,
      TR::Block *block,
      TR::Block *succBlock,
      TR::list<TR::Block*> *newBlocks,
      TR_ScratchList<TR::Node> *fsdStores)
   {
   TR::Block *storeBlock = NULL;
   if ((succBlock->getPredecessors().size() == 1))
      storeBlock = succBlock;
   else
      {
      for (auto blocksIt = newBlocks->begin(); blocksIt != newBlocks->end(); ++blocksIt)
         {
         if ((*blocksIt)->getSuccessors().front()->getTo()->asBlock() == succBlock)
            {
            storeBlock = *blocksIt;
            break;
            }
         }
      }

   if (!storeBlock)
      {
      TR::TreeTop * startTT = succBlock->getEntry();
      TR::Node * startNode = startTT->getNode();
      TR::Node * glRegDeps = NULL;
      if (startNode->getNumChildren() > 0)
         glRegDeps = startNode->getFirstChild();

      TR::Block * newBlock = block->splitEdge(block, succBlock, self()->comp(), NULL, false);

      if (debug("traceFSDSplit"))
         diagnostic("\nSplitting edge, create new intermediate block_%d", newBlock->getNumber());

      if (glRegDeps)
         {
         TR::Node *duplicateGlRegDeps = glRegDeps->duplicateTree();
         TR::Node *origDuplicateGlRegDeps = duplicateGlRegDeps;
         duplicateGlRegDeps = TR::Node::copy(duplicateGlRegDeps);
         newBlock->getEntry()->getNode()->setNumChildren(1);
         newBlock->getEntry()->getNode()->setAndIncChild(0, origDuplicateGlRegDeps);
         for (int32_t i = origDuplicateGlRegDeps->getNumChildren() - 1; i >= 0; --i)
            {
            TR::Node * dep = origDuplicateGlRegDeps->getChild(i);
            if(self()->comp()->getOption(TR_MimicInterpreterFrameShape) || self()->comp()->getOption(TR_PoisonDeadSlots))
               dep->setRegister(NULL); // basically need to do prepareNodeForInstructionSelection
            duplicateGlRegDeps->setAndIncChild(i, dep);
            }
         if(self()->comp()->getOption(TR_MimicInterpreterFrameShape) || self()->comp()->getOption(TR_PoisonDeadSlots))
            {
            TR::Node *glRegDepsParent;
            if (  (newBlock->getSuccessors().size() == 1)
               && newBlock->getSuccessors().front()->getTo()->asBlock()->getEntry() == newBlock->getExit()->getNextTreeTop())
               {
               glRegDepsParent = newBlock->getExit()->getNode();
               }
            else
               {
               glRegDepsParent = newBlock->getExit()->getPrevTreeTop()->getNode();
               TR_ASSERT(glRegDepsParent->getOpCodeValue() == TR::Goto, "Expected block to fall through or end in goto; it ends with %s %s\n",
                  self()->getDebug()->getName(glRegDepsParent->getOpCodeValue()), self()->getDebug()->getName(glRegDepsParent));
               }
            if (self()->comp()->getOption(TR_TraceCG))
               traceMsg(self()->comp(), "zeroOutAutoOnEdge: glRegDepsParent is %s\n", self()->getDebug()->getName(glRegDepsParent));
            glRegDepsParent->setNumChildren(1);
            glRegDepsParent->setAndIncChild(0, duplicateGlRegDeps);
            }
         else           //original path
            {
            newBlock->getExit()->getNode()->setNumChildren(1);
            newBlock->getExit()->getNode()->setAndIncChild(0, duplicateGlRegDeps);
            }
         }

      newBlock->setLiveLocals(new (self()->trHeapMemory()) TR_BitVector(*succBlock->getLiveLocals()));
      newBlock->getEntry()->getNode()->setLabel(generateLabelSymbol(self()));


      if (self()->comp()->getOption(TR_PoisonDeadSlots))
         {
         if (self()->comp()->getOption(TR_TraceCG))
            traceMsg(self()->comp(), "POISON DEAD SLOTS --- New Block Created %d\n", newBlock->getNumber());
         newBlock->setIsCreatedAtCodeGen();
         }

      newBlocks->push_front(newBlock);
      storeBlock = newBlock;
      }
   TR::Node *storeNode;

   if (self()->comp()->getOption(TR_PoisonDeadSlots))
      storeNode = generatePoisonNode(self()->comp(), block, liveAutoSymRef);
   else
      storeNode = TR::Node::createStore(liveAutoSymRef, TR::Node::aconst(block->getEntry()->getNode(), 0));

   if (storeNode)
      {
      TR::TreeTop *storeTree = TR::TreeTop::create(self()->comp(), storeNode);
      storeBlock->prepend(storeTree);
      fsdStores->add(storeNode);
      }
   }


void
OMR::CodeGenerator::reserveCodeCache()
   {
   _codeCache = self()->fe()->getDesignatedCodeCache(self()->comp());
   if (!_codeCache) // Cannot reserve a cache; all are used
      {
      // We may reach this point if all code caches have been used up
      // If some code caches have some space but cannot be used because they are reserved
      // we will throw an exception in the call to getDesignatedCodeCache

      if (self()->comp()->compileRelocatableCode())
         {
         self()->comp()->failCompilation<TR::RecoverableCodeCacheError>("Cannot reserve code cache");
         }

      self()->comp()->failCompilation<TR::CodeCacheError>("Cannot reserve code cache");
      }
   }

uint8_t *
OMR::CodeGenerator::allocateCodeMemory(uint32_t warmSize, uint32_t coldSize, uint8_t **coldCode, bool isMethodHeaderNeeded)
   {
   uint8_t *warmCode;
   warmCode = self()->fe()->allocateCodeMemory(self()->comp(), warmSize, coldSize, coldCode, isMethodHeaderNeeded);
   if (self()->getCodeGeneratorPhase() == TR::CodeGenPhase::BinaryEncodingPhase)
      {
      self()->commitToCodeCache();
      }
   TR_ASSERT( !((warmSize && !warmCode) || (coldSize && !coldCode)), "Allocation failed but didn't throw an exception");
   return warmCode;
   }

uint8_t *
OMR::CodeGenerator::allocateCodeMemory(uint32_t size, bool isCold, bool isMethodHeaderNeeded)
   {
   uint8_t *coldCode;
   if (isCold)
      {
      self()->allocateCodeMemory(0, size, &coldCode, isMethodHeaderNeeded);
      return coldCode;
      }
   return self()->allocateCodeMemory(size, 0, &coldCode, isMethodHeaderNeeded);
   }

void
OMR::CodeGenerator::resizeCodeMemory()
   {
   int32_t codeLength = self()->getCodeEnd()-self()->getBinaryBufferStart();
   self()->fe()->resizeCodeMemory(self()->comp(), self()->getBinaryBufferStart(), codeLength);
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
   if (secondChild->getOpCodeValue() == TR::lconst || secondChild->getOpCodeValue() == TR::luconst)
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
      multiplier = secondChild->getInt();
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

   if (node->getOpCodeValue() == TR::imul || node->getOpCodeValue() == TR::iumul)
      TR::Node::recreate(node, TR::ishl);
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
   if (self()->comp()->getOptions()->getOption(TR_DisableDirectMemoryOps))
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
   if (TR::Compiler->target.cpu.isX86() && valueChild->getOpCode().isMul())
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
   //TR_ASSERTC(missedSite == -1, comp(), "Site %d is missing relocation\n", missedSite);
   }

#if defined(TR_HOST_ARM)
void armCodeSync(uint8_t *start, uint32_t size);
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
      else if (d > div32BitMagicValues[mid][0])
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
   //
   // The table is composed of 32-bit values because the compiler seems to have a problem
   // statically initializing it with int64_t constant values.

   #define NUM_64BIT_MAGIC_VALUES 6
   #define TOINT64(x) (*( (int64_t *) &x))
   static uint32_t div64BitMagicValues[NUM_64BIT_MAGIC_VALUES][6] =

   //     Denominator        Magic Value          Shift

      { {    3, 0,    0x55555556, 0x55555555,    0, 0 },
        {    5, 0,    0x66666667, 0x66666666,    1, 0 },
        {    7, 0,    0x24924925, 0x49249249,    1, 0 },
        {    9, 0,    0x71c71c72, 0x1c71c71c,    0, 0 },
        {   10, 0,    0x66666667, 0x66666666,    2, 0 },
        {   12, 0,    0xaaaaaaab, 0x2aaaaaaa,    1, 0 } };

   // Quick check if 'd' is cached.
   first = 0;
   last  = NUM_64BIT_MAGIC_VALUES-1;
   while (first <= last)
      {
      mid = (first + last) / 2;
      if (TOINT64(div64BitMagicValues[mid][0]) == d)
         {
         *m = TOINT64(div64BitMagicValues[mid][2]);
         *s = TOINT64(div64BitMagicValues[mid][4]);
         return;
         }
      else if (d > TOINT64(div64BitMagicValues[mid][0]))
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
   nc = -1 - (-d)%d;
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
   uintptr_t boundary = self()->comp()->getOptions()->getJitMethodEntryAlignmentBoundary(self());

   /* Align cursor to boundary */
   if (boundary && (boundary & boundary - 1) == 0)
      {
      uintptr_t round = boundary - 1;
      uintptr_t offset = self()->comp()->cg()->getPreJitMethodEntrySize();

      _binaryBufferCursor += offset;
      _binaryBufferCursor = (uint8_t *)(((uintptr_t)_binaryBufferCursor + round) & ~round);
      _binaryBufferCursor -= offset;
      self()->setJitMethodEntryPaddingSize(_binaryBufferCursor - _binaryBufferStart);
      memset(_binaryBufferStart, 0, self()->getJitMethodEntryPaddingSize());
      }

   return _binaryBufferCursor;
   }


int32_t
OMR::CodeGenerator::setEstimatedLocationsForSnippetLabels(int32_t estimatedSnippetStart)
   {
   TR::Snippet *cursor;

   self()->setEstimatedSnippetStart(estimatedSnippetStart);

   if (self()->hasTargetAddressSnippets())
      {
      estimatedSnippetStart = self()->setEstimatedLocationsForTargetAddressSnippetLabels(estimatedSnippetStart);
      }

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

   for (auto iterator = _snippetList.begin(); iterator != _snippetList.end(); ++iterator)
      {
      codeOffset = (*iterator)->emitSnippet();
      if (codeOffset != NULL)
         {
         TR_ASSERT((*iterator)->getLength(self()->getBinaryBufferCursor()-self()->getBinaryBufferStart()) + self()->getBinaryBufferCursor() >= codeOffset,
                 "%s length estimate must be conservatively large (snippet @ " POINTER_PRINTF_FORMAT ", estimate=%d, actual=%d)",
                 self()->getDebug()->getName(*iterator), *iterator,
                 (*iterator)->getLength(self()->getBinaryBufferCursor()-self()->getBinaryBufferStart()),
                 codeOffset - self()->getBinaryBufferCursor());
         self()->setBinaryBufferCursor(codeOffset);
         }
      }

   retVal = self()->getBinaryBufferCursor();

   // Emit target address snippets first.
   //
   if (self()->hasTargetAddressSnippets())
      {
      self()->emitTargetAddressSnippets();
      }

   // Emit constant data snippets last.
   //
   if (self()->hasDataSnippets())
      {
      self()->emitDataSnippets();
      }

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
       l1OpCode == TR::multianewarray  ||
       l1OpCode == TR::MergeNew)
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

TR::Instruction *OMR::CodeGenerator::getVirtualGuardForPatching(TR::Instruction *vgdnop)
   {
   TR_ASSERT(vgdnop->isVirtualGuardNOPInstruction(),
      "getGuardForPatching called with non VirtualGuardNOPInstruction [%p] - this only works for guards!", vgdnop);

   if (!vgdnop->isMergeableGuard())
      return vgdnop;

   // If there are no previous instructions the instruction must be the patch point
   // as there is nothing to merge with
   if (!vgdnop->getPrev())
      return vgdnop;

   // Guard merging is only done when the guard trees are consecutive in treetop order
   // only separated by BBStart and BBEnd trees Skip back to the BBStart since we need
   // to get the block for vgdnop
   if (vgdnop->getPrev()->getNode()->getOpCodeValue() != TR::BBStart)
      return vgdnop;

   // there could be local RA generated reg-reg movs between guards so we resort to checking the
   // trees and making sure that the guards are in treetop order
   // we only merge blocks in the same extended blocks - virtual guard head merger will
   // arrange for this to happen when possible for guards
   TR::Instruction *toReturn = vgdnop;
   TR::Block *extendedBlockStart = vgdnop->getPrev()->getNode()->getBlock()->startOfExtendedBlock();
   for (TR::Instruction *prevI = vgdnop->getPrev(); prevI; prevI = prevI->getPrev())
      {
      if (prevI->isVirtualGuardNOPInstruction())
         {
         if (self()->areMergeableGuards(prevI, vgdnop))
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
             prevI->getNode()->getBranchDestination() == vgdnop->getNode()->getBranchDestination())
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
   if (toReturn != vgdnop)
      {
      TR::DebugCounter::incStaticDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "guardMerge/(%s)", self()->comp()->signature()));
      if (self()->comp()->getOption(TR_TraceCG))
         traceMsg(self()->comp(), "vgdnop instruction [%p] begins scanning for patch instructions for mergeable guard [%p]\n", vgdnop, toReturn);
      }
   return toReturn;
   }

TR::Instruction
*OMR::CodeGenerator::getInstructionToBePatched(TR::Instruction *vgdnop)
   {
   TR::Instruction   * nextI;
   TR::Node          *firstBBEnd = NULL;

   for (nextI=self()->getVirtualGuardForPatching(vgdnop)->getNext(); nextI!=NULL; nextI=nextI->getNext())
      {
      if (nextI->isVirtualGuardNOPInstruction())
         {
         if (!self()->areMergeableGuards(vgdnop, nextI))
            return NULL;
         continue;
         }

      if (nextI->isPatchBarrier() || self()->comp()->isPICSite(nextI))
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
OMR::CodeGenerator::sizeOfInstructionToBePatched(TR::Instruction *vgdnop)
   {
   TR::Instruction *instToBePatched = self()->getInstructionToBePatched(vgdnop);
   if (instToBePatched)
      return instToBePatched->getBinaryLengthLowerBound();
   else
        return 0;
   }

int32_t
OMR::CodeGenerator::sizeOfInstructionToBePatchedHCRGuard(TR::Instruction *vgdnop)
   {
   TR::Instruction   *nextI;
   TR::Node          *firstBBEnd = NULL;
   int32_t             accumulatedSize = 0;

   for (nextI=self()->getInstructionToBePatched(vgdnop); nextI!=NULL; nextI=nextI->getNext())
      {
      if (nextI->isVirtualGuardNOPInstruction())
         {
         if (!self()->areMergeableGuards(vgdnop, nextI))
            break;
         continue;
         }

      if (nextI->isPatchBarrier() || self()->comp()->isPICSite(nextI))
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


TR::Instruction *
OMR::CodeGenerator::saveOrRestoreRegisters(TR_BitVector *regs, TR::Instruction *cursor, bool doSaves)
   {
   // use store/load multiple to save or restore registers
   // in the prologue/epilogue on platforms that support store/load
   // multiple instructions (e.g. ppc32 and z)
   //

   // the registers need to be in sequence
   //
   int32_t startIdx = -1;
   int32_t endIdx = -1;
   int32_t prevIdx = -1;
   int32_t i = 0;
   int32_t numRegs = regs->elementCount();
   traceMsg(self()->comp(), "numRegs %d at cursor %p\n", numRegs, cursor);

   int32_t savedRegs = 0;
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
            // for the regs upto this point
            //
            if (i > 1)
               {
               // insert store/load multiple
               //
               traceMsg(self()->comp(), "found pattern for start %d end %d at cursor %p\n", startIdx, endIdx, cursor);
               cursor = self()->getLinkage()->composeSavesRestores(cursor, startIdx, endIdx, -1 /*_mapRegsToStack[startIdx]*/, numRegs, doSaves);
               savedRegs += i;
               }
            else
               {
               // insert a single store/load for prevIdx
               //
               traceMsg(self()->comp(), "pattern broken idx %d at cursor %p doSaves %d\n", prevIdx, cursor, doSaves);
               if (doSaves)
                  cursor = self()->getLinkage()->savePreservedRegister(cursor, prevIdx, -1);
               else
                  cursor =self()->getLinkage()->restorePreservedRegister(cursor, prevIdx, -1);
               savedRegs++;
               }
            startIdx = -1;
            endIdx = -1;
            i = 0;
            }
        }
      else
         startIdx = curIdx;
      i++;
      prevIdx = curIdx;
      }

   traceMsg(self()->comp(), "savedRegs %d at cursor %p startIdx %d endIdx %d\n", savedRegs, cursor, startIdx, endIdx);
   // do the remaining
   if ((numRegs > 1) &&
         (startIdx != -1))
      {
      // compose save restores
      cursor = self()->getLinkage()->composeSavesRestores(cursor, startIdx, endIdx, -1 /*_mapRegsToStack[startIdx]*/, numRegs, doSaves);
      savedRegs += (endIdx - startIdx + 1);
      }

   if (savedRegs != numRegs)
      {
      resIt.setBitVector(*regs);
      for (; savedRegs; --savedRegs) resIt.getNextElement();
      while (resIt.hasMoreElements())
         {
         int32_t curIdx = resIt.getNextElement();
         // insert a single store for curIdx
         //
         if (doSaves)
            cursor = self()->getLinkage()->savePreservedRegister(cursor, curIdx, -1);
         else
            cursor = self()->getLinkage()->restorePreservedRegister(cursor, curIdx, -1);
         }
      }
   return cursor;
   }

#ifdef DEBUG

void
OMR::CodeGenerator::dumpSpillStats(TR_FrontEnd *fe)
   {
   if (debug("spillStats"))
      {
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"Register Spilling/Rematerialization:");
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%8d registers spilled", _totalNumSpilledRegisters);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%8d constants rematerialized", _totalNumRematerializedConstants);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%8d locals rematerialized", _totalNumRematerializedLocals);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%8d statics rematerialized", _totalNumRematerializedStatics);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%8d indirect accesses rematerialized", _totalNumRematerializedIndirects);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%8d addresses rematerialized", _totalNumRematerializedAddresses);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%8d XMMRs rematerialized", _totalNumRematerializedXMMRs);
      }
   }

void
OMR::CodeGenerator::shutdown(TR_FrontEnd *fe, TR::FILE *logFile)
   {
   }
#endif

// I need to preserve the type information for monitorenter/exit through
// code generation, but the secondChild is being used for other monitor
// optimizations and I can't find anywhere to stick it on the TR::Node.
// Creating the node with more children doesn't seem to help either.
//
void
OMR::CodeGenerator::addMonClass(TR::Node* monNode, TR_OpaqueClassBlock* clazz)
   {
   _monitorMapping.add(monNode);
   _monitorMapping.add(clazz);
   }

TR_OpaqueClassBlock *
OMR::CodeGenerator::getMonClass(TR::Node* monNode)
   {
   for (int i = 0; i < _monitorMapping.size(); i += 2)
      if (_monitorMapping[i] == monNode)
         return (TR_OpaqueClassBlock *) _monitorMapping[i+1];
   return 0;
   }


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
         byteValue = testWord >> (56 - bitCount);
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
   else if (node->getOpCodeValue() == TR::iuconst)
      value = (int64_t) node->getUnsignedInt();
   else if (node->getOpCodeValue() == TR::luconst)
      value = node->getUnsignedLongInt();
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
   if (self()->comp()->getOptions()->getOption(TR_DisableTraps))
      return false;

   if (!doChecks)
      return true;

   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCode &opCode = firstChild->getOpCode();

   if (opCode.isLoadVar() || (TR::Compiler->target.is64Bit() && opCode.getOpCodeValue() == TR::l2i))
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

bool OMR::CodeGenerator::ilOpCodeIsSupported(TR::ILOpCodes o)
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
   TR::DebugCounterAggregation *aggregatedCounters = self()->comp()->getPersistentInfo()->getDynamicCounters()->createAggregation(self()->comp());
   aggregatedCounters->aggregateStandardCounters(self()->comp(), cursor->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
      return cursor;
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
   TR::DebugCounterAggregation *aggregatedCounters = self()->comp()->getPersistentInfo()->getDynamicCounters()->createAggregation(self()->comp());
   aggregatedCounters->aggregateStandardCounters(self()->comp(), cursor->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
      return cursor;
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
   TR::DebugCounterAggregation *aggregatedCounters = self()->comp()->getPersistentInfo()->getDynamicCounters()->createAggregation(self()->comp());
   aggregatedCounters->aggregateStandardCounters(self()->comp(), cursor->getNode(), name, delta, fidelity, staticDelta);
   if (!aggregatedCounters->hasAnyCounters())
      return cursor;
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
   if (r->isAOTRelocation())
      {
      TR_ASSERT(false, "Cannot use addRelocation to add an AOT relocation. Please use addAOTRelocation here");
      }
   else
      {
      _relocationList.push_front(r);
      }
   }

void OMR::CodeGenerator::addAOTRelocation(TR::Relocation *r, const char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node)
   {
   TR_ASSERT(generatingFileName, "AOT relocation location has improper NULL filename specified");
   if (self()->comp()->compileRelocatableCode())
      {
      TR::RelocationDebugInfo *genData = new(self()->trHeapMemory()) TR::RelocationDebugInfo;
      genData->file = generatingFileName;
      genData->line = generatingLineNumber;
      genData->node = node;
      r->setDebugInfo(genData);
      _aotRelocationList.push_back(r);
      }
   }

void OMR::CodeGenerator::addAOTRelocation(TR::Relocation *r, TR::RelocationDebugInfo* info)
   {
   if (self()->comp()->compileRelocatableCode())
      {
      TR_ASSERT(info, "AOT relocation location does not have associated debug information");
      r->setDebugInfo(info);
      _aotRelocationList.push_back(r);
      }
   }

void
OMR::CodeGenerator::addStaticRelocation(const TR::StaticRelocation &relocation)
   {
   _staticRelocationList.push_back(relocation);
   }

intptrj_t OMR::CodeGenerator::hiValue(intptrj_t address)
   {
   if (self()->comp()->compileRelocatableCode()) // We don't want to store values using HI_VALUE at compile time, otherwise, we do this a 2nd time when we relocate (and new value is based on old one)
      return ((address >> 16) & 0x0000FFFF);
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


TR::RegisterPair * OMR::CodeGenerator::allocate64bitRegisterPair(TR::Register * lo, TR::Register * ho)
   {
   TR::RegisterPair *temp = new (self()->trHeapMemory()) TR::RegisterPair(TR_GPR64);
   temp->setLowOrder(lo, self());
   temp->setHighOrder(ho, self());
   self()->addAllocatedRegisterPair(temp);
   return temp;
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
   if (!self()->comp()->compileRelocatableCode() || TR::Compiler->target.is32Bit() || node==NULL)
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

   if (node->getFirstChild()->isSingleRefUnevaluated() &&
       valueChild->isSingleRefUnevaluated() &&
       valueChild->getFirstChild()->isSingleRefUnevaluated())
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
