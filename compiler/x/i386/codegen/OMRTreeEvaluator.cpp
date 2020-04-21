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

#include <stdint.h>
#include <stdlib.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/X86Evaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/BinaryCommutativeAnalyser.hpp"
#include "x/codegen/CompareAnalyser.hpp"
#include "x/codegen/ConstantDataSnippet.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "x/codegen/IntegerMultiplyDecomposer.hpp"
#include "x/codegen/SubtractAnalyser.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "codegen/IA32LinkageUtils.hpp"
#include "codegen/IA32PrivateLinkage.hpp"
#endif

class TR_OpaqueMethodBlock;

///////////////////////
//
// Helper functions
//

TR::Register *OMR::X86::I386::TreeEvaluator::longArithmeticCompareRegisterWithImmediate(
      TR::Node       *node,
      TR::Register   *cmpRegister,
      TR::Node       *immedChild,
      TR_X86OpCodes firstBranchOpCode,
      TR_X86OpCodes secondBranchOpCode,
      TR::CodeGenerator *cg)
   {
   int32_t      lowValue       = immedChild->getLongIntLow();
   int32_t      highValue      = immedChild->getLongIntHigh();

   TR::LabelSymbol *startLabel    = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *highDoneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   TR::Register *targetRegister = cg->allocateRegister();

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   generateLabelInstruction(LABEL, node, startLabel, cg);
   compareGPRegisterToImmediate(node, cmpRegister->getHighOrder(), highValue, cg);
   generateRegInstruction(SETNE1Reg, node, targetRegister, cg);
   generateLabelInstruction(JNE4, node, highDoneLabel, cg);

   compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
   generateRegInstruction(SETNE1Reg, node, targetRegister, cg);
   generateLabelInstruction(firstBranchOpCode, node, doneLabel, cg);
   generateRegInstruction(NEG1Reg, node, targetRegister, cg);
   generateLabelInstruction(JMP4, node, doneLabel, cg);

   generateLabelInstruction(LABEL, node, highDoneLabel, cg);
   generateLabelInstruction(secondBranchOpCode, node, doneLabel, cg);
   generateRegInstruction(NEG1Reg, node, targetRegister, cg);
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 3, cg);
   deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
   deps->addPostCondition(targetRegister, TR::RealRegister::ByteReg, cg);

   generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

   generateRegRegInstruction(MOVSXReg4Reg1, node, targetRegister, targetRegister, cg);

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::compareLongAndSetOrderedBoolean(
      TR::Node       *node,
      TR_X86OpCodes highSetOpCode,
      TR_X86OpCodes lowSetOpCode,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Register *targetRegister;

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::lconst &&
       testRegister == NULL && performTransformation(comp, "O^O compareLongAndSetOrderedBoolean: checking that the second child node does not have an assigned register: %d\n", testRegister))
      {
      int32_t      lowValue    = secondChild->getLongIntLow();
      int32_t      highValue   = secondChild->getLongIntHigh();
      TR::Node     *firstChild  = node->getFirstChild();
      TR::Register *cmpRegister = cg->evaluate(firstChild);

      TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      startLabel->setStartInternalControlFlow();
      doneLabel->setEndInternalControlFlow();
      generateLabelInstruction(LABEL, node, startLabel, cg);

      compareGPRegisterToImmediate(node, cmpRegister->getHighOrder(), highValue, cg);

      targetRegister = cg->allocateRegister();

      if (cg->enableRegisterInterferences())
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

      generateRegInstruction(highSetOpCode, node, targetRegister, cg);
      generateLabelInstruction(JNE4, node, doneLabel, cg);

      compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
      generateRegInstruction(lowSetOpCode, node, targetRegister, cg);

      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 3, cg);
      deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
      deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
      deps->addPostCondition(targetRegister, TR::RealRegister::NoReg, cg);
      generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

      // Result of lcmpXX is an integer.
      //
      generateRegRegInstruction(MOVSXReg4Reg1, node, targetRegister, targetRegister, cg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      targetRegister = temp.longOrderedBooleanAnalyser(node,
                                                       highSetOpCode,
                                                       lowSetOpCode);
      }
   return targetRegister;
   }

void OMR::X86::I386::TreeEvaluator::compareLongsForOrder(
      TR::Node          *node,
      TR_X86OpCodes    highOrderBranchOp,
      TR_X86OpCodes    highOrderReversedBranchOp,
      TR_X86OpCodes    lowOrderBranchOp,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::lconst &&
       testRegister == NULL && performTransformation(comp, "O^O compareLongsForOrder: checking that the second child node does not have an assigned register: %d\n", testRegister))
      {
      int32_t        lowValue          = secondChild->getLongIntLow();
      int32_t        highValue         = secondChild->getLongIntHigh();
      TR::Node       *firstChild        = node->getFirstChild();
      // KAS:: need delayed evaluation here and use of memimm instructions to reduce
      // register pressure
      TR::Register   *cmpRegister       = cg->evaluate(firstChild);
      TR::LabelSymbol *startLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *doneLabel        = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();
      List<TR::Register> popRegisters(cg->trMemory());

      startLabel->setStartInternalControlFlow();
      doneLabel->setEndInternalControlFlow();
      generateLabelInstruction(LABEL, node, startLabel, cg);

      compareGPRegisterToImmediate(node, cmpRegister->getHighOrder(), highValue, cg);

      TR::RegisterDependencyConditions  *deps = NULL;
      if (node->getNumChildren() == 3)
         {
         TR::Node *third = node->getChild(2);
         cg->evaluate(third);
         deps = generateRegisterDependencyConditions(third, cg, 2, &popRegisters);
         deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
         deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
         deps->stopAddingConditions();

         generateLabelInstruction(highOrderBranchOp, node, destinationLabel, deps, cg);
         generateLabelInstruction(JNE4, node, doneLabel, deps, cg);
         compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
         generateLabelInstruction(lowOrderBranchOp, node, destinationLabel, deps, cg);
         }
      else
         {
         generateLabelInstruction(highOrderBranchOp, node, destinationLabel, cg);
         generateLabelInstruction(JNE4, node, doneLabel, cg);
         compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
         generateLabelInstruction(lowOrderBranchOp, node, destinationLabel, cg);
         deps = generateRegisterDependencyConditions((uint8_t)0, 2, cg);
         deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
         deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
         deps->stopAddingConditions();
         }

      generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

      if (deps)
         deps->setMayNeedToPopFPRegisters(true);

      if (!popRegisters.isEmpty())
         {
         ListIterator<TR::Register> popRegsIt(&popRegisters);
         for (TR::Register *popRegister = popRegsIt.getFirst(); popRegister != NULL; popRegister = popRegsIt.getNext())
            {
            generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
            cg->stopUsingRegister(popRegister);
            }
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.longOrderedCompareAndBranchAnalyser(node,
                                               lowOrderBranchOp,
                                               highOrderBranchOp,
                                               highOrderReversedBranchOp);
      }
   }

///////////////////////
//
// Member functions
//
// TODO:AMD64: Reorder these to match their declaration in TreeEvaluator.hpp
//
TR::Register *OMR::X86::I386::TreeEvaluator::aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *reg = loadConstant(node, node->getInt(), TR_RematerializableAddress, cg);
   node->setRegister(reg);
   return reg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *lowRegister,
               *highRegister;
   int32_t      lowValue  = node->getLongIntLow();
   int32_t      highValue = node->getLongIntHigh();

   if (abs(lowValue - highValue) <= 128)
      {
      if (lowValue <= highValue)
         {
         lowRegister = cg->allocateRegister();
         highRegister = loadConstant(node, highValue, TR_RematerializableInt, cg);
         if (lowValue == highValue)
            {
            generateRegRegInstruction(MOV4RegReg, node, lowRegister, highRegister, cg);
            }
         else
            {
            generateRegMemInstruction(LEA4RegMem, node, lowRegister,
                                      generateX86MemoryReference(highRegister, lowValue - highValue, cg), cg);
            }
         }
      else
         {
         lowRegister = loadConstant(node, lowValue, TR_RematerializableInt, cg);
         highRegister = cg->allocateRegister();
         generateRegMemInstruction(LEA4RegMem, node, highRegister,
                                   generateX86MemoryReference(lowRegister, highValue - lowValue, cg), cg);
         }
      }
   else
      {
      lowRegister = loadConstant(node, lowValue, TR_RematerializableInt, cg);
      highRegister = loadConstant(node, highValue, TR_RematerializableInt, cg);
      }

   TR::RegisterPair *longRegister = cg->allocateRegisterPair(lowRegister, highRegister);
   node->setRegister(longRegister);
   return longRegister;
   }

// also handles ilstore
TR::Register *OMR::X86::I386::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *valueChild;
   TR::SymbolReference *symRef = node->getSymbolReference();
   bool isVolatile = false;

   if (symRef && !symRef->isUnresolved())
      {
      TR::Symbol *symbol = symRef->getSymbol();
      isVolatile = symbol->isVolatile();
#ifdef J9_PROJECT_SPECIFIC
      TR_OpaqueMethodBlock *caller = node->getOwningMethod();
      if (isVolatile && caller)
         {
         TR_ResolvedMethod *m = comp->fe()->createResolvedMethod(cg->trMemory(), caller, node->getSymbolReference()->getOwningMethod(comp));
         if (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicLong_lazySet)
            {
            isVolatile = false;
            }
         }
#endif
      }

   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   // Handle special cases
   //
   if (!isVolatile &&
       valueChild->getRegister() == NULL &&
       valueChild->getReferenceCount() == 1)
      {
      // Special case storing a double value into a long variable
      //
      if (valueChild->getOpCodeValue() == TR::dbits2l &&
          !valueChild->normalizeNanValues())
         {
         if (node->getOpCode().isIndirect())
            {
            node->setChild(1, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::dstorei);
            dstoreEvaluator(node, cg);
            node->setChild(1, valueChild);
            TR::Node::recreate(node, TR::lstorei);
            }
         else
            {
            node->setChild(0, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::dstore);
            dstoreEvaluator(node, cg);
            node->setChild(0, valueChild);
            TR::Node::recreate(node, TR::lstore);
            }
         cg->decReferenceCount(valueChild);
         return NULL;
         }
      }

   TR::MemoryReference  *lowMR  = NULL;
   TR::MemoryReference  *highMR;
   TR::Instruction *instr = NULL;

   if (!isVolatile &&
       valueChild->getOpCodeValue() == TR::lconst &&
       valueChild->getRegister() == NULL)
      {
      lowMR  = generateX86MemoryReference(node, cg);
      highMR = generateX86MemoryReference(*lowMR, 4, cg);

      int32_t lowValue  = valueChild->getLongIntLow();
      int32_t highValue = valueChild->getLongIntHigh();

      if (lowValue == highValue)
         {
         TR::Register *valueReg = loadConstant(node, lowValue, TR_RematerializableInt, cg);
         instr = generateMemRegInstruction(S4MemReg, node, lowMR, valueReg, cg);
         generateMemRegInstruction(S4MemReg, node, highMR, valueReg, cg);
         cg->stopUsingRegister(valueReg);
         }
      else
         {
         instr = generateMemImmInstruction(S4MemImm4, node, lowMR, lowValue, cg);
         generateMemImmInstruction(S4MemImm4, node, highMR, highValue, cg);
         }
      }

   else
      {
      TR::Machine *machine = cg->machine();
      TR::Register *eaxReg=NULL, *edxReg=NULL, *ecxReg=NULL, *ebxReg=NULL;
      TR::RegisterDependencyConditions  *deps = NULL;

      TR::Register *valueReg = cg->evaluate(valueChild);
      if (valueReg)
         {
         lowMR  = generateX86MemoryReference(node, cg);
         highMR = generateX86MemoryReference(*lowMR, 4, cg);

         if (isVolatile)
            {
            if (cg->useSSEForDoublePrecision() && performTransformation(comp, "O^O Using SSE for volatile store %s\n", cg->getDebug()->getName(node)))
               {
               //Get stack piece
               TR::MemoryReference *stackLow  = cg->machine()->getDummyLocalMR(TR::Int64);
               TR::MemoryReference *stackHigh = generateX86MemoryReference(*stackLow, 4, cg);

               //generate: stack1 <- (valueReg->getloworder())    [S4MemReg]
               instr = generateMemRegInstruction(S4MemReg, node, stackLow, valueReg->getLowOrder(), cg);
               //generate: stack1_plus4 <- (valueReg->gethighorder())
               generateMemRegInstruction(S4MemReg, node, stackHigh, valueReg->getHighOrder(), cg);


               //generate: xmm <- stack1
               TR::MemoryReference *stack = generateX86MemoryReference(*stackLow, 0, cg);
               //Allocate XMM Reg
               TR::Register *reg = cg->allocateRegister(TR_FPR);
               generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, reg, stack, cg);

               //generate: lowMR <- xmm
               generateMemRegInstruction(MOVSDMemReg, node, lowMR, reg, cg);

               //Stop using  xmm
               cg->stopUsingRegister(reg);
               }
            else
               {
               TR_ASSERT( cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction(), "Assumption of support of the CMPXCHG8B instruction failed in lstoreEvaluator()" );

               eaxReg = cg->allocateRegister();
               edxReg = cg->allocateRegister();
               ecxReg = cg->allocateRegister();
               ebxReg = cg->allocateRegister();
               deps = generateRegisterDependencyConditions((uint8_t)4, (uint8_t)4, cg);

               deps->addPostCondition(eaxReg, TR::RealRegister::eax, cg);
               deps->addPostCondition(edxReg, TR::RealRegister::edx, cg);
               deps->addPostCondition(ecxReg, TR::RealRegister::ecx, cg);
               deps->addPostCondition(ebxReg, TR::RealRegister::ebx, cg);
               deps->addPreCondition(eaxReg, TR::RealRegister::eax, cg);
               deps->addPreCondition(edxReg, TR::RealRegister::edx, cg);
               deps->addPreCondition(ecxReg, TR::RealRegister::ecx, cg);
               deps->addPreCondition(ebxReg, TR::RealRegister::ebx, cg);

               instr = generateRegMemInstruction(L4RegMem, node, eaxReg, lowMR, cg); // forming the EDX:EAX pair
               generateRegMemInstruction(L4RegMem, node, edxReg, highMR, cg);
               lowMR->setIgnoreVolatile();
               highMR->setIgnoreVolatile();
               generateRegRegInstruction(MOV4RegReg, node, ebxReg, valueReg->getLowOrder(), cg); // forming the ECX:EBX pair
               generateRegRegInstruction(MOV4RegReg, node, ecxReg, valueReg->getHighOrder(), cg);

               TR::MemoryReference  *cmpxchgMR = generateX86MemoryReference(node, cg);
               generateMemInstruction (cg->comp()->target().isSMP() ? LCMPXCHG8BMem : CMPXCHG8BMem, node, cmpxchgMR, deps, cg);

               cg->stopUsingRegister(eaxReg);
               cg->stopUsingRegister(edxReg);
               cg->stopUsingRegister(ecxReg);
               cg->stopUsingRegister(ebxReg);
               }
            }
         else if(symRef && symRef->isUnresolved() && symRef->getSymbol()->isVolatile() && (!comp->getOption(TR_DisableNewX86VolatileSupport) && cg->comp()->target().is32Bit()) )
            {
            TR_ASSERT( cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction(), "Assumption of support of the CMPXCHG8B instruction failed in lstoreEvaluator()" );
            eaxReg = cg->allocateRegister();
            edxReg = cg->allocateRegister();
            ecxReg = cg->allocateRegister();
            ebxReg = cg->allocateRegister();
            deps = generateRegisterDependencyConditions((uint8_t)4, (uint8_t)4, cg);

            deps->addPostCondition(eaxReg, TR::RealRegister::eax, cg);
            deps->addPostCondition(edxReg, TR::RealRegister::edx, cg);
            deps->addPostCondition(ecxReg, TR::RealRegister::ecx, cg);
            deps->addPostCondition(ebxReg, TR::RealRegister::ebx, cg);
            deps->addPreCondition(eaxReg, TR::RealRegister::eax, cg);
            deps->addPreCondition(edxReg, TR::RealRegister::edx, cg);
            deps->addPreCondition(ecxReg, TR::RealRegister::ecx, cg);
            deps->addPreCondition(ebxReg, TR::RealRegister::ebx, cg);

            generateRegRegInstruction(MOV4RegReg, node, ebxReg, valueReg->getLowOrder(), cg); // forming the ECX:EBX pair
            generateRegRegInstruction(MOV4RegReg, node, ecxReg, valueReg->getHighOrder(), cg);

            instr = generateRegMemInstruction(L4RegMem, node, eaxReg, lowMR, cg); // forming the EDX:EAX pair
            generateRegMemInstruction(L4RegMem, node, edxReg, highMR, cg);
            lowMR->setIgnoreVolatile();
            highMR->setIgnoreVolatile();
            lowMR->setProcessAsLongVolatileLow();
            highMR->setProcessAsLongVolatileHigh();

            TR::MemoryReference  *cmpxchgMR = generateX86MemoryReference(node, cg);
            generateMemInstruction (cg->comp()->target().isSMP() ? LCMPXCHG8BMem : CMPXCHG8BMem, node, cmpxchgMR, deps, cg);

            cg->stopUsingRegister(eaxReg);
            cg->stopUsingRegister(edxReg);
            cg->stopUsingRegister(ecxReg);
            cg->stopUsingRegister(ebxReg);
            }
         else
            {
            instr = generateMemRegInstruction(S4MemReg, node, lowMR, valueReg->getLowOrder(), cg);
            generateMemRegInstruction(S4MemReg, node, highMR, valueReg->getHighOrder(), cg);

            TR::SymbolReference& mrSymRef = lowMR->getSymbolReference();
            if (mrSymRef.isUnresolved())
               {
               padUnresolvedDataReferences(node, mrSymRef, cg);
               }
            }
         }
      }
   cg->decReferenceCount(valueChild);
   if (lowMR && !(valueChild->isDirectMemoryUpdate() && node->getOpCode().isIndirect()))
      lowMR->decNodeReferenceCounts(cg);


   if (node->getSymbolReference()->getSymbol()->isVolatile())
      {
      TR_OpaqueMethodBlock *caller = node->getOwningMethod();
      if ((lowMR || highMR) && caller)
         {
#ifdef J9_PROJECT_SPECIFIC
         TR_ResolvedMethod *m = comp->fe()->createResolvedMethod(cg->trMemory(), caller, node->getSymbolReference()->getOwningMethod(comp));
         if (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicLong_lazySet)
            {
            if (lowMR)
               lowMR->setIgnoreVolatile();
            if (highMR)
               highMR->setIgnoreVolatile();
            }
#endif
         }
      }

   if (instr && node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);

   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Restore the default FPCW if it has been forced to single precision mode.
   //
   TR::Compilation *comp = cg->comp();
   if (cg->enableSinglePrecisionMethods() &&
       comp->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, DOUBLE_PRECISION_ROUND_TO_NEAREST), cg), cg);
      }

   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *returnRegister = cg->evaluate(firstChild);
   TR::Register *lowRegister    = returnRegister->getLowOrder();
   TR::Register *highRegister   = returnRegister->getHighOrder();

   const TR::X86LinkageProperties &linkageProperties = cg->getProperties();
   TR::RealRegister::RegNum machineLowReturnRegister =
      linkageProperties.getLongLowReturnRegister();

   TR::RealRegister::RegNum machineHighReturnRegister =
      linkageProperties.getLongHighReturnRegister();

   TR::RegisterDependencyConditions *dependencies = NULL;
   if (machineLowReturnRegister != TR::RealRegister::NoReg)
      {
      dependencies = generateRegisterDependencyConditions((uint8_t)3, 0, cg);
      dependencies->addPreCondition(lowRegister, machineLowReturnRegister, cg);
      dependencies->addPreCondition(highRegister, machineHighReturnRegister, cg);
      dependencies->stopAddingConditions();
      }

   if (linkageProperties.getCallerCleanup())
      {
      generateInstruction(RET, node, dependencies, cg);
      }
   else
      {
      generateImmInstruction(RETImm2, node, 0, dependencies, cg);
      }

   if (comp->getJittedMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      comp->setReturnInfo(TR_LongReturn);
      }

   cg->decReferenceCount(firstChild);
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairAddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register            *targetRegister = NULL;
   TR::Node                *secondChild    = node->getSecondChild();
   TR::Node                *firstChild     = node->getFirstChild();
   TR::Instruction         *instr;
   TR::MemoryReference  *lowMR,
                          *highMR;

   // See if we can generate a direct memory operation. In this case, no
   // target register is generated, and we return NULL to the caller (which
   // should be a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();
   bool firstChildAlreadyEvaluated = false;
   bool needsEflags = NEED_CC(node) || (node->getOpCodeValue() == TR::luaddc);

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         lowMR = generateX86MemoryReference(*reg->getMemRef(), 4, cg);
         firstChildAlreadyEvaluated = true;
         }
      else
         {
         lowMR = generateX86MemoryReference(firstChild, cg, false);
         }
      highMR = generateX86MemoryReference(*lowMR, 4, cg);
      }

   if (!needsEflags &&
       secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL &&
       (isMemOp || firstChild->getReferenceCount() == 1))
      {
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);

      int32_t lowValue  = secondChild->getLongIntLow();
      int32_t highValue = secondChild->getLongIntHigh();

      if (lowValue >= -128 && lowValue <= 127)
         {
         if (isMemOp)
            instr = generateMemImmInstruction(ADD4MemImms, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(ADD4RegImms, node, targetRegister->getLowOrder(), lowValue, cg);
         }
      else if (lowValue == 128)
         {
         if (isMemOp)
            instr = generateMemImmInstruction(SUB4MemImms, node, lowMR, -128, cg);
         else
            instr = generateRegImmInstruction(SUB4RegImms, node, targetRegister->getLowOrder(), -128, cg);

         // Use SBB of the negation of (highValue + 1) for the high order,
         // since we are getting the carry bit in the wrong sense from the SUB.
         highValue = -(highValue + 1);
         }
      else
         {
         if (isMemOp)
            instr = generateMemImmInstruction(ADD4MemImm4, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(ADD4RegImm4, node, targetRegister->getLowOrder(), lowValue, cg);
         }

      TR_X86OpCodes opCode;

      if (highValue >= -128 && highValue <= 127)
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = SBB4MemImms;
            else
               opCode = SBB4RegImms;
         else
            if (isMemOp)
               opCode = ADC4MemImms;
            else
               opCode = ADC4RegImms;
         }
      else
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = SBB4MemImm4;
            else
               opCode = SBB4RegImm4;
         else
            if (isMemOp)
               opCode = ADC4MemImm4;
            else
               opCode = ADC4RegImm4;
         }

      if (isMemOp)
         {
         generateMemImmInstruction(opCode, node, highMR, highValue, cg);

         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by const", node);
         }
      else
         {
         generateRegImmInstruction(opCode, node, targetRegister->getHighOrder(), highValue, cg);
         }
      }
   else if (isMemOp && !needsEflags)
      {
      TR::Register *tempReg = cg->evaluate(secondChild);
      instr = generateMemRegInstruction(ADD4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      generateMemRegInstruction(ADC4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.longAddAnalyser(node);
      return node->getRegister();
      }

   if (isMemOp)
      {
      if (!firstChildAlreadyEvaluated)
         lowMR->decNodeReferenceCounts(cg);
      else
         lowMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairSubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *firstChild     = node->getFirstChild();
   TR::Node                *secondChild    = node->getSecondChild();
   TR::Register            *targetRegister = NULL;
   TR::Instruction         *instr;
   TR::MemoryReference  *lowMR,
                          *highMR;

   // See if we can generate a direct memory operation. In this case, no
   // target register is generated, and we return NULL to the caller (which
   // should be a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();
   bool firstChildAlreadyEvaluated = false;
   bool needsEflags = NEED_CC(node) || (node->getOpCodeValue() == TR::lusubb);

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         lowMR = generateX86MemoryReference(*reg->getMemRef(), 4, cg);
         firstChildAlreadyEvaluated = true;
         }
      else
         {
         lowMR = generateX86MemoryReference(firstChild, cg, false);
         }
      highMR = generateX86MemoryReference(*lowMR, 4, cg);
      }

   if (!needsEflags &&
       secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL &&
       (isMemOp || firstChild->getReferenceCount() == 1))
      {
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);

      int32_t lowValue = secondChild->getLongIntLow();
      int32_t highValue = secondChild->getLongIntHigh();

      if (lowValue >= -128 && lowValue <= 127)
         {
         if (isMemOp)
            instr = generateMemImmInstruction(SUB4MemImms, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(SUB4RegImms, node, targetRegister->getLowOrder(), lowValue, cg);
         }
      else if (lowValue == 128)
         {
         if (isMemOp)
            instr = generateMemImmInstruction(ADD4MemImms, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(ADD4RegImms, node, targetRegister->getLowOrder(), -128, cg);

         // Use ADC of the negation of (highValue + 1) for the high order,
         // since we are getting the carry bit in the wrong sense from the ADD.
         highValue = -(highValue + 1);
         }
      else
         {
         if (isMemOp)
            instr = generateMemImmInstruction(SUB4MemImm4, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(SUB4RegImm4, node, targetRegister->getLowOrder(), lowValue, cg);
         }

      TR_X86OpCodes opCode;

      if (highValue >= -128 && highValue <= 127)
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = ADC4MemImms;
            else
               opCode = ADC4RegImms;
         else
            if (isMemOp)
               opCode = SBB4MemImms;
            else
               opCode = SBB4RegImms;
         }
      else
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = ADC4MemImm4;
            else
               opCode = ADC4RegImm4;
         else
            if (isMemOp)
               opCode = SBB4MemImm4;
            else
               opCode = SBB4RegImm4;
         }

      if (isMemOp)
         {
         generateMemImmInstruction(opCode, node, highMR, highValue, cg);

         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by const", node);
         }
      else
         {
         generateRegImmInstruction(opCode, node, targetRegister->getHighOrder(), highValue, cg);
         }
      }
   else if (isMemOp && !needsEflags)
      {
      TR::Register *tempReg = cg->evaluate(secondChild);
      instr = generateMemRegInstruction(SUB4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      generateMemRegInstruction(SBB4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by var", node);
      }
   else
      {
      TR_X86SubtractAnalyser  temp(cg);
      temp.longSubtractAnalyser(node);
      return node->getRegister();
      }

   if (isMemOp)
      {
      if (!firstChildAlreadyEvaluated)
         lowMR->decNodeReferenceCounts(cg);
      else
         lowMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairDualMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT((node->getOpCodeValue() == TR::lumulh) || (node->getOpCodeValue() == TR::lmul), "Unexpected operator. Expected lumulh or lmul.");
   if (node->isDualCyclic() && (node->getChild(2)->getReferenceCount() == 1))
      {
      // other part of this dual is not used, and is dead
      TR::Node *pair = node->getChild(2);
      // break dual into parts before evaluation
      // pair has only one reference, so need to avoid recursiveness removal of its subtree
      pair->incReferenceCount();
      node->removeChild(2);
      pair->removeChild(2);
      cg->decReferenceCount(pair->getFirstChild());
      cg->decReferenceCount(pair->getSecondChild());
      cg->decReferenceCount(pair);
      // evaluate this part again
      return cg->evaluate(node);
      }
   else
      {
      // use long dual analyser
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.longDualMultiplyAnalyser(node);
      }

   return node->getRegister();
   }


TR::Register *OMR::X86::I386::TreeEvaluator::integerPairMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *lowRegister    = NULL;
   TR::Register *highRegister;
   TR::Register *targetRegister;
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Node     *secondChild    = node->getSecondChild();
   TR::X86ImmSymInstruction  *instr;
   TR::MemoryReference    *nodeMR = NULL;
   TR::Compilation *comp = cg->comp();

   bool needsUnsignedHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   if (node->isDualCyclic() || needsUnsignedHighMulOnly)
      {
      return integerPairDualMulEvaluator(node, cg);
      }

   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      TR_X86OpCodes opCode;
      int32_t        lowValue  = secondChild->getLongIntLow();
      int32_t        highValue = secondChild->getLongIntHigh();
      int64_t        value     = secondChild->getLongInt();
      int32_t        absValue  = (highValue < 0) ? -highValue : highValue;
      bool           firstIU2L = false;
      bool           intMul    = false;
      TR::ILOpCodes   firstOp   = firstChild->getOpCodeValue();
      if (firstOp == TR::iu2l)
         {
         firstIU2L = true;
         }
      if (firstOp == TR::i2l && value >= TR::getMinSigned<TR::Int32>() && value <= TR::getMaxSigned<TR::Int32>())
         {
         }
      if (lowValue == 0 && highValue == 0)
         {
         lowRegister = cg->allocateRegister();
         generateRegRegInstruction(XOR4RegReg, node, lowRegister, lowRegister, cg);
         highRegister = cg->allocateRegister();
         generateRegRegInstruction(XOR4RegReg, node, highRegister, highRegister, cg);
         }
      else if (cg->convertMultiplyToShift(node))
         {
         // Don't do a direct memory operation if we have to negate the result
         //
         if (absValue != highValue)
            node->setDirectMemoryUpdate(false);
         targetRegister = cg->evaluate(node);
         if (absValue != highValue)
            {
            generateRegInstruction(NEG4Reg, node, targetRegister->getLowOrder(), cg);
            generateRegImmInstruction(ADC4RegImms, node, targetRegister->getHighOrder(), 0, cg);
            generateRegInstruction(NEG4Reg, node, targetRegister->getHighOrder(), cg);
            }
         return targetRegister;
         }
      else if (lowValue == 0)
         {
         TR::Register *multiplierRegister;
         if (absValue == 3 || absValue == 5 || absValue == 9)
            {
            if (firstIU2L                            &&
                firstChild->getReferenceCount() == 1 &&
                firstChild->getRegister() == 0)
               {
               firstChild         = firstChild->getFirstChild();
               multiplierRegister = cg->evaluate(firstChild);
               }
            else
               {
               multiplierRegister = cg->evaluate(firstChild)->getLowOrder();
               }
            TR::MemoryReference  *tempMR  = generateX86MemoryReference(cg);
            if (firstChild->getReferenceCount() > 1)
               {
               highRegister = cg->allocateRegister();
               }
            else
               {
               highRegister = multiplierRegister;
               }
            tempMR->setBaseRegister(multiplierRegister);
            tempMR->setIndexRegister(multiplierRegister);
            tempMR->setStrideFromMultiplier(absValue - 1);
            generateRegMemInstruction(LEA4RegMem, node, highRegister, tempMR, cg);
            }
         else
            {
            absValue = highValue; // No negation necessary
            if (firstIU2L                            &&
                firstChild->getReferenceCount() == 1 &&
                firstChild->getRegister() == 0)
               {
               firstChild         = firstChild->getFirstChild();
               multiplierRegister = cg->evaluate(firstChild);
               }
            else if (firstChild->getOpCodeValue() == TR::lushr     &&
                     firstChild->getSecondChild()->getOpCodeValue() == TR::iconst    &&
                     firstChild->getSecondChild()->getInt() == 32 &&
                     firstChild->getReferenceCount() == 1         &&
                     firstChild->getRegister() == 0)
               {
               firstChild         = firstChild->getFirstChild();
               multiplierRegister = cg->evaluate(firstChild)->getHighOrder();
               }
            else
               {
               multiplierRegister = cg->evaluate(firstChild)->getLowOrder();
               }

            int32_t dummy = 0;
            TR_X86IntegerMultiplyDecomposer *mulDecomposer =
               new (cg->trHeapMemory()) TR_X86IntegerMultiplyDecomposer((int64_t)absValue,
                                                   multiplierRegister,
                                                   node,
                                                   cg,
                                                   firstChild->getReferenceCount() == 1 ? true : false);
            highRegister = mulDecomposer->decomposeIntegerMultiplier(dummy, 0);

            if (highRegister == 0) // cannot do the decomposition
               {
               if (firstChild->getReferenceCount() > 1 ||
                   firstChild->getRegister() != NULL   ||
                   !firstChild->getOpCode().isMemoryReference())
                  {
                  if (highValue >= -128 && highValue <= 127)
                     {
                     opCode = IMUL4RegRegImms;
                     }
                  else
                     {
                     opCode = IMUL4RegRegImm4;
                     }
                  if (firstChild->getReferenceCount() > 1 ||
                      firstChild->getRegister() != NULL)
                     {
                     highRegister = cg->allocateRegister();
                     }
                  else
                     {
                     highRegister = multiplierRegister;
                     }
                  generateRegRegImmInstruction(opCode, node, highRegister, multiplierRegister, highValue, cg);
                  }
               else
                  {
                  if (highValue >= -128 && highValue <= 127)
                     {
                     opCode = IMUL4RegMemImms;
                     }
                  else
                     {
                     opCode = IMUL4RegMemImm4;
                     }
                  nodeMR = generateX86MemoryReference(firstChild, cg);
                  highRegister = cg->allocateRegister();
                  generateRegMemImmInstruction(opCode, node, highRegister, nodeMR, highValue, cg);
                  }
               }
            }
         lowRegister = cg->allocateRegister();
         generateRegRegInstruction(XOR4RegReg, node, lowRegister, lowRegister, cg);
         if (absValue != highValue)
            {
            generateRegInstruction(NEG4Reg, node, lowRegister, cg);
            generateRegImmInstruction(ADC4RegImms, node, highRegister, 0, cg);
            generateRegInstruction(NEG4Reg, node, highRegister, cg);
            }
         }
      else if (highValue == 0)
         {
         TR::Register *multiplierRegister;
         TR::Register *sourceLow;

         if ((firstIU2L || intMul)                &&
             firstChild->getReferenceCount() == 1 &&
             firstChild->getRegister() == 0)
            {
            firstChild = firstChild->getFirstChild();
            sourceLow  = cg->evaluate(firstChild);
            }
         else
            {
            multiplierRegister = cg->evaluate(firstChild);
            sourceLow          = multiplierRegister->getLowOrder();
            }
         if (firstChild->getReferenceCount() == 1)
            {
            highRegister = sourceLow;
            }
         else
            {
            highRegister = cg->allocateRegister();
            }
         lowRegister = loadConstant(node, lowValue, TR_RematerializableInt, cg);
         TR::RegisterDependencyConditions  *dependencies = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
         dependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
         dependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
         dependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
         dependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
         if (intMul)
            {
            opCode = IMUL4AccReg;
            }
         else
            {
            opCode = MUL4AccReg;
            }
         generateRegRegInstruction(opCode, node,
                                   lowRegister,
                                   sourceLow,
                                   dependencies, cg);
         if (!firstIU2L && !intMul)
            {
            generateRegRegInstruction(TEST4RegReg, node,
                                      multiplierRegister->getHighOrder(),
                                      multiplierRegister->getHighOrder(), cg);
            TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            startLabel->setStartInternalControlFlow();
            doneLabel->setEndInternalControlFlow();
            generateLabelInstruction(LABEL, node, startLabel, cg);
            generateLabelInstruction(JE4, node, doneLabel, cg);
            // create the array of temporary allocated registers
            TR::Register *tempRegArray[TR_X86IntegerMultiplyDecomposer::MAX_NUM_REGISTERS];
            int32_t tempRegArraySize = 0;
            TR_X86IntegerMultiplyDecomposer *mulDecomposer =
               new (cg->trHeapMemory()) TR_X86IntegerMultiplyDecomposer((int64_t)lowValue,
                                                   multiplierRegister->getHighOrder(),
                                                   node,
                                                   cg,
                                                   firstChild->getReferenceCount() == 1 ? true : false);
            TR::Register *tempRegister = mulDecomposer->decomposeIntegerMultiplier(tempRegArraySize, tempRegArray);

            if (tempRegister == 0) // decomposition failed
               {
               if (lowValue >= -128 && lowValue <= 127)
                  {
                  opCode = IMUL4RegRegImms;
                  }
               else
                  {
                  opCode = IMUL4RegRegImm4;
                  }
               if (firstChild->getReferenceCount() == 1)
                  {
                  tempRegister = multiplierRegister->getHighOrder();
                  }
               else
                  {
                  tempRegister = cg->allocateRegister();
                  TR_ASSERT(tempRegArraySize<=TR_X86IntegerMultiplyDecomposer::MAX_NUM_REGISTERS,"Too many temporary registers to handle");
                  tempRegArray[tempRegArraySize++] = tempRegister;
                  }
               generateRegRegImmInstruction(opCode,
                                            node,
                                            tempRegister,
                                            multiplierRegister->getHighOrder(),
                                            lowValue, cg);
               }
            generateRegRegInstruction(ADD4RegReg, node, highRegister, tempRegister, cg);

            // add dependencies for lowRegister, highRegister, mutiplierRegister and temp registers
            uint8_t numDeps = 3 + tempRegArraySize;
            TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions(numDeps, numDeps, cg);
            deps->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
            deps->addPreCondition(highRegister, TR::RealRegister::edx, cg);
            deps->addPreCondition(multiplierRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            int i;
            for (i=0; i < tempRegArraySize; i++)
               deps->addPreCondition(tempRegArray[i], TR::RealRegister::NoReg, cg);
            deps->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
            deps->addPostCondition(highRegister, TR::RealRegister::edx, cg);
            deps->addPostCondition(multiplierRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            for (i=0; i < tempRegArraySize; i++)
               deps->addPostCondition(tempRegArray[i], TR::RealRegister::NoReg, cg);
            generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
            for (i=0; i < tempRegArraySize; i++)
               cg->stopUsingRegister(tempRegArray[i]);
            }
         }
      else // i.e. if (highValue !=0 && lowValue != 0)
         {
         TR::Register *multiplierRegister;
         TR::Register *sourceLow;
         TR::Register *tempRegister=NULL;

         if ((firstIU2L || intMul)                &&
             firstChild->getReferenceCount() == 1 &&
             firstChild->getRegister() == 0)
            {
            firstChild = firstChild->getFirstChild();
            sourceLow  = cg->evaluate(firstChild);
            }
         else
            {
            multiplierRegister = cg->evaluate(firstChild);
            sourceLow          = multiplierRegister->getLowOrder();
            }
         if (firstChild->getReferenceCount() == 1)
            {
            highRegister = sourceLow;
            }
         else
            {
            highRegister = cg->allocateRegister();
            }

         // First lowOrder * highValue

         // create the array of temporary allocated registers
         TR::Register *tempRegArray[TR_X86IntegerMultiplyDecomposer::MAX_NUM_REGISTERS];
         int32_t tempRegArraySize = 0;

         if (absValue == 3 || absValue == 5 || absValue == 9)
            {
            TR::MemoryReference  *tempMR  = generateX86MemoryReference(cg);

            tempRegister = cg->allocateRegister();
            tempRegArray[tempRegArraySize++] = tempRegister;

            tempMR->setBaseRegister(sourceLow);
            tempMR->setIndexRegister(sourceLow);
            tempMR->setStrideFromMultiplier(absValue - 1);
            generateRegMemInstruction(LEA4RegMem, node, tempRegister, tempMR, cg);
            }
         else
            {
            absValue = highValue; // No negation necessary

            TR_X86IntegerMultiplyDecomposer *mulDecomposer =
               new (cg->trHeapMemory()) TR_X86IntegerMultiplyDecomposer((int64_t)absValue,
                                                   sourceLow,
                                                   node,
                                                   cg,
                                                   (firstChild->getReferenceCount() == 1 && highRegister != sourceLow) ? true : false);
            tempRegister = mulDecomposer->decomposeIntegerMultiplier(tempRegArraySize, tempRegArray);
            if (tempRegister == 0)
               {
               tempRegister = cg->allocateRegister();
               TR_ASSERT(tempRegArraySize<=TR_X86IntegerMultiplyDecomposer::MAX_NUM_REGISTERS,"Too many temporary registers to handle");
               tempRegArray[tempRegArraySize++] = tempRegister;

               if (firstChild->getReferenceCount() > 1 ||
                   firstChild->getRegister() != NULL   ||
                   !firstChild->getOpCode().isMemoryReference())
                  {
                  if (highValue >= -128 && highValue <= 127)
                     {
                     opCode = IMUL4RegRegImms;
                     }
                  else
                     {
                     opCode = IMUL4RegRegImm4;
                     }
                  generateRegRegImmInstruction(opCode, node, tempRegister, sourceLow, highValue, cg);
                  }
               else
                  {
                  if (highValue >= -128 && highValue <= 127)
                     {
                     opCode = IMUL4RegMemImms;
                     }
                  else
                     {
                     opCode = IMUL4RegMemImm4;
                     }
                  nodeMR = generateX86MemoryReference(firstChild, cg);
                  generateRegMemImmInstruction(opCode, node, tempRegister, nodeMR, highValue, cg);
                  }
               }
            }

         TR_ASSERT(tempRegister!=NULL,"tempRegister shouldn't be NULL at this point");

         if (absValue != highValue)
            {
            generateRegInstruction(NEG4Reg, node, tempRegister, cg);
            }

         // Second lowOrder * lowValue

         lowRegister = loadConstant(node, lowValue, TR_RematerializableInt, cg);
         TR::RegisterDependencyConditions  *dependencies = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
         dependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
         dependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
         dependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
         dependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
         if (intMul)
            {
            opCode = IMUL4AccReg;
            }
         else
            {
            opCode = MUL4AccReg;
            }
         generateRegRegInstruction(opCode, node,
                                   lowRegister,
                                   sourceLow,
                                   dependencies, cg);

         // add both results of the highword
         generateRegRegInstruction(ADD4RegReg, node, highRegister, tempRegister, cg);
         // we can free up temporary registers
         for (int i=0; i < tempRegArraySize; i++)
            cg->stopUsingRegister(tempRegArray[i]);

         // Third highOrder * lowValue
         if (!firstIU2L && !intMul)
            {
            generateRegRegInstruction(TEST4RegReg, node,
                                      multiplierRegister->getHighOrder(),
                                      multiplierRegister->getHighOrder(), cg);
            TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            startLabel->setStartInternalControlFlow();
            doneLabel->setEndInternalControlFlow();
            generateLabelInstruction(LABEL, node, startLabel, cg);
            generateLabelInstruction(JE4, node, doneLabel, cg);

            // reset the array of temporary registers
            tempRegArraySize = 0;

            TR_X86IntegerMultiplyDecomposer *mulDecomposer =
               new (cg->trHeapMemory()) TR_X86IntegerMultiplyDecomposer((int64_t)lowValue,
                                                   multiplierRegister->getHighOrder(),
                                                   node,
                                                   cg,
                                                   firstChild->getReferenceCount() == 1 ? true : false);

            tempRegister = mulDecomposer->decomposeIntegerMultiplier(tempRegArraySize, tempRegArray);
            if (tempRegister == 0)
               {
               if (lowValue >= -128 && lowValue <= 127)
                  {
                  opCode = IMUL4RegRegImms;
                  }
               else
                  {
                  opCode = IMUL4RegRegImm4;
                  }
               if (firstChild->getReferenceCount() == 1)
                  {
                  tempRegister = multiplierRegister->getHighOrder();
                  }
               else
                  {
                  tempRegister = cg->allocateRegister();
                  TR_ASSERT(tempRegArraySize<=TR_X86IntegerMultiplyDecomposer::MAX_NUM_REGISTERS,"Too many temporary registers to handle");
                  tempRegArray[tempRegArraySize++] = tempRegister;
                  }
               generateRegRegImmInstruction(opCode,
                                            node,
                                            tempRegister,
                                            multiplierRegister->getHighOrder(),
                                            lowValue, cg);
               }
            generateRegRegInstruction(ADD4RegReg, node, highRegister, tempRegister, cg);

            // add dependencies for lowRegister, highRegister, mutiplierRegister and temp registers
            uint8_t numDeps = 3 + tempRegArraySize;
            TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions(numDeps, numDeps, cg);

            deps->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
            deps->addPreCondition(highRegister, TR::RealRegister::edx, cg);
            deps->addPreCondition(multiplierRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            int i;
            for (i=0; i < tempRegArraySize; i++)
               deps->addPreCondition(tempRegArray[i], TR::RealRegister::NoReg, cg);
            deps->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
            deps->addPostCondition(highRegister, TR::RealRegister::edx, cg);
            deps->addPostCondition(multiplierRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            for (i=0; i < tempRegArraySize; i++)
               deps->addPostCondition(tempRegArray[i], TR::RealRegister::NoReg, cg);
            generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
            for (i=0; i < tempRegArraySize; i++)
               cg->stopUsingRegister(tempRegArray[i]);
            }
         }
      }

   if (lowRegister == NULL)
      {
      // Evaluation hasn't been done yet; do a general long multiply.
      //
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.longMultiplyAnalyser(node);

      if (debug("traceInlineLongMultiply"))
         diagnostic("\ninlined long multiply at node [" POINTER_PRINTF_FORMAT "] in method %s", node, comp->signature());

      targetRegister = node->getRegister();
      }
   else
      {
      targetRegister = cg->allocateRegisterPair(lowRegister, highRegister);
      node->setRegister(targetRegister);
      cg->recursivelyDecReferenceCount(firstChild);
      cg->recursivelyDecReferenceCount(secondChild);
      }
   if (nodeMR)
      nodeMR->decNodeReferenceCounts(cg);

   return targetRegister;

   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairDivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *lowRegister  = cg->allocateRegister();
   TR::Register *highRegister = cg->allocateRegister();

   TR::Register *firstRegister = cg->evaluate(firstChild);
   TR::Register *secondRegister = cg->evaluate(secondChild);

   TR::Register *firstHigh = firstRegister->getHighOrder();
   TR::Register *secondHigh = secondRegister->getHighOrder();

   TR::Instruction  *divInstr = NULL;

   TR::RegisterDependencyConditions  *idivDependencies = generateRegisterDependencyConditions((uint8_t)6, 6, cg);
   idivDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
   idivDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
   idivDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
   idivDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
   idivDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   idivDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

   TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *callLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, node, startLabel, cg);

   generateRegRegInstruction(MOV4RegReg, node, highRegister, secondHigh, cg);
   generateRegRegInstruction(OR4RegReg, node, highRegister, firstHigh, cg);
   //generateRegRegInstruction(TEST4RegReg, node, highRegister, highRegister, cg);
   generateLabelInstruction(JNE4, node, callLabel, cg);

   generateRegRegInstruction(MOV4RegReg, node, lowRegister, firstRegister->getLowOrder(), cg);
   divInstr = generateRegRegInstruction(DIV4AccReg, node, lowRegister, secondRegister->getLowOrder(), idivDependencies, cg);

   cg->setImplicitExceptionPoint(divInstr);
   divInstr->setNeedsGCMap(0xFF00FFF6);

   TR::RegisterDependencyConditions  *xorDependencies1 = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
   xorDependencies1->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
   xorDependencies1->addPreCondition(highRegister, TR::RealRegister::edx, cg);
   xorDependencies1->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
   xorDependencies1->addPostCondition(highRegister, TR::RealRegister::edx, cg);
   generateRegRegInstruction(XOR4RegReg, node, highRegister, highRegister, xorDependencies1, cg);

   generateLabelInstruction(JMP4, node, doneLabel, cg);

   generateLabelInstruction(LABEL, node, callLabel, cg);

   TR::RegisterDependencyConditions  *dependencies = generateRegisterDependencyConditions((uint8_t)0, 2, cg);
   dependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
   dependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
   J9::IA32PrivateLinkage *linkage = static_cast<J9::IA32PrivateLinkage *>(cg->getLinkage(TR_Private));
   TR::IA32LinkageUtils::pushLongArg(secondChild, cg);
   TR::IA32LinkageUtils::pushLongArg(firstChild, cg);
   TR::X86ImmSymInstruction  *instr =
      generateHelperCallInstruction(node, TR_IA32longDivide, dependencies, cg);
   if (!linkage->getProperties().getCallerCleanup())
      {
      instr->setAdjustsFramePointerBy(-16);  // 2 long args
      }

   // Don't preserve eax and edx
   //
   instr->setNeedsGCMap(0xFF00FFF6);

   TR::RegisterDependencyConditions  *labelDependencies = generateRegisterDependencyConditions((uint8_t)6, 6, cg);
   labelDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
   labelDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
   labelDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
   labelDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
   labelDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
   labelDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
   labelDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
   labelDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
   labelDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   labelDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   labelDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   labelDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

   generateLabelInstruction(LABEL, node, doneLabel, labelDependencies, cg);

   TR::Register *targetRegister = cg->allocateRegisterPair(lowRegister, highRegister);
   node->setRegister(targetRegister);

   return targetRegister;

#else
   TR_ASSERT(0, "Unsupported front end");
   return NULL;
#endif

   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairRemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if J9_PROJECT_SPECIFIC
   // TODO: Consider combining with integerPairDivEvaluator
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *lowRegister  = cg->allocateRegister();
   TR::Register *highRegister = cg->allocateRegister();

   TR::Register *firstRegister = cg->evaluate(firstChild);
   TR::Register *secondRegister = cg->evaluate(secondChild);

   TR::Register *firstHigh = firstRegister->getHighOrder();
   TR::Register *secondHigh = secondRegister->getHighOrder();

   TR::Instruction  *divInstr = NULL;

   TR::RegisterDependencyConditions  *idivDependencies = generateRegisterDependencyConditions((uint8_t)6, 6, cg);
   idivDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
   idivDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
   idivDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
   idivDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
   idivDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
   idivDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   idivDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   idivDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

   TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *callLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, node, startLabel, cg);

   generateRegRegInstruction(MOV4RegReg, node, highRegister, secondHigh, cg);
   generateRegRegInstruction(OR4RegReg, node, highRegister, firstHigh, cg);
   // it doesn't need the test instruction, OR will set the flags properly
   //generateRegRegInstruction(TEST4RegReg, node, highRegister, highRegister, cg);
   generateLabelInstruction(JNE4, node, callLabel, cg);

   generateRegRegInstruction(MOV4RegReg, node, lowRegister, firstRegister->getLowOrder(), cg);
   divInstr = generateRegRegInstruction(DIV4AccReg, node, lowRegister, secondRegister->getLowOrder(), idivDependencies, cg);

   cg->setImplicitExceptionPoint(divInstr);
   divInstr->setNeedsGCMap(0xFF00FFF6);

   generateRegRegInstruction(MOV4RegReg, node, lowRegister, highRegister, cg);

   generateRegRegInstruction(XOR4RegReg, node, highRegister, highRegister, cg);
   generateLabelInstruction(JMP4, node, doneLabel, cg);

   generateLabelInstruction(LABEL, node, callLabel, cg);

   TR::RegisterDependencyConditions  *dependencies = generateRegisterDependencyConditions((uint8_t)4, 6, cg);

   dependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
   dependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
   dependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
   dependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
   dependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   dependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

   J9::IA32PrivateLinkage *linkage = static_cast<J9::IA32PrivateLinkage *>(cg->getLinkage(TR_Private));
   TR::IA32LinkageUtils::pushLongArg(secondChild, cg);
   TR::IA32LinkageUtils::pushLongArg(firstChild, cg);
   TR::X86ImmSymInstruction  *instr =
      generateHelperCallInstruction(node, TR_IA32longRemainder, dependencies, cg);
   if (!linkage->getProperties().getCallerCleanup())
      {
      instr->setAdjustsFramePointerBy(-16);  // 2 long args
      }

   // Don't preserve eax and edx
   //
   instr->setNeedsGCMap(0xFF00FFF6);

   TR::RegisterDependencyConditions  *movDependencies = generateRegisterDependencyConditions((uint8_t)6, 6, cg);
   movDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
   movDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
   movDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
   movDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
   movDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
   movDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
   movDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
   movDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
   movDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   movDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   movDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   movDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

   generateLabelInstruction(LABEL, node, doneLabel, movDependencies, cg);

   TR::Register *targetRegister = cg->allocateRegisterPair(lowRegister, highRegister);
   node->setRegister(targetRegister);

   return targetRegister;
#else
   TR_ASSERT(0, "Unsupported front end");
   return NULL;
#endif
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairNegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *targetRegister = cg->longClobberEvaluate(firstChild);
   node->setRegister(targetRegister);
   generateRegInstruction(NEG4Reg, node, targetRegister->getLowOrder(), cg);
   generateRegImmInstruction(ADC4RegImms, node, targetRegister->getHighOrder(), 0, cg);
   generateRegInstruction(NEG4Reg, node, targetRegister->getHighOrder(), cg);

   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairAbsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *targetRegister = cg->longClobberEvaluate(child);
   node->setRegister(targetRegister);
   TR::Register *signRegister = cg->allocateRegister(TR_GPR);
   generateRegRegInstruction(MOV4RegReg, node, signRegister, targetRegister->getHighOrder(), cg);
   generateRegImmInstruction(SAR4RegImm1, node, signRegister, 31, cg);
   generateRegRegInstruction(ADD4RegReg, node, targetRegister->getLowOrder(), signRegister, cg);
   generateRegRegInstruction(ADC4RegReg, node, targetRegister->getHighOrder(), signRegister, cg);
   generateRegRegInstruction(XOR4RegReg, node, targetRegister->getLowOrder(), signRegister, cg);
   generateRegRegInstruction(XOR4RegReg, node, targetRegister->getHighOrder(), signRegister, cg);
   cg->stopUsingRegister(signRegister);
   cg->decReferenceCount(child);
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairShlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister;
   TR::Node     *secondChild = node->getSecondChild();
   TR::Node     *firstChild  = node->getFirstChild();
   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      int32_t value = secondChild->getInt() & shiftMask(true);
      if (value == 0)
         {
         targetRegister = cg->longClobberEvaluate(firstChild);
         }
      else if (value <= 3 && firstChild->getReferenceCount() > 1)
         {
         TR::Register *tempRegister = cg->evaluate(firstChild);
         targetRegister = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
         generateRegRegInstruction(MOV4RegReg, node, targetRegister->getHighOrder(), tempRegister->getHighOrder(), cg);
         TR::MemoryReference  *tempMR = generateX86MemoryReference(cg);
         tempMR->setIndexRegister(tempRegister->getLowOrder());
         tempMR->setStride(value);
         generateRegMemInstruction(LEA4RegMem, node, targetRegister->getLowOrder(), tempMR, cg);
         generateRegRegImmInstruction(SHLD4RegRegImm1, node,
                                      targetRegister->getHighOrder(),
                                      tempRegister->getLowOrder(), value, cg);
         }
      else
         {
         targetRegister = cg->longClobberEvaluate(firstChild);
         if (value < 32)
            {
            generateRegRegImmInstruction(SHLD4RegRegImm1, node,
                                         targetRegister->getHighOrder(),
                                         targetRegister->getLowOrder(),
                                         value, cg);
            generateRegImmInstruction(SHL4RegImm1, node, targetRegister->getLowOrder(), value, cg);
            }
         else
            {
            if (value != 32)
               {
               value -= 32;
               generateRegImmInstruction(SHL4RegImm1, node, targetRegister->getLowOrder(), value, cg);
               }
            TR::Register *tempHighReg = targetRegister->getHighOrder();
            TR::RegisterPair *targetAsPair = targetRegister->getRegisterPair();
            targetAsPair->setHighOrder(targetRegister->getLowOrder(), cg);
            generateRegRegInstruction(XOR4RegReg, node, tempHighReg, tempHighReg, cg);
            targetAsPair->setLowOrder(tempHighReg, cg);
            }
         }
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      targetRegister = cg->longClobberEvaluate(firstChild);
      TR::Register* shiftAmountReg = cg->evaluate(secondChild);
      if (shiftAmountReg->getLowOrder())
         {
         shiftAmountReg = shiftAmountReg->getLowOrder();
         }

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      generateRegRegInstruction(SHLD4RegRegCL, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), shiftDependencies, cg);
      generateRegInstruction(SHL4RegCL, node, targetRegister->getLowOrder(), shiftDependencies, cg);
      generateRegImmInstruction(TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(CMOVNE4RegReg, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), cg);
      generateRegMemInstruction(CMOVNE4RegMem, node, targetRegister->getLowOrder(),
                                generateX86MemoryReference(cg->findOrCreate4ByteConstant(node, 0), cg), cg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairRolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister;
   TR::Node     *firstChild  = node->getFirstChild();
   TR::Node     *secondChild = node->getSecondChild();

   targetRegister = cg->longClobberEvaluate(firstChild);

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = secondChild->getInt() & rotateMask(true);
      if (value != 0)
         {
         if (value >= 32)
            {
            value -= 32;

            // Swap Register Pairs
            TR::Register *tempLowReg = targetRegister->getLowOrder();
            TR::RegisterPair *targetAsPair = targetRegister->getRegisterPair();
            targetAsPair->setLowOrder(targetRegister->getHighOrder(), cg);
            targetAsPair->setHighOrder(tempLowReg, cg);
            }

         // A rotate of 32 requires only the above swap
         if (value != 0)
            {
            TR::Register *tempHighReg = cg->allocateRegister();

            // Save off the original high register.
            generateRegRegInstruction(MOV4RegReg, node, tempHighReg, targetRegister->getHighOrder(), cg);

            // E.g.
            // HH HH HH HH LL LL LL LL, before
            // HH HH LL LL LL LL LL LL, after
            generateRegRegImmInstruction(SHLD4RegRegImm1, node,
               targetRegister->getHighOrder(),
               targetRegister->getLowOrder(),
               value, cg);

            // HH HH LL LL LL LL LL LL, before
            // HH HH LL LL LL LL HH HH, after
            generateRegRegImmInstruction(SHLD4RegRegImm1, node,
            targetRegister->getLowOrder(),
               tempHighReg,
               value, cg);

            cg->stopUsingRegister(tempHighReg);
            }
         }

         node->setRegister(targetRegister);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
      }
   else
      {
      targetRegister = cg->longClobberEvaluate(firstChild);
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      if (shiftAmountReg->getLowOrder())
         {
         shiftAmountReg = shiftAmountReg->getLowOrder();
         }

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      TR::Register *scratchReg = cg->allocateRegister();
      generateRegRegInstruction(MOV4RegReg, node, scratchReg, targetRegister->getHighOrder(), cg);
      generateRegImmInstruction(TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(CMOVNE4RegReg, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), cg);
      generateRegRegInstruction(CMOVNE4RegReg, node, targetRegister->getLowOrder(), scratchReg, cg);

      generateRegRegInstruction(MOV4RegReg, node, scratchReg, targetRegister->getHighOrder(), cg);
      generateRegRegInstruction(SHLD4RegRegCL, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), shiftDependencies, cg);
      generateRegRegInstruction(SHLD4RegRegCL, node, targetRegister->getLowOrder(), scratchReg, shiftDependencies, cg);

      cg->stopUsingRegister(scratchReg);
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairShrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *targetRegister;
   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      targetRegister = cg->longClobberEvaluate(firstChild);
      int32_t value = secondChild->getInt() & shiftMask(true);
      if (value == 0)
         ;
      else if (value < 32)
         {
         generateRegRegImmInstruction(SHRD4RegRegImm1, node,
                                      targetRegister->getLowOrder(),
                                      targetRegister->getHighOrder(),
                                      value, cg);
         generateRegImmInstruction(SAR4RegImm1, node, targetRegister->getHighOrder(), value, cg);
         }
      else
         {
         if (value != 32)
            {
            value -= 32;
            generateRegImmInstruction(SAR4RegImm1, node, targetRegister->getHighOrder(), value, cg);
            }
         generateRegRegInstruction(MOV4RegReg, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), cg);
         generateRegImmInstruction(SAR4RegImm1, node, targetRegister->getHighOrder(), 31, cg);
         }
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      targetRegister = cg->longClobberEvaluate(firstChild);
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      if (shiftAmountReg->getLowOrder())
         {
         shiftAmountReg = shiftAmountReg->getLowOrder();
         }

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      TR::Register *scratchReg = cg->allocateRegister();
      generateRegRegInstruction(SHRD4RegRegCL, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegInstruction(SAR4RegCL, node, targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegRegInstruction(MOV4RegReg, node, scratchReg, targetRegister->getHighOrder(), cg);
      generateRegImmInstruction(SAR4RegImm1, node, scratchReg, 31, cg);
      generateRegImmInstruction(TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(CMOVNE4RegReg, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), cg);
      generateRegRegInstruction(CMOVNE4RegReg, node, targetRegister->getHighOrder(), scratchReg, cg);

      cg->stopUsingRegister(scratchReg);
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairUshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *targetRegister;
   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      targetRegister = cg->longClobberEvaluate(firstChild);
      int32_t value = secondChild->getInt() & shiftMask(true);
      if (value < 32)
         {
         generateRegRegImmInstruction(SHRD4RegRegImm1, node,
                                      targetRegister->getLowOrder(),
                                      targetRegister->getHighOrder(),
                                      value, cg);
         generateRegImmInstruction(SHR4RegImm1, node, targetRegister->getHighOrder(), value, cg);
         }
      else
         {
         if (value != 32)
            {
            value -= 32;
            generateRegImmInstruction(SHR4RegImm1, node,
                                         targetRegister->getHighOrder(),
                                         value, cg);
            }
         TR::Register *tempLowReg = targetRegister->getLowOrder();
         TR::RegisterPair *targetAsPair = targetRegister->getRegisterPair();
         targetAsPair->setLowOrder(targetRegister->getHighOrder(), cg);
         generateRegRegInstruction(XOR4RegReg, node, tempLowReg, tempLowReg, cg);
         targetAsPair->setHighOrder(tempLowReg, cg);
         }
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      targetRegister = cg->longClobberEvaluate(firstChild);
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      if (shiftAmountReg->getLowOrder())
         {
         shiftAmountReg = shiftAmountReg->getLowOrder();
         }

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      generateRegRegInstruction(SHRD4RegRegCL, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegInstruction(SHR4RegCL, node, targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegImmInstruction(TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(CMOVNE4RegReg, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), cg);
      generateRegMemInstruction(CMOVNE4RegMem, node, targetRegister->getHighOrder(),
                                generateX86MemoryReference(cg->findOrCreate4ByteConstant(node, 0), cg), cg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register            *targetRegister = NULL;
   TR::Node                *firstChild     = node->getFirstChild();
   TR::Node                *secondChild    = node->getSecondChild();
   TR::Instruction         *lowInstr       = NULL,
                          *highInstr      = NULL;
   TR::MemoryReference  *lowMR,
                          *highMR;

   // See if we can generate a direct memory operation. In this case, no
   // target register is generated, and we return NULL to the caller (which
   // should be a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();
   bool firstChildAlreadyEvaluated = false;

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         lowMR = generateX86MemoryReference(*reg->getMemRef(), 4, cg);
         firstChildAlreadyEvaluated = true;
         }
      else
         {
         lowMR = generateX86MemoryReference(firstChild, cg, false);
         }
      highMR = generateX86MemoryReference(*lowMR, 4, cg);
      }

   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t         lowValue  = secondChild->getLongIntLow();
      int32_t         highValue = secondChild->getLongIntHigh();
      TR::Register    *tempReg   = NULL;
      TR::Register    *lowReg;
      TR::Register    *highReg;
      TR_X86OpCodes  opCode;

      if (!isMemOp)
         {
         TR::Register *valueReg = cg->evaluate(firstChild);

         if (firstChild->getReferenceCount() == 1)
            {
            targetRegister = valueReg;
            lowReg = targetRegister->getLowOrder();
            highReg = targetRegister->getHighOrder();
            }
         else
            {
            lowReg  = cg->allocateRegister();
            highReg = cg->allocateRegister();
            targetRegister = cg->allocateRegisterPair(lowReg, highReg);
            if (lowValue != 0)
               generateRegRegInstruction(MOV4RegReg, node, lowReg, valueReg->getLowOrder(), cg);
            if (highValue != 0)
               generateRegRegInstruction(MOV4RegReg, node, highReg, valueReg->getHighOrder(), cg);
            }
         }

      // Do nothing if AND-ing with 0xFFFFFFFFFFFFFFFF.
      // AND-ing with zero is equivalent to clearing the destination.
      //
      if (lowValue == -1)
         ;
      else if (lowValue == 0)
         {
         if (isMemOp)
            {
            tempReg = cg->allocateRegister();
            generateRegRegInstruction(XOR4RegReg, node, tempReg, tempReg, cg);
            lowInstr = generateMemRegInstruction(S4MemReg, node, lowMR, tempReg, cg);
            }
         else
            lowInstr = generateRegRegInstruction(XOR4RegReg, node, lowReg, lowReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? AND4MemImms : AND4MemImm4;
            lowInstr = generateMemImmInstruction(opCode, node, lowMR, lowValue, cg);
            }
         else
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? AND4RegImms : AND4RegImm4;
            lowInstr = generateRegImmInstruction(opCode, node, lowReg, lowValue, cg);
            }
         }

      if (highValue == -1)
         ;
      else if (highValue == 0)
         {
         if (isMemOp)
            {
            // Re-use the temporary register if possible.
            if (tempReg == NULL)
               {
               tempReg = cg->allocateRegister();
               generateRegRegInstruction(XOR4RegReg, node, tempReg, tempReg, cg);
               }
            highInstr = generateMemRegInstruction(S4MemReg, node, highMR, tempReg, cg);
            }
         else
            highInstr = generateRegRegInstruction(XOR4RegReg, node, highReg, highReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (highValue >= -128 && highValue <= 127) ? AND4MemImms : AND4MemImm4;
            highInstr = generateMemImmInstruction(opCode, node, highMR, highValue, cg);
            }
         else
            {
            opCode = (highValue >= -128 && highValue <= 127) ? AND4RegImms : AND4RegImm4;
            highInstr = generateRegImmInstruction(opCode, node, highReg, highValue, cg);
            }
         }

      if (isMemOp)
         {
         if (tempReg != NULL)
            cg->stopUsingRegister(tempReg);

         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] and by const", node);
         }
      }
   else if (isMemOp)
      {
      TR::Register *tempReg = cg->evaluate(secondChild);

      lowInstr = generateMemRegInstruction(AND4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      highInstr = generateMemRegInstruction(AND4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] and by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);

      temp.genericLongAnalyser(node,
                               AND4RegReg,
                               AND4RegReg,
                               AND4RegMem,
                               AND2RegMem,
                               AND1RegMem,
                               AND4RegMem,
                               MOV4RegReg);

      return node->getRegister();
      }

   if (isMemOp)
      {
      if (!firstChildAlreadyEvaluated)
         lowMR->decNodeReferenceCounts(cg);
      else
         lowMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(lowInstr ? lowInstr : highInstr);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register            *targetRegister = NULL;
   TR::Register            *temp           = NULL;
   TR::Node                *firstChild     = node->getFirstChild();
   TR::Node                *secondChild    = node->getSecondChild();
   TR::Instruction         *lowInstr       = NULL,
                          *highInstr      = NULL;
   TR::MemoryReference  *lowMR,
                          *highMR;

   // See if we can generate a direct memory operation. In this case, no
   // target register is generated, and we return NULL to the caller (which
   // should be a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();
   bool firstChildAlreadyEvaluated = false;

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         lowMR = generateX86MemoryReference(*reg->getMemRef(), 4, cg);
         firstChildAlreadyEvaluated = true;
         }
      else
         {
         lowMR = generateX86MemoryReference(firstChild, cg, false);
         }
      highMR = generateX86MemoryReference(*lowMR, 4, cg);
      }

   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t         lowValue  = secondChild->getLongIntLow();
      int32_t         highValue = secondChild->getLongIntHigh();
      TR::Register    *lowReg;
      TR::Register    *highReg;
      TR_X86OpCodes  opCode;
      TR::Register *ccReg = 0;

      if (!isMemOp)
         {
         TR::Register *valueReg = cg->evaluate(firstChild);

         if (firstChild->getReferenceCount() == 1)
            {
            targetRegister = valueReg;
            lowReg = targetRegister->getLowOrder();
            highReg = targetRegister->getHighOrder();
            }
         else
            {
            lowReg  = cg->allocateRegister();
            highReg = cg->allocateRegister();
            targetRegister = cg->allocateRegisterPair(lowReg, highReg);
            if (lowValue != -1)
               generateRegRegInstruction(MOV4RegReg, node, lowReg, valueReg->getLowOrder(), cg);
            if (highValue != -1)
               generateRegRegInstruction(MOV4RegReg, node, highReg, valueReg->getHighOrder(), cg);
            }
         }

      // Do nothing if OR-ing with zero.
      //
      if (lowValue == 0)
         ;
      else
         {
         if (isMemOp)
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? OR4MemImms : OR4MemImm4;
            lowInstr = generateMemImmInstruction(opCode, node, lowMR, lowValue, cg);
            }
         else
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? OR4RegImms : OR4RegImm4;
            lowInstr = generateRegImmInstruction(opCode, node, lowReg, lowValue, cg);
            }
         }

      if (highValue == 0)
         ;
      else
         {
         if (isMemOp)
            {
            opCode = (highValue >= -128 && highValue <= 127) ? OR4MemImms : OR4MemImm4;
            highInstr = generateMemImmInstruction(opCode, node, highMR, highValue, cg);
            }
         else
            {
            opCode = (highValue >= -128 && highValue <= 127) ? OR4RegImms : OR4RegImm4;
            highInstr = generateRegImmInstruction(opCode, node, highReg, highValue, cg);
            }
         }

      if (debug("traceMemOp") && isMemOp)
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] or by const", node);
      }
   else if (isMemOp)
      {
      TR::Register *tempReg = cg->evaluate(secondChild);

      lowInstr = generateMemRegInstruction(OR4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      highInstr = generateMemRegInstruction(OR4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] or by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);

      temp.genericLongAnalyser(node,
                               OR4RegReg,
                               OR4RegReg,
                               OR4RegMem,
                               OR2RegMem,
                               OR1RegMem,
                               OR4RegMem,
                               MOV4RegReg);

      return node->getRegister();
      }

   if (isMemOp)
      {
      if (!firstChildAlreadyEvaluated)
         lowMR->decNodeReferenceCounts(cg);
      else
         lowMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(lowInstr ? lowInstr : highInstr);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register            *targetRegister = NULL;
   TR::Node                *firstChild     = node->getFirstChild();
   TR::Node                *secondChild    = node->getSecondChild();
   TR::Instruction         *lowInstr       = NULL,
                          *highInstr      = NULL;
   TR::MemoryReference  *lowMR,
                          *highMR;

   // See if we can generate a direct memory operation. In this case, no
   // target register is generated, and we return NULL to the caller (which
   // should be a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();
   bool firstChildAlreadyEvaluated = false;

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         lowMR = generateX86MemoryReference(*reg->getMemRef(), 4, cg);
         firstChildAlreadyEvaluated = true;
         }
      else
         {
         lowMR = generateX86MemoryReference(firstChild, cg, false);
         }
      highMR = generateX86MemoryReference(*lowMR, 4, cg);
      }

   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t         lowValue  = secondChild->getLongIntLow();
      int32_t         highValue = secondChild->getLongIntHigh();
      TR::Register    *lowReg;
      TR::Register    *highReg;
      TR_X86OpCodes  opCode;
      TR::Register *ccReg = 0;

      if (!isMemOp)
         {
         targetRegister = cg->longClobberEvaluate(firstChild);
         lowReg  = targetRegister->getLowOrder();
         highReg = targetRegister->getHighOrder();
         }

      // Do nothing if XOR-ing with zero.
      // XOR with 0xFFFFFFFFFFFFFFFF is equivalent to a NOT.
      //
      if (lowValue == 0)
         ;
      else if (lowValue == -1)
         {
         if (isMemOp)
            lowInstr = generateMemInstruction(NOT4Mem, node, lowMR, cg);
         else
            lowInstr = generateRegInstruction(NOT4Reg, node, lowReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? XOR4MemImms : XOR4MemImm4;
            lowInstr = generateMemImmInstruction(opCode, node, lowMR, lowValue, cg);
            }
         else
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? XOR4RegImms : XOR4RegImm4;
            lowInstr = generateRegImmInstruction(opCode, node, lowReg, lowValue, cg);
            }
         }

      if (highValue == 0)
         ;
      else if (highValue == -1)
         {
         if (isMemOp)
            highInstr = generateMemInstruction(NOT4Mem, node, highMR, cg);
         else
            highInstr = generateRegInstruction(NOT4Reg, node, highReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (highValue >= -128 && highValue <= 127) ? XOR4MemImms : XOR4MemImm4;
            highInstr = generateMemImmInstruction(opCode, node, highMR, highValue, cg);
            }
         else
            {
            opCode = (highValue >= -128 && highValue <= 127) ? XOR4RegImms : XOR4RegImm4;
            highInstr = generateRegImmInstruction(opCode, node, highReg, highValue, cg);
            }
         }

      if (debug("traceMemOp") && isMemOp)
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] xor by const", node);
      }
   else if (isMemOp)
      {
      TR::Register *tempReg = cg->evaluate(secondChild);

      lowInstr = generateMemRegInstruction(XOR4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      highInstr = generateMemRegInstruction(XOR4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] xor by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);

      temp.genericLongAnalyser(node,
                               XOR4RegReg,
                               XOR4RegReg,
                               XOR4RegMem,
                               XOR2RegMem,
                               XOR1RegMem,
                               XOR4RegMem,
                               MOV4RegReg);

      return node->getRegister();
      }

   if (isMemOp)
      {
      if (!firstChildAlreadyEvaluated)
         lowMR->decNodeReferenceCounts(cg);
      else
         lowMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(lowInstr ? lowInstr : highInstr);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *targetRegister;

   if (child->getOpCode().isLoadVar() &&
       child->getRegister() == NULL   &&
       child->getReferenceCount() == 1)
      {
      targetRegister = cg->allocateRegister();
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      generateRegMemInstruction(L4RegMem, node, targetRegister, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *temp = cg->evaluate(child);
      if (child->getReferenceCount() == 1)
         {
         cg->stopUsingRegister(temp->getHighOrder());
         targetRegister = temp->getLowOrder();
         }
      else
         {
         targetRegister = cg->allocateRegister();
         generateRegRegInstruction(MOV4RegReg, node, targetRegister, temp->getLowOrder(), cg);
         }

      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(child);

   if (cg->enableRegisterInterferences() && node->getOpCode().getSize() == 1)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(node->getRegister());

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node::recreate(node, TR::ificmpeq);
   integerIfCmpeqEvaluator(node, cg);
   TR::Node::recreate(node, TR::ifacmpeq);
   return NULL;
   }

// ifacmpne handled by ificmpeqEvaluator

TR::Register *OMR::X86::I386::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node::recreate(node, TR::icmpeq);
   TR::Register *targetRegister = integerCmpeqEvaluator(node, cg);
   TR::Node::recreate(node, TR::acmpeq);
   return targetRegister;
   }

// acmpneEvaluator handled by icmpeqEvaluator

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *secondChild = node->getSecondChild();
   TR::Register *targetRegister;
   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t      lowValue    = secondChild->getLongIntLow();
      int32_t      highValue   = secondChild->getLongIntHigh();
      TR::Node     *firstChild  = node->getFirstChild();
      TR::Register *cmpRegister = cg->evaluate(firstChild);
      if ((lowValue | highValue) == 0)
         {
         targetRegister = cmpRegister->getLowOrder();
         if (firstChild->getReferenceCount() != 1)
            {
            targetRegister = cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, node,
                                         targetRegister,
                                         cmpRegister->getLowOrder(), cg);
            }
         generateRegRegInstruction(OR4RegReg, node,
                                      targetRegister,
                                      cmpRegister->getHighOrder(), cg);
         cg->stopUsingRegister(targetRegister);
         targetRegister = cg->allocateRegister();

         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

         generateRegInstruction(SETE1Reg, node, targetRegister, cg);
         }
      else
         {
         compareGPRegisterToConstantForEquality(node, lowValue, cmpRegister->getLowOrder(), cg);
         targetRegister = cg->allocateRegister();
         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

         generateRegInstruction(SETE1Reg, node, targetRegister, cg);

         compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
         TR::Register *highTargetRegister = cg->allocateRegister();
         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(highTargetRegister);

         generateRegInstruction(SETE1Reg, node, highTargetRegister, cg);

         generateRegRegInstruction(AND1RegReg, node, targetRegister, highTargetRegister, cg);
         cg->stopUsingRegister(highTargetRegister);
         }

      // Result of lcmpXX is an integer.
      //
      generateRegRegInstruction(MOVSXReg4Reg1,node,targetRegister,targetRegister,cg);
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      // need analyser-like stuff here for non-constant second children.
      TR_X86CompareAnalyser  temp(cg);
      targetRegister = temp.longEqualityBooleanAnalyser(node, SETE1Reg, AND1RegReg);
      }
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *secondChild = node->getSecondChild();
   TR::Register *targetRegister;
   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t      lowValue    = secondChild->getLongIntLow();
      int32_t      highValue   = secondChild->getLongIntHigh();
      TR::Node     *firstChild  = node->getFirstChild();
      TR::Register *cmpRegister = cg->evaluate(firstChild);
      if ((lowValue | highValue) == 0)
         {
         targetRegister = cmpRegister->getLowOrder();
         if (firstChild->getReferenceCount() != 1)
            {
            targetRegister = cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, node, targetRegister, cmpRegister->getLowOrder(), cg);
            }
         generateRegRegInstruction(OR4RegReg, node, targetRegister, cmpRegister->getHighOrder(), cg);
         cg->stopUsingRegister(targetRegister);
         targetRegister = cg->allocateRegister();

         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

         generateRegInstruction(SETNE1Reg, node, targetRegister, cg);
         }
      else
         {
         compareGPRegisterToConstantForEquality(node, lowValue, cmpRegister->getLowOrder(), cg);
         targetRegister = cg->allocateRegister();

         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(SETNE1Reg, node, targetRegister, cg);

         compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
         TR::Register *highTargetRegister = cg->allocateRegister();
         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(highTargetRegister);

         generateRegInstruction(SETNE1Reg, node, highTargetRegister, cg);
         generateRegRegInstruction(OR1RegReg, node, targetRegister, highTargetRegister, cg);
         cg->stopUsingRegister(highTargetRegister);
         }

      // Result of lcmpXX is an integer.
      //
      generateRegRegInstruction(MOVSXReg4Reg1,node,targetRegister,targetRegister,cg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      targetRegister = temp.longEqualityBooleanAnalyser(node, SETNE1Reg, OR1RegReg);
      }
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETL1Reg, SETB1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETG1Reg, SETAE1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETG1Reg, SETA1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETL1Reg, SETBE1Reg, cg);
   }




TR::Register *OMR::X86::I386::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETB1Reg, SETB1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETA1Reg, SETAE1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETA1Reg, SETA1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, SETB1Reg, SETBE1Reg, cg);
   }




// also handles lucmp
TR::Register *OMR::X86::I386::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *targetRegister;

   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      targetRegister = longArithmeticCompareRegisterWithImmediate(node, cg->evaluate(firstChild),
                                                                  secondChild,
                                                                  JAE4,
                                                                  JGE4, cg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else if (firstChild->getOpCodeValue() == TR::lconst &&
            firstChild->getRegister() == NULL)
      {
      targetRegister = longArithmeticCompareRegisterWithImmediate(node, cg->evaluate(secondChild),
                                                                  firstChild,
                                                                  JBE4,
                                                                  JLE4, cg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      targetRegister = temp.longCMPAnalyser(node);
      }

   node->setRegister(targetRegister);
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();
   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegisterPair(cg->allocateRegister(),
                                                cg->allocateRegister());
      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node         *child    = node->getFirstChild();
   TR::Register     *childReg = cg->intClobberEvaluate(child);
   TR::Register     *highReg  = cg->allocateRegister();
   TR::RegisterPair *longReg  = cg->allocateRegisterPair(childReg, highReg);
   TR::Machine *machine  = cg->machine();

   if (machine->getVirtualAssociatedWithReal(TR::RealRegister::eax) == childReg)
      {
      TR::RegisterDependencyConditions  *cdqDependencies = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
      cdqDependencies->addPreCondition(childReg, TR::RealRegister::eax, cg);
      cdqDependencies->addPreCondition(highReg, TR::RealRegister::edx, cg);
      cdqDependencies->addPostCondition(childReg, TR::RealRegister::eax, cg);
      cdqDependencies->addPostCondition(highReg, TR::RealRegister::edx, cg);
      generateInstruction(CDQAcc, node, cdqDependencies, cg);
      }
   else
      {
      generateRegRegInstruction(MOV4RegReg, node, highReg, childReg, cg);
      generateRegImmInstruction(SAR4RegImm1, node, highReg, 31, cg);
      }

   node->setRegister(longReg);
   cg->decReferenceCount(child);
   return longReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node         *child    = node->getFirstChild();
   TR::Register     *childReg = cg->intClobberEvaluate(child);
   TR::Register     *highReg  = cg->allocateRegister();
   TR::RegisterPair *longReg  = cg->allocateRegisterPair(childReg, highReg);

   generateRegRegInstruction(XOR4RegReg, node, highReg, highReg, cg);

   node->setRegister(longReg);
   cg->decReferenceCount(child);
   return longReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child   = node->getFirstChild();
   TR::Register *longReg;

   if (child->getOpCode().isLoadVar() &&
       child->getRegister() == NULL   &&
       child->getReferenceCount() == 1)
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      longReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      generateRegMemInstruction(MOVSXReg4Mem1, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      generateRegRegInstruction(MOVSXReg4Reg1, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(MOV4RegReg, node, longReg->getHighOrder(), longReg->getLowOrder(), cg);
   generateRegImmInstruction(SAR4RegImm1, node, longReg->getHighOrder(), 8, cg);
   node->setRegister(longReg);
   cg->decReferenceCount(child);

   return longReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child   = node->getFirstChild();
   TR::Register *longReg;

   if (child->getOpCode().isLoadVar() &&
       child->getRegister() == NULL   &&
       child->getReferenceCount() == 1)
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      longReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      generateRegMemInstruction(MOVZXReg4Mem1, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      generateRegRegInstruction(MOVZXReg4Reg1, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(XOR4RegReg, node, longReg->getHighOrder(), longReg->getHighOrder(), cg);
   node->setRegister(longReg);
   cg->decReferenceCount(child);

   return longReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child   = node->getFirstChild();
   TR::Register *longReg;

   if (child->getOpCode().isLoadVar() &&
       child->getRegister() == NULL   &&
       child->getReferenceCount() == 1)
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      longReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegMemInstruction(MOVSXReg4Mem2, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegRegInstruction(MOVSXReg4Reg2, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(MOV4RegReg, node,
                                longReg->getHighOrder(),
                                longReg->getLowOrder(), cg);
   generateRegImmInstruction(SAR4RegImm1, node, longReg->getHighOrder(), 16, cg);
   cg->decReferenceCount(child);

   return longReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child   = node->getFirstChild();
   TR::Register *longReg;

   if (child->getOpCode().isLoadVar() &&
       child->getRegister() == NULL   &&
       child->getReferenceCount() == 1)
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      longReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegMemInstruction(MOVZXReg4Mem2, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegRegInstruction(MOVZXReg4Reg2, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(XOR4RegReg, node, longReg->getHighOrder(),longReg->getHighOrder(), cg);
   cg->decReferenceCount(child);

   return longReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child   = node->getFirstChild();
   TR::Register *longReg;

   if (child->getOpCode().isLoadVar() &&
       child->getRegister() == NULL   &&
       child->getReferenceCount() == 1)
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      longReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegMemInstruction(MOVZXReg4Mem2, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegRegInstruction(MOVZXReg4Reg2, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);

      }

   generateRegRegInstruction(XOR4RegReg, node, longReg->getHighOrder(), longReg->getHighOrder(), cg);
   cg->decReferenceCount(child);

   return longReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool     nodeIsIndirect = node->getOpCode().isIndirect()? 1 : 0;
   TR::Node *valueChild     = node->getChild(nodeIsIndirect);

   if ((valueChild->getOpCodeValue() == TR::lbits2d) && !valueChild->getRegister())
      {
      // Special case storing a long value into a double variable
      //
      TR::Node *longValueChild = valueChild->getFirstChild();
      static TR::ILOpCodes longOpCodes[2] = { TR::lstore, TR::lstorei };
      TR::Node::recreate(node, longOpCodes[nodeIsIndirect]);
      node->setChild(nodeIsIndirect, longValueChild);
      longValueChild->incReferenceCount();

      // valueChild is no longer used here
      //
      cg->recursivelyDecReferenceCount(valueChild);

      lstoreEvaluator(node, cg); // The IA32 version, handles ilstore as well
      return NULL;
      }
   else
      {
      TR::MemoryReference  *storeLowMR = generateX86MemoryReference(node, cg);
      TR::Instruction *instr;

      if (valueChild->getOpCode().isLoadConst())
         {
         instr = generateMemImmInstruction(S4MemImm4, node, generateX86MemoryReference(*storeLowMR, 4, cg), valueChild->getLongIntHigh(), cg);
         generateMemImmInstruction(S4MemImm4, node, storeLowMR, valueChild->getLongIntLow(), cg);
         TR::Register *valueChildReg = valueChild->getRegister();
         if (valueChildReg && valueChildReg->getKind() == TR_X87 && valueChild->getReferenceCount() == 1)
           instr = generateFPSTiST0RegRegInstruction(DSTRegReg, valueChild, valueChildReg, valueChildReg, cg);
         }
      else if (debug("useGPRsForFP") &&
               (cg->getLiveRegisters(TR_GPR)->getNumberOfLiveRegisters() <
               cg->getMaximumNumbersOfAssignableGPRs() - 1) &&
               valueChild->getOpCode().isLoadVar() &&
               valueChild->getRegister() == NULL   &&
               valueChild->getReferenceCount() == 1)
         {
         TR::Register *tempRegister = cg->allocateRegister(TR_GPR);
         TR::MemoryReference  *loadLowMR = generateX86MemoryReference(valueChild, cg);
         generateRegMemInstruction(L4RegMem, node, tempRegister, generateX86MemoryReference(*loadLowMR, 4, cg), cg);
         instr = generateMemRegInstruction(S4MemReg, node, generateX86MemoryReference(*storeLowMR, 4, cg), tempRegister, cg);
         generateRegMemInstruction(L4RegMem, node, tempRegister, loadLowMR, cg);
         generateMemRegInstruction(S4MemReg, node, storeLowMR, tempRegister, cg);
         loadLowMR->decNodeReferenceCounts(cg);
         cg->stopUsingRegister(tempRegister);
         }
      else
         {
         TR::Register *sourceRegister = cg->evaluate(valueChild);
         if (sourceRegister->getKind() == TR_FPR)
            {
            TR_ASSERT(!sourceRegister->isSinglePrecision(), "dstore cannot have float operand\n");
            instr = generateMemRegInstruction(MOVSDMemReg, node, storeLowMR, sourceRegister, cg);
            }
         else
            {
            instr = generateFPMemRegInstruction(DSTMemReg, node, storeLowMR, sourceRegister, cg);
            }
         }

      cg->decReferenceCount(valueChild);
      storeLowMR->decNodeReferenceCounts(cg);
      if (nodeIsIndirect)
         {
         cg->setImplicitExceptionPoint(instr);
         }
      return NULL;
      }

   }

TR::Register *OMR::X86::I386::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *target = cg->allocateSinglePrecisionRegister(TR_X87);
   if (child->getRegister() == NULL && child->getReferenceCount() == 1 && child->getOpCode().isLoadVar())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      generateFPRegMemInstruction(FLLDRegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::SymbolReference     *temp    = cg->allocateLocalTemp(TR::Int64);
      TR::Register            *longReg = cg->evaluate(child);
      TR::MemoryReference  *lowMR   = generateX86MemoryReference(temp, cg);
      generateMemRegInstruction(S4MemReg, node, lowMR, longReg->getLowOrder(), cg);
      generateMemRegInstruction(S4MemReg, node,
                                generateX86MemoryReference(*lowMR, 4, cg),
                                longReg->getHighOrder(), cg);
      generateFPRegMemInstruction(FLLDRegMem, node,
                                  target,
                                  generateX86MemoryReference(*lowMR, 0, cg), cg);
      cg->decReferenceCount(child);
      }

   target->setMayNeedPrecisionAdjustment();
   target->setNeedsPrecisionAdjustment();
   node->setRegister(target);
   if (cg->useSSEForSinglePrecision())
      target = coerceFPRToXMMR(node, target, cg);

   return target;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *target = cg->allocateRegister(TR_X87);
   if (child->getRegister() == NULL && child->getReferenceCount() == 1 && child->getOpCode().isLoadVar())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      generateFPRegMemInstruction(DLLDRegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::SymbolReference     *temp    = cg->allocateLocalTemp(TR::Int64);
      TR::Register            *longReg = cg->evaluate(child);
      TR::MemoryReference  *lowMR   = generateX86MemoryReference(temp, cg);
      generateMemRegInstruction(S4MemReg, node, lowMR, longReg->getLowOrder(), cg);
      generateMemRegInstruction(S4MemReg, node,
                                generateX86MemoryReference(*lowMR, 4, cg),
                                longReg->getHighOrder(), cg);
      generateFPRegMemInstruction(DLLDRegMem, node,
                                  target,
                                  generateX86MemoryReference(*lowMR, 0, cg), cg);
      cg->decReferenceCount(child);
      }

   target->setMayNeedPrecisionAdjustment();
   target->setNeedsPrecisionAdjustment();
   node->setRegister(target);
   if (cg->useSSEForDoublePrecision())
      target = coerceFPRToXMMR(node, target, cg);

   return target;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::performLload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *lowRegister = NULL, *highRegister = NULL;
   TR::SymbolReference *symRef = node->getSymbolReference();
   bool isVolatile = false;

   if (symRef && !symRef->isUnresolved())
      {
      TR::Symbol *symbol = symRef->getSymbol();
      isVolatile = symbol->isVolatile();
      }

   if (isVolatile || (symRef && symRef->isUnresolved()))
      {
      TR::Machine *machine = cg->machine();

      if (cg->useSSEForDoublePrecision() && performTransformation(comp, "O^O Using SSE for volatile load %s\n", cg->getDebug()->getName(node)))
         {
         TR_X86ProcessorInfo &p = cg->getX86ProcessorInfo();

         if (p.isGenuineIntel())
            {
            TR::Register *xmmReg = cg->allocateRegister(TR_FPR);
            generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, xmmReg, sourceMR, cg);

            //allocate: lowRegister
            lowRegister = cg->allocateRegister();
            //allocate: highRegister
            highRegister = cg->allocateRegister();

            generateRegRegInstruction(MOVDReg4Reg, node, lowRegister, xmmReg, cg);
            generateRegImmInstruction(PSRLQRegImm1, node, xmmReg, 0x20, cg);
            generateRegRegInstruction(MOVDReg4Reg, node, highRegister, xmmReg, cg);

            cg->stopUsingRegister(xmmReg);
            }
         else
            {
            //generate stack piece
            TR::MemoryReference *stackLow  = cg->machine()->getDummyLocalMR(TR::Int64);
            TR::MemoryReference *stackHigh = generateX86MemoryReference(*stackLow, 4, cg);

            //allocate: XMM
            TR::Register *reg = cg->allocateRegister(TR_FPR);

            //generate: xmm <- sourceMR
            generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, reg, sourceMR, cg);
            //generate: stack1 <- xmm
            TR::MemoryReference *stack = generateX86MemoryReference(*stackLow, 0, cg);
            generateMemRegInstruction(MOVSDMemReg, node, stack, reg, cg);
            //stop using: xmm
            cg->stopUsingRegister(reg);

            //allocate: lowRegister
            lowRegister = cg->allocateRegister();
            //allocate: highRegister
            highRegister = cg->allocateRegister();

            //generate: lowRegister <- stack1
            generateRegMemInstruction(L4RegMem, node, lowRegister, stackLow, cg);
            //generate: highRegister <- stack1_plus4
            generateRegMemInstruction(L4RegMem, node, highRegister, stackHigh, cg);
            }
         }
      else
         {
         TR_ASSERT( cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction(), "Assumption of support of the CMPXCHG8B instruction failed in performLload()" );

         TR::Register *ecxReg=NULL, *ebxReg=NULL;
         TR::RegisterDependencyConditions  *deps = NULL;

         lowRegister = cg->allocateRegister();
         highRegister = cg->allocateRegister();
         ecxReg = cg->allocateRegister();
         ebxReg = cg->allocateRegister();
         deps = generateRegisterDependencyConditions((uint8_t)4, (uint8_t)4, cg);

         deps->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
         deps->addPostCondition(highRegister, TR::RealRegister::edx, cg);
         deps->addPostCondition(ecxReg, TR::RealRegister::ecx, cg);
         deps->addPostCondition(ebxReg, TR::RealRegister::ebx, cg);
         deps->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
         deps->addPreCondition(highRegister, TR::RealRegister::edx, cg);
         deps->addPreCondition(ecxReg, TR::RealRegister::ecx, cg);
         deps->addPreCondition(ebxReg, TR::RealRegister::ebx, cg);

         generateRegRegInstruction (MOV4RegReg, node, ecxReg, highRegister, cg);
         generateRegRegInstruction (MOV4RegReg, node, ebxReg, lowRegister, cg);
         generateMemInstruction ( cg->comp()->target().isSMP() ? LCMPXCHG8BMem : CMPXCHG8BMem, node, sourceMR, deps, cg);

         cg->stopUsingRegister(ecxReg);
         cg->stopUsingRegister(ebxReg);

         }
      }
   else
      {
      lowRegister  = loadMemory(node, sourceMR, TR_RematerializableInt, node->getOpCode().isIndirect(), cg);
      highRegister = loadMemory(node, generateX86MemoryReference(*sourceMR, 4, cg), TR_RematerializableInt, false, cg);

      TR::SymbolReference& mrSymRef = sourceMR->getSymbolReference();
      if (mrSymRef.isUnresolved())
         {
         padUnresolvedDataReferences(node, mrSymRef, cg);
         }
      }

   TR::RegisterPair *longRegister = cg->allocateRegisterPair(lowRegister, highRegister);
   node->setRegister(longRegister);
   return longRegister;
   }

// also handles ilload
TR::Register *OMR::X86::I386::TreeEvaluator::lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *sourceMR = generateX86MemoryReference(node, cg);
   TR::Register            *reg      = performLload(node, sourceMR, cg);
   reg->setMemRef(sourceMR);
   sourceMR->decNodeReferenceCounts(cg);
   return reg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL && child->getOpCode().isLoadVar() && child->getReferenceCount() == 1)
      {
      // Load up the child as a double, then as a long if necessary
      //
      tempMR = generateX86MemoryReference(child, cg);
      performDload(node, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *longReg = cg->evaluate(child);

      tempMR = cg->machine()->getDummyLocalMR(TR::Int64);
      generateMemRegInstruction(S4MemReg, node, tempMR, longReg->getLowOrder(), cg);
      generateMemRegInstruction(S4MemReg, node, generateX86MemoryReference(*tempMR, 4, cg), longReg->getHighOrder(), cg);

      performDload(node, generateX86MemoryReference(*tempMR, 0, cg), cg);
      }

   cg->decReferenceCount(child);
   return node->getRegister();
   }

TR::Register *OMR::X86::I386::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child   = node->getFirstChild();
   TR::Register            *lowReg  = cg->allocateRegister();
   TR::Register            *highReg = cg->allocateRegister();
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL && child->getOpCode().isLoadVar() && (child->getReferenceCount() == 1))
      {
      // Load up the child as a long, then as a double if necessary.
      //
      tempMR = generateX86MemoryReference(child, cg);

      generateRegMemInstruction(L4RegMem, node, lowReg, tempMR, cg);
      generateRegMemInstruction(L4RegMem, node, highReg, generateX86MemoryReference(*tempMR, 4, cg), cg);

      if (child->getReferenceCount() > 1)
         performDload(child, generateX86MemoryReference(*tempMR, 0, cg), cg);

      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *doubleReg = cg->evaluate(child);

      tempMR = cg->machine()->getDummyLocalMR(TR::Double);
      if (doubleReg->getKind() == TR_FPR)
         generateMemRegInstruction(MOVSDMemReg, node, tempMR, doubleReg, cg);
      else
         generateFPMemRegInstruction(DSTMemReg, node, tempMR, doubleReg, cg);

      generateRegMemInstruction(L4RegMem, node, lowReg, generateX86MemoryReference(*tempMR, 0, cg), cg);
      generateRegMemInstruction(L4RegMem, node, highReg, generateX86MemoryReference(*tempMR, 4, cg), cg);
      }

   // There's a special-case check for NaN values, which have to be
   // normalized to a particular NaN value.
   //

   TR::LabelSymbol *lab0 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *lab1 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *lab2 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *lab3 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   if (node->normalizeNanValues())
      {
      lab0->setStartInternalControlFlow();
      lab2->setEndInternalControlFlow();
      generateLabelInstruction(LABEL, node, lab0, cg);
      generateRegImmInstruction(CMP4RegImm4, node, highReg, 0x7FF00000, cg);
      generateLabelInstruction(JG4, node, lab1, cg);
      generateLabelInstruction(JE4, node, lab3, cg);
      generateRegImmInstruction(CMP4RegImm4, node, highReg, 0xFFF00000, cg);
      generateLabelInstruction(JA4, node, lab1, cg);
      generateLabelInstruction(JB4, node, lab2, cg);
      generateLabelInstruction(LABEL, node, lab3, cg);
      generateRegRegInstruction(TEST4RegReg, node, lowReg, lowReg, cg);
      generateLabelInstruction(JE4, node, lab2, cg);
      generateLabelInstruction(LABEL, node, lab1, cg);
      generateRegImmInstruction(MOV4RegImm4, node, highReg, 0x7FF80000, cg);
      generateRegRegInstruction(XOR4RegReg, node, lowReg, lowReg, cg);
      }

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)2, cg);
   deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
   generateLabelInstruction(LABEL, node, lab2, deps, cg);

   TR::RegisterPair *target = cg->allocateRegisterPair(lowReg, highReg);
   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t                              lowValue    = secondChild->getLongIntLow();
      int32_t                              highValue   = secondChild->getLongIntHigh();
      TR::Node                             *firstChild  = node->getFirstChild();
      TR::Register                         *cmpRegister = NULL;
      TR::RegisterDependencyConditions  *deps        = NULL;

      if ((lowValue | highValue) == 0)
         {
         TR::Node     *landConstChild;
         TR::Register *targetRegister;
         bool         targetNeedsToBeExplicitlyStopped = false;

         if (firstChild->getOpCodeValue() == TR::land &&
             firstChild->getReferenceCount() == 1    &&
             firstChild->getRegister() == NULL       &&
             (landConstChild = firstChild->getSecondChild())->getOpCodeValue() == TR::lconst &&
             landConstChild->getLongIntLow()  == 0   &&
             landConstChild->getLongIntHigh() == 0xffffffff)
            {
            TR::Node *landFirstChild = firstChild->getFirstChild();
            if (landFirstChild->getReferenceCount() == 1 &&
                landFirstChild->getRegister() == NULL    &&
                landFirstChild->getOpCode().isLoadVar())
               {
               targetRegister = cg->allocateRegister();
               TR::MemoryReference  *tempMR = generateX86MemoryReference(landFirstChild, cg);
               tempMR->getSymbolReference().addToOffset(4);
               generateRegMemInstruction(L4RegMem,
                                         landFirstChild,
                                         targetRegister,
                                         tempMR, cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            else
               {
               targetRegister = cg->evaluate(landFirstChild)->getHighOrder();
               }

            generateRegRegInstruction(TEST4RegReg, node, targetRegister, targetRegister, cg);
            cg->decReferenceCount(landFirstChild);
            }
         else
            {
            cmpRegister = cg->evaluate(firstChild);
            targetRegister = cmpRegister->getLowOrder();
            if (firstChild->getReferenceCount() != 1)
               {
               targetRegister = cg->allocateRegister();
               generateRegRegInstruction(MOV4RegReg, node, targetRegister, cmpRegister->getLowOrder(), cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            generateRegRegInstruction(OR4RegReg, node, targetRegister, cmpRegister->getHighOrder(), cg);
            }

         generateConditionalJumpInstruction(JE4, node, cg);

         if (targetNeedsToBeExplicitlyStopped)
            {
            cg->stopUsingRegister(targetRegister);
            }
         }
      else
         {
         List<TR::Register>  popRegisters(cg->trMemory());
         TR::LabelSymbol     *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol     *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         startLabel->setStartInternalControlFlow();
         doneLabel->setEndInternalControlFlow();

         cmpRegister = cg->evaluate(firstChild);
         generateLabelInstruction(LABEL, node, startLabel, cg);
         compareGPRegisterToConstantForEquality(node, lowValue, cmpRegister->getLowOrder(), cg);

         // Evaluate the global register dependencies and emit the branches by hand;
         // we cannot call generateConditionalJumpInstruction() here because we need
         // to add extra post-conditions, and the conditions need to be present on
         // multiple instructions.
         //
         if (node->getNumChildren() == 3)
            {
            TR::Node *third = node->getChild(2);
            cg->evaluate(third);
            deps = generateRegisterDependencyConditions(third, cg, 2, &popRegisters);
            deps->setMayNeedToPopFPRegisters(true);
            deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
            deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            deps->stopAddingConditions();
            generateLabelInstruction(JNE4, node, doneLabel, deps, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(JE4, node, destinationLabel, deps, cg);
            cg->decReferenceCount(third);
            }
         else
            {
            generateLabelInstruction(JNE4, node, doneLabel, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(JE4, node, destinationLabel, cg);
            deps = generateRegisterDependencyConditions((uint8_t)0, 2, cg);
            deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
            deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            }

         generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

         if (!popRegisters.isEmpty())
            {
            ListIterator<TR::Register> popRegsIt(&popRegisters);
            for (TR::Register *popRegister = popRegsIt.getFirst(); popRegister != NULL; popRegister = popRegsIt.getNext())
               {
               generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
               cg->stopUsingRegister(popRegister);
               }
            }
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.longEqualityCompareAndBranchAnalyser(node, NULL, destinationLabel, JE4);
      }
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   if (secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      List<TR::Register>                    popRegisters(cg->trMemory());
      int32_t                              lowValue    = secondChild->getLongIntLow();
      int32_t                              highValue   = secondChild->getLongIntHigh();
      TR::Node                             *firstChild  = node->getFirstChild();
      TR::Register                         *cmpRegister = NULL;
      TR::RegisterDependencyConditions  *deps        = NULL;

      if ((lowValue | highValue) == 0)
         {
         TR::Node     *landConstChild;
         TR::Register *targetRegister;
         bool         targetNeedsToBeExplicitlyStopped = false;

         if (firstChild->getOpCodeValue() == TR::land &&
             firstChild->getReferenceCount() == 1    &&
             firstChild->getRegister() == NULL       &&
             (landConstChild = firstChild->getSecondChild())->getOpCodeValue() == TR::lconst &&
             landConstChild->getLongIntLow()  == 0   &&
             landConstChild->getLongIntHigh() == 0xffffffff)
            {
            TR::Node *landFirstChild = firstChild->getFirstChild();
            if (landFirstChild->getReferenceCount() == 1 &&
                landFirstChild->getRegister() == NULL    &&
                landFirstChild->getOpCode().isLoadVar())
               {
               targetRegister = cg->allocateRegister();
               TR::MemoryReference  *tempMR = generateX86MemoryReference(landFirstChild, cg);
               tempMR->getSymbolReference().addToOffset(4);
               generateRegMemInstruction(L4RegMem,
                                         landFirstChild,
                                         targetRegister,
                                         tempMR, cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            else
               {
               targetRegister = cg->evaluate(landFirstChild)->getHighOrder();
               }
            generateRegRegInstruction(TEST4RegReg, node, targetRegister,targetRegister, cg);
            cg->decReferenceCount(landFirstChild);
            }
         else
            {
            cmpRegister = cg->evaluate(firstChild);
            targetRegister = cmpRegister->getLowOrder();
            if (firstChild->getReferenceCount() != 1)
               {
               targetRegister = cg->allocateRegister();
               generateRegRegInstruction(MOV4RegReg, node, targetRegister, cmpRegister->getLowOrder(), cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            generateRegRegInstruction(OR4RegReg, node, targetRegister, cmpRegister->getHighOrder(), cg);
            }

         generateConditionalJumpInstruction(JNE4, node, cg);

         if (targetNeedsToBeExplicitlyStopped)
            {
            cg->stopUsingRegister(targetRegister);
            }
         }
      else
         {
         cmpRegister = cg->evaluate(firstChild);
         compareGPRegisterToConstantForEquality(node, lowValue, cmpRegister->getLowOrder(), cg);

         // Evaluate the global register dependencies and emit the branches by hand;
         // we cannot call generateConditionalJumpInstruction() here because we need
         // to add extra post-conditions, and the conditions need to be present on
         // multiple instructions.
         //
         if (node->getNumChildren() == 3)
            {
            TR::Node *third = node->getChild(2);
            cg->evaluate(third);
            deps = generateRegisterDependencyConditions(third, cg, 1, &popRegisters);
            deps->setMayNeedToPopFPRegisters(true);
            deps->stopAddingConditions();
            generateLabelInstruction(JNE4, node, destinationLabel, deps, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(JNE4, node, destinationLabel, deps, cg);
            cg->decReferenceCount(third);

            if (!popRegisters.isEmpty())
               {
               ListIterator<TR::Register> popRegsIt(&popRegisters);
               for (TR::Register *popRegister = popRegsIt.getFirst(); popRegister != NULL; popRegister = popRegsIt.getNext())
                  {
                  generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
                  cg->stopUsingRegister(popRegister);
                  }
               }
            }
         else
            {
            generateLabelInstruction(JNE4, node, destinationLabel, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(JNE4, node, destinationLabel, cg);
            }
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.longEqualityCompareAndBranchAnalyser(node, destinationLabel, destinationLabel, JNE4);
      }
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (generateLAddOrSubForOverflowCheck(node, cg))
      {
      generateConditionalJumpInstruction(JO4, node, cg, true);
      }
   else
      {
      TR_X86OpCodes compareOp = node->getOpCode().isUnsigned() ? JB4 : JL4;
      TR_X86OpCodes reverseCompareOp = node->getOpCode().isUnsigned() ? JA4 : JG4;
      compareLongsForOrder(node, compareOp, reverseCompareOp, JB4, cg);
      }
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (generateLAddOrSubForOverflowCheck(node, cg))
      {
      generateConditionalJumpInstruction(JNO4, node, cg, true);
      }
   else
      {
      TR_X86OpCodes compareOp = node->getOpCode().isUnsigned() ? JA4 : JG4;
      TR_X86OpCodes reverseCompareOp = node->getOpCode().isUnsigned() ? JB4 : JL4;
      compareLongsForOrder(node, compareOp, reverseCompareOp, JAE4, cg);
      }
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_X86OpCodes compareOp = node->getOpCode().isUnsigned() ? JA4 : JG4;
   TR_X86OpCodes reverseCompareOp = node->getOpCode().isUnsigned() ? JB4 : JL4;
   compareLongsForOrder(node, compareOp, reverseCompareOp, JA4, cg);
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_X86OpCodes compareOp = node->getOpCode().isUnsigned() ? JB4 : JL4;
   TR_X86OpCodes reverseCompareOp = node->getOpCode().isUnsigned() ? JA4 : JG4;
   compareLongsForOrder(node, compareOp, reverseCompareOp, JBE4, cg);
   return NULL;
   }


TR::Register *OMR::X86::I386::TreeEvaluator::lselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *condition = node->getChild(0);
   TR::Node *trueVal   = node->getChild(1);
   TR::Node *falseVal  = node->getChild(2);

   TR::Register *falseReg = cg->evaluate(falseVal);
   TR::Register *trueReg  = cg->longClobberEvaluate(trueVal);

   auto condOp = condition->getOpCode();
   bool longCompare = (condition->getOpCode().isBooleanCompare() && condition->getFirstChild()->getOpCode().isLong());
   if (!longCompare && condOp.isCompareForEquality() && condition->getFirstChild()->getOpCode().isIntegerOrAddress())
      {
      compareIntegersForEquality(condition, cg);
      if(condOp.isCompareTrueIfEqual())
         {
         generateRegRegInstruction(CMOVNE4RegReg, node,
                                   trueReg-> getRegisterPair()->getLowOrder(),
                                   falseReg->getRegisterPair()->getLowOrder(), cg);
         generateRegRegInstruction(CMOVNE4RegReg, node,
                                   trueReg-> getRegisterPair()->getHighOrder(),
                                   falseReg->getRegisterPair()->getHighOrder(), cg);
         }
      else
         {
         generateRegRegInstruction(CMOVE4RegReg, node,
                                   trueReg-> getRegisterPair()->getLowOrder(),
                                   falseReg->getRegisterPair()->getLowOrder(), cg);
         generateRegRegInstruction(CMOVE4RegReg, node,
                                   trueReg-> getRegisterPair()->getHighOrder(),
                                   falseReg->getRegisterPair()->getHighOrder(), cg);
         }
      }
   else if (!longCompare && condOp.isCompareForOrder() && condition->getFirstChild()->getOpCode().isIntegerOrAddress())
      {
      compareIntegersForOrder(condition, cg);
      generateRegRegInstruction((condOp.isCompareTrueIfEqual()) ?
                         ((condOp.isCompareTrueIfGreater()) ? CMOVL4RegReg : CMOVG4RegReg) :
                         ((condOp.isCompareTrueIfGreater()) ? CMOVLE4RegReg : CMOVGE4RegReg), node, trueReg->getRegisterPair()->getLowOrder(), falseReg->getRegisterPair()->getLowOrder(), cg);
      generateRegRegInstruction((condOp.isCompareTrueIfEqual()) ?
                         ((condOp.isCompareTrueIfGreater()) ? CMOVL4RegReg : CMOVG4RegReg) :
                         ((condOp.isCompareTrueIfGreater()) ? CMOVLE4RegReg : CMOVGE4RegReg), node, trueReg->getRegisterPair()->getHighOrder(), falseReg->getRegisterPair()->getHighOrder(), cg);
      }
   else
      {
      TR::Register *condReg  = cg->evaluate(condition);
      generateRegRegInstruction(TEST4RegReg, node, condReg, condReg, cg); // the condition is an int
      generateRegRegInstruction(CMOVE4RegReg, node,
                                trueReg-> getRegisterPair()->getLowOrder(),
                                falseReg->getRegisterPair()->getLowOrder(), cg);
      generateRegRegInstruction(CMOVE4RegReg, node,
                                trueReg-> getRegisterPair()->getHighOrder(),
                                falseReg->getRegisterPair()->getHighOrder(), cg);
      }

   node->setRegister(trueReg);
   cg->decReferenceCount(condition);
   cg->decReferenceCount(trueVal);
   cg->decReferenceCount(falseVal);
   return node->getRegister();
   }

TR::Register *
OMR::X86::I386::TreeEvaluator::integerPairByteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in byteswapEvaluator");
   TR::Node *child = node->getFirstChild();
   TR::Register *target     = cg->longClobberEvaluate(child);
   TR::RegisterPair *pair   = target->getRegisterPair();
   TR::Register *lo   = pair->getLowOrder();
   TR::Register *hi   = pair->getHighOrder();

   generateRegInstruction(BSWAP4Reg, node, lo, cg);
   generateRegInstruction(BSWAP4Reg, node, hi, cg);
   pair->setLowOrder(hi, cg);
   pair->setHighOrder(lo, cg);

   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::integerPairMinMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_X86OpCodes SETccHi = BADIA32Op;
   TR_X86OpCodes SETccLo = BADIA32Op;
   switch (node->getOpCodeValue())
      {
      case TR::lmin:
         SETccHi = SETL1Reg;
         SETccLo = SETB1Reg;
         break;
      case TR::lmax:
         SETccHi = SETG1Reg;
         SETccLo = SETA1Reg;
         break;
      default:
         TR_ASSERT(false, "INCORRECT IL OPCODE.");
         break;
      }
   auto operand0 = cg->evaluate(node->getChild(0));
   auto operand1 = cg->evaluate(node->getChild(1));
   auto result = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());

   generateRegRegInstruction(CMP4RegReg, node, operand0->getLowOrder(), operand1->getLowOrder(), cg);
   generateRegInstruction(SETccLo, node, result->getLowOrder(), cg); // t1 = (low0 < low1)
   generateRegRegInstruction(CMP4RegReg, node, operand0->getHighOrder(), operand1->getHighOrder(), cg);
   generateRegInstruction(SETccHi, node, result->getHighOrder(), cg); // t2 = (high0 < high1)
   generateRegRegInstruction(CMOVE4RegReg, node, result->getHighOrder(), result->getLowOrder(), cg); // if (high0 == high1) then t2 = t1 = (low0 < low1)

   generateRegRegInstruction(TEST1RegReg,  node, result->getHighOrder(), result->getHighOrder(),  cg);
   generateRegRegInstruction(MOV4RegReg,   node, result->getLowOrder(),  operand0->getLowOrder(),  cg);
   generateRegRegInstruction(MOV4RegReg,   node, result->getHighOrder(), operand0->getHighOrder(), cg);
   generateRegRegInstruction(CMOVE4RegReg, node, result->getLowOrder(),  operand1->getLowOrder(),  cg);
   generateRegRegInstruction(CMOVE4RegReg, node, result->getHighOrder(), operand1->getHighOrder(), cg);

   node->setRegister(result);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   return result;
   }

TR::Register *
OMR::X86::I386::TreeEvaluator::lcmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *pointer      = node->getChild(0);
   TR::Node *compareValue = node->getChild(1);
   TR::Node *replaceValue = node->getChild(2);

   TR::Register *pointerReg = cg->evaluate(pointer);
   TR::MemoryReference *memRef = generateX86MemoryReference(pointerReg, 0, cg);
   TR::Register *compareReg = cg->longClobberEvaluate(compareValue); // clobber evaluate because edx:eax may potentially get clobbered
   TR::Register *replaceReg = cg->evaluate(replaceValue);

   TR::Register *resultReg = cg->allocateRegister();
   generateRegRegInstruction(XOR4RegReg, node, resultReg, resultReg, cg);

   TR::RegisterDependencyConditions *deps =
      generateRegisterDependencyConditions((uint8_t)4, (uint8_t)4, cg);
   deps->addPreCondition (compareReg->getHighOrder(), TR::RealRegister::edx, cg);
   deps->addPreCondition (compareReg->getLowOrder(),  TR::RealRegister::eax, cg);
   deps->addPreCondition (replaceReg->getHighOrder(), TR::RealRegister::ecx, cg);
   deps->addPreCondition (replaceReg->getLowOrder(),  TR::RealRegister::ebx, cg);
   deps->addPostCondition(compareReg->getHighOrder(), TR::RealRegister::edx, cg);
   deps->addPostCondition(compareReg->getLowOrder(),  TR::RealRegister::eax, cg);
   deps->addPostCondition(replaceReg->getHighOrder(), TR::RealRegister::ecx, cg);
   deps->addPostCondition(replaceReg->getLowOrder(),  TR::RealRegister::ebx, cg);
   generateMemInstruction(cg->comp()->target().isSMP() ? LCMPXCHG8BMem : CMPXCHG8BMem, node, memRef, deps, cg);

   cg->stopUsingRegister(compareReg);

   generateRegInstruction(SETNE1Reg, node, resultReg, cg);

   node->setRegister(resultReg);
   cg->decReferenceCount(pointer);
   cg->decReferenceCount(compareValue);
   cg->decReferenceCount(replaceValue);

   return resultReg;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register *
OMR::X86::I386::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register *
OMR::X86::I386::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

