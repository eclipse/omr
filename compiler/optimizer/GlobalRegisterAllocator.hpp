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

#ifndef GLOBAL_REGISTER_ALLOCATOR_INCL
#define GLOBAL_REGISTER_ALLOCATOR_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "cs2/hashtab.h"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "infra/BitVector.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/UseDefInfo.hpp"

class TR_GlobalRegister;
class TR_NodeMappings;
class TR_RegisterCandidate;
class TR_RegisterCandidates;
class TR_StructureSubGraphNode;
namespace TR { class Block; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;

struct StoresInBlockInfo : public TR_Link<StoresInBlockInfo>
   {
   TR_ALLOC(TR_Memory::GlobalRegisterAllocator)

   TR_GlobalRegister  *_gr;
   TR::TreeTop *_lastStore;
   bool _origStoreExists;
   };

struct TR_PairedSymbols
   {
   TR_ALLOC(TR_Memory::GlobalRegisterAllocator)

   TR_PairedSymbols(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2) : _symRef1(symRef1), _symRef2(symRef2) {}

   TR::SymbolReference *_symRef1;
   TR::SymbolReference *_symRef2;
   };

struct TR_SymRefCandidatePair
   {
   TR_ALLOC(TR_Memory::GlobalRegisterAllocator)

   TR_SymRefCandidatePair(TR::SymbolReference *symRef, TR_RegisterCandidate *rc) : _symRef(symRef), _rc(rc) {}

   TR::SymbolReference *_symRef;
   TR_RegisterCandidate *_rc;
   };

struct TR_NodeTriple
   {
   TR_ALLOC(TR_Memory::GlobalRegisterAllocator)

   TR_NodeTriple(TR::Node *node1, TR::Node *node2, TR::Node *node3, int32_t childNum) : _node1(node1), _node2(node2), _node3(node3), _childNum(childNum) {}

   TR::Node *_node1;
   TR::Node *_node2;
   TR::Node *_node3;
   int32_t _childNum;
   };

class TR_LiveRangeSplitter : public TR::Optimization
   {
   public:
   TR_LiveRangeSplitter(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LiveRangeSplitter(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   typedef TR::typed_allocator<std::pair<uint32_t const, TR_RegisterCandidate*>, TR::Region&> SymRefCandidateMapAllocator;
   typedef std::less<uint32_t> SymRefCandidateMapComparator;
   typedef std::map<uint32_t, TR_RegisterCandidate*, SymRefCandidateMapComparator, SymRefCandidateMapAllocator> SymRefCandidateMap;

   void splitLiveRanges();
   void splitLiveRanges(TR_StructureSubGraphNode *structureNode);
   void replaceAutosUsedIn(TR::TreeTop *currentTree, TR::Node *node, TR::Node *parent, TR::Block * block, List<TR::Block> *blocksInLoop, List<TR::Block> *exitBlocks, vcount_t visitCount, int32_t executionFrequency, SymRefCandidateMap &registerCandidates, TR_SymRefCandidatePair **correspondingSymRefs, TR_BitVector *replacedAutosInCurrentLoop, TR_BitVector *autosThatCannotBereplacedInCurrentLoop, TR_StructureSubGraphNode *loop, TR::Block *loopInvariantBlock);
   void placeStoresInLoopExits(TR::Node *node, TR_StructureSubGraphNode *loop, List<TR::Block> *blocksInLoop, TR::SymbolReference *orig, TR::SymbolReference *replacement);
   TR_SymRefCandidatePair *splitAndFixPreHeader(TR::SymbolReference *symRef, TR_SymRefCandidatePair **correspondingSymRefs, TR::Block *loopInvariantBlock, TR::Node *node);
   void fixExitsAfterSplit(TR::SymbolReference *symRef, TR_SymRefCandidatePair *correspondingSymRefCandidate, TR_SymRefCandidatePair **correspondingSymRefs, TR::Block *loopInvariantBlock, List<TR::Block> *blocksInLoop, TR::Node *node, SymRefCandidateMap &registerCandidates, TR_StructureSubGraphNode *loop, TR_BitVector *replacedAutosInCurrentLoop, TR::SymbolReference *origSymRef);
   void appendStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node);
   void prependStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node);

   private:
   List<TR::Block> _splitBlocks;
   int32_t _numberOfGPRs;
   int32_t _numberOfFPRs;
   TR_BitVector *_storedSymRefs;
   bool _changedSomething;
   TR::SymbolReference **_origSymRefs;
   int32_t _origSymRefCount;
   };

struct TR_ReplaceNode
   {
   TR_ALLOC(TR_Memory::GlobalRegisterAllocator)
   TR_ReplaceNode() : _old(NULL), _new(NULL) {};
   TR_ReplaceNode(TR::Node *o, TR::Node *n) : _old(o), _new(n) {};
   TR::Node *_old;
   TR::Node *_new;
   };


class TR_GlobalRegisterAllocator : public TR::Optimization
   {
public:
   TR_GlobalRegisterAllocator(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_GlobalRegisterAllocator(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   void walkTreesAndCollectSymbolDataTypes();
   void visitNodeForDataType(TR::Node*);

   bool candidateCouldNeedSignExtension(int32_t symRefNum)
      {
      return (_candidatesNeedingSignExtension &&
              _candidatesNeedingSignExtension->get(symRefNum));
      }

   TR_BitVector *signExtensionRequired() { return _signExtAdjustmentReqd; }
   void setSignExtensionRequired(bool b, int32_t regNum)
      {
      if (b)
         _signExtAdjustmentReqd->set(regNum);
      else
         _signExtAdjustmentReqd->reset(regNum);
      }

   TR_BitVector *signExtensionNotRequired() { return _signExtAdjustmentNotReqd; }
   void setSignExtensionNotRequired(bool b, int32_t regNum)
      {
      if (b)
         _signExtAdjustmentNotReqd->set(regNum);
      else
         _signExtAdjustmentNotReqd->reset(regNum);
      }

   TR::Node *           resolveTypeMismatch(TR::DataType oldType, TR::Node *newNode);
   TR::Node *           resolveTypeMismatch(TR::Node *oldNode, TR::Node *newNode);
   TR::Node *           resolveTypeMismatch(TR::DataType inputOldType, TR::Node *oldNode, TR::Node *newNode);

private:
   typedef TR::typed_allocator<std::pair<uint32_t const, TR_RegisterCandidate*>, TR::Region&> SymRefCandidateMapAllocator;
   typedef std::less<uint32_t> SymRefCandidateMapComparator;
   typedef std::map<uint32_t, TR_RegisterCandidate*, SymRefCandidateMapComparator, SymRefCandidateMapAllocator> SymRefCandidateMap;

   void                findIfThenRegisterCandidates();
   void                findLoopAutoRegisterCandidates();
   void                findLoopsAndCorrespondingAutos(TR_StructureSubGraphNode *, vcount_t, SymRefCandidateMap &);
   void                findLoopsAndAutosNoStructureInfo(vcount_t visitCount, TR_RegisterCandidate **registerCandidates);
   void                initializeControlFlowInfo();
   void                markAutosUsedIn(TR::Node *, TR::Node *, TR::Node *, TR::Node **, TR::Block *, List<TR::Block> *, vcount_t, int32_t, SymRefCandidateMap &, TR_BitVector *, TR_BitVector *, bool);
   void                signExtendAllDefNodes(TR::Node *, List<TR::Node> *);
   void                findSymsUsedInIndirectAccesses(TR::Node *, TR_BitVector *, TR_BitVector *, bool);

   void                offerAllAutosAndRegisterParmAsCandidates(TR::Block **, int32_t, bool onlySelectedCandidates = false);
   void                offerAllFPAutosAndParmsAsCandidates(TR::Block **, int32_t);

   bool                isDependentStore(TR::Node *, const TR_UseDefInfo::BitVector &, TR::SymbolReference *, bool *);

   bool                allocateForSymRef(TR::SymbolReference *symRef);
   bool                isSymRefAvailable(TR::SymbolReference *symRef);
   bool                isSymRefAvailable(TR::SymbolReference *symRef, List<TR::Block> *blocksInLoop);
   bool                allocateForType(TR::DataType dt);
   bool                isNodeAvailable(TR::Node *node);
   bool                isTypeAvailable(TR::SymbolReference *symref);

   void                assignRegisters();
   void                assignRegisters(TR::Block *);
   void                transformBlock(TR::TreeTop *);
   void                transformNode(TR::Node *, TR::Node *, int32_t, TR::TreeTop *, TR::Block * &, TR_Array<TR_GlobalRegister> &, TR_NodeMappings *);
   TR_GlobalRegister * getGlobalRegister(TR::Symbol *, TR_Array<TR_GlobalRegister> &, TR::Block *);
   TR_GlobalRegister * getGlobalRegisterWithoutChangingCurrentCandidate(TR::Symbol *, TR_Array<TR_GlobalRegister> &, TR::Block *);
   void                transformBlockExit(TR::TreeTop *, TR::Node *, TR::Block *, TR_Array<TR_GlobalRegister> &, TR::Block *);
   void                transformMultiWayBranch(TR::TreeTop *, TR::Node *, TR::Block *, TR_Array<TR_GlobalRegister> &, bool regStarTransformDone = false);
   void                addCandidateReloadsToEntry(TR::TreeTop *, TR_Array<TR_GlobalRegister> &, TR::Block *);
   void                addRegLoadsToEntry(TR::TreeTop *, TR_Array<TR_GlobalRegister> &, TR::Block *);
   void                addGlRegDepToExit(TR_Array<TR::Node *> &, TR::Node *, TR_Array<TR_GlobalRegister> &, TR::Block *);
   void                addStoresForCatchBlockLoads(TR::TreeTop *appendPoint, TR_Array<TR_GlobalRegister> &extBlockRegisters, TR::Block *throwingBlock);
   TR::TreeTop *        findPrevTreeTop(TR::TreeTop * &, TR::Node * &, TR::Block *, TR::Block *);
   void                prepareForBlockExit(TR::TreeTop * &, TR::Node * &, TR::Block *, TR_Array<TR_GlobalRegister> &,
                                           TR::Block *, TR_Array<TR::Node *> &);
   bool                markCandidateForReloadInSuccessors(int32_t, TR_GlobalRegister *, TR_GlobalRegister *, TR::Block *, bool);
   void                reloadNonRegStarVariables(TR::TreeTop *, TR::Node *, TR::Block *, bool);

   bool                registerIsLiveAcrossEdge(TR::TreeTop *, TR::Node *, TR::Block *, TR_GlobalRegister *, TR::Block * &, int32_t);
   TR::Block *          createNewSuccessorBlock(TR::Block *, TR::Block *, TR::TreeTop *, TR::Node *, TR_RegisterCandidate * rc);
   TR::Block *          extendBlock(TR::Block *, TR::Block *);
   TR::Block *          createBlock(TR::Block *, TR::Block *);
   bool                tagCandidates(TR::Block *, bool);
   bool                tagCandidates(TR::Block *, uint32_t, bool);
   int32_t             numberOfRegistersLiveOnEntry(TR_Array<TR_GlobalRegister> &, bool);
   TR::Node *           createStoreToRegister();
   TR::Node *           createStoreFromRegister();
   TR::Node *           createLoadFromRegister();

   void                appendGotoBlock(TR::Block *gotoBlock, TR::Block *curBlock);
   TR::Block           *getAppendBlock(TR::Block *);

   CS2::HashTable<int32_t, TR::SymbolReference *, TR::Allocator> _defIndexToTempMap;

   TR_ScratchList<TR_PairedSymbols> _pairedSymbols;

   TR::SparseBitVector _allocatedPerformCellSymRefs;
   int32_t _performCellCount;

   TR_PairedSymbols   *findOrCreatePairedSymbols(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2);
   TR_PairedSymbols   *findPairedSymbols(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2);
   TR_ScratchList<TR_PairedSymbols>   *getPairedSymbols() { return &_pairedSymbols; }

   StoresInBlockInfo * findRegInStoreInfo(TR_GlobalRegister *);

   bool isSplittingCopy(TR::Node *node);
   void restoreOriginalSymbol(TR::Node *node, vcount_t visitCount);

   void findLoopsAndCreateAutosForSignExt(TR_StructureSubGraphNode *structureNode, vcount_t visitCount);
   void createStoresForSignExt(TR::Node *node, TR::Node *parent, TR::Node *grandParent, TR::TreeTop *treetop, TR::Node **currentArrayAccess, TR::Block * block, List<TR::Block> *blocksInLoop, vcount_t visitCount, bool hasCatchBlock);

   void populateSymRefNodes(TR::Node *node, vcount_t visitCount);

   /*
   void splitLiveRanges();
   void splitLiveRanges(TR_StructureSubGraphNode *structureNode, vcount_t visitCount, TR_RegisterCandidate **registerCandidates);
   void replaceAutosUsedIn(TR::Node *node, TR::Node *parent, TR::Block * block, List<TR::Block> *blocksInLoop, vcount_t visitCount, int32_t executionFrequency, TR_RegisterCandidate **registerCandidates, TR_SymRefCandidatePair **correspondingSymRefs, TR_BitVector *replacedAutosInCurrentLoop, TR_StructureSubGraphNode *loop, TR::Block *loopInvariantBlock);
   void placeStoresInPreHeaderAndInLoopExits(TR::Node *node, TR_StructureSubGraphNode *loop, TR::Block *loopInvariantBlock, List<TR::Block> *blocksInLoop, TR::SymbolReference *orig, TR::SymbolReference *replacement);
   void appendStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node);
   */

   vcount_t     _visitCount;
   int32_t      _firstGlobalRegisterNumber;
   int32_t      _lastGlobalRegisterNumber;
   bool         _gotoCreated;
   TR::Block    *_appendBlock;
   SymRefCandidateMap *_registerCandidates;
   TR_ScratchList<TR::Block> _newBlocks;
   TR_BitVector *_temp, *_temp2;
   TR_BitVector *_candidatesNeedingSignExtension;
   TR_BitVector *_candidatesSignExtendedInThisLoop;
   List<TR::Node> _nodesToBeChecked;
   List<TR_NodeTriple> _nodeTriplesToBeChecked;
   List<TR::Node> _rejectedSignExtNodes;
   TR::SymbolReference *_storeSymRef;
   TR_LinkHead<StoresInBlockInfo> _storesInBlockInfo;
   TR_BitVector *_signExtAdjustmentReqd;
   TR_BitVector *_signExtAdjustmentNotReqd;
   TR_BitVector *_signExtDifference;
   TR_BitVector *_valueModifiedSymRefs;
   TR_RegisterCandidates * _candidates;
   TR_BitVector *_seenInternalMethods;

   TR::Node **_nodesForSymRefs;

   struct BlockInfo
      {
      TR_ALLOC(TR_Memory::GlobalRegisterAllocator)

      BlockInfo() : _alwaysReached(false) { }

      bool           _inALoop;
      bool           _alwaysReached;
      };

   BlockInfo & blockInfo(int32_t);
   BlockInfo *               _blockInfo;
   int32_t                   _origSymRefCount;
   TR::Block                  *_osrCatchSucc;
   };
#endif
