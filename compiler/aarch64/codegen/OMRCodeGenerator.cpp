/*******************************************************************************
 * Copyright (c) 2018, 2021 IBM Corp. and others
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

#include <stdlib.h>

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64OutOfLineCodeSection.hpp"
#include "codegen/ARM64SystemLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterIterator.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "control/Recompilation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "ras/DebugCounter.hpp"

OMR::ARM64::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
      OMR::CodeGenerator(comp),
      _dataSnippetList(getTypedAllocator<TR::ARM64ConstantDataSnippet*>(comp->allocator())),
      _outOfLineCodeSectionList(getTypedAllocator<TR_ARM64OutOfLineCodeSection*>(comp->allocator())),
      _firstTimeLiveOOLRegisterList(NULL)
   {
   }

void
OMR::ARM64::CodeGenerator::initialize()
   {
   self()->OMR::CodeGenerator::initialize();

   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = self()->comp();

   // Initialize Linkage for Code Generator
   cg->initializeLinkage();

   _unlatchedRegisterList =
      (TR::RealRegister**)cg->trMemory()->allocateHeapMemory(sizeof(TR::RealRegister*)*(TR::RealRegister::NumRegisters + 1));

   _unlatchedRegisterList[0] = 0; // mark that list is empty

   _linkageProperties = &cg->getLinkage()->getProperties();

   cg->setStackPointerRegister(cg->machine()->getRealRegister(_linkageProperties->getStackPointerRegister()));
   cg->setMethodMetaDataRegister(cg->machine()->getRealRegister(_linkageProperties->getMethodMetaDataRegister()));

   // Tactical GRA settings
   //
   cg->setGlobalRegisterTable(_linkageProperties->getRegisterAllocationOrder());
   _numGPR = _linkageProperties->getNumAllocatableIntegerRegisters();
   _numFPR = _linkageProperties->getNumAllocatableFloatRegisters();
   cg->setLastGlobalGPR(_numGPR - 1);
   cg->setLastGlobalFPR(_numGPR + _numFPR - 1);

   cg->getLinkage()->initARM64RealRegisterLinkage();
   cg->setSupportsGlRegDeps();
   cg->setSupportsGlRegDepOnFirstBlock();

   cg->addSupportedLiveRegisterKind(TR_GPR);
   cg->addSupportedLiveRegisterKind(TR_FPR);
   cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_GPR);
   cg->setLiveRegisters(new (cg->trHeapMemory()) TR_LiveRegisters(comp), TR_FPR);

   cg->setSupportsVirtualGuardNOPing();

   cg->setSupportsRecompilation();

   cg->setSupportsSelect();


   if (!comp->getOption(TR_DisableTraps) && TR::Compiler->vm.hasResumableTrapHandler(comp))
      {
      _numberBytesReadInaccessible = 4096;
      _numberBytesWriteInaccessible = 4096;
      cg->setHasResumableTrapHandler();
      }
   else
      {
      _numberBytesReadInaccessible = 0;
      _numberBytesWriteInaccessible = 0;
      }

   if (!comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      for (int32_t i = 0; i < TR_numSpillKinds; i++)
         _globalRegisterBitVectors[i].init(cg->getNumberOfGlobalRegisters(), cg->trMemory());

      for (TR_GlobalRegisterNumber grn=0; grn < cg->getNumberOfGlobalRegisters(); grn++)
         {
         TR::RealRegister::RegNum reg = (TR::RealRegister::RegNum)cg->getGlobalRegister(grn);
         if (cg->getFirstGlobalGPR() <= grn && grn <= cg->getLastGlobalGPR())
            _globalRegisterBitVectors[ TR_gprSpill ].set(grn);
         else if (cg->getFirstGlobalFPR() <= grn && grn <= cg->getLastGlobalFPR())
            _globalRegisterBitVectors[ TR_fprSpill ].set(grn);

         if (!cg->getProperties().getPreserved(reg))
            _globalRegisterBitVectors[ TR_volatileSpill ].set(grn);
         if (cg->getProperties().getIntegerArgument(reg) || cg->getProperties().getFloatArgument(reg))
            _globalRegisterBitVectors[ TR_linkageSpill  ].set(grn);
         }
      }

   // Calculate inverse of getGlobalRegister function
   //
   TR_GlobalRegisterNumber grn;
   int i;

   TR_GlobalRegisterNumber globalRegNumbers[TR::RealRegister::NumRegisters];
   for (i = 0; i < cg->getNumberOfGlobalGPRs(); i++)
     {
     grn = cg->getFirstGlobalGPR() + i;
     globalRegNumbers[cg->getGlobalRegister(grn)] = grn;
     }
   for (i = 0; i < cg->getNumberOfGlobalFPRs(); i++)
     {
     grn = cg->getFirstGlobalFPR() + i;
     globalRegNumbers[cg->getGlobalRegister(grn)] = grn;
     }

   // Initialize linkage reg arrays
   TR::ARM64LinkageProperties linkageProperties = cg->getProperties();
   for (i = 0; i < linkageProperties.getNumIntArgRegs(); i++)
     _gprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getIntegerArgumentRegister(i)];
   for (i = 0; i < linkageProperties.getNumFloatArgRegs(); i++)
     _fprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getFloatArgumentRegister(i)];

   if (comp->getOption(TR_TraceRA))
      {
      cg->setGPRegisterIterator(new (cg->trHeapMemory()) TR::RegisterIterator(cg->machine(), TR::RealRegister::FirstGPR, TR::RealRegister::LastGPR));
      cg->setFPRegisterIterator(new (cg->trHeapMemory()) TR::RegisterIterator(cg->machine(), TR::RealRegister::FirstFPR, TR::RealRegister::LastFPR));
      }

   cg->getLinkage()->setParameterLinkageRegisterIndex(comp->getJittedMethodSymbol());

   if (comp->target().isSMP())
      cg->setEnforceStoreOrder();
   }

void
OMR::ARM64::CodeGenerator::beginInstructionSelection()
   {
   TR::Compilation *comp = self()->comp();
   TR::Node *startNode = comp->getStartTree()->getNode();
   if (comp->getMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      _returnTypeInfoInstruction = generateImmInstruction(self(), TR::InstOpCode::dd, startNode, 0);
      }
   else
      {
      _returnTypeInfoInstruction = NULL;
      }

   new (self()->trHeapMemory()) TR::ARM64AdminInstruction(TR::InstOpCode::proc, startNode, NULL, _returnTypeInfoInstruction, self());
   }

void
OMR::ARM64::CodeGenerator::endInstructionSelection()
   {
   if (_returnTypeInfoInstruction != NULL)
      {
      _returnTypeInfoInstruction->setSourceImmediate(static_cast<uint32_t>(self()->comp()->getReturnInfo()));
      }
   }

void
OMR::ARM64::CodeGenerator::doRegisterAssignment(TR_RegisterKinds kindsToAssign)
   {
   // Registers are assigned in backward direction

   TR::Compilation *comp = self()->comp();

   TR::Instruction *instructionCursor = self()->getAppendInstruction();

   if (!comp->getOption(TR_DisableOOL))
      {
      if (!self()->isOutOfLineColdPath())
         {
         // Allocates these lists when the register assignment starts.
         // As doRegisterAssignment is called mutiple times for the cold path of OutOfLineCodeSections,
         // we do allocation only when we are not in the cold path.
         TR::list<TR::Register*> *firstTimeLiveOOLRegisterList = new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::Register*>(self()->comp()->allocator()));
         self()->setFirstTimeLiveOOLRegisterList(firstTimeLiveOOLRegisterList);

         TR::list<TR::Register*> *spilledRegisterList = new (self()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::Register*>(comp->allocator()));
         self()->setSpilledRegisterList(spilledRegisterList);
         }
      }

   if (self()->getDebug())
      self()->getDebug()->startTracingRegisterAssignment();

   while (instructionCursor)
      {
      TR::Instruction *prevInstruction = instructionCursor->getPrev();

      self()->tracePreRAInstruction(instructionCursor);

      self()->setCurrentBlockIndex(instructionCursor->getBlockIndex());

      instructionCursor->assignRegisters(TR_GPR);

      // Maintain Internal Control Flow Depth
      // Track internal control flow on labels
      if (instructionCursor->isLabel())
         {
         TR::ARM64LabelInstruction *li = (TR::ARM64LabelInstruction *)instructionCursor;

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

      self()->freeUnlatchedRegisters();
      self()->buildGCMapsForInstructionAndSnippet(instructionCursor);

      self()->tracePostRAInstruction(instructionCursor);

      instructionCursor = prevInstruction;
      }

   if (self()->getDebug())
      {
      self()->getDebug()->stopTracingRegisterAssignment();
      }
   }

void
OMR::ARM64::CodeGenerator::generateBinaryEncodingPrePrologue(TR_ARM64BinaryEncodingData &data)
   {
   data.recomp = NULL;
   data.cursorInstruction = self()->getFirstInstruction();
   data.i2jEntryInstruction = data.cursorInstruction;
   }

void
OMR::ARM64::CodeGenerator::doBinaryEncoding()
   {
   TR_ARM64BinaryEncodingData data;
   data.estimate = 0;
   TR::Compilation *comp = self()->comp();
   TR::Instruction *tempInstruction;

   self()->generateBinaryEncodingPrePrologue(data);

   data.cursorInstruction = self()->getFirstInstruction();

   while (data.cursorInstruction && data.cursorInstruction->getOpCodeValue() != TR::InstOpCode::proc)
      {
      data.estimate = data.cursorInstruction->estimateBinaryLength(data.estimate);
      data.cursorInstruction = data.cursorInstruction->getNext();
      }

   tempInstruction = data.cursorInstruction;

   if (data.recomp != NULL)
      {
      tempInstruction = data.recomp->generatePrologue(tempInstruction);
      }

   self()->getLinkage()->createPrologue(tempInstruction);

   bool skipOneReturn = false;
   while (data.cursorInstruction)
      {
      if (data.cursorInstruction->getOpCodeValue() == TR::InstOpCode::retn)
         {
         if (skipOneReturn == false)
            {
            TR::Instruction *temp = data.cursorInstruction->getPrev();
            self()->getLinkage()->createEpilogue(temp);
            data.cursorInstruction = temp->getNext();
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

   data.estimate = self()->setEstimatedLocationsForSnippetLabels(data.estimate);

   self()->setEstimatedCodeLength(data.estimate);

   data.cursorInstruction = self()->getFirstInstruction();
   uint8_t *coldCode = NULL;
   uint8_t *temp = self()->allocateCodeMemory(self()->getEstimatedCodeLength(), 0, &coldCode);

   self()->setBinaryBufferStart(temp);
   self()->setBinaryBufferCursor(temp);
   self()->alignBinaryBufferCursor();

   while (data.cursorInstruction)
      {
      self()->setBinaryBufferCursor(data.cursorInstruction->generateBinaryEncoding());
      self()->addToAtlas(data.cursorInstruction);

      if (data.cursorInstruction == data.i2jEntryInstruction)
         {
         self()->setPrePrologueSize(self()->getBinaryBufferCursor() - self()->getBinaryBufferStart());
         self()->comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()->getSymbol()->getStaticSymbol()->setStaticAddress(self()->getBinaryBufferCursor());
         }

      data.cursorInstruction = data.cursorInstruction->getNext();
      }

   // Create exception table entries for outlined instructions.
   //
   if (!self()->comp()->getOption(TR_DisableOOL))
      {
      auto oiIterator = self()->getARM64OutOfLineCodeSectionList().begin();
      while (oiIterator != self()->getARM64OutOfLineCodeSectionList().end())
         {
         uint32_t startOffset = (*oiIterator)->getFirstInstruction()->getBinaryEncoding() - self()->getCodeStart();
         uint32_t endOffset   = (*oiIterator)->getAppendInstruction()->getBinaryEncoding() - self()->getCodeStart();

         TR::Block * block = (*oiIterator)->getBlock();
         bool needsETE = (*oiIterator)->getFirstInstruction()->getNode()->getOpCode().hasSymbolReference() &&
                         (*oiIterator)->getFirstInstruction()->getNode()->getSymbolReference() &&
                         (*oiIterator)->getFirstInstruction()->getNode()->getSymbolReference()->canCauseGC();

         if (needsETE && block && !block->getExceptionSuccessors().empty())
            block->addExceptionRangeForSnippet(startOffset, endOffset);

         ++oiIterator;
         }
      }

   if (!constantIsSignedImm21((intptr_t)self()->getBinaryBufferCursor() - (intptr_t)self()->getBinaryBufferStart()))
      {
      // Workaround for huge code
      // Range of conditional branch instruction is +/- 1MB
      self()->comp()->failCompilation<TR::AssertionFailure>("Generated code is too large");
      }

   self()->getLinkage()->performPostBinaryEncoding();
   }

TR::Linkage *OMR::ARM64::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage;
   switch (lc)
      {
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::ARM64SystemLinkage(self());
         break;
      default:
         TR_ASSERT(false, "using system linkage for unrecognized convention %d\n", lc);
         linkage = new (self()->trHeapMemory()) TR::ARM64SystemLinkage(self());
      }
   self()->setLinkage(lc, linkage);
   return linkage;
   }

void OMR::ARM64::CodeGenerator::emitDataSnippets()
   {
   for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
      {
      self()->setBinaryBufferCursor((*iterator)->emitSnippetBody());
      }
   }

int32_t OMR::ARM64::CodeGenerator::setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart)
   {
   // Assume constants should be aligned according to their size.
   for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
      {
      auto size = (*iterator)->getDataSize();
      estimatedSnippetStart = ((estimatedSnippetStart+size-1)/size) * size;
      (*iterator)->getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart);
      estimatedSnippetStart += (*iterator)->getLength(estimatedSnippetStart);
      }
   return estimatedSnippetStart;
   }

TR::ARM64ConstantDataSnippet *OMR::ARM64::CodeGenerator::findOrCreateConstantDataSnippet(TR::Node * node, void * c, size_t s)
   {
   // A simple linear search should suffice for now since the number of data constants
   // produced is typically very small.  Eventually, this should be implemented as an
   // ordered list or a hash table.
   for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
      {
      if ((*iterator)->getKind() == TR::Snippet::IsConstantData &&
          (*iterator)->getDataSize() == s)
         {
         // if pointers require relocation, then not all pointers may be relocated for the same reason
         //   so be conservative and do not combine them (e.g. HCR versus profiled inlined site enablement)
         if (!memcmp((*iterator)->getRawData(), c, s) &&
                (!self()->profiledPointersRequireRelocation() || (*iterator)->getNode() == node))
            {
            return (*iterator);
            }
         }
      }

   // Constant was not found: create a new snippet for it and add it to the constant list.
   //
   auto snippet = new (self()->trHeapMemory()) TR::ARM64ConstantDataSnippet(self(), node, c, s);
   _dataSnippetList.push_back(snippet);
   return snippet;
   }

TR::ARM64ConstantDataSnippet *OMR::ARM64::CodeGenerator::findOrCreate4ByteConstant(TR::Node * n, int32_t c)
   {
   return self()->findOrCreateConstantDataSnippet(n, &c, 4);
   }

TR::ARM64ConstantDataSnippet *OMR::ARM64::CodeGenerator::findOrCreate8ByteConstant(TR::Node * n, int64_t c)
   {
   return self()->findOrCreateConstantDataSnippet(n, &c, 8);
   }


void OMR::ARM64::CodeGenerator::dumpDataSnippets(TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   for (auto iterator = _dataSnippetList.begin(); iterator != _dataSnippetList.end(); ++iterator)
      {
      (*iterator)->print(outFile, self()->getDebug());
      }
   }


TR::Instruction *OMR::ARM64::CodeGenerator::generateSwitchToInterpreterPrePrologue(TR::Instruction *cursor, TR::Node *node)
   {
   TR_UNIMPLEMENTED();

   return NULL;
   }


// different from evaluate in that it returns a clobberable register
TR::Register *OMR::ARM64::CodeGenerator::gprClobberEvaluate(TR::Node *node)
   {
   TR::Register *resultReg = self()->evaluate(node);

   if (node->getReferenceCount() > 1)
      {
      TR::Register *targetReg = resultReg->containsCollectedReference() ? self()->allocateCollectedReferenceRegister() : self()->allocateRegister(resultReg->getKind());

      if (resultReg->containsInternalPointer())
         {
         targetReg->setContainsInternalPointer();
         targetReg->setPinningArrayPointer(resultReg->getPinningArrayPointer());
         }

      generateMovInstruction(self(), node, targetReg, resultReg);
      return targetReg;
      }
   else
      {
      return resultReg;
      }
   }

uint32_t
OMR::ARM64::CodeGenerator::registerBitMask(int32_t reg)
   {
   return 1 << (reg-1);
   }

void OMR::ARM64::CodeGenerator::buildRegisterMapForInstruction(TR_GCStackMap *map)
   {
   TR_InternalPointerMap * internalPtrMap = NULL;
   TR::GCStackAtlas *atlas = self()->getStackAtlas();
   //
   // Build the register map
   //
   for (int i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; ++i)
      {
      TR::RealRegister * reg = self()->machine()->getRealRegister((TR::RealRegister::RegNum)i);
      if (reg->getHasBeenAssignedInMethod())
         {
         TR::Register *virtReg = reg->getAssignedRegister();
         if (virtReg)
            {
            if (virtReg->containsInternalPointer())
               {
               if (!internalPtrMap)
                  internalPtrMap = new (self()->trHeapMemory()) TR_InternalPointerMap(self()->trMemory());
               internalPtrMap->addInternalPointerPair(virtReg->getPinningArrayPointer(), i);
               atlas->addPinningArrayPtrForInternalPtrReg(virtReg->getPinningArrayPointer());
               }
            else if (virtReg->containsCollectedReference())
               map->setRegisterBits(self()->registerBitMask(i));
            }
         }
      }

   map->setInternalPointerMap(internalPtrMap);
   }

TR_GlobalRegisterNumber OMR::ARM64::CodeGenerator::pickRegister(TR_RegisterCandidate *regCan,
                                                          TR::Block **barr,
                                                          TR_BitVector &availRegs,
                                                          TR_GlobalRegisterNumber &highRegisterNumber,
                                                          TR_LinkHead<TR_RegisterCandidate> *candidates)
   {
   return OMR::CodeGenerator::pickRegister(regCan, barr, availRegs, highRegisterNumber, candidates);
   }

bool OMR::ARM64::CodeGenerator::allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *regCan, TR::Node *branchNode)
   {
   // If return false, processLiveOnEntryBlocks has to dis-qualify any candidates which are referenced
   // within any CASE of a SWITCH statement.
   return true;
   }

int32_t OMR::ARM64::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *node)
   {
   if (node->getOpCodeValue() == TR::table)
      {
      auto numBranchTableEntries = node->getNumChildren() - 2;
      return  self()->getNumberOfGlobalGPRs() - ((numBranchTableEntries >= 5) ? 2 : 1);
      }
   else if (node->getOpCodeValue() == TR::lookup)
      {
      auto numChildren = node->getNumChildren();
      auto numTmpReg = (!constantIsUnsignedImm12(node->getChild(2)->getCaseConstant())
         || !constantIsUnsignedImm12(node->getChild(numChildren-1)->getCaseConstant())) ? 2 : 1;
      return  self()->getNumberOfGlobalGPRs() - numTmpReg;
      }
   return INT_MAX;
   }

int32_t OMR::ARM64::CodeGenerator::getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *node)
   {
   return INT_MAX;
   }

int32_t OMR::ARM64::CodeGenerator::getMaximumNumbersOfAssignableGPRs()
   {
   return TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1;
   }

int32_t OMR::ARM64::CodeGenerator::getMaximumNumbersOfAssignableFPRs()
   {
   return TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1;
   }

bool OMR::ARM64::CodeGenerator::isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt)
   {
   return self()->machine()->getRealRegister((TR::RealRegister::RegNum)self()->getGlobalRegister(i))->getState() == TR::RealRegister::Free;
   }

TR_GlobalRegisterNumber OMR::ARM64::CodeGenerator::getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type)
   {
   TR_GlobalRegisterNumber result;

   if (type == TR::Float || type == TR::Double)
      {
      if (linkageRegisterIndex >= self()->getProperties()._numFloatArgumentRegisters)
         return -1;
      else
         result = _fprLinkageGlobalRegisterNumbers[linkageRegisterIndex];
      }
   else
      {
      if (linkageRegisterIndex >= self()->getProperties()._numIntegerArgumentRegisters)
         return -1;
      else
         result = _gprLinkageGlobalRegisterNumbers[linkageRegisterIndex];
      }

   return result;
   }

void OMR::ARM64::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label)
   {
   // for "tbz/tbnz" instruction
   TR_ASSERT(label->getCodeLocation(), "Attempt to relocate to a NULL label address!");

   intptr_t distance = reinterpret_cast<intptr_t>(label->getCodeLocation() - reinterpret_cast<uint8_t *>(cursor));
   TR_ASSERT_FATAL(constantIsSignedImm16(distance), "offset (%d) is too large for imm14", distance);
   *cursor |= ((distance >> 2) & 0x3fff) << 5; // imm14
   }

void OMR::ARM64::CodeGenerator::apply24BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label)
   {
   // for "b.cond" instruction
   TR_ASSERT(label->getCodeLocation(), "Attempt to relocate to a NULL label address!");

   intptr_t distance = (uintptr_t)label->getCodeLocation() - (uintptr_t)cursor;
   *cursor |= ((distance >> 2) & 0x7ffff) << 5; // imm19
   }

void OMR::ARM64::CodeGenerator::apply32BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label)
   {
   // for unconditional "b" instruction
   TR_ASSERT(label->getCodeLocation(), "Attempt to relocate to a NULL label address!");

   intptr_t distance = (uintptr_t)label->getCodeLocation() - (uintptr_t)cursor;
   *cursor |= ((distance >> 2) & 0x3ffffff); // imm26
   }

int64_t OMR::ARM64::CodeGenerator::getLargestNegConstThatMustBeMaterialized()
   {
   TR_ASSERT(0, "Not Implemented on AArch64");
   return 0;
   }

int64_t OMR::ARM64::CodeGenerator::getSmallestPosConstThatMustBeMaterialized()
   {
   TR_ASSERT(0, "Not Implemented on AArch64");
   return 0;
   }

bool
OMR::ARM64::CodeGenerator::directCallRequiresTrampoline(intptr_t targetAddress, intptr_t sourceAddress)
   {
   return
      !self()->comp()->target().cpu.isTargetWithinUnconditionalBranchImmediateRange(targetAddress, sourceAddress) ||
      self()->comp()->getOption(TR_StressTrampolines);
   }

TR::Instruction *
OMR::ARM64::CodeGenerator::generateNop(TR::Node *node, TR::Instruction *preced)
   {
   if (preced)
      return new (self()->trHeapMemory()) TR::Instruction(TR::InstOpCode::nop, node, preced, self());
   else
      return new (self()->trHeapMemory()) TR::Instruction(TR::InstOpCode::nop, node, self());
   }

TR_ARM64OutOfLineCodeSection *
OMR::ARM64::CodeGenerator::findARM64OutOfLineCodeSectionFromLabel(TR::LabelSymbol *label)
   {
   auto oiIterator = self()->getARM64OutOfLineCodeSectionList().begin();
   while (oiIterator != self()->getARM64OutOfLineCodeSectionList().end())
      {
      if ((*oiIterator)->getEntryLabel() == label)
         return *oiIterator;
      ++oiIterator;
      }

   return NULL;
   }

TR_ARM64ScratchRegisterManager *
OMR::ARM64::CodeGenerator::generateScratchRegisterManager(int32_t capacity)
   {
   return new (self()->trHeapMemory()) TR_ARM64ScratchRegisterManager(capacity, self());
   }

void OMR::ARM64::CodeGenerator::setRealRegisterAssociation(TR::Register *virtualRegister, TR::RealRegister::RegNum realNum)
   {
   if (!virtualRegister->isLive() || realNum == TR::RealRegister::NoReg || realNum == TR::RealRegister::xzr)
      {
      return;
      }

   TR::RealRegister *realReg = self()->machine()->getRealRegister(realNum);
   self()->getLiveRegisters(virtualRegister->getKind())->setAssociation(virtualRegister, realReg);
   }


TR::Instruction *OMR::ARM64::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond)
   {
   TR::Node *node = cursor->getNode();

   if (!constantIsUnsignedImm12(delta))
      {
      TR::Register *deltaReg = self()->allocateRegister();
      cursor = loadConstant64(self(), node, delta, deltaReg, cursor);
      cursor = self()->generateDebugCounterBump(cursor, counter, deltaReg, cond);
      if (cond)
         {
         TR::addDependency(cond, deltaReg, TR::RealRegister::NoReg, TR_GPR, self());
         }
      self()->stopUsingRegister(deltaReg);
      return cursor;
      }

   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT_FATAL(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = self()->allocateRegister();
   TR::Register *counterReg = self()->allocateRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor, false, TR_DebugCounter);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::ldrimmw, node, counterReg, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), cursor);
   cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addimmw, node, counterReg, counterReg, delta, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::strimmw, node, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), counterReg, cursor);

   if (cond)
      {
      uint32_t preCondCursor = cond->getAddCursorForPre();
      uint32_t postCondCursor = cond->getAddCursorForPost();
      TR::addDependency(cond, addrReg, TR::RealRegister::NoReg, TR_GPR, self());
      TR::addDependency(cond, counterReg, TR::RealRegister::NoReg, TR_GPR, self());
      }
   self()->stopUsingRegister(addrReg);
   self()->stopUsingRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::ARM64::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond)
   {
   TR::Node *node = cursor->getNode();
   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT_FATAL(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = self()->allocateRegister();
   TR::Register *counterReg = self()->allocateRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor, false, TR_DebugCounter);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::ldrimmw, node, counterReg, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), cursor);
   cursor = generateTrg1Src2Instruction(self(), TR::InstOpCode::addw, node, counterReg, counterReg, deltaReg, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::strimmw, node, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), counterReg, cursor);

   if (cond)
      {
      uint32_t preCondCursor = cond->getAddCursorForPre();
      uint32_t postCondCursor = cond->getAddCursorForPost();
      TR::addDependency(cond, addrReg, TR::RealRegister::NoReg, TR_GPR, self());
      TR::addDependency(cond, counterReg, TR::RealRegister::NoReg, TR_GPR, self());
      }
   self()->stopUsingRegister(addrReg);
   self()->stopUsingRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::ARM64::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm)
   {
   TR::Node *node = cursor->getNode();

   if (!constantIsUnsignedImm12(delta))
      {
      TR::Register *deltaReg = srm.findOrCreateScratchRegister();
      cursor = loadConstant64(self(), node, delta, deltaReg, cursor);
      cursor = self()->generateDebugCounterBump(cursor, counter, deltaReg, srm);
      srm.reclaimScratchRegister(deltaReg);
      return cursor;
      }

   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT_FATAL(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = srm.findOrCreateScratchRegister();
   TR::Register *counterReg = srm.findOrCreateScratchRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor, false, TR_DebugCounter);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::ldrimmw, node, counterReg, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), cursor);
   cursor = generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addimmw, node, counterReg, counterReg, delta, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::strimmw, node, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), counterReg, cursor);

   srm.reclaimScratchRegister(addrReg);
   srm.reclaimScratchRegister(counterReg);
   return cursor;
   }

TR::Instruction *OMR::ARM64::CodeGenerator::generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm)
   {
   TR::Node *node = cursor->getNode();
   intptr_t addr = counter->getBumpCountAddress();

   TR_ASSERT_FATAL(addr, "Expecting a non-null debug counter address");

   TR::Register *addrReg = srm.findOrCreateScratchRegister();
   TR::Register *counterReg = srm.findOrCreateScratchRegister();

   cursor = loadAddressConstant(self(), node, addr, addrReg, cursor, false, TR_DebugCounter);
   cursor = generateTrg1MemInstruction(self(), TR::InstOpCode::ldrimmw, node, counterReg, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), cursor);
   cursor = generateTrg1Src2Instruction(self(), TR::InstOpCode::addw, node, counterReg, counterReg, deltaReg, cursor);
   cursor = generateMemSrc1Instruction(self(), TR::InstOpCode::strimmw, node, new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()), counterReg, cursor);
   srm.reclaimScratchRegister(addrReg);
   srm.reclaimScratchRegister(counterReg);
   return cursor;
   }
