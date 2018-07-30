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

#ifndef OMR_SIMPLIFIERHANDLERS_INCL
#define OMR_SIMPLIFIERHANDLERS_INCL

#include "optimizer/OMRSimplifierHelpers.hpp"
#include "optimizer/OMRSimplifierHandlers.hpp"

#include <limits.h>
#include <math.h>
#include "compile/Compilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage.hpp"                 // for Linkage
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                          // for POINTER_PRINTF_FORMAT
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"                    // for getMinSigned, etc
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"           // for LabelSymbol
#include "il/symbol/StaticSymbol.hpp"          // for StaticSymbol
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Simplifier.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "optimizer/TransformUtil.hpp"

#define OP_PLUS  0
#define OP_MINUS 1
#define CLEARBIT32 0x7FFFFFFF

#define TR_MAX_PADDING 256
#define TR_MAX_LENGTH_FOR_UNEVEN_LENGTHS 16
#define TR_SPACE_CHAR 0x40
#define TR_MAX_PADDING_SIZE 16000
#define TR_PDPOWER_POSITIVE_INLINE_THRESHOLD 32
#define TR_PDPOWER_NEGATIVE_INLINE_THRESHOLD -32
#define TR_MAX_OCONST_FOLDING_SIZE 256
#define TR_MAX_ARRAYSET_EXPANSION_LEN 256
#define DFP_SHIFT_AMOUNT_CHILD_INDEX 1


#define FMA_CONST_LOWBOUND     5.915260931E-272
#define FMA_CONST_HIGHBOUNDI2D 8.371160993643E298
#define FMA_CONST_HIGHBOUNDF2D 5.282945626245E269



/**
 * If both sides of a branch are adds, simplify to use atmost one add
 * e.g. if (a + b != x + y) where b and y are constants, would be simplified
 * to   if (a != x + (y - b))
 *
 * This cannot be applied when the comparison operator is a <, <=, >, >=
 * in general.  That _may_ be possible when more information about the value
 * ranges is known (i.e. during value propagation)
 */
#define SIMPLIFY_BRANCH_ARITHMETIC(Type,Width)                                     \
   if ((firstChild ->getOpCode().isSub() || firstChild ->getOpCode().isAdd()) &&   \
       (firstChild ->getSecondChild()->getOpCode().isLoadConst()) &&               \
       (secondChild->getOpCode().isSub() || secondChild->getOpCode().isAdd()) &&   \
       (secondChild->getSecondChild()->getOpCode().isLoadConst()) &&               \
       (firstChild ->getReferenceCount()==1) &&                                    \
       (secondChild ->getReferenceCount()==1))                                     \
      {                                                                            \
      Width konst;                                                                 \
      if (firstChild->getOpCode().isSub())                                         \
         {                                                                         \
         if (secondChild->getOpCode().isSub())                                     \
            konst = secondChild->getSecondChild()->get##Type() -                   \
                    firstChild->getSecondChild()->get##Type();                     \
         else                                                                      \
            konst = secondChild->getSecondChild()->get##Type() +                   \
                    firstChild->getSecondChild()->get##Type();                     \
         }                                                                         \
      else                                                                         \
         {                                                                         \
         if (secondChild->getOpCode().isAdd())                                     \
            konst = secondChild->getSecondChild()->get##Type() -                   \
               firstChild->getSecondChild()->get##Type();                          \
         else                                                                      \
            konst = secondChild->getSecondChild()->get##Type() +                   \
               firstChild->getSecondChild()->get##Type();                          \
         }                                                                         \
      node->setAndIncChild(0, firstChild->getFirstChild());                        \
      firstChild->recursivelyDecReferenceCount();                                  \
      firstChild = firstChild->getFirstChild();                                    \
      if (konst == 0)                                                              \
         {                                                                         \
         node->setAndIncChild(1, secondChild->getFirstChild());                    \
         secondChild->recursivelyDecReferenceCount();                              \
         secondChild = secondChild->getFirstChild();                               \
         }                                                                         \
      else                                                                         \
         {                                                                         \
         TR::Node * grandChild = secondChild->getSecondChild();                      \
         if (grandChild->getReferenceCount() == 1)                                 \
            grandChild->set##Type(konst);                                          \
         else                                                                      \
            {                                                                      \
            grandChild->recursivelyDecReferenceCount();                            \
            secondChild->setAndIncChild(1, TR::Node::create(grandChild, grandChild->getOpCodeValue(), 0, (int32_t)konst)); \
            }                                                                      \
         }                                                                         \
      dumpOptDetails(s->comp(), "%ssimplified arithmetic in branch [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(),        \
                  node);                                                           \
      }

/**
 * Binary identity or zero operation
 *
 * If the second child is a constant that represents an identity operation,
 * replace this node with the first child.
 *
 * If the second child is a constant that represents a zero operation,
 * replace this node with the second child.
 */
#define BINARY_IDENTITY_OR_ZERO_OP(ConstType,Type,NullValue,ZeroValue)\
   if(secondChild->getOpCode().isLoadConst())\
      {\
      ConstType value = secondChild->get##Type();\
      if(value == NullValue)\
         return s->replaceNodeWithChild(node, firstChild, s->_curTree, block);\
      if(value == ZeroValue)\
         {\
         if(performTransformation(s->comp(), "%sFound op with iconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(),node))\
            {\
            s->anchorChildren(node, s->_curTree);\
            s->prepareToReplaceNode(node, secondChild->getOpCodeValue());\
            node->set##Type(value);\
            return node;\
            }\
         }\
      }

static TR::ILOpCodes addOps[TR::NumTypes] = { TR::BadILOp,
                                               TR::badd,    TR::sadd,    TR::iadd,    TR::ladd,
                                               TR::fadd,    TR::dadd,
                                               TR::BadILOp, TR::BadILOp, TR::BadILOp,
                                               TR::BadILOp, TR::BadILOp,  TR::BadILOp};

static TR::ILOpCodes subOps[TR::NumTypes] = { TR::BadILOp,
                                               TR::bsub,    TR::ssub,    TR::isub,    TR::lsub,
                                               TR::fsub,    TR::dsub,
                                               TR::asub,    TR::BadILOp, TR::BadILOp,
                                               TR::BadILOp, TR::BadILOp, TR::BadILOp};

static TR::ILOpCodes constOps[TR::NumTypes] = { TR::BadILOp,
                                                 TR::bconst,  TR::sconst,  TR::iconst,    TR::lconst,
                                                 TR::fconst,  TR::dconst,
                                                 TR::aconst,  TR::BadILOp,  TR::BadILOp,
                                                 TR::BadILOp, TR::BadILOp, TR::BadILOp};

static TR::ILOpCodes negOps[TR::NumTypes] = { TR::BadILOp,
                                               TR::bneg,    TR::sneg,    TR::ineg,    TR::lneg,
                                               TR::fneg,    TR::dneg,
                                               TR::BadILOp, TR::BadILOp, TR::BadILOp,
                                               TR::BadILOp, TR::BadILOp, TR::BadILOp};


/*
 * Local helper functions
 */


template <typename T>
inline void setCCAddSigned(T value, T operand1, T operand2, TR::Node *node, TR::Simplifier * s)
   {
   // for an add (T = A + B) an overflow occurs iff the sign(A) == sign(B) and sign(T) != sign(A)
   if (((operand1<0) == (operand2<0)) &&
       !((value<0) == (operand1<0)))
      s->setCC(node, OMR::ConditionCode3);
   else if ( value < 0 )
      s->setCC(node, OMR::ConditionCode1);
   else if ( value > 0 )
      s->setCC(node, OMR::ConditionCode2);
   else
      s->setCC(node, OMR::ConditionCode0);
   }

template <typename T>
inline void setCCAddUnsigned(T value, T operand1, TR::Node *node, TR::Simplifier * s)
   {
   bool carry = value < operand1;

   if (value == 0 && !carry)
      s->setCC(node, OMR::ConditionCode0);
   else if (value != 0 && !carry)
      s->setCC(node, OMR::ConditionCode1);
   else if (value == 0 && carry)
      s->setCC(node, OMR::ConditionCode2);
   else
      s->setCC(node, OMR::ConditionCode3);
   }

template <typename T>
inline void setCCSubSigned(T value, T operand1, T operand2, TR::Node *node, TR::Simplifier * s)
   {
   // for a sub (T = A - B) an overflow occurs iff the sign(A) != sign(B) and sign(T) == sign(B)
   if (!((operand1<0) == (operand2<0)) &&
       ((value<0) == (operand2<0)))
      s->setCC(node, OMR::ConditionCode3);
   else if ( value < 0 )
      s->setCC(node, OMR::ConditionCode1);
   else if ( value > 0 )
      s->setCC(node, OMR::ConditionCode2);
   else
      s->setCC(node, OMR::ConditionCode0);
   }

template <typename T>
inline void setCCSubUnsigned(T value, T operand1, TR::Node *node, TR::Simplifier * s)
   {
   bool borrow = value > operand1;

   if (value != 0 && borrow)
      s->setCC(node, OMR::ConditionCode1);
   else if (value == 0 && !borrow)
      s->setCC(node, OMR::ConditionCode2);
   else if (value != 0 && !borrow)
      s->setCC(node, OMR::ConditionCode3);
   else
      TR_ASSERT(0,"condition code of 0 is not possible for logical sub");
   }

template <typename T>
inline void setCCOr(T value, TR::Node *node, TR::Simplifier *s)
   {
   if (value == (T)0)
      s->setCC(node, OMR::ConditionCode0);
   else
      s->setCC(node, OMR::ConditionCode1);
   }

static void convertToTestUnderMask(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   // butest evaluator is only implemented on Z
   if (!TR::Compiler->target.cpu.isZ())
      return;

   if (debug("disableConvertToTestUnderMask"))
      return;
   auto cm = s->comp();

   /*
    * ificmpeq          ificmpeq
    *   iand              butest
    *     iushr             bloadi x
    *       iloadi x          aload
    *         aload         bconst 1
    *       iconst 24     iconst 0
    *     iconst 1
    *   iconst 0
    */
   TR::Node *iand, *iushr, *load, *constCompare;
   int32_t constVal;
   if ((iand = node->getChild(0))->getOpCodeValue() == TR::iand &&
       node->getChild(1)->getOpCode().isLoadConst() &&
       (constVal = node->getChild(1)->getConst<int32_t>()) == 0 &&
       (iushr = iand->getChild(0))->getOpCodeValue() == TR::iushr &&
       iand->getChild(1)->getOpCode().isLoadConst() &&
       (constVal = iand->getChild(1)->getConst<int32_t>()) >= TR::getMinSigned<TR::Int8>() && constVal <= TR::getMaxSigned<TR::Int8>() &&
       (load = iushr->getChild(0))->getOpCode().isLoadVar() &&
       iushr->getChild(1)->getOpCode().isLoadConst() &&
       (constVal = iushr->getChild(1)->getConst<int32_t>()) == 24 &&
       !load->getSymbol()->isParm() && load->getOpCode().isLoadIndirect() &&  // Limit it to non-parm, indirect cases
       load->getReferenceCount() == 1 &&
       performTransformation(cm, "%sTransforming iand/iushr to byte test under mask ["
             POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
      {
      // anchor iand subtree
      auto anchor = TR::TreeTop::create(s->comp(), TR::Node::create(TR::treetop, 1, iand));
      s->_curTree->insertBefore(anchor);

      auto bloadi = TR::Node::createWithSymRef(TR::bloadi, 1, load->getChild(0), 0, load->getSymbolReference());
      node->setAndIncChild(0, TR::Node::create(TR::butest, 2, bloadi,
            TR::Node::bconst(iand->getChild(1)->getConst<int8_t>())));
      iand->recursivelyDecReferenceCount();
      }
   }

static bool isBitwiseIntComplement(TR::Node * n)
   {
   if (n->getOpCodeValue() == TR::ixor)
      {
      TR::Node * secondChild = n->getSecondChild();
      if (secondChild->getOpCodeValue() == TR::iconst &&
          secondChild->getInt() == -1)
         {
         return true;
         }
      }
   return false;
   }

static void setExprInvariant(TR_RegionStructure *region, TR::Node *node)
   {
   if (region && region->getInvariantExpressions())
      {
      region->setExprInvariant(node);
      }
   }

template <typename T>
static void foldConstant(TR::Node *node, T value, TR::Simplifier *s, bool anchorChildrenP)
   {
   if (!performTransformationSimplifier(node, s)) return;
   if (anchorChildrenP) s->anchorChildren(node, s->_curTree);
   s->prepareToReplaceNode(node, TR::ILOpCode::getConstOpCode<T>());
   node->setConst<T>(value);
   dumpOptDetails(s->comp(), " to %s %lld\n", node->getOpCode().getName(), value);
   }

template <typename T>
static TR::Node *binaryIdentityOp(TR::Node *node, T nil, TR::Simplifier *s)
   {
   TR::Node *secondChild = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getConst<T>() == nil)
      {
      return s->replaceNode(node, node->getFirstChild(), s->_curTree);
      }
   return 0;
   }

static bool isIMulComposerOpCode(TR::Node * node)
   {
   TR::ILOpCodes childOp = node->getOpCodeValue();

   if (childOp == TR::iadd || childOp == TR::isub || childOp == TR::imul || childOp == TR::ineg)
      return true;

   return false;
   }

bool imulComposer(TR::Node * node, int *value, TR::Node ** multiplier )
   {
   TR::Node * firstChild;
   TR::Node * secondChild;

   TR::Node * lMultiplier = NULL;
   TR::Node * rMultiplier = NULL;

   int lcount = 0;
   int rcount = 0;

   *multiplier = NULL;
   *value = 0;

   if (node->getOpCodeValue() == TR::iadd || node->getOpCodeValue() == TR::isub)
      {
      bool rAnswer = false, lAnswer = false;

      firstChild = node->getFirstChild();
      secondChild = node->getSecondChild();

      if (isIMulComposerOpCode(firstChild))
         {
         if (firstChild->getReferenceCount() > 1) return false;

         lAnswer = imulComposer(firstChild, &lcount, &lMultiplier);

         if (!lAnswer)
            return false;
         }
      else
         {
         lMultiplier = firstChild;
         lcount = 1;
         }

      if (isIMulComposerOpCode(secondChild))
         {
         if (secondChild->getReferenceCount() > 1) return false;

         rAnswer = imulComposer(secondChild, &rcount, &rMultiplier);

         if (!rAnswer)
            return false;
         }
      else
         {
         rMultiplier = secondChild;
         rcount = 1;
         }

      if (lMultiplier != rMultiplier)
         return false;

      if (!lAnswer && !rAnswer)
         return false;

      *multiplier = lMultiplier;

      if (node->getOpCodeValue() == TR::isub)
         *value = lcount - rcount;
      else
         *value = lcount + rcount;

      return true;
      }
   else if (node->getOpCodeValue() == TR::imul)
      {
      firstChild = node->getFirstChild();
      secondChild = node->getSecondChild();

      if (secondChild->getOpCodeValue() == TR::iconst && isNonNegativePowerOf2(secondChild->getInt()))
         {
         *multiplier = firstChild;
         *value = secondChild->getInt();

         return true;
         }
      }
   else if (node->getOpCodeValue() == TR::ineg)
      {
      bool lAnswer = false;

      firstChild = node->getFirstChild();

      if (isIMulComposerOpCode(firstChild))
         {
         if (firstChild->getReferenceCount() > 1) return false;

         lAnswer = imulComposer(firstChild, &lcount, &lMultiplier);

         if (!lAnswer)
            return false;
         }
      else
         {
         lMultiplier = firstChild;
         lcount = 1;
         }

      *value = -lcount;
      *multiplier = lMultiplier;

      return true;
      }

   return false;
   }

static bool isSmallConstant(TR::Node *node, TR::Simplifier *s)
   {
   if (node->getOpCode().isLoadConst() &&
       !s->comp()->cg()->isMaterialized(node))
      return true;
    return false;
   }

static void reassociateBigConstants(TR::Node *node, TR::Simplifier *s)
   {
   if (s->reassociate() &&
       (node->getOpCode().isAdd() || node->getOpCode().isSub()) &&
       node->getFirstChild()->getReferenceCount() > 1 &&
       node->getSecondChild()->getOpCode().isLoadConst() &&
       s->comp()->cg()->isMaterialized(node->getSecondChild()))
      {
      TR_HashId index;
      if (!s->_hashTable.locate(node->getFirstChild()->getGlobalIndex(), index))
         {
         s->_hashTable.add(node->getFirstChild()->getGlobalIndex(), index, node);
         }
      else
         {
         TR::Node *base_node = (TR::Node *) s->_hashTable.getData(index);

         if (base_node != node &&
             base_node->getReferenceCount() >= 1 &&
            (base_node->getOpCodeValue() == node->getOpCodeValue()) &&
            base_node->getFirstChild() == node->getFirstChild() &&
            // need to double check in case base_node has changed
             base_node->getSecondChild()->getOpCode().isLoadConst() &&
             s->comp()->cg()->isMaterialized(base_node->getSecondChild()))
            {
            int64_t diff = node->getSecondChild()->get64bitIntegralValue() -
                           base_node->getSecondChild()->get64bitIntegralValue();

            if (!s->comp()->cg()->shouldValueBeInACommonedNode(diff) &&
                performTransformation(s->comp(), "%sReusing big constant from node 0x%p in node 0x%p\n", s->optDetailString(), base_node, node))
               {
               node->getFirstChild()->recursivelyDecReferenceCount();
               node->getSecondChild()->recursivelyDecReferenceCount();

               TR::Node * diff_node = TR::Node::create(node,
                                     node->getSecondChild()->getOpCodeValue(), 0);
               diff_node->set64bitIntegralValue(diff);
               node->setAndIncChild (0, base_node);
               node->setAndIncChild (1, diff_node);
               }
            }
         }
      }
   }

static TR::Node *addSimplifierCommon(TR::Node* node, TR::Block * block, TR::Simplifier* s)
   {
   TR::Node* firstChild, *secondChild;
   if ((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
       (s->_curTree->getNode()->getNumChildren() > 0) &&
       s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
      (s->_curTree->getNode()->getFirstChild()->getLastChild() == node))
      return node;

   //
   //       a*add          a*add
   //       / \            / \
   //    a*add t2  =>  a*add  c1
   //     / \            / \
   //    t1  c1         t1 t2
   if (s->reassociate() &&
       node->getOpCode().isArrayRef())
      {
      firstChild = node->getFirstChild();
      secondChild = node->getSecondChild();
      if (firstChild->getOpCodeValue() == node->getOpCodeValue() &&
          firstChild->getSecondChild()->getOpCode().isLoadConst() &&
          !s->comp()->cg()->isMaterialized(firstChild->getSecondChild()))
         {
         if (performTransformation(s->comp(), "%sReordering constant terms in node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newFirstChild = TR::Node::create(firstChild, node->getOpCodeValue(), 2);
            newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
            newFirstChild->setAndIncChild(1, secondChild);
            newFirstChild->setIsInternalPointer(firstChild->isInternalPointer());

            node->setAndIncChild(0, newFirstChild);
            node->setAndIncChild(1, firstChild->getSecondChild());
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            }
         }
      }


   TR_RegionStructure * region = s->containingStructure();
   if (s->reassociate() &&
       node->getOpCode().isAdd())
      {
      firstChild = node->getFirstChild();
      secondChild = node->getSecondChild();

      // R9:    +              +
      //       / \            / \
      //      +-  c2    =>   t   +-
      //     / \                / \
      //    t   c1             c2 c1
      if ((firstChild->getOpCode().isAdd() ||
           firstChild->getOpCode().isSub()) &&
          ((!isExprInvariant(region, firstChild->getFirstChild()) &&
          isExprInvariant(region, firstChild->getSecondChild()) &&
     isExprInvariant(region, secondChild))  ||
          (!firstChild->getFirstChild()->getOpCode().isLoadConst() &&
          firstChild->getSecondChild()->getOpCode().isLoadConst() &&
     secondChild->getOpCode().isLoadConst())) &&

     !(firstChild->getReferenceCount() > 1 &&
          secondChild->getOpCode().isLoadConst() &&
          !s->comp()->cg()->isMaterialized(secondChild))
          )
         {
         if (performTransformation(s->comp(), "%sApplied reassociation rule 9 to node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newSecondChild = TR::Node::create(secondChild,
                                       firstChild->getOpCode().isAdd() ?
                                          addOps[secondChild->getDataType()] :
                                          subOps[secondChild->getDataType()], 2);

            TR::Node * c1 = firstChild->getSecondChild();
            if (c1->getDataType() !=
                secondChild->getDataType())
               {
               TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(c1->getDataType(), secondChild->getDataType(), false /* !wantZeroExtension */);

               c1 = TR::Node::create(conversionOpCode, 1, c1);
               }
            newSecondChild->setAndIncChild(0, secondChild);
            newSecondChild->setAndIncChild(1, c1);

            node->setAndIncChild(0, firstChild->getFirstChild());
            node->setAndIncChild(1, newSecondChild);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            setExprInvariant(region, newSecondChild);
            node->setVisitCount(0);
            node = s->simplify(node, block);
            }
         }

      // R9_1:
      //        +              +
      //       / \            / \
      //      c1  +    =>    +   t
      //         / \        / \
      //        t   c2     c1 c2
      else if (isExprInvariant(region, firstChild) &&
               secondChild->getOpCode().isAdd() &&
               !isExprInvariant(region, secondChild->getFirstChild()) &&
               isExprInvariant(region, secondChild->getSecondChild()) &&
               !isSmallConstant(secondChild->getSecondChild(), s))
         {
         if (performTransformation(s->comp(), "%sApplied reassociation rule 9_1 to node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newFirstChild = TR::Node::create(firstChild, node->getOpCodeValue(), 2);
            newFirstChild->setAndIncChild(0, firstChild);
            newFirstChild->setAndIncChild(1, secondChild->getSecondChild());

            node->setAndIncChild(0, newFirstChild);
            node->setAndIncChild(1, secondChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            setExprInvariant(region, newFirstChild);
            }
         }
      // R9_1_1:
      //        +              +
      //       / \            / \
      //      +  t2     =>   c   +
      //     / \                / \
      //    c  t1              t1 t2
      else if (firstChild->getOpCode().isAdd() &&
               isExprInvariant(region, firstChild->getFirstChild()) &&
               !isExprInvariant(region, firstChild->getSecondChild()) &&
               (!isExprInvariant(region, secondChild) ||
                 (node->getOpCode().isArrayRef() && isSmallConstant(secondChild, s))) &&
               firstChild->getSecondChild()->getDataType() == secondChild->getDataType())
         {
         if (performTransformation(s->comp(), "%sApplied reassociation rule 9_1_1 to node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newSecondChild = TR::Node::create(secondChild, addOps[secondChild->getDataType()], 2);
            newSecondChild->setAndIncChild(0, firstChild->getSecondChild());
            newSecondChild->setAndIncChild(1, secondChild);

            node->setAndIncChild(0, firstChild->getFirstChild());
            node->setAndIncChild(1, newSecondChild);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            }
         }
      // R9_1_2:
      //        +              -
      //       / \            / \
      //      -  c2     =>   +   t
      //     / \            / \
      //    c1  t         c1  c2
      else if (firstChild->getOpCode().isSub() &&
               firstChild->getFirstChild()->getOpCode().isLoadConst() &&
               secondChild->getOpCode().isLoadConst() &&
               firstChild->getFirstChild()->getDataType() == secondChild->getDataType())
         {
         if (performTransformation(s->comp(), "%sApplied reassociation rule 9_1_2 to node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newFirstChild = TR::Node::create(firstChild, node->getOpCodeValue(), 2);
            newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
            newFirstChild->setAndIncChild(1, secondChild);

            node->setAndIncChild(0, newFirstChild);
            node->setAndIncChild(1, firstChild->getSecondChild());
            TR::Node::recreate(node, subOps[node->getDataType()]);

            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            setExprInvariant(region, newFirstChild);
            }
         }

      // R9_1_3:
      //         +                +               +               -
      //       /   \            /   \           /   \           /   \
      //      +     +     =>   +     +         -     -    =>   +     +
      //     / \   / \        / \   / \       / \   / \       / \   / \
      //    t1  c1 t2 c2     t1 t2  c1 c2    t1 c1 t2 c2     t1 t2 c1 c2
      else if (!node->getOpCode().isRef() &&
               firstChild->getOpCodeValue() == secondChild->getOpCodeValue() &&
               (firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub()) &&
               !isExprInvariant(region, firstChild->getFirstChild()) &&
               !isExprInvariant(region, secondChild->getFirstChild()) &&
               isExprInvariant(region, firstChild->getSecondChild()) &&
               isExprInvariant(region, secondChild->getSecondChild()))
         {
         if (performTransformation(s->comp(), "%sApplied reassociation rule 9_1_3 to node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newFirstChild = TR::Node::create(firstChild, node->getOpCodeValue(), 2);
            newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
            newFirstChild->setAndIncChild(1, secondChild->getFirstChild());

            TR::Node * newSecondChild = TR::Node::create(secondChild, node->getOpCodeValue(), 2);
            newSecondChild->setAndIncChild(0, firstChild->getSecondChild());
            newSecondChild->setAndIncChild(1, secondChild->getSecondChild());

            TR::Node::recreate(node, firstChild->getOpCodeValue());
            node->setAndIncChild(0, newFirstChild);
            node->setAndIncChild(1, newSecondChild);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            setExprInvariant(region, newSecondChild);

            simplifyChildren(node, block, s);
            }
         }
      // R9_2:
      //        +              +
      //       / \            / \
      //      c1  -    =>    -   t
      //         / \        / \
      //        t   c2     c1 c2
      else if (node->getOpCode().isAdd() &&
               !node->getOpCode().isArrayRef() &&
               isExprInvariant(region, firstChild) &&
               secondChild->getOpCode().isSub() &&
               firstChild->getDataType() == secondChild->getSecondChild()->getDataType() &&
               !isExprInvariant(region, secondChild->getFirstChild()) &&
               isExprInvariant(region, secondChild->getSecondChild()))
         {
         if (performTransformation(s->comp(), "%sApplied reassociation rule 9_2 to node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newFirstChild = TR::Node::create(firstChild,
                                      subOps[firstChild->getDataType()], 2);
            newFirstChild->setAndIncChild(0, firstChild);
            newFirstChild->setAndIncChild(1, secondChild->getSecondChild());

            node->setAndIncChild(0, newFirstChild);
            node->setAndIncChild(1, secondChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            setExprInvariant(region, newFirstChild);
            }
         }

      // R9_3:
      //       aiadd          aiadd
      //       /  \            /  \
      //      c1  isub  =>  aiadd  t
      //          / \        / \
      //         t   c2     c1 ineg
      //                         \
      //                          c2
      else if (node->getOpCode().isArrayRef() &&
               isExprInvariant(region, firstChild) &&
               secondChild->getOpCode().isSub() &&
               !secondChild->getFirstChild()->getOpCode().isRef() && // can't move ref to the RHS

          (!isExprInvariant(region, secondChild->getFirstChild()) ||
                isSmallConstant(secondChild->getFirstChild(), s)) && // better address expression

               isExprInvariant(region, secondChild->getSecondChild()) &&
               !isSmallConstant(secondChild->getSecondChild(), s))
         {
         if (performTransformation(s->comp(), "%sApplied reassociation rule 9_3 to node 0x%p\n", s->optDetailString(), node))
            {
            TR::Node * newFirstChild = TR::Node::create(firstChild, node->getOpCodeValue(), 2);
            TR::Node * newNeg = TR::Node::create(secondChild->getSecondChild(),
                               negOps[secondChild->getSecondChild()->getDataType()], 1);
            TR::Node * zero = TR::Node::create(secondChild->getSecondChild(),
                              constOps[secondChild->getSecondChild()->getDataType()], 0);
            zero->set64bitIntegralValue(0);

            newNeg->setAndIncChild(0, secondChild->getSecondChild());
            newNeg = s->simplify(newNeg, block);

            newFirstChild->setAndIncChild(0, firstChild);
            newFirstChild->setAndIncChild(1, newNeg);
            newFirstChild->setIsInternalPointer(node->isInternalPointer());

            node->setAndIncChild(0, newFirstChild);
            node->setAndIncChild(1, secondChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            setExprInvariant(region, newFirstChild);
            }
         }
      }

   reassociateBigConstants(node, s);
   return node;
   }

template <class T>
static TR::Node *addSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::iuaddc)
      return node;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      T value = firstChild->getConst<T>() + secondChild->getConst<T>();
      foldConstant<T>(node, value, s, false /* !anchorChildren*/);

      if (node->getOpCode().isUnsigned())
         {
         if (node->nodeRequiresConditionCodes())
            setCCAddUnsigned<T>(value, firstChild->getConst<T>(), node, s);
         }
      else
         {
         if (node->nodeRequiresConditionCodes())
            setCCAddSigned<T>(value, firstChild->getConst<T>(), secondChild->getConst<T>(), node, s);
         }

      return node;
      }

   if (!node->getOpCode().isRef())
      {
      orderChildren(node, firstChild, secondChild, s);
      }

   if (node->nodeRequiresConditionCodes())
      {
      return node;
      }

   TR::Node *identity  = 0;
   if (0 != (identity = binaryIdentityOp<T>(node, 0, s)))
      return identity;

   TR::ILOpCode firstChildOp  = firstChild->getOpCode();
   TR::ILOpCode secondChildOp = secondChild->getOpCode();
   TR::ILOpCode nodeOp        = node->getOpCode();

   TR::Node * iMulComposerChild = NULL;
   T iMulComposerValue = 0;
   // normalize adds of positive constants to be isubs.  Have to do it this way because
   // more negative value than positive in a 2's complement signed representation
   if (nodeOp.isAdd()          &&
       secondChildOp.isLoadConst() &&
       secondChild->getConst<T>() > 0)
      {
      if (performTransformation(s->comp(), "%sNormalized xadd of xconst > 0 in node [%s] to xsub of -xconst\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node::recreate(node, TR::ILOpCode::getSubOpCode<T>());
         if (secondChild->getReferenceCount() == 1)
            {
            secondChild->setConst<T>(-secondChild->getConst<T>());
            }
         else
            {
            TR::Node * newSecondChild = TR::Node::create(secondChild, TR::ILOpCode::getConstOpCode<T>(), 0);
            newSecondChild->setConst<T>(-secondChild->getConst<T>());
            node->setAndIncChild(1, newSecondChild);
            secondChild->recursivelyDecReferenceCount();
            }
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   else if (nodeOp.isAdd() && firstChildOp.isNeg())
      {
      TR::Node * newFirstChild = firstChild->getFirstChild();
      // turn ineg +(-1) into bitwise complement
      if (TR::DataType::isSignedInt32<T>() && // this code has not been ported to templates yet
          secondChildOp.isLoadConst() &&
          secondChild->getConst<T>() == -1)
         {
         if (performTransformation(s->comp(), "%sReduced xadd of -1 and an xneg in node [%s] to bitwise complement\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            s->anchorChildren(node, s->_curTree);
            TR::Node::recreate(node, TR::ixor);
            node->setAndIncChild(0, newFirstChild);
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            node = s->simplify(node, block);
            s->_alteredBlock = true;
            }
         }
      // Turn add with first child negated into a subtract
      else if (performTransformation(s->comp(), "%sReduced iadd with negated first child in node [%s] to isub\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         s->anchorChildren(node, s->_curTree);
         TR::Node::recreate(node, TR::ILOpCode::getSubOpCode<T>());
         node->setAndIncChild(1, newFirstChild);
         node->setChild(0, secondChild);
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         node = s->simplify(node, block);
         s->_alteredBlock = true;
         }
      }
   // turn add with other child negated in to a subtract
   else if (nodeOp.isAdd() && secondChildOp.isNeg())
      {
      if (performTransformation(s->comp(), "%sReduced iadd with negated second child in node [%s] to isub\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         s->anchorChildren(node, s->_curTree);
         TR::Node * newSecondChild = secondChild->getFirstChild();
         TR::Node::recreate(node, TR::ILOpCode::getSubOpCode<T>());
         node->setAndIncChild(1, newSecondChild);
         secondChild->recursivelyDecReferenceCount();
         s->_alteredBlock = true;
         node->setVisitCount(0);
         node = s->simplify(node, block);
         }
      }
   else if (TR::DataType::isSignedInt32<T>() && // this code has not been ported to templates yet
            imulComposer(node, (int*)&iMulComposerValue, &iMulComposerChild ))
      {
      bool iMulDecomposeReport = s->comp()->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition);
      if (iMulDecomposeReport)
         dumpOptDetails(s->comp(), "\nImul composition succeeded for a value of %d.\n ", iMulComposerValue);

      if (s->getLastRun() && s->comp()->cg()->codegenMulDecomposition((int64_t)iMulComposerValue)
          && performTransformation(s->comp(), "%sFactored iadd with distributed imul with a codegen decomposible constant in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * lChild = node->getFirstChild();
         TR::Node * rChild = node->getSecondChild();
         TR::Node * constNode = NULL;

         if (iMulDecomposeReport)
            dumpOptDetails(s->comp(), "Putting the node back to imul with %d, for node [%s]. \n", iMulComposerValue, iMulComposerChild->getName(s->getDebug()));
         TR::Node::recreate(node, TR::ILOpCode::getMulOpCode<T>());

         node->setAndIncChild(0, iMulComposerChild);
         constNode = TR::Node::create(node, TR::ILOpCode::getConstOpCode<T>(), 0, iMulComposerValue);
         node->setAndIncChild(1, constNode);

         lChild->recursivelyDecReferenceCount();
         rChild->recursivelyDecReferenceCount();
         }
      }
      // reduce imul operations by factoring
   else if (firstChildOp.isMul() &&
            firstChild->getReferenceCount() == 1 &&
            secondChildOp.isMul()             &&
            secondChild->getReferenceCount() == 1)
      {
      TR::Node * llChild     = firstChild->getFirstChild();
      TR::Node * lrChild     = firstChild->getSecondChild();
      TR::Node * rlChild     = secondChild->getFirstChild();
      TR::Node * rrChild     = secondChild->getSecondChild();
      TR::Node * factorChild = NULL;
      // moved this check down:
      // if (performTransformation(s->comp(), "%sFactored iadd with distributed imul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         if (llChild == rlChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            }
         else if (llChild == rrChild)
            {
            factorChild = llChild;
            secondChild->setChild(1, lrChild);
            }
         else if (lrChild == rlChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            }
         else if (lrChild == rrChild)
            {
            factorChild = lrChild;
            secondChild->setChild(1, llChild);
            }

         // if invariance info available, do not group invariant
         // and non-invariant terms together
         // also, avoid conflict with Rule 13
         TR_RegionStructure * region;
         if (factorChild &&
             !s->getLastRun()  &&
             (region = s->containingStructure()) &&
             (isExprInvariant(region, secondChild->getFirstChild()) !=
              isExprInvariant(region, secondChild->getSecondChild()) ||
              isExprInvariant(region, factorChild)))  // to avoid conflict with Rule 13
            {
            factorChild = NULL;
            }

         if (!factorChild ||
             !performTransformation(s->comp(), "%sFactored iadd with distributed imul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            secondChild->setChild(0, rlChild);
            secondChild->setChild(1, rrChild);
            }
         else
            {
            TR::Node::recreate(node, TR::ILOpCode::getMulOpCode<T>());
            node->setChild(0, factorChild)->decReferenceCount();
            TR::Node::recreate(secondChild, TR::ILOpCode::getAddOpCode<T>());
            firstChild->decReferenceCount();
            secondChild->setVisitCount(0);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node->setChild(1, s->simplify(secondChild, block));
            }
         }
      }
   else if (nodeOp.getOpCodeValue() != TR::aiadd &&
            !((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
               (s->_curTree->getNode()->getNumChildren() > 0) &&
          s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
         (s->_curTree->getNode()->getFirstChild()->getLastChild() == node)) &&
            (firstChildOp.isAdd() ||
             firstChildOp.isSub()))
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCode().isLoadConst())
         {
         if (secondChildOp.isLoadConst())
            {
            if (!s->reassociate() && // use new rules
               performTransformation(s->comp(), "%sFound xadd of xconst with xadd or xsub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               if (firstChild->getReferenceCount()>1)
                  {
                  // it makes sense to uncommon it in this situation
                  TR::Node * newFirstChild = TR::Node::create(node, firstChildOp.getOpCodeValue(), 2);
                  newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
                  newFirstChild->setAndIncChild(1, firstChild->getSecondChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(0,newFirstChild);
                  firstChild = newFirstChild;
                  }

               T value  = secondChild->getConst<T>();
               if (firstChildOp.isAdd())
                  {
                  value += lrChild->getConst<T>();
                  }
               else
                  {
                  value -= lrChild->getConst<T>();
                  }
               if (value > 0)
                  {
                  value = -value;
                  TR::Node::recreate(node, TR::ILOpCode::getSubOpCode<T>());
                  }
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setConst<T>(value);
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::ILOpCode::getConstOpCode<T>(), 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setConst<T>(value);
                  secondChild->recursivelyDecReferenceCount();
                  }

               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         else if (!s->reassociate() && // use new rules
                  (firstChild->getReferenceCount() == 1) &&
                  performTransformation(s->comp(), "%sFound xadd of non-xconst with xadd or isub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            // move constants up the tree so they will tend to get merged together
            node->setChild(1, lrChild);
            firstChild->setChild(1, secondChild);
            TR::Node::recreate(node, firstChildOp.getOpCodeValue());
            TR::Node::recreate(firstChild, TR::ILOpCode::getAddOpCode<T>());
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   else if (nodeOp.getOpCodeValue() == TR::aiadd         &&
            firstChildOp.getOpCodeValue() == TR::aiadd   &&
            secondChildOp.isLoadConst())
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if ((lrChild->getOpCode().isLoadConst()) &&
          ((firstChild->getFutureUseCount() == 0) ||
           (!s->comp()->cg()->areAssignableGPRsScarce() &&
            (lrChild->getConst<T>() == -1*secondChild->getConst<T>()))))
         {
         if (performTransformation(s->comp(), "%sFound aiadd of iconst with aiadd x and iconst in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            T value  = secondChild->getConst<T>();
            value += lrChild->getConst<T>();
            if (secondChild->getReferenceCount() == 1)
               {
               secondChild->setConst<T>(value);
               }
            else
               {
               TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::ILOpCode::getConstOpCode<T>(), 0);
               node->setAndIncChild(1, foldedConstChild);
               foldedConstChild->setConst<T>(value);
               secondChild->recursivelyDecReferenceCount();
               }
            node->setAndIncChild(0, firstChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   return addSimplifierCommon(node, block, s);
   }

template <class T>
TR::Node *subSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   scount_t futureUseCountOfFirstChild = node->getFirstChild()->getFutureUseCount();

   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::iusubb)
      return node;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      T value = firstChild->getConst<T>() - secondChild->getConst<T>();
      foldConstant<T>(node, value, s, false /* !anchorChilren */);

      if (node->getOpCode().isUnsigned())
         {
         if (node->nodeRequiresConditionCodes())
            setCCSubUnsigned<T>(value, firstChild->getConst<T>(), node, s);
         }
      else
         {
         if (node->nodeRequiresConditionCodes())
            setCCSubSigned<T>(value, firstChild->getConst<T>(), secondChild->getConst<T>(), node, s);
         }
      return node;
      }

   if (!node->nodeRequiresConditionCodes()) // don't do identity op on cc nodes on zemulator
      {
      TR::Node *identity  = 0;
      if (0 != (identity = binaryIdentityOp<T>(node, 0, s)))
         return identity;
      }

   TR::ILOpCode secondChildOp = secondChild->getOpCode();
   TR::ILOpCode firstChildOp  = firstChild->getOpCode();

   TR::Node * MulComposerChild = NULL;
   T MulComposerValue = 0;

   // first check case where both nodes are the same and replace with constant 0
   if (firstChild == secondChild)
      {
      if (node->nodeRequiresConditionCodes())
         {
         setCCSubSigned<T>(0, firstChild->getConst<T>(), secondChild->getConst<T>(), node, s);
         }
      foldConstant<T>(node, 0, s, true /* !anchorChilren */);

      return node;
      }

    if (node->nodeRequiresConditionCodes())
       {
       return node;
       }

    if (node->getOpCodeValue() != TR::aiadd &&
        !((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
          (s->_curTree->getNode()->getNumChildren() > 0) &&
      s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
     (s->_curTree->getNode()->getFirstChild()->getLastChild() == node)) &&
            ((node->getOpCode().isSub()) &&
             ((secondChildOp.isAdd()) || (secondChildOp.isSub()))))
      {
      TR::Node * rlChild = secondChild->getFirstChild();
      TR::Node * rrChild = secondChild->getSecondChild();

      if ((rrChild->getOpCode().isLoadConst()) &&
          (rlChild == firstChild) &&
          node->cannotOverflow() && secondChild->cannotOverflow())
         {
         if (performTransformation(s->comp(), "%sFolded isub with children related through iconst in node [%s] to iconst \n", s->optDetailString(), node->getName(s->getDebug())))
            {
            node->setChild(0, 0);
            node->setChild(1, 0);

            TR::Node::recreate(node, TR::ILOpCode::getConstOpCode<T>());
            if (secondChildOp.isAdd())
               node->setConst<T>(-1*rrChild->getConst<T>());
            else
               node->setConst<T>(rrChild->getConst<T>());
            node->setNumChildren(0);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   // normalize subtracts of positive constants to be iadds.  Have to do it this way because
   // more negative value than positive in a 2's complement signed representation
   else if (secondChildOp.isLoadConst() &&
            secondChild->getConst<T>() > 0)
      {
      if (performTransformation(s->comp(), "%sNormalized isub of iconst > 0 in node [%s] to iadd of -iconst \n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node::recreate(node, TR::ILOpCode::getAddOpCode<T>());
         if (secondChild->getReferenceCount() == 1)
            {
            secondChild->setConst<T>(-secondChild->getConst<T>());
            }
         else
            {
            TR::Node * newSecondChild = TR::Node::create(secondChild, TR::ILOpCode::getConstOpCode<T>(), 0);
            newSecondChild->setConst<T>(-secondChild->getConst<T>());
            node->setAndIncChild(1, newSecondChild);
            secondChild->recursivelyDecReferenceCount();
            }
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   // turn isub with second child ineg into an iadd
   else if (secondChildOp.isNeg())
      {
      if (performTransformation(s->comp(), "%sReduced xsub with negated second child in node [%s] to xadd\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * newSecondChild = secondChild->getFirstChild();
         TR::Node::recreate(node, TR::ILOpCode::getAddOpCode<T>());
         node->setAndIncChild(1, newSecondChild);
         secondChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
      // turn isub with first child ineg into an ineg with iadd child.  isn't quicker, but normalizes expressions and drives inegs up the tree
   else if (firstChildOp.isNeg())
      {
      if (performTransformation(s->comp(), "%sReduced xsub with negated first child in node [%s] to ineg of xadd\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * newFirstChild = firstChild->getFirstChild();
         TR::Node::recreate(node, TR::ILOpCode::getNegOpCode<T>());
         TR::Node * newNode = TR::Node::create(node, TR::ILOpCode::getAddOpCode<T>(), 2);
         newNode->setAndIncChild(0, newFirstChild);
         newNode->setChild(1, secondChild);
         node->setChild(1, NULL);
         node->setAndIncChild(0, newNode);
         node->setNumChildren(1);
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   // turn bitwise complement + 1 into a 2's complement negate
    else if (isBitwiseIntComplement(firstChild) &&
            secondChildOp.isLoadConst()         &&
            secondChild->getConst<T>() == -1)
      {
      if (performTransformation(s->comp(), "%sReduced isub of bitwise complement and iconst -1 in node [%s] to 2s complement negation\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * firstGrandChild = firstChild->getFirstChild();
         TR::Node::recreate(node, TR::ILOpCode::getNegOpCode<T>());
         node->setAndIncChild(0, firstGrandChild);
         node->setNumChildren(1);
         secondChild->recursivelyDecReferenceCount();
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   else if (TR::DataType::isSignedInt32<T>() && // this code has not been ported to templates yet
            imulComposer(node, (int*)&MulComposerValue, &MulComposerChild ))  // TODO
      {
      bool iMulDecomposeReport = s->comp()->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition);
      if (iMulDecomposeReport)
         dumpOptDetails(s->comp(), "\nImul composition succeeded for a value of %d.\n ", MulComposerValue);

      if (s->getLastRun() && s->comp()->cg()->codegenMulDecomposition((int64_t)MulComposerValue)
          && performTransformation(s->comp(), "%sFactored iadd with distributed imul with a codegen decomposible constant in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * lChild = node->getFirstChild();
         TR::Node * rChild = node->getSecondChild();
         TR::Node * constNode = NULL;

         if (iMulDecomposeReport)
            dumpOptDetails(s->comp(), "Putting the node back to imul with %d, for node [%s]. \n", MulComposerValue, MulComposerChild->getName(s->getDebug()));
         TR::Node::recreate(node, TR::ILOpCode::getMulOpCode<T>());

         node->setAndIncChild(0, MulComposerChild);
         constNode = TR::Node::create(node, TR::ILOpCode::getConstOpCode<T>(), 0, MulComposerValue);
         node->setAndIncChild(1, constNode);

         lChild->recursivelyDecReferenceCount();
         rChild->recursivelyDecReferenceCount();
         }

      }
   // reduce imul operations by factoring
   else if (firstChildOp.isMul()              &&
            firstChild->getReferenceCount() == 1 &&
            secondChildOp.isMul()             &&
            secondChild->getReferenceCount() == 1)
      {
      TR::Node * llChild     = firstChild->getFirstChild();
      TR::Node * lrChild     = firstChild->getSecondChild();
      TR::Node * rlChild     = secondChild->getFirstChild();
      TR::Node * rrChild     = secondChild->getSecondChild();
      TR::Node * factorChild = NULL;
      // moved this check down:
      // if (performTransformation(s->comp(), "%sFactored isub with distributed imul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         if (llChild == rlChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            }
         else if (llChild == rrChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            secondChild->setChild(1, rlChild);
            }
         else if (lrChild == rlChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            }
         else if (lrChild == rrChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            secondChild->setChild(1, rlChild);
            }

         // if invariance info available, do not group invariant
         // and non-invariant terms together
         TR_RegionStructure * region;
         if (factorChild && (region = s->containingStructure()) &&
             !s->getLastRun()  &&
             (isExprInvariant(region, secondChild->getFirstChild()) !=
              isExprInvariant(region, secondChild->getSecondChild()) ||
              isExprInvariant(region, factorChild)))  // to avoid conflict with rule 13
            {
            factorChild = NULL;
            }

         if (!factorChild ||
             !performTransformation(s->comp(), "%sFactored xsub with distributed xmul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            secondChild->setChild(0, rlChild);
            secondChild->setChild(1, rrChild);
            }
         else
            {
            TR::Node::recreate(node, TR::ILOpCode::getMulOpCode<T>());
            node->setChild(factorChild->getOpCode().isLoadConst() ? 1:0, factorChild)->decReferenceCount();
            TR::Node::recreate(secondChild, TR::ILOpCode::getSubOpCode<T>());
            firstChild->decReferenceCount();
            secondChild->setVisitCount(0);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node->setChild(factorChild->getOpCode().isLoadConst() ? 0:1, s->simplify(secondChild, block));
            }
         }
      }

   // 78418 - re-enable this reduction. Comments from 70791 retained for
   //  reference. The problems that were fixed with 70791 will now be fixed
   //  with store propagation for 5.0...
   // Original 70791 comments:
   //   70791 the firstChild->getReferenceCount()==1 test we all believe should
   //   really be on an inner if (page down a couple times), but LoopAtom gets
   //   a lot slower for register usage problems.  when we have a solution for
   //   that problem, the test should be moved back down there.
   else if (!((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
          (s->_curTree->getNode()->getNumChildren() > 0) &&
      s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
     (s->_curTree->getNode()->getFirstChild()->getLastChild() == node)) &&
           (firstChildOp.isAdd() || firstChildOp.isSub()))
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCode().isLoadConst())
         {
         if (secondChildOp.isLoadConst())
            {
            if (performTransformation(s->comp(), "%sFound xsub of xconst with xadd or xsub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               if (firstChild->getReferenceCount()>1)
                  {
                  // it makes sense to uncommon it in this situation
                  TR::Node * newFirstChild = TR::Node::create(node, firstChildOp.getOpCodeValue(), 2);
                  newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
                  newFirstChild->setAndIncChild(1, firstChild->getSecondChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(0,newFirstChild);
                  firstChild = newFirstChild;
                  }

               T value = - secondChild->getConst<T>();
               if (firstChildOp.isAdd())
                  {
                  value += lrChild->getConst<T>();
                  }
               else // firstChildOp == TR::isub
                  {
                  value -= lrChild->getConst<T>();
                  }
               if (value > 0)
                  {
                  value = -value;
                  }
               else
                  {
                  TR::Node::recreate(node, TR::ILOpCode::getAddOpCode<T>());
                  }
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setConst<T>(value);
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::ILOpCode::getConstOpCode<T>(), 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setConst<T>(value);
                  secondChild->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         else // Original 70791 comments:
              //   moving this test to the outer if to preserve
              //   performance in LoopAtom.  we all still believe the test
              //   should really be here, but a heavier solution is needed
              //   to fix register pressure problems thta result
            if ((firstChild->getReferenceCount() == 1) && (performTransformation(s->comp(), "%sFound xsub of non-xconst with xadd or xsub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug()))))
            {
            // move constants up the tree so they will tend to get merged together
            node->setChild(1, lrChild);
            firstChild->setChild(1, secondChild);
            TR::Node::recreate(node, firstChildOp.getOpCodeValue());
            TR::Node::recreate(firstChild, TR::ILOpCode::getSubOpCode<T>());
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   else if (firstChild->getOpCode().isLoadConst() &&
            firstChild->getConst<T>() == 0 &&
            performTransformation(s->comp(), "%sReduce xsub from 0 [%s] to xneg \n",s->optDetailString(),node->getName(s->getDebug())))
      {
      TR::Node::recreate(node, TR::ILOpCode::negateOpCode(node->getDataType()));
      node->setVisitCount(0);
      node->getFirstChild()->recursivelyDecReferenceCount();
      node->setChild(0, node->getSecondChild());
      node->setNumChildren(1);
      s->_alteredBlock = true;
      return node;
      }

  if ((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
       (s->_curTree->getNode()->getNumChildren() > 0) &&
        s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
      (s->_curTree->getNode()->getFirstChild()->getLastChild() == node))
     return node;

   reassociateBigConstants(node, s);

   return node;
   }

// normalize shift constant so that we can common, fold and strength reduce more reliably
static void normalizeConstantShiftAmount(TR::Node * node, int32_t shiftMask, TR::Node * & secondChild, TR::Simplifier * s)
   {
   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t shiftValue = secondChild->getInt() & (shiftMask);
      if (shiftValue != secondChild->getInt())
         {
         if (performTransformation(s->comp(), "%sReducing constant of node [%s] from %d to %d\n", s->optDetailString(), secondChild->getName(s->getDebug()), secondChild->getInt(), shiftValue))
            {
            if (secondChild->getReferenceCount() > 1)
               {
               secondChild->decReferenceCount();
               TR::Node * newChild = TR::Node::create(secondChild, TR::iconst, 0);
               node->setAndIncChild(1, newChild);
               secondChild = newChild;
               }
            secondChild->setInt(shiftValue);
            s->_alteredBlock = true;
            }
         }
      }
   }

static void normalizeShiftAmount(TR::Node * node, int32_t normalizationConstant, TR::Simplifier * s)
   {
   if (s->comp()->cg()->needsNormalizationBeforeShifts() &&
       !node->isNormalizedShift())
      {
      TR::Node * secondChild = node->getSecondChild();
      //
      // Some platforms like IA32 obey Java semantics for shifts even if the
      // shift amount is greater than 31. However other platforms like PPC need
      // to normalize the shift amount to range (0, 31) before shifting in order
      // to obey Java semantics. This can be captured in the IL and commoning/hoisting
      // can be done (look at Compressor.compress).
      //
      if ((secondChild->getOpCodeValue() != TR::iconst) &&
          ((secondChild->getOpCodeValue() != TR::iand) ||
           (secondChild->getSecondChild()->getOpCodeValue() != TR::iconst) ||
           (secondChild->getSecondChild()->getInt() != normalizationConstant)))
         {
         if (performTransformation(s->comp(), "%sPlatform specific normalization of shift node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            //
            // Not normalized yet
            //
            TR::Node * secondChild = node->getSecondChild();
            TR::Node * normalizedNode = TR::Node::create(TR::iand, 2, secondChild, TR::Node::create(secondChild, TR::iconst, 0, normalizationConstant));
            secondChild->recursivelyDecReferenceCount();
            node->setAndIncChild(1, normalizedNode);
            node->setNormalizedShift(true);
            s->_alteredBlock = true;
            }
         }
      }
   }

static void orderChildrenByHighWordZero(TR::Node * node, TR::Node * & firstChild, TR::Node * & secondChild, TR::Simplifier * s)
   {
   if (!secondChild->getOpCode().isLoadConst() &&
       secondChild->isHighWordZero() &&
       !firstChild->isHighWordZero())
      {
      swapChildren(node, firstChild, secondChild, s);
      }
   }

static TR::Node *foldDemotionConversion(TR::Node * node, TR::ILOpCodes opcode, TR::ILOpCodes foldedOpCode, TR::Simplifier * s)
   {
   TR::Node * firstChild = node->getFirstChild();

   if (!s->isLegalToFold(node, firstChild))
      return NULL;

   if (firstChild->getOpCodeValue() == opcode &&
       performTransformation(s->comp(), "%sFolding conversion node [%s] %s and its child [%s] %s\n",
                              s->optDetailString(), node->getName(s->getDebug()), node->getOpCode().getName(),
                              firstChild->getName(s->getDebug()), firstChild->getOpCode().getName()))
      {
      TR::Node::recreate(node, foldedOpCode);
      node->setAndIncChild(0, firstChild->getFirstChild());
      s->prepareToStopUsingNode(firstChild, s->_curTree);
      firstChild->recursivelyDecReferenceCount();
      return node;
      }

   return NULL;
   }

//---------------------------------------------------------------------
// Reduce long operations to int operations if the result is going to be converted
// back to an int result.
//
static bool reduceLongOp(TR::Node * node, TR::Block * block, TR::Simplifier * s, TR::ILOpCodes newConversionOp)
   {
   // If the child is referenced elsewhere we can't reduce it.
   //
   TR::Node * firstChild = node->getFirstChild();
   if (firstChild->getReferenceCount() != 1)
      return false;

   bool isUnsigned = false;
   // See if the long operation is one that can be reduced
   //
   TR::ILOpCodes newOp = TR::BadILOp;
   bool convertSecondChild = true;
   switch (firstChild->getOpCodeValue())
      {
      case TR::ladd: newOp = TR::iadd; break;
      case TR::lsub: newOp = TR::isub; break;
      case TR::lmul: newOp = TR::imul; break;
      case TR::land: newOp = TR::iand; break;
      case TR::lor:  newOp = TR::ior;  break;
      case TR::lxor: newOp = TR::ixor; break;
      case TR::luadd: newOp = TR::iuadd; isUnsigned = true; break;
      case TR::lusub: newOp = TR::iusub; isUnsigned = true; break;
      case TR::lushr:
         isUnsigned = true;
      case TR::lshr:
         if (node->getOpCodeValue() == TR::l2s &&
             firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
            if ((firstChild->getSecondChild()->get64bitIntegralValue() & LONG_SHIFT_MASK) <= 16)
               {
               newOp = isUnsigned ? TR::iushr : TR::ishr;
               convertSecondChild = false; // the second child is already an iconst
               }
            }
         break;
      case TR::lushl:
    isUnsigned = true;
      case TR::lshl:
         if (firstChild->getSecondChild()->getOpCode().isLoadConst())
            {
            if ((firstChild->getSecondChild()->get64bitIntegralValue() & LONG_SHIFT_MASK) < 32)
               {
               newOp = isUnsigned ? TR::iushl : TR::ishl;
               convertSecondChild = false; // the second child is already an iconst
               }
            else
               {
               if (!performTransformation(s->comp(), "%sReducing long operation in node [" POINTER_PRINTF_FORMAT "] to an int operation\n", s->optDetailString(), node))
                  return false;
               // shift by > 31 bits gets constant 0
               if (newConversionOp == TR::BadILOp)
                  {
                  TR::Node::recreate(node, isUnsigned ? TR::iuconst : TR::iconst);
                  firstChild->recursivelyDecReferenceCount();
                  node->setNumChildren(0);
                  node->setChild(0, 0);
                  node->setInt(0);
                  }
               else
                  {
                  TR::Node::recreate(firstChild, isUnsigned ? TR::iuconst : TR::iconst);
                  firstChild->getFirstChild()->recursivelyDecReferenceCount();
                  firstChild->getSecondChild()->recursivelyDecReferenceCount();
                  firstChild->setInt(0);
                  firstChild->setNumChildren(0);
                  firstChild->setChild(0, 0);
                  firstChild->setChild(1, 0);
                  TR::Node::recreate(node, newConversionOp);
                  }
               s->_alteredBlock = true;
               simplifyChildren(node, block, s);
               return true;
               }
            }
      break;
      case TR::lneg:
      case TR::luneg:
         {
         if (!performTransformation(s->comp(), "%sReducing long operation in node [" POINTER_PRINTF_FORMAT "] to an int operation\n", s->optDetailString(), node))
            return false;
         if (newConversionOp == TR::BadILOp)
            {
            TR::Node::recreate(node, isUnsigned ? TR::iuneg : TR::ineg);
            TR::Node::recreate(firstChild, TR::l2i);
            }
         else
            {
            TR::Node * temp = TR::Node::create(TR::l2i, 1, firstChild->getFirstChild());
            firstChild->getFirstChild()->decReferenceCount();
            TR::Node::recreate(firstChild, isUnsigned ? TR::iuneg : TR::ineg);
            firstChild->setAndIncChild(0, temp);
            TR::Node::recreate(node, newConversionOp);
            }
         s->_alteredBlock = true;
         simplifyChildren(node, block, s);
         return true;
         }
      break;
      default:
      break;
      }
   if (newOp == TR::BadILOp)
      return false;

   if (!performTransformation(s->comp(), "%sReducing long operation in node [" POINTER_PRINTF_FORMAT "] to an int operation\n", s->optDetailString(), node))
      return false;

   // If the original node was l2i the reduced long operation replaces it.
   // Otherwise the l2x is replaced by i2x and the long operation made into an
   // int operation.
   //
   if (newConversionOp == TR::BadILOp)
      {
      TR::Node::recreate(node, newOp);
      node->setNumChildren(2);
      node->setAndIncChild(1, convertSecondChild ?
            TR::Node::create(TR::l2i, 1, firstChild->getSecondChild()) :
            firstChild->getSecondChild());
      firstChild->getSecondChild()->decReferenceCount();
      TR::Node::recreate(firstChild, TR::l2i);
      firstChild->setNumChildren(1);
      firstChild->setChild(1, 0);
      firstChild->setIsNonNegative(false);
      }
   else
      {
      TR::Node::recreate(node, newConversionOp);
      TR::Node::recreate(firstChild, newOp);
      TR::Node * firstFirst  = firstChild->getFirstChild();
      TR::Node * firstSecond = firstChild->getSecondChild();
      TR::Node * temp0 = TR::Node::create(TR::l2i, 1, firstFirst);
      TR::Node * temp1 = convertSecondChild ?
         TR::Node::create(TR::l2i, 1, firstSecond) :
         firstSecond;
      firstChild->setAndIncChild(0, temp0);
      firstChild->setAndIncChild(1, temp1);
      firstFirst->decReferenceCount();
      firstSecond->decReferenceCount();
      }

   s->_alteredBlock = true;
   simplifyChildren(node, block, s);
   return true;
   }

static void fold2SmallerIntConstant(TR::Node * node, TR::Node *child, TR::DataType sdt, TR::DataType tdt, TR::Simplifier * s)
   {
   int32_t val;
   switch (sdt)
      {
      case TR::Int16:
         val = (int32_t)child->getShortInt();
         break;
      case TR::Int32:
         val = (int32_t)child->getInt();
         break;
      case TR::Int64:
         val = (int32_t)child->getLongInt();
         break;
      default:
         TR_ASSERT(0, "invalid source for fold");
      }
   switch (tdt)
      {
      case TR::Int8:
         foldByteConstant(node, (val<<24)>>24, s, false /* !anchorChildren */);
         break;
      case TR::Int16:
         foldShortIntConstant(node, (val<<16)>>16, s, false /* !anchorChildren */);
         break;
      case TR::Int32:
         foldIntConstant(node, val, s, false /* !anchorChildren */);
         break;
      default:
         TR_ASSERT(0, "invalid target for fold");
      }
   }

static TR::Node *intDemoteSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::ILOpCode op = node->getOpCode();

   TR::DataType sourceDataType(TR::DataTypes::NoType), targetDataType(TR::DataTypes::NoType);

   if (!decodeConversionOpcode(op, node->getDataType(), sourceDataType, targetDataType))
      return node;

   TR::ILOpCodes inverseOp1 = TR::ILOpCode::getProperConversion(targetDataType, sourceDataType, false); // sign extend
   TR::ILOpCodes inverseOp2 = TR::ILOpCode::getProperConversion(targetDataType, sourceDataType, true ); // zero extend
   uint32_t sourceSize = TR::ILOpCode::getSize(inverseOp1);
   uint32_t targetSize = op.getSize();
   bool sourceIs64Bit = sourceDataType == TR::Int64;

   TR::Node * firstChild = node->getFirstChild();

   // Constant transformation (handles signedness)

   if (firstChild->getOpCode().isLoadConst())
      {
      fold2SmallerIntConstant(node, firstChild, sourceDataType, targetDataType, s);
      return node;
      }

   TR::Node * result;

   // Cancel transformation (Tw=type w)
   //   Tx 2 Ty                                 Example:    TR::l2b
   //     |           ==>    subtree                          |          ==> subtree
   //   Ty 2 Tx                                             TR::bu2lu
   //     |                                                   |
   //   subtree                                             subtree

   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, inverseOp1)))
      return result;

   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, inverseOp2)))
      return result;

   // Long Conversion transformation (Tw=type w)
   //   Tx 2 Ty                                 Example:    TR::l2b
   //     |           ==>    Tz 2 Ty                           |      ==>  TR::s2bu
   //   Tz 2 Tx                                             TR::s2l
   //
   //  where
   //     i)  Tx is 64 bit type
   //     ii) Tz is integral
   //    iii) Tx 2 Tz would be a demotion (and not promotion) which is checked for
   //
   //  Tz = childSourceDataType
   // TODO: do we really need Tx to be 64 bit?

   TR::ILOpCode childOp = firstChild->getOpCode();
   TR::DataType childSourceDataType(TR::DataTypes::NoType), childTargetDataType(TR::DataTypes::NoType);

   if (sourceIs64Bit && decodeConversionOpcode(childOp, firstChild->getDataType(), childSourceDataType, childTargetDataType) && (childSourceDataType != targetDataType))
      {
      bool wantZeroExtension = childOp.isZeroExtension();
      uint32_t childSourceSize = TR::ILOpCode::getSize(TR::ILOpCode::getProperConversion(childTargetDataType, childSourceDataType, wantZeroExtension));  // size of childSourceDataType (hack)
      if (childSourceDataType.isIntegral() && (childSourceSize < sourceSize))
         {
         bool wantZeroExtension = childOp.isZeroExtension();
         TR::ILOpCodes foldOp =  TR::ILOpCode::getProperConversion(childSourceDataType, targetDataType, wantZeroExtension);
         result = foldDemotionConversion(node, childOp.getOpCodeValue(), foldOp, s);
         if (result)
            return result;
         }
      }

   // Redundant "and" tranformation
   //    Tx 2 Ty                                                     Tx 2 Ty
   //       |         where                                             |
   //     Tx and   "Tx constant" & 0xF..F == "Tx constant"    ==>    subtree
   //      /   \
   // subtree  Tx constant
   //
   //   TR::l2b
   //       |
   //   TR::land
   //    /     \
   // subtree  lconst 22
   //
   int64_t andVal;
   switch (targetSize)
      {
      case 1: andVal = 0xFF; break;
      case 2: andVal = 0xFFFF; break;
      case 4: andVal = 0xFFFFFFFF; break;
      default: TR_ASSERT(0, "invalid call to intDemotSimplifier");
      }
   TR::ILOpCodes andOp = TR::BadILOp, andConstOp = TR::BadILOp;
   switch (sourceDataType)
      {
        case TR::Int16: andOp = TR::sand;  andConstOp = TR::sconst;  break;
        case TR::Int32: andOp = TR::iand;  andConstOp = TR::iconst;  break;
        case TR::Int64: andOp = TR::land;  andConstOp = TR::lconst;  break;
        default: break;

      }

   if ((result = foldRedundantAND(node, andOp, andConstOp, andVal, s)))
         return result;

   // Long reduce transformation
   //       Tx 2 Ty      ==>   Tz 2 Ty      where Tz is next smaller type
   //         |                  |
   //       subtree-x          subtree-z
   if (sourceIs64Bit)
       {
       TR::ILOpCodes reduceOp;
       if (targetDataType == TR::Int32)
           {
            reduceOp = TR::BadILOp;
           }
       else
           {
           reduceOp = TR::ILOpCode::getProperConversion(TR::Int32, targetDataType, false); // appropriate integer to integer op
           }
       reduceLongOp(node, block, s, reduceOp);
       }

   return node;
   }

static bool hasCommonedChild(TR::Node *node, TR::SparseBitVector &isVisited)
   {
   TR::ILOpCode& op   = node->getOpCode();
   isVisited[node->getGlobalIndex()] = true;

   if (node->getReferenceCount() > 1)
      return true;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);

      if (!isVisited[child->getGlobalIndex()])
         {
         if (hasCommonedChild(child, isVisited))
            return true;;
         }
      }
   return false;
   }

static TR::SymbolReference *getUniqueSymRef(TR::Node *node, bool &isLegal , TR::SparseBitVector &isVisited)
   {
   TR::ILOpCode& op   = node->getOpCode();
   isVisited[node->getGlobalIndex()] = true;

   if (node->getReferenceCount() > 1)
      {
      isLegal = false;
      return NULL;
      }

   if (op.isLoadConst())
      return NULL;


   if (op.hasSymbolReference())
      {
      if (!node->getOpCode().isLoadVar() &&
          !node->getOpCode().isLoadAddr())
         {
         isLegal = false;
         return NULL;
         }

      //Must be loadvar or loadaddr
      return  node->getSymbolReference();
      }

   TR::SymbolReference *savedSymRef = NULL;
   TR::SymbolReference *symRef    = NULL;
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);

      if (!isVisited[child->getGlobalIndex()])
         {
         symRef = getUniqueSymRef(child, isLegal, isVisited);
         if (symRef != NULL)
            {
            if (savedSymRef != NULL && savedSymRef != symRef)
               isLegal = false;
            savedSymRef = symRef;
            }
         if (!isLegal) return NULL;
         }
      }
   return savedSymRef;
   }

// We handle two kind of cases here:
// 1. an ifcmp is comparing with a const (or variable a), it is the first real instruction in the block
//    the other operand is defined in another blocks, at least one of the def is const (or a),
//    We move this ifcmp to after that def, hopefully the ifcmp can be resolved at compile time
// 2. TODO: for cmp,
static void partialRedundantCompareElimination(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   //return;
   TR::Compilation *comp = s->comp();
   if (comp->getOption(TR_DisablePRBE)) return;

   //Don't do it after GRA
   if (comp->cg()->getGRACompleted())
      return;

#if defined(ENABLE_SPMD_SIMD)
   static const char *e = feGetEnv("TR_DisableSPMD");
   static int disableSPMD = e ? atoi(e) : false;

   if (!disableSPMD)
      {
      // PRBE prevents unrolling the kernel loop for SIMDization by confusing loop canonizalization
      // if the range is known at the compile time
      TR_ResolvedMethod *method;
      if (node->getInlinedSiteIndex() != -1)
         method = comp->getInlinedResolvedMethod(node->getInlinedSiteIndex());
      else
         method = comp->getCurrentMethod();

      if (method->getRecognizedMethod() == TR::com_ibm_simt_SPMDKernel_execute) return;
      }
#endif

   bool didSomething = false;
   TR::ILOpCode opcode = node->getOpCode();
   if (!opcode.isBranch() || !opcode.isBooleanCompare())
      return;

   //Is comapring with const and  is the first real treetop in the block?
   if (node->getSecondChild()->getOpCode().isLoadConst() &&
       block->getFirstRealTreeTop() == s->_curTree)
      {
      bool                isLegal       = true;
      TR::SparseBitVector isVisited(comp->allocator());
      TR::SymbolReference *symRef        = getUniqueSymRef(node->getFirstChild(), isLegal , isVisited);

      //If ifcmp has any child with referecounCount > 1, then not legal
      if (!isLegal || symRef == NULL) return;

      if (hasCommonedChild(node->getSecondChild(), isVisited)) return;

      TR::CFGEdgeList   &predecessors  = ((TR::CFGNode *)block)->getPredecessors();
      TR::CFG             *cfg           = comp->getFlowGraph();
      TR::TreeTop         *fallThroughTT = block->getExit()->getNextTreeTop();
      TR::Block           *fallThroughBB = fallThroughTT->getNode()->getBlock();
      bool                onlyOnePred   = (predecessors.size() == 1);

      if (!onlyOnePred)
         {
         //Is there store of const at the end of predecessor
         for (auto predEdge = predecessors.begin(); predEdge != predecessors.end();)
            {
            TR::Block   *predBlock = (*(predEdge++))->getFrom()->asBlock();
            if (predBlock == cfg->getStart()->asBlock()) continue;
            TR::TreeTop *gotoTree  = predBlock->getLastRealTreeTop();
            //Last node of this predecessor is a goto
            if (gotoTree)
               {
               bool predEndWithGoto = (gotoTree->getNode()->getOpCodeValue() == TR::Goto);
               bool isImmediatePred = (predBlock->getExit()->getNextTreeTop() == block->getEntry());
               //Don't need to do anything if only has on pred and it's immmediate pred
                 if  (predEndWithGoto || isImmediatePred)
                  {
                  TR::TreeTop         *storeTree   = predEndWithGoto ? gotoTree->getPrevTreeTop() : gotoTree;
                  TR::Node            *storeNode   = storeTree->getNode();
                  TR::SymbolReference *storeSymRef = storeNode->getOpCode().hasSymbolReference()?storeNode->getSymbolReference():NULL;
                  //Node before goto is a store of const
                  if (storeNode->getOpCode().isStoreDirect() &&
                      storeSymRef == symRef &&
                      storeNode->getFirstChild()->getOpCode().isLoadConst())
                     {
                     // change the following case
                     //
                     // store a
                     //    load const
                     // goto block_B
                     // BBend A
                     //   ...
                     // BBBegin B
                     //   ifcmpeq --> block_x
                     //   ...
                     // BBend B
                     // BBBegin C
                     // ...
                     //
                     // To:
                     //
                     // store a
                     //    load const
                     // BBend A
                     // BBBegin E
                     // ifcmpeq --> block_x
                     // BBend E
                     // BBBegin D
                     // goto block_C
                     // BBend D
                     //   ...
                     // BBBegin B
                     // ifcmpeq --> block_x
                     //   ...
                     // BBend B
                     // BBBegin C
                     // ...
                     //
                     //also consider the case of block_A is the immediate predecessor of block_B
                     //check if it is feeding the ifcmp
                     //if (num > comp->getOptions()->getLastIpaOptTransformationIndex()) return;
                     //num++;
                     if (performTransformation(comp, "%sDuplicating %p and insert it after %p\n", s->optDetailString(), node, storeNode))
                        {
                        TR::Block           *ifCmpBlock    = block;
                        TR_RegionStructure *parent        = predBlock->getCommonParentStructureIfExists(fallThroughBB, cfg);
                        TR::Block           *branchDestBB  = node->getBranchDestination()->getNode()->getBlock();

                        //Create new ifcmp block
                        TR::Block *newIfCmpBlock = TR::Block::createEmptyBlock(block->getEntry()->getNode(), comp, predBlock->getFrequency(), predBlock);

                        //Create new gotoBlock
                        TR::Block *gotoBlock     = TR::Block::createEmptyBlock(gotoTree->getNode(), comp, predBlock->getFrequency(), predBlock);


                        if (predEndWithGoto)
                           {
                           //Insert goto in new gotoBlock
                           storeTree->join(gotoTree->getNextTreeTop());
                           gotoTree->join(gotoBlock->getExit());
                           gotoBlock->getEntry()->join(gotoTree);
                           }
                        else
                           gotoTree = TR::TreeTop::create(comp, gotoBlock->getEntry(), TR::Node::create(storeNode, TR::Goto, 0, fallThroughTT));

                        //Hook up gotoBlock with fallThrough of pred block
                        gotoBlock->getExit()->join(predBlock->getExit()->getNextTreeTop());

                        //Hook up predBlock with newIfCmpBlock
                        predBlock->getExit()->join(newIfCmpBlock->getEntry());

                        //Hook up newIfCmpBlock with gotoBlock
                        newIfCmpBlock->getExit()->join(gotoBlock->getEntry());


                        cfg->addNode(newIfCmpBlock, parent);
                        cfg->addNode(gotoBlock, parent);

                        //traceMsg(comp, "\n AAA num=%d adding edge between block %d to block %d\n", num, predBlock->getNumber(), newIfCmpBlock->getNumber());

                        //Update edges for gotoBlock
                        cfg->addEdge(predBlock, newIfCmpBlock);
                        cfg->addEdge(newIfCmpBlock, gotoBlock);
                        cfg->addEdge(gotoBlock, fallThroughBB);

                        //Clone every treetop of block, all referenceCount to be 1
                        TR::TreeTop *curTree    = block->getEntry()->getNextTreeTop();
                        TR::TreeTop *curNewTree = newIfCmpBlock->getEntry();
                        while (curTree != block->getExit())
                           {
                           TR::TreeTop * newTreeTop = curTree->duplicateTree();
                           newTreeTop->join(curNewTree->getNextTreeTop());
                           curNewTree->join(newTreeTop);
                           curNewTree = newTreeTop;
                           curTree    = curTree->getNextTreeTop();
                           }

                        gotoTree->getNode()->setBranchDestination(fallThroughTT);

                        //Update edges of predBlock
                        cfg->addEdge(newIfCmpBlock, branchDestBB);
                        cfg->removeEdge(predBlock, block);

                        didSomething = true;
                        }
                     }
                  }
               }
            }
         }
      if (didSomething)
         {
         //fallThroughBB has at least two predecessors now
         //it's safe to unset this flags since ifcmp doesn't have any child with refrerenceCount >1
         fallThroughBB->setIsExtensionOfPreviousBlock(false);
         }
      }
   }

static void addressCompareConversion(TR::Node * node, TR::Simplifier * s)
   {
   TR::Node * firstChild  = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();

   TR::ILOpCodes firstOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
   TR::ILOpCodes addresOp = TR::BadILOp;
   switch (node->getOpCodeValue())
      {
      case TR::iflcmpne:
      case TR::ificmpne: addresOp=TR::ifacmpne;break;
      case TR::iflcmpeq:
      case TR::ificmpeq: addresOp=TR::ifacmpeq;break;
      default: return;
      }

      if (firstOp == TR::a2i && firstChild->getFirstChild()->getType().isAddress() &&
          TR::Compiler->target.is32Bit() &&
          firstChild->getReferenceCount()==1)
         {
         if ((secondOp == TR::iconst && secondChild->getInt()==0) ||
            (secondOp == TR::a2i))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, addresOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::a2i)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               dumpOptDetails(s->comp(), "Address Compare Conversion: found both children a2i in node %p\n", node);
               }
            else if (secondOp == TR::iconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::aconst(secondChild, secondChild->getInt(),4);
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  newSecondChild->setIsClassPointerConstant(false);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::aconst);
                  secondChild->setIsClassPointerConstant(false);
                  }
                  dumpOptDetails(s->comp(), "Address Compare Conversion: found child 1 a2i and child 2 iconst in node %p\n", node);
               }

            }
         }
      else  if (firstOp == TR::a2l && firstChild->getFirstChild()->getType().isAddress() &&
                TR::Compiler->target.is64Bit() &&
                firstChild->getReferenceCount()==1)
         {
        if ((secondOp == TR::lconst && secondChild->getLongInt()==0) ||
            (secondOp == TR::a2l))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, addresOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::a2l)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               dumpOptDetails(s->comp(), "Address Compare Conversion: found both children a2l in node %p\n", node);
               }
            else if (secondOp == TR::lconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::aconst(secondChild, secondChild->getLongInt(),8);
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  newSecondChild->setIsClassPointerConstant(false);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::aconst);
                  secondChild->setIsClassPointerConstant(false);
                  }
               dumpOptDetails(s->comp(), "Address Compare Conversion: found child 1 a2l and child 2 lconst in node %p\n", node);
               }
            }
         }
   }

static void longCompareNarrower(TR::Node * node, TR::Simplifier * s, TR::ILOpCodes intOp, TR::ILOpCodes charOp, TR::ILOpCodes shortOp, TR::ILOpCodes byteOp)
   {
   TR::Node * firstChild  = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   static char * reportCompareDemotions = feGetEnv("TR_ReportCompareDemotions");
   //if (s->_cg()>getSupportsEfficientNarrowIntComputation() &&
   //    performTransformation(s->comp(), "%sLong compare narrower for node [%p]\n", s->optDetailString(), node))
      {
      TR::ILOpCodes firstOp  = firstChild->getOpCodeValue();
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if ((firstOp == TR::i2l) &&
          performTransformation(s->comp(), "%sLong compare narrower for node [%p]\n", s->optDetailString(), node))
         {
         if (secondOp == TR::iconst || secondOp == TR::i2l ||
             (secondOp == TR::lconst               &&
              secondChild->getLongInt() >= INT_MIN &&
              secondChild->getLongInt() <= INT_MAX))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, intOp);
            node->notifyChangeToValueOfNode();
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::i2l)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Long Compare Narrower: found both children i2l in method %s\n", s->comp()->signature());
                  }
               }
            else if (secondOp == TR::lconst)
               {
               if (secondChild->getReferenceCount()==1)
                  {
                  TR::Node::recreate(secondChild, TR::iconst);
                  secondChild->setInt((int32_t)secondChild->getLongInt());
                  secondChild->notifyChangeToValueOfNode();
                  }
               else
                  {
                  secondChild->decReferenceCount();
                  TR::Node * iConstNode = TR::Node::create(node, TR::iconst, 0, (int32_t)secondChild->getLongInt());
                  node->setAndIncChild(1, iConstNode);
                  }
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 i2l and child 2 lconst in iconst range in method %s\n", s->comp()->signature());
                  }
               }
            else if (reportCompareDemotions)
               {
               dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 i2l and child 2 iconst in method %s\n", s->comp()->signature());
               }
            }
         }
      else if (node->getOpCode().isCompareForEquality() &&
               firstOp == TR::land &&
               firstChild->getSecondChild()->getOpCodeValue() == TR::lconst &&
               secondOp == TR::lconst)
         {
         uint64_t val1 = firstChild->getSecondChild()->getLongInt();
         uint64_t val2 = secondChild->getLongInt();
         if ((val1 & 0xffffffff00000000ull) == 0 &&
             (val2 & 0xffffffff00000000ull) == 0 &&
             performTransformation(s->comp(), "%sLong compare narrower: equality with no upper bits [%p]\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, intOp);
            node->notifyChangeToValueOfNode();
            TR::Node* ival1 = TR::Node::create(node, TR::iconst, 0);
            ival1->setInt((int)val1);
            TR::Node* ival2 = TR::Node::create(node, TR::iconst, 0);
            ival2->setInt((int)val2);
            node->setAndIncChild(1, ival2);
            TR::Node* conv = TR::Node::create(node, TR::l2i, 1);
            conv->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node* newAnd = TR::Node::create(node, TR::iand, 2);
            newAnd->setAndIncChild(0, conv);
            newAnd->setAndIncChild(1, ival1);
            node->setAndIncChild(0, newAnd);
            firstChild->recursivelyDecReferenceCount();
            secondChild->decReferenceCount();
            }
         }
      else if (s->cg()->getSupportsEfficientNarrowIntComputation() &&
               performTransformation(s->comp(), "%sLong compare narrower for node [%p]\n", s->optDetailString(), node))
         {
         if (firstOp == TR::su2l)
            {
            if (secondOp == TR::cconst || secondOp == TR::su2l ||
                (secondOp == TR::lconst               &&
                 secondChild->getLongInt() >= 0 &&
                 secondChild->getLongInt() <= USHRT_MAX))
               {
               node->setAndIncChild(0, firstChild->getFirstChild());
               TR::Node::recreate(node, charOp);
               node->notifyChangeToValueOfNode();
               firstChild->recursivelyDecReferenceCount();
               if (secondOp == TR::su2l)
                  {
                  node->setAndIncChild(1, secondChild->getFirstChild());
                  secondChild->recursivelyDecReferenceCount();
                  if (reportCompareDemotions)
                     {
                     dumpOptDetails(s->comp(), "Long Compare Narrower: found both children c2l in method %s\n", s->comp()->signature());
                     }
                  }
               else if (secondOp == TR::lconst)
                  {
                  if (secondChild->getReferenceCount()==1)
                     {
                     TR::Node::recreate(secondChild, TR::cconst);
                     secondChild->setConst<uint16_t>((uint16_t)secondChild->getLongInt());
                     secondChild->notifyChangeToValueOfNode();
                     }
                  else
                     {
                     secondChild->decReferenceCount();
                     TR::Node * cConstNode = TR::Node::cconst(node, (uint16_t)secondChild->getLongInt());
                     node->setAndIncChild(1, cConstNode);
                     }
                  if (reportCompareDemotions)
                     {
                     dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 c2l and child 2 lconst in cconst range in method %s\n", s->comp()->signature());
                     }
                  }
               else if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 c2l and child 2 cconst in method %s\n", s->comp()->signature());
                  }
               }
            }
         else if (firstOp == TR::s2l)
            {
            if (secondOp == TR::sconst || secondOp == TR::s2l ||
                (secondOp == TR::lconst               &&
                 secondChild->getLongInt() >= s->cg()->getMinShortForLongCompareNarrower() &&
                 secondChild->getLongInt() <= SHRT_MAX))
               {
               node->setAndIncChild(0, firstChild->getFirstChild());
               TR::Node::recreate(node, shortOp);
               node->notifyChangeToValueOfNode();
               firstChild->recursivelyDecReferenceCount();
               if (secondOp == TR::s2l)
                  {
                  node->setAndIncChild(1, secondChild->getFirstChild());
                  secondChild->recursivelyDecReferenceCount();
                  if (reportCompareDemotions)
                     {
                     dumpOptDetails(s->comp(), "Long Compare Narrower: found both children s2l in method %s\n", s->comp()->signature());
                     }
                  }
               else if (secondOp == TR::lconst)
                  {
                  if (secondChild->getReferenceCount()==1)
                     {
                     TR::Node::recreate(secondChild, TR::sconst);
                     secondChild->setShortInt((int16_t)secondChild->getLongInt());
                     secondChild->notifyChangeToValueOfNode();
                     }
                  else
                     {
                     secondChild->decReferenceCount();
                     TR::Node * sConstNode = TR::Node::sconst(node, (int16_t)secondChild->getLongInt());
                     node->setAndIncChild(1, sConstNode);
                     }

                  if (reportCompareDemotions)
                     {
                     dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 s2l and child 2 lconst in sconst range in method %s\n", s->comp()->signature());
                     }
                  }
               else if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 s2l and child 2 sconst in method %s\n", s->comp()->signature());
                  }
               }
            }
         else if (firstOp == TR::b2l)
            {
            if (secondOp == TR::bconst || secondOp == TR::b2l ||
                (secondOp == TR::lconst               &&
                 secondChild->getLongInt() >= s->cg()->getMinByteForLongCompareNarrower() &&
                 secondChild->getLongInt() <= SCHAR_MAX))
               {
               node->setAndIncChild(0, firstChild->getFirstChild());
               TR::Node::recreate(node, byteOp);
               node->notifyChangeToValueOfNode();
               firstChild->recursivelyDecReferenceCount();
               if (secondOp == TR::b2l)
                  {
                  node->setAndIncChild(1, secondChild->getFirstChild());
                  secondChild->recursivelyDecReferenceCount();
                  if (reportCompareDemotions)
                     {
                     dumpOptDetails(s->comp(), "Long Compare Narrower: found both children b2l in method %s\n", s->comp()->signature());
                     }
                  }
               else if (secondOp == TR::lconst)
                  {
                  if (secondChild->getReferenceCount()==1)
                     {
                     TR::Node::recreate(secondChild, TR::bconst);
                     secondChild->setByte((int8_t)secondChild->getLongInt());
                     secondChild->notifyChangeToValueOfNode();
                     }
                  else
                     {
                     secondChild->decReferenceCount();
                     TR::Node * bConstNode = TR::Node::bconst(node, (int8_t)secondChild->getLongInt());
                     node->setAndIncChild(1, bConstNode);
                     }
                  if (reportCompareDemotions)
                     {
                     dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 b2l and child 2 lconst in bconst range in method %s\n", s->comp()->signature());
                     }
                  }
               else if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Long Compare Narrower: found child 1 b2l and child 2 bconst in method %s\n", s->comp()->signature());
                  }
               }
            }
         }
      }
   }

static void ifjlClassSimplifier(TR::Node * node, TR::Simplifier * s)
   {
   static char *disableJLClassSimplification = feGetEnv("TR_DisableJLClassSimplification");
   if (disableJLClassSimplification)
      return;

   if (node->getFirstChild()->getOpCode().hasSymbolReference() &&
       node->getSecondChild()->getOpCode().hasSymbolReference() &&
       (node->getFirstChild()->getSymbolReference() == node->getSecondChild()->getSymbolReference()) &&
       (node->getFirstChild()->getSymbolReference() == s->getSymRefTab()->findJavaLangClassFromClassSymbolRef()) &&
       performTransformation(s->comp(), "%sSimplify test on j/l/Class children [%p]\n", s->optDetailString(), node))
      {
      TR::Node *oldFirstChild = node->getFirstChild();
      TR::Node *oldSecondChild = node->getSecondChild();
      TR::Node *firstGrandChild = node->getFirstChild()->getFirstChild();
      TR::Node *secondGrandChild = node->getSecondChild()->getFirstChild();
      node->setAndIncChild(0, firstGrandChild);
      node->setAndIncChild(1, secondGrandChild);
      oldFirstChild->recursivelyDecReferenceCount();
      oldSecondChild->recursivelyDecReferenceCount();
      }
   }

static void transformToLongDivBy10Bitwise(TR::Node * origNode, TR::Node * node, TR::Simplifier * s)
   {

   /* long divide by 10 transforms to sequence of shift operations.  Nodes names
      as they appear from left to right on each line of the pseudocode below.
      long ldivs10(long n) {
         long q, r;
         n = n + (n>>63 & 9);  (nodes n1, n2, n3)
         q = (n >> 1) + (n >> 2);  (nodes q1, q2, q3)
         q = q + (q >> 4);  (nodes q4, q5)
         q = q + (q >> 8);  (nodes q6, q7)
         q = q + (q >> 16); (nodes q8, q9)
         q = q + (q >> 32); (nodes q10, q11)
         q = q >> 3;  (nodes 12)
         r = n - q*10; (nodes r1, r2)
         return q + ((r + 6) >> 4); nodes (ret1, ret2, ret3)
       }
   */

   TR::Node * firstChild = origNode->getFirstChild();
   TR::Node * secondChild = origNode->getSecondChild();

   //n = n + (n>>63 & 9);  (nodes n1, n2, n3)
   TR::Node *n2Node = TR::Node::create(TR::lshr, 2, firstChild, TR::Node::create(secondChild, TR::iconst, 0, 63));
   TR::Node *temp = NULL;
   TR::Node *n3Node = TR::Node::create(TR::land, 2, n2Node, temp = TR::Node::create(secondChild, TR::lconst, 0));
   temp->setLongInt(9);
   TR::Node *n1Node = TR::Node::create(TR::ladd, 2, firstChild, n3Node);

   //q = (n >> 1) + (n >> 2);  (nodes q1, q2, q3)
   TR::Node * q1Node = TR::Node::create(TR::lshr, 2, n1Node, TR::Node::create(secondChild, TR::iconst, 0, 1));
   TR::Node * q3Node = TR::Node::create(TR::lshr, 2, n1Node, TR::Node::create(secondChild, TR::iconst, 0, 2));
   TR::Node * q2Node = TR::Node::create(TR::ladd, 2, q1Node, q3Node);

   //q = q + (q >> 4);  (nodes q4, q5)
   TR::Node * q5Node = TR::Node::create(TR::lshr, 2, q2Node, TR::Node::create(secondChild, TR::iconst, 0, 4));
   TR::Node * q4Node = TR::Node::create(TR::ladd, 2, q2Node, q5Node);

   //q = q + (q >> 8);  (nodes q6, q7)
   TR::Node * q7Node = TR::Node::create(TR::lshr, 2, q4Node, TR::Node::create(secondChild, TR::iconst, 0, 8));
   TR::Node * q6Node = TR::Node::create(TR::ladd, 2, q4Node, q7Node);

   //q = q + (q >> 16); (nodes q8, q9)
   TR::Node * q9Node = TR::Node::create(TR::lshr, 2, q6Node, TR::Node::create(secondChild, TR::iconst, 0, 16));
   TR::Node * q8Node = TR::Node::create(TR::ladd, 2, q6Node, q9Node);

   //q = q + (q >> 32); (nodes q10, q11)
   TR::Node * q11Node = TR::Node::create(TR::lshr, 2, q8Node, TR::Node::create(secondChild, TR::iconst, 0, 32));
   TR::Node * q10Node = TR::Node::create(TR::ladd, 2, q8Node, q11Node);

   //q = q >> 3;  (nodes 12)
   TR::Node * q12Node = TR::Node::create(TR::lshr, 2, q10Node, TR::Node::create(secondChild, TR::iconst, 0, 3));

   //r = n - q*10; (nodes r1, r2)
   TR::Node * r2Node = TR::Node::create(TR::lmul, 2, q12Node, temp = TR::Node::create(secondChild, TR::lconst, 0));
   temp->setLongInt(10);
   TR::Node * r1Node = TR::Node::create(TR::lsub, 2, n1Node, r2Node);

   //return q + ((r + 6) >> 4); nodes (ret1, ret2, ret3)
   TR::Node * ret2Node = TR::Node::create(TR::ladd, 2, r1Node, temp = TR::Node::create(secondChild, TR::lconst, 0));
   temp->setLongInt(6);
   TR::Node * ret3Node = TR::Node::create(TR::lshr, 2, ret2Node, TR::Node::create(secondChild, TR::iconst, 0, 4));

   // Create the long divide tree
   node->setNumChildren(2);
   node->setAndIncChild(0, q12Node);
   node->setAndIncChild(1, ret3Node);
   node->setByteCodeInfo(q12Node->getByteCodeInfo());
   node->getByteCodeInfo().setDoNotProfile(1);
   }

static int decomposeConstant(char bitPosition[], char operationType[], int64_t value, int wordSize)
   {
   int32_t count           = 0;
   int32_t pos             = 0;
   int32_t subsequentCount = 0;
   int32_t lastBit         = 0;
   int32_t currentBit      = 0;
   int32_t i;
   int32_t op_plus         = OP_PLUS;
   int32_t op_minus        = OP_MINUS;

#if defined(DEBUG_DECOMPOSE)
   dumpOptDetails(s->comp(), "\n");
#endif

   if (value & ((int64_t)1 << (wordSize - 1)))
      {
      value = -value;
      op_plus = OP_MINUS;
      op_minus = OP_PLUS;
      }

   while (value != 0 && (pos < (wordSize)))
      {
        currentBit = value & 1;
        value >>= 1;

#if defined(DEBUG_DECOMPOSE)
        if (currentBit)
           dumpOptDetails(s->comp(), "1");
        else
           dumpOptDetails(s->comp(), "0");
#endif

        if (currentBit)
           {
           if (lastBit)
              {
              // the previous bit was one we are aiming for substract
              subsequentCount++;
              }
           else
              {
              // we don't have anything so far so set as add
              subsequentCount = 0;
              operationType[count] = op_plus;
              bitPosition[count] = pos;

              count++;
              }
           }
        else
           {
           if (subsequentCount > 1)
              {
              // there are more then 1 bits set in a row set the operation as substract
              operationType[count - 1] = op_minus;
              operationType[count] = op_plus;
              bitPosition[count] = pos;

              count ++;
              }
           else if (subsequentCount == 1)
              {
              operationType[count] = op_plus;
              bitPosition[count] = pos - 1;

              count ++;
              }

           subsequentCount = 0;
           }

        lastBit = currentBit;
        pos++;
      }

   // do the last set
   if (subsequentCount > 1)
      {
      if (pos >= wordSize - 1)
         {
         // we've reached the limit of the word size
         // most likely the transformation will be not worth it
         // but decompose it anyway
         pos = (wordSize - 1) - subsequentCount;
         for (i=0; i<subsequentCount; i++)
            {
            operationType[count] = op_plus;
            bitPosition[count] = pos;
            pos++;
            count++;
            }
         }
      else
         {
         operationType[count - 1] = op_minus;
         operationType[count] = op_plus;
         bitPosition[count] = pos;

         count++;
         }
      }
   else if (subsequentCount == 1)
      {
      operationType[count] = op_plus;
      bitPosition[count] = pos - 1;

      count++;
      }

#if defined(DEBUG_DECOMPOSE)
   dumpOptDetails(s->comp(), ", pos = %d\n", pos);
#endif

   return count;
   }

static TR::Node *generateDecomposedTree(TR::Node * parentNode, TR::Node * multiplierChild, TR::Simplifier * s, char bitPosition[], char operationType[], int from, int to, int tabspace, bool isLong)
   {
   int count = to - from;
   int i;
   TR::ILOpCodes mul_op, neg_op, add_op, sub_op;

   if (!isLong)
      {
      mul_op = TR::imul; neg_op = TR::ineg; add_op = TR::iadd; sub_op = TR::isub;
      }
   else
      {
      mul_op = TR::lmul; neg_op = TR::lneg; add_op = TR::ladd; sub_op = TR::lsub;
      }

   if (count <= 2)
      {
      if (count == 1)
         {
         if (operationType[from] == OP_MINUS)
            {
            TR::Node * thisNode = NULL;

            if (bitPosition[from] != 0)
               {
               TR::Node * constNode1 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from]))):
                                             (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from])));
               TR::Node * shlNode = TR::Node::create(mul_op, 2, multiplierChild, constNode1);
               thisNode = TR::Node::create(neg_op, 1, shlNode);
               }
            else
               {
               thisNode = TR::Node::create(neg_op, 1, multiplierChild);
               }

            return thisNode;
            }
         else
            {
            TR::Node * thisNode = NULL;
            if (bitPosition[from] != 0)
               {
               TR::Node * constNode1 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from]))):
                                             (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from])));
               thisNode = TR::Node::create(mul_op, 2, multiplierChild, constNode1);
               }
            else
               {
               thisNode = multiplierChild;
               }
            return thisNode;
            }
         }
      else
         {
         if (operationType[from+1] == OP_MINUS)
            {
            TR::Node * firstChild = NULL;
            TR::Node * secondChild = NULL;
            TR::Node * thisNode = NULL;

            if (operationType[from] == OP_MINUS)
               {
               if (bitPosition[from] == 0)
                  {
                  firstChild = TR::Node::create(neg_op, 1, multiplierChild);
                  }
               else
                  {
                  TR::Node * constNode1 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from]))):
                                                (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from])));
                  TR::Node * shlNode = TR::Node::create(mul_op, 2, multiplierChild, constNode1);
                  firstChild = TR::Node::create(neg_op, 1, shlNode);
                  }
               }
            else
               {
               if (bitPosition[from] == 0)
                  {
                  firstChild = multiplierChild;
                  }
               else
                  {
                  TR::Node * constNode1 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from]))):
                                                (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from])));
                  firstChild = TR::Node::create(mul_op, 2, multiplierChild, constNode1);
                  }
               }

            if (bitPosition[from+1] == 0)
               {
               thisNode = TR::Node::create(sub_op, 2, firstChild, multiplierChild);
               }
            else
               {
               TR::Node * constNode2 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from+1]))):
                                             (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from+1])));
               secondChild = TR::Node::create(mul_op, 2, multiplierChild, constNode2);

               thisNode = TR::Node::create(sub_op, 2, firstChild, secondChild);
               }

            return thisNode;
            }
         else
            {
            if (operationType[from] == OP_MINUS)
               {
               TR::Node * firstChild = NULL;
               TR::Node * secondChild = NULL;

               if (bitPosition[from+1] == 0)
                  {
                  firstChild = multiplierChild;
                  }
               else
                  {
                  TR::Node * constNode1 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from+1]))):
                                                (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from+1])));
                  firstChild = TR::Node::create(mul_op, 2, multiplierChild, constNode1);
                  }

               if (bitPosition[from] == 0)
                  {
                  secondChild = multiplierChild;
                  }
               else
                  {
                  TR::Node * constNode2 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from]))):
                                                (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from])));
                  secondChild = TR::Node::create(mul_op, 2, multiplierChild, constNode2);
                  }

               TR::Node * thisNode = TR::Node::create(sub_op, 2, firstChild, secondChild);

               return thisNode;
               }
            else
               {
               TR::Node * firstChild = NULL;
               TR::Node * secondChild = NULL;

               if (bitPosition[from] == 0)
                  {
                  firstChild = multiplierChild;
                  }
               else
                  {
                  TR::Node * constNode1 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from]))):
                                                (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from])));
                  firstChild = TR::Node::create(mul_op, 2, multiplierChild, constNode1);
                  }

               if (bitPosition[from+1] == 0)
                  {
                  secondChild = multiplierChild;
                  }
               else
                  {
                  TR::Node * constNode2 = isLong?(TR::Node::lconst( parentNode, (int64_t)1 << (int32_t)(bitPosition[from+1]))):
                                                (TR::Node::iconst(parentNode, 1 << (int32_t)(bitPosition[from+1])));
                  secondChild = TR::Node::create(mul_op, 2, multiplierChild, constNode2);
                  }

               TR::Node * thisNode = TR::Node::create(add_op, 2, firstChild, secondChild);

               return thisNode;
               }
            }
         }
      }
   else
      {
      int mid = (int32_t)(count >> 1)+1;
      TR::Node * thisNode   = NULL;
      TR::Node * firstChild  = NULL;
      TR::Node * secondChild = NULL;
      char thisOperation = operationType[from+mid];

      if (thisOperation==OP_MINUS)
         {
         for (i=from+mid;i<to;i++)
            operationType[i] = (operationType[i] == OP_MINUS) ? OP_PLUS : OP_MINUS;
         }

      firstChild = generateDecomposedTree (parentNode, multiplierChild, s, bitPosition, operationType, from, from + mid, tabspace+1, isLong);
      secondChild = generateDecomposedTree (parentNode, multiplierChild, s, bitPosition, operationType, from + mid, to, tabspace+1, isLong);

      if (thisOperation==OP_MINUS)
         thisNode = TR::Node::create(sub_op, 2, firstChild, secondChild);
      else
         thisNode = TR::Node::create(add_op, 2, firstChild, secondChild);

      return thisNode;
      }

   return NULL;
   }

static void printTree(TR::Simplifier * s, char bitPosition[], char operationType[], int from, int to, int tabspace, bool isLong)
   {
   int count = to - from;
   int i;

   traceMsg(s->comp(), "\n");
   for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");

   if (count <= 2)
      {
      if (count == 1)
         {
         if (operationType[from] == OP_MINUS)
            {
            traceMsg(s->comp(), isLong?"lneg\n":"ineg\n");
            operationType[from] = (operationType[from] == OP_MINUS) ? OP_PLUS : OP_MINUS;
            for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");
            traceMsg(s->comp(), "\t-> %cn<<%d ", (operationType[from] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from]));
            }
         else
            traceMsg(s->comp(), "-> %cn<<%d ", (operationType[from] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from]));
         }
      else
         {
         if (operationType[from+1] == OP_MINUS)
            {
            traceMsg(s->comp(), isLong?"lsub\n":"isub\n");
            for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");
            if (operationType[from] == OP_MINUS)
               {
               traceMsg(s->comp(), isLong?"\tlneg\n":"\tineg\n");
               operationType[from] = (operationType[from] == OP_MINUS) ? OP_PLUS : OP_MINUS;
               traceMsg(s->comp(), "\t\t-> %cn<<%d \n", (operationType[from] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from]));
               }
            else
               {
               traceMsg(s->comp(), "\t-> %cn<<%d \n", (operationType[from] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from]));
               }
            operationType[from+1] = (operationType[from+1] == OP_MINUS) ? OP_PLUS : OP_MINUS;
            for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");
            traceMsg(s->comp(), "\t-> %cn<<%d \n", (operationType[from+1] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from+1]));
            }
         else
            {
            if (operationType[from] == OP_MINUS)
               {
               traceMsg(s->comp(), isLong?"lsub\n":"isub\n");
               for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");
               traceMsg(s->comp(), "\t-> %cn<<%d \n", (operationType[from+1] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from+1]));
               operationType[from] = (operationType[from] == OP_MINUS) ? OP_PLUS : OP_MINUS;
               for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");
               traceMsg(s->comp(), "\t-> %cn<<%d \n", (operationType[from] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from]));
               }
            else
               {
               traceMsg(s->comp(), isLong?"ladd\n":"iadd\n");
               for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");
               traceMsg(s->comp(), "\t-> %cn<<%d \n", (operationType[from] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from]));
               for (i=0;i<tabspace;i++) traceMsg(s->comp(), "\t");
               traceMsg(s->comp(), "\t-> %cn<<%d \n", (operationType[from+1] == OP_MINUS)? '-':'+', (int32_t)(bitPosition[from+1]));
               }
            }
         }
      }
   else
      {
      int mid = (int32_t)(count >> 1)+1;

      if (operationType[from+mid]==OP_MINUS)
         {
         traceMsg(s->comp(), isLong?"lsub\n":"isub\n");
         for (i=from+mid;i<to;i++)
            operationType[i] = (operationType[i] == OP_MINUS) ? OP_PLUS : OP_MINUS;
         }
      else
         traceMsg(s->comp(), isLong?"ladd\n":"iadd\n");

      printTree(s, bitPosition, operationType, from, from + mid, tabspace+1, isLong);
      printTree(s, bitPosition, operationType, from + mid, to, tabspace+1, isLong);
      }
   traceMsg(s->comp(), "\n");
   }

static void decomposeMultiply(TR::Node *node, TR::Simplifier *s, bool isLong)
   {
   auto comp = s->comp();
   TR::Node *firstChild = node->getFirstChild(), *secondChild = node->getSecondChild();
   int count = 0;
   int i;
   char bitPosition[64];
   char operationType[64];
   char temp;

   count = decomposeConstant(bitPosition, operationType, isLong?secondChild->getLongInt():secondChild->getInt(), isLong?64:32);

   // reverse to get better results
   for (i=0;i<(count>>1);i++)
      {
      temp = bitPosition[i];
      bitPosition[i] = bitPosition[count - 1 - i];
      bitPosition[count - 1 - i] = temp;

      temp = operationType[i];
      operationType[i] = operationType[count - 1 - i];
      operationType[count - 1 - i] = temp;
      }

   bool iMulDecomposeReport = comp->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition);
   if (iMulDecomposeReport)
      {
      dumpOptDetails(comp, "\nvalue = " POINTER_PRINTF_FORMAT ", count = %d\n",
            isLong ? secondChild->getLongInt():secondChild->getInt(), count);

      char line[256] = "";
      for (i=0; i<count; i++)
         {
         switch (operationType[i])
            {
            case OP_PLUS: strcat(line, "+ "); break;
            case OP_MINUS: strcat(line, "- "); break;
            }
         char tmp[10] = "";
         sprintf(tmp, "2^%d ", (int32_t)(bitPosition[i]));
         strcat(line, tmp);
         }
      dumpOptDetails(comp, "%s\n", line);
      }

   if ((!s->containingStructure() || s->getLastRun()) &&
       comp->cg()->mulDecompositionCostIsJustified(count, bitPosition, operationType,
         isLong ? secondChild->getLongInt() : secondChild->getInt()) &&
       performTransformation(comp, "%sDecomposing mul with a constant, to shift left, "
             "add, sub, neg operations [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
      {
      if (iMulDecomposeReport)
         {
         char tempBitPos[64], tempOpType[64];
         int j;

         for (j=0;j<count;j++)
            {
            tempBitPos[j] = bitPosition[j];
            tempOpType[j] = operationType[j];
            }

         traceMsg(comp, "MUL Decomposition in method: %s\n", comp->signature());
         printTree(s, tempBitPos, tempOpType, 0, count, 0, isLong);
         }

      secondChild->decReferenceCount();
      firstChild->decReferenceCount();

      TR::Node * decomposedMulNode = generateDecomposedTree(node, firstChild, s, bitPosition, operationType, 0, count, 0, isLong);

      TR::Node::recreate(node, decomposedMulNode->getOpCodeValue());
      node->setChild(0, decomposedMulNode->getFirstChild());
      if (decomposedMulNode->getNumChildren() == 2)
         {
         node->setChild(1, decomposedMulNode->getSecondChild());
         }
      else // single neg opearation
         {
         node->setNumChildren(1);
         }
      }
   }


static void addSimplifierJavaPacked(TR::Node* node, TR::Block * block, TR::Simplifier* s)
   {
   return;
   }

static void foldAddressConstant(TR::Node * node, int64_t value, TR::Simplifier * s, bool anchorChildrenP)
   {
   if (!performTransformationSimplifier(node, s)) return;

   if (anchorChildrenP) s->anchorChildren(node, s->_curTree);

   s->prepareToReplaceNode(node, TR::aconst );
   node->setAddress(value);

   if (!node->getOpCode().isRef())
      setIsHighWordZero(node, s);

   dumpOptDetails(s->comp(), " to %s", node->getOpCode().getName());
   dumpOptDetails(s->comp(), " 0x%x\n", node->getAddress());

   }

// Returns a new node which represents the _quotient_ of the divide/rem stored
// in node (top will be an ladd).
//
TR::Node *getQuotientUsingMagicNumberMultiply(TR::Node *node, TR::Block *block, TR::Simplifier *s)
   {
   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   // the node we'll create and return to the caller
   TR::Node * replacementNode;

   if(node->getOpCodeValue() == TR::idiv || node->getOpCodeValue() == TR::irem)
      {
      // expose the magic number squence for this optimization
      // lowered tree will look like this:
      // iadd (node)
      //   ishr (node3)
      //     iadd (node2)
      //       iload [ dividend ]
      //       imulh (node1)
      //         ===>iload [ dividend ]
      //         iconst [ magic number ]
      //     iconst [ shift amount ]
      //   ishr (node4)
      //     ===>iload [ dividend ]
      //     iconst 31

      int32_t divisor, magicNumber, shiftAmount;
      divisor = secondChild->getInt();

      s->cg()->compute32BitMagicValues(divisor, &magicNumber, &shiftAmount);

      TR::Node * node1, * node2, * node3, * node4;

      node1 = TR::Node::create(TR::imulh, 2, firstChild,
                              TR::Node::create(firstChild, TR::iconst, 0, magicNumber));

      if ( (divisor > 0) && (magicNumber < 0) )
         {
         node2 = TR::Node::create(TR::iadd, 2, node1, firstChild);
         }
      else if ( (divisor < 0) && (magicNumber > 0) )
         {
         node2 = TR::Node::create(TR::isub, 2, node1, firstChild);
         }
      else
         node2 = node1;

      if(shiftAmount == 0)
         {
         if (divisor > 0)
            {
            node4 = TR::Node::create(TR::iushr, 2, firstChild,
                                     TR::Node::create(firstChild, TR::iconst, 0, 31));
            }
         else
            {
            node4 = TR::Node::create(TR::iushr, 2, node2,
                                     TR::Node::create(node2, TR::iconst, 0, 31));
            }

         replacementNode = TR::Node::create(TR::iadd, 2, node2, node4);
         }
      else
         {
         node3 = TR::Node::create(TR::ishr, 2, node2,
                                  TR::Node::create(node2, TR::iconst, 0, shiftAmount));

         if (divisor > 0)
            node4 = TR::Node::create(TR::iushr, 2, firstChild,
                                     TR::Node::create(firstChild, TR::iconst, 0, 31));
         else
            node4 = TR::Node::create(TR::iushr, 2, node3,
                                     TR::Node::create(node3, TR::iconst, 0, 31));
         replacementNode = TR::Node::create(TR::iadd, 2, node3, node4);
         }
      }
   else if(node->getOpCodeValue() == TR::ldiv || node->getOpCodeValue() == TR::lrem)
      {
      //
      // the magic number squence for this optimization
      // lowered tree will look like this:
      // ladd (node)
      //   lshr (node3)
      //     ladd (node2)
      //       lload [ dividend ]
      //       lmulh (node1)
      //         ===>lload [ dividend ]
      //         lconst [ magic number ]
      //     lconst [ shift amount ]
      //   lshr (node4)
      //     ===>lload [ dividend ]
      //     lconst 63

      int64_t divisor, magicNumber, shiftAmount;

      divisor = secondChild->getLongInt();

      s->cg()->compute64BitMagicValues(divisor, &magicNumber, &shiftAmount);

      // work nodes
      TR::Node * node1, * node2, * node3, * node4;//, * constNode;

      secondChild = TR::Node::create(firstChild, TR::lconst, 0);
      secondChild->setLongInt(magicNumber);

      // Multiply high divisor and magic number
      node1 = TR::Node::create(TR::lmulh, 2, firstChild, secondChild);
      //firstChild->incReferenceCount();

      if ( (divisor > 0) && ( magicNumber < 0 ) )
         {
         node2 = TR::Node::create(TR::ladd, 2, node1, firstChild);
         }
      else if ( (divisor < 0) && ( magicNumber > 0 ) )
         {
         node2 = TR::Node::create(TR::lsub, 2, node1, firstChild);
         }
      else
         node2 = node1;

      // If the shiftAmount is 0,  we do node3 <- node2 >> 0, i.e. node3 <- node2.
      // don't bother generating the lshr in this case.
      if(shiftAmount == 0)
         {
         if (divisor > 0)
            {
            node4 = TR::Node::create(TR::lushr, 2, firstChild,
                                     TR::Node::create(firstChild, TR::iconst, 0, 63));
            }
         else
            {
            node4 = TR::Node::create(TR::lushr, 2, node2,
                                     TR::Node::create(node2, TR::iconst, 0, 63));
            }

         replacementNode = TR::Node::create(TR::ladd, 2, node2, node4);
         }
      else
         {
         node3 = TR::Node::create(TR::lshr, 2, node2,
                                  TR::Node::create(node2, TR::iconst, 0, shiftAmount));

         if (divisor > 0)
            {
            node4 = TR::Node::create(TR::lushr, 2, firstChild,
                                     TR::Node::create(firstChild, TR::iconst, 0, 63));
            }
         else
            {
            node4 = TR::Node::create(TR::lushr, 2, node3,
                                     TR::Node::create(node3, TR::iconst, 0, 63));
            }

         replacementNode = TR::Node::create(TR::ladd, 2, node3, node4);
         }
      }
   else
      {
      TR_ASSERT(0, "getQuotientUsingMagicNumberMultiply called with incorrect opcode in node %p, need TR::idiv/TR::ldiv/TR::irem/TR::lrem.", node);
      }

   // done.  return the new node to caller.
   return replacementNode;
   }

/*
 * Run down tree and change the searchOp to its unsigned equivalent.  Only change
 * uncommoned nodes.  Only valid if we don't see any ops that could shift the bits
 * around ie any ops other than loads, ands, ors or convs
 */
static void changeConverts2Unsigned(TR::Node *node, TR::ILOpCodes searchOp,TR::Simplifier *s)
   {
   if(node->getReferenceCount() > 1) return;
   TR::ILOpCode op = node->getOpCode();
   if(!(op.isConversion() || op.isAnd() || op.isOr() || op.isLoad())) return;

   if(node->getOpCodeValue() == searchOp)
      {
      TR::ILOpCodes newOp = TR::BadILOp;
      switch(searchOp)
         {
         case TR::b2i: newOp = TR::bu2i; break;
         case TR::s2i: newOp = TR::su2i; break;
         default: return;
         }

      if (performTransformation(s->comp(), "%sConverted x2i [%s] to unsigned xu2i\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node::recreate(node, newOp);
         return;
         }
      }
   for (int32_t i=0; i<node->getNumChildren(); i++)
      {
      changeConverts2Unsigned(node->getChild(i),searchOp,s);
      }
   }


static void simplifyIntBranchArithmetic(TR::Node * node, TR::Node * & firstChild, TR::Node * & secondChild, TR::Simplifier * s)
   {
   SIMPLIFY_BRANCH_ARITHMETIC(Int,int32_t);
   }

static void simplifyLongBranchArithmetic(TR::Node * node, TR::Node * & firstChild, TR::Node * & secondChild, TR::Simplifier * s)
   {
   SIMPLIFY_BRANCH_ARITHMETIC(LongInt,int64_t);
   }

//**************************************
// Conditional branch folding
//
// Since the trees are normalized, if the first child is a constant then the
// second child must also be a constant.
//
// Fold by changing from a conditional branch to an unconditional branch or
// removing it.
//
// If the branch is to the immediately following block, remove the node.
//
static bool conditionalBranchFold(
   int32_t takeBranch, TR::Node * & node, TR::Node * firstChild, TR::Node * secondChild, TR::Block * block, TR::Simplifier * s)
   {
   if (branchToFollowingBlock(node, block, s->comp()))
      {
      s->conditionalToUnconditional(node, block, false);
      return true;
      }
   if (firstChild->getOpCode().isLoadConst())
      {
      TR_ASSERT(secondChild->getOpCode().isLoadConst(),
             "Tree not normalized in expression simplification");
      s->conditionalToUnconditional(node, block, takeBranch);
      return true;
      }
   return false;
   }

// Comparing const 0 to non-zero node for equality
static bool conditionalZeroComparisonBranchFold(
   TR::Node * & node, TR::Node * firstChild, TR::Node * secondChild, TR::Block * block, TR::Simplifier * s)
   {
   TR_ASSERT(node->getOpCode().isCompareForEquality(),
             "Expecting compart for equality for %p\n", node);

   if ((firstChild->isNonZero() || firstChild->isNonNull()) &&
         secondChild->getOpCode().isLoadConst() &&
         secondChild->getIntegerNodeValue<int64_t>() == 0)
      {
      s->conditionalToUnconditional(node, block, !node->getOpCode().isCompareTrueIfEqual());
      return true;
      }

   return false;
   }

// If either child is a NaN replace this node with that child.
//
static TR::Node *binaryNanFloatOp(TR::Node * node, TR::Node * firstChild, TR::Node * secondChild, TR::Simplifier * s)
   {
   if (isNaNFloat(secondChild))
      return s->replaceNode(node, secondChild, s->_curTree);
   if (isNaNFloat(firstChild))
      return s->replaceNode(node, firstChild, s->_curTree);
   return 0;
   }

// If either child is a NaN replace this node with that child.
//
static TR::Node *binaryNanDoubleOp(TR::Node * node, TR::Node * firstChild, TR::Node * secondChild, TR::Simplifier * s)
   {
   if (isNaNDouble(secondChild))
      return s->replaceNode(node, secondChild, s->_curTree);
   if (isNaNDouble(firstChild))
      return s->replaceNode(node, firstChild, s->_curTree);
   return 0;
   }

static float floatRecip(float value)
   {
   // return the reciprocal of a non-zero power of two float
   union {
      float f;
      int32_t i;
   } u;
   int32_t float_exp;
   u.f = value;
   float_exp = (u.i >> 23) & 0xff;
   TR_ASSERT(float_exp != 0 && float_exp != 0xff && (u.i & 0x7fffff) == 0, "bad argument to floatRecip");
   float_exp = ((0xfe - float_exp) & 0xff) << 23;
   u.i = (u.i & 0x807fffff) | float_exp;
   return u.f;
   }

static double doubleRecip(double value)
   {
   // return the reciprocal of a non-zero power of two double
   union {
      double d;
      int64_t i;
   } u;
   int64_t double_exp;
   u.d = value;
   double_exp = (u.i >> 52) & 0x7ff;
   TR_ASSERT(double_exp != 0 && double_exp != 0x7ff && (u.i & CONSTANT64(0xfffffffffffff)) == 0, "bad argument to doubleRecip");
   double_exp = ((0x7fe - double_exp) & 0x7ff) << 52;
   u.i = (u.i & CONSTANT64(0x800fffffffffffff)) | double_exp;
   return u.d;
   }

//---------------------------------------------------------------------
// Legal check
// bbStartNode -- start node of nextBlock
// inEdge      -- incoming edges of nextBlock
static bool isLegalToMerge(TR::Node * node, TR::Block * block, TR::Block * nextBlock, TR::CFGEdgeList  &nextExcpOut,
                    TR::Node    * bbStartNode, TR::CFGEdgeList& inEdge, TR::Simplifier * s, bool &blockIsEmpty)
   {
   TR::CFGEdge* outEdge = block->getSuccessors().front();

   // Skip if there are glRegDeps inbetween
   if (block->getExit()->getNode()->getNumChildren() >  0)
      return false;
   if (nextBlock->getEntry()->getNode()->getNumChildren() >  0)
      return false;
   if (nextBlock->isOSRCodeBlock())
      return false;

   blockIsEmpty = (block->getEntry() != NULL && block->getEntry()->getNextTreeTop() == block->getExit());
   if (inEdge.empty() || !blockIsEmpty && (inEdge.front() != outEdge || (inEdge.size() > 1)))
      return false;

   //Can't merge block with nextBlock if block is entry point and inEdge has more than one edge
   //Since there may be a goto/branch which go around entry point, in that case merging will skip the prologue for that empty entry point
   //See ey1f520, block 5(INTERNAL4) and block 5
   if (blockIsEmpty && (inEdge.size() > 1) && (block == s->comp()->getStartBlock() || block->hasExceptionPredecessors()))
     return false;

   // See if the two blocks have compatible exception edges
   //
   if (nextBlock->hasExceptionPredecessors())
      return false;

   TR::CFGEdgeList &excpOut     = block->getExceptionSuccessors();
   if (excpOut.empty())
      {
      if (!nextExcpOut.empty())
         return false;
      }
   else
      {
      if (excpOut.size() != nextExcpOut.size())
         return false;
      for (auto outEdge = excpOut.begin(); outEdge != excpOut.end(); ++outEdge)
         {
         for (auto nextOutEdge = nextExcpOut.begin(); ; ++nextOutEdge)
            {
            if (nextOutEdge == nextExcpOut.end())
               return false;
            if ((*nextOutEdge)->getTo() == (*outEdge)->getTo())
               break;
            }
         }
      }

   TR::CFG * cfg = s->comp()->getFlowGraph();
   outEdge = block->getSuccessors().front();

   //If nextBlock has predecessor whose target can't be changed, then don't merge
   if (!(inEdge.empty()) && (inEdge.front() != outEdge || (inEdge.size() > 1)))
      {
      for (auto ie = inEdge.begin(); ie != inEdge.end(); ++ie)
         {
         if ((*ie) != outEdge && (*ie)->getFrom() != cfg->getStart())
            {
            //traceMsg(s->comp()," fromBlock:%p:%p:%d cfg->getStart()=%p toBlock:%d",ie->getData()->getFrom(),ie->getData()->getFrom()->asBlock(), ie->getData()->getFrom()->getNumber(),s->comp()->getFlowGraph()->getStart(), nextBlock->getNumber());
            // change edge "To" target if no exsiting edge to the merge block exist
            bool isLegal = (*ie)->getFrom()->asBlock()->getLastRealTreeTop()->isLegalToChangeBranchDestination(s->comp());
            if (!isLegal)
               return false;
            }
         }
      }

   // Do not remove loop pre-header unless it is last run
   if (cfg &&
       /* cfg->getStructure() && */
          !s->comp()->cg()->getGRACompleted() &&
       ((block->isLoopInvariantBlock()) ||
        (nextBlock->isLoopInvariantBlock())))
      return false;

   if (block->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets() ||
      (block->getLastRealTreeTop()->getNode()->getOpCodeValue()==TR::treetop &&
       block->getLastRealTreeTop()->getNode()->getFirstChild()->getOpCode().isJumpWithMultipleTargets()))
      return false;

   if (block->getLastRealTreeTop()->getNode()->getOpCode().isReturn())
      return false;

   // The two blocks can be merged.
   //
   if (block->getNumber() >= 0 && nextBlock->getNumber())
      {
      if (!performTransformation(s->comp(), "%sMerge blocks: block_%d and block_%d\n", s->optDetailString(), block->getNumber(), nextBlock->getNumber()))
         return false;
      }
   else
      {
      if (!performTransformation(s->comp(), "%sMerge blocks [" POINTER_PRINTF_FORMAT "] and [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), block, nextBlock))
         return false;
      }

   return true;
   }

// Changes branch destinations for merge blocks.
//
static void changeBranchDestinationsForMergeBlocks(TR::Block * block, TR::Block * nextBlock,
                    TR::Node    * bbStartNode, TR::CFGEdgeList &inEdge, TR::CFGEdgeList::iterator &inEdgeIter, TR::Simplifier * s)
   {
   TR::CFGEdgeList &successors     = block->getSuccessors();
   TR::CFGEdge* outEdge = successors.front();

   //if (inEdge->getNextElement() != NULL)
      {
      TR::CFG * cfg = s->comp()->getFlowGraph();
      if (cfg->getStructure() != NULL)
         {
         // merging structures does not handle multiple predecessors to the second block
         dumpOptDetails(s->comp(), "%s Resetting structure\n", s->optDetailString());
         cfg->setStructure(NULL);
         s->_containingStructure = NULL;
         }
      }

   for (auto ie = inEdge.begin(); ie != inEdge.end();)
      {
      auto next = ie;
      ++next;
      if ((*ie) != outEdge)
         {
         //nextBlock's predecessor has an edge to block already
         if ((*ie)->getFrom()->hasSuccessor(block))
            {
            TR::Block *predBlock = (*ie)->getFrom()->asBlock();
            TR::Node  *brNode    = predBlock->getLastRealTreeTop()->getNode();

            if ((predBlock->getSuccessors().size() == 2)
                && brNode->getOpCode().isBranch()
                && !brNode->getOpCode().isJumpWithMultipleTargets())
               {
               predBlock->getLastRealTreeTop()->unlink(true);
               }
            else
               (*ie)->getFrom()->asBlock()->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(s->comp(), nextBlock->getEntry(),block->getEntry());
            s->comp()->getFlowGraph()->removeEdge((*ie));
            }
         else //Redirect the "To" of the edge to block
            {
            block->setIsExtensionOfPreviousBlock(false);
            (*ie)->setTo(block);
            (*ie)->getFrom()->asBlock()->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(s->comp(), nextBlock->getEntry(),block->getEntry());
            }
         }
      else
         inEdgeIter = ie;
      ie = next;
      }
   }

//
// To be able to convert:
//   iffcmpX
//     i2f
//       iload expression
//     fconst
//
// to:
//
//   ificmpX
//     iload expression
//     iconst
//
// requires that you know the number of digits of precision on the iload expression
// and can be sure that the precision will be the same with or without the i2f.
// Consider if the iload expression had the value 33554432. After the i2f, the value
// would be 3355430 because the float representation of the value does not as much
// precision as the integer representation. If the range of the value was known to be
// within the range of the float value, the conversion could still be done.
//
// This same argument applies for l2f and l2d since long has a higher precision than
// float and double.
//
// Without knowing the precision, we can still do the reduction if we know the constant
// is within the precision of the data type and therefore lost precision won't affect the
// comparison.
//
static bool floatExactlyRepresented(float constant)
   {
   const float positiveFloatExactMax = (float)  ((1 << 24) - 1);
   const float negativeFloatExactMax = (float) -((1 << 24) - 1);
   return (constant >= negativeFloatExactMax && constant <= positiveFloatExactMax);
   }

static bool doubleExactlyRepresented(double constant)
   {
   const double positiveDoubleExactMax = (double)  (((int64_t)1 << 53) - 1);
   const double negativeDoubleExactMax = (double) -(((int64_t)1 << 53) - 1);
   return (constant >= negativeDoubleExactMax && constant <= positiveDoubleExactMax);
   }

static bool intValueInFloatRange(TR::Node* conversionNode, float constant)
   {
   if (floatExactlyRepresented(constant))
      {
      return true;
      }
   else
      {
      return false; // we are not smart enough to know this yet
      }
   }

static bool longValueInDoubleRange(TR::Node* conversionNode, double constant)
   {
   if (doubleExactlyRepresented(constant))
      {
      return true;
      }
   else
      {
      return false; // we are not smart enough to know this yet
      }
   }

static bool longValueInFloatRange(TR::Node* conversionNode, float constant)
   {
   if (floatExactlyRepresented(constant))
      {
      return true;
      }
   else
      {
      return false; // we are not smart enough to know this yet
      }
   }

static TR::ILOpCodes doubleToFloatOp(TR::ILOpCodes dOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (dOp)
      {
      case TR::ifdcmpeq:
         op = TR::iffcmpeq;
         break;
      case TR::ifdcmpne:
         op = TR::iffcmpne;
         break;
      case TR::ifdcmplt:
         op = TR::iffcmplt;
         break;
      case TR::ifdcmpge:
         op = TR::iffcmpge;
         break;
      case TR::ifdcmpgt:
         op = TR::iffcmpgt;
         break;
      case TR::ifdcmple:
         op = TR::iffcmple;
         break;
      case TR::ifdcmpequ:
         op = TR::iffcmpequ;
         break;
      case TR::ifdcmpneu:
         op = TR::iffcmpneu;
         break;
      case TR::ifdcmpltu:
         op = TR::iffcmpltu;
         break;
      case TR::ifdcmpgeu:
         op = TR::iffcmpgeu;
         break;
      case TR::ifdcmpgtu:
         op = TR::iffcmpgtu;
         break;
      case TR::ifdcmpleu:
         op = TR::iffcmpleu;
         break;
      default:
         return op;
      }

   return op;
   }

static TR::ILOpCodes doubleToIntegerOp(TR::ILOpCodes dOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (dOp)
      {
      case TR::ifdcmpeq:
      case TR::ifdcmpequ:
         op = TR::ificmpeq;
         break;
      case TR::ifdcmpne:
      case TR::ifdcmpneu:
         op = TR::ificmpne;
         break;
      case TR::ifdcmplt:
      case TR::ifdcmpltu:
         op = TR::ificmplt;
         break;
      case TR::ifdcmpge:
      case TR::ifdcmpgeu:
         op = TR::ificmpge;
         break;
      case TR::ifdcmpgt:
      case TR::ifdcmpgtu:
         op = TR::ificmpgt;
         break;
      case TR::ifdcmple:
      case TR::ifdcmpleu:
         op = TR::ificmple;
         break;
      default:
         return op;
      }

   return op;
   }

static TR::ILOpCodes doubleToLongOp(TR::ILOpCodes dOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (dOp)
      {
      case TR::ifdcmpeq:
      case TR::ifdcmpequ:
         op = TR::iflcmpeq;
         break;
      case TR::ifdcmpne:
      case TR::ifdcmpneu:
         op = TR::iflcmpne;
         break;
      case TR::ifdcmplt:
      case TR::ifdcmpltu:
         op = TR::iflcmplt;
         break;
      case TR::ifdcmpge:
      case TR::ifdcmpgeu:
         op = TR::iflcmpge;
         break;
      case TR::ifdcmpgt:
      case TR::ifdcmpgtu:
         op = TR::iflcmpgt;
         break;
      case TR::ifdcmple:
      case TR::ifdcmpleu:
         op = TR::iflcmple;
         break;
      default:
         return op;
      }

   return op;
   }

static TR::ILOpCodes doubleToShortOp(TR::ILOpCodes dOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (dOp)
      {
      case TR::ifdcmpeq:
      case TR::ifdcmpequ:
         op = TR::ifscmpeq;
         break;
      case TR::ifdcmpne:
      case TR::ifdcmpneu:
         op = TR::ifscmpne;
         break;
      case TR::ifdcmplt:
      case TR::ifdcmpltu:
         op = TR::ifscmplt;
         break;
      case TR::ifdcmpge:
      case TR::ifdcmpgeu:
         op = TR::ifscmpge;
         break;
      case TR::ifdcmpgt:
      case TR::ifdcmpgtu:
         op = TR::ifscmpgt;
         break;
      case TR::ifdcmple:
      case TR::ifdcmpleu:
         op = TR::ifscmple;
         break;
      default:
         return op;
      }

   return op;
   }

static TR::ILOpCodes doubleToCharOp(TR::ILOpCodes dOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (dOp)
      {
      case TR::ifdcmpeq:
      case TR::ifdcmpequ:
         op = TR::ifsucmpeq;
         break;
      case TR::ifdcmpne:
      case TR::ifdcmpneu:
         op = TR::ifsucmpne;
         break;
      case TR::ifdcmplt:
      case TR::ifdcmpltu:
         op = TR::ifsucmplt;
         break;
      case TR::ifdcmpge:
      case TR::ifdcmpgeu:
         op = TR::ifsucmpge;
         break;
      case TR::ifdcmpgt:
      case TR::ifdcmpgtu:
         op = TR::ifsucmpgt;
         break;
      case TR::ifdcmple:
      case TR::ifdcmpleu:
         op = TR::ifsucmple;
         break;
      default:
         return op;
      }

   return op;
   }

static TR::ILOpCodes doubleToByteOp(TR::ILOpCodes dOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (dOp)
      {
      case TR::ifdcmpeq:
      case TR::ifdcmpequ:
         op = TR::ifbcmpeq;
         break;
      case TR::ifdcmpne:
      case TR::ifdcmpneu:
         op = TR::ifbcmpne;
         break;
      case TR::ifdcmplt:
      case TR::ifdcmpltu:
         op = TR::ifbcmplt;
         break;
      case TR::ifdcmpge:
      case TR::ifdcmpgeu:
         op = TR::ifbcmpge;
         break;
      case TR::ifdcmpgt:
      case TR::ifdcmpgtu:
         op = TR::ifbcmpgt;
         break;
      case TR::ifdcmple:
      case TR::ifdcmpleu:
         op = TR::ifbcmple;
         break;
      default:
         return op;
      }

   return op;
   }

static TR::ILOpCodes floatToIntegerOp(TR::ILOpCodes fOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (fOp)
      {
      case TR::iffcmpeq:
      case TR::iffcmpequ:
         op = TR::ificmpeq;
         break;
      case TR::iffcmpne:
      case TR::iffcmpneu:
         op = TR::ificmpne;
         break;
      case TR::iffcmplt:
      case TR::iffcmpltu:
         op = TR::ificmplt;
         break;
      case TR::iffcmpge:
      case TR::iffcmpgeu:
         op = TR::ificmpge;
         break;
      case TR::iffcmpgt:
      case TR::iffcmpgtu:
         op = TR::ificmpgt;
         break;
      case TR::iffcmple:
      case TR::iffcmpleu:
         op = TR::ificmple;
         break;
      default:
         return op;
      }
   return op;
   }

static TR::ILOpCodes floatToLongOp(TR::ILOpCodes fOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (fOp)
      {
      case TR::iffcmpeq:
      case TR::iffcmpequ:
         op = TR::iflcmpeq;
         break;
      case TR::iffcmpne:
      case TR::iffcmpneu:
         op = TR::iflcmpne;
         break;
      case TR::iffcmplt:
      case TR::iffcmpltu:
         op = TR::iflcmplt;
         break;
      case TR::iffcmpge:
      case TR::iffcmpgeu:
         op = TR::iflcmpge;
         break;
      case TR::iffcmpgt:
      case TR::iffcmpgtu:
         op = TR::iflcmpgt;
         break;
      case TR::iffcmple:
      case TR::iffcmpleu:
         op = TR::iflcmple;
         break;
      default:
         return op;
      }
   return op;
   }

static TR::ILOpCodes floatToShortOp(TR::ILOpCodes fOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (fOp)
      {
      case TR::iffcmpeq:
      case TR::iffcmpequ:
         op = TR::ifscmpeq;
         break;
      case TR::iffcmpne:
      case TR::iffcmpneu:
         op = TR::ifscmpne;
         break;
      case TR::iffcmplt:
      case TR::iffcmpltu:
         op = TR::ifscmplt;
         break;
      case TR::iffcmpge:
      case TR::iffcmpgeu:
         op = TR::ifscmpge;
         break;
      case TR::iffcmpgt:
      case TR::iffcmpgtu:
         op = TR::ifscmpgt;
         break;
      case TR::iffcmple:
      case TR::iffcmpleu:
         op = TR::ifscmple;
         break;
      default:
         return op;
      }
   return op;
   }

static TR::ILOpCodes floatToCharOp(TR::ILOpCodes fOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (fOp)
      {
      case TR::iffcmpeq:
      case TR::iffcmpequ:
         op = TR::ifsucmpeq;
         break;
      case TR::iffcmpne:
      case TR::iffcmpneu:
         op = TR::ifsucmpne;
         break;
      case TR::iffcmplt:
      case TR::iffcmpltu:
         op = TR::ifsucmplt;
         break;
      case TR::iffcmpge:
      case TR::iffcmpgeu:
         op = TR::ifsucmpge;
         break;
      case TR::iffcmpgt:
      case TR::iffcmpgtu:
         op = TR::ifsucmpgt;
         break;
      case TR::iffcmple:
      case TR::iffcmpleu:
         op = TR::ifsucmple;
         break;
      default:
         return op;
      }
   return op;
   }

static TR::ILOpCodes floatToByteOp(TR::ILOpCodes fOp)
   {
   TR::ILOpCodes op = TR::BadILOp;
   switch (fOp)
      {
      case TR::iffcmpeq:
      case TR::iffcmpequ:
         op = TR::ifbcmpeq;
         break;
      case TR::iffcmpne:
      case TR::iffcmpneu:
         op = TR::ifbcmpne;
         break;
      case TR::iffcmplt:
      case TR::iffcmpltu:
         op = TR::ifbcmplt;
         break;
      case TR::iffcmpge:
      case TR::iffcmpgeu:
         op = TR::ifbcmpge;
         break;
      case TR::iffcmpgt:
      case TR::iffcmpgtu:
         op = TR::ifbcmpgt;
         break;
      case TR::iffcmple:
      case TR::iffcmpleu:
         op = TR::ifbcmple;
         break;
      default:
         return op;
      }
   return op;
   }

// Floating point precision check functions

static bool doubleConstIsRepresentableExactlyAsFloat(double dValue, float& fValue)
   {
   volatile float temp = (float)dValue;
   volatile double dValueTemp = dValue;
   if ((double)temp == dValueTemp)
      {
      fValue = temp;
      return true;
      }
   return false;
   }

static bool doubleConstIsRepresentableExactlyAsInt(double dValue, int32_t& iValue)
   {
   volatile int32_t temp = (int32_t)dValue;
   volatile double dValueTemp = dValue;
   if ((double)temp == dValueTemp)
      {
      iValue = temp;
      return true;
      }
   return false;
   }

static bool doubleConstIsRepresentableExactlyAsLong(double dValue, int64_t& lValue)
   {
   volatile int64_t temp = (int64_t)dValue;
   volatile double dValueTemp = dValue;
   if ((double)temp == dValueTemp)
      {
      lValue = temp;
      return true;
      }
   return false;
   }

static bool doubleConstIsRepresentableExactlyAsShort(double dValue, int16_t& sValue)
   {
   volatile int16_t temp = (int16_t)dValue;
   volatile double dValueTemp = dValue;
   if ((double)temp == dValueTemp)
      {
      sValue = temp;
      return true;
      }
   return false;
   }

static bool doubleConstIsRepresentableExactlyAsChar(double dValue, uint16_t& cValue)
   {
   volatile uint16_t temp = (uint16_t)dValue;
   volatile double dValueTemp = dValue;
   if ((double)temp == dValueTemp)
      {
      cValue = temp;
      return true;
      }
   return false;
   }

static bool doubleConstIsRepresentableExactlyAsByte(double dValue, int8_t& bValue)
   {
   volatile int8_t temp = (int8_t)dValue;
   volatile double dValueTemp = dValue;
   if ((double)temp == dValueTemp)
      {
      bValue = temp;
      return true;
      }
   return false;
   }

static bool floatConstIsRepresentableExactlyAsInt(float fValue, int32_t& iValue)
   {
   volatile int32_t temp = (int32_t)fValue;
   volatile float fValueTemp = fValue;
   if ((float)temp == fValueTemp)
      {
      iValue = temp;
      return true;
      }
   return false;
   }

static bool floatConstIsRepresentableExactlyAsLong(float fValue, int64_t& lValue)
   {
   volatile int64_t temp = (int64_t)fValue;
   volatile float fValueTemp = fValue;
   if ((float)temp == fValueTemp)
      {
      lValue = temp;
      return true;
      }
   return false;
   }

static bool floatConstIsRepresentableExactlyAsShort(float fValue, int16_t& sValue)
   {
   volatile int16_t temp = (int16_t)fValue;
   volatile float fValueTemp = fValue;
   if ((float)temp == fValueTemp)
      {
      sValue = temp;
      return true;
      }
   return false;
   }

static bool floatConstIsRepresentableExactlyAsChar(float fValue, uint16_t& cValue)
   {
   volatile uint16_t temp = (uint16_t)fValue;
   volatile float fValueTemp = fValue;
   if ((float)temp == fValueTemp)
      {
      cValue = temp;
      return true;
      }
   return false;
   }

static bool floatConstIsRepresentableExactlyAsByte(float fValue, int8_t& bValue)
   {
   volatile int8_t temp = (int8_t)fValue;
   volatile float fValueTemp = fValue;
   if ((float)temp == fValueTemp)
      {
      bValue = temp;
      return true;
      }
   return false;
   }



static void intCompareNarrower(TR::Node * node, TR::Simplifier * s, TR::ILOpCodes ushortOp, TR::ILOpCodes shortOp, TR::ILOpCodes byteOp)
   {
   TR::Node * firstChild  = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   static char * reportCompareDemotions = feGetEnv("TR_ReportCompareDemotions");
   if (s->cg()->getSupportsEfficientNarrowIntComputation())
      {
      TR::ILOpCodes firstOp  = firstChild->getOpCodeValue();
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if (firstOp == TR::su2i && firstChild->getReferenceCount()==1)
         {
         if (secondOp == TR::su2i ||
             (secondOp == TR::iconst               &&
              secondChild->getInt() >= 0 &&
              secondChild->getInt() <= USHRT_MAX))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, ushortOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::su2i)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found both children c2i in method %s\n", s->comp()->signature());
                  }
               }
            else if (secondOp == TR::iconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::cconst(secondChild, (uint16_t)secondChild->getInt());
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::cconst);
                  secondChild->setConst<uint16_t>((uint16_t)secondChild->getInt());
                  }
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 c2i and child 2 iconst in cconst range in method %s\n", s->comp()->signature());
                  }
               }
            else if (reportCompareDemotions)
               {
               dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 c2i and child 2 cconst in method %s\n", s->comp()->signature());
               }
            }
         }
      else if (firstOp == TR::s2i && firstChild->getReferenceCount() == 1)
         {
         if (secondOp == TR::s2i ||
             (secondOp == TR::iconst             &&
              secondChild->getInt() >= SHRT_MIN &&
              secondChild->getInt() <= SHRT_MAX))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, shortOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::s2i)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found both children s2i in method %s\n", s->comp()->signature());
                  }
               }
            else if (secondOp == TR::iconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::sconst(secondChild, (int16_t)secondChild->getInt());
                  newSecondChild->setShortInt((int16_t)secondChild->getInt());
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::sconst);
                  secondChild->setShortInt((int16_t)secondChild->getInt());
                  }
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 s2i and child 2 iconst in sconst range in method %s\n", s->comp()->signature());
                  }
               }
            else if (reportCompareDemotions)
               {
               dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 s2i and child 2 sconst in method %s\n", s->comp()->signature());
               }
            }
         }
      else if (firstOp == TR::b2i && firstChild->getReferenceCount() == 1)
         {
         if (secondOp == TR::b2i ||
             (secondOp == TR::iconst              &&
              secondChild->getInt() >= SCHAR_MIN &&
              secondChild->getInt() <= SCHAR_MAX))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, byteOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::b2i)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found both children b2i in method %s\n", s->comp()->signature());
                  }
               }
            else if (secondOp == TR::iconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::bconst(secondChild, (int8_t)secondChild->getInt());
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::bconst);
                  secondChild->setByte((int8_t)secondChild->getInt());
                  }

               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 b2i and child 2 iconst in bconst range in method %s\n", s->comp()->signature());
                  }
               }
            else if (reportCompareDemotions)
               {
               dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 b2i and child 2 bconst in method %s\n", s->comp()->signature());
               }
            }
         }
      }
   }

static void unsignedIntCompareNarrower(TR::Node * node, TR::Simplifier * s, TR::ILOpCodes ushortOp, TR::ILOpCodes ubyteOp)
   {
   TR::Node * firstChild  = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   static char * reportCompareDemotions = feGetEnv("TR_ReportCompareDemotions");
   if (s->cg()->getSupportsEfficientNarrowUnsignedIntComputation())
      {
      TR::ILOpCodes firstOp  = firstChild->getOpCodeValue();
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if (firstOp == TR::su2i && firstChild->getReferenceCount()==1)
         {
         if (secondOp == TR::su2i ||
             (secondOp == TR::iuconst               &&
              secondChild->getUnsignedInt() <= USHRT_MAX))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, ushortOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::su2i)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found both children c2i in method %s\n", s->comp()->signature());
                  }
               }
            else if (secondOp == TR::iuconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::cconst(secondChild, (uint16_t)secondChild->getInt());
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::cconst);
                  secondChild->setConst<uint16_t>((uint16_t)secondChild->getUnsignedInt());
                  }
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 c2i and child 2 iconst in cconst range in method %s\n", s->comp()->signature());
                  }
               }
            else if (reportCompareDemotions)
               {
               dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 c2i and child 2 cconst in method %s\n", s->comp()->signature());
               }
            }
         }
      else if (firstOp == TR::s2i && firstChild->getReferenceCount() == 1)
         {
         if (secondOp == TR::s2i ||
             (secondOp == TR::iconst               &&
              secondChild->getUnsignedInt() <= SHRT_MAX))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, ushortOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::s2i)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found both children s2i in method %s\n", s->comp()->signature());
                  }
               }
            else if (secondOp == TR::iconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::sconst(secondChild, (int16_t)secondChild->getInt());
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::sconst);
                  secondChild->setShortInt((int16_t)secondChild->getInt());
                  }
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 s2i and child 2 iconst in sconst range in method %s\n", s->comp()->signature());
                  }
               }
            else if (reportCompareDemotions)
               {
               dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 s2i and child 2 sconst in method %s\n", s->comp()->signature());
               }
            }
         }
      else if (firstOp == TR::b2i && firstChild->getReferenceCount() == 1)
         {
         if (secondOp == TR::b2i ||
             (secondOp == TR::iuconst               &&
              secondChild->getUnsignedInt() <= SCHAR_MAX))
            {
            node->setAndIncChild(0, firstChild->getFirstChild());
            TR::Node::recreate(node, ubyteOp);
            firstChild->recursivelyDecReferenceCount();
            if (secondOp == TR::b2i)
               {
               node->setAndIncChild(1, secondChild->getFirstChild());
               secondChild->recursivelyDecReferenceCount();
               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found both children b2i in method %s\n", s->comp()->signature());
                  }
               }
            else if (secondOp == TR::iuconst)
               {
               if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newSecondChild = TR::Node::bconst(secondChild, (int8_t)secondChild->getUnsignedInt());
                  secondChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(1, newSecondChild);
                  }
               else
                  {
                  TR::Node::recreate(secondChild, TR::bconst);
                  secondChild->setByte((int8_t)secondChild->getInt());
                  }

               if (reportCompareDemotions)
                  {
                  dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 b2i and child 2 iconst in bconst range in method %s\n", s->comp()->signature());
                  }
               }
            else if (reportCompareDemotions)
               {
               dumpOptDetails(s->comp(), "Integer Compare Narrower: found child 1 b2i and child 2 bconst in method %s\n", s->comp()->signature());
               }
            }
         }
      }
   }

//---------------------------------------------------------------------
// Helper function to convert int to float.  Should never be added to SimplifierTable!
//
static TR::Node *integerToFloatHelper(uint32_t absValue, bool isUnsignedOperation, TR::Node *node, TR::Simplifier *s)
   {
   TR::Node *firstChild = node->getFirstChild();

   if (leadingZeroes(absValue) + trailingZeroes(absValue) >= 8)
      {
      //no need for rounding
      foldFloatConstant(node, isUnsignedOperation ? (float)firstChild->getUnsignedInt() : (float)firstChild->getInt(), s);
      return node;
      }
   else
      {
      uint32_t roundBit = 0x80000000;
      roundBit >>= (leadingZeroes(absValue) + 24);

      if ((absValue & (4* roundBit -1)) != roundBit) absValue += roundBit;
      absValue &= ~((2 * roundBit) - 1);

      if (isUnsignedOperation)
         foldFloatConstant(node,  (float)firstChild->getUnsignedInt() , s);
      else
         foldFloatConstant(node, firstChild->getInt() < 0 ? -(float)absValue : (float)absValue, s);

      return node;
      }
   }

static bool containsLoad(TR::Node * node, TR::SymbolReference * symRef, vcount_t visitCount)
   {
   if(node->getOpCode().hasSymbolReference())
      if (node->getOpCode().isLoad() && node->getSymbolReference() == symRef)
         return true;

   int32_t i = node->getNumChildren()-1;
   for (;i >= 0;i--)
      {
      TR::Node * child=node->getChild(i);
      if (child->getReferenceCount() == visitCount)
         continue;
      if (containsLoad(child, symRef, visitCount))
         return true;
      }

   return false;
   }

static bool canMovePastTree(TR::TreeTop * tree, TR::SymbolReference * symRef, TR::Compilation * c, bool blockHasCatchers)
   {
   TR::Node * thisNode = tree->getNode();
   TR::SymbolReference * definingSymRef = NULL;

   if (containsLoad(thisNode, symRef, c->getVisitCount()))
      return false;   // don't move any update past a tree that loads the symbol

   if (blockHasCatchers && tree->getNode()->exceptionsRaised())
      return false;   // can't move it past any tree that could cause an exception if we have catchers

   if(thisNode->getOpCodeValue() ==  TR::treetop)
       thisNode = thisNode->getFirstChild();
   if (thisNode->getOpCode().isBranch()
       || thisNode->getOpCode().isJumpWithMultipleTargets())
      return false;   // can't move it past branch or switch node at end of block
   else if (thisNode->getOpCode().isStore())
      {
      definingSymRef = thisNode->getSymbolReference();
      if (definingSymRef == symRef)
         return false;
      }
   else if (thisNode->getOpCodeValue() == TR::treetop ||
            thisNode->getOpCode().isResolveOrNullCheck())
      {
      TR::Node * child = thisNode->getFirstChild();
      if (child->getOpCode().isStore())
         {
         definingSymRef = child->getSymbolReference();
         if (definingSymRef == symRef)
            return false;
         }
      else if (child->getOpCode().isCall() ||
               child->getOpCodeValue() == TR::monexit)
         definingSymRef = child->getSymbolReference();
      else if (thisNode->getOpCode().isResolveCheck())
         definingSymRef = child->getSymbolReference();
      }

   // Check if the definition could modify our symbol
   //
   if (definingSymRef != 0 && definingSymRef->getUseDefAliases().contains(symRef, c))
      return false;

   return true;
   }

static bool containsNode(TR::Node * tree, TR::Node * searchNode, vcount_t visitCount)
   {
   if (tree == searchNode)
      return true;

   int32_t i = tree->getNumChildren()-1;
   for (;i >= 0;i--)
      {
      TR::Node * child=tree->getChild(i);
      if (child->getReferenceCount() == visitCount)
         continue;
      if (containsNode(child, searchNode, visitCount))
         return true;
      }

   return false;
   }

static bool isBitwiseLongComplement(TR::Node * n)
   {
   if (n->getOpCodeValue() == TR::lxor)
      {
      TR::Node * secondChild = n->getSecondChild();
      if (secondChild->getOpCodeValue() == TR::lconst &&
          secondChild->getLongInt() == -1)
         {
         return true;
         }
      }
   return false;
   }

// This method replaces an or of shifts with a rotate opcode.
// the pseudocode it is trying to match resembles:
// (b) << (22) | (b) >> (32-22);
// If my input b = A B C D, after I run the above code, it would be  B C D A
// In testarossa, the IL trying to match would resemble this:
// ior
//    ishl
//       iload
//       iconst
//    iushr
//       iload
//       iconst
// Since we are simplifying children first, in all likelyhood the ishl will get transformed like this:
//n380n     istore  <auto slot 8>[id=232:"b"] [#60  Auto] [flags 0x3 0x0 ] (Int32)
//n379n       ior
//n373n         imul
//n371n           iload  <auto slot 8>[id=232:"b"] [#60  Auto] [flags 0x3 0x0 ] (Int32)
//n372n           iconst 0x400000
//n378n         iushr
//n374n           iload  <auto slot 8>[id=232:"b"] [#60  Auto] [flags 0x3 0x0 ] (Int32)
//n377n           iconst 10
//

template <typename T>
static bool checkAndReplaceRotation(TR::Node *node,TR::Block *block, TR::Simplifier *s)
   {
   static char* disableROLSimplification = feGetEnv("TR_DisableROLSimplification");
   if (disableROLSimplification)
      return false;

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR_ASSERT(node->getOpCode().isOr() || node->getOpCode().isXor(), "Node %p being checked for rotation pattern is not an (x)or!\n",node); // I believe this is general enough to allow other opcodes, like add's, xor's. But just want to be safe for now.

  // traceMsg(TR::comp(), "In checkAndReplaceRotation for node %p firstChild = %p secondChild = %p\n",node,firstChild,secondChild);

   if (!firstChild->getOpCode().isShift() && !firstChild->getOpCode().isShiftLogical() && !firstChild->getOpCode().isMul())
      return false;

   if (!secondChild->getOpCode().isShift() && !secondChild->getOpCode().isShiftLogical() && !secondChild->getOpCode().isMul())
      return false;

   // To simplify my code, I am not going to handle these two cases on the assumption that simplifier will already have simplified these away.

   //   if (firstChild->getOpCode().isLeftShift() && !secondChild->getOpCode().isShiftLogical() && !secondChild->getOpCode().isRightShift())
   //      return false;

   //   if ( secondChild->getOpCode().isLeftShift() && !firstChild->getOpCode().isShiftLogical() && !firstChild->getOpCode().isRightShift())
   //      return false;

   TR::Node *mulNode = firstChild->getOpCode().isMul() ? firstChild : ( secondChild->getOpCode().isMul() ? secondChild : 0 );
   TR::Node *shiftNode = firstChild->getOpCode().isMul() ? secondChild : ( secondChild->getOpCode().isMul() ? firstChild : 0 );

   if (!mulNode || !shiftNode || !(shiftNode->getOpCode().isShiftLogical() && shiftNode->getOpCode().isRightShift()))
         return false;

   TR::Node *mulConstNode = mulNode->getFirstChild()->getOpCode().isLoadConst() ? mulNode->getFirstChild() : (mulNode->getSecondChild()->getOpCode().isLoadConst() ?  mulNode->getSecondChild() : 0 );
   TR::Node *shiftConstNode = shiftNode->getFirstChild()->getOpCode().isLoadConst() ? shiftNode->getFirstChild() : (shiftNode->getSecondChild()->getOpCode().isLoadConst() ?  shiftNode->getSecondChild() : 0 );
   TR::Node *mulNonConstNode = mulNode->getFirstChild()->getOpCode().isLoadConst() ? mulNode->getSecondChild() : (mulNode->getSecondChild()->getOpCode().isLoadConst() ?  mulNode->getFirstChild() : 0 );
   TR::Node *shiftNonConstNode = shiftNode->getFirstChild()->getOpCode().isLoadConst() ? shiftNode->getSecondChild() : (shiftNode->getSecondChild()->getOpCode().isLoadConst() ?  shiftNode->getFirstChild() : 0 );

  if ( !mulConstNode || ! shiftConstNode || mulNonConstNode != shiftNonConstNode ) //requires commoning to have commoned the non const value.
     return false;

   //traceMsg(TR::comp(), "firstChild: isMul = %d isShiftLogical = %d isRightShift = %d secondChild: isMul = %d isShiftLogical = %d isRightShift = %d\n",firstChild->getOpCode().isMul(), firstChild->getOpCode().isShiftLogical(), firstChild->getOpCode().isRightShift(), secondChild->getOpCode().isMul(), firstChild->getOpCode().isShiftLogical(), firstChild->getOpCode().isRightShift());

   T mulConst = mulConstNode->getConst<T>();
   T shiftConst = shiftConstNode->getConst<T>();

   // have to calculate the left shift amount from the multiply

   T correctLeftShiftAmount = sizeof(T)*8 - shiftConst;

   //traceMsg(TR::comp(), "Spot 2.5 correctLeftShiftAmount = %d mulConst = %d shiftConst = %d sizeof(T)*8 = %d\n",correctLeftShiftAmount,mulConst,shiftConst,sizeof(T)*8);

   if ( (1 << correctLeftShiftAmount) != mulConst )      // Only a rotate if the const values add up to the size of the node type.  ie for ints, this better add up to 32
      return false;

   if (!performTransformation(s->comp(), "%sReduced or/xor/add in node [" POINTER_PRINTF_FORMAT "] to rol\n", s->optDetailString(), node))
      return false;

   TR::Node *newConstNode = 0;
   switch (mulConstNode->getDataType())
      {
      case TR::Int32:
         newConstNode = TR::Node::iconst(mulConstNode, correctLeftShiftAmount);
         break;
      case TR::Int64:
         newConstNode = TR::Node::lconst(mulConstNode, correctLeftShiftAmount);
         break;
      default:
         TR_ASSERT(0, "Data Type of node %p not supported in checkAndReplaceRotation!\n",mulConstNode);
      }

   TR::Node::recreate(node, TR::ILOpCode::getRotateOpCodeFromDt(mulConstNode->getDataType())); // this line will have to change based on T

   // Set the common nonconst node (child of mul and shift) as the child of rol node,
   // and then recursively decrement both mul and shift node.
   // The nonconst node will survive as it is attached to the rol node before recursivelyDecRefCount is called.
   node->setAndIncChild(0,mulNonConstNode);
   node->setAndIncChild(1,newConstNode);
   mulNode->recursivelyDecReferenceCount();
   shiftNode->recursivelyDecReferenceCount();

   s->_alteredBlock = true;
   s->simplify(node, block);
   return true;
   }


//---------------------------------------------------------------------
// Long helper to convert to float -- should never be called from Simplifier Table!
//
static TR::Node *longToFloatHelper(uint64_t absValue, bool isUnsignedOperation, TR::Node *node, TR::Simplifier *s)
   {
   TR::Node * firstChild = node->getFirstChild();


   if (leadingZeroes(absValue) + trailingZeroes(absValue) >= (64-24))
      {
      foldFloatConstant(node, isUnsignedOperation ? (float)firstChild->getUnsignedLongInt() : (float)firstChild->getLongInt(), s);
      return node;
      }
   else
      {
      uint64_t roundBit = CONSTANT64(0x8000000000000000);
      roundBit >>= (leadingZeroes(absValue) + 24);

      if ((absValue & (4* roundBit -1)) != roundBit) absValue += roundBit;
      absValue &= ~((2 * roundBit) - 1);
      if (isUnsignedOperation)
         foldFloatConstant(node, (float)absValue, s);
      else
         foldFloatConstant(node, firstChild->getLongInt() < 0 ? -(float)absValue : (float)absValue, s);
      return node;
      }
   }

//---------------------------------------------------------------------
// This method should never be added to SimplifierTable! It is used to convert values to doubles taking into account whether the operation is signed or not.
//
static TR::Node *longToDoubleHelper(uint64_t absValue, bool isUnsignedOperation, TR::Node *node, TR::Simplifier *s)
   {
   TR::Node * firstChild = node->getFirstChild();

   // determine if we have enough bits to precisely represent the constant.
   // long has 64 bits, double 53
   if (leadingZeroes(absValue) + trailingZeroes(absValue) >= (64-53))
      {
      foldDoubleConstant(node, isUnsignedOperation ? (double)firstChild->getUnsignedLongInt() : (double)firstChild->getLongInt(), s);
      return node;
      }
   else // probably the native compile rounds to ieee-754, but to be sure we round it manually
      {
      uint64_t roundBit = CONSTANT64(0x8000000000000000);
      roundBit >>= (leadingZeroes(absValue) + 53);

      if ((absValue & (4* roundBit -1)) != roundBit) absValue += roundBit;
      absValue &= ~((2 * roundBit) - 1);
      if(isUnsignedOperation)
         foldDoubleConstant(node, (double)absValue , s);
      else
         foldDoubleConstant(node, firstChild->getLongInt() < 0 ? -(double)absValue : (double)absValue, s);
      return node;
      }
   }

//---------------------------------------------------------------------
// Float convert to int
//
static int64_t floatToLong(float value, bool roundUp)
   {
   int32_t pattern=*(int32_t *)&value;
   int64_t result;
   if ((pattern & 0x7f800000)==0x7f800000 && (pattern & 0x007fffff)!=0)
      result = 0L;  // This is a NaN value
   else if (value <= TR::getMinSigned<TR::Int64>())
      result = TR::getMinSigned<TR::Int64>();
   else if (value >= TR::getMaxSigned<TR::Int64>())
      result = TR::getMaxSigned<TR::Int64>();
   else
      {
      if (roundUp)
         {
         if (value > 0)
            {
            value += 0.5;
            }
         else
            {
            value -= 0.5;
            }
         }

      result = (int64_t)value;
      }
   return result;
   }

//---------------------------------------------------------------------
// Double convert to int
//
static int64_t doubleToLong(double value, bool roundUp)
   {
   int64_t pattern = *(int64_t *)&value;
   int64_t result;
   if ((pattern & DOUBLE_ORDER(CONSTANT64(0x7ff0000000000000))) == DOUBLE_ORDER(CONSTANT64(0x7ff0000000000000)) &&
       (pattern & DOUBLE_ORDER(CONSTANT64(0x000fffffffffffff))) != 0)
      result = CONSTANT64(0);   // This is a NaN value
   else if (value <= TR::getMinSigned<TR::Int64>())
      result = TR::getMinSigned<TR::Int64>();
   else if (value >= TR::getMaxSigned<TR::Int64>())
      result = TR::getMaxSigned<TR::Int64>();
   else
      {
      if (roundUp)
         {
         if (value > 0)
            {
            value += 0.5;
            }
         else
            {
            value -= 0.5;
            }
         }

      result = (int64_t)value;
      }
   return result;
   }


/**
 * Checks if node is a loadaddr of a deleted label.
 */
static bool isDeletedLabelLoadaddr(TR::Node * node)
   {
   return false;
   }

// Simplify patterns that test particular bits in a byte via removing shift op
// These pattens mostly occur from languages with native bit type like C and PLX
// They are for testing if a particular bit is ON or OFF
// ifbcmpne                           ifbcmpne
//  band                               band
//    bshl                              ibload
//      ibload           ===>             loadaddr
//        loadaddr                      buconst N>>s
//      iconst s                       buconst M>>s
//    buconst N
//  buconst M

static void bitTestingOp(TR::Node * node, TR::Simplifier *s)
  {
  TR_ASSERT(node->getOpCode().isBooleanCompare(), "expecting compare node");
  if (TR::ILOpCode::isEqualCmp(node->getOpCode().getOpCodeValue()) &&
      TR::ILOpCode::isNotEqualCmp(node->getOpCode().getOpCodeValue()))
     return;

  TR::Node * firstChild = node->getFirstChild();
  TR::Node * secondChild = node->getSecondChild();

  if (!firstChild->getOpCode().isBitwiseLogical())
     return;
  if (!secondChild->getOpCode().isLoadConst())
     return;
  if (!firstChild->getFirstChild()->getOpCode().isLeftShift())
     return;
  if (!firstChild->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
     return;
  if (!firstChild->getSecondChild()->getOpCode().isLoadConst())
     return;

  int32_t shiftAmount = (int32_t)firstChild->getFirstChild()->getSecondChild()->get64bitIntegralValue();
  int64_t bitAmount = (int64_t)firstChild->getSecondChild()->get64bitIntegralValue();
  int64_t cmpAmount = (int64_t)secondChild->get64bitIntegralValue();

  if  ((bitAmount>>shiftAmount)<<shiftAmount!=bitAmount)
     return;

  if  ((cmpAmount>>shiftAmount)<<shiftAmount!=cmpAmount)
     return;

  if (!performTransformation(s->comp(), "%sRemoving shift node [" POINTER_PRINTF_FORMAT "] \n", s->optDetailString(), firstChild->getFirstChild()))
        return;

  TR::Node *shift=firstChild->getFirstChild();

  firstChild->setAndIncChild(0, shift->getFirstChild());
  shift->recursivelyDecReferenceCount();

  TR::DataType dt = node->getFirstChild()->getDataType();

  switch (dt)
     {
     case TR::Int8:
        foldByteConstant(firstChild->getSecondChild(),
                             (bitAmount & 0xFF)>>shiftAmount,
                             s, false /* !anchorChildren */);
        foldByteConstant(secondChild,
                             (cmpAmount & 0xFF)>>shiftAmount,
                             s, false /* !anchorChildren */);
        break;
     case TR::Int16:
        foldShortIntConstant(firstChild->getSecondChild(),
                             (bitAmount & 0xFFFF)>>shiftAmount,
                             s, false /* !anchorChildren */);
        foldShortIntConstant(secondChild,
                             (cmpAmount & 0xFFFF)>>shiftAmount,
                             s, false /* !anchorChildren */);
        break;
     case TR::Int32:
        foldIntConstant(firstChild->getSecondChild(),
                        (bitAmount & 0xFFFFFFFF)>>shiftAmount,
                        s, false /* !anchorChildren */);
        foldIntConstant(secondChild,
                        (cmpAmount & 0xFFFFFFFF)>>shiftAmount,
                        s, false /* !anchorChildren */);
        break;
     case TR::Int64:
        foldLongIntConstant(firstChild->getSecondChild(),
                            bitAmount>>shiftAmount,
                            s, false /* !anchorChildren */);
        foldLongIntConstant(secondChild,
                            cmpAmount>>shiftAmount,
                            s, false /* !anchorChildren */);
        break;
     default:
        TR_ASSERT(false, "should be unreachable");
     }
  }

/**
 * analyse expression and determine if it's result will exceed the precision of the node's
 * type.  The motivation is to see if using FMA whose intermediate result has higher precision
 * than the arguments, would be identical to an FP strict implementation.  If so, then
 * we can substitute (a*b)+c expression trees at codegeneration time with fma instructions,
 * if supported on the platform.

   In general we can use fmadd/fmsub if.
     - Multiply and add/subtract are in same method and
           - One child of multiply is double and other is power of 2 and method is not strictfp
           - One child of multiply is f2d or i2d and other is double const of <(53-24) or <(53-31)
        bits precision ,but result might overflow a double range or denormalised precision
     - Children of multiplies are both f2d
     - One child of multiply is f2d or i2d and other is double const of <(53-24) or <(53-31) bits precision
       and result cannot overflow a double range or precision.  This is currently not handled as it requires
       value propagation of floats.

  NOTE: rounding of very large values can cause overflow, but these constants would have to be:
  i2d: |constant| must be < 2^993
  f2d: |constant| must be < 2^896 and >= 2^-901
 */

static bool isOperationFPCompliant(TR::Node *parent,TR::Node *arithExprNode, TR::Simplifier *s)
   {
   static char * nofma = feGetEnv("TR_NOFMA");
   if (nofma) return false;

   if (!s->cg()->supportsFusedMultiplyAdd()) return false; // no point if FMA not supported

   if (!arithExprNode->getOpCode().isMul()) return false;

   if (s->comp()->getOption(TR_IgnoreIEEERestrictions)) return true;

   TR::Node *mulNode = arithExprNode;
   bool traceIt = false;
   bool i2dConv=false;
   bool f2dConv=false;

   /** if both children are being converted to double, and they're going from
    * less precision to double precision then it's safe to convert them */
   if (mulNode->getDataType()== TR::Double && mulNode->getFirstChild()->getOpCode().isConversion() &&
      mulNode->getSecondChild()->getOpCode().isConversion())
     { // long->double is only case where we must give up
     if (mulNode->getFirstChild()->getOpCode().isLong())
        {
        if (traceIt) traceMsg(s->comp(), "Node: %p fails because conversion type is long\n",mulNode->getFirstChild());
        return false;
        }
     if (mulNode->getSecondChild()->getOpCode().isLong())
        {
        if (traceIt) traceMsg(s->comp(), "Node: %p fails because conversion type is long\n",mulNode->getSecondChild());
        return false;
        }
     if (traceIt) traceMsg(s->comp(), "Node: %p passes because conversion type is <long\n",mulNode);
     return true;
     }

   bool sameCaller = mulNode->getInlinedSiteIndex() == parent->getInlinedSiteIndex();

   bool mulChildIsConst = mulNode->getSecondChild()->getOpCode().isLoadConst();
   TR::Node *mulConstChild = mulNode->getSecondChild();
   TR::Node *mulNonConstChild = mulNode->getFirstChild();

   /**
     * assume that constants have already been folded */
   if (mulNode->getFirstChild()->getOpCode().isLoadConst())
      {
      mulChildIsConst = true;
      mulConstChild = mulNode->getFirstChild();
      mulNonConstChild = mulNode->getSecondChild();
      }

   // 'constant' may be loaded out of literal pool
   if (!mulChildIsConst && s->cg()->supportsOnDemandLiteralPool())
      {
      if (mulNonConstChild->getOpCode().isLoadIndirect() && // the "nonconst' node is actually the const
         mulNonConstChild->getSymbolReference()->isLiteralPoolAddress())
         {
         TR::Node * temp = mulConstChild;
         mulConstChild = mulNonConstChild;
         mulNonConstChild = temp;
         mulChildIsConst=true;
         }

      if (mulChildIsConst || (mulConstChild->getOpCode().isLoadIndirect() &&
         mulConstChild->getSymbolReference()->isLiteralPoolAddress()))
         {
         // offset in the sym ref points to the constant node
         TR::Node *constNode = (TR::Node *)(mulConstChild->getSymbolReference()->getOffset());
         if (traceIt) traceMsg(s->comp(), "LiteralPool load detected:%p points to %p\n",mulConstChild,constNode);
         mulConstChild = constNode;
         mulChildIsConst = true;
         }
      }

   if (!mulChildIsConst)
      {
      if (traceIt) traceMsg(s->comp(), "Node %p fails since not a const\n",mulNode);
      return false;
      }

   // We guarantee at this point that the 2nd child is the constant

   if (!sameCaller)
      {
      if (traceIt) traceMsg(s->comp(), "Node: %p fails because split between two methods \n",mulNode);
      return false;
      }

   // we look at trailing zeros of the constant ie how 'small' is the number, and that
   // number indicates the kind of precision of the result.  Subtract this number
   // from the precision of current type and if smaller than the precision limit then
   // a fused multiply will have same results as lower-precision mult.
   // So, for example  i2f: float has 24 bits of precision, so the most precision
   // we have 53-24 bits of precision 'available'--anything requiring more bits will
   // give different results between fused multiply and double multiply
   // So the constant must not cause the precision to increase by more than
   // this 'available' bits.  So, if the constant is 1000...010000000000,
   // then there are 10 zeros and hence can increase precision requirements by 53-10=43 bits.
   // available precision is 53-24 = 19, 43 > 19 (=14) so can't use fmul
   // This boils down to whether precision of the constant is greater than the precision of the
   // non-const argument (aka the conversion operator).
   //
   if (mulNonConstChild->getOpCode().isConversion())
      {
      uint32_t precisionLimit=53; // bit count must be more than this to assure no rounding issues
      switch(mulNonConstChild->getOpCodeValue())
         {
         case TR::i2d: precisionLimit = 31; i2dConv = true; break;
         case TR::f2d: precisionLimit = 24; f2dConv = true; break;
         default: break;
         /*
          these nodes are actually never generated by J9, so no chance to test them
         case TR::b2d: precisionLimit = 8; break; //unsigned
         case TR::su2d: precisionLimit = 16; break; //unsigned
         case TR::s2d: precisionLimit = 15; break; //signed
         */
         }

      double possibleRoundedValue=1;// if not a float or double value, doesn't matter
      ///
      /// The Java language spec
      /// lists bit sizes for doubles.  53 bits used for precision exponent, 11 for magnitude
      /// and leading one for sign.  So check # bits in precision only by masking
      /// off these first 11+1 bits
      uint32_t bitcount;
      switch(mulConstChild->getDataType())
         {
         // I don't believe it is possible to reach here with anything other than a floating
         // point const child; even in the case of a literal pool load.
         case TR::Float:
           {
           union {
               float f;
               int32_t i;
           } u;
           u.f = mulConstChild->getFloat();
           possibleRoundedValue = fabs(u.f);
           bitcount = trailingZeroes(u.i & 0x007fffff); // just look @ mantissa
           break;
           }
         case TR::Double: // rely on fact same storage used for longs
            if (mulConstChild->getDataType() == TR::Double)
               {
               possibleRoundedValue = fabs(mulConstChild->getDouble());
               }
            bitcount = trailingZeroes(mulConstChild->getLongIntLow());
            if (32 == bitcount) bitcount+=  trailingZeroes(0x000fffff & mulConstChild->getLongIntHigh());

            if (traceIt) traceMsg(s->comp(), "Node:%p  bitcount:%d hex:%x%x value:%g\n",mulConstChild,bitcount,
                mulConstChild->getLongIntHigh(),mulConstChild->getLongIntLow(),
                mulConstChild->getDouble());

            break;

            {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
            TR_ASSERT(0, "We should not reach here for these data types.");
            return false;
            }

         default:
            return false;
         }

        // check for huge constants where rounding could cause overflow
        if (i2dConv)
           {
           if (possibleRoundedValue >= FMA_CONST_HIGHBOUNDI2D)
              {
               if (traceIt) traceMsg(s->comp(), "Fails because |constant| %e exceeds %e\n",possibleRoundedValue,FMA_CONST_HIGHBOUNDI2D);
               return false;
              }
           }
        else if (f2dConv)
           {
           if (possibleRoundedValue >= FMA_CONST_HIGHBOUNDF2D || possibleRoundedValue < FMA_CONST_LOWBOUND)
              {
               if (traceIt) traceMsg(s->comp(), "Fails because |constant| %e exceeds range %e-%e \n",possibleRoundedValue,FMA_CONST_LOWBOUND,
                                   FMA_CONST_HIGHBOUNDF2D);
               return false;
              }
           }

      if (bitcount > precisionLimit) return true;
      else if (traceIt)
        traceMsg(s->comp(), "Node:%p failed bitcount:%d >= precision:%d\n",mulConstChild,bitcount,precisionLimit);
      }

   bool loadConstIsPowerOfTwoDbl = (mulConstChild->getDataType() == TR::Double) &&
                isNZDoublePowerOfTwo(mulConstChild->getDouble());

   bool loadConstIsPowerOfTwoFloat = (mulConstChild->getDataType() == TR::Float) &&
                isNZFloatPowerOfTwo(mulConstChild->getFloat());

   bool isStrictfp =  s->comp()->getCurrentMethod()->isStrictFP() || s->comp()->getOption(TR_StrictFP);


   if ((loadConstIsPowerOfTwoDbl || loadConstIsPowerOfTwoFloat) && !isStrictfp)
      return true;
   else if (traceIt)
      traceMsg(s->comp(), "Fails because not power of 2 or !strict\n",mulConstChild);

   if (traceIt) traceMsg(s->comp(), "Fails because strict, no const or bitcount exceeded\n");
   return false;
   }

// Convert a branch that uses bitwise operators into proper logical
// control flow.
//
// FIXME: currently handles only binary bitwise ops
// could be generalized to if's of the form 'if ( c1 | c2 | c3 | ... | cn)'
// would need to convert the expression to conjunctive or disjunctive normal
// form, and insert a loop in the following method.
//
static void bitwiseToLogical(TR::Node * node, TR::Block * block, TR::Simplifier *s)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   if (opCode != TR::ificmpne && opCode != TR::ificmpeq)
      return;
   // the first child must be an ior or an iand
   TR::Node * firstChild = node->getFirstChild();
   if (firstChild->getOpCodeValue() != TR::ior &&
       firstChild->getOpCodeValue() != TR::iand)
      return;
   if (firstChild->getReferenceCount() != 1)
      return;
   if (firstChild->getFirstChild() ->getOpCodeValue() != TR::b2i)
      return;
   if (firstChild->getSecondChild()->getOpCodeValue() != TR::b2i)
      return;
   TR::Node   * cmp1 = firstChild->getFirstChild ()->getFirstChild();
   TR::Node   * cmp2 = firstChild->getSecondChild()->getFirstChild();
   if (!cmp1->getOpCode().isBooleanCompare())
      return;
   if (!cmp2->getOpCode().isBooleanCompare())
      return;

   // second child must be int
   TR::Node * secondChild = node->getSecondChild();
   if (secondChild->getOpCodeValue() != TR::iconst)
      return;
   // second child must be 0 or 1
   int32_t konst = secondChild->getInt();
   if (konst != 0 && konst != 1)
      return;

   TR::Block * nextBlock = block->getNextBlock();

   // FIXME: We will return if the next block extends the current block to ensure that it
   // does not have references to the current block.
   // The smarter way is to uncommon all the references in the next block referencing the
   // current block. The class TR_HandleInjectedBasicBlocks in the inliner will be useful
   //
   if (nextBlock->isExtensionOfPreviousBlock())
      return;

   if (!performTransformation(s->comp(), "%sConvert comparison with bitwise ops [" POINTER_PRINTF_FORMAT "] to logical control flow\n", s->optDetailString(), node))
      return;

   TR::CFG      * cfg  = s->comp()->getFlowGraph();
   bool isZero       = (secondChild->getInt() == 0);
   bool disjunctive  = (firstChild ->getOpCodeValue() == TR::ior);
   TR::TreeTop  * dest = node->getBranchDestination();
   TR::Block * destBlock = dest->getNode()->getBlock();

   // imaginary normalization
   if (node->getOpCodeValue() == TR::ificmpne)
      isZero = !isZero;

   // fixup current tree to do the first compare
   TR::Node::recreate(node, (disjunctive) ?
                        cmp1->getOpCode().convertCmpToIfCmp() :
                        TR::ILOpCode(cmp1->getOpCode().getOpCodeForReverseBranch()).convertCmpToIfCmp());
   node->setAndIncChild(0, cmp1->getFirstChild ());
   node->setAndIncChild(1, cmp1->getSecondChild());

   // create temps to be used in ze next block
   TR::TreeTop * prevTree = block->getLastRealTreeTop()->getPrevTreeTop();
   TR::SymbolReference * temp1 = s->comp()->getSymRefTab()->
      createTemporary(s->comp()->getMethodSymbol(), cmp2->getFirstChild()->getDataType());
   prevTree = TR::TreeTop::create(s->comp(), prevTree, TR::Node::createStore(temp1, cmp2->getFirstChild()));
   TR::SymbolReference * temp2 = s->comp()->getSymRefTab()->
      createTemporary(s->comp()->getMethodSymbol(), cmp2->getSecondChild()->getDataType());
   prevTree = TR::TreeTop::create(s->comp(), prevTree, TR::Node::createStore(temp2, cmp2->getSecondChild()));

   // create new block with the second compare in it
   TR::ILOpCodes newOp = ((isZero) ?
                         TR::ILOpCode(cmp2->getOpCode().getOpCodeForReverseBranch()).convertCmpToIfCmp() :
                         TR::ILOpCode(cmp2->getOpCodeValue()).convertCmpToIfCmp());
   TR::Node * newCmp = TR::Node::createif(newOp,
                                       TR::Node::createLoad(cmp2->getFirstChild (), temp1),
                                       TR::Node::createLoad(cmp2->getSecondChild(), temp2));

   newCmp->setBranchDestination(dest);
   TR::Block * newBlock = TR::Block::createEmptyBlock(cmp2, s->comp(), -1, nextBlock);

   //FIXME: Currently the nextBlock must not extent the current block
   //if (nextBlock->isExtensionOfPreviousBlock())
   //   newBlock->setIsExtensionOfPreviousBlock();

   newBlock->append(TR::TreeTop::create(s->comp(), newCmp));
   cfg->addNode(newBlock, nextBlock->getParentStructureIfExists(cfg));
   cfg->addEdge(newBlock, nextBlock);
   cfg->addEdge(newBlock, destBlock);

   // add the new block between block and nextBlock
   block->getExit()->join(newBlock->getEntry());
   newBlock->getExit()->join(nextBlock->getEntry());
   cfg->addEdge   (block, newBlock );
   if ((disjunctive && !isZero) || (!disjunctive && isZero))
      {
      s->_blockRemoved |= cfg->removeEdge(block, nextBlock);
      }
   else
      {
      node->setBranchDestination(nextBlock->getEntry());
      //nextBlock->setIsExtensionOfPreviousBlock(false);
      // FIXME: check if there may be things commoned that are not so any longer
      s->_blockRemoved |= cfg->removeEdge(block, destBlock);
      }

   firstChild->recursivelyDecReferenceCount();
   secondChild->decReferenceCount();
   }

static bool skipRemLoweringForPositivePowersOfTen(TR::Simplifier *s)
   {
   return false;
   }

static bool skipRemLowering(int64_t divisor, TR::Simplifier *s)
   {
   return false;
   }


/** \brief
 *     Reduce ifxcmpge to ifxcmpeq if possible to simplify the trees so that a code generator may have the opportunity
 *     to carry out in-memory bit tests to evaluate the respective tree.
 *
 *  \details
 *     The transformation is only performed under the following conditions:
 *
 *     - the first child is logical AND node
 *     - the second child of the logical AND node is a constant N
 *     - the second child of the ifxcmpge node is a constant N,
 *     - either the comparison is unsigned or N >= 0
 *
 *     If all conditions are satisfied then ifxcmpge can be reduced to ifxcmpeq.
 *
 *  \example
 *     <code>
 *     (0) ificmpge --> block_5
 *     (?)   iand
 *     (?)     iload y
 *     (?)     iconst 16
 *     (?)   iconst 16
 *     </code>
 *
 *     Can be reduced to:
 *
 *     <code>
 *     (0) ificmpeq --> block_5
 *     (?)   iand
 *     (?)     iload y
 *     (?)     iconst 16
 *     (?)   iconst 16
 *     </code>
 *
 *     Since the constants are equal, the case of greater-than is impossible, and we can reduce ificmpge to ificmpeq.
 *     In other words, the result of iand cannot be greater than the constant, and we can simply perform an equality
 *     check, which provides further optimization opportunities.
 *
 *  \note
 *     The following special case. We need to check that either the comparison is unsigned, or the mask has the high /
 *     sign bit clear. Otherwise it's possible for the result of iand to compare greater. For example, consider the
 *     following tree:
 *
 *     <code>
 *     (0) ificmpge --> block_7
 *     (?)   iand
 *     (?)     iload y
 *     (?)     iconst -2
 *     (?)   iconst -2
 *     </code>
 *
 *     If <c>y</c> is non-negative, then the result of iand will also be non-negative, and therefore greater than -2.
 */
class IfxcmpgeToIfxcmpeqReducer
   {
   public:

   IfxcmpgeToIfxcmpeqReducer(TR::Simplifier* simplifier, TR::Node* node)
   :
      _simplifier(simplifier), _node(node)
   {}

   /** \brief
    *     Determines whether an ifxcmpge node can be reduced to ifxcmpeq.
    *
    *  \return
    *     <c>true</c> if the reduction is possible; <c>false</c> otherwise.
    */
   bool isReducible()
      {
      bool result = false;

      switch (_node->getOpCodeValue())
         {
         case TR::ifbcmpge:
            result =  isReducibleSpecific<int8_t>();
            break;
         case TR::ifbucmpge:
            result =  isReducibleSpecific<uint8_t>();
            break;

         case TR::ifscmpge:
            result =  isReducibleSpecific<int16_t>();
            break;
         case TR::ifsucmpge:
            result =  isReducibleSpecific<uint16_t>();
            break;

         case TR::ificmpge:
            result =  isReducibleSpecific<int32_t>();
            break;
         case TR::ifiucmpge:
            result =  isReducibleSpecific<uint32_t>();
            break;

         case TR::iflcmpge:
            result =  isReducibleSpecific<int64_t>();
            break;
         case TR::iflucmpge:
            result =  isReducibleSpecific<uint64_t>();
            break;

         case TR::ifbcmple:
         case TR::ifbucmple:
         case TR::ifscmple:
         case TR::ifsucmple:
         case TR::ificmple:
         case TR::ifiucmple:
         case TR::iflcmple:
         case TR::iflucmple:
            // earlier xforms to reverse children may bring these opcodes here
            // do not reduce in this case
            result = false;
            break;

         default:
            TR_ASSERT(0, "unexpected opcode provided to isReducible()");
            break;
         }

         return result;
      }

   /** \brief
    *     Perform the reduction.
    *
    *  \return
    *     The transformed node if a reduction was performed.
    */
   TR::Node* reduce()
      {
      if (performTransformation(_simplifier->comp(), "%sReduce an ifxcmpge node [%p] to ifxcmpeq\n", _simplifier->optDetailString(), _node))
         {
         TR::ILOpCodes ifcmpeqOpCode = TR::ILOpCode::ifcmpeqOpCode(_node->getSecondChild()->getDataType());
         TR::Node::recreate(_node, ifcmpeqOpCode);
         }

      return _node;
      }

   private:

   /** \brief
    *     The actual implementation of `isReducible` for an exact type.
    */
   template<typename T>
   bool isReducibleSpecific() const
      {
      TR::Node* lhsNode = _node->getFirstChild();
      TR::Node* rhsNode = _node->getSecondChild();

      if (lhsNode->getOpCode().isAnd() && lhsNode->getSecondChild()->getOpCode().isIntegralConst() &&
            rhsNode->getOpCode().isIntegralConst())
         {
         T andConst = lhsNode->getSecondChild()->getConst<T>();

         if (andConst == rhsNode->getConst<T>() && (_node->getOpCode().isUnsignedCompare() || andConst >= 0))
            {
            return true;
            }
         }

      return false;
      }

   private:

   TR::Simplifier* _simplifier;

   TR::Node* _node;
   };


/*
 * Simplifier handlers:
 * Each handler function corresponds to an entry in the simplifierOpts table,
 * can handle one or more opcodes
 */

//---------------------------------------------------------------------
// Default simplification, does nothing except simplify the children
//
TR::Node *dftSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (node->getOpCode().isBranch() && (removeIfToFollowingBlock(node, block, s) == NULL))
      return NULL;
   simplifyChildren(node, block, s);
   return node;
   }

TR::Node *lstoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return node;
   }

TR::Node *directLoadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s); // For all those direct loads with child nodes...
   TR::TransformUtil::transformDirectLoad(s->comp(), node);
   return node;
   }

TR::Node *indirectLoadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (node->getOpCode().isLoadIndirect())
      node->getFirstChild()->setIsNonNegative(true);

   simplifyChildren(node, block, s);

   // Give the FrontEnd the first shot at this
   //
   TR::Node *transformed = TR::TransformUtil::transformIndirectLoad(s->comp(), node);
   if (transformed)
      {
      if (node == transformed)
         return node;
      else
         return s->replaceNode(node, transformed, s->_curTree);
      }

   TR::Node *resultNode = s->simplifyIndirectLoadPatterns(node);
   if (resultNode != NULL)
      return resultNode;

   TR::Node *firstChild = node->getFirstChild();
   if (firstChild->getOpCodeValue() == TR::loadaddr)
       {
       bool newOType = false;
       TR::DataType loadDataType = newOType ? node->getDataType() : node->getSymbolReference()->getSymbol()->getDataType();
       intptrj_t loadSize = newOType ? node->getSize() : node->getSymbolReference()->getSymbol()->getSize();
       intptrj_t loadSymSize = node->getSymbolReference()->getSymbol()->getSize();

       TR::DataType addrDataType = firstChild->getSymbolReference()->getSymbol()->getDataType();
       intptrj_t addrSize = firstChild->getSymbolReference()->getSymbol()->getSize();
       TR::Symbol* addrSymbol = firstChild->getSymbolReference()->getSymbol();
       bool localStatic = false;

       TR::ILOpCodes convOpCode = TR::ILOpCode::getProperConversion(addrDataType, loadDataType, false);


      bool isSame = loadDataType == addrDataType;
      if (addrDataType == TR::Aggregate && loadSize != addrSize)
         isSame = false;

      bool allowLoadSize = false;
      if (newOType)
         allowLoadSize = (loadSize == loadSymSize) && (loadSize == 1 || loadSize == 2 || loadSize == 4 || loadSize == 8);
      else
         allowLoadSize = (loadSize == TR::Compiler->om.sizeofReferenceAddress());

      if (isSame &&
          (node->getSymbol()->getSize() == firstChild->getSymbol()->getSize()) &&
          (firstChild->getSymbolReference()->getSymbol()->isAutoOrParm() || localStatic) &&
          node->getSymbolReference()->getOffset() == 0 &&
          (node->getSymbol()->isVolatile() == firstChild->getSymbol()->isVolatile()) &&
          performTransformation(s->comp(), "%sReplace indirect load %s [" POINTER_PRINTF_FORMAT "] with ",
          s->optDetailString(), node->getOpCode().getName(),node))
         {
         TR::SymbolReference *childRef = firstChild->getSymbolReference();
         TR_ASSERT(childRef, "Unexpected null symbol reference on child node\n");

         TR::Node *loadNode = node;
         if (loadDataType != addrDataType)
            {
            uint8_t precision = node->getType().isAddress() ? (TR::Compiler->target.is64Bit() ? 8 : 4) : 0;
            TR::Node::recreateWithoutProperties(node, convOpCode, 1);

            loadNode = TR::Node::create(node, TR::BadILOp, 0);
            node->setAndIncChild(0, loadNode);
            }

         if (!newOType && addrDataType == TR::Aggregate && node->getDataType() != TR::Aggregate)
            addrDataType = node->getDataType();

         TR::Node::recreateWithoutProperties(loadNode, s->comp()->il.opCodeForDirectLoad(addrDataType), 0, childRef);

         dumpOptDetails(s->comp(),"%s [" POINTER_PRINTF_FORMAT "] (load %s [" POINTER_PRINTF_FORMAT "])\n",
             node->getOpCode().getName(), node, loadNode->getOpCode().getName(), loadNode);

         firstChild->recursivelyDecReferenceCount();

         if (addrDataType == TR::Aggregate)
            {
            return s->simplify(node, block);
            }

         return node;
         }
      }

   if (s->comp()->cg()->getSupportsVectorRegisters())
      {
      TR::Node *addressNode = node->getFirstChild();

      // Can convert to getvelem if trying to load a scalar from a vector
      if ((node->getType().isIntegral() || node->getType().isDouble()) &&
          ((addressNode->getOpCode().isArrayRef() && addressNode->getSecondChild()->getOpCode().isLoadConst() &&
           addressNode->getFirstChild()->getOpCode().hasSymbolReference() && addressNode->getFirstChild()->getSymbol()->getType().isVector()) ||
          (addressNode->getOpCode().hasSymbolReference() && addressNode->getSymbol()->getType().isVector())) &&
          performTransformation(s->comp(), "%sReplace indirect load [" POINTER_PRINTF_FORMAT "] with getvelem", s->optDetailString(), node))
         {
         int32_t offset = 0;
         TR::SymbolReference *oldSymRef = node->getSymbolReference();

         if (addressNode->getOpCode().isArrayRef())
            {
            offset = addressNode->getSecondChild()->get64bitIntegralValue();
            addressNode = addressNode->getFirstChild();
            }
         else
            {
            offset = oldSymRef->getOffset();
            }

         TR::SymbolReference *newSymRef = s->comp()->getSymRefTab()->createSymbolReference(
               TR::Symbol::createShadow(s->comp()->trHeapMemory(), addressNode->getSymbol()->getDataType()), 0);
         TR::Node *vloadiNode = TR::Node::createWithSymRef(TR::vloadi, 1, 1, addressNode, newSymRef);
         TR::Node *indexNode = TR::Node::iconst(offset/(node->getSize()));

         TR::Node *getvelemNode = TR::Node::create(TR::getvelem, 2, vloadiNode, indexNode);

         dumpOptDetails(s->comp(),"[" POINTER_PRINTF_FORMAT "]\n", getvelemNode);

         s->replaceNode(node, getvelemNode, s->_curTree);
         return s->simplify(getvelemNode, block);
         }
      }

   return node;
   }

TR::Node *indirectStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (node->getOpCode().isStoreIndirect())
      node->getFirstChild()->setIsNonNegative(true);

   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   // Check if we are storing an address of a deleted label
   if (isDeletedLabelLoadaddr(secondChild) &&
       performTransformation(s->comp(), "%sRemoving indirect store [" POINTER_PRINTF_FORMAT "] to deleted label [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node, secondChild->getSymbol()))
      {
      s->removeNode(node, s->_curTree);
      return NULL;
      }

   if (firstChild->getOpCodeValue() == TR::loadaddr)
       {
       bool newOType = false;

       TR::DataType storeDataType = newOType ? node->getDataType() : node->getSymbolReference()->getSymbol()->getDataType();
       intptrj_t storeSize = newOType ? node->getSize() : node->getSymbolReference()->getSymbol()->getSize();
       intptrj_t storeSymSize = node->getSymbolReference()->getSymbol()->getSize();

       TR::DataType addrDataType = firstChild->getSymbolReference()->getSymbol()->getDataType();
       intptrj_t addrSize = firstChild->getSymbolReference()->getSymbol()->getSize();
       TR::Symbol *addrSymbol = firstChild->getSymbolReference()->getSymbol();

       bool isSameType = true;
       bool localStatic = false;

       TR::ILOpCodes convOpCode = TR::ILOpCode::getProperConversion(storeDataType, addrDataType, false);

         bool allowStoreSize = false;
         if (newOType)
            allowStoreSize = (storeSize == storeSymSize) && (storeSize == 1 || storeSize == 2 || storeSize == 4 || storeSize == 8);
         else
            allowStoreSize = (storeSize == TR::Compiler->om.sizeofReferenceAddress());

       if ((storeDataType == addrDataType && isSameType) &&
           (firstChild->getSymbolReference()->getSymbol()->isAutoOrParm()  || localStatic) &&
           node->getSymbolReference()->getOffset() == 0 &&
           (node->getSymbol()->isVolatile() == firstChild->getSymbol()->isVolatile()) &&
           performTransformation(s->comp(), "%sReplace indirect store %s [" POINTER_PRINTF_FORMAT "] with ",
            s->optDetailString(), node->getOpCode().getName(),node))
         {

         if (storeDataType != addrDataType)
            {
            TR::Node *conv = TR::Node::create(node, convOpCode, 1);
            conv->setChild(0, secondChild);
            node->setAndIncChild(0, conv);
            }
         else
            {
            node->setChild(0, secondChild);
            }

         TR::Node::recreate(node, s->comp()->il.opCodeForDirectStore(addrDataType));
         node->setSymbolReference(firstChild->getSymbolReference());
         firstChild->recursivelyDecReferenceCount();
         node->setNumChildren(1);

         dumpOptDetails(s->comp(),"%s [" POINTER_PRINTF_FORMAT "]\n",node->getOpCode().getName(),node);

         if (addrDataType == TR::Aggregate)
            {
            return s->simplify(node, block);
            }
         return node;
         }
      }

   if ((node->getOpCodeValue() == TR::wrtbari) && 0 &&
       !node->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
      {
      TR::Node *valueChild = node->getSecondChild();
      if ((valueChild->getOpCodeValue() == TR::New) ||
          (valueChild->getOpCodeValue() == TR::newarray) ||
          (valueChild->getOpCodeValue() == TR::anewarray) ||
          (valueChild->getOpCodeValue() == TR::MergeNew) ||
          (valueChild->getOpCodeValue() == TR::multianewarray))
         {
         bool seenGCPoint = false;
         bool removeWrtBar = false;
         TR::TreeTop *cursor = s->_curTree;
         while (cursor)
            {
            TR::Node *cursorNode = cursor->getNode();

            if (cursorNode->getOpCodeValue() == TR::BBStart)
               break;

            if (cursorNode->getOpCodeValue() == TR::treetop)
               {
               if (cursorNode->getFirstChild() == valueChild)
                  {
                  if (seenGCPoint)
                     {
                     removeWrtBar = false;
                     break;
                     }
                  else
                     removeWrtBar = true;
                  }
               }

            if (cursorNode->canGCandReturn())
               seenGCPoint = true;

            cursor = cursor->getPrevTreeTop();
            }

         if (removeWrtBar && !s->comp()->getOptions()->realTimeGC() &&
             performTransformation(s->comp(), "%sFolded indirect write barrier to iastore because GC could not have occurred enough times to require iwrtbar [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::astorei);
            node->getChild(2)->recursivelyDecReferenceCount();
            node->setNumChildren(2);
            }
         }
      }

   if (s->comp()->cg()->getSupportsVectorRegisters())
      {
      TR::Node *addressNode = node->getFirstChild();
      TR::Node *valueNode = node->getSecondChild();

      // Can convert to vsetelem if it is a scalar store to a vector, or it's an array ref to a vector with a constant offset
      if ((node->getType().isIntegral() || node->getType().isDouble()) &&
          (!valueNode->getOpCode().isLoadConst() || s->getLastRun()) &&
          ((addressNode->getOpCode().isArrayRef() && addressNode->getSecondChild()->getOpCode().isLoadConst() &&
           addressNode->getFirstChild()->getOpCode().hasSymbolReference() && addressNode->getFirstChild()->getSymbol()->getType().isVector()) ||
          (addressNode->getOpCode().hasSymbolReference() && addressNode->getSymbol()->getType().isVector())) &&
          performTransformation(s->comp(), "%sReplace indirect store [" POINTER_PRINTF_FORMAT "] with vsetelem", s->optDetailString(), node))
         {
         int32_t offset = 0;
         TR::SymbolReference *oldSymRef = node->getSymbolReference();

         if (addressNode->getOpCode().isArrayRef())
            {
            offset = addressNode->getSecondChild()->get64bitIntegralValue();
            addressNode = addressNode->getFirstChild();
            }
         else
            {
            offset = oldSymRef->getOffset();
            }

         TR::SymbolReference *newSymRef = s->comp()->getSymRefTab()->createSymbolReference(TR::Symbol::createShadow(s->comp()->trHeapMemory(), addressNode->getSymbol()->getDataType()), 0);
         TR::Node *vloadiNode = TR::Node::createWithSymRef(TR::vloadi, 1, 1, addressNode, newSymRef);
         TR::Node *indexNode = TR::Node::iconst(offset/(node->getSize()));   // idx = byte offset / element size
         TR::Node *vsetelemNode = TR::Node::create(TR::vsetelem, 3, vloadiNode, indexNode, valueNode);
         auto newVStorei = TR::Node::createWithSymRef(TR::vstorei, 2, 2, addressNode, vsetelemNode, newSymRef);
         dumpOptDetails(s->comp(),"[" POINTER_PRINTF_FORMAT "]\n", newVStorei);
         s->replaceNode(node, newVStorei, s->_curTree);
         newVStorei->setReferenceCount(0);
         return s->simplify(newVStorei, block);
         }
      }

   return node;
   }

TR::Node *astoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   // Check if we are storing an address of a deleted label
   if (isDeletedLabelLoadaddr(node->getFirstChild()) &&
       performTransformation(s->comp(), "%sRemoving astore [" POINTER_PRINTF_FORMAT "] to deleted label [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node, node->getFirstChild()->getSymbol()))
      {
      s->removeNode(node, s->_curTree);
      return NULL;
      }
   return node;
   }

TR::Node *directStoreSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * child = node->getFirstChild();
   TR::SymbolReference * symRef = node->getSymbolReference();
   if (child->getOpCode().isLoadVar()  &&
       child->getReferenceCount() == 1 &&
       symRef == child->getSymbolReference() &&
       performTransformation(s->comp(), "%sFolded direct store of load of same symbol on node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
      {
      node->recursivelyDecReferenceCount();
      s->_invalidateUseDefInfo = true;
      s->_alteredBlock = true; // this will invalidate the valueNumberInfo
      return NULL;
      }

   // pattern match on update form:
   // Ystore <sym>
   //    Yadd/Ysub
   //       Yload <sym>/Yconst X
   //       Yconst X/Yload <sym>
   // where reference count on the Yload child is > 1
   // don't really care what Y and X are for this pattern match
   // when we find an update form where the Yload has ref count > 1, we'd like to swing this tree down below the node
   // that uses it to avoid ugly register copying (because both the original and incremented value are required)
   // NOTE: this likely won't improve register pressure; it's purely a codegen quality improvement
   else if (child->getOpCode().isAdd() || child->getOpCode().isSub())
      {
      TR::Node * firstGrandchild = child->getFirstChild();
      TR::Node * secondGrandchild = child->getSecondChild();
      bool firstLoadsSymRef = (firstGrandchild->getOpCode().isLoadVar() && symRef == firstGrandchild->getSymbolReference()); // && firstGrandchild->getReferenceCount() > 1);
      bool firstIsLoadConst = (firstGrandchild->getOpCode().isLoadConst());
      bool secondLoadsSymRef = (secondGrandchild->getOpCode().isLoadVar() && symRef == secondGrandchild->getSymbolReference()); // && secondGrandchild->getReferenceCount() > 1);
      bool secondIsLoadConst = (secondGrandchild->getOpCode().isLoadConst());

      if ((firstLoadsSymRef && secondIsLoadConst) || (firstIsLoadConst && secondLoadsSymRef))
         {
         // found a candidate update form tree: now walk trees forward until either something stores to <sym> or end of block
         // remember tree where last use of node was on this walk: insert our update tree following that last use tree

         // first, find the actual update tree (this is obviously our first potential insertion point for the update tree since it's already here)
         TR::TreeTop *updateTree = block->getEntry();
         while (updateTree->getNode() != node)
            {
            updateTree = updateTree->getNextRealTreeTop();

            // if we reach the end of the block, then the update tree is probably under a ResolveCHK node and we don't really want to move that
            if (updateTree == block->getExit())
               return node;
            }

         //dumpOptDetails(s->comp(), "found update form treetop [" POINTER_PRINTF_FORMAT "] node [" POINTER_PRINTF_FORMAT "] for symbol #%d[" POINTER_PRINTF_FORMAT "]\n", updateTree->getNode(), node, symRef->getReferenceNumber());


         // only proceed if previous tree is a treetop holding an iload of <sym> (more specific pattern match)
         TR::TreeTop * loadTree = updateTree->getPrevRealTreeTop();
         if (loadTree->getNode()->getOpCodeValue() == TR::treetop)
            {
            TR::Node * loadNode = loadTree->getNode()->getFirstChild();
            if (loadNode->getOpCode().isLoadVarDirect() && loadNode->getSymbolReference() == symRef)
               {
               //dumpOptDetails(s->comp(), "found load [" POINTER_PRINTF_FORMAT "] of symbol in previous tree\n", loadNode);

               // now, walk trees forward to end of block or a tree that accesses <sym>
               // when we find a tree containing a use of <sym>, update insertion point with tree following the use tree
               TR::TreeTop *walkingTree = updateTree->getNextRealTreeTop();
               TR::TreeTop * insertionTree = NULL;
               bool blockHasCatchers = block->hasExceptionSuccessors();
               while (walkingTree != block->getExit())
                  {
                  if (!canMovePastTree(walkingTree, symRef, s->comp(), blockHasCatchers))
                     break;

                  if (containsNode(walkingTree->getNode(), loadNode, s->comp()->getVisitCount()))
                     insertionTree = walkingTree;

                  walkingTree = walkingTree->getNextRealTreeTop();
                  }

               // if we found a better insertion point, snip tree out of its original place and insert it after insertionTree
               if (insertionTree != NULL && performTransformation(s->comp(), "%smove update tree [" POINTER_PRINTF_FORMAT "] to after [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node, insertionTree->getNode()))
                  {
                  // make sure we still simplify trees following the original update tree's position
                  s->_curTree = updateTree->getNextRealTreeTop();

                  node->setVisitCount(s->comp()->getVisitCount());
                  TR::TreeTop * afterInsertion = insertionTree->getNextRealTreeTop();

                  // disconnect update tree
                  updateTree->getPrevRealTreeTop()->join(updateTree->getNextRealTreeTop());

                  // and re-connect at new insertion point
                  updateTree->join(insertionTree->getNextRealTreeTop());
                  insertionTree->join(updateTree);

                  // So we decided to swing this update tree below the load, but eliminateDeadTrees might subsequently try to undo it
                  // by moving the load below the update tree (bad pass!).  We'll allow it to do that (because we can come along and re-swing
                  // the update below the load) so long as it doesn't move the load under a branch tree.  If the load gets moved into a branch
                  // tree, then there's no place we could put the update tree that's below the load but still in the block
                  // (executive summary: all our careful work above would be for naught).  So mark the load here so that
                  // eliminateDeadTrees won't make us look (as?) bad.
                  loadNode->setIsDontMoveUnderBranch(true);

                  //s->comp()->dumpMethodTrees("Trees after swing");
                  }
               }
            }
         }
      }

   return node;
   }

TR::Node *vsetelemSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   // try to simplify multiple vsetelems to vsplats if they are storing the same value to the same vector
   TR::SparseBitVector seenIndex(s->comp()->allocator());
   TR::Node *valueNode = node->getThirdChild();

   // Catches patterns like this:
   // vstore <vec>
   //    vsetelem (VectorDouble)
   //       vsetelem (VectorDouble)
   //          vload <vec>
   //          iconst 0
   //          dload <a>
   //    iconst 1
   //    ==> dload
   // Since the value being set is the same for all elements in the vector, we can convert it into a single vsplats

   for (TR::Node *vsetelemNode = node; vsetelemNode && vsetelemNode->getOpCodeValue() == TR::vsetelem; vsetelemNode = vsetelemNode->getFirstChild())
      {
      // Can't determine which element is being stored
      if (!vsetelemNode->getSecondChild()->getOpCode().isLoadConst())
         break;

      // If we've seen this index before, then the outer set element overrides the inner set element, so it's safe to skip this node
      int8_t index = vsetelemNode->getSecondChild()->get64bitIntegralValue();
      if (seenIndex[index])
         continue;

      // If the value node isn't the same in this set element, we can't splat it
      if (valueNode != vsetelemNode->getThirdChild())
         break;

      seenIndex[index] = true;
      }

   // Can convert to a splat if we've seen 16 bytes worth of vsetelems
   if ((seenIndex.PopulationCount() * valueNode->getSize()) == 16)
      {
      TR::Node *vsplatsNode = TR::Node::create(TR::vsplats, 1, valueNode);
      s->replaceNode(node, vsplatsNode, s->_curTree);
      return s->simplify(vsplatsNode, block);
      }

   simplifyChildren(node, block, s);

   return node;
   }

TR::Node *v2vSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (node->getDataType() == node->getFirstChild()->getDataType())
      {
      node = s->replaceNode(node, node->getFirstChild(), s->_curTree);
      return s->simplify(node, block);
      }

   return node;
   }

TR::Node *gotoSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return _gotoSimplifier(node, block, s->_curTree, s);
   }

TR::Node *ifdCallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return s->simplifyiCallMethods(node, block);
   }

TR::Node *lcallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return s->simplifylCallMethods(node, block);
   }

TR::Node *acallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return s->simplifyaCallMethods(node, block);
   }

TR::Node *vcallSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return node;
   }

TR::Node *treetopSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   // The treetop node should have a single child.
   //
   TR_ASSERT(node->getNumChildren() == 1, "Simplifier, bad treetop node %p", node);
   TR::Node * child = node->getFirstChild();
   child->decFutureUseCount();
   if (child->getVisitCount() != s->comp()->getVisitCount())
      {
      child = s->simplify(child, block);

      // If the child is null, this treetop can be removed
      //
      if (child == NULL)
         {
         s->prepareToStopUsingNode(node, s->_curTree);
         return NULL;
         }
      node->setFirst(child);
      }

   if ((s->comp()->useCompressedPointers() &&
         child->getOpCode().isStore() &&
         child->getType().isAddress() &&
         child->getReferenceCount() > 1))
      {
      // if the child is a store, then compressedRefs may still refer to it
      // so cannot remove it

      // compjazz 54194: This code makes the store and all its children swing down, which is illegal.
      }
   else if (child->getOpCode().isStore() && !child->getOpCode().isWrtBar())
      {
      TR::Node *sNode = s->replaceNode(node, child, s->_curTree);
      sNode->setReferenceCount(0);
      return sNode;
      }

   return node;
   }

// for anchors: compressed pointers
TR::Node *anchorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (!s->comp()->useAnchors())
      return node;

   // if the child of an anchor is not an indirect load/store
   // then the anchor is redundant
   //
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   if (!firstChild->getOpCode().isStoreIndirect() &&
         !firstChild->getOpCode().isLoadIndirect())
      {
      if (performTransformation(s->comp(), "%sRemoving anchor node %p\n", s->optDetailString(), node))
         {
         if (firstChild->getOpCode().isStore() &&
               firstChild->getReferenceCount() == 1)
            {
            if (!firstChild->getOpCode().isWrtBar())
               {
               ///dumpOptDetails(s->comp(), "came in here for node %p first %p\n", node, firstChild);
               TR::Node *sNode = s->replaceNode(node, firstChild, s->_curTree);
               sNode->setReferenceCount(0);
               return sNode;
               }
            }
         else
            {
            TR::Node::recreate(node, TR::treetop);
            ////firstChild->decReferenceCount();
            secondChild->decReferenceCount();
            node->setNumChildren(1);
            ////return NULL;
            }
         }
      }
   return node;
   }

//---------------------------------------------------------------------
// Add simplifiers
//

TR::Node *iaddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::iuaddc)
      return node;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->chkClassPointerConstant())
      return node;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if (node->getOpCode().isUnsigned())
         {
         uint32_t value = firstChild->getUnsignedInt() + secondChild->getUnsignedInt();
         foldUIntConstant(node, value, s, false /* !anchorChildren*/);

         if (node->nodeRequiresConditionCodes())
            setCCAddUnsigned(value, firstChild->getUnsignedInt(), node, s);
         }
      else
         {
         int32_t value = firstChild->getInt() + secondChild->getInt();
         foldIntConstant(node, value, s, false /* !anchorChildren*/);

         if (node->nodeRequiresConditionCodes())
            setCCAddSigned(value, firstChild->getInt(), secondChild->getInt(), node, s);
         }
      return node;
      }

   if (!node->getOpCode().isRef())
      {
      orderChildren(node, firstChild, secondChild, s);
      }

   if (node->nodeRequiresConditionCodes())
      {
      return node;
      }

   BINARY_IDENTITY_OP(Int, 0)
   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();
   TR::ILOpCodes nodeOp        = node->getOpCodeValue();

   TR::Node * iMulComposerChild = NULL;
   int iMulComposerValue = 0;
   // normalize adds of positive constants to be isubs.  Have to do it this way because
   // more negative value than positive in a 2's complement signed representation
   if (nodeOp == TR::iadd          &&
       secondChildOp == TR::iconst &&
       secondChild->getInt() > 0)
      {
      if (performTransformation(s->comp(), "%sNormalized iadd of iconst > 0 in node [%s] to isub of -iconst\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * newSecondChild = (secondChild->getReferenceCount() == 1) ? secondChild : TR::Node::create(secondChild, TR::iconst, 0);
         newSecondChild->setInt(-secondChild->getInt());
         TR::Node::recreateWithoutProperties(node, TR::isub, 2, firstChild, newSecondChild);
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   else if (nodeOp == TR::iadd && firstChildOp == TR::ineg)
      {
      TR::Node * newFirstChild = firstChild->getFirstChild();
      // turn ineg +(-1) into bitwise complement
      if (secondChildOp == TR::iconst &&
          secondChild->getInt() == -1)
         {
         if (performTransformation(s->comp(), "%sReduced iadd of -1 and an ineg in node [%s] to bitwise complement\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            s->anchorChildren(node, s->_curTree);
            TR::Node::recreate(node, TR::ixor);
            node->setAndIncChild(0, newFirstChild);
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            node = s->simplify(node, block);
            s->_alteredBlock = true;
            }
         }
      // Turn add with first child negated into a subtract
      else if (performTransformation(s->comp(), "%sReduced iadd with negated first child in node [%s] to isub\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         s->anchorChildren(node, s->_curTree);
         TR::Node::recreate(node, TR::isub);
         node->setAndIncChild(1, newFirstChild);
         node->setChild(0, secondChild);
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         node = s->simplify(node, block);
         s->_alteredBlock = true;
         }
      }
   // turn add with other child negated in to a subtract
   else if (nodeOp == TR::iadd && secondChildOp == TR::ineg)
      {
      if (performTransformation(s->comp(), "%sReduced iadd with negated second child in node [%s] to isub\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         s->anchorChildren(node, s->_curTree);
         TR::Node * newSecondChild = secondChild->getFirstChild();
         TR::Node::recreate(node, TR::isub);
         node->setAndIncChild(1, newSecondChild);
         secondChild->recursivelyDecReferenceCount();
         s->_alteredBlock = true;
         node->setVisitCount(0);
         node = s->simplify(node, block);
         }
      }
   else if (imulComposer(node, &iMulComposerValue, &iMulComposerChild ))
      {
      bool iMulDecomposeReport = s->comp()->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition);
      if (iMulDecomposeReport)
         dumpOptDetails(s->comp(), "\nImul composition succeeded for a value of %d.\n ", iMulComposerValue);

      if (s->getLastRun() && s->comp()->cg()->codegenMulDecomposition((int64_t)iMulComposerValue)
          && performTransformation(s->comp(), "%sFactored iadd with distributed imul with a codegen decomposible constant in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * lChild = node->getFirstChild();
         TR::Node * rChild = node->getSecondChild();
         TR::Node * constNode = NULL;

         if (iMulDecomposeReport)
            dumpOptDetails(s->comp(), "Putting the node back to imul with %d, for node [%s]. \n", iMulComposerValue, iMulComposerChild->getName(s->getDebug()));
         TR::Node::recreate(node, TR::imul);

         node->setAndIncChild(0, iMulComposerChild);
         constNode = TR::Node::create(node, TR::iconst, 0, iMulComposerValue);
         node->setAndIncChild(1, constNode);

         lChild->recursivelyDecReferenceCount();
         rChild->recursivelyDecReferenceCount();
         }

      }
      // reduce imul operations by factoring
   else if (firstChildOp == TR::imul &&
            firstChild->getReferenceCount() == 1 &&
            secondChildOp == TR::imul             &&
            secondChild->getReferenceCount() == 1)
      {
      TR::Node * llChild     = firstChild->getFirstChild();
      TR::Node * lrChild     = firstChild->getSecondChild();
      TR::Node * rlChild     = secondChild->getFirstChild();
      TR::Node * rrChild     = secondChild->getSecondChild();
      TR::Node * factorChild = NULL;
      // moved this check down:
      // if (performTransformation(s->comp(), "%sFactored iadd with distributed imul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         if (llChild == rlChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            }
         else if (llChild == rrChild)
            {
            factorChild = llChild;
            secondChild->setChild(1, lrChild);
            }
         else if (lrChild == rlChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            }
         else if (lrChild == rrChild)
            {
            factorChild = lrChild;
            secondChild->setChild(1, llChild);
            }

         // if invariance info available, do not group invariant
         // and non-invariant terms together
         // also, avoid conflict with Rule 13
         TR_RegionStructure * region;
         if (factorChild &&
             !s->getLastRun()  &&
             (region = s->containingStructure()) &&
             (isExprInvariant(region, secondChild->getFirstChild()) !=
         isExprInvariant(region, secondChild->getSecondChild()) ||
              isExprInvariant(region, factorChild)))  // to avoid conflict with Rule 13
            {
            factorChild = NULL;
            }

         if (!factorChild ||
             !performTransformation(s->comp(), "%sFactored iadd with distributed imul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
       secondChild->setChild(0, rlChild);
       secondChild->setChild(1, rrChild);
            }
         else
            {
            TR::Node::recreate(node, TR::imul);
            node->setChild(0, factorChild)->decReferenceCount();
            TR::Node::recreate(secondChild, TR::iadd);
            firstChild->decReferenceCount();
            secondChild->setVisitCount(0);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node->setChild(1, iaddSimplifier(secondChild, block, s));
            }
         }
      }
   else if (nodeOp != TR::aiadd &&
            !((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
               (s->_curTree->getNode()->getNumChildren() > 0) &&
          s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
         (s->_curTree->getNode()->getFirstChild()->getLastChild() == node)) &&
            (firstChildOp == TR::iadd ||
             firstChildOp == TR::isub))
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCodeValue() == TR::iconst)
         {
         if (secondChildOp == TR::iconst)
            {
            if (!s->reassociate() && // use new rules
               performTransformation(s->comp(), "%sFound iadd of iconst with iadd or isub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               if (firstChild->getReferenceCount()>1)
                  {
                  // it makes sense to uncommon it in this situation
                  TR::Node * newFirstChild = TR::Node::create(node, firstChildOp, 2);
                  newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
                  newFirstChild->setAndIncChild(1, firstChild->getSecondChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(0,newFirstChild);
                  firstChild = newFirstChild;
                  }

               int32_t value  = secondChild->getInt();
               if (firstChildOp == TR::iadd)
                  {
                  value += lrChild->getInt();
                  }
               else
                  {
                  value -= lrChild->getInt();
                  }
               if (value > 0)
                  {
                  value = -value;
                  TR::Node::recreate(node, TR::isub);
                  }
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setInt(value);
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::iconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setInt(value);
                  secondChild->recursivelyDecReferenceCount();
                  }

               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         else if (!s->reassociate() && // use new rules
                  (firstChild->getReferenceCount() == 1) &&
                  performTransformation(s->comp(), "%sFound iadd of non-iconst with iadd or isub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            // move constants up the tree so they will tend to get merged together
            node->setChild(1, lrChild);
            firstChild->setChild(1, secondChild);
            TR::Node::recreate(node, firstChildOp);
            TR::Node::recreate(firstChild, TR::iadd);
            firstChild->setIsNonZero(false);
            firstChild->setIsZero(false);
            firstChild->setIsNonNegative(false);
            firstChild->setIsNonPositive(false);
            firstChild->setCannotOverflow(false);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   else if (nodeOp == TR::aiadd         &&
            firstChildOp == TR::aiadd   &&
            secondChildOp == TR::iconst &&
            // In rare cases, the base aload has a really large reference count
            // so make sure the count can be safely incremented
            firstChild->getFirstChild()->getReferenceCount() < (MAX_RCOUNT-16))
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if ((lrChild->getOpCodeValue() == TR::iconst) &&
          ((firstChild->getFutureUseCount() == 0) ||
           (!s->comp()->cg()->areAssignableGPRsScarce() &&
            (lrChild->getInt() == -1*secondChild->getInt()))))
         {
         if (performTransformation(s->comp(), "%sFound aiadd of iconst with aiadd x and iconst in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            int32_t value  = secondChild->getInt();
            value += lrChild->getInt();
            if (secondChild->getReferenceCount() == 1)
               {
               secondChild->setInt(value);
               }
            else
               {
               TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::iconst, 0);
               node->setAndIncChild(1, foldedConstChild);
               foldedConstChild->setInt(value);
               secondChild->recursivelyDecReferenceCount();
               }
            node->setAndIncChild(0, firstChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   else if ((node->getOpCodeValue() == TR::iadd) &&
            (node->getReferenceCount() == 1) && // iadd should be unique
            (node->getFirstChild()->getOpCodeValue() == TR::a2i) && // node is iadd, so it has two children
            (node->getFirstChild()->getReferenceCount() == 1) && // a2i should be unique
            (node->getSecondChild()->getOpCode().isLoadConst()) && // iadd second child should be const to be pushed down
            (node->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::aiadd) && // iadd -> a2i -> aiadd
            (node->getFirstChild()->getFirstChild()->getReferenceCount() == 1) // aiadd should be unique
           )
      {
      // Push down the iadd into the aiadd's second child:
      //
      // n1.3083n   (  0)      iadd (in *GPR_6512) (profilingCode )                                             [0x2BB38260] loc=[-1,2848,1153] rc=0 vc=631 vn=- sti=- udi=14723 nc=2 flg=0x80
      // n1.3081n   (  0)        a2i (in *GPR_6512) (profilingCode unneededConv )                               [0x2BB381C0] loc=[-1,2847,1153] rc=0 vc=631 vn=- sti=- udi=14723 nc=1 addr=4 flg=0x8080
      // n1.26322n  (  0)          aiadd (in *GPR_6512) (profilingCode X>=0 internalPtr )                       [0x374931DC] loc=[-1,2844,1153] rc=0 vc=1859 vn=- sti=- udi=14723 nc=2 addr=4 flg=0x8180
      // n1.3077n   (  2)            ==>aload (in GPR_6513) (profilingCode )
      // n1.3086n   (  0)            ==>iconst 692 (profilingCode X!=0 X>=0 )
      // n1.3082n   (  0)        iconst -24 (profilingCode X!=0 X<=0 )                                          [0x2BB38210] loc=[-1,2848,1153] rc=0 vc=631 vn=- sti=- udi=- nc=0 flg=0x284
      //
      // In this case, both are iconst and can be added together.
      //
      // Otherwise, new trees are required:
      //
      // iadd
      //   a2i
      //     aiadd
      //       addr
      //       int1
      //   iconst N
      //
      // to
      //
      // iadd
      //   a2i
      //     aiadd
      //       addr
      //       iadd
      //         int1
      //         iconst N
      //   iconst 0

      TR::Node * node_a2i = node->getFirstChild();
      TR::Node * node_aiadd = node_a2i->getFirstChild();

      if ((node_aiadd->getSecondChild()->getOpCodeValue() == TR::iconst) &&
          (node->getSecondChild()->getReferenceCount() == 1) && // iadd iconst should be unique
          (node_aiadd->getSecondChild()->getReferenceCount() == 1) && // aiadd iconst should be unique
          performTransformation(s->comp(), "%sPushing down iadd [%p] iconst [%p] into aiadd [%p] iconst [%p] below\n",
                s->optDetailString(), node, node->getSecondChild(), node_aiadd, node_aiadd->getSecondChild()))
         {
         // 1) both are iconst and can be added together.

         TR::Node * node_iadd_iconst = node->getSecondChild();

         dumpOptDetails(s->comp(), "    (iconst [%p] + iconst [%p])\n", node_iadd_iconst, node_aiadd->getSecondChild());

         int64_t v = node_iadd_iconst->get64bitIntegralValue() + node_aiadd->getSecondChild()->get64bitIntegralValue();
         node_aiadd->getSecondChild()->set64bitIntegralValue(v);
         node->getSecondChild()->set64bitIntegralValue(0);
         }
      else
         {
         // 2) aiadd integer child is not an iconst, so new trees are required.
         bool passesPrecisionTest = true;


         if (passesPrecisionTest &&
             performTransformation(s->comp(), "%sPushing down iadd [%p] iconst [%p] into aiadd [%p] second child [%p], creating new iadd\n",
                   s->optDetailString(), node, node->getSecondChild(), node_aiadd, node_aiadd->getSecondChild()))
            {
            dumpOptDetails(s->comp(), "    (creating iadd of [%p] + [%p])\n", node->getSecondChild(), node_aiadd->getSecondChild());

            // handle arbitrary integer nodes by using an iadd to add them together
            TR::Node * new_aiadd_integer_node = TR::Node::create(node, TR::iadd, 2);
            new_aiadd_integer_node->setChild(0, node->getSecondChild());
            new_aiadd_integer_node->setChild(1, node_aiadd->getSecondChild());
            node_aiadd->setAndIncChild(1, new_aiadd_integer_node);

            node->setAndIncChild(1, TR::Node::create(node, TR::iconst, 0, 0));
            }
         }
      }

   addSimplifierJavaPacked(node, block, s);
   return addSimplifierCommon(node, block, s);
   }

TR::Node *laddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::luaddc)
      return node;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->chkClassPointerConstant())
      return node;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst() &&
       performTransformation(s->comp(), "%sSimplified ladd in node [" POINTER_PRINTF_FORMAT "] to lconst\n", s->optDetailString(), node))
      {
      if (node->nodeRequiresConditionCodes())
         {
         setCCAddSigned(firstChild->getLongInt() + secondChild->getLongInt(),
                        firstChild->getLongInt(), secondChild->getLongInt(), node, s);
         }

      int64_t val1 = (firstChild->getDataType() == TR::Address) ? firstChild->getAddress() : firstChild->getLongInt();
      int64_t val2 = (secondChild->getDataType() == TR::Address) ? secondChild->getAddress() : secondChild->getLongInt();
      foldLongIntConstant(node, val1 + val2, s, false /* !anchorChildren */);
      if (node->getOpCodeValue() == TR::aladd)
         TR::Node::recreate(node, TR::aconst);

      return node;
      }

   if (!node->getOpCode().isArrayRef())
      {
      orderChildren(node, firstChild, secondChild, s);
      orderChildrenByHighWordZero(node, firstChild, secondChild, s);
      }

   if (node->nodeRequiresConditionCodes())
      {
      return node;
      }

   BINARY_IDENTITY_OP(LongInt, 0L)
   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();
   // normalize adds of positive constants to be isubs.  Have to do it this way because
   // more negative value than positive in a 2's complement signed representation
   if (node->getOpCodeValue() == TR::ladd &&
       secondChildOp == TR::lconst        &&
       secondChild->getLongInt() > 0)
      {
      if (performTransformation(s->comp(), "%sNormalized ladd of lconst > 0 in node [" POINTER_PRINTF_FORMAT "] to lsub of -lconst\n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::lsub);
         if (secondChild->getReferenceCount() == 1)
            {
            secondChild->setLongInt(-secondChild->getLongInt());
            }
         else
            {
            TR::Node * newSecondChild = TR::Node::create(secondChild, TR::lconst, 0);
            newSecondChild->setLongInt(-secondChild->getLongInt());
            node->setAndIncChild(1, newSecondChild);
            secondChild->decReferenceCount();
            }
         setIsHighWordZero(secondChild, s);
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   else if (node->getOpCodeValue() == TR::ladd && firstChildOp == TR::lneg)
      {
      TR::Node * newFirstChild = firstChild->getFirstChild();
      // turn lneg +(-1) into bitwise complement
      if (secondChildOp == TR::lconst &&
          secondChild->getLongInt() == -1) // -1LL works for IBM but not for MS
         {
         if (performTransformation(s->comp(), "%sReduced ladd of -1 and an lneg in node [" POINTER_PRINTF_FORMAT "] to bitwise complement\n", s->optDetailString(), node))
            {
            s->anchorChildren(node, s->_curTree);
            TR::Node::recreate(node, TR::lxor);
            node->setAndIncChild(0, newFirstChild);
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      // Turn add with first child negated into a subtract
      else
         {
         if (performTransformation(s->comp(), "%sReduced ladd with negated first child in node [" POINTER_PRINTF_FORMAT "] to lsub\n", s->optDetailString(), node))
            {
            s->anchorChildren(node, s->_curTree);
            TR::Node::recreate(node, TR::lsub);
            node->setAndIncChild(1, newFirstChild);
            node->setChild(0, secondChild);
            firstChild->recursivelyDecReferenceCount();
            node = s->simplify(node,block);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   // turn add with other child negated in to a subtract
   else if (node->getOpCodeValue() == TR::ladd && secondChildOp == TR::lneg)
      {
      if (performTransformation(s->comp(), "%sReduced ladd with negated second child in node [" POINTER_PRINTF_FORMAT "] to lsub\n", s->optDetailString(), node))
         {
         s->anchorChildren(node, s->_curTree);
         TR::Node * newSecondChild = secondChild->getFirstChild();
         TR::Node::recreate(node, TR::lsub);
         node->setAndIncChild(1, newSecondChild);
         secondChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   // reduce imul operations by factoring
   else if (firstChildOp == TR::lmul              &&
            firstChild->getReferenceCount() == 1 &&
            secondChildOp == TR::lmul             &&
            secondChild->getReferenceCount() == 1)
      {
      TR::Node * llChild     = firstChild->getFirstChild();
      TR::Node * lrChild     = firstChild->getSecondChild();
      TR::Node * rlChild     = secondChild->getFirstChild();
      TR::Node * rrChild     = secondChild->getSecondChild();
      TR::Node * factorChild = NULL;
      if (performTransformation(s->comp(), "%sFactored ladd with distributed lmul in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         if (llChild == rlChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            }
         else if (llChild == rrChild)
            {
            factorChild = llChild;
            secondChild->setChild(1, lrChild);
            }
         else if (lrChild == rlChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            }
         else if (lrChild == rrChild)
            {
            factorChild = lrChild;
            secondChild->setChild(1, llChild);
            }
         if (factorChild)
            {
            TR::Node::recreate(node, TR::lmul);
            node->setChild(0, factorChild)->decReferenceCount();
            TR::Node::recreate(secondChild, TR::ladd);
            firstChild->decReferenceCount();
            secondChild->setVisitCount(0);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node->setChild(1, s->simplify(secondChild, block));
            }
         }
      }
   else if (!node->getOpCode().isArrayRef() &&
       (firstChildOp == TR::ladd ||
             firstChildOp == TR::lsub))
      {
      if (secondChildOp == TR::lconst)
         {
         TR::Node * lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::lconst)
            {
            if (performTransformation(s->comp(), "%sFound ladd of lconst with ladd or lsub of x and const in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               if (firstChild->getReferenceCount()>1)
                  {
                  // it makes sense to uncommon it this situation
                  TR::Node * newFirstChild = TR::Node::create(node, firstChildOp, 2);
                  newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
                  newFirstChild->setAndIncChild(1, firstChild->getSecondChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(0,newFirstChild);
                  firstChild = newFirstChild;
                  }

               int64_t  value   = secondChild->getLongInt();
               TR::Node * llChild = firstChild->getFirstChild();
               if (firstChildOp == TR::ladd)
                  {
                  value += lrChild->getLongInt();
                  }
               else // firstChildOp == TR::lsub
                  {
                  value -= lrChild->getLongInt();
                  }
               if (value > 0)
                  {
                  value = -value;
                  TR::Node::recreate(node, TR::lsub);
                  }
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setLongInt(value);
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setLongInt(value);
                  secondChild->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, llChild);
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         else  // move constants up the tree so they will tend to get merged together
            {
            if ((firstChild->getReferenceCount() == 1) &&
                 performTransformation(s->comp(), "%sFound ladd of non-lconst with ladd or lsub of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               node->setChild(1, lrChild);
               firstChild->setChild(1, secondChild);
               TR::Node::recreate(node, firstChildOp);
               TR::Node::recreate(firstChild, TR::ladd);
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         }
      }
   else if (node->getOpCode().isArrayRef() &&
       firstChild->getOpCode().isArrayRef() &&
       secondChildOp == TR::lconst)
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if ((lrChild->getOpCodeValue() == TR::lconst) &&
          ((firstChild->getFutureUseCount() == 0) ||
           (lrChild->getLongInt() == -1*secondChild->getLongInt())))
         {
         if (performTransformation(s->comp(), "%sFound aladd of lconst with aladd x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
            {
            int64_t value  = secondChild->getLongInt();
            value += lrChild->getLongInt();
            if (secondChild->getReferenceCount() == 1)
               secondChild->setLongInt(value);
            else
               {
               TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
               node->setAndIncChild(1, foldedConstChild);
               foldedConstChild->setLongInt(value);
               secondChild->recursivelyDecReferenceCount();
               }
            node->setAndIncChild(0, firstChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
     else if ((firstChild->getReferenceCount() == 1) &&
               performTransformation(s->comp(), "%sFound aladd of non-lconst with aladd x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         // move constants up the tree so they will tend to be merged together
         node->setChild(1, lrChild);
         firstChild->setChild(1, secondChild);
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   addSimplifierJavaPacked(node, block, s);
   return node;
   }

TR::Node *faddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanFloatOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldFloatConstant(node, TR::Compiler->arith.floatAddFloat(firstChild->getFloat(), secondChild->getFloat()), s);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   // In IEEE FP Arithmetic, f + 0.0 is f
   // Pretend to be an int before doing the comparison
   // The simplification was removed because when f=-0.0
   // the result of f+(+0.0) is +0.0 (and not f)
   // If we would have ranges about f we might use the simplification in some cases
   //   BINARY_IDENTITY_OP(Int, FLOAT_POS_ZERO)

   // In IEEE FP Arithmetic, f + -0.0 is f
   // Pretend to be an int before doing the comparison
   //
   BINARY_IDENTITY_OP(FloatBits, FLOAT_NEG_ZERO)

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();

   if (isOperationFPCompliant(node, firstChild, s)) firstChild->setIsFPStrictCompliant(true);
   if (isOperationFPCompliant(node, secondChild, s))secondChild->setIsFPStrictCompliant(true);
   return node;
   }

TR::Node *daddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanDoubleOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, TR::Compiler->arith.doubleAddDouble(firstChild->getDouble(), secondChild->getDouble()), s);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   // In IEEE FP Arithmetic, d + 0.0 is d
   // Pretend to be an int before doing the comparison
   //
   // According to the java spec -0.0 + 0.0 == 0.0
   // BINARY_IDENTITY_OP(LongInt, DOUBLE_POS_ZERO)
   // In IEEE FP Arithmetic, d + -0.0 is d
   // Pretend to be an int before doing the comparison
   //
   BINARY_IDENTITY_OP(LongInt, DOUBLE_NEG_ZERO)

   if (isOperationFPCompliant(node, firstChild, s)) firstChild->setIsFPStrictCompliant(true);
   if (isOperationFPCompliant(node, secondChild, s))secondChild->setIsFPStrictCompliant(true);
   return node;
   }

TR::Node *baddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, firstChild->getByte() + secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OP(Byte, 0)
   return node;
   }

TR::Node *saddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return  addSimplifier <int16_t> (node, block, s);
   }

//---------------------------------------------------------------------
// Subtract simplifiers
//

TR::Node *isubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   scount_t futureUseCountOfFirstChild = node->getFirstChild()->getFutureUseCount();

   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::iusubb)
      return node;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if (node->getOpCode().isUnsigned())
         {
         uint32_t value = firstChild->getUnsignedInt() - secondChild->getUnsignedInt();
         foldUIntConstant(node, value, s, false /* !anchorChildren*/);

         if (node->nodeRequiresConditionCodes())
            setCCSubUnsigned(value, firstChild->getUnsignedInt(), node, s);
         }
      else
         {
         int32_t value = firstChild->getInt() - secondChild->getInt();
         foldIntConstant(node, value, s, false /* !anchorChilren */);

         if (node->nodeRequiresConditionCodes())
            setCCSubSigned(value, firstChild->getInt(), secondChild->getInt(), node, s);
         }
      return node;
      }

   if (!node->nodeRequiresConditionCodes()) // don't do identity op on cc nodes on zemulator
      BINARY_IDENTITY_OP(Int, 0)

   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();
   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();

   TR::Node * iMulComposerChild = NULL;
   int iMulComposerValue = 0;

   // first check case where both nodes are the same and replace with constant 0
   if (firstChild == secondChild)
      {
      if (node->nodeRequiresConditionCodes())
         {
         setCCSubSigned(0, 0, 0, node, s);
         }

      foldIntConstant(node, 0, s, true /* !anchorChilren */);

      return node;
      }

    if (node->nodeRequiresConditionCodes())
       {
       return node;
       }

    if (node->getOpCodeValue() != TR::aiadd &&
        !((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
          (s->_curTree->getNode()->getNumChildren() > 0) &&
      s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
     (s->_curTree->getNode()->getFirstChild()->getLastChild() == node)) &&
            ((node->getOpCodeValue() == TR::isub) &&
             ((secondChildOp == TR::iadd) || (secondChildOp == TR::isub))))
      {
      TR::Node * rlChild = secondChild->getFirstChild();
      TR::Node * rrChild = secondChild->getSecondChild();

      if ((rrChild->getOpCodeValue() == TR::iconst) &&
          (rlChild == firstChild) &&
          node->cannotOverflow() && secondChild->cannotOverflow())
         {
         if (performTransformation(s->comp(), "%sFolded isub with children related through iconst in node [%s] to iconst \n", s->optDetailString(), node->getName(s->getDebug())))
            {
            node->setChild(0, 0);
            node->setChild(1, 0);

            TR::Node::recreate(node, TR::iconst);
            if (secondChildOp == TR::iadd)
               node->setInt(-1*rrChild->getInt());
            else
               node->setInt(rrChild->getInt());
            node->setNumChildren(0);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   // normalize subtracts of positive constants to be iadds.  Have to do it this way because
   // more negative value than positive in a 2's complement signed representation
   else if (secondChildOp == TR::iconst &&
            secondChild->getInt() > 0)
      {
      if (performTransformation(s->comp(), "%sNormalized isub of iconst > 0 in node [%s] to iadd of -iconst \n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node::recreate(node, TR::iadd);
         if (secondChild->getReferenceCount() == 1)
            {
            secondChild->setInt(-secondChild->getInt());
            }
         else
            {
            TR::Node * newSecondChild = TR::Node::create(secondChild, TR::iconst, 0);
            newSecondChild->setInt(-secondChild->getInt());
            node->setAndIncChild(1, newSecondChild);
            secondChild->recursivelyDecReferenceCount();
            }
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = iaddSimplifier(node, block, s);
         }
      }
   // turn isub with second child ineg into an iadd
   else if (secondChildOp == TR::ineg)
      {
      if (performTransformation(s->comp(), "%sReduced isub with negated second child in node [%s] to iadd\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * newSecondChild = secondChild->getFirstChild();
         TR::Node::recreate(node, TR::iadd);
         node->setAndIncChild(1, newSecondChild);
         secondChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = iaddSimplifier(node, block, s);
         }
      }
      // turn isub with first child ineg into an ineg with iadd child.  isn't quicker, but normalizes expressions and drives inegs up the tree
   else if (firstChildOp == TR::ineg)
      {
      if (performTransformation(s->comp(), "%sReduced isub with negated first child in node [%s] to ineg of iadd\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * newFirstChild = firstChild->getFirstChild();
         TR::Node::recreate(node, TR::ineg);
         TR::Node * newNode = TR::Node::create(node, TR::iadd, 2);
         newNode->setAndIncChild(0, newFirstChild);
         newNode->setChild(1, secondChild);
         node->setChild(1, NULL);
         node->setAndIncChild(0, newNode);
         node->setNumChildren(1);
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   // turn bitwise complement + 1 into a 2's complement negate
   else if (isBitwiseIntComplement(firstChild) &&
            secondChildOp == TR::iconst         &&
            secondChild->getInt() == -1)
      {
      if (performTransformation(s->comp(), "%sReduced isub of bitwise complement and iconst -1 in node [%s] to 2s complement negation\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * firstGrandChild = firstChild->getFirstChild();
         TR::Node::recreate(node, TR::ineg);
         node->setAndIncChild(0, firstGrandChild);
         node->setNumChildren(1);
         secondChild->recursivelyDecReferenceCount();
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   else if (imulComposer(node, &iMulComposerValue, &iMulComposerChild ))
      {
      bool iMulDecomposeReport = s->comp()->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition);
      if (iMulDecomposeReport)
         dumpOptDetails(s->comp(), "\nImul composition succeeded for a value of %d.\n ", iMulComposerValue);

      if (s->getLastRun() && s->comp()->cg()->codegenMulDecomposition((int64_t)iMulComposerValue)
          && performTransformation(s->comp(), "%sFactored iadd with distributed imul with a codegen decomposible constant in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         TR::Node * lChild = node->getFirstChild();
         TR::Node * rChild = node->getSecondChild();
         TR::Node * constNode = NULL;

         if (iMulDecomposeReport)
            dumpOptDetails(s->comp(), "Putting the node back to imul with %d, for node [%s]. \n", iMulComposerValue, iMulComposerChild->getName(s->getDebug()));
         TR::Node::recreate(node, TR::imul);

         node->setAndIncChild(0, iMulComposerChild);
         constNode = TR::Node::create(node, TR::iconst, 0, iMulComposerValue);
         node->setAndIncChild(1, constNode);

         lChild->recursivelyDecReferenceCount();
         rChild->recursivelyDecReferenceCount();
         }

      }
   // reduce imul operations by factoring
   else if (firstChildOp == TR::imul              &&
            firstChild->getReferenceCount() == 1 &&
            secondChildOp == TR::imul             &&
            secondChild->getReferenceCount() == 1)
      {
      TR::Node * llChild     = firstChild->getFirstChild();
      TR::Node * lrChild     = firstChild->getSecondChild();
      TR::Node * rlChild     = secondChild->getFirstChild();
      TR::Node * rrChild     = secondChild->getSecondChild();
      TR::Node * factorChild = NULL;
      // moved this check down:
      // if (performTransformation(s->comp(), "%sFactored isub with distributed imul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         if (llChild == rlChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            }
         else if (llChild == rrChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            secondChild->setChild(1, rlChild);
            }
         else if (lrChild == rlChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            }
         else if (lrChild == rrChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            secondChild->setChild(1, rlChild);
            }

         // if invariance info available, do not group invariant
         // and non-invariant terms together
         TR_RegionStructure * region;
         if (factorChild && (region = s->containingStructure()) &&
             !s->getLastRun()  &&
             (isExprInvariant(region, secondChild->getFirstChild()) !=
         isExprInvariant(region, secondChild->getSecondChild()) ||
              isExprInvariant(region, factorChild)))  // to avoid conflict with rule 13
            {
            factorChild = NULL;
            }

         if (!factorChild ||
             !performTransformation(s->comp(), "%sFactored isub with distributed imul in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
       secondChild->setChild(0, rlChild);
       secondChild->setChild(1, rrChild);
            }
         else
            {
            TR::Node::recreate(node, TR::imul);
            node->setChild(factorChild->getOpCode().isLoadConst() ? 1:0, factorChild)->decReferenceCount();
            TR::Node::recreate(secondChild, TR::isub);
            firstChild->decReferenceCount();
            secondChild->setVisitCount(0);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node->setChild(factorChild->getOpCode().isLoadConst() ? 0:1, s->simplify(secondChild, block));
            }
         }
      }

   // 78418 - re-enable this reduction. Comments from 70791 retained for
   //  reference. The problems that were fixed with 70791 will now be fixed
   //  with store propagation for 5.0...
   // Original 70791 comments:
   //   70791 the firstChild->getReferenceCount()==1 test we all believe should
   //   really be on an inner if (page down a couple times), but LoopAtom gets
   //   a lot slower for register usage problems.  when we have a solution for
   //   that problem, the test should be moved back down there.
   else if (!((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
          (s->_curTree->getNode()->getNumChildren() > 0) &&
      s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
     (s->_curTree->getNode()->getFirstChild()->getLastChild() == node)) &&
           (firstChildOp == TR::iadd || firstChildOp == TR::isub))
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCodeValue() == TR::iconst)
         {
         if (secondChildOp == TR::iconst)
            {
            if (performTransformation(s->comp(), "%sFound isub of iconst with iadd or isub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               if (firstChild->getReferenceCount()>1)
                  {
                  // it makes sense to uncommon it in this situation
                  TR::Node * newFirstChild = TR::Node::create(node, firstChildOp, 2);
                  newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
                  newFirstChild->setAndIncChild(1, firstChild->getSecondChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(0,newFirstChild);
                  firstChild = newFirstChild;
                  }

               int32_t value = - secondChild->getInt();
               if (firstChildOp == TR::iadd)
                  {
                  value += lrChild->getInt();
                  }
               else // firstChildOp == TR::isub
                  {
                  value -= lrChild->getInt();
                  }
               if (value > 0)
                  {
                  value = -value;
                  }
               else
                  {
                  TR::Node::recreate(node, TR::iadd);
                  }
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setInt(value);
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::iconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setInt(value);
                  secondChild->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         else // Original 70791 comments:
              //   moving this test to the outer if to preserve
              //   performance in LoopAtom.  we all still believe the test
              //   should really be here, but a heavier solution is needed
              //   to fix register pressure problems thta result
            if ((firstChild->getReferenceCount() == 1) && (performTransformation(s->comp(), "%sFound isub of non-iconst with iadd or isub of x and const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug()))))
            {
            // move constants up the tree so they will tend to get merged together
            node->setChild(1, lrChild);
            firstChild->setChild(1, secondChild);
            TR::Node::recreate(node, firstChildOp);
            TR::Node::recreate(firstChild, TR::isub);

            // conservatively reset flags -- a future pass of VP will set them up
            // again if needed
            //
            firstChild->setIsNonZero(false);
            firstChild->setIsZero(false);
            firstChild->setIsNonNegative(false);
            firstChild->setIsNonPositive(false);
            firstChild->setCannotOverflow(false);

            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   else if (node->getFirstChild()->getOpCode().isLoadConst() &&
            node->getFirstChild()->get64bitIntegralValue() == 0 &&
            performTransformation(s->comp(), "%sReduce isub from 0 [%s] to ineg \n",s->optDetailString(),node->getName(s->getDebug())))
      {
      TR::Node::recreate(node, TR::ILOpCode::negateOpCode(node->getDataType()));
      node->setVisitCount(0);
      node->getFirstChild()->recursivelyDecReferenceCount();
      node->setChild(0, node->getSecondChild());
      node->setNumChildren(1);
      s->_alteredBlock = true;
      return node;
      }
    /* Simplify address arithmetic. Ie.
     *
     *     isub                           isub
     *       a2i                            node A
     *         aladd                        node B
     *           ==>loadaddr
     *           node A
     *       a2i
     *         aladd
     *           ==>loadaddr
     *           node B
     *
     * todo: recursive aladd's
     */
    else if (firstChildOp == TR::a2i && secondChildOp == TR::a2i)
       {
       TR::Node *LBase=NULL, *RBase=NULL;
       TR::Node *exprA=NULL, *exprB=NULL;
       if (firstChild->getChild(0)->getOpCode().isArrayRef() &&
           firstChild->getChild(0)->getChild(0)->getOpCode().isLoadAddr())
          {
          LBase = firstChild->getChild(0)->getChild(0);
          exprA = firstChild->getChild(0)->getChild(1);
          }
       else if (firstChild->getChild(0)->getOpCode().isLoadAddr())
          {
          LBase = firstChild->getChild(0);
          }

       if (secondChild->getChild(0)->getOpCode().isArrayRef() &&
           secondChild->getChild(0)->getChild(0)->getOpCode().isLoadAddr())
          {
          RBase = secondChild->getChild(0)->getChild(0);
          exprB = secondChild->getChild(0)->getChild(1);
          }
       else if (secondChild->getChild(0)->getOpCode().isLoadAddr())
          {
          RBase = secondChild->getChild(0);
          }

       if (LBase && RBase && LBase == RBase &&
           performTransformation(s->comp(), "%sRemove loadaddr in address computation in [" POINTER_PRINTF_FORMAT "]\n",
                 s->optDetailString(), node))
          {
          if (!exprA)
             exprA = TR::Node::createConstZeroValue(NULL, TR::Int32);
          if (!exprB)
             exprB = TR::Node::createConstZeroValue(NULL, TR::Int32);
          node->setAndIncChild(0, exprA);
          node->setAndIncChild(1, exprB);
          firstChild->recursivelyDecReferenceCount();
          secondChild->recursivelyDecReferenceCount();
          s->_alteredBlock = true;
          return node;
          }
       }

  if ((s->comp()->cg()->supportsLengthMinusOneForMemoryOpts() &&
       (s->_curTree->getNode()->getNumChildren() > 0) &&
        s->_curTree->getNode()->getFirstChild()->getOpCode().isMemToMemOp()) &&
      (s->_curTree->getNode()->getFirstChild()->getLastChild() == node))
     return node;

   reassociateBigConstants(node, s);

   return node;
   }

TR::Node *lsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::lusubb)
      return node;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if (node->nodeRequiresConditionCodes())
         {
         setCCSubSigned(firstChild->getLongInt() - secondChild->getLongInt(),
                        firstChild->getLongInt(), secondChild->getLongInt(), node, s);
         }

      foldLongIntConstant(node, firstChild->getLongInt() - secondChild->getLongInt(), s, false /* !anchorChildren */);

      return node;
      }

   if (!node->nodeRequiresConditionCodes())
      BINARY_IDENTITY_OP(LongInt, 0L)

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();
   // first check case where both nodes are the same and replace with constant 0
   if (firstChild == secondChild)
      {
      if (node->nodeRequiresConditionCodes())
         {
         setCCSubSigned(0, 0, 0, node, s);
         }
      foldLongIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }

   if (node->nodeRequiresConditionCodes())
      {
      return node;
      }

   if (node->getOpCodeValue() != TR::aladd &&
            ((node->getOpCodeValue() == TR::lsub) &&
             ((secondChildOp == TR::ladd) || (secondChildOp == TR::lsub))))
      {
      TR::Node * rlChild = secondChild->getFirstChild();
      TR::Node * rrChild = secondChild->getSecondChild();

      if ((rrChild->getOpCodeValue() == TR::lconst) &&
          (rlChild == firstChild) &&
          node->cannotOverflow() && secondChild->cannotOverflow())
         {
         if (performTransformation(s->comp(), "%sFolded lsub with children related through lconst in node [" POINTER_PRINTF_FORMAT "] to lconst \n", s->optDetailString(), node))
            {
            node->setChild(0, 0);
            node->setChild(1, 0);

            TR::Node::recreate(node, TR::lconst);
            if (secondChildOp == TR::ladd)
               node->setLongInt(-1*rrChild->getLongInt());
            else
               node->setLongInt(rrChild->getLongInt());
            node->setNumChildren(0);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   // normalize subtracts of positive constants to be ladds.  Have to do it this way because
   // more negative value than positive in a 2's complement signed representation
   else if (secondChildOp == TR::lconst &&
            secondChild->getLongInt() > 0)
      {
      if (performTransformation(s->comp(), "%sNormalized lsub of lconst > 0 in node [" POINTER_PRINTF_FORMAT "] to ladd of -lconst \n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::ladd);
         if (secondChild->getReferenceCount() == 1)
            {
            secondChild->setLongInt(-secondChild->getLongInt());
            }
         else
            {
            TR::Node * newSecondChild = TR::Node::create(secondChild, TR::lconst, 0);
            newSecondChild->setLongInt(-secondChild->getLongInt());
            node->setAndIncChild(1, newSecondChild);
            secondChild->recursivelyDecReferenceCount();
            }
         setIsHighWordZero(secondChild, s);
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   // turn lsub with second child lneg into an ladd
   else if (secondChildOp == TR::lneg)
      {
      if (performTransformation(s->comp(), "%sReduced lsub with negated second child in node [" POINTER_PRINTF_FORMAT "] to ladd\n", s->optDetailString(), node))
         {
         TR::Node * newSecondChild = secondChild->getFirstChild();
         TR::Node::recreate(node, TR::ladd);
         node->setChild(1, newSecondChild);
         if (secondChild->decReferenceCount() != 0)
            {
            newSecondChild->incReferenceCount();
            }
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   // turn lsub with first child lneg into an lneg with ladd child.  isn't quicker, but normalizes expressions and drives lnegs up the tree
   else if (firstChildOp == TR::lneg)
      {
      if (performTransformation(s->comp(), "%sReduced lsub with negated first child in node [" POINTER_PRINTF_FORMAT "] to lneg of ladd\n", s->optDetailString(), node))
         {
         TR::Node * newFirstChild = firstChild->getFirstChild();
         TR::Node::recreate(node, TR::lneg);
         TR::Node * newNode = TR::Node::create(node, TR::ladd, 2);
         newNode->setChild(0, newFirstChild);
         newNode->setChild(1, secondChild);
         node->setChild(1, NULL);
         node->setAndIncChild(0, newNode);
         node->setNumChildren(1);
         if (firstChild->decReferenceCount() != 0)
            {
            newFirstChild->incReferenceCount();
            }
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
   // turn bitwise complement + 1 into a 2's complement negate
   else if (isBitwiseLongComplement(firstChild) &&
            secondChildOp == TR::lconst          &&
            secondChild->getLongInt() == -1) // -1LL works for IBM but not MS
      {
      if (performTransformation(s->comp(), "%sReduced lsub of bitwise complement and lconst -1 in node [" POINTER_PRINTF_FORMAT "] to 2s complement negation\n", s->optDetailString(), node))
         {
         TR::Node * firstGrandChild = firstChild->getFirstChild();
         TR::Node::recreate(node, TR::lneg);
         node->setAndIncChild(0, firstGrandChild);
         node->setNumChildren(1);
         secondChild->recursivelyDecReferenceCount();
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   // reduce imul operations by factoring
   else if (firstChildOp == TR::lmul              &&
            firstChild->getReferenceCount() == 1 &&
            secondChildOp == TR::lmul             &&
            secondChild->getReferenceCount() == 1)
      {
      if (performTransformation(s->comp(), "%sFactored lsub with distributed lmul in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         TR::Node * llChild     = firstChild->getFirstChild();
         TR::Node * lrChild     = firstChild->getSecondChild();
         TR::Node * rlChild     = secondChild->getFirstChild();
         TR::Node * rrChild     = secondChild->getSecondChild();
         TR::Node * factorChild = NULL;
         if (llChild == rlChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            }
         else if (llChild == rrChild)
            {
            factorChild = llChild;
            secondChild->setChild(0, lrChild);
            secondChild->setChild(1, rlChild);
            }
         else if (lrChild == rlChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            }
         else if (lrChild == rrChild)
            {
            factorChild = lrChild;
            secondChild->setChild(0, llChild);
            secondChild->setChild(1, rlChild);
            }
         if (factorChild)
            {
            TR::Node::recreate(node, TR::lmul);
            node->setChild(0, factorChild)->decReferenceCount();
            TR::Node::recreate(secondChild, TR::lsub);
            firstChild->decReferenceCount();
            secondChild->setVisitCount(0);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node->setChild(1, s->simplify(secondChild, block));
            }
         }
      }
   else if ((firstChildOp == TR::ladd ||
             firstChildOp == TR::lsub))
      {
      if (secondChildOp == TR::lconst)
         {
         TR::Node * lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::lconst)
            {
            if (performTransformation(s->comp(), "%sFound lsub of lconst with ladd or lsub of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               if (firstChild->getReferenceCount()>1)
                  {
                  // it makes sense to uncommon it in this situation
                  TR::Node * newFirstChild = TR::Node::create(node, firstChildOp, 2);
                  newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
                  newFirstChild->setAndIncChild(1, firstChild->getSecondChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setAndIncChild(0,newFirstChild);
                  firstChild = newFirstChild;
                  }

               int64_t  value   = - secondChild->getLongInt();
               TR::Node * llChild = firstChild->getFirstChild();
               if (firstChildOp == TR::ladd)
                  {
                  value += lrChild->getLongInt();
                  }
               else // firstChildOp == TR::lsub
                  {
                  value -= lrChild->getLongInt();
                  }
               if (value > 0)
                  {
                  value = -value;
                  }
               else
                  {
                  TR::Node::recreate(node, TR::ladd);
                  }
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setLongInt(value);
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setLongInt(value);
                  secondChild->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, llChild);
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         else  // move constants up the tree so they will tend to get merged together
            {
            if ((firstChild->getReferenceCount() == 1) &&
                 performTransformation(s->comp(), "%sFound lsub of non-lconst with ladd or lsub of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               node->setChild(1, lrChild);
               firstChild->setChild(1, secondChild);
               TR::Node::recreate(node, firstChildOp);
               TR::Node::recreate(firstChild, TR::lsub);
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         }
      }
   // 113726: turn lsub to lxor if first operand fits in 32-bit imm and is all ones and the
   // second operand is smaller than the first.
   else if (firstChildOp == TR::lconst && secondChildOp == TR::land &&
            secondChild->getSecondChild()->getOpCodeValue() == TR::lconst)
      {
      int64_t lSubValue = firstChild->getLongInt();
      int64_t lAndValue = secondChild->getSecondChild()->getLongInt();

      // Check to see if the constants fit in 32-bit imm. and that all low bits in lSubValue are set.
      //i.e., we are checking for something like: 63 - i%64
      if ((lSubValue & 0xFFFFFFFF) == lSubValue &&
          (lSubValue & (lSubValue + 1)) == 0 &&
          lAndValue <= lSubValue)
         {
         TR::Node::recreate(node, TR::lxor);
         node->setVisitCount(0);
         s->_alteredBlock = true;
         node = s->simplify(node, block);
         }
      }
// 184251
   else if (firstChildOp == TR::lmul                                       &&
            firstChild->getSecondChild()->getOpCodeValue() == TR::lconst   &&
            firstChild->getFirstChild()->getOpCodeValue() == TR::i2l       &&
            secondChildOp == TR::lconst)
      {
      TR::Node * i2lChild = firstChild->getFirstChild();
      TR::Node * lmulChild = firstChild;

      int64_t lSubValue = secondChild->getLongInt();
      int64_t lmulInt   = firstChild->getSecondChild()->getLongInt();
      int32_t shftAmnt  = -1;

      if (firstChildOp == TR::lmul && isPowerOf2(lmulInt) && lmulInt > 0)
         {
         shftAmnt = trailingZeroes(lmulInt);
         }
      else if(firstChildOp == TR::lshl)
         {
         shftAmnt = lmulInt;
         }

      TR::Node * firstChild = i2lChild->getFirstChild();
      if (shftAmnt > 0                                                                          &&
           (firstChild->getOpCodeValue() == TR::iadd || firstChild->getOpCodeValue() == TR::isub) &&
            firstChild->isNonNegative()                                                         &&
            firstChild->getSecondChild()->getOpCodeValue() == TR::iconst     )
         {
         int32_t iValue = firstChild->getSecondChild()->getInt();
         int64_t lValue = (int64_t) iValue;

         // Make sure we don't overflow
         if ((iValue<<shftAmnt) == (int32_t)(lValue<<shftAmnt) &&
             performTransformation(s->comp(), "%sFound lsub/lmul of +ve powOf2 lconst with i2l of iadd or isub of x and iconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
            {
            // Reset the flags on i2lNode to reflect the new child - llChild
            TR::Node * llChild = firstChild->getFirstChild();
            i2lChild->setIsNonZero(llChild->isNonZero());
            i2lChild->setIsZero(llChild->isZero());
            i2lChild->setIsNonNegative(llChild->isNonNegative());
            i2lChild->setIsNonPositive(llChild->isNonPositive());
            i2lChild->setCannotOverflow(llChild->cannotOverflow());
            i2lChild->setIsHighWordZero(false);

            lmulChild->setIsNonZero(false);
            lmulChild->setIsZero(false);
            lmulChild->setIsNonNegative(false);
            lmulChild->setIsNonPositive(false);
            lmulChild->setCannotOverflow(false);
            lmulChild->setIsHighWordZero(false);

            if (firstChild->getOpCodeValue() == TR::iadd)
               {
               lSubValue -= lValue<<shftAmnt;
               }
            else // firstChildOp == TR::isub
               {
               lSubValue += lValue<<shftAmnt;
               }

            if (lmulChild->getReferenceCount() > 1)
               {
               // it makes sense to uncommon it in this situation
               TR::Node * n_lmulChild = TR::Node::create(node, TR::lmul, 2);
               n_lmulChild->setAndIncChild(0, lmulChild->getFirstChild());
               n_lmulChild->setAndIncChild(1, lmulChild->getSecondChild());
               lmulChild->recursivelyDecReferenceCount();
               node->setAndIncChild(0, n_lmulChild);
               lmulChild = n_lmulChild;
               }

            if (i2lChild->getReferenceCount() > 1)
               {
               // it makes sense to uncommon it in this situation
               TR::Node * n_i2lChild = TR::Node::create(lmulChild, TR::i2l, 1);
               n_i2lChild->setAndIncChild(0, i2lChild->getFirstChild());
               i2lChild->recursivelyDecReferenceCount();
               lmulChild->setAndIncChild(0, n_i2lChild);
               i2lChild = n_i2lChild;
               }

            i2lChild->setAndIncChild(0, firstChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();

            // Fixup lsub's const child
            if (secondChild->getReferenceCount() == 1)
               {
               secondChild->setLongInt(lSubValue);
               }
            else
               {
               TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
               node->setAndIncChild(1, foldedConstChild);
               foldedConstChild->setLongInt(lSubValue);
               secondChild->recursivelyDecReferenceCount();
               }

            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }

      }
   else if (firstChildOp == TR::i2l)
      {
      TR::Node * i2lNode = firstChild;
      firstChild = i2lNode->getFirstChild();
      firstChildOp = firstChild->getOpCodeValue();
      if (firstChildOp == TR::iadd || firstChildOp == TR::isub)
         {
         if (secondChildOp == TR::lconst)
            {
            TR::Node * llChild = firstChild->getFirstChild();
            TR::Node * lrChild = firstChild->getSecondChild();
            if (lrChild->getOpCodeValue() == TR::iconst)
               {
               if (node->isNonNegative() && i2lNode->isNonNegative())
                  {
                  // need to ensure no overflow happens on current isub operation to be able to make this transformation
                  // we can loosen this up in the future if we have more information on children nodes but not the parents
                  int64_t value = (int64_t) lrChild->getInt();
                  value = - secondChild->getLongInt();
                  if (performTransformation(s->comp(), "%sFound lsub of lconst with i2l of iadd or isub of x and iconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                     {
                     // Reset the flags on i2lNode to reflect the new child - llChild
                     i2lNode->setIsNonZero(llChild->isNonZero());
                     i2lNode->setIsZero(llChild->isZero());
                     i2lNode->setIsNonNegative(llChild->isNonNegative());
                     i2lNode->setIsNonPositive(llChild->isNonPositive());
                     i2lNode->setCannotOverflow(llChild->cannotOverflow());
                     i2lNode->setIsHighWordZero(false);

                     if (node->getFirstChild()->getReferenceCount() > 1)
                        {
                        // it makes sense to uncommon it in this situation
                        TR::Node * i2lChild = TR::Node::create(node, TR::i2l, 1);
                        i2lChild->setAndIncChild(0, node->getFirstChild()->getFirstChild());
                        node->getFirstChild()->recursivelyDecReferenceCount();
                        node->setAndIncChild(0,i2lChild);
                        }

                     if (firstChild->getReferenceCount() > 1)
                        {
                        // it makes sense to uncommon it in this situation
                        TR::Node * newFirstChild = TR::Node::create(node->getFirstChild(), firstChildOp, 2);
                        newFirstChild->setAndIncChild(0, firstChild->getFirstChild());
                        newFirstChild->setAndIncChild(1, firstChild->getSecondChild());
                        firstChild->recursivelyDecReferenceCount();
                        node->getFirstChild()->setAndIncChild(0,newFirstChild);
                        firstChild = newFirstChild;
                        }

                     int32_t iValue = lrChild->getInt();
                     int64_t lValue = (int64_t) iValue;
                     if (firstChildOp == TR::iadd)
                        {
                        value += lValue;
                        }
                     else // firstChildOp == TR::isub
                        {
                        value -= lValue;
                        }

                     if (value > 0)
                        {
                        value = -value;
                        }
                     else
                        {
                        TR::Node::recreate(node, TR::ladd);
                        }

                     if (secondChild->getReferenceCount() == 1)
                        {
                        secondChild->setLongInt(value);
                        }
                     else
                        {
                        TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
                        node->setAndIncChild(1, foldedConstChild);
                        foldedConstChild->setLongInt(value);
                        secondChild->recursivelyDecReferenceCount();
                        }
                     node->getFirstChild()->setAndIncChild(0, llChild);
                     firstChild->recursivelyDecReferenceCount();
                     node->setVisitCount(0);
                     s->_alteredBlock = true;
                     }
                  }
               }
            }
         }
      }
   /* Simplify address arithmetic. Ie.
    *
    *     lsub                           lsub
    *       a2l                            node A
    *         aladd                        node B
    *           ==>loadaddr
    *           node A
    *       a2l
    *         aladd
    *           ==>loadaddr
    *           node B
    *
    * todo: recursive aladd's
    */
   else if (firstChildOp == TR::a2l && secondChildOp == TR::a2l)
      {
      TR::Node *LBase=NULL, *RBase=NULL;
      TR::Node *exprA=NULL, *exprB=NULL;
      if (firstChild->getChild(0)->getOpCode().isArrayRef() &&
          firstChild->getChild(0)->getChild(0)->getOpCode().isLoadAddr())
         {
         LBase = firstChild->getChild(0)->getChild(0);
         exprA = firstChild->getChild(0)->getChild(1);
         }
      else if (firstChild->getChild(0)->getOpCode().isLoadAddr())
         {
         LBase = firstChild->getChild(0);
         }

      if (secondChild->getChild(0)->getOpCode().isArrayRef() &&
          secondChild->getChild(0)->getChild(0)->getOpCode().isLoadAddr())
         {
         RBase = secondChild->getChild(0)->getChild(0);
         exprB = secondChild->getChild(0)->getChild(1);
         }
      else if (secondChild->getChild(0)->getOpCode().isLoadAddr())
         {
         RBase = secondChild->getChild(0);
         }

      if (LBase && RBase && LBase == RBase &&
          performTransformation(s->comp(), "%sRemove loadaddr in address computation in [" POINTER_PRINTF_FORMAT "]\n",
                s->optDetailString(), node))
         {
         if (!exprA)
            exprA = TR::Node::createConstZeroValue(NULL, TR::Int64);
         if (!exprB)
            exprB = TR::Node::createConstZeroValue(NULL, TR::Int64);
         node->setAndIncChild(0, exprA);
         node->setAndIncChild(1, exprB);
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         s->_alteredBlock = true;
         return node;
         }
      }

   if (!node->nodeRequiresConditionCodes() &&
       secondChild->getOpCodeValue() == TR::land &&
       secondChild->getFirstChild() == firstChild &&
       secondChild->getSecondChild()->getOpCode().isLoadConst())
      {
      uint64_t mask = secondChild->getSecondChild()->getLongInt();
      if (((mask + 1) & mask) == 0 && // mask + 1 power of two?
          performTransformation(s->comp(), "%sNext lower pwr of 2 using land [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         node->recreate(node, TR::land);
         TR::Node* maskNode = TR::Node::create(node, TR::lconst, 0);
         mask = ~mask;
         maskNode->setLongInt(mask);
         node->setAndIncChild(1, maskNode);
         secondChild->recursivelyDecReferenceCount();
         return node;
         }
      }

   return node;
   }

TR::Node *fsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanFloatOp(node, firstChild, secondChild, s)) !=NULL )
      return result;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldFloatConstant(node, TR::Compiler->arith.floatSubtractFloat(firstChild->getFloat(), secondChild->getFloat()), s);
      return node;
      }

   // In IEEE FP Arithmetic, f - 0.0 is f
   // Pretend to be an int before doing the comparison
   //
   BINARY_IDENTITY_OP(FloatBits, FLOAT_POS_ZERO)

   // In IEEE FP Arithmetic, f - -0.0 is f
   // Pretend to be an int before doing the comparison
   // The simplification was removed because when f=-0.0
   // the result of f-(-0.0) is +0.0 (and not f)
   // If we would have ranges about f we might use the simplification in some cases
   // BINARY_IDENTITY_OP(Int, FLOAT_NEG_ZERO)

   firstChild  = node->getFirstChild();
   secondChild = node->getSecondChild();

   if (isOperationFPCompliant(node,firstChild, s)) firstChild->setIsFPStrictCompliant(true);
   if (isOperationFPCompliant(node,secondChild, s))secondChild->setIsFPStrictCompliant(true);

   return node;
   }

TR::Node *dsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanDoubleOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, TR::Compiler->arith.doubleSubtractDouble(firstChild->getDouble(), secondChild->getDouble()), s);
      return node;
      }

   // In IEEE FP Arithmetic, d - 0.0 is d
   // Pretend to be an int before doing the comparison
   //
   // According to the java spec -0.0 - (-0.0) == 0.0
   // BINARY_IDENTITY_OP(LongInt, DOUBLE_NEG_ZERO)
   // In IEEE FP Arithmetic, d - -0.0 is d
   // Pretend to be an int before doing the comparison
   //
   BINARY_IDENTITY_OP(LongInt, DOUBLE_POS_ZERO)

   if (isOperationFPCompliant(node, firstChild, s)) firstChild->setIsFPStrictCompliant(true);
   if (isOperationFPCompliant(node, secondChild, s))secondChild->setIsFPStrictCompliant(true);
   return node;
   }

TR::Node *bsubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, firstChild->getByte() - secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }

   BINARY_IDENTITY_OP(Byte, 0)
   return node;
   }

TR::Node *ssubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return  subSimplifier <int16_t> (node, block, s);
   }

//---------------------------------------------------------------------
// Multiply simplifiers
//

TR::Node *imulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if (node->getOpCode().isUnsigned())
         {
         foldUIntConstant(node, firstChild->getUnsignedInt()* secondChild->getUnsignedInt(), s, false /* !anchorChildren*/);
         }
      else
         {
         foldIntConstant(node, firstChild->getInt()* secondChild->getInt(), s, false /* !anchorChildren*/);
         }
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int32_t, Int, 1, 0)

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // a*(-c) = -(a*c)   // move up ineg for better composing
   if (secondChildOp == TR::iconst &&
       secondChild->getInt() < 0 &&
       secondChild->getInt() != TR::getMinSigned<TR::Int32>() &&
       performTransformation(s->comp(), "%sFound a*(-c) = -(a*c) in imul [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
      {
      TR::Node::recreate(node, TR::ineg);
      node->setNumChildren(1);
      if (secondChild->getReferenceCount() == 1)
         {
         secondChild->setInt(-secondChild->getInt());
         }
      else
         {
         int32_t negVal = -secondChild->getInt();
         secondChild->decReferenceCount();
         secondChild = TR::Node::iconst(negVal);
         secondChild->incReferenceCount();
         }
      TR::Node *newMul = TR::Node::create(TR::imul, 2);
      newMul->setChild(0, firstChild);
      newMul->setChild(1, secondChild);
      node->setAndIncChild(0, newMul);
      s->_alteredBlock = true;
      return s->simplify(node, block);
      }

   if (firstChildOp == TR::imul && firstChild->getReferenceCount() == 1)
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCodeValue() == TR::iconst)
         {
         if (secondChildOp == TR::iconst)
            {
            // (a*c1)*c2 => a*(c1*c2)        todo: overflow?
            if (performTransformation(s->comp(), "%sFound (a*c1)*c2 => a*(c1*c2) in imul [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setInt(secondChild->getInt() * lrChild->getInt());
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::iconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setInt(secondChild->getInt() * lrChild->getInt());
                  secondChild->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         // (a*c1)*b => (a*b)*c1
         else if (performTransformation(s->comp(), "%sFound (a*c1)*b => (a*b)*c1 in imul [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            // move constants up the tree so they will tend to get merged together
            node->setChild(1, lrChild);
            firstChild->setChild(1, secondChild);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   // see if we can distribute multiply by constant over add or sub with a constant.
   // makes for better addressing mode trees for multiple array accesses using indices
   // which differ only by constants, ex. a[i] + a[i+2]
   else if ((secondChildOp == TR::iconst) &&
            (firstChildOp == TR::isub || firstChildOp == TR::iadd))
      {
      TR::Node * lrChild = firstChild->getSecondChild();

      // (a+c1)*c2 = a*c2 + c1*c2
      if (lrChild->getOpCodeValue() == TR::iconst &&
          performTransformation(s->comp(), "%sFound (a+c1)*c2 = a*c2 + c1*c2 in imul [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         int32_t product = lrChild->getInt() * secondChild->getInt();
         if (firstChildOp == TR::isub)
            {
            product = -product;
            }
         TR::Node * productNode = secondChild;
         int32_t  scale       = secondChild->getInt();
         if (secondChild->getReferenceCount() != 1)
            {
            productNode = TR::Node::create(secondChild, TR::iconst, 0);
            productNode->setReferenceCount(1);
            node->setChild(1, productNode);
            secondChild->decReferenceCount();
            }
         if (product > 0)
            {
            productNode->setInt((int32_t)-product);
            TR::Node::recreate(node, TR::isub);
            }
         else
            {
            productNode->setInt((int32_t)product);
            TR::Node::recreate(node, TR::iadd);
            }
         if (firstChild->getReferenceCount() != 1)
            {
            TR::Node * newFirst = TR::Node::create(firstChild, TR::imul, 2);
            newFirst->setReferenceCount(1);
            newFirst->setAndIncChild(0, firstChild->getFirstChild());
            newFirst->setAndIncChild(1, lrChild);
            firstChild->recursivelyDecReferenceCount();
            firstChild = newFirst;
            node->setChild(0, firstChild);
            }
         else
            {
            TR::Node::recreate(firstChild, TR::imul);
            }
         if (lrChild->getReferenceCount() != 1)
            {
            lrChild->decReferenceCount();
            lrChild = TR::Node::create(lrChild, TR::iconst, 0);
            lrChild->setReferenceCount(1);
            firstChild->setChild(1, lrChild);
            }
         lrChild->setInt(scale);
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   else if (secondChildOp == TR::iconst &&
            !s->getLastRun() &&
            secondChild->getInt()!=0 && !isNonNegativePowerOf2(secondChild->getInt()) &&
            secondChild->getInt() != TR::getMinSigned<TR::Int32>())
      {
      decomposeMultiply(node, s, false);
      }

   //       imul            iand
   //       / \             / \
   //    idiv  c2    =>    e   c3
   //     / \
   //    e   c1
   int32_t size;

   if (s->getLastRun() &&
       node->isNonNegative() &&
       node->getOpCode().isMul() &&
       node->getFirstChild()->getOpCode().isDiv() &&
       node->getFirstChild()->getSecondChild()->getOpCode().isLoadConst() &&
       node->getSecondChild()->getOpCode().isLoadConst() &&
       (size = node->getSecondChild()->get64bitIntegralValue()) ==
        node->getFirstChild()->getSecondChild()->get64bitIntegralValue() &&
       ((size & (size - 1)) == 0) &&  // size is  power of 2
        size > 0)
      {
      if (performTransformation(s->comp(), "%sFolded mul and div by the same constant 0x%p\n", s->optDetailString(), node))
         {
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();

         TR::Node::recreate(node, TR::iand);

         TR::Node * newConst = TR::Node::create(node, TR::iconst, 0);
         newConst->setInt(~(size-1));

         node->setAndIncChild(0, node->getFirstChild()->getFirstChild());
         node->setAndIncChild(1, newConst);

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         }
      }

   if (s->reassociate())
   {
   TR_RegionStructure * region = s->containingStructure();

   // R10:   *              *
   //       / \            / \
   //      *  c2    =>    t   *
   //     / \                / \
   //    t   c1             c2 c1
   if (region &&
       node->getOpCodeValue() == TR::imul &&
       isExprInvariant(region, node->getSecondChild()) &&
       node->getFirstChild()->getOpCodeValue() == TR::imul &&
       !isExprInvariant(region, node->getFirstChild()->getFirstChild()) &&
       isExprInvariant(region, node->getFirstChild()->getSecondChild()) &&
       node->getFirstChild()->getReferenceCount() == 1)
      {
      if (performTransformation(s->comp(), "%sApplied reassociation rule 10 to node 0x%p\n", s->optDetailString(), node))
         {
         TR::Node *e = node->getFirstChild()->getFirstChild();
         node->getFirstChild()->setChild(0,node->getSecondChild());
         node->setChild(1, node->getFirstChild());
         node->setChild(0, e);
         }
      }

   // R11:   *               +
   //       / \             / \
   //      +  c2    =>     *  c1*c2
   //     / \             / \
   //    e   c1          e   c2
   if (region &&
       node->getOpCodeValue() == TR::imul &&
       isExprInvariant(region, node->getSecondChild()) &&
       node->getFirstChild()->getOpCodeValue() == TR::iadd &&
       !isExprInvariant(region, node->getFirstChild()->getFirstChild()) &&
       isExprInvariant(region, node->getFirstChild()->getSecondChild()) &&
       node->getFirstChild()->getReferenceCount() == 1)
      {
      if (performTransformation(s->comp(), "%sApplied reassociation rule 11 to node 0x%p\n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::iadd);
         TR::Node *newSecondChild = TR::Node::create(node, TR::imul, 2);
         newSecondChild->setChild(0, node->getFirstChild()->getSecondChild());
         newSecondChild->setAndIncChild(1, node->getSecondChild());
         TR::Node::recreate(node->getFirstChild(), TR::imul);
         node->getFirstChild()->setChild(1, node->getSecondChild());
         node->setAndIncChild(1, newSecondChild);

         setExprInvariant(region, newSecondChild);
         }
      }
   //                         +
   // R13:   *               / \
   //       / \             /   \
   //      +   c    =>     *     *
   //     / \             / \   / \
   //    e1  e2          e1  c e2  c
   else if (region &&
       node->getOpCodeValue() == TR::imul &&
       isExprInvariant(region, node->getSecondChild()) &&
       node->getFirstChild()->getOpCodeValue() == TR::iadd)
      {
      if (performTransformation(s->comp(), "%sApplied reassociation rule 13 to node 0x%p\n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::iadd);
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();

         TR::Node *mul1 = TR::Node::create(node, TR::imul, 2);
         mul1->setAndIncChild(0, firstChild->getFirstChild());
         mul1->setAndIncChild(1, secondChild);

         TR::Node *mul2 = TR::Node::create(node, TR::imul, 2);
         mul2->setAndIncChild(0, firstChild->getSecondChild());
         mul2->setAndIncChild(1, secondChild);

         node->setAndIncChild(0, mul1);
         node->setAndIncChild(1, mul2);

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         }
      }
   //                         -
   // R15:   *               / \
   //       / \             /   \
   //      -   c    =>     *     *
   //     / \             / \   / \
   //    e1  e2          e1  c e2  c
   else if (region &&
       node->getOpCodeValue() == TR::imul &&

       isExprInvariant(region, node->getSecondChild()) &&
       !isExprInvariant(region, node->getFirstChild()) &&
       node->getFirstChild()->getOpCodeValue() == TR::isub)
      {
      if (performTransformation(s->comp(), "%sApplied reassociation rule 15 to node 0x%p\n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::isub);
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();

         TR::Node *mul1 = TR::Node::create(node, TR::imul, 2);
         mul1->setAndIncChild(0, firstChild->getFirstChild());
         mul1->setAndIncChild(1, secondChild);

         TR::Node *mul2 = TR::Node::create(node, TR::imul, 2);
         mul2->setAndIncChild(0, firstChild->getSecondChild());
         mul2->setAndIncChild(1, secondChild);

         // if e2 is const, convert node to TR::iadd
         if (mul2->getFirstChild()->getOpCodeValue() == TR::iconst)
            {
            TR::Node::recreate(node, TR::iadd);
            TR::Node * newConst = TR::Node::create(node, TR::iconst, 0);
            newConst->setInt(-mul2->getFirstChild()->getInt());
            mul2->getFirstChild()->recursivelyDecReferenceCount();
            mul2->setAndIncChild(0, newConst);
            }
         // if c is const, convert node to TR::iadd
         else if (mul2->getSecondChild()->getOpCodeValue() == TR::iconst)
            {
            TR::Node::recreate(node, TR::iadd);
            TR::Node * newConst = TR::Node::create(node, TR::iconst, 0);
            newConst->setInt(-mul2->getSecondChild()->getInt());
            mul2->getSecondChild()->recursivelyDecReferenceCount();
            mul2->setAndIncChild(1, newConst);
            }

         node->setAndIncChild(0, mul1);
         node->setAndIncChild(1, mul2);

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         }
      }
   }

   return node;
   }

TR::Node *lmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->isAdjunct())
      {
      // do not simplify dual operators
      return node;
      }

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getLongInt() * secondChild->getLongInt(), s, false /* !anchorChildren */);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   orderChildrenByHighWordZero(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int64_t, LongInt, 1L, 0L)

   // TODO - strength reduction
   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();
   if (firstChildOp == TR::lmul && firstChild->getReferenceCount() == 1)
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCodeValue() == TR::lconst)
         {
         if (secondChildOp == TR::lconst)
            {
            if (performTransformation(s->comp(), "%sFound lmul of lconst with lmul of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setLongInt(secondChild->getLongInt() * lrChild->getLongInt());
                  setIsHighWordZero(secondChild, s);
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setLongInt(secondChild->getLongInt() * lrChild->getLongInt());
                  secondChild->recursivelyDecReferenceCount();
                  setIsHighWordZero(foldedConstChild, s);
                  }
               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         else // move constants up the tree so they will tend to get merged together
            {
            if (performTransformation(s->comp(), "%sFound lmul of non-lconst with lmul of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               node->setChild(1, lrChild);
               firstChild->setChild(1, secondChild);
               node->setVisitCount(0);
               s->_alteredBlock = true;
               }
            }
         }
      }
   else if (secondChildOp == TR::lconst &&
            (firstChildOp == TR::ladd  || firstChildOp == TR::lsub))
      {
      firstChildOp = firstChild->getOpCodeValue();
      TR::Node *lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCodeValue() == TR::lconst &&
          performTransformation(s->comp(), "%sDistributed lmul with lconst over lsub or ladd with lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         int64_t product = lrChild->getLongInt() * secondChild->getLongInt();
         if (firstChildOp == TR::lsub)
            product = -product;
         TR::Node *productNode = secondChild;
         int64_t scale = secondChild->getLongInt();
         if (secondChild->getReferenceCount() != 1)
            {
            productNode = TR::Node::create(secondChild, TR::lconst, 0);
            node->setAndIncChild(1, productNode);
            secondChild->decReferenceCount();
            }
         if (product > 0)
            {
            productNode->setLongInt((int64_t)-product);
            TR::Node::recreate(node, TR::lsub);
            }
         else
            {
            productNode->setLongInt((int64_t)product);
            TR::Node::recreate(node, TR::ladd);
            }

         TR::Node *newFirst = TR::Node::create(firstChild, TR::lmul, 2);
         node->setAndIncChild(0, newFirst);
         TR::Node *grandChild = firstChild->getFirstChild();
         newFirst->setAndIncChild(0, grandChild);

         if (firstChild->decReferenceCount() == 0)
            {
            grandChild->decReferenceCount();
            lrChild->decReferenceCount();
            }

         lrChild = TR::Node::create(lrChild, TR::lconst, 0);
         newFirst->setAndIncChild(1, lrChild);
         lrChild->setLongInt((int64_t) scale);

         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   // try to distribute multiply by constant over add or sub with a constant
   // makes for better addressing mode trees for multiple array accesses using
   // indices that differ by constants, ex. a[i] + a[i+2]
   // for 64-bit, should be careful to check for overflow on the add/sub since
   // the simplification is then invalid
   else if (secondChildOp == TR::lconst &&
              firstChildOp == TR::i2l)
      {
      TR::Node *i2lNode = firstChild;
      if (((i2lNode->getFirstChild()->getOpCodeValue() == TR::isub) ||
            (i2lNode->getFirstChild()->getOpCodeValue() == TR::iadd)) &&
            i2lNode->getFirstChild()->cannotOverflow())
         {
         firstChild = firstChild->getFirstChild();
         firstChildOp = firstChild->getOpCodeValue();
         TR::Node *lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::iconst &&
               performTransformation(s->comp(), "%sDistributed lmul with lconst over isub or iadd of with iconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
            {
            int64_t product = (int64_t)lrChild->getInt() * secondChild->getLongInt();
            if (firstChildOp == TR::isub)
               product = -product;
            TR::Node *productNode = secondChild;
            int64_t scale = secondChild->getLongInt();
            if (secondChild->getReferenceCount() != 1)
               {
               productNode = TR::Node::create(secondChild, TR::lconst, 0);
               node->setAndIncChild(1, productNode);
               secondChild->decReferenceCount();
               }
            if (product > 0)
               {
               productNode->setLongInt((int64_t)-product);
               TR::Node::recreate(node, TR::lsub);
               }
            else
               {
               productNode->setLongInt((int64_t)product);
               TR::Node::recreate(node, TR::ladd);
               }


            TR::Node *newFirst = TR::Node::create(i2lNode, TR::lmul, 2);
            node->setAndIncChild(0, newFirst);
       TR::Node *grandChild = firstChild->getFirstChild();

       if (i2lNode->getReferenceCount() != 1)
               {
          i2lNode->decReferenceCount();
          i2lNode = TR::Node::create(firstChild, TR::i2l, 1);
          i2lNode->setReferenceCount(1);
          }
       else
          {
          if (firstChild->decReferenceCount() == 0)
        {
        grandChild->decReferenceCount();
        lrChild->decReferenceCount();
        }
          }

       i2lNode->setAndIncChild(0, grandChild);

       // conservatively reset flags -- a future pass of VP will set them up
       // again if needed
       //
       i2lNode->setIsNonZero(false);
       i2lNode->setIsZero(false);
       i2lNode->setIsNonNegative(false);
       i2lNode->setIsNonPositive(false);
       i2lNode->setCannotOverflow(false);
       i2lNode->setIsHighWordZero(false);

       grandChild->setIsNonZero(false);
       grandChild->setIsZero(false);
       grandChild->setIsNonNegative(false);
       grandChild->setIsNonPositive(false);
       grandChild->setCannotOverflow(false);


       newFirst->setFirst(i2lNode);

            lrChild = TR::Node::create(lrChild, TR::lconst, 0);
            newFirst->setAndIncChild(1, lrChild);
            lrChild->setLongInt((int64_t) scale);

            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }
   else if (TR::Compiler->target.is64Bit() && secondChildOp==TR::lconst && !s->getLastRun() && secondChild->getLongInt()!=0 && !isNonNegativePowerOf2(secondChild->getLongInt()) && secondChild->getLongInt() != TR::getMinSigned<TR::Int64>())
      {
      decomposeMultiply(node, s, true);
      }

   return node;
   }

TR::Node *fmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanFloatOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldFloatConstant(node, TR::Compiler->arith.floatMultiplyFloat(firstChild->getFloat(), secondChild->getFloat()), s);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   // we can create x*1.0 when negFma is available.  Don't undo it
   if (!s->comp()->cg()->supportsNegativeFusedMultiplyAdd() || !node->isFPStrictCompliant())
      {
      // In IEEE FP Arithmetic, f * 1.0 is f
      // Pretend to be an int before doing the comparison
      //
      BINARY_IDENTITY_OP(FloatBits, FLOAT_ONE)
      }

   firstChild  = node->getFirstChild();
   secondChild = node->getSecondChild();

   bool firstChildNegated = (TR::fneg == firstChild->getOpCodeValue());
   bool secondChildNegated = (TR::fneg == secondChild->getOpCodeValue());
   bool bothChildrenNegated = (firstChildNegated && secondChildNegated);

   if (bothChildrenNegated)
      {
      if (performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] (-A)*(-B) -> A*B\n", s->optDetailString(), node))
        { // remove negates
        TR::Node * newFirstChild= s->replaceNode(firstChild,firstChild->getFirstChild(), s->_curTree);
        TR::Node * newSecondChild= s->replaceNode(secondChild,secondChild->getFirstChild(), s->_curTree);
        node->setChild(0,newFirstChild);
        node->setChild(1,newSecondChild);
        }
      }
   return node;
   }

TR::Node *dmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanDoubleOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, TR::Compiler->arith.doubleMultiplyDouble(firstChild->getDouble(), secondChild->getDouble()), s);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   if (s->comp()->getOption(TR_IgnoreIEEERestrictions) &&
       !node->isFPStrictCompliant() &&
       !s->comp()->cg()->supportsNegativeFusedMultiplyAdd())  // we can create x*1.0 when negFma is available.  Don't undo it
      {
      // In IEEE FP Arithmetic, d * 1.0 is d
      // Pretend to be an int before doing the comparison
      //
      BINARY_IDENTITY_OP(LongInt, DOUBLE_ONE)
      }
   return node;
   }

TR::Node *bmulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, firstChild->getByte() * secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int8_t, Byte, 1, 0)

   // TODO - strength reduction

   return node;
   }

TR::Node *smulSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getShortInt() * secondChild->getShortInt(), s, false /* !anchorChildren */);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int16_t, ShortInt, 1, 0)

   // TODO - strength reduction

   return node;
   }

//---------------------------------------------------------------------
// Divide simplifiers
//

TR::Node *idivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::iudiv)
      {
      if (!node->getChild(0)->isNonNegative())
         return node;
      if (!node->getChild(1)->isNonNegative())
         return node;
      }

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   static const char * disableILDivPwr2Opt = feGetEnv("TR_DisableILDivPwr2Opt");

   // Only try to fold if the divisor is non-zero.
   // Handle the special case of dividing the maximum negative value by -1
   //
   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t divisor = secondChild->getInt();
      int32_t shftAmnt = -1;
      if (divisor != 0)
         {
         if (firstChild->getOpCode().isLoadConst())
            {
            int32_t dividend = firstChild->getInt();
            if (node->getOpCode().isUnsigned())
               {
               uint32_t uquotient, udivisor, udividend;
               udivisor = secondChild->getUnsignedInt();
               udividend = firstChild->getUnsignedInt();
               if (firstChild->getOpCode().isUnsigned())
                  {
                  if (secondChild->getOpCode().isUnsigned())
                     {
                     uquotient = udividend / udivisor;
                     }
                  else
                     {
                     uquotient = udividend / divisor;
                     }
                  }
               else
                  {
                  if (secondChild->getOpCode().isUnsigned())
                     {
                     uquotient = dividend / udivisor;
                     }
                  else
                     {
                     uquotient = dividend / divisor;
                     }
                  }
               foldUIntConstant(node, uquotient, s, false /* !anchorChildren*/);
               }
            else
               {
               if (divisor == -1 && dividend == TR::getMinSigned<TR::Int32>())
                  return s->replaceNode(node, firstChild, s->_curTree);
               foldIntConstant(node, dividend/divisor, s, false /* !anchorChildren*/);
               }
            }   // first child is constant
         else if (divisor == 1)
            return s->replaceNode(node, firstChild, s->_curTree);
         else if (!secondChild->getOpCode().isUnsigned() && divisor == -1)
            {
            if (performTransformation(s->comp(), "%sReduced idiv by -1 with ineg in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               // make sure we don't lose grandchildren in prepareToReplaceNode
               firstChild->incReferenceCount();

               s->prepareToReplaceNode(node);
               TR::Node::recreate(node, TR::ineg);
               node->setChild(0, firstChild);
               node->setNumChildren(1);
               }
            }
         // Design 1790
         else if ((!disableILDivPwr2Opt)&&((shftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(divisor)) > 0) &&
            (secondChild->getReferenceCount()==1) && performTransformation(s->comp(), "%sPwr of 2 idiv opt node %p\n", s->optDetailString(), node) )
            {
            secondChild->decReferenceCount();
            TR::Node * newNode1;
            if (shftAmnt-1 != 0)
               {
               newNode1 = TR::Node::create(node, TR::ishr, 2);         //shift
               newNode1->setFirst(firstChild);
               newNode1->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt-1)); //shift amount, test for 0
               newNode1->getSecondChild()->incReferenceCount();
               newNode1->incReferenceCount();
               }
            else
               {
               newNode1 = firstChild;
               }
            TR::Node * newNode2 = TR::Node::create(node, TR::iushr, 2); // another shift
            newNode2->setFirst(newNode1);
            newNode2->setSecond(TR::Node::create(node, TR::iconst, 0, 32-shftAmnt));  // shift amount, test for 0
            newNode2->getSecondChild()->incReferenceCount();
            TR::Node * newNode3 = TR::Node::create(node, TR::iadd, 2);
            newNode3->setFirst(newNode2);
            newNode3->setSecond(firstChild);
            newNode3->getFirstChild()->incReferenceCount();
            newNode3->getSecondChild()->incReferenceCount();
            if (divisor>0)
               {
               TR::Node::recreate(node, TR::ishr);
               node->setFirst(newNode3);
               node->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt));
               node->getSecondChild()->incReferenceCount();
               }
            else
               {
               TR::Node * newNode4 = TR::Node::create(node, TR::ishr, 2);
               newNode4->setFirst(newNode3);
               newNode4->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt));
               newNode4->getFirstChild()->incReferenceCount();
               newNode4->getSecondChild()->incReferenceCount();
               TR::Node::recreate(node, TR::ineg);
               node->setNumChildren(1);
               node->setFirst(newNode4);
               }
            node->getFirstChild()->incReferenceCount();
            }
         else if (s->cg()->getSupportsLoweringConstIDiv() && !isPowerOf2(divisor) &&
                  performTransformation(s->comp(), "%sMagic number idiv opt in node %p\n", s->optDetailString(), node))
            {
             // leave idiv as is if the divisor is 2^n. CodeGen generates a fast instruction squence for it.
             // otherwise, expose the magic number squence to allow optimization
             // lowered tree will look like this:
             // iadd (node)
             //   ishr (node3)
             //     iadd (node2)
             //       iload [ dividend ]
             //       imulh (node1)
             //         ===>iload [ dividend ]
             //         iconst [ magic number ]
             //     iconst [ shift amount ]
             //   ishr (node4)
             //     ===>iload [ dividend ]
             //     iconst 31

             int32_t magicNumber, shiftAmount;
             s->cg()->compute32BitMagicValues(divisor, &magicNumber, &shiftAmount);

             TR::Node * node1, * node2, * node3, * node4;

             node1 = TR::Node::create(TR::imulh, 2, firstChild,
                          TR::Node::create(firstChild, TR::iconst, 0, magicNumber));

             if ( (divisor > 0) && (magicNumber < 0) )
                {
                node2 = TR::Node::create(TR::iadd, 2, node1, firstChild);
                }
             else if ( (divisor < 0) && (magicNumber > 0) )
                {
                node2 = TR::Node::create(TR::isub, 2, node1, firstChild);
                }
             else
                node2 = node1;

             node3 = TR::Node::create(TR::ishr, 2, node2,
                          TR::Node::create(node2, TR::iconst, 0, shiftAmount));

             if (divisor > 0)
                node4 = TR::Node::create(TR::iushr, 2, firstChild,
                          TR::Node::create(firstChild, TR::iconst, 0, 31));
             else
                node4 = TR::Node::create(TR::iushr, 2, node3,
                          TR::Node::create(node3, TR::iconst, 0, 31));

             s->prepareToReplaceNode(node);
             TR::Node::recreate(node, TR::iadd);
             node->setAndIncChild(0, node3);
             node->setAndIncChild(1, node4);
             node->setNumChildren(2);

// Disabled pending approval of 1055.
#ifdef TR_DESIGN_1055
             // leave idiv as is if the divisor is 2^n. CodeGen generates a fast instruction squence for it.
             // otherwise, expose the magic number squence to allow optimization
             // lowered tree will look like this:
             // iadd (node)
             //   ishr (node3)
             //     iadd (node2)
             //       iload [ dividend ]
             //       imulh (node1)
             //         ===>iload [ dividend ]
             //         iconst [ magic number ]
             //     iconst [ shift amount ]
             //   ishr (node4)
             //     ===>iload [ dividend ]
             //     iconst 31

            // Generate a new node that has the quotient calculated using the magic number method.
            TR::Node * quotient = getQuotientUsingMagicNumberMultiply(node, block, s);

            // Replace 'node' with our calculation.
            s->prepareToReplaceNode(node);
            TR::Node::recreate(node, quotient->getOpCodeValue());
            // Note that we don't have to incRef since the function call
            // already took care of this for us.
            node->setFirst(quotient->getFirstChild());
            node->setSecond(quotient->getSecondChild());
            node->setNumChildren(2);
#endif // Disabled pending approval of 1055.
            }
         }  // non-zero constant
      } // constant
   return node;
   }

TR::Node *ldivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getOpCodeValue() == TR::ludiv)
      {
      if (!node->getChild(0)->isNonNegative())
         return node;
      if (!node->getChild(1)->isNonNegative())
         return node;
      }

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   static const char * disableILDivPwr2Opt = feGetEnv("TR_DisableILDivPwr2Opt");

   // Only try to fold if the divisor is non-zero.
   // Handle the special case of dividing the maximum negative value by -1
   //
   if (secondChild->getOpCode().isLoadConst())
      {
      int64_t divisor = secondChild->getLongInt();
      if (divisor != 0)
         {
         if (firstChild->getOpCode().isLoadConst())
            {
            int64_t dividend = firstChild->getLongInt();
            if (divisor == -1 && dividend == TR::getMinSigned<TR::Int64>())
               return s->replaceNode(node, firstChild, s->_curTree);
            foldLongIntConstant(node, dividend / divisor, s, false /* !anchorChildren */);
            }
         else if (divisor == 1)
            return s->replaceNode(node, firstChild, s->_curTree);
         else if (divisor == -1)
            {
            if (performTransformation(s->comp(), "%sReduced ldiv by -1 with lneg in node [%p]\n", s->optDetailString(), node))
               {
               // make sure we don't lose grandchildren in prepareToReplaceNode
               firstChild->incReferenceCount();

               s->prepareToReplaceNode(node);
               TR::Node::recreate(node, TR::lneg);
               node->setChild(0, firstChild);
               node->setNumChildren(1);
               }
            }
         else if (s->cg()->getSupportsLoweringConstLDivPower2() && isPowerOf2(divisor))
            {
            // if power of 2 long divisor
            uint64_t value = divisor<0 ? -divisor : divisor;
            int32_t shiftAmount = 0;
            while ((value = (value >> 1)))
              ++shiftAmount;

            int32_t shftAmnt = -1;


            // if dividend can be proven to be positive
            if (firstChild->isNonNegative())
               {
               if (divisor < 0)
                  {
                  if (performTransformation(s->comp(), "%sReduced ldiv power of 2 with lneg lshr in node [%p]\n", s->optDetailString(), node))
                     {
                     TR::Node *newChild = TR::Node::create(secondChild, TR::iconst, 0);
                     newChild->setInt(shiftAmount);

                     TR::Node * node1 = TR::Node::create(TR::lshr, 2, firstChild, newChild);
                     s->prepareToReplaceNode(node);
                     TR::Node::recreate(node, TR::lneg);
                     node->setAndIncChild(0, node1);
                     node->setNumChildren(1);
                     }
                  }
               else
                  {
                  if (performTransformation(s->comp(), "%sReduced ldiv power of 2 with lshr in node [%p]\n", s->optDetailString(), node))
                     {
                     TR::Node::recreate(node, TR::lshr);
                     if (secondChild->getReferenceCount() > 1)
                        {
                        secondChild->decReferenceCount();
                        TR::Node * newChild = TR::Node::create(secondChild, TR::iconst, 0);
                        node->setAndIncChild(1, newChild);
                        secondChild = newChild;
                        }
                     else
                        {
                        TR::Node::recreate(secondChild, TR::iconst);
                        }
                     secondChild->setInt(shiftAmount);
                     s->_alteredBlock = true;
                     }
                  }
               }   // end positive dividend
            else if (firstChild->isNonPositive())
               {
               TR::Node * node1;
               TR::Node * shiftNode =  TR::Node::create(secondChild, TR::iconst, 0, shiftAmount);
               // dividend proven negative
               if (divisor < 0)
                  {
                  if (performTransformation(s->comp(), "%sReduced ldiv power of 2 - neg nominator with lshr lneg in node [%p]\n", s->optDetailString(), node))
                     {
                     node1 = TR::Node::create(TR::lneg, 1, firstChild );

                     s->prepareToReplaceNode(node);
                     TR::Node::recreate(node, TR::lshr);
                     node->setAndIncChild(0, node1);
                     node->setAndIncChild(1, shiftNode);
                     node->setNumChildren(2);
                     }
                  }
               else if ( divisor >= 0 )
                  {
                  if (performTransformation(s->comp(), "%sReduced ldiv power of 2 - neg nominator with lneg lneg in node [%p]\n", s->optDetailString(), node))
                     {
                     if (s->cg()->canUseImmedInstruction(divisor-1))
                        {
                        TR::Node *newChild = TR::Node::create(firstChild, TR::lconst, 0);
                        newChild->setLongInt(divisor-1);

                        node1 = TR::Node::create(TR::ladd, 2, firstChild, newChild);
                        s->prepareToReplaceNode(node);
                        TR::Node::recreate(node, TR::lshr);
                        node->setAndIncChild(0, node1);
                        node->setAndIncChild(1, shiftNode);
                        node->setNumChildren(2);
                        }
                     else
                        {
                        s->prepareToReplaceNode(node);
                        TR::Node::recreate(node, TR::ladd);
                        node->setAndIncChild(0, TR::Node::create(TR::lshr, 2, firstChild, shiftNode));

                        // This is a convulted way of expressing:
                        //    (firstChild && (divisor-1))?1:0
                        // PPC codegen did the whole in 2 instructions, but this
                        // sequence benefits other platforms. Hopefully, VP
                        // can collapse it in some cases.
                        TR::Node *node_const0or1, *node_sign, *node_divisor_1, *node_and, *node_add, *node_shl;
                        node_divisor_1 = TR::Node::create(secondChild, TR::lconst, 0);
                        node_divisor_1->setLongInt(divisor-1);
                        node_and = TR::Node::create(TR::land, 2, firstChild, node_divisor_1);

                        node_add = TR::Node::create(TR::ladd, 2, node_and, node_divisor_1);

                        node_shl = TR::Node::create(TR::lshl, 2, node_add, TR::Node::iconst(firstChild, 63-shiftAmount));

                        node_sign = TR::Node::create(TR::lshr, 2, node_shl, TR::Node::iconst(firstChild, 63));
                        node_const0or1 = TR::Node::create(TR::lneg, 1, node_sign);
                        node->setAndIncChild(1, node_const0or1);
                        node->setNumChildren(2);
                        }
                     }
                  }
               }   // end negative dividend
            // Design 1790
            else if ( (!disableILDivPwr2Opt) && ((shftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(divisor)) > 0) &&
               (secondChild->getReferenceCount()==1) && performTransformation(s->comp(), "%sPwr of 2 ldiv opt node %p\n", s->optDetailString(), node) )
               {
               secondChild->decReferenceCount();
               TR::Node * newNode1;
               if (shftAmnt-1 != 0)
                  {
                  newNode1 = TR::Node::create(node, TR::lshr, 2);
                  newNode1->setFirst(firstChild);
                  newNode1->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt-1));
                  newNode1->getSecondChild()->incReferenceCount();
                  newNode1->incReferenceCount();
                  }
               else
                  newNode1 = firstChild;
               TR::Node * newNode2 = TR::Node::create(node, TR::lushr, 2);
               newNode2->setFirst(newNode1);
               newNode2->setSecond(TR::Node::create(node, TR::iconst, 0, 64-shftAmnt));
               newNode2->getSecondChild()->incReferenceCount();
               TR::Node * newNode3 = TR::Node::create(node, TR::ladd, 2);
               newNode3->setFirst(newNode2);
               newNode3->setSecond(firstChild);
               newNode3->getFirstChild()->incReferenceCount();
               newNode3->getSecondChild()->incReferenceCount();
               if (divisor>0)
                  {
                  TR::Node::recreate(node, TR::lshr);
                  node->setFirst(newNode3);
                  node->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt));
                  node->getSecondChild()->incReferenceCount();
                  }
               else
                  {
                  TR::Node * newNode4 = TR::Node::create(node, TR::lshr, 2);
                  newNode4->setFirst(newNode3);
                  newNode4->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt));
                  newNode4->getFirstChild()->incReferenceCount();
                  newNode4->getSecondChild()->incReferenceCount();
                  TR::Node::recreate(node, TR::lneg);
                  node->setNumChildren(1);
                  node->setFirst(newNode4);
                  }
               node->getFirstChild()->incReferenceCount();
               }
            }       // end power of 2
         else if (s->cg()->getSupportsLoweringConstLDiv() && !isPowerOf2(divisor))
            {
             // otherwise, expose the magic number squence to allow optimization
             // lowered tree will look like this:
             // ladd (node)
             //   lshr (node3)
             //     ladd (node2)
             //       lload [ dividend ]
             //       lmulh (node1)
             //         ===>lload [ dividend ]
             //         lconst [ magic number ]
             //     lconst [ shift amount ]
             //   lshr (node4)
             //     ===>lload [ dividend ]
             //     lconst 63

             int64_t magicNumber, shiftAmount;
             s->cg()->compute64BitMagicValues(divisor, &magicNumber, &shiftAmount);

             TR::Node *node1, *node2, *node3, *node4;

             TR::Node *secondChild = TR::Node::create(firstChild, TR::lconst, 0);
             secondChild->setLongInt(magicNumber);

             // Multiply high divisor and magic number
             node1 = TR::Node::create(TR::lmulh, 2, firstChild, secondChild);

             if ( (divisor > 0) && ( magicNumber < 0 ) )
                {
                node2 = TR::Node::create(TR::ladd, 2, node1, firstChild);
                }
             else if ( (divisor < 0) && ( magicNumber > 0 ) )
                {
                node2 = TR::Node::create(TR::lsub, 2, node1, firstChild);
                }
             else
                node2 = node1;

             node3 = TR::Node::create(TR::lshr, 2, node2,
                                     TR::Node::create(node2, TR::iconst, 0, (int32_t)shiftAmount));

             if (divisor > 0)
                {
                node4 = TR::Node::create(TR::lushr, 2, firstChild,
                                        TR::Node::create(firstChild, TR::iconst, 0, 63));
                }
             else
                {
                node4 = TR::Node::create(TR::lushr, 2, node3,
                                        TR::Node::create(node3, TR::iconst, 0, 63));
                }
             s->prepareToReplaceNode(node);
             TR::Node::recreate(node, TR::ladd);
             node->setAndIncChild(0, node3);
             node->setAndIncChild(1, node4);
             node->setNumChildren(2);
            }  // divisor not power of 2
         }  // non-zero divisor
      } // second child is constant

   if (node->getOpCodeValue() == TR::ldiv)
      {
      firstChild = node->getFirstChild();
      secondChild = node->getSecondChild();
      if ((firstChild->getOpCodeValue() == TR::i2l) && (secondChild->getOpCodeValue() == TR::i2l) &&
          performTransformation(s->comp(), "%sReduced ldiv [%p] of two i2l children to i2l of idiv \n", s->optDetailString(), node))
         {
         TR::TreeTop *curTree = s->_curTree;
         TR::Node *divCheckParent = NULL;
         if ((curTree->getNode()->getOpCodeValue() == TR::DIVCHK) &&
             (curTree->getNode()->getFirstChild() == node))
            divCheckParent = curTree->getNode();

         TR::Node *divNode = TR::Node::create(TR::idiv, 2, firstChild->getFirstChild(), secondChild->getFirstChild());

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         //s->prepareToReplaceNode(node);
         TR::Node::recreate(node, TR::i2l);
         node->setAndIncChild(0, divNode);
         node->setNumChildren(1);

         if (divCheckParent)
            {
            divCheckParent->setAndIncChild(0, divNode);
            node->recursivelyDecReferenceCount();
            return divNode;
            }
         }

      if (secondChild->getOpCode().isLoadConst() && secondChild->getLongInt() == 10 &&
          !skipRemLoweringForPositivePowersOfTen(s) &&
          node->getFirstChild()->getOpCode().isLoadVar() &&     //transformToLongDiv creates a tonne of nodes.  Not good to do this recursively if you have chained divides
          performTransformation(s->comp(), "%sReduced ldiv by 10 [%p] to bitwise ops\n", s->optDetailString(), node))
         {
         TR::TreeTop *curTree = s->_curTree;
         TR::Node *divCheckParent = NULL;
         if ((curTree->getNode()->getOpCodeValue() == TR::DIVCHK) &&
             (curTree->getNode()->getFirstChild() == node))
            divCheckParent = curTree->getNode();

         transformToLongDivBy10Bitwise(node, node, s);
         TR::Node::recreate(node, TR::ladd);
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         if (divCheckParent)
            {
            divCheckParent->setAndIncChild(0, node);
            node->recursivelyDecReferenceCount();
            return node;
            }
         }
      }

   return node;
   }

TR::Node *fdivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanFloatOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getFloatBits() != FLOAT_POS_ZERO &&
       secondChild->getFloatBits() != FLOAT_NEG_ZERO)
      {
      if (firstChild->getOpCode().isLoadConst())
         {
         foldFloatConstant(node, TR::Compiler->arith.floatDivideFloat(firstChild->getFloat(), secondChild->getFloat()), s);
         return node;
         }
      if (isNZFloatPowerOfTwo(secondChild->getFloat()))
         {
         TR::Node::recreate(node, TR::fmul);
         float multiplier = floatRecip(secondChild->getFloat());
         if (secondChild->getReferenceCount() > 1)
            {
            secondChild->decReferenceCount();
            TR::Node * newChild = TR::Node::create(secondChild, TR::fconst, 0);
            node->setAndIncChild(1, newChild);
            secondChild = newChild;
            }
         secondChild->setFloat(multiplier);
         s->_alteredBlock = true;
         }
      }

   // In IEEE FP Arithmetic, f / 1.0 is f
   // Pretend to be an int before doing the comparison
   //
   BINARY_IDENTITY_OP(FloatBits, FLOAT_ONE)

   firstChild  = node->getFirstChild();
   secondChild = node->getSecondChild();

   bool firstChildNegated = (TR::fneg == firstChild->getOpCodeValue());
   bool secondChildNegated = (TR::fneg == secondChild->getOpCodeValue());
   bool bothChildrenNegated = (firstChildNegated && secondChildNegated);

   if (bothChildrenNegated)
      {
      if (performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] (-A)/(-B) -> A/B\n", s->optDetailString(), node))
         { // remove negates
         TR::Node * newFirstChild= s->replaceNode(firstChild,firstChild->getFirstChild(), s->_curTree);
         TR::Node * newSecondChild= s->replaceNode(secondChild,secondChild->getFirstChild(), s->_curTree);
         node->setChild(0,newFirstChild);
         node->setChild(1,newSecondChild);
         }
      }
   return node;
   }

TR::Node *ddivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanDoubleOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (secondChild->getOpCode().isLoadConst())
      {
      if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
         {
         foldDoubleConstant(node, TR::Compiler->arith.doubleDivideDouble(firstChild->getDouble(), secondChild->getDouble()), s);
         return node;
         }
      if (isNZDoublePowerOfTwo(secondChild->getDouble()))
         {
         TR::Node::recreate(node, TR::dmul);
         double multiplier = doubleRecip(secondChild->getDouble());
         if (secondChild->getReferenceCount() > 1)
            {
            secondChild->decReferenceCount();
            TR::Node * newChild = TR::Node::create(secondChild, TR::dconst, 0);
            node->setAndIncChild(1, newChild);
            secondChild = newChild;
            }
         secondChild->setDouble(multiplier);
         s->_alteredBlock = true;
         }
      }
   // In IEEE FP Arithmetic, d / 1.0 is d
   // Pretend to be an int before doing the comparison
   //
   BINARY_IDENTITY_OP(LongInt, DOUBLE_ONE)
   return node;
   }

TR::Node *bdivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, (int8_t)(firstChild->getByte() / secondChild->getByte()), s, false /* !anchorChildren*/);
      return node;
      }

   BINARY_IDENTITY_OP(Byte, 1)
   return node;
   }

TR::Node *sdivSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, (int16_t)(firstChild->getShortInt() / secondChild->getShortInt()), s, false /* !anchorChildren */);
      return node;
      }

   BINARY_IDENTITY_OP(ShortInt, 1)
   return node;
   }

//---------------------------------------------------------------------
// Remainder simplifiers
//

TR::Node *iremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   bool isUnsigned = node->getOpCode().isUnsigned();
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   static const char * disableILRemPwr2Opt = feGetEnv("TR_DisableILRemPwr2Opt");

   // Only try to fold if the divisor is non-zero.
   // Handle the special case of dividing the maximum negative value by -1
   //
   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t dividend = firstChild->getInt();
      int32_t divisor = secondChild->getInt();
      int32_t shftAmnt = -1;
      if (divisor != 0)
         {
         if (divisor == 1 || (!isUnsigned && (divisor == -1)))
            {
            foldIntConstant(node, 0, s, true /* anchorChildren */);
            return node;
            }
         else if (firstChild->getOpCode().isLoadConst())
            {
            int32_t result = (node->getOpCodeValue() == TR::iurem) ?
               (uint32_t)dividend%(uint32_t)divisor :
               dividend%divisor;

            foldIntConstant(node, result, s, false /* !anchorChildren */);
            return node;
            }
         else if ((!disableILRemPwr2Opt)&& (!isUnsigned || isNonNegativePowerOf2(divisor)) &&
            ((shftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(divisor)) > 0) &&
            (secondChild->getReferenceCount()==1) && performTransformation(s->comp(), "%sPwr of 2 irem opt node %p\n", s->optDetailString(), node) )
            {
            // Why is the following assymmetrical ?
            //      if (node->getOpCode().isUnsigned())
            if (node->getOpCodeValue() == TR::iurem)
               {
               secondChild->decReferenceCount();
               TR::Node *newNode = TR::Node::create(node, TR::iconst, 0, ((uint32_t)divisor) - 1);
               TR::Node::recreate(node, TR::iand);
               node->setSecond(newNode);
               node->getSecondChild()->incReferenceCount();
               return node;
               }
            else
               {
               secondChild->decReferenceCount();
               TR::Node * newNode1;
               if (shftAmnt-1 != 0)
                  {
                  newNode1 = TR::Node::create(node, TR::ishr, 2);
                  newNode1->setFirst(firstChild);
                  newNode1->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt-1));
                  newNode1->getSecondChild()->incReferenceCount();
                  newNode1->incReferenceCount();
                  }
               else
                  newNode1 = firstChild;
               TR::Node * newNode2 = TR::Node::create(node, TR::iushr, 2);
               newNode2->setFirst(newNode1);
               newNode2->setSecond(TR::Node::create(node, TR::iconst, 0, 32-shftAmnt));
               newNode2->getSecondChild()->incReferenceCount();
               TR::Node * newNode3 = TR::Node::create(node, TR::iadd, 2);
               newNode3->setFirst(newNode2);
               newNode3->setSecond(firstChild);
               newNode3->getFirstChild()->incReferenceCount();
               newNode3->getSecondChild()->incReferenceCount();
               TR::Node * newNode4 = TR::Node::create(node, TR::iand, 2);
               newNode4->setFirst(newNode3);
               newNode4->setSecond(TR::Node::create(node, TR::iconst, 0, divisor>0 ? -divisor : divisor));
               newNode4->getFirstChild()->incReferenceCount();
               newNode4->getSecondChild()->incReferenceCount();
               TR::Node::recreate(node, TR::isub);
               node->setFirst(firstChild);
               node->setSecond(newNode4);
               node->getFirstChild()->incReferenceCount();
               node->getSecondChild()->incReferenceCount();
               return node;
               }
            }
         else if (node->getOpCodeValue() == TR::irem &&
                  s->cg()->getSupportsLoweringConstIDiv() && !isPowerOf2(divisor) &&
                  !skipRemLowering(divisor, s) &&
                  performTransformation(s->comp(), "%sMagic number irem opt in node %p\n", s->optDetailString(), node))
            {
            // leave irem as is if the divisor is 2^n. CodeGen generates a fast instruction squence for it.
            // otherwise, expose the magic number squence to allow optimization
            // lowered tree will look like this:

            // isub
            //   iload [ dividend ]
            //   imul
            //     iconst divisor
            //     iadd (node)
            //       ishr (node3)
            //         iadd (node2)
            //           ===>iload [ dividend ]
            //           imulh (node1)
            //             ===>iload [ dividend ]
            //             iconst [ magic number ]
            //         iconst [ shift amount ]
            //       ishr (node4)
            //         ===>iload [ dividend ]
            //         iconst 31

            TR::Node * quotient, * node1;

            // Get the quotient using the magic number multiply method
            quotient = getQuotientUsingMagicNumberMultiply(node, block, s);

            node1 = TR::Node::create(TR::imul, 2, secondChild, quotient);

            // Replace 'node' with our calculation.
            s->prepareToReplaceNode(node);
            TR::Node::recreate(node, TR::isub);
            // Note that we have to incRef here since we haven't already
            // accounted for these references.
            node->setAndIncChild(0,firstChild);
            node->setAndIncChild(1,node1);
            node->setNumChildren(2);
            return node;
            }
         }
      }

   // TODO - strength reduction

   return node;
   }

TR::Node *lremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   static const char * disableILRemPwr2Opt = feGetEnv("TR_DisableILRemPwr2Opt");

   // Only try to fold if the divisor is non-zero.
   // Handle the special case of dividing the maximum negative value by -1
   //
   if (secondChild->getOpCode().isLoadConst())
      {
      int64_t divisor = secondChild->getLongInt();
      int32_t shftAmnt = -1;
      uint64_t udivisor = divisor;
      bool upwr2 = (udivisor & (udivisor - 1)) == 0;
      if (divisor != 0)
         {
         if (divisor == 1 || (divisor == -1))
            {
            foldLongIntConstant(node, 0, s, true /* anchorChildren */);
            return node;
            }
         else if (firstChild->getOpCode().isLoadConst())
            {
            int64_t dividend = firstChild->getLongInt();
            if (divisor == -1 && dividend == TR::getMinSigned<TR::Int64>())
               foldLongIntConstant(node, 0, s, false /* !anchorChildren */);
            else
               foldLongIntConstant(node, dividend % divisor, s, false /* !anchorChildren */);
            return node;
            }
         // Design 1792
         else if ((!disableILRemPwr2Opt) && ((shftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(divisor)) > 0) &&
            (secondChild->getReferenceCount()==1) && performTransformation(s->comp(), "%sPwr of 2 lrem opt node %p\n", s->optDetailString(), node) )
            {
            secondChild->decReferenceCount();
            TR::Node * newNode1;
            if (shftAmnt-1 != 0)
               {
               newNode1 = TR::Node::create(node, TR::lshr, 2);
               newNode1->setFirst(firstChild);
               newNode1->setSecond(TR::Node::create(node, TR::iconst, 0, shftAmnt-1));
               newNode1->getSecondChild()->incReferenceCount();
               newNode1->incReferenceCount();
               }
            else
               newNode1 = firstChild;
            TR::Node * newNode2 = TR::Node::create(node, TR::lushr, 2);
            newNode2->setFirst(newNode1);
            newNode2->setSecond(TR::Node::create(node, TR::iconst, 0, 64-shftAmnt));
            newNode2->getSecondChild()->incReferenceCount();
            TR::Node * newNode3 = TR::Node::create(node, TR::ladd, 2);
            newNode3->setFirst(newNode2);
            newNode3->setSecond(firstChild);
            newNode3->getFirstChild()->incReferenceCount();
            newNode3->getSecondChild()->incReferenceCount();
            TR::Node * newNode4 = TR::Node::create(node, TR::land, 2);
            newNode4->setFirst(newNode3);
            int64_t negDivisor = divisor>0 ? -divisor : divisor;
            TR::Node *lconstNode = TR::Node::create(node, TR::lconst, 0, 0);
            lconstNode->setLongInt(negDivisor);
            newNode4->setSecond(lconstNode);
            newNode4->getSecondChild()->setLongInt(negDivisor);
            newNode4->getFirstChild()->incReferenceCount();
            newNode4->getSecondChild()->incReferenceCount();
            TR::Node::recreate(node, TR::lsub);
            node->setFirst(firstChild);
            node->setSecond(newNode4);
            node->getFirstChild()->incReferenceCount();
            node->getSecondChild()->incReferenceCount();
            return node;
            }
         // Disabled pending approval of design 1055.
#ifdef TR_DESIGN_1055
         else if (s->cg()->getSupportsLoweringConstLDiv() && !isPowerOf2(divisor) && !skipRemLowering(divisor, s))
            {
            // otherwise, expose the magic number squence to allow optimization
            // lowered tree will look like this:
            // ladd (node)
            //   lshr (node3)
            //     ladd (node2)
            //       lload [ dividend ]
            //       lmulh (node1)
            //         ===>lload [ dividend ]
            //         lconst [ magic number ]
            //     lconst [ shift amount ]
            //   lshr (node4)
            //     ===>lload [ dividend ]
            //     lconst 63

            TR::Node * quotient, * node1;

            // Get the quotient using the magic number multiply method
            quotient = getQuotientUsingMagicNumberMultiply(node, block, s);

            node1 = TR::Node::create(TR::lmul, 2, secondChild, quotient);

            // Replace 'node' with our calculation.
            s->prepareToReplaceNode(node);
            TR::Node::recreate(node, TR::lsub);
            // We need to incRef here because we've not accounted for these 2
            // references yet.
            node->setAndIncChild(0,firstChild);
            node->setAndIncChild(1,node1);
            node->setNumChildren(2);
            return node;
            }  // divisor not power of 2
#endif  // Disabled pending approval of design 1055.
         }
      }

   if (node->getOpCodeValue() == TR::lrem)
      {
      firstChild = node->getFirstChild();
      secondChild = node->getSecondChild();

      if ((firstChild->getOpCodeValue() == TR::i2l) && (secondChild->getOpCodeValue() == TR::i2l) &&
          performTransformation(s->comp(), "%sReduced lrem [%p] of two i2l children to i2l of irem \n", s->optDetailString(), node))
         {
         TR::TreeTop *curTree = s->_curTree;
         TR::Node *divCheckParent = NULL;
         if ((curTree->getNode()->getOpCodeValue() == TR::DIVCHK) &&
             (curTree->getNode()->getFirstChild() == node))
            divCheckParent = curTree->getNode();

         TR::Node *remNode = TR::Node::create(TR::irem, 2, firstChild->getFirstChild(), secondChild->getFirstChild());

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         TR::Node::recreate(node, TR::i2l);
         node->setAndIncChild(0, remNode);
         node->setNumChildren(1);

         if (divCheckParent)
            {
            divCheckParent->setAndIncChild(0, remNode);
            node->recursivelyDecReferenceCount();
            return remNode;
            }
         return node;
         }

      if (secondChild->getOpCode().isLoadConst() && secondChild->getLongInt() == 10 &&
          !skipRemLoweringForPositivePowersOfTen(s) &&
          node->getFirstChild()->getOpCode().isLoadVar() &&
          performTransformation(s->comp(), "%sReduced lrem by 10 [%p] to sequence of bitwise operations\n", s->optDetailString(), node))
         {
         TR::TreeTop *curTree = s->_curTree;
         TR::Node *divCheckParent = NULL;
         if ((curTree->getNode()->getOpCodeValue() == TR::DIVCHK) &&
             (curTree->getNode()->getFirstChild() == node))
            divCheckParent = curTree->getNode();

         // Create the long divide tree
        TR::Node * ldivNode = TR::Node::create(node, TR::ladd);
         transformToLongDivBy10Bitwise(node, ldivNode, s);

         // Modify the lrem node
         TR::Node::recreate(node, TR::lsub);
         node->setNumChildren(2);
         node->setAndIncChild(0, firstChild);
         node->setAndIncChild(1, TR::Node::create(TR::lmul, 2, ldivNode, secondChild));
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         if (divCheckParent)
            {
            divCheckParent->setAndIncChild(0, node);
            node->recursivelyDecReferenceCount();
            return node;
            }
         return node;
         }
      }

   // TODO - strength reduction

   return node;
   }

TR::Node *fremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanFloatOp(node, firstChild, secondChild, s)))
      return result;

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getFloatBits() != FLOAT_POS_ZERO &&
       secondChild->getFloatBits() != FLOAT_NEG_ZERO)
      {
      if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
         {
         foldFloatConstant(node, TR::Compiler->arith.floatRemainderFloat(firstChild->getFloat(), secondChild->getFloat()), s);
         return node;
         }
      }

   secondChild = node->getSecondChild();

   bool secondChildNegated = (TR::fneg == secondChild->getOpCodeValue());

   if (secondChildNegated) // has no effect on sign--just remove it
      {
      if (performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] A%%(-B) -> A%%B\n", s->optDetailString(), node))
        { // remove negate
        TR::Node * newSecondChild= s->replaceNode(secondChild,secondChild->getFirstChild(), s->_curTree);
        node->setChild(1,newSecondChild);
        }
      }
   return node;
   }

TR::Node *dremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   TR::Node * result;
   if ((result = binaryNanDoubleOp(node, firstChild, secondChild, s)) != NULL)
      return result;

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, TR::Compiler->arith.doubleRemainderDouble(firstChild->getDouble(), secondChild->getDouble()), s);
      return node;
      }

   return node;
   }

TR::Node *bremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, (int8_t)(firstChild->getByte() % secondChild->getByte()), s, false /* !anchorChildren*/);
      return node;
      }

   return node;
   }

TR::Node *sremSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, (int16_t)(firstChild->getShortInt() % secondChild->getShortInt()), s, false /* !anchorChildren */);
      return node;
      }

   return node;
   }

//---------------------------------------------------------------------
// Negate simplifiers
//

TR::Node *inegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, -firstChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   TR::ILOpCodes opCode = firstChild->getOpCodeValue();
   if (opCode == TR::ineg)
      {
      if (performTransformation(s->comp(), "%sCancelled out ineg with ineg child in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         TR::Node * grandChild = firstChild->getFirstChild();
         node = s->replaceNode(node, grandChild, s->_curTree);
         s->_alteredBlock = true;
         }
      }
   else if (opCode == TR::isub)
      {
      if (performTransformation(s->comp(), "%sReduced ineg with isub child in node [" POINTER_PRINTF_FORMAT "] to isub\n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::isub);
         node->setNumChildren(2);
         node->setAndIncChild(0, firstChild->getSecondChild());
         node->setAndIncChild(1, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         s->_alteredBlock = true;
         }
      }
   else if (opCode == TR::l2i &&
            firstChild->getFirstChild()->getOpCodeValue() == TR::lushr &&
            firstChild->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      TR::Node* firstFirst = firstChild->getFirstChild();
      uint32_t shiftBy = firstFirst->getSecondChild()->getInt();
      if (shiftBy == 63 &&
          performTransformation(s->comp(), "%sReplaced ineg of lushr by 63 with lshr node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         TR::Node* shift = TR::Node::create(node, TR::lshr, 2);
         shift->setAndIncChild(0, firstFirst->getFirstChild());
         shift->setAndIncChild(1, firstFirst->getSecondChild());
         TR::Node::recreate(node, TR::l2i);
         node->setAndIncChild(0, shift);
         firstChild->recursivelyDecReferenceCount();
         }
      }

   return node;
   }

TR::Node *lnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, -firstChild->getLongInt(), s, false /* !anchorChildren */);
      return node;
      }

   TR::ILOpCodes opCode = firstChild->getOpCodeValue();
   if (opCode == TR::lneg)
      {
      if (performTransformation(s->comp(), "%sCancelled lneg with lneg child in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         TR::Node * grandChild = firstChild->getFirstChild();
         node = s->replaceNode(node, grandChild, s->_curTree);
         s->_alteredBlock = true;
         }
      }
   else if (opCode == TR::lsub)
      {
      if (performTransformation(s->comp(), "%sReduced lneg with lsub child in node [" POINTER_PRINTF_FORMAT "]\n to lsub", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::lsub);
         node->setNumChildren(2);
         node->setAndIncChild(0, firstChild->getSecondChild());
         node->setAndIncChild(1, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   return node;
   }

TR::Node *fnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldFloatConstant(node, TR::Compiler->arith.floatNegate(firstChild->getFloat()), s);
      return node;
      }

   // Make this  work for -(0-0)
   //
   if (firstChild->getOpCodeValue() == TR::fneg)
     {
      if (performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] --A -> A\n", s->optDetailString(), node))
        {
        node= s->replaceNode(node,firstChild->getFirstChild(), s->_curTree); // remove neg
        }
     }
   // don't just do -A op B -> -(A op B) as not always a win, so catch opportunity here
   else if ((firstChild->getOpCodeValue() == TR::fmul ||
           firstChild->getOpCodeValue() == TR::fdiv ||
           firstChild->getOpCodeValue() == TR::frem))
      {
      TR::Node * firstOpChild  = firstChild->getFirstChild();
      TR::Node * secondOpChild = firstChild->getSecondChild();
      TR::Node * child2Change=NULL;
      uint32_t childNo=0;
      // we've already simplified the children, so won't get both
      // children as negates

      if (firstOpChild->getOpCodeValue()==TR::fneg)
         {
         child2Change = firstOpChild;
         childNo=0;
         }
      else if (secondOpChild->getOpCodeValue()==TR::fneg &&
              firstChild->getOpCodeValue() != TR::frem)  // not valid for rem
         {
         child2Change = secondOpChild;
         childNo=1;
         }
      if (child2Change && child2Change->getReferenceCount() == 1 && performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] -(-A op B) -> A op B (op=*,/,%%)\n", s->optDetailString(), node))
        {
        // remove neg
        TR::Node * newChild= s->replaceNode(child2Change,child2Change->getFirstChild(), s->_curTree);
        firstChild->setChild(childNo,newChild);
        // now replace current node with fmul,fdiv or frem
        node= s->replaceNode(node,node->getFirstChild(), s->_curTree);
        }
   }
   else if (s->comp()->cg()->supportsNegativeFusedMultiplyAdd())
      {
      // unless we've already got an inserted multiply, transformit
      if ((firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub()) &&
          !(firstChild->getFirstChild()->isFPStrictCompliant() || firstChild->getSecondChild()->isFPStrictCompliant()) &&
          performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] -(-A +/- B) -> -((A*1)+/-B)\n", s->optDetailString(), node))
         {
         TR::Node * newConst = TR::Node::create(firstChild, TR::fconst, 0);
         newConst->setFloat(1);
         TR::Node * newMul = TR::Node::create(firstChild,TR::fmul, 2);
         newMul->setAndIncChild(0, firstChild->getFirstChild());
         newMul->setAndIncChild(1, newConst);
         s->replaceNode(firstChild->getFirstChild(),newMul, s->_curTree);
         firstChild->setChild(0,newMul);
         newMul->setIsFPStrictCompliant(true);
         }
      else if (firstChild->getOpCode().isMul() &&
          performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] -(A*B) -> -((A*B)-0)\n", s->optDetailString(), node))
         {
         TR::Node * newConst = TR::Node::create(firstChild, TR::fconst, 0);
         newConst->setFloat(0);
         TR::Node * newAdd = TR::Node::create(firstChild,TR::fsub, 2);
         newAdd->setAndIncChild(0, firstChild);
         newAdd->setAndIncChild(1, newConst);
         s->replaceNode(firstChild,newAdd, s->_curTree);
         node->setChild(0,newAdd);
         firstChild->setIsFPStrictCompliant(true);
         }
      }

   return node;
   }

TR::Node *dnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, TR::Compiler->arith.doubleNegate(firstChild->getDouble()), s);
      return node;
      }

   // Make this work for -(0-0)

   if (s->comp()->cg()->supportsNegativeFusedMultiplyAdd())
      {
      if ((firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub()) &&
     !(firstChild->getFirstChild()->isFPStrictCompliant() || firstChild->getSecondChild()->isFPStrictCompliant()) &&
          performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] -(-A +/- B) -> -((A*1)+/-B)\n", s->optDetailString(), node))
         {
         TR::Node * newConst = TR::Node::create(firstChild->getFirstChild(), TR::dconst, 0);
         TR::Node * newMul = TR::Node::create(firstChild, TR::dmul, 2);
         newConst->setDouble(1);
         newMul->setAndIncChild(1, newConst);
         newMul->setAndIncChild(0, firstChild->getFirstChild());
         s->replaceNode(firstChild->getFirstChild(),newMul, s->_curTree);

         firstChild->setChild(0,newMul);
         newMul->setIsFPStrictCompliant(true);
         }
      else if (firstChild->getOpCode().isMul() &&
          performTransformation(s->comp(), "%sTransforming [" POINTER_PRINTF_FORMAT "] -(A*B) -> -((A*B)-0)\n", s->optDetailString(), node))
         {
         TR::Node * newConst = TR::Node::create(firstChild, TR::dconst, 0);
         TR::Node * newAdd = TR::Node::create(firstChild, TR::dsub, 2);
         newConst->setDouble(0);
         newAdd->setAndIncChild(0, firstChild);
         newAdd->setAndIncChild(1, newConst);
         s->replaceNode(firstChild,newAdd, s->_curTree);
         node->setChild(0,newAdd);
         firstChild->setIsFPStrictCompliant(true);
         }
      }

   return node;
   }

TR::Node *bnegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, -firstChild->getByte(), s, false /* !anchorChildren*/);
      }

   return node;
   }

TR::Node *snegSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, -firstChild->getShortInt(), s, false /* !anchorChildren */);
      }
   return node;
   }

TR::Node *constSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   return node;
   }

TR::Node *lconstSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   setIsHighWordZero(node, s);
   return node;
   }

TR::Node *iabsSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   auto child = node->getChild(0);
   if (child->isNonNegative() &&
       performTransformation(s->comp(), "%sSimplify iabs of non-negative child at [" POINTER_PRINTF_FORMAT "]\n",
             s->optDetailString(), node))
      {
      return s->replaceNodeWithChild(node, child, s->_curTree, block);
      }
   return node;
   }

TR::Node *labsSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   auto child = node->getChild(0);
   if (child->isNonNegative() &&
       performTransformation(s->comp(), "%sSimplify labs of non-negative child at [" POINTER_PRINTF_FORMAT "]\n",
             s->optDetailString(), node))
      {
      return s->replaceNodeWithChild(node, child, s->_curTree, block);
      }
   return node;
   }

//---------------------------------------------------------------------
// Shift left simplifiers
//

TR::Node *ishlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()<<(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren*/);
      return node;
      }

   normalizeConstantShiftAmount(node, INT_SHIFT_MASK, secondChild, s);
   BINARY_IDENTITY_OP(Int, 0)

   if (secondChild->getOpCode().isLoadConst() &&
       performTransformation(s->comp(), "%sChanged ishl by const into imul by const in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
      {
      // Normalize shift by a constant into multiply by a constant
      //
      TR::Node::recreate(node, node->getOpCodeValue() == TR::iushl ? TR::iumul : TR::imul);
      int32_t multiplier = 1 << (secondChild->getInt() & INT_SHIFT_MASK);
      if (secondChild->getReferenceCount() > 1)
         {
         secondChild->decReferenceCount();
         TR::Node * newChild = TR::Node::create(secondChild, TR::iconst, 0);
         node->setAndIncChild(1, newChild);
         secondChild = newChild;
         }
      secondChild->setInt(multiplier);
      s->_alteredBlock = true;
      }
   else
      normalizeShiftAmount(node, 31, s);

   return node;
   }

TR::Node *lshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getLongInt() << (secondChild->getInt() & LONG_SHIFT_MASK), s, false /* !anchorChildren */);
      return node;
      }

   normalizeConstantShiftAmount(node, LONG_SHIFT_MASK, secondChild, s);
   BINARY_IDENTITY_OP(Int, 0)

   if (secondChild->getOpCode().isLoadConst())
      {
      // Canonicalize shift by a constant into multiply by a constant
      //
      performTransformation(s->comp(), "%sCanonicalize long left shift by constant in node [" POINTER_PRINTF_FORMAT "] to long multiply by power of 2\n", s->optDetailString(), node);
      TR::Node::recreate(node, TR::lmul);
      int64_t multiplier = (int64_t)CONSTANT64(1) << (secondChild->getInt() & LONG_SHIFT_MASK);
      if (secondChild->getReferenceCount() > 1)
         {
         secondChild->decReferenceCount();
         TR::Node * newChild = TR::Node::create(secondChild, TR::lconst, 0);
         node->setAndIncChild(1, newChild);
         secondChild = newChild;
         }
      else
         {
         TR::Node::recreate(secondChild, TR::lconst);
         }
      secondChild->setLongInt(multiplier);
      s->_alteredBlock = true;
      }
   else
      normalizeShiftAmount(node, 63, s);

   return node;
   }

TR::Node *bshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, firstChild->getByte()<<(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren*/);
      return node;
      }

   BINARY_IDENTITY_OP(Int, 0)
   return node;
   }

TR::Node *sshlSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getShortInt()<<(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren */);
      return node;
      }
   BINARY_IDENTITY_OP(Int, 0)
   return node;
   }

//---------------------------------------------------------------------
// Shift right simplifiers
//

TR::Node *ishrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()>>(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren*/);
      return node;
      }

   normalizeConstantShiftAmount(node, INT_SHIFT_MASK, secondChild, s);
   BINARY_IDENTITY_OP(Int, 0)

   normalizeShiftAmount(node, 31, s);

   return node;
   }

TR::Node *lshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getLongInt() >> (secondChild->getInt() & LONG_SHIFT_MASK), s, false /* !anchorChildren */);
      return node;
      }

   normalizeConstantShiftAmount(node, LONG_SHIFT_MASK, secondChild, s);
   BINARY_IDENTITY_OP(Int, 0)

   normalizeShiftAmount(node, 63, s);
   return node;
   }

TR::Node *bshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, firstChild->getByte()>>(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren*/);
      return node;
      }

   BINARY_IDENTITY_OP(Int, 0)
   return node;
   }

TR::Node *sshrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getShortInt()>>(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren */);
      return node;
      }
   BINARY_IDENTITY_OP(Int, 0)
   return node;
   }

//---------------------------------------------------------------------
// Unsigned shift right simplifiers
//

TR::Node *iushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldUIntConstant(node, firstChild->getUnsignedInt()>>(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren*/);
      return node;
      }

   normalizeConstantShiftAmount(node, INT_SHIFT_MASK, secondChild, s);
   BINARY_IDENTITY_OP(Int, 0)


   // look for bogus code sequence used in compress to zero extend a short
   // using two shifts.  Also catches pairs of shifts that can be performed
   // by one "and" operation.
   if (secondChild->getOpCodeValue() == TR::iconst &&
       firstChild->getOpCodeValue() == TR::imul)
      {
      TR::Node * constGrandChild = firstChild->getSecondChild();
      int32_t  rightShiftValue = secondChild->getInt() & INT_SHIFT_MASK;
      if (constGrandChild->getOpCodeValue() == TR::iconst &&
          constGrandChild->getInt() == (1 << rightShiftValue))
         {
         TR::Node     * subTree            = firstChild->getFirstChild();
         TR::ILOpCodes opCode             = subTree->getOpCodeValue();
         bool         foundZeroExtension = false;
         if (subTree->getReferenceCount() == 1)
            {
            if (opCode == TR::s2i && rightShiftValue == 16)
               {
               if (performTransformation(s->comp(), "%sReduced left shift followed by iushr equivalent to zero extend short in node [" POINTER_PRINTF_FORMAT "] to su2i\n", s->optDetailString(), node))
                  {
                  TR::Node::recreate(node, TR::su2i);
                  foundZeroExtension = true;
                  }
               }
            else if (opCode == TR::b2i && rightShiftValue == 24)
               {
               if (performTransformation(s->comp(), "%sReduced left shift followed by iushr equivalent to zero extend byte in node [" POINTER_PRINTF_FORMAT "] to bu2i\n", s->optDetailString(), node))
                  {
                  TR::Node::recreate(node, TR::bu2i);
                  foundZeroExtension = true;
                  }
               }
            if (foundZeroExtension)
               {
               node->setVisitCount(0);
               node->setAndIncChild(0, subTree->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setNumChildren(1);
               secondChild->recursivelyDecReferenceCount();
               s->_alteredBlock = true;
               return node;
               }
            }

         // replace the two shifts with a TR::iand to mask off the high bits
         //
         if (performTransformation(s->comp(), "%sReduced left shift followed by iushr in node [" POINTER_PRINTF_FORMAT "] to iand with mask\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::iand);
            uint32_t mask = (UINT_MAX >> rightShiftValue);
            if (secondChild->getReferenceCount() == 1)
               secondChild->setInt(mask);
            else
               {
               node->setAndIncChild(1, TR::Node::iconst(secondChild, mask));
               secondChild->decReferenceCount();
               }
            node->setAndIncChild(0, subTree);
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            return s->simplify(node, block);
            }
         }
      }

   normalizeShiftAmount(node, 31, s);
   return node;
   }

TR::Node *lushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   // Shift left followed by unsigned shift right can become:
   // shift right and conversion to byte, char, or int
   //
   if (secondChild->getOpCode().isLoadConst() &&
      (firstChild->getOpCodeValue() == TR::lushl ||
       firstChild->getOpCodeValue() == TR::lshl) &&
       firstChild->getSecondChild()->getOpCode().isLoadConst())
      {
      int64_t left = firstChild->getSecondChild()->get64bitIntegralValue() & LONG_SHIFT_MASK;
      int64_t right = secondChild->get64bitIntegralValue() & LONG_SHIFT_MASK;

      if (right >= left && (right == 56 || right == 48 || right == 32) &&
          performTransformation(s->comp(), "%sshift left followed by shift right %p of %d can become a shift + conversion\n",
                                 s->optDetailString(),
                                 node, right))
         {
         TR::ILOpCodes op1, op2;
         if (right == 56)
            {
            op1 = TR::l2b; op2 = TR::bu2l;
            }
         else if (right == 48)
            {
            op1 = TR::l2s; op2 = TR::su2l;
            }
         else
            {
            op1 = TR::l2i; op2 = TR::iu2l;
            }

         TR::Node *amount = TR::Node::create(node, TR::iconst, 0);
         amount->setInt((int32_t)(right-left));
         TR::Node *shift = TR::Node::create(TR::lushr, 2, firstChild->getFirstChild(), amount);
         TR::Node *new_node = TR::Node::create(op1, 1, shift);
         new_node = TR::Node::create(op2, 1, new_node);
         return s->simplify(s->replaceNode(node, new_node, s->_curTree), block);
         }
      }

   simplifyChildren(node, block, s);

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, ((uint64_t)firstChild->getLongInt()) >> (secondChild->getInt() & LONG_SHIFT_MASK), s, false /* !anchorChildren */);
      return node;
      }

   normalizeConstantShiftAmount(node, LONG_SHIFT_MASK, secondChild, s);
   BINARY_IDENTITY_OP(Int, 0)

   // look for bogus code sequence used in compress to zero extend a short
   // using two shifts.  Also catches pairs of shifts that can be performed
   // by one "and" operation.
   if (secondChild->getOpCodeValue() == TR::iconst &&
       firstChild->getOpCodeValue() == TR::lmul)
      {
      TR::Node * constGrandChild = firstChild->getSecondChild();
      int32_t  rightShiftValue = secondChild->getInt() & LONG_SHIFT_MASK;
      if (constGrandChild->getOpCodeValue() == TR::lconst &&
          constGrandChild->getLongInt() == (CONSTANT64(1) << rightShiftValue))
         {
         TR::Node     * subTree            = firstChild->getFirstChild();
         TR::ILOpCodes opCode             = subTree->getOpCodeValue();
         bool         foundZeroExtension = false;
         if (subTree->getReferenceCount() == 1)
            {
            if (opCode == TR::i2l && rightShiftValue == 32)
               {
               if (performTransformation(s->comp(), "%sReduced left shift followed by lushr equivalent to zero extend int in node [" POINTER_PRINTF_FORMAT "] to iu2l\n", s->optDetailString(), node))
                  {
                  foundZeroExtension = true;
                  TR::Node::recreate(node, TR::iu2l);
                  }
               }
            else if (opCode == TR::s2l && rightShiftValue == 48)
               {
               if (performTransformation(s->comp(), "%sReduced left shift followed by lushr equivalent to zero extend byte in node [" POINTER_PRINTF_FORMAT "] to bu2l\n", s->optDetailString(), node))
                  {
                  foundZeroExtension = true;
                  TR::Node::recreate(node, TR::su2l);
                  }
               }
            else if (opCode == TR::b2l && rightShiftValue == 56)
               {
               if (performTransformation(s->comp(), "%sReduced left shift followed by lushr equivalent to zero extend byte in node [" POINTER_PRINTF_FORMAT "] to bu2l\n", s->optDetailString(), node))
                  {
                  foundZeroExtension = true;
                  TR::Node::recreate(node, TR::bu2l);
                  }
               }
            if (foundZeroExtension)
               {
               node->setNumChildren(1);
               TR::Node * subChild = subTree->getFirstChild();
               node->setAndIncChild(0, subChild);
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               return node;
               }
            }

         // replace the two shifts with a TR::land to mask off the high bits
         //
         if (performTransformation(s->comp(), "%sReduced left shift followed by lushr in node [" POINTER_PRINTF_FORMAT "] to land with mask\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::land);
            uint64_t mask = (uint64_t) ((uint64_t) -1) >> rightShiftValue;
            if (secondChild->getReferenceCount() == 1)
               {
               TR::Node::recreate(secondChild, TR::lconst);
               secondChild->setLongInt(mask);
               }
            else
               {
               node->setAndIncChild(1, TR::Node::lconst( secondChild, mask));
               secondChild->decReferenceCount();
               }
            node->setAndIncChild(0, subTree);
            firstChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            return s->simplify(node, block);
            }
         }
      }

   normalizeShiftAmount(node, 63, s);
   return node;
   }

TR::Node *bushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, ((uint8_t)firstChild->getByte())>>(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren*/);
      return node;
      }

   BINARY_IDENTITY_OP(Int, 0)
   return node;
   }

TR::Node *sushrSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getUnsignedShortInt()>>(secondChild->getInt() & INT_SHIFT_MASK), s, false /* !anchorChildren */);
      return node;
      }
   BINARY_IDENTITY_OP(Int, 0)
   return node;
   }

//---------------------------------------------------------------------
// Rotate left
//

TR::Node *irolSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      int32_t rotateAmount = secondChild->getInt() & INT_SHIFT_MASK;
      rotateAmount &= 31; // to ensure that 32-rotateAmount below isn't negative.
      int32_t value = (firstChild->getUnsignedInt() << rotateAmount) | (firstChild->getUnsignedInt() >> (32-rotateAmount));

      foldIntConstant(node, value, s, false);
      return node;
      }

   if (secondChild->getOpCode().isLoadConst() && secondChild->getInt()%32 == 0)
      {
      return s->replaceNode(node, firstChild, s->_curTree);
      }

   normalizeShiftAmount(node, 31, s);
   return node;
   }

TR::Node *lrolSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      int32_t rotateAmount = secondChild->getInt() & LONG_SHIFT_MASK;
      uint64_t value = (firstChild->getUnsignedLongInt() << rotateAmount) | (firstChild->getUnsignedLongInt() >> (64-rotateAmount));

      foldLongIntConstant(node, value, s, false);
      return node;
      }

   if (secondChild->getOpCode().isLoadConst() && secondChild->getInt()%64 == 0 )
      {
      return s->replaceNode(node, firstChild, s->_curTree);
      }

   normalizeShiftAmount(node, 63, s);
   return node;
   }

//---------------------------------------------------------------------
// Boolean AND
//

TR::Node *iandSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()&secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int32_t, Int, -1, 0)

   if (TR::Node* foldedNode = tryFoldAndWidened(s, node))
      {
      return foldedNode;
      }

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // look for demorgan's law.  if found can remove one bitwise complement operation
   if (firstChild->getReferenceCount() == 1)
      {
      if (isBitwiseIntComplement(firstChild)    &&
          secondChild->getReferenceCount() == 1 &&
          isBitwiseIntComplement(secondChild))
         {
         if (performTransformation(s->comp(), "%sReduced iand with two complemented children in node [%s] to complemented ior\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            TR::Node *orNode    = TR::Node::create(TR::ior, 2, firstChild->getFirstChild(), secondChild->getFirstChild());
            TR::Node * constNode = firstChild->getSecondChild();
            TR::Node::recreate(node, TR::ixor);
            node->setAndIncChild(0, orNode);
            node->setAndIncChild(1, constNode);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node = s->simplify(node, block);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      // look for reassociation to fold constants
      else if (firstChildOp == TR::iand)
         {
         TR::Node * lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::iconst)
            {
            if (secondChildOp == TR::iconst || (secondChildOp == TR::iuconst && lrChild->chkUnsigned()))
               {
               if (performTransformation(s->comp(), "%sFound iand of iconst with iand of x and iconst in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
                  {
                  if (secondChild->getReferenceCount() == 1)
                     {
                     secondChild->setInt(secondChild->getInt() & lrChild->getInt());
                     }
                  else
                     {
                     TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::iconst, 0);
                     foldedConstChild->setInt(secondChild->getInt() & lrChild->getInt());
                     node->setSecond(s->replaceNode(secondChild,foldedConstChild, s->_curTree));
                     }
                  node->setFirst(s->replaceNode(firstChild,firstChild->getFirstChild(), s->_curTree));
                  s->_alteredBlock = true;
                  }
               }
            else // move constants up the tree so they will tend to get merged together
               {
               if (performTransformation(s->comp(), "%sFound iand of non-iconst with iand x and iconst in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
                  {
                  node->setChild(1, lrChild);
                  firstChild->setChild(1, secondChild);
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  }
               }
            }
         }
      }
   // look for opportunities to perform zero extending conversions
   if (secondChild->getOpCodeValue() == TR::iconst)
      {
      int32_t      maskValue          = secondChild->getInt();
      TR::ILOpCodes firstChildOp       = firstChild->getOpCodeValue();
      bool         foundZeroExtension = false;
      if (maskValue == 255 && firstChildOp == TR::b2i)
         {
         if (performTransformation(s->comp(), "%sReduced iand with iconst 255 in node [%s] to bu2i\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            TR::Node::recreate(node, TR::bu2i);
            foundZeroExtension = true;
            }
         }
      else if (maskValue == 65535 && (firstChildOp == TR::s2i || firstChildOp == TR::su2i))
         {
         if (performTransformation(s->comp(), "%sReduced iand with iconst 65536 in node [%s] to %s\n", s->optDetailString(), node->getName(s->getDebug()), "su2i"))
            {
            TR::Node::recreate(node, TR::su2i);
            foundZeroExtension = true;
            }
         }
      if (foundZeroExtension)
         {
         node->setNumChildren(1);
         node->setAndIncChild(0, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      else
         {
         if (maskValue >= 0 && maskValue < 256)
            changeConverts2Unsigned(firstChild, TR::b2i, s);
         if (maskValue >= 0 && maskValue < 65536)
            changeConverts2Unsigned(firstChild, TR::s2i, s);
         }
      }


   if ((node->getOpCodeValue() == TR::iand) && node->getSecondChild()->getOpCode().isLoadConst())
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      if (firstChild->getOpCode().isBooleanCompare())
         {
         if ((secondChild->getInt() & 0x1) == 0x1)
            {
            // (A cmp B) & 0x1 ==> A cmp B
            TR::Node::recreate(node, firstChild->getOpCodeValue());
            node->setNumChildren(2);
            node->setAndIncChild(0, firstChild->getFirstChild());
            node->setAndIncChild(1, firstChild->getSecondChild());

            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            return node;
            }
         }
      }

   return node;
   }

TR::Node *landSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getLongInt() & secondChild->getLongInt(), s, false /* !anchorChildren */);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   orderChildrenByHighWordZero(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int64_t, LongInt, -1L, 0L)

   if (TR::Node* foldedNode = tryFoldAndWidened(s, node))
      {
      return foldedNode;
      }

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // look for demorgan's law.  if found can remove one bitwise complement operation
   if (firstChild->getReferenceCount() == 1)
      {
      if (isBitwiseLongComplement(firstChild)   &&
          secondChild->getReferenceCount() == 1 &&
          isBitwiseLongComplement(secondChild))
         {
         if (performTransformation(s->comp(), "%sReduced land with two complemented children in node [" POINTER_PRINTF_FORMAT "] to complemented lor\n", s->optDetailString(), node))
            {
            TR::Node *orNode    = TR::Node::create(TR::lor, 2, firstChild->getFirstChild(), secondChild->getFirstChild());
            TR::Node * constNode = firstChild->getSecondChild();
            TR::Node::recreate(node, TR::lxor);
            node->setAndIncChild(0, orNode);
            node->setAndIncChild(1, constNode);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node = s->simplify(node, block);
            }
         }
      // look for reassociation to fold constants
      else if (firstChildOp == TR::land)
         {
         TR::Node * lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::lconst)
            {
            if (secondChildOp == TR::lconst)
               {
               if (performTransformation(s->comp(), "%sFound land of lconst with land of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                  {
                  if (secondChild->getReferenceCount() == 1)
                     {
                     secondChild->setLongInt(secondChild->getLongInt() & lrChild->getLongInt());
                     }
                  else
                     {
                     TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
                     node->setAndIncChild(1, foldedConstChild);
                     foldedConstChild->setLongInt(secondChild->getLongInt() & lrChild->getLongInt());
                     secondChild->recursivelyDecReferenceCount();
                     }
                  node->setAndIncChild(0, firstChild->getFirstChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  }
               }
            else // move constants up the tree so they will tend to get merged together
               {
               if (performTransformation(s->comp(), "%sFound land of non-lconst with land of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                  {
                  node->setChild(1, lrChild);
                  firstChild->setChild(1, secondChild);
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  }
               }
            }
         }
      // look for opportunities to perform zero extending conversions
      else if (secondChild->getOpCodeValue() == TR::lconst)
         {
         int64_t      maskValue          = secondChild->getLongInt();
         TR::ILOpCodes firstChildOp       = firstChild->getOpCodeValue();
         bool         foundZeroExtension = false;
         if (maskValue == 255 && firstChildOp == TR::b2i)
            {
            if (performTransformation(s->comp(), "%sReduced land with lconst 255 in node [" POINTER_PRINTF_FORMAT "] to bu2l\n", s->optDetailString(), node))
               {
               TR::Node::recreate(node, TR::bu2l);
               foundZeroExtension = true;
               }
            }
         else if (maskValue == 65535 && firstChildOp == TR::s2l)
            {
            if (performTransformation(s->comp(), "%sReduced land with lconst 65536 in node [" POINTER_PRINTF_FORMAT "] to su2l\n", s->optDetailString(), node))
               {
               TR::Node::recreate(node, TR::su2l);
               foundZeroExtension = true;
               }
            }
         else if (maskValue == 0xffffffff && firstChildOp == TR::i2l)
            {
            if (performTransformation(s->comp(), "%sReduced land with lconst 0xffffffff in node [" POINTER_PRINTF_FORMAT "] to iu2l\n", s->optDetailString(), node))
               {
               TR::Node::recreate(node, TR::iu2l);
               foundZeroExtension = true;
               }
            }
         if (foundZeroExtension)
            {
            node->setNumChildren(1);
            node->setAndIncChild(0, firstChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            }
         }
      }

    if (node->getOpCodeValue() == TR::land && secondChild->getOpCodeValue() == TR::lconst && firstChild->isHighWordZero())
      {
      setIsHighWordZero(secondChild, s);

      if (secondChild->isHighWordZero() && ((int32_t)secondChild->getLongIntLow()) > 0)
         {
         TR::ILOpCodes firstChildOp = firstChild->getOpCodeValue();

         if (firstChildOp == TR::iu2l &&
            performTransformation(s->comp(), "%sReduced land with lconst and iu2l child in node [" POINTER_PRINTF_FORMAT "] to iand\n", s->optDetailString(), node))
            {
            TR::Node * constChild = NULL;

            if (secondChild->getReferenceCount()==1)
               {
               TR::Node::recreate(secondChild, TR::iconst);
               secondChild->setInt(secondChild->getLongIntLow());
               constChild = secondChild;
               }
            else
               {
               constChild = TR::Node::create(node, TR::iconst, 0);
               constChild->setInt(secondChild->getLongIntLow());
               }
            TR::Node * iandChild = TR::Node::create(TR::iand, 2, firstChild->getFirstChild(), constChild);
            TR::Node::recreate(node, firstChildOp);
            node->setNumChildren(1);
            node->setAndIncChild(0, iandChild);

            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            node->setIsHighWordZero(true);
            }
         }
      }


   if ((node->getOpCodeValue() == TR::land) && node->getSecondChild()->getOpCode().isLoadConst())
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      if ((firstChild->getOpCodeValue() == TR::lor) && firstChild->getSecondChild()->getOpCode().isLoadConst())
         {
         if ((firstChild->getSecondChild()->getLongInt() & secondChild->getLongInt()) == 0)
            {
            // ((A | B) & C) ==> A & C when ((B & C) == 0)
            node->setAndIncChild(0, firstChild->getFirstChild());
            firstChild->recursivelyDecReferenceCount();
            }
         }
      // can we simplify some boolean logic?
      else if ((firstChild->getOpCodeValue() == TR::iu2l) && firstChild->getFirstChild()->getOpCode().isBooleanCompare())
         {
         if ((secondChild->getLongInt() & 0x1) == 1)
            {
            // i2ul(A cmp B) & 0x1 ==> i2ul(A cmp B)
            TR::Node::recreate(node, firstChild->getOpCodeValue());
            node->setNumChildren(1);
            node->setAndIncChild(0, firstChild->getFirstChild());

            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            return node;
            }
         }
      }

   return node;
   }

TR::Node *bandSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, firstChild->getByte() & secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int8_t, Byte, -1, 0)
   return node;
   }

TR::Node *sandSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getShortInt() & secondChild->getShortInt(), s, false /* anchorChildren */);
      return node;
      }
   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OR_ZERO_OP(int16_t, ShortInt, -1, 0)

   if (TR::Node* foldedNode = tryFoldAndWidened(s, node))
      {
      return foldedNode;
      }

   return node;
   }

bool isChildOrConstRedundant(TR::Node *parentOrConstNode, TR::Node *childOrConstNode, TR::Simplifier * s)
   {
   TR_ASSERT(parentOrConstNode->getOpCode().isIntegralConst(),"parentOrConstNode %s (%p) not an integral const\n",parentOrConstNode->getOpCode().getName(),parentOrConstNode);
   TR_ASSERT(childOrConstNode->getOpCode().isIntegralConst(),"childOrConstNode %s (%p) not an integral const\n",childOrConstNode->getOpCode().getName(),childOrConstNode);

   uint64_t parentOrConst = parentOrConstNode->get64bitIntegralValueAsUnsigned();
   uint64_t childOrConst = childOrConstNode->get64bitIntegralValueAsUnsigned();

   // if the result of or'ing the two or constants is the same as the parent (int or) constant then can remove child oor operation
   uint64_t combinedOrConst = childOrConst | parentOrConst;
   if (combinedOrConst == parentOrConst)
      return true;
   else
      return false;
   }

// lor_a
//    lor_b
//       lx
//       lconst 0x0000F0
//    lconst 0xF0F0F0
//
// remove lor_b as it's constant 'or' value is redundant in this subtree given the lor_a constant value
// also works for ior pairs
// returns the unchanged or new firstChild
//
// after tree
//
// lor_a
//    lx
//    lconst 0xF0F0F0
//
TR::Node *removeRedundantIntegralOrPattern1(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node *parentOr = node;
   TR::Node *firstChild = parentOr->getFirstChild();
   if (parentOr->getOpCodeValue() != firstChild->getOpCodeValue())
      return firstChild;
   TR::Node *childOr = firstChild;

   if (!parentOr->getSecondChild()->getOpCode().isLoadConst())
      return firstChild;
   TR::Node *parentOrConstNode = parentOr->getSecondChild();

   if (!childOr->getSecondChild()->getOpCode().isLoadConst())
      return firstChild;
   TR::Node *childOrConstNode = childOr->getSecondChild();

   if (!isChildOrConstRedundant(parentOrConstNode, childOrConstNode, s))
      return firstChild;

   if (!performTransformation(s->comp(), "%sRemove redundant %s 0x%llx [" POINTER_PRINTF_FORMAT "] under %s 0x%llx [" POINTER_PRINTF_FORMAT "]\n",s->optDetailString(),
         childOr->getOpCode().getName(),childOrConstNode->get64bitIntegralValueAsUnsigned(),childOr,
         parentOr->getOpCode().getName(),parentOrConstNode->get64bitIntegralValueAsUnsigned(),parentOr))
      {
      return firstChild;
      }

   return s->replaceNode(childOr, childOr->getFirstChild(), s->_curTree);
   }

// ior
//    bu2i
//       bor
//          bx
//          bconst 0xF0
//    iconst 0xF0F0F0F0
//
// remove bor as it's constant 'or' value is redundant in this subtree given the ior constant value and zero extension
// returns the unchanged or new firstChild
//
// after tree
//
//  ior
//    bu2i (new node)
//       bx
//    iconst 0xF0F0F0F0
//
TR::Node *removeRedundantIntegralOrPattern2(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node *parentOr = node;
   TR::Node *firstChild = parentOr->getFirstChild();
   if (!(firstChild->getOpCodeValue() == TR::bu2i || firstChild->getOpCodeValue() == TR::su2i || firstChild->getOpCodeValue() == TR::iu2l))
      return firstChild;
   TR::Node *zeroExtConv = firstChild;

   if (!parentOr->getSecondChild()->getOpCode().isLoadConst())
      return firstChild;
   TR::Node *parentOrConstNode = parentOr->getSecondChild();

   if (!zeroExtConv->getFirstChild()->getOpCode().isOr())
      return firstChild;
   TR::Node *childOr = zeroExtConv->getFirstChild();

   if (!childOr->getSecondChild()->getOpCode().isLoadConst())
      return firstChild;
   TR::Node *childOrConstNode = childOr->getSecondChild();

   if (!isChildOrConstRedundant(parentOrConstNode, childOrConstNode, s))
      return firstChild;

   if (!performTransformation(s->comp(), "%sRemove redundant %s 0x%llx [" POINTER_PRINTF_FORMAT "] under %s 0x%llx [" POINTER_PRINTF_FORMAT "]\n",s->optDetailString(),
         childOr->getOpCode().getName(),childOrConstNode->get64bitIntegralValueAsUnsigned(),childOr,
         parentOr->getOpCode().getName(),parentOrConstNode->get64bitIntegralValueAsUnsigned(),parentOr))
      {
      return firstChild;
      }

   // create a newFirstChild so we're not limited to cases where zeroExtConv has a refCount=1
   TR::Node *newFirstChild = TR::Node::create(zeroExtConv->getOpCodeValue(), 1, childOr->getFirstChild());
   dumpOptDetails(s->comp(),"%sCreate new zero extension conversion %s [" POINTER_PRINTF_FORMAT "] of childOr child %s [" POINTER_PRINTF_FORMAT "]\n",
      s->optDetailString(),newFirstChild->getOpCode().getName(),newFirstChild,childOr->getFirstChild()->getOpCode().getName(),childOr->getFirstChild());
   return s->replaceNode(zeroExtConv, newFirstChild, s->_curTree);
   }

//---------------------------------------------------------------------
// Boolean OR
//

TR::Node *iorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if (node->getOpCode().isUnsigned())
         {
         uint32_t value = firstChild->getUnsignedInt()|secondChild->getUnsignedInt();
         foldUIntConstant(node, value, s, false /*!anchorChildren*/);
         if (node->nodeRequiresConditionCodes())
            setCCOr(value, node, s);
         }
      else
         {
         uint32_t value = firstChild->getInt()|secondChild->getInt();
         foldIntConstant(node, value, s, false /*!anchorChildren*/);
         if (node->nodeRequiresConditionCodes())
            setCCOr(value, node, s);
         }
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   if (node->nodeRequiresConditionCodes())
      {
      if (secondChild->getOpCode().isLoadConst() &&
          secondChild->getInt() != 0)
         s->setCC(node, OMR::ConditionCode1);
      return node;
      }

   BINARY_IDENTITY_OR_ZERO_OP(int32_t, Int, 0, -1)

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // look for demorgan's law.  if found can remove one bitwise complement operation
   if (firstChild->getReferenceCount() == 1)
      {
      if (isBitwiseIntComplement(firstChild)    &&
          secondChild->getReferenceCount() == 1 &&
          isBitwiseIntComplement(secondChild))
         {
         if (performTransformation(s->comp(), "%sReduced ior with two complemented children in node [" POINTER_PRINTF_FORMAT "] to complemented iand\n", s->optDetailString(), node))
            {
            TR::Node * andNode   = TR::Node::create(TR::iand, 2, firstChild->getFirstChild(), secondChild->getFirstChild());
            TR::Node * constNode = firstChild->getSecondChild();
            TR::Node::recreate(node, TR::ixor);
            node->setAndIncChild(0, andNode);
            node->setAndIncChild(1, constNode);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node = s->simplify(node, block);
            return node;
            }
         }
      //
      //  (a & ~bit1 ) | (bit1)  ==>  a | bit1
      //
      else if (firstChildOp == TR::iand)
         {
         TR::Node * lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::iconst)
            {
            if (secondChildOp == TR::iconst)
               {
               uint32_t and_const = secondChild->getInt();
               uint32_t or_const  = lrChild->getInt();

               if ((((~and_const) | or_const) == or_const) &&
                  performTransformation(s->comp(), "%sFound ior of iconst with iand of x and iconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                  {
                  node->setAndIncChild(0, firstChild->getFirstChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  node = s->simplify(node, block);
                  return node;
                  }
               }
            }
         }
      // look for reassociation to fold constants
      else if (firstChildOp == TR::ior)
         {
         TR::Node * lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::iconst)
            {
            if (secondChildOp == TR::iconst)
               {
               if (performTransformation(s->comp(), "%sFound ior of iconst with ior of x and iconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                  {
                  if (secondChild->getReferenceCount() == 1)
                     {
                     secondChild->setInt(secondChild->getInt() | lrChild->getInt());
                     }
                  else
                     {
                     TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::iconst, 0);
                     node->setAndIncChild(1, foldedConstChild);
                     foldedConstChild->setInt(secondChild->getInt() | lrChild->getInt());
                     secondChild->recursivelyDecReferenceCount();
                     }
                  node->setAndIncChild(0, firstChild->getFirstChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  node = s->simplify(node, block);
                  return node;
                  }
               }
            else // move constants up the tree so they will tend to get merged together
               {
               if (performTransformation(s->comp(), "%sFound ior of non-iconst with ior x and iconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                  {
                  node->setChild(1, lrChild);
                  firstChild->setChild(1, secondChild);
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  node = s->simplify(node, block);
                  return node;
                  }
               }
            }
         }
      }

   TR::Node *resultNode = s->simplifyiOrPatterns(node);
   if (resultNode != NULL)
      {
      return resultNode;
      }

   if(checkAndReplaceRotation<int32_t>(node,block,s))
      {
      return node;
      }

   firstChild = node->setChild(0, removeRedundantIntegralOrPattern1(node, block, s));
   firstChild = node->setChild(0, removeRedundantIntegralOrPattern2(node, block, s));

   return node;
   }

TR::Node *lorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      int64_t value = firstChild->getLongInt() | secondChild->getLongInt();
      foldLongIntConstant(node, value, s, false /* !anchorChildren */);
      if (node->nodeRequiresConditionCodes())
         setCCOr(value, node, s);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   orderChildrenByHighWordZero(node, firstChild, secondChild, s);

   if (node->nodeRequiresConditionCodes())
      {
      if (secondChild->getOpCode().isLoadConst() &&
          secondChild->getLongInt() != 0)
         s->setCC(node, OMR::ConditionCode1);
      return node;
      }

   BINARY_IDENTITY_OR_ZERO_OP(int64_t, LongInt, 0L, -1L)

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // look for demorgan's law.  if found can remove one bitwise complement operation
   if (firstChild->getReferenceCount() == 1)
      {
      if (isBitwiseLongComplement(firstChild)   &&
          secondChild->getReferenceCount() == 1 &&
          isBitwiseLongComplement(secondChild))
         {
         if (performTransformation(s->comp(), "%sReduced lor with two complemented children in node [" POINTER_PRINTF_FORMAT "] to complemented land\n", s->optDetailString(), node))
            {
            TR::Node * andNode   = TR::Node::create(TR::land, 2, firstChild->getFirstChild(), secondChild->getFirstChild());
            TR::Node * constNode = firstChild->getSecondChild();
            TR::Node::recreate(node, TR::lxor);
            node->setAndIncChild(0, andNode);
            node->setAndIncChild(1, constNode);
            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();
            node = s->simplify(node, block);
            node->setVisitCount(0);
            s->_alteredBlock = true;
            node = s->simplify(node, block);
            return node;
            }
         }
      // look for reassociation to fold constants
      else if (firstChildOp == TR::lor)
         {
         TR::Node * lrChild = firstChild->getSecondChild();
         if (lrChild->getOpCodeValue() == TR::lconst)
            {
            if (secondChildOp == TR::lconst)
               {
               if (performTransformation(s->comp(), "%sFound lor of lconst with lor of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                  {
                  if (secondChild->getReferenceCount() == 1)
                     {
                     secondChild->setLongInt(secondChild->getLongInt() | lrChild->getLongInt());
                     }
                  else
                     {
                     TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
                     node->setAndIncChild(1, foldedConstChild);
                     foldedConstChild->setLongInt(secondChild->getLongInt() | lrChild->getLongInt());
                     secondChild->recursivelyDecReferenceCount();
                     }
                  node->setAndIncChild(0, firstChild->getFirstChild());
                  firstChild->recursivelyDecReferenceCount();
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  node = s->simplify(node, block);
                  return node;
                  }
               }
            else // move constants up the tree so they will tend to get merged together
               {
               if (performTransformation(s->comp(), "%sFound lor of non-lconst with lor of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
                  {
                  node->setChild(1, lrChild);
                  firstChild->setChild(1, secondChild);
                  node->setVisitCount(0);
                  s->_alteredBlock = true;
                  node = s->simplify(node, block);
                  return node;
                  }
               }
            }
         }
      }

    if (node->getOpCodeValue() == TR::lor && secondChild->getOpCodeValue() == TR::lconst && firstChild->isHighWordZero())
      {
      setIsHighWordZero(secondChild, s);

      if (secondChild->isHighWordZero() && ((int32_t)secondChild->getLongIntLow()) > 0)
         {
         TR::ILOpCodes firstChildOp = firstChild->getOpCodeValue();

         if (firstChildOp == TR::iu2l &&
            performTransformation(s->comp(), "%sReduced lor with lconst and iu2l child in node [" POINTER_PRINTF_FORMAT "] to ior\n", s->optDetailString(), node))
            {
            TR::Node * constChild = NULL;

            if (secondChild->getReferenceCount()==1)
               {
               TR::Node::recreate(secondChild, TR::iconst);
               secondChild->setInt(secondChild->getLongIntLow());
               constChild = secondChild;
               }
            else
               {
               constChild = TR::Node::create(node, TR::iconst, 0);
               constChild->setInt(secondChild->getLongIntLow());
               }
            TR::Node * iorChild = TR::Node::create(TR::ior, 2, firstChild->getFirstChild(), constChild);
            TR::Node::recreate(node, firstChildOp);
            node->setNumChildren(1);
            node->setAndIncChild(0, iorChild);

            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            node->setIsHighWordZero(true);
            s->_alteredBlock = true;
            node = s->simplify(node, block);
            return node;
            }
         }

      }


   // For 32 bit platforms (p and z), evaluator due to historical reasons, evaluator for rotate does not support 64 bit regs on a 32 bit platform by default
   // Need to disable this transform until that support is added.
   if((TR::Compiler->target.is64Bit() || s->comp()->cg()->use64BitRegsOn32Bit()) && checkAndReplaceRotation<int64_t>(node,block,s))
      {
      return node;
      }

   firstChild = node->setChild(0, removeRedundantIntegralOrPattern1(node, block, s));
   firstChild = node->setChild(0, removeRedundantIntegralOrPattern2(node, block, s));

   return node;
   }

TR::Node *borSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      int8_t byte = firstChild->getByte() | secondChild->getByte();
      foldByteConstant(node, byte, s, false /* !anchorChildren*/);
      if (node->nodeRequiresConditionCodes())
         setCCOr(byte, node, s);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   if (node->nodeRequiresConditionCodes())
      {
      if (secondChild->getOpCode().isLoadConst() &&
          secondChild->getByte() != 0)
         s->setCC(node, OMR::ConditionCode1);
      return node;
      }

   // Check for a bit-copy pattern following the form ((X << K) & N) | (Y & N)
   // Sample tree:
   // bor
   //   band
   //      bshl
   //         ibload (1)
   //         iconst
   //      bconst (1)
   //   band
   //      ibload (2)
   //      bconst (2)
   //
   // Will get transformed to:
   // icall
   //    ibload (1)
   //    iconst
   //    bconst (1)
   //    ibload (2)

   //check for pattern like
   // bor                                  bor
   //   band                                 bload
   //     bload       ----------->           bconst -128
   //     bconst 127
   //  bconst -128

   if (firstChild->getOpCode().isAnd() &&
         firstChild->getReferenceCount() == 1 &&
         secondChild->getOpCode().isLoadConst() &&
         firstChild->getSecondChild()->getOpCode().isLoadConst() &&
         (((secondChild->getByte() | firstChild->getSecondChild()->getByte()) & 0xFF) == 0xFF) &&
         performTransformation(s->comp(), "%sReplacing bor [" POINTER_PRINTF_FORMAT "] child with band child [" POINTER_PRINTF_FORMAT "] \n",
                                  s->optDetailString(),node,firstChild->getFirstChild()))
      {

      node->setAndIncChild(0, firstChild->getFirstChild());
      firstChild->recursivelyDecReferenceCount();
      node->setVisitCount(0);
      s->_alteredBlock = true;
      }
   BINARY_IDENTITY_OR_ZERO_OP(int8_t, Byte, 0, -1)
   return node;
   }

TR::Node *sorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      uint16_t value = firstChild->getShortInt()|secondChild->getShortInt();
      foldShortIntConstant(node, value, s, false /* !anchorChildren */);
      if (node->nodeRequiresConditionCodes())
         setCCOr(value, node, s);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   if (node->nodeRequiresConditionCodes())
      {
      if (secondChild->getOpCode().isLoadConst() &&
          secondChild->getLongInt() != 0)
         s->setCC(node, OMR::ConditionCode1);
      return node;
      }

   BINARY_IDENTITY_OR_ZERO_OP(int16_t, ShortInt, 0, -1)

   return node;
   }

//---------------------------------------------------------------------
// Boolean XOR
//

TR::Node *ixorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();
   if (firstChild == secondChild)
      {
      if (firstChild->getOpCode().isUnsigned())
         foldUIntConstant(node, 0, s, true /* !anchorChildren*/);
      else
         foldIntConstant(node, 0, s, true /* !anchorChildren*/);
      if (node->nodeRequiresConditionCodes())
         setCCOr(0, node, s);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if (firstChild->getOpCode().isUnsigned())
         foldUIntConstant(node, firstChild->getInt()^secondChild->getInt(), s, false /* !anchorChildren*/);
     else
         foldIntConstant(node, firstChild->getInt()^secondChild->getInt(), s, false /* !anchorChildren*/);
      if (node->nodeRequiresConditionCodes())
         setCCOr(firstChild->getInt()^secondChild->getInt(),node, s);
      return node;
      }

   if (node->nodeRequiresConditionCodes())
      return node;

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OP(Int, 0)

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // look for reassociation to fold constants
   if (firstChildOp == TR::ixor &&
       firstChild->getReferenceCount() == 1)
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCodeValue() == TR::iconst)
         {
         if (secondChildOp == TR::iconst)
            {
            if (performTransformation(s->comp(), "%sFound ixor of iconst with ixor of x and iconst in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setInt(secondChild->getInt() ^ lrChild->getInt());
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::iconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setInt(secondChild->getInt() ^ lrChild->getInt());
                  secondChild->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               node = s->simplify(node, block);
               return node;
               }
            }
         else // move constants up the tree so they will tend to get merged together
            {
            if (performTransformation(s->comp(), "%sFound ixor of non-iconst with ixor x and iconst in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               node->setChild(1, lrChild);
               firstChild->setChild(1, secondChild);
               node->setVisitCount(0);
               s->_alteredBlock = true;
               node = s->simplify(node, block);
               return node;
               }
            }
         }
      }


   if(checkAndReplaceRotation<int32_t>(node,block,s))
      {
      return node;
      }

   return node;
   }

TR::Node *lxorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();
   if (firstChild == secondChild)
      {
      foldLongIntConstant(node, 0, s, true /* !anchorChildren */);
      if (node->nodeRequiresConditionCodes())
         setCCOr(0, node, s);
      return node;
      }

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getLongInt() ^ secondChild->getLongInt(), s, false /* !anchorChildren */);
      if (node->nodeRequiresConditionCodes())
         setCCOr(firstChild->getLongInt()^secondChild->getLongInt(),node, s);
      return node;
      }
   if (node->nodeRequiresConditionCodes())
      return node;
   orderChildren(node, firstChild, secondChild, s);
   orderChildrenByHighWordZero(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OP(LongInt, 0L)

   TR::ILOpCodes firstChildOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

   // look for reassociation to fold constants
   if (firstChildOp == TR::lxor &&
       firstChild->getReferenceCount() == 1)
      {
      TR::Node * lrChild = firstChild->getSecondChild();
      if (lrChild->getOpCodeValue() == TR::lconst)
         {
         if (secondChildOp == TR::lconst)
            {
            if (performTransformation(s->comp(), "%sFound lxor of lconst with lxor of x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               if (secondChild->getReferenceCount() == 1)
                  {
                  secondChild->setLongInt(secondChild->getLongInt() ^ lrChild->getLongInt());
                  }
               else
                  {
                  TR::Node * foldedConstChild = TR::Node::create(secondChild, TR::lconst, 0);
                  node->setAndIncChild(1, foldedConstChild);
                  foldedConstChild->setLongInt(secondChild->getLongInt() ^ lrChild->getLongInt());
                  secondChild->recursivelyDecReferenceCount();
                  }
               node->setAndIncChild(0, firstChild->getFirstChild());
               firstChild->recursivelyDecReferenceCount();
               node->setVisitCount(0);
               s->_alteredBlock = true;
               node = s->simplify(node, block);
               return node;
               }
            }
         else // move constants up the tree so they will tend to get merged together
            {
            if (performTransformation(s->comp(), "%sFound lxor of non-lconst with lxor x and lconst in node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               node->setChild(1, lrChild);
               firstChild->setChild(1, secondChild);
               node->setVisitCount(0);
               s->_alteredBlock = true;
               node = s->simplify(node, block);
               return node;
               }
            }
         }
      }

    if (node->getOpCodeValue() == TR::lxor && secondChild->getOpCodeValue() == TR::lconst && firstChild->isHighWordZero())
      {
      setIsHighWordZero(secondChild, s);

      if (secondChild->isHighWordZero() && ((int32_t)secondChild->getLongIntLow()) > 0)
         {
         TR::ILOpCodes firstChildOp = firstChild->getOpCodeValue();

         if (firstChildOp == TR::iu2l &&
            performTransformation(s->comp(), "%sReduced lxor with lconst and iu2l child in node [" POINTER_PRINTF_FORMAT "] to ixor\n", s->optDetailString(), node))
            {
            TR::Node * constChild = NULL;

            if (secondChild->getReferenceCount()==1)
               {
               TR::Node::recreate(secondChild, TR::iconst);
               secondChild->setInt(secondChild->getLongIntLow());
               constChild = secondChild;
               }
            else
               {
               constChild = TR::Node::create(node, TR::iconst, 0);
               constChild->setInt(secondChild->getLongIntLow());
               }
            TR::Node * ixorChild = TR::Node::create(TR::ixor, 2, firstChild->getFirstChild(), constChild);
            TR::Node::recreate(node, firstChildOp);
            node->setNumChildren(1);
            node->setAndIncChild(0, ixorChild);

            firstChild->recursivelyDecReferenceCount();
            secondChild->recursivelyDecReferenceCount();

            node->setIsHighWordZero(true);
            s->_alteredBlock = true;
            node = s->simplify(node, block);
            return node;
            }
         }

      }


   // For 32 bit platforms (p and z), evaluator due to historical reasons, evaluator for rotate does not support 64 bit regs on a 32 bit platform by default
   // Need to disable this transform until that support is added.
   if( (TR::Compiler->target.is64Bit() || s->comp()->cg()->use64BitRegsOn32Bit()) && checkAndReplaceRotation<int64_t>(node,block,s))
      {
      return node;
      }

   return node;
   }

TR::Node *bxorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, firstChild->getByte()^secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OP(Byte, 0)
   return node;
   }

TR::Node *sxorSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getShortInt()^secondChild->getShortInt(), s, false /* !anchorChildren */);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   BINARY_IDENTITY_OP(ShortInt, 0)

   return node;
   }

//---------------------------------------------------------------------
// Integer conversion
//

TR::Node *i2lSimplifier(TR::Node * node, TR::Block *  block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getInt(), s, false /* !anchorChildren */);
      return node;
      }

   TR::ILOpCodes childOp = firstChild->getOpCodeValue();
   if (firstChild->getReferenceCount() == 1)
      {
      bool foundFoldableConversion = false;
      if (childOp == TR::su2i)
         {
         if (performTransformation(s->comp(), "%sReduced i2l with su2i child in node [" POINTER_PRINTF_FORMAT "] to su2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::su2l);
            foundFoldableConversion = true;
            }
         }
      else if (childOp == TR::bu2i)
         {
         if (performTransformation(s->comp(), "%sReduced i2l with su2i child in node [" POINTER_PRINTF_FORMAT "] to su2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::bu2l);
            foundFoldableConversion = true;
            }
         }
      else if (childOp == TR::s2i)
         {
         if (performTransformation(s->comp(), "%sReduced i2l with s2i child in node [" POINTER_PRINTF_FORMAT "] to s2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::s2l);
            foundFoldableConversion = true;
            }
         }
      else if (childOp == TR::b2i)
         {
         if (performTransformation(s->comp(), "%sReduced i2l with b2i child in node [" POINTER_PRINTF_FORMAT "] to b2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::b2l);
            foundFoldableConversion = true;
            }
         }
      if (foundFoldableConversion)
         {
         node->setAndIncChild(0, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         return node;
         }
      }

   if (firstChild->getOpCodeValue() == TR::l2i &&
       firstChild->getFirstChild()->getOpCodeValue() == TR::lshr &&
       firstChild->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      TR::Node* firstFirst = firstChild->getFirstChild();
      uint32_t shiftBy = firstFirst->getSecondChild()->getInt();
      if (shiftBy >= 57 &&
          performTransformation(s->comp(), "%sRemove i2l/l2i from lshr node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::lshr);
         node->setNumChildren(2);
         node->setAndIncChild(0, firstFirst->getFirstChild());
         node->setAndIncChild(1, firstFirst->getSecondChild());
         firstChild->recursivelyDecReferenceCount();
         return node;
         }
      }

   return node;
   }

TR::Node *i2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::iconst)
      {
       uint32_t absValue = firstChild->getInt() >= 0 ? firstChild->getInt() : -firstChild->getInt();
       integerToFloatHelper(absValue,false,node,s);
      }
   return node;
   }

TR::Node *i2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, (double)firstChild->getInt(), s);
      }

   return node;
   }

TR::Node *i2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node,
             node->getOpCode().isUnsigned() ?
             (firstChild->getInt() & 0xff) :
             (firstChild->getInt()<<24)>>24,
                       s, false /* !anchorChildren*/ );
      return node;
      }

   TR::Node * result;
   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::b2i)))
      return result;

   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::bu2i)))
      return result;

   if ((result = foldDemotionConversion(node, TR::s2i, TR::s2b, s)))
      return result;

   if ((result = foldDemotionConversion(node, TR::l2i, TR::l2b, s)))
      return result;

   if ((result = foldRedundantAND(node, TR::iand, TR::iconst, 0xFF, s)))
      return result;

   return node;
   }

TR::Node *i2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, (int16_t)firstChild->getInt(), s, false /* !anchorChildren */);
      return node;
      }

   TR::Node * result;
   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::s2i)))
      return result;

   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::su2i)))
      return result;

   TR::Node *resultNode = s->simplifyi2sPatterns(node);
   if (resultNode != NULL)
      {
      return resultNode;
      }

   if ((result = foldRedundantAND(node, TR::iand, TR::iconst, 0xFFFF, s)))
      return result;

   return node;
   }

TR::Node *i2aSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      uint8_t addrPr = (TR::Compiler->target.is64Bit() ? 8 : 4);
      foldAddressConstant(node, firstChild->getInt(), s, false /* !anchorChildren */);
      return node;
      }
   else if (firstChild->getOpCode().isConversion())  // remove redundant conversions
      {
      while (firstChild->getOpCode().isConversion())
         firstChild = firstChild->getFirstChild();

      if (firstChild->getDataType() == TR::Address &&
          !firstChild->getOpCode().isLoadAddr() &&
          TR::Compiler->target.is32Bit())
         return s->replaceNode(node, firstChild, s->_curTree);
      else
         firstChild = node->getFirstChild(); // reset firstChild
      }

   //iu2a                        aiadd
   // isub        ===>             aload
   //   a2i                        iconst X
   //     aload
   //   iconst -X
   //
   if ((firstChild->getOpCodeValue() == TR::isub || firstChild->getOpCodeValue() == TR::iadd) &&
       (s->comp()->cg()->supports32bitAiadd() || node->isNonNegative()) &&      // todo: do we know NonNegativity in simplifier?
       firstChild->getFirstChild() &&
       firstChild->getFirstChild()->getOpCodeValue() == TR::a2i &&
       firstChild->getSecondChild() &&
       firstChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
       ((firstChild->getOpCodeValue() == TR::isub && firstChild->getSecondChild()->getInt() <= 0) ||  // todo: is positive offset only a correct soln?
        (firstChild->getOpCodeValue() == TR::iadd && firstChild->getSecondChild()->getInt() >= 0)
       ) &&
       performTransformation(s->comp(), "%sTransforming iu2a  [%s] to aiadd\n", s->optDetailString(), node->getName(s->getDebug()))
      )
      {
      TR::Node::recreate(node, TR::aiadd);
      node->setAndIncChild(0, firstChild->getFirstChild()->getFirstChild());
      node->setNumChildren(2);
      int32_t newVal = (firstChild->getOpCodeValue() == TR::isub) ? -firstChild->getSecondChild()->getInt() : firstChild->getSecondChild()->getInt();
      if (firstChild->getReferenceCount() == 1 && firstChild->getSecondChild()->getReferenceCount() == 1)
         {
         firstChild->getSecondChild()->setInt(newVal);
         node->setAndIncChild(1, firstChild->getSecondChild());
         }
      else
         {
         TR::Node * newSecondChild = TR::Node::create(firstChild->getSecondChild(), TR::iconst, 0);
         newSecondChild->setInt(newVal);
         node->setAndIncChild(1, newSecondChild);
         }

      s->prepareToStopUsingNode(firstChild, s->_curTree);
      firstChild->recursivelyDecReferenceCount();
      }

   return node;
   }

//---------------------------------------------------------------------
// Unsigned integer conversion
//

TR::Node *iu2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getUnsignedInt(), s, false /* !anchorChildren */);
      return node;
      }

   TR::ILOpCodes childOp = firstChild->getOpCodeValue();
   if (firstChild->getReferenceCount() == 1)
      {
      bool foundFoldableConversion = false;
      if (childOp == TR::su2i)
         {
         if (performTransformation(s->comp(), "%sReduced iu2l with su2i child in node [" POINTER_PRINTF_FORMAT "] to su2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::su2l);
            foundFoldableConversion = true;
            }
         }
      else if (childOp == TR::bu2i)
         {
         if (performTransformation(s->comp(), "%sReduced iu2l with bu2i child in node [" POINTER_PRINTF_FORMAT "] to bu2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::bu2l);
            foundFoldableConversion = true;
            }
         }
      if (foundFoldableConversion)
         {
         node->setAndIncChild(0, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   return node;
   }

TR::Node *iu2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::iconst)
      {
       uint32_t absValue = firstChild->getUnsignedInt();
       integerToFloatHelper(absValue,false,node,s);
      }
   return node;
   }

TR::Node *iu2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, (double)firstChild->getUnsignedInt(), s);
      }

   return node;
   }

//---------------------------------------------------------------------
// Long conversion
//

TR::Node *l2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return intDemoteSimplifier(node, block, s);
   }

TR::Node *l2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::lconst)
      {

      uint64_t absValue = firstChild->getLongInt() >= 0 ? firstChild->getLongInt() : -firstChild->getLongInt();

      longToFloatHelper(absValue, false, node, s);
      // determine if we have enough bits to precisely represent the constant.
      // long has 64 bits, float 24

      }
   return node;
   }

TR::Node *l2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::lconst)
      {

      uint64_t absValue = firstChild->getLongInt() >= 0 ? firstChild->getLongInt() : -firstChild->getLongInt();
      longToDoubleHelper(absValue,false, node, s);
      }
   return node;
   }

TR::Node *l2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return intDemoteSimplifier(node, block, s);
   }

TR::Node *l2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return intDemoteSimplifier(node, block, s);
   }

TR::Node *l2aSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

     if (firstChild->getOpCode().isLoadConst() &&
        ((int32_t)(firstChild->getLongIntHigh() & 0xffffffff) == (int32_t)0) &&
        ((int64_t)firstChild->getLongInt() >= (int64_t)0))
      {
      uint8_t addrPr = (TR::Compiler->target.is64Bit() ? 8 : 4);
      foldAddressConstant(node, firstChild->getLongInt() & 0xFFFFFFFF, s, false /* !anchorChildren */);
      return node;
      }
   else if (firstChild->getOpCode().isConversion())  // remove redundant conversions
      {
      while (firstChild->getOpCode().isConversion())
         firstChild = firstChild->getFirstChild();

      if (firstChild->getDataType() == TR::Address &&
          !firstChild->getOpCode().isLoadAddr() &&
          TR::Compiler->target.is64Bit())
         return s->replaceNode(node, firstChild, s->_curTree);
      else
         firstChild = node->getFirstChild(); // reset firstChild
      }

   // Fold below into address add
   //l2a/lu2a                    aladd/aiadd
   // lsub        ===>             aload
   //   a2l                        lconst/iconst X
   //     aload
   //   lconst -X
   // -----------------------
   if ((firstChild->getOpCodeValue() == TR::lsub || firstChild->getOpCodeValue() == TR::ladd) &&
       firstChild->getFirstChild() &&
       firstChild->getFirstChild()->getOpCode().isConversion() &&
       firstChild->getFirstChild()->getFirstChild() &&
       firstChild->getFirstChild()->getFirstChild()->getType().isAddress() &&
       firstChild->getSecondChild() &&
       firstChild->getSecondChild()->getOpCodeValue() == TR::lconst &&
       ((firstChild->getOpCodeValue() == TR::lsub && firstChild->getSecondChild()->getLongInt() < 0) || // todo: is positive offset only a correct soln?
        (firstChild->getOpCodeValue() == TR::ladd && firstChild->getSecondChild()->getLongInt() >= 0)
       )
      )
      {
      TR::Node *constChild = firstChild->getSecondChild();
      bool is64Bit = TR::Compiler->target.is64Bit();
      TR::ILOpCodes addressAddOp = ((constChild->getReferenceCount() == 1) && // is firstChild the only parent of constChild?
                                     (constChild->get64bitIntegralValue() <= (int64_t)CONSTANT64(0x000000000FFFFFFF) && // Does 64bit value fit in 32bit?
                                      constChild->get64bitIntegralValue() >= (int64_t)CONSTANT64(0xFFFFFFFFF0000000)) &&
                                      TR::Compiler->target.is32Bit()
                                    ) ? TR::aiadd : TR::aladd;

      if (!(addressAddOp == TR::aladd && !is64Bit) &&  // aladd only allowed in 64bit
          performTransformation(s->comp(), "%sTransforming %s [%s] to address add\n", s->optDetailString(), node->getOpCode().getName(), node->getName(s->getDebug()))
         )
         {
         TR::Node::recreate(node, addressAddOp);

         node->setNumChildren(2);  // upto 2 children do not require node extension. this should suffice. Any more requires Node::addChildren
         node->setAndIncChild(0, firstChild->getFirstChild()->getFirstChild());    // aload

         int64_t newVal = (firstChild->getOpCodeValue() == TR::lsub) ? -constChild->getLongInt() : constChild->getLongInt();
         constChild = TR::Node::create(constChild, (addressAddOp == TR::aladd) ? TR::lconst : TR::iconst, 0);
         if (addressAddOp == TR::aladd)
            constChild->setLongInt(newVal);
         else
            constChild->setInt((int32_t)newVal);
         node->setAndIncChild(1, constChild);

         s->prepareToStopUsingNode(firstChild, s->_curTree);
         firstChild->recursivelyDecReferenceCount();
         }
      }
   return node;
   }

//---------------------------------------------------------------------
// Unsigned Long conversion
//

TR::Node *lu2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::lconst)
      {

      uint64_t absValue = firstChild->getUnsignedLongInt();

      longToFloatHelper(absValue, true, node, s);
      // determine if we have enough bits to precisely represent the constant.
      // long has 64 bits, float 24

      }
   return node;
   }

TR::Node *lu2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::lconst)
      {

      uint64_t absValue = firstChild->getUnsignedLongInt();
      longToDoubleHelper(absValue,true, node, s);
      }
   return node;
   }

//---------------------------------------------------------------------
// Float conversion
//

TR::Node *f2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, floatToInt(firstChild->getFloat(), false), s, false /* !anchorChildren*/);
      return node;
      }

   return node;
   }

TR::Node *f2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, floatToLong(firstChild->getFloat(), false), s, false /* !anchorChildren */);
      return node;
      }

   return node;
   }

TR::Node *f2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, firstChild->getFloat(), s);
      return node;
      }

   return node;
   }

TR::Node *f2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, (floatToInt(firstChild->getFloat(), false)<<24)>>24, s, false /* !anchorChildren*/);
      return node;
      }

   return node;
   }

TR::Node *f2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, (floatToInt(firstChild->getFloat(), false)<<16)>>16, s, false /* !anchorChildren */);
      return node;
      }

   return node;
   }

//---------------------------------------------------------------------
// Double conversion
//

TR::Node *d2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      if (node->getOpCode().isUnsigned())
         {
         if (firstChild->getDouble() > 0.0)
            {
            foldUIntConstant(node, doubleToInt(firstChild->getDouble(), false), s, false /* !anchorChildren */);
            }
         }
      else
         {
         foldIntConstant(node, doubleToInt(firstChild->getDouble(), false), s, false /* !anchorChildren */);
         }
      return node;
      }

   return node;
   }

TR::Node *d2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, doubleToLong(firstChild->getDouble(), false), s, false /* !anchorChildren */);
      return node;
      }

   return node;
   }

TR::Node *d2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
         {
         foldFloatConstant(node, TR::Compiler->arith.doubleToFloat(firstChild->getDouble()), s);
         return node;
         }
      }

   TR::Node *resultNode = s->simplifyd2fPatterns(node);
   if (resultNode != NULL)
      {
      firstChild = resultNode;
      TR_ASSERT(firstChild->getOpCodeValue()== TR::fcall, "Unexpected opcode %d\n", firstChild->getOpCodeValue());
      }

   // because of the above, this and subsequent d2f's can become parents of fcalls, so clean it up when seen
   if (firstChild->getOpCode().isFloat())
      {
      s->replaceNode(node,firstChild, s->_curTree);
      return firstChild;
      }

   return node;
   }

TR::Node *d2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, (doubleToInt(firstChild->getDouble(), false)<<24)>>24, s, false /* !anchorChildren*/);
      return node;
      }

   return node;
   }

TR::Node *d2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, (doubleToInt(firstChild->getDouble(), false)<<16)>>16, s, false /* !anchorChildren */);
      return node;
      }

   return node;
   }

//---------------------------------------------------------------------
// Byte conversion
//

TR::Node *b2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getByte(), s, false /* !anchorChildren*/);
      }
   return node;
   }

TR::Node *b2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getByte(), s, false /* !anchorChildren */);
      }
   return node;
   }

TR::Node *b2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();
   if (firstChild->getOpCode().isLoadConst())
      {
      float f = (float)firstChild->getByte();
      foldFloatConstant(node, f, s);
      }
   return node;
   }

TR::Node *b2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      double g = (double)firstChild->getByte();
      foldDoubleConstant(node, g, s);
      }
   return node;
   }

TR::Node *b2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getByte(), s, false /* !anchorChildren */);
      }
   return node;
   }

//---------------------------------------------------------------------
// Unsigned byte conversion
//

TR::Node *bu2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getUnsignedByte(), s, false /* !anchorChildren*/);
      return node;
      }

   if (firstChild->getOpCodeValue() == TR::i2b &&
       firstChild->getFirstChild()->getOpCodeValue() == TR::iand &&
       firstChild->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      uint64_t val = firstChild->getFirstChild()->getSecondChild()->get64bitIntegralValueAsUnsigned();
      if (((val & 0xFFUL) == 0UL) &&
          performTransformation(s->comp(), "%sReplacing bu2i [" POINTER_PRINTF_FORMAT "] with i2b child [" POINTER_PRINTF_FORMAT "] of iand [" POINTER_PRINTF_FORMAT "] with mask 0, with iconst 0\n",
                                  s->optDetailString(),node,firstChild,firstChild->getFirstChild(),val,TR::getMaxUnsigned<TR::Int8>()))
         {
         // if we're going to zero extend a zero, then we can just replace this entire expression with a 0
         s->anchorNode(firstChild->getFirstChild()->getFirstChild(), s->_curTree); //only really need to anchor this child, everything else is guaranteed to not be a load
         s->prepareToReplaceNode(node, TR::iconst);
         node->setInt(0);
         return node;
         }
      else if (val <= (uint64_t)TR::getMaxUnsigned<TR::Int8>() &&
          performTransformation(s->comp(), "%sRemove bu2i [" POINTER_PRINTF_FORMAT "] with i2b child [" POINTER_PRINTF_FORMAT "] of iand [" POINTER_PRINTF_FORMAT "] with mask %d <= %d\n",
                                  s->optDetailString(),node,firstChild,firstChild->getFirstChild(),val,TR::getMaxUnsigned<TR::Int8>()))
         {
         // the bu2i zero extension is not needed when the grandchild iand will zero out the top 3 bytes.
         // simplify:
         // bu2i
         //    i2b
         //       iand
         //          x
         //          iconst 0-255
         // to:
         // iand
         //    x
         //    iconst 0-255
         TR::Node * grandChild = firstChild->getFirstChild();
         grandChild->incReferenceCount();
         bool anchorChildren = false; // no loads are being removed (just bu2i/i2b) so there is nothing that needs anchoring
         s->prepareToStopUsingNode(node, s->_curTree, anchorChildren);
         s->prepareToStopUsingNode(firstChild, s->_curTree, anchorChildren);
         node->recursivelyDecReferenceCount();
         return grandChild;
         }
      }
   else if (firstChild->getOpCodeValue() == TR::i2b &&
            (firstChild->getFirstChild()->getOpCodeValue() == TR::butest ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::arraycmp ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::trt ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::trtSimple ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::icmpeq ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::lcmpeq ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::icmpne ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::lcmpne) &&
            performTransformation(s->comp(), "%sRemove bu2i [" POINTER_PRINTF_FORMAT "] with i2b child [" POINTER_PRINTF_FORMAT "] with %s grandchild [" POINTER_PRINTF_FORMAT "]\n",
                                  s->optDetailString(), node, firstChild, firstChild->getFirstChild()->getOpCode().getName(), firstChild->getFirstChild()))
      {
      TR::Node * grandChild = firstChild->getFirstChild();
      grandChild->incReferenceCount();
      bool anchorChildren = false; // no loads are being removed (just bu2i/i2b) so there is nothing that needs anchoring
      s->prepareToStopUsingNode(node, s->_curTree, anchorChildren);
      s->prepareToStopUsingNode(firstChild, s->_curTree, anchorChildren);
      node->recursivelyDecReferenceCount();
      return grandChild;
      }

   if (firstChild->getOpCodeValue() == TR::l2b &&
       firstChild->getFirstChild()->getOpCodeValue() == TR::lushr &&
       firstChild->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      uint32_t shiftBy = firstChild->getFirstChild()->getSecondChild()->getInt();
      if (shiftBy >= 56 &&
          performTransformation(s->comp(), "%sReplace bu2i/l2b of lushr with l2i node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         node->recreate(node, TR::l2i);
         node->setAndIncChild(0, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         return node;
         }
      }

   return node;
   }

TR::Node *bu2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getUnsignedByte(), s, false /* !anchorChildren */);
      return node;
      }

   if (firstChild->getOpCodeValue() == TR::i2b &&
       firstChild->getFirstChild()->getOpCodeValue() == TR::iand &&
       firstChild->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      uint64_t val = firstChild->getFirstChild()->getSecondChild()->get64bitIntegralValueAsUnsigned();
      if (((val & 0xFFUL) == 0UL) &&
          performTransformation(s->comp(), "%sReplacing bu2l [" POINTER_PRINTF_FORMAT "] with i2b child [" POINTER_PRINTF_FORMAT "] of iand [" POINTER_PRINTF_FORMAT "] with mask 0, with iconst 0\n",
                                  s->optDetailString(),node,firstChild,firstChild->getFirstChild(),val,TR::getMaxUnsigned<TR::Int8>()))
         {
         // if we're going to zero extend a zero, then we can just replace this entire expression with a 0
         s->anchorNode(firstChild->getFirstChild()->getFirstChild(), s->_curTree); //only really need to anchor this child, everything else is guaranteed to not be a load
         s->prepareToReplaceNode(node, TR::lconst);
         node->setLongInt(0);
         return node;
         }
      else if (val <= (uint64_t)TR::getMaxUnsigned<TR::Int8>() &&
          performTransformation(s->comp(), "%sReplace bu2l [" POINTER_PRINTF_FORMAT "] with i2b child [" POINTER_PRINTF_FORMAT "] of iand [" POINTER_PRINTF_FORMAT "] with mask %d <= %d with i2l\n",
                                  s->optDetailString(),node,firstChild,firstChild->getFirstChild(),val,TR::getMaxUnsigned<TR::Int8>()))
         {
         // the bu2l zero extension is not needed when the grandchild iand will zero out the top 3 bytes.
         // simplify:
         // bu2l
         //    i2b
         //       iand
         //          x
         //          iconst 0-255
         // to:
         // i2l
         //    iand
         //       x
         //       iconst 0-255
         TR::Node * grandChild = firstChild->getFirstChild();
         node->recreate(node, TR::i2l);
         grandChild->incReferenceCount();
         firstChild->recursivelyDecReferenceCount();
         node->setChild(0, grandChild);
         }
      }
   else if (firstChild->getOpCodeValue() == TR::i2b &&
            (firstChild->getFirstChild()->getOpCodeValue() == TR::icmpeq ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::icmpne ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::lcmpeq ||
             firstChild->getFirstChild()->getOpCodeValue() == TR::lcmpne) &&
            performTransformation(s->comp(), "%sRemove bu2l [" POINTER_PRINTF_FORMAT "] with i2b child [" POINTER_PRINTF_FORMAT "] of compare [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node, firstChild, firstChild->getFirstChild()))
      {
      TR::Node * grandChild = firstChild->getFirstChild();
      node->recreate(node, TR::i2l);
      grandChild->incReferenceCount();
      firstChild->recursivelyDecReferenceCount();
      node->setChild(0, grandChild);
      }
   else if (firstChild->getOpCodeValue() == TR::l2b &&
       firstChild->getFirstChild()->getOpCodeValue() == TR::lushr &&
       firstChild->getFirstChild()->getSecondChild()->getOpCode().isLoadConst())
      {
      uint32_t shiftBy = firstChild->getFirstChild()->getSecondChild()->getInt();
      if (shiftBy >= 56 &&
          performTransformation(s->comp(), "%sReplace bu2l/l2b of lushr with lushr node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         node->recreate(node, TR::lushr);
         node->setNumChildren(2);
         node->setAndIncChild(0, firstChild->getFirstChild()->getFirstChild());
         node->setAndIncChild(1, firstChild->getFirstChild()->getSecondChild());
         firstChild->recursivelyDecReferenceCount();
         }
      }

   return node;
   }

TR::Node *bu2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();
   if (firstChild->getOpCode().isLoadConst())
      {
      float f = (float)firstChild->getUnsignedByte();
      foldFloatConstant(node, f, s);
      }

   return node;
   }

TR::Node *bu2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();
   if (firstChild->getOpCode().isLoadConst())
      {
      double g = (double)firstChild->getUnsignedByte();
      foldDoubleConstant(node, g, s);
      }
   return node;
   }

TR::Node *bu2sSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldShortIntConstant(node, firstChild->getUnsignedByte(), s, false /* !anchorChildren */);
      }
   return node;
   }

//---------------------------------------------------------------------
// Short conversion
//

TR::Node *s2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   TR::ILOpCodes childOp = firstChild->getOpCodeValue();
   if (firstChild->getReferenceCount() == 1)
      {
      bool foundFoldableConversion = false;
      if (childOp == TR::bu2s)
         {
         if (performTransformation(s->comp(), "%sReduced s2i with bu2s child in node [" POINTER_PRINTF_FORMAT "] to bu2i\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::bu2i);
            foundFoldableConversion = true;
            }
         }
      else if (childOp == TR::b2s)
         {
         if (performTransformation(s->comp(), "%sReduced s2i with b2s child in node [" POINTER_PRINTF_FORMAT "] to b2i\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::b2i);
            foundFoldableConversion = true;
            }
         }
      if (foundFoldableConversion)
         {
         node->setAndIncChild(0, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   return node;
   }

TR::Node *s2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getShortInt(), s, false /* !anchorChildren */);
      return node;
      }

   TR::ILOpCodes childOp = firstChild->getOpCodeValue();
   if (firstChild->getReferenceCount() == 1)
      {
      bool foundFoldableConversion = false;
      if (childOp == TR::bu2s)
         {
         if (performTransformation(s->comp(), "%sReduced s2l with bu2s child in node [" POINTER_PRINTF_FORMAT "] to bu2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::bu2l);
            foundFoldableConversion = true;
            }
         }
      else if (childOp == TR::b2s)
         {
         if (performTransformation(s->comp(), "%sReduced s2l with b2s child in node [" POINTER_PRINTF_FORMAT "] to b2l\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, TR::b2l);
            foundFoldableConversion = true;
            }
         }
      if (foundFoldableConversion)
         {
         node->setAndIncChild(0, firstChild->getFirstChild());
         firstChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   return node;
   }

TR::Node *s2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldFloatConstant(node, (float)firstChild->getShortInt(), s);
      }
   return node;
   }

TR::Node *s2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, (double)firstChild->getShortInt(), s);
      }
   return node;
   }

TR::Node *s2bSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldByteConstant(node, (int8_t)firstChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   TR::Node * result;
   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::b2s)))
      return result;

   if ((result = s->unaryCancelOutWithChild(node, firstChild, s->_curTree, TR::bu2s)))
      return result;

   if ((result = foldRedundantAND(node, TR::sand, TR::sconst, 0xFF, s)))
      return result;

   return node;
   }

//---------------------------------------------------------------------
// Unsigned short conversion
//

TR::Node *su2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * firstGrandChild;

   if (firstChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getUnsignedShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   TR::ILOpCodes childOp = firstChild->getOpCodeValue();
   if (firstChild->getReferenceCount() == 1)
      {
      bool foundFoldableConversion = false;   // Haven't implemented any of these cases yet
                                              // but declaring the variable for completeness
                                              // See usage in s2lSimplifier for example.
      bool foundDoubleFoldableConversion = false;
      if (childOp == TR::i2s)
         {
         firstGrandChild = firstChild->getFirstChild();
         if (firstGrandChild->getReferenceCount() == 1 && firstGrandChild->getOpCodeValue() == node->getOpCodeValue())
            {
            if (performTransformation(s->comp(), "%sReduced su2i node [" POINTER_PRINTF_FORMAT "] and i2s child [" POINTER_PRINTF_FORMAT "] to no-op\n", s->optDetailString(), node,firstChild))
               {
               foundDoubleFoldableConversion = true;
               }

            }
         }

      TR_ASSERT(!(foundFoldableConversion && foundDoubleFoldableConversion), "Should only attempt one simplification per pass in su2iSimplifier\n");

      if (foundDoubleFoldableConversion)
         {
         node->setAndIncChild(0, firstGrandChild->getFirstChild());
         firstGrandChild->recursivelyDecReferenceCount();
         node->setVisitCount(0);
         s->_alteredBlock = true;
         }
      }
   return node;
   }

TR::Node *su2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldLongIntConstant(node, firstChild->getUnsignedShortInt(), s, false /* !anchorChildren */);
      return node;
      }

   return node;
   }

TR::Node *su2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node *firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldFloatConstant(node, (float)firstChild->getConst<uint16_t>(), s);
      }
   return node;
   }

TR::Node *su2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node *firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldDoubleConstant(node, (double)firstChild->getConst<uint16_t>(), s);
      return node;
      }

   return node;
   }


/**
 * \brief
 *
 * Remove add/subtract operations under comparison nodes to save an
 * addition or subtraction instruction during codegen. This simplification
 * applies to:
 *
 * 1. Signed comparisons of byte, short, int, and long values.
 * 2. Unsigned (in)equality of byte, short, int, and long values.
 *
 * \details
 *
 * \verbatim
 * As an example, assume the following is the original expression
 *        x + oldConst1 > oldConst2
 *
 * This expression can be simplified to
 *        x > newConst
 *        where newConst = oldConst2 - oldConst1
 * The newConst calculation should not produce overflow or underflow.
 * \endverbatim
 *
 * Also note that for unsigned comparison types other than equality and inequality,
 * add/sub simplifications are not possible due to how values are interpreted
 * in the current IL nodes. Essentially, the value of an iconst node is neither
 * signed nor unsigned; and its parent is free to interpret its value.
 *
 * For example, the following transformation is incorrect:
 *
 * \varbatim
 *
 * Before:
 *
 * ifiucmplt --> goto ...
 *     isub (cannotOverflow)
 *         iload x
 *         iconst -1
 *     iconst 100
 *
 * After:
 *
 * ifiucmplt --> goto ...
 *     iload x
 *     iconst 99
 *
 * \endverbatim
 *
 * If x were -1, the first tree would evaluate to true since it's comparing if zero is less than 100.
 * But in the second tree, ifiucmplt interprets the value of x as UINT_MAX. Thus,
 * the second tree is comparing UINT_MAX with 99; and the whole comparison node evalutes
 * to false.
 *
 * Unsigned equality and inequality comparisons are not sensitive to sign interpretation problems.
 *
 * \param node          the comparison node being simpilfied
 * \param s             the simplifier object
*/
TR::Node* removeArithmeticsUnderIntegralCompare(TR::Node* node,
                                                TR::Simplifier * s)
   {
   if (s->comp()->getOption(TR_DisableIntegerCompareSimplification) ||
         !(node->getOpCode().isBooleanCompare()   // The node must be an integer comparison node.
            && node->getNumChildren() > 0
            && node->getFirstChild()->getOpCode().isInteger()))
      return node;

   TR::Node* opNode      = node->getFirstChild();
   TR::Node* secondChild = node->getSecondChild();
   TR::ILOpCodes op      = opNode->getOpCodeValue();

   // Get recognized operation types. Unsigned IL OpCodes are deprecated and not supported here.
   bool isAddOp = (op == TR::iadd || op == TR::ladd || op == TR::sadd || op == TR::badd);
   bool isSubOp = (op == TR::isub || op == TR::lsub || op == TR::ssub || op == TR::bsub);

   bool isUnsignedCompare = node->getOpCode().isUnsignedCompare();
   bool isEqOrNotEqCompare = node->getOpCode().isCompareForEquality();

   bool isSignednessTypeSupported = isEqOrNotEqCompare || (!isUnsignedCompare && opNode->cannotOverflow());

   if ((isAddOp || isSubOp)
         && isSignednessTypeSupported
         && opNode->getSecondChild()->getOpCode().isLoadConst()
         && secondChild->getOpCode().isLoadConst()
         // Only perform the simplification on the first occurance of the add/sub node
         && (opNode->getFutureUseCount() == opNode->getReferenceCount() - 1))
      {
      int64_t signedMax = 0, signedMin = 0;
      uint64_t oldUConst1 = 0, oldUConst2 = 0, newUConst = 0;

      if (opNode->getOpCode().is1Byte())
         {
         oldUConst1 = opNode->getSecondChild()->getUnsignedByte();
         oldUConst2 = secondChild->getUnsignedByte();

         signedMax = TR::getMaxSigned<TR::Int8>();
         signedMin = TR::getMinSigned<TR::Int8>();
         }
      else if (opNode->getOpCode().is2Byte())
         {
         oldUConst1 = opNode->getSecondChild()->getUnsignedShortInt();
         oldUConst2 = secondChild->getUnsignedShortInt();

         signedMax = TR::getMaxSigned<TR::Int16>();
         signedMin = TR::getMinSigned<TR::Int16>();
         }
      else if (opNode->getOpCode().is4Byte())
         {
         oldUConst1 = opNode->getSecondChild()->getUnsignedInt();
         oldUConst2 = secondChild->getUnsignedInt();

         signedMax = TR::getMaxSigned<TR::Int32>();
         signedMin = TR::getMinSigned<TR::Int32>();
         }
      else if (opNode->getOpCode().is8Byte())
         {
         oldUConst1 = opNode->getSecondChild()->getUnsignedLongInt();
         oldUConst2 = secondChild->getUnsignedLongInt();

         signedMax = TR::getMaxSigned<TR::Int64>();
         signedMin = TR::getMinSigned<TR::Int64>();
         }
      else
         {
         if (s->trace())
            traceMsg(s->comp(),
                     "\nEliminating add/sub under compare node n%dn failed due to opcode data type\n",
                     node->getGlobalIndex());

         return node;
         }

      if (!isEqOrNotEqCompare)
         {
         // check for signed overflow
         // Signed integer overflow is undefined behavior in C++11; and unsigned
         // arithmnetic operations are defined. Need to make sure that signed integer results don't overflow.
         int64_t oldConst1 = static_cast<int64_t>(oldUConst1);
         int64_t oldConst2 = static_cast<int64_t>(oldUConst2);

         bool canTransformAdd = isAddOp
                 && !(oldConst1 > 0 && (oldConst2 < signedMin + oldConst1))
                 && !(oldConst1 < 0 && (oldConst2 > signedMax + oldConst1));

         bool canTransformSub = isSubOp
                     && !(oldConst1 > 0 && (oldConst2 > signedMax - oldConst1))
                     && !(oldConst1 < 0 && (oldConst2 < signedMin - oldConst1));

         if (!(canTransformAdd || canTransformSub))
            {
            if (s->trace())
               traceMsg(s->comp(),
                        "\nEliminating add/sub under order compare node n%dn failed due to overflow\n",
                        node->getGlobalIndex());

            return node;
            }
         }

      // Calculate new constant value.
      // For both signed and unsigned comparisons, the new values are calculated using
      // the old unsigned values. The reason being that signed integer overflow is undefined behavior in
      // C++, and that unsigned integer overflow is defined behavior: they shall obey the laws of
      // arithmetic modulo 2^n and produce wrapped values
      // The IL expects integer wrapping in case of overflow; and this matches unsigned integer behaviors in C++.
      newUConst = isAddOp ? (oldUConst2 - oldUConst1) : (oldUConst2 + oldUConst1);

      // Done checking constant values. Transform the tree.
      if (performTransformation(s->comp(),
                                "%sEliminating add/sub operation under integer comparison node n%dn %s\n",
                                s->optDetailString(),
                                node->getGlobalIndex(),
                                node->getOpCode().getName()))
         {
         // It's not recommended to mutate the value of an existing constant node by
         // calling constNode->setConstValue()
         // Hence, creating a new node here and copy the old node's internal info.
         TR::Node* newConstNode = TR::Node::create(secondChild, secondChild->getOpCodeValue(), 0);
         newConstNode->setUnsignedLongInt(newUConst);
         node->setAndIncChild(0, opNode->getFirstChild());
         node->setAndIncChild(1, newConstNode);

         secondChild->decReferenceCount();
         opNode->recursivelyDecReferenceCount();
         }
      }

   return node;
   }

//---------------------------------------------------------------------
// Integer if compare equal (signed and unsigned)
//

TR::Node *ificmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }

   makeConstantTheRightChild(node, firstChild, secondChild, s);

   if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((firstChild->getInt()==secondChild->getInt()), node, firstChild, secondChild, block, s))
      return node;

   if (conditionalZeroComparisonBranchFold (node, firstChild, secondChild, block, s))
      return node;

   simplifyIntBranchArithmetic(node, firstChild, secondChild, s);

   //We will change a if ( a >> C == 0 ) to a if ( a < 2^C )
   if ( firstChild->getOpCode().isRightShift() && //First child is a right shift

        firstChild->getSecondChild()->getOpCode().isLoadConst() &&
        firstChild->getSecondChild()->getInt() <= 31 &&
        firstChild->getSecondChild()->getInt() >= 0 && //Shift value is a positive constant of reasonable size

        ( firstChild->getOpCodeValue() == TR::iushr ||
          firstChild->getFirstChild()->isNonNegative()
        ) && //Either a logical shift or a positive first child to guaruntee zero-extend

        secondChild->getOpCode().isLoadConst() &&
        secondChild->getInt() == 0 && //Second child is a const 0
        performTransformation(s->comp(), "%sTransform (a >> C == 0) to (a < 2^C)\n", s->optDetailString())
      )
      {
      //Change if type
      TR::Node::recreate(node, TR::ifiucmplt);

      TR::Node *newSecondChild = TR::Node::create(node, TR::iconst, 0, 1 << firstChild->getSecondChild()->getInt());
      node->setAndIncChild(1, newSecondChild);

      node->setAndIncChild(0, firstChild->getFirstChild());

      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();

      return node;
      }

   bitwiseToLogical(node, block, s);

   if (firstChild->getOpCode().isBooleanCompare() &&
       (secondChild->getOpCode().isLoadConst()) &&
       ((secondChild->getInt() == 0) || (secondChild->getInt() == 1)) &&
       (firstChild->getOpCode().convertCmpToIfCmp() != TR::BadILOp) &&
       (s->comp()->cg()->getSupportsJavaFloatSemantics() || !(firstChild->getNumChildren()>1 && firstChild->getFirstChild()->getOpCode().isFloatingPoint())) &&
       performTransformation(s->comp(), "%sChanging if opcode %p because first child %p is a comparison opcode\n", s->optDetailString(), node, firstChild))
      {
      TR::Node::recreate(node, firstChild->getOpCode().convertCmpToIfCmp());
      node->setAndIncChild(0, firstChild->getFirstChild());
      node->setAndIncChild(1, firstChild->getSecondChild());
      if (secondChild->getInt() == 0)
         TR::Node::recreate(node, node->getOpCode().getOpCodeForReverseBranch());
      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();
      return node;
      }

   // if we're transforming ificmpeq with an lcmp child
   if ((firstChild->getOpCodeValue() == TR::lcmp) &&
         ((secondChild->getOpCode().isLoadConst()) &&
            secondChild->getInt() == 0) &&
         performTransformation(s->comp(), "%sChanging if opcode %p because first child %p is an lcmp\n", s->optDetailString(), node, firstChild))
      {
      TR::Node::recreate(node, TR::iflcmpeq); //change to iflcmp since operands are longs
      node->setAndIncChild(0, firstChild->getFirstChild());
      node->setAndIncChild(1, firstChild->getSecondChild());
      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();
      return node;
      }

   // if we're transforming ificmpeq with an lcmpeq child
   if ((firstChild->getOpCodeValue() == TR::lcmpeq) &&
         ((secondChild->getOpCode().isLoadConst()) &&
            secondChild->getInt() == 0) &&
         performTransformation(s->comp(), "%sChanging if opcode %p because first child %p is an lcmpeq\n", s->optDetailString(), node, firstChild))
      {
      TR::Node::recreate(node, TR::iflcmpne); //change to iflcmpne since operands are longs
      node->setAndIncChild(0, firstChild->getFirstChild());
      node->setAndIncChild(1, firstChild->getSecondChild());
      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();
      return node;
      }

   if (node->getOpCodeValue() == TR::ificmpeq)
      intCompareNarrower(node, s, TR::ifsucmpeq, TR::ifscmpeq, TR::ifbcmpeq);
   else
      unsignedIntCompareNarrower(node, s, TR::ifscmpeq, TR::ifbcmpeq);


   addressCompareConversion(node, s);
   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   if (s->getLastRun())
      convertToTestUnderMask(node, block, s);

   return node;
   }


//---------------------------------------------------------------------
// Integer if compare not equal (signed and unsigned)
//

TR::Node *ificmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }

   makeConstantTheRightChild(node, firstChild, secondChild, s);

   if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((firstChild->getInt()!=secondChild->getInt()), node, firstChild, secondChild, block, s))
      return node;

   if (conditionalZeroComparisonBranchFold (node, firstChild, secondChild, block, s))
      return node;

   simplifyIntBranchArithmetic(node, firstChild, secondChild, s);

   //We will change a if ( a >> C != 0 ) to a if ( a >= 2^C )
   if ( firstChild->getOpCode().isRightShift() && //First child is a right shift

        firstChild->getSecondChild()->getOpCode().isLoadConst() &&
        firstChild->getSecondChild()->getInt() <= 31 &&
        firstChild->getSecondChild()->getInt() >= 0 && //Shift value is a positive constant of reasonable size

        ( firstChild->getOpCodeValue() == TR::iushr ||
          firstChild->getFirstChild()->isNonNegative()
        ) && //Either a logical shift or a positive first child to guarantee zero-extend

        secondChild->getOpCode().isLoadConst() &&
        secondChild->getInt() == 0 //Second child is a const 0
      )
      {
      //Change if type
      TR::Node::recreate(node, TR::ifiucmpge);

      TR::Node *newSecondChild = TR::Node::create(node, TR::iconst, 0, 1 << firstChild->getSecondChild()->getInt());
      node->setAndIncChild(1, newSecondChild);

      node->setAndIncChild(0, firstChild->getFirstChild());

      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();

      return node;
      }

   bitwiseToLogical(node, block, s);

   if (firstChild->getOpCode().isBooleanCompare() &&
       (secondChild->getOpCode().isLoadConst()) &&
       ((secondChild->getInt() == 0) || (secondChild->getInt() == 1)) &&
       (firstChild->getOpCode().convertCmpToIfCmp() != TR::BadILOp) &&
       (s->comp()->cg()->getSupportsJavaFloatSemantics() || !(firstChild->getNumChildren()>1 && firstChild->getFirstChild()->getOpCode().isFloatingPoint())) &&
       performTransformation(s->comp(), "%sChanging if opcode %p because first child %p is a comparison opcode\n", s->optDetailString(), node, firstChild))
      {
      TR::Node::recreate(node, firstChild->getOpCode().convertCmpToIfCmp());
      node->setAndIncChild(0, firstChild->getFirstChild());
      node->setAndIncChild(1, firstChild->getSecondChild());
      if (secondChild->getInt() == 1)
         TR::Node::recreate(node, node->getOpCode().getOpCodeForReverseBranch());
      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();
      return node;
      }

   if ((firstChild->getOpCodeValue() == TR::lcmp) &&
         ((secondChild->getOpCode().isLoadConst()) &&
            secondChild->getInt() == 0) &&
         performTransformation(s->comp(), "%sChanging if opcode %p because first child %p is an lcmp\n", s->optDetailString(), node, firstChild))
      {
      TR::Node::recreate(node, TR::iflcmpne); //change to iflcmp since operands are longs
      node->setAndIncChild(0, firstChild->getFirstChild());
      node->setAndIncChild(1, firstChild->getSecondChild());
      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();
      return node;
      }

   if (node->getOpCodeValue() == TR::ificmpne)
      intCompareNarrower(node, s, TR::ifsucmpne, TR::ifscmpne, TR::ifbcmpne);
   else
      unsignedIntCompareNarrower(node, s, TR::ifscmpne, TR::ifbcmpne);


   addressCompareConversion(node, s);
   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);

   return node;
   }

//---------------------------------------------------------------------
// Integer if compare less than (signed and unsigned)
//

TR::Node *ificmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   if (node->getOpCodeValue() == TR::ificmplt)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirst->getInt() < originalSecond->getInt()), node, firstChild, secondChild, block, s))
         return node;
      intCompareNarrower(node, s, TR::ifsucmplt, TR::ifscmplt, TR::ifbcmplt);
      }
   else if (node->getOpCodeValue() == TR::ifiucmplt)
      {
      uint32_t originalFirstVal = originalFirst->getUnsignedInt();
      uint32_t originalSecondVal = originalSecond->getUnsignedInt();
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirstVal < originalSecondVal),
                                 node, firstChild, secondChild, block, s))
         return node;
      unsignedIntCompareNarrower(node, s, TR::ifsucmplt, TR::ifbucmplt);
      }


   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Integer if compare less than or equal (signed and unsigned)
//

TR::Node *ificmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   if (node->getOpCodeValue() == TR::ificmple)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirst->getInt() <= originalSecond->getInt()), node, firstChild, secondChild, block, s))
         return node;
      intCompareNarrower(node, s, TR::ifsucmple, TR::ifscmple, TR::ifbcmple);
      }
   else if (node->getOpCodeValue() == TR::ifiucmple)
      {
      uint32_t originalFirstVal = originalFirst->getUnsignedInt();
      uint32_t originalSecondVal = originalSecond->getUnsignedInt();
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirstVal <= originalSecondVal),
                                 node, firstChild, secondChild, block, s))
         return node;
      unsignedIntCompareNarrower(node, s, TR::ifsucmple, TR::ifbucmple);
      }

   removeArithmeticsUnderIntegralCompare(node, s);

   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Integer if compare greater than (signed and unsigned)
//

TR::Node *ificmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   if (node->getOpCodeValue() == TR::ificmpgt)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirst->getInt() > originalSecond->getInt()), node, firstChild, secondChild, block, s))
         return node;
      intCompareNarrower(node, s, TR::ifsucmpgt, TR::ifscmpgt, TR::ifbcmpgt);
      }
   else if (node->getOpCodeValue() == TR::ifiucmpgt)
      {
      uint32_t originalFirstVal = originalFirst->getUnsignedInt();
      uint32_t originalSecondVal = originalSecond->getUnsignedInt();
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirstVal > originalSecondVal),
                                 node, firstChild, secondChild, block, s))
         return node;
      unsignedIntCompareNarrower(node, s, TR::ifsucmpgt, TR::ifbucmpgt);
      }

   removeArithmeticsUnderIntegralCompare(node, s);

   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Integer if compare greater than or equal (signed and unsigned)
//

TR::Node *ificmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   if (node->getOpCodeValue() == TR::ificmpge)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirst->getInt() >= originalSecond->getInt()), node, firstChild, secondChild, block, s))
         return node;
      intCompareNarrower(node, s, TR::ifsucmpge, TR::ifscmpge, TR::ifbcmpge);
      }
   else if (node->getOpCodeValue() == TR::ifiucmpge)
      {
      uint32_t originalFirstVal = originalFirst->getUnsignedInt();
      uint32_t originalSecondVal = originalSecond->getUnsignedInt();
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirstVal >= originalSecondVal),
                                 node, firstChild, secondChild, block, s))
         return node;
      unsignedIntCompareNarrower(node, s, TR::ifsucmpge, TR::ifbucmpge);
      }

   IfxcmpgeToIfxcmpeqReducer ifxcmpReducer(s, node);
   if (ifxcmpReducer.isReducible())
      node = ifxcmpReducer.reduce();

   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Long integer if compare equal (signed and unsigned)
//

TR::Node *iflcmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }


   makeConstantTheRightChild(node, firstChild, secondChild, s);

   if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((firstChild->getLongInt()==secondChild->getLongInt()), node, firstChild, secondChild, block, s))
      return node;

   if (conditionalZeroComparisonBranchFold (node, firstChild, secondChild, block, s))
      return node;

   simplifyLongBranchArithmetic(node, firstChild, secondChild, s);

   if (node->getOpCodeValue() == TR::iflcmpeq)
      {
      longCompareNarrower(node, s, TR::ificmpeq, TR::ifsucmpeq, TR::ifscmpeq, TR::ifbcmpeq);
      }

   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Long integer if compare not equal (signed and unsigned)
//

TR::Node *iflcmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }

   makeConstantTheRightChild(node, firstChild, secondChild, s);

   if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((firstChild->getLongInt()!=secondChild->getLongInt()), node, firstChild, secondChild, block, s))
      return node;

   if (conditionalZeroComparisonBranchFold (node, firstChild, secondChild, block, s))
      return node;

   simplifyLongBranchArithmetic(node, firstChild, secondChild, s);

   if (node->getOpCodeValue() == TR::iflcmpne)
      {
      longCompareNarrower(node, s, TR::ificmpne, TR::ifsucmpne, TR::ifscmpne, TR::ifbcmpne);
      }
   addressCompareConversion(node, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Long integer if compare less than (signed and unsigned)
//

TR::Node *iflcmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   if (node->getOpCodeValue() == TR::iflcmplt)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((firstChild->getLongInt()<secondChild->getLongInt()), node, firstChild, secondChild, block, s))
         return node;
      longCompareNarrower(node, s, TR::ificmplt, TR::ifsucmplt, TR::ifscmplt, TR::ifbcmplt);
      }
   else if(node->getOpCodeValue() == TR::iflucmplt)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((((uint64_t)originalFirst->getLongInt())<((uint64_t)originalSecond->getLongInt())), node, firstChild, secondChild, block, s))
         return node;
      }
   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Long integer if compare less than or equal (signed and unsigned)
//

TR::Node *iflcmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   if (node->getOpCodeValue() == TR::iflcmple)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirst->getLongInt() <= originalSecond->getLongInt()), node, firstChild, secondChild, block, s))
         return node;
      longCompareNarrower(node, s, TR::ificmple, TR::ifsucmple, TR::ifscmple, TR::ifbcmple);
      }
   else if(node->getOpCodeValue() == TR::iflucmple)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((((uint64_t)originalFirst->getLongInt())<=((uint64_t)originalSecond->getLongInt())), node, firstChild, secondChild, block, s))
         return node;
      }
   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Long integer if compare greater than (signed and unsigned)
//

TR::Node *iflcmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   if (node->getOpCodeValue() == TR::iflcmpgt)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirst->getLongInt() > originalSecond->getLongInt()), node, firstChild, secondChild, block, s))
         return node;
      longCompareNarrower(node, s, TR::ificmpgt, TR::ifsucmpgt, TR::ifscmpgt, TR::ifbcmpgt);
      }
   else if(node->getOpCodeValue() == TR::iflucmpgt)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((((uint64_t)originalFirst->getLongInt())>((uint64_t)originalSecond->getLongInt())), node, firstChild, secondChild, block, s))
         return node;
      }
   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Long integer if compare greater than or equal (signed and unsigned)
//

TR::Node *iflcmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }
   TR::Node *originalFirst = firstChild;
   TR::Node *originalSecond = secondChild;

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   if (node->getOpCodeValue() == TR::iflcmpge)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((originalFirst->getLongInt() >= originalSecond->getLongInt()), node, firstChild, secondChild, block, s))
         return node;
      longCompareNarrower(node, s, TR::ificmpge, TR::ifsucmpge, TR::ifscmpge, TR::ifbcmpge);
      }
   else if(node->getOpCodeValue() == TR::iflucmpge)
      {
      if (firstChild->getOpCode().isLoadConst() && conditionalBranchFold((((uint64_t)originalFirst->getLongInt())>=((uint64_t)originalSecond->getLongInt())), node, firstChild, secondChild, block, s))
         return node;
      }

   IfxcmpgeToIfxcmpeqReducer ifxcmpReducer(s, node);
   if (ifxcmpReducer.isReducible())
      node = ifxcmpReducer.reduce();

   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// All compares that are to be normalized
//

TR::Node *normalizeCmpSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (node->getOpCode().isBranch() && (removeIfToFollowingBlock(node, block, s) == NULL))
      return NULL;

   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   if (node->getOpCode().isBranch())
      {
      TR::ILOpCodes op = TR::BadILOp;

      if (firstChild->getOpCode().isDouble()
          && secondChild->getOpCodeValue() == TR::dconst)
         {
         double   dValue = secondChild->getDouble();
         uint64_t diValue = secondChild->getUnsignedLongInt();
         float    fValue;
         int32_t  iValue;
         int64_t  lValue;
         int16_t  sValue;
         uint16_t cValue;
         int8_t   bValue;
         if (firstChild->getOpCodeValue() == TR::f2d &&
             doubleConstIsRepresentableExactlyAsFloat(dValue, fValue) &&
             performTransformation(s->comp(), "%sDemoted double compare of TR::f2d to dconst to float compare at node [%p]\n", s->optDetailString(), node))
            {
            op = doubleToFloatOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::create(node, TR::fconst, 0);
               newSecondChild->setFloat(fValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (firstChild->getOpCodeValue() == TR::i2d &&
                  doubleConstIsRepresentableExactlyAsInt(dValue, iValue) &&
                  performTransformation(s->comp(), "%sDemoted double compare of TR::i2d to dconst to int compare at node [%p]\n", s->optDetailString(), node))
            {
            op = doubleToIntegerOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::create(node, TR::iconst, 0);
               newSecondChild->setInt(iValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (longValueInDoubleRange(node, dValue) && firstChild->getOpCodeValue() == TR::l2d &&
                  doubleConstIsRepresentableExactlyAsLong(dValue, lValue) &&
                  performTransformation(s->comp(), "%sDemoted double compare of TR::l2d to dconst to long compare at node [%p]\n", s->optDetailString(), node))
            {
            op = doubleToLongOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::create(node, TR::lconst, 0);
               newSecondChild->setLongInt(lValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (firstChild->getOpCodeValue() == TR::s2d &&
                  doubleConstIsRepresentableExactlyAsShort(dValue, sValue) &&
                  performTransformation(s->comp(), "%sDemoted double compare of TR::s2d to dconst to short compare at node [%p]\n", s->optDetailString(), node))
            {
            op = doubleToShortOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::sconst(node, sValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (firstChild->getOpCodeValue() == TR::su2d &&
                  doubleConstIsRepresentableExactlyAsChar(dValue, cValue) &&
                  performTransformation(s->comp(), "%sDemoted double compare of TR::su2d to dconst to char compare at node [%p]\n", s->optDetailString(), node))
            {
            op = doubleToCharOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::cconst(node, cValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (firstChild->getOpCodeValue() == TR::b2d &&
                  doubleConstIsRepresentableExactlyAsByte(dValue, bValue) &&
                  performTransformation(s->comp(), "%sDemoted double compare of TR::b2d to dconst to byte compare at node [%p]\n", s->optDetailString(), node))
            {
            op = doubleToByteOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::bconst(node, bValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         }
      else if (firstChild->getOpCode().isFloat()
               && secondChild->getOpCodeValue() == TR::fconst)
         {
         float    fValue = secondChild->getFloat();
         uint32_t fiValue = secondChild->getFloatBits();
         int32_t  iValue;
         int64_t  lValue;
         int16_t  sValue;
         uint16_t cValue;
         int8_t   bValue;

         if (intValueInFloatRange(node, fValue) && firstChild->getOpCodeValue() == TR::i2f &&
             floatConstIsRepresentableExactlyAsInt(fValue, iValue) &&
             performTransformation(s->comp(), "%sDemoted float compare of TR::i2f to fconst to int compare at node [%p]\n", s->optDetailString(), node))
            {
            op = floatToIntegerOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::create(node, TR::iconst, 0);
               newSecondChild->setInt(iValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (longValueInFloatRange(node, fValue) && firstChild->getOpCodeValue() == TR::l2f &&
                  floatConstIsRepresentableExactlyAsLong(fValue, lValue) &&
                  performTransformation(s->comp(), "%sDemoted float compare of TR::l2f to fconst to long compare at node [%p]\n", s->optDetailString(), node))
            {
            op = floatToLongOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::create(node, TR::lconst, 0);
               newSecondChild->setLongInt(lValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (firstChild->getOpCodeValue() == TR::s2f &&
                  floatConstIsRepresentableExactlyAsShort(fValue, sValue) &&
                  performTransformation(s->comp(), "%sDemoted float compare of TR::s2f to fconst to short compare at node [%p]\n", s->optDetailString(), node))
            {
            op = floatToShortOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::sconst(node, sValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (firstChild->getOpCodeValue() == TR::su2f &&
                  floatConstIsRepresentableExactlyAsChar(fValue, cValue) &&
                  performTransformation(s->comp(), "%sDemoted float compare of TR::su2f to fconst to char compare at node [%p]\n", s->optDetailString(), node))
            {
            op = floatToCharOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::cconst(node, cValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         else if (firstChild->getOpCodeValue() == TR::b2f &&
                  floatConstIsRepresentableExactlyAsByte(fValue, bValue) &&
                  performTransformation(s->comp(), "%sDemoted float compare of TR::b2f to fconst to byte compare at node [%p]\n", s->optDetailString(), node))
            {
            op = floatToByteOp(node->getOpCodeValue());
            if (op != TR::BadILOp)
               {
               TR::Node::recreate(node, op);
               TR::Node * newSecondChild = TR::Node::bconst(node, bValue);
               node->setAndIncChild(0, firstChild->getFirstChild());
               node->setAndIncChild(1, newSecondChild);
               firstChild->recursivelyDecReferenceCount();
               secondChild->recursivelyDecReferenceCount();
               }
            }
         }
      }

   return node;
   }

//---------------------------------------------------------------------
// Address if compare equal
//

TR::Node *ifacmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }

   makeConstantTheRightChild(node, firstChild, secondChild, s);
   if (firstChild->getOpCodeValue() == TR::aconst && conditionalBranchFold((firstChild->getAddress()==secondChild->getAddress()), node, firstChild, secondChild, block, s))
      return node;

   // weak symbols aren't necessarily defined, so we have to do the test
   if (!(firstChild->getOpCode().hasSymbolReference() && firstChild->getSymbol()->isWeakSymbol()) &&
       conditionalZeroComparisonBranchFold (node, firstChild, secondChild, block, s))
      return node;

   partialRedundantCompareElimination(node, block, s);

   ifjlClassSimplifier(node, s);

   return node;
   }

//---------------------------------------------------------------------
// Address if compare not equal
//
TR::Node *ifacmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }

   makeConstantTheRightChild(node, firstChild, secondChild, s);
   if (firstChild->getOpCodeValue() == TR::aconst && conditionalBranchFold((firstChild->getAddress()!=secondChild->getAddress()), node, firstChild, secondChild, block, s))
      return node;

   // weak symbols aren't necessarily defined, so we have to do the test
   if (!(firstChild->getOpCode().hasSymbolReference() && firstChild->getSymbol()->isWeakSymbol()) &&
       conditionalZeroComparisonBranchFold (node, firstChild, secondChild, block, s))
      return node;

   partialRedundantCompareElimination(node, block, s);

   ifjlClassSimplifier(node, s);

   return node;
   }

//---------------------------------------------------------------------
// If compares that include equality
//

TR::Node *ifCmpWithEqualitySimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, true);
      return node;
      }
   else if (branchToFollowingBlock(node, block, s->comp()))
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   // Reduce ifcmpge to ifcmpeq in specific cases
   TR::ILOpCode op = node->getOpCode();
   IfxcmpgeToIfxcmpeqReducer ifxcmpReducer(s, node);
   if (op.isCompareForOrder() && op.isCompareTrueIfGreater() && ifxcmpReducer.isReducible())
      node = ifxcmpReducer.reduce();

   if (firstChild->getOpCode().isLoadConst() &&
       secondChild->getOpCode().isLoadConst())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      bool foldBranch = false;
      bool takeBranch;
      switch (opCode)
         {
         case TR::ifbcmpeq:
         case TR::ifbucmpeq:
         case TR::ifscmpeq:
         case TR::ifsucmpeq:
            takeBranch = firstChild->get64bitIntegralValue() == secondChild->get64bitIntegralValue();
            foldBranch = true;
            break;

         case TR::ifbcmple:
         case TR::ifscmple:
            takeBranch = firstChild->get64bitIntegralValue() <= secondChild->get64bitIntegralValue();
            foldBranch = true;
            break;
         case TR::ifbucmple:
         case TR::ifsucmple:
            takeBranch = firstChild->get64bitIntegralValueAsUnsigned() <= secondChild->get64bitIntegralValueAsUnsigned();
            foldBranch = true;
            break;

         case TR::ifbcmpge:
         case TR::ifscmpge:
            takeBranch = firstChild->get64bitIntegralValue() >= secondChild->get64bitIntegralValue();
            foldBranch = true;
            break;
         case TR::ifbucmpge:
         case TR::ifsucmpge:
            takeBranch = firstChild->get64bitIntegralValueAsUnsigned() >= secondChild->get64bitIntegralValueAsUnsigned();
            foldBranch = true;
            break;

         default:
            TR_ASSERT(0,"unexpected opcode %d\n",node->getOpCodeValue());
            foldBranch = false;
         }

      if (foldBranch &&
          conditionalBranchFold(takeBranch, node, firstChild, secondChild, block, s))
         {
         return node;
         }
      }

   static const char * disableFoldIfSet = feGetEnv("TR_DisableFoldIfSet");
   if (!disableFoldIfSet && node->getOpCodeValue() == TR::ifbcmpeq)
      {
      if (secondChild->getOpCodeValue() == TR::bconst  &&
          secondChild->getByte() == 0                 &&
          firstChild->getOpCode().isBooleanCompare() == true &&
          firstChild->getOpCode().isBranch() == false &&
          firstChild->getReferenceCount() == 1)
         {
         TR::ILOpCodes op= firstChild->getOpCode().convertCmpToIfCmp();
         if (op != TR::BadILOp &&
             performTransformation(s->comp(), "%sFolding ifbcmpeq of bconst 0 to boolean compare at node [" POINTER_PRINTF_FORMAT "] to equivalent if?cmp??\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, op);
            TR::Node::recreate(node, node->getOpCode().getOpCodeForReverseBranch());
            secondChild->recursivelyDecReferenceCount();
            node->setAndIncChild(0, firstChild->getFirstChild());
            node->setAndIncChild(1, firstChild->getSecondChild());
            firstChild->recursivelyDecReferenceCount();
            }
         }
      }
   bitTestingOp(node, s);
   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);

   return node;
   }

//---------------------------------------------------------------------
// If compares that exclude equality
//

TR::Node *ifCmpWithoutEqualitySimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }
   else if (branchToFollowingBlock(node, block, s->comp()))
      {
      s->conditionalToUnconditional(node, block, false);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      bool foldBranch = false;
      bool takeBranch;
      switch (opCode)
         {
         case TR::ifbcmpne:
         case TR::ifbucmpne:
         case TR::ifscmpne:
         case TR::ifsucmpne:
            takeBranch = firstChild->get64bitIntegralValue() != secondChild->get64bitIntegralValue();
            foldBranch = true;
            break;

         case TR::ifbcmplt:
         case TR::ifscmplt:
            takeBranch = firstChild->get64bitIntegralValue() < secondChild->get64bitIntegralValue();
            foldBranch = true;
            break;
         case TR::ifbucmplt:
         case TR::ifsucmplt:
            takeBranch = firstChild->get64bitIntegralValueAsUnsigned() < secondChild->get64bitIntegralValueAsUnsigned();
            foldBranch = true;
            break;

         case TR::ifbcmpgt:
         case TR::ifscmpgt:
            takeBranch = firstChild->get64bitIntegralValue() > secondChild->get64bitIntegralValue();
            foldBranch = true;
            break;
         case TR::ifbucmpgt:
         case TR::ifsucmpgt:
            takeBranch = firstChild->get64bitIntegralValueAsUnsigned() > secondChild->get64bitIntegralValueAsUnsigned();
            foldBranch = true;
            break;

         default:
            TR_ASSERT(0,"unexpected opcode %d\n",node->getOpCodeValue());
            foldBranch = false;
         }
      if (foldBranch &&
          conditionalBranchFold(takeBranch, node, firstChild, secondChild, block, s))
         {
         return node;
         }
      }

   static const char * disableFoldIfSet = feGetEnv("TR_DisableFoldIfSet");
   if (!disableFoldIfSet && node->getOpCodeValue() == TR::ifbcmpne)
      {
      if (secondChild->getOpCodeValue() == TR::bconst  &&
          secondChild->getByte() == 0                 &&
          firstChild->getOpCode().isBooleanCompare() == true &&
          firstChild->getOpCode().isBranch() == false &&
          firstChild->getReferenceCount() == 1)
         {
         TR::ILOpCodes op= firstChild->getOpCode().convertCmpToIfCmp();
         if (op != TR::BadILOp &&
             performTransformation(s->comp(), "%sFolding ifbcmpeq of bconst 0 to boolean compare at node [" POINTER_PRINTF_FORMAT "] to equivalent if?cmp??\n", s->optDetailString(), node))
            {
            TR::Node::recreate(node, op);
            secondChild->recursivelyDecReferenceCount();
            node->setAndIncChild(0, firstChild->getFirstChild());
            node->setAndIncChild(1, firstChild->getSecondChild());
            firstChild->recursivelyDecReferenceCount();
            }
         }
      }
   bitTestingOp(node, s);
   removeArithmeticsUnderIntegralCompare(node, s);
   partialRedundantCompareElimination(node, block, s);

   return node;
   }

//---------------------------------------------------------------------
// Integer compares
//

TR::Node *icmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren*/);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()==secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);


   if ((firstChild->getOpCode().isBooleanCompare() && firstChild->getOpCode().isInteger()) &&
       ((secondChild->getOpCode().isLoadConst()) &&
        secondChild->getInt() == 0) &&
       performTransformation(s->comp(), "%sChanging icmpeq opcode %p because first child %p is an int compare\n", s->optDetailString(), node, firstChild))
      {
      TR::Node::recreate(node, firstChild->getOpCode().getOpCodeForReverseBranch());
      node->setAndIncChild(0, firstChild->getFirstChild());
      node->setAndIncChild(1, firstChild->getSecondChild());
      firstChild->recursivelyDecReferenceCount();
      secondChild->recursivelyDecReferenceCount();
      return node;
      }

   //If we have a pattern as follows:
   //
   //icmpeq           <-node
   //   Xcmp          <-firstChild
   //      a          <-some child of type X
   //      b          <-other child of type X
   //   iconst c      <-some constant value
   //
   //We will change this to the pattern:
   //
   //Xcmp[l|eq|gt]
   //   a
   //   b
   //
   //Depending on the value of the iconst.

   if ( node->getOpCode().isSignum() && secondChild->getOpCode().isLoadConst() )
      {
      TR::ILOpCodes newOpCode = TR::BadILOp;

      if ( secondChild->getInt() == -1 )
         {
         //Change to an Xcmpl
         switch ( firstChild->getOpCodeValue() )
            {
            case TR::lcmp:
               newOpCode = TR::lcmplt; break;
            case TR::icmp:
               newOpCode = TR::icmplt; break;
            case TR::scmp:
               newOpCode = TR::scmplt; break;
            case TR::bcmp:
               newOpCode = TR::bcmplt; break;
            default:
               break;
            }
         }
      else if ( secondChild->getInt() == 0 )
         {
         //Change to a Xcmpeq
         switch ( firstChild->getOpCodeValue() )
            {
            case TR::lcmp:
               newOpCode = TR::lcmpeq; break;
            case TR::icmp:
               newOpCode = TR::icmpeq; break;
            case TR::scmp:
               newOpCode = TR::scmpeq; break;
            case TR::bcmp:
               newOpCode = TR::bcmpeq; break;
            default:
               break;
            }
         }
      else if ( secondChild->getInt() == 1 )
         {
         //Change to a Xcmpgt
         switch ( firstChild->getOpCodeValue() )
            {
            case TR::lcmp:
               newOpCode = TR::lcmpgt; break;
            case TR::icmp:
               newOpCode = TR::icmpgt; break;
            case TR::scmp:
               newOpCode = TR::scmpgt; break;
            case TR::bcmp:
               newOpCode = TR::bcmpgt; break;
            default:
               break;
            }
         }
      else
         {
         //Change to a false
         }

      if ( newOpCode != TR::BadILOp &&
           performTransformation(s->comp(), "%sChanging icmpeq opcode %p because first child %p is an %s opcode\n", s->optDetailString(), node, firstChild, firstChild->getOpCode().getName())
         )
         {
         TR::Node::recreate(node, newOpCode);
         node->setAndIncChild(0, firstChild->getFirstChild());
         node->setAndIncChild(1, firstChild->getSecondChild());
         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         }
      }

   if (firstChild->getOpCodeValue() == TR::iand &&
       secondChild->getOpCode().isLoadConst() &&
       firstChild->getSecondChild()->getOpCode().isLoadConst())
      {
      uint32_t val = secondChild->getInt();
      uint32_t mask = firstChild->getSecondChild()->getInt();
      if ((val & (val - 1)) == 0 && val == mask &&
          performTransformation(s->comp(), "%s Changing icmpeq (x&2**c) to 2**c node [" POINTER_PRINTF_FORMAT "] to iand\n", s->optDetailString(), node))
         {
         uint32_t shiftBy = trailingZeroes(mask);
         TR::Node* byNode = TR::Node::create(node, TR::iconst, 0);
         byNode->setInt(shiftBy);
         TR::Node* shiftNode = TR::Node::create(TR::iushr, 2);
         shiftNode->setAndIncChild(0, firstChild->getFirstChild());
         shiftNode->setAndIncChild(1, byNode);
         TR::Node::recreate(node, TR::iand);
         TR::Node* one = TR::Node::create(node, TR::iconst, 0);
         one->setInt(1);
         node->setAndIncChild(0, shiftNode);
         node->setAndIncChild(1, one);
         firstChild->recursivelyDecReferenceCount();
         secondChild->decReferenceCount();
         }
      }

   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *icmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchor children */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()!=secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *icmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()<secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *icmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()<=secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *icmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()>secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *icmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()>=secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

//---------------------------------------------------------------------
// Long compares
//

TR::Node *lcmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getLongInt()==secondChild->getLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   if (firstChild->getOpCodeValue() == TR::land &&
       firstChild->getSecondChild()->getOpCodeValue() == TR::lconst &&
       secondChild->getOpCodeValue() == TR::lconst)
      {
      int64_t val1 = firstChild->getSecondChild()->getLongInt() & 0xffffffff80000000ull;
      int64_t val2 = secondChild->getLongInt() & 0xffffffff80000000ull;
      if (val1 == 0 && val2 == 0 && performTransformation(s->comp(), "%sChanging lcmpeq %p to icmpeq because there are no upper bits\n", s->optDetailString(), node))
         {
         node->recreate(node, TR::icmpeq);
         TR::Node * newSecondChild = TR::Node::create(node, TR::iconst, 0);
         newSecondChild->setInt(secondChild->getLongInt());
         TR::Node * newFirstChild = TR::Node::create(node, TR::l2i, 1);
         newFirstChild->setChild(0, firstChild);
         node->setAndIncChild(0, newFirstChild);
         node->setAndIncChild(1, newSecondChild);
         secondChild->decReferenceCount();
         return node;
         }
      }

   if (firstChild->getOpCodeValue() == TR::land &&
       secondChild->getOpCode().isLoadConst() &&
       firstChild->getSecondChild()->getOpCode().isLoadConst())
      {
      uint64_t val = secondChild->getLongInt();
      uint64_t mask = firstChild->getSecondChild()->getLongInt();
      if ((val & (val - 1)) == 0 && val == mask &&
          performTransformation(s->comp(), "%s Changing lcmpeq (x&2**c) to 2**c node [" POINTER_PRINTF_FORMAT "] to land\n", s->optDetailString(), node))
         {
         uint32_t shiftBy = trailingZeroes(mask);
         TR::Node* byNode = TR::Node::create(node, TR::iconst, 0);
         byNode->setInt(shiftBy);
         TR::Node* shiftNode = TR::Node::create(TR::lushr, 2);
         shiftNode->setAndIncChild(0, firstChild->getFirstChild());
         shiftNode->setAndIncChild(1, byNode);
         TR::Node* landNode = TR::Node::create(TR::land, 2);
         TR::Node* one = TR::Node::create(node, TR::lconst, 0);
         one->setLongInt(1);
         landNode->setAndIncChild(0, shiftNode);
         landNode->setAndIncChild(1, one);
         TR::Node::recreate(node, TR::l2i);
         node->setAndIncChild(0, landNode);
         node->setNumChildren(1);
         firstChild->recursivelyDecReferenceCount();
         secondChild->decReferenceCount();
         return node;
         }
      }

   orderChildren(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *lcmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getLongInt()!=secondChild->getLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   if ((firstChild->getOpCodeValue() == TR::iu2l) && firstChild->getFirstChild()->getOpCode().isBooleanCompare())
      {
      if (secondChild->getOpCode().isLoadConst() && (secondChild->getLongInt() == 0))
         {
         // i2l(A cmp B) != 0 ==> A cmp B
         TR::Node::recreate(node, firstChild->getFirstChild()->getOpCodeValue());
         node->setNumChildren(2);
         node->setAndIncChild(0, firstChild->getFirstChild()->getFirstChild());
         node->setAndIncChild(1, firstChild->getFirstChild()->getSecondChild());

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();
         return node;
         }
      }

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getLongInt() == 0 &&
       firstChild->getOpCodeValue() == TR::land)
      {
      TR::Node* firstFirst = firstChild->getFirstChild();
      TR::Node* firstSecond = firstChild->getSecondChild();
      if (firstSecond->getOpCodeValue() == TR::lshl &&
          firstSecond->getFirstChild()->getOpCode().isLoadConst() &&
          firstSecond->getFirstChild()->getLongInt() == 1 &&
          performTransformation(s->comp(), "%slcmpne of x & (1 << y) to 0 opt node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
         {
         TR::Node::recreate(node, TR::iand);
         TR::Node* one = TR::Node::create(node, TR::iconst, 0);
         one->setInt(1);

         TR::Node* shift = TR::Node::create(TR::lushr, 2);
         shift->setAndIncChild(0, firstFirst);
         shift->setAndIncChild(1, firstSecond->getSecondChild());

         TR::Node* conv = TR::Node::create(TR::l2i, 1);
         conv->setAndIncChild(0, shift);

         node->setAndIncChild(1, one);
         node->setAndIncChild(0, conv);
         firstChild->recursivelyDecReferenceCount();
         secondChild->decReferenceCount();
         }
      else if (firstSecond->getOpCodeValue() == TR::lconst)
         {
         uint64_t val = firstSecond->getLongInt();
         if ((val & (val - 1)) == 0 && // power of two?
             performTransformation(s->comp(), "%slcmpne of (x & 2**c) to 0 node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
            {
            uint32_t byVal = trailingZeroes(val);
            TR::Node* shiftBy = TR::Node::create(node, TR::iconst, 0);
            shiftBy->setInt(byVal);
            TR::Node* shift = TR::Node::create(TR::lushr, 2);
            shift->setAndIncChild(0, firstFirst);
            shift->setAndIncChild(1, shiftBy);
            TR::Node* conv = TR::Node::create(TR::l2i, 1);
            conv->setAndIncChild(0, shift);

            TR::Node* one = TR::Node::create(node, TR::iconst, 0);
            one->setInt(1);

            TR::Node::recreate(node, TR::iand);
            node->setAndIncChild(0, conv);
            node->setAndIncChild(1, one);
            firstChild->recursivelyDecReferenceCount();
            secondChild->decReferenceCount();
            }
         }
      }

   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *lcmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getLongInt()<secondChild->getLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getLongInt() == 0ll &&
       performTransformation(s->comp(), "%sReplace lcmplt to 0 with lushr node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
      {
      TR::Node* shiftBy = TR::Node::create(node, TR::iconst, 0);
      shiftBy->setInt(63);
      // TR::Node::recreate(node, TR::lushr);
      TR::Node* shift = TR::Node::create(TR::lushr, 2, firstChild, shiftBy);
      TR::Node::recreate(node, TR::l2i);
      node->setAndIncChild(0, shift);
      node->setNumChildren(1);
      firstChild->recursivelyDecReferenceCount();
      secondChild->decReferenceCount();
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *lcmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getLongInt()<=secondChild->getLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *lcmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getLongInt()>secondChild->getLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

TR::Node *lcmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getLongInt()>=secondChild->getLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   removeArithmeticsUnderIntegralCompare(node, s);

   return node;
   }

//---------------------------------------------------------------------
// Unsigned long compares
//

TR::Node *lucmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
{
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getUnsignedLongInt()<secondChild->getUnsignedLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
}

TR::Node *lucmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
{
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getUnsignedLongInt()<=secondChild->getUnsignedLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
}

TR::Node *lucmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
{
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getUnsignedLongInt()>secondChild->getUnsignedLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
}

TR::Node *lucmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
{
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getUnsignedLongInt()>=secondChild->getUnsignedLongInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
}

//---------------------------------------------------------------------
// Address compares
//

TR::Node *acmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()==secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *acmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getInt()!=secondChild->getInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   return node;
   }

//---------------------------------------------------------------------
// Byte compares
//

TR::Node *bcmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getByte()==secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *bcmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getByte()!=secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);

   if (secondChild->getOpCode().isLoadConst() && (secondChild->getByte() == 0))
      if (firstChild->getOpCodeValue() == TR::bor)
         if (firstChild->getSecondChild()->getOpCode().isLoadConst() && (firstChild->getSecondChild()->getByte() != 0))
            {
            // A | C != 0  ==> 1, when C != 0
            foldIntConstant(node, 1, s, true /* anchorChildren*/);
            return node;
            }

   return node;
   }

TR::Node *bcmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getByte()<secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }


   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *bcmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getByte()<=secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }


   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *bcmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getByte()>secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }


   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *bcmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getByte()>=secondChild->getByte(), s, false /* !anchorChildren*/);
      return node;
      }


   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

//---------------------------------------------------------------------
// Short compares
//

TR::Node *scmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getShortInt()==secondChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *scmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getShortInt()!=secondChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *scmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getShortInt()<secondChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }


   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *scmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getShortInt()<=secondChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *scmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getShortInt()>secondChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *scmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getShortInt()>=secondChild->getShortInt(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *sucmpeqSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getConst<uint16_t>()==secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *sucmpneSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getConst<uint16_t>()!=secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }

   orderChildren(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *sucmpltSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
     return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getConst<uint16_t>()<secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *sucmpgeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getConst<uint16_t>()>=secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *sucmpgtSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 0, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getConst<uint16_t>()>secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

TR::Node *sucmpleSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild == secondChild)
      {
      foldIntConstant(node, 1, s, true /* anchorChildren */);
      return node;
      }
   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldIntConstant(node, firstChild->getConst<uint16_t>()<=secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }

   makeConstantTheRightChildAndSetOpcode(node, firstChild, secondChild, s);
   return node;
   }

//---------------------------------------------------------------------
// long compare
//

TR::Node *lcmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, LongInt);
   return node;
   }

//---------------------------------------------------------------------
// Pass-through
//

TR::Node *passThroughSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   // Collapse multiple levels of pass-through
   //
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::PassThrough)
      {
      TR::Node * grandChild = firstChild->getFirstChild();
      grandChild->incReferenceCount();
      s->prepareToStopUsingNode(firstChild, s->_curTree);
      firstChild->recursivelyDecReferenceCount();
      node->setFirst(grandChild);
      }
   return node;
   }

//---------------------------------------------------------------------
// End of basic block - see if the block can be merged with the following
// block.
//

TR::Node *endBlockSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR_ASSERT(node->getBlock() == block, "Simplification, BBEnd %s points to block_%d (should be %d)",
      s->comp()->getDebug()->getName(node),
      node->getBlock()->getNumber(),
      block->getNumber());

   if (s->comp()->getProfilingMode() == JitProfiling)
      return node;

   // See if this block has a single successor by looking at the CFG
   //
   if (block->getSuccessors().size() != 1)
      return node;

   // See if the successor is the textually following block, and this is the only
   // predecessor of the following block.
   //
   TR::TreeTop * bbEndTree   = block->getExit();
   TR::TreeTop * bbStartTree = bbEndTree->getNextTreeTop();
   if (bbStartTree == NULL)
      return node;

   TR::Node * bbStartNode = bbStartTree->getNode();
   TR_ASSERT(bbStartNode->getOpCodeValue() == TR::BBStart, "Simplification, BBEnd not followed by BBStart");

   TR::Block * nextBlock                     = bbStartNode->getBlock();
   TR::CFGEdgeList   &inEdge        = nextBlock->getPredecessors();
   TR::CFGEdgeList::iterator inEdgeIter = inEdge.begin();
   TR::CFGEdgeList        &nextExcpOut     = nextBlock->getExceptionSuccessors();
   bool                     moreThanOnePred = (!(nextBlock->getPredecessors().empty()) && (nextBlock->getPredecessors().size() > 1));

   // Exclude all the illegal cases first since we don't want to return in the middle of merging
   bool blockIsEmpty;
   if (!isLegalToMerge(node, block, nextBlock, nextExcpOut, bbStartNode, inEdge,  s, blockIsEmpty))
      return node;

   if (moreThanOnePred)
      changeBranchDestinationsForMergeBlocks(block, nextBlock, bbStartNode, inEdge, inEdgeIter,  s);

   TR::CFG * cfg = s->comp()->getFlowGraph();


   s->_invalidateUseDefInfo = true;
   s->_alteredBlock = true;
   s->_blockRemoved = true;


   if (cfg)
      {
      // Fix up structure by telling it that the blocks are going to be merged.
      //
      TR_Structure * rootStructure = cfg->getStructure();
      if (rootStructure != NULL)
         {
         for (auto excEdge = block->getExceptionSuccessors().begin(); excEdge != block->getExceptionSuccessors().end();)
            cfg->removeEdge(*(excEdge++));

         //mergeblock keep nextBlock but renumber it to block's number
         //it's needed since we may merge block 2 to block 3, but first block must be numbered as 2
         //However mergeBlock only handle structure edges and numbers
         //This is inconsistent with what we do for CFG in the following, it's better for mergeBlock to merge nextBlock to block, this is a TODO
         rootStructure->mergeBlocks(block, nextBlock);

         if(s->containingStructure() == block->getStructureOf()->asRegion())
            {
            s->_containingStructure = nextBlock->getStructureOf()->asRegion();
            }

         if (s->trace())
            {
            traceMsg(s->comp(), "\nStructures after merging blocks:\n");
            s->getDebug()->print(s->comp()->getOutFile(), rootStructure, 6);
            }
         }

      // Fix up the CFG by giving the next block's successors to this block.
      // Also remove exception edges from the next block.
      //
      TR::TreeTop * lastTree = block->getLastRealTreeTop();
      if (lastTree->getNode()->getOpCode().isBranch() ||
          (lastTree->getNode()->getOpCode().isJumpWithMultipleTargets() && lastTree->getNode()->getOpCode().hasBranchChildren()))
         {
         s->prepareToStopUsingNode(lastTree->getNode(), s->_curTree);
         TR::TransformUtil::removeTree(s->comp(), lastTree);
         }

      TR::CFGEdge *e = (*inEdgeIter);
      e->getFrom()->getSuccessors().remove(e);
      e->getTo()->getPredecessors().remove(e);

      for (auto outEdge = nextBlock->getSuccessors().begin(); outEdge != nextBlock->getSuccessors().end(); ++outEdge)
         {
         (*outEdge)->setFrom(block);
         }
      for (auto outEdge = nextExcpOut.begin(); outEdge != nextExcpOut.end(); ++outEdge)
         {
         if (rootStructure != NULL)
            (*outEdge)->setExceptionFrom(block);
         else
            {
            (*outEdge)->getTo()->getExceptionPredecessors().remove((*outEdge));
            }
         }
      cfg->getNodes().remove(nextBlock);
      nextBlock->removeNode();
      //cfg->getRemovedNodes().add(nextBlock);
      }

   // Fix up the trees:
   //    Put this block into the BBEnd node for the next block
   //    Change the exit treetop for this block
   //    Remove the BBStart for the next block
   //    Remove the BBEnd for this block (by returning null from this method)
   //
   //    if (!nextBlock->isCold())
   //       block->setIsCold(false);
   block->inheritBlockInfo(nextBlock,false);

   // Ali: i believe this is the correct way of merging
   // it is important to copy the bytecode and frequency information in a consistent manner with
   // how it is done in other block merge operations (in particular, goto elimination) so that
   // frequency information attached to bytecode isn't lost in the process of block merging.
   // But Wcode compiler which generates asm file will need the line number info to be correct
   // for BBstart node of Prolog so no copy.
   //
   // dont copy the bytecodeinfo for catch blocks. liveMonitor analysis relies on accurate
   // callerIndex information to propagate locked objects. copying can result in the wrong
   // bytecode info on the catch block, in turn incorrect gc stack maps
   //
   if (!block->isCatchBlock())
      block->getEntry()->getNode()->copyByteCodeInfo(nextBlock->getEntry()->getNode());

   if (nextBlock->hasCalls())
      block->setHasCalls(true);
   if (nextBlock->hasCallToSuperCold())
     block->setHasCallToSuperCold(true);

   block->setIsCold(nextBlock->isCold());
   block->setIsSuperCold(nextBlock->isSuperCold());
   if ((nextBlock->getPredecessors().size() == 1) &&
       (block->getFrequency() <= nextBlock->getFrequency()))
      block->setFrequency(nextBlock->getFrequency());

   if (cfg->getStructure())
      {
      block->getStructureOf()->setWasHeaderOfCanonicalizedLoop(false);
      nextBlock->getStructureOf()->setWasHeaderOfCanonicalizedLoop(false);
      }

   nextBlock->getExit()->getNode()->setBlock(block);
   block->setExit(nextBlock->getExit());
   s->prepareToStopUsingNode(bbStartTree->getNode(), s->_curTree);
   TR::TransformUtil::removeTree(s->comp(), bbStartTree);
   s->prepareToStopUsingNode(node, s->_curTree);
   s->requestOpt(OMR::basicBlockPeepHole);
   return NULL;
   }

//---------------------------------------------------------------------
// Ternary
//

TR::Node *ternarySimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->getFirstChild()->getOpCode().isLoadConst())
      {
      int64_t value = node->getFirstChild()->get64bitIntegralValue();
      TR::Node * newNode = value ? node->getChild(1) : node->getChild(2);
      return s->replaceNode(node, newNode, s->_curTree);
      }

   if (node->getChild(1) == node->getChild(2))
      return s->replaceNode(node, node->getChild(1), s->_curTree);

   // sometimes the children are different but represent the same value
   if (node->getChild(1)->getOpCode().isLoadConst() && node->getChild(2)->getOpCode().isLoadConst())
      if (node->getChild(1)->getOpCode().isInteger() && node->getChild(2)->getOpCode().isInteger())
         if (node->getChild(1)->get64bitIntegralValue() == node->getChild(2)->get64bitIntegralValue())
            return s->replaceNode(node, node->getChild(1), s->_curTree);

   return node;
   }

//---------------------------------------------------------------------
// Address conversion
//

TR::Node *a2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      if (firstChild->getType().isAddress())
         foldIntConstant(node, firstChild->getAddress(), s, false /* !anchorChildren */);
      else
         foldIntConstant(node, firstChild->get64bitIntegralValue(), s, false /* !anchorChildren */);
      }
   else
      {
      if (firstChild->isNonNull())
      node->setIsNonZero(true);
      }
   return node;
   }

TR::Node *a2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      if (firstChild->getType().isAddress())
         {
         //on Z the address cast evaluation of a2l of a 4 byte addr child will always zero out bit 32, need to do equivalently here
         if (TR::Compiler->target.cpu.isZ() && node->getFirstChild()->getSize() == 4)
            foldLongIntConstant(node, firstChild->getAddress() & CLEARBIT32, s, false /* !anchorChildren */);
         else
            foldLongIntConstant(node, firstChild->getAddress(), s, false /* !anchorChildren */);
         }
      else
         foldLongIntConstant(node, firstChild->get64bitIntegralValue(), s, false /* !anchorChildren */);
      }
   else
      {
      if (firstChild->isNonNull())
         node->setIsNonZero(true);
      }
   return node;
   }


/*
 * Helper functions for Simplifier handlers
 */

//---------------------------------------------------------------------
// Table and lookup switch
//
TR::Node *switchSimplifier(TR::Node * node, TR::Block * block, bool isTableSwitch, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   TR::TreeTop * target;
   int32_t caseIndex;
   uint32_t numCases = node->getCaseIndexUpperBound() - 2;
   bool convertToGoto = false;

   if (firstChild->getOpCode().isLoadConst())
      {
      uint64_t caseValue = 0;
      if (firstChild->getType().isInt64()) caseValue = (uint64_t)firstChild->getLongInt();
      else caseValue = (uint64_t)firstChild->getInt();
      convertToGoto = true;
      // Find the case index that matches the selector value
      //
      if (isTableSwitch)
         {
         if (caseValue >= numCases) caseIndex = 1; // Take the default destination
         else caseIndex = (uint32_t)caseValue+2; // Take a case destination
         }
      else
         {
         // Try to match a case value against the constant value.
         // If they all fail the case index will be pointing to the default case.
         //
         for (caseIndex = numCases+1; caseIndex > 1; caseIndex--)
            if (node->getChild(caseIndex)->getCaseConstant() == caseValue) break;
         }

      target = node->getChild(caseIndex)->getBranchDestination();
      // Remove CFG edges to all destinations other than the target. Allow for
      // duplicate destinations.
      //
      bool blocksWereRemoved = false;
      TR::CFG * cfg = s->comp()->getFlowGraph();
      if (cfg)
         {
         TR_BitVector removedTargets(cfg->getNumberOfNodes(), s->comp()->trMemory(), stackAlloc, growable);
         for (caseIndex = numCases+1; caseIndex >= 1; caseIndex--)
            {
            TR::TreeTop * dest = node->getChild(caseIndex)->getBranchDestination();
            if (dest != target)
               {
               TR::Block * destBlock = dest->getNode()->getBlock();
               if (!removedTargets.get(destBlock->getNumber()) &&
                   performTransformation(s->comp(), "%sRemoving edge block_%d->block_%d in CFG because switch value is known\n", s->optDetailString(), block->getNumber(), destBlock->getNumber()))
                  {
                  blocksWereRemoved |= cfg->removeEdge(block, destBlock);
                  removedTargets.set(destBlock->getNumber());
                  }
               }
            }
         }
      if (blocksWereRemoved)
         {
         s->_invalidateUseDefInfo = true;
         s->_alteredBlock = true;
         s->_blockRemoved = true;
         }
      }
   else
      {
      // See if there is only one possible switch target.
      // If so, the switch is degenerate and can be replaced by a goto.
      //
      convertToGoto = true;
      target = node->getSecondChild()->getBranchDestination();
      for (caseIndex = numCases+1; caseIndex > 1; caseIndex--)
         if (node->getChild(caseIndex)->getBranchDestination() != target)
            {
            convertToGoto = false;
            break;
            }

      // If the first child is a shift node (possibly with a conversion node as a parent), we can eliminate the
      // shift by performing the reverse shift on all the case values (eg. instead of left-shifting the first child by two,
      // right-shift all the case values by two instead).
      TR::Node *shiftParent = node, *shiftNode = firstChild;
      if (NULL != shiftNode && shiftNode->getOpCode().isConversion())
         {
         shiftParent = firstChild;
         shiftNode = firstChild->getFirstChild();
         }

      if (node->getOpCode().getOpCodeValue() == TR::lookup && NULL != shiftNode && shiftNode->getOpCode().isShift())
         {
         // Shift amount must be constant
         TR::Node *shiftAmtNode = shiftNode->getSecondChild();
         if (NULL != shiftAmtNode && shiftAmtNode->getOpCode().isLoadConst())
            {
            // In order for the transformation to be accurate, any bits shifted out of either the selector node or the
            // case constants must be 0. For the constants, we can check the bits to be shifted out at compile time. For
            // the shift node, which is non-constant, the only way we know those bits are 0 is if they are explicitly
            // zeroed with an AND node.
            //
            // Example: if the selector is 3 (binary 11) and we shift it right 1, and have case values 0 and 1, we'll match 1 (since 11b >> 1 is 1b)
            // If we shift 0 and 1 left 1 bit, we'll compare 11b to 00b and 10b, and not have a match.
            int32_t shiftAmount = shiftAmtNode->getInt();
            bool isLeftShift = shiftNode->getOpCode().isLeftShift();
            int32_t lowNBits = 0, highNBits32 = 0;
            int64_t highNBits64 = 0;
            bool canTransform = true;

            for (int i = 0; i < shiftAmount; i++)
               lowNBits = (lowNBits << 1) + 1;
            highNBits32 = lowNBits << (32 - shiftAmount);
            highNBits64 = lowNBits << ((shiftNode->getOpCode().getSize() * 8) - shiftAmount);

            // Make sure all the case nodes can be shifted safely
            for (int caseIndex = 0; caseIndex < numCases; caseIndex++)
               {
               TR::Node *caseNode = node->getChild(caseIndex + 2);
               if ((isLeftShift && ((caseNode->getCaseConstant() & lowNBits) != 0)) || (!isLeftShift && ((caseNode->getCaseConstant() & highNBits32) != 0)))
                  {
                  canTransform = false;
                  break;
                  }
               }

            // Make sure we have an AND node
            TR::Node *andNode = shiftNode->getFirstChild();
            if (NULL != andNode && andNode->getOpCode().isConversion())
               andNode = andNode->getFirstChild();
            if (NULL != andNode && andNode->getOpCode().isAnd() && NULL != andNode->getSecondChild() && andNode->getSecondChild()->getOpCode().isLoadConst())
               {
               // Make sure that ANDing will zero the bits that will be shifted away
               int64_t andConst = andNode->getSecondChild()->get64bitIntegralValue();
               int64_t shiftConst = lowNBits;

               // If we're left-shifting, it's the high bits that need to be set instead
               if (isLeftShift)
                  shiftConst = highNBits64;

               if ((shiftConst & andConst) != 0)
                  canTransform = false;
               }
            else
               canTransform = false;

            if (!canTransform || !performTransformation(s->comp(), "%sRemoving shift node [" POINTER_PRINTF_FORMAT "] from lookup node [" POINTER_PRINTF_FORMAT "], applying shift to case constants\n", s->optDetailString(), shiftNode, node))
               return node;

            // Restructure the tree to remove the calculation
            shiftParent->setAndIncChild(0, shiftNode->getFirstChild());
            shiftNode->recursivelyDecReferenceCount();


            // Adjust the case values
            for (int caseIndex = 0; caseIndex < numCases; caseIndex++)
               {
               TR::Node *caseNode = node->getChild(caseIndex + 2);
               caseNode->setCaseConstant(isLeftShift ? (caseNode->getCaseConstant() >> shiftAmount) : (caseNode->getCaseConstant() << shiftAmount));
               }
            }
         }
      }

   if (convertToGoto)
      {
      // Change the switch into a goto
      //
      if (!performTransformation(s->comp(), "%sChanging node [" POINTER_PRINTF_FORMAT "] %s into goto\n", s->optDetailString(), node, node->getOpCode().getName()))
         return node;

      s->anchorChildren(node, s->_curTree);
      s->prepareToReplaceNode(node);
      TR::Node::recreate(node, TR::Goto);
      node->setBranchDestination(target);
      return s->simplify(node, block);
      }
   else
      {
      //Reduce cases that are just gotos, ie make the case go where the goto does.
      bool blocksWereRemoved = false;
      TR::CFG * cfg = s->comp()->getFlowGraph();
      int32_t numChildren = node->getCaseIndexUpperBound();
      for(int childIndex=1; childIndex<numChildren; childIndex++)//loop from default to last case
         {
         TR::Node * childNode = node->getChild(childIndex);
         TR::Block * destBlock = childNode->getBranchDestination()->getNode()->getBlock();
         TR::Node * destInst = destBlock->getFirstRealTreeTop()->getNode();
         if(destInst->getOpCodeValue() == TR::Goto)
            {
            TR::TreeTop * newDest = destInst->getBranchDestination();
            TR::Block * newDestBlock = newDest->getNode()->getBlock();
            if (!performTransformation(s->comp(), "%sRedirecting switch [" POINTER_PRINTF_FORMAT "] child %d from block_%d to block_%d\n", s->optDetailString(), node, childIndex, destBlock->getNumber(), newDestBlock->getNumber()))
               return node;

            // Removing a block containing a goto shouldn't be logged for deleted line processing

            childNode->setBranchDestination(newDest);
            if (cfg)
               {
               //Add edge from this block to new destination block it doesn't already exist.
               if (!block->hasSuccessor(newDestBlock))
                  cfg->addEdge(block, newDestBlock);
               //Remove edge between this block and old destination block if this is the only case that leads to the block.
               int otherChildIndex;
               for(otherChildIndex=1; otherChildIndex<numChildren; ++otherChildIndex)//Look for another case/default that leads to destBlock.
                  if(otherChildIndex!=childIndex)//Another case/default, not this case.
                     if(destBlock==node->getChild(otherChildIndex)->getBranchDestination()->getNode()->getBlock())
                        break;//If there's another case/default to destBlock.
               if(otherChildIndex==numChildren)//If another case/default to destBlock wasn't found,
                  blocksWereRemoved|=cfg->removeEdge(block, destBlock);//remove the edge.
               }

            }
         }
      if(blocksWereRemoved) s->_invalidateUseDefInfo = s->_alteredBlock = s->_blockRemoved = true;
      }

   return node;
   }

static void foldUnsignedLongIntConstant(TR::Node * node, uint64_t value, TR::Simplifier * s, bool anchorChildrenP)
   {
   if (!performTransformationSimplifier(node, s)) return;

   if (anchorChildrenP) s->anchorChildren(node, s->_curTree);

   s->prepareToReplaceNode(node, TR::luconst);
   node->setUnsignedLongInt(value);
   setIsHighWordZero(node, s);

   dumpOptDetails(s->comp(), " to %s", node->getOpCode().getName());
   if (node->getLongIntHigh() == 0)
      dumpOptDetails(s->comp(), " 0x%x\n", node->getLongIntLow());
   else
      dumpOptDetails(s->comp(), " 0x%x%08x\n", node->getLongIntHigh(), node->getLongIntLow());
   }

inline OMR::TR_ConditionCodeNumber calculateSignedCC(int64_t result, bool overflow)
   {
   if ((result == 0) && !overflow)
      return OMR::ConditionCode0;
   else if ((result < 0) && !overflow)
      return OMR::ConditionCode1;
   else if ((result > 0) && !overflow)
      return OMR::ConditionCode2;
   else // (overflow)
      return OMR::ConditionCode3;
   }

inline OMR::TR_ConditionCodeNumber calculateUnsignedCC(uint64_t result, bool carryNotBorrow)
   {
   if (result == 0 && !carryNotBorrow)
      return OMR::ConditionCode0;
   else if ((result != 0) && !carryNotBorrow)
      return OMR::ConditionCode1;
   else if ((result == 0) && carryNotBorrow)
      return OMR::ConditionCode2;
   else // if ((result != 0) && carryNotBorrow)
      return OMR::ConditionCode3;
   }

// Both lmulh and lmulhu are adapted from Hackers Delight, Figure 8-2, pg. 132
// uses the simple digit by digit multiplication
//
//             u1 u0
//        x    v1 v0
//        ----------
//       w3 w2 w1 w0
//
// top 64 bits of 128 bit product are in t = w3:w2
//
//
// signed high-order half of 64-bit product
//
static int64_t lmulh(int64_t op1, int64_t op2)
   {
   uint64_t u0, v0, w0;
   int64_t u1, v1, w1, w2, t;

   u0 = op1 & 0xFFFFFFFF; u1 = op1 >> 32;
   v0 = op2 & 0xFFFFFFFF; v1 = op2 >> 32;

   w0 = u0 * v0;

   t = u1 * v0 + (w0 >> 32);
   w1 = t & 0xFFFFFFFF;
   w2 = t >> 32;

   w1 = u0 * v1 + w1;
   t = u1 * v1 + w2 + (w1 >> 32);

   return t;
   }

// unsigned high-order half of 64-bit product
//
static uint64_t lmulhu(uint64_t op1, uint64_t op2)
   {
   // identical to lmulh, except all int64_t become uint64_t
   uint64_t u0, v0, w0;
   uint64_t u1, v1, w1, w2, t;

   u0 = op1 & 0xFFFFFFFF; u1 = op1 >> 32;
   v0 = op2 & 0xFFFFFFFF; v1 = op2 >> 32;

   w0 = u0 * v0;

   t = u1 * v0 + (w0 >> 32);
   w1 = t & 0xFFFFFFFF;
   w2 = t >> 32;

   w1 = u0 * v1 + w1;
   t = u1 * v1 + w2 + (w1 >> 32);

   return t;
   }

static void foldUByteConstant(TR::Node * node, uint8_t value, TR::Simplifier * s, bool anchorChildrenP)
   {
   if (!performTransformationSimplifier(node, s)) return;

   if (anchorChildrenP) s->anchorChildren(node, s->_curTree);

   s->prepareToReplaceNode(node, TR::buconst);
   node->setUnsignedByte(value);
   dumpOptDetails(s->comp(), " to %s %d\n", node->getOpCode().getName(), node->getUnsignedByte());
   }

static void foldCharConstant(TR::Node * node, uint16_t value, TR::Simplifier * s, bool anchorChildrenP)
   {
   if (!performTransformationSimplifier(node, s)) return;

   if (anchorChildrenP) s->anchorChildren(node, s->_curTree);

   s->prepareToReplaceNode(node, TR::cconst);
   node->setConst<uint16_t>(value);
   dumpOptDetails(s->comp(), " to %s %d\n", node->getOpCode().getName(), node->getConst<uint16_t>());
   }

static void removeRestOfBlock(TR::TreeTop *curTree, TR::Compilation *compilation)
   {
   // Remove the rest of the trees in the block.
   //
   TR::TreeTop * treeTop, * next;
   for (treeTop = curTree->getNextTreeTop();
        treeTop->getNode()->getOpCodeValue() != TR::BBEnd;
        treeTop = next)
      {
      //s->removeNode(treeTop->getNode(), s->_curTree, false);
      next = treeTop->getNextTreeTop();
      TR::TransformUtil::removeTree(compilation, treeTop);
      }
   }

static bool isNodeMulHigh(TR::Node *node)
   {
   TR::ILOpCodes mulOp = node->getOpCodeValue();
   return node->getOpCode().isMul() && (mulOp == TR::imulh || mulOp == TR::lmulh || mulOp == TR::lumulh || mulOp == TR::iumulh);
   }

/*
 * Simplifier handlers
 */

//---------------------------------------------------------------------
// Checkcast
//

TR::Node *checkcastSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::Node *fc = node->getFirstChild();

   if ((node->getFirstChild()->getReferenceCount() > 1) &&
       (node->getSecondChild()->getReferenceCount() > 1))
      {
      TR::TreeTop * currTree = block->getEntry();
      while (currTree->getNode() != node)
         currTree = currTree->getNextRealTreeTop();

      TR::TreeTop * prevTree = currTree->getPrevRealTreeTop();
      while (prevTree &&
             ((prevTree->getNode()->getOpCodeValue() == TR::BBStart) ||
              (prevTree->getNode()->getOpCodeValue() == TR::BBEnd)))
          prevTree = prevTree->getPrevRealTreeTop();

      if (prevTree)
         {
         TR::Node * prevNode = prevTree->getNode();
         if ((prevNode->getOpCodeValue() == TR::ificmpeq) ||
             (prevNode->getOpCodeValue() == TR::ificmpne))
            {
            bool equalityTest = true;
            if (prevNode->getOpCodeValue() == TR::ificmpne)
               equalityTest = false;

            TR::Node * firstChild = prevNode->getFirstChild();
            TR::Node * secondChild = prevNode->getSecondChild();

            if (firstChild->getOpCodeValue() == TR::instanceof &&
                secondChild->getOpCodeValue() == TR::iconst &&
                ((secondChild->getInt() == 0 && equalityTest) || (secondChild->getInt() == 1 && !equalityTest)) &&
                firstChild->getFirstChild() == node->getFirstChild() &&
                firstChild->getSecondChild() == node->getSecondChild() &&
                performTransformation(s->comp(), "%sRemoving checkcast node [" POINTER_PRINTF_FORMAT "]\n", s->optDetailString(), node))
               {
               // printf("Removing checkcast in method %s\n", s->comp()->signature());
               node->getFirstChild()->decReferenceCount();
               node->getSecondChild()->decReferenceCount();
               currTree->getPrevTreeTop()->join(currTree->getNextTreeTop());
               return node;
               }
            }
         }
      }

   simplifyChildren(node, block, s);
   return node;
   }

//---------------------------------------------------------------------
// Checkcast and NULLCHK
//

TR::Node *checkcastAndNULLCHKSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return checkcastSimplifier(node, block, s);
   }

//---------------------------------------------------------------------
// Variablenew and similar
//

TR::Node *variableNewSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (  node->getOpCodeValue() == TR::variableNew
      && node->getFirstChild()->getOpCodeValue() == TR::loadaddr
      && performTransformation(s->comp(), "%sReplacing TR::variableNew %p with TR::New\n", s->optDetailString(), node))
      {
      TR::Node::recreate(node, TR::New);
      }

   return node;
   }

//---------------------------------------------------------------------
// Char add
//

TR::Node *caddSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldCharConstant(node, firstChild->getConst<uint16_t>() + secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }
   orderChildren(node, firstChild, secondChild, s);

   BINARY_IDENTITY_OP(UnsignedShortInt, 0)
   return node;
   }

//---------------------------------------------------------------------
// Char subtract
//

TR::Node *csubSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      foldCharConstant(node, firstChild->getConst<uint16_t>() - secondChild->getConst<uint16_t>(), s, false /* !anchorChildren*/);
      return node;
      }

   BINARY_IDENTITY_OP(UnsignedShortInt, 0)
   return node;
   }

//---------------------------------------------------------------------
// High word of 32x32 multiply
//
//

TR::Node * imulhSimplifier(TR::Node * node, TR::Block *block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();
   orderChildren(node, firstChild, secondChild, s);


   if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if (performTransformationSimplifier(node,s))
         {
         s->prepareToReplaceNode(node);
         if (node->getOpCode().isUnsigned())
            {
            uint64_t src1 = firstChild->getUnsignedInt();
            uint64_t src2 = secondChild->getUnsignedInt();
            uint64_t product = src1 * src2;
            uint64_t high = product >> 32;
            uint32_t result = high;
            TR::Node::recreate(node, TR::iuconst);
            node->setUnsignedInt(result);
            }
         else
            {
            int64_t src1 = firstChild->getInt();
            int64_t src2 = secondChild->getInt();
            int64_t product = src1 * src2;
            int64_t high = product >> 32;
            int32_t result = high;
            TR::Node::recreate(node, TR::iconst);
            node->setInt(result);
            }
         }
      }
   else if(secondChild->getOpCode().isLoadConst())
      {
      int64_t src2 = secondChild->getInt();
      if (src2 == 0
         && performTransformation(s->comp(), "%ssecond child [%p] of node [%p] is 0, setting the result of imulh to 0\n",s->optDetailString(), secondChild, node))
         {
         s->prepareToReplaceNode(node);
         TR::Node::recreate(node, TR::iconst);
         node->setInt(0);
         }
      else if (src2 == 1 || src2 == 2)
         {
         if (firstChild->isNegative()
            && performTransformation(s->comp(), "%sfirst child [%p] of node [%p] is negative, setting the result of imulh to -1\n",s->optDetailString(), firstChild, node))
            {
            s->prepareToReplaceNode(node);
            TR::Node::recreate(node, TR::iconst);
            node->setInt(-1);
            }
         else if (firstChild->isNonNegative()
                  && performTransformation(s->comp(), "%sfirst child [%p] of node [%p] is non-negative, setting the result of imulh to 0\n",s->optDetailString(), firstChild, node))
            {
               s->prepareToReplaceNode(node);
               TR::Node::recreate(node, TR::iconst);
               node->setInt(0);
            }
         }
      // If src2 == 2^n
      // imulh             ishr
      //  /    \   =>      /  \
      // src1  src2      src1 32-n
      else if (src2>0 && !(src2 & (src2 - 1))
               && performTransformation(s->comp(), "%ssecond child [%p] of node [%p] is 2's power, converting imulh to ishr\n",s->optDetailString(),secondChild,node))
         {
         // Calculate log2(src2), which is 32-msb
         int32_t msb = 0;
         while (src2 >>= 1) msb++;
         TR::Node::recreate(node, TR::ishr);
         TR::Node * newSecondChild = TR::Node::create(TR::iconst, 0); // Create a new node because changing the value of the node might affect trees referencing this node
         newSecondChild->setInt(32-msb);
         secondChild->recursivelyDecReferenceCount();
         node->setAndIncChild(1, newSecondChild);
         }
      }
   return node;
   }


//---------------------------------------------------------------------
// Long multiply high
//

TR::Node *lmulhSimplifier(TR::Node * node, TR::Block *block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   if (node->isDualHigh())
      {
      // do not simplify dual operators
      return node;
      }

   TR::Node * firstChild = node->getFirstChild(), * secondChild = node->getSecondChild();

   if(firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
      {
      if(performTransformationSimplifier(node,s))
         {
         s->prepareToReplaceNode(node);

         if (node->getOpCode().isUnsigned())
            {
            TR::Node::recreate(node, TR::lconst);
            node->setUnsignedLongInt(lmulhu(firstChild->getUnsignedLongInt(), secondChild->getUnsignedLongInt()));
            }
         else
            {
            TR::Node::recreate(node, TR::lconst);
            node->setLongInt(lmulh(firstChild->getLongInt(), secondChild->getLongInt()));
            }
         }
      }
   return node;
   }

//---------------------------------------------------------------------
// Float convert to char
//

TR::Node *f2cSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldCharConstant(node, floatToInt(firstChild->getFloat(), false)&0x0000FFFF, s, false /* !anchorChildren*/);
      return node;
      }

   return node;
   }

//---------------------------------------------------------------------
// Double convert to char
//

TR::Node *d2cSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCode().isLoadConst())
      {
      foldCharConstant(node, doubleToInt(firstChild->getDouble(), false)&0x0000FFFF, s, false /* !anchorChildren*/);
      return node;
      }

   return node;
   }


TR::Node *expSimplifier(TR::Node *node,TR::Block *block,TR::Simplifier *s)
   {
   simplifyChildren(node, block, s);
   return replaceExpWithMult(node,node->getFirstChild(),node->getSecondChild(),block,s);
   }

//---------------------------------------------------------------------
// Type coersion operators
//
TR::Node *ibits2fSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::iconst)
      {
      TR::Node::recreate(node, TR::fconst);
      node->setNumChildren(0);
      node->setFloatBits(firstChild->getInt());
      firstChild->recursivelyDecReferenceCount();
      }
   return node;
   }

TR::Node *lbits2dSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::lconst)
      {
      TR::Node::recreate(node, TR::dconst);
      node->setNumChildren(0);
      node->setDouble(firstChild->getDouble());
      firstChild->recursivelyDecReferenceCount();
      }
   return node;
   }

TR::Node *fbits2iSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::fconst)
      {
      // Normalize Nan value while creating integer constant
      //
      int32_t intValue;
      if (node->normalizeNanValues() && isNaNFloat(firstChild))
         intValue = FLOAT_NAN;
      else
         intValue = firstChild->getFloatBits();
      TR::Node::recreate(node, TR::iconst);
      node->setInt(intValue);
      node->setNumChildren(0);
      firstChild->recursivelyDecReferenceCount();
      }
   return node;
   }

TR::Node *dbits2lSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node * firstChild = node->getFirstChild();

   if (firstChild->getOpCodeValue() == TR::dconst)
      {
      // Normalize Nan value while creating long constant
      //
      int64_t longValue;
      if (node->normalizeNanValues() && isNaNDouble(firstChild))
         longValue = DOUBLE_NAN;
      else
         longValue = firstChild->getLongInt();
      TR::Node::recreate(node, TR::lconst);
      node->setLongInt(longValue);
      node->setNumChildren(0);
      firstChild->recursivelyDecReferenceCount();
      }
   return node;
   }

TR::Node *ifxcmpoSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;
   simplifyChildren(node, block, s);

   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR_ASSERT((opCode == TR::ificmpo) || (opCode == TR::ificmpno) || (opCode == TR::iflcmpo) || (opCode == TR::iflcmpno), "unsupported ifxcmpo opcode");

   if (node->getFirstChild()->getOpCode().isLoadConst() && node->getSecondChild()->getOpCode().isLoadConst())
      {
      int64_t op1 = node->getFirstChild()->get64bitIntegralValue();
      int64_t op2 = node->getSecondChild()->get64bitIntegralValue();
      int64_t res = ((opCode == TR::iflcmpo) || (opCode == TR::iflcmpno)) ? op1 - op2 : (int32_t)(op1 - op2);
      bool overflow = ((op1 < 0) != (op2 < 0)) && ((op1 < 0) != (res < 0));

      if ((opCode == TR::ificmpo) || (opCode == TR::iflcmpo))
         s->conditionalToUnconditional(node, block, overflow);
      else
         s->conditionalToUnconditional(node, block, !overflow);
      }

   return node;
   }

TR::Node *ifxcmnoSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if (removeIfToFollowingBlock(node, block, s) == NULL)
      return NULL;
   simplifyChildren(node, block, s);

   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR_ASSERT((opCode == TR::ificmno) || (opCode == TR::ificmnno) || (opCode == TR::iflcmno) || (opCode == TR::iflcmnno), "unsupported ifxcmno opcode");

   if (node->getFirstChild()->getOpCode().isLoadConst() && node->getSecondChild()->getOpCode().isLoadConst())
      {
      int64_t op1 = node->getFirstChild()->get64bitIntegralValue();
      int64_t op2 = node->getSecondChild()->get64bitIntegralValue();
      int64_t res = ((opCode == TR::iflcmno) || (opCode == TR::iflcmnno)) ? op1 + op2 : (int32_t)(op1 + op2);
      bool overflow = ((op1 < 0) == (op2 < 0)) && ((op1 < 0) != (res < 0));

      if ((opCode == TR::ificmno) || (opCode == TR::iflcmno))
         s->conditionalToUnconditional(node, block, overflow);
      else
         s->conditionalToUnconditional(node, block, !overflow);
      }

   return node;
   }

//---------------------------------------------------------------------
// Lookup switch
//
TR::Node *lookupSwitchSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return switchSimplifier(node, block, false, s);
   }

//---------------------------------------------------------------------
// Table switch
//
TR::Node *tableSwitchSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return switchSimplifier(node, block, true, s);
   }

//---------------------------------------------------------------------
// nullchk operator
//
TR::Node *nullchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR::ILOpCodes nullCheckRefOp = node->getNullCheckReference()->getOpCodeValue();
   if (nullCheckRefOp == TR::New       ||
       nullCheckRefOp == TR::newarray  ||
       nullCheckRefOp == TR::anewarray ||
       nullCheckRefOp == TR::multianewarray)
      {
      TR::Node::recreate(node, TR::treetop);
      simplifyChildren(node, block, s);
      return node;
      }
   simplifyChildren(node, block, s);

   TR::Compilation *comp = TR::comp();
   if (node->getFirstChild()->getNumChildren() == 0)
      {
      dumpOptDetails(s->comp(), "%sRemoving nullchk with no grandchildren in node [%s]\n", s->optDetailString(), node->getName(s->getDebug()));
      TR::Node::recreate(node, TR::treetop);
      s->_alteredBlock = true;
      }
   else
      {
      TR::Node * refNode = node->getNullCheckReference();

      if (refNode->isNonNull() && performTransformation(s->comp(), "%sRemoving redundant NULLCHK in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         TR::Node::recreate(node, TR::treetop);

      if ((refNode->isNull() ||
          ((refNode->getOpCodeValue() == TR::aconst) &&
      (refNode->getAddress() == 0))) &&
          performTransformation(s->comp(), "%sRemoving rest of the block past a NULLCHK that will fail [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         removeRestOfBlock(s->_curTree, s->comp());

         // Terminate the block with a "return" opcode.
         //
         TR::Node * retNode = TR::Node::create(s->_curTree->getNode(), TR::Return, 0);
         TR::TreeTop::create(s->comp(), s->_curTree, retNode);

          // Remove the regular CFG edges out of the block.
          // If there is already an edge to the exit block leave it there. Otherwise
          // create one.
          //
          bool edgeExistsToEnd = false;
          TR::CFG * cfg = s->comp()->getFlowGraph();
          for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
             {
             if ((*edge)->getTo() == cfg->getEnd())
                {
                edgeExistsToEnd = true;
                break;
                }
             }

          if (!edgeExistsToEnd)
             cfg->addEdge(block, cfg->getEnd());

          // The following loop has seen the function removeEdge
          // end up removing more than one edge in the successorList.
          // Subsequently, if we iterate over the original list, we will end up
          // with an iterator invalidation problem and hence, use a copy instead.

          TR::CFGEdgeList list(block->getSuccessors());
          for (auto edge = list.begin(); edge != list.end(); ++edge)
             {
             if ((*edge)->getTo() != cfg->getEnd())
                s->_blockRemoved |= cfg->removeEdge(*edge);
             }
          }
       }
   return node;
   }

//---------------------------------------------------------------------
// Divide-by-zero check
//
TR::Node *divchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   // Remember the original child of the divchk. If simplification of this
   // child causes it to be replaced by another node, the divchk is no longer
   // needed.
   // The child of the divchk may no longer be a divide node, due to commoning.
   // In this case the divchk is no longer needed.
   //
   TR::Node *originalChild = node->getFirstChild();
   /////TR::ILOpCodes originalChildOpCode = originalChild->getOpCode().getOpCodeValue();
   /////TR_ASSERT(originalChild->getVisitCount() != s->comp()->getVisitCount(),"Simplifier, bad divchk node");
   TR::Node * child = originalChild;
   if (originalChild->getVisitCount() != s->comp()->getVisitCount())
      child = s->simplify(originalChild, block);

   if (child != originalChild ||
       !(child->getOpCode().isDiv() || child->getOpCode().isRem()))
      {
      TR::Node::recreate(node, TR::treetop);
      node->setFirst(child);
      return node;
      }

   // If the divisor is a non-zero constant, this check is redundant and
   // can be removed.
   //
   TR::Node * divisor = child->getSecondChild();
   if (divisor->getOpCode().isLoadConst())
      {
      if (((divisor->getOpCode().isLong() && divisor->getLongInt() != 0) ||
          ((!divisor->getOpCode().isLong()) && divisor->getInt() != 0)) &&
         performTransformation(s->comp(), "%sRemoved divchk with constant non-zero divisor in node[%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         //s->removeNode(node, s->_curTree);
         //return NULL;
         TR::Node::recreate(node, TR::treetop);
         return node;
         }
      }

   return node;
   }

//---------------------------------------------------------------------
// Bound check
//
TR::Node *bndchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node     * boundChild = node->getFirstChild();
   TR::Node     * indexChild = node->getSecondChild();
   TR::ILOpCodes indexOp    = indexChild->getOpCodeValue();
   TR::ILOpCodes boundOp    = boundChild->getOpCodeValue();

   bool removableBoundCheck = false;
   if ((boundOp == TR::iconst) &&
       (boundOp == indexOp) &&
       (boundChild->getInt() > indexChild->getInt()) &&
       (indexChild->getInt() >= 0))
      removableBoundCheck = true;

   // if the bndchk is checking whether the result of an irem is in an array
   // where the length of the array was used as the denominator of the remainder
   // then the bndchk is guaranteed not to trap, so can remove it.   This code
   // tends to come from hash table implementations that use arrays of buckets
   if (removableBoundCheck ||
       (indexOp == TR::irem && indexChild->getFirstChild()->isNonNegative() && boundChild == indexChild->getSecondChild()))
      {
      if (removableBoundCheck)
         {
         if (performTransformation(s->comp(), "%sRemoved bndchk with constant arguments in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            s->removeNode(node, s->_curTree);
            return NULL;
            }
         }
      else
         {
         if (performTransformation(s->comp(), "%sRemoved bndchk with irem with arraylength as denominator in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            s->removeNode(node, s->_curTree);
            return NULL;
            }
         }
      }

   // Simplify the following
   //    BNDCHK
   //      imul
   //        tree-1
   //        iconst k1
   //      imul
   //        tree-2
   //        iconst k2
   //
   // which may result as a consequence of the array-length-simplification optimization.
   // In this case we are guaranteed that neither of the multiplications was going to
   // overflow, so we can safely remove them
   //
   // if the index child is a mul-high, the above transformations may not be valid
   //
   if ((boundChild->getOpCode().isMul() && !isNodeMulHigh(boundChild) &&
            boundChild->getSecondChild()->getOpCode().isLoadConst()) &&
       (indexChild->getOpCode().isMul() && !isNodeMulHigh(indexChild) &&
            indexChild->getSecondChild()->getOpCode().isLoadConst()))
      {
      int32_t k1 = boundChild->getSecondChild()->getInt();
      int32_t k2 = indexChild->getSecondChild()->getInt();
      if (k1 == k2 && k1 > 0)
         {
         if (performTransformation(s->comp(), "%ssimplified algebra in BNDCHK [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            node->setAndIncChild(0, boundChild->getFirstChild());
            node->setAndIncChild(1, indexChild->getFirstChild());
            boundChild->recursivelyDecReferenceCount();
            indexChild->recursivelyDecReferenceCount();
            return node;
            }
         }
      }
   else if (boundChild->getOpCode().isLoadConst() &&
            (indexChild->getOpCode().isMul() && !isNodeMulHigh(indexChild) &&
                 indexChild->getSecondChild()->getOpCode().isLoadConst()))
      {
      int32_t k1 = boundChild->getInt();
      int32_t k2 = indexChild->getSecondChild()->getInt();
      if (k2 > 0 && ((k1 >= k2) && ((k1 % k2) == 0)) && performTransformation(s->comp(), "%ssimplified algebra in BNDCHK [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         if (boundChild->getReferenceCount() > 1)
            {
            node->setAndIncChild(0, TR::Node::create(node, TR::iconst, 0, k1/k2));
            boundChild->decReferenceCount();
            }
         else
            boundChild->setInt(k1/k2);
         node->setAndIncChild(1, indexChild->getFirstChild());
         indexChild->recursivelyDecReferenceCount();
         return node;
         }
      }

   return node;
   }

//---------------------------------------------------------------------
// Arraycopy bound check
//
TR::Node *arraycopybndchkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node     * lhsChild = node->getFirstChild();
   TR::Node     * rhsChild = node->getSecondChild();
   TR::ILOpCodes lhsOp    = lhsChild->getOpCodeValue();
   TR::ILOpCodes rhsOp    = rhsChild->getOpCodeValue();

   // Handle some special cases that are common
   // in arraycopies
   //
   bool removableBoundCheck = false;
   if ((lhsOp == TR::iconst) &&
       (lhsOp == rhsOp) &&
       (lhsChild->getInt() >= rhsChild->getInt()))
      removableBoundCheck = true;
   else if ((lhsChild == rhsChild) ||
       ((lhsChild->getNumChildren() == rhsChild->getNumChildren()) &&
        (lhsChild->getNumChildren() == 1) &&
        s->optimizer()->areNodesEquivalent(lhsChild, rhsChild) &&
       lhsChild->getFirstChild() == rhsChild->getFirstChild()))
      removableBoundCheck = true;
   else if ((rhsChild->getOpCode().isArrayLength()) &&
            (lhsOp == TR::imul) &&
            (lhsChild->getFirstChild() == rhsChild) &&
            lhsChild->getSecondChild()->getOpCode().isLoadConst() &&
            (lhsChild->getSecondChild()->getInt() > 0) &&
            (lhsChild->getSecondChild()->getInt() <= rhsChild->getArrayStride()))
      removableBoundCheck = true;

   if (removableBoundCheck)
      {
      if (performTransformation(s->comp(), "%sRemoved arraycopy bndchk node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         s->removeNode(node, s->_curTree);
         return NULL;
         }
      }

   if (rhsChild->getOpCodeValue() == TR::iadd)
      {
      TR::Node * boundChild  = lhsChild;
      TR::Node * lengthChild = rhsChild->getSecondChild();
      TR::Node * indexChild  = rhsChild->getFirstChild();
      if (lengthChild == boundChild || 
          s->isBoundDefinitelyGELength(boundChild, lengthChild))
         {
         if (indexChild->isZero())
            {
            if (performTransformation(s->comp(), "%sRemoved arraycopy bndchk with zero index in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
               {
               s->removeNode(node, s->_curTree);
               return NULL;
               }
            }
         if (indexChild->getOpCodeValue() == TR::isub)
            {
            if ((indexChild->getFirstChild() == boundChild ||
                 indexChild->getFirstChild() == lengthChild)                &&
                indexChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
                indexChild->getSecondChild()->getInt() == 1)
               {
               if (performTransformation(s->comp(), "%sRemoved arraycopy bndchk with len-1 index in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
                  {
                  s->removeNode(node, s->_curTree);
                  return NULL;
                  }
               }
            }
         else if (indexChild->getOpCodeValue() == TR::iadd)
            {
            if ((indexChild->getFirstChild() == boundChild ||
                 indexChild->getFirstChild() == lengthChild)                &&
                indexChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
                indexChild->getSecondChild()->getInt() == -1)
               {
               if (performTransformation(s->comp(), "%sRemoved arraycopy bndchk with len-1 index in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
                  {
                  s->removeNode(node, s->_curTree);
                  return NULL;
                  }
               }
            }
         else if (indexChild->getOpCode().isArrayLength())
            {
            if (boundChild->getOpCodeValue() == TR::iadd &&
                (boundChild->getFirstChild() == indexChild ||
                 boundChild->getSecondChild() == indexChild))
               {
               if (performTransformation(s->comp(), "%sRemoved arraycopy bndchk with arrayLength index in node [%s] when bound is sum of length and index arraylengths\n", s->optDetailString(), node->getName(s->getDebug())))
                  {
                  s->removeNode(node, s->_curTree);
                  return NULL;
                  }
               }
            }
         }
      }

   // Simplify the following
   //    ArrayCopyBNDCHK
   //      imul
   //        tree-1
   //        iconst k1
   //      imul
   //        tree-2
   //        iconst k2
   //
   // which may result as a consequence of the array-length-simplification optimization.
   // In this case we are guaranteed that neither of the multiplications was going to
   // overflow, so we can safely remove them
   //
   if ((lhsChild->getOpCode().isMul() && lhsChild->getSecondChild()->getOpCode().isLoadConst()) &&
       (rhsChild->getOpCode().isMul() && rhsChild->getSecondChild()->getOpCode().isLoadConst()))
      {
      int32_t k1 = lhsChild->getSecondChild()->getInt();
      int32_t k2 = rhsChild->getSecondChild()->getInt();
      if (k1 == k2 && k1 > 0 && performTransformation(s->comp(), "%ssimplified algebra in BNDCHK [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         node->setAndIncChild(0, lhsChild->getFirstChild());
         node->setAndIncChild(1, rhsChild->getFirstChild());
         lhsChild->recursivelyDecReferenceCount();
         rhsChild->recursivelyDecReferenceCount();
         return node;
         }
      }

   return node;
   }

//---------------------------------------------------------------------
// Bound check with spine check
//
TR::Node *bndchkwithspinechkSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   int32_t boundChildNum = 2;
   int32_t indexChildNum = 3;

   TR::Node     *boundChild = node->getChild(boundChildNum);
   TR::Node     *indexChild = node->getChild(indexChildNum);
   TR::ILOpCodes indexOp = indexChild->getOpCodeValue();
   TR::ILOpCodes boundOp = boundChild->getOpCodeValue();

   bool removeBoundCheck = false;
   bool removeSpineCheck = false;

   if (boundChild->getOpCode().isLoadConst() && indexChild->getOpCode().isLoadConst() &&
       (boundChild->getInt() > indexChild->getInt()) &&
       (indexChild->getInt() >= 0))
      {
      if (performTransformation(s->comp(), "%sRemoved bndchk with constant arguments in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         removeBoundCheck = true;
         }
      }
   else if (indexOp == TR::irem && indexChild->getFirstChild()->isNonNegative() && boundChild == indexChild->getSecondChild())
      {
      // If the bndchk is checking whether the result of an irem is in an array
      // where the length of the array was used as the denominator of the remainder
      // then the bndchk is guaranteed not to trap, so can remove it.   This code
      // tends to come from hash table implementations that use arrays of buckets
      //
      if (performTransformation(s->comp(), "%sRemoved bndchk with irem with arraylength as denominator in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         removeBoundCheck = true;
         }
      }

   if (boundChild->getOpCode().isLoadConst() && !s->comp()->generateArraylets())
      {
      // We need to determine the element size in order to prove the spine
      // check is not necessary.  Derive it from the array element node.
      //
      TR::DataType dt = node->getFirstChild()->getDataType();

      int32_t elementSize = (dt == TR::Address) ?
                              TR::Compiler->om.sizeofReferenceField() :
                              TR::Symbol::convertTypeToSize(dt);

      if (elementSize > 0 && !TR::Compiler->om.isDiscontiguousArray(boundChild->getInt(), elementSize) &&
          performTransformation(s->comp(),
             "%sRemoving spine check because constant arraylength is contiguous in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         removeSpineCheck = true;
         }
      }

   TR::Node *adjustRefCountOnElementNode = NULL;

   if (removeSpineCheck)
      {
      // We can remove the spine check but we can't remove the load or store
      // of the array element.
      //
      TR::Node *elementChild = node->getChild(0);
      TR::Node *treeTopNode;

      if (elementChild->getOpCode().isStore())
         {
         treeTopNode = elementChild;

         // Bump the reference count up to keep the node alive while other
         // nodes are being removed, but drop it back down later so that the
         // treetop node does not end up with a non-zero reference count.
         //
         elementChild->incReferenceCount();
         adjustRefCountOnElementNode = elementChild;
         }
      else
         {
         treeTopNode = TR::Node::create(TR::treetop, 1, elementChild);
         }

      TR::TreeTop *newTree = TR::TreeTop::create(s->comp(), treeTopNode);
      TR::TreeTop *prevTree = s->_curTree;
      newTree->join(prevTree->getNextTreeTop());
      prevTree->join(newTree);

      if (s->trace())
         {
         traceMsg(s->comp(), "removing spine check from node %p, anchoring element child to %p\n", node, treeTopNode);
         }
      }

   if (removeBoundCheck && removeSpineCheck)
      {
      s->removeNode(node, s->_curTree);

      if (adjustRefCountOnElementNode)
         adjustRefCountOnElementNode->decReferenceCount();

      return NULL;
      }
   else if (removeBoundCheck)
      {
      // Convert the check into a spine check
      //
      TR::Node *elementChild = node->getChild(0);
      TR::Node *baseChild = node->getChild(1);

      elementChild->incReferenceCount();
      indexChild->incReferenceCount();
      baseChild->incReferenceCount();

      s->prepareToReplaceNode(node);
      TR::Node::recreate(node, TR::SpineCHK);
      node->setChild(0, elementChild);
      node->setChild(1, baseChild);
      node->setChild(2, indexChild);
      node->setNumChildren(3);
      return node;
      }
   else if (removeSpineCheck)
      {
      // Convert the check into a bound check
      //
      indexChild->incReferenceCount();
      boundChild->incReferenceCount();

      s->prepareToReplaceNode(node);
      TR::Node::recreate(node, TR::BNDCHK);
      node->setChild(0, boundChild);
      node->setChild(1, indexChild);
      node->setNumChildren(2);

      if (adjustRefCountOnElementNode)
         adjustRefCountOnElementNode->decReferenceCount();

      return node;
      }

   // Simplify the following
   //    BNDCHK
   //      imul
   //        tree-1
   //        iconst k1
   //      imul
   //        tree-2
   //        iconst k2
   //
   // which may result as a consequence of the array-length-simplification optimization.
   // In this case we are guaranteed that neither of the multiplications was going to
   // overflow, so we can safely remove them
   //
   // if the index child is a mul-high, the above transformations may not be valid
   //
   if ((boundChild->getOpCode().isMul() && !isNodeMulHigh(boundChild) &&
        boundChild->getSecondChild()->getOpCode().isLoadConst()) &&
       (indexChild->getOpCode().isMul() && !isNodeMulHigh(indexChild) &&
        indexChild->getSecondChild()->getOpCode().isLoadConst()))
      {
      int32_t k1 = boundChild->getSecondChild()->getInt();
      int32_t k2 = indexChild->getSecondChild()->getInt();
      if (k1 == k2 && k1 > 0)
         {
         if (performTransformation(s->comp(), "%ssimplified algebra in BNDCHK [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            node->setAndIncChild(boundChildNum, boundChild->getFirstChild());
            node->setAndIncChild(indexChildNum, indexChild->getFirstChild());
            boundChild->recursivelyDecReferenceCount();
            indexChild->recursivelyDecReferenceCount();
            return node;
            }
         }
      }
   else if (boundChild->getOpCode().isLoadConst() &&
            (indexChild->getOpCode().isMul() && !isNodeMulHigh(indexChild) &&
             indexChild->getSecondChild()->getOpCode().isLoadConst()))
      {
      int32_t k1 = boundChild->getInt();
      int32_t k2 = indexChild->getSecondChild()->getInt();
      if (k2 > 0 && ((k1 >= k2) && ((k1 % k2) == 0)) && performTransformation(s->comp(), "%ssimplified algebra in BNDCHK [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
         {
         if (boundChild->getReferenceCount() > 1)
            {
            node->setAndIncChild(boundChildNum, TR::Node::create(node, TR::iconst, 0, k1/k2));
            boundChild->decReferenceCount();
            }
         else
            boundChild->setInt(k1/k2);
         node->setAndIncChild(indexChildNum, indexChild->getFirstChild());
         indexChild->recursivelyDecReferenceCount();
         return node;
         }
      }

   return node;
   }

// -------------------------------------------------------------------
// xxcmp simplifiers: bcmp, bucmp, scmp, sucmp, icmp, iucmp, lucmp
// -------------------------------------------------------------------
TR::Node *bcmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, Byte);
   return node;
   }

TR::Node *bucmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, UnsignedByte);
   return node;
   }

TR::Node *scmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, ShortInt);
   return node;
   }

TR::Node *sucmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, UnsignedShortInt);
   return node;
   }

TR::Node *icmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, Int);
   return node;
   }

TR::Node *iucmpSimplifier(TR::Node *node, TR::Block * block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, UnsignedInt);
   return node;
   }

TR::Node *lucmpSimplifier(TR::Node *node, TR::Block *block, TR::Simplifier * s)
   {
   XXCMP_SIMPLIFIER(node, block, s, UnsignedLongInt);
   return node;
   }

TR::Node *imaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   int32_t i = 0;
   int32_t num_children = node->getNumChildren();
   int32_t min = 0, max = 0;
   uint32_t umin = 0, umax = 0;
   bool is_all_const = true;
   bool has_const = false;
   int32_t numNonConst = 0;
   bool is_signed  = node->getOpCodeValue() == TR::imax || node->getOpCodeValue() == TR::imin;
   bool max_opcode = node->getOpCodeValue() == TR::imax || node->getOpCodeValue() == TR::iumax;
   TR::Node * constNode = NULL;

   for (i = 0; i < num_children; i++)
      {
      if (is_signed && node->getChild(i)->getOpCode().isLoadConst())
         {
         min = node->getChild(i)->getInt();
         max = node->getChild(i)->getInt();
         has_const=true;
         break;
         }
      else if (node->getChild(i)->getOpCode().isLoadConst())
         {
         umin = node->getChild(i)->getUnsignedInt();
         umax = node->getChild(i)->getUnsignedInt();
         has_const=true;
         break;
         }
      }

   if (has_const)
      {
      for (i = 0; i < num_children; i++)
         {
         if (is_signed && node->getChild(i)->getOpCode().isLoadConst())
            {
            if (node->getChild(i)->getInt() < min)
               min = node->getChild(i)->getInt();
            if (node->getChild(i)->getInt() > max)
               max = node->getChild(i)->getInt();
            constNode=node->getChild(i);
            }
         else if (node->getChild(i)->getOpCode().isLoadConst())
            {
            if (node->getChild(i)->getUnsignedInt() < umin)
               umin = node->getChild(i)->getUnsignedInt();
            if (node->getChild(i)->getUnsignedInt() > umax)
               umax = node->getChild(i)->getUnsignedInt();
            constNode=node->getChild(i);
            }
         else
            {
            is_all_const = false;
            node->setChild(numNonConst, node->getChild(i));
            numNonConst++;
            }
         }

      if (is_all_const)
         {
         if (is_signed)
            foldIntConstant(node, max_opcode ? max : min, s, false /* !anchorChildren*/);
         else
            foldUIntConstant(node, max_opcode ? umax : umin, s, false /* !anchorChildren*/);
         }
      else
         {
         if (is_signed)
            constNode->setInt(max_opcode ? max : min);
         else
            constNode->setUnsignedInt(max_opcode ? umax : umin);
         node->setChild(numNonConst, constNode);
         node->setNumChildren(numNonConst+1);
         }

      }

   return node;
   }

// Used for lmax, lumax, lmin, lumin
TR::Node *lmaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   int32_t i;
   bool has_const = false;
   bool is_all_const = true;
   int32_t numNonConst = 0;
   int32_t num_children = node->getNumChildren();
   int64_t min = 0, max = 0;
   uint64_t umin = 0, umax = 0;
   bool is_signed = node->getOpCodeValue() == TR::lmax || node->getOpCodeValue() == TR::lmin;
   bool max_opcode = node->getOpCodeValue() == TR::lmax || node->getOpCodeValue() == TR::lumax;
   TR::Node *constNode = NULL;

   for (i = 0; i < num_children; i++)
      {
      if (is_signed && node->getChild(i)->getOpCode().isLoadConst())
         {
         min = node->getChild(i)->getLongInt();
         max = node->getChild(i)->getLongInt();
         has_const=true;
         break;
         }
      else if (node->getChild(i)->getOpCode().isLoadConst())
         {
         umin = node->getChild(i)->getUnsignedLongInt();
         umax = node->getChild(i)->getUnsignedLongInt();
         has_const=true;
         break;
         }
      }

   if (has_const)
      {
      for (i = 0; i < num_children; i++)
         {
         if (is_signed && node->getChild(i)->getOpCode().isLoadConst())
            {
            if (node->getChild(i)->getLongInt() < min)
               min = node->getChild(i)->getLongInt();
            if (node->getChild(i)->getLongInt() > max)
               max = node->getChild(i)->getLongInt();
            constNode = node->getChild(i);
            }
         else if (node->getChild(i)->getOpCode().isLoadConst())
            {
            if (node->getChild(i)->getUnsignedLongInt() < umin)
               umin = node->getChild(i)->getUnsignedLongInt();
            if (node->getChild(i)->getUnsignedLongInt() > umax)
               umax = node->getChild(i)->getUnsignedLongInt();
            constNode = node->getChild(i);
            }
         else
            {
            is_all_const = false;
            node->setChild(numNonConst, node->getChild(i));
            numNonConst++;
            }
         }
      if (is_all_const)
         {
         if (is_signed)
            foldLongIntConstant(node, max_opcode ? max : min, s, false /* !anchorChildren */);
         else
            foldUnsignedLongIntConstant(node, max_opcode ? umax : umin, s, false /* !anchorChildren */);
         }
      else
         {
         if (is_signed)
            constNode->setLongInt(max_opcode ? max : min);
         else
            constNode->setUnsignedLongInt(max_opcode ? umax : umin);

         node->setChild(numNonConst, constNode);
         node->setNumChildren(numNonConst+1);
         }
      }

   return node;
   }

TR::Node *fmaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   int32_t i;
   int32_t num_children = node->getNumChildren();
   bool has_const = false;
   bool is_all_const = true;
   int32_t numNonConst = 0;
   float fmin = 0, fmax = 0;
   uint32_t fmini = 0, fmaxi = 0;
   bool max_opcode = node->getOpCodeValue() == TR::fmax;
   TR::Node *constNode = NULL;

   for (i = 0; i < num_children; i++)
      {
      if (node->getChild(i)->getOpCode().isLoadConst())
         {
         static const char *jiagblah = feGetEnv("TR_JIAGTypeAssumes");
         if(jiagblah)
            TR_ASSERT(0, "Should never be considering non float children in fmaxmin simplifier!\n");

         fmin = node->getChild(i)->getFloat();
         fmax = fmin;
         fmaxi = fmini = node->getChild(i)->getFloatBits();
         has_const=true;
         break;
         }
      }

   if (has_const)
      {
      for (i=0; i < num_children; i++)
         {
         if (node->getChild(i)->getOpCode().isLoadConst())
            {
            if (node->getChild(i)->getFloat() < fmin)
               fmin = node->getChild(i)->getFloat();
            if (node->getChild(i)->getFloat() > fmax)
               fmax = node->getChild(i)->getFloat();
            constNode=node->getChild(i);
            }
         else
            {
            is_all_const=false;
            node->setChild(numNonConst, node->getChild(i));
            numNonConst++;
            }
         }
      if (is_all_const)
         {
         foldFloatConstant(node, max_opcode ? fmax : fmin, s);
         }
      else
         {
         constNode->setFloat(max_opcode ? fmax : fmin);
         node->setChild(numNonConst, constNode);
         node->setNumChildren(numNonConst+1);
         }
      }

   return node;
   }

TR::Node *dmaxminSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   int32_t i;
   int32_t num_children = node->getNumChildren();
   double min, max;
   uint64_t mini, maxi;
   bool has_const = false;
   bool is_all_const = true;
   int32_t numNonConst = 0;
   TR::Node * constNode = NULL;
   bool max_opcode = node->getOpCodeValue() == TR::dmax;

   for (i = 0; i < num_children; i++)
      {
      if (node->getChild(i)->getOpCode().isLoadConst())
         {
         min = node->getChild(i)->getDouble();
         max = min;
         maxi = mini = node->getChild(i)->getUnsignedLongInt();
         has_const=true;
         break;
         }
      }

   if (has_const)
      {
      for (i = 0; i < num_children; i++)
         {
         if (node->getChild(i)->getOpCode().isLoadConst())
            {
            if (node->getChild(i)->getDouble() < min)
               min = node->getChild(i)->getDouble();
            if (node->getChild(i)->getDouble() > max)
               max = node->getChild(i)->getDouble();
            constNode = node->getChild(i);
            }
         else
            {
            is_all_const=false;
            node->setChild(numNonConst, node->getChild(i));
            numNonConst++;
            }
         }
      if (is_all_const)
         {
         foldDoubleConstant(node, max_opcode ? max : min, s);
         }
      else
         {
         constNode->setDouble(max_opcode ? max : min);
         node->setChild(numNonConst, constNode);
         node->setNumChildren(numNonConst+1);
         }
      }

   return node;
   }

TR::Node *computeCCSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   TR_ASSERT(node->getNumChildren() == 1, "computeCC node should only have 1 child");

   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCode& ccOriginalChildOp = firstChild->getOpCode();

   simplifyChildren(node, block, s);

   OMR::TR_ConditionCodeNumber cc = s->getCC(firstChild);
   if (cc != OMR::ConditionCodeInvalid)
      {
      TR_ASSERT((uint8_t)cc < 4, "assertion failure");
      foldUByteConstant(node, (uint8_t)cc, s, true);
      }
   else
      {
      TR_ASSERT(ccOriginalChildOp.getOpCodeValue() == firstChild->getOpCodeValue(),
              "computeCC's child node's opcode was changed to a different opcode" );
      }

   // NOTE: we allow simplification of anchored computeCC nodes, so MUST return the same
   // node that was passed in
   return node;
   }

TR::Node * arraysetSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   // Reduce Int64 fill to lower size fill - this generates much more efficient code on z, and there
   // is no reason not to do this if elem child has repeating pattern.
   auto fill = node->getChild(1);
   auto len = node->getChild(2);
   if (fill->getOpCode().isLoadConst() && fill->getDataType() == TR::Int64)
      {
      uint64_t fillVal = fill->getConst<uint64_t>();
      if ((fillVal & 0x0FFFFFFFFL) == ((fillVal >> 32) & 0x0FFFFFFFFL) &&
          performTransformation(s->comp(), "%sTransform large fill arrayset to 4byte fill arrayset [" POINTER_PRINTF_FORMAT "]\n",
                s->optDetailString(), node))
         {
         node->setAndIncChild(1, TR::Node::iconst((int32_t)(fillVal & 0x0FFFFFFFFL)));
         fill->recursivelyDecReferenceCount();
         }
      }
   return node;
   }

//---------------------------------------------------------------------
// bitOpMem simplification
//
TR::Node *bitOpMemSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   if (s->comp()->getOptions()->getOption(TR_ScalarizeSSOps))
      {
      }
   return node;
   }

//---------------------------------------------------------------------
// bitOpMemND simplification
//
TR::Node *bitOpMemNDSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   return node;
   }

// arraycmpWithPad simplifier
//
TR::Node *arrayCmpWithPadSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);


   return node;
   }

TR::Node *NewSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);
   return node;
   }

// Used by MethodEnter/ExitHook
TR::Node *lowerTreeSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   if(node->getOpCodeValue() == TR::MethodExitHook || node->getOpCodeValue() == TR::MethodEnterHook)
      {
      s->_performLowerTreeNodePairs.push_back(std::make_pair(s->_curTree, node));
      return node;
      }
   else
      {
      return postWalkLowerTreeSimplifier(s->_curTree,node, block,s);
      }
   }

// Simplification of arrayLength operator
TR::Node *arrayLengthSimplifier(TR::Node * node, TR::Block * block, TR::Simplifier * s)
   {
   simplifyChildren(node, block, s);

   TR::Node *firstChild = node->getFirstChild();
   TR::ILOpCodes firstChildOp = firstChild->getOpCodeValue();

   // If the length of the array is unknown we can't simply propagate the
   // expression calculating it because contiguous arrays have range limits
   // that other nodes (e.g., BNDCHKwithSpineCHKs) depend on.
   //
   // Non-hybrid TR::arraylength and TR_discontigarraylengths are ok.
   //
   if (node->getOpCodeValue() == TR::contigarraylength)
      {
      if (firstChildOp == TR::newarray || firstChildOp == TR::anewarray)
         {
         TR::Node * arraySize = firstChild->getFirstChild();

         int32_t elementSize = TR::Compiler->om.getSizeOfArrayElement(firstChild);

         if (arraySize->getOpCode().isLoadConst() &&
             elementSize > 0 &&
             !TR::Compiler->om.isDiscontiguousArray(arraySize->getInt(), elementSize) &&
             performTransformation(s->comp(),
                "%sReducing contiguous arraylength of newarray or anewarray in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
            {
            arraySize->incReferenceCount();
            firstChild->setVisitCount(0);
            node->setVisitCount(0);
            node->recursivelyDecReferenceCount();
            s->_alteredBlock = true;
            return arraySize;
            }
         else
            {
            return node;
            }
         }
      }

   // the first child of a newarray or anewarray is the same as the arrayLength of that array
   if ((firstChildOp == TR::newarray || firstChildOp == TR::anewarray) &&
       performTransformation(s->comp(), "%sReducing arraylength of newarray or anewarray in node [%s]\n", s->optDetailString(), node->getName(s->getDebug())))
      {
      TR::Node * arraySize = firstChild->getFirstChild();
      arraySize->incReferenceCount();
      firstChild->setVisitCount(0);
      node->setVisitCount(0);
      node->recursivelyDecReferenceCount();
      s->_alteredBlock = true;
      return arraySize;
      }
   return node;
   }


#endif
