/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

TR::Register *OMR::ARM64::TreeEvaluator::cconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getConst<uint16_t>(), cg);
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
   firstChild->decReferenceCount();
   return node->setRegister(reg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *tempReg = cg->gprClobberEvaluate(firstChild);
   generateNegInstruction(cg, node, tempReg, tempReg, true);
   firstChild->decReferenceCount();
   return node->setRegister(tempReg);
   }

static TR::Register *commonIntegerAbsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *reg = cg->gprClobberEvaluate(firstChild);
   TR::Register *tempReg = cg->allocateRegister();

   bool is64bit = node->getDataType().isInt64();
   TR::InstOpCode::Mnemonic eorOp = is64bit ? TR::InstOpCode::eorx : TR::InstOpCode::eorw;
   TR::InstOpCode::Mnemonic subOp = is64bit ? TR::InstOpCode::subx : TR::InstOpCode::subw;

   generateArithmeticShiftRightImmInstruction(cg, node, tempReg, reg, is64bit ? 63 : 31);
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
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);

   // signed extension: alias of SBFM
   // unsigned extension: alias of UBFM
   TR_ASSERT(imms < 32, "Extension size too big");
   generateTrg1Src1ImmInstruction(cg, op, node, trgReg, trgReg, imms);

   node->setRegister(trgReg);
   child->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM64::TreeEvaluator::b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::sbfmw, 7, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::s2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::sbfmw, 15, cg);
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
   return extendToIntOrLongHelper(node, TR::InstOpCode::ubfmw, 7, cg);
   }

TR::Register *OMR::ARM64::TreeEvaluator::su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, TR::InstOpCode::ubfmw, 15, cg);
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
   return extendToIntOrLongHelper(node, TR::InstOpCode::ubfmx, 31, cg);
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
