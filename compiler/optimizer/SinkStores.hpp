/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
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

#ifndef SINKSTORES_INCL
#define SINKSTORES_INCL

#include <stddef.h>                           // for NULL
#include <stdint.h>                           // for int32_t, uint64_t, etc
#include "compile/Compilation.hpp"            // for Compilation
#include "env/TRMemory.hpp"                   // for TR_Memory, etc
#include "il/Node.hpp"                        // for vcount_t
#include "infra/Flags.hpp"                    // for flags16_t
#include "infra/List.hpp"                     // for List, etc
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_BitVector;
class TR_HashTab;
class TR_LiveOnAllPaths;
class TR_LiveVariableInformation;
class TR_Liveness;
class TR_MovableStore;
class TR_SinkStores;
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

// Store Sinking
// There are two implementations: TR_TrivialSinkStores and TR_GeneralSinkStores

class TR_LiveOnNotAllPaths
   {
   public:
   
   static void *operator new(size_t size, TR::Allocator a)
      { return a.allocate(size); }
   static void  operator delete(void *ptr, size_t size)
      { ((TR_LiveOnNotAllPaths*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() must return the same allocator as used for new */

   /* Virtual destructor is necessary for the above delete operator to work
    * See "Modern C++ Design" section 4.7
    */
   virtual ~TR_LiveOnNotAllPaths() {}
  
   TR_LiveOnNotAllPaths(TR::Compilation *comp, TR_Liveness *liveOnSomePaths, TR_LiveOnAllPaths *liveOnAllPaths);

   TR::Compilation *   comp()          { return _comp; }

   TR_Memory *          trMemory()      { return comp()->trMemory(); }
   TR_StackMemory       trStackMemory() { return trMemory(); }

   TR::Allocator        allocator()     { return comp()->allocator(); }

   TR::Compilation * _comp;

   int32_t        _numNodes;
   int32_t        _numLocals;
   TR_BitVector **_inSetInfo;
   TR_BitVector **_outSetInfo;
   };

class TR_FirstUseOfLoad
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)
   TR_FirstUseOfLoad(TR::Node *node, TR::TreeTop *anchorLocation, int32_t anchorBlockNumber) :
                   _node(node),_anchorLocation(anchorLocation),_anchorBlockNumber(anchorBlockNumber), _isAnchored(false) { }

   bool isAnchored() { return _isAnchored; }
   bool setIsAnchored(bool b = true) { return _isAnchored = b; }

   TR::TreeTop *getAnchorLocation() { return _anchorLocation; }
   TR::TreeTop *setAnchorLocation(TR::TreeTop *loc) { return _anchorLocation = loc; }

   TR::Node *getNode() { return _node; }
   TR::Node *setNode(TR::Node *n) { return _node = n; }

   int32_t getAnchorBlockNumber() { return _anchorBlockNumber; }
   int32_t setAnchorBlockNumber(int32_t b) { return _anchorBlockNumber = b; }

   private:
   int32_t _anchorBlockNumber;
   TR::Node *_node;
   TR::TreeTop *_anchorLocation;
   bool _isAnchored;
   };

class TR_CommonedLoad
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)
   TR_CommonedLoad(TR::Node *node, int32_t symIdx) : _node(node), _isSatisfied(false), _isKilled(false), _symIdx(symIdx) { }

   bool isSatisfied() { return _isSatisfied; }
   bool setIsSatisfied(bool b = true) { return _isSatisfied = b; }

   bool isKilled()  { return _isKilled; }
   bool setIsKilled(bool b = true)  { return _isKilled = b; }

   TR::Node *getNode() { return _node; }
   TR::Node *setNode(TR::Node *n) { return _node = n; }

   int32_t getSymIdx() { return _symIdx; }
   int32_t setSymIdx(int32_t i) { return _symIdx = i; }

   private:
   TR::Node *_node;
   bool _isSatisfied;
   bool _isKilled;
   int32_t _symIdx;
   };

class TR_UseOrKillInfo
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_UseOrKillInfo(TR::TreeTop *tt, TR::Block *block, TR_BitVector *usedSymbols, TR_BitVector *commonedSymbols, uint32_t &indirectLoadCount, TR_BitVector *killedSymbols) :
                   _tt(tt),
                   _block(block),
                   _usedSymbols(usedSymbols),
                   _commonedSymbols(commonedSymbols),
                   _indirectLoadCount(indirectLoadCount),
                   _killedSymbols(killedSymbols),
                   _movableStore(NULL){}

   TR::TreeTop      *_tt;
   uint32_t       _indirectLoadCount;
   TR::Block        *_block;
   TR_BitVector    *_usedSymbols;
   TR_BitVector    *_commonedSymbols;
   TR_BitVector    *_killedSymbols;
   TR_MovableStore *_movableStore;  // all TR_MovableStore's are also TR_UseOrKillInfo's so this is a pointer
                                    // to the corresponding movableStore (if one exists)
   };

class TR_MovableStore
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_MovableStore(TR_SinkStores *s,
                   TR_UseOrKillInfo *useOrKillInfo,
                   int32_t symIdx = -1,
                   TR_BitVector *commonedLoadsUnderTree = 0,
                   TR_BitVector *commonedLoadsAfter = 0,
                   int32_t depth = 0,
                   TR_BitVector *needTempForCommonedLoads = 0,
                    TR_BitVector *satisfiedCommonedLoads = 0);

   TR_UseOrKillInfo *_useOrKillInfo;
   int32_t       _symIdx;
   TR_BitVector *_commonedLoadsUnderTree;   // commoned used syms under this store in the IL tree (syms are not first used)
                                            // - syms that have associated temps are excluded (only when enablePreciseSymbolTracking() == false otherwise all syms are included)
   TR_BitVector *_commonedLoadsAfter;       // used syms which are commoned following the store (syms are first used and are commoned later)
   TR::Compilation * comp() { return _comp; }

   TR_Memory *    trMemory()      { return comp()->trMemory(); }
   TR_StackMemory trStackMemory() { return trMemory(); }

   TR::Compilation * _comp;

   TR_SinkStores * _s;
   int32_t       _depth;                    // a measure of store tree complexity
   bool          _movable;
   TR_BitVector *_needTempForCommonedLoads; // move stores with commoned load
   bool          _isLoadStatic;             // is this a store of a static load?

   // enablePreciseSymbolTracking() uses the data and routines below
   List<TR_CommonedLoad> *_commonedLoadsList;
   int32_t _commonedLoadsCount;
   int32_t _satisfiedCommonedLoadsCount;
   int32_t initCommonedLoadsList(TR::Node *node, vcount_t visitCount);
   bool satisfyCommonedLoad(TR::Node *node);
   bool containsCommonedLoad(TR::Node *node);
   bool containsSatisfiedAndNotKilledCommonedLoad(TR::Node *node);
   bool containsKilledCommonedLoad(TR::Node *node);
   TR_CommonedLoad *getCommonedLoad(TR::Node *node);
   bool areAllCommonedLoadsSatisfied();
   bool containsUnsatisfedLoadFromSymbol(int32_t symIdx);
   bool killCommonedLoadFromSymbol(int32_t symIdx);
   };

class TR_SideExitStorePlacement
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_SideExitStorePlacement(TR::Block *b, TR::TreeTop *t, bool copy)
    : _block(b), _store(t), _copyStore(copy) {}

   bool _copyStore;
   TR::Block *_block;
   TR::TreeTop *_store;
   };

class TR_IndirectLoadAnchor
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_IndirectLoadAnchor(TR::Node *node, TR::Block *anchorBlock, TR::TreeTop *anchorTree)
   : _node(node),_anchorBlock(anchorBlock),_anchorTree(anchorTree) {}

   // treetop <- this is node
   //    iiload

   TR::TreeTop *getAnchorTree() { return _anchorTree; }
   TR::TreeTop *setAnchorTree(TR::TreeTop *t) { return _anchorTree = t; }

   TR::Block *getAnchorBlock() { return _anchorBlock; }
   TR::Block *setAnchorBlock(TR::Block *b) { return _anchorBlock = b; }

   TR::Node *getNode() { return _node; }
   TR::Node *setNode(TR::Node *n) { return _node = n; }

   private:
   TR::TreeTop *_anchorTree;
   TR::Block *_anchorBlock;
   TR::Node *_node;
   };

class TR_StoreInformation
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)
   TR_StoreInformation(TR::TreeTop *store, bool copy, bool needsDuplication = true) :
                        _store(store), _copy(copy), _needsDuplication(needsDuplication), _storeTemp(NULL) { }

   TR::TreeTop *_store;   // original store to be sunk
   TR::TreeTop *_storeTemp;// dup store with commoned loads replaced by temp
   bool        _copy;    // whether original store should be copied or moved
                         //  it should be copied if the store is placed in a block that does NOT extend the original source block
                         //  it should be moved if it is to be placed in a block that extends the original source block
   bool        _needsDuplication;  // has the store already been duplicated by sinkStorePlacement or should it be done in doSinking
   };

class TR_EdgeInformation
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_EdgeInformation(TR::CFGEdge *edge, TR_BitVector *symbols) { _edge = edge; _symbolsUsedOrKilled = symbols; }

   TR::CFGEdge   *_edge;                  // edge where store will be placed
   TR_BitVector *_symbolsUsedOrKilled;   // symbols that are used or killed by stores placed along this edge
   };

class TR_EdgeStorePlacement
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_EdgeStorePlacement(TR_StoreInformation *store, TR_EdgeInformation *edge, TR_Memory * m)
      : _stores(m), _edges(m)
      { _stores.add(store); _edges.add(edge); }

   List<TR_StoreInformation> _stores; // stores to place here
   List<TR_EdgeInformation>  _edges;  // list of cfg edges to _block that should be split to place def
                                      // NOTE: only one of these edges need be split: others can simply be redirected to the new split block
   };

class TR_BlockStorePlacement
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_BlockStorePlacement(TR_StoreInformation *store, TR::Block *block, TR_Memory * m)
      : _stores(m)
      { _stores.add(store); _block = block; }

   List<TR_StoreInformation> _stores; // stores to place here
   TR::Block                 *_block;  // block where stores will be placed (at beginning)
   };


typedef List<TR_BlockStorePlacement> TR_BlockStorePlacementList;
typedef List<TR_EdgeStorePlacement> TR_EdgeStorePlacementList;

class TR_BlockListEntry
   {
   public:
   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_BlockListEntry(TR::Block *block, TR::CFGEdge *pred, TR::Region &region)
      : _preds(region)
      { _block = block; if (pred != 0) _preds.add(pred); _count = 1; }

   TR::Block       * _block;   // block
   List<TR::CFGEdge> _preds;   // list of edges from which we reached block
   int32_t          _count;   // cached count of _preds entries
   };

class TR_OrderedBlockList : public List<TR_BlockListEntry>
   {
   public:
   TR_OrderedBlockList(TR_Memory * m) : List<TR_BlockListEntry>(m) { }
   TR_ALLOC(TR_Memory::DataFlowAnalysis)
   ListElement<TR_BlockListEntry> *addInTraversalOrder(TR::Block *block, bool forward, TR::CFGEdge *edge);
   bool removeBlockFromList(TR::Block *block, TR::CFGEdge *edge);
   };

class TR_SinkStores : public TR::Optimization
   {
   public:
   // abstract base class for store sinking implementations
   // sinkStorePlacement and storeIsSinkingCandidate must be implemented
   //
   TR_SinkStores(TR::OptimizationManager *manager);

   bool usesDataFlowAnalysis()          {return _storeSinkingFlags.testAny(UsesDataFlowAnalysis);}
   void setUsesDataFlowAnalysis(bool b) {_storeSinkingFlags.set(UsesDataFlowAnalysis, b);}

   bool sinkMethodMetaDataStores()          {return _storeSinkingFlags.testAny(SinkMethodMetaDataStores);}
   void setSinkMethodMetaDataStores(bool b) {_storeSinkingFlags.set(SinkMethodMetaDataStores, b);}

   bool sinkStoresWithIndirectLoads()          {return _storeSinkingFlags.testAny(SinkStoresWithIndirectLoads);}
   void setSinkStoresWithIndirectLoads(bool b) {_storeSinkingFlags.set(SinkStoresWithIndirectLoads, b);}

   bool isExceptionFlagIsSticky()         {return _storeSinkingFlags.testAny(ExceptionFlagIsSticky);}
   void setExceptionFlagIsSticky(bool b)  {_storeSinkingFlags.set(ExceptionFlagIsSticky, b);}

   bool sinkStoresWithStaticLoads()          {return _storeSinkingFlags.testAny(SinkStoresWithStaticLoads);}
   void setSinkStoresWithStaticLoads(bool b) {_storeSinkingFlags.set(SinkStoresWithStaticLoads, b);}

   TR::RegisterMappedSymbol *getSinkableSymbol(TR::Node *node);

   bool enablePreciseSymbolTracking();

   private:
   virtual bool storeIsSinkingCandidate(TR::Block *block,
                                        TR::Node *node,
                                        int32_t symIdx,
                                        bool sinkIndirectLoads,
                                        uint32_t &indirectLoadCount,
                                        int32_t &depth,
                                        bool &isLoadStatic,
                                        vcount_t &treeVisitCount,
                                        vcount_t &highVisitCount) = 0;
   virtual bool sinkStorePlacement(TR_MovableStore *store, bool nextStoreWasMoved) = 0;
   void lookForSinkableStores();
   void doSinking();
   TR_EdgeInformation *findEdgeInformation(TR::CFGEdge *edge, List<TR_EdgeInformation> & edgeList);
   void coalesceSimilarEdgePlacements();
   void placeStoresAlongEdges(List<TR_StoreInformation> & stores, List<TR_EdgeInformation> & edges);
   void placeStoresInBlock(List<TR_StoreInformation> & stores, TR::Block *placementBlock);

   protected:
   int32_t performStoreSinking();
   virtual bool storeCanMoveThroughBlock(TR_BitVector *blockKilledSet, TR_BitVector *blockUsedSet, int32_t symIdx, TR_BitVector *allBlockUsedSymbols = NULL, TR_BitVector *allBlockKilledSymbols = NULL);
   bool treeIsSinkableStore(TR::Node *node, bool sinkIndirectLoads, uint32_t &indirectLoadCount, int32_t &depth, bool &isLoadStatic, vcount_t visitCount);
   bool checkLiveMergingPaths(TR_BlockListEntry *blockEntry, int32_t symIdx);
   bool shouldSinkStoreAlongEdge(int symIdx, TR::CFGNode *block, TR::CFGNode *succBlock, int32_t sourceBlockFrequency, bool isLoadStatic, vcount_t visitCount, TR_BitVector *allEdgeInfoUsedOrKilledSymbols);
   void recordPlacementForDefAlongEdge(TR_EdgeStorePlacement *edgePlacement);
   void recordPlacementForDefInBlock(TR_BlockStorePlacement *blockPlacement);
   bool isSafeToSinkThruEdgePlacement(int symIdx, TR::CFGNode *block, TR::CFGNode *succBlock, TR_BitVector *allEdgeInfoUsedOrKilledSymbols);
   bool isSymUsedInEdgePlacement(TR::CFGNode *block, TR::CFGNode *succBlock);

   uint32_t
   insertAnchoredNodes(TR_MovableStore *store,
                       TR::Node *nodeCopy,
                       TR::Node *nodeOrig,
                       TR::Node *nodeParentCopy,
                       int32_t childNum,
                       TR_BitVector *needTempForCommonedLoads,
                       TR_BitVector *blockingUsedSymbols,
                       TR_BitVector *blockingCommonedSymbols,
                       TR::Block *currentBlock,
                       List<TR_IndirectLoadAnchor> *indirectLoadAnchorsForThisStore,
                       vcount_t visitCount);

   int32_t genHandlerIndex() { return _handlerIndex++; }

   // The following hold the dataflow information needed to sink stores
   TR_LiveVariableInformation    * _liveVarInfo;
   TR_LiveOnAllPaths             * _liveOnAllPaths;
   TR_Liveness                   * _liveOnSomePaths;
   TR_LiveOnNotAllPaths          * _liveOnNotAllPaths;

   // Set of blocks that have some symbols that are live on not all paths at their exits
   TR_BitVector                  * _candidateBlocks;

   // One bit vector per block in the method describing which symbols are used or killed
   TR_BitVector                 ** _symbolsUsedInBlock;
   TR_BitVector                 ** _symbolsKilledInBlock;
//   TR_BitVector                 ** _symbolsExceptionUsedInBlock;
//   TR_BitVector                 ** _symbolsExceptionKilledInBlock;

   // These bit vectors hold the symbols defined and used by the candidate for sinking
   // Right now, there's only one tree (a store) represented in these, but in future the
   //   information for a set of trees could be put into these vectors (imagine moving an
   //   entire loop or an if-then-else statement).
   TR_BitVector                  * _usedSymbolsToMove;
   TR_BitVector                  * _killedSymbolsToMove;

   // These lists hold pending placements for sunk stores
   // They are used to detect safe movements after stores are sunk (because sunk stores need to stop other stores)
   TR_EdgeStorePlacementList    ** _placementsForEdgesToBlock;
   TR_BlockStorePlacementList   ** _placementsForBlock;

   // These hold all the store placements for blocks and edges
   TR_EdgeStorePlacementList       _allEdgePlacements;
   TR_BlockStorePlacementList      _allBlockPlacements;

   int32_t                          _handlerIndex;

   // stats tracking
   uint64_t                         _numRemovedStores;
   uint64_t                         _numPlacements;
   uint64_t                         _numTemps;
   uint64_t                         _searchMarkCalls;
   uint64_t                         _searchMarkWalks;
   uint64_t                         _killMarkWalks;
   uint64_t                         _numFirstUseAnchors;
   uint64_t                         _numIndirectLoadAnchors;
   uint64_t                         _numTransformations;

   // Data and routines needed to generate and place temp stores of killed locals
   TR_HashTab                      * _tempSymMap;
   TR::SymbolReference              * findTempSym(TR::Node *load);
   TR_FirstUseOfLoad               * findFirstUseOfLoad(TR::Node *load);
   void                            replaceLoadsWithTempSym(TR::Node *newNode, TR::Node *origNode, TR_BitVector *needTempForCommonedLoads);
   bool                            isCorrectCommonedLoad(TR::Node *commonedLoad, TR::Node *searchNode);
   void                            genStoreToTempSyms(TR::TreeTop *storeLocation,
                                                      TR::Node *node,
                                                      TR_BitVector *allCommonedLoads,
                                                      TR_BitVector *killedLiveCommonedLoads,
                                                      TR::Node *store,
                                                      List<TR_MovableStore> &potentiallyMovableStores);

   void searchAndMarkFirstUses(TR::Node *node,
                               TR::TreeTop *tt,
                               TR_MovableStore *movableStore,
                               TR::Block *currentBlock,
                               TR_BitVector *firstRefsOfCommonedSymbolsForThisStore);

   // Data needed to sink stores that have indirect loads underneath
   TR_HashTab                       *_indirectLoadAnchorMap;
   TR_HashTab                       *_firstUseOfLoadMap;
   ListHeadAndTail<TR_IndirectLoadAnchor>     *_indirectLoadAnchors;

   // Tuning parameters controlled by env vars
   bool                            _sinkAllStores;        // all sinkable stores will be moved regardless of condition
   bool                            _printSinkStoreStats;  // print number of stores removed / sunk / temp created
   bool                            _sinkThruException;    // sink stores thru the exception edges
   int32_t                         _firstSinkOptTransformationIndex;
   int32_t                         _lastSinkOptTransformationIndex;

   enum
      {
      UsesDataFlowAnalysis                     = 0x0001,
      SinkMethodMetaDataStores                 = 0x0002,
      SinkStoresWithIndirectLoads              = 0x0004,
      ExceptionFlagIsSticky                    = 0x0008,
      SinkStoresWithStaticLoads                = 0x0010,

      LastDummyEnum
      };
   bool                            performThisTransformation();
   flags16_t                       _storeSinkingFlags;
   };

class TR_GeneralSinkStores : public TR_SinkStores
   {
   public:
   // performs store sinking using data flow analysis
   // Uses the information computed by LiveOnAllPaths and LiveOnNotAllPaths
   // to sink stores onto paths where they are always used.  Stores (defs)
   // that are already live on all paths are not moved.
   //
   TR_GeneralSinkStores(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_GeneralSinkStores(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   virtual bool storeIsSinkingCandidate(TR::Block *block,
                                        TR::Node *node,
                                        int32_t symIdx,
                                        bool sinkIndirectLoads,
                                        uint32_t &indirectLoadCount,
                                        int32_t &depth,
                                        bool &isLoadStatic,
                                        vcount_t &treeVisitCount,
                                        vcount_t &highVisitCount);
   virtual bool sinkStorePlacement(TR_MovableStore *store, bool nextStoreWasMoved);
   };

class TR_TrivialSinkStores : public TR_SinkStores
   {
   public:
   // performs store sinking without using dataflow analysis by pushing stores as far
   // down fall through path as possible and replicating on side exits
   //
   TR_TrivialSinkStores(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_TrivialSinkStores(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   TR::TreeTop *genSideExitTree(TR::TreeTop *store, TR::Block *exitBlock, bool isFirstGen);
   virtual bool storeIsSinkingCandidate(TR::Block *block,
                                        TR::Node *node,
                                        int32_t symIdx,
                                        bool sinkIndirectLoads,
                                        uint32_t &indirectLoadCount,
                                        int32_t &depth,
                                        bool &isLoadStatic,
                                        vcount_t &treeVisitCount,
                                        vcount_t &highVisitCount);
   virtual bool storeCanMoveThroughBlock(TR_BitVector *blockKilledSet, TR_BitVector *blockUsedSet, int32_t symIdx);
   virtual bool sinkStorePlacement(TR_MovableStore *store, bool nextStoreWasMoved);
   bool passesAnchoringTest(TR_MovableStore *store, bool storeChildIsAnchored, bool nextStoreWasMoved);

   TR::TreeTop *duplicateTreeForSideExit(TR::TreeTop *tree);
   TR::Node    *duplicateNodeForSideExit(TR::Node *node);

   TR_HashTab * _nodesClonedForSideExitDuplication;
   };

// current call store with commoned load with be moved with temp because current
// store sinking code cannot push store with commoned load properly (problem with using _usedSymbolsToMove
// outside Ebb)
// TODO: make this an instance variable and not dep on ZEMUL flag to clean it up
#define MIN_TREE_DEPTH_TO_MOVE     3                      // if temp is used to sink the store, this is the min depth that can be benefit from sinking
// TODO: make this an instance variable and not dep on ZEMUL flag to clean it up
#define MAX_TREE_DEPTH_TO_DUP     12                      // set a max depth when is needed duplication so massive uncommoning does not occur in the duplicated tree
#define MAX_TREE_DEPTH_TO_MOVE    12                      // this is the max depth for movable store

#endif
