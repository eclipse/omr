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

#include "env/CompilerEnv.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "codegen/ARMInstruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/ARMSystemLinkage.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GenerateInstructions.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/StackCheckFailureSnippet.hpp"

struct UnsupportedParameterType : public virtual TR::CompilationException
   {
   virtual const char* what() const throw() { return "Unsupported Parameter Type"; }
   };

TR::ARMLinkageProperties TR::ARMSystemLinkage::properties =
    {                           // TR_System
    IntegersInRegisters|        // linkage properties
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
    FloatsInRegisters  |
#endif
    RightToLeft,
       {                        // register flags
       0,                       // NoReg
       IntegerReturn|           // gr0
       IntegerArgument,
       IntegerReturn|           // gr1
       IntegerArgument,
       IntegerArgument,         // gr2
       IntegerArgument,         // gr3
       Preserved,               // gr4
       Preserved,               // gr5
       Preserved,               // gr6
       Preserved,               // gr7
       Preserved,               // gr8
       Preserved,               // gr9
       Preserved,               // gr10
       Preserved,               // gr11
       Preserved,               // gr12
       Preserved,               // gr13
       Preserved,               // gr14
       Preserved,               // gr15
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
       FloatArgument|           // fp0
       FloatReturn,
       FloatArgument,           // fp1
       FloatArgument,           // fp2
       FloatArgument,           // fp3
       FloatArgument,           // fp4
       FloatArgument,           // fp5
       FloatArgument,           // fp6
       FloatArgument,           // fp7
       Preserved,               // fp8
       Preserved,               // fp9
       Preserved,               // fp10
       Preserved,               // fp11
       Preserved,               // fp12
       Preserved,               // fp13
       Preserved,               // fp14
       Preserved,               // fp15
#else
       0,                        // fp0
       0,                        // fp1
       0,                        // fp2
       0,                        // fp3
       0,                        // fp4
       0,                        // fp5
       0,                        // fp6
       0,                        // fp7
#endif
       },
       {                        // preserved registers
       TR::RealRegister::gr4,
       TR::RealRegister::gr5,
       TR::RealRegister::gr6,
       TR::RealRegister::gr7,
       TR::RealRegister::gr8,
       TR::RealRegister::gr9,
       TR::RealRegister::gr10,
       TR::RealRegister::gr11,
       TR::RealRegister::gr12,
       TR::RealRegister::gr13,
       TR::RealRegister::gr14,
       TR::RealRegister::gr15,
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
       TR::RealRegister::fp8,
       TR::RealRegister::fp9,
       TR::RealRegister::fp10,
       TR::RealRegister::fp11,
       TR::RealRegister::fp12,
       TR::RealRegister::fp13,
       TR::RealRegister::fp14,
       TR::RealRegister::fp15,
#endif
       },
       {                        // argument registers
       TR::RealRegister::gr0,
       TR::RealRegister::gr1,
       TR::RealRegister::gr2,
       TR::RealRegister::gr3,
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
       TR::RealRegister::fp0,
       TR::RealRegister::fp1,
       TR::RealRegister::fp2,
       TR::RealRegister::fp3,
       TR::RealRegister::fp4,
       TR::RealRegister::fp5,
       TR::RealRegister::fp6,
       TR::RealRegister::fp7,
#endif
       },
       {                        // return registers
       TR::RealRegister::gr0,
       TR::RealRegister::gr1,
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
       TR::RealRegister::fp0,
#endif
       },
       MAX_ARM_GLOBAL_GPRS,     // numAllocatableIntegerRegisters
       MAX_ARM_GLOBAL_FPRS,     // numAllocatableFloatRegisters
       0x0000fff0,              // preserved register map
       TR::RealRegister::gr11, // frame register
       TR::RealRegister::gr8, // method meta data register
       TR::RealRegister::gr13, // stack pointer register
       TR::RealRegister::NoReg, // vtable index register
       TR::RealRegister::NoReg,  // j9method argument register
       15,                      // numberOfDependencyGPRegisters
       0,                       // offsetToFirstStackParm (relative to old sp)
       -4,                      // offsetToFirstLocal (not counting out-args)
       4,                       // numIntegerArgumentRegisters
       0,                       // firstIntegerArgumentRegister (index into ArgumentRegisters)
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
       8,                       // numFloatArgumentRegisters
       4,                       // firstFloatArgumentRegister (index into ArgumentRegisters)
       0,                       // firstIntegerReturnRegister
       2                        // firstFloatReturnRegister
#else
       0,                       // numFloatArgumentRegisters
       0,                       // firstFloatArgumentRegister (index into ArgumentRegisters)
       0,                       // firstIntegerReturnRegister
       0                        // firstFloatReturnRegister
#endif
    };

void TR::ARMSystemLinkage::initARMRealRegisterLinkage()
   {
   TR::Machine *machine = cg()->machine();

   // make r15 (PC) unavailable for RA
   TR::RealRegister *reg = machine->getRealRegister(TR::RealRegister::gr15);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   // make r14 (LR) unavailable for RA
   reg = machine->getRealRegister(TR::RealRegister::gr14);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   // make r13 (SP) unavailable for RA
   reg = machine->getRealRegister(TR::RealRegister::gr13);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   // make r12 (IP) unavailable for RA
   reg = machine->getRealRegister(TR::RealRegister::gr12);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   // make r9 unavailable for RA (just in case, because it's meaning is platform defined)
   reg = machine->getRealRegister(TR::RealRegister::gr9);
   reg->setState(TR::RealRegister::Locked);
   reg->setAssignedRegister(reg);

   /*
    * Note: we can assign the same weight to all registers because loads/stores
    * can be done on multiple registers simultaneously.
    */

   // assign "maximum" weight to registers r0-r8
   for (int32_t r = TR::RealRegister::gr0; r <= TR::RealRegister::gr8; ++r)
      {
      machine->getRealRegister(static_cast<TR::RealRegister::RegNum>(r))->setWeight(0xf000);
      }

   // assign "maximum" weight to registers r10-r12
   for (int32_t r = TR::RealRegister::gr10; r <= TR::RealRegister::gr12; ++r)
      {
      machine->getRealRegister(static_cast<TR::RealRegister::RegNum>(r))->setWeight(0xf000);
      }
   }

uint32_t TR::ARMSystemLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }

static void mapSingleParameter(TR::ParameterSymbol *parameter, uint32_t &stackIndex)
   {
   auto size = parameter->getSize();
   auto alignment = size <= 4 ? 4 : 8;
   stackIndex = (stackIndex + alignment - 1)&(~(alignment - 1));
   parameter->setParameterOffset(stackIndex);
   stackIndex += size;
   }

/**
 * @brief Maps symbols in the IL to locations on the stack
 * @param method is the method for which symbols are being stack mapped
 *
 * In general, the shape of a stack frame is as follows:
 *
 * +-----------------------------+
 * | caller frame                |
 * +-----------------------------+
 * | stack arguments             |
 * +=============================+ <-+ (start of callee frame)
 * | saved registers             |   |
 * +-----------------------------+   | frame size
 * | locals                      |   |
 * +-----------------------------+ <-+- $sp
 *
 * A symbol is mapped onto the stack by assigning to it an offset from the stack
 * pointer. All symbols representing stack allocated values must be mapped,
 * including automatics (locals) on the callee frame and stack allocated
 * arguments on the caller frame
 *
 * The algorithm used to map symbols iterates over each symbol in ascending
 * address order. Using the frame shape depicted above as a general example:
 * locals are mapped first, registers second. The algorithm is:
 *
 * 1. Set stackIndex to 0
 * 2. For each symbol that must be mapped onto the **callee** stack frame,
 *    starting at the lowest address:
 *       a. set stackIndex as the symbol offset
 *       b. increment stackIndex by the size of the symbol's type
 *          plus alignment requirements
 * 3. Increment stackIndex by the necessary amount to account for the stack
 *    space required for saved registers
 * 4. Save stackIndex as the size of the callee stack frame
 * 5. For each symbol that must be mapped onto the **caller** stack frame,
 *    starting at the lowest address:
 *       a. set the symbol offset as the current stack index
 *       b. increment the stack index by the size of the symbol's type,
 *          plus alignment requirements
 */
void TR::ARMSystemLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   uint32_t stackIndex = 0;
   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol *localCursor = automaticIterator.getFirst();

   // map non-double automatics
   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->getDataType() != TR::Double)
         {
         localCursor->setOffset(stackIndex);
         stackIndex += (localCursor->getSize()+3)&(~3);
         }
      localCursor = automaticIterator.getNext();
      }

   stackIndex += (stackIndex & 0x4) ? 4 : 0; // align to 8 bytes
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   // map double automatics
   while (localCursor != NULL)
      {
      if (localCursor->getDataType() == TR::Double)
         {
         localCursor->setOffset(stackIndex);
         stackIndex += (localCursor->getSize()+7)&(~7);
         }
      localCursor = automaticIterator.getNext();
      }
   method->setLocalMappingCursor(stackIndex);

   // allocate space for preserved registers and link register (9 registers total)
   stackIndex += 9*4;

   /*
    * Because the rest of the code generator currently expects **all** arguments
    * to be passed on the stack, arguments passed in registers must be spilled
    * in the callee frame. To map the arguments correctly, we use two loops. The
    * first maps the arguments that will come in registers onto the callee stack.
    * At the end of this loop, the `stackIndex` is the the size of the frame.
    * The second loop then maps the remaining arguments onto the caller frame.
    */

   auto nextIntArgReg = 0;
   auto nextFltArgReg = 0;

   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   for (TR::ParameterSymbol *parameter = parameterIterator.getFirst();
        parameter!=NULL && (nextIntArgReg < getProperties().getNumIntArgRegs() || nextFltArgReg < getProperties().getNumFloatArgRegs());
        parameter=parameterIterator.getNext())
      {
      switch (parameter->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
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
         case TR::Int64:
            nextIntArgReg += nextIntArgReg & 0x1; // round to next even number
            if (nextIntArgReg + 1 < getProperties().getNumIntArgRegs())
               {
               nextIntArgReg += 2;
               mapSingleParameter(parameter, stackIndex);
               }
            else
               {
               nextIntArgReg = getProperties().getNumIntArgRegs() + 1;
               }
            break;
         case TR::Float:
            comp()->failCompilation<UnsupportedParameterType>("Compiling methods with a single precision floating point parameter is not supported");
            break;
         case TR::Double:
            if (nextFltArgReg < getProperties().getNumFloatArgRegs())
               {
               nextFltArgReg += 1;
               mapSingleParameter(parameter, stackIndex);
               }
            else
               {
               nextFltArgReg = getProperties().getNumFloatArgRegs() + 1;
               }
            break;
         case TR::Aggregate:
            TR_ASSERT(false, "Function parameters of aggregate types are not currently supported on ARM.");
         }
      }

   // save the stack frame size, aligned to 8 bytes
   stackIndex = (stackIndex + 7)&(~7);
   cg()->setFrameSizeInBytes(stackIndex);

   nextIntArgReg = 0;
   nextFltArgReg = 0;
   parameterIterator.reset();

   for (TR::ParameterSymbol *parameter = parameterIterator.getFirst();
        parameter!=NULL && (nextIntArgReg < getProperties().getNumIntArgRegs() || nextFltArgReg < getProperties().getNumFloatArgRegs());
        parameter=parameterIterator.getNext())
      {
      switch (parameter->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
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
         case TR::Int64:
            nextIntArgReg += stackIndex & 0x1; // round to next even number
            if (nextIntArgReg + 1 < getProperties().getNumIntArgRegs())
               {
               nextIntArgReg += 2;
               }
            else
               {
               mapSingleParameter(parameter, stackIndex);
               }
            break;
         case TR::Float:
            comp()->failCompilation<UnsupportedParameterType>("Compiling methods with a single precision floating point parameter is not supported");
            break;
         case TR::Double:
            if (nextFltArgReg < getProperties().getNumFloatArgRegs())
               {
               nextFltArgReg += 1;
               }
            else
               {
               mapSingleParameter(parameter, stackIndex);
               }
            break;
         case TR::Aggregate:
            TR_ASSERT(false, "Function parameters of aggregate types are not currently supported on ARM.");
         }
      }
   }

void TR::ARMSystemLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   int32_t roundedSize = (p->getSize()+3)&(~3);
   if (roundedSize == 0)
      roundedSize = 4;

   p->setOffset(stackIndex -= roundedSize);
   }

TR::ARMLinkageProperties& TR::ARMSystemLinkage::getProperties()
   {
   return properties;
   }

void TR::ARMSystemLinkage::createPrologue(TR::Instruction *cursor)
   {
   TR::CodeGenerator *codeGen = cg();
   const TR::ARMLinkageProperties& properties = getProperties();
   TR::Machine *machine = codeGen->machine();
   TR::ResolvedMethodSymbol* bodySymbol = comp()->getJittedMethodSymbol();
   TR::Node *firstNode = comp()->getStartTree()->getNode();
   TR::RealRegister *stackPtr = machine->getRealRegister(properties.getStackPointerRegister());

   // Entry breakpoint
   //
   if (comp()->getOption(TR_EntryBreakPoints))
      {
      cursor = new (trHeapMemory()) TR::Instruction(cursor, ARMOp_bad, firstNode, cg());
      }

   // allocate stack space
   auto frameSize = codeGen->getFrameSizeInBytes();
   cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_sub, firstNode, stackPtr, stackPtr, frameSize, 0, cursor);

   // spill argument registers
   auto nextIntArgReg = 0;
   auto nextFltArgReg = 0;
   ListIterator<TR::ParameterSymbol> parameterIterator(&bodySymbol->getParameterList());
   for (TR::ParameterSymbol *parameter = parameterIterator.getFirst();
        parameter!=NULL && (nextIntArgReg < getProperties().getNumIntArgRegs() || nextFltArgReg < getProperties().getNumFloatArgRegs());
        parameter=parameterIterator.getNext())
      {
      auto *stackSlot = new (trHeapMemory()) TR::MemoryReference(stackPtr, parameter->getParameterOffset(), codeGen);
      switch (parameter->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (nextIntArgReg < getProperties().getNumIntArgRegs())
               {
               cursor = generateMemSrc1Instruction(cg(), ARMOp_str, firstNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)(TR::RealRegister::gr0 + nextIntArgReg)), cursor);
               nextIntArgReg++;
               }
            else
               {
               nextIntArgReg = getProperties().getNumIntArgRegs() + 1;
               }
            break;
         case TR::Int64:
            nextIntArgReg += nextIntArgReg & 0x1; // round to next even number
            if (nextIntArgReg + 1 < getProperties().getNumIntArgRegs())
               {
               cursor = generateMemSrc1Instruction(cg(), ARMOp_str, firstNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)(TR::RealRegister::gr0 + nextIntArgReg)), cursor);
               stackSlot = new (trHeapMemory()) TR::MemoryReference(stackPtr, parameter->getParameterOffset() + 4, codeGen);
               cursor = generateMemSrc1Instruction(cg(), ARMOp_str, firstNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)(TR::RealRegister::gr0 + nextIntArgReg + 1)), cursor);
               nextIntArgReg += 2;
               }
            else
               {
               nextIntArgReg = getProperties().getNumIntArgRegs() + 1;
               }
            break;
         case TR::Float:
            comp()->failCompilation<UnsupportedParameterType>("Compiling methods with a single precision floating point parameter is not supported");
            break;
         case TR::Double:
            if (nextFltArgReg < getProperties().getNumFloatArgRegs())
               {
               cursor = generateMemSrc1Instruction(cg(), ARMOp_fstd, firstNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)(TR::RealRegister::fp0 + nextFltArgReg)), cursor);
               nextFltArgReg += 1;
               }
            else
               {
               nextFltArgReg = getProperties().getNumFloatArgRegs() + 1;
               }
            break;
         case TR::Aggregate:
            TR_ASSERT(false, "Function parameters of aggregate types are not currently supported on ARM.");
         }
      }

   // save all preserved registers
   for (int r = TR::RealRegister::gr4; r <= TR::RealRegister::gr11; ++r)
      {
      auto *stackSlot = new (trHeapMemory()) TR::MemoryReference(stackPtr, (TR::RealRegister::gr11 - r + 1)*4 + bodySymbol->getLocalMappingCursor(), codeGen);
      cursor = generateMemSrc1Instruction(cg(), ARMOp_str, firstNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)r), cursor);
      }

   // save link register (r14)
   auto *stackSlot = new (trHeapMemory()) TR::MemoryReference(stackPtr, bodySymbol->getLocalMappingCursor(), codeGen);
   cursor = generateMemSrc1Instruction(cg(), ARMOp_str, firstNode, stackSlot, machine->getRealRegister(TR::RealRegister::gr14), cursor);
   }

void TR::ARMSystemLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR::CodeGenerator *codeGen = cg();
   const TR::ARMLinkageProperties& properties = getProperties();
   TR::Machine *machine = codeGen->machine();
   TR::Node *lastNode = cursor->getNode();
   TR::ResolvedMethodSymbol* bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *stackPtr = machine->getRealRegister(properties.getStackPointerRegister());

   // restore link register (r14)
   auto *stackSlot = new (trHeapMemory()) TR::MemoryReference(stackPtr, bodySymbol->getLocalMappingCursor(), codeGen);
   cursor = generateMemSrc1Instruction(cg(), ARMOp_ldr, lastNode, stackSlot, machine->getRealRegister(TR::RealRegister::gr14), cursor);

   // restore all preserved registers
   for (int r = TR::RealRegister::gr4; r <= TR::RealRegister::gr11; ++r)
      {
      auto *stackSlot = new (trHeapMemory()) TR::MemoryReference(stackPtr, (TR::RealRegister::gr11 - r + 1)*4 + bodySymbol->getLocalMappingCursor(), codeGen);
      cursor = generateMemSrc1Instruction(cg(), ARMOp_ldr, lastNode, stackSlot, machine->getRealRegister((TR::RealRegister::RegNum)r), cursor);
      }

   // remove space for preserved registers
   auto frameSize = codeGen->getFrameSizeInBytes();
   cursor = generateTrg1Src1ImmInstruction(codeGen, ARMOp_add, lastNode, stackPtr, stackPtr, frameSize, 0, cursor);

   // return using `mov r15, r14`
   TR::RealRegister *gr14 = machine->getRealRegister(TR::RealRegister::gr14);
   TR::RealRegister *gr15 = machine->getRealRegister(TR::RealRegister::gr15);
   cursor = generateTrg1Src1Instruction(codeGen, ARMOp_mov, lastNode, gr15, gr14, cursor);
   }

TR::MemoryReference *TR::ARMSystemLinkage::getOutgoingArgumentMemRef(int32_t               totalSize,
                                                                      int32_t               offset,
                                                                      TR::Register          *argReg,
                                                                      TR_ARMOpCodes         opCode,
                                                                      TR::ARMMemoryArgument &memArg)
   {
   int32_t                spOffset = offset - (getProperties().getNumIntArgRegs() * TR::Compiler->om.sizeofReferenceAddress());
   TR::RealRegister    *sp       = cg()->machine()->getRealRegister(properties.getStackPointerRegister());
   TR::MemoryReference *result   = new (trHeapMemory()) TR::MemoryReference(sp, spOffset, cg());

   memArg.argRegister = argReg;
   memArg.argMemory   = result;
   memArg.opCode      = opCode;

   return result;
   }

int32_t TR::ARMSystemLinkage::buildArgs(TR::Node                            *callNode,
                                       TR::RegisterDependencyConditions *dependencies,
                                       TR::Register*                       &vftReg,
                                       bool                                isJNI)
   {
   return buildARMLinkageArgs(callNode, dependencies, vftReg, TR_System, isJNI);
   }

TR::Register *TR::ARMSystemLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   return buildARMLinkageDirectDispatch(callNode, true);
   }

TR::Register *TR::ARMSystemLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

intptr_t TR::ARMSystemLinkage::entryPointFromCompiledMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

intptr_t TR::ARMSystemLinkage::entryPointFromInterpretedMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

