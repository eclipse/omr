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

#ifndef OMR_CHECKLIST_INCL
#define OMR_CHECKLIST_INCL

#ifdef OMRZTPF
#define __TPF_DO_NOT_MAP_ATOE_REMOVE
#endif

#include "infra/BitVector.hpp"

class TR_BitVector;

namespace TR
{

class Block;
class Compilation;
class Node;

class Checklist
   {
   protected :
   TR_BitVector* _v;
   TR::Compilation* _comp;

   public :
   Checklist(TR::Compilation* c);
   ~Checklist();
   bool isEmpty(){ return _v->isEmpty(); }
   void print(TR::Compilation *c) { _v->print(c); }
   };

class NodeChecklist: public Checklist
   {
   public:
   NodeChecklist(TR::Compilation* c);
   bool contains(TR::Node* n) const;
   void add(TR::Node* n);
   void remove(TR::Node* n);
   bool contains(const NodeChecklist &other) const;
   void add(const NodeChecklist &other);
   void remove(const NodeChecklist &other);
   bool operator==(const NodeChecklist &other) const;
   bool operator!=(const NodeChecklist &other) const { return !operator==(other); }
   };

class BlockChecklist: public Checklist
   {
   public:
   BlockChecklist(TR::Compilation* c);
   bool contains(TR::Block* b) const;
   void add(TR::Block* b);
   void remove(TR::Block* b);
   bool contains(const BlockChecklist &other) const;
   void add(const BlockChecklist &other);
   void remove(const BlockChecklist &other);
   bool operator==(const BlockChecklist &other) const;
   bool operator!=(const BlockChecklist &other) const { return !operator==(other); }
   };

}

#endif
