/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
#include "codegen/InstOpCode.hpp"

class TR_OpaqueMethodBlock;

///////////////////////
//
// Helper functions
//

TR::Register *OMR::X86::I386::TreeEvaluator::longArithmeticCompareRegisterWithImmediate(
      TR::Node       *node,
      TR::Register   *cmpRegister,
      TR::Node       *immedChild,
      TR::InstOpCode::Mnemonic firstBranchOpCode,
      TR::InstOpCode::Mnemonic secondBranchOpCode,
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

   generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);
   compareGPRegisterToImmediate(node, cmpRegister->getHighOrder(), highValue, cg);
   generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, targetRegister, cg);
   generateLabelInstruction(TR::InstOpCode::JNE4, node, highDoneLabel, cg);

   compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
   generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, targetRegister, cg);
   generateLabelInstruction(firstBranchOpCode, node, doneLabel, cg);
   generateRegInstruction(TR::InstOpCode::NEG1Reg, node, targetRegister, cg);
   generateLabelInstruction(TR::InstOpCode::JMP4, node, doneLabel, cg);

   generateLabelInstruction(TR::InstOpCode::label, node, highDoneLabel, cg);
   generateLabelInstruction(secondBranchOpCode, node, doneLabel, cg);
   generateRegInstruction(TR::InstOpCode::NEG1Reg, node, targetRegister, cg);
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 3, cg);
   deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
   deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
   deps->addPostCondition(targetRegister, TR::RealRegister::ByteReg, cg);

   generateLabelInstruction(TR::InstOpCode::label, node, doneLabel, deps, cg);

   generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1, node, targetRegister, targetRegister, cg);

   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::compareLongAndSetOrderedBoolean(
      TR::Node       *node,
      TR::InstOpCode::Mnemonic highSetOpCode,
      TR::InstOpCode::Mnemonic lowSetOpCode,
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
      generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);

      compareGPRegisterToImmediate(node, cmpRegister->getHighOrder(), highValue, cg);

      targetRegister = cg->allocateRegister();

      if (cg->enableRegisterInterferences())
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

      generateRegInstruction(highSetOpCode, node, targetRegister, cg);
      generateLabelInstruction(TR::InstOpCode::JNE4, node, doneLabel, cg);

      compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
      generateRegInstruction(lowSetOpCode, node, targetRegister, cg);

      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 3, cg);
      deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
      deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
      deps->addPostCondition(targetRegister, TR::RealRegister::NoReg, cg);
      generateLabelInstruction(TR::InstOpCode::label, node, doneLabel, deps, cg);

      // Result of lcmpXX is an integer.
      //
      generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1, node, targetRegister, targetRegister, cg);

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
      TR::InstOpCode::Mnemonic    highOrderBranchOp,
      TR::InstOpCode::Mnemonic    highOrderReversedBranchOp,
      TR::InstOpCode::Mnemonic    lowOrderBranchOp,
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

      startLabel->setStartInternalControlFlow();
      doneLabel->setEndInternalControlFlow();
      generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);

      compareGPRegisterToImmediate(node, cmpRegister->getHighOrder(), highValue, cg);

      TR::RegisterDependencyConditions  *deps = NULL;
      if (node->getNumChildren() == 3)
         {
         TR::Node *third = node->getChild(2);
         cg->evaluate(third);
         deps = generateRegisterDependencyConditions(third, cg, 2);
         deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
         deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
         deps->stopAddingConditions();

         generateLabelInstruction(highOrderBranchOp, node, destinationLabel, deps, cg);
         generateLabelInstruction(TR::InstOpCode::JNE4, node, doneLabel, deps, cg);
         compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
         generateLabelInstruction(lowOrderBranchOp, node, destinationLabel, deps, cg);
         }
      else
         {
         generateLabelInstruction(highOrderBranchOp, node, destinationLabel, cg);
         generateLabelInstruction(TR::InstOpCode::JNE4, node, doneLabel, cg);
         compareGPRegisterToImmediate(node, cmpRegister->getLowOrder(), lowValue, cg);
         generateLabelInstruction(lowOrderBranchOp, node, destinationLabel, cg);
         deps = generateRegisterDependencyConditions((uint8_t)0, 2, cg);
         deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
         deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
         deps->stopAddingConditions();
         }

      generateLabelInstruction(TR::InstOpCode::label, node, doneLabel, deps, cg);

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

TR::Register*
OMR::X86::I386::TreeEvaluator::BadILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::floadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::aloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::astoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::astoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::istoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::GotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::gotoEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::areturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerReturnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::returnEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::athrowEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::acallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::callEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::directCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::laddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::isubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::asubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::imulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::smulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::idivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iudivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ludivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iuremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerDivOrRemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::inegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerNegEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairNegEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAbsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::labsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairAbsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpUnaryMaskEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ishlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerShlEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairShlEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ishrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerShrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairShrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerUshrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairUshrEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::irolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRolEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lrolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairRolEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::i2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::l2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::l2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::l2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::l2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::l2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::b2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::b2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::s2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::s2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::s2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::su2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2fEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2dEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::su2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::a2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::a2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::a2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::a2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndSetEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::acmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::acmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::acmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::acmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::acmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::scmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::scmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iflcmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iflcmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iflcmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iflcmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iffcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareFloatAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::compareDoubleAndBranchEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifacmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerIfCmpneEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifacmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpltEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifacmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifacmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifacmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unsignedIntegerIfCmpleEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifbcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifbcmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ifscmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ifscmpeqEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::aRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iRegStoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::aselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::iselectEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::MethodEnterHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::MethodExitHookEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::PassThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vnotEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDsplatsEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unaryVectorArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vectorBinaryArithmeticEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDloadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDstoreEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vcallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vconvEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vgetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDvgetelemEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDRegLoadEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::SIMDRegStoreEvaluator(node, cg);
   }


TR::Register*
OMR::X86::I386::TreeEvaluator::vindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::vfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::f2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::f2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2lEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::f2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::d2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::f2iEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::d2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2lEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::d2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::d2bEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::NewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::newvalueEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::newarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::anewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::variableNewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::variableNewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::multianewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::contigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::discontigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::acalliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::calliEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::indirectCallEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::luaddhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::aiaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::aladdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lusubhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::imulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulhEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerMulhEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lmulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairMulEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::CaseEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::NOPEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ResolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ResolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::OverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::overflowCHKEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::UnsignedOverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::overflowCHKEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::SpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::long2StringEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bitOpMemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::allocationFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::loadFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::storeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fullFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::lcmpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflcmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ificmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflcmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iflcmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iuaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::luaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairAddEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairSubEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ibatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::isatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iiatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ilatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::atomicorEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::branchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpSqrtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpSqrtEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dfloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ffloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fceilEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::imaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minmaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairMinMaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::minmaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iuminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerPairMinMaxEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::luminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::fminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::dminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ihbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ilbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::inolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::inotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lhbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::llbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *inputReg = cg->longClobberEvaluate(child);
   TR::Register *inputHigh = inputReg->getHighOrder();
   TR::Register *inputLow = inputReg->getLowOrder();
   TR::Register *resultReg = inputLow;

   //add low result and high result together
   generateRegRegInstruction(TR::InstOpCode::POPCNT4RegReg, node, inputLow, inputLow, cg);
   generateRegRegInstruction(TR::InstOpCode::POPCNT4RegReg, node, inputHigh, inputHigh, cg);
   generateRegRegInstruction(TR::InstOpCode::ADD4RegReg, node, inputLow, inputHigh, cg);

   cg->stopUsingRegister(inputHigh);
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::ibyteswapEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::ibitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bitpermuteEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::scompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::icompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::bexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::sexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::iexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::lexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::badILOpEvaluator(node, cg);
   }

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
            generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, lowRegister, highRegister, cg);
            }
         else
            {
            generateRegMemInstruction(TR::InstOpCode::LEA4RegMem, node, lowRegister,
                                      generateX86MemoryReference(highRegister, lowValue - highValue, cg), cg);
            }
         }
      else
         {
         lowRegister = loadConstant(node, lowValue, TR_RematerializableInt, cg);
         highRegister = cg->allocateRegister();
         generateRegMemInstruction(TR::InstOpCode::LEA4RegMem, node, highRegister,
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

bool OMR::X86::I386::TreeEvaluator::lstoreEvaluatorIsNodeVolatile(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();

   if (symRef && !symRef->isUnresolved())
      {
      TR::Symbol *symbol = symRef->getSymbol();
      return symbol->isVolatile();
      }

   return false;
   }

void OMR::X86::I386::TreeEvaluator::lStoreEvaluatorSetHighLowMRIfNeeded(TR::Node *node,
                                                                        TR::MemoryReference *lowMR,
                                                                        TR::MemoryReference *highMR,
                                                                        TR::CodeGenerator *cg) {}

// also handles lstorei
TR::Register *OMR::X86::I386::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *valueChild;
   TR::SymbolReference *symRef = node->getSymbolReference();
   bool isVolatile = TR::TreeEvaluator::lstoreEvaluatorIsNodeVolatile(node, cg);

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
         instr = generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, lowMR, valueReg, cg);
         generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, highMR, valueReg, cg);
         cg->stopUsingRegister(valueReg);
         }
      else
         {
         instr = generateMemImmInstruction(TR::InstOpCode::S4MemImm4, node, lowMR, lowValue, cg);
         generateMemImmInstruction(TR::InstOpCode::S4MemImm4, node, highMR, highValue, cg);
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
            if (performTransformation(comp, "O^O Using SSE for volatile store %s\n", cg->getDebug()->getName(node)))
               {
               //Get stack piece
               TR::MemoryReference *stackLow  = cg->machine()->getDummyLocalMR(TR::Int64);
               TR::MemoryReference *stackHigh = generateX86MemoryReference(*stackLow, 4, cg);

               //generate: stack1 <- (valueReg->getloworder())    [TR::InstOpCode::S4MemReg]
               instr = generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, stackLow, valueReg->getLowOrder(), cg);
               //generate: stack1_plus4 <- (valueReg->gethighorder())
               generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, stackHigh, valueReg->getHighOrder(), cg);


               //generate: xmm <- stack1
               TR::MemoryReference *stack = generateX86MemoryReference(*stackLow, 0, cg);
               //Allocate XMM Reg
               TR::Register *reg = cg->allocateRegister(TR_FPR);
               generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, reg, stack, cg);

               //generate: lowMR <- xmm
               generateMemRegInstruction(TR::InstOpCode::MOVSDMemReg, node, lowMR, reg, cg);

               //Stop using  xmm
               cg->stopUsingRegister(reg);
               }
            else
               {
               TR_ASSERT_FATAL(cg->comp()->compileRelocatableCode() || cg->comp()->compilePortableCode() || cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_CX8), "Assumption of support of the CMPXCHG8B instruction failed in lstoreEvaluator()" );

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

               instr = generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, eaxReg, lowMR, cg); // forming the EDX:EAX pair
               generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, edxReg, highMR, cg);
               lowMR->setIgnoreVolatile();
               highMR->setIgnoreVolatile();
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, ebxReg, valueReg->getLowOrder(), cg); // forming the ECX:EBX pair
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, ecxReg, valueReg->getHighOrder(), cg);

               TR::MemoryReference  *cmpxchgMR = generateX86MemoryReference(node, cg);
               generateMemInstruction (cg->comp()->target().isSMP() ? TR::InstOpCode::LCMPXCHG8BMem : TR::InstOpCode::CMPXCHG8BMem, node, cmpxchgMR, deps, cg);

               cg->stopUsingRegister(eaxReg);
               cg->stopUsingRegister(edxReg);
               cg->stopUsingRegister(ecxReg);
               cg->stopUsingRegister(ebxReg);
               }
            }
         else if(symRef && symRef->isUnresolved() && symRef->getSymbol()->isVolatile() && (!comp->getOption(TR_DisableNewX86VolatileSupport) && cg->comp()->target().is32Bit()) )
            {
            TR_ASSERT_FATAL(cg->comp()->compileRelocatableCode() || cg->comp()->compilePortableCode() || cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_CX8), "Assumption of support of the CMPXCHG8B instruction failed in lstoreEvaluator()" );
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

            generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, ebxReg, valueReg->getLowOrder(), cg); // forming the ECX:EBX pair
            generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, ecxReg, valueReg->getHighOrder(), cg);

            instr = generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, eaxReg, lowMR, cg); // forming the EDX:EAX pair
            generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, edxReg, highMR, cg);
            lowMR->setIgnoreVolatile();
            highMR->setIgnoreVolatile();
            lowMR->setProcessAsLongVolatileLow();
            highMR->setProcessAsLongVolatileHigh();

            TR::MemoryReference  *cmpxchgMR = generateX86MemoryReference(node, cg);
            generateMemInstruction (cg->comp()->target().isSMP() ? TR::InstOpCode::LCMPXCHG8BMem : TR::InstOpCode::CMPXCHG8BMem, node, cmpxchgMR, deps, cg);

            cg->stopUsingRegister(eaxReg);
            cg->stopUsingRegister(edxReg);
            cg->stopUsingRegister(ecxReg);
            cg->stopUsingRegister(ebxReg);
            }
         else
            {
            instr = generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, lowMR, valueReg->getLowOrder(), cg);
            generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, highMR, valueReg->getHighOrder(), cg);

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

   TR::TreeEvaluator::lStoreEvaluatorSetHighLowMRIfNeeded(node, lowMR, highMR, cg);

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
      generateMemInstruction(TR::InstOpCode::LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, DOUBLE_PRECISION_ROUND_TO_NEAREST), cg), cg);
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
      generateInstruction(TR::InstOpCode::RET, node, dependencies, cg);
      }
   else
      {
      generateImmInstruction(TR::InstOpCode::RETImm2, node, 0, dependencies, cg);
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
            instr = generateMemImmInstruction(TR::InstOpCode::ADD4MemImms, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(TR::InstOpCode::ADD4RegImms, node, targetRegister->getLowOrder(), lowValue, cg);
         }
      else if (lowValue == 128)
         {
         if (isMemOp)
            instr = generateMemImmInstruction(TR::InstOpCode::SUB4MemImms, node, lowMR, -128, cg);
         else
            instr = generateRegImmInstruction(TR::InstOpCode::SUB4RegImms, node, targetRegister->getLowOrder(), -128, cg);

         // Use SBB of the negation of (highValue + 1) for the high order,
         // since we are getting the carry bit in the wrong sense from the SUB.
         highValue = -(highValue + 1);
         }
      else
         {
         if (isMemOp)
            instr = generateMemImmInstruction(TR::InstOpCode::ADD4MemImm4, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(TR::InstOpCode::ADD4RegImm4, node, targetRegister->getLowOrder(), lowValue, cg);
         }

      TR::InstOpCode::Mnemonic opCode;

      if (highValue >= -128 && highValue <= 127)
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = TR::InstOpCode::SBB4MemImms;
            else
               opCode = TR::InstOpCode::SBB4RegImms;
         else
            if (isMemOp)
               opCode = TR::InstOpCode::ADC4MemImms;
            else
               opCode = TR::InstOpCode::ADC4RegImms;
         }
      else
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = TR::InstOpCode::SBB4MemImm4;
            else
               opCode = TR::InstOpCode::SBB4RegImm4;
         else
            if (isMemOp)
               opCode = TR::InstOpCode::ADC4MemImm4;
            else
               opCode = TR::InstOpCode::ADC4RegImm4;
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
      instr = generateMemRegInstruction(TR::InstOpCode::ADD4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      generateMemRegInstruction(TR::InstOpCode::ADC4MemReg, node, highMR, tempReg->getHighOrder(), cg);

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
            instr = generateMemImmInstruction(TR::InstOpCode::SUB4MemImms, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(TR::InstOpCode::SUB4RegImms, node, targetRegister->getLowOrder(), lowValue, cg);
         }
      else if (lowValue == 128)
         {
         if (isMemOp)
            instr = generateMemImmInstruction(TR::InstOpCode::ADD4MemImms, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(TR::InstOpCode::ADD4RegImms, node, targetRegister->getLowOrder(), -128, cg);

         // Use ADC of the negation of (highValue + 1) for the high order,
         // since we are getting the carry bit in the wrong sense from the ADD.
         highValue = -(highValue + 1);
         }
      else
         {
         if (isMemOp)
            instr = generateMemImmInstruction(TR::InstOpCode::SUB4MemImm4, node, lowMR, lowValue, cg);
         else
            instr = generateRegImmInstruction(TR::InstOpCode::SUB4RegImm4, node, targetRegister->getLowOrder(), lowValue, cg);
         }

      TR::InstOpCode::Mnemonic opCode;

      if (highValue >= -128 && highValue <= 127)
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = TR::InstOpCode::ADC4MemImms;
            else
               opCode = TR::InstOpCode::ADC4RegImms;
         else
            if (isMemOp)
               opCode = TR::InstOpCode::SBB4MemImms;
            else
               opCode = TR::InstOpCode::SBB4RegImms;
         }
      else
         {
         if (lowValue == 128)
            if (isMemOp)
               opCode = TR::InstOpCode::ADC4MemImm4;
            else
               opCode = TR::InstOpCode::ADC4RegImm4;
         else
            if (isMemOp)
               opCode = TR::InstOpCode::SBB4MemImm4;
            else
               opCode = TR::InstOpCode::SBB4RegImm4;
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
      instr = generateMemRegInstruction(TR::InstOpCode::SUB4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      generateMemRegInstruction(TR::InstOpCode::SBB4MemReg, node, highMR, tempReg->getHighOrder(), cg);

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
      TR::InstOpCode::Mnemonic opCode;
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
         generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, lowRegister, lowRegister, cg);
         highRegister = cg->allocateRegister();
         generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, highRegister, highRegister, cg);
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
            generateRegInstruction(TR::InstOpCode::NEG4Reg, node, targetRegister->getLowOrder(), cg);
            generateRegImmInstruction(TR::InstOpCode::ADC4RegImms, node, targetRegister->getHighOrder(), 0, cg);
            generateRegInstruction(TR::InstOpCode::NEG4Reg, node, targetRegister->getHighOrder(), cg);
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
            generateRegMemInstruction(TR::InstOpCode::LEA4RegMem, node, highRegister, tempMR, cg);
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
                     opCode = TR::InstOpCode::IMUL4RegRegImms;
                     }
                  else
                     {
                     opCode = TR::InstOpCode::IMUL4RegRegImm4;
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
                     opCode = TR::InstOpCode::IMUL4RegMemImms;
                     }
                  else
                     {
                     opCode = TR::InstOpCode::IMUL4RegMemImm4;
                     }
                  nodeMR = generateX86MemoryReference(firstChild, cg);
                  highRegister = cg->allocateRegister();
                  generateRegMemImmInstruction(opCode, node, highRegister, nodeMR, highValue, cg);
                  }
               }
            }
         lowRegister = cg->allocateRegister();
         generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, lowRegister, lowRegister, cg);
         if (absValue != highValue)
            {
            generateRegInstruction(TR::InstOpCode::NEG4Reg, node, lowRegister, cg);
            generateRegImmInstruction(TR::InstOpCode::ADC4RegImms, node, highRegister, 0, cg);
            generateRegInstruction(TR::InstOpCode::NEG4Reg, node, highRegister, cg);
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
            opCode = TR::InstOpCode::IMUL4AccReg;
            }
         else
            {
            opCode = TR::InstOpCode::MUL4AccReg;
            }
         generateRegRegInstruction(opCode, node,
                                   lowRegister,
                                   sourceLow,
                                   dependencies, cg);
         if (!firstIU2L && !intMul)
            {
            generateRegRegInstruction(TR::InstOpCode::TEST4RegReg, node,
                                      multiplierRegister->getHighOrder(),
                                      multiplierRegister->getHighOrder(), cg);
            TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            startLabel->setStartInternalControlFlow();
            doneLabel->setEndInternalControlFlow();
            generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);
            generateLabelInstruction(TR::InstOpCode::JE4, node, doneLabel, cg);
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
                  opCode = TR::InstOpCode::IMUL4RegRegImms;
                  }
               else
                  {
                  opCode = TR::InstOpCode::IMUL4RegRegImm4;
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
            generateRegRegInstruction(TR::InstOpCode::ADD4RegReg, node, highRegister, tempRegister, cg);

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
            generateLabelInstruction(TR::InstOpCode::label, node, doneLabel, deps, cg);
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
            generateRegMemInstruction(TR::InstOpCode::LEA4RegMem, node, tempRegister, tempMR, cg);
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
                     opCode = TR::InstOpCode::IMUL4RegRegImms;
                     }
                  else
                     {
                     opCode = TR::InstOpCode::IMUL4RegRegImm4;
                     }
                  generateRegRegImmInstruction(opCode, node, tempRegister, sourceLow, highValue, cg);
                  }
               else
                  {
                  if (highValue >= -128 && highValue <= 127)
                     {
                     opCode = TR::InstOpCode::IMUL4RegMemImms;
                     }
                  else
                     {
                     opCode = TR::InstOpCode::IMUL4RegMemImm4;
                     }
                  nodeMR = generateX86MemoryReference(firstChild, cg);
                  generateRegMemImmInstruction(opCode, node, tempRegister, nodeMR, highValue, cg);
                  }
               }
            }

         TR_ASSERT(tempRegister!=NULL,"tempRegister shouldn't be NULL at this point");

         if (absValue != highValue)
            {
            generateRegInstruction(TR::InstOpCode::NEG4Reg, node, tempRegister, cg);
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
            opCode = TR::InstOpCode::IMUL4AccReg;
            }
         else
            {
            opCode = TR::InstOpCode::MUL4AccReg;
            }
         generateRegRegInstruction(opCode, node,
                                   lowRegister,
                                   sourceLow,
                                   dependencies, cg);

         // add both results of the highword
         generateRegRegInstruction(TR::InstOpCode::ADD4RegReg, node, highRegister, tempRegister, cg);
         // we can free up temporary registers
         for (int i=0; i < tempRegArraySize; i++)
            cg->stopUsingRegister(tempRegArray[i]);

         // Third highOrder * lowValue
         if (!firstIU2L && !intMul)
            {
            generateRegRegInstruction(TR::InstOpCode::TEST4RegReg, node,
                                      multiplierRegister->getHighOrder(),
                                      multiplierRegister->getHighOrder(), cg);
            TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
            startLabel->setStartInternalControlFlow();
            doneLabel->setEndInternalControlFlow();
            generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);
            generateLabelInstruction(TR::InstOpCode::JE4, node, doneLabel, cg);

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
                  opCode = TR::InstOpCode::IMUL4RegRegImms;
                  }
               else
                  {
                  opCode = TR::InstOpCode::IMUL4RegRegImm4;
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
            generateRegRegInstruction(TR::InstOpCode::ADD4RegReg, node, highRegister, tempRegister, cg);

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
            generateLabelInstruction(TR::InstOpCode::label, node, doneLabel, deps, cg);
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
   TR_ASSERT(0, "Unsupported front end");
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairRemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(0, "Unsupported front end");
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairNegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *targetRegister = cg->longClobberEvaluate(firstChild);
   node->setRegister(targetRegister);
   generateRegInstruction(TR::InstOpCode::NEG4Reg, node, targetRegister->getLowOrder(), cg);
   generateRegImmInstruction(TR::InstOpCode::ADC4RegImms, node, targetRegister->getHighOrder(), 0, cg);
   generateRegInstruction(TR::InstOpCode::NEG4Reg, node, targetRegister->getHighOrder(), cg);

   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::integerPairAbsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *targetRegister = cg->longClobberEvaluate(child);
   node->setRegister(targetRegister);
   TR::Register *signRegister = cg->allocateRegister(TR_GPR);
   generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, signRegister, targetRegister->getHighOrder(), cg);
   generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, signRegister, 31, cg);
   generateRegRegInstruction(TR::InstOpCode::ADD4RegReg, node, targetRegister->getLowOrder(), signRegister, cg);
   generateRegRegInstruction(TR::InstOpCode::ADC4RegReg, node, targetRegister->getHighOrder(), signRegister, cg);
   generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, targetRegister->getLowOrder(), signRegister, cg);
   generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, targetRegister->getHighOrder(), signRegister, cg);
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
         generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, targetRegister->getHighOrder(), tempRegister->getHighOrder(), cg);
         TR::MemoryReference  *tempMR = generateX86MemoryReference(cg);
         tempMR->setIndexRegister(tempRegister->getLowOrder());
         tempMR->setStride(value);
         generateRegMemInstruction(TR::InstOpCode::LEA4RegMem, node, targetRegister->getLowOrder(), tempMR, cg);
         generateRegRegImmInstruction(TR::InstOpCode::SHLD4RegRegImm1, node,
                                      targetRegister->getHighOrder(),
                                      tempRegister->getLowOrder(), value, cg);
         }
      else
         {
         targetRegister = cg->longClobberEvaluate(firstChild);
         if (value < 32)
            {
            generateRegRegImmInstruction(TR::InstOpCode::SHLD4RegRegImm1, node,
                                         targetRegister->getHighOrder(),
                                         targetRegister->getLowOrder(),
                                         value, cg);
            generateRegImmInstruction(TR::InstOpCode::SHL4RegImm1, node, targetRegister->getLowOrder(), value, cg);
            }
         else
            {
            if (value != 32)
               {
               value -= 32;
               generateRegImmInstruction(TR::InstOpCode::SHL4RegImm1, node, targetRegister->getLowOrder(), value, cg);
               }
            TR::Register *tempHighReg = targetRegister->getHighOrder();
            TR::RegisterPair *targetAsPair = targetRegister->getRegisterPair();
            targetAsPair->setHighOrder(targetRegister->getLowOrder(), cg);
            generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, tempHighReg, tempHighReg, cg);
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

      generateRegRegInstruction(TR::InstOpCode::SHLD4RegRegCL, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), shiftDependencies, cg);
      generateRegInstruction(TR::InstOpCode::SHL4RegCL, node, targetRegister->getLowOrder(), shiftDependencies, cg);
      generateRegImmInstruction(TR::InstOpCode::TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), cg);
      generateRegMemInstruction(TR::InstOpCode::CMOVNE4RegMem, node, targetRegister->getLowOrder(),
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
            generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, tempHighReg, targetRegister->getHighOrder(), cg);

            // E.g.
            // HH HH HH HH LL LL LL LL, before
            // HH HH LL LL LL LL LL LL, after
            generateRegRegImmInstruction(TR::InstOpCode::SHLD4RegRegImm1, node,
               targetRegister->getHighOrder(),
               targetRegister->getLowOrder(),
               value, cg);

            // HH HH LL LL LL LL LL LL, before
            // HH HH LL LL LL LL HH HH, after
            generateRegRegImmInstruction(TR::InstOpCode::SHLD4RegRegImm1, node,
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
      generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, scratchReg, targetRegister->getHighOrder(), cg);
      generateRegImmInstruction(TR::InstOpCode::TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), cg);
      generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node, targetRegister->getLowOrder(), scratchReg, cg);

      generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, scratchReg, targetRegister->getHighOrder(), cg);
      generateRegRegInstruction(TR::InstOpCode::SHLD4RegRegCL, node, targetRegister->getHighOrder(), targetRegister->getLowOrder(), shiftDependencies, cg);
      generateRegRegInstruction(TR::InstOpCode::SHLD4RegRegCL, node, targetRegister->getLowOrder(), scratchReg, shiftDependencies, cg);

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
         generateRegRegImmInstruction(TR::InstOpCode::SHRD4RegRegImm1, node,
                                      targetRegister->getLowOrder(),
                                      targetRegister->getHighOrder(),
                                      value, cg);
         generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, targetRegister->getHighOrder(), value, cg);
         }
      else
         {
         if (value != 32)
            {
            value -= 32;
            generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, targetRegister->getHighOrder(), value, cg);
            }
         generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), cg);
         generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, targetRegister->getHighOrder(), 31, cg);
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
      generateRegRegInstruction(TR::InstOpCode::SHRD4RegRegCL, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegInstruction(TR::InstOpCode::SAR4RegCL, node, targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, scratchReg, targetRegister->getHighOrder(), cg);
      generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, scratchReg, 31, cg);
      generateRegImmInstruction(TR::InstOpCode::TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), cg);
      generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node, targetRegister->getHighOrder(), scratchReg, cg);

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
         generateRegRegImmInstruction(TR::InstOpCode::SHRD4RegRegImm1, node,
                                      targetRegister->getLowOrder(),
                                      targetRegister->getHighOrder(),
                                      value, cg);
         generateRegImmInstruction(TR::InstOpCode::SHR4RegImm1, node, targetRegister->getHighOrder(), value, cg);
         }
      else
         {
         if (value != 32)
            {
            value -= 32;
            generateRegImmInstruction(TR::InstOpCode::SHR4RegImm1, node,
                                         targetRegister->getHighOrder(),
                                         value, cg);
            }
         TR::Register *tempLowReg = targetRegister->getLowOrder();
         TR::RegisterPair *targetAsPair = targetRegister->getRegisterPair();
         targetAsPair->setLowOrder(targetRegister->getHighOrder(), cg);
         generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, tempLowReg, tempLowReg, cg);
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

      generateRegRegInstruction(TR::InstOpCode::SHRD4RegRegCL, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegInstruction(TR::InstOpCode::SHR4RegCL, node, targetRegister->getHighOrder(), shiftDependencies, cg);
      generateRegImmInstruction(TR::InstOpCode::TEST1RegImm1, node, shiftAmountReg, 32, cg);
      generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node, targetRegister->getLowOrder(), targetRegister->getHighOrder(), cg);
      generateRegMemInstruction(TR::InstOpCode::CMOVNE4RegMem, node, targetRegister->getHighOrder(),
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
      TR::InstOpCode::Mnemonic  opCode;

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
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, lowReg, valueReg->getLowOrder(), cg);
            if (highValue != 0)
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, highReg, valueReg->getHighOrder(), cg);
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
            generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, tempReg, tempReg, cg);
            lowInstr = generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, lowMR, tempReg, cg);
            }
         else
            lowInstr = generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, lowReg, lowReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? TR::InstOpCode::AND4MemImms : TR::InstOpCode::AND4MemImm4;
            lowInstr = generateMemImmInstruction(opCode, node, lowMR, lowValue, cg);
            }
         else
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? TR::InstOpCode::AND4RegImms : TR::InstOpCode::AND4RegImm4;
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
               generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, tempReg, tempReg, cg);
               }
            highInstr = generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, highMR, tempReg, cg);
            }
         else
            highInstr = generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, highReg, highReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (highValue >= -128 && highValue <= 127) ? TR::InstOpCode::AND4MemImms : TR::InstOpCode::AND4MemImm4;
            highInstr = generateMemImmInstruction(opCode, node, highMR, highValue, cg);
            }
         else
            {
            opCode = (highValue >= -128 && highValue <= 127) ? TR::InstOpCode::AND4RegImms : TR::InstOpCode::AND4RegImm4;
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

      lowInstr = generateMemRegInstruction(TR::InstOpCode::AND4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      highInstr = generateMemRegInstruction(TR::InstOpCode::AND4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] and by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);

      temp.genericLongAnalyser(node,
                               TR::InstOpCode::AND4RegReg,
                               TR::InstOpCode::AND4RegReg,
                               TR::InstOpCode::AND4RegMem,
                               TR::InstOpCode::AND2RegMem,
                               TR::InstOpCode::AND1RegMem,
                               TR::InstOpCode::AND4RegMem,
                               TR::InstOpCode::MOV4RegReg);

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
      TR::InstOpCode::Mnemonic  opCode;
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
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, lowReg, valueReg->getLowOrder(), cg);
            if (highValue != -1)
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, highReg, valueReg->getHighOrder(), cg);
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
            opCode = (lowValue >= -128 && lowValue <= 127) ? TR::InstOpCode::OR4MemImms : TR::InstOpCode::OR4MemImm4;
            lowInstr = generateMemImmInstruction(opCode, node, lowMR, lowValue, cg);
            }
         else
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? TR::InstOpCode::OR4RegImms : TR::InstOpCode::OR4RegImm4;
            lowInstr = generateRegImmInstruction(opCode, node, lowReg, lowValue, cg);
            }
         }

      if (highValue == 0)
         ;
      else
         {
         if (isMemOp)
            {
            opCode = (highValue >= -128 && highValue <= 127) ? TR::InstOpCode::OR4MemImms : TR::InstOpCode::OR4MemImm4;
            highInstr = generateMemImmInstruction(opCode, node, highMR, highValue, cg);
            }
         else
            {
            opCode = (highValue >= -128 && highValue <= 127) ? TR::InstOpCode::OR4RegImms : TR::InstOpCode::OR4RegImm4;
            highInstr = generateRegImmInstruction(opCode, node, highReg, highValue, cg);
            }
         }

      if (debug("traceMemOp") && isMemOp)
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] or by const", node);
      }
   else if (isMemOp)
      {
      TR::Register *tempReg = cg->evaluate(secondChild);

      lowInstr = generateMemRegInstruction(TR::InstOpCode::OR4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      highInstr = generateMemRegInstruction(TR::InstOpCode::OR4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] or by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);

      temp.genericLongAnalyser(node,
                               TR::InstOpCode::OR4RegReg,
                               TR::InstOpCode::OR4RegReg,
                               TR::InstOpCode::OR4RegMem,
                               TR::InstOpCode::OR2RegMem,
                               TR::InstOpCode::OR1RegMem,
                               TR::InstOpCode::OR4RegMem,
                               TR::InstOpCode::MOV4RegReg);

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
      TR::InstOpCode::Mnemonic  opCode;
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
            lowInstr = generateMemInstruction(TR::InstOpCode::NOT4Mem, node, lowMR, cg);
         else
            lowInstr = generateRegInstruction(TR::InstOpCode::NOT4Reg, node, lowReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? TR::InstOpCode::XOR4MemImms : TR::InstOpCode::XOR4MemImm4;
            lowInstr = generateMemImmInstruction(opCode, node, lowMR, lowValue, cg);
            }
         else
            {
            opCode = (lowValue >= -128 && lowValue <= 127) ? TR::InstOpCode::XOR4RegImms : TR::InstOpCode::XOR4RegImm4;
            lowInstr = generateRegImmInstruction(opCode, node, lowReg, lowValue, cg);
            }
         }

      if (highValue == 0)
         ;
      else if (highValue == -1)
         {
         if (isMemOp)
            highInstr = generateMemInstruction(TR::InstOpCode::NOT4Mem, node, highMR, cg);
         else
            highInstr = generateRegInstruction(TR::InstOpCode::NOT4Reg, node, highReg, cg);
         }
      else
         {
         if (isMemOp)
            {
            opCode = (highValue >= -128 && highValue <= 127) ? TR::InstOpCode::XOR4MemImms : TR::InstOpCode::XOR4MemImm4;
            highInstr = generateMemImmInstruction(opCode, node, highMR, highValue, cg);
            }
         else
            {
            opCode = (highValue >= -128 && highValue <= 127) ? TR::InstOpCode::XOR4RegImms : TR::InstOpCode::XOR4RegImm4;
            highInstr = generateRegImmInstruction(opCode, node, highReg, highValue, cg);
            }
         }

      if (debug("traceMemOp") && isMemOp)
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] xor by const", node);
      }
   else if (isMemOp)
      {
      TR::Register *tempReg = cg->evaluate(secondChild);

      lowInstr = generateMemRegInstruction(TR::InstOpCode::XOR4MemReg, node, lowMR, tempReg->getLowOrder(), cg);
      highInstr = generateMemRegInstruction(TR::InstOpCode::XOR4MemReg, node, highMR, tempReg->getHighOrder(), cg);

      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] xor by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);

      temp.genericLongAnalyser(node,
                               TR::InstOpCode::XOR4RegReg,
                               TR::InstOpCode::XOR4RegReg,
                               TR::InstOpCode::XOR4RegMem,
                               TR::InstOpCode::XOR2RegMem,
                               TR::InstOpCode::XOR1RegMem,
                               TR::InstOpCode::XOR4RegMem,
                               TR::InstOpCode::MOV4RegReg);

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
      generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, targetRegister, tempMR, cg);
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
         generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, targetRegister, temp->getLowOrder(), cg);
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
            generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node,
                                         targetRegister,
                                         cmpRegister->getLowOrder(), cg);
            }
         generateRegRegInstruction(TR::InstOpCode::OR4RegReg, node,
                                      targetRegister,
                                      cmpRegister->getHighOrder(), cg);
         cg->stopUsingRegister(targetRegister);
         targetRegister = cg->allocateRegister();

         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

         generateRegInstruction(TR::InstOpCode::SETE1Reg, node, targetRegister, cg);
         }
      else
         {
         compareGPRegisterToConstantForEquality(node, lowValue, cmpRegister->getLowOrder(), cg);
         targetRegister = cg->allocateRegister();
         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

         generateRegInstruction(TR::InstOpCode::SETE1Reg, node, targetRegister, cg);

         compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
         TR::Register *highTargetRegister = cg->allocateRegister();
         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(highTargetRegister);

         generateRegInstruction(TR::InstOpCode::SETE1Reg, node, highTargetRegister, cg);

         generateRegRegInstruction(TR::InstOpCode::AND1RegReg, node, targetRegister, highTargetRegister, cg);
         cg->stopUsingRegister(highTargetRegister);
         }

      // Result of lcmpXX is an integer.
      //
      generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1,node,targetRegister,targetRegister,cg);
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      // need analyser-like stuff here for non-constant second children.
      TR_X86CompareAnalyser  temp(cg);
      targetRegister = temp.longEqualityBooleanAnalyser(node, TR::InstOpCode::SETE1Reg, TR::InstOpCode::AND1RegReg);
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
            generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, targetRegister, cmpRegister->getLowOrder(), cg);
            }
         generateRegRegInstruction(TR::InstOpCode::OR4RegReg, node, targetRegister, cmpRegister->getHighOrder(), cg);
         cg->stopUsingRegister(targetRegister);
         targetRegister = cg->allocateRegister();

         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

         generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, targetRegister, cg);
         }
      else
         {
         compareGPRegisterToConstantForEquality(node, lowValue, cmpRegister->getLowOrder(), cg);
         targetRegister = cg->allocateRegister();

         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, targetRegister, cg);

         compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
         TR::Register *highTargetRegister = cg->allocateRegister();
         if (cg->enableRegisterInterferences())
            cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(highTargetRegister);

         generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, highTargetRegister, cg);
         generateRegRegInstruction(TR::InstOpCode::OR1RegReg, node, targetRegister, highTargetRegister, cg);
         cg->stopUsingRegister(highTargetRegister);
         }

      // Result of lcmpXX is an integer.
      //
      generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1,node,targetRegister,targetRegister,cg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      targetRegister = temp.longEqualityBooleanAnalyser(node, TR::InstOpCode::SETNE1Reg, TR::InstOpCode::OR1RegReg);
      }
   return targetRegister;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETL1Reg, TR::InstOpCode::SETB1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETG1Reg, TR::InstOpCode::SETAE1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETG1Reg, TR::InstOpCode::SETA1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETL1Reg, TR::InstOpCode::SETBE1Reg, cg);
   }




TR::Register *OMR::X86::I386::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETB1Reg, TR::InstOpCode::SETB1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETA1Reg, TR::InstOpCode::SETAE1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETA1Reg, TR::InstOpCode::SETA1Reg, cg);
   }

TR::Register *OMR::X86::I386::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(node, TR::InstOpCode::SETB1Reg, TR::InstOpCode::SETBE1Reg, cg);
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
                                                                  TR::InstOpCode::JAE4,
                                                                  TR::InstOpCode::JGE4, cg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else if (firstChild->getOpCodeValue() == TR::lconst &&
            firstChild->getRegister() == NULL)
      {
      targetRegister = longArithmeticCompareRegisterWithImmediate(node, cg->evaluate(secondChild),
                                                                  firstChild,
                                                                  TR::InstOpCode::JBE4,
                                                                  TR::InstOpCode::JLE4, cg);
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
      generateInstruction(TR::InstOpCode::CDQAcc, node, cdqDependencies, cg);
      }
   else
      {
      generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, highReg, childReg, cg);
      generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, highReg, 31, cg);
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

   generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, highReg, highReg, cg);

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
      generateRegMemInstruction(TR::InstOpCode::MOVSXReg4Mem1, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg1, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, longReg->getHighOrder(), longReg->getLowOrder(), cg);
   generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, longReg->getHighOrder(), 8, cg);
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
      generateRegMemInstruction(TR::InstOpCode::MOVZXReg4Mem1, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg1, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, longReg->getHighOrder(), longReg->getHighOrder(), cg);
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
      generateRegMemInstruction(TR::InstOpCode::MOVSXReg4Mem2, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegRegInstruction(TR::InstOpCode::MOVSXReg4Reg2, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node,
                                longReg->getHighOrder(),
                                longReg->getLowOrder(), cg);
   generateRegImmInstruction(TR::InstOpCode::SAR4RegImm1, node, longReg->getHighOrder(), 16, cg);
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
      generateRegMemInstruction(TR::InstOpCode::MOVZXReg4Mem2, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg2, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);
      }

   generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, longReg->getHighOrder(),longReg->getHighOrder(), cg);
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
      generateRegMemInstruction(TR::InstOpCode::MOVZXReg4Mem2, node, longReg->getLowOrder(), tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      longReg = cg->allocateRegisterPair(cg->intClobberEvaluate(child), cg->allocateRegister());
      node->setRegister(longReg);
      generateRegRegInstruction(TR::InstOpCode::MOVZXReg4Reg2, node, longReg->getLowOrder(), longReg->getLowOrder(), cg);

      }

   generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, longReg->getHighOrder(), longReg->getHighOrder(), cg);
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

      TR::TreeEvaluator::lstoreEvaluator(node, cg); // The IA32 version, handles lstorei as well
      return NULL;
      }
   else
      {
      TR::MemoryReference  *storeLowMR = generateX86MemoryReference(node, cg);
      TR::Instruction *instr;

      if (valueChild->getOpCode().isLoadConst())
         {
         instr = generateMemImmInstruction(TR::InstOpCode::S4MemImm4, node, generateX86MemoryReference(*storeLowMR, 4, cg), valueChild->getLongIntHigh(), cg);
         generateMemImmInstruction(TR::InstOpCode::S4MemImm4, node, storeLowMR, valueChild->getLongIntLow(), cg);
         TR::Register *valueChildReg = valueChild->getRegister();
         if (valueChildReg && valueChildReg->getKind() == TR_X87 && valueChild->getReferenceCount() == 1)
           instr = generateFPSTiST0RegRegInstruction(TR::InstOpCode::DSTRegReg, valueChild, valueChildReg, valueChildReg, cg);
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
         generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, tempRegister, generateX86MemoryReference(*loadLowMR, 4, cg), cg);
         instr = generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, generateX86MemoryReference(*storeLowMR, 4, cg), tempRegister, cg);
         generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, tempRegister, loadLowMR, cg);
         generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, storeLowMR, tempRegister, cg);
         loadLowMR->decNodeReferenceCounts(cg);
         cg->stopUsingRegister(tempRegister);
         }
      else
         {
         TR::Register *sourceRegister = cg->evaluate(valueChild);
         if (sourceRegister->getKind() == TR_FPR)
            {
            TR_ASSERT(!sourceRegister->isSinglePrecision(), "dstore cannot have float operand\n");
            instr = generateMemRegInstruction(TR::InstOpCode::MOVSDMemReg, node, storeLowMR, sourceRegister, cg);
            }
         else
            {
            instr = generateFPMemRegInstruction(TR::InstOpCode::DSTMemReg, node, storeLowMR, sourceRegister, cg);
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
      generateFPRegMemInstruction(TR::InstOpCode::FLLDRegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::SymbolReference     *temp    = cg->allocateLocalTemp(TR::Int64);
      TR::Register            *longReg = cg->evaluate(child);
      TR::MemoryReference  *lowMR   = generateX86MemoryReference(temp, cg);
      generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, lowMR, longReg->getLowOrder(), cg);
      generateMemRegInstruction(TR::InstOpCode::S4MemReg, node,
                                generateX86MemoryReference(*lowMR, 4, cg),
                                longReg->getHighOrder(), cg);
      generateFPRegMemInstruction(TR::InstOpCode::FLLDRegMem, node,
                                  target,
                                  generateX86MemoryReference(*lowMR, 0, cg), cg);
      cg->decReferenceCount(child);
      }

   cg->stopUsingRegister(target);

   target = coerceST0ToFPR(node, TR::Float, cg);

   node->setRegister(target);

   return target;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *target = cg->allocateRegister(TR_X87);
   if (child->getRegister() == NULL && child->getReferenceCount() == 1 && child->getOpCode().isLoadVar())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
      generateFPRegMemInstruction(TR::InstOpCode::DLLDRegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::SymbolReference     *temp    = cg->allocateLocalTemp(TR::Int64);
      TR::Register            *longReg = cg->evaluate(child);
      TR::MemoryReference  *lowMR   = generateX86MemoryReference(temp, cg);
      generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, lowMR, longReg->getLowOrder(), cg);
      generateMemRegInstruction(TR::InstOpCode::S4MemReg, node,
                                generateX86MemoryReference(*lowMR, 4, cg),
                                longReg->getHighOrder(), cg);
      generateFPRegMemInstruction(TR::InstOpCode::DLLDRegMem, node,
                                  target,
                                  generateX86MemoryReference(*lowMR, 0, cg), cg);
      cg->decReferenceCount(child);
      }

   cg->stopUsingRegister(target);

   target = coerceST0ToFPR(node, TR::Double, cg);

   node->setRegister(target);

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

      if (performTransformation(comp, "O^O Using SSE for volatile load %s\n", cg->getDebug()->getName(node)))
         {
         TR_X86ProcessorInfo &p = cg->getX86ProcessorInfo();

         if (cg->comp()->target().cpu.isGenuineIntel())
            {
            TR::Register *xmmReg = cg->allocateRegister(TR_FPR);
            generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, xmmReg, sourceMR, cg);

            //allocate: lowRegister
            lowRegister = cg->allocateRegister();
            //allocate: highRegister
            highRegister = cg->allocateRegister();

            generateRegRegInstruction(TR::InstOpCode::MOVDReg4Reg, node, lowRegister, xmmReg, cg);
            generateRegImmInstruction(TR::InstOpCode::PSRLQRegImm1, node, xmmReg, 0x20, cg);
            generateRegRegInstruction(TR::InstOpCode::MOVDReg4Reg, node, highRegister, xmmReg, cg);

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
            generateMemRegInstruction(TR::InstOpCode::MOVSDMemReg, node, stack, reg, cg);
            //stop using: xmm
            cg->stopUsingRegister(reg);

            //allocate: lowRegister
            lowRegister = cg->allocateRegister();
            //allocate: highRegister
            highRegister = cg->allocateRegister();

            //generate: lowRegister <- stack1
            generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, lowRegister, stackLow, cg);
            //generate: highRegister <- stack1_plus4
            generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, highRegister, stackHigh, cg);
            }
         }
      else
         {
         TR_ASSERT_FATAL(cg->comp()->compileRelocatableCode() || cg->comp()->compilePortableCode() || cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_CX8), "Assumption of support of the CMPXCHG8B instruction failed in performLload()" );

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

         generateRegRegInstruction (TR::InstOpCode::MOV4RegReg, node, ecxReg, highRegister, cg);
         generateRegRegInstruction (TR::InstOpCode::MOV4RegReg, node, ebxReg, lowRegister, cg);
         generateMemInstruction ( cg->comp()->target().isSMP() ? TR::InstOpCode::LCMPXCHG8BMem : TR::InstOpCode::CMPXCHG8BMem, node, sourceMR, deps, cg);

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

// also handles lloadi
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
      generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, tempMR, longReg->getLowOrder(), cg);
      generateMemRegInstruction(TR::InstOpCode::S4MemReg, node, generateX86MemoryReference(*tempMR, 4, cg), longReg->getHighOrder(), cg);

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

      generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, lowReg, tempMR, cg);
      generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, highReg, generateX86MemoryReference(*tempMR, 4, cg), cg);

      if (child->getReferenceCount() > 1)
         performDload(child, generateX86MemoryReference(*tempMR, 0, cg), cg);

      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *doubleReg = cg->evaluate(child);

      tempMR = cg->machine()->getDummyLocalMR(TR::Double);
      if (doubleReg->getKind() == TR_FPR)
         generateMemRegInstruction(TR::InstOpCode::MOVSDMemReg, node, tempMR, doubleReg, cg);
      else
         generateFPMemRegInstruction(TR::InstOpCode::DSTMemReg, node, tempMR, doubleReg, cg);

      generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, lowReg, generateX86MemoryReference(*tempMR, 0, cg), cg);
      generateRegMemInstruction(TR::InstOpCode::L4RegMem, node, highReg, generateX86MemoryReference(*tempMR, 4, cg), cg);
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
      generateLabelInstruction(TR::InstOpCode::label, node, lab0, cg);
      generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, highReg, 0x7FF00000, cg);
      generateLabelInstruction(TR::InstOpCode::JG4, node, lab1, cg);
      generateLabelInstruction(TR::InstOpCode::JE4, node, lab3, cg);
      generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, highReg, 0xFFF00000, cg);
      generateLabelInstruction(TR::InstOpCode::JA4, node, lab1, cg);
      generateLabelInstruction(TR::InstOpCode::JB4, node, lab2, cg);
      generateLabelInstruction(TR::InstOpCode::label, node, lab3, cg);
      generateRegRegInstruction(TR::InstOpCode::TEST4RegReg, node, lowReg, lowReg, cg);
      generateLabelInstruction(TR::InstOpCode::JE4, node, lab2, cg);
      generateLabelInstruction(TR::InstOpCode::label, node, lab1, cg);
      generateRegImmInstruction(TR::InstOpCode::MOV4RegImm4, node, highReg, 0x7FF80000, cg);
      generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, lowReg, lowReg, cg);
      }

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)2, cg);
   deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
   generateLabelInstruction(TR::InstOpCode::label, node, lab2, deps, cg);

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
               generateRegMemInstruction(TR::InstOpCode::L4RegMem,
                                         landFirstChild,
                                         targetRegister,
                                         tempMR, cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            else
               {
               targetRegister = cg->evaluate(landFirstChild)->getHighOrder();
               }

            generateRegRegInstruction(TR::InstOpCode::TEST4RegReg, node, targetRegister, targetRegister, cg);
            cg->decReferenceCount(landFirstChild);
            }
         else
            {
            cmpRegister = cg->evaluate(firstChild);
            targetRegister = cmpRegister->getLowOrder();
            if (firstChild->getReferenceCount() != 1)
               {
               targetRegister = cg->allocateRegister();
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, targetRegister, cmpRegister->getLowOrder(), cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            generateRegRegInstruction(TR::InstOpCode::OR4RegReg, node, targetRegister, cmpRegister->getHighOrder(), cg);
            }

         generateConditionalJumpInstruction(TR::InstOpCode::JE4, node, cg);

         if (targetNeedsToBeExplicitlyStopped)
            {
            cg->stopUsingRegister(targetRegister);
            }
         }
      else
         {
         TR::LabelSymbol     *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol     *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         startLabel->setStartInternalControlFlow();
         doneLabel->setEndInternalControlFlow();

         cmpRegister = cg->evaluate(firstChild);
         generateLabelInstruction(TR::InstOpCode::label, node, startLabel, cg);
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
            deps = generateRegisterDependencyConditions(third, cg, 2);
            deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
            deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            deps->stopAddingConditions();
            generateLabelInstruction(TR::InstOpCode::JNE4, node, doneLabel, deps, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(TR::InstOpCode::JE4, node, destinationLabel, deps, cg);
            cg->decReferenceCount(third);
            }
         else
            {
            generateLabelInstruction(TR::InstOpCode::JNE4, node, doneLabel, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(TR::InstOpCode::JE4, node, destinationLabel, cg);
            deps = generateRegisterDependencyConditions((uint8_t)0, 2, cg);
            deps->addPostCondition(cmpRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
            deps->addPostCondition(cmpRegister->getHighOrder(), TR::RealRegister::NoReg, cg);
            }

         generateLabelInstruction(TR::InstOpCode::label, node, doneLabel, deps, cg);
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.longEqualityCompareAndBranchAnalyser(node, NULL, destinationLabel, TR::InstOpCode::JE4);
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
               generateRegMemInstruction(TR::InstOpCode::L4RegMem,
                                         landFirstChild,
                                         targetRegister,
                                         tempMR, cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            else
               {
               targetRegister = cg->evaluate(landFirstChild)->getHighOrder();
               }
            generateRegRegInstruction(TR::InstOpCode::TEST4RegReg, node, targetRegister,targetRegister, cg);
            cg->decReferenceCount(landFirstChild);
            }
         else
            {
            cmpRegister = cg->evaluate(firstChild);
            targetRegister = cmpRegister->getLowOrder();
            if (firstChild->getReferenceCount() != 1)
               {
               targetRegister = cg->allocateRegister();
               generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, node, targetRegister, cmpRegister->getLowOrder(), cg);
               targetNeedsToBeExplicitlyStopped = true;
               }
            generateRegRegInstruction(TR::InstOpCode::OR4RegReg, node, targetRegister, cmpRegister->getHighOrder(), cg);
            }

         generateConditionalJumpInstruction(TR::InstOpCode::JNE4, node, cg);

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
            deps = generateRegisterDependencyConditions(third, cg, 1);
            deps->stopAddingConditions();
            generateLabelInstruction(TR::InstOpCode::JNE4, node, destinationLabel, deps, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(TR::InstOpCode::JNE4, node, destinationLabel, deps, cg);
            cg->decReferenceCount(third);
            }
         else
            {
            generateLabelInstruction(TR::InstOpCode::JNE4, node, destinationLabel, cg);
            compareGPRegisterToConstantForEquality(node, highValue, cmpRegister->getHighOrder(), cg);
            generateLabelInstruction(TR::InstOpCode::JNE4, node, destinationLabel, cg);
            }
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.longEqualityCompareAndBranchAnalyser(node, destinationLabel, destinationLabel, TR::InstOpCode::JNE4);
      }
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (generateLAddOrSubForOverflowCheck(node, cg))
      {
      generateConditionalJumpInstruction(TR::InstOpCode::JO4, node, cg);
      }
   else
      {
      TR::InstOpCode::Mnemonic compareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JB4 : TR::InstOpCode::JL4;
      TR::InstOpCode::Mnemonic reverseCompareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JA4 : TR::InstOpCode::JG4;
      compareLongsForOrder(node, compareOp, reverseCompareOp, TR::InstOpCode::JB4, cg);
      }
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (generateLAddOrSubForOverflowCheck(node, cg))
      {
      generateConditionalJumpInstruction(TR::InstOpCode::JNO4, node, cg);
      }
   else
      {
      TR::InstOpCode::Mnemonic compareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JA4 : TR::InstOpCode::JG4;
      TR::InstOpCode::Mnemonic reverseCompareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JB4 : TR::InstOpCode::JL4;
      compareLongsForOrder(node, compareOp, reverseCompareOp, TR::InstOpCode::JAE4, cg);
      }
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic compareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JA4 : TR::InstOpCode::JG4;
   TR::InstOpCode::Mnemonic reverseCompareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JB4 : TR::InstOpCode::JL4;
   compareLongsForOrder(node, compareOp, reverseCompareOp, TR::InstOpCode::JA4, cg);
   return NULL;
   }

TR::Register *OMR::X86::I386::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic compareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JB4 : TR::InstOpCode::JL4;
   TR::InstOpCode::Mnemonic reverseCompareOp = node->getOpCode().isUnsigned() ? TR::InstOpCode::JA4 : TR::InstOpCode::JG4;
   compareLongsForOrder(node, compareOp, reverseCompareOp, TR::InstOpCode::JBE4, cg);
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
         generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node,
                                   trueReg-> getRegisterPair()->getLowOrder(),
                                   falseReg->getRegisterPair()->getLowOrder(), cg);
         generateRegRegInstruction(TR::InstOpCode::CMOVNE4RegReg, node,
                                   trueReg-> getRegisterPair()->getHighOrder(),
                                   falseReg->getRegisterPair()->getHighOrder(), cg);
         }
      else
         {
         generateRegRegInstruction(TR::InstOpCode::CMOVE4RegReg, node,
                                   trueReg-> getRegisterPair()->getLowOrder(),
                                   falseReg->getRegisterPair()->getLowOrder(), cg);
         generateRegRegInstruction(TR::InstOpCode::CMOVE4RegReg, node,
                                   trueReg-> getRegisterPair()->getHighOrder(),
                                   falseReg->getRegisterPair()->getHighOrder(), cg);
         }
      }
   else if (!longCompare && condOp.isCompareForOrder() && condition->getFirstChild()->getOpCode().isIntegerOrAddress())
      {
      compareIntegersForOrder(condition, cg);
      generateRegRegInstruction((condOp.isCompareTrueIfEqual()) ?
                         ((condOp.isCompareTrueIfGreater()) ? TR::InstOpCode::CMOVL4RegReg : TR::InstOpCode::CMOVG4RegReg) :
                         ((condOp.isCompareTrueIfGreater()) ? TR::InstOpCode::CMOVLE4RegReg : TR::InstOpCode::CMOVGE4RegReg), node, trueReg->getRegisterPair()->getLowOrder(), falseReg->getRegisterPair()->getLowOrder(), cg);
      generateRegRegInstruction((condOp.isCompareTrueIfEqual()) ?
                         ((condOp.isCompareTrueIfGreater()) ? TR::InstOpCode::CMOVL4RegReg : TR::InstOpCode::CMOVG4RegReg) :
                         ((condOp.isCompareTrueIfGreater()) ? TR::InstOpCode::CMOVLE4RegReg : TR::InstOpCode::CMOVGE4RegReg), node, trueReg->getRegisterPair()->getHighOrder(), falseReg->getRegisterPair()->getHighOrder(), cg);
      }
   else
      {
      TR::Register *condReg  = cg->evaluate(condition);
      generateRegRegInstruction(TR::InstOpCode::TEST4RegReg, node, condReg, condReg, cg); // the condition is an int
      generateRegRegInstruction(TR::InstOpCode::CMOVE4RegReg, node,
                                trueReg-> getRegisterPair()->getLowOrder(),
                                falseReg->getRegisterPair()->getLowOrder(), cg);
      generateRegRegInstruction(TR::InstOpCode::CMOVE4RegReg, node,
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
   TR::Node *child = node->getFirstChild();
   TR::Register *target     = cg->longClobberEvaluate(child);
   TR::RegisterPair *pair   = target->getRegisterPair();
   TR::Register *lo   = pair->getLowOrder();
   TR::Register *hi   = pair->getHighOrder();

   generateRegInstruction(TR::InstOpCode::BSWAP4Reg, node, lo, cg);
   generateRegInstruction(TR::InstOpCode::BSWAP4Reg, node, hi, cg);
   pair->setLowOrder(hi, cg);
   pair->setHighOrder(lo, cg);

   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }

TR::Register*
OMR::X86::I386::TreeEvaluator::integerPairMinMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic SETccHi = TR::InstOpCode::bad;
   TR::InstOpCode::Mnemonic SETccLo = TR::InstOpCode::bad;
   switch (node->getOpCodeValue())
      {
      case TR::lmin:
         SETccHi = TR::InstOpCode::SETL1Reg;
         SETccLo = TR::InstOpCode::SETB1Reg;
         break;
      case TR::lmax:
         SETccHi = TR::InstOpCode::SETG1Reg;
         SETccLo = TR::InstOpCode::SETA1Reg;
         break;
      default:
         TR_ASSERT(false, "INCORRECT IL OPCODE.");
         break;
      }
   auto operand0 = cg->evaluate(node->getChild(0));
   auto operand1 = cg->evaluate(node->getChild(1));
   auto result = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());

   generateRegRegInstruction(TR::InstOpCode::CMP4RegReg, node, operand0->getLowOrder(), operand1->getLowOrder(), cg);
   generateRegInstruction(SETccLo, node, result->getLowOrder(), cg); // t1 = (low0 < low1)
   generateRegRegInstruction(TR::InstOpCode::CMP4RegReg, node, operand0->getHighOrder(), operand1->getHighOrder(), cg);
   generateRegInstruction(SETccHi, node, result->getHighOrder(), cg); // t2 = (high0 < high1)
   generateRegRegInstruction(TR::InstOpCode::CMOVE4RegReg, node, result->getHighOrder(), result->getLowOrder(), cg); // if (high0 == high1) then t2 = t1 = (low0 < low1)

   generateRegRegInstruction(TR::InstOpCode::TEST1RegReg,  node, result->getHighOrder(), result->getHighOrder(),  cg);
   generateRegRegInstruction(TR::InstOpCode::MOV4RegReg,   node, result->getLowOrder(),  operand0->getLowOrder(),  cg);
   generateRegRegInstruction(TR::InstOpCode::MOV4RegReg,   node, result->getHighOrder(), operand0->getHighOrder(), cg);
   generateRegRegInstruction(TR::InstOpCode::CMOVE4RegReg, node, result->getLowOrder(),  operand1->getLowOrder(),  cg);
   generateRegRegInstruction(TR::InstOpCode::CMOVE4RegReg, node, result->getHighOrder(), operand1->getHighOrder(), cg);

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
   generateRegRegInstruction(TR::InstOpCode::XOR4RegReg, node, resultReg, resultReg, cg);

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
   generateMemInstruction(cg->comp()->target().isSMP() ? TR::InstOpCode::LCMPXCHG8BMem : TR::InstOpCode::CMPXCHG8BMem, node, memRef, deps, cg);

   cg->stopUsingRegister(compareReg);

   generateRegInstruction(TR::InstOpCode::SETNE1Reg, node, resultReg, cg);

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
