/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

static void fpBitsMovHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::Register *trgReg, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);

   generateTrg1Src1Instruction(cg, op, node, trgReg, srcReg);

   cg->decReferenceCount(child);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateSinglePrecisionRegister();

   fpBitsMovHelper(node, TR::InstOpCode::fmov_wtos, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();

   fpBitsMovHelper(node, TR::InstOpCode::fmov_stow, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);

   fpBitsMovHelper(node, TR::InstOpCode::fmov_xtod, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();

   fpBitsMovHelper(node, TR::InstOpCode::fmov_dtox, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateSinglePrecisionRegister();
   TR::Register *tmpReg = cg->allocateRegister();

   union {
      float f;
      int32_t i;
   } fvalue;

   fvalue.f = node->getFloat();
   loadConstant32(cg, node, fvalue.i, tmpReg);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fmov_wtos, node, trgReg, tmpReg);
   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);
   TR::Register *tmpReg = cg->allocateRegister();

   union {
      double d;
      int64_t l;
   } dvalue;

   dvalue.d = node->getDouble();
   loadConstant64(cg, node, dvalue.l, tmpReg);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fmov_xtod, node, trgReg, tmpReg);
   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   return trgReg;
   }

// also handles floadi
TR::Register *
OMR::ARM64::TreeEvaluator::floadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::vldrimms, 4, cg);
   }

// also handles dloadi
TR::Register *
OMR::ARM64::TreeEvaluator::dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::vldrimmd, 8, cg);
   }

// also handles fstorei
TR::Register *
OMR::ARM64::TreeEvaluator::fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::vstrimms, 4, cg);
   }

// also handles dstorei
TR::Register *
OMR::ARM64::TreeEvaluator::dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::vstrimmd, 8, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getFloatReturnRegister(), TR_FPR, TR_FloatReturn, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getDoubleReturnRegister(), TR_FPR, TR_DoubleReturn, cg);
   }

static TR::Register *
commonFpEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, bool isDouble, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg;

   trgReg = isDouble ? cg->allocateRegister(TR_FPR) : cg->allocateSinglePrecisionRegister();
   generateTrg1Src2Instruction(cg, op, node, trgReg, src1Reg, src2Reg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   node->setRegister(trgReg);
   return trgReg;
   }

static TR::Register *
singlePrecisionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   return commonFpEvaluator(node, op, false, cg);
   }

static TR::Register *
doublePrecisionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   return commonFpEvaluator(node, op, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::faddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::fadds, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::daddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::faddd, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::fsubs, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::fsubd, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::fmuls, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::fmuld, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::fdivs, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::fdivd, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::fremEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dremEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static TR::Register *
commonFpUnaryEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, bool isDouble, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *trgReg;

   if (firstChild->getReferenceCount() > 1)
      {
      trgReg = isDouble ? cg->allocateRegister(TR_FPR) : cg->allocateSinglePrecisionRegister();
      }
   else
      {
      trgReg = srcReg;
      }
   generateTrg1Src1Instruction(cg, op, node, trgReg, srcReg);
   cg->decReferenceCount(firstChild);
   node->setRegister(trgReg);
   return trgReg;
   }

static TR::Register *
singlePrecisionUnaryEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   return commonFpUnaryEvaluator(node, op, false, cg);
   }

static TR::Register *
doublePrecisionUnaryEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   return commonFpUnaryEvaluator(node, op, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionUnaryEvaluator(node, TR::InstOpCode::fabss, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionUnaryEvaluator(node, TR::InstOpCode::fabsd, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionUnaryEvaluator(node, TR::InstOpCode::fnegs, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionUnaryEvaluator(node, TR::InstOpCode::fnegd, cg);
   }

static TR::Register *
intFpTypeConversionHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::Register *trgReg;

   TR::DataType dt = node->getDataType();
   if (dt.isFloatingPoint())
      {
      trgReg = dt.isDouble() ? cg->allocateRegister(TR_FPR) : cg->allocateSinglePrecisionRegister();
      }
   else
      {
      trgReg = cg->allocateRegister();
      }

   generateTrg1Src1Instruction(cg, op, node, trgReg, srcReg);

   child->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::scvtf_wtos, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::scvtf_wtod, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::scvtf_xtos, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::scvtf_xtod, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::fcvt_stod, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::fcvtzs_stow, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::fcvtzs_dtow, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::d2cEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::d2sEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::d2bEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::fcvtzs_stox, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::fcvtzs_dtox, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return intFpTypeConversionHelper(node, TR::InstOpCode::fcvt_dtos, cg);
   }

static TR::Instruction *iffcmpHelper(TR::Node *node, TR::ARM64ConditionCode cc, bool isDouble, bool needsExplicitUnorderedCheck, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = NULL;
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::InstOpCode::Mnemonic op;
   bool useRegCompare = true;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      double value = isDouble ? secondChild->getDouble() : secondChild->getFloat();
      if (value == 0.0)
         {
         op = isDouble ? TR::InstOpCode::fcmpd_zero : TR::InstOpCode::fcmps_zero;
         generateSrc1Instruction(cg, op, node, src1Reg);
         useRegCompare = false;
         }
      }

   if (useRegCompare)
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      op = isDouble ? TR::InstOpCode::fcmpd : TR::InstOpCode::fcmps;
      generateSrc2Instruction(cg, op, node, src1Reg, src2Reg);
      }

   TR::LabelSymbol *dstLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::Instruction *result;
   if (node->getNumChildren() == 3)
      {
      thirdChild = node->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare must be a TR::GlRegDeps");

      cg->evaluate(thirdChild);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      if (!needsExplicitUnorderedCheck)
         {
         result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc, deps);
         }
      else
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc);
         result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, TR::CC_VS, deps);
         }
      }
   else
      {
      result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc);
      if (needsExplicitUnorderedCheck)
         {
         result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, TR::CC_VS);
         }
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   if (thirdChild)
      {
      thirdChild->decReferenceCount();
      }
   return result;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_EQ, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_NE, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_MI, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_GE, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_GT, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_LS, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_EQ, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_NE, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_MI, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_GE, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_GT, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_LS, true, false, cg);
   return NULL;
   }

static TR::Register *fcmpHelper(TR::Node *node, TR::ARM64ConditionCode cc, bool isDouble, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::InstOpCode::Mnemonic op;
   bool useRegCompare = true;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      double value = isDouble ? secondChild->getDouble() : secondChild->getFloat();
      if (value == 0.0)
         {
         op = isDouble ? TR::InstOpCode::fcmpd_zero : TR::InstOpCode::fcmps_zero;
         generateSrc1Instruction(cg, op, node, src1Reg);
         useRegCompare = false;
         }
      }

   if (useRegCompare)
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      op = isDouble ? TR::InstOpCode::fcmpd : TR::InstOpCode::fcmps;
      generateSrc2Instruction(cg, op, node, src1Reg, src2Reg);
      }

   generateCSetInstruction(cg, node, trgReg, cc);

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_EQ, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_NE, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_MI, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_GE, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_GT, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_LS, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_EQ, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_NE, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_MI, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_GE, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_GT, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fcmpHelper(node, TR::CC_LS, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dcmplEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

// also handles dRegLoad
TR::Register *
OMR::ARM64::TreeEvaluator::fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      if (node->getOpCodeValue() == TR::fRegLoad)
         globalReg = cg->allocateSinglePrecisionRegister();
      else
         globalReg = cg->allocateRegister(TR_FPR);
      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::fmaxEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::fminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::fminEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::b2dEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::s2dEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::su2dEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_EQ, false, true, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_NE, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_LT, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_PL, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_HI, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iffcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_LE, false, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_EQ, true, true, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_NE, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_LT, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_PL, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_HI, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   iffcmpHelper(node, TR::CC_LE, true, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dcmpequEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dcmpltuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dcmpgeuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dcmpgtuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dcmpleuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::dcmpgEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

// also handles dselect
TR::Register *
OMR::ARM64::TreeEvaluator::fselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *condNode = node->getChild(0);
   TR::Node *trueNode = node->getChild(1);
   TR::Node *falseNode = node->getChild(2);

   TR::Register *condReg = cg->evaluate(condNode);
   TR::Register *trueReg = cg->evaluate(trueNode);
   TR::Register *falseReg = cg->evaluate(falseNode);
   TR::Register *resultReg = trueReg;

   bool isDouble = trueNode->getDataType().isDouble();
   TR::InstOpCode::Mnemonic movOp = isDouble ? TR::InstOpCode::fmovd : TR::InstOpCode::fmovs;

   if (!cg->canClobberNodesRegister(trueNode))
      {
      resultReg = isDouble ? cg->allocateRegister(TR_FPR) : cg->allocateSinglePrecisionRegister();
      generateTrg1Src1Instruction(cg, movOp, node, resultReg, trueReg);
      }

   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   generateCompareBranchInstruction(cg, TR::InstOpCode::cbnzx, node, condReg, doneLabel);
   generateTrg1Src1Instruction(cg, movOp, node, resultReg, falseReg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   node->setRegister(resultReg);
   cg->decReferenceCount(condNode);
   cg->decReferenceCount(trueNode);
   cg->decReferenceCount(falseNode);

   return resultReg;
   }
