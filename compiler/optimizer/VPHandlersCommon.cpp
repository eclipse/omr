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
#include "codegen/FrontEnd.hpp"
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
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/OMRValuePropagation.hpp"
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

bool checkMethodSignature(OMR::ValuePropagation *vp, TR::SymbolReference *symRef, const char *sig)
   {
   TR::Symbol *symbol = symRef->getSymbol();
   if (!symbol->isResolvedMethod())
      return false;

   TR::ResolvedMethodSymbol *method = symbol->castToResolvedMethodSymbol();
   if (!method) return false;

    if (strncmp(method->getResolvedMethod()->signature(vp->trMemory()), sig, strlen(sig)) == 0)
      return true;
   return false;
   }


TR::TreeTop *searchForToStringCall(OMR::ValuePropagation *vp,TR::TreeTop *tt, TR::TreeTop *exitTree,
                                                      TR::Node *newBuffer, vcount_t visitCount,
                                                      TR::TreeTop **toStringTree, bool useStringBuffer)
   {

   for (;tt != exitTree; tt = tt->getNextRealTreeTop())
      {
      TR::Node *node = tt->getNode();

      if (node->getNumChildren() == 1 &&
          node->getFirstChild()->getOpCodeValue() == TR::acall)
         {
         if (checkMethodSignature(vp,node->getFirstChild()->getSymbolReference(),
                                  (useStringBuffer ?
                                   "java/lang/StringBuffer.toString()Ljava/lang/String;" :
                                   "java/lang/StringBuilder.toString()Ljava/lang/String;")))
            {
            TR::Node *call = node->getFirstChild();
            if (call->getFirstChild() == newBuffer)
               *toStringTree = tt;
            return tt;
            }
         }
      }
   return tt;
   }

TR::TreeTop *searchForStringAppend(OMR::ValuePropagation *vp,const char *sig, TR::TreeTop *tt, TR::TreeTop *exitTree,
                                    TR::ILOpCodes opCode, TR::Node *newBuffer, vcount_t visitCount, TR::Node **string)
   {
   int len = 0;
   bool isGlobal = false;
   for (;tt != exitTree; tt = tt->getNextRealTreeTop())
      {
      TR::Node *node = tt->getNode();

      if (node->getNumChildren() == 1 &&
          node->getFirstChild()->getOpCodeValue() == opCode)
         {
         if (checkMethodSignature(vp,node->getFirstChild()->getSymbolReference(), sig))
            {
            TR::Node *call = node->getFirstChild();
            if (call->getFirstChild() == newBuffer)
               {
               if (vp->getConstraint(call->getSecondChild(),isGlobal))
                  {
                  if (vp->getConstraint(call->getSecondChild(),isGlobal)->getClassType())
                     {
                     const char *sigAppend = vp->getConstraint(call->getSecondChild(),isGlobal)->getClassType()->getClassSignature(len);
                     if (call->getSecondChild()->getOpCodeValue() ==  TR::aload && (!strncmp(sigAppend,"Ljava/lang/String;",18)))
                        {
                        *string = call->getSecondChild();
                        }
                     }
                  }
               }
            return tt;
            }
         else
            return tt;
         }

      //if (countNodeOccurrencesInSubTree(node, newBuffer, visitCount) > 0) return tt;
      }
   return tt;
   }

static int cacheStringAppend(OMR::ValuePropagation *vp,TR::Node *node)
   {
   return 0;

   if (!vp->lastTimeThrough())
     return 0;

   TR::TreeTop *tt = vp->_curTree;
   TR::TreeTop *newTree   = tt;
   TR::TreeTop *startTree = 0;
   TR::TreeTop *exitTree  =  vp->_curBlock->getExit();
   TR::Node    *newBuffer;

   if(node->getNumChildren() >= 1)
      newBuffer = node->getFirstChild();
      else
         return 0;

   enum {MAX_STRINGS = 2};
   int        initWithString = 0;
   bool       initWithInteger = false;
   TR::TreeTop *appendTree[MAX_STRINGS+1];
   TR::Node    *appendedString[MAX_STRINGS+1];
   char       pattern[MAX_STRINGS+1];
   int        stringCount = 0;
   bool useStringBuffer=false;
   TR::SymbolReference *valueOfSymRef[MAX_STRINGS+1];
   bool success = false;
   char *sigBuffer="java/lang/StringBuffer.<init>(";
   char *sigBuilder = "java/lang/StringBuilder.<init>(";
   char *sigInit = "java/lang/String.<init>(";


  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   if (checkMethodSignature(vp,node->getSymbolReference(), sigInit))
    {
	  TR::Symbol *symbol =node->getSymbolReference()->getSymbol();
      TR_ResolvedMethod *m = symbol->castToResolvedMethodSymbol()->getResolvedMethod();
      if (strncmp(m->signatureChars(), "(Ljava/lang/String;Ljava/lang/String;)V", m->signatureLength())==0)
        {
	      vp->_cachedStringPeepHolesVcalls.add(new (vp->comp()->trStackMemory()) OMR::ValuePropagation::VPTreeTopPair(tt,tt->getPrevRealTreeTop()));
		}
    }
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   if (checkMethodSignature(vp,node->getSymbolReference(), sigBuffer))
      {
         useStringBuffer=true;
         success = true;
      }
   else if (checkMethodSignature(vp,node->getSymbolReference(), sigBuilder))
      {
         success = true;
	     useStringBuffer=false;
      }
	  else
	  {
	     return 0;
	  }


	if (success)
      {
      TR::Symbol *symbol =node->getSymbolReference()->getSymbol();
      TR_ResolvedMethod *m = symbol->castToResolvedMethodSymbol()->getResolvedMethod();
      if (strncmp(m->signatureChars(), "()V", m->signatureLength())==0)
        {
             // Diagnostics
		}else
        {
      	  return 0;
        }
      }
      else // <init> not found (could be unresolved)
      {
         return 0;
      }


   // now search for StringBuffer.append calls that are chained to one another
   TR::TreeTop *lastAppendTree = 0; // updated when we find an append
   TR::Node    *child = newBuffer;

   while (1)
      {
      startTree = tt->getNextRealTreeTop();
	  appendedString[stringCount] = 0;
      int visitCount = 0;
      if (useStringBuffer)
         tt = searchForStringAppend(vp,"java/lang/StringBuffer.append(",
                                    startTree, exitTree, TR::acall, child, visitCount,
                                    appendedString + stringCount);
      else
         tt = searchForStringAppend(vp,"java/lang/StringBuilder.append(",
                                    startTree, exitTree, TR::acall, child, visitCount,
                                    appendedString + stringCount);

	  if (appendedString[stringCount]) // we found it
         {
         appendTree[stringCount] = tt;

         // we could exit here if too many appends are chained
         if (stringCount >= MAX_STRINGS)
            return 0;

         // see which type of append we have
         TR::Symbol *symbol = tt->getNode()->getFirstChild()->getSymbolReference()->getSymbol();
         TR_ASSERT(symbol->isResolvedMethod(), "assertion failure");
         TR::ResolvedMethodSymbol *method = symbol->castToResolvedMethodSymbol();
         TR_ASSERT(method, "assertion failure");
         TR_ResolvedMethod *m = method->getResolvedMethod();
         if (strncmp(m->signatureChars(), "(Ljava/lang/String;)", 20)==0)
            {
            pattern[stringCount] = 'S';
            valueOfSymRef[stringCount] = 0; // don't need conversion to string
            }
         else // appending something that needs conversion using valueOf
            {
            TR::SymbolReference *symRefForValueOf = 0;
            // In the following we can vp->compare only (C) because we know that
            // StringBuffer.append returns a StringBuffer.
            //s
            char *sigBuffer = m->signatureChars();
            TR_ASSERT(m->signatureLength() >= 3, "The minimum signature length should be 3 for ()V");
            }
         stringCount++;
         }
      else // the chain of appends is broken
         {
         appendTree[stringCount] = 0;
         pattern[stringCount] = 0; // string terminator
         break;
         }
      lastAppendTree = tt;
      child = tt->getNode()->getFirstChild(); // the first node is a NULLCHK and its child is the call
      } // end while

   if (stringCount < 2)
      return 0; // cannot apply StringPeepholes
   if (stringCount > MAX_STRINGS)
      return 0;
   if (stringCount == 3)
      return 0; // same as above

   TR_ASSERT(lastAppendTree, "If stringCount <=2 then we must have found an append");

   // now look for the toString call
     TR::TreeTop *toStringTree = 0;
   //visitCount = vp->comp()->incVisitCount();

   int visitCount=0;
   tt = searchForToStringCall(vp,lastAppendTree->getNextRealTreeTop(), exitTree,
                              lastAppendTree->getNode()->getFirstChild(),
                              visitCount, &toStringTree, useStringBuffer);
   if (!toStringTree)
      return 0;

   vp->_cachedStringBufferVcalls.add(new (vp->comp()->trStackMemory()) OMR::ValuePropagation::VPStringCached(appendTree[0],appendTree[1],appendedString[0],appendedString[1],newTree,toStringTree));
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

   cacheStringAppend(vp,node);

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
