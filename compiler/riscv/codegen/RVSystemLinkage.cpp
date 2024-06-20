/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#include <riscv.h>
#include "codegen/RVSystemLinkage.hpp"
#include "codegen/RVInstruction.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/Node_inlines.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/ParameterSymbol.hpp"


/**
 * @brief Adds dependency
 */
inline void
addDependency(
      TR::RegisterDependencyConditions *dep,
      TR::Register *vreg,
      TR::RealRegister::RegNum rnum,
      TR_RegisterKinds rk,
      TR::CodeGenerator *cg)
   {
   if (vreg == NULL)
      {
      vreg = cg->allocateRegister(rk);
      }

   dep->addPreCondition(vreg, rnum);
   dep->addPostCondition(vreg, rnum);
   }

TR::RVSystemLinkageProperties::RVSystemLinkageProperties()
   {
   _properties = IntegersInRegisters|FloatsInRegisters|RightToLeft;

   /*
    * _registerFlags for each register are defined in architectural order,
    * that is, from x0 to x31, f0 to f31.
    *
    * See https://github.com/riscv/riscv-elf-psabi-doc/blob/master/riscv-elf.md#integer-register-convention
    */

   _registerFlags[TR::RealRegister::zero] = Preserved|RV_Reserved; // zero
   _registerFlags[TR::RealRegister::ra]   = Preserved|RV_Reserved; // return address
   _registerFlags[TR::RealRegister::sp]   = Preserved|RV_Reserved; // sp
   _registerFlags[TR::RealRegister::gp]   = Preserved|RV_Reserved; // gp
   _registerFlags[TR::RealRegister::tp]   = Preserved|RV_Reserved; // tp

   _registerFlags[TR::RealRegister::t0]   = Preserved|RV_Reserved; // fp
   _registerFlags[TR::RealRegister::t1]   = 0;
   _registerFlags[TR::RealRegister::t2]   = 0;

   _registerFlags[TR::RealRegister::s0]   = Preserved;
   _registerFlags[TR::RealRegister::s1]   = Preserved;

   _registerFlags[TR::RealRegister::a0]   = IntegerArgument | IntegerReturn;
   _registerFlags[TR::RealRegister::a1]   = IntegerArgument | IntegerReturn;
   _registerFlags[TR::RealRegister::a2]   = IntegerArgument;
   _registerFlags[TR::RealRegister::a3]   = IntegerArgument;
   _registerFlags[TR::RealRegister::a4]   = IntegerArgument;
   _registerFlags[TR::RealRegister::a5]   = IntegerArgument;
   _registerFlags[TR::RealRegister::a6]   = IntegerArgument;
   _registerFlags[TR::RealRegister::a7]   = IntegerArgument;

   _registerFlags[TR::RealRegister::s2]   = Preserved;
   _registerFlags[TR::RealRegister::s3]   = Preserved;
   _registerFlags[TR::RealRegister::s4]   = Preserved;
   _registerFlags[TR::RealRegister::s5]   = Preserved;
   _registerFlags[TR::RealRegister::s6]   = Preserved;
   _registerFlags[TR::RealRegister::s7]   = Preserved;
   _registerFlags[TR::RealRegister::s8]   = Preserved;
   _registerFlags[TR::RealRegister::s9]   = Preserved;
   _registerFlags[TR::RealRegister::s10]  = Preserved;
   _registerFlags[TR::RealRegister::s11]  = Preserved;

   _registerFlags[TR::RealRegister::t3]   = 0;
   _registerFlags[TR::RealRegister::t4]   = 0;
   _registerFlags[TR::RealRegister::t5]   = 0;
   _registerFlags[TR::RealRegister::t6]   = 0;

   _registerFlags[TR::RealRegister::ft0]  = 0;
   _registerFlags[TR::RealRegister::ft1]  = 0;
   _registerFlags[TR::RealRegister::ft2]  = 0;
   _registerFlags[TR::RealRegister::ft3]  = 0;
   _registerFlags[TR::RealRegister::ft4]  = 0;
   _registerFlags[TR::RealRegister::ft5]  = 0;
   _registerFlags[TR::RealRegister::ft6]  = 0;
   _registerFlags[TR::RealRegister::ft7]  = 0;

   _registerFlags[TR::RealRegister::fs0]  = Preserved;
   _registerFlags[TR::RealRegister::fs1]  = Preserved;

   _registerFlags[TR::RealRegister::fa0]  = FloatArgument | FloatReturn;
   _registerFlags[TR::RealRegister::fa1]  = FloatArgument | FloatReturn;
   _registerFlags[TR::RealRegister::fa2]  = FloatArgument;
   _registerFlags[TR::RealRegister::fa3]  = FloatArgument;
   _registerFlags[TR::RealRegister::fa4]  = FloatArgument;
   _registerFlags[TR::RealRegister::fa5]  = FloatArgument;
   _registerFlags[TR::RealRegister::fa6]  = FloatArgument;
   _registerFlags[TR::RealRegister::fa7]  = FloatArgument;

   _registerFlags[TR::RealRegister::fs2]  = Preserved;
   _registerFlags[TR::RealRegister::fs3]  = Preserved;
   _registerFlags[TR::RealRegister::fs4]  = Preserved;
   _registerFlags[TR::RealRegister::fs5]  = Preserved;
   _registerFlags[TR::RealRegister::fs6]  = Preserved;
   _registerFlags[TR::RealRegister::fs7]  = Preserved;
   _registerFlags[TR::RealRegister::fs8]  = Preserved;
   _registerFlags[TR::RealRegister::fs9]  = Preserved;
   _registerFlags[TR::RealRegister::fs10] = Preserved;
   _registerFlags[TR::RealRegister::fs11] = Preserved;
   _registerFlags[TR::RealRegister::ft8]  = 0;
   _registerFlags[TR::RealRegister::ft9]  = 0;
   _registerFlags[TR::RealRegister::ft10] = 0;
   _registerFlags[TR::RealRegister::ft11] = 0;

   _methodMetaDataRegister      = TR::RealRegister::NoReg;
   _stackPointerRegister        = TR::RealRegister::sp;
   _framePointerRegister        = TR::RealRegister::s0;

   _computedCallTargetRegister  = TR::RealRegister::NoReg;
   _vtableIndexArgumentRegister = TR::RealRegister::NoReg;
   _j9methodArgumentRegister    = TR::RealRegister::NoReg;

   _offsetToFirstLocal            = 0; // To be determined

   initialize();
   }

const TR::RVSystemLinkageProperties
TR::RVSystemLinkage::_properties;

TR::RVSystemLinkage::RVSystemLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   setOffsetToFirstParm(0); // To be determined
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

   FOR_EACH_RESERVED_REGISTER(machine, _properties,
         reg->setState(TR::RealRegister::Locked);
         reg->setAssignedRegister(reg);
   );

   FOR_EACH_REGISTER(machine, reg->setWeight(0xf000));

   // prefer preserved registers over the rest since they're saved / restored
   // in prologue/epilogue.
   FOR_EACH_CALLEE_SAVED_REGISTER(machine, _properties, reg->setWeight(0x0001));
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
      TR_ASSERT_FATAL((stackIndex & 7) == 0, "Unaligned stack index.");
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

   FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine, _properties, stackIndex += 8);

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
            else if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               nextIntArgReg++;
               mapSingleParameter(parameter, stackIndex, true);
               }
            else
               {
               nextFltArgReg = getProperties().getNumFloatArgRegs() + 1;
               }
            break;
         case TR::Aggregate:
            TR_ASSERT_FATAL(false, "Function parameters of aggregate types are not currently supported on RISC-V.");
            break;
         default:
            TR_ASSERT_FATAL(false, "Unknown parameter type.");
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
            else if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               nextIntArgReg++;
               }
            else
               {
               mapSingleParameter(parameter, stackIndex, false);
               }
            break;
         case TR::Aggregate:
            TR_ASSERT_FATAL(false, "Function parameters of aggregate types are not currently supported on RISC-V.");
            break;
         default:
            TR_ASSERT_FATAL(false, "Unknown parameter type.");
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
   TR::CodeGenerator *cg = this->cg();
   TR::Machine *machine = cg->machine();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   const TR::RVLinkageProperties& properties = getProperties();
   TR::RealRegister *sp = machine->getRealRegister(properties.getStackPointerRegister());
   TR::RealRegister *ra = machine->getRealRegister(TR::RealRegister::ra);
   TR::Node *firstNode = comp()->getStartTree()->getNode();

   // allocate stack space
   uint32_t frameSize = cg->getFrameSizeInBytes();
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
      TR::MemoryReference *stackSlot = TR::MemoryReference::createWithDisplacement(cg, sp, 0);
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
      TR::MemoryReference *stackSlot = TR::MemoryReference::createWithDisplacement(cg, sp, parameter->getParameterOffset());
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
               cursor = generateSTORE(op, firstNode, stackSlot, machine->getRealRegister(getProperties().getIntegerArgumentRegister(nextIntArgReg)), cg, cursor);
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
               cursor = generateSTORE(op, firstNode, stackSlot, machine->getRealRegister(getProperties().getFloatArgumentRegister(nextFltArgReg)), cg, cursor);
               nextFltArgReg++;
               }
            else if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               op = (parameter->getSize() == 8) ? TR::InstOpCode::_sd : TR::InstOpCode::_sw;
               cursor = generateSTORE(op, firstNode, stackSlot, machine->getRealRegister(getProperties().getIntegerArgumentRegister(nextIntArgReg)), cg, cursor);
               nextIntArgReg++;
               }
            else
               {
               nextFltArgReg = getProperties().getNumFloatArgRegs() + 1;
               }
            break;
         case TR::Aggregate:
            TR_ASSERT_FATAL(false, "Function parameters of aggregate types are not currently supported on RISC-V.");
            break;
         default:
            TR_ASSERT_FATAL(false, "Unknown parameter type.");
         }
      }

   // save callee-saved registers
   uint32_t offset = bodySymbol->getLocalMappingCursor();
   FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine, _properties,
      TR::MemoryReference *stackSlot = TR::MemoryReference::createWithDisplacement(cg, sp, offset);
      cursor = generateSTORE(TR::InstOpCode::_sd, firstNode, stackSlot, reg, this->cg(), cursor);
      offset += 8;)
   }


void
TR::RVSystemLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR::CodeGenerator *cg = this->cg();
   const TR::RVLinkageProperties& properties = getProperties();
   TR::Machine *machine = cg->machine();
   TR::Node *lastNode = cursor->getNode();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *sp = machine->getRealRegister(properties.getStackPointerRegister());
   TR::RealRegister *zero = machine->getRealRegister(TR::RealRegister::zero);

   // restore callee-saved registers
   uint32_t offset = bodySymbol->getLocalMappingCursor();
   FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine, _properties,
      TR::MemoryReference *stackSlot = TR::MemoryReference::createWithDisplacement(cg, sp, offset);
      cursor = generateLOAD(TR::InstOpCode::_ld, lastNode, reg, stackSlot, this->cg(), cursor);
      offset += 8;)

   // restore link register (x30)
   TR::RealRegister *ra = machine->getRealRegister(TR::RealRegister::ra);
   if (machine->getLinkRegisterKilled())
      {
      TR::MemoryReference *stackSlot = TR::MemoryReference::createWithDisplacement(cg, sp, 0);
      cursor = generateLOAD(TR::InstOpCode::_ld, lastNode, ra, stackSlot, this->cg(), cursor);
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
   int32_t numFloatArgsPassedInGPRs = 0;
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
            if ((numIntegerArgs + numFloatArgsPassedInGPRs) >= properties.getNumIntArgRegs())
               numMemArgs++;
            numIntegerArgs++;
            break;

         case TR::Float:
         case TR::Double:
            if (numFloatArgs >= properties.getNumFloatArgRegs())
               {
               if ((numIntegerArgs + numFloatArgsPassedInGPRs) < properties.getNumIntArgRegs())
                  numFloatArgsPassedInGPRs++;
               else
                  numMemArgs++;
               }
            numFloatArgs++;
            break;

         default:
            TR_ASSERT_FATAL(false, "Argument type %s is not supported\n", childType.toString());
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
   numFloatArgsPassedInGPRs = 0;

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

            if ((numIntegerArgs + numFloatArgsPassedInGPRs) < properties.getNumIntArgRegs())
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
                  TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs + numFloatArgsPassedInGPRs), TR_GPR, cg());
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
               mref = getOutgoingArgumentMemRef(argMemReg, argSize, argRegister, op, pushToMemory[argIndex++]);
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
                  TR::addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
                  }
               }
            else if ((numIntegerArgs + numFloatArgsPassedInGPRs) < properties.getNumIntArgRegs())
               {
               TR::Register *gprRegister = cg()->allocateRegister(TR_GPR);
               TR::Register *zero = cg()->machine()->getRealRegister(TR::RealRegister::zero);
               op = (childType == TR::Float) ? TR::InstOpCode::_fmv_x_s : TR::InstOpCode::_fmv_x_d;
               generateRTYPE(op, callNode, gprRegister, argRegister, zero, cg());

               TR::addDependency(dependencies, gprRegister, properties.getIntegerArgumentRegister(numIntegerArgs + numFloatArgsPassedInGPRs), TR_GPR, cg());
               numFloatArgsPassedInGPRs++;
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
               mref = getOutgoingArgumentMemRef(argMemReg, argSize, argRegister, op, pushToMemory[argIndex++]);
               argSize += 8; // always 8-byte aligned
               }
            numFloatArgs++;
            break;
         } // end of switch
      } // end of for

   // NULL deps for unused integer argument registers
   for (auto i = numIntegerArgs + numFloatArgsPassedInGPRs; i < properties.getNumIntArgRegs(); i++)
      {
      if (i == 0 && resType.isAddress())
         {
         dependencies->addPreCondition(cg()->allocateRegister(), properties.getIntegerArgumentRegister(0));
         dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), properties.getIntegerArgumentRegister(0));
         }
      else
         {
         TR::addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(i), TR_GPR, cg());
         }
      }

   // NULL deps for non-preserved non-argument integer registers
   for (auto rn = TR::RealRegister::FirstGPR; rn <= TR::RealRegister::LastGPR; rn++)
      {
      if (!properties.getPreserved(rn) && !properties.getIntegerArgument(rn))
         {
         TR::addDependency(dependencies, NULL, rn, TR_FPR, cg());
         }
      }

   // NULL deps for unused FP argument registers
   for (auto i = numFloatArgs; i < properties.getNumFloatArgRegs(); i++)
      {
      TR::addDependency(dependencies, NULL, properties.getFloatArgumentRegister(i), TR_FPR, cg());
      }

   // NULL deps for non-preserved non-argument FP registers
   for (auto rn = TR::RealRegister::FirstFPR; rn <= TR::RealRegister::LastFPR; rn++)
      {
      if (!properties.getPreserved(rn) && !properties.getFloatArgument(rn))
         {
         TR::addDependency(dependencies, NULL, rn, TR_FPR, cg());
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
         pp.getNumberOfDependencyRegisters(),
         pp.getNumberOfDependencyRegisters(), trMemory());

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
      auto targetAddr = callNode->getSymbolReference()->getMethodAddress();

      if (targetAddr == NULL)
         {
         /*
          * If the target address is not known yet, we generate `jal` and hope that the target address
          * offset would fit into 20bit immediate.
          *
          * This is the case of recursive calls.
          */
         generateJTYPE(TR::InstOpCode::_jal, callNode, ra, 0, dependencies, callNode->getSymbolReference(), NULL, cg());
         }
      else
         {
         /*
          * If the target address is known, we load it into a register and generate `jalr`. This may be
          * wasteful in cases the target address offset would fit into 20bit immediate of `jal`. However,
          * more often than not, non-jitted functions (library / runtime functions) are too far to fit in
          * the immediate value.
          *
          * So, until a trampolines are implemented, we pay the price and load the target address into
          * register.
          *
          * Note, that here we load the target address into link register (`ra`). It's going to be clobbered
          * anyways and this way we do not need to allocate another one.
          */
         loadConstant64(cg(), callNode, reinterpret_cast<int64_t>(targetAddr), ra);
         generateITYPE(TR::InstOpCode::_jalr, callNode, ra, ra, 0, dependencies, cg());
         }
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
         TR_ASSERT_FATAL(false, "Unsupported direct call Opcode.");
      }

   callNode->setRegister(retReg);
   if (callNode->getOpCode().isCallIndirect())
      {
      callNode->getFirstChild()->decReferenceCount();
      }

   dependencies->stopUsingDepRegs(cg(), retReg);

   return retReg;
   }

intptr_t TR::RVSystemLinkage::entryPointFromCompiledMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

intptr_t TR::RVSystemLinkage::entryPointFromInterpretedMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

