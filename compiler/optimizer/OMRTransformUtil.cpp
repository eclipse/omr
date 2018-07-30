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

#include "optimizer/TransformUtil.hpp"
#include "compile/Compilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "env/CompilerEnv.hpp"

#define OPT_DETAILS "O^O TRANSFORMUTIL: "

TR::TransformUtil *
OMR::TransformUtil::self()
   {
   return static_cast<TR::TransformUtil *>(this);
   }

TR::Node *
OMR::TransformUtil::scalarizeArrayCopy(
      TR::Compilation *comp,
      TR::Node *node,
      TR::TreeTop *tt,
      bool useElementType,
      bool &didTransformArrayCopyNode,
      TR::SymbolReference *sourceRef,
      TR::SymbolReference *targetRef,
      bool castToIntegral)
   {
   TR::CodeGenerator *cg = comp->cg();

   didTransformArrayCopyNode = false;

   if ((comp->getOptLevel() == noOpt) ||
       !comp->getOptions()->getOption(TR_ScalarizeSSOps) ||
       node->getOpCodeValue() != TR::arraycopy ||
       node->getNumChildren() != 3 ||
       comp->requiresSpineChecks() ||
       !node->getChild(2)->getOpCode().isLoadConst() ||
       cg->getOptimizationPhaseIsComplete())
      return node;

   int64_t byteLen = node->getChild(2)->get64bitIntegralValue();
   if (byteLen == 0)
      {
      if (tt)
         {
         // Anchor the first two children
         if (!node->getFirstChild()->safeToDoRecursiveDecrement())
            TR::TreeTop::create(comp, tt->getPrevTreeTop(),
                               TR::Node::create(TR::treetop, 1, node->getFirstChild()));
         if (!node->getSecondChild()->safeToDoRecursiveDecrement())
            TR::TreeTop::create(comp, tt->getPrevTreeTop(),
                               TR::Node::create(TR::treetop, 1, node->getSecondChild()));
         tt->getPrevTreeTop()->join(tt->getNextTreeTop());
         tt->getNode()->recursivelyDecReferenceCount();
         didTransformArrayCopyNode = true;
         }
      return node;
      }
   else if (byteLen < 0)
      {
      return node;
      }
   else if (byteLen > TR_MAX_OTYPE_SIZE)
      {
      return node;
      }
   TR::DataType dataType = TR::Aggregate;

   // Get the element datatype from the (hidden) 4th child
   TR::DataType elementType = node->getArrayCopyElementType();
   int32_t elementSize = TR::Symbol::convertTypeToSize(elementType);

   if (byteLen == elementSize)
      {
      dataType = elementType;
      }
   else if (!useElementType)
      {
      switch (byteLen)
         {
         case 1: dataType = TR::Int8; break;
         case 2: dataType = TR::Int16; break;
         case 4: dataType = TR::Int32; break;
         case 8: dataType = TR::Int64; break;
         }
      }
   else
      {
      return node;
      }

   // load/store double on 64-bit PPC requires offset to be word aligned
   // abort if this requirement is not met.
   // TODO: also need to check if the first two children are aload nodes
   bool cannot_use_load_store_long = false;
   if (TR::Compiler->target.cpu.isPower())
      if (dataType == TR::Int64 && TR::Compiler->target.is64Bit())
         {
         TR::Node * firstChild = node->getFirstChild();
         if (firstChild->getNumChildren() == 2)
            {
            TR::Node *offsetChild = firstChild->getSecondChild();
            TR_ASSERT(offsetChild->getOpCodeValue() != TR::iconst, "iconst shouldn't be used for 64-bit array indexing");
            if (offsetChild->getOpCodeValue() == TR::lconst)
               {
               if ((offsetChild->getLongInt() & 0x3) != 0)
                  cannot_use_load_store_long = true;
               }
            }
         TR::Node *secondChild = node->getSecondChild();
         if (secondChild->getNumChildren() == 2)
            {
            TR::Node *offsetChild = secondChild->getSecondChild();
            TR_ASSERT(offsetChild->getOpCodeValue() != TR::iconst, "iconst shouldn't be used for 64-bit array indexing");
            if (offsetChild->getOpCodeValue() == TR::lconst)
               {
               if ((offsetChild->getLongInt() & 0x3) != 0)
                  cannot_use_load_store_long = true;
               }
            }
         }
   if (cannot_use_load_store_long) return node;

   TR::SymbolReference *nodeRef;

   targetRef = comp->getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(0);
   sourceRef = targetRef;

   bool trace = comp->getOption(TR_TraceScalarizeSSOps);
   if (trace)
      traceMsg(comp,"scalarizeArrayCopy: node %p got targetRef (#%d) and sourceRef (#%d)\n",
         node,targetRef?targetRef->getReferenceNumber():-1,sourceRef?sourceRef->getReferenceNumber():-1);

   if (targetRef == NULL || sourceRef == NULL)
      {
      if (trace)
         traceMsg(comp,"do not scalarizeArrayCopy node %p : targetRef is NULL (%s) or sourceRef is NULL (%s)\n",node,targetRef?"no":"yes",sourceRef?"no":"yes");
      return node;
      }
#ifdef J9_PROJECT_SPECIFIC
   if (targetRef->getSymbol()->getDataType().isBCD() ||
       sourceRef->getSymbol()->getDataType().isBCD())
      {
      return node;
      }
#endif

   if (performTransformation(comp, "%sScalarize arraycopy 0x%p\n", OPT_DETAILS, node))
      {
      TR::Node *store = TR::TransformUtil::scalarizeAddressParameter(comp, node->getSecondChild(), byteLen, dataType, targetRef, true);
      TR::Node *load = TR::TransformUtil::scalarizeAddressParameter(comp, node->getFirstChild(), byteLen, dataType, sourceRef, false);

      if (tt)
         {
         // Transforming
         //    treetop
         //      arrayCopy   <-- node
         // into
         //    *store
         //
         node->recursivelyDecReferenceCount();
         tt->setNode(node);
         }
      else
         {
         for (int16_t c = node->getNumChildren() - 1; c >= 0; c--)
            cg->recursivelyDecReferenceCount(node->getChild(c));
         }

      TR::Node::recreate(node, store->getOpCodeValue());
      node->setSymbolReference(store->getSymbolReference());

      if (store->getOpCode().isStoreIndirect())
         {
         node->setChild(0, store->getFirstChild());
         node->setAndIncChild(1, load);
         node->setNumChildren(2);
         }
      else
         {
         node->setAndIncChild(0, load);
         node->setNumChildren(1);
         }

      didTransformArrayCopyNode = true;
      }

   return node;
   }


TR::Node *
OMR::TransformUtil::scalarizeAddressParameter(
      TR::Compilation *comp,
      TR::Node *address,
      size_t byteLengthOrPrecision, // precision for BCD types and byteLength for all other types
      TR::DataType dataType,
      TR::SymbolReference *ref,
      bool store)
   {
   TR_ASSERT(ref,"symRef should not be NULL in scalarizeAddressParameter for address node %p\n",address);

   TR::Node * loadOrStore = NULL;

#ifdef J9_PROJECT_SPECIFIC
   size_t byteLength = dataType.isBCD() ? TR::DataType::getSizeFromBCDPrecision(dataType, byteLengthOrPrecision) : byteLengthOrPrecision;
#else
   size_t byteLength = byteLengthOrPrecision;
#endif

   bool isLengthValidForDirect = false;
   if (address->getOpCodeValue() == TR::loadaddr &&
       address->getOpCode().hasSymbolReference() &&
       address->getSymbolReference() &&
       !address->getSymbol()->isStatic())
      {
      if (byteLength == address->getSymbol()->getSize())
         isLengthValidForDirect = true;
      }

   if (address->getOpCodeValue() == TR::loadaddr &&
            !address->getSymbol()->isStatic() &&
            isLengthValidForDirect &&
            address->getSymbolReference() == ref &&
            ref->getSymbol()->getDataType() == dataType)
      {
      if (comp->getOption(TR_TraceScalarizeSSOps))
         traceMsg(comp,"\n\tscalarizeAddressParameter auto direct case: address %p, dt %s\n",address,dataType.toString());

      TR::ILOpCodes opcode = store ? comp->il.opCodeForDirectStore(dataType)
                                  : comp->il.opCodeForDirectLoad(dataType);

      loadOrStore = TR::Node::create(address, opcode, store ? 1 : 0);
      loadOrStore->setSymbolReference(ref);
      }
   else
      {
      TR::ILOpCodes opcode = store ? comp->il.opCodeForIndirectArrayStore(dataType)
                                  : comp->il.opCodeForIndirectArrayLoad(dataType);

      loadOrStore = TR::Node::create(address, opcode, store ? 2 : 1);
      loadOrStore->setSymbolReference(ref);
      loadOrStore->setAndIncChild(0, address);
      }

   if (byteLength == 8)
      {
      comp->getJittedMethodSymbol()->setMayHaveLongOps(true);
      }
#ifdef J9_PROJECT_SPECIFIC
   if (loadOrStore->getType().isBCD())
      {
      loadOrStore->setDecimalPrecision(byteLengthOrPrecision);
      }
   else
#endif
      if (!store && loadOrStore->getType().isIntegral() && !loadOrStore->getType().isInt64())
      {
      loadOrStore->setUnsigned(true);
      }

   return loadOrStore;
   }


uint32_t OMR::TransformUtil::_widthToShift[] = { 0, 0, 1, 0, 2, 0, 0, 0, 3};

TR::Node *
OMR::TransformUtil::transformIndirectLoad(TR::Compilation *, TR::Node *node)
   {
   return NULL;
   }

bool
OMR::TransformUtil::isNoopConversion(TR::Compilation *comp, TR::Node *node)
   {
   if (node->getOpCodeValue() == TR::i2a &&
       node->getSize() == 4)
      return true;
   if (node->getOpCodeValue() == TR::a2i &&
       node->getFirstChild()->getSize() == 4)
      return true;
   if (node->getOpCodeValue() == TR::l2a &&
       node->getSize() == 8)
      return true;
   if (node->getOpCodeValue() == TR::a2l &&
       node->getFirstChild()->getSize() == 8)
      return true;

   // address truncations
   if (node->getOpCodeValue() == TR::lu2a &&
       node->getSize() <= 8)
      return true;
   if (node->getOpCodeValue() == TR::iu2a &&
       node->getSize() <= 4)
      return true;
   if (node->getOpCodeValue() == TR::su2a &&
       node->getSize() <= 2)
      return true;

   return false;
   }

// Set the visit count for the given node and its subtree
//
void
OMR::TransformUtil::recursivelySetNodeVisitCount(TR::Node *node, vcount_t visitCount)
   {
   node->decFutureUseCount();
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      TR::TransformUtil::recursivelySetNodeVisitCount(child, visitCount);
      }
   }


bool
OMR::TransformUtil::transformDirectLoad(TR::Compilation *, TR::Node *node)
   {
   return false;
   }


bool
OMR::TransformUtil::transformIndirectLoadChain(
      TR::Compilation *comp,
      TR::Node *node,
      TR::Node *baseExpression,
      TR::KnownObjectTable::Index baseKnownObject,
      TR::Node **removedNode)
   {
   return false;
   }


bool
OMR::TransformUtil::transformIndirectLoadChainAt(
      TR::Compilation *comp,
      TR::Node *node,
      TR::Node *baseExpression,
      uintptrj_t *baseReferenceLocation,
      TR::Node **removedNode)
   {
   return false;
   }


bool
OMR::TransformUtil::fieldShouldBeCompressed(TR::Node *node, TR::Compilation *comp)
   {
   return false;
   }

void
OMR::TransformUtil::removeTree(TR::Compilation *comp, TR::TreeTop * tt)
   {
   comp->getJittedMethodSymbol()->removeTree(tt);
   }

void
OMR::TransformUtil::transformCallNodeToPassThrough(TR::Optimization* opt, TR::Node* node, TR::TreeTop * anchorTree, TR::Node* child)
   {
   opt->anchorAllChildren(node, anchorTree);
   node->removeAllChildren();
   node = TR::Node::recreateWithoutProperties(node, TR::PassThrough, 1, child);
   }
