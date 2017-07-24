/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <algorithm>                            // for std::max, etc
#include <stdint.h>                             // for int32_t, int64_t, etc
#include <stdio.h>                              // for printf, fflush, etc
#include <string.h>                             // for NULL, strncmp, etc
#include "codegen/CodeGenerator.hpp"            // for CodeGenerator
#include "codegen/FrontEnd.hpp"                 // for TR_FrontEnd, etc
#include "env/KnownObjectTable.hpp"         // for KnownObjectTable, etc
#include "codegen/RecognizedMethods.hpp"        // for RecognizedMethod, etc
#include "codegen/InstOpCode.hpp"               // for InstOpCode
#include "compile/Compilation.hpp"              // for Compilation, comp
#include "compile/Method.hpp"                   // for TR_Method, etc
#include "compile/ResolvedMethod.hpp"           // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"     // for SymbolReferenceTable
#include "compile/VirtualGuard.hpp"             // for TR_VirtualGuard
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"          // for TR::Options, etc
#include "cs2/bitvectr.h"                       // for ABitVector<>::Cursor
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                           // for POINTER_PRINTF_FORMAT
#include "env/ObjectModel.hpp"                  // for ObjectModel
#include "env/PersistentInfo.hpp"               // for PersistentInfo
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#ifdef J9_PROJECT_SPECIFIC
#include "env/VMAccessCriticalSection.hpp"      // for VMAccessCriticalSection
#endif
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                         // for Block, etc
#include "il/DataTypes.hpp"                     // for DataTypes, etc
#include "il/ILOpCodes.hpp"                     // for ILOpCodes::treetop, etc
#include "il/ILOps.hpp"                         // for ILOpCode, etc
#include "il/Node.hpp"                          // for Node, etc
#include "il/Node_inlines.hpp"                  // for Node::getFirstChild, etc
#include "il/Symbol.hpp"                        // for Symbol, etc
#include "il/SymbolReference.hpp"               // for SymbolReference, etc
#include "il/TreeTop.hpp"                       // for TreeTop
#include "il/TreeTop_inlines.hpp"               // for TreeTop::getNode, etc
#include "il/symbol/AutomaticSymbol.hpp"        // for AutomaticSymbol
#include "il/symbol/MethodSymbol.hpp"           // for MethodSymbol, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"   // for ResolvedMethodSymbol
#include "il/symbol/StaticSymbol.hpp"           // for StaticSymbol
#include "infra/Array.hpp"                      // for TR_Array
#include "infra/Assert.hpp"                     // for TR_ASSERT
#include "infra/Bit.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"                       // for TR_LinkHead, TR_Pair
#include "infra/List.hpp"                       // for ListIterator, etc
#include "infra/SimpleRegex.hpp"
#include "infra/TRCfgEdge.hpp"                  // for CFGEdge
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"    // for OptimizationManager
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"              // for Optimizer
#include "optimizer/PreExistence.hpp"           // for TR_PrexArgInfo, etc
#include "optimizer/TransformUtil.hpp"
#include "optimizer/UseDefInfo.hpp"             // for TR_UseDefInfo, etc
#include "optimizer/VPConstraint.hpp"           // for TR::VPConstraint, etc
#include "optimizer/OMRValuePropagation.hpp"       // for OMR::ValuePropagation, etc
#include "ras/Debug.hpp"                        // for TR_Debug
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#include "env/PersistentCHTable.hpp"            // for TR_PersistentCHTable
#include "runtime/RuntimeAssumptions.hpp"
#include "env/VMJ9.h"
#include "optimizer/VPBCDConstraint.hpp"        // for VP BCD constraint specifics
#endif

#define OPT_DETAILS "O^O VALUE PROPAGATION: "

extern TR::Node *constrainChildren(OMR::ValuePropagation *vp, TR::Node *node);
extern TR::Node *constrainVcall(OMR::ValuePropagation *vp, TR::Node *node);
extern void createGuardSiteForRemovedGuard(TR::Compilation *comp, TR::Node* ifNode);

static void checkForNonNegativeAndOverflowProperties(OMR::ValuePropagation *vp, TR::Node *node, TR::VPConstraint *constraint = NULL)
   {
   if (!constraint)
      {
      bool isGlobal;
      constraint = vp->getConstraint(node, isGlobal);
      }

   if (node->getOpCode().isLoad())
      {
      node->setCannotOverflow(true);
      //dumpOptDetails(vp->comp(), "Load node %p cannot overflow\n", node);
      }

   if (constraint)
      {
      if (constraint->asIntConst())
         {
         int32_t low = constraint->asIntConst()->getLowInt();
         if (low >= 0)
            node->setIsNonNegative(true);
         if (low <= 0)
            node->setIsNonPositive(true);
         }
      if (constraint->asLongConst())
         {
         int64_t low = constraint->asLongConst()->getLowLong();
         if (low >= 0)
            node->setIsNonNegative(true);
         if (low <= 0)
            node->setIsNonPositive(true);
         }
      if (constraint->asShortConst())
         {
         int16_t low = constraint->asShortConst()->getLowShort();
         if (low >= 0)
             node->setIsNonNegative(true);
         if (low <= 0)
             node->setIsNonPositive(true);
         }
      if (constraint->asIntRange())
         {
         TR::VPIntRange *range = constraint->asIntRange();
         int32_t low = range->getLowInt();
         if (low >= 0)
            node->setIsNonNegative(true);

         int32_t high = range->getHighInt();
         if (high <= 0)
            node->setIsNonPositive(true);

         ///if ((node->getOpCode().isArithmetic() || node->getOpCode().isLoad()) &&
         ///      ((low > TR::getMinSigned<TR::Int32>()) || (high < TR::getMaxSigned<TR::Int32>())))
         if ((node->getOpCode().isLoad() &&
               ((low > TR::getMinSigned<TR::Int32>()) || (high < TR::getMaxSigned<TR::Int32>()))) ||
               (node->getOpCode().isArithmetic() &&
                (range->canOverflow() != TR_yes)))
            {
            node->setCannotOverflow(true);
            //dumpOptDetails(vp->comp(), "Node %p cannot overflow\n", node);
            }
         }
      else if (constraint->asLongRange())
         {
         TR::VPLongRange *range = constraint->asLongRange();
         int64_t low = range->getLowLong();
         if (low >= 0)
            node->setIsNonNegative(true);

         int64_t high = range->getHighLong();
         if (high <= 0)
            node->setIsNonPositive(true);

         ///if ((node->getOpCode().isArithmetic() || node->getOpCode().isLoad()) &&
         ///      ((low > TR::getMinSigned<TR::Int64>()) || (high < TR::getMaxSigned<TR::Int64>())))
         if ((node->getOpCode().isLoad() &&
                  ((low > TR::getMinSigned<TR::Int64>()) || (high < TR::getMaxSigned<TR::Int64>()))) ||
               (node->getOpCode().isArithmetic() &&
                (range->canOverflow() != TR_yes)))
            {
            node->setCannotOverflow(true);
            //dumpOptDetails(vp->comp(), "Node %p cannot overflow\n", node);
            }
         }
      else if (constraint->asShortRange())
         {
         TR::VPShortRange *range = constraint->asShortRange();

         int16_t low = range->getLowShort();
         if (low >= 0)
             node->setIsNonNegative(true);

         int16_t high = range->getHighShort();
         if (high <= 0)
             node->setIsNonPositive(true);

         if ((node->getOpCode().isLoad() && ((low > TR::getMinSigned<TR::Int16>()) || (high < TR::getMaxSigned<TR::Int16>())) ||
             (node->getOpCode().isArithmetic() && (range->canOverflow() != TR_yes))))
              {
              node->setCannotOverflow(true);
              }

         }
      }
   }


static bool isBoolean(TR::VPConstraint *constraint)
   {
   if (constraint)
      {
      if (constraint->asIntConst())
         {
         int32_t low = constraint->asIntConst()->getLowInt();
         if ((low >= 0) && (low <= 1))
            return true;
         }
      if (constraint->asLongConst())
         {
         int64_t low = constraint->asLongConst()->getLowLong();
         if ((low >= 0) && (low <= 1))
            return true;
         }

      if (constraint->asShortConst())
         {
         int16_t low = constraint->asShortConst()->getLowShort();
         if ((low >= 0) && (low <= 1))
             return true;
         }

     if (constraint->asIntRange())
         {
         TR::VPIntRange *range = constraint->asIntRange();
         int32_t low = range->getLowInt();
         int32_t high = range->getHighInt();
         if ((low >= 0) && (high <= 1))
            return true;
         }
      else if (constraint->asLongRange())
         {
         TR::VPLongRange *range = constraint->asLongRange();
         int64_t low = range->getLowLong();
         int64_t high = range->getHighLong();
         if ((low >= 0) && (high <= 1))
            return true;
         }
      else if (constraint->asShortRange())
         {
         TR::VPShortRange *range = constraint->asShortRange();
         int16_t low  = range->getLowShort();
         int16_t high = range->getHighShort();
         if ((low >= 0) && (high <= 1))
             return true;
         }
      }
   return false;
   }



static int32_t arrayElementSize(const char *signature, int32_t len, TR::Node *node, OMR::ValuePropagation *vp)
   {
   if (signature[0] != '[')
      return 0;

   // regular array element size
   if (signature[0] == '[')
      {
      switch (signature[1])
         {
         case 'B': return 1;
         case 'C':
         case 'S': return 2;
         case 'I':
         case 'F': return 4;
         case 'D':
         case 'J': return 8;
         case 'Z': return TR::Compiler->om.elementSizeOfBooleanArray();
         case 'L':
         default :
                   return TR::Compiler->om.sizeofReferenceField();
         }
      }

   return 0;
   }



static void constrainBaseObjectOfIndirectAccess(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return;  // 84287 - disabled for now.

   //if (!debug("enableBaseConstraint"))
   //   return;

   int32_t baseObjectSigLen;
   TR_ResolvedMethod *method = node->getSymbolReference()->getOwningMethod(vp->comp());
   char *baseObjectSig = method->classNameOfFieldOrStatic(node->getSymbolReference()->getCPIndex(), baseObjectSigLen);
   if (baseObjectSig)
      baseObjectSig = classNameToSignature(baseObjectSig, baseObjectSigLen, vp->comp());
   if (baseObjectSig)
      {
      TR_OpaqueClassBlock *baseClass = vp->fe()->getClassFromSignature(baseObjectSig, baseObjectSigLen, method);
      if (  baseClass
         && TR::Compiler->cls.isInterfaceClass(vp->comp(), baseClass)
         && !vp->comp()->getOption(TR_TrustAllInterfaceTypeInfo))
         {
         baseClass = NULL;
         }

      if (baseClass)
         {
         // check if there is an existing class constraint
         // before trying to refine the constraint
         bool isGlobal;
         TR::VPConstraint *baseConstraint = vp->getConstraint(node->getFirstChild(), isGlobal);
         TR::VPConstraint *constraint = TR::VPClassType::create(vp, baseObjectSig, baseObjectSigLen, method, false, baseClass);
         // only if its a class constraint
         if (baseConstraint && baseConstraint->getClassType())
            {
            if (vp->trace())
               {
               traceMsg(vp->comp(), "Found existing class constraint for node (%p) :\n", node->getFirstChild());
               baseConstraint->print(vp->comp(), vp->comp()->getOutFile());
               }
            // check if it can be refined
            if (!constraint->intersect(baseConstraint, vp))
               return; // nope, bail
            }
         vp->addBlockConstraint(node->getFirstChild(), constraint);
         }
      }
   }

// When node is successfully folded, isGlobal is set to true iff all
// constraints in the chain were global, and false otherwise.
static bool tryFoldCompileTimeLoad(
   OMR::ValuePropagation *vp,
   TR::Node *node,
   bool &isGlobal)
   {
   isGlobal = true;

   if (!node->getOpCode().isLoad())
      return false;
   else if (node->getOpCode().isLoadReg())
      return false;
   else if (node->getSymbolReference()->isUnresolved())
      return false;

   if (node->getOpCode().isIndirect())
      {
      if (vp->trace())
         traceMsg(vp->comp(), "  constrainCompileTimeLoad inspecting %s %p\n", node->getOpCode().getName(), node);

      TR::KnownObjectTable *knot = vp->comp()->getKnownObjectTable();
      TR::Node *baseExpression = NULL;
      TR::KnownObjectTable::Index baseKnownObject = TR::KnownObjectTable::UNKNOWN;
      for (TR::Node *curNode = node->getFirstChild(); !baseExpression; curNode = curNode->getFirstChild())
         {
         bool curIsGlobal;
         TR::VPConstraint *constraint = vp->getConstraint(curNode, curIsGlobal);
         if (!curIsGlobal)
            isGlobal = false;
         if (!constraint)
            {
            if (vp->trace())
               traceMsg(vp->comp(), "  - FAIL: %s %p has no constraint\n", curNode->getOpCode().getName(), curNode);
            break;
            }
         else if (curNode->getOpCode().hasSymbolReference() && curNode->getSymbolReference()->isUnresolved())
            {
            if (vp->trace())
               traceMsg(vp->comp(), "  - FAIL: %s %p is unresolved\n", curNode->getOpCode().getName(), curNode);
            break;
            }
         else if (constraint->getKnownObject())
            {
            baseExpression  = curNode;
            baseKnownObject = constraint->getKnownObject()->getIndex();
            if (vp->trace())
               traceMsg(vp->comp(), "  - %s %p is obj%d\n", baseExpression->getOpCode().getName(), baseExpression, baseKnownObject);
            break;
            }
         else if (knot && constraint->isConstString())
            {
            baseExpression  = curNode;
            baseKnownObject = knot->getIndexAt((uintptrj_t*)constraint->getConstString()->getSymRef()->getSymbol()->castToStaticSymbol()->getStaticAddress());
            if (vp->trace())
               traceMsg(vp->comp(), "  - %s %p is string obj%d\n", baseExpression->getOpCode().getName(), baseExpression, baseKnownObject);
            break;
            }
         else if (constraint->isJ9ClassObject() == TR_yes
                  && constraint->isNonNullObject()
                  && constraint->getClassType()
                  && constraint->getClassType()->asFixedClass()
                  && constraint->getClass())
            {
            if (vp->trace())
               traceMsg(vp->comp(), " - %s %p is class object - transforming\n", curNode->getOpCode().getName(), curNode);
            TR::Node *nodeToRemove = NULL;
            uintptrj_t clazz = (uintptrj_t)constraint->getClass();
            bool didSomething = TR::TransformUtil::transformIndirectLoadChainAt(vp->comp(), node, curNode, &clazz, &nodeToRemove);
            if (nodeToRemove)
               {
               TR_ASSERT(didSomething, "How can there be a node to remove if transformIndirectLoadChain didn't do something??");
               vp->removeNode(nodeToRemove, true);
               return true;
               }
            return didSomething;
            }
         else if (!curNode->getOpCode().isLoadIndirect())
            {
            if (curNode->getOpCodeValue() == TR::loadaddr
                && curNode->getSymbolReference()->getSymbol()->isClassObject()
                && !curNode->getSymbolReference()->isUnresolved())
               {
               TR::Node *nodeToRemove = NULL;
               uintptrj_t clazz = (uintptrj_t)curNode->getSymbolReference()->getSymbol()->getStaticSymbol()->getStaticAddress();
               bool didSomething = TR::TransformUtil::transformIndirectLoadChainAt(vp->comp(), node, curNode, &clazz, &nodeToRemove);
               if (nodeToRemove)
                  {
                  TR_ASSERT(didSomething, "How can there be a node to remove if transformIndirectLoadChain didn't do something??");
                  vp->removeNode(nodeToRemove, true);
                  return true;
                  }
               return didSomething;
               }
            else
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "  - FAIL: %s %p is not an indirect load and has insufficient constraints\n", curNode->getOpCode().getName(), curNode);
               break;
               }
            }
#ifdef J9_PROJECT_SPECIFIC
         else if (vp->comp()->fej9()->isFinalFieldPointingAtJ9Class(curNode->getSymbolReference(), vp->comp()))
            {
            if (vp->trace())
               traceMsg(vp->comp(), " - FAIL: %s %p points to a j9class but has insufficient constraints\n", curNode->getOpCode().getName(), curNode);
            baseExpression = NULL;
            break;
            }
         else if (vp->comp()->fej9()->canDereferenceAtCompileTime(curNode->getSymbolReference(), vp->comp()))
            {
            // we can continue up the dereference chain
            if (vp->trace())
               traceMsg(vp->comp(), "  - %s %p is %s\n", curNode->getOpCode().getName(), curNode, curNode->getSymbolReference()->getName(vp->comp()->getDebug()));
            continue;
            }
#endif
         else
            {
            if (vp->trace())
               traceMsg(vp->comp(), "  - FAIL: %s %p is %s\n", curNode->getOpCode().getName(), curNode, curNode->getSymbolReference()->getName(vp->comp()->getDebug()));
            break;
            }
         }

      if (baseExpression)
         {
         // We've got something we can ask the front end to transform
         //
         TR::Node *nodeToRemove = NULL;
         bool didSomething = TR::TransformUtil::transformIndirectLoadChain(vp->comp(), node, baseExpression, baseKnownObject, &nodeToRemove);
         if (nodeToRemove)
            {
            TR_ASSERT(didSomething, "How can there be a node to remove if transformIndirectLoadChain didn't do something??");
            vp->removeNode(nodeToRemove, true);
            }
         return didSomething;
         }
      else
         {
         return false;
         }
      }
   else
      {
      // Do direct loads too
      return false;
      }

   TR_ASSERT(0, "Shouldn't get here");
   return true; // Safe default
   }

static bool constrainCompileTimeLoad(OMR::ValuePropagation *vp, TR::Node *node)
   {
   bool isGlobal;
   if (!tryFoldCompileTimeLoad(vp, node, isGlobal))
      return false;

   // Add a constraint so that VP can continue with the constant value.
   constrainNewlyFoldedConst(vp, node, isGlobal);
   return true;
   }

static bool findConstant(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // See if the node is already known to be a constant
   //
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node, isGlobal);
   if (constraint)
      {
      TR::DataType type = node->getDataType();
      switch (type)
         {
         case TR::Int64:
         case TR::Double:
            if (constraint->asLongConst())
               {
               bool replacedConst = false;
               if (!vp->cg()->materializesLargeConstants() ||
                   (!node->getType().isInt64()) ||
                   (constraint->asLongConst()->getLong() < vp->cg()->getSmallestPosConstThatMustBeMaterialized() &&
                    constraint->asLongConst()->getLong() > vp->cg()->getLargestNegConstThatMustBeMaterialized()) ||
                   (vp->getCurrentParent()->getOpCode().isMul() &&
                    (vp->getCurrentParent()->getSecondChild() == node) &&
                    isNonNegativePowerOf2(constraint->asLongConst()->getLong())))
                  {
                  vp->replaceByConstant(node, constraint, isGlobal);
                  replacedConst = true;
                  }

               if (constraint->getLowLong())
                  node->setIsNonZero(true);
               else
                  node->setIsZero(true);
               return replacedConst;
               }
            break;
         case TR::Address:
            if (constraint->isNullObject())
               {

               vp->replaceByConstant(node, constraint, isGlobal);
               node->setIsNull(true);
               return true;
               }
            else if (constraint->isNonNullObject())
               {
               node->setIsNonNull(true);
               if (constraint->getKnownObject())
                  {
                  TR::VPKnownObject *knownObject = constraint->getKnownObject();
                  // Don't do direct loads here till we get the aliasing right
                  if (node->getOpCode().isLoadIndirect() && !node->getSymbolReference()->hasKnownObjectIndex())
                     {
                     TR::KnownObjectTable *knot = vp->comp()->getKnownObjectTable();
                     TR_ASSERT(knot, "Can't have a known-object constraint without a known-object table");
                     TR::SymbolReference *improvedSymRef = vp->comp()->getSymRefTab()->findOrCreateSymRefWithKnownObject(node->getSymbolReference(), knot->getPointerLocation(knownObject->getIndex()));
                     if (improvedSymRef->hasKnownObjectIndex())
                        {
                        if (performTransformation(vp->comp(), "%sUsing known-object specific symref #%d for obj%d at [%p]\n", OPT_DETAILS, improvedSymRef->getReferenceNumber(), knownObject->getIndex(), node))
                           {
                           node->setSymbolReference(improvedSymRef);
                           return true;
                           }
                        }
                     }
                  }
               }
            break;
         default:
            if (constraint->asIntConstraint())
               {
               int32_t value = constraint->getLowInt();
               if (constraint->asIntConst())
                  {
                  bool replacedConst = false;
                  if (!vp->cg()->materializesLargeConstants() ||
                      (!node->getType().isInt32()) ||
                      (value < vp->cg()->getSmallestPosConstThatMustBeMaterialized() &&
                       value > vp->cg()->getLargestNegConstThatMustBeMaterialized()) ||
                      (vp->getCurrentParent()->getOpCode().isMul() &&
                      (vp->getCurrentParent()->getSecondChild() == node) &&
                      isNonNegativePowerOf2(value))
                      )
                     {
                     vp->replaceByConstant(node, constraint, isGlobal);
                     replacedConst = true;
                     }
                  if (value)
                     node->setIsNonZero(true);
                  else
                     node->setIsZero(true);
                  return replacedConst;
                  }
               else if (value >= 0)
                  node->setIsNonNegative(true);

               if (constraint->getHighInt() <= 0)
                  node->setIsNonPositive(true);

               if ((node->getOpCode().isArithmetic() || node->getOpCode().isLoad()) &&
                     ((value > TR::getMinSigned<TR::Int32>()) || (constraint->getHighInt() < TR::getMaxSigned<TR::Int32>())))
                  {
                  node->setCannotOverflow(true);
                  }
               }
            else if (constraint->asShortConstraint())
               {
                int16_t value = constraint->getLowShort();
                if (constraint->asShortConst())
                  {
                  bool replacedConst = false;
                  if (!vp->cg()->materializesLargeConstants() ||
                      (!node->getType().isInt16()) ||
                      (value < vp->cg()->getSmallestPosConstThatMustBeMaterialized() &&
                       value > vp->cg()->getLargestNegConstThatMustBeMaterialized()) ||
                      (vp->getCurrentParent()->getOpCode().isMul() &&
                       vp->getCurrentParent()->getSecondChild() == node) &&
                       isNonNegativePowerOf2(value))
                       {
                       vp->replaceByConstant(node,constraint,isGlobal);
                       replacedConst = true;
                       }
                    if (value)
                        node->setIsNonZero(true);
                    else
                        node->setIsZero(true);
                    return replacedConst;
                  }
                 else if (value >= 0)
                      node->setIsNonNegative(true);

                 if (constraint->getHighShort() <= 0)
                      node->setIsNonPositive(true);

                  if ((node->getOpCode().isArithmetic() || node->getOpCode().isLoad()) &&
                     ((value > TR::getMinSigned<TR::Int16>()) || (constraint->getHighShort() < TR::getMaxSigned<TR::Int16>())))
                     {
                      node->setCannotOverflow(true);
                     }
                 }
            break;
         }
      }
   return false;
   }

static bool containsUnsafeSymbolReference(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (vp->comp()->getSymRefTab()->findDLPStaticSymbolReference(node->getSymbolReference()) ||
       node->getSymbolReference()->isLitPoolReference())
      return true;

   if (node->getSymbolReference()->getSymbol()->isShadow())
      {
      if (node->getSymbol()->isUnsafeShadowSymbol())
         {
         if (vp->trace())
            traceMsg(vp->comp(), "Node [%p] has an unsafe symbol reference %d, no constraint\n", node,
                     node->getSymbolReference()->getReferenceNumber());
         return true;
         }


      if (node->getSymbolReference() == vp->getSymRefTab()->findInstanceShapeSymbolRef() ||
          node->getSymbolReference() == vp->getSymRefTab()->findInstanceDescriptionSymbolRef() ||
          node->getSymbolReference() == vp->getSymRefTab()->findDescriptionWordFromPtrSymbolRef() ||
#ifdef J9_PROJECT_SPECIFIC
          node->getSymbolReference() == vp->getSymRefTab()->findClassFromJavaLangClassAsPrimitiveSymbolRef() ||
#endif
          (node->getSymbolReference()->getSymbol() == vp->comp()->getSymRefTab()->findGenericIntShadowSymbol()))
         return true;
      }

   return false;
   }

static bool owningMethodDoesNotContainNullChecks(OMR::ValuePropagation *vp, TR::Node *node)
   {
   TR::ResolvedMethodSymbol *method = node->getSymbolReference()->getOwningMethodSymbol(vp->comp());
   if (method && method->skipNullChecks())
      return true;
   return false;
   }

bool constraintFitsInIntegerRange(OMR::ValuePropagation *vp, TR::VPConstraint *constraint)
   {
   if (constraint == NULL)
      return false;

   TR::VPLongConstraint *longConstraint = constraint->asLongConstraint();
   TR::VPIntConstraint *intConstraint = constraint->asIntConstraint();
   TR::VPShortConstraint *shortConstraint = constraint->asShortConstraint();

   if (longConstraint)
      {
      int64_t low = longConstraint->getLowLong(), high = longConstraint->getHighLong();
      if (low >= TR::getMinSigned<TR::Int32>() && high <= TR::getMaxSigned<TR::Int32>())
         return true;
      }
   else if (intConstraint || shortConstraint)
      return true;

   return false;
   }

bool canMoveLongOpChildDirectly(TR::Node *node, int32_t numChild, TR::Node *arithNode)
   {
   return node->getChild(numChild)->getDataType() == arithNode->getDataType() || node->getOpCodeValue() == TR::lshr && numChild > 0;
   }

bool reduceLongOpToIntegerOp(OMR::ValuePropagation *vp, TR::Node *node, TR::VPConstraint *nodeConstraint)
   {
   if (!constraintFitsInIntegerRange(vp, nodeConstraint))
      return false;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      bool isGlobal;
      TR::VPConstraint *childConstraint = vp->getConstraint(node->getChild(i), isGlobal);
      if (!constraintFitsInIntegerRange(vp, childConstraint))
         return false;
      }


   // Adding this check because it is likely suboptimal on machines where you can use long registers to convert things to integers and insert conversion trees.
   // Conversion trees are not free.
   // LoadExtensions (and other codegen peepholes) should in theory catch uneeded conversions (like widening) as described in the case below on 64 bit platforms.
   if (vp->comp()->cg()->supportsNativeLongOperations())
      return false;


   // The node and all children have constraints that fit in integer range.
   // lrem          becomes i2l
   //   i2l                   irem (new node)
   //     iload                 iload (get rid of the l2i)
   //   lconst                l2i (new node)
   //                           lconst
   TR::ILOpCodes newOp = TR::ILOpCode::integerOpForLongOp(node->getOpCodeValue());
   if (newOp == TR::BadILOp)
      return false;

   if (performTransformation(vp->comp(), "%sReduce %s (0x%p) to integer arithmetic\n",
                               OPT_DETAILS, node->getOpCode().getName(), node))
      {
      TR::Node *arithNode =  TR::Node::create(node, newOp, node->getNumChildren());

      for (int32_t i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *child = node->getChild(i);
         if (canMoveLongOpChildDirectly(node, i, arithNode))
            {
            arithNode->setAndIncChild(i, child);
            dumpOptDetails(vp->comp(), "    Transfer integer child %d %s (0x%p)\n", i, child->getOpCode().getName(), child);
            }
         else if (child->getOpCode().isConversion() && child->getFirstChild()->getDataType() == arithNode->getDataType())
            {
            if (child->getReferenceCount() > 1)
               vp->anchorNode(child, vp->_curTree);
            arithNode->setAndIncChild(i, child->getFirstChild());
            dumpOptDetails(vp->comp(), "    Replacing child %d %s (0x%p) with grandchild %s (0x%p)\n", i, child->getOpCode().getName(), child, child->getFirstChild()->getOpCode().getName(), child->getFirstChild());
            }
         else
            {
            TR::Node *newChild = TR::Node::create(node, TR::ILOpCode::getProperConversion(child->getDataType(), arithNode->getDataType(), false), 1);
            newChild->setAndIncChild(0, child);
            arithNode->setAndIncChild(i, newChild);
            dumpOptDetails(vp->comp(), "    Creating new %s (0x%p) above child %d %s (0x%p)\n", newChild->getOpCode().getName(), newChild, i, child->getOpCode().getName(), child);
            }
         }

      vp->prepareToReplaceNode(node, TR::ILOpCode::getProperConversion(arithNode->getDataType(), node->getDataType(), false));
      node->setNumChildren(1);
      node->setAndIncChild(0, arithNode);
      dumpOptDetails(vp->comp(), "  Changed (0x%p) to %s with new child %s (0x%p)\n", node, node->getOpCode().getName(), arithNode->getOpCode().getName(), arithNode);
      return true;
      }

   return false;
   }

static void constrainAConst(OMR::ValuePropagation *vp, TR::Node *node, bool isGlobal)
   {
   TR::VPConstraint *constraint = NULL;
   if (node->getAddress() == 0)
      {
      constraint = TR::VPNullObject::create(vp);
      node->setIsNull(true);
      }
   else
      {
      constraint = TR::VPNonNullObject::create(vp);
      node->setIsNonNull(true);
      if (node->isClassPointerConstant())
         {
         TR::VPConstraint *typeConstraint = TR::VPClass::create(vp, TR::VPFixedClass::create(vp, (TR_OpaqueClassBlock *) node->getAddress()),
                                                              NULL, NULL, NULL,
                                                              TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject));
         vp->addBlockOrGlobalConstraint(node, typeConstraint, isGlobal);
         }
      }
   vp->addBlockOrGlobalConstraint(node, constraint, isGlobal);
   }

TR::Node *constrainAConst(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainAConst(vp, node, true /* isGlobal */);
   return node;
   }

static void constrainIntAndFloatConstHelper(OMR::ValuePropagation *vp, TR::Node *node, int32_t value, bool isGlobal)
   {

   if (value)
      {
      node->setIsNonZero(true);
      if (value & TR::getMinSigned<TR::Int32>())
         node->setIsNonPositive(true);
      else
         node->setIsNonNegative(true);
      }
   else
      {
      node->setIsZero(true);
      node->setIsNonNegative(true);
      node->setIsNonPositive(true);
      }

   vp->addBlockOrGlobalConstraint(node, TR::VPIntConst::create(vp, value), isGlobal);
   }

static void constrainIntConst(OMR::ValuePropagation *vp, TR::Node *node, bool isGlobal)
   {
   int32_t value = node->getInt();
   constrainIntAndFloatConstHelper(vp, node, value, isGlobal);
   }

TR::Node *constrainIntConst(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainIntConst(vp, node, true /* isGlobal */);
   return node;
   }

TR::Node *constrainFloatConst(OMR::ValuePropagation *vp, TR::Node *node)
   {
   int32_t value = node->getFloatBits();
   constrainIntAndFloatConstHelper(vp, node, value, true /* isGlobal */);
   return node;
   }

static void constrainLongConst(OMR::ValuePropagation *vp, TR::Node *node, bool isGlobal)
   {
   int64_t value = node->getLongInt();
   if (value)
      {
      node->setIsNonZero(true);
      if (value & TR::getMinSigned<TR::Int64>())
         node->setIsNonPositive(true);
      else
         node->setIsNonNegative(true);
      }
   else
      {
      node->setIsZero(true);
      node->setIsNonNegative(true);
      node->setIsNonPositive(true);
      }

   vp->addBlockOrGlobalConstraint(node, TR::VPLongConst::create(vp, value), isGlobal);
   }

TR::Node *constrainLongConst(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainLongConst(vp, node, true /* isGlobal */);
   return node;
   }

TR::Node *constrainByteConst(OMR::ValuePropagation *vp, TR::Node *node)
   {
   int8_t value = node->getByte();
   if (value)
      {
      node->setIsNonZero(true);
      if (value & TR::getMinSigned<TR::Int8>())
         node->setIsNonPositive(true);
      else
         node->setIsNonNegative(true);
      }
   else
      {
      node->setIsZero(true);
      node->setIsNonNegative(true);
      node->setIsNonPositive(true);
      }

   // treat constants as always signed. its upto the consumer
   // to decide if a sign-extension or zero-extension (for unsigned
   // datatypes like c2i, bu2i) is needed
   // also donot generate a new global constraint at this point if
   // one is already available.
   //
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node, isGlobal);
   if (!constraint)
      vp->addGlobalConstraint(node, TR::VPIntConst::create(vp, value));
   return node;
   }

TR::Node *constrainShortConst(OMR::ValuePropagation *vp, TR::Node *node)
   {
   int16_t value = node->getShortInt();
   if (value)
      {
      node->setIsNonZero(true);
      if (value & TR::getMinSigned<TR::Int16>())
         node->setIsNonPositive(true);
      else
         node->setIsNonNegative(true);
      }
   else
      {
      node->setIsZero(true);
      node->setIsNonNegative(true);
      node->setIsNonPositive(true);
      }

   // see comments above for byteConst
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node, isGlobal);
   if (!constraint)
      vp->addGlobalConstraint(node, TR::VPShortConst::create(vp, value));
   return node;
   }


// constrainBCDSign tracks 3 ways a known sign can be retrieved from a node
// 1)    via a specific known sign tracked on the node itself (this could be for any type of BCD node)
// 2a)   for a setsign operation where the setsign value is on the node itself
// 2b)   for a setsign operation where the setsign value is a const child of the node
//
#ifdef J9_PROJECT_SPECIFIC
#define TR_ENABLE_BCD_SIGN (true)
TR::Node *constrainBCDSign(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (!TR_ENABLE_BCD_SIGN)
      return node;

   int32_t sign = TR::DataType::getInvalidSignCode();
   if (node->hasKnownSignCode())    // TODO: track assumed sign codes too? -- these are tracked on load operations already so not much benefit
      {
      TR_RawBCDSignCode rawSign = node->getKnownSignCode();
      sign = TR::DataType::getValue(rawSign);
      if (vp->trace())
         traceMsg(vp->comp(),"\tconstrainBCDSign from knownSign : %s (%p) sign %s (0x%x)\n",
            node->getOpCode().getName(),node,TR::DataType::getName(rawSign),sign);
      }
   else if (node->getOpCode().isSetSignOnNode())
      {
      TR_RawBCDSignCode rawSign = node->getSetSign();
      sign = TR::DataType::getValue(rawSign);
      if (vp->trace())
         traceMsg(vp->comp(),"\tconstrainBCDSign from setSignOnNode : %s (%p) sign %s (0x%x)\n",
            node->getOpCode().getName(),node,TR::DataType::getName(rawSign),sign);
      }
   else if (node->getOpCode().isSetSign())
      {
      TR::Node *setSignValue = node->getSetSignValueNode();
      if (setSignValue->getOpCode().isLoadConst() && setSignValue->getType().isIntegral() && setSignValue->getSize() <= 4)
         {
         sign = setSignValue->get32bitIntegralValue();
         if (vp->trace())
            traceMsg(vp->comp(),"\tconstrainBCDSign from setSignOp : %s (%p) sign 0x%x\n",
               node->getOpCode().getName(),node,sign);
         }
      }

   if (sign == TR::DataType::getInvalidSignCode() || sign == TR::DataType::getIgnoredSignCode()) // if no specific sign code see if a clean or preferred sign state can be tracked
      {
      TR_BCDSignConstraint constraintType = TR_Sign_Unknown;
      if (node->hasKnownCleanSign())
         constraintType = TR_Sign_Clean;
      else if (node->hasKnownPreferredSign())
         constraintType = TR_Sign_Preferred;

      if (constraintType != TR_Sign_Unknown)
         {
         if (vp->trace())
            traceMsg(vp->comp(),"\tnode %s (%p) got clean or preferred constraintType %s\n",node->getOpCode().getName(),node,TR::VP_BCDSign::getName(constraintType));
         vp->addGlobalConstraint(node, TR::VP_BCDSign::create(vp, constraintType, node->getDataType()));
         }
      }
   else
      {
      TR_BCDSignCode normalizedSign = TR::DataType::getNormalizedSignCode(node->getDataType(), sign);
      TR_BCDSignConstraint constraintType = TR::VP_BCDSign::getSignConstraintFromBCDSign(normalizedSign);

      if (vp->trace())
         traceMsg(vp->comp(),"\tnode %s (%p) got constraintType %s for sign 0x%x\n",node->getOpCode().getName(),node,TR::VP_BCDSign::getName(constraintType),sign);

      if (constraintType == TR_Sign_Minus && node->hasKnownCleanSign())
         {
         if (vp->trace())
            traceMsg(vp->comp(),"\tpromote constraintType %s->%s as node %s (%p) is clean\n",
               TR::VP_BCDSign::getName(constraintType),TR::VP_BCDSign::getName(TR_Sign_Minus_Clean),node->getOpCode().getName(),node);
         constraintType = TR_Sign_Minus_Clean;
         }

      if (constraintType != TR_Sign_Unknown)
         vp->addGlobalConstraint(node, TR::VP_BCDSign::create(vp, constraintType, node->getDataType()));
      }

   return node;
   }
#endif

TR::Node *constrainBCDAggrLoad(OMR::ValuePropagation *vp, TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::Node *parent = vp->getCurrentParent();

   if (findConstant(vp, node))
      return node;

   constrainChildren(vp, node);

   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node, isGlobal);
   TR::VP_BCDSign *signConstraint = constraint ? constraint->asBCDSign() : NULL;
   if (signConstraint &&
       TR_ENABLE_BCD_SIGN &&
       signConstraint->getDataType() == node->getDataType())
      {
      switch (signConstraint->getSign())
         {
         case TR_Sign_Clean:
            if (!node->hasKnownCleanSign() &&
                performTransformation(vp->comp(), "%sTransfer sign constraint %s to %s (0x%p)\n",
                  OPT_DETAILS, signConstraint->getName(), node->getOpCode().getName(),node))
               {
               if (vp->comp()->cg()->traceBCDCodeGen())
                  traceMsg(vp->comp(),"y^y: VP: Transfer sign constraint %s to %s (0x%p)\n",
                     signConstraint->getName(), node->getOpCode().getName(),node);
               node->setHasKnownCleanSign(true);
               }
            break;
         case TR_Sign_Preferred:
            if (!node->hasKnownPreferredSign() &&
                performTransformation(vp->comp(), "%sTransfer sign constraint %s to %s (0x%p)\n",
                  OPT_DETAILS, signConstraint->getName(), node->getOpCode().getName(),node))
               {
               if (vp->comp()->cg()->traceBCDCodeGen())
                  traceMsg(vp->comp(),"y^y: VP: Transfer sign constraint %s to %s (0x%p)\n",
                     signConstraint->getName(), node->getOpCode().getName(),node);
               node->setHasKnownPreferredSign(true);
               }
            break;
         case TR_Sign_Plus:
            if (!node->knownSignCodeIs(bcd_plus) &&
                performTransformation(vp->comp(), "%sTransfer sign constraint %s to %s (0x%p)\n",
                  OPT_DETAILS, signConstraint->getName(), node->getOpCode().getName(),node))
               {
               if (vp->comp()->cg()->traceBCDCodeGen())
                  traceMsg(vp->comp(),"y^y: VP: Transfer sign constraint %s to %s (0x%p)\n",
                     signConstraint->getName(), node->getOpCode().getName(),node);
               node->setKnownSignCode(bcd_plus);
               }
            break;
         case TR_Sign_Minus:
            if (!node->knownSignCodeIs(bcd_minus) &&
                performTransformation(vp->comp(), "%sTransfer sign constraint %s to %s (0x%p)\n",
                  OPT_DETAILS, signConstraint->getName(), node->getOpCode().getName(),node))
               {
               if (vp->comp()->cg()->traceBCDCodeGen())
                  traceMsg(vp->comp(),"y^y: VP: Transfer sign constraint %s to %s (0x%p)\n",
                     signConstraint->getName(), node->getOpCode().getName(),node);
               node->setKnownSignCode(bcd_minus);
               }
            break;
         case TR_Sign_Unsigned:
            if (!node->knownSignCodeIs(bcd_unsigned) &&
                performTransformation(vp->comp(), "%sTransfer sign constraint %s to %s (0x%p)\n",
                  OPT_DETAILS, signConstraint->getName(), node->getOpCode().getName(),node))
               {
               if (vp->comp()->cg()->traceBCDCodeGen())
                  traceMsg(vp->comp(),"y^y: VP: Transfer sign constraint %s to %s (0x%p)\n",
                     signConstraint->getName(), node->getOpCode().getName(),node);
               node->setKnownSignCode(bcd_unsigned);
               }
            break;
         case TR_Sign_Minus_Clean:
            if (!(node->knownSignCodeIs(bcd_minus) && node->hasKnownCleanSign()) &&
                performTransformation(vp->comp(), "%sTransfer sign constraint %s to %s (0x%p)\n",
                  OPT_DETAILS, signConstraint->getName(), node->getOpCode().getName(),node))
               {
               if (vp->comp()->cg()->traceBCDCodeGen())
                  traceMsg(vp->comp(),"y^y: VP: Transfer sign constraint %s to %s (0x%p)\n",
                     signConstraint->getName(), node->getOpCode().getName(),node);
               node->setKnownSignCode(bcd_minus);
               node->setHasKnownCleanSign(true);
               }
            break;
         default:
            break;
         }
      }

   if (node->getOpCode().isIndirect() &&
       !vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      {
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));
      }
#endif
   return node;
   }

TR::Node *constrainAnyIntLoad(OMR::ValuePropagation *vp, TR::Node *node)
   {
   TR::DataType dataType = node->getDataType();

   // Optimize characters being loaded out of the values array of a constant string
   //
   if (dataType == TR::Int16 && node->getOpCode().isIndirect() &&
       node->getSymbol()->isArrayShadowSymbol() &&
       node->getFirstChild()->getOpCode().isAdd())
      {
      TR::Node *array = node->getFirstChild()->getFirstChild();
      TR::Node *index = node->getFirstChild()->getSecondChild();
      if (index->getOpCode().isLoadConst() && array->getOpCode().isIndirect())
         {
         bool isGlobal;
         TR::Node* base = array->getFirstChild();
         TR::VPConstraint *baseVPConstraint = vp->getConstraint(base, isGlobal);
         if (baseVPConstraint && baseVPConstraint->isConstString())
            {
            TR::VPConstString *constString = baseVPConstraint->getClassType()->asConstString();
            uintptrj_t string = *(uintptrj_t*)constString->getSymRef()->getSymbol()->castToStaticSymbol()->getStaticAddress();

            uint16_t ch = constString->charAt(((TR::Compiler->target.is64Bit() ? index->getLongIntLow() : index->getInt())
                                               - TR::Compiler->om.contiguousArrayHeaderSizeInBytes()) / 2, vp->comp());
            if (ch != 0)
               {
               vp->replaceByConstant(node, TR::VPIntConst::create(vp, ch), true);
               return node;
               }
            }
         }
      }

   //avoid adding the constraint if the symbol represents a variant parm
   TR::Symbol *opSym = node->getOpCode().hasSymbolReference() ? node->getSymbolReference()->getSymbol() : NULL;
   if (opSym && (!opSym->isParm() || vp->isParmInvariant(opSym)))
      {
      // We can't constrain the 32-bit range based on the range of Int8 or
      // Int16, unless allowVPRangeNarrowingBasedOnDeclaredType.
      bool shouldConstrain =
         vp->comp()->getOption(TR_AllowVPRangeNarrowingBasedOnDeclaredType)
         || (dataType != TR::Int8 && dataType != TR::Int16);
      if (shouldConstrain)
         {
         TR::VPConstraint *constraint = TR::VPIntRange::createWithPrecision(vp, dataType, TR_MAX_DECIMAL_PRECISION, TR_maybe);
         if (constraint)
            vp->addGlobalConstraint(node, constraint);
         }
      }

   if (node->isNonNegative())
      vp->addBlockConstraint(node, TR::VPIntRange::create(vp, 0, TR::getMaxSigned<TR::Int32>()));

   checkForNonNegativeAndOverflowProperties(vp, node);

   return node;
   }

TR::Node *constrainShortLoad(OMR::ValuePropagation * vp, TR::Node * node)
   {
   if(findConstant(vp,node))
       return node;

   constrainChildren(vp, node);

   TR::VPConstraint *constraint = TR::VPShortRange::createWithPrecision(vp, TR_MAX_DECIMAL_PRECISION);
   if (constraint)
      vp->addGlobalConstraint(node, constraint);

   if (node->isNonNegative())
      vp->addBlockConstraint(node, TR::VPShortRange::create(vp, 0, TR::getMaxSigned<TR::Int16>()));

   checkForNonNegativeAndOverflowProperties(vp, node);

   vp->checkForInductionVariableLoad(node);

   return node;
   }

TR::Node *constrainIntLoad(OMR::ValuePropagation *vp, TR::Node *node)
   {
   bool wasReplacedByConstant = findConstant(vp, node);
   if (wasReplacedByConstant)
      return node;

   constrainChildren(vp, node);
   constrainAnyIntLoad(vp, node);

   vp->checkForInductionVariableLoad(node);

   if (node->getOpCode().isIndirect() &&
       !vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));
   return node;
   }

bool simplifyJ9ClassFlags(OMR::ValuePropagation *vp, TR::Node *node, bool isLong)
   {
#ifdef J9_PROJECT_SPECIFIC
   uintptrj_t cdfValue = 0;
   bool isGlobal;
   TR::VPConstraint *base = vp->getConstraint(node->getFirstChild(), isGlobal);
   TR::SymbolReference *symRef = node->getSymbolReference();

   if (symRef == vp->comp()->getSymRefTab()->findClassAndDepthFlagsSymbolRef())
      {
      if (node->getFirstChild()->getOpCode().hasSymbolReference() &&
            (node->getFirstChild()->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef()))
         {
         TR::VPConstraint *baseObject = vp->getConstraint(node->getFirstChild()->getFirstChild(), isGlobal);
         if (baseObject && baseObject->getClassType() &&
               baseObject->getClassType()->asFixedClass())
            {
            cdfValue = vp->comp()->fej9()->getClassDepthAndFlagsValue(baseObject->getClassType()->getClass());
            // if the type is a java.lang.Object, make the control branch to the call
            //
            if (baseObject->getClassType()->asFixedClass()->isJavaLangObject(vp))
               cdfValue = TR::Compiler->cls.flagValueForFinalizerCheck(vp->comp());
            }
         }
      }
   else if (symRef == vp->comp()->getSymRefTab()->findClassFlagsSymbolRef())
      {
      TR::VPConstraint *vft = base;
      if (node->getFirstChild()->getOpCode().isConversion())
         vft = vp->getConstraint(node->getFirstChild()->getFirstChild(), isGlobal);
      if (vft && vft->isFixedClass())
         cdfValue = TR::Compiler->cls.classFlagsValue(vft->getClass());
      }

   if (cdfValue > 0)
      {
      if (isLong)
         vp->replaceByConstant(node, TR::VPLongConst::create(vp, cdfValue), true);
      else
         vp->replaceByConstant(node, TR::VPIntConst::create(vp, cdfValue), true);
      return true;
      }
#endif
   return false;
   }

static void checkUnsafeArrayAccess(OMR::ValuePropagation *vp, TR::Node *node)
   {
      if (node->getSymbol()->isUnsafeShadowSymbol())
         {
         if (vp->trace())
            traceMsg(vp->comp(), "Node [%p] has an unsafe symbol reference %d\n", node,
                     node->getSymbolReference()->getReferenceNumber());

         if (node->getFirstChild()->getOpCode().isArrayRef() &&
             node->getFirstChild()->getFirstChild()->getOpCode().isRef())
            {
            TR::Node *baseAddress = node->getFirstChild()->getFirstChild();
            bool isGlobal;
            TR::VPConstraint *baseConstraint = vp->getConstraint(baseAddress, isGlobal);

            if (baseConstraint &&
                baseConstraint->getClassType() &&
                baseConstraint->getClassType()->isArray())
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "is an array access\n");
               vp->addUnsafeArrayAccessNode(node->getGlobalIndex());
               }
            else
               {
               if (vp->trace())
                 traceMsg(vp->comp(), "is not an array access\n");
               }
            }
         }
   }

// Also handles lloadi
//
TR::Node *constrainLload(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   if (node->getOpCode().isIndirect())
      {
      constrainBaseObjectOfIndirectAccess(vp, node);

      checkUnsafeArrayAccess(vp, node);

      // if the node contains an unsafe symbol reference
      // then don't inject a constraint
      if (containsUnsafeSymbolReference(vp, node))
         return node;

      if (constrainCompileTimeLoad(vp, node))
         return node;
      }

   int64_t lo, hi;
   constrainRangeByPrecision(TR::getMinSigned<TR::Int64>(), TR::getMaxSigned<TR::Int64>(), TR_MAX_DECIMAL_PRECISION, lo, hi);
   TR::VPConstraint *constraint = TR::VPLongRange::create(vp,lo,hi);
   if (constraint)
      vp->addGlobalConstraint(node, constraint);

   if (node->isNonNegative())
      vp->addBlockConstraint(node, TR::VPLongRange::create(vp, 0, TR::getMaxSigned<TR::Int64>()));

   checkForNonNegativeAndOverflowProperties(vp, node);

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   vp->checkForInductionVariableLoad(node);

   if (node->getOpCodeValue() == TR::lloadi)
      simplifyJ9ClassFlags(vp, node, true);

   if (node->getOpCode().isIndirect() &&
       !vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));
   return node;
   }

// Also handles ifload
//
TR::Node *constrainFload(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (!findConstant(vp, node))
      constrainChildren(vp, node);

   if (node->getOpCode().isIndirect())
      {
      constrainBaseObjectOfIndirectAccess(vp, node);

      // if the node contains an unsafe symbol reference
      // then don't inject a constraint
      if (containsUnsafeSymbolReference(vp, node))
         return node;
      }

   if (node->getOpCode().isIndirect() &&
       !vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));
   return node;
   }

// Also handles idload
//
TR::Node *constrainDload(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (!findConstant(vp, node))
      constrainChildren(vp, node);

   if (node->getOpCode().isIndirect())
      {
      constrainBaseObjectOfIndirectAccess(vp, node);

      checkUnsafeArrayAccess(vp, node);

      // if the node contains an unsafe symbol reference
      // then don't inject a constraint
      if (containsUnsafeSymbolReference(vp, node))
         return node;
      }

   if (node->getOpCode().isIndirect() &&
       !vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));
   return node;
   }


static const char *getFieldSignature(OMR::ValuePropagation *vp, TR::Node *node, int32_t &len)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   int32_t cookie = symRef->getCPIndex();
   const char *sig;


   if (cookie > 0)
      return symRef->getOwningMethod(vp->comp())->fieldSignatureChars(cookie, len);
   if (cookie == -1)
      {
      // Look for an array element reference. If this is such a reference, look
      // down to get the signature of the base and strip off one layer of array
      // indirection.
      //
      TR::Node *child = node->getFirstChild();
      if (child->isInternalPointer())
         {
         child = child->getFirstChild();
         bool isGlobal;
         TR::VPConstraint *constraint = vp->getConstraint(child, isGlobal);
         if (constraint)
            {
            sig = constraint->getClassSignature(len);
            if (sig && *sig == '[')
               {
               --len;
               return sig+1;
               }
            }
         }
      }
   return NULL;
   }

static void addKnownObjectConstraints(OMR::ValuePropagation *vp, TR::Node *node)
   {
   TR::KnownObjectTable *knot = vp->comp()->getKnownObjectTable();
   if (!knot)
      return;

   TR::SymbolReference *symRef = node->getSymbolReference();
   if (symRef->isUnresolved())
      return;

   uintptrj_t *objectReferenceLocation = NULL;
   if (symRef->hasKnownObjectIndex())
      objectReferenceLocation = symRef->getKnownObjectReferenceLocation(vp->comp());
   else if (symRef->getSymbol()->isFixedObjectRef())
      objectReferenceLocation = (uintptrj_t*)symRef->getSymbol()->castToStaticSymbol()->getStaticAddress();

#ifdef J9_PROJECT_SPECIFIC
   if (objectReferenceLocation)
      {
      bool isString;
      bool isFixedJavaLangClass;
      TR::KnownObjectTable::Index knownObjectIndex;
      TR_OpaqueClassBlock *clazz, *jlClass;

         {
         TR::VMAccessCriticalSection getObjectReferenceLocation(vp->comp());
         clazz   = TR::Compiler->cls.objectClass(vp->comp(), *objectReferenceLocation);
         isString = TR::Compiler->cls.isString(vp->comp(), clazz);
         jlClass = vp->fe()->getClassClassPointer(clazz);
         isFixedJavaLangClass = (jlClass == clazz);
         if (isFixedJavaLangClass)
            {
            // A FixedClass constraint means something different when the class happens to be java/lang/Class.
            // Must add constraints pertaining to the class that the java/lang/Class object represents.
            //
            clazz = TR::Compiler->cls.classFromJavaLangClass(vp->comp(), *objectReferenceLocation);
            }
         knownObjectIndex = knot->getIndex(*objectReferenceLocation);
         }


      if (isString && symRef->getSymbol()->isStatic())
         {
         // There's a lot of machinery around optimizing const strings.  Even
         // though we lose object identity info this way, it's likely better to
         // engage the const string optimizations.
         //
         vp->addGlobalConstraint(node,
            TR::VPClass::create(vp, TR::VPConstString::create(vp, symRef),
            TR::VPNonNullObject::create(vp), NULL, NULL,
            TR::VPObjectLocation::create(vp, TR::VPObjectLocation::HeapObject)));
         }
      else if (jlClass) // without a jlClass, we can't tell what kind of constraint to add
         {
         TR::VPConstraint *constraint = NULL;
         const char *classSig = TR::Compiler->cls.classSignature(vp->comp(), clazz, vp->trMemory());
         if (isFixedJavaLangClass)
            {
            if (performTransformation(vp->comp(), "%sAdd ClassObject constraint to %p based on known java/lang/Class %s =obj%d\n", OPT_DETAILS, node, classSig, knownObjectIndex))
               {
               constraint = TR::VPClass::create(vp,
                  TR::VPKnownObject::createForJavaLangClass(vp, knownObjectIndex),
                  TR::VPNonNullObject::create(vp), NULL, NULL,
                  TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject));
               }
            }
         else
            {
            if (performTransformation(vp->comp(), "%sAdd known-object constraint to %p based on known object obj%d of class %s\n", OPT_DETAILS, node, knownObjectIndex, classSig))
               {
               constraint = TR::VPClass::create(vp,
                  TR::VPKnownObject::create(vp, knownObjectIndex),
                  TR::VPNonNullObject::create(vp), NULL, NULL,
                  TR::VPObjectLocation::create(vp, TR::VPObjectLocation::HeapObject));
               }
            }
         if (constraint)
            {
            if (vp->trace())
               {
               traceMsg(vp->comp(), "      -> Constraint is ");
               constraint->print(vp);
               traceMsg(vp->comp(), "\n");
               }
            vp->addGlobalConstraint(node, constraint);
            }
         }
      }
#endif
   }

TR::Node *constrainAload(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;

   // before undertaking aload specific handling see if the default load transformation helps
   // But even if it does help, node may still be an aload that we can constrain.
   if (TR::TransformUtil::transformDirectLoad(vp->comp(), node)
       && node->getOpCodeValue() != TR::aload)
      {
      constrainNewlyFoldedConst(vp, node, false /* !isGlobal, conservative */);
      return node;
      }

   TR::VPConstraint *constraint = NULL;
#ifdef J9_PROJECT_SPECIFIC
   TR::SymbolReference *symRef = NULL;
   if (node->getOpCode().hasSymbolReference())
      {
      symRef = node->getSymbolReference();

      if (symRef->getSymbol()->isLocalObject())
         {
         vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::StackObject));
         }
      if (symRef->getSymbol()->isClassObject())
         {
         vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject));
         }

      addKnownObjectConstraints(vp, node);

      if (!symRef->getSymbol()->isArrayShadowSymbol())
         {
         TR::Symbol *sym = symRef->getSymbol();
         bool haveClassLookaheadInfo = false;
         if (sym->isStatic() &&
            !symRef->isUnresolved() &&
            (sym->isPrivate() ||
               sym->isFinal()))
            haveClassLookaheadInfo = true;

         if (haveClassLookaheadInfo)
            {
            bool foundInfo = false;
            if (sym->isStatic() && sym->isFinal())
               {
               TR::StaticSymbol * symbol = sym->castToStaticSymbol();
               TR::DataType type = symbol->getDataType();
               TR_OpaqueClassBlock * classOfStatic = symRef->getOwningMethod(vp->comp())->classOfStatic(symRef->getCPIndex());
               if (classOfStatic == NULL)
                  {
                  int         len = 0;
                  char * classNameOfFieldOrStatic = NULL;
                  classNameOfFieldOrStatic = symRef->getOwningMethod(vp->comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), len);
                  if (classNameOfFieldOrStatic)
                     {
                     classNameOfFieldOrStatic=classNameToSignature(classNameOfFieldOrStatic, len, vp->comp());
                     TR_OpaqueClassBlock * curClass  = vp->fe()->getClassFromSignature(classNameOfFieldOrStatic, len, symRef->getOwningMethod(vp->comp()));
                     TR_OpaqueClassBlock * owningClass = vp->comp()->getJittedMethodSymbol()->getResolvedMethod()->containingClass();
                     if (owningClass == curClass)
                        classOfStatic = curClass;
                     }
                  }
               bool isClassInitialized = false;
               TR_PersistentClassInfo * classInfo =
                  vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classOfStatic, vp->comp());
               if (classInfo && classInfo->isInitialized())
                  isClassInitialized = true;

               if (classOfStatic != vp->comp()->getSystemClassPointer() &&
                   isClassInitialized &&
                   !vp->comp()->getOption(TR_AOT) &&
                   (type == TR::Address))
                  {
                  TR::VMAccessCriticalSection constrainAloadCriticalSection(vp->comp(),
                                                                             TR::VMAccessCriticalSection::tryToAcquireVMAccess);
                  void * p = symbol->getStaticAddress();
                  if ((*(void **)p != 0) &&
                      constrainAloadCriticalSection.hasVMAccess())
                     {
                     int32_t arrLength = *((int32_t *) (((uintptrj_t) *(void ***)p) + (uintptrj_t) vp->fe()->getOffsetOfContiguousArraySizeField() ));

                    if (arrLength == 0 && TR::Compiler->om.useHybridArraylets())
                       {
                       arrLength = *((int32_t *) (((uintptrj_t) *(void ***)p) + (uintptrj_t) vp->fe()->getOffsetOfDiscontiguousArraySizeField() ));
                       }

                     int32_t len;
                     const char *sig = symRef->getTypeSignature(len);
                     if (sig && (len > 0) &&
                         (sig[0] == '[' || sig[0] == 'L'))
                        {
                        int32_t elementSize = arrayElementSize(sig, len, node, vp);
                        if (elementSize != 0)
                           {
                           vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
                           vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, arrLength, arrLength, elementSize));
                           vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotClassObject));
                           foundInfo = true;
                           }
                        }
                     }
                  }
               }

            if (!foundInfo)
               {
               TR_PersistentClassInfo * classInfo =
                  vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(symRef->getOwningMethod(vp->comp())->classOfStatic(symRef->getCPIndex()), vp->comp());
               if (classInfo && classInfo->getFieldInfo())
                  {
                  TR_PersistentFieldInfo * fieldInfo = classInfo->getFieldInfo()->findFieldInfo(vp->comp(), node, false);
                  if (fieldInfo)
                     {
                     TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo ? fieldInfo->asPersistentArrayFieldInfo() : 0;
                     if (arrayFieldInfo && arrayFieldInfo->isDimensionInfoValid())
                        {
                        int32_t firstDimension = arrayFieldInfo->getDimensionInfo(0);
                        if (firstDimension >= 0)
                           {
                           int32_t len;
                           const char *sig = getFieldSignature(vp, node, len);
                           if (sig && (len > 0) && (sig[0] == '[' || sig[0] == 'L'))
                              {
                              int32_t elementSize = arrayElementSize(sig, len, node, vp);
                              if (elementSize != 0)
                                 {
                                 if (vp->trace())
                                    traceMsg(vp->comp(), "Using class lookahead info to find out non null, array dimension, and object location\n");
                                 vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, firstDimension, firstDimension, elementSize));
                                 vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotClassObject));
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }

   int32_t          len = 0;
   const char      *sig;

   if (vp->comp()->getCurrentMethod()->isJ9() && symRef &&
       (symRef == vp->comp()->getSymRefTab()->findOrCreateExcpSymbolRef() &&
        vp->_curBlock->isCatchBlock() && vp->_curBlock->getExceptionClassNameChars()))
      {
      if (vp->_curBlock->getExceptionClass())
         {
         constraint = NULL;
         if (vp->_curBlock->specializedDesyncCatchBlock())
            {
            ListIterator<TR_Pair<TR::Node, TR::Block> > throwsIt(&vp->_predictedThrows);
            TR_Pair<TR::Node, TR::Block> *predictedThrow;
            for (predictedThrow = throwsIt.getFirst(); predictedThrow; predictedThrow = throwsIt.getNext())
               {
               TR::Node *predictedNode = predictedThrow->getKey();
               TR::Block *predictedBlock = predictedThrow->getValue();

               TR::TreeTop * tt;
               TR::Node * node = vp->findThrowInBlock(predictedBlock, tt);

               if (node != predictedNode)
                  continue;

               if (node)
                  {
                  // hack we're using the second child of the throw to store the destination block
                  // todo: make this less hacky
                  //
                  node->setNumChildren(2);
                  TR::Block * b = (TR::Block *)node->getSecondChild();
                  node->setNumChildren(1);
                  if (vp->_curBlock == b)
                     {
                     constraint = TR::VPFixedClass::create(vp, vp->_curBlock->getExceptionClass());
                     break;
                     }
                  }
               }
            }
         if (!constraint)
            constraint = TR::VPResolvedClass::create(vp, vp->_curBlock->getExceptionClass());
         }
      else
         {
         len = vp->_curBlock->getExceptionClassNameLength();
         sig = classNameToSignature(vp->_curBlock->getExceptionClassNameChars(), len, vp->comp());
         constraint = TR::VPUnresolvedClass::create(vp, sig, len, symRef->getOwningMethod(vp->comp()));
         }
      vp->addGlobalConstraint(node, constraint);
      vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
      vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotClassObject));
      node->setIsNonNull(true);
      return node;
      }

   bool isFixed = false;
   if (symRef && symRef->getSymbol()->isStatic())
      {
      if (symRef->getSymbol()->isConstString() && !symRef->isUnresolved() && vp->comp()->getStringClassPointer())
         {
         constraint = TR::VPClass::create(vp, TR::VPConstString::create(vp, symRef),
                      TR::VPNonNullObject::create(vp), NULL, NULL,
                      TR::VPObjectLocation::create(vp, TR::VPObjectLocation::HeapObject));
         vp->addGlobalConstraint(node, constraint);
         }
      else
         {
            {
            sig = symRef->getTypeSignature(len, stackAlloc, &isFixed);
            }
         if (sig)
            {
            TR_OpaqueClassBlock *classBlock = vp->fe()->getClassFromSignature(sig, len, symRef->getOwningMethod(vp->comp()));

            if (  classBlock
               && TR::Compiler->cls.isInterfaceClass(vp->comp(), classBlock)
               && !vp->comp()->getOption(TR_TrustAllInterfaceTypeInfo))
               {
               classBlock = NULL;
               }

            if (classBlock)
               {
               TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(classBlock);
               if (jlClass)
                  {
                  if (classBlock != jlClass)
                     {
                     constraint = TR::VPClassType::create(vp, sig, len, symRef->getOwningMethod(vp->comp()), isFixed, classBlock);
                     if (*sig == '[' || sig[0] == 'L')
                        {
                        int32_t elementSize = arrayElementSize(sig, len, node, vp);
                        if (elementSize != 0)
                           {
                           constraint = TR::VPClass::create(vp, (TR::VPClassType*)constraint, NULL, NULL,
                                 TR::VPArrayInfo::create(vp, 0, elementSize == 0 ? TR::getMaxSigned<TR::Int32>() : TR::getMaxSigned<TR::Int32>()/elementSize, elementSize),
                                 TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotClassObject));
                           }
                        }
                     }
                  else
                     {
                     constraint = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject);
                     }
                  vp->addGlobalConstraint(node, constraint);
                  }
               }
            else if (vp->comp()->isOptServer())
               {
               TR_ResolvedMethod *method = node->getSymbolReference()->getOwningMethod(vp->comp());
               constraint = TR::VPUnresolvedClass::create(vp, sig, len, method);
               vp->addBlockConstraint(node, constraint);
               }
            }
         }
      }
#endif

  if (node->isNonNull())
      vp->addBlockConstraint(node, TR::VPNonNullObject::create(vp));
   else if (node->isNull())
      vp->addBlockConstraint(node, TR::VPNullObject::create(vp));

   bool isGlobal;
   constraint = vp->getConstraint(node, isGlobal);
   if (constraint && constraint->isNonNullObject())
      node->setIsNonNull(true);
   return node;
   }


static TR::Node *findArrayLengthNode(OMR::ValuePropagation *vp, TR::Node *node, List<TR::Node> *arraylengthNodes)
   {
   int32_t nodeVN = vp->getValueNumber(node);

   ListIterator<TR::Node> arraylengthNodesIt(arraylengthNodes);
   TR::Node *nextNode;
   for (nextNode = arraylengthNodesIt.getFirst(); nextNode != NULL; nextNode = arraylengthNodesIt.getNext())
      {
      //
      // Check if the arraylength node is still in the trees and
      // still looks like an arraylength node
      //
      if (nextNode->getOpCode().isArrayLength() && (nextNode->getReferenceCount() > 0))
         {
         if (nodeVN == vp->getValueNumber(nextNode->getFirstChild()))
            return nextNode;
         }
      }
   return NULL;
   }



static TR::Node *findArrayIndexNode(OMR::ValuePropagation *vp, TR::Node *node, int32_t stride)
  {
  TR::Node *offset = node->getSecondChild();
  bool usingAladd = (TR::Compiler->target.is64Bit()
                     ) ?
          true : false;

  if (usingAladd)
     {
     int32_t constValue;
     if ((offset->getOpCode().isAdd() || offset->getOpCode().isSub()) &&
         offset->getSecondChild()->getOpCode().isLoadConst())
        {
        if (offset->getSecondChild()->getType().isInt64())
           constValue = (int32_t) offset->getSecondChild()->getLongInt();
        else
           constValue = offset->getSecondChild()->getInt();
        }

     if ((offset->getOpCode().isAdd() &&
         (offset->getSecondChild()->getOpCode().isLoadConst() &&
         (constValue == TR::Compiler->om.contiguousArrayHeaderSizeInBytes()))) ||
         (offset->getOpCode().isSub() &&
         (offset->getSecondChild()->getOpCode().isLoadConst() &&
         (constValue == -(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes()))))
        {
        TR::Node *offset2 = offset->getFirstChild();
        if (offset2->getOpCodeValue() == TR::lmul)
           {
           TR::Node *mulStride = offset2->getSecondChild();
           int32_t constValue;
           if (mulStride->getOpCode().isLoadConst())
              {
              if (mulStride->getType().isInt64())
                 constValue = (int32_t) mulStride->getLongInt();
              else
                 constValue = mulStride->getInt();
              }

           if (mulStride->getOpCode().isLoadConst() &&
              constValue == stride)
              {
              if (offset2->getFirstChild()->getOpCodeValue() == TR::i2l)
                 return offset2->getFirstChild()->getFirstChild();
              else
                 return offset2->getFirstChild();
              }
           }
        else if (stride == 1)
          return offset2;
        }
     }
  else
     {
     if ((offset->getOpCode().isAdd() &&
          (offset->getSecondChild()->getOpCode().isLoadConst() &&
           (offset->getSecondChild()->getInt() == TR::Compiler->om.contiguousArrayHeaderSizeInBytes()))) ||
           (offset->getOpCode().isSub() &&
           (offset->getSecondChild()->getOpCode().isLoadConst() &&
           (offset->getSecondChild()->getInt() == -(int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes()))))
        {
        TR::Node *offset2 = offset->getFirstChild();
        if (offset2->getOpCodeValue() == TR::imul)
           {
           TR::Node *mulStride = offset2->getSecondChild();
           if (mulStride->getOpCode().isLoadConst() &&
               mulStride->getInt() == stride)
              {
              return offset2->getFirstChild();
              }
           }
        else if (stride == 1)
           return offset2;
        }
     }
  return NULL;
  }

// For TR::aiadd and TR::aladd
TR::Node *constrainAddressRef(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *parent = vp->getCurrentParent();
   if (node->getFirstChild()->getOpCode().isLoadVar() &&
       parent &&
       ((parent->getOpCode().isLoadIndirect() ||
        parent->getOpCode().isStoreIndirect()) &&
       (parent->getFirstChild() == node)))
      {
      TR::Node *arrayLoad = node->getFirstChild();
      TR::Node *arraylengthNode = findArrayLengthNode(vp, arrayLoad, vp->getArraylengthNodes());
      TR::Node *indexNode = NULL;
      if (arraylengthNode)
         indexNode = findArrayIndexNode(vp, node, arraylengthNode->getArrayStride());

      if (arraylengthNode && indexNode)
         {
         //TR::VPLessThanOrEqual *rel = TR::VPLessThanOrEqual::create(vp, -1);
         //rel->setHasArtificialIncrement();
         //vp->addBlockConstraint(indexNode, rel, arraylengthNode, false);
         }
      }

   return node;
   }


TR::Node *constrainIiload(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   constrainBaseObjectOfIndirectAccess(vp, node);

   // if the node contains an unsafe symbol reference
   // then don't inject a constraint
   if (containsUnsafeSymbolReference(vp, node))
      return node;

   addKnownObjectConstraints(vp, node);

   if (constrainCompileTimeLoad(vp, node))
      return node;

   TR::SymbolReference *symRef = node->getSymbolReference();
   bool isGlobal = false;
   TR::VPConstraint *base = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (base && base->isConstString())
      {
      TR::VPConstString *kString = base->getClassType()->asConstString();
      int32_t *konst = 0;
      bool success = kString->getFieldByName(symRef, (void *&) konst, vp->comp());
      if (success)
         {
         int32_t realKonst;
         realKonst = *konst;
         if (!base->isNonNullObject() && vp->getCurrentParent()->getOpCodeValue() == TR::NULLCHK)
            {
            // replace node with a PassThrough in the original tree
            TR::Node *passThroughNode = TR::Node::create(TR::PassThrough, 1, node->getFirstChild());
            TR::Node *nullCheckNode = vp->getCurrentParent();
            nullCheckNode->setAndIncChild(0, passThroughNode);

            // create a treetop containing the iiload following the current tree
            TR::TreeTop *newTree = TR::TreeTop::create(vp->comp(), TR::Node::create(TR::treetop, 1, node));
            node->decReferenceCount();
            TR::TreeTop *prevTree = vp->_curTree;
            newTree->join(prevTree->getNextTreeTop());
            prevTree->join(newTree);
            }
         vp->replaceByConstant(node, TR::VPIntConst::create(vp, realKonst), true);
         return node;
         }
      }

   // If the underlying object is a recognized object, we know that
   // its count field must be limited by the amount of heap memory that could
   // have been allocated for its buffer.
   //
#ifdef J9_PROJECT_SPECIFIC
   TR::Symbol::RecognizedField field = symRef->getSymbol()->getRecognizedField();

   if (field == TR::Symbol::Java_io_ByteArrayOutputStream_count)
      {
      // can't exceed address space in size
      vp->addGlobalConstraint(node, TR::VPIntRange::create(vp, 0, TR::getMaxSigned<TR::Int32>() >> 1));
      node->setIsNonNegative(true);
      node->setCannotOverflow(true);
      }
   else
#endif
      constrainAnyIntLoad(vp, node);

   if (simplifyJ9ClassFlags(vp, node, false))
      return node;

   if (!vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));
   return node;
   }

TR::Node *constrainIaload(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   constrainBaseObjectOfIndirectAccess(vp, node);

   // if the node contains an unsafe symbol reference
   // then don't inject a constraint
   if (containsUnsafeSymbolReference(vp, node))
      return node;

   bool isGlobal;

   // an indirectly loaded object cannot be a stack object
   //
   vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotStackObject));

   TR::VPConstraint *constraint;
   int32_t len = 0;
   const char *sig = getFieldSignature(vp, node, len);
   int32_t arrLength = -1;
   int32_t elementSize = -1;
   bool foundInfo = false;
   bool isFixed = false;

   if (node->getOpCode().hasSymbolReference())
      {
      TR::Node *transformedNode = TR::TransformUtil::transformIndirectLoad(vp->comp(), node);
      if (transformedNode)
         {
         // Front-end has changed the node.
         //
         if (transformedNode != node)
            {
            vp->removeNode(node);
            transformedNode->incReferenceCount();
            node = transformedNode;
            }

         // If it is no longer an iaload,
         // the rest of this handler may not be applicable anymore.
         //
         if (node->getOpCode().isLoadIndirect() && node->getDataType() == TR::Address)
            {
            // We can proceed
            }
         else
            {
            return node;
            }
         }

      addKnownObjectConstraints(vp, node);

      TR::SymbolReference *symRef = node->getSymbolReference();
      bool attemptCompileTimeLoad = true;

#ifdef J9_PROJECT_SPECIFIC
      int32_t nonHelperId = symRef->getReferenceNumber()
         - vp->comp()->getSymRefTab()->getNumHelperSymbols();
      switch (nonHelperId)
         {
         case TR::SymbolReferenceTable::componentClassSymbol:
         case TR::SymbolReferenceTable::componentClassAsPrimitiveSymbol:
            // Don't try to dereference through <componentClass> unless we
            // observe below that the base address is constrained to a fixed
            // J9Class representing an array type. It's possible on dead paths
            // to have the base address point to a non-array J9Class, in which
            // case constrainCompileTimeLoad would load some garbage and treat
            // the garbage as a J9Class pointer for loadaddr.
            //
            // Note that if constrainCompileTimeLoad is called for an ancestor
            // node, it won't dereference through this <componentClass>,
            // instead expecting it to have a good constraint if possible.
            attemptCompileTimeLoad = false;
            break;
         default:
            break; // OK to attempt compile-time load
         }

      const static char *needInitializedCheck = feGetEnv("TR_needInitializedCheck");
      TR::VPConstraint *base = vp->getConstraint(node->getFirstChild(), isGlobal);
      if (base && node->getOpCode().hasSymbolReference() &&
          (node->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef()))
         {
         TR::VPClassPresence *nonnull = TR::VPNonNullObject::create(vp);
         if (!base->isFixedClass())
            {
            vp->addBlockOrGlobalConstraint(node, TR::VPClass::create(vp, NULL, nonnull, NULL, NULL, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject)), isGlobal);
            }
         else if (base->isClassObject() == TR_yes)
            {
            // base can only be an instance of java/lang/Class, since
            // we can't load <vft> relative to a J9Class.
            vp->addBlockOrGlobalConstraint(node,
               TR::VPClass::create(vp,
                  TR::VPFixedClass::create(vp, vp->comp()->fej9()->getClassClassPointer(base->getClass())),
                  nonnull,
                  NULL, NULL,
                  TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject)),
               isGlobal);
            }
         else
            {
            vp->addBlockOrGlobalConstraint(node,
               TR::VPClass::create(vp,
                  TR::VPFixedClass::create(vp, base->getClass()),
                  nonnull,
                  NULL, NULL,
                  TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject)),
               isGlobal);
            }
         node->setIsNonNull(true);
         }
      else if (base
               && base->isJavaLangClassObject() == TR_yes
               && base->isNonNullObject()
               && base->getKnownObject())
         {
         if (symRef == vp->comp()->getSymRefTab()->findClassFromJavaLangClassSymbolRef())
            {
            TR::KnownObjectTable *knot = vp->comp()->getOrCreateKnownObjectTable();

               {
               TR::VMAccessCriticalSection constrainIaloadCriticalSection(vp->comp(),
                                                                           TR::VMAccessCriticalSection::tryToAcquireVMAccess);

               if (constrainIaloadCriticalSection.hasVMAccess())
                  {
                  uintptrj_t jlclazz = knot->getPointer(base->getKnownObject()->getIndex());
                  TR_OpaqueClassBlock *clazz = TR::Compiler->cls.classFromJavaLangClass(vp->comp(), jlclazz);
                  if (TR::Compiler->cls.isClassInitialized(vp->comp(), clazz))
                     {
                     // this should be able to be precise because we do know the compenent class pointer precisely
                     // but clearly others make invalid assumptions on seeing this fixed class constraint so be conservative
                     // and don't let the world know if we aren't sure the elements match this type
                     if (!TR::Compiler->cls.isInterfaceClass(vp->comp(), clazz) &&
                         (TR::Compiler->cls.isClassFinal(vp->comp(), clazz)
                          || TR::Compiler->cls.isPrimitiveClass(vp->comp(), clazz)))
                        {
                        vp->addBlockOrGlobalConstraint(node,
                           TR::VPClass::create(vp,
                              TR::VPFixedClass::create(vp, clazz),
                              TR::VPNonNullObject::create(vp),
                              NULL, NULL,
                              TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject)),
                           isGlobal);
                        }
                     else
                        {
                        vp->addBlockOrGlobalConstraint(node,
                           TR::VPClass::create(vp,
                              TR::VPResolvedClass::create(vp, clazz),
                              TR::VPNonNullObject::create(vp),
                              NULL, NULL,
                              TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject)),
                           isGlobal);
                        }
                     node->setIsNonNull(true);
                     }
                  }

               } // VM Access Critical Section
            }
         }
      else if (base
          && base->isJ9ClassObject() == TR_yes
          && base->isNonNullObject()
          && base->getClassType()
          && base->getClassType()->asFixedClass()
          && base->getClass()
          && (!needInitializedCheck || TR::Compiler->cls.isClassInitialized(vp->comp(), base->getClass())))
         {
         if (symRef == vp->comp()->getSymRefTab()->findJavaLangClassFromClassSymbolRef())
            {
            TR::KnownObjectTable *knot = vp->comp()->getOrCreateKnownObjectTable();
            TR_J9VMBase *fej9 = (TR_J9VMBase *)(vp->comp()->fe());
            TR::KnownObjectTable::Index knownObjectIndex = knot->getIndexAt((uintptrj_t*)(base->getClass() + fej9->getOffsetOfJavaLangClassFromClassField()));
            vp->addBlockOrGlobalConstraint(node,
                  TR::VPClass::create(vp,
                     TR::VPKnownObject::createForJavaLangClass(vp, knownObjectIndex),
                     TR::VPNonNullObject::create(vp), NULL, NULL,
                     TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject)),
                  isGlobal);
            node->setIsNonNull(true);
            }
         else if (symRef == vp->comp()->getSymRefTab()->findArrayComponentTypeSymbolRef()
                  && base->getClassType()->isArray() == TR_yes)
            {
            attemptCompileTimeLoad = true;
            TR_OpaqueClassBlock *componentClass = vp->comp()->fej9()->getComponentClassFromArrayClass(base->getClass());
            // this should be able to be precise because we do know the compenent class pointer precisely
            // but clearly others make invalid assumptions on seeing this fixed class constraint so be conservative
            // and don't let the world know if we aren't sure the elements match this type
            if (!TR::Compiler->cls.isInterfaceClass(vp->comp(), componentClass) &&
                (TR::Compiler->cls.isClassFinal(vp->comp(), componentClass)
                 || TR::Compiler->cls.isPrimitiveClass(vp->comp(), componentClass)))
               {
               vp->addBlockOrGlobalConstraint(node,
                  TR::VPClass::create(vp,
                     TR::VPFixedClass::create(vp, componentClass),
                     TR::VPNonNullObject::create(vp),
                     NULL, NULL,
                     TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject)),
                  isGlobal);
               }
            else
               {
               vp->addBlockOrGlobalConstraint(node,
                  TR::VPClass::create(vp,
                     TR::VPResolvedClass::create(vp, componentClass),
                     TR::VPNonNullObject::create(vp),
                     NULL, NULL,
                     TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject)),
                  isGlobal);
               }
            node->setIsNonNull(true);
            }
         }
      else if (symRef == vp->comp()->getSymRefTab()->findJavaLangClassFromClassSymbolRef())
         {
         vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject));
         vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
         node->setIsNonNull(true);
         }
      else if (symRef == vp->comp()->getSymRefTab()->findClassFromJavaLangClassSymbolRef())
         {
         vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject));
         vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
         node->setIsNonNull(true);
         }

#endif

      if (attemptCompileTimeLoad && constrainCompileTimeLoad(vp, node))
         return node;

#ifdef J9_PROJECT_SPECIFIC
      TR::Symbol *sym = node->getSymbol();
      switch (sym->getRecognizedField())
         {
         case TR::Symbol::Java_lang_invoke_MethodHandle_thunks:
            if (!node->isNonNull() && performTransformation(vp->comp(), "%s[%p] recognized field is never null: %s\n", OPT_DETAILS, node, symRef->getName(vp->comp()->getDebug())))
               {
               vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
               node->setIsNonNull(true);
               }
            break;
         case TR::Symbol::Java_lang_invoke_MutableCallSite_target:
            {
            if (!vp->comp()->getOption(TR_DisableMCSBypass) && vp->comp()->getMethodHotness() >= hot)
               {
               TR::VPConstraint *callSiteConstraint = vp->getConstraint(node->getFirstChild(), isGlobal);
               if (callSiteConstraint && callSiteConstraint->getKnownObject())
                  {
                  // Note that the known object constraint does not preclude
                  // the possibility that the callSite expression is null at
                  // this location at run time.  However, we can still do the
                  // optimization on the premise that the code must contain
                  // a null check before this code runs, or else it's already
                  // wrong.  (This is an example of an opt that benefits from
                  // the fact that known object constraints are conditional
                  // on the expression being non-null.)
                  //
                  static char *bypassOnEveryVPRun = feGetEnv("TR_bypassOnEveryVPRun");
                  bool bypassOnThisRun = bypassOnEveryVPRun || vp->getLastRun();
                  if (bypassOnThisRun)
                     {
                     TR::KnownObjectTable::Index callSiteKOI = callSiteConstraint->getKnownObject()->getIndex();
                     uintptrj_t *bypassLocation = NULL;

                        {
                        TR::VMAccessCriticalSection constrainIaloadCriticalSection(vp->comp(),
                                                                                    TR::VMAccessCriticalSection::tryToAcquireVMAccess);
                        if (constrainIaloadCriticalSection.hasVMAccess())
                           {
                           uintptrj_t *siteLocation = vp->comp()->getKnownObjectTable()->getPointerLocation(callSiteKOI);
                           bypassLocation = vp->comp()->fej9()->mutableCallSite_bypassLocation(*siteLocation);
                           if (  !bypassLocation
                              && performTransformation(vp->comp(), "%s[%p] create CallSite bypass for obj%d\n", OPT_DETAILS, node, callSiteKOI))
                              {
                              bypassLocation = vp->comp()->fej9()->mutableCallSite_findOrCreateBypassLocation(*siteLocation);
                              }
                           }
                        else
                           {
                           if (vp->trace())
                              traceMsg(vp->comp(), "[%p] Unable to acquire VM access.  Not attempting to bypass CallSite\n", node);
                           }
                        }

                     if (  bypassLocation
                        && performTransformation(vp->comp(), "%s[%p] bypass CallSite load from obj%d\n", OPT_DETAILS, node, callSiteKOI))
                        {
                        TR::VPConstraint *targetConstraint = vp->getConstraint(node, isGlobal);
                        TR::KnownObjectTable::Index targetKOI = TR::KnownObjectTable::UNKNOWN;
                        if (targetConstraint && targetConstraint->getKnownObject())
                           targetKOI = targetConstraint->getKnownObject()->getIndex();
                        vp->removeChildren(node);
                        TR::Node::recreate(node, TR::aload);
                        node->setNumChildren(0);
                        node->setSymbolReference(vp->comp()->getSymRefTab()->createKnownStaticRefereneceSymbolRef(bypassLocation, targetKOI));
                        return node;
                        }
                     }
                  }
               }
            }
            break;
         default:
         	break;
         }

      if (!symRef->getSymbol()->isArrayShadowSymbol())
         {
         TR::Symbol *sym = symRef->getSymbol();
         bool haveClassLookaheadInfo = false;
         if (((sym->isShadow() &&
             node->getFirstChild()->isThisPointer()) ||
             sym->isStatic()) &&
             !symRef->isUnresolved() &&
             (sym->isPrivate() ||
              sym->isFinal()))
            haveClassLookaheadInfo = true;

         if (haveClassLookaheadInfo)
            {
            if (sym->isStatic() && sym->isFinal())
               {
               TR::StaticSymbol * symbol = sym->castToStaticSymbol();
               TR::DataType type = symbol->getDataType();
               TR_OpaqueClassBlock * classOfStatic = symRef->getOwningMethod(vp->comp())->classOfStatic(symRef->getCPIndex());
               bool isClassInitialized = false;
               TR_PersistentClassInfo * classInfo =
                  vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classOfStatic, vp->comp());
               if (classInfo && classInfo->isInitialized())
                  isClassInitialized = true;

               if ((classOfStatic != vp->comp()->getSystemClassPointer() &&
                   isClassInitialized &&
                   !vp->comp()->getOption(TR_AOT) &&
                    (type == TR::Address)))
                  {
                  TR::VMAccessCriticalSection constrainIaloadCriticalSection(vp->comp(),
                                                                              TR::VMAccessCriticalSection::tryToAcquireVMAccess);
                  void * p = symbol->getStaticAddress();
                  if ((*(void **)p != 0) &&
                      constrainIaloadCriticalSection.hasVMAccess())
                     {
                     arrLength = *((int32_t *) (((uintptrj_t) *(void ***)p) + (uintptrj_t) vp->fe()->getOffsetOfContiguousArraySizeField()));

                     if (arrLength == 0 && TR::Compiler->om.useHybridArraylets())
                        {
                        arrLength = *((int32_t *) (((uintptrj_t) *(void ***)p) + (uintptrj_t) vp->fe()->getOffsetOfDiscontiguousArraySizeField()));
                        }

                     sig = symRef->getTypeSignature(len);
                     if (sig && (len > 0) &&
                         (sig[0] == '[' || sig[0] == 'L'))
                        {
                        elementSize = arrayElementSize(sig, len, node, vp);
                        if (elementSize != 0)
                           {
                           //vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
                           //vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, arrLength, arrLength, elementSize));
                           foundInfo = true;
                           isFixed = true;
                           }
                        }
                     }
                  }
               }

            if (!foundInfo)
               {
               TR_PersistentClassInfo * classInfo =
                  vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(vp->comp()->getCurrentMethod()->containingClass(), vp->comp());
               if (classInfo && classInfo->getFieldInfo())
                  {
                  TR_PersistentFieldInfo * fieldInfo = classInfo->getFieldInfo()->findFieldInfo(vp->comp(), node, false);
                  if (fieldInfo)
                     {
                     TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo ? fieldInfo->asPersistentArrayFieldInfo() : 0;
                     if (arrayFieldInfo && arrayFieldInfo->isDimensionInfoValid())
                        {
                        arrLength = arrayFieldInfo->getDimensionInfo(0);
                        if (arrLength >= 0 && sig && (len > 0) &&
                            (sig[0] == '[' || sig[0] == 'L'))
                           {
                           elementSize = arrayElementSize(sig, len, node, vp);
                           if (elementSize != 0)
                              {
                              if (vp->trace())
                                 traceMsg(vp->comp(), "Using class lookahead info to find out non null, array dimension\n");
                              //vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
                              //vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, arrLength, arrLength, elementSize));
                              isFixed = true;
                              foundInfo = true;
                              }
                           }
                        }
                     }
                  }
               }
            }

         TR::Node *underlyingObject = node->getFirstChild();
          constraint = vp->getConstraint(underlyingObject, isGlobal);
          if (constraint && constraint->getClass())
             {
             int32_t len;
             const char *objectSig = constraint->getClassSignature(len);
             if (objectSig &&
                 (len == 19) && constraint->isFixedClass() &&
                 !strncmp(objectSig, "Ljava/util/TreeMap;", 19))
                {
                TR_ResolvedMethod *method = node->getSymbolReference()->getOwningMethod(vp->comp());
                TR_OpaqueClassBlock *clazz = vp->fe()->getClassFromSignature(objectSig, len, method);
                if (clazz)
                   {
                   TR_ASSERT(!TR::Compiler->cls.isInterfaceClass(vp->comp(), clazz) , "java/util/TreeMap is not an interface");
                   TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(clazz);
                   if (jlClass)
                      {
                      if (clazz != jlClass)
                         {
                         int32_t fieldNameLen = -1;
                         char *fieldName = NULL;
                         if (node && node->getOpCode().hasSymbolReference() &&
                             node->getSymbolReference()->getSymbol()->isShadow())
                            {
                            fieldName = node->getSymbolReference()->getOwningMethod(vp->comp())->fieldName(node->getSymbolReference()->getCPIndex(), fieldNameLen, vp->comp()->trMemory());
                            }

                         if (fieldName && (fieldNameLen > 0) &&
                             !strncmp(fieldName, "java/util/TreeMap.valuesCollection", 34))
                            {
                            TR_OpaqueClassBlock *clazz = vp->fe()->getClassFromSignature("Ljava/util/TreeMap$2;", 21, method);
                            if (clazz != NULL)
                               {
                               constraint = TR::VPFixedClass::create(vp, clazz);
                               vp->addGlobalConstraint(node, constraint);
                               }
                            }
                         }
                      }
                   }
                }
             }
         }
#endif

      }

#ifdef J9_PROJECT_SPECIFIC
   // don't use getClassFromSignature for arrays to determine the type of the
   // component as its done below later using the right api
   //
   if (sig &&
         !(node->getOpCode().hasSymbolReference() &&
            node->getSymbol()->isArrayShadowSymbol() &&
            node->getFirstChild()->getOpCode().isArrayRef()))
      {
      TR_ResolvedMethod *method = node->getSymbolReference()->getOwningMethod(vp->comp());
      TR_OpaqueClassBlock *classBlock = vp->fe()->getClassFromSignature(sig, len, method);

      if (  classBlock
         && TR::Compiler->cls.isInterfaceClass(vp->comp(), classBlock)
         && !vp->comp()->getOption(TR_TrustAllInterfaceTypeInfo))
         {
         classBlock = NULL;
         }

      if (classBlock)
         {
         TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(classBlock);
         if (jlClass)
            {
            if (classBlock != jlClass)
               {
               constraint = TR::VPClassType::create(vp, sig, len, method, isFixed, classBlock);

               if (*sig == '[' || sig[0] == 'L')
                  {
                  elementSize = arrayElementSize(sig, len, node, vp);
                  if (elementSize != 0)
                     {
                     constraint = TR::VPClass::create(vp, (TR::VPClassType*)constraint, NULL, NULL,
                           TR::VPArrayInfo::create(vp, 0, elementSize == 0 ? TR::getMaxSigned<TR::Int32>() : TR::getMaxSigned<TR::Int32>()/elementSize, elementSize),
                           TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotClassObject));
                     }
                  }
               }
            else
               constraint = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject);
            vp->addGlobalConstraint(node, constraint);
            }
         }
      }
#endif


   if (foundInfo)
      {
      vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, arrLength, arrLength, elementSize));
      }

#ifdef J9_PROJECT_SPECIFIC
   if (node->getOpCode().hasSymbolReference() &&
       node->getSymbol()->isArrayShadowSymbol() &&
       node->getFirstChild()->getOpCode().isArrayRef())
      {
      TR::Node *underlyingArray = node->getFirstChild()->getFirstChild();
      if (vp->comp()->generateArraylets() &&
          underlyingArray &&
          underlyingArray->getOpCode().hasSymbolReference() &&
          underlyingArray->getSymbol()->isArrayletShadowSymbol() &&
          underlyingArray->getFirstChild()->getOpCode().isArrayRef())
         {
         underlyingArray = underlyingArray->getFirstChild()->getFirstChild();
         }

      constraint = vp->getConstraint(underlyingArray, isGlobal);
      if (constraint && constraint->getClass())
         {
         int32_t len;
         const char *arraySig = constraint->getClassSignature(len);
         if (arraySig && *arraySig == '[')
            {
            TR_OpaqueClassBlock *clazz = vp->comp()->fej9()->getComponentClassFromArrayClass(constraint->getClass());

            if (vp->comp()->getMethodHotness() >= scorching)
               {
               int32_t prefetchOffset = vp->comp()->fej9()->findFirstHotFieldTenuredClassOffset(vp->comp(), clazz);

               if (prefetchOffset >= 0)
                  {
                  if (vp->comp()->findPrefetchInfo(node)<0)
                     {
                     TR_Pair<TR::Node, uint32_t> *prefetchInfo =
                           new (vp->comp()->trHeapMemory()) TR_Pair<TR::Node, uint32_t> (node, (uint32_t *)(intptr_t)prefetchOffset);
                     vp->comp()->getNodesThatShouldPrefetchOffset().push_front(prefetchInfo);
                     }
                  }
               }

            /*
            We removed the check "!TR::Compiler->cls.isInterfaceClass(vp->comp(), clazz)"
            since java treats classes and interfaces in the same way
            if they are array elements.
            For example, the following two snippets are both rejected by javac but accepted by a bytecode verifier.
            Both throw an ArrayStoreException at runtime.

            Serializable [] sArray = new Serializable[1];
            sArray = new Object();

            StringBuilder [] sArray = new StringBuilder[1];
            sArray = new String();

            However, if we change sArray to a static
            this line staticField = new Object(); won't produce any exceptions if staticField is Serializable
            but it will be rejected by the bytecode verifier if staticField is of a class type.
            */
            if (clazz)
               {
               TR_OpaqueClassBlock *jlClass = vp->comp()->fej9()->getClassClassPointer(clazz);
               if (jlClass)
                  {
                  if (clazz != jlClass)
                     {
                     int32_t fieldNameLen = -1;
                     char *fieldName = NULL;
                     if (underlyingArray && underlyingArray->getOpCode().hasSymbolReference() &&
                         (underlyingArray->getSymbolReference()->getSymbol()->isStatic() ||
                           underlyingArray->getSymbolReference()->getSymbol()->isShadow()))
                        {
                        if (underlyingArray->getSymbolReference()->getSymbol()->isShadow())
                           fieldName = underlyingArray->getSymbolReference()->getOwningMethod(vp->comp())->fieldName(underlyingArray->getSymbolReference()->getCPIndex(), fieldNameLen, vp->comp()->trMemory());
                        else
                           fieldName = underlyingArray->getSymbolReference()->getOwningMethod(vp->comp())->staticName(underlyingArray->getSymbolReference()->getCPIndex(), fieldNameLen, vp->comp()->trMemory());
                        }

                     if (fieldName && (fieldNameLen > 0) &&
                         (!strncmp(fieldName, "java/math/BigDecimal.CACHE0", 27) ||
                          !strncmp(fieldName, "java/math/BigDecimal.CACHE1", 27) ||
                          !strncmp(fieldName, "java/math/BigDecimal.CACHE2", 27)))
                        constraint = TR::VPFixedClass::create(vp, clazz);
                     else
                        constraint = TR::VPResolvedClass::create(vp, clazz);
                     }
                  else
                     constraint = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject);
                  vp->addGlobalConstraint(node, constraint);
                  }
               }
            }
         }
      }

   TR::VPConstraint * base = vp->getConstraint(node->getFirstChild(), isGlobal);

   if (base && base->getClass() && !vp->comp()->getOption(TR_DisableMarkingOfHotFields) &&
      node->getFirstChild()->getOpCode().hasSymbolReference() &&
      node->getFirstChild()->getSymbol()->isCollectedReference() &&
      vp->_curBlock->getGlobalNormalizedFrequency(vp->comp()->getFlowGraph()) >= TR::Options::_hotFieldThreshold)
      {
      vp->comp()->fej9()->markHotField(vp->comp(), node->getSymbolReference(), base->getClass(), base->isFixedClass());
      }
#endif


  if (node->isNonNull())
      vp->addBlockConstraint(node, TR::VPNonNullObject::create(vp));
   else if (node->isNull())
      vp->addBlockConstraint(node, TR::VPNullObject::create(vp));

   constraint = vp->getConstraint(node, isGlobal);
   if (!vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));

   return node;
   }

TR::Node *constrainStore(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   // storage access here, sync region ends
   // memory barrier required at next critical region
   bool globalStore = false;
   if (!node->getSymbol()->isAutoOrParm())
      {
      globalStore = true;

      if (node->getOpCode().isStore())
         {
         if (node->getSymbolReference() == vp->comp()->getSymRefTab()->findThisRangeExtensionSymRef())
            globalStore = false;
         }
      }

   if (globalStore)
      {
      OMR::ValuePropagation::Relationship *syncRel = vp->findConstraint(vp->_syncValueNumber);
      TR::VPSync *sync = NULL;
      if (syncRel && syncRel->constraint)
         sync = syncRel->constraint->asVPSync();
      if (sync && sync->syncEmitted() == TR_yes)
         {
         vp->addConstraintToList(NULL, vp->_syncValueNumber, vp->AbsoluteConstraint, TR::VPSync::create(vp, TR_maybe), &vp->_curConstraints);
         if (vp->trace())
            {
            traceMsg(vp->comp(), "Setting syncRequired due to node [%p]\n", node);
            }
         }
      else
         {
         if (vp->trace())
            {
            if (sync)
               traceMsg(vp->comp(), "syncRequired is already setup at node [%p]\n", node);
            else
               traceMsg(vp->comp(), "No sync constraint found at node [%p]!\n", node);
            }
         }
      }


   // if the node contains an unsafe symbol reference
   // then don't inject a constraint
   if (containsUnsafeSymbolReference(vp, node) ||
       (node->getSymbol()->isAutoOrParm() &&
        node->storedValueIsIrrelevant()))
      return node;

   // If the child is not the original child, inject an equality constraint
   // between this node and the child.
   //
   TR::Node *valueChild;
   if (node->getOpCode().isIndirect())
      valueChild = node->getSecondChild();
   else
      valueChild = node->getFirstChild();

#ifdef J9_PROJECT_SPECIFIC
   if (valueChild->getType().isBCD()) // sign state constraints are currently only propagated to loads so only constrain on stores vs every BCD node
      valueChild = constrainBCDSign(vp, valueChild);
#endif

   if (vp->getValueNumber(node) != vp->getValueNumber(valueChild))
      vp->addBlockConstraint(node, TR::VPEqual::create(vp, 0), valueChild);
   if (node->getOpCode().isIndirect() &&
       !vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));

   if (node->getOpCode().isIndirect())
      constrainBaseObjectOfIndirectAccess(vp, node);

   return node;
   }

// Handles istore, bstore, cstore, sstore
//
TR::Node *constrainIntStore(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainStore(vp, node);

   // See if this is the store that is incrementing an induction variable
   //
   vp->checkForInductionVariableIncrement(node);

   // See if the store is a local boolean negation that can be eliminated.
   // If so, don't treat it as a store.
   //
   TR::Node *child = node->getFirstChild();
   TR::Symbol *storeSymbol = node->getSymbol();

   if (child->getOpCodeValue() == TR::ixor &&
       child->getSecondChild()->getOpCodeValue() == TR::iconst &&
       child->getSecondChild()->getInt() == 1)
      {
      TR::Node *loadNode = child->getFirstChild();
      if (loadNode->getOpCode().isLoadVarDirect() && loadNode->getSymbol() == storeSymbol)
         {
         // This is a boolean negate.
         //
         // See if there is an active boolean negation in the current block
         // whose store is the same value as the load of this negation.
         // If so, this negation can be replaced by the first one's load node
         //
         int32_t loadVN = vp->getValueNumber(loadNode);
         OMR::ValuePropagation::BooleanNegationInfo *bni;
         for (bni = vp->_booleanNegationInfo.getFirst(); bni; bni = bni->getNext())
            {
            if (bni->_storeValueNumber == loadVN)
               {
               // Found a matching boolean negation
               //
               if (performTransformation(vp->comp(), "%sRemoving double boolean negation at [%p]\n", OPT_DETAILS, node))
                  {
                  bni->_loadNode->incReferenceCount();
                  vp->removeChildren(node, true);
                  node->setFirst(bni->_loadNode);
                  node->setNumChildren(1);
                  vp->addBlockConstraint(node, TR::VPEqual::create(vp, 0), bni->_loadNode);
                  return node;
                  }
               }
            }

         // Mark this node as an active boolean negation
         //
         bni = new (vp->trStackMemory()) OMR::ValuePropagation::BooleanNegationInfo;
         bni->_storeValueNumber = vp->getValueNumber(node);
         bni->_loadNode = loadNode;
         vp->_booleanNegationInfo.add(bni);
         }
      }
   return node;
   }

TR::Node *constrainLongStore(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainStore(vp, node);

   // See if this is the store that is incrementing an induction variable
   //
   vp->checkForInductionVariableIncrement(node);
   return node;
   }

// Also handles iastore
//
TR::Node *constrainAstore(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainStore(vp, node);

   // See if there is a constraint for this value
   //
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (constraint)
      {
      if (constraint->isNullObject())
         node->setIsNull(true);
      else if (constraint->isNonNullObject())
         node->setIsNonNull(true);

      // for localVP, check if the value being stored is
      // consistent with the info on the parm
      //
      vp->invalidateParmConstraintsIfNeeded(node, constraint);
      }
   return node;
   }

void canRemoveWrtBar(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // If the value being stored is known to be null we can replace the write
   // barrier store with a regular store
   //
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node, isGlobal);
   if (constraint)
      {
      if (constraint->isNullObject() && !vp->comp()->getOptions()->alwaysCallWriteBarrier() && !vp->comp()->getOptions()->realTimeGC())
         {
         if (node->getOpCode().isIndirect())
            {
            if (performTransformation(vp->comp(), "%sChanging write barrier store into iastore [%p]\n", OPT_DETAILS, node))
               {
               bool invalidateInfo = false;
               if (node->getChild(2) != node->getFirstChild())
                  invalidateInfo = true;

               TR::Node::recreate(node, TR::astorei);
               node->getChild(2)->recursivelyDecReferenceCount();
               node->setNumChildren(2);
               node->setIsNull(true);
               if (invalidateInfo)
                  {
                  vp->invalidateUseDefInfo();
                  vp->invalidateValueNumberInfo();
                  }
               }
            }
         else
            {
            if (performTransformation(vp->comp(), "%sChanging write barrier store into astore [%p]\n", OPT_DETAILS, node))
               {
               TR::Node::recreate(node, TR::astore);
               node->getChild(1)->recursivelyDecReferenceCount();
               node->setNumChildren(1);
               node->setIsNull(true);
               vp->invalidateUseDefInfo();
               vp->invalidateValueNumberInfo();
               }
            }
         }
      else if (constraint->isNonNullObject())
         node->setIsNonNull(true);
      }
   }

// Also handles iwrtbar
//
TR::Node *constrainWrtBar(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   if (node->getOpCode().isIndirect())
      {
      constrainBaseObjectOfIndirectAccess(vp, node);
      // if the node contains an unsafe symbol reference
      // then don't inject a constraint
      if (containsUnsafeSymbolReference(vp, node))
         return node;
      }

   // The case of ArrayStoreCHK will be taken care in constrainArrayStoreChk().
   //
   if (!vp->getCurrentParent() ||
        vp->getCurrentParent() && vp->getCurrentParent()->getOpCodeValue() != TR::ArrayStoreCHK)
      canRemoveWrtBar(vp, node);

   static bool doOpt = feGetEnv("TR_DisableWrtBarOpt") ? false : true;

   if (TR::Compiler->om.shouldGenerateReadBarriersForFieldLoads())
      {
      // TODO (GuardedStorage): Why do we need this restriction?
      doOpt = false;
      }

   TR_WriteBarrierKind gcMode = vp->comp()->getOptions()->getGcMode();

   if (doOpt &&
       ((gcMode == TR_WrtbarCardMarkAndOldCheck) ||
        (gcMode == TR_WrtbarOldCheck)) &&
       (node->getOpCodeValue() == TR::wrtbari) &&
       !node->skipWrtBar())
      {
      TR::Node *valueChild = node->getFirstChild();
      if (valueChild->getOpCode().isArrayRef())
         valueChild = valueChild->getFirstChild();

      bool removeWrtBar = false;

      if ((valueChild->getOpCodeValue() == TR::New) ||
          (valueChild->getOpCodeValue() == TR::newarray) ||
          (valueChild->getOpCodeValue() == TR::anewarray) ||
          (valueChild->getOpCodeValue() == TR::multianewarray) ||
          (valueChild && (valueChild->getOpCodeValue() == TR::loadaddr) &&
           valueChild->getSymbolReference()->getSymbol()->isAuto() &&
           valueChild->getSymbolReference()->getSymbol()->isLocalObject()))
         removeWrtBar = true;
      else if (valueChild->getOpCodeValue() == TR::aload)
         {
         TR_UseDefInfo *useDefInfo = vp->_useDefInfo;

         if (valueChild->isThisPointer() &&
             vp->comp()->getJittedMethodSymbol()->getResolvedMethod()->isConstructor())
            {
            removeWrtBar = true;

            if (vp->_changedThis == TR_maybe)
               {
               vp->_changedThis = TR_no;
               TR::TreeTop *tt = vp->comp()->getStartTree();
               for (; tt; tt = tt->getNextTreeTop())
                  {
                  TR::Node *storeNode = tt->getNode()->getStoreNode();
                  if (storeNode &&
                      (storeNode->getSymbolReference() == valueChild->getSymbolReference()))
                     {
                     vp->_changedThis = TR_yes;
                     removeWrtBar = false;
                     break;
                     }
                  }
               }
            else if (vp->_changedThis == TR_yes)
               removeWrtBar = false;
            }
         else if (useDefInfo)
            {
            uint16_t useIndex = valueChild->getUseDefIndex();
            if (useDefInfo->isUseIndex(useIndex))
               {
               TR_UseDefInfo::BitVector defs(vp->comp()->allocator());
               useDefInfo->getUseDef(defs, useIndex);
               if (!defs.IsZero())
                  {
                  TR_UseDefInfo::BitVector::Cursor cursor(defs);
                  for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                     {
                     int32_t defIndex = cursor;

                     if (defIndex < useDefInfo->getFirstRealDefIndex())
                        {
                        removeWrtBar = false;
                        break;
                        }

                     TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);
                     TR::Node *defNode = defTree->getNode();

                     if (defNode &&
                         (defNode->getOpCodeValue() == TR::astore) &&
                         (defNode->getNumChildren() > 0))
                        {
                        TR::Node *defChild = defNode->getFirstChild();

                      if ((defChild->getOpCodeValue() == TR::New) ||
                          (defChild->getOpCodeValue() == TR::newarray) ||
                          (defChild->getOpCodeValue() == TR::anewarray) ||
                          (defChild->getOpCodeValue() == TR::multianewarray) ||
                          (defChild && (defChild->getOpCodeValue() == TR::loadaddr) &&
                           defChild->getSymbolReference()->getSymbol()->isAuto() &&
                           defChild->getSymbolReference()->getSymbol()->isLocalObject()))
                          removeWrtBar = true;
                      /*
                       else if (defChild && (defChild->getOpCodeValue() == TR::aload))
                          {
                          TR::SymbolReference *symRef = defChild->getSymbolReference();
                          if (symRef)
                             {
                             TR::Symbol *sym = symRef->getSymbol();

                             if (!(sym->getKind() == TR::Symbol::IsStatic && sym->isConstString()))
                                 {
                                 removeWrtBar = false;
                                 break;
                                 }
                              else
                                removeWrtBar = true;
                             }
                          }
                      */
                       else
                          {
                          removeWrtBar = false;
                          break;
                          }
                        }
                     }
                  }
               }
            }
         }

      if (removeWrtBar &&
         performTransformation(vp->comp(), "%sChanging wrtbar to store because the rhs is a new object [%p]\n", OPT_DETAILS, node))
         {
         node->setSkipWrtBar(true);
         }
      }



   // If node is still a write barrier then let's find if it's coming from a new in the same method.
   // If that's the case then the chances are this wrtbar is going to be on a new object and we should
   // let the codegen know so it can generate better sequence.
   if ((node->getOpCodeValue() == TR::wrtbari) &&
       !node->getSymbolReference()->getSymbol()->isArrayShadowSymbol() &&
       !vp->comp()->getOption(TR_DisableWriteBarriersRangeCheck))
      {
      TR::Node *objectChild = node->getFirstChild();
      bool setStackWrtBar = false;
      if ((objectChild->getOpCodeValue() == TR::aload) &&
          objectChild->getSymbolReference()->getSymbol()->isAuto())
         {
         TR_UseDefInfo *useDefInfo = vp->_useDefInfo;

         if (useDefInfo)
            {
            uint16_t useIndex = objectChild->getUseDefIndex();
            if (useDefInfo->isUseIndex(useIndex))
               {
               TR_UseDefInfo::BitVector defs(vp->comp()->allocator());
               useDefInfo->getUseDef(defs, useIndex);
               if (!defs.IsZero())
                  {
                  TR_UseDefInfo::BitVector::Cursor cursor(defs);
                  for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                     {
                     int32_t defIndex = cursor;

                     if (defIndex < useDefInfo->getFirstRealDefIndex())
                        continue;

                     TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);
                     TR::Node *defNode = defTree->getNode();

                     if (defNode && (defNode->getOpCodeValue() == TR::astore) &&
                        (defNode->getNumChildren() > 0))
                        {
                        TR::Node *defChild = defNode->getFirstChild();

                        if (defChild && (defChild->getOpCodeValue() == TR::loadaddr) &&
                            defChild->getSymbolReference()->getSymbol()->isAuto() &&
                            defChild->getSymbolReference()->getSymbol()->isLocalObject())
                           {
                           setStackWrtBar = true;
                           break;
                           }
                        else if (defChild && (defChild->getOpCodeValue() == TR::aload) &&
                                 defChild->getSymbolReference()->getSymbol()->isAuto())
                           {
                           uint16_t useIndexA = defChild->getUseDefIndex();
                           if (useDefInfo->isUseIndex(useIndexA))
                              {
                              TR_UseDefInfo::BitVector defsA(vp->comp()->allocator());
                              useDefInfo->getUseDef(defsA, useIndexA);
                              if (!defsA.IsZero())
                                 {
                                 TR_UseDefInfo::BitVector::Cursor cursorA(defsA);
                                 for (cursorA.SetToFirstOne(); cursorA.Valid(); cursorA.SetToNextOne())
                                    {
                                    int32_t defIndexA = cursorA;
                                    if (defIndexA < useDefInfo->getFirstRealDefIndex())
                                       continue;

                                    TR::TreeTop *defTreeA = useDefInfo->getTreeTop(defIndexA);
                                    TR::Node *defNodeA = defTreeA->getNode();
                                    if (defNodeA && (defNodeA->getOpCodeValue() == TR::astore) &&
                                       (defNodeA->getNumChildren() > 0))
                                       {
                                       TR::Node *defChildA = defNodeA->getFirstChild();
                                       if (defChildA && (defChildA->getOpCodeValue() == TR::loadaddr) &&
                                           defChildA->getSymbolReference()->getSymbol()->isAuto() &&
                                           defChildA->getSymbolReference()->getSymbol()->isLocalObject())
                                          {
                                          setStackWrtBar = true;
                                          break;
                                          }
                                       }
                                    }
                                 }
                              }
                           }
                        }
                     }

                  if (setStackWrtBar &&
                      performTransformation(vp->comp(), "%sSetting wrtbar flag to assume stack object [%p]\n", OPT_DETAILS, node))
                     {
                     node->setLikelyStackWrtBar(true);
                     }
                  }
               }
            }
         }
      }

   // If the node is still a write barrier - we can mark it to specify the location of the dest object
   //
   if (node->getOpCode().isWrtBar())
      {
      TR::Node *destinationChild = node->getOpCode().isIndirect() ? node->getChild(2) : node->getChild(1);
      bool isGlobal;
      TR::VPConstraint *constraint = vp->getConstraint(node, isGlobal);
      if (constraint)
         {
         if (constraint->isHeapObject() == TR_yes &&
             performTransformation(vp->comp(), "%sMarking the wrtbar node [%p] - destination is a heap object\n", OPT_DETAILS, node))
            {
            //printf("--wbar-- heap wrtbar in %s\n", vp->comp()->signature());
            node->setIsHeapObjectWrtBar(true);
            }
         else if (constraint->isHeapObject() == TR_no &&
                  performTransformation(vp->comp(), "%sMarking the wrtbar node [%p] - destination is a non-heap object\n", OPT_DETAILS, node))
            {
            //printf("--wbar-- nonheap wrtbar in %s\n", vp->comp()->signature());
            node->setIsNonHeapObjectWrtBar(true);
            }
         //else
         // printf("--wbar-- no idea in %s\n", vp->comp()->signature());
         }
      }

   return node;
   }

TR::Node *constrainGoto(OMR::ValuePropagation *vp, TR::Node *node)
   {


   // Put the current list of block constraints on to the edge
   //
   TR::Block *target = node->getBranchDestination()->getNode()->getBlock();
   if (vp->trace())
      traceMsg(vp->comp(), "   unconditional branch on node %s (%p), vp->_curBlock block_%d target block_%d\n",
         node->getOpCode().getName(),node, vp->_curBlock->getNumber(), target->getNumber());

   // Find the output edge from the current block that corresponds to this
   // branch
   //
   TR::CFGEdge *edge = vp->findOutEdge(vp->_curBlock->getSuccessors(), target);
   vp->printEdgeConstraints(vp->createEdgeConstraints(edge, false));
   vp->setUnreachablePath();

   return node;
   }

TR::Node *constrainIgoto(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // Remove the current list of block constraints so they don't fall through
   //
   constrainChildren(vp, node);

   bool canFallThrough = vp->_curBlock->getNextBlock() &&
      vp->_curBlock->getNextBlock()->isExtensionOfPreviousBlock();

   for (auto edge= vp->_curBlock->getSuccessors().begin(); edge != vp->_curBlock->getSuccessors().end(); ++edge)
      {
	  auto current = edge;
      bool keepConstraints = (++current != vp->_curBlock->getSuccessors().end()) || canFallThrough;
      vp->printEdgeConstraints(vp->createEdgeConstraints(*edge, keepConstraints));
      }

   if (!canFallThrough)
      vp->setUnreachablePath();
   return node;
   }

TR::Node *constrainReturn(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (node->getDataType() == TR::Address)
      vp->addGlobalConstraint(node, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotStackObject));

   // Remove the current list of block constraints so they don't fall through
   //
   constrainChildren(vp, node);
   vp->setUnreachablePath();
   return node;
   }

TR::Node *constrainMonent(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // After the monitor enter the child must be non-null
   //
   constrainChildren(vp, node);
   vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));

   // If the child is a resolved class type, hide the class pointer in the
   // second child
   //
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (constraint && constraint->getClass())
      {
      TR_OpaqueClassBlock *clazz = constraint->getClass();
      if (constraint->isClassObject() == TR_yes)
         clazz = vp->fe()->getClassClassPointer(clazz);

      if (clazz && (TR::Compiler->cls.classDepthOf(clazz) == 0) &&
          !constraint->isFixedClass())
         clazz = NULL;

      if (node->hasMonitorClassInNode() &&
           clazz &&
          (node->getMonitorClassInNode() != clazz))
         {
         TR_YesNoMaybe answer = vp->fe()->isInstanceOf(clazz, node->getMonitorClassInNode(), true, true);
         if (answer != TR_yes)
            clazz = node->getMonitorClassInNode();
         }

      if ((clazz || !node->hasMonitorClassInNode()) && (performTransformation(vp->comp(), "%sSetting type on MONENTER node [%p] to [%p]\n", OPT_DETAILS, node, clazz)))
         node->setMonitorClassInNode(clazz);
      }
   return node;
   }

TR::Node *constrainMonexit(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   // Propagate constraints so far to the exception edges
   //
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchMonitorExit, NULL, node);

   // After the monitor exit the child must be non-null
   //
   vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));

   // If the child is a resolved class type, hide the class pointer in the
   // second child
   //
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (constraint && constraint->getClass())
      {
      TR_OpaqueClassBlock *clazz = constraint->getClass();
      if (constraint->isClassObject() == TR_yes)
         clazz = vp->fe()->getClassClassPointer(clazz);

      if (clazz && (TR::Compiler->cls.classDepthOf(clazz) == 0) &&
          !constraint->isFixedClass())
         clazz = NULL;

      if ( node->hasMonitorClassInNode() &&
           clazz &&
          (node->getMonitorClassInNode() != clazz))
         {
         TR_YesNoMaybe answer = vp->fe()->isInstanceOf(clazz, node->getMonitorClassInNode(), true, true);
         if (answer != TR_yes)
            clazz = node->getMonitorClassInNode();
         }

      if ((clazz || !node->hasMonitorClassInNode()) &&
          performTransformation(vp->comp(), "%sSetting type on MONEXIT  node [%p] to [%p]\n", OPT_DETAILS, node, clazz))
         node->setMonitorClassInNode(clazz);
      }


   // check for global writes and sync
   //
   // find a constraint for sync in the block
   OMR::ValuePropagation::Relationship *syncRel = vp->findConstraint(vp->_syncValueNumber);
   TR::VPSync *sync = NULL;
   if (syncRel && syncRel->constraint)
      sync = syncRel->constraint->asVPSync();
   bool syncRequired = false, syncReset = false;
   if (sync)
      {
      if (sync->syncEmitted() == TR_no)
         {
         syncRequired = true;
         if (vp->trace())
            traceMsg(vp->comp(), "Going to emit sync at monexit [%p]\n", node);
         }
      else if (sync->syncEmitted() == TR_yes)
         {
         syncReset = true;
         node->setSkipSync(true);
         if (vp->trace())
            traceMsg(vp->comp(), "syncRequired is already setup at monexit [%p]\n", node);
         }
      vp->comp()->setSyncsMarked();
      }
   else
      {
      if (vp->trace())
         {
         if (sync)
            traceMsg(vp->comp(), "syncRequired is already setup at monexit [%p]\n", node);
         else
            traceMsg(vp->comp(), "No sync constraint found at monexit [%p]!\n", node);
         }
      }

   ////if (!node->isReadMonitor() || syncRequired)
   if (syncRequired)
      {
      node->setSkipSync(false);
      // create a constraint indicating a sync has been emitted
      if (!syncReset)
         vp->addConstraintToList(NULL, vp->_syncValueNumber, vp->AbsoluteConstraint, TR::VPSync::create(vp, TR_maybe), &vp->_curConstraints);
      if (vp->trace())
         {
         traceMsg(vp->comp(), "Resetting syncRequired at monexit [%p]\n", node);
         }
      }

   return node;
   }

TR::Node *constrainMonexitfence(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp,node);

   // Propagate constraints so far to the exception edges
   //
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchMonitorExit, NULL, node);

   return node;
   }

TR::Node *constrainTstart(OMR::ValuePropagation *vp, TR::Node *node)
   {
   //TR_ASSERT(0, "Not implemented!");
   constrainChildren(vp,node);
   vp->setUnreachablePath(); // no fallthrough
   return node;
   }

TR::Node *constrainTfinish(OMR::ValuePropagation *vp, TR::Node *node)
   {
   //TR_ASSERT(0, "Not implemented!");
   constrainChildren(vp,node);
   return node;
   }

TR::Node *constrainTabort(OMR::ValuePropagation *vp, TR::Node *node)
   {
   //TR_ASSERT(0, "Not implemented!");
   constrainChildren(vp,node);
   return node;
   }

TR::Node *constrainThrow(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   if (node->throwInsertedByOSR())
      {
      // Propagate constraints so far to the exception edges
      //
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchUserThrows, NULL, node);
      vp->setUnreachablePath();
      return node;
      }

   // See if the throw can be coverted to a goto
   //
   TR::Block * predictedCatchBlock = NULL;
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);

   if (debug("traceThrowToGoto"))
      {
      int32_t len = 1;
      const char *sig = constraint ? constraint->getClassSignature(len) : "?";
      printf("\n\n throw type %.*s\n", len, sig);
      }

   TR_OrderedExceptionHandlerIterator oehi(vp->_curBlock, vp->comp()->trMemory()->currentStackRegion());
   for (TR::Block *catchBlock = oehi.getFirst(); catchBlock; catchBlock = oehi.getNext())
      {
      if (catchBlock->isOSRCatchBlock())
         continue;

      TR_YesNoMaybe willCatch = TR_maybe;
      if (catchBlock->getCatchType() == 0)
         {
         willCatch = TR_yes;
         if (debug("traceThrowToGoto"))
            printf("\n\n catch type yes ...\n");
         }
      else if (!constraint || !constraint->getClass())
         {
         if (debug("traceThrowToGoto"))
            printf("\n\n thrown type unknown\n");
         }
      else if (catchBlock->getExceptionClass())
         {
         willCatch = vp->fe()->isInstanceOf(constraint->getClass(), catchBlock->getExceptionClass(), constraint->isFixedClass());
         if (willCatch == TR_yes)
            vp->registerPreXClass(constraint);

         if (debug("traceThrowToGoto"))
            printf("\n\n catch type %s %.*s\n",
                   (willCatch == TR_yes ? "yes" : (willCatch == TR_no ? "no" : "maybe")),
                   catchBlock->getExceptionClassNameLength(), catchBlock->getExceptionClassNameChars());
         }
      else if (debug("traceThrowToGoto"))
         printf("\n\n catch type maybe unresolved class\n");

      if (willCatch == TR_yes)
         {
         predictedCatchBlock = catchBlock;
         break;
         }
      if (willCatch != TR_no && !debug("aggressiveThrowToGoto"))
         break;
      }

   if (debug("traceThrowToGoto"))
      fflush(stdout);

   if (predictedCatchBlock && !vp->comp()->getOption(TR_DisableThrowToGoto))
      {
      // Temporarily set the throw's second child to be the predicted catch
      // block.
      // Later the throw will be changed to a goto.  This can't be done here
      // because it may require the creation of a new block which screws up
      // our walk through the structure
      //
      node->setSecond((TR::Node *)predictedCatchBlock);
      TR_Pair<TR::Node, TR::Block> *newPredictedThrow = new (vp->trStackMemory()) TR_Pair<TR::Node, TR::Block> (node, vp->_curBlock);
      vp->_predictedThrows.add(newPredictedThrow);
      }
   // Propagate constraints so far to the exception edges
   //
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchUserThrows, NULL, node);
   vp->setUnreachablePath();
   return node;
   }

TR::Node *constrainInstanceOf(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   // If the instance object is null, this node becomes a constant 0.
   //
   // If the instance object is non-null and provably an instance of the given
   // class type, this node becomes a constant 1.
   //
   // If the instance object is provably not an instance of the given class
   // type, this node becomes a constant 0.
   //
   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *constraint = NULL;
   TR::VPConstraint *objectConstraint = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *castConstraint   = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;

   if (objectConstraint)
      {
      int32_t result = -1;
      if (objectConstraint->isNullObject())
         {
         result = 0;
         }
      else if (objectConstraint->getClassType() && castConstraint &&
               castConstraint->isFixedClass() &&
               objectConstraint->getClassType() == castConstraint->getClassType() &&
               objectConstraint->isNonNullObject()&&
               objectConstraint->isClassObject() != TR_yes)
         {
         result = 1;
         }
      else if (objectConstraint->getClass() &&
               castConstraint && castConstraint->getClass())
         {
         TR_OpaqueClassBlock *objectClass = objectConstraint->getClass();
         TR_OpaqueClassBlock *castClass   = castConstraint->getClass();

         bool disabled=vp->comp()->getOption(TR_DisableAOTInstanceOfInlining);

         TR_YesNoMaybe isInstance =
               vp->fe()->isInstanceOf(objectClass, castClass,
                            objectConstraint->isFixedClass(), castConstraint->isFixedClass(), !disabled);

         if (isInstance == TR_yes)
            {

            if (objectConstraint->isNonNullObject())
               {
               if (castConstraint->isFixedClass())
                  {
                  vp->registerPreXClass(objectConstraint);
                  if (objectConstraint->isClassObject() != TR_yes)
                     result = 1;
                  }
               }
            else
               {
               TR::Node::recreate(node, TR::acmpne);
               vp->removeNode(node->getChild(1), true);
               node->setAndIncChild(1, TR::Node::create(node, TR::aconst, 0, 0));
               }



            }
         else if (isInstance == TR_no)
            {
            vp->registerPreXClass(objectConstraint);
            if (objectConstraint->asClass() && castConstraint->asClass())
               {
               vp->checkTypeRelationship(objectConstraint, castConstraint, result, true, false);
               //FIXME: setting isInstance=maybe is a bit conservative
               // read comments for constrainCheckcast
               //
               if (result != 0)
                  {
                  isInstance = TR_maybe;

                  // if the cast class is java/lang/Class
                  // then the object might be an instance and
                  // we can possibly eliminate the instanceof
#if 0
                  TR::VPClassType *castClassType = castConstraint->getClassType();
                  if (castClassType && castClassType->asResolvedClass() &&
                        (castConstraint->isClassObject() == TR_yes))
                     {
                     TR::VPResolvedClass *rc = castClassType->asResolvedClass();
                     if (rc->getClass() == vp->fe()->getClassClassPointer(rc->getClass()))
                        {
                        result = 1;
                        isInstance = TR_yes;
                        }
                     }
#endif
                  }
               }
            else
            result = 0;
            }
         }
      else
         {
         if (castConstraint)
            {
            if (objectConstraint->asClass() && castConstraint->asClass())
               {
               vp->checkTypeRelationship(objectConstraint, castConstraint, result, true, false);
               }
            else
               {
               TR_YesNoMaybe castIsClassObject = vp->isCastClassObject(castConstraint->getClassType());
               if ((castIsClassObject == TR_no) &&
                     !objectConstraint->getClassType() &&
                   //objectConstraint->isNonNullObject() &&
                     (objectConstraint->isClassObject() == TR_yes))
                  {
                  result = 0;
                  if (vp->trace())
                     traceMsg(vp->comp(), "object is a classobject but cast is not java/lang/Class\n");
                  }
               else if ((castIsClassObject == TR_no) &&
                        !objectConstraint->getClassType() &&
                        //objectConstraint->isNonNullObject() &&
                        (objectConstraint->isClassObject() == TR_no))
                  {
                  }
               else if ((castIsClassObject == TR_yes) &&
                        !objectConstraint->getClassType() &&
                        //objectConstraint->isNonNullObject() &&
                        (objectConstraint->isClassObject() == TR_no))
                  {
                  result = 0;
                  if (vp->trace())
                     traceMsg(vp->comp(), "object is not a classobject but cast is java/lang/Class\n");
                  }
               // probably cannot get here
               //
               else if ((castIsClassObject == TR_yes) &&
                        !objectConstraint->getClassType() &&

                        (objectConstraint->isClassObject() == TR_yes))
                  {


                  if (objectConstraint->isNonNullObject())
                     {
                     result = 1;
                     if (vp->trace())
                        traceMsg(vp->comp(), "object is a non-null classobject and cast is java/lang/Class\n");
                     }
                  else
                     {
                  TR::Node::recreate(node, TR::acmpne);
                  vp->removeNode(node->getChild(1), true);
                  node->setAndIncChild(1, TR::Node::create(node, TR::aconst, 0, 0));
                     }


                  }
               else
                  {
                  TR::VPConstraint *intersectConstraint = castConstraint->getClassType();
                  if (intersectConstraint)
                     {
                     if (intersectConstraint->asFixedClass())
                        intersectConstraint = TR::VPResolvedClass::create(vp, intersectConstraint->getClass());
                     if (!objectConstraint->intersect(intersectConstraint, vp))
                        result = 0;
                     }
                  }
               }
            }
         }

      if (result >= 0)
         {
         constraint = TR::VPIntConst::create(vp, result);
         vp->replaceByConstant(node, constraint, lhsGlobal);
         return node;
         }
      }

   constraint = TR::VPIntRange::create(vp, 0, 1);
   vp->addGlobalConstraint(node, constraint);
   return node;
   }

TR::Node *constrainCheckcast(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   // If the object is null, this node can be removed.
   //
   // If the object is provably an instance of the given class type, this node
   // can be removed.
   //
   // If the object is non-null and provably not an instance of the given
   // class type, the rest of the block can be removed, since the exception
   // will always be taken.
   //
   bool isGlobal;
   TR::VPConstraint *objectConstraint = vp->getConstraint(node->getFirstChild(), isGlobal);
   TR::VPConstraint *castConstraint   = vp->getConstraint(node->getSecondChild(), isGlobal);
   TR_YesNoMaybe isInstance = TR_maybe;
   int32_t result = -1;

   if (objectConstraint)
      {
      if (objectConstraint->isNullObject())
         {
         result = 1;
         }
      else if (objectConstraint == castConstraint &&
                 objectConstraint->isClassObject() != TR_yes)
         {
         result = 1;
         }
      else if (objectConstraint->getClass() &&
               castConstraint && castConstraint->getClass())
         {
         TR_OpaqueClassBlock *objectClass = objectConstraint->getClass();
         TR_OpaqueClassBlock *castClass   = castConstraint->getClass();

         bool disabled=vp->comp()->getOption(TR_DisableAOTCheckCastInlining);

         isInstance = vp->fe()->isInstanceOf(objectClass, castClass,
                        objectConstraint->isFixedClass(), castConstraint->isFixedClass(), !disabled);

         if (isInstance == TR_yes)
            {
            if (castConstraint->isFixedClass())
               {
               vp->registerPreXClass(objectConstraint);
               if (objectConstraint->isClassObject() != TR_yes)
                  result = 1;
               }
            }
         else if (isInstance == TR_no &&
                  (objectConstraint->isNonNullObject() ||
                   TR::Compiler->cls.isPrimitiveClass(vp->comp(), objectConstraint->getClass())))
            {
            vp->registerPreXClass(objectConstraint);
            // this is to handle the case when either type is java/lang/Class
            //
            if (objectConstraint->asClass() && castConstraint->asClass() &&
                  objectConstraint->isNonNullObject())
               {
               vp->checkTypeRelationship(objectConstraint, castConstraint, result, false, true);
               // reset isInstance here so that wrong constraints
               // are not propagated below
               //
               // FIXME: resetting to TR_maybe is a bit conservative
               // consider what happens for the following case
               // (A)Class
               // ie. checkcast
               //          aload Class
               //          loadaddr A
               //
               // in this case, we would have eliminated the rest of the
               // block; but setting isInstance=maybe prevents that
               // from happening
               //
               if (result != 0)
                  {
                  isInstance = TR_maybe;
                  }
               }
            else
               result = 0;
            }
         }
      else
         {
         if (castConstraint && objectConstraint->isNonNullObject())
            {
            if (objectConstraint->asClass() && castConstraint->asClass())
               {
               vp->checkTypeRelationship(objectConstraint, castConstraint, result, false, true);
               }
            else
               {
               TR::VPConstraint *intersectConstraint = castConstraint;
               TR_YesNoMaybe castIsClassObject = vp->isCastClassObject(castConstraint->getClassType());
               if (objectConstraint->asClassType() && castConstraint->asClass() && castConstraint->asClass()->getClassType())
                  {
                  TR::VPClassType *type = castConstraint->asClass()->getClassType();
                  TR::VPClassType *dilutedType;
                  if (type && type->asFixedClass())
                     dilutedType = TR::VPResolvedClass::create(vp, type->getClass());
                  else
                     dilutedType = type;

                  if (objectConstraint->isClassObject() == TR_yes)
                     intersectConstraint = TR::VPClass::create(vp, dilutedType, NULL, NULL, NULL, TR::VPObjectLocation::create(vp, TR::VPObjectLocation::ClassObject));
                  else
                     intersectConstraint = dilutedType;
                  }

               // no need to check for nullObject here since
               // objectConstraint is guaranteed to be a non-nullobject
               // due to the test above

               if ((castIsClassObject == TR_no) &&
                     !objectConstraint->getClassType() &&
                     (objectConstraint->isClassObject() == TR_yes))
                  {
                  result = 0;
                  if (vp->trace())
                     traceMsg(vp->comp(), "object is a classobject but cast is not java/lang/Class\n");
                  }
               else if ((castIsClassObject == TR_no) &&
                        !objectConstraint->getClassType() &&
                        //objectConstraint->isNonNullObject() &&
                        (objectConstraint->isClassObject() == TR_no))
                  {
                  }
               else if ((castIsClassObject == TR_yes) &&
                        !objectConstraint->getClassType() &&
                        //objectConstraint->isNonNullObject() &&
                        (objectConstraint->isClassObject() == TR_no))
                  {
                  result = 0;
                  if (vp->trace())
                     traceMsg(vp->comp(), "object is not a classobject but cast is java/lang/Class\n");
                  }
               // probably cannot get here
               //
               else if ((castIsClassObject == TR_yes) &&
                        !objectConstraint->getClassType() &&
                        //objectConstraint->isNonNullObject() &&
                        (objectConstraint->isClassObject() == TR_yes))
                  {
                  result = 1;
                  if (vp->trace())
                     traceMsg(vp->comp(), "object is a non-null classobject and cast is java/lang/Class\n");
                  }
               else if (!objectConstraint->intersect(intersectConstraint, vp))
                  result = 0;
               }
            }
         }
#if 0
      else if (castConstraint && !objectConstraint->intersect(castConstraint, vp) && objectConstraint->isNonNullObject())
         {
         result = 0;
         }
#endif
      }

   if (result == 1 &&
       ((node->getOpCodeValue() != TR::checkcastAndNULLCHK) ||
        (objectConstraint && objectConstraint->isNonNullObject())) &&
       performTransformation(vp->comp(), "%sRemoving redundant checkcast node [%p]\n", OPT_DETAILS, node))
      {
      TR_ASSERT(node->getReferenceCount() == 0, "CheckCast has references");

      // Change the checkcast node into a treetop node
      //
      TR::Node *classNode = node->getSecondChild();
      vp->optimizer()->getEliminatedCheckcastNodes().add(node);
      vp->optimizer()->getClassPointerNodes().add(classNode);
      TR::Node::recreate(node, TR::treetop);
      node->setNumChildren(1);
      vp->removeNode(classNode);
      vp->setChecksRemoved();
      return node;
      }

   if (result != 1)
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchCheckCast, NULL, node);
   if ((node->getOpCodeValue() == TR::checkcastAndNULLCHK) &&
       (!objectConstraint || !objectConstraint->isNonNullObject()))
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchNullCheck, NULL, node);

   bool exceptionTaken = false;
   if ((result == 0) ||
       ((node->getOpCodeValue() == TR::checkcastAndNULLCHK) &&
        (objectConstraint && objectConstraint->isNullObject() ||
         (isInstance == TR_no ))))
      {
      // Everything past this point in the block is dead, since the
      // exception will always be taken.
      //
      exceptionTaken = true;
      vp->mustTakeException();
      }

   else if (castConstraint)
      {
      // We can constrain the object by the type of the class
      //
      TR::VPClassType *classType = castConstraint->getClassType();
      if (classType)
         {
         if (isInstance == TR_no)
            vp->addBlockConstraint(node->getFirstChild(), TR::VPNullObject::create(vp));
         // refine the type..commenting out the test below
         // because its conservative
         else ////if (isInstance == TR_yes)
            {
            TR::VPClassType *newClassType;
            bool castIsClassObject = false;
            // AOT returns null for getClassClass currently,
            // so don't propagate type
            //
            bool applyType = false;
            if (classType->asResolvedClass())
               {
               TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(classType->getClass());
               if (jlClass)
                  {
                  applyType = true;
                  if (classType->getClass() == jlClass)
                     {
                     if (objectConstraint && objectConstraint->getClassType())
                        newClassType = TR::VPResolvedClass::create(vp, (TR_OpaqueClassBlock *)VP_SPECIALKLASS);
                     else
                        newClassType = NULL;
                     castIsClassObject = true;
                     }
                  else
                     {
                     // check for interfaces of Class
                     if ((classType->isClassObject() == TR_maybe) &&
                         (objectConstraint && (objectConstraint->isClassObject() == TR_yes)))
                        {
                           // not adding any new information about the type
                           // and don't want to get classobject property on one of
                           // the interfaces implemented by Class
                           //
                           applyType = false;
                        }
                     else
                        newClassType = TR::VPResolvedClass::create(vp, classType->getClass()); // Do not want to get the 'fixed' property
                     }
                  }
               }
            else
               {
               newClassType = classType;
               applyType = true;
               }

            if (applyType)
               {
               TR::VPConstraint *newTypeConstraint = newClassType;
               if ((objectConstraint && (objectConstraint->isClassObject() == TR_yes)) ||
                     castIsClassObject)
                  newTypeConstraint = TR::VPClass::create(vp, newClassType,
                                                     NULL, NULL, NULL,
                                                     TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject));
               if (newTypeConstraint)
                  vp->addBlockConstraint(node->getFirstChild(), newTypeConstraint);
               }
            }
         }
      }

   // handle checkcastAndNULLCHK here;
   // objectreference should now be injected
   // with a non-null constraint
   //
   if (!exceptionTaken &&
       (node->getOpCodeValue() == TR::checkcastAndNULLCHK))
      {
      TR::Node *child = node->getFirstChild();
      vp->addBlockConstraint(child, TR::VPNonNullObject::create(vp));
      }

   // Mark objects for tenured alignment
   if (vp->cg()->getSupportsTenuredObjectAlignment() &&
       vp->comp()->getMethodHotness() > hot)
      {
      TR_ASSERT(node->getNumChildren()==2, "checkcast should have only 2 children");
      TR::Node *valueChild = node->getFirstChild();
      TR::Node *definitionNode = NULL;
      if (valueChild->getOpCodeValue() == TR::aload)
         {
         TR_UseDefInfo *useDefInfo = vp->_useDefInfo;
         if (useDefInfo)
            {
            uint16_t useIndex = valueChild->getUseDefIndex();
            if (useDefInfo->isUseIndex(useIndex))
               {
               TR_UseDefInfo::BitVector defs(vp->comp()->allocator());
               useDefInfo->getUseDef(defs, useIndex);
               if (!defs.IsZero())
                  {
                  TR_UseDefInfo::BitVector::Cursor cursor(defs);
                  for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                     {
                     int32_t defIndex = cursor;

                     if (defIndex < useDefInfo->getFirstRealDefIndex())
                        continue;

                     TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);
                     TR::Node *defNode = defTree->getNode();

                     if (defNode && (defNode->getOpCodeValue() == TR::astore))
                        {
                        TR_OpaqueMethodBlock *ownMethod = defNode->getOwningMethod();
                        char buf[512];
                        const char *methodSig = vp->fe()->sampleSignature(ownMethod, buf, 512, vp->trMemory());
                        if (methodSig && !strncmp(methodSig, "java/util/HashMap.get(Ljava/lang/Object;)Ljava/lang/Object;", 59))
                           {
                           definitionNode = defNode;
                           break;
                           }
                        }
                     }
                  }
               }
            }
         }

#ifdef J9_PROJECT_SPECIFIC
      if (definitionNode)
         {
         TR::SymbolReference * classSymRef = node->getSecondChild()->getSymbolReference();
         if (classSymRef && !classSymRef->isUnresolved())
            {
            TR::StaticSymbol * classSym = classSymRef->getSymbol()->getStaticSymbol();
            if (classSym)
               {
               uint32_t size = TR::Compiler->cls.classInstanceSize((TR_OpaqueClassBlock *)classSym->getStaticAddress());
               if (vp->cg()->isObjectOfSizeWorthAligning(size))
                  {
                  vp->comp()->fej9()->markClassForTenuredAlignment(vp->comp(), (TR_OpaqueClassBlock *)classSym->getStaticAddress(), 0);
                  }
               }
            }
         }
#endif

      }


   return node;
   }

TR::Node *constrainBCDCHK(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchUserThrows, NULL, node);
   return node;
   }

TR::Node *constrainCheckcastNullChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainCheckcast(vp, node);
   }

TR::Node *constrainNew(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   vp->createExceptionEdgeConstraints(TR::Block::CanCatchNew, NULL, node);

   // The constraints on the child can be applied to this node. We also know
   // that the type is fixed and non-null.
   //
   bool isGlobal;
   TR::VPConstraint *classConstraint = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (classConstraint)
      {
      // since loadaddrs are now primed with a ClassObject property,
      // this property should not be propagated to the 'new' unless the
      // underlying type of the loadaddr is a java/lang/Class
      if (classConstraint->getClass() && !classConstraint->isFixedClass())
         vp->addGlobalConstraint(node, TR::VPFixedClass::create(vp, classConstraint->getClass()));
      else if (classConstraint->asClass() && classConstraint->asClass()->getClassType() &&
               !(classConstraint->asClass()->getClassType()->isClassObject() == TR_yes))
         vp->addGlobalConstraint(node, classConstraint->asClass()->getClassType());
      else
         vp->addGlobalConstraint(node, classConstraint);
      }

   if (classConstraint && classConstraint->getClassType() &&
      classConstraint->getClassType()->getClass())
      {
      node->setAllocationCanBeRemoved(true);
      }
   vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));

   node->setIsNonNull(true);

   return node;
   }

TR::Node *constrainNewArray(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *sizeNode = node->getFirstChild();
   TR::Node *typeNode = node->getSecondChild();

   vp->createExceptionEdgeConstraints(TR::Block::CanCatchArrayNew, NULL, node);

   TR_ASSERT(typeNode->getOpCodeValue() == TR::iconst, "assertion failure");
   int32_t arrayType = typeNode->getInt();

   // See if there are already constraints on the array size. If the size is
   // known to be negative an exception will always be thrown and we can
   // remove the rest of the block.
   //
   bool isGlobal;
   TR::VPConstraint *sizeConstraint = vp->getConstraint(sizeNode, isGlobal);
   int64_t maxSize = TR::Compiler->om.maxArraySizeInElementsForAllocation(node, vp->comp());
   if (sizeConstraint &&
       (sizeConstraint->getHighInt() < 0 ||
        sizeConstraint->getLowInt() > maxSize))
      {
      vp->mustTakeException();
      //node->setAllocationCanBeRemoved(false)
      return node;
      }
   else if (!sizeConstraint)
      {
      dumpOptDetails(vp->comp(), "size node has no known constraint for newarray %p\n", sizeNode);
      //node->setAllocationCanBeRemoved(false)
      }
   else
      node->setAllocationCanBeRemoved(true);

   // The array size (the first child) can be constrained because of memory
   // limitations on the allocation.
   //
   // TODO - should be sizeConstraint = vp->addBlockConstraint(sizeNode, TR::VPIntRange::create(vp, 0, maxSize));
   if (maxSize < TR::getMaxSigned<TR::Int32>())
      {
      vp->addBlockConstraint(sizeNode, TR::VPIntRange::create(vp, 0, maxSize));
      sizeConstraint = vp->getConstraint(sizeNode, isGlobal);
      }

   // Type constraints can be applied to this node.
   //
   int32_t elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
   TR_OpaqueClassBlock *clazz = vp->fe()->getClassFromNewArrayType(arrayType);
   if (clazz)
      vp->addGlobalConstraint(node, TR::VPFixedClass::create(vp, clazz));
   vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
   if (sizeConstraint)
      vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, sizeConstraint->getLowInt(), sizeConstraint->getHighInt(), elementSize));
   else
      vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, 0, TR::getMaxSigned<TR::Int32>(), elementSize));

   node->setIsNonNull(true);
   return node;
   }

TR::Node *constrainVariableNew(OMR::ValuePropagation *vp, TR::Node *node)
  {
  constrainChildren(vp, node);

  node->setIsNonNull(true);
  return node;
  }

TR::Node *constrainVariableNewArray(OMR::ValuePropagation *vp, TR::Node *node)
  {
  constrainChildren(vp, node);
  TR::Node *typeNode = node->getSecondChild();
  bool isGlobal;
  TR::VPConstraint *constraint = vp->getConstraint(typeNode, isGlobal);
  TR_OpaqueClassBlock *elementType;
  if (constraint && constraint->getClassType()
      && constraint->getClassType()->asFixedClass()
      && constraint->isNonNullObject()
      && (elementType = constraint->getClass()))
     {
     if (TR::Compiler->cls.isPrimitiveClass(vp->comp(), elementType))
        {
        TR::Node::recreateWithoutProperties(node, TR::newarray, node->getNumChildren(), vp->comp()->getSymRefTab()->findOrCreateNewArraySymbolRef(typeNode->getSymbolReference()->getOwningMethodSymbol(vp->comp())));
	TR::Node *typeConst = TR::Node::create(TR::iconst, 0, vp->comp()->fe()->getNewArrayTypeFromClass(constraint->getClass()));
	vp->_curTree->insertBefore(OMR::TreeTop::create(vp->comp(), TR::Node::create(TR::treetop, 1, typeNode)));
	node->setAndIncChild(1, typeConst);
	typeNode->recursivelyDecReferenceCount();
	}
#ifdef J9_PROJECT_SPECIFIC
     else
	{
         TR::Node::recreateWithoutProperties(node, TR::anewarray, node->getNumChildren(), vp->comp()->getSymRefTab()->findOrCreateANewArraySymbolRef(typeNode->getSymbolReference()->getOwningMethodSymbol(vp->comp())));
	if (typeNode->getOpCodeValue() != TR::loadaddr)
	   {
	   TR::Node *loadaddr = TR::Node::createWithSymRef(TR::loadaddr, 0,
		    vp->comp()->getSymRefTab()->findOrCreateClassSymbol(typeNode->getSymbolReference()->getOwningMethodSymbol(vp->comp()), 0, elementType));
	   vp->_curTree->insertBefore(OMR::TreeTop::create(vp->comp(), TR::Node::create(TR::treetop, 1, typeNode)));
	   node->setAndIncChild(1, loadaddr);
	   typeNode->recursivelyDecReferenceCount();
	   }
	}
#endif
     }
  node->setIsNonNull(true);
  return node;
  }

TR::Node *constrainANewArray(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *sizeNode = NULL;
   TR::Node *typeNode = NULL;
   sizeNode = node->getFirstChild();
   typeNode = node->getSecondChild();

   vp->createExceptionEdgeConstraints(TR::Block::CanCatchArrayNew, NULL, node);

   bool isGlobal;
   TR::VPConstraint *typeConstraint = vp->getConstraint(typeNode, isGlobal);
   TR_ASSERT(typeConstraint, "Class child of anewarray has no constraint");

   // See if there are already constraints on the array size. If the size is
   // known to be negative an exception will always be thrown and we can
   // remove the rest of the block.
   //
   int32_t elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
   int64_t maxSize = TR::Compiler->om.maxArraySizeInElementsForAllocation(node, vp->comp());

   TR::VPConstraint *sizeConstraint = vp->getConstraint(sizeNode, isGlobal);
   if (sizeConstraint &&
       (sizeConstraint->getHighInt() < 0 ||
        sizeConstraint->getLowInt() > maxSize))
      {
      vp->mustTakeException();
      //node->setAllocationCanBeRemoved(false)
      return node;
      }
   else if (!sizeConstraint)
      {
      dumpOptDetails(vp->comp(), "size node has no known constraint for anewarray %p\n", sizeNode);
      //node->setAllocationCanBeRemoved(false)
      }
   else
      {
      if (typeConstraint && typeConstraint->getClassType() &&
          typeConstraint->getClassType()->getClass())
         {
         TR_OpaqueClassBlock *arrayClass = vp->fe()->getArrayClassFromComponentClass(typeConstraint->getClassType()->getClass());
         if (arrayClass)
            node->setAllocationCanBeRemoved(true);
         }
      }


   // The array size (the first child) can be constrained because of memory
   // limitations on the allocation.
   //
   // TODO - should be sizeConstraint = vp->addBlockConstraint(sizeNode, TR::VPIntRange::create(vp, 0, maxSize));
   if (maxSize < TR::getMaxSigned<TR::Int32>())
      {
      vp->addBlockConstraint(sizeNode, TR::VPIntRange::create(vp, 0, maxSize));
      sizeConstraint = vp->getConstraint(sizeNode, isGlobal);
      }

   // Type constraints can be applied to this node.
   //
   if (typeConstraint && typeConstraint->getClassType())
      {
      typeConstraint = typeConstraint->getClassType()->getArrayClass(vp);

      if (typeConstraint)
         {
         // since loadaddrs are now primed with a ClassObject property,
         // this property should not be propagated to the 'newarray' unless the
         // underlying type of the loadaddr is a java/lang/Class
         if (typeConstraint->getClass() && !typeConstraint->isFixedClass())
            typeConstraint = TR::VPFixedClass::create(vp, typeConstraint->getClass());
         ////else if (typeConstraint->asClass() && typeConstraint->asClass()->getClassType() &&
         ////          !(typeConstraint->asClass()->getClassType()->isClassObject() == TR_yes))
         ////   typeConstraint = typeConstraint->asClass()->getClassType();
         vp->addGlobalConstraint(node, typeConstraint);
         }
      }
   vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
   if (sizeConstraint)
      {
      vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, sizeConstraint->getLowInt(), sizeConstraint->getHighInt(), elementSize));
      }
   else
      vp->addGlobalConstraint(node, TR::VPArrayInfo::create(vp, 0, TR::getMaxSigned<TR::Int32>(), elementSize));

   node->setIsNonNull(true);
   return node;
   }

TR::Node *constrainMultiANewArray(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   // The first child is the number of dimensions. It should be a constant equal
   // to the number of children - 2.
   //
   TR_ASSERT(node->getFirstChild()->getOpCode().isLoadConst() &&
          node->getFirstChild()->getInt() == node->getNumChildren()-2,
          "Bad number of dims for multianewarray");

   int32_t numChildren = node->getNumChildren();

   TR::Node *typeNode = node->getChild(numChildren-1);

   vp->createExceptionEdgeConstraints(TR::Block::CanCatchArrayNew, NULL, node);

   bool isGlobal;
   TR::VPConstraint *typeConstraint = vp->getConstraint(typeNode, isGlobal);
   TR_ASSERT(typeConstraint, "Class child of multianewarray has no constraint");

   // See if there are already constraints on the array size. If any size is
   // known to be negative an exception will always be thrown and we can
   // remove the rest of the block.
   //
   int32_t maxSize = (int32_t)TR::Compiler->om.maxArraySizeInElementsForAllocation(node, vp->comp()); // int32_t is ok because multiANewArray is only useful in Java, where array sizes are bound by TR::getMaxSigned<TR::Int32>()
   int64_t maxHeapSize;

   if (vp->comp()->compileRelocatableCode())
      {
      maxHeapSize = -1;
      }
   else
      {
      maxHeapSize = TR::Compiler->vm.maxHeapSizeInBytes();
      }

   int32_t maxDimSize = TR::getMaxSigned<TR::Int32>();
   if (maxHeapSize > 0)
      {
      int64_t maxHeapSizeInPointers = maxHeapSize / TR::Compiler->om.sizeofReferenceField();
      if (maxHeapSizeInPointers < maxDimSize)
         maxDimSize = (int32_t)maxHeapSizeInPointers;
      }

   TR::Node *child = NULL;
   TR::VPClassType *newType = typeConstraint->getClassType();
   TR::VPConstraint *sizeConstraint = NULL;
   for (int32_t i = numChildren-2; i > 0; i--)
      {
      child = node->getChild(i);
      sizeConstraint = vp->getConstraint(child, isGlobal);
      int32_t maxHigh = maxDimSize;
      if (i == numChildren-2) // last dimension
         maxHigh = maxSize;

      if (sizeConstraint &&
          (sizeConstraint->getHighInt() < 0 ||
           sizeConstraint->getLowInt() > maxHigh))
         {
         vp->mustTakeException();
         return node;
         }

      // The array size can be constrained because of memory limitations on the
      // allocation.
      //
      vp->addBlockConstraint(child, TR::VPIntRange::create(vp, 0, maxHigh));

      // Build up the eventual type of the array
      // TODO - enable this when getArrayClass is correct
      //
      //newType = newType->getArrayClass(vp);
      }

   sizeConstraint = vp->getConstraint(node->getSecondChild(), isGlobal);

   int32_t elementSize;
   if( numChildren-2 == 1 ) // dims == 1
      {
      // A 1 dim array is NOT an array of pointers to arrays!
      // We need to determine the size of the element type defined by the type node
      TR::SymbolReference *symRef = typeNode->getSymbolReference();
      int32_t len;
      const char *sig = symRef->getTypeSignature(len);
      if( sig == NULL ) return(node);
      elementSize = arrayElementSize(sig, len, typeNode, vp);
      if( elementSize == 0 ) return(node);
      }
   else
      {
      elementSize = TR::Compiler->om.sizeofReferenceField();
      }

   // Type constraints can be applied to this node.
   //
   TR::VPArrayInfo *arrayInfo = TR::VPArrayInfo::create(vp, sizeConstraint->getLowInt(), sizeConstraint->getHighInt(), elementSize);
   vp->addGlobalConstraint(node, TR::VPClass::create(vp, newType, TR::VPNonNullObject::create(vp), NULL, arrayInfo,
                      TR::VPObjectLocation::create(vp, TR::VPObjectLocation::NotClassObject)));

   node->setIsNonNull(true);
   return node;
   }


//bit opcode constraints start

static TR::VPLongRange* getLongRange (TR::VPConstraint* c) {return c->asLongRange();}
static TR::VPLongConst* getLongConst (TR::VPConstraint* c) {return c->asLongConst();}
static TR::VPIntRange* getIntRange (TR::VPConstraint* c)   {return c->asIntRange();}
static TR::VPIntConst* getIntConst (TR::VPConstraint* c)   {return c->asIntConst();}
static void getLowHighInts (TR::VPIntRange* range, int32_t& low, int32_t& high)
   {
   low = range->getLowInt();
   high = range->getHighInt();
   }
static void getInt (TR::VPIntConst* c, int32_t& n)
   {
   n = c->getInt();
   }
static void getLowHighLongs (TR::VPLongRange* range, int64_t& low, int64_t& high)
   {
   low = range->getLowLong();
   high = range->getHighLong();
   }
static void getLong (TR::VPLongConst* c, int64_t& n)
   {
   n = c->getLong();
   }

static TR::VPConstraint* createIntConstConstraint(OMR::ValuePropagation *vp, int32_t n) {return TR::VPIntConst::create(vp, n); }
static TR::VPConstraint* createIntRangeConstraint(OMR::ValuePropagation *vp, int32_t l, int32_t h) {return TR::VPIntRange::create(vp, l, h);}
static TR::VPConstraint* createLongConstConstraint(OMR::ValuePropagation *vp, int64_t n) {return TR::VPLongConst::create(vp, n); }
static TR::VPConstraint* createLongRangeConstraint(OMR::ValuePropagation *vp, int64_t l, int64_t h) {return TR::VPLongRange::create(vp, l, h);}

static int32_t integerToPowerOf2 (int32_t n) {return (n==0) ? 0 : floorPowerOfTwo(n); }
static int32_t integerNumberOfLeadingZeros (int32_t n) {return leadingZeroes (n);}
static int32_t integerNumberOfTrailingZeros (int32_t n) {return trailingZeroes(n);}
static int32_t integerBitCount (int32_t n) {return populationCount(n);}
static int64_t longToPowerOf2 (int64_t n) {return (n==0) ? 0 : floorPowerOfTwo64(n);}
static int32_t longNumberOfLeadingZeros (int64_t n) {return leadingZeroes (n);}
static int32_t longNumberOfTrailingZeros (int64_t n) { return trailingZeroes(n); }
static int32_t longBitCount (int64_t n) {return populationCount(n);}

template <typename T>
void swap (T& a, T& b)
   {
   T tmp = b;
   b = a;
   a = tmp;
   }


template <typename FUNC, typename FUNC2, typename T>
void addValidRangeBlockOrGlobalConstraint (OMR::ValuePropagation *vp,
                                                   TR::Node *node,
                                                   FUNC createRange,
                                                   FUNC2 processValue,
                                                   T low,
                                                   T high,
                                                   bool childGlobal)
   {
   T pLow = processValue(low) , pHigh = processValue(high);

   if (pLow > pHigh)
      {
      swap (pLow, pHigh);
      }

   if (vp->trace())
      {
      traceMsg(vp->comp(), "Adding a %s range constraint %lld .. %lld on the node %p\n", (childGlobal) ? "global" : "block" , pLow, pHigh, node);
      }


   vp->addBlockOrGlobalConstraint(node, createRange(vp, pLow, pHigh),childGlobal);
   }

template <typename A,
         typename B,
         typename C,
         typename D,
         typename E,
         typename F,
         typename G,
         typename T>
static TR::Node* constrainHighestOneBitAndLeadingZerosHelper (OMR::ValuePropagation *vp,
                                                                     TR::Node *node,
                                                                     A getConst,
                                                                     B getRange,
                                                                     C getValue,
                                                                     D getValues,
                                                                     E createConst,
                                                                     F createRange,
                                                                     G processValue,
                                                                     T MIN_VALUE,
                                                                     T MAX_VALUE)
  {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   constrainChildren(vp, node);

   if (vp->trace())
      {
      traceMsg(vp->comp(), "calling constrainHighestOneBitAndLeadingZerosHelper for node %p\n", node);
      }

   bool childGlobal;
   TR::VPConstraint *childConstraint = vp->getConstraint(node->getFirstChild(), childGlobal);

   if (childConstraint)
      {
      if (getConst(childConstraint))
         {
         T value = 0;
         getValue (getConst(childConstraint), value);

         //RangeConstraint with the same low and high should be folded into a const constraint
         MIN_VALUE = value;
         MAX_VALUE = value;

         if (vp->trace())
            {
            traceMsg(vp->comp(), "The first child's value of %p %lld is replaced with %lld \n", node, value, processValue (value));
            }

         }
      else if (getRange(childConstraint))
         {
         T low = 0, high = 0;
         getValues(getRange(childConstraint) , low, high);
         if (low < 0 && high < 0)
            {
            //RangeConstraint with the same low and high should be folded into a const constraint.
            //All negative numbers have the highest bit set
            MIN_VALUE = MAX_VALUE;
            if (vp->trace())
               {
               traceMsg(vp->comp(), "Constraint %lld .. %lld of %p 's first child is negative and folded into %lld \n", low, high, node, processValue(MAX_VALUE));
               }
            }
         else if (low >= 0 && high >=0)
            {
            MIN_VALUE=low;
            MAX_VALUE=high;
            }
         //The else case is the same as [min_processValue .. max_processValue]
         }
      }

   addValidRangeBlockOrGlobalConstraint (vp, node, createRange, processValue, MIN_VALUE, MAX_VALUE, childGlobal);
   return node;
   }

template <typename A,
         typename B,
         typename C,
         typename D,
         typename E,
         typename F,
         typename G,
         typename T>
static TR::Node* constrainLowestOneBitAndTrailingZerosHelper (OMR::ValuePropagation *vp,
                                                                     TR::Node *node,
                                                                     A getConst,
                                                                     B getRange,
                                                                     C getValue,
                                                                     D getValues,
                                                                     E createConst,
                                                                     F createRange,
                                                                     G processValue,
                                                                     T MIN_VALUE,
                                                                     T MAX_VALUE)
  {

   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   constrainChildren(vp, node);


   if (vp->trace())
      {
      traceMsg(vp->comp(), "calling constrainLowestOneBitAndTrailingZerosHelper for node %p\n", node);
      }

   bool childGlobal;
   TR::VPConstraint *childConstraint = vp->getConstraint(node->getFirstChild(), childGlobal);
   if (childConstraint && getConst(childConstraint))
      {
      T value = 0;
      getValue (getConst(childConstraint), value);
      MIN_VALUE = value;
      MAX_VALUE = value;
      }

   addValidRangeBlockOrGlobalConstraint (vp, node, createRange, processValue, MIN_VALUE, MAX_VALUE, childGlobal);
   return node;
   }

TR::Node * constrainIntegerHighestOneBit(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainHighestOneBitAndLeadingZerosHelper (vp, node, getIntConst,
                                                       getIntRange, getInt, getLowHighInts,
                                                       createIntConstConstraint,
                                                       //createIntRangeConstraint, integerToPowerOf2, (int32_t) 0, (int32_t) -1);
                                                       createIntRangeConstraint, integerToPowerOf2, (int32_t) TR::getMaxSigned<TR::Int32>(), (int32_t) TR::getMinSigned<TR::Int32>());
   }


TR::Node * constrainIntegerNumberOfLeadingZeros(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainHighestOneBitAndLeadingZerosHelper (vp, node, getIntConst,
                                                       getIntRange, getInt, getLowHighInts,
                                                       createIntConstConstraint,
                                                       createIntRangeConstraint, integerNumberOfLeadingZeros, (int32_t) 0, (int32_t) -1);


   }

TR::Node * constrainLongHighestOneBit(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainHighestOneBitAndLeadingZerosHelper (vp, node, getLongConst,
                                                       getLongRange, getLong, getLowHighLongs,
                                                       createLongConstConstraint,
                                                       //createLongRangeConstraint, longToPowerOf2, (int64_t) 0, (int64_t) -1);
                                                       createLongRangeConstraint, longToPowerOf2, (int64_t) TR::getMaxSigned<TR::Int64>(), TR::getMinSigned<TR::Int64>());

   }

TR::Node * constrainLongNumberOfLeadingZeros(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainHighestOneBitAndLeadingZerosHelper (vp, node, getLongConst,
                                                       getLongRange, getLong, getLowHighLongs,
                                                       createIntConstConstraint,
                                                       createIntRangeConstraint, longNumberOfLeadingZeros, (int64_t) 0, (int64_t) -1);
   }

TR::Node * constrainIntegerLowestOneBit(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainLowestOneBitAndTrailingZerosHelper (vp, node, getIntConst,
                                                       getIntRange, getInt, getLowHighInts,
                                                       createIntConstConstraint,
                                                       createIntRangeConstraint, integerToPowerOf2, (int32_t) TR::getMaxSigned<TR::Int32>(), (int32_t) TR::getMinSigned<TR::Int32>());
   }


   TR::Node * constrainIntegerBitCount(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainLowestOneBitAndTrailingZerosHelper (vp, node, getIntConst,
                                                       getIntRange, getInt, getLowHighInts,
                                                       createIntConstConstraint,
                                                       createIntRangeConstraint, integerBitCount, (int32_t) 0, (int32_t) -1);
   }


TR::Node * constrainIntegerNumberOfTrailingZeros(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainLowestOneBitAndTrailingZerosHelper (vp, node, getIntConst,
                                                       getIntRange, getInt, getLowHighInts,
                                                       createIntConstConstraint,
                                                       createIntRangeConstraint, integerNumberOfTrailingZeros, (int32_t) 0, (int32_t) -1);
   }

TR::Node * constrainLongLowestOneBit(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainLowestOneBitAndTrailingZerosHelper (vp, node, getLongConst,
                                                       getLongRange, getLong, getLowHighLongs,
                                                       createLongConstConstraint,
                                                       createLongRangeConstraint, longToPowerOf2, (int64_t) TR::getMaxSigned<TR::Int64>(), (int64_t) TR::getMinSigned<TR::Int64>());
   }


TR::Node * constrainLongNumberOfTrailingZeros(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainLowestOneBitAndTrailingZerosHelper (vp, node, getLongConst,
                                                       getLongRange, getLong, getLowHighLongs,
                                                       createIntConstConstraint,
                                                       createIntRangeConstraint, longNumberOfTrailingZeros, (int64_t) 0, (int64_t) -1);
   }


   TR::Node * constrainLongBitCount(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainLowestOneBitAndTrailingZerosHelper  (vp, node, getLongConst,
                                                       getLongRange, getLong, getLowHighLongs,
                                                       createIntConstConstraint,
                                                       createIntRangeConstraint, longBitCount, (int64_t) 0, (int64_t) -1);
   }


//bit opcodes constraints end


// Handles TR::arraylength and TR::contigarraylength
//
TR::Node *constrainArraylength(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;

   constrainChildren(vp, node);

   int32_t lowerBoundLimit = 0;
   int32_t upperBoundLimit = TR::getMaxSigned<TR::Int32>();
   int32_t elementSize     = 0;

   // See if the underlying array has bound limits
   //
   TR::Node *objectRef = node->getFirstChild();
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(objectRef, isGlobal);

   if (constraint)
      {
      TR::VPArrayInfo *arrayInfo = constraint->getArrayInfo();
      if (arrayInfo)
         {
         lowerBoundLimit = arrayInfo->lowBound();
         upperBoundLimit = arrayInfo->highBound();
         elementSize     = arrayInfo->elementSize();
         }
      }

   // If the element size is still not known, try to get it from the node or
   // from the signature, and then propagate it back down to the object ref.
   //
   if (!elementSize)
      {
      elementSize = node->getArrayStride();

      if (!elementSize && constraint)
         {
         int32_t len = 0;
         const char *sig = constraint->getClassSignature(len);
         if (sig)
            elementSize = arrayElementSize(sig, len, objectRef, vp);
         }

      if (elementSize)
         {
         constraint = TR::VPArrayInfo::create(vp, lowerBoundLimit, upperBoundLimit, elementSize);
         vp->addBlockOrGlobalConstraint(objectRef, constraint, isGlobal);
         }
      }

   // See if the array length is constant
   //
   if (lowerBoundLimit == upperBoundLimit)
      {
      // A constant can't be used if a contiguity check is still required.
      //
      if (!(node->getOpCodeValue() == TR::contigarraylength || node->getOpCodeValue() == TR::discontigarraylength) ||
          !TR::Compiler->om.isDiscontiguousArray(lowerBoundLimit, elementSize))
         {
         constraint = TR::VPIntConst::create(vp, lowerBoundLimit);
         vp->replaceByConstant(node, constraint, isGlobal);
         return node;
         }
      else
         {
         int32_t intValue = lowerBoundLimit;
         if (node->getOpCodeValue() == TR::contigarraylength)
            intValue = 0;

         constraint = TR::VPIntConst::create(vp, intValue);
         vp->replaceByConstant(node, constraint, isGlobal);
         return node;
         }
      }


   uint32_t shiftAmount = 0;
   if (elementSize > 1)
      {
      if (elementSize == 2)
         shiftAmount = 1;
      else if (elementSize == 4)
         shiftAmount = 2;
      else
         shiftAmount = 3;

      int64_t maxSize = TR::Compiler->om.maxArraySizeInElements(elementSize, vp->comp());
      if (maxSize < upperBoundLimit)
         {
         TR_ASSERT(0 <= maxSize && maxSize <= TR::getMaxSigned<TR::Int32>(), "arraylength must be within Int32 range");
         upperBoundLimit = (int32_t)maxSize;
         }
      }

   // For hybrid arraylets, a constant can't be used if a contiguity check is still required.
   //
   if ( !(node->getOpCodeValue() == TR::contigarraylength || node->getOpCodeValue() == TR::discontigarraylength) ||
        (lowerBoundLimit != upperBoundLimit) ||
        !TR::Compiler->om.isDiscontiguousArray(upperBoundLimit, elementSize) )
      {
      constraint = TR::VPIntRange::create(vp, lowerBoundLimit, upperBoundLimit);
      }
   else
      {
      if (node->getOpCodeValue() == TR::contigarraylength)
         {
         int32_t maxContiguousArraySizeInElements = TR::Compiler->om.maxContiguousArraySizeInBytes() >> shiftAmount;
         if (lowerBoundLimit > maxContiguousArraySizeInElements)
            {
            lowerBoundLimit = 0;
            upperBoundLimit = 0;
            }
         else
            {
            lowerBoundLimit = 0;
            upperBoundLimit = maxContiguousArraySizeInElements;
            }
         }

       constraint = TR::VPIntRange::create(vp, lowerBoundLimit, upperBoundLimit);
       }

   if (constraint)
      {
      vp->addBlockOrGlobalConstraint(node, constraint, isGlobal);
      }

   if (!node->getArrayStride() &&
       performTransformation(vp->comp(), "%sSetting element width for array [%p] to %d\n", OPT_DETAILS, node, elementSize))
      {
      node->setArrayStride(elementSize);
      }

   if (!vp->_curTree->getNode()->getOpCode().isNullCheck())
         vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));

   node->setIsNonNegative(true);
   node->setCannotOverflow(true);
   return node;
   }

static void refineSymbolReferenceForProfiledToOverridden (OMR::ValuePropagation* vp, TR::Node* callNode, TR::SymbolReference * newSymRef)
   {
   for (OMR::ValuePropagation::VirtualGuardInfo *cvg = vp->_convertedGuards.getFirst(); cvg; cvg = cvg->getNext())
      {
      if (cvg->_callNode == callNode)
         {
         if (vp->trace())
            {
            traceMsg(vp->comp(), "Replaced with newSymRef %d in a Profiled2Overridden guard %p\n", newSymRef->getReferenceNumber(), cvg->_newVirtualGuard);

            }

         cvg->_newVirtualGuard->setSymbolReference(newSymRef);
         break;
         }
      }
   }

static TR::MethodSymbol *
refineMethodSymbolInCall(
   OMR::ValuePropagation *vp,
   TR::Node *node,
   TR::SymbolReference *symRef,
   TR_ResolvedMethod *resolvedMethod,
   int32_t offset)
   {
   TR::SymbolReference * newSymRef =
      vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(
         symRef->getOwningMethodIndex(), -1, resolvedMethod, TR::MethodSymbol::Virtual);
   newSymRef->copyAliasSets(symRef, vp->comp()->getSymRefTab());
   newSymRef->setOffset(offset);
   TR::MethodSymbol *methodSymbol = newSymRef->getSymbol()->castToMethodSymbol();
   refineSymbolReferenceForProfiledToOverridden(vp, node, newSymRef);
   node->setSymbolReference(newSymRef);
   if (vp->trace())
      traceMsg(vp->comp(), "Refined method symbol to %s\n", resolvedMethod->signature(vp->trMemory()));
   return methodSymbol;
   }

static void devirtualizeCall(OMR::ValuePropagation *vp, TR::Node *node)
   {
   TR::SymbolReference *symRef        = node->getSymbolReference();
   TR::MethodSymbol    *methodSymbol  = symRef->getSymbol()->castToMethodSymbol();
   bool                interfaceCall = methodSymbol->isInterface();

   if (!methodSymbol->firstArgumentIsReceiver())
      {
      if (vp->trace())
         traceMsg(vp->comp(), "Not attempting to de-virtualize call [%p] without first argument receiver\n", node);
      return;
      }

   int32_t firstArgIndex = node->getFirstArgumentIndex();
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getChild(firstArgIndex), isGlobal);
   TR_OpaqueClassBlock *thisType = NULL;
   if (!constraint || !(thisType = constraint->getClass()))
      {
      if (vp->trace())
         traceMsg(vp->comp(), "Interface call [%p] to %s with unknown object type in %s\n", node, methodSymbol->getMethod()->signature(vp->trMemory(), stackAlloc), vp->comp()->signature());


      static bool dontProfileMore = feGetEnv("TR_DontProfileMoreAtHot") ? true : false;

      if (!dontProfileMore &&
          (vp->comp()->getMethodHotness() == hot) &&
          vp->comp()->getRecompilationInfo() &&
          vp->_isGlobalPropagation &&
          vp->lastTimeThrough() &&
          vp->optimizer()->switchToProfiling())
         {
         dumpOptDetails(vp->comp(), "%s enabling profiling: would be useful to profile arraycopy calls\n", OPT_DETAILS);
         }

      return;
      }

   // If thisType represents an array, change it to a fixed reference to
   // java/lang/Object for the purposes of devirtualization.
   if ( constraint->getArrayInfo())
      {
#ifdef J9_PROJECT_SPECIFIC
      thisType = vp->comp()->getObjectClassPointer();
      if (!thisType)
         {
         if (vp->trace())
            traceMsg(vp->comp(), "Not attempting to de-virtualize call [%p] with array receiver\n", node);
         return;
         }
      constraint = TR::VPFixedClass::create(vp, thisType);
#endif
      }


    if ((constraint->asClass() &&
         (constraint->asClass()->isClassObject() == TR_yes)) ||
        (constraint->asObjectLocation() &&
         (constraint->asObjectLocation()->isClassObject() == TR_yes)))
      {
#ifdef J9_PROJECT_SPECIFIC
      thisType = vp->comp()->getClassClassPointer();
      if (!thisType)
         {
         if (vp->trace())
            traceMsg(vp->comp(), "Not attempting to de-virtualize call [%p] with class object receiver\n", node);
         return;
         }
      constraint = TR::VPFixedClass::create(vp, thisType);
#endif
      }

   // The fixed type may be different than the type of the this pointer.
   // For example,
   //    A a = new B();
   //    a.foo(); // the fixed type is B
   //
   // Find the resolved method for the fixed type.
   //
   TR_ResolvedMethod     * owningMethod           = symRef->getOwningMethod(vp->comp());
   TR_ResolvedMethod     * resolvedMethod;
   TR_ResolvedMethod     * originalResolvedMethod = 0;
   TR_OpaqueClassBlock     * originalMethodClass;

   int32_t offset;
   int32_t len;
   const char * s;
   int32_t cpIndex = symRef->getCPIndex();
   bool needsGuard = false;

   TR::Node *argNode = node->getChild(firstArgIndex);
   if (argNode->getSymbol())
      {
      TR::Symbol *argSym = argNode->getSymbol();
      if (argSym->isParm())
         {
         if (!vp->isParmInvariant(argSym))
            {
            if (vp->trace())
               traceMsg(vp->comp(), "Not attempting to de-virtualize call [%p] because receiver [%p] is not invariant\n", node, argNode);
            return;
            }
         }
      }

   if (methodSymbol->isComputed())
      {
#ifdef J9_PROJECT_SPECIFIC
      if (methodSymbol->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeExact && !symRef->isUnresolved())
         {
         TR::Node *receiver = node->getArgument(0);
         TR::KnownObjectTable *knot = vp->comp()->getOrCreateKnownObjectTable();
         TR::VPConstraint *constraint = vp->getConstraint(receiver, isGlobal);
         if (constraint && constraint->getKnownObject() && knot)
            {
            TR::ResolvedMethodSymbol *owningMethod = symRef->getOwningMethodSymbol(vp->comp());
            resolvedMethod = vp->comp()->fej9()->createMethodHandleArchetypeSpecimen(vp->trMemory(), knot->getPointerLocation(constraint->getKnownObject()->getIndex()), owningMethod->getResolvedMethod());
            if (resolvedMethod)
               {
               TR::SymbolReference *specimenSymRef = vp->getSymRefTab()->findOrCreateMethodSymbol(owningMethod->getResolvedMethodIndex(), -1, resolvedMethod, TR::MethodSymbol::ComputedVirtual);
               if (performTransformation(vp->comp(), "%sSubstituting more specific method symbol on %p: %s\n", OPT_DETAILS, node, specimenSymRef->getName(vp->getDebug())))
                  {
                  node->setSymbolReference(specimenSymRef);
                  }
               else
                  {
                  return;
                  }
               if (vp->optimizer()->getOptimization(OMR::methodHandleInvokeInliningGroup)->requested())
                  {
                  if (vp->trace())
                     traceMsg(vp->comp(), "Not inlining call [%p] because the MethodHandle.invoke inlining group will do a better job\n", node);
                  return;
                  }
               }
            else
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "Not inlining call [%p] because there is no more specific method symbol for obj%d\n", node, constraint->getKnownObject()->getIndex());
               return;
               }
            }
         else
            {
            if (vp->trace())
               traceMsg(vp->comp(), "Not inlining call [%p] because unable to substitute object-specific method symbol\n", node);
            return;
            }
         }
#endif
      }
   else if (methodSymbol->isInterface())
      {
      if (TR::Compiler->cls.isInterfaceClass(vp->comp(), thisType))
         {
         if (vp->trace())
            traceMsg(vp->comp(), "Not attempting to de-virtualize interface call [%p] with interface-class receiver\n", node);
         return;
         }

      int32_t cpIndex = symRef->getCPIndex();

      TR_Method * originalMethod = methodSymbol->getMethod();
      len = originalMethod->classNameLength();
      s = classNameToSignature(originalMethod->classNameChars(), len, vp->comp());
      originalMethodClass = vp->fe()->getClassFromSignature(s, len, owningMethod);

      if (!originalMethodClass)
         {
         if (vp->trace())
            traceMsg(vp->comp(), "Not attempting to de-virtualize interface call [%p] with receiver class %.*s not yet loaded\n", node, len, s);
         return;
         }

      resolvedMethod = owningMethod->getResolvedInterfaceMethod(vp->comp(), thisType, cpIndex);
      if (!resolvedMethod)
         {
         if (vp->trace())
            traceMsg(vp->comp(), "Not attempting to de-virtualize interface call [%p] with no resolved method\n", node, len, s);
         return;
         }

      offset = owningMethod->getResolvedInterfaceMethodOffset(thisType, cpIndex);
      }
   else
      {
      TR_ASSERT(methodSymbol->isVirtual(), "unexpected method symbol kind for %s node %p", node->getOpCode().getName(), node);

      TR::ResolvedMethodSymbol * resolvedMethodSymbol = methodSymbol->getResolvedMethodSymbol();
      if (!resolvedMethodSymbol || symRef == vp->getSymRefTab()->findObjectNewInstanceImplSymbol())
         return;

      originalResolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      originalMethodClass = originalResolvedMethod->classOfMethod();

      if ((vp->fe()->isInstanceOf(thisType, originalMethodClass, true, true) != TR_yes) ||
          (originalMethodClass && TR::Compiler->cls.isInterfaceClass(vp->comp(), originalMethodClass)))
         return;

      if (!constraint->isFixedClass())
         {
         if (!constraint->isPreexistentObject())
            {
            if (originalMethodClass == thisType)
               return;

            s = node->getChild(firstArgIndex)->getTypeSignature(len);
            if (s && vp->fe()->getClassFromSignature(s, len, owningMethod) == thisType)
               return;
            }
         }

      offset = symRef->getOffset();
      resolvedMethod = owningMethod->getResolvedVirtualMethod(vp->comp(), thisType, offset);
      if (!resolvedMethod)
         return;
      }

   if (node->isTheVirtualCallNodeForAGuardedInlinedCall())
      {
#ifdef J9_PROJECT_SPECIFIC
      TR_PersistentClassInfo * classInfo =
         vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(thisType, vp->comp());
      if (!classInfo ||
          !classInfo->isInitialized())
         return;
#else
      return;
#endif
      }

   if (methodSymbol->isVirtual() || methodSymbol->isInterface())
      {
      // This initial attempt to refine the call symbol applies only to virtual
      // calls. We want to be able to do this for interface calls too but we
      // currently can't in general, since we need to preserve the behaviour of
      // invokeinterface, which throws IllegalAccessError when dispatch lands
      // on a non-public method.
      //
      // Maybe TODO? Get the type information we have to the inliner, to
      // prevent interface->virtual transformation without inhibiting inlining.
      if (methodSymbol->isVirtual()
          && (!originalResolvedMethod || !resolvedMethod->isSameMethod(originalResolvedMethod)))
         {
         // The virtual guard noping code relies on the the virtual call node's symbol to
         // refer to the original devirtualized call.  Not devirtualizing the call shouldn't be
         // a performance concern since the call is in a cold block.
         //
         // Why do we need to stop the devirtualization if its a
         // guarded virtual ? Commenting these two lines out so that a guarded virtual gets
         // added to the devirtualizedCalls list and the guard is eventually removed. This
         // has the benefit that the call no longer interferes with aliasing (and other opts
         // like escape analysis).
         //
         if (performTransformation(vp->comp(), "%sDevirtualizing call [%p] to %s\n", OPT_DETAILS, node,
                  resolvedMethod->signature(vp->trMemory())))
            methodSymbol = refineMethodSymbolInCall(vp, node, symRef, resolvedMethod, offset);
         }
      else if (!resolvedMethod->virtualMethodIsOverridden() && !constraint->isFixedClass())
         return;

      if (constraint->isFixedClass() || constraint->isPreexistentObject())
         {
         if (!performTransformation(vp->comp(), "%sChanging an indirect call %s (%s) to a direct call [%p]\n", OPT_DETAILS, resolvedMethod->signature(vp->trMemory()), node->getOpCode().getName(), node))
            return;

         // change the opcode to be a direct call
         //

         if (!vp->registerPreXClass(constraint) && constraint->isPreexistentObject())
            {
            if (!resolvedMethod->virtualMethodIsOverridden() &&
                (constraint->getClass() == constraint->getPreexistence()->getAssumptionClass()))
               vp->_prexMethods.add(resolvedMethod);
            else
               return;
            }

         // If we know the exact callee, then we can still transform even
         // interface calls. Fix the symbol now since it wasn't done earlier.
         // The callee should be public since it was found by
         // getResolvedInterfaceMethod.
         if (methodSymbol->isInterface())
            methodSymbol = refineMethodSymbolInCall(vp, node, symRef, resolvedMethod, offset);
         TR::Node::recreate(node, methodSymbol->getMethod()->directCallOpCode());

         // remove the generated first argument
         //
         TR_ASSERT(firstArgIndex == 1, "more than one generated argument on an indirect call");
         node->getChild(0)->recursivelyDecReferenceCount();
         int32_t numChildren = node->getNumChildren();
         for (int32_t i = 1; i < numChildren; i = (++i & 0x0000ffff)) // this & with 0x0000ffff is to work around an opt bug in the ibm build on ia32
            node->setChild(i - 1, node->getChild(i));
         node->setNumChildren(numChildren - 1);
         firstArgIndex--;
         }
      }

   TR_PrexArgInfo *argInfo = new (vp->trStackMemory()) TR_PrexArgInfo(node->getNumChildren() - firstArgIndex, vp->trMemory());
   bool tracePrex = vp->trace() || vp->comp()->trace(OMR::inlining) || vp->comp()->trace(OMR::invariantArgumentPreexistence);
   if (tracePrex)
      traceMsg(vp->comp(), "PREX.vp: Value propagation populating prex argInfo for %s %p\n", node->getOpCode().getName(), node);
   for (int32_t c = node->getNumChildren() - 1 ; c >= firstArgIndex; --c)
      {
      TR::Node *argument = node->getChild(c);
      if (argument->getDataType() == TR::Address)
         {
         TR::VPConstraint *constr = vp->getConstraint(argument, isGlobal);
         if (constr)
            {
            // TODO: constr is most likely a TR::VPClass constraint, so we should
            // be calling constr->getKnownObject().  Should fix this in dev.
            //
            // Note also that if prex were to start using VP constraints, there
            // would be no need to check isNonNullObject here any more.  That
            // check is only needed because prex arg info's known object info
            // is NOT predicated on the reference being non-null.
            //
            if (constr->asKnownObject() && constr->isNonNullObject())
               {
               argInfo->set(c - firstArgIndex, new (vp->trStackMemory()) TR_PrexArgument(constr->asKnownObject()->getIndex(), vp->comp()));
               if (tracePrex)
                  traceMsg(vp->comp(), "PREX.vp:    Child %d [%p] arg %p is known object obj%d\n", c, argument, argInfo->get(c-firstArgIndex), constr->asKnownObject()->getIndex());
               }
            else if (constr->isFixedClass())
               {
               argInfo->set(c - firstArgIndex, new (vp->trStackMemory()) TR_PrexArgument(TR_PrexArgument::ClassIsFixed, constr->getClass()));
               if (tracePrex)
                  {
                  TR_OpaqueClassBlock *clazz = constr->getClass();
                  const char *sig = TR::Compiler->cls.classSignature(vp->comp(), clazz, vp->trMemory());
                  traceMsg(vp->comp(), "PREX.vp:    Child %d [%p] arg %p has fixed class %p %s\n", c, argument, argInfo->get(c-firstArgIndex), clazz, sig);
                  }
               }
            else if (constr->isPreexistentObject())
               {
               argInfo->set(c - firstArgIndex, new (vp->trStackMemory()) TR_PrexArgument(TR_PrexArgument::ClassIsPreexistent));
               if (tracePrex)
                  traceMsg(vp->comp(), "PREX.vp:    Child %d [%p] arg %p is preexistent\n", c, argument, argInfo->get(c-firstArgIndex));
               }
            }
         }
      }
   if (tracePrex)
      traceMsg(vp->comp(), "PREX.vp: Done populating prex argInfo for %s %p\n", node->getOpCode().getName(), node);

   if ((vp->lastTimeThrough() || !node->getOpCode().isIndirect()) &&
       (vp->_isGlobalPropagation || !vp->getLastRun()) &&
       !methodSymbol->isInterface())
      vp->_devirtualizedCalls.add(new (vp->trStackMemory()) OMR::ValuePropagation::CallInfo(vp, thisType, argInfo));
   vp->invalidateUseDefInfo();
   vp->invalidateValueNumberInfo();
   }

TR::Node *constrainCall(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   if (vp->comp()->getSymRefTab()->element(TR_induceOSRAtCurrentPC) == node->getSymbolReference())
      {
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchOSR, NULL, node);
      }
   else
      {
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchUserThrows, NULL, node);
      }

   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();

   bool callIsDirect = !node->getOpCode().isIndirect();
#ifdef J9_PROJECT_SPECIFIC
   TR::RecognizedMethod rm = symbol->getRecognizedMethod();
   // Look for recognized converters call.
   if (vp->comp()->isConverterMethod(rm) &&
         vp->comp()->canTransformConverterMethod(rm))
      {
      if (!vp->_converterCalls.find(vp->_curTree))
         vp->_converterCalls.add(vp->_curTree);
      }

   if (vp->lastTimeThrough() && callIsDirect && TR::Compiler->om.canGenerateArraylets() &&
         node->getSymbol()->getMethodSymbol()->getMethod() &&
         (node->getSymbol()->getMethodSymbol()->getMethod()->isUnsafeWithObjectArg() ||
         (node->getSymbol()->getMethodSymbol()->getMethod()->isUnsafeCAS() && vp->comp()->getOption(TR_EnableInliningOfUnsafeForArraylets))))

         {
         // printf("adding method %s to inline to list - found in %s\n", resolvedMethod->signature(vp->comp()->trMemory()),vp->comp()->signature());fflush(stdout);
         bool operandGlobal;
         TR::VPConstraint *operand = vp->getConstraint(node->getChild(1), operandGlobal);
         if (operand && ((operand->isClassObject() == TR_yes) ||
                   (operand->getClassType() && ((operand->getClassType()->isArray() == TR_no) || (operand->getClassType()->isClassObject() == TR_yes)))))
                   {
           vp->_unsafeCallsToInline.add(new (vp->trStackMemory()) OMR::ValuePropagation::CallInfo(vp, NULL, NULL));
                   node->setUnsafeGetPutCASCallOnNonArray();
           // printf("change flag for node  %p\n",node);fflush(stdout);
           if (vp->trace())
              traceMsg(vp->comp(), "change unsafe flag for node  [%p]\n", node);
                   }
         }
#endif

   // For an indirect call, see if it can be devirtualized
   //
   if (node->getOpCode().isIndirect() && !debug("dontDevirtualize"))
      {
      devirtualizeCall(vp, node);
      symbol = node->getSymbol()->castToMethodSymbol();
      if (node->getSymbolReference()->isUnresolved() &&
          symbol->isInterface())
         {
         char *sig = symbol->getMethod()->classNameChars();
         int32_t len = symbol->getMethod()->classNameLength();
         sig = classNameToSignature(sig, len, vp->comp());
         TR_ResolvedMethod *method = node->getSymbolReference()->getOwningMethod(vp->comp());
         TR::VPConstraint *constraint = TR::VPUnresolvedClass::create(vp, sig, len, method);
         int32_t firstArgIndex = node->getFirstArgumentIndex();
         bool isGlobal;
         TR::VPConstraint *existingConstraint   = vp->getConstraint(node->getChild(firstArgIndex), isGlobal);
         if (existingConstraint &&
             existingConstraint->intersect(constraint, vp))
            {
            //dumpOptDetails(vp->comp(), "Reached here for call node %p in %s\n", node, vp->comp()->signature());
            }
         else
            {
            //dumpOptDetails(vp->comp(), "First time an interface call made using receiver at node %p in %s\n", node, vp->comp()->signature());
            vp->addBlockConstraint(node->getChild(firstArgIndex), constraint);
            }
         }
      }

   if (node->getType().isInt32() && vp->comp()->getOption(TR_AllowVPRangeNarrowingBasedOnDeclaredType))
      {
      // If this node is a TR::icalli then there may be
      // some value in parsing the return type of the method
      // being invoked to obtain better info about byte/bool/char/short
      //
      TR_Method *method = node->getSymbol()->castToMethodSymbol()->getMethod();
      if (method)
         {
         TR::DataType dataType = method->returnType();

         if (dataType == TR::Int8 || dataType == TR::Int16 || dataType == TR::Int32 || dataType == TR::Int64)
            {
            bool isUnsigned = method->returnTypeIsUnsigned();
            TR::VPConstraint *constraint = TR::VPIntRange::create(vp, dataType, isUnsigned ? TR_yes : TR_no);
            if (constraint)
               vp->addGlobalConstraint(node, constraint);
            }
         }
      }

   vp->constrainRecognizedMethod(node);

   // Return if the node is not a regular call (xcall/xcalli) anymore
   if (!node->getOpCode().isCall() || node->getSymbol()->castToMethodSymbol()->isHelper())
      return node;

   if ( symbol )
      {
#ifdef J9_PROJECT_SPECIFIC
      bool isIntegerAbs = (symbol->getRecognizedMethod() == TR::java_lang_Math_abs_I);
      bool isLongAbs =    (symbol->getRecognizedMethod() == TR::java_lang_Math_abs_L);
      bool isFloatAbs =   (symbol->getRecognizedMethod() == TR::java_lang_Math_abs_F);
      bool isDoubleAbs =  (symbol->getRecognizedMethod() == TR::java_lang_Math_abs_D);

      if (isIntegerAbs || isLongAbs)
         {
         // one special case to worry about here: if the operand of the ABS could be the minimum
         // value of the range (TR::getMinSigned<TR::Int32>() or TR::getMinSigned<TR::Int64>()) then the return value of the ABS could be
         // that same minimum value (yes, that's right, it could be negative).
         bool operandGlobal;
         TR::VPConstraint *operand = vp->getConstraint(node->getFirstChild(), operandGlobal);
         TR::VPConstraint *absConstraint = NULL;
         TR::VPConstraint *positiveConstraint = NULL;
         bool couldBeMinValue = false;

         if (isIntegerAbs)
            {
            positiveConstraint = TR::VPIntRange::create(vp, 0, TR::getMaxSigned<TR::Int32>());
            couldBeMinValue = (operand == NULL || operand->getLowInt() == TR::getMinSigned<TR::Int32>());
            }
         else
            {
            TR_ASSERT(isLongAbs,"");   // sanity check that no other cases get in here by mistake
            positiveConstraint = TR::VPLongRange::create(vp, 0, TR::getMaxSigned<TR::Int64>());
            couldBeMinValue = (operand == NULL || operand->getLowLong() == TR::getMinSigned<TR::Int64>());
            }

         if (couldBeMinValue)
            {
            TR::VPConstraint *minValueConstraint = NULL;
            if (isIntegerAbs)
               minValueConstraint = TR::VPIntConst::create(vp, TR::getMinSigned<TR::Int32>());
            else
               {
               TR_ASSERT(isLongAbs,"");   // sanity check that no other cases get in here by mistake
               minValueConstraint = TR::VPLongConst::create(vp, TR::getMinSigned<TR::Int64>());
               }
            absConstraint = TR::VPMergedConstraints::create(vp, minValueConstraint, positiveConstraint);
            }
         else
            absConstraint = positiveConstraint;
         vp->addGlobalConstraint(node, absConstraint);
         if (!couldBeMinValue)
            node->setIsNonNegative(true);
         node->setCannotOverflow(true);
         }
      else if (isFloatAbs || isDoubleAbs)
         {
         // TODO: when we implement FP constraints, need to add the 0,MAX_F or 0,MAX_D constraint here
         }
      else if (symbol->getRecognizedMethod() == TR::java_lang_Class_getComponentType)
         {
         TR::Node *classPointer = node->getFirstChild();
         if (classPointer->getOpCode().hasSymbolReference() &&
             (classPointer->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef()))
            {
            bool operandGlobal;
            TR::VPConstraint *operand = vp->getConstraint(classPointer->getFirstChild(), operandGlobal);
            if (operand)
               {
               TR::VPArrayInfo *arrayInfo = operand->getArrayInfo();
               if (arrayInfo ||
                   (operand->getClassType() && (operand->getClassType()->isArray() == TR_yes)))
                  {
                  vp->_javaLangClassGetComponentTypeCalls.add(node);
                  }
               }
            }
         }
      else if (symbol->getRecognizedMethod() == TR::java_lang_Integer_numberOfLeadingZeros
            || symbol->getRecognizedMethod() == TR::java_lang_Integer_numberOfTrailingZeros)
         {
         vp->addGlobalConstraint(node, TR::VPIntRange::create(vp, 0, 32));
         }
      else if (symbol->getRecognizedMethod() == TR::java_lang_Long_numberOfLeadingZeros
            || symbol->getRecognizedMethod() == TR::java_lang_Long_numberOfTrailingZeros)
         {
         vp->addGlobalConstraint(node, TR::VPIntRange::create(vp, 0, 64));
         }
      else if (vp->cg()->getSupportsNewObjectAlignment() &&
               symbol->getRecognizedMethod() == TR::java_util_TreeMap_rbInsert)
         {
         TR_ASSERT(node->getNumChildren()==2, "call to TreeMap.rbInsert should have only 2 children");
         TR::Node *valueChild = node->getSecondChild();
         TR::Node *definitionNode = NULL;
         if (valueChild->getOpCodeValue() == TR::aload)
            {
            TR_UseDefInfo *useDefInfo = vp->_useDefInfo;
            if (useDefInfo)
               {
               uint16_t useIndex = valueChild->getUseDefIndex();
               if (useDefInfo->isUseIndex(useIndex))
                  {
                  TR_UseDefInfo::BitVector defs(vp->comp()->allocator());
                  useDefInfo->getUseDef(defs, useIndex);
                  if (!defs.IsZero())
                     {
                     TR_UseDefInfo::BitVector::Cursor cursor(defs);
                     for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
                        {
                        int32_t defIndex = cursor;

                        if (defIndex < useDefInfo->getFirstRealDefIndex())
                           continue;

                        TR::TreeTop *defTree = useDefInfo->getTreeTop(defIndex);
                        TR::Node *defNode = defTree->getNode();

                        if (defNode && (defNode->getOpCodeValue() == TR::astore))
                           {
                           TR::Node *defChild = defNode->getFirstChild();
                           if (!defChild) continue;
                           if (defChild->getOpCodeValue() == TR::New)
                              {
                              definitionNode = defChild;
                              break;
                              }
                           else if (defChild->getOpCodeValue() == TR::aload)
                              {
                              uint16_t useIndexC = defChild->getUseDefIndex();
                              if (useDefInfo->isUseIndex(useIndexC))
                                 {
                                 TR_UseDefInfo::BitVector defsC(vp->comp()->allocator());
                                 useDefInfo->getUseDef(defsC, useIndexC);
                                 if (!defsC.IsZero())
                                    {
                                    TR_UseDefInfo::BitVector::Cursor cursorC(defsC);
                                    for (cursorC.SetToFirstOne(); cursorC.Valid(); cursorC.SetToNextOne())
                                       {
                                       int32_t defIndexC = cursorC;

                                       if (defIndexC < useDefInfo->getFirstRealDefIndex())
                                          continue;

                                       TR::TreeTop *defTreeC = useDefInfo->getTreeTop(defIndexC);
                                       TR::Node *defNodeC = defTreeC->getNode();

                                       if (defNodeC && (defNodeC->getOpCodeValue() == TR::astore))
                                          {
                                          TR::Node *defChildC = defNodeC->getFirstChild();
                                          if (defChildC && defChildC->getOpCodeValue() == TR::New)
                                             {
                                             definitionNode = defChildC;
                                             break;
                                             }
                                          }
                                       }
                                    }
                                 }
                              }
                           }
                           if (definitionNode)
                              break;
                        }
                     }
                  }
               }
            }
         if (definitionNode)
            {
            definitionNode->setAlignTLHAlloc(true);
            }
         }
      else if (callIsDirect &&
               //symbol->isNative() &&
               (symbol->getRecognizedMethod() == TR::java_lang_J9VMInternals_identityHashCode))
         {
         // new hashcode algorithm
         //
         TR_OpaqueClassBlock *jitHelpersClass = vp->comp()->getJITHelpersClassPointer();
         if (jitHelpersClass &&
               TR::Compiler->cls.isClassInitialized(vp->comp(), jitHelpersClass))
            {
            TR::SymbolReference *hashCodeMethodSymRef = NULL;
            TR::SymbolReference *getHelpersSymRef = NULL;
            TR_ScratchList<TR_ResolvedMethod> helperMethods(vp->trMemory());
            vp->comp()->fej9()->getResolvedMethods(vp->trMemory(), jitHelpersClass, &helperMethods);
            ListIterator<TR_ResolvedMethod> it(&helperMethods);
            for (TR_ResolvedMethod *m = it.getCurrent(); m; m = it.getNext())
               {
               char *sig = m->nameChars();
               if (!strncmp(sig, "hashCodeImpl", 11))
                  {
                  hashCodeMethodSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, m, TR::MethodSymbol::Virtual);
                  hashCodeMethodSymRef->setOffset(TR::Compiler->cls.vTableSlot(vp->comp(), m->getPersistentIdentifier(), jitHelpersClass));
                  }
               else if (!strncmp(sig, "getHelpers", 10))
                  {
                  getHelpersSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, m, TR::MethodSymbol::Static);
                  }
               }

            if (vp->comp()->getOption(TR_EnableJITHelpershashCodeImpl) && hashCodeMethodSymRef && getHelpersSymRef &&
                  performTransformation(vp->comp(), "%sChanging call to new hashCodeImpl at node [%p]\n", OPT_DETAILS, node))
               {
               //FIXME: add me to the list of calls to be inlined
               //
               TR_Method *method = getHelpersSymRef->getSymbol()->castToMethodSymbol()->getMethod();
               TR::Node *helpersCallNode = TR::Node::createWithSymRef( node, method->directCallOpCode(), 0, getHelpersSymRef);
               TR::TreeTop *helpersCallTT = TR::TreeTop::create(vp->comp(), TR::Node::create(TR::treetop, 1, helpersCallNode));
               vp->_curTree->insertBefore(helpersCallTT);

               method = hashCodeMethodSymRef->getSymbol()->castToMethodSymbol()->getMethod();
               TR::Node::recreate(node, method->directCallOpCode());
               TR::Node *removedChild = node->getFirstChild();
               //vp->removeNode(removedChild);
               removedChild->recursivelyDecReferenceCount();
               //TR::Node *objChild = node->getSecondChild();
               //node->removeAllChildren();
               //firstChild->decReferenceCount();
               node->setNumChildren(2);
               node->setAndIncChild(0, helpersCallNode);
               //node->setAndIncChild(1, objChild);
               node->setSymbolReference(hashCodeMethodSymRef);

               vp->invalidateUseDefInfo();
               vp->invalidateValueNumberInfo();
               return node;
               }
            }
         }
      else if (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_ensureClassInitialized)
         {
         /*
         n572n       vcall  sun/misc/Unsafe.ensureClassInitialized(Ljava/lang/Class;)V[#631  final native virtual Method -728]
            n577n         aload  java/lang/invoke/MethodHandle$UnsafeGetter.myUnsafe
            n579n         iaload  java/lang/invoke/MethodHandle.defc

         Because Unsafe.ensureClassInitialized is JNI, can't use direct JNI, otherwise can't optimized it at compile time.
         */
         TR::Node *classValueNode = node->getSecondChild();
         bool isGlobal;
         TR::VPConstraint *constraint = vp->getConstraint(classValueNode, isGlobal);
         if (constraint && constraint->getKnownObject() && constraint->isClassObject() == TR_yes && constraint->isNonNullObject())
            {
            TR::VPKnownObject *knowConstraint = constraint->getKnownObject();
            /* get known object class initialization status
             * 1. get J9class of known object
             * 2. get J9class->initializeStatus
             * 3. if J9class->initializeStatus value is 1, remove current call.
             */

            TR_OpaqueClassBlock *j9class = knowConstraint->getClass();
            if (j9class &&
                TR::Compiler->cls.isClassInitialized(vp->comp(), j9class) &&
                performTransformation(vp->comp(), "%s Remove TR::sun_misc_Unsafe_ensureClassInitialized call, class already initialized [%p]\n", OPT_DETAILS, node))
                {
                vp->removeNode(node);
                vp->_curTree->setNode(NULL);
                return NULL;
                }
            }
         }
#endif
      }

   if (node->getOpCode().isIndirect() &&
       !vp->_curTree->getNode()->getOpCode().isNullCheck() &&
       owningMethodDoesNotContainNullChecks(vp, node))
      vp->addBlockConstraint(node->getFirstChild(), TR::VPNonNullObject::create(vp));

   //sync required
   OMR::ValuePropagation::Relationship *syncRel = vp->findConstraint(vp->_syncValueNumber);
   TR::VPSync *sync = NULL;
   if (syncRel && syncRel->constraint)
      sync = syncRel->constraint->asVPSync();
   if (sync && sync->syncEmitted() == TR_yes)
      {
      vp->addConstraintToList(NULL, vp->_syncValueNumber, vp->AbsoluteConstraint, TR::VPSync::create(vp, TR_maybe), &vp->_curConstraints);
      if (vp->trace())
         {
         traceMsg(vp->comp(), "Setting syncRequired due to node [%p]\n", node);
         }
      }
   else
      {
      if (vp->trace())
         {
         if (sync)
            traceMsg(vp->comp(), "syncRequired is already setup at node [%p]\n", node);
         else
            traceMsg(vp->comp(), "No sync constraint found at node [%p]!\n", node);
         }
      }

   return node;
   }

#ifdef J9_PROJECT_SPECIFIC
void getHelperSymRefs(OMR::ValuePropagation *vp, TR::Node *curCallNode, TR::SymbolReference *&getHelpersSymRef, TR::SymbolReference *&helperSymRef, char *helperSig, int32_t helperSigLen, TR::MethodSymbol::Kinds helperCallKind)
   {
   //Function to retrieve the JITHelpers.getHelpers and JITHelpers.<helperSig> method symbol references.
   //
   TR_OpaqueClassBlock *jitHelpersClass = vp->comp()->getJITHelpersClassPointer();

   //If we can't find the helper class, or it isn't initalized, return.
   //
   if (!jitHelpersClass || !TR::Compiler->cls.isClassInitialized(vp->comp(), jitHelpersClass))
      return;

   TR_ScratchList<TR_ResolvedMethod> helperMethods(vp->trMemory());
   vp->comp()->fej9()->getResolvedMethods(vp->trMemory(), jitHelpersClass, &helperMethods);
   ListIterator<TR_ResolvedMethod> it(&helperMethods);

   //Find the symRefs
   //
   for (TR_ResolvedMethod *m = it.getCurrent(); m; m = it.getNext())
      {
      char *sig = m->nameChars();
      //printf("Here is the sig %s and the passed in %s \n", sig,helperSig);
      if (!strncmp(sig, helperSig, helperSigLen))
         {
         if (TR::MethodSymbol::Virtual == helperCallKind)
            {
            //REVISIT FOR IMPL HASH CODE***
            //
            helperSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, m, TR::MethodSymbol::Virtual);
            helperSymRef->setOffset(TR::Compiler->cls.vTableSlot(vp->comp(), m->getPersistentIdentifier(), jitHelpersClass));
            }
         else
            {
            helperSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(curCallNode->getSymbolReference()->getOwningMethodIndex(), -1, m, helperCallKind);
            }
         //printf("found gethelpers, 0x%x \n", helperSymRef);
         }
      else if (!strncmp(sig, "jitHelpers", 10))
         {
         //printf("found helpers");
         getHelpersSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, m, TR::MethodSymbol::Static);
         }
      }
   }
#endif

#ifdef J9_PROJECT_SPECIFIC
void transformToOptimizedCloneCall(OMR::ValuePropagation *vp, TR::Node *node, bool isDirectCall)
   {
   TR::SymbolReference *getHelpersSymRef = NULL;
   TR::SymbolReference *optimizedCloneSymRef = NULL;

   getHelperSymRefs(vp, node, getHelpersSymRef, optimizedCloneSymRef, "optimizedClone", 14, TR::MethodSymbol::Special);

   //printf("helper sym 0x%x, optsym 0x%x \n", getHelpersSymRef, optimizedCloneSymRef);

   if (optimizedCloneSymRef && getHelpersSymRef && performTransformation(vp->comp(), "%sChanging call to new optimizedClone at node [%p]\n", OPT_DETAILS, node))
        {
        //FIXME: add me to the list of calls to be inlined
        //
        TR_Method *method = optimizedCloneSymRef->getSymbol()->castToMethodSymbol()->getMethod();
        TR::Node *helpersCallNode = TR::Node::createWithSymRef(node, method->directCallOpCode(), 0, getHelpersSymRef);
        TR::TreeTop *helpersCallTT = TR::TreeTop::create(vp->comp(), TR::Node::create(TR::treetop, 1, helpersCallNode));
        vp->_curTree->insertBefore(helpersCallTT);

        method = optimizedCloneSymRef->getSymbol()->castToMethodSymbol()->getMethod();
        TR::Node::recreate(node, method->directCallOpCode());
        TR::Node *firstChild = node->getFirstChild();
        firstChild->decReferenceCount();
        node->setNumChildren(2);
        node->setAndIncChild(0, helpersCallNode);
        node->setAndIncChild(1, firstChild);
        node->setSymbolReference(optimizedCloneSymRef);
        vp->invalidateUseDefInfo();
        vp->invalidateValueNumberInfo();
        }
//printf("TRANSFORMED \n");
   }
#endif


TR::Node *transformCloneCallSetup(OMR::ValuePropagation *vp, TR::Node *node, TR::VPConstraint *constraint, bool isDirectCall)
   {
   //Change the call and agruments from object.clone to optimizedClone.
   //
   vp->_objectCloneCallNodes.add(node);
   vp->_objectCloneCallTreeTops.add(vp->_curTree);
   /*
   if (isDirectCall && constraint && constraint->getClass())
      {
      //Determine is we can create a new instance object of the source's class.
      //
      TR_OpaqueClassBlock *clazz = constraint->getClass();
      if (constraint->isClassObject() == TR_yes)
         clazz = vp->fe()->getClassClassPointer(clazz);

      //If we do not know the class, or the class is an array, pass NULL as the second parameter.
      if (clazz && (((TR::Compiler->cls.classDepthOf(clazz) == 0) && !constraint->isFixedClass()) || (constraint->getClassType()->isArray() != TR_no)))
        clazz = NULL;

      vp->_objectCloneInstanceClasses.add(clazz);
      }
   else
      {
      vp->_objectCloneInstanceClasses.add(NULL);
      }
   */
   return node;
   }

#ifdef J9_PROJECT_SPECIFIC
TR::Node *setCloneClassInNode(OMR::ValuePropagation *vp, TR::Node *node, TR::VPConstraint *constraint, bool isGlobal)
   {

   // If the child is a resolved class type, hide the class pointer in the
   // second child
   //

   if(!node->isProcessedByCallCloneConstrain())
      {
      node->setSecond(NULL);
      node->setProcessedByCallCloneConstrain();
      }

   if (constraint && constraint->getClass())
      {
      TR_OpaqueClassBlock *clazz = constraint->getClass();
      if (constraint->isClassObject() == TR_yes)
         clazz = vp->fe()->getClassClassPointer(clazz);

      if (clazz && (TR::Compiler->cls.classDepthOf(clazz) == 0) &&
          !constraint->isFixedClass())
         clazz = NULL;

      if (node->getCloneClassInNode() &&
          clazz &&
          (node->getCloneClassInNode() != clazz))
         {
         TR_YesNoMaybe answer = vp->fe()->isInstanceOf(clazz, node->getCloneClassInNode(), true, true);
         if (answer != TR_yes)
            clazz = node->getCloneClassInNode();
         }
      if (performTransformation(vp->comp(), "%sSetting type on Object.Clone acall node [%p] to [%p]\n", OPT_DETAILS, node, clazz))
         node->setSecond((TR::Node*)clazz);
      }
   return node;
   }
#endif

TR::Node *constrainAcall(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainCall(vp, node);

   // Return if the node is not a regular call (xcall/xcalli) anymore
   if (!node->getOpCode().isCall() || node->getSymbol()->castToMethodSymbol()->isHelper())
      return node;

   // This node can be constrained by the return type of the method.
   //
   TR::VPConstraint *constraint = NULL;
   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::ResolvedMethodSymbol *method = symRef->getSymbol()->getResolvedMethodSymbol();

#ifdef J9_PROJECT_SPECIFIC
   // For the special case of a direct call to Object.clone() the return type
   // will be the same as the type of the "this" argument, which may be more
   // precise than the declared return type of "Object".
   //
   if (method)
      {
      if (!node->getOpCode().isIndirect())
         {
         static char *enableDynamicObjectClone = feGetEnv("TR_enableDynamicObjectClone");
         if (method->getRecognizedMethod() == TR::java_lang_Object_clone
             || method->getRecognizedMethod() == TR::java_lang_J9VMInternals_primitiveClone)
            {
            bool isGlobal;
            if (method->getRecognizedMethod() == TR::java_lang_Object_clone)
              constraint = vp->getConstraint(node->getFirstChild(), isGlobal);
            else
              constraint = vp->getConstraint(node->getLastChild(), isGlobal);

            TR::VPResolvedClass *newTypeConstraint = NULL;
            if (constraint)
               {
               // Do nothing if the class of the object doesn't implement Cloneable
               if (constraint->getClass() && !vp->comp()->fej9()->isCloneable(constraint->getClass()))
                  {
                  if (vp->trace())
                     traceMsg(vp->comp(), "Object Clone: Class of node %p is not cloneable, quit\n", node);

                  TR::DebugCounter::incStaticDebugCounter(vp->comp(), TR::DebugCounter::debugCounterName(vp->comp(), "inlineClone/unsuitable/(%s)/%s/block_%d", vp->comp()->signature(), vp->comp()->getHotnessName(vp->comp()->getMethodHotness()), vp->_curTree->getEnclosingBlock()->getNumber()));

                  return node;
                  }
               if ( constraint->isFixedClass() )
                  {
                  newTypeConstraint = TR::VPFixedClass::create(vp, constraint->getClass());

                  if (!vp->comp()->compileRelocatableCode())
                     {
                     if (constraint->getClassType()
                         && constraint->getClassType()->isArray() == TR_no
                         && !vp->_objectCloneCalls.find(vp->_curTree))
                        {
                        vp->_objectCloneCalls.add(vp->_curTree);
                        vp->_objectCloneTypes.add(new (vp->trStackMemory()) OMR::ValuePropagation::ObjCloneInfo(constraint->getClass(), true));
                        }
                     else if (constraint->getClassType()
                              && constraint->getClassType()->isArray() == TR_yes
                              && !vp->_arrayCloneCalls.find(vp->_curTree))
                        {
                        vp->_arrayCloneCalls.add(vp->_curTree);
                        vp->_arrayCloneTypes.add(constraint->getClass());
                        }
                     }
                  }
               else if ( constraint->getClassType()
                         && constraint->getClassType()->asResolvedClass() )
                  {
                  newTypeConstraint = TR::VPResolvedClass::create(vp, constraint->getClass());
                  if (enableDynamicObjectClone
                      && constraint->getClassType()->isArray() == TR_no
                      && !vp->_objectCloneCalls.find(vp->_curTree))
                     {
                     vp->_objectCloneCalls.add(vp->_curTree);
                     vp->_objectCloneTypes.add(new (vp->trStackMemory()) OMR::ValuePropagation::ObjCloneInfo(constraint->getClass(), false));
                     }
                  }
               }

            if (!constraint || (!constraint->isFixedClass()
                && (enableDynamicObjectClone && !(constraint->getClassType() && constraint->getClassType()->asResolvedClass() && constraint->getClassType()->isArray() == TR_no))))
               TR::DebugCounter::incStaticDebugCounter(vp->comp(), TR::DebugCounter::debugCounterName(vp->comp(), "inlineClone/miss/(%s)/%s/block_%d", vp->comp()->signature(), vp->comp()->getHotnessName(vp->comp()->getMethodHotness()), vp->_curTree->getEnclosingBlock()->getNumber()));
            else
               TR::DebugCounter::incStaticDebugCounter(vp->comp(), TR::DebugCounter::debugCounterName(vp->comp(), "inlineClone/hit/(%s)/%s/block_%d", vp->comp()->signature(), vp->comp()->getHotnessName(vp->comp()->getMethodHotness()), vp->_curTree->getEnclosingBlock()->getNumber()));

            TR::VPClassPresence *cloneResultNonNull = TR::VPNonNullObject::create(vp);
            TR::VPObjectLocation *cloneResultOnHeap = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::HeapObject);
            TR::VPArrayInfo *cloneResultArrayInfo = NULL;
            if (constraint)
               cloneResultArrayInfo = constraint->getArrayInfo();
            TR::VPConstraint *newConstraint = TR::VPClass::create(vp, newTypeConstraint, cloneResultNonNull, NULL, cloneResultArrayInfo, cloneResultOnHeap);

            // This constraint can be global because the result of the clone call
            // needs to have its own value number.
            vp->addGlobalConstraint(node, newConstraint);

            if (method->getRecognizedMethod() == TR::java_lang_Object_clone
                && (constraint && !constraint->isFixedClass()))
               node = setCloneClassInNode(vp, node, newConstraint, isGlobal);

            // OptimizedClone
            if(vp->comp()->getOption(TR_EnableJITHelpersoptimizedClone) && newTypeConstraint)
               transformToOptimizedCloneCall(vp, node, true);
            return node;
            }
         else if (method->getRecognizedMethod() == TR::java_math_BigDecimal_valueOf)
            {
            TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(vp->comp());
            TR_OpaqueClassBlock *classObject = vp->fe()->getClassFromSignature("java/math/BigDecimal", 20, owningMethod);
            if (classObject)
               {
               constraint = TR::VPFixedClass::create(vp, classObject);
               vp->addGlobalConstraint(node, constraint);
               vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
               }
            }
         }
      else
         {
         if ((method->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
             (method->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
             (method->getRecognizedMethod() == TR::java_math_BigDecimal_multiply))
            {
            bool isGlobal;
            constraint = vp->getConstraint(node->getSecondChild(), isGlobal);
            TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(vp->comp());
            TR_OpaqueClassBlock * bigDecimalClass = vp->fe()->getClassFromSignature("java/math/BigDecimal", 20, owningMethod);
            //traceMsg(vp->comp(), "child %p big dec class %p\n", constraint, bigDecimalClass);
            if (constraint && bigDecimalClass &&
                constraint->isFixedClass() &&
                (bigDecimalClass == constraint->getClass()))
               {
               TR::VPConstraint *newConstraint = TR::VPFixedClass::create(vp, bigDecimalClass);
               vp->addBlockOrGlobalConstraint(node, newConstraint,isGlobal);
               vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
               return node;
               }
            }
         }
      }
#endif

   // The rest of the code seems to be Java specific
   if (!vp->comp()->getCurrentMethod()->isJ9())
      return node;

   int32_t len = 0;
   const char * sig = symRef->getTypeSignature(len);

   if (sig == NULL)  // helper
       return node;

   TR_ASSERT(sig[0] == 'L' || sig[0] == '[', "Ref call return type is not a class");

   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
   TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(vp->comp());
   TR_OpaqueClassBlock *classBlock = vp->fe()->getClassFromSignature(sig, len, owningMethod);
   if (  classBlock
      && TR::Compiler->cls.isInterfaceClass(vp->comp(), classBlock)
      && !vp->comp()->getOption(TR_TrustAllInterfaceTypeInfo))
      {
      // Can't trust interface type info coming from method return value
      classBlock = NULL;
      }
   if (classBlock)
      {
      TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(classBlock);
      if (jlClass)
         {
         if (classBlock != jlClass)
            constraint = TR::VPClassType::create(vp, sig, len, owningMethod, false, classBlock);
         else
            constraint = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject);
         vp->addGlobalConstraint(node, constraint);
         }
      }
   else if (symRef->isUnresolved() && symbol && !symbol->isInterface())
      {
      TR::VPConstraint *constraint = TR::VPUnresolvedClass::create(vp, sig, len, owningMethod);
      vp->addGlobalConstraint(node, constraint);
      }

   return node;
   }

// handles unsigned too
TR::Node *constrainAdd(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;

   bool longAdd = node->getOpCode().isLong();
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   TR::VPConstraint *constraint = NULL;
   if (lhs && rhs)
      {
      constraint = lhs->add(rhs, node->getDataType(), vp);
      if (constraint)
         {
         if (longAdd)
            {
            if (constraint->asLongConst())
               {
               vp->replaceByConstant(node, constraint, lhsGlobal);
               return node;
               }
            }
         else
            {
            if (constraint->asIntConst())
               {
               vp->replaceByConstant(node, constraint, lhsGlobal);
               return node;
               }
            }

         bool didReduction = false;
         if (longAdd)
            didReduction = reduceLongOpToIntegerOp(vp, node, constraint);

         vp->addBlockOrGlobalConstraint(node, constraint,lhsGlobal);
         // If we reduced long to integer, node is now an i2l; not safe to do any further addition-related things with node
         if (didReduction)
          return node;
         }
      }

   // If the rhs is a constant create a relative constraint between this node
   // and the lhs child
   //
   if (rhs)
      {
      constraint = NULL;
      if (rhs->asLongConst())
         {
         if (rhs->asLongConst()->getLong() > TR::getMinSigned<TR::Int32>() &&
             rhs->asLongConst()->getLong() < TR::getMaxSigned<TR::Int32>())
            constraint = TR::VPEqual::create(vp, (int32_t)rhs->asLongConst()->getLong());
         }
      else if (rhs->asIntConst())
         {
         //TODO: propagate the unsigned property here
         // to the TR::VPRelation constraint
         // the code below is conservative for unsigned &
         // we can do better (for unsigned) by checking the
         // appropriate ranges; but the downside is that the 'increment'
         // of the Equal constraint needs to be used in the right
         // type context
         if (rhs->asIntConst()->getInt() > TR::getMinSigned<TR::Int32>() &&
             rhs->asIntConst()->getInt() < TR::getMaxSigned<TR::Int32>())
            constraint = TR::VPEqual::create(vp, rhs->asIntConst()->getInt());
         }
      if (constraint)
         {
         if (rhsGlobal)
            vp->addGlobalConstraint(node, constraint, node->getFirstChild());
         else
            vp->addBlockConstraint(node, constraint, node->getFirstChild());
         }
      }

   if (longAdd)
      {
      if (vp->isHighWordZero(node))
         node->setIsHighWordZero(true);
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

// handles unsigned subtract
TR::Node *constrainSubtract(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;

   bool longSub = node->getOpCode().isLong();
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   TR::VPConstraint *constraint = NULL;
   if (lhs && rhs)
      {
      constraint = lhs->subtract(rhs, node->getDataType(), vp);
      if (constraint)
         {
         if (longSub)
            {
            if (constraint->asLongConst())
               {
               vp->replaceByConstant(node, constraint, lhsGlobal);
               return node;
               }
            }
         else
            {
            if (constraint->asIntConst())
               {
               vp->replaceByConstant(node, constraint, lhsGlobal);
               return node;
               }

            if (constraint->asShortConst())
               {
               vp->replaceByConstant(node,constraint,lhsGlobal);
               return node;
               }

            }

         bool didReduction = false;
         if (longSub)
          didReduction = reduceLongOpToIntegerOp(vp, node, constraint);
         vp->addBlockOrGlobalConstraint(node, constraint,lhsGlobal);
         // If we reduced long to integer, node is now an i2l; not safe to do any further subtract-related things with node
         if (didReduction)
          return node;
         }
      }

   // If the rhs is a constant create a relative constraint between this node
   // and the lhs child
   //
   if (rhs)
      {
      constraint = NULL;
      if (rhs->asLongConst())
         {
         if (rhs->asLongConst()->getLong() > TR::getMinSigned<TR::Int32>() &&
             rhs->asLongConst()->getLong() < TR::getMaxSigned<TR::Int32>())
            constraint = TR::VPEqual::create(vp, -(int32_t)rhs->asLongConst()->getLong());
         }
      else if (rhs->asIntConst())
         {
         // TODO: propagate unsigned property
         // to the TR::VPRelation constraint;
         // we can do better for unsigned here
         // see comment above for add
         if (rhs->asIntConst()->getInt() > TR::getMinSigned<TR::Int32>() &&
             rhs->asIntConst()->getInt() < TR::getMaxSigned<TR::Int32>())
         constraint = TR::VPEqual::create(vp, -rhs->asIntConst()->getInt());
         }
      else if(rhs->asShortConst())
         {
         if (rhs->asShortConst()->getShort() > TR::getMinSigned<TR::Int16>() &&
             rhs->asShortConst()->getShort() < TR::getMaxSigned<TR::Int16>())
          constraint = TR::VPEqual::create(vp, -rhs->asShortConst()->getShort());
         }
      if (constraint)
         {
         if (rhsGlobal)
            vp->addGlobalConstraint(node, constraint, node->getFirstChild());
         else
            vp->addBlockConstraint(node, constraint, node->getFirstChild());
         }
      }

   if (longSub)
      {
      if (vp->isHighWordZero(node))
         node->setIsHighWordZero(true);
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainImul(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   //bool isUnsigned = node->getType().isUnsignedInt();

   if (lhs && rhs)
      {
      TR::VPConstraint *constraint = NULL;
      if (lhs->asIntConst() && rhs->asIntConst())
         {
         //if (isUnsigned)
         //   constraint = TR::VPIntConst::create(vp, ((uint32_t)lhs->asIntConst()->getInt()*(uint32_t)rhs->asIntConst()->getInt()), isUnsigned);
         //else
            constraint = TR::VPIntConst::create(vp, lhs->asIntConst()->getInt()*rhs->asIntConst()->getInt());
         }
      else
         {
         int64_t lowerLowerLimit = (int64_t)lhs->getLowInt() * (int64_t)rhs->getLowInt();
         int64_t lowerUpperLimit = (int64_t)lhs->getLowInt() * (int64_t)rhs->getHighInt();
         int64_t upperLowerLimit = (int64_t)lhs->getHighInt() * (int64_t)rhs->getLowInt();
         int64_t upperUpperLimit = (int64_t)lhs->getHighInt() * (int64_t)rhs->getHighInt();
         int64_t tempLower1, tempLower2, lowest;
         int64_t tempHigher1, tempHigher2, highest;
         if (lowerLowerLimit < lowerUpperLimit)
            {
            tempLower1  = lowerLowerLimit;
            tempHigher1 = lowerUpperLimit;
            }
         else
            {
            tempLower1  = lowerUpperLimit;
            tempHigher1 = lowerLowerLimit;
            }
         if (upperLowerLimit < upperUpperLimit)
            {
            tempLower2  = upperLowerLimit;
            tempHigher2 = upperUpperLimit;
            }
         else
            {
            tempLower2  = upperUpperLimit;
            tempHigher2 = upperLowerLimit;
            }

         if (tempLower1 < tempLower2)
            {
            lowest = tempLower1;
            }
         else
            {
            lowest = tempLower2;
            }

         if (tempHigher1 > tempHigher2)
            {
            highest = tempHigher1;
            }
         else
            {
            highest = tempHigher2;
            }

         if (lowest < TR::getMinSigned<TR::Int32>() || highest > TR::getMaxSigned<TR::Int32>())
            {
            constraint = NULL;
            }
         else
            {
            constraint = TR::VPIntRange::create(vp, (int32_t)lowest, (int32_t)highest/*, isUnsigned*/);
            }
         }
      if (constraint)
         {
         if (constraint->asIntConst())
            {
            vp->replaceByConstant(node, constraint, lhsGlobal);
            return node;
            }

         vp->addBlockOrGlobalConstraint(node, constraint,lhsGlobal);
         }
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }


TR::Node *constrainIumul(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   //bool isUnsigned = node->getType().isUnsignedInt();

   if (lhs && rhs)
      {
      TR::VPConstraint *constraint = NULL;
      if (lhs->asIntConst() && rhs->asIntConst())
         {
         //if (isUnsigned)
         //   constraint = TR::VPIntConst::create(vp, ((uint32_t)lhs->asIntConst()->getInt()*(uint32_t)rhs->asIntConst()->getInt()), isUnsigned);
         //else
            constraint = TR::VPIntConst::create(vp, lhs->asIntConst()->getInt()*rhs->asIntConst()->getInt());
         }
      else
         {
         uint64_t lowerLowerLimit = (uint64_t)lhs->getLowInt() * (uint64_t)rhs->getLowInt();
         uint64_t lowerUpperLimit = (uint64_t)lhs->getLowInt() * (uint64_t)rhs->getHighInt();
         uint64_t upperLowerLimit = (uint64_t)lhs->getHighInt() * (uint64_t)rhs->getLowInt();
         uint64_t upperUpperLimit = (uint64_t)lhs->getHighInt() * (uint64_t)rhs->getHighInt();
         uint64_t tempLower1 = 0, tempLower2 = 0, lowest = 0;
         uint64_t tempHigher1 = 0, tempHigher2 = 0, highest = 0;
         if (lowerLowerLimit < lowerUpperLimit)
            {
            tempLower1  = lowerLowerLimit;
            tempHigher1 = lowerUpperLimit;
            }
         else
            {
            tempLower1  = lowerUpperLimit;
            tempHigher1 = lowerLowerLimit;
            }
         if (upperLowerLimit < upperUpperLimit)
            {
            tempLower2  = upperLowerLimit;
            tempHigher2 = upperUpperLimit;
            }
         else
            {
            tempLower2  = upperUpperLimit;
            tempHigher2 = upperLowerLimit;
            }

         if (tempLower1 < tempLower2)
            {
            lowest = tempLower1;
            }
         else
            {
            lowest = tempLower2;
            }

         if (tempHigher1 > tempHigher2)
            {
            highest = tempHigher1;
            }
         else
            {
            highest = tempHigher2;
            }

         if (lowest < TR::getMinUnsigned<TR::Int32>() || highest > TR::getMaxUnsigned<TR::Int32>())
            {
            constraint = NULL;
            }
         else
            {
            constraint = TR::VPIntRange::create(vp, (int32_t)lowest, (int32_t)highest/*, isUnsigned*/);
            }
         }
      if (constraint)
         {
         if (constraint->asIntConst())
            {
            vp->replaceByConstant(node, constraint, lhsGlobal);
            return node;
            }

         vp->addBlockOrGlobalConstraint(node, constraint,lhsGlobal);
         }
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

// maxValue should either be TR::getMaxUnsigned<TR::Int32>() or TR::getMaxSigned<TR::Int32>(); the latter is used
// when this method is called with (positive) signed numbers
bool can64BitUnsignedMultiplyOverflow(uint64_t a, uint64_t b, uint64_t maxValue)
  {
  // a must be the lower number
  if (a > b)
    return can64BitUnsignedMultiplyOverflow(b, a, maxValue);

  // If both numbers are >= 2^32, the product must be >= 2^64
  if (a > TR::getMaxUnsigned<TR::Int32>())
    return true;

  // If both numbers are < 2^32, the product must be < 2^64
  if (b <= TR::getMaxUnsigned<TR::Int32>())
    return false;

  // a is < 2^32; b is >= 2^32
  // If the high word of b is zero, we're multiplying two numbers both less than 2^32, which is < 2^64, so no overflow.
  // If the high word of b is non-zero, the final product x will be ((a * HIGHWORD(b)) << 32) + (a * LOWWORD(b))
  // If we compute the highword of the above expression in a 64-bit variable, a value >= 2^32 indicates overflow
  uint64_t highWord = b >> 32;
  uint64_t lowWord = b & 0xFFFFFFFF;
  uint64_t lowWordOverflow = (a * lowWord) >> 32; // Can't overflow 64 bits because a and lowWord are both < 2^32

  // The high word of a * b will be (a * highWord) + lowWordOverflow.
  // This value could possibly overflow, so check it separately.

  // If the first part is >= 2^32, a * b will overflow
  if (a * highWord > maxValue)
    return true;

  // a * highWord is < 2^32 and lowWordOverflow is < 2^32, so it's safe to add them
  // If the sum is > 2^32, a * b will overflow
  if ((a * highWord) + lowWordOverflow > maxValue)
    return true;

  return false;
  }

bool can64BitSignedMultiplyOverflow(int64_t a, int64_t b)
  {
  // Multiplying by 0 or 1 clearly cannot overflow
  if (a == 0 || b == 0 || a == 1 || b == 1)
    return false;

  // TR::getMinSigned<TR::Int64>() * anything but 0 or 1 will overflow
  if (a == TR::getMinSigned<TR::Int64>() || b == TR::getMinSigned<TR::Int64>())
    return true;

  // If both numbers are < 2^32, the product must be < 2^64
  if (a >= TR::getMinSigned<TR::Int32>() && a <= TR::getMaxSigned<TR::Int32>() && b >= TR::getMinSigned<TR::Int32>() && b <= TR::getMaxSigned<TR::Int32>())
    return false;

  // If both numbers require more than 32 bits, the product must be > 64 bits
  if ((a < TR::getMinSigned<TR::Int32>() || a > TR::getMaxSigned<TR::Int32>()) && (b < TR::getMinSigned<TR::Int32>() || b > TR::getMaxSigned<TR::Int32>()))
    return true;

  // One number can be represented in 32 bits; the other requires 64
  // If both numbers are positive, use the unsigned code, but limit the high word
  // of the result to TR::getMaxSigned<TR::Int32>()
  if (a > 0 && b > 0)
    return can64BitUnsignedMultiplyOverflow((uint64_t)a, (uint64_t)b, TR::getMaxSigned<TR::Int32>());

  // If both are negative, multiply both by -1 and use the unsigned code
  if (a < 0 && a != TR::getMinSigned<TR::Int64>() && b < 0 && b != TR::getMinSigned<TR::Int64>())
    return can64BitUnsignedMultiplyOverflow((uint64_t)(-1 * a), (uint64_t)(-1 * b), TR::getMaxSigned<TR::Int32>());

  // Only remaining case is one is positive, one is negative, and neither is 0 nor TR::getMinSigned<TR::Int64>()
  // Treat both numbers as positive and use the unsigned code
  if (a < 0)
    return can64BitUnsignedMultiplyOverflow((uint64_t)(-1 * a), (uint64_t)b, TR::getMaxSigned<TR::Int32>());
  else if (b < 0)
    return can64BitUnsignedMultiplyOverflow((uint64_t)a, (uint64_t)(-1 * b), TR::getMaxSigned<TR::Int32>());

  return false;
  }

bool can64BitMultiplyOverflow(uint64_t a, uint64_t b, bool isUnsigned)
  {
  if (isUnsigned)
    return can64BitUnsignedMultiplyOverflow(a, b, TR::getMaxUnsigned<TR::Int32>());
  else
    return can64BitSignedMultiplyOverflow((int64_t)a, (int64_t)b);
  }

TR::Node *constrainLmul(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;

   if (lhs && rhs)
      {
      TR::VPConstraint *constraint = NULL;
      if (lhs->asLongConst() && rhs->asLongConst())
         {
        constraint = TR::VPLongConst::create(vp, lhs->asLongConst()->getLong() * rhs->asLongConst()->getLong());
        vp->replaceByConstant(node, constraint, lhsGlobal);
         }
      else
         {
         // If no combination overflows, it's safe to create a constraint
         if (!(can64BitMultiplyOverflow(lhs->getLowLong(), rhs->getLowLong(), node->getOpCode().isUnsigned()) ||
           can64BitMultiplyOverflow(lhs->getLowLong(), rhs->getHighLong(), node->getOpCode().isUnsigned()) ||
           can64BitMultiplyOverflow(lhs->getHighLong(), rhs->getLowLong(), node->getOpCode().isUnsigned()) ||
           can64BitMultiplyOverflow(lhs->getHighLong(), rhs->getHighLong(), node->getOpCode().isUnsigned())))
          {
           int64_t lowerLowerLimit = lhs->getLowLong() * rhs->getLowLong();
           int64_t lowerUpperLimit = lhs->getLowLong() * rhs->getHighLong();
           int64_t upperLowerLimit = lhs->getHighLong() * rhs->getLowLong();
           int64_t upperUpperLimit = lhs->getHighLong() * rhs->getHighLong();
           int64_t tempLower1, tempLower2, lowest;
           int64_t tempHigher1, tempHigher2, highest;
           if (lowerLowerLimit < lowerUpperLimit)
              {
              tempLower1  = lowerLowerLimit;
              tempHigher1 = lowerUpperLimit;
              }
           else
              {
              tempLower1  = lowerUpperLimit;
              tempHigher1 = lowerLowerLimit;
              }
           if (upperLowerLimit < upperUpperLimit)
              {
              tempLower2  = upperLowerLimit;
              tempHigher2 = upperUpperLimit;
              }
           else
              {
              tempLower2  = upperUpperLimit;
              tempHigher2 = upperLowerLimit;
              }

           if (tempLower1 < tempLower2)
              {
              lowest = tempLower1;
              }
           else
              {
              lowest = tempLower2;
              }

           if (tempHigher1 > tempHigher2)
              {
              highest = tempHigher1;
              }
           else
              {
              highest = tempHigher2;
              }

           constraint = TR::VPLongRange::create(vp, lowest, highest);
           }

        if (constraint)
           {
           if (constraint->asLongConst())
              {
              vp->replaceByConstant(node, constraint, lhsGlobal);
              return node;
              }

           bool didReduction = reduceLongOpToIntegerOp(vp, node, constraint);
            vp->addBlockOrGlobalConstraint(node, constraint,lhsGlobal);
           if (didReduction)
            return node;
           }
         }
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

static bool isUnsafeDivide(int64_t dividend, int64_t divisor)
   {
   return (dividend == TR::getMinSigned<TR::Int64>() && divisor == -1);
   }
// Given the ranges for a dividend and divisor, compute the range of the result
bool constrainIntegerDivisionRange(int64_t lhsLow, int64_t lhsHigh, int64_t rhsLow, int64_t rhsHigh, int64_t rangeMin, int64_t rangeMax, int64_t &resultLow, int64_t &resultHigh, bool allowZeroInDivisorRange)
   {
  // No matter what, division by constant 0 isn't something we'll ever simplify
   if (rhsLow == 0 && rhsHigh == 0)
      return false;

   if (!allowZeroInDivisorRange && rhsLow <= 0 && rhsHigh >= 0)
      return false;

   // The range rhsLow, rhsHigh may include 0. If it does, adjust or split the range so it doesn't include 0, since integer division by 0 is
   // meaningless and should be handled by an exception when executed. In other words, this method determines the range of the result
   // assuming the divisor is valid.
   if (rhsLow == 0)
      rhsLow = 1;

   if (rhsHigh == 0)
      rhsHigh = -1;

   resultLow = rangeMax;
   resultHigh = rangeMin;

   if (isUnsafeDivide(lhsLow, rhsLow) || isUnsafeDivide(lhsLow, rhsHigh) ||
       isUnsafeDivide(lhsHigh, rhsLow) || isUnsafeDivide(lhsHigh, rhsHigh))
      return false;

   if (rhsLow < 0 && rhsHigh > 0)
      {
      if (isUnsafeDivide(lhsLow, -1) || isUnsafeDivide(lhsHigh, -1))
         return false;
      int64_t computed[8];
      computed[0] = lhsLow / rhsLow;
      computed[1] = lhsLow / -1;
      computed[2] = lhsLow;
      computed[3] = lhsLow / rhsHigh;

      computed[4] = lhsHigh / rhsLow;
      computed[5] = lhsHigh / -1;
      computed[6] = lhsHigh;
      computed[7] = lhsHigh / rhsHigh;

      for(int i=0; i<8; i++)
         {
         resultLow = std::min(resultLow, computed[i]);
         resultHigh = std::max(resultHigh, computed[i]);
         }
      }
   else
      {

      int64_t computed[4];
      computed[0] = lhsLow / rhsLow;
      computed[1] = lhsLow / rhsHigh;

      computed[2] = lhsHigh / rhsLow;
      computed[3] = lhsHigh / rhsHigh;

      for(int i=0; i<4; i++)
         {
         resultLow = std::min(resultLow, computed[i]);
         resultHigh = std::max(resultHigh, computed[i]);
         }
      }

   if (resultLow < rangeMin)
      resultLow = rangeMin;

   if (resultHigh > rangeMax)
      resultHigh = rangeMax;

   if (resultLow > resultHigh)
      return false; // doesn't make sense

   return true;
   }

TR::Node *cloneDivForDivideByZeroCheck(OMR::ValuePropagation *vp, TR::Node *divNode)
   {
   TR::Node *divCopy = TR::Node::create(divNode, divNode->getOpCodeValue(), 2);
   divCopy->setAndIncChild(0, divNode->getFirstChild());
   divCopy->setAndIncChild(1, divNode->getSecondChild());
   divCopy->incReferenceCount();
   return divCopy;
   }

bool doesRangeContainZero(int64_t rangeMin, int64_t rangeMax)
   {
   if (rangeMin == 0 || rangeMax == 0)
      return true;
   else if (rangeMin < 0 && rangeMax > 0)
      return true;
   else
      return false;
   }

TR::Node *constrainIdiv(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   bool isUnsigned = node->getOpCode().isUnsigned();
   TR::VPConstraint *constraint = NULL;
   if (lhs && rhs)
      {
      if (lhs->asIntConst() && rhs->asIntConst())
         {
         int32_t lhsConst = lhs->asIntConst()->getInt();
         int32_t rhsConst = rhs->asIntConst()->getInt();
         if (lhsConst == TR::getMinSigned<TR::Int32>() && rhsConst == -1)
            constraint = TR::VPIntConst::create(vp, TR::getMinSigned<TR::Int32>());
         else if (rhsConst != 0)
            {
            if (isUnsigned)
               constraint = TR::VPIntConst::create(vp, ((uint32_t)lhsConst/(uint32_t)rhsConst));
            else
               constraint = TR::VPIntConst::create(vp, lhsConst/rhsConst);
            }
         }
      else
         {
         TR::VPIntConstraint *lhsInt = lhs->asIntConstraint();
         TR::VPIntConstraint *rhsInt = rhs->asIntConstraint();
         if (lhsInt && rhsInt)
            {
            int64_t lhs1, lhs2, rhs1, rhs2;
            int64_t range_min, range_max;
            if (isUnsigned)
               {
               lhs1 = lhs->getUnsignedLowInt(), lhs2 = lhs->getUnsignedHighInt();
               rhs1 = rhs->getUnsignedLowInt(), rhs2 = rhs->getUnsignedHighInt();
               range_min = 0; range_max = TR::getMaxSigned<TR::Int32>();     // we don't support unsigned range constraint yet. Only support upto TR::getMaxSigned<TR::Int32>() for unsigned
               }
            else
               {
               lhs1 = lhs->getLowInt(), lhs2 = lhs->getHighInt();
               rhs1 = rhs->getLowInt(), rhs2 = rhs->getHighInt();
               range_min = TR::getMinSigned<TR::Int32>(); range_max = TR::getMaxSigned<TR::Int32>();
               }
            int64_t lo, hi;

            if (constrainIntegerDivisionRange(lhs1, lhs2, rhs1, rhs2, range_min, range_max,
                  lo, hi, vp->computeDivRangeWhenDivisorCanBeZero(node)))
               constraint = TR::VPIntRange::create(vp, (int32_t)lo, (int32_t)hi);
            }
         }
      }

   if (constraint)
      {
      if (constraint->asIntConst())
         {
         // If constrainIntegerDivisionRange reduced the range to a constant, but the original
         // dividend contained 0 in its range, create a copy of the original div, to be anchored
         // under the original DIVCHK, so that the constant value can be optimized further,
         // but division by zero is still caught
         TR::Node *newDiv = NULL;
         if (doesRangeContainZero(rhs->getLowInt(), rhs->getHighInt()))
            newDiv = cloneDivForDivideByZeroCheck(vp, node);
         vp->replaceByConstant(node, constraint, lhsGlobal);
         if (newDiv != NULL)
            return newDiv;
         return node;
         }
      vp->addBlockOrGlobalConstraint(node, constraint, lhsGlobal);
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLdiv(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   bool isUnsigned = node->getOpCode().isUnsigned();
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::VPConstraint *lhs = vp->getConstraint(firstChild, lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(secondChild, rhsGlobal);
   TR::Node *returnNode = node;

   lhsGlobal &= rhsGlobal;
   if (lhs && lhs->asLongConst() &&
       rhs && rhs->asLongConst())
      {
      TR::VPConstraint *constraint = NULL;
      int64_t lhsConst = lhs->asLongConst()->getLong();
      int64_t rhsConst = rhs->asLongConst()->getLong();
      if (lhsConst == TR::getMinSigned<TR::Int64>() && rhsConst == -1)
         constraint = TR::VPLongConst::create(vp, TR::getMinSigned<TR::Int64>());
      else if (rhsConst != 0L)
         constraint = TR::VPLongConst::create(vp, TR::Compiler->arith.longDivideLong(lhsConst, rhsConst));
      if (constraint)
         {
         vp->replaceByConstant(node, constraint, lhsGlobal);
         }
      }
   else if (lhs && rhs)
      {
      TR::VPLongConstraint *lhsConstraint= lhs->asLongConstraint();
      TR::VPLongConstraint *rhsConstraint = rhs->asLongConstraint();

      // Try and reduce to an integer divide
      if (vp->lastTimeThrough() &&
          !isUnsigned &&
          lhsConstraint && (lhsConstraint->getLow() >= ((int64_t) TR::getMinSigned<TR::Int32>())) && (lhsConstraint->getHigh() <= ((int64_t) TR::getMaxSigned<TR::Int32>())) &&
          rhsConstraint && (rhsConstraint->getLow() >= ((int64_t) TR::getMinSigned<TR::Int32>())) && (rhsConstraint->getHigh() <= ((int64_t) TR::getMaxSigned<TR::Int32>())) &&
          ((lhsConstraint->getLow() > TR::getMinSigned<TR::Int32>()) || (rhsConstraint->getLow() > -1) || (rhsConstraint->getHigh() < -1)) &&
          performTransformation(vp->comp(), "%sChange node [" POINTER_PRINTF_FORMAT "] ldiv->i2l of idiv\n",OPT_DETAILS, node))
         {
         // Change node into i2l to be used by any commoned references
         TR::Node::recreate(node, TR::i2l);
         node->setNumChildren(1);

         TR::Node *lhsNode = TR::Node::create(TR::l2i, 1, firstChild);
         TR::Node *rhsNode = TR::Node::create(TR::l2i, 1, secondChild);
         TR::Node *newIdivNode = TR::Node::create(TR::idiv, 2, lhsNode, rhsNode);
         node->setAndIncChild(0, newIdivNode);

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         if (vp->_curTree->getNode()->getOpCodeValue() == TR::DIVCHK &&
             vp->_curTree->getNode()->getFirstChild() == node)
            {
            // messier scenario
            // in this case, my parent is relying on me to be a divide
            //   rather than relying on me to be a long value
            //   so return the idiv node and anchor the i2l in a
            //   treetop following this divchk

            // create a treetop containing the i2l following the current tree
            TR::Node *newTreeNode = TR::Node::create(TR::treetop, 1, node);
            TR::TreeTop *newTree = TR::TreeTop::create(vp->comp(), newTreeNode);
            TR::TreeTop *prevTree = vp->_curTree;
            newTree->join(prevTree->getNextTreeTop());
            prevTree->join(newTree);

            // make sure parent has idiv as child
            returnNode = newIdivNode;
            newIdivNode->incReferenceCount();
            node->decReferenceCount();
            }
         // else parent requires the long value so returnNode should be the i2l

         // Create a constraint for the newly-created idiv node
         int64_t lhs1 = lhsConstraint->getLow(), lhs2 = lhsConstraint->getHigh();
         int64_t rhs1 = rhsConstraint->getLow(), rhs2 = rhsConstraint->getHigh();
         int64_t lo = 0, hi = 0;
         TR::VPConstraint *idivConstraint = NULL;
         if (constrainIntegerDivisionRange(lhs1, lhs2, rhs1, rhs2, TR::getMinSigned<TR::Int32>(), TR::getMaxSigned<TR::Int32>(), lo, hi, vp->computeDivRangeWhenDivisorCanBeZero(newIdivNode)))
            idivConstraint = TR::VPIntRange::create(vp, (int32_t)lo, (int32_t)hi);
         if (idivConstraint)
            {
            // If constrainIntegerDivisionRange reduced the range to a constant, but the original
            // divisor contained 0 in its range, create a copy of the original div, to be anchored
            // under the original DIVCHK, so that the constant value can be optimized further,
            // but division by zero is still caught
            if (idivConstraint->asIntConst())
               {
               TR::Node *newDiv = NULL;
               if (doesRangeContainZero(rhs1, rhs2))
                  newDiv = cloneDivForDivideByZeroCheck(vp, newIdivNode);
               vp->replaceByConstant(newIdivNode, idivConstraint, lhsGlobal);
               if (newDiv != NULL)
                  return newDiv;
               else
                  return returnNode;
               }

            vp->addBlockOrGlobalConstraint(newIdivNode, idivConstraint, lhsGlobal);
            vp->addBlockOrGlobalConstraint(node, TR::VPLongRange::create(vp, lo, hi), lhsGlobal);
            }
         else
            {
            // Even with no constraint on idiv, the result of i2l is in int range.
            TR::VPConstraint *i2lConstraint = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int32>(), TR::getMaxSigned<TR::Int32>());
            vp->addBlockOrGlobalConstraint(node, i2lConstraint, lhsGlobal);
            }
         }
      else if (!isUnsigned)
         {
         if (lhsConstraint && rhsConstraint)
            {
            int64_t lhs1 = lhsConstraint->getLowLong(), lhs2 = lhsConstraint->getHighLong();
            int64_t rhs1 = rhsConstraint->getLowLong(), rhs2 = rhsConstraint->getHighLong();
            int64_t lo, hi;
            if (constrainIntegerDivisionRange(lhs1, lhs2, rhs1, rhs2, TR::getMinSigned<TR::Int64>(), TR::getMaxSigned<TR::Int64>(), lo, hi, vp->computeDivRangeWhenDivisorCanBeZero(node)))
               {
               TR::VPConstraint *constraint = TR::VPLongRange::create(vp, lo, hi);
               if (constraint)
                  {
                  // If constrainIntegerDivisionRange reduced the range to a constant, but the original
                  // divisor contained 0 in its range, create a copy of the original div, to be anchored
                  // under the original DIVCHK, so that the constant value can be optimized further,
                  // but division by zero is still caught
                  if (constraint->asLongConst())
                     {
                     TR::Node *newDiv = NULL;
                     if (doesRangeContainZero(rhs1, rhs2))
                        newDiv = cloneDivForDivideByZeroCheck(vp, node);
                     vp->replaceByConstant(node, constraint, lhsGlobal);
                     if (NULL != newDiv)
                        return newDiv;
                     else
                        return node;
                     }

                  vp->addBlockOrGlobalConstraint(node, constraint,lhsGlobal);
                  }
               }
            }
         }
      else
         {
         uint64_t lhsLow = lhsConstraint->getUnsignedLowLong();
         uint64_t rhsLow = rhsConstraint->getUnsignedLowLong();
         uint64_t lhsHigh = lhsConstraint->getUnsignedHighLong();
         uint64_t rhsHigh = rhsConstraint->getUnsignedHighLong();

         if (rhsLow >= 1 &&
             rhsLow <= rhsHigh &&
             lhsLow <= lhsHigh)
            {
            // Non-negative dividend, positive divisor
            vp->addBlockOrGlobalConstraint(node, TR::VPLongRange::create(vp, lhsLow / rhsHigh, lhsHigh / rhsLow) ,lhsGlobal);
            }
         }
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return returnNode;
   }

TR::Node *removeRedundantREM(OMR::ValuePropagation *vp, TR::Node *node, TR::VPConstraint *nodeConstraint, TR::VPConstraint *firstChildConstraint, TR::VPConstraint *secondChildConstraint)
   {
   if (!(node->getOpCode().isRem() && node->getType().isIntegral()))
      return NULL;

   int64_t nodePrecision = nodeConstraint->getPrecision();
   int64_t fcPrecision = firstChildConstraint->getPrecision();
   int64_t scPrecision = secondChildConstraint->getPrecision();

   bool powerOfTen = false;
   int64_t constValue;

   if (secondChildConstraint->asIntConst() && isPositivePowerOfTen(secondChildConstraint->getLowInt()))
      {
      powerOfTen = true;
      constValue = secondChildConstraint->getLowInt();
      }
   else if (secondChildConstraint->asLongConst() && isPositivePowerOfTen(secondChildConstraint->getLowLong()))
      {
      powerOfTen = true;
      constValue = secondChildConstraint->getLowLong();
      }

   if (powerOfTen)
      {
      bool isUnsigned = node->getOpCode().isUnsigned();
      // the max value for a remainder result is the divisor value - 1 so when the divisor is a power of ten then -1 this value is
      // one decimal precision value less (e.g. 100 - 1 = 99)
      int32_t refinedPrecision = trailingZeroes((uint64_t)constValue);
      if (!isUnsigned &&
          fcPrecision <= refinedPrecision &&
          performTransformation(vp->comp(),"%sRemove %s [0x%p] as child %s [0x%p] prec %lld <= divisor max prec %d (value %lld)\n",
                                  OPT_DETAILS,node->getOpCode().getName(),node,
                                  node->getFirstChild()->getOpCode().getName(),node->getFirstChild(),
                                  fcPrecision,refinedPrecision,constValue))
         {
         // in this case based on the child prec and the divisor constant value that the irem is not needed (999 < 1000)
         // so can remove the irem completely
         // irem
         //    child prec=3 (i.e. max value is 999)
         //    iconst 1000
         //
         // this doesn't work for unsigned (iurem) as these ops treat the operands as unsigned so, for example, a prec=1 dividend that is negative
         // when interpreted as unsigned will actually have 10 digits of precision (-1 would be 0xFFffFFff = 4,294,967,295)
         //
         return vp->replaceNode(node, node->getFirstChild(), vp->_curTree);
         }
      }

   return NULL;
   }

TR::Node *constrainIrem(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   TR::VPConstraint *constraint = NULL;
   lhsGlobal &= rhsGlobal;
   bool isUnsigned = node->getOpCode().isUnsigned();
   if (lhs && lhs->asIntConst() &&
       rhs && rhs->asIntConst())
      {
      int32_t lhsConst = lhs->asIntConst()->getInt();
      int32_t rhsConst = rhs->asIntConst()->getInt();
      if (lhsConst == TR::getMinSigned<TR::Int32>() && rhsConst == -1)
         constraint = TR::VPIntConst::create(vp, 0);
      else if (rhsConst != 0)
         {
         if (isUnsigned)
            constraint = TR::VPIntConst::create(vp, ((uint32_t)lhsConst)%((uint32_t)rhsConst));
         else
            constraint = TR::VPIntConst::create(vp, lhsConst%rhsConst);
         }
      if (constraint)
         vp->replaceByConstant(node, constraint, lhsGlobal);
      }
   else if (rhs && rhs->asIntConst() && lhs && lhs->asIntConstraint())
      {
      int32_t lhsLow = lhs->asIntConstraint()->getLowInt();
      int32_t lhsHigh = lhs->asIntConstraint()->getHighInt();
      int32_t rhsConst = rhs->asIntConst()->getInt();
      if (!isUnsigned && rhsConst < 0)
         rhsConst *= -1;
      rhsConst--; // If the original const was 10, the range is from 0 to 9

      if (lhsLow > 0)
         constraint = TR::VPIntRange::create(vp, 0, rhsConst);
      else if (!isUnsigned && lhsHigh < 0)
         constraint = TR::VPIntRange::create(vp, -rhsConst, 0);
      else if (!isUnsigned)
         constraint = TR::VPIntRange::create(vp, -rhsConst, rhsConst);

      if (constraint)
         {
         vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
         }
      }

   if (constraint && lhs && lhs->asIntConstraint() && rhs && rhs->asIntConstraint())
      {
      TR::Node *newNode = removeRedundantREM(vp, node, constraint, lhs, rhs);
      if (NULL != newNode)
         node = newNode;
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLrem(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      {
      return node;
      }
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   TR::VPConstraint *constraint = NULL;
   lhsGlobal &= rhsGlobal;
   if (lhs && lhs->asLongConst() &&
       rhs && rhs->asLongConst())
      {
      int64_t lhsConst = lhs->asLongConst()->getLong();
      int64_t rhsConst = rhs->asLongConst()->getLong();
      if (lhsConst == TR::getMinSigned<TR::Int64>() && rhsConst == -1)
         constraint = TR::VPLongConst::create(vp, 0);
      else if (rhsConst != 0)
         constraint = TR::VPLongConst::create(vp, TR::Compiler->arith.longRemainderLong(lhsConst,rhsConst));
      if (constraint)
         vp->replaceByConstant(node, constraint, lhsGlobal);
      }
   else if (rhs && rhs->asLongConst() && lhs && lhs->asLongConstraint())
      {
      int64_t lhsLow = lhs->asLongConstraint()->getLowLong();
      int64_t lhsHigh = lhs->asLongConstraint()->getHighLong();
      int64_t rhsConst = rhs->asLongConst()->getLong();
      if (rhsConst < 0)
         rhsConst *= -1;
      rhsConst--; // If the original const was 10, the range is from 0 to 9

      if (lhsLow > 0)
         constraint = TR::VPLongRange::create(vp, 0, rhsConst);
      else if (lhsHigh < 0)
         constraint = TR::VPLongRange::create(vp, -rhsConst, 0);
      else
         constraint = TR::VPLongRange::create(vp, -rhsConst, rhsConst);

      if (constraint)
         {
         bool didReduction = reduceLongOpToIntegerOp(vp, node, constraint);
         vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);

         if (didReduction)
            return node;
         }
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   if (constraint && lhs && lhs->asLongConstraint() && rhs && rhs->asLongConstraint())
      {
      TR::Node *newNode = removeRedundantREM(vp, node, constraint, lhs, rhs);
      if (NULL != newNode)
         node = newNode;
      }

   checkForNonNegativeAndOverflowProperties(vp, node);

   return node;
   }

TR::Node *constrainIneg(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool isGlobal;
   TR::VPConstraint *child = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (child)
      {
      if (child->asIntConst())
         {
         TR::VPConstraint *constraint = TR::VPIntConst::create(vp, -child->asIntConst()->getInt());
         vp->replaceByConstant(node, constraint, isGlobal);
         }
      else
         {
         int32_t high = child->getHighInt();
         int32_t low = child->getLowInt();

         TR::VPConstraint *lowConstraint = NULL;
         TR::VPConstraint *highConstraint = NULL;
         TR::VPConstraint *constraint = NULL;
         if (low == TR::getMinSigned<TR::Int32>())
            {
            lowConstraint = TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), TR::getMinSigned<TR::Int32>(), TR_yes);
            low++;
            }

         if (high == TR::getMinSigned<TR::Int32>())
            {
            highConstraint = TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), TR::getMinSigned<TR::Int32>(), TR_yes);
            high++;
            }

          if (!highConstraint)
             {
             int32_t newHigh = -low;
             int32_t newLow = -high;
             constraint = TR::VPIntRange::create(vp, newLow, newHigh, TR_yes);

             if (lowConstraint)
                constraint = TR::VPMergedConstraints::create(vp, lowConstraint, constraint);
             }
          else
             constraint = highConstraint;

          if (constraint)
             {
             vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);
             }
          }
       }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLneg(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool isGlobal;
   TR::VPConstraint *child = vp->getConstraint(node->getFirstChild(), isGlobal);

   if (child)
      {
      if (child->asLongConst())
         {
         TR::VPConstraint *constraint = TR::VPLongConst::create(vp, -child->asLongConst()->getLong());
         vp->replaceByConstant(node, constraint, isGlobal);
         }
      else
         {
         int64_t high = child->getHighLong();
         int64_t low = child->getLowLong();

         TR::VPConstraint *lowConstraint = NULL;
         TR::VPConstraint *highConstraint = NULL;
         TR::VPConstraint *constraint = NULL;
         if (low == TR::getMinSigned<TR::Int64>())
            {
            lowConstraint = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), TR::getMinSigned<TR::Int64>(), TR_yes);
            low++;
            }

         if (high == TR::getMinSigned<TR::Int64>())
            {
            highConstraint = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), TR::getMinSigned<TR::Int64>(), TR_yes);
            high++;
            }

         if (!highConstraint)
            {
            int64_t newHigh = -low;
            int64_t newLow = -high;
            constraint = TR::VPLongRange::create(vp, newLow, newHigh, TR_yes);

            if (lowConstraint)
               constraint = TR::VPMergedConstraints::create(vp, lowConstraint, constraint);
            }
         else
            constraint = highConstraint;

         if (constraint)
            {
            bool didReduction = reduceLongOpToIntegerOp(vp, node, constraint);
            vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);

            if (didReduction)
            return node;
            }
          }
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainIabs(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool isGlobal;
   TR::VPConstraint *child = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (child)
      {
      if (child->asIntConst())
         {
         if (child->asIntConst()->getInt() < 0)
          {
          TR::VPConstraint *constraint = TR::VPIntConst::create(vp, -child->asIntConst()->getInt());
          vp->replaceByConstant(node, constraint, isGlobal);
          }
         else
          {
          TR::VPConstraint *constraint = TR::VPIntConst::create(vp, child->asIntConst()->getInt());
          vp->replaceByConstant(node, constraint, isGlobal);
          }
         }
      else
         {
         int32_t high = child->getHighInt();
         int32_t low = child->getLowInt();

         if (low < 0 && high <= 0)
          {
          int32_t temp = low * -1;
          low = high * -1;
          high = temp;
          }
         else if (low < 0 && high > 0)
          {
          high = std::max(low * -1, high);
          low = 0;
          }
         else if (performTransformation(vp->comp(),"%sRemoving %s [0x%p] as child %s [0x%p] is known to be positive\n",
               OPT_DETAILS,node->getOpCode().getName(),node,
               node->getFirstChild()->getOpCode().getName(),node->getFirstChild()))
            {
            // The range of values for the child is entirely positive, so remove the iabs
            return vp->replaceNode(node, node->getFirstChild(), vp->_curTree);
            }

         if (low == high)
          {
           TR::VPConstraint *constraint = TR::VPIntConst::create(vp, low);
           vp->replaceByConstant(node, constraint, isGlobal);
          }
         else
          {
           TR::VPConstraint *constraint = TR::VPIntRange::create(vp, low, high);
             vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);
          }
         }
       }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLabs(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool isGlobal;
   TR::VPConstraint *child = vp->getConstraint(node->getFirstChild(), isGlobal);

   if (child)
      {
      if (child->asLongConst())
         {
         if (child->asLongConst()->getLong() < 0)
          {
          TR::VPConstraint *constraint = TR::VPLongConst::create(vp, -child->asLongConst()->getLong());
          vp->replaceByConstant(node, constraint, isGlobal);
          }
      else
          {
          TR::VPConstraint *constraint = TR::VPLongConst::create(vp, child->asLongConst()->getLong());
          vp->replaceByConstant(node, constraint, isGlobal);
          }
         }
      else
         {
         int64_t high = child->getHighLong();
         int64_t low = child->getLowLong();

         if (low < 0 && high <= 0)
          {
          int64_t temp = low * -1;
          low = high * -1;
          high = temp;
          }
         else if (low < 0 && high > 0)
          {
          high = std::max(low * -1, high);
          low = 0;
          }
         else if (performTransformation(vp->comp(),"%sRemoving %s [0x%p] as child %s [0x%p] is known to be positive\n",
               OPT_DETAILS,node->getOpCode().getName(),node,
               node->getFirstChild()->getOpCode().getName(),node->getFirstChild()))
            {
            // The range of values for the child is entirely positive, so remove the labs
            return vp->replaceNode(node, node->getFirstChild(), vp->_curTree);
            }

         if (low == high)
          {
           TR::VPConstraint *constraint = TR::VPLongConst::create(vp, low);
           vp->replaceByConstant(node, constraint, isGlobal);
          }
         else
          {
           TR::VPConstraint *constraint = TR::VPLongRange::create(vp, low, high);
           bool didReduction = reduceLongOpToIntegerOp(vp, node, constraint);
             vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);

             if (didReduction)
            return node;
          }
          }
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }


TR::Node *constrainIshl(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   //bool isUnsigned = node->getType().isUnsignedInt();
   if (lhs && lhs->asIntConst() &&
       rhs && rhs->asIntConst())
      {
      int32_t lhsConst = lhs->asIntConst()->getInt();
      int32_t rhsConst = rhs->asIntConst()->getInt();
      TR::VPConstraint *constraint = TR::VPIntConst::create(vp, lhsConst << (rhsConst & 0x01F)/*, isUnsigned*/);
      vp->replaceByConstant(node, constraint, lhsGlobal);
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLshl(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   if (lhs && lhs->asLongConst() &&
       rhs && rhs->asLongConst())
      {
      int64_t lhsConst = lhs->asLongConst()->getLong();
      int64_t rhsConst = rhs->asLongConst()->getLong();
      TR::VPConstraint *constraint = TR::VPLongConst::create(vp, lhsConst << (rhsConst & 0x03F));
      vp->replaceByConstant(node, constraint, lhsGlobal);
      }

   if (lhs && lhs->asLongConst() &&
       lhs->asLongConst()->getLong() == 1)
      {
      TR::VPConstraint *constraint = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), TR::getMaxSigned<TR::Int64>(), true);
      vp->addBlockConstraint(node, constraint);
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

static TR::Node *distributeShift(OMR::ValuePropagation *vp, TR::Node *node, int32_t shiftAmount,
                                TR::VPConstraint *&constraint)
   {
   //FIXME: temporarily disabled
   //
   return NULL;
   // check if the distributive property can be applied
   // (a + b) >> amount ==> a >> amount or b >> amount if
   // it can be proven that the a (or b) >= 2^amt
   //
   // ishr                        ishr
   //    iadd             ==>        iload a
   //       iload a                  iconst amt
   //       iload b
   //    iconst amt
   //
   TR::Node *replaceNode = NULL;

   ///traceMsg(vp->comp(), "attempting to distributeShift on node [%p]\n", node);

   TR::ILOpCode &nodeOp = node->getFirstChild()->getOpCode();
   if ((nodeOp.isAdd() || nodeOp.isSub()) && node->getFirstChild()->cannotOverflow())
      {
      TR::Node *lhsGrandChild = node->getFirstChild()->getFirstChild();
      TR::Node *rhsGrandChild = node->getFirstChild()->getSecondChild();
      bool isGlobal;
      TR::VPConstraint *lhsGConstraint = vp->getConstraint(lhsGrandChild, isGlobal);
      TR::VPConstraint *rhsGConstraint = vp->getConstraint(rhsGrandChild, isGlobal);
      TR::VPConstraint *c = NULL;
      // check for commutativity
      //
      if (lhsGConstraint && !nodeOp.isSub())
         {
         int32_t low = lhsGConstraint->getLowInt();
         int32_t high = lhsGConstraint->getHighInt();
         if ((low >> shiftAmount == 0) && (high >> shiftAmount == 0))
            {
            replaceNode = rhsGrandChild;
            c = rhsGConstraint;
            }
         }
      else if (rhsGConstraint)
         {
         int32_t low = rhsGConstraint->getLowInt();
         int32_t high = rhsGConstraint->getHighInt();
         if ((low >> shiftAmount == 0) && (high >> shiftAmount == 0))
            {
            replaceNode = lhsGrandChild;
            c = lhsGConstraint;
            }
         }

      if (vp->trace())
         traceMsg(vp->comp(), "found replaceNode is [%p]\n", replaceNode);

      if (replaceNode)
         {
         int64_t pow2Val = (1 << shiftAmount);
         TR::VPConstraint *lhsORrhs = NULL;
         if (replaceNode == rhsGrandChild) //lhs is the redundant expr
            lhsORrhs = rhsGConstraint;
         else
            lhsORrhs = lhsGConstraint;

         if (lhsORrhs)
            {
            int32_t low = lhsORrhs->getLowInt();
            int32_t high = lhsORrhs->getHighInt();
            if ((low <= 0) && (((int64_t)(high+1) & (pow2Val-1))) == 0)
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "found opportunity to distribute shift in node [%p] replaceNode [%p]\n", node, replaceNode);
               }
            else
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "failed additive expr constraint is not within range, node [%p] replaceNode [%p]\n", node, replaceNode);
               replaceNode = NULL;
               }
            }
         else
            {
            if (vp->trace())
               traceMsg(vp->comp(), "failed no constraint found, node [%p] replaceNode [%p]\n", node, replaceNode);
            replaceNode = NULL;
            }
         }

       if (replaceNode &&
            performTransformation(vp->comp(), "%sDistributing shift across add/sub [%p]\n", OPT_DETAILS, node->getFirstChild()))
         {
         if (replaceNode->getReferenceCount() > 1)
            replaceNode = replaceNode->duplicateTree();

         node->getFirstChild()->recursivelyDecReferenceCount();
         node->setAndIncChild(0, replaceNode);
         if (c)
            {
            if (node->getOpCodeValue() == TR::iushr)
               {
               if (c->getLowInt() >= 0)
                  constraint = TR::VPIntRange::create(vp, ((uint32_t)c->getLowInt()) >> shiftAmount, ((uint32_t)c->getHighInt()) >> shiftAmount);
               else if (c->getHighInt() < 0)
                  constraint = TR::VPIntRange::create(vp, ((uint32_t)c->getHighInt()) >> shiftAmount, ((uint32_t)c->getLowInt()) >> shiftAmount);
               // this path is probably never taken for unsigned
               else
                  {
                  if (shiftAmount > 0)
                     constraint = TR::VPIntRange::create(vp, 0, (uint32_t)0xffffffff >> shiftAmount);
                  else
                     constraint = TR::VPIntRange::create(vp, 0, TR::getMaxSigned<TR::Int32>());
                  }
               }
            else
               constraint = TR::VPIntRange::create(vp, c->getLowInt() >> shiftAmount, c->getHighInt() >> shiftAmount);
            }
         }
      }
   return replaceNode;
   }

TR::Node *constrainIshr(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool rhsGlobal;
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   if (rhs && rhs->asIntConst())
      {
      int32_t rhsConst = rhs->asIntConst()->getInt();
      int32_t shiftAmount = rhsConst & 0x01F;

      bool lhsGlobal;
      TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
      lhsGlobal &= rhsGlobal;

      int32_t low, high;
      if (lhs)
         {
         low = lhs->getLowInt();
         high = lhs->getHighInt();
         }
      else
         {
         low = TR::getMinSigned<TR::Int32>();
         high = TR::getMaxSigned<TR::Int32>();
         }

      TR::VPConstraint *constraint = TR::VPIntRange::create(vp, low >> shiftAmount, high >> shiftAmount);


      if (constraint && !constraint->asIntConst())
         distributeShift(vp, node, shiftAmount, constraint);

      if (constraint)
         {
         if (constraint->asIntConst())
            {
            vp->replaceByConstant(node, constraint, lhsGlobal);
            return node;
            }
         vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
         }
      }
   checkForNonNegativeAndOverflowProperties(vp, node);

   TR::Node * firstChild = node->getFirstChild();
   if(firstChild->isNonNegative() /*&& firstChild->getType().isSignedInt()*/ && vp->lastTimeThrough() &&
      performTransformation(vp->comp(), "%sChange node [" POINTER_PRINTF_FORMAT "] ishr->iushr\n",OPT_DETAILS, node))
      {
      TR::Node::recreate(node, TR::iushr);
      //TR::Node *lhs = node->getFirstChild();
      //TR::Node *rhs = node->getSecondChild();
      //TR::Node::recreate(node, TR::iu2i);
      //node->setNumChildren(1);
      //TR::Node *newIushrNode = TR::Node::create(vp->comp(), TR::iushr, 2, lhs, rhs);
      //node->setAndIncChild(0, newIushrNode);
      //lhs->decReferenceCount();
      //rhs->decReferenceCount();
      }
   // this handler is not called for unsigned shifts
   //#ifdef DEBUG
   //else if(!firstChild->getType().isSignedInt()) dumpOptDetails(vp->comp(), "FIXME: Need to support ishr opt for Unsigned datatypes!\n");
   //FIXME
   //#endif
   return node;
   }

TR::Node *constrainLshr(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;

   bool longShr = node->getOpCode().isLong();

   constrainChildren(vp, node);

   bool rhsGlobal;
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   if (rhs && rhs->asIntConst())
      {
      int32_t rhsConst = rhs->asIntConst()->getInt();
      int32_t shiftAmount = rhsConst & 0x03F;

      bool lhsGlobal;
      TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
      lhsGlobal &= rhsGlobal;

      int64_t low, high;
      if (lhs)
         {
         low = lhs->getLowLong();
         high = lhs->getHighLong();
         }
      else
         {
         low = TR::getMinSigned<TR::Int64>();
         high = TR::getMaxSigned<TR::Int64>();
         }

      TR::VPConstraint *constraint = TR::VPLongRange::create(vp, low >> shiftAmount, high >> shiftAmount);
      if (constraint)
         {
         if (constraint->asLongConst())
            {
            vp->replaceByConstant(node, constraint, lhsGlobal);
            return node;
            }

         bool didReduction = false;
         if (longShr)
            {
            TR::Node *secondChild = node->getSecondChild();
            didReduction = reduceLongOpToIntegerOp(vp, node, constraint);
            //If the shift amount is between 32 and 63, inclusive, then if we convert the node to an integer shift we must reduce this shift value. Integer shifts are
            //masked by 0x1f in java, and if the first child is integer constrained, a shift by an amount in this range is effectively just a shift by 31.
            if (didReduction && shiftAmount >= 32)
               {
               secondChild->decReferenceCount();
               node->getFirstChild()->setAndIncChild(1, TR::Node::create(node, TR::iconst, 0, 31));
               }
            }

         vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);

         // If we reduced long to integer, node is now an i2l; not safe to do any further addition-related things with node
         if (didReduction)
          return node;
         }
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainIushr(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   // check the type of the firstchild
   // since ishr could be converted to iushr
   // for signed types
   //bool isUnsigned = node->getFirstChild()->getType().isUnsignedInt();
   // iushr could feed into a store for example, so check that
   //
   TR::Node *parent = vp->getCurrentParent();
   //if (parent && parent->getType().isUnsignedInt())
   //   isUnsigned = true;

   bool rhsGlobal;
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   if (rhs && rhs->asIntConst())
      {
      int32_t rhsConst = rhs->asIntConst()->getInt();
      int32_t shiftAmount = rhsConst & 0x01F;
      if (shiftAmount)
         {
         node->setIsNonNegative(true);
         }

      bool lhsGlobal;
      TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
      lhsGlobal &= rhsGlobal;

      int32_t low, high;
      if (lhs)
         {
         low = lhs->getLowInt();
         high = lhs->getHighInt();
         }
      else
         {
         //if (isUnsigned)
         //   {
         //   low = TR::getMinUnsigned<TR::Int32>();
         //   high = TR::getMaxUnsigned<TR::Int32>();
         //   }
         //else
         //   {
            low = TR::getMinSigned<TR::Int32>();
            high = TR::getMaxSigned<TR::Int32>();
         //   }
         }

      TR::VPConstraint *constraint = NULL;
      if (low == high)
         constraint = TR::VPIntConst::create(vp, ((uint32_t)low) >> shiftAmount/*, isUnsigned*/);
      else if (low >= 0)
         constraint = TR::VPIntRange::create(vp, ((uint32_t)low) >> shiftAmount, ((uint32_t)high) >> shiftAmount/*, isUnsigned*/);
      else if (high < 0 /*&& !isUnsigned*/)
         constraint = TR::VPIntRange::create(vp, ((uint32_t)high) >> shiftAmount, ((uint32_t)low) >> shiftAmount);
      // this path is probably never taken for unsigned
      else
         {
         if (shiftAmount > 0)
            constraint = TR::VPIntRange::create(vp, 0, (uint32_t)0xffffffff >> shiftAmount);
                  // if shiftAmount ==0 range should remain the same
         else
            {
            constraint = TR::VPIntRange::create(vp, low, high);
            }
         }

      if (constraint && !constraint->asIntConst())
         distributeShift(vp, node, shiftAmount, constraint);

      if (constraint)
         {
         if (constraint->asIntConst())
            {
            vp->replaceByConstant(node, constraint, lhsGlobal);
            return node;
            }

         vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
         }
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLushr(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool rhsGlobal;
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   if (rhs && rhs->asIntConst())
      {
      int32_t rhsConst = rhs->asIntConst()->getInt();
      int32_t shiftAmount = rhsConst & 0x03F;
      if (shiftAmount)
         {
         node->setIsNonNegative(true);
         }

      bool lhsGlobal;
      TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
      lhsGlobal &= rhsGlobal;

      int64_t low, high;
      if (lhs)
         {
         low = lhs->getLowLong();
         high = lhs->getHighLong();
         }
      else
         {
         low = TR::getMinSigned<TR::Int64>();
         high = TR::getMaxSigned<TR::Int64>();
         }

      TR::VPConstraint *constraint = NULL;
      if (low == high)
         constraint = TR::VPLongConst::create(vp, ((uint64_t)low) >> shiftAmount);
      else if (low >= 0)
         constraint = TR::VPLongRange::create(vp, ((uint64_t)low) >> shiftAmount, ((uint64_t)high) >> shiftAmount);
      else if (high < 0)
         constraint = TR::VPLongRange::create(vp, ((uint64_t)high) >> shiftAmount, ((uint64_t)low) >> shiftAmount);
      else
         {
         if (shiftAmount > 0)
            constraint = TR::VPLongRange::create(vp, 0, ((uint64_t)-1L) >> shiftAmount);
         else
            constraint = TR::VPLongRange::create(vp, low, high);
         }

      if (constraint)
         {
         if (constraint->asLongConst())
            {
            vp->replaceByConstant(node, constraint, lhsGlobal);
            return node;
            }

         vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
         }
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainIand(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   bool signBitsMaskedOff=0;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   TR::VPConstraint *constraint = NULL;
   //bool isUnsigned = node->getType().isUnsignedInt();

   bool canBeRemoved = false;

   if (rhs && rhs->asIntConst())
      {
      int32_t mask = rhs->asIntConst()->getInt();
      signBitsMaskedOff = leadingZeroes(mask);

      // take opportunity to remove if unneeded
      if(255 == mask && lhs) // loading from byte
         {
         if(lhs->asIntRange())
            {
            TR::VPIntRange *range = lhs->asIntRange();
            int32_t low = range->getLowInt();
            int32_t high = range->getHighInt();
            //bool withinRange = isUnsigned ? ((uint32_t)high <= 255) : (high <= 255);
            if (low >= 0 && high <= 255 &&
                performTransformation(vp->comp(), "%sRemoving node [%p] %s\n", OPT_DETAILS, node, node->getOpCode().getName()))
               {
               if(node->getReferenceCount() > 1)
                  node->getFirstChild()->incReferenceCount();

               node->decReferenceCount();
               // fix the const child's reference count
               if (node->getReferenceCount() == 0)
                  node->getSecondChild()->decReferenceCount();

               //printf("remove andi 255 opportunity found in %s\n", vp->comp()->signature());
               // invalidate its use def info
               int32_t useIndex = node->getUseDefIndex();
               TR_UseDefInfo *info = vp->optimizer()->getUseDefInfo();
               if (info && (info->isDefIndex(useIndex) || info->isUseIndex(useIndex)))
                  {
                  if (info->getNode(useIndex) == node)
                     info->clearNode(useIndex);
                  }

               node->setUseDefIndex(0);

               return node->getFirstChild(); // assume const is right child
               }
            }
         }

     if ((mask & 0x80000000) == 0)
         {
         node->setIsNonNegative(true);
         }
      if (mask == 0)
         {
         constraint = TR::VPIntConst::create(vp, 0/*, isUnsigned*/);
         }
      else if (lhs && lhs->asIntConst())
         {
         //if (isUnsigned)
         //   constraint = TR::VPIntConst::create(vp, ((uint32_t)lhs->asIntConst()->getInt() & (uint32_t)mask), isUnsigned);
         //else
            constraint = TR::VPIntConst::create(vp, lhs->asIntConst()->getInt() & mask);
         }
      else
         {
         // Below code will fold away isArray test laid down by
         // loop versioner to check if the type of the object we are getting
         // arraylength from is an array type. It should also work in
         // general for isArray tests in a user program
         //
         if(rhs && rhs->asIntConst())
            {
            TR::Node *firstChild = node->getFirstChild();
            TR::Node *secondChild = node->getSecondChild();

            if ((firstChild->getOpCodeValue() == TR::iloadi) &&
                (firstChild->getSymbolReference() == vp->comp()->getSymRefTab()->findClassIsArraySymbolRef()) &&
                (rhs->getLowInt() == TR::Compiler->cls.flagValueForArrayCheck(vp->comp())))
               {
               TR::Node *firstGrandChild = firstChild->getFirstChild();
               if ((firstGrandChild->getOpCodeValue() == TR::aloadi) &&
                   (firstGrandChild->getSymbolReference() == vp->comp()->getSymRefTab()->findClassRomPtrSymbolRef()))
                  {
                  TR::Node *vftLoad = firstGrandChild->getFirstChild();
                  if ((vftLoad->getOpCodeValue() == TR::aloadi) &&
                      (vftLoad->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef()))
                     {
                     TR::Node *objectLoad = vftLoad->getFirstChild();
                     bool objectLoadGlobal;
                     TR::VPConstraint *objConstraint = vp->getConstraint(objectLoad, objectLoadGlobal);
                     if (objConstraint && objConstraint->getClassType() && (objConstraint->getClassType()->isArray() != TR_maybe))
                        {
                        //dumpOptDetails(vp->comp(), "Folding isArray test in %p\n", objectLoad);
                        //printf("Folding isArray test in %s\n", vp->comp()->signature()); fflush(stdout);
                        if (objConstraint->getClassType()->isArray() == TR_yes)
                           constraint = TR::VPIntConst::create(vp, rhs->asIntConst()->getLowInt()/*, isUnsigned*/);
                        else
                           constraint = TR::VPIntConst::create(vp, 0/*, isUnsigned*/);
                        }
                     else
                        {
                        //if (vp->_isGlobalPropagation)
                        //   {
                        //   dumpOptDetails(vp->comp(), "NOT Folding isArray test in %p\n", objectLoad);
                        //   printf("NOT Folding isArray test in %s\n", vp->comp()->signature()); fflush(stdout);
                        //   }
                        }
                     }
                  }
               }
            }

         // look for mod by power of 2 case
         if (!constraint)
            {
            if (mask != 0xffffffff &&
                isNonNegativePowerOf2(mask + 1))
               {
               int32_t low = 0;
               int32_t high = mask;
//                if (isUnsigned)
//                   {
//                   if (lhs && (uint32_t)lhs->getLowInt() >= 0 &&
//                               (uint32_t)lhs->getHighInt() <= mask)
//                      {
//                      if ((uint32_t)lhs->getLowInt() > 0)
//                        low = lhs->getLowInt();
//                      if ((uint32_t)lhs->getHighInt() >= 0)
//                         high = lhs->getHighInt();
//                      }
//                   }
//                else
                  if (lhs && lhs->getLowInt() >= 0 && lhs->getHighInt() <= mask)
                     {
                     canBeRemoved = true;
                     if (vp->trace())
                        traceMsg(vp->comp(), "Removing redundant iand [%p] due to range\n", node);

                     if (lhs->getLowInt() > 0)
                        low = lhs->getLowInt();
                     if (lhs->getHighInt() >= 0)
                        high = lhs->getHighInt();
                     }
               constraint = TR::VPIntRange::create(vp, low, high/*, isUnsigned*/);
               }
            else if ((mask & 0x80000000) == 0)
               {
               constraint = TR::VPIntRange::create(vp, 0, mask/*, isUnsigned*/);
               }
            else
               {
               //if (isUnsigned)
               //   constraint = TR::VPIntRange::create(vp, TR::getMinUnsigned<TR::Int32>(), mask, isUnsigned);
               //else
                  constraint = TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), mask & TR::getMaxSigned<TR::Int32>());
               }
            }
         }
      }
   else
      {
      if (lhs && lhs->getLowInt() > 0)
         constraint = TR::VPIntRange::create(vp, 0, lhs->getHighInt()/*, isUnsigned*/);
      }

   //Evaluate to zero optimization: lhs will atleast contain the same number of trailng zeros as lrhs.
   //When lhs & rhs, and rhs < 0b1000..(number of trailing zeros in lhs), then the whole iand node evaluates to a constant zero.
   //The Opt doesn't kick in when rhs is negative, because of MSB being 1.

   bool lrhsGlobal;
   TR::VPConstraint *lrhs = NULL;
   if(node->getFirstChild()->getNumChildren() > 1)
      lrhs = vp->getConstraint(node->getFirstChild()->getSecondChild(), lrhsGlobal);
   if(rhs && lrhs && lrhs->asIntConst())
      {
    int32_t mask = lrhs->asIntConst()->getInt();
      int32_t shift=0;
      if(rhs->asIntRange())
         {
         TR::VPIntRange *range = rhs->asIntRange();
         int32_t low = range->getLowInt();
         int32_t high = range->getHighInt();
       if((node->getFirstChild()->getOpCodeValue() == TR::imul || node->getFirstChild()->getOpCodeValue() == TR::iumul) &&
       ((high & 0x80000000) ==0) &&
       ((low & 0x80000000) ==0))
          {
        while(!(mask  & 0x1))
           {
             shift++;
             mask >>= 1 ;
           }
          if(high <  1 << shift )
           constraint = TR::VPIntConst::create(vp, 0);
          }
       else if((node->getFirstChild()->getOpCodeValue() == TR::ishl || node->getFirstChild()->getOpCodeValue() == TR::iushl) &&
            ((high & 0x80000000) ==0) &&
            ((low & 0x80000000) ==0))
          {
          if(high < 1 << mask)
           constraint = TR::VPIntConst::create(vp, 0);
          }
         }
      else if(rhs->asIntConst())
         {
         int32_t iandMask =rhs->asIntConst()->getInt();
         if((node->getFirstChild()->getOpCodeValue() == TR::imul || node->getFirstChild()->getOpCodeValue() == TR::iumul) &&
           ((iandMask & 0x80000000) ==0))
            {
            while(!(mask  & 0x1))
             {
               shift++;
               mask >>= 1 ;
               }
            if(iandMask <  1 << shift )
              constraint = TR::VPIntConst::create(vp, 0);
            }
         else if((node->getFirstChild()->getOpCodeValue() == TR::ishl || node->getFirstChild()->getOpCodeValue() == TR::iushl) &&
                ((iandMask & 0x80000000) ==0))
            {
            if(iandMask < 1 << mask)
               constraint = TR::VPIntConst::create(vp, 0);
            }
         }
      }

   if (canBeRemoved &&
         performTransformation(vp->comp(), "%sRemoving redundant node [%p] %s\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      TR::Node *firstChild = node->getFirstChild();
      firstChild->incReferenceCount();
      vp->removeNode(node, false);
      node = firstChild;
      return node;
      }

   // need this to catch mask of 255 or shortint
   // FIXME: disabled for unsigned
   if(!constraint && (lhs || rhs)/* && !isUnsigned*/)
      {
      int32_t lhsHigh=TR::getMaxSigned<TR::Int32>(),rhsHigh=TR::getMaxSigned<TR::Int32>();
      int32_t lhsLow=TR::getMinSigned<TR::Int32>(),rhsLow=TR::getMinSigned<TR::Int32>();
      int32_t high,low;

      if(lhs && lhs->asIntRange())
         {
         TR::VPIntRange *range = lhs->asIntRange();
         lhsLow = range->getLowInt();
         lhsHigh = range->getHighInt();
         }

      if(rhs && rhs->asIntRange())
         {
         TR::VPIntRange *range = rhs->asIntRange();
         rhsLow = range->getLowInt();
         rhsHigh = range->getHighInt();
         }

      if(rhsLow >= 0 || lhsLow >=0)
         {
         if(rhsLow < 0) // punt
            {
            low = lhsLow;
            high = lhsHigh;
            }
         else if(lhsLow < 0)
            {
            low = rhsLow;
            high = rhsHigh;
            }
          else
            {
            low = rhsLow<lhsLow?rhsLow:lhsLow;
            high = rhsHigh<lhsHigh?rhsHigh:lhsHigh; // intersection, so will be smaller
            }
          if(low > 0) low =0;

          constraint = TR::VPIntRange::create(vp, low, high);

          //printf("new constraint opportunity %d-%d (%x to %x)found in %s\n", low,high,low,high,vp->comp()->signature());
         }
      }

   if (constraint)
      {
      if (constraint->asIntConst())
         {
         vp->replaceByConstant(node, constraint, lhsGlobal);
         return node;
         }
      vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
      }

   checkForNonNegativeAndOverflowProperties(vp, node);

   TR::Node * firstChild = node->getFirstChild();

   // ishr will propogate the sign bit.  If we mask off any propogation
   // then can replace with unsigned shift.  If we mask off more high bits
   // than are shifted, we can do it.
   if(TR::ishr == firstChild->getOpCodeValue() &&
      firstChild->getReferenceCount() < 2 /*&&
                                            firstChild->getFirstChild()->getType().isSignedInt()*/)  // FIXME: handle unsigned when supported
      {
      bool doNotShiftHighBit=false;
      bool isGlobal;
      TR::Node * shiftAmount = firstChild->getSecondChild();
      TR::VPConstraint *shiftConstraint = vp->getConstraint(shiftAmount, isGlobal);
      int32_t bitsShifted=32;

      if (shiftConstraint && shiftConstraint->asIntConst())
         {
         bitsShifted =  shiftConstraint->asIntConst()->getInt();
         }
      else if (shiftConstraint && shiftConstraint->asIntRange() &&
               shiftConstraint->asIntRange()->getLowInt() >=0)
         {
         bitsShifted = shiftConstraint->asIntRange()->getHighInt();
         }

      if((bitsShifted < (int32_t)signBitsMaskedOff) && vp->lastTimeThrough() &&
         performTransformation(vp->comp(), "%s Node [" POINTER_PRINTF_FORMAT "]: ishr -> iushr (parent ignores sign bits)\n",
                                OPT_DETAILS, firstChild))
         {
         TR::Node::recreate(firstChild, TR::iushr);
         //TR::Node *lhs = firstChild->getFirstChild();
         //TR::Node *rhs = firstChild->getSecondChild();
         //TR::Node::recreate(firstChild, TR::iu2i);
         //firstChild->setNumChildren(1);
         //TR::Node *newIushrNode = TR::Node::create(vp->comp(), TR::iushr, 2, lhs, rhs);
         //firstChild->setAndIncChild(0, newIushrNode);
         //lhs->decReferenceCount();
         //rhs->decReferenceCount();
         }
      }

   return node;
   }

TR::Node *constrainLand(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   TR::VPConstraint *constraint = NULL;
   if (rhs && rhs->asLongConst())
      {
      int64_t mask = rhs->asLongConst()->getLong();
      if (mask >= 0L)
         {
         node->setIsNonNegative(true);
         }
      if (mask == 0L)
         {
         constraint = TR::VPLongConst::create(vp, 0);
         }
      else if (lhs && lhs->asLongConst())
         {
         constraint = TR::VPLongConst::create(vp, lhs->asLongConst()->getLong() & mask);
         }
      else
         {
         // look for mod by power of 2 case
         if (mask != -1L &&
             isNonNegativePowerOf2(mask + 1))
            {
            int64_t low = 0;
            int64_t high = mask;
            if (lhs && lhs->getLowLong() >= 0 && lhs->getHighLong() <= mask)
               {
               if (lhs->getLowLong() > 0)
                  low = lhs->getLowLong();
               if (lhs->getHighLong() >= 0)
                  high = lhs->getHighLong();
               }
            constraint = TR::VPLongRange::create(vp, low, high);
            }
         else if ((mask & TR::getMinSigned<TR::Int64>()) == 0L)
            {
            constraint = TR::VPLongRange::create(vp, 0L, mask);
            }
         else
            {
            constraint = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), mask & TR::getMaxSigned<TR::Int64>());
            }
         }
      }
   else if (lhs && lhs->getLowLong() > 0)
      {
      constraint = TR::VPLongRange::create(vp, 0L, lhs->getHighLong());
      }
   if (constraint)
      {
      if (constraint->asLongConst())
         {
         vp->replaceByConstant(node, constraint, lhsGlobal);
         return node;
         }
      vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainIor(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   //bool isUnsigned = node->getType().isUnsignedInt();

   if (lhs && lhs->asIntConst() &&
       rhs && rhs->asIntConst())
      {
      int32_t lhsConst = lhs->asIntConst()->getInt();
      int32_t rhsConst = rhs->asIntConst()->getInt();
      TR::VPConstraint *constraint = NULL;
      //if (isUnsigned)
      //   constraint = TR::VPIntConst::create(vp, ((uint32_t)lhsConst | (uint32_t)rhsConst), isUnsigned);
      //else
      constraint = TR::VPIntConst::create(vp, lhsConst | rhsConst/*, isUnsigned*/);
      vp->replaceByConstant(node, constraint, lhsGlobal);
      }

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLor(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   if (lhs && lhs->asLongConst() &&
       rhs && rhs->asLongConst())
      {
      int64_t lhsConst = lhs->asLongConst()->getLong();
      int64_t rhsConst = rhs->asLongConst()->getLong();
      TR::VPConstraint *constraint = TR::VPLongConst::create(vp, lhsConst | rhsConst);
      vp->replaceByConstant(node, constraint, lhsGlobal);
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainIxor(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   //bool isUnsigned = node->getType().isUnsignedInt();

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   TR::VPConstraint *constraint = NULL;
   if (lhs && rhs && rhs->asIntConst())
      {
      int32_t rhsConst = rhs->asIntConst()->getInt();
      if (lhs->asIntConst())
         {
         int32_t lhsConst = lhs->asIntConst()->getInt();
         //if (isUnsigned)
         //   constraint = TR::VPIntConst::create(vp, ((uint32_t)lhsConst ^ (uint32_t)rhsConst), isUnsigned);
         //else
         constraint = TR::VPIntConst::create(vp, lhsConst ^ rhsConst/*, isUnsigned*/);
         vp->replaceByConstant(node, constraint, lhsGlobal);
         return node;
         }
      if (rhsConst == 1 && lhs->asIntRange())
         {
         // Special common case of xor a range (usually 0->1) with 1
         //
         constraint = TR::VPIntRange::create(vp, (lhs->getLowInt() & ~1), (lhs->getHighInt() | 1)/*, isUnsigned*/);
         if (constraint)
            {
            vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
            }
         }
      }

   // Look for nested boolean negations
   //
   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainLxor(OMR::ValuePropagation *vp, TR::Node *node)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;
   if (lhs && lhs->asLongConst() &&
       rhs && rhs->asLongConst())
      {
      int64_t lhsConst = lhs->asLongConst()->getLong();
      int64_t rhsConst = rhs->asLongConst()->getLong();
      TR::VPConstraint *constraint = TR::VPLongConst::create(vp, lhsConst ^ rhsConst);
      vp->replaceByConstant(node, constraint, lhsGlobal);
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

static TR::Node *constrainWidenToLong(OMR::ValuePropagation *vp, TR::Node *node, int64_t low, int64_t high, bool isUnsigned)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);
   if (constraint)
      {
      if (constraint->asIntConstraint())
         {
         if (isUnsigned)
            {
            if (constraint->getLowInt() > 0)
               low  = constraint->getLowInt();
            if (constraint->getLowInt() >= 0 && constraint->getHighInt() < high)
               high = constraint->getHighInt();
            }
         else
            {
            if (constraint->getLowInt() > low)
               low  = constraint->getLowInt();
            if (constraint->getHighInt() < high)
               high = constraint->getHighInt();
            }
         }
      else if (constraint->asShortConstraint())
         {
         if (isUnsigned)
            {
            if (constraint->getLowShort() > 0)
               low  = constraint->getLowShort();
            if (constraint->getLowShort() > 0 && constraint->getHighShort() < high)
               high = constraint->getHighShort();
            }
         else
            {
            if (constraint->getLowShort() > low)
               low  = constraint->getLowShort();
            if (constraint->getHighShort() < high)
               high = constraint->getHighShort();
            }
         }
      }

   if (low <= high)
      {
      // Make sure the constraint is not trivial
      //
      constraint = TR::VPLongRange::create(vp, low, high);
      if (constraint)
         {
         vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);
         }
      if (low >= 0)
         node->setIsNonNegative(true);
      }

   if (vp->isHighWordZero(node))
      node->setIsHighWordZero(true);

   checkForNonNegativeAndOverflowProperties(vp, node);
   return node;
   }

TR::Node *constrainI2l(OMR::ValuePropagation *vp, TR::Node *node)
   {

   if (node->getFirstChild()->isNonNegative())
     node->setIsNonNegative(true);

   return constrainWidenToLong(vp, node, TR::getMinSigned<TR::Int32>(), TR::getMaxSigned<TR::Int32>(), false);
   }

TR::Node *constrainIu2l(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainWidenToLong(vp, node, TR::getMinUnsigned<TR::Int32>(), TR::getMaxUnsigned<TR::Int32>(), true);
   }

void replaceWithSmallerType(OMR::ValuePropagation *vp, TR::Node *node)
   {
#if 1
   return;  // not enabled yet
#endif

   if (node->getReferenceCount() > 1)
      return;

   if (vp->comp()->getJittedMethodSymbol() && // avoid NULL pointer on non-Wcode builds
       vp->comp()->getJittedMethodSymbol()->isNoTemps())
      {
      dumpOptDetails(vp->comp(), "replaceWithSmallerType not safe to perform when NOTEMPS enabled\n");
      return;
      }

   TR::DataType newType = node->getDataType();
   TR::Node *load = node->getFirstChild();

   TR::Compilation *comp = vp->comp();
   TR::SymbolReference *oldRef =  load->getSymbolReference();

   if (!load->getOpCode().isLoadVar() ||
       load->getReferenceCount() > 1 ||
       !oldRef->getSymbol()->isAuto() ||
        oldRef->getUseDefAliases().hasAliases())
       return;


   TR_UseDefInfo *useDefInfo = vp->_useDefInfo;

   if (!useDefInfo)
      return;

   uint16_t useIndex = load->getUseDefIndex();

   if (!useIndex || !useDefInfo->isUseIndex(useIndex))
      return;

   TR_UseDefInfo::BitVector defs(vp->comp()->allocator());
   useDefInfo->getUseDef(defs, useIndex);
   if (defs.IsZero() || (defs.PopulationCount() > 1))
      return;
   TR_UseDefInfo::BitVector::Cursor cursor(defs);
   cursor.SetToFirstOne();
   int32_t defIndex = cursor;
   if (defIndex < useDefInfo->getFirstRealDefIndex())
      return;

   TR::Node *defNode = useDefInfo->getNode(defIndex);
   if (!defNode->getOpCode().isStore())
       return;

   TR_UseDefInfo::BitVector usesFromDefs(comp->allocator());
   useDefInfo->getUsesFromDef(usesFromDefs, defIndex);
   if (usesFromDefs.PopulationCount() > 1)
      return;

   TR::Node *value = defNode->getFirstChild();

   TR::ILOpCodes convOpcode = TR::ILOpCode::getProperConversion(value->getDataType(), newType, false /* !wantZeroExtension */);
   if (convOpcode == TR::BadILOp)
      return;


   if (performTransformation(comp, "%sReplace [" POINTER_PRINTF_FORMAT "] and [" POINTER_PRINTF_FORMAT "] with store and load of a smaller type\n", OPT_DETAILS, defNode, node))
      {
      // Transform
      TR::Node *conv = TR::Node::create(convOpcode, 1, value);
      defNode->setAndIncChild(0, conv);
      value->recursivelyDecReferenceCount();

      // change opcode of the def and use nodes to store and load smaller type
      TR::Node::recreate(defNode, comp->il.opCodeForDirectStore(newType));
      TR::Node::recreate(node, comp->il.opCodeForDirectLoad(newType));
      node->getFirstChild()->recursivelyDecReferenceCount();
      node->setNumChildren(0);

      // create new temp
      TR::SymbolReference *symRef = comp->getSymRefTab()->createTemporary(vp->comp()->getMethodSymbol(), newType);
      defNode->setSymbolReference(symRef);
      node->setSymbolReference(symRef);
      }

   }


static TR::Node *constrainNarrowIntValue(OMR::ValuePropagation *vp, TR::Node *node, int32_t low, int32_t high)
   {
   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);

   if (constraint)
      {
      if (constraint->asIntConstraint() || constraint->asMergedIntConstraints())
         {
         if (constraint->getLowInt() > low && constraint->getHighInt() < high)
            {
            if (constraint->getLowInt() > low)
               low  = constraint->getLowInt();
            if (constraint->getHighInt() < high)
               high = constraint->getHighInt();
            }
         }
      else if (constraint->asLongConstraint() || constraint->asMergedLongConstraints())
         {
         if (constraint->getLowLong() > low && constraint->getHighLong() < high)
            {
            if (constraint->getLowLong() <= TR::getMaxSigned<TR::Int32>() &&
               constraint->getLowLong() > low)
               low  = (int32_t)constraint->getLowLong();
            if (constraint->getHighLong() >= TR::getMinSigned<TR::Int32>() &&
               constraint->getHighLong() < high)
               high = (int32_t)constraint->getHighLong();
            }

         if (vp->comp()->getOptions()->getDebugEnableFlag(TR_EnableUnneededNarrowIntConversion) &&
             constraint->getLowLong() >= 0 && node->isUnsigned())
            {
            node->setUnneededConversion(true);
            }
         }
      }

   if (low <= high)
      {
      // Make sure the constraint is not trivial
      //
      int64_t min = TR::getMinSigned<TR::Int32>(), max = TR::getMaxSigned<TR::Int32>();
      switch (node->getDataType())
         {
         case TR::Int16: constraint = TR::VPShortRange::create(vp, low, high); min = TR::getMinSigned<TR::Int16>(); max = TR::getMaxSigned<TR::Int16>(); break;
         case TR::Int8:  min = TR::getMinSigned<TR::Int8>(); max = TR::getMaxSigned<TR::Int8>();    // byte nodes are still using Int constraint
         case TR::Int32: constraint = TR::VPIntRange::create(vp, low, high); break;
         default: TR_ASSERT(false, "Invalid node datatype");
         }
      if (constraint)
         {
         vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);
         }
      if (low >= 0)
         node->setIsNonNegative(true);
      if ((low > min) || (high < max))
         node->setCannotOverflow(true);
      }

   replaceWithSmallerType(vp, node);

   return node;
   }

TR::Node *constrainNarrowToByte(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainNarrowIntValue(vp, node, TR::getMinSigned<TR::Int8>(), TR::getMaxSigned<TR::Int8>());
   }

TR::Node *constrainNarrowToShort(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainNarrowIntValue(vp, node, TR::getMinSigned<TR::Int16>(), TR::getMaxSigned<TR::Int16>());
   }

TR::Node *constrainNarrowToChar(OMR::ValuePropagation *vp, TR::Node *node)
   {
   int32_t low = TR::getMinUnsigned<TR::Int16>();
   int32_t high = TR::getMaxUnsigned<TR::Int16>();
   int32_t max = TR::getMaxUnsigned<TR::Int16>() + 1;

   if (findConstant(vp, node))
      return node;
   constrainChildren(vp, node);
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);

   if (node->getFirstChild()->getOpCodeValue() == TR::b2i
       || node->getFirstChild()->getOpCodeValue() == TR::bu2i
       || node->getFirstChild()->getOpCodeValue() == TR::b2s
       || node->getFirstChild()->getOpCodeValue() == TR::bu2s
       || node->getFirstChild()->getOpCodeValue() == TR::b2l
       || node->getFirstChild()->getOpCodeValue() == TR::bu2l)
      {
      max = TR::getMaxUnsigned<TR::Int8>() + 1;
      high = TR::getMaxUnsigned<TR::Int8>();
      }

   if (constraint)
      {
      if (constraint->asIntConstraint() || constraint->asMergedConstraints())
         {
         if (constraint->getLowInt() > 0
              && constraint->getLowInt() < TR::getMaxUnsigned<TR::Int16>()
              && constraint->getHighInt() > 0
              && constraint->getHighInt() < TR::getMaxUnsigned<TR::Int16>())
            {
            if (constraint->getLowInt() > low)
               low = constraint->getLowInt();
            if (constraint->getHighInt() < high)
               high = constraint->getHighInt();
            }

         if (constraint->getLowInt() < 0 && constraint->getLowInt() == constraint->getHighInt())
            low = high = max + constraint->getLowInt() % (TR::getMaxUnsigned<TR::Int16>() + 1);
         }
      else if (constraint->asLongConstraint() || constraint->asMergedLongConstraints())
         {
         if (constraint->getLowInt() > 0
              && constraint->getLowInt() < TR::getMaxUnsigned<TR::Int16>()
              && constraint->getHighInt() > 0
              && constraint->getHighInt() < TR::getMaxUnsigned<TR::Int16>())
            {
            if (constraint->getLowLong() <= TR::getMaxSigned<TR::Int32>() && constraint->getLowLong() > low)
               low  = (int32_t)constraint->getLowLong();
            if (constraint->getHighLong() >= TR::getMinSigned<TR::Int32>() && constraint->getHighLong() < high)
               high = (int32_t)constraint->getHighLong();
            }

         if (constraint->getLowLong() < 0 && constraint->getLowLong() == constraint->getHighLong())
            low = high = max + (int32_t)constraint->getLowLong() % (TR::getMaxUnsigned<TR::Int16>() + 1);
         }
      }

   if (low <= high)
      {
      // Make sure the constraint is not trivial
      //
      constraint = TR::VPIntRange::create(vp, low, high/*, true*/);
      if (constraint)
         {
         vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);
         }
      if (low >= 0)
         node->setIsNonNegative(true);

      if ((low > TR::getMinSigned<TR::Int32>()) || (high < TR::getMaxSigned<TR::Int32>()))
         node->setCannotOverflow(true);
      }

   replaceWithSmallerType(vp, node);

   return node;
   }

TR::Node *constrainNarrowToInt(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainNarrowIntValue(vp, node, TR::getMinSigned<TR::Int32>(), TR::getMaxSigned<TR::Int32>());
   }

/*
static int32_t findConstantValue(OMR::ValuePropagation *vp, TR::Node *node, TR::VPConstraint *constraint)
   {
   int32_t value = constraint->asIntConst()->getInt();
   int32_t high = 0;
   switch (node->getOpCodeValue())
      {
      case TR::bu2i:
          high = TR::getMaxUnsigned<TR::Int8>()+1;
          break;
      case TR::su2i:
          high = TR::getMaxUnsigned<TR::Int16>()+1;
          break;
      }

   // if the consumer wants an unsigned value,
   // then find the right constant that needs to
   // be propagated. otherwise, the value is already
   // correct (i.e. if it is negative, then it needs to
   // be sign-extended as we widen)
   //
   if (high > 0)
      {
      // value is negative, which means that it wrapped
      // around on the smaller datatype's number line
      //
      if (value < 0)
         value = high + value;
      }
   return value;
   }
*/


static bool constrainWidenToInt(OMR::ValuePropagation *vp, TR::Node*& node, int32_t low, int32_t high, bool isUnsigned,
      TR::ILOpCodes correspondingNarrowingOpCode)
   {

   if (findConstant(vp, node))
      return true;

   constrainChildren(vp, node);

   bool             isGlobal;
   TR::Node        *firstChild             = node->getFirstChild();
   TR::Node        *grandChild             = firstChild->getNumChildren() > 0 ? firstChild->getFirstChild() : 0;
   TR::VPConstraint *constraint             = vp->getConstraint(firstChild, isGlobal);
   TR::VPConstraint *preNarrowingConstraint = 0;
   bool             yankConversionPair     = false;

   // we can constant propagate here if the first child
   // is known to be a constant
   // FIXME: disabled as we'll let simplifier take care of this
   /*
   if (constraint && constraint->asIntConst())
      {
      ///int32_t result = findConstantValue(vp, node, constraint);
      ///TR::VPConstraint *c = TR::VPIntConst::create(vp, result);
      ///vp->replaceByConstant(node, c, isGlobal);
      vp->replaceByConstant(node, constraint, isGlobal);
      return true;
      }
   */

   if (firstChild->getOpCodeValue() == correspondingNarrowingOpCode)
      {
      preNarrowingConstraint = vp->getConstraint(firstChild->getFirstChild(), isGlobal);
      if (preNarrowingConstraint)
         {
         if (isUnsigned)
            {
            if (preNarrowingConstraint->getLowInt() >= 0 &&
                preNarrowingConstraint->getHighInt() <= high)
               {
               yankConversionPair = true;
               }
            }
         else
            {
            if (preNarrowingConstraint->getLowInt() >= low &&
                preNarrowingConstraint->getHighInt() <= high)
               {
               yankConversionPair = true;
               }
            }
         }
      }

   ///traceMsg(vp->comp(), "yankConversionPair %d node %p\n", yankConversionPair, node);
   TR::Node *origNode = node;
   // static const char *p = feGetEnv("TR_ConversionFolder");
   if (//p &&
       yankConversionPair)
      {
      // have to increment the reference count of the grandchild to keep it from
      // being completele removed by removeNode
      //printf("found one (sign extension conversion removal) in method %s, node %p, low was %d, high was %d\n", vp->comp()->signature(), grandChild, preNarrowingConstraint->getLowInt(), preNarrowingConstraint->getHighInt());
      grandChild->incReferenceCount();
      vp->removeNode(node, false);
      node = grandChild;
      }

   if (constraint)
      {
      if (isUnsigned)
         {
         // Checks below to adjust high have been changed to >= (they were previously just >).
         //
         // The check for constraint->getLowInt() >= 0 is still needed. If the number was
         // defined as PIC S999 (3 digits, signed), the incoming range would be [-999, 999].
         // Widened to an unsigned integer, this gives the disjoint range [0, 999], [64537, 65535],
         // which, when merged into one constraint, would be [0, 65535]. In this case, low and high
         // should already be 0 and 65535, so we don't want to adjust anything based on the child
         // constraint.
         if (constraint->asShortConstraint())
            {
            if (constraint->getLowShort() > 0)
               low  = constraint->getLowShort();
            if (constraint->getLowShort() >= 0 && constraint->getHighShort() < high)
               high = constraint->getHighShort();
            }
         else
            {
            if (constraint->getLowInt() > 0)
               low  = constraint->getLowInt();
            if (constraint->getLowInt() >= 0 && constraint->getHighInt() < high)
               high = constraint->getHighInt();
            }
         }
      else
         {
         // the underlying constraint is the unsigned bit representation of
         // the desired constraint
#if 0
         int32_t newLow, newHigh;
         if (constraint->asIntConst())
            {
            if (correspondingNarrowingOpCode == TR::i2s)
               low = (int16_t)constraint->getLowInt();
            else
               low = (int8_t)constraint->getLowInt();
            // constant
            high = low;
            }
         else // range
            {
            int32_t maxLow = low;
            int32_t maxHigh;
            if (correspondingNarrowingOpCode == TR::i2s)
               {
               newLow = (int16_t)constraint->getLowInt();
               newHigh = (int16_t)constraint->getHighInt();
               maxHigh = TR::getMaxUnsigned<TR::Int16>();
               }
            else
               {
               newLow = (int8_t)constraint->getLowInt();
               newHigh = (int8_t)constraint->getHighInt();
               maxHigh = TR::getMaxUnsigned<TR::Int8>();
               }
            // constrain the low
            if (newLow > low)
               low = newLow;
            // now constrain the high
            // check for overflow on the byte/short numberline
            // if so, then we don't know anything about the new range
            //
            if (constraint->getHighInt() > high) // overflow
               {
               low = maxLow;
               high = maxHigh;
               }
            else if (newHigh < high)
               high = newHigh;
            }
#else
         if (constraint->asShortConstraint())
          {
           if (constraint->getLowShort() > low)
              low  = constraint->getLowShort();
           if (constraint->getHighShort() < high)
              high = constraint->getHighShort();
          }
         else
          {
           if (constraint->getLowInt() > low)
              low  = constraint->getLowInt();
           if (constraint->getHighInt() < high)
              high = constraint->getHighInt();
           }
#endif
         }
      }

   if (low <= high)
      {
      // Make sure the constraint is not trivial
      //
      constraint = TR::VPIntRange::create(vp, low, high);
      if (constraint)
         {
         // the node may have changed due to yankConversionPair
         // use the origNode to apply the constraint
         // consider the scenario
         // istore a
         //    c2i
         //      i2c
         //        iconst 50
         //  BNDCHK
         //     iconst 10
         //     iload a
         // 'a' and 'c2i' have the same valuenumber, but if the new
         // node is used (child of i2c) then the store constraint for 'a' is not
         // created (ie. when load 'a' is encountered later, there is no
         // matching constraint for the vn of 'a')
         // this is can be problematic for vp as the BNDCHK is never optimized
         // away and vp ends up analyzing dead code (the constraint for load 'a'
         // is now [0-10] which conflicts with [50])
         //
         vp->addBlockOrGlobalConstraint(origNode, constraint ,isGlobal);
         }
      if (low >= 0)
         node->setIsNonNegative(true);

      if (high <= 0)
         node->setIsNonPositive(true);

      if ((node->getOpCode().isArithmetic() || node->getOpCode().isLoad()) &&
            ((low > TR::getMinSigned<TR::Int32>()) ||  (high < TR::getMaxSigned<TR::Int32>())))
         node->setCannotOverflow(true);
      }
   return false;
   }

TR::Node *constrainB2i(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToInt(vp, node, TR::getMinSigned<TR::Int8>(), TR::getMaxSigned<TR::Int8>(), false, TR::i2b);
   return node;
   }

TR::Node *constrainB2s(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToInt(vp, node, TR::getMinSigned<TR::Int8>(), TR::getMaxSigned<TR::Int8>(), false, TR::s2b);
   return node;
   }

TR::Node *constrainBu2i(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToInt(vp, node, TR::getMinUnsigned<TR::Int8>(), TR::getMaxUnsigned<TR::Int8>(), true, TR::i2b);
   return node;
   }

TR::Node *constrainBu2s(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToInt(vp, node, TR::getMinUnsigned<TR::Int8>(), TR::getMaxUnsigned<TR::Int8>(), true, TR::s2b);
   return node;
   }

TR::Node *constrainB2l(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToLong(vp, node, TR::getMinSigned<TR::Int8>(), TR::getMaxSigned<TR::Int8>(), false);
   return node;
   }

TR::Node *constrainBu2l(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToLong(vp, node, TR::getMinUnsigned<TR::Int8>(), TR::getMaxUnsigned<TR::Int8>(), true);
   return node;
   }

TR::Node *constrainS2i(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToInt(vp, node, TR::getMinSigned<TR::Int16>(), TR::getMaxSigned<TR::Int16>(), false, TR::i2s);
   return node;
   }

TR::Node *constrainSu2i(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToInt(vp, node, TR::getMinUnsigned<TR::Int16>(), TR::getMaxUnsigned<TR::Int16>(), true, TR::i2s);
   return node;
   }

TR::Node *constrainS2l(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToLong(vp, node, TR::getMinSigned<TR::Int16>(), TR::getMaxSigned<TR::Int16>(), false);
   return node;
   }

TR::Node *constrainSu2l(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainWidenToLong(vp, node, TR::getMinUnsigned<TR::Int16>(), TR::getMaxUnsigned<TR::Int16>(), true);
   return node;
   }

static void changeConditionalToGoto(OMR::ValuePropagation *vp, TR::Node *node, TR::CFGEdge *branchEdge)
   {
#ifdef J9_PROJECT_SPECIFIC
   createGuardSiteForRemovedGuard(vp->comp(), node);
#endif

   // Fall through path is unreachable.
   // Set the current constraints to "unreachable path"
   //
   vp->setUnreachablePath();

   // Remove the children and change the node to a goto,
   //
   vp->removeChildren(node, false);
   TR::Node::recreate(node, TR::Goto);
   vp->setEnableSimplifier();

   // Remember that the fall-through edge is to be removed
   // But do not remove it if fall-through and target blocks
   // are the same.
   TR::Block *fallThrough = vp->_curBlock->getExit()->getNextTreeTop()->getNode()->getBlock();
   TR::CFGEdge *edge = vp->findOutEdge(vp->_curBlock->getSuccessors(), fallThrough);
   TR::Block *target = node->getBranchDestination()->getNode()->getBlock();
   if (fallThrough != target)
      vp->_edgesToBeRemoved->add(edge);
   vp->printEdgeConstraints(vp->createEdgeConstraints(edge, true));
   }

static void removeConditionalBranch(OMR::ValuePropagation *vp, TR::Node *node, TR::CFGEdge *branchEdge)
   {
#ifdef J9_PROJECT_SPECIFIC
   createGuardSiteForRemovedGuard(vp->comp(), node);
#endif

   // Branch path is unreachable.
   // Set the edge constraints on the branch path to "unreachable path"
   //
   vp->setUnreachablePath(branchEdge);

   // Remove this tree.
   //
   vp->removeNode(node, false);
   vp->_curTree->setNode(NULL);
   vp->setEnableSimplifier();
   // Do not remove the branch edge if both
   // fall-through and target blocks are the same.
   TR::Block *fallThrough = vp->_curBlock->getExit()->getNextTreeTop()->getNode()->getBlock();
   TR::Block *target = node->getBranchDestination()->getNode()->getBlock();
   if (fallThrough != target)
      vp->_edgesToBeRemoved->add(branchEdge);
   }

// check for compatibility between types
//
static TR::VPConstraint*
genTypeResult(OMR::ValuePropagation *vp, TR::VPConstraint *objectRefConstraint,
               TR::VPConstraint *classConstraint, bool &applyType, bool isInstanceOf)
   {
   TR::VPConstraint *newConstraint = NULL;
   if (objectRefConstraint)
      {
      if (vp->trace())
            traceMsg(vp->comp(), "Preempting type intersection..\n");
      TR::VPClass *instanceClass = classConstraint->asClass();
      TR::VPClassPresence *presence = classConstraint->getClassPresence();
      TR::VPClassType *type = classConstraint->getClassType();
      if (instanceClass)
         {
         TR::VPClassType *newType;
         // dilute the type because don't want to get the 'fixed' property
         if (type && type->asFixedClass())
            newType = TR::VPResolvedClass::create(vp, type->getClass());
         else
            newType = type;
         bool castIsClassObject = false;
         bool castIsInterfaceImplementedByClass = false;

         // determine if the instanceClass is a java/lang/Class
         // if so, then the result constraint can only claim to
         // be a classObject
         //
         if (type && type->asResolvedClass())
            {
            TR::VPResolvedClass *rc = type->asResolvedClass();
            TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(rc->getClass());
            if (jlClass)
               {
               applyType = true;
               if (rc->getClass() == jlClass)
                  castIsClassObject = true;
               else if (rc->isClassObject() == TR_maybe)
                  castIsInterfaceImplementedByClass = true;
               }
            else
               applyType = false;
            }

         // check if the two types are compatible
         //
         instanceClass->typeIntersect(presence, newType, objectRefConstraint, vp);

         // intersections failed?
         //
         if (!presence && objectRefConstraint->getClassPresence() && instanceClass->getClassPresence())
            newConstraint = NULL;
         else if (!newType && objectRefConstraint->getClassType() && instanceClass->getClassType())
            {
            if (applyType)
               newConstraint = NULL;
            }

         // one of the types could be null
         else if (presence && presence->isNullObject())
            newConstraint = presence;
         else if (isInstanceOf &&
                  !objectRefConstraint->getClassType() &&
                  !castIsClassObject && !castIsInterfaceImplementedByClass &&
                  objectRefConstraint->isNonNullObject() &&
                     (objectRefConstraint->isClassObject() == TR_yes))
            {
            // objectRefconstraint does not have a type and only
            // has a classObject property i.e.
            // <classObject> intersecting with
            // <classObject, type, non-null>
            //
            // if the castClass type is not java/lang/Class (or one of the interfaces
            // implemented by it) and VP is analyzing an instanceof, then the types cannot
            // possibly be related - so the result should indicate incompatibility.
            // in other cases (equality or inequality) the result is not known.
            //
            newConstraint = NULL;
            }
         else
            {
            TR::VPObjectLocation *loc = NULL;
            if (castIsClassObject)
               {
               // the object could have no type to begin with,
               // if so, do *not* create the specialClass constraint
               //
               if (objectRefConstraint->getClassType())
                  newType = TR::VPResolvedClass::create(vp, (TR_OpaqueClassBlock *)VP_SPECIALKLASS);
               else
                  newType = NULL;
               }
            else if (castIsInterfaceImplementedByClass)
               {
               // dont want to propagate the interface implemented by Class
               // as the type if the object is a classobject
               if (objectRefConstraint->isClassObject() == TR_yes)
                  newType = NULL;
               }

            // inject a classObject property on the constraint
            if ((objectRefConstraint && (objectRefConstraint->isClassObject() == TR_yes)) ||
                  castIsClassObject)
               loc = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::ClassObject);

            // final construction
            newConstraint = TR::VPClass::create(vp, newType, presence, NULL, NULL, loc);
            }
         }
      }
   else
      {
      if (vp->trace())
         traceMsg(vp->comp(), "ObjectRef has no constraint, so applying cast class properties...\n");

      TR::VPClassType *newType = NULL;
      bool castIsClassObject = false;
      if (classConstraint->getClassType()->asResolvedClass())
         {
         // check if cast class is a java/lang/Class type
         //
         TR_OpaqueClassBlock *klass = classConstraint->getClassType()->getClass();
         TR_OpaqueClassBlock *jlClass = vp->fe()->getClassClassPointer(klass);
         if (jlClass)
            {
            applyType = true;
            if (klass == jlClass)
               {
               newType = NULL;
               castIsClassObject = true;
               }
            else
               newType = TR::VPResolvedClass::create(vp, classConstraint->getClass());
            }
         else
            applyType = false;
         }
      else
         {
         newType = classConstraint->getClassType();
         applyType = true;
         }
      if (applyType)
         {
         TR::VPObjectLocation *loc = NULL;
         if (castIsClassObject)
            loc = TR::VPObjectLocation::create(vp, TR::VPObjectLocation::JavaLangClassObject);

         newConstraint = TR::VPClass::create(vp, newType,
                                          classConstraint->getClassPresence(), NULL, NULL, loc);
         TR_ASSERT(newConstraint, "newConstraint cannot be NULL\n");
         }
      }
   if (vp->trace())
      {
      traceMsg(vp->comp(), "genTypeResult returning constraint: ");
      if (newConstraint == NULL)
         traceMsg(vp->comp(), "none (generalized?)");
      else
         newConstraint->print(vp->comp(), vp->comp()->getOutFile());
      traceMsg(vp->comp(), "\n");
      }
   return newConstraint;
   }

static TR::Node* getOriginalCallNode(TR_VirtualGuard* vGuard, TR::Compilation* comp, TR::Node* guardNode)
   {

   TR::Block * block = guardNode->getBranchDestination()->getNode()->getBlock();
   TR_ByteCodeInfo& bci = comp->getInlinedCallSite(vGuard->getCalleeIndex())._byteCodeInfo;

   for (TR::TreeTop* tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
      {
      if (tt->getNode()->getNumChildren())
         {
         TR::Node* possiblyCall = tt->getNode()->getFirstChild();

   TR::VPConstraint* constraintFromMethodPointer = NULL;
         if    (possiblyCall->getOpCode().isCall() &&
               possiblyCall->getOpCode().isIndirect() &&
               possiblyCall->getByteCodeInfo().getCallerIndex() == bci.getCallerIndex() &&
               possiblyCall->getByteCodeIndex() == bci.getByteCodeIndex())
               {
               return possiblyCall;
               }
         }
      }
   return NULL;
   }


#ifdef J9_PROJECT_SPECIFIC
static int numConcreteClasses (List<TR_PersistentClassInfo>* subClasses)
  {

  TR::Compilation* comp = TR::comp();
  int count = 0;
  ListIterator<TR_PersistentClassInfo> i(subClasses);
  for (TR_PersistentClassInfo *ptClassInfo = i.getFirst(); ptClassInfo; ptClassInfo = i.getNext())
    {
    TR_OpaqueClassBlock* clazz = ptClassInfo->getClassId();
    if  (!TR::Compiler->cls.isInterfaceClass(comp, clazz) && !TR::Compiler->cls.isAbstractClass(comp, clazz))
      {
      count++;
      }
    }

  return count ;
  }
#endif


static void addDelayedConvertedGuard (TR::Node* node,
                                       TR::Node* callNode,
                                       TR::ResolvedMethodSymbol* methodSymbol,
                                       TR_VirtualGuard* oldVirtualGuard,
                                       OMR::ValuePropagation* vp,
                                       TR_VirtualGuardKind guardKind,
                                       TR_OpaqueClassBlock* objectClass)
   {

   if (!callNode->getFirstChild()->getOpCode().hasSymbolReference() ||
       callNode->getFirstChild()->getSymbolReference() != vp->comp()->getSymRefTab()->findVftSymbolRef() ||
       !callNode->getFirstChild()->getFirstChild()->getOpCode().isLoadVarDirect() || //I guess, having a vft symbol implies that there is something underneath it?
       !callNode->getSecondChild()->getOpCode().isLoadVarDirect() || //it also implies, there should be a receiver argument?
       !callNode->getSecondChild()->getOpCode().hasSymbolReference() || //I think its safer to exclude statics
       !callNode->getSecondChild()->getSymbolReference()->getSymbol()->isAutoOrParm()
       )
      {
      return;
      }


   TR::Node *newReceiver=TR::Node::createLoad(callNode, callNode->getSecondChild()->getSymbolReference());
   TR::Node* newGuardNode = TR_VirtualGuard::createMethodGuardWithReceiver
                       (guardKind,
                       vp->comp(),
                       oldVirtualGuard->getCalleeIndex(),
                       callNode,
                       node->getBranchDestination(),
                       methodSymbol,
                       objectClass /*oldVirtualGuard->getThisClass()*/,
             newReceiver);

   if (vp->trace())
      {
      traceMsg(vp->comp(), "P2O: oldGuard %p newGuard %p currentTree %p\n", oldVirtualGuard, newGuardNode, vp->_curTree);
      }

   TR_VirtualGuard* newGuard = vp->comp()->findVirtualGuardInfo(newGuardNode);
   //do not add a new guard yet ... it will be added in doDelayedTransformation and we will fix the IL accordingly
   vp->comp()->removeVirtualGuard(newGuard);
   //finish the rest of a transformation in doDelayedTransformation
   vp->_convertedGuards.add(new (vp->trStackMemory()) OMR::ValuePropagation::VirtualGuardInfo(vp, oldVirtualGuard, newGuard, newGuardNode, callNode));

   }


#ifdef J9_PROJECT_SPECIFIC
TR_ResolvedMethod * findSingleImplementer(
   TR_OpaqueClassBlock * thisClass, int32_t cpIndexOrVftSlot, TR_ResolvedMethod * callerMethod, TR::Compilation * comp, bool locked, TR_YesNoMaybe useGetResolvedInterfaceMethod)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return 0;



   TR_PersistentClassInfo * classInfo = comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(thisClass, comp, true);
   if (!classInfo)
      {
      return 0;
      }

   TR_ResolvedMethod *implArray[2]; // collect maximum 2 implemeters if you can
   int32_t implCount = TR_ClassQueries::collectImplementorsCapped(classInfo, implArray, 2, cpIndexOrVftSlot, callerMethod, comp, locked, useGetResolvedInterfaceMethod);
   return (implCount == 1 ? implArray[0] : 0);
   }
#endif


// Handles ificmpeq, ificmpne, iflcmpeq, iflcmpne, ifacmpeq, ifacmpne
//
static TR::Node *constrainIfcmpeqne(OMR::ValuePropagation *vp, TR::Node *node, bool branchOnEqual)
   {
   constrainChildren(vp, node);

   // Find the output edge from the current block that corresponds to this
   // branch
   //
   TR::Block *target = node->getBranchDestination()->getNode()->getBlock();
   TR::Block *fallThrough = vp->_curBlock->getNextBlock();
   // check for target and fall through edges being the same
   if (fallThrough == target)
      return node;

   TR::VPConstraint* constraintFromMethodPointer = NULL;
   TR::Node *lhsChild = node->getFirstChild();
   TR::Node *rhsChild = node->getSecondChild();

   bool isGlobal;
   TR::VPConstraint *lhs = NULL;
   TR::VPConstraint *rhs = NULL;
   TR::VPConstraint *rel = NULL;


   TR::CFGEdge *edge = vp->findOutEdge(vp->_curBlock->getSuccessors(), target);

   bool cannotBranch      = false;
   bool cannotFallThrough = false;

   // quick check for virtual guards to
   // facilitate (in)equality checks below
   // vguards are handled specially later
   //
   bool ignoreVirtualGuard = true;
   if (((node->getOpCodeValue() == TR::ifacmpne ||
         node->getOpCodeValue() == TR::ifacmpeq ||
         node->getOpCodeValue() == TR::ificmpne ||
         node->getOpCodeValue() == TR::ificmpeq) &&
         node->isTheVirtualGuardForAGuardedInlinedCall()))
      {
      TR_VirtualGuard *vGuard = vp->comp()->findVirtualGuardInfo(node);
      if (vGuard && !vGuard->canBeRemoved())
         {
         if (vp->trace())
            traceMsg(vp->comp(), "   found virtual guard node %p with inner assumptions (cannot be removed)\n", node);
         ignoreVirtualGuard = false;
         }
      }

   // Quick check for the same value number
   //
   if (vp->getValueNumber(lhsChild) == vp->getValueNumber(rhsChild) && ignoreVirtualGuard)
      {
      if (vp->trace())
         traceMsg(vp->comp(), "   cmp children have same value number: %p = %p = %d\n", lhsChild, rhsChild, vp->getValueNumber(lhsChild));
      if (branchOnEqual)
         cannotFallThrough = true;
      else
         cannotBranch = true;
      }

   // See if there are absolute constraints on the children
   //
   else
      {
      lhs = vp->getConstraint(lhsChild, isGlobal);
      rhs = vp->getConstraint(rhsChild, isGlobal);

      if (lhs && rhs)
         {
         if (vp->trace())
            {
            traceMsg(vp->comp(), "   cmp %p child absolute constraints:\n      %p: ", node, lhsChild);
            lhs->print(vp);
            traceMsg(vp->comp(), "\n      %p: ", rhsChild);
            rhs->print(vp);
            traceMsg(vp->comp(), "\n");
            }
         if (lhs->mustBeEqual(rhs, vp) && ignoreVirtualGuard)
            {
            if (vp->trace())
               traceMsg(vp->comp(), "   cmp children must be equal by absolute constraints: %p == %p\n", lhsChild, rhsChild);
            if (branchOnEqual)
               cannotFallThrough = true;
            else
               cannotBranch = true;
            }
         else if (lhs->mustBeNotEqual(rhs, vp) && ignoreVirtualGuard)
            {
            if (vp->trace())
               traceMsg(vp->comp(), "   cmp children must be not equal by absolute constraints: %p != %p\n", lhsChild, rhsChild);
            if (branchOnEqual)
               cannotBranch = true;
            else
               cannotFallThrough = true;
            }
         }
      }

   // See if there are relative constraints on the children
   //
   if (!cannotBranch && !cannotFallThrough)
      {
      rel = vp->getConstraint(lhsChild, isGlobal, rhsChild);
      if (rel && ignoreVirtualGuard)
         {
         if (vp->trace())
            {
            traceMsg(vp->comp(), "   cmp %p child relative constraint: ", node);
            rel->print(vp);
            traceMsg(vp->comp(), "\n");
            }
         if (rel->mustBeEqual())
            {
            if (vp->trace())
               traceMsg(vp->comp(), "   cmp children must be equal by relative constraint: %p == %p\n", lhsChild, rhsChild);
            if (branchOnEqual)
               cannotFallThrough = true;
            else
               cannotBranch = true;
            }
         else if (rel->mustBeNotEqual())
            {
            if (vp->trace())
               traceMsg(vp->comp(), "   cmp children must be not equal by relative constraint: %p != %p\n", lhsChild, rhsChild);
            if (branchOnEqual)
               cannotBranch = true;
            else
               cannotFallThrough = true;
            }
         }
      }

   if (cannotBranch &&
       performTransformation(vp->comp(), "%sRemoving conditional branch [%p] %s\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      removeConditionalBranch(vp, node, edge);
      return node;
      }

   if (cannotFallThrough &&
       performTransformation(vp->comp(), "%sChanging node [%p] %s into goto\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      vp->printEdgeConstraints(vp->createEdgeConstraints(edge, false));
      changeConditionalToGoto(vp, node, edge);
      return node;
      }

#ifdef J9_PROJECT_SPECIFIC
   if (!vp->comp()->compileRelocatableCode() && vp->comp()->isOptServer() &&
       vp->lastTimeThrough() &&
       vp->comp()->performVirtualGuardNOPing() &&
       !vp->_curBlock->isCold() &&
       !vp->_curBlock->getNextBlock()->isExtensionOfPreviousBlock() &&
       (node->getNumChildren() == 2) &&
       ((node->getOpCodeValue() == TR::ifacmpeq) ||
        (node->getOpCodeValue() == TR::ifacmpne)))
      {
      TR::Node *first = node->getFirstChild();
      TR::Node *second = node->getSecondChild();
      if ((second->getOpCodeValue() == TR::aconst) &&
          (second->getAddress() == 0))
         {
         const char *clazzToBeInitialized = NULL;
         int32_t clazzNameLen = -1;
         bool isGlobal;
         TR::VPConstraint *objectRefConstraint = vp->getConstraint(first, isGlobal);
         if (objectRefConstraint)
            {
            TR::VPClassType *typeConstraint = objectRefConstraint->getClassType();
            if (typeConstraint)
               {
               TR::VPConstraint *resolvedTypeConstraint = typeConstraint->asResolvedClass();
               if (resolvedTypeConstraint)
                  {
                  TR_OpaqueClassBlock *clazz = resolvedTypeConstraint->getClass();

                  TR_PersistentClassInfo * classInfo =
                     vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, vp->comp());

                  if (vp->trace())
                     traceMsg(vp->comp(), "MyDebug: clazz %p classInfo %p classInfo->isInitialized() %d\n",
                              clazz, classInfo, classInfo ? classInfo->isInitialized() : -1);
                  if (!classInfo ||
                      !classInfo->isInitialized())
                     {
                     int32_t len;
                     const char *sig = resolvedTypeConstraint->getClassSignature(len);
                     clazzToBeInitialized = sig;
                     clazzNameLen = len;
                     }
                  }
               else
                  {
                  TR::VPUnresolvedClass *unresolvedTypeConstraint = typeConstraint->asUnresolvedClass();
                  if (unresolvedTypeConstraint)
                     {
                     int32_t len;
                     const char *sig = unresolvedTypeConstraint->getClassSignature(len);
                     TR_ResolvedMethod *owningMethod = unresolvedTypeConstraint->getOwningMethod();
                     if (owningMethod)
                        {
                        TR_OpaqueClassBlock *clazz = vp->fe()->getClassFromSignature(sig, len, owningMethod);
                        if (!clazz)
                           {
                           clazzToBeInitialized = sig;
                           clazzNameLen = len;
                           }
                        else
                           {
                           TR_PersistentClassInfo * classInfo =
                              vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, vp->comp());

                           if (vp->trace())
                              traceMsg(vp->comp(), "MyDebug: clazz %p classInfo %p classInfo->isInitialized() %d\n",
                                       clazz, classInfo, classInfo ? classInfo->isInitialized() : -1);
                           if (!classInfo ||
                               !classInfo->isInitialized())
                              {
                              clazzToBeInitialized = sig;
                              clazzNameLen = len;
                              }
                           }
                        }
                     }
                  }

               if (clazzToBeInitialized &&
                   (((clazzNameLen == 35) && !strncmp(clazzToBeInitialized, "Lcom/ibm/ejs/ras/TraceEnabledToken;", clazzNameLen)) ||
                    ((clazzNameLen == 41) && !strncmp(clazzToBeInitialized, "Lcom/ibm/websphere/ras/TraceEnabledToken;", clazzNameLen))) &&
                   performTransformation(vp->comp(), "%sUsing side-effect guard to fold away condition on a load from an uninitializec class  %s [%p]\n", OPT_DETAILS, clazzToBeInitialized, node))
                  {
                  char *clazzToBeInitializedCopy = (char *)vp->trMemory()->allocateMemory(clazzNameLen+1, heapAlloc);
                  strcpy(clazzToBeInitializedCopy, clazzToBeInitialized);
                  vp->_classesToCheckInit.add(new (vp->trStackMemory()) OMR::ValuePropagation::ClassInitInfo(vp, clazzToBeInitializedCopy, clazzNameLen));
                  }
               }
            }
         }
      }
#endif

   // Propagate current constraints to the branch target
   //
   if (vp->trace())
      traceMsg(vp->comp(), "   Conditional branch\n");
   OMR::ValuePropagation::EdgeConstraints *edgeConstraints = vp->createEdgeConstraints(edge, true);

   // Find constraints to apply to the not-equal edge
   //
   TR::VPConstraint *notEqualConstraint = NULL;
   TR::Node         *constraintChild = NULL;
   if (lhs)
      {
      constraintChild = rhsChild;
      if (lhs->isNullObject())
         notEqualConstraint = TR::VPNonNullObject::create(vp);
      else if (lhs->asIntConst())
         {
         //if (lhs->isUnsigned())
         //   notEqualConstraint = TR::VPIntConst::createExclusion(vp, lhs->asIntConst()->getInt(), true);
         //else
         notEqualConstraint = TR::VPIntConst::createExclusion(vp, lhs->asIntConst()->getInt());
         }
      else if (lhs->asLongConst())
         notEqualConstraint = TR::VPLongConst::createExclusion(vp, lhs->asLongConst()->getLong());
      }
   if (rhs && !notEqualConstraint)
      {
      constraintChild = lhsChild;
      if (rhs->isNullObject())
         notEqualConstraint = TR::VPNonNullObject::create(vp);
      else if (rhs->asIntConst())
         {
         //if (rhs->isUnsigned())
         //   notEqualConstraint = TR::VPIntConst::createExclusion(vp, rhs->asIntConst()->getInt(), true);
         //else
         notEqualConstraint = TR::VPIntConst::createExclusion(vp, rhs->asIntConst()->getInt());
         }
      else if (rhs->asLongConst())
         notEqualConstraint = TR::VPLongConst::createExclusion(vp, rhs->asLongConst()->getLong());
      }

   // See if one child is an instanceof and the other is a constant. If so, we
   // can apply a type constraint to the object child of the instanceof along
   // the path where the instanceof is true.
   //
   TR::VPConstraint *instanceofConstraint = NULL;
   TR::Node         *instanceofObjectRef = NULL;
   bool             instanceofOnBranch = false;
   bool instanceofDetected = false;
   bool instanceofDetectedAndFixedType = false;
   bool isInstanceOf = false;

   if (lhsChild->getOpCodeValue() == TR::instanceof &&
       rhs && rhs->asIntConst())
      {
      int32_t value = rhs->asIntConst()->getInt();
      if (value == 0 || value == 1)
         {
         instanceofObjectRef = lhsChild->getFirstChild();
         TR::Node *classChild = lhsChild->getSecondChild();
         TR::VPConstraint *classConstraint = vp->getConstraint(classChild, isGlobal);
         if (classConstraint && classConstraint->getClassType())
            {
            instanceofConstraint = classConstraint;
            instanceofDetected = true;
            isInstanceOf = true;
            if (value ^ (int32_t)branchOnEqual)
               instanceofOnBranch = false;
            else
               instanceofOnBranch = true;
            }
         }
      }

   // don't propagate the classConstraint to the object if the guard is nopable
   //
   bool isVirtualGuardNopable = node->isNopableInlineGuard();

   if (((node->getOpCodeValue() == TR::ifacmpne ||
         node->getOpCodeValue() == TR::ifacmpeq) &&
        node->isTheVirtualGuardForAGuardedInlinedCall()))
      {
      TR_VirtualGuard *virtualGuard = vp->comp()->findVirtualGuardInfo(node);
      if (virtualGuard)
         {
         switch (virtualGuard->getTestType())
            {
            case TR_VftTest:
                  {
#ifdef J9_PROJECT_SPECIFIC
                  instanceofObjectRef = node->getFirstChild();
                  TR::Node *classChild = node->getSecondChild();
                  bool foldedGuard = false;
                  static const char* enableJavaLangClassFolding = feGetEnv ("TR_EnableFoldJavaLangClass");
                  if (enableJavaLangClassFolding && (instanceofObjectRef->getOpCodeValue() == TR::aloadi) &&
                      (instanceofObjectRef->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef()))
                     {
                     TR::Node *objectChild = instanceofObjectRef->getFirstChild();
                     TR::VPConstraint *objectConstraint = vp->getConstraint(objectChild, isGlobal);
                     TR::VPConstraint *classConstraint = vp->getConstraint(classChild, isGlobal);

                     //if objectConstraint is equal to java/lang/Class we can fold the guard
                     if (ignoreVirtualGuard && classConstraint && classConstraint->getClassType() &&
                         objectConstraint && objectConstraint->isClassObject() ==  TR_yes && vp->comp()->getClassClassPointer())
                        {
                        bool       testForEquality  = (node->getOpCodeValue() == TR::ifacmpeq);
                        bool       childrenAreEqual = (vp->comp()->getClassClassPointer() == classConstraint->getClass());

                        if (testForEquality == childrenAreEqual)
                           cannotFallThrough = true;
                        else
                           cannotBranch = true;
                        foldedGuard = true;
                        }
                     }

                  if (!foldedGuard && (instanceofObjectRef->getOpCodeValue() == TR::aloadi) &&
                      (instanceofObjectRef->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef()))
                     instanceofObjectRef = instanceofObjectRef->getFirstChild();

                  TR::VPConstraint *classConstraint = vp->getConstraint(classChild, isGlobal);
                  //foldedGuard indicates that we already set cannotBranch or cannotFallThrough
                  if (!foldedGuard && classConstraint && classConstraint->getClassType())
                     {
                     instanceofConstraint = classConstraint;
                     instanceofDetectedAndFixedType = true;
                     instanceofDetected = true;
                     //printf("Reached here in %s\n", signature(vp->comp()->getCurrentMethod()));
                     if (node->getOpCodeValue() == TR::ifacmpne)
                        instanceofOnBranch = false;
                     else
                        instanceofOnBranch = true;
                     }
#endif
                  }
               break;
            case TR_MethodTest:
                  {
                  TR::Node *vtableEntryNode = node->getFirstChild();
                  if ((vtableEntryNode->getOpCodeValue() == TR::aloadi) &&
                      vtableEntryNode->getOpCode().hasSymbolReference() &&
                      vp->comp()->getSymRefTab()->isVtableEntrySymbolRef(vtableEntryNode->getSymbolReference()))
                     /*(vtableEntryNode->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef()))*/
                     {
                     TR::Node *classNode = vtableEntryNode->getFirstChild();
                     TR::Node   *methodPtrNode    = node->getSecondChild();
                     TR::VPConstraint *classConstraint = vp->getConstraint(classNode, isGlobal);
                     if (ignoreVirtualGuard && classConstraint && classConstraint->isFixedClass())
                        {
                        uint8_t   *clazz            = (uint8_t*)classConstraint->getClass();
                        int32_t    vftOffset        = vtableEntryNode->getSymbolReference()->getOffset();
                        intptrj_t  vftEntry         = *(intptrj_t*)(clazz + vftOffset);
                        bool       childrenAreEqual = (vftEntry == methodPtrNode->getAddress());
                        bool       testForEquality  = (node->getOpCodeValue() == TR::ifacmpeq);
                        traceMsg(vp->comp(), "TR_MethodTest: node=%p, vtableEntryNode=%p, clazz=%p, vftOffset=%d, vftEntry=%p, childrenAreEqual=%d, testForEquality=%d\n",
                                 node, vtableEntryNode, clazz, vftOffset, vftEntry, childrenAreEqual, testForEquality);
                        if (testForEquality == childrenAreEqual)
                           cannotFallThrough = true;
                        else
                           cannotBranch = true;
                        }

                     if (vtableEntryNode->getSymbolReference() == vp->comp()->getSymRefTab()->findVftSymbolRef())
                        {
                        TR_OpaqueClassBlock* classFromMethod = vp->comp()->fe()->getClassFromMethodBlock((TR_OpaqueMethodBlock*)methodPtrNode->getAddress());
                        constraintFromMethodPointer = TR::VPResolvedClass::create(vp, classFromMethod);
                        }

                     }
                  }
               break;
            default:
            	break;
            }
         }
      }

   // Avoid analyzing the virtual call path if we know that the call will
   // will be devirtualized. Devirtualization only happens late in VP
   // as this affects CFG which cannot change while analysis is in progress.
   // This code would help in avoiding the virtual call path when creating constraints
   // for a return value (for example) from a guarded virtual call
   //
   TR::VPConstraint *virtualGuardConstraint = NULL;
   bool virtualGuardWillBeEliminated = false;
   if (node->isTheVirtualGuardForAGuardedInlinedCall() &&
       node->isNonoverriddenGuard())
      {
   //TR_VirtualGuard *virtualGuard = vp->comp()->findVirtualGuardInfo(node);
      TR::Node *callNode = NULL;
      TR::TreeTop *targetTree = node->getBranchDestination();
      TR::Node *nextRealNode = targetTree->getNextRealTreeTop()->getNode();
      if (nextRealNode->getOpCode().isTreeTop() && (nextRealNode->getNumChildren() > 0))
         nextRealNode = nextRealNode->getFirstChild();

      if (nextRealNode->isTheVirtualCallNodeForAGuardedInlinedCall())
         callNode = nextRealNode;

      //if (!virtualGuard->isInlineGuard())
      //  callNode = virtualGuard->getCallNode();
      if (callNode &&
          callNode->getOpCode().isCallIndirect())
         {
         TR::SymbolReference *symRef        = callNode->getSymbolReference();
         TR::MethodSymbol    *methodSymbol  = symRef->getSymbol()->castToMethodSymbol();

         int32_t firstArgIndex = callNode->getFirstArgumentIndex();
         bool isGlobal;
         TR::VPConstraint *constraint = vp->getConstraint(callNode->getChild(firstArgIndex), isGlobal);
         TR_OpaqueClassBlock *thisType;
         //dumpOptDetails(vp->comp(), "Guard %p Call %p constraint %p\n", node, callNode, constraint);
         if (constraint &&
             constraint->isFixedClass() &&
             (thisType = constraint->getClass()) &&
              methodSymbol->isVirtual())
            {
            TR::ResolvedMethodSymbol * resolvedMethodSymbol = methodSymbol->getResolvedMethodSymbol();
            if (resolvedMethodSymbol)
               {
               TR_ResolvedMethod *originalResolvedMethod = resolvedMethodSymbol->getResolvedMethod();
               TR_OpaqueClassBlock *originalMethodClass = originalResolvedMethod->classOfMethod();

               if ((vp->fe()->isInstanceOf(thisType, originalMethodClass, true, true) == TR_yes) &&
                   !originalResolvedMethod->virtualMethodIsOverridden())
                 {
                 virtualGuardConstraint = constraint;
                 TR_VirtualGuard *virtualGuard = vp->comp()->findVirtualGuardInfo(node);
                 if (virtualGuard && virtualGuard->canBeRemoved())
                    virtualGuardWillBeEliminated = true;
                 }
              }
           }
        }
     }

   // Apply new constraints to the branch edge
   //
   if (branchOnEqual)
      {
      if (lhs)
         {
         TR::VPConstraint *resultType = NULL;
         int32_t result = 1;
         if (rhs && rhs->getClassType() && lhs->getClassType())
            {
            // if intersection fails, result = 0
            vp->checkTypeRelationship(lhs, rhs, result, false, false);
            if (!result)
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "   types are inconsistent, result will not be propagated\n");
               }
            }
         if (result && !isVirtualGuardNopable &&
               !vp->addEdgeConstraint(rhsChild, lhs, edgeConstraints))
            {
            // propagation of constraints failed
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      if (rhs)
         {
         TR::VPConstraint *resultType = NULL;
         int32_t result = 1;
         if (lhs && lhs->getClassType() && rhs->getClassType())
            {
            // if intersection fails, result = 0
            vp->checkTypeRelationship(lhs, rhs, result, false, false);
            if (!result)
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "   types are inconsistent, result will not be propagated\n");
               }

            }
         if (result && !isVirtualGuardNopable &&
               !vp->addEdgeConstraint(lhsChild, rhs, edgeConstraints))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }
   else
      {
      if (notEqualConstraint)
         {
         if (!vp->addEdgeConstraint(constraintChild, notEqualConstraint, edgeConstraints))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }

   if (instanceofConstraint && instanceofOnBranch)
      {
      if (instanceofDetected)
         {
         bool isGlobal;
         TR::VPConstraint *newConstraint = NULL;
         TR::VPConstraint *objectRefConstraint = vp->getConstraint(instanceofObjectRef, isGlobal);
         if (objectRefConstraint)
            {
            TR::VPClass *instanceClass = instanceofConstraint->asClass();
            TR_ASSERT(instanceClass, "Cast class must be a VPClass wrapper\n");
            if (instanceClass)
               {
               // AOT returns NULL for getClassClass currently,
               // so don't propagate type
               bool applyType = true;
               bool removeBranch = false;
               newConstraint = genTypeResult(vp, objectRefConstraint, instanceClass, applyType, isInstanceOf);

               TR::VPConstraint *newTypeConstraint = newConstraint;
               if (newTypeConstraint && !newTypeConstraint->asClassType() && newTypeConstraint->asClass())
                  newTypeConstraint = newTypeConstraint->asClass()->getClassType();
               else if (newTypeConstraint && !newTypeConstraint->asClassType())
                  newTypeConstraint = NULL;

               if (newTypeConstraint && instanceofDetectedAndFixedType &&
                   newTypeConstraint->asResolvedClass() && !newTypeConstraint->asFixedClass())
                  {
                  TR_OpaqueClassBlock *clazz = newTypeConstraint->asResolvedClass()->getClass();
                  if (clazz)
                     {
                     TR::VPFixedClass *newFixedConstraint = TR::VPFixedClass::create(vp, clazz);
                     newConstraint = newConstraint->intersect(newFixedConstraint, vp);
                     }
                  }

               if (!newConstraint && applyType)
                  removeBranch = true;
               else if (newConstraint && !isVirtualGuardNopable &&
                           !vp->addEdgeConstraint(instanceofObjectRef, newConstraint, edgeConstraints))
                  removeBranch = true;

               if (removeBranch)
                  {
                  if (!vp->intersectionFailed())
                     cannotBranch = true;
                  else
                     vp->setIntersectionFailed(false);
                  }
               }
            }
         else
            {
            bool applyType = true;
            newConstraint = genTypeResult(vp, objectRefConstraint, instanceofConstraint, applyType, isInstanceOf);

            TR::VPConstraint *newTypeConstraint = newConstraint;
            if (newTypeConstraint && !newTypeConstraint->asClassType() && newTypeConstraint->asClass())
                newTypeConstraint = newTypeConstraint->asClass()->getClassType();
            else if (newTypeConstraint && !newTypeConstraint->asClassType())
               newTypeConstraint = NULL;

            if (newTypeConstraint && instanceofDetectedAndFixedType &&
                newTypeConstraint->asResolvedClass() && !newTypeConstraint->asFixedClass())
               {
               TR_OpaqueClassBlock *clazz = newTypeConstraint->asResolvedClass()->getClass();
               if (clazz)
                  {
                  TR::VPFixedClass *newFixedConstraint = TR::VPFixedClass::create(vp, clazz);
                  newConstraint = newConstraint->intersect(newFixedConstraint, vp);
                  }
               }

            if (applyType && !isVirtualGuardNopable &&
                  !vp->addEdgeConstraint(instanceofObjectRef, newConstraint, edgeConstraints))
               {
               if (!vp->intersectionFailed())
                  cannotBranch = true;
               else
                  vp->setIntersectionFailed(false);
               }
            }
         }
      ///if (!vp->addEdgeConstraint(instanceofObjectRef, instanceofConstraint, edgeConstraints))
      else if (!vp->addEdgeConstraint(instanceofObjectRef, instanceofConstraint, edgeConstraints))
         cannotBranch = true;
      }


   // Apply relative constraint to the branch edge
   //
   // Do not create the (in)equality constraint on object comparison.  There is no
   // intrinsic value of the comparison, and instead we end with way too many objects
   // related together because they were all related to null.  This increases the time
   // it takes to propagate constraints, but provides no useful value since we track
   // (non-)nullness using a different mechanism anyway.
   //
   if (node->getOpCodeValue() != TR::ifacmpeq && node->getOpCodeValue() != TR::ifacmpne)
      {
      if (branchOnEqual)
         {
         if (!vp->addEdgeConstraint(lhsChild, TR::VPEqual::create(vp, 0), edgeConstraints, rhsChild))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      else
         {
         if (!vp->addEdgeConstraint(lhsChild, TR::VPNotEqual::create(vp, 0), edgeConstraints, rhsChild))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }

   if (vp->trace() && !cannotBranch)
      vp->printEdgeConstraints(edgeConstraints);

   // Apply new constraints to the fall through edge
   //
   if (branchOnEqual)
      {
      if (notEqualConstraint)
         {
         if (!vp->addBlockConstraint(constraintChild, notEqualConstraint, NULL, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }
   else
      {
      if (lhs)
         {
         TR::VPConstraint *resultType = NULL;
         int32_t result = 1;
         if (rhs && rhs->getClassType() && lhs->getClassType())
            {
            // if intersection fails, result = 0
            vp->checkTypeRelationship(lhs, rhs, result, false, false);
            if (!result)
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "   types are inconsistent, result will not be propagated\n");
               }
            }

         if (result && !isVirtualGuardNopable &&
               !vp->addBlockConstraint(rhsChild, lhs, NULL, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      if (rhs)
         {
         TR::VPConstraint *resultType = NULL;
         int32_t result = 1;
         if (lhs && lhs->getClassType() && rhs->getClassType())
            {
            // if intersection fails, result = 0
            vp->checkTypeRelationship(lhs, rhs, result, false, false);
            if (!result)
               {
               if (vp->trace())
                  traceMsg(vp->comp(), "   types are inconsistent, result will not be propagated\n");
               }
            }

         if (result && !isVirtualGuardNopable &&
               !vp->addBlockConstraint(lhsChild, rhs, NULL, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }

   if (instanceofConstraint && !instanceofOnBranch)
      {
      if (instanceofDetected)
         {
         bool isGlobal;
         TR::VPConstraint *newConstraint = NULL;
         TR::VPConstraint *objectRefConstraint = vp->getConstraint(instanceofObjectRef, isGlobal);
         if (objectRefConstraint)
            {
            TR::VPClass *instanceClass = instanceofConstraint->asClass();
            TR_ASSERT(instanceClass, "Cast class must be a VPClass wrapper\n");
            if (instanceClass)
               {
               // AOT returns NULL for getClassClass currently,
               // so don't propagate type
               bool applyType = true;
               bool removeBranch = false;
               newConstraint = genTypeResult(vp, objectRefConstraint, instanceClass, applyType, isInstanceOf);

               TR::VPConstraint *newTypeConstraint = newConstraint;
               if (newTypeConstraint && !newTypeConstraint->asClassType() && newTypeConstraint->asClass())
                  newTypeConstraint = newTypeConstraint->asClass()->getClassType();
               else if (newTypeConstraint && !newTypeConstraint->asClassType())
                  newTypeConstraint = NULL;

               if (newTypeConstraint && instanceofDetectedAndFixedType &&
                   newTypeConstraint->asResolvedClass() && !newTypeConstraint->asFixedClass())
                  {
                  TR_OpaqueClassBlock *clazz = newTypeConstraint->asResolvedClass()->getClass();
                  if (clazz)
                     {
                     TR::VPFixedClass *newFixedConstraint = TR::VPFixedClass::create(vp, clazz);
                     newConstraint = newConstraint->intersect(newFixedConstraint, vp);
                     }
                  }

               if (!newConstraint && applyType)
                  removeBranch = true;
               else if (newConstraint && !isVirtualGuardNopable &&
                           !vp->addBlockConstraint(instanceofObjectRef, newConstraint, NULL, false))
                  removeBranch = true;
               if (removeBranch)
                  {
                  if (!vp->intersectionFailed())
                     cannotFallThrough = true;
                  else
                     vp->setIntersectionFailed(false);
                  }
               }
            }
         else
            {
            bool applyType = true;
            newConstraint = genTypeResult(vp, objectRefConstraint, instanceofConstraint, applyType, isInstanceOf);
            TR::VPConstraint *newTypeConstraint = newConstraint;
            if (newTypeConstraint && !newTypeConstraint->asClassType() && newTypeConstraint->asClass())
               newTypeConstraint = newTypeConstraint->asClass()->getClassType();
            else if (newTypeConstraint && !newTypeConstraint->asClassType())
               newTypeConstraint = NULL;

            if (newTypeConstraint && instanceofDetectedAndFixedType &&
                newTypeConstraint->asResolvedClass() && !newTypeConstraint->asFixedClass())
               {
               TR_OpaqueClassBlock *clazz = newTypeConstraint->asResolvedClass()->getClass();
               if (clazz)
                  {
                  TR::VPFixedClass *newFixedConstraint = TR::VPFixedClass::create(vp, clazz);
                  newConstraint = newConstraint->intersect(newFixedConstraint, vp);
                  }
               }

            if (applyType && !isVirtualGuardNopable &&
                  !vp->addBlockConstraint(instanceofObjectRef, newConstraint, NULL, false))
               {
               if (!vp->intersectionFailed())
                  cannotFallThrough = true;
               else
                  vp->setIntersectionFailed(false);
               }
            }
         }
      else if (!vp->addBlockConstraint(instanceofObjectRef, instanceofConstraint, NULL, false))
         cannotFallThrough = true;
      }

   if (virtualGuardConstraint && virtualGuardWillBeEliminated)
      cannotBranch = true;

   if (node->getOpCodeValue() != TR::ifacmpeq && node->getOpCodeValue() != TR::ifacmpne)
      {
      // Apply relative constraint to the fall through edge
      //
      if (branchOnEqual)
         {
         if (!vp->addBlockConstraint(lhsChild, TR::VPNotEqual::create(vp, 0), rhsChild, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      else
         {
         if (!vp->addBlockConstraint(lhsChild, TR::VPEqual::create(vp, 0), rhsChild, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }

   if (cannotBranch &&
       performTransformation(vp->comp(), "%sRemoving conditional branch [%p] %s\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      TR_ASSERT(!cannotFallThrough, "Cannot branch or fall through");
      removeConditionalBranch(vp, node, edge);
      }
   else if (cannotFallThrough &&
            performTransformation(vp->comp(), "%sChanging node [%p] %s into goto\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      TR_ASSERT(!cannotBranch, "Cannot branch or fall through");
      changeConditionalToGoto(vp, node, edge);
      }
   else if (constraintFromMethodPointer && lhsChild->getFirstChild()->getFirstChild()->getOpCode().isLoadVarDirect())
      {
      //if constraintFromMethodPointer is set we should be able to assume that lhsChild->getFirstChild()->getFirstChild() is not NULL, since non-NULL constraintFromMethodPointer implies the following trees:
      // n235n     ifacmpne --> block_25 BBStart at n230n (inlineProfiledGuard )                       [0x7fffd5110a40] bci=[0,0,314] rc=0 vc=56 vn=49 sti=- udi=- nc=2 flg=0x1020
      // n233n       iaload <vtable-entry-symbol> [#552 Shadow +768] [flags 0x10607 0x0 ]              [0x7fffd51109b0] bci=[-1,0,2302] rc=1 vc=56 vn=7 sti=- udi=- nc=1
      // n232n         iaload <vft-symbol> [#449 Shadow] [flags 0x18607 0x0 ] (X>=0 )                  [0x7fffd5110968] bci=[-1,0,2302] rc=1 vc=56 vn=6 sti=- udi=- nc=1 flg=0x100
      // n3n             ==>aload
      // n234n       aconst 0x7fd070 (methodPointerConstant )                                          [0x7fffd51109f8] bci=[0,0,314] rc=1 vc=56 vn=5 sti=- udi=- nc=0 flg=0x2000


      bool isGlobal;
      TR::VPConstraint* intersected = vp->getConstraint(lhsChild->getFirstChild()->getFirstChild(), isGlobal);

         intersected =
            (intersected) ?
               intersected->intersect(constraintFromMethodPointer, vp):
               intersected = constraintFromMethodPointer;

      if (intersected)
         {
            if (!branchOnEqual)
               {
               vp->addBlockConstraint(lhsChild->getFirstChild()->getFirstChild(), intersected);
               }
               else
               {
               //not sure about this case (is this the right way to add an edge constraint??
               vp->addEdgeConstraint(lhsChild->getFirstChild()->getFirstChild(), intersected, edgeConstraints);
               }
         }
      }
   else if (node->isProfiledGuard() && node->getOpCodeValue() == TR::ifacmpne)
       {
#ifdef J9_PROJECT_SPECIFIC
       TR_OpaqueClassBlock* objectClass =  lhs ? lhs->getClass() : NULL;
       TR_OpaqueClassBlock* rhsClass =  rhs ? rhs->getClass() : NULL;

       TR_VirtualGuard *vGuard = vp->comp()->findVirtualGuardInfo(node);
       TR::Node* callNode = getOriginalCallNode(vGuard, vp->comp(), node);


       if (vp->trace())
          {
          traceMsg(vp->comp(), "P2O: Considering a candidate at %p. Object Constraint : ", node);
          lhs->print(vp);
          traceMsg(vp->comp(), " , Guard constraint : ");
          rhs->print(vp);
          traceMsg(vp->comp(), "\n");
          }

       static const char* disableProfiled2Overridden = feGetEnv ("TR_DisableProfiled2Overridden");
       if (!disableProfiled2Overridden && callNode && vp->lastTimeThrough())
          {


          TR_OpaqueClassBlock* callClass = NULL;
          TR::MethodSymbol* interfaceMethodSymbol = NULL;

          //interfaces are weird we would create an UnresolvedClass Constraint and then wrap it in a Class constraint
          if (rhs && !objectClass &&
                lhs && lhs->getClassType() &&
                (interfaceMethodSymbol = callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()) &&
                interfaceMethodSymbol->isInterface())
             {
             int32_t len;
             TR_ResolvedMethod* owningMethod = callNode->getSymbolReference()->getOwningMethod(vp->comp());
             const char * sig  = lhs->getClassType()->getClassSignature(len);
             objectClass = vp->comp()->fe()->getClassFromSignature(sig, len, owningMethod, true);

             //The object's class can sometimes be less specific than the class of the call
             //if we ask for a rammethod in such a class we would trigger an assert ("no ram method in the vft slot")
             //to avoid this we would to insert another isInstanceOf (objectClass, callClass)
             TR_Method* interfaceMethod = interfaceMethodSymbol->getMethod();
             //re-using len and sig for getting a class of a call
             len = interfaceMethod->classNameLength();
             sig = classNameToSignature(interfaceMethod->classNameChars(), len, vp->comp());
             callClass = vp->comp()->fe()->getClassFromSignature(sig, len, owningMethod, true);
             }

          if (!callClass && !callNode->getSymbolReference()->isUnresolved())
             {
             TR::ResolvedMethodSymbol* resolvedCallMethodSymbol = callNode->getSymbolReference()->getSymbol()->getResolvedMethodSymbol();
             if (resolvedCallMethodSymbol)
                {
                TR_ResolvedMethod *resolvedCallMethod = resolvedCallMethodSymbol->getResolvedMethod();
                callClass = resolvedCallMethod->containingClass();
                }
             }

          //if all the conditions below are satisfied, especially the last one, we can cut some corners and
          //use findSingleImplementer to get a concrete implementation
          //regardless of the type(e.g. virtual vs interface)
          //of a call and the type of a receiver (e.g. abstract, interface, normal classes)
          //if findSingleImplementer returns a method than it must be the implementation Inliner used since
          //it would be the only one available
          if (objectClass && rhsClass && callClass && !lhs->isFixedClass() &&
              vp->comp()->fe()->isInstanceOf (rhsClass, objectClass, true, true, true) == TR_yes &&
              vp->comp()->fe()->isInstanceOf (objectClass, callClass, true, true, true) == TR_yes) /*the object class may be less specific than callClass*/
             {

             TR::SymbolReference* symRef = callNode->getSymbolReference();
             TR::MethodSymbol* methodSymbol = symRef->getSymbol()->castToMethodSymbol();


             if (methodSymbol && !(!methodSymbol->isInterface() && TR::Compiler->cls.isInterfaceClass(vp->comp(), objectClass)))
                {

                int32_t cpIndexOrVftSlot = TR::Compiler->cls.isInterfaceClass(vp->comp(), objectClass) ? symRef->getCPIndex() : symRef->getOffset();

                TR_YesNoMaybe useGetResolvedInterfaceMethod = methodSymbol->isInterface() ? TR_yes : TR_no;
                TR::MethodSymbol::Kinds methodKind = methodSymbol->isInterface() ? TR::MethodSymbol::Interface : TR::MethodSymbol::Virtual;

                TR_ResolvedMethod* rvm = findSingleImplementer (objectClass, cpIndexOrVftSlot, symRef->getOwningMethod(vp->comp()), vp->comp(), false, useGetResolvedInterfaceMethod);

        TR_ScratchList<TR_PersistentClassInfo> subClasses(vp->comp()->trMemory());

        //in AOT or when CHOpts are disabled findClassInfoAfterLocking returns NULL unconditionally
        //we should treat this case conservatively meaning we assume that there are more than two concrete classes in the given hierarchy
        //most likely rvm will also be NULL in this case, but it is better to not rely on this implicit connection between different
        //CH queries.
        TR_PersistentClassInfo *objectClassInfo = vp->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(objectClass, vp->comp());

        bool searchSucceeded = false;

        if (objectClassInfo)
          {
          TR_ClassQueries::collectAllSubClasses(objectClassInfo, &subClasses, vp->comp());
          subClasses.add(objectClassInfo);
          searchSucceeded = true;
          }

      /*
        the limited version of a transformation is enabled
        Namely, we only do a transformation if there is no more than one instantiable class in the hierarchy
        starting from objectClass. If there is more than one concrete class in hierarchy
        overriding methods in objectClass and if any call was devirtualized by invariantargumentpreexisence downstream
        we will produce the functionally incorrect code by swapping a profiled test with a method test as
        the method test is insufficient for the devirtualizations done by invariantargumentpreexisence
      */

      if (searchSucceeded && numConcreteClasses(&subClasses) < 2 && rvm)
                   {
                   TR::ResolvedMethodSymbol* cMethodSymbol = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(
                   symRef->getOwningMethodIndex(), -1, rvm, methodKind)->getSymbol()->castToResolvedMethodSymbol();

                   TR_VirtualGuardKind guardKind =
                      methodSymbol->isInterface()  ? TR_InterfaceGuard :
                      TR::Compiler->cls.isAbstractClass(vp->comp(), objectClass) ? TR_AbstractGuard : TR_HierarchyGuard;

                   addDelayedConvertedGuard(node, callNode, cMethodSymbol, vGuard, vp, guardKind, objectClass);
                   }
                }
             }
          }
#endif
       }


   return node;
   }

TR::Node *constrainIfcmpeq(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainIfcmpeqne(vp, node, true);
   }

TR::Node *constrainIfcmpne(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainIfcmpeqne(vp, node, false);
   }



void getLimits(OMR::ValuePropagation *vp, int32_t *zLow, int32_t *zHigh, TR::Node *symNode, bool isGlobal)
   {
         TR::VPConstraint *zConstraint = vp->getConstraint(symNode, isGlobal);

         *zLow = TR::getMinSigned<TR::Int32>();
         *zHigh = TR::getMaxSigned<TR::Int32>();

         if (zConstraint)
            {
            TR::VPIntRange *zRange = zConstraint->asIntRange();
            if (zRange)
               {
               *zLow = zRange->getLow();
               *zHigh = zRange->getHigh();
               }
            }
   }

void getLimits(OMR::ValuePropagation *vp, int64_t *zLow, int64_t *zHigh, TR::Node *symNode, bool isGlobal)
   {
         TR::VPConstraint *zConstraint = vp->getConstraint(symNode, isGlobal);

         *zLow = TR::getMinSigned<TR::Int64>();
         *zHigh = TR::getMaxSigned<TR::Int64>();

         if (zConstraint)
            {
            TR::VPLongRange *zRange = zConstraint->asLongRange();
            if (zRange)
               {
               *zLow = zRange->getLowLong();
               *zHigh = zRange->getHighLong();
               }
            }
   }

void getConstValue(int32_t *cConst, TR::Node *constNode)
   {
      *cConst = constNode->getInt();
   }

void getConstValue(int64_t *cConst, TR::Node *constNode)
   {
      *cConst = constNode->getLongInt();
   }

void getExtremes(int32_t *lowEnd, int32_t *highEnd)
   {
      *lowEnd = TR::getMinSigned<TR::Int32>();
      *highEnd = TR::getMaxSigned<TR::Int32>();
   }

void getExtremes(int64_t *lowEnd, int64_t *highEnd)
   {
      *lowEnd = TR::getMinSigned<TR::Int64>();
      *highEnd = TR::getMaxSigned<TR::Int64>();
   }

TR::Node *makeNewRhsNode(OMR::ValuePropagation *vp, TR::Node *node, TR::Node *symNodeY, int32_t constDiff)
   {
      TR::Node *newBaNode = TR::Node::create(node, TR::iconst, 0, constDiff);
      TR::Node *newRhsNode = TR::Node::create(TR::isub, 2, symNodeY, newBaNode);
      return newRhsNode;
   }

TR::Node *makeNewRhsNode(OMR::ValuePropagation *vp, TR::Node *node, TR::Node *symNodeY, int64_t constDiff)
   {
      TR::Node *newBaNode = TR::Node::create(node, TR::lconst, 0, 0);
      newBaNode->setLongInt(constDiff);
      TR::Node *newRhsNode = TR::Node::create(TR::lsub, 2, symNodeY, newBaNode);
      return newRhsNode;
   }


// Decide and execute simplification:  x + a < y + b  changes into  x < y + (b - a) .
//
template <class IntType>
static TR::Node *simplifyInequality(OMR::ValuePropagation *vp, TR::Node *node, TR::Node *lhsChild, TR::Node *rhsChild, bool isGlobal, bool childrenReversed)
   {

   // Pattern match * + a, * + b
   // (+ was converted to - in a previous optimization)
   //
   if ((lhsChild->getOpCode().isAdd() || lhsChild->getOpCode().isSub()) &&
       (rhsChild->getOpCode().isAdd() || rhsChild->getOpCode().isSub()) &&
       lhsChild->getSecondChild()->getOpCode().isLoadConst() &&
       rhsChild->getSecondChild()->getOpCode().isLoadConst())
      {

      // Get ranges and constants
      //
      IntType xLow, xHigh;
      IntType yLow, yHigh;

      getLimits (vp, &xLow, &xHigh, lhsChild->getFirstChild(), isGlobal);
      getLimits (vp, &yLow, &yHigh, rhsChild->getFirstChild(), isGlobal);

      IntType aConst;
      IntType bConst;
      IntType cConst;

      getConstValue (&aConst, lhsChild->getSecondChild());
      getConstValue (&bConst, rhsChild->getSecondChild());

      if (lhsChild->getOpCode().isSub())
      aConst = - aConst;
      if (rhsChild->getOpCode().isSub())
      bConst = - bConst;

      IntType low, high;

      getExtremes (&low, &high);



      // Compute overflow conditions
      //
      if ( // no overflow in x + a
           (aConst > 0 ? xHigh <= high - aConst : xLow >= low - aConst) &&
           // no overflow in y + b
           (bConst > 0 ? yHigh <= high - bConst : yLow >= low - bConst) &&
           // no overflow in b - a
           (aConst < 0 ? bConst <= high + aConst : bConst >= low + aConst) &&
           // no overflow in y + (b - a)
           (aConst > bConst ? yLow >= low + aConst - bConst : yHigh <= high - bConst + aConst))
         {


         // Execute optimization
         //
         TR::Node *newLhsNode = lhsChild->getFirstChild();

         cConst = aConst - bConst;

         TR::Node *newRhsNode = makeNewRhsNode(vp, node, rhsChild->getFirstChild(), cConst);

         if (childrenReversed)
            {
            node->setAndIncChild(0, newRhsNode);
            node->setAndIncChild(1, newLhsNode);
            }
         else
            {
            node->setAndIncChild(0, newLhsNode);
            node->setAndIncChild(1, newRhsNode);
            }

         rhsChild->recursivelyDecReferenceCount();
         lhsChild->recursivelyDecReferenceCount();
         lhsChild = newLhsNode;
         rhsChild = newRhsNode;

         // A newly-created node may have a value number larger than the limit (e.g. 200,000 for WCode).
         // Values larger than that limit are used for other purposes, so there can't be any overlap. It's not safe
         // to call constrainChildren on a newly-created node
         //constrainChildren(vp, node);

         }
      }
   return node;
   }

static int64_t clippedDifference(int64_t left, int64_t right)
   {
   TR_ASSERT(left >= right, "clippedDifference expects left >= right");

   // If left is positive and right is negative, left-right could overflow, and
   // what should be a large positive difference could end up negative.  This
   // function's purpose is to returns a value X with the property that the
   // expression "X > Y" always returns the same result as the ideal un-bounded
   // mathematical expression "(left-right) > Y" would have returned,
   // for Y within the range TR::getMinSigned<TR::Int32>() <= Y <= -TR::getMinSigned<TR::Int32>(). (Note: -TR::getMinSigned<TR::Int32>() = TR::getMaxSigned<TR::Int32>()+1)
   //
   // Because left >= right, it is impossible for (left-right) to be negative,
   // so a negative difference unambiguously indicates an overflow, and we
   // instead return TR::getMaxSigned<TR::Int64>() to meet the above requirement.
   //
   // (Note that overflow in the negative direction can't occur when left >= right.)

   if (left - right < 0)
      return TR::getMaxSigned<TR::Int64>();
   else
      return left - right;
   }

// Handles ificmplt, ificmple, ificmpgt, ificmpge
//
static TR::Node *constrainIfcmplessthan(OMR::ValuePropagation *vp, TR::Node *node, TR::Node *lhsChild, TR::Node *rhsChild, bool orEqual)
   {
   bool isUnsigned = node->getOpCode().isUnsignedCompare();

   // Note that if the comparison is gt or ge, the children have been swapped so
   // that we can do a lt or le comparison
   //
   bool childrenReversed = (rhsChild == node->getFirstChild()); // MUST be before constrainChildren
   constrainChildren(vp, node);

   // Find the output edge from the current block that corresponds to this
   // branch
   //
   TR::Block *target = node->getBranchDestination()->getNode()->getBlock();
   TR::Block *fallThrough = vp->_curBlock->getNextBlock();
   // check for target and fall-through edges being the same
   if (fallThrough == target)
      return node;

   // it's possible the children have been updated.  Reset rhs & lhs in case
   if(childrenReversed)
      {
      lhsChild = node->getSecondChild();
      rhsChild = node->getFirstChild();
      }
   else
      {
      lhsChild = node->getFirstChild();
      rhsChild = node->getSecondChild();
      }

   TR::CFGEdge *edge = vp->findOutEdge(vp->_curBlock->getSuccessors(), target);

   bool isGlobal;
   TR::VPConstraint *lhs = NULL;
   TR::VPConstraint *rhs = NULL;
   TR::VPConstraint *rel = NULL;

   bool cannotBranch      = false;
   bool cannotFallThrough = false;

   // Quick check for the same value number
   //
   if (vp->getValueNumber(lhsChild) == vp->getValueNumber(rhsChild))
      {
      if (orEqual)
         cannotFallThrough = true;
      else
         cannotBranch = true;
      }

   // See if there are absolute constraints on the children
   //
   else
      {
      lhs = vp->getConstraint(lhsChild, isGlobal);
      rhs = vp->getConstraint(rhsChild, isGlobal);

      if (lhsChild->getOpCode().isLong() && !isUnsigned)
         {
         simplifyInequality<int64_t> (vp, node, lhsChild, rhsChild, isGlobal, childrenReversed);
         }
      else if (!isUnsigned)
         {
         simplifyInequality<int32_t> (vp, node, lhsChild, rhsChild, isGlobal, childrenReversed);
         }

      if (lhs && rhs && !isUnsigned)
         {
         if (orEqual)
            {
            if (lhs->mustBeLessThanOrEqual(rhs, vp))
               cannotFallThrough = true;
            else if (rhs->mustBeLessThan(lhs, vp))
               cannotBranch = true;
            }
         else
            {
            if (lhs->mustBeLessThan(rhs, vp))
               cannotFallThrough = true;
            else if (rhs->mustBeLessThanOrEqual(lhs, vp))
               cannotBranch = true;
            }
         }
      }

   // See if there are relative constraints on the children
   //
   if (!cannotBranch && !cannotFallThrough && !isUnsigned)
      {
      rel = vp->getConstraint(lhsChild, isGlobal, rhsChild);
      if (rel)
         {
         if (orEqual)
            {
            if (rel->mustBeLessThanOrEqual())
               cannotFallThrough = true;
            else if (rel->mustBeGreaterThan())
               cannotBranch = true;
            }
         else
            {
            if (rel->mustBeLessThan())
               cannotFallThrough = true;
            else if (rel->mustBeGreaterThanOrEqual())
               cannotBranch = true;
            }

         if (rel->asEqual() && !cannotFallThrough && !cannotBranch && (lhs || rhs))
            {
            // If we know lhs == rhs+k, and rhs+k can't overflow, we can tell which way the comparison will go.
            // Likewise, since lhs-k == rhs, if we know lhs-k can't overflow, we're also good to go.

            TR::VPEqual *relation = rel->asEqual();
            int32_t increment = relation->increment();

            int64_t maxIncrement = 0; // If absIncrement is more than this, overflow may occur, and (for example) a==b+k does not imply a>b
            int64_t absIncrement = increment;
            if (absIncrement < 0)
               {
               absIncrement = -absIncrement;
               if (rhs)
                  {
                  if (rhsChild->getOpCode().isLong())
                     maxIncrement = clippedDifference(rhs->getLowLong(), TR::getMinSigned<TR::Int64>());
                  else
                     maxIncrement = clippedDifference(rhs->getLowInt(),  TR::getMinSigned<TR::Int32>());
                  }
               else
                  {
                  if (lhsChild->getOpCode().isLong())
                     maxIncrement = clippedDifference(TR::getMaxSigned<TR::Int64>(), lhs->getHighLong());
                  else
                     maxIncrement = clippedDifference(TR::getMaxSigned<TR::Int32>(),  lhs->getHighInt());
                  }
               }
            else
               {
               if (rhs)
                  {
                  if (rhsChild->getOpCode().isLong())
                     maxIncrement = clippedDifference(TR::getMaxSigned<TR::Int64>(), rhs->getHighLong());
                  else
                     maxIncrement = clippedDifference(TR::getMaxSigned<TR::Int32>(),  rhs->getHighInt());
                  }
               else
                  {
                  if (lhsChild->getOpCode().isLong())
                     maxIncrement = clippedDifference(lhs->getLowLong(), TR::getMinSigned<TR::Int64>());
                  else
                     maxIncrement = clippedDifference(lhs->getLowInt(),  TR::getMinSigned<TR::Int32>());
                  }
               }

            if (vp->trace())
               {
               traceMsg(vp->comp(), "   Conditional relation check on %s [%p]: increment=%d, absIncrement=" INT64_PRINTF_FORMAT ", maxIncrement=" INT64_PRINTF_FORMAT ", orEqual=%s\n",
                  node->getOpCode().getName(), node, increment, absIncrement, maxIncrement, orEqual? "true" : "false");
               }

            // At this point we have: TR::getMinSigned<TR::Int32>() <= absIncrement <= -TR::getMinSigned<TR::Int32>(), and
            // maxIncrement comes from clippedDifference, so by the definition
            // of clippedDifference, we can compare them with the expected results.
            //
            if (absIncrement <= maxIncrement)
               {
               // We're in luck.  rhs + increment can't overflow, so now we can
               // tell how the comparison will go.

               if (orEqual)
                  {
                  if (increment <= 0)
                     cannotFallThrough = true;
                  else
                     cannotBranch = true;
                  }
               else
                  {
                  if (increment < 0)
                     cannotFallThrough = true;
                  else
                     cannotBranch = true;
                  }
               }

            if (cannotBranch || cannotFallThrough)
               if (!performTransformation(vp->comp(), "%sSuccessful conditional relation check on %s [%p]\n", OPT_DETAILS, node->getOpCode().getName(), node))
                  {
                  cannotBranch      = false;
                  cannotFallThrough = false;
                  }
            }
         }
      }

   if (cannotBranch &&
       performTransformation(vp->comp(), "%sRemoving conditional branch [%p] %s\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      removeConditionalBranch(vp, node, edge);
      return node;
      }

   if (cannotFallThrough &&
       performTransformation(vp->comp(), "%sChanging node [%p] %s into goto\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      vp->printEdgeConstraints(vp->createEdgeConstraints(edge, false));
      changeConditionalToGoto(vp, node, edge);
      return node;
      }

   // Propagate current constraints to the branch target
   //
   if (vp->trace())
      traceMsg(vp->comp(), "   Conditional branch\n");
   OMR::ValuePropagation::EdgeConstraints *edgeConstraints = vp->createEdgeConstraints(edge, true);

   // Find extra constraints to apply to the two edges
   //
   int32_t adjustment = orEqual ? 0 : 1;
   int32_t intBoundary;
   int64_t longBoundary;

   int32_t minIntValue = /*isUnsigned ? TR::getMinUnsigned<TR::Int32>() :*/ TR::getMinSigned<TR::Int32>();
   int32_t maxIntValue = /*isUnsigned ? TR::getMaxUnsigned<TR::Int32>() :*/ TR::getMaxSigned<TR::Int32>();
   int64_t minLongValue = isUnsigned ? TR::getMinUnsigned<TR::Int64>() : TR::getMinSigned<TR::Int64>();

   if (lhsChild->getOpCode().isLong())
      {
      // Create extra constraints for lhs and rhs on the branch edge
      //
      if (lhs)
         longBoundary = lhs->getLowLong();
      else
         longBoundary = TR::getMinSigned<TR::Int64>();
      longBoundary += adjustment;
      if (longBoundary != TR::getMinSigned<TR::Int64>() && !isUnsigned)
         {
         if (!vp->addEdgeConstraint(rhsChild, TR::VPLongRange::create(vp, longBoundary, TR::getMaxSigned<TR::Int64>()), edgeConstraints))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }

      if (rhs)
         longBoundary = rhs->getHighLong();
      else
         longBoundary = TR::getMaxSigned<TR::Int64>();
      longBoundary -= adjustment;
      if (isUnsigned && longBoundary < 0)
         longBoundary = TR::getMaxSigned<TR::Int64>();
      if (longBoundary != TR::getMaxSigned<TR::Int64>())
         {
         if (!vp->addEdgeConstraint(lhsChild, TR::VPLongRange::create(vp, minLongValue, longBoundary), edgeConstraints))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }
   else if (!isUnsigned)
      {
      // Create extra constraints for lhs and rhs on the branch edge
      //
      if (lhs)
         intBoundary = lhs->getLowInt();
      else
         intBoundary = minIntValue;
      intBoundary += adjustment;
      bool modifiedBoundary = /*isUnsigned ? ((uint32_t)intBoundary != TR::getMinUnsigned<TR::Int32>())
                                :*/ (intBoundary != TR::getMinSigned<TR::Int32>());
      //if (intBoundary != TR::getMinSigned<TR::Int32>())
      if (modifiedBoundary)
         {
         TR::VPConstraint *constraint = TR::VPIntRange::create(vp, intBoundary, maxIntValue/*, isUnsigned*/);
         //if (!vp->addEdgeConstraint(rhsChild, TR::VPIntRange::create(vp, intBoundary, TR::getMaxSigned<TR::Int32>()), edgeConstraints))
         if (!vp->addEdgeConstraint(rhsChild, constraint, edgeConstraints))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }

      if (rhs)
         intBoundary = rhs->getHighInt();
      else
         intBoundary = maxIntValue;
      intBoundary -= adjustment;
      modifiedBoundary = /*isUnsigned ? ((uint32_t)intBoundary != TR::getMaxUnsigned<TR::Int32>())
                           :*/ (intBoundary != TR::getMaxSigned<TR::Int32>());
      //if (intBoundary != TR::getMaxSigned<TR::Int32>())
      if (modifiedBoundary)
         {
         TR::VPConstraint *constraint = TR::VPIntRange::create(vp, minIntValue, intBoundary/*, isUnsigned*/);
         //if (!vp->addEdgeConstraint(lhsChild, TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), intBoundary), edgeConstraints))
         if (!vp->addEdgeConstraint(lhsChild, constraint, edgeConstraints))
            {
            if (!vp->intersectionFailed())
               cannotBranch = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }

   // Apply relative constraint to the branch edge
   //
   //TR::VPLessThanOrEqual *newConstraint = TR::VPLessThanOrEqual::create(vp, -adjustment);
   //if (newConstraint->increment() != 0)
   //   newConstraint->setHasArtificialIncrement();
   //if (!vp->addEdgeConstraint(lhsChild, newConstraint, edgeConstraints, rhsChild))
   //   cannotBranch = true;

   if (vp->trace() && !cannotBranch)
      vp->printEdgeConstraints(edgeConstraints);

   if (lhsChild->getOpCode().isLong() && !isUnsigned)
      {
      // Create extra constraints for lhs and rhs on the fall through edge
      //
      if (lhs)
         longBoundary = lhs->getHighLong();
      else
         longBoundary = TR::getMaxSigned<TR::Int64>();
      longBoundary += adjustment-1;
      if (longBoundary != TR::getMaxSigned<TR::Int64>())
         {
         if (!vp->addBlockConstraint(rhsChild, TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), longBoundary), NULL, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }

      if (rhs)
         longBoundary = rhs->getLowLong();
      else
         longBoundary = TR::getMinSigned<TR::Int64>();
      longBoundary += 1-adjustment;
      if (longBoundary != TR::getMinSigned<TR::Int64>())
         {
         if (!vp->addBlockConstraint(lhsChild, TR::VPLongRange::create(vp, longBoundary, TR::getMaxSigned<TR::Int64>()), NULL, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }
   else if (!isUnsigned)
      {
      // Create extra constraints for lhs and rhs on the fall through edge
      //
      if (lhs)
         intBoundary = lhs->getHighInt();
      else
         intBoundary = maxIntValue;
      intBoundary += adjustment-1;
      bool modifiedBoundary = /*isUnsigned ? ((uint32_t)intBoundary != TR::getMaxUnsigned<TR::Int32>())
                                           :*/ (intBoundary != TR::getMaxSigned<TR::Int32>());
      //if (intBoundary != TR::getMaxSigned<TR::Int32>())
      if (modifiedBoundary)
         {
         TR::VPConstraint *constraint = TR::VPIntRange::create(vp, minIntValue, intBoundary/*, isUnsigned*/);
         //if (!vp->addBlockConstraint(rhsChild, TR::VPIntRange::create(vp, TR::getMinSigned<TR::Int32>(), intBoundary), NULL, false))
         if (!vp->addBlockConstraint(rhsChild, constraint, NULL, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }

      if (rhs)
         intBoundary = rhs->getLowInt();
      else
         intBoundary = minIntValue;
      intBoundary += 1-adjustment;
      //if (intBoundary != TR::getMinSigned<TR::Int32>())
      modifiedBoundary = /*isUnsigned ? ((uint32_t)intBoundary != TR::getMinUnsigned<TR::Int32>())
                           :*/ (intBoundary != TR::getMinSigned<TR::Int32>());
      if (modifiedBoundary)
         {
         TR::VPConstraint *constraint = TR::VPIntRange::create(vp, intBoundary, maxIntValue/*, isUnsigned*/);
         //if (!vp->addBlockConstraint(lhsChild, TR::VPIntRange::create(vp, intBoundary, TR::getMaxSigned<TR::Int32>()), NULL, false))
         if (!vp->addBlockConstraint(lhsChild, constraint, NULL, false))
            {
            if (!vp->intersectionFailed())
               cannotFallThrough = true;
            else
               vp->setIntersectionFailed(false);
            }
         }
      }

   // Apply relative constraint to the fall through edge
   //
   // FIXME: Now disable the long case
   if (!lhsChild->getOpCode().isLong() && !rhsChild->getOpCode().isLong())
      {
      //TR::VPGreaterThanOrEqual *newConstraint = TR::VPGreaterThanOrEqual::create(vp, 1-adjustment);
      //if (newConstraint->increment() != 0)
      //   newConstraint->setHasArtificialIncrement();

      //if (!vp->addBlockConstraint(lhsChild, newConstraint, rhsChild, false))
      //   cannotFallThrough = true;
      }

   if (cannotBranch &&
       performTransformation(vp->comp(), "%sRemoving conditional branch [%p] %s\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      TR_ASSERT(!cannotFallThrough, "Cannot branch or fall through");
      removeConditionalBranch(vp, node, edge);
      }
   else if (cannotFallThrough &&
            performTransformation(vp->comp(), "%sChanging node [%p] %s into goto\n", OPT_DETAILS, node, node->getOpCode().getName()))
      {
      TR_ASSERT(!cannotBranch, "Cannot branch or fall through");
      changeConditionalToGoto(vp, node, edge);
      }
   return node;
   }

TR::Node *constrainIfcmplt(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainIfcmplessthan(vp, node, node->getFirstChild(), node->getSecondChild(), false);
   }

TR::Node *constrainIfcmple(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainIfcmplessthan(vp, node, node->getFirstChild(), node->getSecondChild(), true);
   }

TR::Node *constrainIfcmpgt(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainIfcmplessthan(vp, node, node->getSecondChild(), node->getFirstChild(), false);
   }

TR::Node *constrainIfcmpge(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainIfcmplessthan(vp, node, node->getSecondChild(), node->getFirstChild(), true);
   }

TR::Node *constrainCondBranch(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // TODO - handle the special cases for conditional branches:
   //    integer compares, long compares, ref compares
   //

   constrainChildren(vp, node);

   // Put the current list of block constraints on to the edge
   //
   TR::Block *target = node->getBranchDestination()->getNode()->getBlock();
   if (vp->trace())
      traceMsg(vp->comp(), "   Conditional branch\n");

   // Find the output edge from the current block that corresponds to this
   // branch
   //
   TR::CFGEdge *edge = vp->findOutEdge(vp->_curBlock->getSuccessors(), target);
   vp->printEdgeConstraints(vp->createEdgeConstraints(edge, true));
   return node;
   }

// Handles icmpeq, icmpne, lcmpeq, lcmpne, acmpeq, acmpne
//
static TR::Node *constrainCmpeqne(OMR::ValuePropagation *vp, TR::Node *node, bool testEqual)
   {
   constrainChildren(vp, node);

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(node->getFirstChild(), lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(node->getSecondChild(), rhsGlobal);
   lhsGlobal &= rhsGlobal;

   //bool isUnsigned = node->getType().isUnsignedInt();

   int32_t result = -1;
   if (lhs && rhs)
      {
      if (lhs->mustBeEqual(rhs, vp))
         result = testEqual ? 1 : 0;
      else if (lhs->mustBeNotEqual(rhs, vp))
         result = testEqual ? 0 : 1;
      }

   // If the result is known, replace the compare with the corresponding
   // constant
   //
   TR::VPConstraint *constraint = NULL;
   if (result >= 0)
      {
      if ((lhsGlobal || vp->lastTimeThrough()) &&
          performTransformation(vp->comp(), "%sChanging node [%p] %s into constant %d\n", OPT_DETAILS, node, node->getOpCode().getName(), result))
         {
         vp->removeChildren(node);
         TR::ILOpCodes op = /*isUnsigned ? TR::iuconst : */TR::iconst;
         TR::Node::recreate(node, op);
         node->setInt(result);
         vp->invalidateValueNumberInfo();
         return node;
         }
      constraint = TR::VPIntConst::create(vp, result/*, isUnsigned*/);
      }
   else
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();
      bool lhsIsBool = false;
      if (lhs && vp->lastTimeThrough())
         lhsIsBool = isBoolean(lhs);

      if (secondChild->getOpCode().isLoadConst() &&
          lhsIsBool && vp->getCurrentParent())
         {
         int64_t constValue;
         if ((node->getOpCodeValue() == TR::icmpeq) ||
             (node->getOpCodeValue() == TR::icmpne))
            constValue = (int64_t) secondChild->getInt();
         else
            constValue = secondChild->getLongInt();

         if (((constValue == 1) && ((node->getOpCodeValue() == TR::icmpeq) || (node->getOpCodeValue() == TR::lcmpeq))) ||
             ((constValue == 0) && ((node->getOpCodeValue() == TR::icmpne) || (node->getOpCodeValue() == TR::lcmpne))))
            {
            if (performTransformation(vp->comp(), "%sReduced identity operation on bool in node [" POINTER_PRINTF_FORMAT "] \n", OPT_DETAILS, node))
               {
               TR::Node *parent = vp->getCurrentParent();
               vp->invalidateValueNumberInfo();
               vp->invalidateUseDefInfo();

               int32_t i;
               for (i = parent->getNumChildren()-1; i >= 0; i--)
                  {
                  if (parent->getChild(i) == node)
                     break;
                  }

                TR::Node *newChild = firstChild;
                if (firstChild->getOpCode().isLong())
                   newChild = TR::Node::create(TR::l2i, 1, firstChild);

                parent->setAndIncChild(i, newChild);
                node->recursivelyDecReferenceCount();
               }
            }
         else if (((constValue == 1) && ((node->getOpCodeValue() == TR::icmpne) || (node->getOpCodeValue() == TR::lcmpne))) ||
                  ((constValue == 0) && ((node->getOpCodeValue() == TR::icmpeq) || (node->getOpCodeValue() == TR::lcmpeq))))
            {
            if (firstChild->getOpCodeValue() == node->getOpCodeValue())
              {
              TR::Node *firstGrandChild = firstChild->getFirstChild();
              TR::Node *secondGrandChild = firstChild->getSecondChild();

              bool b;
              TR::VPConstraint *firstGrand = vp->getConstraint(firstGrandChild, b);

              bool firstGrandIsBool = false;
              if (firstGrand && vp->lastTimeThrough())
                 firstGrandIsBool = isBoolean(firstGrand);


              if (secondGrandChild->getOpCode().isLoadConst() &&
                  firstGrandIsBool && vp->getCurrentParent())
                 {
                 int64_t firstGrandConstValue;
                 if ((firstChild->getOpCodeValue() == TR::icmpeq) ||
                     (firstChild->getOpCodeValue() == TR::icmpne))
                    firstGrandConstValue = (int64_t) secondGrandChild->getInt();
                 else
                    firstGrandConstValue = secondGrandChild->getLongInt();

                 if (constValue == firstGrandConstValue)
                    {
                    if (performTransformation(vp->comp(), "%sReduced 2 NOTs of bool in node [" POINTER_PRINTF_FORMAT "] \n", OPT_DETAILS, node))
                       {
                       TR::Node *parent = vp->getCurrentParent();
                       vp->invalidateValueNumberInfo();
                       vp->invalidateUseDefInfo();

                       int32_t i;
                       for (i = parent->getNumChildren()-1; i >= 0; i--)
                          {
                          if (parent->getChild(i) == node)
                             break;
                          }

                      TR::Node *newChild = firstGrandChild;
                      if (firstGrandChild->getOpCode().isLong())
                         newChild = TR::Node::create(TR::l2i, 1, firstGrandChild);

                       parent->setAndIncChild(i, newChild);
                       node->recursivelyDecReferenceCount();
                       }
                     }
                  }
               }
            }
         }

      constraint = TR::VPIntRange::create(vp, 0, 1/*, isUnsigned*/);
      }

   vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
   return node;
   }

TR::Node *constrainCmpeq(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainCmpeqne(vp, node, true);
   }

TR::Node *constrainCmpne(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainCmpeqne(vp, node, false);
   }

// Handles icmplt, icmple, icmpgt, icmpge
//
static TR::Node *constrainCmplessthan(OMR::ValuePropagation *vp, TR::Node *node, TR::Node *lhsChild, TR::Node *rhsChild, bool orEqual)
   {
   TR_ASSERT(!node->getOpCode().isUnsignedCompare(), "This code doesn't handle logical comparisons, but it should be easy to fix it.");

   // Note that if the comparison is gt or ge, the children have been swapped so
   // that we can do a lt or le comparison
   //
   bool childrenReversed = (rhsChild == node->getFirstChild());// MUST be before constainChildren

   constrainChildren(vp, node);

   // it's possible the children have been updated.  Reset rhs & lhs in case
   if(childrenReversed)
      {
      lhsChild = node->getSecondChild();
      rhsChild = node->getFirstChild();
      }
   else
      {
      lhsChild = node->getFirstChild();
      rhsChild = node->getSecondChild();
      }

   bool lhsGlobal, rhsGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(lhsChild, lhsGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(rhsChild, rhsGlobal);
   lhsGlobal &= rhsGlobal;

   //bool isUnsigned = node->getType().isUnsignedInt();

   int32_t result = -1;
   if (lhs && rhs)
      {
      if (orEqual)
         {
         if (lhs->mustBeLessThanOrEqual(rhs, vp))
            result = 1;
         else if (rhs->mustBeLessThan(lhs, vp))
            result = 0;
         }
      else
         {
         if (lhs->mustBeLessThan(rhs, vp))
            result = 1;
         else if (rhs->mustBeLessThanOrEqual(lhs, vp))
            result = 0;
         }
      }

   // If the result is known, replace the compare with the corresponding
   // constant
   //
   TR::VPConstraint *constraint = NULL;
   if (result >= 0)
      {
      if ((lhsGlobal || vp->lastTimeThrough()) &&
          performTransformation(vp->comp(), "%sChanging node [%p] %s into constant %d\n", OPT_DETAILS, node, node->getOpCode().getName(), result))
         {
         vp->removeChildren(node);
         TR::ILOpCodes op = /*isUnsigned ? TR::iuconst : */TR::iconst;
         TR::Node::recreate(node, op);
         node->setInt(result);
         vp->invalidateValueNumberInfo();
         return node;
         }
      constraint = TR::VPIntConst::create(vp, result/*, isUnsigned*/);
      }
   else
      constraint = TR::VPIntRange::create(vp, 0, 1/*, isUnsigned*/);

   vp->addBlockOrGlobalConstraint(node, constraint ,lhsGlobal);
   return node;
   }

TR::Node *constrainCmplt(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainCmplessthan(vp, node, node->getFirstChild(), node->getSecondChild(), false);
   }

TR::Node *constrainCmple(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainCmplessthan(vp, node, node->getFirstChild(), node->getSecondChild(), true);
   }

TR::Node *constrainCmpgt(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainCmplessthan(vp, node, node->getSecondChild(), node->getFirstChild(), false);
   }

TR::Node *constrainCmpge(OMR::ValuePropagation *vp, TR::Node *node)
   {
   return constrainCmplessthan(vp, node, node->getSecondChild(), node->getFirstChild(), true);
   }

TR::Node *constrainCmp(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);
   vp->addGlobalConstraint(node, TR::VPIntRange::create(vp, 0, 1));
   return node;
   }

TR::Node *constrainFloatCmp(OMR::ValuePropagation *vp, TR::Node *node)
   {
   vp->addGlobalConstraint(node, TR::VPIntRange::create(vp, -1, 1));
   return node;
   }

TR::Node *constrainSwitch(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // Process the switch expression before the switch cases
   //
   TR::Node *myParent = vp->getCurrentParent();
   vp->setCurrentParent(node);
   vp->launchNode(node->getFirstChild(), node, 0);
   vp->setCurrentParent(myParent);

   constrainChildren(vp, node);

   // if something is known about the selector, some
   // of the cases can be removed
   //
   // FIXME: process only lookupswitches for now
   //
   if (node->getOpCodeValue() != TR::table)
      {
      TR::Node *selector = node->getFirstChild();
      bool isGlobal;
      TR::VPConstraint *constraint = vp->getConstraint(selector, isGlobal);
      bool isInt64 = selector->getType().isInt64();
      int64_t maxCase=LONG_MIN;
      int64_t minCase=LONG_MAX;


      if (!isInt64 && constraint && constraint->asIntConstraint())
         {
         //bool isUnsigned = selector->getType().isUnsignedInt();
         int32_t low = constraint->asIntConstraint()->getLow();
         int32_t high = constraint->asIntConstraint()->getHigh();
         minCase=INT_MAX;
         maxCase=INT_MIN;
         bool casesRemoved = false;
         int32_t i;
         for (i = node->getNumChildren()-1; i > 1; i--)
            {
            int32_t value = node->getChild(i)->getCaseConstant();
            if (/*!isUnsigned &&*/ ((low <  value && high < value) ||
                                (low > value && high > value)))
               {
               TR::Block *target = node->getChild(i)->getBranchDestination()->getNode()->getBlock();
               if (vp->trace())
                  traceMsg(vp->comp(), "   Case %d (target %d) is unreachable\n", value, target->getNumber());
               node->removeChild(i);
               casesRemoved = true;
               }
            else
              {
              if(minCase > value)
                minCase = value;
              if(maxCase < value)
                maxCase = value;
              }
            }

         // remove the edges and propagate paths as unreachable
         //
         if (casesRemoved)
            {
            TR_ScratchList<TR::Block> remainingTargets(vp->trMemory());
            for (i = node->getNumChildren()-1; i > 1; i--)
               {
               TR::Block *target = node->getChild(i)->getBranchDestination()->getNode()->getBlock();
               remainingTargets.add(target);
               }

            TR::Block *defaultTarget = node->getChild(1)->getBranchDestination()->getNode()->getBlock();
            TR::CFGEdge *defaultEdge = vp->findOutEdge(vp->_curBlock->getSuccessors(), defaultTarget);
            for (auto e = vp->_curBlock->getSuccessors().begin(); e != vp->_curBlock->getSuccessors().end(); ++e)
               {
               if (((*e) == defaultEdge) ||
                     remainingTargets.find(toBlock((*e)->getTo())))
                  continue;
               vp->setUnreachablePath(*e);
               vp->_edgesToBeRemoved->add(*e);
               }
            }
         if(low >= minCase && high <= maxCase)
           node->setCannotOverflow(true);  // Let evaluator know value range of selector is covered by min and max case
         } // if IntConstraint
      else if (isInt64 && constraint && constraint->asLongConstraint())
         {
         //bool isUnsigned = selector->getType().isUnsignedInt();
         int64_t low = constraint->asLongConstraint()->getLow();
         int64_t high = constraint->asLongConstraint()->getHigh();
         bool casesRemoved = false;
         int32_t i;
         for (i = node->getNumChildren()-1; i > 1; i--)
            {
            int64_t value = node->getChild(i)->getCaseConstant();
            if (/*!isUnsigned &&*/ ((low <  value && high < value) ||
                                (low > value && high > value)))
               {
               TR::Block *target = node->getChild(i)->getBranchDestination()->getNode()->getBlock();
               if (vp->trace())
                  traceMsg(vp->comp(), "   Case %d (target %d) is unreachable\n", value, target->getNumber());
               node->removeChild(i);
               casesRemoved = true;
               }
            else
              {
              if(minCase > value)
                minCase = value;
              if(maxCase < value)
                maxCase = value;
              }
            }

         // remove the edges and propagate paths as unreachable
         //
         if (casesRemoved)
            {
            TR_ScratchList<TR::Block> remainingTargets(vp->trMemory());
            for (i = node->getNumChildren()-1; i > 1; i--)
               {
               TR::Block *target = node->getChild(i)->getBranchDestination()->getNode()->getBlock();
               remainingTargets.add(target);
               }

            TR::Block *defaultTarget = node->getChild(1)->getBranchDestination()->getNode()->getBlock();
            TR::CFGEdge *defaultEdge = vp->findOutEdge(vp->_curBlock->getSuccessors(), defaultTarget);
            for (auto e = vp->_curBlock->getSuccessors().begin(); e != vp->_curBlock->getSuccessors().end(); ++e)
               {
               if (((*e)== defaultEdge) ||
                     remainingTargets.find(toBlock((*e)->getTo())))
                  continue;
               vp->setUnreachablePath(*e);
               vp->_edgesToBeRemoved->add(*e);
               }
            }
         if(low >= minCase && high <= maxCase)
           node->setCannotOverflow(true);  // Let evaluator know value range of selector is covered by min and max case
         } // ... else if LongConstraint
      }

   // Fall-through is now unreachable
   //
   vp->setUnreachablePath();

   return node;
   }

TR::Node *constrainCase(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // Put the current list of block constraints on to the edge
   //
   TR::Block *target = node->getBranchDestination()->getNode()->getBlock();
   if (vp->trace())
      traceMsg(vp->comp(), "   Switch case branch\n");

   // Find the output edge from the current block that corresponds to this
   // branch
   //
   TR::CFGEdge *edge = vp->findOutEdge(vp->_curBlock->getSuccessors(), target);
   vp->printEdgeConstraints(vp->createEdgeConstraints(edge, true));
   return node;
   }

static void constrainClassObjectLoadaddr(
   OMR::ValuePropagation *vp,
   TR::Node *node,
   bool isGlobal)
   {
   TR_ASSERT(
      node->getOpCodeValue() == TR::loadaddr,
      "constrainClassObjectLoadaddr: n%un %s is not a loadaddr\n",
      node->getGlobalIndex(),
      node->getOpCode().getName());

   TR::SymbolReference *symRef = node->getSymbolReference();

   TR_ASSERT(
      symRef->getSymbol()->isClassObject(),
      "constrainClassObjectLoadaddr: n%un loadaddr is not for a class\n",
      node->getGlobalIndex());

   TR::VPConstraint *constraint = TR::VPClass::create(
      vp,
      TR::VPClassType::create(vp, symRef, true),
      TR::VPNonNullObject::create(vp),
      NULL,
      NULL,
      TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject));

   vp->addBlockOrGlobalConstraint(node, constraint, isGlobal);
   }

TR::Node *constrainLoadaddr(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // If this is loading the address of a class object or the address of a
   // pointer to a class object, the type is the class.
   //
   TR::VPConstraint *constraint = NULL;
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol           *symbol = symRef->getSymbol();
   if (symbol->isAddressOfClassObject())
      {
      //add a ClassObject property
      TR::VPConstraint *constraint = TR::VPClass::create(vp, TR::VPClassType::create(vp, symRef, false, true),
                                                         NULL, NULL, NULL,
                                                         TR::VPObjectLocation::create(vp, TR::VPObjectLocation::J9ClassObject));
      vp->addGlobalConstraint(node, constraint);
      //vp->addGlobalConstraint(node, TR::VPClassType::create(vp, symRef, false, true));
      //
      vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
      }
   else if (symbol->isClassObject())
      {
      constrainClassObjectLoadaddr(vp, node, true /* isGlobal */);
      }

   else if (symbol->isLocalObject())
      {
      TR::VPClassType *typeConstraint = 0;
      TR::AutomaticSymbol *localObj = symbol->castToLocalObjectSymbol();
      symRef                       = localObj->getClassSymbolReference();
      if (localObj->getKind() == TR::New)
         {
         if (symRef)
            typeConstraint = TR::VPClassType::create(vp, symRef, true);
         }
      else if (localObj->getKind() == TR::anewarray)
         {
         typeConstraint = TR::VPClassType::create(vp, symRef, true);
         typeConstraint = typeConstraint->getClassType()->getArrayClass(vp);
         if (typeConstraint)
            {
            if (typeConstraint->getClass() && !typeConstraint->isFixedClass())
               typeConstraint = TR::VPFixedClass::create(vp, typeConstraint->getClass());
            }
         }
      else
         {
         TR_OpaqueClassBlock *clazz = vp->fe()->getClassFromNewArrayType(localObj->getArrayType());
         if (clazz)
            typeConstraint = TR::VPFixedClass::create(vp, clazz);
         }

      if (typeConstraint)
          vp->addGlobalConstraint(node, typeConstraint);
      vp->addGlobalConstraint(node, TR::VPNonNullObject::create(vp));
      }
   else
      {
      // Look at the def nodes for this node to see if the value being addressed
      // is known to be null or non-null
      //
      bool isGlobal;
      constraint = vp->mergeDefConstraints(node, vp->AbsoluteConstraint, isGlobal);
      if (constraint)
         {
         if (constraint->isNullObject())
            node->setPointsToNull(true);
         else if (constraint->isNonNullObject())
            node->setPointsToNonNull(true);
         }
      }
   return node;
   }

void constrainNewlyFoldedConst(OMR::ValuePropagation *vp, TR::Node *node, bool isGlobal)
   {
   switch (node->getOpCodeValue())
      {
      case TR::iconst:
         constrainIntConst(vp, node, isGlobal);
         break;

      case TR::lconst:
         constrainLongConst(vp, node, isGlobal);
         break;

      case TR::aconst:
         constrainAConst(vp, node, isGlobal);
         break;

      case TR::loadaddr:
         if (node->getSymbolReference()->getSymbol()->isClassObject())
            constrainClassObjectLoadaddr(vp, node, isGlobal);
         break;

      default:
         if (vp->trace())
            {
            traceMsg(
               vp->comp(),
               "constrainNewlyFoldedConst does not recognize n%un %s\n",
               node->getGlobalIndex(),
               node->getOpCode().getName());
            }
         break;
      }
   }

// Perform null check processing (used by TR::NULLCHK and TR::ResolveAndNULLCHK).
// Return 1 if the null check can be removed.
// Return 2 if the null check must be taken.
// Return 0 if the null check may or may not be taken.
//
static int32_t handleNullCheck(OMR::ValuePropagation *vp, TR::Node *node, bool resolveChkToo)
   {
   // Find the child containing the reference to be null checked. This is not
   // necessarily the direct child of this node.
   // Also the node to be null checked may have gone away due to earlier
   // optimizations. In this case the null check is redundant and can be
   // removed.
   //
   TR::Node *referenceChild = node->getNullCheckReference();

   if (referenceChild == NULL)
      {
      constrainChildren(vp, node);
      return 1;
      }

   // Process the reference child.
   //
   vp->launchNode(referenceChild, node, 0);

   // If the reference child has a non-null constraint the null check can be
   // eliminated.
   //
   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(referenceChild, isGlobal);
   if (constraint && constraint->isNonNullObject())
      {
      constrainChildren(vp, node);
      return 1;
      }

   // Propagate to the exception edges
   //
   if (!resolveChkToo)
      {
      /////vp->createExceptionEdgeConstraints(TR::Block::CanCatchNullCheck, vp->createValueConstraint(referenceChild, vp->AbsoluteConstraint, TR::VPNullObject::create(vp)), node);
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchNullCheck, NULL, node);
      }

   // If the reference child is null everything past this point in the block is
   // dead, since the exception will always be taken.
   //
   if (constraint && constraint->isNullObject())
      {
      vp->mustTakeException();
      return 2;
      }

   // After processing the children we can assert that the reference child is
   // now non-null.
   //
   constrainChildren(vp, node);
   // After processing, the referenceChild may no longer exist,
   // for e.g. arraylength is constant propagated
   // in that case, the nullcheck is redundant
   //
   if (!node->getNullCheckReference())
      return 1;

   vp->addBlockConstraint(referenceChild, TR::VPNonNullObject::create(vp));
   return 0;
   }

// Perform resolve check processing (used by TR::ResolveCHK and
// TR::ResolveAndNULLCHK).
// Return true if the resolve check can be removed.
//
static bool handleResolveCheck(OMR::ValuePropagation *vp, TR::Node *node, bool nullChkToo)
   {
   // Process all children of this node's child. If an exception is going to be
   // raised it will be after these children have been evaluated but before the
   // child node itself is evaluated, so this is the correct point at which to
   // propagate exception information.
   //
   TR::Node *child = node->getFirstChild();
   constrainChildren(vp, child);

   // See if the child has been proven to be resolved at this program point
   // (can't remove the check for a store to a final field)
   //
   if (!child->hasUnresolvedSymbolReference() &&
       !(node->getOpCode().isStore() && child->getSymbol()->isFinal()))
      {
      // The child has been transformed into a resolved reference. No need
      // for this ResolveCHK any more.
      //
      return true;
      }

   int32_t index = child->getSymbolReference()->getUnresolvedIndex() + vp->_firstUnresolvedSymbolValueNumber;

   // See if the constraint for this unresolved symbol exists. If it does, it
   // means that there has been a resolve check on all paths to this point.
   //
   OMR::ValuePropagation::Relationship *rel = vp->findConstraint(index);
   if (rel)
      {
      //
      // Could be a 0 or 1 (range)
      //
      //TR_ASSERT(rel->constraint->asIntConst(), "assertion failure");
      //
      if (child->getOpCode().isStore())
         {
         if (rel->constraint->asIntConst() &&
            rel->constraint->asIntConst()->getInt() == 1)
            return true;
         }
      else
         return true;
      }

   // Propagate to the exception edges
   //
   if (nullChkToo)
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchNullCheck | TR::Block::CanCatchResolveCheck, NULL, node);
   else
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchResolveCheck, NULL, node);

   //
   // This symbol can now be marked as resolved
   //
   // Create an int const == 1 if store is seen; subsequent stores dominated by this one
   // can only be removed if this constraint (int const 1) reaches them. This is because seeing
   // a resolvechk for the load is not sufficient (in every case) to remove the
   // resolvechk on the dominated store (if the unresolved field happened to be final, then the
   // store should raise an illegal access error even though the load will not)
   //
   if (child->getOpCode().isStore())
      vp->addConstraintToList(node, index, vp->AbsoluteConstraint, TR::VPIntConst::create(vp, 1), &vp->_curConstraints);
   else if (!rel)
      vp->addConstraintToList(node, index, vp->AbsoluteConstraint, TR::VPIntConst::create(vp, 0), &vp->_curConstraints);

   return false;
   }

TR::Node *constrainAsm(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchUserThrows, NULL, node);
   return node;
   }

TR::Node *constrainNullChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // Process the reference child and see if the null check can be removed
   //
   int32_t nullCheckResult = handleNullCheck(vp, node, false);

   // Now remove the check if we can
   //
   if (nullCheckResult == 1 &&
       performTransformation(vp->comp(), "%sRemoving redundant null check node [%p]\n", OPT_DETAILS, node))
      {
      TR_ASSERT(node->getReferenceCount() == 0, "NULLCHK has references");
      TR_ASSERT(node->getNumChildren() == 1, "NULLCHK has more than one child");

      // If the child of the nullchk is normally a treetop node, replace the
      // nullchk with that node
      TR::Node *child = node->getFirstChild();
      //
      if (child->getOpCode().isTreeTop())
         {
         if (vp->comp()->useCompressedPointers() &&
               child->getOpCode().isStoreIndirect())
            {
            TR::Node::recreate(node, TR::treetop);
            }
         else
            {
            TR_ASSERT(child->getReferenceCount() == 1, "Treetop child of NULLCHK has other uses");

            // Increment the reference count on the child so that we know it is
            // going to be kept.
            //
            child->setReferenceCount(0);
            vp->_curTree->setNode(child);
            }
         }
      else
         {
         TR::Node::recreate(node, TR::treetop);
         }
      vp->setChecksRemoved();
      }
   return node;
   }

TR::Node *constrainZeroChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);
   TR::Node *child = node->getFirstChild();
   bool isGlobal;
   TR::VPConstraint *nonzeroConstraint  = TR::VPLongRange::create(vp, TR::getMinSigned<TR::Int64>(), -1)->merge(TR::VPLongRange::create(vp, 1, TR::getMaxSigned<TR::Int64>()), vp);

   TR::VPConstraint *childConstraint = vp->getConstraint(child, isGlobal);
   if (childConstraint)
      {
      TR::VPConstraint *zeroConstraint  = TR::VPIntConst::create(vp, 0);
      if (!zeroConstraint->intersect(childConstraint, vp))
         {
         // Can't be zero
         if (performTransformation(vp->comp(), "%sRemoving unnecessary %s [%p]\n", OPT_DETAILS, node->getOpCode().getName(), node))
            {
            for (int32_t i = 1; i < node->getNumChildren(); i++)
               node->getChild(i)->recursivelyDecReferenceCount();
            TR::Node::recreate(node, TR::treetop);
            node->setNumChildren(1);
            vp->setChecksRemoved();
            }
         }

      if (!nonzeroConstraint->intersect(childConstraint, vp))
         {
         // Can't be nonzero
         if (performTransformation(vp->comp(), "%sRemoving inevitable %s [%p]\n", OPT_DETAILS, node->getOpCode().getName(), node))
            vp->mustTakeException();
         }
      }

   // From here onward we know it's nonzero
   //
   vp->addBlockConstraint(child, nonzeroConstraint);

   return node;
   }

TR::Node *constrainResolveChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // See if the resolve check can be eliminated
   //
   bool removeTheCheck = handleResolveCheck(vp, node, false);

   // Now process the child itself.
   //
   constrainChildren(vp, node);

   // Processing the child could have caused it to be removed
   if (node->getNumChildren()==0)
      {
      TR::Node::recreate(node, TR::treetop);
      return node;
      }

   // Processing the child could have caused it to be changed to a resolved
   // symbol (if a call was devirtualized)
   //
   TR::Node *child = node->getFirstChild();
   if (!child->hasUnresolvedSymbolReference() &&
       !(node->getOpCode().isStore() && child->getSymbol()->isFinal()))
       removeTheCheck = true;

   // Now remove the check if we can
   //
   if (removeTheCheck &&
       performTransformation(vp->comp(), "%sRemoving redundant resolve check node [%p]\n", OPT_DETAILS, node))
      {
      TR_ASSERT(node->getReferenceCount() == 0, "ResolveCHK has references");
      TR_ASSERT(node->getNumChildren() == 1, "ResolveCHK has more than one child");

      // If the child of the check is normally a treetop node, replace the
      // check node with that node
      //
      if (child->getOpCode().isTreeTop())
         {
         if (vp->comp()->useCompressedPointers() &&
               child->getOpCode().isStoreIndirect())
            {
            TR::Node::recreate(node, TR::treetop);
            }
         else
            {
            TR_ASSERT(child->getReferenceCount() == 1, "Treetop child of ResolveCHK has other uses");

            // Increment the reference count on the child so that we know it is
            // going to be kept.
            //
            child->setReferenceCount(0);
            //vp->_curTree->setNode(child);
            node = child;
            }
         }
      else
         {
         TR::Node::recreate(node, TR::treetop);
         }
      vp->setChecksRemoved();
      }

   // storage access here, sync region ends
   // memory barrier required at next critical region
   OMR::ValuePropagation::Relationship *syncRel = vp->findConstraint(vp->_syncValueNumber);
   TR::VPSync *sync = NULL;
   if (!removeTheCheck && syncRel && syncRel->constraint)
      sync = syncRel->constraint->asVPSync();
   if (sync && sync->syncEmitted() == TR_yes)
      {
      vp->addConstraintToList(NULL, vp->_syncValueNumber, vp->AbsoluteConstraint, TR::VPSync::create(vp, TR_maybe), &vp->_curConstraints);
      if (vp->trace())
         {
         traceMsg(vp->comp(), "Setting syncRequired due to node [%p]\n", node);
         }
      }
   else
      {
      if (vp->trace())
         {
         if (sync)
            traceMsg(vp->comp(), "syncRequired is already setup at node [%p]\n", node);
         else if (removeTheCheck)
            traceMsg(vp->comp(), "check got removed at node [%p], syncRequired unchanged\n", node);
         else
            traceMsg(vp->comp(), "No sync constraint found at node [%p]!\n", node);
         }
      }

   return node;
   }

TR::Node *constrainResolveNullChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   // See if the resolve check can be eliminated
   //
   bool removeTheCheck = handleResolveCheck(vp, node, true);

   // Now process the child itself.
   //
   constrainChildren(vp, node);

   // Processing the child could have caused it to be changed to a resolved
   // symbol (if a call was devirtualized)
   //
   TR::Node *child = node->getFirstChild();
   if (!child->hasUnresolvedSymbolReference() &&
       !(node->getOpCode().isStore() && child->getSymbol()->isFinal()))
       removeTheCheck = true;

   // See if the null check can be eliminated
   //
   if (handleNullCheck(vp, node, !removeTheCheck) == 1)
      {
      if (removeTheCheck)
         {
         if (performTransformation(vp->comp(), "%sChanging ResolveAndNULLCHK node into a treetop node [%p]\n", OPT_DETAILS, node))
            {
            TR::Node::recreate(node, TR::treetop);
            vp->setChecksRemoved();
            }
         }
      else
         {
         if (performTransformation(vp->comp(), "%sChanging ResolveAndNULLCHK node into a ResolveCHK node [%p]\n", OPT_DETAILS, node))
            {
            TR::Node::recreate(node, TR::ResolveCHK);
            vp->setChecksRemoved();
            }
         }
      }
   else if (removeTheCheck)
      {
      if (performTransformation(vp->comp(), "%sChanging ResolveAndNULLCHK node into a NULLCHK node [%p]\n", OPT_DETAILS, node))
         {
         TR::Node::recreate(node, TR::NULLCHK);
         node->setSymbolReference(vp->comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(vp->comp()->getMethodSymbol()));
         vp->setChecksRemoved();
         }
      }
   return node;
   }

TR::Node *constrainOverflowChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp,node);
   return node;
   }

TR::Node *constrainUnsignedOverflowChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp,node);
   return node; 
   }

TR::Node *constrainDivChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *divNode = node->getFirstChild();
   bool removeDivCheck = false;
   bool complexDivCheck = true;

   if (divNode->getOpCode().isDiv() || divNode->getOpCode().isRem())
      {
      bool isGlobal;
      TR::VPConstraint *denominator  = vp->getConstraint(divNode->getSecondChild(), isGlobal);

      // If the denominator has a range [!= 0]
      //       then remove the div check.
      //
      if (denominator &&
          !denominator->asMergedConstraints())
          {
          if (divNode->getType().isInt32())
             {
             if (denominator->getLowInt() > 0 ||
                 denominator->getHighInt() < 0)
                 removeDivCheck = true;

             if (denominator->getLowInt() > -1 ||
                 denominator->getHighInt() < -1)
                complexDivCheck = false;
             }
          else if (divNode->getType().isInt64())
             {
             if (denominator->getLowLong() > 0 ||
                 denominator->getHighLong() < 0)
                 removeDivCheck = true;

             if (denominator->getLowLong() > -1 ||
                 denominator->getHighLong() < -1)
                complexDivCheck = false;
             }
          }
       else if (denominator)
          {
          if (!denominator->asMergedConstraints()->getList()->isEmpty())
             {
             removeDivCheck = true;
             complexDivCheck = false;
             }

          ListIterator<TR::VPConstraint> it(denominator->asMergedConstraints()->getList());
          TR::VPConstraint *nextConstraint = NULL;
          for (nextConstraint = it.getFirst(); nextConstraint != NULL; nextConstraint = it.getNext())
             {
             if (divNode->getType().isInt32())
                {
                if ((nextConstraint->getLowInt() <= 0) &&
                    (nextConstraint->getHighInt() >= 0))
                    removeDivCheck = false;

                if (nextConstraint->getLowInt() <= -1 &&
                    nextConstraint->getHighInt() >= -1)
                   complexDivCheck = true;
                }
             else if (divNode->getType().isInt64())
                {
                if ((nextConstraint->getLowLong() <= 0) &&
                    (nextConstraint->getHighLong() >= 0))
                    removeDivCheck = false;

                if (nextConstraint->getLowLong() <= -1 &&
                    nextConstraint->getHighLong() >= -1)
                   complexDivCheck = true;
                }
             }
          }


      TR::VPConstraint *numerator  = vp->getConstraint(divNode->getFirstChild(), isGlobal);
      if (numerator)
          {
          if (divNode->getType().isInt32())
             {
             if (numerator->getLowInt() > TR::getMinSigned<TR::Int32>())
                 complexDivCheck = false;
              }
          else if (divNode->getType().isInt64())
             {
             if (numerator->getLowLong() > TR::getMinSigned<TR::Int64>())
                 complexDivCheck = false;
             }
          }
      }
   else
      removeDivCheck = true;

   if (removeDivCheck &&
       performTransformation(vp->comp(), "%sRemoving redundant div check node [%p]\n", OPT_DETAILS, node))
      TR::Node::recreate(node, TR::treetop);
   else
      {
      if (!complexDivCheck)
         {
         if (divNode->getOpCode().isDiv() || divNode->getOpCode().isRem())
            divNode->setSimpleDivCheck(true);
         }
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchDivCheck, NULL, node);
      }
   return node;
   }


TR::Node *constrainTRT(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchBoundCheck, NULL, node);
   return node;
   }


TR::Node *constrainBndChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   // There are two parts to the bounds check, the array size and the
   // index value to be checked.
   //
   TR::Node *sizeNode  = node->getFirstChild();
   TR::Node *indexNode = node->getSecondChild();

   bool isGlobal;
   TR::VPConstraint *sizeBefore  = vp->getConstraint(sizeNode, isGlobal);
   TR::VPConstraint *index = vp->getConstraint(indexNode, isGlobal);

   // If the index has a range [0  -> less than the lower bound of the size]
   //       then remove the bound check.
   //
   if (sizeBefore && index)
      {
      if (index->getLowInt() >= 0 &&
          index->getHighInt() < sizeBefore->getLowInt() &&
          performTransformation(vp->comp(), "%sRemoving unnecessary bound check node [%p]\n", OPT_DETAILS, node))
         {
         // Change this node to a treetop containing the index (which is
         // currently the second child of the bounds check).
         //
         TR::Node::recreate(node, TR::treetop);
         vp->removeNode(sizeNode);
         node->setChild(0, indexNode);
         node->setChild(1, NULL);
         node->setNumChildren(1);
         vp->setChecksRemoved();
         return node;
         }
      }

   bool globalFlag;
   TR::VPConstraint *relativeConstraint = vp->getConstraint(indexNode, globalFlag, sizeNode);
   if (relativeConstraint &&
       relativeConstraint->mustBeLessThan() &&
       performTransformation(vp->comp(), "%sRemoving redundant bound check node (subsumed) [%p]\n", OPT_DETAILS, node))
      {
      // Change this node to a treetop containing the index (which is
      // currently the second child of the bounds check).
      //
      TR::Node::recreate(node, TR::treetop);
      vp->removeNode(sizeNode);
      node->setChild(0, indexNode);
      node->setChild(1, NULL);
      node->setNumChildren(1);
      vp->setChecksRemoved();
      return node;
      }

   if (vp->_enableVersionBlocks &&
       !vp->_disableVersionBlockForThisBlock &&
       vp->lastTimeThrough())
      vp->_bndChecks->add(node);
   // TODO - create a constraint along the exception edge if possible
   //
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchBoundCheck, NULL, node);

   if (sizeNode == indexNode)
      {
      vp->mustTakeException();
      return node;
      }

   // We now know the BNDCHK has succeeded, and can make some assertions about the index value.
   //    1) Its low bound must be >= 0
   //    2) Its high bound must be (maximum array size-1)
   //
   uint32_t elementSize = 1;
   if (sizeNode->getOpCode().isArrayLength())
      {
      elementSize = sizeNode->getArrayStride();
      }

   int32_t minIndex = 0;
   int32_t maxIndex = TR::Compiler->om.maxArraySizeInElements(elementSize, vp->comp()) - 1;

   if (sizeBefore)
      maxIndex = std::min(maxIndex, sizeBefore->getHighInt() - 1);

   // Calculate the new constraint for the index value. If it is null, we
   // will definitely fail the bndchk
   //
   TR::VPConstraint *indexConstraint = NULL;
   if (minIndex <= maxIndex)
      {
      indexConstraint = TR::VPIntRange::create(vp, minIndex, maxIndex);
      if (index)
         indexConstraint = index->intersect(indexConstraint, vp);
      }
   else
      indexConstraint = NULL;

   if (!indexConstraint || (index && index->getLowInt() >= maxIndex+1))
      {
      // Everything past this point in the block is dead, since the exception
      // will always be taken.
      //
      vp->mustTakeException();
      return node;
      }

   vp->addBlockConstraint(indexNode, indexConstraint);

   // After the BNDCHK succeeds, we know the array size must be at least as
   // large as the low end of the index range + 1.
   //
   int32_t minSize = indexConstraint->getLowInt()+1;
   int32_t maxSize = TR::Compiler->om.maxArraySizeInElements(elementSize, vp->comp());
   TR::VPConstraint *sizeAfter =  TR::VPIntRange::create(vp, minSize, maxSize);
   if (sizeBefore)
     sizeAfter = sizeBefore->intersect(sizeAfter, vp);

   vp->addBlockConstraint(sizeNode, sizeAfter);

   // Propagate the array size down to the underlying array object
   //
   if (sizeNode->getOpCode().isArrayLength())
      {
      TR::Node *objectRef = sizeNode->getFirstChild();
      vp->addBlockConstraint(objectRef, TR::VPArrayInfo::create(vp, minSize, maxSize, 0));
      }
   return node;
   }


TR::Node *constrainBndChkWithSpineChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   // There are two parts to the bounds check, the array size and the
   // index value to be checked.
   //
   int32_t arrayChildNum = 1;
   int32_t boundChildNum = 2;
   int32_t indexChildNum = 3;

   TR::Node *sizeNode  = node->getChild(boundChildNum);
   TR::Node *indexNode = node->getChild(indexChildNum);

   bool isGlobal;
   TR::VPConstraint *sizeConstraint  = vp->getConstraint(sizeNode, isGlobal);
   TR::VPConstraint *indexConstraint = vp->getConstraint(indexNode, isGlobal);

   bool removeBndChk = false;
   bool removeSpineChk = false;


   // See if the underlying array has bound limits
   //
   TR::Node *objectRef = node->getChild(arrayChildNum);
   TR::VPConstraint *objConstraint = vp->getConstraint(objectRef, isGlobal);

   if (indexConstraint && objConstraint)
      {
      TR::VPArrayInfo *arrayInfo = objConstraint->getArrayInfo();
      if (arrayInfo)
         {
         int32_t lowerBoundLimit = arrayInfo->lowBound();
         if (indexConstraint->getLowInt() >= 0 &&
             indexConstraint->getHighInt() < lowerBoundLimit &&
             performTransformation(vp->comp(), "%sRemoving unnecessary bound check from composite node [%p]\n", OPT_DETAILS, node))
            {
            removeBndChk = true;
            }
         }
      }

   // -------------------------------------------------------------------------
   // First determine if we can fold the bound or spine check (or both).
   // -------------------------------------------------------------------------

   // If the index has a range [0  -> less than the lower bound of the size]
   //       then remove the bound check.
   //
   if (sizeConstraint && indexConstraint)
      {
      if (indexConstraint->getLowInt() >= 0 &&
          indexConstraint->getHighInt() < sizeConstraint->getLowInt() &&
          performTransformation(vp->comp(), "%sRemoving unnecessary bound check from composite node [%p]\n", OPT_DETAILS, node))
         {
         removeBndChk = true;
         }
      }

   if (sizeConstraint && !vp->comp()->generateArraylets())
      {
      // If we can determine the element size then we can possibly eliminate the spine check.
      //
      int32_t elementSize = 0;

      if (sizeNode->getOpCode().isArrayLength())
         {
         elementSize = sizeNode->getArrayStride();
         }

      if (!elementSize)
         {
         // See if the base array has bound limits
         //
         TR::Node *objectRef = node->getSecondChild();
         bool isGlobal;
         TR::VPConstraint *constraint = vp->getConstraint(objectRef, isGlobal);

         if (constraint)
            {
            TR::VPArrayInfo *arrayInfo = constraint->getArrayInfo();
            if (arrayInfo)
               {
               elementSize = arrayInfo->elementSize();
               }
            }

         // If the element size is still not known, try to get it from the node or
         // from the signature, and then propagate it back down to the object ref.
         //
         if (!elementSize && constraint)
            {
            int32_t len;
            const char *sig = constraint->getClassSignature(len);
            if (sig)
               elementSize = arrayElementSize(sig, len, objectRef, vp);
            }
         }

      if (elementSize > 0 &&
          !TR::Compiler->om.isDiscontiguousArray(sizeConstraint->getHighInt(), elementSize) &&
          (sizeConstraint->getLowInt() > 0) &&
          performTransformation(vp->comp(), "%sRemoving unnecessary spine check from composite node [%p] : min arraylength = %d, max arraylength=%d, element size=%d\n",
             OPT_DETAILS, node, sizeConstraint->getLowInt(), sizeConstraint->getHighInt(), elementSize))
         {
         removeSpineChk = true;
         }
      }

   bool globalFlag;
   TR::VPConstraint *relativeConstraint = vp->getConstraint(indexNode, globalFlag, sizeNode);
   if (!removeBndChk &&
       relativeConstraint &&
       relativeConstraint->mustBeLessThan() &&
       performTransformation(vp->comp(), "%sRemoving redundant bound check from node (subsumed) [%p]\n", OPT_DETAILS, node))
      {
      removeBndChk = true;
      }

   // -------------------------------------------------------------------------
   // Now do the work to remove the bound or spine check.
   // -------------------------------------------------------------------------

   if (removeBndChk && removeSpineChk)
      {
      // Change this node to a treetop containing the index.
      //
      TR::Node::recreate(node, TR::treetop);

      TR::Node *arrayElement = node->getChild(0);
      TR::Node *baseArray = node->getChild(1);
      vp->removeNode(baseArray);
      vp->removeNode(sizeNode);
      node->setChild(0, indexNode);
      node->setChild(1, NULL);
      node->setChild(2, NULL);
      node->setChild(3, NULL);
      node->setNumChildren(1);
      vp->setChecksRemoved();

      TR::Node *treeTopNode;
      if (arrayElement->getOpCode().isStore())
         {
         treeTopNode = arrayElement;
         }
      else
         {
         treeTopNode = TR::Node::create(TR::treetop, 1, arrayElement);
         }

      arrayElement->decReferenceCount();

      TR::TreeTop *newTree = TR::TreeTop::create(vp->comp(), treeTopNode);
      TR::TreeTop *prevTree = vp->_curTree;
      newTree->join(prevTree->getNextTreeTop());
      prevTree->join(newTree);

      return node;
      }
   else if (removeBndChk)
      {
      // Convert the node to a spine check.
      //
      TR::Node::recreate(node, TR::SpineCHK);

      vp->removeNode(sizeNode);
      node->setChild(2, indexNode);
      node->setChild(3, NULL);
      node->setNumChildren(3);
      vp->setChecksRemoved();
      return node;
      }
   else if (removeSpineChk)
      {
      // Convert the node to a bound check.
      //
      TR::Node::recreate(node, TR::BNDCHK);
      TR::Node *arrayElement = node->getChild(0);
      TR::Node *baseArray = node->getChild(1);
      vp->removeNode(baseArray);
      node->setChild(0, sizeNode);
      node->setChild(1, indexNode);
      node->setChild(2, NULL);
      node->setChild(3, NULL);
      node->setNumChildren(2);

      TR::Node *treeTopNode;
      if (arrayElement->getOpCode().isStore())
         {
         treeTopNode = arrayElement;
         }
      else
         {
         treeTopNode = TR::Node::create(TR::treetop, 1, arrayElement);
         }

      arrayElement->decReferenceCount();

      TR::TreeTop *newTree = TR::TreeTop::create(vp->comp(), treeTopNode);
      TR::TreeTop *prevTree = vp->_curTree;
      newTree->join(prevTree->getNextTreeTop());
      prevTree->join(newTree);

      return constrainBndChk(vp, node);
      }

   if (vp->_enableVersionBlocks &&
       !vp->_disableVersionBlockForThisBlock &&
       vp->lastTimeThrough())
      vp->_bndChecks->add(node);

   vp->createExceptionEdgeConstraints(TR::Block::CanCatchBoundCheck, NULL, node);

   if ((sizeNode == indexNode) &&
         !((sizeNode->getOpCodeValue() == TR::iconst) && (sizeNode->getInt() == 0)))
      {
      vp->mustTakeException();
      return node;
      }

   // -------------------------------------------------------------------------
   // We can now make some assertions about the index value:
   //    1) its low bound must be >= 0
   //    2) its high bound must be (maximum array size-1)
   // -------------------------------------------------------------------------

   bool isSizeContigArrayLength = true; //(sizeNode->getOpCodeValue() == TR::contigarraylength) ? true : false;

   uint32_t elementSize = 1;
   if (sizeNode->getOpCode().isArrayLength())
      {
      elementSize = sizeNode->getArrayStride();
      }

   int32_t low = 0;
   int32_t high = TR::getMaxSigned<TR::Int32>();

   if (sizeConstraint && !isSizeContigArrayLength)
      {
      high = sizeConstraint->getHighInt() - 1;
      }
   else if (elementSize > 0)
      {
      high = high/elementSize - 1;
      }

   // Calculate the new constraint for the index value.  If it is null, we
   // will definitely fail the bndchk
   //
   TR::VPConstraint *constraint = NULL;
   bool foundArrayInfo = false;

   if (isSizeContigArrayLength)
      {
      // See if the underlying array has bound limits
      //
      TR::Node *objectRef = node->getChild(arrayChildNum);
      bool isGlobal;
      TR::VPConstraint *objConstraint = vp->getConstraint(objectRef, isGlobal);

      if (objConstraint)
         {
         TR::VPArrayInfo *arrayInfo = objConstraint->getArrayInfo();
         if (arrayInfo)
            {
            int32_t lowerBoundLimit = arrayInfo->lowBound();
            int32_t upperBoundLimit = arrayInfo->highBound();
            constraint = TR::VPIntRange::create(vp, 0, upperBoundLimit-1);
            foundArrayInfo = true;
            }
         }

      if (!constraint)
         constraint = TR::VPIntRange::create(vp, low, high);

      if (indexConstraint && constraint)
         constraint = indexConstraint->intersect(constraint, vp);
      }
   else if (low <= high)
      {
      constraint = TR::VPIntRange::create(vp, low, high);

      if (indexConstraint && constraint)
         constraint = indexConstraint->intersect(constraint, vp);
      }
   else
      constraint = NULL;

   if (/* (!isSizeContigArrayLength || foundArrayInfo) && */
       (!constraint || (indexConstraint && indexConstraint->getLowInt() >= high+1)))
      {
      // Everything past this point in the block is dead, since the exception
      // will always be taken.
      //
      vp->mustTakeException();
      return node;
      }


   vp->addBlockConstraint(indexNode, constraint);

   // -------------------------------------------------------------------------
   // We can make an assertion about the minimum array size.  It must be at
   // least as large as the low end of the index range + 1.
   // -------------------------------------------------------------------------

   if (!isSizeContigArrayLength)
      {
      // Contiguous arraylengths with a lower bound of zero imply the array may be
      // discontiguous and hence the lower bound must remain at zero.
      //
      low = constraint->getLowInt();
      if (!isSizeContigArrayLength || (low > 0))
         low++;

      high = TR::getMaxSigned<TR::Int32>();
      if (elementSize > 0)
         {
         high = high/elementSize;
         }

      constraint =  TR::VPIntRange::create(vp, low, high);

      if (sizeConstraint)
         {
         constraint =  TR::VPIntRange::create(vp, low, high);
         constraint = sizeConstraint->intersect(constraint, vp);
         }

      vp->addBlockConstraint(sizeNode, constraint);
      }

#if 0
//-----------------------------------------------
   // We can make an assertion about the minimum array size. It must be at least
   // as large as the low end of the index range + 1.
   //
   low = constraint->getLowInt()+1;

   high = TR::getMaxSigned<TR::Int32>();
   if (elementSize > 0)
      high = high/elementSize;
   constraint =  TR::VPIntRange::create(vp, low, high);
   if (size)
     constraint = size->intersect(constraint, vp);

   vp->addBlockConstraint(sizeNode, constraint);
//----------------------------------------------------------
#endif

   // Propagate the array size down to the underlying array object
   //
   if (sizeNode->getOpCode().isArrayLength())
      {
      TR::Node *objectRef = sizeNode->getFirstChild();
      vp->addBlockConstraint(objectRef, TR::VPArrayInfo::create(vp, low, high, 0));
      }

   return node;
   }




TR::Node *constrainArrayCopyBndChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *lhsNode = node->getFirstChild();
   TR::Node *rhsNode = node->getSecondChild();

   bool isGlobal;
   TR::VPConstraint *lhs = vp->getConstraint(lhsNode, isGlobal);
   TR::VPConstraint *rhs = vp->getConstraint(rhsNode, isGlobal);

   // If the lhs is guaranteed to be >= the rhs, remove the bound check.
   //
   if ((vp->getValueNumber(lhsNode) == vp->getValueNumber(rhsNode)) ||
       (lhs && rhs && lhs->getLowInt() >= rhs->getHighInt()))
      {
      if (performTransformation(vp->comp(), "%sRemoving redundant arraycopy bound check node [%p]\n", OPT_DETAILS, node))
         {
         vp->removeNode(node);
         vp->setChecksRemoved();
         return NULL;
         }
      }

   // TODO - create a constraint along the exception edge if possible
   //
   vp->createExceptionEdgeConstraints(TR::Block::CanCatchBoundCheck, NULL, node);

   // We can now make some assertions about the children
   //    1) Both of them must be >= 0
   //    2) Both of them must be  < maximum array size
   //    3) The lhs low bound >= the rhs low bound
   //    3) The rhs high bound <= the lhs high bound
   //
   uint32_t elementSize = 1;
   bool isArrayLengthCheck = false;
   if (lhsNode->getOpCode().isArrayLength())
      {
      elementSize = lhsNode->getArrayStride();
      isArrayLengthCheck = true;
      }

   int32_t low = 0;
   int32_t high = TR::getMaxSigned<TR::Int32>();
   if (elementSize > 0)
      high = high/elementSize-1;

   if (lhs && lhs->getHighInt() < high)
      high = lhs->getHighInt();

   if (rhs && rhs->getLowInt() > low)
      low = rhs->getLowInt();

   // Calculate the new constraints for the children.
   //
   TR::VPConstraint *constraint = NULL;
   TR::VPConstraint *lhsConstraint = NULL;
   TR::VPConstraint *rhsConstraint = NULL;
   if (low <= high)
      {
      constraint = TR::VPIntRange::create(vp, low, high);
      if (lhs)
         lhsConstraint = lhs->intersect(constraint, vp);
      else
         lhsConstraint = constraint;
      if (rhs)
         rhsConstraint = rhs->intersect(constraint, vp);
      else
         rhsConstraint = constraint;
      }
   else
      lhsConstraint = NULL;

   if (!(lhsConstraint && rhsConstraint))
      {
      // Everything past this point in the block is dead, since the exception
      // will always be taken.
      //
      vp->mustTakeException();
      return node;
      }

   vp->addBlockConstraint(lhsNode, lhsConstraint);
   vp->addBlockConstraint(rhsNode, rhsConstraint);

   // If this is an arraylength check, the arraylength range can be propagated
   // down to the underlying array
   //
   if (isArrayLengthCheck)
      {
      TR::Node *objectRef = lhsNode->getFirstChild();
      vp->addBlockConstraint(objectRef, TR::VPArrayInfo::create(vp, lhsConstraint->getLowInt(), lhsConstraint->getHighInt(), 0));
      }
   return node;
   }

TR::Node *constrainArrayStoreChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *child = node->getFirstChild();
   TR::Node *objectRef;
   TR::Node *arrayRef;
   bool canBeRemoved = false;
   bool mustFail = false;
   bool isGlobal;

   objectRef = child->getSecondChild();
   arrayRef = child->getChild(2);

   if (objectRef->getOpCode().isLoadVar() &&
       objectRef->getOpCode().isIndirect() &&
       objectRef->getFirstChild()->isInternalPointer())
      {
      TR::Node *base = objectRef->getFirstChild()->getFirstChild();
      if (base->getOpCode().hasSymbolReference() &&
            base->getSymbol()->isArrayletShadowSymbol())
         {
         if (base->getFirstChild()->getOpCode().isArrayRef())
            base = base->getFirstChild()->getFirstChild();
         }

      if (base == arrayRef)
         canBeRemoved = true;
      }

   if (canBeRemoved != true)
      {
      TR::VPConstraint *object = vp->getConstraint(objectRef, isGlobal);
      TR::VPConstraint *array  = vp->getConstraint(arrayRef, isGlobal);

      // If the object reference is null we can remove this check.
      // If the array reference is null we can remove this check, since there
      // will be a nullcheck on the array reference before storing into it.
      //
      if (object && object->isNullObject())
         canBeRemoved = true;
      else if (array && array->isNullObject())
         canBeRemoved = true;

      // If the array is a resolved class and known to be an array type we may
      // still be able to remove the check ...
      //
      else if (array && array->getClass())
         {
         int32_t len;
         const char *sig = array->getClassSignature(len);
         if (sig && sig[0] == '[')
            {
            // If the array is known to be a fixed array of Object,
            // the check can be removed
            // TODO -  get a pointer to the Object class from somewhere and
            // compare object pointers instead of signatures
            //
            if (len == 19 && array->isFixedClass() &&
                !strncmp(sig, "[Ljava/lang/Object;", 19))
               {
               canBeRemoved = true;
               }

            // If the object's class is resolved too, see if we can prove the
            // check will succeed.
            //
            else if (object && object->getClass())
               {
               TR_OpaqueClassBlock *arrayComponentClass = vp->fe()->getComponentClassFromArrayClass(array->getClass());
               TR_OpaqueClassBlock *objectClass = object->getClass();
               if (object->asClass() && object->isClassObject() == TR_yes)
                  objectClass = vp->fe()->getClassClassPointer(objectClass);
               if (array->asClass() && array->isClassObject() == TR_yes)
                  arrayComponentClass = vp->fe()->getClassClassPointer(array->getClass());

               TR_YesNoMaybe isInstance = TR_maybe;
               if (arrayComponentClass)
                  isInstance = vp->fe()->isInstanceOf(objectClass, arrayComponentClass, object->isFixedClass(), array->isFixedClass());
                  //isInstance = vp->fe()->isInstanceOf(object->getClass(), arrayComponentClass, object->isFixedClass(), array->isFixedClass());

               if (isInstance == TR_yes)
                  {
                  vp->registerPreXClass(object);
                  canBeRemoved = true;
                  }
               else if (isInstance == TR_no && debug("enableMustFailArrayStoreCheckOpt"))
                  {
                  vp->registerPreXClass(object);
                  mustFail = true;
                  }
               else if (arrayComponentClass && objectClass && !TR::Compiler->cls.isClassArray(vp->comp(), arrayComponentClass) &&
                        (arrayComponentClass == objectClass) && !vp->comp()->fe()->classHasBeenExtended(objectClass))
                  {
                  if (vp->trace())
                     traceMsg(vp->comp(),"Setting arrayStoreClass on ArrayStoreChk node [%p] to [%p]\n", node, objectClass);
                  node->setArrayStoreClassInNode(objectClass);
                  }

              else if ( !vp->comp()->compileRelocatableCode() &&
                        !(TR::Options::getCmdLineOptions()->getOption(TR_DisableArrayStoreCheckOpts))  &&
                        arrayComponentClass &&
                        objectClass  &&
                        vp->fe()->isInstanceOf(objectClass, arrayComponentClass,true,true)
                        )
                  {
                  if (vp->trace())
                     traceMsg(vp->comp(),"Setting arrayComponentClass on ArrayStoreChk node [%p] to [%p]\n", node, arrayComponentClass);
                  node->setArrayComponentClassInNode(arrayComponentClass);
                  }

               }
            }
         }
      }

   // Remove the check if we can
   //
   if (canBeRemoved)
      {
      // Change the write barrier into iastore
      //
      canRemoveWrtBar(vp, child);

      if (performTransformation(vp->comp(), "%sRemoving redundant arraystore check node [%p]\n", OPT_DETAILS, node))
         {
         // Child is the iastore. Just change this node to a treetop
         //
         TR::Node::recreate(node, TR::treetop);
         //DemandLiteralPool can add extra aload to arraystore check node, but treetop can only have one child
         if (vp->comp()->cg()->supportsOnDemandLiteralPool() && node->getNumChildren() > 1)
            {
            vp->removeNode(node->getChild(1));
            node->setNumChildren(1);
            }

         vp->setChecksRemoved();
         return node;
         }
      }

   vp->createExceptionEdgeConstraints(TR::Block::CanCatchArrayStoreCheck, NULL, node);

   // If the check must fail, remove the rest of the block
   //
   if (mustFail)
      {
      vp->mustTakeException();
      }
   return node;
   }

TR::Node *constrainArrayChk(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *srcNode = node->getFirstChild();
   TR::Node *dstNode = node->getSecondChild();

   bool isGlobal;
   TR::VPConstraint *src = vp->getConstraint(srcNode, isGlobal);
   TR::VPConstraint *dst = vp->getConstraint(dstNode, isGlobal);
   TR::VPClassType *srcType = src ? src->getClassType() : NULL;
   TR::VPClassType *dstType = dst ? dst->getClassType() : NULL;
   bool removeThisNode = false;

   // If either array is null we can remove this check since this node will
   // never be reached.
   //
   if (src && src->isNullObject())
      removeThisNode = true;
   else if (src && src->isNullObject())
      removeThisNode = true;

   // If the arrays are the same object we can remove this check
   //
   else if (srcNode == dstNode ||
            vp->getValueNumber(srcNode) == vp->getValueNumber(dstNode))
      {
      removeThisNode = true;
      }

   // If the arrays are of the same primitive type we can remove this check
   //
   else if (srcType && srcType == dstType && srcType->isPrimitiveArray(vp->comp()))
      {
      removeThisNode = true;
      }

   if (removeThisNode &&
       performTransformation(vp->comp(), "%sRemoving redundant array check node [%p]\n", OPT_DETAILS, node))
      {
      // Re-anchor the children and mark this tree as removable
      //
      vp->removeNode(node);
      return NULL;
      }

   if (srcType)
      {
      if (srcType->isPrimitiveArray(vp->comp()))
         node->setArrayChkPrimitiveArray1(true);
      else if (srcType->isReferenceArray(vp->comp()))
         node->setArrayChkReferenceArray1(true);
      }
   if (dstType)
      {
      if (dstType->isPrimitiveArray(vp->comp()))
         node->setArrayChkPrimitiveArray2(true);
      else if (dstType->isReferenceArray(vp->comp()))
         node->setArrayChkReferenceArray2(true);
      }

   vp->createExceptionEdgeConstraints(TR::Block::CanCatchArrayStoreCheck, NULL, node);
   return node;
   }

TR::Node *constrainArraycopy(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainChildren(vp, node);

   TR::Node *srcObjNode = NULL;
   TR::Node *srcNode    = NULL;
   TR::Node *dstObjNode = NULL;
   TR::Node *dstNode    = NULL;
   TR::Node *lenNode    = NULL;

   if (node->getNumChildren() == 5)
      {
      srcObjNode = node->getFirstChild();
      dstObjNode = node->getSecondChild();
      srcNode    = node->getChild(2);
      dstNode    = node->getChild(3);
      lenNode    = node->getChild(4);
      }
   else
      {
      srcNode = node->getFirstChild();
      dstNode = node->getSecondChild();
      lenNode = node->getChild(2);
      }

   bool isGlobal;
   TR::VPConstraint *len = vp->getConstraint(lenNode, isGlobal);

   // If the length to be copied is zero, remove the arraycopy itself
   //
   if (len && node->getNumChildren()==3 && len->asIntConst() && len->getLowInt() == 0 && performTransformation(vp->comp(), "%sRemoving arraycopy node [%p]\n", OPT_DETAILS, node))
      {
      vp->removeArrayCopyNode(vp->_curTree);
      vp->removeNode(node);
      vp->_curTree->setNode(NULL);
      vp->invalidateUseDefInfo();
      vp->invalidateValueNumberInfo();
      return node;
      }

   // If this is the 5-children version of arraycopy, see if it can be changed
   // into the 3-children version.
   //
   if (node->getNumChildren() == 5)
      {
      TR::VPConstraint *src = vp->getConstraint(srcObjNode, isGlobal);
      TR::VPConstraint *dst = vp->getConstraint(dstObjNode, isGlobal);
      TR::VPClassType *srcType = src ? src->getClassType() : NULL;
      TR::VPClassType *dstType = dst ? dst->getClassType() : NULL;
      TR::DataType elementType = TR::NoType;
      if (srcType && srcType->isPrimitiveArray(vp->comp()))
         elementType = srcType->getPrimitiveArrayDataType();
      else if (dstType && dstType->isPrimitiveArray(vp->comp()))
         elementType = dstType->getPrimitiveArrayDataType();
      if (elementType != TR::NoType &&
          performTransformation(vp->comp(), "%sTransforming arraycopy node [%p]\n", OPT_DETAILS, node))
         {
         node->setChild(0, srcNode);
         node->setChild(1, dstNode);
         node->setChild(2, lenNode);
         node->setChild(3, NULL);
         node->setChild(4, NULL);
         srcObjNode->recursivelyDecReferenceCount();
         dstObjNode->recursivelyDecReferenceCount();
         node->setNumChildren(3);
         node->setArrayCopyElementType(elementType);
         vp->invalidateUseDefInfo();
         vp->invalidateValueNumberInfo();
         ///node->setReferenceArrayCopy(false, vp->comp());
         }
      }

   // If this is the 3-child version of arraycopy, see if the copy can be done
   // as a scalar copy
   //

   if (node->getNumChildren() == 3)
      {
      ListIterator<OMR::ValuePropagation::TR_TreeTopNodePair> treesIt1(&vp->_scalarizedArrayCopies);
      bool alreadyAdded = false;
      OMR::ValuePropagation::TR_TreeTopNodePair *scalarizedArrayCopy;
      for (scalarizedArrayCopy = treesIt1.getFirst();
           scalarizedArrayCopy; scalarizedArrayCopy = treesIt1.getNext())
         {
         if (scalarizedArrayCopy->_node == node)
            {
            alreadyAdded = true;
            break;
            }
         }

      if (!alreadyAdded)
         vp->_scalarizedArrayCopies.add(new (vp->comp()->trStackMemory()) OMR::ValuePropagation::TR_TreeTopNodePair(vp->_curTree, node));
      }
   else
      {
      vp->createExceptionEdgeConstraints(TR::Block::CanCatchArrayStoreCheck, NULL, node);
      }

   return node;
   }

TR::Node *constrainIntegralToBCD(OMR::ValuePropagation *vp, TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   constrainChildren(vp, node);

   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node->getFirstChild(), isGlobal);

   int32_t prec = TR_MAX_DECIMAL_PRECISION;

   if (constraint)
      prec = constraint->getPrecision();
   else
      {
      if (node->getFirstChild()->getType().isInt16())
         prec = std::min(prec, getPrecisionFromValue(TR::getMaxSigned<TR::Int16>()));
      else if (node->getFirstChild()->getType().isInt32())
         prec = std::min(prec, getPrecisionFromValue(TR::getMaxSigned<TR::Int32>()));
      else if (node->getFirstChild()->getType().isInt64())
         prec = std::min(prec, getPrecisionFromValue(TR::getMaxSigned<TR::Int64>()));
      }

   if (prec > node->getSourcePrecision())
      return node;

   if (performTransformation(vp->comp(), "%sSetting source precision on node %s [0x%x] to %d\n",OPT_DETAILS, node->getOpCode().getName(), node, prec))
      node->setSourcePrecision(prec);

#endif
   return node;
   }

TR::Node *constrainBCDToIntegral(OMR::ValuePropagation *vp, TR::Node *node)
   {
#ifdef J9_PROJECT_SPECIFIC
   constrainChildren(vp, node);

   bool isGlobal;
   TR::VPConstraint *constraint = vp->getConstraint(node, isGlobal);

   int32_t p = node->getFirstChild()->getDecimalPrecision();
   int64_t lo, hi;

   if (node->getType().isInt64())
    {
    // If a node has precision < the max precision (for its data type) and has the isNonNegative (x >= 0) flag set,
    // we know that it can't possibly be represented as a negative number, so the min value for the constraint must be at least 0
    if (p < node->getMaxIntegerPrecision() && node->isNonNegative())
        constrainRangeByPrecision(TR::getMinSigned<TR::Int64>(), TR::getMaxSigned<TR::Int64>(), p, lo, hi, true);
      else
        constrainRangeByPrecision(TR::getMinSigned<TR::Int64>(), TR::getMaxSigned<TR::Int64>(), p, lo, hi);
      }
   else
    {
    if (p < node->getMaxIntegerPrecision() && node->isNonNegative())
        constrainRangeByPrecision(TR::getMinSigned<TR::Int32>(), TR::getMaxSigned<TR::Int32>(), p, lo, hi, true);
      else
        constrainRangeByPrecision(TR::getMinSigned<TR::Int32>(), TR::getMaxSigned<TR::Int32>(), p, lo, hi);
      }

   if (node->getType().isInt64())
      constraint = TR::VPLongRange::create(vp,lo,hi);
   else
      constraint = TR::VPIntRange::create(vp,lo,hi);

   if (constraint)
      {
      vp->addBlockOrGlobalConstraint(node, constraint ,isGlobal);
      checkForNonNegativeAndOverflowProperties(vp, node, constraint);
      }

#endif
   return node;
   }
