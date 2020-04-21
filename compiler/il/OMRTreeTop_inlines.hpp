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

#ifndef OMR_TREETOP_INLINE_INCL
#define OMR_TREETOP_INLINE_INCL

#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/OMRTreeTop.hpp"
#include "il/TreeTop.hpp"
#include "infra/Assert.hpp"

namespace TR { class Block; }

/*
 * Performance sensitive implementations are included here to
 * support inlining.
 *
 * This file will be included by OMRTreeTop.cpp
 */

inline TR::TreeTop *
OMR::TreeTop::self()
   { return static_cast<TR::TreeTop*>(this); }

inline TR::TreeTop *
OMR::TreeTop::getNextTreeTop()
   { return _pNext; }

inline TR::TreeTop *
OMR::TreeTop::setNextTreeTop(TR::TreeTop *p)
   { return (_pNext = p); }

inline TR::TreeTop *
OMR::TreeTop::getPrevTreeTop()
   { return _pPrev; }

inline TR::TreeTop *
OMR::TreeTop::setPrevTreeTop(TR::TreeTop *p)
   { return (_pPrev = p); }

inline TR::Node *
OMR::TreeTop::getNode()
   { return _pNode; }

inline TR::Node *
OMR::TreeTop::setNode(TR::Node *p)
   { return (_pNode = p); }

inline void
OMR::TreeTop::unlink(bool decRefCountRecursively)
   {
   TR::TreeTop *prevTT = self()->getPrevTreeTop();
   TR::TreeTop *nextTT = self()->getNextTreeTop();

   prevTT->setNextTreeTop(nextTT);
   nextTT->setPrevTreeTop(prevTT);
   if (decRefCountRecursively)
      self()->getNode()->recursivelyDecReferenceCount();
   }

inline void
OMR::TreeTop::join(TR::TreeTop * p)
   {
   self()->setNextTreeTop(p);
   
   if (p)
      p->setPrevTreeTop(self());
   }

inline TR::TreeTop *
OMR::TreeTop::insertAfter(TR::TreeTop *tt)
   {
   tt->join(self()->getNextTreeTop());
   self()->join(tt);
   return tt;
   }

inline TR::TreeTop *
OMR::TreeTop::insertBefore(TR::TreeTop *tt)
   {
   self()->getPrevTreeTop()->join(tt);
   tt->join(self());
   return tt;
   }

inline void
OMR::TreeTop::insertNewTreeTop(TR::TreeTop *beforeNewTreeTop, TR::TreeTop * afterNewTreeTop)
   {
   TR_ASSERT(beforeNewTreeTop->getNextTreeTop() == afterNewTreeTop && afterNewTreeTop->getPrevTreeTop() == beforeNewTreeTop, "beforeNewTreeTop and afterNewTreeTop must be consecutive");
   beforeNewTreeTop->join(self());
   self()->join(afterNewTreeTop);
   }

inline TR::TreeTop *
OMR::TreeTop::getNextRealTreeTop()
   {
   TR::TreeTop *treeTop;
   for (treeTop = self()->getNextTreeTop();
        treeTop && treeTop->getNode() && treeTop->getNode()->getOpCode().isExceptionRangeFence();
        treeTop = treeTop->getNextTreeTop())
      {}
   return treeTop;
   }

inline TR::TreeTop *
OMR::TreeTop::getPrevRealTreeTop()
   {
   TR::TreeTop *treeTop;
   for (treeTop = self()->getPrevTreeTop();
        treeTop && treeTop->getNode()->getOpCode().isExceptionRangeFence();
        treeTop = treeTop->getPrevTreeTop())
      {}
   return treeTop;
   }

inline TR::Block *
OMR::TreeTop::getEnclosingBlock( bool forward)
   {
   TR::TreeTop * tt = self();
   if (forward)
      while (tt->getNode()->getOpCodeValue() != TR::BBEnd)
         {
         tt = tt->getNextTreeTop();
         //TR_ASSERT(tt && tt->getNode(), "either tt or node on a tt null here, we will segfault");
         }
   else
      while (tt->getNode()->getOpCodeValue() != TR::BBStart)
         {
         tt = tt->getPrevTreeTop();
         //TR_ASSERT(tt && tt->getNode(), "either tt or node on a tt null here, we will segfault");
         }
   return tt->getNode()->getBlock();
   }

#endif
