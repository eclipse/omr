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

#if DEBUG

#ifndef DOMINATORSCHK_INCL
#define DOMINATORSCHK_INCL

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"

class TR_BitVector;
namespace TR { class Block; }

// Calculate the dominator tree. This uses the simple but O(n^2) algorithm
// described in Muchnick.

// The algorithm has two distinct parts.
// The first part of the algorithm performs a fixed point iteration until it
// computes the dominators of each node in the flow graph.
// The second part of the algorithm computes immediate dominators given that
// the sets of dominators have already been computed earlier.
// The algorithm uses an array of information data structures, one per basic block.
// The blocks are ordered in the array in depth-first order. The first entry
// in the array is a dummy node that is used as the root for the forest.
//

class TR_DominatorsChk
   {
   public:
   TR_ALLOC(TR_Memory::DominatorsChk)

   TR_DominatorsChk(TR::Compilation *);

   struct BBInfoChk
      {
      TR_ALLOC(TR_Memory::DominatorsChk)

      TR::Block     *_block;     // The block whose info this is
      BBInfoChk    *_idom;      // The immediate dominator for this block
      TR_BitVector *_bucket;    // The blocks that dominate this node
      TR_BitVector *_tmpbucket; // The Tmp set used in Muchnick's algorithm
                                // to compute immediate dominators
      };

   private :

   TR::Compilation * comp()          {return _compilation;}
   TR_Memory *      trMemory()      { return comp()->trMemory(); }
   TR_StackMemory   trStackMemory() { return trMemory(); }

   void    findDominators(TR::Block *start);
   void    initialize(TR::Block *block, TR::Block *parent);
   void    findImmediateDominators();
   void    initialize();

   TR::Compilation *_compilation;
   BBInfoChk      *_info;
   int32_t         _numBlocks;
   int32_t         _topDfNum;
   vcount_t         _visitCount;

   public :
   int32_t      *_dfNumbers;

   BBInfoChk* getDominatorsChkInfo()
      {
      return _info;
      }

   class StackInfo
      {
      public:
      TR::CFGEdgeList::iterator curIterator;
      TR::CFGEdgeList * list;
      int32_t                  parent;
      };

   };

#endif

#endif
