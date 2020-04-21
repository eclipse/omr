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

#include <stddef.h>
#include <stdint.h>
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
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
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#endif
#include "env/ObjectModel.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCAOTRelocation.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "p/codegen/PPCOutOfLineCodeSection.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/Runtime.hpp"

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg);
static void switchDispatch(TR::Node *node, bool fromTableEval, TR::CodeGenerator *cg);
static bool isGlDepsUnBalanced(TR::Node *node, TR::CodeGenerator *cg);
static void lookupScheme1(TR::Node *node, bool unbalanced, bool fromTableEval, TR::CodeGenerator *cg);
static void lookupScheme2(TR::Node *node, bool unbalanced, bool fromTableEval, TR::CodeGenerator *cg);
static void lookupScheme3(TR::Node *node, bool unbalanced, TR::CodeGenerator *cg);
static void lookupScheme4(TR::Node *node, TR::CodeGenerator *cg);

TR::Register *OMR::Power::TreeEvaluator::ifacmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::iflucmpltEvaluator(node, cg);
   else
      return TR::TreeEvaluator::ifiucmpltEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::ifacmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::iflucmpgeEvaluator(node, cg);
   else
      return TR::TreeEvaluator::ifiucmpgeEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::ifacmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::iflucmpgtEvaluator(node, cg);
   else
      return TR::TreeEvaluator::ifiucmpgtEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::ifacmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::iflucmpleEvaluator(node, cg);
   else
      return TR::TreeEvaluator::ifiucmpleEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::lucmpltEvaluator(node, cg);
   else
      return TR::TreeEvaluator::iucmpltEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::lucmpgeEvaluator(node, cg);
   else
      return TR::TreeEvaluator::iucmpgeEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::lucmpgtEvaluator(node, cg);
   else
      return TR::TreeEvaluator::iucmpgtEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::lucmpleEvaluator(node, cg);
   else
      return TR::TreeEvaluator::iucmpleEvaluator(node, cg);
   }


TR::Register *OMR::Power::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::LabelSymbol *dstLabel, TR::Node *node, TR::CodeGenerator *cg, bool isSigned, bool isHint, bool likeliness)
   {
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::Node     *secondChild = node->getSecondChild();
   TR::Node     *firstChild = node->getFirstChild();

   TR::Register *src1Reg   = cg->evaluate(firstChild);
   if (secondChild->getOpCode().isLoadConst())
      {
      TR::InstOpCode::Mnemonic cmpOp;
      if (isSigned)
         {
         int64_t value = secondChild->get64bitIntegralValue();
         if (value >= LOWER_IMMED && value <= UPPER_IMMED)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, src1Reg, value);
            }
         else
            {
            TR::Register *src2Reg = cg->evaluate(secondChild);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, condReg, src1Reg, src2Reg);
            }
         }
      else
         {
         uint64_t value = (uint64_t) secondChild->get64bitIntegralValue();

         TR::Register *tReg = NULL;
         bool newReg = false;
         if (node->getOpCodeValue() == TR::ifbucmplt || node->getOpCodeValue() == TR::ifbucmple || node->getOpCodeValue() == TR::ifbucmpgt || node->getOpCodeValue() == TR::ifbucmpge)
            {
            tReg = cg->allocateRegister();
            newReg = true;
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tReg, src1Reg, 0, 0xff);
            value &= (uint64_t)0xff;
            }
         else if (node->getOpCodeValue() == TR::ifsucmplt || node->getOpCodeValue() == TR::ifsucmple || node->getOpCodeValue() == TR::ifsucmpgt || node->getOpCodeValue() == TR::ifsucmpge)
            {
            tReg = cg->allocateRegister();
            newReg = true;
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tReg, src1Reg, 0, 0xffff);
            value &= (uint64_t)0xffff;
            }
         else
            tReg = src1Reg;

         if (value <= (uint64_t)0xFFFF)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, tReg, value);
         else
            {
            TR::Register *secondReg = NULL;
            bool sNewReg = false;
            if (node->getOpCodeValue() == TR::ifbucmplt || node->getOpCodeValue() == TR::ifbucmple || node->getOpCodeValue() == TR::ifbucmpgt || node->getOpCodeValue() == TR::ifbucmpge)
               {
               secondReg = cg->gprClobberEvaluate(secondChild);
               sNewReg = true;
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, secondReg, secondReg, 0, 0xff);
               }
            else if (node->getOpCodeValue() == TR::ifsucmplt || node->getOpCodeValue() == TR::ifsucmple || node->getOpCodeValue() == TR::ifsucmpgt || node->getOpCodeValue() == TR::ifsucmpge)
               {
               secondReg = cg->gprClobberEvaluate(secondChild);
               sNewReg = true;
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, secondReg, secondReg, 0, 0xffff);
               }
            else
               secondReg = cg->evaluate(secondChild);

            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, tReg, secondReg);

            if (sNewReg)
               cg->stopUsingRegister(secondReg);
            }

         if (newReg)
            cg->stopUsingRegister(tReg);
         }
      }
   else
      {
      TR::Register *tReg = NULL;
      bool newReg = false;
      TR::Register *src2Reg = NULL;

      if (node->getOpCodeValue() == TR::ifbucmplt || node->getOpCodeValue() == TR::ifbucmple || node->getOpCodeValue() == TR::ifbucmpgt || node->getOpCodeValue() == TR::ifbucmpge)
         {
         tReg = cg->allocateRegister();
         newReg = true;
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tReg, src1Reg, 0, 0xff);
         src2Reg = cg->gprClobberEvaluate(secondChild);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, src2Reg, src2Reg, 0, 0xff);
         }
      else if (node->getOpCodeValue() == TR::ifsucmplt || node->getOpCodeValue() == TR::ifsucmple || node->getOpCodeValue() == TR::ifsucmpgt || node->getOpCodeValue() == TR::ifsucmpge)
         {
         tReg = cg->allocateRegister();
         newReg = true;
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tReg, src1Reg, 0, 0xffff);
         src2Reg = cg->gprClobberEvaluate(secondChild);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, src2Reg, src2Reg, 0, 0xffff);
         }
      else
         {
         tReg = src1Reg;
         src2Reg = cg->evaluate(secondChild);
         }

      if (isSigned)
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, condReg, tReg, src2Reg);
      else
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, tReg, src2Reg);

      if (newReg)
         {
         cg->stopUsingRegister(src2Reg);
         cg->stopUsingRegister(tReg);
         }
      }

   if (node->getOpCode().isIf() && node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");
      cg->evaluate(thirdChild);
      if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
         generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
               generateRegisterDependencyConditions(cg, thirdChild, 0));
      else
         generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
               generateRegisterDependencyConditions(cg, thirdChild, 0));
      cg->decReferenceCount(thirdChild);
      }
   else
      {
      if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
         generateConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg);
      else
         generateConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg);
      }

   cg->stopUsingRegister(condReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::Node *node, TR::CodeGenerator *cg, bool isSigned)
   {
   return TR::TreeEvaluator::compareIntsForOrder(branchOp, node->getBranchDestination()->getNode()->getLabel(), node, cg, isSigned, false, false);
   }

static void fixDepsForLongCompare(TR::RegisterDependencyConditions *deps, TR::Register *src1High, TR::Register *src1Low, TR::Register *src2High, TR::Register *src2Low)
   {
   if (deps == NULL) return;
   if (src1High != NULL && !deps->usesRegister(src1High))
      {
      deps->addPostCondition(src1High, TR::RealRegister::NoReg);
      deps->addPreCondition(src1High, TR::RealRegister::NoReg);
      }
   if (src1Low != NULL && !deps->usesRegister(src1Low))
      {
      deps->addPostCondition(src1Low, TR::RealRegister::NoReg);
      deps->addPreCondition(src1Low, TR::RealRegister::NoReg);
      }
   if (src2High != NULL && !deps->usesRegister(src2High))
      {
      deps->addPostCondition(src2High, TR::RealRegister::NoReg);
      deps->addPreCondition(src2High, TR::RealRegister::NoReg);
      }
   if (src2Low != NULL && !deps->usesRegister(src2Low))
      {
      deps->addPostCondition(src2Low, TR::RealRegister::NoReg);
      deps->addPreCondition(src2Low, TR::RealRegister::NoReg);
      }
   }

static TR::Register *compareLongsForOrderWithAnalyser(TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::Mnemonic reversedBranchOp, TR::Register *condReg, TR::LabelSymbol *destinationLabel,
                                                     TR::RegisterDependencyConditions *deps, TR::Node *node, TR::CodeGenerator *cg, bool isHint, bool likeliness)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild;
   TR::Register *src1Reg = firstChild->getRegister();
   TR::Register *src2Reg = secondChild->getRegister();
   TR::LabelSymbol *label1   = generateLabelSymbol(cg);

   bool firstHighZero = false;
   bool secondHighZero = false;
   bool firstUseHighOrder = false;
   bool secondUseHighOrder = false;

   if (firstChild->isHighWordZero())
      {
      firstHighZero = true;
      TR::ILOpCodes firstOp = firstChild->getOpCodeValue();
      if (firstChild->getReferenceCount() == 1 && src1Reg == NULL)
         {
	 if (firstOp == TR::iu2l || firstOp == TR::su2l ||
	     (firstOp == TR::lushr &&
              firstChild->getSecondChild()->getOpCode().isLoadConst() &&
              (firstChild->getSecondChild()->getInt() & LONG_SHIFT_MASK) == 32))
	    {
	    firstChild = firstChild->getFirstChild();
	    if (firstOp == TR::lushr)
	       {
	       firstUseHighOrder = true;
	       }
	    }
         }
      }

   if (secondChild->isHighWordZero())
      {
      secondHighZero = true;
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if (secondChild->getReferenceCount() == 1 && src2Reg == NULL)
         {
	 if (secondOp == TR::iu2l || secondOp == TR::su2l ||
	     (secondOp == TR::lushr &&
              secondChild->getSecondChild()->getOpCode().isLoadConst() &&
              (secondChild->getSecondChild()->getInt() & LONG_SHIFT_MASK) == 32))
	    {
	    secondChild = secondChild->getFirstChild();
	    if (secondOp == TR::lushr)
	       {
	       secondUseHighOrder = true;
	       }
	    }
         }
      }

   src1Reg = cg->evaluate(firstChild);
   src2Reg = cg->evaluate(secondChild);

   if (node->getNumChildren() == 3)
         {
         thirdChild = node->getChild(2);
         TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");
         cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(cg, thirdChild, 4);
         cg->decReferenceCount(thirdChild);
         }

   TR::Register *src1Low = NULL;
   TR::Register *src1High = NULL;
   TR::Register *src2Low = NULL;
   TR::Register *src2High = NULL;

   if (!firstHighZero)
      {
      src1Low = src1Reg->getLowOrder();
      src1High = src1Reg->getHighOrder();
      }
   else
      {
      if (src1Reg->getRegisterPair())
         {
         if (firstUseHighOrder)
            {
            src1Low = src1Reg->getHighOrder();
            }
         else
            {
            src1Low = src1Reg->getLowOrder();
            }
         }
      else
         src1Low = src1Reg;

      src1High = 0;
      }

   if (!secondHighZero)
      {
      src2Low = src2Reg->getLowOrder();
      src2High = src2Reg->getHighOrder();
      }
   else
      {
      if (src2Reg->getRegisterPair())
         {
         if (secondUseHighOrder)
            {
            src2Low = src2Reg->getHighOrder();
            }
	 else
            {
            src2Low = src2Reg->getLowOrder();
            }
         }
      else
         src2Low = src2Reg;

      src2High = 0;
      }

   if (firstHighZero && secondHighZero)
      {
      // signed and unsigned are the same case
      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, src1Low, src2Low);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label1);
      if (deps)
         {
         if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
            generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, destinationLabel, condReg, deps);
         else
            generateDepConditionalBranchInstruction(cg, branchOp, node, destinationLabel, condReg, deps);
         }
      else
         {
         if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
            generateConditionalBranchInstruction(cg, branchOp, likeliness, node, destinationLabel, condReg);
         else
            generateConditionalBranchInstruction(cg, branchOp, node, destinationLabel, condReg);
         }
      }
   else
      {
      if (deps == NULL)
         deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
      fixDepsForLongCompare(deps, src1High, src1Low, src2High, src2Low);
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
                                            generateControlFlowInstruction(cg, TR::InstOpCode::iflong, node, deps);
      TR::InstOpCode::Mnemonic finalBranchOp = branchOp;
      cfop->addTargetRegister(condReg);
      if (firstHighZero)
         {
         cfop->addSourceRegister(src2High);
         cfop->addSourceRegister(src2Low);
         cfop->addSourceImmediate(0);
         cfop->addSourceRegister(src1Low);
         finalBranchOp = reversedBranchOp;
         }
      else
         {
         cfop->addSourceRegister(src1High);
         cfop->addSourceRegister(src1Low);
         cfop->addSourceImmediate(0);
         cfop->addSourceRegister(src2Low);
         }

      cfop->setLabelSymbol(destinationLabel);
      cfop->setOpCode2Value(finalBranchOp);
      }

   cg->stopUsingRegister(condReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::Mnemonic reversedBranchOp, TR::LabelSymbol *dstLabel,
                                               TR::Node *node, TR::CodeGenerator *cg, bool isSigned, bool isHint, bool likeliness)
   {
   TR::Node       *firstChild  = node->getFirstChild();
   TR::Register   *src1Reg;
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::Node        *thirdChild;
   int64_t value;

   if (secondChild->getOpCode().isLoadConst())
      {
      value = secondChild->get64bitIntegralValue();
      }
   if (cg->comp()->target().is64Bit())
      {
      src1Reg = cg->evaluate(firstChild);
      if (secondChild->getOpCode().isLoadConst() &&
          secondChild->getRegister() == NULL &&
          ((!isSigned && ((uint64_t)value <= 0xffff)) ||
          (isSigned && (value >= LOWER_IMMED && value <= UPPER_IMMED))))
         {
         if (isSigned)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi8, node, condReg, src1Reg, value);
         else
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, src1Reg, value);
         }
      else
         {
         if (isSigned)
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, condReg, src1Reg, cg->evaluate(secondChild));
         else
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl8, node, condReg, src1Reg, cg->evaluate(secondChild));
         }
      if (node->getOpCode().isIf() && node->getNumChildren() == 3)
         {
         TR::Node *thirdChild = node->getChild(2);
         TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");
         cg->evaluate(thirdChild);
         if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
            generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         else
            generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         cg->decReferenceCount(thirdChild);
         }
      else
         {
         if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
            generateConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg);
         else
            generateConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg);
         }
      cg->stopUsingRegister(condReg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return NULL;
      }
   else // 32 bit target
      {
      TR::RegisterDependencyConditions *deps = NULL;

      if ((firstChild->isHighWordZero() || secondChild->isHighWordZero()) &&
	  !(secondChild->getOpCode().isLoadConst() &&
           secondChild->getRegister() == NULL))
	 {
	 return compareLongsForOrderWithAnalyser(branchOp, reversedBranchOp, condReg, dstLabel, deps, node, cg, isHint, likeliness);
	 }

      TR::Register *src2Reg;
      src1Reg = cg->evaluate(firstChild);
      bool useImmed = secondChild->getOpCode().isLoadConst() &&
          secondChild->getRegister() == NULL &&
          value >= 0 && value <= 0xffff;

      if (!useImmed)
            src2Reg = cg->evaluate(secondChild);

      if (node->getOpCode().isIf() && node->getNumChildren() == 3)
         {
         thirdChild = node->getChild(2);
         TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");
         cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(cg, thirdChild, 4);
         cg->decReferenceCount(thirdChild);
         }

      if (deps == NULL)
	    deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());

      fixDepsForLongCompare(deps, src1Reg->getHighOrder(), src1Reg->getLowOrder(), useImmed ? NULL : src2Reg->getHighOrder(), useImmed ? NULL : src2Reg->getLowOrder());
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
      generateControlFlowInstruction(cg, TR::InstOpCode::iflong, node, deps);
      cfop->addTargetRegister(condReg);
      cfop->addSourceRegister(src1Reg->getHighOrder());
      cfop->addSourceRegister(src1Reg->getLowOrder());

      if (useImmed)
         {
         cfop->addSourceImmediate(secondChild->getLongIntHigh());
         cfop->addSourceImmediate(secondChild->getLongIntLow());
         }
      else
         {
         cfop->addSourceRegister(src2Reg->getHighOrder());
         cfop->addSourceRegister(src2Reg->getLowOrder());
         }
      cfop->setLabelSymbol(dstLabel);
      cfop->setOpCode2Value(branchOp);
      cg->stopUsingRegister(condReg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return NULL;
      }
   }

TR::Register *OMR::Power::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::Mnemonic reversedBranchOp, TR::Node *node, TR::CodeGenerator *cg, bool isSigned)
   {
   return TR::TreeEvaluator::compareLongsForOrder(branchOp, reversedBranchOp, node->getBranchDestination()->getNode()->getLabel(), node, cg, isSigned, false, false);
   }

TR::Register *compareLongAndSetOrderedBoolean(TR::InstOpCode::Mnemonic compareOp, TR::InstOpCode::Mnemonic branchOp, TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild       = node->getFirstChild();
   TR::Register    *src1Reg          = cg->evaluate(firstChild);
   TR::Node        *secondChild      = node->getSecondChild();
   TR::Register    *condReg          = cg->allocateRegister(TR_CCR);
   TR::Register    *trgReg           = cg->allocateRegister();
   TR::LabelSymbol *label1           = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel        = generateLabelSymbol(cg);

   TR::Register   *src2Reg = cg->evaluate(secondChild);
   TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
   generateControlFlowInstruction(cg, TR::InstOpCode::setblong, node);
   cfop->addTargetRegister(condReg);
   cfop->addTargetRegister(trgReg);
   cfop->addSourceRegister(src1Reg->getHighOrder());
   cfop->addSourceRegister(src1Reg->getLowOrder());
   cfop->addSourceRegister(src2Reg->getHighOrder());
   cfop->addSourceRegister(src2Reg->getLowOrder());
   cfop->setOpCode2Value(compareOp);
   cfop->setOpCode3Value(branchOp);
   cg->stopUsingRegister(condReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::LabelSymbol *temp = node->getBranchDestination()->getNode()->getLabel();
   if (node->getNumChildren() > 0)
      {
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      generateDepLabelInstruction(cg, TR::InstOpCode::b, node, temp,
            generateRegisterDependencyConditions(cg, child, 0));
      cg->decReferenceCount(child);
      }
   else
      {
      generateLabelInstruction(cg, TR::InstOpCode::b, node, temp);
      }
   return NULL;
   }

// also handles areturn
TR::Register *OMR::Power::TreeEvaluator::ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());
   const TR::PPCLinkageProperties &linkageProperties = cg->getProperties();
   TR::RealRegister::RegNum machineReturnRegister =
                linkageProperties.getIntegerReturnRegister();
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 0, cg->trMemory());
   dependencies->addPreCondition(returnRegister, machineReturnRegister);
   generateAdminInstruction(cg, TR::InstOpCode::ret, node);
   generateDepInstruction(cg, TR::InstOpCode::blr, node, dependencies);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }
 
TR::Register *OMR::Power::TreeEvaluator::lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());
   const TR::PPCLinkageProperties &linkageProperties = cg->getProperties();
   TR::RegisterDependencyConditions *dependencies;
   if (cg->comp()->target().is64Bit())
      {
      TR::RealRegister::RegNum machineReturnRegister =
                   linkageProperties.getIntegerReturnRegister();
      dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 0, cg->trMemory());
      dependencies->addPreCondition(returnRegister, machineReturnRegister);
      }
   else
      {
      TR::Register *lowReg    = returnRegister->getLowOrder();
      TR::Register *highReg   = returnRegister->getHighOrder();

      TR::RealRegister::RegNum machineLowReturnRegister =
         linkageProperties.getLongLowReturnRegister();

      TR::RealRegister::RegNum machineHighReturnRegister =
         linkageProperties.getLongHighReturnRegister();
      dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 0, cg->trMemory());
      dependencies->addPreCondition(lowReg, machineLowReturnRegister);
      dependencies->addPreCondition(highReg, machineHighReturnRegister);
      }
   generateAdminInstruction(cg, TR::InstOpCode::ret, node);
   generateDepInstruction(cg, TR::InstOpCode::blr, node, dependencies);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

// areturn handled by ireturnEvaluator

TR::Register *OMR::Power::TreeEvaluator::returnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, TR::InstOpCode::ret, node);
   generateInstruction(cg, TR::InstOpCode::blr, node);
   return NULL;
   }

static TR::InstOpCode::Mnemonic cmp2bun(TR::ILOpCodes op)
    {
    switch (op)
       {
       case TR::fcmpeq:
       case TR::fcmpne:
       case TR::fcmplt:
       case TR::fcmpgt:
       case TR::fcmpgeu:
       case TR::fcmpleu:
       case TR::dcmpeq:
       case TR::dcmpne:
       case TR::dcmplt:
       case TR::dcmpgt:
       case TR::dcmpgeu:
       case TR::dcmpleu:
          return TR::InstOpCode::bad;
       case TR::fcmpge:
       case TR::fcmple:
       case TR::dcmpge:
       case TR::dcmple:
          return TR::InstOpCode::beq;
       case TR::fcmpequ:
       case TR::fcmpltu:
       case TR::fcmpgtu:
       case TR::dcmpequ:
       case TR::dcmpltu:
       case TR::dcmpgtu:
          return TR::InstOpCode::bun;
       default:
          TR_ASSERT(false, "assertion failure");
       }
     return TR::InstOpCode::bad;
    }


static TR::InstOpCode::Mnemonic cmp2branch(TR::ILOpCodes op, TR::CodeGenerator *cg)
    {
    switch (op)
       {
       case TR::icmpeq:
       case TR::acmpeq:
       case TR::lcmpeq:
       case TR::fcmpeq:
       case TR::dcmpeq:
       case TR::fcmpequ:
       case TR::dcmpequ:
       case TR::bcmpeq:
          return TR::InstOpCode::beq;
       case TR::icmpne:
       case TR::acmpne:
       case TR::lcmpne:
       case TR::fcmpne:
       case TR::dcmpne:
       case TR::fcmpneu:
       case TR::dcmpneu:
       case TR::bcmpne:
          return TR::InstOpCode::bne;
       case TR::icmplt:
       case TR::iucmplt:
       case TR::acmplt:
       case TR::lcmplt:
       case TR::lucmplt:
       case TR::fcmple:
       case TR::dcmple:
       case TR::fcmplt:
       case TR::dcmplt:
       case TR::fcmpltu:
       case TR::dcmpltu:
       case TR::bucmplt:
       case TR::bcmplt:
          return TR::InstOpCode::blt;
       case TR::icmpge:
       case TR::iucmpge:
       case TR::acmpge:
       case TR::lcmpge:
       case TR::lucmpge:
       case TR::fcmpgeu:
       case TR::dcmpgeu:
       case TR::bucmpge:
       case TR::bcmpge:
          return TR::InstOpCode::bge;
       case TR::icmpgt:
       case TR::iucmpgt:
       case TR::acmpgt:
       case TR::lcmpgt:
       case TR::lucmpgt:
       case TR::fcmpge:
       case TR::dcmpge:
       case TR::fcmpgt:
       case TR::dcmpgt:
       case TR::fcmpgtu:
       case TR::dcmpgtu:
       case TR::bucmpgt:
       case TR::bcmpgt:
          return TR::InstOpCode::bgt;
       case TR::icmple:
       case TR::iucmple:
       case TR::acmple:
       case TR::lcmple:
       case TR::lucmple:
       case TR::fcmpleu:
       case TR::dcmpleu:
       case TR::bucmple:
       case TR::bcmple:
          return TR::InstOpCode::ble;
       default:
       TR_ASSERT(false, "assertion failure");
       }

    return TR::InstOpCode::bad;
    }


static TR::InstOpCode::Mnemonic cmp2cmp(TR::ILOpCodes op, TR::CodeGenerator *cg)
    {
    switch (op)
       {
       case TR::icmpeq:
       case TR::icmpne:
       case TR::icmplt:
       case TR::icmpge:
       case TR::icmpgt:
       case TR::icmple:
       case TR::bcmpeq:
       case TR::bcmpne:
       case TR::bcmplt:
       case TR::bcmpge:
       case TR::bcmpgt:
       case TR::bcmple:
          return TR::InstOpCode::cmp4;
       case TR::iucmplt:
       case TR::iucmpge:
       case TR::iucmpgt:
       case TR::iucmple:
       case TR::bucmplt:
       case TR::bucmpge:
       case TR::bucmpgt:
       case TR::bucmple:
          return TR::InstOpCode::cmpl4;
       case TR::lcmpeq:
       case TR::lcmpne:
       case TR::lcmplt:
       case TR::lcmpge:
       case TR::lcmpgt:
       case TR::lcmple:
          return cg->comp()->target().is64Bit() ? TR::InstOpCode::cmp8 : TR::InstOpCode::cmp4;
       case TR::lucmplt:
       case TR::lucmpge:
       case TR::lucmpgt:
       case TR::lucmple:
       case TR::acmpeq:
       case TR::acmpne:
       case TR::acmplt:
       case TR::acmpge:
       case TR::acmpgt:
       case TR::acmple:
          return cg->comp()->target().is64Bit() ? TR::InstOpCode::cmpl8 : TR::InstOpCode::cmpl4;
       default:
       TR_ASSERT(false, "assertion failure");
       }
    return TR::InstOpCode::bad;
    }


static TR::InstOpCode::Mnemonic cmp2cmpi(TR::ILOpCodes op, TR::CodeGenerator *cg)
    {
    switch (op)
       {
       case TR::icmpeq:
       case TR::icmpne:
       case TR::icmplt:
       case TR::icmpge:
       case TR::icmpgt:
       case TR::icmple:
       case TR::bcmpeq:
       case TR::bcmpne:
       case TR::bcmplt:
       case TR::bcmpge:
       case TR::bcmpgt:
       case TR::bcmple:
          return TR::InstOpCode::cmpi4;
       case TR::iucmplt:
       case TR::iucmpge:
       case TR::iucmpgt:
       case TR::iucmple:
       case TR::bucmplt:
       case TR::bucmpge:
       case TR::bucmpgt:
       case TR::bucmple:
          return TR::InstOpCode::cmpli4;
       case TR::lcmpeq:
       case TR::lcmpne:
       case TR::lcmplt:
       case TR::lcmpge:
       case TR::lcmpgt:
       case TR::lcmple:
          return cg->comp()->target().is64Bit() ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4;
       case TR::lucmplt:
       case TR::lucmpge:
       case TR::lucmpgt:
       case TR::lucmple:
       case TR::acmpeq:
       case TR::acmpne:
       case TR::acmplt:
       case TR::acmpge:
       case TR::acmpgt:
       case TR::acmple:
          return cg->comp()->target().is64Bit() ? TR::InstOpCode::cmpli8 : TR::InstOpCode::cmpli4;
       default:
       TR_ASSERT(false, "assertion failure");
       }
    return TR::InstOpCode::bad;
    }


TR::Register *OMR::Power::TreeEvaluator::iselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::DataType type = node->getType();
   bool two_reg = (cg->comp()->target().is32Bit()) && type.getDataType() == TR::Int64;
   TR::Register *resultReg = two_reg ?
                            cg->allocateRegisterPair(cg->allocateRegister(),cg->allocateRegister()) :
                            (type.getDataType() == TR::Float ?
                            cg->allocateSinglePrecisionRegister() :
                            (type.getDataType() == TR::Double ?
                            cg->allocateRegister(TR_FPR) :
                            cg->allocateRegister(TR_GPR)));

   // Mark the result reg as collected reference if required.
   if (!node->isNotCollected())
      {
      resultReg->setContainsCollectedReference();
      }

   TR::InstOpCode::Mnemonic move_opcode = (type.isIntegral() || type.isAddress()) ? TR::InstOpCode::mr : TR::InstOpCode::fmr;
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isBooleanCompare() &&
       firstChild->getRegister() == NULL &&
       firstChild->getReferenceCount() == 1 &&
       !(firstChild->getFirstChild()->getType().isInt64() && cg->comp()->target().is32Bit()))
      {
      //This is now either 64 bit only.
      // (cmp1Reg [branch_opcode] cmp2Reg) ? trueReg : falseReg;
      TR::DataType compare_type = firstChild->getFirstChild()->getType();

      TR::Register * trueReg = cg->evaluate(node->getChild(1));
      TR::Register * falseReg = cg->evaluate(node->getChild(2));
      TR::Register * cmp1Reg = cg->evaluate(firstChild->getFirstChild());
      TR::Register * cmp2Reg = NULL;      //Do not evaluate this unless we have to.

      TR::InstOpCode::Mnemonic branch_opcode = cmp2branch(firstChild->getOpCodeValue(), cg);
      TR::Register    *ccr = cg->allocateRegister(TR_CCR);
      TR::LabelSymbol *doneLabel  = generateLabelSymbol(cg);

      bool useImmediateCompare = false;

      if (compare_type.isIntegral() || compare_type.isAddress())
         {
         TR::LabelSymbol *label;
         int64_t value;

         if (firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
               value = firstChild->getSecondChild()->get64bitIntegralValue();
               useImmediateCompare = value >= LOWER_IMMED && value <= UPPER_IMMED;
            }

         //We do not have an immediate within range at firstChild->getSecondChild(), so evaluate it.
         if(!useImmediateCompare)
            {
            cmp2Reg = cg->evaluate(firstChild->getSecondChild());
            }

         if (useImmediateCompare)
            {
            generateTrg1Src1ImmInstruction(cg, cmp2cmpi(firstChild->getOpCodeValue(), cg), node, ccr,
                                             cmp1Reg, value);
            }
         else
            {
            generateTrg1Src2Instruction(cg, cmp2cmp(firstChild->getOpCodeValue(), cg), node, ccr,
                                          cmp1Reg, cmp2Reg);
            }

         if (two_reg)
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getHighOrder(), trueReg->getHighOrder());
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getLowOrder(), trueReg->getLowOrder());
            }
         else
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg, trueReg);
            }
         generateConditionalBranchInstruction(cg, branch_opcode, node, doneLabel, ccr);
         if (two_reg)
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getHighOrder(), falseReg->getHighOrder());
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getLowOrder(), falseReg->getLowOrder());
            }
         else
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg, falseReg);
            }
         }
      else if (compare_type.isFloatingPoint())
         {
         cmp2Reg = cg->evaluate(firstChild->getSecondChild());
         generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, ccr, cmp1Reg, cmp2Reg);

         if (two_reg)
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getHighOrder(), trueReg->getHighOrder());
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getLowOrder(), trueReg->getLowOrder());
            }
         else
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg, trueReg);
            }

         generateConditionalBranchInstruction(cg, branch_opcode, node, doneLabel, ccr);
         TR::InstOpCode::Mnemonic branch_opcode2 = cmp2bun(firstChild->getOpCodeValue());
         if (branch_opcode2 != TR::InstOpCode::bad)
            generateConditionalBranchInstruction(cg, branch_opcode2, node, doneLabel, ccr);

         if (two_reg)
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getHighOrder(), falseReg->getHighOrder());
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg->getLowOrder(), falseReg->getLowOrder());
            }
         else
            {
            generateTrg1Src1Instruction(cg, move_opcode, node, resultReg, falseReg);
            }
         }
      else
         {
         TR_ASSERT(false, "Unsupported compare type for select\n");
         }

      TR::RegisterDependencyConditions *dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, two_reg ? 8 : 5, cg->trMemory());
      if (two_reg)
         {
         dep->addPostCondition(resultReg->getHighOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(resultReg->getLowOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(falseReg->getHighOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(falseReg->getLowOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(trueReg->getHighOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(trueReg->getLowOrder(), TR::RealRegister::NoReg);
         }
      else
         {
         dep->addPostCondition(resultReg, TR::RealRegister::NoReg);
         dep->addPostCondition(falseReg, TR::RealRegister::NoReg);
         dep->addPostCondition(trueReg, TR::RealRegister::NoReg);
         }
      dep->addPostCondition(cmp1Reg, TR::RealRegister::NoReg);
      if (cmp2Reg) dep->addPostCondition(cmp2Reg, TR::RealRegister::NoReg);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, dep);

      cg->stopUsingRegister(ccr);
      node->setRegister(resultReg);
      cg->decReferenceCount(node->getChild(0)->getChild(0));
      cg->decReferenceCount(node->getChild(0)->getChild(1));
      cg->decReferenceCount(node->getChild(1));
      cg->decReferenceCount(node->getChild(2));
      }
   else
      {
      TR::Register *  trueReg = cg->evaluate(node->getChild(1));
      TR::Register * falseReg = cg->evaluate(node->getChild(2));
      TR::Register *  condReg = cg->evaluate(node->getChild(0));

      TR::Register *ccr       = cg->allocateRegister(TR_CCR);
      bool useRegPairForCond = (condReg->getRegisterPair() != NULL);
      TR::PPCControlFlowInstruction *i = (TR::PPCControlFlowInstruction*)
            generateControlFlowInstruction(cg, TR::InstOpCode::iselect, node, NULL, 0, two_reg, useRegPairForCond);
      i->addTargetRegister(ccr);
      if (two_reg)
         {
         i->addTargetRegister(resultReg->getHighOrder());
         i->addTargetRegister(resultReg->getLowOrder());
         }
      else
         i->addTargetRegister(resultReg);
      i->addSourceRegister(useRegPairForCond ? condReg->getLowOrder() : condReg);
      if (two_reg)
         {
         i->addSourceRegister(trueReg->getHighOrder());
         i->addSourceRegister(trueReg->getLowOrder());
         i->addSourceRegister(falseReg->getHighOrder());
         i->addSourceRegister(falseReg->getLowOrder());
         }
      else
         {
         i->addSourceRegister(trueReg);
         i->addSourceRegister(falseReg);
         }

      if (useRegPairForCond)
         i->addSourceRegister(condReg->getHighOrder());

      i->setOpCode2Value(move_opcode);

      cg->stopUsingRegister(ccr);

      node->setRegister(resultReg);

      cg->decReferenceCount(node->getChild(0));
      cg->decReferenceCount(node->getChild(1));
      cg->decReferenceCount(node->getChild(2));
      }

   return resultReg;

   }

TR::Register *OMR::Power::TreeEvaluator::compareIntsForEquality(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic branchOp = node->getOpCode().isCompareTrueIfEqual() ? TR::InstOpCode::beq : TR::InstOpCode::bne;
   return TR::TreeEvaluator::compareIntsForEquality(branchOp, node->getBranchDestination()->getNode()->getLabel(),
                                 node, cg, false, false);
   }

TR::Register *OMR::Power::TreeEvaluator::compareIntsForEquality(TR::InstOpCode::Mnemonic branchOp, TR::LabelSymbol *dstLabel, TR::Node *node, TR::CodeGenerator *cg, bool isHint, bool likeliness)
   {
   if (virtualGuardHelper(node, cg))
      return NULL;
   TR::Compilation *comp = cg->comp();
   TR::Register *src1Reg, *condReg;
   TR::Node     *firstChild = node->getFirstChild();
   TR::Node     *secondChild = node->getSecondChild();

#ifdef J9_PROJECT_SPECIFIC
if (cg->profiledPointersRequireRelocation() && secondChild->getOpCodeValue() == TR::aconst &&
   (secondChild->isClassPointerConstant() || secondChild->isMethodPointerConstant()))
   {
   if (node->isProfiledGuard())
      {
      TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
      TR_AOTGuardSite *site = comp->addAOTNOPSite();
      site->setType(TR_ProfiledGuard);
      site->setGuard(virtualGuard);
      site->setNode(node);
      site->setAconstNode(secondChild);
      }
   else
      {
      TR_ASSERT(!(node->isNopableInlineGuard()),"Should not evaluate class or method pointer constants underneath NOPable guards as they are runtime assumptions handled by virtualGuardHelper");
      cg->evaluate(secondChild);
      }
   }
#endif

   if (firstChild->getOpCodeValue() == TR::b2i &&
       firstChild->getOpCodeValue() == secondChild->getOpCodeValue() &&
       // Children of b2i don't necessarily have high bits cleared
       // e.g. if child is an i2b/l2b we don't clear the high bits
       // That's why we check that both b2i nodes have loads (lbz) as children to guarantee high bits are clear
       firstChild->getFirstChild()->getOpCode().isLoad() &&
       firstChild->getFirstChild()->getOpCodeValue() == secondChild->getFirstChild()->getOpCodeValue())
      {
      /*
       * Catches the following:
       * ificmpeq/ificmpne
       *   b2i
       *     load
       *   b2i
       *     load
       *
       * Does the compare against the children of the b2i nodes, since eq/ne is not affected by sign extension
       * Skips evaluating b2i if it is unused (i.e. refcount == 1, which is usually the case)
       */
      if (firstChild->getReferenceCount() > 1)
         cg->evaluate(firstChild);
      else
         cg->evaluate(firstChild->getFirstChild());
      if (secondChild->getReferenceCount() > 1)
         cg->evaluate(secondChild);
      else
         cg->evaluate(secondChild->getFirstChild());

      TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0,1, cg->trMemory());

      src1Reg = firstChild->getFirstChild()->getRegister();
      TR::Register *src2Reg = secondChild->getFirstChild()->getRegister();
      condReg = cg->allocateRegister(TR_CCR);

      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, src1Reg, src2Reg);

      conditions->addPostCondition(condReg, TR::RealRegister::cr0);

      TR::InstOpCode::Mnemonic opCode = (node->getOpCodeValue() == TR::ificmpeq ||
                              node->getOpCodeValue() == TR::ifscmpeq ||
                              node->getOpCodeValue() == TR::ifbcmpeq)
         ? TR::InstOpCode::beq : TR::InstOpCode::bne;
      TR::LabelSymbol *label = node->getBranchDestination()->getNode()->getLabel();

      if (node->getNumChildren() == 3)
         {
         TR::Node *thirdChild = node->getChild(2);
         TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");
         cg->evaluate(thirdChild);
         generateDepConditionalBranchInstruction(cg, opCode, node, label, condReg,
               generateRegisterDependencyConditions(cg, thirdChild, 0));
         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg), conditions);
         cg->decReferenceCount(thirdChild);
         }
      else
         {
         generateDepConditionalBranchInstruction(cg, opCode, node, label, condReg, conditions);
         }

      // If a child isn't in a register that means we
      // 1) didn't evaluate it here
      // 2) evaluated the grandchild instead
      // 3) had we evaluated the child, the grandchild's refcount would have been decremented, so we have to decrement it instead
      // If a child is in a register, regardless of whether or not we evaluated it, we don't have to decrement the grandchild
      if (!firstChild->getRegister())
         cg->decReferenceCount(firstChild->getFirstChild());
      if (!secondChild->getRegister())
         cg->decReferenceCount(secondChild->getFirstChild());
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      cg->stopUsingRegister(condReg);

      return NULL;
      }

   int64_t      value = secondChild->getOpCode().isLoadConst() ? secondChild->get64bitIntegralValue() : 0;
   bool         cannotInline = false;
    if ((firstChild->getOpCodeValue() == TR::instanceof) &&
        !(comp->getOption(TR_OptimizeForSpace) ||
          comp->getOption(TR_DisableInlineIfInstanceOf)) &&
        (firstChild->getRegister() == NULL) &&
        (node->getReferenceCount() <=1) &&
        secondChild->getOpCode().isLoadConst() &&
        (value==0 || value==1))
       {
#ifdef J9_PROJECT_SPECIFIC
       if (TR::TreeEvaluator::ifInstanceOfEvaluator(node, cg) == NULL)
          return(NULL);
#endif
       cannotInline = true;
       if(!(comp->isOptServer()))
          {
          TR::Node::recreate(firstChild, TR::icall);
          }
       }

    src1Reg   = cg->evaluate(firstChild);
    if (cannotInline)
       {
       TR::Node::recreate(firstChild, TR::instanceof);
       }
    condReg = cg->allocateRegister(TR_CCR);

    bool isSigned = !node->getOpCode().isUnsignedCompare();

   if (isSigned)
      {
      if (secondChild->getOpCode().isLoadConst() &&
         secondChild->getRegister() == NULL &&
         value >= LOWER_IMMED && value <= UPPER_IMMED)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, src1Reg, value);
         }
      else
         {
         TR::Register *src2Reg = cg->evaluate(secondChild);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, condReg, src1Reg, src2Reg);
         }
      }
   else
      {
      if (secondChild->getOpCode().isLoadConst() &&
         secondChild->getRegister() == NULL &&
         secondChild->get64bitIntegralValue() >= 0 &&
         secondChild->get64bitIntegralValue() <= 0xFFFF)
         {

         TR::Register *tReg = NULL;
         bool newReg = false;
         uint64_t value = secondChild->get64bitIntegralValue();
         tReg = src1Reg;

         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, tReg, value);

         if (newReg)
         cg->stopUsingRegister(tReg);
         }
      else
         {
         TR::Register *tReg = NULL;
         bool newReg = false;
         TR::Register *secondReg = NULL;
         tReg = src1Reg;
         secondReg = cg->evaluate(secondChild);

         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, tReg, secondReg);

         if (newReg)
         cg->stopUsingRegister(tReg);
         cg->stopUsingRegister(secondReg);
         }
      }

    if (node->getOpCode().isIf() && node->getNumChildren() == 3)
       {
       TR::Node *thirdChild = node->getChild(2);
       cg->evaluate(thirdChild);
       if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
          generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
                generateRegisterDependencyConditions(cg, thirdChild, 0));
       else
          generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
                generateRegisterDependencyConditions(cg, thirdChild, 0));

       cg->decReferenceCount(thirdChild);
       }
    else
       {
       if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
          generateConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg);
       else
          generateConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg);
       }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(condReg);
   return NULL;
   }

// also handles ificmpne, and ifacmpeq and ifacmpne in 32-bit mode
// for ifacmpeq, opcode has been temporarily set to ificmpeq
TR::Register *OMR::Power::TreeEvaluator::ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForEquality(node, cg);
   return NULL;
   }

// ificmpne handled by ificmpeqEvaluator

TR::Register *OMR::Power::TreeEvaluator::ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild=node->getFirstChild(), *secondChild=node->getSecondChild();
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::blt, node, cg, true);
   if (secondChild->getOpCode().isLoadConst() && secondChild->getInt()>=0)
      firstChild->setIsNonNegative(true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::bge, node, cg, true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::bgt, node, cg, true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::ble, node, cg, true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::blt, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::bge, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::bgt, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntsForOrder(TR::InstOpCode::ble, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::compareLongsForEquality(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic branchOp = node->getOpCode().isCompareTrueIfEqual() ? TR::InstOpCode::beq : TR::InstOpCode::bne;
   return TR::TreeEvaluator::compareLongsForEquality(branchOp, node->getBranchDestination()->getNode()->getLabel(),
                                 node, cg, false, false);
   }

TR::Register *OMR::Power::TreeEvaluator::compareLongsForEquality(TR::InstOpCode::Mnemonic branchOp, TR::LabelSymbol *dstLabel, TR::Node *node, TR::CodeGenerator *cg, bool isHint, bool likeliness)
   {
   if (cg->comp()->target().is64Bit())
      {
      if (virtualGuardHelper(node, cg))
         return NULL;
      }

   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   static bool disableCompareToOpt = (feGetEnv("TR_disableCompareToOpt")!=NULL);

   //Peephole to not compare top half of operands if shifted by more than 32
   //Useful for our implementation of BigDecimal.compareTo()
   if (!disableCompareToOpt && cg->comp()->target().is32Bit() &&
       firstChild->getOpCodeValue() == TR::lshr && secondChild->getOpCodeValue() == TR::lshr &&
       firstChild->getReferenceCount() == 1 && secondChild->getReferenceCount() == 1)
      {
      TR::Register *firstShiftResultReg = firstChild->getRegister();
      TR::Register *secondShiftResultReg = secondChild->getRegister();
      TR::Node *firstShiftSourceNode = firstChild->getFirstChild();
      TR::Node *secondShiftSourceNode = secondChild->getFirstChild();
      TR::Node *firstShiftAmountNode = firstChild->getSecondChild();
      TR::Node *secondShiftAmountNode = secondChild->getSecondChild();

      if (firstShiftAmountNode->getOpCode().isLoadConst() && secondShiftAmountNode->getOpCode().isLoadConst()&&
          !firstShiftResultReg && !secondShiftResultReg)
         {
         int64_t firstShiftValue = firstShiftAmountNode->getInt()&0x3f;;
         int64_t secondShiftValue = secondShiftAmountNode->getInt()&0x3f;;

         if (firstShiftValue > 32 && secondShiftValue > 32)
            {
            cg->decReferenceCount(firstShiftAmountNode);
            cg->decReferenceCount(secondShiftAmountNode);
            TR::Register *firstShiftSourceReg = cg->evaluate(firstShiftSourceNode);
            TR::Register *secondShiftSourceReg = cg->evaluate(secondShiftSourceNode);
            cg->decReferenceCount(firstShiftSourceNode);
            cg->decReferenceCount(secondShiftSourceNode);
            firstShiftResultReg = cg->allocateRegister();
            secondShiftResultReg = cg->allocateRegister();
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, firstShiftResultReg, firstShiftSourceReg->getHighOrder(), firstShiftValue-32);
            firstChild->setRegister(firstShiftResultReg);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, secondShiftResultReg, secondShiftSourceReg->getHighOrder(), secondShiftValue-32);
            secondChild->setRegister(secondShiftResultReg);
            if (node->getOpCode().isIf())
               {
               TR::Node::recreate(node, node->getOpCode().isCompareTrueIfEqual() ? TR::ificmpeq : TR::ificmpne);
               }
            else
               {
               TR::Node::recreate(node, node->getOpCode().isCompareTrueIfEqual() ? TR::icmpeq : TR::icmpne);
               }
            return TR::TreeEvaluator::compareIntsForEquality(branchOp, dstLabel, node, cg, isHint, likeliness);
            }
         }
      }

   TR::Register   *src1Reg = cg->evaluate(firstChild);
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::Node        *thirdChild;
   int64_t value = secondChild->getOpCode().isLoadConst() ? secondChild->get64bitIntegralValue() : 0;
   TR::RegisterDependencyConditions *deps = NULL;
   bool isSigned = !node->getOpCode().isUnsignedCompare();
   TR::Compilation *comp = cg->comp();

   if (cg->comp()->target().is64Bit())
      {

#ifdef J9_PROJECT_SPECIFIC
if (cg->profiledPointersRequireRelocation() && secondChild->getOpCodeValue() == TR::aconst &&
   (secondChild->isClassPointerConstant() || secondChild->isMethodPointerConstant()))
   {
   if (node->isProfiledGuard())
      {
      TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
      TR_AOTGuardSite *site = comp->addAOTNOPSite();
      site->setType(TR_ProfiledGuard);
      site->setGuard(virtualGuard);
      site->setNode(node);
      site->setAconstNode(secondChild);
      }
   else
      {
      TR_ASSERT(!(node->isNopableInlineGuard()),"Should not evaluate class or method pointer constants underneath NOPable guards as they are runtime assumptions handled by virtualGuardHelper");
      cg->evaluate(secondChild);
      }
   }
#endif

      if (secondChild->getOpCode().isLoadConst() &&
          secondChild->getRegister() == NULL &&
         ((!isSigned && ((uint64_t)value <= 0xffff)) ||
         (isSigned && (value >= LOWER_IMMED && value <= UPPER_IMMED))) &&
         (secondChild->getOpCodeValue() != TR::aconst || !cg->constantAddressesCanChangeSize(secondChild)))

         {
         if (isSigned)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi8, node, condReg, src1Reg, value);
         else
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, src1Reg, value);
         }
      else
         {
         TR::Register *src2Reg   = cg->evaluate(secondChild);
         if (isSigned)
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, condReg, src1Reg, src2Reg);
         else
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl8, node, condReg, src1Reg, src2Reg);
         }

      if (node->getOpCode().isIf() && node->getNumChildren() == 3)
         {
         thirdChild = node->getChild(2);
         cg->evaluate(thirdChild);
         if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
            generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         else
            generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         cg->decReferenceCount(thirdChild);
         }
      else
         {
         if (isHint && cg->comp()->target().cpu.id() >= TR_PPCgp)
            generateConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg);
         else
            generateConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg);
         }
      cg->stopUsingRegister(condReg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return NULL;
      }
   else // 32 bit target
      {
      TR::Register *src2Reg;
      bool useImmed = secondChild->getOpCode().isLoadConst() &&
          secondChild->getRegister() == NULL &&
          value >= 0 && value <= 0xffff;
      if (!useImmed)
         src2Reg = cg->evaluate(secondChild);

      if (node->getOpCode().isIf() && node->getNumChildren() == 3)
         {
         thirdChild = node->getChild(2);
         cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(cg, thirdChild, 4);
         cg->decReferenceCount(thirdChild);
         }

      if (deps == NULL)
         deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
      fixDepsForLongCompare(deps, src1Reg->getHighOrder(), src1Reg->getLowOrder(), useImmed ? NULL : src2Reg->getHighOrder(), useImmed ? NULL : src2Reg->getLowOrder());
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
      generateControlFlowInstruction(cg, TR::InstOpCode::iflong, node, deps);
      cfop->addTargetRegister(condReg);
      cfop->addSourceRegister(src1Reg->getHighOrder());
      cfop->addSourceRegister(src1Reg->getLowOrder());
      if (useImmed)
         {
         cfop->addSourceImmediate(secondChild->getLongIntHigh());
         cfop->addSourceImmediate(secondChild->getLongIntLow());
         }
      else
         {
         cfop->addSourceRegister(src2Reg->getHighOrder());
         cfop->addSourceRegister(src2Reg->getLowOrder());
         }
      cfop->setLabelSymbol(dstLabel);
      cfop->setOpCode2Value(branchOp);

      cg->stopUsingRegister(condReg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return NULL;
      }
   }

// also handles iflcmpne, and ifacmpeq and ifacmpne in 64-bit mode
// for ifacmpeq, opcode has been temporarily set to iflcmpeq
TR::Register *OMR::Power::TreeEvaluator::iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForEquality(node, cg);
   return NULL;
   }


TR::Register *OMR::Power::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::blt, TR::InstOpCode::bgt, node, cg, true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::bge, TR::InstOpCode::ble, node, cg, true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::bgt, TR::InstOpCode::blt, node, cg, true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::ble, TR::InstOpCode::bge, node, cg, true);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::blt, TR::InstOpCode::bgt, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::bge, TR::InstOpCode::ble, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::bgt, TR::InstOpCode::blt, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareLongsForOrder(TR::InstOpCode::ble, TR::InstOpCode::bge, node, cg, false);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      {
      TR::Node::recreate(node, TR::iflcmpeq);
      TR::TreeEvaluator::iflcmpeqEvaluator(node, cg);
      }
   else
      {
      TR::Node::recreate(node, TR::ificmpeq);
      TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
      }
   TR::Node::recreate(node, TR::ifacmpeq);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::ifacmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      {
      TR::Node::recreate(node, TR::iflcmpne);
      TR::TreeEvaluator::iflcmpeqEvaluator(node, cg);
      }
   else
      {
      TR::Node::recreate(node, TR::ificmpne);
      TR::TreeEvaluator::ificmpeqEvaluator(node, cg);
      }
   TR::Node::recreate(node, TR::ifacmpne);
   return NULL;
   }


// also handles acmpeq in 32-bit mode
// and also: bcmpeq, scmpeq
TR::Register *OMR::Power::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg    = cg->allocateRegister();
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *temp1Reg     = NULL;
   TR::Register *temp2Reg     = cg->allocateRegister();
   bool         killTemp1    = false;

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int64_t value = secondChild->get64bitIntegralValue();
      if (value == 0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         temp1Reg = addConstantToInteger(node, src1Reg , -value, cg);
         killTemp1 = true;
         }
      }
   else
      {
      if (secondChild->getOpCode().isLoadConst() &&
          secondChild->get64bitIntegralValue()==0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         temp1Reg = cg->allocateRegister();
         TR::Register *src2Reg   = cg->evaluate(secondChild);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp1Reg, src2Reg, src1Reg);
         killTemp1 = true;
         }
      }
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, temp2Reg, temp1Reg);
   generateShiftRightLogicalImmediate(cg, node, trgReg, temp2Reg, 5);

   if (killTemp1)
      cg->stopUsingRegister(temp1Reg);
   cg->stopUsingRegister(temp2Reg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles acmpne in 32-bit mode
// and also: bcmpne, scmpne
TR::Register *OMR::Power::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg    = cg->allocateRegister();
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *temp1Reg     = NULL;
   TR::Register *temp2Reg     = cg->allocateRegister();
   bool         killTemp1    = false;

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int64_t value = secondChild->get64bitIntegralValue();
      if (value == 0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         temp1Reg = addConstantToInteger(node, src1Reg , -value, cg);
         killTemp1 = true;
         }
      }
   else
      {
      temp1Reg = cg->allocateRegister();
      TR::Register *src2Reg   = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp1Reg, src2Reg ,src1Reg);
      killTemp1 = true;
      }
   // 64-bit needs sign extensions if we use the subf/addic/subfe sequence.
   // therefore, we prefer the subf/cntlzw/rlwinm/xori sequence
   if (cg->comp()->target().is64Bit())
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, temp2Reg, temp1Reg);
      generateShiftRightLogicalImmediate(cg, node, temp2Reg, temp2Reg, 5);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, temp2Reg, 1);
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, temp2Reg, temp1Reg, -1);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subfe, node, trgReg, temp2Reg, temp1Reg);
      }

   if (killTemp1)
      cg->stopUsingRegister(temp1Reg);
   cg->stopUsingRegister(temp2Reg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

void OMR::Power::TreeEvaluator::genBranchSequence(TR::Node* node,
   TR::Register* src1Reg, TR::Register* src2Reg, TR::Register* trgReg,
   TR::InstOpCode::Mnemonic branchOpCode, TR::InstOpCode::Mnemonic cmpOpCode, TR::CodeGenerator* cg)
   {
   TR::Register    *condReg = cg->allocateRegister(TR_CCR);
   TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
      generateControlFlowInstruction(cg, TR::InstOpCode::setbool, node);
   cfop->addTargetRegister(condReg);
   cfop->addTargetRegister(trgReg);
   cfop->addSourceRegister(src1Reg);
   cfop->addSourceRegister(src2Reg);
   cfop->setOpCode2Value(branchOpCode);
   cfop->setCmpOpValue(cmpOpCode);
   cg->stopUsingRegister(condReg);
   }


// also handles bcmplt, scmplt
TR::Register *OMR::Power::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg;
      int64_t value = secondChild->get64bitIntegralValue();
      bool            killTemp1 = false;

      if (value == 0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         TR::Register *temp2Reg = addConstantToInteger(node, src1Reg, -value, cg);
         temp1Reg = cg->allocateRegister();
         killTemp1 = true;
         if (value > 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, temp1Reg, temp2Reg, src1Reg);
            }
         cg->stopUsingRegister(temp2Reg);
         }
      generateShiftRightLogicalImmediate(cg, node, trgReg, temp1Reg, 31);
      if (killTemp1)
         cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::blt, TR::InstOpCode::cmp4, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles bcmple, scmple
TR::Register *OMR::Power::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg;
      int64_t value = secondChild->get64bitIntegralValue();
      bool    killTemp1 = false;
      if (value == -1)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         TR::Register *temp2Reg = addConstantToInteger(node, src1Reg, -(value+1), cg);
         temp1Reg = cg->allocateRegister();
         killTemp1 = true;
         if (value >= 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, temp1Reg, temp2Reg, src1Reg);
            }
         cg->stopUsingRegister(temp2Reg);
         }
      generateShiftRightLogicalImmediate(cg, node, trgReg, temp1Reg, 31);
      if (killTemp1)
         cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::ble, TR::InstOpCode::cmp4, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles bcmpge, scmpge
TR::Register *OMR::Power::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg = cg->allocateRegister();
      int64_t value = secondChild->get64bitIntegralValue();
      if (value == 0)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, temp1Reg, src1Reg, 31);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, trgReg, temp1Reg, 1);
         }
      else
         {
         TR::Register *temp2Reg = addConstantToInteger(node, src1Reg, -value, cg);
         if (value > 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nor, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nand, node, temp1Reg, temp2Reg, src1Reg);
            }
         cg->stopUsingRegister(temp2Reg);
         generateShiftRightLogicalImmediate(cg, node, trgReg, temp1Reg, 31);
         }
      cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bge, TR::InstOpCode::cmp4, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles bcmpgt, scmpgt
TR::Register *OMR::Power::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg = cg->allocateRegister();
      int64_t value = secondChild->get64bitIntegralValue();
      if (value == -1)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, temp1Reg, src1Reg, 31);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, trgReg, temp1Reg, 1);
         }
      else
         {
         TR::Register *temp2Reg = addConstantToInteger(node, src1Reg, -(value+1), cg);
         if (value >= 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nor, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < -1)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nand, node, temp1Reg, temp2Reg, src1Reg);
            }
         cg->stopUsingRegister(temp2Reg);
         generateShiftRightLogicalImmediate(cg, node, trgReg, temp1Reg, 31);
         }
      cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bgt, TR::InstOpCode::cmp4, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles bucmplt, sucmplt
TR::Register *OMR::Power::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();

   if (secondChild->getOpCode().isLoadConst() &&
	   secondChild->getRegister() == NULL  &&
           secondChild->get64bitIntegralValue() == 0)
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::blt, TR::InstOpCode::cmpl4, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles bucmple, sucmple
TR::Register *OMR::Power::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register   *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg    = cg->allocateRegister();

   TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::ble, TR::InstOpCode::cmpl4, cg);
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }


// also handles bucmpge, sucmpge
TR::Register *OMR::Power::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL &&
       secondChild->get64bitIntegralValue() == 0)
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bge, TR::InstOpCode::cmpl4, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles bucmpgt, sucmpgt
TR::Register *OMR::Power::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register   *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg    = cg->allocateRegister();

   TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bgt, TR::InstOpCode::cmpl4, cg);
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles acmpeq in 64-bit mode
TR::Register *OMR::Power::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      {
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmp4, TR::InstOpCode::beq, node,cg);
      }

   TR::Register *trgReg    = cg->allocateRegister();
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *temp1Reg     = NULL;
   TR::Register *temp2Reg     = cg->allocateRegister();
   bool         killTemp1    = false;
   int64_t      value;

   if (secondChild->getOpCode().isLoadConst())
      value = secondChild->getLongInt();

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      if (value == 0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         temp1Reg = addConstantToLong(node, src1Reg , -value, NULL, cg);
         killTemp1 = true;
         }
      }
   else
      {
      if (secondChild->getOpCode().isLoadConst() && value==0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         temp1Reg = cg->allocateRegister();
         TR::Register *src2Reg   = cg->evaluate(secondChild);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp1Reg, src2Reg, src1Reg);
         killTemp1 = true;
         }
      }
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzd, node, temp2Reg, temp1Reg);
   generateShiftRightLogicalImmediate(cg, node, trgReg, temp2Reg, 6);

   if (killTemp1)
      cg->stopUsingRegister(temp1Reg);
   cg->stopUsingRegister(temp2Reg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }


// also handles acmpne in 64-bit mode
TR::Register *OMR::Power::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      {
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmp4, TR::InstOpCode::bne, node, cg);
      }

   TR::Register *trgReg    = cg->allocateRegister();
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *temp1Reg     = NULL;
   TR::Register *temp2Reg     = cg->allocateRegister();
   bool         killTemp1    = false;


   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int64_t value = secondChild->getLongInt();
      if (value == 0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         temp1Reg =  addConstantToLong(node, src1Reg , -value, NULL, cg);
         killTemp1 = true;
         }
      }
   else
      {
      temp1Reg = cg->allocateRegister();
      TR::Register *src2Reg   = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp1Reg, src2Reg ,src1Reg);
      killTemp1 = true;
      }
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, temp2Reg, temp1Reg, -1);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::subfe, node, trgReg, temp2Reg, temp1Reg);

   if (killTemp1)
      cg->stopUsingRegister(temp1Reg);
   cg->stopUsingRegister(temp2Reg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }


TR::Register *OMR::Power::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmp4, TR::InstOpCode::blt, node, cg);

   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg;
      int64_t value = secondChild->getLongInt();
      bool            killTemp1 = false;

      if (value == 0)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         TR::Register *temp2Reg = addConstantToLong(node, src1Reg, -value, NULL, cg);
         temp1Reg = cg->allocateRegister();
         killTemp1 = true;
         if (value > 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, temp1Reg, temp2Reg, src1Reg);
            }

         cg->stopUsingRegister(temp2Reg);
         }
      generateShiftRightLogicalImmediateLong(cg, node, trgReg, temp1Reg, 63);
      if (killTemp1)
         cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::blt, TR::InstOpCode::cmp8, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmp4, TR::InstOpCode::ble, node, cg);

   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg;
      int64_t value = secondChild->getLongInt();
      bool    killTemp1 = false;
      if (value == -1)
         {
         temp1Reg = src1Reg;
         }
      else
         {
         TR::Register *temp2Reg = addConstantToLong(node, src1Reg, -(value+1), NULL, cg);
         temp1Reg = cg->allocateRegister();
         killTemp1 = true;
         if (value >= 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, temp1Reg, temp2Reg, src1Reg);
            }
         cg->stopUsingRegister(temp2Reg);
         }
      generateShiftRightLogicalImmediateLong(cg, node, trgReg, temp1Reg, 63);
      if (killTemp1)
         cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::ble, TR::InstOpCode::cmp8, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
     return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmp4, TR::InstOpCode::bge, node, cg);

   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg = cg->allocateRegister();
      int64_t value = secondChild->getLongInt();
      if (value == 0)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, temp1Reg, src1Reg, 63);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, trgReg, temp1Reg, 1);
         }
      else if (value == 1)
         {
         TR::Register    *temp2Reg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg,TR::InstOpCode::neg, node, temp1Reg, src1Reg);
         generateTrg1Src2Instruction(cg,TR::InstOpCode::andc, node, temp2Reg, temp1Reg, src1Reg);
         generateShiftRightLogicalImmediateLong(cg, node, trgReg, temp2Reg, 63);
         cg->stopUsingRegister(temp2Reg);
         }
      else
         {
         TR::Register *temp2Reg = addConstantToLong(node, src1Reg, -value, NULL, cg);
         if (value > 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nor, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nand, node, temp1Reg, temp2Reg, src1Reg);
            }
         cg->stopUsingRegister(temp2Reg);
         generateShiftRightLogicalImmediateLong(cg, node, trgReg, temp1Reg, 63);
         }
      cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bge, TR::InstOpCode::cmp8, cg);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmp4, TR::InstOpCode::bgt, node, cg);

   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *trgReg    = cg->allocateRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Register    *temp1Reg = cg->allocateRegister();
      int64_t value = secondChild->getLongInt();
      if (value == 0)
         {
         TR::Register    *temp2Reg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg,TR::InstOpCode::neg, node, temp1Reg, src1Reg);
         generateTrg1Src2Instruction(cg,TR::InstOpCode::andc, node, temp2Reg, temp1Reg, src1Reg);
         generateShiftRightLogicalImmediateLong(cg, node, trgReg, temp2Reg, 63);
         cg->stopUsingRegister(temp2Reg);
         }
      else if (value == -1)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, temp1Reg, src1Reg, 63);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, trgReg, temp1Reg, 1);
         }
      else
         {
         TR::Register *temp2Reg = addConstantToLong(node, src1Reg, -(value+1), NULL, cg);
         if (value > 0)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nor, node, temp1Reg, temp2Reg, src1Reg);
            }
         else // if (value < -1)
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::nand, node, temp1Reg, temp2Reg, src1Reg);
            }
         cg->stopUsingRegister(temp2Reg);
         generateShiftRightLogicalImmediateLong(cg, node, trgReg, temp1Reg, 63);
         }
      cg->stopUsingRegister(temp1Reg);
      }
   else
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bgt, TR::InstOpCode::cmp8, cg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmpl4, TR::InstOpCode::blt, node, cg);

   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg  = cg->allocateRegister();
   TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::blt, TR::InstOpCode::cmpl8, cg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmpl4, TR::InstOpCode::ble, node, cg);

   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg  = cg->allocateRegister();
   TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::ble, TR::InstOpCode::cmpl8, cg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
     return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmpl4, TR::InstOpCode::bge, node, cg);

   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg  = cg->allocateRegister();
   TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bge, TR::InstOpCode::cmpl8, cg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is32Bit())
      return compareLongAndSetOrderedBoolean(TR::InstOpCode::cmpl4, TR::InstOpCode::bgt, node, cg);

   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg  = cg->allocateRegister();
   TR::TreeEvaluator::genBranchSequence(node, src1Reg, src2Reg, trgReg, TR::InstOpCode::bgt, TR::InstOpCode::cmpl8, cg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg           = cg->allocateRegister();
   TR::Node     *firstChild  = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node     *secondChild = node->getSecondChild();
   TR::Register *temp1Reg, *temp2Reg, *temp3Reg;

   if (cg->comp()->target().is64Bit())
      {
      if (secondChild->getOpCode().isLoadConst() && secondChild->getLongInt()==0)
         {
         if (cg->comp()->target().cpu.id() >= TR_PPCp9)
            {
            TR::Register *condReg = cg->allocateRegister(TR_CCR);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi8, node, condReg, src1Reg, 0);
            generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
            cg->stopUsingRegister(condReg);
            }
         else
            {
            temp1Reg = cg->allocateRegister();

            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, trgReg, src1Reg, 63);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, temp1Reg, src1Reg, -1);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::adde, node, trgReg, trgReg, trgReg);

            cg->stopUsingRegister(temp1Reg);
            }
         }
      else
         {
         TR::Register *src2Reg = cg->evaluate(secondChild);
         if (cg->comp()->target().cpu.id() >= TR_PPCp9)
            {
            TR::Register *condReg = cg->allocateRegister(TR_CCR);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, condReg, src1Reg, src2Reg);
            generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
            cg->stopUsingRegister(condReg);
            }
         else
            {
            temp1Reg = cg->allocateRegister();
            temp2Reg = cg->allocateRegister();
            temp3Reg = cg->allocateRegister();

            generateShiftRightLogicalImmediateLong(cg, node, temp1Reg, src1Reg, 63);
            generateShiftRightLogicalImmediateLong(cg, node, temp2Reg, src2Reg, 63);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subfc, node, temp3Reg, src1Reg, src2Reg);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subfe, node, trgReg, temp2Reg, temp1Reg);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subfc, node, temp2Reg, temp3Reg, trgReg);
            generateTrg1Src1Instruction(cg, TR::InstOpCode::subfze, node, trgReg, trgReg);

            cg->stopUsingRegister(temp1Reg);
            cg->stopUsingRegister(temp2Reg);
            cg->stopUsingRegister(temp3Reg);
            }
         }
      }
   else
      {
      if (secondChild->getOpCode().isLoadConst() && secondChild->getLongInt()==0)
         {
         temp1Reg = cg->allocateRegister();
         temp2Reg = cg->allocateRegister();
         temp3Reg = cg->allocateRegister();

         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, temp1Reg, src1Reg->getHighOrder(), 31);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, temp2Reg, src1Reg->getLowOrder(), 0);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::subfze, node, temp3Reg, src1Reg->getHighOrder());
         generateShiftRightLogicalImmediate(cg, node, temp2Reg, temp3Reg, 31);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, trgReg, temp1Reg, temp2Reg);

         cg->stopUsingRegister(temp1Reg);
         cg->stopUsingRegister(temp2Reg);
         cg->stopUsingRegister(temp3Reg);
         }
      else
         {
         TR::Register *src2Reg = cg->evaluate(secondChild);
         TR::Register *cond1Reg = cg->allocateRegister(TR_CCR);

         TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
            generateControlFlowInstruction(cg, TR::InstOpCode::lcmp, node);
         cfop->addTargetRegister(cond1Reg);
         cfop->addTargetRegister(trgReg);
         cfop->addSourceRegister(src1Reg->getHighOrder());
         cfop->addSourceRegister(src1Reg->getLowOrder());
         cfop->addSourceRegister(src2Reg->getHighOrder());
         cfop->addSourceRegister(src2Reg->getLowOrder());

         cg->stopUsingRegister(cond1Reg);
         }
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::lcmpeqEvaluator(node, cg);
   else
      return TR::TreeEvaluator::icmpeqEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::lcmpneEvaluator(node, cg);
   else
      return TR::TreeEvaluator::icmpneEvaluator(node, cg);
   }


static
void collectGlDeps(TR::Node *node, TR_BitVector *vector)
   {
   int32_t  ii;

   for (ii=0; ii<node->getNumChildren(); ii++)
      vector->set(node->getChild(ii)->getGlobalRegisterNumber());
   }

static
bool isGlDepsUnBalanced(TR::Node *node, TR::CodeGenerator *cg)
   {
   int32_t     total = node->getNumChildren();
   int32_t     ii, numDeps;
   TR::Node     *child;

   child = node->getSecondChild();
   numDeps = (child->getNumChildren()>0)?child->getFirstChild()->getNumChildren():0;
   for (ii=2; ii<total; ii++)
      {
      child = node->getChild(ii);
      if ((child->getNumChildren()==0 && numDeps!=0) ||
          (child->getNumChildren()>0 && numDeps!=child->getFirstChild()->getNumChildren()))
         break;
      }
   if (ii != total)
      return(true);

   if (numDeps == 0)
      return(false);

   TR_BitVector  avec, bvec;

   avec.init(cg->getNumberOfGlobalRegisters(), cg->trMemory());
   bvec.init(cg->getNumberOfGlobalRegisters(), cg->trMemory());

   collectGlDeps(node->getSecondChild()->getFirstChild(), &avec);
   for (ii=2; ii<total; ii++)
      {
      collectGlDeps(node->getChild(ii)->getFirstChild(), &bvec);
      if (!(avec == bvec))
         return(true);
      if (ii != total-1)
         bvec.empty();
      }
   return(false);
   }

static
void switchDispatch(TR::Node *node, bool fromTableEval, TR::CodeGenerator *cg)
   {
   int32_t    total = node->getNumChildren();
   int32_t    ii;
   bool       unbalanced;

   if (fromTableEval)
      {
      if (total<=UPPER_IMMED)
         lookupScheme1(node, true, true, cg);
      else
         lookupScheme2(node, true, true, cg);
      return;
      }

   unbalanced = isGlDepsUnBalanced(node, cg);

   if (!unbalanced)
      {
      for (ii=2; ii<total; ii++)
         {
         if (node->getChild(ii)->getNumChildren()>0)
            {
            TR::Node *glDepNode = node->getChild(ii)->getFirstChild();
            if (glDepNode != NULL)
               cg->evaluate(glDepNode);
            }
         }
      }

   if (total <= 12)
      {
      for (ii=2; ii<total && node->getChild(ii)->getCaseConstant()>=LOWER_IMMED && node->getChild(ii)->getCaseConstant()<=UPPER_IMMED; ii++)
         ;
      if (ii == total)
         {
         lookupScheme1(node, unbalanced, false, cg);
         return;
         }
      }

   // The children are in ascending order already.
   if (total <= 9)
      {
      CASECONST_TYPE  preInt = node->getChild(2)->getCaseConstant();
      for (ii=3; ii<total; ii++)
         {
         CASECONST_TYPE diff = node->getChild(ii)->getCaseConstant() - preInt;
         preInt += diff;
         if (diff<0 || diff>UPPER_IMMED)
            break;
         }
      if (ii >= total)
         {
         lookupScheme2(node, unbalanced, false, cg);
         return;
         }
      }

   if (total <= 8 || unbalanced)
      {
      lookupScheme3(node, unbalanced, cg);
      return;
      }

   lookupScheme4(node, cg);
   }

static void lookupScheme1(TR::Node *node, bool unbalanced, bool fromTableEval, TR::CodeGenerator *cg)
   {
   int32_t      total = node->getNumChildren();
   TR::Register *selector = cg->evaluate(node->getFirstChild());
   bool         isInt64 = false;
   bool two_reg = (cg->comp()->target().is32Bit()) && isInt64;
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::RegisterDependencyConditions *acond, *bcond, *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
   TR::Node     *secondChild = node->getSecondChild();

   TR::LabelSymbol *toDefaultLabel = NULL;
   if (two_reg)
      {
      toDefaultLabel = generateLabelSymbol(cg);
      TR::addDependency(conditions, selector->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      }
   else
      TR::addDependency(conditions, selector, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::addDependency(conditions, cndRegister, TR::RealRegister::NoReg, TR_CCR, cg);

   acond = conditions;
   if (secondChild->getNumChildren()>0 && !unbalanced)
      {
      cg->evaluate(secondChild->getFirstChild());
      acond = acond->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }

   for (int i=2; i<total; i++)
      {
      TR::Node *child = node->getChild(i);
      if (isInt64)
         {
         int64_t value = child->getCaseConstant();
         if (cg->comp()->target().is64Bit())
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi8, node, cndRegister, selector, value);
            }
         else
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, cndRegister, selector->getHighOrder(), 0);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, toDefaultLabel, cndRegister);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, cndRegister, selector->getHighOrder(), value);
            } // 64bit target?
         } //64bit int selector
      else
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, cndRegister, selector, fromTableEval?(i-2):(int32_t)(child->getCaseConstant()));
         }
      if (unbalanced)
	 {
         bcond = conditions;
         if (child->getNumChildren() > 0)
	    {
            cg->evaluate(child->getFirstChild());
            bcond = bcond->clone(cg, generateRegisterDependencyConditions(cg, child->getFirstChild(), 0));
	    }
         generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, child->getBranchDestination()->getNode()->getLabel(), cndRegister, bcond);
	 }
      else
	 {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, child->getBranchDestination()->getNode()->getLabel(), cndRegister);
	 }
      }

   if (two_reg)
     generateLabelInstruction(cg, TR::InstOpCode::label, node, toDefaultLabel);

   if (secondChild->getNumChildren()>0 && unbalanced)
      {
      cg->evaluate(secondChild->getFirstChild());
      acond = acond->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }
   generateDepLabelInstruction(cg, TR::InstOpCode::b, node, secondChild->getBranchDestination()->getNode()->getLabel(), acond);
   cg->stopUsingRegister(cndRegister);
   }

static void lookupScheme2(TR::Node *node, bool unbalanced, bool fromTableEval, TR::CodeGenerator *cg)
   {
   int32_t     total = node->getNumChildren();
   TR::Register *selector = cg->evaluate(node->getFirstChild());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::Register *valRegister = NULL;

   bool         isInt64 = false;
   bool two_reg = (cg->comp()->target().is32Bit()) && isInt64;
   TR::LabelSymbol *toDefaultLabel = NULL;
   if ( two_reg )
      {
      valRegister = cg->allocateRegisterPair(cg->allocateRegister(),cg->allocateRegister());
      toDefaultLabel = generateLabelSymbol(cg);
      }
   else
      {
      valRegister = cg->allocateRegister();
      }

   TR::RegisterDependencyConditions *acond, *bcond, *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
   TR::Node     *secondChild = node->getSecondChild();

   if(two_reg)
      {
      TR::addDependency(conditions, valRegister->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, valRegister->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      }
   else
      {
      TR::addDependency(conditions, valRegister, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector, TR::RealRegister::NoReg, TR_GPR, cg);
      }

   TR::addDependency(conditions, cndRegister, TR::RealRegister::NoReg, TR_CCR, cg);
   conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   if(two_reg)
      {
      conditions->getPreConditions()->getRegisterDependency(1)->setExcludeGPR0();
      conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();
      }

   acond = conditions;
   if (secondChild->getNumChildren()>0 && !unbalanced)
      {
      cg->evaluate(secondChild->getFirstChild());
      acond = acond->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }

   int32_t   preInt = fromTableEval?0:node->getChild(2)->getCaseConstant();

   if (isInt64)
      {
      if (two_reg)
         {
         loadConstant(cg, node, (int32_t)(preInt >> 32), valRegister->getHighOrder());
         loadConstant(cg, node, (int32_t)preInt, valRegister->getLowOrder());
         }
      else
         loadConstant(cg, node, preInt, valRegister);
      }
   else
      {
      loadConstant(cg, node, (int32_t) preInt, valRegister);
      }

   for (int i=2; i<total; i++)
      {
      TR::Node *child = node->getChild(i);

      if (isInt64)
         {
         if (cg->comp()->target().is64Bit())
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, cndRegister, selector, valRegister);
            }
         else
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, cndRegister, selector->getHighOrder(), valRegister->getHighOrder());
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, toDefaultLabel, cndRegister);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, cndRegister, selector->getLowOrder(), valRegister->getLowOrder());
            }
         }
      else
         {
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, cndRegister, selector, valRegister);
         }

      if (unbalanced)
	 {
         bcond = conditions;
         if (child->getNumChildren() > 0)
	    {
            cg->evaluate(child->getFirstChild());
            bcond = bcond->clone(cg, generateRegisterDependencyConditions(cg, child->getFirstChild(), 0));
	    }
         generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, child->getBranchDestination()->getNode()->getLabel(), cndRegister, bcond);
	 }
      else
	 {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, child->getBranchDestination()->getNode()->getLabel(), cndRegister);
	 }
      if (i<total-1)
	 {
         int32_t diff = fromTableEval?1:(node->getChild(i+1)->getCaseConstant()-preInt);
         preInt += diff;
         if (!isInt64)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, valRegister, valRegister, (int32_t)diff);
         else
            addConstantToLong(node, valRegister, diff, valRegister, cg);

         }
      }

   if (two_reg)
      generateLabelInstruction(cg, TR::InstOpCode::label, node, toDefaultLabel);

   if (secondChild->getNumChildren()>0 && unbalanced)
      {
      cg->evaluate(secondChild->getFirstChild());
      acond = acond->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }
   generateDepLabelInstruction(cg, TR::InstOpCode::b, node, secondChild->getBranchDestination()->getNode()->getLabel(), acond);

   cg->stopUsingRegister(cndRegister);
   cg->stopUsingRegister(valRegister);
   }

static void lookupScheme3(TR::Node *node, bool unbalanced, TR::CodeGenerator *cg)
   {
   TR::Instruction *rel1, *rel2;
   int32_t     total = node->getNumChildren();
   int32_t     numberOfEntries = total - 2;
   int32_t     nextAddress = 4;
   size_t      dataTableSize = numberOfEntries * sizeof(int);
   int32_t     *dataTable = NULL;
   int64_t     *dataTable64 = NULL;
   bool        isInt64 = false;
   TR::Compilation *comp = cg->comp();
   bool        two_reg = isInt64 && cg->comp()->target().is32Bit();
   if (isInt64)
      {
      dataTableSize *=2;
      dataTable64 =  (int64_t *)cg->allocateCodeMemory(dataTableSize, cg->getCurrentEvaluationBlock()->isCold());
      }
   else
      dataTable =  (int32_t *)cg->allocateCodeMemory(dataTableSize, cg->getCurrentEvaluationBlock()->isCold());

   intptr_t   address = isInt64 ? ((intptr_t) dataTable64) : ((intptr_t)dataTable);
   TR::Register *selector = cg->evaluate(node->getFirstChild());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::Register *addrRegister = cg->allocateRegister();
   TR::Register *dataRegister = NULL;
   TR::LabelSymbol *toDefaultLabel = NULL;
   if (two_reg)
      {
      dataRegister = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      toDefaultLabel = generateLabelSymbol(cg);
      }
   else
      dataRegister =  cg->allocateRegister();

   TR::RegisterDependencyConditions *acond, *bcond, *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());
   TR::Node     *secondChild = node->getSecondChild();

   TR::addDependency(conditions, addrRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   if (isInt64 && cg->comp()->target().is64Bit())
      {
      TR::addDependency(conditions, dataRegister->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, dataRegister->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      }
   else
      {
      TR::addDependency(conditions, dataRegister, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector, TR::RealRegister::NoReg, TR_GPR, cg);
      }

   TR::addDependency(conditions, cndRegister, TR::RealRegister::NoReg, TR_CCR, cg);
   conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();

   acond = conditions;
   if (secondChild->getNumChildren()>0 && !unbalanced)
      {
      cg->evaluate(secondChild->getFirstChild());
      acond = acond->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }

   if (cg->comp()->target().is64Bit())
      {
      int32_t offset = TR_PPCTableOfConstants::allocateChunk(1, cg);

      if (offset != PTOC_FULL_INDEX)
         {
         offset *= TR::Compiler->om.sizeofReferenceAddress();
         TR_PPCTableOfConstants::setTOCSlot(offset, address);
         if (offset<LOWER_IMMED||offset>UPPER_IMMED)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, addrRegister, cg->getTOCBaseRegister(), cg->hiValue(offset));
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, LO_VALUE(offset), 8, cg));
            }
         else
            {
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), offset, 8, cg));
            }

         if(isInt64)
            generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, 0, 8, cg));
         else
            generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, 0, 4, cg));
         }
      else
         {
         if (cg->needRelocationsForLookupEvaluationData())
            {
            loadAddressConstant(cg, true, node, address, addrRegister);
            generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, 0, 4, cg));
            }
         else
            {
            loadAddressConstant(cg, false, node, cg->hiValue(address)<<16, addrRegister);
            nextAddress = LO_VALUE((int32_t)address);
            if (isInt64)
               {
               if (nextAddress >= 32760)
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ldu, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 8, cg));
                  nextAddress = 8;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 8, cg));
                  nextAddress += 8;
                  }
               }
            else
               {
               if (nextAddress >= 32764)
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
                  nextAddress = 4;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
                  nextAddress += 4;
                  }
               } // is64BitInt ?
            } // isAOT?
         }
      }  // ...if 64BitTarget
   else  // if 32bit mode
      {
      rel1 = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, addrRegister, cg->hiValue(address));

      if (isInt64)
         {
         nextAddress = LO_VALUE(address);
         if (nextAddress == 32760)
            {
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
            nextAddress += 4;
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister->getLowOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
            nextAddress = 4;
            }
         else
            {
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
            nextAddress += 4;
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getLowOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
            nextAddress += 4;
            }
         }
      else
         {
         if (cg->comp()->compileRelocatableCode())
            {
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, LO_VALUE(address), 4, cg));
            }
         else
            {
            nextAddress = LO_VALUE(address);
            if (nextAddress >= 32764)
               {
               rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
               nextAddress = 4;
               }
            else
               {
               rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
               nextAddress += 4;
               }
            }
         }

      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data3 = orderedPairSequence1;
      cg->getAheadOfTimeCompile()->getRelocationList().push_front(new (cg->trHeapMemory()) TR::PPCPairedRelocation(
                                     rel1,
                                     rel2,
                                     (uint8_t *)recordInfo,
                                     TR_AbsoluteMethodAddressOrderedPair, node));
      } // if 64BitTarget or 32BitTarget

   for (int32_t ii=2; ii<total; ii++)
      {
      TR::Node *child = node->getChild(ii);
      if (isInt64)
         {
         if (cg->comp()->target().is64Bit())
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, cndRegister, selector, dataRegister);
            }
         else
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, cndRegister, selector->getHighOrder(), dataRegister->getHighOrder());
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, toDefaultLabel, cndRegister);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, cndRegister, selector->getLowOrder(), dataRegister->getLowOrder());
            }
         }
      else
         {
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, cndRegister, selector, dataRegister);
         }

      if (two_reg)
         generateLabelInstruction(cg, TR::InstOpCode::label, node, toDefaultLabel);

      if (unbalanced)
         {
         bcond = conditions;
         if (child->getNumChildren() > 0)
            {
            cg->evaluate(child->getFirstChild());
            bcond = bcond->clone(cg, generateRegisterDependencyConditions(cg, child->getFirstChild(), 0));
            }
         generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, child->getBranchDestination()->getNode()->getLabel(), cndRegister, bcond);
         }
      else
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, child->getBranchDestination()->getNode()->getLabel(), cndRegister);
         }
      if (ii < total-1)
         {
         if (isInt64)
            {
            if (cg->comp()->target().is64Bit())
               {
               if (nextAddress >= 32760)
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ldu, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 8, cg));
                  nextAddress = 8;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 8, cg));
                  nextAddress += 8;
                  }
               }
            else
               {
               if (nextAddress == 32760)
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
                  nextAddress += 4;
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister->getLowOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
                  nextAddress = 4;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
                  nextAddress += 4;
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getLowOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
                  nextAddress += 4;
                  }
               } // 64BitTarget ?
            } // isInt64
         else
            {
            if (nextAddress >= 32764)
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
               nextAddress = 4;
               }
            else
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, nextAddress, 4, cg));
               nextAddress += 4;
               }
            }
         }

      if (isInt64)
         {
         dataTable64[ii-2] = node->getChild(ii)->getCaseConstant();
         }
      else
         {
         dataTable[ii-2] = node->getChild(ii)->getCaseConstant();
         }
      }

   if (secondChild->getNumChildren()>0 && unbalanced)
      {
      cg->evaluate(secondChild->getFirstChild());
      acond = acond->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }
   generateDepLabelInstruction(cg, TR::InstOpCode::b, node, secondChild->getBranchDestination()->getNode()->getLabel(), acond);

   cg->stopUsingRegister(cndRegister);
   cg->stopUsingRegister(dataRegister);
   cg->stopUsingRegister(addrRegister);
   }

static void lookupScheme4(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Instruction  *rel1, *rel2;
   int32_t  total = node->getNumChildren();
   int32_t  numberOfEntries = total - 2;
   size_t  dataTableSize = numberOfEntries * sizeof(int);
   bool        isInt64 = false;
   bool        two_reg = isInt64 && cg->comp()->target().is32Bit();
   int32_t *dataTable = NULL;
   int64_t *dataTable64 = NULL;
   intptr_t  address = NULL;
   if (isInt64)
      {
      dataTableSize *= 2;
      dataTable64 =  (int64_t *)cg->allocateCodeMemory(dataTableSize, cg->getCurrentEvaluationBlock()->isCold());
      address = (intptr_t)dataTable64;
      }
   else
      {
      dataTable =  (int32_t *)cg->allocateCodeMemory(dataTableSize, cg->getCurrentEvaluationBlock()->isCold());
      address = (intptr_t)dataTable;
      }

   TR::Register *selector = cg->evaluate(node->getFirstChild());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::Register *addrRegister = cg->allocateRegister();
   TR::Register *dataRegister = NULL;
   TR::Compilation *comp = cg->comp();
   if (two_reg)
      dataRegister = cg->allocateRegisterPair(cg->allocateRegister(),cg->allocateRegister());
   else
      dataRegister = cg->allocateRegister();

   TR::Register *lowRegister = cg->allocateRegister();
   TR::Register *highRegister = cg->allocateRegister();
   TR::Register *pivotRegister = cg->allocateRegister();
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(9, 9, cg->trMemory());
   TR::Node     *secondChild = node->getSecondChild();

   int32_t loVal = node->getChild(2)->getCaseConstant();
   int32_t hiVal = node->getChild(total-1)->getCaseConstant();
   bool isUnsigned = (hiVal < loVal) ? true : false;
   TR::InstOpCode::Mnemonic cmp_opcode = isUnsigned ? TR::InstOpCode::cmpl4 : TR::InstOpCode::cmp4;

   cg->machine()->setLinkRegisterKilled(true);
   TR::addDependency(conditions, addrRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, pivotRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   if (two_reg)
      {
      TR::addDependency(conditions, dataRegister->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, dataRegister->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      }
   else
      {
      TR::addDependency(conditions, dataRegister, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(conditions, selector, TR::RealRegister::NoReg, TR_GPR, cg);
      }

   TR::addDependency(conditions, lowRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, highRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, cndRegister, TR::RealRegister::NoReg, TR_CCR, cg);
   conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPreConditions()->getRegisterDependency(1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();

   if (secondChild->getNumChildren() > 0)
      {
      cg->evaluate(secondChild->getFirstChild());
      conditions = conditions->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }

   loadConstant(cg, node, (numberOfEntries-1)<<2, highRegister);

   if (cg->comp()->target().is64Bit())
      {
      int32_t offset = TR_PPCTableOfConstants::allocateChunk(1, cg);

      if (offset != PTOC_FULL_INDEX)
         {
         offset *= 8;
         TR_PPCTableOfConstants::setTOCSlot(offset, address);
         if (offset<LOWER_IMMED||offset>UPPER_IMMED)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, pivotRegister, cg->getTOCBaseRegister(), cg->hiValue(offset));
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, new (cg->trHeapMemory()) TR::MemoryReference(pivotRegister, LO_VALUE(offset), 8, cg));
            }
         else
            {
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), offset, 8, cg));
            }
         }
      else
         {
         loadAddressConstant(cg, cg->needRelocationsForLookupEvaluationData(), node, address, addrRegister);
         }
      }
   else
      {
      rel1 = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, pivotRegister, cg->hiValue(address));
      rel2 = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, addrRegister,
                           pivotRegister, LO_VALUE(address));

      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data3 = orderedPairSequence1;
      cg->getAheadOfTimeCompile()->getRelocationList().push_front(new (cg->trHeapMemory()) TR::PPCPairedRelocation(
                                     rel1,
                                     rel2,
                                     (uint8_t *)recordInfo,
                                     TR_AbsoluteMethodAddressOrderedPair,
                                     node));
      }

   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, lowRegister, 0);
   TR::LabelSymbol *backLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *searchLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *biggerLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *linkLabel = generateLabelSymbol(cg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, backLabel);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, pivotRegister, highRegister, lowRegister);
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, pivotRegister, pivotRegister, 31, 0xfffffffc);
   if (isInt64)
      {
      if (two_reg)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwzx, node, dataRegister->getHighOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, pivotRegister, 4, cg));
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, pivotRegister, pivotRegister, 4);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwzx, node, dataRegister->getLowOrder(), new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, pivotRegister, 4, cg));
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, pivotRegister, pivotRegister, -4);
         }
      else
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldx, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, pivotRegister, 8, cg));
         }
      }
   else
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwzx, node, dataRegister, new (cg->trHeapMemory()) TR::MemoryReference(addrRegister, pivotRegister, 4, cg));
      }

   if (isInt64)
      {
      if (two_reg)
         {
         generateTrg1Src2Instruction(cg, cmp_opcode, node, cndRegister, dataRegister->getHighOrder(), selector->getHighOrder());
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, searchLabel, cndRegister);
         generateTrg1Src2Instruction(cg, cmp_opcode, node, cndRegister, dataRegister->getLowOrder(), selector->getLowOrder());
         }
      else
         {
         cmp_opcode  = isUnsigned ? TR::InstOpCode::cmpl8 : TR::InstOpCode::cmp8;
         generateTrg1Src2Instruction(cg, cmp_opcode, node, cndRegister, dataRegister, selector);
         }
      }
   else
      {
      generateTrg1Src2Instruction(cg, cmp_opcode, node, cndRegister, dataRegister, selector);
      }

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, searchLabel, cndRegister);

   static bool disableNewLookupScheme4 = (feGetEnv("TR_DisableNewLookupScheme4") != NULL);
   if (disableNewLookupScheme4)
      {
      generateLabelInstruction(cg, TR::InstOpCode::bl, node, linkLabel);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, linkLabel);
      generateTrg1Instruction(cg, TR::InstOpCode::mflr, node, dataRegister);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, pivotRegister, pivotRegister, 20);
      }
   else
      {
      // New code
      cg->fixedLoadLabelAddressIntoReg(node, dataRegister, linkLabel);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, linkLabel);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, pivotRegister, pivotRegister, 16);
      }

   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, dataRegister, pivotRegister, dataRegister);
   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, dataRegister);
   generateInstruction(cg, TR::InstOpCode::bctr, node);
   for (int32_t ii=2; ii<total; ii++)
      {
      generateLabelInstruction(cg, TR::InstOpCode::b, node, node->getChild(ii)->getBranchDestination()->getNode()->getLabel());
      if ( isInt64 )
         dataTable64[ii-2] = node->getChild(ii)->getCaseConstant();
      else
         dataTable[ii-2] = node->getChild(ii)->getCaseConstant();
      }
   generateLabelInstruction(cg, TR::InstOpCode::label, node, searchLabel);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, biggerLabel, cndRegister);
   generateTrg1Src2Instruction(cg, cmp_opcode, node, cndRegister, pivotRegister, highRegister);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, node->getSecondChild()->getBranchDestination()->getNode()->getLabel(), cndRegister);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, lowRegister, pivotRegister, 4);
   generateLabelInstruction(cg, TR::InstOpCode::b, node, backLabel);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, biggerLabel);
   generateTrg1Src2Instruction(cg, cmp_opcode, node, cndRegister, pivotRegister, lowRegister);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, node->getSecondChild()->getBranchDestination()->getNode()->getLabel(), cndRegister);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, highRegister, pivotRegister, -4);

   generateDepLabelInstruction(cg, TR::InstOpCode::b, node, backLabel, conditions);

   cg->stopUsingRegister(cndRegister);
   cg->stopUsingRegister(addrRegister);
   cg->stopUsingRegister(dataRegister);
   cg->stopUsingRegister(lowRegister);
   cg->stopUsingRegister(highRegister);
   cg->stopUsingRegister(pivotRegister);
   }

TR::Register *OMR::Power::TreeEvaluator::lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   switchDispatch(node, false, cg);
   cg->decReferenceCount(node->getFirstChild());
   return(NULL);
   }

TR::Register *OMR::Power::TreeEvaluator::tableEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static const int32_t MIN_SIZE_FOR_TABLE_SWITCH = 8;

   int32_t numCases = node->getNumChildren() - 2;

   TR::Compilation *comp = cg->comp();
   TR::Register    *sReg     = cg->evaluate(node->getFirstChild());
   TR::Register    *reg1     = cg->allocateRegister();
   TR::Register    *ccReg    = cg->allocateRegister(TR_CCR);
   TR::Register    *tReg     = NULL;
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
   TR::LabelSymbol *table    = generateLabelSymbol(cg);

   TR::addDependency(conditions, reg1, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions,ccReg, TR::RealRegister::NoReg, TR_CCR, cg);
   TR::addDependency(conditions, sReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();

   TR::Node *secondChild = node->getSecondChild();
   if (secondChild->getNumChildren() > 0)
      {
      cg->evaluate(secondChild->getFirstChild());
      conditions = conditions->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }

   if (!node->isSafeToSkipTableBoundCheck())
      {
      if (numCases > UPPER_IMMED)
         {
         tReg = cg->allocateRegister();
         TR::addDependency(conditions, tReg, TR::RealRegister::NoReg, TR_GPR, cg);
         loadConstant(cg, node, numCases, tReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, ccReg, sReg, tReg);
         }
      else
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, ccReg, sReg, numCases);

      generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, secondChild->getBranchDestination()->getNode()->getLabel(), ccReg);
      }

      {
      if (cg->comp()->target().is64Bit())
         {
         int32_t offset = TR_PPCTableOfConstants::allocateChunk(1, cg);

         if (tReg == NULL)
            {
            tReg = cg->allocateRegister();
            TR::addDependency(conditions, tReg, TR::RealRegister::NoReg, TR_GPR, cg);
            }

         if (offset != PTOC_FULL_INDEX)
            {
            offset *= 8;
            cg->itemTracking(offset, table);
            if (offset<LOWER_IMMED||offset>UPPER_IMMED)
               {
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, reg1, cg->getTOCBaseRegister(), cg->hiValue(offset));
               generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, reg1, new (cg->trHeapMemory()) TR::MemoryReference(reg1, LO_VALUE(offset), 8, cg));
               }
            else
               {
               generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, reg1, new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), offset, 8, cg));
               }
            }
         else
            {
            cg->fixedLoadLabelAddressIntoReg(node, reg1, table, NULL, tReg);
            }

         generateShiftLeftImmediate(cg, node, tReg, sReg, 2);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, reg1, reg1, tReg);
         }
      else
         {
         generateShiftLeftImmediate(cg, node, reg1, sReg, 2);
         cg->fixedLoadLabelAddressIntoReg(node, reg1, table, NULL, tReg, true);
         }

      generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, reg1);
      generateInstruction(cg, TR::InstOpCode::bctr, node);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, table);
      int32_t i = 0;
      for (; i < numCases-1; ++i)
         generateLabelInstruction(cg, TR::InstOpCode::b, node, node->getChild(i+2)->getBranchDestination()->getNode()->getLabel());

      generateDepLabelInstruction(cg, TR::InstOpCode::b, node, node->getChild(i+2)->getBranchDestination()->getNode()->getLabel(), conditions);
      }

   if (tReg != NULL)
      cg->stopUsingRegister(tReg);
   cg->stopUsingRegister(reg1);
   cg->stopUsingRegister(ccReg);

   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }


TR::Instruction *
OMR::Power::TreeEvaluator::generateNullTestInstructions(
      TR::CodeGenerator *cg,
      TR::Register *trgReg,
      TR::Node *node,
      bool nullPtrSymRefRequired)
   {
   TR::Instruction *gcPoint = NULL;
   TR::Compilation *comp = cg->comp();

   if (cg->getHasResumableTrapHandler())
      {
      if (cg->comp()->target().is64Bit())
         gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::tdeqi, node, trgReg, NULLVALUE);
      else
         gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::tweqi, node, trgReg, NULLVALUE);
      cg->setCanExceptByTrap();
      }
   else
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      if (nullPtrSymRefRequired)
         {
         symRef = comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp->getMethodSymbol());
         }

      TR::LabelSymbol *snippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, symRef);
      TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
      TR::Register    *condReg = cg->allocateRegister(TR_CCR);
      TR::Register    *jumpReg = cg->allocateRegister();

      if (snippetLabel == NULL)
         {
         snippetLabel = generateLabelSymbol(cg);
         cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, symRef));
         }

      // trampoline kills gr11
      TR::addDependency(conditions, jumpReg, TR::RealRegister::gr11, TR_GPR, cg);
      if (cg->comp()->target().is64Bit())
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, trgReg, NULLVALUE);
      else
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, trgReg, NULLVALUE);
      if (cg->comp()->target().cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beql, PPCOpProp_BranchUnlikely, node, snippetLabel, condReg, conditions);
      else
         gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beql, node, snippetLabel, condReg, conditions);

      gcPoint->setExceptBranchOp();
      cg->stopUsingRegister(condReg);
      cg->stopUsingRegister(jumpReg);
      }

   return gcPoint;
   }

TR::Register *OMR::Power::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // NOTE: ZEROCHK is intended to be general and straightforward.  If you're
   // thinking of adding special code for specific situations in here, consider
   // whether you want to add your own CHK opcode instead.  If you feel the
   // need for special handling here, you may also want special handling in the
   // optimizer, in which case a separate opcode may be more suitable.
   //
   // On the other hand, if the improvements you're adding could benefit other
   // users of ZEROCHK, please go ahead and add them!
   //

   TR::LabelSymbol *slowPathLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *restartLabel  = generateLabelSymbol(cg);
   slowPathLabel->setStartInternalControlFlow();
   restartLabel->setEndInternalControlFlow();

   // Temporarily hide the first child so it doesn't appear in the outlined call
   //
   node->rotateChildren(node->getNumChildren()-1, 0);
   node->setNumChildren(node->getNumChildren()-1);

   // Outlined instructions for check failure
   //
   TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, slowPathLabel, restartLabel, cg);
   cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

   // Restore the first child
   //
   node->setNumChildren(node->getNumChildren()+1);
   node->rotateChildren(0, node->getNumChildren()-1);

   // Children other than the first are only for the outlined path; we don't need them here
   //
   for (int32_t i = 1; i < node->getNumChildren(); i++)
      cg->recursivelyDecReferenceCount(node->getChild(i));

   // Inline instructions for the check
   //
   TR::Node *valueToCheck = node->getFirstChild();

   if (valueToCheck->getOpCode().isBooleanCompare() &&
       valueToCheck->getChild(0)->getOpCode().isIntegerOrAddress() &&
       valueToCheck->getChild(1)->getOpCode().isIntegerOrAddress() &&
       performTransformation(cg->comp(), "O^O CODEGEN Optimizing ZEROCHK+%s %s\n", valueToCheck->getOpCode().getName(), valueToCheck->getName(cg->getDebug())))
      {
      if (valueToCheck->getOpCode().isCompareForOrder())
         {
         if (valueToCheck->getChild(0)->getOpCode().is8Byte() ||
             valueToCheck->getChild(1)->getOpCode().is8Byte())
            {
            // switch branches since we want to go to OOL on node evaluating to 0
            // which corresponds to an ifcmp fall-through
            TR::InstOpCode::Mnemonic branchOp = cmp2branch(valueToCheck->getOpCode().getOpCodeForReverseBranch(), cg);
            TR::InstOpCode::Mnemonic reverseBranchOp = cmp2branch(valueToCheck->getOpCodeValue(), cg);
            TR::TreeEvaluator::compareLongsForOrder(branchOp, reverseBranchOp, slowPathLabel, valueToCheck, cg,
                                 valueToCheck->getOpCode().isUnsignedCompare(), true, PPCOpProp_BranchUnlikely);
            }
         else
            {
            // switch branches since we want to go to OOL on node evaluating to 0
            // which corresponds to an ifcmp fall-through
            TR::TreeEvaluator::compareIntsForOrder(cmp2branch(valueToCheck->getOpCode().getOpCodeForReverseBranch(), cg),
                                slowPathLabel, valueToCheck, cg, valueToCheck->getOpCode().isUnsignedCompare(),
                                true, PPCOpProp_BranchUnlikely);
            }
         }
      else
         {
         TR_ASSERT(valueToCheck->getOpCode().isCompareForEquality(), "Compare opcode must either be compare for order or for equality");
         if (valueToCheck->getChild(0)->getOpCode().is8Byte() ||
             valueToCheck->getChild(1)->getOpCode().is8Byte())
            {
            // switch branches since we want to go to OOL on node evaluating to 0
            // which corresponds to an ifcmp fall-through
            TR::TreeEvaluator::compareLongsForEquality(cmp2branch(valueToCheck->getOpCode().getOpCodeForReverseBranch(), cg),
                                    slowPathLabel, valueToCheck, cg, true, PPCOpProp_BranchUnlikely);
            }
         else
            {
            TR::TreeEvaluator::compareIntsForEquality(cmp2branch(valueToCheck->getOpCode().getOpCodeForReverseBranch(), cg),
                                   slowPathLabel, valueToCheck, cg, true, PPCOpProp_BranchUnlikely);
            }
         }
      }
   else
      {
      TR::Register *value = cg->evaluate(node->getFirstChild());
      TR::Register *condReg = cg->allocateRegister(TR_CCR);
      if (cg->comp()->target().is64Bit())
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi8, node, condReg, value, 0);
      else
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, value, 0);
      if (cg->comp()->target().cpu.id() >= TR_PPCgp) // Use PPC AS branch hint.
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, node, slowPathLabel, condReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, slowPathLabel, condReg);

      cg->decReferenceCount(node->getFirstChild());
      cg->stopUsingRegister(condReg);
      }
   generateLabelInstruction(cg, TR::InstOpCode::label, node, restartLabel);

   return NULL;
   }

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (!cg->willGenerateNOPForVirtualGuard(node))
      {
      return false;
      }

   TR::Compilation *comp = cg->comp();
   TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);

   TR_VirtualGuardSite *site = NULL;

   if (cg->comp()->compileRelocatableCode())
      {
      site = (TR_VirtualGuardSite *)comp->addAOTNOPSite();
      TR_AOTGuardSite *aotSite = (TR_AOTGuardSite *)site;
      aotSite->setType(virtualGuard->getKind());
      aotSite->setNode(node);

      switch (virtualGuard->getKind())
         {
         case TR_DirectMethodGuard:
         case TR_NonoverriddenGuard:
         case TR_InterfaceGuard:
         case TR_AbstractGuard:
         case TR_MethodEnterExitGuard:
         case TR_HCRGuard:
            aotSite->setGuard(virtualGuard);
            break;

         case TR_ProfiledGuard:
            break;

         default:
            TR_ASSERT(0, "got AOT guard in node but virtual guard not one of known guards supported for AOT. Guard: %d", virtualGuard->getKind());
            break;
         }
      }
   else if (!node->isSideEffectGuard())
      {
      TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
      site = virtualGuard->addNOPSite();
      }
   else
      site = comp->addSideEffectNOPSite();

   TR::RegisterDependencyConditions *deps;
   if (node->getNumChildren() == 3)
      {
      TR::Node *third = node->getChild(2);
      cg->evaluate(third);
      deps = generateRegisterDependencyConditions(cg, third, 0);
      }
   else
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 0, cg->trMemory());

   if(virtualGuard->shouldGenerateChildrenCode())
      cg->evaluateChildrenWithMultipleRefCount(node);

   TR::LabelSymbol *label = node->getBranchDestination()->getNode()->getLabel();
   generateVirtualGuardNOPInstruction(cg, node, site, deps, label);
   cg->recursivelyDecReferenceCount(node->getFirstChild());
   cg->recursivelyDecReferenceCount(node->getSecondChild());

   return true;
#else
   return false;
#endif
   }

// generates max/min code for i, iu, l, lu, f, and d
static TR::Register *generateMaxMin(TR::Node *node, TR::CodeGenerator *cg, bool max)
   {
   TR::Node *child  = node->getFirstChild();
   TR::DataType data_type = child->getDataType();
   TR::DataType type = child->getType();
   TR::InstOpCode::Mnemonic move_op = type.isIntegral() ? TR::InstOpCode::mr : TR::InstOpCode::fmr;
   TR::InstOpCode::Mnemonic cmp_op;
   bool two_reg = (cg->comp()->target().is32Bit() && type.isInt64());
   TR::Register *trgReg;

   switch (node->getOpCodeValue())
      {
      case TR::imax:
      case TR::imin:
         cmp_op = TR::InstOpCode::cmp4;  break;
      case TR::iumax:
      case TR::iumin:
         cmp_op = TR::InstOpCode::cmpl4; break;
      case TR::lmax:
      case TR::lmin:
         cmp_op = TR::InstOpCode::cmp8;  break;
      case TR::lumax:
      case TR::lumin:
         cmp_op = TR::InstOpCode::cmpl8; break;
      case TR::fmax:
      case TR::fmin:
      case TR::dmax:
      case TR::dmin:
         cmp_op = TR::InstOpCode::fcmpu; break;
      default:       TR_ASSERT(false, "assertion failure");       break;
      }

   TR::Register *reg = cg->evaluate(child);

   if (cg->canClobberNodesRegister(child))
      {
      trgReg = reg; // use first child as a target
      }
   else
      {
      switch (data_type)
         {
         case TR::Int32:
            trgReg = cg->allocateRegister();
            break;
         case TR::Int64:
            if (two_reg)
               trgReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
            else
               trgReg = cg->allocateRegister();
            break;
         case TR::Float:
            trgReg = cg->allocateSinglePrecisionRegister();
            break;
         case TR::Double:
            trgReg = cg->allocateRegister(TR_FPR);
            break;
         default:
            TR_ASSERT(false, "assertion failure");
         }
      if (two_reg)
         {
         generateTrg1Src1Instruction(cg, move_op, node, trgReg->getLowOrder(), reg->getLowOrder());
         generateTrg1Src1Instruction(cg, move_op, node, trgReg->getHighOrder(), reg->getHighOrder());
         }
      else
         {
         generateTrg1Src1Instruction(cg, move_op, node, trgReg, reg);
         }
      }

   int n = node->getNumChildren();
   for (int i = 1; i < n; i++)
      {
      TR::Node *child = node->getChild(i);
      TR::Register *reg = cg->evaluate(child);
      TR::Register *condReg = cg->allocateRegister(TR_CCR);
      TR::LabelSymbol *label = generateLabelSymbol(cg);
      TR::RegisterDependencyConditions *dep;
      if (two_reg)
         {
         TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
            generateControlFlowInstruction(cg, TR::InstOpCode::iflong, node, NULL /*deps */);
         cfop->addTargetRegister(condReg);
         cfop->addSourceRegister(trgReg->getHighOrder());
         cfop->addSourceRegister(trgReg->getLowOrder());
         cfop->addSourceRegister(reg->getHighOrder());
         cfop->addSourceRegister(reg->getLowOrder());
         cfop->setLabelSymbol(label);
         cfop->setOpCode2Value(max ? TR::InstOpCode::bge : TR::InstOpCode::ble);
         generateTrg1Src1Instruction(cg, move_op, node, trgReg->getLowOrder(), reg->getLowOrder());
         generateTrg1Src1Instruction(cg, move_op, node, trgReg->getHighOrder(), reg->getHighOrder());

         dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
         dep->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(reg->getLowOrder(), TR::RealRegister::NoReg);
         dep->addPostCondition(reg->getHighOrder(), TR::RealRegister::NoReg);
         }
      else
         {
         generateTrg1Src2Instruction(cg, cmp_op, node, condReg, trgReg, reg);
         generateConditionalBranchInstruction(cg, max ? TR::InstOpCode::bge : TR::InstOpCode::ble, node, label, condReg);
         generateTrg1Src1Instruction(cg, move_op, node, trgReg, reg);

         dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());
         dep->addPostCondition(trgReg, TR::RealRegister::NoReg);
         dep->addPostCondition(reg, TR::RealRegister::NoReg);
         }
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, label, dep);
      cg->stopUsingRegister(condReg);
      }

   node->setRegister(trgReg);
   for (int i = 0; i < n; i++)
      {
      cg->decReferenceCount(node->getChild(i));
      }
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::maxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return generateMaxMin(node, cg, true);
   }

TR::Register *OMR::Power::TreeEvaluator::minEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return generateMaxMin(node, cg, false);
   }

TR::Register *OMR::Power::TreeEvaluator::igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()>=1, "at least one child expected for igoto");

   TR::Node *labelAddr = node->getFirstChild();
   TR::Register *addrReg = cg->evaluate(labelAddr);
   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() > 1)
      {
      TR_ASSERT(node->getNumChildren() == 2 && node->getChild(1)->getOpCodeValue() == TR::GlRegDeps, "igoto has maximum of two children and second one must be global register dependency");
      TR::Node *glregdep = node->getChild(1);
      cg->evaluate(glregdep);
      deps = generateRegisterDependencyConditions(cg, glregdep, 0);
      cg->decReferenceCount(glregdep);
      }
   cg->machine()->setLinkRegisterKilled(true);
   generateSrc1Instruction(cg, TR::InstOpCode::mtlr, node, addrReg, 0);
   if (deps)
      generateDepInstruction(cg, TR::InstOpCode::blr, node, deps);
   else
      generateInstruction(cg, TR::InstOpCode::blr, node);
   cg->decReferenceCount(labelAddr);
   node->setRegister(NULL);
   return NULL;
   }

//******************************************************************************
// evaluator for ificmno, ificmnno, ificmpo, ificmpno, iflcmno, iflcmnno, iflcmpo, iflcmpno
TR::Register *OMR::Power::TreeEvaluator::ifxcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR_ASSERT((opCode == TR::ificmno) || (opCode == TR::ificmnno) ||
             (opCode == TR::iflcmno) || (opCode == TR::iflcmnno) ||
             (opCode == TR::ificmpo) || (opCode == TR::ificmpno) ||
             (opCode == TR::iflcmpo) || (opCode == TR::iflcmpno), "invalid opcode");

   bool nodeIs64Bit = node->getFirstChild()->getSize() == 8;
   bool reverseBranch = (opCode == TR::ificmnno) || (opCode == TR::iflcmnno) || (opCode == TR::ificmpno) || (opCode == TR::iflcmpno);

   TR::Register *rs1 = cg->evaluate(node->getFirstChild());
   TR::Register *rs2 = cg->evaluate(node->getSecondChild());
   TR::Register *res = cg->allocateRegister();
   TR::Register *tmp = cg->allocateRegister();
   TR::Register *cr  = cg->allocateRegister(TR_CCR);

   // calculating overflow manually is faster than the XERov route

   if ((opCode == TR::ificmno) || (opCode == TR::ificmnno) || (opCode == TR::iflcmno) || (opCode == TR::iflcmnno))
      {
      generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, res, rs1, rs2); // compare negative
      generateTrg1Src2Instruction(cg, TR::InstOpCode::eqv, node, tmp, rs1, rs2);
      }
   else
      {
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, res, rs2, rs1); // compare
      generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR,  node, tmp, rs2, rs1);
      }

   generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, res, res, rs1);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, res, res, tmp);
   generateTrg1Src1ImmInstruction(cg, nodeIs64Bit ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4, node, cr, res, 0);

   TR::LabelSymbol *dstLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::InstOpCode::Mnemonic branchOp = reverseBranch ? TR::InstOpCode::bge : TR::InstOpCode::blt;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, cr, generateRegisterDependencyConditions(cg, thirdChild, 0));
      cg->decReferenceCount(thirdChild);
      }
   else
      {
      generateConditionalBranchInstruction(cg, branchOp, node, dstLabel, cr);
      }

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(res);
   cg->stopUsingRegister(tmp);
   cg->stopUsingRegister(cr);
   return NULL;
   }
