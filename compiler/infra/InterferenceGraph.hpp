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

#ifndef INTERFERENCEGRAPH_INCL
#define INTERFERENCEGRAPH_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "compile/Compilation.hpp"
#include "cs2/bitvectr.h"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/IGBase.hpp"
#include "infra/IGNode.hpp"

template <class T> class TR_Stack;

class TR_InterferenceGraph : public TR_IGBase
   {
   struct HashTable;
   friend struct HashTable;

   // Ordered table of all allocated nodes.
   //
   TR_Array<TR_IGNode *> * _nodeTable;

   // FILO structure of nodes for simplify/select phases.
   //
   TR_Stack<TR_IGNode *> * _nodeStack;

   // Hash table mapping of entity addresses to IG nodes.
   //
   struct HashTableEntry
      {
      HashTableEntry *_next;
      TR_IGNode      *_igNode;
      };

   struct HashTable
      {
      int32_t          _numBuckets;
      HashTableEntry **_buckets;
      };

   HashTable _entityHash;

   virtual bool simplify();
   virtual bool select();

   protected:

   TR::Compilation * comp() {return _compilation;}

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }

   void partitionNodesIntoDegreeSets(CS2::ABitVector<TR::Allocator> &workingSet,
                                     CS2::ABitVector<TR::Allocator> &colourableDegreeSet,
                                     CS2::ABitVector<TR::Allocator> &notColourableDegreeSet);

   TR::Compilation *_compilation;
   TR_Memory *_trMemory;

   public:

   TR_InterferenceGraph(TR::Compilation *comp) :
      _compilation(comp),
      _nodeTable(NULL),
      _nodeStack(NULL),
      _trMemory(comp->trMemory()),
      TR_IGBase() {}

   TR_InterferenceGraph(TR::Compilation *comp, int32_t estimatedNodes = 32);

   TR_IGNode *getNodeTable(IGNodeIndex i) {return ((*_nodeTable)[i]);}

   TR_Stack<TR_IGNode *> *getNodeStack()  {return _nodeStack;}
   TR_Stack<TR_IGNode *> *setNodeStack(TR_Stack<TR_IGNode *> *ns) {return(_nodeStack = ns);}

   int32_t entityHashBucket(void *entity) {return (int32_t)(((uintptr_t)entity >> 2) % _entityHash._numBuckets);}
   void addIGNodeToEntityHash(TR_IGNode *igNode);

   TR_IGNode *add(void *entity, bool ignoreDuplicates = false);
   void addInterferenceBetween(void *entity1, void *entity2)
       {
       TR_IGNode *igNode1 = getIGNodeForEntity(entity1);
       TR_IGNode *igNode2 = getIGNodeForEntity(entity2);
       TR_ASSERT(igNode1,"addInterferenceBetween: entity1 %p does not exist in this interference graph\n", entity1);
       TR_ASSERT(igNode2,"addInterferenceBetween: entity2 %p does not exist in this interference graph\n", entity2);

       addInterferenceBetween(igNode1,igNode2);
       }

   void addInterferenceBetween(TR_IGNode *igNode1, TR_IGNode *igNode2);
   void removeAllInterferences(void *entity);

   TR_IGNode *getIGNodeForEntity(void *entity);
   bool hasInterference(void *entity1, void *entity2);
   bool hasInterference(TR_IGNode *node1, TR_IGNode *node2)
       {
       IMIndex bit = getNodePairToBVIndex(node1->getIndex(), node2->getIndex());
       return getInterferenceMatrix()->isSet(bit);
       }

   void removeInterferenceBetween(void *entity1, void *entity2)
       {
       TR_IGNode *igNode1 = getIGNodeForEntity(entity1);
       TR_IGNode *igNode2 = getIGNodeForEntity(entity2);

       TR_ASSERT(igNode1, "hasInterference: entity1 %p does not exist in this interference graph\n", entity1);
       TR_ASSERT(igNode2, "hasInterference: entity2 %p does not exist in this interference graph\n", entity2);
       removeInterferenceBetween(igNode1,igNode2);
       }
   void removeInterferenceBetween(TR_IGNode *node1, TR_IGNode *node2);

   void virtualRemoveEntityFromIG(void *entity);
   void virtualRemoveNodeFromIG(TR_IGNode *igNode);

   IGNodeDegree findMaxDegree();

   bool doColouring(IGNodeColour numColours);
   IGNodeColour findMinimumChromaticNumber();

#ifdef DEBUG
   void dumpIG(char *msg = NULL);
#else
   void dumpIG(char *msg = NULL) {}
#endif
   };
#endif
