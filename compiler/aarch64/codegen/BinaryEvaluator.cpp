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
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Bit.hpp"

/**
 * Generic helper that handles a number of similar binary operations.
 * @param[in] node : calling node
 * @param[in] regOp : the target AArch64 instruction opcode
 * @param[in] regOpImm : the matching AArch64 immediate instruction opcode
 * regOpImm == regOp indicates that the passed opcode has no immediate form.
 * @param[in] cg : codegenerator
 * @return target register
 */
static inline TR::Register *
genericBinaryEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic regOp, TR::InstOpCode::Mnemonic regOpImm, bool is64Bit, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = NULL;
   TR::Register *trgReg = NULL;
   int64_t value = 0;

   if(1 == firstChild->getReferenceCount())
      {
      trgReg = src1Reg;
      }
   else if(1 == secondChild->getReferenceCount() && secondChild->getRegister() != NULL)
      {
      trgReg = src2Reg;
      }
   else
      {
      trgReg = cg->allocateRegister();
      }

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      if(is64Bit)
         {
         value = secondChild->getLongInt();
         }
      else
         {
         value = secondChild->getInt();
         }
      /* When regOp == regOpImm, an immediate version of the instruction does not exist. */
      if(constantIsUnsignedImm12(value) && regOp != regOpImm)
         {
         generateTrg1Src1ImmInstruction(cg, regOpImm, node, trgReg, src1Reg, value);
         }
      else
         {
         src2Reg = cg->allocateRegister();
         if(is64Bit)
            {
            loadConstant64(cg, node, value, src2Reg);
            }
         else
            {
            loadConstant32(cg, node, value, src2Reg);
            }
         generateTrg1Src2Instruction(cg, regOp, node, trgReg, src1Reg, src2Reg);
         cg->stopUsingRegister(src2Reg);
         }
      }
   else
      {
      src2Reg = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, regOp, node, trgReg, src1Reg, src2Reg);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

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
   return genericBinaryEvaluator(node, TR::InstOpCode::addw, TR::InstOpCode::addimmw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::laddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	return genericBinaryEvaluator(node, TR::InstOpCode::addx, TR::InstOpCode::addimmx, true, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::isubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericBinaryEvaluator(node, TR::InstOpCode::subw, TR::InstOpCode::subimmw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	return genericBinaryEvaluator(node, TR::InstOpCode::subx, TR::InstOpCode::subimmx, true, cg);
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

// Multiply a register by a 64-bit constant
static void mulConstant64(TR::Node *node, TR::Register *treg, TR::Register *sreg, int64_t value, TR::CodeGenerator *cg)
   {
   if (value == 0)
      {
      loadConstant64(cg, node, 0, treg);
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
      loadConstant64(cg, node, value, tmpReg);
      generateMulInstruction(cg, node, treg, sreg, tmpReg);
      cg->stopUsingRegister(tmpReg);
      }
   }

TR::Register *
OMR::ARM64::TreeEvaluator::imulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = NULL;
   TR::Register *trgReg = NULL;
   int32_t value = 0;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      value = secondChild->getInt();
      if (value > 0 && cg->convertMultiplyToShift(node))
         {
         trgReg = cg->evaluate(node);
         return trgReg;
         }
      }

   if(1 == firstChild->getReferenceCount())
      {
      trgReg = src1Reg;
      }
   else if(1 == secondChild->getReferenceCount() && (src2Reg = secondChild->getRegister()) != NULL)
      {
      trgReg = src2Reg;
      }
   else
      {
      trgReg = cg->allocateRegister();
      }

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
         mulConstant32(node, trgReg, src1Reg, value, cg);
      }
   else
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
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
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg;
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *tmpReg = NULL;

   TR::Register *zeroReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *cond = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(cond, zeroReg, TR::RealRegister::xzr, TR_GPR, cg);

   // imulh is generated for constant idiv and the second child is the magic number
   // assume magic number is usually a large odd number with little optimization opportunity
   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int32_t value = secondChild->getInt();
      src2Reg = tmpReg = cg->allocateRegister();
      loadConstant32(cg, node, value, src2Reg);
      }
   else
      {
      src2Reg = cg->evaluate(secondChild);
      }

   generateTrg1Src3Instruction(cg, TR::InstOpCode::smaddl, node, trgReg, src1Reg, src2Reg, zeroReg, cond);
   cg->stopUsingRegister(zeroReg);
   /* logical shift right by 32 bits */
   uint32_t imm = 0x183F; // N=1, immr=32, imms=63
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmx, node, trgReg, trgReg, imm);

   if (tmpReg)
      {
      cg->stopUsingRegister(tmpReg);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = NULL;
   TR::Register *trgReg = NULL;
   int64_t value = 0;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      value = secondChild->getLongInt();
      if (value > 0 && cg->convertMultiplyToShift(node))
         {
         trgReg = cg->evaluate(node);
         return trgReg;
         }
      }

   if(1 == firstChild->getReferenceCount())
      {
      trgReg = src1Reg;
      }
   else if(1 == secondChild->getReferenceCount() && (src2Reg = secondChild->getRegister()) != NULL)
      {
      trgReg = src2Reg;
      }
   else
      {
      trgReg = cg->allocateRegister();
      }

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
         mulConstant64(node, trgReg, src1Reg, value, cg);
      }
   else
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      generateMulInstruction(cg, node, trgReg, src1Reg, src2Reg);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
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
OMR::ARM64::TreeEvaluator::lmulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg;
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *tmpReg = NULL;

   // lmulh is generated for constant ldiv and the second child is the magic number
   // assume magic number is usually a large odd number with little optimization opportunity
   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int64_t value = secondChild->getLongInt();
      src2Reg = tmpReg = cg->allocateRegister();
      loadConstant64(cg, node, value, src2Reg);
      }
   else
      {
      src2Reg = cg->evaluate(secondChild);
      }

   generateTrg1Src2Instruction(cg, TR::InstOpCode::smulh, node, trgReg, src1Reg, src2Reg);

   if (tmpReg)
      {
      cg->stopUsingRegister(tmpReg);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::idivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericBinaryEvaluator(node, TR::InstOpCode::sdivw, TR::InstOpCode::sdivw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return iremHelper(node, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericBinaryEvaluator(node, TR::InstOpCode::sdivx, TR::InstOpCode::sdivx, true, cg);
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

TR::Register *
OMR::ARM64::TreeEvaluator::iandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// boolean and of 2 integers
	return genericBinaryEvaluator(node, TR::InstOpCode::andw, TR::InstOpCode::andimmw, false, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// boolean and of 2 integers
	return genericBinaryEvaluator(node, TR::InstOpCode::andx, TR::InstOpCode::andimmx, true, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// boolean or of 2 integers
	return genericBinaryEvaluator(node, TR::InstOpCode::orrw, TR::InstOpCode::orrimmw, false, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// boolean or of 2 integers
	return genericBinaryEvaluator(node, TR::InstOpCode::orrx, TR::InstOpCode::orrimmx, true, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// boolean xor of 2 integers
	return genericBinaryEvaluator(node, TR::InstOpCode::eorw, TR::InstOpCode::eorimmw, false, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// boolean xor of 2 integers
	return genericBinaryEvaluator(node, TR::InstOpCode::eorx, TR::InstOpCode::eorimmx, true, cg);
	}
