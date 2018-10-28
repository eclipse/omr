/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "z/codegen/CompareAnalyser.hpp"

#include <stddef.h>                                // for NULL
#include <stdint.h>                                // for int32_t, uint32_t, etc
#include "codegen/Analyser.hpp"                    // for NUM_ACTIONS
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/RegisterDependency.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"                        // for LONG_SHIFT_MASK
#include "il/ILOpCodes.hpp"                        // for ILOpCodes::iload, etc
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                           // for Symbol
#include "il/SymbolReference.hpp"                  // for SymbolReference
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "z/codegen/S390GenerateInstructions.hpp"

void
TR_S390CompareAnalyser::integerCompareAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic regRegOpCode, TR::InstOpCode::Mnemonic regMemOpCode,
   TR::InstOpCode::Mnemonic memRegOpCode)
   {
   TR_ASSERT( 0, "TR_S390CompareAnalyser::integerCompareAnalyser: Not implemented yet");
   return;
   }

TR::RegisterDependencyConditions *
TR_S390CompareAnalyser::longOrderedCompareAndBranchAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::S390BranchCondition brCmpLowTrueCond,
   TR::InstOpCode::S390BranchCondition brCmpHighTrueCond, TR::InstOpCode::S390BranchCondition brCmpHighFalseCond, TR::LabelSymbol * trueTarget, TR::LabelSymbol * falseTarget,
   bool &internalControlFlowStarted)
   {
   TR::Node * firstChild = root->getFirstChild();
   TR::Node * secondChild = root->getSecondChild();
   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();
   bool firstIU2L = false;
   bool secondIU2L = false;
   bool useFirstHighOrder = false;
   bool useSecondHighOrder = false;
   bool firstHighZero = false;
   bool secondHighZero = false;
   bool isUnsignedCmp = root->getOpCode().isUnsignedCompare();

   // can generate better code for compares when one or more children are TR::iu2l
   // but only when we don't need the result of the iu2l for another parent.
   // Unsigned compare is used for address comparisons, and as such will almost
   // never be compared to constants, including NULL.  Avoid complications with
   // below optimization and just skip it.
   if ( firstChild->isHighWordZero() && !isUnsignedCmp && (brCmpLowTrueCond==TR::InstOpCode::COND_BE || brCmpLowTrueCond==TR::InstOpCode::COND_BNE) )
      {
      firstHighZero = true;
      }

   if ( secondChild->isHighWordZero() && !isUnsignedCmp)
      {
      secondHighZero = true;
      }

   setInputs(firstChild, firstRegister, secondChild, secondRegister,
             true, false, _cg->comp());

   TR::MemoryReference * lowFirstMR = NULL;
   TR::MemoryReference * highFirstMR = NULL;
   TR::MemoryReference * lowSecondMR = NULL;
   TR::MemoryReference * highSecondMR = NULL;
   TR::Register * delayedFirst = NULL;
   TR::Register * delayedSecond = NULL;

   if (_cg->whichChildToEvaluate(root) == 0)
      {
      if (getEvalChild1())
         {
         if (firstChild->getReferenceCount() == 1 &&
            firstChild->getRegister() == NULL &&
            (firstChild->getOpCodeValue() == TR::lload || (firstChild->getOpCodeValue() == TR::iload && firstIU2L)))
            {
            lowFirstMR = generateS390MemoryReference(firstChild, _cg);
            delayedFirst = _cg->allocateRegister();
            if (!firstIU2L)
               {
               highFirstMR = generateS390MemoryReference(*lowFirstMR, 4, _cg);
               generateRXInstruction(_cg, TR::InstOpCode::L, root, delayedFirst, highFirstMR);
               }
            }
         else
            {
            firstRegister = _cg->evaluate(firstChild);
            }
         }
      if (getEvalChild2())
         {
         if (secondChild->getReferenceCount() == 1 &&
            secondChild->getRegister() == NULL &&
            (secondChild->getOpCodeValue() == TR::lload || (secondChild->getOpCodeValue() == TR::iload && secondIU2L)))
            {
            lowSecondMR = generateS390MemoryReference(secondChild, _cg);
            delayedSecond = _cg->allocateRegister();
            if (!secondIU2L)
               {
               highSecondMR = generateS390MemoryReference(*lowSecondMR, 4, _cg);
               generateRXInstruction(_cg, TR::InstOpCode::L, root, delayedSecond, highSecondMR);
               }
            }
         else
            {
            secondRegister = _cg->evaluate(secondChild);
            }
         }
      }
   else
      {
      if (getEvalChild2())
         {
         if (secondChild->getReferenceCount() == 1 &&
            secondChild->getRegister() == NULL &&
            (secondChild->getOpCodeValue() == TR::lload || (secondChild->getOpCodeValue() == TR::iload && secondIU2L)))
            {
            lowSecondMR = generateS390MemoryReference(secondChild, _cg);
            delayedSecond = _cg->allocateRegister();
            if (!secondIU2L)
               {
               highSecondMR = generateS390MemoryReference(*lowSecondMR, 4, _cg);
               generateRXInstruction(_cg, TR::InstOpCode::L, root, delayedSecond, highSecondMR);
               }
            }
         else
            {
            secondRegister = _cg->evaluate(secondChild);
            }
         }
      if (getEvalChild1())
         {
         if (firstChild->getReferenceCount() == 1 &&
            firstChild->getRegister() == NULL &&
            (firstChild->getOpCodeValue() == TR::lload || (firstChild->getOpCodeValue() == TR::iload && firstIU2L)))
            {
            lowFirstMR = generateS390MemoryReference(firstChild, _cg);
            delayedFirst = _cg->allocateRegister();
            if (!firstIU2L)
               {
               highFirstMR = generateS390MemoryReference(*lowFirstMR, 4, _cg);
               generateRXInstruction(_cg, TR::InstOpCode::L, root, delayedFirst, highFirstMR);
               }
            }
         else
            {
            firstRegister = _cg->evaluate(firstChild);
            }
         }
      }

   if (firstHighZero && firstRegister && firstRegister->getRegisterPair())
      {
      if (!useFirstHighOrder)
         {
         firstRegister = firstRegister->getLowOrder();
         }
      else
         {
         firstRegister = firstRegister->getHighOrder();
         }
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

   TR::MemoryReference * lowMR = NULL;
   TR::MemoryReference * highMR = NULL;
   TR::RegisterDependencyConditions * deps;
   bool hasGlobalDeps = ((root->getNumChildren() == 3) ? true : false);
   uint32_t numAdditionalRegDeps = hasGlobalDeps ? 5 : 7;

   if (getCmpReg1Reg2())
      {
      TR_ASSERT( delayedFirst == NULL && delayedSecond == NULL, "Bad combination in long ordered compare analyser\n");
      TR::Register * firstLow;

      if (hasGlobalDeps)
         {
         TR::Node * thirdChild = root->getChild(2);
         _cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(_cg, thirdChild, numAdditionalRegDeps);
         }
      else
         {
         deps = new (_cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numAdditionalRegDeps, _cg);
         }

      if (firstHighZero == false)
         {
         firstLow = firstRegister->getLowOrder();
         if (hasGlobalDeps)
            {
            deps->addPostConditionIfNotAlreadyInserted(firstRegister->getHighOrder(), TR::RealRegister::AssignAny);
            deps->addPostConditionIfNotAlreadyInserted(firstRegister->getLowOrder(), TR::RealRegister::AssignAny);
            }
         else
            {
            deps->addPostCondition(firstRegister->getHighOrder(), TR::RealRegister::AssignAny);
            deps->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::AssignAny);
            }
         }
      else
         {
         firstLow = firstRegister;
         deps->addPostConditionIfNotAlreadyInserted(firstLow, TR::RealRegister::AssignAny);
         TR_ASSERT(firstLow->getRegisterPair() == NULL,
            "TR_S390CompareAnalyser::longOrderedCompareAndBranchAnalyser: Using reg pair as single reg\n");
         }

      TR::Register * secondLow = NULL;
      if (secondHighZero == false)
         {
         secondLow = secondRegister->getLowOrder();
         if (hasGlobalDeps)
            {
            deps->addPostConditionIfNotAlreadyInserted(secondRegister->getHighOrder(), TR::RealRegister::AssignAny);
            deps->addPostConditionIfNotAlreadyInserted(secondRegister->getLowOrder(), TR::RealRegister::AssignAny);
            }
         else
            {
            deps->addPostCondition(secondRegister->getHighOrder(), TR::RealRegister::AssignAny);
            deps->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::AssignAny);
            }
         }
      else
         {
         secondLow = secondRegister;
         deps->addPostConditionIfNotAlreadyInserted(secondLow, TR::RealRegister::AssignAny);
         TR_ASSERT(secondLow->getRegisterPair() == NULL,
            "TR_S390CompareAnalyser::longOrderedCompareAndBranchAnalyser: Using reg pair as single reg\n");
         }

      if (firstHighZero)
         {
         if (secondHighZero == false)   // if both are ui2l, then we just need an unsigned low order compare
            {
            generateRRInstruction(_cg, TR::InstOpCode::LTR, root, secondRegister->getHighOrder(), secondRegister->getHighOrder());
            }
         }
      else if (secondHighZero)
         {
         generateRRInstruction(_cg, TR::InstOpCode::LTR, root, firstRegister->getHighOrder(), firstRegister->getHighOrder());
         }
      else
         {
         if (isUnsignedCmp)
            {
            generateRRInstruction(_cg, TR::InstOpCode::CLR, root, firstRegister->getHighOrder(), secondRegister->getHighOrder());
            }
         else
            {
            generateRRInstruction(_cg, TR::InstOpCode::CR, root, firstRegister->getHighOrder(), secondRegister->getHighOrder());
            }
         }

      if (firstHighZero == false || secondHighZero == false)
         {
         TR::LabelSymbol * cflowRegionStart = generateLabelSymbol(_cg);
         cflowRegionStart->setStartInternalControlFlow();
         internalControlFlowStarted = true;
         generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, root, cflowRegionStart);

         // See if we can prove results as false
         if (brCmpHighFalseCond != TR::InstOpCode::COND_NOP)
            {
            generateS390BranchInstruction(_cg, branchOp, brCmpHighFalseCond, root, falseTarget);
            }
         // See if we can prove the result as true
         if (brCmpHighTrueCond != TR::InstOpCode::COND_NOP)
            {
            generateS390BranchInstruction(_cg, branchOp, brCmpHighTrueCond, root, trueTarget);
            }
         }
      generateRRInstruction(_cg, TR::InstOpCode::CLR, root, firstLow, secondLow);
      }
   ///// Reg + Mem  /////////////////////////////////////////
   else if (getCmpReg1Mem2())
      {
      // If 2nd child is a TR::iu2l, then the memory contents is only an int and should be loaded into lowMR.
      // The highword is just zero.
      if (secondIU2L)
         {
          highMR = NULL;
          lowMR = generateS390MemoryReference(secondChild, _cg);
         }
      else
         {
         highMR = generateS390MemoryReference(secondChild, _cg);
         lowMR = generateS390MemoryReference(*highMR, 4, _cg);
         }
      numAdditionalRegDeps += 2;

      if (hasGlobalDeps)
         {
         TR::Node * thirdChild = root->getChild(2);
         _cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(_cg, thirdChild, numAdditionalRegDeps);
         }
      else
         {
         deps = new (_cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numAdditionalRegDeps, _cg);
         }
      if (highMR != NULL)
         deps->addAssignAnyPostCondOnMemRef(highMR);

      TR::Register * firstLow;
      if (delayedFirst)
         {
         firstLow = NULL;
         deps->addPostConditionIfNotAlreadyInserted(delayedFirst, TR::RealRegister::AssignAny);
         }
      else if (firstHighZero == false)
         {
         firstLow = firstRegister->getLowOrder();
         if (hasGlobalDeps)
            {
            deps->addPostConditionIfNotAlreadyInserted(firstRegister->getHighOrder(), TR::RealRegister::AssignAny);
            deps->addPostConditionIfNotAlreadyInserted(firstRegister->getLowOrder(), TR::RealRegister::AssignAny);
            }
         else
            {
            deps->addPostCondition(firstRegister->getHighOrder(), TR::RealRegister::AssignAny);
            deps->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::AssignAny);
            }
         }
      else
         {
         firstLow = firstRegister;
         deps->addPostConditionIfNotAlreadyInserted(firstLow, TR::RealRegister::AssignAny);
         }

      if (secondIU2L)
         {
         generateRRInstruction(_cg, TR::InstOpCode::LTR, root, firstRegister->getHighOrder(), firstRegister->getHighOrder());
         }
      else
         {
         TR::Register * temp = NULL;
         if (delayedFirst)
            {
            temp = delayedFirst;
            }
         else
            {
            temp = firstRegister->getHighOrder();
            }
         if (temp != NULL)
            {
            if (isUnsignedCmp)
               {
               generateRXInstruction(_cg, TR::InstOpCode::CL, firstChild, temp, highMR);
               }
            else
               {
               generateRXInstruction(_cg, TR::InstOpCode::C, firstChild, temp, highMR);
               }
            }
         else
            {
            // if NULL, no need to compare the highOrder
            brCmpHighFalseCond = TR::InstOpCode::COND_NOP;
            brCmpHighTrueCond  = TR::InstOpCode::COND_NOP;
            }
         }

      if (firstHighZero == false || secondHighZero == false)
         {
         TR::LabelSymbol * cflowRegionStart = generateLabelSymbol(_cg);
         cflowRegionStart->setStartInternalControlFlow();
         internalControlFlowStarted = true;
         generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, root, cflowRegionStart);

         // See if we can prove results as false
         if (brCmpHighFalseCond != TR::InstOpCode::COND_NOP)
            {
            generateS390BranchInstruction(_cg, branchOp, brCmpHighFalseCond, root, falseTarget);
            }
         // See if we can prove the result as true
         if (brCmpHighTrueCond != TR::InstOpCode::COND_NOP)
            {
            generateS390BranchInstruction(_cg, branchOp, brCmpHighTrueCond, root, trueTarget);
            }
         }

      if (delayedFirst)
         {
         generateRXInstruction(_cg, TR::InstOpCode::L, firstChild, delayedFirst, lowFirstMR);
         firstLow = delayedFirst;
         }
      generateRXInstruction(_cg, TR::InstOpCode::CL, firstChild, firstLow, lowMR);
      }
   ///////  Mem + Reg  ///////////////////////////////////////
   else // Must be CmpMem1Reg2
      {
      TR_ASSERT( 0, "TR_S390CompareAnalyser::longOrderedCompareAndBranchAnalyser: Unchecked code");

      // If 1st child is a TR::iu2l, then the memory contents is only an int and should be loaded into lowMR.
      // The highword is just zero.
      if (firstIU2L)
         {
          highMR = NULL;
          lowMR = generateS390MemoryReference(firstChild, _cg);
         }
      else
         {
         highMR = generateS390MemoryReference(firstChild, _cg);
         lowMR = generateS390MemoryReference(*highMR, 4, _cg);
         }

      numAdditionalRegDeps += 2;
      if (hasGlobalDeps)
         {
         TR::Node * thirdChild = root->getChild(2);
         _cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(_cg, thirdChild, numAdditionalRegDeps);
         }
      else
         {
         deps = new (_cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numAdditionalRegDeps, _cg);
         }

      if (highMR != NULL)
         deps->addAssignAnyPostCondOnMemRef(highMR);

      TR::Register * secondLow;
      if (delayedSecond)
         {
         deps->addPostConditionIfNotAlreadyInserted(delayedSecond, TR::RealRegister::AssignAny);
         secondLow = NULL;
         }
      else if (secondHighZero == false)
         {
         deps->addPostConditionIfNotAlreadyInserted(secondRegister->getHighOrder(), TR::RealRegister::AssignAny);
         secondLow = secondRegister->getLowOrder();
         }
      else
         {
         secondLow = secondRegister;
         }
      if (secondLow)
         {
         deps->addPostConditionIfNotAlreadyInserted(secondLow, TR::RealRegister::AssignAny);
         }

      if (firstIU2L)
         {
         if (secondHighZero == false)   // if both are ui2l, then we just need an unsigned low order compare
            {
            generateRRInstruction(_cg, TR::InstOpCode::LTR, root, secondRegister->getHighOrder(), secondRegister->getHighOrder());
            }
         }
      else
         {
         TR::Register * temp;
         if (delayedSecond)
            {
            temp = delayedSecond;
            }
         else
            {
            temp = secondRegister->getHighOrder();
            }
         if (isUnsignedCmp)
            {
            generateRXInstruction(_cg, TR::InstOpCode::CL, firstChild, temp, highMR);
            }
         else
            {
            generateRXInstruction(_cg, TR::InstOpCode::C, firstChild, temp, highMR);
            }
         }

      if (firstHighZero == false || secondHighZero == false)
         {
         TR::LabelSymbol * cflowRegionStart = generateLabelSymbol(_cg);
         cflowRegionStart->setStartInternalControlFlow();
         internalControlFlowStarted = true;
         generateS390LabelInstruction(_cg, TR::InstOpCode::LABEL, root, cflowRegionStart);

         // See if we can prove results as false
         if (brCmpHighFalseCond != TR::InstOpCode::COND_NOP)
            {
            generateS390BranchInstruction(_cg, branchOp, brCmpHighTrueCond, root, falseTarget);
            }
         // See if we can prove the result as true
         if (brCmpHighTrueCond != TR::InstOpCode::COND_NOP)
            {
            generateS390BranchInstruction(_cg, branchOp, brCmpHighFalseCond, root, trueTarget);
            }
         }

      if (delayedSecond)
         {
         generateRXInstruction(_cg, TR::InstOpCode::L, firstChild, delayedSecond, lowSecondMR);
         secondLow = delayedSecond;
         }

      generateRXInstruction(_cg, TR::InstOpCode::CL, root, secondLow, lowMR);
      }

   // Perform required branch on compare of low values
   if (brCmpLowTrueCond != TR::InstOpCode::COND_NOP)
      {
      generateS390BranchInstruction(_cg, branchOp, brCmpLowTrueCond, root, trueTarget);
      }

   if (delayedFirst)
      {
      _cg->stopUsingRegister(delayedFirst);
      }

   if (delayedSecond)
      {
      _cg->stopUsingRegister(delayedSecond);
      }


   if (hasGlobalDeps)
      {
      TR::Node * thirdChild = root->getChild(2);
      _cg->decReferenceCount(thirdChild);
      }

   _cg->decReferenceCount(firstChild);

   if (root->getFirstChild() != firstChild)  // May have skipped original first child earlier (i.e. if 1st child is iu2l)
      _cg->decReferenceCount(root->getFirstChild());

   _cg->decReferenceCount(secondChild);

   if (root->getSecondChild() != secondChild)
      _cg->decReferenceCount(root->getSecondChild());

   if (lowFirstMR)
      {
      lowFirstMR->stopUsingMemRefRegister(_cg);
      }
   if (highFirstMR)
      {
      highFirstMR->stopUsingMemRefRegister(_cg);
      }
   if (lowSecondMR)
      {
      lowSecondMR->stopUsingMemRefRegister(_cg);
      }
   if (highSecondMR)
      {
      highSecondMR->stopUsingMemRefRegister(_cg);
      }
   if (lowMR != NULL)
      {
      lowMR->stopUsingMemRefRegister(_cg);
      }
   if (highMR != NULL)
      {
      highMR->stopUsingMemRefRegister(_cg);
      }


   return deps;
   }

void
TR_S390CompareAnalyser::longEqualityCompareAndBranchAnalyser(TR::Node * root, TR::LabelSymbol * firstBranchLabel,
   TR::LabelSymbol * secondBranchLabel, TR::InstOpCode::Mnemonic  secondBranchOp)
   {
   TR_ASSERT( 0, "TR_S390CompareAnalyser::longEqualityCompareAndBranchAnalyser: Not implemented");
   return;
   }


TR::Register *
TR_S390CompareAnalyser::longEqualityBooleanAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic setOpCode, TR::InstOpCode::Mnemonic combineOpCode)
   {
   TR_ASSERT( 0, "TR_S390CompareAnalyser::longEqualityBooleanAnalyser: Not implemented");
   return NULL;
   }

TR::Register *
TR_S390CompareAnalyser::longOrderedBooleanAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic highSetOpCode, TR::InstOpCode::Mnemonic lowSetOpCode)
   {
   TR_ASSERT( 0,"TR_S390CompareAnalyser::longOrderedBooleanAnalyser: Not implemented");
   return NULL;
   }

TR::Register *
TR_S390CompareAnalyser::longCMPAnalyser(TR::Node * root)
   {
   TR_ASSERT( 0, "TR_S390CompareAnalyser::longCMPAnalyser: Not implemented");
   return NULL;
   }

const uint8_t TR_S390CompareAnalyser::_actionMap[NUM_ACTIONS] =
   {
   // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   EvalChild1 |        //  0    0     0    0    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     0    0    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     0    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  0    0     0    0    1     1
   CmpReg1Mem2, EvalChild1 |        //  0    0     0    1    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     0    1    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     0    1    1     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     0    1    1     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     1    0    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     1    0    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     1    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  0    0     1    0    1     1
   CmpReg1Mem2, EvalChild1 |        //  0    0     1    1    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     1    1    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     1    1    1     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  0    0     1    1    1     1
   EvalChild2 | CmpReg1Reg2, EvalChild2 |        //  0    1     0    0    0     0
   CmpMem1Reg2, EvalChild2 |        //  0    1     0    0    0     1
   CmpMem1Reg2, EvalChild1 |        //  0    1     0    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  0    1     0    0    1     1
   CmpReg1Mem2, EvalChild2 |        //  0    1     0    1    0     0
   CmpMem1Reg2, EvalChild2 |        //  0    1     0    1    0     1
   CmpMem1Reg2, EvalChild2 |        //  0    1     0    1    1     0
   CmpMem1Reg2, EvalChild2 |        //  0    1     0    1    1     1
   CmpMem1Reg2, EvalChild2 |        //  0    1     1    0    0     0
   CmpMem1Reg2, EvalChild2 |        //  0    1     1    0    0     1
   CmpMem1Reg2, EvalChild1 |        //  0    1     1    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  0    1     1    0    1     1
   CmpReg1Mem2, EvalChild2 |        //  0    1     1    1    0     0
   CmpMem1Reg2, EvalChild2 |        //  0    1     1    1    0     1
   CmpMem1Reg2, EvalChild2 |        //  0    1     1    1    1     0
   CmpMem1Reg2, EvalChild2 |        //  0    1     1    1    1     1
   CmpMem1Reg2, EvalChild1 |        //  1    0     0    0    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     0    0    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     0    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  1    0     0    0    1     1
   CmpReg1Mem2, EvalChild1 |        //  1    0     0    1    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     0    1    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     0    1    1     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     0    1    1     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     1    0    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     1    0    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     1    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  1    0     1    0    1     1
   CmpReg1Mem2, EvalChild1 |        //  1    0     1    1    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     1    1    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     1    1    1     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    0     1    1    1     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     0    0    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     0    0    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     0    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  1    1     0    0    1     1
   CmpReg1Mem2, EvalChild1 |        //  1    1     0    1    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     0    1    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     0    1    1     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     0    1    1     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     1    0    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     1    0    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     1    0    1     0
   CmpReg1Mem2, EvalChild1 |        //  1    1     1    0    1     1
   CmpReg1Mem2, EvalChild1 |        //  1    1     1    1    0     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     1    1    0     1
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     1    1    1     0
   EvalChild2 | CmpReg1Reg2, EvalChild1 |        //  1    1     1    1    1     1
   EvalChild2 | CmpReg1Reg2
   };
