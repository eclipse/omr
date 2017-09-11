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

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "codegen/ARMInstruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/BinaryCommutativeAnalyser.hpp"


void TR_ARMBinaryCommutativeAnalyser::genericAnalyser(TR::Node       *root,
                                                       TR_ARMOpCodes regToRegOpCode,
                                                       TR_ARMOpCodes copyOpCode,
                                                       bool          nonClobberingDestination)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   if (cg()->whichChildToEvaluate(root) == 0)
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      setReversedOperands(true);
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      }
   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   setInputs(firstChild, firstRegister, secondChild, secondRegister, nonClobberingDestination);

   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }


   if (getOpReg1Reg2())
      {
//      new TR::ARMRegRegInstruction(regToRegOpCode, firstRegister, secondRegister);
//      root->setRegister(firstRegister);
      }
   else if (getOpReg2Reg1())
      {
//      new TR::ARMRegRegInstruction(regToRegOpCode, secondRegister, firstRegister);
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }
   else if (getCopyReg1())
      {
//      TR::Register *tempReg = root->setRegister(cg()->allocateRegister());
//      new TR::ARMRegRegInstruction(copyOpCode, tempReg, firstRegister);
//      new TR::ARMRegRegInstruction(regToRegOpCode, tempReg, secondRegister);
      }
   else if (getCopyReg2())
      {
//      TR::Register *tempReg = root->setRegister(cg()->allocateRegister());
//      new TR::ARMRegRegInstruction(copyOpCode, tempReg, secondRegister);
//      new TR::ARMRegRegInstruction(regToRegOpCode, tempReg, firstRegister);
//      notReversedOperands();
      }
   else if (getOpReg1Mem2())
      {
//      TR_ARMMemoryReference *tempMR = new TR_ARMMemoryReference(secondChild);
//      new TR::ARMRegMemInstruction(memToRegOpCode, firstRegister, tempMR);
//      tempMR->decNodeReferenceCounts();
//      root->setRegister(firstRegister);
      }
   else
      {
//      TR_ARMMemoryReference *tempMR = new TR_ARMMemoryReference(firstChild);
//      new TR::ARMRegMemInstruction(memToRegOpCode, secondRegister, tempMR);
//      tempMR->decNodeReferenceCounts();
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();

   return;

   }

void TR_ARMBinaryCommutativeAnalyser::genericLongAnalyser(TR::Node       *root,
                                                           TR_ARMOpCodes lowRegToRegOpCode,
                                                           TR_ARMOpCodes highRegToRegOpCode,
                                                           TR_ARMOpCodes lowMemToRegOpCode,
                                                           TR_ARMOpCodes highMemToRegOpCode,
                                                           TR_ARMOpCodes copyOpCode)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   if (cg()->whichChildToEvaluate(root) == 0)
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      setReversedOperands(true);
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      }
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


   if (getOpReg1Reg2())
      {
//      new TR::ARMRegRegInstruction(lowRegToRegOpCode,
//                                  firstRegister->getLowOrder(),
//                                  secondRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(highRegToRegOpCode,
//                                  firstRegister->getHighOrder(),
//                                  secondRegister->getHighOrder());
//      root->setRegister(firstRegister);
      }
   else if (getOpReg2Reg1())
      {
//      new TR::ARMRegRegInstruction(lowRegToRegOpCode,
//                                  secondRegister->getLowOrder(),
//                                  firstRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(highRegToRegOpCode,
//                                  secondRegister->getHighOrder(),
//                                  firstRegister->getHighOrder());
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }
   else if (getCopyReg1())
      {
//      TR::Register *tempReg = root->setRegister(cg()->allocateRegisterPair());
//      new TR::ARMRegRegInstruction(copyOpCode,
//                                  tempReg->getLowOrder(),
//                                  firstRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(copyOpCode,
//                                  tempReg->getHighOrder(),
//                                  firstRegister->getHighOrder());
//      new TR::ARMRegRegInstruction(lowRegToRegOpCode,
//                                  tempReg->getLowOrder(),
//                                  secondRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(highRegToRegOpCode,
//                                  tempReg->getHighOrder(),
//                                  secondRegister->getHighOrder());
      }
   else if (getCopyReg2())
      {
//      TR::Register *tempReg = root->setRegister(cg()->allocateRegisterPair());
//      new TR::ARMRegRegInstruction(copyOpCode,
//                                  tempReg->getLowOrder(),
//                                  secondRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(copyOpCode,
//                                  tempReg->getHighOrder(),
//                                  secondRegister->getHighOrder());
//      new TR::ARMRegRegInstruction(lowRegToRegOpCode,
//                                  tempReg->getLowOrder(),
//                                  firstRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(highRegToRegOpCode,
//                                  tempReg->getHighOrder(),
//                                  firstRegister->getHighOrder());
//      notReversedOperands();
      }
   else if (getOpReg1Mem2())
      {
//      TR_ARMMemoryReference *lowMR  = new TR_ARMMemoryReference(secondChild);
//      TR_ARMMemoryReference *highMR = new TR_ARMMemoryReference(*lowMR, 4);
//      new TR::ARMRegMemInstruction(lowMemToRegOpCode,
//                                  firstRegister->getLowOrder(),
//                                  lowMR);
//      new TR::ARMRegMemInstruction(highMemToRegOpCode,
//                                  firstRegister->getHighOrder(),
//                                  highMR);
//      lowMR->decNodeReferenceCounts();
//      root->setRegister(firstRegister);
      }
   else
      {
//      TR_ARMMemoryReference *lowMR  = new TR_ARMMemoryReference(firstChild);
//      TR_ARMMemoryReference *highMR = new TR_ARMMemoryReference(*lowMR, 4);
//      new TR::ARMRegMemInstruction(lowMemToRegOpCode,
//                                  secondRegister->getLowOrder(),
//                                  lowMR);
//      new TR::ARMRegMemInstruction(highMemToRegOpCode,
//                                  secondRegister->getHighOrder(),
//                                  highMR);
//      lowMR->decNodeReferenceCounts();
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();

   return;

   }


void TR_ARMBinaryCommutativeAnalyser::integerAddAnalyser(TR::Node       *root,
                                                         TR_ARMOpCodes regToRegOpCode)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   if (cg()->whichChildToEvaluate(root) == 0)
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

   setInputs(firstChild, firstRegister, secondChild, secondRegister, true);

   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }


   if (getOpReg1Reg2())
      {
//      new TR::ARMRegRegInstruction(regToRegOpCode, firstRegister, secondRegister);
//      root->setRegister(firstRegister);
      }
   else if (getOpReg2Reg1())
      {
//      new TR::ARMRegRegInstruction(regToRegOpCode, secondRegister, firstRegister);
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }
   else if (getCopyRegs())
      {
//      TR::Register *tempReg = root->setRegister(cg()->allocateRegister());
//      TR_ARMMemoryReference *tempMR = new TR_ARMMemoryReference();
//      tempMR->setBaseRegister(firstRegister);
//      tempMR->setIndexRegister(secondRegister);
//      new TR::ARMRegMemInstruction(LEA4RegMem, tempReg, tempMR);
      }
   else if (getOpReg1Mem2())
      {
//      TR_ARMMemoryReference *tempMR = new TR_ARMMemoryReference(secondChild);
//      new TR::ARMRegMemInstruction(memToRegOpCode, firstRegister, tempMR);
//      tempMR->decNodeReferenceCounts();
//      root->setRegister(firstRegister);
      }
   else
      {
//      TR_ARMMemoryReference *tempMR = new TR_ARMMemoryReference(firstChild);
//      new TR::ARMRegMemInstruction(memToRegOpCode, secondRegister, tempMR);
//      tempMR->decNodeReferenceCounts();
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();

   return;

   }

void TR_ARMBinaryCommutativeAnalyser::longAddAnalyser(TR::Node *root)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   if (cg()->whichChildToEvaluate(root) == 0)
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

   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   if (getOpReg1Reg2())
      {
//      new TR::ARMRegRegInstruction(ADD4RegReg, firstRegister->getLowOrder(), secondRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(ADC4RegReg, firstRegister->getHighOrder(), secondRegister->getHighOrder());
//      root->setRegister(firstRegister);
      }
   else if (getOpReg2Reg1())
      {
//      new TR::ARMRegRegInstruction(ADD4RegReg, secondRegister->getLowOrder(), firstRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(ADC4RegReg, secondRegister->getHighOrder(), firstRegister->getHighOrder());
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }
   else if (getCopyRegs())
      {
//      TR::Register     *lowRegister  = cg()->allocateRegister();
//      TR::Register     *highRegister = cg()->allocateRegister();
//      TR_RegisterPair *tempReg      = cg()->allocateRegisterPair(lowRegister, highRegister);
//      root->setRegister(tempReg);
//      new TR::ARMRegRegInstruction(MOV4RegReg, lowRegister, firstRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(MOV4RegReg, highRegister, firstRegister->getHighOrder());
//      new TR::ARMRegRegInstruction(ADD4RegReg, tempReg->getLowOrder(), secondRegister->getLowOrder());
//      new TR::ARMRegRegInstruction(ADC4RegReg, tempReg->getHighOrder(), secondRegister->getHighOrder());
//      root->setRegister(tempReg);
      }
   else if (getOpReg1Mem2())
      {
//      TR_ARMMemoryReference *lowMR  = new TR_ARMMemoryReference(secondChild);
//      TR_ARMMemoryReference *highMR = new TR_ARMMemoryReference(*lowMR, 4);
//      new TR::ARMRegMemInstruction(ADD4RegMem, firstRegister->getLowOrder(), lowMR);
//      new TR::ARMRegMemInstruction(ADC4RegMem, firstRegister->getHighOrder(), highMR);
//      lowMR->decNodeReferenceCounts();
//      root->setRegister(firstRegister);
      }
   else
      {
//      TR_ARMMemoryReference *lowMR  = new TR_ARMMemoryReference(firstChild);
//      TR_ARMMemoryReference *highMR = new TR_ARMMemoryReference(*lowMR, 4);
//      new TR::ARMRegMemInstruction(ADD4RegMem, secondRegister->getLowOrder(), lowMR);
//      new TR::ARMRegMemInstruction(ADC4RegMem, secondRegister->getHighOrder(), highMR);
//      lowMR->decNodeReferenceCounts();
//      root->setRegister(secondRegister);
//      notReversedOperands();
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();

   return;

   }


const uint8_t TR_ARMBinaryCommutativeAnalyser::actionMap[NUM_ACTIONS] =
   {                  // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   EvalChild1 |       //  0    0     0    0    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    0     0    0    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    0     0    0    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    0     0    0    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    0     0    1    0     0
   CopyReg2,

   EvalChild1 |       //  0    0     0    1    0     1
   OpReg2Reg1,

   EvalChild1 |       //  0    0     0    1    1     0
   CopyReg2,

   EvalChild1 |       //  0    0     0    1    1     1
   OpReg2Reg1,

   EvalChild1 |       //  0    0     1    0    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    0    0     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    0    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |       //  0    0     1    1    0     0
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    1    0     1
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     0
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     1
   OpReg1Reg2,

   EvalChild1 |       //  0    1     0    0    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    1     0    0    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    1     0    0    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    1     0    0    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    1     0    1    0     0
   CopyReg2,

   EvalChild1 |       //  0    1     0    1    0     1
   OpReg2Reg1,

   EvalChild1 |       //  0    1     0    1    1     0
   CopyReg2,

   EvalChild1 |       //  0    1     0    1    1     1
   OpReg2Reg1,

   EvalChild1 |       //  0    1     1    0    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild2 |       //  0    1     1    0    0     1
   OpReg2Mem1,

   EvalChild1 |       //  0    1     1    0    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    1     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |       //  0    1     1    1    0     0
   OpReg1Reg2,

   OpReg2Mem1,        //  0    1     1    1    0     1

   EvalChild1 |       //  0    1     1    1    1     0
   OpReg1Reg2,

   OpReg2Mem1,        //  0    1     1    1    1     1

   EvalChild2 |       //  1    0     0    0    0     0
   CopyReg1,

   EvalChild2 |       //  1    0     0    0    0     1
   OpReg2Reg1,

   EvalChild2 |       //  1    0     0    0    1     0
   CopyReg1,

   EvalChild2 |       //  1    0     0    0    1     1
   OpReg2Reg1,

   CopyReg1,          //  1    0     0    1    0     0

   OpReg2Reg1,        //  1    0     0    1    0     1

   CopyReg1,          //  1    0     0    1    1     0

   OpReg2Reg1,        //  1    0     0    1    1     1

   EvalChild2 |       //  1    0     1    0    0     0
   OpReg1Reg2,

   EvalChild2 |       //  1    0     1    0    0     1
   OpReg1Reg2,

   EvalChild2 |       //  1    0     1    0    1     0
   OpReg1Reg2,

   OpReg1Mem2,        //  1    0     1    0    1     1

   OpReg1Reg2,        //  1    0     1    1    0     0

   OpReg1Reg2,        //  1    0     1    1    0     1

   OpReg1Reg2,        //  1    0     1    1    1     0

   OpReg1Reg2,        //  1    0     1    1    1     1

   EvalChild2 |       //  1    1     0    0    0     0
   CopyReg1,

   EvalChild2 |       //  1    1     0    0    0     1
   OpReg2Reg1,

   EvalChild2 |       //  1    1     0    0    1     0
   CopyReg1,

   EvalChild2 |       //  1    1     0    0    1     1
   OpReg2Reg1,

   CopyReg1,          //  1    1     0    1    0     0

   OpReg2Reg1,        //  1    1     0    1    0     1

   CopyReg1,          //  1    1     0    1    1     0

   OpReg2Reg1,        //  1    1     0    1    1     1

   EvalChild2 |       //  1    1     1    0    0     0
   OpReg1Reg2,

   EvalChild2 |       //  1    1     1    0    0     1
   OpReg1Reg2,

   EvalChild2 |       //  1    1     1    0    1     0
   OpReg1Reg2,

   OpReg1Mem2,        //  1    1     1    0    1     1

   OpReg1Reg2,        //  1    1     1    1    0     0

   OpReg1Reg2,        //  1    1     1    1    0     1

   OpReg1Reg2,        //  1    1     1    1    1     0

   OpReg1Reg2         //  1    1     1    1    1     1
   };

