/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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

#include <stdint.h>               // for int32_t, uint8_t, uint32_t
#include "env/TRMemory.hpp"       // for TR_Memory, etc
#include "il/DataTypes.hpp"       // for DataTypes
#include "il/Node.hpp"            // for Node, vcount_t
#include "env/KnownObjectTable.hpp"

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

   static bool transformIndirectLoadChainAt(TR::Compilation *, TR::Node *node, TR::Node *baseExpression, uintptrj_t *baseReferenceLocation, TR::Node **removedNode);

   static bool fieldShouldBeCompressed(TR::Node *node, TR::Compilation *comp);

   static void removeTree(TR::Compilation *, TR::TreeTop * tt);
   
   private:

   static uint32_t _widthToShift[];

   };

}

#endif
