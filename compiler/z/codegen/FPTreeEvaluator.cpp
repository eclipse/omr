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

#include <stddef.h>                                 // for NULL, size_t
#include <stdint.h>                                 // for int64_t, int32_t, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction
#include "codegen/Linkage.hpp"                      // for Linkage
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"                      // for Machine, etc
#include "codegen/MemoryReference.hpp"              // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"                         // for TR_HeapMemory, etc
#include "env/jittypes.h"                           // for uintptrj_t
#include "il/DataTypes.hpp"                         // for DataTypes, etc
#include "il/ILOpCodes.hpp"                         // for ILOpCodes::lu2f, etc
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode
#include "il/symbol/AutomaticSymbol.hpp"            // for AutomaticSymbol
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/Bit.hpp"                            // for isEven
#include "infra/List.hpp"                           // for List
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "runtime/Runtime.hpp"
#include "z/codegen/BinaryAnalyser.hpp"
#include "z/codegen/BinaryCommutativeAnalyser.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"

static TR::InstOpCode::Mnemonic getIntToFloatLogicalConversion(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic convertOpCode)
   {
   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12))
         {
         generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, targetRegister, targetRegister, shift_amount, (int8_t)(63|0x80), 0);
         }
      else if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
         {
         generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, targetRegister, targetRegister, shift_amount, (int8_t)(63|0x80), 0);
         }
      else
         {
         generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, targetRegister, shift_amount);
         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, targetRegister, targetRegister, shift_amount);
         }

      cg->ensure64BitRegister(targetRegister);
      }
   else
      {
      if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
         {
         generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetRegister, targetRegister, 32+shift_amount, (int8_t)(63|0x80), 0);
         }
      else
         {
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, targetRegister, shift_amount);
         generateRSInstruction(cg, TR::InstOpCode::SRL, node, targetRegister, shift_amount);
         }
      }
   }

inline void
genArithmeticConversionForInt(TR::Node * node, TR::CodeGenerator * cg, TR::Register * targetRegister, int8_t shift_amount)
   {
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, targetRegister, shift_amount);
      generateRSInstruction(cg, TR::InstOpCode::SRAG, node, targetRegister, targetRegister, shift_amount);
      cg->ensure64BitRegister(targetRegister);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, targetRegister, shift_amount);
      generateRSInstruction(cg, TR::InstOpCode::SRA, node, targetRegister, shift_amount);
      }
   }

//Convert the targetRegister value to appropriate integer type
static void generateValueConversionToIntType(TR::CodeGenerator *cg, TR::Node *node, TR::Register *targetRegister)
   {
   int32_t nshift = 0;
   bool use64bit = TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit();

   //switch (node->getDataType())
   switch (node->getOpCodeValue())
      {
      case TR::f2c:
      case TR::d2c:
        //case TR_UInt16:
         nshift = use64bit ? 48 : 16;
         genLogicalConversionForInt(node, cg, targetRegister, nshift);
         break;
      case TR::f2s:
      case TR::d2s:
        //case TR_SInt16:
         nshift = use64bit ? 48 : 16;
         genArithmeticConversionForInt(node, cg, targetRegister, nshift);
         break;
      case TR::f2b:
      case TR::d2b:
        //case TR_SInt8:
         nshift = use64bit ? 56 : 24;
         genArithmeticConversionForInt(node, cg, targetRegister, nshift);
         break;
      case TR::f2bu:
      case TR::d2bu:
         //case TR_UInt8:
         nshift = use64bit ? 56 : 24;
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
   TR::Instruction *cursor = NULL;

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
         generateRILInstruction(cg, TR::InstOpCode::XILF, node, targetRegister, (uintptrj_t)0x80000000);

         //5) Convert the targetRegister value to appropriate integer type, if needed
         generateValueConversionToIntType(cg, node, targetRegister);

         // Clean-up copied source register
         cg->stopUsingRegister(srcRegCpy);
         }
      }
   else  // Signed Target Path
      {
      //1) Reset targetRegister
      cursor = generateRIInstruction(cg, TR::InstOpCode::LA, node, targetRegister, 0);

      // Java expect that for signed conversion to fixed, if src float is NaN, target to have 0.0.
      //2) NaN test and branch to done
      TR::LabelSymbol *doneLabel = NULL;

      doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      generateRRInstruction(cg, compareOp, node, srcRegister, srcRegister);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BORC, node, doneLabel);   //NaN results in CC3
      cursor->setStartInternalControlFlow();

      //3) Convert to Fixed
      generateRRFInstruction(cg, convertOp, node, targetRegister, srcRegister, (int8_t) 0x5, true);

      //4) Convert the targetRegister value to appropriate integer type, if needed
      generateValueConversionToIntType(cg, node, targetRegister);

      //5) Set up register dependencies
      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 4, cg);
      deps->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(srcRegister, TR::RealRegister::AssignAny);


      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, deps);
      cursor->setEndInternalControlFlow();
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
         nshift = (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) ? 48 : 16;
         genArithmeticConversionForInt(node, cg, srcRegister, nshift);
         break;
     case TR::su2f:
     case TR::su2d:
	 //case TR_UInt16    :
         nshift = (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) ? 48 : 16;
         genLogicalConversionForInt(node, cg, srcRegister, nshift);
         break;
     case TR::b2f:
     case TR::b2d:
	 //case TR_SInt8    :
         nshift = (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) ? 56 : 24;
         genArithmeticConversionForInt(node, cg, srcRegister, nshift);
         break;
     case TR::bu2f:
     case TR::bu2d:
	 //case TR_UInt8    :
         nshift = (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) ? 56 : 24;
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
      TR::LabelSymbol * skipAddLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::Instruction *cursor;
      TR::RegisterDependencyConditions * deps = NULL;
      skipAddLabel->setEndInternalControlFlow();
      generateRRInstruction(cg, TR::InstOpCode::LTR, node, srcRegister, srcRegister);
      // if positive or zero, done BNLRC
      // otherwise, add double constant 0x41f0000000000000 to converted value
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, node, skipAddLabel);
      cursor->setStartInternalControlFlow();
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
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * Handles l2f,l2d,l2e
 */
TR::Register *
commonLong2FloatEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::InstOpCode::Mnemonic convertOp = TR::InstOpCode::BAD;
   TR::InstOpCode::Mnemonic addOp = TR::InstOpCode::BAD;
   bool isUnsigned = false;
   bool is128 = false;
   bool is64 = false;
   switch (node->getOpCodeValue())
      {
      case TR::lu2f:
         isUnsigned = true;
         // fall-through
      case TR::l2f:
         convertOp = TR::InstOpCode::CEGBR;
         addOp = TR::InstOpCode::AEB;
         break;
      case TR::lu2d:
         isUnsigned = true;
         // fall-through
      case TR::l2d:
         is64 = true;
         convertOp = TR::InstOpCode::CDGBR;
         addOp = TR::InstOpCode::ADB;
         break;
      default:
         TR_ASSERT(false,"unexpected opcode %d in commonLong2FloatEvaluator\n",node->getOpCodeValue());
      }

   TR::Node *litBaseNode = node->getNumChildren() == 2 ? node->getSecondChild() : NULL;
   TR::Register *targetReg = NULL;
   if (is128)
      targetReg = cg->allocateFPRegisterPair();
   else
      targetReg = cg->allocateRegister(TR_FPR);

   bool restrictToGPR0 = false;
   uint8_t depsNeeded = 0;
   if (restrictToGPR0)
      depsNeeded += 1;  // GPR0

   if (isUnsigned)
      {
      depsNeeded += 1; // litBase
      if (is128)
         depsNeeded += 6; // 2 regPair + 4 legal FP
      else
         depsNeeded += 1; // targetReg
      }
   TR::RegisterDependencyConditions *deps = depsNeeded > 0 ? new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, depsNeeded, cg) : NULL;
   TR::Node *srcNode = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(srcNode);
   TR::Register *gprTemp64 = cg->allocate64bitRegister();

   // the 64 bit register gprTemp64 is going to be clobbered and R0 is safe to clobber (so are R1,R15)
   if (restrictToGPR0)
      deps->addPostCondition(gprTemp64, TR::RealRegister::GPR0);
   generateRSInstruction(cg, TR::InstOpCode::SLLG, node, gprTemp64, srcReg->getHighOrder(), 32);
   TR::Instruction *cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, gprTemp64, srcReg->getLowOrder());

   // Convert from fixed to floating point
   generateRRInstruction(cg, convertOp, node, targetReg, gprTemp64);

   // For unsigned, we need to fix case where long_max < unsignedSrc <= ulong_max
   TR::LabelSymbol *doneLabel = deps ? TR::LabelSymbol::create(cg->trHeapMemory(),cg) : NULL; // attach all deps to this label at end
   if (isUnsigned)
      {
      TR::Register *litBase = NULL;
      bool stopUsingMemRefRegs = false;
      if (litBaseNode)
         {
         litBase = cg->evaluate(litBaseNode);
         cg->decReferenceCount(litBaseNode);
         }
      else if (cg->isLiteralPoolOnDemandOn())
         {
         litBase = cg->allocateRegister();
         generateLoadLiteralPoolAddress(cg, node, litBase);
         stopUsingMemRefRegs = true;
         }
      else
         {
         litBase = cg->getLitPoolRealRegister();
         }

      TR::Register *tempFloatReg = NULL;
      TR::MemoryReference * two_pow_64_hi = NULL;
      TR::MemoryReference * two_pow_64_lo = NULL;
      if (is128)
         {
         deps->addPostCondition(targetReg, TR::RealRegister::FPPair);
         deps->addPostCondition(targetReg->getHighOrder(), TR::RealRegister::LegalFirstOfFPPair);
         deps->addPostCondition(targetReg->getLowOrder(), TR::RealRegister::LegalSecondOfFPPair);

         tempFloatReg = cg->allocateFPRegisterPair();

         deps->addPostCondition(tempFloatReg, TR::RealRegister::FPPair);
         deps->addPostCondition(tempFloatReg->getHighOrder(), TR::RealRegister::LegalFirstOfFPPair);
         deps->addPostCondition(tempFloatReg->getLowOrder(), TR::RealRegister::LegalSecondOfFPPair);
         two_pow_64_hi = generateS390MemoryReference((int64_t)CONSTANT64(0x403f000000000000),
                                                     TR::Int64,
                                                     cg,
                                                     litBase);
         two_pow_64_lo = generateS390MemoryReference((int64_t)0,
                                                     TR::Int64,
                                                     cg,
                                                     litBase);
         }
      else
         {  // float add -2^63 + 2^64 triggers inexact exception in 31bit. Workaround is lengthen to double and add

         deps->addPostCondition(targetReg, TR::RealRegister::AssignAny);
         two_pow_64_hi = generateS390MemoryReference((int64_t)CONSTANT64(0x43f0000000000000),
                                                  TR::Int64,
                                                  cg,
                                                  litBase);
         }

      deps->addPostCondition(litBase, TR::RealRegister::AssignAny);
      generateRRInstruction(cg, TR::InstOpCode::LTGR, node, gprTemp64, gprTemp64);
      // the fixed to float convertOp only handles signed source operands so if the converted value is negative and the
      // source was an unsigned number then add 2^64 to correct the value
      TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      startLabel->setStartInternalControlFlow();
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startLabel, deps);
      TR::Instruction *cursor =generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, node, doneLabel);
      if (is128)
         {
         generateRXInstruction(cg, TR::InstOpCode::LD, node, tempFloatReg->getHighOrder(), two_pow_64_hi);
         generateRXInstruction(cg, TR::InstOpCode::LD, node, tempFloatReg->getLowOrder(), two_pow_64_lo);
         generateRRInstruction(cg, addOp, node, targetReg, tempFloatReg);
         }
      else if(is64)
         {
         generateRXInstruction(cg, addOp, node, targetReg, two_pow_64_hi);
         }
      else
         {
         // float add -2^63 + 2^64 triggers inexact exception in 31bit. Workaround is lengthen to double and add
         addOp = TR::InstOpCode::ADB;
         generateRRInstruction(cg, TR::InstOpCode::LDEBR, node, targetReg, targetReg);
         generateRXInstruction(cg, addOp, node, targetReg, two_pow_64_hi);
         generateRRInstruction(cg, TR::InstOpCode::LEDBR, node, targetReg, targetReg);
         }

      doneLabel->setEndInternalControlFlow();
      cg->stopUsingRegister(tempFloatReg);
      if (stopUsingMemRefRegs)
         {
         if (two_pow_64_hi)
            two_pow_64_hi->stopUsingMemRefRegister(cg);
         if (two_pow_64_lo)
            two_pow_64_lo->stopUsingMemRefRegister(cg);
         }
         
      if (cg->isLiteralPoolOnDemandOn())
         {
         cg->stopUsingRegister(litBase);
         }
      }
   else
      {
      // TODO: stop attaching litBaseNode in the l2d (signed) case
      if (litBaseNode)
         {
         cg->evaluate(litBaseNode);
         cg->decReferenceCount(litBaseNode);
         }
      }

   if (doneLabel)
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, deps);
   cg->stopUsingRegister(gprTemp64);
   cg->decReferenceCount(srcNode);

   node->setRegister(targetReg);
   return targetReg;
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

   //traceMsg(comp,"In fload evaluator for Node %p. isRegisterSymbol = %d\n",node,node->getSymbolReference()->getSymbol()->isRegisterSymbol());

   if (node->getSymbolReference()->getSymbol()->isRegisterSymbol())
     return TR::TreeEvaluator::fRegLoadEvaluator(node, cg);
   else
     return floadHelper(node, cg, NULL);
   }

TR::Register *
OMR::Z::TreeEvaluator::dloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (node->getSymbolReference()->getSymbol()->isRegisterSymbol())
     return TR::TreeEvaluator::dRegLoadEvaluator(node, cg);
   else
     return dloadHelper(node, cg, NULL);
   }

TR::Register *
OMR::Z::TreeEvaluator::fstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (node->getSymbolReference()->getSymbol()->isRegisterSymbol())
     TR::TreeEvaluator::fRegStoreEvaluator(node, cg);
   else
     fstoreHelper(node, cg);
   return NULL;
   }

TR::Register *
OMR::Z::TreeEvaluator::dstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (node->getSymbolReference()->getSymbol()->isRegisterSymbol())
     TR::TreeEvaluator::dRegStoreEvaluator(node, cg);
   else
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

/**
 * Generate code for a % b for both float and double.
 * The code calls out to an assembler version of fmod that uses standard
 * JIT linkage, except that it is known that the function does not kill
 * any registers other than FPR0/FPR1/FPR2/GPR-ra
 * The call-out does not always happen - in fact, it is hopefully rare.
 * The DIxBR instruction is used to do a divide-to-integer. The call-out is
 * only done if the result is not exact.
 */
TR::Register *
OMR::Z::TreeEvaluator::floatRemHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction * cursor = NULL;
   TR::Register * firstRegister = cg->fprClobberEvaluate(node->getFirstChild());
   TR::Register * secondRegister = cg->fprClobberEvaluate(node->getSecondChild());
   TR::Register * tempRegister = cg->allocateRegister(TR_FPR);
   TR::Register * targetRegister = cg->allocateRegister(TR_FPR);
   TR::Register * dummyRegister3 = cg->allocateRegister(TR_FPR);
   TR::Register * dummyRegister4 = cg->allocateRegister(TR_FPR);
   TR::Register * dummyRegister5 = cg->allocateRegister(TR_FPR);
   TR::Register * dummyRegister6 = cg->allocateRegister(TR_FPR);
   TR::Register * dummyRegister7 = cg->allocateRegister(TR_FPR);
   TR::LabelSymbol * labelNotExact = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * labelOK = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::Register * raReg = cg->allocateRegister();

   //
   // Algorithm: c = a rem b;
   //              target = a;
   //              target = DivideToInteger(target, b, tmp);
   //              if (target inexact) target = fmod(a,b);
   //              if (target is +0 AND a is < +0) then target is -0;
   // The check for the special case of 0's should be on rare path code, not in the mainline


   if (node->getDataType() == TR::Float)
      {
      generateRRInstruction(cg, TR::InstOpCode::LER, node, targetRegister, firstRegister);
      labelOK->setEndInternalControlFlow();
      cursor = generateRRFInstruction(cg, TR::InstOpCode::DIEBR, node, targetRegister, secondRegister, tempRegister, 0x5);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BCRC, node, labelNotExact);    // remainder not exact
      cursor->setStartInternalControlFlow();
      generateRXInstruction(cg, TR::InstOpCode::TCEB, node, targetRegister, 0x800);  // c is +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, labelOK);        // it is not +0
      generateRXInstruction(cg, TR::InstOpCode::TCEB, node, firstRegister, 0x555);   // a is < +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, labelOK);        // it is not < +0
      generateRRInstruction(cg, TR::InstOpCode::LCEBR, node, targetRegister, targetRegister); // negate answer to be -0
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelOK);        // it is not < +0
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LDR, node, targetRegister, firstRegister);
      labelOK->setEndInternalControlFlow();
      cursor = generateRRFInstruction(cg, TR::InstOpCode::DIDBR, node, targetRegister, secondRegister, tempRegister, 0x5);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BCRC, node, labelNotExact);    // remainder not exact
      cursor->setStartInternalControlFlow();
      generateRXInstruction(cg, TR::InstOpCode::TCDB, node, targetRegister, 0x800);  // c is +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, labelOK);        // it is not +0
      generateRXInstruction(cg, TR::InstOpCode::TCDB, node, firstRegister, 0x555);   // a is < +0 ?
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, labelOK);        // it is not < +0
      generateRRInstruction(cg, TR::InstOpCode::LCDBR, node, targetRegister, targetRegister); // negate answer to be -0
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelOK);        // it is not < +0
      }
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelNotExact);

   void * remHelper ;
   if (node->getDataType() == TR::Float)
     remHelper = (void *) cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390floatRemainder, false, false, false)->getMethodAddress();
   else
     remHelper = (void *) cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390doubleRemainder, false, false, false)->getMethodAddress();

   TR::RegisterDependencyConditions * postDeps = generateRegisterDependencyConditions(0, 12, cg);

   genLoadAddressConstant(cg, node, (uintptrj_t) remHelper, raReg, NULL, postDeps, NULL);

   cursor = generateRRInstruction(cg, TR::InstOpCode::BASR, node, raReg, raReg);

   TR::Linkage * linkage = cg->getS390Linkage();
   postDeps->addPostCondition(raReg, linkage->getReturnAddressRegister());
   postDeps->addPostCondition(targetRegister, TR::RealRegister::FPR0);
   postDeps->addPostCondition(firstRegister, TR::RealRegister::FPR1);
   postDeps->addPostCondition(secondRegister, TR::RealRegister::FPR2);
   postDeps->addPostCondition(dummyRegister3, TR::RealRegister::FPR3);
   postDeps->addPostCondition(dummyRegister4, TR::RealRegister::FPR4);
   postDeps->addPostCondition(dummyRegister5, TR::RealRegister::FPR5);
   postDeps->addPostCondition(dummyRegister6, TR::RealRegister::FPR6);
   postDeps->addPostCondition(dummyRegister7, TR::RealRegister::FPR7);
   postDeps->addPostCondition(tempRegister, TR::RealRegister::AssignAny);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelOK,postDeps);

   node->setRegister(targetRegister);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(raReg);
   cg->stopUsingRegister(firstRegister);
   cg->stopUsingRegister(secondRegister);
   cg->stopUsingRegister(dummyRegister3);
   cg->stopUsingRegister(dummyRegister4);
   cg->stopUsingRegister(dummyRegister5);
   cg->stopUsingRegister(dummyRegister6);
   cg->stopUsingRegister(dummyRegister7);
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
   if (TR::Compiler->target.cpu.getS390SupportsFPE() && !disabled)
      {
      TR::Register *tempreg;
      tempreg = cg->allocate64bitRegister();
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
   TR::Register * targetReg;
   TR::Register * sourceReg;
   TR::Instruction * cursor = NULL;
   TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
   //Disable FPE Version For Now Due To Performance Reasons
   bool disabled = true;
   sourceReg = cg->evaluate(node->getFirstChild());

   TR::SymbolReference * f2iSR = cg->allocateLocalTemp(TR::Int32);
   TR::MemoryReference * tempMR = generateS390MemoryReference(node, f2iSR, cg);

   TR::LabelSymbol * infinityNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * negativeInfinityNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * positiveInfinityNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * cleansedNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
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
   cleansedNumber->setEndInternalControlFlow();
   generateRXInstruction(cg, TR::InstOpCode::TCEB, node, sourceReg, (uint32_t) 0x03f);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, cleansedNumber);
   cursor->setStartInternalControlFlow();
   TR::MemoryReference * positiveInfinity = generateS390MemoryReference((int32_t)0x7f800000, TR::Int32, cg, litBase);
   TR::MemoryReference * negativeInfinity = generateS390MemoryReference((int32_t)0xff800000, TR::Int32, cg, litBase);
   TR::MemoryReference * NaN              = generateS390MemoryReference((int32_t)0x7fc00000, TR::Int32, cg, litBase);
   if (litBase)
      {
      deps->addPostCondition(litBase, TR::RealRegister::AssignAny);
      }
   generateRXInstruction(cg, TR::InstOpCode::TCEB, node, sourceReg, (uint32_t) 0x00f);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, infinityNumber);
   if (node->normalizeNanValues())
      {
      generateRXInstruction(cg, TR::InstOpCode::LE, node, sourceReg, NaN);
      }
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, infinityNumber);
   generateRXInstruction(cg, TR::InstOpCode::TCEB, node, sourceReg, (uint32_t) 0x010);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, positiveInfinityNumber);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, negativeInfinityNumber);
   generateRXInstruction(cg, TR::InstOpCode::LE, node, sourceReg, negativeInfinity);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, positiveInfinityNumber);
   generateRXInstruction(cg, TR::InstOpCode::LE, node, sourceReg, positiveInfinity);
   deps->addPostCondition(sourceReg, TR::RealRegister::AssignAny);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cleansedNumber, deps);
   generateRXInstruction(cg, TR::InstOpCode::STE, node, sourceReg, tempMR);
   if((TR::Compiler->target.cpu.getS390SupportsFPE()) && (!disabled))
      {
      TR::Register *targetReg = cg->allocate64bitRegister();
      generateRRInstruction(cg, TR::InstOpCode::LGDR, node, targetReg, sourceReg);
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, targetReg, targetReg, 32);
      node->setRegister(targetReg);
      cg->decReferenceCount(node->getFirstChild());
      if (node->getNumChildren() == 2)
         cg->decReferenceCount(node->getSecondChild());
      return targetReg;
      }

   TR::MemoryReference * tempMR1 = generateS390MemoryReference(node, f2iSR, cg);
   generateRXInstruction(cg, TR::InstOpCode::STE, node, sourceReg, tempMR1);

   TR::MemoryReference * tempMR2 = generateS390MemoryReference(node, f2iSR, cg);
   targetReg = genericLoadHelper<32, 32, MemReg>(node, cg, tempMR2, NULL, false, true);
   node->setRegister(targetReg);
   tempMR2->stopUsingMemRefRegister(cg);

   cg->decReferenceCount(node->getFirstChild());
   if (node->getNumChildren() == 2)
      cg->decReferenceCount(node->getSecondChild());
      
   if (cg->isLiteralPoolOnDemandOn())
      {
      cg->stopUsingRegister(litBase);
      }
      
   return targetReg;
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
   if((TR::Compiler->target.cpu.getS390SupportsFPE()) && (!disabled))
      {
      if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
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
         TR_ASSERT( false,"must ensure sourceReg->getHighOrder() is a 64BitReg in SLLG so local RA will use 64 bit instructions\n");
         TR::Register * targetReg = cg->allocateRegister(TR_FPR);
         TR::RegisterDependencyConditions * ldgrDeps = NULL;
         ldgrDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions( 0, 3, cg);
         sourceReg = cg->gprClobberEvaluate(firstChild);
         ldgrDeps->addPostCondition(sourceReg, TR::RealRegister::EvenOddPair);
         ldgrDeps->addPostCondition(sourceReg->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         ldgrDeps->addPostCondition(sourceReg->getLowOrder(), TR::RealRegister::LegalOddOfPair);
         generateRSInstruction(cg, TR::InstOpCode::SLLG, node, sourceReg->getHighOrder(), sourceReg->getHighOrder(), 32);
         generateRRInstruction(cg, TR::InstOpCode::LR, node, sourceReg->getHighOrder(), sourceReg->getLowOrder());
         TR::Instruction * cursor =  generateRRInstruction(cg, TR::InstOpCode::LDGR, node, targetReg, sourceReg);
         cursor->setDependencyConditions(ldgrDeps);
         cg->stopUsingRegister(sourceReg);
         node->setRegister(targetReg);
         cg->decReferenceCount(firstChild);
         return targetReg;
         }
      }
   else
      {
      TR::Register * targetReg = cg->allocateRegister(TR_FPR);
      sourceReg = cg->evaluate(firstChild);
      TR::SymbolReference * l2dSR = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempMR = generateS390MemoryReference(node, l2dSR, cg);
      TR::MemoryReference * tempMR1 = generateS390MemoryReference(node, l2dSR, cg);
      if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
         {
         generateRXInstruction(cg, TR::InstOpCode::STG, node, sourceReg, tempMR);
         }
      else
         {
         generateRSInstruction(cg, TR::InstOpCode::STM, node, sourceReg, tempMR);
         }
      generateRXInstruction(cg, TR::InstOpCode::LD, node, targetReg, tempMR1);
      node->setRegister(targetReg);
      cg->decReferenceCount(firstChild);
      return targetReg;
      }
   }

/**
 * Type-coerce double to long
 */
TR::Register *
OMR::Z::TreeEvaluator::dbits2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
 // Disable For Now Due To Performance Reasons
   bool disabled = true;
   TR::Compilation *comp = cg->comp();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetReg = NULL;
   TR::Register * sourceReg = NULL;
   TR::Instruction * cursor = NULL;
   TR::LabelSymbol * infinityNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * negativeInfinityNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * positiveInfinityNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * cleansedNumber = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
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
   TR::MemoryReference * positiveInfinity = generateS390MemoryReference((int64_t)CONSTANT64(0x7ff0000000000000), TR::Int64, cg, litBase);
   TR::MemoryReference * negativeInfinity = generateS390MemoryReference((int64_t)CONSTANT64(0xfff0000000000000), TR::Int64, cg, litBase);
   TR::MemoryReference * NaN              = generateS390MemoryReference((int64_t)CONSTANT64(0x7ff8000000000000), TR::Int64, cg, litBase);
   if ((TR::Compiler->target.cpu.getS390SupportsFPE()) && (!disabled))
      {
      if(TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
         {
         TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
         targetReg = cg->allocateRegister();
         //Not clobber evaluating because non FPSE path doesnt
         sourceReg = cg->evaluate(firstChild);
         cleansedNumber->setEndInternalControlFlow();
         generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x03f);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, cleansedNumber);
         cursor->setStartInternalControlFlow();

         if (litBase)
            {
            deps->addPostCondition(litBase, TR::RealRegister::AssignAny);
            }
         generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x00f);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, infinityNumber);
         if (node->normalizeNanValues())
            {
            generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, NaN);
            }
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, infinityNumber);
         generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x010);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, positiveInfinityNumber);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, negativeInfinityNumber);
         generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, negativeInfinity);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, positiveInfinityNumber);
         generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, positiveInfinity);
         deps->addPostCondition(sourceReg, TR::RealRegister::AssignAny);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cleansedNumber, deps);
         generateRRInstruction(cg, TR::InstOpCode::LGDR, node, targetReg, sourceReg);
         if (node->getNumChildren() == 2)
            {
            cg->decReferenceCount(node->getSecondChild());
            }
         cg->decReferenceCount(firstChild);
         node->setRegister(targetReg);
         }
      else
         {
         TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
         //Not clobber evaluating because non FPSE path doesnt
         sourceReg = cg->evaluate(firstChild);
         TR::Register * evenReg = cg->allocate64bitRegister();
         TR::Register * oddReg = cg->allocate64bitRegister();
         targetReg = cg->allocateConsecutiveRegisterPair(oddReg, evenReg);
         TR::RegisterDependencyConditions * lgdrDeps = NULL;
         cleansedNumber->setEndInternalControlFlow();
         generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x03f);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, cleansedNumber);
         cursor->setStartInternalControlFlow();
         if (litBase)
            {
            deps->addPostCondition(litBase, TR::RealRegister::AssignAny);
            }
         generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x00f);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, infinityNumber);
         if (node->normalizeNanValues())
            {
            generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, NaN);
            }
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, infinityNumber);
         generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x010);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, positiveInfinityNumber);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, negativeInfinityNumber);
         generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, negativeInfinity);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, positiveInfinityNumber);
         generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, positiveInfinity);
         deps->addPostCondition(sourceReg, TR::RealRegister::AssignAny);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cleansedNumber, deps);
         generateRRInstruction(cg, TR::InstOpCode::LGDR, node, oddReg, sourceReg);
         generateRRInstruction(cg, TR::InstOpCode::LGR, node, evenReg , oddReg);
         cursor = generateRSInstruction(cg,TR::InstOpCode::SRLG, node, evenReg, evenReg, 32 );
         lgdrDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions( 0, 3, cg);
         lgdrDeps->addPostCondition(targetReg, TR::RealRegister::EvenOddPair);
         lgdrDeps->addPostCondition(evenReg, TR::RealRegister::LegalEvenOfPair);
         lgdrDeps->addPostCondition(oddReg, TR::RealRegister::LegalOddOfPair);
         cursor->setDependencyConditions(lgdrDeps);
         if (node->getNumChildren() == 2)
            {
            cg->decReferenceCount(node->getSecondChild());
            }
         cg->decReferenceCount(firstChild) ;
         node->setRegister(targetReg);
         }
      }
   else
      {
      TR::Register * sourceReg;
      TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
      sourceReg = cg->evaluate(firstChild);
      cleansedNumber->setEndInternalControlFlow();
      generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x03f);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, cleansedNumber);
      cursor->setStartInternalControlFlow();
      if (litBase)
         {
         deps->addPostCondition(litBase, TR::RealRegister::AssignAny);
         }
      generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x00f);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, infinityNumber);
      if (node->normalizeNanValues())
         {
         generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, NaN);
         }
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, infinityNumber);
      generateRXInstruction(cg, TR::InstOpCode::TCDB, node, sourceReg, (uint32_t) 0x010);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, positiveInfinityNumber);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, negativeInfinityNumber);
      generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, negativeInfinity);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cleansedNumber);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, positiveInfinityNumber);
      generateRXInstruction(cg, TR::InstOpCode::LD, node, sourceReg, positiveInfinity);
      deps->addPostCondition(sourceReg, TR::RealRegister::AssignAny);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cleansedNumber, deps);

      TR::SymbolReference * d2lSR = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempMR = generateS390MemoryReference(node, d2lSR, cg);
      generateRXInstruction(cg, TR::InstOpCode::STD, node, sourceReg, tempMR);
      TR::MemoryReference * tempMR1 = generateS390MemoryReference(node, d2lSR, cg);
#ifdef ZOS_LOAD
      targetReg = genericLoadHelper<64, 64, MemReg>(node, cg, tempMR1, NULL, false, true);
      node->setRegister(targetReg);
      tempMR1->stopUsingMemRefRegister(cg);
#else
      if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
         {
         targetReg = lloadHelper64(node, cg, tempMR1);
         }
      else
         {
         targetReg = lloadHelper(node, cg, tempMR1);
         }
#endif
      if (node->getNumChildren() == 2)
         {
         cg->decReferenceCount(node->getSecondChild());
         }
      cg->decReferenceCount(firstChild);
      }
      
   if (cg->isLiteralPoolOnDemandOn())
      {
      cg->stopUsingRegister(litBase);
      }
      
   return targetReg;
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
   TR_ASSERT( TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(), "l2dHelper64() is for 64bit code-gen only!");
   TR::Instruction * cursor;
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * longRegister = cg->evaluate(firstChild);
   TR::Register * targetFloatRegister = cg->allocateRegister(TR_FPR);

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196) && node->getOpCodeValue() == TR::lu2d)
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
      TR::LabelSymbol * skipAddLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 1, cg);
      skipAddLabel->setEndInternalControlFlow();

      generateRRInstruction(cg, TR::InstOpCode::LTGR, node, longRegister, longRegister);
      //if positive or zero, done BNLRC
      //otherwise, add double constant representing 2^64 to converted value
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, node, skipAddLabel);
      cursor->setStartInternalControlFlow();
      deps->addPostCondition(targetFloatRegister, TR::RealRegister::AssignAny);
      generateS390ImmOp(cg, TR::InstOpCode::ADB, node, targetFloatRegister, targetFloatRegister, (int64_t) CONSTANT64(0x43f0000000000000), deps);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAddLabel, deps);
      }

   node->setRegister(targetFloatRegister);
   cg->decReferenceCount(firstChild);

   return targetFloatRegister;
   }


TR::Register *
l2fHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(), "l2fHelper64() is for 64bit code-gen only!");
   TR::Instruction * cursor;
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
      TR::LabelSymbol * skipAddLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 1, cg);
      skipAddLabel->setEndInternalControlFlow();

      generateRRInstruction(cg, TR::InstOpCode::LTGR, node, longRegister, longRegister);
      //if positive or zero, done BNLRC
      //otherwise, add double constant 0x43f0000000000000 to converted value
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, node, skipAddLabel);
      cursor->setStartInternalControlFlow();
      deps->addPostCondition(targetFloatRegister, TR::RealRegister::AssignAny);
      //cannot use opcodes TR::InstOpCode::AE or TR::InstOpCode::AEB with generateS390ImmOp,
      //   so for now transfer from float to double, add constant, then back to float
      generateRRInstruction(cg, TR::InstOpCode::LDEBR, node, targetFloatRegister, targetFloatRegister);
      generateS390ImmOp(cg, TR::InstOpCode::ADB, node, targetFloatRegister, targetFloatRegister, (int64_t) CONSTANT64(0x43f0000000000000), deps);
      generateRRInstruction(cg, TR::InstOpCode::LEDBR, node, targetFloatRegister, targetFloatRegister);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAddLabel, deps);
      }

   node->setRegister(targetFloatRegister);
   cg->decReferenceCount(firstChild);

   return targetFloatRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::l2fEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return l2fHelper64(node, cg);
      }
#if defined(PYTHON) || defined(OMR_RUBY) || defined(JITTEST)
   else
      {
      return commonLong2FloatEvaluator(node, cg);
      }
#else
   else
      {
      TR::Node::recreate(node, TR::fcall);
      node->setSymbolReference(cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390jitMathHelperConvertLongToFloat, false, false, false));
      return cg->evaluate(node);
      }
#endif
   }

TR::Register *
OMR::Z::TreeEvaluator::l2dEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) ? l2dHelper64(node, cg) : commonLong2FloatEvaluator(node, cg);
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
f2lHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( TR::Compiler->target.is32Bit(), "f2lHelper() is for 32bit code-gen only!");

   TR::Instruction * cursor;
   TR::Node * firstChild = node->getFirstChild();

   TR::LabelSymbol * label2 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label3 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label4 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label5 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label6 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label7 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label8 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   TR::Register * floatRegister = cg->evaluate(firstChild);
   TR::Register * tempFloatRegister = cg->allocateRegister(TR_FPR);

   TR::Register * tempRegister = cg->allocateRegister();
   TR::Register * tempRegister2 = cg->allocateRegister();
   TR::SymbolReference * evenSR = cg->allocateLocalTemp(TR::Int32);
   TR::SymbolReference * oddSR = cg->allocateLocalTemp(TR::Int32);
   TR::MemoryReference * evenMR = generateS390MemoryReference(node, evenSR, cg);
   TR::MemoryReference * oddMR = generateS390MemoryReference(node, oddSR, cg);
   TR::Register * evenRegister = cg->allocateRegister();
   TR::Register * oddRegister = cg->allocateRegister();

   TR::Register * targetRegisterPair = cg->allocateConsecutiveRegisterPair(oddRegister, evenRegister);

   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);

   // WE assign this pair early to ensure better allocation across control flow
   dependencies->addPostCondition(targetRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(evenRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(oddRegister, TR::RealRegister::LegalOddOfPair);

   // We need to assign these registers early so that any spilling will occur at the
   // bottom of the loop.  Because the pair lives across the backedge, register pressure
   // could cause the pair to spill inside the loop.
   dependencies->addPostCondition(tempRegister, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(tempRegister2, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(tempFloatRegister, TR::RealRegister::AssignAny);

   //Assume result (long)0x0
   cursor = generateRIInstruction(cg, TR::InstOpCode::LA, node, evenRegister, 0);
   cursor = generateRIInstruction(cg, TR::InstOpCode::LA, node, oddRegister, 0);
   // Round FP towards zero
   cursor = generateRRFInstruction(cg, TR::InstOpCode::FIEBR, node, tempFloatRegister, floatRegister, (int8_t) 0x5, true);
   cursor = generateRXInstruction(cg, TR::InstOpCode::TCEB, node, tempFloatRegister, (uint32_t) 0xccf);
   //if result 0, done
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BMRC, node, label7);
   cursor->setStartInternalControlFlow();
   TR::MemoryReference * evenMRcopy = generateS390MemoryReference(*evenMR, 0, cg); // prevent duplicate use of memory reference
   cursor = generateRXInstruction(cg, TR::InstOpCode::STE, node, tempFloatRegister, evenMRcopy);
   //Test if NaN
   cursor = generateRXInstruction(cg, TR::InstOpCode::TCEB, node, tempFloatRegister, (uint32_t) 0x30);
   //If NaN , go to label6
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLRC, node, label6);
   //Load as positive FP
   cursor = generateRRInstruction(cg, TR::InstOpCode::LPEBR, node, tempFloatRegister, tempFloatRegister);
   cursor = generateRXInstruction(cg, TR::InstOpCode::STE, node, tempFloatRegister, oddMR);
   TR::MemoryReference * oddMRcopy = generateS390MemoryReference(*oddMR, 0, cg); // prevent duplicate use of memory reference
   cursor = generateRXInstruction(cg, TR::InstOpCode::L, node, oddRegister, oddMRcopy);
   cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegister, oddRegister);
   // shift by 23 bits, so that tempRegister will have biased exponent
   cursor = generateRSInstruction(cg, TR::InstOpCode::SRL, node, tempRegister, 23);
   //subtract exponent Bias
   cursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempRegister, -127);
   //tempRegister now has actual exponent value
   cursor = generateRIInstruction(cg, TR::InstOpCode::CHI, node, tempRegister, 63);
   // if exponent is bigger than 63, overflow , go to label 6
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, node, label6);
   //clear exponent bits, odd register will have only fraction
   cursor = generateS390ImmOp(cg, TR::InstOpCode::N, node, oddRegister, oddRegister, (int32_t) 0x007fffff, dependencies);
   cursor = generateRIInstruction(cg, TR::InstOpCode::LA, node, tempRegister2, 1);
   cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tempRegister2, 23);
   //tempRegister = 0x00800000
   cursor = generateRRInstruction(cg, TR::InstOpCode::OR, node, oddRegister, tempRegister2);
   //oddRegister has fraction in 0-22 bits and bit 23 is 1
   cursor = generateRIInstruction(cg, TR::InstOpCode::CHI, node, tempRegister, 23);
   //if exponent value is 23, go to label5
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, label5);
   //if exponent value is >23, go to label3
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BHRC, node, label3);
   // exponent value is less than 23
   cursor = generateRRInstruction(cg, TR::InstOpCode::LCR, node, tempRegister, tempRegister);
   //tempRegister now has complement of the exponent
   cursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempRegister, 23);
   //tempRegister now has (23-exponent)

   //shift fraction right by (23-exponent) bits
   //LABEL2
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label2);
   cursor = generateRSInstruction(cg, TR::InstOpCode::SRDL, node, targetRegisterPair, 1);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, tempRegister, label2);

   // go to label5
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, label5);

   //exponent value is >23
   //LABEL3
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label3);

   cursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempRegister, -23);
   //tempRegister now has (exponent - 23)

   //shift fraction left by (exponent-23) bits
   //LABEL4
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label4);
   cursor = generateRSInstruction(cg, TR::InstOpCode::SLDL, node, targetRegisterPair, 1);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, tempRegister, label4);

   //LABEL5
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label5);

   TR::MemoryReference * evenMRcopy2 = generateS390MemoryReference(*evenMR, 0, cg); // prevent duplicate use of memory reference
   cursor = generateSIInstruction(cg, TR::InstOpCode::TM, node, evenMRcopy2, (uint32_t) 0x00000080);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, label7);

   cursor = generateRRInstruction(cg, TR::InstOpCode::LCR, node, evenRegister, evenRegister);
   cursor = generateRRInstruction(cg, TR::InstOpCode::LCR, node, oddRegister, oddRegister);

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, label7);
   cursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, evenRegister, -1);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, label7);

   //LABEL6
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label6);

   cursor = generateRIInstruction(cg, TR::InstOpCode::LA, node, oddRegister, 1);
   cursor = generateRSInstruction(cg, TR::InstOpCode::SLDL, node, targetRegisterPair, 63);
   TR::MemoryReference * evenMRcopy3 = generateS390MemoryReference(*evenMR, 0, cg); // prevent duplicate use of memory reference
   cursor = generateSIInstruction(cg, TR::InstOpCode::TM, node, evenMRcopy3, (uint32_t) 0x00000080);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BORC, node, label7);

   cursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, evenRegister, -1);
   cursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, oddRegister, -1);

   // long value is 0xFFFFFFFF FFFFFFFF
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, label7);

   //LABEL7
   label7->setEndInternalControlFlow();
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label7, dependencies);

   node->setRegister(targetRegisterPair);
   cg->decReferenceCount(firstChild);
   cg->stopUsingRegister(tempFloatRegister);
   cg->stopUsingRegister(tempRegister);
   cg->stopUsingRegister(tempRegister2);
   evenMR->stopUsingMemRefRegister(cg);
   evenMRcopy->stopUsingMemRefRegister(cg);
   evenMRcopy2->stopUsingMemRefRegister(cg);
   evenMRcopy3->stopUsingMemRefRegister(cg);
   oddMR->stopUsingMemRefRegister(cg);
   oddMRcopy->stopUsingMemRefRegister(cg);

   return targetRegisterPair;
   }

TR::Register *
f2lHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(), "f2lHelper64() is for 64bit code-gen only!");
   TR::Instruction * cursor;
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * floatRegister = cg->evaluate(firstChild);
   TR::Register * targetRegister = cg->allocate64bitRegister();

   TR::LabelSymbol * doneLabel = NULL;

   //Assume Float.NaN
   cursor = generateRIInstruction(cg, TR::InstOpCode::LA, node, targetRegister, 0);
   TR::RegisterDependencyConditions * dependencies = NULL;

   doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   cursor = generateRRInstruction(cg, TR::InstOpCode::CEBR, node, floatRegister, floatRegister);
   // if NaN, then done
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BORC, node, doneLabel);
   // this path requires internal control flow
   doneLabel->setEndInternalControlFlow();
   cursor->setStartInternalControlFlow();
   dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
   dependencies->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(floatRegister, TR::RealRegister::AssignAny);

   // not NaN, do the conversion
   generateRRFInstruction(cg, TR::InstOpCode::CGEBR, node, targetRegister, floatRegister, (int8_t) 0x5, true);
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, dependencies);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::f2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) ? f2lHelper64(node, cg) : f2lHelper(node, cg);
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
d2lHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction * cursor;


   TR::Node * firstChild = node->getFirstChild();

   TR::LabelSymbol * label2 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label3 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label4 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label5 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label6 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label7 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * label8 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   TR::Register * floatRegister = cg->evaluate(firstChild);
   TR::Register * tempFloatRegister = cg->allocateRegister(TR_FPR);

   TR::Register * tempRegister = cg->allocateRegister();
   TR::Register * tempRegister2 = cg->allocateRegister();
   TR::SymbolReference * tempSR1 = cg->allocateLocalTemp(TR::Float);
   TR::SymbolReference * tempSR2 = cg->allocateLocalTemp(TR::Double);
   TR::MemoryReference * tempMR1 = generateS390MemoryReference(node, tempSR1, cg);
   TR::MemoryReference * tempMR2 = generateS390MemoryReference(node, tempSR2, cg);
   TR::Register * evenRegister = cg->allocateRegister();
   TR::Register * oddRegister = cg->allocateRegister();

   TR::Register * targetRegisterPair = cg->allocateConsecutiveRegisterPair(oddRegister, evenRegister);

   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);

   // We assign this pair early to ensure better allocation across control flow
   dependencies->addPostCondition(targetRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(evenRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(oddRegister, TR::RealRegister::LegalOddOfPair);

   // We need to assign these registers early so that any spilling will occur at the
   // bottom of the loop.  Because the pair lives across the backedge, register pressure
   // could cause the pair to spill inside the loop.
   dependencies->addPostCondition(tempRegister, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(tempRegister2, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(tempFloatRegister, TR::RealRegister::AssignAny);

   //Assume result (long)0x0
   generateRIInstruction(cg, TR::InstOpCode::LA, node, evenRegister, 0);
   generateRIInstruction(cg, TR::InstOpCode::LA, node, oddRegister, 0);
   // Round double towards zero
   generateRRFInstruction(cg, TR::InstOpCode::FIDBR, node, tempFloatRegister, floatRegister, (int8_t) 0x5, true);
   generateRXInstruction(cg, TR::InstOpCode::TCDB, node, tempFloatRegister, (uint32_t) 0xccf);
   // if 0 then done
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLRC, node, label7);
   cursor->setStartInternalControlFlow();

   generateRXInstruction(cg, TR::InstOpCode::STE, node, tempFloatRegister, tempMR1);
   generateRXInstruction(cg, TR::InstOpCode::TCDB, node, tempFloatRegister, (uint32_t) 0x30);
   //If NaN, go to label6
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLRC, node, label6);
   // Load as positive
   generateRRInstruction(cg, TR::InstOpCode::LPDBR, node, tempFloatRegister, tempFloatRegister);
   TR::MemoryReference * tempMR2copy = generateS390MemoryReference(*tempMR2, 0, cg);
   generateRXInstruction(cg, TR::InstOpCode::STD, node, tempFloatRegister, tempMR2copy);
   TR::MemoryReference * tempMR2copy2 = generateS390MemoryReference(*tempMR2, 0, cg);
   generateRSInstruction(cg, TR::InstOpCode::LM, node, targetRegisterPair, tempMR2copy2);
   generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegister, evenRegister);
   //shift by 20 bits, so that tempRegister will have biased exponent
   generateRSInstruction(cg, TR::InstOpCode::SRL, node, tempRegister, 20);
   //subtract the exponent bias
   generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempRegister, -1023);
   //tempRegister now has actual exponent value
   generateRIInstruction(cg, TR::InstOpCode::CHI, node, tempRegister, 63);
   // go to label6 , if exponent is bigger than 63, 'cause it is overflow
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, node, label6);
   //clear exponent bits, evenRegister will have only fraction

   generateS390ImmOp(cg, TR::InstOpCode::N, node, evenRegister, evenRegister, (int32_t) 0x000fffff, dependencies);
   generateRIInstruction(cg, TR::InstOpCode::LA, node, tempRegister2, 1);
   generateRSInstruction(cg, TR::InstOpCode::SLL, node, tempRegister2, 20);
   generateRRInstruction(cg, TR::InstOpCode::OR, node, evenRegister, tempRegister2);
   // 20th bit of evenRegister is made 1
   generateRIInstruction(cg, TR::InstOpCode::CHI, node, tempRegister, 52);
   //exponent == 52 -> go to label5
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, label5);
   //exponent > 52 -> go to label3
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BHRC, node, label3);
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, tempRegister, tempRegister);
   generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempRegister, 52);
   // tempRegister now have (52 - exponent)

   //Shift fraction right by (52-exponent) bits
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label2);
   generateRSInstruction(cg, TR::InstOpCode::SRDL, node, targetRegisterPair, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, tempRegister, label2);
   // go to label5
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, label5);

   //LABEL3
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label3);
   generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempRegister, -52);
   // tempRegister now have (exponent-52)

   //shift fraction left by (exponent-52) bits
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label4);
   generateRSInstruction(cg, TR::InstOpCode::SLDL, node, targetRegisterPair, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, tempRegister, label4);

   //LABEL5
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label5);
   TR::MemoryReference * tempMR1copy = generateS390MemoryReference(*tempMR1, 0, cg);
   generateSIInstruction(cg, TR::InstOpCode::TM, node, tempMR1copy, (uint32_t) 0x00000080);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZRC, node, label7);
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, evenRegister, evenRegister);
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, oddRegister, oddRegister);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, node, label7);
   generateRIInstruction(cg, TR::InstOpCode::AHI, node, evenRegister, -1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, label7);
   //LABEL6
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label6);
   generateRIInstruction(cg, TR::InstOpCode::LA, node, oddRegister, 1);
   generateRSInstruction(cg, TR::InstOpCode::SLDL, node, targetRegisterPair, 63);
   TR::MemoryReference * tempMR1copy2 = generateS390MemoryReference(*tempMR1, 0, cg);
   generateSIInstruction(cg, TR::InstOpCode::TM, node, tempMR1copy2, (uint32_t) 0x00000080);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BORC, node, label7);
   generateRIInstruction(cg, TR::InstOpCode::AHI, node, evenRegister, -1);
   generateRIInstruction(cg, TR::InstOpCode::AHI, node, oddRegister, -1);

   // long value is 0xFFFFFFFF FFFFFFFF
   //LABEL7
   label7->setEndInternalControlFlow();
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label7, dependencies);

   node->setRegister(targetRegisterPair);
   cg->decReferenceCount(firstChild);
   cg->stopUsingRegister(tempFloatRegister);
   cg->stopUsingRegister(tempRegister);
   cg->stopUsingRegister(tempRegister2);
   tempMR1->stopUsingMemRefRegister(cg);
   tempMR1copy->stopUsingMemRefRegister(cg);
   tempMR1copy2->stopUsingMemRefRegister(cg);
   tempMR2->stopUsingMemRefRegister(cg);
   tempMR2copy->stopUsingMemRefRegister(cg);
   tempMR2copy2->stopUsingMemRefRegister(cg);

   return targetRegisterPair;
   }


TR::Register *
d2lHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(), "d2lHelper64(...) is for 64bit code-gen only");
   bool checkNaN = true;

   TR::Node * firstChild = node->getFirstChild();
   TR::Register * floatRegister = cg->evaluate(firstChild);
   TR::Register * targetRegister = cg->allocate64bitRegister();

   TR::LabelSymbol * label1 = NULL;
   // default result = 0
   TR::RegisterDependencyConditions * dependencies = NULL;
   if (checkNaN)
      generateRIInstruction(cg, TR::InstOpCode::LA, node, targetRegister, 0);
   if (checkNaN)
      {
      label1 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      // if NaN
      generateRXInstruction(cg, TR::InstOpCode::TCDB, node, floatRegister, (uint32_t) 0x00f);

      TR::Instruction *cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNZRC, node, label1);
      cursor->setStartInternalControlFlow();
      label1->setEndInternalControlFlow();
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
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::d2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) ? d2lHelper64(node, cg) : d2lHelper(node, cg);
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
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, TR::InstOpCode::COND_BERC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpneEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNERC, TR::InstOpCode::COND_BNERC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpltEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLRC, TR::InstOpCode::COND_BHRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgeEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, TR::InstOpCode::COND_BNHRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BHRC, TR::InstOpCode::COND_BLRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpleEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNHRC, TR::InstOpCode::COND_BNLRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpequEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, TR::InstOpCode::COND_BERC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpneuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNERC, TR::InstOpCode::COND_BNERC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpltuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLRC, TR::InstOpCode::COND_BHRC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgeuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, TR::InstOpCode::COND_BNHRC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpgtuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BHRC, TR::InstOpCode::COND_BLRC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::iffcmpleuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNHRC, TR::InstOpCode::COND_BNLRC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpeqEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, TR::InstOpCode::COND_BERC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpneEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNERC, TR::InstOpCode::COND_BNERC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpltEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLRC, TR::InstOpCode::COND_BHRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgeEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, TR::InstOpCode::COND_BNHRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BHRC, TR::InstOpCode::COND_BLRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpleEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNHRC, TR::InstOpCode::COND_BNLRC, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpequEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BERC, TR::InstOpCode::COND_BERC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpneuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNERC, TR::InstOpCode::COND_BNERC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpltuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLRC, TR::InstOpCode::COND_BHRC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgeuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNLRC, TR::InstOpCode::COND_BNHRC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpgtuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BHRC, TR::InstOpCode::COND_BLRC, true);
   }

TR::Register *
OMR::Z::TreeEvaluator::fcmpleuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNHRC, TR::InstOpCode::COND_BNLRC, true);
   }

////  GRA Support ////////////////////////////////////////////////////////////////////////

TR::Register *
OMR::Z::TreeEvaluator::fRegLoadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * globalReg = node->getRegister();
   TR_GlobalRegisterNumber globalRegNum;

   if(globalReg == NULL)
     {
      if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isRegisterSymbol())
        {
        TR::AutomaticSymbol *regSym=node->getSymbolReference()->getSymbol()->castToRegisterSymbol();
        globalRegNum = regSym->getGlobalRegisterNumber();
        }
      else
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
      if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isRegisterSymbol())
        {
        TR::AutomaticSymbol *regSym=node->getSymbolReference()->getSymbol()->castToRegisterSymbol();
        globalRegNum = regSym->getGlobalRegisterNumber();
        }
      else
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
   TR_GlobalRegisterNumber globalRegNum;
   if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isRegisterSymbol())
     {
     TR::AutomaticSymbol *regSym=node->getSymbolReference()->getSymbol()->castToRegisterSymbol();
     globalRegNum = regSym->getGlobalRegisterNumber();
     }
   else
     globalRegNum = node->getGlobalRegisterNumber();

   globalReg = cg->evaluate(child);

   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::dRegStoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * child = node->getFirstChild();

   TR::Register * globalReg=NULL;
   TR_GlobalRegisterNumber globalRegNum;
   if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isRegisterSymbol())
     {
     TR::AutomaticSymbol *regSym=node->getSymbolReference()->getSymbol()->castToRegisterSymbol();
     globalRegNum = regSym->getGlobalRegisterNumber();
     }
   else
     globalRegNum = node->getGlobalRegisterNumber();

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
