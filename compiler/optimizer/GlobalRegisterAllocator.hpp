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

#ifndef GLOBAL_REGISTER_ALLOCATOR_INCL
#define GLOBAL_REGISTER_ALLOCATOR_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "infra/BitVector.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/UseDefInfo.hpp"

namespace TR { class GlobalRegister; }
class TR_NodeMappings;
namespace TR { class RegisterCandidate; }
namespace TR { class RegisterCandidates; }
class TR_StructureSubGraphNode;
namespace TR { class Block; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;

struct StoresInBlockInfo : public TR_Link<StoresInBlockInfo>
   {
   TR_ALLOC(TR_Memory::GlobalRegisterAllocator)

   TR::GlobalRegister  *_gr;
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

   TR_SymRefCandidatePair(TR::SymbolReference *symRef, TR::RegisterCandidate *rc) : _symRef(symRef), _rc(rc) {}

   TR::SymbolReference *_symRef;
   TR::RegisterCandidate *_rc;
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

   typedef TR::typed_allocator<std::pair<uint32_t const, TR::RegisterCandidate*>, TR::Region&> SymRefCandidateMapAllocator;
   typedef std::less<uint32_t> SymRefCandidateMapComparator;
   typedef std::map<uint32_t, TR::RegisterCandidate*, SymRefCandidateMapComparator, SymRefCandidateMapAllocator> SymRefCandidateMap;

   virtual void splitLiveRanges();
   void splitLiveRanges(TR_StructureSubGraphNode *structureNode);
   virtual void replaceAutosUsedIn(TR::TreeTop *currentTree, TR::Node *node, TR::Node *parent, TR::Block * block, List<TR::Block> *blocksInLoop, List<TR::Block> *exitBlocks, vcount_t visitCount, int32_t executionFrequency, SymRefCandidateMap &registerCandidates, TR_SymRefCandidatePair **correspondingSymRefs, TR_BitVector *replacedAutosInCurrentLoop, TR_BitVector *autosThatCannotBereplacedInCurrentLoop, TR_StructureSubGraphNode *loop, TR::Block *loopInvariantBlock);
   void placeStoresInLoopExits(TR::Node *node, TR_StructureSubGraphNode *loop, List<TR::Block> *blocksInLoop, TR::SymbolReference *orig, TR::SymbolReference *replacement);
   TR_SymRefCandidatePair *splitAndFixPreHeader(TR::SymbolReference *symRef, TR_SymRefCandidatePair **correspondingSymRefs, TR::Block *loopInvariantBlock, TR::Node *node);
   void fixExitsAfterSplit(TR::SymbolReference *symRef, TR_SymRefCandidatePair *correspondingSymRefCandidate, TR_SymRefCandidatePair **correspondingSymRefs, TR::Block *loopInvariantBlock, List<TR::Block> *blocksInLoop, TR::Node *node, SymRefCandidateMap &registerCandidates, TR_StructureSubGraphNode *loop, TR_BitVector *replacedAutosInCurrentLoop, TR::SymbolReference *origSymRef);
   void appendStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node);
   void prependStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node);

   protected:
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

   typedef TR::typed_allocator<std::pair<uint32_t const, TR::RegisterCandidate*>, TR::Region&> SymRefCandidateMapAllocator;
   typedef std::less<uint32_t> SymRefCandidateMapComparator;
   typedef std::map<uint32_t, TR::RegisterCandidate*, SymRefCandidateMapComparator, SymRefCandidateMapAllocator> SymRefCandidateMap;

private:

   void                findIfThenRegisterCandidates();
   void                findLoopsAndCorrespondingAutos(TR_StructureSubGraphNode *, vcount_t, SymRefCandidateMap &);
   void                findLoopsAndAutosNoStructureInfo(vcount_t visitCount, TR::RegisterCandidate **registerCandidates);
   void                initializeControlFlowInfo();
   virtual void        markAutosUsedIn(TR::Node *, TR::Node *, TR::Node *, TR::Node **, TR::Block *, List<TR::Block> *, vcount_t, int32_t, SymRefCandidateMap &, TR_BitVector *, TR_BitVector *, bool);
   void                signExtendAllDefNodes(TR::Node *, List<TR::Node> *);
   void                findSymsUsedInIndirectAccesses(TR::Node *, TR_BitVector *, TR_BitVector *, bool);

   void                offerAllAutosAndRegisterParmAsCandidates(TR::Block **, int32_t, bool onlySelectedCandidates = false);
   void                offerAllFPAutosAndParmsAsCandidates(TR::Block **, int32_t);

   bool                allocateForSymRef(TR::SymbolReference *symRef);
   bool                allocateForType(TR::DataType dt);

   void                assignRegisters();
   void                assignRegisters(TR::Block *);
   virtual void        transformNode(TR::Node *, TR::Node *, int32_t, TR::TreeTop *, TR::Block * &, TR_Array<TR::GlobalRegister> &, TR_NodeMappings *);
   void                addGlRegDepToExit(TR_Array<TR::Node *> &, TR::Node *, TR_Array<TR::GlobalRegister> &, TR::Block *);
   virtual void        prepareForBlockExit(TR::TreeTop * &, TR::Node * &, TR::Block *, TR_Array<TR::GlobalRegister> &,
                                           TR::Block *, TR_Array<TR::Node *> &);
   bool                markCandidateForReloadInSuccessors(int32_t, TR::GlobalRegister *, TR::GlobalRegister *, TR::Block *, bool);
   void                reloadNonRegStarVariables(TR::TreeTop *, TR::Node *, TR::Block *, bool);
   virtual bool        registerIsLiveAcrossEdge(TR::TreeTop *, TR::Node *, TR::Block *, TR::GlobalRegister *, TR::Block * &, int32_t);
   TR::Block *          extendBlock(TR::Block *, TR::Block *);
   TR::Block *          createBlock(TR::Block *, TR::Block *);
   bool                tagCandidates(TR::Block *, bool);
   bool                tagCandidates(TR::Block *, uint32_t, bool);
   TR::Node *           createStoreToRegister();
   TR::Node *           createStoreFromRegister();
   TR::Node *           createLoadFromRegister();

   TR::Block           *getAppendBlock(TR::Block *);

   TR_ScratchList<TR_PairedSymbols> _pairedSymbols;

   TR_PairedSymbols   *findPairedSymbols(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2);
   TR_ScratchList<TR_PairedSymbols>   *getPairedSymbols() { return &_pairedSymbols; }

   bool isSplittingCopy(TR::Node *node);
   void restoreOriginalSymbol(TR::Node *node, vcount_t visitCount);

   void findLoopsAndCreateAutosForSignExt(TR_StructureSubGraphNode *structureNode, vcount_t visitCount);
   void createStoresForSignExt(TR::Node *node, TR::Node *parent, TR::Node *grandParent, TR::TreeTop *treetop, TR::Node **currentArrayAccess, TR::Block * block, List<TR::Block> *blocksInLoop, vcount_t visitCount, bool hasCatchBlock);

   void populateSymRefNodes(TR::Node *node, vcount_t visitCount);

   /*
   void splitLiveRanges();
   void splitLiveRanges(TR_StructureSubGraphNode *structureNode, vcount_t visitCount, TR::RegisterCandidate **registerCandidates);
   void replaceAutosUsedIn(TR::Node *node, TR::Node *parent, TR::Block * block, List<TR::Block> *blocksInLoop, vcount_t visitCount, int32_t executionFrequency, TR::RegisterCandidate **registerCandidates, TR_SymRefCandidatePair **correspondingSymRefs, TR_BitVector *replacedAutosInCurrentLoop, TR_StructureSubGraphNode *loop, TR::Block *loopInvariantBlock);
   void placeStoresInPreHeaderAndInLoopExits(TR::Node *node, TR_StructureSubGraphNode *loop, TR::Block *loopInvariantBlock, List<TR::Block> *blocksInLoop, TR::SymbolReference *orig, TR::SymbolReference *replacement);
   void appendStoreToBlock(TR::SymbolReference *storeSymRef, TR::SymbolReference *loadSymRef, TR::Block *block, TR::Node *node);
   */
protected:
   void                findLoopAutoRegisterCandidates();
   TR::Block *          createNewSuccessorBlock(TR::Block *, TR::Block *, TR::TreeTop *, TR::Node *, TR::RegisterCandidate * rc);
   void                appendGotoBlock(TR::Block *gotoBlock, TR::Block *curBlock);
   void                transformBlock(TR::TreeTop *);
   bool                isTypeAvailable(TR::SymbolReference *symref);
   bool                isSymRefAvailable(TR::SymbolReference *symRef);
   bool                isSymRefAvailable(TR::SymbolReference *symRef, List<TR::Block> *blocksInLoop);
   void                transformMultiWayBranch(TR::TreeTop *, TR::Node *, TR::Block *, TR_Array<TR::GlobalRegister> &, bool regStarTransformDone = false);
   void                transformBlockExit(TR::TreeTop *, TR::Node *, TR::Block *, TR_Array<TR::GlobalRegister> &, TR::Block *);
   void                addCandidateReloadsToEntry(TR::TreeTop *, TR_Array<TR::GlobalRegister> &, TR::Block *);
   void                addRegLoadsToEntry(TR::TreeTop *, TR_Array<TR::GlobalRegister> &, TR::Block *);
   void                addStoresForCatchBlockLoads(TR::TreeTop *appendPoint, TR_Array<TR::GlobalRegister> &extBlockRegisters, TR::Block *throwingBlock);
   bool                isNodeAvailable(TR::Node *node);
   TR::GlobalRegister * getGlobalRegister(TR::Symbol *, TR_Array<TR::GlobalRegister> &, TR::Block *);
   TR::GlobalRegister * getGlobalRegisterWithoutChangingCurrentCandidate(TR::Symbol *, TR_Array<TR::GlobalRegister> &, TR::Block *);
   StoresInBlockInfo * findRegInStoreInfo(TR::GlobalRegister *);
   int32_t             numberOfRegistersLiveOnEntry(TR_Array<TR::GlobalRegister> &, bool);
   TR_PairedSymbols   *findOrCreatePairedSymbols(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2);
   bool                isDependentStore(TR::Node *, const TR_UseDefInfo::BitVector &, TR::SymbolReference *, bool *);
   TR::TreeTop *        findPrevTreeTop(TR::TreeTop * &, TR::Node * &, TR::Block *, TR::Block *);

   vcount_t     _visitCount;
   int32_t      _firstGlobalRegisterNumber;
   int32_t      _lastGlobalRegisterNumber;
   TR::Block    *_appendBlock;
   SymRefCandidateMap *_registerCandidates;
   TR_ScratchList<TR::Block> _newBlocks;
   TR_BitVector *_temp, *_temp2;
   TR_BitVector *_candidatesNeedingSignExtension;
   TR_BitVector *_candidatesSignExtendedInThisLoop;
   TR_LinkHead<StoresInBlockInfo> _storesInBlockInfo;
   TR_BitVector *_signExtAdjustmentReqd;
   TR_BitVector *_signExtAdjustmentNotReqd;
   TR_BitVector *_signExtDifference;
   TR_BitVector *_valueModifiedSymRefs;
   TR::RegisterCandidates * _candidates;

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
