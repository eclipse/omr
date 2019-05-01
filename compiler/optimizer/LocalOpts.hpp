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

#ifndef LOCALOPTS_INCL
#define LOCALOPTS_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/KnownObjectTable.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_RegionStructure;
class TR_RematAdjustments;
class TR_RematState;
class TR_Structure;
namespace TR { class CFGEdge; }
namespace TR { class Optimizer; }
namespace TR { class ParameterSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }
struct TR_BDChain;
template <class T> class TR_LinkHeadAndTail;

bool collectSymbolReferencesInNode(TR::Node *node,
                                   TR::SparseBitVector &symbolReferencesInNode,
                                   int32_t *numDeadSubNodes, vcount_t visitCount, TR::Compilation *c,
                                   bool *seenInternalPointer = NULL , bool *seenArraylet = NULL,
                                   bool *cantMoveUnderBranch = NULL);

/*
 * Class TR_HoistBlocks
 * ====================
 *
 * Block hoisting duplicates code in a block and places it into its predecessors 
 * that only have the block as a unique successor. This would enable better/more 
 * commoning or other optimizations to occur.
 */

class TR_HoistBlocks : public TR::Optimization
   {
public:
   TR_HoistBlocks(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_HoistBlocks(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   int32_t process(TR::TreeTop *, TR::TreeTop *);
   bool hasSynergy(TR::Block *, TR::Node *);
   virtual const char * optDetailString() const throw();
   };

class TR_BlockManipulator : public TR::Optimization
   {
public:
   TR_BlockManipulator(TR::OptimizationManager *manager)
       : TR::Optimization(manager)
   {}

   int32_t performChecksAndTreesMovement(TR::Block *, TR::Block *, TR::Block *, TR::TreeTop *, vcount_t, TR::Optimizer *);
   TR::Block *getBestChoiceForExtension(TR::Block *);
   bool isBestChoiceForFallThrough(TR::Block *, TR::Block *);
   int32_t estimatedHotness(TR::CFGEdge *, TR::Block *);
   TR::Block *breakFallThrough(TR::Block *faller, TR::Block *fallee, bool isOutlineSuperColdBlock = true);
   int32_t countNumberOfTreesInSameExtendedBlock(TR::Block *block);
   };

class TR_ExtendBasicBlocks : public TR_BlockManipulator
   {
public:
   TR_ExtendBasicBlocks(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_ExtendBasicBlocks(manager);
      }

   virtual int32_t perform();
   virtual const char *optDetailString() const throw();

private:
   int32_t orderBlocksWithFrequencyInfo();
   int32_t orderBlocksWithoutFrequencyInfo();
   };

class TR_PeepHoleBasicBlocks : public TR_BlockManipulator
   {
public:
   TR_PeepHoleBasicBlocks(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_PeepHoleBasicBlocks(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();
   };


/*
 * Class TR_EliminateRedundantGotos
 * ================================
 *
 * Redundant goto elimination changes a conditional branch that branches to 
 * a block containing only a goto to branch directly to the destination 
 * of the goto.
 */

class TR_EliminateRedundantGotos : public TR::Optimization
   {
public:
   TR_EliminateRedundantGotos(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_EliminateRedundantGotos(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual const char * optDetailString() const throw();
   int32_t process(TR::TreeTop *, TR::TreeTop *);

private:
   void placeAsyncCheckBefore(TR::TreeTop *);
   void renumberInAncestors(TR_Structure *str, int32_t newNumber);
   void renumberExitEdges(TR_RegionStructure *region, int32_t oldN, int32_t newN);
   void redirectPredecessors(TR::Block *block, TR::Block *destBlock, const TR::CFGEdgeList &preds, bool emptyBlock, bool asyncMessagesFlag);
   void fixPredecessorRegDeps(TR::Node *regdepsParent, TR::Block *destBlock);
   };

/*
 * Class TR_CleanseTrees
 * =====================
 *
 * Cleanse Trees changes the textual order in which blocks are laid down to 
 * catch some specific patterns that are created by optimizations like 
 * loop canonicalizer. T
 */

class TR_CleanseTrees : public TR_BlockManipulator
   {
public:
   TR_CleanseTrees(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_CleanseTrees(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual const char * optDetailString() const throw();
   int32_t process(TR::TreeTop *, TR::TreeTop *);
   };


class TR_ArraysetStoreElimination : public TR::Optimization
   {
public:
   TR_ArraysetStoreElimination(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_ArraysetStoreElimination(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   int32_t process(TR::TreeTop *, TR::TreeTop *);
   void reduceArraysetStores(TR::Block *, TR_BitVector *, TR_BitVector *, TR_BitVector *);
   bool optimizeArraysetIfPossible(TR::Node *, TR::Node *, TR::TreeTop *, TR::Node *, TR_BitVector *, TR_BitVector *, TR_BitVector *, vcount_t, TR::TreeTop *);
   virtual const char * optDetailString() const throw();
   };

/*
 * Class TR_CompactNullChecks
 * ==========================
 *
 * Minimizes the cases where an explicit null check needs to be performed 
 * (null checks are implicit, i.e. no code is generated for performing null checks; 
 * instead we use hardware trap on IA32). Sometimes (say as a result of dead 
 * store removal of a store to a field) an explicit null check node may need to 
 * be created in the IL trees (if the original dead store was responsible for 
 * checking nullness implicitly); this pass attempts to find another expression 
 * in the same block that can be used to perform the null check implicitly again.
 */

class TR_CompactNullChecks : public TR::Optimization
   {
public:
   TR_CompactNullChecks(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_CompactNullChecks(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   int32_t process(TR::TreeTop *, TR::TreeTop *);

   void compactNullChecks(TR::Block *, TR_BitVector *);
   bool replacePassThroughIfPossible(TR::Node *, TR::Node *, TR::Node *, TR::Node *, bool *, TR_BitVector *, vcount_t, vcount_t, TR::TreeTop *);

   bool replaceNullCheckIfPossible(TR::Node *cursorNode, TR::Node *objectRef, TR::Node *prevNode, TR::Node *currentParent, TR_BitVector *writtenSymbols, vcount_t visitCount, vcount_t initialVisitCount, bool &compactionDone);

   virtual const char * optDetailString() const throw();

   bool _isNextTree;
   };

/*
 * Class TR_SimplifyAnds
 * =====================
 *
 * And simplification is an optimization that is aimed at simplifying the 
 * control flow created primarily by the loop versioning optimization 
 * (block versioning emits similar trees as well though it usually 
 * does transformations in far fewer cases than loop versioning). It is 
 * also possible that other optimizations or indeed the original user 
 * program results in similar opportunities but its most common coming 
 * from loop versioning. Here is an example loop:
 * 
 * i=0;
 * while (i < N)
 *    {
 *    a[i] = a[i] + b[i];
 *    i++;
 *    }
 * 
 * This loop has 3 null checks and 3 bound checks in Java, one for each of 
 * the 3 array accesses. Each of these checks gets considered in isolation 
 * by the loop versioner and the following tests are done outside the loop 
 * to decide if the fast loop (without any null check or bound check) or 
 * the slow loop (with all the checks) should be executed.
 * 
 * if (a == null)     goto blockA; // test emitted for the null check on the read of a[i]
 * if (a.length <= N) goto blockA; // test emitted for the bound check on the read of a[i]
 * if (i < 0)         goto blockA; // second test emitted for the bound check on the read of a[i]
 * if (b == null)     goto blockA; // test emitted for the null check on the read of b[i]
 * if (b.length <= N) goto blockA; // test emitted for the bound check on the read of b[i]
 * if (i < 0)         goto blockA; // second test emitted for the bound check on the read of b[i]
 * if (a == null)     goto blockA; // test emitted for the null check on the write of a[i]
 * if (a.length <= N) goto blockA; // test emitted for the bound check on the write of a[i]
 * if (i < 0)         goto blockA; // second test emitted for the bound check on the write of a[i]
 * 
 * There are obviously some redundant tests emitted here by the loop versioner 
 * but that optimization simply relies on later cleanup passes to eliminate those 
 * redundant tests. This is where and simplification comes in. The above control flow 
 * construct is similar to a "logical and" of all those conditions and this is 
 * where the name of the optimization comes from.
 * 
 * And simplification runs after basic block extension has been done and a long 
 * extended block created containing the above cascade of tests and local CSE has 
 * commoned up expressions within that extended block. This means that there 
 * are several pairs of "obvious" semantically equivalent trees in the above 
 * extended block, namely, nodes with the same IL opcode and the same children. 
 * This is the exact pattern that and simplification looks for and optimizes in 
 * a manner such that a given test (opcode) will only be done once in an 
 * extended block with the exact same children. e.g. in the above example we 
 * will not test if (a == null) more than once and ditto for if (i < 0). Note 
 * it may be possible for other optimizations to do this cleanup, in particular 
 * value propagation knows about relations between expressions but and 
 * simplification is a very simple cheap pass compared to value propagation 
 * and can be run more times if deemed appropriate.
 * 
 * There was also another unrelated transformation that was subsequently 
 * added to and simplification to optimize:
 * 
 * t = rhs1;
 * if (cond)
 *   t = t + rhs2;
 * 
 * to:
 * 
 * t = rhs1;
 * t = t + (rhs2 & ~(cond-1));
 * 
 * which is a branch free but equivalent code sequence that was beneficial 
 * on one of the important Java benchmarks. This minor control flow simplifying 
 * transformation could be separated into a different pass strictly speaking 
 * but is done as part of the pass as it stands.
 */

class TR_SimplifyAnds : public TR::Optimization
   {
public:
   TR_SimplifyAnds(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_SimplifyAnds(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   int32_t process(TR::TreeTop *, TR::TreeTop *);
   virtual const char * optDetailString() const throw();
   };

/*
 * Class TR_Rematerialization
 * ==========================
 *
 * Rematerialization is an optimization that aims to avoid register spills by 
 * un-commoning certain nodes that would be cheaper to simply re-evaluate. A 
 * simple example is if we have a load of a parm commoned across a range of 
 * code that has very high register pressure. The commoned load obviously 
 * adds to register pressure and could be the trigger for spills occurring 
 * across the area of high register pressure if the local register allocator 
 * simply cannot fit in all the values into available registers.
 * 
 * Rematerialization keeps track of the number of commoned expressions at 
 * every point as it walks over the IL trees and if it sees that the number 
 * of commoned expressions is more than the available registers on that platform, 
 * it un-commons one of the expressions if possible (after making sure this 
 * will not change the program's behaviour). This would cause the expression to 
 * be evaluated again (i.e. the expression must be "rematerialized") and in the 
 * case of our load of an auto this should be cheaper than spilling to a stack 
 * slot and reloading from that slot. Some cheap arithmetic operations are also 
 * considered cheap enough to re-evaluate instead of spilling, e.g. iadd.
 * 
 * Finally there are some expressions that can be un-commoned regardless of 
 * register pressure. One example is some addressing expressions that can be 
 * folded into a memory reference on some platforms depending on the addressing 
 * modes that are available. e.g. a shift and add can be folded into the memory 
 * reference on X86...so we do not need to common those operations under 
 * aiadd/aladd on that platform since it need never be evaluated into a register 
 * if we did not common the aiadd/aladd or any of its children.
 * 
 * One question might be: Why do we have rematerialization un-common when we 
 * could have simply taught local CSE to avoid commoning in the first place? 
 * This goes back to the insight that commoning of nodes has more than just 
 * the benefit of keeping the value in a register; a commoned node also helps 
 * simplify some node pattern matching in places like simplifier. Such places 
 * do not need to check for two different nodes being the same semantically, 
 * they can just check for the same node on the assumption that if they were 
 * semantically equivalent then local CSE would have commoned them up anyway. 
 * Thus we rarely stop local CSE from commoning (though there are occasional 
 * cases in which this is the best option); we usually want to let it do its 
 * job and we also let downstream optimizations benefit from that commoning 
 * in the above manner. Rematerialization runs late enough in the optimization 
 * strategy that it no longer matters as much if un-commoning happens at 
 * that point; there are not very many pattern matching optimizations 
 * to follow anyway.
 * 
 * Generalizing this point, one should keep an open mind about creating an 
 * optimization pass to "undo" what an earlier optimization pass did 
 * rather than simply limit the earlier optimization to not do something; 
 * this is especially relevant when the "opportunity for undoing" could 
 * have arisen not just from an earlier optimization pass but also from 
 * the user having written their code in a particular suboptimal manner.
 */

#define LONGREG_NEST 5
class TR_Rematerialization : public TR::Optimization
   {
public:
   TR_Rematerialization(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_Rematerialization(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual const char * optDetailString() const throw();
   int32_t process(TR::TreeTop *, TR::TreeTop *);

   void addParentToList(TR::Node *, List<TR::Node> *, TR::Node *, List< List<TR::Node> > *);
   void removeNodeFromList(TR::Node *node, List<TR::Node> *nodes, List< List<TR::Node> > *parents, bool checkSymRefs, List<TR::Node> *loadsAlreadyVisited, List<TR::Node> *loadsAlreadyVisitedThatCannotBeRematerialized, const TR::SparseBitVector &aliases);

   void removeNodeFromList(TR::Node *node, List<TR::Node> *nodes, List< List<TR::Node> > *parents, bool checkSymRefs, List<TR::Node> *loadsAlreadyVisited = NULL, List<TR::Node> *loadsAlreadyVisitedThatCannotBeRematerialized = NULL)
      {
      TR::SparseBitVector EMPTY_SET(comp()->allocator());
      removeNodeFromList(node, nodes, parents, checkSymRefs, loadsAlreadyVisited, loadsAlreadyVisitedThatCannotBeRematerialized, EMPTY_SET);
      }

   void findSymsUsedInIndirectAccesses(TR::Node *, List<TR::Node> *, List< List<TR::Node> > *);

   //bool examineNode(TR::TreeTop *, TR::Node *, TR::Node *, int32_t, List<TR::Node> *, List<TR::Node> *, List< List<TR::Node> > *, List<TR::Node> *, List< List<TR::Node> > *, List<TR::Node> *, List<TR::Node> *);
   bool examineNode(TR::TreeTop *, TR::Node *, TR::Node *, vcount_t, TR_RematState *, TR_RematAdjustments &);
   void rematerializeNode(TR::TreeTop *, TR::Node *, TR::Node *, vcount_t, List<TR::Node> *, List<TR::Node> *, List< List<TR::Node> > *, List<TR::Node> *, List< List<TR::Node> > *, List<TR::Node> *, List<TR::Node> *, bool);

   void rematerializeSSAddress(TR::Node *parent, int32_t addrChildIndex);
   void rematerializeAddresses(TR::Node *indirectNode, TR::TreeTop *treeTop, vcount_t visitCount);
   bool isRematerializable(TR::Node *parent, TR::Node *node,  bool onlyConsiderOpCode = false);
   bool isRematerializableLoad(TR::Node *node, TR::Node *parent);

   int32_t _counter;
   TR::Block *_curBlock;
   bool _underGlRegDeps;
   int32_t *_heightArray;
   int32_t _nodeCount;

   List<TR::Node> _prefetchNodes;
   };

class TR_BlockSplitter : public TR::Optimization
   {
public:
   TR_BlockSplitter(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_BlockSplitter(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

private:
   double alpha;
   TR::Block *splitBlock(TR::Block *pred, TR_LinkHeadAndTail<BlockMapper>* bMap);
   TR_RegionStructure *getParentStructure(TR::Block *block);
   bool disableSynergy();
   bool isLoopHeader(TR::Block * block);
   bool isExitEdge(TR::Block *mergeNode, TR::Block *successor);
   bool hasIVUpdate(TR::Block *block);
   bool hasLoopAsyncCheck(TR::Block *block);
   void dumpBlockMapper(TR_LinkHeadAndTail<BlockMapper>* bMap);
   int32_t pruneAndPopulateBlockMapper(TR_LinkHeadAndTail<BlockMapper>* bMap, int32_t depth);
   int32_t synergisticDepthCalculator(TR_LinkHeadAndTail<BlockMapper>* bMap, TR::Block * startPoint);

   struct Synergy
      {
      ncount_t replicationCost;
      uint16_t upwardSynergy;
      uint16_t downwardSynergy;
      int16_t  blockFrequency;
      };

   int32_t processNode(TR::Node* node, int32_t blockIndex, TR_Array<uint32_t>* synergyIndices, TR_Array<Synergy>* synergies=NULL);
   double calculateBlockSplitScore(Synergy& synergy);
   void dumpSynergies(TR_Array<Synergy>* toDump);

   class TR_IndexedBinaryHeapElement
      {
   public:
      TR_ALLOC(TR_Memory::LocalOpts)

      TR_IndexedBinaryHeapElement(TR::Block * object, uint32_t  index)
         : _object(object),
           _index(index)
         { }

      TR::Block * getObject()
         {
         return _object;
         }

      void setObject(TR::Block * object)
         {
         _object = object;
         }

      uint32_t getIndex()
         {
         return _index;
         }

      void setIndex(uint32_t index)
         {
         _index = index;
         }

      int compareObjects(const void* l, const void* r)
         {
         TR::Block * left  = (TR::Block *)l;
         TR::Block * right = (TR::Block *)r;

         return left->getFrequency() - right->getFrequency();
         }

      int compareIndices(const void* l, const void* r)
         {
         TR_IndexedBinaryHeapElement* left  = (TR_IndexedBinaryHeapElement*)l;
         TR_IndexedBinaryHeapElement* right = (TR_IndexedBinaryHeapElement*)r;

         return left->getIndex() - right->getIndex();
         }

      bool objectLT(const void* r)
         {
         if (compareObjects(getObject(), r) <= -1)
             return true;
         else
             return false;
         }

      bool objectGT(const void* r)
         {
         if (compareObjects(getObject(), r) >= 1)
             return true;
         else
             return false;
         }

      bool objectEqual(const void* r)
         {
         if (compareObjects(getObject(), r) == 0)
             return true;
         else
             return false;
         }

      bool indexLT(const void* r)
         {
         if (compareIndices(this, r) <= -1)
             return true;
         else
             return false;
         }

      bool indexGT(const void* r)
         {
         if (compareIndices(this, r) >= 1)
             return true;
         else
             return false;
         }

      bool indexEqual(const void* r)
         {
         if (compareIndices(this, r) == 0)
             return true;
         else
             return false;
         }
    private:
      TR::Block * _object;
      uint32_t  _index;
      };


   class TR_BinaryHeap : public TR_Array<TR_IndexedBinaryHeapElement*>
      {
   public:

      TR_BinaryHeap(TR_Memory * m, uint32_t size=0)
         : TR_Array<TR_IndexedBinaryHeapElement*>(m, size == 0 ? 8 : size),
           _maxSize(size)
         { }

      void setMaxSize(uint32_t size)
         {
         while (_nextIndex > size)
            removeMin();
         _maxSize = size;
         }

      uint32_t getMaxSize()
         {
         return _maxSize;
         }

      uint32_t remove(TR_IndexedBinaryHeapElement* t)
         {
         TR_ASSERT(false, "Don't call remove for a specific object on a binaryHeap!\n");
	 return 0;
         }

      uint32_t add(TR_IndexedBinaryHeapElement* t)
         {
         if (
             _array[0] &&
             _maxSize > 0 &&
             _nextIndex >= _maxSize &&
             !t->objectGT(_array[0]->getObject())
            )
            return _nextIndex;

         while (_maxSize > 0 && _nextIndex >= _maxSize)
            {
            removeMin();
            }
         if (_nextIndex >= _internalSize)
            {
            growTo(_internalSize*2);
            }
         int i;

         for (i = _nextIndex; i > 0 && _array[i/2]->objectGT(t->getObject()); i /=2 )
            {
            //traceMsg("\tMoving %d to %d\n", i, i/2);
            _array[i] = _array[i/2];
            }
         //traceMsg("Storing new item at location %d, _nextIndex is %d\n", i, _nextIndex);
         _array[i] = t;

         return _nextIndex++;
         }

      TR_IndexedBinaryHeapElement* getMin()
         {
         return _array[0];
         }

      uint32_t removeMin()
         {
         --_nextIndex;
         TR_IndexedBinaryHeapElement* toMove = _array[_nextIndex];
         uint32_t i = 0;

         while(2 * i + 1 < _nextIndex)
            {
            uint32_t child = 2 * i + 1;
            if (child + 1 < _nextIndex && _array[child + 1]->objectLT(_array[child]->getObject()))
               {
               child++;
               }
            if (toMove->objectLT(_array[child]->getObject()))
               break;
            _array[i] = _array[child];
            i = child;
            }
         _array[i] = toMove;
         _array[_nextIndex] = NULL;
         return _nextIndex;
         }

      void dumpList(TR::Compilation * comp)
         {
         traceMsg(comp,"heap dump\n");
         for (uint32_t i = 0; i < _nextIndex; i++)
            {
            traceMsg(comp,"%d [idx %d], ", _array[i]->getObject()->getNumber(), _array[i]->getIndex());
            }
         traceMsg(comp,"end heap dump\n");
         }

   private:
      uint32_t _maxSize;
      };

   void heapElementQuickSort(TR_Array<TR_IndexedBinaryHeapElement*>* array, int32_t left, int32_t right);
   void quickSortSwap(TR_Array<TR_IndexedBinaryHeapElement*>* array, int32_t left, int32_t right);
   bool containCycle(TR::Block *blk, TR_LinkHeadAndTail<BlockMapper>* bMap);
   };

class TR_InvariantArgumentPreexistence : public TR::Optimization
   {
   public:
   TR_InvariantArgumentPreexistence(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_InvariantArgumentPreexistence(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:

   class ParmInfo
      {
      public:
      void clear(); // we know nothing

      void     setSymbol(TR::ParameterSymbol *s) { _symbol = s;}
      TR::ParameterSymbol *getSymbol() { return _symbol; }

      void     setNotInvariant()           { _isNotInvariant = true; }
      bool     isInvariant()               { return !_isNotInvariant; }

      void     setClassIsFixed()           { _classIsFixed = true; }
      bool     classIsFixed()              { return _classIsFixed; }

      void     setClassIsCurrentlyFinal()  { _classIsCurrentlyFinal = true; }
      bool     classIsCurrentlyFinal()     { return _classIsCurrentlyFinal; }

      void     setClassIsRefined()         { _classIsRefined = true; }
      bool     classIsRefined()            { return _classIsRefined; }

      void     setClass(TR_OpaqueClassBlock *clazz) { _clazz = clazz; }
      TR_OpaqueClassBlock *getClass() { return _clazz; }

      TR::KnownObjectTable::Index getKnownObjectIndex()             { return _knownObjectIndex; }
      void setKnownObjectIndex(TR::KnownObjectTable::Index index)   { _knownObjectIndex = index; }
      bool hasKnownObjectIndex()                                   { return _knownObjectIndex != TR::KnownObjectTable::UNKNOWN; }

      private:
      TR::ParameterSymbol * _symbol;
      TR_OpaqueClassBlock *_clazz;
      TR::KnownObjectTable::Index _knownObjectIndex;

      bool     _isNotInvariant;
      bool     _classIsFixed;
      bool     _classIsCurrentlyFinal;
      bool     _classIsRefined;
      };

   void processNode        (TR::Node *node, TR::TreeTop *treeTop, vcount_t visitCount);
   void processIndirectCall(TR::Node *node, TR::TreeTop *treeTop, vcount_t visitCount);
   void processIndirectLoad(TR::Node *node, TR::TreeTop *treeTop, vcount_t visitCount);
   bool convertCall(TR::Node *node, TR::TreeTop *treeTop);

   ParmInfo *getSuitableParmInfo(TR::Node *node);

   TR::SymbolReferenceTable *_peekingSymRefTab;
   ParmInfo     *_parmInfo;
   bool          _isOutermostMethod;
   bool _success;
   };

class TR_CheckcastAndProfiledGuardCoalescer : public TR::Optimization
   {
   public:
   TR_CheckcastAndProfiledGuardCoalescer(TR::OptimizationManager *manager):
      TR::Optimization(manager) {};
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_CheckcastAndProfiledGuardCoalescer(manager);
      }

   virtual int32_t perform();
   void doBasicCase (TR::TreeTop* checkcastTree, TR::TreeTop* profiledGuardTree);

   virtual const char * optDetailString() const throw();

   private:
      TR::Node* storeObjectInATemporary (TR::TreeTop* checkcastTree);
   };

class TR_ColdBlockMarker : public TR_BlockManipulator
   {
   public:
   TR_ColdBlockMarker(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_ColdBlockMarker(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   protected:
   void initialize();
   bool identifyColdBlocks();
   int32_t isBlockCold(TR::Block *block);
   bool hasNotYetRun(TR::Node *node);
   bool hasAnyExistingColdBlocks();

   bool _enableFreqCBO;
   bool _exceptionsAreRare;
   bool _notYetRunMeansCold;
   };

class TR_ColdBlockOutlining : public TR_ColdBlockMarker
   {
   public:
   TR_ColdBlockOutlining(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_ColdBlockOutlining(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   void reorderColdBlocks();
   };

class TR_ProfiledNodeVersioning : public TR::Optimization
   {
   public:
   TR_ProfiledNodeVersioning(TR::OptimizationManager *manager)
       : TR::Optimization(manager)
       {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_ProfiledNodeVersioning(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   // Use this as follows:
   //
   // TR_ByteCodeInfo oldInfo = temporarilySetProfilingBcInfoOnNewArrayLengthChild(newArray);
   // (do whatever)
   // newArray->getFirstChild()->setByteCodeInfo(oldIfdo);
   //
   static TR_ByteCodeInfo temporarilySetProfilingBcInfoOnNewArrayLengthChild(TR::Node *newArray, TR::Compilation *comp);

   };

// Look for simple anchored treetops that can be removed
// This is important to clean up the noOpt code for some WCode frontends that generate a lot of temporary symbols
class TR_TrivialDeadTreeRemoval : public TR::Optimization
   {
   public:
   TR_TrivialDeadTreeRemoval(TR::OptimizationManager *manager)
      : TR::Optimization(manager), _currentTreeTop(NULL), _currentBlock(NULL), _commonedTreeTopList(trMemory())
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_TrivialDeadTreeRemoval(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual const char * optDetailString() const throw();

   void transformBlock(TR::TreeTop * entryTree, TR::TreeTop * exitTree);
   void examineNode(TR::Node *node, vcount_t visitCount);

   static void preProcessTreetop(TR::TreeTop *treeTop, List<TR::TreeTop> &commonedTreeTopList, char *optDetails, TR::Compilation *comp);
   static void postProcessTreetop(TR::TreeTop *treeTop, List<TR::TreeTop> &commonedTreeTopList, char *optDetails, TR::Compilation *comp);
   static void processCommonedChild(TR::Node *child, TR::TreeTop *treeTop, List<TR::TreeTop> &commonedTreeTopList, char *optDetails, TR::Compilation *comp);
   private:
   TR_ScratchList<TR::TreeTop> _commonedTreeTopList;
   TR::TreeTop *_currentTreeTop;
   TR::Block *_currentBlock;

   };

class TR_TrivialBlockExtension : public TR::Optimization
   {
   public:

   TR_TrivialBlockExtension(TR::OptimizationManager *manager)
       : TR::Optimization(manager)
       {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_TrivialBlockExtension(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual const char * optDetailString() const throw();
   };

#endif
