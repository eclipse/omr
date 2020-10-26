/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "codegen/RVInstruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/LabelSymbol.hpp"

TR::Register *
genericReturnEvaluator(TR::Node *node, TR::RealRegister::RegNum rnum, TR_RegisterKinds rk, TR_ReturnInfo i,  TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *returnRegister = cg->evaluate(firstChild);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 0, cg->trMemory());
   deps->addPreCondition(returnRegister, rnum);
   generateADMIN(cg, TR::InstOpCode::retn, node, deps);

   cg->comp()->setReturnInfo(i);
   cg->decReferenceCount(firstChild);

   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getIntegerReturnRegister(), TR_GPR, TR_IntReturn, cg);
   }

// also handles areturn
TR::Register *
OMR::RV::TreeEvaluator::lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getLongReturnRegister(), TR_GPR, TR_LongReturn, cg);
   }

// void return
TR::Register *
OMR::RV::TreeEvaluator::returnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateADMIN(cg, TR::InstOpCode::retn, node);
   cg->comp()->setReturnInfo(TR_VoidReturn);
   return NULL;
   }

TR::Register *OMR::RV::TreeEvaluator::gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::LabelSymbol *gotoLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::RealRegister *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);
   if (node->getNumChildren() > 0)
      {
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      generateJTYPE(TR::InstOpCode::_jal, node, zero, gotoLabel,
            generateRegisterDependencyConditions(cg, child, 0), cg);
      cg->decReferenceCount(child);
      }
   else
      {
      generateJTYPE(TR::InstOpCode::_jal, node, zero, gotoLabel, cg);
      }
   return NULL;
   }

static TR::Instruction *ificmpHelper(TR::InstOpCode::Mnemonic op, TR::Node *node, bool reverse, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = NULL;

   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::LabelSymbol *dstLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::Instruction *result;
   if (node->getNumChildren() == 3)
      {
      TR_UNIMPLEMENTED();
#if 0
      thirdChild = node->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare must be a TR::GlRegDeps");

      cg->evaluate(thirdChild);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc, deps);
#endif
      }
   else
      {
      if (reverse)
         result = generateBTYPE(op, node, dstLabel, src2Reg, src1Reg, cg);
      else
         result = generateBTYPE(op, node, dstLabel, src1Reg, src2Reg, cg);
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
OMR::RV::TreeEvaluator::ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_beq, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bne, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_blt, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bge, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_blt, node, true, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bge, node, true, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bltu, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bgeu, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bltu, node, true, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bgeu, node, true, cg);
   return NULL;
   }

// also handles ifacmpeq
TR::Register *
OMR::RV::TreeEvaluator::iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_beq, node, false, cg);
   return NULL;
   }

// also handles ifacmpne
TR::Register *
OMR::RV::TreeEvaluator::iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bne, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_blt, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bge, node, false, cg);
   return NULL;
   }

// also handles ifacmplt
TR::Register *
OMR::RV::TreeEvaluator::iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bltu, node, false, cg);
   return NULL;
   }

// also handles ifacmpge
TR::Register *
OMR::RV::TreeEvaluator::iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bgeu, node, false, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_blt, node, true, cg);
   return NULL;
   }

TR::Register *
OMR::RV::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bge, node, true, cg);
   return NULL;
   }

// also handles ifacmpgt
TR::Register *
OMR::RV::TreeEvaluator::iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bltu, node, true, cg);
   return NULL;
   }

// also handles ifacmple
TR::Register *
OMR::RV::TreeEvaluator::iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(TR::InstOpCode::_bgeu, node, true, cg);
   return NULL;
   }

static TR::Register *icmpHelper(TR::InstOpCode::Mnemonic op1, TR::InstOpCode::Mnemonic op2, uint32_t imm2, TR::Node *node, bool reverse, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Instruction *result = nullptr;

   if (reverse)
      result = generateRTYPE(op1, node, trgReg, src2Reg, src1Reg, cg);
   else
      result = generateRTYPE(op1, node, trgReg, src1Reg, src2Reg, cg);

   if (op2 != TR::InstOpCode::bad)
      {
      result = generateITYPE(op2, node, trgReg, trgReg, imm2, cg, result);
      }

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

// also handles bcmpeq and scmpeq
TR::Register *
OMR::RV::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_sub, TR::InstOpCode::_sltiu, 1, node, false, cg);
   }

// also handles bcmpne and scmpne
TR::Register *
OMR::RV::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   /*
    * We cannot use icmpHelper() because that one generates
    * RTYPE followed by ITYPE instruction. Here we need to
    * generate
    *
    *     sub result, left, right
    *     sltu result, zero, result
    *
    * i.e., RTYPE followed by RTYPE
    */
   TR::Register *trgReg = cg->allocateRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);
   TR::Instruction *result = nullptr;

   result = generateRTYPE(TR::InstOpCode::_sub, node, trgReg, src1Reg, src2Reg, cg);
   result = generateRTYPE(TR::InstOpCode::_sltu, node, trgReg, zero, trgReg, cg);


   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;

   }

// also handles bcmplt and scmplt
TR::Register *
OMR::RV::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_slt, TR::InstOpCode::bad, 0, node, false, cg);
   }

// also handles bcmple and scmple
TR::Register *
OMR::RV::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_slt, TR::InstOpCode::_sltiu, 1, node, true, cg);
   }

// also handles bcmpge and scmpge
TR::Register *
OMR::RV::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_slt, TR::InstOpCode::_xori, 1, node, false, cg);
   }

// also handles bcmpgt and scmpgt
TR::Register *
OMR::RV::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_slt, TR::InstOpCode::bad, 0, node, true, cg);
   }

// also handles bucmplt and sucmplt
TR::Register *
OMR::RV::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_sltu, TR::InstOpCode::bad, 0, node, false, cg);
   }

// also handles bucmple and sucmple
TR::Register *
OMR::RV::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_sltu, TR::InstOpCode::_xori, 1, node, true, cg);
   }

// also handles bucmpge and sucmpge
TR::Register *
OMR::RV::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_sltu, TR::InstOpCode::_xori, 1, node, false, cg);
   }

// also handles bucmpgt and sucmpgt
TR::Register *
OMR::RV::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(TR::InstOpCode::_sltu, TR::InstOpCode::bad, 0, node, true, cg);
   }

// also handles acmpeq
TR::Register *
OMR::RV::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return icmpeqEvaluator(node, cg);
   }

// also handles acmpne
TR::Register *
OMR::RV::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return icmpneEvaluator(node, cg);
   }

// also handles acmplt
TR::Register *
OMR::RV::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return icmpltEvaluator(node, cg);
   }

// also handles acmpge
TR::Register *
OMR::RV::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return icmpgeEvaluator(node, cg);
   }

// also handles acmpgt
TR::Register *
OMR::RV::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return icmpgtEvaluator(node, cg);
   }

// also handles acmple
TR::Register *
OMR::RV::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return icmpleEvaluator(node, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return iucmpltEvaluator(node, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return iucmpgeEvaluator(node, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return iucmpgtEvaluator(node, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is64Bit(), "RV32 not yet supported");
   return iucmpleEvaluator(node, cg);
   }

TR::Register *
OMR::RV::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *tmpReg = cg->allocateRegister();

   /* Here we set up registers such that
    *
    *  - trgReg = 1  iff  src1Reg < src2Reg
    *  - tmpReg = 1  iff  src1Reg > src2Reg
    *
    */
   generateRTYPE(TR::InstOpCode::_slt, node, trgReg, src1Reg, src2Reg, cg);
   generateRTYPE(TR::InstOpCode::_slt, node, tmpReg, src2Reg, src1Reg, cg);

   /*
    * There are three outcomes possible:
    *
    *   (i) trgReg = 0, tmpReg = 1  => lcmp =>  1
    *  (ii) trgReg = 1, tmpReg = 0  => lcmp => -1
    * (iii) trgReg = 0, tmpReg = 0  => lcmp =>  0
    */
   generateRTYPE(TR::InstOpCode::_sub, node, trgReg, tmpReg, trgReg, cg);

   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *
OMR::RV::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::acmpeqEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

// also handles lselect, aselect, bselect, sselect
TR::Register *
OMR::RV::TreeEvaluator::iselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *condNode = node->getChild(0);
   TR::Node *trueNode   = node->getChild(1);
   TR::Node *falseNode  = node->getChild(2);

   TR::Register *condReg = cg->evaluate(condNode);
   TR::Register *trueReg = cg->gprClobberEvaluate(trueNode);
   TR::Register *falseReg = cg->evaluate(falseNode);
   TR::RealRegister *zero = cg->machine()->getRealRegister(TR::RealRegister::zero);

   // Internal pointers cannot be handled since we cannot set the pinning array
   // on the result register without knowing which side of the select will be
   // taken.
   TR_ASSERT_FATAL_WITH_NODE(
      node,
      !trueReg->containsInternalPointer() && !falseReg->containsInternalPointer(),
      "Select nodes cannot have children that are internal pointers"
   );
   if (falseReg->containsCollectedReference())
      {
      if (cg->comp()->getOption(TR_TraceCG))
         traceMsg(
            cg->comp(),
            "Setting containsCollectedReference on result of select node in register %s\n",
            cg->getDebug()->getName(trueReg));
      trueReg->setContainsCollectedReference();
      }

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *joinLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   joinLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3,cg->trMemory());
   deps->addPostCondition(condReg, TR::RealRegister::NoReg);
   deps->addPostCondition(trueReg, TR::RealRegister::NoReg);
   deps->addPostCondition(falseReg, TR::RealRegister::NoReg);

   generateLABEL(cg, TR::InstOpCode::label, node, startLabel);
   generateBTYPE(TR::InstOpCode::_bne, node, joinLabel, condReg, zero, cg);
   generateITYPE(TR::InstOpCode::_addi, node, trueReg, falseReg, 0, cg);
   generateLABEL(cg, TR::InstOpCode::label, node, joinLabel, deps);

   node->setRegister(trueReg);
   cg->decReferenceCount(condNode);
   cg->decReferenceCount(trueNode);
   cg->decReferenceCount(falseNode);

   return trueReg;
   }


TR::Register *
OMR::RV::TreeEvaluator::lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::lookupEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::tableEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::tableEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::NULLCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ZEROCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ResolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ResolveAndNULLCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::DIVCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::BNDCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ArrayStoreCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::RV::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:RV: Enable TR::TreeEvaluator::ArrayCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::RV::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static TR::Register *
commonMinMaxEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 2, "The number of children for imax/imin/lmax/lmin must be 2.");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->gprClobberEvaluate(firstChild);
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *joinLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   joinLabel->setEndInternalControlFlow();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());
   deps->addPostCondition(src1Reg, TR::RealRegister::NoReg);
   deps->addPostCondition(src2Reg, TR::RealRegister::NoReg);

   generateLABEL(cg, TR::InstOpCode::label, node, startLabel);
   generateBTYPE(op, node, joinLabel, src1Reg, src2Reg, cg);
   generateITYPE(TR::InstOpCode::_addi, node, src1Reg, src2Reg, 0, cg);
   generateLABEL(cg, TR::InstOpCode::label, node, joinLabel, deps);

   node->setRegister(src1Reg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return src1Reg;
   }

// Also handles lmax
TR::Register *
OMR::RV::TreeEvaluator::imaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, TR::InstOpCode::_bge, cg);
   }

// Also handles lumax
TR::Register *
OMR::RV::TreeEvaluator::iumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, TR::InstOpCode::_bgeu, cg);
   }


// Also handles lmin
TR::Register *
OMR::RV::TreeEvaluator::iminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, TR::InstOpCode::_blt, cg);
   }

// Also handles lumin
TR::Register *
OMR::RV::TreeEvaluator::iuminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, TR::InstOpCode::_bltu, cg);
   }

