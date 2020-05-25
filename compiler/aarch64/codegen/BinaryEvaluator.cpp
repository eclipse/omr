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

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64ShiftCode.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/AutomaticSymbol.hpp"
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
      trgReg = secondChild->getRegister();
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
      else if (constantIsUnsignedImm12(-value) &&
               (regOpImm == TR::InstOpCode::addimmw || regOpImm == TR::InstOpCode::addimmx ||
                regOpImm == TR::InstOpCode::subimmw || regOpImm == TR::InstOpCode::subimmx))
         {
         TR::InstOpCode::Mnemonic negatedOp;
         switch (regOpImm)
            {
            case TR::InstOpCode::addimmw:
               negatedOp = TR::InstOpCode::subimmw;
               break;
            case TR::InstOpCode::addimmx:
               negatedOp = TR::InstOpCode::subimmx;
               break;
            case TR::InstOpCode::subimmw:
               negatedOp = TR::InstOpCode::addimmw;
               break;
            case TR::InstOpCode::subimmx:
               negatedOp = TR::InstOpCode::addimmx;
               break;
            default:
               TR_ASSERT(false, "Unsupported op");
            }
         generateTrg1Src1ImmInstruction(cg, negatedOp, node, trgReg, src1Reg, -value);
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

// multiply and add
// multiply and subtract
static TR::Register *
generateMaddOrMsub(TR::Node *node, TR::Node *mulNode, TR::Node *anotherNode, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   if ((mulNode->getOpCodeValue() == TR::imul || mulNode->getOpCodeValue() == TR::lmul) &&
       mulNode->getReferenceCount() == 1 &&
       mulNode->getRegister() == NULL)
      {
      TR::Register *trgReg = cg->allocateRegister();
      TR::Register *mulSrc1Reg = cg->evaluate(mulNode->getFirstChild());
      TR::Register *mulSrc2Reg = cg->evaluate(mulNode->getSecondChild());
      TR::Register *src3Reg = cg->evaluate(anotherNode);

      generateTrg1Src3Instruction(cg, op, node, trgReg, mulSrc1Reg, mulSrc2Reg, src3Reg);

      node->setRegister(trgReg);
      cg->decReferenceCount(mulNode->getFirstChild());
      cg->decReferenceCount(mulNode->getSecondChild());
      cg->decReferenceCount(mulNode);
      cg->decReferenceCount(anotherNode);
      return trgReg;
      }
   else
      {
      return NULL; // not applicable
      }
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *retReg;

   retReg = generateMaddOrMsub(node, firstChild, secondChild, TR::InstOpCode::maddw, cg); // x*y + z
   if (retReg)
      {
      return retReg;
      }
   retReg = generateMaddOrMsub(node, secondChild, firstChild, TR::InstOpCode::maddw, cg); // x + y*z
   if (retReg)
      {
      return retReg;
      }

   return genericBinaryEvaluator(node, TR::InstOpCode::addw, TR::InstOpCode::addimmw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::laddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *retReg;

   retReg = generateMaddOrMsub(node, firstChild, secondChild, TR::InstOpCode::maddx, cg); // x*y + z
   if (retReg)
      {
      return retReg;
      }
   retReg = generateMaddOrMsub(node, secondChild, firstChild, TR::InstOpCode::maddx, cg); // x + y*z
   if (retReg)
      {
      return retReg;
      }

   return genericBinaryEvaluator(node, TR::InstOpCode::addx, TR::InstOpCode::addimmx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::isubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *retReg;

   retReg = generateMaddOrMsub(node, secondChild, firstChild, TR::InstOpCode::msubw, cg); // x - y*z
   if (retReg)
      {
      return retReg;
      }

   return genericBinaryEvaluator(node, TR::InstOpCode::subw, TR::InstOpCode::subimmw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *retReg;

   retReg = generateMaddOrMsub(node, secondChild, firstChild, TR::InstOpCode::msubx, cg); // x - y*z
   if (retReg)
      {
      return retReg;
      }

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

   if (secondOp == TR::iconst)
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

/**
 * Helper template function that decodes bitmask for logic op codes
 * @param[in] Nbit: N bit
 * @param[in] immr : immr part of immediate
 * @param[in] imms : imms part of immediate
 * @param[out] wmask : returned immediate
 * @return true if successfully decoded
 */
template<typename U>
bool
decodeBitMasksImpl(bool Nbit, uint32_t immr, uint32_t imms, U &wmask)
   {
   const size_t immBitLen = sizeof(U) * 8;
   if ((immBitLen == 32) && Nbit)
      {
      return false;
      }
   int len = Nbit ? 6 : (31 - leadingZeroes((~imms) & 0x3f));
   if ((len <= 0) || (immBitLen < (1 << len)))
      {
      return false;
      }

   uint32_t levels = (1 << len) - 1;
   uint32_t s = imms & levels;

   if (s == levels)
      {
      return false;
      }
   U welem = (static_cast<U>(1) << (s + 1)) - 1;
   uint32_t elemLen = 1 << len;
   uint32_t r = immr & levels;
   U mask = (immBitLen == elemLen) ? (~static_cast<U>(0)) : (static_cast<U>(1) << elemLen) - 1;
   U elem = ((welem << (elemLen - r)) | (welem >> r)) & mask;

   while (elemLen < immBitLen)
      {
      elem |= (elem << elemLen);
      elemLen *= 2;
      }
   wmask = elem;
   return true;
   }

/**
 * Decodes bitmask for 32bit variants of logic op codes
 * @param[in] Nbit: N bit
 * @param[in] immr : immr part of immediate
 * @param[in] imms : imms part of immediate
 * @param[out] wmask : returned immediate
 * @return true if successfully decoded
 */
bool
decodeBitMasks(bool Nbit, uint32_t immr, uint32_t imms, uint32_t &wmask)
   {
   return decodeBitMasksImpl(Nbit, immr, imms, wmask);
   }

/**
 * Decodes bitmask for 64bit variants of logic op codes
 * @param[in] Nbit: N bit
 * @param[in] immr : immr part of immediate
 * @param[in] imms : imms part of immediate
 * @param[out] wmask : returned immediate
 * @return true if successfully decoded
 */
bool
decodeBitMasks(bool Nbit, uint32_t immr, uint32_t imms, uint64_t &wmask)
   {
   return decodeBitMasksImpl(Nbit, immr, imms, wmask);
   }

/**
 * Helper template function that find rotate count and ones count for value.
 * @param[in] value : immediate value element
 * @param[in] elementSize : element size
 * @param[out] rotateCount : right rotate count
 * @param[out] onesCount : number of consective ones in value
 * @return true if value has contiguous ones (as elementSize-bit integer)
 */
template<typename T, typename U> inline
bool findLogicImmediateBitPattern(T value, int elementSize, int32_t &rotateCount, int32_t &onesCount)
   {
   U signExtendedValue = value;
   const size_t size = sizeof(U) * 8;
   // if elementSize is 2 or 4, ones in value are always contiguous.
   if (elementSize > 4)
      {
      if (elementSize < size)
         {
         signExtendedValue = (((U)value) << (size - elementSize)) >> (size - elementSize);
         }
      if (!contiguousBits(signExtendedValue))
         {
         return false;
         }
      }

   int32_t tz = trailingZeroes(value);
   onesCount = populationCount(value);
   // if value has trailing zeroes, right rorate count is elementSize - (trailing zeroes count).
   // if not, right rotate count is leading ones of value (viewed as elementSize-bit integer),
   // which is obtained by subtracting trailing ones count from total ones count.
   rotateCount = (tz != 0) ? (elementSize - tz) : (onesCount - trailingZeroes(~value));

   return true;
   }

/**
 * Helper function for encoding immediate value of logic instructions.
 * @param[in] value : immediate value to encode
 * @param[in] is64Bit : true if 64bit instruction
 * @param[out] n : N bit
 * @param[out] immEncoded : immr and imms encoded in 12bit field
 * @return true if value can be encoded as immediate operand
 */
bool
logicImmediateHelper(uint64_t value, bool is64Bit, bool &n, uint32_t &immEncoded)
   {
   uint64_t mask = ~(uint64_t)0;
   int elementSize = is64Bit ? 64 : 32;
   // leading ones in imms
   uint32_t imms = 1 << 7;

   if (!is64Bit)
      {
      mask >>= elementSize;
      imms >>= 1;
      }
   // all zeroes or all ones are not allowed
   if ((value == 0) || (value == mask))
      {
      return false;
      }

   do
      {
      uint32_t highBits = value >> (elementSize /2);
      uint32_t lowBits = value & (mask >> (elementSize / 2));
      if (highBits != lowBits)
         {
         // found element size
         break;
         }
      imms = imms | (imms >> 1);
      value = lowBits;
      mask >>= elementSize / 2;
      elementSize /= 2;

      } while (elementSize > 2);

   int32_t rotateCount;
   int32_t onesCount;
   if (is64Bit)
      {
      if (!findLogicImmediateBitPattern<uint64_t, int64_t>(value, elementSize, rotateCount, onesCount))
         {
         return false;
         }
      }
   else
      {
      if (!findLogicImmediateBitPattern<uint32_t, int32_t>((uint32_t)value, elementSize, rotateCount, onesCount))
         {
         return false;
         }
      }
   TR_ASSERT((onesCount > 0) && (onesCount < elementSize), "Failed to encode imms.");
   TR_ASSERT((rotateCount >= 0) && (rotateCount < elementSize), "Failed to encode immr.");

   imms |= onesCount - 1;
   imms &= (1 << 6) - 1;
   immEncoded = (rotateCount << 6) | imms;
   n = elementSize == 64;

   return true;
   }

/**
 * Helper function that handles logic binary operations.
 * @param[in] node : calling node
 * @param[in] regOp : the target AArch64 instruction opcode
 * @param[in] regOpImm : the matching AArch64 immediate instruction opcode
 * regOpImm == regOp indicates that the passed opcode has no immediate form.
 * @param[in] is64Bit : true if regOp and regOpImm are 64bit opcode.
 * @param[in] cg : codegenerator
 * @return target register
 */
static inline TR::Register *
logicBinaryEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic regOp, TR::InstOpCode::Mnemonic regOpImm, bool is64Bit, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = NULL;
   TR::Register *trgReg = NULL;
   int64_t value = 0;

   if (1 == firstChild->getReferenceCount())
      {
      trgReg = src1Reg;
      }
   else if (1 == secondChild->getReferenceCount() && secondChild->getRegister() != NULL)
      {
      trgReg = secondChild->getRegister();
      }
   else
      {
      trgReg = cg->allocateRegister();
      }

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      if (is64Bit)
         {
         value = secondChild->getLongInt();
         }
      else
         {
         value = secondChild->getInt();
         }
      bool N;
      uint32_t immEncoded;
      if (logicImmediateHelper(is64Bit ? value : (uint32_t)value, is64Bit, N, immEncoded))
         {
         generateLogicalImmInstruction(cg, regOpImm, node, trgReg, src1Reg, N, immEncoded);
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

TR::Register *
OMR::ARM64::TreeEvaluator::iandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean and of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::andw, TR::InstOpCode::andimmw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean and of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::andx, TR::InstOpCode::andimmx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean or of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::orrw, TR::InstOpCode::orrimmw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean or of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::orrx, TR::InstOpCode::orrimmx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean xor of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::eorw, TR::InstOpCode::eorimmw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean xor of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::eorx, TR::InstOpCode::eorimmx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::aladdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = laddEvaluator(node, cg);

   if (node->isInternalPointer())
      {
      TR::AutomaticSymbol *pinningArrayPointerSymbol = NULL;

      if (node->getPinningArrayPointer())
         {
         pinningArrayPointerSymbol = node->getPinningArrayPointer();
         }
      else
         {
         TR::Node *firstChild = node->getFirstChild();
         if ((firstChild->getOpCodeValue() == TR::aload) &&
             firstChild->getSymbolReference()->getSymbol()->isAuto() &&
             firstChild->getSymbolReference()->getSymbol()->isPinningArrayPointer())
            {
            TR::Symbol *firstChildSymbol = firstChild->getSymbolReference()->getSymbol();

            pinningArrayPointerSymbol = firstChildSymbol->isInternalPointer() ?
               firstChildSymbol->castToInternalPointerAutoSymbol()->getPinningArrayPointer() :
               firstChildSymbol->castToAutoSymbol();
            }
         else if (firstChild->getRegister() &&
                  firstChild->getRegister()->containsInternalPointer())
            {
            pinningArrayPointerSymbol = firstChild->getRegister()->getPinningArrayPointer();
            }
         }

      if (pinningArrayPointerSymbol)
         {
         trgReg->setContainsInternalPointer();
         trgReg->setPinningArrayPointer(pinningArrayPointerSymbol);
         }
      }

   return trgReg;
   }
