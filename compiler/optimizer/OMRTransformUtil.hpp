/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_TRANSFORMUTIL_INCL
#define OMR_TRANSFORMUTIL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TRANSFORMUTIL_CONNECTOR
#define OMR_TRANSFORMUTIL_CONNECTOR
namespace OMR { class TransformUtil; }
namespace OMR { typedef OMR::TransformUtil TransformUtilConnector; }
#endif

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "env/KnownObjectTable.hpp"
#include "optimizer/Optimization.hpp"

namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class TreeTop; }
namespace TR { class SymbolReference; }
namespace TR { class TransformUtil; }

namespace OMR
{

class OMR_EXTENSIBLE TransformUtil
   {
   public:

   TR_ALLOC(TR_Memory::Optimizer)

   TransformUtil() {}

   TR::TransformUtil * self();

   static TR::Node *scalarizeArrayCopy(
         TR::Compilation *comp,
         TR::Node *node,
         TR::TreeTop *tt,
         bool useElementType,
         bool &didTransformArrayCopy,
         TR::SymbolReference *sourceRef = NULL,
         TR::SymbolReference *targetRef = NULL,
         bool castToIntegral = true);

   static TR::Node *scalarizeAddressParameter(
         TR::Compilation *comp,
         TR::Node *address,
         size_t byteLengthOrPrecision,
         TR::DataType dataType,
         TR::SymbolReference *ref,
         bool store);

   static uint32_t convertWidthToShift(int32_t i) { return _widthToShift[i]; }
   
   static bool isNoopConversion(TR::Compilation *, TR::Node *node);
   
   static void recursivelySetNodeVisitCount(TR::Node *node, vcount_t);
   
   static TR::Node *transformIndirectLoad(TR::Compilation *, TR::Node *node);

   static bool transformDirectLoad(TR::Compilation *, TR::Node *node);

   // These change node in-place.  If removedNode node ends up non-null, it's a
   // node that has been unhooked from the trees, so the caller is responsible
   // for decrementing its refcount (probably recursively) and for anchoring it
   // as necessary.
   //
   static bool transformIndirectLoadChain(TR::Compilation *, TR::Node *node, TR::Node *baseExpression, TR::KnownObjectTable::Index baseKnownObject, TR::Node **removedNode);

   static bool transformIndirectLoadChainAt(TR::Compilation *, TR::Node *node, TR::Node *baseExpression, uintptr_t *baseReferenceLocation, TR::Node **removedNode);

   static bool fieldShouldBeCompressed(TR::Node *node, TR::Compilation *comp);

   static void removeTree(TR::Compilation *, TR::TreeTop * tt);

   /**
    * \brief
    *    This function serves as a tool to transform a call node to a TR::PassThrough node with one child in place
    *    as a safe way to eliminate the call while preserving a valid tree shape for potential null check.
    *
    * \parm opt
    *    The optimization asking for this transformation.
    *
    * \parm node
    *    The call node to be transformed.
    *
    * \parm anchorTree
    *    The tree before which the children of the call node are to be anchored.
    *
    * \parm child
    *    The child for node after the transformation.
    *
    * \note
    *    The child node might be used as the target of the potential null check or the result of the call that is
    *    used elsewhere. The caller has to be aware of the two potential uses of child node and to avoid having an
    *    invalid tree after transformation.
    */
   static void transformCallNodeToPassThrough(TR::Optimization* opt, TR::Node* node, TR::TreeTop * anchorTree, TR::Node* child);

   /**
    *    \brief
    *       Create a conditional alternate implementation path for existing operations. Two blocks will be inserted, one
    *       block with comparison tree (compareBlock) and one block to take when comparison succeeds (ifBlock). The
    *       resulting CFG will look like
    *
    *          elseBlock               ifBlock
    *              |                   /     \
    *             ...      ===>       /       \
    *              |               elseBlock  thenBlock
    *          mergeBlock             |        /
    *                                ...      /
    *                                  \     /
    *                                   \   /
    *                                 mergeBlock
    *
    *       In the form of trees, this API converts
    *
    *       start block_A
    *          <tree sequence 1>
    *       end block_A
    *
    *          ...
    *       start block_B
    *          <tree sequence 2>
    *       end block_B
    *
    *       to:
    *
    *       start block_A
    *          if <cond> goto block_D   // ---> ifTree
    *       end block_A
    *
    *       start block_C
    *          <tree sequence 1>
    *       end block_C
    *
    *          ...
    *
    *       start block_B
    *          <tree sequence 2>
    *       end block_B
    *
    *       start block_D
    *          thenTree
    *          goto block_B
    *       end block_D
    *
    *   \param this                  the block to be used as the else block
    *   \param ifTree                the if tree to use in constructing the conditional blocks and cfg
    *   \param thenTree              the tree to use as the branch target of ifTree
    *   \param elseBlock             the block to enter when ifTree fails
    *   \param mergeBlock            the block that the alternate path is going to merge to
    *   \param cfg                   the cfg to update with the new information
    *   \parm markCold               whether to mark the if block cold
    *   \note
    *      After transformation, the original elseBlock will contain only one tree, that is the ifTree. Its original trees
    *      will be in a newly created block which is its fall through block after the transformation.
    */
   static void createConditionalAlternatePath(TR::Compilation* comp,
                                              TR::TreeTop *ifTree,
                                              TR::TreeTop *thenTree,
                                              TR::Block* elseBlock,
                                              TR::Block* mergeBlock,
                                              TR::CFG *cfg,
                                              bool markCold = true);

#if defined(OMR_GC_SPARSE_HEAP_ALLOCATION)
   /**
    * \brief
    *    Generate IL to load dataAddr pointer from the array header
    *
    * \param comp
    *    The compilation object
    *
    * \param arrayObject
    *    The array object node
    *
    * \return
    *    IL for loading dataAddr pointer
    */
   static TR::Node *generateDataAddrLoadTrees(TR::Compilation *comp, TR::Node *arrayObject);
#endif /* OMR_GC_SPARSE_HEAP_ALLOCATION */

   /**
    * \brief
    *    Generate array element access IL for on and off heap contiguous arrays
    *
    * \param comp
    *    The compilation object
    *
    * \param arrayNode
    *    The array object node
    *
    * \param offsetNode
    *    The offset node (in bytes)
    *
    * \return
    *    IL to access array element at offset provided by offsetNode or
    *    first array element if no offset node is provided
    */
   static TR::Node *generateArrayElementAddressTrees(
      TR::Compilation *comp,
      TR::Node *arrayNode,
      TR::Node *offsetNode = NULL);

   /**
    * \brief
    *    Generate IL to access first array element
    *
    * \param comp
    *    The compilation object
    *
    * \param arrayObject
    *    The array object node
    *
    * \return
    *    IL for accessing first array element
    */
   static TR::Node *generateFirstArrayElementAddressTrees(TR::Compilation *comp, TR::Node *arrayObject);

   /**
    * \brief
    *    Generates IL to convert element index to offset in bytes using element
    *    size (node or integer)
    *
    * \param comp
    *    The compilation object
    *
    * \param indexNode
    *    Array element index node
    *
    * \param elementSizeNode
    *    Array element size tree (ex: const node containing element size)
    *
    * \param elementSize
    *    Array element size
    *
    * \param useShiftOpCode
    *    Use left shift instead of multiplication
    *
    * \return
    *    IL to convert array element index to offset in bytes
    */
   static TR::Node *generateConvertArrayElementIndexToOffsetTrees(
      TR::Compilation *comp,
      TR::Node *indexNode,
      TR::Node *elementSizeNode = NULL,
      int32_t elementSize = 0,
      bool useShiftOpCode = false);

   private:

   static uint32_t _widthToShift[];

   };

}

#endif
