/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "z/codegen/BinaryAnalyser.hpp"

#include <stddef.h>                                // for NULL
#include <stdint.h>                                // for uint8_t
#include "codegen/Analyser.hpp"                    // for NUM_ACTIONS
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                    // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/MemoryReference.hpp"             // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                // for RegisterPair
#include "codegen/TreeEvaluator.hpp"
#include "codegen/S390Evaluator.hpp"               // for TR_S390ComputeCC
#include "compile/Compilation.hpp"                 // for Compilation, comp
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"                        // for ILOpCodes::lusubb, etc
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "z/codegen/S390GenerateInstructions.hpp"

namespace TR { class Instruction; }

#define   ENABLE_ZARCH_FOR_32    1

void
TR_S390BinaryAnalyser::remapInputs(TR::Node * firstChild, TR::Register * firstRegister,
                                    TR::Node * secondChild, TR::Register * secondRegister)
   {
   if (!cg()->useClobberEvaluate())
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
      if (cg()->canClobberNodesRegister(firstChild))
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
         if (cg()->canClobberNodesRegister(secondChild))
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
   TR::Compilation *comp = TR::comp();

   TR::SymbolReference * firstReference = firstChild->getOpCode().hasSymbolReference() ? firstChild->getSymbolReference() : NULL;
   TR::SymbolReference * secondReference = secondChild->getOpCode().hasSymbolReference() ? secondChild->getSymbolReference() : NULL;

   setInputs(firstChild, firstRegister, secondChild, secondRegister,
             false, false, comp,
             (cg()->isAddressOfStaticSymRefWithLockedReg(firstReference) ||
              cg()->isAddressOfPrivateStaticSymRefWithLockedReg(firstReference)),
             (cg()->isAddressOfStaticSymRefWithLockedReg(secondReference) ||
              cg()->isAddressOfPrivateStaticSymRefWithLockedReg(secondReference)));

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
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   remapInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (getCopyReg1())
      {
      TR::Register * thirdReg;
      bool done = false;

      if (firstRegister->getKind() == TR_GPR64)
         {
         thirdReg = cg()->allocate64bitRegister();
         }
      else if (firstRegister->getKind() == TR_VRF)
         {
         TR_ASSERT(false,"VRF: genericAnalyser unimplemented");
         }
      else if (firstRegister->getKind() != TR_FPR && firstRegister->getKind() != TR_VRF)
         {
         thirdReg = cg()->allocateRegister();
         }
      else
         {
         thirdReg = cg()->allocateRegister(TR_FPR);
         }

      if (cg()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
         {
         if (getBinaryReg3Reg2() || secondRegister != NULL)
            {
            if (regToRegOpCode == TR::InstOpCode::SR)
               {
               generateRRRInstruction(cg(), TR::InstOpCode::SRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            else if (regToRegOpCode == TR::InstOpCode::SLR)
               {
               generateRRRInstruction(cg(), TR::InstOpCode::SLRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            else if (regToRegOpCode == TR::InstOpCode::SGR)
               {
               generateRRRInstruction(cg(), TR::InstOpCode::SGRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            else if (regToRegOpCode == TR::InstOpCode::SLGR)
               {
               generateRRRInstruction(cg(), TR::InstOpCode::SLGRK, root, thirdReg, firstRegister, secondRegister);
               done = true;
               }
            }
         }

      if (!done)
         {
         generateRRInstruction(cg(), copyOpCode, root, thirdReg, firstRegister);
         if (getBinaryReg3Reg2() || (secondRegister != NULL))
            {
            generateRRInstruction(cg(), regToRegOpCode, root, thirdReg, secondRegister);
            }
         else
            {
            TR::Node* loadBaseAddr = is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild;
            TR::MemoryReference * tempMR = generateS390MemoryReference(loadBaseAddr, cg());

            //floating-point arithmatics don't have RXY format instructions, so no long displacement
            if (secondChild->getOpCode().isFloatingPoint())
               {
               tempMR->enforce4KDisplacementLimit(secondChild, cg(), NULL);
               }

            generateRXInstruction(cg(), memToRegOpCode, root, thirdReg, tempMR);
            tempMR->stopUsingMemRefRegister(cg());
            if (is16BitMemory2Operand)
               {
               cg()->decReferenceCount(secondChild->getFirstChild());
               }
            }
         }

      root->setRegister(thirdReg);
      }
   else if (getBinaryReg1Reg2())
      {
      generateRRInstruction(cg(), regToRegOpCode, root, firstRegister, secondRegister);
      root->setRegister(firstRegister);
      }
   else // assert getBinaryReg1Mem2() == true
      {
      TR_ASSERT(  !getInvalid(), "TR_S390BinaryAnalyser::invalid case\n");

      TR::MemoryReference * tempMR = generateS390MemoryReference(is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild, cg());
      //floating-point arithmatics don't have RXY format instructions, so no long displacement
      if (secondChild->getOpCode().isFloatingPoint())
         {
         tempMR->enforce4KDisplacementLimit(secondChild, cg(), NULL);
         }

      generateRXInstruction(cg(), memToRegOpCode, root, firstRegister, tempMR);
      tempMR->stopUsingMemRefRegister(cg());
      if (is16BitMemory2Operand)
         cg()->decReferenceCount(secondChild->getFirstChild());
      root->setRegister(firstRegister);
      }

   cg()->decReferenceCount(firstChild);
   cg()->decReferenceCount(secondChild);

   return;
   }



void
TR_S390BinaryAnalyser::longSubtractAnalyser(TR::Node * root)
   {
   TR::Node * firstChild;
   TR::Node * secondChild;
   TR::Instruction * cursor = NULL;
   TR::RegisterDependencyConditions * dependencies = NULL;
   bool setsOrReadsCC = NEED_CC(root) || (root->getOpCodeValue() == TR::lusubb);
   TR::InstOpCode::Mnemonic regToRegOpCode;
   TR::InstOpCode::Mnemonic memToRegOpCode;
   TR::Compilation *comp = TR::comp();

   if (TR::Compiler->target.is64Bit() || cg()->use64BitRegsOn32Bit())
      {
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
      }
   else
      {
      regToRegOpCode = TR::InstOpCode::SLR;
      memToRegOpCode = TR::InstOpCode::SL;
      }

   firstChild = root->getFirstChild();
   secondChild = root->getSecondChild();
   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();

   setInputs(firstChild, firstRegister, secondChild, secondRegister,
             false, false, comp);

   /**  Attempt to use SGH to subtract halfword (64 <- 16).
    * The second child is a halfword from memory */
   bool is16BitMemory2Operand = false;
   if (TR::Compiler->target.cpu.getS390SupportsZNext() &&
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
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   remapInputs(firstChild, firstRegister, secondChild, secondRegister);

   if ((root->getOpCodeValue() == TR::lusubb) &&
       TR_S390ComputeCC::setCarryBorrow(root->getChild(2), false, cg()))
      {
      // use SLBGR rather than SLGR/SGR
      //     SLBG rather than SLG/SG
      // or
      // use SLBR rather than SLR
      //     SLB rather than SL
      bool uses64bit = TR::Compiler->target.is64Bit() || cg()->use64BitRegsOn32Bit();
      regToRegOpCode = uses64bit ? TR::InstOpCode::SLBGR : TR::InstOpCode::SLBR;
      memToRegOpCode = uses64bit ? TR::InstOpCode::SLBG  : TR::InstOpCode::SLB;
      }

   if (TR::Compiler->target.is64Bit() || cg()->use64BitRegsOn32Bit())
      {
      if (getCopyReg1())
         {
         TR::Register * thirdReg = cg()->allocate64bitRegister();

         root->setRegister(thirdReg);
         generateRRInstruction(cg(), TR::InstOpCode::LGR, root, thirdReg, firstRegister);
         if (getBinaryReg3Reg2())
            {
            generateRRInstruction(cg(), regToRegOpCode, root, thirdReg, secondRegister);
            }
         else // assert getBinaryReg3Mem2() == true
            {
            TR::MemoryReference * longMR = generateS390MemoryReference(secondChild, cg());

            generateRXInstruction(cg(), memToRegOpCode, root, thirdReg, longMR);
            longMR->stopUsingMemRefRegister(cg());
            }
         }
      else if (getBinaryReg1Reg2())
         {
         generateRRInstruction(cg(), regToRegOpCode, root, firstRegister, secondRegister);

         root->setRegister(firstRegister);
         }
      else // assert getBinaryReg1Mem2() == true
         {
         TR_ASSERT(  !getInvalid(), "TR_S390BinaryAnalyser::invalid case\n");

         TR::Node* baseAddrNode = is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild;
         TR::MemoryReference * longMR = generateS390MemoryReference(baseAddrNode, cg());

         generateRXInstruction(cg(), memToRegOpCode, root, firstRegister, longMR);

         longMR->stopUsingMemRefRegister(cg());
         root->setRegister(firstRegister);

         if(is16BitMemory2Operand)
            {
            cg()->decReferenceCount(secondChild->getFirstChild());
            }
         }

      }
   else    // if 32bit codegen...
      {
      bool zArchTrexsupported = performTransformation(comp, "O^O Use SL/SLB for long sub.");

      TR::Register * highDiff = NULL;
      TR::LabelSymbol * doneLSub = TR::LabelSymbol::create(cg()->trHeapMemory(),cg());
      if (getCopyReg1())
         {
         TR::Register * lowThird = cg()->allocateRegister();
         TR::Register * highThird = cg()->allocateRegister();

         TR::RegisterPair * thirdReg = cg()->allocateConsecutiveRegisterPair(lowThird, highThird);

         highDiff = highThird;

         dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 9, cg());
         dependencies->addPostCondition(firstRegister, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(firstRegister->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::LegalOddOfPair);

         // If 2nd operand has ref count of 1 and can be accessed by a memory reference,
         // then second register will not be used.
         if(secondRegister == firstRegister && !setsOrReadsCC)
            {
            TR_ASSERT( false, "lsub with identical children - fix Simplifier");
            }
         if (secondRegister != NULL && firstRegister != secondRegister)
            {
            dependencies->addPostCondition(secondRegister, TR::RealRegister::EvenOddPair);
            dependencies->addPostCondition(secondRegister->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
            dependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::LegalOddOfPair);
            }
         dependencies->addPostCondition(highThird, TR::RealRegister::AssignAny);

         root->setRegister(thirdReg);
         generateRRInstruction(cg(), TR::InstOpCode::LR, root, highThird, firstRegister->getHighOrder());
         generateRRInstruction(cg(), TR::InstOpCode::LR, root, lowThird, firstRegister->getLowOrder());
         if (getBinaryReg3Reg2())
            {
            if ((ENABLE_ZARCH_FOR_32 && zArchTrexsupported) || setsOrReadsCC)
               {
               generateRRInstruction(cg(), regToRegOpCode, root, lowThird, secondRegister->getLowOrder());
               generateRRInstruction(cg(), TR::InstOpCode::SLBR, root, highThird, secondRegister->getHighOrder());
               }
            else
               {
               generateRRInstruction(cg(), TR::InstOpCode::SR, root, highThird, secondRegister->getHighOrder());
               generateRRInstruction(cg(), TR::InstOpCode::SLR, root, lowThird, secondRegister->getLowOrder());
               }
            }
         else // assert getBinaryReg3Mem2() == true
            {
            TR::MemoryReference * highMR = generateS390MemoryReference(secondChild, cg());
            TR::MemoryReference * lowMR = generateS390MemoryReference(*highMR, 4, cg());
            dependencies->addAssignAnyPostCondOnMemRef(highMR);

            if ((ENABLE_ZARCH_FOR_32 && zArchTrexsupported) || setsOrReadsCC)
               {
               generateRXInstruction(cg(), memToRegOpCode, root, lowThird, lowMR);
               generateRXInstruction(cg(), TR::InstOpCode::SLB, root, highThird, highMR);
               }
            else
               {
               generateRXInstruction(cg(), TR::InstOpCode::S, root, highThird, highMR);
               generateRXInstruction(cg(), TR::InstOpCode::SL, root, lowThird, lowMR);
               }
            highMR->stopUsingMemRefRegister(cg());
            lowMR->stopUsingMemRefRegister(cg());
            }
         }
      else if (getBinaryReg1Reg2())
         {
         dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg());
         dependencies->addPostCondition(firstRegister, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(firstRegister->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::LegalOddOfPair);

         if(secondRegister == firstRegister)
            {
            TR_ASSERT( false, "lsub with identical children - fix Simplifier");
            }

         if (secondRegister != firstRegister)
            {
            dependencies->addPostCondition(secondRegister, TR::RealRegister::EvenOddPair);
            dependencies->addPostCondition(secondRegister->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
            dependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::LegalOddOfPair);
            }

         if ((ENABLE_ZARCH_FOR_32 && zArchTrexsupported) || setsOrReadsCC)
            {
            generateRRInstruction(cg(), regToRegOpCode, root, firstRegister->getLowOrder(), secondRegister->getLowOrder());
            generateRRInstruction(cg(), TR::InstOpCode::SLBR, root, firstRegister->getHighOrder(), secondRegister->getHighOrder());
            }
         else
            {
            generateRRInstruction(cg(), TR::InstOpCode::SR, root, firstRegister->getHighOrder(), secondRegister->getHighOrder());
            generateRRInstruction(cg(), TR::InstOpCode::SLR, root, firstRegister->getLowOrder(), secondRegister->getLowOrder());
            }

         highDiff = firstRegister->getHighOrder();
         root->setRegister(firstRegister);
         }
      else // assert getBinaryReg1Mem2() == true
         {
         TR_ASSERT(  !getInvalid(),"TR_S390BinaryAnalyser::invalid case\n");

         dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg());
         dependencies->addPostCondition(firstRegister, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(firstRegister->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::LegalOddOfPair);

         TR::MemoryReference * highMR = generateS390MemoryReference(secondChild, cg());
         TR::MemoryReference * lowMR = generateS390MemoryReference(*highMR, 4, cg());
         dependencies->addAssignAnyPostCondOnMemRef(highMR);

         if ((ENABLE_ZARCH_FOR_32 && zArchTrexsupported) || setsOrReadsCC)
            {
            generateRXInstruction(cg(), memToRegOpCode, root, firstRegister->getLowOrder(), lowMR);
            generateRXInstruction(cg(), TR::InstOpCode::SLB, root, firstRegister->getHighOrder(), highMR);
            }
         else
            {
            generateRXInstruction(cg(), TR::InstOpCode::S, root, firstRegister->getHighOrder(), highMR);
            generateRXInstruction(cg(), TR::InstOpCode::SL, root, firstRegister->getLowOrder(), lowMR);
            }
         highDiff = firstRegister->getHighOrder();
         root->setRegister(firstRegister);
         highMR->stopUsingMemRefRegister(cg());
         lowMR->stopUsingMemRefRegister(cg());
         }

      if (!((ENABLE_ZARCH_FOR_32 && zArchTrexsupported) || setsOrReadsCC))
         {
         // Check for overflow in LS int. If overflow, we are done.
         generateS390BranchInstruction(cg(), TR::InstOpCode::BRC,TR::InstOpCode::COND_MASK3, root, doneLSub);

         // Increment MS int due to overflow in LS int
         generateRIInstruction(cg(), TR::InstOpCode::AHI, root, highDiff, -1);

         generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, root, doneLSub, dependencies);
         }
      }

   cg()->decReferenceCount(firstChild);
   cg()->decReferenceCount(secondChild);

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
