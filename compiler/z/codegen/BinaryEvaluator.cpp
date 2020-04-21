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

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/StorageInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "ras/Debug.hpp"
#include "ras/Delimiter.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/BinaryAnalyser.hpp"
#include "z/codegen/BinaryCommutativeAnalyser.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"

/**
 * lnegFor32bit - 32 bit code generation to negate a long integer
 */
TR::RegisterPair * lnegFor32Bit(TR::Node * node, TR::CodeGenerator * cg, TR::RegisterPair * targetRegisterPair, TR::RegisterDependencyConditions * dep = 0)
   {
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
   TR::RegisterDependencyConditions * localDeps = NULL;


   // If deps are passed in, we assume that there is a wider scoped set of deps that are
   // handled by the caller.   If the deps are null, we must handle deps for internal control
   // flow ourselves here.
   //
   if (!dep)
      {
      localDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
      localDeps->addPostCondition(targetRegisterPair->getHighOrder(), TR::RealRegister::AssignAny);
      }
   else
      {
      dep->addPostConditionIfNotAlreadyInserted(targetRegisterPair->getHighOrder(), TR::RealRegister::AssignAny);
      }

   // Do complements on reg pair
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegisterPair->getHighOrder(), targetRegisterPair->getHighOrder());
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegisterPair->getLowOrder(), targetRegisterPair->getLowOrder());


   // Check to see if we need to propagate an overflow bit from LS int to MS int.
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, cFlowRegionEnd);

   // Increment MS int due to overflow in LS int
   generateRIInstruction(cg, TR::InstOpCode::AHI, node, targetRegisterPair->getHighOrder(), -1);

   // Not equal, straight through
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, localDeps);
   cFlowRegionEnd->setEndInternalControlFlow();
   return targetRegisterPair;
   }

/**
 * lnegFor128Bit - negate a 128bit integer in 64-bit register pairs
 */
TR::RegisterPair * lnegFor128Bit(TR::Node * node, TR::CodeGenerator * cg, TR::RegisterPair * targetRegisterPair, TR::RegisterDependencyConditions * dep = 0)
   {
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

   TR::RegisterDependencyConditions * localDeps = NULL;

   // If deps are passed in, we assume that there is a wider scoped set of deps that are
   // handled by the caller.   If the deps are null, we must handle deps for internal control
   // flow ourselves here.
   //
   if (!dep)
      {
      localDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
      localDeps->addPostCondition(targetRegisterPair->getHighOrder(), TR::RealRegister::AssignAny);
      }
   else
      {
      dep->addPostConditionIfNotAlreadyInserted(targetRegisterPair->getHighOrder(), TR::RealRegister::AssignAny);
      }


   // Do complements on reg pair
   generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegisterPair->getHighOrder(), targetRegisterPair->getHighOrder());
   generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegisterPair->getLowOrder(), targetRegisterPair->getLowOrder());


   // Check to see if we need to propagate an overflow bit from low word long to high word long.
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, cFlowRegionEnd);


   // Subtract 1 from high word that was added in first LCGR if low word is not 0, i.e.
   generateRIInstruction(cg, TR::InstOpCode::AGHI, node, targetRegisterPair->getHighOrder(), -1);

   // Not equal, straight through
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, localDeps);
   cFlowRegionEnd->setEndInternalControlFlow();
   return targetRegisterPair;
   }

/**
 * laddConst - adds the long value in targetRegisterPair with the constant value
 */
inline TR::RegisterPair *
laddConst(TR::Node * node, TR::CodeGenerator * cg, TR::RegisterPair * targetRegisterPair, int64_t value, TR::RegisterDependencyConditions * deps)
   {
   TR::Compilation *comp = cg->comp();
   int32_t h_value = (int32_t)(value>>32);
   int32_t l_value = (int32_t)value;

   TR::Register * lowOrder = targetRegisterPair->getLowOrder();
   TR::Register * highOrder = targetRegisterPair->getHighOrder();

   if (performTransformation(comp, "O^O Use AL/ALC to perform long add.\n"))
      {     // wrong carry bit to be set
      if ( l_value != 0 )
         {
         // tempReg is used to hold the immediate on GoldenEagle or better.
         // On other hardware, we use highOrder
         //
         TR::Register * tempReg = cg->allocateRegister();

         generateS390ImmOp(cg, TR::InstOpCode::AL, node, tempReg, lowOrder, l_value, deps);
         generateS390ImmOp(cg, TR::InstOpCode::ALC, node, tempReg, highOrder, h_value, deps);

         if(deps) deps->addPostCondition(tempReg, TR::RealRegister::AssignAny);
         cg->stopUsingRegister(tempReg);
         }
      else
         {
         generateS390ImmOp(cg, TR::InstOpCode::A, node, highOrder, highOrder, h_value);
         }
      }
   else
      {
      // Add high value
      generateS390ImmOp(cg, TR::InstOpCode::A, node, highOrder, highOrder, h_value);

      // Add low value
      if ( l_value != 0 )
         {
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
         TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

         TR::RegisterDependencyConditions * dependencies = NULL;
         // tempReg is used to hold the immediate on GoldenEagle or better.
         // On other hardware, we use highOrder
         //
         TR::Register * tempReg = cg->allocateRegister();
         dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
         dependencies->addPostCondition(tempReg, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(highOrder, TR::RealRegister::AssignAny);

         generateS390ImmOp(cg, TR::InstOpCode::AL, node, tempReg, lowOrder, l_value);

         // Check for overflow in LS(h_value) int. If overflow, increment MS(l_value) int.
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK12, node, cFlowRegionEnd);

         // Increment MS int due to overflow in LS int
         generateRIInstruction(cg, TR::InstOpCode::AHI, node, highOrder, 1);

         cg->stopUsingRegister(tempReg);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
         cFlowRegionEnd->setEndInternalControlFlow();
         }
      }
   return targetRegisterPair;
   }

static bool
genNullTestForCompressedPointers(TR::Node *node, TR::CodeGenerator *cg, TR::Register *targetRegister, TR::LabelSymbol * &skipAdd)
   {
   TR::Compilation *comp = cg->comp();
   bool hasCompressedPointers = false;
   if (cg->comp()->target().is64Bit() &&
         comp->useCompressedPointers() &&
         node->containsCompressionSequence()  /* &&
         !node->isNonZero() */ )
      {
      hasCompressedPointers = true;
//      TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR_SInt32);
      TR::Node *n = node;
      bool isNonZero = false;
      if (n->isNonZero())
        isNonZero = true;

      if ((n->getOpCodeValue() == TR::lshl) ||
          (n->getOpCodeValue() == TR::lushr))
         n = n->getFirstChild();

      TR::Node *addOrSubNode = NULL;
      if (n->getOpCodeValue() == TR::ladd)
         {
         addOrSubNode = n;

         if (n->getFirstChild()->isNonZero())
            isNonZero = true;
         if (n->getFirstChild()->getOpCodeValue() == TR::iu2l || n->getFirstChild()->getOpCode().isShift())
            {
            if (n->getFirstChild()->getFirstChild()->isNonZero())
               isNonZero = true;
            }
         }
      else if (n->getOpCodeValue() == TR::lsub)
         {
         addOrSubNode = n;

         if (n->getFirstChild()->isNonZero())
            isNonZero = true;
         if (n->getFirstChild()->getOpCodeValue() == TR::a2l || n->getFirstChild()->getOpCode().isShift())
            {
            if (n->getFirstChild()->getFirstChild()->isNonNull())
               isNonZero = true;
            }
         }

      if (!isNonZero)
         {

         TR::Instruction *current = cg->getAppendInstruction();
         TR_ASSERT( current != NULL, "Could not get current instruction");

         if (!targetRegister)
            {
            targetRegister = cg->gprClobberEvaluate(addOrSubNode->getFirstChild());
             //targetRegister->incReferenceCount();
            //node->setRegister(targetRegister);
            }

         skipAdd = generateLabelSymbol(cg);
         skipAdd->setEndInternalControlFlow();

         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol( cg);

         if (addOrSubNode->getFirstChild()->getOpCode().isShift() && addOrSubNode->getFirstChild()->getRegister())
            {
            TR::Register* r = addOrSubNode->getFirstChild()->getRegister();
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, r, 0, TR::InstOpCode::COND_BE, skipAdd);
            }
         else
            {
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, targetRegister, 0, TR::InstOpCode::COND_BE, skipAdd);
            }

         cFlowRegionStart->setStartInternalControlFlow();
         }
      }

   return hasCompressedPointers;
   }

/**
 * laddHelper64- add 2 long integers for 64bit platform
 */
TR::Register *
laddHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   TR::LabelSymbol *skipAdd = NULL;

   TR::Node *curTreeNode = cg->getCurrentEvaluationTreeTop()->getNode();
   bool bumpedRefCount = false;

   bool isCompressionSequence = cg->comp()->target().is64Bit() &&
           comp->useCompressedPointers() &&
           node->containsCompressionSequence();

   if (NEED_CC(node) || (node->getOpCodeValue() == TR::luaddc))
      {
      TR_ASSERT( !isCompressionSequence, "CC computation not supported with compression sequence.\n");
      TR_ASSERT(node->getOpCodeValue() == TR::ladd || node->getOpCodeValue() == TR::luaddc,
              "CC computation not supported for this node %p\n", node);

      // we need the carry from integerAddAnalyser for the CC sequence, thus we use logical add instead of add
      // we assume that the analyser will use the opcodes provided and does not generate alternative instructions
      // which do not produce a carry.
      TR_S390BinaryCommutativeAnalyser(cg).integerAddAnalyser(node, TR::InstOpCode::ALGR, TR::InstOpCode::ALG, TR::InstOpCode::LGR);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   if (isCompressionSequence &&
         curTreeNode->getOpCode().isNullCheck() &&
         (curTreeNode->getNullCheckReference()->getOpCodeValue() == TR::l2a) &&
         (curTreeNode->getNullCheckReference()->getFirstChild() == node)  )
      {
      bumpedRefCount = true;
      firstChild->incReferenceCount();
      }

   if (isCompressionSequence && !firstChild->isNonZero())
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);
      }

   TR::Register *secondRegister = NULL;
   if (isCompressionSequence)
      {
      if (isCompressionSequence &&
            (secondChild->getReferenceCount() > 1))
               secondRegister = cg->evaluate(secondChild);
      }

   bool hasCompressedPointers = genNullTestForCompressedPointers(node, cg, targetRegister, skipAdd);

   if (hasCompressedPointers &&
         ((secondChild->getOpCodeValue() != TR::lconst) ||
          (secondChild->getRegister() != NULL)))
      {
      secondRegister = cg->evaluate(secondChild);

      bool addDepForCompressedValue = false;

      TR::MemoryReference * laddMR = generateS390MemoryReference(cg);
      laddMR->populateMemoryReference(firstChild, cg);
      laddMR->populateMemoryReference(secondChild, cg);
      if (!targetRegister)
         targetRegister = cg->allocateRegister();

      if (firstChild->getReferenceCount() > 1 &&
            firstChild->getRegister() &&
            targetRegister != firstChild->getRegister())
         addDepForCompressedValue = true;

      generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister, laddMR);

      node->setRegister(targetRegister);

      if (skipAdd)
         {
         TR::RegisterDependencyConditions *conditions;
         conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);

         conditions->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
         conditions->addPostCondition(secondRegister, TR::RealRegister::AssignAny);
         if (addDepForCompressedValue)
            conditions->addPostCondition(firstChild->getRegister(), TR::RealRegister::AssignAny);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAdd, conditions);
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else if (secondChild->getOpCodeValue() == TR::lconst && secondChild->getRegister() == NULL)
      {
      int64_t long_value = secondChild->getLongInt();

      if (!targetRegister)
         {
         targetRegister = cg->gprClobberEvaluate(firstChild);
         }

      generateS390ImmOp(cg, TR::InstOpCode::AG, node, targetRegister, targetRegister, long_value);
      node->setRegister(targetRegister);

      if (hasCompressedPointers && skipAdd)
         {
         TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
         conditions->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAdd, conditions);
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.integerAddAnalyser(node, TR::InstOpCode::AGR, TR::InstOpCode::AG, TR::InstOpCode::LGR);
      targetRegister = node->getRegister();

      if (hasCompressedPointers && skipAdd)
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAdd);
      }

   if (bumpedRefCount)
      firstChild->decReferenceCount();

   return targetRegister;
   }

/**
 * Check if opcode does not affect AGI
 */
static bool noAGI(TR::InstOpCode & op)
   {
   return (op.isStore()  || op.isLoad() ||op.isBranchOp() ||
         op.getOpCodeValue() == TR::InstOpCode::CHI||
         op.getOpCodeValue() == TR::InstOpCode::CLR||
         op.getOpCodeValue() == TR::InstOpCode::CLGR||
         op.getOpCodeValue() == TR::InstOpCode::CHI||
         op.getOpCodeValue() == TR::InstOpCode::CGHI);
   }

/**
 * Search backwards looking for any preceeding definition of the specified registers.
 * Look backwards iat at most searchLimit instructions.
 * @return Defining instruction if found, NULL if none found
 */
static TR::Instruction *
findPreviousAGIDef(TR::Instruction* prev,uint32_t searchLimit,TR::Register *useA, TR::Compilation * comp)
   {
   bool trace=false;

   static char * skipit = feGetEnv("TR_NOLAYOPT");
   if(skipit) return prev;

   TR::Instruction *curInsn = prev;
   TR_ASSERT(useA ,"null\n");
   TR_ASSERT(prev,"prev is null\n");
   if(trace)traceMsg(comp, "in find at %p, looking for %p for %d\n",prev,useA,searchLimit);
   while(searchLimit-- && curInsn)
      {
      if(trace)traceMsg(comp, "Looking at %p %d\n",curInsn,searchLimit);
      TR::InstOpCode op = curInsn->getOpCode();
      if(op.isAdmin() || op.isLabel())
         {
         ++searchLimit;//skip this guy
         }
      else if(!op.hasBypass() && !noAGI(op) &&
            (useA && curInsn->defsRegister(useA)))
         {
         if(trace)traceMsg(comp, "Def found:%p\n",curInsn);
         return curInsn;
         }

      curInsn = curInsn->getPrev();
      }
   //traceMsg(comp, "none found\n");
   return NULL;
   }

/**
 * Generic addition of 32bits
 * Handles all int types (byte,char,short, and int)
 */
TR::Register *
generic32BitAddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register * targetRegister = NULL;
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();

   if (secondChild->getOpCode().isLoadConst())
      {
      rcount_t refCnt = firstChild->getReferenceCount();

      int32_t value;
      switch (secondChild->getDataType())
         {
         case TR::Int8:
            {
            value = (int32_t) secondChild->getByte();
            break;
            }
         case TR::Int16:
            {
            value = (int32_t) secondChild->getShortInt();
            break;
            }
         case TR::Int32:
            {
            value = secondChild->getInt();
            break;
            }
         default:
            TR_ASSERT_FATAL(false, "Unexpected data type (%s) in generic32BitAddEvaluator", secondChild->getDataType().toString());
            break;
         }

      int32_t delay = cg->machine()->getAGIDelay();
      if (refCnt> 1)
         {
         --delay;
         }

      delay = 2 * delay;  // dual issue

      // AHIK available on z196
      bool useAHIK = false;

      // If negative or large enough range we must use LAY. LA has more restrictions.
      bool useLA = cg->comp()->target().is64Bit() && node->isNonNegative()
         && ((value < MAXLONGDISP && value > MINLONGDISP) || (value >= 0 && value <= MAXDISP));

      TR::Register * childTargetReg = cg->evaluate(firstChild);
      bool canClobberReg = cg->canClobberNodesRegister(firstChild);
      if (!canClobberReg)
         {
         // Make sure all reg props are correctly propagated
         //
         targetRegister = cg->allocateClobberableRegister(childTargetReg);
         }
      else
         {
         targetRegister = childTargetReg;
         }

      if (!findPreviousAGIDef(cg->getAppendInstruction(), delay, childTargetReg, comp))
         {
         useLA = false;
         }

      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
         {
         if (!canClobberReg && (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL))
            {
            useAHIK = true;
            }
         }

      if (useAHIK)
         {
         generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, targetRegister, childTargetReg, value);
         }
      else if (useLA && performTransformation(comp, "O^O Use LA/LAY instead of AHI for %p.\n",node))
         {
         TR::MemoryReference * memRef = generateS390MemoryReference(childTargetReg, value, cg);
         generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister, memRef);
         }
      else
         {
         if (!canClobberReg)
            {
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, targetRegister, childTargetReg);
            }
         generateS390ImmOp(cg, TR::InstOpCode::A, node, targetRegister, targetRegister, value);
         }

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      TR::DataType type = node->getDataType();
      temp.integerAddAnalyser(node, TR::InstOpCode::AR, type == TR::Int16 ? TR::InstOpCode::AH : TR::InstOpCode::A, TR::InstOpCode::LR);
      targetRegister = node->getRegister();
      }

   return targetRegister;
   }

/**
 * Generic subtraction of 32bits
 * This generic subtrac evaluator is invoked from different evaluator functions and handles all subtract types shorter than an int
 * (i.e. byte-bsub, short-ssub, and int-isub)
 *
 * It does not handle long type.
 */
inline TR::Register *
generic32BitSubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Node * secondChild = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst())
      {
      TR::Node * firstChild = node->getFirstChild();
      int32_t value;

      switch (secondChild->getDataType())
         {
         case TR::Int8:
            {
            value = (int32_t) secondChild->getByte();
            break;
            }
         case TR::Int16:
            {
            value = (int32_t) secondChild->getShortInt();
            break;
            }
         case TR::Int32:
            {
            value = secondChild->getInt();
            break;
            }
         default:
            TR_ASSERT_FATAL(false, "Unexpected data type (%s) in generic32BitSubEvaluator", secondChild->getDataType().toString());
            break;
         }

      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196) &&
            firstChild->getRegister() && !cg->canClobberNodesRegister(firstChild) &&
          ((-value) >= MIN_IMMEDIATE_VAL && (-value) <= MAX_IMMEDIATE_VAL))
         {
         TR::Register * sourceRegister = cg->evaluate(firstChild);
         targetRegister = cg->allocateClobberableRegister(sourceRegister);
         generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, targetRegister, sourceRegister, -value);
         }
      else
         {
         targetRegister = cg->gprClobberEvaluate(firstChild);
         generateS390ImmOp(cg, TR::InstOpCode::A, node, targetRegister, targetRegister, -value);
         }

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else // The second child of the sub node isn't a loadConstant type.
      {
      TR_S390BinaryAnalyser temp(cg);
      TR::DataType type = node->getDataType();
      temp.intBinaryAnalyser(node, TR::InstOpCode::SR, type == TR::Int16 ? TR::InstOpCode::SH : TR::InstOpCode::S);
      targetRegister = node->getRegister();
      }

   return targetRegister;
   }

//////////////////////////////////////////////////////////////////////////////////

#define DIVISION   true
#define REMAINDER  false

/**
 * Generate code for lDiv or lRem for 64bit platforms
 * If isDivision = true we return the quotient register.
 *               = false we return the remainder register.
 * algorithm is the same as 32bit iDivRemGenericEvaluator() except the opcodes
 */
inline TR::Register *
lDivRemGenericEvaluator64(TR::Node * node, TR::CodeGenerator * cg, bool isDivision)
   {
   TR::LabelSymbol * doDiv = generateLabelSymbol(cg);
   TR::LabelSymbol * skipDiv = NULL;

   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   int32_t shiftAmnt;
   // A/A, return 1 (div) or 0 (rem).
   if (firstChild == secondChild)
      {
      TR::Register * sourceRegister = cg->evaluate(firstChild);
      TR::Register * returnRegister = cg->allocateRegister();
      int32_t retValue = 0;  // default REM

      if (isDivision)
         {
         retValue = 1;
         }

      generateRIInstruction(cg, TR::InstOpCode::LGHI, node, returnRegister, retValue);

      node->setRegister(returnRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return returnRegister;
      }

   // Constant denoninator:
   //     - unsigned case: handle subset of cases (1, 2**x)
   //     - signed case:   all cases
   bool isUnsigned = node->getOpCode().isUnsigned();
   bool isConstDenominator = secondChild->getOpCode().isLoadConst();
   bool handleConstDenominator = isConstDenominator;
   if (isUnsigned && isConstDenominator)
      {
      int64_t denominator = secondChild->getLongInt();
      handleConstDenominator = (denominator == 1) ||
          // use denoninator > 0 test to bypass the code
          (denominator > 0 && TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(denominator) && isDivision);
      }

   if (handleConstDenominator)
      {
      int64_t denominator = secondChild->getLongInt();

      //case A/-1 Q=-A, R=0
      if (denominator == -1)
         {
         TR::Register * firstRegister = cg->gprClobberEvaluate(firstChild);

         if (!isDivision)  //remainder
            {
            generateLoad32BitConstant(cg, node, 0, firstRegister, true);
            }
         else
            {
            TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
            TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
            TR::LabelSymbol * doDiv = generateLabelSymbol(cg);

            //special case, need to check if dividend is 0x8000000000000000 - but need to check it in 2 steps
            generateS390ImmOp(cg, TR::InstOpCode::CG, node, firstRegister, firstRegister, (int64_t) CONSTANT64(0x8000000000000000));

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            cFlowRegionStart->setStartInternalControlFlow();
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, cFlowRegionEnd);

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doDiv);

            // Do complements on reg
            generateRRInstruction(cg, TR::InstOpCode::LCGR, node, firstRegister, firstRegister);

            TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
            deps->addPostCondition(firstRegister, TR::RealRegister::AssignAny);
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
            cFlowRegionEnd->setEndInternalControlFlow();
            }
         node->setRegister(firstRegister);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return firstRegister;
         }
      //case A/1 Q=A, R=0
      else if (denominator == 1)
         {
         TR::Register * firstRegister = cg->gprClobberEvaluate(firstChild);

         if (!isDivision) //remainder
            {
            generateLoad32BitConstant(cg, node, 0, firstRegister, true);
            }
         node->setRegister(firstRegister);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return firstRegister;
         }
      //case A/(+-)2^x - division is done using shifts
      else if ((shiftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(denominator)) > 0)
         {
         TR::Register * firstRegister = NULL;

         int64_t absValueOfDenominator = denominator>0 ? denominator : -denominator;

         if (isDivision)
            {
            firstRegister = cg->gprClobberEvaluate(firstChild);

            // This adjustment only applies to signed division as we are effectively trying to skirt the sign bit during
            // the shift operation below
            if (!isUnsigned && !firstChild->isNonNegative())
               {
               TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
               TR::LabelSymbol * skipSet = generateLabelSymbol(cg);

               generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
               cFlowRegionStart->setStartInternalControlFlow();
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, firstRegister, 0,TR::InstOpCode::COND_BNL, skipSet);

               //adjustment to dividend if dividend is negative
               TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
               deps->addPostCondition(firstRegister, TR::RealRegister::AssignAny);
               generateS390ImmOp(cg, TR::InstOpCode::AG, node, firstRegister, firstRegister, absValueOfDenominator-1, deps);
               generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipSet, deps);
               skipSet->setEndInternalControlFlow();
               }

            //divide
           if (isUnsigned)
              generateRSInstruction(cg, TR::InstOpCode::SRLG, node, firstRegister, firstRegister, shiftAmnt );
           else
              generateRSInstruction(cg, TR::InstOpCode::SRAG, node, firstRegister, firstRegister, shiftAmnt );

            //fix sign of quotient if necessary
            if (denominator < 0)
               generateRRInstruction(cg, TR::InstOpCode::LCGR, node, firstRegister, firstRegister);
            }
         else //remainder
            {
            firstRegister = cg->gprClobberEvaluate(firstChild);

            TR::LabelSymbol * done = generateLabelSymbol(cg);
            TR::RegisterDependencyConditions *deps = NULL;

            // This adjustment only applies to signed division as we are effectively trying to skirt the sign bit during
            // the shift operation below
            if (!isUnsigned && !firstChild->isNonNegative())
               {
               TR::Register * tempRegister1 = cg->allocateRegister();
               TR::Register * tempRegister2 = cg->allocateRegister();
               generateRSInstruction(cg, TR::InstOpCode::SRAG, node, tempRegister2, firstRegister, 63);
               generateRRInstruction(cg, TR::InstOpCode::LGR, node, tempRegister1, firstRegister);
               generateRILInstruction(cg, TR::InstOpCode::NIHF, node, tempRegister2, static_cast<int32_t>((absValueOfDenominator-1)>>32) );
               generateRILInstruction(cg, TR::InstOpCode::NILF, node, tempRegister2, static_cast<int32_t>(absValueOfDenominator-1));
               generateRRInstruction(cg, TR::InstOpCode::AGR, node, firstRegister, tempRegister2);
               if (denominator != static_cast<int32_t>(absValueOfDenominator))
                  generateRILInstruction(cg, TR::InstOpCode::NIHF, node, firstRegister, static_cast<int32_t>(( CONSTANT64(0xFFFFFFFFFFFFFFFF) - absValueOfDenominator +1)>>32));
               generateRILInstruction(cg, TR::InstOpCode::NILF, node, firstRegister, static_cast<int32_t>( CONSTANT64(0xFFFFFFFFFFFFFFFF) - absValueOfDenominator +1));
               generateRRInstruction(cg, TR::InstOpCode::SGR, node, tempRegister1, firstRegister);
               generateRRInstruction(cg, TR::InstOpCode::LGR, node, firstRegister, tempRegister1);
               cg->stopUsingRegister(tempRegister1);
               cg->stopUsingRegister(tempRegister2);
               }
            else
               {
               generateShiftAndKeepSelected64Bit(node, cg, firstRegister, firstRegister, 64-shiftAmnt, 63, 0, true, false);
               }
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, done, deps);
            }

         node->setRegister(firstRegister);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return firstRegister;
         }
      }

   // DSGR R1,R2  & DSG R1,D(Rx,Rb)
   // R1 represents a reg pair  (eve,odd)=(r1,r1+1)
   // The divisor goes in R1+1
   // After the op is executed,
   //    R1   -> Remainder
   //    R1+1 -> Quotient
   //
   // Note:  D and DR require that R1=0 before executing the division
   //        is this requirement true for DSGR or DSG? -- not mentioned in POP

   uint8_t numPostConditions = 5;
   bool doConditionalRemainder = false;

   TR::Register * sourceRegister = cg->evaluate(secondChild);
   TR::Register *remRegister = NULL;
   TR::Register *quoRegister = NULL;
   if (doConditionalRemainder)
      {
      numPostConditions++; // for sourceRegister (the divisor)
      remRegister = cg->gprClobberEvaluate(firstChild);
      quoRegister = cg->allocateRegister();
      }
   else
      {
      remRegister = cg->allocateRegister();
      quoRegister = cg->gprClobberEvaluate(firstChild);
      }

   // Choose return register based on isDivision flag
   TR::Register * returnRegister = isDivision ? quoRegister : remRegister;

   // Setup dependency for reg pairs
   TR::RegisterPair * targetRegisterPair = cg->allocateConsecutiveRegisterPair(quoRegister, remRegister);
   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numPostConditions, cg);
   dependencies->addPostCondition(targetRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(remRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(quoRegister, TR::RealRegister::LegalOddOfPair);

   // TODO(#3685): Remove this Java-ism from OMR and push it into OpenJ9
   if (!doConditionalRemainder &&
         !node->isSimpleDivCheck() &&
         !isUnsigned &&
         !firstChild->isNonNegative() &&
         !secondChild->isNonNegative() &&
         !(secondChild->getOpCodeValue() == TR::lconst &&
            secondChild->getLongInt() != CONSTANT64(0xFFFFFFFFFFFFFFFF)))
      {
      //  We have to check for a special case where we do 0x8000000000000000/0xffffffffffffffff
      //  JAVA requires we return 0x8000000000000000 rem 0x00000000
      generateS390ImmOp(cg, TR::InstOpCode::CG, node, quoRegister, quoRegister, (int64_t) CONSTANT64(0x8000000000000000));
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doDiv);
      generateS390ImmOp(cg, TR::InstOpCode::CG, node, sourceRegister, sourceRegister, -1);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doDiv);
      genLoadLongConstant(cg, node, 0, remRegister);

      skipDiv = generateLabelSymbol(cg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, skipDiv);

      // Label to do the division
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doDiv);
      }

      TR::LabelSymbol *doneLabel = NULL;
      TR::Register *absDividendReg = NULL;
      bool absDividendRegIsTemp = false;
      if (doConditionalRemainder)
         {
         TR_ASSERT(secondChild->getOpCode().isLoadConst() && secondChild->getType().isInt64(),"secondChild %s (%p) of rem should be int64 constant\n",
            secondChild->getOpCode().getName(),secondChild);
         if (skipDiv == NULL)
            skipDiv = generateLabelSymbol(cg);
         skipDiv->setEndInternalControlFlow();
         dependencies->addPostCondition(sourceRegister, TR::RealRegister::AssignAny);

         if (firstChild->isNonNegative())
            {
            absDividendReg = remRegister;
            }
         else
            {
            absDividendRegIsTemp = true;
            absDividendReg = cg->allocateRegister();
            // LGPR is needed so negative numbers do not always take the slow path after the logical compare (but functionally it is not needed)
            generateRRInstruction(cg, TR::InstOpCode::LPGR, node, absDividendReg, remRegister);
            }

         int64_t value = secondChild->getLongInt();

         if (value <= TR::getMaxSigned<TR::Int32>())
            {
            generateRILInstruction(cg, TR::InstOpCode::CLGFI, node, absDividendReg, static_cast<int32_t>(value));
            }
         else
            {
            generateRRInstruction(cg, TR::InstOpCode::CLGR, node, absDividendReg, sourceRegister);
            }
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK4, node, skipDiv); // branch to done on <
         }

   // Do division
   if (isUnsigned)
      {
      if (doConditionalRemainder)
         generateRRInstruction(cg, TR::InstOpCode::LGR, node, quoRegister, remRegister); // in doConditionalRemainder case this is the slow path so do move here
      generateRIInstruction(cg, TR::InstOpCode::LGHI, node, remRegister, 0);
      generateRRInstruction(cg, TR::InstOpCode::DLGR, node, targetRegisterPair, sourceRegister);
      }
   else
      {
      if (doConditionalRemainder)
         generateRRInstruction(cg, TR::InstOpCode::LGR, node, quoRegister, remRegister); // in doConditionalRemainder case this is the slow path so do move here
      generateRRInstruction(cg, TR::InstOpCode::DSGR, node, targetRegisterPair, sourceRegister);
      }

   // Label to skip the division
   if (skipDiv != NULL)
      {
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipDiv, dependencies);
      }
   else
      {
      generateS390PseudoInstruction(cg, TR::InstOpCode::DEPEND, node, dependencies);
      }

   if (absDividendRegIsTemp && absDividendReg)
      cg->stopUsingRegister(absDividendReg);

   node->setRegister(returnRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(targetRegisterPair);
   return returnRegister;
   }

/**
 * Generate code for iDiv or iRem.
 * If isDivision = true we return the quotient register.
 *               = false we return the remainder register.
 */
TR::Register *
iDivRemGenericEvaluator(TR::Node * node, TR::CodeGenerator * cg, bool isDivision, TR::MemoryReference * divchkDivisorMR)
   {
   TR::LabelSymbol * doDiv = generateLabelSymbol(cg);
   TR::LabelSymbol * skipDiv = NULL;

   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Instruction * cursor = NULL;

   char * REG_USER_DEF  = "LR=Reg_user_def";
   TR_Debug * debugObj = cg->getDebug();

   // A/A, return 1 (div) or 0 (rem).
   if (firstChild == secondChild)
      {
      TR::Register * sourceRegister = cg->evaluate(firstChild);
      TR::Register * returnRegister = cg->allocateRegister();
      int32_t retValue = 0;  // default REM

      if (isDivision)
         {
         retValue = 1;
         }


      generateRIInstruction(cg, TR::InstOpCode::LHI, node, returnRegister, retValue);

      node->setRegister(returnRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return returnRegister;
      }

   // DR R1,R2  & D R1,D(Rx,Rb)
   // R1 represents a reg pair  (eve,odd)=(r1,r1+1)
   // The divisor goes in R1+1
   // After the op is executed,
   //    R1   -> Remainder
   //    R1+1 -> Quotient
   //
   // Note:  D and DR require that R1=0 before executing the division
   // TODO(#3685): Remove this Java-ism from OMR and push it into OpenJ9
   bool needCheck = (!node->isSimpleDivCheck()            &&
                     !node->getOpCode().isUnsigned() &&
                     !firstChild->isNonNegative()         &&
                     !secondChild->isNonNegative());

   TR::MemoryReference * sourceMR = NULL;
   TR::Register * sourceRegister = NULL;
   bool srcRegAllocated = false;
   if (divchkDivisorMR && !needCheck)
      {
      // make a copy of the MR
      sourceMR = generateS390MemoryReference(*divchkDivisorMR, 0, cg);
      }
   else if ( secondChild->getRegister()==NULL && secondChild->getReferenceCount()==1 &&
             secondChild->getOpCode().isMemoryReference() &&
             !needCheck)
      {
      sourceMR = generateS390MemoryReference(secondChild, cg);
      }
   else
      {
      if (divchkDivisorMR)
         {
         TR::MemoryReference * tempMR = generateS390MemoryReference(*divchkDivisorMR, 0, cg);
         TR::DataType dtype = secondChild->getType();
         sourceRegister = cg->allocateRegister();
         generateRXInstruction(cg, dtype.isInt64()? TR::InstOpCode::LG : TR::InstOpCode::L, secondChild, sourceRegister, tempMR);
         secondChild->setRegister(sourceRegister);
         srcRegAllocated = true;
         tempMR->stopUsingMemRefRegister(cg);
         }
      else
         {
         sourceRegister = cg->evaluate(secondChild);
         }
      }
   TR::Register * quoRegister = cg->allocateRegister();
   TR::Register * remRegister = cg->gprClobberEvaluate(firstChild);

   // Choose return register based on isDivision flag
   TR::Register * returnRegister = remRegister;
   if (isDivision)
      {
      returnRegister = quoRegister;
      }

   // Setup dependency for reg pairs
   TR::RegisterPair * targetRegisterPair = cg->allocateConsecutiveRegisterPair(quoRegister, remRegister);

   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, sourceMR ? 6 : 4, cg);
   dependencies->addPostCondition(targetRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(remRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(quoRegister, TR::RealRegister::LegalOddOfPair);
   if(sourceRegister) dependencies->addPostConditionIfNotAlreadyInserted(sourceRegister, TR::RealRegister::AssignAny);
   if(sourceMR && sourceMR->getBaseRegister() && !sourceMR->getBaseRegister()->getRealRegister())
     dependencies->addPostConditionIfNotAlreadyInserted(sourceMR->getBaseRegister(), TR::RealRegister::AssignAny);
   if(sourceMR && sourceMR->getIndexRegister() && !sourceMR->getIndexRegister()->getRealRegister())
     dependencies->addPostConditionIfNotAlreadyInserted(sourceMR->getIndexRegister(), TR::RealRegister::AssignAny);


   //  We have to check for a special case where we do 0x80000000/0xffffffff
   //  JAVA requires we return 0x80000000 rem 0x00000000
   if (needCheck)
      {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

      generateS390ImmOp(cg, TR::InstOpCode::C, node, remRegister, remRegister, (int32_t) 0x80000000);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doDiv);

      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, sourceRegister, -1, TR::InstOpCode::COND_BNE, doDiv);
      cursor =
         generateRRInstruction(cg, TR::InstOpCode::LR, node, quoRegister, remRegister);

      if (debugObj)
         {
         debugObj->addInstructionComment( toS390RRInstruction(cursor),  REG_USER_DEF);
         }

      // TODO: Can we allow setting the condition code here by moving the load before the compare?
      generateLoad32BitConstant(cg, node, 0, remRegister, false);

      skipDiv = generateLabelSymbol(cg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, skipDiv);

      // Label to do the division
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doDiv);
      }

   // We have to setup high word
   if (node->getOpCode().isUnsigned())
      generateRSInstruction(cg, TR::InstOpCode::SRDL, node, targetRegisterPair, 32);
   else
      generateRSInstruction(cg, TR::InstOpCode::SRDA, node, targetRegisterPair, 32);

   // Do division
   if (sourceMR)
      {
      if (node->getOpCode().isUnsigned())
         generateRXInstruction(cg, TR::InstOpCode::DL, node, targetRegisterPair, sourceMR);
      else
         generateRXInstruction(cg, TR::InstOpCode::D, node, targetRegisterPair, sourceMR);
      }
   else
      {
      if (node->getOpCode().isUnsigned())
         generateRRInstruction(cg, TR::InstOpCode::DLR, node, targetRegisterPair, sourceRegister);
      else
         generateRRInstruction(cg, TR::InstOpCode::DR, node, targetRegisterPair, sourceRegister);
      }

   // Label to skip the division
   if (skipDiv)
     {
     // Add a dependency to make the register assignment a pair
     generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipDiv, dependencies);
     skipDiv->setEndInternalControlFlow();
     }
   else
     generateS390PseudoInstruction(cg, TR::InstOpCode::DEPEND, node, dependencies);

   node->setRegister(returnRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(targetRegisterPair);
   if (srcRegAllocated)
      {
      cg->stopUsingRegister(sourceRegister);
      }
   return returnRegister;
   }

inline TR::Register *
genericLongShiftSingle(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic shiftOp)
   {
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = NULL;
   TR::Register * trgReg = NULL;
   TR::Register * src2Reg = NULL;
   TR::MemoryReference * tempMR = NULL;

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = secondChild->getInt();

      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
         {
         // Generate RISBG for lshl + i2l sequence
         if (node->getOpCodeValue() == TR::lshl)
            {
            if (firstChild->getOpCodeValue() == TR::i2l && firstChild->isSingleRefUnevaluated() && (firstChild->isNonNegative() || firstChild->getFirstChild()->isNonNegative()))
               {
               srcReg = cg->evaluate(firstChild->getFirstChild());
               trgReg = cg->allocateRegister();
               auto mnemonic = cg->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;

               generateRIEInstruction(cg, mnemonic, node, trgReg, srcReg, (int8_t)(32-value), (int8_t)((63-value)|0x80), (int8_t)value);

               node->setRegister(trgReg);

               cg->decReferenceCount(firstChild->getFirstChild());
               cg->decReferenceCount(firstChild);
               cg->decReferenceCount(secondChild);

               return trgReg;
               }
            else if (firstChild->getOpCodeValue() == TR::land)
               {
               if (trgReg = TR::TreeEvaluator::tryToReplaceShiftLandWithRotateInstruction(firstChild, cg, value, node->getOpCodeValue() == TR::lshl))
                  {
                  node->setRegister(trgReg);
                  cg->decReferenceCount(firstChild);
                  cg->decReferenceCount(secondChild);
                  return trgReg;
                  }
               }
            }
         else if (node->getOpCodeValue() == TR::lshr || node->getOpCodeValue() == TR::lushr)
            {
            // Generate RISBGN for (lshr + land) and (lushr + land) sequences
            if (firstChild->getOpCodeValue() == TR::land)
               {
               if (trgReg = TR::TreeEvaluator::tryToReplaceShiftLandWithRotateInstruction(firstChild, cg, -value, node->getOpCodeValue() == TR::lshr))
                  {
                  node->setRegister(trgReg);
                  cg->decReferenceCount(firstChild);
                  cg->decReferenceCount(secondChild);
                  return trgReg;
                  }
               }
            }
         }

      srcReg = cg->evaluate(firstChild);
      trgReg = cg->allocateRegister();
      if ((value & 0x3f) == 0)
         {
         generateRRInstruction(cg, TR::InstOpCode::LGR, node, trgReg, srcReg);
         node->setRegister(trgReg);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return  trgReg;
         }
      generateRSInstruction(cg, shiftOp, node, trgReg, srcReg, value & 0x3f);
      }
   else
      {
      srcReg = cg->evaluate(firstChild);
      trgReg = cg->allocateRegister();
      TR::Register * src2Reg;
      // Mask by 63 is redundant since SRLG will only use 6 bits anyway
      bool skippedAnd = false;
      if ((shiftOp == TR::InstOpCode::SRLG ||
           shiftOp == TR::InstOpCode::SLLG) &&
          secondChild->isSingleRefUnevaluated() &&
          secondChild->getOpCode().isAnd() && secondChild->getOpCode().isInteger() &&
          secondChild->getSecondChild()->getOpCode().isLoadConst() &&
          secondChild->getSecondChild()->getConst<int64_t>() == 63)
         {
         src2Reg = cg->evaluate(secondChild->getFirstChild());
         cg->recursivelyDecReferenceCount(secondChild->getSecondChild());
         skippedAnd = true;
         }
      else
         {
         src2Reg = cg->evaluate(secondChild);
         }

      TR::MemoryReference * tempMR = generateS390MemoryReference(src2Reg, 0, cg);
      generateRSInstruction(cg, shiftOp, node, trgReg, srcReg, tempMR);
      tempMR->stopUsingMemRefRegister(cg);
      if (skippedAnd)
         cg->decReferenceCount(secondChild->getFirstChild());
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

inline TR::Register *
genericIntShift(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic shiftOp, TR::InstOpCode::Mnemonic altShiftOp)
   {
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * trgReg = NULL;
   TR::Register * srcReg = NULL;
   TR::Register * src2Reg = NULL;
   TR::MemoryReference * tempMR = NULL;
   TR::Compilation *comp = cg->comp();

   bool canUseAltShiftOp = true;

   int32_t refCnt = 0;
   int32_t shiftAmount = -1;

   // If we are on z/Architecture, then we can use SLLG even in 31-bit mode for doing a
   // shift. Only generate SLLG over SLL if it saves an instruction - an LR to copy a
   // register - because we need a non-destructive op.
   // Same goes for SLA/SLAG.
   // Right shift may be dangerous as upper half of register may have garbage
   // hense don't use SRLG, SRAG.

   if (shiftOp == TR::InstOpCode::SRL || shiftOp == TR::InstOpCode::SRA)
      {
      canUseAltShiftOp = altShiftOp == TR::InstOpCode::SRLK || altShiftOp == TR::InstOpCode::SRAK;
      }

   if (node->getOpCodeValue() == TR::bushr)
      {
      if (node->getFirstChild()->getOpCodeValue() == TR::bconst &&
          node->getReferenceCount() == 1 &&
          node->getFirstChild()->getRegister() == NULL)
         {
         srcReg = node->getFirstChild()->setRegister(cg->allocateRegister());
         generateLoad32BitConstant(cg, node->getFirstChild(), node->getFirstChild()->getByte() & 0xFF, srcReg, true);
         }
      else
         {
         // we need to worry about something like:
         // bushr
         //    badd
         //       bconst 0x80 (==-128)
         //       bconst 1
         // we must clear the all but the bottom byte of srcReg
         srcReg = cg->evaluate(firstChild);
         generateS390ImmOp(cg, TR::InstOpCode::N, node, srcReg, srcReg, 0xff);
         }
      }
   else
      {
      srcReg = cg->evaluate(firstChild);
      if (node->getOpCodeValue() == TR::sushr)
         {
         cg->clearHighOrderBits(node, srcReg, 16);
         }
      }
   refCnt = firstChild->getReferenceCount();
   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = secondChild->getInt();
      shiftAmount = value & 0x1f;
      }

   if ( !cg->canClobberNodesRegister(firstChild) ||
        srcReg->containsCollectedReference() ||
        ((refCnt == 1) && canUseAltShiftOp && (shiftAmount != 0)) )
      {
      trgReg = cg->allocateRegister();
      if (!canUseAltShiftOp || shiftAmount == 0)
         {
         TR::InstOpCode::Mnemonic loadRegOpCode = TR::InstOpCode::getLoadRegOpCodeFromNode(cg, node);

         generateRRInstruction(cg, loadRegOpCode, node, trgReg, srcReg);
         }
      }
   else
      {
      trgReg = srcReg;
      }

   if (secondChild->getOpCode().isLoadConst())
      {
      if (shiftAmount != 0)
         {
         if (trgReg != srcReg && canUseAltShiftOp )
            {
            generateRSInstruction(cg, altShiftOp, node, trgReg, srcReg, shiftAmount);
            }
         else
            {
            generateRSInstruction(cg, shiftOp, node, trgReg, shiftAmount);
            }
         }
      }
   else
      {
      // NILL instruction is destructive so always use gprClobberEvaluate
      src2Reg = cg->gprClobberEvaluate(secondChild);

      // Java Spec defines the shift amount for integer shifts to be masked with
      // 0x1f (0-31).
      generateRIInstruction(cg, TR::InstOpCode::NILL, node, src2Reg, 0x1f);

      TR::MemoryReference * tempMR = generateS390MemoryReference(src2Reg, 0, cg);
      if (trgReg != srcReg && canUseAltShiftOp )
         {
         generateRSInstruction(cg, altShiftOp, node, trgReg, srcReg, tempMR);
         }
      else
         {
         generateRSInstruction(cg, shiftOp, node, trgReg, tempMR);
         }
      tempMR->stopUsingMemRefRegister(cg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }


/**
 * iaddEvaluator - add 2 integers
 *    - also handles TR::aiadd
 */
TR::Register *
OMR::Z::TreeEvaluator::iaddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generic32BitAddEvaluator(node, cg);
   }

/**
 * laddEvaluator - add 2 long integers
 */
TR::Register *
OMR::Z::TreeEvaluator::laddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return laddHelper64(node, cg);
   }

TR::Register *
genericRotateLeft(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      if (node->getOpCodeValue() == TR::lor)
         {
         TR::Node * lushr = NULL;
         TR::Node * lshl  = NULL;

         if (firstChild->getOpCodeValue() == TR::lushr)
            {
            lushr = firstChild;
            lshl  = secondChild;
            }
         else if (secondChild->getOpCodeValue() == TR::lushr)
            {
            lushr = secondChild;
            lshl  = firstChild;
            }

         if (lushr &&
             lushr->getOpCodeValue() == TR::lushr && lushr->getReferenceCount() == 1 && lushr->getRegister() == NULL &&
             lshl->getOpCodeValue()  == TR::lshl &&   lshl->getReferenceCount() == 1 &&  lshl->getRegister() == NULL)
            {
            int32_t rShftAmnt = lushr->getSecondChild()->getInt();
            int32_t lShftAmnt = lshl->getSecondChild()->getInt();
            TR::Node * lshlSourceNode  = lshl->getFirstChild();
            TR::Node * lushrSourceNode = lushr->getFirstChild();

            if (rShftAmnt + lShftAmnt == 64 && lshlSourceNode == lushrSourceNode)
               {
               TR::Register * targetReg = NULL;
               TR::Register * sourceReg = cg->evaluate(lshlSourceNode);

               if (!cg->canClobberNodesRegister(lshlSourceNode))
                  {
                  targetReg = cg->allocateClobberableRegister(sourceReg);
                  }
               else
                  {
                  targetReg = sourceReg;
                  }

               TR::InstOpCode::Mnemonic opCode = cg->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;
                  generateRIEInstruction(cg, opCode, node, targetReg, sourceReg, 0, 63, lShftAmnt);

               // Clean up skipped nodes
               cg->decReferenceCount(lushrSourceNode);
               cg->decReferenceCount(lushr->getSecondChild());
               cg->decReferenceCount(lshlSourceNode);
               cg->decReferenceCount(lshl->getSecondChild());
               return targetReg;
               }
            }
         }

      TR::Node* andChild = NULL;
      TR::Node* shiftChild = NULL;
      if (firstChild->getOpCodeValue() == TR::land)
         {
         andChild = firstChild;
         shiftChild = secondChild;
         }
      else if (secondChild->getOpCodeValue() == TR::land)
         {
         andChild = secondChild;
         shiftChild = firstChild;
         }
      if (node->getOpCodeValue() == TR::lor &&
          andChild &&
          andChild->getRegister() == NULL &&
          andChild->getReferenceCount() == 1)
         {
         TR::Node* data = NULL;
         uint64_t shiftBy = 0;
         uint64_t mask1 = 0;
         uint64_t mask2 = 0;
         uint64_t bitPos = 0;

         if (shiftChild->getOpCodeValue() == TR::lshl &&
             shiftChild->getRegister() == NULL &&
             shiftChild->getReferenceCount() == 1 &&
             shiftChild->getSecondChild()->getOpCode().isLoadConst())
            {
            data = shiftChild->getFirstChild();
            shiftBy = shiftChild->getSecondChild()->getLongInt();
            mask1 = 1ULL << shiftBy;
            if (data->getOpCodeValue() == TR::i2l &&
                data->getRegister() == NULL)
               data = data->getFirstChild();
            if (data->getOpCodeValue() != TR::icmpeq &&
                data->getOpCodeValue() != TR::icmpne)
               mask1 = 0;
            }

         if (andChild->getSecondChild()->getOpCode().isLoadConst())
            mask2 = andChild->getSecondChild()->getLongInt();
         bitPos = 63 - shiftBy;

         if (mask1 && mask1 == (~mask2) && shiftBy <= 63 && performTransformation(cg->comp(), "O^O Insert bit using RISBG node [%p]\n", node))
            {
            TR::Node* otherData = andChild->getFirstChild();
            TR::Register* toReg = cg->evaluate(otherData);
            TR::Register* fromReg = cg->evaluate(data);
            TR::Register* targetReg = cg->allocateRegister();

            generateRRInstruction(cg, TR::InstOpCode::LGR, node, targetReg, toReg);
            generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, targetReg, fromReg, bitPos, bitPos, shiftBy);
            cg->decReferenceCount(andChild->getSecondChild());
            cg->decReferenceCount(shiftChild->getSecondChild());
            cg->decReferenceCount(otherData);
            cg->decReferenceCount(data);
            if (shiftChild->getFirstChild()->getOpCodeValue() == TR::i2l)
               cg->decReferenceCount(shiftChild->getFirstChild());

            return targetReg;
            }
         else if (mask2 && shiftBy)
            {
            mask1 = (1ULL << shiftBy) - 1;
            if (mask1 == mask2 && performTransformation(cg->comp(), "O^O Insert contiguous bits using RISBG node [%p]\n", node))
               {
               TR::Register* toReg = cg->evaluate(andChild->getFirstChild());
               TR::Register* fromReg = cg->evaluate(shiftChild->getFirstChild());
               TR::Register* targetReg = cg->allocateRegister();
               generateRRInstruction(cg, TR::InstOpCode::LGR, node, targetReg, toReg);
               generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, targetReg, fromReg, 0, bitPos, shiftBy);
               cg->decReferenceCount(andChild->getFirstChild());
               cg->decReferenceCount(shiftChild->getFirstChild());
               cg->decReferenceCount(andChild->getSecondChild());
               cg->decReferenceCount(shiftChild->getSecondChild());
               return targetReg;
               }
            }
         }

      if (node->getOpCodeValue() == TR::lor ||
          node->getOpCodeValue() == TR::lxor)
         {
         TR::Node* shiftChild = NULL;
         TR::Node* otherChild = NULL;
         if (firstChild->getOpCodeValue() == TR::lshl ||
             firstChild->getOpCodeValue() == TR::lushr)
            {
            shiftChild = firstChild;
            otherChild = secondChild;
            }
         else if (secondChild->getOpCodeValue() == TR::lshl ||
                  secondChild->getOpCodeValue() == TR::lushr)
            {
            shiftChild = secondChild;
            otherChild = firstChild;
            }
         if (shiftChild &&
             shiftChild->getSecondChild()->getOpCode().isLoadConst() &&
             shiftChild->isSingleRefUnevaluated() &&
             performTransformation(cg->comp(), "O^O Combine or/shift into rotate node [%p]\n", node))
            {
            uint32_t shiftBy = shiftChild->getSecondChild()->getInt();
            uint32_t firstBit = 0;
            uint32_t lastBit = 63;
            if (shiftChild->getOpCodeValue() == TR::lushr)
               {
               firstBit = shiftBy;
               shiftBy = 64 - shiftBy;
               }
            else
               lastBit = 63 - shiftBy;
            TR::Register* toReg = cg->evaluate(otherChild);
            TR::Register* fromReg = cg->evaluate(shiftChild->getFirstChild());
            TR::Register* targetReg = cg->allocateRegister();
            TR::InstOpCode::Mnemonic opcode = TR::InstOpCode::ROSBG;
            if (node->getOpCodeValue() == TR::lxor)
               opcode = TR::InstOpCode::RXSBG;
            generateRRInstruction(cg, TR::InstOpCode::LGR, node, targetReg, toReg);
            generateRIEInstruction(cg, opcode, node, targetReg, fromReg, firstBit, lastBit, shiftBy);
            cg->decReferenceCount(shiftChild->getFirstChild());
            cg->decReferenceCount(shiftChild->getSecondChild());
            return targetReg;
            }
         }
      }

   return NULL;
   }


/**
 * Rotate and insert
 */
TR::Register *
genericRotateAndInsertHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      TR::Node * firstChild = node->getFirstChild();
      TR::Node * secondChild = node->getSecondChild();
      TR::Node * skipedConversion = NULL;
      TR::Compilation *comp = cg->comp();

      if (firstChild->getOpCode().isConversion())
         {
         if (firstChild->getReferenceCount() > 1)
            {
            return NULL;
            }
         if (!firstChild->getFirstChild()->getType().isIntegral()) // only skip integer to integer conversions
            {
            return NULL;
            }
         skipedConversion = firstChild;
         firstChild = firstChild->getFirstChild();
         }

      // Turn this pattern into a rotate
      // iand
      //    iush[r|l]
      //      ...
      //      iconts 6
      //    iconst 4     <<<<<<< Must be consecutive 1's
      //
      if (node->getOpCode().isAnd() &&
          (firstChild->getOpCode().isLeftShift() || firstChild->getOpCode().isRightShift()) &&
          firstChild->getRegister() == NULL && firstChild->getReferenceCount() <= 1 &&
          secondChild->getOpCode().isLoadConst() &&
          firstChild->getSecondChild()->getOpCode().isLoadConst())
         {
         uint64_t value = 0;

         // Mask
         switch (secondChild->getDataType())
            {
            case TR::Address:
               TR_ASSERT( cg->comp()->target().is32Bit(),"genericRotateAndInsertHelper: unexpected data type");
            case TR::Int64:
               value = (uint64_t) secondChild->getLongInt();
               break;
            case TR::Int32:
               value = ((uint64_t) secondChild->getInt()) & 0x00000000FFFFFFFFL;  //avoid sign extension
               break;
            case TR::Int16:
               value = ((uint64_t) secondChild->getShortInt()) & 0x000000000000FFFFL;
               break;
            case TR::Int8:
               value = ((uint64_t) secondChild->getByte()) & 0x00000000000000FFL;
               break;
            default:
               TR_ASSERT( 0,"genericRotateAndInsertHelper: Unexpected Type\n");
               break;
            }

         int32_t length;
         if (firstChild->getType().isInt8())
            length = 8;
         else if (firstChild->getType().isInt16())
            length = 16;
         else if (firstChild->getType().isInt32())
            length = 32;
         else
            length = 64;

         int32_t tZeros = trailingZeroes(value);
         int32_t lZeros = leadingZeroes(value);
         int32_t popCnt = populationCount(value);
         int32_t shiftAmnt = (firstChild->getSecondChild()->getInt()) % length;
         int32_t msBit = lZeros;
         int32_t lsBit = 63 - tZeros;
         int32_t shiftMsBit = 64 - length;
         int32_t shiftLsBit = 63;
         int useOrder = 0; //For l2i conversions in 31-bit, 1 = lowOrder, 2 = highOrder, 0 = neither

         TR::Register *sourceReg = cg->evaluate(firstChild->getFirstChild());

         if (firstChild->getOpCode().isRightShift())
            {
            shiftMsBit += shiftAmnt;
            // Turn right shift into left shift
            shiftAmnt = 64 - shiftAmnt;
            }
         else
            {
            shiftLsBit -= shiftAmnt;
            }

         if (shiftMsBit > msBit) //Conditions for when we don't need any sign extension
            {
            if (firstChild->getFirstChild()->isNonNegative() || firstChild->chkUnsigned() || firstChild->getOpCodeValue() == TR::lushr)
               msBit = shiftMsBit;
            else
               {
               traceMsg(comp, "Cannot use RISBG, number could be negative, no sign extension available for RISBG\n");
               return NULL;
               }
            }
         if (lsBit > shiftLsBit)
            lsBit = shiftLsBit;

         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp,"[%p] and/sh[r,l] => rotated-and-insert: tZeros %d, lZeros %d, popCnt %d\n", node, tZeros, lZeros, popCnt);
            traceMsg(comp,"\t               => rotated-and-insert: shiftMsBit %d, shiftLsBit %d \n", shiftMsBit, shiftLsBit);
            traceMsg(comp,"\t               => rotated-and-insert: msBit %d, lsBit %d \n", msBit, lsBit);
            }

         // Make sure we have consecutive 1's
         //
         if (popCnt == (64 - lZeros - tZeros))
            {
            TR::Register * targetReg = NULL;

            if (!cg->canClobberNodesRegister(firstChild->getFirstChild()))
               targetReg = cg->allocateClobberableRegister(sourceReg);
            else
               targetReg = sourceReg;

            if (msBit > lsBit)
               {
               if (node->getType().isInt64())
                  {
                  generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetReg, targetReg);
                  }
               else
                  {
                  generateRRInstruction(cg, TR::InstOpCode::XR, node, targetReg, targetReg);
                  }
               }
            else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196) && !node->getType().isInt64())
               {
               generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetReg, sourceReg, msBit, 0x80 + lsBit, shiftAmnt);
               }
            else
               {
               auto mnemonic = cg->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;

               generateRIEInstruction(cg, mnemonic, node, targetReg, sourceReg, msBit, 0x80 + lsBit, shiftAmnt);
               }

            // Clean up skipped node
            firstChild->setRegister(sourceReg);
            cg->decReferenceCount(firstChild->getFirstChild());
            cg->decReferenceCount(firstChild->getSecondChild());

            if (skipedConversion)
               {
               skipedConversion->setRegister(sourceReg);
               cg->decReferenceCount(firstChild);
               }
            return targetReg;
            }
         }
      }

   return NULL;
   }

TR::Register *
OMR::Z::TreeEvaluator::tryToReplaceShiftLandWithRotateInstruction(TR::Node * node, TR::CodeGenerator * cg, int32_t shiftAmount, bool isSignedShift)
   {
   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      TR::Node * firstChild = node->getFirstChild();
      TR::Node * secondChild = node->getSecondChild();
      TR::Compilation *comp = cg->comp();

      // Given the right cases, transform land into RISBG as it may be better than discrete pair of ands
      if (node->getOpCodeValue() == TR::land && secondChild->getOpCode().isLoadConst())
         {
         uint64_t longConstValue = secondChild->getLongInt();

         // RISBG instructions perform unsigned rotation, so if we are selecting the sign bit via the logical AND we cannot
         // perform a signed shift because we must preserve the sign bit. In general we cannot handle this case because we are
         // not certain our input will be non-negative, hence we must conservatively disallow the optimization in such a case.
         if (longConstValue >= 0x8000000000000000LL && shiftAmount != 0 && isSignedShift)
            {
            return NULL;
            }
         // The Shift amount operand in RISBG only holds 6 bits so the optimization will not work on shift amounts >63 or < 63.
         if (shiftAmount > 63 || shiftAmount < -63)
            {
            return NULL;
            }
         // If the node entered is a common node, then there is no guarantee that the child of the common node will still be alive.
         // And when we try to access the value in that node, we might receive values from a node that might've been killed already.
         // Thus, the optimization will not work.
         if (node->getRegister() != NULL)
            {
            return NULL;
            }
         int32_t tZeros = trailingZeroes(longConstValue);
         int32_t lZeros = leadingZeroes(longConstValue);
         int32_t tOnes  = trailingZeroes(~longConstValue);
         int32_t lOnes  = leadingZeroes(~longConstValue);
         int32_t popCnt = populationCount(longConstValue);

         // if the population count is 0 then the result of the AND will also be 0,
         // because a popCnt=0 means there are no overlapping bits between the two AND
         // operands. We cannot generate a RISBG for this case as RISBG cannot be
         // used to zero out the contents of the entire register. The starting and ending
         // positions are inclusive, so even if start=end, we will preserve at least one bit.
         if (popCnt == 0)
            return NULL;

         // the minimum value for the bit positions is 0, and lsBit can equal msBit iff popCnt = 1
         // if popCnt == 0 then we cannot generate a RISBG instruction because RISBG preserves at
         // least one bit position
         int32_t msBit = 0;
         int32_t lsBit = 0;

         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp,"[%p] land => rotated-and-insert: tZeros %d, lZeros %d, popCnt %d\n", node, tZeros, lZeros, popCnt);
            traceMsg(comp,"          => rotated-and-insert: lOnes %d, tOnes %d\n", lOnes, tOnes);
            }

         bool doTransformation = false;

         // We now try to detect if the lconst's bit pattern is such that we can use a single RISBG
         // instead of multiple AND instructions

         // The first block will catch all cases where the lConstValue is a series of contiguous ones
         // surrounded by zeros
         // ex: 0001111000, 011110, 111111, 001111, 11100 etc...
         //
         // The 'else if' block will consider all cases where we have a series of contiguous zeros
         // surrounded by ones
         // ex: 1110000111, 1000001
         if (popCnt == (64 - lZeros - tZeros))
            {
            msBit = lZeros;
            lsBit = 63 - tZeros;
            // The below checks make sure we are not better off using one of Z architecture's
            // 16-bit AND immediate instructions (NILF,NILH,NILL, NIHF,NIHH, NIHL).
            // Details for each check below:
            //
            // "firstChild->getReferenceCount() > 1"
            //    If the firstChild's refCount>1, we will need to clobber evaluate this case
            //    because we cannot destroy the value. In such a case we're better off with
            //    RISBG because RISBG won't clobber anything and hence can do the operation
            //    faster.
            //
            // "|| lZeros > 31 || tZeros > 31"
            //    If there are more than 31 contiguous zeros, then we must use RISBG because
            //    no NI** instruction do zero out that many bits in a single instruction.
            //
            // "|| (lZeros > 0 && tZeros > 0"
            //    NI** instructions need to explicitly specify which bit positions to zero out.
            //    If our constant at minimum looks like 0111...110 (there are zeros in the top
            //    half and bottom half of the register), then it's better to use RISBG.
            //    This is because there are no NI** instructions allowing us to specify bits in
            //    the top half and bottom half of the register to zero out.
               
            if (firstChild->getReferenceCount() > 1 || lZeros > 31 || tZeros > 31
                || (lZeros > 0 && tZeros > 0))
               {
               // If the shift amount is NOT zero, then we must account for the shifted range
               // [1] If the resultant shifted msb in a right shift is shifted "too far" to the right, then we are basically zeroing the register. This optimization will not be used.
               // [2] Same case if the resultant lsb of a left shift is shifted out of range.
               // Eg[1]: 000...0011 >> 3. msb = 62, lsb = 63. If we right shift, then msb and lsb will both be > 63 (out of range)
               // Eg[2]: 11100...00 << 10. msb = 0, lsb = 2. If we left shift, then msb and lsb will both be < 0 (out of range)
               if ((shiftAmount < 0 && msBit - shiftAmount > 63) || (shiftAmount > 0 && lsBit - shiftAmount < 0))
                  {
                  return NULL;
                  }
               doTransformation = true;
               }
            }
         else if (popCnt == (lOnes + tOnes))
            {
            msBit = 64 - tOnes;
            lsBit = lOnes - 1;
            // The below checks make sure we are not better off using one of Z architecture's
            // 16-bit AND immediate instructions (NILF,NILH,NILL, NIHF,NIHH, NIHL).
            // Details for each check below:
            //
            // "firstChild->getReferenceCount() > 1"
            //    same as above
            //
            // "(lOnes < 32 && tOnes < 32)"
            //    This case implies there are zeros at the high/low boundary of the register.
            //    Example value: 11111000011111. The NI** instructions can only operate on
            //    the top half or bottom half of a register at a time. So such a case
            //    will require two NI** instructions. Hence we are better off using a single RISBG
            //
            // "shiftAmount == 0"
            //    For cases where we have zeros surrounded by one, if the shift value is not zero
            //    then the optimization will not work.

            if ((firstChild->getReferenceCount()> 1 || (lOnes < 32 && tOnes < 32)) && shiftAmount == 0)
               {
               // Similar to above, if the shift amount is NOT zero, then we must account for the shifted range
               if ((shiftAmount < 0 && msBit - shiftAmount > 63) || (shiftAmount > 0 && lsBit - shiftAmount < 0))
                  {
                  return NULL;
                  }
               doTransformation = true;
               }
            }

         if (doTransformation && performTransformation(comp, "O^O Use RISBG instead of 2 ANDs for %p.\n", node))
            {
            TR::Register * targetReg = cg->allocateRegister();
            TR::Register * sourceReg = cg->evaluate(firstChild);

            // if possible then use the instruction that doesn't set the CC as it's faster
            TR::InstOpCode::Mnemonic opCode = cg->comp()->target().cpu.getSupportsArch(TR::CPU::zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;

            // If the shift amount is zero, this instruction sets the rotation factor to 0 and sets the zero bit(0x80).
            // So it's effectively zeroing out every bit except the inclusive range of lsBit to msBit.
            // The bits in that range are preserved as is.
            //
            // If the shift amount is NOT zero, then we must account for the shifted range
            // [1] If the shifted lsb of a left shift is still in range, but the msb is out of range, then we can still "capture" the shifted bits by upperbounding the msb.
            // [2] Similar case if the msb of a right shift is still in range but the lsb is out of range.
            //
            // Eg[1]: 00...0111 >> 2. msb = 61, lsb = 63. If we right shift, we get 000...0001, so we can still preserve the last bit.
            // Eg[2]: 1110...00 << 2. msb = 0, lsb = 2. If we left left, we can still preserve the first bit.

            int32_t rangeStart = msBit;
            int32_t rangeEnd = lsBit;

            if (shiftAmount < 0)
               {
               rangeStart = msBit - shiftAmount;
               if (lsBit - shiftAmount > 63)
                  {
                  rangeEnd = 63;
                  }
               else
                  {
                  rangeEnd = lsBit - shiftAmount;
                  }
               }
            else if (shiftAmount > 0)
               {                  
               rangeEnd = lsBit - shiftAmount;
               if (msBit - shiftAmount < 0)
                  {
                  rangeStart = 0;
                  }
               else
                  {
                  rangeStart = msBit - shiftAmount;
                  }
               }
            if (shiftAmount < 0)
               {
               shiftAmount = 64 + shiftAmount;
               }
            generateRIEInstruction(cg, opCode, node, targetReg, sourceReg, rangeStart, 0x80 + rangeEnd, shiftAmount);
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
            return targetReg;
            }
         }
      }
   return NULL;
   }

////////////////////////////////////////////////////////////////////////////////////////

/**
 * lsubHelper64 - subtract 2 long integers (child1 - child2) for 64Bit codegen
 */
TR::Register *
lsubHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node * secondChild = node->getSecondChild();
   TR::Register * targetRegister = NULL;

   TR::Node * firstChild = node->getFirstChild();
   /*
      if (firstChild->getRegister())
      targetRegister = firstChild->getRegister();
      else
      targetRegister = cg->evaluate(firstChild);
    */
   TR::Node *curTreeNode = cg->getCurrentEvaluationTreeTop()->getNode();

   bool isCompressionSequence = false;
   if (cg->comp()->target().is64Bit() &&
         comp->useCompressedPointers() &&
         node->containsCompressionSequence())
      isCompressionSequence = true;

   if (NEED_CC(node) || (node->getOpCodeValue() == TR::lusubb))
      {
      TR_ASSERT( !isCompressionSequence,"CC computation not supported with compression sequence.\n");
      TR_ASSERT(node->getOpCodeValue() == TR::lsub || node->getOpCodeValue() == TR::lusubb,
              "CC computation not supported for this node %p\n", node);

      // we need the borrow from longSubtractAnalyser for the CC sequence,
      // and we assume that the analyser does not generate alternative instructions
      // which do not produce a borrow.
      TR_S390BinaryAnalyser(cg).longSubtractAnalyser(node);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   bool bumpedRefCount = false;
   if (isCompressionSequence &&
         curTreeNode->getOpCode().isNullCheck() &&
         (firstChild->getOpCodeValue() == TR::a2l) &&
         (firstChild->getFirstChild() == curTreeNode->getNullCheckReference()))
      {
      bumpedRefCount = true;
      firstChild->getFirstChild()->incReferenceCount();
      }

   if (isCompressionSequence)
      targetRegister = cg->gprClobberEvaluate(firstChild);

   TR::Register *secondRegister = NULL;
   if (isCompressionSequence &&
         (secondChild->getReferenceCount() > 1) /*
                                                   ((secondChild->getOpCodeValue() != TR::lconst) ||
                                                   (secondChild->getRegister() != NULL)) */ )
            secondRegister = cg->evaluate(secondChild);

   TR::LabelSymbol *skipAdd = NULL;
   bool hasCompressedPointers = genNullTestForCompressedPointers(node, cg, targetRegister, skipAdd);


   if (hasCompressedPointers &&
         ((secondChild->getOpCodeValue() != TR::lconst) ||
          (secondChild->getRegister() != NULL)))
      {
      /*
         TR::MemoryReference * laddMR = generateS390MemoryReference(cg);
         laddMR->populateAddTree(node, cg);
         laddMR->eliminateNegativeDisplacement(node, cg);
         laddMR->enforceDisplacementLimit(node, cg);

         generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister, laddMR);
       */

      secondRegister = cg->evaluate(secondChild);
      generateRRInstruction(cg, TR::InstOpCode::SGR, node, targetRegister, secondRegister);

      node->setRegister(targetRegister);

      if (skipAdd)
         {
         TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
         conditions->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
         conditions->addPostCondition(secondRegister, TR::RealRegister::AssignAny);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAdd, conditions);
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else if (secondChild->getOpCodeValue() == TR::lconst && secondChild->getRegister() == NULL)
      {
      int64_t long_value = secondChild->getLongInt();

      long_value = -long_value;

      if (!targetRegister)
         targetRegister = cg->gprClobberEvaluate(firstChild);

      generateS390ImmOp(cg, TR::InstOpCode::AG, node, targetRegister, targetRegister, long_value);

      if (hasCompressedPointers && skipAdd)
         {
         TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
         conditions->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAdd, conditions);
         }

      node->setRegister(targetRegister);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else if(firstChild->getOpCodeValue() == TR::lconst && firstChild->getRegister() == NULL &&
           secondChild->getRegister() != NULL)
      {
      // This is the case of: #constant - index;
      // This case is when the first operand is constant and the second operand
      // is in a register. We can save one register and one instruction.

      int64_t long_value = firstChild->getLongInt();

      if (!targetRegister)
         targetRegister = cg->gprClobberEvaluate(secondChild);

      generateRRInstruction(cg, TR::InstOpCode::LCGR, secondChild, targetRegister, targetRegister);
      generateS390ImmOp(cg, TR::InstOpCode::AG, node, targetRegister, targetRegister, long_value);

      if (hasCompressedPointers && skipAdd)
         {
         TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
         conditions->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAdd, conditions);
         }

      node->setRegister(targetRegister);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_S390BinaryAnalyser temp(cg);
      temp.longSubtractAnalyser(node);
      targetRegister = node->getRegister();

      if (hasCompressedPointers && skipAdd)
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipAdd);

      }

   if (bumpedRefCount)
      firstChild->getFirstChild()->decReferenceCount();

   return targetRegister;
   }

/**
 * dualMulHelper64- 64bit version of dual multiply Helper
 */
TR::Register *
OMR::Z::TreeEvaluator::dualMulHelper64(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg)
   {
   bool needsUnsignedHighMulOnly = (lmulNode == NULL);
   // Both parts of multiplication required if !needsUnsignedHighMulOnly
   // targetHigh:targetLow <-- firstChild * secondChild
   //
   // MLGR R1, R2 does this, where targetHigh = R1, targetLow = R1+1, firstChild = R1+1, secondChild = R2
   //
   // firstChild is overwritten, secondChild is unchanged (unless it is R1 also?)
   //
   // ignore whether children are constant or zero, which may be suboptimal
   TR::Node * firstChild =  lumulhNode->getFirstChild();
   TR::Node * secondChild = lumulhNode->getSecondChild();

   // adapted from unsigned imulEvaluator
   TR::Register * secondRegister = cg->evaluate(secondChild);
   TR::Instruction * cursor = NULL;
   TR::Register * lmulTargetRegister = cg->gprClobberEvaluate(firstChild);
   TR::Register * lumulhTargetRegister = cg->allocateRegister();
   TR::RegisterPair * trgtRegPair = cg->allocateConsecutiveRegisterPair(lmulTargetRegister, lumulhTargetRegister);

   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);
   dependencies->addPostCondition(trgtRegPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(trgtRegPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(trgtRegPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

   // do multiplication
   // TODO: sometimes reg-mem may be more efficient?
   cursor = generateRRInstruction(cg, TR::InstOpCode::MLGR, node, trgtRegPair, secondRegister);
   cursor->setDependencyConditions(dependencies);

   if (!needsUnsignedHighMulOnly)
      {
      lmulNode->setRegister(lmulTargetRegister);
      }
   else
      {
      cg->stopUsingRegister(lmulTargetRegister);
      }

   lumulhNode->setRegister(lumulhTargetRegister);
   cg->stopUsingRegister(trgtRegPair);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return node->getRegister();
   }

/**
 * dualMulEvaluator - evaluator for quad precision multiply using dual operators
 */
TR::Register *
OMR::Z::TreeEvaluator::dualMulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   bool needsUnsignedHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   TR_ASSERT( (node->getOpCodeValue() == TR::lumulh) || (node->getOpCodeValue() == TR::lmul),"Unexpected operator. Expected lumulh or lmul.");
   TR_ASSERT( node->isDualCyclic() || needsUnsignedHighMulOnly, "Should be either calculating cyclic dual or just the high part of the lmul.");
   if (node->isDualCyclic() && node->getChild(2)->getReferenceCount() == 1)
      {
      // other part of this dual is not used, and is dead
      TR::Node *pair = node->getChild(2);
      // break dual into parts before evaluation
      // pair has only one reference, so need to avoid recursiveness removal of its subtree
      pair->incReferenceCount();
      node->removeChild(2);
      pair->removeChild(2);
      cg->decReferenceCount(pair->getFirstChild());
      cg->decReferenceCount(pair->getSecondChild());
      cg->decReferenceCount(pair);
      // evaluate this part again
      return cg->evaluate(node);
      }
   else
      {
      TR::Node *lmulNode;
      TR::Node *lumulhNode;
      if (!needsUnsignedHighMulOnly)
         {
         traceMsg(comp, "Found lmul/lumulh for node = %p\n", node);
         lmulNode = (node->getOpCodeValue() == TR::lmul) ? node : node->getChild(2);
         lumulhNode = lmulNode->getChild(2);
         TR_ASSERT((lumulhNode->getReferenceCount() > 1) && (lmulNode->getReferenceCount() > 1),
               "Expected both lumulh and lmul have external references.");
         // we only evaluate the lumulh children, and internal cycle does not indicate evaluation
         cg->decReferenceCount(lmulNode->getFirstChild());
         cg->decReferenceCount(lmulNode->getSecondChild());
         cg->decReferenceCount(lmulNode->getChild(2));
         cg->decReferenceCount(lumulhNode->getChild(2));
         }
      else
         {
         diagnostic("Found lumulh only node = %p\n", node);
         lumulhNode = node;
         lmulNode = NULL;
         }

      return TR::TreeEvaluator::dualMulHelper64(node, lmulNode, lumulhNode, cg);
      }
   }

/**
 * lmulHelper64- 64bit version of long multiply Helper
 */
TR::Register *
lmulHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   TR::Register * targetRegister = NULL;
   TR::Register * sourceRegister = NULL;

   // Determine if second child is constant.  There are special tricks for these.
   bool secondChildIsConstant = (secondChild->getOpCodeValue() == TR::lconst);

   if (!secondChildIsConstant && firstChild->isHighWordZero() && !secondChild->isHighWordZero())
      {
      // swap children pointers since optimizer puts highWordZero children as firstChild if the
      // second one is non-constant and non-highWordZero
      TR::Node * tempChild = firstChild;

      firstChild  = secondChild;
      secondChild = tempChild;
      }

   if (secondChildIsConstant)
      {
      int64_t value = secondChild->getLongInt();

      // LA Ry,(Rx,Rx) is the best instruction scale array index by 2
      // because it will create AGI of only 1 cycle, instead of 5 as SLL
      // but we need to make sure that  LA Ry,(Rx,Rx) itself does not create
      // AGI with preceeding instruction. Hence the check if FirstChild
      // has already been evaluated (by checking if it has a register).

      bool create_LA = false;
      if (firstChild->getRegister() != NULL &&
          value == 2 &&
          cg->comp()->target().is64Bit()) // 3 way AGEN LA is cracked on zG
         {
         create_LA = true;
         }

      sourceRegister = cg->evaluate(firstChild);
      bool canClobber = cg->canClobberNodesRegister(firstChild);
      targetRegister = !canClobber ? cg->allocateRegister() : sourceRegister;

      if (create_LA)
         {
         TR::MemoryReference * interimMemoryReference = generateS390MemoryReference(cg);
         interimMemoryReference->setBaseRegister(sourceRegister, cg);
         interimMemoryReference->setIndexRegister(sourceRegister);
         generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister, interimMemoryReference);
         }
      else
         {
         generateS390ImmOp(cg, TR::InstOpCode::MSG, node, sourceRegister, targetRegister, value);
         }

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::MSGR, TR::InstOpCode::MSG, TR::InstOpCode::LGR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::baddEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* lhsChild = node->getChild(0);
   TR::Node* rhsChild = node->getChild(1);

   // We don't have an instruction which adds a source register with a target memory reference, so force the
   // evaluation of both chlidren here and pass the BAD mnemonic for the register-to-memory operand to the generic
   // analyzer to ensure it is never generated
   cg->evaluate(lhsChild);
   cg->evaluate(rhsChild);

   TR_S390BinaryCommutativeAnalyser temp(cg);
   temp.genericAnalyser(node, TR::InstOpCode::AR, TR::InstOpCode::BAD, TR::InstOpCode::LR);

   cg->decReferenceCount(lhsChild);
   cg->decReferenceCount(rhsChild);

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::saddEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* lhsChild = node->getChild(0);
   TR::Node* rhsChild = node->getChild(1);

   TR_S390BinaryCommutativeAnalyser temp(cg);
   temp.genericAnalyser(node, TR::InstOpCode::AR, TR::InstOpCode::AH, TR::InstOpCode::LR);

   cg->decReferenceCount(lhsChild);
   cg->decReferenceCount(rhsChild);

   return node->getRegister();
   }

/**
 * caddEvaluator - unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::caddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generic32BitAddEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::bsubEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* lhsChild = node->getChild(0);
   TR::Node* rhsChild = node->getChild(1);

   // We don't have an instruction which adds a source register with a target memory reference, so force the
   // evaluation of both chlidren here and pass the BAD mnemonic for the register-to-memory operand to the generic
   // analyzer to ensure it is never generated
   cg->evaluate(lhsChild);
   cg->evaluate(rhsChild);

   TR_S390BinaryAnalyser temp(cg);
   temp.genericAnalyser(node, TR::InstOpCode::SR, TR::InstOpCode::BAD, TR::InstOpCode::LR);

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::ssubEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* lhsChild = node->getChild(0);
   TR::Node* rhsChild = node->getChild(1);

   TR_S390BinaryAnalyser temp(cg);
   temp.genericAnalyser(node, TR::InstOpCode::SR, TR::InstOpCode::SH, TR::InstOpCode::LR);

   return node->getRegister();
   }

/**
 * isubEvaluator - subtract 2 integers or subtract a short from an integer
 * (child1 - child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::isubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if ((node->getOpCodeValue() == TR::asub) && cg->comp()->target().is64Bit())
      {
      return lsubHelper64(node, cg);
      }
   else
      {
      return generic32BitSubEvaluator(node, cg);
      }
   }

/**
 * lsubEvaluator - subtract 2 long integers
 * (child1 - child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::lsubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return lsubHelper64(node, cg);
   }

/**
 * csubEvaluator - subtract 2 unsigned short integers
 * (child1 - child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::csubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generic32BitSubEvaluator(node, cg);
   }

/**
 * lmulhEvaluator - multiply 2 long words but the result is the high word
 */
TR::Register *
OMR::Z::TreeEvaluator::lmulhEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   bool needsUnsignedHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   if (node->isDualCyclic() || needsUnsignedHighMulOnly)
      {
      return TR::TreeEvaluator::dualMulEvaluator(node, cg);
      }

   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Register * firstRegister = cg->gprClobberEvaluate(firstChild);
   TR::Register * targetRegister = cg->allocateRegister();
   TR::Register * sourceRegister = firstRegister;
   TR::Compilation *comp = cg->comp();

   TR::RegisterDependencyConditions * dependencies =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
   TR::RegisterPair * targetRegisterPair = cg->allocateConsecutiveRegisterPair(sourceRegister, targetRegister);

   dependencies->addPostCondition(targetRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(targetRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(sourceRegister, TR::RealRegister::LegalOddOfPair);

   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      int64_t value = secondChild->getLongInt();
      int64_t absValue = value > 0 ? value : -value;

      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * posMulh = generateLabelSymbol(cg);
      TR::LabelSymbol * doneMulh = generateLabelSymbol(cg);

      // positive first child, branch to posMulh label
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, sourceRegister, 0, TR::InstOpCode::COND_BNL, posMulh);

      // Negative first child, do complements on register
      generateRRInstruction(cg, TR::InstOpCode::LCGR, node, sourceRegister, sourceRegister);

      // MLG
      generateS390ImmOp(cg, TR::InstOpCode::MLG, node, targetRegisterPair, targetRegisterPair, absValue, dependencies);

      // Undo earlier complement
      targetRegisterPair = lnegFor128Bit(node, cg, targetRegisterPair, dependencies);

      // Branch to done
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneMulh);

      // Label for positive first child
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, posMulh);

      generateS390ImmOp(cg, TR::InstOpCode::MLG, node, targetRegisterPair, targetRegisterPair, absValue, dependencies);

      // Label for done
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneMulh, dependencies);
      doneMulh->setEndInternalControlFlow();

      // second child is negative
      if (value < 0)
         {
         targetRegisterPair = lnegFor128Bit( node, cg, targetRegisterPair, dependencies);
         }
      }
   else
      {
      TR_ASSERT( 0,"TR::TreeEvaluator::lmulhEvaluator -- RegReg not implemented for lmulh.");
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(targetRegisterPair);
   return targetRegister;
   }

/**
 * imulhEvaluator - multiply 2 words but the result is the high word.
 *
 * Handles imulh and iumulh.
 *
 */
TR::Register *
OMR::Z::TreeEvaluator::mulhEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   bool isUnsigned = (node->getOpCodeValue() == TR::iumulh);
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Register * firstRegister = cg->gprClobberEvaluate(firstChild);
   TR::Register * targetRegister = cg->allocateRegister();
   TR::Instruction * cursor = NULL;

   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);

   TR::RegisterPair * targetRegisterPair = cg->allocateConsecutiveRegisterPair(firstRegister, targetRegister);
   dependencies->addPostCondition(targetRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(targetRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(firstRegister, TR::RealRegister::LegalOddOfPair);

   if(secondChild->getOpCode().isLoadConst() && secondChild->getInt() == 0)
      {
      cursor = generateRRInstruction(cg, TR::InstOpCode::XR, node, targetRegister, targetRegister);
      }
   else
      {
      TR::Register * secondRegister = cg->evaluate(secondChild);
      if(isUnsigned)
         {
         cursor = generateRREInstruction(cg, TR::InstOpCode::MLR, node, targetRegisterPair, secondRegister);
         }
      else
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::MR, node, targetRegisterPair, secondRegister);
         }
      }

   cursor->setDependencyConditions(dependencies);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(targetRegisterPair);
   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::bmulEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* lhsChild = node->getChild(0);
   TR::Node* rhsChild = node->getChild(1);

   // We don't have an instruction which multiplies a source register with a target memory reference, so force the
   // evaluation of both chlidren here and pass the BAD mnemonic for the register-to-memory operand to the generic
   // analyzer to ensure it is never generated
   cg->evaluate(lhsChild);
   cg->evaluate(rhsChild);

   TR_S390BinaryCommutativeAnalyser temp(cg);
   temp.genericAnalyser(node, TR::InstOpCode::MSR, TR::InstOpCode::BAD, TR::InstOpCode::LR);

   cg->decReferenceCount(lhsChild);
   cg->decReferenceCount(rhsChild);

   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::smulEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* lhsChild = node->getChild(0);
   TR::Node* rhsChild = node->getChild(1);

   TR_S390BinaryCommutativeAnalyser temp(cg);
   temp.genericAnalyser(node, TR::InstOpCode::MSR, TR::InstOpCode::MH, TR::InstOpCode::LR);

   cg->decReferenceCount(lhsChild);
   cg->decReferenceCount(rhsChild);

   return node->getRegister();
   }

/**
 * imulEvaluator - multiply 2 integers
 */
TR::Register *
OMR::Z::TreeEvaluator::imulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node* secondChild = node->getSecondChild();
   TR::Node* firstChild = node->getFirstChild();
   TR::Node* halfwordNode = NULL;
   TR::Node* regNode = NULL;

   TR::Register * targetRegister = NULL;
   TR::Register * sourceRegister = NULL;
   bool isMultHalf = false;

   if(firstChild->getOpCodeValue() == TR::s2i &&
      firstChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
      firstChild->isSingleRefUnevaluated() &&
      firstChild->getFirstChild()->isSingleRefUnevaluated())
      {
      isMultHalf = true;
      halfwordNode = firstChild;
      regNode = secondChild;
      }
   else if(secondChild->getOpCodeValue() == TR::s2i &&
           secondChild->getFirstChild()->getOpCodeValue() == TR::sloadi &&
           secondChild->isSingleRefUnevaluated() &&
           secondChild->getFirstChild()->isSingleRefUnevaluated())
      {
      isMultHalf = true;
      halfwordNode = secondChild;
      regNode = firstChild;
      }

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = secondChild->getInt();

      // LA Ry,(Rx,Rx) is the best instruction scale array index by 2
      // because it will create AGI of only 1 cycle, instead of 5 as SLL
      // but we need to make sure that  LA Ry,(Rx,Rx) itself does not create
      // AGI with preceeding instruction. Hence the check if FirstChild
      // has already been evaluated (by checking if it has a register).

      bool create_LA = false;
      if ((firstChild->getRegister() != NULL) && (value == 2) && (node->isNonNegative() && cg->comp()->target().is64Bit()))
         {
         create_LA = true;
         }

      sourceRegister = cg->evaluate(firstChild);
      bool canClobber = cg->canClobberNodesRegister(firstChild);
      if (!canClobber)
         {
         targetRegister = cg->allocateRegister();
         }
      else
         {
         targetRegister = sourceRegister;
         }

      if (create_LA)
         {
         TR::MemoryReference * interimMemoryReference = generateS390MemoryReference(cg);
         interimMemoryReference->setBaseRegister(sourceRegister, cg);
         interimMemoryReference->setIndexRegister(sourceRegister);
         generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister, interimMemoryReference);
         }
      else
         {
         generateS390ImmOp(cg, TR::InstOpCode::MS, node, sourceRegister, targetRegister, value);
         }

      // This hack is to compensate bad IL tree from escape analysis to compute hashcode
      //  imul
      //     loadAddr
      //     iconst
      // When refCount is 1, We can't use srcReg as targetReg, because the targetReg must not be
      // collected reference reg
      if (targetRegister->containsCollectedReference())
         {
         TR::Register * tmpReg = cg->allocateRegister();
         generateRRInstruction(cg, TR::InstOpCode::LR, node, tmpReg, targetRegister);

         cg->stopUsingRegister(targetRegister);
         targetRegister=tmpReg;
         }

      node->setRegister(targetRegister);
      }
   else if(isMultHalf)
      {
      // Emit MH directly
      targetRegister = cg->evaluate(regNode);
      TR::MemoryReference* tmpMR = generateS390MemoryReference(halfwordNode->getFirstChild(), cg);
      generateRXInstruction(cg, TR::InstOpCode::MH, node, targetRegister, tmpMR);
      node->setRegister(targetRegister);
      tmpMR->stopUsingMemRefRegister(cg);
      cg->decReferenceCount(halfwordNode->getFirstChild());
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::MSR, TR::InstOpCode::MS, TR::InstOpCode::LR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

/**
 * lmulEvaluator - multiply 2 long integers
 *
 * This implementation is functionally correct, but certainly not optimal.
 * We can do a lot more stuff if one of the params is a const.
 */
TR::Register *
OMR::Z::TreeEvaluator::lmulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   bool needsUnsignedHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   if (node->isDualCyclic() || needsUnsignedHighMulOnly)
      {
      return TR::TreeEvaluator::dualMulEvaluator(node, cg);
      }

   return lmulHelper64(node, cg);
   }

/**
 * idivEvaluator -  divide 2 integers
 * (child1 / child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::idivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Register * targetRegister = NULL;


   if (secondChild->getOpCode().isLoadConst())
      {
      TR::RegisterPair * targetRegisterPair = NULL;
      int32_t value = secondChild->getInt();
      targetRegister = cg->gprClobberEvaluate(firstChild);
      int32_t shftAmnt = -1;

      if (value == -1 && !node->getOpCode().isUnsigned())
         {
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
         TR::LabelSymbol * doDiv = generateLabelSymbol(cg);
         TR::LabelSymbol * skipDiv = generateLabelSymbol(cg);

         // If the divisor is -1 we need to check the dividend for 0x80000000
         generateS390ImmOp(cg, TR::InstOpCode::C, node, targetRegister, targetRegister, (int32_t )0x80000000);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doDiv);

         TR::RegisterDependencyConditions * dependencies
           = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
         dependencies->addPostCondition(targetRegister, TR::RealRegister::AssignAny);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, skipDiv);

         // Label to do the division
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doDiv);

         // Do division by -1 (take complement)
         generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, targetRegister);

         // Label to skip the division
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipDiv, dependencies);
         skipDiv->setEndInternalControlFlow();
         }
      else if ((shftAmnt = TR::TreeEvaluator::checkNonNegativePowerOfTwo(value)) > 0)
         {
         // Strength reduction
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
         TR::LabelSymbol * doShift = generateLabelSymbol(cg);

         if (!firstChild->isNonNegative() && !node->getOpCode().isUnsigned())
            {
            TR::RegisterDependencyConditions * dependencies
               = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
            dependencies->addPostCondition(targetRegister, TR::RealRegister::AssignAny);

            // load and test
            generateRRInstruction(cg, TR::InstOpCode::LTR, node, targetRegister, targetRegister);

            // if positive value, branch to doShift
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            cFlowRegionStart->setStartInternalControlFlow();
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, doShift);

            // add (divisor-1) before the shift if negative value
            generateS390ImmOp(cg, TR::InstOpCode::A, node, targetRegister, targetRegister, value-1, dependencies);

            // Label to do the shift
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doShift, dependencies);
            doShift->setEndInternalControlFlow();
            }

         // do the division by shifting
         if (node->getOpCode().isUnsigned())
            {
            generateRSInstruction(cg, TR::InstOpCode::SRL, node, targetRegister, shftAmnt);
            }
         else
            {
            generateRSInstruction(cg, TR::InstOpCode::SRA, node, targetRegister, shftAmnt);
            }
         }
      else
         {
         // Division with no exception check
         TR::Register * tmpReg = NULL;
         TR::Register * quoRegister = cg->allocateRegister();
         bool useTmp = false;
         // Create dependency for DR inst function requirement

         targetRegisterPair = cg->allocateConsecutiveRegisterPair(quoRegister, targetRegister);
         if (secondChild->getRegister() == NULL)
            {
            useTmp = true;
            tmpReg = cg->allocateRegister();
            generateLoad32BitConstant(cg, secondChild, value, tmpReg, true);
            }
         else
            {
            tmpReg = secondChild->getRegister();
            }

         if (node->getOpCode().isUnsigned())
            generateRSInstruction(cg, TR::InstOpCode::SRDL, node, targetRegisterPair, 32);
         else
            generateRSInstruction(cg, TR::InstOpCode::SRDA, node, targetRegisterPair, 32);

         if (node->getOpCode().isUnsigned())
            generateRRInstruction(cg, TR::InstOpCode::DLR, node, targetRegisterPair, tmpReg);
         else
            generateRRInstruction(cg, TR::InstOpCode::DR, node, targetRegisterPair, tmpReg);

         targetRegister = quoRegister;

         if (useTmp)
            cg->stopUsingRegister(tmpReg);
         }

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      if (targetRegisterPair)
         {
         cg->stopUsingRegister(targetRegisterPair);
         }
      }
   else
      {
      targetRegister = iDivRemGenericEvaluator(node, cg, DIVISION, NULL);
      }
   return targetRegister;
   }

/**
 * ldivEvaluator -  divide 2 long integers
 * (child1 / child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::ldivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return lDivRemGenericEvaluator64(node, cg, DIVISION);
   }

/**
 * bdivEvaluator -  divide 2 bytes
 * (child1 / child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bdivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

/**
 * sdivEvaluator -  divide 2 short integers
 * (child1 / child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::sdivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

/**
 * iremEvaluator -  remainder of 2 integers
 * (child1 % child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::iremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Register * targetRegister = NULL;
   bool isUnsigned = node->getOpCode().isUnsigned();

   if (secondChild->getOpCode().isLoadConst())
      {
      TR::Register * quoRegister = NULL;
      TR::RegisterPair * targetRegisterPair = NULL;
      int32_t value = secondChild->getInt();
      targetRegister = cg->gprClobberEvaluate(firstChild);
      int32_t shftAmnt = -1;

      if (!isUnsigned && (value == -1))
         {
         // Remainder of division by -1 is always 0.
         generateRRInstruction(cg, TR::InstOpCode::XR, node, targetRegister, targetRegister);
         }
      else if ((!isUnsigned || isNonNegativePowerOf2(value)) &&
         ((shftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(value)) > 0))
         {
         TR::LabelSymbol * done = generateLabelSymbol(cg);
         TR::RegisterDependencyConditions *deps = NULL;

         if (!isUnsigned && !firstChild->isNonNegative())
            {
            int32_t absValue = value>0 ? value : -value;
            TR::Register * tempRegister1 = cg->allocateRegister();
            TR::Register * tempRegister2 = cg->allocateRegister();
            generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegister2, targetRegister);
            generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegister1, targetRegister);
            generateRSInstruction(cg, TR::InstOpCode::SRA, node, tempRegister2, 31);
            generateRILInstruction(cg, TR::InstOpCode::NILF, node, tempRegister2, absValue-1);
            generateRRInstruction(cg, TR::InstOpCode::AR, node, tempRegister1, tempRegister2);
            generateRILInstruction(cg, TR::InstOpCode::NILF, node, tempRegister1, 0xFFFFFFFF-absValue+1);
            generateRRInstruction(cg, TR::InstOpCode::SR, node, targetRegister, tempRegister1);
            cg->stopUsingRegister(tempRegister1);
            cg->stopUsingRegister(tempRegister2);
            }
         else
            {
            generateShiftAndKeepSelected31Bit(node, cg, targetRegister, targetRegister, 0x3f & (32 - shftAmnt), 31, 0, true, false);
            }
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, done, deps);
         }
      else
         {
         // Division with no exception check
         TR::Register * tmpReg = NULL;
         bool useTmp = false;
         quoRegister = cg->allocateRegister();

         targetRegisterPair = cg->allocateConsecutiveRegisterPair(quoRegister, targetRegister);

         if (secondChild->getRegister() == NULL)
            {
            useTmp = true;
            tmpReg = cg->allocateRegister();
            }
         else
            {
            tmpReg = secondChild->getRegister();
            }

         TR::LabelSymbol *cFlowRegionEnd = NULL;
         TR::Register *absDividendReg = NULL;
         bool absDividendRegIsTemp = false;
         TR::RegisterDependencyConditions *deps = NULL;

         generateLoad32BitConstant(cg, secondChild, value, tmpReg, true);

         if (node->getOpCode().isUnsigned())
            generateRSInstruction(cg, TR::InstOpCode::SRDL, node, targetRegisterPair, 32);
         else
            generateRSInstruction(cg, TR::InstOpCode::SRDA, node, targetRegisterPair, 32);

         if (node->getOpCode().isUnsigned())
            generateRRInstruction(cg, TR::InstOpCode::DLR, node, targetRegisterPair, tmpReg);
         else
            generateRRInstruction(cg, TR::InstOpCode::DR, node, targetRegisterPair, tmpReg);

         if (useTmp)
            cg->stopUsingRegister(tmpReg);

         if (absDividendRegIsTemp && absDividendReg)
            cg->stopUsingRegister(absDividendReg);

         if (cFlowRegionEnd)
            {
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
            cFlowRegionEnd->setEndInternalControlFlow();
            }
         }
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      if (targetRegisterPair)
         {
         cg->stopUsingRegister(targetRegisterPair);
         }
      }
   else
      {
      targetRegister = iDivRemGenericEvaluator(node, cg, REMAINDER, NULL);
      }
   return targetRegister;
   }

/**
 * lremEvaluator -  remainder of 2 long integers
 * (child1 % child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::lremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return lDivRemGenericEvaluator64(node, cg, REMAINDER);
   }

/**
 * bremEvaluator -  remainder of 2 bytes
 * (child1 % child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

/**
 * sremEvaluator -  remainder of 2 short integers
 * (child1 % child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::sremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

/**
 * inegEvaluator - negate an integer
 */
TR::Register *
OMR::Z::TreeEvaluator::inegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * sourceRegister;
   TR::Register * targetRegister = cg->allocateRegister();

   // ineg over iabs is an integer form of pdSetSign 0xd (negative); handle it in one operation
   if (firstChild->getOpCodeValue() == TR::iabs && firstChild->getReferenceCount() == 1 && firstChild->getRegister() == NULL)
      {
      sourceRegister = firstChild->getFirstChild()->getRegister();
      if (sourceRegister == NULL)
         sourceRegister = cg->evaluate(firstChild->getFirstChild());
      cg->decReferenceCount(firstChild->getFirstChild());

      // Load Negative
      if (cg->comp()->target().is64Bit() && targetRegister->alreadySignExtended())
         generateRRInstruction(cg, TR::InstOpCode::LNGR, node, targetRegister, sourceRegister);
      else
         generateRRInstruction(cg, TR::InstOpCode::LNR, node, targetRegister, sourceRegister);
      }
   else if (firstChild->getOpCodeValue() == TR::imul &&
            firstChild->getSecondChild()->getOpCode().isLoadConst() &&
            firstChild->getSecondChild()->getInt() != 0x80000000 &&
            firstChild->getReferenceCount() == 1 &&
            firstChild->getRegister() == NULL &&
            performTransformation(cg->comp(), "O^O Replace ineg/imul by const with imul by -const.\n"))
      {
      TR::Node* oldConst = firstChild->getSecondChild();
      int32_t val = oldConst->getInt();
      TR::Node* newConst = TR::Node::create(node, TR::iconst, 0, -val);
      cg->decReferenceCount(oldConst);
      firstChild->setAndIncChild(1, newConst);
      cg->stopUsingRegister(targetRegister);
      targetRegister = cg->evaluate(firstChild);
      }
   else
      {
      sourceRegister = cg->evaluate(firstChild);

      // Do complement
      if (cg->comp()->target().is64Bit() && targetRegister->alreadySignExtended())
         generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegister, sourceRegister);
      else
         generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, sourceRegister);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }


/**
 * lnegEvaluator - negate a long integer
 */
TR::Register *
OMR::Z::TreeEvaluator::lnegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   TR::Register * sourceRegister;
   targetRegister = cg->allocateRegister();

      if (firstChild->getOpCodeValue() == TR::labs && firstChild->getReferenceCount() == 1 && firstChild->getRegister() == NULL)
      {
      // Load Negative
      sourceRegister = firstChild->getFirstChild()->getRegister();
      if (sourceRegister == NULL)
         sourceRegister = cg->evaluate(firstChild->getFirstChild());
      cg->decReferenceCount(firstChild->getFirstChild());
      generateRRInstruction(cg, TR::InstOpCode::LNGR, node, targetRegister, sourceRegister);
      }
   else
      {
      // Do complement
      sourceRegister = cg->evaluate(firstChild);
      generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegister, sourceRegister);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * bnegEvaluator - negate a byte
 */
TR::Register *
OMR::Z::TreeEvaluator::bnegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * sourceRegister = cg->evaluate(firstChild);
   TR::Register * targetRegister = cg->allocateRegister();

   // Load complement
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, sourceRegister);

   // LL: Sign extended high order 24 bits in register.
   cg->signExtendedHighOrderBits(node, targetRegister, 24);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * snegEvaluator - negate a short integer
 */
TR::Register *
OMR::Z::TreeEvaluator::snegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * sourceRegister = cg->evaluate(firstChild);
   TR::Register * targetRegister = cg->allocateRegister();

   // Load complement
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, sourceRegister);

   // LL: Sign extended high order 16 bits in register.
   // This code path can only be generated by the optimizer, but currently it is not.
   cg->signExtendedHighOrderBits(node, targetRegister, 16);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * ishlEvaluator - shift integer left
 * (child1 << child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::ishlEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   auto altShiftOp = TR::InstOpCode::SLLG;

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      altShiftOp = TR::InstOpCode::SLLK;
      }

   return genericIntShift(node, cg, TR::InstOpCode::SLL, altShiftOp);
   }

/**
 * lshlEvaluator - shift long integer left
 * (child1 << child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::lshlEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return genericLongShiftSingle(node, cg, TR::InstOpCode::SLLG);
   }

/**
 * bshlEvaluator - shift byte left
 * (child1 << child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bshlEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   auto altShiftOp = TR::InstOpCode::SLLG;

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      altShiftOp = TR::InstOpCode::SLLK;
      }

   return genericIntShift(node, cg, TR::InstOpCode::SLL, altShiftOp);
   }

/**
 * sshlEvaluator - shift short integer left
 * (child1 << child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::sshlEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   auto altShiftOp = TR::InstOpCode::SLLG;

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      altShiftOp = TR::InstOpCode::SLLK;
      }

   return genericIntShift(node, cg, TR::InstOpCode::SLL, altShiftOp);
   }

/**
 * ishrEvaluator - shift integer right arithmetically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::ishrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::InstOpCode::Mnemonic altShiftOp = TR::InstOpCode::SRAG;
   return genericIntShift(node, cg, TR::InstOpCode::SRA, altShiftOp);
   }

/**
 * lshrEvaluator - shift long integer right arithmetically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::lshrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return genericLongShiftSingle(node, cg, TR::InstOpCode::SRAG);
   }

/**
 * bshrEvaluator - shift byte right arithmetically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bshrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::InstOpCode::Mnemonic altShiftOp = TR::InstOpCode::SRAG;
   return genericIntShift(node, cg, TR::InstOpCode::SRA, altShiftOp);
   }

/**
 * sshrEvaluator - shift short integer right arithmetically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::sshrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::InstOpCode::Mnemonic altShiftOp = TR::InstOpCode::SRAG;
   return genericIntShift(node, cg, TR::InstOpCode::SRA, altShiftOp);
   }

/**
 * iushrEvaluator - shift integer right logically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::iushrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   auto altShiftOp = TR::InstOpCode::SRLG;

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      altShiftOp = TR::InstOpCode::SRLK;
      }

   return genericIntShift(node, cg, TR::InstOpCode::SRL, altShiftOp);
   }

/**
 * lushrEvaluator - shift long integer right logically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::lushrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return genericLongShiftSingle(node, cg, TR::InstOpCode::SRLG);
   }

/**
 * bushrEvaluator - shift byte right logically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bushrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   auto altShiftOp = TR::InstOpCode::SRLG;

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      altShiftOp = TR::InstOpCode::SRLK;
      }

   return genericIntShift(node, cg, TR::InstOpCode::SRL, altShiftOp);
   }

/**
 * sushrEvaluator - shift short integer right logically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::sushrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   auto altShiftOp = TR::InstOpCode::SRLG;

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      altShiftOp = TR::InstOpCode::SRLK;
      }

   return genericIntShift(node, cg, TR::InstOpCode::SRL, altShiftOp);
   }

/**
 * rolEvaluator - rotate left evaluator (both for longs and ints!)
 */
TR::Register *
OMR::Z::TreeEvaluator::integerRolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *sourceRegister = NULL;
   TR::Register *targetRegister = NULL;
   TR::Node     *secondChild = node->getSecondChild();
   TR::Node     *firstChild  = node->getFirstChild();
   bool nodeIs64Bit = node->getSize() > 4;

   if (secondChild->getOpCode().isLoadConst())
      {
      intptr_t rotateAmount = secondChild->getConstValue();
      if (rotateAmount == 0)
         {
         targetRegister = cg->evaluate(firstChild);
         }
      else
         {
         sourceRegister = cg->evaluate(firstChild);
         targetRegister = nodeIs64Bit ? cg->allocateRegister() : cg->allocateRegister();
         generateRSInstruction(cg, nodeIs64Bit ? TR::InstOpCode::RLLG : TR::InstOpCode::RLL, node, targetRegister, sourceRegister, rotateAmount);
         }
      }
   else
      {
      sourceRegister = cg->evaluate(firstChild);
      targetRegister = nodeIs64Bit ? cg->allocateRegister() : cg->allocateRegister();

      TR::MemoryReference* memRef = generateS390MemoryReference(cg);
      memRef->populateMemoryReference(secondChild, cg);
      generateRSInstruction(cg, nodeIs64Bit ? TR::InstOpCode::RLLG : TR::InstOpCode::RLL, node, targetRegister, sourceRegister, memRef);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;

   }


/**
 * iandEvaluator - boolean and of 2 integers
 */
TR::Register *
OMR::Z::TreeEvaluator::iandEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if (targetRegister = genericRotateAndInsertHelper(node, cg))
      {
      node->setRegister(targetRegister);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      return targetRegister;
      }

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = getIntegralValue(secondChild);
      targetRegister = cg->gprClobberEvaluate(firstChild);

      generateS390ImmOp(cg, TR::InstOpCode::N, node, targetRegister, targetRegister, value);

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::NR, TR::InstOpCode::N, TR::InstOpCode::LR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

/**
 * landEvaluator - boolean and of 2 long integers
 */
TR::Register *
OMR::Z::TreeEvaluator::landEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if ((targetRegister = genericRotateAndInsertHelper(node, cg)))
      {
      node->setRegister(targetRegister);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return targetRegister;
      }
   if ((targetRegister = TR::TreeEvaluator::tryToReplaceShiftLandWithRotateInstruction(node, cg, 0, false)))
      {
      //children dereferenced in tryToReplaceShiftLandWithRotateInstruction
      node->setRegister(targetRegister);
      return targetRegister;
      }

   // LL: if second child is long constant & bitwiseOpNeedsLiteralFromPool returns false,
   // then we don't need to put the constant in literal pool, so we'll evaluate first child and
   // use AND Immediate instructions.
   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);

      generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, targetRegister, targetRegister, secondChild->getLongInt());

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::NGR, TR::InstOpCode::NG, TR::InstOpCode::LGR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

/**
 * bandEvaluator - boolean and of 2 bytes
 */
TR::Register *
OMR::Z::TreeEvaluator::bandEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

/**
 * sandEvaluator - boolean and of 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::sandEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

/**
 * candEvaluator - boolean and of 2 unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::candEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }


/**
 * iorEvaluator - boolean or of 2 integers
 */
TR::Register *
OMR::Z::TreeEvaluator::iorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst())
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);

      int32_t value=getIntegralValue(secondChild);

      generateS390ImmOp(cg, TR::InstOpCode::O, node, targetRegister, targetRegister, value);

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::OR, TR::InstOpCode::O, TR::InstOpCode::LR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

/**
 * lorEvaluator - boolean or of 2 long integers
 */
TR::Register *
OMR::Z::TreeEvaluator::lorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   // See if rotate-left can be used
   //
   if (targetRegister = genericRotateLeft(node, cg))
      {
      node->setRegister(targetRegister);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return targetRegister;
      }

   // LL: if second child is constant & bitwiseOpNeedsLiteralFromPool function call return false,
   // then we don't need to put the constant in literal pool, so we'll evaluate first child and
   // use OR Immediate instructions.
   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);

      generateS390ImmOp(cg, TR::InstOpCode::getOrOpCode(), node, targetRegister, targetRegister, secondChild->getLongInt());

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::OGR, TR::InstOpCode::OG, TR::InstOpCode::LGR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

/**
 * borEvaluator - boolean or of 2 bytes
 */
TR::Register *
OMR::Z::TreeEvaluator::borEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

/**
 * sorEvaluator - boolean or of 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::sorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

/**
 * corEvaluator - boolean or of 2 unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::corEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

/**
 * ixorEvaluator - boolean xor of 2 integers
 */
TR::Register *
OMR::Z::TreeEvaluator::ixorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst())
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);

      int32_t value=getIntegralValue(secondChild);

      generateS390ImmOp(cg, TR::InstOpCode::X, node, targetRegister, targetRegister, value);

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::XR, TR::InstOpCode::X, TR::InstOpCode::LR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

/**
 * lxorEvaluator - boolean xor of 2 long integers
 */
TR::Register *
OMR::Z::TreeEvaluator::lxorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   // See if rotate-left can be used
   //
   if (targetRegister = genericRotateLeft(node, cg))
      {
      node->setRegister(targetRegister);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return targetRegister;
      }

   // LL: if second child is constant & bitwiseOpNeedsLiteralFromPool function call return false,
   // then we don't need to put the constant in literal pool, so we'll evaluate first child and
   // use XOR Immediate instructions.
   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);

      generateS390ImmOp(cg, TR::InstOpCode::getXOROpCode(), node, targetRegister, targetRegister, secondChild->getLongInt());

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, TR::InstOpCode::XGR, TR::InstOpCode::XG, TR::InstOpCode::LGR);
      targetRegister = node->getRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

/**
 * bxorEvaluator - boolean xor of 2 bytes
 */
TR::Register *
OMR::Z::TreeEvaluator::bxorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

/**
 * sxorEvaluator - boolean xor of 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::sxorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

/**
 * cxorEvaluator - boolean xor of 2 unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::cxorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::dexpEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT(0, "This evaluator is not functionally correct. Do Not use.");

   return TR::TreeEvaluator::libmFuncEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::fexpEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT(0, "This evaluator is not functionally correct. Do Not use.");

   return TR::TreeEvaluator::libmFuncEvaluator(node, cg);
   }
