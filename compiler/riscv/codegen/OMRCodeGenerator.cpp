/*******************************************************************************
 * Copyright (c) 2019, 2021 IBM Corp. and others
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
#include <riscv.h>

#include "codegen/RVInstruction.hpp"
#include "codegen/RVSystemLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterIterator.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

OMR::RV::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
      OMR::CodeGenerator(comp),
      _constantData(NULL),
      _outOfLineCodeSectionList(getTypedAllocator<TR_RVOutOfLineCodeSection*>(comp->allocator()))
   {
   }

void
OMR::RV::CodeGenerator::initialize()
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
   cg->setGlobalRegisterTable(TR::Machine::getGlobalRegisterTable());
   _numGPR = _linkageProperties->getNumAllocatableIntegerRegisters();
   _numFPR = _linkageProperties->getNumAllocatableFloatRegisters();
   cg->setLastGlobalGPR(TR::Machine::getLastGlobalGPRRegisterNumber());
   cg->setLastGlobalFPR(TR::Machine::getLastGlobalFPRRegisterNumber());

   cg->getLinkage()->initRVRealRegisterLinkage();

   _numberBytesReadInaccessible = 0;
   _numberBytesWriteInaccessible = 0;

   if (TR::Compiler->vm.hasResumableTrapHandler(comp))
      cg->setHasResumableTrapHandler();

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
   TR::RVLinkageProperties linkageProperties = cg->getProperties();
   for (i = 0; i < linkageProperties.getNumIntArgRegs(); i++)
     _gprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getIntegerArgumentRegister(i)];
   for (i = 0; i < linkageProperties.getNumFloatArgRegs(); i++)
     _fprLinkageGlobalRegisterNumbers[i] = globalRegNumbers[linkageProperties.getFloatArgumentRegister(i)];

   if (comp->getOption(TR_TraceRA))
      {
      cg->setGPRegisterIterator(new (cg->trHeapMemory()) TR::RegisterIterator(cg->machine(), TR::RealRegister::FirstGPR, TR::RealRegister::LastGPR));
      cg->setFPRegisterIterator(new (cg->trHeapMemory()) TR::RegisterIterator(cg->machine(), TR::RealRegister::FirstFPR, TR::RealRegister::LastFPR));
      }
   }

void
OMR::RV::CodeGenerator::beginInstructionSelection()
   {
   TR::Compilation *comp = self()->comp();
   TR::Node *startNode = comp->getStartTree()->getNode();
   if (comp->getMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      _returnTypeInfoInstruction = new (self()->trHeapMemory()) TR::DataInstruction(TR::InstOpCode::dd, startNode, 0, self());
      }
   else
      {
      _returnTypeInfoInstruction = NULL;
      }

   new (self()->trHeapMemory()) TR::AdminInstruction(TR::InstOpCode::proc, startNode, NULL, _returnTypeInfoInstruction, self());
   }

void
OMR::RV::CodeGenerator::endInstructionSelection()
   {
   if (_returnTypeInfoInstruction != NULL)
      {
      _returnTypeInfoInstruction->setSourceImmediate(static_cast<uint32_t>(self()->comp()->getReturnInfo()));
      }
   }

static void
expandFarConditionalBranches(TR::CodeGenerator *cg)
   {
   for (TR::Instruction *instr = cg->getFirstInstruction(); instr; instr = instr->getNext())
      {
      TR::BtypeInstruction *branch = instr->getBtypeInstruction();

      if (branch)
         {
         int32_t distance = branch->getLabelSymbol()->getEstimatedCodeLocation() - branch->getEstimatedBinaryLocation();

         if (!VALID_SBTYPE_IMM(distance))
            branch->expandIntoFarBranch();
         }
      }
   }

void
OMR::RV::CodeGenerator::doBinaryEncoding()
   {
   TR::Compilation *comp = self()->comp();
   int32_t estimate = 0;
   TR::Instruction *cursorInstruction = self()->getFirstInstruction();

   self()->getLinkage()->createPrologue(cursorInstruction);

   bool skipOneReturn = false;
   while (cursorInstruction)
      {
      if (cursorInstruction->getOpCodeValue() == TR::InstOpCode::retn)
         {
         if (skipOneReturn == false)
            {
            TR::Instruction *temp = cursorInstruction->getPrev();
            self()->getLinkage()->createEpilogue(temp);
            cursorInstruction = temp->getNext();
            skipOneReturn = true;
            }
         else
            {
            skipOneReturn = false;
            }
         }
      estimate = cursorInstruction->estimateBinaryLength(estimate);
      cursorInstruction = cursorInstruction->getNext();
      }

   estimate = self()->setEstimatedLocationsForSnippetLabels(estimate);

   // RISCV_BRANCH_REACH is defined as 8 KiB since the range is from -4KiB
   // to +4KiB, however at any point the maximum distance one can branch is
   // 4KiB (both directions). Hence `RISCV_BRANCH_REACH / 2` in the test below.
   if (estimate > RISCV_BRANCH_REACH / 2)
      expandFarConditionalBranches(self());

   self()->setEstimatedCodeLength(estimate);

   cursorInstruction = self()->getFirstInstruction();
   uint8_t *coldCode = NULL;
   uint8_t *temp = self()->allocateCodeMemory(self()->getEstimatedCodeLength(), 0, &coldCode);

   self()->setBinaryBufferStart(temp);
   self()->setBinaryBufferCursor(temp);
   self()->alignBinaryBufferCursor();

   while (cursorInstruction)
      {
      self()->setBinaryBufferCursor(cursorInstruction->generateBinaryEncoding());
      cursorInstruction = cursorInstruction->getNext();
      }
   }

TR::Linkage *OMR::RV::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage;
   switch (lc)
      {
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::RVSystemLinkage(self());
         break;
      default:
         TR_ASSERT(false, "using system linkage for unrecognized convention %d\n", lc);
         linkage = new (self()->trHeapMemory()) TR::RVSystemLinkage(self());
      }
   self()->setLinkage(lc, linkage);
   return linkage;
   }

void OMR::RV::CodeGenerator::emitDataSnippets()
   {
   TR_UNIMPLEMENTED();
   /*
    * Commented out until TR::ConstantDataSnippet is implemented
   self()->setBinaryBufferCursor(_constantData->emitSnippetBody());
    */
   }

bool OMR::RV::CodeGenerator::hasDataSnippets()
   {
   return (_constantData == NULL) ? false : true;
   }

int32_t OMR::RV::CodeGenerator::setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart)
   {
   TR_UNIMPLEMENTED();
   return 0;
   /*
    * Commented out until TR::ConstantDataSnippet is implemented
   return estimatedSnippetStart + _constantData->getLength();
    */
   }


#ifdef DEBUG
void OMR::RV::CodeGenerator::dumpDataSnippets(TR::FILE *outFile)
   {
   if (outFile == NULL)
      return;

   TR_UNIMPLEMENTED();
   /*
    * Commented out until TR::ConstantDataSnippet is implemented
   _constantData->print(outFile);
    */
   }
#endif


TR::Instruction *OMR::RV::CodeGenerator::generateSwitchToInterpreterPrePrologue(TR::Instruction *cursor, TR::Node *node)
   {
   TR_UNIMPLEMENTED();

   return NULL;
   }


// different from evaluate in that it returns a clobberable register
TR::Register *OMR::RV::CodeGenerator::gprClobberEvaluate(TR::Node *node)
   {
   if (node->getReferenceCount() > 1)
      {
      TR::Register *sourceReg = self()->evaluate(node);
      TR::Register *targetReg = self()->allocateRegister();
      generateITYPE(TR::InstOpCode::_addi, node, targetReg, sourceReg, 0, self());

      if (sourceReg->containsCollectedReference())
         {
         if (self()->comp()->getOption(TR_TraceCG))
            traceMsg(
               self()->comp(),
               "Setting containsCollectedReference on register %s\n",
               self()->getDebug()->getName(targetReg));
         targetReg->setContainsCollectedReference();
         }
      if (sourceReg->containsInternalPointer())
         {
         TR::AutomaticSymbol *pinningArrayPointer = sourceReg->getPinningArrayPointer();
         if (self()->comp()->getOption(TR_TraceCG))
            traceMsg(
               self()->comp(),
               "Setting containsInternalPointer on register %s and setting pinningArrayPointer to " TR_FMTSPC_PTR "\n",
               self()->getDebug()->getName(targetReg),
               PTR_TO_FMTSPC_PTR(pinningArrayPointer));
         targetReg->setContainsInternalPointer();
         targetReg->setPinningArrayPointer(pinningArrayPointer);
         }

      return targetReg;
      }
   else
      {
      return self()->evaluate(node);
      }
   }


void OMR::RV::CodeGenerator::buildRegisterMapForInstruction(TR_GCStackMap *map)
   {
   TR_UNIMPLEMENTED();
   }


TR_GlobalRegisterNumber OMR::RV::CodeGenerator::pickRegister(TR_RegisterCandidate *regCan,
                                                          TR::Block **barr,
                                                          TR_BitVector &availRegs,
                                                          TR_GlobalRegisterNumber &highRegisterNumber,
                                                          TR_LinkHead<TR_RegisterCandidate> *candidates)
   {
   return OMR::CodeGenerator::pickRegister(regCan, barr, availRegs, highRegisterNumber, candidates);
   }

bool OMR::RV::CodeGenerator::allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *regCan, TR::Node *branchNode)
   {
   // If return false, processLiveOnEntryBlocks has to dis-qualify any candidates which are referenced
   // within any CASE of a SWITCH statement.
   return true;
   }

int32_t OMR::RV::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *node)
   {
   TR_UNIMPLEMENTED();

   return 0;
   }

int32_t OMR::RV::CodeGenerator::getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *node)
   {
   TR_UNIMPLEMENTED();

   return 0;
   }

int32_t OMR::RV::CodeGenerator::getMaximumNumbersOfAssignableGPRs()
   {
   return TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1;
   }

int32_t OMR::RV::CodeGenerator::getMaximumNumbersOfAssignableFPRs()
   {
   return TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1;
   }

bool OMR::RV::CodeGenerator::isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt)
   {
   return self()->machine()->getRealRegister((TR::RealRegister::RegNum)self()->getGlobalRegister(i))->getState() == TR::RealRegister::Free;
   }

TR_GlobalRegisterNumber OMR::RV::CodeGenerator::getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type)
   {
   TR_UNIMPLEMENTED();

   return 0;
   }

void OMR::RV::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label)
   {
   // for "b.cond" instruction
   TR_ASSERT(label->getCodeLocation(), "Attempt to relocate to a NULL label address!");

   intptr_t distance = (uintptr_t)label->getCodeLocation() - (uintptr_t)cursor;

   TR_ASSERT(VALID_SBTYPE_IMM(distance), "Invalid Branch offset out of range");
   *cursor |= ENCODE_SBTYPE_IMM(distance);
   }

void OMR::RV::CodeGenerator::apply16BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label, int8_t d, bool isInstrOffset)
   {
   apply16BitLabelRelativeRelocation(cursor, label);
   }

void OMR::RV::CodeGenerator::apply24BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label)
   {
   TR_ASSERT(false, "Unsupported relocation");
   }

void OMR::RV::CodeGenerator::apply32BitLabelRelativeRelocation(int32_t *cursor, TR::LabelSymbol *label)
   {
   // for "b.cond" instruction
   TR_ASSERT(label->getCodeLocation(), "Attempt to relocate to a NULL label address!");

   intptr_t distance = (uintptr_t)label->getCodeLocation() - (uintptr_t)cursor;

   TR_ASSERT(VALID_UJTYPE_IMM(distance), "Invalid Branch offset out of range");
   *cursor |= ENCODE_UJTYPE_IMM(distance);
   }

int64_t OMR::RV::CodeGenerator::getLargestNegConstThatMustBeMaterialized()
   {
   TR_ASSERT(0, "Not Implemented on AArch64");
   return 0;
   }

int64_t OMR::RV::CodeGenerator::getSmallestPosConstThatMustBeMaterialized()
   {
   TR_ASSERT(0, "Not Implemented on AArch64");
   return 0;
   }

bool OMR::RV::CodeGenerator::isILOpCodeSupported(TR::ILOpCodes o)
   {
   switch(o)
      {
      case TR::a2i:
         return false;
      default:
         return OMR::CodeGenerator::isILOpCodeSupported(o);
      }
   }
