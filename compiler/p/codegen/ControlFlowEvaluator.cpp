/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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

#define OPT_DETAILS_EVALUATE_INT_COMPARE_TO_CONDITION_REGISTER "O^O evaluateIntCompareToConditionRegister: "
#define OPT_DETAILS_CHECK_SELECT_REVERSE "O^O checkSelectReverse: "

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg);
static void switchDispatch(TR::Node *node, bool fromTableEval, TR::CodeGenerator *cg);
static bool isGlDepsUnBalanced(TR::Node *node, TR::CodeGenerator *cg);
static void lookupScheme1(TR::Node *node, bool unbalanced, bool fromTableEval, TR::CodeGenerator *cg);
static void lookupScheme2(TR::Node *node, bool unbalanced, bool fromTableEval, TR::CodeGenerator *cg);
static void lookupScheme3(TR::Node *node, bool unbalanced, TR::CodeGenerator *cg);
static void lookupScheme4(TR::Node *node, TR::CodeGenerator *cg);

/**
 * @brief Represents an integer comparison condition.
 */
enum class CompareCondition
   {
   eq,
   ne,
   lt,
   ge,
   gt,
   le
   };

/**
 * \brief
 *    Represents a condition as a (potentially reversed) bit in a condition register field.
 */
struct CRCompareCondition
   {
   TR::RealRegister::CRCC crcc; //< Which bit in the CR field the result will be placed in.
   bool isReversed; //< Whether the bit in the CR field is the complement of the comparison result.

   CRCompareCondition(TR::RealRegister::CRCC crcc, bool isReversed) : crcc(crcc), isReversed(isReversed) {}
   };

/**
 * \brief
 *    Gets the placement of a condition in a CR field after a compare instruction is run.
 *
 * \param cond
 *    The condition which should be checked for.
 *
 * \return
 *    The placement of the provided condition in a CR field.
 */
CRCompareCondition compareConditionInCR(CompareCondition cond)
   {
   switch (cond)
      {
      case CompareCondition::eq:
         return CRCompareCondition(TR::RealRegister::CRCC_EQ, false);
      case CompareCondition::ne:
         return CRCompareCondition(TR::RealRegister::CRCC_EQ, true);
      case CompareCondition::lt:
         return CRCompareCondition(TR::RealRegister::CRCC_LT, false);
      case CompareCondition::ge:
         return CRCompareCondition(TR::RealRegister::CRCC_LT, true);
      case CompareCondition::gt:
         return CRCompareCondition(TR::RealRegister::CRCC_GT, false);
      case CompareCondition::le:
         return CRCompareCondition(TR::RealRegister::CRCC_GT, true);
      default:
         TR_ASSERT_FATAL(false, "Invalid CompareCondition %d", static_cast<int>(cond));
      }
   }

/**
 * \brief
 *    Returns a condition representing the logical complement of the provided condition.
 *
 * \param cond
 *    The condition to be reversed.
 *
 * \return
 *    The logical complement of cond.
 */
CompareCondition reverseCondition(CompareCondition cond)
   {
   switch (cond)
      {
      case CompareCondition::eq:
         return CompareCondition::ne;
      case CompareCondition::ne:
         return CompareCondition::eq;
      case CompareCondition::lt:
         return CompareCondition::ge;
      case CompareCondition::ge:
         return CompareCondition::lt;
      case CompareCondition::gt:
         return CompareCondition::le;
      case CompareCondition::le:
         return CompareCondition::gt;
      default:
         TR_ASSERT_FATAL(false, "Invalid CompareCondition %d", static_cast<int>(cond));
      }
   }

/**
 * \brief
 *    Returns a condition that should be used if the order of operands to the compare instruction
 *    is flipped.
 *
 * \param cond
 *    The original condition.
 *
 * \return
 *    The condition that should be used if the compare operands are flipped.
 */
CompareCondition flipConditionOrder(CompareCondition cond)
   {
   switch (cond)
      {
      case CompareCondition::eq:
         return CompareCondition::eq;
      case CompareCondition::ne:
         return CompareCondition::ne;
      case CompareCondition::lt:
         return CompareCondition::gt;
      case CompareCondition::ge:
         return CompareCondition::le;
      case CompareCondition::gt:
         return CompareCondition::lt;
      case CompareCondition::le:
         return CompareCondition::ge;
      default:
         TR_ASSERT_FATAL(false, "Invalid CompareCondition %d", static_cast<int>(cond));
      }
   }

/**
 * \brief
 *    Gets the opcode of a conditional branch instruction that branches when the provided condition
 *    is true.
 *
 * \param cond
 *    The condition upon which the instruction should branch.
 *
 * \return
 *    An extended mnemonic for the bc instruction that branches when the provided condition is true.
 */
TR::InstOpCode::Mnemonic compareConditionToBranch(CompareCondition cond)
   {
   switch (cond)
      {
      case CompareCondition::eq:
         return TR::InstOpCode::beq;
      case CompareCondition::ne:
         return TR::InstOpCode::bne;
      case CompareCondition::lt:
         return TR::InstOpCode::blt;
      case CompareCondition::ge:
         return TR::InstOpCode::bge;
      case CompareCondition::gt:
         return TR::InstOpCode::bgt;
      case CompareCondition::le:
         return TR::InstOpCode::ble;
      default:
         TR_ASSERT_FATAL(false, "Invalid CompareCondition %d", static_cast<int>(cond));
      }
   }

/**
 * \brief
 *    Gets the opcode of an isel instruction that selects based on the given condition code bit.
 *
 * \param cond
 *    The condition code bit to select based on.
 *
 * \return
 *    An extended mnemonic for the isel instruction that selects based on the given condition code
 *    bit.
 */
TR::InstOpCode::Mnemonic compareConditionToISel(TR::RealRegister::CRCC crcc)
   {
   switch (crcc)
      {
      case TR::RealRegister::CRCC_EQ:
         return TR::InstOpCode::iseleq;
      case TR::RealRegister::CRCC_LT:
         return TR::InstOpCode::isellt;
      case TR::RealRegister::CRCC_GT:
         return TR::InstOpCode::iselgt;
      case TR::RealRegister::CRCC_FU:
         return TR::InstOpCode::iselun;
      default:
         TR_ASSERT_FATAL(false, "Invalid CRCC %d in compareConditionToISel", static_cast<int>(crcc));
      }
   }

/**
 * \brief
 *    Represents information about a comparison to be performed.
 */
struct CompareInfo
   {
   CompareCondition cond; //< The condition that is being evaluated for this comparison.
   TR::DataTypes type; //< The types of the operands being compared.
   bool isUnsigned; //< Whether an unsigned integral comparison should be performed.
   bool isUnorderedTrue; //< Whether unordered floating-point operands should evaluate as true.

   CompareInfo(CompareCondition cond, TR::DataTypes type, bool isUnsigned, bool isUnorderedTrue)
      : cond(cond), type(type), isUnsigned(isUnsigned), isUnorderedTrue(isUnorderedTrue) {}
   };

/**
 * \brief
 *    Determines whether the provided value is valid for a 16-bit signed SI field in an
 *    instruction.
 *
 * \param value
 *    The value to be checked.
 *
 * \return
 *    true if the value provided can be encoded in a 16-bit SI field; false otherwise.
 */
bool is16BitSignedImmediate(int64_t value)
   {
   return value >= -0x8000 && value < 0x8000;
   }

/**
 * \brief
 *    Determines whether the provided value is valid for a 16-bit unsigned UI field in an
 *    instruction.
 *
 * \param value
 *    The value to be checked.
 *
 * \return
 *    true if the value provided can be encoded in a 16-bit UI field; false otherwise.
 */
bool is16BitUnsignedImmediate(uint64_t value)
   {
   return value < 0x10000;
   }

/**
 * \brief
 *    Evaluates a node, sign-extending it to fill a register if its type is less than the size of
 *    a register.
 *
 *    The returned register may or may not be the same as the value of \c node->getRegister() so it
 *    is necessary to call stopUsingExtendedRegister when done using the returned register.
 *
 * \param node
 *    The node to evaluate.
 *
 * \param extendInt32
 *    This flag determines whether 32-bit integers need to be sign-extended when running on 64-bit.
 *    If only 32-bit instructions will be used on the register, then this flag does not need to be
 *    set.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    A register containing the evaluated value of the provided node, sign-extended to the desired
 *    length.
 */
static TR::Register *evaluateAndSignExtend(TR::Node *node, bool extendInt32, TR::CodeGenerator *cg)
   {
   TR::Register *srcReg = cg->evaluate(node);

   switch (node->getDataType().getDataType())
      {
      case TR::Int8:
         {
         TR::Register *trgReg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::extsb, node, trgReg, srcReg);
         return trgReg;
         }

      case TR::Int16:
         {
         TR::Register *trgReg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, srcReg);
         return trgReg;
         }

      case TR::Int32:
         if (extendInt32 && cg->comp()->target().is64Bit())
            {
            TR::Register *trgReg = cg->allocateRegister();
            generateTrg1Src1Instruction(cg, TR::InstOpCode::extsw, node, trgReg, srcReg);
            return trgReg;
            }
         else
            {
            return srcReg;
            }

      default:
         return srcReg;
      }
   }

/**
 * \brief
 *    Evaluates a node, zero-extending it to fill a register if its type is less than the size of
 *    a register.
 *
 *    The returned register may or may not be the same as the value of \c node->getRegister() so it
 *    is necessary to call stopUsingExtendedRegister when done using the returned register.
 *
 * \param node
 *    The node to evaluate.
 *
 * \param extendInt32
 *    This flag determines whether 32-bit integers need to be zero-extended when running on 64-bit.
 *    If only 32-bit instructions will be used on the register, then this flag does not need to be
 *    set.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    A register containing the evaluated value of the provided node, zero-extended to the desired
 *    length.
 */
static TR::Register *evaluateAndZeroExtend(TR::Node *node, bool extendInt32, TR::CodeGenerator *cg)
   {
   TR::Register *srcReg = cg->evaluate(node);

   switch (node->getDataType().getDataType())
      {
      case TR::Int8:
         {
         TR::Register *trgReg = cg->allocateRegister();
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, srcReg, 0, 0xffu);
         return trgReg;
         }

      case TR::Int16:
         {
         TR::Register *trgReg = cg->allocateRegister();
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, srcReg, 0, 0xffffu);
         return trgReg;
         }

      case TR::Int32:
         if (extendInt32 && cg->comp()->target().is64Bit())
            {
            TR::Register *trgReg = cg->allocateRegister();
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, srcReg, 0, 0xffffffffu);
            return trgReg;
            }
         else
            {
            return srcReg;
            }

      default:
         return srcReg;
      }
   }

/**
 * \brief
 *    Evaluates a node, sign- or zero-extending it to fill a register if its type is less than the
 *    size of a register.
 *
 *    The returned register may or may not be the same as the value of \c node->getRegister() so it
 *    is necessary to call stopUsingExtendedRegister when done using the returned register.
 *
 * \param node
 *    The node to evaluate.
 *
 * \param isUnsigned
 *    If true, the register will be zero-extended; otherwise, it will be sign-extended.
 *
 * \param extendInt32
 *    This flag determines whether 32-bit integers need to be extended when running on 64-bit. If
 *    only 32-bit instructions will be used on the register, then this flag does not need to be
 *    set.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    A register containing the evaluated value of the provided node, sign- or zero-extended to the
 *    desired length.
 */
static TR::Register *evaluateAndExtend(TR::Node *node, bool isUnsigned, bool extendInt32, TR::CodeGenerator *cg)
   {
   return isUnsigned ? evaluateAndZeroExtend(node, extendInt32, cg) : evaluateAndSignExtend(node, extendInt32, cg);
   }

/**
 * \brief
 *    Stops using a register returned from the evaluateAndExtend, evaluateAndZeroExtend, or
 *    evaluateAndSignExtend helpers.
 *
 * \param reg
 *    The sign- or zero-extended register to be freed.
 *
 * \param node
 *    The node from which the provided register was evaluated.
 *
 * \param cg
 *    The code generator.
 */
static void stopUsingExtendedRegister(TR::Register *reg, TR::Node *node, TR::CodeGenerator *cg)
   {
   if (reg != node->getRegister())
      cg->stopUsingRegister(reg);
   }

/**
 * \brief
 *    Evaluates a 64-bit comparison on 32-bit machines using register pairs for the operands.
 *
 * \param condReg
 *    The condition register in which the result of the comparison should be placed.
 *
 * \param node
 *    The node for which this comparison is being generated.
 *
 * \param firstChild
 *    The node corresponding to the left-hand side operand of the comparison.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    The condition code corresponding to the CR field bit into which the result of the comparison
 *    was placed.
 */
CompareCondition evaluateDualIntCompareToConditionRegister(
      TR::Register *condReg,
      TR::Node *node,
      TR::Node *firstChild,
      TR::Node *secondChild,
      const CompareInfo& compareInfo,
      TR::CodeGenerator *cg)
   {
   TR::Register *condReg2 = cg->allocateRegister(TR_CCR);
   TR::Register *firstReg = cg->evaluate(firstChild);

   if (secondChild->getOpCode().isLoadConst() && !secondChild->getRegister() && secondChild->getReferenceCount() == 1)
      {
      int32_t secondHi = secondChild->getLongIntHigh();
      int32_t secondLo = secondChild->getLongIntLow();

      if (compareInfo.isUnsigned && is16BitUnsignedImmediate(secondHi))
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, firstReg->getHighOrder(), secondHi);
         }
      else if (!compareInfo.isUnsigned && is16BitSignedImmediate(secondHi))
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, firstReg->getHighOrder(), secondHi);
         }
      else
         {
         TR::Register *secondHiReg = cg->allocateRegister();

         loadConstant(cg, node, secondHi, secondHiReg);
         generateTrg1Src2Instruction(cg, compareInfo.isUnsigned ? TR::InstOpCode::cmpl4 : TR::InstOpCode::cmp4, node, condReg, firstReg->getHighOrder(), secondHiReg);

         cg->stopUsingRegister(secondHiReg);
         }

      if (is16BitUnsignedImmediate(secondLo))
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg2, firstReg->getLowOrder(), secondLo);
         }
      else
         {
         TR::Register *secondLoReg = cg->allocateRegister();

         loadConstant(cg, node, secondLo, secondLoReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg2, firstReg->getHighOrder(), secondLoReg);

         cg->stopUsingRegister(secondLoReg);
         }
      }
   else
      {
      TR::Register *secondReg = cg->evaluate(secondChild);

      generateTrg1Src2Instruction(cg, compareInfo.isUnsigned ? TR::InstOpCode::cmpl4 : TR::InstOpCode::cmp4, node, condReg, firstReg->getHighOrder(), secondReg->getHighOrder());
      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg2, firstReg->getLowOrder(), secondReg->getLowOrder());
      }

   switch (compareInfo.cond)
      {
      case CompareCondition::eq:
         // x_h == y_h && x_l == y_l
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crand, node, condReg, condReg2, condReg,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RB));
         break;

      case CompareCondition::ne:
         // x_h != y_h || x_l != y_l
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crnand, node, condReg, condReg2, condReg,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RB));
         break;

      case CompareCondition::lt:
         // x_h < y_h || (x_h == y_h && x_l < y_l)
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crand, node, condReg2, condReg, condReg2,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB));
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, condReg, condReg2, condReg,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB));
         break;

      case CompareCondition::ge:
         // x_h > y_h || (x_h == y_h && x_l >= y_l)
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crandc, node, condReg2, condReg, condReg2,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB));
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, condReg, condReg2, condReg,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RB));
         break;

      case CompareCondition::gt:
         // x_h > y_h || (x_h == y_h && x_l > y_l)
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crand, node, condReg2, condReg, condReg2,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RB));
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, condReg, condReg2, condReg,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RB));
         break;

      case CompareCondition::le:
         // x_h < y_h || (x_h == y_h && x_l <= y_l)
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crandc, node, condReg2, condReg, condReg2,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RB));
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, condReg, condReg2, condReg,
            (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_EQ << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB));
         break;

      default:
         TR_ASSERT_FATAL(false, "Invalid CompareCondition %d", static_cast<int>(compareInfo.cond));
      }

   cg->stopUsingRegister(condReg2);
   return CompareCondition::eq;
   }

static bool registerRecentlyWritten(TR::Register *reg, uint32_t windowSize, TR::CodeGenerator *cg)
   {
   uint32_t i = 0;
   TR::Instruction *cursor = cg->getAppendInstruction();

   while (cursor && i < windowSize)
      {
      if (cursor->getOpCode().getMaxBinaryLength() != 0)
         {
         if (cursor->getTargetRegister(0) == reg)
            return true;
         i++;
         }

      cursor = cursor->getPrev();
      }

   return false;
   }

/**
 * \brief
 *    Evaluates an integral comparison to all bits of a CR field using a cmp or cmpi instruction.
 *
 *    Under certain conditions, this function may actually evaluate the comparison by reversing its
 *    operands, in which case this routine will return true to indicate that this has been done.
 *
 *    This function is not capable of handling 64-bit integral comparisons on 32-bit machines, as
 *    such comparisons cannot be performed in a single instruction and would require many extra CR
 *    logical instructions, rendering it too inefficient for general use.
 *
 * \param condReg
 *    The condition register in which the result of the comparison should be placed.
 *
 * \param node
 *    The node for which this comparison is being generated.
 *
 * \param firstChild
 *    The node corresponding to the left-hand side operand of the comparison.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison. Only the fields relating
 *    to the types of values being compared (i.e. type and signedness) will be used.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    true if the order of operands to the compare instruction was reversed; false otherwise.
 */
bool evaluateThreeWayIntCompareToConditionRegister(
      TR::Register *condReg,
      TR::Node *node,
      TR::Node *firstChild,
      TR::Node *secondChild,
      const CompareInfo& compareInfo,
      TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic cmpOp;
   TR::InstOpCode::Mnemonic cmpiOp;

   bool is64Bit;

   switch (compareInfo.type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         is64Bit = false;
         break;
      case TR::Int64:
         is64Bit = true;
         break;
      case TR::Address:
         is64Bit = cg->comp()->target().is64Bit();
         break;
      default:
         TR_ASSERT_FATAL_WITH_NODE(node, false, "Cannot call evaluateThreeWayIntCompareToConditionRegister with data type %s", TR::DataType::getName(compareInfo.type));
      }

   TR_ASSERT_FATAL(!is64Bit || cg->comp()->target().is64Bit(), "Cannot call evaluateThreeWayIntCompareToConditionRegister for 64-bit values on 32-bit");

   if (is64Bit)
      {
      if (compareInfo.isUnsigned)
         {
         cmpOp = TR::InstOpCode::cmpl8;
         cmpiOp = TR::InstOpCode::cmpli8;
         }
      else
         {
         cmpOp = TR::InstOpCode::cmp8;
         cmpiOp = TR::InstOpCode::cmpi8;
         }
      }
   else
      {
      if (compareInfo.isUnsigned)
         {
         cmpOp = TR::InstOpCode::cmpl4;
         cmpiOp = TR::InstOpCode::cmpli4;
         }
      else
         {
         cmpOp = TR::InstOpCode::cmp4;
         cmpiOp = TR::InstOpCode::cmpi4;
         }
      }

   TR::Register *firstReg = evaluateAndExtend(firstChild, compareInfo.isUnsigned, false, cg);
   bool canUseCmpi = secondChild->getOpCode().isLoadConst() &&
      (compareInfo.isUnsigned
         ? is16BitUnsignedImmediate(secondChild->get64bitIntegralValueAsUnsigned())
         : is16BitSignedImmediate(secondChild->get64bitIntegralValue()));

   static bool disableFlipCompare = feGetEnv("TR_DisableFlipCompare") != NULL;
   CompareCondition cond = compareInfo.cond;
   bool wasFlipped = false;

   if (canUseCmpi)
      {
      generateTrg1Src1ImmInstruction(cg, cmpiOp, node, condReg, firstReg, secondChild->get64bitIntegralValue());
      }
   else if (
      (firstReg->containsInternalPointer() || registerRecentlyWritten(firstReg, 4, cg)) &&
      performTransformation(
         cg->comp(),
         "%sflipping order of compare operands (n%dn, n%dn) while evaluating n%dn to avoid P6 FXU reject",
         OPT_DETAILS_EVALUATE_INT_COMPARE_TO_CONDITION_REGISTER,
         firstChild->getGlobalIndex(),
         secondChild->getGlobalIndex(),
         node->getGlobalIndex()
      )
   )
      {
      TR::Register *secondReg = evaluateAndExtend(secondChild, compareInfo.isUnsigned, false, cg);
      generateTrg1Src2Instruction(cg, cmpOp, node, condReg, secondReg, firstReg);
      stopUsingExtendedRegister(secondReg, secondChild, cg);

      wasFlipped = true;
      }
   else
      {
      TR::Register *secondReg = evaluateAndExtend(secondChild, compareInfo.isUnsigned, false, cg);
      generateTrg1Src2Instruction(cg, cmpOp, node, condReg, firstReg, secondReg);
      stopUsingExtendedRegister(secondReg, secondChild, cg);
      }

   stopUsingExtendedRegister(firstReg, firstChild, cg);
   return wasFlipped;
   }

/**
 * \brief
 *    Evaluates an integral comparison to a single bit of a CR field.
 *
 * \param condReg
 *    The condition register in which the result of the comparison should be placed.
 *
 * \param node
 *    The node for which this comparison is being generated.
 *
 * \param firstChild
 *    The node corresponding to the left-hand side operand of the comparison.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    The condition code corresponding to the CR field bit into which the result of the comparison
 *    was placed.
 */
CompareCondition evaluateIntCompareToConditionRegister(
   TR::Register *condReg,
   TR::Node *node,
   TR::Node *firstChild,
   TR::Node *secondChild,
   const CompareInfo& compareInfo,
   TR::CodeGenerator *cg
)
   {
   if (compareInfo.type == TR::Int64 && !cg->comp()->target().is64Bit())
      return evaluateDualIntCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg);

   if (evaluateThreeWayIntCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg))
      return flipConditionOrder(compareInfo.cond);
   else
      return compareInfo.cond;
   }

/**
 * \brief
 *    Evaluates a floating-point comparison to a single bit of a CR field.
 *
 * \param condReg
 *    The condition register in which the result of the comparison should be placed.
 *
 * \param node
 *    The node for which this comparison is being generated.
 *
 * \param firstChild
 *    The node corresponding to the left-hand side operand of the comparison.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    The condition code corresponding to the CR field bit into which the result of the comparison
 *    was placed.
 */
CompareCondition evaluateFloatCompareToConditionRegister(
      TR::Register *condReg,
      TR::Node *node,
      TR::Node *firstChild,
      TR::Node *secondChild,
      const CompareInfo& compareInfo,
      TR::CodeGenerator *cg)
   {
   CRCompareCondition crCond = compareConditionInCR(compareInfo.cond);
   TR::Register *firstReg = cg->evaluate(firstChild);
   TR::Register *secondReg = cg->evaluate(secondChild);

   generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, firstReg, secondReg);

   // When we're using the negation of a CR bit (e.g. x >= y is checked as x < y), we must take
   // into account the possibility that the two operands were unordered for a floating-point
   // comparison. The isUnorderedTrue flag tells us whether two unordered operands should return
   // true, so if that is different from whether the CR bit is negated, we must flip that CR bit
   // if the operands were unordered. Since both CR bits can never be set simultaneously, this can
   // be done with either cror or crxor, as both are equivalent.
   if (crCond.isReversed != compareInfo.isUnorderedTrue)
      generateTrg1Src2ImmInstruction(
         cg,
         TR::InstOpCode::crxor,
         node,
         condReg,
         condReg,
         condReg,
         (crCond.crcc << TR::RealRegister::pos_RT) | (crCond.crcc << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_FU << TR::RealRegister::pos_RB)
      );

   return compareInfo.cond;
   }

/**
 * \brief
 *    Evaluates an arbitrary comparison to a single bit of a CR field.
 *
 * \param condReg
 *    The condition register in which the result of the comparison should be placed.
 *
 * \param node
 *    The node for which this comparison is being generated.
 *
 * \param firstChild
 *    The node corresponding to the left-hand side operand of the comparison.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    The condition code corresponding to the CR field bit into which the result of the comparison
 *    was placed.
 */
CompareCondition evaluateCompareToConditionRegister(
      TR::Register *condReg,
      TR::Node *node,
      TR::Node *firstChild,
      TR::Node *secondChild,
      const CompareInfo& compareInfo,
      TR::CodeGenerator *cg)
   {
   switch (compareInfo.type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
      case TR::Address:
         return evaluateIntCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg);

      case TR::Float:
      case TR::Double:
         return evaluateFloatCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg);

      default:
         TR_ASSERT_FATAL_WITH_NODE(node, false, "Unrecognized comparison type %s", TR::DataType::getName(compareInfo.type));
      }
   }

CompareInfo getCompareInfo(TR::ILOpCode op)
   {
   TR::DataTypes type = op.expectedChildType(0);

   switch (op.getOpCodeValue())
      {
      case TR::bcmpeq:
      case TR::scmpeq:
      case TR::icmpeq:
      case TR::lcmpeq:
      case TR::acmpeq:
      case TR::fcmpeq:
      case TR::dcmpeq:
         return CompareInfo(CompareCondition::eq, type, false, false);

      case TR::fcmpequ:
      case TR::dcmpequ:
         return CompareInfo(CompareCondition::eq, type, false, true);

      case TR::bcmpne:
      case TR::scmpne:
      case TR::icmpne:
      case TR::lcmpne:
      case TR::acmpne:
      case TR::fcmpne:
      case TR::dcmpne:
         return CompareInfo(CompareCondition::ne, type, false, false);

      case TR::fcmpneu:
      case TR::dcmpneu:
         return CompareInfo(CompareCondition::ne, type, false, true);

      case TR::bcmplt:
      case TR::scmplt:
      case TR::icmplt:
      case TR::lcmplt:
      case TR::fcmplt:
      case TR::dcmplt:
         return CompareInfo(CompareCondition::lt, type, false, false);

      case TR::bucmplt:
      case TR::sucmplt:
      case TR::iucmplt:
      case TR::lucmplt:
      case TR::acmplt:
         return CompareInfo(CompareCondition::lt, type, true, false);

      case TR::fcmpltu:
      case TR::dcmpltu:
         return CompareInfo(CompareCondition::lt, type, false, true);

      case TR::bcmpge:
      case TR::scmpge:
      case TR::icmpge:
      case TR::lcmpge:
      case TR::fcmpge:
      case TR::dcmpge:
         return CompareInfo(CompareCondition::ge, type, false, false);

      case TR::bucmpge:
      case TR::sucmpge:
      case TR::iucmpge:
      case TR::lucmpge:
      case TR::acmpge:
         return CompareInfo(CompareCondition::ge, type, true, false);

      case TR::fcmpgeu:
      case TR::dcmpgeu:
         return CompareInfo(CompareCondition::ge, type, false, true);

      case TR::bcmpgt:
      case TR::scmpgt:
      case TR::icmpgt:
      case TR::lcmpgt:
      case TR::fcmpgt:
      case TR::dcmpgt:
         return CompareInfo(CompareCondition::gt, type, false, false);

      case TR::bucmpgt:
      case TR::sucmpgt:
      case TR::iucmpgt:
      case TR::lucmpgt:
      case TR::acmpgt:
         return CompareInfo(CompareCondition::gt, type, true, false);

      case TR::fcmpgtu:
      case TR::dcmpgtu:
         return CompareInfo(CompareCondition::gt, type, false, true);

      case TR::bcmple:
      case TR::scmple:
      case TR::icmple:
      case TR::lcmple:
      case TR::fcmple:
      case TR::dcmple:
         return CompareInfo(CompareCondition::le, type, false, false);

      case TR::bucmple:
      case TR::sucmple:
      case TR::iucmple:
      case TR::lucmple:
      case TR::acmple:
         return CompareInfo(CompareCondition::le, type, true, false);

      case TR::fcmpleu:
      case TR::dcmpleu:
         return CompareInfo(CompareCondition::le, type, false, true);

      default:
         return CompareInfo(CompareCondition::eq, TR::DataTypes::NoType, false, false);
      }
   }

CompareCondition evaluateToConditionRegister(TR::Register *condReg, TR::Node *node, TR::Node *condNode, TR::CodeGenerator *cg)
   {
   static bool disableCondRegEval = feGetEnv("TR_DisableCondRegEval") != NULL;

   if (!disableCondRegEval && !condNode->getRegister() && condNode->getReferenceCount() == 1)
      {
      auto compareInfo = getCompareInfo(condNode->getOpCode());

      if (compareInfo.type != TR::DataTypes::NoType &&
          performTransformation(cg->comp(), "%sevaluating n%dn directly to a condition register\n", OPT_DETAILS_EVALUATE_INT_COMPARE_TO_CONDITION_REGISTER, condNode->getGlobalIndex()))
         {
         auto cond = evaluateCompareToConditionRegister(condReg, condNode, condNode->getFirstChild(), condNode->getSecondChild(), compareInfo, cg);

         cg->decReferenceCount(condNode->getFirstChild());
         cg->decReferenceCount(condNode->getSecondChild());
         return cond;
         }
      }

   TR::Register *condIntReg = cg->evaluate(condNode);

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, condIntReg, 0);
   return CompareCondition::ne;
   }

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
      if (isHint)
         generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
               generateRegisterDependencyConditions(cg, thirdChild, 0));
      else
         generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
               generateRegisterDependencyConditions(cg, thirdChild, 0));
      cg->decReferenceCount(thirdChild);
      }
   else
      {
      if (isHint)
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
         if (isHint)
            generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, destinationLabel, condReg, deps);
         else
            generateDepConditionalBranchInstruction(cg, branchOp, node, destinationLabel, condReg, deps);
         }
      else
         {
         if (isHint)
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
         if (isHint)
            generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         else
            generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         cg->decReferenceCount(thirdChild);
         }
      else
         {
         if (isHint)
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
   generateAdminInstruction(cg, TR::InstOpCode::retn, node);
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
   generateAdminInstruction(cg, TR::InstOpCode::retn, node);
   generateDepInstruction(cg, TR::InstOpCode::blr, node, dependencies);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

// areturn handled by ireturnEvaluator

TR::Register *OMR::Power::TreeEvaluator::returnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, TR::InstOpCode::retn, node);
   generateInstruction(cg, TR::InstOpCode::blr, node);
   return NULL;
   }

bool checkSelectReverse(TR::CodeGenerator *cg, TR::Node *node, TR::Node *&trueNode, TR::Node *&falseNode)
   {
   static bool disableSelectReverse = feGetEnv("TR_DisableSelectReverse") != NULL;

   cg->evaluate(trueNode);
   cg->evaluate(falseNode);

   if (!disableSelectReverse && !cg->canClobberNodesRegister(trueNode) && cg->canClobberNodesRegister(falseNode) &&
       performTransformation(cg->comp(), "%sreversing condition on n%dn to avoid a register shuffle\n", OPT_DETAILS_CHECK_SELECT_REVERSE, node->getGlobalIndex()))
      {
      TR::Node *tmpNode = trueNode;
      trueNode = falseNode;
      falseNode = tmpNode;

      return true;
      }

   return false;
   }

// Also handles bselect, sselect, aselect, and lselect
TR::Register *OMR::Power::TreeEvaluator::iselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *condNode = node->getFirstChild();
   TR::Node *trueNode = node->getSecondChild();
   TR::Node *falseNode = node->getThirdChild();

   TR::Register *trgReg;
   TR::Register *condReg = cg->allocateRegister(TR_CCR);

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      {
      TR::Register *trueReg = cg->evaluate(trueNode);
      TR::Register *falseReg = cg->evaluate(falseNode);

      auto cond = evaluateToConditionRegister(condReg, node, condNode, cg);

      CRCompareCondition crCond = compareConditionInCR(cond);
      TR::InstOpCode::Mnemonic iselOp = compareConditionToISel(crCond.crcc);

      if (node->getOpCodeValue() == TR::lselect && !cg->comp()->target().is64Bit())
         {
         trgReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
         if (crCond.isReversed)
            {
            generateTrg1Src3Instruction(cg, iselOp, node, trgReg->getLowOrder(), falseReg->getLowOrder(), trueReg->getLowOrder(), condReg);
            generateTrg1Src3Instruction(cg, iselOp, node, trgReg->getHighOrder(), falseReg->getHighOrder(), trueReg->getHighOrder(), condReg);
            }
         else
            {
            generateTrg1Src3Instruction(cg, iselOp, node, trgReg->getLowOrder(), trueReg->getLowOrder(), falseReg->getLowOrder(), condReg);
            generateTrg1Src3Instruction(cg, iselOp, node, trgReg->getHighOrder(), trueReg->getHighOrder(), falseReg->getHighOrder(), condReg);
            }
         }
      else
         {
         trgReg = cg->allocateRegister();
         if (crCond.isReversed)
            generateTrg1Src3Instruction(cg, iselOp, node, trgReg, falseReg, trueReg, condReg);
         else
            generateTrg1Src3Instruction(cg, iselOp, node, trgReg, trueReg, falseReg, condReg);
         }

      TR_ASSERT_FATAL_WITH_NODE(
         node,
         !trueReg->containsInternalPointer() && !falseReg->containsInternalPointer(),
         "Select nodes cannot have children that are internal pointers"
      );
      if (trueReg->containsCollectedReference() || falseReg->containsCollectedReference())
         trgReg->setContainsCollectedReference();
      }
   else
      {
      bool selectReverse = checkSelectReverse(cg, node, trueNode, falseNode);

      trgReg = cg->gprClobberEvaluate(trueNode);
      TR::Register *falseReg = cg->evaluate(falseNode);

      TR_ASSERT_FATAL_WITH_NODE(
         node,
         !trgReg->containsInternalPointer() && !falseReg->containsInternalPointer(),
         "Select nodes cannot have children that are internal pointers"
      );
      if (falseReg->containsCollectedReference())
         trgReg->setContainsCollectedReference();

      auto cond = evaluateToConditionRegister(condReg, node, condNode, cg);
      if (selectReverse)
         cond = reverseCondition(cond);

      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();

      TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
      endLabel->setEndInternalControlFlow();

      generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);
      generateConditionalBranchInstruction(cg, compareConditionToBranch(cond), node, endLabel, condReg);

      TR::RegisterDependencyConditions *deps;

      if (node->getOpCodeValue() == TR::lselect && !cg->comp()->target().is64Bit())
         {
         deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg->trMemory());
         deps->addPostCondition(condReg, TR::RealRegister::NoReg);
         deps->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(falseReg->getLowOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(falseReg->getHighOrder(), TR::RealRegister::NoReg);

         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg->getLowOrder(), falseReg->getLowOrder());
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg->getHighOrder(), falseReg->getHighOrder());
         }
      else
         {
         deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg->trMemory());
         deps->addPostCondition(condReg, TR::RealRegister::NoReg);
         deps->addPostCondition(trgReg, TR::RealRegister::NoReg);
         deps->addPostCondition(falseReg, TR::RealRegister::NoReg);

         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, falseReg);
         }

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(condNode);
   cg->decReferenceCount(trueNode);
   cg->decReferenceCount(falseNode);
   cg->stopUsingRegister(condReg);

   return trgReg;
   }

// Also handles dselect
TR::Register *OMR::Power::TreeEvaluator::fselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *condNode = node->getFirstChild();
   TR::Node *trueNode = node->getSecondChild();
   TR::Node *falseNode = node->getThirdChild();
   bool selectReverse = checkSelectReverse(cg, node, trueNode, falseNode);

   TR::Register *trgReg = cg->gprClobberEvaluate(trueNode);
   TR::Register *falseReg = cg->evaluate(falseNode);

   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   auto cond = evaluateToConditionRegister(condReg, node, condNode, cg);
   if (selectReverse)
      cond = reverseCondition(cond);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();

   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
   endLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg->trMemory());
   deps->addPostCondition(condReg, TR::RealRegister::NoReg);
   deps->addPostCondition(trgReg, TR::RealRegister::NoReg);
   deps->addPostCondition(falseReg, TR::RealRegister::NoReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);
   generateConditionalBranchInstruction(cg, compareConditionToBranch(cond), node, endLabel, condReg);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, trgReg, falseReg);
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);

   node->setRegister(trgReg);
   cg->decReferenceCount(condNode);
   cg->decReferenceCount(trueNode);
   cg->decReferenceCount(falseNode);
   cg->stopUsingRegister(condReg);

   return trgReg;
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
        !comp->getOption(TR_DisableInlineIfInstanceOf) &&
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
       if (isHint)
          generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
                generateRegisterDependencyConditions(cg, thirdChild, 0));
       else
          generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
                generateRegisterDependencyConditions(cg, thirdChild, 0));

       cg->decReferenceCount(thirdChild);
       }
    else
       {
       if (isHint)
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
         if (isHint)
            generateDepConditionalBranchInstruction(cg, branchOp, likeliness, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         else
            generateDepConditionalBranchInstruction(cg, branchOp, node, dstLabel, condReg,
                  generateRegisterDependencyConditions(cg, thirdChild, 0));
         cg->decReferenceCount(thirdChild);
         }
      else
         {
         if (isHint)
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

/**
 * \brief
 *    Generates an internal control flow sequence using a branch to set a target register to either
 *    0 or 1 depending on the result of a comparison.
 *
 * \param trgReg
 *    The register into which the result of the comparison should be written.
 *
 * \param node
 *    The node for which the comparison is being performed.
 *
 * \param firstChild
 *    The node corresponding to the left-hand side operand of the comparison.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison.
 *
 * \param cg
 *    The code generator.
 */
void generateCompareBranchSequence(
      TR::Register *trgReg,
      TR::Node *node,
      TR::Node *firstChild,
      TR::Node *secondChild,
      const CompareInfo& compareInfo,
      TR::CodeGenerator *cg)
   {
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   CompareCondition cond = evaluateCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();

   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
   endLabel->setEndInternalControlFlow();

   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);
   generateConditionalBranchInstruction(cg, compareConditionToBranch(reverseCondition(cond)), node, endLabel, condReg);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());
   deps->addPostCondition(condReg, TR::RealRegister::NoReg);
   deps->addPostCondition(trgReg, TR::RealRegister::NoReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);

   cg->stopUsingRegister(condReg);
   }

/**
 * \brief
 *    Generates a sequence using the POWER10 setbc family of instructions to set a target register
 *    to either 0 or 1 depending on the result of a comparison without branching.
 *
 * \param trgReg
 *    The register into which the result of the comparison should be written.
 *
 * \param node
 *    The node for which the comparison is being performed.
 *
 * \param firstChild
 *    The node corresponding to the left-hand side operand of the comparison.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison.
 *
 * \param cg
 *    The code generator.
 */
void generateCompareSetBoolean(
      TR::Register *trgReg,
      TR::Node *node,
      TR::Node *firstChild,
      TR::Node *secondChild,
      const CompareInfo& compareInfo,
      TR::CodeGenerator *cg)
   {
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   CRCompareCondition cond = compareConditionInCR(evaluateCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg));

   generateTrg1Src1ImmInstruction(cg, cond.isReversed ? TR::InstOpCode::setbcr : TR::InstOpCode::setbc, node, trgReg, condReg, cond.crcc);

   cg->stopUsingRegister(condReg);
   }

TR::Register *intEqualityEvaluator(TR::Node *node, bool flipResult, TR::DataTypes type, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *trgReg = cg->allocateRegister();

   CompareInfo compareInfo(flipResult ? CompareCondition::ne : CompareCondition::eq, type, false, false);

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      {
      generateCompareSetBoolean(trgReg, node, firstChild, secondChild, compareInfo, cg);
      }
   else if (cg->comp()->target().is32Bit() && type == TR::Int64)
      {
      generateCompareBranchSequence(trgReg, node, firstChild, secondChild, compareInfo, cg);
      }
   else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
      {
      TR::Register *condReg = cg->allocateRegister(TR_CCR);

      evaluateThreeWayIntCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg);

      // By taking the lower bit of setb, we can compute the result of (x < y || x > y), which is
      // the same as x != y for integers.
      generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 0, 1);
      if (!flipResult)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 1);

      cg->stopUsingRegister(condReg);
      }
   else
      {
      TR::Register *src1Reg = evaluateAndSignExtend(firstChild, false, cg);
      TR::Register *diffReg;

      if (secondChild->getOpCode().isLoadConst() && !secondChild->getRegister())
         {
         int64_t src2Value = secondChild->get64bitIntegralValue();

         if (src2Value != 0)
            {
            if (type == TR::Int64 || (type == TR::Address && cg->comp()->target().is64Bit()))
               addConstantToLong(node, src1Reg, -src2Value, trgReg, cg);
            else
               addConstantToInteger(node, trgReg, src1Reg, -src2Value, cg);

            diffReg = trgReg;
            }
         else
            {
            diffReg = src1Reg;
            }
         }
      else
         {
         TR::Register *src2Reg = evaluateAndSignExtend(secondChild, false, cg);

         generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, trgReg, src2Reg, src1Reg);
         diffReg = trgReg;

         stopUsingExtendedRegister(src2Reg, secondChild, cg);
         }

      if (type == TR::Int64 || (type == TR::Address && cg->comp()->target().is64Bit()))
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzd, node, trgReg, diffReg);
         generateShiftRightLogicalImmediate(cg, node, trgReg, trgReg, 6);
         }
      else
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, trgReg, diffReg);
         generateShiftRightLogicalImmediate(cg, node, trgReg, trgReg, 5);
         }

      if (flipResult)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 1);

      stopUsingExtendedRegister(src1Reg, firstChild, cg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

/**
 * \brief
 *    Generates a branchless sequence for comparing the specified register to the integral constant
 *    0, placing the result in the sign bit of the returned register.
 *
 * \param node
 *    The node for which the comparison is being performed.
 *
 * \param cond
 *    The condition code corresponding to the comparison that should be performed.
 *
 * \param srcReg
 *    The register containing the value which should be compared to 0. This register is not
 *    clobbered when performing the computation.
 *
 * \param tempReg
 *    A register whose value can be clobbered to hold intermediate values used to perform the
 *    comparison.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    The register containing the result of the comparison in its sign bit. This register may be
 *    either srcReg or tempReg depending on what comparison was performed.
 */
TR::Register *intOrderZeroToSignBit(TR::Node *node, CompareCondition cond, TR::Register *srcReg, TR::Register *tempReg, TR::CodeGenerator *cg)
   {
   switch (cond)
      {
      case CompareCondition::lt:
         {
         return srcReg;
         }

      case CompareCondition::ge:
         {
         generateTrg1Src2Instruction(cg, TR::InstOpCode::nor, node, tempReg, srcReg, srcReg);
         return tempReg;
         }

      case CompareCondition::gt:
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, tempReg, srcReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, tempReg, tempReg, srcReg);
         return tempReg;
         }

      case CompareCondition::le:
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, tempReg, srcReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::orc, node, tempReg, srcReg, tempReg);
         return tempReg;
         }

      default:
         TR_ASSERT_FATAL_WITH_NODE(node, false, "Invalid compare condition %d for intOrderZeroToSignBit", static_cast<int>(cond));
      }
   }

/**
 * \brief
 *    Determines whether a comparison is a signed comparison that does not use register pairs
 *    against a value for which the value of the sign bit is known.
 *
 * \param secondChild
 *    The node corresponding to the right-hand side operand of the comparison.
 *
 * \param compareInfo
 *    The CompareInfo struct containing information about the comparison.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    true if the comparison is a signed comparison, does not use register pairs, and is against
 *    a value with a known sign bit; false otherwise.
 */
bool isSimpleSignedCompareToKnownSign(TR::Node *secondChild, const CompareInfo& compareInfo, TR::CodeGenerator *cg)
   {
   if (compareInfo.isUnsigned)
      return false;

   if (compareInfo.type == TR::Int64 && !cg->comp()->target().is64Bit())
      return false;

   return secondChild->isNonNegative() || secondChild->isNegative() || secondChild->getOpCode().isLoadConst();
   }

/**
 * \brief
 *    Gets the length of an integral type in bits.
 *
 * \param dt
 *    The data type to get the length of.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    The length of the provided type in bits.
 */
int32_t getTypeBitLength(TR::DataTypes dt, TR::CodeGenerator *cg)
   {
   switch (dt)
      {
      case TR::Int8:
         return 8;
      case TR::Int16:
         return 16;
      case TR::Int32:
         return 32;
      case TR::Int64:
         return 64;
      case TR::Address:
         return cg->comp()->target().is64Bit() ? 64 : 32;
      default:
         TR_ASSERT_FATAL(false, "Invalid data type %s for getTypeBitLength", TR::DataType::getName(dt));
      }
   }

/**
 * \brief
 *    Determines whether an integral type is smaller than the size of a GPR on the current
 *    machine. This is meant to be used to enable optimizations that make use of extra bits
 *    available in the GPR that cannot actually be set by the value.
 *
 *    Note that this method considers the size of a GPR on the target machine to be equal to the
 *    size of an address. While this is not strictly true in hardware since it's possible to make
 *    use of the upper 32 bits of a register when running in 32-bit mode on a 64-bit machine, this
 *    function assumes that the caller will not do this.
 *
 * \param dt
 *    The data type to check.
 *
 * \param cg
 *    The code generator.
 *
 * \return
 *    true if the provided type is smaller than the size of a GPR; false otherwise.
 */
bool isTypeSubRegister(TR::DataTypes dt, TR::CodeGenerator *cg)
   {
   switch (dt)
      {
      case TR::Int8:
      case TR::Int16:
         return true;
      case TR::Int32:
         return cg->comp()->target().is64Bit();
      case TR::Int64:
      case TR::Address:
         return false;
      default:
         TR_ASSERT_FATAL(false, "Invalid data type %s for isTypeSubRegister", TR::DataType::getName(dt));
      }
   }

TR::Register *intOrderEvaluator(TR::Node *node, const CompareInfo& compareInfo, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *trgReg = cg->allocateRegister();

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      {
      generateCompareSetBoolean(trgReg, node, firstChild, secondChild, compareInfo, cg);
      }
   else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
      {
      CompareCondition cond = compareInfo.cond;
      TR::Register *condReg = cg->allocateRegister(TR_CCR);

      if (evaluateThreeWayIntCompareToConditionRegister(condReg, node, firstChild, secondChild, compareInfo, cg))
         cond = flipConditionOrder(compareInfo.cond);

      CRCompareCondition crCond = compareConditionInCR(cond);

      if (crCond.crcc == TR::RealRegister::CRCC_LT)
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 1, 1);
         if (crCond.isReversed)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 1);
         }
      else
         {
         TR_ASSERT_FATAL_WITH_NODE(node, crCond.crcc == TR::RealRegister::CRCC_GT, "Invalid CRCC %d in intOrderEvaluator", crCond.crcc);

         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crxor, node, condReg, condReg, condReg,
            (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB));
         generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
         if (crCond.isReversed)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 1);
         }

      cg->stopUsingRegister(condReg);
      }
   else if (isSimpleSignedCompareToKnownSign(secondChild, compareInfo, cg))
      {
      int32_t bitLength = getTypeBitLength(compareInfo.type, cg);
      TR::Register *signReg;

      TR::Register *src1Reg = cg->evaluate(firstChild);

      if (secondChild->getOpCode().isLoadConst() && secondChild->get64bitIntegralValue() == 0)
         {
         signReg = intOrderZeroToSignBit(node, compareInfo.cond, src1Reg, trgReg, cg);
         }
      else
         {
         // Since we know the sign of the second operand of the comparison, we can use the results
         // of the simple comparisons x < 0, (x - y) < 0, and (x - y - 1) < 0 to compute the result
         // of the full comparison. Since we can check whether a number is < 0 by simply taking its
         // sign bit, this allows us to compute the result of the computation without branches.
         //
         // e.g. to compute x < y when y < 0, we can simply compute (x < 0 && (x - y) < 0)
         //      to compute x > y when y < 0, we can simply compute (x >= 0 || (x - y - 1) >= 0)
         bool flipResult = (compareInfo.cond == CompareCondition::gt || compareInfo.cond == CompareCondition::ge);
         bool diffSubOne = (compareInfo.cond == CompareCondition::le || compareInfo.cond == CompareCondition::gt);

         // Currently, the isNonNegative flag on a node is set by global VP, even for const nodes.
         // However, Tril tests do not run GVP which results in this flag never being set. In order
         // to ensure that this code path gets used during testing, we must have special handling
         // for const nodes.
         bool isNonNegative;

         bool useAddImmediate = false;
         int64_t addImmediate;

         if (secondChild->getOpCode().isLoadConst())
            {
            int64_t src2Value = secondChild->get64bitIntegralValue();

            isNonNegative = src2Value >= 0;
            addImmediate = diffSubOne ? -src2Value - 1 : -src2Value;
            useAddImmediate = is16BitSignedImmediate(addImmediate) || (!secondChild->getRegister() && secondChild->getReferenceCount() == 1);
            }
         else
            {
            isNonNegative = secondChild->isNonNegative();
            }

         if (useAddImmediate)
            {
            if (bitLength > 32)
               addConstantToLong(node, src1Reg, addImmediate, trgReg, cg);
            else
               addConstantToInteger(node, trgReg, src1Reg, addImmediate, cg);
            }
         else
            {
            TR::Register *src2Reg = cg->evaluate(secondChild);

            generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, trgReg, src2Reg, src1Reg);
            if (diffSubOne)
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, trgReg, trgReg, -1);
            }

         if (isNonNegative)
            generateTrg1Src2Instruction(cg, flipResult ? TR::InstOpCode::nor : TR::InstOpCode::OR, node, trgReg, trgReg, src1Reg);
         else
            generateTrg1Src2Instruction(cg, flipResult ? TR::InstOpCode::nand : TR::InstOpCode::AND, node, trgReg, trgReg, src1Reg);

         signReg = trgReg;
         }

      generateTrg1Src1Imm2Instruction(
         cg,
         bitLength > 32 ? TR::InstOpCode::rldicl : TR::InstOpCode::rlwinm,
         node,
         trgReg,
         signReg,
         bitLength > 32 ? (65 - bitLength) : (33 - bitLength),
         1ull
      );
      }
   else if (isTypeSubRegister(compareInfo.type, cg))
      {
      // When the size of the type of values being compared is less than the size of a register, we
      // can simply sign- or zero-extend the values to fill a register, subtract them from each
      // other, then check the sign of the result.
      TR::Register *src1Reg = evaluateAndExtend(firstChild, compareInfo.isUnsigned, true, cg);

      bool flipOrder = (compareInfo.cond == CompareCondition::gt || compareInfo.cond == CompareCondition::le);
      bool flipResult = (compareInfo.cond == CompareCondition::le || compareInfo.cond == CompareCondition::ge);
      bool isConst = secondChild->getOpCode().isLoadConst();

      if (isConst && compareInfo.isUnsigned && secondChild->get64bitIntegralValue() < 0)
         isConst = false;

      if (flipOrder && isConst && is16BitSignedImmediate(secondChild->get64bitIntegralValue()))
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, trgReg, src1Reg, secondChild->get64bitIntegralValue());
         }
      else if (!flipOrder && isConst && is16BitSignedImmediate(-secondChild->get64bitIntegralValue()))
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, trgReg, src1Reg, -secondChild->get64bitIntegralValue());
         }
      else
         {
         TR::Register *src2Reg = evaluateAndExtend(secondChild, compareInfo.isUnsigned, true, cg);

         if (flipOrder)
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, trgReg, src1Reg, src2Reg);
         else
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, trgReg, src2Reg, src1Reg);

         stopUsingExtendedRegister(src2Reg, secondChild, cg);
         }

      if (cg->comp()->target().is64Bit())
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, trgReg, trgReg, 1, 0x1ull);
      else
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 1, 0x1ull);

      if (flipResult)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 0x1ull);

      stopUsingExtendedRegister(src1Reg, firstChild, cg);
      }
   else
      {
      generateCompareBranchSequence(trgReg, node, node->getFirstChild(), node->getSecondChild(), compareInfo, cg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::bcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, false, TR::Int8, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, true, TR::Int8, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int8, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int8, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int8, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int8, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int8, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int8, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int8, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::bucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int8, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::scmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, false, TR::Int16, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::scmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, true, TR::Int16, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::scmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int16, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::scmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int16, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::scmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int16, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::scmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int16, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::sucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int16, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::sucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int16, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::sucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int16, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::sucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int16, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, false, TR::Int32, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, true, TR::Int32, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int32, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int32, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int32, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int32, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int32, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int32, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int32, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int32, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, false, TR::Int64, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, true, TR::Int64, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int64, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int64, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int64, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int64, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Int64, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Int64, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Int64, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Int64, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, false, TR::Address, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intEqualityEvaluator(node, true, TR::Address, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::lt, TR::Address, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::ge, TR::Address, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::gt, TR::Address, true, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::acmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intOrderEvaluator(node, CompareInfo(CompareCondition::le, TR::Address, true, false), cg);
   }

TR::Register *floatCompareEvaluator(TR::Node *node, const CompareInfo& compareInfo, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *trgReg = cg->allocateRegister();

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P10))
      {
      generateCompareSetBoolean(trgReg, node, firstChild, secondChild, compareInfo, cg);
      }
   else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
      {
      CRCompareCondition crCond = compareConditionInCR(compareInfo.cond);
      TR::Register *condReg = cg->allocateRegister(TR_CCR);
      TR::Register *lhsReg = cg->evaluate(firstChild);
      TR::Register *rhsReg = cg->evaluate(secondChild);

      generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, lhsReg, rhsReg);

      switch (crCond.crcc)
         {
         case TR::RealRegister::CRCC_EQ:
            // We can compute (x < y || x > y) by taking the negation of the lowest bit of the
            // result of setb. However, for *cmpeq and *cmpneu, we actually need to compute
            // (x < y || x > y || is_unordered(x, y)), so we set the LT bit if the FU bit is
            // set to get setb to return -1.
            if (crCond.isReversed == compareInfo.isUnorderedTrue)
               generateTrg1Src2ImmInstruction(
                  cg,
                  TR::InstOpCode::crxor,
                  node,
                  condReg,
                  condReg,
                  condReg,
                  (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_FU << TR::RealRegister::pos_RB)
               );

            generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 0, 1);
            if (!crCond.isReversed)
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 1);
            break;

         case TR::RealRegister::CRCC_LT:
            // To check for x < y, we can take the sign bit of the result of setb. We do need to
            // take unordered operands into account, though.
            if (crCond.isReversed != compareInfo.isUnorderedTrue)
               generateTrg1Src2ImmInstruction(
                  cg,
                  TR::InstOpCode::crxor,
                  node,
                  condReg,
                  condReg,
                  condReg,
                  (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_FU << TR::RealRegister::pos_RB)
               );

            generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 1, 1);
            if (crCond.isReversed)
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 1);
            break;

         case TR::RealRegister::CRCC_GT:
            // To check for x > y, we can simply clear the LT bit of the CR and then take the
            // result of setb directly. We do need to take unordered operands into account, though.
            if (crCond.isReversed != compareInfo.isUnorderedTrue)
               generateTrg1Src2ImmInstruction(
                  cg,
                  TR::InstOpCode::crxor,
                  node,
                  condReg,
                  condReg,
                  condReg,
                  (TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_GT << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_FU << TR::RealRegister::pos_RB)
               );

            generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::crxor, node, condReg, condReg, condReg,
               (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RT) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB));
            generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
            if (crCond.isReversed)
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, node, trgReg, trgReg, 1);
            break;

         default:
            TR_ASSERT_FATAL_WITH_NODE(node, false, "Invalid CRCC %d in floatCompareEvaluator", crCond.crcc);
         }

      cg->stopUsingRegister(condReg);
      }
   else
      {
      generateCompareBranchSequence(trgReg, node, firstChild, secondChild, compareInfo, cg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::eq, TR::Float, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::eq, TR::Float, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ne, TR::Float, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ne, TR::Float, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::lt, TR::Float, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::lt, TR::Float, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ge, TR::Float, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ge, TR::Float, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::gt, TR::Float, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::gt, TR::Float, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::le, TR::Float, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::le, TR::Float, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::eq, TR::Double, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::eq, TR::Double, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ne, TR::Double, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ne, TR::Double, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::lt, TR::Double, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::lt, TR::Double, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ge, TR::Double, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::ge, TR::Double, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::gt, TR::Double, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::gt, TR::Double, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::le, TR::Double, false, false), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatCompareEvaluator(node, CompareInfo(CompareCondition::le, TR::Double, false, true), cg);
   }

TR::Register *OMR::Power::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *src1Reg = cg->evaluate(firstChild);

   if (cg->comp()->target().is64Bit())
      {
      if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
         {
         TR::Register *condReg = cg->allocateRegister(TR_CCR);

         if (secondChild->getOpCode().isLoadConst() && is16BitSignedImmediate(secondChild->get64bitIntegralValue()))
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi8, node, condReg, src1Reg, secondChild->get64bitIntegralValue());
            }
         else
            {
            TR::Register *src2Reg = cg->evaluate(secondChild);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, condReg, src1Reg, src2Reg);
            }

         generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);

         cg->stopUsingRegister(condReg);
         }
      else if (secondChild->getOpCode().isLoadConst() && secondChild->get64bitIntegralValue() == 0)
         {
         TR::Register *temp1Reg = cg->allocateRegister();

         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, trgReg, src1Reg, 63);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, temp1Reg, src1Reg, -1);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::adde, node, trgReg, trgReg, trgReg);

         cg->stopUsingRegister(temp1Reg);
         }
      else
         {
         TR::Register *src2Reg = cg->evaluate(secondChild);
         TR::Register *temp1Reg = cg->allocateRegister();
         TR::Register *temp2Reg = cg->allocateRegister();
         TR::Register *temp3Reg = cg->allocateRegister();

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
   else
      {
      if (secondChild->getOpCode().isLoadConst() && secondChild->get64bitIntegralValue() == 0)
         {
         TR::Register *temp1Reg = cg->allocateRegister();
         TR::Register *temp2Reg = cg->allocateRegister();
         TR::Register *temp3Reg = cg->allocateRegister();

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
         TR::Register *condReg = cg->allocateRegister(TR_CCR);

         TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
         startLabel->setStartInternalControlFlow();

         TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
         endLabel->setEndInternalControlFlow();

         generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, condReg, src1Reg->getHighOrder(), src2Reg->getHighOrder());
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, endLabel, condReg);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, -1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, endLabel, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, src1Reg->getLowOrder(), src2Reg->getLowOrder());
         generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, endLabel, condReg);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, endLabel, condReg);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);

         TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg->trMemory());

         deps->addPostCondition(src1Reg->getLowOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(src1Reg->getHighOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(src2Reg->getLowOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(src2Reg->getHighOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(condReg, TR::RealRegister::NoReg);
         deps->addPostCondition(trgReg, TR::RealRegister::NoReg);

         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);

         cg->stopUsingRegister(condReg);
         }
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

static TR::Register *floatThreeWayCompareEvaluator(TR::Node *node, bool unorderedIsLess, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);

   generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, src1Reg, src2Reg);

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P9))
      {
      TR::RealRegister::CRCC crcc = unorderedIsLess ? TR::RealRegister::CRCC_LT : TR::RealRegister::CRCC_GT;
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::cror, node, condReg, condReg, condReg,
         (crcc << TR::RealRegister::pos_RT) | (crcc << TR::RealRegister::pos_RA) | (TR::RealRegister::CRCC_FU << TR::RealRegister::pos_RB));
      generateTrg1Src1Instruction(cg, TR::InstOpCode::setb, node, trgReg, condReg);
      }
   else
      {
      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();

      TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
      endLabel->setEndInternalControlFlow();

      generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);
      if (unorderedIsLess)
         {
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, endLabel, condReg);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, endLabel, condReg);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, -1);
         }
      else
         {
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, -1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, endLabel, condReg);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, endLabel, condReg);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
         }

      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());

      deps->addPostCondition(trgReg, TR::RealRegister::NoReg);
      deps->addPostCondition(condReg, TR::RealRegister::NoReg);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);
      }

   node->setRegister(trgReg);
   cg->stopUsingRegister(condReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatThreeWayCompareEvaluator(node, true, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return floatThreeWayCompareEvaluator(node, false, cg);
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
            TR_ASSERT_FATAL_WITH_NODE(node, 0x00008000 != cg->hiValue(offset), "TOC offset (0x%x) is unexpectedly high. Can not encode upper 16 bits into an addis instruction.", offset);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, addrRegister, cg->getTOCBaseRegister(), cg->hiValue(offset));
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, LO_VALUE(offset), 8));
            }
         else
            {
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, TR::MemoryReference::createWithDisplacement(cg, cg->getTOCBaseRegister(), offset, 8));
            }

         if(isInt64)
            generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, 0, 8));
         else
            generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, 0, 4));
         }
      else
         {
         if (cg->needRelocationsForLookupEvaluationData())
            {
            loadAddressConstant(cg, true, node, address, addrRegister);
            generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, 0, 4));
            }
         else
            {
            loadAddressConstant(cg, false, node, cg->hiValue(address)<<16, addrRegister);
            nextAddress = LO_VALUE((int32_t)address);
            if (isInt64)
               {
               if (nextAddress >= 32760)
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ldu, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 8));
                  nextAddress = 8;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 8));
                  nextAddress += 8;
                  }
               }
            else
               {
               if (nextAddress >= 32764)
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
                  nextAddress = 4;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
                  nextAddress += 4;
                  }
               } // is64BitInt ?
            } // isAOT?
         }
      }  // ...if 64BitTarget
   else  // if 32bit mode
      {
      rel1 = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, addrRegister, (int16_t)cg->hiValue(address));

      if (isInt64)
         {
         nextAddress = LO_VALUE(address);
         if (nextAddress == 32760)
            {
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
            nextAddress += 4;
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister->getLowOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
            nextAddress = 4;
            }
         else
            {
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
            nextAddress += 4;
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getLowOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
            nextAddress += 4;
            }
         }
      else
         {
         if (cg->comp()->compileRelocatableCode())
            {
            rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, LO_VALUE(address), 4));
            }
         else
            {
            nextAddress = LO_VALUE(address);
            if (nextAddress >= 32764)
               {
               rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
               nextAddress = 4;
               }
            else
               {
               rel2 = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
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
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ldu, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 8));
                  nextAddress = 8;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 8));
                  nextAddress += 8;
                  }
               }
            else
               {
               if (nextAddress == 32760)
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
                  nextAddress += 4;
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister->getLowOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
                  nextAddress = 4;
                  }
               else
                  {
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getHighOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
                  nextAddress += 4;
                  generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister->getLowOrder(), TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
                  nextAddress += 4;
                  }
               } // 64BitTarget ?
            } // isInt64
         else
            {
            if (nextAddress >= 32764)
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::lwzu, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
               nextAddress = 4;
               }
            else
               {
               generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, dataRegister, TR::MemoryReference::createWithDisplacement(cg, addrRegister, nextAddress, 4));
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
            TR_ASSERT_FATAL_WITH_NODE(node, 0x00008000 != cg->hiValue(offset), "TOC offset (0x%x) is unexpectedly high. Can not encode upper 16 bits into an addis instruction.", offset);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, pivotRegister, cg->getTOCBaseRegister(), cg->hiValue(offset));
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, TR::MemoryReference::createWithDisplacement(cg, pivotRegister, LO_VALUE(offset), 8));
            }
         else
            {
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, addrRegister, TR::MemoryReference::createWithDisplacement(cg, cg->getTOCBaseRegister(), offset, 8));
            }
         }
      else
         {
         loadAddressConstant(cg, cg->needRelocationsForLookupEvaluationData(), node, address, addrRegister);
         }
      }
   else
      {
      rel1 = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, pivotRegister, (int16_t)cg->hiValue(address));
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
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwzx, node, dataRegister->getHighOrder(), TR::MemoryReference::createWithIndexReg(cg, addrRegister, pivotRegister, 4));
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, pivotRegister, pivotRegister, 4);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwzx, node, dataRegister->getLowOrder(), TR::MemoryReference::createWithIndexReg(cg, addrRegister, pivotRegister, 4));
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, pivotRegister, pivotRegister, -4);
         }
      else
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldx, node, dataRegister, TR::MemoryReference::createWithIndexReg(cg, addrRegister, pivotRegister, 8));
         }
      }
   else
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwzx, node, dataRegister, TR::MemoryReference::createWithIndexReg(cg, addrRegister, pivotRegister, 4));
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
         if (tReg == NULL)
            {
            tReg = cg->allocateRegister();
            TR::addDependency(conditions, tReg, TR::RealRegister::NoReg, TR_GPR, cg);
            }

         cg->fixedLoadLabelAddressIntoReg(node, reg1, table, NULL, tReg);
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
      gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beql, PPCOpProp_BranchUnlikely, node, snippetLabel, condReg, conditions);

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
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   CompareCondition cond = evaluateToConditionRegister(condReg, node, node->getFirstChild(), cg);

   generateConditionalBranchInstruction(cg, compareConditionToBranch(reverseCondition(cond)), PPCOpProp_BranchUnlikely, node, slowPathLabel, condReg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, restartLabel);

   cg->stopUsingRegister(condReg);
   cg->decReferenceCount(node->getFirstChild());
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
         case TR_BreakpointGuard:
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
   TR_ASSERT(node->getNumChildren() == 2, "max/min node should only support 2 children");

   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::DataType data_type = firstChild->getDataType();
   TR::DataType type = firstChild->getType();
   TR::InstOpCode::Mnemonic move_op = type.isIntegral() ? TR::InstOpCode::mr : TR::InstOpCode::fmr;
   TR::InstOpCode::Mnemonic cmp_op;
   bool two_reg = (cg->comp()->target().is32Bit() && type.isInt64());
   bool check_nan = type.isFloatingPoint();

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

   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg;

   if (cg->canClobberNodesRegister(firstChild))
      {
      trgReg = src1Reg; // use first child as a target
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
         generateTrg1Src1Instruction(cg, move_op, node, trgReg->getLowOrder(), src1Reg->getLowOrder());
         generateTrg1Src1Instruction(cg, move_op, node, trgReg->getHighOrder(), src1Reg->getHighOrder());
         }
      else
         {
         generateTrg1Src1Instruction(cg, move_op, node, trgReg, src1Reg);
         }
      }

   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::LabelSymbol *start_label = generateLabelSymbol(cg);
   TR::LabelSymbol *end_label = generateLabelSymbol(cg);
   start_label->setStartInternalControlFlow();
   end_label->setEndInternalControlFlow();
   TR::LabelSymbol *nan_label;
   if (check_nan)
      nan_label = generateLabelSymbol(cg);
   TR::RegisterDependencyConditions *dep;

   generateLabelInstruction(cg, TR::InstOpCode::label, node, start_label);
   if (two_reg)
      {
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
         generateControlFlowInstruction(cg, TR::InstOpCode::iflong, node, NULL /*deps */);
      cfop->addTargetRegister(condReg);
      cfop->addSourceRegister(trgReg->getHighOrder());
      cfop->addSourceRegister(trgReg->getLowOrder());
      cfop->addSourceRegister(src2Reg->getHighOrder());
      cfop->addSourceRegister(src2Reg->getLowOrder());
      cfop->setLabelSymbol(end_label);
      cfop->setOpCode2Value(max ? TR::InstOpCode::bge : TR::InstOpCode::ble);
      generateTrg1Src1Instruction(cg, move_op, node, trgReg->getLowOrder(), src2Reg->getLowOrder());
      generateTrg1Src1Instruction(cg, move_op, node, trgReg->getHighOrder(), src2Reg->getHighOrder());

      dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg->trMemory());
      dep->addPostCondition(condReg, TR::RealRegister::NoReg);
      dep->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::NoReg);
      dep->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::NoReg);
      dep->addPostCondition(src2Reg->getLowOrder(), TR::RealRegister::NoReg);
      dep->addPostCondition(src2Reg->getHighOrder(), TR::RealRegister::NoReg);
      }
   else
      {
      generateTrg1Src2Instruction(cg, cmp_op, node, condReg, trgReg, src2Reg);
      if (check_nan)
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bnun, node, nan_label, condReg);
         // Move the NaN which is in one of trgReg or src2Reg to trgReg by fadd
         generateTrg1Src2Instruction(cg, TR::InstOpCode::fadd, node, trgReg, trgReg, src2Reg);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, end_label);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, nan_label);
         }
      generateConditionalBranchInstruction(cg, max ? TR::InstOpCode::bge : TR::InstOpCode::ble, node, end_label, condReg);
      generateTrg1Src1Instruction(cg, move_op, node, trgReg, src2Reg);

      dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg->trMemory());
      dep->addPostCondition(condReg, TR::RealRegister::NoReg);
      dep->addPostCondition(trgReg, TR::RealRegister::NoReg);
      dep->addPostCondition(src2Reg, TR::RealRegister::NoReg);
      }
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, end_label, dep);
   cg->stopUsingRegister(condReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
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
