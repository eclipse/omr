/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64ShiftCode.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Bit.hpp"

static TR::Register *addOrSubInteger(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *trgReg = cg->allocateRegister();
   bool isAdd = node->getOpCode().isAdd();

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int32_t value = secondChild->getInt();
      if (constantIsUnsignedImm12(value))
         {
         generateTrg1Src1ImmInstruction(cg, isAdd ? TR::InstOpCode::addimmw : TR::InstOpCode::subimmw, node, trgReg, src1Reg, value);
         }
      else
         {
         TR::Register *tmpReg = cg->allocateRegister();
         loadConstant32(cg, node, value, tmpReg);
         generateTrg1Src2Instruction(cg, isAdd ? TR::InstOpCode::addw : TR::InstOpCode::subw, node, trgReg, src1Reg, tmpReg);
         cg->stopUsingRegister(tmpReg);
         }
      }
   else
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, isAdd ? TR::InstOpCode::addw : TR::InstOpCode::subw, node, trgReg, src1Reg, src2Reg);
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return addOrSubInteger(node, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::laddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::laddEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::isubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return addOrSubInteger(node, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lsubEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

// Multiply a register by a 32-bit constant
static void mulConstant32(TR::Node *node, TR::Register *treg, TR::Register *sreg, int32_t value, TR::CodeGenerator *cg)
   {
   if (value == 0)
      {
      loadConstant32(cg, node, 0, treg);
      }
   else if (value == 1)
      {
      generateMovInstruction(cg, node, treg, sreg);
      }
   else if (value == -1)
      {
      generateNegInstruction(cg, node, treg, sreg);
      }
   else
      {
      TR::Register *tmpReg = cg->allocateRegister();
      loadConstant32(cg, node, value, tmpReg);
      generateMulInstruction(cg, node, treg, sreg, tmpReg);
      cg->stopUsingRegister(tmpReg);
      }
   }

TR::Register *
OMR::ARM64::TreeEvaluator::imulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *trgReg;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int32_t value = secondChild->getInt();
      if (value > 0 && cg->convertMultiplyToShift(node))
         {
         // The multiply has been converted to a shift.
         trgReg = cg->evaluate(node);
         return trgReg;
         }
      else
         {
         trgReg = cg->allocateRegister();
         mulConstant32(node, trgReg, src1Reg, value, cg);
         }
      }
   else
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      trgReg = cg->allocateRegister();
      generateMulInstruction(cg, node, trgReg, src1Reg, src2Reg);
      }
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::imulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::imulhEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::lmulEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static TR::Register *idivHelper(TR::Node *node, bool is64bit, TR::CodeGenerator *cg)
   {
   // TODO: Add checks for special cases

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg = cg->allocateRegister();

   generateTrg1Src2Instruction(cg, is64bit ? TR::InstOpCode::sdivx : TR::InstOpCode::sdivw, node, trgReg, src1Reg, src2Reg);

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

static TR::Register *iremHelper(TR::Node *node, bool is64bit, TR::CodeGenerator *cg)
   {
   // TODO: Add checks for special cases

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *tmpReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegister();

   generateTrg1Src2Instruction(cg, is64bit ? TR::InstOpCode::sdivx : TR::InstOpCode::sdivw, node, tmpReg, src1Reg, src2Reg);
   generateTrg1Src3Instruction(cg, is64bit ? TR::InstOpCode::msubx : TR::InstOpCode::msubw, node, trgReg, tmpReg, src2Reg, src1Reg);

   cg->stopUsingRegister(tmpReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::idivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return idivHelper(node, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return iremHelper(node, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return idivHelper(node, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return iremHelper(node, true, cg);
   }

static TR::Register *shiftHelper(TR::Node *node, TR::ARM64ShiftCode shiftType, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *trgReg = cg->allocateRegister();
   bool is64bit = node->getDataType().isInt64();
   TR::InstOpCode::Mnemonic op;

   if (secondOp == TR::iconst || secondOp == TR::iuconst)
      {
      int32_t value = secondChild->getInt();
      uint32_t shift = is64bit ? (value & 0x3F) : (value & 0x1F);
      switch (shiftType)
         {
         case TR::SH_LSL:
            generateLogicalShiftLeftImmInstruction(cg, node, trgReg, srcReg, shift);
            break;
         case TR::SH_LSR:
            generateLogicalShiftRightImmInstruction(cg, node, trgReg, srcReg, shift);
            break;
         case TR::SH_ASR:
            generateArithmeticShiftRightImmInstruction(cg, node, trgReg, srcReg, shift);
            break;
         default:
            TR_ASSERT(false, "Unsupported shift type.");
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      switch (shiftType)
         {
         case TR::SH_LSL:
            op = is64bit ? TR::InstOpCode::lslvx : TR::InstOpCode::lslvw;
            break;
         case TR::SH_LSR:
            op = is64bit ? TR::InstOpCode::lsrvx : TR::InstOpCode::lsrvw;
            break;
         case TR::SH_ASR:
            op = is64bit ? TR::InstOpCode::asrvx : TR::InstOpCode::asrvw;
            break;
         default:
            TR_ASSERT(false, "Unsupported shift type.");
         }
      generateTrg1Src2Instruction(cg, op, node, trgReg, srcReg, shiftAmountReg);
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

// also handles lshl
TR::Register *
OMR::ARM64::TreeEvaluator::ishlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return shiftHelper(node, TR::SH_LSL, cg);
   }

// also handles lshr
TR::Register *
OMR::ARM64::TreeEvaluator::ishrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return shiftHelper(node, TR::SH_ASR, cg);
   }

// also handles lushr
TR::Register *
OMR::ARM64::TreeEvaluator::iushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return shiftHelper(node, TR::SH_LSR, cg);
   }

// also handles lrol
TR::Register *
OMR::ARM64::TreeEvaluator::irolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(firstChild);
   bool is64bit = node->getDataType().isInt64();
   TR::InstOpCode::Mnemonic op;

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = secondChild->getInt();
      uint32_t shift = is64bit ? (value & 0x3F) : (value & 0x1F);

      if (shift != 0)
         {
         shift = is64bit ? (64 - shift) : (32 - shift); // change ROL to ROR
         op = is64bit ? TR::InstOpCode::extrx : TR::InstOpCode::extrw; // ROR is an alias of EXTR
         generateTrg1Src2ShiftedInstruction(cg, op, node, trgReg, trgReg, trgReg, TR::SH_LSL, shift);
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      generateNegInstruction(cg, node, shiftAmountReg, shiftAmountReg); // change ROL to ROR
      if (is64bit)
         {
         // 32->64 bit sign extension: SXTW is alias of SBFM
         uint32_t imm = 0x101F; // N=1, immr=0, imms=31
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmx, node, shiftAmountReg, shiftAmountReg, imm);
         }
      op = is64bit ? TR::InstOpCode::rorvx : TR::InstOpCode::rorvw;
      generateTrg1Src2Instruction(cg, op, node, trgReg, trgReg, shiftAmountReg);
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

// Evaluate land, lor and lxor
static inline TR::Register *
evaluateBooleanTypeInteger(TR::Node *node, TR::InstOpCode::Mnemonic regOp, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->gprClobberEvaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->gprClobberEvaluate(secondChild);
   TR::Register *trgReg = cg->allocateRegister();

   generateTrg1Src2Instruction(cg, regOp, node, trgReg, src1Reg, src2Reg);

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	return evaluateBooleanTypeInteger(node, TR::InstOpCode::andx, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	return evaluateBooleanTypeInteger(node, TR::InstOpCode::orrx, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	return evaluateBooleanTypeInteger(node, TR::InstOpCode::eorx, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iandEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::iorEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ixorEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}
