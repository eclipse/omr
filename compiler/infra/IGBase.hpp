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

#ifndef IGBASE_INCL
#define IGBASE_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/IGNode.hpp"

class TR_BitVector;

typedef uint64_t IMIndex;

// Number of pre-computed high indices into the interference matrix.
//
#define NUM_PRECOMPUTED_HIGH_INDICES  64

class TR_IGBase
   {
   private:

   // Number of nodes in the graph.
   //
   IGNodeIndex _numNodes;

   // The _interferenceMatrix is a lower triangular bit matrix that summarizes interferences
   // between nodes.  For a node pair (L,H), the corresponding bit should be set if the two
   // nodes interfere.
   //
   // The low and high node indices begin at 0.
   //
   // The bit matrix is implemented with a bit vector, as follows:
   //
   //                       Low Node
   //                        Index
   //
   //                  0  1  2  3  4  5 ...
   //                +---------------------
   //               0| X                         The bit position for a node pair (L,H)
   //               1| 0  X                      can be computed with:
   //   High Node   2| 1  2  X
   //     Index     3| 3  4  5  X                   bit = L + ((H-1)*H)/2
   //               4| 6  7  8  9  X
   //               5|10 11 12 13 14  X
   //               :|
   //               :|
   //
   // The total number of bits required for N nodes = N * (N-1) / 2
   //
   TR_BitVector * _interferenceMatrix;
   static IMIndex _highIndexTable[NUM_PRECOMPUTED_HIGH_INDICES];

   // For graph colouring, attempt to colour the graph using this many colours.
   //
   IGNodeColour _numColours;

   // Actual number of colours that graph was coloured in.
   //
   IGNodeColour _numberOfColoursUsedToColour;

   protected:

   IMIndex getNodePairToBVIndex(IGNodeIndex index1, IGNodeIndex index2);
   IMIndex getOrderedNodePairToBVIndex(IGNodeIndex low, IGNodeIndex high);

   public:

   TR_ALLOC(TR_Memory::IGBase)

   TR_IGBase() :
      _numNodes(0),
      _numColours(0),
      _numberOfColoursUsedToColour(0),
      _interferenceMatrix(NULL)
       {
       }

   IGNodeIndex getNumNodes()              {return _numNodes;}
   IGNodeIndex setNumNodes(IGNodeIndex n) {return (_numNodes = n);}
   IGNodeIndex incNumNodes()              {return (++_numNodes);}
   bool isEmpty()                         {return (_numNodes == 0);}

   IGNodeColour getNumColours()               {return _numColours;}
   IGNodeColour setNumColours(IGNodeColour c) {return (_numColours = c);}

   TR_BitVector *getInterferenceMatrix()        {return _interferenceMatrix;}
   void setInterferenceMatrix(TR_BitVector *im) {_interferenceMatrix = im;}

   IGNodeColour getNumberOfColoursUsedToColour() {return _numberOfColoursUsedToColour;}
   IGNodeColour setNumberOfColoursUsedToColour(IGNodeColour c) {return (_numberOfColoursUsedToColour = c);}
   };
#endif
