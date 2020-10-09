/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/BinaryAnalyser.hpp"
#include "z/codegen/BinaryCommutativeAnalyser.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"
#include "z/codegen/Linkage.hpp"

static TR::InstOpCode::Mnemonic getIntToFloatLogicalConversion(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic convertOpCode)
   {
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
      {
      switch(convertOpCode)
         {
         case TR::InstOpCode::CEFBR:
            return TR::InstOpCode::CELFBR;
            break;

         case TR::InstOpCode::CDFBR:
            return TR::InstOpCode::CDLFBR;
            break;

         case TR::InstOpCode::CXFBR:
            return TR::InstOpCode::CXLFBR;
            break;

         case TR::InstOpCode::CEGBR:
            return TR::InstOpCode::CELGBR;
            break;

         case TR::InstOpCode::CDGBR:
            return TR::InstOpCode::CDLGBR;
            break;

         case TR::InstOpCode::CXGBR:
            return TR::InstOpCode::CXLGBR;
            break;

         default:
            break;
         }
      }

   return convertOpCode;
   }

/**
 * "Convert to Logical" opcodes. Requires Arch(9) or above and IEEE FP
 */
static TR::InstOpCode::Mnemonic getFloatToIntLogicalConversion(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic convertOpCode)
   {
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
      {
      switch(convertOpCode)
         {
         case TR::InstOpCode::CFEBR:
            return TR::InstOpCode::CLFEBR;
            break;

         case TR::InstOpCode::CFDBR:
            return TR::InstOpCode::CLFDBR;
            break;

         case TR::InstOpCode::CFXBR:
            return TR::InstOpCode::CLFXBR;
            break;

         case TR::InstOpCode::CGEBR:
            return TR::InstOpCode::CLGEBR;
            break;

         case TR::InstOpCode::CGDBR:
            return TR::InstOpCode::CLGDBR;
            break;

         case TR::InstOpCode::CGXBR:
            return TR::InstOpCode::CLGXBR;
            break;

         default:
            break;
         }
      }

   return convertOpCode;
   }

static bool
strictFPForMultiplyAndAdd(TR::CodeGenerator * cg)
   {
   //return 'true' - multiply-and-add is not a valid operation to perform with current JLS
   //return (isStrictFP(cg->comp()->getCurrentMethod()) ||
   //        cg->comp()->assumeDefaultStrictFP());
   return true;
   }

inline TR::Register *
unaryEvaluator(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic regToRegOpCode, TR::Register * newTargetRegister = NULL)
   {
   TR::Node * firstChild = node->getFirstChild();

   TR::Register * srcRegister = (newTargetRegister != NULL) ? cg->evaluate(firstChild) : cg->gprClobberEvaluate(firstChild);

   TR::Register * targetRegister ;
   if (newTargetRegister)
      {
      targetRegister = newTargetRegister;
      }
   else
      {
      targetRegister = srcRegister;
      }

   generateRRInstruction(cg, regToRegOpCode, node, targetRegister, srcRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

inline void
genLogicalConversionForInt(TR::Node * node, TR::CodeGenerator * cg, TR::Register * targetRegister, int8_t shift_amount)
   {
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12))
      {
      generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, targetRegister, targetRegister, shift_amount, (int8_t)(63|0x80), 0);
      }
   else
      {
      generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, targetRegister, targetRegister, shift_amount, (int8_t)(63|0x80), 0);
      }
   }

inline void
genArithmeticConversionForInt(TR::Node * node, TR::CodeGenerator * cg, TR::Register * targetRegister, int8_t shift_amount)
   {
   generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, targetRegister, shift_amount);
   generateRSInstruction(cg, TR::InstOpCode::SRAG, node, targetRegister, targetRegister, shift_amount);
   }

//Convert the targetRegister value to appropriate integer type
static void generateValueConversionToIntType(TR::CodeGenerator *cg, TR::Node *node, TR::Register *targetRegister)
   {
   int32_t nshift = 0;

   switch (node->getOpCodeValue())
      {
      case TR::f2c:
      case TR::d2c:
        //case TR_UInt16:
         nshift = 48;
         genLogicalConversionForInt(node, cg, targetRegister, nshift);
         break;
      case TR::f2s:
      case TR::d2s:
        //case TR_SInt16:
         nshift = 48;
         genArithmeticConversionForInt(node, cg, targetRegister, nshift);
         break;
      case TR::f2b:
      case TR::d2b:
        //case TR_SInt8:
         nshift = 56;
         genArithmeticConversionForInt(node, cg, targetRegister, nshift);
         break;
      case TR::f2bu:
      case TR::d2bu:
         //case TR_UInt8:
         nshift = 56;
         genLogicalConversionForInt(node, cg, targetRegister, nshift);
         break;
      default:
         break;
      }
   }

/**
 * Helper Method to generate extended floating point literal in FPR pair registers
 * Saves constant in literal pool
 * Input: high and low 64bit bitwise quantities
 * @return FPR register pair containing extended floating point literal
 */
TR::Register *
generateExtendedFloatConstantReg(TR::Node *node, TR::CodeGenerator *cg, int64_t constHi, int64_t constLo,
                                 TR::RegisterDependencyConditions ** depsPtr)
   {
   TR::Register * litBase = NULL;
   bool stopUsingMemRefRegs = false;

   if (cg->isLiteralPoolOnDemandOn())
      {
      litBase = cg->allocateRegister();
      generateLoadLiteralPoolAddress(cg, node, litBase);
      stopUsingMemRefRegs = true;
      }
   else
      {
      litBase = cg->getLitPoolRealRegister();
      }

   TR::Register *trgReg = cg->allocateFPRegisterPair();

   //For ICF...
   if(depsPtr)
      {
      (*depsPtr)->addPostCondition(litBase, TR::RealRegister::AssignAny);
      (*depsPtr)->addPostCondition(trgReg, TR::RealRegister::FPPair);
      (*depsPtr)->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::LegalFirstOfFPPair);
      (*depsPtr)->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::LegalSecondOfFPPair);
      }

   TR::MemoryReference * mrHi = generateS390MemoryReference(constHi, TR::Int64, cg, litBase);
   TR::MemoryReference * mrLo = generateS390MemoryReference(constLo, TR::Int64, cg, litBase);

   generateRXInstruction(cg, TR::InstOpCode::LD, node, trgReg->getHighOrder(), mrHi);
   generateRXInstruction(cg, TR::InstOpCode::LD, node, trgReg->getLowOrder(), mrLo);

   if (stopUsingMemRefRegs)
      {
      mrHi->stopUsingMemRefRegister(cg);
      mrLo->stopUsingMemRefRegister(cg);
      }

   if (cg->isLiteralPoolOnDemandOn())
      {
      cg->stopUsingRegister(litBase);
      }

   return trgReg;
   }

/**
 * Convert float/Double/LongDouble to fixed number i.e. int,short,byte, or char
 */
inline TR::Register *
convertToFixed(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();

   // Should handle only (<= 32bit) targets
   TR_ASSERT(node->getDataType() <= TR::Int32 && node->getDataType() >= TR::Int8,
             "convertToFixed only handles 32 bit integer target type\n");

   // Determine if it's unsigned conversion
   bool isUnsignedConversion = node->getOpCode().isUnsignedConversion();

   //Assign opcodes based on source float type
   TR::InstOpCode::Mnemonic compareOp;
   TR::InstOpCode::Mnemonic convertOp;
   TR::InstOpCode::Mnemonic roundToIntOp;
   TR::InstOpCode::Mnemonic subFPOp;
   int64_t MSBIntConstInFP = 0;
   switch (firstChild->getDataType())
      {
      case TR::Float:
         compareOp =    TR::InstOpCode::CEBR;
         convertOp =    TR::InstOpCode::CFEBR;
         roundToIntOp = TR::InstOpCode::FIDBR;    // Double versions. Unsigned conversion will need to use long form
         subFPOp =      TR::InstOpCode::SDB;
         MSBIntConstInFP = (int64_t)CONSTANT64(0x41e0000000000000);
         break;
      case TR::Double:
         compareOp =    TR::InstOpCode::CDBR;
         convertOp =    TR::InstOpCode::CFDBR;
         roundToIntOp = TR::InstOpCode::FIDBR;
         subFPOp =      TR::InstOpCode::SDB;
         MSBIntConstInFP = (int64_t)CONSTANT64(0x41e0000000000000);
         break;
      default:
         TR_ASSERT(false, "Invalid source floating point type\n");
         return NULL;
      }

   // Try to see if we can use "Convert to Logical" opcodes. Requires Arch(9) or above and IEEE FP
   bool floatToIntLogicalConverted = false;
   if(isUnsignedConversion) //target = unsigned integer
      {
      TR::InstOpCode::Mnemonic oldconvertOp = convertOp;
      convertOp = getFloatToIntLogicalConversion(cg, convertOp);
      floatToIntLogicalConverted = (convertOp != oldconvertOp);
      }

   //Conversion Code Sequences:
   //Unsigned Target Path
   if(isUnsignedConversion)
      {
      if(floatToIntLogicalConverted)
         {
         //Convert to Logical for BFP path
         //1) Convert to Logical
         //   round to zero as we want truncation when we convert to int
         generateRRFInstruction(cg, convertOp, node, targetRegister, srcRegister, (uint8_t)0x5 /*round to zero*/, (uint8_t)0x0);

         //2) Convert the targetRegister value to appropriate integer type, if needed
         generateValueConversionToIntType(cg, node, targetRegister);
         }
      else
         {
         /* Convert to Fixed for HFP and BFP path, "Branchless" conversion:
          * *short floating point must use long form for enough precision bits when we subtract by MSB (4 bytes)
          * 0) short to long, if required
          * 1) fp round to 0
          * 2) sub MSB (of target integer, in float value)
          * 3) convert to fixed
          *     if  max<a<+inf, result will be max signed int (MSB-1)
          *     if  0<=a<=max,  result will be converted, fixed value
          *     if  min<=a<0,   result will be converted, fixed value (MSB unset)
          *     if -inf<a<min,  result will be min signed int (MSB)
          * 4) XOR MSB  (will fix sign-bit)
          * 5) Convert the targetRegister value to appropriate integer type, if needed (for size < Int32)
          *
          * The idea behind this is below
          * (int)(a - MSB) xor MSB = (unsigned int)a
          *
          * Why this works: We will subtract by MSB just before conversion. This means that MSB will be 0 for value >= 2^31.
          *                 If value < 2^31, MSB will be 1 due to borrow and lower bits will NOT be touched.
          *                 The XOR MSB test will put the sign back if original exceeded max integral value.
          *                 xor will take care of -inf<a<min case (result is min signed value as above)
          *                 by unsetting MSB, which gives 0 as required by XLC lang ref.
          */

         // Prepare to clobber source floating point register
         TR::Register *srcRegCpy = cg->fprClobberEvaluate(firstChild);

         //0) convert from short to long BFP if necessary
         if(firstChild->getDataType() == TR::Float)
            {
            generateRRInstruction(cg, TR::InstOpCode::LDEBR, node, srcRegCpy, srcRegCpy);         //short to long FP
            convertOp = TR::InstOpCode::CFDBR;         //update convert op to long form
            }

         //1) fp round to 0
         //   todo: the only reason this is placed here is because we can specify rounding mode explicitly to "round to zero".
         //         convertOp for hex doesn't allow setting rounding mode mask, but IEEE does, so we may not need this here for IEEE
         generateRRFInstruction(cg, roundToIntOp, node, srcRegCpy, srcRegCpy, (uint8_t)0x5 /*round to zero*/, true);

         //2) subtract MSB
         switch(firstChild->getDataType())
            {
            case TR::Float:
            case TR::Double:
               generateS390ImmOp(cg, subFPOp, node, srcRegCpy, srcRegCpy, MSBIntConstInFP);
               break;
            default: TR_ASSERT(false, "unknown first child type\n"); break;
            }

         //3) Convert to Fixed
         generateRRFInstruction(cg, convertOp, node, targetRegister, srcRegCpy, (int8_t) 0x5, true);

         //4) Handle the sign bit by XOR
         generateRILInstruction(cg, TR::InstOpCode::XILF, node, targetRegister, 0x80000000);

         //5) Convert the targetRegister value to appropriate integer type, if needed
         generateValueConversionToIntType(cg, node, targetRegister);

         // Clean-up copied source register
         cg->stopUsingRegister(srcRegCpy);
         }
      }
   else  // Signed Target Path
      {
      //1) Reset targetRegister
      generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetRegister, targetRegister);

      // Java expect that for signed conversion to fixed, if src float is NaN, target to have 0.0.
      //2) NaN test and branch to done
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

      generateRRInstruction(cg, compareOp, node, srcRegister, srcRegister);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, cFlowRegionEnd);   //NaN results in CC3

      //3) Convert to Fixed
      generateRRFInstruction(cg, convertOp, node, targetRegister, srcRegister, (int8_t) 0x5, true);

      //4) Convert the targetRegister value to appropriate integer type, if needed
      generateValueConversionToIntType(cg, node, targetRegister);

      //5) Set up register dependencies
      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 4, cg);
      deps->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(srcRegister, TR::RealRegister::AssignAny);


      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
      cFlowRegionEnd->setEndInternalControlFlow();
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * Convert to float/Double/LongDouble from fixed number i.e. int,short,byte, or char
 */
inline TR::Register *
convertFromFixed(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcRegister = cg->evaluate(firstChild);

   TR::Register * targetRegister;
   TR::Register * lowReg, * highReg;

   TR::InstOpCode::Mnemonic convertOpCode;
   // convert the srcRegister value to appropriate type, if needed
   int32_t nshift = 0;
   bool isUint32Src = false;
   bool isFloatTgt = false;
   targetRegister = cg->allocateRegister(TR_FPR);

   switch (node->getOpCodeValue())
     {
     case TR::s2f:
     case TR::s2d:
       //case TR_SInt16:
         nshift = 48;
         genArithmeticConversionForInt(node, cg, srcRegister, nshift);
         break;
     case TR::su2f:
     case TR::su2d:
   //case TR_UInt16    :
         nshift = 48;
         genLogicalConversionForInt(node, cg, srcRegister, nshift);
         break;
     case TR::b2f:
     case TR::b2d:
   //case TR_SInt8    :
         nshift = 56;
         genArithmeticConversionForInt(node, cg, srcRegister, nshift);
         break;
     case TR::bu2f:
     case TR::bu2d:
   //case TR_UInt8    :
         nshift = 56;
         genLogicalConversionForInt(node, cg, srcRegister, nshift);
         break;
     case TR::iu2f:
     case TR::iu2d:
       //case TR_UInt32   :
         isUint32Src = true;
         break;
      default         :
         break;
      }
   switch (node->getDataType())
      {
      case TR::Float   :
         convertOpCode = TR::InstOpCode::CEFBR;
         isFloatTgt = true;
         break;
      case TR::Double  :
         convertOpCode = TR::InstOpCode::CDFBR;
         break;
      default         :
         TR_ASSERT(false, "should be unreachable");
         return NULL;
      }

   // convert from fixed to floating point
   // todo: unsigned conversions from/to fp will likely need a refactor as same code is found in other evaluators
   //       We can also take advantage of logical conversions that will save us a branch and add/load lengthen insns
   generateRRInstruction(cg, convertOpCode, node, targetRegister, srcRegister);
   if (isUint32Src)
      {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * skipAddLabel = generateLabelSymbol(cg);
      TR::RegisterDependencyConditions * deps = NULL;
      generateRRInstruction(cg, TR::InstOpCode::LTR, node, srcRegister, srcRegister);
      // If positive or zero, done BNL
      // otherwise, add double constant 0x41f0000000000000 to converted value
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, skipAddLabel);
      switch(node->getDataType())
         {
         case TR::Float:  // fall-through
            generateRRInstruction(cg, TR::InstOpCode::LDEBR, node, targetRegister, targetRegister);
         case TR::Double:
            deps = generateRegisterDependencyConditions(0, 1, cg);
            deps->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
            generateS390ImmOp(cg, TR::InstOpCode::ADB, node, targetRegister, targetRegister,
                              (int64_t) CONSTANT64(0x41f0000000000000), deps);
            break;
         default:
            TR_ASSERT(false, "unknown target fp type");
            break;
         }
      if(isFloatTgt)
         generateRRInstruction(cg, TR::InstOpCode::LEDBR, node, targetRegister, targetRegister);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAddLabel, deps);
      skipAddLabel->setEndInternalControlFlow();
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

inline TR::MemoryReference *
fstoreHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   TR::Register * valueReg = cg->evaluate(valueChild);

   TR::MemoryReference * tempMR = generateS390MemoryReference(node, cg);

   generateRXInstruction(cg, TR::InstOpCode::STE, node, valueReg, tempMR);

   cg->decReferenceCount(valueChild);

   return tempMR;
   }

inline TR::MemoryReference *
dstoreHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   TR::Register * valueReg = cg->evaluate(valueChild);

   TR::MemoryReference * tempMR = generateS390MemoryReference(node, cg, true);

   generateRXInstruction(cg, TR::InstOpCode::STD, node, valueReg, tempMR);

   cg->decReferenceCount(valueChild);

   return tempMR;
   }


inline TR::Register *
floadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * srcMR)
   {
   TR::Register * tempReg = node->setRegister(cg->allocateRegister(TR_FPR));
   TR::MemoryReference * tempMR = srcMR;
   if (tempMR == NULL)
      {
      tempMR = generateS390MemoryReference(node, cg);
      //traceMsg(cg->comp(), "Generated memory reference %p for node %p with offset %d",tempMR,node,tempMR->getOffset());
      }

   generateRXInstruction(cg, TR::InstOpCode::LE, node, tempReg, tempMR);
   tempMR->stopUsingMemRefRegister(cg);
   return tempReg;
   }

inline TR::Register *
dloadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * srcMR)
   {
   TR::Register * tempReg = node->setRegister(cg->allocateRegister(TR_FPR));
   TR::MemoryReference * tempMR = srcMR;

   if (tempMR == NULL)
      {
      tempMR = generateS390MemoryReference(node, cg, true);
      }
   generateRXInstruction(cg, TR::InstOpCode::LD, node, tempReg, tempMR);
   tempMR->stopUsingMemRefRegister(cg);
   return tempReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::fconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetReg = cg->allocateRegister(TR_FPR);
   float value = node->getFloat();

   generateS390ImmOp(cg, TR::InstOpCode::LE, node, targetReg, value);
   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::dconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetReg = cg->allocateRegister(TR_FPR);
   double value = node->getDouble();

   generateS390ImmOp(cg, TR::InstOpCode::LD, node, targetReg, value);
   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::floadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return floadHelper(node, cg, NULL);
   }

TR::Register *
OMR::Z::TreeEvaluator::dloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return dloadHelper(node, cg, NULL);
   }

TR::Register *
OMR::Z::TreeEvaluator::fstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   fstoreHelper(node, cg);
   return NULL;
   }

TR::Register *
OMR::Z::TreeEvaluator::dstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   dstoreHelper(node, cg);
   return NULL;
   }

TR::Register *
OMR::Z::TreeEvaluator::faddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   if ((canUseNodeForFusedMultiply(node->getFirstChild()) || canUseNodeForFusedMultiply(node->getSecondChild())) &&
        generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::MAEBR))
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "Successfully changed fadd to fused multiply and add operation\n");
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.floatBinaryCommutativeAnalyser(node, TR::InstOpCode::AEBR, TR::InstOpCode::AEB);
      cg->decReferenceCount(node->getFirstChild());
      cg->decReferenceCount(node->getSecondChild());
      }

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::daddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   if ((canUseNodeForFusedMultiply(node->getFirstChild()) || canUseNodeForFusedMultiply(node->getSecondChild())) &&
       generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::MADBR))
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "Successfully changed dadd to fused multiply and add operation\n");
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.doubleBinaryCommutativeAnalyser(node, TR::InstOpCode::ADBR, TR::InstOpCode::ADB);
      cg->decReferenceCount(node->getFirstChild());
      cg->decReferenceCount(node->getSecondChild());
      }

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::fmulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_S390BinaryCommutativeAnalyser temp(cg);
   temp.floatBinaryCommutativeAnalyser(node, TR::InstOpCode::MEEBR, TR::InstOpCode::MEEB);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::dmulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_S390BinaryCommutativeAnalyser temp(cg);
   TR::InstOpCode::Mnemonic memOpCode = TR::InstOpCode::MDB;
   TR::InstOpCode::Mnemonic regOpCode = TR::InstOpCode::MDBR;

   temp.doubleBinaryCommutativeAnalyser(node, regOpCode, memOpCode);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::fsubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();

   if (canUseNodeForFusedMultiply(node->getFirstChild()) &&
       generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::MSEBR))
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "Successfully changed fsub to fused multiply and sub operation\n");
      }
   else if (canUseNodeForFusedMultiply(node->getSecondChild()) &&
            generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::MAEBR, TR::InstOpCode::LCEBR))
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "Successfully changed fsub to fused multiply, negate, and add operation\n");
      }
   else
      {
      TR_S390BinaryAnalyser temp(cg);
      temp.floatBinaryAnalyser(node, TR::InstOpCode::SEBR, TR::InstOpCode::SEB);
      }

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::dsubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();

   if (canUseNodeForFusedMultiply(node->getFirstChild()) &&
       generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::MSDBR))
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "Successfully changed dsub to fused multiply and subtract operation\n");
      }
   else if (canUseNodeForFusedMultiply(node->getSecondChild()) &&
            generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::MADBR, TR::InstOpCode::LCDBR))
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "Successfully changed dsub to fused multiply, negate, and add operation\n");
      }
   else
      {
      TR_S390BinaryAnalyser temp(cg);
      temp.doubleBinaryAnalyser(node, TR::InstOpCode::SDBR, TR::InstOpCode::SDB);
      }

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::fdivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_S390BinaryAnalyser temp(cg);
   temp.floatBinaryAnalyser(node, TR::InstOpCode::DEBR, TR::InstOpCode::DEB);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::ddivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_S390BinaryAnalyser temp(cg);
   temp.doubleBinaryAnalyser(node, TR::InstOpCode::DDBR, TR::InstOpCode::DDB);
   return node->getRegister();
   }

/** \brief Generates code for a % b for both float and double values
 *  \details
 *  The code uses divide-to-integer instructions DIxBR to get remainder of two double/float values.
 *  In rare cases when result of DIxBR is not exact it calls out to double/float remainder helper which
 *  calls fmod to get remainder and uses System linkage.
 */
TR::Register *
OMR::Z::TreeEvaluator::floatRemHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * firstRegister = cg->fprClobberEvaluate(node->getFirstChild());
   TR::Register * secondRegister = cg->fprClobberEvaluate(node->getSecondChild());
   TR::Register * tempRegister = cg->allocateRegister(TR_FPR);
   TR::Register * targetRegister = cg->allocateRegister(TR_FPR);
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * labelNotExact = generateLabelSymbol(cg);
   TR::LabelSymbol * labelOK = generateLabelSymbol(cg);
   TR::ILOpCodes opCode = node->getOpCodeValue();
   //
   // Algorithm: c = a rem b;
   //              target = a;
   //              target = DivideToInteger(target, b, tmp);
   //              if (target inexact) target = fmod(a,b);
   //              if (target is +0 AND a is < +0) then target is -0;
   // The check for the special case of 0's should be on rare path code, not in the mainline

   // callNode is used as dangling node used to pass to buildSystemLInkageDispatch to generate a call to fmod helper
   TR::Node *callNode = NULL;

   if (node->getDataType() == TR::Float)
      {
      generateRRInstruction(cg, TR::InstOpCode::LER, node, targetRegister, firstRegister);
      generateRRFInstruction(cg, TR::InstOpCode::DIEBR, node, targetRegister, secondRegister, tempRegister, 0x5);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK3, node, labelNotExact);    // remainder not exact
      generateRXEInstruction(cg, TR::InstOpCode::TCEB, node, targetRegister, generateS390MemoryReference(0x800, cg), 0);  // c is +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, labelOK);        // it is not +0
      generateRXEInstruction(cg, TR::InstOpCode::TCEB, node, firstRegister, generateS390MemoryReference(0x555, cg), 0);   // a is < +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, labelOK);        // it is not < +0
      generateRRInstruction(cg, TR::InstOpCode::LCEBR, node, targetRegister, targetRegister); // negate answer to be -0
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelOK);        // it is not < +0
      TR::SymbolReference *helperCallSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390jitMathHelperFREM);
      callNode = TR::Node::createWithSymRef(node, TR::fcall, 2, helperCallSymRef);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LDR, node, targetRegister, firstRegister);
      generateRRFInstruction(cg, TR::InstOpCode::DIDBR, node, targetRegister, secondRegister, tempRegister, 0x5);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK3, node, labelNotExact);    // remainder not exact
      generateRXEInstruction(cg, TR::InstOpCode::TCDB, node, targetRegister, generateS390MemoryReference(0x800, cg), 0);  // c is +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, labelOK);        // it is not +0
      generateRXEInstruction(cg, TR::InstOpCode::TCDB, node, firstRegister, generateS390MemoryReference(0x555, cg), 0);   // a is < +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, labelOK);        // it is not < +0
      generateRRInstruction(cg, TR::InstOpCode::LCDBR, node, targetRegister, targetRegister); // negate answer to be -0
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelOK);        // it is not < +0
      TR::SymbolReference *helperCallSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390jitMathHelperDREM);
      callNode = TR::Node::createWithSymRef(node, TR::dcall, 2, helperCallSymRef);
      }

   // Putting call-out part in the OOL as we only need to call helper when result is not exact which is very rare.
   TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(labelNotExact,labelOK,cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
   outlinedSlowPath->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelNotExact);
   callNode->setChild(0, node->getFirstChild());
   callNode->setChild(1, node->getSecondChild());
   TR::Linkage *linkage = cg->createLinkage(TR_System);
   TR::Register *helperReturnRegister = linkage->buildSystemLinkageDispatch(callNode);
   generateRRInstruction(cg, node->getDataType() == TR::Float ? TR::InstOpCode::LER : TR::InstOpCode::LDR, node,  targetRegister, helperReturnRegister);

   // buildSystemLinkageDispatch sets a helperReturnRegister to callNode.
   // Artificially setting reference count of callNode to 1 and decreasing it makes sure that this register is no longer live in the method.
   callNode->setReferenceCount(1);
   cg->decReferenceCount(callNode);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelOK);
   outlinedSlowPath->swapInstructionListsWithCompilation();

   TR::RegisterDependencyConditions * postDeps = generateRegisterDependencyConditions(0, 4, cg);
   postDeps->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
   postDeps->addPostCondition(firstRegister, TR::RealRegister::AssignAny);
   postDeps->addPostCondition(secondRegister, TR::RealRegister::AssignAny);
   postDeps->addPostCondition(tempRegister, TR::RealRegister::AssignAny);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelOK, postDeps);
   labelOK->setEndInternalControlFlow();
   node->setRegister(targetRegister);
   cg->stopUsingRegister(firstRegister);
   cg->stopUsingRegister(secondRegister);
   cg->stopUsingRegister(tempRegister);
   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::dremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::floatRemHelper(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::fremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::floatRemHelper(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::fnegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return unaryEvaluator(node, cg, TR::InstOpCode::LCEBR);
   }

TR::Register *
OMR::Z::TreeEvaluator::dnegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return unaryEvaluator(node, cg, TR::InstOpCode::LCDBR);
   }

// type coercion helper for fbits2i and dbits2l
TR::Register *
FPtoIntBitsTypeCoercionHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetReg = NULL;
   TR::Register * sourceReg = NULL;
   TR::DataType nodeType = node->getDataType();

   if (node->getFirstChild()->isSingleRefUnevaluated() &&
          node->getFirstChild()->getOpCode().isLoadVar())
      {
      TR::MemoryReference * tempmemref = generateS390MemoryReference(node->getFirstChild(), cg);
      if (nodeType == TR::Int64)
         targetReg = genericLoadHelper<64, 64, MemReg>(node, cg, tempmemref, NULL, false, true);
      else
         targetReg = genericLoadHelper<32, 32, MemReg>(node, cg, tempmemref, NULL, false, true);
      }
   else
      {
      // Evaluate child, where sourceReg should be an FPR.
      sourceReg = cg->evaluate(node->getFirstChild());
      TR::SymbolReference * f2iSR = cg->allocateLocalTemp(nodeType);

      // It seems the FP instruction performance is sub-optimal (i.e. LGDR), and we're better
      // off storing to memory and then loading into a GPR.
      TR::MemoryReference * tempMR1 = generateS390MemoryReference(node, f2iSR, cg);
      auto mnemonic = nodeType == TR::Int64 ? TR::InstOpCode::STD : TR::InstOpCode::STE;
      generateRXInstruction(cg, mnemonic, node, sourceReg, tempMR1);

      TR::MemoryReference * tempMR2 = generateS390MemoryReference(*tempMR1, 0, cg);
      if (nodeType == TR::Int64)
         targetReg = genericLoadHelper<64, 64, MemReg>(node, cg, tempMR2, NULL, false, true);
      else
         targetReg = genericLoadHelper<32, 32, MemReg>(node, cg, tempMR2, NULL, false, true);
      }

    if (node->normalizeNanValues())
       {
       TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);

       TR::Register * targetFPR = cg->allocateRegister(TR_FPR);
       deps->addPostCondition(targetFPR, TR::RealRegister::AssignAny);

       TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
       TR::LabelSymbol * cleansedNumber = generateLabelSymbol(cg);

       TR::Register * litBase = NULL;

       if (node->getNumChildren() == 2)
          {
          litBase = cg->evaluate(node->getSecondChild());
          }
       else if (cg->isLiteralPoolOnDemandOn())
          {
          litBase = cg->allocateRegister();
          generateLoadLiteralPoolAddress(cg, node, litBase);
          }

       TR::MemoryReference* nanMemRef = nodeType == TR::Int64 ?
                 generateS390MemoryReference(static_cast<int64_t>(0x7ff8000000000000), nodeType, cg, litBase) :
                 generateS390MemoryReference(static_cast<int32_t>(0x7fc00000),         nodeType, cg, litBase);

       auto testMnemonic = nodeType == TR::Int64 ? TR::InstOpCode::TCDB : TR::InstOpCode::TCEB;

       if (litBase)
          {
          deps->addPostCondition(litBase, TR::RealRegister::AssignAny);
          }
       deps->addPostCondition(targetReg, TR::RealRegister::AssignAny);

       generateRRInstruction(cg, TR::InstOpCode::LDGR, node, targetFPR, targetReg);

       generateRXEInstruction(cg, testMnemonic, node, targetFPR, generateS390MemoryReference(0x00F, cg), 0);

       generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, cleansedNumber);

       generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
       cFlowRegionStart->setStartInternalControlFlow();

       if (nodeType == TR::Int64)
          targetReg = genericLoadHelper<64, 64, MemReg>(node, cg, nanMemRef, NULL, false, true);
       else
          targetReg = genericLoadHelper<32, 32, MemReg>(node, cg, nanMemRef, NULL, false, true);

       generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cleansedNumber, deps);
       cleansedNumber->setEndInternalControlFlow();

       cg->stopUsingRegister(targetFPR);

       if (cg->isLiteralPoolOnDemandOn())
          {
          cg->stopUsingRegister(litBase);
          }
       }
    node->setRegister(targetReg);
    cg->decReferenceCount(node->getFirstChild());

    if (node->getNumChildren() == 2)
       cg->decReferenceCount(node->getSecondChild());

    return targetReg;
   }

/**
 * Type-coerce int to float
 */
TR::Register *
OMR::Z::TreeEvaluator::ibits2fEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   //Disable FPE Version For Now Due To Performance Reasons
   bool disabled = true;
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetReg = cg->allocateRegister(TR_FPR);
   TR::Register * sourceReg;
   sourceReg = cg->evaluate(firstChild);
   if (cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_FPE) && !disabled)
      {
      TR::Register *tempreg;
      tempreg = cg->allocateRegister();
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempreg, sourceReg, 32);
      generateRRInstruction(cg, TR::InstOpCode::LDGR, node, targetReg, tempreg);
      cg->stopUsingRegister(tempreg);
      }
   else
      {
      TR::SymbolReference * i2fSR = cg->allocateLocalTemp(TR::Int32);
      TR::MemoryReference * tempMR = generateS390MemoryReference(node, i2fSR, cg);
      TR::MemoryReference * tempMR1 = generateS390MemoryReference(node, i2fSR, cg);
      generateRXInstruction(cg, TR::InstOpCode::ST, node, sourceReg, tempMR);
      generateRXInstruction(cg, TR::InstOpCode::LE, node, targetReg, tempMR1);
      }
   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   return targetReg;
   }

/**
 * Type-coerce float to int
 */
TR::Register *
OMR::Z::TreeEvaluator::fbits2iEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return FPtoIntBitsTypeCoercionHelper(node, cg);
   }

/**
 * Type-coerce long to double
 */
TR::Register *
OMR::Z::TreeEvaluator::lbits2dEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   //Disable For Now Due To Performance Ressons
   bool disabled = true;
   TR::Register * sourceReg;
   TR::Node * firstChild = node->getFirstChild();
   TR::Compilation *comp = cg->comp();
   if((cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_FPE)) && (!disabled))
      {
      sourceReg = cg->evaluate(firstChild);
      TR::Register * targetReg = cg->allocateRegister(TR_FPR);
      generateRRInstruction(cg, TR::InstOpCode::LDGR, node, targetReg ,sourceReg);
      node->setRegister(targetReg);
      cg->decReferenceCount(firstChild);
      return targetReg;
      }
   else
      {
      TR::Register * targetReg = cg->allocateRegister(TR_FPR);
      sourceReg = cg->evaluate(firstChild);
      TR::SymbolReference * l2dSR = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempMR = generateS390MemoryReference(node, l2dSR, cg);
      TR::MemoryReference * tempMR1 = generateS390MemoryReference(node, l2dSR, cg);
      generateRXInstruction(cg, TR::InstOpCode::STG, node, sourceReg, tempMR);
      generateRXInstruction(cg, TR::InstOpCode::LD, node, targetReg, tempMR1);
      node->setRegister(targetReg);
      cg->decReferenceCount(firstChild);
      return targetReg;
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::dbits2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return FPtoIntBitsTypeCoercionHelper(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::i2fEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return convertFromFixed(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::i2dEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return convertFromFixed(node, cg);
   }

/*****************************************************************************
 * Double Representation:                                                    *
 *       ______________________________ ________________________________     *
 *      |S| Biased Exponent(11 bits)   |    Fraction(52 bits)           |    *
 *      |_|____________________________|________________________________|    *
 *      0 1                            12                               63   *
 *                                                                           *
 *   S               =  sign bit ( 1= Negative ; 0=positive)                 *
 *   Biased Exponent = Exponent + 0x3FF = Exponent +1023                     *
 *                     Representing exponent range (-1022, ...+1023)         *
 *   Fraction        = Fraction part of the float after normalization        *
 *****************************************************************************/

TR::Register *
l2dHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction * cursor;
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * longRegister = cg->evaluate(firstChild);
   TR::Register * targetFloatRegister = cg->allocateRegister(TR_FPR);

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196) && node->getOpCodeValue() == TR::lu2d)
      {
      generateRRFInstruction(cg, TR::InstOpCode::CDLGBR, node, targetFloatRegister, longRegister, (uint8_t)0x0, (uint8_t)0x0);
      cg->decReferenceCount(firstChild);
      node->setRegister(targetFloatRegister);
      return targetFloatRegister;
      }

   generateRRInstruction(cg, TR::InstOpCode::CDGBR, node, targetFloatRegister, longRegister);

   //handle unsigned case
   //if(firstChild->getDataType() == TR_UInt64)
   if (node->getOpCodeValue() == TR::lu2d)
      {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * skipAddLabel = generateLabelSymbol(cg);
      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 1, cg);

      generateRRInstruction(cg, TR::InstOpCode::LTGR, node, longRegister, longRegister);
      // If positive or zero, done BNL
      //otherwise, add double constant representing 2^64 to converted value

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, skipAddLabel);
      deps->addPostCondition(targetFloatRegister, TR::RealRegister::AssignAny);
      generateS390ImmOp(cg, TR::InstOpCode::ADB, node, targetFloatRegister, targetFloatRegister, (int64_t) CONSTANT64(0x43f0000000000000), deps);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAddLabel, deps);
      skipAddLabel->setEndInternalControlFlow();
      }

   node->setRegister(targetFloatRegister);
   cg->decReferenceCount(firstChild);

   return targetFloatRegister;
   }


TR::Register *
l2fHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * longRegister = cg->evaluate(firstChild);
   TR::Register * targetFloatRegister = cg->allocateRegister(TR_FPR);
   TR::InstOpCode::Mnemonic convertOpCode = TR::InstOpCode::CEGBR;



   bool IntToFloatLogicalConverted = false;
   if(node->getOpCodeValue() == TR::lu2f)
      {
      TR::InstOpCode::Mnemonic oldConvertOpCode = convertOpCode;
      convertOpCode = getIntToFloatLogicalConversion(cg, convertOpCode);
      IntToFloatLogicalConverted = (oldConvertOpCode != convertOpCode);
      }

   if(IntToFloatLogicalConverted)
      generateRRFInstruction(cg, convertOpCode, node, targetFloatRegister, longRegister, (uint8_t)0x5, (uint8_t)0x0);
   else
      generateRRInstruction(cg, convertOpCode, node, targetFloatRegister, longRegister);

   //handle unsigned case
   //if(firstChild->getDataType() == TR_UInt64)
   if (!IntToFloatLogicalConverted && node->getOpCodeValue() == TR::lu2f)
      {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * skipAddLabel = generateLabelSymbol(cg);
      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 1, cg);

      generateRRInstruction(cg, TR::InstOpCode::LTGR, node, longRegister, longRegister);
      // If positive or zero, done BNL
      //otherwise, add double constant 0x43f0000000000000 to converted value
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, skipAddLabel);
      deps->addPostCondition(targetFloatRegister, TR::RealRegister::AssignAny);
      //cannot use opcodes TR::InstOpCode::AE or TR::InstOpCode::AEB with generateS390ImmOp,
      //   so for now transfer from float to double, add constant, then back to float
      generateRRInstruction(cg, TR::InstOpCode::LDEBR, node, targetFloatRegister, targetFloatRegister);
      generateS390ImmOp(cg, TR::InstOpCode::ADB, node, targetFloatRegister, targetFloatRegister, (int64_t) CONSTANT64(0x43f0000000000000), deps);
      generateRRInstruction(cg, TR::InstOpCode::LEDBR, node, targetFloatRegister, targetFloatRegister);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAddLabel, deps);
      skipAddLabel->setEndInternalControlFlow();
      }

   node->setRegister(targetFloatRegister);
   cg->decReferenceCount(firstChild);

   return targetFloatRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::l2fEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return l2fHelper64(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::l2dEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return l2dHelper64(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::f2iEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return convertToFixed(node, cg);
   }


/*****************************************************************************
 * Float Representation:                                                     *
 *       ______________________________ ________________________________     *
 *      |S| Biased Exponent(8 bits)    |    Fraction(23 bits)           |    *
 *      |_|____________________________|________________________________|    *
 *      0 1                            9                                31   *
 *                                                                           *
 *   S               =  sign bit ( 1= Negative ; 0=positive)                 *
 *   Biased Exponent = Exponent + 0x7F = Exponent +127                       *
 *                     Representing exponent range (-126, ...+127)           *
 *   Fraction        = Fraction part of the float after normalization        *
 *****************************************************************************/

TR::Register *
f2lHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * floatRegister = cg->evaluate(firstChild);
   TR::Register * targetRegister = cg->allocateRegister();

   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

   //Assume Float.NaN
   generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetRegister, targetRegister);
   TR::RegisterDependencyConditions * dependencies = NULL;

   generateRRInstruction(cg, TR::InstOpCode::CEBR, node, floatRegister, floatRegister);
   // if NaN, then done
   // this path requires internal control flow
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, cFlowRegionEnd);
   dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
   dependencies->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(floatRegister, TR::RealRegister::AssignAny);

   // not NaN, do the conversion
   generateRRFInstruction(cg, TR::InstOpCode::CGEBR, node, targetRegister, floatRegister, (int8_t) 0x5, true);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::f2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return f2lHelper64(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::f2dEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * newReg = cg->allocateRegister(TR_FPR);
   TR::Register * newDoubleReg = unaryEvaluator(node, cg, TR::InstOpCode::LDEBR, newReg);
   newDoubleReg->setIsSinglePrecision(false);
   return newDoubleReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::d2iEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return convertToFixed(node, cg);
   }

/*****************************************************************************
 * Double Representation:                                                    *
 *       ______________________________ ________________________________     *
 *      |S| Biased Exponent(11 bits)   |    Fraction(52 bits)           |    *
 *      |_|____________________________|________________________________|    *
 *      0 1                            12                               63   *
 *                                                                           *
 *   S               =  sign bit ( 1= Negative ; 0=positive)                 *
 *   Biased Exponent = Exponent + 0x3FF = Exponent +1023                     *
 *                     Representing exponent range (-1022, ...+1023)         *
 *   Fraction        = Fraction part of the float after normalization        *
 *****************************************************************************/

TR::Register *
d2lHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   bool checkNaN = true;

   TR::Node * firstChild = node->getFirstChild();
   TR::Register * floatRegister = cg->evaluate(firstChild);
   TR::Register * targetRegister = cg->allocateRegister();

   TR::LabelSymbol * cFlowRegionStart = NULL;
   TR::LabelSymbol * label1 = NULL;
   // default result = 0
   TR::RegisterDependencyConditions * dependencies = NULL;
   if (checkNaN)
      {
      generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetRegister, targetRegister);
      label1 = generateLabelSymbol(cg);
      cFlowRegionStart = generateLabelSymbol(cg);
      // if NaN
      generateRXEInstruction(cg, TR::InstOpCode::TCDB, node, floatRegister, generateS390MemoryReference(0x00F, cg), 0);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNZ, node, label1);
      dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
      dependencies->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
      dependencies->addPostCondition(floatRegister, TR::RealRegister::AssignAny);
      }
   if (node->getOpCodeValue() == TR::d2lu)
      generateRRFInstruction(cg, TR::InstOpCode::CLGDBR, node, targetRegister, floatRegister, (int8_t) 0x5, true);
   else
      generateRRFInstruction(cg, TR::InstOpCode::CGDBR, node, targetRegister, floatRegister, (int8_t) 0x5, true);

   if (checkNaN)
      {// done label
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label1, dependencies);
      label1->setEndInternalControlFlow();
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::d2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return d2lHelper64(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::d2fEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * newReg = cg->allocateRegister(TR_FPR);
   return unaryEvaluator(node, cg, TR::InstOpCode::LEDBR, newReg);
   }

//*********************************************************************
// All Float comparators also handle equivalent double comparators
//*********************************************************************
TR::Register *
OMR::Z::TreeEvaluator::iffcmpeqEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpneEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpltEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgeEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpleEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpequEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpneuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpltuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgeuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgtuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpleuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpeqEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpneEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpltEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgeEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpleEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpequEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpneuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpltuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgeuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgtuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpleuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, true);
   }

////  GRA Support ////////////////////////////////////////////////////////////////////////

TR::Register *
OMR::Z::TreeEvaluator::fRegLoadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * globalReg = node->getRegister();
   TR_GlobalRegisterNumber globalRegNum;

   if(globalReg == NULL)
     {
     globalRegNum = node->getGlobalRegisterNumber();
     }

   if (globalReg == NULL)
      {
      globalReg = cg->allocateSinglePrecisionRegister();
      node->setRegister(globalReg);
      }

   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::dRegLoadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * globalReg = node->getRegister();
   TR_GlobalRegisterNumber globalRegNum;

   if(globalReg == NULL)
     {
     globalRegNum = node->getGlobalRegisterNumber();
     }

   if (globalReg == NULL)
      {
      globalReg = cg->allocateSinglePrecisionRegister();
      node->setRegister(globalReg);
      }

   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::fRegStoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * child = node->getFirstChild();

   TR::Register * globalReg=NULL;
   TR_GlobalRegisterNumber globalRegNum = node->getGlobalRegisterNumber();

   globalReg = cg->evaluate(child);

   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::dRegStoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * child = node->getFirstChild();

   TR::Register * globalReg=NULL;
   TR_GlobalRegisterNumber globalRegNum = node->getGlobalRegisterNumber();

   globalReg = cg->evaluate(child);

   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::dceilEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::floorCeilEvaluator(node, cg, TR::InstOpCode::FIDBR, (int8_t)0x6);
   }

TR::Register *
OMR::Z::TreeEvaluator::dfloorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::floorCeilEvaluator(node, cg, TR::InstOpCode::FIDBR, (int8_t)0x7);
   }

TR::Register *
OMR::Z::TreeEvaluator::fceilEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::floorCeilEvaluator(node, cg, TR::InstOpCode::FIEBR, (int8_t)0x6);
   }

TR::Register *
OMR::Z::TreeEvaluator::ffloorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::floorCeilEvaluator(node, cg, TR::InstOpCode::FIEBR, (int8_t)0x7);
   }

TR::Register *
OMR::Z::TreeEvaluator::floorCeilEvaluator(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic regToRegOpCode, int8_t mask)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcRegister = cg->gprClobberEvaluate(firstChild);
   TR::Register * targetRegister = srcRegister;

   generateRRFInstruction(cg, regToRegOpCode, node, targetRegister, srcRegister, mask, true);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }
