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

#pragma csect(CODE,"TRZCGBase#C")
#pragma csect(STATIC,"TRZCGBase#S")
#pragma csect(TEST,"TRZCGBase#T")

#include <algorithm>                                // for std::find, etc
#include <limits.h>                                 // for INT_MAX, INT_MIN
#include <stddef.h>                                 // for size_t
#include <stdint.h>                                 // for int32_t, etc
#include <stdio.h>                                  // for printf, sprintf
#include <stdlib.h>                                 // for atoi, abs
#include <string.h>                                 // for NULL, strcmp, etc
#include "codegen/BackingStore.hpp"                 // for TR_BackingStore
#include "codegen/CodeGenPhase.hpp"                 // for CodeGenPhase, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd, etc
#include "codegen/GCStackAtlas.hpp"                 // for GCStackAtlas
#include "codegen/GCStackMap.hpp"                   // for TR_GCStackMap, etc
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction, etc
#include "codegen/Linkage.hpp"                      // for Linkage, REGNUM, etc
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"                 // for TR_LiveRegisters, etc
#include "codegen/Machine.hpp"                      // for Machine, etc
#include "codegen/MemoryReference.hpp"              // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/RecognizedMethods.hpp"            // for RecognizedMethod, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterIterator.hpp"             // for RegisterIterator
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/Snippet.hpp"                      // for TR::S390Snippet, etc
#include "codegen/StorageInfo.hpp"
#include "codegen/SystemLinkage.hpp"                // for toSystemLinkage, etc
#include "codegen/TreeEvaluator.hpp"                // for TreeEvaluator, etc
#include "compile/Compilation.hpp"                  // for Compilation, etc
#include "compile/Method.hpp"                       // for TR_Method, mcount_t
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"                // for TR_Recompilation, etc
#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"                // for TR_Recompilation, etc
#endif
#include "cs2/arrayof.h"                            // for ArrayOf
#include "cs2/hashtab.h"                            // for HashTable, etc
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/Processors.hpp"                       // for TR_Processor, etc
#include "env/StackMemoryRegion.hpp"                // for TR::StackMemoryRegion
#include "env/TRMemory.hpp"                         // for Allocator, etc
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                             // for Block, toBlock, etc
#include "il/DataTypes.hpp"                         // for DataTypes, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                             // for ILOpCode, etc
#include "il/Node.hpp"                              // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode, etc
#include "il/symbol/AutomaticSymbol.hpp"            // for AutomaticSymbol
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol, etc
#include "il/symbol/MethodSymbol.hpp"               // for MethodSymbol
#include "il/symbol/ParameterSymbol.hpp"            // for ParameterSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"               // for StaticSymbol
#include "infra/Array.hpp"                          // for TR_Array
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/Bit.hpp"                            // for isOdd, isEven
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/Flags.hpp"                          // for flags32_t
#include "infra/HashTab.hpp"                        // for TR_HashTabInt, etc
#include "infra/List.hpp"                           // for ListIterator, etc
#include "infra/Random.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/Stack.hpp"                          // for TR_Stack
#include "infra/CfgEdge.hpp"                        // for CFGEdge
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"                  // for Optimizer
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase, etc
#include "ras/DebugCounter.hpp"
#include "ras/Delimiter.hpp"                        // for Delimiter
#include "runtime/Runtime.hpp"                      // for TR_LinkageInfo
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/EndianConversion.hpp"           // for bos
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"
#include "z/codegen/TRSystemLinkage.hpp"

#if J9_PROJECT_SPECIFIC
#include "z/codegen/S390Register.hpp"
#include "trj9/z/codegen/J9S390PrivateLinkage.hpp"
#endif

class TR_IGNode;
namespace TR { class DebugCounterBase; }
namespace TR { class S390PrivateLinkage; }
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
         uintptrj_t temp  = (uintptrj_t)ptr->getValue();
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
   if (TR::Compiler->target.cpu.isZ() &&
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

      if (TR::Compiler->target.is64Bit())
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

         intptrj_t offset = 0;
         if (TR::Compiler->target.is64Bit())
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
         if (TR::Compiler->target.is64Bit())
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
   return self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10);
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkz900: returns true if it's zArchitecture, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkz900()
   {
   return TR::Compiler->target.cpu.getS390SupportsZ900();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkz990: returns true if it's Trex, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkz990()
   {
   return TR::Compiler->target.cpu.getS390SupportsZ990();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkz9: returns true if it's Golden Eagle, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkz9()
   {
   // LL:  Enable machine check
   return TR::Compiler->target.cpu.getS390SupportsZ9();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkz10: returns true if it's z6, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkz10()
   {
   return TR::Compiler->target.cpu.getS390SupportsZ10();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkz196: returns true if it's zGryphon, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkz196()
   {
   return TR::Compiler->target.cpu.getS390SupportsZ196();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkzEC12: returns true if it's zHelix, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkzEC12()
   {
   return TR::Compiler->target.cpu.getS390SupportsZEC12();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkZ13: returns true if it's z13, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkZ13()
   {
   return TR::Compiler->target.cpu.getS390SupportsZ13();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkZ14: returns true if it's z14, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkZ14()
   {
   return TR::Compiler->target.cpu.getS390SupportsZ14();
   }

////////////////////////////////////////////////////////////////////////////////
// TR_S390ProcessorInfo::checkZNext: returns true if it's zNext, otherwise false.
////////////////////////////////////////////////////////////////////////////////
bool
TR_S390ProcessorInfo::checkZNext()
   {
   return TR::Compiler->target.cpu.getS390SupportsZNext();
   }

////////////////////////////////////////////////////////////////////////////////
//  TR_S390ProcessorInfo::getProcessor: returns the TR_s390gpX for current processorInfo
////////////////////////////////////////////////////////////////////////////////
TR_Processor
TR_S390ProcessorInfo::getProcessor()
   {
   TR_Processor result = TR_s370gp7;

   if (supportsArch(TR_S390ProcessorInfo::TR_zNext))
      {
      result = TR_s370gp13;
      }
   else if (supportsArch(TR_S390ProcessorInfo::TR_z14))
      {
      result = TR_s370gp12;
      }
   else if (supportsArch(TR_S390ProcessorInfo::TR_z13))
      {
      result = TR_s370gp11;
      }
   else if (supportsArch(TR_S390ProcessorInfo::TR_zEC12))
      {
      result = TR_s370gp10;
      }
   else if(supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      result = TR_s370gp9;
      }
   else if (supportsArch(TR_S390ProcessorInfo::TR_z10))
      {
      result = TR_s370gp8;
      }

   return result;
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator member functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::CodeGenerator - Construct the S390 code generator
////////////////////////////////////////////////////////////////////////////////
OMR::Z::CodeGenerator::CodeGenerator()
   : OMR::CodeGenerator(),
     _processorInfo(),
     _extentOfLitPool(-1),
     _recompPatchInsnList(getTypedAllocator<TR::Instruction*>(self()->comp()->allocator())),
     _targetList(getTypedAllocator<TR::S390TargetAddressSnippet*>(self()->comp()->allocator())),
     _constantHash(self()->comp()->allocator()),
     _constantHashCur(_constantHash),
     _constantList(getTypedAllocator<TR::S390ConstantDataSnippet*>(self()->comp()->allocator())),
     _writableList(getTypedAllocator<TR::S390WritableDataSnippet*>(self()->comp()->allocator())),
     _transientLongRegisters(self()->trMemory()),
     _snippetDataList(getTypedAllocator<TR::S390ConstantDataSnippet*>(self()->comp()->allocator())),
     _outOfLineCodeSectionList(getTypedAllocator<TR_S390OutOfLineCodeSection*>(self()->comp()->allocator())),
     _returnTypeInfoInstruction(NULL),
     _ARSaveAreaForTM(NULL),
     _notPrintLabelHashTab(NULL),
     _interfaceSnippetToPICsListHashTab(NULL),
     _currentCheckNode(NULL),
     _currentBCDCHKHandlerLabel(NULL),
     _internalControlFlowRegisters(getTypedAllocator<TR::Register*>(self()->comp()->allocator())),
     _nodesToBeEvaluatedInRegPairs(self()->comp()->allocator()),
     _nodeAddressOfCachedStatic(NULL),
     _ccInstruction(NULL),
     _previouslyAssignedTo(self()->comp()->allocator("LocalRA")),
     _bucketPlusIndexRegisters(self()->comp()->allocator()),
     _currentDEPEND(NULL)
   {
   TR::Compilation *comp = self()->comp();
   _cgFlags = 0;

   // Initialize Linkage for Code Generator
   self()->initializeLinkage();

   // Check for -Xjit disable options and target this specific compilation for the proper target
   if (comp->getOption(TR_DisableZ10))
      {
      _processorInfo.disableArch(TR_S390ProcessorInfo::TR_z10);
      }

   if (comp->getOption(TR_DisableZ196))
      {
      _processorInfo.disableArch(TR_S390ProcessorInfo::TR_z196);
      }

   if (comp->getOption(TR_DisableZEC12))
      {
      _processorInfo.disableArch(TR_S390ProcessorInfo::TR_zEC12);
      }

   if (comp->getOption(TR_DisableZ13))
      {
      _processorInfo.disableArch(TR_S390ProcessorInfo::TR_z13);
      }

   if (comp->getOption(TR_DisableZ14))
      {
      _processorInfo.disableArch(TR_S390ProcessorInfo::TR_z14);
      }

   if (comp->getOption(TR_DisableZNext))
      {
      _processorInfo.disableArch(TR_S390ProcessorInfo::TR_zNext);
      }

   _unlatchedRegisterList =
      (TR::RealRegister**)self()->trMemory()->allocateHeapMemory(sizeof(TR::RealRegister*)*(TR::RealRegister::NumRegisters));
   _unlatchedRegisterList[0] = 0; // mark that list is empty

   bool enableBranchPreload = comp->getOption(TR_EnableBranchPreload);
   bool disableBranchPreload = comp->getOption(TR_DisableBranchPreload);

   if (enableBranchPreload || (!disableBranchPreload && comp->isOptServer() && _processorInfo.supportsArch(TR_S390ProcessorInfo::TR_zEC12)))
      self()->setEnableBranchPreload();
   else
      self()->setDisableBranchPreload();

   static bool bpp = (feGetEnv("TR_BPRP")!=NULL);

   if ((enableBranchPreload && bpp) || (bpp && !disableBranchPreload && comp->isOptServer() && _processorInfo.supportsArch(TR_S390ProcessorInfo::TR_zEC12)))
      self()->setEnableBranchPreloadForCalls();
   else
      self()->setDisableBranchPreloadForCalls();

   if (self()->supportsBranchPreloadForCalls())
      {
      _callsForPreloadList = new (self()->trHeapMemory()) TR::list<TR_BranchPreloadCallData*>(getTypedAllocator<TR_BranchPreloadCallData*>(comp->allocator()));
      }

   // Check if platform supports highword facility - both hardware and OS combination.
   if (self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196) && !comp->getOption(TR_MimicInterpreterFrameShape))
      {
      // On 31-bit zlinux we need to check RAS support
      if (!TR::Compiler->target.isLinux() || TR::Compiler->target.is64Bit() || TR::Compiler->target.cpu.getS390SupportsHPRDebug())
         self()->setSupportsHighWordFacility(true);
      }

   self()->setOnDemandLiteralPoolRun(true);
   self()->setGlobalStaticBaseRegisterOn(false);

   self()->setGlobalPrivateStaticBaseRegisterOn(false);

   if (!TR::Compiler->target.isLinux())
      self()->setExpandExponentiation();

   self()->setMultiplyIsDestructive();

   static char * noGraFIX= feGetEnv("TR_NOGRAFIX");
   if (!noGraFIX)
      {
      if ( !comp->getOption(TR_DisableLongDispStackSlot) )
         {
         self()->setExtCodeBaseRegisterIsFree(true);
         }
      else
         {
         self()->setExtCodeBaseRegisterIsFree(false);
         }
      }

   self()->setIsOutOfLineHotPath(false);

   if (comp->getOption(TR_DisableRegisterPressureSimulation))
      self()->machine()->initializeGlobalRegisterTable();

   self()->setUsesRegisterPairsForLongs();
   self()->setSupportsDivCheck();
   self()->setSupportsLoweringConstIDiv();
   self()->setSupportsTestUnderMask();

   // Initialize preprologue offset to be 8 bytes for bodyInfo / methodInfo
   self()->setPreprologueOffset(8);

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
   self()->setSupportsBCDToDFPReduction();
   self()->setSupportsIntDFPConversions();

   if (_processorInfo.supportsArch(TR_S390ProcessorInfo::TR_z10))
      {
      self()->setSupportsTranslateAndTestCharString();

      self()->setSupportsBCDToDFPReduction();
      self()->setSupportsIntDFPConversions();

      if (!comp->getOptions()->getOption(TR_DisableTraps) && TR::Compiler->vm.hasResumableTrapHandler(comp))
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

   if (_processorInfo.supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      self()->setSupportsAtomicLoadAndAdd();
      }

   if (_processorInfo.supportsArch(TR_S390ProcessorInfo::TR_zEC12))
      {
      self()->setSupportsZonedDFPConversions();
      if (TR::Compiler->target.cpu.getS390SupportsTM() && !comp->getOption(TR_DisableTM))
         self()->setSupportsTM();
      }

   if(self()->getSupportsTM())
      self()->setSupportsTMHashMapAndLinkedQueue();

   if (_processorInfo.supportsArch(TR_S390ProcessorInfo::TR_z13) && !comp->getOption(TR_DisableArch11PackedToDFP))
      self()->setSupportsFastPackedDFPConversions();

   if (!comp->getOption(TR_DisableShrinkWrapping))
      {
      self()->setSupportsShrinkWrapping();
      // we can use store/load multiple to coalesce reg saves/restores
      // inserted by shrink wrapping
      self()->setUsesLoadStoreMultiple();
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

   if(!(self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z13) && !comp->getOption(TR_DisableZ13)))
     {
     comp->setOption(TR_DisableZ13LoadAndMask);
     comp->setOption(TR_DisableZ13LoadImmediateOnCond);
     }


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

   // This enables the tactical GRA
   self()->setSupportsGlRegDeps();
   if (comp->getOption(TR_DisableRegisterPressureSimulation))
      self()->prepareForGRA();

   self()->addSupportedLiveRegisterKind(TR_GPR);
   self()->addSupportedLiveRegisterKind(TR_FPR);
   self()->addSupportedLiveRegisterKind(TR_GPR64);
   self()->addSupportedLiveRegisterKind(TR_AR);
   self()->addSupportedLiveRegisterKind(TR_VRF);

   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_GPR);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_FPR);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_GPR64);
   self()->setLiveRegisters(new (self()->trHeapMemory()) TR_LiveRegisters(comp), TR_AR);
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

   if (comp->getOptions()->getRegisterAssignmentTraceOption(TR_TraceRARegisterStates))
      {
      self()->setGPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_GPR));
      self()->setFPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_FPR));
      self()->setARegisterIterator(new (self()->trHeapMemory())  TR::RegisterIterator(self()->machine(), TR_AR));
      self()->setHPRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_HPR));
      self()->setVRFRegisterIterator(new (self()->trHeapMemory()) TR::RegisterIterator(self()->machine(), TR_VRF));
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
   return TR::Compiler->target.is64Bit() || self()->use64BitRegsOn32Bit();
   }

TR_GlobalRegisterNumber
OMR::Z::CodeGenerator::getGlobalHPRFromGPR (TR_GlobalRegisterNumber n)
   {
   TR_ASSERT(self()->isGlobalGPR(n), "getGlobalHPRFromGPR:  n != GPR?");
   TR::RealRegister *gpr = toRealRegister(self()->machine()->getGPRFromGlobalRegisterNumber(n));
   TR::RealRegister *hpr = gpr->getHighWordRegister();
   return self()->machine()->getGlobalReg(hpr->getRegisterNumber());
   }

TR_GlobalRegisterNumber
OMR::Z::CodeGenerator::getGlobalGPRFromHPR (TR_GlobalRegisterNumber n)
   {
   TR_ASSERT(self()->isGlobalHPR(n), "getGlobalGPRFromHPR:  n != HPR?");
   //traceMsg(comp(), "getGlobalGPRFromHPR called with n=%d ",n);
   TR::RealRegister *hpr = toRealRegister(self()->machine()->getHPRFromGlobalRegisterNumber(n));
   //traceMsg(comp(), "hpr = %s ", getDebug()->getName(hpr));
   TR::RealRegister *gpr = hpr->getLowWordRegister();
   //traceMsg(comp(), "gpr = %s\n", getDebug()->getName(gpr));
   return self()->machine()->getGlobalReg(gpr->getRegisterNumber());
   }

bool OMR::Z::CodeGenerator::isStackBased(TR::MemoryReference *mr)
   {
   if (mr->getBaseRegister() && mr->getBaseRegister()->getGPRofArGprPair()->getRealRegister()) // need to consider AR register pair as base reg
      {
      return (toRealRegister(mr->getBaseRegister()->getGPRofArGprPair()->getRealRegister())->getRegisterNumber() ==
              self()->getStackPointerRealRegister()->getRegisterNumber());
      }
   return false;
   }

bool OMR::Z::CodeGenerator::prepareForGRA()
   {
   bool enableHighWordGRA = self()->supportsHighWordFacility() && !self()->comp()->getOption(TR_DisableHighWordRA);
   bool enableVectorGRA = self()->getSupportsVectorRegisters() && !self()->comp()->getOption(TR_DisableVectorRegGRA);

   if (!_globalRegisterTable)
      {
      // TBR
      static char * noGraFIX= feGetEnv("TR_NOGRAFIX");
      if (noGraFIX)
         {
         if ( !self()->comp()->getOption(TR_DisableLongDispStackSlot) )
            {
            self()->setExtCodeBaseRegisterIsFree(true);
            }
         else
            {
            self()->setExtCodeBaseRegisterIsFree(false);
            }
         }

      self()->machine()->initializeGlobalRegisterTable();
      self()->setGlobalRegisterTable(self()->machine()->getGlobalRegisterTable());
      self()->setFirstGlobalHPR(self()->machine()->getFirstGlobalHPRRegisterNumber());
      self()->setLastGlobalHPR(self()->machine()->getLastGlobalHPRRegisterNumber());
      self()->setLastGlobalGPR(self()->machine()->getLastGlobalGPRRegisterNumber());
      self()->setLast8BitGlobalGPR(self()->machine()->getLast8BitGlobalGPRRegisterNumber());
      self()->setFirstGlobalFPR(self()->machine()->getFirstGlobalFPRRegisterNumber());
      self()->setLastGlobalFPR(self()->machine()->getLastGlobalFPRRegisterNumber());

      self()->setFirstGlobalVRF(self()->machine()->getFirstGlobalVRFRegisterNumber());
      self()->setLastGlobalVRF(self()->machine()->getLastGlobalVRFRegisterNumber());
      self()->setFirstGlobalAR(self()->machine()->getFirstGlobalAccessRegisterNumber());
      self()->setLastGlobalAR(self()->machine()->getLastGlobalAccessRegisterNumber());

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

      static char * disableGRAOnFirstBB = feGetEnv("TR390GRAONFIRSTBB");

      // plx has stack-based linkage
      if (!disableGRAOnFirstBB)
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
      if (enableHighWordGRA)
         {
         _globalGPRsPreservedAcrossCalls.init(NUM_S390_GPR+NUM_S390_FPR+NUM_S390_AR+NUM_S390_HPR, self()->trMemory());
         _globalFPRsPreservedAcrossCalls.init(NUM_S390_GPR+NUM_S390_FPR+NUM_S390_AR+NUM_S390_HPR, self()->trMemory());
         }
      else
         {
         _globalGPRsPreservedAcrossCalls.init(NUM_S390_GPR+NUM_S390_FPR+NUM_S390_AR, self()->trMemory());
         _globalFPRsPreservedAcrossCalls.init(NUM_S390_GPR+NUM_S390_FPR+NUM_S390_AR, self()->trMemory());
         }

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
               if (enableHighWordGRA && self()->getFirstGlobalHPR() <= grn)
                  {
                  // this is a bit tricky, we consider Global HPRs part of Global GPRs
                  _globalRegisterBitVectors[ TR_hprSpill ].set(grn);
                  }
               else
                  {
                  _globalRegisterBitVectors[ TR_gprSpill ].set(grn);
                  }
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
               if ((enableHighWordGRA) && (grn >= self()->getFirstGlobalGPR() && grn <= self()->getLastGlobalGPR()))
                  {
                  TR_GlobalRegisterNumber grnHPR = self()->getFirstGlobalHPR() - self()->getFirstGlobalGPR() + grn;
                  _globalRegisterBitVectors[ TR_linkageSpill  ].set(grnHPR);
                  }
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
TR::S390PrivateLinkage * OMR::Z::CodeGenerator::getS390PrivateLinkage() {return TR::toS390PrivateLinkage(self()->getLinkage());}


TR::SystemLinkage * OMR::Z::CodeGenerator::getS390SystemLinkage() {return toSystemLinkage(self()->getLinkage());}

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
TR::RealRegister * OMR::Z::CodeGenerator::getExtCodeBaseRealRegister()
   {return self()->getS390Linkage()->getExtCodeBaseRealRegister();}
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

   if (_processorInfo.supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   else if (_processorInfo.supportsArch(TR_S390ProcessorInfo::TR_z10))
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
      if (self()->isAddressOfStaticSymRefWithLockedReg(symRef))
         return false;

      if (self()->isAddressOfPrivateStaticSymRefWithLockedReg(symRef))
         return false;
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

/**
 * Consider overflow when generating IMMed instruction for PLX
 */
bool
OMR::Z::CodeGenerator::mayImmedInstructionCauseOverFlow(TR::Node * node)
   {

   return false;
   }

bool
OMR::Z::CodeGenerator::canUseImmedInstruction( int64_t value )
   {
   if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
      return true;
   else
      return false;
   }


// Use with care, lightly tested
// Z
void OMR::Z::CodeGenerator::changeRegisterKind(TR::Register * temp, TR_RegisterKinds rk)
   {
   if (rk == temp->getKind()) return;
   if (_liveRegisters[temp->getKind()] && _liveRegisters[rk])
      TR_LiveRegisters::moveRegToList(_liveRegisters[temp->getKind()], _liveRegisters[rk], temp);
   temp->setKind(rk);
   }

void
OMR::Z::CodeGenerator::ensure64BitRegister(TR::Register *reg)
   {
   if (self()->use64BitRegsOn32Bit() && TR::Compiler->target.is32Bit() &&
       reg->getKind() == TR_GPR)
      self()->changeRegisterKind(reg, TR_GPR64);
   }

bool
OMR::Z::CodeGenerator::isAddMemoryUpdate(TR::Node * node, TR::Node * valueChild)
   {
   static char * disableASI = feGetEnv("TR_DISABLEASI");

   if (_processorInfo.supportsArch(TR_S390ProcessorInfo::TR_z10))
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
         else if (TR::Compiler->target.is64Bit() && (valueChild->getOpCodeValue() == TR::ladd || valueChild->getOpCodeValue() == TR::lsub))
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
   TR::ResolvedMethodSymbol * methodSymbol = self()->comp()->getJittedMethodSymbol();
   TR::Node * startNode = self()->comp()->getStartTree()->getNode();
   TR::Instruction * cursor = NULL;

   self()->setCurrentBlockIndex(startNode->getBlock()->getNumber());

   if (self()->comp()->getJittedMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      if(self()->comp()->getJittedMethodSymbol()->isJNI() &&
         !self()->comp()->getOption(TR_MimicInterpreterFrameShape))
        {
        intptrj_t jniMethodTargetAddress = (intptrj_t)methodSymbol->getResolvedMethod()->startAddressForJNIMethod(self()->comp());
        if(TR::Compiler->target.is64Bit())
          {
          cursor = new (self()->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, startNode, UPPER_4_BYTES(jniMethodTargetAddress), cursor, self());
          cursor = new (self()->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, startNode, LOWER_4_BYTES(jniMethodTargetAddress), cursor, self());
          }
       else
          cursor = new (self()->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, startNode, UPPER_4_BYTES(jniMethodTargetAddress), cursor, self());
       }

      _returnTypeInfoInstruction = new (self()->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, startNode, 0, NULL, cursor, self());
      generateS390PseudoInstruction(self(), TR::InstOpCode::PROC, startNode);
      }
   else
      {
      generateS390PseudoInstruction(self(), TR::InstOpCode::PROC, startNode);
      }

   //TODO vm thread work: need to add a vm thread dependency at the top of the instruction strea.
   //x86 does this by adding a dependency on the TR::InstOpCode::PROC instruction.  This causes an assertion
   //failure on 390 (error: node does not have block set).  One solution is to add the dependency
   //to the first instruction that has a block set.  This can be implemented in endInstructionSelection().

   // Reset the VirtualGuardLabel pointeres inside all TR_Blocks to 0
   //
#if TODO  // need to understand this
   for (TR::Block *block = comp()->getStartBlock(); block; block = block->getNextBlock())
      {
      block->setVirtualGuardLabel(0);
      }
#endif
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

         // if we encountered a call, insert after the call or after the lable that follows the call instr.
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
         TR::RealRegister * spReg =  self()->getS390Linkage()->getS390RealRegister(self()->getS390Linkage()->getStackPointerRegister());
         int32_t frameSize = self()->getFrameSizeInBytes();

         TR::MemoryReference * tempMR = generateS390MemoryReference(spReg, frameSize, self());
         tempMR->setCreatedDuringInstructionSelection();
         cursor = generateRXYInstruction(self(), TR::InstOpCode::getExtendedLoadOpCode(), node,
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
   intptrj_t codeCacheBase, codeCacheTop;
   // _currentCodeCache in the compilation object is set right before Instruction selection.
   // if it returns null, we have a call during optimizer stage, in which case, we use
   // the next best estimate which is the base of our first code cache.
   if (self()->comp()->getCurrentCodeCache() == NULL)
      {
      codeCacheBase = (intptrj_t)self()->fe()->getCodeCacheBase();
      codeCacheTop = (intptrj_t)self()->fe()->getCodeCacheTop();
      }
   else
      {
      codeCacheBase = (intptrj_t)self()->fe()->getCodeCacheBase(self()->comp()->getCurrentCodeCache());
      codeCacheTop = (intptrj_t)self()->fe()->getCodeCacheTop(self()->comp()->getCurrentCodeCache());
      }

   intptrj_t offset1 = (intptrj_t) data->_callSymRef->getMethodAddress() - codeCacheBase;
   intptrj_t offset2 = (intptrj_t) data->_callSymRef->getMethodAddress() - codeCacheTop;
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
 * Do logic for determining if we want to make ext code base reg available
 */
bool
OMR::Z::CodeGenerator::isExtCodeBaseFreeForAssignment()
   {
   bool extCodeBaseIsFree = false;

   // Codegen's extCodeBase flag has ultimate veto
   // For N3+ we use a stack slot to spill the ExtCodeBase, compute the disp, use it, and unspill
   //   when LongDispStackSlot is enabled
   // (see TR::MemoryReference::generateBinaryEncoding(..)
   //
   if ( !self()->comp()->getOption(TR_DisableLongDispStackSlot) && self()->getExtCodeBaseRegisterIsFree())
      {
      extCodeBaseIsFree = true;
      }

   return extCodeBaseIsFree;
   }

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
   else if (self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) && !self()->anyLitPoolSnippets())
      {
      litPoolRegIsFree = true;
      }

   return litPoolRegIsFree;
   }


TR::Instruction *
OMR::Z::CodeGenerator::upgradeToHPRInstruction(TR::Instruction * inst)
   {
   TR::Instruction * s390Inst = inst;
   TR::Register * targetReg = s390Inst->getRegisterOperand(1);
   TR::Register * srcReg = s390Inst->getRegisterOperand(2);
   bool targetUpgradable = false;
   bool srcUpgradable = false;
   TR::InstOpCode::Mnemonic newOpCode  = TR::InstOpCode::BAD;
   TR::Instruction * newInst = NULL;
   TR::Node * node = inst->getNode();
   int32_t immValue = 0;
   TR::MemoryReference *mr;

   if (s390Inst->getOpCode().is64bit() ||
       (targetReg && targetReg->is64BitReg()) ||
       (srcReg && srcReg->is64BitReg()))
      {
      return NULL;
      }

   if (targetReg && targetReg->isHighWordUpgradable())
      {
      targetUpgradable = true;
      TR::RealRegister * targetRealReg = targetReg->getAssignedRealRegister();

      if (targetRealReg)
         {
         if (targetRealReg->getState() == TR::RealRegister::Locked)
            {
            targetUpgradable = false;
            }
         }
      }

   if (srcReg && srcReg->isHighWordUpgradable())
      {
      srcUpgradable = true;

      TR::RealRegister * srcRealReg = srcReg->getAssignedRealRegister();;
      if (srcRealReg)
         {
         if (srcRealReg->getState() == TR::RealRegister::Locked)
            {
            srcUpgradable = false;
            }
         }
      }

   if (targetUpgradable)
      {
      switch (s390Inst->getOpCodeValue())
         {
         case TR::InstOpCode::AR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::AHHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::AHHLR;
               }
            newInst = generateRRRInstruction(self(), newOpCode, node, targetReg, targetReg, srcReg, inst->getPrev());
            srcReg->decTotalUseCount();
            targetReg->decTotalUseCount();
            targetReg->incFutureUseCount();
            break;
         case TR::InstOpCode::ALR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::ALHHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::ALHHLR;
               }
            newInst = generateRRRInstruction(self(), newOpCode, node, targetReg, targetReg, srcReg, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            targetReg->incFutureUseCount();
            break;
         case TR::InstOpCode::AHI:
         case TR::InstOpCode::AFI:
            newOpCode = TR::InstOpCode::AIH;
            immValue = ((TR::S390RIInstruction*)inst)->getSourceImmediate();
            // signed extend 32-bit
            newInst = generateRILInstruction(self(), newOpCode, node, targetReg, immValue,inst->getPrev());
            targetReg->decTotalUseCount();
            break;
         case TR::InstOpCode::BRCT:
            newOpCode = TR::InstOpCode::BRCTH;
            //BRCTH has extended immediate
            newInst= generateS390BranchInstruction(self(), TR::InstOpCode::BRCTH, node, targetReg, ((TR::S390LabeledInstruction*)inst)->getLabelSymbol(), inst->getPrev());
            targetReg->decTotalUseCount();
            break;
         case TR::InstOpCode::CR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::CHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::CHLR;
               }
            newInst = generateRRInstruction(self(), newOpCode, node, targetReg, srcReg, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::CLR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::CLHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::CLHLR;
               }
            newInst = generateRRInstruction(self(), newOpCode, node, targetReg, srcReg, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::C:
         case TR::InstOpCode::CY:
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newOpCode = TR::InstOpCode::CHF;
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::CL:
         case TR::InstOpCode::CLY:
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newOpCode = TR::InstOpCode::CLHF;
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::CFI:
            immValue = ((TR::S390RILInstruction*)inst)->getSourceImmediate();
            newOpCode = TR::InstOpCode::CIH;
            newInst = generateRILInstruction(self(), newOpCode, node, targetReg, immValue, inst->getPrev());
            targetReg->decTotalUseCount();
            break;
         case TR::InstOpCode::CLFI:
            immValue = ((TR::S390RILInstruction*)inst)->getSourceImmediate();
            newOpCode = TR::InstOpCode::CLIH;
            newInst = generateRILInstruction(self(), newOpCode, node, targetReg, immValue, inst->getPrev());
            targetReg->decTotalUseCount();
            break;
         case TR::InstOpCode::LB:
            newOpCode = TR::InstOpCode::LBH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::LH:
         case TR::InstOpCode::LHY:
            newOpCode = TR::InstOpCode::LHH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::L:
         case TR::InstOpCode::LY:
            newOpCode = TR::InstOpCode::LFH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::LAT:
            newOpCode = TR::InstOpCode::LFHAT;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::LLC:
            newOpCode = TR::InstOpCode::LLCH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::LLH:
            newOpCode = TR::InstOpCode::LLHH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::LR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::LHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::LHLR;
               }
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::LLCR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::LLCHHR;
               srcReg->decTotalUseCount();
               }
            else
               {
               newOpCode = TR::InstOpCode::LLCHLR;
               }
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::LLHR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::LLHHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::LLHHLR;
               }
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::LHI:
            //IIHF need to be sign-extend
            newOpCode = TR::InstOpCode::IIHF;
            immValue = ((TR::S390RIInstruction*)inst)->getSourceImmediate();
            newInst = generateRILInstruction(self(), newOpCode, node, targetReg, immValue, inst->getPrev());
            targetReg->decTotalUseCount();
            break;
            /*-------- these guys are cracked instructions, not worth it
         case TR::InstOpCode::NR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::NHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::NHLR;
               }
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::OR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::OHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::OHLR;
               }
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::XR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::XHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::XHLR;
               }
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
            --------------*/
         case TR::InstOpCode::ST:
         case TR::InstOpCode::STY:
            newOpCode = TR::InstOpCode::STFH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::STC:
         case TR::InstOpCode::STCY:
            newOpCode = TR::InstOpCode::STCH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::STH:
         case TR::InstOpCode::STHY:
            newOpCode = TR::InstOpCode::STHH;
            mr = ((TR::S390RXInstruction *)inst)->getMemoryReference();
            mr->resetMemRefUsedBefore();
            // mr re-use?
            newInst = generateRXYInstruction(self(), newOpCode, node, targetReg, mr, inst->getPrev());
            targetReg->decTotalUseCount();
            // mem ref need to dec ref count too
            if (mr->getBaseRegister())
               {
               mr->getBaseRegister()->decTotalUseCount();
               }
            if (mr->getIndexRegister())
               {
               mr->getIndexRegister()->decTotalUseCount();
               }
            break;
         case TR::InstOpCode::SR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::SHHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::SHHLR;
               }
            newInst = generateRRRInstruction(self(), newOpCode, node, targetReg, targetReg, srcReg, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            targetReg->incFutureUseCount();
            break;
         case TR::InstOpCode::SLR:
            if (srcUpgradable)
               {
               newOpCode = TR::InstOpCode::SLHHHR;
               }
            else
               {
               newOpCode = TR::InstOpCode::SLHHLR;
               }
            newInst = generateRRRInstruction(self(), newOpCode, node, targetReg, targetReg, srcReg, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            targetReg->incFutureUseCount();
            break;
         case TR::InstOpCode::SLFI:
            //ALSIH
            newOpCode = TR::InstOpCode::ALSIH;
            immValue = ((TR::S390RILInstruction*)inst)->getSourceImmediate();
            newInst = generateRILInstruction(self(), newOpCode, node, targetReg, -immValue, inst->getPrev());
            targetReg->decTotalUseCount();
            break;
         }
      }
   else if (srcUpgradable)
      {
      switch (s390Inst->getOpCodeValue())
         {
         case TR::InstOpCode::LR:
            newOpCode = TR::InstOpCode::LLHFR;
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::LLCR:
            newOpCode = TR::InstOpCode::LLCLHR;
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::LLHR:
            newOpCode = TR::InstOpCode::LLHLHR;
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
            /*---- these guys are cracked instructions, not worth it
         case TR::InstOpCode::NR:
            newOpCode = TR::InstOpCode::NLHR;
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::OR:
            newOpCode = TR::InstOpCode::OLHR;
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
         case TR::InstOpCode::XR:
            newOpCode = TR::InstOpCode::XLHR;
            newInst = generateExtendedHighWordInstruction(node, self(), newOpCode, targetReg, srcReg, 0, inst->getPrev());
            targetReg->decTotalUseCount();
            srcReg->decTotalUseCount();
            break;
            ----*/
         }
      }

   if (newInst)
      {
      // make sure the registers will keep on getting upgraded for other instructions
      if (targetUpgradable)
         targetReg->setIsNotHighWordUpgradable(false);
      if (srcUpgradable)
         srcReg->setIsNotHighWordUpgradable(false);

      TR::Instruction * s390NewInst = newInst;
      //traceMsg(comp(), "\n Old inst: %p, new inst : %p", inst, newInst);
      s390NewInst->setBlockIndex(s390Inst->getBlockIndex());
      if (s390Inst->getDependencyConditions())
         s390NewInst->setDependencyConditionsNoBookKeeping(s390Inst->getDependencyConditions());
      if (s390Inst->throwsImplicitNullPointerException())
         s390NewInst->setThrowsImplicitNullPointerException();
      if (s390Inst->throwsImplicitException())
         s390NewInst->setThrowsImplicitException();
      if (s390Inst->isOutOfLineEX())
         s390NewInst->setOutOfLineEX();
      if (s390Inst->isStartInternalControlFlow())
         s390NewInst->setStartInternalControlFlow();
      if (s390Inst->isEndInternalControlFlow())
         s390NewInst->setEndInternalControlFlow();
      if (s390Inst->getIndex())
         s390NewInst->setIndex(s390Inst->getIndex());
      if (s390Inst->getGCMap())
         s390NewInst->setGCMap(s390Inst->getGCMap());
      if (s390Inst->needsGCMap())
         s390NewInst->setNeedsGCMap(s390Inst->getGCRegisterMask());

      self()->replaceInst(inst, newInst);

      if (self()->getDebug())
         self()->getDebug()->addInstructionComment(s390NewInst, "HPR Upgraded");

      return newInst;
      }

   return NULL;
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
    if (insertInstr->isStartInternalControlFlow())
      {
      nestingDepth--;
      if(nestingDepth==0)
        break;
      }
    else if (insertInstr->isEndInternalControlFlow())
      nestingDepth++;
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
   if ( self()->comp()->getOption(TR_Enable390FreeVMThreadReg))
      {
      self()->getVMThreadRegister()->setFutureUseCount(self()->getVMThreadRegister()->getTotalUseCount());
      }
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
      _extentOfLitPool = self()->setEstimatedOffsetForTargetAddressSnippets();
      _extentOfLitPool = self()->setEstimatedOffsetForConstantDataSnippets(_extentOfLitPool);

      TR::Linkage *s390PrivateLinkage = self()->getS390Linkage();

      if ( self()->comp()->getOption(TR_Enable390AccessRegs) )
         {
         s390PrivateLinkage->lockAccessRegisters();
         }

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

      // Check to see if we need to lock the ext code base reg
      if ( self()->isExtCodeBaseFreeForAssignment() )
         {
         s390PrivateLinkage->unlockRegister(s390PrivateLinkage->getExtCodeBaseRealRegister());
         }
      else
         {
         s390PrivateLinkage->lockRegister(s390PrivateLinkage->getExtCodeBaseRealRegister());
         }
      }

   // Lock vector registers. This is done for testing purposes and to artificially increase register pressure
   // We go from the end since in most cases, we want to test functionality of the first half i.e 0-15 for overlap with FPR etc
   if (TR::Options::_numVecRegsToLock > 0  &&
       TR::Options::_numVecRegsToLock <= (TR::RealRegister::LastAssignableVRF - TR::RealRegister::FirstAssignableVRF))
      {
      for(int32_t i = TR::RealRegister::LastAssignableVRF;
              i > (TR::RealRegister::LastAssignableVRF - TR::Options::_numVecRegsToLock);
              i--)
         {

         self()->machine()->getRegisterFile(i)->setState(TR::RealRegister::Locked);
         }
      }
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
         if (self()->supportsHighWordFacility() && !self()->comp()->getOption(TR_DisableHighWordRA))
            rks = (TR_RegisterKinds) ((int)rks | TR_HPR_Mask);

         self()->getDebug()->startTracingRegisterAssignment("backward", rks);
         }
      }

   // pre-pass to set all internal control flow reg deps to 64bit regs
   TR::Instruction * currInst = instructionCursor;
   if (self()->supportsHighWordFacility() && !self()->comp()->getOption(TR_DisableHighWordRA) && TR::Compiler->target.is64Bit())
      {
      while (currInst)
         {
         TR::LabelSymbol *labelSym = currInst->isLabel() ? toS390LabelInstruction(currInst)->getLabelSymbol() : NULL;
         if (currInst->isEndInternalControlFlow() || (labelSym && labelSym->isEndInternalControlFlow()))
            {
            if (currInst->getDependencyConditions())
               {
               // traceMsg(comp(), "\nsetting all conds on [%p] to be 64-bit regs", currInst);
               currInst->getDependencyConditions()->set64BitRegisters();
               }
            }
         currInst = currInst->getPrev();
         }
      }

   // Set all CFG Blocks unvisited
   for (currBlock = self()->comp()->getStartBlock(); currBlock != NULL; currBlock = currBlock->getNextBlock())
     {
     currBlock->setHasBeenVisited(false);
     }

   // Be stingy with allocating the blocked list (for HPR upgrades). Space requirement is small, but it adds up
   self()->machine()->allocateUpgradedBlockedList(new (self()->comp()->trStackMemory()) TR_Stack<TR::RealRegister *>(self()->comp()->trMemory(), 16, true, stackAlloc));

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
            if (self()->machine()->getRegisterFile(i)->getState() == TR::RealRegister::Assigned)
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
            realReg = self()->machine()->getRegisterFile(i);

            if ( realReg->getState() == TR::RealRegister::Free && realReg->getHighWordRegister()->getState() == TR::RealRegister::Free)
               {
               dcbInstr->setAssignableReg(realReg);
               realReg->setHasBeenAssignedInMethod(true);
               break;
               }
            }

            self()->traceRegisterAssignment("BEST FREE REG for DCB is %R", dcbInstr->getAssignableReg());
         }

      self()->tracePreRAInstruction(instructionCursor);

      if (self()->supportsHighWordFacility() && !self()->comp()->getOption(TR_DisableHighWordRA) && !self()->comp()->getOption(TR_DisableHPRUpgrade))
         {
         TR::Instruction * newInst = self()->upgradeToHPRInstruction(instructionCursor);
         if (newInst)
            {
            instructionCursor = newInst;
            }
         }
      if (self()->supportsHighWordFacility() && self()->comp()->getOption(TR_DisableHighWordRA))
         self()->setAvailableHPRSpillMask(0xffff0000);

      prevInstruction = instructionCursor->getPrev();

      if (instructionCursor->getNode()->getOpCodeValue() == TR::BBEnd)
         self()->comp()->setCurrentBlock(instructionCursor->getNode()->getBlock());

      // Main register assignment procedure
      self()->setCurrentBlockIndex(instructionCursor->getBlockIndex());
      instructionCursor->assignRegisters(TR_GPR);

      handleLoadWithRegRanges(instructionCursor, self());

      // Maintain Internal Control Flow Depth

      // Track internal control flow on instructions
      if (instructionCursor->isStartInternalControlFlow())
         {
         _internalControlFlowNestingDepth--;
         if(_internalControlFlowNestingDepth==0)
            self()->endInternalControlFlow(instructionCursor);        // Walking backwards so start is end
         }
      if (instructionCursor->isEndInternalControlFlow())
         {
         _internalControlFlowNestingDepth++;
         self()->startInternalControlFlow(instructionCursor);
         }

      // Track internal control flow on labels
      if (instructionCursor->getOpCode().getOpCodeValue() == TR::InstOpCode::LABEL)
         {
         TR::S390LabelInstruction *li = toS390LabelInstruction(instructionCursor);

         if (li->getLabelSymbol() != NULL)
            {
            if (li->getLabelSymbol()->isStartInternalControlFlow())
               {
               TR_ASSERT(!li->isStartInternalControlFlow(),
                       "An instruction should not be both the start of internal control flow and have a lebel that is pegged as the start of internal control flow, because we should only count it once");
               _internalControlFlowNestingDepth--;
               if (_internalControlFlowNestingDepth == 0)
                  self()->endInternalControlFlow(instructionCursor);        // Walking backwards so start is end
               }
            if (li->getLabelSymbol()->isEndInternalControlFlow())
               {
               TR_ASSERT(!li->isEndInternalControlFlow(),
                       "An instruction should not be both the end of internal control flow and have a lebel that is pegged as the end of internal control flow, because we should only count it once");
               _internalControlFlowNestingDepth++;
               self()->startInternalControlFlow(instructionCursor);
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
         TR_ASSERT(self()->machine()->getS390RealRegister(i)->getState() != TR::RealRegister::Assigned,
                    "ASSERTION: GPR registers are still assigned at end of first RA pass! %s regNum=%s\n", self()->comp()->signature(), self()->getDebug()->getName(self()->machine()->getS390RealRegister(i)));
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

///////////////////////////////////////////////////////////////////////////////

TR::Instruction* realInstruction(TR::Instruction* inst, bool forward)
   {
   while (inst && (inst->getKind() == TR::Instruction::IsPseudo      ||
                   inst->getKind() == TR::Instruction::IsNotExtended ||
                   inst->getKind() == TR::Instruction::IsLabel))
      {
      inst = forward ? inst->getNext() : inst->getPrev();
      }

   return inst;
   }

TR::Instruction* realInstructionWithLabels(TR::Instruction* inst, bool forward)
   {
   while (inst && (inst->getKind() == TR::Instruction::IsPseudo && !inst->isAsmGen() ||
                   inst->getKind() == TR::Instruction::IsNotExtended))
      {
      inst = forward ? inst->getNext() : inst->getPrev();
      }

   return inst;
   }

TR::Instruction* realInstructionWithLabelsAndRET(TR::Instruction* inst, bool forward)
   {
   while (inst && (inst->getKind() == TR::Instruction::IsPseudo && !inst->isAsmGen() ||
                   inst->getKind() == TR::Instruction::IsNotExtended) && !inst->isRet())
      {
      inst = forward ? inst->getNext() : inst->getPrev();
      }

   return inst;
   }

TR_S390Peephole::TR_S390Peephole(TR::Compilation* comp, TR::CodeGenerator *cg)
   : _fe(comp->fe()),
     _outFile(comp->getOutFile()),
     _cursor(cg->getFirstInstruction()),
     _cg(cg)
   {
   }

bool
TR_S390Peephole::isBarrierToPeepHoleLookback(TR::Instruction *current)
   {
   if (NULL == current) return true;

   TR::Instruction *s390current = current;

   if (s390current->isLabel()) return true;
   if (s390current->isCall())  return true;
   if (s390current->isAsmGen()) return true;
   if (s390current->isBranchOp()) return true;
   if (s390current->getOpCodeValue() == TR::InstOpCode::DCB) return true;

   return false;
   }

bool
TR_S390Peephole::AGIReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable AGIReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed=false;
   int32_t windowSize=0;
   const int32_t MaxWindowSize=8; // Look down 8 instructions (4 cycles)
   const int32_t MaxLAWindowSize=6; // Look up 6 instructions (3 cycles)
   static char *disableRenaming = feGetEnv("TR_DisableRegisterRenaming");

   TR::InstOpCode::Mnemonic curOp = (_cursor)->getOpCodeValue();

   // LR Reduction or LGFR reduction could have changed the original instruction
   // make sure AGI reduction only works on these OpCodes
   if (!(curOp == TR::InstOpCode::LR || curOp == TR::InstOpCode::LTR ||
         curOp == TR::InstOpCode::LGR || curOp == TR::InstOpCode::LTGR))
      {
      return false;
      }

   if (disableRenaming!=NULL)
      return false;

   TR::Register *lgrTargetReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(1);
   TR::Register *lgrSourceReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(2);

   TR::RealRegister *gpr0 = _cg->machine()->getS390RealRegister(TR::RealRegister::GPR0);

   // no renaming possible if both target and source are the same
   // this can happend with LTR and LTGR
   // or if source reg is gpr0
   if  (toRealRegister(lgrSourceReg)==gpr0 || (lgrTargetReg==lgrSourceReg) ||
        toRealRegister(lgrTargetReg)==_cg->getStackPointerRealRegister(NULL))
      return performed;

   TR::Instruction* current=_cursor->getNext();

   // if:
   //  1) target register is invalidated, or
   //  2) current instruction is a branch or call
   // then cannot continue.
   // if:
   //  1) source register is invalidated, or
   //  2) current instruction is a label
   // then cannot rename registers, but might be able to replace
   // _cursor instruction with LA

   bool reachedLabel = false;
   bool reachedBranch = false;
   bool sourceRegInvalid = false;
   bool attemptLA = false;

   while ((current != NULL) &&
         !current->matchesTargetRegister(lgrTargetReg) &&
         !isBarrierToPeepHoleLookback(current) &&
         windowSize<MaxWindowSize)
      {
      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         return false;
         }

      // if we reach a label or the source reg becomes invalidated
      // then we cannot rename regs in mem refs, but we could still try using LA
      reachedLabel = reachedLabel || current->isLabel();
      reachedBranch = reachedBranch || (current->isBranchOp() && !(current->isExceptBranchOp()));

      // does instruction load or store? otherwise just ignore and move to next instruction
      if (current->isLoad() || current->isStore())
         {
         TR::MemoryReference *mr = current->getMemoryReference();
         // do it for all memory reference
         while (mr)
            {
            if (mr && mr->getBaseRegister()==lgrTargetReg)
               {
               // here we would like to swap registers, see if we can
               if(!reachedLabel && !reachedBranch && !sourceRegInvalid)
                  {
                  if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
                  if(performTransformation(comp(), "O^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
                     {
                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nchanging base reg of ");
                        printInstr(comp(), current);
                        }
                     // Update Base Register
                     mr->setBaseRegister(lgrSourceReg, _cg);

                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nto");
                        printInstr(comp(), current);
                        }
                     performed = true;
                     }
                  }
               else // couldn't swap regs, but potentially can use LA
                  {
                  attemptLA = true;
                  }
               }
            if (mr && mr->getIndexRegister()==lgrTargetReg)
               {
               // here we would like to swap registers, see if we can
               if(!reachedLabel && !reachedBranch && !sourceRegInvalid)
                  {
                  if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
                  if(performTransformation(comp(), "O^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
                     {
                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nchanging index reg of ");
                        printInstr(comp(), current);
                        }
                     // Update Index Register
                     mr->setIndexRegister(lgrSourceReg);

                     if (comp()->getOption(TR_TraceCG))
                        {
                        printInfo("\nto");
                        printInstr(comp(), current);
                        }
                     performed=true;
                     }
                  }
               else // couldn't swap regs, but potentially can use LA
                  {
                  attemptLA = true;
                  }
               }
            if (mr == current->getMemoryReference())
               mr = current->getMemoryReference2();
            else
               mr = NULL;
            }
         }

         // If Current instruction invalidates the source register, subsequent instructions' memref cannot be renamed.
         sourceRegInvalid = sourceRegInvalid || current->matchesTargetRegister(lgrSourceReg);

         current=current->getNext();
         windowSize++;
      }

   // We can switch the _cursor to instruction to LA if:
   // 1. The opcode is LGR
   // 2. The target is 64-bit (because LA sets the upppermost bit to 0 on 31-bit)
   attemptLA &= _cursor->getOpCodeValue() == TR::InstOpCode::LGR && TR::Compiler->target.is64Bit();
   if (attemptLA)
      {
      // in order to switch the instruction to LA, we check to see that
      // we are eliminating the AGI, not just propagating it up in the code
      // so we check for labels, source register invalidation, and routine calls
      // (basically the opposite of the above while loop condition)
      current = _cursor->getPrev();
      windowSize = 0;
      while ((current != NULL) &&
      !isBarrierToPeepHoleLookback(current) &&
      !current->matchesTargetRegister(lgrSourceReg) &&
      windowSize<MaxLAWindowSize)
         {
         current = current->getPrev();
         windowSize++;
         }

      // if we reached the end of the loop and didn't find a conflict, switch the instruction to LA
      if (windowSize==MaxLAWindowSize && performTransformation(comp(), "\nO^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: AGI Reduction on %p.\n", current))
            {
            if (comp()->getOption(TR_TraceCG))
               {
               printInstr(comp(), _cursor);
               printInfo("\nAGI Reduction by switching the above instruction to LA");
               }

            // create new instruction and delete old one
            TR::MemoryReference* mr = generateS390MemoryReference(lgrSourceReg, 0, _cg);
            TR::Instruction* oldCursor = _cursor;
            _cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, comp()->getStartTree()->getNode(), lgrTargetReg, mr, _cursor->getPrev());
            _cursor->setNext(oldCursor->getNext());
            _cg->deleteInst(oldCursor);

            performed = true;
            }
         }
      }

   return performed;
   }

/**
 * \brief Swaps guarded storage loads with regular loads and a software read barrier
 *
 * \details
 * This function swaps LGG/LLGFSG with regular loads and software read barrier sequence
 * for runs with -Xgc:concurrentScavenge on hardware that doesn't support guarded storage facility.
 * The sequence first checks if concurrent scavange is in progress and if the current object pointer is
 * in the evacuate space then calls the GC helper to update the object pointer.
 */

bool
TR_S390Peephole::replaceGuardedLoadWithSoftwareReadBarrier()
   {
   if (!TR::Compiler->om.shouldReplaceGuardedLoadWithSoftwareReadBarrier())
      {
      return false;
      }

   auto* concurrentScavangeNotActiveLabel = generateLabelSymbol(_cg);
   TR::S390RXInstruction *load = static_cast<TR::S390RXInstruction*> (_cursor);
   TR::MemoryReference *loadMemRef = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);
   TR::Register *loadTargetReg = _cursor->getRegisterOperand(1);
   TR::Register *vmReg = _cg->getLinkage()->getMethodMetaDataRealRegister();
   TR::Register *raReg = _cg->machine()->getS390RealRegister(_cg->getReturnAddressRegister());
   TR::Instruction* prev = load->getPrev();

   // If guarded load target and mem ref registers are the same,
   // preserve the register before overwriting it, since we need to repeat the load after calling the GC helper.
   bool shouldPreserveLoadReg = (loadMemRef->getBaseRegister() == loadTargetReg);
   if (shouldPreserveLoadReg) {
      TR::MemoryReference *gsIntermediateAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSIntermediateResultOffset(comp()), _cg);
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, load->getNode(), loadTargetReg, gsIntermediateAddrMemRef, prev);
      prev = _cursor;
   }

   if (load->getOpCodeValue() == TR::InstOpCode::LGG)
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, load->getNode(), loadTargetReg, loadMemRef, prev);
      }
   else
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LLGF, load->getNode(), loadTargetReg, loadMemRef, prev);
      _cursor = generateRSInstruction(_cg, TR::InstOpCode::SLLG, load->getNode(), loadTargetReg, loadTargetReg, TR::Compiler->om.compressedReferenceShift(), _cursor);
      }

   // Check if concurrent scavange is in progress and if object pointer is in the evacuate space
   TR::MemoryReference *privFlagMR = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetConcurrentScavengeActiveByteAddressOffset(comp()), _cg);
   _cursor = generateSIInstruction(_cg, TR::InstOpCode::TM, load->getNode(), privFlagMR, 0x00000002, _cursor);
   _cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   _cursor = generateRXInstruction(_cg, TR::InstOpCode::CG, load->getNode(), loadTargetReg,
   		generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetEvacuateBaseAddressOffset(comp()), _cg), _cursor);
   _cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   _cursor = generateRXInstruction(_cg, TR::InstOpCode::CG, load->getNode(), loadTargetReg,
   		generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetEvacuateTopAddressOffset(comp()), _cg), _cursor);
   _cursor = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC2, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   // Save result of LA to gsParameters.operandAddr as invokeJ9ReadBarrier helper expects it to be set
   TR::MemoryReference *loadAddrMemRef = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);
   _cursor = generateRXInstruction(_cg, TR::InstOpCode::LA, load->getNode(), loadTargetReg, loadAddrMemRef, _cursor);
   TR::MemoryReference *gsOperandAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSOperandAddressOffset(comp()), _cg);
   _cursor = generateRXInstruction(_cg, TR::InstOpCode::STG, load->getNode(), loadTargetReg, gsOperandAddrMemRef, _cursor);

   // Use raReg to call handleReadBarrier helper, preserve raReg before the call in the load reg
   _cursor = generateRRInstruction(_cg, TR::InstOpCode::LGR, load->getNode(), loadTargetReg, raReg, _cursor);
   TR::MemoryReference *gsHelperAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSHandlerAddressOffset(comp()), _cg);
   _cursor = generateRXYInstruction(_cg, TR::InstOpCode::LG, load->getNode(), raReg, gsHelperAddrMemRef, _cursor);
   _cursor = new (_cg->trHeapMemory()) TR::S390RRInstruction(TR::InstOpCode::BASR, load->getNode(), raReg, raReg, _cursor, _cg);
   _cursor = generateRRInstruction(_cg, TR::InstOpCode::LGR, load->getNode(), raReg, loadTargetReg, _cursor);

   if (shouldPreserveLoadReg) {
      TR::MemoryReference * restoreBaseRegAddrMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetGSIntermediateResultOffset(comp()), _cg);
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, load->getNode(), loadTargetReg, restoreBaseRegAddrMemRef, _cursor);
   }
   // Repeat load as the object pointer got updated by GC after calling handleReadBarrier helper
   TR::MemoryReference *updateLoadMemRef = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);
   if (load->getOpCodeValue() == TR::InstOpCode::LGG)
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LG, load->getNode(), loadTargetReg, updateLoadMemRef,_cursor);
      }
   else
      {
      _cursor = generateRXInstruction(_cg, TR::InstOpCode::LLGF, load->getNode(), loadTargetReg, updateLoadMemRef, _cursor);
      _cursor = generateRSInstruction(_cg, TR::InstOpCode::SLLG, load->getNode(), loadTargetReg, loadTargetReg, TR::Compiler->om.compressedReferenceShift(), _cursor);
      }

   _cursor = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, load->getNode(), concurrentScavangeNotActiveLabel, _cursor);

   _cg->deleteInst(load);

   return true;
   }

///////////////////////////////////////////////////////////////////////////////
bool
TR_S390Peephole::ICMReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable ICMReduction on 0x%p.\n",_cursor))
         return false;
      }

   // Look for L/LTR instruction pair and reduce to LTR
   TR::Instruction* prev = _cursor->getPrev();
   TR::Instruction* next = _cursor->getNext();
   if (next->getOpCodeValue() != TR::InstOpCode::LTR && next->getOpCodeValue() != TR::InstOpCode::CHI)
      return false;

   bool performed = false;
   bool isICMOpportunity = false;

   TR::S390RXInstruction* load= (TR::S390RXInstruction*) _cursor;

   TR::MemoryReference* mem = load->getMemoryReference();

   if (mem == NULL) return false;
   // We cannot reduce L to ICM if the L uses both index and base registers.
   if (mem->getBaseRegister() != NULL && mem->getIndexRegister() != NULL) return false;

   // L and LTR/CHI instructions must work on the same register.
   if (load->getRegisterOperand(1) != next->getRegisterOperand(1)) return false;

   if (next->getOpCodeValue() == TR::InstOpCode::LTR)
      {
      // We must check for sourceRegister on the LTR instruction, in case there was a dead load
      //        L      GPRX, MEM       <-- this is a dead load, for whatever reason (e.g. GRA)
      //        LTR    GPRX, GPRY
      // it is wrong to transform the above sequence into
      //        ICM    GPRX, MEM
      if (next->getRegisterOperand(1) != ((TR::S390RRInstruction*)next)->getRegisterOperand(2)) return false;
      isICMOpportunity = true;
      }
   else
      {
      // CHI Opportunty:
      //      L  GPRX,..
      //      CHI  GPRX, '0'
      // reduce to:
      //      ICM GPRX,...
      TR::S390RIInstruction* chi = (TR::S390RIInstruction*) next;

      // CHI must be comparing against '0' (i.e. NULLCHK) or else our
      // condition codes will be wrong.
      if (chi->getSourceImmediate() != 0) return false;

      isICMOpportunity = true;
      }

   if (isICMOpportunity && performTransformation(comp(), "\nO^O S390 PEEPHOLE: L being reduced to ICM at [%s].\n", _cursor->getName(comp()->getDebug())))
         {
         if (comp()->getOption(TR_TraceCG))
            {
            printInfo("\n\n *** L reduced to ICM.\n\n Old Instructions:");
            printInst();
            _cursor = _cursor->getNext();
            printInst();
            }

      // Prevent reuse of memory reference
      TR::MemoryReference* memcp = generateS390MemoryReference(*load->getMemoryReference(), 0, _cg);

      if ((memcp->getBaseRegister() == NULL) &&
          (memcp->getIndexRegister() != NULL))
         {
         memcp->setBaseRegister(memcp->getIndexRegister(), _cg);
         memcp->setIndexRegister(0);
         }

      // Do the reduction - create the icm instruction
      TR::S390RSInstruction* icm = new (_cg->trHeapMemory()) TR::S390RSInstruction(TR::InstOpCode::ICM, load->getNode(), load->getRegisterOperand(1), 0xF, memcp, prev, _cg);
      _cursor = icm;

      // Check if the load has an implicit NULLCHK.  If so, we need to ensure a GCmap is copied.
      if (load->throwsImplicitNullPointerException())
         {
         icm->setNeedsGCMap(0x0000FFFF);
         icm->setThrowsImplicitNullPointerException();
         icm->setGCMap(load->getGCMap());

         TR_Debug * debugObj = _cg->getDebug();
         if (debugObj)
            debugObj->addInstructionComment(icm, "Throws Implicit Null Pointer Exception");
         }

      _cg->replaceInst(load, icm);
      _cg->deleteInst(next);

      if (comp()->getOption(TR_TraceCG))
         {
         printInfo("\n\n New Instruction:");
         printInst();
         printInfo("\n ***\n");
         }

      memcp->stopUsingMemRefRegister(_cg);
      performed = true;
      }

   return performed;
   }

bool
TR_S390Peephole::LAReduction()
   {
   TR::Instruction *s390Instr = _cursor;
   bool performed = false;
   if (s390Instr->getKind() == TR::Instruction::IsRX)
      {
      TR::Register *laTargetReg = s390Instr->getRegisterOperand(1);
      TR::MemoryReference *mr = s390Instr->getMemoryReference();
      TR::Register *laBaseReg = NULL;
      TR::Register *laIndexReg = NULL;
      TR::SymbolReference *symRef = NULL;
      if (mr)
         {
         laBaseReg = mr->getBaseRegister();
         laIndexReg = mr->getIndexRegister();
         symRef = mr->getSymbolReference();
         }

      if (mr &&
          mr->isBucketBaseRegMemRef() &&
          mr->getOffset() == 0 &&
          laBaseReg && laTargetReg &&
          laBaseReg == laTargetReg &&
          laIndexReg == NULL &&
          (symRef == NULL || symRef->getOffset() == 0) &&
          (symRef == NULL || symRef->getSymbol() == NULL))
         {
         // remove LA Rx,0(Rx)
         if (comp()->getOption(TR_TraceCG))
            { printInfo("\n"); }
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: Removing redundant LA (0x%p) with matching target and base registers (%s)\n", _cursor,comp()->getDebug()->getName(laTargetReg)))
            {
            if (comp()->getOption(TR_TraceCG))
               printInstr(comp(), _cursor);
            _cg->deleteInst(_cursor);
            performed = true;
            }
         }
      }
   return performed;
   }

bool
TR_S390Peephole::duplicateNILHReduction()
   {
   if (_cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILH)
      {
      TR::Instruction * na = _cursor;
      TR::Instruction * nb = _cursor->getNext();

      // want to delete nb if na == nb - use deleteInst

      if ((na->getKind() == TR::Instruction::IsRI) &&
          (nb->getKind() == TR::Instruction::IsRI))
         {
         // NILH == generateRIInstruction
         TR::S390RIInstruction * cast_na = (TR::S390RIInstruction *)na;
         TR::S390RIInstruction * cast_nb = (TR::S390RIInstruction *)nb;

         if (cast_na->isImm() == cast_nb->isImm())
            {
            if (cast_na->isImm())
               {
               if (cast_na->getSourceImmediate() == cast_nb->getSourceImmediate())
                  {
                  if (cast_na->matchesTargetRegister(cast_nb->getRegisterOperand(1)) &&
                      cast_nb->matchesTargetRegister(cast_na->getRegisterOperand(1)))
                     {
                     if (performTransformation(comp(), "O^O S390 PEEPHOLE: deleting duplicate NILH from pair %p %p*\n", _cursor, _cursor->getNext()))
                        {
                        _cg->deleteInst(_cursor->getNext());
                        return true;
                        }
                     }
                  }
               }
            }
         }
      }
   return false;
   }


bool
TR_S390Peephole::clearsHighBitOfAddressInReg(TR::Instruction *inst, TR::Register *targetReg)
   {
   if (inst->defsRegister(targetReg))
      {
      if (inst->getOpCodeValue() == TR::InstOpCode::LA)
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "LA inst 0x%x clears high bit on targetReg 0x%x (%s)\n", inst, targetReg, targetReg->getRegisterName(comp()));
         return true;
         }

      if (inst->getOpCodeValue() == TR::InstOpCode::LAY)
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "LAY inst 0x%x clears high bit on targetReg 0x%x (%s)\n", inst, targetReg, targetReg->getRegisterName(comp()));
         return true;
         }

      if (inst->getOpCodeValue() == TR::InstOpCode::NILH)
         {
         TR::S390RIInstruction * NILH_RI = (TR::S390RIInstruction *)inst;
         if (NILH_RI->isImm() &&
             NILH_RI->getSourceImmediate() == 0x7FFF)
            {
            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "NILH inst 0x%x clears high bit on targetReg 0x%x (%s)\n", inst, targetReg, targetReg->getRegisterName(comp()));
            return true;
            }
         }
      }
   return false;
   }

bool
TR_S390Peephole::unnecessaryNILHReduction()
   {
   return false;
   }

bool
TR_S390Peephole::NILHReduction()
   {
   bool transformed = false;

   transformed |= duplicateNILHReduction();
   TR_ASSERT(_cursor->getOpCodeValue() == TR::InstOpCode::NILH, "Expecting duplicateNILHReduction to leave a NILH as current instruction\n");
   transformed |= unnecessaryNILHReduction();

   return transformed;
   }


static TR::Instruction *
findActiveCCInst(TR::Instruction *curr, TR::InstOpCode::Mnemonic op, TR::Register *ccReg)
   {
   TR::Instruction * next = curr;
   while (next = next->getNext())
      {
      if (next->getOpCodeValue() == op)
         return next;
      if (next->usesRegister(ccReg) ||
          next->isLabel() ||
          next->getOpCode().setsCC() ||
          next->isCall() ||
          next->getOpCode().setsCompareFlag())
         break;
      }
   return NULL;
   }

bool
TR_S390Peephole::branchReduction()
   {
   return false;
   }

/**
 * Peek ahead in instruction stream to see if we find register being used
 * in a memref
 */
bool
TR_S390Peephole::seekRegInFutureMemRef(int32_t maxWindowSize, TR::Register *targetReg)
   {
   TR::Instruction * current = _cursor->getNext();
   int32_t windowSize=0;


   while ((current != NULL) &&
         !current->matchesTargetRegister(targetReg) &&
         !isBarrierToPeepHoleLookback(current) &&
         windowSize<maxWindowSize)
      {
      // does instruction load or store? otherwise just ignore and move to next instruction
      if (current->isLoad() || current->isStore())
         {
         TR::MemoryReference *mr = current->getMemoryReference();

         if (mr && (mr->getBaseRegister()==targetReg || mr->getIndexRegister()==targetReg))
            {
            return true;
            }
         }
      current = current->getNext();
      windowSize++;
      }

   return false;
   }

/**
 * Removes CGIT/CIT nullchk when CLT can implicitly do nullchk
 * in SignalHandler.c
 */
bool
TR_S390Peephole::removeMergedNullCHK()
   {
      if (TR::Compiler->target.isZOS())
        {
        // CLT cannot do the job in zOS because in zOS it is legal to read low memory address (like 0x000000, literally NULL),
        // and CLT will read the low memory address legally (in this case NULL) to compare it with the other operand.
        // The result will not make sense since we should trap for NULLCHK first.
        return false;
        }

   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable removeMergedNullCHK on 0x%p.\n",_cursor))
         return false;
      }


   int32_t windowSize=0;
   const int32_t maxWindowSize=8;
   static char *disableRemoveMergedNullCHK = feGetEnv("TR_DisableRemoveMergedNullCHK");

   if (disableRemoveMergedNullCHK != NULL) return false;

   TR::Instruction * cgitInst = _cursor;
   TR::Instruction * current = cgitInst->getNext();

   cgitInst->setUseDefRegisters(false);
   TR::Register * cgitSource = cgitInst->getSourceRegister(0);

   while ((current != NULL) &&
         !isBarrierToPeepHoleLookback(current) &&
         windowSize<maxWindowSize)
      {
      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         return false;
         }

      if (current->getOpCodeValue() == TR::InstOpCode::CLT)
          {
          TR::Instruction * cltInst = current;
          cltInst->setUseDefRegisters(false);
          TR::Register * cltSource = cltInst->getSourceRegister(0);
          TR::Register * cltSource2 = cltInst->getSourceRegister(1);
          if (!cgitSource || !cltSource || !cltSource2)
             return false;
          if (toRealRegister(cltSource2)->getRegisterNumber() == toRealRegister(cgitSource)->getRegisterNumber())
             {
                if (performTransformation(comp(), "\nO^O S390 PEEPHOLE: removeMergedNullCHK on 0x%p.\n", cgitInst))
                   {
                   printInfo("\nremoving: ");
                   printInstr(comp(), cgitInst);
                   printInfo("\n");
                   }

                _cg->deleteInst(cgitInst);

                return true;
             }
          }
       else if (current->usesRegister(cgitSource))
          {
          return false;
          }

      current = current->getNext() == NULL ? NULL : current->getNext();
      windowSize++;
      }

   return false;
   }

/**
 * LRReduction performs several LR reduction/removal transformations:
 *
 * (design 1980)
 * convert
 *      LTR GPRx, GPRx
 * to
 *      CHI GPRx, 0
 * This is an AGI reduction as LTR defines GPRx once again, while CHI simply sets the condition code
 *
 *
 *  removes unnecessary LR/LGR/LTR/LGTR's of the form
 *      LR  GPRx, GPRy
 *      LR  GPRy, GPRx   <--- Removed.
 *      CHI GPRx, 0
 * Most of the  redundant LR's are independently generated by global and
 * local register assignment.
 *
 * There is a further extension to this peephole which can transform
 *      LR  GPRx, GPRy
 *      LTR GPRx, GPRx
 * to
 *      LTR GPRx, GPRy
 * assuming that condition code is not incorrectly clobbered between the
 * LR and LTR.  However, there are very few opportunities to exercise this
 * peephole, so is not included.
 *
 * Convert
 *       LR GPRx, GPRy
 *       CHI GPRx, 0
 * to
 *       LTR GPRx, GPRy
 */
bool
TR_S390Peephole::LRReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable LRReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed = false;
   int32_t windowSize=0;
   const int32_t maxWindowSize=20;
   static char *disableLRReduction = feGetEnv("TR_DisableLRReduction");

   if (disableLRReduction != NULL) return false;

   //The _defRegs in the instruction records virtual def reg till now that needs to be reset to real reg.
   _cursor->setUseDefRegisters(false);

   TR::Register *lgrSourceReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(2);
   TR::Register *lgrTargetReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(1);
   TR::InstOpCode lgrOpCode = _cursor->getOpCode();

   if (lgrTargetReg == lgrSourceReg &&
      (lgrOpCode.getOpCodeValue() == TR::InstOpCode::LR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LGR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::LDR ||
       lgrOpCode.getOpCodeValue() == TR::InstOpCode::CPYA))
       {
       if (performTransformation(comp(), "O^O S390 PEEPHOLE: Removing redundant LR/LGR/LDR/CPYA at %p\n", _cursor))
          {
            // Removing redundant LR.
          _cg->deleteInst(_cursor);
          performed = true;
          return performed;
          }
       }

   // If both target and source are the same, and we have a load and test,
   // convert it to a CHI
   if  (lgrTargetReg == lgrSourceReg &&
      (lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTR || lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTGR))
      {
      bool isAGI = seekRegInFutureMemRef(4, lgrTargetReg);

      if (isAGI && performTransformation(comp(), "\nO^O S390 PEEPHOLE: Transforming load and test to compare halfword immediate at %p\n", _cursor))
         {
         // replace LTGR with CGHI, LTR with CHI
         TR::Instruction* oldCursor = _cursor;
         _cursor = generateRIInstruction(_cg, lgrOpCode.is64bit() ? TR::InstOpCode::CGHI : TR::InstOpCode::CHI, comp()->getStartTree()->getNode(), lgrTargetReg, 0, _cursor->getPrev());

         _cg->replaceInst(oldCursor, _cursor);

         performed = true;

         // instruction is now a CHI, not a LTR, so we must return
         return performed;
         }

      TR::Instruction *prev = _cursor->getPrev();
      if((prev->getOpCodeValue() == TR::InstOpCode::LR && lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTR) ||
         (prev->getOpCodeValue() == TR::InstOpCode::LGR && lgrOpCode.getOpCodeValue() == TR::InstOpCode::LTGR))
        {
        TR::Register *prevTargetReg = ((TR::S390RRInstruction*)prev)->getRegisterOperand(1);
        TR::Register *prevSourceReg = ((TR::S390RRInstruction*)prev)->getRegisterOperand(2);
        if((lgrTargetReg == prevTargetReg || lgrTargetReg == prevSourceReg) &&
           performTransformation(comp(), "\nO^O S390 PEEPHOLE: Transforming load register into load and test register and removing current at %p\n", _cursor))
          {
          TR::Instruction *newInst = generateRRInstruction(_cg, lgrOpCode.is64bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, prev->getNode(), prevTargetReg, prevSourceReg, prev->getPrev());
          _cg->replaceInst(prev, newInst);
          if (comp()->getOption(TR_TraceCG))
            printInstr(comp(), _cursor);
          _cg->deleteInst(prev);
          _cg->deleteInst(_cursor);
          _cursor = newInst;
          return true;
          }
        }
      TR::Instruction *next = _cursor->getNext();
      //try to remove redundant LTR, LTGR when we can reuse the condition code of an arithmetic logical operation, ie. Add/Subtract Logical
      //this is also done by isActiveLogicalCC, and the end of generateS390CompareAndBranchOpsHelper when the virtual registers match
      //but those cannot handle the case when the virtual registers are not the same but we do have the same restricted register
      //which is why we are handling it here when all the register assignments are done, and the redundant LR's from the
      //clobber evaluate of the add/sub logical are cleaned up as well.
      // removes the redundant LTR/LTRG, and corrects the mask of the BRC
      // from:
      // SLR @01, @04
      // LTR @01, @01
      // BRC (MASK8, 0x8) Label
      //
      // to:
      // SLR @01, @04
      // BRC (0x10) Label
      //checks that the prev instruction is an add/sub logical operation that sets the same target register as the LTR/LTGR insn, and that we branch immediately after
      if (prev->getOpCode().setsCC() && prev->getOpCode().setsCarryFlag() && prev->getRegisterOperand(1) == lgrTargetReg && next->getOpCodeValue() == TR::InstOpCode::BRC)
         {
         TR::InstOpCode::S390BranchCondition branchCond = ((TR::S390BranchInstruction *) next)->getBranchCondition();

         if ((branchCond == TR::InstOpCode::COND_BE || branchCond == TR::InstOpCode::COND_BNE) &&
            performTransformation(comp(), "\nO^O S390 PEEPHOLE: Removing redundant Load and Test instruction at %p, because CC can be reused from logical instruction %p\n",_cursor, prev))
            {
            _cg->deleteInst(_cursor);
            if (branchCond == TR::InstOpCode::COND_BE)
               ((TR::S390BranchInstruction *) next)->setBranchCondition(TR::InstOpCode::COND_MASK10);
            else if (branchCond == TR::InstOpCode::COND_BNE)
               ((TR::S390BranchInstruction *) next)->setBranchCondition(TR::InstOpCode::COND_MASK5);
            performed = true;
            return performed;
            }
         }
      }

   TR::Instruction * current = _cursor->getNext();

   // In order to remove LTR's, we need to ensure that there are no
   // instructions that set CC or read CC.
   bool lgrSetCC = lgrOpCode.setsCC();
   bool setCC = false, useCC = false;

   while ((current != NULL) &&
            !isBarrierToPeepHoleLookback(current) &&
           !(current->isBranchOp() && current->getKind() == TR::Instruction::IsRIL &&
              ((TR::S390RILInstruction *)  current)->getTargetSnippet() ) &&
           windowSize < maxWindowSize)
      {

      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         return false;
         }

      TR::InstOpCode curOpCode = current->getOpCode();
      current->setUseDefRegisters(false);
      // if we encounter the CHI GPRx, 0, attempt the transformation the LR->LTR
      // and remove the CHI GPRx, 0
      if ((curOpCode.getOpCodeValue() == TR::InstOpCode::CHI || curOpCode.getOpCodeValue() == TR::InstOpCode::CGHI) &&
            ((curOpCode.is32bit() && lgrOpCode.is32bit()) ||
             (curOpCode.is64bit() && lgrOpCode.is64bit())))
         {
         TR::Register *curTargetReg=((TR::S390RIInstruction*)current)->getRegisterOperand(1);
         int32_t srcImm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
         if(curTargetReg == lgrTargetReg && (srcImm == 0) && !(setCC || useCC))
            {
            if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
            if(performTransformation(comp(), "O^O S390 PEEPHOLE: Transforming LR/CHI to LTR at %p\n", _cursor))
               {
               if (comp()->getOption(TR_TraceCG))
                  {
                  printInfo("\nRemoving CHI instruction:");
                  printInstr(comp(), current);
                  char tmp[50];
                  sprintf(tmp,"\nReplacing load at %p with load and test", _cursor);
                  printInfo(tmp);
                  }

               // Remove the CHI
               _cg->deleteInst(current);

               // Replace the LR with LTR
               TR::Instruction* oldCursor = _cursor;
               _cursor = generateRRInstruction(_cg, lgrOpCode.is64bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, comp()->getStartTree()->getNode(), lgrTargetReg, lgrSourceReg, _cursor->getPrev());

               _cg->replaceInst(oldCursor, _cursor);

               lgrOpCode = _cursor->getOpCode();

               lgrSetCC = true;

               performed = true;
               }
            }
         }

      // if we encounter the LR  GPRy, GPRx that we want to remove

      if (curOpCode.getOpCodeValue() == lgrOpCode.getOpCodeValue() &&
          current->getKind() == TR::Instruction::IsRR)
         {
         TR::Register *curSourceReg = ((TR::S390RRInstruction*)current)->getRegisterOperand(2);
         TR::Register *curTargetReg = ((TR::S390RRInstruction*)current)->getRegisterOperand(1);

         if ( ((curSourceReg == lgrTargetReg && curTargetReg == lgrSourceReg) ||
              (curSourceReg == lgrSourceReg && curTargetReg == lgrTargetReg)))
            {
            // We are either replacing LR/LGR (lgrSetCC won't be set)
            // or if we are modifying LTR/LGTR, then no instruction can
            // set or read CC between our original and current instruction.

            if ((!lgrSetCC || !(setCC || useCC)))
               {
               if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
               if (performTransformation(comp(), "O^O S390 PEEPHOLE: Duplicate LR/CPYA removal at %p\n", current))
                  {
                  if (comp()->getOption(TR_TraceCG))
                     {
                     printInfo("\nDuplicate LR/CPYA:");
                     printInstr(comp(), current);
                     char tmp[50];
                     sprintf(tmp,"is removed as duplicate of %p.", _cursor);
                     printInfo(tmp);
                     }

                  // Removing redundant LR/CPYA.
                  _cg->deleteInst(current);

                  performed = true;
                  current = current->getNext();
                  windowSize = 0;
                  setCC = setCC || current->getOpCode().setsCC();
                  useCC = useCC || current->getOpCode().readsCC();
                  continue;
                  }
               }
            }
         }

      // Flag if current instruction sets or reads CC -> used to determine
      // whether LTR/LGTR transformation is valid.
      setCC = setCC || curOpCode.setsCC();
      useCC = useCC || curOpCode.readsCC();

      // If instruction overwrites either of the original source and target registers,
      // we cannot remove any duplicates, as register contents may have changed.
      if (current->isDefRegister(lgrSourceReg) ||
          current->isDefRegister(lgrTargetReg))
         break;

      current = current->getNext();
      windowSize++;
      }

   return performed;
   }

/**
 * Catch the pattern where an LLC/LGFR can be converted
 *    LLC   Rs, Mem
 *    LGFR  Rs, Rs
 * Can be replaced with
 *    LLGC  Rs, Mem
 * Also applies to the pattern where an LLC/LLGTR can be converted
 *    LLC   Rs, Mem
 *    LLGTR  Rs, Rs
 * Can be replaced with
 *    LLGC  Rs, Mem
 */
bool
TR_S390Peephole::LLCReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable LLCReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed = false;

   TR::Instruction *current = _cursor->getNext();
   TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();

   if (curOpCode == TR::InstOpCode::LGFR || curOpCode == TR::InstOpCode::LLGTR)
      {
      TR::Register *llcTgtReg = ((TR::S390RRInstruction *) _cursor)->getRegisterOperand(1);

      TR::Register *curSrcReg = ((TR::S390RRInstruction *) current)->getRegisterOperand(2);
      TR::Register *curTgtReg = ((TR::S390RRInstruction *) current)->getRegisterOperand(1);

      if (llcTgtReg == curSrcReg && llcTgtReg == curTgtReg)
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: Transforming LLC/%s to LLGC at %p.\n", (curOpCode == TR::InstOpCode::LGFR) ? "LGFR" : "LLGTR" ,current))
            {
            if (comp()->getOption(TR_TraceCG))
               {
               if (curOpCode == TR::InstOpCode::LGFR)
                  printInfo("\nRemoving LGFR instruction:");
               else
                  printInfo("\nRemoving LLGTR instruction:");
               printInstr(comp(), current);
               char tmp[50];
               sprintf(tmp,"\nReplacing LLC at %p with LLGC", _cursor);
               printInfo(tmp);
               }

            // Remove the LGFR/LLGTR
            _cg->deleteInst(current);
            // Replace the LLC with LLGC
            TR::Instruction *oldCursor = _cursor;
            TR::MemoryReference* memRef = ((TR::S390RXInstruction *) oldCursor)->getMemoryReference();
            memRef->resetMemRefUsedBefore();
            _cursor = generateRXInstruction(_cg, TR::InstOpCode::LLGC, comp()->getStartTree()->getNode(), llcTgtReg, memRef, _cursor->getPrev());
            _cg->replaceInst(oldCursor, _cursor);
            performed = true;
            }
         }
      }

   return performed;
   }

/**
 *  Catch the pattern where an LGR/LGFR are redundent
 *    LGR   Rt, Rs
 *    LGFR  Rt, Rt
 *  can be replaced by
 *    LGFR  Rt, Rs
 */
bool
TR_S390Peephole::LGFRReduction()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable LGFRReduction on 0x%p.\n",_cursor))
         return false;
      }

   bool performed=false;
   int32_t windowSize=0;
   const int32_t maxWindowSize=2;
   static char *disableLGFRRemoval = feGetEnv("TR_DisableLGFRRemoval");

   if (disableLGFRRemoval != NULL) return false;

   TR::Register *lgrSourceReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(2);
   TR::Register *lgrTargetReg = ((TR::S390RRInstruction*)_cursor)->getRegisterOperand(1);

   // We cannot do anything if both target and source are the same,
   // which can happen with LTR and LTGR
   if  (lgrTargetReg == lgrSourceReg) return performed;

   TR::Instruction * current = _cursor->getNext();
   TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();

   if (curOpCode == TR::InstOpCode::LGFR)
      {
      TR::Register *curSourceReg=((TR::S390RRInstruction*)current)->getRegisterOperand(2);
      TR::Register *curTargetReg=((TR::S390RRInstruction*)current)->getRegisterOperand(1);

      if (curSourceReg == lgrTargetReg && curTargetReg == lgrTargetReg)
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: Redundant LGR at %p.\n", current))
            {
            if (comp()->getOption(TR_TraceCG))
               {
               printInfo("\n\tRedundent LGR/LGFR:");
               printInstr(comp(), current);
               char tmp[50];
               sprintf(tmp, "\t is removed as duplicate of %p.", _cursor);
               printInfo(tmp);
               }

            ((TR::S390RRInstruction*)current)->setRegisterOperand(2,lgrSourceReg);

            // Removing redundant LR.
            _cg->deleteInst(_cursor);

            performed = true;
            _cursor = _cursor->getNext();
            }
         }
      }

   return performed;
   }

/**
 * Catch the pattern where a CRJ/LHI conditional load immediate sequence
 *
 *    CRJ  Rx, Ry, L, M
 *    LHI  Rz, I
 * L: ...  ...
 *
 *  can be replaced by
 *
 *    CR     Rx, Ry
 *    LOCHI  Rz, I, M
 *    ...    ...
 */
bool
TR_S390Peephole::ConditionalBranchReduction(TR::InstOpCode::Mnemonic branchOPReplacement)
   {
   bool disabled = comp()->getOption(TR_DisableZ13) || comp()->getOption(TR_DisableZ13LoadImmediateOnCond);

   // This optimization relies on hardware instructions introduced in z13
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z13) || disabled)
      return false;

   TR::S390RIEInstruction* branchInst = static_cast<TR::S390RIEInstruction*> (_cursor);

   TR::Instruction* currInst = _cursor;
   TR::Instruction* nextInst = _cursor->getNext();

   TR::Instruction* label = branchInst->getBranchDestinationLabel()->getInstruction();

   // Check that the instructions within the fall-through block can be conditionalized
   while (currInst = realInstructionWithLabelsAndRET(nextInst, true))
      {
      if (currInst->getKind() == TR::Instruction::IsLabel)
         {
         if (currInst == label)
            break;
         else
            return false;
         }

      if (currInst->getOpCodeValue() != TR::InstOpCode::LHI && currInst->getOpCodeValue() != TR::InstOpCode::LGHI)
         return false;

      nextInst = currInst->getNext();
      }

   currInst = _cursor;
   nextInst = _cursor->getNext();

   TR::InstOpCode::S390BranchCondition cond = getBranchConditionForMask(0xF - (getMaskForBranchCondition(branchInst->getBranchCondition()) & 0xF));

   if (performTransformation(comp(), "O^O S390 PEEPHOLE: Conditionalizing fall-through block following %p.\n", currInst))
      {
      // Conditionalize the fall-though block
      while (currInst = realInstructionWithLabelsAndRET(nextInst, true))
         {
         if (currInst == label)
            break;

         // Because of the previous checks, LHI or LGHI instruction is guaranteed to be here
         TR::S390RIInstruction* RIInst = static_cast<TR::S390RIInstruction*> (currInst);

         // Conditionalize from "Load Immediate" to "Load Immediate on Condition"
         _cg->replaceInst(RIInst, _cursor = generateRIEInstruction(_cg, RIInst->getOpCode().getOpCodeValue() == TR::InstOpCode::LHI ? TR::InstOpCode::LOCHI : TR::InstOpCode::LOCGHI, RIInst->getNode(), RIInst->getRegisterOperand(1), RIInst->getSourceImmediate(), cond, RIInst->getPrev()));

         currInst = _cursor;
         nextInst = _cursor->getNext();
         }

      // Conditionalize the branch instruction from "Compare and Branch" to "Compare"
      _cg->replaceInst(branchInst, generateRRInstruction(_cg, branchOPReplacement, branchInst->getNode(), branchInst->getRegisterOperand(1), branchInst->getRegisterOperand(2), branchInst->getPrev()));

      return true;
      }

   return false;
   }


/**
 * Catch the pattern where an CR/BRC can be converted
 *    CR R1,R2
 *    BRC  Mask, Lable
 * Can be replaced with
 *    CRJ  R1,R2,Lable
 */
bool
TR_S390Peephole::CompareAndBranchReduction()
   {
   bool performed = false;

   TR::Instruction *current = _cursor;
   TR::Instruction *next = _cursor->getNext();
   TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();
   TR::InstOpCode::Mnemonic nextOpCode = next->getOpCodeValue();
   if ((curOpCode == TR::InstOpCode::CR || curOpCode == TR::InstOpCode::CGR || curOpCode == TR::InstOpCode::CGFR)
      && (nextOpCode == TR::InstOpCode::BRC || nextOpCode == TR::InstOpCode::BRCL))
      {
      printf("Finding CR + BRC\n");
      printf("method=%s\n", comp()->signature());
      }

   return performed;
   }

/**
 * Catch the pattern where a LOAD/NILL load-and-mask sequence
 *
 *    LOAD  Rx, Mem             where LOAD = L     => LZOpCode = LZRF
 *    NILL  Rx, 0xFF00 (-256)         LOAD = LG    => LZOpCode = LZRG
 *                                    LOAD = LLGF  => LZOpCode = LLZRGF
 *  can be replaced by
 *
 *    LZOpCode  Rx, Mem
 */
bool
TR_S390Peephole::LoadAndMaskReduction(TR::InstOpCode::Mnemonic LZOpCode)
   {
   bool disabled = comp()->getOption(TR_DisableZ13) || comp()->getOption(TR_DisableZ13LoadAndMask);

   // This optimization relies on hardware instructions introduced in z13
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z13) || disabled)
      return false;

   if (_cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILL)
      {
      TR::S390RXInstruction* loadInst = static_cast<TR::S390RXInstruction*> (_cursor);
      TR::S390RIInstruction* nillInst = static_cast<TR::S390RIInstruction*> (_cursor->getNext());

      if (!nillInst->isImm() || nillInst->getSourceImmediate() != 0xFF00)
         return false;

      TR::Register* loadTargetReg = loadInst->getRegisterOperand(1);
      TR::Register* nillTargetReg = nillInst->getRegisterOperand(1);

      if (loadTargetReg != nillTargetReg)
         return false;

      if (performTransformation(comp(), "O^O S390 PEEPHOLE: Transforming load-and-mask sequence at %p.\n", nillInst))
         {
         // Remove the NILL instruction
         _cg->deleteInst(nillInst);

         loadInst->getMemoryReference()->resetMemRefUsedBefore();

         // Replace the load instruction with load-and-mask instruction
         _cg->replaceInst(loadInst, _cursor = generateRXYInstruction(_cg, LZOpCode, comp()->getStartTree()->getNode(), loadTargetReg, loadInst->getMemoryReference(), _cursor->getPrev()));

         return true;
         }
      }
   return false;
   }

bool swapOperands(TR::Register * trueReg, TR::Register * compReg, TR::Instruction * curr)
   {
   TR::InstOpCode::S390BranchCondition branchCond;
   uint8_t mask;
   switch (curr->getKind())
      {
      case TR::Instruction::IsRR:
         branchCond = ((TR::S390RRInstruction *) curr)->getBranchCondition();
         ((TR::S390RRInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RRInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRIE:
         branchCond = ((TR::S390RIEInstruction *) curr)->getBranchCondition();
         ((TR::S390RIEInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RIEInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RIEInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRRS:
         branchCond = ((TR::S390RRSInstruction *) curr)->getBranchCondition();
         ((TR::S390RRSInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RRSInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRSInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
         branchCond = ((TR::S390RRFInstruction *) curr)->getBranchCondition();
         ((TR::S390RRFInstruction *) curr)->setBranchCondition(getReverseBranchCondition(branchCond));
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      case TR::Instruction::IsRRF2:
         mask = ((TR::S390RRFInstruction *) curr)->getMask3();
         ((TR::S390RRFInstruction *) curr)->setMask3(getReverseBranchMask(mask));
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(2,trueReg);
         ((TR::S390RRFInstruction *) curr)->setRegisterOperand(1,compReg);
         break;
      default:
         // unsupport instruction type, bail
        return false;
      }
      return true;
   }

void insertLoad(TR::Compilation * comp, TR::CodeGenerator * cg, TR::Instruction * i, TR::Register * r)
   {
   switch(r->getKind())
     {
     case TR_FPR:
       new (comp->trHeapMemory()) TR::S390RRInstruction(TR::InstOpCode::LDR, i->getNode(), r, r, i, cg);
       break;
     default:
       new (comp->trHeapMemory()) TR::S390RRInstruction(TR::InstOpCode::LR, i->getNode(), r, r, i, cg);
       break;
     }
   }

bool hasDefineToRegister(TR::Instruction * curr, TR::Register * reg)
   {
   TR::Instruction * prev = curr->getPrev();
   prev = realInstruction(prev, false);
   for(int32_t i = 0; i < 3 && prev; ++i)
      {
      if (prev->defsRegister(reg))
         {
         return true;
         }

      prev = prev->getPrev();
      prev = realInstruction(prev, false);
      }
   return false;
   }

/**
 * z10 specific hw performance bug
 * On z10, applies to GPRs only.
 * There are cases where load of a GPR and its complemented value are required
 * in same grouping, causing pipeline flush + late load = perf hit.
 */
bool
TR_S390Peephole::trueCompEliminationForCompare()
   {
   // z10 specific
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) || _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }
   TR::Instruction * curr = _cursor;
   TR::Instruction * prev = _cursor->getPrev();
   TR::Instruction * next = _cursor->getNext();
   TR::Register * compReg;
   TR::Register * trueReg;

   prev = realInstruction(prev, false);
   next = realInstruction(next, true);

   switch(curr->getKind())
      {
      case TR::Instruction::IsRR:
         compReg = ((TR::S390RRInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRIE:
         compReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRRS:
         compReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
         compReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(1);
         break;
      default:
         // unsupport instruction type, bail
         return false;
      }

   // only applies to GPR's
   if (compReg->getKind() != TR_GPR || trueReg->getKind() != TR_GPR)
      {
      return false;
      }

   if (!hasDefineToRegister(curr, compReg))
      {
      return false;
      }

   TR::Instruction * branchInst = NULL;
   // current instruction sets condition code or compare flag, check to see
   // if it has multiple branches using this condition code..if so, abort.
   TR::Instruction * nextInst = next;
   while(nextInst && !nextInst->isLabel() && !nextInst->getOpCode().setsCC() &&
         !nextInst->isCall() && !nextInst->getOpCode().setsCompareFlag())
      {
      if(nextInst->isBranchOp())
         {
         if(branchInst == NULL)
            {
            branchInst = nextInst;
            }
         else
            {
            // there are multiple branches using the same branch condition
            // just give up.
            // we can probably just insert load instructions here still, but
            // we have to sort out the wild branches first
            return false;
            }
         }
      nextInst = nextInst->getNext();
      }
   if (branchInst && prev && prev->usesRegister(compReg) && !prev->usesRegister(trueReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 1 at %p.\n",curr))
         {
         swapOperands(trueReg, compReg, curr);
         if(next && next->usesRegister(trueReg)) insertLoad (comp(), _cg, next->getPrev(), compReg);
         TR::InstOpCode::S390BranchCondition branchCond = ((TR::S390BranchInstruction *) branchInst)->getBranchCondition();
         ((TR::S390BranchInstruction *) branchInst)->setBranchCondition(getReverseBranchCondition(branchCond));
         return true;
         }
      else
         return false;
      }
   if (next && next->usesRegister(compReg) && !next->usesRegister(trueReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 2 at %p.\n",curr))
         {
         if (branchInst && prev && !prev->usesRegister(trueReg))
            {
            swapOperands(trueReg, compReg, curr);
            TR::InstOpCode::S390BranchCondition branchCond = ((TR::S390BranchInstruction *) branchInst)->getBranchCondition();
            ((TR::S390BranchInstruction *) branchInst)->setBranchCondition(getReverseBranchCondition(branchCond));
            }
         else
            {
            insertLoad (comp(), _cg, next->getPrev(), compReg);
            }
         return true;
         }
      else
         return false;
      }

   bool loadInserted = false;
   if (prev && prev->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 3 at %p.\n",curr))
         {
         insertLoad (comp(), _cg, prev, trueReg);
         loadInserted = true;
         }
      }
   if (next && next->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare case 4 at %p.\n",curr))
         {
         insertLoad (comp(), _cg, next->getPrev(), trueReg);
         loadInserted = true;
         }
      }
   return loadInserted ;
   }

/**
 * z10 specific hw performance bug
 * On z10, applies to GPRs only.
 * There are cases where load of a GPR and its complemented value are required
 * in same grouping, causing pipeline flush + late load = perf hit.
 */
bool
TR_S390Peephole::trueCompEliminationForCompareAndBranch()
   {
   // z10 specific
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) || _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }

   TR::Instruction * curr = _cursor;
   TR::Instruction * prev = _cursor->getPrev();
   prev = realInstruction(prev, false);

   TR::Instruction * next = _cursor->getNext();
   next = realInstruction(next, true);

   TR::Register * compReg;
   TR::Register * trueReg;
   TR::Instruction * btar = NULL;
   TR::LabelSymbol * ls = NULL;

   bool isWCodeCmpEqSwap = false;

   switch (curr->getKind())
      {
      case TR::Instruction::IsRIE:
         compReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RIEInstruction *) curr)->getRegisterOperand(1);
         btar = ((TR::S390RIEInstruction *) curr)->getBranchDestinationLabel()->getInstruction();
         break;
      case TR::Instruction::IsRRS:
         compReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRSInstruction *) curr)->getRegisterOperand(1);
         break;
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
      case TR::Instruction::IsRRF2:
         compReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(2);
         trueReg = ((TR::S390RRFInstruction *) curr)->getRegisterOperand(1);
         break;
      default:
         // unsupport instruction type, bail
         return false;
      }

   // only applies to GPR's
   if (compReg->getKind() != TR_GPR || trueReg->getKind() != TR_GPR)
      {
      return false;
      }

   if (!hasDefineToRegister(curr, compReg))
      {
      return false;
      }


   btar = realInstruction(btar, true);
   bool backwardBranch = false;
   if (btar)
      {
      backwardBranch = curr->getIndex() - btar->getIndex() > 0;
      }

   if (backwardBranch)
      {
      if ((prev && btar) &&
          (prev->usesRegister(compReg) || btar->usesRegister(compReg)) &&
          (!prev->usesRegister(trueReg) && !btar->usesRegister(trueReg)) )
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 1 at %p.\n",curr))
            {
            swapOperands(trueReg, compReg, curr);
            if (next && next->usesRegister(trueReg)) insertLoad (comp(), _cg, next->getPrev(), compReg);
            return true;
            }
         else
            return false;
         }
      }
   else
      {
      if ((prev && next) &&
          (prev->usesRegister(compReg) || next->usesRegister(compReg)) &&
          (!prev->usesRegister(trueReg) && !next->usesRegister(trueReg)) )
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(!isWCodeCmpEqSwap && performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 2 at %p.\n",curr))
            {
            swapOperands(trueReg, compReg, curr);
            if (btar && btar->usesRegister(trueReg)) insertLoad (comp(), _cg, btar->getPrev(), compReg);
            return true;
            }
         else
            return false;
         }
      }
   if (prev && prev->usesRegister(compReg) && !prev->usesRegister(trueReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(!isWCodeCmpEqSwap && performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 3 at %p.\n",curr))
         {
         swapOperands(trueReg, compReg, curr);
         if (btar && btar->usesRegister(trueReg)) insertLoad (comp(), _cg, btar->getPrev(), compReg);
         if (next && next->usesRegister(trueReg)) insertLoad (comp(), _cg, next->getPrev(), compReg);
         return true;
         }
      else
         return false;
      }

   bool loadInserted = false;
   if (prev && prev->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 4 at %p.\n",curr) )
         {
         insertLoad (comp(), _cg, prev, trueReg);
         loadInserted = true;
         }
      }
   if (btar && btar->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 5 at %p.\n",curr) )
         {
         insertLoad (comp(), _cg, btar->getPrev(), trueReg);
         loadInserted = true;
         }
      }
   if (next && next->usesRegister(compReg))
      {
      if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
      if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for compare and branch case 6 at %p.\n",curr) )
         {
         insertLoad (comp(), _cg, next->getPrev(), trueReg);
         loadInserted = true;
         }
      }
   return loadInserted;
   }

bool
TR_S390Peephole::trueCompEliminationForLoadComp()
   {
   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) || _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }
   TR::Instruction * curr = _cursor;
   TR::Instruction * next = _cursor->getNext();
   next = realInstruction(next, true);
   TR::Instruction * prev = _cursor->getPrev();
   prev = realInstruction(prev, false);

   TR::Register * srcReg = ((TR::S390RRInstruction *) curr)->getRegisterOperand(2);
   TR::RealRegister *tempReg = NULL;
   if((toRealRegister(srcReg))->getRegisterNumber() == TR::RealRegister::GPR1)
      {
      tempReg = _cg->machine()->getS390RealRegister(TR::RealRegister::GPR2);
      }
   else
      {
      tempReg = _cg->machine()->getS390RealRegister(TR::RealRegister::GPR1);
      }

   if (prev && prev->defsRegister(srcReg))
      {
      // src register is defined in the previous instruction, check to see if it's
      // used in the next instruction, if so, inject a load after the current insruction
      if (next && next->usesRegister(srcReg))
         {
         insertLoad (comp(), _cg, curr, tempReg);
         return true;
         }
      }

   TR::Instruction * prev2 = NULL;
   if (prev)
      {
      prev2 = realInstruction(prev->getPrev(), false);
      }
   if (prev2 && prev2->defsRegister(srcReg))
      {
      // src register is defined 2 instructions ago, insert a load before the current instruction
      // if the true value is used before or after
      if ((next && next->usesRegister(srcReg)) || (prev && prev->usesRegister(srcReg)))
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for load complement at %p.\n",curr))
            {
            insertLoad (comp(), _cg, curr->getPrev(), tempReg);
            return true;
            }
         else
            return false;
         }
      }

   TR::Instruction * prev3 = NULL;
   if (prev2)
      {
      prev3 = realInstruction(prev2->getPrev(), false);
      }
   if (prev3 && prev3->defsRegister(srcReg))
      {
      // src registers is defined 3 instructions ago, insert a load before the current instruction
      // if the true value is use before
      if(prev && prev->usesRegister(srcReg))
         {
         if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
         if(performTransformation(comp(), "O^O S390 PEEPHOLE: true complement elimination for load complement at %p.\n",curr))
            {
            insertLoad (comp(), _cg, curr->getPrev(), tempReg);
            return true;
            }
         else
            return false;
         }
      }
   return false;
   }

/**
 * Catch pattern where a DAA instruction is followed by 2 BRCL NOPs; The second one to an outlined label, e.g.
 *
 * CVBG    GPR6, 144(GPR5)
 * BRCL    NOP(0x0), Snippet Label L0059, labelTargetAddr=0x0000000029B8CC2A
 * BRCL    NOP(0x0),         Label L0058, labelTargetAddr=0x0000000029B8C896
 *
 * The second BRCL is generate to create the proper RA snapshot in case we branch to the OOL path.
 * Since Peephole optimizations come after RA, we can safely remove this instruction.
 *
 * Note that for 4-byte instructions such as CVB there is an extra 2-byte NOP used as padding following
 * the 4-byte instruction.
 */
bool
TR_S390Peephole::DAARemoveOutlinedLabelNop(bool hasPadding)
   {
   // Make sure the current instruction is a DAA intrinsic instruction
   if (_cursor->throwsImplicitException())
      {
      TR::Instruction* currInst = _cursor;
      TR::Instruction* nextInst = _cursor->getNext();

      if (hasPadding)
         {
         if (nextInst->getOpCodeValue() == TR::InstOpCode::NOP)
            nextInst = nextInst->getNext();
         else
            {
            TR_ASSERT(0, "S390 PEEPHOLE: DAA instruction padding was not found following instruction %p.\n", nextInst);

            return false;
            }
         }

      TR::Instruction* nextInst2 = nextInst->getNext();

      // Check if we have two NOP BRCs following the DAA intrinsic instruction
      if ((nextInst ->getOpCodeValue() == TR::InstOpCode::BRC || nextInst ->getOpCodeValue() == TR::InstOpCode::BRCL) &&
          (nextInst2->getOpCodeValue() == TR::InstOpCode::BRC || nextInst2->getOpCodeValue() == TR::InstOpCode::BRCL))
         {
         TR::S390RegInstruction* OOLbranchInst = static_cast<TR::S390RegInstruction*> (nextInst2);

         if (OOLbranchInst->getBranchCondition() == TR::InstOpCode::COND_MASK0)
            if (performTransformation(comp(), "O^O S390 PEEPHOLE: Eliminating redundant DAA NOP BRC/BRCL to OOL path %p.\n", OOLbranchInst))
               {
               OOLbranchInst->remove();

               return true;
               }
         }
      }

      return false;
   }

/**
 * Catch pattern where a DAA instruction memory reference generates spill code for long displacement, e.g.
 *
 * STG     GPR1, 148(GPR5)
 * LGHI    GPR1, 74048
 * CVBG    GPR6, 0(GPR1,GPR5)
 * LG      GPR1, 108(GPR5)
 * BRCL    NOP(0x0), Snippet Label L0059, labelTargetAddr=0x0000000029B8CC2A
 * BRCL    NOP(0x0),         Label L0058, labelTargetAddr=0x0000000029B8C896
 *
 * Because the SignalHandler looks for the RestoreGPR7 Snippet address by a hardcoded offset it will
 * instead encounter the memory reference spill code in the place where the RestoreGPR7 Snippet BRCL
 * NOP should have been. This will result in a wild branch.
 *
 * Since this is such a rare occurrence, the workaround is to remove the DAA intrinsic instructions
 * and replace them by an unconditional branch to the OOL path.
 *
 * Note that for 4-byte instructions such as CVB there is an extra 2-byte NOP used as padding following
 * the 4-byte instruction.
 */
bool
TR_S390Peephole::DAAHandleMemoryReferenceSpill(bool hasPadding)
   {
   // Make sure the current instruction is a DAA intrinsic instruction
   if (_cursor->throwsImplicitException())
      {
      TR::Instruction* currInst = _cursor;
      TR::Instruction* nextInst = _cursor->getNext();

      if (hasPadding)
         {
         if (nextInst->getOpCodeValue() == TR::InstOpCode::NOP)
            nextInst = nextInst->getNext();
         else
            {
            TR_ASSERT(0, "S390 PEEPHOLE: DAA instruction padding was not found following instruction %p.\n", nextInst);

            return false;
            }
         }

      if (nextInst->getOpCodeValue() != TR::InstOpCode::BRC && nextInst->getOpCodeValue() != TR::InstOpCode::BRCL)
         {
         if (performTransformation(comp(), "O^O S390 PEEPHOLE: Eliminating DAA intrinsic due to a memory reference spill near instruction %p.\n", currInst))
            {
            currInst = _cursor;
            nextInst = _cursor->getNext();

            TR::S390BranchInstruction* OOLbranchInst;

            // Find the OOL Branch Instruction
            while (currInst != NULL)
               {
               if (currInst->getOpCodeValue() == TR::InstOpCode::BRC || currInst->getOpCodeValue() == TR::InstOpCode::BRCL)
                  {
                  // Convert the instruction to a Branch Instruction to access the Label Symbol
                  OOLbranchInst = static_cast<TR::S390BranchInstruction*> (currInst);

                  // We have found the correct BRC/BRCL
                  if (OOLbranchInst->getLabelSymbol()->isStartOfColdInstructionStream())
                     break;
                  }

               currInst = nextInst;
               nextInst = nextInst->getNext();
               }

            if (currInst == NULL)
               return false;

            currInst = _cursor;
            nextInst = _cursor->getNext();

            // Remove all instructions up until the OOL Branch Instruction
            while (currInst != OOLbranchInst)
               {
               currInst->remove();

               currInst = nextInst;
               nextInst = nextInst->getNext();
               }

            // Sanity check: OOLbranchInst should be a NOP BRC/BRCL
            TR_ASSERT(OOLbranchInst->getBranchCondition() == TR::InstOpCode::COND_MASK0, "S390 PEEPHOLE: DAA BRC/BRCL to the OOL path is NOT a NOP branch %p.\n", OOLbranchInst);

            // Change the NOP BRC to the OOL path to an unconditional branch
            OOLbranchInst->setBranchCondition(TR::InstOpCode::COND_MASK15);

            _cursor = OOLbranchInst;

            return true;
            }
         }
      }

   return false;
   }

/**
 * Catch pattern where Input and Output registers in an int shift are the same
 *    SLLG    GPR6,GPR6,1
 *    SLAK    GPR15,GPR15,#474 0(GPR3)
 *  replace with shorter version
 *    SLL     GPR6,1
 *    SLA     GPR15,#474 0(GPR3)
 *
 * Applies to SLLK, SRLK, SLAK, SRAK, SLLG, SLAG
 * @return true if replacement ocurred
 *         false otherwise
 */
bool
TR_S390Peephole::revertTo32BitShift()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable revertTo32BitShift on 0x%p.\n",_cursor))
         return false;
      }

   TR::S390RSInstruction * instr = (TR::S390RSInstruction *) _cursor;

   // The shift is supposed to be an integer shift when reducing 64bit shifts.
   // Note the NOT in front of second boolean expr. pair
   TR::InstOpCode::Mnemonic oldOpCode = instr->getOpCodeValue();
   if ((oldOpCode == TR::InstOpCode::SLLG || oldOpCode == TR::InstOpCode::SLAG)
         && !(instr->getNode()->getOpCodeValue() == TR::ishl || instr->getNode()->getOpCodeValue() == TR::iushl))
      {
      return false;
      }

   TR::Register* sourceReg = (instr->getSecondRegister())?(instr->getSecondRegister()->getRealRegister()):NULL;
   TR::Register* targetReg = (instr->getRegisterOperand(1))?(instr->getRegisterOperand(1)->getRealRegister()):NULL;

   // Source and target registers must be the same
   if (sourceReg != targetReg)
      {
      return false;
      }

   bool reverted = false;
   if (comp()->getOption(TR_TraceCG)) { printInfo("\n"); }
   if (performTransformation(comp(), "O^O S390 PEEPHOLE: Reverting int shift at %p from SLLG/SLAG/S[LR][LA]K to SLL/SLA/SRL/SRA.\n", instr))
      {
      reverted = true;
      TR::InstOpCode::Mnemonic newOpCode = TR::InstOpCode::BAD;
      switch (oldOpCode)
         {
         case TR::InstOpCode::SLLG:
         case TR::InstOpCode::SLLK:
            newOpCode = TR::InstOpCode::SLL; break;
         case TR::InstOpCode::SLAG:
         case TR::InstOpCode::SLAK:
            newOpCode = TR::InstOpCode::SLA; break;
         case TR::InstOpCode::SRLK:
            newOpCode = TR::InstOpCode::SRL; break;
         case TR::InstOpCode::SRAK:
            newOpCode = TR::InstOpCode::SRA; break;
         default: //PANIC!!!
            TR_ASSERT( 0,"Unexpected OpCode for revertTo32BitShift\n");
            break;
         }

      TR::S390RSInstruction* newInstr = NULL;

      if (instr->getSourceImmediate())
         {
         newInstr = new (_cg->trHeapMemory()) TR::S390RSInstruction(newOpCode, instr->getNode(), instr->getRegisterOperand(1), instr->getSourceImmediate(), instr->getPrev(), _cg);
         }
      else if (instr->getMemoryReference())
         {
         TR::MemoryReference* memRef = instr->getMemoryReference();
         memRef->resetMemRefUsedBefore();
         newInstr = new (_cg->trHeapMemory()) TR::S390RSInstruction(newOpCode, instr->getNode(), instr->getRegisterOperand(1), memRef, instr->getPrev(), _cg);
         }
      else
         {
         TR_ASSERT( 0,"Unexpected RSY format\n");
         }

      _cursor = newInstr;
      _cg->replaceInst(instr, newInstr);

      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "\nTransforming int shift from SLLG/SLAG/S[LR][LA]K (%p) to SLL/SLA/SRL/SRA %p.\n", instr, newInstr);
      }
   return reverted;
   }




/**
 * Try to inline EX dispatched constant instruction snippet
 * into the code section to eliminate i-cache misses and improve
 * runtime performance.
 */
bool
TR_S390Peephole::inlineEXtargetHelper(TR::Instruction *inst, TR::Instruction * instrB4EX)
   {
   if (performTransformation(comp(), "O^O S390 PEEPHOLE: Converting LARL;EX into EXRL instr=[%p]\n", _cursor))
      {
      // fetch the dispatched SS instr
      TR::S390ConstantDataSnippet * cnstDataSnip = _cursor->getMemoryReference()->getConstantDataSnippet();
      TR_ASSERT( (cnstDataSnip->getKind() == TR::Snippet::IsConstantInstruction),"The constant data snippet kind should be ConstantInstruction!\n");
      TR::Instruction * cnstDataInstr = ((TR::S390ConstantInstructionSnippet *)cnstDataSnip)->getInstruction();

      // EX Dispatch would have created a load of the literal pool address, a register which is then killed
      // immediately after its use in the EX instruction. Since we are transforming the EX to an EXRL, this
      // load is no longer needed. We search backwards for this load of the literal pool and remove it.

      TR::Instruction * iterator = instrB4EX;
      int8_t count = 0;
      while (iterator && count < 3)
         {
         if (iterator->getOpCodeValue() != TR::InstOpCode::LARL)
            {
            iterator = realInstructionWithLabels(iterator->getPrev(), false);

            if (iterator == NULL || iterator->isCall() || iterator->isBranchOp())
               break;
            }

         if (iterator->getOpCodeValue() == TR::InstOpCode::LARL)
            {
            TR::S390RILInstruction* LARLInst = reinterpret_cast<TR::S390RILInstruction*> (iterator);

            // Make sure the register we load the literal pool address in is the same register referenced by the EX instruction
            if (LARLInst->isLiteralPoolAddress() && LARLInst->getRegisterOperand(1) == _cursor->getMemoryReference()->getBaseRegister())
               {
               _cg->deleteInst(iterator);
               break;
               }
            }
         else
            {
            // Make sure the register referenced by EX instruction is not be used ?
            TR::Register *baseReg =  _cursor->getMemoryReference()->getBaseRegister();
            if(iterator && iterator->usesRegister(baseReg))
              break;
            }
         ++count;
         }

      // generate new label
      TR::LabelSymbol * ssInstrLabel = TR::LabelSymbol::create(_cg->trHeapMemory(),_cg);
      TR::Instruction * labelInstr = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _cursor->getNode(), ssInstrLabel, inst);

      //cnstDataInstr->move(labelInstr);
      cnstDataInstr->setNext(labelInstr->getNext());
      labelInstr->getNext()->setPrev(cnstDataInstr);
      labelInstr->setNext(cnstDataInstr);
      cnstDataInstr->setPrev(labelInstr);

      // generate EXRL
      TR::S390RILInstruction * newEXRLInst = new (_cg->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::EXRL, _cursor->getNode(), _cursor->getRegisterOperand(1), ssInstrLabel, _cursor->getPrev(), _cg);

      // Replace the EX with EXRL
      TR::Instruction * oldCursor = _cursor;
      _cursor = newEXRLInst;
      _cg->replaceInst(oldCursor, newEXRLInst);

      // remove the instr constant data snippet
      ((TR::S390ConstantInstructionSnippet *)cnstDataSnip)->setIsRefed(false);
      (_cg->getConstantInstructionSnippets()).remove(cnstDataSnip);

      return true;
      }
   return false;
   }


bool
TR_S390Peephole::inlineEXtarget()
   {
   // try to find a label followed an unconditional branch
   TR::Instruction * inst = _cursor;
   int8_t count = 0;
   int8_t maxCount = 30;

   while (inst && count < maxCount)
      {
      inst = realInstructionWithLabels(inst->getPrev(), false);

      if (inst == NULL)
         break;

      // Check to see if we found a label or not
      if (inst->isLabel())
         {
         inst = realInstructionWithLabels(inst->getPrev(), false);

         if (inst == NULL)
            break;

         // check if the label is following an unconditional branch
         if (inst && inst->getOpCode().getOpCodeValue() == TR::InstOpCode::BRC &&
             ((TR::S390BranchInstruction *)inst)->getBranchCondition() == TR::InstOpCode::COND_BRC)
           {
           return inlineEXtargetHelper(inst,_cursor->getPrev());
           }
         }
      else
         {
         }

      ++count;
      }

   if (!comp()->getOption(TR_DisableForcedEXInlining) &&
       performTransformation(comp(), "O^O S390 PEEPHOLE: Inserting unconditional branch to hide EX target instr=[%p]\n",_cursor))
       {
       // Find previous instruction, skipping LARL so we can find it to delete it later.
       TR::Instruction *prev = realInstructionWithLabels(_cursor->getPrev(), false);
       TR::Instruction *instrB4EX = prev;
       if (prev->getOpCode().getOpCodeValue() == TR::InstOpCode::LARL)
          {
          prev = realInstructionWithLabels(prev->getPrev(), false);
          }

       // Generate branch and label
       TR::LabelSymbol *targetLabel = TR::LabelSymbol::create(_cg->trHeapMemory(),_cg);
       TR::Instruction *branchInstr = generateS390BranchInstruction(_cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, _cursor->getNode(), targetLabel, prev);
       TR::Instruction *labelInstr = generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, _cursor->getNode(), targetLabel, branchInstr);
       inlineEXtargetHelper(branchInstr,instrB4EX);
       return true;
       }
   return false;
   }

/**
 * Exploit zGryphon distinct Operands facility:
 * ex:
 * LR      GPR6,GPR0   ; clobber eval
 * AHI     GPR6,-1
 *
 * becomes:
 * AHIK    GPR6,GPR0, -1
 */
bool
TR_S390Peephole::attemptZ7distinctOperants()
   {
   if (comp()->getOption(TR_Randomize))
      {
      if (_cg->randomizer.randomBoolean() && performTransformation(comp(),"O^O Random Codegen  - Disable attemptZ7distinctOperants on 0x%p.\n",_cursor))
         return false;
      }

   bool performed=false;
   int32_t windowSize=0;
   const int32_t maxWindowSize=4;

   TR::Instruction * instr = _cursor;

   if (!_cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return false;
      }

   if (instr->getOpCodeValue() != TR::InstOpCode::LR && instr->getOpCodeValue() != TR::InstOpCode::LGR)
      {
      return false;
      }

   TR::Register *lgrTargetReg = instr->getRegisterOperand(1);
   TR::Register *lgrSourceReg = instr->getRegisterOperand(2);

   TR::Instruction * current = _cursor->getNext();

   while ( (current != NULL) &&
           !current->isLabel() &&
           !current->isCall() &&
           !(current->isBranchOp() && !(current->isExceptBranchOp())) &&
           windowSize < maxWindowSize)
      {
      // do not look across Transactional Regions, the Register save mask is optimistic and does not allow renaming
      if (current->getOpCodeValue() == TR::InstOpCode::TBEGIN ||
          current->getOpCodeValue() == TR::InstOpCode::TBEGINC ||
          current->getOpCodeValue() == TR::InstOpCode::TEND ||
          current->getOpCodeValue() == TR::InstOpCode::TABORT)
         {
         return false;
         }

      TR::InstOpCode::Mnemonic curOpCode = current->getOpCodeValue();

      // found the first next def of lgrSourceReg
      // todo : verify that reg pair is handled
      if (current->defsRegister(lgrSourceReg))
         {
         // we cannot do this if the source register's value is updated:
         // ex:
         // LR      GPR6,GPR0
         // SR      GPR0,GPR11
         // AHI     GPR6,-1
         //
         // this sequence cannot be transformed into:
         // SR      GPR0,GPR11
         // AHIK    GPR6,GPR0, -1
         return false;
         }
      if (!comp()->getOption(TR_DisableHighWordRA) && instr->getOpCodeValue() == TR::InstOpCode::LGR)
         {
         // handles this case:
         // LGR     GPR2,GPR9        ; LR=Clobber_eval
         // LFH     HPR9,#366#SPILL8 Auto[<spill temp 0x84571FDB0>] ?+0(GPR5) ; Load Spill
         // AGHI    GPR2,16
         //
         // cannot transform this into:
         // LFH     HPR9,#366#SPILL8 Auto[<spill temp 0x84571FDB0>] 96(GPR5) ; Load Spill
         // AGHIK   GPR2,GPR9,16,

         TR::RealRegister * lgrSourceHighWordRegister = toRealRegister(lgrSourceReg)->getHighWordRegister();
         if (current->defsRegister(lgrSourceHighWordRegister))
            {
            return false;
            }
         }
      // found the first next use/def of lgrTargetRegister
      if (current->usesRegister(lgrTargetReg))
         {
         if (current->defsRegister(lgrTargetReg))
            {
            TR::Instruction * newInstr = NULL;
            TR::Instruction * prevInstr = current->getPrev();
            TR::Node * node = instr->getNode();
            TR::Register * srcReg = NULL;

            if (curOpCode != TR::InstOpCode::AHI && curOpCode != TR::InstOpCode::AGHI &&
                curOpCode != TR::InstOpCode::SLL && curOpCode != TR::InstOpCode::SLA &&
                curOpCode != TR::InstOpCode::SRA && curOpCode != TR::InstOpCode::SRLK)
               {
               srcReg = current->getRegisterOperand(2);

               // ex:  LR R1, R2
               //      XR R1, R1
               // ==>
               //      XRK R1, R2, R2
               if (srcReg == lgrTargetReg)
                  {
                  srcReg = lgrSourceReg;
                  }
               }

            if (  (current->getOpCode().is32bit() &&  instr->getOpCodeValue() == TR::InstOpCode::LGR)
               || (current->getOpCode().is64bit() &&  instr->getOpCodeValue() == TR::InstOpCode::LR))
               {

               // Make sure we abort if the register copy and the subsequent operation
               //             do not have the same word length (32-bit or 64-bit)
               //
               //  e.g:    LGR R1, R2
               //          SLL R1, 1
               //      ==>
               //          SLLK R1, R2, 1
               //
               //      NOT valid as R1's high word will not be cleared

               return false;
               }

            switch (curOpCode)
               {
               case TR::InstOpCode::AR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ARK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::AGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::AGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::ALR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ALRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::ALGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ALGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::AHI:
                  {
                  int16_t imm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
                  newInstr = generateRIEInstruction(_cg, TR::InstOpCode::AHIK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::AGHI:
                  {
                  int16_t imm = ((TR::S390RIInstruction*)current)->getSourceImmediate();
                  newInstr = generateRIEInstruction(_cg, TR::InstOpCode::AGHIK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::NR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::NRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::NGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::NGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::XR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::XRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::XGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::XGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::OR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::ORK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::OGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::OGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLA:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                    mf->resetMemRefUsedBefore();
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLAK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLAK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLL:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                   mf->resetMemRefUsedBefore();
                   newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLLK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SLLK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }

               case TR::InstOpCode::SRA:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction *)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                    mf->resetMemRefUsedBefore();
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRAK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRAK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SRL:
                  {
                  int16_t imm = ((TR::S390RSInstruction*)current)->getSourceImmediate();
                  TR::MemoryReference * mf = ((TR::S390RSInstruction*)current)->getMemoryReference();
                  if(mf != NULL)
                  {
                    mf->resetMemRefUsedBefore();
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRLK, node, lgrTargetReg, lgrSourceReg, mf, prevInstr);
                  }
                  else
                    newInstr = generateRSInstruction(_cg, TR::InstOpCode::SRLK, node, lgrTargetReg, lgrSourceReg, imm, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SLRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               case TR::InstOpCode::SLGR:
                  {
                  newInstr = generateRRRInstruction(_cg, TR::InstOpCode::SLGRK, node, lgrTargetReg, lgrSourceReg, srcReg, prevInstr);
                  break;
                  }
               default:
                  return false;
               }

            // Merge LR and the current inst into distinct Operants
            _cg->deleteInst(instr);
            _cg->replaceInst(current, newInstr);
            _cursor = instr->getNext();
            performed = true;
            TR::Instruction * s390NewInstr = newInstr;
            }
         return performed;
         }
      windowSize++;
      current = current->getNext();
      }

   return performed;
   }


void
TR_S390Peephole::markBlockThatModifiesRegister(TR::Instruction * cursor,
                                               TR::Register * targetReg)
   {
   // some stores use targetReg as part of source
   if (targetReg && !cursor->isStore() && !cursor->isCompare())
      {
      if (targetReg->getRegisterPair())
         {
         TR::RealRegister * lowReg = toRealRegister(targetReg->getLowOrder());
         TR::RealRegister * highReg = toRealRegister(targetReg->getHighOrder());
         lowReg->setModified(true);
         highReg->setModified(true);
         if (cursor->getOpCodeValue() == TR::InstOpCode::getLoadMultipleOpCode())
            {
            uint8_t numRegs = (lowReg->getRegisterNumber() - highReg->getRegisterNumber())-1;
            if (numRegs > 0)
               {
               for (uint8_t i=highReg->getRegisterNumber()+1; i++; i<= numRegs)
                  {
                  _cg->getS390Linkage()->getS390RealRegister(REGNUM(i))->setModified(true);
                  }
               }
            }
         }
      else
         {
         // some stores use targetReg as part of source
         TR::RealRegister * rReg = toRealRegister(targetReg);
         rReg->setModified(true);
         }
      }
   }

void
TR_S390Peephole::reloadLiteralPoolRegisterForCatchBlock()
   {
   // When dynamic lit pool reg is disabled, we lock R6 as dedicated lit pool reg.
   // This causes a failure when we come back to a catch block because the register context will not be preserved.
   // Hence, we can not assume that R6 will still contain the lit pool register and hence need to reload it.

   bool isZ10 = TR::Compiler->target.cpu.getS390SupportsZ10();

   // we only need to reload literal pool for Java on older z architecture on zos when on demand literal pool is off
   if ( TR::Compiler->target.isZOS() && !isZ10 && !_cg->isLiteralPoolOnDemandOn())
      {
      // check to make sure that we actually need to use the literal pool register
      TR::Snippet * firstSnippet = _cg->getFirstSnippet();
      if ((_cg->getLinkage())->setupLiteralPoolRegister(firstSnippet) > 0)
         {
         // the imm. operand will be patched when the actual address of the literal pool is known at binary encoding phase
         TR::S390RILInstruction * inst = (TR::S390RILInstruction *) generateRILInstruction(_cg, TR::InstOpCode::LARL, _cursor->getNode(), _cg->getLitPoolRealRegister(), reinterpret_cast<void*>(0xBABE), _cursor);
         inst->setIsLiteralPoolAddress();
         }
      }
   }

bool
TR_S390Peephole::DeadStoreToSpillReduction()
  {
  return false;
  }

bool
TR_S390Peephole::tryMoveImmediate()
  {
  return false;
  }

/** \details
 *     This transformation may not always be possible because the LHI instruction does not modify the condition
 *     code while the XR instruction does. We must be pessimistic in our algorithm and carry out the transformation
 *     if and only if there exists an instruction B that sets the condition code between the LHI instruction A and
 *     some instruction C that reads the condition code.
 *
 *     That is, we are trying to find instruction that comes after the LHI in the execution order that will clobber
 *     the condition code before any instruction that consumes a condition code.
 */
bool TR_S390Peephole::ReduceLHIToXR()
  {
  TR::S390RIInstruction* lhiInstruction = static_cast<TR::S390RIInstruction*>(_cursor);

  if (lhiInstruction->getSourceImmediate() == 0)
     {
     TR::Instruction* nextInstruction = lhiInstruction->getNext();

     while (nextInstruction != NULL && !nextInstruction->getOpCode().readsCC())
        {
        if (nextInstruction->getOpCode().setsCC() || nextInstruction->getNode()->getOpCodeValue() == TR::BBEnd)
           {
           TR::DebugCounter::incStaticDebugCounter(_cg->comp(), "z/peephole/LHI/XR");

           TR::Instruction* xrInstruction = generateRRInstruction(_cg, TR::InstOpCode::XR, lhiInstruction->getNode(), lhiInstruction->getRegisterOperand(1), lhiInstruction->getRegisterOperand(1));

           _cg->replaceInst(lhiInstruction, xrInstruction);

           _cursor = xrInstruction;

           return true;
           }

        nextInstruction = nextInstruction->getNext();
        }
     }

  return false;
  }

void
TR_S390Peephole::perform()
   {
   TR::Delimiter d(comp(), comp()->getOption(TR_TraceCG), "peephole");

   if (comp()->getOption(TR_TraceCG))
      printInfo("\nPost Peephole Optimization Instructions:\n");

   bool moveInstr;

   while (_cursor != NULL)
      {
      if (_cursor->getNode()!= NULL && _cursor->getNode()->getOpCodeValue() == TR::BBStart)
         {
         comp()->setCurrentBlock(_cursor->getNode()->getBlock());
         // reload literal pool for catch blocks that need it
         TR::Block * blk = _cursor->getNode()->getBlock();
         if ( blk->isCatchBlock() && (blk->getFirstInstruction() == _cursor) )
            reloadLiteralPoolRegisterForCatchBlock();
         }

      if (_cursor->getOpCodeValue() != TR::InstOpCode::FENCE &&
          _cursor->getOpCodeValue() != TR::InstOpCode::ASSOCREGS &&
          _cursor->getOpCodeValue() != TR::InstOpCode::DEPEND)
         {
         TR::RegisterDependencyConditions * deps =  _cursor->getDependencyConditions();
         bool depCase = (_cursor->isBranchOp() || _cursor->isLabel()) && deps;
         if (depCase)
            {
            _cg->getS390Linkage()->markPreservedRegsInDep(deps);
            }

         //handle all other regs
         TR::Register *reg = _cursor->getRegisterOperand(1);
         markBlockThatModifiesRegister(_cursor, reg);
         }

      // this code is used to handle all compare instruction which sets the compare flag
      // we can eventually extend this to include other instruction which sets the
      // condition code and and uses a complemented register
      if (_cursor->getOpCode().setsCompareFlag() &&
          _cursor->getOpCodeValue() != TR::InstOpCode::CHLR &&
          _cursor->getOpCodeValue() != TR::InstOpCode::CLHLR)
         {
         trueCompEliminationForCompare();
         if (comp()->getOption(TR_TraceCG))
            printInst();
         }

      if (_cursor->isBranchOp())
         forwardBranchTarget();

      moveInstr = true;
      switch (_cursor->getOpCodeValue())
         {
         case TR::InstOpCode::AP:
         case TR::InstOpCode::SP:
         case TR::InstOpCode::MP:
         case TR::InstOpCode::DP:
         case TR::InstOpCode::CP:
         case TR::InstOpCode::SRP:
         case TR::InstOpCode::CVBG:
         case TR::InstOpCode::CVBY:
         case TR::InstOpCode::ZAP:
            if (!DAAHandleMemoryReferenceSpill(false))
               DAARemoveOutlinedLabelNop(false);
            break;

         case TR::InstOpCode::CVB:
            if (!DAAHandleMemoryReferenceSpill(true))
               DAARemoveOutlinedLabelNop(true);
            break;

         case TR::InstOpCode::CPYA:
            {
            LRReduction();
            break;
            }
         case TR::InstOpCode::IPM:
            {
            if (!branchReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;
            }
        case TR::InstOpCode::ST:
            {
            if(DeadStoreToSpillReduction())
              break;
            if(tryMoveImmediate())
              break;
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
        case TR::InstOpCode::STY:
            {
            if(DeadStoreToSpillReduction())
              break;
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
        case TR::InstOpCode::STG:
          if(DeadStoreToSpillReduction())
            break;
          if(tryMoveImmediate())
            break;
          if(comp()->getOption(TR_TraceCG))
            printInst();
          break;
        case TR::InstOpCode::STE:
        case TR::InstOpCode::STEY:
        case TR::InstOpCode::STD:
        case TR::InstOpCode::STDY:
           if (DeadStoreToSpillReduction())
              break;
           if (comp()->getOption(TR_TraceCG))
              printInst();
          break;
         case TR::InstOpCode::LDR:
            {
            LRReduction();
            break;
            }
         case TR::InstOpCode::L:
            {
            if (!ICMReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            LoadAndMaskReduction(TR::InstOpCode::LZRF);
            break;
            }
         case TR::InstOpCode::LA:
            {
            if (!LAReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;
            }

         case TR::InstOpCode::LHI:
            {
            // This optimization is disabled by default because there exist cases in which we cannot determine whether this transformation
            // is functionally valid or not. The issue resides in the various runtime patching sequences using the LHI instruction as a
            // runtime patch point for an offset. One concrete example can be found in the virtual dispatch sequence for unresolved calls
            // on 31-bit platforms where an LHI instruction is used and is patched at runtime.
            //
            // TODO (Issue #255): To enable this optimization we need to implement an API which marks instructions that will be patched at
            // runtime and prevent ourselves from modifying such instructions in any way.
            //
            // ReduceLHIToXR();
            }
            break;

         case TR::InstOpCode::EX:
            {
            static char * disableEXRLDispatch = feGetEnv("TR_DisableEXRLDispatch");

            if (_cursor->isOutOfLineEX() && !comp()->getCurrentBlock()->isCold() && !(bool)disableEXRLDispatch && _cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
               inlineEXtarget();
            break;
            }
         case TR::InstOpCode::LHR:
            {
            break;
            }
         case TR::InstOpCode::LR:
         case TR::InstOpCode::LTR:
            {
            LRReduction();
            AGIReduction();
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }

            if (attemptZ7distinctOperants())
               {
               moveInstr = false;
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }

            break;
            }
         case TR::InstOpCode::LLC:
            {
            LLCReduction();
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }
            break;
            }
         case TR::InstOpCode::LLGF:
            {
            LoadAndMaskReduction(TR::InstOpCode::LLZRGF);
            break;
            }
         case TR::InstOpCode::LG:
            {
            LoadAndMaskReduction(TR::InstOpCode::LZRG);
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
         case TR::InstOpCode::LGR:
         case TR::InstOpCode::LTGR:
            {
            LRReduction();
            LGFRReduction();
            AGIReduction();
            if (comp()->getOption(TR_TraceCG))
               {
               printInst();
               }

            if (attemptZ7distinctOperants())
               {
               moveInstr = false;
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }

            break;
            }
         case TR::InstOpCode::CGIT:
            {
            if (TR::Compiler->target.is64Bit() && !removeMergedNullCHK())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;
            }
         case TR::InstOpCode::CRJ:
            {
            ConditionalBranchReduction(TR::InstOpCode::CR);

            trueCompEliminationForCompareAndBranch();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::CGRJ:
            {
            ConditionalBranchReduction(TR::InstOpCode::CGR);

            trueCompEliminationForCompareAndBranch();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::CRB:
         case TR::InstOpCode::CRT:
         case TR::InstOpCode::CGFR:
         case TR::InstOpCode::CGRT:
         case TR::InstOpCode::CLRB:
         case TR::InstOpCode::CLRJ:
         case TR::InstOpCode::CLRT:
         case TR::InstOpCode::CLGRB:
         case TR::InstOpCode::CLGFR:
         case TR::InstOpCode::CLGRT:
            {
            trueCompEliminationForCompareAndBranch();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::LCGFR:
         case TR::InstOpCode::LCGR:
         case TR::InstOpCode::LCR:
            {
            trueCompEliminationForLoadComp();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         case TR::InstOpCode::SLLG:
         case TR::InstOpCode::SLAG:
         case TR::InstOpCode::SLLK:
         case TR::InstOpCode::SRLK:
         case TR::InstOpCode::SLAK:
         case TR::InstOpCode::SRAK:
            revertTo32BitShift();
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;

         case TR::InstOpCode::NILH:
            if (!NILHReduction())
               {
               if (comp()->getOption(TR_TraceCG))
                  printInst();
               }
            break;

         case TR::InstOpCode::NILF:
            if (_cursor->getNext()->getOpCodeValue() == TR::InstOpCode::NILF)
               {
               TR::Instruction * na = _cursor;
               TR::Instruction * nb = _cursor->getNext();

               // want to delete nb if na == nb - use deleteInst

               if ((na->getKind() == TR::Instruction::IsRIL) &&
                   (nb->getKind() == TR::Instruction::IsRIL))
                  {
                  // NILF == generateRILInstruction
                  TR::S390RILInstruction * cast_na = (TR::S390RILInstruction *)na;
                  TR::S390RILInstruction * cast_nb = (TR::S390RILInstruction *)nb;

                  bool instructionsMatch =
                     (cast_na->getTargetPtr() == cast_nb->getTargetPtr()) &&
                     (cast_na->getTargetSnippet() == cast_nb->getTargetSnippet()) &&
                     (cast_na->getTargetSymbol() == cast_nb->getTargetSymbol()) &&
                     (cast_na->getTargetLabel() == cast_nb->getTargetLabel()) &&
                     (cast_na->getMask() == cast_nb->getMask()) &&
                     (cast_na->getSymbolReference() == cast_nb->getSymbolReference());

                  if (instructionsMatch)
                     {
                     if (cast_na->matchesTargetRegister(cast_nb->getRegisterOperand(1)) &&
                         cast_nb->matchesTargetRegister(cast_na->getRegisterOperand(1)))
                        {
                        if (cast_na->getSourceImmediate() == cast_nb->getSourceImmediate())
                           {
                           if (performTransformation(comp(), "O^O S390 PEEPHOLE: deleting duplicate NILF from pair %p %p*\n", _cursor, _cursor->getNext()))
                              {
                              _cg->deleteInst(_cursor->getNext());
                              }
                           }
                        // To perform
                        //
                        // NILF     @05,X'000000C0'
                        // NILF     @05,X'000000FF'
                        //
                        // ->
                        //
                        // NILF     @05,X'000000C0'
                        //
                        // test if
                        // ((C0 & FF) == C0) && ((FF & C0) != FF)
                        //
                        // want to remove the second unnecessary instruction
                        //
                        else if (((cast_na->getSourceImmediate() & cast_nb->getSourceImmediate()) == cast_na->getSourceImmediate()) &&
                                 ((cast_nb->getSourceImmediate() & cast_na->getSourceImmediate()) != cast_nb->getSourceImmediate()))
                           {
                           if (performTransformation(comp(), "O^O S390 PEEPHOLE: deleting unnecessary NILF from pair %p %p*\n", _cursor, _cursor->getNext()))
                              {
                              _cg->deleteInst(_cursor->getNext());
                              }
                           }
                        }
                     }
                  }
               }
            break;

         case TR::InstOpCode::SRL:
         case TR::InstOpCode::SLL:
            {
            // Turn
            //     SRL      @00_@GN11888_VSWKSKEY,24
            //     SRL      @00_@GN11888_VSWKSKEY,4
            // into
            //     SRL      @00_@GN11888_VSWKSKEY,28
            if (_cursor->getNext()->getOpCodeValue() ==
                _cursor->getOpCodeValue())
               {
               TR::Instruction * na = _cursor;
               TR::Instruction * nb = _cursor->getNext();

               if ((na->getKind() == TR::Instruction::IsRS) &&
                   (nb->getKind() == TR::Instruction::IsRS))
                  {
                  // SRL == SLL == generateRSInstruction
                  TR::S390RSInstruction * cast_na = (TR::S390RSInstruction *)na;
                  TR::S390RSInstruction * cast_nb = (TR::S390RSInstruction *)nb;

                  bool instrMatch =
                        (cast_na->getFirstRegister() == cast_nb->getFirstRegister()) &&
                        (!cast_na->hasMaskImmediate()) && (!cast_nb->hasMaskImmediate()) &&
                        (cast_na->getMemoryReference() == NULL) && (cast_nb->getMemoryReference() == NULL);

                  if (instrMatch)
                     {
                     if (((cast_na->getSourceImmediate() + cast_nb->getSourceImmediate()) < 64) &&
                         performTransformation(comp(), "O^O S390 PEEPHOLE: merging SRL/SLL pair %p %p\n", _cursor, _cursor->getNext()))
                        {
                        // Delete the second instruction
                        uint32_t deletedImmediate = cast_nb->getSourceImmediate();
                        _cg->deleteInst(_cursor->getNext());
                        cast_na->setSourceImmediate(cast_na->getSourceImmediate() + deletedImmediate);
                        }
                     }
                  }
               }
            }
            break;

         case TR::InstOpCode::LGG:
         case TR::InstOpCode::LLGFSG:
            {
            replaceGuardedLoadWithSoftwareReadBarrier();

            if (comp()->getOption(TR_TraceCG))
               printInst();

            break;
            }

         default:
            {
            if (comp()->getOption(TR_TraceCG))
               printInst();
            break;
            }
         }

      if(moveInstr == true)
         _cursor = _cursor->getNext();
      }

   if (comp()->getOption(TR_TraceCG))
      printInfo("\n\n");
   }

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


bool
OMR::Z::CodeGenerator::needsVMThreadDependency()
   {
   if (self()->comp()->getOption(TR_Enable390FreeVMThreadReg))
      return true;
   return false;
   }


void
OMR::Z::CodeGenerator::doPeephole()
   {
   TR_S390Peephole ph(self()->comp(), self());
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
           (*iterator)->getKind()!=TR::Snippet::IsEyeCatcherData ||
           (*iterator)->getKind()!=TR::Snippet::IsTargetAddress  ||
           (*iterator)->getKind()!=TR::Snippet::IsLookupSwitch
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
    if (!_targetList.empty())
      {
      return true;
      }

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
   if (self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      return (!self()->comp()->getOptions()->getOption(TR_DisableUTF16BEEncoder) ||
               (self()->getSupportsVectorRegisters() && !self()->comp()->getOptions()->getOption(TR_DisableSIMDUTF16BEEncoder)));
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
   TR_S390BinaryEncodingData data;
   data.loadArgSize = 0;
   self()->fe()->generateBinaryEncodingPrologue(&data, self());

   TR::Recompilation * recomp = self()->comp()->getRecompilationInfo();
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
                     self()->getS390Linkage()->getS390RealRegister(REGNUM(i))->setModified(true);
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
            if (!0)
               {
               self()->getLinkage()->createEpilogue(temp);
               }

            if (self()->comp()->getOption(TR_EnableLabelTargetNOPs))
               {
               for (TR::Instruction *inst = temp->getNext(); inst != originalNextInstruction; inst = inst->getNext())
                  {
                  TR::Instruction *s390Inst = inst;
                  if (s390Inst->getKind() == TR::Instruction::IsLabel)
                     {
                     if (self()->comp()->getOption(TR_TraceLabelTargetNOPs))
                        traceMsg(self()->comp(),"\t\tepilogue inst %p (%s) setSkipForLabelTargetNOPs\n",s390Inst,self()->comp()->getDebug()->getOpCodeName(&s390Inst->getOpCode()));
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
   if (!(bool)disableEXRLDispatch && self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
      {
      _extentOfLitPool = self()->setEstimatedOffsetForTargetAddressSnippets();
      _extentOfLitPool = self()->setEstimatedOffsetForConstantDataSnippets(_extentOfLitPool);
      }

   self()->setEstimatedCodeLength(data.estimate);

   data.cursorInstruction = self()->getFirstInstruction();

   uint8_t *coldCode = NULL;
   uint8_t *temp = self()->allocateCodeMemory(self()->getEstimatedCodeLength(), 0, &coldCode);


   self()->setBinaryBufferStart(temp);
   self()->setBinaryBufferCursor(temp);


   static char *disableAlignJITEP = feGetEnv("TR_DisableAlignJITEP");

   // Adjust the binary buffer cursor with appropriate padding.
   if (!disableAlignJITEP && !self()->comp()->compileRelocatableCode() && self()->allowSplitWarmAndColdBlocks())
      {
      int32_t alignedBase = 256 - self()->getPreprologueOffset();
      int32_t padBase = ( 256 + alignedBase - ((intptrj_t)temp) % 256) % 256;

      // Threshold determines the maximum number of bytes to align.  If the JIT EP is already close
      // to the beginning of the cache line (i.e. pad bytes is big), then we might not benefit from
      // aligning JIT EP to the true cache boundary.  By default, we align only if padByte exceed 192.
      static char *alignJITEPThreshold = feGetEnv("TR_AlignJITEPThreshold");
      int32_t threshold = (alignJITEPThreshold)?atoi(alignJITEPThreshold):192;

      if (padBase < threshold)
         {
         self()->setBinaryBufferCursor(temp + padBase);
         }
      }


   TR_HashTab * branchHashTable = new (self()->trStackMemory()) TR_HashTab(self()->comp()->trMemory(), stackAlloc, 60, true);
   TR_HashTab * labelHashTable = new (self()->trStackMemory()) TR_HashTab(self()->comp()->trMemory(), stackAlloc, 60, true);
   TR_HashTab * notPrintLabelHashTable = new (self()->trStackMemory()) TR_HashTab(self()->comp()->trMemory(), stackAlloc, 60, true);

   while (data.cursorInstruction)
      {
      uint8_t * const instructionStart = self()->getBinaryBufferCursor();
      if (data.cursorInstruction->isBreakPoint())
         {
         self()->addBreakPointAddress(instructionStart);
         }

      if (self()->comp()->cg()->isBranchInstruction(data.cursorInstruction))
         {
         TR::LabelSymbol * branchLabelSymbol = ((TR::S390BranchInstruction *)data.cursorInstruction)->getLabelSymbol();
         if (data.cursorInstruction->getKind() == TR::Instruction::IsRIE &&
             (toS390RIEInstruction(data.cursorInstruction)->getRieForm() == TR::S390RIEInstruction::RIE_RR ||
              toS390RIEInstruction(data.cursorInstruction)->getRieForm() == TR::S390RIEInstruction::RIE_RI8))
            {
            branchLabelSymbol = toS390RIEInstruction(data.cursorInstruction)->getBranchDestinationLabel();
            }
         if (branchLabelSymbol)
            {
            TR_HashId hashIndex = 0;
            branchHashTable->add((void *)branchLabelSymbol, hashIndex, (void *)branchLabelSymbol);
            }
         }
      else if (data.cursorInstruction->getKind() == TR::Instruction::IsRIL) // e.g. LARL/EXRL
         {
         if (((TR::S390RILInstruction *)data.cursorInstruction)->getTargetLabel() != NULL)
            {
            TR::LabelSymbol * targetLabel = ((TR::S390RILInstruction *)data.cursorInstruction)->getTargetLabel();
            TR_HashId hashIndex = 0;
            branchHashTable->add((void *)targetLabel, hashIndex, (void *)targetLabel);
            }
         }
      else if (self()->comp()->cg()->isLabelInstruction(data.cursorInstruction))
         {
         TR::LabelSymbol * labelSymbol = ((TR::S390BranchInstruction *)data.cursorInstruction)->getLabelSymbol();
         TR_HashId hashIndex = 0;
         labelHashTable->add((void *)labelSymbol, hashIndex, (void *)labelSymbol);
         }

      self()->setBinaryBufferCursor(data.cursorInstruction->generateBinaryEncoding());

      TR_ASSERT(data.cursorInstruction->getEstimatedBinaryLength() >= self()->getBinaryBufferCursor() - instructionStart,
              "\nInstruction length estimate must be conservatively large \n(instr=" POINTER_PRINTF_FORMAT ", opcode=%s, estimate=%d, actual=%d",
              data.cursorInstruction,
              self()->getDebug()? self()->getDebug()->getOpCodeName(&data.cursorInstruction->getOpCode()) : "(unknown)",
              data.cursorInstruction->getEstimatedBinaryLength(),
              self()->getBinaryBufferCursor() - instructionStart);

      self()->addToAtlas(data.cursorInstruction);

      if (data.cursorInstruction == data.preProcInstruction)
         {
         self()->setPrePrologueSize(self()->getBinaryBufferCursor() - self()->getBinaryBufferStart());
         if ((!self()->comp()->getOptions()->getOption(TR_DisableGuardedCountingRecompilations) ||
              self()->comp()->isJProfilingCompilation()) &&
             TR::Options::getCmdLineOptions()->allowRecompilation())
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
            TR_LinkageInfo * linkageInfo = TR_LinkageInfo::get(self()->getCodeStart());
            TR_ASSERT(data.loadArgSize == argSize, "arg size %d != %d\n", data.loadArgSize, argSize);
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


   // Create list of unused labels that should not be printed
   TR_HashTabIterator lit(labelHashTable);
   for (TR::LabelSymbol *labelSymbol = (TR::LabelSymbol *)lit.getFirst();labelSymbol;labelSymbol = (TR::LabelSymbol *)lit.getNext())
      {
      TR_HashId hashIndex = 0;
      if (!(branchHashTable->locate((void *)labelSymbol, hashIndex)))
         notPrintLabelHashTable->add((void *)labelSymbol, hashIndex, (void *)labelSymbol);
      }

   self()->setLabelHashTable(notPrintLabelHashTable);

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

   // Do a pass to tag the basic blocks for Code Mining
   TR_Debug * debugObj = self()->getDebug();
   if (debugObj)
      {
      TR::SimpleRegex * regex =  self()->comp()->getOptions()->getTraceForCodeMining();
      if (regex && TR::SimpleRegex::match(regex, "BBN"))
         {
         data.cursorInstruction = self()->getFirstInstruction();
         int32_t currentBlock = -1;
         int32_t currentBlockFreq = 0;
         while (data.cursorInstruction)
            {
            // Add basic block comment to the current instruction
            // String will be BBN=###, 11 digits should be enough
            char *BB_STRING = (char *)self()->trMemory()->allocateHeapMemory(sizeof(char) * 21);
            TR::Node *node = data.cursorInstruction->getNode();

            if(node && node->getOpCodeValue() == TR::BBStart)
               {
               currentBlock = node->getBlock()->getNumber();
               currentBlockFreq = node->getBlock()->getFrequency();
               }

            // Add it as a comment
            sprintf(BB_STRING, "BBN=%d freq=%d", currentBlock, currentBlockFreq);
            debugObj->addInstructionComment(data.cursorInstruction, BB_STRING);
            data.cursorInstruction = data.cursorInstruction->getNext();
            }
         }
      }
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
   TR::RegisterPair * regPair = NULL;

   if (lowRegister->getKind() == TR_GPR64 && highRegister->getKind() == TR_GPR64)
      {
      regPair = self()->allocate64bitRegisterPair(lowRegister, highRegister);
      }
   else
      {
      regPair = self()->allocateRegisterPair(lowRegister, highRegister);
      }

   // Set associations for providing hint to RA
   regPair->setAssociation(TR::RealRegister::EvenOddPair);

   highRegister->setAssociation(TR::RealRegister::LegalEvenOfPair);
   highRegister->setSiblingRegister(lowRegister);

   lowRegister->setAssociation(TR::RealRegister::LegalOddOfPair);
   lowRegister->setSiblingRegister(highRegister);

   return regPair;
   }

TR::RegisterPair *
OMR::Z::CodeGenerator::allocateArGprPair(TR::Register * lowRegister, TR::Register * highRegister)
   {
   TR::RegisterPair * regPair = NULL;
   TR_ASSERT((highRegister->getKind() == TR_GPR64 || highRegister->getKind() == TR_GPR) &&
           lowRegister->getKind() == TR_AR, "expecting AR<->GPR pair\n");

   if (highRegister->getKind() == TR_GPR64)
      {
      regPair = self()->allocate64bitRegisterPair(lowRegister, highRegister);
      }
   else
      {
      regPair = self()->allocateRegisterPair(lowRegister, highRegister);
      }

   // Set associations for providing hint to RA
   regPair->setAssociation(TR::RealRegister::ArGprPair);

   highRegister->setAssociation(TR::RealRegister::GprOfArGprPair);
   highRegister->setSiblingRegister(lowRegister);
   highRegister->setIsUsedInMemRef();

   lowRegister->setAssociation(TR::RealRegister::ArOfArGprPair);
   lowRegister->setSiblingRegister(highRegister);
   lowRegister->setIsUsedInMemRef();

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

/**
 * RR-type Memory-Memory instructions like MVCL and CLCL
 * need special handling in AR mode if both src and dest pair use the same GPR but different AR's
 * as well as the opposite: same ARs but different GPR's:
 *
 * for example for the MVCL below, we need to create copy of AR_1600 via CPYA and use different ARs for different GPRs
 *
 * MVCL    GPR_1792:AR_1600(GPR_1792:AR_1600),GPR_1760:AR_1600(GPR_1760:AR_1600)
 */
void
OMR::Z::CodeGenerator::splitBaseRegisterPairsForRRMemoryInstructions(TR::Node *node, TR::RegisterPair * sourceReg, TR::RegisterPair * targetReg)
   {
   TR::Register *sourceRegHigh = sourceReg->getHighOrder();
   TR::Register *targetRegHigh = targetReg->getHighOrder();

   if (!sourceRegHigh->isArGprPair() || !targetRegHigh->isArGprPair()) return ;

   if (sourceRegHigh->getGPRofArGprPair() == targetRegHigh->getGPRofArGprPair())
      {
      // same GPR's and same AR's - all good
      if (sourceRegHigh->getARofArGprPair() == targetRegHigh->getARofArGprPair()) return ;

      TR::Register * sourceRegHighGPR = sourceRegHigh->getGPRofArGprPair();
      TR::Register * sourceRegHighAR = sourceRegHigh->getARofArGprPair();

      TR::Register * tempReg = self()->allocateRegister(sourceRegHighGPR->getKind());

      TR::InstOpCode::Mnemonic copyOp = TR::InstOpCode::LR;

      if (sourceRegHighGPR->getKind() == TR_GPR64) copyOp = TR::InstOpCode::LGR;

      generateRRInstruction(self(), copyOp, node, tempReg, sourceRegHighGPR);

      TR::RegisterPair *regpair = self()->allocateArGprPair(sourceRegHighAR, tempReg);
      sourceReg->setHighOrder(regpair, self());
      sourceReg->getHighOrder()->setSiblingRegister(sourceReg->getLowOrder());
      sourceReg->getLowOrder()->setSiblingRegister(sourceReg->getHighOrder());
      self()->stopUsingRegister(sourceRegHigh);
      self()->stopUsingRegister(regpair);
      self()->stopUsingRegister(tempReg);
    }
   else
      {
      // different GPR's and different AR's - all good
      if (sourceRegHigh->getARofArGprPair() != targetRegHigh->getARofArGprPair()) return ;

      TR::Register * sourceRegHighGPR = sourceRegHigh->getGPRofArGprPair();
      TR::Register * sourceRegHighAR = sourceRegHigh->getARofArGprPair();

      TR::Register * tempReg = self()->allocateRegister(sourceRegHighAR->getKind());

      generateRRInstruction(self(), TR::InstOpCode::CPYA, node, tempReg, sourceRegHighAR);

      TR::RegisterPair *regpair = self()->allocateArGprPair(tempReg, sourceRegHighGPR);

      sourceReg->setHighOrder(regpair, self());
      sourceReg->getHighOrder()->setSiblingRegister(sourceReg->getLowOrder());
      sourceReg->getLowOrder()->setSiblingRegister(sourceReg->getHighOrder());
      self()->stopUsingRegister(sourceRegHigh);
      self()->stopUsingRegister(regpair);
      self()->stopUsingRegister(tempReg);
      }
   return ;
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

   if (self()->getS390Linkage()->isCAASpecialArgumentRegister(linkageRegisterIndex))
      {
      result = self()->machine()->getGlobalCAARegisterNumber();
      }
   else if (self()->getS390Linkage()->isEnvironmentSpecialArgumentRegister(linkageRegisterIndex))
      {
      result = self()->machine()->getGlobalEnvironmentRegisterNumber();
      }
   else if (self()->getS390Linkage()->isParentDSASpecialArgumentRegister(linkageRegisterIndex))
      {
      result = self()->machine()->getGlobalParentDSARegisterNumber();
      }
   else if (isFloat)
      {
      result = self()->machine()->getLastLinkageFPR() - linkageRegisterIndex;
      }
   else if (type.isVector())
      {
      result = self()->machine()->getLastGlobalVRFRegisterNumber() - linkageRegisterIndex;
      }

   else
      {
//      traceMsg(self()->comp(), "lastLinkageGPR = %d linkaeRegisterIndex = %d  getGlobalRegisterNumber(getS390Linkage()->getIntegerArgumentRegister(linkageRegisterIndex)-1) = %d\n",machine()->getLastLinkageGPR(),linkageRegisterIndex, self()->getGlobalRegisterNumber(getS390Linkage()->getIntegerArgumentRegister(linkageRegisterIndex)-1));
//      traceMsg(self()->comp(), "getS390Linkage()->getIntegerArgumentRegister(linkageRegisterIndex) = %d\n",getS390Linkage()->getIntegerArgumentRegister(linkageRegisterIndex));

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
   //     Lit pool
   //     extCode Base )
   //   = 11 assignable regs
   //
   int32_t maxGPRs = 0;


   maxGPRs = 11 + (self()->isLiteralPoolOnDemandOn() ? 1 : 0)               // => Litpool ptr is available
                + ((self()->getExtCodeBaseRegisterIsFree()                   &&
                    !self()->comp()->getOption(TR_DisableLongDispStackSlot)     )? 1 : 0)  // => ExtCodeBase is free
                + (self()->needsVMThreadDependency() ? 1 : 0);    //VM Thread Register is available

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

   int32_t numRestrictedRegs = self()->comp()->getOptions()->getNumRestrictedGPRs();
   maxGPRs -= numRestrictedRegs;
   bool longNeeds1Reg = TR::Compiler->target.is64Bit() || self()->use64BitRegsOn32Bit();


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

         if (longNeeds1Reg)
            {
            if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
               {
               return maxGPRs - 1;   // CGHI R,IMM
               }
            else
               {
               return maxGPRs - 2;   // CGR R1,R2
               }
            }
         else // 32bit
            {
            int32_t valueHigh = node->getSecondChild()->getLongIntHigh();
            int32_t valueLow = node->getSecondChild()->getLongIntLow();

            if ((valueHigh >= MIN_IMMEDIATE_VAL && valueHigh <= MAX_IMMEDIATE_VAL) &&
               (valueLow >= MIN_IMMEDIATE_VAL && valueLow <= MAX_IMMEDIATE_VAL))
               {
               return maxGPRs - 2;   // CHI Rh,IMM_H & CLFI Rl,IMM_L
               }
            else
               {
               return maxGPRs - 3;   // Need one extra reg to manufacture IMM
               }
            }
         }
      else
         {
         return longNeeds1Reg ? maxGPRs - 2 : maxGPRs - 5;
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

         if (self()->comp()->cg()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12) && value >= MIN_IMMEDIATE_BYTE_VAL && value <= MAX_IMMEDIATE_BYTE_VAL)
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
   //int32_t numRestrictedRegs = comp()->getOptions()->getNumRestrictedGPRs();
   //maxNumberOfAssignableGPRS -= numRestrictedRegs;
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

   TR::RealRegister * realReg = self()->machine()->getS390RealRegister(realNum);
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


bool OMR::Z::CodeGenerator::isInternalControlFlowReg(TR::Register *reg)
  {
  for(auto cursor = _internalControlFlowRegisters.begin(); cursor != _internalControlFlowRegisters.end(); ++cursor)
     {
     if (reg == *cursor)
        return true;
     }
  return false;
  }

void OMR::Z::CodeGenerator::startInternalControlFlow(TR::Instruction *instr)
  {
  // Visit all the post conditions of instr. All virtual registers in this post condition should remain
  // conservatively alive though out the entire internal control flow.
  TR::RegisterDependencyConditions *conds = instr->getDependencyConditions();

  if (conds) // we may have an empty internal control flow
     {
     TR_S390RegisterDependencyGroup *postConds = conds->getPostConditions();
     OMR::Z::Machine *mach=self()->machine();
     int32_t n = conds->getNumPostConditions();
     int32_t i;
     for (i = 0; i < n; i++)
        {
        TR::Register *r=NULL;
        if(!self()->afterRA())
          r = postConds->getRegisterDependency(i)->getRegister(self());
        else
          {
          TR::RealRegister::RegNum rr = postConds->getRegisterDependency(i)->getRealRegister();
          if(rr>TR::RealRegister::NoReg && rr<=TR::RealRegister::MISC)
            r = mach->getS390RealRegister(rr);
          }
        if(r) _internalControlFlowRegisters.push_back(r);
        }
     }
  // At the outermost internal control flow bottom remember the instruction for any spill insertion
  if (_internalControlFlowNestingDepth == 1)
     _instructionAtEndInternalControlFlow = instr;
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
      targetRegister = self()->allocate64bitRegister();
      }

   if (srcRegister->containsInternalPointer())
      {
      targetRegister->setContainsInternalPointer();
      targetRegister->setPinningArrayPointer(srcRegister->getPinningArrayPointer());
      }
   if (srcRegister->isArGprPair())
      return self()->allocateArGprPair(srcRegister->getARofArGprPair() , targetRegister);
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
      if ((node->getType().isInt64() || TR::Compiler->target.is64Bit() )
         && TR::Compiler->target.is32Bit())
         {
         if (self()->use64BitRegsOn32Bit())
            {
            TR::Register * targetRegister = self()->allocate64bitRegister();

            cursor = generateRRInstruction(self(), TR::InstOpCode::LGR, node, targetRegister, srcRegister);

            return targetRegister;
            }
         else
            {
            TR::RegisterPair * trgRegPair = (TR::RegisterPair *) srcRegister;
            TR::Register * lowRegister = srcRegister->getLowOrder();
            TR::Register * highRegister = srcRegister->getHighOrder();
            bool allocatePair=false;
            if (!data.canClobberLowWord())
               {
               lowRegister = self()->allocateRegister();
               self()->stopUsingRegister(lowRegister); // Allocate pair will make these live again
               allocatePair = true;
               }
            if (!data.canClobberHighWord())
               {
               highRegister = self()->allocateRegister();
               self()->stopUsingRegister(highRegister); // Allocate pair will make these live again
               allocatePair = true;
               }

            TR::RegisterPair * tempRegPair = allocatePair ? self()->allocateConsecutiveRegisterPair(lowRegister, highRegister) : trgRegPair;
            if (!data.canClobberLowWord())
               {
               cursor = generateRRInstruction(self(), TR::InstOpCode::LR, node, tempRegPair->getLowOrder(), trgRegPair->getLowOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }
               }

            if (!data.canClobberHighWord())
               {
               cursor = generateRRInstruction(self(), TR::InstOpCode::LR, node, tempRegPair->getHighOrder(), trgRegPair->getHighOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }
               }
            return tempRegPair;
            }
         }
      else if (node->getOpCode().isFloat())
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
         TR::InstOpCode::Mnemonic loadRegOpCode = TR::InstOpCode::getLoadRegOpCode();
         if (node->getType().isAddress())
            {
            if (TR::Compiler->target.is64Bit())
               loadRegOpCode = TR::InstOpCode::LGR;
            else
               loadRegOpCode = TR::InstOpCode::LR;
            }
         if (self()->supportsHighWordFacility() && !self()->comp()->getOption(TR_DisableHighWordRA) && TR::Compiler->target.is64Bit())
            {
            loadRegOpCode = TR::InstOpCode::getLoadRegOpCodeFromNode(self(), node);
            if (srcRegister->is64BitReg())
               {
               loadRegOpCode = TR::InstOpCode::LGR;
               }
            }
         cursor = generateRRInstruction(self(), loadRegOpCode, node, targetRegister->getGPRofArGprPair(), srcRegister);

         if (debugObj)
            {
            debugObj->addInstructionComment( toS390RRInstruction(cursor), CLOBBER_EVAL);
            }

         return targetRegister;
         }
      }
   return self()->evaluate(node);
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
      if (self()->comp()->cg()->traceBCDCodeGen())
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
                  if (self()->comp()->cg()->traceBCDCodeGen())
                     traceMsg(self()->comp(),"\ty^y : skip update on reg %s node %s (%p), storageRef has already changed -- was #%d (%s), now #%d (%s)\n",
                        self()->getDebug()->getName(listReg),
                        listNode->getOpCode().getName(),listNode,
                        srcStorageReference->getReferenceNumber(),self()->getDebug()->getName(srcStorageReference->getSymbol()),
                        listReg->getStorageReference()->getReferenceNumber(),self()->getDebug()->getName(listReg->getStorageReference()->getSymbol()));
                  }
               else
                  {
                  if (self()->comp()->cg()->traceBCDCodeGen())
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

bool OMR::Z::CodeGenerator::isMemcpyWithPadIfFoldable(TR::Node *node, TR_MemCpyPadTypes type)
   {
   TR::Node *equalNode =node->getChild(TR_MEMCPY_PAD_EQU_LEN_INDEX);

   if (type == OneByte || type == TwoByte) // non-ND cases are always foldable
      {
      return true;
      }
   else if (self()->inlineNDmemcpyWithPad(node) &&
            equalNode->getOpCode().isLoadConst() &&
            equalNode->get64bitIntegralValue() == 1)
      {
      // In the equal case of NDmemcpyWithPad, the evaluation to MVCL can be avoided. Thus CC is not established.
      // Therefore NDmemcpyWithPad is not foldable. Ref: inlineMemcpyWithPadEvaluator.
      return false;
      }
   else if (self()->useMVCLForMemcpyWithPad(node, type)) // smaller (fits in MVCL) ND cases are also foldable
      {
      return true;
      }
   else
      {
      // what remains is large or unknown max length ND cases that require an explicit overlap check in the evaluator
      // that make if folding not possible (leads to reg assigner bugs due to complex control flow and avoiding this
      // ends up needing code on par with the non-folded case anyway)
      return false;
      }
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
      *(int16_t *) cursor |= (int16_t) (((intptrj_t) label_cursor - (((intptrj_t) cursor) - 1)) / 2);
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
      *(int16_t *) cursor = (int16_t) (((intptrj_t) label_cursor - (((intptrj_t) cursor) - addressDifferenceDivisor)) / 2);
      }
   else
      *(int16_t *) cursor = (int16_t) (((intptrj_t) label->getCodeLocation() - (((intptrj_t) cursor) - addressDifferenceDivisor)) / addressDifferenceDivisor);
   }


void
OMR::Z::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   *(int16_t *) cursor = (int16_t) (((intptrj_t) label->getCodeLocation() - ((intptrj_t) cursor - 2)) / 2);
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::apply32BitLabelRelativeRelocation
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   *(int32_t *) (((uint8_t *) cursor) + 2) = (int32_t) (((intptrj_t) (label->getCodeLocation()) - ((intptrj_t) cursor)) / 2);
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::apply32BitLabelTableRelocation
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::apply32BitLabelTableRelocation(int32_t * cursor, TR::LabelSymbol * label)
   {
   *(uint32_t *) cursor = (intptrj_t) label->getCodeLocation() - ((intptrj_t) cursor - *(uint32_t *) cursor);
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

   if ((s = self()->getFirstTargetAddress()) != NULL)
      {
      return s;
      }
   else if ((s = self()->getFirstConstantData()) != NULL)
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
OMR::Z::CodeGenerator::createConstantInstruction(TR::CodeGenerator * cg, TR::Node *node, TR::Instruction * instr)
   {
   TR::S390ConstantInstructionSnippet * cis = new (cg->trHeapMemory()) TR::S390ConstantInstructionSnippet(cg,node,instr);
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

TR::S390LabelTableSnippet *
OMR::Z::CodeGenerator::createLabelTable(TR::Node * node, int32_t size)
   {
   TR::S390LabelTableSnippet * labelTableSnippet = new (self()->trHeapMemory()) TR::S390LabelTableSnippet(self(), node, size);
   _snippetDataList.push_front(labelTableSnippet);
   return labelTableSnippet;
   }


void
OMR::Z::CodeGenerator::addDataConstantSnippet(TR::S390ConstantDataSnippet * snippet)
   {
   _snippetDataList.push_front(snippet);
   }

/**
 * Estimate the offsets for constant snippets from code base
 * the size of the targetAddressSnippet should be passed as input
 * since the constant is emitted after all the target address snippets
 */
int32_t
OMR::Z::CodeGenerator::setEstimatedOffsetForConstantDataSnippets(int32_t targetAddressSnippetSize)
   {
   TR::S390ConstantDataSnippet * cursor;
   bool first;
   int32_t size;
   int32_t offset = targetAddressSnippetSize;
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
   self()->setBinaryBufferCursor((uint8_t *) (((uintptrj_t) (self()->getBinaryBufferCursor() + 7) / 8) * 8));

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
               self()->setBinaryBufferCursor((uint8_t *) (((uintptrj_t) (self()->getBinaryBufferCursor() + size - 1) / size) * size));
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
                  self()->setBinaryBufferCursor((uint8_t *) (((uintptrj_t)(self()->getBinaryBufferCursor() + size -1)/size)*size));
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
               self()->setBinaryBufferCursor((uint8_t *) (((uintptrj_t) (self()->getBinaryBufferCursor() + size - 1) / size) * size));
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
   self()->setBinaryBufferCursor((uint8_t *) (((uintptrj_t) (self()->getBinaryBufferCursor() + 7) / 8) * 8));
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
         if (TR::Compiler->target.isLinux())
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
            if (TR::Compiler->target.is64Bit())
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
            if (TR::Compiler->target.is64Bit())
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
   if (TR::Compiler->target.is64Bit())
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



/**
 * This method sets estimated offset for each targetAddressSnippets
 * from the codebase (points to the 1st snippet)
 * this method is called in generateBinaryEncoding phase where the
 * displacement is required to decide if a index register should be
 * set up for RX instructions
 * @return the total size of the targetAddressSnippets
 *
 * Logic in this method should always be kept in sync with
 * logic in void OMR::Z::CodeGenerator::emitDataSnippets()
 * Constants are emited in order of decreasing size,
 * hense the first emited constant may not be iterator.getFirst(),
 * but the first constant with biggest size
 */
int32_t
OMR::Z::CodeGenerator::setEstimatedOffsetForTargetAddressSnippets()
   {
   int32_t estimatedOffset = 0;

   for (auto iterator = _targetList.begin(); iterator != _targetList.end(); ++iterator)
      {
      (*iterator)->setCodeBaseOffset(estimatedOffset);
      estimatedOffset += (*iterator)->getLength(0);
      }

   return estimatedOffset;
   }


////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::TargetAddressSnippet Functions
////////////////////////////////////////////////////////////////////////////////
int32_t
OMR::Z::CodeGenerator::setEstimatedLocationsForTargetAddressSnippetLabels(int32_t estimatedSnippetStart)
   {
   self()->setEstimatedSnippetStart(estimatedSnippetStart);
   // Conservatively add maximum padding to get to 8 byte alignment.
   estimatedSnippetStart += 6;
   for (auto iterator = _targetList.begin(); iterator != _targetList.end(); ++iterator)
      {
      (*iterator)->setEstimatedCodeLocation(estimatedSnippetStart);
      estimatedSnippetStart += (*iterator)->getLength(estimatedSnippetStart);
      }
   return estimatedSnippetStart;
   }

void
OMR::Z::CodeGenerator::emitTargetAddressSnippets()
   {
   uint8_t * codeOffset;
   int8_t size = 8;

   // We align this stucture on 8bytes to guarantee full predictability of
   // of the lit pool layout.
   //
   self()->setBinaryBufferCursor((uint8_t *) (((uintptrj_t) (self()->getBinaryBufferCursor() + size - 1) / size) * size));

   for (auto iterator = _targetList.begin(); iterator != _targetList.end(); ++iterator)
      {
      codeOffset = (*iterator)->emitSnippetBody();
      if (codeOffset != NULL)
         {
         self()->setBinaryBufferCursor(codeOffset);
         }
      }
   }



TR::S390LookupSwitchSnippet *
OMR::Z::CodeGenerator::CreateLookupSwitchSnippet(TR::Node * node, TR::Snippet * s)
   {
   _targetList.push_front((TR::S390LookupSwitchSnippet *) s);
   return (TR::S390LookupSwitchSnippet *) s;
   }



TR::S390TargetAddressSnippet *
OMR::Z::CodeGenerator::CreateTargetAddressSnippet(TR::Node * node, TR::Snippet * s)
   {
   TR::S390TargetAddressSnippet * targetsnippet;
   targetsnippet = new (self()->trHeapMemory()) TR::S390TargetAddressSnippet(self(), node, s);
   _targetList.push_front(targetsnippet);
   return targetsnippet;
   }

TR::S390TargetAddressSnippet *
OMR::Z::CodeGenerator::CreateTargetAddressSnippet(TR::Node * node, TR::LabelSymbol * s)
   {
   TR::S390TargetAddressSnippet * targetsnippet;
   targetsnippet = new (self()->trHeapMemory()) TR::S390TargetAddressSnippet(self(), node, s);
   _targetList.push_front(targetsnippet);
   return targetsnippet;
   }

TR::S390TargetAddressSnippet *
OMR::Z::CodeGenerator::CreateTargetAddressSnippet(TR::Node * node, TR::Symbol * s)
   {
   TR_ASSERT(self()->supportsOnDemandLiteralPool() == false, "May not be here with Literal Pool On Demand enabled\n");
   TR::S390TargetAddressSnippet * targetsnippet;
   targetsnippet = new (self()->trHeapMemory()) TR::S390TargetAddressSnippet(self(), node, s);
   _targetList.push_front(targetsnippet);
   return targetsnippet;
   }

TR::S390TargetAddressSnippet *
OMR::Z::CodeGenerator::findOrCreateTargetAddressSnippet(TR::Node * node, uintptrj_t addr)
   {
   TR_ASSERT(self()->supportsOnDemandLiteralPool() == false, "May not be here with Literal Pool On Demand enabled\n");
   // A simple linear search should suffice for now since the number of FP constants
   // produced is typically very small.  Eventually, this should be implemented as an
   // ordered list or a hash table.
   //
   for (auto iterator = _targetList.begin(); iterator != _targetList.end(); ++iterator)
      {
      if ((*iterator)->getKind() == TR::Snippet::IsTargetAddress && (*iterator)->getTargetAddress() == addr)
         {
         return *iterator;
         }
      }

   TR::S390TargetAddressSnippet * targetsnippet;

   targetsnippet = new (self()->trHeapMemory()) TR::S390TargetAddressSnippet(self(), node, addr);
   _targetList.push_front(targetsnippet);
   return targetsnippet;
   }

TR::S390TargetAddressSnippet *
OMR::Z::CodeGenerator::getFirstTargetAddress()
   {
   if(_targetList.empty())
      return NULL;
   else
      return _targetList.front();
   }

void
OMR::Z::CodeGenerator::freeAndResetTransientLongs()
   {
   int32_t num_lReg;

   for (num_lReg=0; num_lReg < _transientLongRegisters.size(); num_lReg++)
      self()->stopUsingRegister(_transientLongRegisters[num_lReg]);
   _transientLongRegisters.setSize(0);
   }

// routines for shrinkwrapping
//
bool
OMR::Z::CodeGenerator::processInstruction(TR::Instruction *instr,
                                          TR_BitVector **registerUsageInfo,
                                          int32_t &blockNum,
                                          int32_t &isFence,
                                          bool traceIt)
   {
   TR::Instruction *s390Instr = instr;

   if (s390Instr->getOpCode().isCall())
      {
      TR::Node *callNode = s390Instr->getNode();
      if (callNode->getSymbolReference())
         {
#if 0
         TR::SymbolReference *symRef = callNode->getSymbolReference();
         if (symRef->getSymbol() && symRef->getSymbol()->isMethod())
            {
            if (!symRef->getSymbol()->castToMethodSymbol()->preservesAllRegisters())
               {
               if (traceIt)
                  traceMsg(comp(), "call instr [%p] does not preserve regs\n", s390Instr);
               registerUsageInfo[blockNum]->setAll(TR::RealRegister::NumRegisters);
               }
            else
               {
               if (traceIt)
                 traceMsg(comp(), "call instr [%p] preserves regs\n", s390Instr);
               }
            }
#else
         bool callPreservesRegisters = true;
         TR::SymbolReference *symRef = callNode->getSymbolReference();
         TR::MethodSymbol *m = NULL;
         if (symRef->getSymbol()->isMethod())
            {
            m = symRef->getSymbol()->castToMethodSymbol();
            if ((m->isHelper() && !m->preservesAllRegisters()) ||
                m->isVMInternalNative() ||
                m->isJITInternalNative() ||
                m->isJNI() ||
                m->isSystemLinkageDispatch() || (m->getLinkageConvention() == TR_System))
               callPreservesRegisters = false;
            }

         if (!callPreservesRegisters)
            {
            if (traceIt)
               traceMsg(self()->comp(), "call instr [%p] does not preserve regs\n", s390Instr);
            registerUsageInfo[blockNum]->setAll(TR::RealRegister::NumRegisters);
            }
         else
            {
            if (traceIt)
               traceMsg(self()->comp(), "call instr [%p] preserves regs\n", s390Instr);
            }
#endif
         }
      }

   switch (s390Instr->getKind())
      {
      case TR::Instruction::IsPseudo:
         {
         // process BBStart and BBEnd nodes
         // and populate the RUSE info at each block
         //
         if (s390Instr->getOpCodeValue() == TR::InstOpCode::FENCE)
            {
            TR::Node *fenceNode = s390Instr->getNode();
            if (fenceNode->getOpCodeValue() == TR::BBStart)
               {
               blockNum = fenceNode->getBlock()->getNumber();
               isFence = 1;
               if (traceIt)
                  traceMsg(self()->comp(), "Now generating register use information for block_%d\n", blockNum);
               }
            else if (fenceNode->getOpCodeValue() == TR::BBEnd)
               isFence = 2;
            }
         return false;
         }

      case TR::Instruction::IsBranchOnIndex:
         {
         // get the target and src registers
         //
         int32_t tgtRegNum = ((TR::RealRegister*)s390Instr->getRegisterOperand(1))->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "branchOnIndex instr [%p] USES register [%d]\n", s390Instr, tgtRegNum);
         registerUsageInfo[blockNum]->set(tgtRegNum);

         TR::Register * sourceRegister = s390Instr->getRegisterOperand(2);
         TR::RegisterPair * regPair = sourceRegister->getRegisterPair();
         if (regPair)
            {
            int32_t srcHighNum = toRealRegister(sourceRegister->getHighOrder())->getRegisterNumber();
            int32_t srcLowNum  = toRealRegister(sourceRegister->getLowOrder())->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "branchOnIndex instr [%p] USES register pair [%d, %d]\n", s390Instr, srcHighNum, srcLowNum);
            }
         else
            {
            int32_t srcNum = toRealRegister(sourceRegister)->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "branchOnIndex instr [%p] USES register [%d]\n", s390Instr, srcNum);
            }
         return true;
         }
      case TR::Instruction::IsReg:
         {
         TR::Register * targetRegister = s390Instr->getRegisterOperand(1);
         TR::RegisterPair * regPair = targetRegister->getRegisterPair();
         if (regPair)
            {
            int32_t tgtHighNum = toRealRegister(targetRegister->getHighOrder())->getRegisterNumber();
            int32_t tgtLowNum  = toRealRegister(targetRegister->getLowOrder())->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "reg instr [%p] USES register pair [%d, %d]\n", s390Instr, tgtLowNum, tgtHighNum);
            registerUsageInfo[blockNum]->set(tgtHighNum);
            registerUsageInfo[blockNum]->set(tgtLowNum);
            }
         else
            {
            int32_t tgtRegNum = toRealRegister(targetRegister)->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "reg instr [%p] USES register [%d]\n", s390Instr, tgtRegNum);
            registerUsageInfo[blockNum]->set(tgtRegNum);
            }
         return true;
         }
      case TR::Instruction::IsRR:
         {
         if (((TR::S390RRInstruction *)s390Instr)->getFirstConstant() >= 0)
            ; //nothing to do
         else
            {
            TR::Register * targetRegister = s390Instr->getRegisterOperand(1);
            TR::RegisterPair * regPair = targetRegister->getRegisterPair();
            if (regPair)
               {
               int32_t tgtHighNum = toRealRegister(targetRegister->getHighOrder())->getRegisterNumber();
               int32_t tgtLowNum  = toRealRegister(targetRegister->getLowOrder())->getRegisterNumber();
               if (traceIt)
                  traceMsg(self()->comp(), "RR instr [%p] USES register pair [%d %d]\n", s390Instr, tgtLowNum, tgtHighNum);
               registerUsageInfo[blockNum]->set(tgtHighNum);
               registerUsageInfo[blockNum]->set(tgtLowNum);
               }
            else
               {
               int32_t tgtRegNum = toRealRegister(targetRegister)->getRegisterNumber();
               if (traceIt)
                  traceMsg(self()->comp(), "RR instr [%p] USES register [%d]\n", s390Instr, tgtRegNum);
               registerUsageInfo[blockNum]->set(tgtRegNum);
               }
            }

         if (((TR::S390RRInstruction *)s390Instr)->getSecondConstant() >= 0)
            ; //nothing to do
         else
            {
            TR::Register * sourceRegister = ((TR::S390RRInstruction *)instr)->getRegisterOperand(2);
            TR::RegisterPair *regPair = sourceRegister->getRegisterPair();
            int32_t srcRegNum = 0;
            if (regPair)
               {
               srcRegNum = toRealRegister(sourceRegister->getHighOrder())->getRegisterNumber();
               if (traceIt)
                  traceMsg(self()->comp(), "RR instr [%p] USES register pair [%d]\n", s390Instr, srcRegNum);
               }
            else
               {
               srcRegNum = toRealRegister(sourceRegister)->getRegisterNumber();
               if (traceIt)
                  traceMsg(self()->comp(), "RR instr [%p] USES register [%d]\n", s390Instr, srcRegNum);
               }
            }
         return true;
         }
      case TR::Instruction::IsRRE:
         {
         TR::Register * targetRegister = s390Instr->getRegisterOperand(1);
         TR::RegisterPair * regPair = targetRegister->getRegisterPair();
         if (regPair)
            {
            int32_t tgtHighNum = toRealRegister(targetRegister->getHighOrder())->getRegisterNumber();
            int32_t tgtLowNum  = toRealRegister(targetRegister->getLowOrder())->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "RRE instr [%p] USES register pair [%d %d]\n", s390Instr, tgtLowNum, tgtHighNum);
            registerUsageInfo[blockNum]->set(tgtHighNum);
            registerUsageInfo[blockNum]->set(tgtLowNum);
            }
         else
            {
            int32_t tgtRegNum = toRealRegister(targetRegister)->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "RRE instr [%p] USES register [%d]\n", s390Instr, tgtRegNum);
            registerUsageInfo[blockNum]->set(tgtRegNum);
            }

         int32_t srcRegNum = toRealRegister(s390Instr->getRegisterOperand(2))->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "RRE instr [%p] USES register [%d]\n", s390Instr, srcRegNum);
         return true;
         }
      case TR::Instruction::IsRRD: // RRD is encoded use RRF
      case TR::Instruction::IsRRF:
      case TR::Instruction::IsRRF2:
      case TR::Instruction::IsRRF3:
      case TR::Instruction::IsRRF4:
      case TR::Instruction::IsRRF5:
         {
         TR::S390RRFInstruction *s = (TR::S390RRFInstruction *)s390Instr;
         int32_t tgtRegNum = toRealRegister(s->getRegisterOperand(1))->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "RRF instr [%p] USES register [%d]\n", s, tgtRegNum);
         registerUsageInfo[blockNum]->set(tgtRegNum);

         if (s->isSourceRegister2Present()) //RRF or RRF2
            {
            int32_t srcRegNum = toRealRegister(s->getRegisterOperand(3))->getRegisterNumber();
            if (traceIt)
               traceMsg(self()->comp(), "RRF instr [%p] USES register [%d]\n", s, srcRegNum);
            }

         int32_t srcRegNum = toRealRegister(s->getRegisterOperand(2))->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "RRF instr [%p] USES register [%d]\n", s, srcRegNum);
         return true;
         }
      case TR::Instruction::IsRRR:
         {
         TR::S390RRRInstruction *s = (TR::S390RRRInstruction *)s390Instr;
         int32_t tgtRegNum = toRealRegister(s->getRegisterOperand(1))->getRegisterNumber();
         registerUsageInfo[blockNum]->set(tgtRegNum);

         int32_t srcReg2Num = toRealRegister(s->getRegisterOperand(3))->getRegisterNumber();

         int32_t srcRegNum = toRealRegister(s->getRegisterOperand(2))->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "RRR instr [%p] USES register [%d %d %d]\n", s, srcRegNum, srcReg2Num, tgtRegNum);

         return true;
         }
      case TR::Instruction::IsRI:
      case TR::Instruction::IsRIL:
         {
         if (s390Instr->getRegisterOperand(1))
            {
            int32_t tgtRegNum = toRealRegister(s390Instr->getRegisterOperand(1))->getRegisterNumber();
            registerUsageInfo[blockNum]->set(tgtRegNum);
            if (traceIt)
               traceMsg(self()->comp(), "RIL instr [%p] USES register [%d]\n", s390Instr, tgtRegNum);
            }
         return true;
         }
      case TR::Instruction::IsRS:
      case TR::Instruction::IsRSY:
         {
         TR::S390RSInstruction *s = (TR::S390RSInstruction *)s390Instr;
         if (s->getSourceImmediate())
            {
            if (s->getRegisterOperand(1)->getRegisterPair())
               {
               int32_t tgtHighNum = toRealRegister(s->getRegisterOperand(1)->getHighOrder())->getRegisterNumber();
               int32_t tgtLowNum  = toRealRegister(s->getRegisterOperand(1)->getLowOrder())->getRegisterNumber();
               registerUsageInfo[blockNum]->set(tgtHighNum);
               registerUsageInfo[blockNum]->set(tgtLowNum);
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES register pair [%d %d]\n", s, tgtLowNum, tgtHighNum);
               }
            else
               {
               int32_t tgtRegNum = toRealRegister(s->getRegisterOperand(1))->getRegisterNumber();
               registerUsageInfo[blockNum]->set(tgtRegNum);
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES register [%d]\n", s, tgtRegNum);
               }

            if (!s->isTargetPair() && s->getLastRegister() != NULL)
               {
               int32_t lastRegNum = toRealRegister(s->getLastRegister())->getRegisterNumber();
               registerUsageInfo[blockNum]->set(lastRegNum);
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES register [%d]\n", s, lastRegNum);
               }
            }
         else if (s->getMaskImmediate())
            {
            int32_t firstRegNum = toRealRegister(s->getFirstRegister())->getRegisterNumber();
            registerUsageInfo[blockNum]->set(firstRegNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES first register [%d]\n", s, firstRegNum);
            TR::MemoryReference * mr = s->getMemoryReference();
            if (mr->getBaseRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
               }
            if (mr->getIndexRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr indexRegister [%d]\n", s, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
               }
            }
         else if (s->getOpCode().usesRegPairForTarget() && s->getOpCode().usesRegPairForSource())
            {
            int32_t tgtHighNum = toRealRegister(s->getRegisterOperand(1)->getHighOrder())->getRegisterNumber();
            int32_t tgtLowNum  = toRealRegister(s->getRegisterOperand(1)->getLowOrder())->getRegisterNumber();
            registerUsageInfo[blockNum]->set(tgtHighNum);
            registerUsageInfo[blockNum]->set(tgtLowNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES register pair [%d %d]\n", s, tgtLowNum, tgtHighNum);
            int32_t secondRegNum = toRealRegister(s->getSecondRegister()->getHighOrder())->getRegisterNumber();
            registerUsageInfo[blockNum]->set(secondRegNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES register pair [%d]\n", s, secondRegNum);
            TR::MemoryReference * mr = s->getMemoryReference();
            if (mr->getBaseRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
               }
            if (mr->getIndexRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr indexRegister [%d]\n", s, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
               }
            }
         else if (s->getLastRegister() == NULL || s->getOpCode().usesRegPairForTarget())
            {
            int32_t firstRegNum = toRealRegister(s->getFirstRegister())->getRegisterNumber();
            registerUsageInfo[blockNum]->set(firstRegNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES first register [%d]\n", s, firstRegNum);
            TR::MemoryReference * mr = s->getMemoryReference();
            if (mr->getBaseRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
               }
            if (mr->getIndexRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr indexRegister [%d]\n", s, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
               }
            }
         else
            {
            int32_t firstRegNum = toRealRegister(s->getFirstRegister())->getRegisterNumber();
            registerUsageInfo[blockNum]->set(firstRegNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES first register [%d]\n", s, firstRegNum);
            int32_t lastRegNum = toRealRegister(s->getLastRegister())->getRegisterNumber();
            registerUsageInfo[blockNum]->set(lastRegNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES last register [%d]\n", s, lastRegNum);

            TR::MemoryReference * mr = s->getMemoryReference();
            if (mr->getBaseRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
               }
            if (mr->getIndexRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RS instr [%p] USES mr indexRegister [%d]\n", s, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
               }
            }
         return true;
         }
      case TR::Instruction::IsRSL:
      case TR::Instruction::IsSI:
      case TR::Instruction::IsSIY:
      case TR::Instruction::IsSIL:
      case TR::Instruction::IsS:
         {
         TR::MemoryReference * mr = s390Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "RSL instr [%p] USES mr baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "RSL instr [%p] USES mr indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         return true;
         }
      case TR::Instruction::IsRX:
      case TR::Instruction::IsRXY:
         {
         if (s390Instr->getRegisterOperand(1)->getRegisterPair())
            {
            int32_t tgtHighNum = toRealRegister(s390Instr->getRegisterOperand(1)->getHighOrder())->getRegisterNumber();
            int32_t tgtLowNum  = toRealRegister(s390Instr->getRegisterOperand(1)->getLowOrder())->getRegisterNumber();
            registerUsageInfo[blockNum]->set(tgtHighNum);
            registerUsageInfo[blockNum]->set(tgtLowNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES register pair [%d %d]\n", s390Instr, tgtLowNum, tgtHighNum);
            }
         else
            {
            int32_t tgtRegNum = toRealRegister(s390Instr->getRegisterOperand(1))->getRegisterNumber();
            registerUsageInfo[blockNum]->set(tgtRegNum);
            if (traceIt)
               traceMsg(self()->comp(), "RS instr [%p] USES register [%d]\n", s390Instr, tgtRegNum);
            }

         TR::MemoryReference * mr = s390Instr->getMemoryReference();
         if (mr)
            {
            if (mr->getBaseRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RSL instr [%p] USES mr baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
               }
            if (mr->getIndexRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "RSL instr [%p] USES mr indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
               }
            }

         return true;
         }
      case TR::Instruction::IsRXYb:
         {
         return true;
         }
      case TR::Instruction::IsRXF:
         {
         int32_t tgtRegNum = toRealRegister(s390Instr->getRegisterOperand(1))->getRegisterNumber();
         registerUsageInfo[blockNum]->set(tgtRegNum);

         int32_t srcRegNum = toRealRegister(s390Instr->getRegisterOperand(2))->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "RRR instr [%p] USES register [%d %d]\n", s390Instr, srcRegNum, tgtRegNum);

         TR::MemoryReference * mr = s390Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "RXF instr [%p] USES mr baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "RXF instr [%p] USES mr indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         return true;
         }
      case TR::Instruction::IsMem:
         {
         TR::MemoryReference * mr = s390Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "Mem instr [%p] USES mr baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "Mem instr [%p] USES mr indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         return true;
         }
      case TR::Instruction::IsSS1:
         {
         TR::MemoryReference * mr = s390Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS1 instr [%p] USES mr1 baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS1 instr [%p] USES mr1 indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }

         mr = s390Instr->getMemoryReference2();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS1 instr [%p] USES mr2 baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS1 instr [%p] USES mr2 indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         return true;
         }
      case TR::Instruction::IsSS2:
      case TR::Instruction::IsSSE:
         {
         TR::MemoryReference * mr = s390Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS2 instr [%p] USES mr baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS2 instr [%p] USES mr indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }

         mr = s390Instr->getMemoryReference2();
         if (mr)
            {
            if (mr->getBaseRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "SS2 instr [%p] USES mr2 baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
               }
            if (mr->getIndexRegister())
               {
               if (traceIt)
                  traceMsg(self()->comp(), "SS2 instr [%p] USES mr2 indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
               }
            }
         return true;
         }
      case TR::Instruction::IsSS4:
         {
         TR::S390SS4Instruction *s = (TR::S390SS4Instruction *)s390Instr;
         int32_t lenRegNum = toRealRegister(s->getLengthReg())->getRegisterNumber();
         int32_t srcKeyRegNum = toRealRegister(s->getSourceKeyReg())->getRegisterNumber();
         registerUsageInfo[blockNum]->set(lenRegNum);
         if (traceIt)
            traceMsg(self()->comp(), "SS4 instr [%p] USES register [%d %d]\n", s, lenRegNum, srcKeyRegNum);

         TR::MemoryReference * mr = s->getMemoryReference();
         if (traceIt)
            traceMsg(self()->comp(), "SS2 instr [%p] USES mr baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());

         mr = s->getMemoryReference2();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS4 instr [%p] USES mr baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SS4 instr [%p] USES mr indexRegister [%d]\n", s, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         return true;
         }
      case TR::Instruction::IsSSF:
         {
         TR::S390SSFInstruction *s = (TR::S390SSFInstruction *)s390Instr;
         int32_t firstRegNum = toRealRegister(s->getFirstRegister())->getRegisterNumber();
         registerUsageInfo[blockNum]->set(firstRegNum);
         if (traceIt)
            traceMsg(self()->comp(), "SSF instr [%p] USES register [%d]\n", s, firstRegNum);

         TR::MemoryReference * mr = s->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SSF instr [%p] USES mr1 baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SSF instr [%p] USES mr1 indexRegister [%d]\n", s, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }

         mr = s->getMemoryReference2();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SSF instr [%p] USES mr2 baseRegister [%d]\n", s, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "SSF instr [%p] USES mr2 indexRegister [%d]\n", s, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         return true;
         }
      case TR::Instruction::IsRRS:
      case TR::Instruction::IsRIE:
         {
         int32_t tgtRegNum, srcRegNum;
         if (s390Instr->getRegisterOperand(1))
            {
            tgtRegNum = toRealRegister(s390Instr->getRegisterOperand(1))->getRegisterNumber();
            registerUsageInfo[blockNum]->set(tgtRegNum);
            }

         if (s390Instr->getRegisterOperand(2))
            {
            srcRegNum = toRealRegister(s390Instr->getRegisterOperand(2))->getRegisterNumber();
            }

         if (traceIt)
            traceMsg(self()->comp(), "RRS/RIE instr [%p] USES registers [%d %d]\n", s390Instr, srcRegNum, tgtRegNum);
         return true;
         }
      case TR::Instruction::IsRIS:
         {
         int32_t tgtRegNum = toRealRegister(s390Instr->getRegisterOperand(1))->getRegisterNumber();
         if (traceIt)
            traceMsg(self()->comp(), "RIS instr [%p] USES register [%d]\n", s390Instr, tgtRegNum);
         registerUsageInfo[blockNum]->set(tgtRegNum);

         TR::MemoryReference * mr = s390Instr->getMemoryReference();
         if (mr->getBaseRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "RIS instr [%p] USES mr baseRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getBaseRegister())->getRegisterNumber());
            }
         if (mr->getIndexRegister())
            {
            if (traceIt)
               traceMsg(self()->comp(), "RIS instr [%p] USES mr indexRegister [%d]\n", s390Instr, ((TR::RealRegister*)mr->getIndexRegister())->getRegisterNumber());
            }
         return true;
         }
      default:
         {
         ///TR_ASSERTC( 0,_jit, "unexpected instruction kind");
         return false;
         }
      }


   return false;
   }

uint32_t
OMR::Z::CodeGenerator::isPreservedRegister(int32_t regIndex)
   {
   // is register preserved in prologue
   //
   for (int32_t pindex = TR::RealRegister::GPR6;
         pindex <= TR::RealRegister::GPR12;
         pindex++)
      {
      TR::RealRegister::RegNum idx = REGNUM(pindex);
      if (idx == REGNUM(regIndex))
         return pindex;
      }
   return -1;
   }

bool
OMR::Z::CodeGenerator::isReturnInstruction(TR::Instruction *instr)
   {
   return instr->getOpCodeValue() == TR::InstOpCode::RET;
   }

bool
OMR::Z::CodeGenerator::isBranchInstruction(TR::Instruction *instr)
   {
   return instr->isBranchOp();
   }

bool
OMR::Z::CodeGenerator::isLabelInstruction(TR::Instruction *instr)
   {
   return instr->isLabel();
   }

int32_t
OMR::Z::CodeGenerator::isFenceInstruction(TR::Instruction *instr)
   {
   TR::Instruction *s390Instr = instr;

   if (s390Instr->getOpCodeValue() == TR::InstOpCode::FENCE)
      {
      TR::Node *fenceNode = s390Instr->getNode();
      if (fenceNode->getOpCodeValue() == TR::BBStart)
         return 1;
      else if (fenceNode->getOpCodeValue() == TR::BBEnd)
         return 2;
      }

   return 0;
   }

bool
OMR::Z::CodeGenerator::isAlignmentInstruction(TR::Instruction *instr)
   {
   return false;
   }

TR::Instruction *
OMR::Z::CodeGenerator::splitEdge(TR::Instruction *instr,
                                 bool isFallThrough,
                                 bool needsJump,
                                 TR::Instruction *newSplitLabel,
                                 TR::list<TR::Instruction*> *jmpInstrs,
                                 bool firstJump)
   {
   // instr is the jump instruction containing the target
   // this is the edge that needs to be split
   // if !isFallThrough, then the instr points at the BBEnd
   // of the block containing the jump
   //
   TR::LabelSymbol *newLabel = NULL;
   if (!newSplitLabel)
      newLabel = generateLabelSymbol(self());
   else
      {
      newLabel = ((TR::S390LabelInstruction *)newSplitLabel)->getLabelSymbol();
      }
   TR::LabelSymbol *targetLabel = NULL;
   TR::Instruction *location = NULL;
   if (isFallThrough)
      {
      location = instr;
      }
   else
      {
      // compare and branch
      if (instr->getKind() == TR::Instruction::IsRIE)
         {
         TR::S390RIEInstruction *labelInstr = (TR::S390RIEInstruction *)instr;
         targetLabel = labelInstr->getBranchDestinationLabel();
         labelInstr->setBranchDestinationLabel(newLabel);
         location = targetLabel->getInstruction()->getPrev();
         }
      else
         {
         TR::S390LabeledInstruction *labelInstr = (TR::S390LabeledInstruction *)instr;
         targetLabel = labelInstr->getLabelSymbol();
         labelInstr->setLabelSymbol(newLabel);
         location = targetLabel->getInstruction()->getPrev();
         }
      // now fixup any remaining jmp instrs that jmp to the target
      // so that they now jmp to the new label
      //
      for (auto jmpItr = jmpInstrs->begin(); jmpItr != jmpInstrs->end(); ++jmpItr)
         {
         TR::S390LabeledInstruction *l = (TR::S390LabeledInstruction *)(*jmpItr);
         if (l->getLabelSymbol() == targetLabel)
            {
            traceMsg(self()->comp(), "split edge fixing jmp instr %p\n", *jmpItr);
            l->setLabelSymbol(newLabel);
            }
         }
      }

   TR::Instruction *cursor = newSplitLabel;
   if (!cursor)
      cursor = generateS390LabelInstruction(self(), TR::InstOpCode::LABEL, location->getNode(), newLabel, location);

   if (!isFallThrough && needsJump)
      {
      TR::Instruction *jmpLocation = cursor->getPrev();
      TR::Instruction *i = generateS390BranchInstruction(self(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, jmpLocation->getNode(), targetLabel, jmpLocation);
      traceMsg(self()->comp(), "split edge jmp instr at [%p]\n", i);
      }
   return cursor;
   }

TR::Instruction *
OMR::Z::CodeGenerator::splitBlockEntry(TR::Instruction *instr)
   {
   TR::LabelSymbol *newLabel = generateLabelSymbol(self());
   TR::Instruction *location = instr->getPrev();
   return generateS390LabelInstruction(self(), TR::InstOpCode::LABEL, location->getNode(), newLabel, location);
   }

int32_t
OMR::Z::CodeGenerator::computeRegisterSaveDescription(TR_BitVector *regs, bool populateInfo)
   {
   uint32_t rsd = 0;
   // if the reg is present in this bitvector,
   // then it must have been assigned.
   //
   TR_BitVectorIterator regIt(*regs);
   while (regIt.hasMoreElements())
      {
      int32_t regIndex = self()->isPreservedRegister(regIt.getNextElement());
      if (regIndex != -1)
         {
         rsd |= 1 << (REGNUM(regIndex) - 1);
         }
      }
   // place the register save size in the top half
   rsd |= self()->getLinkage()->getRegisterSaveSize() << 16;
   return rsd;
   }

void
OMR::Z::CodeGenerator::processIncomingParameterUsage(TR_BitVector **registerUsageInfo, int32_t blockNum)
   {
   TR::ResolvedMethodSymbol             *bodySymbol = self()->comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>  paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol               *paramCursor;

   for (paramCursor = paramIterator.getFirst();
       paramCursor != NULL;
       paramCursor = paramIterator.getNext())
      {
      int32_t ai = paramCursor->getAllocatedIndex();
      int32_t ai_l = paramCursor->getAllocatedLow();

      traceMsg(self()->comp(), "found %d %d used as parms\n", ai, ai_l);
      if (ai > 0)
         registerUsageInfo[blockNum]->set(ai);
      if (ai_l > 0)
         registerUsageInfo[blockNum]->set(ai_l);
      }

   int32_t litPoolNumber = self()->getLinkage(bodySymbol->getLinkageConvention())->setupLiteralPoolRegister(self()->getFirstSnippet());
   if (litPoolNumber > 0)
      registerUsageInfo[blockNum]->set(litPoolNumber);
   }

void
OMR::Z::CodeGenerator::updateSnippetMapWithRSD(TR::Instruction *instr, int32_t rsd)
   {
   // at this point, instr is a jmp instruction
   // determine if it branches to a snippet and if so
   // mark the maps in the snippet with the right rsd
   //
   TR::S390LabelInstruction *labelInstr = (TR::S390LabelInstruction *)instr;
   TR::LabelSymbol *targetLabel = labelInstr->getLabelSymbol();

   TR_S390OutOfLineCodeSection *oiCursor = self()->findOutLinedInstructionsFromLabel(targetLabel);
   if (oiCursor)
      {
      TR::Instruction *cur = oiCursor->getFirstInstruction();
      TR::Instruction *end = oiCursor->getAppendInstruction();
      traceMsg(self()->comp(), "found oi start %p end %p\n", cur, end);
      while (cur != end)
         {
         if (cur->needsGCMap())
            {
            TR_GCStackMap *map = cur->getGCMap();
            if (map)
               {
               traceMsg(self()->comp(), "      instr %p rsd %x map %p\n", cur, rsd, map);
               map->setRegisterSaveDescription(rsd);
               }
            }
         cur = cur->getNext();
         }
      }
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

bool
OMR::Z::CodeGenerator::isTargetSnippetOrOutOfLine(TR::Instruction *instr, TR::Instruction **start, TR::Instruction **end)
   {
   TR::S390LabelInstruction *labelInstr = (TR::S390LabelInstruction *)instr;
   TR::LabelSymbol *targetLabel = labelInstr->getLabelSymbol();
   TR_S390OutOfLineCodeSection *oiCursor = self()->findOutLinedInstructionsFromLabel(targetLabel);
   if (oiCursor)
      {
      *start = oiCursor->getFirstInstruction();
      *end = oiCursor->getAppendInstruction();
      return true;
      }
   else
      return false;
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


   if (self()->supportsHighWordFacility() && !self()->comp()->getOption(TR_DisableHighWordRA))
      {
      for (int32_t i = TR::RealRegister::FirstHPR; i <= TR::RealRegister::LastHPR; i++)
         {
         TR::RealRegister * realReg = self()->machine()->getS390RealRegister((TR::RealRegister::RegNum) i);
         if (realReg->getHasBeenAssignedInMethod())
            {
            TR::Register * virtReg = realReg->getAssignedRegister();
            if (virtReg && virtReg->containsCollectedReference() && virtReg->getAssignedRegister() == NULL)
               {
               // 2 bits per register, '10' means HPR has collectible, '11' means both HPR and GPR have collectibles
               map->setHighWordRegisterBits(1 << ((i - TR::RealRegister::FirstHPR)*2 +1) );
               //traceMsg(comp(), "\nsetting HPR regmap 0x%x\n", 1 << ((i - TR::RealRegister::FirstHPR)*2 + 1));
               }
            }
         }
      }
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
      {
      TR::RealRegister * realReg = self()->machine()->getS390RealRegister((TR::RealRegister::RegNum) i);
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

TR::Register*
OMR::Z::CodeGenerator::copyRestrictedVirtual(TR::Register * virtReg, TR::Node *node, TR::Instruction ** preced)
      {
      TR::Register *copyReg;
      TR::Instruction *i = NULL;
      if (virtReg->getKind() == TR_GPR64)
         {
         copyReg = self()->allocate64bitRegister();
         i = generateRRInstruction(self(), TR::InstOpCode::LGR, node, copyReg, virtReg, preced ? *preced : NULL);
         }
      else
         {
         copyReg = self()->allocateRegister();
         i = generateRRInstruction(self(), TR::InstOpCode::LR, node, copyReg, virtReg, preced ? *preced : NULL);
         }
      if (preced && *preced)
         *preced = i;
      return copyReg;
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

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::CodeGenerator::dumpTargetAddressSnippets
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::CodeGenerator::dumpTargetAddressSnippets(TR::FILE *outFile)
   {

   if (outFile == NULL)
      {
      return;
      }

   int32_t size;

   for (auto iterator = _targetList.begin(); iterator != _targetList.end(); ++iterator)
      {
      self()->getDebug()->print(outFile, *iterator);
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

bool
OMR::Z::CodeGenerator::constLoadNeedsLiteralFromPool(TR::Node *node)
   {
   if (node->isClassUnloadingConst() || node->getType().isIntegral() || node->getType().isAddress())
      {
      return false;
      }
   else
      {
      return true;  // Floats/Doubles require literal pool
      }
   }

void
OMR::Z::CodeGenerator::setGlobalStaticBaseRegisterOnFlag()
   {
   if (self()->comp()->getOption(TR_DisableGlobalStaticBaseRegister))
      self()->setGlobalStaticBaseRegisterOn(false);
   }

bool
OMR::Z::CodeGenerator::isAddressOfStaticSymRefWithLockedReg(TR::SymbolReference *symRef)
   {
   return false;
   }

bool
OMR::Z::CodeGenerator::isAddressOfPrivateStaticSymRefWithLockedReg(TR::SymbolReference *symRef)
   {
   return false;
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

   if (TR::Compiler->target.is32Bit() && !self()->use64BitRegsOn32Bit()) return true;

   if (TR::Compiler->target.isLinux())
      {
      TR_FrontEnd * fe = self()->comp()->fe();
      intptrj_t codeCacheBase, codeCacheTop;
      // _currentCodeCache in the compilation object is set right before Instruction selection.
      // if it returns null, we have a call during optimizer stage, in which case, we use
      // the next best estimate which is the base of our first code cache.
      if (self()->comp()->getCurrentCodeCache() == NULL)
         {
         codeCacheBase = (intptrj_t)fe->getCodeCacheBase();
         codeCacheTop = (intptrj_t)fe->getCodeCacheTop();
         }
      else
         {
         codeCacheBase = (intptrj_t)fe->getCodeCacheBase(self()->comp()->getCurrentCodeCache());
         codeCacheTop = (intptrj_t)fe->getCodeCacheTop(self()->comp()->getCurrentCodeCache());
         }
      return ( (((intptrj_t)value - codeCacheBase ) <=  (intptrj_t)(INT_MAX))
            && (((intptrj_t)value - codeCacheBase ) >=  (intptrj_t)(INT_MIN))
            && (((intptrj_t)value - codeCacheTop ) <=  (intptrj_t)(INT_MAX))
            && (((intptrj_t)value - codeCacheTop ) >=  (intptrj_t)(INT_MIN)) );
      }
   else
      {
      return (intptrj_t)value<=(intptrj_t)(INT_MAX) && (intptrj_t)value>=(intptrj_t)(INT_MIN);
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

/**
 * Check if BNDS check should use a CLFI
 */
bool
OMR::Z::CodeGenerator::bndsChkNeedsLiteralFromPool(TR::Node *node)
   {
   int64_t value=getIntegralValue(node);

   if (value <= GE_MAX_IMMEDIATE_VAL && value >= GE_MIN_IMMEDIATE_VAL)
      {
      return false;
      }
   else
      {
      return true;
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

/**
 * Check if arithmetic operations with a constant requires entry in the literal pool.
 */
bool
OMR::Z::CodeGenerator::arithmeticNeedsLiteralFromPool(TR::Node *node)
   {
   int64_t value = getIntegralValue(node);

   return value > GE_MAX_IMMEDIATE_VAL || value < GE_MIN_IMMEDIATE_VAL;
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
      if (self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10) &&
          !self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
         {
         if (tReg->getKind() != TR_FPR)
            {
            return false;
            }
         }

      if (opcd == ccInst->getOpCodeValue() && tReg == ccTgtReg &&  sReg == ccSrcReg &&
          performTransformation(self()->comp(), "O^O isActiveCompareCC: RR Compare Op [%s\t %s, %s] can reuse CC from ccInstr [%p]\n",
             self()->getDebug()->getOpCodeName(&ccInst->getOpCode()), self()->getDebug()->getName(tReg),self()->getDebug()->getName(sReg), ccInst))
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
   bool is64BitReg = (targetRegister->getKind() == TR_GPR64);

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
         TR_ASSERT(TR::Compiler->target.is64Bit() || is64BitReg, "This shift can only work on 64 bits register!\n");

         // Can use Load Halfword to sign extended & load bits 48-63 from source to target register
         generateRRInstruction(self(), TR::InstOpCode::LGHR, node, targetRegister, srcRegister);
         break;

      case 56:
         TR_ASSERT(TR::Compiler->target.is64Bit() || is64BitReg, "This shift can only work on 64 bits register!\n");

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
         TR_ASSERT(TR::Compiler->target.is64Bit() || srcRegister->getKind()==TR_GPR64, "This shift can only work on 64 bits register!\n");

         // Can use Load Logical Halfword to load bits 48-63 from source to target register.
         generateRRInstruction(self(), TR::InstOpCode::LLGHR, node, targetRegister, srcRegister);
         break;

      case 56:
         TR_ASSERT(TR::Compiler->target.is64Bit() || srcRegister->getKind()==TR_GPR64, "This shift can only work on 64 bits register!\n");

         // Can use Load Logical Character to load bits 56-63 from source to target register.
         generateRRInstruction(self(), TR::InstOpCode::LLGCR, node, targetRegister, srcRegister);
         break;

      default:
         TR_ASSERT( 0, "No appropriate instruction to use for clearing this number of bits!\n");
         break;
      }

   return true;
   }

TR::RegisterDependencyConditions *
OMR::Z::CodeGenerator::addVMThreadPreCondition(TR::RegisterDependencyConditions *deps, TR::Register *reg)
   {
   if (self()->needsVMThreadDependency())
      {
      //TODO This IF statement is for codegen level vm thread work, where we
      //allocate a fixed virtual register to represent the vm thread register.
      //for IL level vm thread work, any register can be used, so we need to
      //replace this IF statement with an TR_ASSERTC( reg,comp(), "reg cannot be NULL\n");
      if (reg == NULL)
         {
         reg = self()->getVMThreadRegister();
         }
      if (deps == NULL)
         {
         deps = generateRegisterDependencyConditions(1,0, self());
         }
      else if (deps->getPreConditions() == NULL)
         {
         deps->setNumPreConditions(1, self()->trMemory());
         }
      else
         {
         traceMsg(self()->comp(), "copying and expanding dependency list, numPre = %i, numPost = %i\n", deps->getNumPreConditions(), deps->getNumPostConditions());
         deps = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(deps, 1, 0, self());
         }
      //TODO: using addPreConditionIfNotAlreadyInserted() is slower than using addPreCondition()
      //think about changing the name of this method to addVMThreadPreConditionIfNotAlreadyInserted()
      //and add one for addVMThreadPreCondition, which uses deps->addPreCondition()
      deps->addPreConditionIfNotAlreadyInserted(reg,(TR::RealRegister::RegNum)self()->getVMThreadRegister()->getAssociation());
      }
   return deps;
   }

TR::RegisterDependencyConditions *
OMR::Z::CodeGenerator::addVMThreadPostCondition(TR::RegisterDependencyConditions *deps, TR::Register *reg)
   {
   if (self()->needsVMThreadDependency())
      {
      //TODO This IF statement is for codegen level vm thread work, where we
      //allocate a fixed virtual register to represent the vm thread register.
      //for IL level vm thread work, any register can be used, so we need to
      //replace this IF statement with an TR_ASSERTC( reg,comp(), "reg cannot be NULL\n");
      if (reg == NULL)
         {
         reg = self()->getVMThreadRegister();
         }
      if (deps == NULL)
         {
         deps = generateRegisterDependencyConditions(0,0, self());
         }
      //TODO: using addPostConditionIfNotAlreadyInserted() is slower than using addPostCondition()
      //think about changing the name of this method to addVMThreadPostConditionIfNotAlreadyInserted()
      //and add one for addVMThreadPostCondition, which uses deps->addPostCondition()
      deps->addPostConditionIfNotAlreadyInserted(reg,(TR::RealRegister::RegNum)self()->getVMThreadRegister()->getAssociation());
      }
   return deps;
   }

TR::RegisterDependencyConditions *
OMR::Z::CodeGenerator::addVMThreadDependencies(TR::RegisterDependencyConditions *deps, TR::Register *reg)
   {
   deps = self()->addVMThreadPreCondition(deps, reg);
   deps = self()->addVMThreadPostCondition(deps, reg);
   return deps;
   }

void
OMR::Z::CodeGenerator::setVMThreadRequired(bool v)
   {
   if (self()->needsVMThreadDependency())
      self()->setVMThreadRequired(v);
   }

bool OMR::Z::CodeGenerator::ilOpCodeIsSupported(TR::ILOpCodes o)
   {
   if (!OMR::CodeGenerator::ilOpCodeIsSupported(o))
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
   if (TR::Compiler->target.is64Bit())
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
         int32_t globalRegNumAR = globalRegNum - self()->getFirstGlobalGPR() + self()->getFirstGlobalAR();
         if (globalRegNum!=-1 && globalRegNumAR!=-1)
            {
            liveOnEntryUsage[globalRegNumAR].set(b->getNumber());
            liveOnExitUsage[globalRegNumAR].set(b->getNumber());
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

         TR_BitVector *volatileRegs = self()->comp()->cg()->getGlobalRegisters(TR_volatileSpill, callLinkage);

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

int32_t
OMR::Z::CodeGenerator::biasDecimalFloatFrac(TR::DataType dt, int32_t frac)
   {
   switch (dt)
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalFloat:      return TR_DECIMAL_FLOAT_BIAS-frac;
      case TR::DecimalDouble:     return TR_DECIMAL_DOUBLE_BIAS-frac;
      case TR::DecimalLongDouble: return TR_DECIMAL_LONG_DOUBLE_BIAS-frac;
#endif
      default: TR_ASSERT(0, "unexpected biasDecimalFloatFrac dt %s",dt.toString());
      }
   return 0;
   }

TR::Instruction *
OMR::Z::CodeGenerator::genLoadAddressToRegister(TR::Register *reg, TR::MemoryReference *origMR, TR::Node *node, TR::Instruction *preced)
   {
   intptrj_t offset = origMR->getOffset();
   TR::CodeGenerator *cg = self();
   if (offset >= TR_MIN_RX_DISP && offset <= TR_MAX_RX_DISP)
      {
      preced = generateRXInstruction(cg, TR::InstOpCode::LA, node, reg, origMR, preced);
      }
   else if (offset > MINLONGDISP && offset < MAXLONGDISP)
      {
      preced = generateRXYInstruction(cg, TR::InstOpCode::LAY, node, reg, origMR, preced);
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
      return opcode.isDiv() || opcode.isExponentiation() || opcode.isRem()
#ifdef J9_PROJECT_SPECIFIC
         || opcode.getOpCodeValue() == TR::pddivrem
#endif
         ;
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

/**
 * Determine if the Left-to-right copy semantics is allowed on NDmemcpyWithPad
 * Communicates with the evaluator to do MVC semantics under certain condition no matter how the overlap is
 */
bool OMR::Z::CodeGenerator::inlineNDmemcpyWithPad(TR::Node * node, int64_t * maxLengthPtr)
      {
      return false;
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
         opCodeToUse == TR::InstOpCode::LAM || opCodeToUse == TR::InstOpCode::LMY || opCodeToUse == TR::InstOpCode::VLM))
      return;

   bool isAR = opCodeToUse == TR::InstOpCode::LAM;
   bool isVector = opCodeToUse == TR::InstOpCode::VLM ? true : false;
   uint32_t numVRFs = TR::RealRegister::LastAssignableVRF - TR::RealRegister::FirstAssignableVRF;

   TR::Register *lowReg  = isVector ? toS390VRSInstruction(inst)->getFirstRegister()  : toS390RSInstruction(inst)->getFirstRegister();
   TR::Register *highReg = isVector ? toS390VRSInstruction(inst)->getSecondRegister() : toS390RSInstruction(inst)->getSecondRegister();

   if (!cg->getRAPassAR() && lowReg->getKind() == TR_AR)
      return;
   else if (inst->getRegisterOperand(1)->getRegisterPair())
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
      TR::RealRegister *reg = cg->machine()->getS390RealRegister(i + (isAR ? TR::RealRegister::AR0 : TR::RealRegister::GPR0));

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
   TR::Snippet *constant = self()->Create8ByteConstant(cursor->getNode(), reinterpret_cast<intptrj_t> (counter->getBumpCountSymRef(self()->comp())->getSymbol()->getStaticSymbol()->getStaticAddress()), false);
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

   if ((op == TR::a2i || op == TR::i2a) && TR::Compiler->target.is32Bit())
      return true;

   if ((op == TR::a2l || op == TR::l2a) && TR::Compiler->target.is64Bit() &&
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

bool
OMR::Z::CodeGenerator::globalAccessRegistersSupported()
   {
   return false; // TODO : Identitiy needs folding
   }

void
OMR::Z::CodeGenerator::setRAPassAR()
   {
   TR_ASSERT(0, "ASC Mode AR not supported");
   }

void
OMR::Z::CodeGenerator::resetRAPassAR()
   {
   TR_ASSERT(0, "ASC Mode AR not supported");
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
   if (dt == TR::Float && !self()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z14))
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

bool TR_S390Peephole::forwardBranchTarget()
   {
   TR::LabelSymbol *targetLabelSym = NULL;
   switch(_cursor->getOpCodeValue())
      {
      case TR::InstOpCode::BRC: targetLabelSym = ((TR::S390BranchInstruction*)_cursor)->getLabelSymbol(); break;
      case TR::InstOpCode::CRJ:
      case TR::InstOpCode::CGRJ:
      case TR::InstOpCode::CIJ:
      case TR::InstOpCode::CGIJ:
      case TR::InstOpCode::CLRJ:
      case TR::InstOpCode::CLGRJ:
      case TR::InstOpCode::CLIJ:
      case TR::InstOpCode::CLGIJ: targetLabelSym = toS390RIEInstruction(_cursor)->getBranchDestinationLabel(); break;
      default:
         return false;
      }
   if (!targetLabelSym)
      return false;

   auto targetLabelInsn = targetLabelSym->getInstruction();
   if (!targetLabelInsn)
      return false;
   auto tmp = targetLabelInsn;
   while (tmp->isLabel() || _cg->isFenceInstruction(tmp))  // skip labels and fences
      tmp = tmp->getNext();
   if (tmp->getOpCodeValue() == TR::InstOpCode::BRC)
      {
      auto firstBranch = (TR::S390BranchInstruction*)tmp;
      if (firstBranch->getBranchCondition() == TR::InstOpCode::COND_BRC &&
          performTransformation(comp(), "\nO^O S390 PEEPHOLE: forwarding branch target in %p\n", _cursor))
         {
         auto newTargetLabelSym = firstBranch->getLabelSymbol();
         switch(_cursor->getOpCodeValue())
            {
            case TR::InstOpCode::BRC: ((TR::S390BranchInstruction*)_cursor)->setLabelSymbol(newTargetLabelSym); break;
            case TR::InstOpCode::CRJ:
            case TR::InstOpCode::CGRJ:
            case TR::InstOpCode::CIJ:
            case TR::InstOpCode::CGIJ:
            case TR::InstOpCode::CLRJ:
            case TR::InstOpCode::CLGRJ:
            case TR::InstOpCode::CLIJ:
            case TR::InstOpCode::CLGIJ:
               toS390RIEInstruction(_cursor)->setBranchDestinationLabel(newTargetLabelSym); break;
            default:
               return false;
            }
         return true;
         }
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
      if (self()->comp()->getOption(TR_TraceVIP))
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
   if (self()->comp()->getOption(TR_TraceVIP))
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

   if (op.getOpCodeValue() == TR::pddivrem)
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
