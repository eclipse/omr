/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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

   if (node->getOpCodeValue() != TR::aladd)
      {
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
      }
   else
      {
      /*
       * Because trgReg can contain an internal pointer, we cannot use the same virtual register
       * for trgReg and sources for aladd except for the case
       * where src1Reg also contains an internal pointer and has the same pinning array as the node.
       */
      if ((1 == firstChild->getReferenceCount()) && node->isInternalPointer() && src1Reg->containsInternalPointer() &&
         (node->getPinningArrayPointer() == src1Reg->getPinningArrayPointer()))
         {
         trgReg = src1Reg;
         }
      else
         {
         trgReg = cg->allocateRegister();
         }
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
      if((constantIsUnsignedImm12(value) || constantIsUnsignedImm12Shifted(value)) && regOp != regOpImm)
         {
         generateTrg1Src1ImmInstruction(cg, regOpImm, node, trgReg, src1Reg, value);
         }
      else if ((constantIsUnsignedImm12(-value) || constantIsUnsignedImm12Shifted(-value))  &&
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
         TR::InstOpCode::Mnemonic negatedOp = TR::InstOpCode::bad;

         if ((secondChild->getReferenceCount() == 1) &&
             (regOp == TR::InstOpCode::addw || regOp == TR::InstOpCode::addx ||
             regOp == TR::InstOpCode::subw || regOp == TR::InstOpCode::subx) &&
             (is64Bit ? shouldLoadNegatedConstant64(value) : shouldLoadNegatedConstant32(value)))
            {
            switch (regOp)
               {
               case TR::InstOpCode::addw:
                  negatedOp = TR::InstOpCode::subw;
                  break;
               case TR::InstOpCode::addx:
                  negatedOp = TR::InstOpCode::subx;
                  break;
               case TR::InstOpCode::subw:
                  negatedOp = TR::InstOpCode::addw;
                  break;
               case TR::InstOpCode::subx:
                  negatedOp = TR::InstOpCode::addx;
                  break;
               default:
                  TR_ASSERT_FATAL(false, "Unsupported op");
               }
            }
         if (negatedOp != TR::InstOpCode::bad)
            {
            src2Reg = cg->allocateRegister();

            if(is64Bit)
               {
               loadConstant64(cg, node, -value, src2Reg);
               }
            else
               {
               loadConstant32(cg, node, -value, src2Reg);
               }
            generateTrg1Src2Instruction(cg, negatedOp, node, trgReg, src1Reg, src2Reg);
            cg->stopUsingRegister(src2Reg);
            }
         else
            {
            src2Reg = cg->evaluate(secondChild);
            generateTrg1Src2Instruction(cg, regOp, node, trgReg, src1Reg, src2Reg);
            }
         }
      }
   else
      {
      src2Reg = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, regOp, node, trgReg, src1Reg, src2Reg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
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
      TR::Register *trgReg;
      TR::Node *mulFirstChild = mulNode->getFirstChild();
      TR::Node *mulSecondChild = mulNode->getSecondChild();
      TR::Register *mulSrc1Reg = cg->evaluate(mulFirstChild);
      TR::Register *mulSrc2Reg = cg->evaluate(mulSecondChild);
      TR::Register *src3Reg = cg->evaluate(anotherNode);
      if (node->getOpCodeValue() != TR::aladd)
         {
         if (mulFirstChild->getReferenceCount() == 1)
            {
            trgReg = mulSrc1Reg;
            }
         else if (mulSecondChild->getReferenceCount() == 1)
            {
            trgReg = mulSrc2Reg;
            }
         else if (anotherNode->getReferenceCount() == 1)
            {
            trgReg = src3Reg;
            }
         else
            {
            trgReg = cg->allocateRegister();
            }
         }
      else
         {
         /*
          * Because trgReg can contain an internal pointer, we cannot use the same virtual register
          * for trgReg and sources for aladd except for the case
          * where src1Reg also contains an internal pointer and has the same pinning array as the node.
          */
         if ((1 == mulFirstChild->getReferenceCount()) && node->isInternalPointer() && mulSrc1Reg->containsInternalPointer() &&
            (node->getPinningArrayPointer() == mulSrc1Reg->getPinningArrayPointer()))
            {
            trgReg = mulSrc1Reg;
            }
         else
            {
            trgReg = cg->allocateRegister();
            }
         }

      generateTrg1Src3Instruction(cg, op, node, trgReg, mulSrc1Reg, mulSrc2Reg, src3Reg);

      node->setRegister(trgReg);
      cg->decReferenceCount(mulFirstChild);
      cg->decReferenceCount(mulSecondChild);
      cg->decReferenceCount(mulNode);
      cg->decReferenceCount(anotherNode);
      return trgReg;
      }
   else
      {
      return NULL; // not applicable
      }
   }

/**
 * @brief Answers whether the node is a NOT node.
 *
 * @param[in] node: node
 *
 * @return true if the node is a NOT node.
 */
static bool
isNot(TR::Node *node)
   {
   return node->getOpCode().isXor() && node->getSecondChild()->getOpCode().isLoadConst() && (node->getSecondChild()->getConstValue() == -1);
   }

/**
 * @brief Answers whether the node is a shift node with constant amount
 *
 * @param[in] node: node
 *
 * @return true if the node is a shift node with constant amount
 */
static bool
isShiftWithConstAmountNode(TR::Node *node)
   {
   return node->getOpCode().isShift() && (node->getSecondChild()->getOpCodeValue() == TR::iconst);
   }

/**
 * @brief Answers whether the node can be handled as shifted register binary operation
 *
 * @param[in]         node: node
 * @param[in]          lhs: left hand side operand node
 * @param[in]          rhs: right hand side operand node
 * @param[out] source1Node: reference to first source node
 * @param[out] source2Node: reference to second source Node
 * @param[out]   shiftNode: reference to shift node
 * @param[out]     notNode: reference to NOT node
 * @param[out]  shiftValue: reference to shift amount
 *
 * @return true if the node can be handled as shifted register binary operation
 */
static bool
isShiftedBinaryOp(TR::Node *node, TR::Node *lhs, TR::Node *rhs, TR::Node *&source1Node, TR::Node *&source2Node, TR::Node *&shiftNode, TR::Node *&notNode, int32_t &shiftValue)
   {
   if ((!lhs->getOpCode().isLoadConst()) &&
       (rhs->getReferenceCount() == 1) &&
       (rhs->getRegister() == NULL))
      {
      if (isShiftWithConstAmountNode(rhs))
        {
         source1Node = lhs;
         shiftNode = rhs;
         TR::Node *shiftChild = shiftNode->getFirstChild();
         /* ((~a) >> n) is equal to ~(a >> n) if the shift is arithmetic */
         if (node->getOpCode().isBitwiseLogical() && (shiftChild->getReferenceCount() == 1) &&
            (shiftChild->getRegister() == NULL) && isNot(shiftChild) &&
            shiftNode->getOpCode().isRightShift() && (!shiftNode->getOpCode().isShiftLogical()))
            {
            notNode = shiftChild;
            source2Node = notNode->getFirstChild();
            }
         else
            {
            source2Node = shiftNode->getFirstChild();
            }
         shiftValue = shiftNode->getSecondChild()->getInt();
         return true;
         }
      else if (node->getOpCode().isBitwiseLogical() && isNot(rhs))
         {
         source1Node = lhs;
         notNode = rhs;
         TR::Node *childOfNot = notNode->getFirstChild();
         if ((childOfNot->getReferenceCount() == 1) && (childOfNot->getRegister() == NULL) &&
            isShiftWithConstAmountNode(childOfNot))
            {
            source2Node = childOfNot->getFirstChild();
            shiftValue = childOfNot->getSecondChild()->getInt();
            shiftNode = childOfNot;
            }
         else
            {
            source2Node = childOfNot;
            }
         return true;
         }
      }
   return false;
   }

/**
 * @brief Generates add/sub/and/or/eor shifted register instruction if possible
 *
 * @param[in]   node:  node
 * @param[in]     op:  mnemonic for this node
 * @param[in]  notOp:  mnemonic for this node if NOT is applied to the operand (for bitwise logical op only)
 * @param[in]     cg:  code generator
 *
 * @return register which contains the result of the operation. NULL if the operation cannot be encoded in shifted register instruction.
 */
static TR::Register *
generateShiftedBinaryOperation(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::InstOpCode::Mnemonic notOp, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *source1Node = NULL;
   TR::Node *source2Node = NULL;
   TR::Node *shiftNode = NULL;
   TR::Node *notNode = NULL;

   int32_t shiftValue = 0;

   if (!(isShiftedBinaryOp(node, firstChild, secondChild, source1Node, source2Node, shiftNode, notNode, shiftValue) ||
       ((!node->getOpCode().isSub()) && isShiftedBinaryOp(node, secondChild, firstChild, source1Node, source2Node, shiftNode, notNode, shiftValue))))
      {
      return NULL;
      }

   const bool is64bit = node->getDataType().isInt64();
   if ((shiftValue < 0) || (shiftValue > (is64bit ? 63 : 31)))
      {
      return NULL;
      }
   TR::Register *treg;
   TR::Register *s1reg = cg->evaluate(source1Node);
   TR::Register *s2reg = cg->evaluate(source2Node);
   if (node->getOpCodeValue() != TR::aladd)
      {
      if (source1Node->getReferenceCount() == 1)
         {
         treg = s1reg;
         }
      else if (source2Node->getReferenceCount() == 1)
         {
         treg = s2reg;
         }
      else
         {
         treg = cg->allocateRegister();
         }
      }
   else
      {
      /*
       * Because treg can contain an internal pointer, we cannot use the same virtual register
       * for treg and sources for aladd except for the case
       * where s1reg also contains an internal pointer and has the same pinning array as the node.
       */
      if ((1 == source1Node->getReferenceCount()) && node->isInternalPointer() && s1reg->containsInternalPointer() &&
         (node->getPinningArrayPointer() == s1reg->getPinningArrayPointer()))
         {
         treg = s1reg;
         }
      else
         {
         treg = cg->allocateRegister();
         }
      }
   if (shiftNode != NULL)
      {
      TR::ARM64ShiftCode code = (shiftNode->getOpCode().isLeftShift() ? TR::SH_LSL : (shiftNode->getOpCode().isShiftLogical() ? TR::SH_LSR : TR::SH_ASR));

      generateTrg1Src2ShiftedInstruction(cg, (notNode == NULL) ? op : notOp, node, treg, s1reg, s2reg, code, shiftValue);
      }
   else
      {
      generateTrg1Src2Instruction(cg, (notNode == NULL) ? op : notOp, node, treg, s1reg, s2reg);
      }

   node->setRegister(treg);
   cg->recursivelyDecReferenceCount((source1Node == firstChild) ? secondChild : firstChild);
   cg->decReferenceCount(source1Node);
   return treg;
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

   retReg = generateShiftedBinaryOperation(node, TR::InstOpCode::addw, TR::InstOpCode::addw, cg);
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

   retReg = generateShiftedBinaryOperation(node, TR::InstOpCode::addx, TR::InstOpCode::addx, cg);
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

   retReg = generateShiftedBinaryOperation(node, TR::InstOpCode::subw, TR::InstOpCode::subw, cg);
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

   retReg = generateShiftedBinaryOperation(node, TR::InstOpCode::subx, TR::InstOpCode::subx, cg);
   if (retReg)
      {
      return retReg;
      }

   return genericBinaryEvaluator(node, TR::InstOpCode::subx, TR::InstOpCode::subimmx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::inlineVectorBinaryOp(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, binaryEvaluatorHelper evaluatorHelper)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *lhsReg = NULL, *rhsReg = NULL;

   lhsReg = cg->evaluate(firstChild);
   rhsReg = cg->evaluate(secondChild);

   TR_ASSERT(lhsReg->getKind() == TR_VRF, "unexpected Register kind");
   TR_ASSERT(rhsReg->getKind() == TR_VRF, "unexpected Register kind");

   TR::Register *resReg = cg->allocateRegister(TR_VRF);

   node->setRegister(resReg);
   TR_ASSERT_FATAL_WITH_NODE(node, (op != TR::InstOpCode::bad) || (evaluatorHelper != NULL), "If op is TR::InstOpCode::bad, evaluatorHelper must not be NULL");
   if (evaluatorHelper != NULL)
      {
      (*evaluatorHelper)(node, resReg, lhsReg, rhsReg, cg);
      }
   else
      {
      generateTrg1Src2Instruction(cg, op, node, resReg, lhsReg, rhsReg);
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return resReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic addOp;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
         addOp = TR::InstOpCode::vadd16b;
         break;
      case TR::Int16:
         addOp = TR::InstOpCode::vadd8h;
         break;
      case TR::Int32:
         addOp = TR::InstOpCode::vadd4s;
         break;
      case TR::Int64:
         addOp = TR::InstOpCode::vadd2d;
         break;
      case TR::Float:
         addOp = TR::InstOpCode::vfadd4s;
         break;
      case TR::Double:
         addOp = TR::InstOpCode::vfadd2d;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, addOp);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic subOp;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
         subOp = TR::InstOpCode::vsub16b;
         break;
      case TR::Int16:
         subOp = TR::InstOpCode::vsub8h;
         break;
      case TR::Int32:
         subOp = TR::InstOpCode::vsub4s;
         break;
      case TR::Int64:
         subOp = TR::InstOpCode::vsub2d;
         break;
      case TR::Float:
         subOp = TR::InstOpCode::vfsub4s;
         break;
      case TR::Double:
         subOp = TR::InstOpCode::vfsub2d;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, subOp);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vmulInt64Helper(TR::Node *node, TR::Register *productReg, TR::Register *lhsReg, TR::Register *rhsReg, TR::CodeGenerator *cg)
   {
   /*
    * Vector mul instruction on aarch64 does not support 64-bit interger element.
    *
    * Suppose we have two long integers (64 bits), AB and CD, where each of A, B, C, and D is one word (32 bits).
    * To get their product, AB * CD, we can do the following:
    *
    *  AB * CD = B*D + 2^32*(A*D + B*C) + 2^64*(A*C)
    *
    * Since we are only keeping the lower 64 bits of the result, this is equivalent to:
    *
    *  AB * CD = B*D + 2^32*(A*D + B*C)
    *
    * To perform (A*D + B*C), we reverse elements in AB and do 32-bit element wise multiplication for BA and CD.
    *
    */
   TR::Register *tmp1Reg = cg->allocateRegister(TR_VRF);
   TR::Register *tmp2Reg = cg->allocateRegister(TR_VRF);

   /* Reverse 32bit words in 64bit elements */
   generateTrg1Src1Instruction(cg, TR::InstOpCode::vrev64_4s, node, tmp1Reg, lhsReg);

   generateTrg1Src2Instruction(cg, TR::InstOpCode::vmul4s, node, tmp2Reg, tmp1Reg, rhsReg);
   /*
    * At this moment, upper 32bit of each 64-bit element of tmp2Reg is A*D, lower 32bit is B*C.
    * Get A*D + B*C into the lower 32bit.
    */
   generateVectorShiftImmediateInstruction(cg, TR::InstOpCode::vushr2d, node, tmp1Reg, tmp2Reg, 32);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vadd4s, node, tmp2Reg, tmp1Reg, tmp2Reg);
   /* Shift left by 32bit to have the result in the upper 32bit part. */
   generateVectorShiftImmediateInstruction(cg, TR::InstOpCode::vshl2d, node, productReg, tmp2Reg, 32);

   /* Move the lower 32-bit of the second element to the upper 32-bit of the first element. */
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vuzp1_4s, node, tmp1Reg, lhsReg, lhsReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vuzp1_4s, node, tmp2Reg, rhsReg, rhsReg);
   /* Finally, get the result by UMLAL. */
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vumlal_2d, node, productReg, tmp1Reg, tmp2Reg);

   cg->stopUsingRegister(tmp1Reg);
   cg->stopUsingRegister(tmp2Reg);

   return productReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic mulOp = TR::InstOpCode::bad;
   binaryEvaluatorHelper evaluatorHelper = NULL;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
         mulOp = TR::InstOpCode::vmul16b;
         break;
      case TR::Int16:
         mulOp = TR::InstOpCode::vmul8h;
         break;
      case TR::Int32:
         mulOp = TR::InstOpCode::vmul4s;
         break;
      case TR::Int64:
         evaluatorHelper = vmulInt64Helper;
         break;
      case TR::Float:
         mulOp = TR::InstOpCode::vfmul4s;
         break;
      case TR::Double:
         mulOp = TR::InstOpCode::vfmul2d;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, mulOp, evaluatorHelper);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vdivIntHelper(TR::Node *node, TR::Register *resultReg, TR::Register *lhsReg, TR::Register *rhsReg, TR::CodeGenerator *cg)
   {
   struct DivOps
      {
      TR::InstOpCode::Mnemonic vectorToGPROp;
      TR::InstOpCode::Mnemonic divOp;
      TR::InstOpCode::Mnemonic gprToVectorOp;
      uint32_t                 numElements;
      };
   static const struct DivOps ops[4] = {
      {TR::InstOpCode::smovwb, TR::InstOpCode::sdivw, TR::InstOpCode::vinswb, 16},
      {TR::InstOpCode::smovwh, TR::InstOpCode::sdivw, TR::InstOpCode::vinswh, 8},
      {TR::InstOpCode::umovws, TR::InstOpCode::sdivw, TR::InstOpCode::vinsws, 4},
      {TR::InstOpCode::umovxd, TR::InstOpCode::sdivx, TR::InstOpCode::vinsxd, 2}
   };
   TR::DataType eType = node->getDataType().getVectorElementType();
   const uint32_t index = (eType == TR::Int8) ? 0 :
                         ((eType == TR::Int16) ? 1 :
                         ((eType == TR::Int32) ? 2 : 3));
   struct DivOps op = ops[index];

   TR_ARM64ScratchRegisterManager *srm = cg->generateScratchRegisterManager();
   TR::Register *tmp1Reg = srm->findOrCreateScratchRegister(TR_GPR);
   TR::Register *tmp2Reg = srm->findOrCreateScratchRegister(TR_GPR);

   const uint32_t numElementsPerLoop = (op.numElements > 4) ? 4 : op.numElements;
   const uint32_t loopCount = op.numElements / numElementsPerLoop;

  /*
   * For 8-bit elements,
   *
   * movzw   w2, #4
   * loop:
   * vushr4s v2, v2, #8
   * smovwb  w0, v0.b[0]
   * smovwb  w1, v1.b[0]
   * sdivw   w0, w1, w0
   * vinswb  v2.b[3], w0
   * smovwb  w0, v0.b[4]
   * smovwb  w1, v1.b[4]
   * sdivw   w0, w1, w0
   * vinswb  v2.b[7], w0
   * smovwb  w0, v0.b[8]
   * smovwb  w1, v1.b[8]
   * sdivw   w0, w1, w0
   * vinswb  v2.b[11], w0
   * smovwb  w0, v0.b[12]
   * smovwb  w1, v1.b[12]
   * sdivw   w0, w1, w0
   * vinswb  v2.b[15], w0
   * vushr4s v0, v0, #8
   * vushr4s v1, v1, #8
   * subw    w2, w2, #1
   * cbnzw   w2, loop
   *
   * For 16-bit elements,
   *
   * movzw   w2, #2
   * loop:
   * vushr4s v2, v2, #16
   * smovwh  w0, v0.h[0]
   * smovwh  w1, v1.h[0]
   * sdivw   w0, w1, w0
   * vinswh  v2.h[1], w0
   * smovwh  w0, v0.h[2]
   * smovwh  w1, v1.h[2]
   * sdivw   w0, w1, w0
   * vinswh  v2.h[3], w0
   * smovwh  w0, v0.h[4]
   * smovwh  w1, v1.h[4]
   * sdivw   w0, w1, w0
   * vinswh  v2.h[5], w0
   * smovwh  w0, v0.h[6]
   * smovwh  w1, v1.h[6]
   * sdivw   w0, w1, w0
   * vinswh  v2.h[7], w0
   * vushr4s v0, v0, #16
   * vushr4s v1, v1, #16
   * subw    w2, w2, #1
   * cbnz    w2, loop
   *
   */

   TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
   if (loopCount > 1)
      {
      TR::Register *counterReg = srm->findOrCreateScratchRegister(TR_GPR);
      TR::Register *tmp1VectorReg = srm->findOrCreateScratchRegister(TR_VRF);
      TR::Register *tmp2VectorReg = srm->findOrCreateScratchRegister(TR_VRF);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::vorr16b, node, tmp1VectorReg, lhsReg, lhsReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::vorr16b, node, tmp2VectorReg, rhsReg, rhsReg);
      loadConstant32(cg, node, loopCount, counterReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
      generateVectorShiftImmediateInstruction(cg, TR::InstOpCode::vushr4s, node, resultReg, resultReg, (eType == TR::Int8) ? 8 : 16);
      for (int32_t i = 0; i < numElementsPerLoop; i++)
         {
         generateMovVectorElementToGPRInstruction(cg, op.vectorToGPROp, node, tmp1Reg, tmp1VectorReg, i * loopCount);
         generateMovVectorElementToGPRInstruction(cg, op.vectorToGPROp, node, tmp2Reg, tmp2VectorReg, i * loopCount);
         generateTrg1Src2Instruction(cg, op.divOp, node, tmp1Reg, tmp1Reg, tmp2Reg);
         generateMovGPRToVectorElementInstruction(cg, op.gprToVectorOp, node, resultReg, tmp1Reg, (i + 1) * loopCount - 1);
         }
      generateVectorShiftImmediateInstruction(cg, TR::InstOpCode::vushr4s, node, tmp1VectorReg, tmp1VectorReg, (eType == TR::Int8) ? 8 : 16);
      generateVectorShiftImmediateInstruction(cg, TR::InstOpCode::vushr4s, node, tmp2VectorReg, tmp2VectorReg, (eType == TR::Int8) ? 8 : 16);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subimmw, node, counterReg, counterReg, 1);

      TR::RegisterDependencyConditions *cond = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3 + srm->numAvailableRegisters(), cg->trMemory());
      cond->addPostCondition(lhsReg, TR::RealRegister::NoReg);
      cond->addPostCondition(rhsReg, TR::RealRegister::NoReg);
      cond->addPostCondition(resultReg, TR::RealRegister::NoReg);
      srm->addScratchRegistersToDependencyList(cond);

      generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzw, node, counterReg, loopLabel, cond);
      }
   else
      {
      for (int32_t i = 0; i < numElementsPerLoop; i++)
         {
         generateMovVectorElementToGPRInstruction(cg, op.vectorToGPROp, node, tmp1Reg, lhsReg, i);
         generateMovVectorElementToGPRInstruction(cg, op.vectorToGPROp, node, tmp2Reg, rhsReg, i);
         generateTrg1Src2Instruction(cg, op.divOp, node, tmp1Reg, tmp1Reg, tmp2Reg);
         generateMovGPRToVectorElementInstruction(cg, op.gprToVectorOp, node, resultReg, tmp1Reg, i);
         }
      }

   srm->stopUsingRegisters();

   return resultReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic divOp = TR::InstOpCode::bad;
   binaryEvaluatorHelper evaluatorHelper = NULL;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
         evaluatorHelper = vdivIntHelper;
         break;
      case TR::Float:
         divOp = TR::InstOpCode::vfdiv4s;
         break;
      case TR::Double:
         divOp = TR::InstOpCode::vfdiv2d;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, divOp, evaluatorHelper);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic andOp;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
         andOp = TR::InstOpCode::vand16b;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, andOp);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic orrOp;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
         orrOp = TR::InstOpCode::vorr16b;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, orrOp);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic xorOp;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
         xorOp = TR::InstOpCode::veor16b;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, xorOp);
   }

/**
 * @brief A helper function for generating instuction sequence for min/max operations for vectors of 64-bit integer elements.
 *
 * @param[in] node: node
 * @param[in] isMax: true if operation is max
 * @param[in] resReg: the result register
 * @param[in] lhsReg: the first argument register
 * @param[in] rhsReg: the second argument register
 * @param[in] cg: CodeGenerator
 * @return register containing the result
 */
static TR::Register *
vminmaxInt64Helper(TR::Node *node, bool isMax, TR::Register *resReg, TR::Register *lhsReg, TR::Register *rhsReg, TR::CodeGenerator *cg)
   {
   /*
    * Vector min/max instruction on aarch64 does not support 64-bit interger element.
    *
    * For min, we do
    *    cmge  v2.2d, v0.2d, v1.2d
    *    bsl   v2.16b, v1.16b, v0.16b
    *
    * For max,
    *    cmge  v2.2d, v0.2d, v1.2d
    *    bsl   v2.16b, v0.16b, v1.16b
    *
    * The result is in v2 for both cases.
    *
    */

   TR_ASSERT_FATAL_WITH_NODE(node, lhsReg->getKind() == TR_VRF, "unexpected Register kind");
   TR_ASSERT_FATAL_WITH_NODE(node, rhsReg->getKind() == TR_VRF, "unexpected Register kind");

   generateTrg1Src2Instruction(cg, TR::InstOpCode::vcmge2d, node, resReg, lhsReg, rhsReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::vbsl16b, node, resReg, (isMax ? lhsReg : rhsReg), (isMax ? rhsReg : lhsReg));

   return resReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vminInt64Helper(TR::Node *node, TR::Register *resReg, TR::Register *lhsReg, TR::Register *rhsReg, TR::CodeGenerator *cg)
   {
   return vminmaxInt64Helper(node, false, resReg, lhsReg, rhsReg, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::vmaxInt64Helper(TR::Node *node, TR::Register *resReg, TR::Register *lhsReg, TR::Register *rhsReg, TR::CodeGenerator *cg)
   {
   return vminmaxInt64Helper(node, true, resReg, lhsReg, rhsReg, cg);
   }

TR::Register*
OMR::ARM64::TreeEvaluator::vminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic minOp = TR::InstOpCode::bad;
   binaryEvaluatorHelper evaluatorHelper = NULL;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
         minOp = TR::InstOpCode::vsmin16b;
         break;
      case TR::Int16:
         minOp = TR::InstOpCode::vsmin8h;
         break;
      case TR::Int32:
         minOp = TR::InstOpCode::vsmin4s;
         break;
      case TR::Int64:
         evaluatorHelper = vminInt64Helper;
         break;
      /*
       * The behavior of vfmin and vfmax basically follows minimu/maximum of IEEE754-2019.
       * If either one of the arguments is NaN, the result is NaN.
       * If comparing +0 and -0, the result is -0 for min, +0 for max.
       */
      case TR::Float:
         minOp = TR::InstOpCode::vfmin4s;
         break;
      case TR::Double:
         minOp = TR::InstOpCode::vfmin2d;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, minOp, evaluatorHelper);
   }

TR::Register*
OMR::ARM64::TreeEvaluator::vmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL_WITH_NODE(node, node->getDataType().getVectorLength() == TR::VectorLength128,
                   "Only 128-bit vectors are supported %s", node->getDataType().toString());

   TR::InstOpCode::Mnemonic maxOp = TR::InstOpCode::bad;
   binaryEvaluatorHelper evaluatorHelper = NULL;
   switch(node->getDataType().getVectorElementType())
      {
      case TR::Int8:
         maxOp = TR::InstOpCode::vsmax16b;
         break;
      case TR::Int16:
         maxOp = TR::InstOpCode::vsmax8h;
         break;
      case TR::Int32:
         maxOp = TR::InstOpCode::vsmax4s;
         break;
      case TR::Int64:
         evaluatorHelper = vmaxInt64Helper;
         break;
      /*
       * The behavior of vfmin and vfmax basically follows minimu/maximum of IEEE754-2019.
       * If either one of the arguments is NaN, the result is NaN.
       * If comparing +0 and -0, the result is -0 for min, +0 for max.
       */
      case TR::Float:
         maxOp = TR::InstOpCode::vfmax4s;
         break;
      case TR::Double:
         maxOp = TR::InstOpCode::vfmax2d;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s", node->getDataType().toString());
         return NULL;
      }
   return inlineVectorBinaryOp(node, cg, maxOp, evaluatorHelper);
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
      generateMovInstruction(cg, node, treg, sreg, false);
      }
   else if (value == -1)
      {
      generateNegInstruction(cg, node, treg, sreg, false);
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
      generateMovInstruction(cg, node, treg, sreg, true);
      }
   else if (value == -1)
      {
      generateNegInstruction(cg, node, treg, sreg, true);
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

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
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
   generateLogicalShiftRightImmInstruction(cg, node, trgReg, trgReg, 32, true);

   if (tmpReg)
      {
      cg->stopUsingRegister(tmpReg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
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

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
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

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

static TR::Register *iremHelper(TR::Node *node, bool is64bit, bool isUnsigned, TR::CodeGenerator *cg)
   {
   // TODO: Add checks for special cases

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *tmpReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegister();

   TR::InstOpCode::Mnemonic op = is64bit ? (isUnsigned ? TR::InstOpCode::udivx : TR::InstOpCode::sdivx) :
                                           (isUnsigned ? TR::InstOpCode::udivw : TR::InstOpCode::sdivw);
   generateTrg1Src2Instruction(cg, op, node, tmpReg, src1Reg, src2Reg);
   generateTrg1Src3Instruction(cg, is64bit ? TR::InstOpCode::msubx : TR::InstOpCode::msubw, node, trgReg, tmpReg, src2Reg, src1Reg);

   cg->stopUsingRegister(tmpReg);
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
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

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

static TR::Register* subInt32DivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *tmpReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegister();
   const uint32_t operandBits = TR::DataType::getSize(node->getDataType()) * 8;

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, node, trgReg, src1Reg, operandBits - 1); // sxtb or sxth
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, node, tmpReg, src2Reg, operandBits - 1); // sxtb or sxth

   generateTrg1Src2Instruction(cg, TR::InstOpCode::sdivw, node, trgReg, trgReg, tmpReg);

   cg->stopUsingRegister(tmpReg);
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register*
OMR::ARM64::TreeEvaluator::bdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return subInt32DivEvaluator(node, cg);
   }

TR::Register*
OMR::ARM64::TreeEvaluator::sdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return subInt32DivEvaluator(node, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::idivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericBinaryEvaluator(node, TR::InstOpCode::sdivw, TR::InstOpCode::sdivw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return iremHelper(node, false, false, cg);
   }

TR::Register*
OMR::ARM64::TreeEvaluator::iudivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericBinaryEvaluator(node, TR::InstOpCode::udivw, TR::InstOpCode::udivw, false, cg);
   }

TR::Register*
OMR::ARM64::TreeEvaluator::iuremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return iremHelper(node, false, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericBinaryEvaluator(node, TR::InstOpCode::sdivx, TR::InstOpCode::sdivx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return iremHelper(node, true, false, cg);
   }

TR::Register*
OMR::ARM64::TreeEvaluator::ludivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericBinaryEvaluator(node, TR::InstOpCode::udivx, TR::InstOpCode::udivx, true, cg);
   }

/**
 * @brief Generates ubfm instruction for mask and shift operation if possible
 *
 * @param[in] shiftNode: node with shift opcode
 * @param[in]        cg: code generator
 *
 * @returns register which contains the result of the operation. NULL if the operation cannot be encoded in ubfm instruction.
 */
static TR::Register *
generateUBFMForMaskAndShift(TR::Node *shiftNode, TR::CodeGenerator *cg)
   {
   TR::Node *andNode = NULL;
   TR::Node *shiftValueNode = NULL;
   TR::Node *maskNode = NULL;
   TR::Node *sourceNode = NULL;
   if (shiftNode->getFirstChild()->getOpCode().isAnd())
      {
      andNode = shiftNode->getFirstChild();
      shiftValueNode = shiftNode->getSecondChild();
      if (!shiftValueNode->getOpCode().isLoadConst())
         {
         return NULL;
         }
      if ((andNode->getReferenceCount() > 1) || (andNode->getRegister() != NULL))
         {
         return NULL;
         }
      if (andNode->getSecondChild()->getOpCode().isLoadConst())
         {
         sourceNode = andNode->getFirstChild();
         maskNode = andNode->getSecondChild();
         }
      else if (andNode->getFirstChild()->getOpCode().isLoadConst())
         {
         sourceNode = andNode->getSecondChild();
         maskNode = andNode->getFirstChild();
         }
      }

   if (maskNode == NULL)
      {
      return NULL;
      }
   const uint32_t operandBits = TR::DataType::getSize(shiftNode->getDataType()) * 8;
   const bool is64bit = shiftNode->getDataType().isInt64();
   const int64_t shiftValue = shiftValueNode->getConstValue();

   if ((shiftValue <= 0) || (shiftValue > (operandBits - 1)))
      {
      return NULL;
      }
   const uint64_t maskValue = maskNode->get64bitIntegralValueAsUnsigned();

   if (shiftNode->getOpCode().isLeftShift())
      {
      if ((maskValue << shiftValue) == 0)
         {
         /*
          * The result is always 0 in this case.
          */
         TR::Register *reg = cg->allocateRegister();
         generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, shiftNode, reg, 0);
         shiftNode->setRegister(reg);
         cg->recursivelyDecReferenceCount(andNode);
         cg->decReferenceCount(shiftValueNode);
         return reg;
         }

      if (((maskValue & 1) == 1) && (((maskValue >> (operandBits - 1)) & 1) == 0) && contiguousBits(maskValue))
         {
         /*
          * maskValue has consecutive 1s from the least significant bits. The most significant bit must be 0. Otherwise, mask is all 1s.
          * So, this mask and shift operation is copying consecutive bits starting from the least significant bits of the source register
          * to bit position shiftValue of the destination register.
          */
         uint32_t width = populationCount(maskValue);
         uint32_t maxWidth = operandBits - static_cast<uint32_t>(shiftValue);
         if (width > maxWidth)
            {
            width = maxWidth;
            }

         TR::Register *reg;
         TR::Register *sreg = cg->evaluate(sourceNode);
         if (sourceNode->getReferenceCount() == 1)
            {
            reg = sreg;
            }
         else
            {
            reg = cg->allocateRegister();
            }

         generateUBFIZInstruction(cg, shiftNode, reg, sreg, static_cast<uint32_t>(shiftValue), width, is64bit);
         shiftNode->setRegister(reg);
         cg->recursivelyDecReferenceCount(andNode);
         cg->decReferenceCount(shiftValueNode);
         return reg;
         }
      }
   else /* right shift*/
      {
      uint64_t shiftedMask = (maskValue >> shiftValue);
      /*
       * If the lsb of shiftedMask is set and the shiftedMask has consecutive 1s, then this operation is copying
       * consecutive 1s starting from the bit position shiftValue of the source register to the least significant bits of the destination register.
       */
      if (((shiftedMask & 1) == 1) && contiguousBits(shiftedMask))
         {
         uint32_t width = populationCount(shiftedMask);
         uint32_t shiftRemainderWidth = operandBits - shiftValue;

         bool isRightShift = (width == shiftRemainderWidth);  /* In this case, it works as a shift. */

         TR::Register *reg;
         TR::Register *sreg = cg->evaluate(sourceNode);
         if (sourceNode->getReferenceCount() == 1)
            {
            reg = sreg;
            }
         else
            {
            reg = cg->allocateRegister();
            }

         if (isRightShift)
            {
            TR::Register *shiftSrcReg = sreg;
            if (shiftNode->getOpCode().isShiftLogical())
               {
               if (operandBits < 32)
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmw, andNode, reg, sreg, operandBits - 1); // uxth or uxtb
                  shiftSrcReg = reg;
                  }
               generateLogicalShiftRightImmInstruction(cg, shiftNode, reg, shiftSrcReg, shiftValue, is64bit);
               }
            else
               {
               if (operandBits < 32)
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, andNode, reg, sreg, operandBits - 1); // sxth or sxtb
                  shiftSrcReg = reg;
                  }
               generateArithmeticShiftRightImmInstruction(cg, shiftNode, reg, shiftSrcReg, shiftValue, is64bit);
               }
            }
         else
            {
            generateUBFXInstruction(cg, shiftNode, reg, sreg, static_cast<uint32_t>(shiftValue), width, is64bit);
            }
         shiftNode->setRegister(reg);
         cg->recursivelyDecReferenceCount(andNode);
         cg->decReferenceCount(shiftValueNode);
         return reg;
         }
      }

   return NULL;
   }

static TR::Register *shiftHelper(TR::Node *node, TR::ARM64ShiftCode shiftType, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *trgReg = NULL;
   bool is64bit = node->getDataType().isInt64();
   const uint32_t operandBits = TR::DataType::getSize(node->getDataType()) * 8;

   TR::InstOpCode::Mnemonic op;

   if (secondOp == TR::iconst)
      {
      int32_t value = secondChild->getInt();
      if (value == 0)
         {
         if (firstChild->getReferenceCount() == 1)
            {
            trgReg = srcReg;
            }
         else
            {
            trgReg = cg->allocateRegister();
            generateMovInstruction(cg, node, trgReg, srcReg, is64bit);
            }
         }
      else
         {
         uint32_t shift = is64bit ? (value & 0x3F) : (value & 0x1F);
         TR::Register *shiftSrcReg = srcReg;
         trgReg = cg->allocateRegister();

         switch (shiftType)
            {
            case TR::SH_LSL:
               generateLogicalShiftLeftImmInstruction(cg, node, trgReg, shiftSrcReg, shift, is64bit);
               break;
            case TR::SH_LSR:
               if (operandBits < 32)
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmw, node, trgReg, srcReg, operandBits - 1); // uxth or uxtb
                  shiftSrcReg = trgReg;
                  }
               generateLogicalShiftRightImmInstruction(cg, node, trgReg, shiftSrcReg, shift, is64bit);
               break;
            case TR::SH_ASR:
               if (operandBits < 32)
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, node, trgReg, srcReg, operandBits - 1); // sxth or sxtb
                  shiftSrcReg = trgReg;
                  }
               generateArithmeticShiftRightImmInstruction(cg, node, trgReg, shiftSrcReg, shift, is64bit);
               break;
            default:
               TR_ASSERT(false, "Unsupported shift type.");
            }
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      TR::Register *shiftSrcReg = srcReg;
      trgReg = cg->allocateRegister();

      switch (shiftType)
         {
         case TR::SH_LSL:
            op = is64bit ? TR::InstOpCode::lslvx : TR::InstOpCode::lslvw;
            break;
         case TR::SH_LSR:
            op = is64bit ? TR::InstOpCode::lsrvx : TR::InstOpCode::lsrvw;
            if (operandBits < 32)
               {
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmw, node, trgReg, srcReg, operandBits - 1); // uxth or uxtb
               shiftSrcReg = trgReg;
               }
            break;
         case TR::SH_ASR:
            op = is64bit ? TR::InstOpCode::asrvx : TR::InstOpCode::asrvw;
            if (operandBits < 32)
               {
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmw, node, trgReg, srcReg, operandBits - 1); // sxth or sxtb
               shiftSrcReg = trgReg;
               }
            break;
         default:
            TR_ASSERT(false, "Unsupported shift type.");
         }
      generateTrg1Src2Instruction(cg, op, node, trgReg, shiftSrcReg, shiftAmountReg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles lshl
TR::Register *
OMR::ARM64::TreeEvaluator::ishlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (node->getOpCodeValue() == TR::lshl)
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();
      if (firstChild->getOpCodeValue() == TR::i2l &&
          firstChild->getRegister() == NULL &&
          firstChild->getReferenceCount() == 1 &&
          secondChild->getOpCodeValue() == TR::iconst &&
          secondChild->getInt() < 32)
         {
         // Typical IL sequence in array access (sign-extend index value and shift it to left)
         TR::Node *indexChild = firstChild->getFirstChild();
         TR::Register *srcReg = cg->evaluate(indexChild);
         TR::Register *trgReg = (indexChild->getReferenceCount() == 1) ? srcReg : cg->allocateRegister();
         int32_t bitsToShift = secondChild->getInt();
         int32_t imm = ((64 - bitsToShift) << 6) | 31; // immr = 64 - bitsToShift, imms = 31
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmx, node, trgReg, srcReg, imm);
         node->setRegister(trgReg);
         cg->recursivelyDecReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return trgReg;
         }
      }
   TR::Register *reg = generateUBFMForMaskAndShift(node, cg);
   if (reg != NULL)
      {
      return reg;
      }
   return shiftHelper(node, TR::SH_LSL, cg);
   }

// also handles lshr
TR::Register *
OMR::ARM64::TreeEvaluator::ishrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *reg = generateUBFMForMaskAndShift(node, cg);
   if (reg != NULL)
      {
      return reg;
      }
   return shiftHelper(node, TR::SH_ASR, cg);
   }

// also handles lushr
TR::Register *
OMR::ARM64::TreeEvaluator::iushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *reg = generateUBFMForMaskAndShift(node, cg);
   if (reg != NULL)
      {
      return reg;
      }
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
      TR::Register *shiftAmountSrcReg;
      TR::Register *shiftAmountReg;
      TR::Register *tempReg;
      bool useTempReg;

      if (secondChild->getOpCode().isNeg() && secondChild->getRegister() == NULL && secondChild->getReferenceCount() == 1)
         {
         // If the second child is `ineg` and not commoned, cancel out double negation
         auto shiftAmountNode = secondChild->getFirstChild();
         useTempReg = is64bit && shiftAmountNode->getReferenceCount() > 1;
         tempReg = useTempReg ? cg->allocateRegister() : NULL;
         shiftAmountSrcReg = cg->evaluate(shiftAmountNode);
         shiftAmountReg = useTempReg ? tempReg : shiftAmountSrcReg;
         if (is64bit)
            {
            // 32->64 bit sign extension: SXTW is alias of SBFM
            uint32_t imm = 0x1F; // immr=0, imms=31, N bit is pre-encoded in `sbfmx` opcode.
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmx, node, shiftAmountReg, shiftAmountSrcReg, imm);
            }
         cg->decReferenceCount(shiftAmountNode);
         }
      else
         {
         useTempReg = secondChild->getReferenceCount() > 1;
         tempReg = useTempReg ? cg->allocateRegister() : NULL;
         shiftAmountSrcReg = cg->evaluate(secondChild);
         shiftAmountReg = useTempReg ? tempReg : shiftAmountSrcReg;

         generateNegInstruction(cg, node, shiftAmountReg, shiftAmountSrcReg); // change ROL to ROR
         if (is64bit)
            {
            // 32->64 bit sign extension: SXTW is alias of SBFM
            uint32_t imm = 0x1F; // immr=0, imms=31, N bit is pre-encoded in `sbfmx` opcode.
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmx, node, shiftAmountReg, shiftAmountReg, imm);
            }
         }
      op = is64bit ? TR::InstOpCode::rorvx : TR::InstOpCode::rorvw;
      generateTrg1Src2Instruction(cg, op, node, trgReg, trgReg, shiftAmountReg);

      if (useTempReg)
         {
         cg->stopUsingRegister(tempReg);
         }
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
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
 * @param[in] retNotOp : the matching AArch64 opcode where NOT is applied to its operand
 * @param[in] is64Bit : true if regOp and regOpImm are 64bit opcode.
 * @param[in] cg : codegenerator
 * @return target register
 */
static inline TR::Register *
logicBinaryEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic regOp, TR::InstOpCode::Mnemonic regOpImm, TR::InstOpCode::Mnemonic regNotOp, bool is64Bit, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = generateShiftedBinaryOperation(node, regOp, regNotOp, cg);
   if (trgReg != NULL)
      {
      return trgReg;
      }
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = NULL;
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
      if (node->getOpCode().isXor() && (value == -1))
         {
         generateMvnInstruction(cg, node, trgReg, src1Reg, is64Bit);
         }
      else
         {
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
      }
   else
      {
      src2Reg = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, regOp, node, trgReg, src1Reg, src2Reg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

/**
 * @brief Generates ubfm instruction for shift and mask operation if possible
 *
 * @param[in] andNode: node with and opcode
 * @param[in]      cg: code generator
 *
 * @returns register which contains the result of the operation. NULL if the operation cannot be encoded in ubfm instruction.
 */
static TR::Register *
generateUBFMForShiftAndMask(TR::Node *andNode, TR::CodeGenerator *cg)
   {
   TR::Node *shiftNode = NULL;
   TR::Node *maskNode = NULL;
   if (andNode->getFirstChild()->getOpCode().isShift())
      {
      shiftNode = andNode->getFirstChild();
      maskNode = andNode->getSecondChild();
      }
   else if (andNode->getSecondChild()->getOpCode().isShift())
      {
      shiftNode = andNode->getSecondChild();
      maskNode = andNode->getFirstChild();
      }
   if (shiftNode == NULL)
      {
      return NULL;
      }

   if ((shiftNode->getReferenceCount() == 1) &&
       (shiftNode->getRegister() == NULL) &&
       shiftNode->getSecondChild()->getOpCode().isLoadConst() &&
       maskNode->getOpCode().isLoadConst())
      {
      const uint32_t operandBits = TR::DataType::getSize(shiftNode->getDataType()) * 8;
      const bool is64bit = shiftNode->getDataType().isInt64();
      int64_t shiftValue = shiftNode->getSecondChild()->getConstValue();
      if ((shiftValue <= 0) || (shiftValue > (operandBits - 1)))
         {
         return NULL;
         }
      uint64_t maskValue = maskNode->get64bitIntegralValueAsUnsigned();
      TR::Node *sourceNode = shiftNode->getFirstChild();

      if (shiftNode->getOpCode().isLeftShift())
         {
         if (maskValue < (static_cast<uint64_t>(1) << shiftValue))
            {
            /*
             * The result is always 0 in this case.
             */
            TR::Register *reg = cg->allocateRegister();
            generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, andNode, reg, 0);
            andNode->setRegister(reg);
            cg->recursivelyDecReferenceCount(shiftNode);
            cg->decReferenceCount(maskNode);
            return reg;
            }

         uint64_t shiftedMask = (maskValue >> shiftValue);
         if (((shiftedMask & 1) == 1) && contiguousBits(shiftedMask))
            {
            /*
             * shiftedMask has consecutive 1s from the least significant bits.
             * So, this shift and mask operation is copying consecutive bits starting from the least significant bits of the source register
             * to bit position shiftValue of the destination register.
             */
            uint32_t width = populationCount(shiftedMask);

            TR::Register *reg;
            TR::Register *sreg = cg->evaluate(sourceNode);
            if (sourceNode->getReferenceCount() == 1)
               {
               reg = sreg;
               }
            else
               {
               reg = cg->allocateRegister();
               }
            generateUBFIZInstruction(cg, andNode, reg, sreg, static_cast<uint32_t>(shiftValue), width, is64bit);
            andNode->setRegister(reg);
            cg->recursivelyDecReferenceCount(shiftNode);
            cg->decReferenceCount(maskNode);
            return reg;
            }
         }
      else /* right shift*/
         {
         /*
          * If the lsb of maskValue is set and the msb is not set and the maskValue has consecutive 1s, then this operation is copying
          * consecutive 1s starting from the bit position shiftValue of the source register to the least significant bits of the destination register.
          * We consider arithmetic shift only.
          */
         if ((!shiftNode->getOpCode().isShiftLogical()) && ((maskValue & 1) == 1) && (((maskValue >> (operandBits - 1)) & 1) == 0) && contiguousBits(maskValue))
            {
            uint32_t width = populationCount(maskValue);
            uint32_t shiftRemainderWidth = operandBits - shiftValue;

            if (width > shiftRemainderWidth)
               {
               if (shiftNode->getOpCode().isShiftLogical())
                  {
                  /* If it is a logical shift, we can ignore the mask beyond the msb. */
                  width = shiftRemainderWidth;
                  }
               else
                  {
                  return NULL;
                  }
               }
            bool isLogicalShiftRight = (width == shiftRemainderWidth);  /* In this case, it works as a logical shift right. */

            TR::Register *reg;
            TR::Register *sreg = cg->evaluate(sourceNode);
            TR::Register *shiftSrcReg = sreg;
            if (sourceNode->getReferenceCount() == 1)
               {
               reg = sreg;
               }
            else
               {
               reg = cg->allocateRegister();
               }
            if (isLogicalShiftRight)
               {
               if (operandBits < 32)
                  {
                  generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ubfmw, andNode, reg, sreg, operandBits - 1); // uxth or uxtb
                  shiftSrcReg = reg;
                  }
               generateLogicalShiftRightImmInstruction(cg, andNode, reg, shiftSrcReg, shiftValue, is64bit);
               }
            else
               {
               generateUBFXInstruction(cg, andNode, reg, sreg, static_cast<uint32_t>(shiftValue), width, is64bit);
               }
            andNode->setRegister(reg);
            cg->recursivelyDecReferenceCount(shiftNode);
            cg->decReferenceCount(maskNode);
            return reg;
            }
         }
      }
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *reg = generateUBFMForShiftAndMask(node, cg);
   if (reg != NULL)
      {
      return reg;
      }
   // boolean and of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::andw, TR::InstOpCode::andimmw, TR::InstOpCode::bicw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *reg = generateUBFMForShiftAndMask(node, cg);
   if (reg != NULL)
      {
      return reg;
      }
   // boolean and of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::andx, TR::InstOpCode::andimmx, TR::InstOpCode::bicx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean or of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::orrw, TR::InstOpCode::orrimmw, TR::InstOpCode::ornw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean or of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::orrx, TR::InstOpCode::orrimmx, TR::InstOpCode::ornx, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean xor of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::eorw, TR::InstOpCode::eorimmw, TR::InstOpCode::eonw, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // boolean xor of 2 integers
   return logicBinaryEvaluator(node, TR::InstOpCode::eorx, TR::InstOpCode::eorimmx, TR::InstOpCode::eonx, true, cg);
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
