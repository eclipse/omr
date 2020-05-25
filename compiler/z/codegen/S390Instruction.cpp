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

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/S390Snippets.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#endif
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "optimizer/Structure.hpp"
#include "ras/Debug.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/EndianConversion.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"

void
TR::S390RSInstruction::generateAdditionalSourceRegisters(TR::Register * fReg, TR::Register *lReg)
   {

   int32_t firstRegNum = toRealRegister(fReg)->getRegisterNumber();
   int32_t lastRegNum = toRealRegister(lReg)->getRegisterNumber();
   if (firstRegNum != lastRegNum &&
       lastRegNum - firstRegNum > 1)
      {
      TR::Machine *machine = cg()->machine();
      int8_t numRegsToAdd = lastRegNum - firstRegNum - 1;
      // _additionalRegisters = new (cg()->trHeapMemory(),TR_MemoryBase::Array) TR_Array<TR::Register *>(cg()->trMemory(), numRegsToAdd, false);
      int8_t curReg = firstRegNum+1;
      for (int8_t i=0; i < numRegsToAdd; i++)
         {
         // (*_additionalRegisters)[i] = machine->getRealRegister(((TR::RealRegister::RegNum)curReg));
         TR::Register *temp = machine->getRealRegister(((TR::RealRegister::RegNum)curReg));
         useSourceRegister(temp);
        curReg++;
         }
      }
   }

uint8_t *
TR::S390EInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t instructionLength = getOpCode().getInstructionLength();
   getOpCode().copyBinaryToBuffer(instructionStart);

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

uint8_t *
TR::S390IEInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t instructionLength = getOpCode().getInstructionLength();
   getOpCode().copyBinaryToBuffer(instructionStart);

   *(cursor + 3) = getImmediateField1() << 4 | getImmediateField2();

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

bool isLoopEntryAlignmentEnabled(TR::Compilation *comp)
   {

   if(comp->getOption(TR_DisableLoopEntryAlignment))
      return false;
   if(comp->getOption(TR_EnableLoopEntryAlignment))
      return true;

   // Loop alignment not worth the extra bytes for <= warm compilations because if a loop at warm would have
   // exhibited benefit from alignment then the loop must have been hot enough for us to recompile the method
   // to > warm in the first place.

   if(comp->getOptLevel() <= warm)
      return false;

   if (comp->getOption(TR_EnableLabelTargetNOPs)) // only one type of NOP insertion is currently supported
      return false;

   return true;
   }

/**
 * Determines whether the given instruction should be aligned with NOPs.
 * Currently, it supports alignment of loop entry blocks.
 */
bool TR::S390LabeledInstruction::isNopCandidate()
   {
   TR::Compilation *comp = cg()->comp();
   if (!isLoopEntryAlignmentEnabled(comp))
      return false;

   bool isNopCandidate = false;

   TR::Node * node = getNode();

   // Re:  node->getLabel() == getLabelSymbol()
   // Make sure the label is the one that corresponds with BBStart
   // An example where this is not the case is labels in the pre-
   // prologue where they take on the node of the first BB.
   if (node != NULL && node->getOpCodeValue() == TR::BBStart &&
       node->getLabel() == getLabelSymbol())
      {
      TR::Block * block = node->getBlock();

      // Frequency 6 is "Don't Know".  Do not bother aligning.
      if (block->firstBlockInLoop() && !block->isCold() && block->getFrequency() > 1000)
         {
         TR_BlockStructure* blockStructure = block->getStructureOf();
         if (blockStructure)
            {
            TR_RegionStructure *region = (TR_RegionStructure*)blockStructure->getContainingLoop();
            if (region)
               {
               // make sure block == entry, so that you're doing stuff for loop entry
               //
               TR::Block *entry = region->getEntryBlock();
               assert(entry==block);
               uint32_t loopLength = 0;
               for (auto e = entry->getPredecessors().begin(); e != entry->getPredecessors().end(); ++e)
                  {
                  TR::Block *predBlock = toBlock((*e)->getFrom());
                  if (!region->contains(predBlock->getStructureOf(), region->getParent()))
                     continue;
                  // Backedge found.
                  TR::Instruction* lastInstr = predBlock->getLastInstruction();

                  // Find the branch instruction.
                  int32_t window = 4;  // limit search distance.
                  while(window >= 0 && !lastInstr->getOpCode().isBranchOp())
                     {
                     window--;
                     lastInstr = lastInstr->getPrev();
                     }

                  // Determine Length of Loop.
                  TR::Instruction * firstInstr = block->getFirstInstruction();
                  TR::Instruction * currInstr;
                  for(currInstr=firstInstr; ; currInstr=currInstr->getNext())
                     {
                     if(currInstr==NULL)
                        {
                        // We might be here if the loop structure isn't consecutive in memory
                        return false;
                        }

                     loopLength += currInstr->getOpCode().getInstructionLength();
                     if(loopLength>256)
                        return false;
                     if(currInstr==lastInstr)
                        break;
                     }

                  // Determine if the loop will cross cache line boundry
                  uint8_t * cursor = cg()->getBinaryBufferCursor();
                  if( loopLength <= 256 && (((uint64_t)cursor+loopLength)&0xffffff00) > ((uint64_t)cursor&0xffffff00) )
                     {
                     isNopCandidate = true;
                     traceMsg(comp, "Insert NOP instructions for loop alignment\n");
                     break;
                     }
                  else
                     {
                     isNopCandidate = false;
                     break;
                     }
                  }
               }
            }
         }
      }
   return isNopCandidate;
   }

////////////////////////////////////////////////////////
// TR::S390LabelInstruction:: member functions
////////////////////////////////////////////////////////

/**
 * LabelInstruction is a pseudo instruction that generates a label or constant (with a label)
 *
 * Currently, this pseudo instruction handles the following 2 scenarios:
 *
 * 1.  emit a label
 *
 * 2.  emit a snippet label
 *
 */
uint8_t *
TR::S390LabelInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();

   bool tryToUseLabelTargetNOPs = comp->getOption(TR_EnableLabelTargetNOPs);
   bool traceLabelTargetNOPs = comp->getOption(TR_TraceLabelTargetNOPs);
   intptr_t offsetForLabelTargetNOPs = 0;
   if (tryToUseLabelTargetNOPs)
      {
      tryToUseLabelTargetNOPs = considerForLabelTargetNOPs(true); // inEncodingPhase=true
      if (tryToUseLabelTargetNOPs)
         {
         offsetForLabelTargetNOPs = instructionStart-cg()->getCodeStart();
         // the offset that matters is the offset relative to the start of the object (the object start is 256 byte aligned but programs in this object may only be 8 byte aligned)
         // instructionStart-cg()->getCodeStart() is the offset just for this one routine or nested routine so have to add in the object size for any already
         // compiled separate programs (batchObjCodeSize) and any offset for any already compiled parent programs for the nested case (getCodeSize)
         size_t batchObjCodeSize = 0;
         size_t objCodeSize = 0;

         if (traceLabelTargetNOPs)
            traceMsg(comp,"\toffsetForLabelTargetNOPs = absOffset 0x%p + batchObjCodeSize 0x%p + objCodeSize 0x%p = 0x%p\n",
               offsetForLabelTargetNOPs,batchObjCodeSize,objCodeSize,offsetForLabelTargetNOPs + batchObjCodeSize + objCodeSize);

         offsetForLabelTargetNOPs = offsetForLabelTargetNOPs + batchObjCodeSize + objCodeSize;

         if (traceLabelTargetNOPs)
            {
            TR::Instruction *thisInst = this;
            TR::Instruction *prevInst = getPrev();
            traceMsg(comp,"\tTR::S390LabeledInstruction %p (%s) at cursor %p (skipThisOne=%s, offset = %p, codeStart %p)\n",
               this,thisInst->getOpCode().getMnemonicName(),instructionStart,isSkipForLabelTargetNOPs()?"true":"false",
               offsetForLabelTargetNOPs,cg()->getCodeStart());
            traceMsg(comp,"\tprev %p (%s)\n",
               prevInst,prevInst->getOpCode().getMnemonicName());
            }
         }
      }

   TR::LabelSymbol * label = getLabelSymbol();
   TR::Snippet * snippet = getCallSnippet();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   uint16_t binOpCode = *(uint16_t *) (getOpCode().getOpCodeBinaryRepresentation());

   if (getOpCode().getOpCodeValue() == TR::InstOpCode::DC)
      {
      AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, label));
      cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                             __FILE__, __LINE__, getNode());

      if (label->getCodeLocation() != NULL)
         {
         *((uintptr_t *) cursor) = boa((uintptr_t) label->getCodeLocation());
         }

      cursor += sizeof(uintptr_t);
      }
   else  // must be real LABEL instruction
      {
      // Insert Padding if necessary.
      if (_alignment != 0)
         {
         if (tryToUseLabelTargetNOPs && traceLabelTargetNOPs)
            traceMsg(comp,"force tryToUseLabelTargetNOPs to false because _alignment already needed on inst %p\n",this);
         tryToUseLabelTargetNOPs = false;

         int32_t padding = _alignment - (((uintptr_t)instructionStart) % _alignment);

         for (int i = padding / 6; i > 0; i--)
            {
            (*(uint16_t *) cursor) = 0xC004;
            cursor += sizeof(uint16_t);
            (*(uint32_t *) cursor) = 0xDEADBEEF;
            cursor += sizeof(uint32_t);
            }
         padding = padding % 6;
         for (int i = padding / 4; i > 0; i--)
            {
            (*(uint32_t *) cursor) = 0xA704BEEF;
            cursor += sizeof(uint32_t);
            }
         padding = padding % 4;
         if (padding == 2)
            {
            (*(uint16_t *) cursor) = 0x1800;
            cursor += sizeof(uint16_t);
            }
         }

      getLabelSymbol()->setCodeLocation(instructionStart);
      TR_ASSERT(getOpCode().getOpCodeValue() == TR::InstOpCode::LABEL, "LabelInstr not DC or LABEL, what is it??");
      }

   // Insert NOPs for loop alignment if possible
   uint64_t offset = 0;
   bool doUseLabelTargetNOPs = false;
   uint8_t * newInstructionStart = NULL;
   int32_t labelTargetBytesInserted = 0;
   if (tryToUseLabelTargetNOPs)
      {
      int32_t labelTargetNOPLimit = comp->getOptions()->getLabelTargetNOPLimit();
      if (isOdd(labelTargetNOPLimit))
         {
         if (traceLabelTargetNOPs)
            traceMsg(comp,"\tlabelTargetNOPLimit %d is odd reduce by 1 to %d\n",labelTargetNOPLimit,labelTargetNOPLimit-1);
         labelTargetNOPLimit--;
         }
      offset = (uint64_t)offsetForLabelTargetNOPs&(0xff);
      if (traceLabelTargetNOPs)
         traceMsg(comp,"\tcomparing offset %lld >? labelTargetNOPLimit %d (offsetForLabelTargetNOPs = %p)\n",offset,labelTargetNOPLimit,offsetForLabelTargetNOPs);
      if (offset > labelTargetNOPLimit)
         {
         doUseLabelTargetNOPs = true;
         newInstructionStart = cursor + (256 - offset);
         labelTargetBytesInserted = 256 - offset;
         if (traceLabelTargetNOPs)
            traceMsg(comp,"\tset useLabelTargetNOPs = true : offsetForLabelTargetNOPs=%p > labelTargetNOPLimit %d, offset=%lld, cursor %p -> %p, labelTargetBytesInserted=%d\n",
               offsetForLabelTargetNOPs,labelTargetNOPLimit,offset,cursor,newInstructionStart,labelTargetBytesInserted);
         }
      }
   else
      {
      offset = (uint64_t)cursor&(0xff);
      newInstructionStart = (uint8_t *) (((uintptr_t)cursor+256)/256*256);
      }

   if (offset && (doUseLabelTargetNOPs || isNopCandidate()))
      {
      if (!doUseLabelTargetNOPs)
         setEstimatedBinaryLength(getEstimatedBinaryLength()+256-offset);
      getLabelSymbol()->setCodeLocation(newInstructionStart);     // Label location need to be updated
      assert(offset%2==0);
      TR::Instruction * prevInstr = getPrev();
      TR::Instruction * instr;
      TR_Debug * debugObj = cg()->getDebug();
      // Insert a BRC instruction to skip NOP instructions if there're more than 3 NOPs to insert
      if(offset<238)
         {
         uint8_t *curBeforeJump = cursor;
         instr = generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, getNode(), getLabelSymbol(), prevInstr);
         instr->setEstimatedBinaryLength(10);
         cg()->setBinaryBufferCursor(cursor = instr->generateBinaryEncoding());
         if (debugObj)
            debugObj->addInstructionComment(instr, "Skip NOP instructions for loop alignment");
         if (doUseLabelTargetNOPs)
            {
            int32_t bytesOfJump = cursor - curBeforeJump;
            if (traceLabelTargetNOPs)
               traceMsg(comp,"\tbytesOfJump=%d : update offset %lld -> %lld\n",bytesOfJump,offset,offset+bytesOfJump);
            offset+=bytesOfJump;
            offset = (uint64_t)offset&0xff;
            }
         else
            {
            offset = (uint64_t)cursor&0xff;
            }
         prevInstr = instr;
         }
      // Insert NOP instructions until cursor is aligned
      for(; offset;)
         {
         uint8_t *curBeforeNOPs = cursor;
         if(offset<=250)
            {
            instr = new (cg()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 6, getNode(), prevInstr, cg());
            if (doUseLabelTargetNOPs && traceLabelTargetNOPs)
               traceMsg(comp,"\tgen 6 byte NOP at offset %lld\n",offset);
            instr->setEstimatedBinaryLength(6);
            }
         else if(offset<=252)
            {
            instr = new (cg()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 4, getNode(), prevInstr, cg());
            if (doUseLabelTargetNOPs && traceLabelTargetNOPs)
               traceMsg(comp,"\tgen 4 byte NOP at offset %lld\n",offset);
            instr->setEstimatedBinaryLength(4);
            }
         else if(offset<=254)
            {
            instr = new (cg()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, getNode(), prevInstr, cg());
            if (doUseLabelTargetNOPs && traceLabelTargetNOPs)
               traceMsg(comp,"\tgen 2 byte NOP at offset %lld\n",offset);
            instr->setEstimatedBinaryLength(2);
            }
         cg()->setBinaryBufferCursor(cursor = instr->generateBinaryEncoding());
         if (doUseLabelTargetNOPs)
            {
            int32_t bytesOfNOPs = cursor - curBeforeNOPs;
            if (traceLabelTargetNOPs)
               traceMsg(comp,"\tbytesOfNOPs=%d : end update offset %lld -> %lld\n",bytesOfNOPs,offset,offset+bytesOfNOPs);
            offset+=bytesOfNOPs;
            offset = (uint64_t)offset&0xff;
            }
         else
            {
            offset = (uint64_t)cursor&0xff;
            }
         prevInstr = instr;
         }
      instructionStart = cursor;
      if (doUseLabelTargetNOPs)
         {
         if (traceLabelTargetNOPs)
            traceMsg(comp,"\tfinished NOP insertion : setBinaryLength to 0, addAccumError = estBinLen %d - bytesInserted %d = %d, setBinaryEncoding to 0x%x\n",
               getEstimatedBinaryLength(),labelTargetBytesInserted,getEstimatedBinaryLength() - labelTargetBytesInserted,instructionStart);
         setBinaryLength(0);
         TR_ASSERT(getEstimatedBinaryLength() >= labelTargetBytesInserted,"label inst %p : estimatedBinaryLength %d must be >= labelTargetBytesInserted %d inserted\n",
            this,getEstimatedBinaryLength(),labelTargetBytesInserted);
         cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - labelTargetBytesInserted);
         setBinaryEncoding(instructionStart);
         return cursor;
         }
      }

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

bool
TR::S390LabelInstruction::considerForLabelTargetNOPs(bool inEncodingPhase)
   {
   TR::Compilation *comp = cg()->comp();
   TR::Instruction *thisInst = this;
   bool traceLabelTargetNOPs = comp->getOption(TR_TraceLabelTargetNOPs);
   TR::Instruction *prevInst = getPrev();
   while (prevInst && prevInst->getOpCode().getOpCodeValue() == TR::InstOpCode::ASSOCREGS)
      prevInst = prevInst->getPrev();

   bool doConsider = true;
   if (traceLabelTargetNOPs)
      traceMsg(comp,"considerForLabelTargetNOPs check during %s phase : %p (%s), prev  %p (%s)\n",
         inEncodingPhase?"encoding":"estimating",this,thisInst->getOpCode().getMnemonicName(),prevInst,prevInst->getOpCode().getMnemonicName());

   if (isSkipForLabelTargetNOPs())
      {
      if (traceLabelTargetNOPs)
         traceMsg(comp,"\t%p (%s) marked with isSkipForLabelTargetNOPs = true : set doConsider=false\n",this,thisInst->getOpCode().getMnemonicName());
      doConsider = false;
      }
   else if (prevInst && prevInst->getOpCode().getOpCodeValue() == TR::InstOpCode::BASR)
      {
      if (traceLabelTargetNOPs)
         traceMsg(comp,"\tprev %p is a BASR : set doConsider=false\n",prevInst);
      doConsider = false;
      }
   else if (inEncodingPhase && !wasEstimateDoneForLabelTargetNOPs())
      {
      // this is the case where some instructions (e.g. SCHEDON/SCHEDOFF) have been inserted between the estimate and encoding phases
      // and therefore this routine would return false during estimation and true during encoding and the estimate bump for NOPs would have been missed
      if (traceLabelTargetNOPs)
         traceMsg(comp,"\t%p (%s) marked with wasEstimateDoneForLabelTargetNOPs = false : set doConsider=false\n",this,thisInst->getOpCode().getMnemonicName());
      doConsider = false;
      }
   else
      {
      if (traceLabelTargetNOPs)
         traceMsg(comp,"\t%p (%s) may need to be aligned : set doConsider=true\n",this,thisInst->getOpCode().getMnemonicName());
      doConsider = true;
      }

   return doConsider;
   }

int32_t
TR::S390LabelInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getLabelSymbol() != NULL)
      {
      getLabelSymbol()->setEstimatedCodeLocation(currentEstimate);
      }
   TR::Compilation *comp = cg()->comp();
   uint8_t estimatedSize = 0;

   if (getOpCode().getOpCodeValue() == TR::InstOpCode::DC)
      {
      estimatedSize = sizeof(uintptr_t);
      }
   else
      {
      estimatedSize = 0;
      }

   if (comp->getOption(TR_EnableLabelTargetNOPs))
      {
      if (considerForLabelTargetNOPs(false)) // inEncodingPhase=false (in estimating phase now)
         {
         bool traceLabelTargetNOPs = comp->getOption(TR_TraceLabelTargetNOPs);
         int32_t maxNOPBump = 256 - comp->getOptions()->getLabelTargetNOPLimit();
         if (traceLabelTargetNOPs)
            traceMsg(comp,"\tmaxNOPBump = %d\n",maxNOPBump);
         if (getLabelSymbol() != NULL)
            {
            if (traceLabelTargetNOPs)
               traceMsg(comp,"\tupdate label %s (%p) estimatedCodeLocation with estimate %d + maxNOPBump %d = %d\n",
                  cg()->getDebug()->getName(getLabelSymbol()),getLabelSymbol(),currentEstimate,maxNOPBump,currentEstimate+maxNOPBump);
            getLabelSymbol()->setEstimatedCodeLocation(currentEstimate+maxNOPBump);
            }
         if (traceLabelTargetNOPs)
            traceMsg(comp,"\tupdate estimatedSize %d by maxNOPBump %d -> %d\n",estimatedSize,maxNOPBump,estimatedSize+maxNOPBump);
         estimatedSize += maxNOPBump;
         setEstimatedBinaryLength(estimatedSize);
         setEstimateDoneForLabelTargetNOPs();
         return currentEstimate + estimatedSize;
         }
      }
   else if (isLoopEntryAlignmentEnabled(comp))
      {
      // increase estimate by 256 if it's a loop alignment candidate
      TR::Node * node = getNode();
      if(node != NULL && node->getOpCodeValue() == TR::BBStart &&
         node->getLabel() == getLabelSymbol() && node->getBlock()->firstBlockInLoop() &&
         !node->getBlock()->isCold() && node->getBlock()->getFrequency() > 1000)
         {
         setEstimatedBinaryLength(estimatedSize+256);
         return currentEstimate + estimatedSize + 256;
         }
      }
   setEstimatedBinaryLength(estimatedSize);
   return currentEstimate + estimatedSize;
   }

void
TR::S390LabelInstruction::assignRegistersAndDependencies(TR_RegisterKinds kindToBeAssigned)
   {
   //
   TR::Register **_sourceReg = sourceRegBase();
   TR::Register **_targetReg = targetRegBase();
   TR::MemoryReference **_sourceMem = sourceMemBase();
   TR::MemoryReference **_targetMem = targetMemBase();
   TR::Compilation *comp = cg()->comp();

   //
   // If there are any dependency conditions on this instruction, apply them.
   // Any register or memory references must be blocked before the condition
   // is applied, then they must be subsequently unblocked.
   //
   if (getDependencyConditions())
      {
      block(_sourceReg, _sourceRegSize, _targetReg, _targetRegSize,  _targetMem, _sourceMem);
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());
      unblock(_sourceReg, _sourceRegSize, _targetReg, _targetRegSize,  _targetMem, _sourceMem);
      }

   assignOrderedRegisters(kindToBeAssigned);

   // Compute bit vector of free regs
   //  if none found, find best spill register
   assignFreeRegBitVector();

   // this is the return label from OOL
   if (getLabelSymbol()->isEndOfColdInstructionStream())
      {
      TR::Machine *machine = cg()->machine();
      if (comp->getOption(TR_TraceRA))
         traceMsg (comp,"\nOOL: taking register state snap shot\n");
      cg()->setIsOutOfLineHotPath(true);
      machine->takeRegisterStateSnapShot();
      }

   }

////////////////////////////////////////////////////////
// TR::S390BranchInstruction:: member functions
////////////////////////////////////////////////////////

void
TR::S390BranchInstruction::assignRegistersAndDependencies(TR_RegisterKinds kindToBeAssigned)
   {
   //
   // If there are any dependency conditions on this instruction, apply them.
   // Any register or memory references must be blocked before the condition
   // is applied, then they must be subsequently unblocked.
   //

   TR::Register **_sourceReg = sourceRegBase();
   TR::Register **_targetReg = targetRegBase();
   TR::MemoryReference **_sourceMem = sourceMemBase();
   TR::MemoryReference **_targetMem = targetMemBase();
   TR::Compilation *comp = cg()->comp();

   if (getDependencyConditions())
      {
      block(_sourceReg, _sourceRegSize, _targetReg, _targetRegSize, _targetMem, _sourceMem);
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());
      unblock(_sourceReg, _sourceRegSize, _targetReg, _targetRegSize,  _targetMem, _sourceMem);
      }

   assignOrderedRegisters(kindToBeAssigned);

   // Compute bit vector of free regs
   //  if none found, find best spill register
   assignFreeRegBitVector();

   cg()->freeUnlatchedRegisters();

   if (getOpCode().isBranchOp() && getLabelSymbol()->isStartOfColdInstructionStream())
      {
      // Switch to the outlined instruction stream and assign registers.
      //
      TR_S390OutOfLineCodeSection *oi = cg()->findS390OutOfLineCodeSectionFromLabel(getLabelSymbol());
      TR_ASSERT(oi, "Could not find S390OutOfLineCodeSection stream from label.  instr=%p, label=%p\n", this, getLabelSymbol());
      if (!oi->hasBeenRegisterAssigned())
         oi->assignRegisters(kindToBeAssigned);
      }
   if (getOpCode().getOpCodeValue() == TR::InstOpCode::AP /*&&     toS390SS2Instruction(this)->getLabel()*/ )
      {
      TR_S390OutOfLineCodeSection *oi = cg()->findS390OutOfLineCodeSectionFromLabel(toS390SS2Instruction(this)->getLabel());
      TR_ASSERT(oi, "Could not find S390OutOfLineCodeSection stream from label.  instr=%p, label=%p\n", this, toS390SS2Instruction(this)->getLabel());
      if (!oi->hasBeenRegisterAssigned())
      oi->assignRegisters(kindToBeAssigned);
      }
   if (getOpCode().isBranchOp() && getLabelSymbol()->isEndOfColdInstructionStream())
      {
      // This if statement prevents RA to restore register snapshot on regular branches to the
      // OOL section merging point. Register snapshot is a snapshot of register states taken at
      // OOL merge label. Using this snapshot RA can enforce the similarity of register states
      // at the end of main-stream code and OOL path.
      // Generally the safer option is to not reuse OOL merge label for any other purpose. This
      // can be done by creating an extra label right after merge point label.
      if (cg()->getIsInOOLSection())
         {
         // Branches from inside an OOL section to the merge-points are not allowed. Branches
         // in the OOL section can jump to the end of section and then only one branch (the
         // last instruction of an OOL section) jumps to the merge-point. In other words, OOL
         // section must contain exactly one exit point.
         TR_ASSERT(cg()->getAppendInstruction() == this, "OOL section must have only one branch to the merge point\n");
         // Start RA for OOL cold path, restore register state from snap shot
         TR::Machine *machine = cg()->machine();
         if (comp->getOption(TR_TraceRA))
            traceMsg (comp, "\nOOL: Restoring Register state from snap shot\n");
         cg()->setIsOutOfLineHotPath(false);
         machine->restoreRegisterStateFromSnapShot();
         }
      // Reusing the OOL Section merge label for other branches might be unsafe.
      else if(comp->getOption(TR_TraceRA))
         traceMsg (comp, "\nOOL: Reusing the OOL Section merge label for other branches might be unsafe.\n");
      }
   }

/**
 *
 * BranchInstruction is a pseudo instruction that generates instructions to branch to a symbol.
 *
 * Currently, this pseudo instruction handles the following 2 scenarios:
 *
 * 1.  emit branch instruction to a label.
 * 2.  emit branch instruction to a snippet.
 *
 *     On G5, the instruction will be a branch relative or branch on condition instruction.
 *     If the branch distance is within +/- 2**16, it will be done with a relative branch.
 *     Otherwise, it will be done with the following (horrible) sequence:
 *     BRAS r7, 4
 *     DC   branching target address
 *     L    r7,0(,r7)
 *     BCR  r7
 *     Subtle note: since 64-bit is running on a Freeway or above (by definition), slow muck
 *     will not be generated. Note this is different than BranchOnCount and BranchOnIndex
 *     since they don't have long branch equivalents for themselves.
 */
uint8_t *
TR::S390BranchInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol * label = getLabelSymbol();
   TR::Snippet * snippet = getCallSnippet();
   uint8_t * cursor = instructionStart;
   memset(static_cast<void*>(cursor), 0, getEstimatedBinaryLength());
   TR::Compilation *comp = cg()->comp();

   intptr_t distance;
   uint8_t * relocationPoint = NULL;
   bool doRelocation;
   bool shortRelocation = false;
   bool longRelocation = false;

   // msf - commented out since it does not work in cross-compile mode - TR_ASSERT(((binOpCode & 0xFF0F)== 0xA704),"Only TR::InstOpCode::BRC is handled here\n");

   if (label->getCodeLocation() != NULL)
      {
      // Label location is known
      // calculate the relative branch distance
      distance = (label->getCodeLocation() - cursor)/2;
      doRelocation = false;
      }
   else if (label->isRelativeLabel())
      {
      distance = label->getDistance();
      doRelocation = false;
      }
   else
      {
      // Label location is unknown
      // estimate the relative branch distance
      distance = (cg()->getBinaryBufferStart() + label->getEstimatedCodeLocation()) -
        (cursor + cg()->getAccumulatedInstructionLengthError());
      distance /= 2;
      doRelocation = true;
      }

   if (distance >= MIN_IMMEDIATE_VAL && distance <= MAX_IMMEDIATE_VAL)
      {
      // if distance is within 32K limit, generate short instruction
      getOpCode().copyBinaryToBuffer(cursor);
      if (getOpCode().isBranchOp() && (getBranchCondition() != TR::InstOpCode::COND_NOP))
         {
         *(cursor + 1) &= (uint8_t) 0x0F;
         *(cursor + 1) |= (uint8_t) getMask();
         }
      *(int16_t *) (cursor + 2) |= bos(distance);
      relocationPoint = cursor + 2;
      shortRelocation = true;
      cursor += getOpCode().getInstructionLength();
      }
   else
      {
      // Since N3 and up, generate BRCL instruction
      getOpCode().copyBinaryToBuffer(cursor);
      *(uint8_t *) cursor = 0xC0; // change to BRCL,keep the mask
      setOpCodeValue(TR::InstOpCode::BRCL);
      if (getOpCode().isBranchOp() && (getBranchCondition() != TR::InstOpCode::COND_NOP))
         {
         *(cursor + 1) &= (uint8_t) 0x0F;
         *(cursor + 1) |= (uint8_t) getMask();
         }
      *(int32_t *) (cursor + 2) |= boi(distance);
      longRelocation = true;
      relocationPoint = cursor;
      cursor += 6;
      }

   if (doRelocation)
      {
      if (shortRelocation)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(relocationPoint, label));
         }
      else if (longRelocation)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(relocationPoint, label));
         }
      else
         {
         AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(relocationPoint, label));
         cg()->addProjectSpecializedRelocation(relocationPoint, NULL, NULL, TR_AbsoluteMethodAddress,
                                __FILE__, __LINE__, getNode());

         }
      }

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t
TR::S390BranchInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   int32_t length = 6;
   TR::Compilation *comp = cg()->comp();

   setEstimatedBinaryLength(length);
   return currentEstimate + getEstimatedBinaryLength();
   }

////////////////////////////////////////////////////////
// TR::S390BranchOnCountInstruction:: member functions
////////////////////////////////////////////////////////

/**
 * BranchOnCountInstruction is a pseudo instruction that generates instructions to branch on count to a symbol.
 *
 * Currently, this pseudo instruction handles the following scenario:
 *
 * 1.  emit branch instruction to a label.
 *     On G5, the instruction will be a branch relative or branch on condition instruction.
 *     If the branch distance is within +/- 2**16, it will be done with a relative branch.
 *     Otherwise, it will be done with the following (horrible) sequence:
 *     BRAS r7, 4
 *     DC   branching target address
 *     L    r7,0(,r7)
 *     BCT  Rx,R7
 *     MSF:: NB::: 64-bit support needs to be added
 *
 *     For W-Code, we generate
 *     BRCT Rx,*+8
 *     BRC  0xf, *+10
 *     BCRL 0xf, label
 */
uint8_t *
TR::S390BranchOnCountInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol * label = getLabelSymbol();
   TR::Snippet * snippet = getCallSnippet();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   uint16_t binOpCode = *(uint16_t *) (getOpCode().getOpCodeBinaryRepresentation());
   TR::Compilation *comp = cg()->comp();

   intptr_t distance;

   uint8_t * relocationPoint = NULL;
   bool doRelocation;
   bool shortRelocation = false;

   if (label->getCodeLocation() != NULL)
      {
      // Label location is known
      // calculate the relative branch distance
      distance = (label->getCodeLocation() - cursor) / 2;
      doRelocation = false;
      }
   else if (label->isRelativeLabel())
      {
      distance = label->getDistance();
      doRelocation = false;
      }
   else
      {
      // Label location is unknown
      // estimate the relative branch distance
      distance = (cg()->getBinaryBufferStart() + label->getEstimatedCodeLocation()) -
         cursor -
         cg()->getAccumulatedInstructionLengthError();
      distance /= 2;
      doRelocation = true;
      }
   if ((binOpCode & 0xFF0F) == 0xCC06) //BRCTH
      {
      getOpCode().copyBinaryToBuffer(instructionStart);
      *(int32_t *) (cursor + 2) |= boi(distance);
      relocationPoint = cursor + 2;
      cursor += getOpCode().getInstructionLength();
      toRealRegister(getRegisterOperand(1))->setRegisterField((uint32_t *) instructionStart);
      if (doRelocation)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(relocationPoint, label));
         }
      }
   else
      {
      // if distance is within 32K limit on G5
      if (distance >= MIN_IMMEDIATE_VAL && distance <= MAX_IMMEDIATE_VAL)
         {
         getOpCode().copyBinaryToBuffer(instructionStart);
         *(int16_t *) (cursor + 2) |= bos(distance);
         relocationPoint = cursor + 2;
         shortRelocation = true;
         cursor += getOpCode().getInstructionLength();
         toRealRegister(getRegisterOperand(1))->setRegisterField((uint32_t *) instructionStart);
         }
      else
         {
         TR_ASSERT(((binOpCode & 0xFF0F) == 0xA706), "Only TR::InstOpCode::BRCT is handled here\n");

         TR::LabelSymbol *relLabel = TR::LabelSymbol::createRelativeLabel(cg()->trHeapMemory(),
                                                                        cg(),
                                                                        +4);
         setLabelSymbol(relLabel);

         relLabel = TR::LabelSymbol::createRelativeLabel(cg()->trHeapMemory(),
                                                        cg(),
                                                        +5);

         TR::Instruction *instr;
         instr = generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, getNode(), relLabel, this);
         instr->setEstimatedBinaryLength(4);
         instr = generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, getNode(), label, instr);
         instr->setEstimatedBinaryLength(8);
         return generateBinaryEncoding();
         }

      if (doRelocation)
         {
         if (shortRelocation)
            {
            cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(relocationPoint, label));
            }
         else
            {
            AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", relocationPoint);
            cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(relocationPoint, label));
            cg()->addProjectSpecializedRelocation(relocationPoint, NULL, NULL, TR_AbsoluteMethodAddress,
                                   __FILE__, __LINE__, getNode());
            }
         }
      }
   toRealRegister(getRegisterOperand(1))->setRegisterField((uint32_t *) instructionStart);

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t
TR::S390BranchOnCountInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   setEstimatedBinaryLength(getOpCode().getInstructionLength());
   //code could be expanded into BRAS(4)+DC(4)+L(4)+BCT(4) sequence
   setEstimatedBinaryLength(16);
   return currentEstimate + getEstimatedBinaryLength();
   }

bool
TR::S390BranchOnCountInstruction::refsRegister(TR::Register * reg)
   {
   if (matchesAnyRegister(reg, getRegisterOperand(1)))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   else
      {
      return false;
      }
   }

/**
 * BranchOnIndexInstruction is a pseudo instruction that generates instructions to branch to a symbol.
 *
 * Currently, this pseudo instruction handles the following scenario:
 *
 * 1.  emit branch instruction to a label.
 *     On G5, the instruction will be a branch relative or branch on condition instruction.
 *     If the branch distance is within +/- 2**16, it will be done with a relative branch.
 *     Otherwise, it will be done with the following (horrible) sequence:
 *     BRAS r7, 4
 *     DC   branching target address
 *     L    r7,0(,r7)
 *     BXL/BXH  rx,ry,r7
 *     MSF:: NB::: 64-bit support needs to be added -- Done: LD
 *     LD: could do better for long displacement on N3 and 64bit--TODO
 */
uint8_t *
TR::S390BranchOnIndexInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol * label = getLabelSymbol();
   TR::Snippet * snippet = getCallSnippet();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   uint16_t binOpCode = *(uint16_t *) (getOpCode().getOpCodeBinaryRepresentation());
   bool shortRelocation = false;

   intptr_t distance;
   uint8_t * relocationPoint = NULL;
   bool doRelocation;

   if (label->getCodeLocation() != NULL)
      {
      // Label location is known
      // calculate the relative branch distance
      distance = (label->getCodeLocation() - cursor) / 2;
      doRelocation = false;
      }
   else
      {
      // Label location is unknown
      // estimate the relative branch distance
      distance = (cg()->getBinaryBufferStart() + label->getEstimatedCodeLocation()) -
         cursor -
         cg()->getAccumulatedInstructionLengthError();
      distance /= 2;
      doRelocation = true;
      }

   // if distance is within 32K limit on G5
   if (distance >= MIN_IMMEDIATE_VAL && distance <= MAX_IMMEDIATE_VAL)
      {
      getOpCode().copyBinaryToBuffer(instructionStart);
      TR::Register * srcReg = getRegForBinaryEncoding(getRegisterOperand(2));

      *(int16_t *) (cursor + 2) |= bos(distance);
      relocationPoint = cursor + 2;
      shortRelocation = true;
      cursor += getOpCode().getInstructionLength();
      toRealRegister(getRegisterOperand(1))->setRegisterField((uint32_t *) instructionStart);
      toRealRegister(srcReg)->setRegister2Field((uint32_t *) instructionStart);
      }
   else
      {
      TR_ASSERT_FATAL(false, "Cannot encode branch on index instruction because distance (%d) is out of range", distance);
      }

   if (doRelocation)
      {
      if (shortRelocation)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(relocationPoint, label));
         }
      else
         {
         AOTcgDiag1(cg()->comp(), "add TR_AbsoluteMethodAddress cursor=%x\n", relocationPoint);

         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(relocationPoint, label));
         cg()->addProjectSpecializedRelocation(relocationPoint, NULL, NULL, TR_AbsoluteMethodAddress,
                                __FILE__, __LINE__, getNode());
         }
      }

   toRealRegister(getRegisterOperand(1))->setRegisterField((uint32_t *) instructionStart);

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t
TR::S390BranchOnIndexInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   setEstimatedBinaryLength(getOpCode().getInstructionLength());
   //code could be expanded into BRAS(4)+DC(4)+L(4)+BXLE(4) sequence
   //or  BRAS(4)+DC(8)+LG(6)+BXLG(6) sequence
   if (sizeof(intptr_t) == 8)
      {
      setEstimatedBinaryLength(24);
      }
   else
      {
      setEstimatedBinaryLength(16);
      }
   return currentEstimate + getEstimatedBinaryLength();
   }

bool
TR::S390BranchOnIndexInstruction::refsRegister(TR::Register * reg)
   {
   if (matchesAnyRegister(reg, getRegisterOperand(1), getRegisterOperand(2)))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390FenceInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

uint8_t *
TR::S390PseudoInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   TR::Compilation *comp = cg()->comp();

   if (_fenceNode != NULL)    // must be fence node
   {
      if (_fenceNode->getRelocationType() == TR_AbsoluteAddress)
         {
         for (int32_t i = 0; i < _fenceNode->getNumRelocations(); ++i)
            {
            *(uint8_t * *) (_fenceNode->getRelocationDestination(i)) = instructionStart;
            }
         }
      else if (_fenceNode->getRelocationType() == TR_EntryRelative32Bit)
         {
         for (int32_t i = 0; i < _fenceNode->getNumRelocations(); ++i)
            {
            *(uint32_t *) (_fenceNode->getRelocationDestination(i)) = boi(cg()->getCodeLength());
            }
         }
      else // entryrelative16bit
      {
         for (int32_t i = 0; i < _fenceNode->getNumRelocations(); ++i)
            {
            *(uint16_t *) (_fenceNode->getRelocationDestination(i)) = bos((uint16_t) cg()->getCodeLength());
            }
         }
      }
   setBinaryLength(0);

   if (_callDescLabel != NULL) // We have to emit a branch around a 8-byte aligned call descriptor.
      {
      // For zOS-31 XPLINK, if the call descriptor is too far away from the native call NOP, we have
      // to manually emit a branch around the call descriptor in the main line code.
      //    BRC  4 + Padding
      //    <Padding>
      //    DC <call Descriptor> // 8-bytes aligned.
      _padbytes = ((intptr_t)(cursor + 4) + 7) / 8 * 8 - (intptr_t)(cursor + 4);

      // BRC 4 + padding.
      *((uint32_t *) cursor) = boi(0xA7F40000 + 6 + _padbytes / 2);
      cursor += sizeof(uint32_t);

      // Add Padding to make sure Call Descriptor is aligned.
      if (_padbytes == 2)
         {
         *(int16_t *) cursor = bos(0x0000);                     // padding 2-bytes
         cursor += 2;
         }
      else if (_padbytes == 4)
         {
         *(int32_t *) cursor = boi(0x00000000);
         cursor += 4;
         }
      else if (_padbytes == 6)
         {
         *(int32_t *) cursor = boi(0x00000000);
         cursor += 4;
         *(uint16_t *) cursor = bos(0x0000);
         cursor += 2;
         }
      TR_ASSERT(((intptr_t)cursor) % 8 == 0, "Call Descriptor not aligned\n");
      // Encode the Call Descriptor
      memcpy (cursor, &_callDescValue,8);
      getCallDescLabel()->setCodeLocation(cursor);
      cursor += 8;
      setBinaryLength(cursor - instructionStart);
      }
   setBinaryEncoding(instructionStart);

   return cursor;
   }

int32_t
TR::S390PseudoInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(0);

   return currentEstimate;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390DebugCounterBumpInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

/**
 * This instruction is bumps the value at the snippet corresponding to the address of the debug counter count.
 * The valid opcode is TR::InstOpCode::DCB
 *
 * Either 2 or 4 instructions are encoded matching the following conditions:
 *
 * If a free register was saved:                      Otherwise if a spill is required:
 *                                                    STG     Rscrtch,   offToLongDispSlot(,GPR5)
 * LGRL    Rscrtch,   counterRelocation               LGRL    Rscrtch,   counterRelocation
 * AGSI 0(Rscrtch),   delta                           AGSI 0(Rscrtch),   delta
 *                                                    LG      Rscrtch,   offToLongDispSlot(,GPR5)
 *
 * If the platform is 32-bit, ASI replaces AGSI
 */
uint8_t *
TR::S390DebugCounterBumpInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   TR::Compilation *comp = cg()->comp();

   int32_t offsetToLongDispSlot = (cg()->getLinkage())->getOffsetToLongDispSlot();

   TR::RealRegister * scratchReg  = getAssignableReg();
   bool spillNeeded = true;

   // If we found a free register during RA, we don't need to spill
   if (scratchReg)
      {
      spillNeeded = false;
      }
   else
      {
	   scratchReg = assignBestSpillRegister();
      }

   TR_ASSERT(scratchReg!=NULL, "TR_S390DebugCounterBumpInstruction::generateBinaryEncoding -- A scratch reg should always be found.");

   traceMsg(comp, "[%p] DCB using %s as scratch reg with spill=%s\n", this, cg()->getDebug()->getName(scratchReg), spillNeeded ? "true" : "false");

   if (spillNeeded)
      {
      *(int32_t *) cursor  = boi(0xE3005000 | (offsetToLongDispSlot&0xFFF));              // STG Rscrtch,offToLongDispSlot(,GPR5)
      scratchReg->setRegisterField((uint32_t *)cursor);                                   //
      cursor += 4;                                                                        //
      *(int16_t *) cursor = bos(0x0024);                                                  //
      cursor += 2;                                                                        //
      }

   *(int16_t *) cursor  = bos(0xC408);                                                    // LGRL Rscrtch, counterRelocation
   scratchReg->setRegisterField((uint32_t *)cursor);                                      //
   cg()->addRelocation(new (cg()->trHeapMemory())                                         //
      TR::LabelRelative32BitRelocation(cursor, getCounterSnippet()->getSnippetLabel()));   //
   cursor += 6;                                                                           //

   *(int32_t *) cursor  = boi(0xEB000000 | (((int8_t) getDelta()) << 16));                // On 64-bit platforms
   scratchReg->setBaseRegisterField((uint32_t *)cursor);                                  // AGSI 0(Rscrtch), delta
   cursor += 4;                                                                           //
   *(int16_t *) cursor  = comp->target().is64Bit() ? bos(0x007A) : bos(0x006A);     // On 32-bit platforms
   cursor += 2;                                                                           //  ASI 0(Rscrtch), delta

   if (spillNeeded)
      {
      *(int32_t *) cursor  = boi(0xE3005000 | (offsetToLongDispSlot&0xFFF));              // LG Rscrtch,offToLongDispSlot(,GPR5)
      scratchReg->setRegisterField((uint32_t *)cursor);                                   //
      cursor += 4;                                                                        //
      *(int16_t *) cursor = bos(0x0004);                                                  //
      cursor += 2;                                                                        //
      }

   setEstimatedBinaryLength(spillNeeded ? 24 : 12);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);

   return cursor;
   }

// TR::S390ImmInstruction:: member functions
/**
 * This instruction is used to generate a constant value in JIT code
 * so the valid opcode is TR::InstOpCode::DC
 */
uint8_t *
TR::S390ImmInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   TR_ASSERT( getOpCode().getOpCodeValue() == TR::InstOpCode::DC, "ImmInstruction is for TR::InstOpCode::DC only!");
   TR::Compilation *comp = cg()->comp();

   /*
      getOpCode().copyBinaryToBuffer(instructionStart);

      (*(uint32_t*) cursor) &= 0xFFFF0000;
      (*(uint32_t*) cursor) |= getSourceImmediate();
   */
   (*(uint32_t *) cursor) = boi(getSourceImmediate());

#ifdef J9_PROJECT_SPECIFIC
   //AOT Relocation
   if (getEncodingRelocation())
      addEncodingRelocation(cg(), cursor, __FILE__, __LINE__, getNode());
#endif

   if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
      {
      // On 64-bit, the second word of the pointer is the one in getStaticPICSites.
      // We can't register the first word because the second word wouldn't yet
      // be binary-encoded by the time we need to compute the proper hash key.
      //
      void **locationToPatch = (void**)(cursor - (comp->target().is64Bit()?4:0));
      cg()->jitAddPicToPatchOnClassUnload(*locationToPatch, locationToPatch);
      }
   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
      {
      // On 64-bit, the second word of the pointer is the one in getStaticHCRPICSites.
      // We can't register the first word because the second word wouldn't yet
      // be binary-encoded by the time we need to compute the proper hash key.
      //
      void **locationToPatch = (void**)(cursor - (comp->target().is64Bit()?4:0));
      cg()->jitAddPicToPatchOnClassRedefinition(*locationToPatch, locationToPatch);
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)locationToPatch, (uint8_t *)*locationToPatch, TR_HCR, cg()), __FILE__,__LINE__, getNode());
      }

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
/**
 * The following safe virtual downcast method is only used in an assertion
 * check within "toS390ImmInstruction"
 */
TR::S390ImmInstruction *
TR::S390ImmInstruction::getS390ImmInstruction()
   {
   return this;
   }
#endif

// TR::S390ImmInstruction:: member functions
/**
 * This instruction is used to generate a constant value in JIT code
 * so the valid opcode is TR::InstOpCode::DC
 */
uint8_t *
TR::S390Imm2Instruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   TR_ASSERT( getOpCode().getOpCodeValue() == TR::InstOpCode::DC2,"ImmInstruction is for TR::InstOpCode::DC2 only!");
   TR::Compilation *comp = cg()->comp();

   (*(uint16_t *) cursor) = boi(getSourceImmediate());

#ifdef J9_PROJECT_SPECIFIC
   //AOT Relocation
   if (getEncodingRelocation())
      addEncodingRelocation(cg(), cursor, __FILE__, __LINE__, getNode());
#endif

   if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
      {
      // On 64-bit, the second word of the pointer is the one in getStaticPICSites.
      // We can't register the first word because the second word wouldn't yet
      // be binary-encoded by the time we need to compute the proper hash key.
      //
      void **locationToPatch = (void**)(cursor - (comp->target().is64Bit()?4:0));
      cg()->jitAddPicToPatchOnClassUnload(*locationToPatch, locationToPatch);
      }

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

// TR::S390ImmSnippetInstruction:: member functions
uint8_t *
TR::S390ImmSnippetInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());

   getOpCode().copyBinaryToBuffer(instructionStart);
   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

///////////////////////////////////////////////////////////
// TR::S390ImmSymInstruction:: member functions
///////////////////////////////////////////////////////////
uint8_t *
TR::S390ImmSymInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());

   getOpCode().copyBinaryToBuffer(instructionStart);

   (*(uint32_t *) cursor) |= boi(getSourceImmediate());
   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

///////////////////////////////////////////////////////
// TR::S390RegInstruction:: member functions
///////////////////////////////////////////////////////
bool
TR::S390RegInstruction::refsRegister(TR::Register * reg)
   {
   if (matchesTargetRegister(reg))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

uint8_t *
TR::S390RegInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());

   getOpCode().copyBinaryToBuffer(instructionStart);

   if (getOpCode().isBranchOp() && (getBranchCondition() != TR::InstOpCode::COND_NOP))
      {
      *(instructionStart + 1) &= (uint8_t) 0x0F;
      *(instructionStart + 1) |= (uint8_t) getMask();
      }

   TR::Register * tgtReg = getRegForBinaryEncoding(getRegisterOperand(1));
   toRealRegister(tgtReg)->setRegister2Field((uint32_t *) cursor);

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

bool
TR::S390RRInstruction::refsRegister(TR::Register * reg)
   {
   if (matchesTargetRegister(reg) || matchesAnyRegister(reg, getRegisterOperand(2)))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

uint8_t *
TR::S390RRInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t instructionLength = getOpCode().getInstructionLength();
   getOpCode().copyBinaryToBuffer(instructionStart);

   // HACK WARNING!!!!
   // Should change references to use RRE Instruction instead of RR instruction
   //
   if (instructionLength == 4)
      {
      if (getRegisterOperand(2))
         {
         TR::Register * srcReg = getRegForBinaryEncoding(getRegisterOperand(2));
         toRealRegister(srcReg)->setRegisterField((uint32_t *) cursor, 0);
         }
      TR::Register * tgtReg = getRegForBinaryEncoding(getRegisterOperand(1));
      toRealRegister(tgtReg)->setRegisterField((uint32_t *) cursor, 1);
      }
   else // RR
   {
      int32_t i=1;
      if (getFirstConstant() >=0)
         {
         (*(uint32_t *) cursor) &= boi(0xFF0FFFFF);
         (*(uint32_t *) cursor) |= boi((getFirstConstant() & 0xF)<< 20);
         }
      else
         toRealRegister(getRegForBinaryEncoding(getRegisterOperand(i++)))->setRegister1Field((uint32_t *) cursor);
      if (getSecondConstant() >= 0)
         {
         (*(uint32_t *) cursor) &= boi(0xFFF0FFFF);
         (*(uint32_t *) cursor) |= boi((getSecondConstant() & 0xF)<< 16);
         }
      else
         toRealRegister(getRegForBinaryEncoding(getRegisterOperand(i)))->setRegister2Field((uint32_t *) cursor);
   }

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  TranslateInstruction

bool
TR::S390TranslateInstruction::refsRegister(TR::Register * reg)
   {
   if (matchesAnyRegister(reg, getRegisterOperand(1)) ||
      matchesAnyRegister(reg, getTableRegister()) ||
      matchesAnyRegister(reg, getTermCharRegister()) ||
      matchesAnyRegister(reg, getRegisterOperand(2)))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

uint8_t *
TR::S390TranslateInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t instructionLength = getOpCode().getInstructionLength();
   getOpCode().copyBinaryToBuffer(instructionStart);

   toRealRegister(getRegisterOperand(2))->setRegisterField((uint32_t *) cursor, 0);
   toRealRegister(getRegForBinaryEncoding(getRegisterOperand(1)))->setRegisterField((uint32_t *) cursor, 1);

   // LL: Write mask if present
   if (isMaskPresent())
      {
      setMaskField((uint32_t *) cursor, getMask(), 3);
      }

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

// TR::S390RRFInstruction:: member functions /////////////////////////////////////////

bool
TR::S390RRFInstruction::refsRegister(TR::Register * reg)
   {
   if (matchesTargetRegister(reg) || matchesAnyRegister(reg, getRegisterOperand(2)) ||
         (_isSourceReg2Present &&  matchesAnyRegister(reg, getRegisterOperand(3))))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

uint8_t *
TR::S390RRFInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t instructionLength = getOpCode().getInstructionLength();
   getOpCode().copyBinaryToBuffer(instructionStart);
   TR::Compilation *comp = cg()->comp();

   TR::Register *srcReg = getRegisterOperand(2);
   bool isSrcPair = false, isSrc2Pair = false, isTgtPair = false;
   TR::RegisterPair* srcRegPair = srcReg != NULL ? srcReg->getRegisterPair() : NULL;
   if (srcRegPair != NULL)
     isSrcPair = true;

   TR::Register *src2Reg = NULL;
   TR::RegisterPair* src2RegPair = NULL;
   if (isSourceRegister2Present())
      {
      src2Reg = getRegisterOperand(3);
      src2RegPair = getRegisterOperand(3)->getRegisterPair();
      }
   if (src2RegPair != NULL) // handle VRFs!
     isSrc2Pair = true;

   TR::Register *tgtReg = getRegisterOperand(1);
   TR::RegisterPair* tgtRegPair = tgtReg != NULL ? tgtReg->getRegisterPair() : NULL;
   if (tgtRegPair != NULL)
     isTgtPair = true;

   switch(getKind())
      {
      // note that RRD and RRF have swapped R1 and R3 positions but in both cases R1 is the target and R3 is a source (src2 in particular)
      case TR::Instruction::IsRRD: 			// nnnn 1x32 e.g. MSDBR/MSEBR/MSER/MSDR
         if ( !isTgtPair )
            toRealRegister(tgtReg)->setRegisterField((uint32_t *) cursor, 3); // encode target in R1 position
         else
            toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 3);

         if ( !isSrcPair )
            toRealRegister(srcReg)->setRegisterField((uint32_t *) cursor, 0); // encode src in R2 position
         else
            toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 0);

         if ( !isSrc2Pair )
            toRealRegister(getRegisterOperand(3))->setRegisterField((uint32_t *) cursor, 1); // encode src2 in R3 position
         else
            toRealRegister(src2RegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 1);

         break;
      case TR::Instruction::IsRRF: 			// nnnn 3x12 e.g. IEDTR/IEXTR
         if ( !isTgtPair )
            toRealRegister(tgtReg)->setRegisterField((uint32_t *) cursor, 1); // encode target in R1 position
         else
            toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 1);

         if ( !isSrcPair )
            toRealRegister(srcReg)->setRegisterField((uint32_t *) cursor, 0); // encode src in R2 position
         else
            toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 0);

         if ( !isSrc2Pair )
            toRealRegister(getRegisterOperand(3))->setRegisterField((uint32_t *) cursor, 3); // encode src2 in R3 position
         else
            toRealRegister(src2RegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 3);

         break;
      case TR::Instruction::IsRRF2:			// M3,  ., R1, R2
         if (tgtReg != NULL)
            {
            if ( !isTgtPair )
               toRealRegister(getRegForBinaryEncoding(tgtReg))->setRegisterField((uint32_t *) cursor, 1);
            else
               toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 1);
            }

         if (srcReg != NULL)
            {
            if ( !isSrcPair )
               toRealRegister(getRegForBinaryEncoding(srcReg))->setRegisterField((uint32_t *) cursor, 0);
            else
               toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 0);
            }

         setMaskField((uint32_t *) cursor, getMask3(), 3);
         break;
      case TR::Instruction::IsRRF3:			// R3, M4, R1, R2
         if ( !isTgtPair )
            toRealRegister(tgtReg)->setRegisterField((uint32_t *) cursor, 1);
         else
            toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 1);

         if ( !isSrcPair )
            toRealRegister(srcReg)->setRegisterField((uint32_t *) cursor, 0);
         else
            toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 0);

         if ( !isSrc2Pair )
            toRealRegister(getRegisterOperand(3))->setRegisterField((uint32_t *) cursor, 3);
         else
            toRealRegister(src2RegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 3);

         setMaskField((uint32_t *) cursor, getMask4(), 2);
         break;
      case TR::Instruction::IsRRF4:			// .,  M4, R1, R2
         if ( !isTgtPair )
            toRealRegister(tgtReg)->setRegisterField((uint32_t *) cursor, 1);
         else
            toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 1);

         if ( !isSrcPair )
            toRealRegister(srcReg)->setRegisterField((uint32_t *) cursor, 0);
         else
            toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 0);

         setMaskField((uint32_t *) cursor, getMask4(), 2);
         break;
      case TR::Instruction::IsRRF5:			// .,  M4, R1, R2
         if ( !isTgtPair )
            toRealRegister(tgtReg)->setRegisterField((uint32_t *) cursor, 1);
         else
            toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 1);

         if ( !isSrcPair )
            toRealRegister(srcReg)->setRegisterField((uint32_t *) cursor, 0);
         else
            toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 0);

         setMaskField((uint32_t *) cursor, getMask4(), 2);
         setMaskField((uint32_t *) cursor, getMask3(), 3);
         break;
      default:
         TR_ASSERT( 0,"Unsupported RRF format!");
      }

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

// TR::S390RRRInstruction:: member functions /////////////////////////////////////////

bool
TR::S390RRRInstruction::refsRegister(TR::Register * reg)
   {
   if (matchesTargetRegister(reg) || matchesAnyRegister(reg, getRegisterOperand(2)) ||
       (matchesAnyRegister(reg, getRegisterOperand(3))))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

uint8_t *
TR::S390RRRInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t instructionLength = getOpCode().getInstructionLength();
   getOpCode().copyBinaryToBuffer(instructionStart);
   TR::Compilation *comp = cg()->comp();

   TR::RegisterPair* srcRegPair = getRegisterOperand(2)->getRegisterPair();
   TR::RegisterPair* src2RegPair = getRegisterOperand(3)->getRegisterPair();
   TR::RegisterPair* tgtRegPair = getRegisterOperand(1)->getRegisterPair();
   bool isTgtFPPair = false;

   if (!isTgtFPPair)
      {
      toRealRegister(getRegisterOperand(2))->setRegisterField((uint32_t *) cursor, 0);
      toRealRegister(getRegisterOperand(1))->setRegisterField((uint32_t *) cursor, 1);
      toRealRegister(getRegisterOperand(3))->setRegisterField((uint32_t *) cursor, 3);
      }
   else
      {
      toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 0);
      toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 1);
      toRealRegister(src2RegPair->getHighOrder())->setRegisterField((uint32_t *) cursor, 3);
      }

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

////////////////////////////////////////////////////////////////////////////
// TR::S390RIInstruction:: member functions
////////////////////////////////////////////////////////////////////////////

/**
 *      RI Format
 *    ________ ____ ____ _________________
 *   |Op Code | R1 |OpCd|       I2        |
 *   |________|____|____|_________________|
 *   0         8   12   16               31
 *
 */
uint8_t *
TR::S390RIInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());

   getOpCode().copyBinaryToBuffer(instructionStart);

   if (getRegisterOperand(1) != NULL)
      toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);

   // it is either RI or RIL
   if (getOpCode().getInstructionLength() == 4)
      {
      (*(int16_t *) (cursor + 2)) |= bos((int16_t) getSourceImmediate());
      }
   else
      {
      //TODO: to remove this code when it's sure there is
      // no RIL instructions going through this path
      TR_ASSERT( 0,"Do not use RI to generate RIL instructions");
      (*(int32_t *) (cursor + 2)) |= boi((int32_t) getSourceImmediate());
      }

   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);

   setBinaryEncoding(instructionStart);

   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

////////////////////////////////////////////////////////////////////////////
// TR::S390RILInstruction:: member functions
////////////////////////////////////////////////////////////////////////////

/**
 *      RIL Format
 *    ________ ____ ____ ______________________________
 *   |Op Code | R1 |OpCd|             I2               |
 *   |________|____|____|______________________________|
 *   0         8   12   16                            47
 */
bool
TR::S390RILInstruction::refsRegister(TR::Register * reg)
   {
   if (reg == getRegisterOperand(1))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

int32_t
TR::S390RILInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   int32_t delta = 0;
   TR::Compilation *comp = cg()->comp();

   if (comp->target().is64Bit()                        &&
        (getOpCode().getOpCodeValue() == TR::InstOpCode::LGRL  ||
         getOpCode().getOpCodeValue() == TR::InstOpCode::LGFRL ||
         getOpCode().getOpCodeValue() == TR::InstOpCode::LLGFRL
        )
      )
      {
      //  We could end up generating
      //  LLIHF Rscrtch, (addr&0xFFFFFFFF00000000)>>32
      //  IILF  Rscrtch, (addr&0xFFFFFFFF)
      //  LG    Rtgt   , 0(,Rscrtch)
      //
      delta = 12;
      }
   else if (comp->target().is64Bit()                        &&
        getOpCode().getOpCodeValue() == TR::InstOpCode::STGRL
        )
      {
      //  We could end up generating
      //  LLIHF Rscrtch, (addr&0xFFFFFFFF00000000)>>32
      //  IILF  Rscrtch, (addr&0xFFFFFFFF)
      //  STG   Rscrtch, SPILLSLOT(,GPR5)
      //  STG   Rtgt   , 0(,Rscrtch)
      //  LG    Rscrtch, SPILLSLOT(,GPR5)
      //
      delta = 24;
      }
   else if (comp->target().is64Bit()                        &&
            (getOpCode().getOpCodeValue() == TR::InstOpCode::STRL ||
             getOpCode().getOpCodeValue() == TR::InstOpCode::LRL))
      {
      //  We could end up generating
      //  STG   Rscrtch, SPILLSLOT(,GPR5)
      //  LLIHF Rscrtch, (addr&0xFFFFFFFF00000000)>>32
      //  IILF  Rscrtch, (addr&0xFFFFFFFF)
      //  ST    Rtgt   , 0(,Rscrtch) / L    Rtgt   , 0(,Rscrtch)
      //  LG    Rscrtch, SPILLSLOT(,GPR5)
      //
      delta = 22;
      }
   else if (getOpCode().getOpCodeValue() == TR::InstOpCode::LARL || getOpCode().getOpCodeValue() == TR::InstOpCode::LRL)
      {
      //code could have 2 byte padding
      // LARL could also have 8 more bytes for patching if target becomes unaddressable.
      delta = 10;
      }
   else
      {
      delta = 2;
      }

   setEstimatedBinaryLength(getOpCode().getInstructionLength()+delta);

   return currentEstimate + getEstimatedBinaryLength();
   }

/**
 * Compute Target Offset
 * This method calculates the target address by computing the offset in halfwords. This value will
 * be used by binary encoding to set the correct target. Example of instruction that uses this are
 * BRCL and BRASL instruction. Usually its used for getting helper method's target addresses.
 *
 * @param offset original value of offset in halfwords.
 * @param currInst current instruction address.
 *
 * @return offset to target address in half words
 */
int32_t
TR::S390RILInstruction::adjustCallOffsetWithTrampoline(int32_t offset, uint8_t * currentInst)
   {
   int32_t offsetHalfWords = offset;

   // Check to make sure that we can reach our target!  Otherwise, we need to look up appropriate
   // trampoline and branch through the trampoline.

   if (cg()->directCallRequiresTrampoline(getTargetPtr(), (intptr_t)currentInst))
      {
      intptr_t targetAddr;

#if defined(CODE_CACHE_TRAMPOLINE_DEBUG)
      printf("Target: %p,  Cursor: %p, Our Reference # is: %d\n",getTargetPtr(),(uintptr_t)currentInst,getSymbolReference()->getReferenceNumber());
#endif
      if (getSymbolReference()->getReferenceNumber() < TR_S390numRuntimeHelpers)
         targetAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(getSymbolReference()->getReferenceNumber(), (void *)currentInst);
      else
         targetAddr = cg()->fe()->methodTrampolineLookup(cg()->comp(), getSymbolReference(), (void *)currentInst);

      TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinBranchRelativeRILRange(targetAddr, (intptr_t)currentInst),
                      "Local trampoline must be directly reachable.");

      offsetHalfWords = (int32_t)((targetAddr - (uintptr_t)currentInst) / 2);
      }

   return offsetHalfWords;
   }

uint8_t *
TR::S390RILInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t i2;
   bool spillNeeded = false;
   TR::Compilation *comp = cg()->comp();
   int32_t offsetToLongDispSlot = (cg()->getLinkage())->getOffsetToLongDispSlot();

   if (isLiteralPoolAddress())
      {
      setTargetSnippet(cg()->getFirstSnippet());
      }

   if (getTargetSnippet() &&
      getTargetSnippet()->getKind() == TR::Snippet::IsConstantData)
      {
      // Using RIL to get to a literal pool entry
      //
      getOpCode().copyBinaryToBuffer(instructionStart);
      toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetSnippet()->getSnippetLabel()));
      cursor += getOpCode().getInstructionLength();
      }
   else if (getTargetSnippet() &&
           getTargetSnippet()->getKind() == TR::Snippet::IsWritableData)
      {
      // Using RIL to get to a literal pool entry
      //
      getOpCode().copyBinaryToBuffer(instructionStart);
      toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetSnippet()->getSnippetLabel()));
      cursor += getOpCode().getInstructionLength();
      }
   else if (getOpCode().getOpCodeValue() == TR::InstOpCode::LRL   ||
       getOpCode().getOpCodeValue() == TR::InstOpCode::LGRL  ||
       getOpCode().getOpCodeValue() == TR::InstOpCode::LGFRL ||
       getOpCode().getOpCodeValue() == TR::InstOpCode::LLGFRL ||
       getOpCode().getOpCodeValue() == TR::InstOpCode::STRL  ||
       getOpCode().getOpCodeValue() == TR::InstOpCode::STGRL
      )
      {
         {
      //  Using RIL to get to a Static
      //
      uintptr_t addr = getTargetPtr();

      i2 = (int32_t)((addr - (uintptr_t)cursor) / 2);

      if (cg()->comp()->target().cpu.isTargetWithinBranchRelativeRILRange((intptr_t)getTargetPtr(), (intptr_t)cursor))
         {
         getOpCode().copyBinaryToBuffer(instructionStart);
         toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);
         (*(int32_t *) (cursor + 2)) |= boi(i2);
         cursor += getOpCode().getInstructionLength();
         }
      else // We need to do things the old fashioned way
         {
         TR_ASSERT( cg()->comp()->target().is64Bit() ,"We should only be here on 64-bit platforms\n");

         TR::RealRegister * scratchReg = NULL;
         TR::RealRegister * sourceReg  = NULL;

         if (getOpCode().getOpCodeValue() == TR::InstOpCode::LGRL  ||
             getOpCode().getOpCodeValue() == TR::InstOpCode::LGFRL  ||
             getOpCode().getOpCodeValue() == TR::InstOpCode::LLGFRL )
            {
            sourceReg = (TR::RealRegister * )((TR::S390RegInstruction *)this)->getRegisterOperand(1);
            if (!scratchReg) scratchReg = sourceReg;
            }
         else if (getOpCode().getOpCodeValue() == TR::InstOpCode::STGRL ||
                  getOpCode().getOpCodeValue() == TR::InstOpCode::STRL ||
                  getOpCode().getOpCodeValue() == TR::InstOpCode::LRL)
            {
            sourceReg = (TR::RealRegister * )((TR::S390RegInstruction *)this)->getRegisterOperand(1);
            if (!scratchReg) scratchReg  = assignBestSpillRegister();

            spillNeeded = true;
            }
         else
            {
            TR_ASSERT( 0, "Unsupported opcode\n");
            }

         // Spill the scratch register
         //
         if (spillNeeded)
            {
            *(int32_t *) cursor  = boi(0xE3005000 | (offsetToLongDispSlot&0xFFF)); // STG scratchReg, offToLongDispSlot(,GPR5)
            scratchReg->setRegisterField((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor = bos(0x0024);
            cursor += 2;
            }

         //  Get the target address into scratch register
         //
         if (((uint64_t)addr)>>32)
            {
            // LLIHF scratchReg, <addr&0xFFFFFFFF00000000>
            //
            *(int32_t *) cursor  = boi(0xC00E0000 | (uint32_t)(0x0000FFFF&(((uint64_t)addr)>>48)));
            scratchReg->setRegisterField((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor  = bos(0xFFFF&(uint16_t)(((uint64_t)addr)>>32));
            cursor += 2;

            if (addr&0xFFFFFFFF)  //  LLIHF does zero out low word... so only do IILF if non-NULL
               {
               // IILF scratchReg, <addr&0x00000000FFFFFFFF>
               //
               *(int32_t *) cursor  = boi(0xC0090000 | (0xFFFF&((uint32_t)addr>>16)));
               scratchReg->setRegisterField((uint32_t *)cursor);
               cursor += 4;
               *(int16_t *) cursor  = bos(addr&0xFFFF);
               cursor += 2;
               }
            }
         else // Note, we must handle NULL correctly
            {
            // LLILF scratchReg, <addr&0x00000000FFFFFFFF>
            //
            *(int32_t *) cursor  = boi(0xC00F0000 | (0xFFFF&((uint32_t)addr>>16)));
            scratchReg->setRegisterField((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor  = bos(addr&0xFFFF);
            cursor += 2;
            }

         if (getOpCode().getOpCodeValue() == TR::InstOpCode::LGRL)
            {
            // LG sourceReg, 0(,scratchReg)
            //
            *(int32_t *) cursor  = boi(0xE3000000);                  // LG  sourceReg, 0(,scratchReg)
            sourceReg->setRegisterField((uint32_t *)cursor);
            scratchReg->setBaseRegisterField((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor  = bos(0x0004);
            cursor += 2;
            }
         else if (getOpCode().getOpCodeValue() == TR::InstOpCode::LGFRL)
            {
            // LGF sourceReg, 0(,scratchReg)
            //
            *(int32_t *) cursor  = boi(0xE3000000);                  // LGF sourceReg, 0(,scratchReg)
            sourceReg->setRegisterField((uint32_t *)cursor);
            scratchReg->setRegister2Field((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor  = bos(0x0014);
            cursor += 2;
            }
         else if (getOpCode().getOpCodeValue() == TR::InstOpCode::LLGFRL)
            {
            // LLGF sourceReg, 0(,scratchReg)
            //
            *(int32_t *) cursor  = boi(0xE3000000);                  // LLGF sourceReg, 0(,scratchReg)
            sourceReg->setRegisterField((uint32_t *)cursor);
            scratchReg->setRegister2Field((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor  = bos(0x0016);
            cursor += 2;
            }
         else if (getOpCode().getOpCodeValue() == TR::InstOpCode::LRL)
            {
            // L sourceReg, 0(,scratchReg)
            //
            *(int32_t *) cursor  = boi(0x58000000);                  // L sourceReg, 0(,scratchReg)
            sourceReg->setRegisterField((uint32_t *)cursor);
            scratchReg->setRegister2Field((uint32_t *)cursor);
            cursor += 4;
            }
         else if (getOpCode().getOpCodeValue() == TR::InstOpCode::STGRL)
            {
            // STGRL sourceReg, 0(,scratchReg)
            //
            *(int32_t *) cursor  = boi(0xE3000000);                  // STG sourceReg, 0(,scratchReg)
            sourceReg->setRegisterField((uint32_t *)cursor);
            scratchReg->setRegister2Field((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor  = bos(0x0024);
            cursor += 2;
            }
         else if (getOpCode().getOpCodeValue() == TR::InstOpCode::STRL)
            {
            // ST sourceReg, 0(,scratchReg)
            //
            *(int32_t *) cursor  = boi(0x50000000);                  // ST sourceReg, 0(,scratchReg)
            sourceReg->setRegisterField((uint32_t *)cursor);
            scratchReg->setRegister2Field((uint32_t *)cursor);
            cursor += 4;
            }
         else
            {
            TR_ASSERT( 0,"Unrecognized opcode\n");
            }

         // Unspill the scratch register
         //
         if (spillNeeded)
            {
            *(int32_t *) cursor  = boi(0xE3005000 | (offsetToLongDispSlot&0xFFF)); // LG scratchReg, offToLongDispSlot(,GPR5)
            scratchReg->setRegisterField((uint32_t *)cursor);
            cursor += 4;
            *(int16_t *) cursor = bos(0x0004);
            cursor += 2;
            }

         }
         }
      }
   else if (getOpCode().getOpCodeValue() == TR::InstOpCode::EXRL)
      {
      i2 = (int32_t)((getTargetPtr() - (uintptr_t)cursor) / 2);

      if (isImmediateOffsetInBytes()) i2 = (int32_t)(getImmediateOffsetInBytes() / 2);

      getOpCode().copyBinaryToBuffer(instructionStart);
      (*(uint32_t *) cursor) &= boi(0xFFFF0000);
      toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);

      if (getTargetSnippet() != NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetSnippet()->getSnippetLabel()));
         }
      else if (getTargetLabel() != NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetLabel()));
         }

      cursor += getOpCode().getInstructionLength();
      }
   else if (getOpCode().getOpCodeValue() == TR::InstOpCode::LARL)
      {
      // Check if this LARL is loading unloadable constant
      TR_OpaqueClassBlock * unloadableClass = NULL;
      bool doRegisterPIC = false;
      if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
         {
         unloadableClass = (TR_OpaqueClassBlock *) getTargetPtr();
         doRegisterPIC = true;
         }
      else if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), this) != comp->getStaticMethodPICSites()->end())
         {
         unloadableClass = (TR_OpaqueClassBlock *) cg()->fe()->createResolvedMethod(cg()->trMemory(),
               (TR_OpaqueMethodBlock *) getTargetPtr(), comp->getCurrentMethod())->classOfMethod();
         doRegisterPIC = true;
         }

         if (doRegisterPIC &&
               !TR::Compiler->cls.sameClassLoaders(comp, unloadableClass, comp->getCurrentMethod()->classOfMethod()))
            {
            cg()->jitAdd32BitPicToPatchOnClassUnload((void *) unloadableClass, (void *) (uintptr_t *) (cursor+2));

               // register 32 bit patchable immediate part of a LARL instruction
            }

      TR::Symbol * sym = (getNode() && getNode()->getOpCode().hasSymbolReference())?getNode()->getSymbol():NULL;
      if (!(sym && sym->isStartPC()) && getTargetSymbol() && getTargetSymbol()->isMethod())
         {
         setImmediateOffsetInBytes((uint8_t *) getSymbolReference()->getMethodAddress() - cursor);
         }

      if (sym && sym->isStartPC())
         setImmediateOffsetInBytes((uint8_t *) sym->getStaticSymbol()->getStaticAddress() - cursor);

      i2 = (int32_t)((getTargetPtr() - (uintptr_t)cursor) / 2);

      if (isImmediateOffsetInBytes()) i2 = (int32_t)(getImmediateOffsetInBytes() / 2);

      getOpCode().copyBinaryToBuffer(instructionStart);
      (*(uint32_t *) cursor) &= boi(0xFFFF0000);
      toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);
      if (getTargetLabel() != NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetLabel()));
         }
      else
         {
         if (getTargetSnippet() != NULL)
            {
            cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetSnippet()->getSnippetLabel()));
            }
         else
            {
            bool isHelper = getSymbolReference() && getSymbolReference()->getSymbol()->isMethod() &&
               getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper();
#if defined(TR_TARGET_64BIT)
            // Check to make sure that we can reach our target!  Otherwise, we
            // need to look up appropriate trampoline and branch through the
            // trampoline.
            if (!isImmediateOffsetInBytes() && !cg()->comp()->target().cpu.isTargetWithinBranchRelativeRILRange((intptr_t)getTargetPtr(), (intptr_t)cursor))
               {
               intptr_t targetAddr = ((intptr_t)(cursor) + ((intptr_t)(i2) * 2));
               TR_ASSERT( targetAddr != getTargetPtr(), "LARL is correct already!\n");
               // lower 32 bits should be correct.
               TR_ASSERT( (int32_t)(targetAddr) == (int32_t)(getTargetPtr()), "LARL lower 32-bits is incorrect!\n");
               (*(int32_t *) (cursor + 2)) |= boi(i2);
               cursor += getOpCode().getInstructionLength();

               // Check upper 16-bits to see if we need to fix it with IIHH
               if (targetAddr >> 48 != getTargetPtr() >> 48)
                  {
                  (*(uint16_t *) cursor) = bos(0xA5F0);
                  toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);
                  (*(int16_t *) (cursor + 2)) |= bos((int16_t)(getTargetPtr() >> 48));
                  cursor += 4;
                  }

               // Check upper lower 16-bits to see if we need to fix it with IIHL
               if ((targetAddr << 16) >> 48 != (getTargetPtr() << 16) >> 48)
                  {
                  (*(uint16_t *) cursor) = bos(0xA5F1);
                  toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);
                  (*(int16_t *) (cursor + 2)) |= bos((int16_t) ((getTargetPtr() << 16) >> 48));
                  cursor += 4;
                  }
               cursor -= getOpCode().getInstructionLength();
               }
            else
               {
               (*(int32_t *) (cursor + 2)) |= boi(i2);
               }

#else
            (*(int32_t *) (cursor + 2)) |= boi(i2);
#endif
            if (isHelper)
               {
               AOTcgDiag2(comp, "add TR_HelperAddress cursor=%p i2=%p\n", cursor, i2);
               cg()->addProjectSpecializedRelocation(cursor+2, (uint8_t*) getSymbolReference(), NULL, TR_HelperAddress,
                                         __FILE__, __LINE__, getNode());
               }
            }
         }
      cursor += getOpCode().getInstructionLength();
      }
   else if (getOpCode().getOpCodeValue() == TR::InstOpCode::BRCL)
      {
      // If it is a branch to unresolvedData snippet, we need to align the immediate field so that it can be
      // patched atomically using ST
      if (getTargetSnippet() != NULL && (getTargetSnippet()->getKind() == TR::Snippet::IsUnresolvedData))
         {
         // address must be 4 byte aligned for atomic patching
         int32_t padSize = 4 - ((uintptr_t) (cursor + 2) % 4);
         if (padSize == 2)
            {
            (*(uint16_t *) cursor) = bos(0x1800);
            cursor += 2;
            }
         }

      (*(uint16_t *) cursor) = bos(0xC0F4);
      if (getMask() <= 0x000000f)
         {
         (*(uint16_t *) cursor) &= bos(0xFF0F);
         (*(uint16_t *) cursor) |= bos(getMask() << 4);
         }

      if (getTargetSnippet() != NULL)
         {
         // delegate to targetSnippet Label to patch the imm. operand
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetSnippet()->getSnippetLabel()));
         }
      else
         {
         i2 = (int32_t)((getTargetPtr() - (uintptr_t)cursor) / 2);

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
        if (comp->getOption(TR_EnableRMODE64))
#endif
            {
            // get the correct target addr for helpers
            i2 = adjustCallOffsetWithTrampoline(i2, cursor);
            }
#endif

         (*(int32_t *) (cursor + 2)) = boi(i2);
         }

      cursor += getOpCode().getInstructionLength();
      }
   else if (getOpCode().getOpCodeValue() == TR::InstOpCode::BRASL)
      {
      // When a method is recompiled the entry point of the old implementation gets patched with a branch
      // to the preprologue. The callee is then responsible for patching the address of the callers BRASL
      // to point to the new implementation, and subsequently branch to the new location. Because this
      // patching happens at runtime we must ensure that the store to the relative immediate offset in the
      // BRASL is done atomically. On zOS we allow atomic patching on both a 4 and an 8 byte boundary. On
      // zLinux 64 bit we only allow atomic patching on a 4 byte boundary as we use Multi-Code Caches and
      // require trampolines.

#if defined(J9ZOS390) || !defined(TR_TARGET_64BIT)
      // Address must not cross an 8 byte boundary for atomic patching
      int32_t padSize = ((uintptr_t) (cursor + 4) % 8) == 0 ? 2 : 0;
#else
      // Address must be 4 byte aligned for atomic patching
      int32_t padSize = 4 - ((uintptr_t) (cursor + 2) % 4);
#endif

      if (padSize == 2)
         {
         (*(uint16_t *) cursor) = bos(0x1800);
         cursor += 2;
         }
#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
      if (comp->getOption(TR_EnableRMODE64))
#endif
         {
         if (cg()->hasCodeCacheSwitched())
            {
            cg()->redoTrampolineReservationIfNecessary(this, getSymbolReference());
            }
         }
#endif

      (*(uint16_t *) cursor) = bos(0xC005);
      toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);

      // delegate to targetSnippet Label to patch the imm. operand
      if (getTargetSnippet() != NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getTargetSnippet()->getSnippetLabel()));
         }
      else
         {
         /**
          * Three possible scenarios:
          * 1- Recursive call: Address is calculated by getCodeStart. MethodAddress has not set yet
          * 2- 'targetAddress' is provided: We use provided address to generate the brasl address
          * 3- 'targetAddress' is not provided: In this case the address is extracted from TargetSymbol.
          *     If targetAddress is not provided, TargetSymbol should be a method symbol.
          */
         TR_ASSERT(getTargetPtr() || (getTargetSymbol() && getTargetSymbol()->isMethod()),"targetAddress or method symbol should be provided for BRASL\n");

         if (comp->isRecursiveMethodTarget(getTargetSymbol()))
            {
            // call myself case
            uint8_t * jitTojitStart = cg()->getCodeStart();

            // Calculate jit-to-jit entry point
            jitTojitStart += ((*(int32_t *) (jitTojitStart - 4)) >> 16) & 0x0000ffff;
            *(int32_t *) (cursor + 2) = boi(((intptr_t) jitTojitStart - (intptr_t) cursor) / 2);
            }
         else
            {
            TR::MethodSymbol *callSymbol = NULL;
            if (getTargetSymbol())
               {
               callSymbol = getTargetSymbol()->isMethod() ? getTargetSymbol()->castToMethodSymbol() : NULL;
               }

            i2 = (int32_t)((getTargetPtr() - (uintptr_t)cursor) / 2);

            if (getTargetPtr() == 0 && callSymbol)
               {
               i2 = (int32_t)(((uintptr_t)(callSymbol->getMethodAddress()) - (uintptr_t)cursor) / 2);
               }
#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
            if (comp->getOption(TR_EnableRMODE64))
#endif
               {
               i2 = adjustCallOffsetWithTrampoline(i2, cursor);
               }
#endif
            (*(int32_t *) (cursor + 2)) = boi(i2);
            TR::Symbol *sym = getSymbolReference() ? getSymbolReference()->getSymbol() : NULL;
            TR::ResolvedMethodSymbol *resolvedMethodSym = sym ? sym->getResolvedMethodSymbol() : NULL;
            TR_ResolvedMethod *resolvedMethod = resolvedMethodSym ? resolvedMethodSym->getResolvedMethod() : NULL;
            if (sym && sym->castToMethodSymbol()->isHelper())
               {
               AOTcgDiag1(comp, "add TR_HelperAddress cursor=%x\n", cursor);
               cg()->addProjectSpecializedRelocation(cursor+2, (uint8_t*) getSymbolReference(), NULL, TR_HelperAddress,
                     __FILE__, __LINE__, getNode());
               }
            else if (resolvedMethod)
               {
               cg()->addProjectSpecializedRelocation(cursor+2, (uint8_t *)getSymbolReference()->getMethodAddress(), NULL, TR_MethodCallAddress,
                     __FILE__, __LINE__, getNode());
               }
            }
         }
      cursor += getOpCode().getInstructionLength();
      }
   else if (getOpCode().isExtendedImmediate())
      {
      getOpCode().copyBinaryToBuffer(instructionStart);
      toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);

      // LL: Verify extended immediate instruction length must be 6
      TR_ASSERT( getOpCode().getInstructionLength()==6, "Extended immediate instruction must be length of 6\n");
      (*(int32_t *) (cursor + 2)) |= boi((int32_t) getSourceImmediate());

      cursor += getOpCode().getInstructionLength();
      }
   else
      {
      TR_ASSERT(0, "OpCode:%d is not handled in RILInstruction yet", getOpCode().getOpCodeValue());
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

/**
 *    RS Format
 *     ________ ____ ____ ____ ____________
 *    |Op Code | R1 | R3 | B2 |     D2     |
 *    |________|____|____|____|____________|
 *    0         8   12   16   20          31
 *     ________ ____ ____ ____ ____________
 *    |Op Code | R1 | M3 | B2 |     D2     |
 *    |________|____|____|____|____________|
 *    0         8   12   16   20          31
 *
 *    There are three different uses for the RS type:
 *    1. targetReg => (R1,R3), MemRef    => (B2,D2)                                   ... (STM, LM)
 *
 *    2. targetReg => R1,      MemRef    => (B2,D2),                 MaskImm => M3    ... (ICM)
 *
 *    3. targetReg => R1,      MemRef    => (B2,D2), secondReg => R3                  ... (SLLG,...)
 *       targetReg => R1,                          , secondReg => R3 sourceImm => D2  ... (SLLG,...)
 *       targetReg => R1,      MemRef    => (B2,D2)                                   ... (SLL,...)
 *       targetReg => R1,                                            sourceImm => D2  ... (SLL,...)
 */
int32_t
TR::S390RSInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390RSInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   if (getMemoryReference() != NULL)
      {
      padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
      // Opcode could have been changed by memref for long disp RS => RSY
      // It is also possible that we inserted ext code for large DISP field.
      cursor += padding;
      }
   else
      {
      (*(int16_t *) (cursor + 2)) |= bos(getSourceImmediate());
      }

   if (getMaskImmediate())
      {
      (*(int8_t *) (cursor + 1))  |= getMaskImmediate();
      }

   TR::InstOpCode& opCode = getOpCode();

   if (getMemoryReference() != NULL)
      {
      if (getMemoryReference()->isLongDisplacementRequired())
         {
         auto longDisplacementMnemonic = TR::InstOpCode::getEquivalentLongDisplacementMnemonic(getOpCodeValue());

         if (longDisplacementMnemonic != TR::InstOpCode::BAD)
            {
            opCode = TR::InstOpCode(longDisplacementMnemonic);
            }
         }
      }

   opCode.copyBinaryToBufferWithoutClear(cursor);

   // AKA Even Reg
   toRealRegister(getFirstRegister())->setRegister1Field((uint32_t *) cursor);

   // AKA Odd Reg
   TR::InstOpCode::Mnemonic op = getOpCodeValue();
   if (op == TR::InstOpCode::CDS || op == TR::InstOpCode::CDSG || op == TR::InstOpCode::MVCLE || op == TR::InstOpCode::MVCLU || op == TR::InstOpCode::CLCLE || op == TR::InstOpCode::CLCLU)
      {
      toRealRegister(getSecondRegister()->getHighOrder())->setRegister2Field((uint32_t *) cursor);
      }
   else if (getLastRegister())
      {
      toRealRegister(getLastRegister())->setRegister2Field((uint32_t *) cursor);
      }

   instructionStart = cursor;
   cursor += opCode.getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);
   return cursor;
   }

// TR::S390RRSInstruction:: member functions

/**
 * RRS Format
 *  ________ ____ ____ ____ ____________ ___________________
 * |Op Code | R1 | R2 | B4 |     D4     | M3 |////| Op Code |
 * |________|____|____|____|____________|____|____|_________|
 * 0         8   12   16   20           32   36   40       47
 */
uint8_t *
TR::S390RRSInstruction::generateBinaryEncoding()
   {

   // acquire the current cursor location so we can start generating our
   // instruction there.
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();

   // we'll need to keep the start location, and our current writing location.
   uint8_t * cursor = instructionStart;

   // clear the number of bytes we intend to use in the buffer.
   memset( (void*)cursor, 0, getEstimatedBinaryLength());

   // overlay the actual instruction op code to the buffer.  this will properly
   // set the second half of the op code in the 6th byte.
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // add the register number for the first register to the buffer.  this will
   // write in the first nibble after the op code.
   toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);

   // add the register number for the second register to the buffer   this will
   // write the reg evaluation into the right-most nibble of a byte which starts just
   // after the bytecode.
   toRealRegister(getRegisterOperand(2))->setRegister2Field((uint32_t *) cursor);

   // advance the cursor in the buffer 2 bytes (op code + reg specifications).
   cursor += 2;

   // evaluate the memory reference into the buffer.
   int32_t padding = getBranchDestinationLabel()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // add the comparison mask for the instruction to the buffer.
   *(cursor) |= (uint8_t)getMask();

   // the advance the cursor 1 byte for the mask byte we just wrote, and another
   // byte for the tail part of the op code.
   cursor += 2;

   // set the binary length of our instruction
   setBinaryLength(cursor - instructionStart);

   // set the binary encoding to point at where our instruction begins in the
   // buffer
   setBinaryEncoding(instructionStart);

   // account for the error in estimation
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());

   // return the the cursor for the next instruction.
   return cursor;
   }

// TR::S390RIEInstruction:: member functions
/**
 *    RIE Format
 *
 *    RIE1: CRJ, GGRJ
 *     ________ ____ ____ _________________ ___________________
 *    |Op Code | R1 | R2 |       I4        | M3 |////| Op Code |
 *    |________|____|____|_________________|____|____|_________|
 *    0         8   12   16                32   36   40       47
 *
 *    RIE2: CIJ, CGIJ
 *     ________ ____ ____ _________________ ___________________
 *    |Op Code | R1 | M3 |       I4        |   I2    | Op Code |
 *    |________|____|____|_________________|_________|_________|
 *    0         8   12   16                32        40       47
 *
 *    RIE3:  CIT, CGIT
 *     ________ ____ ____ _________________ ___________________
 *    |Op Code | R1 | // |       I2        | M3 |////| Op Code |
 *    |________|____|____|_________________|____|____|_________|
 *
 *    RIE4:  AHIK
 *     ________ ____ ____ ________ ________ _________ _________
 *    |Op Code | R1 | R3 |       I2        |/////////| Op Code |
 *    |________|____|____|________|________|_________|_________|
 *    0         8   12   16       24       32        40       47
 *
 *    RIE5:  RISBG
 *     ________ ____ ____ ________ ________ _________ _________
 *    |Op Code | R1 | R2 |   I3   |   I4   |    I5   | Op Code |
 *    |________|____|____|________|________|_________|_________|
 *    0         8   12   16       24       32        40       47
 *
 *    RIE6:  LOCHI
 *     ________ ____ ____ ________ ________ _________ _________
 *    |Op Code | R1 | M3 |       I2        |/////////| Op Code |
 *    |________|____|____|_________________|_________|_________|
 *    0         8   12   16                32  36    40       47
 */
int32_t
TR::S390RIEInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getBranchDestinationLabel())
      setEstimatedBinaryLength(12);
   else
      setEstimatedBinaryLength(getOpCode().getInstructionLength());
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *
TR::S390RIEInstruction::generateBinaryEncoding()
   {

   // let's determine what form of RIE we are dealing with
   bool RIE1 = (getRieForm() == TR::S390RIEInstruction::RIE_RR);
   bool RIE2 = (getRieForm() == TR::S390RIEInstruction::RIE_RI8);
   bool RIE3 = (getRieForm() == TR::S390RIEInstruction::RIE_RI16A);
   bool RIE4 = (getRieForm() == TR::S390RIEInstruction::RIE_RRI16);
   bool RIE5 = (getRieForm() == TR::S390RIEInstruction::RIE_IMM);
   bool RIE6 = (getRieForm() == TR::S390RIEInstruction::RIE_RI16G);

   // acquire the current cursor location so we can start generating our
   // instruction there.
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();

   // we'll need to keep the start location, and our current writing location.
   uint8_t * cursor = instructionStart;

   // clear the number of bytes we intend to use in the buffer.
   memset((void*)cursor, 0, getEstimatedBinaryLength());

   // overlay the actual instruction op code to the buffer.  this will properly
   // set the second half of the op code in the 6th byte.
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // add the register number for the first register to the buffer after the op code.
   TR::Register * tgtReg = getRegForBinaryEncoding(getRegisterOperand(1));
   toRealRegister(tgtReg)->setRegister1Field((uint32_t *) cursor);

   // for RIE1 or RIE5 form, we encode the second register.
   if(RIE1 || RIE5 || RIE4)
      {
      // add the register number for the second register to the buffer after the first register.
      TR::Register * srcReg = getRegForBinaryEncoding(getRegisterOperand(2));
      toRealRegister(srcReg)->setRegister2Field((uint32_t *) cursor);
      }
   // for RIE2 or RIE6 form, we encode the branch mask.
   else if(RIE2 || RIE6)
      {
      *(cursor + 1) |= (uint8_t)getMask()>>4;
      }
   // for RIE3 the field is empty

   // advance the cursor past the the first 2 bytes.
   cursor += 2;

   // if we have a branch destination (RIE1, RIE2), we encode that now.
   if (getBranchDestinationLabel())
      {
      TR::LabelSymbol * label = getBranchDestinationLabel();
      int32_t distance = 0;
      int32_t trampolineDistance = 0;
      bool doRelocation = false;

      if (label->getCodeLocation() != NULL)
         {
         // Label location is known
         // calculate the relative branch distance
         distance = (label->getCodeLocation() - instructionStart);
         doRelocation = false;
         }
      else
         {
         // Label location is unknown
         // estimate the relative branch distance
         distance = (cg()->getBinaryBufferStart() + label->getEstimatedCodeLocation()) -
            (instructionStart + cg()->getAccumulatedInstructionLengthError());
         doRelocation = true;
         }

      // now we'll encode the branch destination.
      if ((distance / 2) >= MIN_IMMEDIATE_VAL && (distance / 2) <= MAX_IMMEDIATE_VAL)
         {
         if (doRelocation)
            {
            cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(cursor, label));
            }
         else
            {
            *(int16_t *) (cursor) |= bos(distance / 2);
            }
         }
      else
         {
         //generate CHI/CGHI  BRCL sequence
         cursor = instructionStart;

         // clear the number of bytes we intend to use in the buffer.
         memset((void*)cursor, 0, getEstimatedBinaryLength());

         //Generate equivalent Compare instruction
         TR::InstOpCode::Mnemonic cmpOp;
         bool isRR=false;
         bool isRRE=false;
         bool isRI=false;
         bool isRIL=false;
         switch (getOpCodeValue())
            {
            case TR::InstOpCode::CGRJ   :
                 cmpOp = TR::InstOpCode::CGR;
                 *(uint16_t *) (cursor) = bos(0xB920);
                 isRRE = true;
                 break;
            case TR::InstOpCode::CLGRJ  :
                 cmpOp = TR::InstOpCode::CLGR;
                 *(uint16_t *) (cursor) = bos(0xB921);
                 isRRE = true;
                 break;
            case TR::InstOpCode::CLRJ   :
                 cmpOp = TR::InstOpCode::CLR;
                 *(uint8_t *) (cursor) = 0x15;
                 isRR  = true;
                 break;
            case TR::InstOpCode::CRJ    :
                 cmpOp = TR::InstOpCode::CR;
                 *(uint8_t *) (cursor) = 0x19;
                 isRR  = true;
                 break;
            case TR::InstOpCode::CGIJ   :
                 cmpOp = TR::InstOpCode::CGHI;
                 *(uint16_t *) (cursor) = bos(0xA70F);
                 isRI  = true;
                 break;
            case TR::InstOpCode::CIJ    :
                 cmpOp = TR::InstOpCode::CHI;
                 *(uint16_t *) (cursor) = bos(0xA70E);
                 isRI  = true;
                 break;
            case TR::InstOpCode::CLGIJ  :
                 cmpOp = TR::InstOpCode::CLGFI;
                 *(uint16_t *) (cursor) = bos(0xC20E);
                 isRIL = true;
                 break;
            case TR::InstOpCode::CLIJ   :
                 cmpOp = TR::InstOpCode::CLFI;
                 *(uint16_t *) (cursor) = bos(0xC20F);
                 isRIL = true;
                 break;
            }

         if (isRRE)
            {
            //position cursor forward so that the registers encoding lines up with isRR
            cursor += 2;
            }

         // add the register number for the first register to the buffer after the op code.
         TR::Register * tgtReg = getRegForBinaryEncoding(getRegisterOperand(1));
         toRealRegister(tgtReg)->setRegister1Field((uint32_t *) cursor);

         if (isRI || isRIL)
            {
            cursor += 2;
            }

         if (isRR || isRRE)
            {
            // add the register number for the second register to the buffer after the first register.
            TR::Register * srcReg = getRegForBinaryEncoding(getRegisterOperand(2));
            toRealRegister(srcReg)->setRegister2Field((uint32_t *) cursor);
            cursor += 2;
            }
         else if (isRI)
            {
            //TR::InstOpCode::CHI/TR::InstOpCode::CGHI are signed so sign extend the 8 bit immediate value to 16 bits
            (*(int16_t *) (cursor)) = bos((int16_t) getSourceImmediate8());
            cursor += 2;
            }
         else if (isRIL)
            {
            //TR::InstOpCode::CLGFI/TR::InstOpCode::CLFI are unsigned so zero extend the 8 bit immediate value to 32 bits
            (*(int8_t *) (cursor+3)) =  getSourceImmediate8();
            cursor += 4;
            }

         //Now generate BRCL
         *(uint16_t *) cursor = bos(0xC004);
         *(cursor + 1) |= (uint8_t) getMask();

         if (doRelocation)
            {
            cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, label));
            }
         else
            {
            distance = (label->getCodeLocation() - cursor);
            *(int32_t *) (cursor + 2) |= boi(distance / 2);
            }

         cursor += 6;

         setBinaryLength(cursor - instructionStart);
         setBinaryEncoding(instructionStart);

         cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
         return cursor;
         }
      }
   // if we don't have a branch destination (and not type RIE5), we have a 16-bit immediate value to compare
   else if (!RIE5)
      {
      *(int16_t *) (cursor) |= bos((int16_t)getSourceImmediate16());
      TR_ASSERT( ((getSourceImmediate16() & 0xB900) != 0xB900) ||
               ((getOpCodeValue() != 0xEC71) && (getOpCodeValue() != 0xEC73)),
               "CLFIT has Immediate value 0xB9XX, signalHandler might confuse DIVCHK with BNDCHK");
      }
   // if we are type RIE5 place the two immediate values in
   else
      {
      if (getOpCodeValue() == TR::InstOpCode::RISBLG || getOpCodeValue() == TR::InstOpCode::RISBHG)
         // mask the I3 field as bits 1-3 must be 0
         *(int8_t *) (cursor) |= (int8_t)getSourceImmediate8One() & 0x1F;
      else if (getOpCodeValue() == TR::InstOpCode::RISBG || getOpCodeValue() == TR::InstOpCode::RISBGN)
         {
         // mask the I3 field as bits 0-1 must be 0
         *(int8_t *) (cursor) |= (int8_t)getSourceImmediate8One() & 0x3F;
         TR_ASSERT(((int8_t)getSourceImmediate8One() & 0xC0) == 0, "Bits 0-1 in the I3 field for %s must be 0", getOpCodeValue() == TR::InstOpCode::RISBG ? "RISBG" : "RISBGN");
         }
      else
         *(int8_t *) (cursor) |= (int8_t)getSourceImmediate8One();

      cursor += 1;

      if (getOpCodeValue() == TR::InstOpCode::RISBLG || getOpCodeValue() == TR::InstOpCode::RISBHG)
         // mask the I4 field as bits 1-2 must be 0
         *(int8_t *) (cursor) |= (int8_t)getSourceImmediate8Two() & 0x9F;
      else if (getOpCodeValue() == TR::InstOpCode::RISBG || getOpCodeValue() == TR::InstOpCode::RISBGN)
         {
         *(int8_t *) (cursor) |= (int8_t)getSourceImmediate8Two() & 0xBF;
         TR_ASSERT(((int8_t)getSourceImmediate8Two() & 0x40) == 0, "Bit 1 in the I4 field for %s must be 0", getOpCodeValue() == TR::InstOpCode::RISBG ? "RISBG" : "RISBGN");
         }
      else
         *(int8_t *) (cursor) |= (int8_t)getSourceImmediate8Two();
      cursor -= 1;
      }

   // advance the cursor past the 2 bytes we just wrote to the buffer.
   cursor += 2;

   // for RIE1 and RIE3 we now generate the branch mask
   if (RIE1 || RIE3)
      {
      *(cursor) |= (uint8_t)getMask();
      }
   // for RIE2 we load the 8 bit immediate for comparison
   else
      {
      // add in the immediate comparison value
      (*(int8_t *) (cursor)) |= getSourceImmediate8();
      }

   // advance the cursor past the byte we just wrote and the op code byte at
   // the end.
   cursor += 2;

   // set the binary length of our instruction
   setBinaryLength(cursor - instructionStart);

   // set the binary encoding to point at where our instruction begins in the buffer
   setBinaryEncoding(instructionStart);

   // account for the error in estimation
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());

   // return the updated cursor position.
   return cursor;

   }

/**
 * Split the RIE instruction into separate compare and long branch
 * Instructions. We need to do this so that compiler listings
 * work properly, we can't just fix this up in the binary buffer
 * like its done for JAVA
 */
uint8_t *
TR::S390RIEInstruction::splitIntoCompareAndLongBranch(void)
   {
   //Generate equivalent Compare instruction
   TR::InstOpCode::Mnemonic cmpOp;
   bool isRR = false;
   bool isRRE = false;
   bool isRI = false;
   bool isRIL = false;
   switch (getOpCodeValue())
      {
      case TR::InstOpCode::CGRJ   :
       cmpOp = TR::InstOpCode::CGR;
       isRRE = true;
       break;
      case TR::InstOpCode::CLGRJ  :
       cmpOp = TR::InstOpCode::CLGR;
       isRRE = true;
       break;
      case TR::InstOpCode::CLRJ   :
       cmpOp = TR::InstOpCode::CLR;
       isRR  = true;
       break;
      case TR::InstOpCode::CRJ    :
       cmpOp = TR::InstOpCode::CR;
       isRR  = true;
       break;
      case TR::InstOpCode::CGIJ   :
       cmpOp = TR::InstOpCode::CGHI;
       isRI  = true;
       break;
      case TR::InstOpCode::CIJ    :
       cmpOp = TR::InstOpCode::CHI;
       isRI  = true;
       break;
      case TR::InstOpCode::CLGIJ  :
       cmpOp = TR::InstOpCode::CLGFI;
       isRIL = true;
       break;
      case TR::InstOpCode::CLIJ   :
       cmpOp = TR::InstOpCode::CLFI;
       isRIL = true;
       break;
      }

  TR::Instruction * prevInstr = this;
  TR::Instruction * currInstr = prevInstr;
  uint8_t *cursor;

   if (isRRE || isRR)
      currInstr = generateRRInstruction(cg(), cmpOp, getNode(), getRegisterOperand(1),
        getRegisterOperand(2), prevInstr);
   else if (isRI)
      currInstr = generateRIInstruction(cg(), cmpOp, getNode(), getRegisterOperand(1),
        (int16_t) getSourceImmediate8(),prevInstr);
   else if (isRIL)
      currInstr = generateRILInstruction(cg(), cmpOp, getNode(), getRegisterOperand(1),
        (uint8_t) getSourceImmediate8(), prevInstr);

   currInstr->setEstimatedBinaryLength(currInstr->getOpCode().getInstructionLength());
   cg()->setBinaryBufferCursor(cursor = currInstr->generateBinaryEncoding());

   prevInstr = currInstr;

   // generate a BRC and the binary encoding will fix it up to a BRCL if needed
   currInstr = generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, getBranchCondition(), getNode(), getBranchDestinationLabel(), prevInstr);
   currInstr->setEstimatedBinaryLength(6);

   cg()->setBinaryBufferCursor(cursor = currInstr->generateBinaryEncoding());

   // Remove the current instructions from the list
   this->getPrev()->setNext(this->getNext());
   this->getNext()->setPrev(this->getPrev());
   // set 'this' - the compare and branch instruction being removed, 'next' pointer to the instruction after the inserted BRC/BRCL
   // so the main cg generateBinaryEncoding loop does not encode the new CHI and BRC/BRCL instruction a second time
   this->setNext(currInstr->getNext());

   return cursor;
   }

/**
 * To be used at instruction level if combined compare and branch will not work.
 * The current combined compare and branch instruction is split into seperate
 * compare instruction and a new branch instruction. The insertion point for the
 * branch is provided as an argument. The compare instruction is inserted right after
 * the combined compare and branch instruction.
 */
void
TR::S390RIEInstruction::splitIntoCompareAndBranch(TR::Instruction *insertBranchAfterThis)
   {
   //Generate equivalent Compare instruction
   TR::InstOpCode::Mnemonic cmpOp;
   bool isRR = false;
   bool isRRE = false;
   bool isRI = false;
   bool isRIL = false;
   switch (getOpCodeValue())
      {
      case TR::InstOpCode::CGRJ   :
       cmpOp = TR::InstOpCode::CGR;
       isRRE = true;
       break;
      case TR::InstOpCode::CLGRJ  :
       cmpOp = TR::InstOpCode::CLGR;
       isRRE = true;
       break;
      case TR::InstOpCode::CLRJ   :
       cmpOp = TR::InstOpCode::CLR;
       isRR  = true;
       break;
      case TR::InstOpCode::CRJ    :
       cmpOp = TR::InstOpCode::CR;
       isRR  = true;
       break;
      case TR::InstOpCode::CGIJ   :
       cmpOp = TR::InstOpCode::CGHI;
       isRI  = true;
       break;
      case TR::InstOpCode::CIJ    :
       cmpOp = TR::InstOpCode::CHI;
       isRI  = true;
       break;
      case TR::InstOpCode::CLGIJ  :
       cmpOp = TR::InstOpCode::CLGFI;
       isRIL = true;
       break;
      case TR::InstOpCode::CLIJ   :
       cmpOp = TR::InstOpCode::CLFI;
       isRIL = true;
       break;
      default:
        TR_ASSERT(false,"Don't know how to split this compare and branch opcode %s\n",getOpCode().getMnemonicName());
        break;
      }

  TR::Instruction * prevInstr = this;

   if (isRRE || isRR)
      generateRRInstruction(cg(), cmpOp, getNode(), getRegisterOperand(1),getRegisterOperand(2), prevInstr);
   else if (isRI)
      generateRIInstruction(cg(), cmpOp, getNode(), getRegisterOperand(1),(int16_t) getSourceImmediate8(),prevInstr);
   else if (isRIL)
      generateRILInstruction(cg(), cmpOp, getNode(), getRegisterOperand(1),(uint8_t) getSourceImmediate8(), prevInstr);

   // generate a BRC
   generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, getBranchCondition(), getNode(), getBranchDestinationLabel(), insertBranchAfterThis);

   // Remove the current instructions from the list
   this->getPrev()->setNext(this->getNext());
   this->getNext()->setPrev(this->getPrev());

   }

// TR::S390RISInstruction:: member functions
/**
 *    RIS Format
 *
 *     ________ ____ ____ ____ ____________ ___________________
 *    |Op Code | R1 | M3 | B4 |     D4     |    I2   | Op Code |
 *    |________|____|____|____|____________|_________|_________|
 *    0         8   12   16   20           32        40       47
 */
uint8_t *
TR::S390RISInstruction::generateBinaryEncoding()
   {

   // acquire the current cursor location so we can start generating our
   // instruction there.
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();

   // we'll need to keep the start location, and our current writing location.
   uint8_t * cursor = instructionStart;

   // clear the number of bytes we intend to use in the buffer.
   memset((void*)cursor, 0, getEstimatedBinaryLength());

   // overlay the actual instruction op code to the buffer.  this will properly
   // set the second half of the op code in the 6th byte.
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // add the register number for the first register to the buffer right after the op code.
   toRealRegister(getRegisterOperand(1))->setRegister1Field((uint32_t *) cursor);

   // advance the cursor one byte.
   cursor += 1;

   // add the comparison mask for the instruction to the buffer.
   *(cursor) |= (uint8_t)getMask();

   // advance the cursor past the register and mask encoding.
   cursor += 1;

   // add the memory reference for the target to the buffer, and advance the
   // cursor past what gets written.
   int32_t padding = getBranchDestinationLabel()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // add in the immediate second operand to the instruction
   (*(int8_t *) (cursor)) |= getSourceImmediate();

   // advance the cursor past that byte and the op code byte.
   cursor += 2;

   // set the binary length of our instruction
   setBinaryLength(cursor - instructionStart);

   // set the binary encoding to point at where our instruction begins in the
   // buffer
   setBinaryEncoding(instructionStart);

   // account for the error in estimation
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());

   // return the the cursor for the next instruction.
   return cursor;
   }

// TR::S390MemInstruction:: member functions

bool
TR::S390MemInstruction::refsRegister(TR::Register * reg)
   {
   // 64bit mem refs clobber high word regs
   if (reg->getKind() != TR_FPR && reg->getKind() != TR_VRF && reg->getRealRegister())
      {
      TR::RealRegister * realReg = (TR::RealRegister *)reg;
      TR::Register * regMem = reg;

      if (getMemoryReference()->refsRegister(regMem))
         {
         return true;
         }
      }
   if (getMemoryReference()->refsRegister(reg))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

uint8_t *
TR::S390MemInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());

   getOpCode().copyBinaryToBuffer(instructionStart);
   if (getMemAccessMode() >= 0)
      {
      (*(uint32_t *) cursor) &= boi(0xFF0FFFFF);
      (*(uint32_t *) cursor) |= boi((getMemAccessMode() & 0xF)<< 20);
      }

   getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

// TR::S390RXInstruction:: member functions
bool
TR::S390RXInstruction::refsRegister(TR::Register * reg)
   {
   // 64bit mem refs clobber high word regs
   if (reg->getKind() != TR_FPR && reg->getKind() != TR_VRF && getMemoryReference()&& reg->getRealRegister())
      {
      TR::RealRegister * realReg = (TR::RealRegister *)reg;
      TR::Register * regMem = reg;

      if (getMemoryReference()->refsRegister(regMem))
         {
         return true;
         }
      }
   if (matchesTargetRegister(reg))
      {
      return true;
      }
   else if ((getMemoryReference() != NULL) && (getMemoryReference()->refsRegister(reg) == true))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

int32_t
TR::S390RXInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390RXInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   TR::InstOpCode& opCode = getOpCode();

   if (getMemoryReference()->isLongDisplacementRequired())
      {
      auto longDisplacementMnemonic = TR::InstOpCode::getEquivalentLongDisplacementMnemonic(getOpCodeValue());

      if (longDisplacementMnemonic != TR::InstOpCode::BAD)
         {
         opCode = TR::InstOpCode(longDisplacementMnemonic);
         }
      }

   opCode.copyBinaryToBufferWithoutClear(cursor);

   TR::Register * trgReg = getRegForBinaryEncoding(getRegisterOperand(1));
   toRealRegister(trgReg)->setRegisterField((uint32_t *) cursor);

   instructionStart = cursor;
   cursor += opCode.getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

int32_t
TR::S390RXEInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390RXEInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0;

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the actual instruction, reg, and mask
   getOpCode().copyBinaryToBufferWithoutClear(cursor);
   TR::Register * trgReg = getRegForBinaryEncoding(getRegisterOperand(1));
   toRealRegister(trgReg)->setRegisterField((uint32_t *) cursor);
   setMaskField(((uint32_t*)cursor + 1), getM3(), 7);        // M3: bits 32-36

   cursor += (getOpCode().getInstructionLength());

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding);

   return cursor;
   }

int32_t
TR::S390RXYInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390RXYInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the actual instruction and reg
   getOpCode().copyBinaryToBufferWithoutClear(cursor);
   TR::Register * trgReg = getRegForBinaryEncoding(getRegisterOperand(1));
   toRealRegister(trgReg)->setRegisterField((uint32_t *) cursor);

   instructionStart = cursor;
   cursor += (getOpCode().getInstructionLength());

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

// TR::S390RXFInstruction:: member functions /////////////////////////////////////////

int32_t
TR::S390RXFInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
   }

bool
TR::S390RXFInstruction::refsRegister(TR::Register * reg)
   {
   // 64bit mem refs clobber high word regs
   if (reg->getKind() != TR_FPR && reg->getKind() != TR_VRF && reg->getRealRegister())
      {
      TR::RealRegister * realReg = (TR::RealRegister *)reg;
      TR::Register * regMem = reg;

      if (getMemoryReference()->refsRegister(regMem))
         {
         return true;
         }
      }
   if (matchesTargetRegister(reg) || matchesAnyRegister(reg, getRegisterOperand(2)) || getMemoryReference()->refsRegister(reg) == true)
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

uint8_t *
TR::S390RXFInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   int32_t padding = 0, longDispTouchUpPadding = 0;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);

   cursor += padding;

   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   TR::RegisterPair* srcRegPair = getRegisterOperand(2)->getRegisterPair();
   TR::RegisterPair* tgtRegPair = getRegisterOperand(1)->getRegisterPair();

   bool isTgtFPPair = false;

   if(isTgtFPPair)
      {
      toRealRegister(srcRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor);
      toRealRegister(tgtRegPair->getHighOrder())->setRegisterField((uint32_t *) cursor + 1, 7);
      }
   else
      {
      toRealRegister(getRegisterOperand(2))->setRegisterField((uint32_t *) cursor);
      toRealRegister(getRegisterOperand(1))->setRegisterField((uint32_t *) cursor + 1, 7);
      }

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
//                       Vector Instruction Support                           //
////////////////////////////////////////////////////////////////////////////////
//
/**** Vector Instruction Generic ***/

void appendElementSizeMnemonic(char * opCodeBuffer, int8_t elementSize)
   {
   char *extendedMnemonic[] = {"B","H","F","G","Q"};
   strcat(opCodeBuffer, extendedMnemonic[elementSize]);
   }

const char *
getOpCodeName(TR::InstOpCode * opCode)
   {
   return TR::InstOpCode::metadata[opCode->getOpCodeValue()].name;
   }

TR::S390VInstruction::~S390VInstruction()
   {
   if (_opCodeBuffer)
      TR::Instruction::jitPersistentFree((char *)_opCodeBuffer);
   }

int32_t
TR::S390VInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   setEstimatedBinaryLength(getOpCode().getInstructionLength());

   return currentEstimate + getEstimatedBinaryLength();
   }

char *
TR::S390VInstruction::setOpCodeBuffer(char *c)
   {
   if (_opCodeBuffer)
      TR_ASSERT(false, "trying to set OpCodeBuffer after it has already been set!\n");

   _opCodeBuffer = (char *) TR::Instruction::jitPersistentAlloc(strlen(c)+1);
   return strcpy(_opCodeBuffer, c);
   }

/**
 *
 * VRI  getExtendedMnemonicName
 *
*/
const char *
TR::S390VRIInstruction::getExtendedMnemonicName()
   {
   if (getOpCodeBuffer())
      return getOpCodeBuffer();

   char tmpOpCodeBuffer[16];
   strcpy(tmpOpCodeBuffer, getOpCode().getMnemonicName());

   if (!getOpCode().hasExtendedMnemonic())
      return setOpCodeBuffer(tmpOpCodeBuffer);

   bool printM3 = getPrintM3();
   bool printM4 = getPrintM4();
   bool printM5 = getPrintM5();

   int32_t elementSize = -1;
   if (getOpCode().usesM3())
      {
      elementSize = getM3();
      printM3 = false;
      }
   else if (getOpCode().usesM4())
      {
      elementSize = getM4();
      printM4 = false;
      }
   else if (getOpCode().usesM5())
      {
      elementSize = getM5();
      printM5 = false;
      }

   // handles VFTCI
   if (getOpCode().isVectorFPOp())
      {
      strcat(tmpOpCodeBuffer, "DB");
      if (getM5() & 0x8)
         tmpOpCodeBuffer[0] = 'W';
      printM5 = false;
      }
   else
      appendElementSizeMnemonic(tmpOpCodeBuffer, elementSize);

   setPrintM3(printM3);
   setPrintM4(printM4);
   setPrintM5(printM5);

   return setOpCodeBuffer(tmpOpCodeBuffer);
   }

/**
 * VRI preGenerateBinaryEncoding
 */
uint8_t*
TR::S390VRIInstruction::preGenerateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   memset(static_cast<void*>(instructionStart), 0, getEstimatedBinaryLength());

   getOpCode().copyBinaryToBuffer(instructionStart);

   return instructionStart;
   }

/**
 * VRI postGenerateBinaryEncoding
 */
uint8_t*
TR::S390VRIInstruction::postGenerateBinaryEncoding(uint8_t* cursor)
   {
   // move cursor
   uint8_t* instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);

   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

/** \details
 *
 * VRI-a generate binary encoding for VRI-a instruction format
 */
uint8_t *
TR::S390VRIaInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   (*reinterpret_cast<uint16_t*>(cursor + 2)) |= bos(static_cast<uint16_t>(getImmediateField16()));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t*>(cursor), getM3(), 3);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-b generate binary encoding for VRI-b instruction format
 */
uint8_t *
TR::S390VRIbInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   (*reinterpret_cast<uint16_t*>(cursor + 2)) |= bos(static_cast<uint16_t>(getImmediateField16()));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t*>(cursor), getM4(), 3);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-c generate binary encoding for VRI-c instruction format
 */
uint8_t *
TR::S390VRIcInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   (*reinterpret_cast<uint16_t*>(cursor + 2)) |= bos(static_cast<uint16_t>(getImmediateField16()));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));
   toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t*>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t*>(cursor), getM4(), 3);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-d generate binary encoding for VRI-d instruction format
 */
uint8_t *
TR::S390VRIdInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(3) != NULL, "3rd Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   (*reinterpret_cast<uint16_t*>(cursor + 2)) |= bos(static_cast<uint16_t>(getImmediateField16()));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));
   toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t*>(cursor));
   toRealRegister(getRegisterOperand(3))->setRegister3Field(reinterpret_cast<uint32_t*>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t*>(cursor), getM5(), 3);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-e generate binary encoding for VRI-e instruction format
 */
uint8_t *
TR::S390VRIeInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   (*reinterpret_cast<uint16_t*>(cursor + 2)) |= bos(static_cast<uint16_t>(getImmediateField16()));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));
   toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t*>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t*>(cursor), getM5(), 2);
   setMaskField(reinterpret_cast<uint32_t*>(cursor), getM4(), 3);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-f generate binary encoding for VRI-f instruction format
 */
uint8_t *
TR::S390VRIfInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(3) != NULL, "3rd Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   (*reinterpret_cast<uint16_t *>(cursor + 3)) |= bos(static_cast<uint16_t>((getImmediateField16() & 0xff) << 4));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t *>(cursor));
   toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t *>(cursor));
   toRealRegister(getRegisterOperand(3))->setRegister3Field(reinterpret_cast<uint32_t *>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t *>(cursor), getM5(), 1);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-g generate binary encoding for VRI-g instruction format
 */
uint8_t *
TR::S390VRIgInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   // I3 stored in _constantImm16 as uint16_t
   // I4 stored in _constantImm8 as uint8_t
   (*reinterpret_cast<uint16_t*>(cursor + 3)) |= bos(static_cast<uint16_t>((getImmediateField16() & 0xff) << 4));
   (*(cursor + 2)) |= bos(getImmediateField8());

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));
   toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t*>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t*>(cursor), getM5(), 1);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-h generate binary encoding for VRI-h instruction format
 */
uint8_t *
TR::S390VRIhInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   // do cursor +4 first
   *(cursor + 4) |= bos(static_cast<uint8_t>(getImmediateField8() << 4)) ;
   (*reinterpret_cast<uint16_t*>(cursor + 2)) |= bos(static_cast<uint16_t>(getImmediateField16()));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t *>(cursor));

   // Masks: none

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRI-i generate binary encoding for VRI-i instruction format
 */
uint8_t *
TR::S390VRIiInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");

   // Generate Binary Encoding
   uint8_t* cursor = preGenerateBinaryEncoding();

   // The Immediate field
   (*reinterpret_cast<uint16_t *>(cursor + 3)) |= bos(static_cast<uint16_t>((getImmediateField8() & 0xff) << 4));

   // Operands
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t *>(cursor));
   toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t *>(cursor));

   // Masks
   setMaskField(reinterpret_cast<uint32_t *>(cursor), getM4(), 1);

   return postGenerateBinaryEncoding(cursor);
   }

/** \details
 *
 * VRR instruction format get extended mnemonic name returns a charactor array that contains
 * the extended mnemonic names
 */
const char *
TR::S390VRRInstruction::getExtendedMnemonicName()
   {
   if (getOpCodeBuffer())
      return getOpCodeBuffer();

   char tmpOpCodeBuffer[16];
   strcpy(tmpOpCodeBuffer, getOpCode().getMnemonicName());

   if (!getOpCode().hasExtendedMnemonic())
      return setOpCodeBuffer(tmpOpCodeBuffer);

   bool printM3 = getPrintM3();
   bool printM4 = getPrintM4();
   bool printM5 = getPrintM5();
   bool printM6 = getPrintM6();

   bool useBInsteadOfDB = false;
   uint8_t singleSelector = 0;
   if (getOpCode().isVectorFPOp())
      {
      singleSelector = getM5();
      printM5 = false;
      }

   uint8_t ccMask = 0;
   // get condition code mask for printing out "S" suffix
   if (getOpCode().setsCC())
      {
      if (getOpCode().isVectorFPOp())
         {
         ccMask = getM6();
         printM6 = false;
         }
      else if (getOpCode().usesM5())
         {
         ccMask = getM5();
         printM5 = false;
         }
      }

   switch (getOpCode().getOpCodeValue())
      {
      case TR::InstOpCode::VSTRC:
         // only string instruction that uses M6 as the CC mask
         ccMask = getM6();
         break;
      case TR::InstOpCode::VFAE:
         printM5 = true;
         break;

      // handle Vector FP extended mnemonics
      case TR::InstOpCode::VLDE:
         useBInsteadOfDB = true;
      case TR::InstOpCode::VFSQ:
         singleSelector = getM4();
      case TR::InstOpCode::WFC:
      case TR::InstOpCode::WFK:
         printM4 = false;
         break;
      case TR::InstOpCode::VCDG:
      case TR::InstOpCode::VCDLG:
      case TR::InstOpCode::VCGD:
      case TR::InstOpCode::VCLGD:
      case TR::InstOpCode::VLED:
         useBInsteadOfDB = true;
      case TR::InstOpCode::VFI:
         singleSelector = getM4();
         printM5 = true;
         break;
      case TR::InstOpCode::VFM:
      case TR::InstOpCode::VFMA:
      case TR::InstOpCode::VFMS:
         printM6 = false;
         break;
      case TR::InstOpCode::VFPSO:
         printM3 = printM4 = printM5 = false;
         singleSelector = getM4();
         if (getM5() == 0)
            strcpy(tmpOpCodeBuffer, "VFLC");
         else if (getM5() == 1)
            strcpy(tmpOpCodeBuffer, "VFLN");
         else if (getM5() == 2)
            strcpy(tmpOpCodeBuffer, "VFLP");
         break;
      }

   // the first mask is generally not printed
   // first mask is also the element size control mask
   int32_t elementSize = -1;
   if (getOpCode().usesM3())
      {
      elementSize = getM3();
      printM3 = false;
      }
   else if (getOpCode().usesM4())
      {
      elementSize = getM4();
      printM4 = false;
      }
   else if (getOpCode().usesM5())
      {
      elementSize = getM5();
      printM5 = false;
      }

   // handle vector string and vector float opcodes
   if (getOpCode().isVectorFPOp())
      {
      // if operating on single element, replace V with W
      if (singleSelector & 0x8)
         tmpOpCodeBuffer[0] = 'W';

      // these FP instructions end with a B
      if (useBInsteadOfDB)
         strcat(tmpOpCodeBuffer, "B");
      else
         strcat(tmpOpCodeBuffer, "DB");
      }
   else
      {
      if (getOpCode().isVectorStringOp() && (ccMask & 0x2))
         strcat(tmpOpCodeBuffer, "Z");

      // these opcodes with +H suffix should be HW
      // eg. VUPL+H should be VUPLHW since VUPLH is another opcode
      if ((getOpCodeValue() == TR::InstOpCode::VUPL || getOpCodeValue() == TR::InstOpCode::VMAL || getOpCodeValue() == TR::InstOpCode::VML) && elementSize == 1)
         strcat(tmpOpCodeBuffer, "HW");
      else
         appendElementSizeMnemonic(tmpOpCodeBuffer, elementSize);
      }

   // append "S" suffix if mask indicates that it sets condition code
   if (ccMask & 0x1)
      strcat(tmpOpCodeBuffer, "S");

   setPrintM3(printM3);
   setPrintM4(printM4);
   setPrintM5(printM5);
   setPrintM6(printM6);

   return setOpCodeBuffer(tmpOpCodeBuffer);
   }

/** \details
 *
 *  VRR Generate Binary Encoding for most sub-types of the VRR instruction format
 *
 *  VRR-g,h,i have their own implementations due to format differences.
*/
uint8_t *
TR::S390VRRInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset((void*)cursor, 0, getEstimatedBinaryLength());

   // Copy binary
   getOpCode().copyBinaryToBuffer(instructionStart);

   // Masks
   uint8_t maskIn0=0, maskIn1=0, maskIn2=0, maskIn3=0;
   switch(getKind())
      {
      case TR::Instruction::IsVRRa: maskIn1 = getM5(); maskIn2 = getM4(); maskIn3 = getM3(); break;
      case TR::Instruction::IsVRRb: maskIn1 = getM5(); maskIn3 = getM4(); break;
      case TR::Instruction::IsVRRc: maskIn1 = getM6(); maskIn2 = getM5(); maskIn3 = getM4(); break;
      case TR::Instruction::IsVRRd: maskIn0 = getM5(); maskIn1 = getM6(); break;
      case TR::Instruction::IsVRRe: maskIn0 = getM6(); maskIn2 = getM5(); break;
      case TR::Instruction::IsVRRf: break; // no mask
      default: break;
      }
   setMaskField(reinterpret_cast<uint32_t *>(cursor), maskIn1, 1);
   setMaskField(reinterpret_cast<uint32_t *>(cursor), maskIn2, 2);
   setMaskField(reinterpret_cast<uint32_t *>(cursor), maskIn0, 0);

   // Operands 1-4, sets RXB when in setRegisterXField() methods
   if (getRegisterOperand(1) != NULL)
      toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t *>(cursor));

   if (getRegisterOperand(2) != NULL)
      toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t *>(cursor));

   if (getRegisterOperand(3) != NULL)
      toRealRegister(getRegisterOperand(3))->setRegister3Field(reinterpret_cast<uint32_t *>(cursor));

   if (getRegisterOperand(4) != NULL)
      {
       // Will cause assertion failure by now
      toRealRegister(getRegisterOperand(4))->setRegister4Field(reinterpret_cast<uint32_t *>(cursor));
      }
   else
      {
       // mask in mask field 3 (bit 32-35) is aliased with register operand 4
      setMaskField(reinterpret_cast<uint32_t *>(cursor), maskIn3, 3);
      }

   // Cursor move
   // update binary length
   // update binary length estimate error
   cursor += getOpCode().getInstructionLength();

   setBinaryLength(cursor - instructionStart);

   setBinaryEncoding(instructionStart);

   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

/** \details
 *
 * VRR-a generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRaInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");

   // Generate Binary Encoding
   return TR::S390VRRInstruction::generateBinaryEncoding();
   }

/** \details
 *
 * VRR-b generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRbInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(3) != NULL, "3rd Operand should not be NULL!");

   // Generate Binary Encoding
   return TR::S390VRRInstruction::generateBinaryEncoding();
   }

/** \details
 *
 * VRR-d generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRcInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(3) != NULL, "3rd Operand should not be NULL!");

   // Generate Binary Encoding
   return TR::S390VRRInstruction::generateBinaryEncoding();
   }

/** \details
 *
 * VRR-d generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRdInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(3) != NULL, "3rd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(4) != NULL, "4th Operand should not be NULL!");

   // Generate Binary Encoding
   return TR::S390VRRInstruction::generateBinaryEncoding();
   }

/** \details
 *
 * VRR-e generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRReInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(3) != NULL, "3rd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(4) != NULL, "4th Operand should not be NULL!");

   // Generate Binary Encoding
   return TR::S390VRRInstruction::generateBinaryEncoding();
   }

/** \details
 *
 * VRR-f generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRfInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR_ASSERT(getRegisterOperand(1) != NULL, "First Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(3) != NULL, "3rd Operand should not be NULL!");

   // Generate Binary Encoding
   return TR::S390VRRInstruction::generateBinaryEncoding();
   }

/** \details
 *
 * VRR-g generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRgInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR::Register* v1Reg = getRegisterOperand(1);
   TR_ASSERT( v1Reg != NULL, "VRR-g V1 should not be NULL!");

   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset((void*)cursor, 0, getEstimatedBinaryLength());

   // Copy binary
   getOpCode().copyBinaryToBuffer(instructionStart);

   // Operands 1 at the second field
   toRealRegister(v1Reg)->setRegister2Field(reinterpret_cast<uint32_t *>(cursor));

   // Cursor move
   // update binary length
   // update binary length estimate error
   cursor += getOpCode().getInstructionLength();
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

/** \details
 *
 * VRR-h generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRhInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR::Register* v1Reg = getRegisterOperand(1);
   TR::Register* v2Reg = getRegisterOperand(2);
   TR_ASSERT(v1Reg != NULL, "VRR-h operand should not be NULL!");
   TR_ASSERT(v2Reg != NULL, "VRR-h Operand should not be NULL!");

   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset((void*)cursor, 0, getEstimatedBinaryLength());

   // Copy binary
   getOpCode().copyBinaryToBuffer(instructionStart);

   // Masks
   uint8_t mask3 = getM3();
   setMaskField(reinterpret_cast<uint32_t *>(cursor), mask3, 1);

   // Operands
   toRealRegister(v1Reg)->setRegister2Field(reinterpret_cast<uint32_t *>(cursor));
   toRealRegister(v2Reg)->setRegister3Field(reinterpret_cast<uint32_t *>(cursor));

   // Cursor move
   // update binary length
   // update binary length estimate error
   cursor += getOpCode().getInstructionLength();
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

/** \details
 *
 * VRR-i generate binary encoding
 * Performs error checking on the operands and then deleagte the encoding work to its parent class
 */
uint8_t *
TR::S390VRRiInstruction::generateBinaryEncoding()
   {
   // Error Checking
   TR::Register* r1Reg = getRegisterOperand(1);
   TR::Register* v2Reg = getRegisterOperand(2);
   TR_ASSERT(r1Reg != NULL, "First Operand should not be NULL!");
   TR_ASSERT(v2Reg != NULL, "2nd Operand should not be NULL!");

   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset((void*)cursor, 0, getEstimatedBinaryLength());

   // Copy binary
   getOpCode().copyBinaryToBuffer(instructionStart);

   setMaskField(reinterpret_cast<uint32_t *>(cursor), getM3(), 1);
   setMaskField(reinterpret_cast<uint32_t *>(cursor), getM4(), 2);

   // Operands
   toRealRegister(r1Reg)->setRegister1Field(reinterpret_cast<uint32_t *>(cursor));
   toRealRegister(v2Reg)->setRegister2Field(reinterpret_cast<uint32_t *>(cursor));

   // Cursor move
   // update binary length
   // update binary length estimate error
   cursor += getOpCode().getInstructionLength();
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

/** \details
 *
 * VStorage Instruction get extended mnemonic name
 */
const char *
TR::S390VStorageInstruction::getExtendedMnemonicName()
   {
   if (getOpCodeBuffer())
      return getOpCodeBuffer();

   char tmpOpCodeBuffer[16];
   strcpy(tmpOpCodeBuffer, getOpCode().getMnemonicName());

   if (getOpCode().hasExtendedMnemonic())
      {
      appendElementSizeMnemonic(tmpOpCodeBuffer, getMaskField());
      setPrintMaskField(false);
      }

   return setOpCodeBuffer(tmpOpCodeBuffer);
   }

/**
 * VStorage Instruction generate binary encoding
 *
 * Generate binary encoding for most vector register-storage formats: VRS, VRV, and VRX
 * VSI, VRV, and VRS-d have their own implementations and are not handled here
 */
uint8_t *
TR::S390VStorageInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset(static_cast<void*>(cursor), 0, getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   // For large disp scenarios the memref could insert new inst
   if (getMemoryReference() != NULL)
      {
      padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
      cursor += padding;
      }

   // Overlay the instruction
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // Overlay operands
   if(getRegisterOperand(1))
      {
      toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));
      }
   if(getRegisterOperand(2))
      {
      toRealRegister(getRegisterOperand(2))->setRegister2Field(reinterpret_cast<uint32_t*>(cursor));
      }

   // Mask
   setMaskField(reinterpret_cast<uint32_t*>(cursor), _maskField, 3);

   instructionStart = cursor;
   cursor += (getOpCode().getInstructionLength());

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

/**
 * VStorageInstruction  estimateBinaryLength
*/
int32_t
TR::S390VStorageInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

/**** VRS variants ***/
/** \details
 *
 * VRS-d generate binary encoding implementation.
 * Differs from its base class's generic VStorage instruction implementation
 */
uint8_t *
TR::S390VRSdInstruction::generateBinaryEncoding()
   {
    TR::MemoryReference* memRef = getMemoryReference();
    TR::Register* v1Reg = getOpCode().isStore() ? getRegisterOperand(2) : getRegisterOperand(1);
    TR::Register* r3Reg = getOpCode().isStore() ? getRegisterOperand(1) : getRegisterOperand(2);

    TR_ASSERT(v1Reg  != NULL, "V1 in VRS-d should not be NULL!");
    TR_ASSERT(r3Reg  != NULL, "R3 in VRS-d should not be NULL!");
    TR_ASSERT(memRef != NULL, "Memory Ref in VRS-d should not be NULL!");

    uint8_t * instructionStart = cg()->getBinaryBufferCursor();
    uint8_t * cursor = instructionStart;
    memset((void*)cursor, 0, getEstimatedBinaryLength());
    int32_t padding = 0, longDispTouchUpPadding = 0;
    TR::Compilation *comp = cg()->comp();

    // For large disp scenarios the memref could insert new inst
    padding = memRef->generateBinaryEncoding(cursor, cg(), this);
    cursor += padding;

    // Overlay the instruction
    getOpCode().copyBinaryToBufferWithoutClear(cursor);

    // Overlay register operands
    toRealRegister(r3Reg)->setRegister2Field(reinterpret_cast<uint32_t*>(cursor));
    toRealRegister(v1Reg)->setRegister4Field(reinterpret_cast<uint32_t*>(cursor));

    instructionStart = cursor;
    cursor += (getOpCode().getInstructionLength());

    // Finish patching up if long disp was needed
    longDispTouchUpPadding = memRef->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);

    setBinaryLength(cursor - instructionStart);
    setBinaryEncoding(instructionStart);
    cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

    return cursor;
   }

/** \details
 *
 * VRV format generate binary encoding
 * Differs from its base class's generic VStorage instruction implementation
 *
 */
uint8_t *
TR::S390VRVInstruction::generateBinaryEncoding()
   {
   TR_ASSERT(getRegisterOperand(1) != NULL, "1st Operand should not be NULL!");
   TR_ASSERT(getRegisterOperand(2) != NULL, "2nd Operand should not be NULL!");

   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset(static_cast<void*>(cursor), 0, getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   // For large disp scenarios the memref could insert new inst
   if (getMemoryReference() != NULL)
      {
      padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
      cursor += padding;
      }

   // Overlay the instruction
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // Overlay operands
   // VRV target and source registers are the same. so don't handle operand(2)
   toRealRegister(getRegisterOperand(1))->setRegister1Field(reinterpret_cast<uint32_t*>(cursor));

   // Mask
   setMaskField(reinterpret_cast<uint32_t*>(cursor), _maskField, 3);

   instructionStart = cursor;
   cursor += (getOpCode().getInstructionLength());

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

/** \details
 *
 * VSI format generate binary encoding implementation
 * Differs from its base class's generic VStorage instruction implementation
*/
uint8_t *
TR::S390VSIInstruction::generateBinaryEncoding()
   {
   TR::MemoryReference* memRef = getMemoryReference();
   TR::Register* v1Reg = getRegisterOperand(1);

   TR_ASSERT(v1Reg  != NULL, "Operand V1 should not be NULL!");
   TR_ASSERT(memRef != NULL, "Memory Ref should not be NULL!");

   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset((void*)cursor, 0, getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   // For large disp scenarios the memref could insert new inst
   padding = memRef->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the instruction
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // Immediate I3
   *(cursor + 1) |= bos(static_cast<uint8_t>(getImmediateField3()));

   // Operand 4: get V1 and put it at the 4th operand field
   toRealRegister(v1Reg)->setRegister4Field(reinterpret_cast<uint32_t*>(cursor));

   instructionStart = cursor;
   cursor += (getOpCode().getInstructionLength());

   // Finish patching up if long disp was needed
   longDispTouchUpPadding = memRef->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390MemMemInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
int32_t
TR::S390MemMemInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   int32_t length = 0;
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390MemMemInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   TR::Compilation *comp = cg()->comp();

   memset(static_cast<void*>(cursor), 0, getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;

   // generate binary for first memory reference operand
   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // generate binary for second memory reference operand
   padding = getMemoryReference2()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the actual instruction op and length
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference())
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
   if (getMemoryReference2())
      longDispTouchUpPadding += getMemoryReference2()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390SS1Instruction:: member functions
////////////////////////////////////////////////////////////////////////////////
int32_t
TR::S390SS1Instruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   int32_t length = 0;
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390SS1Instruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   TR::Compilation *comp = cg()->comp();

   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;

   // generate binary for first memory reference operand
   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;
   // generate binary for second memory reference operand
   padding = getMemoryReference2()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // If we spilled the 2nd memref, than we need to ensure the base register field and the displacement
   // for the first memref is re-written to the now changed cursor
   if (getLocalLocalSpillReg2())
      {
      toRealRegister(getMemoryReference()->getBaseRegister())->setBaseRegisterField((uint32_t *)cursor);
      *(uint32_t *)cursor &= boi(0xFFFFF000); // Clear out the memory first
      *(uint32_t *)cursor |= boi(getMemoryReference()->getOffset() & 0x00000FFF);
      }

   // Overlay the actual instruction op and length
   getOpCode().copyBinaryToBufferWithoutClear(cursor);
   // generate binary for the length field
   TR_ASSERT(getLen() >= 0 && getLen() <= 255,"SS1 L must be in the range 0->255 and not %d on inst %p\n",getLen(),this);
   (*(uint32_t *) cursor) &= boi(0xFF00FFFF);
   (*(uint8_t *) (cursor + 1)) |= getLen();

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   longDispTouchUpPadding  = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
   longDispTouchUpPadding += getMemoryReference2()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);

   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390SS2Instruction:: member functions
////////////////////////////////////////////////////////////////////////////////
uint8_t *
TR::S390SS2Instruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   // generate binary for first memory reference operand
   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   if (getMemoryReference2())
      {
      // generate binary for second memory reference operand
      padding = getMemoryReference2()->generateBinaryEncoding(cursor, cg(), this);
      cursor += padding;
      }
   else
      {
      (*(int16_t *) (cursor + 4)) = bos(getShiftAmount() & 0xFFF);
      }

   // If we spilled the 2nd memref, than we need to ensure the base register field and the displacement
   // for the first memref is re-written to the now changed cursor
   if (getLocalLocalSpillReg2())
      {
      toRealRegister(getMemoryReference()->getBaseRegister())->setBaseRegisterField((uint32_t *)cursor);
      *(uint32_t *)cursor &= boi(0xFFFFF000); // Clear out the memory first
      *(uint32_t *)cursor |= boi(getMemoryReference()->getOffset() & 0x00000FFF);
      }

   // Overlay the actual instruction op and lengths
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // generate binary for the length field
   TR_ASSERT(getLen() >= 0 && getLen() <= 15,"SS2 L1 must be in the range 0->15 and not %d from [%p]\n",getLen(), getNode());
   TR_ASSERT(getLen2() >= 0 && getLen2() <= 15,"SS2 L2 must be in the range 0->15 and not %d\n",getLen2());
   (*(uint32_t *) cursor) &= boi(0xFF00FFFF);
   uint16_t lenField = (getLen() << 4) | getLen2();
   (*(uint8_t *) (cursor + 1)) |= lenField;

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
   if (getMemoryReference2())
      longDispTouchUpPadding += getMemoryReference2()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390SS2Instruction:: member functions
////////////////////////////////////////////////////////////////////////////////
uint8_t *
TR::S390SS4Instruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   // generate binary for first memory reference operand
   if(getMemoryReference())
      {
      padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
      cursor += padding;
      }

   // generate binary for second memory reference operand
   if(getMemoryReference2())
      {
      padding = getMemoryReference2()->generateBinaryEncoding(cursor, cg(), this);
      cursor += padding;
      }

   // Overlay the actual instruction op and lengths
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // generate binary for the length and source key registers
   if(getLengthReg())
      toRealRegister(getRegForBinaryEncoding(getLengthReg()))->setRegisterField((uint32_t *)cursor, 5);
   if(getSourceKeyReg())
      toRealRegister(getRegForBinaryEncoding(getSourceKeyReg()))->setRegisterField((uint32_t *)cursor, 4);

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference())
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
   if (getMemoryReference2())
      longDispTouchUpPadding += getMemoryReference2()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390SSFInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
bool
TR::S390SSFInstruction::refsRegister(TR::Register * reg)
   {
   // 64bit mem refs clobber high word regs
   if (reg->getKind() != TR_FPR && reg->getKind() != TR_VRF && getMemoryReference()&& reg->getRealRegister())
      {
      TR::RealRegister * realReg = (TR::RealRegister *)reg;
      TR::Register * regMem = reg;

      if (getMemoryReference()->refsRegister(regMem))
         {
         return true;
         }
      if (getMemoryReference2()->refsRegister(regMem))
         {
         return true;
         }
      }
   if (matchesTargetRegister(reg))
      {
      return true;
      }
   else if ((getMemoryReference() != NULL) && (getMemoryReference()->refsRegister(reg) == true))
      {
      return true;
      }
   else if ((getMemoryReference2() != NULL) && (getMemoryReference2()->refsRegister(reg) == true))
      {
      return true;
      }
   else if (getDependencyConditions())
      {
      return getDependencyConditions()->refsRegister(reg);
      }
   return false;
   }

int32_t
TR::S390SSFInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390SSFInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   getOpCode().copyBinaryToBuffer(instructionStart);

   // encode target register
   toRealRegister(getFirstRegister())->setRegister1Field((uint32_t *) cursor);

   // generate binary for first memory reference operand
   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // generate binary for second memory reference operand
   padding = getMemoryReference2()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference())
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
   if (getMemoryReference2())
      longDispTouchUpPadding += getMemoryReference2()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390RSLInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
int32_t
TR::S390RSLInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390RSLInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the actual instruction and reg
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   // generate binary for the length field
   TR_ASSERT(getLen() <= 0xf,"length %d is too large to encode in RSL instruction\n",getLen());
   (*(uint32_t *) cursor) &= boi(0xFF0FFFFF);
   (*(uint8_t *) (cursor + 1)) |= (getLen() << 4);

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390RSLbInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
int32_t
TR::S390RSLbInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390RSLbInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the actual instruction
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   instructionStart = cursor;

   // generate binary for the length field
   TR_ASSERT(getLen() <= UCHAR_MAX,"length %d is too large to encode in RSLb instruction\n",getLen());
   (*(uint32_t *) cursor) &= boi(0xFF00FFFF);
   (*(uint8_t *) (cursor + 1)) |= (getLen()&UCHAR_MAX);

   cursor += 2;

   TR_ASSERT(getRegisterOperand(1),"expecting a reg for RSLb instruction op %d\n",getOpCodeValue());

   TR::Register *reg = getRegForBinaryEncoding(getRegisterOperand(1));
   toRealRegister(reg)->setBaseRegisterField((uint32_t *)cursor);

   cursor += 2;

   TR_ASSERT(getMask() <= 0xf,"mask %d is too large to encode in RSLb instruction\n",getMask());

   (*(uint16_t *) cursor) &= bos(0xF0FF);
   (*(uint8_t *) (cursor)) |= (getMask()&0xf);

   cursor += 2;

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390SIInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
int32_t
TR::S390SIInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390SIInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   TR::InstOpCode& opCode = getOpCode();

   if (getMemoryReference()->isLongDisplacementRequired())
      {
      auto longDisplacementMnemonic = TR::InstOpCode::getEquivalentLongDisplacementMnemonic(getOpCodeValue());

      if (longDisplacementMnemonic != TR::InstOpCode::BAD)
         {
         opCode = TR::InstOpCode(longDisplacementMnemonic);
         }
      }

   opCode.copyBinaryToBufferWithoutClear(cursor);

   (*(uint32_t *) cursor) &= boi(0xFF00FFFF);
   (*(uint8_t *) (cursor + 1)) |= getSourceImmediate();

   instructionStart = cursor;
   cursor += opCode.getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// LL: TR::S390SIYInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
int32_t
TR::S390SIYInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390SIYInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   (*(uint32_t *) cursor) &= boi(0xFF00FFFF);
   (*(uint8_t *) (cursor + 1)) |= getSourceImmediate();

   instructionStart = cursor;
   cursor += (getOpCode().getInstructionLength());

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390SILInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

int32_t
TR::S390SILInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

/**
 *      SIL Format
 *    ________________ ____ ____________ ________________
 *   | Op Code        | B1 |    D1      |   I2           |
 *   |________________|____|____________|________________|
 *   0               16    20           32              47
 */
uint8_t *
TR::S390SILInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the actual instruction and reg
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   (*(int16_t *) (cursor + 4)) |= bos((int16_t) getSourceImmediate());

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390SInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
int32_t
TR::S390SInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   if (getMemoryReference() != NULL)
      {
      return getMemoryReference()->estimateBinaryLength(currentEstimate, cg(), this);
      }
   else
      {
      return TR::Instruction::estimateBinaryLength(currentEstimate);
      }
   }

uint8_t *
TR::S390SInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   int32_t padding = 0, longDispTouchUpPadding = 0;
   TR::Compilation *comp = cg()->comp();

   padding = getMemoryReference()->generateBinaryEncoding(cursor, cg(), this);
   cursor += padding;

   // Overlay the actual instruction and reg
   getOpCode().copyBinaryToBufferWithoutClear(cursor);

   instructionStart = cursor;
   cursor += getOpCode().getInstructionLength();

   // Finish patching up if long disp was needed
   if (getMemoryReference() != NULL)
      {
      longDispTouchUpPadding = getMemoryReference()->generateBinaryEncodingTouchUpForLongDisp(cursor, cg(), this);
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength() - padding - longDispTouchUpPadding);

   return cursor;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390NOPInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

int32_t
TR::S390NOPInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   // could have 2 byte, 4 byte or 6 byte NOP
   setEstimatedBinaryLength(6);

   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *
TR::S390NOPInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   TR::Compilation *comp = cg()->comp();

   if (getBinaryLength() == 2)
      {
      uint16_t nopInst;
         nopInst = 0x1800;
      (*(uint16_t *) cursor) = bos(nopInst);
      cursor += 2;
      }
   else if (getBinaryLength() == 4)
      {
      uint32_t nopInst = 0x47000000;
      (*(uint32_t *) cursor) = boi(nopInst);
      cursor += 4;
      }
   else if(getBinaryLength() == 6)
      {
      uint32_t nopInst1 = 0xc0040000;
      uint16_t nopInst2 = 0x0;
      *(uint32_t *) cursor = boi(nopInst1);
      *(uint16_t *)(cursor+4) = bos(nopInst2);
      cursor += 6;
      }
   else
      {
      TR_ASSERT(0, "TODO - NOP other than 2, 4 or 6 bytes not supported yet \n");
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

int32_t
TR::S390AlignmentNopInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   self()->setEstimatedBinaryLength(_alignment - 2);
   return currentEstimate + self()->getEstimatedBinaryLength();
   }

uint8_t*
TR::S390AlignmentNopInstruction::generateBinaryEncoding()
   {
   bool trace = cg()->comp()->getOption(TR_TraceCG);
   uint32_t currentMisalign = reinterpret_cast<uintptr_t>(cg()->getBinaryBufferCursor()) % _alignment;

   if (currentMisalign != 0)
      {
      uint32_t nopsOfLength6ToAdd = (_alignment - currentMisalign) / 6;
      uint32_t nopsOfLength4ToAdd = ((_alignment - currentMisalign) % 6) / 4;
      uint32_t nopsOfLength2ToAdd = (((_alignment - currentMisalign) % 6) % 4) / 2;
      
      if (trace)
         traceMsg(cg()->comp(), "Expanding alignment nop %p into %u instructions: [ ", self(), nopsOfLength6ToAdd + nopsOfLength4ToAdd + nopsOfLength2ToAdd);

      for (uint32_t i = 0; i < nopsOfLength2ToAdd; ++i)
         {
         TR::Instruction *nop = new (cg()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, getNode(), self(), cg());

         if (trace)
            traceMsg(cg()->comp(), "%p ", nop);
         }

      for (uint32_t i = 0; i < nopsOfLength4ToAdd; ++i)
         {
         TR::Instruction *nop = new (cg()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 4, getNode(), self(), cg());

         if (trace)
            traceMsg(cg()->comp(), "%p ", nop);
         }
      
      for (uint32_t i = 0; i < nopsOfLength6ToAdd; ++i)
         {
         TR::Instruction *nop = new (cg()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 6, getNode(), self(), cg());

         if (trace)
            traceMsg(cg()->comp(), "%p ", nop);
         }

      if (trace)
         traceMsg(cg()->comp(), "]\n");
      }
   else
      {
      if (trace)
         traceMsg(cg()->comp(), "Eliminating alignment nop %p, since the next instruction is already aligned\n", self());
      }

   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - currentMisalign);

   // When the trace log prints the list of instructions after binary encoding, we don't want this
   // instruction to show up any more. Removing it from the linked list of instructions does this
   // without affecting this instruction's next pointer, so the binary encoding loop can continue
   // and encode the actual nops we emitted as if nothing happened.
   self()->remove();

   return cg()->getBinaryBufferCursor();
   }

// ////////////////////////////////////////////////////////////////////////////////
// TR::S390MIIInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

/**
 * MII Format
 *
 *     ________ ____ _______________ _________________________
 *    |Op Code | M1 |      RI2      |           RI3           |
 *    |________|____|_______________|_________________________|
 *    0         8   12              24                       47
 */
int32_t
TR::S390MIIInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
   //TODO:for now use instruction length (for case 0(R14), later need to add support for longer disposition
   //potentially reuse Memoryreference->estimateBinaryLength

   return TR::Instruction::estimateBinaryLength(currentEstimate);
   }

uint8_t *
TR::S390MIIInstruction::generateBinaryEncoding()
   {

   // acquire the current cursor location so we can start generating our
   // instruction there.
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();

   // we'll need to keep the start location, and our current writing location.
   uint8_t * cursor = instructionStart;

   int distance = (cg()->getBinaryBufferStart() + getLabelSymbol()->getEstimatedCodeLocation()) -
         (cursor + cg()->getAccumulatedInstructionLengthError());

   static bool bpp = (feGetEnv("TR_BPPNOBINARY")!=NULL);
   intptr_t offset = (intptr_t) getSymRef()->getMethodAddress() - (intptr_t) instructionStart;

   if (distance >= MIN_12_RELOCATION_VAL && distance <= MAX_12_RELOCATION_VAL && !bpp &&
         offset >= MIN_24_RELOCATION_VAL && offset <= MAX_24_RELOCATION_VAL)
      {
      // clear the number of bytes we intend to use in the buffer.
      memset((void*)cursor, 0, getEstimatedBinaryLength());

      // overlay the actual instruction op code to the buffer.
      getOpCode().copyBinaryToBuffer(cursor);

      // advance the cursor one byte.
      cursor += 1;

      //add relocation 12-24 bits
      //TODO: later add TR::12BitInstructionRelativeRelocation
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative12BitRelocation(cursor, getLabelSymbol(),false));

      // add the comparison mask for the instruction to the buffer.
      *(int16_t *) cursor |= ((uint16_t)getMask() << 12);

      // advance the cursor past the mask encoding and  part of RI2 (relocation) (cursor at 16th bit)
      cursor += 1;

      // add RI3 24-47 # of halfword from current or memory
      //TODO need a check too

      intptr_t offset = (intptr_t) getSymRef()->getMethodAddress() - (intptr_t) instructionStart;

      int32_t offsetInHalfWords = (int32_t) (offset/2);

      //mask out first 8 bits
      offsetInHalfWords &= 0x00ffffff;

      //  *(cursor) = (offsetInHalfWords << 8);
      *(int32_t *) (cursor) |= offsetInHalfWords;

      cursor += 4;
      }

   // set the binary length of our instruction
   setBinaryLength(cursor - instructionStart);

   // set the binary encoding to point at where our instruction begins in the
   // buffer
   setBinaryEncoding(instructionStart);

   // account for the error in estimation
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());

   // return the the cursor for the next instruction.
   return cursor;
   }

// ////////////////////////////////////////////////////////////////////////////////
// TR::S390SMIInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
/**
 *    SMI Format
 *     ________ ____ ____ ____ ____________ ___________________
 *    |Op Code | M1 |    | B3 |     D3     |        RI2        |
 *    |________|____|____|____|____________|___________________|
 *    0         8   12   16   20           32        40       47
 */
int32_t
TR::S390SMIInstruction::estimateBinaryLength(int32_t  currentEstimate)
   {
  //TODO:for now use instruction length (for case 0(R14), later need to add support for longer disposition
  //potentially reuse Memoryreference->estimateBinaryLength

   return TR::Instruction::estimateBinaryLength(currentEstimate);
   }

uint8_t *
TR::S390SMIInstruction::generateBinaryEncoding()
   {

   // acquire the current cursor location so we can start generating our
   // instruction there.
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();

   // we'll need to keep the start location, and our current writing location.
   uint8_t * cursor = instructionStart;

   int distance = (cg()->getBinaryBufferStart() + getLabelSymbol()->getEstimatedCodeLocation()) -
       (cursor + cg()->getAccumulatedInstructionLengthError());

   static bool bpp = (feGetEnv("TR_BPPNOBINARY")!=NULL);

   if (distance >= MIN_RELOCATION_VAL && distance <= MAX_RELOCATION_VAL && !bpp)
      {
      // clear the number of bytes we intend to use in the buffer.
      memset((void*)cursor, 0, getEstimatedBinaryLength());

      // overlay the actual instruction op code to the buffer.
      getOpCode().copyBinaryToBuffer(cursor);

      // advance the cursor one byte.
      cursor += 1;

      // add the comparison mask for the instruction to the buffer.
      *(cursor) |= ((uint8_t)getMask() << 4);

      // advance the cursor past the mask encoding.
      cursor += 1;

      // add the memory reference for the target to the buffer, and advance the
      // cursor past what gets written.
      //TODO: make sure MemoryReference()->generateBinaryEncoding handles it correctly
      int32_t padding = getMemoryReference()->generateBinaryEncoding(instructionStart, cg(), this);
      //TODO: fix for when displacement is too big and need an extra instruction cursor += padding;
      cursor += 2;
      // add relocation
      //TODO: later add TR::16BitInstructionRelativeRelocation
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(cursor, getLabelSymbol(), 4, true));

      // advance the cursor past the immediate.
      cursor += 2;
      }

   // set the binary length of our instruction
   setBinaryLength(cursor - instructionStart);

   // set the binary encoding to point at where our instruction begins in the
   // buffer
   setBinaryEncoding(instructionStart);

   // account for the error in estimation
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());

   // return the the cursor for the next instruction.
   return cursor;
   }

#ifdef J9_PROJECT_SPECIFIC
/**
 * VirtualGuardNOPInstruction is a pseudo instruction that generates a BRC or BRCL
 * This branch will be to a label.  It is assumed it is within range of a BRC on a G5--
 * previous checking will ensure this.  If on a freeway, we are free to choose either
 * BRC or BRCL depending on the range.
 */
uint8_t *
TR::S390VirtualGuardNOPInstruction::generateBinaryEncoding()
   {
   uint8_t * instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol * label = getLabelSymbol();
   uint8_t * cursor = instructionStart;
   memset( (void*)cursor,0,getEstimatedBinaryLength());
   uint16_t binOpCode;
   intptr_t distance;
   bool doRelocation;
   bool shortRelocation = false;
   bool longRelocation = false;
   uint8_t * relocationPoint = NULL;
   TR::Compilation *comp = cg()->comp();
   TR::Instruction *guardForPatching = cg()->getVirtualGuardForPatching(this);

   // a previous guard is patching to the same destination and we can recycle the patch
   // point so setup the patching location to use this previous guard and generate no
   // instructions ourselves
   if (guardForPatching != this)
      {
      _site->setLocation(guardForPatching->getBinaryEncoding());
      setBinaryLength(0);
      setBinaryEncoding(cursor);
      if (label->getCodeLocation() == NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation((uint8_t *) (&_site->getDestination()), label));
         }
      else
         {
         _site->setDestination(label->getCodeLocation());
         }
      cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
      return cursor;
      }

   _site->setLocation(cursor);
   if (label->getCodeLocation() != NULL)
      {
      // Label location is known--doubt this will ever occur, but for completeness....
      // calculate the relative branch distance
      distance = (label->getCodeLocation() - cursor);
      doRelocation = false;
      _site->setDestination(label->getCodeLocation());
#ifdef DEBUG
      if (debug("traceVGNOP"))
         {
         printf("####> virtual location = %p, label location = %p\n", cursor, label->getCodeLocation());
         }
#endif
      }
   else
      {
      // Label location is unknown
      // estimate the relative branch distance
      _site->setDestination(cursor);
      distance = (cg()->getBinaryBufferStart() + label->getEstimatedCodeLocation()) -
         cursor -
         cg()->getAccumulatedInstructionLengthError();

      //bool brcRangeExceeded = (distance<MIN_IMMEDIATE_VAL || distance>MAX_IMMEDIATE_VAL);

      AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", (uint8_t *) (&_site->getDestination()));
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation((uint8_t *) (&_site->getDestination()), label));

      doRelocation = true;
#ifdef DEBUG
      if (debug("traceVGNOP"))
         {
         printf("####> virtual location(est) = %p+%d, label (relocation) = %p\n", cursor, distance, label);
         }
#endif
      }

   /**
    * Z supports two types of patching:
    *   1.  NOP branch is inserted.  When assumption is triggered, the condition
    *       nibble is changed from 0x0 to 0xf.
    *   2.  No branch instruction is inserted.  When assumption is triggered, a
    *       full taken branch is patched in.
    */

   // We can only do the empty patch (i.e. no instrs emitted, and a full taken patch is written
   // in) if the patching occurs during GC pause times.  The patching of up to 6-bytes is potentially
   // not atomic.

   bool performEmptyPatch = getNode()->isStopTheWorldGuard() || getNode()->isProfiledGuard();

   // Stop the world guards that are merged with profiled guards never need to generate NOPs for patching because
   // the profiled guard will generate the NOP branch to the same location the stop the world guard needs to branch
   // to, so we can always use the NOP branch form the profiled guard as our stop the world patch point.

   int32_t sumEstimatedBinaryLength = getNode()->isProfiledGuard() ? 6 : 0;

   if (performEmptyPatch)
      {
      // so at this point we know we are an stop the world guard and that there may be other stop the world guards
      // after us that will use us as their patch point. We now calculate how much space we
      // are sure to have to overwrite to inform us about what to do next
      TR::Node *firstBBEnd = NULL;
      for (TR::Instruction *nextI = getNext(); nextI != NULL; nextI = nextI->getNext())
         {
         if (nextI->isVirtualGuardNOPInstruction())
            {
            // if we encounter a guard that will not use us as a patch point we need to stop
            // counting and get out
            if (cg()->getVirtualGuardForPatching(nextI) != this)
               break;
            // otherwise they are going to use us as patch point so skip them and keep going
            continue;
            }

         if (comp->isPICSite(nextI))
            break;

         // the following instructions can be patched at runtime to set destinations etc
         // and so we cannot include them in the region for an HCR guard to overwrite because
         // the runtime patching could take place after the HCR guard has been patched
         // TODO: BRCL and BRASL are only patchable under specific conditions - this check can be
         // made more specific to let some cases through
         if (nextI->getOpCodeValue() == TR::InstOpCode::LABEL ||
             nextI->getOpCodeValue() == TR::InstOpCode::BRCL ||
             nextI->getOpCodeValue() == TR::InstOpCode::BRASL ||
             nextI->getOpCodeValue() == TR::InstOpCode::DC ||
             nextI->getOpCodeValue() == TR::InstOpCode::DC2 ||
             nextI->getOpCodeValue() == TR::InstOpCode::LARL)
            break;

         // we shouldn't need to check for PSEUDO instructions, but VGNOP has a non-zero length so
         // skip PSEUDO instructions from length consideration because in practice they will generally
         // be zero
         if (nextI->getOpCode().getInstructionFormat() != PSEUDO)
            sumEstimatedBinaryLength += nextI->getOpCode().getInstructionLength();

         // we never need more than 6 bytes so save time and get out when we have found what we want
         if (sumEstimatedBinaryLength >= 6)
            break;

         TR::Node *node = nextI->getNode();
         if (node && node->getOpCodeValue() == TR::BBEnd)
            {
            if (firstBBEnd == NULL)
               firstBBEnd = node;
            else if (firstBBEnd != node &&
                     (node->getBlock()->getNextBlock() == NULL ||
                      !node->getBlock()->getNextBlock()->isExtensionOfPreviousBlock()))
               break;
            }

         if (node && node->getOpCodeValue()==TR::BBStart && firstBBEnd!=NULL && !node->getBlock()->isExtensionOfPreviousBlock())
            break;
         }
      }

   // If we scanned ahead and see that the estimated binary length is zero (i.e. return point of slow path is right after the guard -
   // methodEnter/Exit is an example of such) we'll just generate the NOP branch.
   if (performEmptyPatch && (sumEstimatedBinaryLength > 0) &&
       performTransformation(comp, "O^O CODE GENERATION: Generating empty patch for guard node %p\n", getNode()))
      {
      doRelocation = false;

      if (sumEstimatedBinaryLength < 6)
         {
         // short branch
         if (distance >= MIN_IMMEDIATE_VAL && distance <= MAX_IMMEDIATE_VAL)
            {
            if (sumEstimatedBinaryLength <= 2)
               {
               *(uint16_t *) cursor = bos(0x1800);
               cursor += 2;
               }
            }
         // long branch
         else
            {
            if (sumEstimatedBinaryLength <= 2)
               {
               *(uint32_t *) cursor = boi(0xa7040000);
               cursor += 4;
               }
            else if (sumEstimatedBinaryLength <= 4)
               {
               *(uint16_t *) cursor = bos(0x1800);
               cursor += 2;
               }
            }
         }
      }
   else if (distance >= MIN_IMMEDIATE_VAL && distance <= MAX_IMMEDIATE_VAL)
      {
      // if distance is within 32K limit, generate short instruction
      binOpCode = *(uint16_t *) (TR::InstOpCode::getOpCodeBinaryRepresentation(TR::InstOpCode::BRC));
      *(uint16_t *) cursor = bos(binOpCode);
      *(uint16_t *) cursor &= bos(0xff0f); // turn into a nop
      *(uint16_t *) (cursor + 2) |= bos(distance / 2);
      relocationPoint = cursor + 2;
      shortRelocation = true;
      cursor += TR::InstOpCode::getInstructionLength(TR::InstOpCode::BRC);
      }
   else
      {
      // Since N3 and up, generate BRCL instruction
      binOpCode = *(uint16_t *) (TR::InstOpCode::getOpCodeBinaryRepresentation(TR::InstOpCode::BRCL));
      *(uint16_t *) cursor = bos(binOpCode);
      *(uint16_t *) cursor &= bos(0xff0f); // turn into a nop
      *(uint32_t *) (cursor + 2) |= boi(distance / 2);
      longRelocation = true;
      relocationPoint = cursor;
      setOpCodeValue(TR::InstOpCode::BRCL);
      cursor += TR::InstOpCode::getInstructionLength(TR::InstOpCode::BRCL);
      }

   // for debugging--a branch instead of NOP
   static char * forceBranch = feGetEnv("TR_FORCEVGBR");
   if (NULL != forceBranch)
      {
      *(int16_t *) (instructionStart) |= bos(0x00f0);
      }

   if (doRelocation)
      {
      if (shortRelocation)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative16BitRelocation(relocationPoint, label));
         }
      else
         {
         TR_ASSERT( longRelocation, "how did I get here??\n");
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(relocationPoint, label));
         }
      }

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }
#endif
