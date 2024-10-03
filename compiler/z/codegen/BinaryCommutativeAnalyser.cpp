/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include "z/codegen/BinaryCommutativeAnalyser.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/Analyser.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"

void
TR_S390BinaryCommutativeAnalyser::remapInputs(TR::Node * firstChild, TR::Register *firstRegister,
                                              TR::Node * secondChild, TR::Register *secondRegister,
                                              bool nonClobberingDestination)
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

      if (firstRegister)
         {
         if (cg()->canClobberNodesRegister(firstChild) || nonClobberingDestination)
            {
            setClob1();
            }
         else
            {
            resetClob1();
            }
         }

      if (secondRegister)
         {
         if (cg()->canClobberNodesRegister(secondChild) || nonClobberingDestination)
            {
            setClob2();
            }
         else
            {
            resetClob2();
            }
         }

      if (firstChild == secondChild &&
          firstChild->getReferenceCount() == 2 &&
          firstRegister &&
          cg()->isRegisterClobberable(firstRegister,1))
         {
         // allow clobbering of f1 when (d2f==firstChild)
         // fmul
         //    =>d2f (in f1) refCount=2
         //    =>d2f (in f1)
         //
         // however if the trees look like:
         // d2f   (f1 -- passThrough eval with no clobber evaluate)
         //    =>i2d (f1) refCount > 1
         // fmul
         //    =>d2f (in f1) refCount=2
         //    =>d2f (in f1)
         //
         // then it is not legal to clobber f1 because the commoned i2d requires the unclobbered f1 value
         // isRegisterClobberable(firstRegister,1) returns false in this latter case as firstRegister is attached to more than one unique node
         setClob1();
         setClob2();
         }
      }
   }

TR::Register *
TR_S390BinaryCommutativeAnalyser::allocateAddSubRegister(TR::Node * node, TR::Register * src1Reg)
   {
   TR::Register* trgReg = NULL;

   if (src1Reg->containsInternalPointer() || !src1Reg->containsCollectedReference())
      {
      trgReg = cg()->allocateRegister();

      if (src1Reg->containsInternalPointer())
         {
         trgReg->setPinningArrayPointer(src1Reg->getPinningArrayPointer());
         trgReg->setContainsInternalPointer();
         }
      }
   else
      {
      trgReg = cg()->allocateCollectedReferenceRegister();
      }

   return trgReg;
   }

void
TR_S390BinaryCommutativeAnalyser::genericAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic regToRegOpCode, TR::InstOpCode::Mnemonic memToRegOpCode,
   TR::InstOpCode::Mnemonic copyOpCode, bool nonClobberingDestination,
   TR::LabelSymbol *targetLabel, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond)
   {
   TR::Node * firstChild;
   TR::Node * secondChild;
   TR::Node * initFirstChild = NULL;
   TR::Node * initSecondChild = NULL;
   TR::Instruction * cursor = NULL;
   TR::Register * nodeReg = NULL;
   TR::Instruction * finalInstr = NULL;
   TR::Compilation *comp = cg()->comp();
   const char * CLOBBER_EVAL  = "LR=Clobber_eval";
   TR_Debug * debugObj = cg()->getDebug();
   if (cg()->whichChildToEvaluate(root) == 0)
      {
      firstChild = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      setReversedOperands(true);
      firstChild = root->getSecondChild();
      secondChild = root->getFirstChild();
      }

   //  On 64bit
   //  If we are doing l2i and only once, we can just get the value
   //  directly from the 64bit register.
   //  Typical example:
   //
   //     (0)  BNDCHK
   //     (1)    l2i [node >= 0] (in GPR_0063)
   //     (1)      ilload #155[00000000801188C8] Shadow[<array-size>]24
   //     (2)        iaload #177[000000008012C5E0] Shadow[java/lang/ThreadGroup.childrenGroups
   //     (1)          ==>aload at [000000008012DBE8] (in &GPR_0058)
   //     (1)    l2i (in GPR_0067)
   //     (2)      ==>i2l at [0000000080117A70]
   //  We need to be sure that the operation is non-clobbering to safely omit the l2i
   //
   bool adjustDiplacementOnFirstChild  = false;
   bool adjustDiplacementOnSecondChild = false;

   if (root->getOpCode().isBooleanCompare() || nonClobberingDestination)
      {
      if (firstChild->getOpCodeValue() == TR::bu2i &&
          firstChild->getReferenceCount() == 1    &&
          firstChild->getRegister() == NULL       &&
             (firstChild->getFirstChild()->getOpCodeValue() == TR::bload ||	
              firstChild->getFirstChild()->getOpCodeValue() == TR::bloadi  ))
          {
          initFirstChild = firstChild;

          firstChild = firstChild->getFirstChild();
          cg()->evaluate(firstChild);
          }

      if (secondChild->getOpCodeValue() == TR::bu2i &&
          secondChild->getReferenceCount() == 1    &&
          secondChild->getRegister() == NULL       &&
             (secondChild->getFirstChild()->getOpCodeValue() == TR::bload ||	
              secondChild->getFirstChild()->getOpCodeValue() == TR::bloadi  ))
          {
          initSecondChild = secondChild;

          secondChild = secondChild->getFirstChild();
          cg()->evaluate(secondChild);
          }
      }

   if (cg()->comp()->target().is64Bit())
      {
      if (firstChild->getOpCodeValue() == TR::l2i && firstChild->getReferenceCount() == 1 &&
          firstChild->getRegister() == NULL && nonClobberingDestination)
         {
         initFirstChild = firstChild;
         firstChild     = firstChild->getFirstChild();

         // We need to adjust the offset for a 32bit version of the memref
         adjustDiplacementOnFirstChild = true;
         }

      if (secondChild->getOpCodeValue() == TR::l2i && secondChild->getReferenceCount() == 1 &&
          secondChild->getRegister() == NULL && nonClobberingDestination)
         {
         initSecondChild = secondChild;
         secondChild     = secondChild->getFirstChild();


         // We need to adjust the offset for a 32bit version of the memref
         adjustDiplacementOnSecondChild = true;
         }
      }

   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();

   setInputs(firstChild, firstRegister, secondChild,
             secondRegister, nonClobberingDestination,
             false, comp);

   bool isLoadNodeNested = false;

   // All of the memToRegOpCodes (MGH, MSGC and MSC) used in the if block below
   // rely on the miscellaneous-instruction-extension facility 2 being installed

   // TODO: add MH and MHY here; outside of the z14 if check.
   if (cg()->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2))
      {
      bool isSetReg2Mem1 = false;

      if(root->getOpCodeValue() == TR::lmul)
         {
         if(firstChild->getOpCodeValue() == TR::s2l &&
                 firstChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
                 firstChild->getReferenceCount() == 1 &&
                 firstChild->getRegister() == NULL &&
                 firstChild->getFirstChild()->getReferenceCount() == 1 &&
                 firstChild->getFirstChild()->getRegister() == NULL)
            {
            /* Use MGH when 64<-64x16 and the multiplier is a short int in storage
             * AND it's never loaded before. */
            isLoadNodeNested = true;
            isSetReg2Mem1 = true;
            memToRegOpCode = TR::InstOpCode::MGH;
            }
         else if(firstChild->getOpCodeValue() == TR::lloadi &&
                 firstChild->getReferenceCount() == 1 &&
                 firstChild->getRegister() == NULL)
            {
            /* Use MSGC when 64<-64x64 and the multiplier is a long in storage
             * AND it's never loaded before */
            isSetReg2Mem1 = true;
            memToRegOpCode = TR::InstOpCode::MSGC;
            }
         }
      else if(root->getOpCodeValue() == TR::imul)
         {
         if(firstChild->getOpCodeValue() == TR::iloadi &&
                 firstChild->getReferenceCount() == 1 &&
                 firstChild->getRegister() == NULL)
            {
             /* Use MSC when 32<-32x32 and the multiplier is an int in storage
              * AND it's never loaded before */
             isSetReg2Mem1 = true;
             memToRegOpCode = TR::InstOpCode::MSC;
            }
         }

      if(isSetReg2Mem1)
         {
         resetReg1();
         setMem1();
         setClob1();

         resetReg2();
         resetMem2();
         setClob2();
         }
      }

   if ((root->getSize()==1 ||
        root->getSize()==2) &&
        root->getOpCode().isBitwiseLogical())
      {
      resetMem1();
      resetMem2();
      }

   if (root->getOpCode().isBooleanCompare())
      {
     if (firstChild->getSize()==1 ||
         (root->getOpCode().isUnsigned() && firstChild->getSize()==2))
      resetMem1();

     if (secondChild->getSize()==1 ||
         (root->getOpCode().isUnsigned() && secondChild->getSize()==2))
      resetMem2();
     }

   // Selectively evaluate the two children
   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   remapInputs(firstChild, firstRegister, secondChild, secondRegister, nonClobberingDestination);

   if (getOpReg1Reg2())
      {
      if (targetLabel != NULL)
         {
         finalInstr = generateS390CompareAndBranchInstruction(cg(), regToRegOpCode,
                                                              root, firstRegister, secondRegister,
                                                              getReversedOperands()?rBranchOpCond:fBranchOpCond,
                                                              targetLabel, false, true);
         nodeReg = firstRegister;
         }
      else
         {
         if (cg()->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2))
            {
            // Check for multiplications on z14
            TR::InstOpCode::Mnemonic z14OpCode = TR::InstOpCode::bad;

            if(root->getOpCodeValue() == TR::lmul &&
                    firstRegister != NULL &&
                    firstRegister->is64BitReg() &&
                    secondRegister != NULL &&
                    secondRegister->is64BitReg())
               {
               z14OpCode = TR::InstOpCode::MSGRKC;
               }
            else if(root->getOpCodeValue() == TR::imul &&
                    firstRegister != NULL &&
                    secondRegister != NULL)
               {
               z14OpCode = TR::InstOpCode::MSRKC;
               }

            if(z14OpCode != TR::InstOpCode::bad)
               {
               bool isCanClobberFirstReg = cg()->canClobberNodesRegister(firstChild);
               nodeReg = isCanClobberFirstReg ? firstRegister : cg()->allocateRegister();
               generateRRFInstruction(cg(), z14OpCode, root, nodeReg, firstRegister, secondRegister, 0, 0);
               }
            }

         if(nodeReg == NULL)
            {
            // for Compare R1, R2 for int or fp regs, check if valid CCInfo exists;
            // with same compare ops; if reg is switched, swap them..
            TR::Instruction* ccInst = cg()->ccInstruction();
            TR::InstOpCode opcTmp = TR::InstOpCode(regToRegOpCode);
            if ( opcTmp.setsCompareFlag() && comp->getOption(TR_EnableEBBCCInfo) &&
                 cg()->isActiveCompareCC(regToRegOpCode, firstRegister, secondRegister) &&
                 performTransformation(comp, "O^O BinaryCommunicativeAnalyser case 3 RR Compare [%s\t %s, %s]: reuse CC from ccInst [%p].",
                                       ccInst->getOpCode().getMnemonicName(),
                                       debugObj->getName(firstRegister),
                                       debugObj->getName(secondRegister),
                                       ccInst))
               {
               nodeReg = firstRegister; // CCInfo already exists; don't generate compare op any more;
               }
            else if (opcTmp.setsCompareFlag() && comp->getOption(TR_EnableEBBCCInfo) &&
                     cg()->isActiveCompareCC(regToRegOpCode, secondRegister, firstRegister) &&
                     performTransformation(comp, "O^O BinaryCommunicativeAnalyser case 4 RR Compare [%s\t %s, %s]: reuse CC from ccInst [%p].",
                                           ccInst->getOpCode().getMnemonicName(),
                                           debugObj->getName(firstRegister),
                                           debugObj->getName(secondRegister),
                                           ccInst))
               {
               nodeReg = secondRegister;
               notReversedOperands(); // CCInfo already exists; don't generate compare op any more;
               }
            else
               {
               generateRRInstruction(cg(), regToRegOpCode, root, firstRegister, secondRegister);
               nodeReg = firstRegister; // CCInfo already exists; don't generate compare op any more;
               }
            }
         }
      }
   else if (getOpReg2Reg1())
      {
      if (targetLabel != NULL)
         {
         finalInstr = generateS390CompareAndBranchInstruction(cg(), regToRegOpCode, root,
                                                              secondRegister, firstRegister,
                                                              (!getReversedOperands()) ? rBranchOpCond : fBranchOpCond,
                                                              targetLabel, false, true);
         nodeReg = secondRegister;
         notReversedOperands();
         }
      else
         {
         // for Compare GPR1, GPR2 or compare FPR1, FPR2, check if valid CCInfo exists;
         // with same compare ops; if reg is switched, swap them..
         TR::Instruction* ccInst = cg()->ccInstruction();
         TR::InstOpCode opcTmp = TR::InstOpCode(regToRegOpCode);
         if ( opcTmp.setsCompareFlag() && comp->getOption(TR_EnableEBBCCInfo) &&
              cg()->isActiveCompareCC(regToRegOpCode, firstRegister, secondRegister) &&
              performTransformation(comp, "O^O BinaryCommunicativeAnalyser case 5 RR Compare [%s\t %s, %s]: reuse CC from ccInst [%p].", ccInst->getOpCode().getMnemonicName(), debugObj->getName(firstRegister),debugObj->getName(secondRegister),ccInst) )
            {
            nodeReg = firstRegister; // CCInfo already exists; don't generate compare op any more;
            }
         else if (opcTmp.setsCompareFlag() && comp->getOption(TR_EnableEBBCCInfo) &&
                  cg()->isActiveCompareCC(regToRegOpCode, secondRegister, firstRegister) &&
                  performTransformation(comp, "O^O BinaryCommunicativeAnalyser case 6 RR Compare [%s\t %s, %s]: reuse CC from ccInst [%p].", ccInst->getOpCode().getMnemonicName(), debugObj->getName(firstRegister),debugObj->getName(secondRegister),ccInst) )
            {
            nodeReg = secondRegister;
            notReversedOperands(); // CCInfo already exists; don't generate compare op any more;
            }
         else
            {
            generateRRInstruction(cg(), regToRegOpCode, root, secondRegister, firstRegister);
            nodeReg = secondRegister;
            notReversedOperands();
            }
         }
      }
   else if (getCopyReg1())
      {
      switch (firstRegister->getKind())
         {
         case TR_RegisterKinds::TR_GPR:
            {
            nodeReg = allocateAddSubRegister(root, firstRegister);
            break;
            }

         case TR_RegisterKinds::TR_FPR:
            {
            nodeReg = cg()->allocateRegister(TR_FPR);
            break;
            }

         default:
            {
            TR_ASSERT_FATAL(false, "Generic binary commutative analyser for register kind (%d) is unimplemented", firstRegister->getKind());
            break;
            }
         }

      cursor = generateRRInstruction(cg(), copyOpCode, root, nodeReg, firstRegister);
      if (debugObj)
         {
         debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
         }


      if (targetLabel != NULL)
         finalInstr = generateS390CompareAndBranchInstruction(cg(), regToRegOpCode, root, nodeReg, secondRegister, getReversedOperands()?rBranchOpCond:fBranchOpCond, targetLabel, false, true);
      else
         generateRRInstruction(cg(), regToRegOpCode, root, nodeReg, secondRegister);
      }
   else if (getCopyReg2())
      {
      switch (secondRegister->getKind())
         {
         case TR_RegisterKinds::TR_GPR:
         case TR_RegisterKinds::TR_FPR:
            {
            nodeReg = cg()->allocateRegister(secondRegister->getKind());
            break;
            }

         default:
            {
            TR_ASSERT_FATAL(false, "Generic binary commutative analyser for register kind (%d) is unimplemented", secondRegister->getKind());
            break;
            }
         }

      cursor = generateRRInstruction(cg(), copyOpCode, root, nodeReg, secondRegister);
      if (debugObj)
         {
         debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
         }

      if (targetLabel != NULL)
         finalInstr = generateS390CompareAndBranchInstruction(cg(), regToRegOpCode, root, nodeReg, firstRegister, (!getReversedOperands())?rBranchOpCond:fBranchOpCond, targetLabel, false, true);
      else
         generateRRInstruction(cg(), regToRegOpCode, root, nodeReg, firstRegister);
      notReversedOperands();
      }
   else if (getOpReg3Mem2() || getOpReg1Mem2())
      {
      nodeReg = firstRegister;
      if (getOpReg3Mem2())
         {
         switch (firstRegister->getKind())
            {
            case TR_RegisterKinds::TR_GPR:
               {
               nodeReg = allocateAddSubRegister(root, firstRegister);
               break;
               }

            case TR_RegisterKinds::TR_FPR:
               {
               nodeReg = cg()->allocateRegister(TR_FPR);
               break;
               }

            default:
               {
               TR_ASSERT_FATAL(false, "Generic binary commutative analyser for register kind (%d) is unimplemented", firstRegister->getKind());
               break;
               }
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, nodeReg, firstRegister);
         }

      TR::MemoryReference * tempMR = TR::MemoryReference::create(cg(), secondChild);
      //floating-point arithmatics don't have RXY format instructions, so no long displacement
      if (secondChild->getOpCode().isFloatingPoint())
         {
         tempMR->enforce4KDisplacementLimit(secondChild, cg(), NULL);
         }
      if (adjustDiplacementOnSecondChild)
         {
         tempMR->addToOffset(4);
         tempMR->setDispAdjusted();
         }

      auto instructionFormat = TR::InstOpCode(memToRegOpCode).getInstructionFormat();

      if (instructionFormat == RXE_FORMAT)
         {
         generateRXEInstruction(cg(), memToRegOpCode, root, nodeReg, tempMR, 0);
         }
      else
         {
         generateRXInstruction(cg(), memToRegOpCode, root, nodeReg, tempMR);
         }

      tempMR->stopUsingMemRefRegister(cg());
      }
   else // OpReg2Mem1
      {
      TR_ASSERT(!getInvalid(), "TR_S390CommutativeBinaryAnalyser::invalid case\n");

      nodeReg = secondRegister;
      if (getOpReg3Mem1())
         {
         switch (secondRegister->getKind())
            {
            case TR_RegisterKinds::TR_GPR:
               {
               nodeReg = allocateAddSubRegister(root, secondRegister);
               break;
               }

            case TR_RegisterKinds::TR_FPR:
               {
               nodeReg = cg()->allocateRegister(TR_FPR);
               break;
               }

            default:
               {
               TR_ASSERT_FATAL(false, "Generic binary commutative analyser for register kind (%d) is unimplemented", secondRegister->getKind());
               break;
               }
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, nodeReg, secondRegister);
         }

      TR::Node* loadNode = isLoadNodeNested ? firstChild->getFirstChild() : firstChild;
      TR::MemoryReference * tempMR = TR::MemoryReference::create(cg(), loadNode);

      //floating-point arithmatics don't have RXY format instructions, so no long displacement
      if (firstChild->getOpCode().isFloatingPoint())
         {
         tempMR->enforce4KDisplacementLimit(firstChild, cg(), NULL);
         }

      if (adjustDiplacementOnFirstChild)
         {
         tempMR->addToOffset(4);
         tempMR->setDispAdjusted();
         }

      auto instructionFormat = TR::InstOpCode(memToRegOpCode).getInstructionFormat();

      if (instructionFormat == RXE_FORMAT)
         {
         generateRXEInstruction(cg(), memToRegOpCode, root, nodeReg, tempMR, 0);
         }
      else
         {
         generateRXInstruction(cg(), memToRegOpCode, root, nodeReg, tempMR);
         }

      if (!tempMR->getPseudoLive())
         {
         tempMR->stopUsingMemRefRegister(cg());
         }

      if(isLoadNodeNested)
         {
         cg()->decReferenceCount(loadNode);
         }

      notReversedOperands();
      }


   if ( initFirstChild )
      {
      initFirstChild->setRegister(firstChild->getRegister());
      cg()->decReferenceCount(firstChild);
      }

   if ( initSecondChild )
      {
      initSecondChild->setRegister(secondChild->getRegister());
      cg()->decReferenceCount(secondChild);
      }

   // TODO: use compareAnalyser instead for BNDCHK
   //
   if (nodeReg && !root->getOpCode().isBndCheck() && !root->getOpCode().isBranch())
      {
      root->setRegister(nodeReg);
      }

   // Check to see if we have generated branch instruction
   if (!finalInstr && targetLabel != NULL)
      {
      generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, getReversedOperands()?rBranchOpCond:fBranchOpCond , root, targetLabel);
      }

   return;
   }

/**
 * Remove conversion if legal to do so
 */
bool
TR_S390BinaryCommutativeAnalyser::conversionIsRemoved(TR::Node * root, TR::Node * &child)
   {
   if (((root->getDataType() == TR::Int64 && child->getOpCodeValue() == TR::a2l &&
         cg()->comp()->target().is64Bit()) ||
        (root->getDataType() == TR::Int32 && child->getOpCodeValue() == TR::a2i &&
       cg()->comp()->target().is32Bit()))  &&
       child->getFirstChild()->getOpCodeValue() == TR::aloadi &&
       child->getFirstChild()->getRegister() == NULL &&
       child->getFirstChild()->getReferenceCount() == 1 &&
       child->getReferenceCount() == 1 )
      {
      cg()->decReferenceCount(child);
      child = child->getFirstChild();
      return true;
      }
   return false;
   }

/**
 * Also handles 64bit integer Add for 64bit platform code-gen
 */
void
TR_S390BinaryCommutativeAnalyser::integerAddAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic regToRegOpCode, TR::InstOpCode::Mnemonic memToRegOpCode, TR::InstOpCode::Mnemonic copyOpCode)
   {
   TR::Node * firstChild;
   TR::Node * secondChild;
   TR::Instruction * cursor = NULL;
   // the following flag would be required if analyser in future generates alternative add instructions
   // which do not produce a carry. The flag would be used to prevent such optimizations when the carry is needed.
   /* bool setsOrReadsCC = NEED_CC(node) || (node->getOpCodeValue() == TR::luaddc) || (node->getOpCodeValue() == TR::iuaddc); */

   const char * CLOBBER_EVAL = "LR=Clobber_eval";
   TR_Debug * debugObj = cg()->getDebug();
   if (cg()->whichChildToEvaluate(root) == 0)
      {
      firstChild = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      firstChild = root->getSecondChild();
      secondChild = root->getFirstChild();
      setReversedOperands(true);
      }

   TR::Register * firstRegister  = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();
   TR::Compilation *comp = cg()->comp();

   bool          noDec2         = false;

   if ( secondRegister == NULL)
      noDec2 = conversionIsRemoved(root,secondChild);

   setInputs(firstChild, firstRegister, secondChild, secondRegister,
             false, false, comp);

   bool is16BitMemory2Operand = false;

   /*
    * Check if AH can be used to evaluate this add node.
    * The second operand of AH is a 16-bit number from memory. And using
    * AH directly can save a load instruction.
    */
   if (secondChild->getOpCodeValue() == TR::s2i &&
       secondChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
       secondChild->getReferenceCount() == 1 &&
       secondChild->getRegister() == NULL &&
       secondChild->getFirstChild()->getReferenceCount() == 1 &&
       secondChild->getFirstChild()->getRegister() == NULL)
      {
      setMem2();
      memToRegOpCode = TR::InstOpCode::AH;
      is16BitMemory2Operand = true;
      }

   /* Attempt to use AGH to add halfword from memory */
   if (cg()->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2) &&
       secondChild->getOpCodeValue() == TR::s2l &&
       secondChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
       secondChild->getReferenceCount() == 1 &&
       secondChild->getRegister() == NULL &&
       secondChild->getFirstChild()->getReferenceCount() == 1 &&
       secondChild->getFirstChild()->getRegister() == NULL)
      {
      setMem2();
      memToRegOpCode = TR::InstOpCode::AGH;
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

   if ((root->getOpCodeValue() == TR::iuaddc || root->getOpCodeValue() == TR::luaddc) &&
       TR_S390ComputeCC::setCarryBorrow(root->getChild(2), false, cg()))
      {
      // use ALCGR rather than ALGR
      //     ALCG rather than ALG
      // or
      // use ALCR rather than ALR
      //     ALC rather than AL
      bool nodeIs64Bit = root->getType().isInt64();
      regToRegOpCode = nodeIs64Bit ? TR::InstOpCode::ALCGR : TR::InstOpCode::ALCR;
      memToRegOpCode = nodeIs64Bit ? TR::InstOpCode::ALCG  : TR::InstOpCode::ALC;
      }

   if (getOpReg1Reg2())
      {
      generateRRInstruction(cg(), regToRegOpCode, root, firstRegister, secondRegister);
      root->setRegister(firstRegister);
      }
   else if (getOpReg2Reg1())
      {
      generateRRInstruction(cg(), regToRegOpCode, root, secondRegister, firstRegister);
      root->setRegister(secondRegister);
      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      TR::Register * tempReg = root->setRegister(allocateAddSubRegister(root, firstRegister));
      bool done = false;

      if (cg()->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
         {
         if (regToRegOpCode == TR::InstOpCode::AR)
            {
            cursor = generateRRRInstruction(cg(), TR::InstOpCode::ARK, root, tempReg, secondRegister, firstRegister);
            done = true;
            }
         else if (regToRegOpCode == TR::InstOpCode::ALR)
            {
            cursor = generateRRRInstruction(cg(), TR::InstOpCode::ALRK, root, tempReg, secondRegister, firstRegister);
            done = true;
            }
         else if (regToRegOpCode == TR::InstOpCode::AGR)
            {
            cursor = generateRRRInstruction(cg(), TR::InstOpCode::AGRK, root, tempReg, secondRegister, firstRegister);
            done = true;
            }
         else if (regToRegOpCode == TR::InstOpCode::ALGR)
            {
            cursor = generateRRRInstruction(cg(), TR::InstOpCode::ALGRK, root, tempReg, secondRegister, firstRegister);
            done = true;
            }
         }

      if (!done)
         {
         auto mnemonic = firstRegister->is64BitReg() ?
            TR::InstOpCode::LGR :
            TR::InstOpCode::LR;

         generateRRInstruction(cg(), mnemonic, root, tempReg, firstRegister);
         generateRRInstruction(cg(), regToRegOpCode, root, tempReg, secondRegister);
         }
      }
   else if (getOpReg3Mem2() || getOpReg1Mem2())
      {
      TR::Register * tempReg = firstRegister;

      if (getOpReg3Mem2())
         {
         switch (firstRegister->getKind())
            {
            case TR_RegisterKinds::TR_GPR:
               {
               tempReg = allocateAddSubRegister(root, firstRegister);
               break;
               }

            case TR_RegisterKinds::TR_FPR:
               {
               tempReg = cg()->allocateRegister(TR_FPR);
               break;
               }

            default:
               {
               TR_ASSERT_FATAL(false, "Generic binary commutative analyser for register kind (%d) is unimplemented", firstRegister->getKind());
               break;
               }
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, tempReg, firstRegister);
         }

      TR::MemoryReference * tempMR = TR::MemoryReference::create(cg(), is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild);

      generateRXInstruction(cg(), memToRegOpCode, root, tempReg, tempMR);
      root->setRegister(tempReg);
      tempMR->stopUsingMemRefRegister(cg());
      if (is16BitMemory2Operand)
         cg()->decReferenceCount(secondChild->getFirstChild());
      }
   else
      {
      TR_ASSERT(!getInvalid(), "TR_S390CommutativeBinaryAnalyser::invalid case\n");

      TR::Register *tempReg = secondRegister;

      if (getOpReg3Mem1())
         {
         switch (secondRegister->getKind())
            {
            case TR_RegisterKinds::TR_GPR:
               {
               tempReg = allocateAddSubRegister(root, secondRegister);
               break;
               }

            case TR_RegisterKinds::TR_FPR:
               {
               tempReg = cg()->allocateRegister(TR_FPR);
               break;
               }

            default:
               {
               TR_ASSERT_FATAL(false, "Generic binary commutative analyser for register kind (%d) is unimplemented", secondRegister->getKind());
               break;
               }
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, tempReg, secondRegister);
         }

      TR::MemoryReference * tempMR = TR::MemoryReference::create(cg(), firstChild);

      generateRXInstruction(cg(), memToRegOpCode, root, tempReg, tempMR);
      root->setRegister(tempReg);
      tempMR->stopUsingMemRefRegister(cg());
      notReversedOperands();
      }

   //if (hasCompressedPointers)
   //   generateS390LabelInstruction(cg(), TR::InstOpCode::label, root, skipAdd);

   cg()->decReferenceCount(firstChild);
   cg()->decReferenceCount(secondChild);

   return;
   }

bool
TR_S390BinaryCommutativeAnalyser::getCopyReg1()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & CopyReg1)   ? true : false;
      }
   return (actionMap[getInputs()] & CopyReg1)   ? true : false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getCopyReg2()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & CopyReg2)   ? true : false;
      }
   return (actionMap[getInputs()] & CopyReg2)   ? true : false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getOpReg1Reg2()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & OpReg1Reg2) ? true : false;
      }
   return (actionMap[getInputs()] & OpReg1Reg2) ? true : false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getOpReg2Reg1()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & OpReg2Reg1) ? true : false;
      }
   return (actionMap[getInputs()] & OpReg2Reg1) ? true : false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getOpReg1Mem2()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & OpReg1Mem2) ? true : false;
      }
   return (actionMap[getInputs()] & OpReg1Mem2) ? true : false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getOpReg3Mem2()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & OpReg3Mem2) ? true : false;
      }
   return false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getOpReg2Mem1()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & OpReg2Mem1) ? true : false;
      }
   return (actionMap[getInputs()] & OpReg2Mem1) ? true : false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getOpReg3Mem1()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & OpReg3Mem1) ? true : false;
      }
   return false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getCopyRegs()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & (CopyReg1 | CopyReg2)) ? true : false;
      }
   return (actionMap[getInputs()] & (CopyReg1 | CopyReg2)) ? true : false;
   }

bool
TR_S390BinaryCommutativeAnalyser::getInvalid()
   {
   if (!cg()->useClobberEvaluate())
      {
      return (clobEvalActionMap[getInputs()] & InvalidBCACEMap) ? true : false;
      }
   return false;
   }

const uint8_t TR_S390BinaryCommutativeAnalyser::actionMap[NUM_ACTIONS] =
   {
   // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   EvalChild1 |        //  0    0     0    0    0     0
   EvalChild2 | CopyReg1,

   EvalChild1 |        //  0    0     0    0    0     1
   EvalChild2 | OpReg2Reg1,

   EvalChild1 |        //  0    0     0    0    1     0
   EvalChild2 | CopyReg1,

   EvalChild1 |        //  0    0     0    0    1     1
   EvalChild2 | OpReg2Reg1,

   EvalChild1 |        //  0    0     0    1    0     0
   CopyReg2,

   EvalChild1 |        //  0    0     0    1    0     1
   OpReg2Reg1,

   EvalChild1 |        //  0    0     0    1    1     0
   CopyReg2,

   EvalChild1 |        //  0    0     0    1    1     1
   OpReg2Reg1,

   EvalChild1 |        //  0    0     1    0    0     0
   EvalChild2 | OpReg1Reg2,

   EvalChild1 |        //  0    0     1    0    0     1
   EvalChild2 | OpReg1Reg2,

   EvalChild1 |        //  0    0     1    0    1     0
   EvalChild2 | OpReg1Reg2,

   EvalChild1 |        //  0    0     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |        //  0    0     1    1    0     0
   OpReg1Reg2,

   EvalChild1 |        //  0    0     1    1    0     1
   OpReg1Reg2,

   EvalChild1 |        //  0    0     1    1    1     0
   OpReg1Reg2,

   EvalChild1 |        //  0    0     1    1    1     1
   OpReg1Reg2,

   EvalChild1 |        //  0    1     0    0    0     0
   EvalChild2 | CopyReg1,

   EvalChild1 |        //  0    1     0    0    0     1
   EvalChild2 | OpReg2Reg1,

   EvalChild1 |        //  0    1     0    0    1     0
   EvalChild2 | CopyReg1,

   EvalChild1 |        //  0    1     0    0    1     1
   EvalChild2 | OpReg2Reg1,

   EvalChild1 |        //  0    1     0    1    0     0
   CopyReg2,

   EvalChild1 |        //  0    1     0    1    0     1
   OpReg2Reg1,

   EvalChild1 |        //  0    1     0    1    1     0
   CopyReg2,

   EvalChild1 |        //  0    1     0    1    1     1
   OpReg2Reg1,

   EvalChild1 |        //  0    1     1    0    0     0
   EvalChild2 | OpReg1Reg2,

   EvalChild2 |        //  0    1     1    0    0     1
   OpReg2Mem1,

   EvalChild1 |        //  0    1     1    0    1     0
   EvalChild2 |

   OpReg1Reg2,

   EvalChild1 |        //  0    1     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |        //  0    1     1    1    0     0
   OpReg1Reg2,

   OpReg2Mem1,        //  0    1     1    1    0     1

   EvalChild1 |        //  0    1     1    1    1     0
   OpReg1Reg2,

   OpReg2Mem1,        //  0    1     1    1    1     1

   EvalChild2 |        //  1    0     0    0    0     0
   CopyReg1,

   EvalChild2 |        //  1    0     0    0    0     1
   OpReg2Reg1,

   EvalChild2 |        //  1    0     0    0    1     0
   CopyReg1,

   EvalChild2 |        //  1    0     0    0    1     1
   OpReg2Reg1,

   CopyReg1,          //  1    0     0    1    0     0

   OpReg2Reg1,        //  1    0     0    1    0     1

   CopyReg1,          //  1    0     0    1    1     0

   OpReg2Reg1,        //  1    0     0    1    1     1

   EvalChild2 |        //  1    0     1    0    0     0
   OpReg1Reg2,

   EvalChild2 |        //  1    0     1    0    0     1
   OpReg1Reg2,

   EvalChild2 |        //  1    0     1    0    1     0
   OpReg1Reg2,

   OpReg1Mem2,        //  1    0     1    0    1     1

   OpReg1Reg2,        //  1    0     1    1    0     0

   OpReg1Reg2,        //  1    0     1    1    0     1

   OpReg1Reg2,        //  1    0     1    1    1     0

   OpReg1Reg2,        //  1    0     1    1    1     1

   EvalChild2 |        //  1    1     0    0    0     0
   CopyReg1,

   EvalChild2 |        //  1    1     0    0    0     1
   OpReg2Reg1,

   EvalChild2 |        //  1    1     0    0    1     0
   CopyReg1,

   EvalChild2 |        //  1    1     0    0    1     1
   OpReg2Reg1,

   CopyReg1,          //  1    1     0    1    0     0

   OpReg2Reg1,        //  1    1     0    1    0     1

   CopyReg1,          //  1    1     0    1    1     0

   OpReg2Reg1,        //  1    1     0    1    1     1

   EvalChild2 |        //  1    1     1    0    0     0
   OpReg1Reg2,

   EvalChild2 |        //  1    1     1    0    0     1
   OpReg1Reg2,

   EvalChild2 |        //  1    1     1    0    1     0
   OpReg1Reg2,

   OpReg1Mem2,        //  1    1     1    0    1     1

   OpReg1Reg2,        //  1    1     1    1    0     0

   OpReg1Reg2,        //  1    1     1    1    0     1

   OpReg1Reg2,        //  1    1     1    1    1     0

   OpReg1Reg2         //  1    1     1    1    1     1

   };

const uint16_t TR_S390BinaryCommutativeAnalyser::clobEvalActionMap[NUM_ACTIONS] =
   {
                                // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   InvalidBCACEMap,			// 0 0 0 0 0 0
   InvalidBCACEMap,			// 0 0 0 0 0 1
   InvalidBCACEMap,			// 0 0 0 0 1 0
   InvalidBCACEMap,			// 0 0 0 0 1 1
   InvalidBCACEMap,			// 0 0 0 1 0 0
   InvalidBCACEMap,			// 0 0 0 1 0 1
   InvalidBCACEMap,			// 0 0 0 1 1 0
   InvalidBCACEMap,			// 0 0 0 1 1 1
   InvalidBCACEMap,			// 0 0 1 0 0 0
   InvalidBCACEMap,			// 0 0 1 0 0 1
   InvalidBCACEMap,			// 0 0 1 0 1 0
   InvalidBCACEMap,			// 0 0 1 0 1 1
   InvalidBCACEMap,			// 0 0 1 1 0 0
   InvalidBCACEMap,			// 0 0 1 1 0 1
   InvalidBCACEMap,			// 0 0 1 1 1 0
   InvalidBCACEMap,			// 0 0 1 1 1 1
   InvalidBCACEMap,			// 0 1 0 0 0 0
   InvalidBCACEMap,			// 0 1 0 0 0 1
   InvalidBCACEMap,			// 0 1 0 0 1 0
   InvalidBCACEMap,			// 0 1 0 0 1 1
   OpReg3Mem1,			// 0 1 0 1 0 0
   OpReg2Mem1,			// 0 1 0 1 0 1
   OpReg3Mem1,			// 0 1 0 1 1 0
   OpReg2Mem1,			// 0 1 0 1 1 1
   InvalidBCACEMap,			// 0 1 1 0 0 0
   InvalidBCACEMap,			// 0 1 1 0 0 1
   InvalidBCACEMap,			// 0 1 1 0 1 0
   InvalidBCACEMap,			// 0 1 1 0 1 1
   OpReg3Mem1,			// 0 1 1 1 0 0
   OpReg2Mem1,			// 0 1 1 1 0 1
   OpReg3Mem1,			// 0 1 1 1 1 0
   OpReg2Mem1,			// 0 1 1 1 1 1
   InvalidBCACEMap,			// 1 0 0 0 0 0
   InvalidBCACEMap,			// 1 0 0 0 0 1
   OpReg3Mem2,			// 1 0 0 0 1 0
   OpReg3Mem2,			// 1 0 0 0 1 1
   CopyReg1,			// 1 0 0 1 0 0
   OpReg2Reg1,			// 1 0 0 1 0 1
   CopyReg1,			// 1 0 0 1 1 0
   OpReg2Reg1,			// 1 0 0 1 1 1
   InvalidBCACEMap,			// 1 0 1 0 0 0
   InvalidBCACEMap,			// 1 0 1 0 0 1
   OpReg1Mem2,			// 1 0 1 0 1 0
   OpReg1Mem2,			// 1 0 1 0 1 1
   OpReg1Reg2,			// 1 0 1 1 0 0
   OpReg1Reg2,			// 1 0 1 1 0 1
   OpReg1Reg2,			// 1 0 1 1 1 0
   OpReg1Reg2,			// 1 0 1 1 1 1
   InvalidBCACEMap,			// 1 1 0 0 0 0
   InvalidBCACEMap,			// 1 1 0 0 0 1
   OpReg3Mem2,			// 1 1 0 0 1 0
   OpReg3Mem2,			// 1 1 0 0 1 1
   CopyReg1,			// 1 1 0 1 0 0
   OpReg2Reg1,			// 1 1 0 1 0 1
   CopyReg1,			// 1 1 0 1 1 0
   OpReg2Reg1,			// 1 1 0 1 1 1
   InvalidBCACEMap,			// 1 1 1 0 0 0
   InvalidBCACEMap,			// 1 1 1 0 0 1
   OpReg1Mem2,			// 1 1 1 0 1 0
   OpReg1Mem2,			// 1 1 1 0 1 1
   OpReg1Reg2,			// 1 1 1 1 0 0
   OpReg1Reg2,			// 1 1 1 1 0 1
   OpReg1Reg2,			// 1 1 1 1 1 0
   OpReg1Reg2			// 1 1 1 1 1 1
   };
