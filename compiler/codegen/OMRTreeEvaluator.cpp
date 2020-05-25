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

#include "codegen/TreeEvaluator.hpp"

#include <stdint.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Register.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include "infra/TreeServices.hpp"
#include "ras/Debug.hpp"

TR::Register *
OMR::TreeEvaluator::brdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The direct rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getFirstChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the load operation to the load evaluator.
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::brdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The indirect rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node (first child), so we delegate
   // evaluation to the load evaluator (which will evaluate the first child as well).
   return TR::TreeEvaluator::bloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::srdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The direct rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getFirstChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the load operation to the load evaluator.
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::srdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The indirect rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node (first child), so we delegate
   // evaluation to the load evaluator (which will evaluate the first child as well).
   return TR::TreeEvaluator::sloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::irdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The direct rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getFirstChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the load operation to the load evaluator.
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::irdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The indirect rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node (first child), so we delegate
   // evaluation to the load evaluator (which will evaluate the first child as well).
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::lrdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The direct rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getFirstChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the load operation to the load evaluator.
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::lrdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The indirect rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node (first child), so we delegate
   // evaluation to the load evaluator (which will evaluate the first child as well).
   return TR::TreeEvaluator::lloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::ardbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The direct rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getFirstChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the load operation to the load evaluator.
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::ardbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The indirect rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node (first child), so we delegate
   // evaluation to the load evaluator (which will evaluate the first child as well).
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::frdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The direct rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getFirstChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the load operation to the load evaluator.
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::frdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The indirect rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node (first child), so we delegate
   // evaluation to the load evaluator (which will evaluate the first child as well).
   return TR::TreeEvaluator::floadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::drdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The direct rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getFirstChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the load operation to the load evaluator.
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::drdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The indirect rdbar IL represents a load with side effects.
   // Currently we don't use the side effect node (first child), so we delegate
   // evaluation to the load evaluator (which will evaluate the first child as well).
   return TR::TreeEvaluator::dloadEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::bwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::bwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::bstoreEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::swrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::swrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::sstoreEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::iwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::iwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::istoreEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::lwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

TR::Register *
OMR::TreeEvaluator::lwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::lstoreEvaluator(node, cg);
   }

bool OMR::TreeEvaluator::isStaticClassSymRef(TR::SymbolReference * symRef)
   {
   return (symRef && symRef->getSymbol() && symRef->getSymbol()->isStatic() && symRef->getSymbol()->isClassObject());
   }

// unresolved casts or casts to things other than abstract or interface benefit from
// an equality test
bool OMR::TreeEvaluator::instanceOfOrCheckCastNeedEqualityTest(TR::Node * node, TR::CodeGenerator *cg)
   {
   TR::Node            *castClassNode    = node->getSecondChild();
   TR::SymbolReference *castClassSymRef  = castClassNode->getSymbolReference();

   if (!TR::TreeEvaluator::isStaticClassSymRef(castClassSymRef))
      {
      return true;
      }

   TR::StaticSymbol    *castClassSym     = castClassSymRef->getSymbol()->getStaticSymbol();

   if (castClassSymRef->isUnresolved())
      {
      return false;
      }
   else
      {
      TR_OpaqueClassBlock * clazz;

      if (castClassSym
          && (clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress())
          && !TR::Compiler->cls.isInterfaceClass(cg->comp(), clazz)
          && (
              !TR::Compiler->cls.isAbstractClass(cg->comp(), clazz)

              // here be dragons
              // int.class, char.class, etc are final & abstract
              // usually instanceOf calls on these classes are ripped out by the optimizer
              // but in some cases they can persist to codegen which without the following
              // case causes assertions because we opt out of calling the helper and doing
              // all inline tests. Really we could just jmp to the failed side, but to reduce
              // service risk we are going to do an equality test that we know will fail
              // NOTE final abstract is not enough - all array classes are final abstract
              //      to prevent them being used with new and being extended...
              || (TR::Compiler->cls.isAbstractClass(cg->comp(), clazz) && TR::Compiler->cls.isClassFinal(cg->comp(), clazz)
                  && TR::Compiler->cls.isPrimitiveClass(cg->comp(), clazz)))
         )
         return true;
      }

   return false;
   }


// resolved casts that are not to abstract, interface, or array need a super test
bool OMR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(TR::Node * node, TR::CodeGenerator *cg)
   {
   TR::Node            *castClassNode    = node->getSecondChild();
   TR::MethodSymbol    *helperSym        = node->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *castClassSymRef  = castClassNode->getSymbolReference();

   if (!TR::TreeEvaluator::isStaticClassSymRef(castClassSymRef))
      {
      // We could theoretically do a super test on something with no sym, but it would require significant
      // changes to platform code. The benefit is little at this point (shows up from reference arraycopy reductions)

      if (cg->supportsInliningOfIsInstance() &&
          node->getOpCodeValue() == TR::instanceof &&
          node->getSecondChild()->getOpCodeValue() != TR::loadaddr)
         return true;
      else
         return false;
      }

   TR::StaticSymbol    *castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();

   if (castClassSymRef->isUnresolved())
      {
      return false;
      }
   else
      {
      TR_OpaqueClassBlock * clazz;
      // If the class is a regular class (i.e., not an interface nor an array) and
      // not known to be a final class, an inline superclass test can be generated.
      // If the helper does not preserve all the registers there will not be
      // enough registers to do the superclass test inline.
      // Also, don't generate the superclass test if optimizing for space.
      //
      if (castClassSym &&
          (clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress()) &&
          !TR::Compiler->cls.isClassArray(cg->comp(), clazz) &&
          !TR::Compiler->cls.isInterfaceClass(cg->comp(), clazz) &&
          !TR::Compiler->cls.isClassFinal(cg->comp(), clazz) &&
           helperSym->preservesAllRegisters() &&
          !cg->comp()->getOption(TR_OptimizeForSpace))
         return true;
      }
   return false;
   }

TR_GlobalRegisterNumber
OMR::TreeEvaluator::getHighGlobalRegisterNumberIfAny(TR::Node *node, TR::CodeGenerator *cg)
   {
    //No need for register pairs in 64-bit mode
    if (cg->comp()->target().is64Bit())
        return -1;

    //if the node itself doesn't have a type (e.g passthrough) we assume it has a child with a type
    //However we keep track of the initial node as it will contain the register information.
    TR::Node *rootNode = node;

    while (node->getType() == TR::NoType) {
        node = node->getFirstChild();
        TR_ASSERT(node, "The node should always be valid while looking for a Child with a type");
    }
    TR_ASSERT(node->getType() != TR::NoType, "Expecting node %p, to have a specific type here", node);

    //Only need a register pair if the node is a 64bit Int
    return node->getType().isInt64() ? rootNode->getHighGlobalRegisterNumber() : -1;
   }

// Returns n where value = 2^n.
//   If value != 2^n, returns -1
int32_t
OMR::TreeEvaluator::checkNonNegativePowerOfTwo(int32_t value)
   {
   if (isNonNegativePowerOf2(value))
      {
      int32_t shiftAmount = 0;
      while ((value = ((uint32_t) value) >> 1))
         {
         ++shiftAmount;
         }
      return shiftAmount;
      }
   else
      {
      return -1;
      }
   }

// (Same as other checkNonNegativePowerOfTwo above.)
int32_t
OMR::TreeEvaluator::checkNonNegativePowerOfTwo(int64_t value)
   {
   if (isNonNegativePowerOf2(value))
      {
      int32_t shiftAmount = 0;
      while ((value = ((uint64_t) value) >> 1))
         {
         ++shiftAmount;
         }
      return shiftAmount;
      }
   else
      {
      return -1;
      }
   }

// Returns n where value = 2^n or -2^n.
//   If value != 2^n or -2^n, returns -1
int32_t OMR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(int32_t value)
   {
   if (isNonNegativePowerOf2(value))
      {
      int32_t shiftAmount = 0;
      while ((value = ((uint32_t) value) >> 1))
         {
         ++shiftAmount;
         }
      return shiftAmount;
      }
   else if (isNonPositivePowerOf2(value))
      {
      int32_t shiftAmount = 0;
      value = -value;
      while ((value = ((uint32_t) value) >> 1))
         {
         ++shiftAmount;
         }
      return shiftAmount;
      }
   else
      {
      return -1;
      }
   }

// (Same as other checkPositiveOrNegativePowerOfTwo above.)
int32_t OMR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(int64_t value)
   {
   if (isNonNegativePowerOf2(value))
      {
      int32_t shiftAmount = 0;
      while ((value = ((uint64_t) value) >> 1))
         {
         ++shiftAmount;
         }
      return shiftAmount;
      }
   else if (isNonPositivePowerOf2(value))
      {
      int32_t shiftAmount = 0;
      value = -value;
      while ((value = ((uint64_t) value) >> 1))
         {
         ++shiftAmount;
         }
      return shiftAmount;
      }
   else
      {
      return -1;
      }
   }

TR::Register *
OMR::TreeEvaluator::compressedRefsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *loadOrStoreNode = node->getFirstChild();

   // for indirect stores, check if the value child has been
   // evaluated already. in this case the store has already
   // been evaluated (possibly under a check)
   //
   bool mustBeEvaluated = true;
   if (loadOrStoreNode->getOpCode().isStoreIndirect())
      {
      if (loadOrStoreNode->isStoreAlreadyEvaluated())
         mustBeEvaluated = false;
      }

   if (mustBeEvaluated)
      {
      TR::Register *loadOrStoreRegister = cg->evaluate(loadOrStoreNode);
      if (loadOrStoreNode->getOpCode().isStoreIndirect())
         loadOrStoreNode->setStoreAlreadyEvaluated(true);
      }

   cg->decReferenceCount(loadOrStoreNode);
   cg->decReferenceCount(node->getSecondChild());
   return 0;
   }


TR::Register *
OMR::TreeEvaluator::computeCCEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp= cg->comp();
   TR::Node *child = node->getFirstChild();

   if (child->getRegister())
      {
      // set up for re-evaluate
      // reduce reference count to indicate not using present evaluation
      cg->decReferenceCount(child);

      // if reference count is zero, this is last use so can reuse present child
      // otherwise need to clone to prevent conflict with live register.
      // ref counts of children of clone/original were already inc during tree lowering
      // so do not need to increase them here.
      if (child->getReferenceCount() > 0)
         child = TR::Node::copy(child);

      child->setReferenceCount(1);
      child->setRegister(NULL);
      }
   else
      {
      // has not been evaluated before, as there were no non CC references prior to this one
      // evaluate as normal, so add result is available for future non CC uses
      // we need to decrease reference counts on children of virtual clone which were raised in
      // tree lowering to allow children's registers to be kept alive to this point.
      for (int32_t i = child->getNumChildren()-1; i >= 0; --i)
         child->getChild(i)->decReferenceCount();
      }
   // set flag so CC must be calculated, doesn't need to be reset as won't be re-evaluated until another computeCC
   child->setNodeRequiresConditionCodes(true);
   TR::Register *result = cg->evaluate(child);

   // do not call child->setRegister(result) - there is no register for the computeCC, no point setting it.
   cg->decReferenceCount(child);
   return result;
   }


TR::Register *OMR::TreeEvaluator::unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL(false, "Opcode %s is not implemented\n", node->getOpCode().getName());
   return NULL;
   }

TR::Register *OMR::TreeEvaluator::badILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL(false, "badILOp %s cannot be evaluated\n", node->getOpCode().getName());
   return NULL;
   }

bool OMR::TreeEvaluator::nodeIsIArithmeticOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u)
   {
   return TR::TreeEvaluator::nodeIsIAddOverflowCheck(node, u) || TR::TreeEvaluator::nodeIsISubOverflowCheck(node, u);
   }

bool OMR::TreeEvaluator::nodeIsIAddOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u)
   {
   TR::Compilation *comp= TR::comp();
   if (comp->getOption(TR_DisableTreePatternMatching))
      return false;

   TR_PatternBuilder pb(comp);
   static TR_Pattern *pattern =
      pb.choice(pb(TR::ificmpge), pb(TR::ificmplt), pb.children(
         pb(TR::iand,
            pb(TR::ixor,
               pb(TR::iadd, u,u->operationNode,
                  pb(u,u->leftChild),
                  pb(u,u->rightChild)),
               pb(u,u->leftChild)
            ),
            pb(TR::ixor,
               pb(u,u->operationNode),
               pb(u,u->rightChild)
            )
         ),
         pb.iconst(0)
      ));
   return pattern->matches(node, (TR::Node**)u, comp);
   }

bool OMR::TreeEvaluator::nodeIsISubOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u)
   {
   TR::Compilation *comp= TR::comp();
   if (comp->getOption(TR_DisableTreePatternMatching))
      return false;

   TR_PatternBuilder pb(comp);
   static TR_Pattern *pattern =
      pb.choice(pb(TR::ificmpge), pb(TR::ificmplt), pb.children(
         pb(TR::iand,
            pb(TR::ixor,
               pb(TR::isub, u,u->operationNode,
                  pb(u,u->leftChild),
                  pb(u,u->rightChild)),
               pb(u,u->leftChild)
            ),
            pb(TR::ixor,
               pb(u,u->leftChild),
               pb(u,u->rightChild)
            )
         ),
         pb.iconst(0)
      ));
   return pattern->matches(node, (TR::Node**)u, comp);
   }

bool OMR::TreeEvaluator::nodeIsLArithmeticOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u)
   {
   return TR::TreeEvaluator::nodeIsLAddOverflowCheck(node, u) || TR::TreeEvaluator::nodeIsLSubOverflowCheck(node, u);
   }

bool OMR::TreeEvaluator::nodeIsLAddOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u)
   {
   TR::Compilation *comp= TR::comp();
   if (comp->getOption(TR_DisableTreePatternMatching))
      return false;

   TR_PatternBuilder pb(comp);
   static TR_Pattern *pattern =
      pb.choice(pb(TR::iflcmpge), pb(TR::iflcmplt), pb.children(
         pb(TR::land,
            pb(TR::lxor,
               pb(TR::ladd, u,u->operationNode,
                  pb(u,u->leftChild),
                  pb(u,u->rightChild)),
               pb(u,u->leftChild)
            ),
            pb(TR::lxor,
               pb(u,u->operationNode),
               pb(u,u->rightChild)
            )
         ),
         pb.lconst(0)
      ));
   return pattern->matches(node, (TR::Node**)u, comp);
   }

bool OMR::TreeEvaluator::nodeIsLSubOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u)
   {
   TR::Compilation *comp= TR::comp();
   if (comp->getOption(TR_DisableTreePatternMatching))
      return false;

   TR_PatternBuilder pb(comp);
   static TR_Pattern *pattern =
      pb.choice(pb(TR::iflcmpge), pb(TR::iflcmplt), pb.children(
         pb(TR::land,
            pb(TR::lxor,
               pb(TR::lsub, u,u->operationNode,
                  pb(u,u->leftChild),
                  pb(u,u->rightChild)),
               pb(u,u->leftChild)
            ),
            pb(TR::lxor,
               pb(u,u->leftChild),
               pb(u,u->rightChild)
            )
         ),
         pb.lconst(0)
      ));
   return pattern->matches(node, (TR::Node**)u, comp);
   }

// FIXME: this seems to be derived from OutOfLineCodeSection::evaluateNodesWithFutureUses
// however, it seems to something extra for the spinechk nodes. These two routines should
// be consolidated
//
void OMR::TreeEvaluator::evaluateNodesWithFutureUses(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp= cg->comp();
   if (node->getRegister() != NULL)
      {
      // Node has already been evaluated outside this tree.
      //
      return;
      }

   if (node->getFutureUseCount() > 0)
      {
      TR::Node *actualLoadOrStoreChild = node;

      while (actualLoadOrStoreChild->getOpCode().isConversion() ||
         actualLoadOrStoreChild->chkCompressionSequence())
         actualLoadOrStoreChild = actualLoadOrStoreChild->getFirstChild();

      if (actualLoadOrStoreChild->getOpCode().isStore() ||
         actualLoadOrStoreChild->getOpCode().isLoadConst() ||
         actualLoadOrStoreChild->getOpCode().isArrayRef() ||
         (actualLoadOrStoreChild->getOpCode().isLoad() &&
         actualLoadOrStoreChild->getSymbolReference() &&
            (actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayShadowSymbol() ||
            actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayletShadowSymbol())))
         {
         // These types of nodes are likey specific to one path or another and may cause
         // a failure if evaluated on a common path.
         //
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "avoiding escaping commoned subtree %p [RealLoad/Store: %p], but processing its children: node is ", node, actualLoadOrStoreChild);
            if (actualLoadOrStoreChild->getOpCode().isStore())
               traceMsg(comp, "store\n");
            else if (actualLoadOrStoreChild->getOpCode().isLoadConst())
               traceMsg(comp, "const\n");
            else if (actualLoadOrStoreChild->getOpCode().isArrayRef())
               traceMsg(comp, "arrayref (aiadd/aladd)\n");
            else if (actualLoadOrStoreChild->getOpCode().isLoad() && actualLoadOrStoreChild->getSymbolReference())
               {
               if (actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
                  traceMsg(comp, "array shadow\n");
               else if (actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayletShadowSymbol())
                  traceMsg(comp, "arraylet shadow\n");
               }
            }
         }
      else
         {
         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "O^O pre-evaluating escaping commoned subtree %p\n", node);

         (void)cg->evaluate(node);

         // Do not decrement the reference count here.  It will be decremented when the call node is evaluated
         // again in the helper instruction stream.
         //

         return;
         }
      }

   for (int32_t i = 0; i<node->getNumChildren(); i++)
      TR::TreeEvaluator::evaluateNodesWithFutureUses(node->getChild(i), cg);
   }

// This routine detects every subtree S of a node N that meets the following criteria:
//    (1) the first reference to S is in a subtree of N, and
//    (2) not all references to S appear under N
// This is very similar to the futureUseCount concept for TR::Node, except that this one
// talks about the future uses *beyond* the treeTop on which this is called.
// Note that this routine only make sense during tree evaluation time. It assumes
// that all previous (strictly historical) uses have already been discounted from
// the nodes' reference counts
//
void OMR::TreeEvaluator::initializeStrictlyFutureUseCounts(TR::Node *node, vcount_t visitCount, TR::CodeGenerator *cg)
   {
   if (node->getRegister() != NULL)
      {
      // Node has already been evaluated outside this tree.
      //
      return;
      }

   if (node->getVisitCount() != visitCount)
      {
      // First time this node has been encountered in this tree.
      //
      node->setFutureUseCount(node->getReferenceCount());
      node->setVisitCount(visitCount);

      for (int32_t i = 0; i<node->getNumChildren(); i++)
         TR::TreeEvaluator::initializeStrictlyFutureUseCounts(node->getChild(i), visitCount, cg);
      }

   if (node->getReferenceCount() > 0)
      node->decFutureUseCount();

   }
