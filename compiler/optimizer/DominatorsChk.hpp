/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#if DEBUG

#ifndef DOMINATORSCHK_INCL
#define DOMINATORSCHK_INCL

#include <stdint.h>                 // for int32_t
#include "compile/Compilation.hpp"  // for Compilation
#include "env/TRMemory.hpp"         // for TR_Memory, etc
#include "il/Node.hpp"              // for vcount_t

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
