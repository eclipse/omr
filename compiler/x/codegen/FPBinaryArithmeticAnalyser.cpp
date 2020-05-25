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

#include "x/codegen/FPBinaryArithmeticAnalyser.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

uint8_t
TR_X86FPBinaryArithmeticAnalyser::getIA32FPOpPackage(TR::Node *node)
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

void TR_X86FPBinaryArithmeticAnalyser::setInputs(TR::Node     *firstChild,
                                                  TR::Register *firstRegister,
                                                  TR::Node     *secondChild,
                                                  TR::Register *secondRegister)
   {

   if (firstRegister)
      {
      setReg1();
      }

   if (secondRegister)
      {
      setReg2();
      }

   if (firstChild->getOpCode().isMemoryReference() &&
       firstChild->getReferenceCount() == 1)
      {
      setMem1();
      }

   if (secondChild->getOpCode().isMemoryReference() &&
       secondChild->getReferenceCount() == 1)
      {
      setMem2();
      }

   if ((firstChild->getReferenceCount() == 1) &&
       isIntToFPConversion(firstChild))
      {
      setConv1();
      }

   if ((secondChild->getReferenceCount() == 1) &&
       isIntToFPConversion(secondChild))
      {
      setConv2();
      }

   if (firstChild->getReferenceCount() == 1)
      {
      setClob1();
      }

   if (secondChild->getReferenceCount() == 1)
      {
      setClob2();
      }
   }


uint8_t
TR_X86FPBinaryArithmeticAnalyser::getIA32FPOpPackage(TR::ILOpCodes op)
   {
   uint8_t package = 0;

   switch (op)
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


bool
TR_X86FPBinaryArithmeticAnalyser::isIntToFPConversion(TR::Node *child)
   {
   /* conversion to float can't use the direct memory float int
      instructions because of precision adjustment. We have to
      load them in registers and do spill/reload to truncate the
      precision */
   if (/*child->getOpCodeValue() == TR::i2f ||
       child->getOpCodeValue() == TR::s2f ||*/
       child->getOpCodeValue() == TR::i2d ||
       child->getOpCodeValue() == TR::s2d)
      {
      TR::Node *convChild = child->getFirstChild();
      if (convChild->getRegister() == NULL && convChild->getReferenceCount() == 1)
         {
         if (convChild->getOpCode().isLoadVar())
            {
            return true;
            }
         }
      }

   return false;
   }


void TR_X86FPBinaryArithmeticAnalyser::genericFPAnalyser(TR::Node *root)
   {

   TR::Register         *targetRegister     = NULL,
                        *sourceRegister     = NULL,
                        *tempReg            = NULL;
   TR::Node             *targetChild        = NULL,
                        *sourceChild        = NULL,
                        *opChild[2]         = {NULL, NULL};
   TR::Compilation      *comp               = _cg->comp();
   TR::MemoryReference  *constMR            = NULL;
   bool                 operandNeedsScaling = false;
   TR::Register         *scalingRegister    = NULL;

   opChild[0] = root->getFirstChild();
   opChild[1] = root->getSecondChild();

   do
      {
      setInputs(opChild[0], opChild[0]->getRegister(),
                opChild[1], opChild[1]->getRegister());

      if (isEvalTarget())
         targetRegister = _cg->evaluate(opChild[0]);

      if (isEvalSource())
         sourceRegister = _cg->evaluate(opChild[1]);
      }
   while (isEvalTarget() || isEvalSource()); // TODO improve this by optimizing the action map

   targetChild = opChild[ getOpsReversed() ];
   sourceChild = opChild[ getOpsReversed() ^ 1 ];
   targetRegister = targetChild->getRegister();
   sourceRegister = sourceChild->getRegister();

   // The operands may need a store-reload precision adjustment, especially if they were
   // produced from an arithmetic instruction.
   //
   // Only floats require this under extended exponent mode, and both floats and doubles
   // require this under strictfp mode.
   //
   if (targetRegister && targetRegister->needsPrecisionAdjustment())
      {
      TR::TreeEvaluator::insertPrecisionAdjustment(targetRegister, root, _cg);
      }

   if (sourceRegister && sourceRegister->needsPrecisionAdjustment())
      {
      TR::TreeEvaluator::insertPrecisionAdjustment(sourceRegister, root, _cg);
      }

   // For strictfp double precision multiplication and division, scale down one of the operands
   // first to prevent double denormalized mantissa rounding.
   //
   if ((comp->getCurrentMethod()->isStrictFP() || comp->getOption(TR_StrictFP)) && root->getOpCode().isDouble())
      {
      static char *scaleX87StrictFPDivides = feGetEnv("TR_scaleX87StrictFPDivides");

      if (root->getOpCode().isMul() ||
          (scaleX87StrictFPDivides && root->getOpCode().isDiv()))
         {
         scalingRegister = _cg->allocateRegister(TR_X87);

         constMR = generateX86MemoryReference(_cg->findOrCreate8ByteConstant(root, DOUBLE_EXPONENT_SCALE), _cg);

         generateFPRegMemInstruction(DLDRegMem, root, scalingRegister, constMR, _cg);
         operandNeedsScaling = true;
         }
      }

   // Make the target register clobberable if it already isn't.
   //
   if (isCopyReg())
      {
      tempReg = _cg->allocateRegister(TR_X87);
      if (targetRegister->isSinglePrecision())
         tempReg->setIsSinglePrecision();
      generateFPST0STiRegRegInstruction(FLDRegReg, root, tempReg, targetRegister, _cg);
      targetRegister = tempReg;
      }

   // Scale down the target register.
   //
   if (operandNeedsScaling)
      {
      generateFPST0ST1RegRegInstruction(FSCALERegReg, root, targetRegister, scalingRegister, _cg);
      }

   root->setRegister(targetRegister);

   if (isOpRegReg())
      {
      generateFPArithmeticRegRegInstruction(getRegRegOp(), root, targetRegister, sourceRegister, _cg);
      }
   else if (isOpRegMem())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(sourceChild, _cg);
      generateFPRegMemInstruction(getRegMemOp(), root, targetRegister, tempMR, _cg);
      tempMR->decNodeReferenceCounts(_cg);
      }
   else if (isOpRegConv())
      {
      TR_X86OpCodes          regIntOp;
      TR::Node                *intLoad = sourceChild->getFirstChild();
      TR::MemoryReference  *tempMR = generateX86MemoryReference(intLoad, _cg);

      if (sourceChild->getOpCodeValue() == TR::i2f ||
          sourceChild->getOpCodeValue() == TR::i2d)
         {
         regIntOp = getRegConvIOp();
         }
      else
         {
         regIntOp = getRegConvSOp();
         }

      generateFPRegMemInstruction(regIntOp, root, targetRegister, tempMR, _cg);
      tempMR->decNodeReferenceCounts(_cg);
      _cg->decReferenceCount(intLoad);
      }
   else
      {
      diagnostic("\nFPBinaryArithmeticAnalyser() ==> invalid instruction format!\n");
      }

   // Scale the result back up and pop the scaling constant from the FP stack.
   //
   if (operandNeedsScaling)
      {
      generateFPRegInstruction(DCHSReg, root, scalingRegister, _cg);
      generateFPST0ST1RegRegInstruction(FSCALERegReg, root, root->getRegister(), scalingRegister, _cg);
      generateFPSTiST0RegRegInstruction(FSTRegReg, root, scalingRegister, scalingRegister, _cg);
      _cg->stopUsingRegister(scalingRegister);
      }

   // The result of the binary operation will require a precision adjustment for
   // 1. floats used in extended exponent mode
   // 2. floats and doubles under strictfp mode
   // 3. floats and doubles used to compare against a constant
   // 4. doubles under forced strictFP semantics
   //
   targetRegister->setMayNeedPrecisionAdjustment();

   if ((root->getOpCode().isFloat() && !comp->getJittedMethodSymbol()->usesSinglePrecisionMode()) ||
       comp->getCurrentMethod()->isStrictFP() ||
       comp->getOption(TR_StrictFP) ||
       operandNeedsScaling)
      {
      targetRegister->setNeedsPrecisionAdjustment();
      }

   _cg->decReferenceCount(sourceChild);
   _cg->decReferenceCount(targetChild);

   return;
   }


const TR_X86OpCodes TR_X86FPBinaryArithmeticAnalyser::_opCodePackage[kNumFPPackages][kNumFPArithVariants] =
   {
   // PACKAGE
   //        reg1Reg2      reg2Reg1      reg1Mem2      reg2Mem1
   //        reg1ConvS2    reg1ConvI2    reg2ConvS1    reg2ConvI1

   // Unknown
           { BADIA32Op,    BADIA32Op,    BADIA32Op,    BADIA32Op,
             BADIA32Op,    BADIA32Op,    BADIA32Op,    BADIA32Op },

   // fadd
           { FADDRegReg,   FADDRegReg,   FADDRegMem,   FADDRegMem,
             FSADDRegMem,  FIADDRegMem,  FSADDRegMem,  FIADDRegMem },

   // dadd
           { DADDRegReg,   DADDRegReg,   DADDRegMem,   DADDRegMem,
             DSADDRegMem,  DIADDRegMem,  DSADDRegMem,  DIADDRegMem },

   // fmul
           { FMULRegReg,   FMULRegReg,   FMULRegMem,   FMULRegMem,
             FSMULRegMem,  FIMULRegMem,  FSMULRegMem,  FIMULRegMem },

   // dmul
           { DMULRegReg,   DMULRegReg,   DMULRegMem,   DMULRegMem,
             DSMULRegMem,  DIMULRegMem,  DSMULRegMem,  DIMULRegMem },

   // fsub
           { FSUBRegReg,   FSUBRRegReg,  FSUBRegMem,   FSUBRRegMem,
             FSSUBRegMem,  FISUBRegMem,  FSSUBRRegMem, FISUBRRegMem },

   // dsub
           { DSUBRegReg,   DSUBRRegReg,  DSUBRegMem,   DSUBRRegMem,
             DSSUBRegMem,  DISUBRegMem,  DSSUBRRegMem, DISUBRRegMem },

   // fdiv
           { FDIVRegReg,   FDIVRRegReg,  FDIVRegMem,   FDIVRRegMem,
             FSDIVRegMem,  FIDIVRegMem,  FSDIVRRegMem, FIDIVRRegMem },

   // ddiv
           { DDIVRegReg,   DDIVRRegReg,  DDIVRegMem,   DDIVRRegMem,
             DSDIVRegMem,  DIDIVRegMem,  DSDIVRRegMem, DIDIVRRegMem }
   };


const uint8_t TR_X86FPBinaryArithmeticAnalyser::_actionMap[NUM_ACTION_SETS] =
   {
   // This table occupies NUM_ACTION_SETS bytes of storage.

   // RT = Target in register       RS = Source in register
   // MT = Target in memory         MS = Source in memory
   // CT = Target is clobberable    CS = Source is clobberable
   // VT = Target is conversion     VS = Source is conversion

   // RT MT CT VT RS MS CS VS
   /* 0  0  0  0  0  0  0  0 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  0  0  0  1 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  0  0  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  0  0  0  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  0  0  1  0  0 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  0  1  0  1 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  0  1  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  0  0  1  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  0  1  0  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  1  0  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  1  0  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  0  0  1  0  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  0  0  1  1  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  1  1  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  0  1  1  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  0  0  1  1  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  0  1  0  0  0  0 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  0  0  0  1 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  0  0  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  1  0  0  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  1  0  1  0  0 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  0  1  0  1 */    kEvalTarget | kEvalSource | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  0  1  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  1  0  1  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  0  0  1  1  0  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  1  0  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  1  0  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  0  1  1  0  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  0  1  1  1  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  1  1  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  0  0  1  1  1  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  0  1  1  1  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  0  1  0  0  0  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  0  0  0  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  0  0  0  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  0  0  0  1  1 */    kEvalTarget | kReg1Conv2,
   /* 0  0  1  0  0  1  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  0  0  1  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  0  0  1  1  0 */    kEvalTarget | kReg1Mem2,
   /* 0  0  1  0  0  1  1  1 */    kEvalTarget | kReg1Mem2,
   /* 0  0  1  0  1  0  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  0  1  0  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  0  1  0  1  0 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  0  1  0  1  1 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  0  1  1  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  0  1  1  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  0  1  1  1  0 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  0  1  1  1  1 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  1  0  0  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  1  0  0  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  1  0  0  1  0 */    kEvalSource | kReg1Conv2 | kReverse,
   /* 0  0  1  1  0  0  1  1 */    kEvalTarget | kReg1Conv2,
   /* 0  0  1  1  0  1  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  1  0  1  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  0  1  1  0  1  1  0 */    kEvalTarget | kReg1Mem2,
   /* 0  0  1  1  0  1  1  1 */    kEvalTarget | kReg1Mem2,
   /* 0  0  1  1  1  0  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  1  1  0  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  1  1  0  1  0 */    kReg1Conv2 | kReverse,
   /* 0  0  1  1  1  0  1  1 */    kReg1Conv2 | kReverse,
   /* 0  0  1  1  1  1  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  1  1  1  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  0  1  1  1  1  1  0 */    kReg1Conv2 | kReverse,
   /* 0  0  1  1  1  1  1  1 */    kReg1Conv2 | kReverse,
   /* 0  1  0  0  0  0  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  0  0  0  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  0  0  0  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  0  0  0  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  0  0  1  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  0  0  1  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  0  0  1  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  0  0  1  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  0  1  0  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  0  1  0  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  0  1  0  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  0  0  1  0  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  0  0  1  1  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  0  1  1  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  0  1  1  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  0  0  1  1  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  0  1  0  0  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  1  0  0  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  1  0  0  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  1  0  0  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  1  0  1  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  1  0  1  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  0  1  0  1  1  0 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  1  0  1  1  1 */    kEvalTarget | kEvalSource | kReg1Reg2 | kReverse,
   /* 0  1  0  1  1  0  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  1  1  0  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  1  1  0  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  0  1  1  0  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  0  1  1  1  0  0 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  1  1  1  0  1 */    kEvalTarget | kCopyTarget | kReg1Reg2,
   /* 0  1  0  1  1  1  1  0 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  0  1  1  1  1  1 */    kEvalTarget | kReg1Reg2 | kReverse,
   /* 0  1  1  0  0  0  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  0  0  0  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  0  0  0  1  0 */    kEvalSource | kReg1Mem2 | kReverse,
   /* 0  1  1  0  0  0  1  1 */    kEvalSource | kReg1Mem2 | kReverse,
   /* 0  1  1  0  0  1  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  0  0  1  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  0  0  1  1  0 */    kEvalTarget | kReg1Mem2,
   /* 0  1  1  0  0  1  1  1 */    kEvalTarget | kReg1Mem2,
   /* 0  1  1  0  1  0  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  0  1  0  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  0  1  0  1  0 */    kReg1Mem2 | kReverse,
   /* 0  1  1  0  1  0  1  1 */    kReg1Mem2 | kReverse,
   /* 0  1  1  0  1  1  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  0  1  1  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  0  1  1  1  0 */    kReg1Mem2 | kReverse,
   /* 0  1  1  0  1  1  1  1 */    kReg1Mem2 | kReverse,
   /* 0  1  1  1  0  0  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  1  0  0  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  1  0  0  1  0 */    kEvalSource | kReg1Mem2 | kReverse,
   /* 0  1  1  1  0  0  1  1 */    kEvalSource | kReg1Mem2 | kReverse,
   /* 0  1  1  1  0  1  0  0 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  1  0  1  0  1 */    kEvalTarget | kEvalSource | kReg1Reg2,
   /* 0  1  1  1  0  1  1  0 */    kEvalTarget | kReg1Mem2,
   /* 0  1  1  1  0  1  1  1 */    kEvalTarget | kReg1Mem2,
   /* 0  1  1  1  1  0  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  1  1  0  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  1  1  0  1  0 */    kReg1Mem2 | kReverse,
   /* 0  1  1  1  1  0  1  1 */    kReg1Mem2 | kReverse,
   /* 0  1  1  1  1  1  0  0 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  1  1  1  0  1 */    kEvalTarget | kReg1Reg2,
   /* 0  1  1  1  1  1  1  0 */    kReg1Mem2 | kReverse,
   /* 0  1  1  1  1  1  1  1 */    kReg1Mem2 | kReverse,
   /* 1  0  0  0  0  0  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  0  0  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  0  0  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  0  0  0  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  0  0  1  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  0  1  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  0  1  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  0  0  1  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  0  1  0  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  1  0  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  1  0  1  0 */    kReg1Reg2 | kReverse,
   /* 1  0  0  0  1  0  1  1 */    kReg1Reg2 | kReverse,
   /* 1  0  0  0  1  1  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  1  1  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  0  1  1  1  0 */    kReg1Reg2 | kReverse,
   /* 1  0  0  0  1  1  1  1 */    kReg1Reg2 | kReverse,
   /* 1  0  0  1  0  0  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  0  0  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  0  0  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  1  0  0  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  1  0  1  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  0  1  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  0  1  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  1  0  1  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  0  0  1  1  0  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  1  0  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  1  0  1  0 */    kReg1Reg2 | kReverse,
   /* 1  0  0  1  1  0  1  1 */    kReg1Reg2 | kReverse,
   /* 1  0  0  1  1  1  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  1  1  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  0  0  1  1  1  1  0 */    kReg1Reg2 | kReverse,
   /* 1  0  0  1  1  1  1  1 */    kReg1Reg2 | kReverse,
   /* 1  0  1  0  0  0  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  0  0  0  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  0  0  0  1  0 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  0  0  0  1  1 */    kReg1Conv2,
   /* 1  0  1  0  0  1  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  0  0  1  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  0  0  1  1  0 */    kReg1Mem2,
   /* 1  0  1  0  0  1  1  1 */    kReg1Mem2,
   /* 1  0  1  0  1  0  0  0 */    kReg1Reg2,
   /* 1  0  1  0  1  0  0  1 */    kReg1Reg2,
   /* 1  0  1  0  1  0  1  0 */    kReg1Reg2,
   /* 1  0  1  0  1  0  1  1 */    kReg1Reg2,
   /* 1  0  1  0  1  1  0  0 */    kReg1Reg2,
   /* 1  0  1  0  1  1  0  1 */    kReg1Reg2,
   /* 1  0  1  0  1  1  1  0 */    kReg1Reg2,
   /* 1  0  1  0  1  1  1  1 */    kReg1Reg2,
   /* 1  0  1  1  0  0  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  1  0  0  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  1  0  0  1  0 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  1  0  0  1  1 */    kReg1Conv2,
   /* 1  0  1  1  0  1  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  1  0  1  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  0  1  1  0  1  1  0 */    kReg1Mem2,
   /* 1  0  1  1  0  1  1  1 */    kReg1Mem2,
   /* 1  0  1  1  1  0  0  0 */    kReg1Reg2,
   /* 1  0  1  1  1  0  0  1 */    kReg1Reg2,
   /* 1  0  1  1  1  0  1  0 */    kReg1Reg2,
   /* 1  0  1  1  1  0  1  1 */    kReg1Reg2,
   /* 1  0  1  1  1  1  0  0 */    kReg1Reg2,
   /* 1  0  1  1  1  1  0  1 */    kReg1Reg2,
   /* 1  0  1  1  1  1  1  0 */    kReg1Reg2,
   /* 1  0  1  1  1  1  1  1 */    kReg1Reg2,
   /* 1  1  0  0  0  0  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  0  0  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  0  0  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  0  0  0  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  0  0  1  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  0  1  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  0  1  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  0  0  1  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  0  1  0  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  1  0  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  1  0  1  0 */    kReg1Reg2 | kReverse,
   /* 1  1  0  0  1  0  1  1 */    kReg1Reg2 | kReverse,
   /* 1  1  0  0  1  1  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  1  1  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  0  1  1  1  0 */    kReg1Reg2 | kReverse,
   /* 1  1  0  0  1  1  1  1 */    kReg1Reg2 | kReverse,
   /* 1  1  0  1  0  0  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  0  0  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  0  0  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  1  0  0  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  1  0  1  0  0 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  0  1  0  1 */    kEvalSource | kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  0  1  1  0 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  1  0  1  1  1 */    kEvalSource | kReg1Reg2 | kReverse,
   /* 1  1  0  1  1  0  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  1  0  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  1  0  1  0 */    kReg1Reg2 | kReverse,
   /* 1  1  0  1  1  0  1  1 */    kReg1Reg2 | kReverse,
   /* 1  1  0  1  1  1  0  0 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  1  1  0  1 */    kCopyTarget | kReg1Reg2,
   /* 1  1  0  1  1  1  1  0 */    kReg1Reg2 | kReverse,
   /* 1  1  0  1  1  1  1  1 */    kReg1Reg2 | kReverse,
   /* 1  1  1  0  0  0  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  0  0  0  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  0  0  0  1  0 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  0  0  0  1  1 */    kReg1Conv2,
   /* 1  1  1  0  0  1  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  0  0  1  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  0  0  1  1  0 */    kReg1Mem2,
   /* 1  1  1  0  0  1  1  1 */    kReg1Mem2,
   /* 1  1  1  0  1  0  0  0 */    kReg1Reg2,
   /* 1  1  1  0  1  0  0  1 */    kReg1Reg2,
   /* 1  1  1  0  1  0  1  0 */    kReg1Reg2,
   /* 1  1  1  0  1  0  1  1 */    kReg1Reg2,
   /* 1  1  1  0  1  1  0  0 */    kReg1Reg2,
   /* 1  1  1  0  1  1  0  1 */    kReg1Reg2,
   /* 1  1  1  0  1  1  1  0 */    kReg1Reg2,
   /* 1  1  1  0  1  1  1  1 */    kReg1Reg2,
   /* 1  1  1  1  0  0  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  1  0  0  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  1  0  0  1  0 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  1  0  0  1  1 */    kReg1Conv2,
   /* 1  1  1  1  0  1  0  0 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  1  0  1  0  1 */    kEvalSource | kReg1Reg2,
   /* 1  1  1  1  0  1  1  0 */    kReg1Conv2,
   /* 1  1  1  1  0  1  1  1 */    kReg1Conv2,
   /* 1  1  1  1  1  0  0  0 */    kReg1Reg2,
   /* 1  1  1  1  1  0  0  1 */    kReg1Reg2,
   /* 1  1  1  1  1  0  1  0 */    kReg1Reg2,
   /* 1  1  1  1  1  0  1  1 */    kReg1Reg2,
   /* 1  1  1  1  1  1  0  0 */    kReg1Reg2,
   /* 1  1  1  1  1  1  0  1 */    kReg1Reg2,
   /* 1  1  1  1  1  1  1  0 */    kReg1Reg2,
   /* 1  1  1  1  1  1  1  1 */    kReg1Reg2
   };
