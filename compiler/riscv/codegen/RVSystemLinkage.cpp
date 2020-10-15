/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include <riscv.h>
#include "codegen/RVSystemLinkage.hpp"
#include "codegen/RVInstruction.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/Node_inlines.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/ParameterSymbol.hpp"


//getRegisterNumber

#define FOR_EACH_REGISTER(machine, block)                                        \
   for (int regNum = TR::RealRegister::x0; regNum <= TR::RealRegister::x31; regNum++) \
      {                                                                          \
      TR::RealRegister *reg                                                      \
                   = machine->getRealRegister((TR::RealRegister::RegNum)regNum); \
      { block; }                                                                 \
      }                                                                          \
   for (int regNum = TR::RealRegister::f0; regNum <= TR::RealRegister::f31; regNum++) \
      {                                                                          \
      TR::RealRegister *reg                                                      \
                   = machine->getRealRegister((TR::RealRegister::RegNum)regNum); \
      { block; }                                                                 \
      }


#define FOR_EACH_CALLEE_SAVED_REGISTER(machine, block)                           \
   FOR_EACH_REGISTER(machine,                                                    \
   if (_properties._registerFlags[(TR::RealRegister::RegNum)regNum] == Preserved)\
      { block; }                                                                 \
   )

#define FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine, block)                  \
   FOR_EACH_CALLEE_SAVED_REGISTER(machine,                                       \
   if (reg->getHasBeenAssignedInMethod())                                        \
      { block; }                                                                 \
   )

TR::RVSystemLinkage::RVSystemLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   int i;

   _properties._properties = IntegersInRegisters|FloatsInRegisters|RightToLeft;

   _properties._registerFlags[TR::RealRegister::zero] = Preserved|RV_Reserved; // zero
   _properties._registerFlags[TR::RealRegister::ra]   = Preserved|RV_Reserved; // return address
   _properties._registerFlags[TR::RealRegister::sp]   = Preserved|RV_Reserved; // sp
   _properties._registerFlags[TR::RealRegister::gp]   = Preserved|RV_Reserved; // gp
   _properties._registerFlags[TR::RealRegister::tp]   = Preserved|RV_Reserved; // tp

   _properties._registerFlags[TR::RealRegister::t0]   = Preserved|RV_Reserved; // fp
   _properties._registerFlags[TR::RealRegister::t1]   = 0;
   _properties._registerFlags[TR::RealRegister::t2]   = 0;

   _properties._registerFlags[TR::RealRegister::s0]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s1]   = Preserved;

   _properties._registerFlags[TR::RealRegister::a0]   = IntegerArgument | IntegerReturn;
   _properties._registerFlags[TR::RealRegister::a1]   = IntegerArgument | IntegerReturn;
   _properties._registerFlags[TR::RealRegister::a2]   = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::a3]   = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::a4]   = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::a5]   = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::a6]   = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::a7]   = IntegerArgument;

   _properties._registerFlags[TR::RealRegister::s2]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s3]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s4]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s5]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s6]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s7]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s8]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s9]   = Preserved;
   _properties._registerFlags[TR::RealRegister::s10]  = Preserved;
   _properties._registerFlags[TR::RealRegister::s11]  = Preserved;

   _properties._registerFlags[TR::RealRegister::t3]   = 0;
   _properties._registerFlags[TR::RealRegister::t4]   = 0;
   _properties._registerFlags[TR::RealRegister::t5]   = 0;
   _properties._registerFlags[TR::RealRegister::t6]   = 0;

   _properties._registerFlags[TR::RealRegister::ft0]  = 0;
   _properties._registerFlags[TR::RealRegister::ft1]  = 0;
   _properties._registerFlags[TR::RealRegister::ft2]  = 0;
   _properties._registerFlags[TR::RealRegister::ft3]  = 0;
   _properties._registerFlags[TR::RealRegister::ft4]  = 0;
   _properties._registerFlags[TR::RealRegister::ft5]  = 0;
   _properties._registerFlags[TR::RealRegister::ft6]  = 0;
   _properties._registerFlags[TR::RealRegister::ft7]  = 0;

   _properties._registerFlags[TR::RealRegister::fs0]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs1]  = Preserved;

   _properties._registerFlags[TR::RealRegister::fa0]  = FloatArgument | FloatReturn;
   _properties._registerFlags[TR::RealRegister::fa1]  = FloatArgument | FloatReturn;
   _properties._registerFlags[TR::RealRegister::fa2]  = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fa3]  = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fa4]  = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fa5]  = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fa6]  = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fa7]  = FloatArgument;

   _properties._registerFlags[TR::RealRegister::fs2]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs3]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs4]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs5]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs6]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs7]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs8]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs9]  = Preserved;
   _properties._registerFlags[TR::RealRegister::fs10] = Preserved;
   _properties._registerFlags[TR::RealRegister::fs11] = Preserved;
   _properties._registerFlags[TR::RealRegister::ft8]  = 0;
   _properties._registerFlags[TR::RealRegister::ft9]  = 0;
   _properties._registerFlags[TR::RealRegister::ft10] = 0;
   _properties._registerFlags[TR::RealRegister::ft11] = 0;


   _properties._numIntegerArgumentRegisters  = 8;
   _properties._firstIntegerArgumentRegister = 0;

   _properties._argumentRegisters[0]  = TR::RealRegister::a0;
   _properties._argumentRegisters[1]  = TR::RealRegister::a1;
   _properties._argumentRegisters[2]  = TR::RealRegister::a2;
   _properties._argumentRegisters[3]  = TR::RealRegister::a3;
   _properties._argumentRegisters[4]  = TR::RealRegister::a4;
   _properties._argumentRegisters[5]  = TR::RealRegister::a5;
   _properties._argumentRegisters[6]  = TR::RealRegister::a6;
   _properties._argumentRegisters[7]  = TR::RealRegister::a7;

   _properties._firstIntegerReturnRegister = 0;
   _properties._returnRegisters[0]  = TR::RealRegister::a0;
   _properties._returnRegisters[1]  = TR::RealRegister::a1;

   _properties._numFloatArgumentRegisters    = 8;
   _properties._firstFloatArgumentRegister   = 8;

   _properties._argumentRegisters[8+0]  = TR::RealRegister::fa0;
   _properties._argumentRegisters[8+1]  = TR::RealRegister::fa1;
   _properties._argumentRegisters[8+2]  = TR::RealRegister::fa2;
   _properties._argumentRegisters[8+3]  = TR::RealRegister::fa3;
   _properties._argumentRegisters[8+4]  = TR::RealRegister::fa4;
   _properties._argumentRegisters[8+5]  = TR::RealRegister::fa5;
   _properties._argumentRegisters[8+6]  = TR::RealRegister::fa6;
   _properties._argumentRegisters[8+7]  = TR::RealRegister::fa7;

   _properties._firstFloatReturnRegister   = 2;
   _properties._returnRegisters[2+0]  = TR::RealRegister::fa0;
   _properties._returnRegisters[2+1]  = TR::RealRegister::fa1;

   _properties._numAllocatableIntegerRegisters = 7 + 11 + 6; // t0-t6 + s1-s11 + a2-a7
   _properties._numAllocatableFloatRegisters   = 32;

   _properties._methodMetaDataRegister      = TR::RealRegister::NoReg;
   _properties._stackPointerRegister        = TR::RealRegister::sp;
   _properties._framePointerRegister        = TR::RealRegister::s0;

   _properties._numberOfDependencyGPRegisters = 32; // To be determined
   setOffsetToFirstParm(0); // To be determined
   _properties._offsetToFirstLocal            = 0; // To be determined
   }


const TR::RVLinkageProperties&
TR::RVSystemLinkage::getProperties()
   {
   return _properties;
   }


void
TR::RVSystemLinkage::initRVRealRegisterLinkage()
   {
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *reg;
   int icount;

   reg = machine->getRealRegister(TR::RealRegister::RegNum::zero);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::ra);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::sp);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::gp);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::tp);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::s0); // FP
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);


   FOR_EACH_REGISTER(machine, reg->setWeight(0xf000));

   // prefer preserved registers over the rest since they're saved / restored
   // in prologue/epilogue.
   FOR_EACH_CALLEE_SAVED_REGISTER(machine, reg->setWeight(0x0001));
   }

uint32_t
TR::RVSystemLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }


static void mapSingleParameter(TR::ParameterSymbol *parameter, uint32_t &stackIndex, bool inCalleeFrame)
   {
   if (inCalleeFrame)
      {
      auto size = parameter->getSize();
      auto alignment = size <= 4 ? 4 : 8;
      stackIndex = (stackIndex + alignment - 1) & (~(alignment - 1));
      parameter->setParameterOffset(stackIndex);
      stackIndex += size;
      }
   else
      { // in caller's frame -- always 8-byte aligned
      TR_ASSERT((stackIndex & 7) == 0, "Unaligned stack index.");
      parameter->setParameterOffset(stackIndex);
      stackIndex += 8;
      }
   }


void
TR::RVSystemLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   TR::Machine *machine = cg()->machine();
   uint32_t stackIndex = 0;
   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol *localCursor = automaticIterator.getFirst();

   stackIndex = 8; // [sp+0] is for link register

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

   FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine, stackIndex += 8);

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
               mapSingleParameter(parameter, stackIndex, true);
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
               mapSingleParameter(parameter, stackIndex, true);
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
               mapSingleParameter(parameter, stackIndex, false);
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
               mapSingleParameter(parameter, stackIndex, false);
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
TR::RVSystemLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   int32_t roundedSize = (p->getSize() + 3) & (~3);
   if (roundedSize == 0)
      roundedSize = 4;

   p->setOffset(stackIndex -= roundedSize);
   }


void
TR::RVSystemLinkage::createPrologue(TR::Instruction *cursor)
   {
   createPrologue(cursor, comp()->getJittedMethodSymbol()->getParameterList());
   }


void
TR::RVSystemLinkage::createPrologue(TR::Instruction *cursor, List<TR::ParameterSymbol> &parmList)
   {
   TR::CodeGenerator *cg = cg();
   TR::Machine *machine = cg->machine();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   const TR::RVLinkageProperties& properties = getProperties();
   TR::RealRegister *sp = machine->getRealRegister(properties.getStackPointerRegister());
   TR::RealRegister *ra = machine->getRealRegister(TR::RealRegister::ra);
   TR::Node *firstNode = comp()->getStartTree()->getNode();

   // allocate stack space
   uint32_t frameSize = (uint32_t)cg->getFrameSizeInBytes();
   if (VALID_ITYPE_IMM(frameSize))
      {
      cursor = generateITYPE(TR::InstOpCode::_addi, firstNode, sp, sp, -frameSize, cg, cursor);
      }
   else
      {
      TR_UNIMPLEMENTED();
      }

   // save link register (ra)
   if (machine->getLinkRegisterKilled())
      {
      TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, 0, cg);
      cursor = generateSTORE(TR::InstOpCode::_sd, firstNode, stackSlot, ra, cg, cursor);
      }

   // spill argument registers
   int32_t nextIntArgReg = 0;
   int32_t nextFltArgReg = 0;
   ListIterator<TR::ParameterSymbol> parameterIterator(&parmList);
   for (TR::ParameterSymbol *parameter = parameterIterator.getFirst();
        parameter != NULL && (nextIntArgReg < getProperties().getNumIntArgRegs() || nextFltArgReg < getProperties().getNumFloatArgRegs());
        parameter = parameterIterator.getNext())
      {
      TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, parameter->getParameterOffset(), cg);
      TR::InstOpCode::Mnemonic op;

      switch (parameter->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               op = (parameter->getSize() == 8) ? TR::InstOpCode::_sd : TR::InstOpCode::_sw;
               cursor = generateSTORE(op, firstNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)(TR::RealRegister::a0 + nextIntArgReg)), cg, cursor);
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
               op = (parameter->getSize() == 8) ? TR::InstOpCode::_fsd : TR::InstOpCode::_fsw;
               cursor = generateSTORE(op, firstNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)(TR::RealRegister::fa0 + nextFltArgReg)), cg, cursor);
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
   uint32_t offset = bodySymbol->getLocalMappingCursor();
   FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine,
      TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, offset, cg);
      cursor = generateSTORE(TR::InstOpCode::_sd, firstNode, stackSlot, reg, cg(), cursor);
      offset += 8;)
   }


void
TR::RVSystemLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR::CodeGenerator *cg = cg();
   const TR::RVLinkageProperties& properties = getProperties();
   TR::Machine *machine = cg->machine();
   TR::Node *lastNode = cursor->getNode();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *sp = machine->getRealRegister(properties.getStackPointerRegister());
   TR::RealRegister *zero = machine->getRealRegister(TR::RealRegister::zero);

   // restore callee-saved registers
   uint32_t offset = bodySymbol->getLocalMappingCursor();
   FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine,
      TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, offset, cg);
      cursor = generateLOAD(TR::InstOpCode::_ld, lastNode, reg, stackSlot, cg(), cursor);
      offset += 8;)

   // restore link register (x30)
   TR::RealRegister *ra = machine->getRealRegister(TR::RealRegister::ra);
   if (machine->getLinkRegisterKilled())
      {
      TR::MemoryReference *stackSlot = new (trHeapMemory()) TR::MemoryReference(sp, 0, cg);
      cursor = generateLOAD(TR::InstOpCode::_ld, lastNode, ra, stackSlot, cg(), cursor);
      }

   // remove space for preserved registers
   uint32_t frameSize = cg->getFrameSizeInBytes();
   if (VALID_ITYPE_IMM(frameSize))
      {
      cursor = generateITYPE(TR::InstOpCode::_addi, lastNode, sp, sp, frameSize, cg, cursor);
      }
   else
      {
      TR_UNIMPLEMENTED();
      }

   // return
   cursor = generateITYPE(TR::InstOpCode::_jalr, lastNode, zero, ra, 0, cg, cursor);
   }


int32_t TR::RVSystemLinkage::buildArgs(TR::Node *callNode,
                                       TR::RegisterDependencyConditions *dependencies)

   {
   const TR::RVLinkageProperties &properties = getProperties();
   TR::RVMemoryArgument *pushToMemory = NULL;
   TR::Register *argMemReg;
   TR::Register *tempReg;
   int32_t argIndex = 0;
   int32_t numMemArgs = 0;
   int32_t argSize = 0;
   int32_t numIntegerArgs = 0;
   int32_t numFloatArgs = 0;
   int32_t totalSize;
   int32_t i;

   TR::Node *child;
   TR::DataType childType;
   TR::DataType resType = callNode->getType();

   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();

   /* Step 1 - figure out how many arguments are going to be spilled to memory i.e. not in registers */
   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (numIntegerArgs >= properties.getNumIntArgRegs())
               numMemArgs++;
            numIntegerArgs++;
            break;

         case TR::Float:
         case TR::Double:
            if (numFloatArgs >= properties.getNumFloatArgRegs())
                  numMemArgs++;
            numFloatArgs++;
            break;

         default:
            TR_ASSERT(false, "Argument type %s is not supported\n", childType.toString());
         }
      }

   // From here, down, any new stack allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   /* End result of Step 1 - determined number of memory arguments! */
   if (numMemArgs > 0)
      {
      pushToMemory = new (trStackMemory()) TR::RVMemoryArgument[numMemArgs];

      argMemReg = cg()->allocateRegister();
      }

   totalSize = numMemArgs * 8;
   // align to 16-byte boundary
   totalSize = (totalSize + 15) & (~15);

   numIntegerArgs = 0;
   numFloatArgs = 0;

   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      TR::MemoryReference *mref = NULL;
      TR::Register *argRegister;
      TR::InstOpCode::Mnemonic op;

      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (childType == TR::Address)
               argRegister = pushAddressArg(child);
            else if (childType == TR::Int64)
               argRegister = pushLongArg(child);
            else
               argRegister = pushIntegerWordArg(child);

            if (numIntegerArgs < properties.getNumIntArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempReg = cg()->allocateCollectedReferenceRegister();
                  else
                     tempReg = cg()->allocateRegister();
                  generateITYPE(TR::InstOpCode::_addi, callNode, tempReg, argRegister, 0, cg());
                  argRegister = tempReg;
                  }
               if (numIntegerArgs == 0 &&
                  (resType.isAddress() || resType.isInt32() || resType.isInt64()))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();

                  dependencies->addPreCondition(argRegister, TR::RealRegister::a0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::a0);
                  }
               else
                  {
                  addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               }
            else
               {
               // numIntegerArgs >= properties.getNumIntArgRegs()
               if (childType == TR::Address || childType == TR::Int64)
                  {
                  op = TR::InstOpCode::_sd;
                  }
               else
                  {
                  op = TR::InstOpCode::_sw;
                  }
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, op, pushToMemory[argIndex++]);
               argSize += 8; // always 8-byte aligned
               }
            numIntegerArgs++;
            break;

         case TR::Float:
         case TR::Double:
            if (childType == TR::Float)
               argRegister = pushFloatArg(child);
            else
               argRegister = pushDoubleArg(child);

            if (numFloatArgs < properties.getNumFloatArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  // TODO: check if this is the best way to move floating point values
                  // between regs
                  op = (childType == TR::Float) ? TR::InstOpCode::_fsgnj_d : TR::InstOpCode::_fsgnj_s;
                  generateRTYPE(op, callNode, tempReg, argRegister, argRegister, cg());
                  argRegister = tempReg;
                  }
               if ((numFloatArgs == 0 && resType.isFloatingPoint()))
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);

                  dependencies->addPreCondition(argRegister, TR::RealRegister::fa0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::fa0);
                  }
               else
                  {
                  addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
                  }
               }
            else
               {
               // numFloatArgs >= properties.getNumFloatArgRegs()
               if (childType == TR::Double)
                  {
                  op = TR::InstOpCode::_fsd;
                  }
               else
                  {
                  op = TR::InstOpCode::_fsw;
                  }
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, op, pushToMemory[argIndex++]);
               argSize += 8; // always 8-byte aligned
               }
            numFloatArgs++;
            break;
         } // end of switch
      } // end of for

   // NULL deps for non-preserved and non-system regs
   while (numIntegerArgs < properties.getNumIntArgRegs())
      {
      if (numIntegerArgs == 0 && resType.isAddress())
         {
         dependencies->addPreCondition(cg()->allocateRegister(), properties.getIntegerArgumentRegister(0));
         dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), properties.getIntegerArgumentRegister(0));
         }
      else
         {
         addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
         }
      numIntegerArgs++;
      }

   int32_t floatRegsUsed = (numFloatArgs > properties.getNumFloatArgRegs()) ? properties.getNumFloatArgRegs() : numFloatArgs;
   for (i = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::f0 + floatRegsUsed); i <= TR::RealRegister::LastFPR; i++)
      {
      if (!properties.getPreserved((TR::RealRegister::RegNum)i))
         {
         // NULL dependency for non-preserved regs
         addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_FPR, cg());
         }
      }

   if (numMemArgs > 0)
      {
      TR::RealRegister *sp = cg()->machine()->getRealRegister(properties.getStackPointerRegister());
      generateITYPE(TR::InstOpCode::_addi, callNode, argMemReg, sp, - totalSize, cg());

      for (argIndex = 0; argIndex < numMemArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateSTORE(pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg, cg());
         cg()->stopUsingRegister(aReg);
         }

      cg()->stopUsingRegister(argMemReg);
      }

   return totalSize;
   }


TR::Register *TR::RVSystemLinkage::buildDispatch(TR::Node *callNode)
   {
   const TR::RVLinkageProperties &pp = getProperties();
   TR::RealRegister *sp = cg()->machine()->getRealRegister(pp.getStackPointerRegister());
   TR::RealRegister *ra = cg()->machine()->getRealRegister(TR::RealRegister::ra);
   TR::Register *target = nullptr;

   if (callNode->getOpCode().isCallIndirect())
      {
      target = cg()->evaluate(callNode->getFirstChild());
      }

   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   int32_t totalSize = buildArgs(callNode, dependencies);
   if (totalSize > 0)
      {
      if (VALID_ITYPE_IMM(-totalSize))
         {
         generateITYPE(TR::InstOpCode::_addi, callNode, sp, sp, -totalSize, cg());
         }
      else
         {
         TR_ASSERT_FATAL(false, "Too many arguments.");
         }
      }

   if (callNode->getOpCode().isCallIndirect())
      {
      generateITYPE(TR::InstOpCode::_jalr, callNode, ra, target, 0, dependencies, cg());
      }
   else
      {
      TR::SymbolReference *callSymRef = callNode->getSymbolReference();
      generateJTYPE(TR::InstOpCode::_jal, callNode, ra,
            (uintptr_t)callSymRef->getMethodAddress(), dependencies, callSymRef, NULL, cg());
      }

   cg()->machine()->setLinkRegisterKilled(true);

   if (totalSize > 0)
      {
      if (VALID_ITYPE_IMM(totalSize))
         {
         generateITYPE(TR::InstOpCode::_addi, callNode, sp, sp, totalSize, cg());
         }
      else
         {
         TR_ASSERT_FATAL(false, "Too many arguments.");
         }
      }

   TR::Register *retReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::icalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getIntegerReturnRegister());
         break;
      case TR::lcall:
      case TR::lcalli:
      case TR::acall:
      case TR::acalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getLongReturnRegister());
         break;
      case TR::fcall:
      case TR::fcalli:
      case TR::dcall:
      case TR::dcalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getFloatReturnRegister());
         break;
      case TR::call:
      case TR::calli:
         retReg = NULL;
         break;
      default:
         retReg = NULL;
         TR_ASSERT(false, "Unsupported direct call Opcode.");
      }

   callNode->setRegister(retReg);
   if (callNode->getOpCode().isCallIndirect())
      {
      callNode->getFirstChild()->decReferenceCount();
      }
   return retReg;
   }

