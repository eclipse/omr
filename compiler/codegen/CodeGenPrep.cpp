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

/**
 * Support code for TR::CodeGenerator.  Code related to preparing the IL
 * before actual code generation should go in this file.
 */

#include "codegen/CodeGenerator.hpp" // IWYU pragma: keep

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>                           // For std::find
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/OSRData.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "il/AliasSetInterface.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/TypeLayout.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/IL.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/HashTab.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Structure.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "ras/Delimiter.hpp"
#include "runtime/Runtime.hpp"

#define OPT_DETAILS "O^O CODE GENERATION: "

void
OMR::CodeGenerator::lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {
   self()->lowerTreesPropagateBlockToNode(parent);

   static const char * disableILMulPwr2Opt = feGetEnv("TR_DisableILMulPwr2Opt");

   if (!disableILMulPwr2Opt &&
       ((parent->getOpCodeValue() == TR::imul) || (parent->getOpCodeValue() == TR::lmul)) &&
       performTransformation(self()->comp(), "%sPwr of 2 mult opt node %p\n", OPT_DETAILS, parent) )
      {
      TR::Node * firstChild = parent->getFirstChild();
      TR::Node * secondChild = parent->getSecondChild();

      if (secondChild->getOpCode().isLoadConst())
         {
         bool is32BitOp = parent->getOpCode().is4Byte();
         int64_t value = is32BitOp ? secondChild->getInt() : secondChild->getLongInt();

         int32_t shftAmnt = TR::TreeEvaluator::checkPositiveOrNegativePowerOfTwo(value);
         if (shftAmnt > 0)
            {
            if (value > 0)
               {
               if (secondChild->getReferenceCount()==1)
                  {
                  if (is32BitOp)
                     {
                     TR::Node::recreate(parent, TR::ishl);
                     }
                  else
                     {
                     TR::Node::recreate(secondChild, TR::iconst);
                     TR::Node::recreate(parent, TR::lshl);
                     }
                  secondChild->setInt(shftAmnt);
                  }
               else if (secondChild->getReferenceCount()>1)
                  {
                  TR::Node * newChild = TR::Node::create(parent, TR::iconst, 0, shftAmnt);
                  parent->getSecondChild()->decReferenceCount();
                  parent->setSecond(newChild);
                  parent->getSecondChild()->incReferenceCount();
                  TR::Node::recreate(parent, is32BitOp ? TR::ishl : TR::lshl);
                  }
               }
            else //negative value of the multiply constant
               {
               if (secondChild->getReferenceCount() == 1)
                  {
                  TR::Node * newChild = TR::Node::create(parent, is32BitOp ? TR::ishl : TR::lshl, 2);;

                  newChild->setVisitCount(parent->getVisitCount());
                  newChild->incReferenceCount();
                  newChild->setFirst(firstChild);
                  newChild->setSecond(secondChild);
                  if (is32BitOp)
                     {
                     TR::Node::recreate(parent, TR::ineg);
                     }
                  else
                     {
                     TR::Node::recreate(secondChild, TR::iconst);
                     TR::Node::recreate(parent, TR::lneg);
                     }
                  secondChild->setInt(shftAmnt);
                  parent->setNumChildren(1);
                  parent->setFirst(newChild);
                  }
               else if (secondChild->getReferenceCount() > 1)
                  {
                  TR::Node * newChild = TR::Node::create(parent, TR::iconst, 0, shftAmnt);
                  TR::Node * newChild2 = TR::Node::create(parent, is32BitOp ? TR::ishl : TR::lshl, 2);
                  newChild2->setFirst(parent->getFirstChild());
                  newChild2->setSecond(newChild);
                  newChild2->getFirstChild()->incReferenceCount();
                  newChild2->getSecondChild()->incReferenceCount();
                  parent->getFirstChild()->decReferenceCount();
                  parent->getSecondChild()->decReferenceCount();
                  parent->setNumChildren(1);
                  TR::Node::recreate(parent, is32BitOp ? TR::ineg : TR::lneg);
                  parent->setFirst(newChild2);
                  parent->getFirstChild()->incReferenceCount();
                  }
               }
            }
         }
      }
   }

/**
 * @brief Lower newvalue into a new
 *
 * The `newvalue` opcode is primarely useful to the optimizer for
 * representing fused allocation an initialization. However, it's
 * functionality can also be implemented using the `new` opcode
 * (although doing so makes analyses more difficult in the optimizer).
 *
 * This function takes a `newvalue` tree and lowers it to an equivalent
 * sequence of trees using a `new` opcode. For example, the following
 * tree:
 *
 *    treetop
 *      newvalue jitNewValueObject (identityless)
 *        loadaddr Vec3
 *        fload x
 *        fload y
 *        fload z
 *
 * will turned into:
 *
 *    treetop
 *      fload x
 *    treetop
 *      fload y
 *    treetop
 *      fload z
 *    treetop
 *      new jitNewValueObject (skipZeroInit)
 *        loadaddr Vec3
 *    istorei Vec3.x
 *      ==>new
 *      ==>fload x
 *    istorei Vec3.y
 *      ==>new
 *      ==>fload y
 *    istorei Vec3.z
 *      ==>new
 *      ==>fload z
 *    fence
 *
 * Note that the (helper) symbol reference for the `new` node is be
 * the same as the symbol reference for the `newvalue` node.
 *
 * The fence at the end is needed for platforms with weak memory ordering
 * such as POWER. It may appear anywhere after allocation + initialization
 * but before any operation that could publish the resulting reference to
 * other threads.
 *
 * @param comp pointer to the compilation object
 * @param node the node being lowered
 * @param tt the TreeTop anchoring the node
 */
static void
lowerNewValue(TR::Compilation *comp, TR::Node *node, TR::TreeTop *tt)
   {
   // Transmute newvalue node into new.
   // Importantly, the helper symref of the newvalue node is preserved.
   TR::Node::recreate(node, TR::New);
   node->setCanSkipZeroInitialization(true);
   node->setIdentityless(false);

   auto* valueClass = static_cast<TR_OpaqueClassBlock *>(node->getFirstChild()->getSymbol()->getStaticSymbol()->getStaticAddress());
   const TR::TypeLayout* typeLayout = comp->typeLayout(valueClass);
   TR::IL il;

   // cursors to iterate over the treetops that compute field values and store those values into fields
   auto* fieldValueTreeTopCursor = tt->getPrevTreeTop();
   auto* fieldStoreTreeTopCursor = tt;

   // cache the treetop that must go at the end of the new sequence
   auto* nextTreeTop = tt->getNextTreeTop();

   // Iterate over very child that sets a field, anchoring computations
   // before the allocation node and anchoring stores to fields after.
   //
   // The first child is skipped as it's only used to specify the type
   // of the value being constructed. The second child is the one that
   // actually initializes the first field.
   for (int i = 1; i < node->getNumChildren(); ++i)
      {
      TR::Node* fieldValueNode = node->getChild(i);
      node->setChild(i, NULL);

      // anchor calculation of the field's value
      auto* ttNode = TR::Node::create(TR::treetop, 1);
      ttNode->setFirst(fieldValueNode);
      fieldValueTreeTopCursor->join(TR::TreeTop::create(comp, ttNode));
      fieldValueTreeTopCursor = fieldValueTreeTopCursor->getNextTreeTop();

      // generate store to the field
      const TR::TypeLayoutEntry& fieldEntry = typeLayout->entry(i - 1);
      auto* symref = comp->getSymRefTab()->findOrFabricateShadowSymbol(valueClass,
                                                                       fieldEntry._datatype,
                                                                       fieldEntry._offset,
                                                                       fieldEntry._isVolatile,
                                                                       fieldEntry._isPrivate,
                                                                       fieldEntry._isFinal,
                                                                       fieldEntry._fieldname,
                                                                       fieldEntry._typeSignature
                                                                       );
      const auto storeOpCode = il.opCodeForIndirectStore(fieldValueNode->getDataType());
      auto* storeNode = TR::Node::createWithSymRef(storeOpCode, 2, symref);
      storeNode->setAndIncChild(0, node);
      storeNode->setAndIncChild(1, fieldValueNode);
      fieldStoreTreeTopCursor->join(TR::TreeTop::create(comp, storeNode));
      fieldStoreTreeTopCursor = fieldStoreTreeTopCursor->getNextTreeTop();
      }
   node->setNumChildren(1);

   // link anchord values to allocation treetop
   fieldValueTreeTopCursor->join(tt);

   // add memory fence
   auto* allocFenceTreeTop = TR::TreeTop::create(comp, TR::Node::createAllocationFence(NULL, node));
   fieldStoreTreeTopCursor->join(allocFenceTreeTop);

   // finish by linking to the next TreeTop
   allocFenceTreeTop->join(nextTreeTop);
   }

void
OMR::CodeGenerator::lowerTreeIfNeeded(
      TR::Node *node,
      int32_t childNumberOfNode,
      TR::Node *parent,
      TR::TreeTop *tt)
   {
   if (node->getOpCodeValue() == TR::loadaddr && node->getOpCode().hasSymbolReference() && node->getSymbol()->isLabel())
      {
      node->getSymbol()->setHasAddrTaken();
      }

   if (node->getOpCodeValue() == TR::ldiv && self()->codegenSupportsUnsignedIntegerDivide())
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();
      TR::ILOpCodes firstChildOp = firstChild->getOpCodeValue();
      TR::ILOpCodes secondChildOp = secondChild->getOpCodeValue();

      if (firstChildOp == TR::iu2l && secondChildOp == TR::iu2l &&
          performTransformation(self()->comp(), "%sReduced ldiv with highWordZero children in node [%p] to idiv\n", OPT_DETAILS, node))
         {
         TR::Node *idivChild = TR::Node::create(TR::idiv, 2, firstChild->getFirstChild(), secondChild->getFirstChild());

         TR::Node::recreate(node, TR::iu2l);
         node->setNumChildren(1);
         node->setChild(0, idivChild);
         idivChild->incReferenceCount();
         idivChild->setUnsigned(true);

         firstChild->recursivelyDecReferenceCount();
         secondChild->recursivelyDecReferenceCount();

         node->setIsHighWordZero(true);
         }
      }

   if (self()->getLocalsIG() && self()->getSupportsCompactedLocals())
      {
      // compact locals might map 2 autos to the same slot,
      // in which case we can remove the tree of
      // store auto1 (colour x)
      //  load auto2 (colour x)
      if (node->getOpCode().isStoreDirect() &&
          node->getSymbolReference()->getSymbol()->isAuto() &&
          node->getFirstChild()->getReferenceCount() == 1 &&
          node->getFirstChild()->getOpCode().isLoadVarDirect() &&
          node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto())
         {
         TR::AutomaticSymbol *storeLocal =  node->getSymbolReference()->getSymbol()->castToAutoSymbol();
         TR::AutomaticSymbol *loadLocal = node->getFirstChild()->getSymbolReference()->getSymbol()->castToAutoSymbol();

         if(!storeLocal->holdsMonitoredObject() &&
            !storeLocal->isInternalPointer() &&
            !storeLocal->isPinningArrayPointer() &&
            !loadLocal->holdsMonitoredObject() &&
            !loadLocal->isInternalPointer() &&
            !loadLocal->isPinningArrayPointer())
            {
            TR_IGNode *storeIGNode = self()->getLocalsIG()->getIGNodeForEntity(storeLocal);
            TR_IGNode *loadIGNode = self()->getLocalsIG()->getIGNodeForEntity(loadLocal);

            if ((storeIGNode != NULL) && (loadIGNode != NULL))
               {
               IGNodeColour colourStore = storeIGNode->getColour();
               IGNodeColour colourLoad = loadIGNode->getColour();

               if (colourLoad == colourStore && colourLoad != UNCOLOURED &&
                   performTransformation(self()->comp(), "%sCoalescing locals by removing store tree %p, load = %d, store = %d \n", OPT_DETAILS, node, colourLoad, colourStore))
                  {
                  // if we are removing the initialization of the store local, it will become uninitialized
                  if (storeLocal->isInitializedReference() && !loadLocal->isInitializedReference())
                     {
                     storeLocal->setUninitializedReference();
                     }

                  // Update BitVector to mark that both locals are 'live', even if
                  // through coalescing, we removed all references to the locals.
                  // This will prevent removeUnusedLocals from removing the local
                  // and excluding it (incorrectly) from the stack atlas.
                  TR_BitVector * liveButMaybeUnreferencedLocals = self()->getLiveButMaybeUnreferencedLocals();
                  if (!liveButMaybeUnreferencedLocals)
                     {
                     int32_t numLocals = self()->comp()->getMethodSymbol()->getAutomaticList().getSize();
                     liveButMaybeUnreferencedLocals = new (self()->trHeapMemory()) TR_BitVector(numLocals, self()->trMemory());
                     self()->setLiveButMaybeUnreferencedLocals(liveButMaybeUnreferencedLocals);
                     }
                  if (!storeLocal->isLiveLocalIndexUninitialized())
                     liveButMaybeUnreferencedLocals->set(storeLocal->getLiveLocalIndex());
                  if (!loadLocal->isLiveLocalIndexUninitialized())
                     liveButMaybeUnreferencedLocals->set(loadLocal->getLiveLocalIndex());

                  // Remove this treetop
                  tt->getPrevTreeTop()->setNextTreeTop(tt->getNextTreeTop());
                  tt->getNextTreeTop()->setPrevTreeTop(tt->getPrevTreeTop());
                  tt->getNode()->recursivelyDecReferenceCount();
                  return;
                  }
               }
            }
         }
      }

   // Somewhat hacky, limited use.
   //
   if (node->getOpCode().isBranch())
      {
      TR::Node  * destinationNode  = node->getBranchDestination()->getNode();
      if (destinationNode)
         {
         TR::Block * destinationBlock = node->getBranchDestination()->getNode()->getBlock();

         if (destinationBlock && (destinationBlock->getVisitCount() == self()->comp()->getVisitCount()))
            self()->getCurrentBlock()->setBranchesBackwards();
         }
      }

   if (node->getOpCodeValue() == TR::BBStart && self()->comp()->getFlowGraph()->getStructure() != NULL)
      {
      TR::Block * block = node->getBlock();
      TR_Structure *blockStructure = block->getStructureOf();
      if (blockStructure)
         {
         TR_RegionStructure *region = (TR_RegionStructure*) blockStructure->getContainingLoop();
         if (region)
            {
            TR::Block *entry = region->getEntryBlock();
            entry->setFirstBlockInLoop();
            }
         }
      }
   else if (node->getOpCodeValue() == TR::BBEnd)
      {
      node->getBlock()->setVisitCount(self()->comp()->getVisitCount());
      }

   if (node->getOpCode().mustBeLowered())
      {
      if ((node->getOpCodeValue() == TR::athrow) && node->throwInsertedByOSR())
         {
         // Remove this treetop
         tt->getPrevTreeTop()->setNextTreeTop(tt->getNextTreeTop());
         tt->getNextTreeTop()->setPrevTreeTop(tt->getPrevTreeTop());
         node->recursivelyDecReferenceCount();
         }
      else
         self()->lowerTree(node, tt);
      }
   else if (node->getOpCodeValue() == TR::newvalue)
      {
      lowerNewValue(self()->comp(), node, tt);
      }

   if (node->getOpCodeValue() == TR::loadaddr || node->getOpCode().isLoadVarDirect())
     {
     if(node->getSymbol()->isParm())
       node->getSymbol()->setParmHasToBeOnStack();
     }
   else if (node->getOpCode().isStore() &&
       node->getSymbol()->isParm() && node->getSymbol()->isCollectedReference())
      node->getSymbol()->setParmHasToBeOnStack();

   if (node->getOpCode().isSelect())
      {
      self()->rematerializeCmpUnderSelect(node);
      }

   }

/**
 * Scan down the current subtree looking for any b2i or i2b (we know the parent is a [i]bstore).  If the
 * operations between the store and the convert cannot trigger an overflow or underflow of the byte value
 * ie consist only of bitwise and, or or negate, then we can consider the upper bits to be garbage as they
 * will be thrownaway as part of the store.  Thus, we can mark the convert an unneeded.  Codegen can then
 * avoid generating the associated code.
 */
void OMR::CodeGenerator::identifyUnneededByteConvNodes(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount, TR::DataType storeType)
   {
   parent->setVisitCount(visitCount);

   TR::ILOpCode &opCode = parent->getOpCode();

   // any other opcode could potentially cause over/underflow and change the lowest byte
   // if the b2i is noped.
   if (opCode.isConversion() ||
       (opCode.isAnd() &&
        parent->getSecondChild()->getOpCode().isLoadConst() &&
        parent->getSecondChild()->getType().isIntegral() &&
        parent->getSecondChild()->get64bitIntegralValue() >= 0 &&
        parent->getSecondChild()->get64bitIntegralValue() < 128)
        || opCode.isStore() ||
       opCode.isLoad() || ((parent == treeTop->getNode()) && opCode.isBooleanCompare()))
      {
      bool skipFirstChild = opCode.isIndirect();
      for (int32_t childCount = parent->getNumChildren() - 1; childCount >= 0; childCount--)
         {
         TR::Node *child = parent->getChild(childCount);

         // If the subtree needs to be lowered, call the VM to lower it
         //
         if (child->getVisitCount() != visitCount)
            {
            if (childCount==0 && skipFirstChild) continue;// skip address of indirect store

            TR::ILOpCodes childOp = child->getOpCodeValue();
            // we've found an i2b, and we're ultimately feeding a bstore,
            // so mark as a nop.  If it's shared, then unhook it from other
            // instances and share the children.
            // o-type is evaluated as passThrough... making this optimization invalid
            bool isOneByte = (storeType == TR::Int8
#ifdef J9_PROJECT_SPECIFIC
                              || (treeTop->getNode()->getOpCode().isBCDStore() && treeTop->getNode()->getSize() == 1)
#endif
                             )
                  && child->getOpCode().isConversion();
            bool isTwoByte = (storeType == TR::Int16
#ifdef J9_PROJECT_SPECIFIC
                              || (treeTop->getNode()->getOpCode().isBCDStore() && treeTop->getNode()->getSize() == 2)
#endif
                             )
                  && child->getOpCode().isConversion();
            if ((isOneByte &&
                (childOp == TR::i2b || childOp == TR::b2i || childOp == TR::bu2i)) ||
                (isTwoByte &&
                (childOp == TR::i2s || childOp == TR::s2i || childOp == TR::su2i)))
               {
               if (child->getReferenceCount() > 1 &&
                   (!treeTop->getNode()->getOpCode().isBooleanCompare() ||
                    (childOp != TR::bu2i && childOp != TR::su2i)) &&
                   performTransformation(self()->comp(), "%sReplacing shared i2b/b2i node %p\n", OPT_DETAILS, child))
                  {
                  TR::Node * newChild = TR::Node::create(childOp, 1, child->getFirstChild());
                  child->decReferenceCount();
                  parent->setAndIncChild(childCount,newChild);
                  child= newChild;
                  }
               if ((childOp == TR::b2i || childOp == TR::s2i) && treeTop->getNode()->getOpCode().isBooleanCompare() &&
                   (child->getFirstChild()->getOpCodeValue() == TR::s2b ||
                    child->getFirstChild()->getOpCodeValue() == TR::f2b ||
                    child->getFirstChild()->getOpCodeValue() == TR::d2b ||
                    child->getFirstChild()->getOpCodeValue() == TR::i2b ||
                    child->getFirstChild()->getOpCodeValue() == TR::l2b ||
                    child->getFirstChild()->getOpCodeValue() == TR::f2s ||
                    child->getFirstChild()->getOpCodeValue() == TR::d2s ||
                    child->getFirstChild()->getOpCodeValue() == TR::i2s ||
                    child->getFirstChild()->getOpCodeValue() == TR::l2s ||
                    child->getFirstChild()->getOpCodeValue() == TR::iRegLoad ||
                    child->getFirstChild()->getOpCodeValue() == TR::iuRegLoad ) &&
                   performTransformation(self()->comp(), "%sChanging b2i node %p to unsigned conversion\n", OPT_DETAILS, child))
                  {
                  TR::Node::recreate(child, (storeType == TR::Int8 ? TR::bu2i : TR::su2i));
                  }
               else if (performTransformation(self()->comp(), "%sMarking i2b/b2i node %p as unneeded\n", OPT_DETAILS, child))
                  child->setUnneededConversion(true);
               }

            if (child->getReferenceCount() == 1)
               self()->identifyUnneededByteConvNodes(child, treeTop, visitCount, storeType);

            }
         }
      }
   }

void OMR::CodeGenerator::identifyUnneededByteConvNodes()
   {
   vcount_t visitCount = self()->comp()->incVisitCount();

   TR::Block * block = NULL;
   TR::TreeTop * tt;
   TR::Node * node;
   TR::Block * firstColdBlock = NULL;

   if (!performTransformation(self()->comp(), "%s ===>   Identify and mark Unneeded b2i/i2b conversions  <===\n", OPT_DETAILS)) return;

   for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      node = tt->getNode();

      TR_ASSERT(node->getVisitCount() != visitCount, "Code Gen: error in marking b2i nodes");

      if ((
#ifdef J9_PROJECT_SPECIFIC
           (node->getType().isBCD() && node->getSize() <= 2) ||
#endif
           node->getOpCode().isByte() || node->getOpCode().isShort()) && node->getOpCode().isStore())
         {
         self()->identifyUnneededByteConvNodes(node, tt, visitCount, node->getDataType());
         }
      // Don't do conversion when consuming treetop is a compare == or != 0 to 128
      else if (node->getOpCode().isBooleanCompare() &&
               (TR::ILOpCode::isEqualCmp(node->getOpCode().getOpCodeValue()) ||
                TR::ILOpCode::isNotEqualCmp(node->getOpCode().getOpCodeValue())) &&
               node->getSecondChild()->getOpCode().isLoadConst() &&
               node->getSecondChild()->getType().isIntegral())
         {
         int64_t cmpValue = node->getSecondChild()->get64bitIntegralValue();
         if (cmpValue >= 0 && cmpValue < 0x80)
            {
            self()->identifyUnneededByteConvNodes(node, tt, visitCount, TR::Int8);
            }
         if (cmpValue >= 0 && cmpValue < 0x8000)
            {
            self()->identifyUnneededByteConvNodes(node, tt, visitCount, TR::Int16);
            }
         }
      }
   }

bool shouldResetRequiresConditionCodes(TR::Node *node)
   {
   if (!node->chkOpsNodeRequiresConditionCodes())
      return false;
   if (!node->nodeRequiresConditionCodes())
      return false;
   if (node->getOpCode().isArithmetic())
      return true;

   return false;
   }


// Pre-order traversal, remove the flags on the way in.
// On the way out set the flags on the first child of
// computeCC nodes.
//
void
OMR::CodeGenerator::cleanupFlags(TR::Node *node)
   {
   if (node->getVisitCount() == self()->comp()->getVisitCount())
      return;

   node->setVisitCount(self()->comp()->getVisitCount());

   // clean up any stale values that might be kicking around from previous phases
   if (shouldResetRequiresConditionCodes(node))
      node->setNodeRequiresConditionCodes(false);

   if (node->isAdjunct())
      node->setIsAdjunct(false);

   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      {
      TR::Node *child = node->getChild(i);
      self()->cleanupFlags(child);
      }

   if (node->getOpCodeValue() == TR::computeCC)
      {
      // implicitly uncommon child by increasing the reference count of its children
      // as though their parent had been uncommoned. This has the effect of keeping their registers alive
      // until the actual point of uncommoning (if in fact it is required).

      TR::Node *ccNode = node->getFirstChild();
      for (int32_t i = ccNode->getNumChildren()-1; i >= 0; --i)
         {
         ccNode->getChild(i)->incReferenceCount();
         }
      // do not set requires condition code flag, this will only be set when is about to be eval for CC node
      }

   // J9 (should only be true for J9)
   //
   if (node->isDualHigh())
      {
      TR_ASSERT(!node->getChild(2)->isAdjunct(), "Code Limitation: Cannot cope with common adjunct between dual highs. node = %p.", node);
      node->getChild(2)->setIsAdjunct(true);
      }
   }


void
OMR::CodeGenerator::addCountersToEdges(TR::Block *b)
   {
   TR::Node *lastNode = b->getLastRealTreeTop()->getNode();
   bool found = (std::find(_counterBlocks.begin(), _counterBlocks.end(), b) != _counterBlocks.end());
   if (lastNode->getOpCode().isBranch() && !found)
      {
      TR::Block *dest = lastNode->getBranchDestination()->getNode()->getBlock();
      TR::Block *fallthrough = b->getNextBlock();

      const char *c = TR::DebugCounter::debugCounterName(self()->comp(), "block_%d TAKEN", b->getNumber());

      if (c && self()->comp()->getOptions()->dynamicDebugCounterIsEnabled(c) && !(dest->getPredecessors().size() == 1) /* && !toBlock(relevantEdge->getFrom())->getSuccessors().isSingleton() */)
         {
         TR::Node *glRegDeps = NULL;
         if (dest->getEntry()->getNode()->getNumChildren() > 0)
            {
            glRegDeps = dest->getEntry()->getNode()->getFirstChild();
            TR_ASSERT(glRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");
            }

         TR::Block *newBlock = b->splitEdge(b, dest, self()->comp());

         traceMsg(self()->comp(), "\nSplitting edge, create new intermediate block_%d to add edge counters", newBlock->getNumber());
         if (glRegDeps)
            newBlock->takeGlRegDeps(self()->comp(), glRegDeps);

         dest = newBlock;
         _counterBlocks.push_front(newBlock);
         }

      TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "block_%d TAKEN", b->getNumber()), dest->getEntry()->getNextTreeTop());

      if (lastNode->getOpCode().isIf())
         {
         TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "block_%d NOT TAKEN", b->getNumber()), fallthrough->getEntry()->getNextTreeTop());
         }
      }
   }


void
OMR::CodeGenerator::insertDebugCounters()
   {
   TR::Block * block = NULL;
   TR::TreeTop * tt;
   TR::Node * node;

   for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      node = tt->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         self()->setCurrentBlock(block);

         if (self()->comp()->getOption(TR_EnableCFGEdgeCounters))
            self()->addCountersToEdges(block);

         if (block->isCold())
            {
            TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "blocks/%sCompiles/coldBlocks/=%d", self()->comp()->getHotnessName(self()->comp()->getMethodHotness()), block->getNumber()), tt->getNextTreeTop(), 1, TR::DebugCounter::Free);
            TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "coldBlocks/byJittedBody/(%s)/%s/=%d", self()->comp()->signature(), self()->comp()->getHotnessName(self()->comp()->getMethodHotness()), block->getNumber()), tt->getNextTreeTop(), 1, TR::DebugCounter::Free);
            }
         else
            {
            TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "blocks/%sCompiles/warmBlocks/=%d", self()->comp()->getHotnessName(self()->comp()->getMethodHotness()), block->getNumber()), tt->getNextTreeTop());
            TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "warmBlocks/byJittedBody/(%s)/%s/=%d", self()->comp()->signature(), self()->comp()->getHotnessName(self()->comp()->getMethodHotness()), block->getNumber()), tt->getNextTreeTop());
            }

         // create a counter for switch targets that have one predecessor
         if (block->getPredecessors().size() == 1)
            {
            TR::Block *predBlock = block->getPredecessors().front()->getFrom()->asBlock();
            if (predBlock->getEntry() && predBlock->getExit())
               {
               TR::Node* predNode = predBlock->getLastRealTreeTop()->getNode();
               if (predNode->getOpCode().isSwitch())
                  {
                  const char *opName = predNode->getOpCode().getName();
                  TR::DebugCounter::prependDebugCounter
                     (self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "branchtargets/%s/(%s)/%s/%d/%d",
                  opName,
                  self()->comp()->signature(),
                  self()->comp()->getHotnessName(self()->comp()->getMethodHotness()),
                  predNode->getByteCodeIndex(),
                  tt->getNode()->getByteCodeIndex()),
                     tt->getNextTreeTop());
                  }
               }
            }
         }

      // branch IL counter
      if (node->getOpCode().isBranch() && !node->getOpCode().isSwitch())
         {
         const char *opName = tt->getNode()->getOpCode().getName();
         TR::DebugCounter::prependDebugCounter
            (self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "branches/%s/(%s)/%s/%d",
               opName,
               self()->comp()->signature(),
               self()->comp()->getHotnessName(self()->comp()->getMethodHotness()),
               tt->getNode()->getByteCodeIndex()),
            tt);
         }

      if (node->getOpCode().isNew())
         {
         const char *opName = node->getOpCode().getName();
         if (node->getOpCodeValue() == TR::New || node->getOpCodeValue() == TR::anewarray)
            {
            TR::Node *classChild = node->getChild(node->getNumChildren()-1);
            if (classChild->getOpCodeValue() != TR::loadaddr)
               {
               TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(),
                  "allocations/%s/child-%s", opName, classChild->getOpCode().getName()
                  ), tt);
               }
            else if (classChild->getSymbolReference()->isUnresolved())
               {
               TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(),
                  "allocations/%s/unresolved", opName, classChild->getOpCode().getName()
                  ), tt);
               }
            else
               {
               int32_t length;
               char *className = TR::Compiler->cls.classNameChars(self()->comp(), (TR_OpaqueClassBlock*)classChild->getSymbol()->castToStaticSymbol()->getStaticAddress(), length);
               TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(),
                  "allocations/%s/(%.*s)", opName, length, className
                  ), tt);
               }
            }
         else
            {
            //newarray
            int32_t type = node->getChild(node->getNumChildren()-1)->getInt();
            char typeName[30];
            switch (type)
               {
               case 4:
                  sprintf(typeName, "boolean");
                  break;
               case 8:
                  sprintf(typeName, "byte");
               case 5:
                  sprintf(typeName, "char");
                  break;
               default:
                  sprintf(typeName, "non-char");
                  break;
               }

            TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "allocations/%s/%s", opName, typeName), tt);
            }
         }
      }
   }





void OMR::CodeGenerator::rematerializeCmpUnderSelect(TR::Node *node)
   {
   if (node->getFirstChild()->getOpCode().isBooleanCompare() && node->getFirstChild()->getReferenceCount() > 1)
      {
      TR::Node *replacement = TR::Node::copy(node->getFirstChild());
      replacement->setReferenceCount(0);

      node->getFirstChild()->decReferenceCount();
      node->setAndIncChild(0,replacement);

      replacement->getFirstChild()->incReferenceCount();
      replacement->getSecondChild()->incReferenceCount();
      }
   }
