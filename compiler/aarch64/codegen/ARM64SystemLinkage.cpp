/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "codegen/ARM64SystemLinkage.hpp"

#include "codegen/ARM64Instruction.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/MemoryReference.hpp"
#include "il/symbol/AutomaticSymbol.hpp"


TR::ARM64SystemLinkage::ARM64SystemLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   int i;

   _properties._properties = IntegersInRegisters|FloatsInRegisters|RightToLeft;

   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::x0]    = IntegerReturn|IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x1]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x2]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x3]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x4]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x5]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x6]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x7]    = IntegerArgument;

   for (i = TR::RealRegister::x8; i <= TR::RealRegister::x15; i++)
      _properties._registerFlags[i] = 0; // x8 - x15

   _properties._registerFlags[TR::RealRegister::x16]   = ARM64_Reserved; // IP0
   _properties._registerFlags[TR::RealRegister::x17]   = ARM64_Reserved; // IP1

   for (i = TR::RealRegister::x18; i <= TR::RealRegister::x28; i++)
      _properties._registerFlags[i] = Preserved; // x18 - x28 Preserved

   _properties._registerFlags[TR::RealRegister::x29]   = ARM64_Reserved; // FP
   _properties._registerFlags[TR::RealRegister::x30]   = ARM64_Reserved; // LR
   _properties._registerFlags[TR::RealRegister::xzr]   = ARM64_Reserved; // zero or SP

   _properties._registerFlags[TR::RealRegister::v0]    = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::v1]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v2]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v3]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v4]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v5]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v6]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v7]    = FloatArgument;

   for (i = TR::RealRegister::v8; i <= TR::RealRegister::v15; i++)
      _properties._registerFlags[i] = Preserved; // v8 - v15 Preserved
   for (i = TR::RealRegister::v16; i <= TR::RealRegister::LastFPR; i++)
      _properties._registerFlags[i] = 0; // v16 - v31

   _properties._numIntegerArgumentRegisters  = 8;
   _properties._firstIntegerArgumentRegister = 0;
   _properties._numFloatArgumentRegisters    = 8;
   _properties._firstFloatArgumentRegister   = 8;

   _properties._argumentRegisters[0]  = TR::RealRegister::x0;
   _properties._argumentRegisters[1]  = TR::RealRegister::x1;
   _properties._argumentRegisters[2]  = TR::RealRegister::x2;
   _properties._argumentRegisters[3]  = TR::RealRegister::x3;
   _properties._argumentRegisters[4]  = TR::RealRegister::x4;
   _properties._argumentRegisters[5]  = TR::RealRegister::x5;
   _properties._argumentRegisters[6]  = TR::RealRegister::x6;
   _properties._argumentRegisters[7]  = TR::RealRegister::x7;
   _properties._argumentRegisters[8]  = TR::RealRegister::v0;
   _properties._argumentRegisters[9]  = TR::RealRegister::v1;
   _properties._argumentRegisters[10] = TR::RealRegister::v2;
   _properties._argumentRegisters[11] = TR::RealRegister::v3;
   _properties._argumentRegisters[12] = TR::RealRegister::v4;
   _properties._argumentRegisters[13] = TR::RealRegister::v5;
   _properties._argumentRegisters[14] = TR::RealRegister::v6;
   _properties._argumentRegisters[15] = TR::RealRegister::v7;

   _properties._firstIntegerReturnRegister = 0;
   _properties._firstFloatReturnRegister   = 1;

   _properties._returnRegisters[0]  = TR::RealRegister::x0;
   _properties._returnRegisters[1]  = TR::RealRegister::v0;

   _properties._numAllocatableIntegerRegisters = 27;
   _properties._numAllocatableFloatRegisters   = 32;

   _properties._methodMetaDataRegister      = TR::RealRegister::NoReg;
   _properties._stackPointerRegister        = TR::RealRegister::sp;
   _properties._framePointerRegister        = TR::RealRegister::x29;

   _properties._numberOfDependencyGPRegisters = 32; // To be determined
   _properties._offsetToFirstParm             = 0; // To be determined
   _properties._offsetToFirstLocal            = 0; // To be determined
   }


const TR::ARM64LinkageProperties&
TR::ARM64SystemLinkage::getProperties()
   {
   return _properties;
   }


void
TR::ARM64SystemLinkage::initARM64RealRegisterLinkage()
   {
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *reg;
   int icount;

   reg = machine->getARM64RealRegister(TR::RealRegister::RegNum::x16); // IP0
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getARM64RealRegister(TR::RealRegister::RegNum::x17); // IP1
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getARM64RealRegister(TR::RealRegister::RegNum::x29); // FP
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getARM64RealRegister(TR::RealRegister::RegNum::x30); // LR
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getARM64RealRegister(TR::RealRegister::RegNum::xzr); // zero or SP
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   // assign "maximum" weight to registers x0-x15
   for (icount = TR::RealRegister::x0; icount <= TR::RealRegister::x15; icount++)
      machine->getARM64RealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers x18-x28
   for (icount = TR::RealRegister::x18; icount <= TR::RealRegister::x28; icount++)
      machine->getARM64RealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers v0-v31
   for (icount = TR::RealRegister::v0; icount <= TR::RealRegister::v31; icount++)
      machine->getARM64RealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);
   }


uint32_t
TR::ARM64SystemLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }


static void mapSingleParameter(TR::ParameterSymbol *parameter, uint32_t &stackIndex)
   {
   auto size = parameter->getSize();
   auto alignment = size <= 4 ? 4 : 8;
   stackIndex = (stackIndex + alignment - 1) & (~(alignment - 1));
   parameter->setParameterOffset(stackIndex);
   stackIndex += size;
   }


void
TR::ARM64SystemLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   TR::Machine *machine = cg()->machine();
   uint32_t stackIndex = 0;
   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol *localCursor = automaticIterator.getFirst();

   // map non-long/double automatics
   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0
          && localCursor->getDataType() != TR::Int64
          && localCursor->getDataType() != TR::Double)
         {
         localCursor->setOffset(stackIndex);
         stackIndex += (localCursor->getSize() + 3) & (~3);
         }
      localCursor = automaticIterator.getNext();
      }

   stackIndex += (stackIndex & 0x4) ? 4 : 0; // align to 8 bytes
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   // map long/double automatics
   while (localCursor != NULL)
      {
      if (localCursor->getDataType() == TR::Int64
          || localCursor->getDataType() == TR::Double)
         {
         localCursor->setOffset(stackIndex);
         stackIndex += (localCursor->getSize() + 7) & (~7);
         }
      localCursor = automaticIterator.getNext();
      }
   method->setLocalMappingCursor(stackIndex);

   // allocate space for preserved registers (x19-x28, v8-v15)
   for (int r = TR::RealRegister::x19; r <= TR::RealRegister::x28; r++)
      {
      TR::RealRegister *rr = machine->getARM64RealRegister((TR::RealRegister::RegNum)r);
      if (rr->getHasBeenAssignedInMethod())
         {
         stackIndex += 8;
         }
      }
   for (int r = TR::RealRegister::v8; r <= TR::RealRegister::v15; r++)
      {
      TR::RealRegister *rr = machine->getARM64RealRegister((TR::RealRegister::RegNum)r);
      if (rr->getHasBeenAssignedInMethod())
         {
         stackIndex += 8;
         }
      }
   stackIndex += 8; // for link register

   /*
    * Because the rest of the code generator currently expects **all** arguments
    * to be passed on the stack, arguments passed in registers must be spilled
    * in the callee frame. To map the arguments correctly, we use two loops. The
    * first maps the arguments that will come in registers onto the callee stack.
    * At the end of this loop, the `stackIndex` is the the size of the frame.
    * The second loop then maps the remaining arguments onto the caller frame.
    */

   int32_t nextIntArgReg = 0;
   int32_t nextFltArgReg = 0;
   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   for (TR::ParameterSymbol *parameter = parameterIterator.getFirst();
        parameter != NULL && (nextIntArgReg < getProperties().getNumIntArgRegs() || nextFltArgReg < getProperties().getNumFloatArgRegs());
        parameter = parameterIterator.getNext())
      {
      switch (parameter->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               nextIntArgReg++;
               mapSingleParameter(parameter, stackIndex);
               }
            else
               {
               nextIntArgReg = getProperties().getNumIntArgRegs() + 1;
               }
            break;
         case TR::Float:
         case TR::Double:
            if (nextFltArgReg < getProperties().getNumFloatArgRegs())
               {
               nextFltArgReg++;
               mapSingleParameter(parameter, stackIndex);
               }
            else
               {
               nextFltArgReg = getProperties().getNumFloatArgRegs() + 1;
               }
            break;
         case TR::Aggregate:
            TR_ASSERT(false, "Function parameters of aggregate types are not currently supported on AArch64.");
            break;
         default:
            TR_ASSERT(false, "Unknown parameter type.");
         }
      }

   // save the stack frame size, aligned to 16 bytes
   stackIndex = (stackIndex + 15) & (~15);
   cg()->setFrameSizeInBytes(stackIndex);

   nextIntArgReg = 0;
   nextFltArgReg = 0;
   parameterIterator.reset();
   for (TR::ParameterSymbol *parameter = parameterIterator.getFirst();
        parameter != NULL && (nextIntArgReg < getProperties().getNumIntArgRegs() || nextFltArgReg < getProperties().getNumFloatArgRegs());
        parameter = parameterIterator.getNext())
      {
      switch (parameter->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               nextIntArgReg++;
               }
            else
               {
               mapSingleParameter(parameter, stackIndex);
               }
            break;
         case TR::Float:
         case TR::Double:
            if (nextFltArgReg < getProperties().getNumFloatArgRegs())
               {
               nextFltArgReg++;
               }
            else
               {
               mapSingleParameter(parameter, stackIndex);
               }
            break;
         case TR::Aggregate:
            TR_ASSERT(false, "Function parameters of aggregate types are not currently supported on AArch64.");
            break;
         default:
            TR_ASSERT(false, "Unknown parameter type.");
         }
      }
   }


void
TR::ARM64SystemLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   int32_t roundedSize = (p->getSize() + 3) & (~3);
   if (roundedSize == 0)
      roundedSize = 4;

   p->setOffset(stackIndex -= roundedSize);
   }


void
TR::ARM64SystemLinkage::createPrologue(TR::Instruction *cursor)
   {
   createPrologue(cursor, comp()->getJittedMethodSymbol()->getParameterList());
   }


void
TR::ARM64SystemLinkage::createPrologue(TR::Instruction *cursor, List<TR::ParameterSymbol> &parmList)
   {
   TR::CodeGenerator *codeGen = cg();
   TR::Machine *machine = codeGen->machine();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::RealRegister *sp = machine->getARM64RealRegister(properties.getStackPointerRegister());
   TR::Node *firstNode = comp()->getStartTree()->getNode();

   // allocate stack space
   uint32_t frameSize = (uint32_t)codeGen->getFrameSizeInBytes();
   if (constantIsUnsignedImm12(frameSize))
      {
      cursor = generateTrg1Src1ImmInstruction(codeGen, TR::InstOpCode::subimmx, firstNode, sp, sp, frameSize, cursor);
      }
   else
      {
      TR_ASSERT(false, "Not implemented yet.");
      }

   // save link register (x30)
   TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, bodySymbol->getLocalMappingCursor(), codeGen);
   cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, firstNode, stackSlot, machine->getARM64RealRegister(TR::RealRegister::x30), cursor);

   // spill argument registers
   int32_t nextIntArgReg = 0;
   int32_t nextFltArgReg = 0;
   ListIterator<TR::ParameterSymbol> parameterIterator(&parmList);
   for (TR::ParameterSymbol *parameter = parameterIterator.getFirst();
        parameter != NULL && (nextIntArgReg < getProperties().getNumIntArgRegs() || nextFltArgReg < getProperties().getNumFloatArgRegs());
        parameter = parameterIterator.getNext())
      {
      TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, parameter->getParameterOffset(), codeGen);
      switch (parameter->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, firstNode, stackSlot, machine->getARM64RealRegister((TR::RealRegister::RegNum)(TR::RealRegister::x0 + nextIntArgReg)), cursor);
               nextIntArgReg++;
               }
            else
               {
               nextIntArgReg = getProperties().getNumIntArgRegs() + 1;
               }
            break;
         case TR::Float:
         case TR::Double:
            if (nextFltArgReg < getProperties().getNumFloatArgRegs())
               {
               cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::vstrimmd, firstNode, stackSlot, machine->getARM64RealRegister((TR::RealRegister::RegNum)(TR::RealRegister::v0 + nextFltArgReg)), cursor);
               nextFltArgReg++;
               }
            else
               {
               nextFltArgReg = getProperties().getNumFloatArgRegs() + 1;
               }
            break;
         case TR::Aggregate:
            TR_ASSERT(false, "Function parameters of aggregate types are not currently supported on AArch64.");
            break;
         default:
            TR_ASSERT(false, "Unknown parameter type.");
         }
      }

   // save callee-saved registers
   uint32_t offset = bodySymbol->getLocalMappingCursor() + 8; // +8 for LR
   for (int r = TR::RealRegister::x19; r <= TR::RealRegister::x28; r++)
      {
      TR::RealRegister *rr = machine->getARM64RealRegister((TR::RealRegister::RegNum)r);
      if (rr->getHasBeenAssignedInMethod())
         {
         TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, offset, codeGen);
         cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, firstNode, stackSlot, rr, cursor);
         offset += 8;
         }
      }
   for (int r = TR::RealRegister::v8; r <= TR::RealRegister::v15; r++)
      {
      TR::RealRegister *rr = machine->getARM64RealRegister((TR::RealRegister::RegNum)r);
      if (rr->getHasBeenAssignedInMethod())
         {
         TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, offset, codeGen);
         cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::vstrimmd, firstNode, stackSlot, rr, cursor);
         offset += 8;
         }
      }
   }


void
TR::ARM64SystemLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR::CodeGenerator *codeGen = cg();
   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::Machine *machine = codeGen->machine();
   TR::Node *lastNode = cursor->getNode();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *sp = machine->getARM64RealRegister(properties.getStackPointerRegister());

   // restore callee-saved registers
   uint32_t offset = bodySymbol->getLocalMappingCursor() + 8; // +8 for LR
   for (int r = TR::RealRegister::x19; r <= TR::RealRegister::x28; r++)
      {
      TR::RealRegister *rr = machine->getARM64RealRegister((TR::RealRegister::RegNum)r);
      if (rr->getHasBeenAssignedInMethod())
         {
         TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, offset, codeGen);
         cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, lastNode, rr, stackSlot, cursor);
         offset += 8;
         }
      }
   for (int r = TR::RealRegister::v8; r <= TR::RealRegister::v15; r++)
      {
      TR::RealRegister *rr = machine->getARM64RealRegister((TR::RealRegister::RegNum)r);
      if (rr->getHasBeenAssignedInMethod())
         {
         TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, offset, codeGen);
         cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::vldrimmd, lastNode, rr, stackSlot, cursor);
         offset += 8;
         }
      }

   // restore link register (x30)
   TR::RealRegister *lr = machine->getARM64RealRegister(TR::RealRegister::lr);
   TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, bodySymbol->getLocalMappingCursor(), codeGen);
   cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, lastNode, lr, stackSlot, cursor);

   // remove space for preserved registers
   uint32_t frameSize = codeGen->getFrameSizeInBytes();
   if (constantIsUnsignedImm12(frameSize))
      {
      cursor = generateTrg1Src1ImmInstruction(codeGen, TR::InstOpCode::addimmx, lastNode, sp, sp, frameSize, cursor);
      }
   else
      {
      TR_ASSERT(false, "Not implemented yet.");
      }

   // return
   cursor = generateRegBranchInstruction(codeGen, TR::InstOpCode::ret, lastNode, lr, cursor);
   }


int32_t TR::ARM64SystemLinkage::buildArgs(TR::Node *callNode,
                                       TR::RegisterDependencyConditions *dependencies)

   {
   TR_ASSERT(false, "Not implemented yet.");

   return 0;
   }


TR::Register *TR::ARM64SystemLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR_ASSERT(false, "Not implemented yet.");

   return NULL;
   }


TR::Register *TR::ARM64SystemLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR_ASSERT(false, "Not implemented yet.");

   return NULL;
   }
