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

#ifndef LA_INCL
#define LA_INCL

#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "infra/BitVector.hpp"

namespace TR { class Block; }
namespace TR { class Optimizer; }
namespace TR { class TreeTop; }

class TR_LocalAnalysisInfo
   {
   public:
   TR_ALLOC(TR_Memory::LocalAnalysis)
   typedef TR_BitVector ContainerType;

   TR_LocalAnalysisInfo(TR::Compilation *comp, bool);

   TR::Compilation *comp()            { return _compilation; }

   TR_Memory *    trMemory()         { return _trMemory; }
   TR_StackMemory trStackMemory()    { return _trMemory; }
   TR_HeapMemory  trHeapMemory()     { return _trMemory; }

   struct LAInfo
      {
      TR_ALLOC(TR_Memory::LocalAnalysis)

      TR::Block *_block;
      ContainerType *_analysisInfo;
      ContainerType *_downwardExposedAnalysisInfo;
      ContainerType *_downwardExposedStoreAnalysisInfo;
      };

#define NODES_PER_CHUNK 3
   class HashTable
      {
      public:
      HashTable(int32_t numBuckets, TR::Compilation *comp)
         : _allocator(comp->allocator()), _numBuckets(numBuckets)
         {
         _buckets = (Chunk**)_allocator.allocate(numBuckets*sizeof(Chunk*));
         memset(_buckets, 0, numBuckets*sizeof(Chunk*));
         }
      ~HashTable()
         {
         int32_t i;
         for (i = _numBuckets-1; i >= 0; i--)
            {
            Chunk *chunk, *next;
            for (chunk = _buckets[i]; chunk; chunk = next)
               {
               next = chunk->_next;
               _allocator.deallocate(chunk, sizeof(Chunk));
               }
            }
         _allocator.deallocate(_buckets, _numBuckets*sizeof(Chunk*));
         }
      void add(TR::Node *node, int32_t bucket)
         {
         if (!_buckets[bucket])
            _buckets[bucket] = allocateChunk();
         Chunk *chunk = _buckets[bucket];
         int32_t i;
         for (i = 0; i < NODES_PER_CHUNK; i++)
            {
            if (!chunk->_nodes[i])
               {
               chunk->_nodes[i] = node;
               return;
               }
            }
         _buckets[bucket] = allocateChunk();
         _buckets[bucket]->_next = chunk;
         _buckets[bucket]->_nodes[0] = node;
         }
      int32_t hash(TR::Node *);

      private:
      struct Chunk
         {
         Chunk *_next;
         TR::Node *_nodes[NODES_PER_CHUNK];
         };

      public:
      class Cursor
         {
         public:
         Cursor(HashTable *table, int32_t bucket);
         TR::Node *firstNode();
         TR::Node *nextNode();

         private:
         HashTable &_table;
         Chunk     *_chunk;
         int32_t    _bucket;
         int32_t    _index;
         };

      private:
      friend class Cursor;
      Chunk *allocateChunk()
         {
         Chunk *chunk = (Chunk*)_allocator.allocate(sizeof(Chunk));
         memset(chunk, 0, sizeof(Chunk));
         return chunk;
         }
      TR::Allocator _allocator;
      int32_t _numBuckets;
      Chunk **_buckets;
      };

   //void initialize(TR::Block *);
   int hasOldExpressionOnRhs(TR::Node *node, bool recalcContainsCall = true, bool containsCallInStoreLhsSubTree = false);
   bool collectSupportedNodes(TR::Node *, TR::Node *);
   bool countSupportedNodes(TR::Node *node, TR::Node *parent, bool &containsCallInStoreLhs);
   bool areSyntacticallyEquivalent(TR::Node *, TR::Node *);
   bool isCallLike(TR::Node *node);
   bool containsCall(TR::Node *, bool &);
   bool containsCallInTree(TR::Node *, bool &);
   void containsCallResetVisitCounts(TR::Node *);
   bool trace() { return _trace; }

   TR::Compilation *_compilation;
   TR_Memory * _trMemory;
   //TR::Block **_blocksInfo;
   TR::Node **_supportedNodesAsArray;
   TR::Node **_nullCheckNodesAsArray;
   TR::Optimizer *_optimizer;
   HashTable *_hashTable;
   ContainerType *_checkSymbolReferences;
   ContainerType *_checkExpressions;
   int32_t _numNodes;
   int32_t _numNullChecks;
   vcount_t _visitCount;
   int32_t _numBlocks;
   bool    _trace;
   };

class TR_LocalAnalysis
   {
   public:
   TR_ALLOC(TR_Memory::LocalAnalysis)

   typedef TR_LocalAnalysisInfo::ContainerType ContainerType;

    TR_LocalAnalysis(TR_LocalAnalysisInfo &, bool);

   void initializeLocalAnalysis(bool, bool = false);
   void initializeBlocks(TR::Block *, TR::BitVector &);

   TR::Compilation *comp() {return _lainfo._compilation;}

   TR_Memory *    trMemory()         { return comp()->trMemory(); }
   TR_StackMemory trStackMemory()    { return comp()->trMemory(); }
   TR_HeapMemory  trHeapMemory()     { return comp()->trMemory(); }

   int32_t       getNumNodes() {return _lainfo._numNodes;}
   bool          trace()       {return _trace; }
   ContainerType *getAnalysisInfo(int32_t blockNum)
      {return _info[blockNum]._analysisInfo;}
   ContainerType *getDownwardExposedAnalysisInfo(int32_t blockNum)
      {return _info[blockNum]._downwardExposedAnalysisInfo;}
   ContainerType *getDownwardExposedStoreAnalysisInfo(int32_t blockNum)
      {return _info[blockNum]._downwardExposedStoreAnalysisInfo;}


   ContainerType *getCheckSymbolReferences() {return _lainfo._checkSymbolReferences;}
   ContainerType *getCheckExpressions() {return _lainfo._checkExpressions;}

   // Note that the properties list needs to be updated if any new opcode is
   // required to be commoned.
   // Also called by Partial Redundancy since the list of supported opcodes must
   // be the same for both.
   //
   static bool isSupportedOpCode(TR::ILOpCode &opCode, TR::Compilation *comp) { return opCode.isSupportedForPRE(); }

   static bool isSupportedNodeForFieldPrivatization(TR::Node * node, TR::Compilation * comp, TR::Node * parent);
   static bool isSupportedNodeForPREPerformance(TR::Node * node, TR::Compilation * comp, TR::Node * parent);
   static bool isSupportedNodeForFunctionality(TR::Node * node, TR::Compilation * comp, TR::Node * parent, bool isSupportedStoreNode = false);
   static bool isSupportedNode(TR::Node *node, TR::Compilation *comp, TR::Node *parent, bool isSupportedStoreNode = false);

   protected:

   ContainerType *allocateContainer(int32_t size)
      {
      return new (trStackMemory()) TR_BitVector(size, trMemory(), stackAlloc, notGrowable);
      }
   ContainerType *allocateTempContainer(int32_t size)
      {
      return new (trStackMemory()) TR_BitVector(size, trMemory(), stackAlloc, notGrowable);
      }

   TR_LocalAnalysisInfo::LAInfo *_info;
   TR_LocalAnalysisInfo & _lainfo;
   bool _registersScarce;
   bool _trace;
   };



class TR_LocalTransparency : public TR_LocalAnalysis
   {
   public:

   TR_LocalTransparency(TR_LocalAnalysisInfo &, bool);
   void updateInfoForSupportedNodes(TR::Node *, ContainerType *, ContainerType *, ContainerType *, ContainerType *, ContainerType *, ContainerType *, vcount_t);
   void updateUsesAndDefs(TR::Node *, ContainerType *, ContainerType *, ContainerType *,  ContainerType *, ContainerType *, vcount_t, ContainerType *, TR_BitVector *, ContainerType *);
   void adjustInfoForAddressAdd(TR::Node *, TR::Node *, ContainerType *, ContainerType *);

   bool loadaddrAsLoad() {return _loadaddrAsLoad;}
   ContainerType *getTransparencyInfo(int32_t);


   private:

   TR::TreeTop *_checkTree;
   ContainerType *_hasTransparencyInfoFor;
   ContainerType **_transparencyInfo;
   ContainerType **_blockCheckTransparencyInfo;
   ContainerType *_supportedNodes;
   int32_t _storedSymbolReferenceNumber;
   bool _isStoreTree;
   bool _isNullCheckTree;
   bool _inNullCheckReferenceSubtree;
   bool _inStoreLhsTree;
   bool _loadaddrAsLoad;
   };


/**
 * This is pretty much a straight implementation of what is in Muchnick Section
 * 13.3 on Partial Redundancy Elimination (PRE). Local anticipatability is one
 * of the 2 local analyses (along with 5 global analyses) that make up the PRE
 * analysis.
 *
 * Local analyses occur on a per basic block (or extended basic block after
 * extension is done) granularity in Testarossa and this analysis itself deals
 * with a fairly simple question : For every expression in the basic block it
 * figures out whether the value of that expression would be changed if it were
 * computed immediately upon entry to that block (There is also implemented a
 * downward exposed version of this where we figure out if the values of that
 * expression would be changed if it were computed at the very end of the
 * block).
 *
 * This analysis helps identify a) which expressions appear in which basic
 * blocks and b) whether those expressions can "move" up or down past that
 * basic block (code motion and global commoning are what PRE aims to do and
 * this question is fundamental to both those transformations). It does this
 * analysis by walking the block and looking at kill points and depending on
 * where the kill points are relative to the expression itself, it determines
 * whether the expression can be moved.
 *
 * For a more rigourous explanation of this analysis (and indeed all of PRE)
 * please read Muchnick or the Lazy Code Motion paper by Knoop, Ruthing and
 * Steffan (PLDI 1992)
 */
class TR_LocalAnticipatability : public TR_LocalAnalysis
   {
   public:

   TR_LocalAnticipatability(TR_LocalAnalysisInfo &, TR_LocalTransparency *, bool trace);
   void analyzeBlock(TR::Block *, vcount_t, vcount_t, TR_BitVector *);
   bool updateAnticipatabilityForSupportedNodes(TR::Node *, ContainerType *, ContainerType *, TR::Block *, ContainerType *, ContainerType *, ContainerType *, TR_BitVector *, ContainerType *, vcount_t);
   void updateUsesAndDefs(TR::Node *, TR::Block *, ContainerType *, ContainerType *, ContainerType *, ContainerType *, TR_BitVector *, ContainerType *, vcount_t);
   bool adjustInfoForAddressAdd(TR::Node *, TR::Node *, ContainerType *, ContainerType *, ContainerType *, ContainerType *, TR::Block *);
   bool loadaddrAsLoad() {return _loadaddrAsLoad;}
   void killDownwardExposedExprs(TR::Block *, ContainerType *, TR::Node *node);
   void killDownwardExposedExprs(TR::Block *, TR::Node *node);

   private:

   TR::TreeTop *_checkTree;
   ContainerType *_seenCallSymbolReferences;
   int32_t _storedSymbolReferenceNumber;
   bool _isStoreTree;
   bool _isNullCheckTree;
   bool _inNullCheckReferenceSubtree;
   bool _inStoreLhsTree;
   bool _loadaddrAsLoad;
   TR_LocalTransparency * _localTransparency;
   ContainerType *_downwardExposedBeforeButNotAnymore;
   ContainerType *_notDownwardExposed;
   ContainerType *_temp;
   ContainerType *_temp2;
   ContainerType *_visitedNodes;
   ContainerType *_visitedNodesAfterThisTree;
   };


#endif
