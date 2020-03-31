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

#include "codegen/ARM64ConditionCode.hpp"
#include "codegen/ARM64HelperCallSnippet.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg);

TR::Register *
genericReturnEvaluator(TR::Node *node, TR::RealRegister::RegNum rnum, TR_RegisterKinds rk, TR_ReturnInfo i,  TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *returnRegister = cg->evaluate(firstChild);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 0, cg->trMemory());
   deps->addPreCondition(returnRegister, rnum);
   generateAdminInstruction(cg, TR::InstOpCode::retn, node, deps);

   cg->comp()->setReturnInfo(i);
   cg->decReferenceCount(firstChild);

   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getIntegerReturnRegister(), TR_GPR, TR_IntReturn, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getLongReturnRegister(), TR_GPR, TR_LongReturn, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::areturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return genericReturnEvaluator(node, cg->getProperties().getLongReturnRegister(), TR_GPR, TR_ObjectReturn, cg);
   }

// void return
TR::Register *
OMR::ARM64::TreeEvaluator::returnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, TR::InstOpCode::retn, node);
   cg->comp()->setReturnInfo(TR_VoidReturn);
   return NULL;
   }

TR::Register *OMR::ARM64::TreeEvaluator::gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::LabelSymbol *gotoLabel = node->getBranchDestination()->getNode()->getLabel();
   if (node->getNumChildren() > 0)
      {
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, gotoLabel,
            generateRegisterDependencyConditions(cg, child, 0));
      cg->decReferenceCount(child);
      }
   else
      {
      generateLabelInstruction(cg, TR::InstOpCode::b, node, gotoLabel);
      }
   return NULL;
   }

static TR::Instruction *ificmpHelper(TR::Node *node, TR::ARM64ConditionCode cc, bool is64bit, TR::CodeGenerator *cg)
   {
   if (virtualGuardHelper(node, cg))
      return NULL;

   TR::Compilation *comp = cg->comp();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = NULL;
   TR::Register *src1Reg = cg->evaluate(firstChild);
   bool useRegCompare = true;

#ifdef J9_PROJECT_SPECIFIC
if (cg->profiledPointersRequireRelocation() && secondChild->getOpCodeValue() == TR::aconst &&
   (secondChild->isClassPointerConstant() || secondChild->isMethodPointerConstant()))
   {
   if (node->isProfiledGuard())
      {
      TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
      TR_AOTGuardSite *site = comp->addAOTNOPSite();
      site->setType(TR_ProfiledGuard);
      site->setGuard(virtualGuard);
      site->setNode(node);
      site->setAconstNode(secondChild);
      }
   else
      {
      TR_ASSERT(!(node->isNopableInlineGuard()),"Should not evaluate class or method pointer constants underneath NOPable guards as they are runtime assumptions handled by virtualGuardHelper");
      cg->evaluate(secondChild);
      }
   }
#endif

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int64_t value = is64bit ? secondChild->getLongInt() : secondChild->getInt();
      if (constantIsUnsignedImm12(value))
         {
         generateCompareImmInstruction(cg, node, src1Reg, value, is64bit);
         useRegCompare = false;
         }
      }

   if (useRegCompare)
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      generateCompareInstruction(cg, node, src1Reg, src2Reg, is64bit);
      }

   TR::LabelSymbol *dstLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::Instruction *result;
   if (node->getNumChildren() == 3)
      {
      thirdChild = node->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "The third child of a compare must be a TR::GlRegDeps");

      cg->evaluate(thirdChild);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc, deps);
      }
   else
      {
      result = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, dstLabel, cc);
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
OMR::ARM64::TreeEvaluator::ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_EQ, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_NE, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LT, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_GE, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_GT, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LE, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_CC, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_CS, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_HI, false, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LS, false, cg);
   return NULL;
   }

// also handles iflucmpeq, ifacmpeq
TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_EQ, true, cg);
   return NULL;
   }

// also handles ifacmpne
TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_NE, true, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LT, true, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_GE, true, cg);
   return NULL;
   }

// also handles ifacmplt
TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_CC, true, cg);
   return NULL;
   }

// also handles ifacmpge
TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_CS, true, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_GT, true, cg);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LE, true, cg);
   return NULL;
   }

// also handles ifacmpgt
TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_HI, true, cg);
   return NULL;
   }

// also handles ifacmple
TR::Register *
OMR::ARM64::TreeEvaluator::iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ificmpHelper(node, TR::CC_LS, true, cg);
   return NULL;
   }

static TR::Register *icmpHelper(TR::Node *node, TR::ARM64ConditionCode cc, bool is64bit, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   bool useRegCompare = true;

   if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
      {
      int64_t value = is64bit ? secondChild->getLongInt() : secondChild->getInt();
      if (constantIsUnsignedImm12(value))
         {
         generateCompareImmInstruction(cg, node, src1Reg, value, is64bit);
         useRegCompare = false;
         }
      }

   if (useRegCompare)
      {
      TR::Register *src2Reg = cg->evaluate(secondChild);
      generateCompareInstruction(cg, node, src1Reg, src2Reg, is64bit);
      }

   generateCSetInstruction(cg, node, trgReg, cc);

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_EQ, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_NE, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LT, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LE, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_GE, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_GT, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_CC, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LS, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_CS, false, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_HI, false, cg);
   }

// also handles lucmpeq, acmpeq
TR::Register *
OMR::ARM64::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_EQ, true, cg);
   }

// also handles lucmpne, acmpne
TR::Register *
OMR::ARM64::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_NE, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LT, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_GE, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_GT, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LE, true, cg);
   }

// also handles acmplt
TR::Register *
OMR::ARM64::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_CC, true, cg);
   }

// also handles acmpge
TR::Register *
OMR::ARM64::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_CS, true, cg);
   }

// also handles acmpgt
TR::Register *
OMR::ARM64::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_HI, true, cg);
   }

// also handles acmple
TR::Register *
OMR::ARM64::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, TR::CC_LS, true, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *tmpReg = cg->allocateRegister();

   generateCompareInstruction(cg, node, src1Reg, src2Reg, true);
   generateCSetInstruction(cg, node, trgReg, TR::CC_GE);
   generateCSetInstruction(cg, node, tmpReg, TR::CC_LE);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::subw, node, trgReg, trgReg, tmpReg);

   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::acmpeqEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static uint32_t sumOf2ConsecutivePowersOf2(uint32_t numberOfCases)
   {
   for (uint32_t i = 3; i < 0xc0000000; i <<= 1)
      {
      if (i == numberOfCases)
         {
         return ((i & (i - 1)) >> 1) + 1;
         }
      }
   return 0;
   }

static void binarySearchCaseSpace(TR::CodeGenerator *cg, TR::Register *selectorReg, TR::Node *lookupNode, uint32_t lowChild, uint32_t highChild, bool &evaluateDefaultGlRegDeps)
   {
   uint32_t numCases = highChild - lowChild + 1;
   uint32_t pivot;
   if (0 == (pivot = sumOf2ConsecutivePowersOf2(numCases)))
      {
      pivot = lowChild + (numCases / 2) - 1;
      }
   else
      {
      pivot += lowChild - 1;
      }

   if (pivot >= lowChild)
      {
      int32_t pivotValue = lookupNode->getChild(pivot)->getCaseConstant();
      generateCompareImmInstruction(cg, lookupNode, selectorReg, pivotValue);
      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *pivotLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      pivotLabel->setEndInternalControlFlow();
      generateLabelInstruction(cg, TR::InstOpCode::label, lookupNode, startLabel);

      // We are guaranteed that the case children are sorted.
      // Image the Z32 number lines
      //                [------------------------|--------------------------)                                                     signed
      //   TR::getMinSigned<TR::Int32>()         0            TR::getMaxSigned<TR::Int32>()      TR::getMaxUnsigned<TR::Int32>()
      //                                         [--------------------------|------------------------------------)                unsigned
      // If all the cases of the lookup are on the same 'half' of the number lines, it doesn't matter
      // if we use JA or JG, so lets use JG.
      // For the signed case, we better use TR::CC_GT if the numbers straddle the 0 boundary; we already do.
      // For the unsigned case, we better use TR::CC_CS if the numbers straddle the TR::getMaxSigned<TR::Int32>() boundary. If that's
      // the case, the highVal will be less than (in the signed sense) than lowVal.
      //
      bool isUnsigned;
      int32_t lowVal  = lookupNode->getChild(lowChild)->getCaseConstant();
      int32_t highVal = lookupNode->getChild(highChild)->getCaseConstant();
      if (highVal < lowVal)
         {
         isUnsigned = true;
         }
      else
         {
         isUnsigned = false;
         }

      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, lookupNode, pivotLabel, isUnsigned ? TR::CC_CS : TR::CC_GT);

      if (lowChild == pivot)
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, lookupNode, lookupNode->getChild(lowChild)->getBranchDestination()->getNode()->getLabel(), TR::CC_EQ);
         generateLabelInstruction(cg, TR::InstOpCode::label, lookupNode, lookupNode->getChild(1)->getBranchDestination()->getNode()->getLabel());
         evaluateDefaultGlRegDeps = false;
         }
      else
         {
         binarySearchCaseSpace(cg, selectorReg, lookupNode, lowChild, pivot, evaluateDefaultGlRegDeps);
         }
      generateLabelInstruction(cg, TR::InstOpCode::label, lookupNode, pivotLabel);
      }
   else
      {
      TR_ASSERT(pivot == lowChild - 1 && lowChild == highChild, "unexpected pivot value in binarySearchCaseSpace");
      }

   if (highChild == pivot + 1)
      {
      int32_t  highValue = lookupNode->getChild(highChild)->getCaseConstant();
      generateCompareImmInstruction(cg, lookupNode, selectorReg, highValue);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, lookupNode, lookupNode->getChild(highChild)->getBranchDestination()->getNode()->getLabel(), TR::CC_EQ);
      generateLabelInstruction(cg, TR::InstOpCode::label, lookupNode, lookupNode->getChild(1)->getBranchDestination()->getNode()->getLabel());
      evaluateDefaultGlRegDeps = false;
      }
   else
      {
      binarySearchCaseSpace(cg, selectorReg, lookupNode, pivot + 1, highChild, evaluateDefaultGlRegDeps);
      }
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *selectorReg = cg->evaluate(node->getFirstChild());
   bool evaluateDefaultGlRegDeps = true;
   TR::RealRegister::RegNum depsRegisterIndex = TR::RealRegister::NoReg;
   bool selectorRegInGlRegDeps = false;
   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());

   startLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();
   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);

   for (int i = 1; i < node->getNumChildren(); i++)
      {
      TR::Node *caseChild = node->getChild(i);
      if (0 < caseChild->getNumChildren())
         {
         TR::Node *firstCaseChild = caseChild->getFirstChild();
         if (firstCaseChild->getOpCodeValue() == TR::GlRegDeps)
            {
            for (int j = firstCaseChild->getNumChildren() - 1; j >= 0; j--)
               {
               TR::Node *child = firstCaseChild->getChild(j);
               TR::Register *globalReg = NULL;

               if (child->getOpCodeValue() == TR::PassThrough)
                  {
                  globalReg = child->getFirstChild()->getRegister();
                  }
               else
                  {
                  globalReg = child->getRegister();
                  }

               TR_GlobalRegisterNumber globalRegNum = child->getGlobalRegisterNumber();
               TR_GlobalRegisterNumber highGlobalRegNum = child->getHighGlobalRegisterNumber();

               if (globalReg->getKind() == TR_GPR
                     && 0 > highGlobalRegNum
                     && (globalReg == selectorReg))
                  {
                  depsRegisterIndex = (TR::RealRegister::RegNum) cg->getGlobalRegister(globalRegNum);
                  selectorRegInGlRegDeps = true;
                  }
               else if (globalReg->getKind() == TR_GPR
                        || globalReg->getKind() == TR_FPR)
                  {
                  TR::RegisterPair *globalRegPair = globalReg->getRegisterPair();
                  TR::RealRegister::RegNum registerIndex = (TR::RealRegister::RegNum) cg->getGlobalRegister(globalRegNum);
                  if (!deps->searchPostConditionRegister(registerIndex))
                     {
                     deps->addPostCondition(globalRegPair ? globalRegPair->getLowOrder() : globalReg, registerIndex);
                     }
                  if (0 <= highGlobalRegNum)
                     {
                     TR::RealRegister::RegNum highRegisterIndex = (TR::RealRegister::RegNum) cg->getGlobalRegister(highGlobalRegNum);
                     if (!deps->searchPostConditionRegister(highRegisterIndex))
                        {
                        deps->addPostCondition(globalRegPair->getHighOrder(), highRegisterIndex);
                        }
                     }
                  }
               }
            }
         }
      }

   binarySearchCaseSpace(cg, selectorReg, node, 2, node->getNumChildren() - 1, evaluateDefaultGlRegDeps);
   cg->decReferenceCount(node->getFirstChild());

   deps->addPostCondition(selectorReg, depsRegisterIndex);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, deps);
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::tableEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int32_t numBranchTableEntries = node->getNumChildren() - 2;
   TR::Node *defaultChild = node->getSecondChild();
   TR::Register *selectorReg = cg->evaluate(node->getFirstChild());
   TR::Register *tmpRegister = NULL;
   TR::RegisterDependencyConditions *conditions;
   int32_t i;

   if (5 <= numBranchTableEntries)
      {
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
      tmpRegister = cg->allocateRegister();
      TR::addDependency(conditions, tmpRegister, TR::RealRegister::NoReg, TR_GPR, cg);
      }
   else
      {
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
      }

   TR::addDependency(conditions, selectorReg, TR::RealRegister::NoReg, TR_GPR, cg);

   if (0 < defaultChild->getNumChildren())
      {
      cg->evaluate(defaultChild->getFirstChild());
      conditions = conditions->clone(cg, generateRegisterDependencyConditions(cg, defaultChild->getFirstChild(), 0));
      }

   if (5 > numBranchTableEntries)
      {
      for (i = 0; i < numBranchTableEntries; i++)
         {
         generateCompareImmInstruction(cg, node, selectorReg, i);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, node->getChild(2+i)->getBranchDestination()->getNode()->getLabel(), TR::CC_EQ);
         }

      generateLabelInstruction(cg, TR::InstOpCode::b, node, defaultChild->getBranchDestination()->getNode()->getLabel(), conditions);
      }
   else
      {
      if (!constantIsUnsignedImm12(numBranchTableEntries))
         {
         loadConstant32(cg, node, numBranchTableEntries, tmpRegister);
         generateCompareInstruction(cg, node, selectorReg, tmpRegister);
         }
      else
         {
         generateCompareImmInstruction(cg, node, selectorReg, numBranchTableEntries);
         }

      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, defaultChild->getBranchDestination()->getNode()->getLabel(), TR::CC_CS);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::adr, node, tmpRegister, 12); // distance between this instruction to the jump table
      generateTrg1Src2ShiftedInstruction(cg, TR::InstOpCode::addx, node, tmpRegister, tmpRegister, selectorReg, TR::SH_LSL, 2);
      generateRegBranchInstruction(cg, TR::InstOpCode::br, node, tmpRegister);

      for (i = 2; i < node->getNumChildren()-1; i++)
         {
         generateLabelInstruction(cg, TR::InstOpCode::b, node, node->getChild(i)->getBranchDestination()->getNode()->getLabel());
         }
      generateLabelInstruction(cg, TR::InstOpCode::b, node, node->getChild(i)->getBranchDestination()->getNode()->getLabel(), conditions);
      }

   if (NULL != tmpRegister)
      cg->stopUsingRegister(tmpRegister);

   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ZEROCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::DIVCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::BNDCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ArrayStoreCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::ArrayCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

static TR::Register *
commonMinMaxEvaluator(TR::Node *node, bool is64bit, TR::ARM64ConditionCode cc, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Register *trgReg;

   if (cg->canClobberNodesRegister(firstChild))
      {
      trgReg = src1Reg; // use the first child as the target
      }
   else
      {
      trgReg = cg->allocateRegister();
      }

   TR_ASSERT(node->getNumChildren() == 2, "The number of children for imax/imin/lmax/lmin must be 2.");

   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   // ToDo:
   // Optimize the code by using generateCompareImmInstruction() when possible
   generateCompareInstruction(cg, node, src1Reg, src2Reg, is64bit);

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::cselx : TR::InstOpCode::cselw;
   generateCondTrg1Src2Instruction(cg, op, node, trgReg, src1Reg, src2Reg, cc);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return trgReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::imaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, false, TR::CC_GT, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, true, TR::CC_GT, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::iminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, false, TR::CC_LT, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::lminEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonMinMaxEvaluator(node, true, TR::CC_LT, cg);
   }

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (!cg->willGenerateNOPForVirtualGuard(node))
      {
      return false;
      }

   TR::Compilation *comp = cg->comp();
   TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);

   TR_VirtualGuardSite *site = NULL;

   if (cg->comp()->compileRelocatableCode())
      {
      site = (TR_VirtualGuardSite *)comp->addAOTNOPSite();
      TR_AOTGuardSite *aotSite = (TR_AOTGuardSite *)site;
      aotSite->setType(virtualGuard->getKind());
      aotSite->setNode(node);

      switch (virtualGuard->getKind())
         {
         case TR_DirectMethodGuard:
         case TR_NonoverriddenGuard:
         case TR_InterfaceGuard:
         case TR_MethodEnterExitGuard:
         case TR_HCRGuard:
         case TR_AbstractGuard:
            aotSite->setGuard(virtualGuard);
            break;

         case TR_ProfiledGuard:
            break;

         default:
            TR_ASSERT(0, "got AOT guard in node but virtual guard not one of known guards supported for AOT. Guard: %d", virtualGuard->getKind());
            break;
         }
      }
   else if (!node->isSideEffectGuard())
      {
      site = virtualGuard->addNOPSite();
      }
   else
      site = comp->addSideEffectNOPSite();

   TR::RegisterDependencyConditions *deps;
   if (node->getNumChildren() == 3)
      {
      TR::Node *third = node->getChild(2);
      cg->evaluate(third);
      deps = generateRegisterDependencyConditions(cg, third, 0);
      }
   else
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 0, cg->trMemory());

   if(virtualGuard->shouldGenerateChildrenCode())
      cg->evaluateChildrenWithMultipleRefCount(node);

   TR::LabelSymbol *label = node->getBranchDestination()->getNode()->getLabel();
   generateVirtualGuardNOPInstruction(cg, node, site, deps, label);
   cg->recursivelyDecReferenceCount(node->getFirstChild());
   cg->recursivelyDecReferenceCount(node->getSecondChild());

   return true;
#else
   return false;
#endif
   }

TR::Register *OMR::ARM64::TreeEvaluator::igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *labelAddr = node->getFirstChild();
   TR::Register *addrReg = cg->evaluate(labelAddr);
   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() > 1)
      {
      TR_ASSERT(node->getNumChildren() == 2 && node->getChild(1)->getOpCodeValue() == TR::GlRegDeps, "igoto has maximum of two children and second one must be global register dependency");
      TR::Node *glregdep = node->getChild(1);
      cg->evaluate(glregdep);
      deps = generateRegisterDependencyConditions(cg, glregdep, 0);
      cg->decReferenceCount(glregdep);
      }
   if (deps)
      generateRegBranchInstruction(cg, TR::InstOpCode::br, node, addrReg, deps);
   else
      generateRegBranchInstruction(cg, TR::InstOpCode::br, node, addrReg);
   cg->decReferenceCount(labelAddr);
   node->setRegister(NULL);
   return NULL;
   }
