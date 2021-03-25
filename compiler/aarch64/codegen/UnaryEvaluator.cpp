/*******************************************************************************
 * Copyright (c) 2018, 2021 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

// Common evaluator for integer xconst
static TR::Register *commonConstEvaluator(TR::Node *node, int32_t value, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->allocateRegister();
   loadConstant32(cg, node, value, tempReg);
   return node->setRegister(tempReg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getInt(), cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getShortInt(), cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getByte(), cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->profiledPointersRequireRelocation() &&
         (node->isMethodPointerConstant() || node->isClassPointerConstant()))
      {
      TR::Register *trgReg = node->setRegister(cg->allocateRegister());
      TR_ExternalRelocationTargetKind reloKind = TR_NoRelocation;
      if (node->isMethodPointerConstant())
         {
         reloKind = TR_MethodPointer;
         if (node->getInlinedSiteIndex() == -1)
            reloKind = TR_RamMethod;
         }
      else if (node->isClassPointerConstant())
         reloKind = TR_ClassPointer;

      TR_ASSERT(reloKind != TR_NoRelocation, "relocation kind shouldn't be TR_NoRelocation");
      loadAddressConstantInSnippet(cg, node, node->getAddress(), trgReg, reloKind);

      return trgReg;
      }
   TR::Compilation *comp = cg->comp();
   TR_ResolvedMethod *method = comp->getCurrentMethod();
   bool isPicSite = (node->isClassPointerConstant() && cg->fe()->isUnloadAssumptionRequired(reinterpret_cast<TR_OpaqueClassBlock *>(node->getAddress()), method)) ||
                     (node->isMethodPointerConstant() && cg->fe()->isUnloadAssumptionRequired(
                        cg->fe()->createResolvedMethod(cg->trMemory(), reinterpret_cast<TR_OpaqueMethodBlock *>(node->getAddress()), method)->classOfMethod(), method));

   if (isPicSite)
      {
      TR::Register *trgReg = node->setRegister(cg->allocateRegister());
      loadAddressConstantInSnippet(cg, node, node->getAddress(), trgReg, TR_NoRelocation, true);
      return trgReg;
      }

   TR::Register *tempReg = node->setRegister(cg->allocateRegister());
   loadConstant64(cg, node, node->getAddress(), tempReg, NULL);
   return tempReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->allocateRegister();
   loadConstant64(cg, node, node->getLongInt(), tempReg);
   return node->setRegister(tempReg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::inegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *reg = cg->gprClobberEvaluate(firstChild);
   generateNegInstruction(cg, node, reg, reg);
   node->setRegister(reg);
   cg->decReferenceCount(firstChild);
   return reg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *tempReg = cg->gprClobberEvaluate(firstChild);
   generateNegInstruction(cg, node, tempReg, tempReg, true);
   node->setRegister(tempReg);
   cg->decReferenceCount(firstChild);
   return tempReg;
   }

static TR::Register *commonIntegerAbsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *reg = cg->gprClobberEvaluate(firstChild);
   TR::Register *tempReg = cg->allocateRegister();

   bool is64bit = node->getDataType().isInt64();
   TR::InstOpCode::Mnemonic eorOp = is64bit ? TR::InstOpCode::eorx : TR::InstOpCode::eorw;
   TR::InstOpCode::Mnemonic subOp = is64bit ? TR::InstOpCode::subx : TR::InstOpCode::subw;

   generateArithmeticShiftRightImmInstruction(cg, node, tempReg, reg, is64bit ? 63 : 31, is64bit);
   generateTrg1Src2Instruction(cg, eorOp, node, reg, reg, tempReg);
   generateTrg1Src2Instruction(cg, subOp, node, reg, reg, tempReg);

   cg->stopUsingRegister(tempReg);
   node->setRegister(reg);
   cg->decReferenceCount(firstChild);
   return reg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonIntegerAbsEvaluator(node, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::labsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonIntegerAbsEvaluator(node, cg);
   }

static TR::Register *commonUnaryHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::Register *trgReg = (child->getReferenceCount() == 1) ? srcReg : cg->allocateRegister();

   generateTrg1Src1Instruction(cg, op, node, trgReg, srcReg);
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::sbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = commonUnaryHelper(node, TR::InstOpCode::rev16w, cg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, node, trgReg, trgReg, 15); // sign extension
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::ibyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonUnaryHelper(node, TR::InstOpCode::revw, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::lbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonUnaryHelper(node, TR::InstOpCode::revx, cg);
   }

/*
 * highest one bit
 */
static TR::Register *hbitHelper(TR::Node *node, bool is64bit, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *tmpReg = cg->allocateRegister();
   TR::InstOpCode::Mnemonic op;

   op = is64bit ? TR::InstOpCode::clzx : TR::InstOpCode::clzw;
   generateTrg1Src1Instruction(cg, op, node, tmpReg, srcReg);
   generateCompareImmInstruction(cg, node, srcReg, 0, is64bit);
   if (is64bit)
      {
      // mov trgReg, #0x80000000
      generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, trgReg, 0x8000 | TR::MOV_LSL48);
      }
   else
      {
      // mov trgReg, #0x8000000000000000
      generateTrg1ImmInstruction(cg, TR::InstOpCode::movzw, node, trgReg, 0x8000 | TR::MOV_LSL16);
      }
   op = is64bit ? TR::InstOpCode::cselx : TR::InstOpCode::cselw;
   generateCondTrg1Src2Instruction(cg, op, node, trgReg, trgReg, srcReg, TR::CC_NE);
   op = is64bit ? TR::InstOpCode::lsrvx : TR::InstOpCode::lsrvw;
   generateTrg1Src2Instruction(cg, op, node, trgReg, trgReg, tmpReg);

   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::ihbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return hbitHelper(node, false, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::lhbitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return hbitHelper(node, true, cg);
   }

/*
 * number of leading zeros
 */
TR::Register *OMR::ARM64::TreeEvaluator::inolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonUnaryHelper(node, TR::InstOpCode::clzw, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::lnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonUnaryHelper(node, TR::InstOpCode::clzx, cg);
   }

/*
 * number of trailing zeros
 */
static TR::Register *notzHelper(TR::Node *node, bool is64bit, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::Register *trgReg = (child->getReferenceCount() == 1) ? srcReg : cg->allocateRegister();
   TR::InstOpCode::Mnemonic op;

   op = is64bit ? TR::InstOpCode::rbitx : TR::InstOpCode::rbitw;
   generateTrg1Src1Instruction(cg, op, node, trgReg, srcReg);
   op = is64bit ? TR::InstOpCode::clzx : TR::InstOpCode::clzw;
   generateTrg1Src1Instruction(cg, op, node, trgReg, trgReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::inotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return notzHelper(node, false, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::lnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return notzHelper(node, true, cg);
   }

// also handles i2b, i2s, l2b, l2s, s2b
TR::Register *OMR::ARM64::TreeEvaluator::l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

static TR::Register *extendToIntOrLongHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, uint32_t imms, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::Register *trgReg = (child->getReferenceCount() == 1) ? srcReg : cg->allocateRegister();

   // signed extension: alias of SBFM
   // unsigned extension: alias of UBFM
   TR_ASSERT(imms < 32, "Extension size too big");
   generateTrg1Src1ImmInstruction(cg, op, node, trgReg, srcReg, imms);

   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);
   if (child->getOpCodeValue() == TR::bload || child->getOpCodeValue() == TR::bloadi)
      {
      // No sign extension needed.
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, node, trgReg, trgReg, 7);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::s2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);
   if (child->getOpCodeValue() == TR::sload || child->getOpCodeValue() == TR::sloadi)
      {
      // No sign extension needed.
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, node, trgReg, trgReg, 15);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::sbfmx, 7, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::sbfmx, 15, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::sbfmx, 31, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();

   if (child->getReferenceCount() == 1
       && (child->getOpCodeValue() == TR::bload || child->getOpCodeValue() == TR::bloadi)
       && child->getRegister() == NULL)
      {
      // Use unsigned load
      TR::Register *trgReg = commonLoadEvaluator(child, TR::InstOpCode::ldrbimm, cg);
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   else
      {
      return extendToIntOrLongHelper(node, TR::InstOpCode::ubfmw, 7, cg);
      }
   }

TR::Register *OMR::ARM64::TreeEvaluator::su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();

   if (child->getReferenceCount() == 1
       && (child->getOpCodeValue() == TR::sload || child->getOpCodeValue() == TR::sloadi)
       && child->getRegister() == NULL)
      {
      // Use unsigned load
      TR::Register *trgReg = commonLoadEvaluator(child, TR::InstOpCode::ldrhimm, cg);
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   else
      {
      return extendToIntOrLongHelper(node, TR::InstOpCode::ubfmw, 15, cg);
      }
   }

TR::Register *OMR::ARM64::TreeEvaluator::bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::ubfmx, 7, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::ubfmx, 15, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);

   if (child->getOpCodeValue() == TR::iload || child->getOpCodeValue() == TR::iloadi)
      {
      // No need for zero extension
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmx, node, trgReg, trgReg, 31);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::l2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (comp->useCompressedPointers())
      {
      // pattern match the sequence under the l2a
      //    iaload f      l2a                       <- node
      //       aload O       ladd
      //                       lshl
      //                          i2l
      //                            iiload f        <- load
      //                               aload O
      //                          iconst shftKonst
      //                       lconst HB
      //
      // -or- if the load is known to be null
      //  l2a
      //    i2l
      //      iiload f
      //         aload O
      //
      TR::Node *firstChild = node->getFirstChild();
      TR::Register *source = cg->evaluate(firstChild);

      if ((firstChild->containsCompressionSequence() || (TR::Compiler->om.compressedReferenceShift() == 0)) && !node->isl2aForCompressedArrayletLeafLoad())
         source->setContainsCollectedReference();

      node->setRegister(source);
      cg->decReferenceCount(firstChild);

      return source;
      }
   else
      return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }
