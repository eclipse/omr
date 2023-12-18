/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#include "codegen/CodeGenerator.hpp"
#include "codegen/RVInstruction.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/LabelSymbol.hpp"


static void fpBitsMovHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::Register *trgReg, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::RealRegister *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);

   generateRTYPE(op, node, trgReg, srcReg, zero, cg);

   cg->decReferenceCount(child);
   }

TR::Register *
OMR::RV::TreeEvaluator::ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateSinglePrecisionRegister();

   fpBitsMovHelper(node, TR::InstOpCode::_fmv_s_x, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();

   fpBitsMovHelper(node, TR::InstOpCode::_fmv_x_s, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);

   fpBitsMovHelper(node, TR::InstOpCode::_fmv_d_x, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();

   fpBitsMovHelper(node, TR::InstOpCode::_fmv_x_d, trgReg, cg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateSinglePrecisionRegister();
   TR::Register *tmpReg = cg->allocateRegister();
   TR::Register *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);


   union {
      float f;
      int32_t i;
   } fvalue;

   fvalue.f = node->getFloat();
   loadConstant32(cg, node, fvalue.i, tmpReg);
   generateRTYPE(TR::InstOpCode::_fmv_s_x, node, trgReg, tmpReg, zero, cg);
   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);
   TR::Register *tmpReg = cg->allocateRegister();
   TR::Register *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);

   union {
      double d;
      int64_t l;
   } dvalue;

   dvalue.d = node->getDouble();
   loadConstant64(cg, node, dvalue.l, tmpReg);
   generateRTYPE(TR::InstOpCode::_fmv_d_x, node, trgReg, tmpReg, zero, cg);
   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   return trgReg;
   }

// also handles floadi
TR::Register *
OMR::RV::TreeEvaluator::floadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::_flw, cg);
   }

// also handles dloadi
TR::Register *
OMR::RV::TreeEvaluator::dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::_fld, cg);
   }

// also handles fstorei
TR::Register *
OMR::RV::TreeEvaluator::fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::_fsw, cg);
   }

// also handles dstorei
TR::Register *
OMR::RV::TreeEvaluator::dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::_fsd, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getFloatReturnRegister(), TR_FPR, TR_FloatReturn, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
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
   generateRTYPE(op, node, trgReg, src1Reg, src2Reg, cg);
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
OMR::RV::TreeEvaluator::faddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::_fadd_s, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::daddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::_fadd_d, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::_fsub_s, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::_fsub_d, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::_fmul_s, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::_fmul_d, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::_fdiv_s, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::_fdiv_d, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::fremEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::dremEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static TR::Register *
commonFpUnaryEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, bool isDouble, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *trgReg;

   if (node->getReferenceCount() > 1)
      {
      trgReg = isDouble ? cg->allocateRegister(TR_FPR) : cg->allocateSinglePrecisionRegister();
      }
   else
      {
      trgReg = srcReg;
      }
   generateRTYPE(op, node, trgReg, srcReg, srcReg, cg);
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
OMR::RV::TreeEvaluator::fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionUnaryEvaluator(node, TR::InstOpCode::_fsgnjx_s, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionUnaryEvaluator(node, TR::InstOpCode::_fsgnjx_d, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionUnaryEvaluator(node, TR::InstOpCode::_fsgnjn_s, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionUnaryEvaluator(node, TR::InstOpCode::_fsgnjn_d, cg);
   }

static TR::Register*
commonFPtoINTconversionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op,
      TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);
   TR::Register *trgReg = cg->allocateRegister(TR_GPR);

   /*
    * Converting NaN to integral type should produce 0. Since
    * RISC-V fcvt* instructions don't do it, we have to test
    * for NaN before and eventually load 0 into result (target)
    * register.
    */

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *joinLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   joinLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());

   deps->addPostCondition(trgReg, TR::RealRegister::NoReg);
   deps->addPostCondition(src1Reg, TR::RealRegister::NoReg);

   generateLABEL(cg, TR::InstOpCode::label, node, startLabel);

   /*
    * Check if srcReg is NaN, if so then result 0.
    *
    * To do so, we use _feq_d / _feq_s on the same register
    * which for NaN value returns 0. This is very convenient
    * since 0 is the desired result value, so by using target
    * register to hold the result of comparison, we can just
    * jump over the actual conversion via fcvt_?_?.
    */
   TR::InstOpCode::Mnemonic feqOp = firstChild->getDataType().isDouble()
                                       ? TR::InstOpCode::_feq_d
                                       : TR::InstOpCode::_feq_s;
   generateRTYPE(feqOp, node, trgReg, src1Reg, src1Reg, cg);
   generateBTYPE(TR::InstOpCode::_beq, node, joinLabel, trgReg, zero, cg);
   /*
    * Now do the actual conversion
    */
   generateRTYPE(op, node, trgReg, src1Reg, zero, cg);

   generateLABEL(cg, TR::InstOpCode::label, node, joinLabel, deps);

   cg->decReferenceCount(firstChild);
   node->setRegister(trgReg);
   return trgReg;
   }

static TR::Register*
commonINTtoFPconversionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);

   generateRTYPE(op, node, trgReg, src1Reg, zero, cg);

   cg->decReferenceCount(firstChild);
   node->setRegister(trgReg);
   return trgReg;
   }

static TR::Register*
commonFPPtoFPconversionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);

   generateRTYPE(op, node, trgReg, src1Reg, zero, cg);

   cg->decReferenceCount(firstChild);
   node->setRegister(trgReg);
   return trgReg;
   }


TR::Register *
OMR::RV::TreeEvaluator::i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonINTtoFPconversionEvaluator(node, TR::InstOpCode::_fcvt_s_w, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
   return commonINTtoFPconversionEvaluator(node, TR::InstOpCode::_fcvt_d_w, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonINTtoFPconversionEvaluator(node, TR::InstOpCode::_fcvt_s_l, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonINTtoFPconversionEvaluator(node, TR::InstOpCode::_fcvt_d_l, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFPPtoFPconversionEvaluator(node, TR::InstOpCode::_fcvt_d_s, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFPtoINTconversionEvaluator(node, TR::InstOpCode::_fcvt_w_s, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFPtoINTconversionEvaluator(node, TR::InstOpCode::_fcvt_w_d, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:RV: Enable TR::TreeEvaluator::d2cEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
   return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:RV: Enable TR::TreeEvaluator::d2sEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
   return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:RV: Enable TR::TreeEvaluator::d2bEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
   return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFPtoINTconversionEvaluator(node, TR::InstOpCode::_fcvt_l_s, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFPtoINTconversionEvaluator(node, TR::InstOpCode::_fcvt_l_d, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFPPtoFPconversionEvaluator(node, TR::InstOpCode::_fcvt_s_d, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpeqEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpneEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpltEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpgeEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpgtEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpleEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static TR::Register *
compareHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, bool reverse, TR::CodeGenerator *cg)
{
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::Register *trgReg = cg->allocateRegister();

   if (reverse)
      generateRTYPE(op, node, trgReg, src2Reg, src1Reg, cg);
   else
      generateRTYPE(op, node, trgReg, src1Reg, src2Reg, cg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   node->setRegister(trgReg);
   return trgReg;
}

static TR::Register *
compareNotEqualHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::Register *tmpReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegister();

   /*
    * fcmpne / dcmpne should return 0 if either operand is
    * NaN. Since feq.s / feq.d returns 0 in that case, simply
    * comparing for equality (using feq.s / feq.d) and negating
    * the result is not enough.
    *
    * We have to explicitly check for both operands to be non-NaN
    * before negating result of feq.s / feq.d. To check whether a value
    * is NaN, we again use feq.s / feq.d on the same value, this returns
    * 0 only when the value is NaN. The result of fcmpne / dcmpne is then
    * logical and of all three comparisons. This way, we avoid branching.
    */

   generateRTYPE(op,                    node, trgReg, src1Reg, src1Reg, cg);
   generateRTYPE(op,                    node, tmpReg, src2Reg, src2Reg, cg);
   generateRTYPE(TR::InstOpCode::_and,  node, trgReg, trgReg,  tmpReg,  cg);

   generateRTYPE(op,                    node, tmpReg, src1Reg, src2Reg, cg);
   generateITYPE(TR::InstOpCode::_xori, node, tmpReg, tmpReg,  1,       cg);
   generateRTYPE(TR::InstOpCode::_and,  node, trgReg, trgReg,  tmpReg,  cg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(tmpReg);
   node->setRegister(trgReg);
   return trgReg;
   }

static TR::Register *
compareUnorderedHelper(TR::Node *node, TR::InstOpCode::Mnemonic cmpOp, bool reverse, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::Register *trgReg = cg->allocateRegister();

   /*
    * To avoid explicit NaN checks, we perform opposite comparison
    * and flip the result, i.e, when computing "(x >= y) or unordered",
    * actually compute "not (x < y)"
    *
    * The cmpOp passed as parameter is already the opposite comparison.
    */

   if (reverse)
      generateRTYPE(cmpOp, node, trgReg, src2Reg, src1Reg, cg);
   else
      generateRTYPE(cmpOp, node, trgReg, src1Reg, src2Reg, cg);

   generateITYPE(TR::InstOpCode::_xori, node, trgReg, trgReg, 1, cg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   node->setRegister(trgReg);
   return trgReg;
   }

static TR::Register *
compareEqualOrUnorderedHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::Register *trgReg = cg->allocateRegister();

   /*
    * Here we exploit the fact that exactly one of the following conditions
    * must always be true:
    *
    *   1. x < y
    *   2. x > y
    *   3. x == y
    *   4  x and y are unordered
    *
    *   We want to check whether conditions 3 or 4 are true, but we can as well check that
    *   neither 1 nor 2 are true, i.e., compute !(x < y || x > y). This takes only two
    *   comparisons.
    */
   TR::InstOpCode::Mnemonic ltOp = firstChild->getDataType().isDouble()
                                       ? TR::InstOpCode::_flt_d
                                       : TR::InstOpCode::_flt_s;
   TR::Register *tmpReg = cg->allocateRegister();

   generateRTYPE(ltOp,                  node, trgReg, src1Reg, src2Reg, cg);
   generateRTYPE(ltOp,                  node, tmpReg, src2Reg, src1Reg, cg);
   generateRTYPE(TR::InstOpCode::_or,   node, trgReg, trgReg,  tmpReg,  cg);
   generateITYPE(TR::InstOpCode::_xori, node, trgReg, trgReg,  1,       cg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(tmpReg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::fcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_feq_s, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareNotEqualHelper(node, TR::InstOpCode::_feq_s, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_flt_s, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_fle_s, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_flt_s, true, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_fle_s, true, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_feq_d, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareNotEqualHelper(node, TR::InstOpCode::_feq_d, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_flt_d, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_fle_d, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_flt_d, true, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareHelper(node, TR::InstOpCode::_fle_d, true, cg);
   }

// also handles dRegLoad
TR::Register *
OMR::RV::TreeEvaluator::fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
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

static TR::Register *
commonFpMinMaxEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic cmpOp, bool reverse, bool isDouble, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 2, "The number of children for fmax/fmin/dmax/dmin must be 2.");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::Register *cmpReg = cg->allocateRegister();
   TR::RealRegister *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);
   TR::Register *trgReg;

   TR::InstOpCode::Mnemonic feqOp = isDouble ? TR::InstOpCode::_feq_d : TR::InstOpCode::_feq_s;

   if (cg->canClobberNodesRegister(firstChild))
      {
      trgReg = src1Reg; // use the first child as the target
      }
   else
      {
      trgReg = cg->allocateRegister(TR_FPR);
      if (isDouble)
         generateRTYPE(TR::InstOpCode::_fsgnj_d, node, trgReg, src1Reg, src1Reg, cg);
      else
         generateRTYPE(TR::InstOpCode::_fsgnj_s, node, trgReg, src1Reg, src1Reg, cg);
      }

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *moveSrc2RegToTrgReg = generateLabelSymbol(cg);
   TR::LabelSymbol *joinLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   joinLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
   deps->addPostCondition(cmpReg, TR::RealRegister::NoReg);
   deps->addPostCondition(trgReg, TR::RealRegister::NoReg);
   deps->addPostCondition(src1Reg, TR::RealRegister::NoReg);
   deps->addPostCondition(src2Reg, TR::RealRegister::NoReg);

   generateLABEL(cg, TR::InstOpCode::label, node, startLabel);

   /*
    * Check if src1Reg is NaN, if so then result is NaN.
    *
    * Note, that the value is already stored in trgReg, so we check
    * src1Reg (using feq.s / feq.d) and if it's NaN, we just jump
    * to the end (joinLabel)
    */
   generateRTYPE(feqOp, node, cmpReg, src1Reg, src1Reg, cg);
   generateBTYPE(TR::InstOpCode::_beq, node, joinLabel, cmpReg, zero, cg);

   /*
    * Check if src2Reg is NaN, if so then result is NaN.
    *
    * If src2Reg is NaN, we have to move it to trgReg, just like
    * later on when we compare and (eventually) move src2Reg into trgReg.
    * So if src2Reg is NaN, just jump over comparison to that move.
    */
   generateRTYPE(feqOp, node, cmpReg, src2Reg, src2Reg, cg);
   generateBTYPE(TR::InstOpCode::_beq, node, moveSrc2RegToTrgReg, cmpReg, zero, cg);

   /*
    * Finally, compare the two values.
    */
   if (reverse)
      generateRTYPE(cmpOp, node, cmpReg, src2Reg, src1Reg, cg);
   else
      generateRTYPE(cmpOp, node, cmpReg, src1Reg, src2Reg, cg);

   generateBTYPE(TR::InstOpCode::_bne, node, joinLabel, cmpReg, zero, cg);
   generateLABEL(cg, TR::InstOpCode::label, node, moveSrc2RegToTrgReg);
   if (isDouble)
      generateRTYPE(TR::InstOpCode::_fsgnj_d, node, trgReg, src2Reg, src2Reg, cg);
   else
      generateRTYPE(TR::InstOpCode::_fsgnj_s, node, trgReg, src2Reg, src2Reg, cg);

   generateLABEL(cg, TR::InstOpCode::label, node, joinLabel, deps);

   cg->stopUsingRegister(cmpReg);
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFpMinMaxEvaluator(node, TR::InstOpCode::_flt_s, true, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::fminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFpMinMaxEvaluator(node, TR::InstOpCode::_flt_s, false, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFpMinMaxEvaluator(node, TR::InstOpCode::_flt_d, true, true, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFpMinMaxEvaluator(node, TR::InstOpCode::_flt_d, false, true, cg);
   }


TR::Register *
OMR::RV::TreeEvaluator::b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::b2dEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::s2dEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::su2dEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpequEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpltuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpgeuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpgtuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ifdcmpleuEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register*
OMR::RV::TreeEvaluator::fcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareEqualOrUnorderedHelper(node, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::fcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   /*
    * To implement fcmpneu, we just use feq.s and flip the result without
    * further checks, i.e.,
    *
    *   feq.s  trgReg, src1Reg, src2Reg
    *   xori   trgReg, trgReg, trgReg
    *
    * This works since feq.s returns 0 if either operand is NaN.
    *
    * We reuse compareUnorderedHelper() with feq.s to do this since
    * it essentially generates a comparison followed by xori to negate
    * the result.
    */

   return compareUnorderedHelper(node, TR::InstOpCode::_feq_s, false, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::fcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_fle_s, true, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::fcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_flt_s, false, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::fcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_fle_s, false, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::fcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_flt_s, true, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareEqualOrUnorderedHelper(node, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::dcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   /*
    * To implement dcmpneu, we just use feq.d and flip the result without
    * further checks, i.e.,
    *
    *   feq.d  trgReg, src1Reg, src2Reg
    *   xori   trgReg, trgReg, trgReg
    *
    * This works since feq.d returns 0 if either operand is NaN.
    *
    * We reuse compareUnorderedHelper() with feq.d to do this since
    * it essentially generates a comparison followed by xori to negate
    * the result.
    */

   return compareUnorderedHelper(node, TR::InstOpCode::_feq_d, false, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_fle_d, true, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_flt_d, false, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_fle_d, false, cg);
   }

TR::Register*
OMR::RV::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareUnorderedHelper(node, TR::InstOpCode::_flt_d, true, cg);
   }

static TR::Register *
compareThreeWayHelper(TR::Node *node, bool unorderedIsLess, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::Register *tmpReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegister();
   TR::RealRegister *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);

   TR::InstOpCode::Mnemonic ltOp = firstChild->getDataType().isDouble()
                                      ? TR::InstOpCode::_flt_d
                                      : TR::InstOpCode::_flt_s;
   TR::InstOpCode::Mnemonic eqOp = firstChild->getDataType().isDouble()
                                      ? TR::InstOpCode::_feq_d
                                      : TR::InstOpCode::_feq_s;

   /*
    * fcmpl / dcmpl does basically following:
    *
    *     if (src1 > src2)
    *        trg = 1;
    *     else if (src1 == src2)
    *        trg = 0;
    *     else
    *        trg = -1;
    *
    * so we generate:
    *
    *      ...
    *      flt.d   tmp, src2, src1  ; note swapped arguments!
    *      addi    trg, zero, 1
    *    start:                     ; start internal control flow
    *      bnez    tmp, join
    *      feq.d   trg, src1, src2
    *      addi    trg, trg,  -1
    *    join:                      ; end internal control flow
    *      ...
    *
    * Similarly, fcmpg / dcmpg does following:
    *
    *     if (src1 < src2)
    *        trg = -1;
    *     else if (src1 == src2)
    *        trg = 0;
    *     else
    *        trg = 1;
    *
    * so we generate:
    *
    *      ...
    *      flt.d   tmp, src1, src2
    *      addi    trg, zero, -1
    *    start:                     ; start internal control flow
    *      bnez    tmp, join
    *      feq.d   trg, src1, src2
    *      xori    trg, trg,  1
    *    join:                      ; end internal control flow
    *      ...
    *
    */

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *joinLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   joinLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
   deps->addPostCondition(tmpReg, TR::RealRegister::NoReg);
   deps->addPostCondition(trgReg, TR::RealRegister::NoReg);
   deps->addPostCondition(src1Reg, TR::RealRegister::NoReg);
   deps->addPostCondition(src2Reg, TR::RealRegister::NoReg);

   if (unorderedIsLess)
      {
      generateRTYPE(ltOp, node, tmpReg, src2Reg, src1Reg, cg);
      generateITYPE(TR::InstOpCode::_addi, node, trgReg, zero, 1, cg);
      generateLABEL(cg, TR::InstOpCode::label, node, startLabel);
      generateBTYPE(TR::InstOpCode::_bne, node, joinLabel, tmpReg, zero, cg);
      generateRTYPE(eqOp, node, trgReg, src1Reg, src2Reg, cg);
      generateITYPE(TR::InstOpCode::_addi, node, trgReg, trgReg, -1, cg);
      generateLABEL(cg, TR::InstOpCode::label, node, joinLabel, deps);
      }
   else
      {
      generateRTYPE(ltOp, node, tmpReg, src1Reg, src2Reg, cg);
      generateITYPE(TR::InstOpCode::_addi, node, trgReg, zero, -1, cg);
      generateLABEL(cg, TR::InstOpCode::label, node, startLabel);
      generateBTYPE(TR::InstOpCode::_bne, node, joinLabel, tmpReg, zero, cg);
      generateRTYPE(eqOp, node, trgReg, src1Reg, src2Reg, cg);
      generateITYPE(TR::InstOpCode::_xori, node, trgReg, trgReg, 1, cg);
      generateLABEL(cg, TR::InstOpCode::label, node, joinLabel, deps);
      }


   cg->stopUsingRegister(tmpReg);
   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

// also handles fcmpl
TR::Register *
OMR::RV::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareThreeWayHelper(node, true, cg);
   }

// also handles fcmpg
TR::Register *
OMR::RV::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareThreeWayHelper(node, false, cg);
   }

static TR::Register *
commonFpSelectEvaluator(TR::Node *node, bool isDouble, TR::CodeGenerator *cg)
   {
   TR::Node *condNode = node->getChild(0);
   TR::Node *trueNode   = node->getChild(1);
   TR::Node *falseNode  = node->getChild(2);

   TR::Register *condReg = cg->evaluate(condNode);
   TR::Register *trueReg = cg->evaluate(trueNode);
   TR::Register *falseReg = cg->evaluate(falseNode);
   TR::RealRegister *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);

   if (!cg->canClobberNodesRegister(trueNode))
      {
      TR::Register* resultReg = cg->allocateRegister(TR_FPR);
      if (isDouble)
         generateRTYPE(TR::InstOpCode::_fsgnj_d, node, resultReg, trueReg, trueReg, cg);
      else
         generateRTYPE(TR::InstOpCode::_fsgnj_s, node, resultReg, trueReg, trueReg, cg);
      trueReg = resultReg;
      }

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *joinLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   joinLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg->trMemory());
   deps->addPostCondition(condReg, TR::RealRegister::NoReg);
   deps->addPostCondition(trueReg, TR::RealRegister::NoReg);
   deps->addPostCondition(falseReg, TR::RealRegister::NoReg);

   generateLABEL(cg, TR::InstOpCode::label, node, startLabel);
   generateBTYPE(TR::InstOpCode::_bne, node, joinLabel, condReg, zero, cg);
   if (isDouble)
      generateRTYPE(TR::InstOpCode::_fsgnj_d, node, trueReg, falseReg, falseReg, cg);
   else
      generateRTYPE(TR::InstOpCode::_fsgnj_s, node, trueReg, falseReg, falseReg, cg);
   generateLABEL(cg, TR::InstOpCode::label, node, joinLabel, deps);

   node->setRegister(trueReg);
   cg->decReferenceCount(condNode);
   cg->decReferenceCount(trueNode);
   cg->decReferenceCount(falseNode);

   return trueReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::fselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFpSelectEvaluator(node, false, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::dselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonFpSelectEvaluator(node, true, cg);
   }
