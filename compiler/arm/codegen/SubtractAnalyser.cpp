/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "codegen/SubtractAnalyser.hpp"

#include "arm/codegen/ARMInstruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

void TR_ARMSubtractAnalyser::integerSubtractAnalyser(TR::Node       *root,
                                                      TR_ARMOpCodes regToRegOpCode,
                                                      TR_ARMOpCodes memToRegOpCode)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   firstChild  = root->getFirstChild();
   secondChild = root->getSecondChild();
   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   if (getCopyReg1())
      {
      TR::Register *thirdReg = root->setRegister(cg()->allocateRegister());
      if (getSubReg3Reg2())
         {
         new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(ARMOp_sub, root, thirdReg, secondRegister, firstRegister, cg());
         }
      else // assert getSubReg3Mem2() == true
         {
//         TR_ARMMemoryReference *tempMR = new (cg()->trHeapMemory()) TR_ARMMemoryReference(secondChild);
//         new (cg()->trHeapMemory()) TR::ARMRegMemInstruction(memToRegOpCode, thirdReg, tempMR, cg());
//         tempMR->decNodeReferenceCounts();
         }
      root->setRegister(thirdReg);
      }
   else if (getSubReg1Reg2())
      {
      new (cg()->trHeapMemory()) TR::ARMTrg1Src1Instruction(ARMOp_sub, root, firstRegister, secondRegister, cg());
      root->setRegister(firstRegister);
      }
   else // assert getSubReg1Mem2() == true
      {
//      TR_ARMMemoryReference *tempMR = new (cg()->trHeapMemory()) TR_ARMMemoryReference(secondChild);
//      new (cg()->trHeapMemory()) TR::ARMRegMemInstruction(memToRegOpCode, firstRegister, tempMR, cg());
//      tempMR->decNodeReferenceCounts();
//      root->setRegister(firstRegister);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();

   return;

   }

void TR_ARMSubtractAnalyser::longSubtractAnalyser(TR::Node *root)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   firstChild  = root->getFirstChild();
   secondChild = root->getSecondChild();
   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   if (getCopyReg1())
      {
      TR::Register     *lowThird  = cg()->allocateRegister();
      TR::Register     *highThird = cg()->allocateRegister();
      TR::RegisterPair *thirdReg  = cg()->allocateRegisterPair(lowThird, highThird);
      root->setRegister(thirdReg);
      new (cg()->trHeapMemory()) TR::ARMTrg1Src1Instruction(ARMOp_mov, root, lowThird, firstRegister->getLowOrder(), cg());
      new (cg()->trHeapMemory()) TR::ARMTrg1Src1Instruction(ARMOp_mov, root, highThird, firstRegister->getHighOrder(), cg());
      if (getSubReg3Reg2())
         {
         new (cg()->trHeapMemory()) TR::ARMTrg1Src1Instruction(ARMOp_sub, root, lowThird, secondRegister->getLowOrder(), cg());
//         new (cg()->trHeapMemory()) TR::ARMTrg1Src1Instruction(SBB4RegReg, root, highThird, secondRegister->getHighOrder(), cg());
         }
      else // assert getSubReg3Mem2() == true
         {
//         TR_ARMMemoryReference *lowMR = new (cg()->trHeapMemory()) TR_ARMMemoryReference(secondChild);
//         new (cg()->trHeapMemory()) TR::ARMRegMemInstruction(SUB4RegMem, lowThird, lowMR, cg());
//         TR_ARMMemoryReference *highMR = new (cg()->trHeapMemory()) TR_ARMMemoryReference(*lowMR, 4);
//         new (cg()->trHeapMemory()) TR::ARMRegMemInstruction(SBB4RegMem, highThird, highMR, cg());
//         lowMR->decNodeReferenceCounts();
         }
      root->setRegister(thirdReg);
      }
   else if (getSubReg1Reg2())
      {
//      new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(root, ARMOp_subf, firstRegister->getLowOrder(), secondRegister->getLowOrder(), cg());
//      new (cg()->trHeapMemory()) TR::ARMRegRegInstruction(SBB4RegReg, firstRegister->getHighOrder(), secondRegister->getHighOrder(), cg());
      root->setRegister(firstRegister);
      }
   else // assert getSubReg1Mem2() == true
      {
//      TR_ARMMemoryReference *lowMR = new (cg()->trHeapMemory()) TR_ARMMemoryReference(secondChild);
//      new (cg()->trHeapMemory()) TR::ARMRegMemInstruction(SUB4RegMem, firstRegister->getLowOrder(), lowMR, cg());
//      TR_ARMMemoryReference *highMR = new (cg()->trHeapMemory()) TR_ARMMemoryReference(*lowMR, 4);
//      new (cg()->trHeapMemory()) TR::ARMRegMemInstruction(SBB4RegMem, firstRegister->getHighOrder(), highMR, cg());
//      lowMR->decNodeReferenceCounts();
//      root->setRegister(firstRegister);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();

   return;

   }

const uint8_t TR_ARMSubtractAnalyser::actionMap[NUM_ACTIONS] =
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
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    1    0     1
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    1    1     0
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    0     0    1    1     1
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
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    1    0     1
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     0
   SubReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     1
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
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    1    0     1
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    1    1     0
   CopyReg1   |
   SubReg3Reg2,

   EvalChild1 |       //  0    1     0    1    1     1
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
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    1    0     1
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    1    1     0
   SubReg1Reg2,

   EvalChild1 |       //  0    1     1    1    1     1
   SubReg1Reg2,

   EvalChild2 |       //  1    0     0    0    0     0
   CopyReg1   |
   SubReg3Reg2,

   EvalChild2 |       //  1    0     0    0    0     1
   CopyReg1   |
   SubReg3Reg2,

   EvalChild2 |       //  1    0     0    0    1     0
   CopyReg1   |
   SubReg3Reg2,

   CopyReg1   |       //  1    0     0    0    1     1
   SubReg3Mem2,

   CopyReg1   |       //  1    0     0    1    0     0
   SubReg3Reg2,

   CopyReg1   |       //  1    0     0    1    0     1
   SubReg3Reg2,

   CopyReg1   |       //  1    0     0    1    1     0
   SubReg3Reg2,

   CopyReg1   |       //  1    0     0    1    1     1
   SubReg3Reg2,

   EvalChild2 |       //  1    0     1    0    0     0
   SubReg1Reg2,

   EvalChild2 |       //  1    0     1    0    0     1
   SubReg1Reg2,

   EvalChild2 |       //  1    0     1    0    1     0
   SubReg1Reg2,

   SubReg1Mem2,       //  1    0     1    0    1     1

   SubReg1Reg2,       //  1    0     1    1    0     0

   SubReg1Reg2,       //  1    0     1    1    0     1

   SubReg1Reg2,       //  1    0     1    1    1     0

   SubReg1Reg2,       //  1    0     1    1    1     1

   EvalChild2 |       //  1    1     0    0    0     0
   CopyReg1   |
   SubReg3Reg2,

   EvalChild2 |       //  1    1     0    0    0     1
   CopyReg1   |
   SubReg3Reg2,

   EvalChild2 |       //  1    1     0    0    1     0
   CopyReg1   |
   SubReg3Reg2,

   CopyReg1   |       //  1    1     0    0    1     1
   SubReg3Mem2,

   CopyReg1   |       //  1    1     0    1    0     0
   SubReg3Reg2,

   CopyReg1   |       //  1    1     0    1    0     1
   SubReg3Reg2,

   CopyReg1   |       //  1    1     0    1    1     0
   SubReg3Reg2,

   CopyReg1   |       //  1    1     0    1    1     1
   SubReg3Reg2,

   EvalChild2 |       //  1    1     1    0    0     0
   SubReg1Reg2,

   EvalChild2 |       //  1    1     1    0    0     1
   SubReg1Reg2,

   EvalChild2 |       //  1    1     1    0    1     0
   SubReg1Reg2,

   SubReg1Mem2,       //  1    1     1    0    1     1

   SubReg1Reg2,       //  1    1     1    1    0     0

   SubReg1Reg2,       //  1    1     1    1    0     1

   SubReg1Reg2,       //  1    1     1    1    1     0

   SubReg1Reg2        //  1    1     1    1    1     1
   };
