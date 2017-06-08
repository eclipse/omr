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

#include "z/codegen/BinaryCommutativeAnalyser.hpp"

#include <stddef.h>                                 // for NULL
#include <stdint.h>                                 // for uint16_t, etc
#include "codegen/Analyser.hpp"                     // for NUM_ACTIONS
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction
#include "codegen/MemoryReference.hpp"              // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/TreeEvaluator.hpp"                // for lloadHelper, etc
#include "compile/Compilation.hpp"                  // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"                         // for DataTypes::Int32, etc
#include "il/ILOpCodes.hpp"                         // for ILOpCodes::land, etc
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"

#define ENABLE_ZARCH_FOR_32    1

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
   TR::Register * trgReg;

   if (src1Reg->containsInternalPointer() || !src1Reg->containsCollectedReference())
      {
      if ((node->getType().isInt64() || src1Reg->getKind() == TR_GPR64) && cg()->use64BitRegsOn32Bit())
         trgReg = cg()->allocate64bitRegister();
      else
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
   char * CLOBBER_EVAL  = "LR=Clobber_eval";
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
             (firstChild->getFirstChild()->getOpCodeValue() == TR::buload ||
              firstChild->getFirstChild()->getOpCodeValue() == TR::buloadi  ))
          {
          initFirstChild = firstChild;

          firstChild = firstChild->getFirstChild();
          cg()->evaluate(firstChild);
          }

      if (secondChild->getOpCodeValue() == TR::bu2i &&
          secondChild->getReferenceCount() == 1    &&
          secondChild->getRegister() == NULL       &&
             (secondChild->getFirstChild()->getOpCodeValue() == TR::buload ||
              secondChild->getFirstChild()->getOpCodeValue() == TR::buloadi  ))
          {
          initSecondChild = secondChild;

          secondChild = secondChild->getFirstChild();
          cg()->evaluate(secondChild);
          }
      }

   if (TR::Compiler->target.is64Bit())
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

   // TODO: add MH and MHY here; outside of the zNext if check.
   if(TR::Compiler->target.cpu.getS390SupportsZNext())
      {
      bool isSetReg2Mem1 = false;

      if(root->getOpCodeValue() == TR::lmul)
         {
         if(firstChild->getOpCodeValue() == TR::s2l &&
                 firstChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
                 firstChild->isSingleRefUnevaluated() &&
                 firstChild->getFirstChild()->isSingleRefUnevaluated())
            {
            /* Use MGH when 64<-64x16 and the multiplier is a short int in storage
             * AND it's never loaded before. */
            isLoadNodeNested = true;
            isSetReg2Mem1 = true;
            memToRegOpCode = TR::InstOpCode::MGH;
            }
         else if(firstChild->getOpCodeValue() == TR::lloadi &&
                 firstChild->isSingleRefUnevaluated())
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
                 firstChild->isSingleRefUnevaluated())
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
         if(TR::Compiler->target.cpu.getS390SupportsZNext())
            {
            // Check for multiplications on zNext
            TR::InstOpCode::Mnemonic zNextOpCode = TR::InstOpCode::BAD;

            if(root->getOpCodeValue() == TR::lmul &&
                    firstRegister != NULL &&
                    firstRegister->is64BitReg() &&
                    secondRegister != NULL &&
                    secondRegister->is64BitReg())
               {
               zNextOpCode = TR::InstOpCode::MSGRKC;
               }
            else if(root->getOpCodeValue() == TR::imul &&
                    firstRegister != NULL &&
                    secondRegister != NULL)
               {
               zNextOpCode = TR::InstOpCode::MSRKC;
               }

            if(zNextOpCode != TR::InstOpCode::BAD)
               {
               bool isCanClobberFirstReg = cg()->canClobberNodesRegister(firstChild);
               nodeReg = isCanClobberFirstReg ? firstRegister : cg()->allocate64bitRegister();
               generateRRFInstruction(cg(), zNextOpCode, root, nodeReg, firstRegister, secondRegister, 0, 0);
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
                                       debugObj->getOpCodeName(&ccInst->getOpCode()),
                                       debugObj->getName(firstRegister),
                                       debugObj->getName(secondRegister),
                                       ccInst))
               {
               nodeReg = firstRegister; // CCInfo already exists; don't generate compare op any more;
               }
            else if (opcTmp.setsCompareFlag() && comp->getOption(TR_EnableEBBCCInfo) &&
                     cg()->isActiveCompareCC(regToRegOpCode, secondRegister, firstRegister) &&
                     performTransformation(comp, "O^O BinaryCommunicativeAnalyser case 4 RR Compare [%s\t %s, %s]: reuse CC from ccInst [%p].",
                                           debugObj->getOpCodeName(&ccInst->getOpCode()),
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
              performTransformation(comp, "O^O BinaryCommunicativeAnalyser case 5 RR Compare [%s\t %s, %s]: reuse CC from ccInst [%p].", debugObj->getOpCodeName(&ccInst->getOpCode()), debugObj->getName(firstRegister),debugObj->getName(secondRegister),ccInst) )
            {
            nodeReg = firstRegister; // CCInfo already exists; don't generate compare op any more;
            }
         else if (opcTmp.setsCompareFlag() && comp->getOption(TR_EnableEBBCCInfo) &&
                  cg()->isActiveCompareCC(regToRegOpCode, secondRegister, firstRegister) &&
                  performTransformation(comp, "O^O BinaryCommunicativeAnalyser case 6 RR Compare [%s\t %s, %s]: reuse CC from ccInst [%p].", debugObj->getOpCodeName(&ccInst->getOpCode()), debugObj->getName(firstRegister),debugObj->getName(secondRegister),ccInst) )
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
      if (secondRegister->getKind() == TR_GPR64)
         {
         nodeReg = cg()->allocate64bitRegister();
         }
      else if (secondRegister->getKind() != TR_FPR && secondRegister->getKind() != TR_VRF)
         {
         nodeReg = allocateAddSubRegister(root, firstRegister);
         }
      else
         {
         nodeReg = cg()->allocateRegister(TR_FPR);
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
      if (secondRegister->getKind() == TR_GPR64)
         {
         nodeReg = cg()->allocate64bitRegister();
         }
      else if (secondRegister->getKind() != TR_FPR && secondRegister->getKind() != TR_VRF)
         {
         nodeReg = cg()->allocateRegister();
         }
      else
         {
         nodeReg = cg()->allocateRegister(TR_FPR);
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
         if (firstRegister->getKind() == TR_GPR64)
            {
            nodeReg = cg()->allocate64bitRegister();
            }
         else if (firstRegister->getKind() != TR_FPR && firstRegister->getKind() != TR_VRF)
            {
            nodeReg = allocateAddSubRegister(root, firstRegister);
            }
         else
            {
            nodeReg = cg()->allocateRegister(TR_FPR);
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, nodeReg, firstRegister);
         }

      TR::MemoryReference * tempMR = generateS390MemoryReference(secondChild, cg(), true);
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
      generateRXInstruction(cg(), memToRegOpCode, root, nodeReg, tempMR);
      tempMR->stopUsingMemRefRegister(cg());
      }
   else // OpReg2Mem1
      {
      TR_ASSERT(!getInvalid(), "TR_S390CommutativeBinaryAnalyser::invalid case\n");

      nodeReg = secondRegister;
      if (getOpReg3Mem1())
         {
         if (secondRegister->getKind() == TR_GPR64)
            {
            nodeReg = cg()->allocate64bitRegister();
            }
         else if (secondRegister->getKind() != TR_FPR && secondRegister->getKind() != TR_VRF)
            {
            nodeReg = allocateAddSubRegister(root, secondRegister);
            }
         else
            {
            nodeReg = cg()->allocateRegister(TR_FPR);
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, nodeReg, secondRegister);
         }

      TR::Node* loadNode = isLoadNodeNested ? firstChild->getFirstChild() : firstChild;
      TR::MemoryReference * tempMR = generateS390MemoryReference(loadNode, cg(), true);

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

      generateRXInstruction(cg(), memToRegOpCode, root, nodeReg, tempMR);
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
   if (nodeReg && !root->getOpCode().isBndCheck())
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


void
TR_S390BinaryCommutativeAnalyser::genericLongAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic lowRegToRegOpCode,
   TR::InstOpCode::Mnemonic highRegToRegOpCode, TR::InstOpCode::Mnemonic lowMemToRegOpCode, TR::InstOpCode::Mnemonic highMemToRegOpCode, TR::InstOpCode::Mnemonic copyOpCode)
   {
   TR::Node * firstChild;
   TR::Node * secondChild;
   TR::Instruction * cursor = NULL;
   TR::RegisterDependencyConditions * dependencies = NULL;
   TR::Register * twoLow;
   TR::Register * twoHigh;
   TR::Register * oneLow;
   TR::Register * oneHigh;
   TR::Node * initFirstChild = NULL;
   TR::Node * initSecondChild = NULL;
   bool      needsRegPairDep = true;

   char * CLOBBER_EVAL = "LR=Clobber_eval";
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
   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();

   if (root->getOpCodeValue() == TR::land  ||
       root->getOpCodeValue() == TR::lor   ||
       root->getOpCodeValue() == TR::lxor  )
      {
      needsRegPairDep = false;
      }

   bool firstHighZero = false;
   bool secondHighZero = false;
   bool useFirstHighOrder = false;
   bool useSecondHighOrder = false;

   if (firstChild->isHighWordZero())
      {
      firstHighZero = true;
      TR::ILOpCodes firstOp = firstChild->getOpCodeValue();
      if (firstChild->getReferenceCount() == 1 && firstRegister == 0)
         {
         // bu2l, su2l, c2l need sign-extention
         if (firstOp ==
            TR::iu2l ||
            (firstOp == TR::lushr &&
            firstChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
            (firstChild->getSecondChild()->getInt() & LONG_SHIFT_MASK) == 32))
            {
            firstChild = firstChild->getFirstChild();
            initFirstChild = firstChild;
            firstRegister = firstChild->getRegister();
            if (firstOp == TR::lushr)
               {
               useFirstHighOrder = true;
               }
            }
         }
      }

   if (secondChild->isHighWordZero())
      {
      secondHighZero = true;
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if (secondChild->getReferenceCount() == 1 && secondRegister == 0)
         {
         // bu2l, su2l, c2l need sign-extention
         if (secondOp ==
            TR::iu2l ||
            (secondOp == TR::lushr &&
            secondChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
            (secondChild->getSecondChild()->getInt() & LONG_SHIFT_MASK) == 32))
            {
            secondChild = secondChild->getFirstChild();
            initSecondChild = secondChild;
            secondRegister = secondChild->getRegister();
            if (secondOp == TR::lushr)
               {
               useSecondHighOrder = true;
               }
            }
         }
      }

   setInputs(firstChild, firstRegister, secondChild, secondRegister,
             false, false, cg()->comp());

   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   remapInputs(firstChild, firstRegister, secondChild, secondRegister);

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

   if (getOpReg1Reg2())
      {
      //diagnostic("\nOpReg1Reg2\n");
      if (!firstHighZero)
         {
         oneLow = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow = firstRegister;
         oneHigh = cg()->allocateRegister();;
         }

      if (!secondHighZero)
         {
         twoLow = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow = secondRegister;
         twoHigh = 0;
         }

      TR::RegisterPair * tempReg = cg()->allocateConsecutiveRegisterPair(oneLow, oneHigh);

      if (needsRegPairDep)
         {
         dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg());
         dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(oneHigh, TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(oneLow, TR::RealRegister::LegalOddOfPair);
         }

      cursor = generateRRInstruction(cg(), lowRegToRegOpCode, root, oneLow, twoLow);

      if (firstHighZero)
         {
         if (secondHighZero || root->getOpCodeValue() == TR::land)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::XR, root, oneHigh, oneHigh);
            }
         else
            {
            cursor = generateRRInstruction(cg(), copyOpCode, root, oneHigh, twoHigh);
            }
         }
      else if (secondHighZero)
         {
         if (root->getOpCodeValue() == TR::land)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::XR, root, oneHigh, oneHigh);
            }
         }
      else
         {
         cursor = generateRRInstruction(cg(), highRegToRegOpCode, root, oneHigh, twoHigh);
         }

      root->setRegister(tempReg);
      }
   else if (getOpReg2Reg1())
      {
      //diagnostic("\nOpReg2Reg1\n");
      if (!firstHighZero)
         {
         oneLow = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow = secondRegister;
         twoHigh = cg()->allocateRegister();
         }

      TR::RegisterPair * tempReg = cg()->allocateConsecutiveRegisterPair(twoLow, twoHigh);

      if (needsRegPairDep)
         {
         dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg());
         dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(twoHigh, TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(twoLow, TR::RealRegister::LegalOddOfPair);
         }

      cursor = generateRRInstruction(cg(), lowRegToRegOpCode, root, twoLow, oneLow);

      if (secondHighZero)
         {
         if (firstHighZero || root->getOpCodeValue() == TR::land)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::XR, root, twoHigh, twoHigh);
            }
         else
            {
            cursor = generateRRInstruction(cg(), copyOpCode, root, twoHigh, oneHigh);
            }
         }
      else if (firstHighZero)
         {
         if (root->getOpCodeValue() == TR::land)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::XR, root, twoHigh, twoHigh);
            }
         }
      else
         {
         cursor = generateRRInstruction(cg(), highRegToRegOpCode, root, twoHigh, oneHigh);
         }

      root->setRegister(tempReg);
      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      //diagnostic("\nCopyRegs\n");

      TR::Register * lowRegister = cg()->allocateRegister();
      TR::Register * highRegister = cg()->allocateRegister();

      TR::RegisterPair * tempReg = cg()->allocateConsecutiveRegisterPair(lowRegister, highRegister);

      if (needsRegPairDep)
         {
         dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg());
         dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(highRegister, TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(lowRegister, TR::RealRegister::LegalOddOfPair);
         }

      if (!firstHighZero)
         {
         oneLow = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow = secondRegister;
         twoHigh = 0;
         }

      generateRRInstruction(cg(), copyOpCode, root, lowRegister, oneLow);

      if (firstHighZero)
         {
         if (secondHighZero || root->getOpCodeValue() == TR::land)
            {
            generateRRInstruction(cg(), TR::InstOpCode::XR, root, highRegister, highRegister);
            }
         else
            {
            generateRRInstruction(cg(), copyOpCode, root, highRegister, twoHigh);
            }
         }
      else if (secondHighZero)
         {
         if (root->getOpCodeValue() == TR::land)
            {
            generateRRInstruction(cg(), TR::InstOpCode::XR, root, highRegister, highRegister);
            }
         else
            {
            generateRRInstruction(cg(), copyOpCode, root, highRegister, oneHigh);
            }
         }
      else
         {
         generateRRInstruction(cg(), copyOpCode, root, highRegister, oneHigh);
         generateRRInstruction(cg(), highRegToRegOpCode, root, highRegister, twoHigh);
         }

      cursor = generateRRInstruction(cg(), lowRegToRegOpCode, root, lowRegister, twoLow);


      root->setRegister(tempReg);
      }
   else // RX instruction
      {
      TR_ASSERT(!getInvalid(), "TR_S390CommutativeBinaryAnalyser::invalid case\n");

      //diagnostic("\nOpMem\n");

      TR::MemoryReference * lowMR = NULL;
      TR::MemoryReference * highMR = NULL;
      TR::Register * targetReg;
      TR::Register * targetLow;
      TR::Register * targetHigh;

      bool targetHighZero;
      bool memHighZero;
      bool useMemHighOrder;

      bool secondOperandIsInRegister = false;
      TR::Register * longRegister = NULL;
      bool createdClobberPair = false;

      if (getOpReg1Mem2() || getOpReg3Mem2())
         {
         if (!secondHighZero && secondChild->getSymbolReference()->isUnresolved())
            {
            secondOperandIsInRegister = true;
            lloadHelper(secondChild, cg(), NULL);
            longRegister = secondChild->getRegister();
            }
         else
            {

/*
 *
 [0x65cd0c08] (  1)    lor (in GPR_0x78e6b5f8:GPR_0x78e6b5bc)
 [0x65cd0bd0] (  0)      lushr
 [0x65cd0b60] (  0)        ilload #496[0x6b699868]+8  final Shadow[java/util/UUID.mostSigBits J]
              (  0)          ==>aRegLoad at [0x789ae4e8] (in &GPR_0x78e6ae4c)
 [0x65cd0b98] (  1)        iconst 32
 [0x65cd0af0] (  0)      lor (in GPR_0x78e6b5f8:GPR_0x78e6b5bc)
...
For trees with one of the children having lushr by 32 bits and in highWordZero when useSecondHighOrder or useFirstHighOrder are set
and that child is a mem ref, we need to use high order
 */
            highMR = generateS390MemoryReference(secondChild, cg(), true /*canUseRX*/);
            if (!secondHighZero || (secondChild->getType().isInt64() && !useSecondHighOrder))
               {
               lowMR = generateS390MemoryReference(*highMR, 4, cg());
               }
            else
               {
               lowMR = highMR;
               }
            }

         if (getOpReg3Mem2())
            {
            if (firstHighZero)
               {
               targetReg = allocateAddSubRegister(root, firstRegister);
               generateRRInstruction(cg(), copyOpCode, root, targetReg, firstRegister);
               }
            else
               {
               createdClobberPair = true;
               TR::Register * lowRegister = allocateAddSubRegister(root, firstRegister->getLowOrder());
               TR::Register * highRegister = allocateAddSubRegister(root, firstRegister->getHighOrder());
               targetReg = cg()->allocateConsecutiveRegisterPair(lowRegister, highRegister);
               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getLowOrder(), firstRegister->getLowOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }

               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getHighOrder(), firstRegister->getHighOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }
               }
            }
         else
            {
            targetReg = firstRegister;
            }

         targetHighZero = firstHighZero;
         memHighZero = secondHighZero;
         useMemHighOrder = useSecondHighOrder;
         }
      else
         {
         if (!firstHighZero && firstChild->getSymbolReference()->isUnresolved())
            {
            secondOperandIsInRegister = true;
            lloadHelper(firstChild, cg(), NULL);
            longRegister = firstChild->getRegister();
            }
         else
            {
            /*
             * need to use !useFirstHighOrder (see the comment above)
             */
            highMR = generateS390MemoryReference(firstChild, cg(), true /*canUseRX*/);
            if (!firstHighZero || (firstChild->getType().isInt64() && !useFirstHighOrder))
               {
               lowMR = generateS390MemoryReference(*highMR, 4, cg());
               }
            else
               {
               lowMR = highMR;
               }
            }

         if (getOpReg3Mem1())
            {
            if (secondHighZero)
               {
               targetReg = allocateAddSubRegister(root, secondRegister);
               generateRRInstruction(cg(), copyOpCode, root, targetReg, secondRegister);
               }
            else
               {
               createdClobberPair = true;
               TR::Register * lowRegister = allocateAddSubRegister(root, secondRegister->getLowOrder());
               TR::Register * highRegister = allocateAddSubRegister(root, secondRegister->getHighOrder());
               targetReg = cg()->allocateConsecutiveRegisterPair(lowRegister, highRegister);
               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getLowOrder(), secondRegister->getLowOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }

               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getHighOrder(), secondRegister->getHighOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }
               }
            }
         else
            {
            targetReg = secondRegister;
            }

         targetHighZero = secondHighZero;
         memHighZero = firstHighZero;
         useMemHighOrder = useFirstHighOrder;
         }

      if (targetHighZero)
         {
         targetLow = targetReg;
         targetHigh = cg()->allocateRegister();
         }
      else
         {
         targetLow = targetReg->getLowOrder();
         targetHigh = targetReg->getHighOrder();
         }

      TR::RegisterPair * tempReg = NULL;
      if (createdClobberPair)
         {
         tempReg = (TR::RegisterPair *) targetReg;
         }
      else
         {
         tempReg = cg()->allocateConsecutiveRegisterPair(targetLow, targetHigh);
         }

      dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg());
      if (needsRegPairDep)
         {
         dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(targetHigh, TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(targetLow, TR::RealRegister::LegalOddOfPair);
         }

      if (highMR)
         {
         dependencies->addAssignAnyPostCondOnMemRef(highMR);
         }

      if (secondOperandIsInRegister)
         {
         cursor = generateRRInstruction(cg(), lowRegToRegOpCode, root, targetLow, longRegister->getLowOrder());
         }
      else
         {
         cursor = generateRXInstruction(cg(), lowMemToRegOpCode, root, targetLow, lowMR);
         }

      if (memHighZero)
         {
         if (root->getOpCodeValue() == TR::land || targetHighZero)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::XR, root, targetHigh, targetHigh);
            }
         }
      else if (targetHighZero)
         {
         if (root->getOpCodeValue() == TR::land)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::XR, root, targetHigh, targetHigh);
            }
         else
            {
            if (secondOperandIsInRegister)
               {
               cursor = generateRRInstruction(cg(), copyOpCode, root, targetHigh, longRegister->getHighOrder());
               }
            else
               {
               cursor = generateRXInstruction(cg(), TR::InstOpCode::L, root, targetHigh, highMR);
               }
            }
         }
      else
         {
         if (secondOperandIsInRegister)
            {
            cursor = generateRRInstruction(cg(), highRegToRegOpCode, root, targetHigh, longRegister->getHighOrder());
            }
         else
            {
            cursor = generateRXInstruction(cg(), highMemToRegOpCode, root, targetHigh, highMR);
            }
         }

      root->setRegister(tempReg);

      if (!secondOperandIsInRegister)
         {
         highMR->stopUsingMemRefRegister(cg());
         }

      if (!getOpReg1Mem2())
         {
         notReversedOperands();
         }
      }

   if (needsRegPairDep)
      {
      generateS390PseudoInstruction(cg(), TR::InstOpCode::DEPEND, root, dependencies);
      }

   if ( initFirstChild )
      {
      cg()->decReferenceCount(initFirstChild);
      }

   if ( initSecondChild )
      {
      cg()->decReferenceCount(initSecondChild);
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
         TR::Compiler->target.is64Bit()) ||
        (root->getDataType() == TR::Int32 && child->getOpCodeValue() == TR::a2i &&
       TR::Compiler->target.is32Bit()))  &&
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

   char * CLOBBER_EVAL = "LR=Clobber_eval";
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
       secondChild->isSingleRefUnevaluated() &&
       secondChild->getFirstChild()->isSingleRefUnevaluated())
      {
      setMem2();
      memToRegOpCode = TR::InstOpCode::AH;
      is16BitMemory2Operand = true;
      }

   /**  Attempt to use AGH to add halfworf from memory */
   if (TR::Compiler->target.cpu.getS390SupportsZNext() &&
       secondChild->getOpCodeValue() == TR::s2l &&
       secondChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
       secondChild->isSingleRefUnevaluated() &&
       secondChild->getFirstChild()->isSingleRefUnevaluated())
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

      if (cg()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
         if (firstRegister->getKind() == TR_GPR64 ||
             ((firstChild->getType().isInt64() || TR::Compiler->target.is64Bit()) && cg()->use64BitRegsOn32Bit()))
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::LGR, root, tempReg, firstRegister);
            }
         else
            {
            if (cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
               {
               cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCodeFromNode(cg(), root), root, tempReg, firstRegister);
               }
            else
               {
               cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), root, tempReg, firstRegister);
               }
            }
         if (debugObj)
            {
            debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
            }
         generateRRInstruction(cg(), regToRegOpCode, root, tempReg, secondRegister);
         }
      }
   else if (getOpReg3Mem2() || getOpReg1Mem2())
      {
      TR::Register * tempReg = firstRegister;

      if (getOpReg3Mem2())
         {
         if (firstRegister->getKind() == TR_GPR64)
            {
            tempReg = cg()->allocate64bitRegister();
            }
         else if (firstRegister->getKind() != TR_FPR && firstRegister->getKind() != TR_VRF)
            {
            tempReg = allocateAddSubRegister(root, firstRegister);
            }
         else
            {
            tempReg = cg()->allocateRegister(TR_FPR);
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, tempReg, firstRegister);
         }

      TR::MemoryReference * tempMR = generateS390MemoryReference(is16BitMemory2Operand ? secondChild->getFirstChild() : secondChild, cg());

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
         if (secondRegister->getKind() == TR_GPR64)
            {
            tempReg = cg()->allocate64bitRegister();
            }
         else if (secondRegister->getKind() != TR_FPR && secondRegister->getKind() != TR_VRF)
            {
            tempReg = allocateAddSubRegister(root, secondRegister);
            }
         else
            {
            tempReg = cg()->allocateRegister(TR_FPR);
            }

         cursor = generateRRInstruction(cg(), copyOpCode, root, tempReg, secondRegister);
         }

      TR::MemoryReference * tempMR = generateS390MemoryReference(firstChild, cg());

      generateRXInstruction(cg(), memToRegOpCode, root, tempReg, tempMR);
      root->setRegister(tempReg);
      tempMR->stopUsingMemRefRegister(cg());
      notReversedOperands();
      }

   //if (hasCompressedPointers)
   //   generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, root, skipAdd);

   cg()->decReferenceCount(firstChild);
   cg()->decReferenceCount(secondChild);

   return;
   }

void
TR_S390BinaryCommutativeAnalyser::longAddAnalyser(TR::Node * root, TR::InstOpCode::Mnemonic copyOpCode)
   {
   TR_ASSERT(TR::Compiler->target.is32Bit(), " should call integerAddAnalyser() for 64Bit code-gen!");
   TR::Node * firstChild;
   TR::Node * secondChild;
   TR::Register * highSum = NULL;
   TR::LabelSymbol * doneLAdd = TR::LabelSymbol::create(cg()->trHeapMemory(),cg());
   TR::Instruction * cursor = NULL;
   TR::RegisterDependencyConditions * dependencies = NULL;
   TR::Register * twoLow;
   TR::Register * twoHigh;
   TR::Register * oneLow;
   TR::Register * oneHigh;
   TR::InstOpCode::Mnemonic regToRegOpCode = TR::InstOpCode::ALR;
   TR::InstOpCode::Mnemonic memToRegOpCode = TR::InstOpCode::AL;
   bool setsOrReadsCC = NEED_CC(root) || (root->getOpCodeValue() == TR::luaddc);
   TR::Compilation *comp = cg()->comp();

   char * CLOBBER_EVAL = "LR=Clobber_eval";
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
   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();

   bool firstHighZero = false;
   bool secondHighZero = false;
   bool useFirstHighOrder = false;
   bool useSecondHighOrder = false;
   bool zArchTrexSupported = performTransformation(comp, "O^O Use AL/ALC for long add.");

   // Can generate better code for long adds when one or more children have a high order zero word
   // can avoid the evaluation when we don't need the result of such nodes for another parent.
   //

   setInputs(firstChild, firstRegister, secondChild, secondRegister,
             false, false, comp);

   if (getEvalChild1())
      {
      firstRegister = cg()->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = cg()->evaluate(secondChild);
      }

   remapInputs(firstChild, firstRegister, secondChild, secondRegister);

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

   if (root->getOpCodeValue() == TR::luaddc &&
       TR_S390ComputeCC::setCarryBorrow(root->getChild(2), false, cg()))
      {
      // use ALCR rather than ALR
      //     ALC rather than AL
      regToRegOpCode = TR::InstOpCode::ALCR;
      memToRegOpCode = TR::InstOpCode::ALC;
      }

   if (getOpReg1Reg2())
      {
      if (!firstHighZero)
         {
         oneLow = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow = firstRegister;
         oneHigh = cg()->allocateRegister();
         }

      if (!secondHighZero)
         {
         twoLow = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow = secondRegister;
         twoHigh = 0;
         }

      if ((ENABLE_ZARCH_FOR_32 && zArchTrexSupported) || setsOrReadsCC)
         {
         // However carry bits are difficult to handle without using add with carry
         // instructions and hence code has been re-enabled for handling luaddc and luadd under computeCC.
         //
         generateRRInstruction(cg(), regToRegOpCode, root, oneLow, twoLow);
         if (firstHighZero)
            {
            if (secondHighZero)
               {
               generateRRInstruction(cg(), TR::InstOpCode::XR, root, oneHigh, oneHigh);
               }
            else
               {
               generateRRInstruction(cg(), TR::InstOpCode::LR, root, oneHigh, twoHigh);
               }
            }
         else
            {
            if (secondHighZero)
               {
                 //            generateRIInstruction(cg(), TR::InstOpCode::AHI, root, oneHigh, 0);
               }
            else
               {
               generateRRInstruction(cg(), TR::InstOpCode::ALCR, root, oneHigh, twoHigh);
               }
            }
         }
      else
         {
         if (firstHighZero)
            {
            if (secondHighZero)
               {
               generateRRInstruction(cg(), TR::InstOpCode::XR, root, oneHigh, oneHigh);
               }
            else
               {
               generateRRInstruction(cg(), TR::InstOpCode::LR, root, oneHigh, twoHigh);
               }
            }
         else
            {
            if (secondHighZero)
               {
               //            generateRIInstruction(cg(), TR::InstOpCode::AHI, root, oneHigh, 0);
               }
            else
               {
               generateRRInstruction(cg(), TR::InstOpCode::AR, root, oneHigh, twoHigh);
               }
            }
         }
      TR::RegisterPair * tempReg = cg()->allocateConsecutiveRegisterPair(oneLow, oneHigh);

      dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg());
      dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(oneHigh, TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(oneLow, TR::RealRegister::LegalOddOfPair);

      if (!((ENABLE_ZARCH_FOR_32 && zArchTrexSupported) || setsOrReadsCC))
         {
         cursor = generateRRInstruction(cg(), TR::InstOpCode::ALR, root, oneLow, twoLow);
         }

      highSum = oneHigh;
      root->setRegister(tempReg);
      }
   else if (getOpReg2Reg1())
      {
      if (!firstHighZero)
         {
         oneLow = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow = secondRegister;
         twoHigh = cg()->allocateRegister();
         }

      if ((ENABLE_ZARCH_FOR_32 && zArchTrexSupported) || setsOrReadsCC)
         {
         generateRRInstruction(cg(), regToRegOpCode, root, twoLow, oneLow);
         if (firstHighZero)
            {
            if (secondHighZero)
               {
               generateRRInstruction(cg(), TR::InstOpCode::XR, root, twoHigh, twoHigh);
               }
            }
         else
            {
            if (secondHighZero)
               {
               generateRRInstruction(cg(), TR::InstOpCode::LR, root, twoHigh, oneHigh);
               }
            else
               {
               generateRRInstruction(cg(), TR::InstOpCode::ALCR, root, twoHigh, oneHigh);
               }
            }
         }
      else
         {

         if (firstHighZero)
            {
            if (secondHighZero)
               {
               generateRRInstruction(cg(), TR::InstOpCode::XR, root, twoHigh, twoHigh);
               }
            }
         else
            {
            if (secondHighZero)
               {
               generateRRInstruction(cg(), TR::InstOpCode::LR, root, twoHigh, oneHigh);
               }
            else
               {
               generateRRInstruction(cg(), TR::InstOpCode::AR, root, twoHigh, oneHigh);
               }
            }
         }

      TR::RegisterPair * tempReg = cg()->allocateConsecutiveRegisterPair(twoLow, twoHigh);

      dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg());
      dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(twoHigh, TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(twoLow, TR::RealRegister::LegalOddOfPair);

      if (!((ENABLE_ZARCH_FOR_32 && zArchTrexSupported) || setsOrReadsCC))
         {
         cursor = generateRRInstruction(cg(), TR::InstOpCode::ALR, root, twoLow, oneLow);
         }

      highSum = twoHigh;
      root->setRegister(tempReg);
      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      TR::Register * lowRegister = cg()->allocateRegister();
      TR::Register * highRegister = cg()->allocateRegister();

      TR::RegisterPair * tempReg = cg()->allocateConsecutiveRegisterPair(lowRegister, highRegister);

      highSum = highRegister;

      dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg());
      dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(highRegister, TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(lowRegister, TR::RealRegister::LegalOddOfPair);

      if (!firstHighZero)
         {
         oneLow = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow = secondRegister;
         twoHigh = 0;
         }

      if ((ENABLE_ZARCH_FOR_32 && zArchTrexSupported) || setsOrReadsCC)
         {
         generateRRInstruction(cg(), TR::InstOpCode::LR, root, lowRegister, oneLow);
         generateRRInstruction(cg(), regToRegOpCode, root, lowRegister, twoLow);

         if (!firstHighZero)
            {
            generateRRInstruction(cg(), TR::InstOpCode::LR, root, highRegister, oneHigh);
            }
         else
            {
            generateRRInstruction(cg(), TR::InstOpCode::XR, root, highRegister, highRegister);
            }

         if (secondHighZero)
            {
            //         generateRIInstruction(cg(), TR::InstOpCode::AHI, root, highRegister, 0);
            }
         else
            {
            generateRRInstruction(cg(), TR::InstOpCode::ALCR, root, highRegister, twoHigh);
            }
         }
      else
         {
         generateRRInstruction(cg(), TR::InstOpCode::LR, root, lowRegister, oneLow);

         if (!firstHighZero)
            {
            generateRRInstruction(cg(), TR::InstOpCode::LR, root, highRegister, oneHigh);
            }
         else
            {
            generateRRInstruction(cg(), TR::InstOpCode::XR, root, highRegister, highRegister);
            }

         if (secondHighZero)
            {
            //         generateRIInstruction(cg(), TR::InstOpCode::AHI, root, highRegister, 0);
            }
         else
            {
            generateRRInstruction(cg(), TR::InstOpCode::AR, root, highRegister, twoHigh);
            }

         cursor = generateRRInstruction(cg(), TR::InstOpCode::ALR, root, lowRegister, twoLow);

         highSum = highRegister;
         }

      root->setRegister(tempReg);
      }
   else // RX instruction
      {
      TR_ASSERT(!getInvalid(), "TR_S390CommutativeBinaryAnalyser::invalid case\n");

      TR::MemoryReference * lowMR = NULL;
      TR::MemoryReference * highMR = NULL;
      TR::Register * targetReg;
      TR::Register * targetLow;
      TR::Register * targetHigh;

      bool targetHighZero;
      bool memHighZero;
      bool useMemHighOrder;

      bool secondOperandIsInRegister = false;
      TR::Register * longRegister = NULL;
      bool createdClobberPair = false;

      if (getOpReg1Mem2() || getOpReg3Mem2())
         {
         if (!secondHighZero && secondChild->getSymbolReference()->isUnresolved())
            {
            secondOperandIsInRegister = true;
            lloadHelper(secondChild, cg(), NULL);
            longRegister = secondChild->getRegister();
            }
         else
            {
            highMR = generateS390MemoryReference(secondChild, cg());
            if (!secondHighZero || (secondChild->getType().isInt64()))
               {
               lowMR = generateS390MemoryReference(*highMR, 4, cg());
               }
            else
               {
               lowMR = highMR;
               }
            }

         if (getOpReg3Mem2())
            {
            if (firstHighZero)
               {
               targetReg = allocateAddSubRegister(root, firstRegister);
               generateRRInstruction(cg(), copyOpCode, root, targetReg, firstRegister);
               }
            else
               {
               createdClobberPair = true;
               TR::Register * lowRegister = allocateAddSubRegister(root, firstRegister->getLowOrder());
               TR::Register * highRegister = allocateAddSubRegister(root, firstRegister->getHighOrder());
               targetReg = cg()->allocateConsecutiveRegisterPair(lowRegister, highRegister);
               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getLowOrder(), firstRegister->getLowOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }

               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getHighOrder(), firstRegister->getHighOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }
               }
            }
         else
            {
            targetReg = firstRegister;
            }

         targetHighZero = firstHighZero;
         memHighZero = secondHighZero;
         useMemHighOrder = useSecondHighOrder;
         }
      else
         {
         if (!firstHighZero && firstChild->getSymbolReference()->isUnresolved())
            {
            secondOperandIsInRegister = true;
            lloadHelper(firstChild, cg(), NULL);
            longRegister = firstChild->getRegister();
            }
         else
            {
            highMR = generateS390MemoryReference(firstChild, cg());
            if (!firstHighZero || (firstChild->getType().isInt64()))
               {
               lowMR = generateS390MemoryReference(*highMR, 4, cg());
               }
            else
               {
               lowMR = highMR;
               }
            }

         if (getOpReg3Mem1())
            {
            if (secondHighZero)
               {
               targetReg = allocateAddSubRegister(root, secondRegister);
               generateRRInstruction(cg(), copyOpCode, root, targetReg, secondRegister);
               }
            else
               {
               createdClobberPair = true;
               TR::Register * lowRegister = allocateAddSubRegister(root, secondRegister->getLowOrder());
               TR::Register * highRegister = allocateAddSubRegister(root, secondRegister->getHighOrder());
               targetReg = cg()->allocateConsecutiveRegisterPair(lowRegister, highRegister);
               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getLowOrder(), secondRegister->getLowOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }

               cursor = generateRRInstruction(cg(), copyOpCode, root,
                                      targetReg->getHighOrder(), secondRegister->getHighOrder());
               if (debugObj)
                  {
                  debugObj->addInstructionComment(toS390RRInstruction(cursor), CLOBBER_EVAL);
                  }
               }
            }
         else
            {
            targetReg = secondRegister;
            }

         targetHighZero = secondHighZero;
         memHighZero = firstHighZero;
         useMemHighOrder = useFirstHighOrder;
         }

      if (targetHighZero)
         {
         targetLow = targetReg;
         targetHigh = cg()->allocateRegister();
         generateRRInstruction(cg(), TR::InstOpCode::XR, root, targetHigh, targetHigh);
         }
      else
         {
         targetLow = targetReg->getLowOrder();
         targetHigh = targetReg->getHighOrder();
         }

      TR::RegisterPair * tempReg = NULL;
      if (createdClobberPair)
         {
         tempReg = (TR::RegisterPair *)targetReg;
         }
      else
         {
         tempReg = cg()->allocateConsecutiveRegisterPair(targetLow, targetHigh);
         }

      dependencies = new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg());
      dependencies->addPostCondition(tempReg, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(targetHigh, TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(targetLow, TR::RealRegister::LegalOddOfPair);

      if (highMR)
         {
         dependencies->addAssignAnyPostCondOnMemRef(highMR);
         }

      if ((ENABLE_ZARCH_FOR_32 && zArchTrexSupported) || setsOrReadsCC)
         {
         if (secondOperandIsInRegister)
            {
            generateRRInstruction(cg(), regToRegOpCode, root, targetLow, longRegister->getLowOrder());
            }
         else
            {
            generateRXInstruction(cg(), memToRegOpCode, root, targetLow, lowMR);
            }

         if (secondOperandIsInRegister)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::ALCR, root, targetHigh, longRegister->getHighOrder());
            }
         else
            {
            cursor = generateRXInstruction(cg(), TR::InstOpCode::ALC, root, targetHigh, highMR);
            }
         }
      else
         {
         if (!memHighZero)
            {
            if (secondOperandIsInRegister)
               {
               generateRRInstruction(cg(), TR::InstOpCode::AR, root, targetHigh, longRegister->getHighOrder());
               }
            else
               {
               generateRXInstruction(cg(), TR::InstOpCode::A, root, targetHigh, highMR);
               }
            }

         if (secondOperandIsInRegister)
            {
            cursor = generateRRInstruction(cg(), TR::InstOpCode::ALR, root, targetLow, longRegister->getLowOrder());
            }
         else
            {
            cursor = generateRXInstruction(cg(), TR::InstOpCode::AL, root, targetLow, lowMR);
            }
         }

      highSum = targetHigh;

      root->setRegister(tempReg);

      if (!secondOperandIsInRegister)
         {
         highMR->stopUsingMemRefRegister(cg());
         }

      if (!getOpReg1Mem2())
         {
         notReversedOperands();
         }
      }

      if (!((ENABLE_ZARCH_FOR_32 && zArchTrexSupported) || setsOrReadsCC))
      {
      // Check for overflow in LS int. If overflow, increment MS int.
      generateS390BranchInstruction(cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK12, root, doneLAdd);

      // Increment MS int due to overflow in LS int
      generateRIInstruction(cg(), TR::InstOpCode::AHI, root, highSum, 1);

      generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, root, doneLAdd, dependencies);
      }

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
