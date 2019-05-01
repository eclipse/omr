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

#ifndef OMR_RESOLVEDMETHODSYMBOL_INCL
#define OMR_RESOLVEDMETHODSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_RESOLVEDMETHODSYMBOL_CONNECTOR
#define OMR_RESOLVEDMETHODSYMBOL_CONNECTOR
namespace OMR { class ResolvedMethodSymbol; }
namespace OMR { typedef OMR::ResolvedMethodSymbol ResolvedMethodSymbolConnector; }
#endif

#include "il/symbol/MethodSymbol.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "il/Node.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "infra/TRlist.hpp"

class TR_BitVector;
class TR_ExtraLinkageInfo;
class TR_FrontEnd;
class TR_InlineBlocks;
class TR_Memory;
class TR_OSRMethodData;
class TR_OSRPoint;
namespace TR { class AutomaticSymbol; }
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class Compilation; }
namespace TR { class IlGenRequest; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }

namespace OMR
{

/**
 * Class for resolved method symbols
 */
class OMR_EXTENSIBLE ResolvedMethodSymbol : public TR::MethodSymbol
   {

protected:

   ResolvedMethodSymbol(TR_ResolvedMethod *, TR::Compilation *);

public:

   TR::ResolvedMethodSymbol * self();

   template <typename AllocatorType>
   static TR::ResolvedMethodSymbol * create(AllocatorType, TR_ResolvedMethod *, TR::Compilation *);

   void initForCompilation(TR::Compilation *);

   void setParameterList()                                                    { _resolvedMethod->makeParameterList(self()); }
   List<TR::ParameterSymbol>& getParameterList()                               { return _parameterList; }

   /// With zOS type 1 linkage, the parameters in the parameter list
   /// don't look at all like the source level (logical) parameters.
   List<TR::ParameterSymbol>& getLogicalParameterList(TR::Compilation *comp);
   ListBase<TR::AutomaticSymbol>& getAutomaticList()                           { return _automaticList;}
   void setAutomaticList(List<TR::AutomaticSymbol> list)                       { _automaticList = list; };

   List<TR::Block>& getTrivialDeadTreeBlocksList()                             { return _trivialDeadTreeBlocksList;}
   List<TR::AutomaticSymbol>& getVariableSizeSymbolList()                      { return _variableSizeSymbolList;}
   ListBase<TR::RegisterMappedSymbol>& getMethodMetaDataList()                 { return _methodMetaDataList;}

   void removeUnusedLocals();

   ncount_t generateAccurateNodeCount();
   ncount_t recursivelyCountChildren(TR::Node *node);

   List<TR::SymbolReference> & getAutoSymRefs(int32_t slot);
   TR_Array<List<TR::SymbolReference> > *getAutoSymRefs() {return _autoSymRefs;}
   void setAutoSymRefs(TR_Array<List<TR::SymbolReference> > *l) {_autoSymRefs = l;}

   TR::SymbolReference *getParmSymRef(int32_t slot);
   void setParmSymRef(int32_t slot, TR::SymbolReference *symRef);

   List<TR::SymbolReference> & getPendingPushSymRefs(int32_t slot);
   TR_Array<List<TR::SymbolReference> > *getPendingPushSymRefs() {return _pendingPushSymRefs;}
   void setPendingPushSymRefs(TR_Array<List<TR::SymbolReference> > *l) {_pendingPushSymRefs = l;}

   TR_Array<TR_OSRPoint *> &getOSRPoints() { return _osrPoints; }
   TR_OSRPoint *findOSRPoint(TR_ByteCodeInfo &bcInfo);
   uint32_t addOSRPoint(TR_OSRPoint *point) { return _osrPoints.add(point) ; }
   uint32_t getNumOSRPoints() { return _osrPoints.size(); }
   bool sharesStackSlot(TR::SymbolReference *symRef);

   TR_ByteCodeInfo& getOSRByteCodeInfo(TR::Node *node);
   bool isOSRRelatedNode(TR::Node *node);
   bool isOSRRelatedNode(TR::Node *node, TR_ByteCodeInfo &bci);
   TR::TreeTop *getOSRTransitionTreeTop(TR::TreeTop *tt);

   uint32_t getNumParameterSlots() const     {return _resolvedMethod->numberOfParameterSlots(); }

   uint32_t getNumPPSlots() const            { return _resolvedMethod->numberOfPendingPushes(); }

   void addAutomatic(TR::AutomaticSymbol *p);

   void addTrivialDeadTreeBlock(TR::Block *b);
   void addVariableSizeSymbol(TR::AutomaticSymbol *s);
   void addMethodMetaDataSymbol(TR::RegisterMappedSymbol*s);

   mcount_t            getResolvedMethodIndex() { return _methodIndex; }
   TR_ResolvedMethod * getResolvedMethod()      { return _resolvedMethod; }

   void setResolvedMethodIndex(mcount_t index)  { _methodIndex = index; }
   void setResolvedMethod(TR_ResolvedMethod *m) {_resolvedMethod = m;}

   const char * signature(TR_Memory * m) { return _resolvedMethod->signature(m); }

   bool genIL(TR_FrontEnd *fe, TR::Compilation *comp, TR::SymbolReferenceTable *symRefTab, TR::IlGenRequest & customRequest);
   void cleanupUnreachableOSRBlocks(int32_t inlinedSiteIndex, TR::Compilation *comp);
   void insertRematableStoresFromCallSites(TR::Compilation *comp, int32_t siteIndex, TR::TreeTop *induceOSRTree);
   void insertStoresForDeadStackSlotsBeforeInducingOSR(TR::Compilation *comp, int32_t inlinedSiteIndex, TR_ByteCodeInfo &byteCodeInfo, TR::TreeTop *induceOSRTree);
   void insertStoresForDeadStackSlots(TR::Compilation *comp, TR_ByteCodeInfo &byteCodeInfo, TR::TreeTop *insertTree, bool keepStashedArgsLive=true);
   bool sharesStackSlots(TR::Compilation *comp);
   void resetLiveLocalIndices();

   bool genPartialIL(TR_FrontEnd *, TR::Compilation *, TR::SymbolReferenceTable *, bool forceClassLookahead = false, TR_InlineBlocks *blocksToInline = 0);

   void genOSRHelperCall(int32_t inlinedSiteIndex, TR::SymbolReferenceTable* symRefTab, TR::CFG *cfg = NULL);
   void genAndAttachOSRCodeBlocks(int32_t inlinedSiteIndex) ;
   int  matchInduceOSRCall(TR::TreeTop* insertionPoint,
                               int16_t callerIndex,
                               int16_t byteCodeIndex,
                               const char* childPath);

   TR::TreeTop *genInduceOSRCallNode(TR::TreeTop* insertionPoint, int32_t numChildren, bool copyChildren, bool shouldSplitBlock = true, TR::CFG *cfg = NULL);
   TR::TreeTop *genInduceOSRCall(TR::TreeTop* insertionPoint, int32_t inlinedSiteIndex, int32_t numChildren, bool copyChildren, bool shouldSplitBlock, TR::CFG *cfg = NULL);
   TR::TreeTop *genInduceOSRCall(TR::TreeTop* insertionPoint, int32_t inlinedSiteIndex, TR_OSRMethodData *osrMethodData, int32_t numChildren, bool copyChildren, bool shouldSplitBlock, TR::CFG *cfg = NULL);
   TR::TreeTop *genInduceOSRCallAndCleanUpFollowingTreesImmediately(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, bool shouldSplitBlock, TR::Compilation *comp);

   bool canInjectInduceOSR(TR::Node* node);

   bool induceOSRAfter(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, TR::TreeTop* branch, bool extendRemainder, int32_t offset, TR::TreeTop ** lastTreeTop = NULL);
   bool induceOSRAfterAndRecompile(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, TR::TreeTop* branch, bool extendRemainder, int32_t offset, TR::TreeTop ** lastTreeTop);
   TR::TreeTop *induceImmediateOSRWithoutChecksBefore(TR::TreeTop *insertionPoint);

   int32_t incTempIndex(TR_FrontEnd * fe);

   void setFirstJitTempIndex(int32_t index) { _firstJitTempIndex = index; }
   int32_t getFirstJitTempIndex() { TR_ASSERT(_tempIndex >= 0, "assertion failure"); return _firstJitTempIndex; }

   TR::SymbolReference * getSyncObjectTemp()       { return _syncObjectTemp; }
   void setSyncObjectTemp(TR::SymbolReference * t) { _syncObjectTemp = t; }
   int32_t getSyncObjectTempIndex();

   TR::SymbolReference *getThisTempForObjectCtor()        { return _thisTempForObjectCtor; }
   void setThisTempForObjectCtor(TR::SymbolReference *t)  { _thisTempForObjectCtor = t; }
   int32_t getThisTempForObjectCtorIndex();

   uint8_t unimplementedOpcode() { return _unimplementedOpcode; }
   void setUnimplementedOpcode(uint8_t o) { _unimplementedOpcode = o; }

   TR::CFG * getFlowGraph() { return _flowGraph; }
   void setFlowGraph(TR::CFG * cfg) { _flowGraph = cfg; }

   TR::Compilation * comp() { return _comp; }
   void setComp(TR::Compilation *comp) { _comp = comp; }

   TR::TreeTop * getFirstTreeTop()        { return _firstTreeTop; }

   /**
    * Walks the blocks to find the last treeTop.  It will start at the block
    * provided, if not block argument is supplied it will start at the first
    * block.
    */
   TR::TreeTop * getLastTreeTop(TR::Block * = 0);

   /** Create a dummy block and insert it as the first block in the tree and cfg. */
   TR::Block * prependEmptyFirstBlock();

   void setFirstTreeTop(TR::TreeTop *);
   void removeTree(TR::TreeTop *tt);

   bool mayHaveLongOps()                     {return _methodFlags.testAny(MayHaveLongOps); }
   void setMayHaveLongOps(bool b)            { _methodFlags.set(MayHaveLongOps, b); }

   bool mayHaveLoops()                       {return _methodFlags.testAny(MayHaveLoops); }
   void setMayHaveLoops(bool b)              { _methodFlags.set(MayHaveLoops, b); }

   bool mayHaveNestedLoops()                 {return _methodFlags.testAny(MayHaveLoops); }
   void setMayHaveNestedLoops(bool b)        { _methodFlags.set(MayHaveLoops, b); }

   bool mayHaveInlineableCall()              {return _methodFlags.testAny(MayHaveInlineableCall); }
   void setMayHaveInlineableCall(bool b)     { _methodFlags.set(MayHaveInlineableCall, b); }

   bool mayContainMonitors()                 {return _methodFlags.testAny(MayContainMonitors); }
   void setMayContainMonitors(bool b)        { _methodFlags.set(MayContainMonitors, b); }

   bool hasNews()                            {return _methodFlags.testAny(HasNews); }
   void setHasNews(bool b)                   { _methodFlags.set(HasNews, b); }

   bool hasDememoizationOpportunities()      {return _methodFlags2.testAny(HasDememoizationOpportunities); }
   void setHasDememoizationOpportunities(bool b) { _methodFlags2.set(HasDememoizationOpportunities, b); }

   bool hasEscapeAnalysisOpportunities();

   bool mayHaveIndirectCalls()               { return _methodFlags.testAny(MayHaveIndirectCalls); }
   void setMayHaveIndirectCalls(bool b)      { _methodFlags.set(MayHaveIndirectCalls, b); }

   bool hasThisCalls()                       { return _methodFlags.testAny(HasThisCalls);}
   void setHasThisCalls(bool b)              { _methodFlags.set(HasThisCalls, b); }

   bool hasMethodHandleInvokes()             {return _methodFlags2.testAny(HasMethodHandleInvokes); }
   void setHasMethodHandleInvokes(bool b)    { _methodFlags2.set(HasMethodHandleInvokes, b); }
   bool doJSR292PerfTweaks();

   bool hasCheckCasts()                      { return _methodFlags2.testAny(HasCheckCasts); }
   void setHasCheckCasts(bool b)             { _methodFlags2.set(HasCheckCasts, b); }
   bool hasInstanceOfs()                     { return _methodFlags2.testAny(HasInstanceOfs); }
   void setHasInstanceOfs(bool b)            { _methodFlags2.set(HasInstanceOfs, b); }
   bool hasCheckcastsOrInstanceOfs()         { return _methodFlags2.testAny(HasCheckCasts|HasInstanceOfs); }
   bool hasBranches()                        { return _methodFlags2.testAny(HasBranches); }
   void setHasBranches(bool b)               { _methodFlags2.set(HasBranches, b); }

   int32_t getNumberOfBackEdges();

   bool canDirectNativeCall()                { return _properties.testAny(CanDirectNativeCall); }
   void setCanDirectNativeCall(bool b)       { _properties.set(CanDirectNativeCall,b); }

   bool canReplaceWithHWInstr()              { return _properties.testAny(CanReplaceWithHWInstr); }
   void setCanReplaceWithHWInstr(bool b)     { _properties.set(CanReplaceWithHWInstr,b); }

   bool skipNullChecks()                     { return _properties.testAny(CanSkipNullChecks); }
   bool skipBoundChecks()                    { return _properties.testAny(CanSkipBoundChecks); }
   bool skipCheckCasts()                     { return _properties.testAny(CanSkipCheckCasts); }
   bool skipDivChecks()                      { return _properties.testAny(CanSkipDivChecks); }
   bool skipArrayStoreChecks()               { return _properties.testAny(CanSkipArrayStoreChecks); }
   bool skipChecksOnArrayCopies()            { return _properties.testAny(CanSkipChecksOnArrayCopies); }
   bool skipZeroInitializationOnNewarrays()  { return _properties.testAny(CanSkipZeroInitializationOnNewarrays); }

   bool hasSnapshots()                       { return _properties.testAny(HasSnapshots); }
   void setHasSnapshots(bool v=true)         { _properties.set(HasSnapshots,v); }

   bool hasUnkilledTemps()                   { return _properties.testAny(HasUnkilledTemps); }
   void setHasUnkilledTemps(bool v=true)     { _properties.set(HasUnkilledTemps,v); }

   bool detectInternalCycles(TR::CFG *cfg, TR::Compilation *comp);
   bool catchBlocksHaveRealPredecessors(TR::CFG *cfg, TR::Compilation *comp);

   void setCannotAttemptOSR(int32_t n);
   bool cannotAttemptOSRAt(TR_ByteCodeInfo &bci, TR::Block *blockToOSRAt, TR::Compilation *comp);
   bool cannotAttemptOSRDuring(int32_t callSite, TR::Compilation *comp, bool runCleanup = true);
   bool supportsInduceOSR(TR_ByteCodeInfo &bci, TR::Block *blockToOSRAt, TR::Compilation *comp, bool runCleanup = true);
   bool hasOSRProhibitions();

   typedef enum
      {
      kDefaultInlinePriority=0,
      kHighInlinePriority,
      kLowInlinePriority
      } InlinePriority;

   bool isSideEffectFree()                { return _properties.testAny( IsSideEffectFree); }
   void setIsSideEffectFree(bool b)       { _properties.set(IsSideEffectFree,b); }
   int32_t uncheckedSetTempIndex(int32_t index) { return _tempIndex = index; }

   int32_t setTempIndex(int32_t index, TR_FrontEnd * fe);

   int32_t getTempIndex() { return _tempIndex; }
   int32_t getArrayCopyTempSlot(TR_FrontEnd * fe);

   int32_t getProfilingByteCodeIndex(int32_t bytecodeIndex);
   void addProfilingOffsetInfo(int32_t startBCI, int32_t endBCI);
   void setProfilerFrequency(int32_t bytecodeIndex, int32_t frequency);
   int32_t getProfilerFrequency(int32_t bytecodeIndex);
   void clearProfilingOffsetInfo();
   void dumpProfilingOffsetInfo(TR::Compilation *comp);

protected:
   enum Properties
      {
      CanSkipNullChecks                         = 1 << 0,
      CanSkipBoundChecks                        = 1 << 1,
      CanSkipCheckCasts                         = 1 << 2,
      CanSkipDivChecks                          = 1 << 3,
      CanSkipChecksOnArrayCopies                = 1 << 4,
      CanSkipZeroInitializationOnNewarrays      = 1 << 5,
      CanSkipArrayStoreChecks                   = 1 << 6,
      HasSnapshots                              = 1 << 7,
      HasUnkilledTemps                          = 1 << 8,
      CanDirectNativeCall                       = 1 << 9,
      CanReplaceWithHWInstr                     = 1 << 10,
      IsSideEffectFree                          = 1 << 12,
      IsLeaf                                    = 1 << 13,
      FoundThrow                                = 1 << 14,
      HasExceptionHandlers                      = 1 << 15,
      MayHaveVirtualCallProfileInfo             = 1 << 16,
      AggressivelyInlineThrows                  = 1 << 18,
      LastProperty = AggressivelyInlineThrows,
      };

   flags32_t _properties;

private:

   TR::Compilation *                        _comp;
   TR_ResolvedMethod *                       _resolvedMethod;
   List<TR::AutomaticSymbol>                  _automaticList;
   List<TR::ParameterSymbol>                  _parameterList;
   List<TR::Block>                            _trivialDeadTreeBlocksList;
   List<TR::AutomaticSymbol>                  _variableSizeSymbolList;
   List<TR::RegisterMappedSymbol>             _methodMetaDataList;
   TR_Array<List<TR::SymbolReference> >     * _autoSymRefs;
   TR_Array<List<TR::SymbolReference> >     * _pendingPushSymRefs;
   TR_Array<TR::SymbolReference*>           * _parmSymRefs;

   TR_Array<TR_OSRPoint *>                   _osrPoints;

   // most of the following are only used if the genIL is called for the method.
   // todo: implement a way to avoid the memory usage for when genIL is not called
   //
   TR::CFG *                                _flowGraph;
   TR::TreeTop *                            _firstTreeTop;
   TR::SymbolReference *                    _syncObjectTemp;
   TR::SymbolReference *                    _thisTempForObjectCtor;
   TR::SymbolReference *                    _atcDeferredCountTemp;
   int32_t                                   _tempIndex;
   int32_t                                   _arrayCopyTempSlot;
   int32_t                                   _firstJitTempIndex;
   mcount_t                                  _methodIndex;

   TR_BitVector                              *_cannotAttemptOSR;
   uint8_t                                   _unimplementedOpcode;

   TR::list< std::pair<int32_t, std::pair<int32_t, int32_t> > > _bytecodeProfilingOffsets;

   //used if estimateCodeSize is called
   enum
      {
      bbStart        = 0x01,
      isCold         = 0x02,
      inlineableCall = 0x04,
      isBranch       = 0x08
      };

   int32_t  _throwCount;

   TR::TreeTop *induceOSRAfterImpl(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, TR::TreeTop* branch, bool extendRemainder, int32_t offset, TR::TreeTop ** lastTreeTop = NULL);

/*-- TR_JittedMethodSymbol --------------------------------*/
public:

   /**
    * Jitted method symbol facotry
    */
   template <typename AllocatorType>
   static TR::ResolvedMethodSymbol * createJittedMethodSymbol(AllocatorType m, TR_ResolvedMethod *, TR::Compilation *);

   uint32_t&   getLocalMappingCursor();
   void        setLocalMappingCursor(uint32_t i);

   uint32_t    getProloguePushSlots();
   uint32_t    setProloguePushSlots(uint32_t s);

   uint32_t    getScalarTempSlots();
   uint32_t    setScalarTempSlots(uint32_t s);

   uint32_t    getObjectTempSlots();
   uint32_t    setObjectTempSlots(uint32_t s);

   bool        containsOnlySinglePrecision();
   void        setContainsOnlySinglePrecision(bool b);

   bool        usesSinglePrecisionMode();
   void        setUsesSinglePrecisionMode(bool b);

   bool        isNoTemps();
   void        setNoTemps(bool b=true);

private:
   uint32_t                                  _localMappingCursor;
   uint32_t                                  _prologuePushSlots;
   uint32_t                                  _scalarTempSlots;
   uint32_t                                  _objectTempSlots;

   };
}
#endif
