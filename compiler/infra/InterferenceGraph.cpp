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

#include "infra/InterferenceGraph.hpp"

#include <stdint.h>
#include <string.h>
#include "env/StackMemoryRegion.hpp"
#include "compile/Compilation.hpp"
#include "cs2/bitvectr.h"
#include "env/TRMemory.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/IGBase.hpp"
#include "infra/IGNode.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"

TR_InterferenceGraph::TR_InterferenceGraph(TR::Compilation *comp, int32_t estimatedNodes) :
      _compilation(comp),
      _nodeTable(NULL),
      _nodeStack(NULL),
      _trMemory(comp->trMemory()),
      TR_IGBase()
   {
   TR_ASSERT(estimatedNodes > 0, "interference graph must be created with a node estimate!");

   int32_t numBits = (estimatedNodes * (estimatedNodes - 1)) >> 1;
   setInterferenceMatrix(new (trHeapMemory()) TR_BitVector(numBits, trMemory(), heapAlloc, growable));
   _nodeTable = new (trHeapMemory()) TR_Array<TR_IGNode *>(trMemory(), estimatedNodes, false, heapAlloc);
   _nodeStack = new (trHeapMemory()) TR_Stack<TR_IGNode *>(trMemory(), estimatedNodes, false, heapAlloc);

   // TODO: allocate from stack memory
   //
   _entityHash._numBuckets = 73;
   _entityHash._buckets = (HashTableEntry **)trMemory()->allocateHeapMemory(_entityHash._numBuckets*sizeof(HashTableEntry *));
   memset(_entityHash._buckets, 0, _entityHash._numBuckets*sizeof(HashTableEntry *));
   }


// Add a new, unique entity to the interference graph.
//
TR_IGNode *TR_InterferenceGraph::add(void *entity, bool ignoreDuplicates)
   {
   TR_IGNode *igNode = getIGNodeForEntity(entity);

   if (igNode != NULL && ignoreDuplicates)
      return igNode;

   TR_ASSERT(!igNode,
           "entity %p already exists in this interference graph\n", entity);

   igNode = new (trHeapMemory()) TR_IGNode(entity, trMemory());

   addIGNodeToEntityHash(igNode);

   igNode->setIndex(getNumNodes());
   ((*_nodeTable)[getNumNodes()]) = igNode;
   incNumNodes();

   return igNode;
   }


// Add an interference between two existing entities in an interference graph.
//
void TR_InterferenceGraph::addInterferenceBetween(TR_IGNode *igNode1,
                                                  TR_IGNode *igNode2)
   {
   IMIndex bitNum = getNodePairToBVIndex(igNode1->getIndex(), igNode2->getIndex());
   if (igNode1 != igNode2 && !getInterferenceMatrix()->isSet(bitNum))
      {
      getInterferenceMatrix()->set(bitNum);
      igNode2->getAdjList().add(igNode1);
      igNode1->getAdjList().add(igNode2);
      igNode2->incDegree();
      igNode1->incDegree();
      }
   }


bool TR_InterferenceGraph::hasInterference(void *entity1,
                                           void *entity2)
   {
   TR_IGNode *igNode1 = getIGNodeForEntity(entity1);
   TR_IGNode *igNode2 = getIGNodeForEntity(entity2);

   TR_ASSERT(igNode1, "hasInterference: entity1 %p does not exist in this interference graph\n", entity1);
   TR_ASSERT(igNode2, "hasInterference: entity2 %p does not exist in this interference graph\n", entity2);

   IMIndex bit = getNodePairToBVIndex(igNode1->getIndex(), igNode2->getIndex());
   return getInterferenceMatrix()->isSet(bit);
   }


void TR_InterferenceGraph::removeInterferenceBetween(TR_IGNode *igNode1, TR_IGNode *igNode2)
   {
   igNode1->getAdjList().remove(igNode2);
   igNode2->getAdjList().remove(igNode1);
   igNode1->decDegree();
   igNode2->decDegree();
   IMIndex bit = getNodePairToBVIndex(igNode1->getIndex(), igNode2->getIndex());
   getInterferenceMatrix()->reset(bit);
   }


void TR_InterferenceGraph::removeAllInterferences(void *entity)
   {
   TR_IGNode *igNode = getIGNodeForEntity(entity);

   TR_ASSERT(igNode, "removeAllInterferences: entity %p is not in the interference graph\n", entity);

   ListIterator<TR_IGNode> iterator(&igNode->getAdjList());
   TR_IGNode *cursor = iterator.getFirst();
   IMIndex bit;

   while (cursor)
      {
      cursor->getAdjList().remove(igNode);
      cursor->decDegree();
      bit = getNodePairToBVIndex(igNode->getIndex(), igNode->getIndex());
      getInterferenceMatrix()->reset(bit);
      cursor = iterator.getNext();
      }

   // reset working degree?

   igNode->setDegree(0);
   igNode->getAdjList().deleteAll();
   }


void TR_InterferenceGraph::addIGNodeToEntityHash(TR_IGNode *igNode)
   {
   void *entity = igNode->getEntity();

   TR_ASSERT(entity, "addIGNodeToEntityHash: IGNode %p does not reference its data\n", igNode);

   int32_t hashBucket = entityHashBucket(entity);

   // TODO: allocate from stack memory
   //
   HashTableEntry *entry = (HashTableEntry *)trMemory()->allocateHeapMemory(sizeof(HashTableEntry));
   entry->_igNode = igNode;

   HashTableEntry *prevEntry = _entityHash._buckets[hashBucket];
   if (prevEntry)
      {
      entry->_next = prevEntry->_next;
      prevEntry->_next = entry;
      }
   else
      entry->_next = entry;

   _entityHash._buckets[hashBucket] = entry;
   }


TR_IGNode * TR_InterferenceGraph::getIGNodeForEntity(void *entity)
   {
   int32_t hashBucket = entityHashBucket(entity);

   HashTableEntry *firstEntry = _entityHash._buckets[hashBucket];
   if (firstEntry)
      {
      HashTableEntry *entry = firstEntry;

      do
         {
         if (entry->_igNode->getEntity() == entity)
            {
            return entry->_igNode;
            }
         entry = entry->_next;
         }
      while (entry != firstEntry);
      }

   return NULL;
   }


// Doesn't actually remove a node from the IG, but adjusts the working degrees of its
// neighbours and itself to appear that way.
//
void TR_InterferenceGraph::virtualRemoveEntityFromIG(void *entity)
   {
   TR_IGNode *igNode = getIGNodeForEntity(entity);
   igNode->decWorkingDegreeOfNeighbours();
   igNode->setIsRemovedFromIG();
   igNode->setWorkingDegree(0);
   }


// Doesn't actually remove a node from the IG, but adjusts the working degrees of its
// neighbours and itself to appear that way.
//
void TR_InterferenceGraph::virtualRemoveNodeFromIG(TR_IGNode *igNode)
   {
   igNode->decWorkingDegreeOfNeighbours();
   igNode->setIsRemovedFromIG();
   igNode->setWorkingDegree(0);
   }


// Partition the nodes identified by the 'workingSet' bit vector into sets
// based on their degree.
//
void TR_InterferenceGraph::partitionNodesIntoDegreeSets(TR_BitVector *workingSet,
                                                        TR_BitVector *colourableDegreeSet,
                                                        TR_BitVector *notColourableDegreeSet)
   {
   int32_t               i;
   TR_BitVectorIterator bvi(*workingSet);

   TR_ASSERT(getNumColours() > 0,
          "can't partition without knowing the number of available colours\n");

   // Empty the existing degree sets.
   //
   colourableDegreeSet->empty();
   notColourableDegreeSet->empty();

   // Partition the specified nodes into sets based on their working degree.
   //
   while (bvi.hasMoreElements())
      {
      i = bvi.getNextElement();

      if (getNodeTable(i)->getWorkingDegree() < unsigned(getNumColours()))
         {
         colourableDegreeSet->set(i);
         }
      else
         {
         notColourableDegreeSet->set(i);
         }
      }

#ifdef DEBUG
   if (debug("traceIG"))
     {
     diagnostic("\nPartitioned Nodes\n");
     diagnostic("-----------------\n");

     diagnostic("\n[%4d] degree < %-10d : ", colourableDegreeSet->elementCount(), getNumColours());
     colourableDegreeSet->print(comp());

     diagnostic("\n[%4d] degree < MAX_DEGREE : ", notColourableDegreeSet->elementCount());
     notColourableDegreeSet->print(comp());
     diagnostic("\n\n");
     }
#endif
   }


IGNodeDegree TR_InterferenceGraph::findMaxDegree()
   {
   IGNodeDegree maxDegree = 0;

   for (IGNodeIndex i=0; i<getNumNodes(); i++)
      {
      if (getNodeTable(i)->getDegree() > maxDegree)
         {
         maxDegree = getNodeTable(i)->getDegree();
         }
      }

   return maxDegree;
   }


// General Briggs graph colouring algorithm.
//
bool TR_InterferenceGraph::doColouring(IGNodeColour numColours)
   {
   bool success;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR_ASSERT(numColours > 0, "doColouring: invalid chromatic number\n");
   setNumColours(numColours);

   // TODO: reset stacks, colours?
   //
   if ((success = simplify()))
      {
      success = select();
      }

   if (debug("traceIG"))
      {
      if (!success)
         {
         diagnostic("COLOUR IG: Could not find a colouring for IG %p with %d colours.\n",
                     this, numColours);
         }
      else
         {
         diagnostic("COLOUR IG: IG %p coloured with %d colours.\n", this, getNumberOfColoursUsedToColour());
         }
      }

   return success;
   }


// Find the minimum number of colours required to colour this interference graph.
//
IGNodeColour TR_InterferenceGraph::findMinimumChromaticNumber()
   {
   return 0;
   }


// Perform Simplify colouring phase on an interference graph.
//
bool TR_InterferenceGraph::simplify()
   {
   TR_IGNode *igNode,
             *bestSpillNode;

   if (getNumNodes()==0) return true;

   TR_BitVector *workingSet = new (trStackMemory()) TR_BitVector(getNumNodes(), trMemory(), stackAlloc);
   workingSet->setAll(getNumNodes());

   TR_BitVector * colourableDegreeSet = new (trStackMemory()) TR_BitVector(getNumNodes(), trMemory(), stackAlloc);
   TR_BitVector * notColourableDegreeSet = new (trStackMemory()) TR_BitVector(getNumNodes(), trMemory(), stackAlloc);

   for (auto i = 0U; i < getNumNodes(); i++)
      {
      igNode = getNodeTable(i);
      igNode->setWorkingDegree(igNode->getDegree());
      igNode->resetIsRemovedFromIG();
      igNode->setColour(UNCOLOURED);
      }

   while (!workingSet->isEmpty())
      {
      partitionNodesIntoDegreeSets(workingSet,colourableDegreeSet,notColourableDegreeSet);

      // Push nodes from the colourable set onto the stack and adjust the degrees of
      // their neighbours until there are no nodes remaining in the colourable set.
      //
      if (!colourableDegreeSet->isEmpty())
         {
         TR_BitVectorIterator bvi(*colourableDegreeSet);

         while (bvi.hasMoreElements())
            {
            igNode = getNodeTable(bvi.getNextElement());

            if (debug("traceIG"))
               {
               diagnostic("SIMPLIFY: Selected colourable IG node #%d with degree %d: (igNode=%p, entity=%p)\n",
                           igNode->getIndex(), igNode->getWorkingDegree(), igNode, igNode->getEntity());
               }

            virtualRemoveNodeFromIG(igNode);
            workingSet->reset(igNode->getIndex());
            getNodeStack()->push(igNode);
            }

         // Repartition the nodes.
         //
         continue;
         }

      // There are no nodes left in the colourable set.  Choose a spill candidate among nodes in
      // the non-colourable degree set.
      //
      TR_ASSERT(!notColourableDegreeSet->isEmpty(),
             "not colourable set must contain at least one member\n");

      //CS2::ABitVector<TR::Allocator>::Cursor notColourableCursor(notColourableDegreeSet);
      TR_BitVectorIterator bvi;
      if (!notColourableDegreeSet->isEmpty())
         {
         // Choose the node from this degree set with the largest degree
         // and optimistically push it onto the stack.
         //
         int32_t degree = -1;

         bestSpillNode = NULL;
         while (bvi.hasMoreElements())
            {
            igNode = getNodeTable(bvi.getNextElement());

            // TODO: This unsigned conversion was inserted while addressing warnings. The warning at this line warned
            // us about signed/unsigned comparison. The C++ standard dictates that signed values are always converted
            // to unsigned before the comparison. Because `degree` is initialized to -1, the unsigned conversion will
            // produce UINT_MAX in this case, so the comparison below will never succeed and we should always assert.
            if (igNode->getDegree() > unsigned(degree))
               {
               degree = igNode->getDegree();
               bestSpillNode = igNode;
               }
            }

         TR_ASSERT_FATAL(bestSpillNode, "Could not find a spill candidate.\n");

         virtualRemoveNodeFromIG(bestSpillNode);
         workingSet->reset(bestSpillNode->getIndex());
         getNodeStack()->push(bestSpillNode);
         }
      }

   return true;
   }


// Perform Select colouring phase on an interference graph.
//
bool TR_InterferenceGraph::select()
   {
   TR_IGNode            *igNode;
   TR_BitVectorIterator  bvi;

   TR_BitVector *availableColours = new (trStackMemory()) TR_BitVector(getNumColours(), trMemory(), stackAlloc);
   TR_BitVector *assignedColours = new (trStackMemory()) TR_BitVector(getNumColours(), trMemory(), stackAlloc);

   setNumberOfColoursUsedToColour(0);

   while (!getNodeStack()->isEmpty())
      {
      igNode = getNodeStack()->pop();

      availableColours->setAll(getNumColours());

      ListIterator<TR_IGNode> iterator(&igNode->getAdjList());
      TR_IGNode *adjCursor = iterator.getFirst();

      while (adjCursor)
         {
         if (adjCursor->getColour() != UNCOLOURED)
            {
            availableColours->reset(adjCursor->getColour());
            }

         adjCursor = iterator.getNext();
         }

      if (debug("traceIG"))
         {
         diagnostic("SELECT:  For IG node #%d (%p), available colours = ", igNode->getIndex(), igNode);
         availableColours->print(_compilation);
         diagnostic("\n");
         }

      bvi.setBitVector(*availableColours);

      if (bvi.hasMoreElements())
         {
         IGNodeColour colour = (IGNodeColour)bvi.getNextElement();
         igNode->setColour(colour);

         if (!assignedColours->isSet(colour))
            {
            assignedColours->set(colour);
            }

         if (debug("traceIG"))
            {
            diagnostic("         Selected colour: %d\n", colour);
            }
         }
      else
         {
         // No colours are available.  Colouring has failed.
         //
         if (debug("traceIG"))
            {
            diagnostic("         NO COLOURS AVAILABLE\n");
            }

         return false;
         }
      }

   setNumberOfColoursUsedToColour(assignedColours->elementCount());
   return true;
   }


#ifdef DEBUG
void TR_InterferenceGraph::dumpIG(const char *msg)
   {
   if (msg)
      {
      diagnostic("\nINTERFERENCE GRAPH %p: %s\n\n", this, msg);
      }
   else
      {
      diagnostic("\nINTERFERENCE GRAPH %p\n\n", this);
      }

   for (IGNodeIndex i=0; i<getNumNodes(); i++)
      {
      getNodeTable(i)->print(comp());
      }
   }
#endif
