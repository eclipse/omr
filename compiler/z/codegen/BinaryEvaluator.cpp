/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <math.h>                                   // for exp
#include <stddef.h>                                 // for size_t
#include <stdint.h>                                 // for int32_t, int64_t, etc
#include <stdio.h>                                  // for NULL, printf, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd, etc
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction
#include "codegen/Machine.hpp"                      // for MAXDISP, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/StorageInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"                         // for TR_HeapMemory, etc
#include "env/jittypes.h"                           // for intptrj_t
#include "il/AliasSetInterface.hpp"
#include "il/DataTypes.hpp"                         // for DataTypes::Int16, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node, rcount_t
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/Bit.hpp"                            // for isEven, etc
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "ras/Delimiter.hpp"                        // for Delimiter
#include "runtime/Runtime.hpp"
#include "z/codegen/BinaryAnalyser.hpp"
#include "z/codegen/BinaryCommutativeAnalyser.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"


//#define TRACE_EVAL
#if defined(TRACE_EVAL)
#define EVAL_BLOCK
#if defined (EVAL_BLOCK)
#define PRINT_ME(string,node,cg) TR::Delimiter evalDelimiter(TR::comp(), TR::comp()->getOption(TR_TraceCG), "EVAL", string)
#else
extern void PRINT_ME(char * string, TR::Node * node, TR::CodeGenerator * cg);
#endif
#else
#define PRINT_ME(string,node,cg)
#endif

#define ENABLE_ZARCH_FOR_32    1

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

   if ( ENABLE_ZARCH_FOR_32 &&
         performTransformation(comp, "O^O Use AL/ALC to perform long add.\n")
      )
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

/**
 * laddHelper - add 2 long integers for 32bit platform
 */
TR::Register *
laddHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( TR::Compiler->target.is32Bit(),"Should call laddEvaluator64(...) for 64Bit CodeGen!");
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::RegisterPair * targetRegisterPair = NULL;

   TR::Instruction * cursor = NULL;

   if (NEED_CC(node) || (node->getOpCodeValue() == TR::luaddc))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::ladd || node->getOpCodeValue() == TR::luadd || node->getOpCodeValue() == TR::luaddc,
              "CC computation not supported for this node %p\n", node);

      // we need the carry from longAddAnalyser for the CC sequence,
      // and we assume that the analyser does not generate alternative instructions
      // which do not produce a carry.
      TR_S390BinaryCommutativeAnalyser(cg).longAddAnalyser(node, TR::InstOpCode::LR);
      targetRegisterPair = (TR::RegisterPair *) node->getRegister();
      return targetRegisterPair;
      }

   if (secondChild->getOpCodeValue() == TR::lconst && secondChild->getRegister() == NULL)
      {
      TR::LabelSymbol * doneLAdd = generateLabelSymbol(cg);
      TR::Node * firstChild = node->getFirstChild();

      targetRegisterPair = (TR::RegisterPair *) cg->gprClobberEvaluate(firstChild);
      targetRegisterPair = laddConst(node, cg, targetRegisterPair, secondChild->getLongInt(), NULL);

      node->setRegister(targetRegisterPair);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      temp.longAddAnalyser(node, TR::InstOpCode::LR);
      targetRegisterPair = (TR::RegisterPair *) node->getRegister();
      }
   return targetRegisterPair;
   }

/**
 * laddHelper64- add 2 long integers for 64bit platform
 */
TR::Register *
laddHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT(TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(),
      "Should call laddEvaluator(...) for 32Bit CodeGen!");
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   TR::LabelSymbol *skipAdd = NULL;

   TR::Node *curTreeNode = cg->getCurrentEvaluationTreeTop()->getNode();
   bool bumpedRefCount = false;

   bool isCompressionSequence = TR::Compiler->target.is64Bit() &&
           comp->useCompressedPointers() &&
           node->containsCompressionSequence();

   if (NEED_CC(node) || (node->getOpCodeValue() == TR::luaddc))
      {
      TR_ASSERT( !isCompressionSequence, "CC computation not supported with compression sequence.\n");
      TR_ASSERT(node->getOpCodeValue() == TR::ladd || node->getOpCodeValue() == TR::luadd || node->getOpCodeValue() == TR::luaddc,
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

   bool hasCompressedPointers = TR::TreeEvaluator::genNullTestForCompressedPointers(node, cg, targetRegister, skipAdd);

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

   cg->ensure64BitRegister(targetRegister);
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
         case TR::Address:
            TR_ASSERT( TR::Compiler->target.is32Bit(),"Should not be here for 64bit!");
         case TR::Int32:
            {
            PRINT_ME("iconst", node, cg);
            value = secondChild->getInt();
            break;
            }
         case TR::Int16:
            {
            PRINT_ME("sconst", node, cg);
            value = (int32_t) secondChild->getShortInt();
            break;
            }
//          case TR_UInt16:
//             {
//             PRINT_ME("cconst", node, cg);
//             value = (int32_t) secondChild->getConst<uint16_t>();
//             break;
//             }
         case TR::Int8:
            {
            PRINT_ME("bconst", node, cg);
            value = (int32_t) secondChild->getByte();
            break;
            }
         default:
            TR_ASSERT( 0, "generic32BitAddEvaluator: Unexpected Type\n");
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
      bool useLA = node->isNonNegative() && !cg->use64BitRegsOn32Bit() &&
         ((value < MAXLONGDISP && value > MINLONGDISP) ||
          (value >= 0 && value <= MAXDISP)) && (TR::Compiler->target.is64Bit() || node->isNonNegative());

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

      if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
         if (value < 0 || value > MAXDISP)
            {
            generateRXYInstruction(cg, TR::InstOpCode::LAY, node, targetRegister, memRef);
            }
         else
            {
            generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister, memRef);
            }
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
 * (i.e. byte-bsub, char-csub, short-ssub, and int-isub)
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
         case TR::Address:
            TR_ASSERT( TR::Compiler->target.is32Bit(), "generic32BitSubEvaluator(): unexpected data type for 64bit code-gen!");
         case TR::Int32:
            {
            PRINT_ME("iconst", node, cg);
            value = secondChild->getInt();
            break;
            }
         case TR::Int16:
            {
            PRINT_ME("sconst", node, cg);
            value = (int32_t) secondChild->getShortInt();
            }
            break;
//          case TR_UInt16:
//             {
//             PRINT_ME("cconst", node, cg);
//             value = (int32_t) secondChild->getConst<uint16_t>();
//             break;
//             }
         case TR::Int8:
            {
            PRINT_ME("bconst", node, cg);
            value = (int32_t) secondChild->getByte();
            break;
            }
         default:
            TR_ASSERT( 0, "generic32BitAddEvaluator: Unexpected Type\n");
            break;
         }

      if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196) &&
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
 * Generate code for lDiv or lRem.
 * If isDivision = true we return the quotient register.
 *               = false we return the remainder register.
 */
inline TR::Register *
lDivRemGenericEvaluator(TR::Node * node, TR::CodeGenerator * cg, bool isDivision)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Compilation *comp = cg->comp();

   TR::RegisterPair * dividendPair = (TR::RegisterPair *) cg->gprClobberEvaluate(firstChild);

   int32_t shiftAmnt;
   //case: A/A Q=1, R=0
   if (secondChild == firstChild)
      {
      if (isDivision)
         {
         generateLoad32BitConstant(cg, node, 1, dividendPair->getLowOrder(), true);
         generateLoad32BitConstant(cg, node, 0, dividendPair->getHighOrder(), true);
         }
      else //remainder
         {
         generateLoad32BitConstant(cg, node, 0, dividendPair->getLowOrder(), true);
         generateLoad32BitConstant(cg, node, 0, dividendPair->getHighOrder(), true);
         }
      node->setRegister(dividendPair);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return dividendPair;
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
      if (denominator == -1 &&
            performTransformation(comp, "O^O Denominator is -1 for ldir/lrem.\n"))
         {
         if (!isDivision)  //remainder
            {
            generateLoad32BitConstant(cg, node, 0, dividendPair->getLowOrder(), true);
            generateLoad32BitConstant(cg, node, 0, dividendPair->getHighOrder(), true);
            }
         else
            {

            TR::RegisterDependencyConditions * dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);
            dep->addPostCondition(dividendPair, TR::RealRegister::EvenOddPair);
            dep->addPostCondition(dividendPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
            dep->addPostCondition(dividendPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

            TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
            TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

            //special case, need to check if dividend is 0x8000000000000000 - but need to check it in 2 steps
            TR::LabelSymbol * doDiv = generateLabelSymbol(cg);
            generateS390ImmOp(cg, TR::InstOpCode::C, node, dividendPair->getHighOrder(), dividendPair->getHighOrder(), (int32_t) 0x80000000);

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            cFlowRegionStart->setStartInternalControlFlow();

            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doDiv);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, dividendPair->getLowOrder(), (signed char)0, TR::InstOpCode::COND_BE, cFlowRegionEnd);
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doDiv);

            dividendPair = lnegFor32Bit(node, cg, dividendPair, dep);

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dep);
            cFlowRegionEnd->setEndInternalControlFlow();
            }
         node->setRegister(dividendPair);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return dividendPair;
         }
      //case A/1 Q=A, R=0
      else if (denominator == 1 &&
            performTransformation(comp, "O^O Denominator is 1 for ldir/lrem.\n"))
         {
         if (!isDivision) //remainder
            {
            generateLoad32BitConstant(cg, node, 0, dividendPair->getLowOrder(), true);
            generateLoad32BitConstant(cg, node, 0, dividendPair->getHighOrder(), true);
            }
         node->setRegister(dividendPair);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return dividendPair;
         }
      //case A/(+-)2^x - division is done using shifts
      else if ((shiftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(denominator)) > 0 &&
            performTransformation(comp, "O^O Denominator is powerOfTwo (%d) for ldir/lrem.\n",denominator))
         {
         int64_t absValueOfDenominator = denominator>0 ? denominator : -denominator;
         TR::LabelSymbol * done = generateLabelSymbol(cg);

         //setup dependencies for shift instructions for division or remainder
         TR::RegisterDependencyConditions * dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
         dep->addPostCondition(dividendPair, TR::RealRegister::EvenOddPair);
         dep->addPostCondition(dividendPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         dep->addPostCondition(dividendPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

         if (isDivision)
            {
            if (!firstChild->isNonNegative())
               {
               TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
               TR::LabelSymbol * skipSet = generateLabelSymbol(cg);

               generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
               cFlowRegionStart->setStartInternalControlFlow();
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, dividendPair->getHighOrder(), (signed char)0, TR::InstOpCode::COND_BNL, skipSet);

               //adjustment to dividend if dividend is negative
               dividendPair = laddConst(node, cg, dividendPair,absValueOfDenominator-1, dep);

               generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipSet, dep);
               skipSet->setEndInternalControlFlow();
               }

            //divide
            if (isUnsigned)
               generateRSInstruction(cg, TR::InstOpCode::SRDL, node, dividendPair, shiftAmnt, dep);
            else
               generateRSInstruction(cg, TR::InstOpCode::SRDA, node, dividendPair, shiftAmnt, dep);

            //fix sign of quotient if necessary
            if (denominator < 0)
               dividendPair = lnegFor32Bit(node, cg, dividendPair, dep);

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, done, dep);
            }
         else //remainder
            {

            if (!firstChild->isNonNegative())
               {
               TR::Register * tempRegister1h = cg->allocateRegister();
               TR::Register * tempRegister1l = cg->allocateRegister();
               TR::Register * tempRegister2h = cg->allocateRegister();
               TR::Register * tempRegister2l = cg->allocateRegister();
               TR::RegisterPair * tempRegisterPair1 = cg->allocateConsecutiveRegisterPair(tempRegister1h, tempRegister1l);
               TR::RegisterPair * tempRegisterPair2 = cg->allocateConsecutiveRegisterPair(tempRegister2h, tempRegister2l);
               generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegisterPair2->getLowOrder(), dividendPair->getLowOrder());
               generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegisterPair2->getHighOrder(), dividendPair->getHighOrder());
               generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegisterPair1->getLowOrder(), dividendPair->getLowOrder());
               generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegisterPair1->getHighOrder(), dividendPair->getHighOrder());
               generateRSInstruction(cg, TR::InstOpCode::SRDA, node, tempRegisterPair2, 63);
               generateRILInstruction(cg, TR::InstOpCode::NILF, node, tempRegisterPair2->getLowOrder(), static_cast<int32_t>(absValueOfDenominator-1) );
               generateRILInstruction(cg, TR::InstOpCode::NILF, node, tempRegisterPair2->getHighOrder(), static_cast<int32_t>((absValueOfDenominator-1)>>32) );
               generateRRInstruction(cg, TR::InstOpCode::ALR, node, dividendPair->getLowOrder(), tempRegisterPair2->getLowOrder());
               generateRRInstruction(cg, TR::InstOpCode::ALCR, node, dividendPair->getHighOrder(), tempRegisterPair2->getHighOrder());
               generateRILInstruction(cg, TR::InstOpCode::NILF, node, dividendPair->getLowOrder(), static_cast<int32_t>( CONSTANT64(0xFFFFFFFFFFFFFFFF) - absValueOfDenominator +1) );
               if (absValueOfDenominator != static_cast<int32_t>(absValueOfDenominator))
                  generateRILInstruction(cg, TR::InstOpCode::NILF, node, dividendPair->getHighOrder(), static_cast<int32_t>(( CONSTANT64(0xFFFFFFFFFFFFFFFF) - absValueOfDenominator +1)>>32) );
               generateRRInstruction(cg, TR::InstOpCode::SLR, node, tempRegisterPair1->getLowOrder(), dividendPair->getLowOrder());
               generateRRInstruction(cg, TR::InstOpCode::SLBR, node, tempRegisterPair1->getHighOrder(), dividendPair->getHighOrder());
               generateRRInstruction(cg, TR::InstOpCode::LR, node, dividendPair->getLowOrder(), tempRegisterPair1->getLowOrder());
               generateRRInstruction(cg, TR::InstOpCode::LR, node, dividendPair->getHighOrder(), tempRegisterPair1->getHighOrder());
               cg->stopUsingRegister(tempRegisterPair1);
               cg->stopUsingRegister(tempRegisterPair2);
               }
            else
               {
               generateRSInstruction(cg, TR::InstOpCode::SLDL, node, dividendPair, 64-shiftAmnt, dep);
               generateRSInstruction(cg, TR::InstOpCode::SRDL, node, dividendPair, 64-shiftAmnt, dep);
               }
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, done, dep);
            }
         node->setRegister(dividendPair);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return dividendPair;
         }
      }

   if (performTransformation(comp, "O^O Using 64bit ldiv on 32bit driver.\n"))
      {
      TR::Instruction * cursor = NULL;
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * gotoDiv = generateLabelSymbol(cg);
      TR::LabelSymbol * dontDiv = generateLabelSymbol(cg);
      TR::RegisterPair * divisorPair = (TR::RegisterPair *) cg->gprClobberEvaluate(secondChild);
      TR::Register     * litPoolBase = NULL;
      bool highWordRADisabled = !cg->supportsHighWordFacility() || comp->getOption(TR_DisableHighWordRA);

      // 1st-6th Post Conditions as listed below
      // Setup arguments
      TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 8, cg);
      dependencies->addPostCondition(dividendPair, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(dividendPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(dividendPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);
      dependencies->addPostCondition(divisorPair, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(divisorPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(divisorPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

      if (!node->getOpCode().isUnsigned() &&
            !node->isSimpleDivCheck() &&
            !firstChild->isNonNegative() &&
            !secondChild->isNonNegative() &&
            !(secondChild->getOpCodeValue() == TR::lconst &&
               secondChild->getLongInt() != CONSTANT64(0xFFFFFFFFFFFFFFFF)))
         {
         litPoolBase = cg->allocateRegister();
         dependencies->addPostCondition(litPoolBase, TR::RealRegister::AssignAny);
         generateLoadLiteralPoolAddress(cg, node, litPoolBase);
         }

      //shift all 64 bits of dividend into dividendPair->getLowOrder()
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, dividendPair->getHighOrder(), dividendPair->getHighOrder(), 32);

      generateRRInstruction(cg, TR::InstOpCode::LR, node, dividendPair->getHighOrder(), dividendPair->getLowOrder());

      generateRRInstruction(cg, TR::InstOpCode::LGR, node, dividendPair->getLowOrder(), dividendPair->getHighOrder());

      //shift all 64 bits of divisor into divisorPair->getHighOrder()
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, divisorPair->getHighOrder(), divisorPair->getHighOrder(), 32);

      generateRRInstruction(cg, TR::InstOpCode::LR, node, divisorPair->getHighOrder(), divisorPair->getLowOrder());

      if (!node->getOpCode().isUnsigned()) // Signed Divide Code
         {
         if (!node->isSimpleDivCheck() &&
               !firstChild->isNonNegative() &&
               !secondChild->isNonNegative() &&
               !(secondChild->getOpCodeValue() == TR::lconst &&
                  secondChild->getLongInt() != CONSTANT64(0xFFFFFFFFFFFFFFFF)))
            {

            //  We have to check for a special case where we do 0x8000000000000000/0xffffffffffffffff
            //  JAVA requires we return 0x8000000000000000 rem 0x00000000
            generateS390ImmOp(cg, TR::InstOpCode::CG, node, dividendPair->getHighOrder(), dividendPair->getHighOrder(),
                  (int64_t)(CONSTANT64(0x8000000000000000)) , dependencies, litPoolBase);

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            cFlowRegionStart->setStartInternalControlFlow();

            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, gotoDiv);
            dontDiv->setEndInternalControlFlow();

            //  Fix up for remainder case
            if (!isDivision)
               {
               cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, divisorPair->getHighOrder(), (signed char)-1, TR::InstOpCode::COND_BNE, gotoDiv);
               // Check to see if compare generated a register to build constChar
               TR::Instruction *compareInstr=(cursor->getOpCode().isCompare())?NULL:cursor->getPrev();
               if(compareInstr && compareInstr->getNumRegisterOperands()==2)
                 {
                 dependencies->addPostCondition(compareInstr->getRegisterOperand(2), TR::RealRegister::AssignAny);
                 }
               generateRIInstruction(cg, TR::InstOpCode::LGHI, node, dividendPair->getHighOrder(), 0);

               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, dontDiv);
               }
            else
               {
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, divisorPair->getHighOrder(), (signed char)-1, TR::InstOpCode::COND_BE, dontDiv);
               }

            // Label to do the division
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, gotoDiv);
            }


         //do division
         generateRRInstruction(cg, TR::InstOpCode::DSGR, node, dividendPair, divisorPair->getHighOrder());

         // Label to skip the division
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, dontDiv, dependencies);
         }
      else // Unsigned Divide Code
         {
         //clear high order 64 bits of register pair
         generateRRInstruction(cg, TR::InstOpCode::XGR, node, dividendPair->getHighOrder(), dividendPair->getHighOrder());

         //do division
         generateRRInstruction(cg, TR::InstOpCode::DLGR, node, dividendPair, divisorPair->getHighOrder());
         }

      // mainly common clean-up code
      if (isDivision)
         {
         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, dividendPair->getHighOrder(), dividendPair->getLowOrder(), 32);
         }
      else //remainder
         {
         generateRRInstruction(cg, TR::InstOpCode::LR, node, dividendPair->getLowOrder(), dividendPair->getHighOrder());

         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, dividendPair->getHighOrder(), dividendPair->getHighOrder(), 32);
         }

      // The label is necessary so that RA does not insert 32-bit register moves
      // inbetween the 64-bit operations
      TR::LabelSymbol * depLabel = generateLabelSymbol(cg);
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, depLabel, dependencies);

      node->setRegister(dividendPair);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      cg->stopUsingRegister(divisorPair);

      if (!node->getOpCode().isUnsigned())
         {
         if (litPoolBase)
            cg->stopUsingRegister(litPoolBase);
         }
      return dividendPair;
      }
   else
      {
      TR::Instruction * cursor = NULL;
      TR::Register * EPReg = cg->allocateRegister();
      TR::Register * RAReg = cg->allocateRegister();
      TR::Register * evenRegister = cg->allocateRegister();
      TR::Register * oddRegister = cg->allocateRegister();

      TR::RegisterPair * targetRegisterPair = cg->allocateConsecutiveRegisterPair(oddRegister, evenRegister);
      TR::RegisterPair * divisorPair = (TR::RegisterPair *) cg->gprClobberEvaluate(secondChild);

      // 1st-4th Pre Conditions as listed below
      // 1st-10th Post Conditions as listed below
      // 11th Post Condition may be added by generateDirectCall() if it uses generateRegLitRefInstruction.
      TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 11, cg);

      // Setup arguments
      dependencies->addPreCondition(dividendPair->getLowOrder(), TR::RealRegister::GPR3);
      dependencies->addPreCondition(dividendPair->getHighOrder(), TR::RealRegister::GPR2);
      dependencies->addPreCondition(divisorPair->getLowOrder(), TR::RealRegister::GPR9);
      dependencies->addPreCondition(divisorPair->getHighOrder(), TR::RealRegister::GPR8);

      cg->stopUsingRegister(dividendPair);
      cg->stopUsingRegister(divisorPair);
      // Kill scratch regs
      TR::Register * tempReg1 = cg->allocateRegister();
      TR::Register * tempReg2 = cg->allocateRegister();
      TR::Register * tempReg3 = cg->allocateRegister();
      TR::Register * tempReg4 = cg->allocateRegister();
      TR::Register * tempReg5 = cg->allocateRegister();
      TR::Register * tempReg6 = cg->allocateRegister();
      dependencies->addPostCondition(tempReg1, TR::RealRegister::GPR0);
      dependencies->addPostCondition(tempReg2, TR::RealRegister::GPR1);
      dependencies->addPostCondition(tempReg3, TR::RealRegister::GPR9);
      dependencies->addPostCondition(tempReg4, TR::RealRegister::GPR8);

      dependencies->addPostCondition(evenRegister, TR::RealRegister::GPR2);

      dependencies->addPostCondition(oddRegister, TR::RealRegister::GPR3);
      dependencies->addPostCondition(tempReg5, TR::RealRegister::GPR10);
      dependencies->addPostCondition(tempReg6, TR::RealRegister::GPR11);

      // Setup regs for call out
      dependencies->addPostCondition(EPReg, cg->getEntryPointRegister());
      dependencies->addPostCondition(RAReg, cg->getReturnAddressRegister());

      // call out
      // Setup return value
      if (isDivision)
         {
         cursor = generateDirectCall(cg, node, false, cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390longDivide, false, false, false), dependencies);
         }
      else
         {
         cursor = generateDirectCall(cg, node, false, cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390longRemainder, false, false, false), dependencies);
         }
      cursor->setDependencyConditions(dependencies);

      node->setRegister(targetRegisterPair);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      cg->stopUsingRegister(EPReg);
      cg->stopUsingRegister(RAReg);
      cg->stopUsingRegister(tempReg1);
      cg->stopUsingRegister(tempReg2);
      cg->stopUsingRegister(tempReg3);
      cg->stopUsingRegister(tempReg4);
      cg->stopUsingRegister(tempReg5);
      cg->stopUsingRegister(tempReg6);
      return targetRegisterPair;
      }
   }

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
      TR::Register * returnRegister = cg->allocate64bitRegister();
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
            if (!firstChild->isNonNegative())
               {
               TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
               TR::LabelSymbol * skipSet = generateLabelSymbol(cg);

               generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
               cFlowRegionStart->setStartInternalControlFlow();
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, firstRegister, (signed char)0,TR::InstOpCode::COND_BNL, skipSet);

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
            if (!firstChild->isNonNegative())
               {
               TR::Register * tempRegister1 = cg->allocate64bitRegister();
               TR::Register * tempRegister2 = cg->allocate64bitRegister();
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
      quoRegister = cg->allocate64bitRegister();
      }
   else
      {
      remRegister = cg->allocate64bitRegister();
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

   if (!doConditionalRemainder &&
         !node->isSimpleDivCheck() &&
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
            absDividendReg = cg->allocate64bitRegister();
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
   bool needCheck = (!node->isSimpleDivCheck()            &&
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

      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, sourceRegister, (signed char)-1, TR::InstOpCode::COND_BNE, doDiv);
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

/**
 * Generic evaluator for lshl, lushr and lshr
 */
inline TR::Register *
genericLongShift(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic shiftOp)
   {
   TR_ASSERT(TR::Compiler->target.is32Bit() && !cg->use64BitRegsOn32Bit(),
      "genericLongShift is for 32bit code-gen only! ");

   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * trgReg = cg->gprClobberEvaluate(firstChild);

   TR::Register * src2Reg = NULL;
   TR::MemoryReference * tempMR = NULL;

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = secondChild->getInt();
      if ((value & 0x3f) != 0)
         {
         generateRSInstruction(cg, shiftOp, node, trgReg, value & 0x3f);
         }
      }
   else
      {
      TR::Register * src2Reg = cg->evaluate(secondChild);
      if(src2Reg->getRegisterPair()) src2Reg = src2Reg->getRegisterPair()->getHighOrder();
      TR::MemoryReference * tempMR = generateS390MemoryReference(src2Reg, 0, cg);
      generateRSInstruction(cg, shiftOp, node, trgReg, tempMR);
      tempMR->stopUsingMemRefRegister(cg);
      }

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

inline TR::Register *
genericLongShiftSingle(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic shiftOp)
   {
   TR_ASSERT(TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(),
         "genericLongShiftSingle() is for 64bit code-gen only! ");
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = NULL;
   TR::Register * trgReg = cg->allocate64bitRegister();
   TR::Register * src2Reg = NULL;
   TR::MemoryReference * tempMR = NULL;

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t value = secondChild->getInt();

      if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
         {
         // Generate RISBG for lshl + i2l sequence
         if (node->getOpCodeValue() == TR::lshl)
            {
            if (firstChild->getOpCodeValue() == TR::i2l && firstChild->isSingleRefUnevaluated() && (firstChild->isNonNegative() || firstChild->getFirstChild()->isNonNegative()))
               {
               srcReg = cg->evaluate(firstChild->getFirstChild());
               auto mnemonic = cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;

               generateRIEInstruction(cg, mnemonic, node, trgReg, srcReg, (int8_t)(32-value), (int8_t)((63-value)|0x80), (int8_t)value);

               node->setRegister(trgReg);

               cg->decReferenceCount(firstChild->getFirstChild());
               cg->decReferenceCount(firstChild);
               cg->decReferenceCount(secondChild);

               return trgReg;
               }
            }
         }

      srcReg = cg->evaluate(firstChild);
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
         TR::InstOpCode::Mnemonic loadRegOpCode = TR::InstOpCode::getLoadRegOpCode();
         if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && TR::Compiler->target.is64Bit())
            {
            loadRegOpCode = TR::InstOpCode::getLoadRegOpCodeFromNode(cg, node);
            if (srcReg->is64BitReg())
               {
               loadRegOpCode = TR::InstOpCode::LGR;
               }
            }
         generateRRInstruction(cg, loadRegOpCode, node, trgReg, srcReg);
         }
      }
   else
      {
      trgReg = srcReg;
      }

   if (canUseAltShiftOp &&
       (TR::InstOpCode(altShiftOp).is64bit() || TR::InstOpCode(altShiftOp).is32to64bit()))
      cg->ensure64BitRegister(trgReg);

   if (secondChild->getOpCode().isLoadConst())
      {
      if (shiftAmount != 0)
         {
         if (trgReg != srcReg && canUseAltShiftOp )
            {
            if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && srcReg->assignToHPR() &&
                (altShiftOp == TR::InstOpCode::SLLK || altShiftOp == TR::InstOpCode::SRLK ))
               {
               if (altShiftOp == TR::InstOpCode::SLLK)
                  {
                  if (trgReg->assignToHPR())
                     altShiftOp = TR::InstOpCode::SLLHH;
                  else
                     altShiftOp = TR::InstOpCode::SLLLH;
                  }
               else
                  {
                  if (trgReg->assignToHPR())
                     altShiftOp = TR::InstOpCode::SRLHH;
                  else
                     altShiftOp = TR::InstOpCode::SRLLH;
                  }
               generateExtendedHighWordInstruction(node, cg, altShiftOp, trgReg, srcReg, shiftAmount);
               }
            else
               {
               generateRSInstruction(cg, altShiftOp, node, trgReg, srcReg, shiftAmount);
               }
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
   PRINT_ME("iadd", node, cg);
   return generic32BitAddEvaluator(node, cg);
   }

/**
 * laddEvaluator - add 2 long integers
 */
TR::Register *
OMR::Z::TreeEvaluator::laddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("ladd", node, cg);

   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return laddHelper64(node, cg);
      }
   else
      {
      return laddHelper(node, cg);
      }
   }


/**
 * address add helper function
 */
TR::Register *
OMR::Z::TreeEvaluator::addrAddHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (node->getOpCodeValue() == TR::aiadd)
      return generic32BitAddEvaluator(node, cg);
   else if (node->getOpCodeValue() == TR::aladd)
      return TR::TreeEvaluator::laddEvaluator(node, cg);
   else
      TR_ASSERT(0,"Wrong il-opCode for calling addAddHelper!\n");

   return NULL;
   }

/**
 * lsubHelper - subtract 2 long integers (child1 - child2) for 32bit codegen
 */
TR::Register *
lsubHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT( TR::Compiler->target.is32Bit(), "Call lsubHelper64(...) for 64bit code-gen!");
   TR::Node * secondChild = node->getSecondChild();
   TR::Instruction * cursor = NULL;
   TR::RegisterPair * targetRegisterPair = NULL;

   if (NEED_CC(node) || (node->getOpCodeValue() == TR::lusubb))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::lsub || node->getOpCodeValue() == TR::lusub || node->getOpCodeValue() == TR::lusubb,
              "CC computation not supported for this node %p\n", node);

      // we need the borrow from longSubtractAnalyser for the CC sequence,
      // and we assume that the analyser does not generate alternative instructions
      // which do not produce a borrow.
      TR_S390BinaryAnalyser(cg).longSubtractAnalyser(node);
      targetRegisterPair = (TR::RegisterPair *) node->getRegister();
      return targetRegisterPair;
      }


   if (secondChild->getOpCodeValue() == TR::lconst && secondChild->getRegister() == NULL)
      {
      TR::Node * firstChild = node->getFirstChild();
      targetRegisterPair = (TR::RegisterPair *) cg->gprClobberEvaluate(firstChild);
      TR::LabelSymbol * doneLSub = generateLabelSymbol(cg);
      int32_t h_value = secondChild->getLongIntHigh();
      int32_t l_value = secondChild->getLongIntLow();

      // Invert the lconst sign
      int64_t value = (((int64_t) h_value) << 32 | (uint32_t) l_value);

      value = -value;
      h_value = (int32_t) (value >> 32);
      l_value = (int32_t) value;

      TR::Register * lowOrder = targetRegisterPair->getLowOrder();
      TR::Register * highOrder = targetRegisterPair->getHighOrder();

      if ( ENABLE_ZARCH_FOR_32 &&
            performTransformation(comp, "O^O Use AL/ALC to perform long add.\n")
         )
         {     // wrong carry bit to be set
         if ( l_value != 0 )
            {
            // tempReg is used to hold the immediate on GoldenEagle or better.
            // On other hardware, we use highOrder
            //
            TR::Register * tempReg = cg->allocateRegister();

            generateS390ImmOp(cg, TR::InstOpCode::AL, node, tempReg, lowOrder, l_value);
            generateS390ImmOp(cg, TR::InstOpCode::ALC, node, tempReg, highOrder, h_value);

            cg->stopUsingRegister(tempReg);
            }
         else
            {
            generateS390ImmOp(cg, TR::InstOpCode::A, node, highOrder, highOrder, h_value);
            }
         }
      else
         {
         TR::RegisterDependencyConditions * dependencies =
            new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);

         // Add high value
         generateS390ImmOp(cg, TR::InstOpCode::A, node, highOrder, highOrder, h_value);

         // Add low value
         if ( l_value != 0 )
            {
            // tempReg is used to hold the immediate on GoldenEagle or better.
            // On other hardware, we use highOrder
            //
            TR::Register * tempReg = cg->allocateRegister();
            dependencies->addPostCondition(tempReg, TR::RealRegister::AssignAny);

            generateS390ImmOp(cg, TR::InstOpCode::AL, node, tempReg, lowOrder, l_value);

            // Check for overflow in LS(h_value) int. If overflow, increment MS(l_value) int.
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK12, node, doneLSub);

            // Increment MS int due to overflow in LS int
            generateRIInstruction(cg, TR::InstOpCode::AHI, node, highOrder, 1);

            cg->stopUsingRegister(tempReg);

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLSub, dependencies);
            }
         }

      node->setRegister(targetRegisterPair);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_S390BinaryAnalyser temp(cg);
      temp.longSubtractAnalyser(node);
      targetRegisterPair = (TR::RegisterPair *) node->getRegister();
      }

   return targetRegisterPair;
   }

////////////////////////////////////////////////////////////////////////////////////////

TR::Register *
genericRotateLeft(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
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

               TR::InstOpCode::Mnemonic opCode = cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;
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
            TR::Register* targetReg = cg->allocate64bitRegister();

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
               TR::Register* targetReg = cg->allocate64bitRegister();
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
            TR::Register* targetReg = cg->allocate64bitRegister();
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
   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
      {
      TR::Node * firstChild = node->getFirstChild();
      TR::Node * secondChild = node->getSecondChild();
      TR::Node * skipedConversion = NULL;
      bool LongToInt = false;
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
         if (TR::Compiler->target.is32Bit() && firstChild->getOpCodeValue() == TR::l2i)
            {
            LongToInt = true;
            traceMsg(comp, "Long2Int conversion in between, need to evaluate if RISBG can be used \n");
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
         // if GRA had decided to assign HPR to these nodes, we cannot use RISBG because they are 64-bit
         // instructions
         if (cg->supportsHighWordFacility() &&
             firstChild->getFirstChild()->getRegister() &&
             firstChild->getFirstChild()->getRegister()->assignToHPR())
            {
            return NULL;
            }

         uint64_t value = 0;

         // Mask
         switch (secondChild->getDataType())
            {
            case TR::Address:
               TR_ASSERT( TR::Compiler->target.is32Bit(),"genericRotateAndInsertHelper: unexpected data type");
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

         // Check for the case where the shift source is in a 64-bit register.
         if (sourceReg && sourceReg->getKind() == TR_GPR64)
            LongToInt = false;

         if (firstChild->getOpCode().isRightShift())
            {
            shiftMsBit += shiftAmnt;
            // Turn right shift into left shift
            shiftAmnt = 64 - shiftAmnt;

            if (LongToInt)
               {
               int32_t shiftRightAmt = (64-shiftAmnt); //get original shiftRight amount
               if (msBit - shiftRightAmt >= 32)
                  useOrder = 1;
               else if (shiftRightAmt >= 32)
                  {
                  shiftAmnt = 64 - (shiftRightAmt - 32); //decrease the shift amount
                  useOrder = 2;
                  }
               else if (lsBit <= shiftRightAmt + 31)
                  {
                  shiftAmnt = 32 - shiftRightAmt; //do a left shift instead
                  useOrder = 2;
                  }
               else
                  return NULL;
               }
            }
         else
            {
            if (LongToInt)
               useOrder = 1;
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
            if (LongToInt)
               {
               TR::RegisterPair *sourceRegPair = (TR::RegisterPair *) sourceReg;
               if (useOrder == 1)
                  {
                  traceMsg(comp,"\t               => Using LowOrder Register\n");
                  sourceReg = sourceRegPair->getLowOrder(); //use loworderReg
                  }
               else if (useOrder == 2)
                  {
                  traceMsg(comp,"\t               => Using HighOrder Register\n");
                  sourceReg = sourceRegPair->getHighOrder(); //use highorderReg
                  }
               else //cannot use RISBG
                  return NULL;
               }

            if (!cg->canClobberNodesRegister(firstChild->getFirstChild()))
               targetReg = cg->allocateClobberableRegister(sourceReg);
            else
               targetReg = sourceReg;

            cg->ensure64BitRegister(targetReg);

            if (msBit > lsBit)
               {
               if ((TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) && node->getType().isInt64())
                  {
                  generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetReg, targetReg);
                  }
               else
                  {
                  generateRRInstruction(cg, TR::InstOpCode::XR, node, targetReg, targetReg);
                  }
               }
            else if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196) && !node->getType().isInt64())
               {
               generateRIEInstruction(cg, TR::InstOpCode::RISBLG, node, targetReg, sourceReg, msBit, 0x80 + lsBit, shiftAmnt);
               }
            else if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
               {
               auto mnemonic = cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;

               generateRIEInstruction(cg, mnemonic, node, targetReg, sourceReg, msBit, 0x80 + lsBit, shiftAmnt);
               }
            else
               {
               if (!cg->canClobberNodesRegister(firstChild->getFirstChild()))
                  {
                  cg->stopUsingRegister(targetReg);
                  }

               return NULL;
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
OMR::Z::TreeEvaluator::tryToReplaceLongAndWithRotateInstruction(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
      {
      TR::Node * firstChild = node->getFirstChild();
      TR::Node * secondChild = node->getSecondChild();
      TR::Compilation *comp = cg->comp();

      // Given the right case, transform land into RISBG as it may be better than discrete pair of ands
      if (node->getOpCodeValue() == TR::land && secondChild->getOpCode().isLoadConst())
         {
         uint64_t longConstValue = secondChild->getLongInt();

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

            if (firstChild->getReferenceCount()> 1 || (lOnes < 32 && tOnes < 32))
               {
               doTransformation = true;
               }
            }

         if (doTransformation && performTransformation(comp, "O^O Use RISBG instead of 2 ANDs for %p.\n", node))
            {
            TR::Register * targetReg = NULL;
            TR::Register * sourceReg = cg->evaluate(firstChild);

            if (!cg->canClobberNodesRegister(firstChild))
               {
               targetReg = cg->allocateClobberableRegister(sourceReg);
               }
            else
               {
               targetReg = sourceReg;
               }

               // if possible then use the instruction that doesn't set the CC as it's faster
               TR::InstOpCode::Mnemonic opCode = cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12) ? TR::InstOpCode::RISBGN : TR::InstOpCode::RISBG;

               // this instruction sets the rotation factor to 0 and sets the zero bit(0x80).
               // So it's effectively zeroing out every bit except the inclusive range of lsBit to msBit
               // The bits in that range are preserved as is
               generateRIEInstruction(cg, opCode, node, targetReg, sourceReg, msBit, 0x80 + lsBit, 0);

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
   TR_ASSERT(TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(),
      "Call lsubHelper(...) for 32bit code-gen!");

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
   if (TR::Compiler->target.is64Bit() &&
         comp->useCompressedPointers() &&
         node->containsCompressionSequence())
      isCompressionSequence = true;

   if (NEED_CC(node) || (node->getOpCodeValue() == TR::lusubb))
      {
      TR_ASSERT( !isCompressionSequence,"CC computation not supported with compression sequence.\n");
      TR_ASSERT(node->getOpCodeValue() == TR::lsub || node->getOpCodeValue() == TR::lusub || node->getOpCodeValue() == TR::lusubb,
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
   bool hasCompressedPointers = TR::TreeEvaluator::genNullTestForCompressedPointers(node, cg, targetRegister, skipAdd);


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

   cg->ensure64BitRegister(targetRegister);
   return targetRegister;
   }

/**
 * lmulHelper- 32bit version of long multiply Helper
 *
 * This implementation is functionally correct, but certainly not optimal.
 * We can do a lot more stuff if one of the params is a const.
 */
TR::Register *
lmulHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( TR::Compiler->target.is32Bit(), "lmulHelper() is for 32bit code-gen only!");
   TR::Node * firstChild = node->getFirstChild();
   TR::RegisterPair * trgtRegPair;
   TR::Node * secondChild = node->getSecondChild();
   TR::Compilation *comp = cg->comp();

   TR::Instruction * cursor = NULL;

   // Determine if second child is constant.  There are special tricks for these.
   bool secondChildIsConstant = secondChild->getOpCodeValue() == TR::lconst && secondChild->getRegister() == NULL;
   bool firstHighZero = false, secondHighZero = false;
   bool highWordRADisabled = !cg->supportsHighWordFacility() || comp->getOption(TR_DisableHighWordRA);
   if (!secondChildIsConstant && firstChild->isHighWordZero() && !secondChild->isHighWordZero())
      {
      // swap children pointers since optimizer puts highWordZero children as firstChild if the
      // second one is non-constant and non-highWordZero
      TR::Node * tempChild = firstChild;

      firstChild = secondChild;
      secondChild = tempChild;
      }

   trgtRegPair = (TR::RegisterPair *) cg->gprClobberEvaluate(firstChild);

   // 2nd Child Constant
   if (secondChildIsConstant)
      {
      int64_t src2Value = secondChild->getLongInt();

      //0 Case.  Don't Shift
      bool noShift = (src2Value == 0);

      // Check to see if power of 2 and get shift value.
      if (isNonNegativePowerOf2(src2Value))
         {
         if (!noShift)
            {
            int32_t shiftAmount = 0;
            while ((src2Value = ((uint64_t) src2Value) >> 1))
               {
               ++shiftAmount;
               }

            TR_ASSERT( shiftAmount <= 63 && shiftAmount > 0, "lmulEvaluator - unexpected shift amount for Power of 2.");
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLDL, node, trgtRegPair, shiftAmount);
            }
         node->setRegister(trgtRegPair);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return trgtRegPair;
         }
      }

   if (firstChild->isHighWordZero())
      {
      firstHighZero = true;
      }

   if (secondChild->isHighWordZero())
      {
      secondHighZero = true;
      }

   if (performTransformation(comp, "O^O Using 64bit lmul on 32bit platform.\n"))
      {
      // Common signed / unsigned start-up code
      TR::RegisterPair * src2RegPair = NULL;

      TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);
      dependencies->addPostCondition(trgtRegPair->getHighOrder(), TR::RealRegister::AssignAny);
      dependencies->addPostCondition(trgtRegPair->getLowOrder(), TR::RealRegister::AssignAny);

      if (!node->getOpCode().isUnsigned()) // Signed Multiply
         {
         int64_t value = getIntegralValue(secondChild);
         bool useMGHI = secondChildIsConstant &&
            value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL &&
            secondChild->getRegister()==NULL;

         if (!useMGHI)
            {
            // Disable due to problems with SPILLs
            src2RegPair = (TR::RegisterPair *) cg->gprClobberEvaluate(secondChild);
            }

         //shift all 64 bits of multiplicand into trgtRegPair->getHighOrder()
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, trgtRegPair->getHighOrder(), trgtRegPair->getHighOrder(), 32);

         cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, trgtRegPair->getHighOrder(), trgtRegPair->getLowOrder());

         if (!useMGHI)
            {
            dependencies->addPostCondition(src2RegPair->getHighOrder(), TR::RealRegister::AssignAny);
            dependencies->addPostCondition(src2RegPair->getLowOrder(), TR::RealRegister::AssignAny);
            }

         if (!useMGHI)
            {
            //shift all 64 bits of multiplier into src2RegPair->getHighOrder()
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, src2RegPair->getHighOrder(), src2RegPair->getHighOrder(), 32);

            cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, src2RegPair->getHighOrder(), src2RegPair->getLowOrder());

            //do multiplication
            cursor = generateRRInstruction(cg, TR::InstOpCode::MSGR, node, trgtRegPair->getHighOrder(), src2RegPair->getHighOrder());
            }
         else
            {
            cursor = generateRIInstruction(cg, TR::InstOpCode::MGHI, node, trgtRegPair->getHighOrder(), value);
            }

         //shift product back into original trgtRegPair
         cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, trgtRegPair->getLowOrder(), trgtRegPair->getHighOrder());

         if (!useMGHI)
            {
            cg->stopUsingRegister(src2RegPair);
            }
         }
      else // Unsigned Multiply Support
         {
         src2RegPair = (TR::RegisterPair *) cg->evaluate(secondChild);

         TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);

         dependencies->addPostCondition(src2RegPair, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(src2RegPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(src2RegPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

         dependencies->addPostCondition(trgtRegPair, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(trgtRegPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(trgtRegPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

         //shift all 64 bits of multiplicand into trgtRegPair->getHighOrder()
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, trgtRegPair->getHighOrder(), trgtRegPair->getHighOrder(), 32);

         cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, trgtRegPair->getHighOrder(), trgtRegPair->getLowOrder());

         //put value in low order because the MLGR instruction will look for the multiplicand in register R1+1 (the odd in the pair)
         cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, trgtRegPair->getLowOrder(), trgtRegPair->getHighOrder());

         //shift all 64 bits of multiplier into src2RegPair->getHighOrder()
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, src2RegPair->getHighOrder(), src2RegPair->getHighOrder(), 32);

         cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, src2RegPair->getHighOrder(), src2RegPair->getLowOrder());

         //do multiplication
         cursor = generateRRInstruction(cg, TR::InstOpCode::MLGR, node, trgtRegPair, src2RegPair->getHighOrder());

         //shift product back into original trgtRegPair
         cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, trgtRegPair->getHighOrder(), trgtRegPair->getLowOrder());
         }

      // Common signed / unsigned clean-up code

      cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, trgtRegPair->getHighOrder(), trgtRegPair->getHighOrder(), 32);

      cursor->setDependencyConditions(dependencies);

      node->setRegister(trgtRegPair);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return trgtRegPair;
      }
   else
      {
      TR::Register * tempRegister = cg->allocateRegister();
      TR::Register * lowRegister = cg->allocateRegister();
      TR::Register * highRegister = cg->allocateRegister();

      TR::RegisterPair * tempRegisterPair = cg->allocateConsecutiveRegisterPair(lowRegister, highRegister);

      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * lmul1 = generateLabelSymbol(cg);
      TR::LabelSymbol * lmul2 = generateLabelSymbol(cg);


      TR::RegisterPair * src2RegPair = (TR::RegisterPair *) cg->gprClobberEvaluate(secondChild);

      TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 9, cg);
      dependencies->addPostCondition(tempRegisterPair, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(highRegister, TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(lowRegister, TR::RealRegister::LegalOddOfPair);
      dependencies->addPostCondition(src2RegPair, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(src2RegPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(src2RegPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);
      dependencies->addPostCondition(trgtRegPair, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(trgtRegPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(trgtRegPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

      // We can break down the lmul as such:
      //                     (Ahx2^32 + Al) ->  trgt
      //                   X (Bhx2^32 + Bl) ->  Src2
      //---------------------------------------
      //                      Bl*Ah   + Al*Bl
      //            Ah*Bh  +  Bh*Al
      //---------------------------------------
      //

      // Any terms in A and B that produce a product GE to 2^64
      // will cause overflow, and as such are ignored.
      //   -> So Ah*Bh is not computed.
      //   -> We only need lower words of Bl*Ah+Bh*Al

      generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegister, trgtRegPair->getHighOrder());
      generateRRInstruction(cg, TR::InstOpCode::LTR, node, tempRegisterPair->getLowOrder(), trgtRegPair->getLowOrder());

      // Prod = Al*Bl
      generateRRInstruction(cg, TR::InstOpCode::MR, node, trgtRegPair, src2RegPair->getLowOrder());

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, lmul1);

      generateRRInstruction(cg, TR::InstOpCode::ALR, node, trgtRegPair->getHighOrder(), src2RegPair->getLowOrder());

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, lmul1);

      generateRIInstruction(cg, TR::InstOpCode::CHI, node, src2RegPair->getLowOrder(), 0);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNM, node, lmul2);

      generateRRInstruction(cg, TR::InstOpCode::ALR, node, trgtRegPair->getHighOrder(), tempRegisterPair->getLowOrder());

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, lmul2, dependencies);
      lmul2->setEndInternalControlFlow();

      if (!(secondChildIsConstant && 0 == secondChild->getLongIntHigh()) && !secondHighZero)
         {
         // If second child is constant and has 0's for high order bits,
         // We know for sure that Ah * Bl is 0.
         // Ah*Bl
         generateRRInstruction(cg, TR::InstOpCode::MR, node, tempRegisterPair, src2RegPair->getHighOrder());

         // Prod += Ah*Bl<<32
         generateRRInstruction(cg, TR::InstOpCode::ALR, node, trgtRegPair->getHighOrder(), tempRegisterPair->getLowOrder());
         }

      // Al*Bh
      generateRRInstruction(cg, TR::InstOpCode::MR, node, src2RegPair, tempRegister);

      // Prod += Al*Bh<<32
      cursor = generateRRInstruction(cg, TR::InstOpCode::ALR, node, trgtRegPair->getHighOrder(), src2RegPair->getLowOrder());
      // Add a dependency to make the regist assignment a pair
      cursor->setDependencyConditions(dependencies);

      node->setRegister(trgtRegPair);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      cg->stopUsingRegister(tempRegister);
      cg->stopUsingRegister(tempRegisterPair);
      cg->stopUsingRegister(src2RegPair);
      return trgtRegPair;
      }
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
   TR::Register * lumulhTargetRegister = cg->allocate64bitRegister();
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
 * dualMulHelper32on64 - 32bit version of dual multiply Helper for 64bit registers
 */
TR::Register *
dualMulHelper32on64(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   bool needsUnsignedHighMulOnly = (lmulNode == NULL);
   // Both parts of multiplication required if !needsUnsignedHighMulOnly, can use 64bit MLGR to do it.
   // but need to extend 32bit registers to 64bit
   //
   // First combine 32bit reg pairs into a 64bit single reg for both multiplier and multiplicant, do the multiply
   // then split both the high 64bits and low 64bits of the 128 bit answer into 32bit reg pairs.
   //
   // ignore whether children are constant or zero, which may be suboptimal
   TR::Node * firstChild =  lumulhNode->getFirstChild();
   TR::Node * secondChild = lumulhNode->getSecondChild();

   TR::Instruction * cursor = NULL;
   TR::RegisterPair * lmulTargetRegister  = (TR::RegisterPair *) cg->gprClobberEvaluate(firstChild);
   TR::RegisterPair * lumulhTargetRegister = (TR::RegisterPair *) cg->gprClobberEvaluate(secondChild);

   // seems to need only three conditions, not six
   //TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);
   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);
   bool highWordRADisabled = !cg->supportsHighWordFacility() || comp->getOption(TR_DisableHighWordRA);
   // doesn't seem to need these conditions
   //dependencies->addPostCondition(lumulhTargetRegister, TR::RealRegister::EvenOddPair);
   //dependencies->addPostCondition(lumulhTargetRegister->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
   //dependencies->addPostCondition(lumulhTargetRegister->getLowOrder(), TR::RealRegister::LegalOddOfPair);

   dependencies->addPostCondition(lmulTargetRegister, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(lmulTargetRegister->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(lmulTargetRegister->getLowOrder(), TR::RealRegister::LegalOddOfPair);

   // shift all 64 bits of multiplicand into lmulTargetRegister->getHighOrder()
   cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lmulTargetRegister->getHighOrder(), lmulTargetRegister->getHighOrder(), 32);

   cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, lmulTargetRegister->getHighOrder(), lmulTargetRegister->getLowOrder());

   // put value in low order because the MLGR instruction will look for the multiplicand in register R1+1 (the odd in the pair)
   cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, lmulTargetRegister->getLowOrder(), lmulTargetRegister->getHighOrder());

   // shift all 64 bits of multiplier into lumulhTargetRegister->getHighOrder()
   cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lumulhTargetRegister->getHighOrder(), lumulhTargetRegister->getHighOrder(), 32);

   cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, lumulhTargetRegister->getHighOrder(), lumulhTargetRegister->getLowOrder());

   // do multiplication
   cursor = generateRRInstruction(cg, TR::InstOpCode::MLGR, node, lmulTargetRegister, lumulhTargetRegister->getHighOrder());

   // cannot do dependency here, because otherwise it will assume it is only a 32bit register, whereas it is 64bit

   // put high part of product into the lumulhTargetRegister, highOrder and lowOrder.
   cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, lumulhTargetRegister->getHighOrder(), lmulTargetRegister->getHighOrder());

   cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, lumulhTargetRegister->getLowOrder(), lmulTargetRegister->getHighOrder());

   // shift down the high 32bits of the high 64bits of the product for the highOrder in lumulhTargetRegister
   // but leave the low 32bits in lowOrder alone (upper 32bits will be ignored)
   cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, lumulhTargetRegister->getHighOrder(), lumulhTargetRegister->getHighOrder(), 32);

   // make sure low part of product is in the lumulhTargetRegister, both its highOrder and lowOrder.
   cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, lmulTargetRegister->getHighOrder(), lmulTargetRegister->getLowOrder());

   // shift down the high 32bits of the low 64bits of the product for the highOrder in lmulTargetRegister
   // but leave the low 32 bits in lowOrder alone (upper 32bits will be ignored)
   cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, lmulTargetRegister->getHighOrder(), lmulTargetRegister->getHighOrder(), 32);

   // have to do dependancy here, because need the 64bit context (virtual registers are 32 bit)
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
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return node->getRegister();
   }

/**
 * dualMulHelper32on32 - 32bit version of dual multiply Helper on 32 bit platforms
 */
TR::Register *
dualMulHelper32on32(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg)
   {
   //TR_ASSERTC( "Should never reach here,comp, 0, if 64bit registers always available on 32bit.");
   //traceMsg(comp, "Should never reach here, if 64bit registers always available on 32bit.\n");

   bool needsUnsignedHighMulOnly = (lmulNode == NULL);
   // Both parts of multiplication required if !needsUnsignedHighMulOnly.
   TR::Node * firstChild =  lumulhNode->getFirstChild();
   TR::Node * secondChild = lumulhNode->getSecondChild();

   // requires:
   //   7 registers: five general purpose: RR1, RR2, RR3, RR4, RR5,
   //                and an even-odd register pair used as accumulator for multiply: AC0:AC1
   // entry:
   //   RR5:RR1 = a = evaluate(firstChild)
   //   RR3:RR4 = b = evaluate(secondChild)
   //   RR1,RR3,RR4 are overwritten with result; AC1, AC0 are overwritten. RR5 is preserved.
   // exit
   //   RR4:RR3:RR2:RR1 = r = a * b
   // for simplicity will act as though RR5 is not preserved, and use clobberEvaluate

   //   1  LR     AC1, RR1      // AC1 = al
   //   2  MLR    AC0, RR4      // AC0:AC1 = al * bl
   //   3  XR     RR1, AC1      //
   //   3b XR     AC1, RR1      //
   //   3c XR     RR1, AC1      // RR1 = (al * bl)l; AC1 = al
   //   4  LR     RR2, AC0      // RR2 = (al * bl)h
   //   5  MLR    AC0, RR3      // AC0:AC1 = al * bh
   //   6  XR     RR3, AC0      //
   //   6b XR     AC0, RR3      //
   //   6c XR     RR3, AC0      // RR3 = (al * bh)h; AC0 = bh
   //   7  ALR    RR2, AC1      // RR2 = (al * bl)h + (al * bh)l
   //   8  LHI    AC1, 0
   //   8b ALCR   RR3, AC1      // AC1 = 0; RR3 = (al * bh)h + [carry]
   //   9  LR     AC1, RR5      // AC1 = ah
   //  10  MLR    AC0, AC0      // AC0:AC1 = ah * bh
   //  11  XR     RR4, AC0      //
   //  11b XR     AC0, RR4      //
   //  11c XR     RR4, AC0      // RR4 = (ah * bh)h; AC0 = bl
   //  12  ALR    RR3, AC1      // RR3 = (al * bh)h + (ah * bh)l
   //  13  LHI    AC1, 0
   //  13b ALCR   RR4, AC1      // RR4 = (ah * bh)h + [carry]
   //  14  LR     AC1, RR5      // AC1 = ah
   //  15  MLR    AC0, AC0      // AC0:AC1 = ah * bl
   //  16  ALR    RR2, AC1      // RR2 = (al * bl)h + (al * bh)l              + (ah * bl)l
   //  17  ALCR   RR3, AC0      // RR3 =              (al * bh)h + (ah * bh)l + (ah * bl)h + [carry]
   //  18  LHI    AC1, 0
   //  18b ALCR   RR4, AC1      // RR4 =                           (ah * bh)h              + [carry]
   TR::Instruction * cursor = NULL;
   TR::RegisterPair * aRegister  = (TR::RegisterPair *) cg->gprClobberEvaluate(firstChild);
   TR::RegisterPair * bRegister = (TR::RegisterPair *) cg->gprClobberEvaluate(secondChild);

   TR::Register * ac1  = cg->allocateRegister();
   TR::Register * ac0 = cg->allocateRegister();
   TR::Register * rr2 = cg->allocateRegister();
   TR::RegisterPair * accRegPair = cg->allocateConsecutiveRegisterPair(ac1, ac0);

   // conditions on the four MUL instructions
   TR::RegisterDependencyConditions  *dependencies;
   dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);
   dependencies->addPostCondition(accRegPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(accRegPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(accRegPair->getLowOrder(),  TR::RealRegister::LegalOddOfPair);

   //   RR5:RR1 = a = evaluate(firstChild)
   //   RR3:RR4 = b = evaluate(secondChild)
   TR::Register * rr5 = aRegister->getHighOrder();
   TR::Register * rr1 = aRegister->getLowOrder();
   TR::Register * rr3 = bRegister->getHighOrder();
   TR::Register * rr4 = bRegister->getLowOrder();

   //   RR4:RR3:RR2:RR1 = r = a * b
   TR::RegisterPair * lmulTargetRegister  = cg->allocateConsecutiveRegisterPair(rr1, rr2);
   TR::RegisterPair * lumulhTargetRegister = cg->allocateConsecutiveRegisterPair(rr3, rr4);
   //   1  LR     AC1, RR1      // AC1  = al
   cursor = generateRRInstruction(cg, TR::InstOpCode::LR  , node, ac1, rr1);
   //   2  MLR    AC0, RR4      // AC0:AC1 = al * bl
   cursor = generateRRInstruction(cg, TR::InstOpCode::MLR , node, accRegPair, rr4);
   // cursor->setDependencyConditions(depsMul[0]);
   //   3  XR     RR1, AC1      //
   //   3b XR     AC1, RR1      //
   //   3c XR     RR1, AC1      // RR1 = (al * bl)l; AC1 = al
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, rr1, ac1);
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, ac1, rr1);
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, rr1, ac1);
   //   4  LR     RR2, AC0      // RR2 = (al * bl)h
   cursor = generateRRInstruction(cg, TR::InstOpCode::LR  , node, rr2, ac0);
   //   5  MLR    AC0, RR3      // AC0:AC1 = al * bh
   cursor = generateRRInstruction(cg, TR::InstOpCode::MLR , node, accRegPair, rr3);
   // cursor->setDependencyConditions(depsMul[1]);
   //   6  XR     RR3, AC0      //
   //   6b XR     AC0, RR3      //
   //   6c XR     RR3, AC0      // RR3 = (al * bh)h; AC0 = bh
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, rr3, ac0);
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, ac0, rr3);
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, rr3, ac0);
   //   7  ALR    RR2, AC1      // RR2 = (al * bl)h + (al * bh)l
   cursor = generateRRInstruction(cg, TR::InstOpCode::ALR , node, rr2, ac1);
   //   8  LHI    AC1, 0
   //   8b ALCR   RR3, AC1      // AC1 = 0; RR3 = (al * bh)h + [carry]
   cursor = generateRIInstruction(cg, TR::InstOpCode::LHI , node, ac1, 0);
   cursor = generateRRInstruction(cg, TR::InstOpCode::ALCR, node, rr3, ac1);
   //   9  LR     AC1, RR5      // AC1  = ah
   cursor = generateRRInstruction(cg, TR::InstOpCode::LR  , node, ac1, rr5);
   //  10  MLR    AC0, AC0       // AC0:AC1 = ah * bh
   cursor = generateRRInstruction(cg, TR::InstOpCode::MLR , node, accRegPair, ac0);
   // cursor->setDependencyConditions(depsMul[2]);
   //  11  XR     RR4, AC0      //
   //  11b XR     AC0, RR4      //
   //  11c XR     RR4, AC0      // RR4 = (ah * bh)h; AC0 = bl
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, rr4, ac0);
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, ac0, rr4);
   cursor = generateRRInstruction(cg, TR::InstOpCode::XR  , node, rr4, ac0);
   //  12  ALR    RR3, AC1      // RR3 = (al * bh)l + (ah * bh)l
   cursor = generateRRInstruction(cg, TR::InstOpCode::ALR , node, rr3, ac1);
   //  13  LHI    AC1, 0
   //  13b ALCR   RR4, AC1      // RR4 = (ah * bh)h + [carry]
   cursor = generateRIInstruction(cg, TR::InstOpCode::LHI , node, ac1, 0);
   cursor = generateRRInstruction(cg, TR::InstOpCode::ALCR, node, rr4, ac1);
   //  14  LR     AC1, RR5      // AC1  = ah
   cursor = generateRRInstruction(cg, TR::InstOpCode::LR  , node, ac1, rr5);
   //  15  MLR    AC0, AC0       // AC0:AC1 = ah * bl
   cursor = generateRRInstruction(cg, TR::InstOpCode::MLR , node, accRegPair, ac0);
   // cursor->setDependencyConditions(depsMul[3]);
   //  16  ALR    RR2, AC1      // RR2 = (al * bl)h + (al * bh)l              + (ah * bl)l
   cursor = generateRRInstruction(cg, TR::InstOpCode::ALR , node, rr2, ac1);
   //  17  ALCR    RR3, AC0      // RR3 =              (al * bh)l + (ah * bh)l + (ah * bl)h + [carry]
   cursor = generateRRInstruction(cg, TR::InstOpCode::ALCR, node, rr3, ac0);
   //  18  LHI    AC1, 0
   //  18b ALCR   RR4, AC1      // RR4 =                           (ah * bh)h              + [carry]
   cursor = generateRIInstruction(cg, TR::InstOpCode::LHI , node, ac1, 0);
   cursor = generateRRInstruction(cg, TR::InstOpCode::ALCR, node, rr4, ac1);
   cursor->setDependencyConditions(dependencies);

   if (!needsUnsignedHighMulOnly)
      {
      lmulNode->setRegister(lmulTargetRegister);
      }
   else
      {
      cg->stopUsingRegister(lmulTargetRegister);
      cg->stopUsingRegister(rr1);
      cg->stopUsingRegister(rr2);
      }

   lumulhNode->setRegister(lumulhTargetRegister);

   // ac0, ac1 and rr5 are no longer required, and neither are aRegister, bRegister, accRegPair
   cg->stopUsingRegister(ac0);
   cg->stopUsingRegister(ac1);
   cg->stopUsingRegister(rr5);
   cg->stopUsingRegister(aRegister);
   cg->stopUsingRegister(bRegister);
   cg->stopUsingRegister(accRegPair);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return node->getRegister();
   }

/**
 * dualMulHelper32- 32bit version of dual multiply Helper
 */
TR::Register *
OMR::Z::TreeEvaluator::dualMulHelper32(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   if (performTransformation(comp, "O^O Using 64bit multiply on 32bit platform for Quad mul.\n"))
      {
      return dualMulHelper32on64(node, lmulNode, lumulhNode, cg);
      }
   else
      {
      return dualMulHelper32on32(node, lmulNode, lumulhNode, cg);
      }
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

      if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
         {
         return TR::TreeEvaluator::dualMulHelper64(node, lmulNode, lumulhNode, cg);
         }
      else
         {
         return TR::TreeEvaluator::dualMulHelper32(node, lmulNode, lumulhNode, cg);
         }
      }
   }

/**
 * lmulHelper64- 64bit version of long multiply Helper
 */
TR::Register *
lmulHelper64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT(TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit(),
      "lmulHelper64() is for 64bit code-gen only!");
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
      PRINT_ME("lconst", node, cg);
      int64_t value = secondChild->getLongInt();

      // LA Ry,(Rx,Rx) is the best instruction scale array index by 2
      // because it will create AGI of only 1 cycle, instead of 5 as SLL
      // but we need to make sure that  LA Ry,(Rx,Rx) itself does not create
      // AGI with preceeding instruction. Hence the check if FirstChild
      // has already been evaluated (by checking if it has a register).

      bool create_LA = false;
      if (firstChild->getRegister() != NULL &&
          value == 2 &&
          !cg->use64BitRegsOn32Bit()) // 3 way AGEN LA is cracked on zG
         {
         create_LA = true;
         }

      sourceRegister = cg->evaluate(firstChild);
      bool canClobber = cg->canClobberNodesRegister(firstChild);
      targetRegister = !canClobber ? cg->allocate64bitRegister() : sourceRegister;

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
   cg->ensure64BitRegister(targetRegister);
   return targetRegister;
   }


/**
 * baddEvaluator - add 2 bytes
 */
TR::Register *
OMR::Z::TreeEvaluator::baddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("badd", node, cg);

   return generic32BitAddEvaluator(node, cg);
   }

/**
 * saddEvaluator - add 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::saddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("sadd", node, cg);

   return generic32BitAddEvaluator(node, cg);
   }

/**
 * caddEvaluator - unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::caddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("cadd", node, cg);

   return generic32BitAddEvaluator(node, cg);
   }

/**
 * isubEvaluator - subtract 2 integers or subtract a short from an integer
 * (child1 - child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::isubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("isub", node, cg);

   if ((node->getOpCodeValue() == TR::asub) && TR::Compiler->target.is64Bit())
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
   PRINT_ME("lsub", node, cg);
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return lsubHelper64(node, cg);
      }
   else
      {
      return lsubHelper(node, cg);
      }
   }


/**
 * bsubEvaluator - subtract 2 bytes
 * (child1 - child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bsubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("bsub", node, cg);

   return generic32BitSubEvaluator(node, cg);
   }

/**
 * ssubEvaluator - subtract 2 short integers
 * (child1 - child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::ssubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("ssub", node, cg);

   return generic32BitSubEvaluator(node, cg);
   }

/**
 * csubEvaluator - subtract 2 unsigned short integers
 * (child1 - child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::csubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("csub", node, cg);

   return generic32BitSubEvaluator(node, cg);
   }

/**
 * lmulhEvaluator - multiply 2 long words but the result is the high word
 */
TR::Register *
OMR::Z::TreeEvaluator::lmulhEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("lmulh", node, cg);

   bool needsUnsignedHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   if (node->isDualCyclic() || needsUnsignedHighMulOnly)
      {
      return TR::TreeEvaluator::dualMulEvaluator(node, cg);
      }

   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Register * firstRegister = cg->gprClobberEvaluate(firstChild);
   TR::Register * targetRegister = cg->allocate64bitRegister();
   TR::Register * sourceRegister;
   TR::Register * resultRegister;
   TR::Compilation *comp = cg->comp();
   bool highWordRADisabled = !cg->supportsHighWordFacility() || comp->getOption(TR_DisableHighWordRA);

   if (TR::Compiler->target.is32Bit() && !cg->use64BitRegsOn32Bit())
      {
      // Turn 32-bit input reg-pair into a single 64-bit register
      // The first child will represent its 64-bit value in the low words of
      // of an even/odd pair of regs

      TR_ASSERT(firstRegister->getRegisterPair(), "lmulh expected to have reg pair as first child\n");
      TR::Register * srcRegH = firstRegister->getHighOrder();
      TR::Register * srcRegL = firstRegister->getLowOrder();

      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, srcRegH, srcRegH, 32);

      generateRRInstruction(cg, TR::InstOpCode::LR, node, srcRegH, srcRegL);

      generateRRInstruction(cg, TR::InstOpCode::LGR, node, srcRegL, srcRegH);

      sourceRegister = srcRegL;
      }
   else
      {
      sourceRegister = firstRegister;
      }

   TR::RegisterDependencyConditions * dependencies =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
   TR::RegisterPair * targetRegisterPair = cg->allocateConsecutiveRegisterPair(sourceRegister, targetRegister);

   dependencies->addPostCondition(targetRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(targetRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(sourceRegister, TR::RealRegister::LegalOddOfPair);

   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      PRINT_ME("lconst", node, cg);

      int64_t value = secondChild->getLongInt();
      int64_t absValue = value > 0 ? value : -value;

      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * posMulh = generateLabelSymbol(cg);
      TR::LabelSymbol * doneMulh = generateLabelSymbol(cg);

      // positive first child, branch to posMulh label
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, sourceRegister, (signed char)0, TR::InstOpCode::COND_BNL, posMulh);

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

   // Turn 64-bit register output back into 32-bit registers
   //
   if (TR::Compiler->target.is32Bit() && !cg->use64BitRegsOn32Bit())
      {
      //  LMG will have put the high 64-bits of the result in targetRegisterPair->getHighOrder()
      //  We need to split this into two 32-bit values with the high word going into
      //  the even register's low word and the low word going into the odd register's low word
      //
      generateRRInstruction(cg, TR::InstOpCode::LGR, node, targetRegisterPair->getLowOrder(), targetRegisterPair->getHighOrder());

      TR::Instruction * cursor;
      cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, targetRegisterPair->getHighOrder(), targetRegisterPair->getHighOrder(), 32);
      cursor->setDependencyConditions(dependencies);

      resultRegister = targetRegisterPair;
      cg->stopUsingRegister(firstRegister);
      }
   else
      {
      resultRegister = targetRegister;
      }

   node->setRegister(resultRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(targetRegisterPair);
   return resultRegister;
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
   PRINT_ME("imulh", node, cg);
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

/**
 * imulEvaluator - multiply 2 integers
 */
TR::Register *
OMR::Z::TreeEvaluator::imulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("imul", node, cg);
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
      PRINT_ME("iconst", node, cg);
      int32_t value = secondChild->getInt();

      // LA Ry,(Rx,Rx) is the best instruction scale array index by 2
      // because it will create AGI of only 1 cycle, instead of 5 as SLL
      // but we need to make sure that  LA Ry,(Rx,Rx) itself does not create
      // AGI with preceeding instruction. Hence the check if FirstChild
      // has already been evaluated (by checking if it has a register).

      bool create_LA = false;
      if ((firstChild->getRegister() != NULL) && (value == 2) && (node->isNonNegative() && !cg->use64BitRegsOn32Bit()))
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
      if (!node->getOpCode().isUnsigned())
         {
         TR_S390BinaryCommutativeAnalyser temp(cg);
         temp.genericAnalyser(node, TR::InstOpCode::MSR, TR::InstOpCode::MS, TR::InstOpCode::LR);
         targetRegister = node->getRegister();
         }
      else //Unsigned Multiply Support
         {
         TR::Register * secondRegister = cg->evaluate(secondChild);
         TR::Instruction * cursor = NULL;
         TR::Register * trgtRegPairFirst = cg->gprClobberEvaluate(firstChild);
         TR::Register * trgtRegPairSecond = cg->allocateRegister();
         TR::RegisterPair * trgtRegPair = cg->allocateConsecutiveRegisterPair(trgtRegPairFirst, trgtRegPairSecond);

         TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);

         dependencies->addPostCondition(trgtRegPair, TR::RealRegister::EvenOddPair);
         dependencies->addPostCondition(trgtRegPair->getHighOrder(), TR::RealRegister::LegalEvenOfPair);
         dependencies->addPostCondition(trgtRegPair->getLowOrder(), TR::RealRegister::LegalOddOfPair);

         //do multiplication
         cursor = generateRRInstruction(cg, TR::InstOpCode::MLR, node, trgtRegPair, secondRegister);

         cursor->setDependencyConditions(dependencies);

         node->setRegister(trgtRegPairFirst);
         cg->stopUsingRegister(trgtRegPairSecond);
         cg->stopUsingRegister(trgtRegPair);
         targetRegister = trgtRegPair->getLowOrder();
         }
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
   PRINT_ME("lmul", node, cg);
   bool needsUnsignedHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   if (node->isDualCyclic() || needsUnsignedHighMulOnly)
      {
      return TR::TreeEvaluator::dualMulEvaluator(node, cg);
      }

   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return lmulHelper64(node, cg);
      }
   else
      {
      return lmulHelper(node, cg);
      }
   }

/**
 * bmulEvaluator - multiply 2 bytes
 */
TR::Register *
OMR::Z::TreeEvaluator::bmulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("bmul", node, cg);
   TR_ASSERT( 0,"bmulEvaluator: not implemented\n");
   return NULL;
   }

/**
 * smulEvaluator - multiply 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::smulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("smul", node, cg);
   TR_ASSERT( 0, "smulEvaluator: not implemented\n");
   return NULL;
   }

/**
 * idivEvaluator -  divide 2 integers
 * (child1 / child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::idivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("idiv", node, cg);

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
   PRINT_ME("ldiv", node, cg);
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return lDivRemGenericEvaluator64(node, cg, DIVISION);
      }
   else
      {
      return lDivRemGenericEvaluator(node, cg, DIVISION);
      }
   }

/**
 * bdivEvaluator -  divide 2 bytes
 * (child1 / child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bdivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("bdiv", node, cg);
   TR_ASSERT( 0, "bdivEvaluator: not implemented\n");
   return NULL;
   }

/**
 * sdivEvaluator -  divide 2 short integers
 * (child1 / child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::sdivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("sdiv", node, cg);
   TR_ASSERT( 0,"sdivEvaluator: not implemented\n");
   return NULL;
   }

/**
 * iremEvaluator -  remainder of 2 integers
 * (child1 % child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::iremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("irem", node, cg);

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
   PRINT_ME("lrem", node, cg);
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return lDivRemGenericEvaluator64(node, cg, REMAINDER);
      }
   else
      {
      return lDivRemGenericEvaluator(node, cg, REMAINDER);
      }
   }

/**
 * bremEvaluator -  remainder of 2 bytes
 * (child1 % child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("brem", node, cg);
   TR_ASSERT( 0, "bremEvaluator: not implemented\n");
   return NULL;
   }

/**
 * sremEvaluator -  remainder of 2 short integers
 * (child1 % child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::sremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("srem", node, cg);
   TR_ASSERT( 0, "sremEvaluator: not implemented\n");
   return NULL;
   }

/**
 * inegEvaluator - negate an integer
 */
TR::Register *
OMR::Z::TreeEvaluator::inegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("ineg", node, cg);
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
      if (TR::Compiler->target.is64Bit() && targetRegister->alreadySignExtended())
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
      if (TR::Compiler->target.is64Bit() && targetRegister->alreadySignExtended())
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
   PRINT_ME("lneg", node, cg);
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      TR::Register * sourceRegister;
      targetRegister = cg->allocate64bitRegister();

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
      }
   else // if 32bit codegen
      {
      TR::RegisterPair * targetRegisterPair = (TR::RegisterPair *) cg->gprClobberEvaluate(firstChild);
      targetRegister = (TR::Register *) lnegFor32Bit(node, cg, targetRegisterPair);
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
   PRINT_ME("bneg", node, cg);
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
   PRINT_ME("sneg", node, cg);
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
   PRINT_ME("ishl", node, cg);

   auto altShiftOp = TR::InstOpCode::SLLG;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   PRINT_ME("lshl", node, cg);
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return genericLongShiftSingle(node, cg, TR::InstOpCode::SLLG);
      }
   else
      {
      return genericLongShift(node, cg, TR::InstOpCode::SLDL);
      }
   }

/**
 * bshlEvaluator - shift byte left
 * (child1 << child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bshlEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("bshl", node, cg);

   auto altShiftOp = TR::InstOpCode::SLLG;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   PRINT_ME("sshl", node, cg);

   auto altShiftOp = TR::InstOpCode::SLLG;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   PRINT_ME("ishr", node, cg);
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
   PRINT_ME("lshr", node, cg);
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return genericLongShiftSingle(node, cg, TR::InstOpCode::SRAG);
      }
   else
      {
      return genericLongShift(node, cg, TR::InstOpCode::SRDA);
      }
   }

/**
 * bshrEvaluator - shift byte right arithmetically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bshrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("bshr", node, cg);
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
   PRINT_ME("sshr", node, cg);
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
   PRINT_ME("iushr", node, cg);

   auto altShiftOp = TR::InstOpCode::SRLG;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   PRINT_ME("lushr", node, cg);
   if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
      {
      return genericLongShiftSingle(node, cg, TR::InstOpCode::SRLG);
      }
   else
      {
      return genericLongShift(node, cg, TR::InstOpCode::SRDL);
      }
   }

/**
 * bushrEvaluator - shift byte right logically
 * (child1 >> child2)
 */
TR::Register *
OMR::Z::TreeEvaluator::bushrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("bushr", node, cg);

   auto altShiftOp = TR::InstOpCode::SRLG;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   PRINT_ME("sushr", node, cg);

   auto altShiftOp = TR::InstOpCode::SRLG;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
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
   bool useRegPairs = TR::Compiler->target.is32Bit() && !cg->use64BitRegsOn32Bit() && node->getOpCodeValue() == TR::lrol;

   if (secondChild->getOpCode().isLoadConst())
      {
      intptrj_t rotateAmount = secondChild->getConstValue();
      if (rotateAmount == 0)
         {
         targetRegister = cg->evaluate(firstChild);
         }
      else
         {
         sourceRegister = cg->evaluate(firstChild);
         targetRegister = nodeIs64Bit ? cg->allocate64bitRegister() : cg->allocateRegister();
         //generateRSYInstruction(cg, nodeIs64Bit ? TR::InstOpCode::RLLG : TR::InstOpCode::RLL, node,targetRegister,0,0);
         //generateRegImmInstruction(ROLRegImm1(nodeIs64Bit), node, targetRegister, rotateAmount , cg);
         generateRSInstruction(cg, nodeIs64Bit ? TR::InstOpCode::RLLG : TR::InstOpCode::RLL, node, targetRegister, sourceRegister, rotateAmount);
         }
      }
   else if(useRegPairs)
      {
      sourceRegister = cg->evaluate(firstChild);
      targetRegister = cg->allocateConsecutiveRegisterPair();
      TR::Register * scratchReg = cg->allocate64bitRegister();

      // Shift high order by 32 bit and loaded into temp scratch register
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, scratchReg, sourceRegister->getHighOrder(), 32);

      // Now move the low order into the low half of temp scratch register
      generateRRInstruction(cg, TR::InstOpCode::LR, node, scratchReg, sourceRegister->getLowOrder());


      // generate the memref from register of 2nd child using the high order
      TR::Register * src2reg = cg->evaluate(secondChild);
      if(src2reg->getRegisterPair()) src2reg = src2reg->getHighOrder();
      TR::MemoryReference * memRef = generateS390MemoryReference(src2reg, 0, cg);

      // Now rotate left on the 64bit scratch register
      generateRSInstruction(cg, TR::InstOpCode::RLLG, node, scratchReg, scratchReg, memRef);

      // Now copy the result back into register pair
      generateRRInstruction(cg, TR::InstOpCode::LR, node, targetRegister->getLowOrder(), scratchReg);
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, scratchReg, scratchReg, 32);
      generateRRInstruction(cg, TR::InstOpCode::LR, node, targetRegister->getHighOrder(), scratchReg);
      cg->stopUsingRegister(scratchReg);
      }
   else
      {
      sourceRegister = cg->evaluate(firstChild);
      targetRegister = nodeIs64Bit ? cg->allocate64bitRegister() : cg->allocateRegister();

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
   PRINT_ME("iand", node, cg);
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
      PRINT_ME("iconst", node, cg);
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
   PRINT_ME("land", node, cg);
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if ((TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) &&
       ((targetRegister = genericRotateAndInsertHelper(node, cg))
             || (targetRegister = TR::TreeEvaluator::tryToReplaceLongAndWithRotateInstruction(node, cg))))
      {
      node->setRegister(targetRegister);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return targetRegister;
      }

   // LL: if second child is long constant & bitwiseOpNeedsLiteralFromPool returns false,
   // then we don't need to put the constant in literal pool, so we'll evaluate first child and
   // use AND Immediate instructions.
   if (secondChild->getOpCodeValue() == TR::lconst)
      {
      targetRegister = cg->gprClobberEvaluate(firstChild);

      if (cg->use64BitRegsOn32Bit())
         {
         generateS390ImmOp(cg, TR::InstOpCode::NG, node, targetRegister, targetRegister, secondChild->getLongInt());
         }
      else
         {
         generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, targetRegister, targetRegister, secondChild->getLongInt());
         }
      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
         {
         temp.genericAnalyser(node, TR::InstOpCode::NGR, TR::InstOpCode::NG, TR::InstOpCode::LGR);
         }
      else
         {
         temp.genericLongAnalyser(node, TR::InstOpCode::NR, TR::InstOpCode::NR, TR::InstOpCode::N, TR::InstOpCode::N, TR::InstOpCode::getLoadRegOpCode());
         }
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
   PRINT_ME("band", node, cg);
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

/**
 * sandEvaluator - boolean and of 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::sandEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("sand", node, cg);
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }

/**
 * candEvaluator - boolean and of 2 unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::candEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("cand", node, cg);
   return TR::TreeEvaluator::iandEvaluator(node, cg);
   }


/**
 * iorEvaluator - boolean or of 2 integers
 */
TR::Register *
OMR::Z::TreeEvaluator::iorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("ior", node, cg);
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst())
      {
      PRINT_ME("iconst", node, cg);
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
   PRINT_ME("lor", node, cg);
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   // See if rotate-left can be used
   //
   if ((TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) &&
         (targetRegister = genericRotateLeft(node, cg)))
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

      if (cg->use64BitRegsOn32Bit())
         {
         generateS390ImmOp(cg, TR::InstOpCode::OG, node, targetRegister, targetRegister, secondChild->getLongInt());
         }
      else
         {
         generateS390ImmOp(cg, TR::InstOpCode::getOrOpCode(), node, targetRegister, targetRegister, secondChild->getLongInt());
         }

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
         {
         temp.genericAnalyser(node, TR::InstOpCode::OGR, TR::InstOpCode::OG, TR::InstOpCode::LGR);
         }
      else
         {
         temp.genericLongAnalyser(node, TR::InstOpCode::OR, TR::InstOpCode::OR, TR::InstOpCode::O, TR::InstOpCode::O, TR::InstOpCode::getLoadRegOpCode());
         }
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
   PRINT_ME("bor", node, cg);
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

/**
 * sorEvaluator - boolean or of 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::sorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("sor", node, cg);
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

/**
 * corEvaluator - boolean or of 2 unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::corEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("cor", node, cg);
   return TR::TreeEvaluator::iorEvaluator(node, cg);
   }

/**
 * ixorEvaluator - boolean xor of 2 integers
 */
TR::Register *
OMR::Z::TreeEvaluator::ixorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("ixor", node, cg);
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst())
      {
      PRINT_ME("iconst", node, cg);
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
   PRINT_ME("lxor", node, cg);
   TR::Register * targetRegister = NULL;
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   // See if rotate-left can be used
   //
   if ((TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) &&
         (targetRegister = genericRotateLeft(node, cg)))
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

      if (cg->use64BitRegsOn32Bit())
         {
         generateS390ImmOp(cg, TR::InstOpCode::XG, node, targetRegister, targetRegister, secondChild->getLongInt());
         }
      else
         {
         generateS390ImmOp(cg, TR::InstOpCode::getXOROpCode(), node, targetRegister, targetRegister, secondChild->getLongInt());
         }

      node->setRegister(targetRegister);
      }
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      if (TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit())
         {
         temp.genericAnalyser(node, TR::InstOpCode::XGR, TR::InstOpCode::XG, TR::InstOpCode::LGR);
         }
      else
         {
         temp.genericLongAnalyser(node, TR::InstOpCode::XR, TR::InstOpCode::XR, TR::InstOpCode::X, TR::InstOpCode::X, TR::InstOpCode::getLoadRegOpCode());
         }
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
   PRINT_ME("bxor", node, cg);
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

/**
 * sxorEvaluator - boolean xor of 2 short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::sxorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("sxor", node, cg);
   return TR::TreeEvaluator::ixorEvaluator(node, cg);
   }

/**
 * cxorEvaluator - boolean xor of 2 unsigned short integers
 */
TR::Register *
OMR::Z::TreeEvaluator::cxorEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   PRINT_ME("cxor", node, cg);
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
