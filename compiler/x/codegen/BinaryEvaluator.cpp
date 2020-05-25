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
#include "codegen/Instruction.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/X86Evaluator.hpp"
#include "compile/Compilation.hpp"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "x/codegen/BinaryCommutativeAnalyser.hpp"
#include "x/codegen/DivideCheckSnippet.hpp"
#include "x/codegen/IntegerMultiplyDecomposer.hpp"
#include "x/codegen/SubtractAnalyser.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "env/CompilerEnv.hpp"

extern TR::Register *intOrLongClobberEvaluate(TR::Node *node, bool nodeIs64Bit, TR::CodeGenerator *cg);

///////////////////
//
// Hack markers
//

// Escape analysis can produce a tree that is not type correct; in particular a
// left-shift with an address first child (TR::loadaddr of local object used for
// initialization of the flags field of the local object).  In this case if the
// register is to be clobbered and if the child is evaluated in collected
// reference register, a copy is inserted so that the register used to evaluate
// the shl is not marked as collected. The ideal tr.dev fix should be the
// introduction of a TR::a2i / TR::a2l opcode in the IL. Causes Jgrinder test
// case GC failure.
//
#define SHIFT_MAY_HAVE_ADDRESS_CHILD (1)


// forceSize is a separate function from evaluateAndForceSize so the C++
// compiler can decide whether or not to inline it.  In contrast, we really
// want evaluateAndForceSize to be inlined because it can turn into evaluate()
// on IA32, and it has a lot of call sites.
//
static void forceSize(TR::Node *node, TR::Register *reg, bool is64Bit, TR::CodeGenerator *cg)
   {
   if (is64Bit && node->getSize() <= 4 && !node->isNonNegative())
      {
      // TODO:AMD64: Don't sign-extend the same register twice.  Perhaps use
      // the FP precision adjustment flag to indicate when a sign-extension has
      // already been done.
      generateRegRegInstruction(MOVSXReg8Reg4, node, reg, reg, cg);
      }
   }

// Used for aiadd, to sign-extend the second child on AMD64 from 4 to 8 bytes.
// Note that there is never a need to sign-extend the first child of any add.
inline TR::Register *evaluateAndForceSize(TR::Node *node, bool is64Bit, TR::CodeGenerator *cg)
   {
   TR::Register *reg = cg->evaluate(node);
   if (cg->comp()->target().is64Bit())
      {
      forceSize(node, reg, is64Bit, cg);
      }
   else
      {
      TR_ASSERT(!is64Bit, "64-bit expressions should be using register pairs on IA32");
      }
   return reg;
   }

bool OMR::X86::TreeEvaluator::analyseSubForLEA(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool                    nodeIs64Bit    = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Node                *firstChild     = node->getFirstChild();
   TR::Node                *secondChild    = node->getSecondChild();
   TR::ILOpCode            &firstOp        = firstChild->getOpCode();
   TR::MemoryReference  *leaMR          = NULL;
   int32_t                 stride         = 0;
   TR::Register            *targetRegister;
   TR::Register            *indexRegister;

   // sub
   //    ?           (firstChild)
   //    const       (secondChild)
   //
   TR_ASSERT(secondChild->getOpCode().isLoadConst(), "assertion failure");
   intptr_t displacement = -TR::TreeEvaluator::integerConstNodeValue(secondChild, cg);
   intptr_t dummyConstValue = 0;

   if (firstChild->getRegister() == NULL && firstChild->getReferenceCount() == 1)
      {
      stride = TR::MemoryReference::getStrideForNode(firstChild, cg);
      if (stride)
         {
         // sub
         //    mul/shl   (firstChild)
         //       ?
         //       stride
         //    const     (secondChild)
         //
         indexRegister = cg->evaluate(firstChild->getFirstChild());
         leaMR = generateX86MemoryReference(NULL,
                                         indexRegister,
                                         stride,
                                         displacement, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, leaMR, cg);
         cg->decReferenceCount(firstChild->getFirstChild());
         cg->decReferenceCount(firstChild->getSecondChild());
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         node->setRegister(targetRegister);
         return true;
         }

      if (firstOp.isAdd() &&
          TR::TreeEvaluator::constNodeValueIs32BitSigned(secondChild, &dummyConstValue, cg))
         {
         // sub
         //    add         (firstChild)
         //       ?        (llChild)
         //       ?        (lrchild)
         //    const       (secondChild)
         //
         TR::Node *llChild = firstChild->getFirstChild();
         TR::Node *lrChild = firstChild->getSecondChild();
         if (llChild->getRegister() == NULL    &&
             llChild->getReferenceCount() == 1 &&
             (stride = TR::MemoryReference::getStrideForNode(llChild, cg)))
            {
            // sub
            //    add         (firstChild)
            //       mul/shl  (llChild)
            //          ?
            //          stride
            //       ?        (lrchild)
            //    const       (secondChild)
            //
            leaMR = generateX86MemoryReference(cg->evaluate(lrChild),
                                            cg->evaluate(llChild->getFirstChild()),
                                            stride,
                                            displacement, cg);
            cg->decReferenceCount(llChild->getFirstChild());
            cg->decReferenceCount(llChild->getSecondChild());
            }
         else if (lrChild->getRegister() == NULL    &&
                  lrChild->getReferenceCount() == 1 &&
                  (stride = TR::MemoryReference::getStrideForNode(lrChild, cg)))
            {
            // sub
            //    add         (firstChild)
            //       ?        (llchild)
            //       mul/shl  (lrChild)
            //          ?
            //          stride
            //    const       (secondChild)
            //
            leaMR = generateX86MemoryReference(cg->evaluate(llChild),
                                            cg->evaluate(lrChild->getFirstChild()),
                                            stride,
                                            displacement, cg);
            cg->decReferenceCount(lrChild->getFirstChild());
            cg->decReferenceCount(lrChild->getSecondChild());
            }
         else
            {
            leaMR = generateX86MemoryReference(cg->evaluate(llChild),
                                            cg->evaluate(lrChild),
                                            0,
                                            displacement, cg);
            }
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, leaMR, cg);
         cg->decReferenceCount(llChild);
         cg->decReferenceCount(lrChild);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         node->setRegister(targetRegister);
         return true;
         }
      }

   return false;

   }

bool OMR::X86::TreeEvaluator::analyseAddForLEA(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (!performTransformation(cg->comp(), "O^O analyseAddForLEA\n"))
        return false;

   bool                 nodeIs64Bit    = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Node            *firstChild     = node->getFirstChild();
   TR::Node            *secondChild    = node->getSecondChild();
   TR::ILOpCode         &firstOp        = firstChild->getOpCode();
   TR::ILOpCode         &secondOp       = secondChild->getOpCode();
   TR::MemoryReference *leaMR          = NULL;
   int32_t              stride         = 0;
   int32_t              stride1        = 0;
   int32_t              stride2        = 0;
   TR::Node            *indexNode      = NULL;
   TR::Node            *baseNode       = NULL;
   TR::Node            *constNode      = NULL;
   TR::Register        *targetRegister = NULL;
   TR::Register        *indexRegister  = NULL;

   //TR_ASSERT(node->getSize() == firstChild->getSize(), "evaluateAndForceSize never needed for firstChild");

   if ((secondOp.isAdd() || secondOp.isSub()) &&
       secondChild->getReferenceCount() == 1 && secondChild->getRegister() == NULL)
      {
      TR::Node *addFirstChild   = secondChild->getFirstChild();
      TR::Node *addSecondChild  = secondChild->getSecondChild();
      TR::ILOpCode &addFirstOp  = addFirstChild->getOpCode();
      TR::ILOpCode &addSecondOp = addSecondChild->getOpCode();

      int32_t stride = TR::MemoryReference::getStrideForNode(addFirstChild, cg);

      if (stride &&
          addFirstChild->getReferenceCount() == 1 && addFirstChild->getRegister() == NULL &&
          addSecondOp.isLoadConst())
         {
         // add
         //    firstChild     (into baseRegister)
         //    add/sub        (secondChild)
         //       mul/shl     (addFirstChild)
         //          ?        (into indexNode)
         //          stride
         //       const       (addSecondChild)
         //

         intptr_t offset = TR::TreeEvaluator::integerConstNodeValue(addSecondChild, cg);
         if (secondOp.isSub())
            offset = -offset;

         TR::Register *baseRegister;
         if (firstChild->getOpCodeValue() == TR::loadaddr &&
             firstChild->getSymbolReference()->getSymbol()->isMethodMetaData())
            {
            baseRegister = cg->getMethodMetaDataRegister();
            offset += firstChild->getSymbolReference()->getOffset();
            }
         else
            {
            baseRegister = cg->evaluate(firstChild);
            }

         indexNode = addFirstChild->getFirstChild();
         indexRegister = evaluateAndForceSize(indexNode, nodeIs64Bit, cg);

         leaMR = generateX86MemoryReference(baseRegister, indexRegister, stride, offset, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, leaMR, cg);

         cg->decReferenceCount(addSecondChild);
         cg->decReferenceCount(addFirstChild->getFirstChild());
         cg->decReferenceCount(addFirstChild->getSecondChild());
         cg->decReferenceCount(addFirstChild);
         cg->decReferenceCount(secondChild);
         cg->decReferenceCount(firstChild);

         node->setRegister(targetRegister);

         return true;
         }
      }

   intptr_t dummyConstValue = 0;

   if (secondOp.isLoadConst())
      {
      constNode = secondChild;
      }
   if (firstChild->getRegister() == NULL && firstChild->getReferenceCount() == 1)
      {
      stride1 = TR::MemoryReference::getStrideForNode(firstChild, cg);
      }
   if (secondChild->getRegister() == NULL && secondChild->getReferenceCount() == 1)
      {
      stride2 = TR::MemoryReference::getStrideForNode(secondChild, cg);
      }
   if (stride1 | stride2)
      {
      if (stride1)
         {
         indexNode = firstChild;
         stride    = stride1;
         baseNode  = secondChild;
         }
      else
         {
         indexNode = secondChild;
         stride    = stride2;
         baseNode  = firstChild;
         }
      }
   if (indexNode)
      {
      // add           (** children may be reversed **)
      //    baseNode      (possibly also constNode)
      //    mul/shl       (indexNode)
      //       ?          (into indexRegister)
      //       stride
      //

      indexRegister = cg->evaluate(indexNode->getFirstChild());
      TR::Node *tempNode;
      if (constNode)
         {
         // can be a long for an aladd
         if (constNode->getOpCodeValue() == TR::lconst)
            leaMR = generateX86MemoryReference(NULL,
                                            indexRegister,
                                            stride,
                                            constNode->getLongInt(), cg);
         else
            leaMR = generateX86MemoryReference(NULL,
                                            indexRegister,
                                            stride,
                                            constNode->getInt(), cg);
         tempNode = NULL;
         }
      else if (baseNode->getRegister()       == NULL    &&
               baseNode->getReferenceCount() == 1       &&
               baseNode->getOpCode().isAdd()            &&
               baseNode->getSecondChild()->getOpCode().isLoadConst() &&
               TR::TreeEvaluator::constNodeValueIs32BitSigned(baseNode->getSecondChild(), &dummyConstValue, cg))
         {
         // add            (** children may be reversed **)
         //    add            (baseNode)
         //       ?
         //       const
         //    mul/shl        (indexNode)
         //       ?
         //       stride
         //
         TR_ASSERT(!constNode, "cannot have two const nodes in this pattern");
         constNode = baseNode->getSecondChild();
         leaMR = generateX86MemoryReference(cg->evaluate(baseNode->getFirstChild()),
                                         indexRegister,
                                         stride,
                                         TR::TreeEvaluator::integerConstNodeValue(constNode, cg), cg);
         tempNode = baseNode->getFirstChild();
         }
      else
         {
         leaMR = generateX86MemoryReference(cg->evaluate(baseNode),
                                         indexRegister,
                                         stride,
                                         0, cg);
         tempNode = baseNode;
         }
      targetRegister = cg->allocateRegister();
      generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, leaMR, cg);
      if (tempNode)
         {
         cg->decReferenceCount(tempNode);
         }
      cg->decReferenceCount(indexNode->getFirstChild());
      cg->decReferenceCount(indexNode->getSecondChild());
      cg->decReferenceCount(indexNode);
      if (constNode)
         {
         cg->decReferenceCount(constNode);
         }
      node->setRegister(targetRegister);
      return true;
      }
   else if (constNode  &&
         TR::TreeEvaluator::constNodeValueIs32BitSigned(constNode, &dummyConstValue, cg))
      {
      // add
      //    firstChild
      //    const
      //
      if (firstChild->getRegister()       == NULL &&
          firstChild->getReferenceCount() == 1    &&
          firstOp.isAdd())
         {
         // add
         //    add   (firstChild)
         //       ?
         //       ?
         //    const
         //
         // We should only traverse more deeply if firstChild's children do not have
         // associated registers. Otherwise, we risk extending the lifetimes of
         // firstChild's grandchildren's registers, and hence adding register pressure.
         //
         if (firstChild->getFirstChild()->getReferenceCount() == 1 &&
             firstChild->getFirstChild()->getRegister() == NULL)
            {
            stride1 = TR::MemoryReference::getStrideForNode(firstChild->getFirstChild(), cg);
            }
         if (firstChild->getSecondChild()->getReferenceCount() == 1 &&
             firstChild->getSecondChild()->getRegister() == NULL)
            {
            stride2 = TR::MemoryReference::getStrideForNode(firstChild->getSecondChild(), cg);
            }
         if (stride1 | stride2)
            {
            if (stride1)
               {
               // add
               //    add            (firstChild)
               //       mul/shl     (indexNode)
               //          ?
               //          stride1
               //       ?           (baseNode)
               //    const          (constNode)
               //

               baseNode  = firstChild->getSecondChild();
               indexNode = firstChild->getFirstChild()->getFirstChild();
               TR_ASSERT(node->getSize() == indexNode->getSize(), "evaluateAndForceSize never needed for indexNode");
               leaMR = generateX86MemoryReference(cg->evaluate(baseNode),
                                               cg->evaluate(indexNode),
                                               stride1,
                                               TR::TreeEvaluator::integerConstNodeValue(constNode, cg),
                                               cg);
               cg->decReferenceCount(firstChild->getFirstChild()->getSecondChild());
               cg->decReferenceCount(firstChild->getFirstChild());
               }
            else
               {
               // add
               //    add            (firstChild)
               //       ?           (baseNode)
               //       mul/shl     (indexNode)
               //          ?
               //          stride2
               //    const          (constNode)
               //
               baseNode  = firstChild->getFirstChild();
               indexNode = firstChild->getSecondChild()->getFirstChild();
               TR_ASSERT(node->getSize() == baseNode->getSize(), "evaluateAndForceSize never needed for baseNode");
               leaMR = generateX86MemoryReference(cg->evaluate(baseNode),
                                               cg->evaluate(indexNode),
                                               stride2,
                                               TR::TreeEvaluator::integerConstNodeValue(constNode, cg),
                                               cg);
               cg->decReferenceCount(firstChild->getSecondChild()->getSecondChild());
               cg->decReferenceCount(firstChild->getSecondChild());
               }
            }
         else
            {
            baseNode  = firstChild->getFirstChild();
            indexNode = firstChild->getSecondChild();
            TR_ASSERT(node->getSize() == baseNode->getSize(), "evaluateAndForceSize never needed for baseNode");
            if (indexNode->getOpCode().isLoadConst())
               {
               // add
               //    add
               //       ?        (baseNode)
               //       const    (indexNode)
               //    const
               //

               leaMR = generateX86MemoryReference(cg->evaluate(baseNode),
                                               TR::TreeEvaluator::integerConstNodeValue(constNode,cg) +
                                               TR::TreeEvaluator::integerConstNodeValue(indexNode, cg), cg);
               }
            else
               {
               leaMR = generateX86MemoryReference(cg->evaluate(baseNode),
                                               cg->evaluate(indexNode),
                                               0,
                                               TR::TreeEvaluator::integerConstNodeValue(constNode, cg), cg);
               }
            }
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, leaMR, cg);
         cg->decReferenceCount(baseNode);
         cg->decReferenceCount(indexNode);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(constNode);
         node->setRegister(targetRegister);
         return true;
         }
      }
   else if(firstChild->getOpCode().getOpCodeValue() == TR::loadaddr &&
          (firstChild->getRegister() == NULL && firstChild->getReferenceCount() == 1))
      {
      // add
      //    loadaddr    (firstChild)
      //    ?           (secondChild)
      // fold parent add of 'loadaddr + secondChild' into lea generated for the loadaddr
      bool isInternal = node->getOpCode().isArrayRef()
                    &&  node->isInternalPointer()
                    &&  node->getPinningArrayPointer();
      leaMR = generateX86MemoryReference(firstChild->getSymbolReference(), cg);
      leaMR->populateMemoryReference(secondChild, cg);
      // generateLEAForLoadAddr calls leaMR->decNodeReferenceCounts(cg)
      //
      // leaMR is not first chid's memory reference, it also plus second second child,
      // so can't allocate register directly according to first kid's symbol
      targetRegister = TR::TreeEvaluator::generateLEAForLoadAddr(firstChild, leaMR, firstChild->getSymbolReference(), cg, isInternal);
      cg->decReferenceCount(firstChild);
      node->setRegister(targetRegister);
      return true;
      }
   else if((firstChild->getOpCode().isSub() || firstChild->getOpCode().isAdd()) &&
           (firstChild->getOpCode().getSize() == node->getOpCode().getSize()) &&
           (firstChild->getRegister() == NULL) &&
           (firstChild->getSecondChild()->getOpCode().isLoadConst()) &&
           TR::TreeEvaluator::constNodeValueIs32BitSigned(firstChild->getSecondChild(), &dummyConstValue, cg) &&
           (firstChild->getReferenceCount() == 1)
           )
      {
      // add
      //    sub/add    (firstChild)
      //        child 0
      //        loadconstant
      //    ?        (second)
      intptr_t val = TR::TreeEvaluator::integerConstNodeValue(firstChild->getSecondChild(), cg);
      if (firstChild->getOpCode().isSub())
         val = -val;
      leaMR = generateX86MemoryReference(cg->evaluate(firstChild->getFirstChild()),
                                         cg->evaluate(secondChild),
                                         0,
                                         val,
                                         cg);
      targetRegister = cg->allocateRegister();
      generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, leaMR, cg);
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild->getFirstChild());
      cg->decReferenceCount(firstChild->getSecondChild());
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return true;
      }
   return false;
   }

static void evaluateIfNotConst(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (!node->getOpCode().isLoadConst())
      {
      diagnostic("Evaluating Non Constant %p (%s)\n",
         node, node->getOpCode().getName());
      cg->evaluate(node);
      }
   else
      {
      diagnostic("Not evaluating Constant %p (%s)\n",
         node, node->getOpCode().getName());
      }
   }

TR::Register *OMR::X86::TreeEvaluator::integerDualAddOrSubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT((node->getOpCodeValue() == TR::luaddh) || (node->getOpCodeValue() == TR::ladd)
      || (node->getOpCodeValue() == TR::lusubh) || (node->getOpCodeValue() == TR::lsub)
      , "Unexpected dual operator. Expected ladd, luaddh, lsub, or lusubh as part of cyclic dual.");

   TR::Node *pair = node->getChild(2);
   bool requiresCarryOnEntry = cg->requiresCarry();
   if (pair->getReferenceCount() == 1)
      {
      diagnostic("Found %p (%s) with only reference to %p (%s)\n",
                   node, node->getOpCode().getName(),
                   pair, pair->getOpCode().getName());
      if (node->isDualHigh())
         {
         // for high part carry is still required, so evaluate the children for low part for this first
         diagnostic("Evaluating Children of %p (%s) for %p (%s)\n",
                      pair, pair->getOpCode().getName(),
                      node, node->getOpCode().getName());
         evaluateIfNotConst(pair->getFirstChild(), cg);
         evaluateIfNotConst(pair->getSecondChild(), cg);
         }
      else
         {
         // for low part, high part is not required, but children must be evaluated.
         diagnostic("Evaluating Children of %p (%s) for %p (%s) (not required for quad calc)\n",
                      pair, pair->getOpCode().getName(),
                      node, node->getOpCode().getName());
         evaluateIfNotConst(pair->getFirstChild(), cg);
         evaluateIfNotConst(pair->getSecondChild(), cg);
         cg->decReferenceCount(pair->getFirstChild());
         cg->decReferenceCount(pair->getSecondChild());
         }
      diagnostic("Evaluating Children of %p (%s) after those of %p (%s)\n",
                   node, node->getOpCode().getName(),
                   pair, pair->getOpCode().getName());
      evaluateIfNotConst(node->getFirstChild(), cg);
      evaluateIfNotConst(node->getSecondChild(), cg);

      if (node->isDualHigh())
         {
         // for high part carry is still required, so evaluate the low part for this first
         diagnostic("Evaluating %p (%s) for %p (%s)\n",
                      pair, pair->getOpCode().getName(),
                      node, node->getOpCode().getName());
         cg->setComputesCarry(true);
         cg->evaluate(pair);
         }
      diagnostic("Evaluating %p (%s) after %p (%s)\n",
                   node, node->getOpCode().getName(),
                   pair, pair->getOpCode().getName());
      cg->setRequiresCarry(true);
      cg->setComputesCarry(true);
      cg->evaluate(node);
      cg->decReferenceCount(pair);
      cg->decReferenceCount(node);
      }
   else
      {
      diagnostic("Found %p (%s) for %p (%s)\n",
            pair, pair->getOpCode().getName(),
            node, node->getOpCode().getName());
      TR::Node *lowNode  = node->isDualHigh() ? pair : node;
      TR::Node *highNode = lowNode->getChild(2);

      // Evaluate the children for low part for this first
      diagnostic("Evaluating Children of %p (%s) for %p (%s)\n",
         lowNode,  lowNode->getOpCode().getName(),
         highNode, highNode->getOpCode().getName());
      evaluateIfNotConst(lowNode->getFirstChild(), cg);
      evaluateIfNotConst(lowNode->getSecondChild(), cg);
      diagnostic("Evaluating Children of %p (%s) after those for %p (%s)\n",
         highNode, highNode->getOpCode().getName(),
         lowNode,  lowNode->getOpCode().getName());
      evaluateIfNotConst(highNode->getFirstChild(), cg);
      evaluateIfNotConst(highNode->getSecondChild(), cg);

      diagnostic("Evaluating %p (%s) for %p (%s)\n",
         lowNode, lowNode->getOpCode().getName(),
         highNode, highNode->getOpCode().getName());
      cg->setComputesCarry(true);
      cg->evaluate(lowNode);
      diagnostic("Evaluating %p (%s) after %p (%s)\n",
         highNode, highNode->getOpCode().getName(),
         lowNode,  lowNode->getOpCode().getName());
      cg->setRequiresCarry(true);
      cg->setComputesCarry(true);
      cg->evaluate(highNode);
      cg->decReferenceCount(lowNode);
      cg->decReferenceCount(highNode);
      }
   cg->setRequiresCarry(requiresCarryOnEntry);
   return node->getRegister();
   }

TR::Register *OMR::X86::TreeEvaluator::integerAddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   const TR::ILOpCodes  opCode              = node->getOpCodeValue();
   bool                 nodeIs64Bit         = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Instruction     *instr               = NULL;
   TR::MemoryReference *tempMR              = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   bool                 isWithCarry         = (opCode == TR::luaddh);
   TR::Compilation     *comp                = cg->comp();

   bool computesCarry = cg->computesCarry();
   cg->setComputesCarry(false);
   if (node->isDualCyclic() && !computesCarry)
      {
      // first visit for this node, recurse once
      return TR::TreeEvaluator::integerDualAddOrSubEvaluator(node, cg);
      }
   computesCarry = computesCarry || isWithCarry || node->nodeRequiresConditionCodes();

   if (NEED_CC(node) || (opCode == TR::luaddc) || (opCode == TR::iuaddc))
      {
      TR_ASSERT((nodeIs64Bit  && opCode == TR::ladd || opCode == TR::luaddc) ||
                (!nodeIs64Bit && opCode == TR::iadd || opCode == TR::iuaddc),
                "CC computation not supported for this node %p with opcode %s\n", node, cg->comp()->getDebug()->getName(opCode));

      // we need eflags from integerAddAnalyser for the CC sequence
      const bool isWithCarry = (opCode == TR::luaddc) || (opCode == TR::iuaddc);
      TR_X86BinaryCommutativeAnalyser(cg).integerAddAnalyser(node, AddRegReg(nodeIs64Bit, isWithCarry),
                                                                   AddRegMem(nodeIs64Bit, isWithCarry),
                                                                   true, /* produce eflags */
                                                                   isWithCarry ? node->getChild(2) : 0);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();
   if (!computesCarry && TR::TreeEvaluator::analyseAddForLEA(node, cg))
      {
      targetRegister = node->getRegister();
      }
   else
      {
      if (isMemOp)
         {
         // Make sure the original value is evaluated before the update if it
         // is going to be used again.
         //
         if (firstChild->getReferenceCount() > 1)
            {
            TR::Register *reg = cg->evaluate(firstChild);
            tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
            oursIsTheOnlyMemRef = false;
            }
         else
            {
            tempMR = generateX86MemoryReference(firstChild, cg);
            }
         }

      intptr_t constValue;
      if (!targetRegister                                                 &&
          secondChild->getOpCode().isLoadConst()                          &&
          (secondChild->getRegister() == NULL)                            &&
          ((TR::TreeEvaluator::constNodeValueIs32BitSigned(secondChild, &constValue, cg)) ||
           (comp->useCompressedPointers() &&
           (constValue == TR::Compiler->vm.heapBaseAddress()) &&
            firstChild->getReferenceCount() > 1 && !isMemOp))             &&
            performTransformation(comp, "O^O analyseAddForLEA: checking that second node is a memory reference %x\n", constValue))
         {
         if (!isMemOp)
            targetRegister = cg->evaluate(firstChild);

         bool internalPointerMismatch = false;

         if (targetRegister &&
             node->getOpCode().isArrayRef() &&
             node->isInternalPointer())
            {
            if (targetRegister->containsInternalPointer())
               {
               if (node->getPinningArrayPointer() !=  targetRegister->getPinningArrayPointer())
                  {
                  internalPointerMismatch = true;
                  }
               }
            }

         // We cannot re-use target register if this is a TR::aiadd and it
         // has reference count > 1; this is because the TR::aiadd should NOT
         // be marked as a collected reference whereas the first child (base)
         // may be marked as a collected reference. Thus re-using the same register
         // would cause an inconsistency.
         //
         if (targetRegister &&
             (firstChild->getReferenceCount() > 1 ||
              (node->getOpCode().isArrayRef() &&
               (internalPointerMismatch || targetRegister->containsCollectedReference()))))
            {
            TR::Register *firstOperandReg = targetRegister;

            // For a non-internal pointer TR::aiadd created when merging news (for example)
            // commoning is permissible across a GC point; however the register
            // needs to be marked as a collected reference for GC to behave correctly;
            // note that support for internal pointer TR::aiadd (e.g. array access) is not present
            // still
            //
            if (targetRegister->containsCollectedReference() &&
                (node->getOpCode().isArrayRef()) &&
               !node->isInternalPointer())
               {
               targetRegister = cg->allocateCollectedReferenceRegister();
               }
            else
               {
               targetRegister = cg->allocateRegister();
               }
            // comp()->useCompressedPointers
            // ladd
            //    ==>iu2l
            // the firstChild is commoned, so the add & iu2l will get different registers
            //
            TR::Register *tempReg = firstOperandReg;
            if (TR::TreeEvaluator::genNullTestSequence(node, firstOperandReg, targetRegister, cg))
               tempReg = targetRegister;

            if (computesCarry)
               {
               generateRegRegInstruction(MOVRegReg(nodeIs64Bit), node, targetRegister, tempReg, cg);
               generateRegImmInstruction(AddRegImm4(nodeIs64Bit, isWithCarry), node, targetRegister, constValue, cg);
               }
            else
               {
               tempMR = generateX86MemoryReference(tempReg, constValue, cg);
               generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, tempMR, cg);
               }
            }
         else
            {
            if (constValue >= -128 && constValue <= 127)
               {
               if (computesCarry)
                  {
                  if (isMemOp)
                     instr = generateMemImmInstruction(AddMemImms(nodeIs64Bit, isWithCarry), node, tempMR, constValue, cg);
                  else
                     instr = generateRegImmInstruction(AddRegImms(nodeIs64Bit, isWithCarry), node, targetRegister, constValue, cg);
                  }
               else if (constValue == 1)
                  {
                  if (isMemOp)
                     instr = generateMemInstruction(INCMem(nodeIs64Bit), node, tempMR, cg);
                  else
                     instr = generateRegImmInstruction(ADDRegImms(nodeIs64Bit), node, targetRegister, 1, cg);
                  }
               else if (constValue == -1)
                  {
                  if (isMemOp)
                     instr = generateMemInstruction(DECMem(nodeIs64Bit), node, tempMR, cg);
                  else
                     instr = generateRegImmInstruction(SUBRegImms(nodeIs64Bit), node, targetRegister, 1, cg);
                  }
               else
                  {
                  if (isMemOp)
                     instr = generateMemImmInstruction(ADDMemImms(nodeIs64Bit), node, tempMR, constValue, cg);
                  else
                     instr = generateRegImmInstruction(ADDRegImms(nodeIs64Bit), node, targetRegister, constValue, cg);
                  }
               }
            else if ((constValue == 128) && !computesCarry)
               {
               if (isMemOp)
                  instr = generateMemImmInstruction(SUBMemImms(nodeIs64Bit), node, tempMR, (uint32_t)-128, cg);
               else
                  instr = generateRegImmInstruction(SUBRegImms(nodeIs64Bit), node, targetRegister, (uint32_t)-128, cg);
               }
            else
               {
               // comp()->useCompressedPointers
               // ladd
               //    iu2l
               // the firstChild is not commoned
               //
               TR::TreeEvaluator::genNullTestSequence(node, targetRegister, targetRegister, cg);

               if (isMemOp)
                  instr = generateMemImmInstruction(AddMemImm4(nodeIs64Bit, isWithCarry), node, tempMR, constValue, cg);
               else
                  instr = generateRegImmInstruction(AddRegImm4(nodeIs64Bit, isWithCarry), node, targetRegister, constValue, cg);
               }
            if (debug("traceMemOp"))
               diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by const %d", node, constValue);
            }
         }
      else if (isMemOp)
         {
         instr = generateMemRegInstruction(AddMemReg(nodeIs64Bit, isWithCarry), node, tempMR, cg->evaluate(secondChild), cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by var", node);
         }

      if (isMemOp)
         {
         if (oursIsTheOnlyMemRef)
            tempMR->decNodeReferenceCounts(cg);
         else
            tempMR->stopUsingRegisters(cg);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         cg->setImplicitExceptionPoint(instr);
         }
      else if (targetRegister)
         {
         node->setRegister(targetRegister);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      else
         {
         TR_X86BinaryCommutativeAnalyser  temp(cg);
         temp.integerAddAnalyser(node, AddRegReg(nodeIs64Bit, isWithCarry), AddRegMem(nodeIs64Bit, isWithCarry), computesCarry, 0);
         targetRegister = node->getRegister();
         }
      }


   if (targetRegister &&
       node->getOpCode().isArrayRef() &&
       node->isInternalPointer())
      {
      if (node->getPinningArrayPointer())
         {
         targetRegister->setContainsInternalPointer();
         targetRegister->setPinningArrayPointer(node->getPinningArrayPointer());
         }
      else
         {
         TR::Node *firstChild = node->getFirstChild();
         if ((firstChild->getOpCodeValue() == TR::aload) &&
             firstChild->getSymbolReference()->getSymbol()->isAuto() &&
             firstChild->getSymbolReference()->getSymbol()->isPinningArrayPointer())
            {
            targetRegister->setContainsInternalPointer();

            if (!firstChild->getSymbolReference()->getSymbol()->isInternalPointer())
               {
               targetRegister->setPinningArrayPointer(firstChild->getSymbolReference()->getSymbol()->castToAutoSymbol());
               }
            else
               targetRegister->setPinningArrayPointer(firstChild->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }
         else if (firstChild->getRegister() &&
                  firstChild->getRegister()->containsInternalPointer())
            {
            targetRegister->setContainsInternalPointer();
            targetRegister->setPinningArrayPointer(firstChild->getRegister()->getPinningArrayPointer());
            }
         }
      }

   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::baddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister       = NULL;
   TR::Node            *firstChild           = node->getFirstChild();
   TR::Node            *secondChild          = node->getSecondChild();
   TR::Instruction     *instr                = NULL;
   bool                 countsAreDecremented = false;
   TR::MemoryReference *tempMR               = NULL;
   bool                 oursIsTheOnlyMemRef  = true;
   TR::Compilation     *comp                 = cg->comp();

   // This comment has been tested on X86_32 and X86_64 Linux only, and has not been tested
   // on any Windows operating systems.
   // TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 baddEvaluator not yet supported");

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //

   if (NEED_CC(node))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::badd,
                "CC computation not supported for this node %p with opcode %s\n", node, cg->comp()->getDebug()->getName(node->getOpCode()));

      // we need eflags from integerAddAnalyser for the CC sequence
      TR_X86BinaryCommutativeAnalyser(cg).integerAddAnalyser(node, ADD1RegReg,
                                                                   ADD1RegMem,
                                                                   true/* produce eflags */);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::bconst &&
       testRegister == NULL && performTransformation(comp, "O^O BaddEvaluator: checking that the store has not happened yet. Target register: %x\n", testRegister))
      {
      int32_t value  = secondChild->getByte();
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);
      if (targetRegister && firstChild->getReferenceCount() > 1)
         {
         tempMR = generateX86MemoryReference(targetRegister, value, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
         }
      else
         {
         if (value == 1)
            {
            if (isMemOp)
               instr = generateMemInstruction(INC1Mem, node, tempMR, cg);
            else
               instr = generateRegInstruction(INC1Reg, node, targetRegister, cg);
            }
         else if (value == -1)
            {
            if (isMemOp)
               instr = generateMemInstruction(DEC1Mem, node, tempMR, cg);
            else
               instr = generateRegInstruction(DEC1Reg, node, targetRegister, cg);
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(ADD1MemImm1, node, tempMR, value, cg);
            else
               instr = generateRegImmInstruction(ADD1RegImm1, node, targetRegister, value, cg);
            }
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by const %d", node, value);
         }
      }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(ADD1MemReg, node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.integerAddAnalyser(node, ADD1RegReg, ADD1RegMem);
      targetRegister = node->getRegister();
      countsAreDecremented = true;
      }

   if (!countsAreDecremented)
      {
      if (isMemOp)
         {
         if (oursIsTheOnlyMemRef)
            tempMR->decNodeReferenceCounts(cg);
         else
            tempMR->stopUsingRegisters(cg);
         cg->setImplicitExceptionPoint(instr);
         }
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   if (cg->enableRegisterInterferences() && targetRegister)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::saddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Instruction     *instr               = NULL;
   TR::MemoryReference *tempMR              = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   //This comment has been tested on X86_32 and X86_64 Linux only, and has not been tested
   //on any Windows operating systems.
   //TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 baddEvaluator not yet supported");

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   if (NEED_CC(node))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::sadd,
                "CC computation not supported for this node %p with opcode %s\n", node, cg->comp()->getDebug()->getName(node->getOpCode()));

      // we need eflags from integerAddAnalyser for the CC sequence
      TR_X86BinaryCommutativeAnalyser(cg).integerAddAnalyser(node, ADD2RegReg,
                                                                   ADD2RegMem,
                                                                   true/* produce eflags */);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::sconst &&
       testRegister == NULL         &&
       performTransformation(comp, "O^O SaddEvaluator: checking that the store has not happened yet. Target register: %x\n", testRegister))
      {
      int32_t value  = secondChild->getShortInt();
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);
      if (targetRegister && firstChild->getReferenceCount() > 1)
         {
         tempMR = generateX86MemoryReference(targetRegister, value, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
         }
      else
         {
         if (value >= -128 && value <= 127)
            {
            if (value == 1)
               {
               if (isMemOp)
                  instr = generateMemInstruction(INC2Mem, node, tempMR, cg);
               else
                  instr = generateRegInstruction(INC4Reg, node, targetRegister, cg);
               }
            else if (value == -1)
               {
               if (isMemOp)
                  instr = generateMemInstruction(DEC2Mem, node, tempMR, cg);
               else
                  instr = generateRegInstruction(DEC4Reg, node, targetRegister, cg);
               }
            else
               {
               if (isMemOp)
                  instr = generateMemImmInstruction(ADD2MemImms, node, tempMR, value, cg);
               else
                  instr = generateRegImmInstruction(ADD4RegImms, node, targetRegister, value, cg);
               }
            }
         else if (value == 128)
            {
            if (isMemOp)
               instr = generateMemImmInstruction(SUB2MemImms, node, tempMR, (uint32_t)-128, cg);
            else
               instr = generateRegImmInstruction(SUB4RegImms, node, targetRegister, (uint32_t)-128, cg);
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(ADD2MemImm2, node, tempMR, value, cg);
            else
               instr = generateRegImmInstruction(ADD2RegImm2, node, targetRegister, value, cg);
            }
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by const %d", node, value);
         }
      }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(ADD2MemReg, node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.integerAddAnalyser(node, ADD4RegReg, ADD2RegMem);
      return node->getRegister();
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::caddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Instruction     *instr               = NULL;
   TR::MemoryReference *tempMR              = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 baddEvaluator not yet supported");

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::sconst &&
      testRegister == NULL         &&
      performTransformation(comp, "O^O CaddEvaluator: checking that the store has not happened yet. Target register:  %x\n", testRegister))
   {
   int32_t value  = secondChild->getConst<uint16_t>();
   if (!isMemOp)
      targetRegister = cg->evaluate(firstChild);
   if (targetRegister && firstChild->getReferenceCount() > 1)
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(targetRegister, value, cg);
      targetRegister = cg->allocateRegister();
      generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
      }
   else
      {
      if (value >= 0 && value <= 127)
         {
         if (value == 1)
            {
            if (isMemOp)
               instr = generateMemInstruction(INC2Mem, node, tempMR, cg);
            else
               instr = generateRegInstruction(INC4Reg, node, targetRegister, cg);
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(ADD2MemImms, node, tempMR, value, cg);
            else
               instr = generateRegImmInstruction(ADD4RegImms, node, targetRegister, value, cg);
            }
         }
      else
         {
         if (isMemOp)
            instr = generateMemImmInstruction(ADD2MemImm2, node, tempMR, value, cg);
         else
            instr = generateRegImmInstruction(ADD2RegImm2, node, targetRegister, value, cg);
         }
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by const %d", node, value);
      }
   }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(ADD2MemReg, node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] inc by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.integerAddAnalyser(node, ADD4RegReg, ADD2RegMem);
      return node->getRegister();
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::integerSubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   const TR::ILOpCodes  opCode              = node->getOpCodeValue();
   bool                 nodeIs64Bit         = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Register        *targetRegister      = NULL;
   TR::Instruction     *instr               = NULL;
   TR::MemoryReference *tempMR              = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   bool                 isWithBorrow        = (node->getOpCodeValue() == TR::lusubh);
   TR::Compilation     *comp                = cg->comp();

   bool computesCarry = cg->computesCarry();
   cg->setComputesCarry(false);
   if (node->isDualCyclic() && !computesCarry)
      {
      // first visit for this node, recurse once
      return TR::TreeEvaluator::integerDualAddOrSubEvaluator(node, cg);
      }
   computesCarry = computesCarry || isWithBorrow || node->nodeRequiresConditionCodes();

   if (NEED_CC(node) || (node->getOpCodeValue() == TR::lusubb) || (node->getOpCodeValue() == TR::iusubb))
      {
      TR_ASSERT((nodeIs64Bit  && (opCode == TR::lsub) || opCode == TR::lusubb) ||
                (!nodeIs64Bit && opCode == TR::isub || opCode == TR::iusubb),
                "CC computation not supported for this node %p with opcode %s\n", node, cg->comp()->getDebug()->getName(opCode));

      const bool isWithBorrow = (opCode == TR::lusubb) || (opCode == TR::iusubb);
      TR_X86SubtractAnalyser(cg).integerSubtractAnalyser(node, SubRegReg(nodeIs64Bit, isWithBorrow),
                                                               SubRegMem(nodeIs64Bit, isWithBorrow),
                                                               MOVRegReg(nodeIs64Bit),
                                                               true,
                                                               isWithBorrow ? node->getChild(2) : 0);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // We can generate a direct memory operation. In this case there is no
      // target register generated - return NULL to the caller (which should be
      // a store) to indicate that the store has already been done.
      //
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   intptr_t constValue;
   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       testRegister == NULL     &&
       TR::TreeEvaluator::constNodeValueIs32BitSigned(secondChild, &constValue, cg) &&
       performTransformation(comp, "O^O IntegerSubEvaluator: register is not NULL, or second operand is not a 32 bit constant. Register value: %x\n", testRegister))
      {
      if (!computesCarry && TR::TreeEvaluator::analyseSubForLEA(node, cg))
         {
         return node->getRegister();
         }
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);
      if (targetRegister && !computesCarry && firstChild->getReferenceCount() > 1)
         {
         tempMR = generateX86MemoryReference(targetRegister, -constValue, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, tempMR, cg);
         }
      else
         {
         if (constValue >= -128 && constValue <= 127)
            {
            if (computesCarry)
               {
               if (isMemOp)
                  instr = generateMemImmInstruction(SubMemImms(nodeIs64Bit, isWithBorrow), node, tempMR, constValue, cg);
               else
                  instr = generateRegImmInstruction(SubRegImms(nodeIs64Bit, isWithBorrow), node, targetRegister, constValue, cg);
               }
            else if (constValue == 1)
               {
               if (isMemOp)
                  instr = generateMemInstruction(DECMem(nodeIs64Bit), node, tempMR, cg);
               else
                  instr = generateRegImmInstruction(SUBRegImms(nodeIs64Bit), node, targetRegister, 1, cg);
               }
            else if (constValue == -1)
               {
               if (isMemOp)
                  instr = generateMemInstruction(INCMem(nodeIs64Bit), node, tempMR, cg);
               else
                  instr = generateRegImmInstruction(ADDRegImms(nodeIs64Bit), node, targetRegister, 1, cg);
               }
            else
               {
               if (isMemOp)
                  instr = generateMemImmInstruction(SUBMemImms(nodeIs64Bit), node, tempMR, constValue, cg);
               else
                  instr = generateRegImmInstruction(SUBRegImms(nodeIs64Bit), node, targetRegister, constValue, cg);
               }
            }
         else if ((constValue == 128) && !computesCarry)
            {
            if (isMemOp)
               instr = generateMemImmInstruction(ADDMemImms(nodeIs64Bit), node, tempMR, (uint32_t)-128, cg);
            else
               instr = generateRegImmInstruction(ADDRegImms(nodeIs64Bit), node, targetRegister, (uint32_t)-128, cg);
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(SubMemImm4(nodeIs64Bit, isWithBorrow), node, tempMR, constValue, cg);
            else
               instr = generateRegImmInstruction(SubRegImm4(nodeIs64Bit, isWithBorrow), node, targetRegister, constValue, cg);
            }

         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by const %d", node, constValue);
         }
      }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(SubMemReg(nodeIs64Bit, isWithBorrow), node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by var", node);
      }
   else
      {
      TR_X86SubtractAnalyser  temp(cg);
      temp.integerSubtractAnalyser(node,
         SubRegReg(nodeIs64Bit, isWithBorrow), SubRegMem(nodeIs64Bit, isWithBorrow), MOVRegReg(nodeIs64Bit), computesCarry, 0);

      return node->getRegister();
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::bsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister       = NULL;
   TR::Node            *firstChild           = node->getFirstChild();
   TR::Node            *secondChild          = node->getSecondChild();
   TR::Instruction     *instr                = NULL;
   bool                 countsAreDecremented = false;
   TR::MemoryReference *tempMR               = NULL;
   bool                 oursIsTheOnlyMemRef  = true;
   TR::Compilation     *comp                 = cg->comp();

   if (NEED_CC(node))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::bsub,
                "CC computation not supported for this node %p with opcode %s\n", node, cg->comp()->getDebug()->getName(node->getOpCode()));

      // we need eflags from integerAddAnalyser for the CC sequence
      TR_X86SubtractAnalyser(cg).integerSubtractAnalyser(node, SUB1RegReg,
                                                               SUB1RegMem,
                                                               MOV1RegReg,
                                                               true/* produce eflags */);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::bconst &&
       testRegister == NULL                       &&
       performTransformation(comp, "O^O BSUBEvaluator: checking that the store has not happened yet. Target register:  %x\n", testRegister))
      {
      int32_t value = secondChild->getByte();
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);
      if (targetRegister && firstChild->getReferenceCount() > 1)
         {
         tempMR = generateX86MemoryReference(targetRegister, -value, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
         }
      else
         {
         if (value == 1)
            {
            if (isMemOp)
               instr = generateMemInstruction(DEC1Mem, node, tempMR, cg);
            else
               instr = generateRegInstruction(DEC1Reg, node, targetRegister, cg);
            }
         else if (value == -1)
            {
            if (isMemOp)
               instr = generateMemInstruction(INC1Mem, node, tempMR, cg);
            else
               instr = generateRegInstruction(INC1Reg, node, targetRegister, cg);
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(SUB1MemImm1, node, tempMR, value, cg);
            else
               instr = generateRegImmInstruction(SUB1RegImm1, node, targetRegister, value, cg);
            }
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by const %d", node, value);
         }
      }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(SUB1MemReg, node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by var", node);
      }
   else
      {
      TR_X86SubtractAnalyser  temp(cg);
      temp.integerSubtractAnalyser(node, SUB1RegReg, SUB1RegMem, MOV1RegReg);
      targetRegister = node->getRegister();
      countsAreDecremented = true;
      }

   if (!countsAreDecremented)
      {
      if (isMemOp)
         {
         if (oursIsTheOnlyMemRef)
            tempMR->decNodeReferenceCounts(cg);
         else
            tempMR->stopUsingRegisters(cg);
         cg->setImplicitExceptionPoint(instr);
         }
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   if (cg->enableRegisterInterferences() && targetRegister)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::ssubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Instruction     *instr               = NULL;
   TR::MemoryReference *tempMR              = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   if (NEED_CC(node))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::ssub,
                "CC computation not supported for this node %p with opcode %s\n", node, cg->comp()->getDebug()->getName(node->getOpCode()));

      // we need eflags from integerAddAnalyser for the CC sequence
      TR_X86SubtractAnalyser(cg).integerSubtractAnalyser(node, SUB2RegReg,
                                                               SUB2RegMem,
                                                               MOV4RegReg,
                                                               true/* produce eflags */);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::sconst &&
       testRegister  == NULL         &&
       performTransformation(comp, "O^O SSUBEvaluator: checking that the store has not happened yet. Target register:  %x\n", testRegister))
      {
      int32_t value = secondChild->getShortInt();
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);
      if (firstChild->getReferenceCount() > 1)
         {
         tempMR = generateX86MemoryReference(targetRegister, -value, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
         }
      else
         {
         if (value >= -128 && value <= 127)
            {
            if (value == 1)
               {
               if (isMemOp)
                  instr = generateMemInstruction(DEC2Mem, node, tempMR, cg);
               else
                  instr = generateRegInstruction(DEC4Reg, node, targetRegister, cg);
               }
            else if (value == -1)
               {
               if (isMemOp)
                  instr = generateMemInstruction(INC2Mem, node, tempMR, cg);
               else
                  instr = generateRegInstruction(INC4Reg, node, targetRegister, cg);
               }
            else
               {
               if (isMemOp)
                  instr = generateMemImmInstruction(SUB2MemImms, node, tempMR, value, cg);
               else
                  instr = generateRegImmInstruction(SUB4RegImms, node, targetRegister, value, cg);
               }
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(SUB2MemImm2, node, tempMR, value, cg);
            else
               instr = generateRegImmInstruction(SUB2RegImm2, node, targetRegister, value, cg);
            }
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by const %d", node, value);
         }
      }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(SUB2MemReg, node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by var", node);
      }
   else
      {
      TR_X86SubtractAnalyser  temp(cg);
      temp.integerSubtractAnalyser(node, SUB4RegReg, SUB2RegMem, MOV4RegReg); // Use MOV4 to avoid unnecessary size prefix
      return node->getRegister();
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::csubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Instruction     *instr               = NULL;
   TR::MemoryReference *tempMR              = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   if (NEED_CC(node))
      {
      TR_ASSERT(node->getOpCodeValue() == TR::csub,
                "CC computation not supported for this node %p with opcode %s\n", node, cg->comp()->getDebug()->getName(node->getOpCode()));

      // we need eflags from integerAddAnalyser for the CC sequence
      TR_X86SubtractAnalyser(cg).integerSubtractAnalyser(node, SUB2RegReg,
                                                               SUB2RegMem,
                                                               MOV4RegReg,
                                                               true/* produce eflags */);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCodeValue() == TR::sconst &&
       testRegister == NULL         &&
       performTransformation(comp, "O^O CSubEvaluator: checking that the store has not happened yet. Target register: %x\n", testRegister))
      {
      int32_t value  = secondChild->getConst<uint16_t>();
      if (!isMemOp)
         targetRegister = cg->evaluate(firstChild);
      if (targetRegister && firstChild->getReferenceCount() > 1)
         {
         tempMR = generateX86MemoryReference(targetRegister, value, cg);
         targetRegister = cg->allocateRegister();
         generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
         }
      else
         {
         if (value >= 0 && value <= 127)
            {
            if (value == 1)
               {
               if (isMemOp)
                  instr = generateMemInstruction(INC2Mem, node, tempMR, cg);
               else
                  instr = generateRegInstruction(INC4Reg, node, targetRegister, cg);
               }
            else
               {
               if (isMemOp)
                  instr = generateMemImmInstruction(ADD2MemImms, node, tempMR, value, cg);
               else
                  instr = generateRegImmInstruction(ADD4RegImms, node, targetRegister, value, cg);
               }
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(ADD2MemImm2, node, tempMR, value, cg);
            else
               instr = generateRegImmInstruction(ADD2RegImm2, node, targetRegister, value, cg);
            }
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by const %d", node, value);
         }
      }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(SUB2MemReg, node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] dec by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.integerAddAnalyser(node, ADD4RegReg, ADD2RegMem);
      return node->getRegister();
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::integerDualMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT((node->getOpCodeValue() == TR::lumulh) || (node->getOpCodeValue() == TR::lmul), "Unexpected operator. Expected lumulh or lmul.");
   if (node->isDualCyclic() && (node->getChild(2)->getReferenceCount() == 1))
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

   bool needsHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   if (node->isDualCyclic() || needsHighMulOnly)
      {
      TR::Node *lmulNode;
      TR::Node *lumulhNode;
      if (!needsHighMulOnly)
         {
         diagnostic("Found lmul/lumulh for node = %p\n", node);
         lmulNode = (node->getOpCodeValue() == TR::lmul) ? node : node->getChild(2);
         lumulhNode = lmulNode->getChild(2);
         TR_ASSERT((lumulhNode->getReferenceCount() > 1) && (lmulNode->getReferenceCount() > 1), "Expected both lumulh and lmul have external references.");

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

      // Both parts or high part of multiplication required
      // RDX:RAX <-- RAX * r/m doubleword (isn't this RDX?)
      // result <-- RAX, but by-product is RDX will be required at later evaluation of lumulh
      // (or result/by-product may be reversed, if lumulh encountered first, and RAX may not be required if lumulh is on its own)
      TR::RegisterDependencyConditions  *multDependencies = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
      TR::Register * lmulTargetRegister;
      TR::Register * lumulhTargetRegister;
      lmulTargetRegister  = cg->longClobberEvaluate(lumulhNode->getFirstChild());
      lumulhTargetRegister = cg->longClobberEvaluate(lumulhNode->getSecondChild());

      multDependencies->addPreCondition(lmulTargetRegister, TR::RealRegister::eax, cg);
      multDependencies->addPreCondition(lumulhTargetRegister, TR::RealRegister::edx, cg);
      multDependencies->addPostCondition(lmulTargetRegister, TR::RealRegister::eax, cg);
      multDependencies->addPostCondition(lumulhTargetRegister, TR::RealRegister::edx, cg);

      // TODO: sometimes acc-mem may be more efficient
      generateRegRegInstruction(MUL8AccReg, node, lmulTargetRegister, lumulhTargetRegister, multDependencies, cg);

      if (needsHighMulOnly)
         cg->stopUsingRegister(lmulTargetRegister);
      else
         lmulNode->setRegister(lmulTargetRegister);

      lumulhNode->setRegister(lumulhTargetRegister);
      cg->decReferenceCount(lumulhNode->getFirstChild());
      cg->decReferenceCount(lumulhNode->getSecondChild());
      return node->getRegister();
      }

   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::integerMulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister  = 0;
   TR::Node     *firstChild      = node->getFirstChild();
   TR::Node     *secondChild     = node->getSecondChild();
   TR::DataType dataType         = secondChild->getDataType();
   bool         nodeIs64Bit      = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   bool firstChildRefCountIsZero = false;
   intptr_t     value;

   if (node->isDualCyclic())
      {
      return TR::TreeEvaluator::integerDualMulEvaluator(node, cg);
      }

   if (secondChild->getOpCode().isLoadConst() &&
      (value = TR::TreeEvaluator::integerConstNodeValue(secondChild, cg)))
      {
      if (value == 0)
         {
         if (firstChild->getReferenceCount() > 1)
            {
            // Respect the evaluation point of the child
            cg->evaluate(firstChild);
            }
         else
            {
            firstChildRefCountIsZero = true;
            cg->recursivelyDecReferenceCount(firstChild);
            }
         targetRegister = cg->allocateRegister();
         // note: this zeros the whole register even on AMD64
         generateRegRegInstruction(XOR4RegReg, node, targetRegister, targetRegister, cg);
         }
      else
         {
         bool canClobberSource;

         if (SHIFT_MAY_HAVE_ADDRESS_CHILD && firstChild->getDataType() == TR::Address)
            {
            // Can't re-use firstChild's register because it may be a collected
            // reference reference register, which is not suitable to contain
            // the result of the multiply.

            // Ensure we have a source register for the decomposer
            //
            cg->evaluate(firstChild);

            // Tell the decomposer not to clobber it
            //
            canClobberSource = false;
            }
         else
            {
            canClobberSource = (firstChild->getReferenceCount() == 1);
            }

         TR_X86IntegerMultiplyDecomposer *mulDecomposer =
            new (cg->trHeapMemory()) TR_X86IntegerMultiplyDecomposer(value,
                                                                     firstChild->getRegister(),
                                                                     node,
                                                                     cg,
                                                                     canClobberSource);
         int32_t dummy;
         targetRegister = mulDecomposer->decomposeIntegerMultiplier(dummy, 0);

         // decomposition failed
         // large constants must be loaded into a register first so these cases
         // are evaluated by the generic analyser call below
         if (targetRegister == 0 && IS_32BIT_SIGNED(value) && (dataType != TR::Int8)) // decomposition failed
            {
            TR_X86OpCodes opCode = BADIA32Op;

            if (firstChild->getReferenceCount() > 1 ||
                firstChild->getRegister() != 0)
               {
               if (value >= -128 && value <= 127)
                  {
                  opCode = IMulRegRegImms(node->getSize());
                  }
               else
                  {
                  opCode = IMulRegRegImm4(node->getSize());
                  }
               targetRegister = cg->allocateRegister();
               generateRegRegImmInstruction(opCode, node, targetRegister, cg->evaluate(firstChild), value, cg);
               }
            else
               {
               if (firstChild->getOpCode().isMemoryReference())
                  {
                  if (value >= -128 && value <= 127)
                     {
                     opCode = IMulRegMemImms(node->getSize());
                     }
                  else
                     {
                     opCode = IMulRegMemImm4(node->getSize());
                     }
                  TR::MemoryReference *tempMR = generateX86MemoryReference(firstChild, cg);
                  targetRegister = cg->allocateRegister();
                  generateRegMemImmInstruction(opCode, node, targetRegister, tempMR, value, cg);
                  tempMR->decNodeReferenceCounts(cg);
                  }
               else
                  {
                  if (value >= -128 && value <= 127)
                     {
                     opCode = IMulRegRegImms(node->getSize());
                     }
                  else
                     {
                     opCode = IMulRegRegImm4(node->getSize());
                     }
                  targetRegister = cg->evaluate(firstChild);
                  generateRegRegImmInstruction(opCode, node, targetRegister, targetRegister, value, cg);
                  }
               }
            }
         }
      }

   //Evaluate the byte multiplier
   if (!targetRegister && dataType == TR::Int8)
      {
      targetRegister = cg->intClobberEvaluate(secondChild);
      //Set register dependecy on EAX. AX = AL * r/m8
      TR::RegisterDependencyConditions  *multDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      multDependencies->addPreCondition(targetRegister, TR::RealRegister::eax, cg);
      multDependencies->addPostCondition(targetRegister, TR::RealRegister::eax, cg);

      if (firstChild->getReferenceCount() == 1 &&
          firstChild->getOpCode().isMemoryReference())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);
         generateRegMemInstruction(IMUL1AccMem, node, targetRegister, tempMR, multDependencies, cg);
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         TR::Register *tempRegister = cg->evaluate(firstChild);
         generateRegRegInstruction(IMUL1AccReg, node, targetRegister, tempRegister, multDependencies, cg);
         }
      }

   if (targetRegister)
      {
      node->setRegister(targetRegister);
      if (!firstChildRefCountIsZero)
         cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser temp(cg);
      temp.genericAnalyser(node, IMulRegReg(node->getSize()), IMulRegMem(node->getSize()), MovRegReg(node->getSize()));
      targetRegister = node->getRegister();
      }

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::integerMulhEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   const bool nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(node, cg);

   bool needsUnsignedHighMulOnly = (node->getOpCodeValue() == TR::lumulh) && !node->isDualCyclic();
   if (node->isDualCyclic() || needsUnsignedHighMulOnly)
      {
      return TR::TreeEvaluator::integerDualMulEvaluator(node, cg);
      }

   if (  secondChild->getOpCode().isLoadConst()
      && TR::TreeEvaluator::integerConstNodeValue(secondChild, cg) == 0)
      {
      // TODO: Why special-case this?  Simplifier should catch this.

      if (firstChild->getReferenceCount() > 1)
         cg->evaluate(firstChild); // Respect the evaluation point of the child -- TODO: this could be sub-optimal and is likely best handled elsewhere
      cg->recursivelyDecReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      TR::Register *targetRegister = cg->allocateRegister();
      generateRegRegInstruction(XOR4RegReg, node, targetRegister, targetRegister, cg);
      node->setRegister(targetRegister);
      return targetRegister;
      }

   // High word multiplication
   // EDX:EAX <-- EAX * r/m doubleword
   // result <-- EDX
   //
   TR::Register *targetRegister = TR::TreeEvaluator::intOrLongClobberEvaluate(secondChild, nodeIs64Bit, cg);
   TR::Register *eaxRegister    = TR::TreeEvaluator::intOrLongClobberEvaluate(firstChild,  nodeIs64Bit, cg);

   TR::RegisterDependencyConditions  *multDependencies = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
   multDependencies->addPreCondition (eaxRegister, TR::RealRegister::eax, cg);
   multDependencies->addPostCondition(eaxRegister, TR::RealRegister::eax, cg);
   multDependencies->addPreCondition (targetRegister, TR::RealRegister::edx, cg);
   multDependencies->addPostCondition(targetRegister, TR::RealRegister::edx, cg);

   TR_ASSERT(node->getOpCodeValue() == TR::imulh, "IMULAccReg only works for imulh");
   generateRegRegInstruction(IMULAccReg(nodeIs64Bit), node, eaxRegister, targetRegister, multDependencies, cg);

   cg->stopUsingRegister(eaxRegister);
   node->setRegister(targetRegister);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::signedIntegerDivOrRemAnalyser(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool           nodeIs64Bit  = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Node       *dividend     = node->getFirstChild();
   TR::ILOpCode   &rootOpCode   = node->getOpCode();
   intptr_t      dvalue       = TR::TreeEvaluator::integerConstNodeValue(node->getSecondChild(), cg);
   TR_ASSERT(dvalue != 0, "assertion failure");

   bool isMinInt = ((!nodeIs64Bit && dvalue == TR::getMinSigned<TR::Int32>()) ||
                    (nodeIs64Bit && dvalue == TR::getMinSigned<TR::Int64>()));

   TR::Register *dividendRegister = TR::TreeEvaluator::intOrLongClobberEvaluate(dividend, nodeIs64Bit, cg);
   TR::Register *edxRegister      = NULL;

   if (rootOpCode.isRem())
      {
      if (!isPowerOf2(dvalue))
         {
         // The dividend is required in the remainder calculation
         edxRegister = cg->allocateRegister();
         }
      }
   else
      {
      // Reuse the dividend register if performing only division
      if (!isPowerOf2(dvalue))
         {
         edxRegister = dividendRegister;
         }
      else
         {
         edxRegister  = cg->allocateRegister();
         }
      }

   if (isPowerOf2(dvalue))
      {
      bool isNegative = false;
      TR::Register *tempRegister = dividendRegister;

      if (dvalue < 0 && !isMinInt)
         {
         dvalue = -dvalue;
         isNegative = true;
         }

      if (rootOpCode.isRem())
         {
         // We only need the remainder.  We can get it with bitmask operations.

         TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
         deps->addPostCondition(tempRegister, TR::RealRegister::NoReg, cg);

         // Note: a recurring complication with this logic is that we can only
         // effectively use 31-bit masks, because the 32nd bit gets
         // sign-extended and hence applies to all of the upper 33 bits.
         // Because longs have 63 bits (in addition to the sign bit), that
         // means that there aways seems to be one odd case to consider.  31
         // cases can be handled with one 31-bit masking technique, and another
         // 31 can be handled with a second technique, leaving one straggler
         // that needs special treatment.
         //
         const uintptr_t SIGN32 = CONSTANT64(0x80000000);

         // Note: We could check node->isNegative and node->isNonNegative to
         // try to avoid this internal control flow.  However, in those cases,
         // the optimizer could easily transform the rem node into the
         // appropriate bitmask operation, and then we won't even get to this
         // code.  Hence, there's no point complicating this logic (which is
         // already complicated enough).
         //
         TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         startLabel->setStartInternalControlFlow();
         doneLabel->setEndInternalControlFlow();
         generateLabelInstruction(LABEL, node, startLabel, cg);

         // Compute remainder assuming divisor is positive, but also
         // preserve divisor's sign bit in the answer.
         //
         if (isMinInt)
            {
            // No masking necessary for this case; just test if the dividend
            // is negative.
            //
            generateRegRegInstruction(TESTRegReg(nodeIs64Bit), node, tempRegister, tempRegister, cg);
            generateLabelInstruction(JNS4, node, doneLabel, cg);
            }
         else if (nodeIs64Bit)
            {
            if (1 <= dvalue && dvalue <= 0x40000000)
               {
               // We only need to keep as many as 30 bits plus the sign bit.
               // Together, that's only 31 bits, so we can do this with 31-bit
               // masks if we first rotate the sign bit into the low 32 bits.
               //
               generateRegImmInstruction(ROL8RegImm1, node, tempRegister, 1, cg);
               generateRegImmInstruction(AND8RegImm4, node, tempRegister, (uint32_t)(2*dvalue-1), cg);
               generateRegImmInstruction(ROR8RegImm1, node, tempRegister, 1, cg);
               }
            else if (dvalue == CONSTANT64(0x80000000))
               {
               // Keep only the low 31 bits plus the sign bit
               //
               generateRegImmInstruction(ROL8RegImm1,   node, tempRegister, 1, cg);
               generateRegRegInstruction(MOVZXReg8Reg4, node, tempRegister, tempRegister, cg);
               generateRegImmInstruction(ROR8RegImm1,   node, tempRegister, 1, cg);
               }
            else
               {
               TR_ASSERT(dvalue > CONSTANT64(0x80000000), "dvalue should not be negative at this point; TR::getMinSigned<TR::Int64>() is handled elsewhere");

               // We only need to discard as many as 31 bits, so rotate the
               // portion to be discarded into the low 32 bits and mask it off.
               //
               generateRegImmInstruction(ROL8RegImm1, node, tempRegister, 32, cg);
               generateRegImmInstruction(AND8RegImm4, node, tempRegister, SIGN32 | (uint32_t)((int64_t)(dvalue-1) >> 32), cg);
               generateRegImmInstruction(ROR8RegImm1, node, tempRegister, 32, cg);
               }

            // If the value is non-negative, branch around the compensation
            // logic for negative values.
            //
            // (Each case above must make sure to copy the value's sign bit to
            // the carry flag so that this JAE will do the right thing.  One
            // way to do that is to finish with an ROR.)
            //
            generateLabelInstruction(JAE4, node, doneLabel, cg);
            }
         else
            {
            TR_ASSERT(!nodeIs64Bit, "assertion failure");
            TR_ASSERT(IS_32BIT_SIGNED(dvalue), "assertion failure");
            generateRegImmInstruction(AND4RegImm4, node, tempRegister, (uint32_t) SIGN32 + dvalue - 1, cg);
            generateLabelInstruction(JNS4, node, doneLabel, cg);
            }

         // If dividend was negative, sign-extend it.
         //
         if (!nodeIs64Bit || dvalue <= CONSTANT64(0x80000000))
            {
            generateRegInstruction(DECReg(nodeIs64Bit), node, tempRegister, cg);
            generateRegImmInstruction(ORRegImm4(nodeIs64Bit), node, tempRegister, (uint32_t) -dvalue, cg);
            generateRegInstruction(INCReg(nodeIs64Bit), node, tempRegister, cg);
            generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
            }
         else
            {
            TR_ASSERT(nodeIs64Bit, "assertion failure");
            generateRegInstruction(DEC8Reg, node, tempRegister, cg);
            if (dvalue == CONSTANT64(0x100000000))
               {
               // Can't set exactly 32 bits with one OR operation, so we need to be sneaky.
               //
               // Clear top 32 bits & flip bottom 32 bits
               generateRegImmInstruction(XOR4RegImms, node, tempRegister, (unsigned)-1, cg);
               // Flip all 64 bits
               generateRegImmInstruction(XOR8RegImms, node, tempRegister, (unsigned)-1, cg);
               // Now the top 32 bits are set, and bottom 32 bits have their original values
               }
            else
               {
               // Rotate so sign bit is at bit 31
               generateRegImmInstruction(ROR8RegImm1, node, tempRegister, 33, cg);
               // Set the appropriate bits, making sure the Imm4 has its sign bit clear to leave the top 33 bits alone
               generateRegImmInstruction(OR8RegImm4, node, tempRegister, (~SIGN32) & (uint32_t)((int64_t)(-dvalue) >> 33), cg);
               // Un-rotate
               generateRegImmInstruction(ROL8RegImm1, node, tempRegister, 33, cg);
               }
            generateRegInstruction(INC8Reg, node, tempRegister, cg);
            generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
            }

         return tempRegister;
         }

      //
      // Division by powers of 2.
      //
      if (!isMinInt)
         {
         TR::RegisterDependencyConditions  *cdqDependencies = 0;

         // If dividend is negative, we must add dvalue-1 to it in order to get the right result from the SAR.
         // We use CDQ (or CQO) to allow us to do this without introducing control flow.
         //
         if ((dividend->isNonNegative() == false) && (dvalue > 0))
            {
            TR::RegisterDependencyConditions  *cdqDependencies = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
            cdqDependencies->addPreCondition(tempRegister, TR::RealRegister::eax, cg);
            cdqDependencies->addPreCondition(edxRegister, TR::RealRegister::edx, cg);
            cdqDependencies->addPostCondition(tempRegister, TR::RealRegister::eax, cg);
            cdqDependencies->addPostCondition(edxRegister, TR::RealRegister::edx, cg);
            generateInstruction(CXXAcc(nodeIs64Bit), node, cdqDependencies, cg);

            if (dvalue == 2) // special case when working with 2
               {
               generateRegRegInstruction(SUBRegReg(nodeIs64Bit), node, tempRegister, edxRegister, cg);
               }
            else if (!nodeIs64Bit || (dvalue > 0 && dvalue <= CONSTANT64(0x80000000)))
               {
               int32_t mask = dvalue-1;
               TR_ASSERT(mask >= 0,
                  "AMD64: mask must have sign bit clear so that, when sign-extended, it clears the upper 32 bits of tempRegister");
               generateRegImmInstruction(ANDRegImm4(nodeIs64Bit), node, edxRegister, (uint32_t)dvalue-1, cg);
               generateRegRegInstruction(ADDRegReg(nodeIs64Bit), node, tempRegister, edxRegister, cg);
               }
            else
               {
               // dvalue-1 is too big for an Imm4, so shift off the bits instead.
               //
               int32_t shiftAmount = leadingZeroes(dvalue)+1;
               generateRegImmInstruction(SHL8RegImm1, node, edxRegister, shiftAmount, cg);
               generateRegImmInstruction(SHR8RegImm1, node, edxRegister, shiftAmount, cg);
               generateRegRegInstruction(ADDRegReg(nodeIs64Bit), node, tempRegister, edxRegister, cg);
               }
            }

         generateRegImmInstruction(SARRegImm1(nodeIs64Bit), node, tempRegister, trailingZeroes(dvalue), cdqDependencies, cg);

         if (isNegative)
            {
            generateRegInstruction(NEGReg(nodeIs64Bit), node, tempRegister, cdqDependencies, cg);
            }
         }
      else
         {
         // MIN_INT or MIN_LONG divisor
         //
         TR::Register *quotientRegister = cg->allocateRegister();
         generateRegRegInstruction(XORRegReg(nodeIs64Bit), node, quotientRegister, quotientRegister, cg);

         if (nodeIs64Bit)
            {
            generateRegImm64Instruction(MOV8RegImm64, node, edxRegister, dvalue, cg);
            generateRegRegInstruction(CMP8RegReg, node, edxRegister, tempRegister, cg);
            }
         else
            {
            generateRegImmInstruction(CMP4RegImm4, node, tempRegister, (int32_t)dvalue, cg);
            }

         generateRegInstruction(SETE1Reg, node, quotientRegister, cg);
         tempRegister = quotientRegister;
         }

      cg->stopUsingRegister(edxRegister);
      return tempRegister;
      }
   else
      {
      intptr_t     m, s;
      TR::Register  *eaxRegister = cg->allocateRegister();

      if (nodeIs64Bit)
         {
         // This prevents a compile error on IA32, where intptr_t can't be cast to int64_t.
         //
         int64_t tm, ts;
         cg->compute64BitMagicValues(dvalue, &tm, &ts);
         m = tm;
         s = ts;
         }
      else
         {
         // On AMD64, this has the desired effect of sign-extending the magic
         // numbers; on IA32, it doesn't do anything because intptr_t is the
         // same as int32_t.
         //
         int32_t tm, ts;
         cg->compute32BitMagicValues(dvalue, &tm, &ts);
         m = tm;
         s = ts;
         }

      TR::RegisterDependencyConditions  *multDependencies = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
      if (!rootOpCode.isRem() && !(dvalue > 0 && m < 0) && !(dvalue < 0 && m > 0))
         {
         multDependencies->addPreCondition(eaxRegister, TR::RealRegister::eax, cg);
         multDependencies->addPreCondition(dividendRegister, TR::RealRegister::edx, cg);
         multDependencies->addPostCondition(eaxRegister, TR::RealRegister::eax, cg);
         multDependencies->addPostCondition(dividendRegister, TR::RealRegister::edx, cg);
         }
      else
         {
         // if the register opt was done before, we need to change the
         // registers here.  For REM this wasn't done.  So, it should be fine.
         //
         if (!rootOpCode.isRem())
            {
            edxRegister = cg->allocateRegister();
            }
         multDependencies->addPreCondition(eaxRegister, TR::RealRegister::eax, cg);
         multDependencies->addPreCondition(edxRegister, TR::RealRegister::edx, cg);
         multDependencies->addPostCondition(eaxRegister, TR::RealRegister::eax, cg);
         multDependencies->addPostCondition(edxRegister, TR::RealRegister::edx, cg);
         }

      // Multiply by the magic reciprocal
      //
      if (!nodeIs64Bit || IS_32BIT_SIGNED(m))
         generateRegImmInstruction(MOVRegImm4(nodeIs64Bit), node, eaxRegister, m, cg);
      else
         generateRegMemInstruction(LEA8RegMem, node, eaxRegister, generateX86MemoryReference(m, cg), cg);
      generateRegRegInstruction(IMULAccReg(nodeIs64Bit), node, eaxRegister, dividendRegister, multDependencies, cg);

      cg->stopUsingRegister(eaxRegister);

      // Some special adjustment (?) when multiplier and divisor have opposite signs.
      //
      if ( (dvalue > 0) && (m < 0) )
         {
         generateRegRegInstruction(ADDRegReg(nodeIs64Bit), node, edxRegister, dividendRegister, cg);
         }
      else if ( (dvalue < 0) && (m > 0) )
         {
         generateRegRegInstruction(SUBRegReg(nodeIs64Bit), node, edxRegister, dividendRegister, cg);
         }

      // Do the magic shift
      //
      generateRegImmInstruction(SARRegImm1(nodeIs64Bit), node, edxRegister, s, cg);

      // If edx is negative, then it's off by 1, so adjust it.
      //
      // TODO: I think we can do this without the extra register:
      //   rol rdx, 1
      //   ror rdx, 1     ; Now the sign bit is in the carry flag
      //   adc rdx, 0     ; Increment iff sign bit was set
      //
      if (!(dividend->isNonNegative() && (dvalue > 0)))
         {
         eaxRegister = cg->allocateRegister();
         generateRegRegInstruction(MOVRegReg(nodeIs64Bit), node, eaxRegister, edxRegister, cg);
         generateRegImmInstruction(SHRRegImm1(nodeIs64Bit), node, eaxRegister, nodeIs64Bit ? 63: 31, cg);
         generateRegRegInstruction(ADDRegReg(nodeIs64Bit), node, edxRegister, eaxRegister, cg);

         cg->stopUsingRegister(eaxRegister);
         }

      // Calculate remainder from quotient.
      //
      if (rootOpCode.isRem())
         {
         if (nodeIs64Bit && !IS_32BIT_SIGNED(dvalue))
            {
            TR::Register *scratchReg = cg->allocateRegister();
            generateRegImm64Instruction(MOV8RegImm64, node, scratchReg, dvalue, cg);
            generateRegRegInstruction(IMULRegReg(nodeIs64Bit), node, edxRegister, scratchReg, cg);
            cg->stopUsingRegister(scratchReg);
            }
         else
            {
            TR_X86OpCodes opCode;
            if (dvalue >= -128 && dvalue <= 127)
               {
               opCode = IMULRegRegImms(nodeIs64Bit);
               }
            else
               {
               opCode = IMULRegRegImm4(nodeIs64Bit);
               }
            generateRegRegImmInstruction(opCode, node, edxRegister, edxRegister, dvalue, cg);
            }

         generateRegRegInstruction(SUBRegReg(nodeIs64Bit), node, dividendRegister, edxRegister, multDependencies, cg);
         cg->stopUsingRegister(edxRegister);
         }
      else if (dividendRegister != edxRegister)
         {
         cg->stopUsingRegister(dividendRegister);
         dividendRegister = edxRegister;
         }
      return dividendRegister;
      }
   }

TR::Register *OMR::X86::TreeEvaluator::integerDivOrRemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool      nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::ILOpCode &opCode  = node->getOpCode();
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   // Signed division by a constant can be done cheaper than using IDIV.
   if (!opCode.isUnsigned() && secondChild->getOpCode().isLoadConst() &&
       TR::TreeEvaluator::integerConstNodeValue(secondChild, cg) != 0)
      {
      TR::Register *reg = TR::TreeEvaluator::signedIntegerDivOrRemAnalyser(node, cg);
      node->setRegister(reg);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return reg;
      }
   else
      {
      bool needsExplicitOverflowCheck = !cg->enableImplicitDivideCheck() && !node->divisionCannotOverflow();

      TR::LabelSymbol *startLabel, *divisionLabel = NULL, *overflowSnippetLabel = NULL, *restartLabel = NULL;

      TR::Instruction *divideInstr = NULL;

      TR::Register *eaxRegister     = TR::TreeEvaluator::intOrLongClobberEvaluate(firstChild, nodeIs64Bit, cg);
      TR::Register *edxRegister     = cg->allocateRegister();
      TR::Register *divisorRegister = NULL;

      if (needsExplicitOverflowCheck)
         {
         // Get divisor in a register (1) to simplify the snippet, and (2)
         // because we need to use it twice anyway, so arguably it should be in
         // a register.
         //
         divisorRegister = cg->evaluate(secondChild);
         }
      else if (secondChild->getReferenceCount() == 1 &&
         !secondChild->getRegister()            &&
          secondChild->getOpCode().isMemoryReference())
         {
         TR_ASSERT(divisorRegister==NULL, "No source register because source is in memory");
         }
      else
         {
         divisorRegister = cg->evaluate(secondChild);
         }

      TR::RegisterDependencyConditions  *edxDeps = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      edxDeps->addPreCondition (edxRegister, TR::RealRegister::edx, cg);
      edxDeps->addPostCondition(edxRegister, TR::RealRegister::edx, cg);

      TR::RegisterDependencyConditions  *eaxEdxDeps = edxDeps->clone(cg, 1);
      eaxEdxDeps->addPreCondition (eaxRegister, TR::RealRegister::eax, cg);
      eaxEdxDeps->addPostCondition(eaxRegister, TR::RealRegister::eax, cg);

      TR::RegisterDependencyConditions  *allDeps = eaxEdxDeps->clone(cg, 1);
      allDeps->addPreCondition (divisorRegister, TR::RealRegister::NoReg, cg);
      allDeps->addPostCondition(divisorRegister, TR::RealRegister::NoReg, cg);

      if (opCode.isDiv())
         node->setRegister(eaxRegister);
      else
         node->setRegister(edxRegister);

      // Start the explicit overflow check logic if necessary
      //
      if (needsExplicitOverflowCheck)
         {
         startLabel           = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         divisionLabel        = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         overflowSnippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         restartLabel         = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         startLabel->setStartInternalControlFlow();
         restartLabel->setEndInternalControlFlow();

         // Only TR::getMinSigned<TR::Int32>() (or TR::getMinSigned<TR::Int64>()) will overflow when compared with 1
         //
         generateLabelInstruction (LABEL, node, startLabel, cg);
         generateRegImmInstruction(CMPRegImms(nodeIs64Bit), node, eaxRegister, 1, cg);
         generateLabelInstruction (JO4,   node, overflowSnippetLabel, cg);
         generateLabelInstruction (LABEL, node, divisionLabel, cg);
         }

      // Emit the appropriate division instruction(s)
      //
      if (!nodeIs64Bit && node->isUnsigned())
         {
         generateRegRegInstruction(XORRegReg(nodeIs64Bit), node, edxRegister, edxRegister, edxDeps, cg);

         if (divisorRegister)
            {
            divideInstr = generateRegRegInstruction(DIVAccReg(nodeIs64Bit), node, eaxRegister, divisorRegister, eaxEdxDeps, cg);
            }
         else
            {
            TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, cg);
            divideInstr = generateRegMemInstruction(DIVAccMem(nodeIs64Bit), node, eaxRegister, tempMR, eaxEdxDeps, cg);
            tempMR->decNodeReferenceCounts(cg);
            }
         }
      else
         {
         if (divisorRegister)
            {
            if (node->getFirstChild()->isNonNegative() ||
                node->getOpCode().isUnsigned())
               generateRegRegInstruction(XOR4RegReg, node, edxRegister, edxRegister, edxDeps, cg);
            else
               generateInstruction(CXXAcc(nodeIs64Bit), node, eaxEdxDeps, cg);
            if (node->getOpCode().isUnsigned() ||
                (node->getFirstChild()->isNonNegative() && node->getSecondChild()->isNonNegative()))
               divideInstr = generateRegRegInstruction(DIVAccReg(nodeIs64Bit), node, eaxRegister, divisorRegister, eaxEdxDeps, cg);
            else
               divideInstr = generateRegRegInstruction(IDIVAccReg(nodeIs64Bit), node, eaxRegister, divisorRegister, eaxEdxDeps, cg);
            }
         else
            {
            TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, cg);
            if (node->getFirstChild()->isNonNegative() ||
                node->getOpCode().isUnsigned())
               generateRegRegInstruction(XOR4RegReg, node, edxRegister, edxRegister, edxDeps, cg);
            else
               generateInstruction(CXXAcc(nodeIs64Bit), node, eaxEdxDeps, cg);
            if (node->getOpCode().isUnsigned() ||
                (node->getFirstChild()->isNonNegative() && node->getSecondChild()->isNonNegative()))
               divideInstr = generateRegMemInstruction(DIVAccMem(nodeIs64Bit), node, eaxRegister, tempMR, eaxEdxDeps, cg);
            else
               divideInstr = generateRegMemInstruction(IDIVAccMem(nodeIs64Bit), node, eaxRegister, tempMR, eaxEdxDeps, cg);
            tempMR->decNodeReferenceCounts(cg);
            }
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      cg->setImplicitExceptionPoint(divideInstr);

      // Finish the explicit overflow check logic if necessary
      //
      if (needsExplicitOverflowCheck)
         {
         generateLabelInstruction(LABEL, node, restartLabel, allDeps, cg);

         TR_ASSERT(divisorRegister != NULL, "Divide check snippet needs divisor in a register");
         cg->addSnippet(new (cg->trHeapMemory()) TR::X86DivideCheckSnippet(restartLabel,
                                                      overflowSnippetLabel,
                                                      divisionLabel,
                                                      node->getOpCode(),
                                                      node->getType(),
                                                      divideInstr->getX86RegInstruction()->getIA32RegRegInstruction(),
                                                      cg));
         }

      // Return the appropriate result register
      //
      if (opCode.isDiv())
         {
         cg->stopUsingRegister(edxRegister);
         return eaxRegister;
         }
      else
         {
         TR_ASSERT(opCode.isRem(), "assertion failure");
         cg->stopUsingRegister(eaxRegister);
         return edxRegister;
         }
      }
   TR_ASSERT(0, "Shouldn't get here");
   }

TR::X86RegInstruction  *OMR::X86::TreeEvaluator::generateRegisterShift(TR::Node *node, TR_X86OpCodes immShiftOpCode, TR_X86OpCodes regShiftOpCode,TR::CodeGenerator *cg)
   {
   bool                  nodeIs64Bit    = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Register         *targetRegister = NULL;
   TR::Node             *firstChild     = node->getFirstChild();
   TR::Node             *secondChild    = node->getSecondChild();
   TR::X86RegInstruction *instr          = NULL;
   TR::Compilation      *comp           = cg->comp();

   if (secondChild->getOpCode().isLoadConst())
      {
      intptr_t shiftAmount = TR::TreeEvaluator::integerConstNodeValue(secondChild, cg) & TR::TreeEvaluator::shiftMask(nodeIs64Bit);
      if (shiftAmount == 0)
         {
         targetRegister = cg->evaluate(firstChild);
         }
      else
         {
         targetRegister = cg->evaluate(firstChild);

         if (SHIFT_MAY_HAVE_ADDRESS_CHILD     &&
             node->getOpCode().isLeftShift()  &&
             (targetRegister->containsCollectedReference() ||
             node->containsCompressionSequence()))
            {
            TR::Register *origTargetRegister = targetRegister;
            targetRegister = cg->allocateRegister();
            instr = generateRegRegInstruction(MOVRegReg(nodeIs64Bit), node, targetRegister, origTargetRegister, cg);
            }
         else
            {
            targetRegister = TR::TreeEvaluator::intOrLongClobberEvaluate(firstChild, (SHIFT_MAY_HAVE_ADDRESS_CHILD) ? TR::TreeEvaluator::getNodeIs64Bit(firstChild, cg) : nodeIs64Bit, cg);
            }

         instr = generateRegImmInstruction(immShiftOpCode, node, targetRegister, shiftAmount, cg);
         }
      }
   else
      {
      TR::ILOpCodes op = secondChild->getOpCodeValue();
      TR::Register *shiftAmountReg = NULL;
      if (op == TR::su2i || op == TR::s2i || op == TR::b2i || op == TR::bu2i || op == TR::l2i)
         {
         if (secondChild->getReferenceCount() == 1 && secondChild->getRegister() == 0)
            {
            static char *reportShiftAmount = feGetEnv("TR_ReportShiftAmount");
            TR::Node     *grandChild        = secondChild->getFirstChild();
            if (secondChild->getOpCode().isLoadIndirect() &&
                grandChild->getReferenceCount() == 1 &&
                grandChild->getRegister() == 0)
               {
               TR::Node::recreate(grandChild, TR::bloadi);
               secondChild->decReferenceCount();
               secondChild = grandChild;
               if (reportShiftAmount)
                  diagnostic("shift: removed sign or zero extend to shift amount in memory in method %s\n",
                              comp->signature());
               }
            else if (secondChild->getOpCode().isLoadVarDirect() &&
                     grandChild->getReferenceCount() == 1 &&
                     grandChild->getRegister() == 0)
               {
               TR::Node::recreate(grandChild, TR::bload);
               secondChild->decReferenceCount();
               secondChild = grandChild;
               if (reportShiftAmount)
                  diagnostic("shift: removed sign or zero extend to shift amount in memory in method %s\n",
                              comp->signature());
               }
            else if (op != TR::l2i || comp->target().is64Bit()) // cannot use a reigster pair as the shift amount
               {
               secondChild->decReferenceCount();
               secondChild = grandChild;
               if (reportShiftAmount)
                  diagnostic("shift: removed sign or zero extend to shift amount in method %s\n",
                              comp->signature());
               }
            else if (op == TR::l2i && grandChild->getRegister())
               {
               secondChild->decReferenceCount();
               secondChild = grandChild;
               shiftAmountReg = grandChild->getRegister()->getLowOrder();
               }
            }
         }
      if(!shiftAmountReg)
         shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      targetRegister = TR::TreeEvaluator::intOrLongClobberEvaluate(firstChild, nodeIs64Bit, cg);

      if (SHIFT_MAY_HAVE_ADDRESS_CHILD     &&
          node->getOpCode().isLeftShift()  &&
          targetRegister->containsCollectedReference())
         {
         TR::Register *origTargetRegister = targetRegister;
         targetRegister = cg->allocateRegister();
         instr = generateRegRegInstruction(MOVRegReg(nodeIs64Bit), node, targetRegister, origTargetRegister, cg);
         }

      instr = generateRegRegInstruction(regShiftOpCode, node, targetRegister, shiftAmountReg, shiftDependencies, cg);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return instr;
   }

TR::X86MemInstruction  *OMR::X86::TreeEvaluator::generateMemoryShift(TR::Node *node, TR_X86OpCodes immShiftOpCode, TR_X86OpCodes regShiftOpCode, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->isDirectMemoryUpdate(), "assertion failure");

   bool                  nodeIs64Bit         = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Register         *targetRegister      = NULL;
   TR::Node             *firstChild          = node->getFirstChild();
   TR::Node             *secondChild         = node->getSecondChild();
   TR::X86MemInstruction *instr               = NULL;
   TR::MemoryReference  *memRef              = NULL;
   bool                  oursIsTheOnlyMemRef = true;
   TR::Compilation      *comp                = cg->comp();

   // Make sure the original value is evaluated before the update if it
   // is going to be used again.
   //
   if (firstChild->getReferenceCount() > 1)
      {
      TR::Register *reg = cg->evaluate(firstChild);
      memRef = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
      oursIsTheOnlyMemRef = false;
      }
   else
      {
      memRef = generateX86MemoryReference(firstChild, cg, false);
      }

   bool loadConstant = secondChild->getOpCode().isLoadConst();
   if (loadConstant && performTransformation(comp, "O^O GenerateMemoryShift: load is not constant %d\n", loadConstant))
      {
      intptr_t shiftAmount = TR::TreeEvaluator::integerConstNodeValue(secondChild, cg) & TR::TreeEvaluator::shiftMask(nodeIs64Bit);
      if (shiftAmount != 0)
         {
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shift by const %d", node, shiftAmount);
         instr = generateMemImmInstruction(immShiftOpCode, node, memRef, shiftAmount, cg);
         }
      }
   else
      {
      TR::ILOpCodes op = secondChild->getOpCodeValue();
      TR::Register *shiftAmountReg = NULL;
      if (op == TR::su2i || op == TR::s2i || op == TR::b2i || op == TR::bu2i || op == TR::l2i)
         {
         if (secondChild->getReferenceCount() == 1 && secondChild->getRegister() == 0)
            {
            static char * reportShiftAmount = feGetEnv("TR_ReportShiftAount");
            TR::Node* grandChild = secondChild->getFirstChild();

            if (grandChild->getOpCode().isLoadIndirect() &&
                grandChild->getReferenceCount() == 1 &&
                grandChild->getRegister() == 0)
               {
               TR::Node::recreate(grandChild, TR::bloadi);
               secondChild->decReferenceCount();
               secondChild = grandChild;
               if (reportShiftAmount)
                  diagnostic("shift: removed sign or zero extend to shift amount in memory in method %s\n", comp->signature());
               }
            else if (grandChild->getOpCode().isLoadVarDirect() &&
                     grandChild->getReferenceCount() == 1 &&
                     grandChild->getRegister() == 0)
               {
               TR::Node::recreate(grandChild, TR::bload);
               secondChild->decReferenceCount();
               secondChild = grandChild;
               if (reportShiftAmount)
                  diagnostic("shift: removed sign or zero extend to shift amount in memory in method %s\n", comp->signature());
               }
            else if (op != TR::l2i || comp->target().is64Bit())
               {
               secondChild->decReferenceCount();
               secondChild = grandChild;
               if (reportShiftAmount)
                  diagnostic("shift: removed sign or zero extend to shift amount in method %s\n", comp->signature());
               }
            else if (op == TR::l2i && grandChild->getRegister())
               {
               // Pick up the low order register if the load has been evaluated
               secondChild->decReferenceCount();
               secondChild = grandChild;
               shiftAmountReg = grandChild->getRegister()->getLowOrder();
               }
            }
         }

      if (!shiftAmountReg)
         shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      instr = generateMemRegInstruction(regShiftOpCode, node, memRef, shiftAmountReg, shiftDependencies, cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shift by var", node);
      }

   if (oursIsTheOnlyMemRef)
      memRef->decNodeReferenceCounts(cg);
   else
      memRef->stopUsingRegisters(cg);
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return instr;
   }

TR::Register *OMR::X86::TreeEvaluator::integerShlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool       nodeIs64Bit     = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Node  *shiftAmountNode = node->getSecondChild();
   intptr_t shiftAmount;
   TR::Compilation *comp = cg->comp();

   if (node->isDirectMemoryUpdate())
      {
      TR::TreeEvaluator::generateMemoryShift(node, SHLMemImm1(nodeIs64Bit), SHLMemCL(nodeIs64Bit), cg);
      }
   else if (shiftAmountNode->getOpCode().isLoadConst()  &&
            (shiftAmount = TR::TreeEvaluator::integerConstNodeValue(shiftAmountNode, cg) & TR::TreeEvaluator::shiftMask(nodeIs64Bit),
             shiftAmount > 0 && shiftAmount <= 3)       &&
             performTransformation(comp, "O^O IntegerShlEvaluator: replace shift with lea\n"))
      {
      // Turn small shift into LEA
      //
      TR::Node *firstChild  = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();
      TR::MemoryReference  *memRef = generateX86MemoryReference(cg);
      memRef->setIndexRegister(cg->evaluate(firstChild));
      memRef->setStride(shiftAmount);
      TR::Register *targetRegister = cg->allocateRegister();
      generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, targetRegister, memRef, cg);
      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR::TreeEvaluator::generateRegisterShift(node, SHLRegImm1(nodeIs64Bit), SHLRegCL(nodeIs64Bit), cg);
      }
   return node->getRegister();
   }

TR::Register *OMR::X86::TreeEvaluator::integerRolEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = NULL;
   TR::Node     *secondChild = node->getSecondChild();
   TR::Node     *firstChild  = node->getFirstChild();
   bool          nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(node, cg);

   if (secondChild->getOpCode().isLoadConst())
      {
      intptr_t rotateAmount = TR::TreeEvaluator::integerConstNodeValue(secondChild, cg) & TR::TreeEvaluator::rotateMask(nodeIs64Bit);
      if (rotateAmount == 0)
         {
         targetRegister = cg->evaluate(firstChild);
         }
      else
         {
         targetRegister = TR::TreeEvaluator::intOrLongClobberEvaluate(firstChild, nodeIs64Bit, cg);
         generateRegImmInstruction(ROLRegImm1(nodeIs64Bit), node, targetRegister, rotateAmount , cg);
         }
      }
   else
      {
      targetRegister = TR::TreeEvaluator::intOrLongClobberEvaluate(firstChild, nodeIs64Bit, cg);
      TR::Register *rotateAmountReg = cg->evaluate(secondChild);
      TR::RegisterDependencyConditions  *rotateDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);

      rotateDependencies->addPreCondition(rotateAmountReg, TR::RealRegister::ecx, cg);
      rotateDependencies->addPostCondition(rotateAmountReg, TR::RealRegister::ecx, cg);

      generateRegRegInstruction(ROLRegCL(nodeIs64Bit), node, targetRegister, rotateAmountReg, rotateDependencies, cg);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::integerShrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   if (node->isDirectMemoryUpdate())
      {
      TR::TreeEvaluator::generateMemoryShift(node, SARMemImm1(nodeIs64Bit), SARMemCL(nodeIs64Bit), cg);
      }
   else
      {
      TR::TreeEvaluator::generateRegisterShift(node, SARRegImm1(nodeIs64Bit), SARRegCL(nodeIs64Bit), cg);
      }
   return node->getRegister();
   }

TR::Register *OMR::X86::TreeEvaluator::integerUshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Compilation *comp = cg->comp();

   // Just in case this is a lowered "arraylength" opcode below a null check,
   // save the instruction that would cause an implicit exception since it might
   // be clobbered by evaluation of the second child.
   //
   TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();

   if (node->isDirectMemoryUpdate())
      {
      TR::Instruction *shiftInstruction = TR::TreeEvaluator::generateMemoryShift(node, SHRMemImm1(nodeIs64Bit), SHRMemCL(nodeIs64Bit), cg);
      if (shiftInstruction)
         faultingInstruction = shiftInstruction;
      }
   else
      {
      TR::TreeEvaluator::generateRegisterShift(node, SHRRegImm1(nodeIs64Bit), SHRRegCL(nodeIs64Bit), cg);
      }

   if (comp->useCompressedPointers() && nodeIs64Bit &&
         (TR::Compiler->vm.heapBaseAddress() == 0) &&
         (node->getFirstChild()->getOpCodeValue() == TR::a2l))
      {
      node->setIsHighWordZero(true);
      }
   // Now set up the proper faulting instruction for this sequence
   //
   cg->setImplicitExceptionPoint(faultingInstruction);

   return node->getRegister();
   }

TR::Register *OMR::X86::TreeEvaluator::bshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Node            *firstChild          = node->getFirstChild();
   TR::MemoryReference *tempMR              = NULL;
   TR::Instruction     *instr               = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::ILOpCodes testOpCode = secondChild->getOpCodeValue();
   if (testOpCode == TR::bconst && performTransformation(comp, "O^O BSHLEvaluator: second child is not an 8-bit signed, two's complement number: %x\n", testOpCode))
      {
      int32_t value = secondChild->getByte();
      if (isMemOp)
         {
         if (value != 0)
            {
            instr = generateMemImmInstruction(SHL1MemImm1, node, tempMR, value, cg);
            if (debug("traceMemOp"))
               diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shl by const %d", node, value);
            }
         }
      else
         {
         if (value == 0)
            {
            targetRegister = cg->intClobberEvaluate(firstChild);
            }
         else if (value <= 3 && firstChild->getReferenceCount() > 1)
            {
            targetRegister = cg->evaluate(firstChild);
            TR::MemoryReference  *tempMR = generateX86MemoryReference(cg);
            tempMR->setIndexRegister(targetRegister);
            tempMR->setStride(value);
            targetRegister = cg->allocateRegister();
            instr = generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
            }
         else
            {
            targetRegister = cg->intClobberEvaluate(firstChild);
            instr = generateRegImmInstruction(SHL1RegImm1, node, targetRegister, value, cg);
            }
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      if (isMemOp)
         {
         instr = generateMemRegInstruction(SHL1MemCL, node, tempMR, shiftAmountReg, shiftDependencies, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT"] shl by var", node);
         }
      else
         {
         targetRegister = cg->intClobberEvaluate(firstChild);
         instr = generateRegRegInstruction(SHL1RegCL, node, targetRegister, shiftAmountReg, shiftDependencies, cg);
         }
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      if (instr)
         cg->setImplicitExceptionPoint(instr);
      }
   else
      {
      if (cg->enableRegisterInterferences())
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::sshlEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *secondChild         = node->getSecondChild();
   TR::Node            *firstChild          = node->getFirstChild();
   TR::MemoryReference *tempMR              = NULL;
   TR::Instruction     *instr               = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   TR::ILOpCodes testOpcode = secondChild->getOpCodeValue();
   if (testOpcode == TR::iconst && performTransformation(comp, "O^O SSHLEvaluator: second child is not a 16-bit integer constant: %x\n", testOpcode))
      {
      int32_t value = secondChild->getInt();
      if (isMemOp)
         {
         if (value != 0)
            {
            instr = generateMemImmInstruction(SHL2MemImm1, node, tempMR, value, cg);
            if (debug("traceMemOp"))
               diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shl by const %d", node, value);
            }
         }
      else
         {
         if (value == 0)
            {
            targetRegister = cg->intClobberEvaluate(firstChild);
            }
         else if (value <= 3 && firstChild->getReferenceCount() > 1)
            {
            targetRegister = cg->evaluate(firstChild);
            TR::MemoryReference  *tempMR = generateX86MemoryReference(cg);
            tempMR->setIndexRegister(targetRegister);
            tempMR->setStride(value);
            targetRegister = cg->allocateRegister();
            instr = generateRegMemInstruction(LEA4RegMem, node, targetRegister, tempMR, cg);
            }
         else
            {
            targetRegister = cg->intClobberEvaluate(firstChild);
            instr = generateRegImmInstruction(SHL4RegImm1, node, targetRegister, value, cg);
            }
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      if (isMemOp)
         {
         instr = generateMemRegInstruction(SHL2MemCL, node, tempMR, shiftAmountReg, shiftDependencies, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shl by var", node);
         }
      else
         {
         targetRegister = cg->intClobberEvaluate(firstChild);
         instr = generateRegRegInstruction(SHL4RegCL, node, targetRegister, shiftAmountReg, shiftDependencies, cg);
         }
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      if (instr)
         cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::bshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::MemoryReference *tempMR              = NULL;
   TR::Instruction     *instr               = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }
   else
      {
      targetRegister = cg->intClobberEvaluate(firstChild);
      }

   TR::ILOpCodes testOpcode = secondChild->getOpCodeValue();
   if (testOpcode == TR::bconst && performTransformation(comp, "O^O BSHREvaluator: second child is not an 8-bit signed Two's complement opcode %x\n", testOpcode))
      {
      int32_t value = secondChild->getByte();
      if (value != 0)
         {
         if (isMemOp)
            {
            instr = generateMemImmInstruction(SAR1MemImm1, node, tempMR, value, cg);
            if (debug("traceMemOp"))
               diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT"] shr by const %d", node, value);
            }
         else
            instr = generateRegImmInstruction(SAR1RegImm1, node, targetRegister, value, cg);
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      if (isMemOp)
         {
         instr = generateMemRegInstruction(SAR1MemCL, node, tempMR, shiftAmountReg, shiftDependencies, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shr by var", node);
         }
      else
         instr = generateRegRegInstruction(SAR1RegCL, node, targetRegister, shiftAmountReg, shiftDependencies, cg);
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      if (instr)
         cg->setImplicitExceptionPoint(instr);
      }
   else
      {
      if (cg->enableRegisterInterferences())
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::sshrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::MemoryReference *tempMR              = NULL;
   TR::Instruction     *instr               = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register * reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }
   else
      {
      targetRegister = cg->intClobberEvaluate(firstChild);
      }

   TR::ILOpCodes testOpcode = secondChild->getOpCodeValue();
   if (testOpcode == TR::iconst && performTransformation(comp, "O^O SSHREvaluator: second child is not a 16-bit signed two's complement number %x\n", testOpcode))
      {
      int32_t value = secondChild->getInt();
      if (value != 0)
         {
         if (isMemOp)
            {
            instr = generateMemImmInstruction(SAR2MemImm1, node, tempMR, value, cg);
            if (debug("traceMemOp"))
               diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shr by const %d", node, value);
            }
         else
            instr = generateRegImmInstruction(SAR2RegImm1, node, targetRegister, value, cg);
         }
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      if (isMemOp)
         {
         instr = generateMemRegInstruction(SAR2MemCL, node, tempMR, shiftAmountReg, shiftDependencies, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] shr by var", node);
         }
      else
         instr = generateRegRegInstruction(SAR2RegCL, node, targetRegister, shiftAmountReg, shiftDependencies, cg);
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      if (instr)
         cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::bushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::MemoryReference *tempMR              = NULL;
   TR::Instruction     *instr               = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();
   TR::ILOpCodes testOpcode1 = firstChild->getOpCodeValue();
   TR::ILOpCodes testOpcode2 = secondChild->getOpCodeValue();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }
   else if (testOpcode1 == TR::bconst &&
            performTransformation(comp, "O^O BUSHREvaluator: first child is not an 8-bit signed two's complement, or an 8 bit unsigned %x\n", testOpcode1))
      {
      targetRegister = cg->allocateRegister();
      int32_t value = firstChild->get64bitIntegralValue();
      generateRegImmInstruction(MOV1RegImm1, node, targetRegister, value, cg);
      }
   else
      {
      targetRegister = cg->intClobberEvaluate(firstChild);
      }

   if (testOpcode2 == TR::bconst &&
       performTransformation(comp, "O^O BUSHREvaluator: first child is not an 8-bit signed two's complement, or an 8 bit unsigned %x\n", testOpcode2))
      {
      int32_t value = secondChild->get64bitIntegralValue();

      if (isMemOp)
         {
         instr = generateMemImmInstruction(SHR1MemImm1, node, tempMR, value, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] ushr by const %d", node, value);
         }
      else
         instr = generateRegImmInstruction(SHR1RegImm1, node, targetRegister, value, cg);
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      if (isMemOp)
         {
         instr = generateMemRegInstruction(SHR1MemCL, node, tempMR, shiftAmountReg, shiftDependencies, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] ushr by var", node);
         }
      else
         instr = generateRegRegInstruction(SHR1RegCL, node, targetRegister, shiftAmountReg, shiftDependencies, cg);
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      if (instr)
         cg->setImplicitExceptionPoint(instr);
      }
   else
      {
      if (cg->enableRegisterInterferences())
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::sushrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register        *targetRegister      = NULL;
   TR::Node            *firstChild          = node->getFirstChild();
   TR::Node            *secondChild         = node->getSecondChild();
   TR::MemoryReference *tempMR              = NULL;
   TR::Instruction     *instr               = NULL;
   bool                 oursIsTheOnlyMemRef = true;
   TR::Compilation     *comp                = cg->comp();

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      // Make sure the original value is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }
   else
      {
      targetRegister = cg->intClobberEvaluate(firstChild);
      }

   TR::ILOpCodes testOpcode = secondChild->getOpCodeValue();
   if (testOpcode == TR::iconst && performTransformation(comp, "O^O SUSHREvaluator: opcode is not a 16-bit signed two's complement %x\n", testOpcode))
      {
      int32_t value = secondChild->getInt();
      if (isMemOp)
         {
         instr = generateMemImmInstruction(SHR2MemImm1, node, tempMR, value, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] ushr by const %d", node, value);
         }
      else
         instr = generateRegImmInstruction(SHR2RegImm1, node, targetRegister, value, cg);
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);

      TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(shiftAmountReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(shiftAmountReg, TR::RealRegister::ecx, cg);

      if (isMemOp)
         {
         instr = generateMemRegInstruction(SHR2MemCL, node, tempMR, shiftAmountReg, shiftDependencies, cg);
         if (debug("traceMemOp"))
            diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] ushr by var", node);
         }
      else
         instr = generateRegRegInstruction(SHR2RegCL, node, targetRegister, shiftAmountReg, shiftDependencies, cg);
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      if (instr)
         cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR_X86OpCodes OMR::X86::TreeEvaluator::_logicalOpPackage[numLogicalOpPackages][numLogicalOpForms] =
   {
   // band
      { AND1RegReg, AND1RegMem, MOV1RegReg, AND1RegImm1, BADIA32Op,
        AND1MemImm1,AND1MemImm1,AND1MemReg, NOT1Mem   },
   // cand/sand
      { AND2RegReg, AND2RegMem, MOV4RegReg, AND2RegImms, AND2RegImm2,
        AND2MemImms,AND2MemImm2,AND2MemReg, NOT2Mem },
   // iand
      { AND4RegReg, AND4RegMem, MOV4RegReg, AND4RegImms, AND4RegImm4,
        AND4MemImms,AND4MemImm4,AND4MemReg, NOT4Mem },
   // land
      { AND8RegReg, AND8RegMem, MOV8RegReg, AND8RegImms, AND8RegImm4,
        AND8MemImms,AND8MemImm4,AND8MemReg, NOT8Mem },
   // bor
      { OR1RegReg,  OR1RegMem,  MOV1RegReg, OR1RegImm1,  BADIA32Op,
        OR1MemImm1, OR1MemImm1, OR1MemReg,  NOT1Mem   },
   // cor, sor
      { OR2RegReg,  OR2RegMem,  MOV4RegReg, OR2RegImms,  OR2RegImm2,
        OR2MemImms, OR2MemImm2, OR2MemReg,  NOT2Mem   },
   // ior
      { OR4RegReg,  OR4RegMem,  MOV4RegReg, OR4RegImms,  OR4RegImm4,
        OR4MemImms, OR4MemImm4, OR4MemReg,  NOT4Mem   },
   // lor
      { OR8RegReg,  OR8RegMem,  MOV8RegReg, OR8RegImms,  OR8RegImm4,
        OR8MemImms, OR8MemImm4, OR8MemReg,  NOT8Mem   },
   // bxor
      { XOR1RegReg, XOR1RegMem, MOV1RegReg, XOR1RegImm1, BADIA32Op,
        XOR1MemImm1,XOR1MemImm1,XOR1MemReg, NOT1Mem   },
   // cxor, sxor
      { XOR2RegReg, XOR2RegMem, MOV4RegReg, XOR2RegImms, XOR2RegImm2,
        XOR2MemImms,XOR2MemImm2,XOR2MemReg, NOT2Mem   },
   // ixor
      { XOR4RegReg, XOR4RegMem, MOV4RegReg, XOR4RegImms, XOR4RegImm4,
        XOR4MemImms,XOR4MemImm4,XOR4MemReg, NOT4Mem   },
   // lxor
      { XOR8RegReg, XOR8RegMem, MOV8RegReg, XOR8RegImms, XOR8RegImm4,
        XOR8MemImms,XOR8MemImm4,XOR8MemReg, NOT8Mem   },
   };


TR::Register *OMR::X86::TreeEvaluator::logicalEvaluator(TR::Node          *node,
                                                   TR_X86OpCodes    package[],
                                                   TR::CodeGenerator *cg)
   {
   bool                 nodeIs64Bit              = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   TR::Register        *targetRegister           = NULL;
   TR::Node            *firstChild               = node->getFirstChild();
   TR::Node            *secondChild              = node->getSecondChild();
   intptr_t            constValue               = 0;
   TR::MemoryReference *tempMR                   = NULL;
   TR::Instruction     *instr                    = NULL;
   bool                 oursIsTheOnlyMemRef      = true;
   bool                 firstGrandchildEvaluated = false;
   TR::Compilation     *comp                     = cg->comp();

   TR::Register * testRegister = secondChild->getRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       testRegister == NULL     &&
       performTransformation(comp, "O^O LogicalEvaluator: checking that the store has not happened yet. Target register: %x\n", testRegister))
      {
      switch (secondChild->getDataType())
         {
         case TR::Int64:
            constValue = secondChild->getLongInt();
            break;
         case TR::Int32:
            constValue = secondChild->getInt();
            break;
         case TR::Int16:
            constValue = secondChild->getShortInt();
            break;
         case TR::Int8:
            constValue = secondChild->getByte();
            break;
         default:
            TR_ASSERT(0, "Unexpected data type for logical node [" POINTER_PRINTF_FORMAT "]\n", secondChild);
         }
      }

   // See if we can generate a direct memory operation. In this case there is no
   // target register generated and we return NULL to the caller (which should be
   // a store) to indicate that the store has already been done.
   //
   bool isMemOp = node->isDirectMemoryUpdate();

   if (isMemOp)
      {
      TR_ASSERT(!node->nodeRequiresConditionCodes(),
             "a node requiring CC evaluation should not be a direct mem update");

      // Make sure the original constValue is evaluated before the update if it
      // is going to be used again.
      //
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *reg = cg->evaluate(firstChild);
         tempMR = generateX86MemoryReference(*reg->getMemRef(), 0, cg);
         oursIsTheOnlyMemRef = false;
         }
      else
         {
         tempMR = generateX86MemoryReference(firstChild, cg, false);
         }
      }

   testRegister = secondChild->getRegister();
   if (secondChild->getOpCode().isLoadConst() &&
       testRegister == NULL     &&
       IS_32BIT_SIGNED(constValue)            &&
       performTransformation(comp, "O^O checking that the store has not happened yet. Target register: %x\n", testRegister))
      {
      if (!isMemOp && node->getOpCode().isAnd()
          && !firstChild->getRegister()
          && firstChild->getOpCode().isConversion()
          && firstChild->getType().isIntegral()
          && firstChild->getFirstChild()->getType().isIntegral()
          && firstChild->getSize() > firstChild->getFirstChild()->getSize()
          && ((constValue >> (8 * firstChild->getFirstChild()->getSize())) == 0))
         {
         targetRegister = cg->evaluate(firstChild->getFirstChild());
         if (firstChild->getFirstChild()->getReferenceCount() > 1
             || firstChild->getReferenceCount() > 1)
            {
            TR::Register *temp = cg->allocateRegister();
            generateRegRegInstruction(MOVRegReg(nodeIs64Bit), node, temp, targetRegister, cg);
            targetRegister = temp;
            }
         firstGrandchildEvaluated = true;
         }
      if (!firstGrandchildEvaluated && !isMemOp)
         targetRegister = TR::TreeEvaluator::intOrLongClobberEvaluate(firstChild, nodeIs64Bit, cg);

      if (node->getOpCode().isXor() && constValue == -1)
         {
         if (isMemOp)
            {
            instr = generateMemInstruction(package[TR::TreeEvaluator::logicalNotOp], node, tempMR, cg);
            if (debug("traceMemOp"))
               diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] logical not", node);
            }
         else
            {
            // No harm doing the whole register
            instr = generateRegInstruction(NOTReg(nodeIs64Bit), node, targetRegister, cg);
            }
         }
      else
         {
         if (constValue >= -128 && constValue <= 127)
            {
            if (isMemOp)
               instr = generateMemImmInstruction(package[TR::TreeEvaluator::logicalByteMemImmOp], node, tempMR, constValue, cg);
            else
               instr = generateRegImmInstruction(package[TR::TreeEvaluator::logicalByteImmOp], node, targetRegister, constValue, cg);
            }
         else
            {
            if (isMemOp)
               instr = generateMemImmInstruction(package[TR::TreeEvaluator::logicalMemImmOp], node, tempMR, constValue, cg);
            else
               instr = generateRegImmInstruction(package[TR::TreeEvaluator::logicalImmOp], node, targetRegister, constValue, cg);
            }
         }
      if (debug("traceMemOp") && isMemOp)
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] logical op by const %d", node, constValue);
      }
   else if (isMemOp)
      {
      instr = generateMemRegInstruction(package[TR::TreeEvaluator::logicalMemRegOp], node, tempMR, cg->evaluate(secondChild), cg);
      if (debug("traceMemOp"))
         diagnostic("\n*** Node [" POINTER_PRINTF_FORMAT "] logical op by var", node);
      }
   else
      {
      TR_X86BinaryCommutativeAnalyser  temp(cg);
      temp.genericAnalyser(node,
                           package[TR::TreeEvaluator::logicalRegRegOp],
                           package[TR::TreeEvaluator::logicalRegMemOp],
                           package[TR::TreeEvaluator::logicalCopyOp]);
      targetRegister = node->getRegister();
      return targetRegister;
      }

   if (isMemOp)
      {
      if (oursIsTheOnlyMemRef)
         tempMR->decNodeReferenceCounts(cg);
      else
         tempMR->stopUsingRegisters(cg);
      cg->setImplicitExceptionPoint(instr);
      }
   node->setRegister(targetRegister);
   if (firstGrandchildEvaluated)
      cg->recursivelyDecReferenceCount(firstChild);
   else
      cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::bandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[bandOpPackage], cg);

   if (cg->enableRegisterInterferences() && targetRegister)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::sandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[sandOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::candEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[candOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::iandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[iandOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::borEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[borOpPackage], cg);

   if (cg->enableRegisterInterferences() && targetRegister)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::sorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[sorOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::corEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[corOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::iorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[iorOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::bxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[bxorOpPackage], cg);

   if (cg->enableRegisterInterferences() && targetRegister)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::sxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[sxorOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::cxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[cxorOpPackage], cg);
   }

TR::Register *OMR::X86::TreeEvaluator::ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::logicalEvaluator(node, _logicalOpPackage[ixorOpPackage], cg);
   }
