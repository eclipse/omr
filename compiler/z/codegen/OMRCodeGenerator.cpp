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

// On zOS XLC linker can't handle files with same name at link time.
// This workaround with pragma is needed. What this does is essentially
// give a different name to the codesection (csect) for this file. So it
// doesn't conflict with another file with the same name.
#pragma csect(CODE,"TRZCGBase#C")
#pragma csect(STATIC,"TRZCGBase#S")
#pragma csect(TEST,"TRZCGBase#T")

#include <algorithm>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterIterator.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/StorageInfo.hpp"
#include "codegen/SystemLinkage.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/PrivateLinkage.hpp"
#include "control/RecompilationInfo.hpp"
#endif
#include "cs2/hashtab.h"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/Processors.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
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
#include "infra/List.hpp"
#include "infra/Random.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/Stack.hpp"
#include "infra/CfgEdge.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "ras/Delimiter.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/EndianConversion.hpp"
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390Peephole.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"
#include "z/codegen/SystemLinkageLinux.hpp"
#include "z/codegen/SystemLinkagezOS.hpp"

#if J9_PROJECT_SPECIFIC
#include "z/codegen/S390Recompilation.hpp"
#include "z/codegen/S390Register.hpp"
#endif

class TR_IGNode;
namespace TR { class DebugCounterBase; }
namespace TR { class SimpleRegex; }

#define OPT_DETAILS "O^O CODE GENERATION: "

bool compareNodes(TR::Node * node1, TR::Node * node2, TR::CodeGenerator *cg);
bool isMatchingStoreRestore(TR::Instruction *cursorLoad, TR::Instruction *cursorStore, TR::CodeGenerator *cg);
void handleLoadWithRegRanges(TR::Instruction *inst, TR::CodeGenerator *cg);

void
OMR::Z::CodeGenerator::preLowerTrees()
   {
   OMR::CodeGenerator::preLowerTrees();

   _ialoadUnneeded.init();

   }

void
OMR::Z::CodeGenerator::lowerTreesWalk(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
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

      self()->checkIsUnneededIALoad(parent, child, treeTop);
      }

   self()->lowerTreesPostChildrenVisit(parent, treeTop, visitCount);

   }

void
OMR::Z::CodeGenerator::checkIsUnneededIALoad(TR::Node *parent, TR::Node *node, TR::TreeTop *tt)
   {

   ListIterator<TR_Pair<TR::Node, int32_t> > listIter(&_ialoadUnneeded);
   bool inList = false;

   if (node->getOpCodeValue() == TR::aloadi)
      {
      TR_Pair<TR::Node, int32_t> *ptr;
      for (ptr = listIter.getFirst(); ptr; ptr = listIter.getNext())
         {
         if (ptr->getKey() == node)
            {
            inList = true;
            break;
            }
         }
      if (inList)
         {
         uintptr_t temp  = (uintptr_t)ptr->getValue();
         int32_t updatedTemp = (int32_t)temp + 1;
         ptr->setValue((int32_t*)(intptr_t)updatedTemp);
         }
      else // Not in list
         {
         // We only need to track future references to this iaload if refcount  > 1
         if (node->getReferenceCount() > 1)
            {
            uint32_t *temp ;
            TR_Pair<TR::Node, int32_t> *newEntry = new (self()->trStackMemory()) TR_Pair<TR::Node, int32_t> (node, (int32_t *)1);
            _ialoadUnneeded.add(newEntry);
            }
         node->setUnneededIALoad (true);
         }
      }

   if (node->isUnneededIALoad())
      {
      if (parent->getOpCodeValue() == TR::ifacmpne || parent->getOpCodeValue() == TR::ificmpeq || parent->getOpCodeValue() == TR::ificmpne || parent->getOpCodeValue() == TR::ifacmpeq)
         {
         if (!parent->isNopableInlineGuard() || !self()->getSupportsVirtualGuardNOPing())
            {
            node->setUnneededIALoad(false);
            }
         else
            {
            TR_VirtualGuard * virtualGuard = self()->comp()->findVirtualGuardInfo(parent);
            if (!parent->isHCRGuard() && !parent->isOSRGuard() && !self()->comp()->performVirtualGuardNOPing() &&
                self()->comp()->isVirtualGuardNOPingRequired(virtualGuard) &&
                virtualGuard->canBeRemoved())
               {
               node->setUnneededIALoad(false);
               }
            }
         }
      else if (parent->getOpCode().isNullCheck())
         {
         traceMsg(self()->comp(), "parent %p appears to be a nullcheck over node: %p\n", parent, node);
         }
      else if (node->getOpCodeValue() == TR::aloadi && !(node->isClassPointerConstant() || node->isMethodPointerConstant()) || parent->getOpCode().isNullCheck())
         {
         node->setUnneededIALoad(false);
         }
      else if ((parent->getOpCodeValue() != TR::ifacmpne || tt->getNode()->getOpCodeValue() != TR::ifacmpne))
         {
         node->setUnneededIALoad(false);
         }
      }
   }

void
OMR::Z::CodeGenerator::lowerTreesPropagateBlockToNode(TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (!node->getType().isBCD())
#endif
      {
      OMR::CodeGenerator::lowerTreesPropagateBlockToNode(node);
      }

   }

void
OMR::Z::CodeGenerator::lowerTreeIfNeeded(
      TR::Node *node,
      int32_t childNumberOfNode,
      TR::Node *parent,
      TR::TreeTop *tt)
   {

   OMR::CodeGenerator::lowerTreeIfNeeded(node, childNumberOfNode, parent, tt);

   // Z
   if (self()->comp()->target().cpu.isZ() &&
       (node->getOpCode().isLoadVar() || node->getOpCode().isStore()) &&
       node->getOpCode().isIndirect())
      {
      TR::Node* add1 = NULL;
      TR::Node* add2 = NULL;
      TR::Node* sub = NULL;
      TR::Node* const1 = NULL;
      TR::Node* const2 = NULL;
      TR::Node* base = NULL;
      TR::Node* index = NULL;
      TR::ILOpCodes addOp, subOp, constOp;

      if (self()->comp()->target().is64Bit())
         {
         addOp = TR::aladd;
         subOp = TR::lsub;
         constOp = TR::lconst;
         }
      else
         {
         addOp = TR::aiadd;
         subOp = TR::isub;
         constOp = TR::iconst;
         }

      if (node->getFirstChild()->getOpCodeValue() == addOp)
         add1 = node->getFirstChild();
      if (add1 && add1->getFirstChild()->getOpCodeValue() == addOp)
         {
         add2 = add1->getFirstChild();
         base = add2->getFirstChild();
         }
      if (add1 && add1->getSecondChild()->getOpCodeValue() == subOp)
         sub = add1->getSecondChild();
      if (add2 && add2->getSecondChild()->getOpCode().isLoadConst())
         {
         const1 = add2->getSecondChild();
         index = add1->getSecondChild();
         }
      if (sub && sub->getSecondChild()->getOpCode().isLoadConst())
         {
         const2 = sub->getSecondChild();
         index = sub->getFirstChild();
         }

      if (add1 && add2 && const1 == NULL && const2 == NULL)
         {
         if (add2->getFirstChild()->getOpCodeValue() == addOp)
            {
            index = add2->getSecondChild();
            add2 = add2->getFirstChild();
            base = add2->getFirstChild();
            if (add2->getSecondChild()->getOpCode().isLoadConst())
               const2 = add2->getSecondChild();
            if (const2 && add1->getSecondChild()->getOpCode().isLoadConst())
               const1 = add1->getSecondChild();
            }
         }

      if (add1 && add2 && const1 && performTransformation(self()->comp(), "%sBase/index/displacement form addressing prep for node [%p]\n", OPT_DETAILS, node))
         {
         // traceMsg(comp(), "&&& Found pattern root=%llx add1=%llx add2=%llx const1=%llx sub=%llx const2=%llx\n", node, add1, add2, const1, sub, const2);

         intptr_t offset = 0;
         if (self()->comp()->target().is64Bit())
            {
            offset = const1->getLongInt();
            if (const2)
              if (sub)
                 offset -= const2->getLongInt();
              else
                 offset += const2->getLongInt();
            }
         else
            {
            offset = const1->getInt();
            if (const2)
              if (sub)
                 offset -= const2->getInt();
              else
                 offset += const2->getInt();
            }

         TR::Node* newAdd2 = TR::Node::create(addOp, 2, base, index);
         TR::Node* newConst = TR::Node::create(constOp, 0);
         if (self()->comp()->target().is64Bit())
            newConst->setLongInt(offset);
         else
            newConst->setInt(offset);
         TR::Node* newAdd1 = TR::Node::create(addOp, 2, newAdd2, newConst);
         node->setAndIncChild(0, newAdd1);
         add1->recursivelyDecReferenceCount();
         }
      }

   }

bool OMR::Z::CodeGenerator::supportsInliningOfIsInstance()
   {
   return !self()->comp()->getOption(TR_DisableInlineIsInstance);
   }

bool OMR::Z::CodeGenerator::canTransformUnsafeCopyToArrayCopy()
   {
   static char *disableTran = feGetEnv("TR_DISABLEUnsafeCopy");

   return (!disableTran && !self()->comp()->getCurrentMethod()->isJNINative() && !self()->comp()->getOption(TR_DisableArrayCopyOpts));
   }

bool OMR::Z::CodeGenerator::supportsDirectIntegralLoadStoresFromLiteralPool()
   {
   return self()->comp()->target().cpu.getSupportsArch(TR::CPU::z10);
   }

OMR::Z::CodeGenerator::CodeGenerator()
   : OMR::CodeGenerator(),
     _extentOfLitPool(-1),
     _recompPatchInsnList(getTypedAllocator<TR::Instruction*>(self()->comp()->allocator())),
     _constantHash(self()->comp()->allocator()),
     _constantHashCur(_constantHash),
     _constantList(getTypedAllocator<TR::S390ConstantDataSnippet*>(self()->comp()->allocator())),
     _writableList(getTypedAllocator<TR::S390WritableDataSnippet*>(self()->comp()->allocator())),
     _transientLongRegisters(self()->trMemory()),
     _snippetDataList(getTypedAllocator<TR::S390ConstantDataSnippet*>(self()->comp()->allocator())),
     _outOfLineCodeSectionList(getTypedAllocator<TR_S390OutOfLineCodeSection*>(self()->comp()->allocator())),
     _returnTypeInfoInstruction(NULL),
     _interfaceSnippetToPICsListHashTab(NULL),
     _currentCheckNode(NULL),
     _currentBCDCHKHandlerLabel(NULL),
     _nodesToBeEvaluatedInRegPairs(self()->comp()->allocator()),
     _ccInstruction(NULL),
     _previouslyAssignedTo(self()->comp()->allocator("LocalRA")),
     _firstTimeLiveOOLRegisterList(NULL),
     _methodBegin(NULL),
     _methodEnd(NULL),
     _afterRA(false)
   {
   TR::Compilation *comp = self()->comp();
   _cgFlags = 0;

   // Initialize Linkage for Code Generator
   self()->initializeLinkage();

   _unlatchedRegisterList = (TR::RealRegister**)self()->trMemory()->allocateHeapMemory(sizeof(TR::RealRegister*)*(TR::RealRegister::NumRegisters));
   _unlatchedRegisterList[0] = 0; // mark that list is empty

   bool enableBranchPreload = comp->getOption(TR_EnableBranchPreload);
   bool disableBranchPreload = comp->getOption(TR_DisableBranchPreload);

   if (enableBranchPreload || (!disableBranchPreload && comp->isOptServer() && self()->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12)))
      self()->setEnableBranchPreload();
   else
      self()->setDisableBranchPreload();

   static bool bpp = (feGetEnv("TR_BPRP")!=NULL);

   if ((enableBranchPreload && bpp) || (bpp && !disableBranchPreload && comp->isOptServer() && self()->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12)))
      self()->setEnableBranchPreloadForCalls();
   else
      self()->setDisableBranchPreloadForCalls();

   if (self()->supportsBranchPreloadForCalls())
      {
      _callsForPreloadList = new (self()->trHeapMemory()) TR::list<TR_BranchPreloadCallData*>(getTypedAllocator<TR_BranchPreloadCallData*>(comp->allocator()));
      }

   self()->setOnDemandLiteralPoolRun(true);
   self()->setGlobalStaticBaseRegisterOn(false);

   self()->setGlobalPrivateStaticBaseRegisterOn(false);

   self()->setMultiplyIsDestructive();

   self()->setSupportsSelect();

   self()->setIsOutOfLineHotPath(false);

   self()->setUsesRegisterPairsForLongs();
   self()->setSupportsDivCheck();
   self()->setSupportsLoweringConstIDiv();
   self()->setSupportsTestUnderMask();

   // Initialize to be 8 bytes for bodyInfo / methodInfo
   self()->setPreJitMethodEntrySize(8);

   // Support divided by power of 2 logic in ldivSimplifier
   self()->setSupportsLoweringConstLDivPower2();

   //enable LM/STM for volatile longs in 32 bit
   self()->setSupportsInlinedAtomicLongVolatiles();

   self()->setSupportsConstantOffsetInAddressing();

   static char * noVGNOP = feGetEnv("TR_NOVGNOP");
   if (noVGNOP == NULL)
      {
      self()->setSupportsVirtualGuardNOPing();
      }
   if (!comp->getOption(TR_DisableArraySetOpts))
      {
      self()->setSupportsArraySet();
      }
   self()->setSupportsArrayCmp();
   self()->setSupportsArrayCmpSign();
   if (!comp->compileRelocatableCode())
      {
      self()->setSupportsArrayTranslateTRxx();
      }

   self()->setSupportsTestCharComparisonControl();  // TRXX instructions on Danu have mask to disable test char comparison.
   self()->setSupportsReverseLoadAndStore();
   self()->setSupportsSearchCharString(); // CISC Transformation into SRSTU loop - only on z9.
   self()->setSupportsTranslateAndTestCharString(); // CISC Transformation into TRTE loop - only on z6.

   if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      self()->setSupportsTranslateAndTestCharString();

      if (!comp->getOption(TR_DisableTraps) && TR::Compiler->vm.hasResumableTrapHandler(comp))
         {
         self()->setHasResumableTrapHandler();
         }
      }
   else
      {
      comp->setOption(TR_DisableCompareAndBranchInstruction);

      // No trap instructions available for z10 and below.
      // Set disable traps so that the optimizations and codegen can avoid generating
      // trap-specific nodes or instructions.
      comp->setOption(TR_DisableTraps);
      }

   if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      self()->setSupportsAtomicLoadAndAdd();
      }
   else
      {
      // Max min optimization uses the maxMinHelper evaluator which
      // requires conditional loads that are not supported below z196
      comp->setOption(TR_DisableMaxMinOptimization);
      }

   if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12))
      {
      self()->setSupportsZonedDFPConversions();
      if (self()->comp()->target().cpu.getSupportsTransactionalMemoryFacility() && !comp->getOption(TR_DisableTM))
         self()->setSupportsTM();
      }

   if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z13) && !comp->getOption(TR_DisableArch11PackedToDFP))
      self()->setSupportsFastPackedDFPConversions();

   if (!self()->comp()->target().cpu.getSupportsArch(TR::CPU::z14))
      {
      comp->setOption(TR_DisableVectorBCD);
      }

   // Be pessimistic until we can prove we don't exit after doing code-generation
   self()->setExitPointsInMethod(true);

#ifdef DEBUG
   // Clear internal counters for internal profiling of
   // register allocation
   self()->clearTotalSpills();
   self()->clearTotalRegisterXfers();
   self()->clearTotalRegisterMoves();
#endif

   // Set up vector register support for machine after zEC12.
   // This should also happen before prepareForGRA
   if (comp->getOption(TR_DisableSIMD))
      {
      comp->setOption(TR_DisableAutoSIMD);
      comp->setOption(TR_DisableSIMDArrayCompare);
      comp->setOption(TR_DisableSIMDArrayTranslate);
      comp->setOption(TR_DisableSIMDUTF16BEEncoder);
      comp->setOption(TR_DisableSIMDUTF16LEEncoder);
      comp->setOption(TR_DisableSIMDStringHashCode);
      comp->setOption(TR_DisableVectorRegGRA);
      }

   self()->setSupportsRecompilation();

   // This enables the tactical GRA
   self()->setSupportsGlRegDeps();

   self()->addSupportedLiveRegisterKind(TR_GPR);
   self()->addSupportedLiveRegisterKind(TR_FPR);
   self()->addSupportedLiveRegisterKind(TR_VRF);

   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_GPR);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_FPR);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_VRF);

   self()->setSupportsPrimitiveArrayCopy();
   self()->setSupportsReferenceArrayCopy();

   self()->setSupportsPartialInlineOfMethodHooks();

   self()->setSupportsInliningOfTypeCoersionMethods();

   self()->setPerformsChecksExplicitly();

   _numberBytesReadInaccessible = 4096;
   _numberBytesWriteInaccessible = 4096;

   // zLinux sTR requires this as well, otherwise cp00f054.C testcase fails in TR_FPStoreReloadElimination.
   self()->setSupportsJavaFloatSemantics();

   _localF2ISpill = NULL;
   _localD2LSpill = NULL;

   _nextAvailableBlockIndex = -1;
   _currentBlockIndex = -1;

   if (comp->getOption(TR_TraceRA))
      {
      self()->setGPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR::RealRegister::FirstGPR, TR::RealRegister::LastAssignableGPR));
      self()->setFPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR::RealRegister::FirstFPR, TR::RealRegister::LastFPR));
      self()->setVRFRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR::RealRegister::FirstVRF, TR::RealRegister::LastVRF));
      }

   if (!comp->getOption(TR_DisableTLHPrefetch) && !comp->compileRelocatableCode())
      {
      self()->setEnableTLHPrefetching();
      }

   _reusableTempSlot = NULL;
   self()->freeReusableTempSlot();

   self()->setSupportsProfiledInlining();

   self()->getS390Linkage()->setParameterLinkageRegisterIndex(comp->getJittedMethodSymbol());

   self()->getS390Linkage()->initS390RealRegisterLinkage();
   self()->setAccessStaticsIndirectly(true);
   }

bool
OMR::Z::CodeGenerator::getSupportsBitPermute()
   {
   return true;
   }

bool OMR::Z::CodeGenerator::prepareForGRA()
   {
   bool enableVectorGRA = self()->getSupportsVectorRegisters() && !self()->comp()->getOption(TR_DisableVectorRegGRA);

   if (!_globalRegisterTable)
      {
      self()->machine()->initializeGlobalRegisterTable();
      self()->setGlobalRegisterTable(self()->machine()->getGlobalRegisterTable());
      self()->setLastGlobalGPR(self()->machine()->getLastGlobalGPRRegisterNumber());
      self()->setLast8BitGlobalGPR(self()->machine()->getLast8BitGlobalGPRRegisterNumber());
      self()->setFirstGlobalFPR(self()->machine()->getFirstGlobalFPRRegisterNumber());
      self()->setLastGlobalFPR(self()->machine()->getLastGlobalFPRRegisterNumber());

      self()->setFirstGlobalVRF(self()->machine()->getFirstGlobalVRFRegisterNumber());
      self()->setLastGlobalVRF(self()->machine()->getLastGlobalVRFRegisterNumber());

      if (enableVectorGRA)
         {
         static char * traceVecGRN = feGetEnv("TR_traceVectorGRA");
         if (traceVecGRN)
            {
            printf("Java func: %s func: %s\n", self()->comp()->getCurrentMethod()->nameChars(), __FUNCTION__);
            printf("ff  %d\t", self()->machine()->getFirstGlobalFPRRegisterNumber());
            printf("lf  %d\t", self()->machine()->getLastGlobalFPRRegisterNumber());
            printf("fof %d\t", self()->machine()->getFirstOverlappedGlobalFPRRegisterNumber());
            printf("lof %d\t", self()->machine()->getLastOverlappedGlobalFPRRegisterNumber());
            printf("fv  %d\t", self()->machine()->getFirstGlobalVRFRegisterNumber());
            printf("lv  %d\t", self()->machine()->getLastGlobalVRFRegisterNumber());
            printf("fov %d\t", self()->machine()->getFirstOverlappedGlobalVRFRegisterNumber());
            printf("lov %d\t", self()->machine()->getLastOverlappedGlobalVRFRegisterNumber());
            printf("\n--------------------\n");
            }

         self()->setFirstOverlappedGlobalFPR(self()->machine()->getFirstOverlappedGlobalFPRRegisterNumber());
         self()->setLastOverlappedGlobalFPR (self()->machine()->getLastOverlappedGlobalFPRRegisterNumber());
         self()->setFirstOverlappedGlobalVRF(self()->machine()->getFirstOverlappedGlobalVRFRegisterNumber());
         self()->setLastOverlappedGlobalVRF (self()->machine()->getLastOverlappedGlobalVRFRegisterNumber());
         self()->setOverlapOffsetBetweenAliasedGRNs(self()->getFirstOverlappedGlobalVRF() - self()->getFirstOverlappedGlobalFPR());
         }

      self()->setSupportsGlRegDepOnFirstBlock();
      self()->setConsiderAllAutosAsTacticalGlobalRegisterCandidates();

      static char * disableLongGRAOn31Bit = feGetEnv("TR390LONGGRA31BIT");
      // Only enable Long Globals on 64bit pltfrms
      if (disableLongGRAOn31Bit)
         {
         self()->setDisableLongGRA();
         }

      // Initialize _globalGPRsPreservedAcrossCalls and _globalFPRsPreservedAcrossCalls
      // We call init here because getNumberOfGlobal[FG]PRs() is initialized during the call to initialize() above.
      //
      _globalGPRsPreservedAcrossCalls.init(NUM_S390_GPR + NUM_S390_FPR, self()->trMemory());
      _globalFPRsPreservedAcrossCalls.init(NUM_S390_GPR + NUM_S390_FPR, self()->trMemory());

      TR_GlobalRegisterNumber grn;
      for (grn = self()->getFirstGlobalGPR(); grn <= self()->getLastGlobalGPR(); grn++)
         {
         uint32_t globalReg = self()->getGlobalRegister(grn);
         if (globalReg != -1 && self()->getS390Linkage()->getPreserved((TR::RealRegister::RegNum) globalReg))
            {
            _globalGPRsPreservedAcrossCalls.set(grn);
            }
         }
      for (grn = self()->getFirstGlobalFPR(); grn <= self()->getLastGlobalFPR(); grn++)
         {
         uint32_t globalReg = self()->getGlobalRegister(grn);
         if (globalReg != -1 && self()->getS390Linkage()->getPreserved((TR::RealRegister::RegNum) globalReg))
            {
            _globalFPRsPreservedAcrossCalls.set(grn);
            }
         }

      TR::Linkage *linkage = self()->getS390Linkage();
      if (!self()->comp()->getOption(TR_DisableRegisterPressureSimulation))
         {
         for (int32_t i = 0; i < TR_numSpillKinds; i++)
            _globalRegisterBitVectors[i].init(self()->getNumberOfGlobalRegisters(), self()->trMemory());

         for (grn=0; grn < self()->getNumberOfGlobalRegisters(); grn++)
            {
            TR::RealRegister::RegNum reg = (TR::RealRegister::RegNum)self()->getGlobalRegister(grn);
            TR_ASSERT(reg != -1, "Register pressure simulator doesn't support gaps in the global register table; reg %d must be removed", grn);
            if (self()->getFirstGlobalGPR() <= grn && grn <= self()->getLastGlobalGPR())
               {
               _globalRegisterBitVectors[ TR_gprSpill ].set(grn);
               }
            else if (self()->getFirstGlobalFPR() <= grn && grn <= self()->getLastGlobalFPR())
               {
               _globalRegisterBitVectors[TR_fprSpill].set(grn);
               }
            else if (self()->getFirstGlobalVRF() <= grn && grn <= self()->getLastGlobalVRF())
               {
               _globalRegisterBitVectors[TR_vrfSpill].set(grn);
               }

            if (!linkage->getPreserved(reg))
               _globalRegisterBitVectors[ TR_volatileSpill ].set(grn);
            if (linkage->getIntegerArgument(reg) || linkage->getFloatArgument(reg))
               {
               _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);
               }
            if (reg == linkage->getMethodMetaDataRegister())
               _globalRegisterBitVectors[ TR_vmThreadSpill ].set(grn);
            if (reg == linkage->getLitPoolRegister())
               _globalRegisterBitVectors[ TR_litPoolSpill ].set(grn);
            if (reg == linkage->getStaticBaseRegister() && self()->isGlobalStaticBaseRegisterOn())
               _globalRegisterBitVectors[ TR_staticBaseSpill ].set(grn);
            if (reg == TR::RealRegister::GPR0)
               _globalRegisterBitVectors[ TR_gpr0Spill ].set(grn);
            if (reg == TR::RealRegister::GPR1)
               _globalRegisterBitVectors[ TR_gpr1Spill ].set(grn);
            if (reg == TR::RealRegister::GPR2)
               _globalRegisterBitVectors[ TR_gpr2Spill ].set(grn);
            if (reg == TR::RealRegister::GPR3)
               _globalRegisterBitVectors[ TR_gpr3Spill ].set(grn);
            if (reg == TR::RealRegister::GPR4)
               _globalRegisterBitVectors[ TR_gpr4Spill ].set(grn);
            if (reg == TR::RealRegister::GPR5)
               _globalRegisterBitVectors[ TR_gpr5Spill ].set(grn);
            if (reg == TR::RealRegister::GPR6)
               _globalRegisterBitVectors[ TR_gpr6Spill ].set(grn);
            if (reg == TR::RealRegister::GPR7)
               _globalRegisterBitVectors[ TR_gpr7Spill ].set(grn);
            if (reg == TR::RealRegister::GPR8)
               _globalRegisterBitVectors[ TR_gpr8Spill ].set(grn);
            if (reg == TR::RealRegister::GPR9)
               _globalRegisterBitVectors[ TR_gpr9Spill ].set(grn);
            if (reg == TR::RealRegister::GPR10)
               _globalRegisterBitVectors[ TR_gpr10Spill ].set(grn);
            if (reg == TR::RealRegister::GPR11)
               _globalRegisterBitVectors[ TR_gpr11Spill ].set(grn);
            if (reg == TR::RealRegister::GPR12)
               _globalRegisterBitVectors[ TR_gpr12Spill ].set(grn);
            if (reg == TR::RealRegister::GPR13)
               _globalRegisterBitVectors[ TR_gpr13Spill ].set(grn);
            if (reg == TR::RealRegister::GPR14)
               _globalRegisterBitVectors[ TR_gpr14Spill ].set(grn);
            if (reg == TR::RealRegister::GPR15)
               _globalRegisterBitVectors[ TR_gpr15Spill ].set(grn);
            }

         if ((grn = self()->machine()->getGlobalCAARegisterNumber()) != -1)
            _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);

         if ((grn = self()->machine()->getGlobalEnvironmentRegisterNumber()) != -1)
            _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);

         if ((grn = self()->machine()->getGlobalParentDSARegisterNumber()) != -1)
            _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);

         if ((grn = self()->machine()->getGlobalReturnAddressRegisterNumber()) != -1)
             _globalRegisterBitVectors[ TR_linkageSpill ].set(grn);

          if ((grn = self()->machine()->getGlobalEntryPointRegisterNumber()) != -1)
             _globalRegisterBitVectors[ TR_linkageSpill ].set(grn);

         }

      return OMR::CodeGenerator::prepareForGRA();
      }
   else
      {
      return true;
      }
   }

TR::Linkage * OMR::Z::CodeGenerator::getS390Linkage() {return (self()->getLinkage());}

TR::RealRegister * OMR::Z::CodeGenerator::getStackPointerRealRegister(TR::Symbol *symbol)
   {
   TR::Linkage *linkage = self()->getS390Linkage();
   TR::RealRegister *spReg = linkage->getStackPointerRealRegister();
   return spReg;
   }
TR::RealRegister * OMR::Z::CodeGenerator::getEntryPointRealRegister()
   {return self()->getS390Linkage()->getEntryPointRealRegister();}
TR::RealRegister * OMR::Z::CodeGenerator::getReturnAddressRealRegister()
   {return self()->getS390Linkage()->getReturnAddressRealRegister();}
TR::RealRegister * OMR::Z::CodeGenerator::getLitPoolRealRegister()
   {return self()->getS390Linkage()->getLitPoolRealRegister();}

TR::RealRegister::RegNum OMR::Z::CodeGenerator::getEntryPointRegister()
   {return self()->getS390Linkage()->getEntryPointRegister();}
TR::RealRegister::RegNum OMR::Z::CodeGenerator::getReturnAddressRegister()
   {return self()->getS390Linkage()->getReturnAddressRegister();}

TR::RealRegister * OMR::Z::CodeGenerator::getMethodMetaDataRealRegister()
   {
   return self()->getS390Linkage()->getMethodMetaDataRealRegister();
   }

bool
OMR::Z::CodeGenerator::internalPointerSupportImplemented()
   {
   return true;
   }

bool
OMR::Z::CodeGenerator::mulDecompositionCostIsJustified(int32_t numOfOperations, char bitPosition[], char operationType[], int64_t value)
   {
   bool trace = self()->comp()->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition);

   if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      int32_t numCycles = 0;
      numCycles = numOfOperations+1;
      if (value & (int64_t) CONSTANT64(0x0000000000000001))
         {
         numCycles = numCycles-1;
         }
      if (trace)
         if (numCycles <= 3)
            traceMsg(self()->comp(), "MulDecomp cost is justified\n");
         else
            traceMsg(self()->comp(), "MulDecomp cost is too high. numCycle=%i(max:3)\n", numCycles);
      return numCycles <= 3;
      }
   else if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      int32_t numCycles = 0;
      numCycles = numOfOperations+1;
      if (value & (int64_t) CONSTANT64(0x0000000000000001))
         {
         numCycles = numCycles-1;
         }
      if (trace)
         if (numCycles <= 9)
            traceMsg(self()->comp(), "MulDecomp cost is justified\n");
         else
            traceMsg(self()->comp(), "MulDecomp cost is too high. numCycle=%i(max:10)\n", numCycles);
      return numCycles <= 9;
      }
   else
      {
      if (value > MAX_IMMEDIATE_VAL || value < MIN_IMMEDIATE_VAL)
         {
         // If more than 16 bits, then justify the shift / subtract
         return OMR::CodeGenerator::mulDecompositionCostIsJustified(numOfOperations, bitPosition, operationType, value);
         }

      return false;
      }
   }

/**
 * BCD types and aggregates of some eizes are not going to end up in machine registers so do not
 * consider them for global register allocation
 */
bool OMR::Z::CodeGenerator::considerAggregateSizeForGRA(int32_t size)
   {
   switch (size)
      {
      case 1:
      case 2:
      case 4:
      case 8:  return true;
      default: return false;
      }
   }

bool
OMR::Z::CodeGenerator::considerTypeForGRA(TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (node->getType().isBCD())
      return false;
#endif
   if (node->getDataType() == TR::Aggregate)
      return self()->considerAggregateSizeForGRA(0);
   else if (node->getSymbol()&& node->getSymbol()->isMethodMetaData())
      return !node->getSymbol()->getType().isFloatingPoint();
   else
      return true;
   }

bool
OMR::Z::CodeGenerator::considerTypeForGRA(TR::DataType dt)
   {
   if (
#ifdef J9_PROJECT_SPECIFIC
       dt.isBCD() ||
#endif
       dt == TR::Aggregate)
      return false;
   else
      return true;
   }

bool
OMR::Z::CodeGenerator::considerTypeForGRA(TR::SymbolReference *symRef)
   {
   if (symRef &&
       symRef->getSymbol())
      {
#ifdef J9_PROJECT_SPECIFIC
      if (symRef->getSymbol()->getDataType().isBCD())
         return false;
      else
#endif
         if (symRef->getSymbol()->getDataType() == TR::Aggregate)
         return self()->considerAggregateSizeForGRA(symRef->getSymbol()->getSize());
      else
         return true;
      }
   else
      {
      return true;
      }
   }

void
OMR::Z::CodeGenerator::enableLiteralPoolRegisterForGRA ()
   {
   if (self()->comp()->getOption(TR_DisableRegisterPressureSimulation))
      {
      self()->machine()->releaseLiteralPoolRegister();
      static char * noGraFIX= feGetEnv("TR_NOGRAFIX");
      if (!noGraFIX)
         {
         _globalGPRsPreservedAcrossCalls.set(GLOBAL_REG_FOR_LITPOOL);
         }
      }

   }

bool
OMR::Z::CodeGenerator::canUseImmedInstruction( int64_t value )
   {
   if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
      return true;
   else
      return false;
   }

bool
OMR::Z::CodeGenerator::isAddMemoryUpdate(TR::Node * node, TR::Node * valueChild)
   {
   static char * disableASI = feGetEnv("TR_DISABLEASI");

   if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      if (!disableASI && self()->isMemoryUpdate(node) && valueChild->getSecondChild()->getOpCode().isLoadConst())
         {
         if (valueChild->getOpCodeValue() == TR::iadd || valueChild->getOpCodeValue() == TR::isub)
            {
            int32_t value = valueChild->getSecondChild()->getInt();

            if (value < (int32_t) 0x7F && value > (int32_t) 0xFFFFFF80)
               {
               return true;
               }
            }
         else if (self()->comp()->target().is64Bit() && (valueChild->getOpCodeValue() == TR::ladd || valueChild->getOpCodeValue() == TR::lsub))
            {
            int64_t value = valueChild->getSecondChild()->getLongInt();

            if (value < (int64_t) CONSTANT64(0x7F) && value > (int64_t) CONSTANT64(0xFFFFFFFFFFFFFF80))
               {
               return true;
               }
            }
         }
      }

   return false;
   }

bool
OMR::Z::CodeGenerator::isUsing32BitEvaluator(TR::Node *node)
   {
   if ((node->getOpCodeValue() == TR::sadd || node->getOpCodeValue() == TR::ssub) &&
       (node->getFirstChild()->getOpCodeValue() != TR::i2s && node->getSecondChild()->getOpCodeValue() != TR::i2s))
      return true;

   if ((node->getOpCodeValue() == TR::badd || node->getOpCodeValue() == TR::bsub) &&
       (node->getFirstChild()->getOpCodeValue() != TR::i2b && node->getSecondChild()->getOpCodeValue() != TR::i2b))
      return true;

   if (node->getOpCodeValue() == TR::bRegLoad || node->getOpCodeValue() == TR::sRegLoad)
      return true;

   return false;
   }

TR::Instruction *
OMR::Z::CodeGenerator::generateNop(TR::Node *n, TR::Instruction *preced, TR_NOPKind nopKind)
   {
   return new (self()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, n, preced, self());
   }

TR::Instruction*
OMR::Z::CodeGenerator::insertPad(TR::Node* node, TR::Instruction* cursor, uint32_t size, bool prependCursor)
   {
   if (size != 0)
      {
      if (prependCursor)
         {
         cursor = cursor->getPrev();
         }

      switch (size)
         {
         case 2:
            {
            cursor = new (self()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, node, cursor, self());
            }
         break;

         case 4:
            {
            cursor = new (self()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 4, node, cursor, self());
            }
         break;

         case 6:
            {
            cursor = new (self()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 4, node, cursor, self());
            cursor = new (self()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, node, cursor, self());
            }
         break;

         default:
            {
            TR_ASSERT(false, "Unexpected pad size %d\n", size);
            }
         break;
         }
      }

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::beginInstructionSelection()
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::beginInstructionSelection()
   {
   TR::Node * startNode = self()->comp()->getStartTree()->getNode();

   self()->setCurrentBlockIndex(startNode->getBlock()->getNumber());

   if (self()->comp()->getJittedMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      TR::Instruction * cursor = NULL;

      if (self()->comp()->getJittedMethodSymbol()->isJNI() &&
          !self()->comp()->getOption(TR_MimicInterpreterFrameShape))
         {
         TR::ResolvedMethodSymbol * methodSymbol = self()->comp()->getJittedMethodSymbol();
         intptr_t jniMethodTargetAddress = (intptr_t)methodSymbol->getResolvedMethod()->startAddressForJNIMethod(self()->comp());

         cursor = new (self()->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, startNode, UPPER_4_BYTES(jniMethodTargetAddress), cursor, self());

         if (self()->comp()->target().is64Bit())
            {
            cursor = new (self()->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, startNode, LOWER_4_BYTES(jniMethodTargetAddress), cursor, self());
            }
         }

      _returnTypeInfoInstruction = new (self()->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, startNode, 0, NULL, cursor, self());
      }

   generateS390PseudoInstruction(self(), TR::InstOpCode::PROC, startNode);
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::endInstructionSelection
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::endInstructionSelection()
   {
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::doInstructionSelection
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::doInstructionSelection()
   {
   OMR::CodeGenerator::doInstructionSelection();
   if (_returnTypeInfoInstruction != NULL)
      {
      _returnTypeInfoInstruction->setSourceImmediate(static_cast<uint32_t>(self()->comp()->getReturnInfo()));
      }
   }

bool
OMR::Z::CodeGenerator::hasWarmCallsBeforeReturn()
   {
   TR::TreeTop      *pTree, *exitTree;
   TR::Block        *block;
   bool             hasSeenCall = false;
   for (pTree=self()->comp()->getStartTree(); pTree!=NULL; pTree=exitTree->getNextTreeTop())
      {
      block = pTree->getNode()->getBlock();
      exitTree = block->getExit();
      do
         {
         if (TR::CodeGenerator::treeContainsCall(pTree) && !block->isCold())
            {
            hasSeenCall = true;
            break;
            }
         pTree = pTree->getNextTreeTop();
         }
      while (pTree != exitTree);

      if (block == _hottestReturn._returnBlock)
         {
         return hasSeenCall;
         }

      }

   TR_ASSERT(0, "Should've seen return block.\n");
   return false;
   }

void
OMR::Z::CodeGenerator::createBranchPreloadCallData(TR::LabelSymbol *callLabel, TR::SymbolReference * callSymRef, TR::Instruction * instr)
      {
      TR_BranchPreloadCallData * data = new (self()->trHeapMemory()) TR_BranchPreloadCallData(instr, callLabel, self()->comp()->getCurrentBlock(), callSymRef, self()->comp()->getCurrentBlock()->getFrequency());
      _callsForPreloadList->push_front(data);
      }

void
OMR::Z::CodeGenerator::insertInstructionPrefetches()
   {
   if (self()->supportsBranchPreload())
      {
      if (_hottestReturn._frequency > 6)
         {
         TR::Block * block = _hottestReturn._returnBlock;
         int32_t n = block->getNumber();
         TR::Block * ext = block->startOfExtendedBlock();
         block = block->startOfExtendedBlock();

         TR::Instruction * cursor = block->getFirstInstruction();
         //iterate how many instr
         TR::Instruction * first = _hottestReturn._returnInstr;
         TR::Instruction * real = NULL;
         int32_t count = 0;

         if ((self()->comp()->getOptLevel() != warm) || !self()->hasWarmCallsBeforeReturn())
            {
            _hottestReturn._frequency = -1;
            return;
            }
         do
            {
            TR::InstOpCode op = first->getOpCode();
            if (!op.isAdmin())
               {
               real = first;
               if (op.isLabel() || op.isCall())
                  {
                  break;
                  }
               count++;
               }
            first = first->getPrev();
            }
         while (first != cursor);

         if (count <= 2)
            {
            _hottestReturn._insertBPPInEpilogue = true;
            return;
            }

         // if we encountered a call, insert after the call or after the label that follows the call instr.
         if (first != cursor)
            {
            cursor  = real;
            }
         // if we didn't encounter a call or label, but there were real instructions, insert before a real instruction
         else if (real != NULL)
            {
            //if the real instruction isn't the first instruction in the block
            if (real != cursor)
               cursor = real->getPrev();
            else
               cursor = real;

            }
         //attach right before RET instruction then do it in epilogue
         else
            {
            _hottestReturn._insertBPPInEpilogue = true;
            return;
            }

         TR::Instruction * second = cursor->getNext();
         TR::Node * node = cursor->getNode();
         _hottestReturn._returnLabel = generateLabelSymbol(self());

         TR::Register * tempReg = self()->allocateRegister();
         TR::RealRegister * spReg =  self()->getS390Linkage()->getRealRegister(self()->getS390Linkage()->getStackPointerRegister());
         int32_t frameSize = self()->getFrameSizeInBytes();

         TR::MemoryReference * tempMR = generateS390MemoryReference(spReg, frameSize, self());
         tempMR->setCreatedDuringInstructionSelection();
         cursor = generateRXInstruction(self(), TR::InstOpCode::getExtendedLoadOpCode(), node,
               tempReg,
               tempMR, cursor);

         tempMR = generateS390MemoryReference(tempReg, 0, self());
         cursor = generateS390BranchPredictionPreloadInstruction(self(), TR::InstOpCode::BPP, node, _hottestReturn._returnLabel, (int8_t) 0x6, tempMR, cursor);
         cursor->setNext(second);
         self()->stopUsingRegister(tempReg);
         }
      }

   if (self()->supportsBranchPreloadForCalls())
      {
      TR_BranchPreloadCallData * data = NULL;
      for(auto iterator = self()->getCallsForPreloadList()->begin(); iterator != self()->getCallsForPreloadList()->end(); ++iterator)
        self()->insertInstructionPrefetchesForCalls(*iterator);
      }
   }

void
OMR::Z::CodeGenerator::insertInstructionPrefetchesForCalls(TR_BranchPreloadCallData * data)
   {
   static bool bpp = (feGetEnv("TR_BPRP")!=NULL);
   static int minCount = (feGetEnv("TR_Count")!=NULL) ? atoi(feGetEnv("TR_Count")) : 0;
   static int minFR = (feGetEnv("TR_minFR")!=NULL) ? atoi(feGetEnv("TR_minFR")) : 0;
   static int maxFR = (feGetEnv("TR_maxFR")!=NULL) ? atoi(feGetEnv("TR_maxFR")) : 0;
   static bool bppMethod = (feGetEnv("TR_BPRP_Method")!=NULL);
   static bool bppza = (feGetEnv("TR_BPRP_za")!=NULL);

   if (!bpp || data->_frequency <= 6)
      {
      return;
      }

   if (data->_frequency < minFR)
      {
      return;
      }
   if (maxFR != 0 && data->_frequency > maxFR)
      {
      return;
      }

   ////only preload those methods
   //test heuristic enabled with TR_BPRP_Method
   //TODO removed this one?
   if (bppMethod &&  self()->getDebug() && (strcmp(self()->getDebug()->getMethodName(data->_callSymRef), "java/lang/String.equals(Ljava/lang/Object;)Z") &&
         strcmp(self()->getDebug()->getMethodName(data->_callSymRef), "java/util/HashMap.hash(Ljava/lang/Object;)I") &&
         strcmp(self()->getDebug()->getMethodName(data->_callSymRef),"java/util/HashMap.addEntry(ILjava/lang/Object;Ljava/lang/Object;I)V")))
      return;

   /*
    * heuristic to use BPP when BPRP cannot reach, approximate to see if we can use BPRP
    *
    */
   bool canReachWithBPRP = false;

   intptr_t codeCacheBase = (intptr_t)(self()->getCodeCache()->getCodeBase());
   intptr_t codeCacheTop = (intptr_t)(self()->getCodeCache()->getCodeTop());

   intptr_t offset1 = (intptr_t) data->_callSymRef->getMethodAddress() - codeCacheBase;
   intptr_t offset2 = (intptr_t) data->_callSymRef->getMethodAddress() - codeCacheTop;
   if (offset2 >= MIN_24_RELOCATION_VAL && offset2 <= MAX_24_RELOCATION_VAL &&
         offset1 >= MIN_24_RELOCATION_VAL && offset1 <= MAX_24_RELOCATION_VAL)
      {
      canReachWithBPRP = true;
      }
   static bool bppCall = (feGetEnv("TR_BPRP_BPP")!=NULL);
   static bool bppOnly = (feGetEnv("TR_BPRP_BPPOnly")!=NULL);
   static bool bppMax = (feGetEnv("TR_BPRP_PushMax")!=NULL);
   static bool bppLoop = (feGetEnv("TR_BPRP_Loop")!=NULL);
   bool bppMaxPath = bppMax;

   TR::Block * block = data->_callBlock;
   int32_t n = block->getNumber();
   TR::Block * ext = block->startOfExtendedBlock();
   block = block->startOfExtendedBlock();

   /** heuristic to avoid Loops, preload BPP/BPRP outside of a loop, in a loop header or a block above
    * if found a loop, don't mix with push higher into other blocks for all possible edges
    * to enable TR_BPRP_Loop
    * **/
   TR_BlockStructure * blockStr = block->getStructureOf();
   TR::Instruction * first = data->_callInstr;
   bool isInLoop = (blockStr && block->getStructureOf()->getContainingLoop()) ? true : false;

   TR::Block * backupBlock  = NULL;
   if (isInLoop && bppLoop)
      {
      TR_RegionStructure *region = (TR_RegionStructure*)blockStr->getContainingLoop();
      block = region->getEntryBlock();

      uint32_t loopLength = 0;
      TR::Block *topBlock = NULL;
      int32_t current_fr = 0;
      bool insertBeforeLoop;
      for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
         {

         TR::Block *predBlock = toBlock((*e)->getFrom());
         if (!region->contains(predBlock->getStructureOf(), region->getParent()))
            {
            backupBlock = block;
            first = predBlock->getLastInstruction();
            block = predBlock->startOfExtendedBlock();
            bppMaxPath = false; //disable pushMax heuristic

            break;
            }

         }
      }

   static bool noCalls = (feGetEnv("TR_BPRP_NoCalls")!=NULL);
   static bool fix1 = (feGetEnv("TR_BPRP_FIX1")!=NULL);
   static bool fix2 = (feGetEnv("TR_BPRP_FIX2")!=NULL);
   if (self()->comp()->getOptLevel() != warm && self()->comp()->getOptLevel() != cold)
      {
      data->_frequency = -1;
      return;
      }

   TR::Instruction * cursor = block->getFirstInstruction();
   //iterate how many instr
   TR::Instruction * real = NULL;
   int32_t count = 0;
   if (block->getNumber() == 2)
      bppMaxPath = false;

   if (true)//!isInLoop || !bppMaxPath)
      {
      //TODO set max count and break?
      while (first != cursor)
         {
         TR::InstOpCode op = cursor->getOpCode();
         if (real == NULL  &&  (op.isLabel() || op.getOpCodeValue() == TR::InstOpCode::DEPEND))
            {
            real = cursor;
            }
         if (real != NULL && op.getOpCodeValue() == TR::InstOpCode::DEPEND && real->getOpCode().isLabel())
            {
            real = cursor;
            if ( cursor->getNext() && first != cursor->getNext())//op.getOpCodeValue() == TR::InstOpCode::BPRP || op.getOpCodeValue() == TR::InstOpCode::BPP)
               {
               op = cursor->getNext()->getOpCode();
               if (canReachWithBPRP && op.getOpCodeValue() == TR::InstOpCode::BPRP)
                  {
                  real = NULL;
                  }
               }

            break;
            }
         if (!op.isAdmin() && !op.isLabel())
            {
            count++;
            if (canReachWithBPRP && op.getOpCodeValue() == TR::InstOpCode::BPRP)
               {
               real = NULL;
               }
            break;
            }
         cursor = cursor->getNext();
         }

      cursor = real; //can be NULL?
      if (cursor == NULL && !bppMaxPath)
         {
         return;
         }
      }

   if (bppMaxPath)
      {

      uint32_t loopLength = 0;

      TR::Block *topBlock = NULL;
      int32_t current_fr = 0;
      bool useNonMax = false;
      for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
         {
         int32_t count2 = 0;
         TR::Block *predBlock = toBlock((*e)->getFrom());
         block = predBlock->startOfExtendedBlock();
         TR::Block *fr = toBlock((*e)->getTo());
         TR::Instruction * cursor = block->getFirstInstruction();
         //iterate how many instr
         TR::Instruction * first = predBlock->getLastInstruction();
         TR::Instruction * real = NULL;
         int32_t bppMaxPathCount = 0;
         TR::Block *toB = toBlock((*e)->getTo());
         while (first != cursor)
            {
            TR::InstOpCode op = cursor->getOpCode();
            if (real == NULL && (op.isLabel() || op.getOpCodeValue() == TR::InstOpCode::DEPEND))
               {
               real = cursor;
               }
            if (real != NULL && op.getOpCodeValue() == TR::InstOpCode::DEPEND && real->getOpCode().isLabel())
               {
               real = cursor;
               if ( cursor->getNext() && first != cursor)//op.getOpCodeValue() == TR::InstOpCode::BPRP || op.getOpCodeValue() == TR::InstOpCode::BPP)
                  {
                  op = cursor->getNext()->getOpCode();
                  if (canReachWithBPRP && op.getOpCodeValue() == TR::InstOpCode::BPRP)// || op.getOpCodeValue() == TR::InstOpCode::BPP)
                     {
                     //dont want more than one preload in a row
                     real = NULL;
                     }
                  }
               break;
               }

            if (!op.isAdmin() && !op.isLabel())
               {
               count2++;
               if (canReachWithBPRP && op.getOpCodeValue() == TR::InstOpCode::BPRP) // || op.getOpCodeValue() == TR::InstOpCode::BPP)
                  {
                  //dont want more than one preload in a row
                  real = NULL;
                  }
               break;
               }
            cursor = cursor->getNext();
            }
         cursor = real; //can be NULL?
         if (cursor == NULL)
            {
            continue;
            }
         TR::Instruction * second = cursor->getNext();
         TR::Node * node = cursor->getNode();

         if (bppCall && (!canReachWithBPRP || bppOnly) )
            {
            TR::MemoryReference * tempMR;
            TR::Register * tempReg = self()->allocateRegister();

            cursor =  new (self()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, node, tempReg, (data->_callSymRef)->getSymbol(),data->_callSymRef, cursor, self());

            tempMR = generateS390MemoryReference(tempReg, 0, self());
            cursor = generateS390BranchPredictionPreloadInstruction(self(), TR::InstOpCode::BPP, node, data->_callLabel, (int8_t) 0xD, tempMR, cursor);
            cursor->setNext(second);
            self()->stopUsingRegister(tempReg);

            }
         else
            {
            cursor = generateS390BranchPredictionRelativePreloadInstruction(self(), TR::InstOpCode::BPRP, node, data->_callLabel, (int8_t) 0xD, data->_callSymRef, cursor);
            cursor->setNext(second);
            }
         }
      }
   if (!bppMaxPath)
      {

      TR::Instruction * second = cursor->getNext();
      TR::Node * node = cursor->getNode();

      if (bppCall && (!canReachWithBPRP || bppOnly))
         {
         TR::Register * tempReg = self()->allocateRegister();

         cursor =  new (self()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, node, tempReg, (data->_callSymRef)->getSymbol(),  data->_callSymRef, cursor, self());
         TR::MemoryReference * tempMR = generateS390MemoryReference(tempReg, 0, self());
         tempMR->setCreatedDuringInstructionSelection();
         cursor = generateS390BranchPredictionPreloadInstruction(self(), TR::InstOpCode::BPP, node, data->_callLabel, (int8_t) 0xD, tempMR, cursor);
         cursor->setNext(second);
         self()->stopUsingRegister(tempReg);

         }
      else {
         cursor = generateS390BranchPredictionRelativePreloadInstruction(self(), TR::InstOpCode::BPRP, node, data->_callLabel, (int8_t) 0xD, data->_callSymRef, cursor);
         cursor->setNext(second);
      }
      }

   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::doRegisterAssignment
////////////////////////////////////////////////////////////////////////////////

/**
 * Do logic for determining if we want to make lit pool reg available
 */
bool
OMR::Z::CodeGenerator::isLitPoolFreeForAssignment()
   {
   bool litPoolRegIsFree = false;

   // Codegen's litPoolReg flag has ultimate veto
   // If lit on demand is working, we always free up
   // If no lit-on-demand, try to avoid locking up litpool reg anyways
   //
   if (self()->isLiteralPoolOnDemandOn() || (!self()->comp()->hasNativeCall() && self()->getFirstSnippet() == NULL))
      {
      litPoolRegIsFree = true;
      }
   else if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && !self()->anyLitPoolSnippets())
      {
      litPoolRegIsFree = true;
      }

   return litPoolRegIsFree;
   }

static TR::Instruction *skipInternalControlFlow(TR::Instruction *insertInstr)
  {
  int32_t nestingDepth=1;
  for(insertInstr=insertInstr->getPrev(); insertInstr!=NULL; insertInstr=insertInstr->getPrev())
    {
    // Track internal control flow on labels
    if (insertInstr->getOpCodeValue() == TR::InstOpCode::LABEL)
      {
      TR::S390LabelInstruction *li = toS390LabelInstruction(insertInstr);
      TR::LabelSymbol *ls=li->getLabelSymbol();
      if (ls->isStartInternalControlFlow())
        {
        nestingDepth--;
        if(nestingDepth==0)
          break;
        }
      if (ls->isEndInternalControlFlow())
        nestingDepth++;
      }
    } // for
  return insertInstr;
  }

/**
 * \brief
 * Determines if a value should be in a commoned constant node or not.
 *
 * \details
 *
 * A node with a large constant can be materialized and left as commoned nodes.
 * Smaller constants can be uncommoned so that they re-materialize every time when needed as a call
 * parameter. This query is platform specific as constant loading can be expensive on some platforms
 * but cheap on others, depending on their magnitude.
 */
bool OMR::Z::CodeGenerator::shouldValueBeInACommonedNode(int64_t value)
   {
   int64_t smallestPos = self()->getSmallestPosConstThatMustBeMaterialized();
   int64_t largestNeg = self()->getLargestNegConstThatMustBeMaterialized();

   return ((value >= smallestPos) || (value <= largestNeg));
   }

// This method it mostly for safely moving register spills outside of loops
// Within a loop after each instruction we must keep track of what happened to registers
// so that we know if it is safe to move a spill out of the loop
void OMR::Z::CodeGenerator::recordRegisterAssignment(TR::Register *assignedReg, TR::Register *virtualReg)
  {
  // Visit all real registers
  OMR::Z::Machine *mach=self()->machine();
  CS2::HashIndex hi;
  TR::RegisterPair *arp=assignedReg->getRegisterPair();

  if(arp)
    {
    TR::RegisterPair *vrp=virtualReg->getRegisterPair();
    self()->recordRegisterAssignment(arp->getLowOrder(),vrp->getLowOrder());
    self()->recordRegisterAssignment(arp->getHighOrder(),vrp->getHighOrder());
    }
  else
    {
    TR::RealRegister *rar=assignedReg->getRealRegister();
    if(_previouslyAssignedTo.Locate(virtualReg,hi))
      {
      if(_previouslyAssignedTo[hi] != (TR::RealRegister::RegNum)rar->getAssociation())
        _previouslyAssignedTo[hi] = TR::RealRegister::NoReg;
      }
    else
      _previouslyAssignedTo.Add(virtualReg,(TR::RealRegister::RegNum)rar->getAssociation());
    }
  }

TR_RegisterKinds
OMR::Z::CodeGenerator::prepareRegistersForAssignment()
   {
   TR_RegisterKinds kindsMask = OMR::CodeGenerator::prepareRegistersForAssignment();
   if (!self()->isOutOfLineColdPath() && _extentOfLitPool < 0)
      {
      // ILGRA will call prepareRegistersForAssignment so when localRA calls it again lit pool will be set up already
      TR_ASSERT( _extentOfLitPool < 0,"OMR::Z::CodeGenerator::doRegisterAssignment -- extentOfLitPool is assumed unassigned here.");

      int32_t numDummySnippets = self()->comp()->getOptions()->get390LitPoolBufferSize();

      // Emit dummy Constant Data Snippets to force literal pool access to require long displacements
      if (self()->comp()->getOption(TR_Randomize))
         {
         if (randomizer.randomBoolean(300) && performTransformation(self()->comp(),"O^O Random Codegen - Added 6000 dummy slots to  Literal Pool frame to test  Literal Pool displacement.\n"))
            numDummySnippets = 6000;
         }

      for (int32_t i = 0; i < numDummySnippets; i++)
         self()->findOrCreate8ByteConstant(0, (uint64_t)(-i-1));

      // Compute extent of litpool on N3 (exclude snippets)
      // for 31-bit systems, we add 4 to offset estimate because of possible alignment padding
      // between target addresses (4-byte aligned) and any 8-byte constant data.
      _extentOfLitPool = self()->setEstimatedOffsetForConstantDataSnippets();

      TR::Linkage *s390PrivateLinkage = self()->getS390Linkage();

      // Check to see if we need to lock the litpool reg
      if ( self()->isLitPoolFreeForAssignment() )
         {
         s390PrivateLinkage->unlockRegister(s390PrivateLinkage->getLitPoolRealRegister());
         }
      else
         {
         s390PrivateLinkage->lockRegister(s390PrivateLinkage->getLitPoolRealRegister());
         }
      // Check to see if we need to lock the static base reg
      if (!self()->isGlobalStaticBaseRegisterOn())
         {
         if (s390PrivateLinkage->getStaticBaseRealRegister())
            s390PrivateLinkage->unlockRegister(s390PrivateLinkage->getStaticBaseRealRegister());
         }
      else
         {
         if (s390PrivateLinkage->getStaticBaseRealRegister())
            s390PrivateLinkage->lockRegister(s390PrivateLinkage->getStaticBaseRealRegister());
         }

      // Check to see if we need to lock the private static base reg
      if (!self()->isGlobalPrivateStaticBaseRegisterOn())
         {
         if (s390PrivateLinkage->getPrivateStaticBaseRealRegister())
            s390PrivateLinkage->unlockRegister(s390PrivateLinkage->getPrivateStaticBaseRealRegister());
         }
      else
         {
         if (s390PrivateLinkage->getPrivateStaticBaseRealRegister())
            s390PrivateLinkage->lockRegister(s390PrivateLinkage->getPrivateStaticBaseRealRegister());
         }
      }

   TR::Machine* machine = self()->machine();

   // Lock vector registers. This is done for testing purposes and to artificially increase register pressure
   // We go from the end since in most cases, we want to test functionality of the first half i.e 0-15 for overlap with FPR etc
   if (TR::Options::_numVecRegsToLock > 0  && TR::Options::_numVecRegsToLock <= (TR::RealRegister::LastAssignableVRF - TR::RealRegister::FirstAssignableVRF))
       {
       for (int32_t i = TR::RealRegister::LastAssignableVRF; i > (TR::RealRegister::LastAssignableVRF - TR::Options::_numVecRegsToLock); --i)
          {
          machine->realRegister(static_cast<TR::RealRegister::RegNum>(i))->setState(TR::RealRegister::Locked);
          }
       }

   int32_t lockedGPRs = 0;
   int32_t lockedFPRs = 0;
   int32_t lockedVRFs = 0;

   // count up how many registers are locked for each type
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
      {
      TR::RealRegister* realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Locked)
         ++lockedGPRs;
      }

   for (int32_t i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; ++i)
      {
      TR::RealRegister* realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Locked)
         ++lockedFPRs;
      }

   for (int32_t i = TR::RealRegister::FirstVRF; i <= TR::RealRegister::LastVRF; ++i)
      {
      TR::RealRegister* realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Locked)
         ++lockedVRFs;
      }

   machine->setNumberOfLockedRegisters(TR_GPR, lockedGPRs);
   machine->setNumberOfLockedRegisters(TR_FPR, lockedFPRs);
   machine->setNumberOfLockedRegisters(TR_VRF, lockedVRFs);

   return kindsMask;
   }

void
OMR::Z::CodeGenerator::doRegisterAssignment(TR_RegisterKinds kindsToAssign)
   {
   TR::Instruction * prevInstruction, * nextInstruction;

#ifdef DEBUG
   TR::Instruction * origPrevInstruction;
   TR::Instruction * origNextInstruction;
   bool dumpPreVR  = (debug("dumpVRA")  || debug("dumpVRA0"))  && self()->comp()->getOutFile() != NULL;
   bool dumpPostVRF= (debug("dumpVRA")  || debug("dumpVRA1"))  && self()->comp()->getOutFile() != NULL;
   bool dumpPreFP = (debug("dumpFPRA") || debug("dumpFPRA0")) && self()->comp()->getOutFile() != NULL;
   bool dumpPostFP = (debug("dumpFPRA") || debug("dumpFPRA1")) && self()->comp()->getOutFile() != NULL;
   bool dumpPreGP = (debug("dumpGPRA") || debug("dumpGPRA0")) && self()->comp()->getOutFile() != NULL;
   bool dumpPostGP = (debug("dumpGPRA") || debug("dumpGPRA1")) && self()->comp()->getOutFile() != NULL;
#endif

   TR::Instruction * instructionCursor = self()->getAppendInstruction();
   int32_t instCount = 0;
   TR::Block *currBlock = NULL;
   TR::Instruction * currBBEndInstr = instructionCursor;

   if (!self()->comp()->getOption(TR_DisableOOL))
      {
      TR::list<TR::Register*> *firstTimeLiveOOLRegisterList = new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::Register*>(self()->comp()->allocator()));
      self()->setFirstTimeLiveOOLRegisterList(firstTimeLiveOOLRegisterList);

      if (!self()->isOutOfLineColdPath())
         {
         TR::list<TR::Register*> *spilledRegisterList = new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::CFGEdge*>(self()->comp()->allocator()));
         self()->setSpilledRegisterList(spilledRegisterList);
         }
      }

   if (!self()->isOutOfLineColdPath())
      {
      if (self()->getDebug())
         {
         TR_RegisterKinds rks = (TR_RegisterKinds)(TR_GPR_Mask | TR_FPR_Mask | TR_VRF_Mask);

         self()->getDebug()->startTracingRegisterAssignment("backward", rks);
         }
      }

   // Set all CFG Blocks unvisited
   for (currBlock = self()->comp()->getStartBlock(); currBlock != NULL; currBlock = currBlock->getNextBlock())
     {
     currBlock->setHasBeenVisited(false);
     }

   while (instructionCursor)
      {
      TR::Node *treeNode=instructionCursor->getNode();
      if (instructionCursor->getOpCodeValue() == TR::InstOpCode::FENCE && treeNode->getOpCodeValue() == TR::BBEnd)
         {
#ifdef DEBUG
         self()->setCurrentRABlock(treeNode->getBlock());
#endif
         }
      else if(instructionCursor->getOpCodeValue() == TR::InstOpCode::FENCE && treeNode->getOpCodeValue() == TR::BBStart)
         {
         }
      else if(instructionCursor->getOpCodeValue() == TR::InstOpCode::TBEGIN || instructionCursor->getOpCodeValue() == TR::InstOpCode::TBEGINC)
         {
         uint16_t immValue = ((TR::S390SILInstruction*)instructionCursor)->getSourceImmediate();
         uint8_t regMask = 0;
         for(int8_t i = TR::RealRegister::GPR0; i != TR::RealRegister::GPR15 + 1; i++)
            {
            if (self()->machine()->realRegister(static_cast<TR::RealRegister::RegNum>(i))->getState() == TR::RealRegister::Assigned)
               regMask |= (1 << (7 - ((i - 1) >> 1))); // bit 0 = GPR0/1, GPR0=1, GPR15=16. 'Or' with bit [(i-1)>>1]
            }
         immValue = immValue | (regMask<<8);
         ((TR::S390SILInstruction*)instructionCursor)->setSourceImmediate(immValue);
         }
         /**
         * Find a free real register for DCB to use during generate binary encoding phase
         * @see S390DebugCounterBumpInstruction::generateBinaryEncoding()
         */
      else if(instructionCursor->getOpCodeValue() == TR::InstOpCode::DCB)
         {
         TR::S390DebugCounterBumpInstruction *dcbInstr = static_cast<TR::S390DebugCounterBumpInstruction*>(instructionCursor);

         int32_t first = TR::RealRegister::FirstGPR + 1;  // skip GPR0
         int32_t last  = TR::RealRegister::LastAssignableGPR;

         TR::RealRegister * realReg;

         for (int32_t i=first; i<=last; i++)
            {
            realReg = self()->machine()->realRegister(static_cast<TR::RealRegister::RegNum>(i));

            if ( realReg->getState() == TR::RealRegister::Free)
               {
               dcbInstr->setAssignableReg(realReg);
               realReg->setHasBeenAssignedInMethod(true);
               break;
               }
            }

            self()->traceRegisterAssignment("BEST FREE REG for DCB is %R", dcbInstr->getAssignableReg());
         }

      self()->tracePreRAInstruction(instructionCursor);

      prevInstruction = instructionCursor->getPrev();

      if (instructionCursor->getNode()->getOpCodeValue() == TR::BBEnd)
         self()->comp()->setCurrentBlock(instructionCursor->getNode()->getBlock());

      // Main register assignment procedure
      self()->setCurrentBlockIndex(instructionCursor->getBlockIndex());
      instructionCursor->assignRegisters(TR_GPR);

      handleLoadWithRegRanges(instructionCursor, self());

      // Maintain Internal Control Flow Depth

      // Track internal control flow on labels
      if (instructionCursor->getOpCode().getOpCodeValue() == TR::InstOpCode::LABEL)
         {
         TR::S390LabelInstruction *li = toS390LabelInstruction(instructionCursor);

         if (li->getLabelSymbol() != NULL)
            {
            if (li->getLabelSymbol()->isStartInternalControlFlow())
               {
               self()->decInternalControlFlowNestingDepth();
               }
            if (li->getLabelSymbol()->isEndInternalControlFlow())
               {
               self()->incInternalControlFlowNestingDepth();
               }
            }
         }

      //Assertion to check that if you find a branch whose target is in the same basic block
      //as the branch itself, that it is inside Internal Control Flow
      // edTODO cleanup : Why is this disabled?
      if ((instructionCursor->getOpCode().getOpCodeValue() == TR::InstOpCode::BRC) && 0) // Disabled
         {
         // isBranch
         if (!self()->insideInternalControlFlow())
            {
            // not b2l
            if (instructionCursor != NULL)
               {
               if (instructionCursor->getNode() != NULL)
                  {
                  if (instructionCursor->getNode()->getBranchDestination() != NULL)
                     {
                     if (instructionCursor->getNode()->getBranchDestination()->getNode() != NULL)
                        {
                        if ((instructionCursor->getNode()->getBranchDestination()->getNode()->getBlock() != NULL)&&(instructionCursor->getNode()->getBlock() != NULL))
                           {
                           // if branching to the same basic block then the branch should be located inside ICF
                           // otherwise it's potentially not a case where the branch has to be inside Internal Control Flow
                           if (instructionCursor->getNode()->getBranchDestination()->getNode()->getBlock()->getNumber() == instructionCursor->getNode()->getBlock()->getNumber())
                              {
                              TR_ASSERT( 1,"ASSERTION - Branch found outside Internal Control Flow that should be located inside Internal Control Flow\n");
                              }
                           }
                        }
                     }
                  }
               }
            }
         }

      self()->tracePostRAInstruction(instructionCursor);

      self()->freeUnlatchedRegisters();
      self()->buildGCMapsForInstructionAndSnippet(instructionCursor);

      ++instCount;
      instructionCursor = prevInstruction;
      }

      _afterRA=true;

#ifdef DEBUG
   if (debug("traceMsg90GPR") || debug("traceGPRStats"))
      {
      self()->printStats(instCount);
      }
#endif

   // Done Local RA of GPRs, let make sure we don't have any live registers
   if (!self()->isOutOfLineColdPath())
      {
      for (uint32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
         {
         TR_ASSERT(self()->machine()->getRealRegister(i)->getState() != TR::RealRegister::Assigned,
                    "ASSERTION: GPR registers are still assigned at end of first RA pass! %s regNum=%s\n", self()->comp()->signature(), self()->getDebug()->getName(self()->machine()->getRealRegister(i)));
         }

      if (self()->getDebug())
         {
         self()->getDebug()->stopTracingRegisterAssignment();
         }
      }
   }

#ifdef DEBUG
void
OMR::Z::CodeGenerator::incTotalSpills()
   {
   TR::Block * block = self()->getCurrentRABlock();
   if (block->isCold()  /* || block->isRare(_compilation) */)
      _totalColdSpills++;
   else
      _totalHotSpills++;
   }

void
OMR::Z::CodeGenerator::incTotalRegisterXfers()
   {
   TR::Block * block = self()->getCurrentRABlock();
   if (block->isCold()  /* || block->isRare(_compilation) */)
      _totalColdRegisterXfers++;
   else
      _totalHotRegisterXfers++;
   }

void
OMR::Z::CodeGenerator::incTotalRegisterMoves()
   {
   TR::Block * block = self()->getCurrentRABlock();
   if (block->isCold()  /* || block->isRare(_compilation) */)
      _totalColdRegisterMoves++;
   else
      _totalHotRegisterMoves++;
   }

void
OMR::Z::CodeGenerator::printStats(int32_t instCount)
   {
   double totHot = (_totalHotSpills + _totalHotRegisterXfers + _totalHotRegisterMoves);
   fprintf(stderr,
      "REGISTER MOTION Compiled as [%10.10s] Factor [%4.4f] Contains hot blocks with [spills:%4.4d,xfers:%4.4d,moves:%4.4d] cold blocks with [spills:%4.4d,xfers:%4.4d,moves:%4.4d] for %s\n",
      self()->comp()->getHotnessName(self()->comp()->getMethodHotness()), (double)
      (totHot / instCount), _totalHotSpills,
      _totalHotRegisterXfers, _totalHotRegisterMoves, _totalColdSpills, _totalColdRegisterXfers, _totalColdRegisterMoves,
      self()->comp()->signature());
   }
#endif

void
OMR::Z::CodeGenerator::replaceInst(TR::Instruction* old, TR::Instruction* curr)
   {
   TR::Instruction* prv = old->getPrev();
   TR::Instruction* nxt = old->getNext();

   curr->setIndex(old->getIndex());
   curr->setBinLocalFreeRegs(old->getBinLocalFreeRegs());
   curr->setLocalLocalSpillReg1(old->getLocalLocalSpillReg1());
   curr->setLocalLocalSpillReg2(old->getLocalLocalSpillReg2());
   //if they are not beside each other, need to delete the curr instr from where it is, then
   //change pointers that go to old to point to curr, and curr must point to what
   //old used to point to
   if (prv != curr && nxt != curr)
      {
      self()->deleteInst(curr);
      prv->setNext(curr);
      curr->setNext(nxt);
      nxt->setPrev(curr);
      curr->setPrev(prv);
      }
   else  //if consecutive, then can just delete the old one
      {
      self()->deleteInst(old);
      }
   }

// TODO (Issue #254): This should really be a common code generator API. Push this up to the common code generator.
void
OMR::Z::CodeGenerator::deleteInst(TR::Instruction* old)
   {
   TR::Instruction* prv = old->getPrev();
   TR::Instruction* nxt = old->getNext();
   prv->setNext(nxt);

   // The next instruction could be the append instruction
   if (nxt != NULL)
      {
      nxt->setPrev(prv);
      }

   // Update the append instruction if we are deleting the last instruction in the stream
   if (self()->getAppendInstruction() == old)
      {
      self()->setAppendInstruction(prv);
      }
   }

void
OMR::Z::CodeGenerator::doPreRAPeephole()
   {
   TR_S390PreRAPeephole ph(self()->comp());
   ph.perform();
   }

void
OMR::Z::CodeGenerator::doPostRAPeephole()
   {
   TR_S390PostRAPeephole ph(self()->comp());
   ph.perform();
   }

void
OMR::Z::CodeGenerator::AddFoldedMemRefToStack(TR::MemoryReference * mr)
   {
   if (_stackOfMemoryReferencesCreatedDuringEvaluation.contains(mr))
      {
      return;
      }
   _stackOfMemoryReferencesCreatedDuringEvaluation.push(mr);
   }

void
OMR::Z::CodeGenerator::RemoveMemRefFromStack(TR::MemoryReference * mr)
   {
   if (_stackOfMemoryReferencesCreatedDuringEvaluation.contains(mr))
      {
      for (size_t i = 0; i < _stackOfMemoryReferencesCreatedDuringEvaluation.size(); i++)
         {
         if (_stackOfMemoryReferencesCreatedDuringEvaluation.element(i) == mr)
            {
            // note: this does not change the top of stack variable taken at the beginning of evaluation:
            // ...
            // ...
            // mr  <- top of stack before evaluation
            // mr2
            // mr3
            // mr4 <- current top of stack
            //
            // RemoveMemRefFromStack is called from stopUsingMemRefRegister, which is called from
            // an evaluator so that a memory reference's registers can ostensibly "be killed" earlier than
            // they would be by this function.
            //
            // Any stack entries pushed after mr should not exist earlier in the stack because memory references
            // should not be allowed to escape their evaluators - reuse is not possible. This means that
            // there should be no way for remove(i) to delete something before the top of stack before evaluation mark.

            _stackOfMemoryReferencesCreatedDuringEvaluation.remove(i);
            break;
            }
         }
      }
   }

void
OMR::Z::CodeGenerator::StopUsingEscapedMemRefsRegisters(int32_t topOfMemRefStackBeforeEvaluation)
   {
   while (_stackOfMemoryReferencesCreatedDuringEvaluation.topIndex() > topOfMemRefStackBeforeEvaluation)
      {
      // note: this cast is only safe because currently:
      // 1) the only call to AddFoldedMemRefToStack occurs in TR::MemoryReference
      // 2) AddFoldedMemRefToStack is only defined for OMR::Z::CodeGenerator, and requires TR::MemoryReference instead of TR_MemoryReference.
      //
      TR::MemoryReference * potentiallyLeakedMemRef = (TR::MemoryReference *)(_stackOfMemoryReferencesCreatedDuringEvaluation.pop());

      // note: even if stopUsingRegister is called for _baseRegister or _indexRegister, this
      // this should be safe to call / it should be safe to "stop using" a register multiple times.
      potentiallyLeakedMemRef->stopUsingMemRefRegister(self());

      if (self()->comp()->getOption(TR_TraceCG))
         {
         self()->comp()->getDebug()->trace(" _stackOfMemoryReferencesCreatedDuringEvaluation.pop() %p, stopUsingMemRefRegister called.\n", potentiallyLeakedMemRef);
         }
      }
   }

bool
OMR::Z::CodeGenerator::supportsMergingGuards()
   {
   return self()->getSupportsVirtualGuardNOPing() &&
          self()->comp()->performVirtualGuardNOPing() &&
          !self()->comp()->compileRelocatableCode();
   }

bool
OMR::Z::CodeGenerator::supportsNonHelper(TR::SymbolReferenceTable::CommonNonhelperSymbol symbol)
   {
   bool result = false;

   switch (symbol)
      {
      case TR::SymbolReferenceTable::atomicAddSymbol:
      case TR::SymbolReferenceTable::atomicFetchAndAddSymbol:
         {
         result = self()->comp()->target().cpu.getSupportsArch(TR::CPU::z196);
         break;
         }

      case TR::SymbolReferenceTable::atomicSwapSymbol:
         {
         result = true;
         break;
         }
      }

   return result;
   }

// Helpers for profiled interface slots
void
OMR::Z::CodeGenerator::addPICsListForInterfaceSnippet(TR::S390ConstantDataSnippet * ifcSnippet, TR::list<TR_OpaqueClassBlock*> * PICSlist)
   {
   if (_interfaceSnippetToPICsListHashTab == NULL)
      {_interfaceSnippetToPICsListHashTab = new (self()->trHeapMemory()) TR_HashTab(self()->comp()->trMemory(), heapAlloc, 10, true);}

   TR_HashId hashIndex = 0;
   _interfaceSnippetToPICsListHashTab->add((void *)ifcSnippet, hashIndex, (void *)PICSlist);
   }

TR::list<TR_OpaqueClassBlock*> *
OMR::Z::CodeGenerator::getPICsListForInterfaceSnippet(TR::S390ConstantDataSnippet * ifcSnippet)
   {
   if (_interfaceSnippetToPICsListHashTab == NULL)
      return NULL;

   TR_HashId hashIndex = 0;
   if (_interfaceSnippetToPICsListHashTab->locate((void *)ifcSnippet, hashIndex))
      return static_cast<TR::list<TR_OpaqueClassBlock*>*>(_interfaceSnippetToPICsListHashTab->getData(hashIndex));
   else
      return NULL;

   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::doBinaryEncoding
////////////////////////////////////////////////////////////////////////////////

/**
 * Step through the list of snippets looking for any that are not data constants
 * @return true if one is found
 */
bool
OMR::Z::CodeGenerator::anyNonConstantSnippets()
   {

   for (auto iterator = _snippetList.begin(); iterator != _snippetList.end(); ++iterator)
      {
      if ((*iterator)->getKind()!=TR::Snippet::IsConstantData   ||
           (*iterator)->getKind()!=TR::Snippet::IsWritableData   ||
           (*iterator)->getKind()!=TR::Snippet::IsEyeCatcherData
          )
          {
          return true;
          }
      }
   return false;
   }

/**
 * Step through the list of snippets looking for any that are not Unresolved Static Fields
 * @return true if one is found
 */
bool
OMR::Z::CodeGenerator::anyLitPoolSnippets()
   {
   CS2::HashIndex hi;
   TR::S390ConstantDataSnippet *cursor1 = NULL;

    for (auto constantiterator = _constantList.begin(); constantiterator != _constantList.end(); ++constantiterator)
      {
      if ((*constantiterator)->getNeedLitPoolBasePtr())
         {
         return true;
         }
      }
    for(hi = _constantHashCur.SetToFirst(); _constantHashCur.Valid(); hi = _constantHashCur.SetToNext())
        {
          cursor1 = _constantHash.DataAt(hi);
           if (cursor1->getNeedLitPoolBasePtr())
              {
                return true;
              }
        }

    for (auto writeableiterator = _writableList.begin(); writeableiterator != _writableList.end(); ++writeableiterator)
      {
      if ((*writeableiterator)->getNeedLitPoolBasePtr())
         {
         return true;
         }
      }
      return false;
   }

bool
OMR::Z::CodeGenerator::getSupportsEncodeUtf16BigWithSurrogateTest()
   {
   if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      return (!self()->comp()->getOption(TR_DisableUTF16BEEncoder) ||
               (self()->getSupportsVectorRegisters() && !self()->comp()->getOption(TR_DisableSIMDUTF16BEEncoder)));
      }

   return false;
   }

TR_S390ScratchRegisterManager*
OMR::Z::CodeGenerator::generateScratchRegisterManager(int32_t capacity)
   {
   return new (self()->trHeapMemory()) TR_S390ScratchRegisterManager(capacity, self());
   }

void
OMR::Z::CodeGenerator::doBinaryEncoding()
   {
   // Generate the first label by using the placement new operator such that we are guaranteed to call the correct
   // overload of the constructor which can accept a NULL preceding instruction. If cursor is NULL the generated
   // label instruction will be prepended to the start of the instruction stream.
   _methodBegin = new (self()->trHeapMemory()) TR::S390LabelInstruction(TR::InstOpCode::LABEL, self()->comp()->getStartTree()->getNode(), generateLabelSymbol(self()), static_cast<TR::Instruction*>(NULL), self());

   _methodEnd = generateS390LabelInstruction(self(), TR::InstOpCode::LABEL, self()->comp()->findLastTree()->getNode(), generateLabelSymbol(self()));

   TR_S390BinaryEncodingData data;
   data.cursorInstruction = self()->getFirstInstruction();
   data.estimate = 0;
   data.loadArgSize = 0;
   TR::Recompilation* recomp = self()->comp()->getRecompilationInfo();

   //  setup cursor for JIT to JIT transfer
   //
   if (self()->comp()->getJittedMethodSymbol()->isJNI() && !self()->comp()->getOption(TR_FullSpeedDebug))
      {
      data.preProcInstruction = self()->comp()->target().is64Bit() ?
         data.cursorInstruction->getNext()->getNext()->getNext() :
         data.cursorInstruction->getNext()->getNext();
      }
   else
      {
      data.preProcInstruction = data.cursorInstruction->getNext();
      }

   data.jitTojitStart = data.preProcInstruction->getNext();

   // Generate code to setup argument registers for interpreter to JIT transfer
   // This piece of code is right before JIT-JIT entry point
   //
   TR::Instruction * preLoadArgs, * endLoadArgs;
   preLoadArgs = data.preProcInstruction;

   // We need full prolog if there is a call or a non-constant snippet
   //
   TR_BitVector * callBlockBV = self()->getBlocksWithCalls();

   // No exit points, hence we can
   //
   if (callBlockBV->isEmpty() && !self()->anyNonConstantSnippets())
      {
      self()->setExitPointsInMethod(false);
      }

   endLoadArgs = self()->getLinkage()->loadUpArguments(preLoadArgs);

   if (recomp != NULL)
      {
#ifdef J9_PROJECT_SPECIFIC
      if (preLoadArgs != endLoadArgs)
         {
         data.loadArgSize = CalcCodeSize(preLoadArgs->getNext(), endLoadArgs);
         }

      ((TR_S390Recompilation *) recomp)->setLoadArgSize(data.loadArgSize);
#endif
      recomp->generatePrePrologue();
      }
#ifdef J9_PROJECT_SPECIFIC
   else if (self()->comp()->getOption(TR_FullSpeedDebug) || self()->comp()->getOption(TR_SupportSwitchToInterpreter))
      {
      self()->generateVMCallHelperPrePrologue(self()->getFirstInstruction());
      }
#endif

   data.cursorInstruction = self()->getFirstInstruction();

   // Padding for JIT Entry Point
   //
   if (!self()->comp()->compileRelocatableCode())
      {
      data.estimate += 256;
      }

   TR::Instruction* cursor = data.cursorInstruction;

   // TODO: We should be caching the PROC instruction as it's used in several places and is pretty important
   while (cursor && cursor->getOpCodeValue() != TR::InstOpCode::PROC)
      {
      cursor = cursor->getNext();
      }

   if (self()->comp()->getOption(TR_EntryBreakPoints))
      {
      TR::Node *node = self()->comp()->getStartTree()->getNode();
      cursor = generateS390EInstruction(self(), TR::InstOpCode::BREAK, node, cursor);
      }

   if (recomp != NULL)
      {
      cursor = recomp->generatePrologue(cursor);
      }

   self()->getLinkage()->createPrologue(cursor);

   bool isPrivateLinkage = (self()->comp()->getJittedMethodSymbol()->getLinkageConvention() == TR_Private);

   TR::Instruction *instr = self()->getFirstInstruction();

   while (instr)
      {
      TR::Register *reg = instr->getRegisterOperand(1);
      if (reg)
         {
         if (reg->getRegisterPair())
            {
            TR::RealRegister * lowReg = toRealRegister(reg->getLowOrder());
            TR::RealRegister * highReg = toRealRegister(reg->getHighOrder());
            lowReg->setModified(true);
            highReg->setModified(true);
            if (instr->getOpCodeValue() == TR::InstOpCode::LM ||
                instr->getOpCodeValue() == TR::InstOpCode::LMG)
               {
               uint8_t numRegs = (lowReg->getRegisterNumber() - highReg->getRegisterNumber())-1;
               if (numRegs > 0)
                  {
                  for (uint8_t i=highReg->getRegisterNumber()+1; i++; i<= numRegs)
                     {
                     self()->getS390Linkage()->getRealRegister(REGNUM(i))->setModified(true);
                     }
                  }
               }
            }
         else
            {
            TR::RealRegister * rReg = toRealRegister(reg);
            rReg->setModified(true);
            }
         }
      instr = instr->getNext();
      }

   // Create epilogues for all return points
   bool skipOneReturn = false;
   while (data.cursorInstruction)
      {
      if (data.cursorInstruction->getNode() != NULL && data.cursorInstruction->getNode()->getOpCodeValue() == TR::BBStart)
         {
         self()->setCurrentBlock(data.cursorInstruction->getNode()->getBlock());
         }

      if (data.cursorInstruction->getOpCodeValue() == TR::InstOpCode::RET)
         {

         if (skipOneReturn == false)
            {
            TR::Instruction * temp = data.cursorInstruction->getPrev();
            TR::Instruction *originalNextInstruction = temp->getNext();

            self()->getLinkage()->createEpilogue(temp);

            if (self()->comp()->getOption(TR_EnableLabelTargetNOPs))
               {
               for (TR::Instruction *inst = temp->getNext(); inst != originalNextInstruction; inst = inst->getNext())
                  {
                  TR::Instruction *s390Inst = inst;
                  if (s390Inst->getKind() == TR::Instruction::IsLabel)
                     {
                     if (self()->comp()->getOption(TR_TraceLabelTargetNOPs))
                        traceMsg(self()->comp(),"\t\tepilogue inst %p (%s) setSkipForLabelTargetNOPs\n",s390Inst,s390Inst->getOpCode().getMnemonicName());
                     TR::S390LabelInstruction *labelInst = (TR::S390LabelInstruction*)s390Inst;
                     labelInst->setSkipForLabelTargetNOPs();
                     }
                  }
               }

            data.cursorInstruction = temp->getNext();

            /* skipOneReturn only if epilog is generated which is indicated by instructions being */
            /* inserted before TR::InstOpCode::RET.  If no epilog is generated, TR::InstOpCode::RET will stay               */
            if (data.cursorInstruction->getOpCodeValue() != TR::InstOpCode::RET)
               skipOneReturn = true;
            }
         else
            {
            skipOneReturn = false;
            }
         }

      data.estimate = data.cursorInstruction->estimateBinaryLength(data.estimate);
      data.cursorInstruction = data.cursorInstruction->getNext();
      }

   if (self()->comp()->getOption(TR_TraceCG))
      self()->comp()->getDebug()->dumpMethodInstrs(self()->comp()->getOutFile(), "Post Prologue/epilogue Instructions", false);

   data.estimate = self()->setEstimatedLocationsForSnippetLabels(data.estimate);
   // need to reset constant data snippets offset for inlineEXTarget peephole optimization
   static char * disableEXRLDispatch = feGetEnv("TR_DisableEXRLDispatch");
   if (!(bool)disableEXRLDispatch && self()->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      _extentOfLitPool = self()->setEstimatedOffsetForConstantDataSnippets();
      }

   self()->setEstimatedCodeLength(data.estimate);

   data.cursorInstruction = self()->getFirstInstruction();

   uint8_t *coldCode = NULL;
   uint8_t *temp = self()->allocateCodeMemory(self()->getEstimatedCodeLength(), 0, &coldCode);

   self()->setBinaryBufferStart(temp);
   self()->setBinaryBufferCursor(temp);
   self()->alignBinaryBufferCursor();

   while (data.cursorInstruction)
      {
      uint8_t* const instructionStart = self()->getBinaryBufferCursor();

      self()->setBinaryBufferCursor(data.cursorInstruction->generateBinaryEncoding());

      TR_ASSERT(data.cursorInstruction->getEstimatedBinaryLength() >= self()->getBinaryBufferCursor() - instructionStart,
              "\nInstruction length estimate must be conservatively large \n(instr=" POINTER_PRINTF_FORMAT ", opcode=%s, estimate=%d, actual=%d",
              data.cursorInstruction,
              data.cursorInstruction->getOpCode().getMnemonicName(),
              data.cursorInstruction->getEstimatedBinaryLength(),
              self()->getBinaryBufferCursor() - instructionStart);

      self()->addToAtlas(data.cursorInstruction);

      if (data.cursorInstruction == data.preProcInstruction)
         {
         self()->setPrePrologueSize(self()->getBinaryBufferLength());
         self()->comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()->getSymbol()->getStaticSymbol()->setStaticAddress(self()->getBinaryBufferCursor());
         }

      data.cursorInstruction = data.cursorInstruction->getNext();

      // generate magic word
      if (isPrivateLinkage && data.cursorInstruction == data.jitTojitStart)
         {
         uint32_t argSize = self()->getBinaryBufferCursor() - self()->getCodeStart();
         uint32_t magicWord = (argSize << 16) | static_cast<uint32_t>(self()->comp()->getReturnInfo());
         uint32_t recompFlag = 0;

#ifdef J9_PROJECT_SPECIFIC
         if (recomp != NULL && recomp->couldBeCompiledAgain())
            {
            J9::PrivateLinkage::LinkageInfo * linkageInfo = J9::PrivateLinkage::LinkageInfo::get(self()->getCodeStart());
            if (recomp->useSampling())
               {
               recompFlag = METHOD_SAMPLING_RECOMPILATION;
               linkageInfo->setSamplingMethodBody();
               }
            else
               {
               recompFlag = METHOD_COUNTING_RECOMPILATION;
               linkageInfo->setCountingMethodBody();
               }
            }
#endif
         magicWord |= recompFlag;

         // first word is the code size of intrepeter-to-jit glue in bytes and the second word is the return info
         toS390ImmInstruction(data.preProcInstruction)->setSourceImmediate(magicWord);
         *(uint32_t *) (data.preProcInstruction->getBinaryEncoding()) = magicWord;
         }
      }

   // Create exception table entries for outlined instructions.
   //
   if (!self()->comp()->getOption(TR_DisableOOL))
      {
      auto oiIterator = self()->getS390OutOfLineCodeSectionList().begin();
      while (oiIterator != self()->getS390OutOfLineCodeSectionList().end())
         {
       TR::Block * block = (*oiIterator)->getBlock();
         TR::Node *firstNode = (*oiIterator)->getFirstInstruction()->getNode();

         // Create Exception Table Entries for Nodes that can cause GC's.
         bool needsETE = (firstNode->getOpCode().hasSymbolReference())? firstNode->getSymbolReference()->canCauseGC() : false;
#ifdef J9_PROJECT_SPECIFIC
         if (firstNode->getOpCodeValue() == TR::BCDCHK || firstNode->getType().isBCD()) needsETE = true;
#endif
         if (needsETE && block && !block->getExceptionSuccessors().empty())
            {
            uint32_t startOffset = (*oiIterator)->getFirstInstruction()->getBinaryEncoding() - self()->getCodeStart();
            uint32_t endOffset   = (*oiIterator)->getAppendInstruction()->getBinaryEncoding() - self()->getCodeStart();

            block->addExceptionRangeForSnippet(startOffset, endOffset);
            }

         ++oiIterator;
         }
      }

   self()->getLinkage()->performPostBinaryEncoding();
   }

/**
 * Allocate consecutive register pairs. We set up associations which are used as hints
 * for the register allocator.  This does not guarantee that the pair will be consecutive.
 * You must use a dependency on the instruction that requires the pair if the instruction
 * does not have consecutive pair allocation support.
 */
TR::RegisterPair *
OMR::Z::CodeGenerator::allocateConsecutiveRegisterPair()
   {
   TR::Register * lowRegister = self()->allocateRegister();
   TR::Register * highRegister = self()->allocateRegister();

   return self()->allocateConsecutiveRegisterPair(lowRegister, highRegister);
   }

TR::RegisterPair *
OMR::Z::CodeGenerator::allocateConsecutiveRegisterPair(TR::Register * lowRegister, TR::Register * highRegister)
   {
   TR::RegisterPair * regPair = self()->allocateRegisterPair(lowRegister, highRegister);

   // Set associations for providing hint to RA
   regPair->setAssociation(TR::RealRegister::EvenOddPair);

   highRegister->setAssociation(TR::RealRegister::LegalEvenOfPair);
   highRegister->setSiblingRegister(lowRegister);

   lowRegister->setAssociation(TR::RealRegister::LegalOddOfPair);
   lowRegister->setSiblingRegister(highRegister);

   return regPair;
   }

void
OMR::Z::CodeGenerator::processUnusedNodeDuringEvaluation(TR::Node *node)
   {
   //to ensure that if the node is already evaluated we don't call processUnusedStorageRef twice
   bool alreadyProcessedUnusedStorageRef = false;
   TR_ASSERT(self()->getCodeGeneratorPhase() > TR::CodeGenPhase::SetupForInstructionSelectionPhase,"must be past TR::CodeGenPhase::SetupForInstructionSelectionPhase (current is %s)\n",
      self()->getCodeGeneratorPhase().getName());
   if (node)
      {
#ifdef J9_PROJECT_SPECIFIC
      // first deal with an extra address child ref if this is the last reference to a nodeBased storageRef
      if (node->getOpaquePseudoRegister())
         {
         TR_OpaquePseudoRegister *reg = node->getOpaquePseudoRegister();
         TR_StorageReference *ref = reg->getStorageReference();
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\tprocessUnusedNodeDuringEvaluation : bcd/aggr const/ixload %s (%p) reg %s - handle extra ref to addr child (ref is node based %s - %s %p)\n",
               node->getOpCode().getName(),node,self()->getDebug()->getName(reg),ref && ref->isNodeBased()?"yes":"no",
               ref->isNodeBased()?ref->getNode()->getOpCode().getName():"",ref->isNodeBased()?ref->getNode():NULL);
         self()->processUnusedStorageRef(ref);
         alreadyProcessedUnusedStorageRef = true;
         }
#endif
      // unless the node matches set patterns containing only loadaddrs,arithmetic and constants
      // the node must be evaluated so any loads that could be killed are privatised to a register
      if (node->safeToDoRecursiveDecrement())
         {
         self()->recursivelyDecReferenceCount(node);
         }
      else if (node->getReferenceCount() == 1 &&
               node->getOpCode().isLoadVar() &&
               (node->getNumChildren() == 0 || (node->getNumChildren() == 1 && node->getFirstChild()->safeToDoRecursiveDecrement())))
         {
         // i.e. a direct load that has no future refs or a indirect load with no future refs that itself has a child that is safeToRecDec
         self()->recursivelyDecReferenceCount(node);
         }
      else
         {
         self()->evaluate(node);
         self()->decReferenceCount(node);
#ifdef J9_PROJECT_SPECIFIC
         if (node->getOpaquePseudoRegister() && !alreadyProcessedUnusedStorageRef)
            {
            TR_OpaquePseudoRegister *reg = node->getOpaquePseudoRegister();
            TR_StorageReference *ref = reg->getStorageReference();
            if (self()->traceBCDCodeGen())
               traceMsg(self()->comp(),"\tprocessUnusedNodeDuringEvaluation : bcd/aggr const/ixload %s (%p) reg %s - handle extra ref to addr child (ref is node based %s - %s %p)\n",
                  node->getOpCode().getName(),node,self()->getDebug()->getName(static_cast<TR::Register*>(reg)),ref && ref->isNodeBased()?"yes":"no",
                  ref->isNodeBased()?ref->getNode()->getOpCode().getName():"",ref->isNodeBased()?ref->getNode():NULL);
            self()->processUnusedStorageRef(ref);
            }
#endif
         }
      }
   }

TR::RegisterDependencyConditions*
OMR::Z::CodeGenerator::createDepsForRRMemoryInstructions(TR::Node *node, TR::RegisterPair * sourceReg, TR::RegisterPair * targetReg, uint8_t extraDeps)
   {
   uint8_t numDeps = 8 + extraDeps; // TODO: only 6 deps are created below so why is there is always an implicit 2 extra deps created?
   TR::RegisterDependencyConditions * dependencies = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, self());
   dependencies->addPostCondition(sourceReg->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(sourceReg->getLowOrder(), TR::RealRegister::LegalOddOfPair);
   dependencies->addPostCondition(sourceReg, TR::RealRegister::EvenOddPair);

   dependencies->addPostCondition(targetReg->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(targetReg->getLowOrder(), TR::RealRegister::LegalOddOfPair);
   dependencies->addPostCondition(targetReg, TR::RealRegister::EvenOddPair);
   return dependencies;
   }

/**
 * Allocate floating posint register pairs. We set up associations which are used as hints
 * for the register allocator.
 * You must use a dependency on the instruction that requires the pair if the instruction
 * does not have FP pair allocation support.
 */
TR::RegisterPair *
OMR::Z::CodeGenerator::allocateFPRegisterPair()
   {
   TR::Register * lowRegister = self()->allocateRegister(TR_FPR);
   TR::Register * highRegister = self()->allocateRegister(TR_FPR);

   return self()->allocateFPRegisterPair(lowRegister, highRegister);
   }

/**
 * Naming of low/high reg are confusing, but we'll follow existing convention
 * low means the lower 32bit portion of the long double,
 * so it means:
 * --  high memory address(S390 is big-endian)
 * --  high reg # of the pair
 */
TR::RegisterPair *
OMR::Z::CodeGenerator::allocateFPRegisterPair(TR::Register * lowRegister, TR::Register * highRegister)
   {
   TR::RegisterPair * regPair = NULL;

//   regPair = TR::CodeGenerator::allocateRegisterPair(lowRegister, highRegister);

   regPair = self()->allocateFloatingPointRegisterPair(lowRegister, highRegister);

//   regPair->setKind(TR_FPR);

   // Set associations for providing hint to RA
   //DXL regPair->setAssociation(TR::RealRegister::EvenOddPair);
   regPair->setAssociation(TR::RealRegister::FPPair);

   //DXL highRegister->setAssociation(TR::RealRegister::LegalEvenOfPair);
   highRegister->setAssociation(TR::RealRegister::LegalFirstOfFPPair);
   highRegister->setSiblingRegister(lowRegister);

   //DXL lowRegister->setAssociation(TR::RealRegister::LegalOddOfPair);
   lowRegister->setAssociation(TR::RealRegister::LegalSecondOfFPPair);
   lowRegister->setSiblingRegister(highRegister);

   return regPair;
   }

///////////////////////////////////////////////////////////////////////////////////
//  Register Association
bool
OMR::Z::CodeGenerator::enableRegisterAssociations()
   {
   return true;
   }

bool
OMR::Z::CodeGenerator::enableRegisterPairAssociation()
   {
   return true;
   }

////////////////////////////////////////////////////////////////////////////////
//  Tactical GRA
TR_GlobalRegisterNumber
OMR::Z::CodeGenerator::getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type)
   {
   TR_GlobalRegisterNumber result;
   bool isFloat = (type == TR::Float
                   || type == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                   || type == TR::DecimalFloat
                   || type == TR::DecimalDouble
                   || type == TR::DecimalLongDouble
#endif
                   );

   if (isFloat)
      {
      result = self()->machine()->getLastLinkageFPR() - linkageRegisterIndex;
      }
   else if (type.isVector())
      {
      result = self()->machine()->getLastGlobalVRFRegisterNumber() - linkageRegisterIndex;
      }
   else
      {
      result = self()->getGlobalRegisterNumber(self()->getS390Linkage()->getIntegerArgumentRegister(linkageRegisterIndex)-1);
      }

   return result;
   }

inline bool isBetween(int64_t value, int32_t low, int32_t high)
   {
   return low <= value && value < high;
   }

static int64_t getValueIfIntegralConstant(TR::Node *node, TR::Compilation *comp)
   {
   if (node->getOpCode().isLoadConst() && node->getType().isIntegral())
      return node->get64bitIntegralValue();
   else
      return TR::getMaxSigned<TR::Int64>();
   }

static bool canProbablyFoldSecondChildIfConstant(TR::Node *node)
   {
   return (node->getNumChildren() == 2 && !node->getOpCode().isCall())
       || (node->getNumChildren() == 3 && node->getChild(2)->getOpCodeValue() == TR::GlRegDeps);
   }

void OMR::Z::CodeGenerator::simulateNodeEvaluation(TR::Node * node, TR_RegisterPressureState * state, TR_RegisterPressureSummary * summary)
   {
   // GPR0 can't be used in memory references
   //
   if (self()->isCandidateLoad(node, state) && state->_memrefNestDepth >= 1)
      summary->spill(TR_gpr0Spill, self());

   // GPR0 and GPR1 are used by TR::arraytranslate and TR::arraytranslateAndTest
   //
   if (node->getOpCodeValue() == TR::arraytranslate || node->getOpCodeValue() == TR::arraytranslateAndTest)
      summary->spill(TR_gpr1Spill, self());

   // Integer constants can often be folded into immediates, and don't consume registers
   //
   int64_t constValue  = getValueIfIntegralConstant(node, self()->comp());
   int64_t child2Value = TR::getMaxSigned<TR::Int64>();
   if (canProbablyFoldSecondChildIfConstant(node))
      child2Value = getValueIfIntegralConstant(node->getSecondChild(), self()->comp());

   if (isBetween(child2Value, -1<<15, 1<<15))
      {
      // iconst child can probably be folded into an immediate field
      self()->simulateSkippedTreeEvaluation(node->getSecondChild(), state, summary, 'i');
      self()->simulateDecReferenceCount(node->getSecondChild(), state);
      self()->simulateTreeEvaluation(node->getFirstChild(), state, summary);
      self()->simulateDecReferenceCount(node->getFirstChild(), state);
      self()->simulatedNodeState(node)._childRefcountsHaveBeenDecremented = 1;
      self()->simulateNodeGoingLive(node, state);
      }
   else if (node->getOpCode().isLoadConst() && state->_memrefNestDepth >= 1)
      {
      // Can the const be folded into the memref?
      if (isBetween(constValue, -1<<19, 1<<19))
         {
         self()->simulateSkippedTreeEvaluation(node, state, summary, 'i');
         }
      else if (isBetween(constValue, 0, 1<<12))
         {
         self()->simulateSkippedTreeEvaluation(node, state, summary, 'i');
         }
      else // Can't be folded
         {
         OMR::CodeGenerator::simulateNodeEvaluation(node, state, summary);
         }
      }
   else
      {
      // Default to inherited logic
      OMR::CodeGenerator::simulateNodeEvaluation(node, state, summary);
      }
   }

bool
OMR::Z::CodeGenerator::allowGlobalRegisterAcrossBranch(TR_RegisterCandidate * rc, TR::Node * branchNode)
   {
   // If return false, processLiveOnEntryBlocks has to dis-qualify any candidates which are referenced
   // within any CASE of a SWITCH statement.
   return true;
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumberOfAssignableGPRs()
   {
   //  We should try and figure out 11 non-locked regs automatically
   //  For now 16 -
   //   ( JSP
   //     Native SP
   //     VMThread
   //     Lit pool)
   //   = 12 assignable regs
   //
   int32_t maxGPRs = 0;

   maxGPRs = 12 + (self()->isLiteralPoolOnDemandOn() ? 1 : 0);

   //traceMsg(comp(), " getMaximumNumberOfAssignableGPRs: %d\n",  maxGPRs);
   return maxGPRs;
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Block *block)
   {
   TR::Node *node = block->getLastRealTreeTop()->getNode();
   int32_t num = self()->getMaximumNumberOfGPRsAllowedAcrossEdge(node);

   return num >= 0 ?  num : 0;
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node * node)
   {
   int32_t  maxGPRs = self()->getMaximumNumberOfAssignableGPRs();
   bool isFloat = false;
   bool isBCD = false;

   if (node->getOpCode().isIf() && node->getNumChildren() > 0 && !node->getOpCode().isCompBranchOnly())
      {
      TR::DataType dt = node->getFirstChild()->getDataType();

      isFloat = (dt == TR::Float
                 || dt == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                 || dt == TR::DecimalFloat
                 || dt == TR::DecimalDouble
                 || dt == TR::DecimalLongDouble
#endif
                 );

#ifdef J9_PROJECT_SPECIFIC
      isBCD = node->getFirstChild()->getType().isBCD();
#endif
      }

   if (node->getOpCode().isSwitch())
      {
      if (node->getOpCodeValue() == TR::table)
         {
            {
            return maxGPRs - 1;
            }
         }
      else
         {
         return maxGPRs - 1;
         }
      }
   else if (node->getOpCode().isIf() && node->getNumChildren() > 0 && node->getFirstChild()->getType().isInt64())
      {
      if (node->getSecondChild()->getOpCode().isLoadConst())
         {
         int64_t value = getIntegralValue(node->getSecondChild());

         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            return maxGPRs - 1;   // CGHI R,IMM
            }
         else
            {
            return maxGPRs - 2;   // CGR R1,R2
            }
         }
      else
         {
         return maxGPRs - 2;
         }
      }
   else if (node->getOpCode().isIf() && isFloat)
      {
      return maxGPRs - 1;   // We need one GPR for the lit pool ptr
      }
   else if (node->getOpCode().isIf() && !isBCD)
      {
      if (node->getSecondChild()->getOpCode().isLoadConst())
         {
         int64_t value = getIntegralValue(node->getSecondChild());

         if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12) && value >= MIN_IMMEDIATE_BYTE_VAL && value <= MAX_IMMEDIATE_BYTE_VAL)
            {
            return maxGPRs - 2;   // CLGIJ R,IMM,LAB,MASK, last instruction on block boundary
            }
         else if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            return maxGPRs - 1;   // CHI R,IMM
            }
         else
            {
            return maxGPRs - 2;   // CR R1,R2
            }
         }
      else
         {
         return maxGPRs - 2;      // CR R1,R2
         }
      }
   return maxGPRs;
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node * node)
   {
   return 8;
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumberOfVRFsAllowedAcrossEdge(TR::Node * node)
   {
   return 16; // This can be dynamically selected based on context, but for now, keep it conservative..
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumbersOfAssignableGPRs()
   {
   return self()->getMaximumNumberOfAssignableGPRs();

   //int32_t maxNumberOfAssignableGPRS = (8 + (self()->isLiteralPoolOnDemandOn() ? 1 : 0));
   //return maxNumberOfAssignableGPRS;
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumbersOfAssignableFPRs()
   {
   return 16;
   }

int32_t
OMR::Z::CodeGenerator::getMaximumNumbersOfAssignableVRs()
   {
//   TR_ASSERT(false,"getMaximumNumbersOfAssignableVRs unreachable");
   return INT_MAX;
   }

void
OMR::Z::CodeGenerator::setRealRegisterAssociation(TR::Register * reg, TR::RealRegister::RegNum realNum)
   {
   if (!reg->isLive() || realNum == TR::RealRegister::NoReg)
      {
      return;
      }

   TR::RealRegister * realReg = self()->machine()->getRealRegister(realNum);
   self()->getLiveRegisters(reg->getKind())->setAssociation(reg, realReg);
   }

bool
OMR::Z::CodeGenerator::isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt)
   {
   if ((self()->getGlobalRegister(i) != TR::RealRegister::GPR0) || (dt != TR::Address))
     return true;
   else
     return false;
   }

/**
 * Allocates a register with the same collectible and internal pointer
 * characteristics as the given source register.  Used by clobber evaluate
 * to allocate a new register when necessary.
 */
TR::Register *
OMR::Z::CodeGenerator::allocateClobberableRegister(TR::Register *srcRegister)
   {
   TR::Register * targetRegister = NULL;

   if (srcRegister->containsCollectedReference())
      {
      targetRegister = self()->allocateCollectedReferenceRegister();
      }
   else if (srcRegister->getKind() == TR_GPR)
      {
      targetRegister = self()->allocateRegister();
      }
   else if (srcRegister->getKind() == TR_VRF)
      {
      targetRegister = self()->allocateRegister(TR_VRF);
      }
   else //todo: what about FPR?
      {
      targetRegister = self()->allocateRegister();
      }

   if (srcRegister->containsInternalPointer())
      {
      targetRegister->setContainsInternalPointer();
      targetRegister->setPinningArrayPointer(srcRegister->getPinningArrayPointer());
      }

   return targetRegister;
   }

/**
 * Different from evaluate in that it returns a clobberable register
 */
TR::Register *
OMR::Z::CodeGenerator::gprClobberEvaluate(TR::Node * node, bool force_copy, bool ignoreRefCount)
   {
   TR::Instruction * cursor = NULL;

   TR_Debug * debugObj = self()->getDebug();
   TR::Register * srcRegister    = self()->evaluate(node);

#ifdef J9_PROJECT_SPECIFIC
   TR_ASSERT(!(node->getType().isAggregate() || node->getType().isBCD()),"BCD/Aggr node (%s) %p should use ssrClobberEvaluate\n", node->getOpCode().getName(),node);
#else
   TR_ASSERT(!node->getType().isAggregate(),"Aggr node (%s) %p should use ssrClobberEvaluate\n", node->getOpCode().getName(),node);
#endif

   TR_ClobberEvalData data;
   if (!self()->canClobberNodesRegister(node, 1, &data, ignoreRefCount) || force_copy)
      {
      char * CLOBBER_EVAL = "LR=Clobber_eval";
      if (node->getOpCode().isFloat())
         {
         TR::Register * targetRegister = self()->allocateSinglePrecisionRegister();
         cursor = generateRRInstruction(self(), TR::InstOpCode::LER, node, targetRegister, srcRegister);
         if (debugObj)
            {
            debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
            }

         return targetRegister;
         }
      else if (node->getOpCode().isDouble())
         {
         TR::Register * targetRegister = self()->allocateSinglePrecisionRegister();
         cursor = generateRRInstruction(self(), TR::InstOpCode::LDR, node, targetRegister, srcRegister);
         if (debugObj)
            {
            debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
            }

         return targetRegister;
         }
#ifdef J9_PROJECT_SPECIFIC
      else if (node->getDataType() == TR::DecimalLongDouble)
         {
         TR::Register * targetRegister = self()->allocateFPRegisterPair();
         cursor = generateRRInstruction(self(), TR::InstOpCode::LXR, node, targetRegister, srcRegister);
         if (debugObj)
            {
            debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
            }

         return targetRegister;
         }
#endif
      else if (node->getOpCode().isVector())
         {
         TR::Register * targetRegister = self()->allocateClobberableRegister(srcRegister);
         generateVRRaInstruction(self(), TR::InstOpCode::VLR, node, targetRegister, srcRegister);
         return targetRegister;
         }
      else
         {
         TR::Register * targetRegister = self()->allocateClobberableRegister(srcRegister);
         TR::InstOpCode::Mnemonic loadRegOpCode = TR::InstOpCode::getLoadRegOpCodeFromNode(self(), node);

         if (srcRegister->is64BitReg())
            {
            loadRegOpCode = TR::InstOpCode::LGR;
            }

         cursor = generateRRInstruction(self(), loadRegOpCode, node, targetRegister, srcRegister);

         if (debugObj)
            {
            debugObj->addInstructionComment( toS390RRInstruction(cursor), CLOBBER_EVAL);
            }

         return targetRegister;
         }
      }
   return srcRegister;
   }

/**
 * Different from evaluate in that it returns a clobberable register
 */
TR::Register *
OMR::Z::CodeGenerator::fprClobberEvaluate(TR::Node * node)
   {

   TR::Register *srcRegister = self()->evaluate(node);
   if (!self()->canClobberNodesRegister(node))
      {
      TR::Register * targetRegister;
      if (node->getDataType() == TR::Float
#ifdef J9_PROJECT_SPECIFIC
          || node->getDataType() == TR::DecimalFloat
#endif
          )
         {
         targetRegister = self()->allocateRegister(TR_FPR);
         generateRRInstruction(self(), TR::InstOpCode::LER, node, targetRegister, srcRegister);
         }
      else if (node->getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
               || node->getDataType() == TR::DecimalDouble
#endif
               )
         {
         targetRegister = self()->allocateRegister(TR_FPR);
         generateRRInstruction(self(), TR::InstOpCode::LDR, node, targetRegister, srcRegister);
         }
#ifdef J9_PROJECT_SPECIFIC
      else if (node->getDataType() == TR::DecimalLongDouble)
         {
         targetRegister = self()->allocateFPRegisterPair();
         generateRRInstruction(self(), TR::InstOpCode::LXR, node, targetRegister, srcRegister);
         }
#endif
      return targetRegister;
      }
   else
      {
      return self()->evaluate(node);
      }
   }

#ifdef J9_PROJECT_SPECIFIC
TR_OpaquePseudoRegister *
OMR::Z::CodeGenerator::ssrClobberEvaluate(TR::Node * node, TR::MemoryReference *sourceMR) // sourceMR can be NULL
   {
   TR::CodeGenerator *cg = self();
   bool isBCD = node->getType().isBCD();
   TR_OpaquePseudoRegister *srcRegister = cg->evaluateOPRNode(node);
   TR_PseudoRegister *bcdSrcRegister = NULL;
   if (isBCD)
      {
      bcdSrcRegister = srcRegister->getPseudoRegister();
      TR_ASSERT(bcdSrcRegister,"bcdSrcRegister should be non-NULL for bcd node %s (%p)\n",node->getOpCode().getName(),node);
      }

   TR_StorageReference *srcStorageReference = srcRegister->getStorageReference();

   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tneedsClobberEval : %s (%p), storageRef #%d (%p) (isBCD = %s, isReadOnlyTemp = %s, tempRefCount = %d)\n",
         node->getOpCode().getName(),node,srcStorageReference->getReferenceNumber(),srcStorageReference->getSymbol(),
         isBCD?"yes":"no",srcStorageReference->isReadOnlyTemporary()?"yes":"no",srcStorageReference->getTemporaryReferenceCount());

   bool needsClobberDueToRefCount = false;
   bool needsClobberDueToRegCount = false;
   bool needsClobberDueToReuse = false;
   if (node->getReferenceCount() > 1)
      {
      needsClobberDueToRefCount = true;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\t-->needsClobber=true due to node->refCount %d\n",node->getReferenceCount());
      }
   else if (srcStorageReference->getOwningRegisterCount() > 1)
      {
      needsClobberDueToRegCount = true;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\t-->needsClobber=true due to srcStorageReference #%d (%s) owningRegCount %d\n",
            srcStorageReference->getReferenceNumber(),self()->getDebug()->getName(srcStorageReference->getSymbol()),srcStorageReference->getOwningRegisterCount());
      }
   else if (srcStorageReference->isReadOnlyTemporary() &&
            srcStorageReference->getTemporaryReferenceCount() > 1)
      {
      needsClobberDueToReuse = true;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\t-->needsClobber=true due to reuse of readOnlyTemp %s with refCount %d > 1\n",
            self()->getDebug()->getName(srcStorageReference->getSymbol()),srcStorageReference->getTemporaryReferenceCount());
      // all these needsClobberDueToReuse cases should now be caught by the more general getOwningRegisterCount() > 1 above
      TR_ASSERT(false,"needsClobberDueToReuse should not be true for node %p\n",node);
      }
   else
      {
      if (self()->traceBCDCodeGen())
         {
         traceMsg(self()->comp(),"\t\t-->needsClobber=false, srcStorageReference ref #%d (%s) owningRegCount=%d)\n",
            srcStorageReference->getReferenceNumber(),
            self()->comp()->getDebug()->getName(srcStorageReference->getSymbol()),
            srcStorageReference->getOwningRegisterCount());
         }
      }

   if (needsClobberDueToRefCount || needsClobberDueToRegCount || needsClobberDueToReuse)
      {
      TR_ASSERT( srcRegister->isInitialized(),"expecting srcRegister to be initialized in bcdClobberEvaluate\n");
      // if the srcStorageReference is not a temp or a hint then the recursive dec in setStorageReference() will be wrong
      TR_ASSERT(srcStorageReference->isTemporaryBased() || srcStorageReference->isNodeBasedHint(),
            "expecting the srcStorageReference to be either a temporary or a node based hint in bcdClobberEvaluate\n");
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"ssrClobberEvaluate: node %s (%p) with srcReg %s (srcRegSize %d, srcReg->validSymSize %d, srcReg->liveSymSize %d) and srcStorageReference #%d with %s (symSize %d)\n",
            node->getOpCode().getName(),node,self()->getDebug()->getName(srcRegister),
            srcRegister->getSize(),srcRegister->getValidSymbolSize(),srcRegister->getLiveSymbolSize(),
            srcStorageReference->getReferenceNumber(),self()->getDebug()->getName(srcStorageReference->getSymbol()),srcStorageReference->getSymbolSize());

      int32_t byteLength = srcRegister->getValidSymbolSize();
      TR_StorageReference *copyStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(byteLength, self()->comp());
      copyStorageReference->setTemporaryReferenceCount(1);  // so the temp will be alive for the MVC below

      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgot copyStorageReference #%d with %s for clobberEvaluate\n",copyStorageReference->getReferenceNumber(),self()->getDebug()->getName(copyStorageReference->getSymbol()));

      if (isBCD)
         {
         if (sourceMR == NULL)
            sourceMR = generateS390RightAlignedMemoryReference(node, srcStorageReference, cg);
         else
            sourceMR = generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg);
         }
      else
         {
         if (sourceMR == NULL)
            sourceMR = generateS390MemRefFromStorageRef(node, srcStorageReference, cg);
         else
            sourceMR = generateS390MemoryReference(*sourceMR, 0, cg);
         }

      if (srcStorageReference->isReadOnlyTemporary() && srcRegister->getRightAlignedIgnoredBytes() > 0)
         {
         // The MVC must copy over the entire source field including any ignored (but still valid) bytes (but dead bytes are not included as these have been invalidated/clobbered).
         // The subsequent uses of this copied field (via copyStorageReference) will adjust based on the owning registers' ignoredBytes/deadBytes setting
         int32_t regIgnoredBytes = srcRegister->getRightAlignedIgnoredBytes();
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\tremove %d ignoredBytes from sourceMR->_offset (%d->%d) for readOnlyTemp #%d (%s)\n",
               regIgnoredBytes,sourceMR->getOffset(),sourceMR->getOffset()+regIgnoredBytes,srcStorageReference->getReferenceNumber(),self()->getDebug()->getName(srcStorageReference->getSymbol()));
         sourceMR->addToOffset(regIgnoredBytes);
         }

      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tgen MVC or memcpy sequence for copy with size %d\n",byteLength);

      TR::MemoryReference *copyMR = NULL;
      if (isBCD)
         copyMR = generateS390RightAlignedMemoryReference(node, copyStorageReference, cg, true, true); // enforceSSLimits=true, isNewTemp=true
      else
         copyMR = generateS390MemRefFromStorageRef(node, copyStorageReference, cg); // enforceSSLimits=true

      self()->genMemCpy(copyMR, node, sourceMR, node, byteLength);

      // the srcRegister->setStorageReference call will set the temp refCount to the final correct value based on the node's refCount
      copyStorageReference->setTemporaryReferenceCount(0);
      TR_OpaquePseudoRegister *targetRegister = bcdSrcRegister ? self()->allocatePseudoRegister(bcdSrcRegister) : self()->allocateOpaquePseudoRegister(srcRegister);
      targetRegister->setStorageReference(srcStorageReference, NULL);

      int32_t digitsInSrc = isBCD ? TR::DataType::getBCDPrecisionFromSize(node->getDataType(), byteLength) : 0;
      int32_t digitsToClear = srcRegister->getLeftAlignedZeroDigits() > 0 ? srcRegister->getDigitsToClear(0, digitsInSrc) : digitsInSrc;
      int32_t alreadyClearedDigits = digitsInSrc - digitsToClear;
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tupdate srcRegister %s with copy ref #%d (%s)\n",
            self()->getDebug()->getName(srcRegister),copyStorageReference->getReferenceNumber(),self()->getDebug()->getName(copyStorageReference->getSymbol()));
      srcRegister->setStorageReference(copyStorageReference, node); // clears leftAlignedZeroDigits so query getDigitsToClear above
      if (self()->traceBCDCodeGen() && alreadyClearedDigits)
         {
         traceMsg(self()->comp(),"\tsrcRegister %s had leftAlignedZeroDigits %d on original ref #%d\n",
            self()->getDebug()->getName(srcRegister),srcRegister->getLeftAlignedZeroDigits(),srcStorageReference->getReferenceNumber());
         traceMsg(self()->comp(),"\t\t* setting surviving leftAlignedZeroDigits %d on srcRegister %s with new copy ref #%d (digitsInSrc - digitsToClear = %d - %d = %d)\n",
           alreadyClearedDigits,self()->getDebug()->getName(srcRegister),copyStorageReference->getReferenceNumber(),digitsInSrc,digitsToClear,alreadyClearedDigits);
         }
      srcRegister->setLeftAlignedZeroDigits(alreadyClearedDigits);

      if (srcStorageReference->getNodesToUpdateOnClobber())
         {
         //    pd2zd
         // 3     pdcleanB regB
         // 3        pdcleanA regA
         // 2           pdaddA
         //
         //       AP    t1,t2 (pdadd)
         //       ZAP   t1,t1 mark t1 as readOnly, add pdcleanA (w/regA) to list (pdcleanA), increment t1 refCount by 2 (pdaddA nodeRefCount)
         //       nop         mark t1 as readOnly (redundant), add pdcleanB (w/regB) to list (pdcleanB), increment t1 refCount by 3 (pdcleanA nodeRefCount)
         //       UNPK  t3,t1 non-clobber operation, t1 is still alive and readOnly (pd2zd)
         //    ...
         //    pd2zd
         //       =>pdcleanA
         //
         //    UNPK t3,t1 non-clobber operation, t1 is still alive and readOnly
         //    ...
         //    pdaddB
         //       =>pdcleanB regB
         //
         //       MVC   t4,t1  clobber eval for pdadd parent, regB has other use so updated above, go through list and update regA with t4 (on clobber eval for pdaddB)
         //       AP    t1,tx
         //    ...
         //       =>pdcleanA regA   use t4
         //    ...
         //       =>pdcleanB regB   use t4
         //
         ListIterator<TR::Node> listIt(srcStorageReference->getNodesToUpdateOnClobber());
         for (TR::Node *listNode = listIt.getFirst(); listNode; listNode = listIt.getNext())
            {
            TR_OpaquePseudoRegister *listReg = listNode->getOpaquePseudoRegister();
            TR_ASSERT(listReg,"could not find a register for node %p\n",listNode);
            if (listReg != srcRegister)
               {
               if (listReg->getStorageReference() != srcStorageReference)
                  {
                  // in this case the listReg has already had its storageRef changed (likely by skipCopyOnStore/changeCommonedChildAddress in pdstoreEvaluator
                  // so skip this update (the reference counts of the address subtree will be wrong if the refCount is update here.
                  // See addsub2 first pdstore on line_no=335 with disableVIP)
                  //
                  // ipdstore p=29 symA (store then does a ZAP and updates child VTS_2 to symA)
                  //    aiuadd
                  //       aload
                  //       iconst
                  //    pdModPrecA   p=29 (passThrough so skip ssrClobber evaluate and reuse VTS_2 as a read only temp) 1 ref remaining to pdModPrecA
                  //       =>pdaddOverflowMessage p=31 (VTS_2)
                  //
                  //    ZAP symA,VTS_2
                  //
                  // ...
                  // ipdstore p=30
                  //    addr
                  //    pdModPrecB   p=30 (non-passThrough, needs NI, so needs clobberEval with copy ref VTS_3 and goes to update pdModPrecA symA -> VTS_3)
                  //       =>pdaddOverflowMessage p=31 (VTS_2)
                  //
                  // prevent the symA -> VTS_3 update as symA != the orig VTS_2
                  // if the update is done then the last pdModPrecA ref is now using VTS_3 and the final decReferenceCount of the symA aload is never done (live reg alive bug)
                  //
                  if (self()->traceBCDCodeGen())
                     traceMsg(self()->comp(),"\ty^y : skip update on reg %s node %s (%p), storageRef has already changed -- was #%d (%s), now #%d (%s)\n",
                        self()->getDebug()->getName(listReg),
                        listNode->getOpCode().getName(),listNode,
                        srcStorageReference->getReferenceNumber(),self()->getDebug()->getName(srcStorageReference->getSymbol()),
                        listReg->getStorageReference()->getReferenceNumber(),self()->getDebug()->getName(listReg->getStorageReference()->getSymbol()));
                  }
               else
                  {
                  if (self()->traceBCDCodeGen())
                     traceMsg(self()->comp(),"\ty^y : update reg %s with ref #%d (%s) on node %s (%p) refCount %d from _nodesToUpdateOnClobber with copy ref #%d (%s) (skip if refCount == 0)\n",
                        self()->getDebug()->getName(listReg),
                        listReg->getStorageReference()->getReferenceNumber(),self()->getDebug()->getName(listReg->getStorageReference()->getSymbol()),
                        listNode->getOpCode().getName(),listNode,listNode->getReferenceCount(),
                        copyStorageReference->getReferenceNumber(),self()->getDebug()->getName(copyStorageReference->getSymbol()));
                  if (listNode->getReferenceCount() > 0)
                     listReg->setStorageReference(copyStorageReference, listNode);      // TODO: safe to transfer alreadyClearedDigits in this case too?
                  }
               }
            }
         srcStorageReference->emptyNodesToUpdateOnClobber();
         }

      return targetRegister;
      }
   else
      {
      return srcRegister;
      }
   }
#endif

bool OMR::Z::CodeGenerator::useMVCLForMemcpyWithPad(TR::Node *node, TR_MemCpyPadTypes type)
   {
   if (type == TwoByte || type == ND_TwoByte) // two byte pad needed so MVCL is no good
      return false;

   bool useMVCL = false;

   TR::Node *targetLenNode = node->getChild(TR_MEMCPY_PAD_DST_LEN_INDEX);
   TR::Node *sourceLenNode = node->getChild(TR_MEMCPY_PAD_SRC_LEN_INDEX);
   TR::Node *maxLenNode = node->getChild(TR_MEMCPY_PAD_MAX_LEN_INDEX);

   int64_t maxLen = 0;
   if (maxLenNode->getOpCode().isLoadConst())
      maxLen = maxLenNode->get64bitIntegralValue();

   if (maxLen != 0 && maxLen < MVCL_THRESHOLD) // 0 is an unknown maxLength so have to be conservative and not use MVCL
      {
      useMVCL = true;
      }
   else if (targetLenNode->getOpCode().isLoadConst() && sourceLenNode->getOpCode().isLoadConst())
      {
      if (targetLenNode->get64bitIntegralValue() < MVCL_THRESHOLD &&
          sourceLenNode->get64bitIntegralValue() < MVCL_THRESHOLD)
         {
         useMVCL = true;
         }
      }
   return useMVCL;
   }

bool OMR::Z::CodeGenerator::isValidCompareConst(int64_t compareConst)
   {
   const int MIN_FOLDABLE_CC = 0;
   const int MAX_FOLDABLE_CC_VAL = 3;
   return (compareConst >= MIN_FOLDABLE_CC && compareConst <= MAX_FOLDABLE_CC_VAL);
   }

bool OMR::Z::CodeGenerator::isIfFoldable(TR::Node *node, int64_t compareConst)
   {
   if (!self()->isValidCompareConst(compareConst))
      return false;

   return (node->getOpCodeValue() == TR::bitOpMem);
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::apply12BitLabelRelativeRelocation
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::apply12BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label, bool isCheckDisp)
   {
    if (!isCheckDisp)
      {
      //keep the mask 4-12 bits
      *(int16_t *) cursor &= bos(0xf000);
      //update the relocation 12 - 24 bits
      //check for nop padding
      uint16_t * label_cursor = (uint16_t *) label->getCodeLocation();
      if ((*(uint16_t *) label_cursor) == bos(0x1800))
         label_cursor += 1;
      *(int16_t *) cursor |= (int16_t) (((intptr_t) label_cursor - (((intptr_t) cursor) - 1)) / 2);
      return;
      }
   int32_t disp = label->getCodeLocation() - self()->getFirstSnippet()->getSnippetLabel()->getCodeLocation();

   TR::Snippet * s = label->getSnippet();

   if ( s != NULL &&
         (s->getKind() == TR::Snippet::IsConstantData ||
          s->getKind() == TR::Snippet::IsWritableData ||
          s->getKind() == TR::Snippet::IsConstantInstruction )
      )
      {
      int32_t estOffset = s->getCodeBaseOffset();
      TR_ASSERT( estOffset == disp ,
         "apply12BitLabelRelativeRelocation -- [%p] disp (%d) did not equal estimated offset (%d)\n", s, disp, estOffset);
      }
   else
      {
      TR_ASSERT(0, "apply12BitLabelRelativeRelocation -- Unexpected use of 12bit rellocation by [%p]\n", label);
      }

   }

/**
 * The following method is used only for zOS 31 bit system/JNI linkage for producing the 4 byte NOP with last two bytes az
 * offset in double words to the calldescriptor
 *
 * field _instructionOffser is used for 16-bit relocation when we need to specify where relocation starts relative to the first byte of the instruction.
 * Eg.: BranchPreload instruction BPP (48-bit instruction), where 16-bit relocation is at 32-47 bits, apply16BitLabelRelativeRelocation(cursor,label,4,true)
 *
 * The regular OMR::Z::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label) only supports instructions with 16-bit relocations,
 * where the relocation is 2 bytes (hardcoded value) form the start of the instruction
 */
void
OMR::Z::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label, int8_t addressDifferenceDivisor, bool isInstrOffset)
   {
   if (isInstrOffset)
      {
      uint16_t * label_cursor = (uint16_t *) label->getCodeLocation();
      if ((*(uint16_t *) label_cursor) == bos(0x1800))
         label_cursor += 1;
      *(int16_t *) cursor = (int16_t) (((intptr_t) label_cursor - (((intptr_t) cursor) - addressDifferenceDivisor)) / 2);
      }
   else
      *(int16_t *) cursor = (int16_t) (((intptr_t) label->getCodeLocation() - (((intptr_t) cursor) - addressDifferenceDivisor)) / addressDifferenceDivisor);
   }

void
OMR::Z::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   *(int16_t *) cursor = (int16_t) (((intptr_t) label->getCodeLocation() - ((intptr_t) cursor - 2)) / 2);
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::apply32BitLabelRelativeRelocation
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   *(int32_t *) (((uint8_t *) cursor) + 2) = (int32_t) (((intptr_t) (label->getCodeLocation()) - ((intptr_t) cursor)) / 2);
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::apply32BitLabelTableRelocation
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::apply32BitLabelTableRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   *(uint32_t *) cursor = (intptr_t) label->getCodeLocation() - ((intptr_t) cursor - *(uint32_t *) cursor);
   }

/**
 * Get the first snippet entry. If there is at least one targetAddress entry,
 * this value will be returned.  If no targetAddress exists,  the first dataConstant
 * entry will be returned.
 */
TR::Snippet *
OMR::Z::CodeGenerator::getFirstSnippet()
   {
   TR::Snippet * s = NULL;

   if ((s = self()->getFirstConstantData()) != NULL)
      {
      return s;
      }
   else
      {
      return NULL;
      }
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::Constant Data List Functions
////////////////////////////////////////////////////////////////////////////////
TR_S390OutOfLineCodeSection * OMR::Z::CodeGenerator::findS390OutOfLineCodeSectionFromLabel(TR::LabelSymbol *label)
   {
   auto oiIterator = self()->getS390OutOfLineCodeSectionList().begin();
   while (oiIterator != self()->getS390OutOfLineCodeSectionList().end())
      {
      if ((*oiIterator)->getEntryLabel() == label)
         return *oiIterator;
      ++oiIterator;
      }

   return NULL;
   }
////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::Constant Data List Functions
////////////////////////////////////////////////////////////////////////////////
TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::findOrCreateConstant(TR::Node * node, void * c, uint16_t size)
   {
   CS2::HashIndex hi;
   TR_S390ConstantDataSnippetKey key;
   key.c      = c;
   key.size   = size;
   TR::S390ConstantDataSnippet * data;

   // Can only share data snippets for literal pool address when inlined site indices are the same
   // for now conservatively create a new literal pool data snippet per unique node

   TR::SymbolReference * symRef = (node && node->getOpCode().hasSymbolReference()) ? node->getSymbolReference() : NULL;
   TR::Symbol * symbol = (symRef) ? symRef->getSymbol() : NULL;
   bool isStatic = (symbol) ? symbol->isStatic() && !symRef->isUnresolved() : false;
   bool isLiteralPoolAddress = isStatic && !node->getSymbol()->isNotDataAddress();

   // cannot share class and method pointers in AOT compilations because
   // the pointers may be used for different purposes and need to be
   // relocated independently (via different relocation records)
   if (self()->profiledPointersRequireRelocation() && node &&
       (node->getOpCodeValue() == TR::aconst && (node->isClassPointerConstant() || node->isMethodPointerConstant()) ||
        node->getOpCodeValue() == TR::loadaddr && node->getSymbol()->isClassObject() ||
        isLiteralPoolAddress))
      key.node = node;
   else
      key.node = NULL;

   if(_constantHash.Locate(key,hi))
      {
      data = _constantHash.DataAt(hi);
      if (data)
         return data;
      }

   // Constant was not found: create a new snippet for it and add it to the constant list.
   //
   data = new (self()->trHeapMemory()) TR::S390ConstantDataSnippet(self(), node, c, size);
   key.c = (void *)data->getRawData();
   key.size = size;
   if (self()->profiledPointersRequireRelocation() && node &&
       (node->getOpCodeValue() == TR::aconst && (node->isClassPointerConstant() || node->isMethodPointerConstant()) ||
        node->getOpCodeValue() == TR::loadaddr && node->getSymbol()->isClassObject() ||
        isLiteralPoolAddress))
      key.node = node;
   else
      key.node = NULL;
   _constantHash.Add(key,data);
   return data;
   }

TR::S390ConstantInstructionSnippet *
OMR::Z::CodeGenerator::createConstantInstruction(TR::Node *node, TR::Instruction * instr)
   {
   TR::S390ConstantInstructionSnippet * cis = new (self()->trHeapMemory()) TR::S390ConstantInstructionSnippet(self(),node,instr);
   _constantList.push_front(cis);
   return cis;
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::CreateConstant(TR::Node * node, void * c, uint16_t size, bool writable)
   {

   if (writable)
      {
      TR::S390WritableDataSnippet * cursor;
      cursor = new (self()->trHeapMemory()) TR::S390WritableDataSnippet(self(), node, c, size);
      _writableList.push_front(cursor);
      return (TR::S390ConstantDataSnippet *) cursor;
      }
   else
      {
      TR::S390ConstantDataSnippet * cursor;
      cursor = new (self()->trHeapMemory()) TR::S390ConstantDataSnippet(self(), node, c, size);
      TR_S390ConstantDataSnippetKey key;
      key.c = (void *)cursor->getRawData();
      key.size = size;
      _constantHash.Add(key,cursor);
      return cursor;
      }
   }

void
OMR::Z::CodeGenerator::addDataConstantSnippet(TR::S390ConstantDataSnippet * snippet)
   {
   _snippetDataList.push_front(snippet);
   }

/**
 * Estimate the offsets for constant snippets from code base
 */
int32_t
OMR::Z::CodeGenerator::setEstimatedOffsetForConstantDataSnippets()
   {
   TR::S390ConstantDataSnippet * cursor;
   bool first;
   int32_t size;
   int32_t offset = 0;
   int32_t exp;

   // We align this stucture on 8bytes to guarantee full predictability of
   // of the lit pool layout.
   //
   offset = (offset + 7) / 8 * 8;

   // Assume constants will be emitted in order of decreasing size.  Constants should be aligned
   // according to their size.
   //
   // Constant snippet size (S) is a power of 2
   //     - handle when: S = pos(2,exp)
   //     - snippets alignment by size, no max
   #define HANDLE_CONSTANT_SNIPPET(cursor, pow2Size) \
                      (cursor->getConstantSize() == pow2Size)

   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto iterator = _constantList.begin(); iterator != _constantList.end(); ++iterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*iterator), size))
            {
            if (first)
               {
               first = false;
               offset = (int32_t) (((offset + size - 1) / size) * size);
               }
            (*iterator)->setCodeBaseOffset(offset);
            offset += (*iterator)->getLength(0);
            }
         }
      CS2::HashIndex hi;
      for(hi = _constantHashCur.SetToFirst(); _constantHashCur.Valid(); hi = _constantHashCur.SetToNext())
          {
           cursor = _constantHash.DataAt(hi);
           if (HANDLE_CONSTANT_SNIPPET(cursor, size))
              {
              if (first)
                  {
                   first = false;
                   offset = (int32_t) (((offset + size - 1)/size)*size);
                  }
                cursor->setCodeBaseOffset(offset);
                offset += cursor->getLength(0);
               }
           }
        }

   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto writeableiterator = _writableList.begin(); writeableiterator != _writableList.end(); ++writeableiterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*writeableiterator), size))
            {
            if (first)
               {
               first = false;
               offset = (int32_t) (((offset + size - 1) / size) * size);
               }
            (*writeableiterator)->setCodeBaseOffset(offset);
            offset += (*writeableiterator)->getLength(0);
            }
         }
      }

   return offset;
   }

int32_t
OMR::Z::CodeGenerator::setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart)
   {
   TR::S390ConstantDataSnippet * cursor;
   bool first;
   int32_t size;
   int32_t exp;

   // Conservatively estimate the padding necessary to get to 8 byte alignment from end of snippet
   // Need this padding since the alignment of our estimate and our real addresses may be different.
   estimatedSnippetStart += 6;

   // We align this stucture on 8bytes to guarantee full predictability of
   // of the lit pool layout.
   estimatedSnippetStart = (estimatedSnippetStart + 7) / 8 * 8;

   // Assume constants will be emitted in order of decreasing size.  Constants should be aligned
   // according to their size.
   //

   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto iterator = _constantList.begin(); iterator != _constantList.end(); ++iterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*iterator), size))
            {
            if (first)
               {
               first = false;
               estimatedSnippetStart =  ((estimatedSnippetStart + size - 1) / size) * size;
               }
            (*iterator)->getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart);
            estimatedSnippetStart += (*iterator)->getLength(estimatedSnippetStart);
            }
         }
      CS2::HashIndex hi;
      for(hi = _constantHashCur.SetToFirst(); _constantHashCur.Valid(); hi = _constantHashCur.SetToNext())
         {
           cursor = _constantHash.DataAt(hi);
           if (HANDLE_CONSTANT_SNIPPET(cursor, size))
              {
               if (first)
                   {
                    first = false;
                    estimatedSnippetStart =  ((estimatedSnippetStart + size - 1) / size) * size;
                   }
               cursor->getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart);
               estimatedSnippetStart += cursor->getLength(estimatedSnippetStart);
              }
          }
      }

   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto writeableiterator = _writableList.begin(); writeableiterator != _writableList.end(); ++writeableiterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*writeableiterator), size))
            {
            if (first)
               {
               first = false;
               estimatedSnippetStart = ((estimatedSnippetStart + size - 1) / size) * size;
               }
            (*writeableiterator)->getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart);
            estimatedSnippetStart += (*writeableiterator)->getLength(estimatedSnippetStart);
            }
         }
      }
   // Emit Other Misc Data Snippets.
   estimatedSnippetStart = ((estimatedSnippetStart + 7) / 8 * 8);
   for (auto snippetDataIterator = _snippetDataList.begin(); snippetDataIterator != _snippetDataList.end(); ++snippetDataIterator)
      {
      (*snippetDataIterator)->getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart);
      estimatedSnippetStart += (*snippetDataIterator)->getLength(estimatedSnippetStart);
      }
   return estimatedSnippetStart;
   }

void
OMR::Z::CodeGenerator::emitDataSnippets()
   {
   // If you change logic here, be sure to do similar change in
   // the method : TR::S390ConstantDataSnippet *OMR::Z::CodeGenerator::getFirstConstantData()

   TR_ConstHashCursor constCur(_constantHash);
   TR::S390ConstantDataSnippet * cursor;
   TR::S390EyeCatcherDataSnippet * eyeCatcher = NULL;
   uint8_t * codeOffset;
   bool first;
   int32_t size;
   int32_t maxSize = 0;
   int32_t exp;

   // We align this stucture on 8bytes to guarantee full predictability of
   // of the lit pool layout.
   //
   self()->setBinaryBufferCursor((uint8_t *) (((uintptr_t) (self()->getBinaryBufferCursor() + 7) / 8) * 8));

   // Emit constants in order of decreasing size.  Constants will be aligned according to
   // their size.
   //

   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto iterator = _constantList.begin(); iterator != _constantList.end(); ++iterator)
         {
         if (maxSize < (*iterator)->getConstantSize())
            maxSize = size;

         if (HANDLE_CONSTANT_SNIPPET((*iterator), size))
            {
            if (first)
               {
               first = false;
               self()->setBinaryBufferCursor((uint8_t *) (((uintptr_t) (self()->getBinaryBufferCursor() + size - 1) / size) * size));
               }
            codeOffset = (*iterator)->emitSnippetBody();
            if (codeOffset != NULL)
               {
               self()->setBinaryBufferCursor(codeOffset);
               }
            }
         }
      CS2::HashIndex hi;
      for (hi = _constantHashCur.SetToFirst(); _constantHashCur.Valid(); hi = _constantHashCur.SetToNext())
          {
          cursor = _constantHash.DataAt(hi);

          if (maxSize < cursor->getConstantSize())
             maxSize = size;

          if (HANDLE_CONSTANT_SNIPPET(cursor, size))
             {
              if (first)
                 {
                  first = false;
                  self()->setBinaryBufferCursor((uint8_t *) (((uintptr_t)(self()->getBinaryBufferCursor() + size -1)/size)*size));
                 }
             codeOffset = cursor->emitSnippetBody();
             if (codeOffset != NULL)
                 {
                 self()->setBinaryBufferCursor(codeOffset);
                 }
             }
          }
      }

   // Emit writable data in order of decreasing size.  Constants will be aligned according to
   // their size.
   //
   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      first = true;
      for (auto writeableiterator = _writableList.begin(); writeableiterator != _writableList.end(); ++writeableiterator)
         {
       if (maxSize < (*writeableiterator)->getConstantSize())
            maxSize = size;

         if (HANDLE_CONSTANT_SNIPPET((*writeableiterator), size))
            {
            if (first)
               {
               first = false;
               self()->setBinaryBufferCursor((uint8_t *) (((uintptr_t) (self()->getBinaryBufferCursor() + size - 1) / size) * size));
               }
            codeOffset = (*writeableiterator)->emitSnippetBody();
            if (codeOffset != NULL)
               {
               self()->setBinaryBufferCursor(codeOffset);
               }
            }
         }
      }
   // Check that we didn't start at too low an exponent
   TR_ASSERT(1 << self()->constantDataSnippetExponent() >= maxSize, "Failed to emit 1 or more snippets of size %d\n", maxSize);

   // Emit Other Misc Data Snippets.
   self()->setBinaryBufferCursor((uint8_t *) (((uintptr_t) (self()->getBinaryBufferCursor() + 7) / 8) * 8));
   for (auto snippetDataIterator = _snippetDataList.begin(); snippetDataIterator != _snippetDataList.end(); ++snippetDataIterator)
      {
      if ((*snippetDataIterator)->getKind() == TR::Snippet::IsEyeCatcherData )
         {
         eyeCatcher = (TR::S390EyeCatcherDataSnippet *)(*snippetDataIterator);
         }
      else
         {
         codeOffset = (*snippetDataIterator)->emitSnippetBody();
         if (codeOffset != NULL)
            {
            self()->setBinaryBufferCursor(codeOffset);
            }
         }
      }

  //this is old style of eye catcher and not used any more
   if (eyeCatcher != NULL) //WCODE
      {
      // Emit EyeCatcher at the end
      codeOffset = eyeCatcher->emitSnippetBody();
      if (codeOffset != NULL)
         {
         self()->setBinaryBufferCursor(codeOffset);
         }
      }
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::create64BitLiteralPoolSnippet(TR::DataType dt, int64_t value)
   {
   TR_ASSERT( dt == TR::Int64, "create64BitLiteralPoolSnippet is only for data constants\n");

   return self()->findOrCreate8ByteConstant(0, value);
   }

TR::Linkage *
OMR::Z::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage * linkage;
   TR::Compilation *comp = self()->comp();
   switch (lc)
      {
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) TR::S390zLinuxSystemLinkage(self());
         break;

      case TR_Private:
        // no private linkage, fall through to system

      case TR_System:
         if (self()->comp()->target().isLinux())
            linkage = new (self()->trHeapMemory()) TR::S390zLinuxSystemLinkage(self());
         else
            linkage = new (self()->trHeapMemory()) TR::S390zOSSystemLinkage(self());
         break;

      default :
         TR_ASSERT(0, "\nTestarossa error: Illegal linkage convention %d\n", lc);
      }
   self()->setLinkage(lc, linkage);
   return linkage;
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::createLiteralPoolSnippet(TR::Node * node)
   {
   TR::S390ConstantDataSnippet * targetSnippet = NULL;

   // offset in the symbol reference points to the constant node
   // get this constant node
   TR::Node *constNode=(TR::Node *) (node->getSymbolReference()->getOffset());

   int64_t value = 0;
   switch (constNode->getDataType())
      {
      case TR::Int8:
         value = constNode->getByte();
         targetSnippet = self()->findOrCreate4ByteConstant(0, value);
         break;
      case TR::Int16:
         value = constNode->getShortInt();
         targetSnippet = self()->findOrCreate4ByteConstant(0, value);
         break;
      case TR::Int32:
         value = constNode->getInt();
         targetSnippet = self()->findOrCreate4ByteConstant(0, value);
         break;
      case TR::Int64:
         value = constNode->getLongInt();
         targetSnippet = self()->create64BitLiteralPoolSnippet(TR::Int64, value);
         break;
      case TR::Address:
         value = constNode->getAddress();
         if (node->isClassUnloadingConst())
            {
            TR_ASSERT(0, "Shouldn't come here, isClassUnloadingConst is done in aloadHelper");
            if (self()->comp()->target().is64Bit())
               {
               targetSnippet = self()->Create8ByteConstant(node, value, true);
               }
            else
               {
               targetSnippet = self()->Create4ByteConstant(node, value, true);
               }
            if (node->isMethodPointerConstant())
               self()->comp()->getMethodSnippetsToBePatchedOnClassUnload()->push_front(targetSnippet);
            else
               self()->comp()->getSnippetsToBePatchedOnClassUnload()->push_front(targetSnippet);
            }
         else
            {
            if (self()->comp()->target().is64Bit())
               {
               targetSnippet = self()->findOrCreate8ByteConstant(0, value);
               }
            else
               {
               targetSnippet = self()->findOrCreate4ByteConstant(0, value);
               }
            }
         break;
      case TR::Float:
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalFloat:
#endif
         union
            {
            float fvalue;
            int32_t ivalue;
            } fival;
         fival.fvalue = constNode->getFloat();
         targetSnippet = self()->findOrCreate4ByteConstant(0, fival.ivalue);
         break;
      case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalDouble:
#endif
         union
            {
            double dvalue;
            int64_t lvalue;
            } dlval;
         dlval.dvalue = constNode->getDouble();
         targetSnippet = self()->findOrCreate8ByteConstant(0, dlval.lvalue);
         break;
      default:
         TR_ASSERT(0, "createLiteralPoolSnippet: Unknown type.\n");
         break;
      }
   return targetSnippet;
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::findOrCreate2ByteConstant(TR::Node * node, int16_t c, bool isWarm)
   {
   return self()->findOrCreateConstant(node, &c, 2);
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::findOrCreate4ByteConstant(TR::Node * node, int32_t c, bool isWarm)
   {
   return self()->findOrCreateConstant(node, &c, 4);
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::findOrCreate8ByteConstant(TR::Node * node, int64_t c, bool isWarm)
   {
   return self()->findOrCreateConstant(node, &c, 8);
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::Create4ByteConstant(TR::Node * node, int32_t c, bool writable)
   {
   return self()->CreateConstant(node, &c, 4, writable);
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::Create8ByteConstant(TR::Node * node, int64_t c, bool writable)
   {
   return self()->CreateConstant(node, &c, 8, writable);
   }

TR::S390WritableDataSnippet *
OMR::Z::CodeGenerator::CreateWritableConstant(TR::Node * node)
   {
   if (self()->comp()->target().is64Bit())
      {
      return (TR::S390WritableDataSnippet *) self()->Create8ByteConstant(node, 0, true);
      }
   else
      {
      return (TR::S390WritableDataSnippet *) self()->Create4ByteConstant(node, 0, true);
      }
   }

TR::S390ConstantDataSnippet *
OMR::Z::CodeGenerator::getFirstConstantData()
   {
   // Logic in this method should always be kept in sync with
   // logic in void OMR::Z::CodeGenerator::emitDataSnippets()
   // Constants are emited in order of decreasing size,
   // hence the first emitted constant may not be iterator.getFirst(),
   // but the first constant with biggest size
   //
   TR::S390ConstantDataSnippet * cursor;
   TR_ConstHashCursor constCur(_constantHash);
   int32_t exp;

   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      int32_t size = 1 << exp;
      for (auto iterator = _constantList.begin(); iterator != _constantList.end(); ++iterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*iterator), size))
            {
            return *iterator;
            }
         }
      CS2::HashIndex hi;
      for (hi = _constantHashCur.SetToFirst(); _constantHashCur.Valid(); hi = _constantHashCur.SetToNext())
          {
          cursor = _constantHash.DataAt(hi);
          if (HANDLE_CONSTANT_SNIPPET(cursor, size))
              {
              return cursor;
              }
          }
      }
   cursor = NULL;
   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      int32_t size = 1 << exp;
      for (auto writeableiterator = _writableList.begin(); writeableiterator != _writableList.end(); ++writeableiterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*writeableiterator), size))
            {
            return *writeableiterator;
            }
         }
      }

   return cursor;
   }

void
OMR::Z::CodeGenerator::freeAndResetTransientLongs()
   {
   int32_t num_lReg;

   for (num_lReg=0; num_lReg < _transientLongRegisters.size(); num_lReg++)
      self()->stopUsingRegister(_transientLongRegisters[num_lReg]);
   _transientLongRegisters.setSize(0);
   }

TR_S390OutOfLineCodeSection *
OMR::Z::CodeGenerator::findOutLinedInstructionsFromLabel(TR::LabelSymbol *label)
   {
   auto oiIterator = self()->getS390OutOfLineCodeSectionList().begin();
   while (oiIterator != self()->getS390OutOfLineCodeSectionList().end())
      {
      if ((*oiIterator)->getEntryLabel() == label)
         return *oiIterator;
      ++oiIterator;
      }

   return NULL;
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::buildRegisterMapForInstruction
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::buildRegisterMapForInstruction(TR_GCStackMap * map)
   {
   // Build the register map
   //
   TR_InternalPointerMap * internalPtrMap = NULL;
   TR::GCStackAtlas * atlas = self()->getStackAtlas();

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
      {
      TR::RealRegister * realReg = self()->machine()->getRealRegister((TR::RealRegister::RegNum) i);
      if (realReg->getHasBeenAssignedInMethod())
         {
         TR::Register * virtReg = realReg->getAssignedRegister();
         if (virtReg)
            {
            if (virtReg->containsInternalPointer())
               {
               if (!internalPtrMap)
                  {
                  internalPtrMap = new (self()->trHeapMemory()) TR_InternalPointerMap(self()->trMemory());
                  }
               internalPtrMap->addInternalPointerPair(virtReg->getPinningArrayPointer(), i);
               atlas->addPinningArrayPtrForInternalPtrReg(virtReg->getPinningArrayPointer());
               }
            else if (virtReg->containsCollectedReference())
               {
               map->setRegisterBits(self()->registerBitMask(i));
               }
            }
         }
      }

   map->setInternalPointerMap(internalPtrMap);
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::dumpDataSnippets
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::dumpDataSnippets(TR::FILE *outFile)
   {

   if (outFile == NULL)
      {
      return;
      }
   TR_ConstHashCursor constCur(_constantHash);
   TR::S390EyeCatcherDataSnippet * eyeCatcher = NULL;
   TR::S390ConstantDataSnippet *cursor = NULL;
   int32_t size;
   int32_t exp;

   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      for (auto iterator = _constantList.begin(); iterator != _constantList.end(); ++iterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*iterator), size))
            {
            self()->getDebug()->print(outFile, *iterator);
            }
         }
      CS2::HashIndex hi;
      for (hi = _constantHashCur.SetToFirst(); _constantHashCur.Valid(); hi = _constantHashCur.SetToNext())
          {
            cursor = _constantHash.DataAt(hi);
            if (HANDLE_CONSTANT_SNIPPET(cursor, size))
                {
                self()->getDebug()->print(outFile,cursor);
                }
          }
      }
   for (exp = self()->constantDataSnippetExponent(); exp > 0; exp--)
      {
      size = 1 << exp;
      for (auto writeableiterator = _writableList.begin(); writeableiterator != _writableList.end(); ++writeableiterator)
         {
         if (HANDLE_CONSTANT_SNIPPET((*writeableiterator), size))
            {
            self()->getDebug()->print(outFile, *writeableiterator);
            }
         }
      }

   // Emit Other Misc Data Snippets.
   for (auto snippetDataIterator = _snippetDataList.begin(); snippetDataIterator != _snippetDataList.end(); ++snippetDataIterator)
      {
      if((*snippetDataIterator)->getKind() == TR::Snippet::IsEyeCatcherData)
         eyeCatcher = (TR::S390EyeCatcherDataSnippet *)(*snippetDataIterator);
      else
         self()->getDebug()->print(outFile,*snippetDataIterator);
      }

   if (eyeCatcher != NULL) //WCODE
      {
      self()->getDebug()->print(outFile, eyeCatcher);
      }
   }

#if DEBUG
////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::printRegisterMap
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::dumpPreGPRegisterAssignment(TR::Instruction * instructionCursor)
   {
   if (self()->comp()->getOutFile() == NULL)
      {
      return;
      }

#if TODO
   if (instructionCursor->totalReferencedGPRegisters() > 0)
      {
      TR_FrontEnd * fe = comp()->fe();
      trfprintf(comp()->getOutFile(), "\n<< Pre-GPR assignment for instruction: %p", instructionCursor);
      getDebug()->print(comp()->getOutFile(), instructionCursor);
      getDebug()->printReferencedRegisterInfo(comp()->getOutFile(), instructionCursor);

      if (debug("dumpGPRegStatus"))
         {
         getDebug()->printGPRegisterStatus(comp()->getOutFile(), machine());
         }
      }
#else
   if (debug("dumpGPRegStatus"))
      {
      self()->getDebug()->print(self()->comp()->getOutFile(), instructionCursor);
      self()->getDebug()->printGPRegisterStatus(self()->comp()->getOutFile(), self()->machine());
      }
#endif
   }

/**
 * Dump the current instruction with the GP registers assigned and any new
 * instructions that may have been added after it.
 */
void
OMR::Z::CodeGenerator::dumpPostGPRegisterAssignment(TR::Instruction * instructionCursor, TR::Instruction * origNextInstruction)
   {
   if (self()->comp()->getOutFile() == NULL)
      {
      return;
      }

#if TODO
   if (instructionCursor->totalReferencedGPRegisters() > 0)
      {
      TR_FrontEnd * fe = comp()->fe();
      trfprintf(comp()->getOutFile(), "\n>> Post-GPR assignment for instruction: %p", instructionCursor);

      TR::Instruction * nextInstruction = instructionCursor;

      while (nextInstruction && (nextInstruction != origNextInstruction))
         {
         getDebug()->print(comp()->getOutFile(), nextInstruction);
         nextInstruction = nextInstruction->getNext();
         }

      getDebug()->printReferencedRegisterInfo(comp()->getOutFile(), instructionCursor);

      if (debug("dumpGPRegStatus"))
         {
         getDebug()->printGPRegisterStatus(comp()->getOutFile(), machine());
         }
      }
#else
   if (debug("dumpGPRegStatus"))
      {
      self()->getDebug()->print(self()->comp()->getOutFile(), instructionCursor);
      self()->getDebug()->printGPRegisterStatus(self()->comp()->getOutFile(), self()->machine());
      }
#endif
   }
#endif

void
OMR::Z::CodeGenerator::setGlobalStaticBaseRegisterOnFlag()
   {
   if (self()->comp()->getOption(TR_DisableGlobalStaticBaseRegister))
      self()->setGlobalStaticBaseRegisterOn(false);
   }

/**
 * check to see if we can safely generate BRASL or LARL for performace reason;
 * although we don't know the exact binary instruction address yet -- we can use
 * the jit code cache range(which is available now) to see if the target address is reachable.
 * If it is reachable from the start AND the end of the code cache, then we are sure it's reachable.
 * Otherwise we'll generate the slow path.
 * This works quite well for BRASL in that it will generate the good code for
 * jit-to-jit(for single code cache case) call and usually to the fe helper calls
 * as well.
 * Also note that: the immediate value in BRASL/LARL instr. is calculated as:
 *                i2=(targetAddress-currentAddress)/2
 *
 * As discovered through a failed test case, the target address is calculated as
 * follows:
 *                targetAddress = current + (int) 2* i2
 * The cast to int restricts the 2 *i2 to the 32bit signed integer range, instead
 * of i2 being in the range.  As a result, we need to make sure 2*i2 is within
 * 32bit signed integer range instead.
 *
 * With multicode-cache support on zLinux64, we always assume that we can use
 * relative long instruction.  The size of a code cache is restricted by the
 * longest relative branch instruction, which in this case is the relative
 * long BRASL.  However, we only return true for BRASL, which is checked
 * in GenerateInstructions.cpp::generateDirectCall.
 */
bool
OMR::Z::CodeGenerator::canUseRelativeLongInstructions(int64_t value)
   {
   if ((value < 0) || (value % 2 != 0)) return false;

   if (self()->comp()->target().isLinux())
      {
      intptr_t codeCacheBase = (intptr_t)(self()->getCodeCache()->getCodeBase());
      intptr_t codeCacheTop = (intptr_t)(self()->getCodeCache()->getCodeTop());

      return ( (((intptr_t)value - codeCacheBase ) <=  (intptr_t)(INT_MAX))
            && (((intptr_t)value - codeCacheBase ) >=  (intptr_t)(INT_MIN))
            && (((intptr_t)value - codeCacheTop ) <=  (intptr_t)(INT_MAX))
            && (((intptr_t)value - codeCacheTop ) >=  (intptr_t)(INT_MIN)) );
      }
   else
      {
      return (intptr_t)value<=(intptr_t)(INT_MAX) && (intptr_t)value>=(intptr_t)(INT_MIN);
      }
   }

bool
OMR::Z::CodeGenerator::supportsOnDemandLiteralPool()
   {
   if (!self()->comp()->getOption(TR_DisableOnDemandLiteralPoolRegister))
      {
      return true;
      }
   else
      {
      return false;
      }
   }

TR::Register *
OMR::Z::CodeGenerator::evaluateLengthMinusOneForMemoryOps(TR::Node *node, bool clobberEvaluate, bool &lenMinusOne)
   {
   TR::Register *reg;

   if ((node->getReferenceCount() == 1) && self()->supportsLengthMinusOneForMemoryOpts() &&
      ((node->getOpCodeValue()==TR::iadd &&
       node->getSecondChild()->getOpCodeValue()==TR::iconst &&
       node->getSecondChild()->getInt() == 1) ||
       (node->getOpCodeValue()==TR::isub &&
       node->getSecondChild()->getOpCodeValue()==TR::iconst &&
       node->getSecondChild()->getInt() == -1)) &&
      (node->getRegister() == NULL))
       {
       if (clobberEvaluate)
          reg = self()->gprClobberEvaluate(node->getFirstChild());
       else
          reg = self()->evaluate(node->getFirstChild());
       lenMinusOne=true;

       self()->decReferenceCount(node->getFirstChild());
       self()->decReferenceCount(node->getSecondChild());
       }
   else
      {
       if (clobberEvaluate)
          reg = self()->gprClobberEvaluate(node);
       else
          reg = self()->evaluate(node);
      }
   return reg;
   }

TR_GlobalRegisterNumber
OMR::Z::CodeGenerator::getGlobalRegisterNumber(uint32_t realRegNum)
   {
   return self()->machine()->getGlobalReg((TR::RealRegister::RegNum)(realRegNum+1));
   }

bool
OMR::Z::CodeGenerator::bitwiseOpNeedsLiteralFromPool(TR::Node *parent, TR::Node *child)
   {
// Immediate value optimization work: casingc

// LL: Enable this code and add Golden Eagle support
// #if 0

   if (child->getOpCode().isRef()) return true;

   if (child->getOpCodeValue() == TR::lconst)
      {
      // 64-bit value
      int64_t value = getIntegralValue(child);

      // Could move masks to a common header file
      const int64_t hhMask = CONSTANT64(0x0000FFFFFFFFFFFF);
      const int64_t hlMask = CONSTANT64(0xFFFF0000FFFFFFFF);
      const int64_t lhMask = CONSTANT64(0xFFFFFFFF0000FFFF);
      const int64_t llMask = CONSTANT64(0xFFFFFFFFFFFF0000);
      // LL: Mask for high and low 32 bits
      const int64_t highMask = CONSTANT64(0x00000000FFFFFFFF);
      const int64_t lowMask  = CONSTANT64(0xFFFFFFFF00000000);

      if (parent->getOpCode().isOr())
         {
         // We can use OIHF or OILF instruction if long constant value matches the masks
         if (!(value & highMask) || !(value & lowMask))
            {
            // value = 0x********00000000 or value = 0x00000000********
            return false;
            }
         else
            {
            return true;
            }
         }
      if (parent->getOpCode().isAnd())
         {
         // We can use NIHF or NILF instruction if long constant value matches the masks
         if ((value & highMask) == highMask || (value & lowMask) == lowMask)
            {
            // value = 0x********FFFFFFFF or value =  0xFFFFFFFF********
            return false;
            }
         else
            {
            return true;
            }
         }

      // Long constant value matches the mask
      if (parent->getOpCode().isXor() && (!(value & highMask) || !(value & lowMask)))
         {
         // value = 0x********00000000 or value = 0x00000000********
         return false;
         }
      }
   else
      {
      // 32bit value
      int32_t value = getIntegralValue(child);
      if (parent->getOpCode().isOr())
         {
         return false;
         }

      if (parent->getOpCode().isAnd())
         {
         return false;
         }

      if (parent->getOpCode().isXor())
         {
         return false;
         }
      }

   return true;
   }

TR::SymbolReference* OMR::Z::CodeGenerator::allocateReusableTempSlot()
   {
   TR_ASSERT( _cgFlags.testAny(S390CG_reusableSlotIsFree),"The reusable temp slot was already allocated but not freed\n");

   // allocate the reusable slot
   _cgFlags.set(S390CG_reusableSlotIsFree, false);
   if(_reusableTempSlot == NULL)
      _reusableTempSlot = self()->comp()->getSymRefTab()->createTemporary(self()->comp()->getMethodSymbol(), TR::Int64);

   return _reusableTempSlot;
   }

void OMR::Z::CodeGenerator::freeReusableTempSlot()
   {
      _cgFlags.set(S390CG_reusableSlotIsFree, true);
   }

/**
 * Check if CC of previous compare op is still valid;
 * currently we only deal with
 *  - compare R1,R2      (int or fp types)
 */
bool OMR::Z::CodeGenerator::isActiveCompareCC(TR::InstOpCode::Mnemonic opcd, TR::Register* tReg, TR::Register* sReg)
   {
   if (self()->hasCCInfo() && self()->hasCCCompare())
      {
      TR::Instruction* ccInst = self()->ccInstruction();
      TR::Register* ccTgtReg = ccInst->tgtRegArrElem(0);
      // this is the case we try to remove Compare of two registers, either FPRs or GPRs;
      // we will match CCInstr that has identical opcde and same register operands
      TR::Register* ccSrcReg = ccInst->srcRegArrElem(0);

      // On z10 trueCompElimination may swap the previous compare operands, so give up early
      if (self()->comp()->target().cpu.getSupportsArch(TR::CPU::z10) &&
          !self()->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
         {
         if (tReg->getKind() != TR_FPR)
            {
            return false;
            }
         }

      if (opcd == ccInst->getOpCodeValue() && tReg == ccTgtReg &&  sReg == ccSrcReg &&
          performTransformation(self()->comp(), "O^O isActiveCompareCC: RR Compare Op [%s\t %s, %s] can reuse CC from ccInstr [%p]\n",
             ccInst->getOpCode().getMnemonicName(), self()->getDebug()->getName(tReg),self()->getDebug()->getName(sReg), ccInst))
         {
         return true;
         }
      }
   return false;
   }

/**
 * Check if an arithmetic op CC results matches CC of a compare immediate op
 */
bool OMR::Z::CodeGenerator::isActiveArithmeticCC(TR::Register* tstReg)
   {
   if (self()->hasCCInfo() && self()->hasCCSigned() && !self()->hasCCOverflow() && !self()->hasCCCarry() &&
         performTransformation(self()->comp(), "O^O Arithmetic CC: CCInfo(%d) CCSigned(%d) CCOverflow(%d) CCCarry(%d)\n",
            self()->hasCCInfo(), self()->hasCCSigned(), self()->hasCCOverflow(), self()->hasCCCarry())
      )
      {
      TR::Instruction* inst = self()->ccInstruction();
      TR::Register* tgtReg = inst->tgtRegArrElem(0);
      dumpOptDetails(self()->comp(), "tst reg:%s tgt reg:%s\n", tstReg->getRegisterName(self()->comp()),
            tgtReg ? tgtReg->getRegisterName(self()->comp()) : "NULL");

      return (tgtReg == tstReg);
      }
   return false;
   }

bool OMR::Z::CodeGenerator::isActiveLogicalCC(TR::Node* ccNode, TR::Register* tstReg)
   {
   TR::ILOpCodes op = ccNode->getOpCodeValue();

   if (self()->hasCCInfo() && self()->hasCCZero() && !self()->hasCCOverflow() &&
       (TR::ILOpCode::isEqualCmp(op) || TR::ILOpCode::isNotEqualCmp(op)) &&
       performTransformation(self()->comp(), "O^O Logical CC Info: CCInfo(%d) CCZero(%d) CCOverflow(%d)\n", self()->hasCCInfo(), self()->hasCCZero(), self()->hasCCOverflow())
      )
      {
      TR::Instruction* inst = self()->ccInstruction();
      TR::Register* tgtReg = inst->tgtRegArrElem(0);
      dumpOptDetails(self()->comp(), "tst reg:%s tgt reg:%s\n", tstReg->getRegisterName(self()->comp()), tgtReg ? tgtReg->getRegisterName(self()->comp()) : "NULL");

      return (tgtReg == tstReg);
      }
   return false;
   }

bool
OMR::Z::CodeGenerator::excludeInvariantsFromGRAEnabled()
   {
   return true;
   }

uint32_t
OMR::Z::CodeGenerator::getJitMethodEntryAlignmentBoundary()
   {
   return 256;
   }

uint32_t
OMR::Z::CodeGenerator::getJitMethodEntryAlignmentThreshold()
   {
   return 192;
   }

/**
 * This function sign extended the specified number of high order bits in the register.
 */
bool
OMR::Z::CodeGenerator::signExtendedHighOrderBits( TR::Node * node, TR::Register * targetRegister, uint32_t numberOfBits)
   {
   return self()->signExtendedHighOrderBits( node, targetRegister, targetRegister, numberOfBits);
   }

/**
 * This function sign extended the specified number of high order bits in the register.
 */
bool
OMR::Z::CodeGenerator::signExtendedHighOrderBits( TR::Node * node, TR::Register * targetRegister, TR::Register * srcRegister, uint32_t numberOfBits)
   {
   switch (numberOfBits)
      {
      case 16:
         generateRRInstruction(self(), TR::InstOpCode::LHR, node, targetRegister, targetRegister);
         break;

      case 24:
         // Can use Load Byte to sign extended & load bits 56-63 from source to target register
         generateRRInstruction(self(), TR::InstOpCode::LBR, node, targetRegister, targetRegister);
         break;

      case 48:
         // Can use Load Halfword to sign extended & load bits 48-63 from source to target register
         generateRRInstruction(self(), TR::InstOpCode::LGHR, node, targetRegister, srcRegister);
         break;

      case 56:
         // Can use Load Byte to sign extended & load bits 56-63 from source to target register
         generateRRInstruction(self(), TR::InstOpCode::LGBR, node, targetRegister, srcRegister);
         break;

      default:
         TR_ASSERT( 0, "No appropriate instruction to use for sign extend this number of bits!\n");
         break;
      }

   return true;
   }

/**
 * This function zero filled the specified number of high order bits in the register.
 */
bool
OMR::Z::CodeGenerator::clearHighOrderBits( TR::Node * node, TR::Register * targetRegister, uint32_t numberOfBits)
   {
   return self()->clearHighOrderBits( node, targetRegister, targetRegister, numberOfBits);
   }

/**
 * This function zero filled the specified number of high order bits in the register.
 */
bool
OMR::Z::CodeGenerator::clearHighOrderBits( TR::Node * node, TR::Register * targetRegister, TR::Register * srcRegister, uint32_t numberOfBits)
   {
   switch (numberOfBits)
      {
      case 8:
         // Can use AND immediate to clear high 8 bits
         generateRIInstruction(self(), TR::InstOpCode::NILH, node, targetRegister, (int16_t)0x00FF);
         break;

      case 16:
         // Can use AND immediate to clear high 16 bits
         generateRIInstruction(self(), TR::InstOpCode::NILH, node, targetRegister, (int16_t)0x0000);
         break;

      case 24:
         // Can use AND immediate to clear high 24 bits
         generateRILInstruction(self(), TR::InstOpCode::NILF, node, targetRegister, 0x000000FF);
         break;

      case 48:
         // Can use Load Logical Halfword to load bits 48-63 from source to target register.
         generateRRInstruction(self(), TR::InstOpCode::LLGHR, node, targetRegister, srcRegister);
         break;

      case 56:
         // Can use Load Logical Character to load bits 56-63 from source to target register.
         generateRRInstruction(self(), TR::InstOpCode::LLGCR, node, targetRegister, srcRegister);
         break;

      default:
         TR_ASSERT( 0, "No appropriate instruction to use for clearing this number of bits!\n");
         break;
      }

   return true;
   }

bool OMR::Z::CodeGenerator::isILOpCodeSupported(TR::ILOpCodes o)
   {
   if (!OMR::CodeGenerator::isILOpCodeSupported(o))
      {
      return false;
      }
   else
      {
      return (_nodeToInstrEvaluators[o] != TR::TreeEvaluator::badILOpEvaluator);
      }
   }

bool  OMR::Z::CodeGenerator::needs64bitPrecision(TR::Node *node)
   {
   if (self()->comp()->target().is64Bit())
      return true;
   else
      return false;
   }

void
OMR::Z::CodeGenerator::setUnavailableRegistersUsage(TR_Array<TR_BitVector>  & liveOnEntryUsage, TR_Array<TR_BitVector>   & liveOnExitUsage)
   {
   TR_BitVector unavailableRegisters(16, self()->comp()->trMemory());
   TR::Block * b = self()->comp()->getStartBlock();
   unavailableRegisters.empty();

   do
      {
      self()->setUnavailableRegisters(b, unavailableRegisters);

      TR_BitVectorIterator bvi;
      bvi.setBitVector(unavailableRegisters);
      while (bvi.hasMoreElements())
         {
         int32_t regIndex = bvi.getNextElement();
         int32_t globalRegNum = self()->machine()->getGlobalReg((TR::RealRegister::RegNum)(regIndex + 1));

         if (globalRegNum!=-1)
            {
            liveOnEntryUsage[globalRegNum].set(b->getNumber());
            liveOnExitUsage[globalRegNum].set(b->getNumber());
            }
         }
      unavailableRegisters.empty();

      } while (b = b->getNextBlock());

   }

void
OMR::Z::CodeGenerator::resetGlobalRegister(TR::RealRegister::RegNum reg, TR_BitVector &globalRegisters)
   {
   int32_t globalRegNum=self()->machine()->getGlobalReg(reg);
   if (globalRegNum!=-1) globalRegisters.reset(globalRegNum);
   }

void
OMR::Z::CodeGenerator::setGlobalRegister(TR::RealRegister::RegNum reg, TR_BitVector &globalRegisters)
   {
   int32_t globalRegNum=self()->machine()->getGlobalReg(reg);
   if (globalRegNum!=-1) globalRegisters.set(globalRegNum);
   }

void
OMR::Z::CodeGenerator::setRegister(TR::RealRegister::RegNum reg, TR_BitVector &registers)
   {
   if ((int32_t) reg >=  (int32_t) TR::RealRegister::FirstGPR)
      registers.set(REGINDEX(reg));
   }

void
OMR::Z::CodeGenerator::setUnavailableRegisters(TR::Block *b, TR_BitVector &unavailableRegisters)
   {
   }

void
OMR::Z::CodeGenerator::removeUnavailableRegisters(TR_RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters)
   {
   TR_BitVectorIterator loe(rc->getBlocksLiveOnExit());

   while (loe.hasMoreElements())
      {
      int32_t blockNumber = loe.getNextElement();
      TR::Block * b = blocks[blockNumber];
      TR::Node * lastTreeTopNode = b->getLastRealTreeTop()->getNode();

//      traceMsg(comp(),"For register candidate %d, considering block %d.  Looking at node %p\n",rc->getSymbolReference()->getReferenceNumber(),blockNumber,lastTreeTopNode);

      if (lastTreeTopNode->getNumChildren() > 0 && lastTreeTopNode->getFirstChild()->getOpCode().isFunctionCall() && lastTreeTopNode->getFirstChild()->getOpCode().isJumpWithMultipleTargets()) // ensure calls that have multiple targets remove all volatile registers from available registers
         {
         TR_LinkageConventions callLinkage= lastTreeTopNode->getFirstChild()->getSymbol()->castToMethodSymbol()->getLinkageConvention();

         TR_BitVector *volatileRegs = self()->getGlobalRegisters(TR_volatileSpill, callLinkage);

//         traceMsg(comp(), "volatileRegs = \n");
//         volatileRegs->print(comp());
//         traceMsg(comp(), "availableRegs before = \n");
//         availableRegisters.print(comp());

         availableRegisters -= *volatileRegs  ;

//         traceMsg(comp(), "availableRegs after = \n");
//         availableRegisters.print(comp());
         }
      }
   }

void
OMR::Z::CodeGenerator::genMemCpy(TR::MemoryReference *targetMR, TR::Node *targetNode, TR::MemoryReference *sourceMR, TR::Node *sourceNode, int64_t copySize)
   {
   if (copySize > 0 && copySize <= TR_MAX_MVC_SIZE)
      {
      generateSS1Instruction(self(), TR::InstOpCode::MVC, targetNode, copySize-1, targetMR, sourceMR);
      }
   else
      {
      MemCpyConstLenMacroOp op(targetNode, sourceNode, copySize, self());
      op.generate(targetMR, sourceMR);
      }
   }

void
OMR::Z::CodeGenerator::genMemClear(TR::MemoryReference *targetMR, TR::Node *targetNode, int64_t clearSize)
   {
   MemClearConstLenMacroOp op(targetNode, clearSize, self());
   op.generate(targetMR);
   }
/**
 * Input reg can be NULL (when called for a store node or other type that does not return a register)
 */
void
OMR::Z::CodeGenerator::genCopyFromLiteralPool(TR::Node *node, int32_t bytesToCopy, TR::MemoryReference *targetMR, size_t litPoolOffset, TR::InstOpCode::Mnemonic op)
   {
   TR::Register *litPoolBaseReg = NULL;
   TR::Node *litPoolNode = NULL;
   bool stopUsing = false;
   if (self()->isLiteralPoolOnDemandOn())
      {
      if (litPoolNode)
         {
         litPoolBaseReg = self()->evaluate(litPoolNode);
         self()->decReferenceCount(litPoolNode);
         }
      else
         {
         litPoolBaseReg = self()->allocateRegister();
         stopUsing = true;
         generateLoadLiteralPoolAddress(self(), node, litPoolBaseReg);
         }
      }
   else
      {
      litPoolBaseReg = self()->getLitPoolRealRegister();
      if (litPoolNode)
         self()->recursivelyDecReferenceCount(litPoolNode);
      }

   TR::MemoryReference *paddingBytesMR = generateS390ConstantAreaMemoryReference(litPoolBaseReg, litPoolOffset, node, self(), true); // forSS=true
   int32_t mvcSize = bytesToCopy-1;
   generateSS1Instruction(self(), op, node,
                          mvcSize,
                          targetMR,
                          paddingBytesMR);

   if (stopUsing)
      self()->stopUsingRegister(litPoolBaseReg);
   }

TR::Instruction *
OMR::Z::CodeGenerator::genLoadAddressToRegister(TR::Register *reg, TR::MemoryReference *origMR, TR::Node *node, TR::Instruction *preced)
   {
   intptr_t offset = origMR->getOffset();
   TR::CodeGenerator *cg = self();
   if (offset >= TR_MIN_RX_DISP && offset <= TR_MAX_RX_DISP)
      {
      preced = generateRXInstruction(cg, TR::InstOpCode::LA, node, reg, origMR, preced);
      }
   else if (offset > MINLONGDISP && offset < MAXLONGDISP)
      {
      preced = generateRXInstruction(cg, TR::InstOpCode::LAY, node, reg, origMR, preced);
      }
   else
      {
      TR::MemoryReference *tempMR = generateS390MemoryReference(*origMR, 0, cg);
      tempMR->setOffset(0);
      preced = generateRXInstruction(cg, TR::InstOpCode::LA, node, reg, tempMR, preced);
      preced = generateS390ImmOp(cg, TR::InstOpCode::getAddOpCode(), node, reg, reg, (int64_t)offset, NULL, NULL);
      }
   return preced;
   }

/**
 * Allows a platform code generator to assert that a particular node operation will use 64 bit values
 * that are not explicitly present in the node datatype.
 */
bool
OMR::Z::CodeGenerator::usesImplicit64BitGPRs(TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   // packed/dfp conversion operations use GPR/FPR form conversions where the GPR is always a 64 bit register
   return node->getOpCodeValue() == TR::pd2df ||
          node->getOpCodeValue() == TR::pd2dd ||
          node->getOpCodeValue() == TR::pd2de ||
          node->getOpCodeValue() == TR::df2pd ||
          node->getOpCodeValue() == TR::dd2pd ||
          node->getOpCodeValue() == TR::de2pd;
#else
   return false;
#endif
   }

bool OMR::Z::CodeGenerator::nodeRequiresATemporary(TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (node->getOpCode().isPackedRightShift())
      {
      // The most efficient way to perform a packed decimal right shift by a non-zero even amount when the round amount
      // is also zero is to use an MVN instruction.
      // However since this instruction will perform the 'shift' by moving the sign code to the left there are right aligned dead bytes
      // and implied left aligned zero bytes introduced.
      // These dead and implied bytes cannot be within a user variable as the actual full value must be materialized therefore require a
      // temporary to be used for these even right shifts.
      //
      int32_t shiftAmount = -node->getDecimalAdjust();
      int32_t roundAmount = node->getDecimalRound();
      if (roundAmount == 0 && shiftAmount != 0 && ((shiftAmount&0x1)==0))
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\tnodeRequiresATemporary = true for packedShiftRight %s (%p)\n",node->getOpCode().getName(),node);
         return true;
         }
      }
   else if (node->getOpCodeValue() == TR::pddiv)
      {
      // The DP instruction places the quotient left aligned in the result field so a temporary must be used.
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tnodeRequiresATemporary = true for packedDivide %s (%p)\n",node->getOpCode().getName(),node);
      return true;
      }
   else if (node->getOpCode().isConversion() &&
            node->getType().isAnyZoned() &&
            node->getFirstChild()->getType().isZonedSeparateSign())
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tnodeRequiresATemporary = true for zonedSepToZoned %s (%p)\n",node->getOpCode().getName(),node);
      return true;
      }
   else
#endif
      if (TR::MemoryReference::typeNeedsAlignment(node) &&
            node->getType().isAggregate() &&
            node->getOpCode().isRightShift())
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\tnodeRequiresATemporary = true for aggrShiftRight %s (%p)\n",node->getOpCode().getName(),node);
      return true;
      }
   return false;
   }

bool OMR::Z::CodeGenerator::isStorageReferenceType(TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (node->getType().isBCD())
      return true;
   else
#endif
      return false;
   }

bool OMR::Z::CodeGenerator::endAccumulatorSearchOnOperation(TR::Node *node)
   {
   bool endHint = self()->endHintOnOperation(node);

   if (self()->comp()->getOption(TR_PrivatizeOverlaps))
      {
      // phase check as node->getRegister() union is only valid past this point (the TR::CodeGenPhase::SetupForInstructionSelectionPhase will NULL this out before inst selection)
      TR_ASSERT(self()->getCodeGeneratorPhase() > TR::CodeGenPhase::SetupForInstructionSelectionPhase,"must be past TR::CodeGenPhase::SetupForInstructionSelectionPhase (current is %s)\n",
         self()->getCodeGeneratorPhase().getName());
      if (node->getRegister() == NULL && endHint)
         {
         // do not have to fully descend a tree looking for overlapping operations in the special (and actually quite common)
         // cases where a node will initialize to a new storageReference (endHintOnOperation check) and one of the conditions below is true
         // 1) a conversion between types that use storageRefs (BCD and Aggr) and types that don't (e.g. int,float) -- i2pd,i2o etc. It is always safe to end the search
         //    because one side does not use storagRefs (no chance for overlap) and the other will always get a new storageRef.
         // 2) a conversion between types that both use storageRefs AND the evaluator has been fixed up to detect the overlap conditions and privatize to a new temp (pd2zd,ud2pd etc)
         //
         //    zdstore "a"
         //       pd2zd
         //          pdload "b"
         //
         // can UNPK right from "b" to "a" without a temp as long as "a" and "b" do not overlap
         // endAccumulatorSearchOnOperation will return true to allow the pd2zd to UNPK (accumulate) right to the store location "a" as the
         // pd2zd will check and deal with any "a" and "b" overlap if any exists.
         //

         if (node->getOpCode().isConversion() &&
             (self()->isStorageReferenceType(node) && !self()->isStorageReferenceType(node->getFirstChild())) ||   // i2pd
             (!self()->isStorageReferenceType(node) && self()->isStorageReferenceType(node->getFirstChild())))     // pd2i
            {
            // case1 : i2pd,pd2l,i2o,o2i...
            return true;
            }
         else
            {
            // case2: evaluators below have been fixed up to privatize only if needed during instruction selection (privatizeBCDRegisterIfNeeded)
            switch (node->getOpCodeValue())
               {
#ifdef J9_PROJECT_SPECIFIC
               case TR::zd2pd:
               case TR::pd2zd:

               case TR::pd2zdsls:
               case TR::pd2zdsts:
               case TR::pd2zdslsSetSign:
               case TR::pd2zdstsSetSign:
               case TR::zdsls2pd:
               case TR::zdsts2pd:

               case TR::pd2ud:
               case TR::pd2udsl:
               case TR::pd2udst:

               case TR::ud2pd:
               case TR::udsl2pd:
               case TR::udst2pd:

                  return true;
#endif
               default:
                  return false;
               }
            }
         }
      }
   else
      {
      if (endHint)
         return true;
      else
         return false;
      }
   return false;
   }

bool OMR::Z::CodeGenerator::endHintOnOperation(TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   // for example: end hints at BCD operations like zd2pd and pd2zd but not at zdsle2zd and zdsle2zd
   if (node->getOpCode().isConversion() &&
       node->getOpCode().isBinaryCodedDecimalOp() &&
       !TR::ILOpCode::isZonedToZonedConversion(node->getOpCodeValue()) &&
       node->getType().isBCD())
      {
      return true;
      }
   else
#endif
      if (node->getOpCode().isConversion() &&
            node->getType().isAggregate())
      {
      return true;
      }
#ifdef J9_PROJECT_SPECIFIC
   else if (node->getOpCode().isPackedLeftShift() &&
            isOdd(node->getDecimalAdjust()))  // odd left shifts need a new temp
      {
      // odd left shifts may use an MVO and to ensure that the operands don't overlap a new temp is used
      // return false here to avoid a child of the shift from getting a doomed store hint (one that will have to
      // be moved to another new temp before the actual store is completed).
      // pdstore <a>
      //    pdshl    <- end hint after (below) this node
      //       pdadd <- if pdadd is tagged with store hint <a> then it will need to be copied to a new temp *and* the pdshl cannot use <a> either
      //       iconst 3 (some oddShiftAmount)
      return true;
      }
#endif
   else
      {
      return false;
      }
   }

bool OMR::Z::CodeGenerator::nodeMayCauseException(TR::Node *node)
   {
   TR::ILOpCode op = node->getOpCode();

#ifdef J9_PROJECT_SPECIFIC
   if (op.isBinaryCodedDecimalOp())
      {
      TR::ILOpCodes opValue = node->getOpCodeValue();
      TR::DataType type = node->getType();
      if (op.isLoadConst())
         return false;
      else if (op.isLoadVarDirect())
         return false;
      else if (op.isConversion() && type.isAnyPacked() && node->getFirstChild()->getType().isIntegral()) // i2pd/l2pd
         return false;
      else if (!type.isAnyPacked() && op.isModifyPrecision())
         return false;
      else if (opValue == TR::pd2zd || opValue == TR::zd2pd)
         return false;
      else if (opValue == TR::pd2ud || opValue == TR::ud2pd)
         return false;
      else if (!type.isAnyPacked() && op.isSetSign()) // packed may use ZAP for setSign
         return false;
      else
         return true;
      }
   else
#endif
      {
      TR::ILOpCode& opcode = node->getOpCode();
      return opcode.isDiv() || opcode.isExponentiation() || opcode.isRem();
      }
   }

bool
OMR::Z::CodeGenerator::useRippleCopyOrXC(size_t size, char *lit)
   {
   // Look for the same byte repeated across the entire literal
   // Ripple copy is less efficient than an MVC for size <= 8,
   // unless the repeated byte is 0, in which case we can use XC.
   const int32_t TR_MIN_RIPPLE_COPY_SIZE = 8 + 1;
   char byteOne = lit[0];
   if (size <= TR_MAX_MVC_SIZE &&
       (size >= TR_MIN_RIPPLE_COPY_SIZE || byteOne == 0))
      {
      size_t i = 1;
      for (i = 1; i < size; i++)
         {
         if (lit[i] != byteOne)
            break;
         }
      if (i == size)
         return true;
      }

   return false;
   }

void OMR::Z::CodeGenerator::setUsesZeroBasePtr(bool v)
   {
   _cgFlags.set(S390CG_usesZeroBasePtr, v);
   }

bool OMR::Z::CodeGenerator::getUsesZeroBasePtr()
   {
   return _cgFlags.testAny(S390CG_usesZeroBasePtr);
   }

bool OMR::Z::CodeGenerator::IsInMemoryType(TR::DataType type)
   {
#ifdef J9_PROJECT_SPECIFIC
   return type.isBCD();
#else
   return false;
#endif
   }

uint32_t getRegMaskFromRange(TR::Instruction * inst);

bool isMatchingStoreRestore(TR::Instruction *cursorLoad, TR::Instruction *cursorStore, TR::CodeGenerator *cg)
   {
   return false;
   }

uint32_t getRegMaskFromRange(TR::Instruction * inst)
   {
   uint32_t regs = 0;

   TR::Register *lowReg = toS390RSInstruction(inst)->getFirstRegister();
   TR::Register *highReg = toS390RSInstruction(inst)->getSecondRegister();
   if (inst->getRegisterOperand(1)->getRegisterPair())
      highReg = inst->getRegisterOperand(1)->getRegisterPair()->getHighOrder();

   uint32_t lowRegNum = ANYREGINDEX(toRealRegister(lowReg)->getRegisterNumber());
   uint32_t highRegNum = ANYREGINDEX(toRealRegister(highReg)->getRegisterNumber());

   TR_ASSERT(lowRegNum >= 0 && lowRegNum <= 15 && highRegNum >= 0 && highRegNum <= 15, "Unexpected register number");
   for (uint32_t i = lowRegNum; ; i = ((i == 15) ? 0 : i+1))  // wrap around to 0 at 15
      {
      regs = regs | (0x1 << i);
      if (i == highRegNum)
         break;
      }
   return regs;
   }

/**
 * Recursively walk node1 comparing to node2.  This is not a precise function.
 * It is used in a heuristic--many cases are missing and can be added when needed.
 */
bool compareNodes(TR::Node * node1, TR::Node * node2, TR::CodeGenerator *cg)
   {
   if (node1->getOpCodeValue() != node2->getOpCodeValue())
      return false;

   if (!(node1->getOpCode().isArithmetic() || node1->getOpCode().isConversion() || node1->getOpCode().isTwoChildrenAddress()
         || ((node1->getOpCode().isLoad() || node1->getOpCode().isLoadAddr()) && node1->getSymbol() == node2->getSymbol())
         || (node1->getOpCode().isIntegralConst() && node1->get64bitIntegralValue() == node2->get64bitIntegralValue())
         ))
      return false;

   // compare children
   if (node1->getNumChildren() > 0)
      {
      if (node1->getNumChildren() != node2->getNumChildren())
         return false;
      for (uint32_t i = 0; i < node1->getNumChildren(); i++)
         {
         if (!compareNodes(node1->getChild(i), node2->getChild(i), cg))
            return false;
         }
      }

   return true;
   }

void handleLoadWithRegRanges(TR::Instruction *inst, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR::InstOpCode::Mnemonic opCodeToUse = inst->getOpCodeValue();
   if (!(opCodeToUse == TR::InstOpCode::LM  || opCodeToUse == TR::InstOpCode::LMG || opCodeToUse == TR::InstOpCode::LMH ||
         opCodeToUse == TR::InstOpCode::LMY || opCodeToUse == TR::InstOpCode::VLM))
      return;

   bool isVector = opCodeToUse == TR::InstOpCode::VLM ? true : false;
   uint32_t numVRFs = TR::RealRegister::LastAssignableVRF - TR::RealRegister::FirstAssignableVRF;

   TR::Register *lowReg  = isVector ? toS390VRSInstruction(inst)->getFirstRegister()  : toS390RSInstruction(inst)->getFirstRegister();
   TR::Register *highReg = isVector ? toS390VRSInstruction(inst)->getSecondRegister() : toS390RSInstruction(inst)->getSecondRegister();

   if (inst->getRegisterOperand(1)->getRegisterPair())
      highReg = inst->getRegisterOperand(1)->getRegisterPair()->getHighOrder();
   uint32_t lowRegNum = ANYREGINDEX(toRealRegister(lowReg)->getRegisterNumber());
   uint32_t highRegNum = ANYREGINDEX(toRealRegister(highReg)->getRegisterNumber());

   uint32_t regsInRange;
   if (highRegNum >= lowRegNum)
      regsInRange = (highRegNum - lowRegNum + 1);
   else
      regsInRange = ((isVector ? numVRFs : 16) - lowRegNum) + highRegNum + 1;

   if (regsInRange <= 2)
      return;   // registers on instruction, nothing more to do.

   // visit all registers in range.  ignore first and last since RA will have taken care of already
   lowRegNum = lowRegNum == (isVector ? numVRFs - 1 : 15) ? 0 : lowRegNum + 1;
   // wrap around for loop terminal index to 0 at 15 or 31 for Vector Registers

   uint32_t availForShuffle = cg->machine()->genBitMapOfAssignableGPRs() & ~(getRegMaskFromRange(inst));

   for (uint32_t i  = lowRegNum  ;
                 i != highRegNum ;
                 i  = ((i == (isVector ? numVRFs - 1 : 15)) ? 0 : i+1))
      {
      TR::RealRegister *reg = cg->machine()->getRealRegister(i + TR::RealRegister::GPR0);

      // Registers that are assigned need to be checked whether they have a matching STM--otherwise we spill.
      // Since we are called post RA for the LM, we check if assigned registers are last used on the LM so we can
      // avoid needlessly spilling them. (ex. LM(GPR00F,GPR14P,PSALCCAV->LCCAEMS0); ! PSALCCAV needs a reg and only needed on LM)
      TR::Register *virtualReg = reg->getAssignedRegister();
      if (reg->getState() == TR::RealRegister::Assigned && inst != virtualReg->getEndOfRange())
         {
         TR::Instruction *cursor = inst->getPrev();
         bool found = false;
         while (cursor)
            {
            if (cursor->getOpCodeValue() == TR::InstOpCode::FENCE && cursor->getNode()->getOpCodeValue() == TR::BBStart)
               {
               TR::Block *block = cursor->getNode()->getBlock();
               if (!block->isExtensionOfPreviousBlock())
                  break;
               }
            if (cursor->defsRegister(virtualReg))
               break;

            if (isMatchingStoreRestore(inst, cursor, cg))
               {
               cg->traceRegisterAssignment("Skip spilling %R--restored on load", virtualReg);
               found = true;
               break; // found matching store--do nothing since load will restore value
               }
            cursor = cursor->getPrev();
            }

         // register is defined before it was stored, spill or shuffle it to a free reg before load clobbers it
         // start of range will be NULL in some cases, we are cautious and spill (def# 100246)
         if (!found)
            {
            cg->traceRegisterAssignment("trying to free %R for killed reg %R by loadmultiple \n", virtualReg, reg);
            TR::RealRegister *shuffle = cg->machine()->shuffleOrSpillRegister(inst, virtualReg, availForShuffle);
            if (shuffle != NULL)
               {
               //if the LM instruction uses this reg we just shuffled, it will have the wrong real reg, need to fix this up
               if (inst->usesRegister(reg))
                  {
                  inst->renameRegister(reg, shuffle);
                  }
               }
            }
         }
      }
   }

/**
 * Create a snippet of the debug counter address and generate a DCB (DebugCounterBump) pseudo-instruction that will be ignored during register assignment
 * Be aware that the constituent add immediate instruction (ASI/AGSI) sets condition codes for sign and overflow
 * In most cases the condition code is 2; for result greater than zero and no overflow
 *
 * @param cursor     Current binary encoding cursor
 * @param counter    The debug counter to increment
 * @param delta      Integer amount to increment the debug counter
 * @param cond       Register conditions for debug counters are now deprecated on z should be phased out when P architecture eliminates them
 */
TR::Instruction* OMR::Z::CodeGenerator::generateDebugCounterBump(TR::Instruction* cursor, TR::DebugCounterBase* counter, int32_t delta, TR::RegisterDependencyConditions* cond)
   {
   TR::Snippet *constant = self()->Create8ByteConstant(cursor->getNode(), reinterpret_cast<intptr_t> (counter->getBumpCountSymRef(self()->comp())->getSymbol()->getStaticSymbol()->getStaticAddress()), false);
   return generateS390DebugCounterBumpInstruction(self(), TR::InstOpCode::DCB, cursor->getNode(), constant, delta, cursor);
   }

TR::Instruction* OMR::Z::CodeGenerator::generateDebugCounterBump(TR::Instruction* cursor, TR::DebugCounterBase* counter, TR::Register* deltaReg, TR::RegisterDependencyConditions* cond)
   {
   // Z does not have a "Register to Storage Add" instruction, so this is a little tricky to implement
   return cursor;
   }

TR::Instruction* OMR::Z::CodeGenerator::generateDebugCounterBump(TR::Instruction* cursor, TR::DebugCounterBase* counter, int32_t delta, TR_ScratchRegisterManager &srm)
   {
   return cursor;
   }

TR::Instruction* OMR::Z::CodeGenerator::generateDebugCounterBump(TR::Instruction* cursor, TR::DebugCounterBase* counter, TR::Register* deltaReg, TR_ScratchRegisterManager &srm)
   {
   return cursor;
   }

bool
OMR::Z::CodeGenerator::opCodeIsNoOpOnThisPlatform(TR::ILOpCode &opCode)
   {
   TR::ILOpCodes op = opCode.getOpCodeValue();

   if ((op == TR::a2i || op == TR::i2a) && self()->comp()->target().is32Bit())
      return true;

   if ((op == TR::a2l || op == TR::l2a) && self()->comp()->target().is64Bit() &&
       !self()->comp()->useCompressedPointers())
      return true;

   return false;
   }

TR_BackingStore *
OMR::Z::CodeGenerator::getLocalF2ISpill()
   {
   if (_localF2ISpill != NULL)
      {
      return _localF2ISpill;
      }
   else
      {
      TR::AutomaticSymbol *spillSymbol = TR::AutomaticSymbol::create(self()->trHeapMemory(),TR::Float,4);
      spillSymbol->setSpillTempAuto();
      self()->comp()->getMethodSymbol()->addAutomatic(spillSymbol);
      _localF2ISpill = new (self()->trHeapMemory()) TR_BackingStore(self()->comp()->getSymRefTab(), spillSymbol, 0);
      return _localF2ISpill;
      }
   }

TR_BackingStore *
OMR::Z::CodeGenerator::getLocalD2LSpill()
   {
   if (_localD2LSpill != NULL)
      {
      return _localD2LSpill;
      }
   else
      {
      TR::AutomaticSymbol *spillSymbol = TR::AutomaticSymbol::create(self()->trHeapMemory(),TR::Double,8);
      spillSymbol->setSpillTempAuto();
      self()->comp()->getMethodSymbol()->addAutomatic(spillSymbol);
      _localD2LSpill = new (self()->trHeapMemory()) TR_BackingStore(self()->comp()->getSymRefTab(), spillSymbol, 0);
      return _localD2LSpill;
      }
   }

bool
OMR::Z::CodeGenerator::getSupportsConstantOffsetInAddressing(int64_t value)
   {
   return self()->isDispInRange(value);
   }

/**
 * On 390, except for long, address and widened instructions, they all
 * use RX format instructions.  Assume that if Trex is supported, will
 * be using Trex instructions for large enough displacements.
 */
bool OMR::Z::CodeGenerator::isDispInRange(int64_t disp)
   {
   return (MINLONGDISP <= disp) && (disp <= MAXLONGDISP);
   }

bool OMR::Z::CodeGenerator::getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode, TR::DataType dt)
   {

   /*
    * Prior to z14, vector operations that operated on floating point numbers only supported
    * Doubles. On z14 and onward, Float type floating point numbers are supported as well.
    */
   if (dt == TR::Float && !self()->comp()->target().cpu.getSupportsArch(TR::CPU::z14))
      {
      return false;
      }

   // implemented vector opcodes
   switch (opcode.getOpCodeValue())
      {
      case TR::vadd:
      case TR::vsub:
      case TR::vmul:
      case TR::vdiv:
      case TR::vneg:
         if (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vrem:
         if (dt == TR::Int32 || dt == TR::Int64)
            return true;
         else
            return false;
      case TR::vload:
      case TR::vloadi:
      case TR::vstore:
      case TR::vstorei:
         if (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vxor:
      case TR::vor:
      case TR::vand:
         if (dt == TR::Int32 || dt == TR::Int64)
            return true;
         else
            return false;
      case TR::vsplats:
      case TR::getvelem:
      case TR::vsetelem:
         if (dt == TR::Int32 || dt == TR::Int64 || dt == TR::Float || dt == TR::Double)
            return true;
         else
            return false;
      case TR::vushr:
         if (dt == TR::Int32 || dt == TR::Int64)
            return true;
         else
            return false;
      case TR::vl2vd:
         return true;
      default:
        return false;
      }

   return false;
   }

// simple syntactic test for matching direct loads when the caller knows (by some mechanism, e.g. _symRefsToCheckForKills) that
// there is no intervening kill to the load memory
bool
OMR::Z::CodeGenerator::directLoadAddressMatch(TR::Node *load1, TR::Node *load2, bool trace)
   {
   if (!(load1->getOpCode().isLoadVarDirect() && load2->getOpCode().isLoadVarDirect()))
      return false;

   if (load1->getOpCodeValue() != load2->getOpCodeValue())
      return false;

   if (load1->getSize() != load2->getSize())
      return false;

   if (load1->getSymbolReference() != load2->getSymbolReference())
      return false;

   return true;
   }

bool
OMR::Z::CodeGenerator::isOutOf32BitPositiveRange(int64_t value, bool trace)
   {
   if (value < 0 || value > INT_MAX)
      {
      if (trace)
         traceMsg(self()->comp(),"\tisOutOf32BitPositiveRange = true : value %lld < 0 (or > INT_MAX)\n",value);
      return true;
      }
   return false;
   }

int32_t
OMR::Z::CodeGenerator::getMaskSize(
      int32_t leftMostNibble,
      int32_t nibbleCount)
   {
   bool leftMostNibbleIsEven = (leftMostNibble&0x1)==0;
   int32_t maskNibbles = nibbleCount;
   if (leftMostNibbleIsEven) maskNibbles++;  // increase maskNibbles to include the top nibble
   if (maskNibbles&0x1) maskNibbles++;       // if odd then increase to include the bot nibble
   TR_ASSERT((maskNibbles&0x1) == 0,"maskNibbles should be even now and not %d\n",maskNibbles);
   int32_t size = maskNibbles/2;
   return size;
   }

// The search for 'conflicting' address nodes to disallow hints is to solve two different problems
// 1) circular evaluation (asserts) : node->getRegister() == NULL && node->getOpCode().canHaveStorageReferenceHint() below
// 2) clobbering of value child during address child evaluation : node->getType().isBCD() below
// A 'conflicting' address node is a BCD/Aggr or canHaveStorageReferenceHint node that is commoned some where under both
// the address child and value child of the same indirect store
//
// Case one IL looks like:
//
// zdstorei
//   i2a
//      pd2i
//        pd2zd
//         zdload
//   =>pd2zd
//
// In the middle of evaluating the =>pd2zd as the value child the address child is evaluated (as it's using a hint)
// and the same pd2zd node is encountered and an assert happens eventually as the storageRef nodeRefCount underflows
//
// Case two IL looks like:
// zdstorei
//   i2a
//      pd2i
//        pdadd
//         pdX
//         pdconst
//   pd2zd
//    =>pdX
//
// Where pdX is either an already initialized node of any type (add,load etc). or a pdloadi when forceBCDInit is on
// In either case what happens is that the pd2zd is evaluated first and evaluate its child pdX and use it right away to from the source memRef
// Then shortly after, when a hint is used, the address child is evaluated to form the dest memRef. When the parent of the pdX in the address child
// (the pdadd here) has an already initialized child then it does a clobber evaluate.
// The clobber evaluate will update the storageRef on the pdX register to a new temp and then clobber the storageRef already on the pdX
// Normally this is ok as future references to the pdX node will use the new temp BUT in this case the pd2zd in the value child has already used the
// original storageRef to form its source memRef so it's too late to pick up the new temp
//
// The fix for both of these problems is to disallow a hint to break the evaluation relationship/dependency between the value child and address child

// Z
bool
OMR::Z::CodeGenerator::possiblyConflictingNode(TR::Node *node)
   {
   bool possiblyConflicting = false;
#ifdef J9_PROJECT_SPECIFIC
   if (node->getReferenceCount() > 1)
      {
      if (node->getRegister() == NULL && node->getOpCode().canHaveStorageReferenceHint())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\t\t\tpossiblyConflicting=true %s (%p) : reg==NULL and storageRefHint_Case, refCount %d\n",node->getOpCode().getName(),node,node->getReferenceCount());
         possiblyConflicting = true;
         }
      else if (node->getType().isBCD())
         {
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\t\t\tpossiblyConflicting=true %s (%p) : BCDOrAggrType_Case, reg=%s, refCount %d\n",
               node->getOpCode().getName(),node,node->getRegister() ? self()->getDebug()->getName(node->getRegister()) : "NULL",node->getReferenceCount());
         possiblyConflicting = true;
         }
      }
#endif
   return possiblyConflicting;
   }

// Z
bool
OMR::Z::CodeGenerator::foundConflictingNode(TR::Node *node, TR::list<TR::Node*> *conflictingAddressNodes)
   {
   bool foundConflicting = false;
   if (!conflictingAddressNodes->empty() &&
       self()->possiblyConflictingNode(node) &&
       (std::find(conflictingAddressNodes->begin(), conflictingAddressNodes->end(), node) != conflictingAddressNodes->end()))
      {
      foundConflicting = true;
      }
   return foundConflicting;
   }

// Z
void
OMR::Z::CodeGenerator::collectConflictingAddressNodes(TR::Node *parent, TR::Node *node, TR::list<TR::Node*> *conflictingAddressNodes)
   {
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\t\texamining node %s (%p) : register %s\n",
         node->getOpCode().getName(),node,node->getRegister()?self()->getDebug()->getName(node->getRegister()):"NULL");

   if (self()->possiblyConflictingNode(node))
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\t\tadd %s (%p) to conflicting nodes list (refCount %d)\n",node->getOpCode().getName(),node,node->getReferenceCount());
      conflictingAddressNodes->push_front(node);
      }

   for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
      {
      TR::Node *child = node->getChild(i);
      if (child->getRegister() == NULL)
         {
         self()->collectConflictingAddressNodes(node, child, conflictingAddressNodes);
         }
      }
   }

bool
OMR::Z::CodeGenerator::loadOrStoreAddressesMatch(TR::Node *node1, TR::Node *node2)
   {
   TR_ASSERT(node1->getOpCode().isLoadVar() || node1->getOpCode().isStore(),"node1 %s (%p) should be a loadVar or a store\n",
      node1->getOpCode().getName(),node1);
   TR_ASSERT(node2->getOpCode().isLoadVar() || node2->getOpCode().isStore(),"node2 %s (%p) should be a loadVar or a store\n",
      node2->getOpCode().getName(),node2);

   bool foundMatch = false;

   if (node1->getSize() != node2->getSize())
      {
      if (self()->comp()->getOption(TR_TraceCG))
         traceMsg(self()->comp(),"\t\tloadOrStoreAddressesMatch = false (sizes differ) : node1 %s (%p) size = %d and node2 %s (%p) size = %d\n",
               node1->getOpCode().getName(),node1,node1->getSize(),node2->getOpCode().getName(),node2,node2->getSize());
      return false;
      }

   if (node1->getSymbolReference() == node2->getSymbolReference() ||
       !0)  // will be removed after code is stable
      {
      if (node1->getOpCode().isIndirect() && node2->getOpCode().isIndirect() &&
          node1->getSymbolReference()->getOffset() == node2->getSymbolReference()->getOffset())
         {
         TR::Node *addr1 = node1->getFirstChild();
         TR::Node *addr2 = node2->getFirstChild();

         foundMatch = self()->addressesMatch(addr1, addr2);
         }
      else if (!node1->getOpCode().isIndirect() && !node2->getOpCode().isIndirect() &&
               node1->getSymbolReference() == node2->getSymbolReference())
         {
         foundMatch = true;
         }
      }
   if (self()->comp()->getOption(TR_TraceCG))
      traceMsg(self()->comp(),"\t\tloadOrStoreAddressesMatch = %s : node1 %s (%p) and node2 %s (%p)\n",
         foundMatch?"true":"false",node1->getOpCode().getName(),node1,node2->getOpCode().getName(),node2);
   return foundMatch;
   }

#ifdef J9_PROJECT_SPECIFIC
bool OMR::Z::CodeGenerator::reliesOnAParticularSignEncoding(TR::Node *node)
   {
   TR::ILOpCode op = node->getOpCode();
   if (op.isBCDStore() && !node->chkCleanSignInPDStoreEvaluator()) // most common case of needing so put up-front -- the value escapes
      return true;

   if (op.isBCDStore() && node->chkCleanSignInPDStoreEvaluator())
      return false;

   if (op.isAnyBCDArithmetic())
      return false;

   if (op.isSimpleBCDClean())
      return false;

   if (op.isSetSign() || op.isSetSignOnNode())
      return false;

   if (op.isIf())
      return false;

   if (op.isConversion() && !node->getType().isBCD()) // e.g. pd2i,pd2l,pd2df etc
      return false;

   // left shifts do not rely on a clean sign so the only thing to check is if a clean sign property has been propagated thru it (if not then can return false)
   if (node->getType().isBCD() && op.isLeftShift() && !self()->propagateSignThroughBCDLeftShift(node->getType()))
      return false;

   if (node->getType().isBCD() && op.isRightShift())
      return false;

   return true;
   }
#endif

bool OMR::Z::CodeGenerator::loadAndStoreMayOverlap(TR::Node *store, size_t storeSize, TR::Node *load, size_t loadSize)
   {
   TR_ASSERT(store->getOpCode().hasSymbolReference() && store->getSymbolReference(),"store %s (%p) must have a symRef\n",store->getOpCode().getName(),store);
   TR_UseDefAliasSetInterface storeAliases = store->getSymbolReference()->getUseDefAliases();
   return self()->loadAndStoreMayOverlap(store, storeSize, load, loadSize, storeAliases);
   }

bool
OMR::Z::CodeGenerator::checkIfcmpxx(TR::Node *node)
   {
   TR::Node *firstChildNode = node->getFirstChild();
   TR::Node *secondChildNode = node->getSecondChild();

   bool firstChildOK = (firstChildNode->getOpCode().isLoadVar() || firstChildNode->getOpCode().isLoadConst()) &&
                       firstChildNode->getType().isIntegral() &&
                       firstChildNode->getReferenceCount() == 1;

   bool secondChildOK = firstChildOK &&
                        (secondChildNode->getOpCode().isLoadVar() || secondChildNode->getOpCode().isLoadConst()) &&
                        secondChildNode->getType().isIntegral() &&
                        secondChildNode->getReferenceCount() == 1;

   if (secondChildOK &&
       firstChildNode->getSize() == secondChildNode->getSize())
      {
      if (secondChildNode->getOpCode().isLoadConst() &&
          secondChildNode->getIntegerNodeValue<int64_t>() == 0)
         {
         return false; //If the value child is lconst 0, LT/LTG will be better.
         }
      else
         {
         return true;
         }
      }
   else
      {
      return false;
      }
   }

// Z
bool
OMR::Z::CodeGenerator::checkSimpleLoadStore(TR::Node *loadNode, TR::Node *storeNode, TR::Block *block)
   {
   //The pattern below leads to use index reg in addressing, LAY for example, that costs extra for instructions like MVC.
   //   aiadd
   //     load-addr
   //     laod-non-const

//traceMsg(comp(), "Considering storeNode %p and loadNode %p\n",storeNode,loadNode);

   if (loadNode->getOpCode().isIndirect() &&
       loadNode->getFirstChild()->getNumChildren() > 1 &&
       loadNode->getFirstChild()->getOpCode().isAdd() &&
       !loadNode->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      return false;
      }
   else if (storeNode->getOpCode().isIndirect() &&
            storeNode->getFirstChild()->getNumChildren() > 1 &&
            storeNode->getFirstChild()->getOpCode().isAdd() &&
            !storeNode->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      return false;
      }
   else if (loadNode->getSize() != storeNode->getSize())
      {
      return false;
      }
   else if ( !self()->getAccessStaticsIndirectly() && (
                                                (storeNode->getOpCode().hasSymbolReference() && storeNode->getSymbol()->isStatic() && !storeNode->getOpCode().isIndirect()) ||
                                                (loadNode->getOpCode().hasSymbolReference() && loadNode->getSymbol()->isStatic() && !loadNode->getOpCode().isIndirect())
                                               )
            )
      {

      return false;
      }
   else if (loadNode->getOpCode().isLoadConst()&&
            loadNode->getReferenceCount() == 1 )
      {//The const may be loaded into reg and shared. So dont do it.
      return true;
      }
   else if (loadNode->getOpCode().isLoadVar() &&
            loadNode->getReferenceCount() == 1 &&
            loadNode->getSymbolReference())
      {
      if (storeNode->getSize() == 1 && loadNode->getSize() == 1)
         return true;
      else if (self()->loadAndStoreMayOverlap(storeNode, storeNode->getSize(), loadNode, loadNode->getSize()))
         return false;
      else
         return true;
      }
   else
      {
      return false;
      }
   }

bool
OMR::Z::CodeGenerator::checkBitWiseChild(TR::Node *node)
   {
   if (node->getOpCode().isLoadConst() ||
            (node->getOpCode().isLoadVar() && node->getReferenceCount() == 1))
      {
      if (node->getType().isInt8() || node->getType().isInt16())
         return true;
      else
         return false;
      }
   else
      {
      return false;
      }
   }

TR_StorageDestructiveOverlapInfo
OMR::Z::CodeGenerator::getStorageDestructiveOverlapInfo(TR::Node *src, size_t srcLength, TR::Node *dst, size_t dstLength)
   {
   TR_StorageDestructiveOverlapInfo overlapInfo = TR_UnknownOverlap;

   if (srcLength == 1 && dstLength == 1)
      {
      //This is a special case that is safe in terms of DestructiveOverlapInfo.
      overlapInfo = TR_DefinitelyNoDestructiveOverlap;
      }
   else
      {
      switch (self()->storageMayOverlap(src, srcLength, dst, dstLength))
         {
         case TR_PostPosOverlap:
         case TR_SamePosOverlap:
         case TR_PriorPosOverlap:
         case TR_NoOverlap:
              overlapInfo = TR_DefinitelyNoDestructiveOverlap; break;
         case TR_DestructiveOverlap:
              overlapInfo = TR_DefinitelyDestructiveOverlap; break;
         case TR_MayOverlap:
         default: overlapInfo = TR_UnknownOverlap; break;
         }
      }

   return overlapInfo;
   }

// Z
//
// Recursively set the futureUseCount for add and sub nodes
void setIntegralAddSubFutureUseCount(TR::Node *node, vcount_t visitCount)
   {
   if (node == NULL)
      return;

   if ((node->getOpCode().isAdd() || node->getOpCode().isSub()) && node->getType().isIntegral())
      {
      if (node->getVisitCount() != visitCount)
         node->setFutureUseCount(node->getReferenceCount() - 1);
      else
         node->decFutureUseCount();
      }

   if (node->getVisitCount() != visitCount)
      {
      node->setVisitCount(visitCount);
      for (int32_t i = 0; i < node->getNumChildren(); i++)
         setIntegralAddSubFutureUseCount(node->getChild(i), visitCount);
      }
   }

// Z
//
// Flag any add or sub nodes that could be candidates for a branch on count operation
// Matching a tree like the following:
// ificmpgt
//   => iadd
//     iRegLoad GRX
//     iconst -1
//   iconst 0
//   GlRegDeps
//     PassThrough GRX
//       => iadd
void
OMR::Z::CodeGenerator::setBranchOnCountFlag(TR::Node *node, vcount_t visitCount)
   {
   if (self()->comp()->getOption(TR_DisableBranchOnCount))
      return;

   if (node->getOpCodeValue() == TR::treetop)
      {
      self()->setBranchOnCountFlag(node->getFirstChild(), visitCount);
      return;
      }

   // Make sure the futureUseCount is always set correctly for integral add/sub nodes
   setIntegralAddSubFutureUseCount(node, visitCount);

   if (node->getOpCodeValue() != TR::ificmpgt && node->getOpCodeValue() != TR::iflcmpgt)
      return;

   TR::Node *addNode = node->getFirstChild();
   if (!addNode->getType().isIntegral())
      return;
   if (!addNode->getOpCode().isAdd() && !addNode->getOpCode().isSub())
      return;

   TR::Node *zeroNode = node->getSecondChild();
   if (!zeroNode->getOpCode().isIntegralConst() || zeroNode->getIntegerNodeValue<int32_t>() != 0)
      return;

   if (node->getNumChildren() < 3 || node->getChild(2)->getOpCodeValue() != TR::GlRegDeps)
      return;

   // Either add -1 or subtract 1
   TR::Node *delta = addNode->getSecondChild();
   if (!delta->getOpCode().isIntegralConst())
      return;
   if (addNode->getOpCode().isAdd() && delta->getIntegerNodeValue<int32_t>() != -1)
      return;
   else if (addNode->getOpCode().isSub() && delta->getIntegerNodeValue<int32_t>() != 1)
      return;

   // On z/OS, the BRCT instruction decrements the counter and tests for EQUALITY to zero, while
   // the pattern we're matching here is for the counter to be > 0. If the counter is initially <= 0, the IL
   // would decrement and then fall through the branch, while the BRCT would decrement and take the branch.
   // So, we require that the counter is > 0 before it's decremented, or that it's >= 0 after it's decremented.
   if (!addNode->isNonNegative())
      return;

   // Child of the add must be a RegLoad
   TR::Node *regLoad = addNode->getFirstChild();
   if (regLoad->getOpCodeValue() != TR::iRegLoad)
      return;

   // RegDeps must have a passthrough as a child with the same register number as the regload
   TR::Node *regDeps = node->getChild(2);
   bool found = false;
   for (int32_t i = 0; i < regDeps->getNumChildren(); i++)
      {
      TR::Node *passThrough = regDeps->getChild(i);
      if (passThrough->getOpCodeValue() != TR::PassThrough)
         continue;

      // Make sure the regLoad and the passThrough have the same register
      if (regLoad->getGlobalRegisterNumber() == passThrough->getGlobalRegisterNumber() &&
          passThrough->getNumChildren() >= 1 &&
          passThrough->getFirstChild() == addNode)
         {
         found = true;
         break;
         }
      }

   if (!found)
      return;

   // Make sure that the only uses of the add/sub node, so far, are the three we expect: under the iRegStore, under the compare, and in the
   // GLRegDeps under the compare. futureUseCount should've been initialized to be referenceCount - 1 (which will have to be an iRegStore in order
   // for us to see the add node under GLRegDeps here). The compare node here will account for the other two uses, so as long as there's no other
   // uses between the iRegStore and the ificmpgt, all is well.
   if (addNode->getReferenceCount() != addNode->getFutureUseCount() + 3)
      return;

   // Everything is good; set the flags
   if(!performTransformation(self()->comp(), "%s Found branch on count opportunity\n", OPT_DETAILS))
      return;
   dumpOptDetails(self()->comp(), "Branch on count opportunity found (compare: 0x%p\tincrement: 0x%p)\n", node, addNode);

   node->setIsUseBranchOnCount(true);
   addNode->setIsUseBranchOnCount(true);
   }

// Z only
bool
OMR::Z::CodeGenerator::isCompressedClassPointerOfObjectHeader(TR::Node * node)
   {
   return (TR::Compiler->om.generateCompressedObjectHeaders() &&
           node->getOpCode().hasSymbolReference() &&
           (node->getSymbol()->isClassObject() ||
            node->getSymbolReference() == self()->getSymRefTab()->findVftSymbolRef()));
   }

size_t
OMR::Z::CodeGenerator::findOrCreateLiteral(void *value, size_t len)
   {
   TR_ASSERT(0, "findOrCreateLiteral unimplemented\n");
   return 0;
   }

bool
OMR::Z::CodeGenerator::directCallRequiresTrampoline(intptr_t targetAddress, intptr_t sourceAddress)
   {
   return
      !self()->comp()->target().cpu.isTargetWithinBranchRelativeRILRange(targetAddress, sourceAddress) ||
      self()->comp()->getOption(TR_StressTrampolines);
   }
