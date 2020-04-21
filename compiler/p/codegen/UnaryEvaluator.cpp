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
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCOpsDefines.hpp"

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;


TR::Register *OMR::Power::TreeEvaluator::iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = node->setRegister(cg->allocateRegister());
   loadConstant(cg, node, (int32_t)node->get64bitIntegralValue(), tempReg);
   return tempReg;
   }

TR::Register *OMR::Power::TreeEvaluator::aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   bool isClass = node->isClassPointerConstant();
   TR_ResolvedMethod * method = comp->getCurrentMethod();

   bool isPicSite = node->isClassPointerConstant() && cg->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock *) node->getAddress(), method );
   if (!isPicSite)
      isPicSite = node->isMethodPointerConstant() && cg->fe()->isUnloadAssumptionRequired(cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) node->getAddress(), method)->classOfMethod(), method);

   bool isProfiledPointerConstant = node->isClassPointerConstant() || node->isMethodPointerConstant();

   // use data snippet only on class pointers when HCR is enabled
   intptr_t address = cg->comp()->target().is64Bit()? node->getLongInt(): node->getInt();
   if (isClass && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)address, node) ||
       isProfiledPointerConstant && cg->profiledPointersRequireRelocation())
      {
      TR::Register *trgReg = cg->allocateRegister();
      loadAddressConstantInSnippet(cg, node, address, trgReg, NULL,TR::InstOpCode::Op_load, isPicSite, NULL);
      node->setRegister(trgReg);
      return trgReg;
      }
   else
      {
      TR::Register *tempReg = node->setRegister(cg->allocateRegister());
      loadActualConstant(cg, node, address, tempReg, NULL, isPicSite);
      return tempReg;
      }
   }

TR::Register *OMR::Power::TreeEvaluator::lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   if (cg->comp()->target().is64Bit())
      {
      TR::Register *tempReg = node->setRegister(cg->allocateRegister());
      loadConstant(cg, node, node->getLongInt(), tempReg);
      return tempReg;
      }
   TR::Register     *lowReg    = cg->allocateRegister();
   TR::Register     *highReg   = cg->allocateRegister();
   TR::RegisterPair *trgReg   = cg->allocateRegisterPair(lowReg, highReg);
   node->setRegister(trgReg);
   int32_t lowValue  = node->getLongIntLow();
   int32_t highValue = node->getLongIntHigh();
   int32_t difference;

   difference = highValue - lowValue;
   loadConstant(cg, node, lowValue, lowReg);
   if ((highValue >= LOWER_IMMED && highValue <= UPPER_IMMED) ||
       difference < LOWER_IMMED || difference > UPPER_IMMED)
      {
      loadConstant(cg, node, highValue, highReg);
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, highReg, lowReg, difference);
      }

   return trgReg;
   }


TR::Register *OMR::Power::TreeEvaluator::inegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;
   trgReg = cg->allocateRegister(TR_GPR);
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceRegister = cg->evaluate(firstChild);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, trgReg, sourceRegister);

   cg->decReferenceCount(firstChild);
   return node->setRegister(trgReg);
   }

TR::Register *OMR::Power::TreeEvaluator::lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   if (cg->comp()->target().is64Bit())
      {
      return TR::TreeEvaluator::inegEvaluator(node, cg);
      }
   else
      {
      TR::Node     *firstChild     = node->getFirstChild();
      TR::Register *lowReg  = cg->allocateRegister();
      TR::Register *highReg = cg->allocateRegister();
      TR::RegisterPair *trgReg = cg->allocateRegisterPair(lowReg, highReg);
      TR::Register *sourceRegister    = cg->evaluate(firstChild);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, lowReg, sourceRegister->getLowOrder(), 0);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::subfze, node, highReg, sourceRegister->getHighOrder());

      node->setRegister(trgReg);
      cg->decReferenceCount(firstChild);
      return trgReg;
      }
   }

TR::Register *OMR::Power::TreeEvaluator::iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *tmpReg  = cg->allocateRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceRegister = cg->evaluate(firstChild);

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, trgReg, sourceRegister, 31);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, tmpReg, sourceRegister, trgReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::subfc, node, trgReg, trgReg, tmpReg);
   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   return trgReg;
   }


TR::Register *OMR::Power::TreeEvaluator::labsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   if (cg->comp()->target().is64Bit())
      {
      TR::Register *trgReg = cg->allocateRegister();
      TR::Register *tmpReg = cg->allocateRegister();
      TR::Node *firstChild = node->getFirstChild();
      TR::Register *sourceRegister = cg->evaluate(firstChild);

      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, trgReg, sourceRegister, 63);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, tmpReg, sourceRegister, trgReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subfc, node, trgReg, trgReg, tmpReg);
      cg->stopUsingRegister(tmpReg);

      node->setRegister(trgReg);
      cg->decReferenceCount(firstChild);
      return trgReg;
      }
   else
      {
      TR::Node     *firstChild     = node->getFirstChild();
      TR::Register *lowReg  = cg->allocateRegister();
      TR::Register *highReg = cg->allocateRegister();
      TR::RegisterPair *trgReg = cg->allocateRegisterPair(lowReg, highReg);
      TR::Register *sourceRegister    = cg->evaluate(firstChild);
      TR::Register *tempReg = cg->allocateRegister();

      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, tempReg, sourceRegister->getHighOrder(), 31);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, highReg, sourceRegister->getHighOrder(), tempReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, lowReg, sourceRegister->getLowOrder(), tempReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subfc, node, lowReg, tempReg, lowReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subfe, node, highReg, tempReg, highReg);
      cg->stopUsingRegister(tempReg);

      node->setRegister(trgReg);
      cg->decReferenceCount(firstChild);
      return trgReg;
      }
   }

TR::Register *OMR::Power::TreeEvaluator::fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;
   trgReg = cg->allocateSinglePrecisionRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceRegister = cg->evaluate(firstChild);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fabs, node, trgReg, sourceRegister);
   cg->decReferenceCount(firstChild);
   return node->setRegister(trgReg);
   }

TR::Register *OMR::Power::TreeEvaluator::dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;
   trgReg = cg->allocateRegister(TR_FPR);
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceRegister = cg->evaluate(firstChild);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fabs, node, trgReg, sourceRegister);
   cg->decReferenceCount(firstChild);
   return node->setRegister(trgReg);
   }

// also handles b2s, bu2s
TR::Register *OMR::Power::TreeEvaluator::b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg;
   if (node->isUnneededConversion() ||
       (child->getOpCodeValue() != TR::bload &&
        child->getOpCodeValue() != TR::bloadi &&
        child->getOpCodeValue() != TR::buload &&
        child->getOpCodeValue() != TR::buloadi &&
        child->getOpCodeValue() != TR::s2b &&
        child->getOpCodeValue() != TR::f2b &&
        child->getOpCodeValue() != TR::d2b &&
        child->getOpCodeValue() != TR::i2b &&
        child->getOpCodeValue() != TR::l2b &&
        child->getOpCodeValue() != TR::iRegLoad &&
        child->getOpCodeValue() != TR::iuRegLoad ))
      {
      trgReg = cg->gprClobberEvaluate(child);
      }
   else
      {
      trgReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsb, node, trgReg, cg->evaluate(child));
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::b2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::b2lEvaluator(node, cg);
   else
      return TR::TreeEvaluator::b2iEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::s2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg = 0;

   TR_ASSERT(node->getOpCodeValue() == TR::s2i, "s2iEvaluator called for a %s node",
             node->getOpCode().getName());

   if (node->getOpCodeValue() == TR::s2i)// s2i sign extend
      {
      trgReg = child->getRegister();
      if (!trgReg && child->getOpCode().isMemoryReference() && child->getReferenceCount() == 1)
         {
         trgReg = cg->allocateRegister();
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
#ifdef J9_ProjectSpecific
         if (node->getFirstChild()->getOpCodeValue() == TR::irsload)
            {
            tempMR->forceIndexedForm(node->getFirstChild(), cg);
            generateTrg1MemInstruction(cg, TR::InstOpCode::lhbrx, node, trgReg, tempMR);
            generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgReg);
            }
         else
#endif
            generateTrg1MemInstruction(cg, TR::InstOpCode::lha, node, trgReg, tempMR);
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         if(!trgReg)
           {
           // child reg is null. need to evaluate child
           trgReg = cg->evaluate(child);
           if(child->getReferenceCount() == 1)
             {
             generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgReg);
             }
           else
             {
             TR::Register * trgRegOld = trgReg;
             trgReg = cg->allocateRegister();
             generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgRegOld);
             }
           }
         else if(child->getReferenceCount() > 1)
           {
           // child has been evaluated, but will be used later
           TR::Register * trgRegOld = trgReg;
           trgReg = cg->allocateRegister();
           generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgRegOld);
           }
         else
           {
           generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgReg);
           }
         }
      }
   else
      trgReg = cg->gprClobberEvaluate(child);

   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   if (cg->comp()->target().is64Bit())
      {
      TR::Register *trgReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsb, node, trgReg, cg->evaluate(child));
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   else // 32 bit target
      {
      TR::Register *lowReg  = cg->allocateRegister();
      TR::Register *highReg = cg->allocateRegister();
      TR::RegisterPair *trgReg = cg->allocateRegisterPair(lowReg, highReg);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsb, node, lowReg, cg->evaluate(child));
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, highReg, trgReg->getLowOrder(), 31);
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   }

//also handles i2l
TR::Register *OMR::Power::TreeEvaluator::s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   if (cg->comp()->target().is64Bit())
      {
      TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::extsw;
      TR::ILOpCodes  nodeOpCode = node->getOpCodeValue();
      if (nodeOpCode == TR::i2l && child->skipSignExtension())
         {
         if (child->getReferenceCount() < 2 || !cg->useClobberEvaluate())
            {
            cg->decReferenceCount(child);
            return cg->evaluate(child);
            }
         opCode = TR::InstOpCode::mr;
         }
      TR::Register *trgReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, opCode, node, trgReg, cg->evaluate(child));
      node->setRegister(trgReg);
      cg->decReferenceCount(child);

      return trgReg;
      }
   else // 32 bit target
      {
      TR::Register *sourceRegister = NULL;
      TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child),
                                                     cg->allocateRegister());
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, trgReg->getHighOrder(), trgReg->getLowOrder(), 31);
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   }

//also handles su2l
TR::Register *OMR::Power::TreeEvaluator::iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   if (cg->comp()->target().is64Bit())
      {
      if (node->getOpCodeValue() == TR::iu2l && child && child->getReferenceCount() == 1 && !child->getRegister() &&
          (child->getOpCodeValue() == TR::iloadi || child->getOpCodeValue() == TR::iload))
         {
         TR::Register *returnRegister = cg->evaluate(child);
         node->setRegister(returnRegister);
         cg->decReferenceCount(child);
         return returnRegister;
         }

      if (!cg->useClobberEvaluate() && // Sharing registers between nodes only works when not using simple clobber evaluation
          node->getOpCodeValue() == TR::iu2l && child && child->getRegister() &&
          (child->getOpCodeValue() == TR::iloadi || child->getOpCodeValue() == TR::iload))
         {
         node->setRegister(child->getRegister());
         cg->decReferenceCount(child);
         return child->getRegister();
         }

      // TODO: check if first child is memory reference
      TR::Register *trgReg = cg->allocateRegister();
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, trgReg, cg->evaluate(child), 0, CONSTANT64(0x00000000ffffffff));
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   else // 32 bit target
      {
      TR::Register *sourceRegister = NULL;
      TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child),
                                                             cg->allocateRegister());
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg->getHighOrder(), 0);
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   }

TR::Register *OMR::Power::TreeEvaluator::su2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::iu2lEvaluator(node, cg);
   else
      return TR::TreeEvaluator::s2iEvaluator(node, cg);
   }


TR::Register *OMR::Power::TreeEvaluator::su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *temp;

   temp = child->getRegister();
   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (temp == NULL))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, cg->evaluate(child), 0, 0xffff);
      cg->decReferenceCount(child);
      }
   return node->setRegister(trgReg);
   }

TR::Register *OMR::Power::TreeEvaluator::s2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::s2lEvaluator(node, cg);
   else
      return TR::TreeEvaluator::s2iEvaluator(node, cg);
   }

// also handles s2b
TR::Register *OMR::Power::TreeEvaluator::i2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);
   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::i2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegister();

   if (child->getReferenceCount() == 1 &&
       child->getOpCode().isMemoryReference() &&
       child->getRegister() == NULL)
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
      tempMR->addToOffset(node, cg->comp()->target().cpu.isBigEndian()?2:0, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, cg->evaluate(child), 0, 0xffff);
      cg->decReferenceCount(child);
      }
   return node->setRegister(trgReg);
   }

TR::Register *OMR::Power::TreeEvaluator::i2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg = cg->allocateRegister();

   if (cg->comp()->target().cpu.id() != TR_PPCp6 &&  // avoid algebraic loads on P6
       child->getReferenceCount() == 1 &&
       child->getOpCode().isMemoryReference() &&
       child->getRegister() == NULL)
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
      tempMR->addToOffset(node, cg->comp()->target().cpu.isBigEndian()?2:0, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, trgReg, tempMR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgReg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, cg->evaluate(child));
      cg->decReferenceCount(child);
      }

   return node->setRegister(trgReg);
   }

TR::Register *OMR::Power::TreeEvaluator::i2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::s2lEvaluator(node, cg);
   else
      return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::iu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::iu2lEvaluator(node, cg);
   else
      return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::l2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *trgReg;

   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (child->getRegister() == NULL))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 1, cg);
      trgReg = cg->allocateRegister();
      tempMR->addToOffset(node, cg->comp()->target().cpu.isBigEndian()?7:0, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lbz, node, trgReg, tempMR);
      node->setRegister(trgReg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *temp = cg->evaluate(child);
      if (child->getReferenceCount() == 1 || !cg->useClobberEvaluate())
         {
         if (cg->comp()->target().is64Bit())
            trgReg = temp;
         else // 32 bit target
            trgReg = temp->getLowOrder();
         }
      else
         {
         trgReg = cg->allocateRegister();
         if (cg->comp()->target().is64Bit())
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, temp);
	 else
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, temp->getLowOrder());
         }
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      }
   return trgReg;
   }

// is this used?
TR::Register *OMR::Power::TreeEvaluator::l2buEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *trgReg = cg->allocateRegister();

   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (child->getRegister() == NULL))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 1, cg);
      tempMR->addToOffset(node, cg->comp()->target().cpu.isBigEndian()?7:0, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lbz, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *tempReg;
      if (cg->comp()->target().is64Bit())
         tempReg = cg->evaluate(child);
      else // 32 bit target
         tempReg = cg->evaluate(child)->getLowOrder();
      cg->decReferenceCount(child);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, tempReg, 0, 0xff);
      }

   return node->setRegister(trgReg);
   }



TR::Register *OMR::Power::TreeEvaluator::l2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *temp;
   TR::Register *trgReg = cg->allocateRegister();

   temp = child->getRegister();
   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (temp == NULL))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
      tempMR->addToOffset(node, cg->comp()->target().cpu.isBigEndian()?6:0, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      if (cg->comp()->target().is64Bit())
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, trgReg, cg->evaluate(child), 0, CONSTANT64(0x000000000000ffff));
      else // 32 bit target
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, cg->evaluate(child)->getLowOrder(), 0, 0xffff);
      cg->decReferenceCount(child);
      }
   return node->setRegister(trgReg);
   }


TR::Register *OMR::Power::TreeEvaluator::l2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *trgReg = cg->allocateRegister();

   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() &&
       child->getRegister() == NULL)
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
      tempMR->addToOffset(node, cg->comp()->target().cpu.isBigEndian()?6:0, cg);
      if (cg->comp()->target().cpu.id() == TR_PPCp6)  // avoid algebraic loads on P6
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, trgReg, tempMR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgReg);
         }
      else
         generateTrg1MemInstruction(cg, TR::InstOpCode::lha, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      if (cg->comp()->target().is64Bit())
         generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, cg->evaluate(child));
      else // 32 bit target
         generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, cg->evaluate(child)->getLowOrder());
      cg->decReferenceCount(child);
      }
   return node->setRegister(trgReg);
   }

TR::Register *OMR::Power::TreeEvaluator::l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *trgReg;
   TR::Register *temp;

   temp = child->getRegister();
   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (temp == NULL))
      {
      trgReg = cg->allocateRegister();
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      tempMR->addToOffset(node, cg->comp()->target().cpu.isBigEndian()?4:0, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      node->setRegister(trgReg);
      }
   else
      {
      temp = cg->evaluate(child);
      if (child->getReferenceCount() == 1 || !cg->useClobberEvaluate())
         {
         if (cg->comp()->target().is64Bit())
            trgReg = temp;
         else // 32 bit target
            trgReg = temp->getLowOrder();
         }
      else
         {
         trgReg = cg->allocateRegister();
         if (cg->comp()->target().is64Bit())
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, temp);
	 else
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, temp->getLowOrder());
         }
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      }
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::l2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (cg->comp()->target().is64Bit())
      {
      // -J9JIT_COMPRESSED_POINTER-
      //
      if (comp->useCompressedPointers())
         {
         // pattern match the sequence under the l2a
         //    iaload f      l2a                       <- node
         //       aload O       ladd
         //                       lshl
         //                          i2l
         //                            iiload f        <- load
         //                               aload O
         //                          iconst shftKonst
         //                       lconst HB
         //
         // -or- if the load is known to be null
         //  l2a
         //    i2l
         //      iiload f
         //         aload O
         //
         TR::Node *firstChild = node->getFirstChild();
         bool hasCompPtrs = false;
         if (firstChild->getOpCode().isAdd() &&
               firstChild->containsCompressionSequence())
            hasCompPtrs = true;
         else if (node->isNull())
            hasCompPtrs = true;

         // after remat changes, this assume is no longer valid
         //
         ///TR_ASSERT(hasCompPtrs, "no compression sequence found under l2a node [%p]\n", node);
         TR::Register *source = cg->evaluate(firstChild);

         if ((firstChild->containsCompressionSequence() || (TR::Compiler->om.compressedReferenceShift() == 0)) && !node->isl2aForCompressedArrayletLeafLoad())
            source->setContainsCollectedReference();

         node->setRegister(source);
         cg->decReferenceCount(firstChild);
         cg->insertPrefetchIfNecessary(node, source);

         return source;
         }
      else
         return TR::TreeEvaluator::passThroughEvaluator(node, cg);
      }
   else
      return TR::TreeEvaluator::l2iEvaluator(node, cg);
   }


TR::Register *OMR::Power::TreeEvaluator::su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   if (cg->comp()->target().is64Bit())
      {
      TR::Register *trgReg = cg->allocateRegister();
      TR::Register *temp;

      temp = child->getRegister();
      if (child->getReferenceCount()==1 &&
          child->getOpCode().isMemoryReference() && (temp == NULL))
         {
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, trgReg, tempMR);
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, trgReg, cg->evaluate(child), 0, CONSTANT64(0x000000000000ffff));
         cg->decReferenceCount(child);
         }

      return node->setRegister(trgReg);
      }
   else // 32 bit target
      {
      bool decSharedNode = false;
      TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child),
                                                             cg->allocateRegister());
      TR::Register *temp;
      temp = child->getRegister();
      if (child->getReferenceCount()==1 &&
          child->getOpCode().isMemoryReference() && (temp == NULL))
         {
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lhz, node, trgReg->getLowOrder(), tempMR);
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg->getLowOrder(), cg->evaluate(child), 0, 0xffff);
         decSharedNode = true;
         }
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg->getHighOrder(), 0);

      node->setRegister(trgReg);

      if (decSharedNode)
         cg->decReferenceCount(child);

      return trgReg;
      }

   }

TR::Register *OMR::Power::TreeEvaluator::bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg;

   if (child->getOpCode().isMemoryReference())
      {
      trgReg = cg->gprClobberEvaluate(child);
      }
   else
      {
      trgReg = child->getRegister();
      if (!trgReg && child->getOpCode().isMemoryReference() && trgReg == NULL)
         {
         trgReg = cg->allocateRegister();
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 2, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lbz, node, trgReg, tempMR);
         child->setRegister(trgReg);
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         trgReg = cg->allocateRegister();
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, cg->evaluate(child), 0, 0xff);
         }
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg;

   if (cg->comp()->target().is64Bit())
      {
      if (child->getOpCode().isMemoryReference())
         {
         trgReg = cg->gprClobberEvaluate(child);
         }
      else
         {
         trgReg = cg->allocateRegister();
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, trgReg, cg->evaluate(child), 0, CONSTANT64(0x00000000000000ff));
         }
      }
   else // 32 bit target
      {
      TR::Register *lowReg;
      if (child->getOpCode().isMemoryReference())
         {
         lowReg = cg->gprClobberEvaluate(child);
         }
      else
         {
         lowReg = cg->allocateRegister();
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, lowReg, cg->evaluate(child), 0, 0xff);
         }

      TR::Register *highReg = cg->allocateRegister();
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, highReg, 0);
      trgReg = cg->allocateRegisterPair(lowReg, highReg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(child);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::bu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::bu2lEvaluator(node, cg);
   else
      return TR::TreeEvaluator::bu2iEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::a2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::l2iEvaluator(node, cg);
   else
      return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::a2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::passThroughEvaluator(node, cg);
   else
      return TR::TreeEvaluator::iu2lEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::a2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::l2bEvaluator(node, cg);
   else
      return TR::TreeEvaluator::i2bEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::a2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      return TR::TreeEvaluator::l2sEvaluator(node, cg);
   else
      return TR::TreeEvaluator::i2sEvaluator(node, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::libmFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node::recreate(node, TR::dcall);
   TR::Register *trgReg = TR::TreeEvaluator::directCallEvaluator(node, cg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::strcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node::recreate(node, TR::icall);
   TR::Register *trgReg = TR::TreeEvaluator::directCallEvaluator(node, cg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::strcFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node::recreate(node, TR::acall);
   TR::Register *trgReg = TR::TreeEvaluator::directCallEvaluator(node, cg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::dfloorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);

   TR::Register *trgReg;
   TR::Register *tmp2Reg;
   TR::Register *tmp3Reg;
   TR::Register *tmp4Reg;

   TR::Register *tmp1Reg, *tmp5Reg;
   TR::Node *const_node, *const_node_max;
   if ((node->getOpCodeValue() == TR::dfloor) || (node->getOpCodeValue() == TR::dceil)){
      trgReg = cg->allocateRegister(TR_FPR);
      tmp2Reg = cg->allocateRegister(TR_FPR);
      tmp3Reg = cg->allocateRegister(TR_FPR);
      tmp4Reg = cg->allocateRegister(TR_FPR);

      const_node = TR::Node::create(node, TR::dconst, 0);
      if (node->getOpCodeValue() == TR::dceil){
        const_node->setDouble(1.0);
      }
      else
        const_node->setDouble(-1.0);
      tmp1Reg = TR::TreeEvaluator::dconstEvaluator(const_node, cg);

      const_node_max = TR::Node::create(node, TR::dconst, 0);
      const_node_max->setDouble(CONSTANT64(0x10000000000000));                      // 2**52
      tmp5Reg = TR::TreeEvaluator::dconstEvaluator(const_node_max, cg);
   }
   else
   {
      trgReg = cg->allocateSinglePrecisionRegister();
      tmp2Reg = cg->allocateSinglePrecisionRegister();
      tmp3Reg = cg->allocateSinglePrecisionRegister();
      tmp4Reg = cg->allocateSinglePrecisionRegister();

      const_node = TR::Node::create(node, TR::fconst, 0);
      if (node->getOpCodeValue() == TR::fceil) {
        const_node->setFloat(1.0);
      }
      else
        const_node->setFloat(-1.0);
      tmp1Reg = TR::TreeEvaluator::fconstEvaluator(const_node, cg);

      const_node_max = TR::Node::create( node, TR::fconst, 0);
      const_node_max->setFloat(0x800000);
      tmp5Reg = TR::TreeEvaluator::fconstEvaluator(const_node_max, cg);
   }

   const_node->unsetRegister();
   const_node_max->unsetRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fctidz, node, trgReg, srcReg);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, trgReg, trgReg);             //trgReg = trunc(x)
   generateTrg1Src2Instruction(cg, TR::InstOpCode::fadd, node, tmp2Reg, trgReg, tmp1Reg);    // tmp2  = trunc(x) -1  for floor
                                                                                   //       = trunc(x) +1  for ceil
   if ((node->getOpCodeValue() == TR::dfloor) || (node->getOpCodeValue() == TR::ffloor))
     generateTrg1Src2Instruction(cg, TR::InstOpCode::fsub, node, tmp3Reg, srcReg, trgReg);   // tmp3  = (x-trunc(x)) for floor
   else
     generateTrg1Src2Instruction(cg, TR::InstOpCode::fsub, node, tmp3Reg, trgReg, srcReg);   // tmp3  = (trunc(x)-x) for ceil

   generateTrg1Src3Instruction(cg, TR::InstOpCode::fsel, node, trgReg, tmp3Reg, trgReg, tmp2Reg);

   // if x is a NaN or out of range  (i.e. fabs(x)>2**52), then floor(x) = x, otherwise the value computed above

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fabs, node, tmp4Reg, srcReg);             // tmp4  = fabs(x)

   generateTrg1Src2Instruction(cg, TR::InstOpCode::fsub, node, tmp5Reg, tmp5Reg, tmp4Reg);   // tmp5  = 2**52 - fabs(x)

   generateTrg1Src3Instruction(cg, TR::InstOpCode::fsel, node, trgReg, tmp5Reg, trgReg, srcReg);


   cg->stopUsingRegister(tmp1Reg);
   cg->stopUsingRegister(tmp2Reg);
   cg->stopUsingRegister(tmp3Reg);

   cg->stopUsingRegister(tmp4Reg);
   cg->stopUsingRegister(tmp5Reg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);


   return trgReg;
   }
