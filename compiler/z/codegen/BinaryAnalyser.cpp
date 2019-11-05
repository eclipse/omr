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

#include "z/codegen/BinaryAnalyser.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/Analyser.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/S390Evaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"

namespace TR { class Instruction; }

void
TR_S390BinaryAnalyser::remapInputs(TR::Node * firstChild, TR::Register * firstRegister,
                                    TR::Node * secondChild, TR::Register * secondRegister)
   {
   TR::CodeGenerator * cg_ = cg();
   if (!cg_->useClobberEvaluate())
      {
      if (firstRegister)
         {
         setReg1();
         }
      if (secondRegister)
         {
         setReg2();
         }

      // firstRegister is always assumed
      if (cg_->canClobberNodesRegister(firstChild))
         {
         setClob1();
         }
      else
         {
         resetClob1();
         }

      // don't touch secondChild memory refs
      if (!getMem2())
         {
         if (cg_->canClobberNodesRegister(secondChild))
            {
            setClob2();
            }
         else
            {
            resetClob2();
            }
         }
      }
   }

void
TR_S390BinaryAnalyser::genericAnalyser(TR::Node * root,
                                       TR::InstOpCode::Mnemonic regToRegOpCode,
                                       TR::InstOpCode::Mnemonic memToRegOpCode,
                                       TR::InstOpCode::Mnemonic copyOpCode)
   {
   TR::Node * firstChild;
   TR::Node * secondChild;
   firstChild = root->getFirstChild();
   secondChild = root->getSecondChild();
   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();
   TR::CodeGenerator * cg_ = cg();
   TR::Compilation *comp = cg_->comp();

   setInputs(firstChild, firstRegister, secondChild, secondRegister, false, false, comp, false, false);

   /*
    * Check if SH or CH can be used to evaluate this integer subtract/compare node.
    * The second operand of SH/CH is a 16-bit number from memory. And using
    * these directly can save a load instruction.
    */
   bool is16BitMemory2Operand = false;
   if (secondChild->getOpCodeValue() == TR::s2i &&
       secondChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
       secondChild->isSingleRefUnevaluated() &&
       secondChild->getFirstChild()->isSingleRefUnevaluated())
      {
      bool supported = true;

      if (memToRegOpCode == TR::InstOpCode::S)
         {
         memToRegOpCode = TR::InstOpCode::SH;
         }
      else if (memToRegOpCode == TR::InstOpCode::C)
         {
         memToRegOpCode = TR::InstOpCode::CH;
         }
      else
         {
         supported = false;
         }

      if (supported)
         {
         setMem2();
         is16BitMemory2Operand = true;
         }
      }

   if (getEvalChild1())
      {
      firstRegister = cg_->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg_->evaluate(secondChild);
      }

   remapInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (getCopyReg1())
      {
      TR::Register* thirdReg = NULL;

      switch (firstRegister->getKind())
         {
         case TR_RegisterKinds::TR_GPR:
         case TR_RegisterKinds::TR_FPR:
            {
            thirdReg = cg_->allocateRegister(firstRegister->getKind());
            break;
            }

         default:
            {
            TR_ASSERT_FATAL(false, "Generic binary analyser for register kind (%d) is unimplemented", firstRegister->getKind());
            break;
            }
         }

      bool done = false;

      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
         {
         if (getBinaryReg3Reg2() || secondRegister != NULL)
            {
            if (regToRegOpCode == TR::InstOpCode::SR)
               {
               generateRRRInstruction(cg_, TR::InstOpCode::SRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            else if (regToRegOpCode == TR::InstOpCode::SLR)
               {
               generateRRRInstruction(cg_, TR::InstOpCode::SLRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            else if (regToRegOpCode == TR::InstOpCode::SGR)
               {
               generateRRRInstruction(cg_, TR::InstOpCode::SGRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            else if (regToRegOpCode == TR::InstOpCode::SLGR)
               {
               generateRRRInstruction(cg_, TR::InstOpCode::SLGRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            }
         }

      if (!done)
         {
         generateRRInstruction(cg_, copyOpCode, root, thirdReg, firstRegister);
         if (getBinaryReg3Reg2() || (secondRegister != NULL))
            {
            generateRRInstruction(cg_, regToRegOpCode, root, thirdReg, secondRegister);
            }
         else
            {
            TR::Node* loadBaseAddr = is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild;
            TR::MemoryReference * tempMR = generateS390MemoryReference(loadBaseAddr, cg_);

            //floating-point arithmatics don't have RXY format instructions, so no long displacement
            if (secondChild->getOpCode().isFloatingPoint())
               {
               tempMR->enforce4KDisplacementLimit(secondChild, cg_, NULL);
               }

            auto instructionFormat = TR::InstOpCode(memToRegOpCode).getInstructionFormat();

            if (instructionFormat == RXE_FORMAT)
               {
               generateRXEInstruction(cg_, memToRegOpCode, root, thirdReg, tempMR, 0);
               }
            else
               {
               generateRXInstruction(cg_, memToRegOpCode, root, thirdReg, tempMR);
               }

            tempMR->stopUsingMemRefRegister(cg_);
            if (is16BitMemory2Operand)
               {
               cg_->decReferenceCount(secondChild->getFirstChild());
               }
            }
         }

      root->setRegister(thirdReg);
      }
   else if (getBinaryReg1Reg2())
      {
      generateRRInstruction(cg_, regToRegOpCode, root, firstRegister, secondRegister);
      root->setRegister(firstRegister);
      }
   else // assert getBinaryReg1Mem2() == true
      {
      TR_ASSERT(  !getInvalid(), "TR_S390BinaryAnalyser::invalid case\n");

      TR::MemoryReference * tempMR = generateS390MemoryReference(is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild, cg_);
      //floating-point arithmatics don't have RXY format instructions, so no long displacement
      if (secondChild->getOpCode().isFloatingPoint())
         {
         tempMR->enforce4KDisplacementLimit(secondChild, cg_, NULL);
         }

      auto instructionFormat = TR::InstOpCode(memToRegOpCode).getInstructionFormat();

      if (instructionFormat == RXE_FORMAT)
         {
         generateRXEInstruction(cg_, memToRegOpCode, root, firstRegister, tempMR, 0);
         }
      else
         {
         generateRXInstruction(cg_, memToRegOpCode, root, firstRegister, tempMR);
         }

      tempMR->stopUsingMemRefRegister(cg_);
      if (is16BitMemory2Operand)
         cg_->decReferenceCount(secondChild->getFirstChild());
      root->setRegister(firstRegister);
      }

   cg_->decReferenceCount(firstChild);
   cg_->decReferenceCount(secondChild);

   return;
   }



void
TR_S390BinaryAnalyser::longSubtractAnalyser(TR::Node * root)
   {
   TR::Instruction * cursor = NULL;
   TR::RegisterDependencyConditions * dependencies = NULL;
   bool setsOrReadsCC = NEED_CC(root) || (root->getOpCodeValue() == TR::lusubb);
   TR::InstOpCode::Mnemonic regToRegOpCode;
   TR::InstOpCode::Mnemonic memToRegOpCode;
   TR::CodeGenerator * cg_ = cg();
   TR::Compilation *comp = cg_->comp();

   if (!setsOrReadsCC)
      {
      regToRegOpCode = TR::InstOpCode::SGR;
      memToRegOpCode = TR::InstOpCode::SG;
      }
   else
      {
      regToRegOpCode = TR::InstOpCode::SLGR;
      memToRegOpCode = TR::InstOpCode::SLG;
      }

   TR::Node* firstChild = root->getFirstChild();
   TR::Node* secondChild = root->getSecondChild();
   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();

   setInputs(firstChild, firstRegister, secondChild, secondRegister,
             false, false, comp);

   /**  Attempt to use SGH to subtract halfword (64 <- 16).
    * The second child is a halfword from memory */
   bool is16BitMemory2Operand = false;
   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z14) &&
       secondChild->getOpCodeValue() == TR::s2l &&
       secondChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
       secondChild->isSingleRefUnevaluated() &&
       secondChild->getFirstChild()->isSingleRefUnevaluated())
      {
      setMem2();
      memToRegOpCode = TR::InstOpCode::SGH;
      is16BitMemory2Operand = true;
      }

   if (getEvalChild1())
      {
      firstRegister = cg_->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg_->evaluate(secondChild);
      }

   remapInputs(firstChild, firstRegister, secondChild, secondRegister);

   if ((root->getOpCodeValue() == TR::lusubb) &&
       TR_S390ComputeCC::setCarryBorrow(root->getChild(2), false, cg_))
      {
      // Use SLBGR rather than SLGR/SGR or use SLBR rather than SLR
      regToRegOpCode = TR::InstOpCode::SLBGR;
      memToRegOpCode = TR::InstOpCode::SLBG;
      }

   if (getCopyReg1())
      {
      TR::Register * thirdReg = cg_->allocateRegister();

      root->setRegister(thirdReg);
      generateRRInstruction(cg_, TR::InstOpCode::LGR, root, thirdReg, firstRegister);
      if (getBinaryReg3Reg2())
         {
         generateRRInstruction(cg_, regToRegOpCode, root, thirdReg, secondRegister);
         }
      else // assert getBinaryReg3Mem2() == true
         {
         TR::MemoryReference * longMR = generateS390MemoryReference(secondChild, cg_);

         generateRXInstruction(cg_, memToRegOpCode, root, thirdReg, longMR);
         longMR->stopUsingMemRefRegister(cg_);
         }
      }
   else if (getBinaryReg1Reg2())
      {
      generateRRInstruction(cg_, regToRegOpCode, root, firstRegister, secondRegister);

      root->setRegister(firstRegister);
      }
   else // assert getBinaryReg1Mem2() == true
      {
      TR_ASSERT(  !getInvalid(), "TR_S390BinaryAnalyser::invalid case\n");

      TR::Node* baseAddrNode = is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild;
      TR::MemoryReference * longMR = generateS390MemoryReference(baseAddrNode, cg_);

      generateRXInstruction(cg_, memToRegOpCode, root, firstRegister, longMR);

      longMR->stopUsingMemRefRegister(cg_);
      root->setRegister(firstRegister);

      if(is16BitMemory2Operand)
         {
         cg_->decReferenceCount(secondChild->getFirstChild());
         }
      }

   cg_->decReferenceCount(firstChild);
   cg_->decReferenceCount(secondChild);

   return;
   }


const uint8_t TR_S390BinaryAnalyser::actionMap[NUM_ACTIONS] =
   {
                                                               // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   EvalChild1 |                                                //  0    0     0    0    0     0
   EvalChild2 | CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    0     0    0    0     1
   EvalChild2 | CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    0     0    0    1     0
   CopyReg1 | BinaryReg3Mem2,
   EvalChild1 |                                                //  0    0     0    0    1     1
   CopyReg1 | BinaryReg3Mem2,
   EvalChild1 |                                                //  0    0     0    1    0     0
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    0     0    1    0     1
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    0     0    1    1     0
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    0     0    1    1     1
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    0     1    0    0     0
   EvalChild2 | BinaryReg1Reg2,
   EvalChild1 |                                                //  0    0     1    0    0     1
   EvalChild2 | BinaryReg1Reg2,
   EvalChild1 |                                                //  0    0     1    0    1     0
   BinaryReg1Mem2,
   EvalChild1 |                                                //  0    0     1    0    1     1
   BinaryReg1Mem2,
   EvalChild1 |                                                //  0    0     1    1    0     0
   BinaryReg1Reg2,
   EvalChild1 |                                                //  0    0     1    1    0     1
   BinaryReg1Reg2,
   EvalChild1 |                                                //  0    0     1    1    1     0
   BinaryReg1Reg2,
   EvalChild1 |                                                //  0    0     1    1    1     1
   BinaryReg1Reg2,
   EvalChild1 |                                                //  0    1     0    0    0     0
   EvalChild2 | CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    1     0    0    0     1
   EvalChild2 | CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    1     0    0    1     0
   CopyReg1 | BinaryReg3Mem2,
   EvalChild1 |                                                //  0    1     0    0    1     1
   CopyReg1 | BinaryReg3Mem2,
   EvalChild1 |                                                //  0    1     0    1    0     0
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    1     0    1    0     1
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    1     0    1    1     0
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    1     0    1    1     1
   CopyReg1 | BinaryReg3Reg2,
   EvalChild1 |                                                //  0    1     1    0    0     0
   EvalChild2 | BinaryReg1Reg2,
   EvalChild1 |                                                //  0    1     1    0    0     1
   EvalChild2 | BinaryReg1Reg2,
   EvalChild1 |                                                //  0    1     1    0    1     0
   BinaryReg1Mem2,
   EvalChild1 |                                                //  0    1     1    0    1     1
   BinaryReg1Mem2,
   EvalChild1 |                                                //  0    1     1    1    0     0
   BinaryReg1Reg2,
   EvalChild1 |                                                //  0    1     1    1    0     1
   BinaryReg1Reg2,
   EvalChild1 |                                                //  0    1     1    1    1     0
   BinaryReg1Reg2,
   EvalChild1 |                                                //  0    1     1    1    1     1
   BinaryReg1Reg2,
   EvalChild2 |                                                //  1    0     0    0    0     0
   CopyReg1 | BinaryReg3Reg2,
   EvalChild2 |                                                //  1    0     0    0    0     1
   CopyReg1 | BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    0     0    0    1     0
   BinaryReg3Mem2,
   CopyReg1 |                                                  //  1    0     0    0    1     1
   BinaryReg3Mem2,
   CopyReg1 |                                                  //  1    0     0    1    0     0
   BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    0     0    1    0     1
   BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    0     0    1    1     0
   BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    0     0    1    1     1
   BinaryReg3Reg2,
   EvalChild2 |                                                //  1    0     1    0    0     0
   BinaryReg1Reg2,
   EvalChild2 |                                                //  1    0     1    0    0     1
   BinaryReg1Reg2,
   EvalChild2 |                                                //  1    0     1    0    1     0
   BinaryReg1Reg2,
   BinaryReg1Mem2,                                             //  1    0     1    0    1     1

   BinaryReg1Reg2,                                             //  1    0     1    1    0     0

   BinaryReg1Reg2,                                             //  1    0     1    1    0     1

   BinaryReg1Reg2,                                             //  1    0     1    1    1     0

   BinaryReg1Reg2,                                             //  1    0     1    1    1     1

   EvalChild2 |                                                //  1    1     0    0    0     0
   CopyReg1 | BinaryReg3Reg2,
   EvalChild2 |                                                //  1    1     0    0    0     1
   CopyReg1 | BinaryReg3Reg2,
   EvalChild2 |                                                //  1    1     0    0    1     0
   CopyReg1 | BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    1     0    0    1     1
   BinaryReg3Mem2,
   CopyReg1 |                                                  //  1    1     0    1    0     0
   BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    1     0    1    0     1
   BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    1     0    1    1     0
   BinaryReg3Reg2,
   CopyReg1 |                                                  //  1    1     0    1    1     1
   BinaryReg3Reg2,
   EvalChild2 |                                                //  1    1     1    0    0     0
   BinaryReg1Reg2,
   EvalChild2 |                                                //  1    1     1    0    0     1
   BinaryReg1Reg2,
   EvalChild2 |                                                //  1    1     1    0    1     0
   BinaryReg1Reg2,
   BinaryReg1Mem2,                                             //  1    1     1    0    1     1

   BinaryReg1Reg2,                                             //  1    1     1    1    0     0

   BinaryReg1Reg2,                                             //  1    1     1    1    0     1

   BinaryReg1Reg2,                                             //  1    1     1    1    1     0

   BinaryReg1Reg2                                              //  1    1     1    1    1     1

   };

const uint8_t TR_S390BinaryAnalyser::clobEvalActionMap[NUM_ACTIONS] =
   {
                                       // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   InvalidBACEMap,			// 0 0 0 0 0 0 thru 0 1 1 1 1 1
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,
   InvalidBACEMap,			// 1 0 0 0 0 0
   InvalidBACEMap,	 		// 1 0 0 0 0 1
   CopyReg1 | BinaryReg3Mem2, 	// 1 0 0 0 1 0
   CopyReg1 | BinaryReg3Mem2, 	// 1 0 0 0 1 1
   CopyReg1 | BinaryReg3Reg2, 	// 1 0 0 1 0 0
   CopyReg1 | BinaryReg3Reg2, 	// 1 0 0 1 0 1
   CopyReg1 | BinaryReg3Reg2, 	// 1 0 0 1 1 0
   CopyReg1 | BinaryReg3Reg2, 	// 1 0 0 1 1 1
   InvalidBACEMap,			// 1 0 1 0 0 0
   InvalidBACEMap,			// 1 0 1 0 0 1
   BinaryReg1Mem2,		// 1 0 1 0 1 0
   BinaryReg1Mem2,		// 1 0 1 0 1 1
   BinaryReg1Reg2,		// 1 0 1 1 0 0
   BinaryReg1Reg2,		// 1 0 1 1 0 1
   BinaryReg1Reg2,		// 1 0 1 1 1 0
   BinaryReg1Reg2,		// 1 0 1 1 1 1
   InvalidBACEMap,			// 1 1 0 0 0 0
   InvalidBACEMap,			// 1 1 0 0 0 1
   CopyReg1 | BinaryReg3Mem2,		// 1 1 0 0 1 0
   CopyReg1 | BinaryReg3Mem2,		// 1 1 0 0 1 1
   CopyReg1 | BinaryReg3Reg2,		// 1 1 0 1 0 0
   CopyReg1 | BinaryReg3Reg2,		// 1 1 0 1 0 1
   CopyReg1 | BinaryReg3Reg2,		// 1 1 0 1 1 0
   CopyReg1 | BinaryReg3Reg2,  		// 1 1 0 1 1 1
   InvalidBACEMap,			// 1 1 1 0 0 0
   InvalidBACEMap,			// 1 1 1 0 0 1
   BinaryReg1Mem2,			// 1 1 1 0 1 0
   BinaryReg1Mem2,			// 1 1 1 0 1 1
   BinaryReg1Reg2,			// 1 1 1 1 0 0
   BinaryReg1Reg2,			// 1 1 1 1 0 1
   BinaryReg1Reg2,			// 1 1 1 1 1 0
   BinaryReg1Reg2			// 1 1 1 1 1 1
   };
