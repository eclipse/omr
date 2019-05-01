/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "codegen/IA32SystemLinkage.hpp"

#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/IA32LinkageUtils.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

namespace TR { class SymbolReference; }
namespace TR { class MemoryReference; }

////////////////////////////////////////////////
//
// Helpful definitions
//
// These are only here to make the rest of the code below somewhat
// self-documenting.
//
enum
   {
   RETURN_ADDRESS_SIZE=4,
   GPR_REG_WIDTH=4,
   IA32_STACK_SLOT_SIZE=4,
   IA32_DEFAULT_STACK_ALIGNMENT=4
   };

TR::IA32SystemLinkage::IA32SystemLinkage(
      TR::CodeGenerator *cg) :
   TR::X86SystemLinkage(cg)
   {
   _properties._properties = CallerCleanup;
   _properties._properties |= UsesPushesForPreservedRegs;

   // if OmitFramePointer specified, don't use one
   if (!cg->comp()->getOption(TR_OmitFramePointer))
      _properties._properties |= AlwaysDedicateFramePointerRegister;

   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::eax]   = IntegerReturn;
   _properties._registerFlags[TR::RealRegister::ebx]   = Preserved;
   _properties._registerFlags[TR::RealRegister::ecx]   = 0;
   _properties._registerFlags[TR::RealRegister::edx]   = IntegerReturn;
   _properties._registerFlags[TR::RealRegister::edi]   = Preserved;
   _properties._registerFlags[TR::RealRegister::esi]   = Preserved;
   _properties._registerFlags[TR::RealRegister::ebp]   = Preserved;
   _properties._registerFlags[TR::RealRegister::esp]   = Preserved;
   _properties._registerFlags[TR::RealRegister::st0]   = FloatReturn;

   _properties._preservedRegisters[0] = TR::RealRegister::ebx;
   _properties._preservedRegisters[1] = TR::RealRegister::edi;
   _properties._preservedRegisters[2] = TR::RealRegister::esi;
   _properties._maxRegistersPreservedInPrologue = 3;
   _properties._preservedRegisters[3] = TR::RealRegister::ebp;
   _properties._preservedRegisters[4] = TR::RealRegister::esp;

   _properties._argumentRegisters[0] = TR::RealRegister::NoReg;

   _properties._returnRegisters[0] = TR::RealRegister::eax; // IntegerReturnRegiste
   _properties._returnRegisters[1] = TR::RealRegister::st0; // Float/Double ReturnReg
   _properties._returnRegisters[2] = TR::RealRegister::edx; // LongHighReturnRegister

   _properties._numIntegerArgumentRegisters = 0;
   _properties._firstIntegerArgumentRegister = 0;
   _properties._numFloatArgumentRegisters    = 0;
   _properties._firstFloatArgumentRegister   = 0;

   _properties._framePointerRegister = TR::RealRegister::ebp;
   _properties._methodMetaDataRegister = TR::RealRegister::NoReg;

   _properties._preservedRegisterMapForGC =
      (  (1 << (TR::RealRegister::ebx-1)) |
         (1 << (TR::RealRegister::esi-1)) |
         (1 << (TR::RealRegister::edi-1)) |
         (1 << (TR::RealRegister::ebp-1)) |
         (1 << (TR::RealRegister::esp-1)));

   uint32_t p = 0;
   _properties._volatileRegisters[p++] = TR::RealRegister::eax;
   _properties._volatileRegisters[p++] = TR::RealRegister::ecx;
   _properties._volatileRegisters[p++] = TR::RealRegister::edx;
   _properties._numberOfVolatileGPRegisters = p;

   for (int32_t r=0; r<=7; r++)
      {
      _properties._volatileRegisters[p++] = TR::RealRegister::xmmIndex(r);
      }
   _properties._numberOfVolatileXMMRegisters = p - _properties._numberOfVolatileGPRegisters;
   _properties._numVolatileRegisters = p;

   _properties._offsetToFirstParm = RETURN_ADDRESS_SIZE;
   _properties._offsetToFirstLocal = _properties.getAlwaysDedicateFramePointerRegister() ? -GPR_REG_WIDTH : 0;
   _properties._OutgoingArgAlignment = IA32_DEFAULT_STACK_ALIGNMENT;

   p = 0;
   // Volatiles that aren't linkage regs
   _properties._allocationOrder[p++] = TR::RealRegister::ecx;

   // Linkage regis
   _properties._allocationOrder[p++] = TR::RealRegister::edx;
   _properties._allocationOrder[p++] = TR::RealRegister::eax;

   // preserved register
   _properties._allocationOrder[p++] = TR::RealRegister::esi;
   _properties._allocationOrder[p++] = TR::RealRegister::edi;
   _properties._allocationOrder[p++] = TR::RealRegister::ebx;
   _properties._allocationOrder[p++] = TR::RealRegister::ebp;

   // floating point
   _properties._allocationOrder[p++] = TR::RealRegister::st0;
   _properties._allocationOrder[p++] = TR::RealRegister::st1;
   _properties._allocationOrder[p++] = TR::RealRegister::st2;
   _properties._allocationOrder[p++] = TR::RealRegister::st3;
   _properties._allocationOrder[p++] = TR::RealRegister::st4;
   _properties._allocationOrder[p++] = TR::RealRegister::st5;
   _properties._allocationOrder[p++] = TR::RealRegister::st6;
   _properties._allocationOrder[p++] = TR::RealRegister::st7;

   }

uint32_t
TR::IA32SystemLinkage::getAlignment(TR::DataType type)
   {
   switch(type)
      {
      case TR::Float:
         return 4;
      case TR::Double:
         return 4;
      case TR::Int8:
         return 1;
      case TR::Int16:
         return 2;
      case TR::Int32:
         return 4;
      case TR::Address:
      case TR::Int64:
         return 4;
      // TODO: struct/union support should be added as a case branch.
      case TR::Aggregate:
      default:
         TR_ASSERT(false, "types still not support in layout\n");
         return 0;
      }
   }


int32_t
TR::IA32SystemLinkage::layoutParm(TR::Node *parmNode, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatReg, TR::parmLayoutResult &layoutResult)
   {
   layoutResult.abstract |= TR::parmLayoutResult::ON_STACK;
   int32_t align = layoutTypeOnStack(parmNode->getDataType(), dataCursor, layoutResult);
   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "layout param node %p on stack\n", parmNode);
   return align;
   }

int32_t
TR::IA32SystemLinkage::layoutParm(TR::ParameterSymbol *parmSymbol, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatReg, TR::parmLayoutResult &layoutResult)
   {
   layoutResult.abstract |= TR::parmLayoutResult::ON_STACK;
   int32_t align = layoutTypeOnStack(parmSymbol->getDataType(), dataCursor, layoutResult);
   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "layout param symbol %p on stack\n", parmSymbol);
   return align;
   }


void
TR::IA32SystemLinkage::setUpStackSizeForCallNode(TR::Node* node)
   {
   const TR::X86LinkageProperties     &properties = getProperties();
   int32_t  sizeOfOutGoingArgs= 0;
   uint16_t intReg = 0, floatReg = 0;
   for (int32_t i= node->getFirstArgumentIndex(); i<node->getNumChildren(); ++i)
      {
      TR::parmLayoutResult fakeParm;
      TR::Node *parmNode = node->getChild(i);
      layoutParm(parmNode, sizeOfOutGoingArgs, intReg, floatReg, fakeParm);
      }

   if (sizeOfOutGoingArgs > cg()->getLargestOutgoingArgSize())
      {
      cg()->setLargestOutgoingArgSize(sizeOfOutGoingArgs);
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "setUpStackSizeForCallNode setLargestOutgoingArgSize %d(for call node %p)\n", sizeOfOutGoingArgs, node);
      }

   }

TR::Register *
TR::IA32SystemLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR_ASSERT(false, "TR::IA32SystemLinkage::buildIndirectDispatch is not supported.\n");
   return NULL;
   }

TR::Register *
TR::IA32SystemLinkage::buildVolatileAndReturnDependencies(
    TR::Node *callNode,
    TR::RegisterDependencyConditions *deps)
   {
   TR_ASSERT(deps != NULL, "expected register dependencies");

   // Allocate virtual register for return value
   //
   TR::Register     *integerReturnReg = NULL;
   TR::Register     *longReturnReg    = NULL;
   TR::Register     *fpReturnReg      = NULL;
   TR::Register     *returnReg        = NULL; // An alias for one of the above
   switch (callNode->getDataType())
      {
      case TR::NoType:
         break;
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         returnReg = integerReturnReg = cg()->allocateRegister();
         break;
      case TR::Address:
         returnReg = integerReturnReg = cg()->allocateCollectedReferenceRegister();
         break;
      case TR::Float:
         returnReg = fpReturnReg = cg()->allocateSinglePrecisionRegister(TR_X87);
         break;
      case TR::Double:
         returnReg = fpReturnReg = cg()->allocateRegister(TR_X87);
         break;
      case TR::Int64:
         returnReg = longReturnReg = (TR::Register*)cg()->allocateRegisterPair(cg()->allocateRegister(), cg()->allocateRegister());
         break;
      case TR::Aggregate:
      default:
         TR_ASSERT(false, "return type still not supported");
      }

   // Deps for volatile regs
   //

   // TODO: This should be less dependent on the real registers, but the way
   // _properties is set up makes that very hard.
   TR_ASSERT(_properties.getIntegerReturnRegister()  == TR::RealRegister::eax, "assertion failure");
   TR_ASSERT(_properties.getLongLowReturnRegister()  == TR::RealRegister::eax, "assertion failure");
   TR_ASSERT(_properties.getLongHighReturnRegister() == TR::RealRegister::edx, "assertion failure");
   TR_ASSERT(_properties.getFloatReturnRegister()    == TR::RealRegister::st0, "assertion failure");

   if (longReturnReg)
      {
      deps->addPostCondition(returnReg->getLowOrder(), TR::RealRegister::eax, cg());
      deps->addPostCondition(returnReg->getHighOrder(), TR::RealRegister::edx, cg());
      }
   else if (integerReturnReg)
      {
      deps->addPostCondition(returnReg, TR::RealRegister::eax, cg());
      deps->addPostCondition(cg()->allocateRegister(), TR::RealRegister::edx, cg());
      }
   else
      {
      deps->addPostCondition(cg()->allocateRegister(), TR::RealRegister::eax, cg());
      deps->addPostCondition(cg()->allocateRegister(), TR::RealRegister::edx, cg());
      }

   deps->addPostCondition(cg()->allocateRegister(), TR::RealRegister::ecx, cg());

   // st0
   if (fpReturnReg)
      {
      deps->addPostCondition(returnReg, _properties.getFloatReturnRegister(), cg());
      }
   else
      {
      // No need for a dummy dep here because FPREGSPILL instruction takes care of it
      }

 // The reg dependency is left open intentionally, and need to be closed by
 // the caller. The reason is because, child class might call this method, while
 // adding more register dependecies;  if we close the reg dependency here,
 // the child class could add NO more register dependencies.

   return returnReg;
   }

int32_t TR::IA32SystemLinkage::buildArgs(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *deps)
   {
   // Push args in reverse order for a system call
   //
   int32_t argSize = 0;
   int32_t firstArg = callNode->getFirstArgumentIndex();
   for (int i = callNode->getNumChildren() - 1; i >= firstArg; i--)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Address:
         case TR::Int32:
            TR::IA32LinkageUtils::pushIntegerWordArg(child, cg());
            argSize += 4;
            break;
         case TR::Float:
            TR::IA32LinkageUtils::pushFloatArg(child,cg());
            argSize += 4;
            break;
         case TR::Double:
            TR::IA32LinkageUtils::pushDoubleArg(child, cg());
            argSize += 8;
            break;
         case TR::Int64:
            TR::IA32LinkageUtils::pushLongArg(child, cg());
            argSize += 8;
            break;
         case TR::Aggregate:
         default:
            TR_ASSERT(0, "Attempted to push unknown type");
            break;
         }
      }
   return argSize;
   }

TR::Register *TR::IA32SystemLinkage::buildDirectDispatch(TR::Node *callNode, bool spillFPRegs)
   {
   TR::RealRegister    *stackPointerReg = machine()->getRealRegister(TR::RealRegister::esp);
   TR::SymbolReference *methodSymRef    = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol    = callNode->getSymbol()->castToMethodSymbol();
   TR::ILOpCodes        callOpCodeValue = callNode->getOpCodeValue();

   if (!methodSymbol->isHelper())
      diagnostic("Building call site for %s\n", methodSymbol->getMethod()->signature(trMemory()));

   TR::RegisterDependencyConditions  *deps;
   deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)6, cg());
   TR::Register *returnReg = buildVolatileAndReturnDependencies(callNode, deps);
   deps->stopAddingConditions();

   TR::RegisterDependencyConditions  *dummy = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)0, cg());

   uint32_t  argSize = buildArgs(callNode, dummy);

   TR::Register* targetAddressReg = NULL;
   TR::MemoryReference* targetAddressMem = NULL;

   // Call-out
   int32_t stackAdjustment = cg()->getProperties().getCallerCleanup() ? 0 : -argSize;
   cg()->resetIsLeafMethod();
   TR::X86ImmInstruction* instr = generateImmSymInstruction(CALLImm4, callNode, (uintptr_t)methodSymbol->getMethodAddress(), methodSymRef, cg());
   instr->setAdjustsFramePointerBy(stackAdjustment);

   if (cg()->getProperties().getCallerCleanup() && argSize > 0)
      {
      // Clean up arguments
      //
      generateRegImmInstruction(
         (argSize <= 127) ? ADD4RegImms : ADD4RegImm4,
         callNode,
         stackPointerReg,
         argSize,
         cg()
         );
      }

   // Label denoting end of dispatch code sequence; dependencies are on
   // this label rather than on the call
   //
   TR::LabelSymbol *endSystemCallSequence = generateLabelSymbol(cg());
   generateLabelInstruction(LABEL, callNode, endSystemCallSequence, deps, cg());

   // Stop using the killed registers that are not going to persist
   //
   if (deps)
      stopUsingKilledRegisters(deps, returnReg);

   // If the method returns a floating point value that is not used, insert a dummy store to
   // eventually pop the value from the floating point stack.
   //
   if ((callNode->getDataType() == TR::Float ||
        callNode->getDataType() == TR::Double) &&
       callNode->getReferenceCount() == 1)
      {
      generateFPSTiST0RegRegInstruction(FSTRegReg, callNode, returnReg, returnReg, cg());
      }

   if (cg()->enableRegisterAssociations())
      associatePreservedRegisters(deps, returnReg);

   return returnReg;
   }
