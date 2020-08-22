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

#ifndef DOMINATORS_INCL
#define DOMINATORS_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "infra/Cfg.hpp"
#include "infra/deque.hpp"

class TR_FrontEnd;
namespace TR { class CFGEdge; }
namespace TR { class ResolvedMethodSymbol; }
template <class T> class ListElement;

// Calculate the dominator tree. This uses the Lengauer and Tarjan algorithm
// described in Muchnick.

// The algorithm uses a forest of trees within the depth-first spanning tree to
// keep track of nodes that have been processed. This is called "the forest".
//
// The algorithm uses an array of information data structures, one per basic block.
// The blocks are ordered in the array in depth-first order. The first entry
// in the array is a dummy node that is used as the root for the forest.
//

class TR_Dominators
   {
   public:
   TR_ALLOC(TR_Memory::Dominators)

   TR_Dominators(TR::Compilation *, bool post = false);
   TR_Dominators(TR::Compilation *, TR::CFG* cfg, bool acceptUnreachableBlocks = true, bool post = false);
   TR::Block       *getDominator(TR::Block *);
   int             dominates(TR::Block *block, TR::Block *other);

   TR::Compilation * comp()         { return _compilation; }
   bool trace() { return _trace; }
   bool isValid() { return _isValid; }
   bool hasUnreachableBlocks() { return _hasUnreachableBlocks; }

   protected:

   typedef TR_BitVector BitVector;

   private:

   struct BBInfo
      {
      explicit BBInfo(TR::Region &region) :
         _bucket(region), _block(NULL), _parent(-1), _idom(-1), _ancestor(-1), _label(-1), _child(-1), _sdno(-1), _size(0)
         {}
      TR::Block     *_block;     // The block whose info this is
      int32_t       _parent;    // The parent in the depth-first spanning tree
      int32_t       _idom;      // The immediate dominator for this block
      int32_t       _ancestor;  // The ancestor in the forest
      int32_t       _label;     // The node in the ancestor chain with minimal
                                // semidominator number
      BitVector _bucket;    // The blocks whose semidominator is this node
      int32_t       _child;

      int32_t       _sdno;      // The index of the semidominator for this block
      int32_t       _size;      // size and child are used for balancing the forest

      int32_t getIndex() {return _block?_block->getNumber()+1:-1;}
#ifdef DEBUG
      void print(TR_FrontEnd *fe, TR::FILE *pOutFile);
#else
      void print(TR_FrontEnd *fe, TR::FILE *pOutFile) { }
#endif
      };

   struct StackInfo
      {
      typedef TR::CFGEdgeList list_type;
      typedef TR::CFGEdgeList::iterator iterator_type;
      StackInfo(list_type &list, iterator_type position, int32_t parent) :
         list(list),
         listPosition(position),
         parent(parent)
         {
         }

      StackInfo(const StackInfo &other) :
         list(other.list),
         listPosition(other.listPosition),
         parent(other.parent)
         {
         }

      list_type &list;
      iterator_type listPosition;
      int32_t parent;
      };

   void    construct(TR::CFG* cfg, bool post, bool acceptUnreachableBlocks = true);

   BBInfo& getInfo(int32_t index) {return _info[index];}
   int32_t blockNumber(int32_t index) {return _info[index]._block->getNumber();}

   void    findDominators(TR::Block *start);
   void    initialize(TR::Block *block, BBInfo *parent);
   int32_t eval(int32_t);
   void    compress(int32_t);
   void    link(int32_t, int32_t);

   protected:
   TR::Region _region;
   public:
   TR::deque<int32_t, TR::Region&> _dfNumbers;
   private:
   TR::Compilation *_compilation;
   TR::deque<BBInfo, TR::Region&>  _info;
   TR::deque<TR::Block *, TR::Region&> _dominators;
   int32_t         _numNodes;
   int32_t         _topDfNum;
   vcount_t        _visitCount;
   bool            _hasUnreachableBlocks;

   protected:
   TR::CFG *         _cfg;
   bool            _postDominators;
   bool            _isValid;
   bool            _trace;
   };



class TR_PostDominators : public TR_Dominators
   {
   public:
   TR_ALLOC(TR_Memory::Dominators)

   typedef TR_BitVector** DependentsTable;

   TR_PostDominators(TR::Compilation * comp) : TR_Dominators(comp, true)
      , _directControlDependents(NULL)//comp->getFlowGraph()->getNumberOfNodes()+1, comp->allocator("PostDominators"))
      {
      }

   void findControlDependents();
   bool isValid() {return _isValid;}
   int32_t numberOfBlocksControlled(int32_t block);

   private:
   typedef TR_BitVector BitVector;
   int32_t countBlocksControlled(int32_t block, BitVector &seen);
   DependentsTable _directControlDependents;
   };
#endif
