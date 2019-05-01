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

#include "il/NodeUtils.hpp"

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"

void
GlobalRegisterInfo::freeNodeExtension(TR::Node * owner)
   {
   owner->freeExtensionIfExists();
   }

void
ChildUnionMembers::CaseInfo::freeNodeExtension( TR::Node * owner)
   {
   owner->freeExtensionIfExists();
   }

void
ChildUnionMembers::MonitorInfo::freeNodeExtension(TR::Node * owner)
   {
   owner->freeExtensionIfExists();
   }

TR::Node *
TR_ParentOfChildNode::getParent()
   {
   return _parent;
   }

void
TR_ParentOfChildNode::setParent(TR::Node * parent)
   {
   _parent = parent;
   }

int32_t
TR_ParentOfChildNode::getChildNumber()
   {
   return _childNumber;
   }

void
TR_ParentOfChildNode::setParentAndChildNumber(TR::Node * parent, int32_t childNumber)
   {
   _parent = parent; _childNumber = childNumber;
   }

void
TR_ParentOfChildNode::setChild(TR::Node * newChild)
   {
   TR_ASSERT(!isNull(), "tried to update a NULL ParentOfChildNode");

   TR::Node * oldChild = _parent->getChild(_childNumber);
   _parent->setChild(_childNumber, newChild);
   oldChild->decReferenceCount();
   newChild->incReferenceCount();
   }

TR::Node *
TR_ParentOfChildNode::getChild()
   {
   return _parent->getChild(_childNumber);
   }

bool
TR_ParentOfChildNode::isNull()
   {
   return (_parent == NULL);
   }
