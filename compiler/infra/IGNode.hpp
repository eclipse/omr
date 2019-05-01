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

#ifndef IGNODE_INCL
#define IGNODE_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"

namespace TR { class Compilation; }

typedef uint32_t IGNodeIndex;
typedef uint32_t IGNodeDegree;
typedef int32_t  IGNodeColour;

// Maximum number of neighbours of any node in the interference graph.
// The formula assumes the degree can not be negative values.
//
#define MAX_DEGREE          ((IGNodeDegree)((1L << (sizeof(IGNodeDegree) << 3)) - 1))

#define UNCOLOURED          -1
#define CANNOTCOLOR         -1

class TR_IGNode
   {
   void           *_pEntity;

   IGNodeIndex     _index;
   IGNodeDegree    _degree;
   IGNodeDegree    _workingDegree;
   IGNodeColour    _colour;
   List<TR_IGNode> _adjList;
   flags8_t        _flags;

   enum
      {
      MustBeSpilled   = 0x01,  // Node must be spilled
      IsRemovedFromIG = 0x02,   // Node has been removed from interference graph
      };

   public:

   TR_ALLOC(TR_Memory::IGNode)

   TR_IGNode(TR_Memory * m) :
      _pEntity(NULL),
      _index(0),
      _degree(0),
      _workingDegree(0),
      _colour(UNCOLOURED),
      _adjList(m),
      _flags(0) {}

   TR_IGNode(void *p, TR_Memory * m) :
      _pEntity(p),
      _index(0),
      _degree(0),
      _workingDegree(0),
      _colour(UNCOLOURED),
      _adjList(m),
      _flags(0) {}

   IGNodeColour getColour()       {return _colour;}
   void setColour(IGNodeColour c) {_colour = c;}
   bool isColoured()              {return (_colour == UNCOLOURED) ? false : true;}

   IGNodeIndex getIndex()       {return _index;}
   void setIndex(IGNodeIndex i) {_index = i;}

   List<TR_IGNode> &getAdjList() {return _adjList;}

   void *getEntity()       {return _pEntity;}
   void setEntity(void *p) {_pEntity = p;}

   IGNodeDegree getWorkingDegree()       {return _workingDegree;}
   void setWorkingDegree(IGNodeDegree d) {_workingDegree = d;}
   IGNodeDegree incWorkingDegree()
      {
      TR_ASSERT(_workingDegree < MAX_DEGREE, "incWorkingDegree: maximum node degree exceeded!");
      return ++_workingDegree;
      }

   IGNodeDegree incWorkingDegreeBy(IGNodeDegree d)
      {
      TR_ASSERT((MAX_DEGREE-_workingDegree) >= d, "incWorkingDegreeBy: maximum node degree exceeded!");
      return (_workingDegree += d);
      }

   IGNodeDegree decWorkingDegree()
      {
      TR_ASSERT(_workingDegree >= 1, "decWorkingDegree: working degree is less than 0! (_workingDegree=%d nodeIndex=%d)",_workingDegree, getIndex());
      return --_workingDegree;
      }

   IGNodeDegree decWorkingDegreeBy(IGNodeDegree d)
      {
      TR_ASSERT(_workingDegree >= d, "decWorkingDegreeBy: degree is less than 0!");
      return (_workingDegree -= d);
      }

   IGNodeDegree getDegree()       {return _degree;}
   void setDegree(IGNodeDegree d) {_degree = d;}

   IGNodeDegree incDegree()
      {
      TR_ASSERT(_degree < MAX_DEGREE, "incDegree: maximum node degree exceeded!");
      return ++_degree;
      }

   IGNodeDegree incDegreeBy(IGNodeDegree d)
      {
      TR_ASSERT((MAX_DEGREE-_degree) >= d, "incDegreeBy: maximum node degree exceeded!");
      return (_degree += d);
      }

   IGNodeDegree decDegree()
      {
      TR_ASSERT(_degree >= 1, "decDegree: degree is less than 0!");
      return --_degree;
      }

   IGNodeDegree decDegreeBy(IGNodeDegree d)
      {
      TR_ASSERT(_degree >= d, "decDegreeBy: degree is less than 0!");
      return (_degree -= d);
      }

   bool isRemovedFromIG()      {return _flags.testAny(IsRemovedFromIG);}
   void setIsRemovedFromIG()   {_flags.set(IsRemovedFromIG);}
   void resetIsRemovedFromIG() {_flags.reset(IsRemovedFromIG);}

   void decWorkingDegreeOfNeighbours();

#ifdef DEBUG
      void print(TR::Compilation *);
#endif

   };

#endif
