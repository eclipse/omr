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

#ifndef OMR_INFRA_SIDE_TABLE
#define OMR_INFRA_SIDE_TABLE

#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/HashTab.hpp"   // For TR_HashTabInt (until we can replace it)
#include "il/Node_inlines.hpp" // Unfortunately we want node->getGlobalIndex here in the header

namespace TR {

template <typename Entry>
class SideTable // abstract
   {
   int32_t         _nextIndex;
   TR_HashTabInt   _indexes;
   TR_Array<Entry> _entries;

   protected:

   SideTable(TR_Memory *mem, TR_AllocationKind allocKind=stackAlloc)
      :_nextIndex(0)
      ,_indexes(mem, allocKind)
      ,_entries(mem, 8, true, allocKind)
      {}

   bool lookupIndex(int32_t key, int32_t *index)
      {
      TR_HashId hashID;
      if (_indexes.locate(key, hashID))
         {
         *index = (int32_t)(intptr_t)_indexes.getData(hashID);
         return true;
         }
      else
         {
         int32_t result = _nextIndex++;
         _indexes.add(key, hashID, (void*)(intptr_t)result);
         *index = result;
         return false;
         }
      }

   Entry &getFor(int32_t key)
      {
      int32_t index;
      lookupIndex(key, &index);
      return getAt(index);
      }

   public:

   int32_t size(){ return _nextIndex; }

   Entry &getAt(int32_t index)
      {
      TR_ASSERT(0 <= index && index < _nextIndex, "SideTable index %d out of range [0,%d]", index, _nextIndex);
      return _entries[index];
      }

   Entry &operator[](int32_t index){ return getAt(index); }
   };

// NodeSideTable::Entry must have a constructor that takes a Node*
//
template <typename Entry>
class NodeSideTable: public SideTable<Entry>
   {
   typedef SideTable<Entry> Super;

   public:

   NodeSideTable(TR_Memory *mem, TR_AllocationKind allocKind=stackAlloc)
      :SideTable<Entry>(mem, allocKind){}

   int32_t indexOf(Node *node)
      {
      int32_t index;
      if (Super::lookupIndex(node->getGlobalIndex(), &index))
         {
         // Existing entry
         }
      else
         {
         // New entry -- call its constructor
         //
         new (&Super::getAt(index)) Entry(node);
         }
      return index;
      }

   Entry &getFor(Node *node)
      {
      return Super::getAt(indexOf(node));
      }

   Entry &operator[](Node *node){ return getFor(node); }
   };

}

#endif
