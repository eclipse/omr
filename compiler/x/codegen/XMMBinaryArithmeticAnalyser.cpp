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

#include "x/codegen/XMMBinaryArithmeticAnalyser.hpp"

#include <stddef.h>                                   // for NULL
#include <stdint.h>                                   // for uint8_t
#include "codegen/CodeGenerator.hpp"                  // for CodeGenerator
#include "codegen/Machine.hpp"                        // for Machine
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"                       // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/TreeEvaluator.hpp"                  // for TreeEvaluator
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                               // for ILOpCode, etc
#include "il/Node.hpp"                                // for Node
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"                           // for TR_ASSERT
#include "x/codegen/FPBinaryArithmeticAnalyser.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                       // for ::BADIA32Op, etc

namespace TR { class SymbolReference; }

static TR::Register *copyRegister(TR::Node *root, TR::Register *reg, TR::CodeGenerator *cg)
   {
   TR::Register *copyReg;
   TR_ASSERT(reg->getKind() == TR_FPR, "incorrect register type\n");
   if (reg->isSinglePrecision())
      {
      copyReg = cg->allocateSinglePrecisionRegister(TR_FPR);
      generateRegRegInstruction(MOVSSRegReg, root, copyReg, reg, cg);
      }
   else
      {
      copyReg = cg->allocateRegister(TR_FPR);
      generateRegRegInstruction(MOVSDRegReg, root, copyReg, reg, cg);
      }
   return copyReg;
   }

void TR_X86XMMBinaryArithmeticAnalyser::setInputs(TR::Node     *firstChild,
                                                  TR::Register *firstRegister,
                                                  TR::Node     *secondChild,
                                                  TR::Register *secondRegister)
   {
   _inputs = 0;

   if (firstRegister)
      {
      setReg1();
      }

   if (secondRegister)
      {
      setReg2();
      }

   if (firstChild->getReferenceCount() == 1)
      {
      setClob1();

      if (!(_codeGen->useSSEForSinglePrecision() && _codeGen->useSSEForDoublePrecision()) && firstChild->getOpCode().isMemoryReference())
         {
         setMem1();
         }
      }

   if (secondChild->getReferenceCount() == 1)
      {
      setClob2();

      if (!(_codeGen->useSSEForSinglePrecision() && _codeGen->useSSEForDoublePrecision()) && secondChild->getOpCode().isMemoryReference())
         {
         setMem2();
         }
      }

   if (getPackage() == kFADDpackage || getPackage() == kDADDpackage ||
       getPackage() == kFMULpackage || getPackage() == kDMULpackage)
      {
      setCommutative();
      }
   }


uint8_t
TR_X86XMMBinaryArithmeticAnalyser::getX86XMMOpPackage(TR::Node *node)
   {
   uint8_t     package;
   TR::ILOpCode op = node->getOpCode();

   switch (op.getOpCodeValue())
      {
      case TR::fadd:  package = kFADDpackage; break;
      case TR::dadd:  package = kDADDpackage; break;
      case TR::fsub:  package = kFSUBpackage; break;
      case TR::dsub:  package = kDSUBpackage; break;
      case TR::fmul:  package = kFMULpackage; break;
      case TR::dmul:  package = kDMULpackage; break;
      case TR::fdiv:  package = kFDIVpackage; break;
      case TR::ddiv:  package = kDDIVpackage; break;
      default:       package = kBADpackage;  break;
      }

   return package;
   }


void TR_X86XMMBinaryArithmeticAnalyser::genericXMMAnalyser(TR::Node *root)
   {
   TR::Node                *targetChild = root->getFirstChild(),
                          *sourceChild = root->getSecondChild();
   TR::Register            *targetRegister = targetChild->getRegister(),
                          *sourceRegister = sourceChild->getRegister(),
                          *tempReg;
   TR::SymbolReference     *targetSymRef = NULL,
                          *sourceSymRef = NULL;
   TR::MemoryReference  *tempMR;

   do
      {
      setInputs(targetChild, targetRegister, sourceChild, sourceRegister);

      if (evaluatesTarget())
         (void)_codeGen->evaluate(targetChild);

      if (evaluatesSource())
         (void)_codeGen->evaluate(sourceChild);

      targetRegister = targetChild->getRegister();
      sourceRegister = sourceChild->getRegister();
      }
   while (resetsInputs());

   // If both operands are on the x87 FP stack, then generate x87 code instead.
   //
   if (targetRegister && targetRegister->getKind() == TR_X87 &&
       sourceRegister && sourceRegister->getKind() == TR_X87)
      {
      TR_X86FPBinaryArithmeticAnalyser  temp(root, _codeGen);
      temp.genericFPAnalyser(root);
      return;
      }

   if (isReversed() ||
       (isCommutative() && canUseRegMemOp() &&
        targetRegister && targetRegister->getKind() == TR_X87 && targetChild->getReferenceCount() == 1 &&
        sourceRegister && sourceRegister->getKind() == TR_FPR))
      {
      TR_ASSERT(sourceRegister, "source register cannot be NULL\n");

      if (sourceRegister->getKind() == TR_X87)
         sourceRegister = TR::TreeEvaluator::coerceFPRToXMMR(sourceChild, sourceRegister, _codeGen);

      if (mustUseRegRegOp())
         {
         TR_ASSERT(targetRegister, "target register cannot be NULL\n");

         if (targetRegister->getKind() == TR_X87)
            targetRegister = TR::TreeEvaluator::coerceFPRToXMMR(targetChild, targetRegister, _codeGen);

         generateRegRegInstruction(getRegRegOp(), root, sourceRegister, targetRegister, _codeGen);
         }
      else if (canUseRegMemOp())
         {
      TR::MemoryReference  *tempMR;

         if (targetRegister &&
             targetRegister->getKind() == TR_X87 &&
             !targetChild->getOpCode().isMemoryReference())
            {
            if (targetRegister->isSinglePrecision())
               {
               tempMR = _codeGen->machine()->getDummyLocalMR(TR::Float);
               generateFPMemRegInstruction(FSTPMemReg, root, tempMR, targetRegister, _codeGen);
               }
            else
               {
               tempMR = _codeGen->machine()->getDummyLocalMR(TR::Double);
               generateFPMemRegInstruction(DSTPMemReg, root, tempMR, targetRegister, _codeGen);
               }
            tempMR = generateX86MemoryReference(*tempMR, 0, _codeGen);
            }
         else if (targetRegister && targetRegister->getKind() == TR_FPR)
            {
            tempMR = NULL;
            }
         else
            {
            TR_ASSERT(targetChild->getOpCode().isMemoryReference(), "target operand should be a memory load\n");
            tempMR = generateX86MemoryReference(targetChild, _codeGen);
            }

         if (tempMR)
            {
            generateRegMemInstruction(getRegMemOp(), root, sourceRegister, tempMR, _codeGen);
            tempMR->decNodeReferenceCounts(_codeGen);
            }
         else
            {
            generateRegRegInstruction(getRegRegOp(), root, sourceRegister, targetRegister, _codeGen);
            }
         }
      else
         TR_ASSERT(0, "missing XMM arithmetic action\n");

      root->setRegister(sourceRegister);
      }
   else
      {
      TR_ASSERT(targetRegister, "target register cannot be NULL\n");

      if (targetRegister->getKind() == TR_X87)
         targetRegister = TR::TreeEvaluator::coerceFPRToXMMR(targetChild, targetRegister, _codeGen);

      // Make the target register clobberable if it isn't already.
      //
      if (copiesTargetReg())
         targetRegister = copyRegister(root, targetRegister, _codeGen);

      if (mustUseRegRegOp())
         {
         TR_ASSERT(sourceRegister, "source register cannot be NULL\n");

         if (sourceRegister->getKind() == TR_X87)
            sourceRegister = TR::TreeEvaluator::coerceFPRToXMMR(sourceChild, sourceRegister, _codeGen);

         generateRegRegInstruction(getRegRegOp(), root, targetRegister, sourceRegister, _codeGen);
         }
      else if (canUseRegMemOp())
         {
      TR::MemoryReference  *tempMR;

         if (sourceRegister &&
             sourceRegister->getKind() == TR_X87 &&
             !sourceChild->getOpCode().isMemoryReference())
            {
            if (sourceRegister->isSinglePrecision())
               {
               tempMR = _codeGen->machine()->getDummyLocalMR(TR::Float);
               generateFPMemRegInstruction(FSTPMemReg, root, tempMR, sourceRegister, _codeGen);
               }
            else
               {
               tempMR = _codeGen->machine()->getDummyLocalMR(TR::Double);
               generateFPMemRegInstruction(DSTPMemReg, root, tempMR, sourceRegister, _codeGen);
               }
            tempMR = generateX86MemoryReference(*tempMR, 0, _codeGen);
            }
         else if (sourceRegister && sourceRegister->getKind() == TR_FPR)
            {
            tempMR = NULL;
            }
         else
            {
            TR_ASSERT(sourceChild->getOpCode().isMemoryReference(), "source operand should be a memory load\n");
            tempMR = generateX86MemoryReference(sourceChild, _codeGen);
            }

         if (tempMR)
            {
            generateRegMemInstruction(getRegMemOp(), root, targetRegister, tempMR, _codeGen);
            tempMR->decNodeReferenceCounts(_codeGen);
            }
         else
            {
            generateRegRegInstruction(getRegRegOp(), root, targetRegister, sourceRegister, _codeGen);
            }
         }
      else
         TR_ASSERT(0, "missing XMM arithmetic action\n");

      root->setRegister(targetRegister);
      }

   _codeGen->decReferenceCount(sourceChild);
   _codeGen->decReferenceCount(targetChild);

   return;
   }


const TR_X86OpCodes TR_X86XMMBinaryArithmeticAnalyser::_opCodePackage[kNumXMMArithPackages][kNumXMMArithVariants] =
   {
   // PACKAGE
   //        regReg        regMem

   // Unknown
           { BADIA32Op,    BADIA32Op },

   // fadd
           { ADDSSRegReg,  ADDSSRegMem },

   // dadd
           { ADDSDRegReg,  ADDSDRegMem },

   // fmul
           { MULSSRegReg,  MULSSRegMem },

   // dmul
           { MULSDRegReg,  MULSDRegMem },

   // fsub
           { SUBSSRegReg,  SUBSSRegMem },

   // dsub
           { SUBSDRegReg,  SUBSDRegMem },

   // fdiv
           { DIVSSRegReg,  DIVSSRegMem },

   // ddiv
           { DIVSDRegReg,  DIVSDRegMem },
   };


const uint8_t TR_X86XMMBinaryArithmeticAnalyser::_actionMap[NUM_XMM_ACTION_SETS] =
   {
   // This table occupies NUM_XMM_ACTION_SETS bytes of storage.

   // CM = Operation is commutative
   // RT = Target in XMMR           RS = Source in XMMR
   // MT = Target in memory         MS = Source in memory
   // CT = Target is clobberable    CS = Source is clobberable

   // kReverse and kCopyTarget are mutually exclusive!
   // kReverse is only possible if CM=1.

   // CM RT MT CT RS MS CS
   /* 0  0  0  0  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  0  0  0  0  1 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  0  0  0  1  0 */    kEvalTarget | kEvalSource | kCopyTarget | kRegReg,
   /* 0  0  0  0  0  1  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 0  0  0  0  1  0  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 0  0  0  0  1  0  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 0  0  0  0  1  1  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 0  0  0  0  1  1  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 0  0  0  1  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  0  1  0  0  1 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  0  1  0  1  0 */    kEvalTarget | kEvalSource | kRegReg,
   /* 0  0  0  1  0  1  1 */    kEvalTarget | kRegMem,
   /* 0  0  0  1  1  0  0 */    kEvalTarget | kRegReg,
   /* 0  0  0  1  1  0  1 */    kEvalTarget | kRegMem,
   /* 0  0  0  1  1  1  0 */    kEvalTarget | kRegReg,
   /* 0  0  0  1  1  1  1 */    kEvalTarget | kRegMem,
   /* 0  0  1  0  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  1  0  0  0  1 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  1  0  0  1  0 */    kEvalTarget | kEvalSource | kCopyTarget | kRegReg,
   /* 0  0  1  0  0  1  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 0  0  1  0  1  0  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 0  0  1  0  1  0  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 0  0  1  0  1  1  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 0  0  1  0  1  1  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 0  0  1  1  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  1  1  0  0  1 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 0  0  1  1  0  1  0 */    kEvalTarget | kEvalSource | kRegReg,
   /* 0  0  1  1  0  1  1 */    kEvalTarget | kRegMem,
   /* 0  0  1  1  1  0  0 */    kEvalTarget | kRegReg,
   /* 0  0  1  1  1  0  1 */    kEvalTarget | kRegMem,
   /* 0  0  1  1  1  1  0 */    kEvalTarget | kRegReg,
   /* 0  0  1  1  1  1  1 */    kEvalTarget | kRegMem,
   /* 0  1  0  0  0  0  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 0  1  0  0  0  0  1 */    kEvalSource | kCopyTarget | kRegMem,
   /* 0  1  0  0  0  1  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 0  1  0  0  0  1  1 */    kCopyTarget | kRegMem,
   /* 0  1  0  0  1  0  0 */    kCopyTarget | kRegReg,
   /* 0  1  0  0  1  0  1 */    kCopyTarget | kRegMem,
   /* 0  1  0  0  1  1  0 */    kCopyTarget | kRegReg,
   /* 0  1  0  0  1  1  1 */    kCopyTarget | kRegMem,
   /* 0  1  0  1  0  0  0 */    kEvalSource | kRegReg,
   /* 0  1  0  1  0  0  1 */    kEvalSource | kRegMem,
   /* 0  1  0  1  0  1  0 */    kEvalSource | kRegReg,
   /* 0  1  0  1  0  1  1 */    kRegMem,
   /* 0  1  0  1  1  0  0 */    kRegReg,
   /* 0  1  0  1  1  0  1 */    kRegMem,
   /* 0  1  0  1  1  1  0 */    kRegReg,
   /* 0  1  0  1  1  1  1 */    kRegMem,
   /* 0  1  1  0  0  0  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 0  1  1  0  0  0  1 */    kEvalSource | kCopyTarget | kRegMem,
   /* 0  1  1  0  0  1  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 0  1  1  0  0  1  1 */    kCopyTarget | kRegMem,
   /* 0  1  1  0  1  0  0 */    kCopyTarget | kRegReg,
   /* 0  1  1  0  1  0  1 */    kCopyTarget | kRegMem,
   /* 0  1  1  0  1  1  0 */    kCopyTarget | kRegReg,
   /* 0  1  1  0  1  1  1 */    kCopyTarget | kRegMem,
   /* 0  1  1  1  0  0  0 */    kEvalSource | kRegReg,
   /* 0  1  1  1  0  0  1 */    kEvalSource | kRegMem,
   /* 0  1  1  1  0  1  0 */    kEvalSource | kRegReg,
   /* 0  1  1  1  0  1  1 */    kRegMem,
   /* 0  1  1  1  1  0  0 */    kRegReg,
   /* 0  1  1  1  1  0  1 */    kRegMem,
   /* 0  1  1  1  1  1  0 */    kRegReg,
   /* 0  1  1  1  1  1  1 */    kRegMem,
   /* 1  0  0  0  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 1  0  0  0  0  0  1 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 1  0  0  0  0  1  0 */    kEvalTarget | kEvalSource | kCopyTarget | kRegReg,
   /* 1  0  0  0  0  1  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 1  0  0  0  1  0  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 1  0  0  0  1  0  1 */    kEvalTarget | kReverse | kRegReg,
   /* 1  0  0  0  1  1  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 1  0  0  0  1  1  1 */    kEvalTarget | kReverse | kRegReg,
   /* 1  0  0  1  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 1  0  0  1  0  0  1 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 1  0  0  1  0  1  0 */    kEvalTarget | kEvalSource | kRegReg,
   /* 1  0  0  1  0  1  1 */    kEvalTarget | kRegMem,
   /* 1  0  0  1  1  0  0 */    kEvalTarget | kRegReg,
   /* 1  0  0  1  1  0  1 */    kEvalTarget | kReverse | kRegMem,
   /* 1  0  0  1  1  1  0 */    kEvalTarget | kRegReg,
   /* 1  0  0  1  1  1  1 */    kEvalTarget | kReverse | kRegMem,
   /* 1  0  1  0  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 1  0  1  0  0  0  1 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 1  0  1  0  0  1  0 */    kEvalTarget | kEvalSource | kCopyTarget | kRegReg,
   /* 1  0  1  0  0  1  1 */    kEvalTarget | kCopyTarget | kRegMem,
   /* 1  0  1  0  1  0  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 1  0  1  0  1  0  1 */    kEvalTarget | kReverse | kRegReg,
   /* 1  0  1  0  1  1  0 */    kEvalTarget | kCopyTarget | kRegReg,
   /* 1  0  1  0  1  1  1 */    kEvalTarget | kReverse | kRegReg,
   /* 1  0  1  1  0  0  0 */    kEvalTarget | kEvalSource | kResetInputs,
   /* 1  0  1  1  0  0  1 */    kEvalSource | kReverse | kRegMem,
   /* 1  0  1  1  0  1  0 */    kEvalTarget | kEvalSource | kRegReg,
   /* 1  0  1  1  0  1  1 */    kEvalTarget | kRegMem,
   /* 1  0  1  1  1  0  0 */    kEvalTarget | kRegReg,
   /* 1  0  1  1  1  0  1 */    kReverse | kRegMem,
   /* 1  0  1  1  1  1  0 */    kEvalTarget | kRegReg,
   /* 1  0  1  1  1  1  1 */    kReverse | kRegMem,

   /* 1  1  0  0  0  0  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 1  1  0  0  0  0  1 */    kEvalSource | kReverse | kRegReg,
   /* 1  1  0  0  0  1  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 1  1  0  0  0  1  1 */    kCopyTarget | kRegMem,
   /* 1  1  0  0  1  0  0 */    kCopyTarget | kRegReg,
   /* 1  1  0  0  1  0  1 */    kReverse | kRegReg,
   /* 1  1  0  0  1  1  0 */    kCopyTarget | kRegReg,
   /* 1  1  0  0  1  1  1 */    kReverse | kRegReg,
   /* 1  1  0  1  0  0  0 */    kEvalSource | kRegReg,
   /* 1  1  0  1  0  0  1 */    kEvalSource | kRegMem,
   /* 1  1  0  1  0  1  0 */    kEvalSource | kRegReg,
   /* 1  1  0  1  0  1  1 */    kRegMem,
   /* 1  1  0  1  1  0  0 */    kRegReg,
   /* 1  1  0  1  1  0  1 */    kRegMem,
   /* 1  1  0  1  1  1  0 */    kRegReg,
   /* 1  1  0  1  1  1  1 */    kRegMem,
   /* 1  1  1  0  0  0  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 1  1  1  0  0  0  1 */    kEvalSource | kReverse | kRegReg,
   /* 1  1  1  0  0  1  0 */    kEvalSource | kCopyTarget | kRegReg,
   /* 1  1  1  0  0  1  1 */    kCopyTarget | kRegMem,
   /* 1  1  1  0  1  0  0 */    kCopyTarget | kRegReg,
   /* 1  1  1  0  1  0  1 */    kReverse | kRegReg,
   /* 1  1  1  0  1  1  0 */    kCopyTarget | kRegReg,
   /* 1  1  1  0  1  1  1 */    kReverse | kRegReg,
   /* 1  1  1  1  0  0  0 */    kEvalSource | kRegReg,
   /* 1  1  1  1  0  0  1 */    kEvalSource | kRegMem,
   /* 1  1  1  1  0  1  0 */    kEvalSource | kRegReg,
   /* 1  1  1  1  0  1  1 */    kRegMem,
   /* 1  1  1  1  1  0  0 */    kRegReg,
   /* 1  1  1  1  1  0  1 */    kRegMem,
   /* 1  1  1  1  1  1  0 */    kRegReg,
   /* 1  1  1  1  1  1  1 */    kRegMem
   };

