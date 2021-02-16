/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#ifndef USEDEFINFO_INCL
#define USEDEFINFO_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/deque.hpp"
#include "infra/vector.hpp"
#include "infra/TRlist.hpp"

class TR_ReachingDefinitions;
class TR_ValueNumberInfo;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class ILOpCode; }
namespace TR { class Optimizer; }
namespace TR { class TreeTop; }
namespace OMR { class Optimizer; }

/**
 * Use/def information.
 *
 * Each tree node that can use or define a value is assigned an index value.
 * The initial index numbers are reserved for the initial definitions of
 * parameters and fields.
 *
 * The index values are partitioned into 3 parts:
 *    0-X     = nodes that can define values
 *    (X+1)-Y = nodes that can both define and use values
 *    (Y+1)-Z = nodes that can use value
 *
 * This means that there are a total of Z index values.
 * The bit vectors that hold use information are of size (Z-X).
 */
class TR_UseDefInfo
   {
   TR::Region _region;
   public:

   static void *operator new(size_t size, TR::Allocator a)
      { return a.allocate(size); }
   static void  operator delete(void *ptr, TR::Allocator a)
      {
      // If there is an exception thrown during construction, the compilation
      // will be aborted, and all memory associated with that compilation will get freed.
      }
   static void  operator delete(void *ptr, size_t size)
      { ((TR_UseDefInfo*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() better return the same allocator as used for new */

   /* Virtual destructor is necessary for the above delete operator to work
    * See "Modern C++ Design" section 4.7
    */
   virtual ~TR_UseDefInfo() {}

   // Construct use def info for the current method's trees. This also assigns
   // use/def index values to the relevant nodes.
   //
#if defined(NEW_USEDEF_INFO_SPARSE_INTERFACE)
   typedef TR::SparseBitVector BitVector;
#else
   typedef TR::BitVector BitVector;
#endif

   /**
    * Data that can be freed when TR_UseDefInfo is no longer needed
    * Data that used to be stack allocated
    */
   class AuxiliaryData
      {
      private:
         AuxiliaryData(int32_t numSymRefs, ncount_t nodeCount, TR::Region &region, TR::Allocator allocator) :
             _region(region),
             _onceReadSymbols(numSymRefs, static_cast<TR_BitVector*>(NULL), _region),
             _onceWrittenSymbols(numSymRefs, static_cast<TR_BitVector*>(NULL), _region),
             _defsForSymbol(0, static_cast<TR_BitVector*>(NULL), _region),
             _neverReadSymbols(numSymRefs, _region),
             _neverReferencedSymbols(numSymRefs, _region),
             _neverWrittenSymbols(numSymRefs, _region),
             _volatileOrAliasedToVolatileSymbols(numSymRefs, _region),
             _onceWrittenSymbolsIndices(numSymRefs, TR::SparseBitVector(allocator), _region),
             _onceReadSymbolsIndices(numSymRefs, TR::SparseBitVector(allocator), _region),
             _expandedAtoms(0, std::make_pair<TR::Node *, TR::TreeTop *>(NULL, NULL), _region),
             _sideTableToUseDefMap(_region),
             _numAliases(numSymRefs, _region),
             _loadsBySymRefNum(numSymRefs, _region),
             _defsForOSR(0, static_cast<TR_BitVector*>(NULL), _region),
             _workBitVector(_region),
             _doneTrivialNode(_region),
             _isTrivialNode(_region)
            {}
      TR::Region _region;

      // BitVector for temporary work. Used in buildUseDefs.
      TR_BitVector _workBitVector;

      // Cache results from isTrivialNode
      TR_BitVector _doneTrivialNode;
      TR_BitVector _isTrivialNode;

      TR::vector<TR_BitVector *, TR::Region&> _onceReadSymbols;
      TR::vector<TR_BitVector *, TR::Region&> _onceWrittenSymbols;
      // defsForSymbol are known definitions of the symbol
      TR::vector<TR_BitVector *, TR::Region&> _defsForSymbol;
      TR_BitVector _neverReadSymbols;
      TR_BitVector _neverReferencedSymbols;
      TR_BitVector _neverWrittenSymbols;
      TR_BitVector _volatileOrAliasedToVolatileSymbols;
      TR::vector<TR::SparseBitVector, TR::Region&> _onceWrittenSymbolsIndices;
      TR::vector<TR::SparseBitVector, TR::Region&> _onceReadSymbolsIndices;

      TR::vector<std::pair<TR::Node *, TR::TreeTop *>, TR::Region&> _expandedAtoms;


      protected:
      TR::deque<uint32_t, TR::Region&> _sideTableToUseDefMap;
      private:
      TR::deque<uint32_t, TR::Region&> _numAliases;
      TR::deque<TR::Node *, TR::Region&> _loadsBySymRefNum;

      protected:
      // used only in TR_OSRDefInfo - should extend AuxiliaryData really:
      TR::vector<TR_BitVector *, TR::Region&> _defsForOSR;

      friend class TR_UseDefInfo;
      friend class TR_ReachingDefinitions;
      friend class TR_OSRDefInfo;
      };

   friend class OMR::Optimizer;

   protected:
   /**
    * Constructs TR_UseDefInfo instance. Note that this should not be called directly.
    * Instead construction is handled by OMR::Optimizer::createUseDefInfo() method.
    *
    * @param cfg                        The compilation instance
    * @param requiresGlobals
    * @param prefersGlobals
    * @param loadsShouldBeDefs
    * @param cannotOmitTrivialDefs
    * @param conversionRegsOnly
    * @param doCompletion
    * @param callsShouldBeUses          Enables inclusion of calls as uses so that the alias analysis can detect
    *                                   when local (stack) variable has been aliased by a function call.
    *                                   A value of false is fine for Java like languages where
    *                                   local (stack) variables cannot be passed by reference to function calls
    *                                   and hence cannot be aliased. However for C like languages this flag should be
    *                                   set to true.
    */
   TR_UseDefInfo(TR::Compilation *, TR::CFG * cfg, TR::Optimizer *,
         bool requiresGlobals = true, bool prefersGlobals = true, bool loadsShouldBeDefs = true, bool cannotOmitTrivialDefs = false,
         bool conversionRegsOnly = false, bool doCompletion = true, bool callsShouldBeUses = false);

   void prepareUseDefInfo(bool requiresGlobals, bool prefersGlobals, bool cannotOmitTrivialDefs, bool conversionRegsOnly);
   void invalidateUseDefInfo();
   virtual bool performAnalysis(AuxiliaryData &aux);
   virtual void processReachingDefinition(void* vblockInfo, AuxiliaryData &aux);
   private:
   bool _runReachingDefinitions(TR_ReachingDefinitions &reachingDefinitions, AuxiliaryData &aux);

   protected:
   TR::Compilation *comp() { return _compilation; }
   TR::Optimizer *optimizer() { return _optimizer; }

   TR_Memory *    trMemory()      { TR_ASSERT(false, "trMemory is prohibited in TR_UseDefInfo. Use comp->allocator()"); return NULL; }
   TR_StackMemory trStackMemory() { TR_ASSERT(false, "trMemory is prohibited in TR_UseDefInfo. Use comp->allocator()"); return NULL; }
   TR_HeapMemory  trHeapMemory()  { TR_ASSERT(false, "trMemory is prohibited in TR_UseDefInfo. Use comp->allocator()"); return NULL; }

   bool trace()       {return _trace;}

   public:
   TR::Allocator allocator() { return comp()->allocator(); }
   bool infoIsValid() {return _isUseDefInfoValid;}
   TR::Node *getNode(int32_t index);
   TR::TreeTop *getTreeTop(int32_t index);
   void clearNode(int32_t index) { _useDefs[index] = TR_UseDef(); }
   bool getUsesFromDefIsZero(int32_t defIndex, bool loadAsDef = false);
   bool getUsesFromDef(BitVector &usesFromDef, int32_t defIndex, bool loadAsDef = false);

   private:
   const BitVector &getUsesFromDef_ref(int32_t defIndex, bool loadAsDef = false);
   public:

   bool getUseDefIsZero(int32_t useIndex);
   bool getUseDef(BitVector &useDef, int32_t useIndex);
   bool getUseDef_noExpansion(BitVector &useDef, int32_t useIndex);
   private:
   const BitVector &getUseDef_ref(int32_t useIndex, BitVector *defs = NULL);
   const BitVector &getUseDef_ref_body(int32_t useIndex, TR_BitVector *visitedDefs, TR_UseDefInfo::BitVector *defs = NULL);
   public:

   void          setUseDef(int32_t useIndex, int32_t defIndex);
   void          resetUseDef(int32_t useIndex, int32_t defIndex);
   void          clearUseDef(int32_t useIndex);
   private:
   bool          isLoadAddrUse(TR::Node * node);

   public:
   bool getDefiningLoads(BitVector &definingLoads, TR::Node *node);
   private:
   const BitVector &getDefiningLoads_ref(TR::Node *node);
   public:

   TR::Node      *getSingleDefiningLoad(TR::Node *node);
   void          resetDefUseInfo() {_defUseInfo.clear();}

   bool          skipAnalyzingForCompileTime(TR::Node *node, TR::Block *block, TR::Compilation *comp, AuxiliaryData &aux);

   private:

   void    findTrivialSymbolsToExclude(TR::Node *node, TR::TreeTop *treeTop, AuxiliaryData &aux);
   bool    isTrivialUseDefNode(TR::Node *node, AuxiliaryData &aux);
   bool    isTrivialUseDefNodeImpl(TR::Node *node, AuxiliaryData &aux);
   bool    isTrivialUseDefSymRef(TR::SymbolReference *symRef, AuxiliaryData &aux);

   // For Languages where an auto can alias a volatile, extra care needs to be taken when setting up use-def
   // The conservative answer is to not index autos that have volatile aliases.

   void setVolatileSymbolsIndexAndRecurse(TR::BitVector &volatileSymbols, int32_t symRefNum);
   void findAndPopulateVolatileSymbolsIndex(TR::BitVector &volatileSymbols);

   bool shouldIndexVolatileSym(TR::SymbolReference *ref, AuxiliaryData &aux);



   public:
   int32_t getNumIrrelevantStores() { return _numIrrelevantStores; }
   int32_t getNumUseNodes() {return _numUseOnlyNodes+_numDefUseNodes;}
   int32_t getNumDefNodes() {return _numDefOnlyNodes+_numDefUseNodes;}
   int32_t getNumDefOnlyNodes() {return _numDefOnlyNodes;}
   int32_t getNumDefsOnEntry() { return (_uniqueIndexForDefsOnEntry ? _numDefsOnEntry : 1); }
   int32_t getTotalNodes()  {return _numDefOnlyNodes+_numDefUseNodes+_numUseOnlyNodes;}
   int32_t getFirstUseIndex() {return _numDefOnlyNodes;}
   int32_t getLastUseIndex() {return getTotalNodes()-1;}
   int32_t getFirstDefIndex() {return 0;}
   int32_t getFirstRealDefIndex() { return getFirstDefIndex()+getNumDefsOnEntry();}
   int32_t getLastDefIndex() {return getNumDefNodes()-1;}

   bool    isDefIndex(uint32_t index) { return index && index <= getLastDefIndex(); }
   bool    isUseIndex(uint32_t index)  { return index >= getFirstUseIndex() && index <= getLastUseIndex(); }

   bool    hasGlobalsUseDefs() {return _indexFields && _indexStatics;}
   bool    canComputeReachingDefs();
   // Information used by the reaching def analysis
   //
   int32_t getNumExpandedUseNodes()
               {return _numExpandedUseOnlyNodes+_numExpandedDefUseNodes;}
   int32_t getNumExpandedDefNodes()
               {return _numExpandedDefOnlyNodes+_numExpandedDefUseNodes;}
   int32_t getNumExpandedDefsOnEntry() {return _numDefsOnEntry;}
   int32_t getExpandedTotalNodes()
               {return _numExpandedDefOnlyNodes+_numExpandedDefUseNodes+_numExpandedUseOnlyNodes;}
   uint32_t getNumAliases (TR::SymbolReference * symRef, AuxiliaryData &aux)
               {return aux._numAliases[symRef->getReferenceNumber()];}

   protected:
   uint32_t getBitVectorSize() {return getNumExpandedDefNodes(); }

   private:

   bool    excludedGlobals(TR::Symbol *);
   bool    isValidAutoOrParm(TR::SymbolReference *);

   public:
   bool    isExpandedDefIndex(uint32_t index)
               { return index && index < getNumExpandedDefNodes(); }
   bool    isExpandedUseIndex(uint32_t index)
               { return index >= _numExpandedDefOnlyNodes && index < getExpandedTotalNodes(); }
   bool    isExpandedUseDefIndex(uint32_t index)
               { return index >= _numExpandedDefOnlyNodes && index < getNumExpandedDefNodes(); }

   bool getDefsForSymbol(TR_BitVector &defs, int32_t symIndex, AuxiliaryData &aux)
      {
      defs |= *(aux._defsForSymbol[symIndex]);
      return !defs.isEmpty();
      }

   bool getDefsForSymbolIsZero(int32_t symIndex, AuxiliaryData &aux)
      {
      return aux._defsForSymbol[symIndex]->isEmpty();
      }

   bool hasLoadsAsDefs() { return _hasLoadsAsDefs; }
   private:

   void dereferenceDefs(int32_t useIndex, BitVector &nodesLookedAt, BitVector &loadDefs);

   public:
   void dereferenceDef(BitVector &useDefInfo, int32_t defIndex, BitVector &nodesLookedAt);
   void buildDefUseInfo(bool loadAsDef = false);
   int32_t getSymRefIndexFromUseDefIndex(int32_t udIndex);

   public:
   int32_t getNumSymbols() { return _numSymbols; }
   int32_t getMemorySymbolIndex (TR::Node *);
   bool isPreciseDef(TR::Node *def, TR::Node *use);
   bool isChildUse(TR::Node *node, int32_t childIndex);

   public:
   bool     _useDefForRegs;
   private:
   bool     _useDefForMemorySymbols;

   void buildValueNumbersToMemorySymbolsMap();
   void findMemorySymbols(TR::Node *node);
   void fillInDataStructures(AuxiliaryData &aux);

   bool indexSymbolsAndNodes(AuxiliaryData &aux);
   bool findUseDefNodes(
      TR::Block *block,
      TR::Node *node,
      TR::Node *parent,
      TR::TreeTop *treeTop,
      AuxiliaryData &aux,
      TR::deque<uint32_t, TR::Region&> &symRefToLocalIndexMap,
      bool considerImplicitStores = false
      );
   bool assignAdjustedNodeIndex(TR::Block *, TR::Node *node, TR::Node *parent, TR::TreeTop *treeTop, AuxiliaryData &aux, bool considerImplicitStores = false);
   bool childIndexIndicatesImplicitStore(TR::Node * node, int32_t childIndex);
   void insertData(TR::Block *, TR::Node *node, TR::Node *parent, TR::TreeTop *treeTop, AuxiliaryData &aux, TR::SparseBitVector &, bool considerImplicitStores = false);
   void buildUseDefs(void *vblockInfo, AuxiliaryData &aux);
   void buildUseDefs(TR::Node *node, void *vanalysisInfo, TR_BitVector &nodesToBeDereferenced, TR::Node *parent, AuxiliaryData &aux);
   int32_t setSingleDefiningLoad(int32_t useIndex, BitVector &nodesLookedAt, BitVector &loadDefs);

   protected:

   TR::Compilation *      _compilation;
   TR::Optimizer *      _optimizer;

   private:

   TR::vector<std::pair<TR::Node *, TR::TreeTop *>, TR::Region&> _atoms;

   private:
   TR::vector<BitVector, TR::Region&> _useDefInfo;
   bool _isUseDefInfoValid;

   TR::list<BitVector, TR::Region&> _infoCache;                    ///< initially empty bit vectors that are used for caching
   const BitVector _EMPTY;                                         ///< the empty bit vector
   TR::vector<const BitVector *,TR::Region&> _useDerefDefInfo; ///< all load defs are dereferenced
   TR::vector<BitVector, TR::Region&> _defUseInfo;
   TR::vector<BitVector, TR::Region&> _loadDefUseInfo;
   TR::vector<int32_t, TR::Region&> _sideTableToSymRefNumMap;

   // Checklist used in getUseDef_ref
   TR_BitVector        *_defsChecklist;

   int32_t             _numDefOnlyNodes;
   int32_t             _numDefUseNodes;
   int32_t             _numUseOnlyNodes;
   int32_t             _numIrrelevantStores;
   int32_t             _numExpandedDefOnlyNodes;
   int32_t             _numExpandedDefUseNodes;
   int32_t             _numExpandedUseOnlyNodes;
   int32_t             _numDefsOnEntry;
   int32_t             _numSymbols;
   int32_t             _numNonTrivialSymbols;
   int32_t             _numStaticsAndFields;

   bool                _indexFields;
   bool                _indexStatics;
   bool                _tempsOnly;
   bool                _trace;
   bool                _hasLoadsAsDefs;
   bool                _hasCallsAsUses;
   bool                _uniqueIndexForDefsOnEntry;

   class TR_UseDef
      {
      public:
      TR_UseDef() :_useDef(NULL) {}
      TR_UseDef(TR::TreeTop *tt) : _def((TR::TreeTop *)((intptr_t)tt|1)) {}
      TR_UseDef(TR::Node  *node) : _useDef(node) {}
      bool        isDef()     { return ((intptr_t)_def & 1) != 0; }
      TR::TreeTop *getDef()    { TR_ASSERT( isDef(), "assertion failure"); return (TR::TreeTop*)((intptr_t)_def & ~1); }
      TR::Node    *getUseDef() { TR_ASSERT(!isDef(), "assertion failure"); return _useDef;   }

      private:
      /** Tagged bottom bit means def */
      union
         {
         TR::Node    *_useDef;
         TR::TreeTop *_def;
         };
      };


   TR::vector<TR_UseDef, TR::Region &> _useDefs;

   class MemorySymbol
      {
      uint32_t         _size;
      uint32_t         _offset;
      int32_t          _localIndex;

      MemorySymbol(uint32_t size, uint32_t offset, int32_t localIndex) : _size(size), _offset(offset), _localIndex(localIndex) {}

      friend class TR_UseDefInfo;
      };
   typedef TR::list<MemorySymbol, TR::Region&> MemorySymbolList;

   int32_t                 _numMemorySymbols;
   TR::vector<MemorySymbolList *, TR::Region&> _valueNumbersToMemorySymbolsMap;
   TR_ValueNumberInfo *_valueNumberInfo;
   TR::CFG                   *_cfg;
   };

#endif
