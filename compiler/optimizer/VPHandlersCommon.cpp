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

#include <stdint.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/TRMemory.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "runtime/Runtime.hpp"
#include "optimizer/TransformUtil.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#include "optimizer/VPBCDConstraint.hpp"
#endif

class TR_OpaqueClassBlock;

#define OPT_DETAILS "O^O VALUE PROPAGATION: "

TR::Node *constrainCall(OMR::ValuePropagation *vp, TR::Node *node);

// **************************************************************************
//
// Node processing routines for Value Propagation
//
// **************************************************************************

// Default handler - don't do anything with this node, but visit the children
// to see if they need processing
//
TR::Node *constrainChildren(OMR::ValuePropagation *vp, TR::Node *node)
   {
   TR::Node *myParent = vp->getCurrentParent();
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      vp->setCurrentParent(node);
      vp->launchNode(node->getChild(i), node, i);
      }
   vp->setCurrentParent(myParent);
   return node;
   }

TR::Node *constrainChildrenFirstToLast(OMR::ValuePropagation *vp, TR::Node *node)
   {
   TR::Node *myParent = vp->getCurrentParent();
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      vp->setCurrentParent(node);
      vp->launchNode(node->getChild(i), node, i);
      }
   vp->setCurrentParent(myParent);
   return node;
   }


TR::Node *constrainVcall(OMR::ValuePropagation *vp, TR::Node *node)
   {
   constrainCall(vp, node);

   // Return if the node is not a call (xcall/xcalli) anymore
   if (!node->getOpCode().isCall())
      return node;

   // Look for System.arraycopy call. If the node is transformed into an arraycopy
   // re-process it.
   //
   vp->transformArrayCopyCall(node);
   if (node->getOpCodeValue() == TR::arraycopy)
      {
      node->setVisitCount(0);
      vp->launchNode(node, vp->getCurrentParent(), 0);
      return node;
      }

   if (vp->transformUnsafeCopyMemoryCall(node))
      return node;

#ifdef J9_PROJECT_SPECIFIC
   TR::SymbolReference *finalizeSymRef = vp->comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_jitCheckIfFinalizeObject, true, true, true);
   if (node->getSymbolReference() == finalizeSymRef)
      {
      TR::Node *receiver = node->getFirstChild();
      bool isGlobal;
      TR::VPConstraint *type = vp->getConstraint(receiver, isGlobal);
      bool canBeRemoved = false;
      // ensure the type is really a fixedClass
      // resolvedClass is not sufficient because java.lang.Object has an
      // empty finalizer method (hasFinalizer returns false) and the call to
      // vm helper is incorrectly optimized in this case
      //
      if (type && type->getClassType() &&
         type->getClassType()->asFixedClass())
         {
         TR_OpaqueClassBlock *klass = type->getClassType()->getClass();
         if (klass && !TR::Compiler->cls.hasFinalizer(vp->comp(), klass) && !vp->comp()->fej9()->isOwnableSyncClass(klass))
            {
            canBeRemoved = true;
            }
         }
      // If a class has a finalizer or is an ownableSync it won't be allocated on the stack. That's ensured
      // by virtue of (indirectly) calling bool J9::ObjectModel::canAllocateInlineClass(TR_OpaqueClassBlock *block)
      // Doesn't make sense to call jitCheckIfFinalizeObject for a stack
      // allocated object, so optimize
      else if (receiver->getOpCode().hasSymbolReference() && receiver->getSymbol()->isLocalObject())
         {
         canBeRemoved = true;
         }

      if (canBeRemoved &&
           performTransformation(vp->comp(), "%s Removing redundant call to jitCheckIfFinalize [%p]\n", OPT_DETAILS, node))
         {
         ///printf("found opportunity in %s to remove call to checkfinalize\n", vp->comp()->signature());fflush(stdout);
         ///traceMsg(vp->comp(), "found opportunity to remove call %p to checkfinalize\n", node);
         TR::TransformUtil::transformCallNodeToPassThrough(vp, node, vp->_curTree, receiver);
         return node;
         }
      }
#endif

   return node;
   }

// NOTE: ValuePropagationTable.hpp is included at the end of this file, when
//       all the functions referenced in the table have been seen.

#include "optimizer/ValuePropagationTable.hpp" // IWYU pragma: keep

static_assert(TR::NumIlOps ==
              (sizeof(constraintHandlers) / sizeof(constraintHandlers[0])),
              "constraintHandlers is not the correct size");
