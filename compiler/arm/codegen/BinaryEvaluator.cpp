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

#include "codegen/AheadOfTimeCompile.hpp"
#include "arm/codegen/ARMInstruction.hpp"
#include "arm/codegen/ARMOperand2.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/ARMAOTRelocation.hpp"
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Bit.hpp"

static void mulConstant(TR::Node *node, TR::Register *trgReg, TR::Register *sourceReg, int32_t value, TR::CodeGenerator *cg);

static TR::Register *addConstantToInteger(TR::Node *node, TR::Register *srcReg, int32_t value, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;
   uint32_t base, rotate;
   intParts localVal(value);

   trgReg = cg->allocateRegister();

   if((localVal.getValue() < 0) && (constantIsImmed8r(-localVal.getValue(), &base, &rotate)))
      {
      generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, trgReg, srcReg, base, rotate);
      }
   else if (constantIsImmed8r(localVal.getValue(), &base, &rotate))
      {
      generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, trgReg, srcReg, base, rotate);
      }
   else
      {
      TR::Register *tmpReg = cg->allocateRegister();
      armLoadConstant(node, localVal.getValue(), tmpReg, cg);
      generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, srcReg, tmpReg);
      }
   return trgReg;
   }

// Also handles TR::aiadd
TR::Register *OMR::ARM::TreeEvaluator::iaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *src1Reg = NULL;
   TR::Register *trgReg = NULL;
   TR::Node     *secondChild = node->getSecondChild();

   TR::Node *firstChild = node->getFirstChild();
   src1Reg = cg->evaluate(firstChild);
   TR::ILOpCodes secondOp = secondChild->getOpCodeValue();

   if (secondOp == TR::iconst &&
       secondChild->getRegister() == NULL)
      {
      trgReg = addConstantToInteger(node, src1Reg, secondChild->getInt(), cg);
      }
   else if (!(trgReg = ishiftAnalyser(node,cg,ARMOp_add)))
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      trgReg = cg->allocateRegister();
      generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, src1Reg, src2Reg);
      }

   if ((node->getOpCodeValue() == TR::aiadd) &&
       node->isInternalPointer())
      {
      if (node->getPinningArrayPointer())
         {
         trgReg->setContainsInternalPointer();
         trgReg->setPinningArrayPointer(node->getPinningArrayPointer());
         }
      else
         {
         TR::Node *firstChild = node->getFirstChild();
         if ((firstChild->getOpCodeValue() == TR::aload) &&
             firstChild->getSymbolReference()->getSymbol()->isAuto() &&
             firstChild->getSymbolReference()->getSymbol()->isPinningArrayPointer())
            {
            trgReg->setContainsInternalPointer();

            if (!firstChild->getSymbolReference()->getSymbol()->isInternalPointer())
               {
               trgReg->setPinningArrayPointer(firstChild->getSymbolReference()->getSymbol()->castToAutoSymbol());
               //firstChild->getSymbolReference()->getSymbol()->setPinningArrayPointer();
               }
            else
               trgReg->setPinningArrayPointer(firstChild->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }
         else if (firstChild->getRegister() &&
                  firstChild->getRegister()->containsInternalPointer())
            {
            trgReg->setContainsInternalPointer();
            trgReg->setPinningArrayPointer(firstChild->getRegister()->getPinningArrayPointer());
            }
         }
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

static TR::RegisterPair *addConstantToLong(TR::Node *node, TR::Register *srcHighReg, TR::Register *srcLowReg,
                               int32_t highValue, int32_t lowValue, TR::CodeGenerator *cg)
   {
   TR::Register *lowReg  = cg->allocateRegister();
   TR::Register *highReg = cg->allocateRegister();
   TR::RegisterPair *trgReg = cg->allocateRegisterPair(lowReg, highReg);
   uint32_t base, rotate;

   // TODO: optimize for negative #s

   // add low half
   if (constantIsImmed8r(lowValue, &base, &rotate))
      {
      generateTrg1Src1ImmInstruction(cg, ARMOp_add_r, node, lowReg, srcLowReg, base, rotate);
      }
   else   // constant won't fit
      {
      TR::Register *lowValueReg = cg->allocateRegister();
      armLoadConstant(node, lowValue, lowValueReg, cg);
      generateTrg1Src2Instruction(cg, ARMOp_add_r, node, lowReg, srcLowReg, lowValueReg);
      }

   // add high half
   if (constantIsImmed8r(highValue, &base, &rotate))
      {
      generateTrg1Src1ImmInstruction(cg, ARMOp_adc, node, highReg, srcHighReg, base, rotate);
      }
   else
      {
      TR::Register *highValueReg = cg->allocateRegister();
      armLoadConstant(node, highValue, highValueReg, cg);
      generateTrg1Src2Instruction(cg, ARMOp_adc, node, highReg, srcHighReg, highValueReg);
      }
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::laddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node         *secondChild = node->getSecondChild();
   TR::Node         *firstChild  = node->getFirstChild();
   TR::RegisterPair *trgReg      = NULL;
   TR::Register     *src1Reg     = cg->evaluate(firstChild);

   if (/* !isSmall() && */ secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      trgReg = addConstantToLong(node, src1Reg->getHighOrder(),
                                 src1Reg->getLowOrder(),
                                 secondChild->getLongIntHigh(),
                                 secondChild->getLongIntLow(),
                                 cg);
      }
   else
      {
      TR::Register *lowReg  = cg->allocateRegister();
      TR::Register *highReg = cg->allocateRegister();
      trgReg = cg->allocateRegisterPair(lowReg, highReg);
      TR::Register  *src2Reg = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, ARMOp_add_r, node, lowReg, src1Reg->getLowOrder(), src2Reg->getLowOrder());
      generateTrg1Src2Instruction(cg, ARMOp_adc, node, highReg, src1Reg->getHighOrder(), src2Reg->getHighOrder());
      }
   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();

   return trgReg;
   }

// aiaddEvaluator handled by iaddEvaluator

TR::Register *OMR::ARM::TreeEvaluator::isubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Node     *firstChild = node->getFirstChild();
   TR::Register *trgReg = NULL;
   TR::Register *src1Reg = NULL;
   uint32_t base, rotate;
   int32_t value;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      src1Reg = cg->evaluate(firstChild);
      value = secondChild->getInt();
      trgReg = addConstantToInteger(node, src1Reg , -value, cg);
      }
   else if(firstChild->getOpCode().isLoadConst() &&
             firstChild->getRegister() == NULL && constantIsImmed8r(firstChild->getInt(), &base, &rotate) )
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      trgReg = cg->allocateRegister();
      generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, trgReg, src2Reg, base, rotate);
      }
   else
      {
      if ((trgReg = ishiftAnalyser(node,cg,ARMOp_sub)) != NULL)
         {
         src1Reg = firstChild->getRegister();
         }
      else
         {
         trgReg = cg->allocateRegister();
         src1Reg = cg->evaluate(firstChild);
         generateTrg1Src2Instruction(cg, ARMOp_sub, node, trgReg, src1Reg, cg->evaluate(secondChild));
         }

      if (src1Reg != NULL) //src1Reg may be null if ishiftAnalyzer combined firstChild's children.
         {
         // the logic below is borrowed from PPC code.
         if (src1Reg->containsInternalPointer() ||
             !src1Reg->containsCollectedReference())
            {
            if (src1Reg->containsInternalPointer())
               {
               trgReg->setPinningArrayPointer(src1Reg->getPinningArrayPointer());
               trgReg->setContainsInternalPointer();
               }
            }
         else
            trgReg->setContainsCollectedReference();
         }
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Register *trgReg = NULL;

   union  {
      int64_t longValue;
      struct  {
         int32_t lowValue; // ENDIAN
         int32_t highValue;
      } x;
   } longVal;

   if (/* !isSmall() && */ secondChild->getOpCodeValue() == TR::lconst &&
       secondChild->getRegister() == NULL)
      {
      TR::Register *src1Reg   = cg->evaluate(firstChild);
      if (cg->comp()->target().cpu.isBigEndian())
         {
         longVal.x.highValue = secondChild->getLongIntLow();
         longVal.x.lowValue = secondChild->getLongIntHigh();
         }
      else
         {
         longVal.x.lowValue = secondChild->getLongIntLow();
         longVal.x.highValue = secondChild->getLongIntHigh();
         }
      longVal.longValue = -longVal.longValue;   // negate the constant
      trgReg = addConstantToLong(node, src1Reg->getHighOrder(), src1Reg->getLowOrder(),
                                         longVal.x.highValue, longVal.x.lowValue,
                                         cg);
      }
   else
      {
      // TODO: add (const - var) version back (or make
      // addConstantToLong do it)
      TR::Register *lowReg  = cg->allocateRegister();
      TR::Register *highReg = cg->allocateRegister();
      TR::Register *src1Reg   = cg->evaluate(firstChild);
      TR::Register *src2Reg   = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, ARMOp_sub_r, node, lowReg, src1Reg->getLowOrder(), src2Reg->getLowOrder());
      generateTrg1Src2Instruction(cg, ARMOp_sbc, node, highReg, src1Reg->getHighOrder(), src2Reg->getHighOrder());
      trgReg = cg->allocateRegisterPair(lowReg, highReg);
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::imulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg          = cg->allocateRegister();
   TR::Node     *firstChild         = node->getFirstChild();
   TR::Node     *secondChild        = node->getSecondChild();
   TR::Register *src1Reg            = cg->evaluate(firstChild);
   if (secondChild->getOpCodeValue() == TR::iconst )
      {
      int32_t value = secondChild->getInt();
      if (value > 0 && cg->convertMultiplyToShift(node))
         {
         // The multiply has been converted to a shift.
         // Note that we have restricted this to positive constant multipliers.
         // We can do it for negative ones too, but then the result of the shift
         // operation needs to be negated here after the shift node is evaluated.
         // The test above then becomes "if (value != 0 && ..."
         //
         trgReg = cg->evaluate(node);
         //
         // Add a negate here if the constant was negative
         //
         return(trgReg);
         }
      else
         mulConstant(node, trgReg, src1Reg, value, cg);
      }
   else  // no constants
      {
      generateTrg1Src2MulInstruction(cg, ARMOp_mul, node, trgReg, src1Reg, cg->evaluate(secondChild));
      }
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;

   }

TR::Register *OMR::ARM::TreeEvaluator::imulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg        = cg->allocateRegister();
   TR::Register *tempRegLow    = cg->allocateRegister();
   TR::Node     *firstChild    = node->getFirstChild();
   TR::Node     *secondChild   = node->getSecondChild();
   TR::Register *src1Reg       = cg->evaluate(firstChild);

   // imulh is generated for constant idiv and the second child is the magic number
   // assume magic number is usually a large odd number with little optimization opportunity
   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      TR::Register *magicReg = cg->allocateRegister();
      int32_t value = (int32_t)secondChild->get64bitIntegralValue();
      armLoadConstant(node, value, magicReg, cg);
      // want bigger value in source1 register, this will usually be the magic number register
      generateTrg2Src2MulInstruction(cg, ARMOp_smull, node, trgReg, tempRegLow, magicReg, src1Reg);
      cg->stopUsingRegister(magicReg);
      }
   else
      {
      generateTrg2Src2MulInstruction(cg, ARMOp_smull, node, trgReg, tempRegLow, cg->evaluate(secondChild), src1Reg);
      }

   cg->stopUsingRegister(tempRegLow);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // todo - re-inline a lot of this.
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *arg1 = cg->evaluate(firstChild);
   TR::Register *arg2 = cg->evaluate(secondChild);
   TR::Register *dd_lowReg, *dr_lowReg;
   TR::Register *dd_highReg, *dr_highReg;
   TR::Register *trgReg, *highReg, *lowReg;
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());

   if (firstChild->getReferenceCount() > 1)
      {
      dd_lowReg = cg->allocateRegister();
      dd_highReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dd_lowReg, arg1->getLowOrder());
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dd_highReg, arg1->getHighOrder());
      }
   else
      {
      dd_lowReg = arg1->getLowOrder();
      dd_highReg = arg1->getHighOrder();
      }

   if (secondChild->getReferenceCount() > 1)
      {
      dr_lowReg = cg->allocateRegister();
      dr_highReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dr_lowReg, arg2->getLowOrder());
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dr_highReg, arg2->getHighOrder());
      }
   else
      {
      dr_lowReg = arg2->getLowOrder();
      dr_highReg = arg2->getHighOrder();
      }

   highReg = cg->allocateRegister();
   lowReg = cg->allocateRegister();
   trgReg = cg->allocateRegisterPair(lowReg, highReg);

   if(cg->comp()->target().cpu.isBigEndian())
      {
      dependencies->addPreCondition(dd_lowReg, TR::RealRegister::gr1);
      dependencies->addPreCondition(dd_highReg, TR::RealRegister::gr0);
      dependencies->addPreCondition(dr_lowReg, TR::RealRegister::gr3);
      dependencies->addPreCondition(dr_highReg, TR::RealRegister::gr2);
      dependencies->addPostCondition(lowReg, TR::RealRegister::gr1);
      dependencies->addPostCondition(highReg, TR::RealRegister::gr0);
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr2);
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr3);
      }
   else
      {
      dependencies->addPreCondition(dd_lowReg, TR::RealRegister::gr0);
      dependencies->addPreCondition(dd_highReg, TR::RealRegister::gr1);
      dependencies->addPreCondition(dr_lowReg, TR::RealRegister::gr2);
      dependencies->addPreCondition(dr_highReg, TR::RealRegister::gr3);
      dependencies->addPostCondition(lowReg, TR::RealRegister::gr0);
      dependencies->addPostCondition(highReg, TR::RealRegister::gr1);
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr2);
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr3);
      }

   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlongMultiply, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlongMultiply, false, false, false));

   cg->machine()->setLinkRegisterKilled(true);
   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }


static TR::Register *signedIntegerDivisionOrRemainderAnalyser(TR::Node *node, int32_t divisor, TR::Register *dividendReg, bool isDivide, TR::CodeGenerator *cg)
   {
   if (divisor == 1)
      {
      if (isDivide)
         {
         return dividendReg;
         }
      else
         {
         TR::Register *trgReg = cg->allocateRegister();
         armLoadConstant(node, 0, trgReg, cg);
         return trgReg;
         }
      }

   if (divisor == -1)
      {
      TR::Register *trgReg = cg->allocateRegister();
      if (isDivide)
         {
         generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, trgReg, dividendReg, 0, 0);
         }
      else
         {
         armLoadConstant(node, 0, trgReg, cg);
         }
      return trgReg;
      }

   if (isPowerOf2(divisor))
      {
      if (!isDivide)
         {
         TR::Register *trgReg = cg->allocateRegister();
         TR::Instruction *instr;
         TR::Instruction *instr2;
         uint32_t base, rotate;
         if(constantIsImmed8r(divisor-1, &base, &rotate))  // if divisor - 1 fits in 8 bit imm field can do this cheaply
            {
            generateTrg1Src1ImmInstruction(cg, ARMOp_and_r, node, trgReg, dividendReg, base, rotate);
            }
         else
            {
            armLoadConstant(node, divisor-1, trgReg, cg);
            generateTrg1Src2Instruction(cg, ARMOp_and_r, node, trgReg, dividendReg, trgReg);
            }

         // Check the sign of dividend when the result of and is non-zero
         instr = generateSrc1ImmInstruction(cg, ARMOp_cmp, node, dividendReg, 0, 0);
         instr->setConditionCode(ARMConditionCodeNE);

         // if dividend is negative then modulus result is negative, condition codes set by cmp above
         if(constantIsImmed8r(divisor, &base, &rotate))  // if divisor fits in 8 bit imm field can do this cheaply
            {
            instr2 = generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, trgReg, trgReg, base, rotate);
            instr2->setConditionCode(ARMConditionCodeMI);
            }
         else
            {
            TR::Register *temp1Reg = cg->allocateRegister();
            armLoadConstant(node, divisor, temp1Reg, cg);
            instr2 = generateTrg1Src2Instruction(cg, ARMOp_sub, node, trgReg, trgReg, temp1Reg);
            instr2->setConditionCode(ARMConditionCodeMI);
            }
         return trgReg;
         }
      else // isDivide
         {
         bool isNegative = false;

         if (isNonPositivePowerOf2(divisor))
            {
            divisor = -divisor;
            isNegative = true;
            }

         uint32_t base, rotate;
         if(!constantIsImmed8r(divisor-1, &base, &rotate))
            {
            return NULL;  // TODO: fix to allow more powers of 2
            }

         TR::Instruction *instr;
         TR::Register *temp1Reg = cg->allocateRegister();

         generateTrg1Src1Instruction(cg, ARMOp_mov_r, node, temp1Reg, dividendReg);
         instr = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, temp1Reg, temp1Reg, base, rotate);
         instr->setConditionCode(ARMConditionCodeMI);
         generateShiftRightArithmeticImmediate(cg, node, temp1Reg, temp1Reg, trailingZeroes(divisor));

         // (x / -2) is same as (x / 2) * -1
         if(isNegative)
            {
            generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, temp1Reg, temp1Reg, 0, 0);
            }
         return temp1Reg;
         }
      }
   else
      {
      int32_t     magicNumber, shiftAmount;
      TR::Register *magicReg = cg->allocateRegister();
      TR::Register *temp1Reg = cg->allocateRegister();
      TR::Register *temp2Reg = cg->allocateRegister();

      cg->compute32BitMagicValues(divisor, &magicNumber, &shiftAmount);
      armLoadConstant(node, magicNumber, magicReg, cg);
      // temp2Reg (low half) not needed and will be overridden below
      // want bigger value in source1 register, this will usually be the magic number register
      generateTrg2Src2MulInstruction(cg, ARMOp_smull, node, temp1Reg, temp2Reg, magicReg, dividendReg);

      if ( (divisor > 0) && (magicNumber < 0) )
         {
         temp2Reg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_add, node, temp2Reg, dividendReg, temp1Reg);
         }
      else if ( (divisor < 0) && (magicNumber > 0) )
         {
         temp2Reg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_rsb,node, temp2Reg, dividendReg, temp1Reg);
         }
      else
         temp2Reg = temp1Reg;

      TR::Register *temp3Reg = cg->allocateRegister();
      TR::Register *temp4Reg = cg->allocateRegister();

      // must check if shift == 0 because ARM assembler converts ASR #0 into ASR #32
      if(shiftAmount == 0)
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, temp3Reg, temp2Reg);
      else
         generateShiftRightArithmeticImmediate(cg, node, temp3Reg, temp2Reg, shiftAmount);

      if (divisor > 0)
         {
         TR_ARMOperand2 *shiftOp = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, dividendReg, 31);
         generateTrg1Src2Instruction(cg, ARMOp_add, node, temp4Reg, temp3Reg, shiftOp);
         }
      else
         {
         TR_ARMOperand2 *shiftOp = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, temp3Reg, 31);
         generateTrg1Src2Instruction(cg,ARMOp_add,node,temp4Reg,temp3Reg,shiftOp);
         }

      if (!isDivide) // is irem
         {
         TR::Register *temp5Reg = cg->allocateRegister();
         TR::Register *temp6Reg = cg->allocateRegister();
         mulConstant(node, temp5Reg, temp4Reg, divisor, cg);
         generateTrg1Src2Instruction(cg, ARMOp_rsb, node, temp6Reg, temp5Reg, dividendReg);
         return temp6Reg;
         }
      else
         {
         return temp4Reg;
         }
      }
   return NULL;
   }

static TR::Register *idivAndIRemHelper(TR::Node *node, bool isDivide, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *dividend     = cg->evaluate(firstChild);
   TR::Register *trgReg;

   if (secondChild->getOpCode().isLoadConst())
      {
      trgReg = signedIntegerDivisionOrRemainderAnalyser(node, secondChild->getInt(), dividend, isDivide, cg);
      if(trgReg)
         {
         node->setRegister(trgReg);
         firstChild->decReferenceCount();
         secondChild->decReferenceCount();
         return trgReg;
         }
      }

   // can't be done cheaply, call helper
   TR::Register *dd_reg, *dr_reg;
   TR::Register *divisor = cg->evaluate(secondChild);
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());

   if (firstChild->getReferenceCount() > 1)
      {
      dd_reg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dd_reg, dividend);
      }
   else
      {
      dd_reg = dividend;
      }

   if (secondChild->getReferenceCount() > 1)
      {
      dr_reg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dr_reg, divisor);
      }
   else
      {
      dr_reg = divisor;
      }

   TR::addDependency(dependencies, dd_reg, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(dependencies, dr_reg, TR::RealRegister::gr1, TR_GPR, cg);

   TR::SymbolReference *helper = cg->symRefTab()->findOrCreateRuntimeHelper(isDivide ? TR_ARMintDivide : TR_ARMintRemainder, false, false, false);

   generateImmSymInstruction(cg, ARMOp_bl,
                              node, (uintptr_t)helper->getMethodAddress(),
                              dependencies,
                              helper);

   trgReg = dd_reg;

   cg->machine()->setLinkRegisterKilled(true);
   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }


static TR::Register *ldivAndLRemHelper(TR::Node *node, bool isDivide, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *dividend = cg->evaluate(firstChild);
   TR::Register *divisor = cg->evaluate(secondChild);
   TR::Register *dd_lowReg, *dr_lowReg;
   TR::Register *dd_highReg, *dr_highReg;
   TR::Register *trgReg, *highReg, *lowReg;
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());

   if (firstChild->getReferenceCount() > 1)
      {
      dd_lowReg = cg->allocateRegister();
      dd_highReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dd_lowReg, dividend->getLowOrder());
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dd_highReg, dividend->getHighOrder());
      }
   else
      {
      dd_lowReg = dividend->getLowOrder();
      dd_highReg = dividend->getHighOrder();
      }

   if (secondChild->getReferenceCount() > 1)
      {
      dr_lowReg = cg->allocateRegister();
      dr_highReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dr_lowReg, divisor->getLowOrder());
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dr_highReg, divisor->getHighOrder());
      }
   else
      {
      dr_lowReg = divisor->getLowOrder();
      dr_highReg = divisor->getHighOrder();
      }

   highReg = cg->allocateRegister();
   lowReg = cg->allocateRegister();
   trgReg = cg->allocateRegisterPair(lowReg, highReg);

   if(cg->comp()->target().cpu.isBigEndian())
      {
      dependencies->addPreCondition(dd_lowReg, TR::RealRegister::gr1);
      dependencies->addPreCondition(dd_highReg, TR::RealRegister::gr0);
      dependencies->addPreCondition(dr_lowReg, TR::RealRegister::gr3);
      dependencies->addPreCondition(dr_highReg, TR::RealRegister::gr2);

      if(isDivide)
         {
         dependencies->addPostCondition(lowReg, TR::RealRegister::gr1);
         dependencies->addPostCondition(highReg, TR::RealRegister::gr0);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr2);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr3);
         }
      else
         {
         dependencies->addPostCondition(lowReg, TR::RealRegister::gr3);
         dependencies->addPostCondition(highReg, TR::RealRegister::gr2);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr0);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);
         }
      }
   else
      {
      dependencies->addPreCondition(dd_lowReg, TR::RealRegister::gr0);
      dependencies->addPreCondition(dd_highReg, TR::RealRegister::gr1);
      dependencies->addPreCondition(dr_lowReg, TR::RealRegister::gr2);
      dependencies->addPreCondition(dr_highReg, TR::RealRegister::gr3);

      if(isDivide)
         {
         dependencies->addPostCondition(lowReg, TR::RealRegister::gr0);
         dependencies->addPostCondition(highReg, TR::RealRegister::gr1);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr2);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr3);
         }
      else
         {
         dependencies->addPostCondition(lowReg, TR::RealRegister::gr2);
         dependencies->addPostCondition(highReg, TR::RealRegister::gr3);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr0);
         dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);
         }
      }

   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlongDivide, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlongDivide, false, false, false));

   cg->machine()->setLinkRegisterKilled(true);
   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::idivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return idivAndIRemHelper(node, true, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return idivAndIRemHelper(node, false, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ldivAndLRemHelper(node, true, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ldivAndLRemHelper(node, false, cg);
   }

static TR::Register *ishiftEvaluator(TR::Node *node, TR_ARMOperand2Type shiftTypeImmed, TR_ARMOperand2Type shiftTypeReg, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild  = node->getFirstChild();
   TR::Node        *secondChild = node->getSecondChild();
   TR::Register    *src1Reg     = cg->evaluate(firstChild);
   TR::Register    *trgReg;
   TR_ARMOperand2 *op;

   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      if ((secondChild->getInt() & 0x1f) == 0)
         {
         if (firstChild->getReferenceCount() == 1)
            {
            trgReg = src1Reg;
            }
         else
            {
            trgReg = cg->allocateRegister();
            op = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, src1Reg);
            }
         }
      else
         {
         trgReg = cg->allocateRegister();
         op = new (cg->trHeapMemory()) TR_ARMOperand2(shiftTypeImmed, src1Reg, secondChild->getInt() & 0x1f);
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      trgReg = cg->allocateRegister();
      op = new (cg->trHeapMemory()) TR_ARMOperand2(0x1f, 0);
      generateTrg1Src2Instruction(cg, ARMOp_and, node, trgReg, shiftAmountReg, op);
      op = new (cg->trHeapMemory()) TR_ARMOperand2(shiftTypeReg, src1Reg, trgReg);
      }

   if (trgReg != src1Reg)
      {
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, op);
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::ishlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ishiftEvaluator(node, ARMOp2RegLSLImmed, ARMOp2RegLSLReg, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegisterPair(cg->allocateRegister(),
                                                       cg->allocateRegister());
   TR::Node     *secondChild = node->getSecondChild();
   TR::Node     *firstChild  = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *srcLowReg = srcReg->getLowOrder();
   TR::Register *srcHighReg = srcReg->getHighOrder();

   if (/* !isSmall() && */ secondChild->getOpCodeValue() == TR::iconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t shiftAmount = secondChild->getInt() & 0x3f;
      if (shiftAmount < 32)
         {
         TR::Register *temp1Reg = node->setRegister(cg->allocateRegister());
         TR::Register *temp2Reg = node->setRegister(cg->allocateRegister());
         generateShiftLeftImmediate(cg, node, temp1Reg, srcHighReg, shiftAmount);
         generateShiftLeftImmediate(cg, node, trgReg->getLowOrder(), srcLowReg, shiftAmount);
         generateShiftRightLogicalImmediate(cg, node, temp2Reg, srcLowReg, 32-shiftAmount);
         generateTrg1Src2Instruction(cg, ARMOp_orr, node, trgReg->getHighOrder(), temp1Reg, temp2Reg);
         }
      else  // shiftAmount >= 32
         {
         generateShiftLeftImmediate(cg, node, trgReg->getHighOrder(), srcLowReg, shiftAmount-32);
         generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), 0, 0);
         }
      }
   else
      {
      TR::Register *temp1Reg = cg->allocateRegister();
      TR::Register *temp2Reg = cg->allocateRegister();
      TR::Register *temp3Reg = cg->allocateRegister();
      TR::Register *temp4Reg = cg->allocateRegister();
      TR::Register *temp5Reg = cg->allocateRegister();
      TR::Register *temp6Reg = cg->allocateRegister();
      TR::Register *shiftAmountReg = cg->allocateRegister();
      generateTrg1Src1ImmInstruction(cg, ARMOp_and, node, shiftAmountReg, cg->evaluate(secondChild), 0x3f, 0);
      generateShiftLeftByRegister(cg, node, temp1Reg, srcHighReg, shiftAmountReg);
      generateShiftLeftByRegister(cg, node, trgReg->getLowOrder(), srcLowReg, shiftAmountReg);
      generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, temp2Reg, shiftAmountReg, 32, 0);
      generateShiftRightLogicalByRegister(cg, node, temp3Reg, srcLowReg, temp2Reg);
      generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, temp4Reg, shiftAmountReg, 32, 0);
      generateShiftLeftByRegister(cg, node, temp5Reg, srcLowReg, temp4Reg);
      generateTrg1Src2Instruction(cg, ARMOp_orr, node, temp6Reg, temp3Reg, temp5Reg);  // TODO: push this into above op.
      generateTrg1Src2Instruction(cg, ARMOp_orr, node, trgReg->getHighOrder(), temp1Reg, temp6Reg);
      }
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);

   return trgReg;
   }


TR::Register *OMR::ARM::TreeEvaluator::ishrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ishiftEvaluator(node, ARMOp2RegASRImmed, ARMOp2RegASRReg, cg);
   }

static TR::Register *longRightShiftEvaluator(TR::Node *node, bool isLogical, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegisterPair(cg->allocateRegister(),
                                                       cg->allocateRegister());
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *srcReg         = cg->evaluate(firstChild);

   // is it shift by constant?
   if (secondChild->getOpCodeValue() == TR::iconst && secondChild->getRegister() == NULL)
      {
      TR::Register *srcLowReg      = srcReg->getLowOrder();
      TR::Register *srcHighReg     = srcReg->getHighOrder();
      int32_t shiftAmount = secondChild->getInt()&0x3f;
      TR_ARMOperand2Type shiftType = isLogical ? ARMOp2RegLSRImmed : ARMOp2RegASRImmed;
      if (shiftAmount >= 32)
         {
         if(isLogical)
            {
            generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), 0, 0);
            }
         else
            {
            new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(ARMOp_mov, node, trgReg->getHighOrder(), new (cg->trHeapMemory()) TR_ARMOperand2(shiftType, srcHighReg, 31), cg);
            }
         if (shiftAmount == 32)
            {
            generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), srcHighReg);
            }
         else
            {
            new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(ARMOp_mov, node, trgReg->getLowOrder(), new (cg->trHeapMemory()) TR_ARMOperand2(shiftType, srcHighReg, shiftAmount-32), cg);
            }
         }
      else // (shiftAmount < 32)
         {
         TR::Register *temp1Reg         = cg->allocateRegister();
         TR::Register *temp2Reg         = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), new (cg->trHeapMemory()) TR_ARMOperand2(shiftType, srcHighReg, shiftAmount));
         generateShiftRightLogicalImmediate(cg, node, temp1Reg, srcLowReg, shiftAmount);
         generateShiftLeftImmediate(cg, node, temp2Reg, srcHighReg, 32-shiftAmount);
         generateTrg1Src2Instruction(cg, ARMOp_orr, node, trgReg->getLowOrder(), temp1Reg, temp2Reg);
         }
      }
   else
      {
      int32_t srcLowValue  = -1;
      int32_t srcHighValue = -1;
      TR::Register *srcLowReg      = NULL;
      TR::Register *srcHighReg     = NULL;

      // is it constant shifted by register?
      if (firstChild->getOpCodeValue() == TR::iconst && firstChild->getRegister() == NULL)
         {
         srcLowValue = firstChild->getLongIntLow();
         srcHighValue = firstChild->getLongIntHigh();
         if (srcLowValue != 0)
            armLoadConstant(node, srcLowValue, srcLowReg, cg);
         if (srcHighValue != 0)
            armLoadConstant(node, srcHighValue, srcHighReg, cg);
         }
      else
         {
         srcLowReg  = srcReg->getLowOrder();
         srcHighReg = srcReg->getHighOrder();
         }

      TR::Register *shiftAmountReg = cg->allocateRegister();
      generateTrg1Src1ImmInstruction(cg, ARMOp_and, node, shiftAmountReg, cg->evaluate(secondChild), 0x3f, 0);

      if (srcHighValue == 0)
         {
         generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), 0, 0);
         if(srcLowValue == 0)
            generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), 0, 0);
         else
            generateShiftRightByRegister(cg, node, trgReg->getLowOrder(), srcLowReg, shiftAmountReg, isLogical);
         }
      else
         {
         // do an out of line call for full shift by register.
         TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());

         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), srcLowReg);
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), srcHighReg);

         TR::addDependency(dependencies, trgReg->getLowOrder(), TR::RealRegister::gr0, TR_GPR, cg);
         TR::addDependency(dependencies, trgReg->getHighOrder(), TR::RealRegister::gr1, TR_GPR, cg);
         TR::addDependency(dependencies, shiftAmountReg, TR::RealRegister::gr2, TR_GPR, cg);
         TR::SymbolReference *longShiftHelper = cg->symRefTab()->findOrCreateRuntimeHelper(isLogical ? TR_ARMlongShiftRightLogical : TR_ARMlongShiftRightArithmetic, false, false, false);

         generateImmSymInstruction(cg, ARMOp_bl,
                                   node, (uintptr_t)longShiftHelper->getMethodAddress(),
                                   dependencies,
                                   longShiftHelper);
         cg->machine()->setLinkRegisterKilled(true);
         }
      }
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::lshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return longRightShiftEvaluator(node, false, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ishiftEvaluator(node, ARMOp2RegLSRImmed, ARMOp2RegLSRReg, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return longRightShiftEvaluator(node, true, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::irolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==2, "Wrong number of children in inlineIntegerRotateLeft");

   TR::Node        *firstChild = node->getFirstChild();
   TR::Node        *secondChild = node->getSecondChild();
   TR::Register    *srcReg = cg->evaluate(firstChild);
   TR::Register    *trgReg = cg->allocateRegister();

   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      int32_t value = secondChild->getInt() & 0x1F;
      if (value != 0)
         {
         // Use ROR (Rotate Right) Operand2
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegRORImmed, srcReg, 32-value));
         }
      else
         {
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, srcReg);
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, trgReg, shiftAmountReg, 1, 5); // Subtract from 32 (1 << 5)
      // Use ROR (Rotate Right) Operand2
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegRORReg, srcReg, trgReg));
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::lrolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==2, "Wrong number of children in inlineIntegerRotateLeft");

   TR::Node        *firstChild = node->getFirstChild();
   TR::Node        *secondChild = node->getSecondChild();
   TR::Register    *srcReg = cg->evaluate(firstChild);
   TR::Register    *srcLowReg = srcReg->getLowOrder();
   TR::Register    *srcHighReg = srcReg->getHighOrder();
   TR::Register    *lowReg = cg->allocateRegister();
   TR::Register    *highReg = cg->allocateRegister();
   TR::Register    *trgReg = cg->allocateRegisterPair(lowReg, highReg);

   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      int32_t value = secondChild->getInt() & 0x3F;
      if (value == 0)
         {
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, lowReg, srcLowReg);
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, highReg, srcHighReg);
         }
      else if (value == 32)
         {
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, lowReg, srcHighReg);
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, highReg, srcLowReg);
         }
      else if (value < 32)
         {
         // Not tested
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, lowReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, srcLowReg, value));
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, highReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, srcHighReg, value));
         generateTrg1Src2Instruction(cg, ARMOp_orr, node, lowReg, lowReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, srcHighReg, 32-value));
         generateTrg1Src2Instruction(cg, ARMOp_orr, node, highReg, highReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, srcLowReg, 32-value));
         }
      else // value > 32
         {
         // Not tested
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, lowReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, srcHighReg, value-32));
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, highReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, srcLowReg, value-32));
         generateTrg1Src2Instruction(cg, ARMOp_orr, node, lowReg, lowReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, srcLowReg, 64-value));
         generateTrg1Src2Instruction(cg, ARMOp_orr, node, highReg, highReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, srcHighReg, 64-value));
         }
      }
   else
      {
      TR_UNIMPLEMENTED();
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

// Help evaluating lor, lxor, land
static TR::Register *ibooleanTypeImmediateEvaluator(TR::Node *srcNode, TR_ARMOpCodes regOp, TR::Register *srcReg, int32_t imm,
                                                  TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = NULL;
   uint32_t base, rotate;

   if (srcNode->getReferenceCount() <= 1 && (regOp == ARMOp_and && imm == -1 || regOp != ARMOp_and && imm == 0))
      { // an operand of a long logical operations can have a half word which is an identity value for that operation
      trgReg = srcReg;
      }
   else if (constantIsImmed8r(imm, &base, &rotate))
      {
      trgReg = cg->allocateRegister();
      generateTrg1Src1ImmInstruction(cg, regOp, srcNode, trgReg, srcReg, base, rotate);
      }
   else if (regOp == ARMOp_and && imm == 0xffff)
      {
      trgReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, srcNode, trgReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, srcReg, 16));
      generateTrg1Src1Instruction(cg, ARMOp_mov, srcNode, trgReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, trgReg, 16));
      }
   return trgReg;
   }

// Do the work for evaluating lor, lxor, land
static inline TR::Register *lbooleanTypeEvaluator(TR::Node *node,
                                                 TR_ARMOpCodes regOp, TR::CodeGenerator *cg)
   {
   TR::Node *     firstChild  = node->getFirstChild();
   TR::Register * src1Reg     = cg->evaluate(firstChild);
   TR::Node *     secondChild = node->getSecondChild();
   TR::Register * trgReg, * trgLow, * trgHigh;
   TR::Register * src2Reg;

   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      int64_t imm64 = secondChild->getLongInt();
      if (!(trgLow = ibooleanTypeImmediateEvaluator(firstChild, regOp, src1Reg->getLowOrder(), (int32_t)imm64, cg)))
         {
         trgLow = cg->allocateRegister();
         armLoadConstant(node, (int32_t)imm64, trgLow, cg);
         new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(regOp, node, trgLow, src1Reg->getLowOrder(), trgLow, cg);
         }
      if (!(trgHigh = ibooleanTypeImmediateEvaluator(firstChild, regOp, src1Reg->getHighOrder(), (int32_t)(imm64>>32), cg)))
         {
         trgHigh = cg->allocateRegister();
         armLoadConstant(node, (int32_t)(imm64>>32), trgHigh, cg);
         new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(regOp, node, trgHigh, src1Reg->getHighOrder(), trgHigh, cg);
         }
      trgReg = cg->allocateRegisterPair(trgLow, trgHigh);
      }
   else
      {
      trgReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      src2Reg = cg->evaluate(secondChild);

      new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(regOp, node, trgReg->getLowOrder(), src2Reg->getLowOrder(), src1Reg->getLowOrder(), cg);
      new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(regOp, node, trgReg->getHighOrder(), src2Reg->getHighOrder(), src1Reg->getHighOrder(), cg);
      }
   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return lbooleanTypeEvaluator(node, ARMOp_and, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return lbooleanTypeEvaluator(node, ARMOp_orr, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return lbooleanTypeEvaluator(node, ARMOp_eor, cg);
   }

// called by ior,ixor,iand,iadd,isub to look for opportunities to combine a shift into these operations.
static TR::Register *ishiftAnalyser(TR::Node *node, TR::CodeGenerator *cg,TR_ARMOpCodes regOp)
   {
   TR_ARMOperand2 *shiftOp;
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *trgReg = NULL;
   bool canCombine = false;
   bool shiftIsFirstChild = false;
   TR_ARMOperand2Type shiftTypeImmed,shiftTypeReg;
   TR::Node *mulSecondChild;

   if((secondChild->getOpCode().isLeftShift() || secondChild->getOpCode().isRightShift() ||  secondChild->getOpCode().isShiftLogical()) &&
             !firstChild->getOpCode().isLoadConst() && secondChild->getReferenceCount() == 1 && secondChild->getRegister() == NULL )
      {
      canCombine = true;
      shiftIsFirstChild = false;
      }
   else if(secondChild->getOpCode().isMul() &&  secondChild->getOpCode().isInt() &&
            !firstChild->getOpCode().isLoadConst() && secondChild->getReferenceCount() == 1 && secondChild->getRegister() == NULL )
      {
      mulSecondChild = secondChild->getSecondChild();
      if(mulSecondChild->getOpCodeValue() == TR::iconst && mulSecondChild->getInt() > 0 && cg->convertMultiplyToShift(secondChild))
         {
         canCombine = true;
         shiftIsFirstChild = false;
         }
      }
   else if((firstChild->getOpCode().isLeftShift() || firstChild->getOpCode().isRightShift() ||  firstChild->getOpCode().isShiftLogical()) &&
           !secondChild->getOpCode().isLoadConst() && firstChild->getReferenceCount() == 1 && firstChild->getRegister() == NULL)
      {
      canCombine = true;
      shiftIsFirstChild = true;
      if(regOp == ARMOp_sub)
         regOp = ARMOp_rsb;
      }
    else if(firstChild->getOpCode().isMul() &&  firstChild->getOpCode().isInt() &&
             !secondChild->getOpCode().isLoadConst() && firstChild->getReferenceCount() == 1 && firstChild->getRegister() == NULL )
      {
      mulSecondChild = firstChild->getSecondChild();
      if(mulSecondChild->getOpCodeValue() == TR::iconst && mulSecondChild->getInt() > 0 && cg->convertMultiplyToShift(firstChild))
         {
         canCombine = true;
         shiftIsFirstChild = true;
         if(regOp == ARMOp_sub)
            regOp = ARMOp_rsb;
         }
      }

   if(canCombine)
      {
      TR::Node *shiftNode         = shiftIsFirstChild ? node->getFirstChild():node->getSecondChild();
      TR::Node *shiftFirstChild   = shiftNode->getFirstChild();
      TR::Node *shiftSecondChild  = shiftNode->getSecondChild();
      TR::Register *src1Reg       = cg->evaluate(shiftFirstChild);

      trgReg = cg->allocateRegister();

      switch(shiftNode->getOpCodeValue())
         {
         case TR::ishl:
            shiftTypeImmed = ARMOp2RegLSLImmed;
            shiftTypeReg = ARMOp2RegLSLReg;
            break;
         case TR::iushr:
            shiftTypeImmed = ARMOp2RegLSRImmed;
            shiftTypeReg = ARMOp2RegLSRReg;
            break;
         case TR::ishr:
            shiftTypeImmed = ARMOp2RegASRImmed;
            shiftTypeReg = ARMOp2RegASRReg;
            break;
         }

      if (shiftSecondChild->getOpCodeValue() == TR::iconst)
         {
         shiftOp = new (cg->trHeapMemory()) TR_ARMOperand2(shiftTypeImmed, src1Reg, shiftSecondChild->getInt() & 0x1f);
         }
      else
         {
         TR::Register *shiftAmountReg = cg->evaluate(shiftSecondChild);
         generateTrg1Src2Instruction(cg, ARMOp_and, node, trgReg, shiftAmountReg, new (cg->trHeapMemory()) TR_ARMOperand2(0x1f,0));
         shiftOp = new (cg->trHeapMemory()) TR_ARMOperand2(shiftTypeReg, src1Reg, trgReg);
         }

      if(shiftIsFirstChild)
         generateTrg1Src2Instruction(cg, regOp, node, trgReg, cg->evaluate(secondChild), shiftOp);
      else
         generateTrg1Src2Instruction(cg, regOp, node, trgReg, cg->evaluate(firstChild), shiftOp);

      shiftFirstChild->decReferenceCount();
      shiftSecondChild->decReferenceCount();
      }

   return trgReg;
   }

// Do the work for evaluating ior, ixor, iand
static inline TR::Register *ibooleanTypeEvaluator(TR::Node *node,
                                                 TR_ARMOpCodes regOp, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg         = NULL;
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Node     *firstChild     = node->getFirstChild();

   auto comp = cg->comp();

   if (comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "In ibooleanTypeEvaluator for n%dn (%p)\n", node->getGlobalIndex(), node);
      }

   uint32_t base, rotate;

  if (secondChild->getOpCodeValue() == TR::iconst &&
      secondChild->getRegister() == NULL && constantIsImmed8r(secondChild->getInt(), &base, &rotate))
      {
      TR::Node *firstChild = node->getFirstChild();
      trgReg = cg->allocateRegister();
      generateTrg1Src1ImmInstruction(cg, regOp, node, trgReg, cg->evaluate(firstChild), base, rotate);
      }
   else if (trgReg = ishiftAnalyser(node,cg,regOp))
      {
      }
   else if(regOp == ARMOp_and && secondChild->getOpCodeValue() == TR::iconst &&
                     secondChild->getRegister() == NULL && secondChild->getInt() == 0xffff)
      {
      trgReg = cg->allocateRegister();
      TR::Register *src = cg->evaluate(firstChild);
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, src, 16));
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, trgReg, 16));
      }
   else
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      trgReg = cg->allocateRegister();
      generateTrg1Src2Instruction(cg, regOp, node, trgReg, cg->evaluate(firstChild), src2Reg);
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::iandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ibooleanTypeEvaluator(node, ARMOp_and, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ibooleanTypeEvaluator(node, ARMOp_orr, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return ibooleanTypeEvaluator(node, ARMOp_eor, cg);
   }

// Multiply a register by a constant
static void mulConstant(TR::Node *node, TR::Register *trgReg, TR::Register *sourceReg, int32_t value, TR::CodeGenerator *cg)
   {
   if (value == 0)
      {
      armLoadConstant(node, 0, trgReg, cg);
      }
   else if (value == 1)
      {
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, sourceReg);
      }
   else if (value == -1)
      {
      generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, trgReg, sourceReg, 0, 0);
      }
   else if (isNonNegativePowerOf2(value))
      {
      generateShiftLeftImmediate(cg, node, trgReg, sourceReg, trailingZeroes(value));
      }
   else if (isNonPositivePowerOf2(value))
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediate(cg, node, tempReg, sourceReg, trailingZeroes(value));
      generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, trgReg, tempReg, 0, 0);
      }
   else if (isNonNegativePowerOf2(value-1))
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediate(cg, node, tempReg, sourceReg, trailingZeroes(value-1));
      generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, tempReg, sourceReg);
      }
   else if (isNonNegativePowerOf2(value+1))
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateShiftLeftImmediate(cg, node, tempReg, sourceReg, trailingZeroes(value+1));
      generateTrg1Src2Instruction(cg, ARMOp_sub, node, trgReg, tempReg, sourceReg);
      }
   else   // constant won't fit
      {
      TR::Register *tempReg = cg->allocateRegister();
      armLoadConstant(node, value, tempReg, cg);
      generateTrg1Src2MulInstruction(cg, ARMOp_mul, node, trgReg, sourceReg, tempReg);
      }
   }
