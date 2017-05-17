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

#ifndef DOMINATORS_INCL
#define DOMINATORS_INCL

#include <stddef.h>                 // for NULL
#include <stdint.h>                 // for int32_t
#include "compile/Compilation.hpp"  // for Compilation
#include "cs2/tableof.h"            // for TableOf
#include "env/TRMemory.hpp"         // for Allocator, TR_Memory, etc
#include "il/Block.hpp"             // for Block
#include "il/Node.hpp"              // for vcount_t
#include "infra/Cfg.hpp"            // for CFG
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
   TR::Block       *getDominator(TR::Block *);
   int             dominates(TR::Block *block, TR::Block *other);

   TR::Compilation * comp()         { return _compilation; }
   TR_Memory *      trMemory()      { return comp()->trMemory(); }
   TR_StackMemory   trStackMemory() { return trMemory(); }
   bool trace() { return _trace; }

   TR::deque<int32_t> _dfNumbers;

   protected:

   typedef TR::SparseBitVector SparseBitVector;

   private:

   struct BBInfo
      {
      BBInfo(TR::Allocator allocator) :
         _bucket(allocator), _block(NULL), _parent(-1), _idom(-1), _ancestor(-1), _label(-1), _child(-1), _sdno(-1), _size(0)
         {}
      TR::Block     *_block;     // The block whose info this is
      int32_t       _parent;    // The parent in the depth-first spanning tree
      int32_t       _idom;      // The immediate dominator for this block
      int32_t       _ancestor;  // The ancestor in the forest
      int32_t       _label;     // The node in the ancestor chain with minimal
                                // semidominator number
      SparseBitVector _bucket;    // The blocks whose semidominator is this node
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

   BBInfo& getInfo(int32_t index) {return _info[index];}
   int32_t blockNumber(int32_t index) {return _info[index]._block->getNumber();}

   void    findDominators(TR::Block *start);
   void    initialize(TR::Block *block, BBInfo *parent);
   int32_t eval(int32_t);
   void    compress(int32_t);
   void    link(int32_t, int32_t);


   TR::Compilation *_compilation;
   TR::deque<BBInfo>  _info;
   TR::deque<TR::Block *> _dominators;
   int32_t         _numNodes;
   int32_t         _topDfNum;
   vcount_t        _visitCount;

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

   typedef CS2::TableOf<SparseBitVector, TR::Allocator> DependentsTable;

   TR_PostDominators(TR::Compilation * comp) : TR_Dominators(comp, true)
      , _directControlDependents(comp->getFlowGraph()->getNumberOfNodes()+1, comp->allocator("PostDominators"))
      {
      }

   void findControlDependents();
   bool isValid() {return _isValid;}
   int32_t numberOfBlocksControlled(int32_t block);

   private:
   typedef TR::BitVector BitVector;
   int32_t countBlocksControlled(int32_t block, BitVector &seen);
   DependentsTable _directControlDependents;
   };
#endif
