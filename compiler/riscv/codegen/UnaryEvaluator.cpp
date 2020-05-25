/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "codegen/RVInstruction.hpp"
#include "codegen/CodeGenerator.hpp"
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

TR::Register *OMR::RV::TreeEvaluator::iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getInt(), cg);
   }

TR::Register *OMR::RV::TreeEvaluator::sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getShortInt(), cg);
   }

TR::Register *OMR::RV::TreeEvaluator::bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getByte(), cg);
   }

TR::Register *OMR::RV::TreeEvaluator::cconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getConst<uint16_t>(), cg);
   }

TR::Register *OMR::RV::TreeEvaluator::aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->allocateRegister();
   intptr_t address = node->getLongInt();
   loadConstant64(cg, node, address, tempReg);
   return node->setRegister(tempReg);
   }

TR::Register *OMR::RV::TreeEvaluator::lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->allocateRegister();
   loadConstant64(cg, node, node->getLongInt(), tempReg);
   return node->setRegister(tempReg);
   }

// also handles lneg
TR::Register *OMR::RV::TreeEvaluator::inegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *reg = cg->gprClobberEvaluate(firstChild);
   TR::Register *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);

   generateRTYPE(TR::InstOpCode::_sub, node, reg, zero, reg, cg);

   firstChild->decReferenceCount();
   return node->setRegister(reg);
   }

static TR::Register *commonIntegerAbsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *reg = cg->gprClobberEvaluate(firstChild);
   TR::Register *tempReg  = cg->allocateRegister();
   bool is64bit = node->getDataType().isInt64();

   generateITYPE(TR::InstOpCode::_srai, node, tempReg,  reg, is64bit ? 63 : 31, cg);
   generateRTYPE(TR::InstOpCode::_xor,  node, reg, reg, tempReg, cg);
   generateRTYPE(TR::InstOpCode::_sub,  node, reg, reg, tempReg, cg);

   cg->stopUsingRegister(tempReg);
   node->setRegister(reg);
   cg->decReferenceCount(firstChild);
   return reg;
   }

TR::Register *OMR::RV::TreeEvaluator::iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonIntegerAbsEvaluator(node, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::labsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonIntegerAbsEvaluator(node, cg);
   }

// also handles i2b, i2s, l2b, l2s
TR::Register *OMR::RV::TreeEvaluator::l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

static TR::Register *extendToIntOrLongHelper(TR::Node *node, uint32_t imms, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);

   node->setRegister(trgReg);
   child->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::RV::TreeEvaluator::b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 7, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::s2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 15, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 7, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 15, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 31, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 7, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 15, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 7, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 15, cg);
   }

TR::Register *OMR::RV::TreeEvaluator::iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return extendToIntOrLongHelper(node, 31, cg);
   }
