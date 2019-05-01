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

#ifndef LCA_INCL
#define LCA_INCL

#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "cs2/bitvectr.h"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_LoopCanonicalizer;
class TR_LoopPredictor;
class TR_LoopReducer;
class TR_LoopVersioner;
class TR_RegionStructure;
class TR_Structure;
class TR_StructureSubGraphNode;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class NodeChecklist; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

class TR_NodeParentPair
   {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentPair(TR::Node *node, TR::Node *parent)
      : _node(node), _parent(parent)
      {
      }

   TR::Node *_node;
   TR::Node *_parent;
   };

class TR_LoopTransformer : public TR::Optimization
   {
   public:
   TR_LoopTransformer(TR::OptimizationManager *manager)
      : TR::Optimization(manager),
        _invariantBlocks(trMemory()),
        _blocksToBeCleansed(trMemory()),
        _analysisStack(trMemory(), 8, false, stackAlloc),
        _writtenAndNotJustForHeapification(NULL),
        _writtenExactlyOnce(comp()->allocator("LoopTransformer")),
        _readExactlyOnce(comp()->allocator("LoopTransformer")),
        _allKilledSymRefs(comp()->allocator("LoopTransformer")),
        _allSymRefs(comp()->allocator("LoopTransformer")),
        _autosAccessed(NULL)
      {
      _doingVersioning = false;
      _nodesInCycle = NULL;
      _indirectInductionVariable = false;
      }

   void initializeSymbolsWrittenAndReadExactlyOnce(int32_t symRefCount, TR_BitVectorGrowable growableOrNot)
      {
      _storeTrees = (TR::TreeTop **)trMemory()->allocateStackMemory(symRefCount*sizeof(TR::TreeTop *));
      memset(_storeTrees, 0, symRefCount*sizeof(TR::TreeTop *));

      _cannotBeEliminated = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growableOrNot);

      _neverRead = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growableOrNot);
      _neverWritten = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growableOrNot);

      TR::BitVector tmp(comp()->allocator("LoopTransformer"));
      comp()->getSymRefTab()->getAllSymRefs(tmp);

      *_neverWritten = tmp;
      *_neverRead = tmp;

      _writtenExactlyOnce.Clear();
      _readExactlyOnce.Clear();
      _allKilledSymRefs.Clear();
      _allSymRefs.Clear();
      }

   virtual int32_t perform(){return 0;}
   virtual TR_LoopCanonicalizer *asLoopCanonicalizer() {return NULL;}
   virtual TR_LoopReducer *asLoopReducer() { return NULL; }
   virtual TR_LoopVersioner *asLoopVersioner() {return NULL;}
   virtual TR_LoopPredictor *asLoopPredictor() {return NULL;}
   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t){return 0;}
   virtual bool isStoreInRequiredForm(int32_t, TR_Structure *);
   virtual int32_t checkLoopForPredictability(TR_Structure *, TR::Block *, TR::Node **, bool returnIfNotPredictable = true);
   virtual int32_t getInductionSymbolReference(TR::Node *);
   virtual void updateStoreInfo(int32_t i, TR::TreeTop *tree) { _storeTrees[i] = tree; }
   virtual void checkIfIncrementInDifferentExtendedBlock(TR::Block *block, int32_t inductionVariable);
   virtual TR::Node *updateLoadUsedInLoopIncrement(TR::Node *node, int32_t);


   protected:
   void createWhileLoopsList(TR_ScratchList<TR_Structure>* whileLoops);

   void detectWhileLoops(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_Structure *root, bool innerFirst);
   void detectWhileLoopsInSubnodesInOrder(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_Structure *root, TR_StructureSubGraphNode *rootNode, TR_RegionStructure *region, vcount_t visitCount, TR_BitVector *pendingList, bool innerFirst);
   void detectWhileLoopsInSubnodesInOrder(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_RegionStructure *region, vcount_t visitCount, TR_BitVector *pendingList, bool innerFirst);

   bool blockIsAlwaysExecutedInLoop(TR::Block *block, TR_RegionStructure *loopStructure, bool *atEntry = NULL);

   TR::Block * createNewEmptyBlock();
   TR::Node* createNewGotoNode();
   void printTrees();
   void adjustTreesInBlock(TR::Block *);
   TR::Node *duplicateExact(TR::Node *, List<TR::Node> *, List<TR::Node> *);
   bool cleanseTrees(TR::Block *);
   bool makeInvariantBlockFallThroughIfPossible(TR::Block *);

   void collectSymbolsWrittenAndReadExactlyOnce(TR_Structure *, vcount_t);

 private:
   struct updateInfo_tables {
     TR::BitVector seenLoads, seenMultipleLoads, seenStores, seenMultipleStores, currentlyWrittenOnce, currentlyReadOnce;
     updateInfo_tables(TR::Allocator a) : seenLoads(a), seenMultipleLoads(a),
                                               seenStores(a), seenMultipleStores(a),
                                               currentlyWrittenOnce(a), currentlyReadOnce(a) {}
   };
   void updateInfo(TR::Node *, vcount_t, updateInfo_tables &);
   void collectSymbolsWrittenAndReadExactlyOnce(TR_Structure *, vcount_t, updateInfo_tables &);

 public:
   TR::Node *getCorrectNumberOfIterations(TR::Node *, TR::Node *);
   TR::Node *containsOnlyInductionVariableAndAdditiveConstant(TR::Node *, int32_t);
   bool isSymbolReferenceWrittenNumberOfTimesInStructure(TR_Structure *, int32_t, int32_t *, int32_t);
     //bool isSymbolReferenceReadNumberOfTimesInStructure(TR_Structure *, int32_t, int32_t *, int32_t);
   bool detectEmptyLoop(TR_Structure *, int32_t *);
   bool findMatchingIVInRegion(TR::TreeTop*, TR_RegionStructure*);

   virtual bool replaceAllInductionVariableComputations(TR::Block *loopInvariantBlock, TR_Structure *, TR::SymbolReference **, TR::SymbolReference *);
   virtual bool examineTreeForInductionVariableUse(TR::Block *loopInvariantBlock, TR::Node *, int32_t, TR::Node *, vcount_t, TR::SymbolReference **)
      {
      TR_ASSERT(0, "Should be overridden in the subclass that makes use of this function\n");
      return true;
      }

   typedef enum
      {
      transformerNoReadOrWrite,
      transformerReadFirst,
      transformerWrittenFirst
      } TR_TransformerDefUseState;

   TR_TransformerDefUseState getSymbolDefUseStateInSubTree(TR::Node *node, TR::RegisterMappedSymbol* indVarSym);
   TR_TransformerDefUseState getSymbolDefUseStateInBlock(TR::Block *block, TR::RegisterMappedSymbol* indVarSym);

   TR::Block *_loopTestBlock;
   TR::TreeTop **_storeTrees;
   TR::TreeTop *_currTree, *_insertionTreeTop, *_loopTestTree, *_asyncCheckTree;
   TR_BitVector *_cannotBeEliminated, *_writtenAndNotJustForHeapification, *_autosAccessed;
   TR_BitVector *_neverRead, *_neverWritten;
   TR::SparseBitVector _writtenExactlyOnce;
   TR::SparseBitVector _readExactlyOnce;
   TR::SparseBitVector _allKilledSymRefs;
   TR::SparseBitVector _allSymRefs;

   TR::Node *_numberOfIterations, *_constNode, *_loadUsedInLoopIncrement;

   TR::TreeTop    *_startOfHeader;
   TR::CFG        *_cfg;
   TR_Structure  *_rootStructure;

   List<TR::Block> _invariantBlocks;
   List<TR::Block> _blocksToBeCleansed;
   TR_Stack<TR_StructureSubGraphNode*> _analysisStack;
   TR_BitVector *_nodesInCycle;
   TR_BitVector *_hasPredictableExits;

   int32_t        _whileIndex;
   vcount_t       _visitCount;
   int32_t        _topDfNum;
   int32_t        _counter;
   int32_t _loopDrivingInductionVar, _nextExpression, _startExpressionForThisInductionVariable, _numberOfTreesInLoop;

   bool _isAddition, _requiresAdditionalCheckForIncrement, _doingVersioning;
   bool _incrementInDifferentExtendedBlock;
   bool _indirectInductionVariable; // JIT Design 1347

   TR::SymbolReference        **_symRefUsedInLoopIncrement;
   };


/**
 * Class TR_LoopCanonicalizer
 * ==========================
 *
 * The loop canonicalizer optimization transforms a while loop into 
 * an if-guarded do-while loop with a loop invariant (pre-header) 
 * block. The loop test is placed at the end of the trees for the 
 * loop, so that the loop back-edge is almost always a backwards branch.
 */

class TR_LoopCanonicalizer : public TR_LoopTransformer
   {
   public:

   TR_LoopCanonicalizer(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopCanonicalizer(manager);
      }

   virtual int32_t perform();
   virtual TR_LoopCanonicalizer *asLoopCanonicalizer() {return this;}
   virtual const char * optDetailString() const throw();

   protected:
   void eliminateRedundantInductionVariablesFromLoop(TR_RegionStructure *naturalLoop);

   private:
   void canonicalizeNaturalLoop(TR_RegionStructure *whileLoop);
   void canonicalizeDoWhileLoop(TR_RegionStructure *doWhileLoop);

   bool isLegalToSplitEdges(TR_RegionStructure *doWhileLoop, TR::Block *blockAtHeadOfLoop);
   bool modifyBranchesForSplitEdges(TR_RegionStructure *doWhileLoop, TR::Block *blockAtHeadOfLoop, TR::Block *loopInvariantBlock,
         TR::Block *targetBlock, bool addToEnd, int32_t *sumPredFreq, bool isCheckOnly = false);

   bool replaceInductionVariableComputationsInExits(TR_Structure *structure, TR::Node *node, TR::SymbolReference *newSymbolReference, TR::SymbolReference *primaryInductionVar, TR::SymbolReference *derivedInductionVar);
   virtual bool examineTreeForInductionVariableUse(TR::Block *loopInvariantBlock, TR::Node *, int32_t, TR::Node *, vcount_t, TR::SymbolReference **);
   void placeInitializationTreeInLoopPreHeader(TR::Block *b, TR::Node *node, TR::SymbolReference *newSymbolReference, TR::SymbolReference *primaryInductionVar, TR::SymbolReference *derivedInductionVar);
   bool incrementedInLockStep(TR_Structure *, TR::SymbolReference *, TR::SymbolReference *, int64_t derivedInductionVarIncrement, int64_t primaryInductionVarIncrement, TR_ScratchList<TR::Block> *derivedInductionVarIncrementBlocks, TR_ScratchList<TR::Block> *primaryInductionVarIncrementBlocks);
   void findIncrements(TR::Node * currentNode, vcount_t visitCount, TR::SymbolReference *derivedInductionVar, TR::SymbolReference *primaryInductionVar, int64_t &derivedInductionVarIncrement, int64_t &primaryInductionVarIncrement, bool &unknownIncrement);
   bool checkIfOrderOfBlocksIsKnown(TR_RegionStructure *naturalLoop, TR::Block *entryBlock, TR::Block *loopTestBlock, TR_ScratchList<TR::Block> *derivedInductionVarIncrementBlocks, TR_ScratchList<TR::Block> *primaryInductionVarIncrementBlocks, uint8_t &primaryFirst);
   bool checkComplexInductionVariableUse(TR_Structure *structure);
   bool checkComplexInductionVariableUseNode(TR::Node *node, bool inAddr);

   void rewritePostToPreIncrementTestInRegion(TR_RegionStructure *region);
   void rewritePostToPreIncrementTestInBlock(TR::Block *block);

   TR::SymbolReference *_symRefBeingReplaced;
   TR::SymbolReference *_primaryInductionVariable;
   TR::Node *_primaryInductionVarStoreInBlock;
   TR::Node *_derivedInductionVarStoreInBlock;
   TR::Node *_primaryInductionVarStoreSomewhereInBlock;
   uint8_t _primaryIncrementedFirst;
   TR::Block *_entryBlock;
   TR::Block *_loopTestBlock;
   TR::Block *_currentBlock;
   TR::Block *_primaryInductionIncrementBlock;
   TR::Block *_derivedInductionIncrementBlock;
   int64_t _primaryIncr;
   int64_t _derivedIncr;
   };


/**
 * Class TR_LoopInverter
 * =====================
 *
 * The loop inverter optimization converts a loop in which the induction 
 * variable counts up from zero into one in which the induction variable 
 * counts down to zero. Note that this is legal only if the inversion of 
 * the loop does not affect program semantics inside the loop (order of 
 * exceptions thrown, etc.). The benefit of inversion is that there are, 
 * in general, instructions that can perform compare/branch against zero 
 * in an efficient manner, and in some cases (e.g. PowerPC) special count 
 * registers can be used for counting.
 */

class TR_LoopInverter : public TR_LoopTransformer
   {
   public:

   TR_LoopInverter(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopInverter(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t);
   bool isInvertibleLoop(int32_t, TR_Structure *);
   bool checkIfSymbolIsReadInKnownTree(TR::Node *, int32_t, TR::TreeTop *, TR::NodeChecklist &);
   };

class TR_RedundantInductionVarElimination : public TR_LoopCanonicalizer
   {
   public:

   TR_RedundantInductionVarElimination(TR::OptimizationManager *manager)
       : TR_LoopCanonicalizer(manager)
       {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_RedundantInductionVarElimination(manager);
      }


   virtual int32_t perform();
   virtual const char * optDetailString() const throw();
   };

#endif
