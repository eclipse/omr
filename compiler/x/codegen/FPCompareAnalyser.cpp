/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "x/codegen/FPCompareAnalyser.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

static bool isUnevaluatedZero(TR::Node *child)
   {
   if (child->getRegister() == NULL)
      {
      switch (child->getOpCodeValue())
         {
         case TR::iconst:
         case TR::sconst:
         case TR::bconst:
            return (child->getInt() == 0);
           case TR::lconst:
            return (child->getLongInt() == 0);
           case TR::fconst:
            return (child->getFloatBits() == 0 || child->getFloatBits() == 0x80000000); // hex for ieee float -0.0
           case TR::dconst:
            return (child->getDoubleBits() == 0 || child->getDoubleBits() == IEEE_DOUBLE_NEGATIVE_0_0);
           case TR::i2f:
         case TR::s2f:
         case TR::b2f:
         case TR::l2f:
         case TR::d2f:
         case TR::i2d:
         case TR::s2d:
         case TR::b2d:
         case TR::l2d:
         case TR::f2d:
            return isUnevaluatedZero(child->getFirstChild());
         default:
            return false;
         }
      }
   return false;
   }

TR::Register *TR_X86FPCompareAnalyser::fpCompareAnalyser(TR::Node       *root,
                                                         TR_X86OpCodes cmpRegRegOpCode,
                                                         TR_X86OpCodes cmpRegMemOpCode,
                                                         TR_X86OpCodes cmpiRegRegOpCode,
                                                         bool           useFCOMIInstructions)
   {
   TR::Node      *firstChild,
                *secondChild;
   TR::ILOpCodes  cmpOp = root->getOpCodeValue();
   bool          reverseMemOp = false;
   bool          reverseCmpOp = false;
   TR::Compilation* comp = _cg->comp();
   TR_X86OpCodes cmpInstr = useFCOMIInstructions ? cmpiRegRegOpCode : cmpRegRegOpCode;

   // Some operators must have their operands swapped to improve the generated
   // code needed to evaluate the result of the comparison.
   //
   bool mustSwapOperands = (cmpOp == TR::iffcmple ||
                            cmpOp == TR::ifdcmple ||
                            cmpOp == TR::iffcmpgtu ||
                            cmpOp == TR::ifdcmpgtu ||
                            cmpOp == TR::fcmple ||
                            cmpOp == TR::dcmple ||
                            cmpOp == TR::fcmpgtu ||
                            cmpOp == TR::dcmpgtu ||
                            (useFCOMIInstructions &&
                             (cmpOp == TR::iffcmplt ||
                              cmpOp == TR::ifdcmplt ||
                              cmpOp == TR::iffcmpgeu ||
                              cmpOp == TR::ifdcmpgeu ||
                              cmpOp == TR::fcmplt ||
                              cmpOp == TR::dcmplt ||
                              cmpOp == TR::fcmpgeu ||
                              cmpOp == TR::dcmpgeu))) ? true : false;

   // Some operators should not have their operands swapped to improve the generated
   // code needed to evaluate the result of the comparison.
   //
   bool preventOperandSwapping = (cmpOp == TR::iffcmpltu ||
                                  cmpOp == TR::ifdcmpltu ||
                                  cmpOp == TR::iffcmpge ||
                                  cmpOp == TR::ifdcmpge ||
                                  cmpOp == TR::fcmpltu ||
                                  cmpOp == TR::dcmpltu ||
                                  cmpOp == TR::fcmpge ||
                                  cmpOp == TR::dcmpge ||
                                  (useFCOMIInstructions &&
                                   (cmpOp == TR::iffcmpgt ||
                                    cmpOp == TR::ifdcmpgt ||
                                    cmpOp == TR::iffcmpleu ||
                                    cmpOp == TR::ifdcmpleu ||
                                    cmpOp == TR::fcmpgt ||
                                    cmpOp == TR::dcmpgt ||
                                    cmpOp == TR::fcmpleu ||
                                    cmpOp == TR::dcmpleu))) ? true : false;

   // For correctness, don't swap operands of these operators.
   //
   if (cmpOp == TR::fcmpg || cmpOp == TR::fcmpl ||
       cmpOp == TR::dcmpg || cmpOp == TR::dcmpl)
      {
      preventOperandSwapping = true;
      }

   // Initial operand evaluation ordering.
   //
   if (preventOperandSwapping || (!mustSwapOperands && _cg->whichChildToEvaluate(root) == 0))
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      setReversedOperands(true);
      }

   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   setInputs(firstChild,
             firstRegister,
             secondChild,
             secondRegister,
             useFCOMIInstructions,

             // If either 'preventOperandSwapping' or 'mustSwapOperands' is set then the
             // initial operand ordering set above must be maintained.
             //
             preventOperandSwapping || mustSwapOperands);

   // Make sure any required operand ordering is respected.
   //
   if ((getCmpReg2Reg1() || getCmpReg2Mem1()) &&
       (mustSwapOperands || preventOperandSwapping))
      {
      reverseCmpOp = getCmpReg2Reg1() ? true : false;
      reverseMemOp = getCmpReg2Mem1() ? true : false;
      }

   // If we are not comparing with a memory operand, one of them evaluates
   // to a zero, and the zero is not already on the stack, then we can use
   // FTST to save a register.
   //
   // (With a memory operand, either the constant zero needs to be loaded
   // to use FCOM, or the memory operand needs to be loaded to use FTST,
   // so there is no gain in using FTST.)
   //
   // If the constant zero is in the target register, using FTST means the
   // comparison will be reversed. We cannot do this if the initial ordering
   // of the operands must be maintained.
   //
   // Finally, if FTST is used and this is the last use of the target, the
   // target register may need to be explicitly popped.
   //
   TR::Register *targetRegisterForFTST = NULL;
   TR::Node     *targetChildForFTST = NULL;

   if (getEvalChild1() && isUnevaluatedZero(firstChild))  // do we need getEvalChild1() here?
      {
      if ( ((getCmpReg1Reg2() || reverseCmpOp) && !(preventOperandSwapping || mustSwapOperands)) ||
            (getCmpReg2Reg1() && !reverseCmpOp))
         {
         if (getEvalChild2())
            {
            secondRegister = _cg->evaluate(secondChild);
            }
         targetRegisterForFTST = secondRegister;
         targetChildForFTST = secondChild;
         notReversedOperands();
         }
      }
   else if (getEvalChild2() && isUnevaluatedZero(secondChild))  // do we need getEvalChild2() here?
      {
      if ( (getCmpReg1Reg2() || reverseCmpOp) ||
           (getCmpReg2Reg1() && !reverseCmpOp && !(preventOperandSwapping || mustSwapOperands)) )
         {
         if (getEvalChild1())
            {
            firstRegister = _cg->evaluate(firstChild);
            }
         targetRegisterForFTST = firstRegister;
         targetChildForFTST = firstChild;
         }
      }

   if (!targetRegisterForFTST)
      {
      // If we have a choice, evaluate the target operand last.  By doing so, we
      // help out the register assigner because the target must be TOS.  This
      // avoids an unneccessary FXCH for the target.
      //
      if (getEvalChild1() && getEvalChild2())
         {
         if (getCmpReg1Reg2() || getCmpReg1Mem2())
            {
            secondRegister = _cg->evaluate(secondChild);
            firstRegister = _cg->evaluate(firstChild);
            }
         else
            {
            firstRegister = _cg->evaluate(firstChild);
            secondRegister = _cg->evaluate(secondChild);
            }
         }
      else
         {
         if (getEvalChild1())
            {
            firstRegister = _cg->evaluate(firstChild);
            }

         if (getEvalChild2())
            {
            secondRegister = _cg->evaluate(secondChild);
            }
         }
      }

   // Adjust the FP precision of feeding operands.
   //
   if (firstRegister &&
       (firstRegister->needsPrecisionAdjustment() ||
        comp->getOption(TR_StrictFPCompares) ||
        (firstRegister->mayNeedPrecisionAdjustment() && secondChild->getOpCode().isLoadConst()) ||
        (firstRegister->mayNeedPrecisionAdjustment() && !secondRegister)))
      {
      TR::TreeEvaluator::insertPrecisionAdjustment(firstRegister, root, _cg);
      }

   if (secondRegister &&
       (secondRegister->needsPrecisionAdjustment() ||
        comp->getOption(TR_StrictFPCompares) ||
        (secondRegister->mayNeedPrecisionAdjustment() && firstChild->getOpCode().isLoadConst()) ||
        (secondRegister->mayNeedPrecisionAdjustment() && !firstRegister)))
      {
      TR::TreeEvaluator::insertPrecisionAdjustment(secondRegister, root, _cg);
      }

   // Generate the compare instruction.
   //
   if (targetRegisterForFTST)
      {
      generateFPRegInstruction(FTSTReg, root, targetRegisterForFTST, _cg);
      }
   else if (!useFCOMIInstructions && (getCmpReg1Mem2() || reverseMemOp))
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, _cg);
      generateFPRegMemInstruction(cmpRegMemOpCode, root, firstRegister, tempMR, _cg);
      tempMR->decNodeReferenceCounts(_cg);
      }
   else if (!useFCOMIInstructions && getCmpReg2Mem1())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, _cg);
      generateFPRegMemInstruction(cmpRegMemOpCode, root, secondRegister, tempMR, _cg);
      notReversedOperands();
      tempMR->decNodeReferenceCounts(_cg);
      }
   else if (getCmpReg1Reg2() || reverseCmpOp)
      {
      generateFPCompareRegRegInstruction(cmpInstr, root, firstRegister, secondRegister, _cg);
      }
   else if (getCmpReg2Reg1())
      {
      generateFPCompareRegRegInstruction(cmpInstr, root, secondRegister, firstRegister, _cg);
      notReversedOperands();
      }

   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);

   // Evaluate the comparison.
   //
   if (getReversedOperands())
      {
      cmpOp = TR::ILOpCode(cmpOp).getOpCodeForSwapChildren();
      TR::Node::recreate(root, cmpOp);
      }

   if (useFCOMIInstructions && !targetRegisterForFTST)
      {
      return NULL;
      }

   // We must manually move the FP condition flags to the EFLAGS register if we don't
   // use the FCOMI instructions.
   //
   TR::Register *accRegister = _cg->allocateRegister();
   TR::RegisterDependencyConditions  *dependencies = generateRegisterDependencyConditions((uint8_t)1, 1, _cg);
   dependencies->addPreCondition(accRegister, TR::RealRegister::eax, _cg);
   dependencies->addPostCondition(accRegister, TR::RealRegister::eax, _cg);
   generateRegInstruction(STSWAcc, root, accRegister, dependencies, _cg);

   // Pop the FTST target register if it is not used any more.
   //
   if (targetRegisterForFTST &&
       targetChildForFTST && targetChildForFTST->getReferenceCount() == 0)
      {
      generateFPSTiST0RegRegInstruction(FSTRegReg, root, targetRegisterForFTST, targetRegisterForFTST, _cg);
      }

   return accRegister;
   }


void TR_X86FPCompareAnalyser::setInputs(TR::Node     *firstChild,
                                         TR::Register *firstRegister,
                                         TR::Node     *secondChild,
                                         TR::Register *secondRegister,
                                         bool         disallowMemoryFormInstructions,
                                         bool         disallowOperandSwapping)
   {


   if (firstRegister)
      {
      setReg1();
      }

   if (secondRegister)
      {
      setReg2();
      }

   if (!disallowMemoryFormInstructions &&
       firstChild->getOpCode().isMemoryReference() &&
       firstChild->getReferenceCount() == 1)
      {
      setMem1();
      }

   if (!disallowMemoryFormInstructions &&
       secondChild->getOpCode().isMemoryReference() &&
       secondChild->getReferenceCount() == 1)
      {
      setMem2();
      }

   if (firstChild->getReferenceCount() == 1)
      {
      setClob1();
      }

   if (secondChild->getReferenceCount() == 1)
      {
      setClob2();
      }

   if (disallowOperandSwapping)
      {
      setNoOperandSwapping();
      }
   }


const uint8_t TR_X86FPCompareAnalyser::_actionMap[NUM_FPCOMPARE_ACTION_SETS] =
   {
   // This table occupies NUM_FPCOMPARE_ACTION_SETS bytes of storage.

   // NS = No swapping of operands allowed
   // R1 = Child1 in register               R2 = Child2 in register
   // M1 = Child1 in memory                 M2 = Child2 in memory
   // C1 = Child1 is clobberable            C2 = Child2 is clobberable

   // Operand swapping allowed.
   //
   // NS R1 M1 C1 R2 M2 C2
   /* 0  0  0  0  0  0  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  0  0  0  0  0  1 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg2Reg1,
   /* 0  0  0  0  0  1  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  0  0  0  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 0  0  0  0  1  0  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  0  0  1  0  1 */    fpEvalChild1 | fpCmpReg2Reg1,
   /* 0  0  0  0  1  1  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  0  0  1  1  1 */    fpEvalChild1 | fpCmpReg2Reg1,
   /* 0  0  0  1  0  0  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  0  0  1  0  0  1 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  0  0  1  0  1  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  0  0  1  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 0  0  0  1  1  0  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  0  1  1  0  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  0  1  1  1  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  0  1  1  1  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  1  0  0  0  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  0  1  0  0  0  1 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg2Reg1,
   /* 0  0  1  0  0  1  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  0  1  0  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 0  0  1  0  1  0  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  1  0  1  0  1 */    fpEvalChild1 | fpCmpReg2Reg1,
   /* 0  0  1  0  1  1  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 0  0  1  0  1  1  1 */    fpEvalChild1 | fpCmpReg2Reg1,
   /* 0  0  1  1  0  0  0 */    fpEvalChild2 | fpCmpReg2Mem1,
   /* 0  0  1  1  0  0  1 */    fpEvalChild2 | fpCmpReg2Mem1,
   /* 0  0  1  1  0  1  0 */    fpEvalChild2 | fpCmpReg2Mem1,
   /* 0  0  1  1  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 0  0  1  1  1  0  0 */    fpCmpReg2Mem1,
   /* 0  0  1  1  1  0  1 */    fpCmpReg2Mem1,
   /* 0  0  1  1  1  1  0 */    fpCmpReg2Mem1,
   /* 0  0  1  1  1  1  1 */    fpCmpReg2Mem1,
   /* 0  1  0  0  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  0  0  0  0  1 */    fpEvalChild2 | fpCmpReg2Reg1,
   /* 0  1  0  0  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  0  0  0  1  1 */    fpCmpReg1Mem2,
   /* 0  1  0  0  1  0  0 */    fpCmpReg1Reg2,
   /* 0  1  0  0  1  0  1 */    fpCmpReg2Reg1,
   /* 0  1  0  0  1  1  0 */    fpCmpReg1Reg2,
   /* 0  1  0  0  1  1  1 */    fpCmpReg2Reg1,
   /* 0  1  0  1  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  0  1  0  0  1 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  0  1  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  0  1  0  1  1 */    fpCmpReg1Mem2,
   /* 0  1  0  1  1  0  0 */    fpCmpReg1Reg2,
   /* 0  1  0  1  1  0  1 */    fpCmpReg1Reg2,
   /* 0  1  0  1  1  1  0 */    fpCmpReg1Reg2,
   /* 0  1  0  1  1  1  1 */    fpCmpReg1Reg2,
   /* 0  1  1  0  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  1  0  0  0  1 */    fpEvalChild2 | fpCmpReg2Reg1,
   /* 0  1  1  0  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  1  0  0  1  1 */    fpCmpReg1Mem2,
   /* 0  1  1  0  1  0  0 */    fpCmpReg1Reg2,
   /* 0  1  1  0  1  0  1 */    fpCmpReg2Reg1,
   /* 0  1  1  0  1  1  0 */    fpCmpReg1Reg2,
   /* 0  1  1  0  1  1  1 */    fpCmpReg2Reg1,
   /* 0  1  1  1  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  1  1  0  0  1 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  1  1  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 0  1  1  1  0  1  1 */    fpCmpReg1Mem2,
   /* 0  1  1  1  1  0  0 */    fpCmpReg1Reg2,
   /* 0  1  1  1  1  0  1 */    fpCmpReg1Reg2,
   /* 0  1  1  1  1  1  0 */    fpCmpReg1Reg2,
   /* 0  1  1  1  1  1  1 */    fpCmpReg1Reg2,

   // Operand swapping disallowed.
   //
   // NS R1 M1 C1 R2 M2 C2
   /* 1  0  0  0  0  0  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  0  0  0  0  1 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  0  0  0  1  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  0  0  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 1  0  0  0  1  0  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  0  0  1  0  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  0  0  1  1  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  0  0  1  1  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  0  1  0  0  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  0  1  0  0  1 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  0  1  0  1  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  0  1  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 1  0  0  1  1  0  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  0  1  1  0  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  0  1  1  1  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  0  1  1  1  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  0  0  0  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  1  0  0  0  1 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  1  0  0  1  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  1  0  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 1  0  1  0  1  0  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  0  1  0  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  0  1  1  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  0  1  1  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  1  0  0  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  1  1  0  0  1 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  1  1  0  1  0 */    fpEvalChild1 | fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  0  1  1  0  1  1 */    fpEvalChild1 | fpCmpReg1Mem2,
   /* 1  0  1  1  1  0  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  1  1  0  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  1  1  1  0 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  0  1  1  1  1  1 */    fpEvalChild1 | fpCmpReg1Reg2,
   /* 1  1  0  0  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  0  0  0  0  1 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  0  0  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  0  0  0  1  1 */    fpCmpReg1Mem2,
   /* 1  1  0  0  1  0  0 */    fpCmpReg1Reg2,
   /* 1  1  0  0  1  0  1 */    fpCmpReg1Reg2,
   /* 1  1  0  0  1  1  0 */    fpCmpReg1Reg2,
   /* 1  1  0  0  1  1  1 */    fpCmpReg1Reg2,
   /* 1  1  0  1  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  0  1  0  0  1 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  0  1  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  0  1  0  1  1 */    fpCmpReg1Mem2,
   /* 1  1  0  1  1  0  0 */    fpCmpReg1Reg2,
   /* 1  1  0  1  1  0  1 */    fpCmpReg1Reg2,
   /* 1  1  0  1  1  1  0 */    fpCmpReg1Reg2,
   /* 1  1  0  1  1  1  1 */    fpCmpReg1Reg2,
   /* 1  1  1  0  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  1  0  0  0  1 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  1  0  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  1  0  0  1  1 */    fpCmpReg1Mem2,
   /* 1  1  1  0  1  0  0 */    fpCmpReg1Reg2,
   /* 1  1  1  0  1  0  1 */    fpCmpReg1Reg2,
   /* 1  1  1  0  1  1  0 */    fpCmpReg1Reg2,
   /* 1  1  1  0  1  1  1 */    fpCmpReg1Reg2,
   /* 1  1  1  1  0  0  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  1  1  0  0  1 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  1  1  0  1  0 */    fpEvalChild2 | fpCmpReg1Reg2,
   /* 1  1  1  1  0  1  1 */    fpCmpReg1Mem2,
   /* 1  1  1  1  1  0  0 */    fpCmpReg1Reg2,
   /* 1  1  1  1  1  0  1 */    fpCmpReg1Reg2,
   /* 1  1  1  1  1  1  0 */    fpCmpReg1Reg2,
   /* 1  1  1  1  1  1  1 */    fpCmpReg1Reg2
   };

TR::Register *TR_IA32XMMCompareAnalyser::xmmCompareAnalyser(TR::Node       *root,
                                                           TR_X86OpCodes cmpRegRegOpCode,
                                                           TR_X86OpCodes cmpRegMemOpCode)
   {
   TR::Node      *firstChild,
                *secondChild;
   TR::ILOpCodes  cmpOp = root->getOpCodeValue();
   bool          reverseMemOp = false;
   bool          reverseCmpOp = false;

   // Some operators must have their operands swapped to improve the generated
   // code needed to evaluate the result of the comparison.
   //
   bool mustSwapOperands = (cmpOp == TR::iffcmple ||
                            cmpOp == TR::ifdcmple ||
                            cmpOp == TR::iffcmpgtu ||
                            cmpOp == TR::ifdcmpgtu ||
                            cmpOp == TR::fcmple ||
                            cmpOp == TR::dcmple ||
                            cmpOp == TR::fcmpgtu ||
                            cmpOp == TR::dcmpgtu ||
                            cmpOp == TR::iffcmplt ||
                            cmpOp == TR::ifdcmplt ||
                            cmpOp == TR::iffcmpgeu ||
                            cmpOp == TR::ifdcmpgeu ||
                            cmpOp == TR::fcmplt ||
                            cmpOp == TR::dcmplt ||
                            cmpOp == TR::fcmpgeu ||
                            cmpOp == TR::dcmpgeu) ? true : false;

   // Some operators should not have their operands swapped to improve the generated
   // code needed to evaluate the result of the comparison.
   //
   bool preventOperandSwapping = (cmpOp == TR::iffcmpltu ||
                                  cmpOp == TR::ifdcmpltu ||
                                  cmpOp == TR::iffcmpge ||
                                  cmpOp == TR::ifdcmpge ||
                                  cmpOp == TR::fcmpltu ||
                                  cmpOp == TR::dcmpltu ||
                                  cmpOp == TR::fcmpge ||
                                  cmpOp == TR::dcmpge ||
                                  cmpOp == TR::iffcmpgt ||
                                  cmpOp == TR::ifdcmpgt ||
                                  cmpOp == TR::iffcmpleu ||
                                  cmpOp == TR::ifdcmpleu ||
                                  cmpOp == TR::fcmpgt ||
                                  cmpOp == TR::dcmpgt ||
                                  cmpOp == TR::fcmpleu ||
                                  cmpOp == TR::dcmpleu) ? true : false;

   // For correctness, don't swap operands of these operators.
   //
   if (cmpOp == TR::fcmpg || cmpOp == TR::fcmpl ||
       cmpOp == TR::dcmpg || cmpOp == TR::dcmpl)
      {
      preventOperandSwapping = true;
      }

   // Initial operand evaluation ordering.
   //
   if (preventOperandSwapping || (!mustSwapOperands && _cg->whichChildToEvaluate(root) == 0))
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      setReversedOperands(true);
      }

   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   setInputs(firstChild,
             firstRegister,
             secondChild,
             secondRegister,
             false,

             // If either 'preventOperandSwapping' or 'mustSwapOperands' is set then the
             // initial operand ordering set above must be maintained.
             //
             preventOperandSwapping || mustSwapOperands);

   // Make sure any required operand ordering is respected.
   //
   if ((getCmpReg2Reg1() || getCmpReg2Mem1()) &&
       (mustSwapOperands || preventOperandSwapping))
      {
      reverseCmpOp = getCmpReg2Reg1() ? true : false;
      reverseMemOp = getCmpReg2Mem1() ? true : false;
      }

   // Evaluate the children if necessary.
   //
   if (getEvalChild1())
      {
      _cg->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      _cg->evaluate(secondChild);
      }

   TR::TreeEvaluator::coerceFPOperandsToXMMRs(root, _cg);

   firstRegister  = firstChild->getRegister();
   secondRegister = secondChild->getRegister();

   // Generate the compare instruction.
   //
   if (getCmpReg1Mem2() || reverseMemOp)
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, _cg);
      generateRegMemInstruction(cmpRegMemOpCode, root, firstRegister, tempMR, _cg);
      tempMR->decNodeReferenceCounts(_cg);
      }
   else if (getCmpReg2Mem1())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, _cg);
      generateRegMemInstruction(cmpRegMemOpCode, root, secondRegister, tempMR, _cg);
      notReversedOperands();
      tempMR->decNodeReferenceCounts(_cg);
      }
   else if (getCmpReg1Reg2() || reverseCmpOp)
      {
      generateRegRegInstruction(cmpRegRegOpCode, root, firstRegister, secondRegister, _cg);
      }
   else if (getCmpReg2Reg1())
      {
      generateRegRegInstruction(cmpRegRegOpCode, root, secondRegister, firstRegister, _cg);
      notReversedOperands();
      }

   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);

   // Update the opcode on the root node if we have swapped its children.
   // TODO: Reverse the children too, or else this looks wrong in the log file
   //
   if (getReversedOperands())
      {
      cmpOp = TR::ILOpCode(cmpOp).getOpCodeForSwapChildren();
      TR::Node::recreate(root, cmpOp);
      }

   return NULL;
   }
