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

#include "arm/codegen/OMRLinkage.hpp"

#include "arm/codegen/ARMInstruction.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "arm/codegen/ARMPrivateLinkage.hpp"
#endif
#include "arm/codegen/ARMSystemLinkage.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

#define LOCK_R14
// #define DEBUG_ARM_LINKAGE

void OMR::ARM::Linkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   }

void OMR::ARM::Linkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   }

void OMR::ARM::Linkage::initARMRealRegisterLinkage()
   {
   }

bool OMR::ARM::Linkage::hasToBeOnStack(TR::ParameterSymbol *parm)
   {
   return false;
   }

void OMR::ARM::Linkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   }

TR::Instruction *OMR::ARM::Linkage::saveArguments(TR::Instruction *cursor)
   {
   TR::CodeGenerator     *codeGen    = self()->cg();
   TR::Machine           *machine    = self()->machine();
   TR::RealRegister      *stackPtr   = machine->getRealRegister(self()->getProperties().getStackPointerRegister());
   TR::ResolvedMethodSymbol   *bodySymbol = self()->comp()->getJittedMethodSymbol();
   TR::Node                 *firstNode  = self()->comp()->getStartTree()->getNode();

   const TR::ARMLinkageProperties& properties = self()->getProperties();

   ListIterator<TR::ParameterSymbol> paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol *paramCursor = paramIterator.getFirst();
   uint32_t            numIntArgs  = 0;

   // TODO Use STM for storing more than one register.
   //
   for (paramCursor=paramIterator.getFirst(); paramCursor!=NULL; paramCursor=paramIterator.getNext())
      {
      TR::RealRegister     *argRegister;
//      int32_t lri = paramCursor->getLinkageRegisterIndex();

      int32_t ai  = paramCursor->getAllocatedIndex();
      int32_t                 offset = paramCursor->getParameterOffset();

      if (numIntArgs >= properties.getNumIntArgRegs())
         break;

//      if (lri >=  0)
//         {
         // We cannot have linkageRegisterIndex as it breaks GRA. Using numIntArgs == 0 to distinguish "this" pointer.
         bool hasToBeOnStack = paramCursor->isReferencedParameter() || paramCursor->isParmHasToBeOnStack() ||
            ((numIntArgs == 0) && paramCursor->isCollectedReference());

         switch (paramCursor->getDataType())
            {
            case TR::Float:
               // in emulation, floats are treated like ints
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
            case TR::Address:
               if (hasToBeOnStack)
                  {
                  argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
                  cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), argRegister, cursor);
                  }
               numIntArgs++;
               break;
            case TR::Double:
               // in emulation, doubles are treated like longs
            case TR::Int64:
            	if (hasToBeOnStack)
                  {
                  argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
                  cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), argRegister, cursor);
                  if (numIntArgs < properties.getNumIntArgRegs()-1)
                     {
                     argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs+1));
                     cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset+4, codeGen), argRegister, cursor);
                     }
                  }
               numIntArgs += 2;
               break;
            }
//         }
      }
   return(cursor);
   }

TR::Instruction *OMR::ARM::Linkage::loadUpArguments(TR::Instruction *cursor)
   {
   TR::CodeGenerator     *codeGen    = self()->cg();
   TR::Machine           *machine    = self()->machine();
   TR::RealRegister      *stackPtr   = machine->getRealRegister(self()->getProperties().getStackPointerRegister());
   TR::ResolvedMethodSymbol   *bodySymbol = self()->comp()->getJittedMethodSymbol();
   TR::Node                 *firstNode  = self()->comp()->getStartTree()->getNode();
   ListIterator<TR::ParameterSymbol>   paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol      *paramCursor = paramIterator.getFirst();
   uint32_t                 numIntArgs = 0;
   const TR::ARMLinkageProperties& properties = self()->getProperties();

   while (paramCursor != NULL && numIntArgs < properties.getNumIntArgRegs())
      {
      TR::RealRegister     *argRegister;
      int32_t                 offset = paramCursor->getParameterOffset();
      bool hasToLoadFromStack = paramCursor->isReferencedParameter() || paramCursor->isParmHasToBeOnStack();

      switch (paramCursor->getDataType())
         {
         case TR::Float:
            // in emulation, floats are treated like ints
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (hasToLoadFromStack &&
                numIntArgs<properties.getNumIntArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
               cursor = generateTrg1MemInstruction(codeGen, ARMOp_ldr, firstNode, argRegister, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), cursor);
               }
            numIntArgs++;
            break;
         case TR::Address:
             if (numIntArgs<properties.getNumIntArgRegs())
                {
                argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
                cursor = generateTrg1MemInstruction(codeGen, ARMOp_ldr, firstNode, argRegister, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), cursor);
                }
             numIntArgs++;
            break;
         case TR::Double:
            // in emulation, doubles are treated like longs
         case TR::Int64:
            if (hasToLoadFromStack &&
                numIntArgs<properties.getNumIntArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
               cursor = generateTrg1MemInstruction(codeGen, ARMOp_ldr, firstNode, argRegister, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), cursor);
               if (numIntArgs < properties.getNumIntArgRegs()-1)
                  {
                  argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs+1));
                  cursor = generateTrg1MemInstruction(codeGen, ARMOp_ldr, firstNode, argRegister, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset + 4, codeGen), cursor);
                  }
               }
            numIntArgs += 2;
            break;
         }
      paramCursor = paramIterator.getNext();
      }
   return cursor;
   }

TR::Instruction *OMR::ARM::Linkage::flushArguments(TR::Instruction *cursor)
   {
   TR::CodeGenerator     *codeGen    = self()->cg();
   TR::Machine           *machine    = self()->machine();
   TR::RealRegister      *stackPtr   = machine->getRealRegister(self()->getProperties().getStackPointerRegister());
   TR::ResolvedMethodSymbol   *bodySymbol = self()->comp()->getJittedMethodSymbol();
   TR::Node                 *firstNode  = self()->comp()->getStartTree()->getNode();
   ListIterator<TR::ParameterSymbol>   paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol      *paramCursor = paramIterator.getFirst();
   uint32_t                 numIntArgs = 0;
   const TR::ARMLinkageProperties& properties = self()->getProperties();

   while (paramCursor != NULL && numIntArgs < properties.getNumIntArgRegs())
      {
      TR::RealRegister     *argRegister;
      int32_t                 offset = paramCursor->getParameterOffset();
      bool hasToSaveToStack = paramCursor->isReferencedParameter() || paramCursor->isParmHasToBeOnStack();

      switch (paramCursor->getDataType())
         {
         case TR::Float:
            // in emulation, floats are treated like ints
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (hasToSaveToStack &&
                numIntArgs<properties.getNumIntArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
               cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), argRegister, cursor);
               }
            numIntArgs++;
            break;
         case TR::Address:
             if (numIntArgs<properties.getNumIntArgRegs())
                {
                argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
                cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), argRegister, cursor);
                }
             numIntArgs++;
            break;
         case TR::Double:
            // in emulation, doubles are treated like longs
         case TR::Int64:
            if (hasToSaveToStack &&
                numIntArgs<properties.getNumIntArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
               cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, codeGen), argRegister, cursor);
               if (numIntArgs < properties.getNumIntArgRegs()-1)
                  {
                  argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs+1));
                  cursor = generateMemSrc1Instruction(codeGen, ARMOp_str, firstNode, new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset + 4, codeGen), argRegister, cursor);
                  }
               }
            numIntArgs += 2;
            break;
         }
      paramCursor = paramIterator.getNext();
      }
   return cursor;
   }

TR::Register *OMR::ARM::Linkage::pushJNIReferenceArg(TR::Node *child)
   {
   TR::Register *pushReg = NULL;
   bool         clobberable  = false;
   if (child->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference *symRef = child->getSymbolReference();
      TR::StaticSymbol    *sym    = symRef->getSymbol()->getStaticSymbol();

      if (sym)
         {
         if (sym->isAddressOfClassObject())
            {
            pushReg = self()->pushAddressArg(child);
            }
         else
            {
            TR::Register           *addrReg = self()->cg()->evaluate(child);
            TR::MemoryReference *tempMR  = new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()->cg());

            // statics are non-collectable
            pushReg = self()->cg()->allocateRegister();

            // pass NULL if ref is NULL, else pass &ref
            generateTrg1MemInstruction(self()->cg(), ARMOp_ldr, child, pushReg, tempMR);
            generateSrc1ImmInstruction(self()->cg(), ARMOp_cmp, child, pushReg, 0, 0);
            TR::Instruction *instr = generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, pushReg, addrReg);
            instr->setConditionCode(ARMConditionCodeNE);

            tempMR->decNodeReferenceCounts();
            self()->cg()->decReferenceCount(child);
            clobberable = true;
            }
         }
      else // must be loadaddr of parm or local
         {
         if (child->pointsToNonNull())
            {
            pushReg = self()->pushAddressArg(child);
            }
         else if (child->pointsToNull())
            {
            pushReg = self()->cg()->allocateRegister();
            generateTrg1ImmInstruction(self()->cg(), ARMOp_mov, child, pushReg, 0, 0);
            self()->cg()->decReferenceCount(child);
            clobberable = true;
            }
         else
            {
            TR::Register *addrReg = self()->cg()->evaluate(child);
            TR::Register *objReg  = self()->cg()->allocateCollectedReferenceRegister();
            TR::LabelSymbol *startLabel   = generateLabelSymbol(self()->cg());
            TR::LabelSymbol *doneLabel    = generateLabelSymbol(self()->cg());

            startLabel->setStartInternalControlFlow();
            doneLabel->setEndInternalControlFlow();
            generateLabelInstruction(self()->cg(), ARMOp_label, child, startLabel);

            TR::MemoryReference *tempMR = new (self()->trHeapMemory()) TR::MemoryReference(addrReg, 0, self()->cg());
            generateTrg1MemInstruction(self()->cg(), ARMOp_ldr, child, objReg, tempMR);

            if (child->getReferenceCount() > 1)
               {
               // Since this points at a parm or local location, it is non-collectable.
               TR::Register *tempReg = self()->cg()->allocateRegister();
               generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, tempReg, addrReg);
               pushReg = tempReg;
               }
            else
               {
               pushReg = addrReg;
               }
            clobberable = true;

            generateSrc1ImmInstruction(self()->cg(), ARMOp_cmp, child, objReg, 0, 0);
            generateConditionalBranchInstruction(self()->cg(), child, ARMConditionCodeNE, doneLabel);
            generateTrg1ImmInstruction(self()->cg(), ARMOp_mov, child, pushReg, 0, 0);

            TR::RegisterDependencyConditions *deps = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, self()->trMemory());
            TR::addDependency(deps, pushReg, TR::RealRegister::NoReg, TR_GPR, self()->cg());
            TR::addDependency(deps, objReg,  TR::RealRegister::NoReg, TR_GPR, self()->cg());

            generateLabelInstruction(self()->cg(), ARMOp_label, child, doneLabel, deps);
            self()->cg()->decReferenceCount(child);
            }
         }
      }
   else
      {
      pushReg = self()->pushAddressArg(child);
      }

   if (!clobberable && child->getReferenceCount() > 0)
      {
      TR::Register *tempReg =
         pushReg->containsCollectedReference() ? self()->cg()->allocateCollectedReferenceRegister()
                                               : self()->cg()->allocateRegister();

      generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, tempReg, pushReg);
      pushReg = tempReg;
      }

   return pushReg;
   }

TR::Register *OMR::ARM::Linkage::pushIntegerWordArg(TR::Node *child)
   {
   TR::CodeGenerator *codeGen      = self()->cg();
   TR::Register         *pushRegister = NULL;
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst() && (child->getDataType() != TR::Float)) /* XXX: We need to fix buildARMLinkageArgs(). */
      {
      pushRegister = codeGen->allocateRegister();
      armLoadConstant(child, child->getInt(), pushRegister, codeGen);
      }
   else
      {
      pushRegister = codeGen->evaluate(child);
      child->setRegister(pushRegister);
      }
   child->decReferenceCount();
   return pushRegister;
   }

TR::Register *OMR::ARM::Linkage::pushAddressArg(TR::Node *child)
   {
   TR::CodeGenerator *codeGen      = self()->cg();
   TR::Register         *pushRegister = NULL;
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst())
      {
      bool isClass = child->isClassPointerConstant();
      pushRegister = codeGen->allocateRegister();
      if (isClass && self()->cg()->wantToPatchClassPointer((TR_OpaqueClassBlock*)child->getAddress(), child))
         {
         loadAddressConstantInSnippet(self()->cg(), child, child->getAddress(), pushRegister);
         }
      else
         {
          if (child->isMethodPointerConstant())
             loadAddressConstant(self()->cg(), child, child->getAddress(), pushRegister, NULL, false, TR_RamMethodSequence);
          else
             loadAddressConstant(self()->cg(), child, child->getAddress(), pushRegister);
         }
      }
   else
      {
      pushRegister = codeGen->evaluate(child);
      child->setRegister(pushRegister);
      }
   child->decReferenceCount();
   return pushRegister;
   }

TR::Register *OMR::ARM::Linkage::pushLongArg(TR::Node *child)
   {
   TR::CodeGenerator *codeGen      = self()->cg();
   TR::Register         *pushRegister = NULL;
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst() /* && (child->getDataType() != TR::Double) */)
      {
      TR::Register *lowRegister  = codeGen->allocateRegister();
      TR::Register *highRegister = codeGen->allocateRegister();
      pushRegister = codeGen->allocateRegisterPair(lowRegister, highRegister);
#ifdef DEBUG_ARM_LINKAGE
printf("pushing long arg: low = %d, high = %d\n", child->getLongIntLow(), child->getLongIntHigh()); fflush(stdout);
#endif
      armLoadConstant(child, child->getLongIntLow(), lowRegister, codeGen);
      armLoadConstant(child, child->getLongIntHigh(), highRegister, codeGen);
      }
   else
      {
      pushRegister = codeGen->evaluate(child);
      child->setRegister(pushRegister);
      }
   child->decReferenceCount();
   return pushRegister;
   }

TR::Register *OMR::ARM::Linkage::pushFloatArg(TR::Node *child)
   {
   // if (isSmall()) return 0;

   TR::Register *pushRegister = self()->cg()->evaluate(child);
   child->setRegister(pushRegister);
   child->decReferenceCount();
   return(pushRegister);
   }

TR::Register *OMR::ARM::Linkage::pushDoubleArg(TR::Node *child)
   {
   // if (isSmall()) return 0;

   TR::Register *pushRegister = self()->cg()->evaluate(child);
   child->setRegister(pushRegister);
   child->decReferenceCount();
   return(pushRegister);
   }

int32_t OMR::ARM::Linkage::numArgumentRegisters(TR_RegisterKinds kind)
   {
   switch (kind)
      {
      case TR_GPR:
         return self()->getProperties().getNumIntArgRegs();
      case TR_FPR:
         return self()->getProperties().getNumFloatArgRegs();
      default:
         return 0;
      }
   }

int32_t OMR::ARM::Linkage::buildARMLinkageArgs(TR::Node                            *callNode,
                                           TR::RegisterDependencyConditions *dependencies,
                                           TR::Register*                       &vftReg,
                                           TR_LinkageConventions               conventions,
                                           bool                                isVirtualOrJNI)
   {
   const TR::ARMLinkageProperties &properties = self()->getProperties();
   TR::Compilation *comp = self()->comp();
   TR::CodeGenerator  *codeGen      = self()->cg();
   TR::ARMMemoryArgument *pushToMemory = NULL;
   void                 *stackMark;

   bool isHelper  = (conventions == TR_Helper);
   bool isVirtual = (isVirtualOrJNI && conventions == TR_Private);
   bool isSystem  = (conventions == TR_System);
   bool bigEndian = TR::Compiler->target.cpu.isBigEndian();
   int32_t   i;
   int32_t   totalSize = 0;
   uint32_t  numIntegerArgs = 0;
   uint32_t  numFloatArgs = 0;
   uint32_t  numMemArgs = 0;
   uint32_t  firstArgumentChild = callNode->getFirstArgumentIndex();
   TR::DataType callNodeDataType = callNode->getDataType();
   TR::DataType resType = callNode->getType();
   TR::MethodSymbol *callSymbol = callNode->getSymbol()->castToMethodSymbol();

   int32_t from, to, step;
   if (isHelper)
      {
      from = callNode->getNumChildren() - 1;
      to   = callNode->getFirstArgumentIndex() - 1;
      step = -1;
      }
   else
      {
      from = callNode->getFirstArgumentIndex();
      to   = callNode->getNumChildren();
      step = 1;
      }

   // reserve register for class pointer during the dispatch sequence
   if (isVirtual)
      {
      for (i = 0; i < firstArgumentChild; i++)
         {
         TR::Node *child = callNode->getChild(i);
         vftReg = self()->cg()->evaluate(child);
         self()->cg()->decReferenceCount(child);
         }
      TR::addDependency(dependencies, vftReg, TR::RealRegister::NoReg, TR_GPR, self()->cg());  // dep 0

#ifndef LOCK_R14
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr14, TR_GPR, cg());  // dep 2
#endif
      }

   uint32_t numIntArgRegs   = properties.getNumIntArgRegs();
   uint32_t numFloatArgRegs = properties.getNumFloatArgRegs();
   TR::RealRegister::RegNum specialArgReg = TR::RealRegister::NoReg;
   switch (callSymbol->getMandatoryRecognizedMethod())
      {
#ifdef J9_PROJECT_SPECIFIC
      // Node: special long args are still only passed in one GPR
      case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
         specialArgReg = self()->getProperties().getJ9MethodArgumentRegister();
         // Other args go in memory
         numIntArgRegs   = 0;
         numFloatArgRegs = 0;
         break;
      case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
         specialArgReg = self()->getProperties().getVTableIndexArgumentRegister();
         break;
      case TR::java_lang_invoke_MethodHandle_invokeWithArgumentsHelper:
         numIntArgRegs   = 0;
         numFloatArgRegs = 0;
         break;
#endif
      }

   //TODO move the TR::addDependency(gr11) to the latter part of the method
   if (isVirtual && (specialArgReg != TR::RealRegister::gr11))
      {
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, self()->cg());  // dep 1
      }

   if (specialArgReg != TR::RealRegister::NoReg)
      {
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp, "Special arg %s in %s\n",
            comp->getDebug()->getName(callNode->getChild(from)),
            comp->getDebug()->getName(self()->machine()->getRealRegister(specialArgReg)));
         }
      // Skip the special arg in the first loop
      from += step;
      }

   for (i = from; (isHelper && i > to) || (!isHelper && i < to); i += step)
      {
      switch (callNode->getChild(i)->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
#ifdef __VFP_FP__
	 case TR::Float:
#endif
            if (numIntegerArgs >= numIntArgRegs)
               numMemArgs++;
            numIntegerArgs++;
            totalSize += 4;
            break;
         case TR::Int64:
#ifdef __VFP_FP__
	 case TR::Double:
#endif
            if (numIntegerArgs + 1 == numIntArgRegs)
               numMemArgs++;
            else if (numIntegerArgs + 1 > numIntArgRegs)
               numMemArgs += 2;
            numIntegerArgs += 2;
            totalSize += 8;
            break;
#ifndef __VFP_FP__
         case TR::Float:
            if (numFloatArgs >= numFloatArgRegs)
               numMemArgs++;
            numFloatArgs++;
            totalSize += 4;
            break;
         case TR::Double:
            if (numFloatArgs >= numFloatArgRegs)
               numMemArgs++;
            numFloatArgs++;
            totalSize += 8;
            break;
#endif // ifndef __VFP_FP__
         }
      }

#ifdef DEBUG_ARM_LINKAGE
// const char *sig = callNode->getSymbol()->getResolvedMethodSymbol()->signature();
const char *sig = "CALL";
printf("%s: numIntegerArgs %d numMemArgs %d\n", sig,  numIntegerArgs, numMemArgs); fflush(stdout);
#endif


   // From here, down, all stack memory allocations will expire / die when the function returns.
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());
   if (numMemArgs > 0)
      {
      pushToMemory = new(self()->trStackMemory()) TR::ARMMemoryArgument[numMemArgs];
      }

   if (specialArgReg)
      from -= step;  // we do want to process special args in the following loop

   numIntegerArgs = 0;
   numFloatArgs = 0;

   int32_t argSize = -(properties.getOffsetToFirstParm());
   int32_t memArg  = 0;
   for (i = from; (isHelper && i > to) || (!isHelper && i < to); i += step)
      {
      TR::Node               *child = callNode->getChild(i);
      TR::DataType            childType = child->getDataType();
      TR::Register           *reg;
      TR::Register           *tempReg;
      TR::MemoryReference *tempMR;
      bool isSpecialArg = (i == from && specialArgReg != TR::RealRegister::NoReg);

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address: // have to do something for GC maps here
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
	 case TR::Float:
#endif
            if (childType == TR::Address)
               reg = self()->pushAddressArg(child);
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
            else if (childType == TR::Float)
               {
               TR::Register *tempReg = self()->cg()->allocateRegister();
               reg = self()->pushFloatArg(child);
               /* Passing a float argument in an integer register.*/
               generateTrg1Src1Instruction(self()->cg(), ARMOp_fmrs, callNode, tempReg, reg);
               reg = tempReg;
               }
#endif
            else
               reg = self()->pushIntegerWordArg(child);
            if (isSpecialArg)
               {
               if (specialArgReg == properties.getIntegerReturnRegister(0))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = self()->cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = self()->cg()->allocateRegister();
                  dependencies->addPreCondition(reg, specialArgReg);
                  dependencies->addPostCondition(resultReg, properties.getIntegerReturnRegister(0));
                  }
               else
                  {
                  TR::addDependency(dependencies, reg, specialArgReg, TR_GPR, self()->cg());
                  }
               }
            else
               {
               if (numIntegerArgs < numIntArgRegs)
                  {
                  if (!self()->cg()->canClobberNodesRegister(child, 0) && (childType != TR::Float))
                     {
                     if (reg->containsCollectedReference())
                        tempReg = self()->cg()->allocateCollectedReferenceRegister();
                     else
                        tempReg = self()->cg()->allocateRegister();
                     generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, tempReg, reg);
                     reg = tempReg;
                     }
                  if (numIntegerArgs == 0 /* &&
                      (callNode->getDataType() !=
                       child->getDataType() ||
                       !reg->containsCollectedReference()) &&
                      (callNode->getDataType() == TR::Address ||
                       child->getDataType() == TR::Address) */)
                     {
                     TR::Register *resultReg;
                     if (resType.isAddress())
                        resultReg = self()->cg()->allocateCollectedReferenceRegister();
/*                     else if (callNode->getDataType() == TR::Double)
                       {

                       } */
                     else
                        resultReg = self()->cg()->allocateRegister();
                     dependencies->addPreCondition(reg, TR::RealRegister::gr0);
                     dependencies->addPostCondition(resultReg, TR::RealRegister::gr0);
                     }
                  else if (numIntegerArgs == 1 && (resType.isInt64() || resType.isDouble()))
                     {
                     TR::Register *resultReg = self()->cg()->allocateRegister();
                     dependencies->addPreCondition(reg, TR::RealRegister::gr1);
                     dependencies->addPostCondition(resultReg, TR::RealRegister::gr1);
                     }
                  else
                     {
                     TR::addDependency(dependencies, reg, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, self()->cg());
                     }
                  }
               else
                  {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing 32-bit arg %d %d %d %d\n", numIntegerArgs, memArg, totalSize, argSize); fflush(stdout);
#endif
                  tempMR = self()->getOutgoingArgumentMemRef(totalSize, argSize, reg, ARMOp_str, pushToMemory[memArg++]);
                  }
               numIntegerArgs++;
               argSize += 4;
               }
            break;
         case TR::Int64:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
	 case TR::Double:
            if (childType == TR::Double)
               {
               reg = self()->pushDoubleArg(child);
               TR::Register *lowReg  = self()->cg()->allocateRegister();
               TR::Register *highReg = self()->cg()->allocateRegister();
               TR::RegisterPair *tempReg = self()->cg()->allocateRegisterPair(lowReg, highReg);
               generateTrg2Src1Instruction(self()->cg(), ARMOp_fmrrd, callNode, lowReg, highReg, reg);
               reg = tempReg;
               }
            else
#endif
               reg = self()->pushLongArg(child);
            if (isSpecialArg)
               {
               // Note: special arg regs use only one reg even on 32-bit platforms.
               // If the special arg is of type TR::Int64, that only means we don't
               // care about the top 32 bits.
               TR::Register *specialArgRegister = reg->getRegisterPair()? reg->getLowOrder() : reg;
               if (specialArgReg == properties.getIntegerReturnRegister(0))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = self()->cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = self()->cg()->allocateRegister();
                  dependencies->addPreCondition(specialArgRegister, specialArgReg);
                  dependencies->addPostCondition(resultReg, properties.getIntegerReturnRegister(0));
                  }
               else
                  {
                  TR::addDependency(dependencies, specialArgRegister, specialArgReg, TR_GPR, self()->cg());
                  }
               }
            else
               {
               if (numIntegerArgs < numIntArgRegs)
                  {
                  if (!self()->cg()->canClobberNodesRegister(child, 0) && (childType != TR::Double))
                     {
                     tempReg = self()->cg()->allocateRegister();

                     if (bigEndian)
                        {
                        generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, tempReg, reg->getRegisterPair()->getHighOrder());
                        reg = self()->cg()->allocateRegisterPair(reg->getRegisterPair()->getLowOrder(), tempReg);
                        }
                     else
                        {
                        generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, tempReg, reg->getRegisterPair()->getLowOrder());
                        reg = self()->cg()->allocateRegisterPair(tempReg, reg->getRegisterPair()->getHighOrder());
                        }
                     }

                  dependencies->addPreCondition(bigEndian ? reg->getRegisterPair()->getHighOrder() : reg->getRegisterPair()->getLowOrder(), properties.getIntegerArgumentRegister(numIntegerArgs));
                  if (numIntegerArgs == 0 && resType.isAddress())
                     {
                     dependencies->addPostCondition(self()->cg()->allocateCollectedReferenceRegister(), TR::RealRegister::gr0);
                     }
                  else
                     {
                     dependencies->addPostCondition(self()->cg()->allocateRegister(), properties.getIntegerArgumentRegister(numIntegerArgs));
                     }

                  if (numIntegerArgs + 1 < numIntArgRegs)
                     {
                     if (!self()->cg()->canClobberNodesRegister(child, 0) && (childType != TR::Double))
                        {
                        tempReg = self()->cg()->allocateRegister();
                        if (bigEndian)
                           {
                           generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, tempReg, reg->getRegisterPair()->getLowOrder());
                           reg->getRegisterPair()->setLowOrder(tempReg, self()->cg());
                           }
                        else
                           {
                           generateTrg1Src1Instruction(self()->cg(), ARMOp_mov, child, tempReg, reg->getRegisterPair()->getHighOrder());
                           reg->getRegisterPair()->setHighOrder(tempReg, self()->cg());
                           }
                        }
                     dependencies->addPreCondition(bigEndian ? reg->getRegisterPair()->getLowOrder() : reg->getRegisterPair()->getHighOrder(), properties.getIntegerArgumentRegister(numIntegerArgs + 1));
                     dependencies->addPostCondition(self()->cg()->allocateRegister(), properties.getIntegerArgumentRegister(numIntegerArgs + 1));
                     }
                  else
                     {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing %s word of 64-bit arg %d %d %d %d\n", bigEndian ? "low" : "high", numIntegerArgs, memArg, totalSize, argSize); fflush(stdout);
#endif
                     tempMR = self()->getOutgoingArgumentMemRef(totalSize, argSize, bigEndian ? reg->getRegisterPair()->getLowOrder() : reg->getRegisterPair()->getHighOrder(), ARMOp_str, pushToMemory[memArg++]);
                     }
                  }
               else
                  {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing 64-bit arg %d %d %d %d\n", numIntegerArgs, memArg, totalSize, argSize); fflush(stdout);
#endif
                  tempMR = self()->getOutgoingArgumentMemRef(totalSize, argSize, bigEndian ? reg->getRegisterPair()->getLowOrder() : reg->getRegisterPair()->getHighOrder(), ARMOp_str, pushToMemory[memArg++]);
                  tempMR = self()->getOutgoingArgumentMemRef(totalSize, argSize + 4, bigEndian ? reg->getRegisterPair()->getHighOrder() : reg->getRegisterPair()->getLowOrder(), ARMOp_str, pushToMemory[memArg++]);
                  }
               numIntegerArgs += 2;
               argSize += 8;
               }
            break;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
         case TR::Float:
#if 0
            // TODO FLOAT
            argSize += 4;
            reg = pushFloatArg(child);
            if (numFloatArgs < numFloatArgRegs)
               {
               if (child->getReferenceCount() > 0)
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), reg, tempReg, ARMOp_fmr);
                  reg = tempReg;
                  }
               TR::addDependency(dependencies, reg, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else
               {
               tempMR = getOutgoingArgumentMemRef(totalSize - argSize, reg, ARMOp_stfs, pushToMemory[memArg++]);
               }
            numFloatArgs++;
#endif
            break;
         case TR::Double:
#if 0
            // TODO FLOAT
            argSize += 8;
            reg = pushDoubleArg(child);
            if (numFloatArgs < numFloatArgRegs)
               {
               if (child->getReferenceCount() > 0)
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), reg, tempReg, ARMOp_fmr);
                  reg = tempReg;
                  }
               TR::addDependency(dependencies, reg, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else
               {
               tempMR = getOutgoingArgumentMemRef(totalSize - argSize, reg, ARMOp_stfd, pushToMemory[memArg++]);
               }
            numFloatArgs++;
#endif
            break;
#endif
         }
      }

   /*
   if (isVirtual)
      {
      static char *dontAddVFTRegDep = feGetEnv("TR_dontAddVFTRegDep");
      if (callSymbol->isComputed() && !dontAddVFTRegDep)
         {
         if (vftReg->getRegisterPair())
            vftReg = vftReg->getLowOrder();
         dependencites->getPreConditions()->setDependencyInfo(0, vftReg, getProperties().getComputedCallTargetRegister());
         dependencites->getPostConditions()->setDependencyInfo(0, vftReg, getProperties().getComputedCallTargetRegister());
         }
      }
   */
   for (i = 0; i < properties.getNumIntArgRegs(); i++)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::gr0 + i);
      if (realReg == specialArgReg)
         continue; // already added deps above.  No need to add them here.
      if (!dependencies->searchPreConditionRegister(realReg))
         {
         if (realReg == properties.getIntegerArgumentRegister(0) && resType.isAddress())
            {
            dependencies->addPreCondition(self()->cg()->allocateRegister(), TR::RealRegister::gr0);
            dependencies->addPostCondition(self()->cg()->allocateCollectedReferenceRegister(), TR::RealRegister::gr0);
            }
         else if ((realReg == TR::RealRegister::gr1) && (resType.isInt64() || resType.isDouble()))
            {
            dependencies->addPreCondition(self()->cg()->allocateRegister(), TR::RealRegister::gr1);
            dependencies->addPostCondition(self()->cg()->allocateRegister(), TR::RealRegister::gr1);
            }
         else
            {
            TR::addDependency(dependencies, NULL, realReg, TR_GPR, self()->cg());
            }
         }
      }

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   /* D0 - D7 are volatile. TODO: live register. */
   for (i = 0; i < 8 /*properties.getNumFloatArgRegs()*/; i++)
      {
      TR::addDependency(dependencies, NULL, (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i), TR_FPR, self()->cg());
      }
#endif
   // add dependencies for other volatile registers; for virtual calls,
   // dependencies on gr11 and gr14 have already been added above
   switch(conventions)
      {
      case TR_Private:
         if ((!isVirtual) && (specialArgReg != TR::RealRegister::gr11))
            TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, self()->cg());
         // deliberate fall-through
      case TR_Helper:
         TR::addDependency(dependencies, NULL, TR::RealRegister::gr4, TR_GPR, self()->cg());
         TR::addDependency(dependencies, NULL, TR::RealRegister::gr5, TR_GPR, self()->cg());
         // deliberate fall-through
      case TR_System:
#ifndef LOCK_R14
         if (!isVirtual)
            TR::addDependency(dependencies, NULL, TR::RealRegister::gr14, TR_GPR, cg());
#endif
         break;
      }

   if (numMemArgs > 0)
      {
      int32_t memFrom, memTo, memStep;
#if 0
      if (isJNI)
         {
         memFrom  = numMemArgs - 1;
         memTo    = 0;
         memStep  = -1;
         }
      else
         {
         memFrom  = 0;
         memTo    = numMemArgs;
         memStep  = 1;
         }

      for (i = memFrom; (isJNI &&  i >= memTo) || (!isJNI && i < memTo); i += memStep)
#else
      for (i = 0; i < numMemArgs; i++)
#endif
         {
#ifdef DEBUG_ARM_LINKAGE
printf("pushing mem arg %d of %d (in reg %x) to [sp + %d]... ", i, numMemArgs, pushToMemory[i].argRegister, pushToMemory[i].argMemory->getOffset()); fflush(stdout);
#endif
         generateMemSrc1Instruction(self()->cg(),
                                    pushToMemory[i].opCode,
                                    callNode,
                                    pushToMemory[i].argMemory,
                                    pushToMemory[i].argRegister);
#ifdef DEBUG_ARM_LINKAGE
printf("done\n"); fflush(stdout);
#endif
         }
      }

   return totalSize;
   }


TR::Register *OMR::ARM::Linkage::buildARMLinkageDirectDispatch(TR::Node *callNode, bool isSystem)
   {
   TR::CodeGenerator *codeGen = self()->cg();

   const TR::ARMLinkageProperties &pp = self()->getProperties();
   TR::RegisterDependencyConditions *dependencies =
      new (self()->trHeapMemory()) TR::RegisterDependencyConditions(pp.getNumberOfDependencyGPRegisters() + 8 /*pp.getNumFloatArgRegs()*/,
                                             pp.getNumberOfDependencyGPRegisters() + 8 /*pp.getNumFloatArgRegs()*/, self()->trMemory());

   TR::Register *vftReg = NULL;
   int32_t argSize = self()->buildArgs(callNode, dependencies, vftReg, false);
   dependencies->stopAddingConditions();

   TR::SymbolReference *callSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();

   bool isMyself = self()->comp()->isRecursiveMethodTarget(callSymbol);

   if (callSymRef->getReferenceNumber() >= TR_ARMnumRuntimeHelpers)
      self()->fe()->reserveTrampolineIfNecessary(self()->comp(), callSymRef, false);

   TR::Instruction *gcPoint;
   TR::Register    *returnRegister;

   if ((callSymbol->isJITInternalNative() ||
        (!callSymRef->isUnresolved() && !callSymbol->isInterpreted() && ((self()->comp()->compileRelocatableCode() && callSymbol->isHelper()) || !self()->comp()->compileRelocatableCode()))))
      {
      gcPoint = generateImmSymInstruction(codeGen,
                                          ARMOp_bl,
                                          callNode,
                                          isMyself ? 0 : (uintptr_t)callSymbol->getMethodAddress(),
                                          dependencies,
                                          callNode->getSymbolReference());
      }
   else
      {
#ifdef J9_PROJECT_SPECIFIC
      TR::LabelSymbol *label = generateLabelSymbol(codeGen);
      TR::Snippet     *snippet;

      if (callSymRef->isUnresolved() || self()->comp()->compileRelocatableCode())
         {
         snippet = new (self()->trHeapMemory()) TR::ARMUnresolvedCallSnippet(codeGen, callNode, label, argSize);
         }
      else
         {
         snippet = new (self()->trHeapMemory()) TR::ARMCallSnippet(codeGen, callNode, label, argSize);
         }

      codeGen->addSnippet(snippet);
      gcPoint = generateImmSymInstruction(codeGen,
                                          ARMOp_bl,
                                          callNode,
                                          0,
                                          dependencies,
                                          new (self()->trHeapMemory()) TR::SymbolReference(self()->comp()->getSymRefTab(), label),
                                          snippet);
#else
      TR_ASSERT(false, "Attempting to handle Java in non-Java project");
#endif
      }
   gcPoint->ARMNeedsGCMap(pp.getPreservedRegisterMapForGC());
   self()->machine()->setLinkRegisterKilled(true);
   codeGen->setHasCall();
   TR::DataType resType = callNode->getType();

   switch(callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::acall:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::fcall:
#endif
         returnRegister = dependencies->searchPostConditionRegister(pp.getIntegerReturnRegister());
         if (resType.isFloatingPoint())
            {
            TR::Register *tempReg = codeGen->allocateSinglePrecisionRegister();
            TR::Instruction *cursor = generateTrg1Src1Instruction(codeGen, ARMOp_fmsr, callNode, tempReg, returnRegister);
            returnRegister = tempReg;
            }
         break;
      case TR::lcall:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::dcall:
#endif
         {
         TR::Register *lowReg;
         TR::Register *highReg;
         lowReg = dependencies->searchPostConditionRegister(pp.getLongLowReturnRegister());
         highReg = dependencies->searchPostConditionRegister(pp.getLongHighReturnRegister());
         returnRegister = codeGen->allocateRegisterPair(lowReg, highReg);
         if (resType.isDouble())
            {
            TR::Register *tempReg = codeGen->allocateRegister(TR_FPR);
            TR::Instruction *cursor = generateTrg1Src2Instruction(codeGen, ARMOp_fmdrr, callNode, tempReg, lowReg, highReg);
            returnRegister = tempReg;
            }
         break;
         }
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::fcall:
      case TR::dcall:
         returnRegister = dependencies->searchPostConditionRegister(pp.getFloatReturnRegister());
         break;
#endif
      case TR::call:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown direct call Opcode.");
      }

   callNode->setRegister(returnRegister);
   return(returnRegister);
   }

bool TR::ARMLinkageProperties::_isBigEndian;
