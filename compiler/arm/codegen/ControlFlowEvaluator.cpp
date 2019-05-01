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
#include "arm/codegen/ARMOperand2.hpp"
#include "codegen/AheadOfTimeCompile.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/ARMAOTRelocation.hpp"
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CompilerEnv.hpp"
#include "infra/Bit.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#endif

static void lookupScheme1(TR::CodeGenerator *cg, TR::Node *node, bool unbalanced);
static void lookupScheme2(TR::CodeGenerator *cg, TR::Node *node, bool unbalanced);
static void lookupScheme3(TR::CodeGenerator *cg, TR::Node *node, bool unbalanced);
static void lookupScheme4(TR::CodeGenerator *cg, TR::Node *node);
static bool isGlDepsUnBalanced(TR::CodeGenerator *cg, TR::Node *node);
static void switchDispatch(TR::CodeGenerator *cg, TR::Node *node);
static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg);

TR::Register *OMR::ARM::TreeEvaluator::gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::LabelSymbol *gotoLabel = node->getBranchDestination()->getNode()->getLabel();
   if (node->getNumChildren() > 0)
      {
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      generateLabelInstruction(cg, ARMOp_b, node, gotoLabel, generateRegisterDependencyConditions(cg, child, 0));
      child->decReferenceCount();
      }
   else
      {
      generateLabelInstruction(cg, ARMOp_b, node, gotoLabel);
      }
   return NULL;
   }

// also handles areturn
TR::Register *OMR::ARM::TreeEvaluator::ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(deps, returnRegister, cg->getProperties().getIntegerReturnRegister(), TR_GPR, cg);

   generateAdminInstruction(cg, ARMOp_ret, node, deps);
   cg->comp()->setReturnInfo(TR_IntReturn);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *returnRegister                      = cg->evaluate(node->getFirstChild());
   TR::Register *lowReg                              = returnRegister->getLowOrder();
   TR::Register *highReg                             = returnRegister->getHighOrder();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
   TR::addDependency(deps, lowReg, cg->getProperties().getLongLowReturnRegister(), TR_GPR, cg);
   TR::addDependency(deps, highReg, cg->getProperties().getLongHighReturnRegister(), TR_GPR, cg);

   generateAdminInstruction(cg, ARMOp_ret, node, deps);
   cg->comp()->setReturnInfo(TR_LongReturn);
   return NULL;
   }

// areturn handled by ireturnEvaluator

TR::Register *OMR::ARM::TreeEvaluator::returnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, ARMOp_ret, node);
   cg->comp()->setReturnInfo(TR_VoidReturn);
   return NULL;
   }

static TR::Instruction *compareIntsForOrder(TR_ARMConditionCode  branchType,
                                           TR::Node             *node,
                                           TR::CodeGenerator    *cg,
                                           TR::SymbolReference  *sr =  NULL)
   {
   TR::Node        *secondChild = node->getSecondChild();
   TR::Node        *firstChild = node->getFirstChild();
   TR::Register    *src1Reg;
   uint32_t        base, rotate;
   TR::LabelSymbol *branchTarget = sr ? NULL : node->getBranchDestination()->getNode()->getLabel();
   TR::Instruction *result;

   int32_t         value = secondChild->getInt();
   bool            cannotInline = false;

   // For TR::instanceof, the opcode must be ificmpeq/ne.
   if (!cg->comp()->getOption(TR_OptimizeForSpace) &&
        firstChild->getOpCodeValue() == TR::instanceof &&
        firstChild->getRegister() == NULL &&
        node->getReferenceCount() <=1 &&
        (value == 0 || value == 1))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::ificmpeq || node->getOpCodeValue() == TR::ificmpne,
             "Unexpected opcode");

      TR_ASSERT(secondChild->getOpCode().isLoadConst(),
             "Unexpected second child.\n");

#ifdef J9_PROJECT_SPECIFIC
      if (OMR::ARM::TreeEvaluator::VMifInstanceOfEvaluator(node, cg) == NULL)
         return NULL;
#endif

      cannotInline = true;
      TR::Node::recreate(firstChild, TR::icall);
      }

   src1Reg = cg->evaluate(firstChild);

   if (cannotInline)
      {
      TR::Node::recreate(firstChild, TR::instanceof);
      }

   bool foundConst = false;
   bool negated;
   int32_t cmpValue;
   if (secondChild->getOpCode().isLoadConst())
      {
      cmpValue = secondChild->getInt();
      negated = cmpValue < 0 && cmpValue != 0x80000000; //the negative of the maximum negative int is itself so can not use cmn here
      if(constantIsImmed8r(negated ? -cmpValue : cmpValue, &base, &rotate))
         {
         generateSrc1ImmInstruction(cg, negated ?  ARMOp_cmn : ARMOp_cmp, node, src1Reg, base, rotate);
         foundConst = true;
         }
      }
   if(!foundConst)
      generateSrc2Instruction(cg, ARMOp_cmp, node, src1Reg, cg->evaluate(secondChild));

   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);

      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps,
             "The third child of a compare must be a TR::GlRegDeps");

      cg->evaluate(thirdChild);

      // NB: must generate reg deps before the conditonal branch
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();  // IS it correct to decReferenceCount before the instruction generation ?!!!
      }

   if (!sr)
      {
      result = generateConditionalBranchInstruction(cg, node, branchType, branchTarget, deps);
      }
   else
      {
      result = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)sr->getMethodAddress(), deps, sr, NULL, NULL, branchType);
      cg->machine()->setLinkRegisterKilled(true);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return result;
   }

TR::Instruction *OMR::ARM::TreeEvaluator::compareIntsForEquality(TR_ARMConditionCode  branchType,
                                           TR::Node             *node,
                                           TR::CodeGenerator    *cg,
                                           TR::SymbolReference  *sr)
   {
   if (virtualGuardHelper(node, cg))
      return NULL;

   TR::Compilation *comp = cg->comp();
   TR::Node        *secondChild = node->getSecondChild();
   TR::Node        *firstChild = node->getFirstChild();

#ifdef J9_PROJECT_SPECIFIC
   if (cg->profiledPointersRequireRelocation() && node->isProfiledGuard() && secondChild->getOpCodeValue() == TR::aconst &&
       (secondChild->isClassPointerConstant() || secondChild->isMethodPointerConstant()))
      {
      TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
      TR_AOTGuardSite *site = comp->addAOTNOPSite();
      site->setType(TR_ProfiledGuard);
      site->setGuard(virtualGuard);
      site->setNode(node);
      site->setAconstNode(secondChild);
      }
#endif

   TR::Register    *src1Reg;
   uint32_t        base, rotate;
   TR::LabelSymbol *branchTarget = sr ? NULL : node->getBranchDestination()->getNode()->getLabel();
   TR::Instruction *result;

   int32_t         value = secondChild->getInt();
   bool            cannotInline = false;

   // For TR::instanceof, the opcode must be ificmpeq/ne.
   if (!cg->comp()->getOption(TR_OptimizeForSpace) &&
        firstChild->getOpCodeValue() == TR::instanceof &&
        firstChild->getRegister() == NULL &&
        node->getReferenceCount() <=1 &&
        (value == 0 || value == 1))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::ificmpeq || node->getOpCodeValue() == TR::ificmpne,
             "Unexpected opcode");

      TR_ASSERT(secondChild->getOpCode().isLoadConst(),
             "Unexpected second child.\n");

#ifdef J9_PROJECT_SPECIFIC
      if (OMR::ARM::TreeEvaluator::VMifInstanceOfEvaluator(node, cg) == NULL)
         return NULL;
#endif

      cannotInline = true;
      TR::Node::recreate(firstChild, TR::icall);
      }

   src1Reg = cg->evaluate(firstChild);

   if (cannotInline)
      {
      TR::Node::recreate(firstChild, TR::instanceof);
      }

   bool foundConst = false;
   bool negated;
   int32_t cmpValue;
   if (secondChild->getOpCode().isLoadConst())
      {
      cmpValue = secondChild->getInt();
      negated = cmpValue < 0 && cmpValue != 0x80000000; //the negative of the maximum negative int is itself so can not use cmn here
      if(constantIsImmed8r(negated ? -cmpValue : cmpValue, &base, &rotate))
         {
         generateSrc1ImmInstruction(cg, negated ?  ARMOp_cmn : ARMOp_cmp, node, src1Reg, base, rotate);
         foundConst = true;
         }
      }
   if(!foundConst)
      generateSrc2Instruction(cg, ARMOp_cmp, node, src1Reg, cg->evaluate(secondChild));

   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);

      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps,
             "The third child of a compare must be a TR::GlRegDeps");

      cg->evaluate(thirdChild);

      // NB: must generate reg deps before the conditonal branch
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();  // IS it correct to decReferenceCount before the instruction generation ?!!!
      }

   if (!sr)
      {
      result = generateConditionalBranchInstruction(cg, node, branchType, branchTarget, deps);
      }
   else
      {
      result = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)sr->getMethodAddress(), deps, sr, NULL, NULL, branchType);
      cg->machine()->setLinkRegisterKilled(true);
      }

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return result;
   }

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::Compilation *comp = cg->comp();
   if ((!node->isNopableInlineGuard() && !node->isHCRGuard() && !node->isBreakpointGuard()) ||
       !cg->getSupportsVirtualGuardNOPing())
      return false;

   TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
   if (!((comp->performVirtualGuardNOPing() || node->isHCRGuard()) &&
         comp->isVirtualGuardNOPingRequired(virtualGuard)) &&
       virtualGuard->canBeRemoved())
      return false;

   if (   node->getOpCodeValue() != TR::ificmpne
       && node->getOpCodeValue() != TR::iflcmpne
       && node->getOpCodeValue() != TR::ifacmpne)
      {
      //TR_ASSERT(0, "virtualGuradHelper: not expecting reversed comparison");
      return false;
      }

   TR_VirtualGuardSite *site = NULL;

   if (comp->compileRelocatableCode())
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
      TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
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

// also handles ificmpne ifacmpeq ifacmpne
// for ifacmpeq, opcode has been temporarily set to ificmpeq
TR::Register *OMR::ARM::TreeEvaluator::ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForEquality(TR::ificmpeq == node->getOpCodeValue() ? ARMConditionCodeEQ : ARMConditionCodeNE, node, cg);
   return NULL;
   }

// ificmpne handled by ificmpeqEvaluator

TR::Register *OMR::ARM::TreeEvaluator::ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeLT, node, cg);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeGE, node, cg);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeGT, node, cg);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeLE, node, cg);
   return NULL;
   }


TR::Register *OMR::ARM::TreeEvaluator::ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeCC, node, cg);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeCS, node, cg);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeHI, node, cg);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   compareIntsForOrder(ARMConditionCodeLS, node, cg);
   return NULL;
   }


TR::Register *OMR::ARM::TreeEvaluator::iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild       = node->getFirstChild();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::Register    *src1Reg          = cg->evaluate(firstChild);
   TR::Register    *src2Reg          = cg->evaluate(secondChild);
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   TR_ARMConditionCode condition = node->getOpCodeValue() == TR::iflcmpeq ? ARMConditionCodeEQ : ARMConditionCodeNE;
   TR::Instruction *cursor;

   // for eq, generate:
   // cmp   hi1, hi2
   // cmpeq lo1, lo2
   // beq <label>

   // for ne, generate:
   // cmp   hi1, hi2
   // cmpeq lo1, lo2       <- also eq because if the first compare
   //                         failed, we're done
   // bne <label>

   cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, src1Reg->getHighOrder(), src2Reg->getHighOrder());
   cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, src1Reg->getLowOrder(), src2Reg->getLowOrder());
   cursor->setConditionCode(ARMConditionCodeEQ);  // make the second instruction conditional
   cursor = generateConditionalBranchInstruction(cg, node, condition, destinationLabel, deps);

   return NULL;
   }

static TR::Register *compareLongAndSetOrderedBoolean(TR_ARMConditionCode branchOp, TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild  = node->getFirstChild();
   TR::Node        *secondChild = node->getSecondChild();
   TR::Register    *src1Reg     = cg->evaluate(firstChild);
   TR::Register    *trgReg      = cg->allocateRegister();
   TR::Register    *tempReg     = cg->allocateRegister();

   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   // Eventually the following test should be whether there is a register allocator that
   // can handle registers being alive across basic block boundaries.
   // For now we just generate pessimistic code.
   if (true)
      {
      TR::Register   *src2Reg = cg->evaluate(secondChild);
      TR::ARMControlFlowInstruction *cfop = generateControlFlowInstruction(cg, ARMOp_setblong, node, deps);
      cfop->addTargetRegister(trgReg);
      cfop->addTargetRegister(tempReg);
      cfop->addSourceRegister(src1Reg->getLowOrder());
      cfop->addSourceRegister(src1Reg->getHighOrder());
      cfop->addSourceRegister(src2Reg->getLowOrder());
      cfop->addSourceRegister(src2Reg->getHighOrder());
      cfop->setConditionCode(branchOp);
      firstChild->decReferenceCount();
      secondChild->decReferenceCount();
      node->setRegister(trgReg);
      return trgReg;
      }
   }

static TR::Register *compareLongsForOrder(TR_ARMConditionCode branchOp, TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild       = node->getFirstChild();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::Register    *src1Reg          = cg->evaluate(firstChild);
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   // Eventually the following test should be whether there is a register allocator that
   // can handle registers being alive across basic block boundaries.
   // For now we just generate pessimistic code.
   static bool disableOOLForLongCompares = (feGetEnv("TR_DisableOOLForLongCompares") != NULL);
   TR::Register *src2Reg = cg->evaluate(secondChild);
   if (cg->comp()->getOption(TR_DisableOOL) || disableOOLForLongCompares)
      {
      TR::ARMControlFlowInstruction *cfop = generateControlFlowInstruction(cg, ARMOp_iflong, node, deps);
      cfop->addSourceRegister(src1Reg->getHighOrder());
      cfop->addSourceRegister(src1Reg->getLowOrder());
      cfop->addSourceRegister(src2Reg->getHighOrder());
      cfop->addSourceRegister(src2Reg->getLowOrder());
      cfop->setLabelSymbol(destinationLabel);
      cfop->setConditionCode(branchOp);
      }
   else
      {
      TR::LabelSymbol *startOOLLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *doneOOLLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *returnLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);;
      TR_ARMOutOfLineCodeSection *outlinedSlowPath = NULL;

      TR::Register *tempReg = cg->allocateRegister();
      TR_Debug *debugObj = cg->getDebug();
      int isGT = (branchOp == ARMConditionCodeGT);

      TR::Instruction *cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, src1Reg->getHighOrder(), src2Reg->getHighOrder());
      cursor = generateConditionalBranchInstruction(cg, node, isGT ? ARMConditionCodeLE : ARMConditionCodeGE, startOOLLabel, deps, cursor);
      if (debugObj)
         debugObj->addInstructionComment(cursor, "Branch to OOL iflong sequence");
      TR::Instruction *OOLBranchInstr = cursor;
      outlinedSlowPath = new (cg->trHeapMemory()) TR_ARMOutOfLineCodeSection(startOOLLabel, doneOOLLabel, cg);
      cg->getARMOutOfLineCodeSectionList().add(outlinedSlowPath);

      // Hot path
      cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, tempReg, 1, 0, cursor); // Setting tempReg to 1

      // Start OOL
      // The compare result has to be live until the end of the last instruction in the mainline, so put it as a dependency
      TR::RegisterDependencyConditions *tempDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
      TR::addDependency(tempDeps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
      //tempDeps->addPreCondition(tempReg, TR::RealRegister::NoReg);
      if (deps)
         deps = deps->clone(cg, tempDeps);
      else
         deps = tempDeps;

      // Need the two sources for compare to survive until the returnLabel.  Use a dependency to hold that, and hold
      // until the compare for them are done.
      TR::RegisterDependencyConditions *newDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
      TR::addDependency(newDeps, src1Reg->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(newDeps, src2Reg->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);

      TR::RegisterDependencyConditions *oolDeps = deps->clone(cg, newDeps);

      outlinedSlowPath->swapInstructionListsWithCompilation(); // OOL code to follow
      TR::Instruction *cursorOOL = generateLabelInstruction(cg, ARMOp_label, node, startOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursorOOL, "Denotes start of OOL iflong sequence");
      // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
      TR_ASSERT(!cursorOOL->getLiveLocals() && !cursorOOL->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
      cursorOOL->setLiveLocals(OOLBranchInstr->getLiveLocals());
      cursorOOL->setLiveMonitors(OOLBranchInstr->getLiveMonitors());

      cursorOOL = generateTrg1ImmInstruction(cg, ARMOp_mov, node, tempReg, 0, 0, cursorOOL); // Setting tempReg to 0
      cursorOOL = generateConditionalBranchInstruction(cg, node, isGT ? ARMConditionCodeLT : ARMConditionCodeGT, returnLabel, cursorOOL);
      cursorOOL = generateSrc2Instruction(cg, ARMOp_cmp, node, src1Reg->getLowOrder(), src2Reg->getLowOrder(), cursorOOL);
      cursorOOL = generateTrg1ImmInstruction(cg, ARMOp_mov, node, tempReg, 1, 0, cursorOOL); // Setting tempReg to 1
      cursorOOL->setConditionCode(isGT ? ARMConditionCodeHI: ARMConditionCodeLS);
      cursorOOL = generateLabelInstruction(cg, ARMOp_label, node, returnLabel, oolDeps, cursorOOL); // Need deps 3/21 change
      cursorOOL = generateLabelInstruction(cg, ARMOp_b, node, doneOOLLabel, cursorOOL);
      if (debugObj)
         debugObj->addInstructionComment(cursorOOL, "Denotes end of OOL iflong sequence: return to mainline");
      outlinedSlowPath->swapInstructionListsWithCompilation(); // Done with OOL code

      cursor = generateLabelInstruction(cg, ARMOp_label, node, doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursor, "OOL iflong code return label");

      // Test result and branch
      cursor = generateSrc1ImmInstruction(cg, ARMOp_tst, node, tempReg, 1, 0, cursor);
      cursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, destinationLabel, deps, cursor);
      cg->stopUsingRegister(tempReg);
      }
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild       = node->getFirstChild();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::Register    *src1Reg          = cg->evaluate(firstChild);
   TR::Register    *src2Reg          = cg->evaluate(secondChild);
   TR::Register    *tempReg          = cg->allocateRegister();
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   generateTrg1Src2Instruction(cg, ARMOp_sub_r, node, tempReg, src1Reg->getLowOrder(), src2Reg->getLowOrder());
   generateTrg1Src2Instruction(cg, ARMOp_sbc_r, node, tempReg, src1Reg->getHighOrder(), src2Reg->getHighOrder());
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeLT, destinationLabel, deps);

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild       = node->getFirstChild();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::Register    *src1Reg          = cg->evaluate(firstChild);
   TR::Register    *src2Reg          = cg->evaluate(secondChild);
   TR::Register    *tempReg          = cg->allocateRegister();
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   generateTrg1Src2Instruction(cg, ARMOp_sub_r, node, tempReg, src1Reg->getLowOrder(), src2Reg->getLowOrder());
   generateTrg1Src2Instruction(cg, ARMOp_sbc_r, node, tempReg, src1Reg->getHighOrder(), src2Reg->getHighOrder());
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeGE, destinationLabel, deps);

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild       = node->getFirstChild();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::Register    *src1Reg          = cg->evaluate(firstChild);
   TR::Register    *src2Reg          = cg->evaluate(secondChild);
   TR::Register    *tempReg          = cg->allocateRegister();
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   generateTrg1Src2Instruction(cg, ARMOp_sub_r, node, tempReg, src1Reg->getLowOrder(), src2Reg->getLowOrder());
   generateTrg1Src2Instruction(cg, ARMOp_sbc_r, node, tempReg, src1Reg->getHighOrder(), src2Reg->getHighOrder());
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeCC, destinationLabel, deps);

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node        *firstChild       = node->getFirstChild();
   TR::Node        *secondChild      = node->getSecondChild();
   TR::Register    *src1Reg          = cg->evaluate(firstChild);
   TR::Register    *src2Reg          = cg->evaluate(secondChild);
   TR::Register    *tempReg          = cg->allocateRegister();
   TR::LabelSymbol *destinationLabel = node->getBranchDestination()->getNode()->getLabel();

   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   generateTrg1Src2Instruction(cg, ARMOp_sub_r, node, tempReg, src1Reg->getLowOrder(), src2Reg->getLowOrder());
   generateTrg1Src2Instruction(cg, ARMOp_sbc_r, node, tempReg, src1Reg->getHighOrder(), src2Reg->getHighOrder());
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeCS, destinationLabel, deps);

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return NULL;
   }


TR::Register *OMR::ARM::TreeEvaluator::iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongsForOrder(ARMConditionCodeGT, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongsForOrder(ARMConditionCodeLE, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongsForOrder(ARMConditionCodeHI, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongsForOrder(ARMConditionCodeLS, node, cg);
   }


TR::Register *OMR::ARM::TreeEvaluator::ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node::recreate(node, TR::ificmpeq);
   ificmpeqEvaluator(node, cg);
   TR::Node::recreate(node, TR::ifacmpeq);
   return NULL;
   }

// ifacmpne handled by ificmpeqEvaluator

static TR::Register *icmpHelper(TR::Node *node, TR_ARMConditionCode cond, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg       = cg->allocateRegister();
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *src2Reg      = cg->evaluate(secondChild);

   TR::Instruction *movInstr;

// TODO: add back immediate cases

   generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, 0, 0);
   generateSrc2Instruction(cg, ARMOp_cmp, node, src1Reg, src2Reg);
   movInstr = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, 1, 0);
   movInstr->setConditionCode(cond);  // tag previous move as conditional

   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   return trgReg;
   }

// also handles acmpeq
// for acmpeq, opcode has been temporarily set to icmpeq
TR::Register *OMR::ARM::TreeEvaluator::icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeEQ, cg);
   }

// also handles acmpneq
// for acmpneq, opcode has been temporarily set to icmpneq
TR::Register *OMR::ARM::TreeEvaluator::icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeNE, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeLT, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeLE, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeGE, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeGT, cg);
   }


TR::Register *OMR::ARM::TreeEvaluator::iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeCC, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeLS, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeCS, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return icmpHelper(node, ARMConditionCodeHI, cg);
   }


TR::Register *OMR::ARM::TreeEvaluator::lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeEQ, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeNE, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeLT, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeGE, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeGT, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeLE, node, cg);
   }




TR::Register *OMR::ARM::TreeEvaluator::lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeCC, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeCS, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeHI, node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareLongAndSetOrderedBoolean(ARMConditionCodeLS, node, cg);
   }






TR::Register *OMR::ARM::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg      = cg->allocateRegister();
   TR::Node     *firstChild  = node->getFirstChild();
   TR::Register *src1Reg     = cg->evaluate(firstChild);
   TR::Node     *secondChild = node->getSecondChild();
   TR::Node     *thirdChild;
   TR::RegisterDependencyConditions *deps = NULL;

   if (node->getNumChildren() == 3)
      {
      thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      thirdChild->decReferenceCount();
      }

   // Eventually the following test should be whether there is a register allocator that
   // can handle registers being alive across basic block boundaries.
   // For now we just generate pessimistic code.
   TR::Register   *src2Reg = cg->evaluate(secondChild);
   TR::ARMControlFlowInstruction *cfop = generateControlFlowInstruction(cg, ARMOp_lcmp, node, deps);
   cfop->addTargetRegister(trgReg);
   cfop->addSourceRegister(src1Reg->getLowOrder());
   cfop->addSourceRegister(src1Reg->getHighOrder());
   cfop->addSourceRegister(src2Reg->getLowOrder());
   cfop->addSourceRegister(src2Reg->getHighOrder());
   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node::recreate(node, TR::icmpeq);
   TR::Register *trgReg = icmpeqEvaluator(node, cg);
   TR::Node::recreate(node, TR::acmpeq);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   switchDispatch(cg, node);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

static void mapGlRegDeps(TR::Node *node, TR_BitVector *vector)
   {
   for (int i = 0; i < node->getNumChildren(); i++)
      vector->set(node->getChild(i)->getGlobalRegisterNumber());
   }

static bool isGlDepsUnBalanced(TR::CodeGenerator *cg, TR::Node *node)
   {
   int32_t  numChildren = node->getNumChildren();
   TR::Node *defaultNode = node->getSecondChild();
   TR::Node *defaultDeps = defaultNode->getNumChildren() > 0 ? defaultNode->getFirstChild() : NULL;
   int32_t  numDefDeps  = defaultDeps ? defaultDeps->getNumChildren() : 0;

   // Return true if the number of dependencies on any of the non-default
   // cases does not agree with the number of those on the default case;
   // otherwise return false if there are no dependencies to consider.
   int i;
   for (i = 2; i < numChildren; i++)
      {
      TR::Node *caseNode = node->getChild(i);
      TR::Node *caseDeps = caseNode->getNumChildren() > 0 ? caseNode->getFirstChild() : NULL;
      if ((!caseDeps && numDefDeps != 0) || (caseDeps && numDefDeps != caseDeps->getNumChildren()))
         break;
      }
   if (i != numChildren)
      {
      return true;
      }
   if (numDefDeps == 0)
      {
      return false;
      }

   // The cases all have the same number (!= 0) of dependencies; see if
   // they all describe the same set of global registers. Return true
   // if they do not; otherwise return false.
   TR_BitVector defDepMap, caseDepMap;

   defDepMap.init(cg->getNumberOfGlobalRegisters(), cg->trMemory());
   caseDepMap.init(cg->getNumberOfGlobalRegisters(), cg->trMemory());
   mapGlRegDeps(defaultDeps, &defDepMap);

   for (i = 2; i < numChildren; i++)
      {
      TR::Node *caseDeps = node->getChild(i)->getFirstChild();
      caseDepMap.empty();
      mapGlRegDeps(caseDeps, &caseDepMap);
      if (!(defDepMap == caseDepMap))
         return true;
      }

   return false;
   }

// Note: When the checks and/or lookupScheme implementations are changed with regards to number of used registers (or dependency),
// the matching GlDepRegs free checks in OMRCodeGenerator.cpp#getMaximumNumberOfGPRsAllowedAcrossEdge(..) must be updated to synch.
static void switchDispatch(TR::CodeGenerator *cg, TR::Node *node)
   {
   int32_t    total = node->getNumChildren();
   int32_t    i;
   bool       unbalanced;

   unbalanced = isGlDepsUnBalanced(cg, node);
   if (!unbalanced)
      {
      for (i = 2; i < total; i++)
         {
         if (node->getChild(i)->getNumChildren() > 0)
            {
            TR::Node *caseDepsNode = node->getChild(i)->getFirstChild();
            if (caseDepsNode != NULL)
               cg->evaluate(caseDepsNode);
            }
         }
      }

   if (total <= 15)
      {
      uint32_t base, rotate;
      for (i = 2; i < total && constantIsImmed8r(node->getChild(i)->getCaseConstant(), &base, &rotate); i++)
         ;
      if (i == total)
         {
         lookupScheme1(cg, node, unbalanced);
         return;
         }
      }

   // The children are in ascending order already.
   if (total <= 9)
      {
      int32_t preInt = node->getChild(2)->getCaseConstant();
      for (i = 3; i < total; i++)
         {
         int32_t diff = node->getChild(i)->getCaseConstant() - preInt;
         uint32_t base, rotate;
         preInt += diff;
         if (diff < 0 || !constantIsImmed8r(diff, &base, &rotate))
            break;
         }
      if (i >= total)
         {
         lookupScheme2(cg, node, unbalanced);
         return;
         }
      }
#if 0
   // any values, less than 8 elements total
   if (total <= 8)
      {
      lookupScheme3(cg, node, unbalanced);
      return;
      }

   // everything else
   lookupScheme4(cg, node, unbalanced);
#else
   // TODO: Implement lookupScheme4. For now, use lookupScheme3.
   // TODO: When changed, the matching GlDepRegs free checks in
   //       OMRCodeGeneratorExt.cpp#getMaximumNumberOfGPRsAllowedAcrossEdge(..) must be updated.
   lookupScheme3(cg, node, unbalanced);
#endif
   }

// Note: When the checks and/or lookupScheme implementations are changed with regards to number of used registers (or dependency),
// the matching GlDepRegs free checks in OMRCodeGenerator.cpp#getMaximumNumberOfGPRsAllowedAcrossEdge(..) must be updated to synch.
// Called by switchDispatch().
static void lookupScheme1(TR::CodeGenerator *cg, TR::Node *node, bool unbalanced)
   {
   TR::Register *selectorReg = cg->evaluate(node->getFirstChild());
   TR::Node     *defaultNode = node->getSecondChild();
   TR::Node     *defDepsNode = defaultNode->getNumChildren() > 0 ? defaultNode->getFirstChild() : NULL;

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(deps, selectorReg, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::RegisterDependencyConditions *defaultDeps = deps;
   if (defDepsNode && !unbalanced)
      {
      cg->evaluate(defDepsNode);
      defaultDeps = defaultDeps->clone(cg, generateRegisterDependencyConditions(cg, defDepsNode, 0));
      }

   for (int i = 2; i < node->getNumChildren(); i++)
      {
      TR::Node        *caseNode        = node->getChild(i);
      TR::LabelSymbol *caseTargetLabel = caseNode->getBranchDestination()->getNode()->getLabel();

      // we don't need to check if the constant is immed;
      // it was checked in the caller
      uint32_t base, rotate;
      constantIsImmed8r(caseNode->getCaseConstant(), &base, &rotate);
      generateSrc1ImmInstruction(cg, ARMOp_cmp, node, selectorReg, base, rotate);

      if (unbalanced)
         {
         TR::RegisterDependencyConditions *caseDeps = deps;
         if (caseNode->getNumChildren() > 0)
            {
            TR::Node *caseDepsNode = caseNode->getFirstChild();
            cg->evaluate(caseDepsNode);
            caseDeps = caseDeps->clone(cg, generateRegisterDependencyConditions(cg, caseDepsNode, 0));
            }
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, caseTargetLabel, caseDeps);
         }
      else
         {
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, caseTargetLabel);
         }
      }

   TR::LabelSymbol *defaultTargetLabel = defaultNode->getBranchDestination()->getNode()->getLabel();

   if (defDepsNode && unbalanced)
      {
      cg->evaluate(defDepsNode);
      defaultDeps = defaultDeps->clone(cg, generateRegisterDependencyConditions(cg, defDepsNode, 0));
      }

   generateLabelInstruction(cg, ARMOp_b, node, defaultTargetLabel, defaultDeps);
   }

// Note: When the checks and/or lookupScheme implementations are changed with regards to number of used registers (or dependency),
// the matching GlDepRegs free checks in OMRCodeGenerator.cpp#getMaximumNumberOfGPRsAllowedAcrossEdge(..) must be updated to synch.
// Called by switchDispatch().
static void lookupScheme2(TR::CodeGenerator *cg, TR::Node *node, bool unbalanced)
   {
   int32_t      numChildren  = node->getNumChildren();
   TR::Register *selectorReg  = cg->evaluate(node->getFirstChild());
   TR::Register *caseConstReg = cg->allocateRegister();
   TR::Node     *defaultNode  = node->getSecondChild();
   TR::Node     *defDepsNode  = defaultNode->getNumChildren() > 0 ? defaultNode->getFirstChild() : NULL;

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
   TR::addDependency(deps, selectorReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, caseConstReg, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::RegisterDependencyConditions *defaultDeps = deps;
   if (defDepsNode && !unbalanced)
      {
      cg->evaluate(defDepsNode);
      defaultDeps = defaultDeps->clone(cg, generateRegisterDependencyConditions(cg, defDepsNode, 0));
      }

   int32_t preInt = node->getChild(2)->getCaseConstant();
   armLoadConstant(node, preInt, caseConstReg, cg);
   for (int i = 2; i < numChildren; i++)
      {
      TR::Node        *caseNode        = node->getChild(i);
      TR::LabelSymbol *caseTargetLabel = caseNode->getBranchDestination()->getNode()->getLabel();
      generateSrc2Instruction(cg, ARMOp_cmp, node, selectorReg, caseConstReg);
      if (unbalanced)
         {
         TR::RegisterDependencyConditions *caseDeps = deps;
         if (caseNode->getNumChildren() > 0)
            {
            cg->evaluate(caseNode->getFirstChild());
            caseDeps = caseDeps->clone(cg, generateRegisterDependencyConditions(cg, caseNode->getFirstChild(), 0));
            }
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, caseTargetLabel,  caseDeps);
         }
      else
         {
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, caseTargetLabel);
         }

      if (i < numChildren - 1)
         {
         int32_t diff = node->getChild(i + 1)->getCaseConstant() - preInt;
         uint32_t base, rotate;
         preInt += diff;
         constantIsImmed8r(diff, &base, &rotate);
         generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, caseConstReg, caseConstReg, base, rotate);
         }
      }

   TR::LabelSymbol *defaultTargetLabel = defaultNode->getBranchDestination()->getNode()->getLabel();

   if (defaultNode->getNumChildren() > 0 && unbalanced)
      {
      cg->evaluate(defDepsNode);
      defaultDeps = defaultDeps->clone(cg, generateRegisterDependencyConditions(cg, defDepsNode, 0));
      }

   generateLabelInstruction(cg, ARMOp_b, node, defaultTargetLabel, defaultDeps);

   cg->stopUsingRegister(caseConstReg);
   }

// Note: When the checks and/or lookupScheme implementations are changed with regards to number of used registers (or dependency),
// the matching GlDepRegs free checks in OMRCodeGenerator.cpp#getMaximumNumberOfGPRsAllowedAcrossEdge(..) must be updated to synch.
// Called by switchDispatch().
static void lookupScheme3(TR::CodeGenerator *cg, TR::Node *node, bool unbalanced)
   {
   int32_t  total = node->getNumChildren();
   int32_t  numberOfEntries = total - 2;
   int32_t *dataTable = (int32_t *)cg->allocateCodeMemory(numberOfEntries * sizeof(int32_t), cg->getCurrentEvaluationBlock()->isCold());
   int32_t  address = (intptr_t)dataTable;
   TR::Register *selector = cg->evaluate(node->getFirstChild());
   TR::Register *addrRegister = cg->allocateRegister();
   TR::Register *dataRegister = cg->allocateRegister();
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
   TR::Node     *defaultNode = node->getSecondChild();
   TR::Node     *defDepsNode = defaultNode->getNumChildren() > 0 ? defaultNode->getFirstChild() : 0;

   TR::addDependency(deps, dataRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, addrRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, selector, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::RegisterDependencyConditions *defaultDeps = deps;
   if (defDepsNode && !unbalanced)
      {
      cg->evaluate(defDepsNode);
      defaultDeps = defaultDeps->clone(cg, generateRegisterDependencyConditions(cg, defDepsNode, 0));
      }

   /* TODO: AOT fixup */
   if (!cg->comp()->getOption(TR_AOT))
      {
      armLoadConstant(node, address, addrRegister, cg, NULL);
      }
   else
      {
      // todo: AOT needs the addr of each instruction that needs fixing
      // NOTE: AOT depends on these instructions appearing exactly as they are.
      // if you change them, be sure to fix the relocation code, as well.
      loadAddressConstant(cg, node, address, addrRegister, NULL, false);
      }

   generatePreIncLoadInstruction(cg, node, dataRegister, addrRegister, 0);

   for (int32_t i = 2; i < total; i++)
      {
      TR::Node        *caseNode        = node->getChild(i);
      TR::LabelSymbol *caseTargetLabel = caseNode->getBranchDestination()->getNode()->getLabel();

      generateSrc2Instruction(cg, ARMOp_cmp, node, selector, dataRegister);

      if (unbalanced)
         {
         TR::RegisterDependencyConditions *caseDeps = deps;
         if (caseNode->getNumChildren() > 0)
            {
            TR::Node *caseDepsNode = caseNode->getFirstChild();
            cg->evaluate(caseDepsNode);
            caseDeps = caseDeps->clone(cg, generateRegisterDependencyConditions(cg, caseDepsNode, 0));
            }
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, caseTargetLabel, caseDeps);
         }
      else
         {
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, caseTargetLabel);
         }
      dataTable[i-2] = node->getChild(i)->getCaseConstant();
      if (i < total - 1)
         {
         generatePreIncLoadInstruction(cg, node, dataRegister, addrRegister, 4);
         }
      }

   TR::LabelSymbol *defaultTargetLabel = defaultNode->getBranchDestination()->getNode()->getLabel();

   if (defDepsNode && unbalanced)
      {
      cg->evaluate(defDepsNode);
      defaultDeps = defaultDeps->clone(cg, generateRegisterDependencyConditions(cg, defDepsNode, 0));
      }

   generateLabelInstruction(cg, ARMOp_b, node, defaultTargetLabel, defaultDeps);

   cg->stopUsingRegister(dataRegister);
   cg->stopUsingRegister(addrRegister);
   }

// Note: When the checks and/or lookupScheme implementations are changed with regards to number of used registers (or dependency),
// the matching GlDepRegs free checks in OMRCodeGenerator.cpp#getMaximumNumberOfGPRsAllowedAcrossEdge(..) must be updated to synch.
// Called by switchDispatch().
static void lookupScheme4(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->comp()->failCompilation<TR::CompilationException>("Automatically failing on lookup scheme 4");

/*  TODO implement lookups
   int32_t  total = node->getNumChildren();
   int32_t  numberOfEntries = total - 2;
   int32_t *dataTable = (int32_t *)cg->allocateCodeMemory(numberOfEntries * sizeof(int), cg->getCurrentEvaluationBlock()->isCold());
   int32_t  address = (int32_t)dataTable;
   TR::Register *selector = cg->evaluate(node->getFirstChild());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::Register *addrRegister = cg->allocateRegister();
   TR::Register *dataRegister = cg->allocateRegister();
   TR::Register *lowRegister = cg->allocateRegister();
   TR::Register *highRegister = cg->allocateRegister();
   TR::Register *pivotRegister = cg->allocateRegister();
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(7, 7);

   cg->machine()->setLinkRegisterKilled(true);
   TR::addDependency(conditions, dataRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, addrRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, selector, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, lowRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, highRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, pivotRegister, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(conditions, cndRegister, TR::RealRegister::NoReg, TR_CCR, cg);
   conditions->getPreConditions()->getRegisterDependency(5)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(5)->setExcludeGPR0();

   armLoadConstant(node, (numberOfEntries-1)<<2, highRegister, cg);
   armLoadConstant(node, address, addrRegister, cg);
   generateTrg1ImmInstruction(node, cg, ARMOp_mov, lowRegister, 0);
   TR::LabelSymbol *backLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *searchLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *biggerLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *linkLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   generateLabelInstruction(node, cg, ARMOp_label, backLabel);
   generateTrg1Src2Instruction(node, cg, ARMOp_add, pivotRegister, highRegister, lowRegister);
   new (cg->trHeapMemory()) TR::ARMTrg1Src1Imm2Instruction(0xfffffffc, 31, pivotRegister, pivotRegister, ARMOp_rlwinm, cg);
   generateTrg1MemInstruction(cg, ARMOp_lwzx, dataRegister, new (cg->trHeapMemory()) TR_ARMMemoryReference(addrRegister, pivotRegister, cg));
   generateTrg1Src2Instruction(node, cg, ARMOp_cmp4, cndRegister, dataRegister, selector);
   generateConditionalBranchInstruction(node, cg, ARMOp_bne, searchLabel, cndRegister);
   generateLabelInstruction(node, cg, ARMOp_bl, linkLabel);
   generateLabelInstruction(node, cg, ARMOp_label, linkLabel);
   new (cg->trHeapMemory()) TR::ARMTrg1Instruction(ARMOp_mflr, dataRegister, cg);
   generateTrg1Src1ImmInstruction(node, cg, ARMOp_addi, pivotRegister, pivotRegister, 20);
   generateTrg1Src2Instruction(node, cg, ARMOp_add, dataRegister, pivotRegister, dataRegister);
   new (cg->trHeapMemory()) TR::ARMSrc1Instruction(ARMOp_mtctr, dataRegister, cg);
   new (cg->trHeapMemory()) TR::Instruction(ARMOp_bctr, cg);
   for (int32_t ii=2; ii<total; ii++)
      {
      generateLabelInstruction(node, cg, ARMOp_b, node->getChild(ii)->getBranchDestination()->getNode()->getLabel());
      dataTable[ii-2] = node->getChild(ii)->getCaseConstant();
      }
   generateLabelInstruction(node, cg, ARMOp_label, searchLabel);
   generateConditionalBranchInstruction(node, cg, ARMOp_bgt, biggerLabel, cndRegister);
   generateTrg1Src2Instruction(node, cg, ARMOp_cmp4, cndRegister, pivotRegister, highRegister);
   generateConditionalBranchInstruction(node, cg, ARMOp_beq, node->getSecondChild()->getBranchDestination()->getNode()->getLabel(), cndRegister);
   generateTrg1Src1ImmInstruction(node, cg, ARMOp_addi, lowRegister, pivotRegister, 4);
   generateLabelInstruction(node, cg, ARMOp_b, backLabel);

   generateLabelInstruction(node, cg, ARMOp_label, biggerLabel);
   generateTrg1Src2Instruction(node, cg, ARMOp_cmp4, cndRegister, pivotRegister, lowRegister);
   generateConditionalBranchInstruction(node, cg, ARMOp_beq, node->getSecondChild()->getBranchDestination()->getNode()->getLabel(), cndRegister);
   generateTrg1Src1ImmInstruction(node, cg, ARMOp_addi, highRegister, pivotRegister, -4);
   new (cg->trHeapMemory()) TR::ARMDepLabelInstruction(ARMOp_b, backLabel, conditions, cg);
   */
   }

TR::Register *OMR::ARM::TreeEvaluator::tableEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int32_t       total = node->getNumChildren();
   int32_t       numBranchTableEntries = total - 2;
   TR::Register   *selectorReg           = cg->evaluate(node->getFirstChild());
   int32_t       i;
   TR::Node       *secondChild = node->getSecondChild();
   TR::RegisterDependencyConditions *conditions;
   TR::Register   *t1Register;

   if (numBranchTableEntries >= 5)
      {
      // Note: When implementation is changed with regards to number of used registers (or dependency),
      // the matching GlDepRegs free checks in OMRCodeGenerator.cpp#getMaximumNumberOfGPRsAllowedAcrossEdge(..) must be updated to synch.
      conditions  = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
      t1Register = cg->allocateRegister();
      TR::addDependency(conditions, t1Register, TR::RealRegister::NoReg, TR_GPR, cg);
      }
   else
      {
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
      }

   TR::addDependency(conditions, selectorReg, TR::RealRegister::NoReg, TR_GPR, cg);

   if (secondChild->getNumChildren() > 0)
      {
      cg->evaluate(secondChild->getFirstChild());
      conditions = conditions->clone(cg, generateRegisterDependencyConditions(cg, secondChild->getFirstChild(), 0));
      }

   if (numBranchTableEntries < 5)
      {
      for (i = 0; i < numBranchTableEntries; i++)
         {
         generateSrc1ImmInstruction(cg, ARMOp_cmp, node, selectorReg, i, 0);
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, node->getChild(2+i)->getBranchDestination()->getNode()->getLabel());
         }
      generateConditionalBranchInstruction(cg, node, ARMConditionCodeAL, secondChild->getBranchDestination()->getNode()->getLabel(), conditions);
      }
   else
      {
      TR::RealRegister *machineIP        = cg->machine()->getRealRegister(TR::RealRegister::gr15);
      uint32_t      base, rotate;

      if (!constantIsImmed8r(numBranchTableEntries, &base, &rotate))
         {
         TR::Register *tempRegister = cg->allocateRegister();
         armLoadConstant(node, numBranchTableEntries, tempRegister, cg);
         generateSrc2Instruction(cg, ARMOp_cmp, node, selectorReg, tempRegister);
         }
      else
         {
         generateSrc1ImmInstruction(cg, ARMOp_cmp, node, selectorReg, base, rotate);
         }
      generateConditionalBranchInstruction(cg, node, ARMConditionCodeCS, secondChild->getBranchDestination()->getNode()->getLabel());

      // code looks like:
      // (A) t1 <- selector - 1
      // (B) r15 <- r15 + selector * 4, (r15 is 8 ahead of (B), so the
      // code at (A) decrements selector by 1 to adjust so that the 0th
      // elem is at the right addr.
      generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, t1Register, selectorReg, 1, 0);
      generateTrg1Src2Instruction(cg, ARMOp_add, node, machineIP, machineIP, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, t1Register, 2));

      for (i = 2; i < total-1; i++)
         {
         generateLabelInstruction(cg, ARMOp_b, node, node->getChild(i)->getBranchDestination()->getNode()->getLabel());
         }
      generateLabelInstruction(cg, ARMOp_b, node, node->getChild(i)->getBranchDestination()->getNode()->getLabel(), conditions);
      }

   return NULL;
   }

static TR::Register *evaluateNULLCHKWithPossibleResolve(TR::Node *node, bool needsResolve, TR::CodeGenerator *cg)
   {
   // NOTE:
   // If no code is generated for the null check, just evaluate the
   // child and decrement its use count UNLESS the child is a pass-through node
   // in which case some kind of explicit test or indirect load must be generated
   // to force the null check at this point.

   // Generate the code for the null check
   //
   TR::Node     *firstChild = node->getFirstChild();
   TR::ILOpCode &opCode     = firstChild->getOpCode();
   TR::Node     *reference  = node->getNullCheckReference();

   // Skip the NULLCHK for TR::loadaddr nodes.
   //
   if (reference->getOpCodeValue() == TR::loadaddr)
      {
      cg->evaluate(firstChild);
      firstChild->decReferenceCount();
      return NULL;
      }

   bool needCode = !(cg->canNullChkBeImplicit(node, true));

   // nullchk is not implicit
   //
   if (needCode)
      {
      // TODO - If a resolve check is needed as well, the resolve must be done
      // before the null check, so that exceptions are handled in the correct
      // order.
      //
      ///// if (needsResolve)
      /////    {
      /////    ...
      /////    }

      TR::Register    *targetRegister = cg->evaluate(reference);

#if (NULLVALUE==0)
      generateSrc2Instruction(cg, ARMOp_tst, node, targetRegister, targetRegister);
#else
#error("NULL is not 0, must fix");
#endif

      TR::SymbolReference *NULLCHKException = node->getSymbolReference();
      TR::Instruction *instr1 = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)NULLCHKException->getMethodAddress(), NULL, NULLCHKException, NULL, NULL, ARMConditionCodeEQ);
      instr1->ARMNeedsGCMap(0xFFFFFFFF);
      cg->machine()->setLinkRegisterKilled(true);
      }

      // For a load, if this is the only use decrement the use count on the
      // grandchild since it has already been evaluated.
      // Otherwise just evaluate the child.
      //
   if (needCode && opCode.isLoad() && firstChild->getReferenceCount() == 1
       && firstChild->getSymbolReference() && !firstChild->getSymbolReference()->isUnresolved())
      {
      firstChild->decReferenceCount();
      reference->decReferenceCount();
      }
   else
      {
      cg->evaluate(firstChild);
      firstChild->decReferenceCount();
      }

      // If an explicit check has not been generated for the null check, there is
      // an instruction that will cause a hardware trap if the exception is to be
      // taken. If this method may catch the exception, a GC stack map must be
      // created for this instruction. All registers are valid at this GC point
      // TODO - if the method may not catch the exception we still need to note
      // that the GC point exists, since maps before this point and after it cannot
      // be merged.
      //
      /////if (!needCode && comp->getMethodSymbol()->isEHAwareMethod())
   if (!needCode)
      {
      TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
      if (faultingInstruction)
         faultingInstruction->setNeedsGCMap();
      }

   reference->setIsNonNull(true);

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return evaluateNULLCHKWithPossibleResolve(node, false, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // NOTE: ZEROCHK is intended to be general and straightforward.  If you're
   // thinking of adding special code for specific situations in here, consider
   // whether you want to add your own CHK opcode instead.  If you feel the
   // need for special handling here, you may also want special handling in the
   // optimizer, in which case a separate opcode may be more suitable.
   //
   // On the other hand, if the improvements you're adding could benefit other
   // users of ZEROCHK, please go ahead and add them!
   //
   // If in doubt, discuss your design with your team lead.

   TR::LabelSymbol *slowPathLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *restartLabel  = generateLabelSymbol(cg);
   slowPathLabel->setStartInternalControlFlow();
   restartLabel->setEndInternalControlFlow();

   // Temporarily hide the first child so it doesn't appear in the outlined call
   //
   node->rotateChildren(node->getNumChildren()-1, 0);
   node->setNumChildren(node->getNumChildren()-1);

   // Outlined instructions for check failure
   //
   TR_ARMOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_ARMOutOfLineCodeSection(node, TR::call, NULL, slowPathLabel, restartLabel, cg);
   cg->getARMOutOfLineCodeSectionList().add(outlinedHelperCall);

   // Restore the first child
   //
   node->setNumChildren(node->getNumChildren()+1);
   node->rotateChildren(0, node->getNumChildren()-1);

   // Children other than the first are only for the outlined path; we don't need them here
   //
   for (int32_t i = 1; i < node->getNumChildren(); i++)
      cg->recursivelyDecReferenceCount(node->getChild(i));

   // Inline instructions for the check
   //
   TR::Node *valueToCheck = node->getFirstChild();

#if 0
/* We need to change compareIntsForOrder and compareIntsForEquality for the optimization code below to work */
   if (valueToCheck->getOpCode().isBooleanCompare() &&
       valueToCheck->getChild(0)->getOpCode().isIntegerOrAddress() &&
       valueToCheck->getChild(1)->getOpCode().isIntegerOrAddress() &&
       performTransformation(cg->comp(), "O^O CODEGEN Optimizing ZEROCHK+%s %s\n", valueToCheck->getOpCode().getName(), valueToCheck->getName(cg->getDebug())))
      {
      if (valueToCheck->getOpCode().isCompareForOrder())
         {
         // switch branches since we want to go to OOL on node evaluating to 0
         // which corresponds to an ifcmp fall-through
         compareIntsForOrder(cmp2branch(valueToCheck->getOpCode().getOpCodeForReverseBranch(), cg),
                             slowPathLabel, valueToCheck, cg, valueToCheck->getOpCode().isUnsignedCompare(),
                             true, PPCOpProp_BranchUnlikely);
         }
      else
         {
         TR_ASSERT(valueToCheck->getOpCode().isCompareForEquality(), "Compare opcode must either be compare for order or for equality");
         compareIntsForEquality(cmp2branch(valueToCheck->getOpCode().getOpCodeForReverseBranch(), cg),
                                slowPathLabel, valueToCheck, cg, true, PPCOpProp_BranchUnlikely);
         }
      }
   else
#endif
      {
      TR::Register *value = cg->evaluate(node->getFirstChild());
      generateSrc1ImmInstruction(cg, ARMOp_cmp, node, value, 0, 0);
      generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, slowPathLabel);

      cg->decReferenceCount(node->getFirstChild());
      }
   generateLabelInstruction(cg, ARMOp_label, node, restartLabel);

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::resolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return evaluateNULLCHKWithPossibleResolve(node, true, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::resolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // No code is generated for the resolve check. The child will reference an
   // unresolved symbol and all check handling is done via the corresponding
   // snippet.
   //
   TR::Node *firstChild = node->getFirstChild();
   cg->evaluate(firstChild);
   firstChild->decReferenceCount();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getFirstChild()->getSecondChild();
   TR::DataType dtype = secondChild->getType();
   TR::Register *srcReg;

   if (!secondChild->getOpCode().isLoadConst() ||
       (dtype.isInt32() && secondChild->getInt()==0) ||
       (dtype.isInt64() && secondChild->getLongInt()==0))
      {
      srcReg = cg->evaluate(secondChild);
      if (dtype.isInt64())
         {
         TR::Register *trgReg = cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_orr, node, trgReg, srcReg->getHighOrder(), srcReg->getLowOrder());
         srcReg = trgReg;
         }
      generateSrc2Instruction(cg, ARMOp_tst, node, srcReg, srcReg);
      TR::SymbolReference *arithmeticException = node->getSymbolReference();
      TR::Instruction *instr1 = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)arithmeticException->getMethodAddress(), NULL, arithmeticException, NULL, NULL, ARMConditionCodeEQ);
      instr1->ARMNeedsGCMap(0xFFFFFFFF);
      cg->machine()->setLinkRegisterKilled(true);
      }
   cg->evaluate(node->getFirstChild());
   node->getFirstChild()->decReferenceCount();

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Instruction *instr1;

   TR::SymbolReference *BNDCHKException = node->getSymbolReference();
   instr1 = compareIntsForOrder(ARMConditionCodeLS, node, cg, BNDCHKException);
   instr1->ARMNeedsGCMap(0xFFFFFFFF);

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO - check this code

   // check that child[0] >= child[1], if not branch to check failure
   //
   // If the first child is a constant and the second isn't, swap the children.
   //

   TR::Node        *firstChild             = node->getFirstChild();
   TR::Node        *secondChild            = node->getSecondChild();
   TR::SymbolReference *BNDCHKException = node->getSymbolReference();
   TR::Instruction *instr;


   if (/* !isSmall() && */ firstChild->getOpCode().isLoadConst())
      {
      if (secondChild->getOpCode().isLoadConst())
         {
         if (firstChild->getInt() < secondChild->getInt())
            {
            // Check will always fail, just jump to the exception handler
            instr = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)BNDCHKException->getMethodAddress(), NULL, BNDCHKException, NULL, NULL, ARMConditionCodeAL);
            cg->machine()->setLinkRegisterKilled(true);
            }
         else
            {
            // Check will always succeed, no need for an instruction
            instr = NULL;
            }
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      else
         {
         node->swapChildren();
         instr = compareIntsForOrder(ARMConditionCodeGT, node, cg, BNDCHKException);
         node->swapChildren();
         }
      }
   else
      {
      instr = compareIntsForOrder(ARMConditionCodeLT, node, cg, BNDCHKException);
      }

   if (instr)
      {
      instr->ARMNeedsGCMap(0xFFFFFFFF);
      }

   return NULL;

   }

static void VMoutlinedHelperArrayStoreCHKEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, bool srcIsNonNull, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR::SymbolReference *arrayStoreChkHelper = node->getSymbolReference();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());

   TR::addDependency(deps, dstReg, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(deps, srcReg, TR::RealRegister::gr1, TR_GPR, cg);

   TR::Instruction *gcPoint = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)arrayStoreChkHelper->getMethodAddress(), deps, arrayStoreChkHelper);
   gcPoint->ARMNeedsGCMap(0xFFFFFFFF);

   cg->machine()->setLinkRegisterKilled(true);
   }

TR::Register *OMR::ARM::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *srcNode = node->getFirstChild()->getSecondChild();
   TR::Node *dstNode = node->getFirstChild()->getThirdChild();
   TR::Register *srcReg = cg->evaluate(srcNode);
   TR::Register *dstReg = cg->evaluate(dstNode);

   if (!srcNode->isNull())
      {
      VMoutlinedHelperArrayStoreCHKEvaluator(node, srcReg, dstReg, srcNode->isNonNull(), cg);
      }

   TR::MemoryReference *storeMR = new (cg->trHeapMemory()) TR::MemoryReference(node->getFirstChild(), TR::Compiler->om.sizeofReferenceAddress(), cg);

   generateMemSrc1Instruction(cg, ARMOp_str, node, storeMR, srcReg);

   if (!srcNode->isNull())
      {
#ifdef J9_PROJECT_SPECIFIC
      VMwrtbarEvaluator(node, srcReg, dstReg, NULL, true, cg);
#endif
      cg->machine()->setLinkRegisterKilled(true);
      }

   cg->decReferenceCount(srcNode);
   cg->decReferenceCount(dstNode);
   storeMR->decNodeReferenceCounts();
   cg->decReferenceCount(node->getFirstChild());

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO - implement this when fast arraycopy is implemented
   //
   return badILOpEvaluator(node, cg);
   }

// generates max/min code for i, iu, l, lu
static TR::Register *generateMaxMin(TR::Node *node, TR::CodeGenerator *cg, bool max)
   {
   TR::Node *child  = node->getFirstChild();
   TR::DataType data_type = child->getDataType();
   TR::DataType type = child->getType();
   bool two_reg = type.isInt64();
   bool isUnsigned = false;
   TR::Instruction *cursor;
   TR::Register *trgReg;

   TR::Register *reg = cg->evaluate(child);

   if (cg->canClobberNodesRegister(child))
      {
      trgReg = reg; // use first child as a target
      }
   else
      {
      switch (data_type)
         {
         case TR::Int32:
            trgReg = cg->allocateRegister();
            break;
         case TR::Int64:
            trgReg = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
            break;
         default:
            TR_ASSERT(false, "assertion failure");
         }
      if (node->getOpCodeValue() == TR::iumax || node->getOpCodeValue() == TR::lumax ||
          node->getOpCodeValue() == TR::iumin || node->getOpCodeValue() == TR::lumin)
         {
         isUnsigned = true;
         }
      if (two_reg)
         {
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), reg->getLowOrder());
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), reg->getHighOrder());
         }
      else
         {
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, reg);
         }
      }

   TR_ARMConditionCode cond = max ? (isUnsigned ? ARMConditionCodeHI : ARMConditionCodeGT) : (isUnsigned ? ARMConditionCodeLS : ARMConditionCodeLT);
   int n = node->getNumChildren();
   int i = 1;
   for ( ; i < n; i++)
      {
      TR::Node *child = node->getChild(i);
      TR::Register *reg = cg->evaluate(child);
      if (two_reg)
         {
         TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
         deps->addPostCondition(trgReg->getLowOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(trgReg->getHighOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(reg->getLowOrder(), TR::RealRegister::NoReg);
         deps->addPostCondition(reg->getHighOrder(), TR::RealRegister::NoReg);

         TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         // First compare
         cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, trgReg->getHighOrder(), reg->getHighOrder());
         cursor = generateConditionalBranchInstruction(cg, node, cond, doneLabel, deps, cursor);

         cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), reg->getLowOrder(), cursor);
         cursor->setConditionCode(ARMConditionCodeNE);
         cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), reg->getHighOrder(), cursor);
         cursor->setConditionCode(ARMConditionCodeNE);
         cursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, doneLabel, deps, cursor);
         // Second compare
         cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, trgReg->getLowOrder(), reg->getLowOrder(), cursor);
         cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), reg->getLowOrder(), cursor);
         cursor->setConditionCode(max ? ARMConditionCodeLS: ARMConditionCodeHI);
         cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), reg->getHighOrder(), cursor);
         cursor->setConditionCode(max ? ARMConditionCodeLS: ARMConditionCodeHI);
         // doneLabel
         cursor = generateLabelInstruction(cg, ARMOp_label, node, doneLabel, deps, cursor);
         }
      else
         {
         cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, trgReg, reg);

         cond = max ? (isUnsigned ? ARMConditionCodeLS : ARMConditionCodeLT) : (isUnsigned ? ARMConditionCodeHI : ARMConditionCodeGT);
         cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, reg, cursor);
         cursor->setConditionCode(max ? ARMConditionCodeLT: ARMConditionCodeGT);
         }
      }

   node->setRegister(trgReg);
   for (i = 0; i < n; i++)
      {
      cg->decReferenceCount(node->getChild(i));
      }
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::maxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return generateMaxMin(node, cg, true);
   }

TR::Register *OMR::ARM::TreeEvaluator::minEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return generateMaxMin(node, cg, false);
   }

TR::Register *OMR::ARM::TreeEvaluator::igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()>=1, "at least one child expected for igoto");

   TR::Machine         *machine    = cg->machine();

   TR::Node *labelAddr = node->getFirstChild();
   TR::Register *addrReg = cg->evaluate(labelAddr);
   TR::RealRegister *gr15 = machine->getRealRegister(TR::RealRegister::gr15);
   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() > 1)
      {
      TR_ASSERT(node->getNumChildren() == 2 && node->getChild(1)->getOpCodeValue() == TR::GlRegDeps, "igoto has maximum of two children and second one must be global register dependency");
      TR::Node *glregdep = node->getChild(1);
      cg->evaluate(glregdep);
      deps = generateRegisterDependencyConditions(cg, glregdep, 0);
      cg->decReferenceCount(glregdep);
      }
   TR::Instruction *instr = generateTrg1Src1Instruction(cg, ARMOp_mov, node, gr15, addrReg);
   cg->decReferenceCount(labelAddr);
   node->setRegister(NULL);
   return NULL;
   }
