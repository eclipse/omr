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

#include "infra/Checklist.hpp"

#include "compile/Compilation.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "infra/BitVector.hpp"

namespace TR
{
   Checklist::Checklist(TR::Compilation* c): _comp(c) { _v = _comp->getBitVectorPool().get(); }
   Checklist::~Checklist()                            {      _comp->getBitVectorPool().release(_v); };

   NodeChecklist::NodeChecklist(TR::Compilation* c): Checklist(c) {};
   bool NodeChecklist::contains(TR::Node* n) const { return _v->isSet(n->getGlobalIndex()); }
   void NodeChecklist::add(TR::Node* n)       {        _v->set(n->getGlobalIndex()); }
   void NodeChecklist::remove(TR::Node* n)    {        _v->reset(n->getGlobalIndex()); }
   void NodeChecklist::add(const NodeChecklist &other)    {        *_v |= *(other._v); }
   void NodeChecklist::remove(const NodeChecklist &other) { return *_v -= *(other._v); }
   bool NodeChecklist::operator==(const NodeChecklist &other) const { return *_v == *(other._v); }
   bool NodeChecklist::contains(const NodeChecklist &other) const
      {
      NodeChecklist diff(_comp);
      diff.add(other);
      diff.remove(*this);
      return diff.isEmpty();
      }

   BlockChecklist::BlockChecklist(TR::Compilation* c): Checklist(c) {};
   bool BlockChecklist::contains(TR::Block* b) const { return _v->isSet(b->getNumber()); }
   void BlockChecklist::add(TR::Block* b)        {        _v->set(b->getNumber()); }
   void BlockChecklist::remove(TR::Block* b)     {        _v->reset(b->getNumber()); }
   void BlockChecklist::add(const BlockChecklist &other)    {        *_v |= *(other._v); }
   void BlockChecklist::remove(const BlockChecklist &other) { return *_v -= *(other._v); }
   bool BlockChecklist::operator==(const BlockChecklist &other) const { return *_v == *(other._v); }
   bool BlockChecklist::contains(const BlockChecklist &other) const
      {
      BlockChecklist diff(_comp);
      diff.add(other);
      diff.remove(*this);
      return diff.isEmpty();
      }
}
