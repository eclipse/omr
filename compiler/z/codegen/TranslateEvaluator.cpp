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

#include "z/codegen/TranslateEvaluator.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/S390Evaluator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"

namespace TR { class Register; }

/**
 * The snippet does a CC compare
 *  trt
 *    ....
 *  ificmpeq
 *    ==>trt
 *    iconst 1
 *
 * The CC's only possible value is 0/1/2 in this case.
 * We can avoid the CC compare by directly branching on the CC that is still in effect right after trt.
 * It is necessary to map mask and compare value into a semantically equivalent branching condition for direct use.
 */
TR::InstOpCode::S390BranchCondition getConditionCodeFromCCCompare(uint8_t maskOnIf, uint8_t compVal)
   {
   uint32_t maskOnIfIndex = 0;

   TR_ASSERT(compVal == 0  || compVal == 1  || compVal == 2,
           "compVal (%d) should be in range of 0/1/2\n", compVal);

   //maskOnIf represents one of 6 possible if-cmps.
   switch (maskOnIf)
      {
      case 0x08:  maskOnIfIndex = 0; break;   //ifXcmpeq
      case 0x06:  maskOnIfIndex = 1; break;   //ifXcmpne
      case 0x04:  maskOnIfIndex = 2; break;   //ifXcmplt
      case 0x0C:  maskOnIfIndex = 3; break;   //ifXcmple
      case 0x02:  maskOnIfIndex = 4; break;   //ifXcmpgt
      case 0x0A:  maskOnIfIndex = 5; break;   //ifXcmpge
      default:   TR_ASSERT(0, "maskOnIf (%d) is not supported\n", maskOnIf); break;
      }

   static TR::InstOpCode::S390BranchCondition convertTable[6][3] =
      { // 0            1            2
         { TR::InstOpCode::COND_MASK8,  TR::InstOpCode::COND_MASK4,  TR::InstOpCode::COND_MASK2  }, //ifXcmpeq
         { TR::InstOpCode::COND_MASK6,  TR::InstOpCode::COND_MASK10, TR::InstOpCode::COND_MASK12 }, //ifXcmpne
         { TR::InstOpCode::COND_MASK0,  TR::InstOpCode::COND_MASK8,  TR::InstOpCode::COND_MASK12 }, //ifXcmplt
         { TR::InstOpCode::COND_MASK8,  TR::InstOpCode::COND_MASK12, TR::InstOpCode::COND_MASK14 }, //ifXcmple
         { TR::InstOpCode::COND_MASK6,  TR::InstOpCode::COND_MASK2,  TR::InstOpCode::COND_MASK0  }, //ifXcmpgt
         { TR::InstOpCode::COND_MASK14, TR::InstOpCode::COND_MASK6,  TR::InstOpCode::COND_MASK2  }  //ifXcmpge
      };

   return convertTable[maskOnIfIndex][compVal];
   }

TR::Register *inlineTrtEvaluator(
   TR::Node *node,
   TR::CodeGenerator *cg,
   TR::InstOpCode::Mnemonic opCode,
   bool isFoldedIf,
   bool isFoldedLookup,
   TR::Node *parent)
   {
   TR::Compilation *comp = cg->comp();
   bool storeRegs = node->getNumChildren() == 5;
   bool isSimple = node->getOpCodeValue() == TR::trtSimple;
   bool packR2 = node->getOpCodeValue() == TR::trt && !storeRegs && !isFoldedLookup;
   int32_t defaultCCMask = 0;


   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tinlineTrtEvaluator %s (%p) -- numChildren %d, storeR1AndR2=%d, isSimple=%d, packR2=%d\n",
         node->getOpCode().getName(),node,
         node->getNumChildren(),storeRegs,isSimple,packR2);

   TR::Node *sourceNode = node->getChild(0);
   TR::Node *tableNode  = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   TR::Node *targetR2Node    = storeRegs ? node->getChild(3)          : NULL;
   TR::Node *targetR1Node    = storeRegs ? node->getChild(4)          : NULL;
   TR::Register *targetR1Reg = storeRegs ? cg->evaluate(targetR1Node) : NULL;
   TR::Register *targetR2Reg = storeRegs ? cg->evaluate(targetR2Node) : NULL;

   TR::Register *sourceReg = sourceNode->getRegister();
   TR::Register *tableReg = tableNode->getRegister();

   TR::MemoryReference *sourceMemRef = sourceReg ? generateS390MemoryReference(sourceReg, 0, cg) : generateS390MemoryReference(cg, node, sourceNode, 0, true);
   TR::MemoryReference *tableMemRef = tableReg  ? generateS390MemoryReference(tableReg, 0, cg) : generateS390MemoryReference(cg, node, tableNode, 0, true);

   TR::Register *r1Reg = cg->allocateRegister();
   TR::Register *r2Reg = cg->allocateRegister();

   if (packR2 && !cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
      {
      generateRRInstruction(cg, TR::InstOpCode::XR, node, r2Reg, r2Reg);
      }

   TR::RegisterDependencyConditions *regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
   regDeps->addPostCondition(r1Reg, TR::RealRegister::GPR1,DefinesDependentRegister);
   regDeps->addPostCondition(r2Reg, TR::RealRegister::GPR2,DefinesDependentRegister);

   if (lengthNode->getOpCode().isLoadConst())
      {
      uint8_t length = lengthNode->getIntegerNodeValue<uint8_t>();
      cg->decReferenceCount(lengthNode);

      TR::Instruction *cursor = generateSS1WithImplicitGPRsInstruction(cg, opCode, node, length, sourceMemRef, tableMemRef, regDeps, NULL, NULL, r1Reg, r2Reg);
      }
   else
      {
      TR::Register *lengthReg = cg->evaluate(lengthNode);

      TR::Instruction *cursor = generateSS1WithImplicitGPRsInstruction(cg, opCode, node, 0, sourceMemRef, tableMemRef, regDeps, NULL, NULL, r1Reg, r2Reg);

      cursor = generateEXDispatch(node, cg, lengthReg, cursor);
      cursor->setDependencyConditions(regDeps);

      cg->decReferenceCount(lengthNode);
      }

   if ((opCode == TR::InstOpCode::TRTR) && (TR::Compiler->target.is32Bit()))
      {
      TR::MemoryReference *r1BitClearRef = generateS390MemoryReference(r1Reg, 0, cg);
      TR::Instruction *cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, r1Reg, r1BitClearRef);
      cursor->setDependencyConditions(regDeps);
      }

   if (packR2)
      {
      TR::Register *conditionCodeReg = getConditionCode(node, cg);

      if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_zEC12))
         {
         generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node,  conditionCodeReg, r2Reg, 48, 55, 8);
         }
      else if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
         {
         generateRIEInstruction(cg, TR::InstOpCode::RISBG, node,  conditionCodeReg, r2Reg, 48, 55, 8);
         }
      else
         {
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, r2Reg, 8);
         generateRRInstruction(cg, TR::InstOpCode::OR, node, conditionCodeReg, r2Reg);
         }

      node->setRegister(conditionCodeReg);
      }
   else if (!isSimple) // folded lookup case for TR::trt
      {
      TR_ASSERT(parent->getOpCodeValue() == TR::trtLookup,"The node %s (%p) is not trtLookup\n",
              parent->getOpCode().getName(),parent);

      TR::Register *tempReg = NULL;
      TR::InstOpCode::S390BranchCondition brCond = TR::InstOpCode::COND_NOP;
      TR::Instruction *cursor = NULL;
      int32_t numChildren = parent->getCaseIndexUpperBound();
      TR::Node *defaultRegDepChild = NULL;
      bool assignDepsToLastBranch = false;

      for (int32_t ii = 1; ii < numChildren; ii++)
         {
         bool needDefaultBranch = true;
         TR::TreeTop *caseDest = NULL;

         bool processDefault = (ii == numChildren-1);
         TR::Node * child = processDefault ? parent->getChild(1) : parent->getChild(ii+1);

         if (processDefault) //This is the default case
            {
            TR::Node *defaultNode = child;

            //check if a default branch is needed
            TR::TreeTop *defaultDest = defaultNode->getBranchDestination();
            TR::Block *defaultDestBlock = defaultDest->getNode()->getBlock();
            TR::Block *fallThroughBlock = cg->getCurrentEvaluationBlock()->getNextBlock();
            needDefaultBranch = defaultDestBlock != fallThroughBlock;

            caseDest = defaultDest;
            if (child->getNumChildren() > 0)
               {
               defaultRegDepChild = child->getFirstChild();
               assignDepsToLastBranch = true;
               }
            }
         else
            {
            //Need to check tryTwoBranchPattern for a proper conversion.
            TR_ASSERT(0, "Only caseCC and caseR2 supported\n");
            }

         if (child->getNumChildren() > 0)
            {
            // GRA
            cg->evaluate(child->getFirstChild());

            if (needDefaultBranch)
               cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, parent, caseDest->getNode()->getLabel(),
                     generateRegisterDependencyConditions(cg, child->getFirstChild(), 0));
            }
         else
            {
            if (needDefaultBranch)
               cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, parent, caseDest->getNode()->getLabel());
            }

         cg->decReferenceCount(child);
         }

      if (assignDepsToLastBranch && cursor->getDependencyConditions() == NULL)
         cursor->setDependencyConditions(generateRegisterDependencyConditions(cg, defaultRegDepChild, 0));

      if (tempReg)
         {
         cg->stopUsingRegister(tempReg);
         }
      }

   cg->stopUsingRegister(r2Reg);
   cg->stopUsingRegister(r1Reg);

   if (sourceReg)
      cg->decReferenceCount(sourceNode);
   if (tableReg)
      cg->decReferenceCount(tableNode);

   if (storeRegs)
      {
      cg->decReferenceCount(targetR1Node);
      cg->decReferenceCount(targetR2Node);
      }

   if (packR2)
      return node->getRegister();

   if (isFoldedLookup)
      {
      cg->decReferenceCount(node);
      return NULL;
      }

   if (isFoldedIf)
      cg->decReferenceCount(node);

   return OMR::Z::TreeEvaluator::getConditionCodeOrFoldIf(node, cg, isFoldedIf, parent);
   }

TR::Register *inlineTrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *sourceNode = node->getChild(0);
   TR::Node *tableNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   if(lengthNode->getOpCode().isLoadConst())
      {
      TR::Register *source = cg->evaluate(sourceNode);
      TR::MemoryReference *sourceMem = generateS390MemoryReference(source, 0, cg);
      TR::Register *table = cg->evaluate(tableNode);
      TR::MemoryReference *tableMem = generateS390MemoryReference(table, 0, cg);

      unsigned char length = lengthNode->getIntegerNodeValue<unsigned char>();
      cg->decReferenceCount(lengthNode);

      generateSS1Instruction(cg, TR::InstOpCode::TR, node, length, sourceMem, tableMem);

      cg->decReferenceCount(sourceNode);
      cg->decReferenceCount(tableNode);
      }
   else
      {
      TR::Register *source = cg->evaluate(sourceNode);
      TR::Register *table = cg->evaluate(tableNode);
      TR::Register *length = cg->evaluate(lengthNode);

      TR::Instruction * cursor = generateSS1Instruction(cg, TR::InstOpCode::TR, node, 0,
                                   new (cg->trHeapMemory()) TR::MemoryReference(source, 0, cg),
                                   new (cg->trHeapMemory()) TR::MemoryReference(table, 0, cg));

      cursor = generateEXDispatch(node, cg, length, cursor);

      cg->decReferenceCount(sourceNode);
      cg->decReferenceCount(tableNode);
      cg->decReferenceCount(lengthNode);
      }
   return NULL;
   }
