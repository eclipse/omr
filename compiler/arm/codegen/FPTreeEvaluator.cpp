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

#include "arm/codegen/ARMInstruction.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
// Helpers
static bool noFPRA = false; // The current RA *does* seem to assign FP regs.  Shall we change this to false?

static TR::Register *moveToFloatRegister(TR::Node *node, TR::Register *srcReg, TR::CodeGenerator *cg)
   {
   // Move from regular registers to single floating point register
   if (srcReg->getKind() == TR_FPR)
      {
      TR_ASSERT(srcReg->isSinglePrecision(), "Expecting a single precision register\n");
      return srcReg;
      }
   else // GPR
      {
      TR::Register *trgReg = cg->allocateSinglePrecisionRegister();
      generateTrg1Src1Instruction(cg, ARMOp_fmsr, node, trgReg, srcReg);
      return trgReg;
      }
   }

static TR::Register *moveToDoubleRegister(TR::Node *node, TR::Register *srcReg, TR::CodeGenerator *cg)
   {
   if (srcReg->getKind() == TR_FPR)
      {
      TR_ASSERT(!srcReg->isSinglePrecision(), "Expecting a double precision register\n");
      return srcReg;
      }
   else // GPR
      {
      TR::Register *trgReg = cg->allocateRegister(TR_FPR);
      TR::RegisterPair *pair = srcReg->getRegisterPair();
      generateTrg1Src2Instruction(cg, ARMOp_fmdrr, node, trgReg, pair->getLowOrder(), pair->getHighOrder());
      return trgReg;
      }
   }

static TR::Register *moveFromFloatRegister(TR::Node *node, TR::Register *srcReg, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   generateTrg1Src1Instruction(cg, ARMOp_fmrs, node, trgReg, srcReg);
   return trgReg;
   }

static TR::Register *moveFromDoubleRegister(TR::Node *node, TR::Register *srcReg, TR::CodeGenerator *cg)
   {
   TR::Register *lowReg  = cg->allocateRegister();
   TR::Register *highReg = cg->allocateRegister();
   TR::RegisterPair *trgReg = cg->allocateRegisterPair(lowReg, highReg);
   generateTrg2Src1Instruction(cg, ARMOp_fmrrd, node, lowReg, highReg, srcReg);
   return trgReg;
   }

static bool resultCanStayInFloatRegister(TR::Node *checkNode, TR::Node *node)
   {
   bool result = true;
   TR::Node *child= NULL;
   static bool disableVFPOpt = (feGetEnv("TR_DisableVFPOpt") != NULL);
   if (disableVFPOpt)
      return false;

   /* TODO: Memo - Below may hurt GRA when regassign for FP/Double is enabled in future. */
   if (node->getReferenceCount() > 1)
      result = false;
   for (uint16_t i = 0; i < checkNode->getNumChildren() && result; i++)
      {
      child = checkNode->getChild(i);
      if (child->getOpCode().isCall())
         result = false;
      else if (checkNode->getOpCodeValue() == TR::fstore && node->getOpCodeValue() == TR::fload)
         result = false;
      /*
      else if (checkNode->getOpCodeValue() == TR::dstore && node->getOpCodeValue() == TR::dload)
         result = false;
         */
      else
         result = resultCanStayInFloatRegister(child, node);
      if (!result || child == node)
         break;
      }
   return result;
   }

static TR::Register *singlePrecisionEvaluator(TR::Node *node, TR_ARMOpCodes opCode, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = NULL;
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = NULL;
   TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();
   TR::Register *trgReg = NULL;

   TR_ASSERT(node->getNumChildren() <= 2, "Not expecting more than 2 children in this node\n");
   if (node->getNumChildren() == 2)
      {
      secondChild = node->getSecondChild();
      src2Reg = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, opCode, node, floatTrgReg, moveToFloatRegister(node, src1Reg, cg), moveToFloatRegister(node, src2Reg, cg));
      }
   else // 1 child
      {
      generateTrg1Src1Instruction(cg, opCode, node, floatTrgReg, moveToFloatRegister(node, src1Reg, cg));
      }

   // Test with no RA, put values back in general registers, conforming to ABI
   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         trgReg = floatTrgReg;
         }
      else
         {
         trgReg = moveFromFloatRegister(node, floatTrgReg, cg);
         cg->stopUsingRegister(floatTrgReg);
         }
      }
   else
      trgReg = floatTrgReg;

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   if (node->getNumChildren() == 2)
      cg->decReferenceCount(secondChild);
   return trgReg;
   }

static TR::Register *doublePrecisionEvaluator(TR::Node *node, TR_ARMOpCodes opCode, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = NULL;
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = NULL;
   TR::Register *doubleTrgReg = cg->allocateRegister(TR_FPR);
   TR::Register *trgReg = NULL;

   TR_ASSERT(node->getNumChildren() <= 2, "Not expecting more than 2 children in this node\n");
   //TR_ASSERT(src1Reg->getRegisterPair(), "Expecting double value in a register pair\n");
   if (node->getNumChildren() == 2)
      {
      secondChild = node->getSecondChild();
      src2Reg = cg->evaluate(secondChild);
      //TR_ASSERT(src2Reg->getRegisterPair(), "Expecting double value in a register pair\n");
      generateTrg1Src2Instruction(cg, opCode, node, doubleTrgReg, moveToDoubleRegister(node, src1Reg, cg), moveToDoubleRegister(node, src2Reg, cg));
      }
   else // 1 child
      {
      generateTrg1Src1Instruction(cg, opCode, node, doubleTrgReg, moveToDoubleRegister(node, src1Reg, cg));
      }

   // Test with no RA, put values back in general registers, conforming to ABI
   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         trgReg = doubleTrgReg;
         }
      else
         {
         trgReg = moveFromDoubleRegister(node, doubleTrgReg, cg);
         cg->stopUsingRegister(doubleTrgReg);
         }
      }
   else
      trgReg = doubleTrgReg;

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   if (node->getNumChildren() == 2)
      cg->decReferenceCount(secondChild);
   return trgReg;
   }

static TR::Register *callLong2DoubleHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());
   int i;

   TR::Register *highReg = cg->allocateRegister();
   TR::Register *lowReg = cg->allocateRegister();
#if defined(__ARM_PCS_VFP)
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);
#else
   TR::Register *trgReg = cg->allocateRegisterPair(lowReg, highReg);
#endif
   TR::Register *doubleTrgReg = NULL;

   if (TR::Compiler->target.cpu.isLittleEndian())
      {
      /* Little Endian */
      dependencies->addPreCondition(srcReg->getHighOrder(), TR::RealRegister::gr1);
      dependencies->addPreCondition(srcReg->getLowOrder(), TR::RealRegister::gr0);
#if defined(__ARM_PCS_VFP)
      dependencies->addPostCondition(trgReg, TR::RealRegister::fp0);
      for (i = 1; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateRegister(TR_FPR), realReg);//TODO live register
         }
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
#else
      dependencies->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::gr1);
      dependencies->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::gr0);
#endif
      }
   else
      {
      /* Big Endian */
      dependencies->addPreCondition(srcReg->getHighOrder(), TR::RealRegister::gr0);
      dependencies->addPreCondition(srcReg->getLowOrder(), TR::RealRegister::gr1);
#if defined(__ARM_PCS_VFP)
      dependencies->addPostCondition(trgReg, TR::RealRegister::fp0);
      for (i = 1; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateRegister(TR_FPR), realReg);//TODO live register
         }
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
#else
      dependencies->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::gr0);
      dependencies->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::gr1);
#endif

      }
   dependencies->stopAddingConditions();

   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlong2Double, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlong2Double, false, false, false));

   if (noFPRA)
      {
#if defined(__ARM_PCS_VFP)
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         doubleTrgReg = trgReg;
         }
      else
         {
         doubleTrgReg = moveFromDoubleRegister(node, trgReg, cg);
         cg->stopUsingRegister(trgReg);
         }
#else
      doubleTrgReg = trgReg;
#endif
      }
   else
      doubleTrgReg = trgReg;

   cg->decReferenceCount(child);
   cg->machine()->setLinkRegisterKilled(true);

   return doubleTrgReg;
   }

static TR::Register *callLong2FloatHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());
   int i;

#if defined(__ARM_PCS_VFP)
   TR::Register *trgReg = cg->allocateSinglePrecisionRegister();
#else
   TR::Register *trgReg = cg->allocateRegister(); // Callout follows soft-float abi
#endif
   TR::Register *floatTrgReg = NULL;

   if (TR::Compiler->target.cpu.isLittleEndian())
      {
      /* Little Endian */
      dependencies->addPreCondition(srcReg->getHighOrder(), TR::RealRegister::gr1);
      dependencies->addPreCondition(srcReg->getLowOrder(), TR::RealRegister::gr0);

#if defined(__ARM_PCS_VFP)
      dependencies->addPostCondition(trgReg, TR::RealRegister::fp0);
      for (i = 1; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateSinglePrecisionRegister(), realReg);//TODO live register
         }
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
#else
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
      dependencies->addPostCondition(trgReg, TR::RealRegister::gr0);
#endif
      }
   else
      {
      /* Big Endian */
      dependencies->addPreCondition(srcReg->getHighOrder(), TR::RealRegister::gr0);
      dependencies->addPreCondition(srcReg->getLowOrder(), TR::RealRegister::gr1);

#if defined(__ARM_PCS_VFP)
      dependencies->addPostCondition(trgReg, TR::RealRegister::fp0);
      for (i = 1; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateSinglePrecisionRegister(), realReg);//TODO live register
         }
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
#else
      dependencies->addPostCondition(trgReg, TR::RealRegister::gr0);
      dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
#endif
      }
   dependencies->stopAddingConditions();

   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlong2Float, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMlong2Float, false, false, false));

   if (noFPRA)
      {
#if defined(__ARM_PCS_VFP)
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         floatTrgReg = trgReg;
         }
      else
         {
         floatTrgReg = moveFromFloatRegister(node, trgReg, cg);
         cg->stopUsingRegister(trgReg);
         }
#else
      floatTrgReg = trgReg;
#endif
      }
   else
      floatTrgReg = trgReg;

   cg->decReferenceCount(child);
   cg->machine()->setLinkRegisterKilled(true);

   return floatTrgReg;
   }

static TR::Register *callDouble2LongHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *doubleSrcReg = cg->evaluate(child);
   TR::Register *srcReg = NULL;
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(11, 11, cg->trMemory());
   int i;
   TR::Register *highReg = cg->allocateRegister();
   TR::Register *lowReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegisterPair(lowReg, highReg);


#if defined(__ARM_PCS_VFP)   // hard fp
   if (doubleSrcReg->getKind() == TR_GPR)
      {
      srcReg = moveToDoubleRegister(node, doubleSrcReg, cg);
      }
   else if (doubleSrcReg->getKind() == TR_FPR)
      {
      srcReg = doubleSrcReg;
      }
   else
      TR_ASSERT(0, "Unknown register type\n");
#else // soft fp
   if (doubleSrcReg->getKind() == TR_GPR)
      {
      srcReg = doubleSrcReg;
      }
   else if (doubleSrcReg->getKind() == TR_FPR)
      {
      srcReg = moveFromDoubleRegister(node, doubleSrcReg, cg);
      }
   else
      TR_ASSERT(0, "Unknown register type\n");
#endif

   if (TR::Compiler->target.cpu.isLittleEndian())
      {
      /* Little Endian */

#if defined(__ARM_PCS_VFP)
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register

      dependencies->addPreCondition(srcReg, TR::RealRegister::fp0);
      for (i = 0; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateRegister(TR_FPR), realReg);//TODO live register
         }
#else
      dependencies->addPreCondition(srcReg->getHighOrder(), TR::RealRegister::gr1);
      dependencies->addPreCondition(srcReg->getLowOrder(), TR::RealRegister::gr0);
#endif
      dependencies->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::gr0);
      dependencies->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::gr1);
      }
   else
      {
      /* Big Endian */
#if defined(__ARM_PCS_VFP)
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register

      dependencies->addPreCondition(srcReg, TR::RealRegister::fp0);
      for (i = 0; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateRegister(TR_FPR), realReg);//TODO live register
         }
#else
      dependencies->addPreCondition(srcReg->getHighOrder(), TR::RealRegister::gr0);
      dependencies->addPreCondition(srcReg->getLowOrder(), TR::RealRegister::gr1);
#endif
      dependencies->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::gr0);
      dependencies->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::gr1);
      }
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr2, TR_GPR, cg); // Block r2
   dependencies->stopAddingConditions();

   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMdouble2Long, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMdouble2Long, false, false, false));

   //dependencies->stopUsingDepRegs(cg, trgReg);

   cg->decReferenceCount(child);
   cg->machine()->setLinkRegisterKilled(true);

   return trgReg;
   }

static TR::Register *callFloat2LongHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *floatSrcReg = cg->evaluate(child);
   TR::Register *srcReg = NULL;
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(11, 11, cg->trMemory());
   int i;
   TR::Register *highReg = cg->allocateRegister();
   TR::Register *lowReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegisterPair(lowReg, highReg);

#if defined(__ARM_PCS_VFP)
   if (floatSrcReg->getKind() == TR_GPR)
      {
      srcReg = moveToFloatRegister(node, floatSrcReg, cg);
      }
   else if (floatSrcReg->getKind() == TR_FPR)
      {
      srcReg = floatSrcReg;
      }
   else
      TR_ASSERT(0, "Unknown register type\n");
#else //soft fp
   if (floatSrcReg->getKind() == TR_GPR)
      {
      srcReg = floatSrcReg;
      }
   else if (floatSrcReg->getKind() == TR_FPR)
      {
      srcReg = moveFromFloatRegister(node, floatSrcReg, cg);
      }
   else
      TR_ASSERT(0, "Unknown register type\n");
#endif

   if (TR::Compiler->target.cpu.isLittleEndian())
      {
      /* Little Endian */
#if defined(__ARM_PCS_VFP)
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
      dependencies->addPreCondition(srcReg, TR::RealRegister::fp0);
      for (i = 0; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateSinglePrecisionRegister(), realReg);//TODO live register
         }
#else
      dependencies->addPreCondition(srcReg, TR::RealRegister::gr0);
#endif
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
      dependencies->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::gr1);
      dependencies->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::gr0);
      }
   else
      {
      /* Big Endian */
#if defined(__ARM_PCS_VFP)
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr0);//TODO live register
      dependencies->addPreCondition(srcReg, TR::RealRegister::fp0);
      for (i = 0; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         dependencies->addPostCondition(cg->allocateSinglePrecisionRegister(), realReg);//TODO live register
         }
#else
      dependencies->addPreCondition(srcReg, TR::RealRegister::gr0);
#endif
      dependencies->addPreCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
      dependencies->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::gr0);
      dependencies->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::gr1);
      }
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr2, TR_GPR, cg); // Block r2
   dependencies->stopAddingConditions();

   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMfloat2Long, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMfloat2Long, false, false, false));

   //dependencies->stopUsingDepRegs(cg, trgReg);
   //cg->stopUsingRegister(dummyReg);
   cg->decReferenceCount(child);
   cg->machine()->setLinkRegisterKilled(true);

   return trgReg;
   }

static TR::Register *callDoubleRemainderHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());
   int i;
   TR::Register *highReg = cg->allocateRegister();
   TR::Register *lowReg = cg->allocateRegister();
#if defined(__ARM_PCS_VFP)
   TR::Register *trgReg = cg->allocateRegister(TR_FPR);
#else
   TR::Register *trgReg = cg->allocateRegisterPair(lowReg, highReg);
#endif
   TR::Register *doubleTrgReg = NULL;

#if defined(__ARM_PCS_VFP)
   if (src1Reg->getKind() == TR_GPR)
      {
      src1Reg = moveToDoubleRegister(node, src1Reg, cg);
      }
   if (src2Reg->getKind() == TR_GPR)
      {
      src2Reg = moveToDoubleRegister(node, src2Reg, cg);
      }
#else
   if (src1Reg->getKind() == TR_FPR)
      {
      src1Reg = moveFromDoubleRegister(node, src1Reg, cg);
      }
   if (src2Reg->getKind() == TR_FPR)
      {
      src2Reg = moveFromDoubleRegister(node, src2Reg, cg);
      }
#endif
   if(!noFPRA)
      {
      if((src1Reg->getKind() == TR_FPR) && !cg->canClobberNodesRegister(firstChild, 0))
         {
         TR::Register *tempReg = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, ARMOp_fcpyd, firstChild, tempReg, src1Reg);
         src1Reg = tempReg;
         }
      if((src2Reg->getKind() == TR_FPR) && !cg->canClobberNodesRegister(secondChild, 0))
         {
         TR::Register *tempReg = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, ARMOp_fcpyd, secondChild, tempReg, src2Reg);
         src2Reg = tempReg;
         }
      }

   if (TR::Compiler->target.cpu.isLittleEndian())
      {
      /* Little Endian */
#if defined(__ARM_PCS_VFP)
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr1, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);

      dependencies->addPreCondition(src1Reg, TR::RealRegister::fp0);
      dependencies->addPreCondition(src2Reg, TR::RealRegister::fp1);

      for (i = 2; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         TR::addDependency(dependencies, NULL, realReg, TR_FPR, cg);
         }
      dependencies->addPostCondition(trgReg, TR::RealRegister::fp0);
      dependencies->addPostCondition(cg->allocateRegister(TR_FPR), TR::RealRegister::fp1);//TODO live register
#else
      dependencies->addPreCondition(src1Reg->getHighOrder(), TR::RealRegister::gr1);
      dependencies->addPreCondition(src1Reg->getLowOrder(), TR::RealRegister::gr0);

      TR::addDependency(dependencies, src2Reg->getLowOrder(), TR::RealRegister::gr2, TR_GPR, cg);
      TR::addDependency(dependencies, src2Reg->getHighOrder(), TR::RealRegister::gr3, TR_GPR, cg);

      dependencies->addPostCondition(highReg, TR::RealRegister::gr1);
      dependencies->addPostCondition(lowReg, TR::RealRegister::gr0);
#endif
      }
   else
      {
      /* Big Endian */
#if defined(__ARM_PCS_VFP)
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr1, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);

      dependencies->addPreCondition(src1Reg, TR::RealRegister::fp0);
      dependencies->addPreCondition(src2Reg, TR::RealRegister::fp1);

      for (i = 2; i < 8; i++)
         {
         TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
         TR::addDependency(dependencies, NULL, realReg, TR_FPR, cg);
         }
      dependencies->addPostCondition(trgReg, TR::RealRegister::fp0);
      dependencies->addPostCondition(cg->allocateRegister(TR_FPR), TR::RealRegister::fp1);//TODO live register
#else
      dependencies->addPreCondition(src1Reg->getHighOrder(), TR::RealRegister::gr0);
      dependencies->addPreCondition(src1Reg->getLowOrder(), TR::RealRegister::gr1);

      TR::addDependency(dependencies, src2Reg->getHighOrder(), TR::RealRegister::gr2, TR_GPR, cg);
      TR::addDependency(dependencies, src2Reg->getLowOrder(), TR::RealRegister::gr3, TR_GPR, cg);

      dependencies->addPostCondition(highReg, TR::RealRegister::gr0);
      dependencies->addPostCondition(lowReg, TR::RealRegister::gr1);
#endif
      }
   dependencies->stopAddingConditions();
   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMdoubleRemainder, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMdoubleRemainder, false, false, false));

   if (noFPRA)
      {
#if defined(__ARM_PCS_VFP)
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         doubleTrgReg = trgReg;
         }
      else
         {
         doubleTrgReg = moveFromDoubleRegister(node, trgReg, cg);
         cg->stopUsingRegister(trgReg);
         }
#else
      doubleTrgReg = trgReg;
#endif
      }
   else
      doubleTrgReg = trgReg;

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->machine()->setLinkRegisterKilled(true);

   return doubleTrgReg;
   }

static TR::Register *callFloatRemainderHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(11, 11, cg->trMemory());
   int i;
#if defined(__ARM_PCS_VFP)
   TR::Register *trgReg = cg->allocateSinglePrecisionRegister();
#else
   TR::Register *trgReg = cg->allocateRegister(TR_GPR);
#endif
   TR::Register *floatTrgReg = NULL;

#if defined(__ARM_PCS_VFP)
   if (src1Reg->getKind() == TR_GPR)
      {
      src1Reg = moveToFloatRegister(node, src1Reg, cg);
      }
   if (src2Reg->getKind() == TR_GPR)
      {
      src2Reg = moveToFloatRegister(node, src2Reg, cg);
      }
#else
   if (src1Reg->getKind() == TR_FPR)
      {
      src1Reg = moveFromFloatRegister(node, src1Reg, cg);
      }
   if (src2Reg->getKind() == TR_FPR)
      {
      src2Reg = moveFromFloatRegister(node, src2Reg, cg);
      }
#endif

#if defined(__ARM_PCS_VFP)
   if(!noFPRA)
      {
      if((src1Reg->getKind() == TR_FPR) && !cg->canClobberNodesRegister(firstChild, 0))
         {
         TR::Register *tempReg = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, ARMOp_fcpys, firstChild, tempReg, src1Reg);
         src1Reg = tempReg;
         }
      if((src2Reg->getKind() == TR_FPR) && !cg->canClobberNodesRegister(secondChild, 0))
         {
         TR::Register *tempReg = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, ARMOp_fcpys, secondChild, tempReg, src2Reg);
         src2Reg = tempReg;
         }
      }

   TR::addDependency(dependencies, NULL, TR::RealRegister::gr1, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
   dependencies->addPreCondition(src1Reg, TR::RealRegister::fp0);
   dependencies->addPreCondition(src2Reg, TR::RealRegister::fs1);
   for (i = 1; i < 8; i++)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0 + i);
      TR::addDependency(dependencies, NULL, realReg, TR_FPR, cg);
      }
   dependencies->addPostCondition(trgReg, TR::RealRegister::fp0);
#else
   dependencies->addPreCondition(src1Reg, TR::RealRegister::gr0);
   dependencies->addPreCondition(src2Reg, TR::RealRegister::gr1);

   dependencies->addPostCondition(trgReg, TR::RealRegister::gr0);
   dependencies->addPostCondition(cg->allocateRegister(), TR::RealRegister::gr1);//TODO live register
#endif
   dependencies->stopAddingConditions();

   generateImmSymInstruction(cg, ARMOp_bl,
                             node, (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMfloatRemainder, false, false, false)->getMethodAddress(),
                             dependencies,
                             cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMfloatRemainder, false, false, false));

   if (noFPRA)
      {
#if defined(__ARM_PCS_VFP)
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         floatTrgReg = trgReg;
         }
      else
         {
         floatTrgReg = moveFromFloatRegister(node, trgReg, cg);
         cg->stopUsingRegister(trgReg);
         }
#else
      floatTrgReg = trgReg;
#endif
      }
   else
      floatTrgReg = trgReg;

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->machine()->setLinkRegisterKilled(true);

   return floatTrgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = NULL;

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      if (noFPRA)
      	 {

         if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
            traceMsg(cg->comp(), "Result of node %p can stay in FP reg (not exec.).\n", node);

         target = cg->allocateRegister();
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, target, tempMR);
         }
      else
      	 {
      	 target = cg->allocateSinglePrecisionRegister();
         if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
            {
            TR::Register *tempReg = cg->allocateRegister();
            generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
            tempMR->decNodeReferenceCounts();

            tempMR = new (cg->trHeapMemory()) TR::MemoryReference(tempReg, 0, cg);
            generateTrg1MemInstruction(cg, ARMOp_flds, node, target, tempMR);
            cg->stopUsingRegister(tempReg);
            }
         else
            generateTrg1MemInstruction(cg, ARMOp_flds, node, target, tempMR);
      	 }
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      TR::Register *srcReg = cg->evaluate(child);
      if (noFPRA)
         {
         target = srcReg;
         }
      else
         {
         target = moveToFloatRegister(node, srcReg, cg);
         cg->stopUsingRegister(srcReg);
         }

      cg->decReferenceCount(child);
      }
   node->setRegister(target);
   return target;
   }

TR::Register *OMR::ARM::TreeEvaluator::fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = cg->allocateRegister();

   TR_ASSERT(!node->normalizeNanValues(), "Check NAN\n");
   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, target, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      TR::Register *srcReg = cg->evaluate(child);
      if (srcReg->getKind() == TR_FPR)
         {
         target = moveFromFloatRegister(node, srcReg, cg);
         cg->stopUsingRegister(srcReg);
         }
      else
         {
         target = srcReg;
         }
      cg->decReferenceCount(child);
      }
   node->setRegister(target);
   return target;
   }

TR::Register *OMR::ARM::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::RegisterPair        *pairTarget = NULL;
   TR::Register            *target = NULL;
   TR::Register            *lowReg  = NULL;
   TR::Register            *highReg = NULL;
   TR::Compilation *comp = cg->comp();

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *highMem, *lowMem;
      TR::MemoryReference  *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 8, cg);
      if (noFPRA)
      	 {
         lowReg  = cg->allocateRegister();
         highReg = cg->allocateRegister();
         highMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 0 : 4, 4, cg);
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, highReg, highMem);
         lowMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 4 : 0, 4, cg);
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, lowReg, lowMem);

         highMem->decNodeReferenceCounts();
         lowMem->decNodeReferenceCounts();
         pairTarget = cg->allocateRegisterPair(lowReg, highReg);
         node->setRegister(pairTarget);
         return pairTarget;
      	 }
      else
      	 {
      	 target = cg->allocateRegister(TR_FPR);
      	 highMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, 0, 8, cg);
         if (highMem->getIndexRegister() && highMem->getBaseRegister())
            {
            TR::Register *tempReg = cg->allocateRegister();
            generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, highMem->getBaseRegister(), highMem->getIndexRegister());
            highMem->decNodeReferenceCounts();

            highMem = new (cg->trHeapMemory()) TR::MemoryReference(tempReg, 0, cg);
            generateTrg1MemInstruction(cg, ARMOp_fldd, node, target, highMem);
            cg->stopUsingRegister(tempReg);
            }
         else
      	    generateTrg1MemInstruction(cg, ARMOp_fldd, node, target, highMem);
      	 highMem->decNodeReferenceCounts();
         node->setRegister(target);
         return target;
      	 }
      }
   else
      {
      TR::Register *srcReg = cg->evaluate(child);
      if (noFPRA)
         {
         target = srcReg;
         }
      else
         {
         target = moveToDoubleRegister(node, srcReg, cg);
         cg->stopUsingRegister(srcReg);
         }

      cg->decReferenceCount(child);
      node->setRegister(target);
      return target;
      }
   }

TR::Register *OMR::ARM::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = NULL;
   TR::Register            *srcReg = NULL;
   TR::Register            *lowReg  = NULL;
   TR::Register            *highReg = NULL;
   TR::Compilation         *comp = cg->comp();

   TR_ASSERT(!node->normalizeNanValues(), "Check NAN\n");
   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *highMem, *lowMem;
      TR::MemoryReference  *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 8, cg);
      lowReg  = cg->allocateRegister();
      highReg = cg->allocateRegister();
      highMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 0 : 4, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, highReg, highMem);
      lowMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 4 : 0, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, lowReg, lowMem);

      highMem->decNodeReferenceCounts();
      lowMem->decNodeReferenceCounts();
      target = cg->allocateRegisterPair(lowReg, highReg);
      tempMR->decNodeReferenceCounts(); // Taken from PPC
      }
   else
      {
      srcReg = cg->evaluate(child);
      if (srcReg->getKind() == TR_FPR)
         {
         target = moveFromDoubleRegister(node, srcReg, cg);
         cg->stopUsingRegister(srcReg);
         }
      else
         {
         target = srcReg;
         }
      cg->decReferenceCount(child);
      }
   node->setRegister(target);
   return target; // returns RegisterPair.
   }

TR::Register *OMR::ARM::TreeEvaluator::fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Use PC relative to get to the constant data, loading data at Snippet area is too far
   // Example:
   //    add	r4, r15, #8 (+2 instructions)
   //    flds   s0, [r15]   (Take advantage of PC+8)
   //    mov    r15, r4     (Continue, bypassing the data)
   //    .word  0x3fc00000  (Float value)
   //    fmrs   r0, s0      (if result goes back to general register)

   TR::Register *trgReg = NULL;
   TR::Register *tempReg = cg->allocateRegister();
   TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();
   float value = node->getFloat();
   uint32_t i32 = *(int32_t *)(&value);
   TR::Compilation *comp = cg->comp();
   TR::RealRegister *machineIPReg = cg->machine()->getRealRegister(TR::RealRegister::gr15);
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());

   traceMsg(comp, "In fconstEvaluator %x\n", i32);
   TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if(!noFPRA)
      {
      TR::addDependency(deps, floatTrgReg, TR::RealRegister::NoReg, TR_FPR, cg);
      }
   deps->stopAddingConditions();

   generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, tempReg, machineIPReg, 8, 0);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(machineIPReg, NULL, cg);
   generateTrg1MemInstruction(cg, ARMOp_flds, node, floatTrgReg, tempMR);
   generateTrg1Src1Instruction(cg, ARMOp_mov, node, machineIPReg, tempReg);
   //cg->stopUsingRegister(tempReg);

   // Place the constant
   generateImmInstruction(cg, ARMOp_dd, node, i32);
   //armCG(cg)->findOrCreateFloatConstant(&value, TR::Float, cursor);

   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         trgReg = floatTrgReg;
         }
      else
         {
         trgReg = moveFromFloatRegister(node, floatTrgReg, cg);
         cg->stopUsingRegister(floatTrgReg);
         }
      }
   else
      trgReg = floatTrgReg;

   TR::LabelSymbol *fenceLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   generateLabelInstruction(cg, ARMOp_label, node, fenceLabel, deps);
   cg->stopUsingRegister(tempReg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Use PC relative to get to the constant data, loading data at Snippet area is too far
   // Example:
   //   add     r4, r15, #12 (Target +3 instructions)
   //   fldd    d5, [r15]    (Take advantage of PC+8)
   //   mov     r15, r4      (Continue, bypassing the data)
   //   .word   0x33333333   (lower 32 for little endian)
   //   .word   0x40033333   (upper 32 for little endian)
   //   fmrrd   r0, r1, d5   (if result goes back to general register)
   TR::Register *trgReg = NULL;
   TR::Register *tempReg = cg->allocateRegister();
   TR::Register *doubleTrgReg = cg->allocateRegister(TR_FPR);
   double value = node->getDouble();
   uint64_t i64 = (*(int64_t *)&value);
   TR::Compilation *comp = cg->comp();
   TR::RealRegister *machineIPReg = cg->machine()->getRealRegister(TR::RealRegister::gr15);
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());

   traceMsg(comp, "In dconstEvaluator %x\n", i64);
   TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if(!noFPRA)
      {
      TR::addDependency(deps, doubleTrgReg, TR::RealRegister::NoReg, TR_FPR, cg);
      }
   deps->stopAddingConditions();

   generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, tempReg, machineIPReg, 12, 0);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(machineIPReg, NULL, cg);
   generateTrg1MemInstruction(cg, ARMOp_fldd, node, doubleTrgReg, tempMR);
   generateTrg1Src1Instruction(cg, ARMOp_mov, node, machineIPReg, tempReg);
   //cg->stopUsingRegister(tempReg);

   // Place the constant
   //armCG(cg)->findOrCreateFloatConstant(&value, TR::Double, high, low);
   if (TR::Compiler->target.cpu.isLittleEndian())
      {
      generateImmInstruction(cg, ARMOp_dd, node, (int32_t)i64);
      generateImmInstruction(cg, ARMOp_dd, node, (int32_t)((i64>>32) & 0xffffffff));
      }
   else
      {
      generateImmInstruction(cg, ARMOp_dd, node, (int32_t)((i64>>32) & 0xffffffff));
      generateImmInstruction(cg, ARMOp_dd, node, (int32_t)i64);
      }

   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(comp, "Result of node %p can stay in FP reg.\n", node);
         trgReg = doubleTrgReg;
         }
      else
         {
         trgReg = moveFromDoubleRegister(node, doubleTrgReg, cg);
         cg->stopUsingRegister(doubleTrgReg);
         }
      }
   else
      trgReg = doubleTrgReg;

   TR::LabelSymbol *fenceLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   generateLabelInstruction(cg, ARMOp_label, node, fenceLabel, deps);
   cg->stopUsingRegister(tempReg);
   node->setRegister(trgReg);
   return trgReg;
   }

// also handles TR::floadi
TR::Register *OMR::ARM::TreeEvaluator::floadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = NULL;
//#define STAYINFP
#ifdef STAYINFP
   TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();
#endif
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);

#ifdef STAYINFP
   if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
      tempMR->decNodeReferenceCounts();
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(tempReg, 0, cg);
      generateTrg1MemInstruction(cg, ARMOp_flds, node, floatTrgReg, tempMR);
      cg->stopUsingRegister(tempReg);
      }
   else
      generateTrg1MemInstruction(cg, ARMOp_flds, node, floatTrgReg, tempMR);
#endif

   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg in fload.\n", node);
         // This seem to break things.. trgReg = floatTrgReg;
#ifdef STAYINFP
         trgReg = floatTrgReg;
#else
         // Load directly to a general register
         trgReg = cg->allocateRegister();
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, trgReg, tempMR);
#endif
         }
      else
         {
          // Load directly to a general register
          trgReg = cg->allocateRegister();
          generateTrg1MemInstruction(cg, ARMOp_ldr, node, trgReg, tempMR);
         }
      }
   else
      {
      TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();
      if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
         {
         TR::Register *tempReg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         tempMR->decNodeReferenceCounts();

         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(tempReg, 0, cg);
         generateTrg1MemInstruction(cg, ARMOp_flds, node, floatTrgReg, tempMR);
         cg->stopUsingRegister(tempReg);
         }
      else
         generateTrg1MemInstruction(cg, ARMOp_flds, node, floatTrgReg, tempMR);
      trgReg = floatTrgReg;
      }

   if (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   tempMR->decNodeReferenceCounts();
   node->setRegister(trgReg);
   return trgReg;
   }

// also handles TR::dloadi
TR::Register *OMR::ARM::TreeEvaluator::dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = NULL;
   TR::Register *doubleTrgReg = cg->allocateRegister(TR_FPR);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);

   if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
      tempMR->decNodeReferenceCounts();

      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(tempReg, 0, cg);
      generateTrg1MemInstruction(cg, ARMOp_fldd, node, doubleTrgReg, tempMR);
      cg->stopUsingRegister(tempReg);
      }
   else
      generateTrg1MemInstruction(cg, ARMOp_fldd, node, doubleTrgReg, tempMR);

   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg in dload.\n", node);
         trgReg = doubleTrgReg;
         }
      else
         {
         trgReg = moveFromDoubleRegister(node, doubleTrgReg, cg);
         cg->stopUsingRegister(doubleTrgReg);
         }
      }
   else
      {
      trgReg = doubleTrgReg;
      }

   if (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   tempMR->decNodeReferenceCounts();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceReg = cg->evaluate(firstChild);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);

   if (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   if (noFPRA)
      {
      if (sourceReg->getKind() == TR_GPR)
         {
         // sourceReg is in a general register
         generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, sourceReg);
         }
      else if (sourceReg->getKind() == TR_FPR)
         {
         if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
            {
            TR::Register *tempReg = cg->allocateRegister();
            generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
            tempMR->setIndexRegister(NULL);
            tempMR->setIndexNode(NULL);
            tempMR->setBaseRegister(tempReg);
            tempMR->setBaseNode(node);
            generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
            cg->stopUsingRegister(tempReg);
            }
         else
            generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
         }
      }
   else
      {
      if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
         {
         TR::Register *tempReg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         tempMR->setIndexRegister(NULL);
         tempMR->setIndexNode(NULL);
         tempMR->setBaseRegister(tempReg);
         tempMR->setBaseNode(node);
         generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
         cg->stopUsingRegister(tempReg);
         }
      else
         generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
      }

   cg->decReferenceCount(firstChild);
   tempMR->decNodeReferenceCounts();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceReg = cg->evaluate(firstChild);
   bool  isUnresolved = node->getSymbolReference()->isUnresolved();

   if (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   if (noFPRA)
      {
      if (sourceReg->getKind() == TR_GPR)
         {
         // sourceReg is in general registers, mimic a lstore
         TR::MemoryReference *lowMR  = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
         TR::MemoryReference *highMR = new (cg->trHeapMemory()) TR::MemoryReference(*lowMR, 4, 4, cg);
         if (TR::Compiler->target.cpu.isBigEndian())
            {
            generateMemSrc1Instruction(cg, ARMOp_str, node, lowMR, sourceReg->getHighOrder());
            generateMemSrc1Instruction(cg, ARMOp_str, node, highMR, sourceReg->getLowOrder());
            }
         else
            {
            generateMemSrc1Instruction(cg, ARMOp_str, node, lowMR, sourceReg->getLowOrder());
            generateMemSrc1Instruction(cg, ARMOp_str, node, highMR, sourceReg->getHighOrder());
            }
         }
      else if (sourceReg->getKind() == TR_FPR)
         {
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
         if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
            {
            TR::Register *tempReg = cg->allocateRegister();
            generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
            tempMR->setIndexRegister(NULL);
            tempMR->setIndexNode(NULL);
            tempMR->setBaseRegister(tempReg);
            tempMR->setBaseNode(node);
            generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
            cg->stopUsingRegister(tempReg);
            }
         else
            generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
         tempMR->decNodeReferenceCounts();
         }
      else
         TR_ASSERT(0, "Unknown register type\n");
      }
   else
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
      if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
         {
         TR::Register *tempReg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         tempMR->setIndexRegister(NULL);
         tempMR->setIndexNode(NULL);
         tempMR->setBaseRegister(tempReg);
         tempMR->setBaseNode(node);
         generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
         cg->stopUsingRegister(tempReg);
         }
      else
         generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
      tempMR->decNodeReferenceCounts();
      }
   cg->decReferenceCount(firstChild);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ifstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *sourceReg = cg->evaluate(secondChild);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);

   if (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   if (noFPRA)
      {
      if (sourceReg->getKind() == TR_GPR)
         {
         // sourceReg is in a general register
         generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, sourceReg);
         }
      else if (sourceReg->getKind() == TR_FPR)
         {
         if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
            {
            TR::Register *tempReg = cg->allocateRegister();
            generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
            tempMR->setIndexRegister(NULL);
            tempMR->setIndexNode(NULL);
            tempMR->setBaseRegister(tempReg);
            //tempMR->setBaseNode(node);
            generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
            cg->stopUsingRegister(tempReg);
            }
         else
            generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
         }
      }
   else
      {
      if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
         {
         TR::Register *tempReg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         tempMR->setIndexRegister(NULL);
         tempMR->setIndexNode(NULL);
         tempMR->setBaseRegister(tempReg);
         generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
         cg->stopUsingRegister(tempReg);
         }
      else
         generateMemSrc1Instruction(cg, ARMOp_fsts, node, tempMR, sourceReg);
      }

   cg->decReferenceCount(secondChild);
   tempMR->decNodeReferenceCounts();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::idstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *sourceReg = cg->evaluate(secondChild);

   if (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }

   if (noFPRA)
      {
      if (sourceReg->getKind() == TR_GPR)
         {
         // sourceReg is in general registers, mimic a lstore
         TR::MemoryReference *lowMR  = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
         TR::MemoryReference *highMR = new (cg->trHeapMemory()) TR::MemoryReference(*lowMR, 4, 4, cg);
         if (TR::Compiler->target.cpu.isBigEndian())
            {
            generateMemSrc1Instruction(cg, ARMOp_str, node, lowMR, sourceReg->getHighOrder());
            generateMemSrc1Instruction(cg, ARMOp_str, node, highMR, sourceReg->getLowOrder());
            }
         else
            {
            generateMemSrc1Instruction(cg, ARMOp_str, node, lowMR, sourceReg->getLowOrder());
            generateMemSrc1Instruction(cg, ARMOp_str, node, highMR, sourceReg->getHighOrder());
            }
         }
      else if (sourceReg->getKind() == TR_FPR)
         {
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
         if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
            {
            TR::Register *tempReg = cg->allocateRegister();
            generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
            tempMR->setIndexRegister(NULL);
            tempMR->setIndexNode(NULL);
            tempMR->setBaseRegister(tempReg);
            generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
            cg->stopUsingRegister(tempReg);
            }
         else
            generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
         tempMR->decNodeReferenceCounts();
         }
      else
         TR_ASSERT(0, "Unknown register type\n");
      }
   else
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
      if (tempMR->getIndexRegister() && tempMR->getBaseRegister())
         {
         TR::Register *tempReg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_add, node, tempReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         tempMR->setIndexRegister(NULL);
         tempMR->setIndexNode(NULL);
         tempMR->setBaseRegister(tempReg);
         generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
         cg->stopUsingRegister(tempReg);
         }
      else
         generateMemSrc1Instruction(cg, ARMOp_fstd, node, tempMR, sourceReg);
      tempMR->decNodeReferenceCounts();
      }
   cg->decReferenceCount(secondChild);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // SOFT-ABI uses general registers to return floats
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   if (returnRegister->getKind() == TR_GPR)
      {
      TR::addDependency(deps, returnRegister, cg->getProperties().getIntegerReturnRegister(), TR_GPR, cg);
      }
   else if (returnRegister->getKind() == TR_FPR)
      {
      TR::Register *temp = moveFromFloatRegister(node, returnRegister, cg);
      TR::addDependency(deps, temp, cg->getProperties().getIntegerReturnRegister(), TR_GPR, cg);
      }
   else
      TR_ASSERT(0, "Unknown register type\n");
   generateAdminInstruction(cg, ARMOp_ret, node, deps);
   cg->comp()->setReturnInfo(TR_FloatReturn);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());

   if (returnRegister->getKind() == TR_GPR)
      {
      TR::addDependency(deps, returnRegister->getLowOrder(), cg->getProperties().getLongLowReturnRegister(), TR_GPR, cg);
      TR::addDependency(deps, returnRegister->getHighOrder(), cg->getProperties().getLongHighReturnRegister(), TR_GPR, cg);
      }
   else if (returnRegister->getKind() == TR_FPR)
      {
      TR::Register *temp = moveFromDoubleRegister(node, returnRegister, cg);
      TR::addDependency(deps, temp->getLowOrder(), cg->getProperties().getLongLowReturnRegister(), TR_GPR, cg);
      TR::addDependency(deps, temp->getHighOrder(), cg->getProperties().getLongHighReturnRegister(), TR_GPR, cg);
      }
   else
      TR_ASSERT(0, "Unknown register type\n");
   generateAdminInstruction(cg, ARMOp_ret, node, deps);
   cg->comp()->setReturnInfo(TR_DoubleReturn);
   return NULL;
   }

static bool isFPStrictMul(TR::Node *node, TR::Compilation * comp)
   {
   if(!node->getOpCode().isMul() ||
      !node->isFPStrictCompliant() ||
      node->getRegister())
   return false;

   if( node->getReferenceCount() < 2 && node->getRegister() == NULL)
     return true;

   node->setIsFPStrictCompliant(false);// need to set this otherwise children get incorrectly bumped
   return false;
   }

/** Generate a fused multiply add from the tree (A * B) + C, where addNode is the + node
 * and mulNode the * subtree.  Not supporting noFPRA at this time
 */
static TR::Register *generateFusedMultiplyAdd(TR::Node *addNode, TR_ARMOpCodes OpCode, TR::CodeGenerator *cg)
   {
   TR::Node     *mulNode = addNode->getFirstChild();
   TR::Node     *addChild    = addNode->getSecondChild();

   if (!isFPStrictMul(mulNode, cg->comp()))
      {
      addChild = addNode->getFirstChild();
      mulNode  = addNode->getSecondChild();
      }

   TR_ASSERT(mulNode->getReferenceCount() < 2,"Mul node 0x%p reference count %d >= 2\n",mulNode,mulNode->getReferenceCount());
   TR_ASSERT(mulNode->getOpCode().isMul(),"Unexpected op!=mul %p\n",mulNode);
   TR_ASSERT(mulNode->isFPStrictCompliant(),"mul node %p is not fpStrict Compliant\n",mulNode);
   TR::Register *source1Register = cg->evaluate(mulNode->getFirstChild());
   TR::Register *source2Register = cg->evaluate(mulNode->getSecondChild());
   TR::Register *source3Register = cg->evaluate(addChild);

   // Result is in the put back to source3Register
   TR::Register *trgRegister = source3Register;
   if (addNode->getDataType() == TR::Float)
      {
      if (noFPRA)
         {
         TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();
         generateTrg1Src2Instruction(cg, OpCode, addNode, floatTrgReg, moveToFloatRegister(addNode, source1Register, cg), moveToFloatRegister(addNode, source2Register, cg));
         generateTrg1Src1Instruction(cg, ARMOp_fmrs, addNode, trgRegister, floatTrgReg);
         cg->stopUsingRegister(floatTrgReg);
         }
      else
         {
         generateTrg1Src2Instruction(cg, OpCode, addNode, trgRegister, source1Register, source2Register);
         }
      }
   else
      {
      // TR::Double
      if (noFPRA)
         {
         TR::Register *doubleTrgReg = cg->allocateRegister(TR_FPR);
         generateTrg1Src2Instruction(cg, OpCode, addNode, doubleTrgReg, moveToDoubleRegister(addNode, source1Register, cg), moveToDoubleRegister(addNode, source2Register, cg));
         generateTrg2Src1Instruction(cg, ARMOp_fmrrd, addNode, trgRegister->getLowOrder(), trgRegister->getHighOrder(), doubleTrgReg);
         cg->stopUsingRegister(doubleTrgReg);
         }
      else
         {
         generateTrg1Src2Instruction(cg, OpCode, addNode, trgRegister, source1Register, source2Register);
         }
      }

   addNode->setRegister(trgRegister);
   cg->decReferenceCount(mulNode->getFirstChild());
   cg->decReferenceCount(mulNode->getSecondChild());
   cg->decReferenceCount(mulNode); // don't forget this guy!
   //cg->decReferenceCount(addChild);
   return trgRegister;
   }

TR::Register *OMR::ARM::TreeEvaluator::faddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: Check conditions to use fmacs if possible
   TR::Compilation *comp = cg->comp();
   TR::Register *result = NULL;
   if (((isFPStrictMul(node->getFirstChild(), comp) && (node->getSecondChild()->getReferenceCount() == 1)) ||
        (isFPStrictMul(node->getSecondChild(), comp) && (node->getFirstChild()->getReferenceCount() == 1))) &&
         performTransformation(comp, "O^O Changing [%p] to fmacs\n", node))
      {
      result = generateFusedMultiplyAdd(node, ARMOp_fmacs, cg);
      }
   else
      {
      result = singlePrecisionEvaluator(node, ARMOp_fadds, cg);
      }
   return result;
   }

TR::Register *OMR::ARM::TreeEvaluator::daddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: Check conditions to use fmacd if possible
   TR::Register *result = NULL;
   TR::Compilation *comp = cg->comp();
   if (((isFPStrictMul(node->getFirstChild(), comp) && (node->getSecondChild()->getReferenceCount() == 1)) ||
        (isFPStrictMul(node->getSecondChild(), comp) && (node->getFirstChild()->getReferenceCount() == 1))) &&
         performTransformation(comp, "O^O Changing [%p] to fmacd\n", node))
      {
      result = generateFusedMultiplyAdd(node, ARMOp_fmacd, cg);
      }
   else
      {
      result = doublePrecisionEvaluator(node, ARMOp_faddd, cg);
      }
   return result;
   }

TR::Register *OMR::ARM::TreeEvaluator::dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: Check conditions to use fmscd if possible
   TR::Compilation *comp = cg->comp();
   TR::Register *result = NULL;
   if (isFPStrictMul(node->getFirstChild(), comp) &&
      (node->getSecondChild()->getReferenceCount() == 1) &&
       performTransformation(comp, "O^O Changing [%p] to fmscd\n",node))
      {
      result = generateFusedMultiplyAdd(node, ARMOp_fmscd, cg);
      }
   else if (isFPStrictMul(node->getSecondChild(), comp) &&
      (node->getFirstChild()->getReferenceCount() == 1) &&
       performTransformation(comp, "O^O Changing [%p] to fnmacd\n",node))
      {
      result = generateFusedMultiplyAdd(node, ARMOp_fnmacd, cg);
      }
   else
      {
      result = doublePrecisionEvaluator(node, ARMOp_fsubd, cg);
      }
   return result;
   }

TR::Register *OMR::ARM::TreeEvaluator::fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: Check conditions to use fmscs if possible
   TR::Compilation *comp = cg->comp();
   TR::Register *result = NULL;
   if (isFPStrictMul(node->getFirstChild(), comp) &&
      (node->getSecondChild()->getReferenceCount() == 1) &&
       performTransformation(comp, "O^O Changing [%p] to fmscs\n",node))
      {
      result = generateFusedMultiplyAdd(node, ARMOp_fmscs, cg);
      }
   else if (isFPStrictMul(node->getSecondChild(), comp) &&
      (node->getFirstChild()->getReferenceCount() == 1) &&
       performTransformation(comp, "O^O Changing [%p] to fnmacs\n",node))
      {
      result = generateFusedMultiplyAdd(node, ARMOp_fnmacs, cg);
      }
   else
      {
      result = singlePrecisionEvaluator(node, ARMOp_fsubs, cg);
      }
   return result;
   }

TR::Register *OMR::ARM::TreeEvaluator::fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, ARMOp_fmuls, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, ARMOp_fmuld, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, ARMOp_fdivs, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, ARMOp_fdivd, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = callFloatRemainderHelper(node, cg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = callDoubleRemainderHelper(node, cg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, ARMOp_fabss, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, ARMOp_fabsd, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: Check tree for possibile combinations
   TR::Register *result = NULL;
   TR::Node *firstChild = node->getFirstChild();
   bool isAdd = firstChild->getOpCode().isAdd();
   TR::Compilation *comp = cg->comp();

   if (firstChild->getReferenceCount() < 2 &&
      !firstChild->getRegister() && isAdd)
      {
      // fneg(node) -> fadd(firstChild) -> a multiply in one of the children
      if (((isFPStrictMul(firstChild->getFirstChild(), comp) &&
         firstChild->getSecondChild()->getReferenceCount() == 1) ||
         (isFPStrictMul(firstChild->getSecondChild(), comp) &&
         firstChild->getFirstChild()->getReferenceCount() == 1)) &&
         performTransformation(comp, "O^O Changing [%p] to fnmscs\n", node))
         {
         result = generateFusedMultiplyAdd(node, ARMOp_fnmscs, cg);
         firstChild->unsetRegister(); //unset as the first child isn't the result node
         }
      else
         {
         result = singlePrecisionEvaluator(node, ARMOp_fnegs, cg);
         }
      }
   else if (isFPStrictMul(firstChild, comp) &&
      firstChild->getReferenceCount() < 2 &&
      !firstChild->getRegister() &&
      performTransformation(comp, "O^O Changing [%p] to fnmuls\n", node))
      {
      // fneg(node) -> fmul(firstChild)
      TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();
      TR::Register *src1Register = cg->evaluate(firstChild->getFirstChild());
      TR::Register *src2Register = cg->evaluate(firstChild->getSecondChild());
      if (noFPRA)
         {
         generateTrg1Src2Instruction(cg, ARMOp_fnmuls, node, floatTrgReg, moveToFloatRegister(node, src1Register, cg), moveToFloatRegister(node, src2Register, cg));
         result = moveFromFloatRegister(node, floatTrgReg, cg);
         cg->stopUsingRegister(floatTrgReg);
         }
      else
         {
         generateTrg1Src2Instruction(cg, ARMOp_fnmuls, node, floatTrgReg, src1Register, src2Register);
         result = floatTrgReg;
         }
      cg->decReferenceCount(firstChild);
      }
   else
      {
      result = singlePrecisionEvaluator(node, ARMOp_fnegs, cg);
      }
   node->setRegister(result);
   return result;
   }

TR::Register *OMR::ARM::TreeEvaluator::dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: Check tree for possible combinations
   TR::Register *result = NULL;
   TR::Node *firstChild = node->getFirstChild();
   bool isAdd = firstChild->getOpCode().isAdd();
   TR::Compilation *comp = cg->comp();

   if (firstChild->getReferenceCount() < 2 &&
      !firstChild->getRegister() && isAdd)
      {
      // dneg(node) -> dadd(firstChild) -> a multiply in one of the children
      if (((isFPStrictMul(firstChild->getFirstChild(), comp) &&
         firstChild->getSecondChild()->getReferenceCount() == 1) ||
         (isFPStrictMul(firstChild->getSecondChild(), comp) &&
         firstChild->getFirstChild()->getReferenceCount() == 1)) &&
         performTransformation(comp, "O^O Changing [%p] to fnmscd\n", node))
         {
         result = generateFusedMultiplyAdd(node, ARMOp_fnmscd, cg);
         firstChild->unsetRegister(); //unset as the first child isn't the result node
         }
      else
         {
         result = doublePrecisionEvaluator(node, ARMOp_fnegd, cg);
         }
      }
   else if (isFPStrictMul(firstChild, comp) &&
      firstChild->getReferenceCount() < 2 &&
      !firstChild->getRegister() &&
      performTransformation(comp, "O^O Changing [%p] to fnmuld\n", node))
      {
      // fneg(node) -> fmul(firstChild)
      TR::Register *doubleTrgReg = cg->allocateRegister(TR_FPR);
      TR::Register *src1Register = cg->evaluate(firstChild->getFirstChild());
      TR::Register *src2Register = cg->evaluate(firstChild->getSecondChild());
      if (noFPRA)
         {
         generateTrg1Src2Instruction(cg, ARMOp_fnmuld, node, doubleTrgReg, moveToDoubleRegister(node, src1Register, cg), moveToDoubleRegister(node, src2Register, cg));
         result = moveFromDoubleRegister(node, doubleTrgReg, cg);
         cg->stopUsingRegister(doubleTrgReg);
         }
      else
         {
         generateTrg1Src2Instruction(cg, ARMOp_fnmuld, node, doubleTrgReg, src1Register, src2Register);
         result = doubleTrgReg;
         }
      cg->decReferenceCount(firstChild);
      }
   else
      {
      result = doublePrecisionEvaluator(node, ARMOp_fnegd, cg);
      }
   node->setRegister(result);
   return result;
   }

TR::Register *OMR::ARM::TreeEvaluator::i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Support iu2f, b2f, s2f and su2f
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = NULL;
   TR::Register *trgReg = NULL;
   TR_ARMOpCodes opcode = (node->getOpCodeValue() == TR::iu2f) ? ARMOp_fuitos : ARMOp_fsitos;

   if (firstChild->getReferenceCount() == 1 &&
       firstChild->getRegister() == NULL &&
      (firstChild->getOpCodeValue() == TR::iload || firstChild->getOpCodeValue() == TR::iloadi) &&
      (firstChild->getNumChildren() > 0) &&
      (firstChild->getFirstChild()->getNumChildren() == 1) &&
      !(firstChild->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP()))
      {
      // Coming from memory, last use. Use flds to save the move
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, 4, cg);
      TR::Register *tempReg = cg->allocateSinglePrecisionRegister();
      TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();

      generateTrg1MemInstruction(cg, ARMOp_flds, firstChild, tempReg, tempMR);
      generateTrg1Src1Instruction(cg, opcode, node, floatTrgReg, tempReg);

      if (noFPRA)
         {
         if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
            {
            traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
            trgReg = floatTrgReg;
            }
         else
            {
            trgReg = moveFromFloatRegister(node, floatTrgReg, cg);
            cg->stopUsingRegister(floatTrgReg);
            }
         }
      else
      	 trgReg = floatTrgReg;

      cg->stopUsingRegister(tempReg);
      node->setRegister(trgReg);
      cg->decReferenceCount(firstChild);
      }
   else
      {
      // GPR -> FP -> Convert
      trgReg = singlePrecisionEvaluator(node, opcode, cg);
      }
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Supports iu2d, b2d, s2d, su2d
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = NULL;
   TR::Register *trgReg = NULL;
   TR::Register *tempReg = cg->allocateSinglePrecisionRegister();

   TR_ARMOpCodes opcode = (node->getOpCodeValue() != TR::iu2d && node->getOpCodeValue() != TR::su2d) ? ARMOp_fsitod : ARMOp_fuitod;  // D[t], S[s]

   if (firstChild->getReferenceCount() == 1 &&
       firstChild->getRegister() == NULL &&
      (firstChild->getOpCodeValue() == TR::iload || firstChild->getOpCodeValue() == TR::iloadi) &&
      (firstChild->getNumChildren() > 0) &&
      (firstChild->getFirstChild()->getNumChildren() == 1) &&
      !(firstChild->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP()))
      {
      // Coming from memory, last use
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, 4, cg);

      generateTrg1MemInstruction(cg, ARMOp_flds, firstChild, tempReg, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      // GPR -> FP -> Convert
      src1Reg = cg->evaluate(firstChild); // int
      tempReg = moveToFloatRegister(node, src1Reg, cg); // fmsr
      cg->decReferenceCount(firstChild);
      }

   TR::Register *doubleTrgReg = cg->allocateRegister(TR_FPR);
   generateTrg1Src1Instruction(cg, opcode, node, doubleTrgReg, tempReg); // The conversion.. source is float, result is double

   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         trgReg = doubleTrgReg;
         }
      else
         {
         trgReg = moveFromDoubleRegister(node, doubleTrgReg, cg);
         cg->stopUsingRegister(doubleTrgReg);
         }
      }
   else
      {
      trgReg = doubleTrgReg;
      }
   cg->stopUsingRegister(tempReg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = callLong2FloatHelper(node, cg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = callLong2DoubleHelper(node, cg);
   node->setRegister(trgReg);
   return trgReg;
   }

// f2s handled by d2sEvaluator
// f2c handled by d2cEvaluator
// f2b handled by d2bEvaluator
// f2l handled by d2lEvaluator

TR::Register *OMR::ARM::TreeEvaluator::f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = NULL;
   TR::Register *trgReg = NULL; // NEW
   TR::Register *doubleTrgReg = cg->allocateRegister(TR_FPR);

   if (firstChild->getReferenceCount() == 1 &&
       firstChild->getRegister() == NULL &&
      (firstChild->getOpCodeValue() == TR::fload || firstChild->getOpCodeValue() == TR::floadi) &&
      (firstChild->getNumChildren() > 0) &&
      (firstChild->getFirstChild()->getNumChildren() == 1) &&
      !(firstChild->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP()))
      {
      // Coming from memory, last use
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, 4, cg);
      TR::Register *tempReg = cg->allocateSinglePrecisionRegister();

      generateTrg1MemInstruction(cg, ARMOp_flds, firstChild, tempReg, tempMR);
      generateTrg1Src1Instruction(cg, ARMOp_fcvtds, node, doubleTrgReg, tempReg);

      tempMR->decNodeReferenceCounts();
      cg->stopUsingRegister(tempReg);
      }
   else
      {
      src1Reg = cg->evaluate(firstChild);
      generateTrg1Src1Instruction(cg, ARMOp_fcvtds, node, doubleTrgReg, moveToFloatRegister(node, src1Reg, cg));
      cg->decReferenceCount(firstChild);
      }

   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         trgReg = doubleTrgReg;
         }
      else
         {
         trgReg = moveFromDoubleRegister(node, doubleTrgReg, cg);
         cg->stopUsingRegister(doubleTrgReg);
         }
      }
   else
      {
      trgReg = doubleTrgReg;
      }

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Supports f2iu
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = NULL;
   TR::Register *trgReg = NULL;
   TR_ARMOpCodes opcode = (node->getOpCodeValue() == TR::f2i) ? ARMOp_ftosizs : ARMOp_ftouizs;
   TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();

   if (firstChild->getReferenceCount() == 1 &&
       firstChild->getRegister() == NULL &&
      (firstChild->getOpCodeValue() == TR::fload || firstChild->getOpCodeValue() == TR::floadi) &&
      (firstChild->getNumChildren() > 0) &&
      (firstChild->getFirstChild()->getNumChildren() == 1) &&
      !(firstChild->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP()))
      {
      // Coming from memory, last use
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, 4, cg);
      TR::Register *tempReg = cg->allocateSinglePrecisionRegister();

      generateTrg1MemInstruction(cg, ARMOp_flds, firstChild, tempReg, tempMR);
      generateTrg1Src1Instruction(cg, opcode, node, floatTrgReg, tempReg);
      tempMR->decNodeReferenceCounts();
      cg->stopUsingRegister(tempReg);
      }
   else
      {
      src1Reg = cg->evaluate(firstChild);
      generateTrg1Src1Instruction(cg, opcode, node, floatTrgReg, moveToFloatRegister(node, src1Reg, cg));
      cg->decReferenceCount(firstChild);
      }

   // Integer result
   trgReg = moveFromFloatRegister(node, floatTrgReg, cg);
   cg->stopUsingRegister(floatTrgReg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Supports d2iu
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = NULL;
   TR::Register *trgReg = NULL;
   TR_ARMOpCodes opcode = (node->getOpCodeValue() == TR::d2i) ? ARMOp_ftosizd : ARMOp_ftouizd;
   TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister();

   if (firstChild->getReferenceCount() == 1 &&
       firstChild->getRegister() == NULL &&
      (firstChild->getOpCodeValue() == TR::dload || firstChild->getOpCodeValue() == TR::dloadi) &&
      (firstChild->getNumChildren() > 0) &&
      (firstChild->getFirstChild()->getNumChildren() == 1) &&
      !(firstChild->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP()))
      {
      // Coming from memory, last use
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, 4, cg);
      TR::Register *tempReg = cg->allocateRegister(TR_FPR); // double

      generateTrg1MemInstruction(cg, ARMOp_fldd, firstChild, tempReg, tempMR);
      generateTrg1Src1Instruction(cg, opcode, node, floatTrgReg, tempReg);
      tempMR->decNodeReferenceCounts();
      cg->stopUsingRegister(tempReg);
      }
   else
      {
      src1Reg = cg->evaluate(firstChild);
      generateTrg1Src1Instruction(cg, opcode, node, floatTrgReg, moveToDoubleRegister(node, src1Reg, cg));
      cg->decReferenceCount(firstChild);
      }

   // Integer result
   trgReg = moveFromFloatRegister(node, floatTrgReg, cg);
   cg->stopUsingRegister(floatTrgReg);

   node->setRegister(trgReg);
   return trgReg;
   }

// also handles f2c
TR::Register *OMR::ARM::TreeEvaluator::d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("Not expecting d2c/f2c\n");
   return NULL;
   }

// also handles f2s
TR::Register *OMR::ARM::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("Not expecting d2s/f2s\n");
   return NULL;
   }

// also handles f2b
TR::Register *OMR::ARM::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("Not expecting d2b/f2b\n");
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = callFloat2LongHelper(node, cg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = callDouble2LongHelper(node, cg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = NULL;
   TR::Register *trgReg = NULL;
   TR::Register *floatTrgReg = cg->allocateSinglePrecisionRegister(); // NEW

   if (firstChild->getReferenceCount() == 1 &&
       firstChild->getRegister() == NULL &&
      (firstChild->getOpCodeValue() == TR::dload || firstChild->getOpCodeValue() == TR::dloadi) &&
      (firstChild->getNumChildren() > 0) &&
      (firstChild->getFirstChild()->getNumChildren() == 1) &&
      !(firstChild->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP()))
      {
      // Coming from memory, last use
      TR::Register *tempReg = cg->allocateRegister(TR_FPR);
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, 8, cg);
      generateTrg1MemInstruction(cg, ARMOp_fldd, firstChild, tempReg, tempMR);
      generateTrg1Src1Instruction(cg, ARMOp_fcvtsd, node, floatTrgReg, tempReg);
      tempMR->decNodeReferenceCounts();
      cg->stopUsingRegister(tempReg);
      }
   else
      {
      src1Reg = cg->evaluate(firstChild);
      generateTrg1Src1Instruction(cg, ARMOp_fcvtsd, node, floatTrgReg, moveToDoubleRegister(node, src1Reg, cg));
      cg->decReferenceCount(firstChild);
      }

   if (noFPRA)
      {
      if (resultCanStayInFloatRegister(cg->getCurrentEvaluationTreeTop()->getNode(), node))
         {
         traceMsg(cg->comp(), "Result of node %p can stay in FP reg.\n", node);
         trgReg = floatTrgReg;
         }
      else
         {
         trgReg = moveFromFloatRegister(node, floatTrgReg, cg);
         cg->stopUsingRegister(floatTrgReg);
         }
      }
   else
      {
      trgReg = floatTrgReg;
      }
   node->setRegister(trgReg);
   return trgReg;
   }

static void ifFloatEvaluator(TR::Node *node, TR_ARMOpCodes opCode, TR_ARMConditionCode cond, TR::CodeGenerator *cg, bool isDouble, bool unordered)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::LabelSymbol *label = node->getBranchDestination()->getNode()->getLabel();

   TR::RegisterDependencyConditions *deps = NULL;

   if (noFPRA)
      {
      if (isDouble)
      	  {
      	  generateSrc2Instruction(cg, opCode, node, moveToDoubleRegister(node, src1Reg, cg), moveToDoubleRegister(node, src2Reg, cg));
      	  }
      else
      	  {
      	  generateSrc2Instruction(cg, opCode, node, moveToFloatRegister(node, src1Reg, cg), moveToFloatRegister(node, src2Reg, cg));
      	  }
      }
   else
      {
      generateSrc2Instruction(cg, opCode, node, src1Reg, src2Reg);
      }
   // Move the FPSCR flags back to the APSR
   generateInstruction(cg, ARMOp_fmstat, node);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      cg->decReferenceCount(thirdChild);
      }

   if (deps)
      {
      generateConditionalBranchInstruction(cg, node, cond, label, deps);
      if (unordered) generateConditionalBranchInstruction(cg, node, ARMConditionCodeVS, label, deps);
      }
   else
      {
      generateConditionalBranchInstruction(cg, node, cond, label);
      if (unordered) generateConditionalBranchInstruction(cg, node, ARMConditionCodeVS, label);
      }
   }

static TR::Register *setboolFloatEvaluator(TR::Node *node, TR_ARMOpCodes opCode, TR_ARMConditionCode cond, TR::CodeGenerator *cg, bool isDouble, bool unordered)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg = cg->allocateRegister();

   if (noFPRA)
      {
      if (isDouble)
      	  {
      	  generateSrc2Instruction(cg, opCode, node, moveToDoubleRegister(node, src1Reg, cg), moveToDoubleRegister(node, src2Reg, cg));
      	  }
      else
      	  {
      	  generateSrc2Instruction(cg, opCode, node, moveToFloatRegister(node, src1Reg, cg), moveToFloatRegister(node, src2Reg, cg));
      	  }
      }
   else
      {
      generateSrc2Instruction(cg, opCode, node, src1Reg, src2Reg);
      }
   // Move the FPSCR flags back to the APSR
   generateInstruction(cg, ARMOp_fmstat, node);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   // Initialize the 0 result first
   generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, 0, 0);

   TR::Instruction *cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, 1, 0);
   cursor->setConditionCode(cond);

   if (unordered)
      {
      cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, 1, 0, cursor);
      cursor->setConditionCode(ARMConditionCodeVS);
      }
   node->setRegister(trgReg);
   return trgReg;
   }

// also handles TR::iffcmpeq, TR::iffcmpequ, TR::ifdcmpequ
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool isDouble = (node->getOpCodeValue() == TR::ifdcmpeq || node->getOpCodeValue() == TR::ifdcmpequ);
   bool unordered =  (node->getOpCodeValue() == TR::iffcmpequ || node->getOpCodeValue() == TR::ifdcmpequ);
   ifFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, ARMConditionCodeEQ, cg, isDouble, unordered);
   return NULL;
   }

// also handles TR::iffcmpne, TR::iffcmpneu, TR::ifdcmpneu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeNE may imply unordered, but looks like it is not used in common code
   bool isDouble = (node->getOpCodeValue() == TR::ifdcmpne || node->getOpCodeValue() == TR::ifdcmpneu);

   ifFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, ARMConditionCodeNE, cg, isDouble, false);
   return NULL;
   }

// also handles TR::iffcmplt, TR::iffcmpltu, TR::ifdcmpltu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeCC is for LT and ARMConditionCodeLT is for LTU
   bool isDouble = (node->getOpCodeValue() == TR::ifdcmplt || node->getOpCodeValue() == TR::ifdcmpltu);
   bool unordered = (node->getOpCodeValue() == TR::iffcmpltu || node->getOpCodeValue() == TR::ifdcmpltu);

   ifFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeLT : ARMConditionCodeCC, cg, isDouble, false);
   return NULL;
   }

// also handles TR::iffcmpge, TR::iffcmpgeu, TR::ifdcmpgeu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeGE is for GE and ARMConditionCodeCS is for GEU
   bool isDouble = (node->getOpCodeValue() == TR::ifdcmpge || node->getOpCodeValue() == TR::ifdcmpgeu);
   bool unordered = (node->getOpCodeValue() == TR::iffcmpgeu || node->getOpCodeValue() == TR::ifdcmpgeu);

   ifFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeCS : ARMConditionCodeGE, cg, isDouble, false);
   return NULL;
   }

// also handles TR::iffcmpgt, TR::iffcmpgtu, TR::ifdcmpgtu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeGT is for GT and ARMConditionCodeHI is for GTU
   bool isDouble = (node->getOpCodeValue() == TR::ifdcmpgt || node->getOpCodeValue() == TR::ifdcmpgtu);
   bool unordered = (node->getOpCodeValue() == TR::iffcmpgtu || node->getOpCodeValue() == TR::ifdcmpgtu);

   ifFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeHI : ARMConditionCodeGT, cg, isDouble, false);
   return NULL;
   }

// also handles TR::iffcmple, TR::iffcmpleu, TR::ifdcmpleu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeLS is for LE and ARMConditionCodeLE is for LEU
   bool isDouble = (node->getOpCodeValue() == TR::ifdcmple || node->getOpCodeValue() == TR::ifdcmpleu);
   bool unordered = (node->getOpCodeValue() == TR::iffcmpleu || node->getOpCodeValue() == TR::ifdcmpleu);

   ifFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeLE : ARMConditionCodeLS, cg, isDouble, false);
   return NULL;
   }

// also handles TR::fcmpeq, TR::fcmpequ, TR::dcmpequ
TR::Register *OMR::ARM::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool isDouble = (node->getOpCodeValue() == TR::dcmpeq || node->getOpCodeValue() == TR::dcmpequ);
   bool unordered =  (node->getOpCodeValue() == TR::fcmpequ || node->getOpCodeValue() == TR::dcmpequ);
   return setboolFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, ARMConditionCodeEQ, cg, isDouble, unordered);
   }

// also handles TR::fcmpne, TR::fcmpneu, TR::dcmpneu
TR::Register *OMR::ARM::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeNE may imply unordered, but looks like it is not used in common code
   bool isDouble = (node->getOpCodeValue() == TR::dcmpne || node->getOpCodeValue() == TR::dcmpneu);
   return setboolFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, ARMConditionCodeNE, cg, isDouble, false);
   }

// also handles TR::fcmplt, TR::fcmpltu, TR::dcmpltu
TR::Register *OMR::ARM::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeCC is for LT and ARMConditionCodeLT is for LTU
   bool isDouble = (node->getOpCodeValue() == TR::dcmplt || node->getOpCodeValue() == TR::dcmpltu);
   bool unordered =  (node->getOpCodeValue() == TR::fcmpltu || node->getOpCodeValue() == TR::dcmpltu);
   return setboolFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeLT : ARMConditionCodeCC, cg, isDouble, false);
   }

// also handles TR::fcmpge
TR::Register *OMR::ARM::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeGE is for GE and ARMConditionCodeCS is for GEU
   bool isDouble = (node->getOpCodeValue() == TR::dcmpge || node->getOpCodeValue() == TR::dcmpgeu);
   bool unordered = (node->getOpCodeValue() == TR::fcmpgeu || node->getOpCodeValue() == TR::dcmpgeu);

   return setboolFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeCS : ARMConditionCodeGE, cg, isDouble, false);
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeGT is for GT and ARMConditionCodeHI is for GTU
   bool isDouble = (node->getOpCodeValue() == TR::dcmpgt || node->getOpCodeValue() == TR::dcmpgtu);
   bool unordered = (node->getOpCodeValue() == TR::fcmpgtu || node->getOpCodeValue() == TR::dcmpgtu);

   return setboolFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeHI : ARMConditionCodeGT, cg, isDouble, false);
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMConditionCodeLS is for LE and ARMConditionCodeLE is for LEU
   bool isDouble = (node->getOpCodeValue() == TR::dcmple || node->getOpCodeValue() == TR::dcmpleu);
   bool unordered = (node->getOpCodeValue() == TR::fcmpleu || node->getOpCodeValue() == TR::dcmpleu);

   return setboolFloatEvaluator(node, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, unordered ? ARMConditionCodeLE : ARMConditionCodeLS, cg, isDouble, false);
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Support TR::fcmpl, TR::dcmpg and TR::fcmpg as well
   // TR::dcmpl/TR::fcmpl (1 if c1 > c2, 0 if c1 == c2, -1 if c1 < c2 or unordered)
   // TR::dcmpg/TR::fcmpg (1 if c1 > c2 or unordered, 0 if c1 == c2, -1 if c1 < c2)
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg = cg->allocateRegister();
   bool isDouble = (node->getOpCodeValue() == TR::dcmpg || node->getOpCodeValue() == TR::dcmpl);
   bool isGType = (node->getOpCodeValue() == TR::dcmpg || node->getOpCodeValue() == TR::fcmpg);

   if (noFPRA)
      {
      if (isDouble)
      	 {
      	 generateSrc2Instruction(cg, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, node, moveToDoubleRegister(node, src1Reg, cg), moveToDoubleRegister(node, src2Reg, cg));
      	 }
      else
      	 {
      	 generateSrc2Instruction(cg, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, node, moveToFloatRegister(node, src1Reg, cg), moveToFloatRegister(node, src2Reg, cg));
      	 }
      }
   else
      {
      generateSrc2Instruction(cg, isDouble ? ARMOp_fcmpd : ARMOp_fcmps, node, src1Reg, src2Reg);
      }
   // Move the FPSCR flags back to the APSR
   generateInstruction(cg, ARMOp_fmstat, node);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   TR::Instruction *cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, 0, 0);
   cursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, trgReg, trgReg, 1, 0, cursor);
   if (isGType)
      cursor->setConditionCode(ARMConditionCodeHI); // GT or unordered
   else
      cursor->setConditionCode(ARMConditionCodeGT); // GT

   cursor = generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, trgReg, trgReg, 1, 0, cursor);
   if (isGType)
      cursor->setConditionCode(ARMConditionCodeCC); // LT
   else
      cursor->setConditionCode(ARMConditionCodeLT); // LT or unordered

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Support dRegLoad
   TR::Register *globalReg = node->getRegister();
   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister(TR_FPR);
      if (node->getOpCodeValue() == TR::fRegLoad)
      	  globalReg->setIsSinglePrecision();
      node->setRegister(globalReg);
      }
   return globalReg;
   }


TR::Register *OMR::ARM::TreeEvaluator::fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Support dRegStore
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   cg->decReferenceCount(child);
   return globalReg;
   }

static TR::Register *generateFloatMaxMin(TR::Node *node, TR::CodeGenerator *cg, bool max)
   {
   TR::Node *firstChild  = node->getFirstChild();
   TR::DataType data_type = firstChild->getDataType();
   TR::DataType type = firstChild->getType();
   TR::Instruction *cursor;
   TR::Register *trgReg, *trgFloatReg;

   if (data_type != TR::Float && data_type != TR::Double && node->getNumChildren() == 2)
      {
      TR_ASSERT(false, "assertion failure"); // We shouldn't get here at all
      }

   bool isFloat = (data_type == TR::Float);
   TR::Register *reg = cg->evaluate(firstChild);
   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   if (noFPRA && reg->getKind() == TR_GPR)
      {
      trgFloatReg = isFloat ? moveToFloatRegister(node, reg, cg) : moveToDoubleRegister(node, reg, cg);
      trgReg = reg;
      }
   else
      {
      if (cg->canClobberNodesRegister(firstChild))
         {
         trgFloatReg = reg; // use first child as a target
         }
      else
         {
         if (isFloat)
            {
            trgFloatReg = cg->allocateSinglePrecisionRegister();
            generateTrg1Src1Instruction(cg, ARMOp_fcpys, node, trgFloatReg, moveToFloatRegister(node, reg, cg));
            }
         else
            {
            trgFloatReg = cg->allocateRegister(TR_FPR);
            generateTrg1Src1Instruction(cg, ARMOp_fcpyd, node, trgFloatReg, moveToDoubleRegister(node, reg, cg));
            }
         }
         if (max)
            {
            trgReg = isFloat ? moveFromFloatRegister(node, reg, cg) : moveFromDoubleRegister(node, reg, cg);
            }
      }

   TR::Node *child = node->getChild(1); // Second child
   TR::Register *childReg = cg->evaluate(child);
   TR::Register *childFloatReg;

   if (noFPRA && childReg->getKind() == TR_GPR)
      {
      childFloatReg = (isFloat) ? moveToFloatRegister(node, childReg, cg) : moveToDoubleRegister(node, childReg, cg);
      }
   else
      {
      childFloatReg = childReg;
      if (!max)
         {
         childReg = (isFloat) ? moveFromFloatRegister(node, childFloatReg, cg) : moveFromDoubleRegister(node, childFloatReg, cg);
         }
      }

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg->trMemory());
   deps->addPostCondition(trgFloatReg, TR::RealRegister::NoReg);

   // Check for NaN by comparing itself and leave
   generateSrc2Instruction(cg, (isFloat) ? ARMOp_fcmps : ARMOp_fcmpd, node, trgFloatReg, trgFloatReg);
   generateInstruction(cg, ARMOp_fmstat, node);
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeVS, doneLabel, deps);

   // Check for NaN
   cursor = generateSrc2Instruction(cg, (isFloat) ? ARMOp_fcmps : ARMOp_fcmpd, node, childFloatReg, childFloatReg);
   cursor = generateInstruction(cg, ARMOp_fmstat, node, cursor);
   cursor = generateTrg1Src1Instruction(cg, (isFloat) ? ARMOp_fcpys : ARMOp_fcpyd, node, trgFloatReg, childFloatReg, cursor);
   cursor->setConditionCode(ARMConditionCodeVS);
   cursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeVS, doneLabel, deps, cursor);

   // TODO: Special case firstChild is -0.0, secondChild is 0.0, compare it in general register
   if (isFloat)
      {
      cursor = generateSrc1ImmInstruction(cg, ARMOp_cmp, node, max ? trgReg : childReg, 0x80, 24, cursor); // 0x80000000
      }
   else
      {
      cursor = generateSrc1ImmInstruction(cg, ARMOp_cmp, node, max ? trgReg->getHighOrder() : childReg->getHighOrder(), 0x80, 24, cursor); // 0x80000000
      cursor = generateSrc1ImmInstruction(cg, ARMOp_cmp, node, max ? trgReg->getLowOrder() : childReg->getLowOrder(), 0, 0, cursor);
      cursor->setConditionCode(ARMConditionCodeEQ);
      }

   /*
    * ;max:                                       ;min:
    * ;compare child with zero if target is -0.0  ;compare target with zero if child is -0.0
    * fcmpzdeq child                              fcmpzdeq target
    * fmstateq                                    fmstateq
    * ;if child is zero, then return child (0.0). ;if target is zero, then return child (-0.0).
    * fcpydeq  target child                       fcpydeq  target child
    * ;if child is not zero, return greater one.  ;if target is not zero, return lesser one.
    * fcmpdne  child  target                      fcmpdne  child  target
    * fmstatne                                    fmstatne
    * fcpydhi  target child                       fcpydlt  target child
    *
    */
   cursor = generateSrc1ImmInstruction(cg, (isFloat) ? ARMOp_fcmpzs : ARMOp_fcmpzd, node, max ? childFloatReg : trgFloatReg, 0, 0, cursor);  // Dummy 0
   cursor->setConditionCode(ARMConditionCodeEQ);
   cursor = generateInstruction(cg, ARMOp_fmstat, node, cursor);
   cursor->setConditionCode(ARMConditionCodeEQ);
   cursor = generateTrg1Src1Instruction(cg, (isFloat) ? ARMOp_fcpys : ARMOp_fcpyd, node, trgFloatReg, childFloatReg, cursor);
   cursor->setConditionCode(ARMConditionCodeEQ);
   cursor = generateSrc2Instruction(cg, (isFloat) ? ARMOp_fcmps : ARMOp_fcmpd, node, childFloatReg, trgFloatReg, cursor);
   cursor->setConditionCode(ARMConditionCodeNE);
   // Move the FPSCR flags back to the APSR
   cursor = generateInstruction(cg, ARMOp_fmstat, node, cursor);
   cursor->setConditionCode(ARMConditionCodeNE);
   cursor = generateTrg1Src1Instruction(cg, (isFloat) ? ARMOp_fcpys : ARMOp_fcpyd, node, trgFloatReg, childFloatReg, cursor);
   cursor->setConditionCode(max ? ARMConditionCodeHI: ARMConditionCodeLT);

   // doneLabel
   cursor = generateLabelInstruction(cg, ARMOp_label, node, doneLabel, deps, cursor);


   if (noFPRA && reg->getKind() == TR_GPR)
      {
      if (cg->canClobberNodesRegister(firstChild))
         {
         if (isFloat)
            {
            cursor = generateTrg1Src1Instruction(cg, ARMOp_fmrs, node, reg, trgFloatReg, cursor);
            }
         else
            {
            cursor = generateTrg2Src1Instruction(cg, ARMOp_fmrrd, node, reg->getLowOrder(), reg->getHighOrder(), trgFloatReg, cursor);
            }
         trgReg = reg;
         }
      else
         {
         trgReg = (isFloat) ? moveFromFloatRegister(node, trgFloatReg, cg) : moveFromDoubleRegister(node, trgFloatReg, cg);
         cg->stopUsingRegister(trgFloatReg);
         }
      }
   else
      {
      trgReg = trgFloatReg;
      }

   node->setRegister(trgReg);
   for (int i = 0; i < 2; i++) // n = 2
      {
      cg->decReferenceCount(node->getChild(i));
      }
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return generateFloatMaxMin(node, cg, true);
   }

TR::Register *OMR::ARM::TreeEvaluator::fminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return generateFloatMaxMin(node, cg, false);
   }

#else
TR::Register *OMR::ARM::TreeEvaluator::ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = cg->allocateRegister();

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, target, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      target = cg->evaluate(child);
      cg->decReferenceCount(child);
   }
   node->setRegister(target);
   return target;
   }

TR::Register *OMR::ARM::TreeEvaluator::fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = cg->allocateRegister();

   TR_ASSERT(!node->normalizeNanValues(), "Check NAN\n");
   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, target, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      target = cg->evaluate(child);
      cg->decReferenceCount(child);
   }
   node->setRegister(target);
   return target;
   }

TR::Register *OMR::ARM::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::RegisterPair        *pairTarget = NULL;
   TR::Register            *target = NULL;
   TR::Register            *lowReg  = NULL;
   TR::Register            *highReg = NULL;
   TR::Compilation *comp = cg->comp();

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *highMem, *lowMem;
      TR::MemoryReference  *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 8, cg);
      lowReg  = cg->allocateRegister();
      highReg = cg->allocateRegister();
      highMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 0 : 4, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, highReg, highMem);
      lowMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 4 : 0, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, lowReg, lowMem);

      highMem->decNodeReferenceCounts();
      lowMem->decNodeReferenceCounts();
      pairTarget = cg->allocateRegisterPair(lowReg, highReg);
      node->setRegister(pairTarget);
      return pairTarget;
      }
   else
      {
      target = cg->evaluate(child);
      cg->decReferenceCount(child);
      node->setRegister(target);
      return target;
      }
   }

TR::Register *OMR::ARM::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::RegisterPair        *pairTarget = NULL;
   TR::Register            *target = NULL;
   TR::Register            *lowReg  = NULL;
   TR::Register            *highReg = NULL;
   TR::Compilation *comp = cg->comp();

   TR_ASSERT(!node->normalizeNanValues(), "Check NAN\n");
   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *highMem, *lowMem;
      TR::MemoryReference  *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 8, cg);
      lowReg  = cg->allocateRegister();
      highReg = cg->allocateRegister();
      highMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 0 : 4, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, highReg, highMem);
      lowMem = new (cg->trHeapMemory()) TR::MemoryReference(*tempMR, (TR::Compiler->target.cpu.isBigEndian()) ? 4 : 0, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, lowReg, lowMem);

      highMem->decNodeReferenceCounts();
      lowMem->decNodeReferenceCounts();
      pairTarget = cg->allocateRegisterPair(lowReg, highReg);
      node->setRegister(pairTarget);
      return pairTarget;
      }
   else
      {
      target = cg->evaluate(child);
      cg->decReferenceCount(child);
      node->setRegister(target);
      return target;
      }
   }

TR::Register *OMR::ARM::TreeEvaluator::fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::floadi
TR::Register *OMR::ARM::TreeEvaluator::floadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::dloadi
TR::Register *OMR::ARM::TreeEvaluator::dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ifstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::idstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::faddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::daddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// f2i handled by d2iEvaluator
// f2s handled by d2sEvaluator
// f2c handled by d2cEvaluator
// f2b handled by d2bEvaluator
// f2l handled by d2lEvaluator

TR::Register *OMR::ARM::TreeEvaluator::f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles f2c
TR::Register *OMR::ARM::TreeEvaluator::d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles f2s
TR::Register *OMR::ARM::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles f2b
TR::Register *OMR::ARM::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles s2f
TR::Register *OMR::ARM::TreeEvaluator::s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles c2f
TR::Register *OMR::ARM::TreeEvaluator::su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }
// also handles TR::iffcmpeq
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpeq
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpeq
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpeq
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpgt
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmple
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpequ
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpltu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpgeu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMOp_bge is really not less than which is equivalent to greater than equal to or unordered
   return NULL;
   }

// also handles TR::iffcmpgtu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::iffcmpleu
TR::Register *OMR::ARM::TreeEvaluator::ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // ARMOp_ble is really not greater than which is equivalent to less than equal to or unordered
   return NULL;
   }

// also handles TR::fcmpeq
TR::Register *OMR::ARM::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::fcmpne
TR::Register *OMR::ARM::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::fcmplt
TR::Register *OMR::ARM::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

// also handles TR::fcmpge
TR::Register *OMR::ARM::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

#endif
