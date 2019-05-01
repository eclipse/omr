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

#include "x/codegen/SubtractAnalyser.hpp"

#include <stdint.h>
#include "codegen/Analyser.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/X86Evaluator.hpp"
#include "compile/Compilation.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

/*
 * \brief 
 * this API is for check nodes(like OverflowCHK) with sub operation where the operands 
 * are given explicitly by the caller and are not the first and second child of the given root node
 *
 * \param root
 *     the check node
 * \param firstChild, secondChild 
 *     the operands for the sub operation 
 */
void TR_X86SubtractAnalyser::integerSubtractAnalyserWithExplicitOperands(TR::Node      *root,
                                                                         TR::Node      *firstChild,
                                                                         TR::Node      *secondChild,
                                                                         TR_X86OpCodes regRegOpCode,
                                                                         TR_X86OpCodes regMemOpCode,
                                                                         TR_X86OpCodes copyOpCode,
                                                                         bool needsEflags, // false by default
                                                                         TR::Node *borrow)  // 0 by default
   {
   TR_ASSERT(root->getOpCodeValue() == TR::OverflowCHK, "unsupported opcode %s for integerSubtractAnalyserWithExplicitOperands on node %p\n", _cg->comp()->getDebug()->getName(root->getOpCodeValue()), root);
   TR::Register *tempReg = integerSubtractAnalyserImpl(root, firstChild, secondChild, regRegOpCode, regMemOpCode, copyOpCode, needsEflags, borrow);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   _cg->stopUsingRegister(tempReg);
   }

/*
 * \brief 
 * this API is for regular sub operation nodes where the first child and second child are the operands by default
 */
void TR_X86SubtractAnalyser::integerSubtractAnalyser(TR::Node      *root,
                                                     TR_X86OpCodes regRegOpCode,
                                                     TR_X86OpCodes regMemOpCode,
                                                     TR_X86OpCodes copyOpCode,
                                                     bool needsEflags, // false by default
                                                     TR::Node *borrow)  // 0 by default
   {
   TR::Node *firstChild = root->getFirstChild();
   TR::Node *secondChild = root->getSecondChild();
   TR::Register *targetRegister = NULL;
   targetRegister = integerSubtractAnalyserImpl(root, firstChild, secondChild, regRegOpCode, regMemOpCode, copyOpCode, needsEflags, borrow);
   root->setRegister(targetRegister);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   }

/*
 * users should call the integerSubtractAnalyser or integerSubtractAnalyserWithExplicitOperands APIs instead of calling this one directly 
 */
TR::Register* TR_X86SubtractAnalyser::integerSubtractAnalyserImpl(TR::Node     *root,
                                                                  TR::Node     *firstChild,
                                                                  TR::Node     *secondChild,
                                                                  TR_X86OpCodes regRegOpCode,
                                                                  TR_X86OpCodes regMemOpCode,
                                                                  TR_X86OpCodes copyOpCode,
                                                                  bool needsEflags, 
                                                                  TR::Node *borrow)  
   {
   TR::Register *targetRegister = NULL;
   TR::Register *firstRegister = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();
   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   bool loadedConst = false;

   needsEflags = needsEflags || NEED_CC(root);

   if (getEvalChild1())
      {
      // if firstChild and secondChild are the same node, then we should
      // evaluate (take the else path) so that the evaluate for the secondChild
      // below will get the correct/already-allocated register.
      if (firstRegister == 0 && firstChild->getOpCodeValue() == TR::iconst && (firstChild != secondChild))
         {
	   // An iconst may have to be generated.  The iconst will be generated after the
	   //    secondChild is evaluated.  Set the loadedConst flag to true.
	   loadedConst = true;
         }
      else
         {
         firstRegister = _cg->evaluate(firstChild);
         }
      }

   if (getEvalChild2())
      {
      secondRegister = _cg->evaluate(secondChild);
      if (firstChild->getRegister())
         {
         firstRegister = firstChild->getRegister();
         }
      else if (!loadedConst)
         {
         firstRegister = _cg->evaluate(firstChild);
         }
      }

   if (loadedConst)
      {
      if (firstRegister == 0)
         {
         // firstchild is an inconst and it has not been evaluated.
         //   Generate the code for an iconst.
         firstRegister = _cg->allocateRegister();
         TR::TreeEvaluator::insertLoadConstant(firstChild, firstRegister, firstChild->getInt(), TR_RematerializableInt, _cg);
         }
      else
         {
         // firstchild was evaluated.  The code for an iconst does not need to be generated.
         //   Set the loadConst flag to false.
         loadedConst = false;
         }
      }

   if (borrow != 0)
      TR_X86ComputeCC::setCarryBorrow(borrow, true, _cg);

   if (getCopyReg1())
      {
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *thirdReg;
         if (firstChild->getOpCodeValue() == TR::iconst && loadedConst)
            {
            thirdReg = firstRegister;
            }
         else
            {
            if (secondChild->getReferenceCount() == 1 && secondRegister != 0 && !needsEflags && (borrow == 0))
               {
               // use one fewer registers if we negate the clobberable secondRegister and add
               // Don't do this though if condition codes are needed.  The sequence
               // depends on the carry flag being valid as if a sub was done.
               //
               bool nodeIs64Bit = TR_X86OpCode(regRegOpCode).hasLongSource();
               generateRegInstruction(NEGReg(nodeIs64Bit), secondChild, secondRegister, _cg);
               thirdReg       = secondRegister;
               secondRegister = firstRegister;
               regRegOpCode   = ADDRegReg(nodeIs64Bit);
               }
            else
               {
               thirdReg = _cg->allocateRegister();
               generateRegRegInstruction(copyOpCode, root, thirdReg, firstRegister, _cg);
               }
            }
         targetRegister = thirdReg;
         if (getSubReg3Reg2())
            {
            generateRegRegInstruction(regRegOpCode, root, thirdReg, secondRegister, _cg);
            }
         else // assert getSubReg3Mem2() == true
            {
            TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, _cg);
            generateRegMemInstruction(regMemOpCode, root, thirdReg, tempMR, _cg);
            tempMR->decNodeReferenceCounts(_cg);
            }
         }
      else
         {
         if (getSubReg3Reg2())
            {
            generateRegRegInstruction(regRegOpCode, root, firstRegister, secondRegister, _cg);
            }
         else // assert getSubReg3Mem2() == true
            {
            TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, _cg);
            generateRegMemInstruction(regMemOpCode, root, firstRegister, tempMR, _cg);
            tempMR->decNodeReferenceCounts(_cg);
            }
         targetRegister = firstRegister;
         }
      }
   else if (getSubReg1Reg2())
      {
      generateRegRegInstruction(regRegOpCode, root, firstRegister, secondRegister, _cg);
      targetRegister = firstRegister;
      }
   else // assert getSubReg1Mem2() == true
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, _cg);
      generateRegMemInstruction(regMemOpCode, root, firstRegister, tempMR, _cg);
      targetRegister = firstRegister;
      tempMR->decNodeReferenceCounts(_cg);
      }
   return targetRegister;
   }

// Volatile memory operands are not allowed in long subtractions
// if we are compiling for an SMP machine, as the carry flag can
// get clobbered by the memory barrier immediately preceding the
// SBB4RegMem instruction.
//
static bool isVolatileMemoryOperand(TR::Node *node)
   {
   if (TR::Compiler->target.isSMP() && node->getOpCode().isMemoryReference())
      {
      TR_ASSERT(node->getSymbolReference(), "expecting a symbol reference\n");
      TR::Symbol *sym = node->getSymbolReference()->getSymbol();
      return (sym && sym->isVolatile());
      }
   return false;
   }

/*
 * \brief 
 * this API is for check nodes(like OverflowCHK) with an lsub operation where the operands 
 * are given explicitly by the caller and are not the first and second child of the given root node
 *
 * \param root
 *     the check node
 * \param firstChild, secondChild 
 *     the operands for the lsub operation 
 */
void TR_X86SubtractAnalyser::longSubtractAnalyserWithExplicitOperands(TR::Node *root, TR::Node *firstChild, TR::Node *secondChild)
   {
   TR_ASSERT(root->getOpCodeValue() == TR::OverflowCHK, "unsupported opcode %s for longSubtractAnalyserWithExplicitOperands on node %p\n", _cg->comp()->getDebug()->getName(root->getOpCodeValue()), root);
   TR::Register *tempReg = longSubtractAnalyserImpl(root, firstChild, secondChild);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   _cg->stopUsingRegister(tempReg);
   }

/*
 * \brief 
 * this API is intended for regular lsub operation nodes where the first child and second child are the operands by default
 */
void TR_X86SubtractAnalyser::longSubtractAnalyser(TR::Node *root)
   {
   TR::Node *firstChild = root->getFirstChild();
   TR::Node *secondChild = root->getSecondChild();
   TR::Register *targetRegister = NULL;
   targetRegister = longSubtractAnalyserImpl(root, firstChild, secondChild);
   root->setRegister(targetRegister);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   }

/*
 * users should call the longSubtractAnalyser or longSubtractAnalyserWithExplicitOperands APIs instead of calling this one directly 
 */
TR::Register* TR_X86SubtractAnalyser::longSubtractAnalyserImpl(TR::Node *root, TR::Node *&firstChild, TR::Node *&secondChild)
   {
   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();
   TR::Register *targetRegister = NULL;

   bool firstHighZero      = false;
   bool secondHighZero     = false; bool useSecondHighOrder = false;

   TR_X86OpCodes regRegOpCode = SUB4RegReg;
   TR_X86OpCodes regMemOpCode = SUB4RegMem;

   bool needsEflags = NEED_CC(root) || (root->getOpCodeValue() == TR::lusubb);

   // Can generate better code for long adds when one or more children have a high order zero word
   // can avoid the evaluation when we don't need the result of such nodes for another parent.
   //
   if (firstChild->isHighWordZero() && !needsEflags)
      {
      firstHighZero = true;
      }

   if (secondChild->isHighWordZero() && !needsEflags)
      {
      secondHighZero = true;
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if (secondChild->getReferenceCount() == 1 && secondRegister == 0)
         {
         if (secondOp == TR::iu2l || secondOp == TR::su2l ||
             secondOp == TR::bu2l ||
             (secondOp == TR::lushr &&
              secondChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (secondChild->getSecondChild()->getInt() & TR::TreeEvaluator::shiftMask(true)) == 32))
            {
            secondChild    = secondChild->getFirstChild();
            secondRegister = secondChild->getRegister();
            if (secondOp == TR::lushr)
               {
               useSecondHighOrder = true;
               }
            }
         }
      }

   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (isVolatileMemoryOperand(firstChild))
      resetMem1();

   if (isVolatileMemoryOperand(secondChild))
      resetMem2();

   if (getEvalChild1())
      {
      firstRegister = _cg->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = _cg->evaluate(secondChild);
      }

   if (secondHighZero && secondRegister && secondRegister->getRegisterPair())
      {
      if (!useSecondHighOrder)
         {
         secondRegister = secondRegister->getLowOrder();
         }
      else
         {
         secondRegister = secondRegister->getHighOrder();
         }
      }

   if (root->getOpCodeValue() == TR::lusubb &&
       TR_X86ComputeCC::setCarryBorrow(root->getChild(2), true, _cg))
      {
      // use SBB rather than SUB
      //
      regRegOpCode = SBB4RegReg;
      regMemOpCode = SBB4RegMem;
      }

   if (getCopyReg1())
      {
      TR::Register     *lowThird  = _cg->allocateRegister();
      TR::Register     *highThird = _cg->allocateRegister();
      TR::RegisterPair *thirdReg  = _cg->allocateRegisterPair(lowThird, highThird);
      targetRegister = thirdReg;
      generateRegRegInstruction(MOV4RegReg, root, lowThird, firstRegister->getLowOrder(), _cg);

      if (firstHighZero)
         {
         generateRegRegInstruction(XOR4RegReg, root, highThird, highThird, _cg);
         }
      else
         {
         generateRegRegInstruction(MOV4RegReg, root, highThird, firstRegister->getHighOrder(), _cg);
         }

      if (getSubReg3Reg2())
         {
         if (secondHighZero)
            {
            generateRegRegInstruction(regRegOpCode, root, lowThird, secondRegister, _cg);
            generateRegImmInstruction(SBB4RegImms, root, highThird, 0, _cg);
            }
         else
            {
            generateRegRegInstruction(regRegOpCode, root, lowThird, secondRegister->getLowOrder(), _cg);
            generateRegRegInstruction(SBB4RegReg, root, highThird, secondRegister->getHighOrder(), _cg);
            }
         }
      else // assert getSubReg3Mem2() == true
         {
         TR::MemoryReference  *lowMR = generateX86MemoryReference(secondChild, _cg);
         /**
          * The below code is needed to ensure correct behaviour when the subtract analyser encounters a lushr bytecode that shifts
          * by 32 bits. This is the only case where the useSecondHighOrder bit is set.
          * When the first child of the lushr is in a register, code above handles the shift. When the first child of the lushr is in
          * memory, the below ensures that the upper part of the first child of the lushr is used as lowMR.
          */
         if (useSecondHighOrder)
            {
            TR_ASSERT(secondHighZero, "useSecondHighOrder should be consistent with secondHighZero. useSecondHighOrder subsumes secondHighZero");
        	   lowMR = generateX86MemoryReference(*lowMR, 4, _cg);
            }

         generateRegMemInstruction(regMemOpCode, root, lowThird, lowMR, _cg);
         if (secondHighZero)
            {
            generateRegImmInstruction(SBB4RegImms, root, highThird, 0, _cg);
            }
         else
            {
            TR::MemoryReference  *highMR = generateX86MemoryReference(*lowMR, 4, _cg);
            generateRegMemInstruction(SBB4RegMem, root, highThird, highMR, _cg);
            }
         lowMR->decNodeReferenceCounts(_cg);
         }
      }
   else if (getSubReg1Reg2())
      {
      if (secondHighZero)
         {
         generateRegRegInstruction(regRegOpCode, root, firstRegister->getLowOrder(), secondRegister, _cg);
         generateRegImmInstruction(SBB4RegImms, root, firstRegister->getHighOrder(), 0, _cg);
         }
      else
         {
         generateRegRegInstruction(regRegOpCode, root, firstRegister->getLowOrder(), secondRegister->getLowOrder(), _cg);
         generateRegRegInstruction(SBB4RegReg, root, firstRegister->getHighOrder(), secondRegister->getHighOrder(), _cg);
         }
      targetRegister = firstRegister;
      }
   else // assert getSubReg1Mem2() == true
      {
      TR::MemoryReference  *lowMR = generateX86MemoryReference(secondChild, _cg);
      /**
       * The below code is needed to ensure correct behaviour when the subtract analyser encounters a lushr bytecode that shifts
       * by 32 bits. This is the only case where the useSecondHighOrder bit is set.
       * When the first child of the lushr is in a register, code above handles the shift. When the first child of the lushr is in
       * memory, the below ensures that the upper part of the first child of the lushr is used as lowMR.
       */
      if (useSecondHighOrder)
     	 lowMR = generateX86MemoryReference(*lowMR, 4, _cg);

      generateRegMemInstruction(regMemOpCode, root, firstRegister->getLowOrder(), lowMR, _cg);

      if (secondHighZero)
         {
         generateRegImmInstruction(SBB4RegImms, root, firstRegister->getHighOrder(), 0, _cg);
         }
      else
         {
         TR::MemoryReference  *highMR = generateX86MemoryReference(*lowMR, 4, _cg);
         generateRegMemInstruction(SBB4RegMem, root, firstRegister->getHighOrder(), highMR, _cg);
         }

      targetRegister = firstRegister;
      lowMR->decNodeReferenceCounts(_cg);
      }

   return targetRegister;
   }

const uint8_t TR_X86SubtractAnalyser::_actionMap[NUM_ACTIONS] =
   {                  // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   EvalChild1 |       //  0    0     0    0    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    0    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    0    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    0    1     1
   CopyReg1   |
   SubReg3Mem2,

   EvalChild1 |       //  0    0     0    1    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    1    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    1    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    1    1     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     1    0    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    0    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    0    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    0    1     1
   SubReg1Mem2,

   EvalChild1 |       //  0    0     1    1    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    1    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    1     0    0    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    0    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    0    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    0    1     1
   CopyReg1   |
   SubReg3Mem2,

   EvalChild1 |       //  0    1     0    1    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    1    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    1    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    1    1     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     1    0    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    0    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    0    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    0    1     1
   SubReg1Mem2,

   EvalChild1 |       //  0    1     1    1    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    1    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    1    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    1    1     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    0     0    0    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    0     0    0    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    0     0    0    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    0     0    0    1     1
   CopyReg1   |
   SubReg3Mem2,

   EvalChild1 |       //  1    0     0    1    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    0     0    1    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    0     0    1    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    0     0    1    1     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    0     1    0    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    0     1    0    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    0     1    0    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    0     1    0    1     1
   SubReg1Mem2,

   EvalChild1 |       //  1    0     1    1    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    0     1    1    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    0     1    1    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    0     1    1    1     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    1     0    0    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    1     0    0    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    1     0    0    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    1     0    0    1     1
   CopyReg1   |
   SubReg3Mem2,

   EvalChild1 |       //  1    1     0    1    0     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    1     0    1    0     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    1     0    1    1     0
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    1     0    1    1     1
   EvalChild2 |
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  1    1     1    0    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    1     1    0    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    1     1    0    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    1     1    0    1     1
   SubReg1Mem2,

   EvalChild1 |       //  1    1     1    1    0     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    1     1    1    0     1
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    1     1    1    1     0
   EvalChild2 |
   SubReg1Reg2,

   EvalChild1 |       //  1    1     1    1    1     1
   EvalChild2 |
   SubReg1Reg2
   };
