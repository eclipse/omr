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

#include "optimizer/UseDefInfo.hpp"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/allocator.h"
#include "cs2/bitvectr.h"
#include "cs2/hashtab.h"
#include "cs2/sparsrbit.h"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/ValueNumberInfo.hpp"


#define MAX_EXPANDED_DEF_OR_USE_NODES         (65000)
#define MAX_DEF_BY_USE_NODES_FOR_LOW_OPT      (10000000)
#define MAX_DEF_BY_USE_NODES_FOR_HIGH_OPT     (INT_MAX/100-1) // will be multiplied by 100 for big methods


#define MAX_SYMBOLS                           USHRT_MAX
#define MAX_EXPANDED_TOTAL_NODES              USHRT_MAX
#define SIDE_TABLE_LIMIT                     (USHRT_MAX)
#define REACHING_DEFS_LIMIT                  (25000000)  // 25 Million


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
TR_UseDefInfo::TR_UseDefInfo(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer,
      bool requiresGlobals, bool prefersGlobals, bool loadsShouldBeDefs, bool cannotOmitTrivialDefs, bool conversionRegsOnly, 
      bool doCompletion, bool callsShouldBeUses)
   : _region(comp->trMemory()->heapMemoryRegion()),
     _compilation(comp),
     _optimizer(optimizer),
     _atoms(0, std::make_pair<TR::Node *, TR::TreeTop *>(NULL, NULL), _region),
     _useDefForMemorySymbols(false),
     _useDefInfo(0, TR_UseDefInfo::BitVector(comp->allocator()), _region),
     _isUseDefInfoValid(false),
     _infoCache(_region),
     _EMPTY(comp->allocator()),
     _useDerefDefInfo(0, static_cast<const BitVector *>(NULL), _region),
     _defUseInfo(0, TR_UseDefInfo::BitVector(comp->allocator()), _region),
     _loadDefUseInfo(0, TR_UseDefInfo::BitVector(comp->allocator()), _region),
     _tempsOnly(false),
     _trace(comp->getOption(TR_TraceUseDefs)),
     _hasLoadsAsDefs(loadsShouldBeDefs),
     _hasCallsAsUses(callsShouldBeUses),
     _useDefs(0, _region),
     _numMemorySymbols(0),
     _valueNumbersToMemorySymbolsMap(0, static_cast<MemorySymbolList *>(NULL), _region),
     _sideTableToSymRefNumMap(comp->getSymRefCount(), _region),
     _cfg(cfg),
     _valueNumberInfo(NULL)
   {
   if (doCompletion)
      prepareUseDefInfo(requiresGlobals, prefersGlobals, cannotOmitTrivialDefs, conversionRegsOnly);
   }

void TR_UseDefInfo::prepareUseDefInfo(bool requiresGlobals, bool prefersGlobals, bool cannotOmitTrivialDefs, bool conversionRegsOnly)
   {
   LexicalTimer tlex("useDefInfo", comp()->phaseTimer());

   TR_UseDefInfo::AuxiliaryData aux(
      comp()->getSymRefCount(),
      comp()->getNodeCount(),
      comp()->trMemory()->heapMemoryRegion(),
      comp()->allocator("UseDefAux")
      );

   int32_t i;
   dumpOptDetails(comp(), "   (Building use/def info)\n");

   if (trace())
      {
      traceMsg(comp(), "started initialization of use/def info\n");
      comp()->dumpMethodTrees("Pre Use Def Trees");
      }

   bool canBuild = false;
   _hasCallsAsUses = false;
   _uniqueIndexForDefsOnEntry = false;

   if (comp()->cg()->getGRACompleted() && conversionRegsOnly)
      _useDefForRegs = true;
   else
      _useDefForRegs = false;

   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
   int32_t numSymRefs = comp()->getSymRefCount();

   if (_hasLoadsAsDefs &&
       !cannotOmitTrivialDefs &&
       (comp()->getMethodHotness() < hot))
      {
      for (int32_t j = 0; j < numSymRefs; j++)
         {
         if (symRefTab->getSymRef(j) &&
             symRefTab->getSymRef(j)->getSymbol()->isAutoOrParm())
            {
            aux._onceReadSymbolsIndices[j].GrowTo(comp()->getNodeCount()); /* no effect for sparse bit vectors, except to make non-null */
            }
         }
      }

   if (_hasLoadsAsDefs &&
         !cannotOmitTrivialDefs)
      {
      for (int32_t j = 0; j < numSymRefs; j++)
         {
         if (symRefTab->getSymRef(j) &&
               symRefTab->getSymRef(j)->getSymbol()->isAutoOrParm())
            {
            aux._onceWrittenSymbolsIndices[j].GrowTo(comp()->getNodeCount()); /* no effect for sparse bit vectors, except to make non-null */
            }
         }
      }

   aux._neverWrittenSymbols.setAll(numSymRefs);
   aux._neverReadSymbols.setAll(numSymRefs);

   aux._neverReferencedSymbols.setAll(numSymRefs);

   comp()->incVisitCount();
   TR::TreeTop *treeTop;
   for (treeTop = comp()->getStartTree(); treeTop != NULL; treeTop = treeTop->getNextTreeTop())
      findTrivialSymbolsToExclude(treeTop->getNode(), treeTop, aux);

   if (trace())
      {
      //int32_t j;
      //for (j=0;j<numSymRefs;j++)
      //   {
      //   if (_onceWrittenSymbols[j])
      //      dumpOptDetails(comp(), "Printing once written sym %d at node %p\n", j, _onceWrittenSymbols[j]->getNode());
      //   }
      }

   if (requiresGlobals || prefersGlobals)
      {
      if (!comp()->getOption(TR_DisableUseDefForShadows))
         _useDefForMemorySymbols = true;
      }

   if (_useDefForMemorySymbols)
      {
      // TODO: This should be a member of TR_UseDefInfo::AuxiliaryData, and discarded when aux is discarded
      // but to do this TR_ValueNumberInfo should be converted to not be dynamically allocated
      // Also clean up deletes for dynamic allocation
      _valueNumberInfo = optimizer()->createValueNumberInfo(false, false, true);
      buildValueNumbersToMemorySymbolsMap();
      }

   if (requiresGlobals || prefersGlobals)
      {
      _indexFields = true;
      _indexStatics = true;
      canBuild = indexSymbolsAndNodes(aux);

      // first try to exclude registers
      if (!canBuild &&
          _useDefForRegs)
         {
         _useDefForRegs = false;
         canBuild = indexSymbolsAndNodes(aux);
         }

      if (requiresGlobals && !canBuild)
         {
         invalidateUseDefInfo();
         optimizer()->setCantBuildGlobalsUseDefInfo(true);
         if (trace())
            traceMsg(comp(),"Use/Def info: cannot build global use/def info as it failed in indexSymbolsAndNodes\n");
         return;
         }
      }

   if (prefersGlobals && !canBuild)
      {
      _indexFields = false;
      canBuild = indexSymbolsAndNodes(aux);
      }

   if (!canBuild)
      {
      _indexFields = false;
      _indexStatics = false;
      canBuild = indexSymbolsAndNodes(aux);
      }

   if (!canBuild)
      {
      invalidateUseDefInfo();
      optimizer()->setCantBuildGlobalsUseDefInfo(true);
      optimizer()->setCantBuildLocalsUseDefInfo(true);
      if (trace())
         traceMsg(comp(),"Use/Def info: cannot build global nor local use/def info as it failed in indexSymbolsAndNodes\n");
      return;
      }


   // Adjust the use/def index for each node now that the sizes are known.
   // There is still potential for index overflow when the adjustment is done.
   //
   comp()->incVisitCount();
   TR::Block *block = NULL;

   for (TR::TreeTop * treeTop = comp()->getStartTree(); treeTop != NULL; treeTop = treeTop->getNextTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
         {
         block = treeTop->getNode()->getBlock();
         }
      if (!assignAdjustedNodeIndex(block, treeTop->getNode(), NULL, treeTop, aux))
         {
         invalidateUseDefInfo();
         optimizer()->setCantBuildGlobalsUseDefInfo(true);
         optimizer()->setCantBuildLocalsUseDefInfo(true);
         return;
         }
      }

   if (trace())
      {
      traceMsg(comp(), "Has loads as defs              = %d\n", _hasLoadsAsDefs);
      traceMsg(comp(), "Number of symbols              = %d\n", _numSymbols);
      traceMsg(comp(), "Number of memory symbols       = %d\n", _numMemorySymbols);
      traceMsg(comp(), "Number of statics and fields   = %d\n", _numStaticsAndFields);
      traceMsg(comp(), "Total nodes for use/def info   = %d\n", getTotalNodes());
      traceMsg(comp(), "   Number of defOnly nodes     = %d\n", _numDefOnlyNodes);
      traceMsg(comp(), "   Number of defUse nodes      = %d\n", _numDefUseNodes);
      traceMsg(comp(), "   Number of useOnly nodes     = %d\n", _numUseOnlyNodes);
      traceMsg(comp(), "Total nodes for reaching defs  = %d\n", getExpandedTotalNodes());
      traceMsg(comp(), "   Number of defOnly nodes     = %d\n", _numExpandedDefOnlyNodes);
      traceMsg(comp(), "   Number of defUse nodes      = %d\n", _numExpandedDefUseNodes);
      traceMsg(comp(), "   Number of useOnly nodes     = %d\n", _numExpandedUseOnlyNodes);
      traceMsg(comp(), "   Number of defs on entry     = %d\n", _numDefsOnEntry);
      }

   _atoms.resize(getTotalNodes());
   _defsChecklist = new (_region) TR_BitVector(getTotalNodes(), _region);

   //  traceMsg(comp(), "Growing useDefInfo to %d\n",getNumUseNodes());
   _useDefInfo.resize(getNumUseNodes(), TR_UseDefInfo::BitVector(comp()->allocator()));
   //   for (i = getNumUseNodes()-1; i >= 0; --i)
   //      _useDefInfo[i].GrowTo(getNumDefNodes());
   _isUseDefInfoValid = true;

   int numRegisters = _useDefForRegs ? comp()->cg()->getNumberOfGlobalRegisters() : 0;

   aux._defsForSymbol.resize(_numSymbols + numRegisters, NULL);
   for (i = 0; i < _numSymbols + numRegisters; ++i)
      aux._defsForSymbol[i] = new (aux._region) TR_BitVector(aux._region);

   // Initialize the array with definitions from method entry
   //
   for (i = 0; i < _numDefsOnEntry; ++i)
      {
      int32_t j;
      if (i < (_numDefsOnEntry - numRegisters))
         j = i;
      else
         j = _numSymbols + (i - (_numDefsOnEntry - numRegisters));

      aux._defsForSymbol[j]->set(i);
      }

   aux._sideTableToUseDefMap.resize(getExpandedTotalNodes());

   int32_t expandedNodeArraySize = getExpandedTotalNodes() * sizeof(TR::Node*);
   aux._expandedAtoms.resize(expandedNodeArraySize, std::make_pair<TR::Node*, TR::TreeTop*>(NULL, NULL));

   fillInDataStructures(aux);

   // Flatten the info.. keep the node around for uses - and the treetop around for defs
   //
   _useDefs.resize(getTotalNodes());
   for (int32_t k = 0; k < getTotalNodes(); ++k)
      {
      std::pair<TR::Node *, TR::TreeTop *> &atom = _atoms[k];
      TR::Node *node = atom.first;
      if (!node) continue;
      int32_t index = node->getUseDefIndex();
      if (isDefIndex(index) && !isUseIndex(index))
         _useDefs[k] = TR_UseDef(atom.second);
      else
         _useDefs[k] = TR_UseDef(node);
      }

   if (trace())
      traceMsg(comp(), "completed initialization of use/def info\n\n");

   // virtually complete use/def info with reaching definitions analysis
   performAnalysis(aux);
   if (_valueNumberInfo)
      {
      delete _valueNumberInfo;
      _valueNumberInfo = NULL;
      }
   }

void TR_UseDefInfo::invalidateUseDefInfo()
   {
   _isUseDefInfoValid = false;
   _useDefInfo.clear();
   _defUseInfo.clear();
   _loadDefUseInfo.clear();
   if (_valueNumberInfo)
      {
      delete _valueNumberInfo;
      _valueNumberInfo = NULL;
      }
   return;
   }

bool TR_UseDefInfo::performAnalysis(AuxiliaryData &aux)
   {
   if (!_isUseDefInfoValid)
      //use-def info hasn't been successfully generated so performing reaching definition analysis is not possible
      return false;
   if (trace())
      traceMsg(comp(), "started reaching definition analysis for use/def\n\n");
   if (_numNonTrivialSymbols > 0)
      {
      bool succeeded = true;
         {
         TR_ReachingDefinitions rd(comp(), _cfg, optimizer(), this, aux, trace());
         succeeded = _runReachingDefinitions(rd, aux);
         }

      if (!succeeded)
         {
         return false;
         }
      }
   else
      {
      processReachingDefinition(NULL, aux);
      }
   if (trace())
      traceMsg(comp(), "completed reaching definition analysis for use/def\n\n");
   return true;
   }

bool TR_UseDefInfo::_runReachingDefinitions(TR_ReachingDefinitions& reachingDefinitions,
                                            AuxiliaryData &aux)
   {
   // stack memory is prohibited in UseDefInfo, but here we have to use it as reachingDefinitions use stack memory for _blockAnalysisInfo
   TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());

   reachingDefinitions.perform();

   bool succeeded = reachingDefinitions._blockAnalysisInfo;
   if (!succeeded)
      {
      invalidateUseDefInfo();
      if (trace())
         traceMsg(comp(), "Method too complex to perform reaching defs, use/def info not built\n");
      }
   else
      {
      // Build use/def information from the bit vector information from reaching defs
      //
      LexicalTimer tlex2("useDefInfo_buildUseDefs", comp()->phaseTimer());
      processReachingDefinition(reachingDefinitions._blockAnalysisInfo, aux);
      }

   return succeeded;
   }

void TR_UseDefInfo::setVolatileSymbolsIndexAndRecurse(TR::BitVector &volatileSymbols, int32_t symRefNum)
   {
   TR::SymbolReference* symRef = comp()->getSymRefTab()->getSymRef(symRefNum);

   if (!symRef || !symRef->getSymbol())
      return;


   if(volatileSymbols[symRef->getReferenceNumber()])    //already set, do not need to recurse.
      return;

 //  traceMsg(comp(), "JIAG, setting volatile Symbols for symref %d.\n",symRefNum);
   volatileSymbols[symRefNum] = true;


   TR::SparseBitVector aliases(comp()->allocator());
   symRef->getUseDefAliases().getAliases(aliases);
   symRef->getUseonlyAliases().getAliasesAndUnionWith(aliases);


   TR::SparseBitVector::Cursor aliasesCursor(aliases);
   for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
      {
      TR::SymbolReference *aliasedSymRef = comp()->getSymRefTab()->getSymRef(aliasesCursor);

      if (!aliasedSymRef || !aliasedSymRef->getSymbol())
         continue;

      setVolatileSymbolsIndexAndRecurse(volatileSymbols,aliasedSymRef->getReferenceNumber());
      }
   }

void TR_UseDefInfo::findAndPopulateVolatileSymbolsIndex(TR::BitVector &volatileSymbols)
   {
//   traceMsg(comp(), "In findAndPopulateVolatileSymbolsIndex\n");
   for (int32_t symRefNumber = comp()->getSymRefTab()->getIndexOfFirstSymRef(); symRefNumber < comp()->getSymRefCount(); symRefNumber++)
      {
 //     traceMsg(comp(), "Considering symRef %d: ",symRefNumber);
      TR::SymbolReference* symRef = comp()->getSymRefTab()->getSymRef(symRefNumber);

      if (!symRef || !symRef->getSymbol())
         continue;

      if (symRef->getSymbol()->isVolatile())
         {
 //        traceMsg(comp(), "it is volatile");
         setVolatileSymbolsIndexAndRecurse(volatileSymbols, symRefNumber);
         }
 //     traceMsg(comp(), "\n");
      }
   }

void TR_UseDefInfo::fillInDataStructures(AuxiliaryData &aux)
   {
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

   // Fill in the data structures
   //
   comp()->incVisitCount();
   TR::Block * block = NULL;
   TR::SparseBitVector expandedLIndexes(comp()->allocator()); //expanded localIndex for variables defined by function
   for (TR::TreeTop* treeTop = comp()->getStartTree(); treeTop != NULL; treeTop = treeTop->getNextTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
         {
         block = treeTop->getNode()->getBlock();
         }
      insertData(block, treeTop->getNode(), NULL, treeTop, aux, expandedLIndexes);
      }
   //If function foo defined variable a, then the expanded localIndex of a related to foo
   //must be in defsForSymbol of all functions aliased to a

   // Set the bit if defs on entry kill anything
   //
   if (_uniqueIndexForDefsOnEntry)
      {
      for (int32_t i = getFirstDefIndex(); i < _numDefsOnEntry; i++)
         {
         TR::SymbolReference  *symRef  = symRefTab->getSymRef(_sideTableToSymRefNumMap[i]);
         if (!symRef)
            continue;
         TR::SparseBitVector aliases(comp()->allocator());
         symRef->getUseDefAliases().getAliases(aliases);

         TR::SparseBitVector::Cursor aliasesCursor(aliases);
         for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
            {
            TR::SymbolReference *aliasedSymRef = comp()->getSymRefTab()->getSymRef(aliasesCursor);

            if (!aliasedSymRef)
               continue;
            TR::Symbol *aliasedSym = aliasedSymRef->getSymbol();
            if (!aliasedSym || !aliasedSym->isMethod())
               continue;
            if (aux._neverReferencedSymbols.get(aliasedSymRef->getReferenceNumber()))
               continue;
            if (excludedGlobals(aliasedSymRef->getSymbol()))
               continue;

            int32_t j = aliasedSymRef->getSymbol()->getLocalIndex();
            if (j == NULL_USEDEF_SYMBOL_INDEX)
               continue;

            aux._defsForSymbol[j]->set(i);

            //traceMsg(comp(),"\n _numDefsOnEntry=%d getNumExpandedDefNodes()=%d j=(u%d s#%d) i=(u%d s#%d)",_numDefsOnEntry, getNumExpandedDefNodes(), j, aliasedSymRef->getReferenceNumber(),i, symRef->getReferenceNumber());

            }
         }
      }
   }

bool TR_UseDefInfo::isLoadAddrUse(TR::Node * node)
   {
   return node->getOpCodeValue() == TR::loadaddr;
   }

void TR_UseDefInfo::findTrivialSymbolsToExclude(TR::Node *node, TR::TreeTop *treeTop, AuxiliaryData &aux)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return;

   node->setVisitCount(comp()->getVisitCount());

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      findTrivialSymbolsToExclude(node->getChild(i), treeTop, aux);


   if (node->getOpCode().hasSymbolReference() &&
       aux._neverReferencedSymbols.get(node->getSymbolReference()->getReferenceNumber()))
      {
      aux._neverReferencedSymbols.reset(node->getSymbolReference()->getReferenceNumber());
      }

   //if (!node->getOpCode().isStore() &&
   //    (node->getNumChildren() > 0))
   //   node = node->getFirstChild();

   if (node->getOpCode().isStoreDirect())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t symRefNum = symRef->getReferenceNumber();
      if (symRef->getSymbol()->isAutoOrParm())
         {
         if (!aux._onceReadSymbolsIndices[symRefNum].IsNull())
            {
            aux._onceReadSymbolsIndices[symRefNum][node->getGlobalIndex()] = true;
            if (trace())
               traceMsg(comp(), "SETTING node %p symRefNum %d\n", node, symRefNum);
            }

         if (aux._neverWrittenSymbols.get(symRefNum))
            {
            aux._neverWrittenSymbols.reset(symRefNum);

            if (trace())
               traceMsg(comp(), "Resetting write bit %d at node %p\n", symRefNum, node);

            if (!aux._onceWrittenSymbolsIndices[symRefNum].IsNull())
               {
               if (symRef->getSymbol()->isParm())
                  aux._onceWrittenSymbolsIndices[symRefNum].ClearToNull();
               else
                  aux._onceWrittenSymbolsIndices[symRefNum][node->getGlobalIndex()] = true;
               if (trace())
                  traceMsg(comp(), "Sym ref %d written once at node %p\n", symRefNum, treeTop->getNode());
               }
            }
         else if (!aux._onceWrittenSymbolsIndices[symRefNum].IsNull())
            {
            aux._onceWrittenSymbolsIndices[symRefNum].ClearToNull();
            }
         }
      }
   else if (node->getOpCode().isLoadVarDirect() ||
            isLoadAddrUse(node))
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t symRefNum = symRef->getReferenceNumber();
      if (symRef->getSymbol()->isAutoOrParm())
         {
         if (aux._neverReadSymbols.get(symRefNum))
            {
            aux._neverReadSymbols.reset(symRefNum);
            aux._loadsBySymRefNum[symRefNum] = node;

            if (trace())
               traceMsg(comp(), "Resetting read bit %d at node %p\n", symRefNum, node);
            }
         else if (!aux._onceReadSymbolsIndices[symRefNum].IsNull())
            {
            TR::Node *prevLoadNode = aux._loadsBySymRefNum[symRefNum];
            if ((prevLoadNode->getByteCodeIndex() != node->getByteCodeIndex()) ||
                (prevLoadNode->getInlinedSiteIndex() != node->getInlinedSiteIndex()))
               {
               aux._onceReadSymbolsIndices[symRefNum].ClearToNull();
               if (trace())
                  traceMsg(comp(), "KILLING bit %d at node %p\n", symRefNum, node);
               }
            }
         }
      }
   }

bool TR_UseDefInfo::isTrivialUseDefNode(TR::Node *node, AuxiliaryData &aux)
   {
   if (aux._doneTrivialNode.get(node->getGlobalIndex()))
      return aux._isTrivialNode.get(node->getGlobalIndex());

   bool result = isTrivialUseDefNodeImpl(node, aux);
   aux._doneTrivialNode.set(node->getGlobalIndex());
   if (result)
      aux._isTrivialNode.set(node->getGlobalIndex());

   return result;
   }

bool TR_UseDefInfo::isTrivialUseDefNodeImpl(TR::Node *node, AuxiliaryData &aux)
   {
   if (node->getOpCode().isStore() &&
       node->getSymbol()->isAutoOrParm() &&
       node->storedValueIsIrrelevant())
      return true;

   if (_useDefForRegs &&
       (node->getOpCode().isLoadReg() ||
       node->getOpCode().isStoreReg()))
      return false;

   TR::SymbolReference *symRef = node->getSymbolReference();
   if (symRef->getSymbol()->isParm())
      {
      if (!aux._neverWrittenSymbols.get(symRef->getReferenceNumber()))
         {
         return false;
         }
      }

   if (isTrivialUseDefSymRef(symRef, aux))
      return true;

   if (_hasLoadsAsDefs && symRef->getSymbol()->isAutoOrParm())
      {
      if (!aux._onceReadSymbolsIndices[symRef->getReferenceNumber()].IsNull() &&
          ((node->getOpCode().isLoadVarDirect() ||
            isLoadAddrUse(node)) ||
           (node->getOpCode().isStoreDirect() &&
            aux._onceReadSymbolsIndices[symRef->getReferenceNumber()].ValueAt(node->getGlobalIndex()))))
         return true;
      }
   else if (symRef->getSymbol()->isAutoOrParm() &&
            (node->getOpCode().isLoadVarDirect() ||
             (isLoadAddrUse(node))))
      return true;

   if (/* _hasLoadsAsDefs && */symRef->getSymbol()->isAutoOrParm())
      {
      if (!aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()].IsNull() &&
          ((node->getOpCode().isLoadVarDirect() ||
            isLoadAddrUse(node)) ||
           (node->getOpCode().isStoreDirect() &&
            aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()].ValueAt(node->getGlobalIndex()))))
         return true;
      }

   return false;
   }


bool TR_UseDefInfo::isTrivialUseDefSymRef(TR::SymbolReference *symRef, AuxiliaryData &aux)
   {
   //if (!_hasLoadsAsDefs)
   //   return false;

   int32_t symRefNum = symRef->getReferenceNumber();
   if (symRef->getSymbol()->isAutoOrParm())
      {
      if (symRef->getSymbol()->isParm())
         {
         if (!aux._neverWrittenSymbols.get(symRefNum))
            {
            return false;
            }
         }

      if (aux._neverWrittenSymbols.get(symRefNum))
         return true;
      else if (aux._neverReadSymbols.get(symRefNum))
         return true;
      }
   return false;
   }

bool TR_UseDefInfo::isValidAutoOrParm(TR::SymbolReference *symRef)
   {
   if (!symRef->getSymbol()->isAutoOrParm())
      return false;

   if (!_tempsOnly)
      return true;

   TR::BitVector useDefAliases(comp()->allocator());
   symRef->getUseDefAliases().getAliases(useDefAliases);

   TR::BitVector useOnlyAliases(comp()->allocator());
   symRef->getUseonlyAliases().getAliases(useOnlyAliases);

   return  (useDefAliases.PopulationCount() == 1 &&
            useOnlyAliases.PopulationCount() == 1);
   }

bool TR_UseDefInfo::excludedGlobals(TR::Symbol *sym)
   {
   if (sym->isStatic() && (sym->isConst() || sym->castToStaticSymbol()->isConstObjectRef()))
      return true;
   if ((sym->isStatic() || sym->isMethodMetaData()) && !_indexStatics)
      return true;
   else if (sym->isShadow() && !_indexFields)
      return true;
   else
      return false;
   }

void TR_UseDefInfo::buildValueNumbersToMemorySymbolsMap()
   {
   LexicalTimer tlex("useDefInfo_buildValueNosToMSM", comp()->phaseTimer());

   _valueNumbersToMemorySymbolsMap.resize(_valueNumberInfo->getNumberOfValues(), NULL);
   for (size_t i = 0; i < _valueNumbersToMemorySymbolsMap.size(); ++i)
      _valueNumbersToMemorySymbolsMap[i] = new (_region) MemorySymbolList(_region);

   comp()->incVisitCount();
   _numMemorySymbols = 0;

   TR::TreeTop * treeTop;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      findMemorySymbols(treeTop->getNode());
      }
   }

void TR_UseDefInfo::findMemorySymbols(TR::Node *node)
   {
   vcount_t visitCount = comp()->getVisitCount();
   if (visitCount == node->getVisitCount())
      return;
   node->setVisitCount(visitCount);

   // Process children
   //
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      findMemorySymbols(node->getChild(i));
      }

   if ((node->getOpCode().isLoadIndirect() || node->getOpCode().isStoreIndirect()) &&
        node->getSymbolReference()->getSymbol()->isShadow() &&
       _valueNumberInfo &&
       _valueNumberInfo->getNext(node->getFirstChild()) != node->getFirstChild()) // value number is shared with some other node
      {
      int32_t valueNumber = _valueNumberInfo->getValueNumber(node->getFirstChild());
      uint32_t size = node->getSymbolReference()->getSymbol()->getSize();
      uint32_t offset = node->getSymbolReference()->getOffset();

      bool found = false;
      for (auto itr = _valueNumbersToMemorySymbolsMap[valueNumber]->begin(), end = _valueNumbersToMemorySymbolsMap[valueNumber]->end();
           itr != end; ++itr)
         {
         MemorySymbol &memorySymbol = *itr;
         if ((memorySymbol._size == size) && (memorySymbol._offset == offset))
            {
            found = true;
            break;
            }
         }

      if (!found)
         _valueNumbersToMemorySymbolsMap[valueNumber]->push_front(MemorySymbol(size, offset, _numMemorySymbols++));

      if (trace())
         traceMsg(comp(), "Node %p has memory symbol index %d (%d:%d:%d)\n", node, _valueNumbersToMemorySymbolsMap[valueNumber]->front()._localIndex, valueNumber, size, offset);
      }
   }

int32_t TR_UseDefInfo::getMemorySymbolIndex(TR::Node * node)
   {
   if (!_useDefForMemorySymbols) return -1;

   if ((!node->getOpCode().isLoadIndirect() && !node->getOpCode().isStoreIndirect()) ||
          !node->getSymbolReference()->getSymbol()->isShadow() ||
        _valueNumberInfo->getNext(node->getFirstChild()) == node->getFirstChild())
      return -1;

   int32_t valueNumber = _valueNumberInfo->getValueNumber(node->getFirstChild());
   uint32_t size = node->getSymbolReference()->getSymbol()->getSize();
   uint32_t offset = node->getSymbolReference()->getOffset();

   for (auto itr = _valueNumbersToMemorySymbolsMap[valueNumber]->begin(), end = _valueNumbersToMemorySymbolsMap[valueNumber]->end();
        itr != end; ++itr)
      {
      MemorySymbol &memorySymbol = *itr;
      if ((memorySymbol._size == size) && (memorySymbol._offset == offset))
         return memorySymbol._localIndex;
      }
   TR_ASSERT(false, "Could not find memory symbol index for node %p (%d:%d:%d)", node, valueNumber, size, offset);
   return -1;
   }

bool TR_UseDefInfo::shouldIndexVolatileSym(TR::SymbolReference*ref, AuxiliaryData &aux)
   {
   if (! ref->getSymbol()->isVolatile())        //index non-volatiles
      return true;

   return false;
   }

/**
 * Find all symbols whose uses and defs are to be tracked and give them a
 * local index. This index is used to index into the array of bit vectors
 * that show which nodes define which symbols.
 *
 * At the same time count the number of defs on entry to the method.
 *
 * Symbol indices are assigned in a particular order as follows:
 *
 * 0 to (_numStaticsAndFields-1):
 *    Field, metadata and non-constant static symbols; these are the symbols
 *    that used to be killed by calls and by unresolved references.
 *    Currently _numStaticsAndFields is not used since kill sets are based
 *    on alias sets.
 *
 * _numStaticsAndFields to _numDefsOnEntry:
 *    Parameter, local, and constant static symbols; together with the
 *    previous group these are all the symbols that include the method entry
 *    as a definition point.
 */
bool TR_UseDefInfo::indexSymbolsAndNodes(AuxiliaryData &aux)
   {
   LexicalTimer tlex("indexSymbolsAndNodes", comp()->phaseTimer());
   if (trace())
      {
      traceMsg(comp(), "Trying to index nodes with _indexFields = %d, _indexStatics = %d\n", _indexFields, _indexStatics);
      }

   _numSymbols = _numMemorySymbols;
   _numNonTrivialSymbols = 0;
   int32_t symRefCount = comp()->getSymRefCount();
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
   TR::SymbolReference *symRef;
   TR::Symbol *sym;
   int32_t symRefNumber;

   if (trace())
      {
      traceMsg(comp(), "_neverReferencedSymbols[count = %d]: ", aux._neverReferencedSymbols.elementCount());
      aux._neverReferencedSymbols.print(comp());
      traceMsg(comp(), "\n");
      }

   TR_BitVector relevantAliases(aux._region);
   TR_BitVector methodsAndShadows(aux._region);
   TR_BitVector referencedSymRefs(aux._region);
   TR_BitVector aliases(aux._region);

   for (symRefNumber = symRefTab->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
      {
      LexicalTimer tlex("indexSymbolsAndNodes_refs", comp()->phaseTimer());
      symRef = symRefTab->getSymRef(symRefNumber);

      if (!symRef || !symRef->getSymbol())
         continue;

      if (aux._neverReferencedSymbols.get(symRefNumber))
         continue;

      referencedSymRefs.set(symRefNumber);

      if (excludedGlobals(symRef->getSymbol()))
         continue;

      if (symRef->getSymbol()->isMethod() || symRef->getSymbol()->isRegularShadow())
         methodsAndShadows.set(symRefNumber);
      else
         relevantAliases.set(symRefNumber);
      }

   TR_BitVectorIterator refIter(referencedSymRefs);
   while (refIter.hasMoreElements())
      {
      LexicalTimer tlex2("indexSymbolsAndNodes_refs", comp()->phaseTimer());

      symRefNumber = refIter.getNextElement();
      symRef = symRefTab->getSymRef(symRefNumber);
      if (symRef)
         {
         sym = symRef->getSymbol();

         // record number of relevant aliases
         uint32_t num_aliases = 0;
         uint32_t numMethodsAndShadows = 0;

         if (symRef->sharesSymbol())
            {
            symRef->getUseDefAliases().getAliases(aliases);
            num_aliases = aliases.commonElementCount(relevantAliases);
            numMethodsAndShadows = aliases.commonElementCount(methodsAndShadows);
            }

         if (num_aliases == 0 && !sym->isMethod())
            num_aliases = 1; // alias symbol with itself

         if (numMethodsAndShadows != 0)
            num_aliases++;

         if (symRef->getSymbol()->isShadow())
            num_aliases++;

         aux._numAliases[symRefNumber] = num_aliases;

         if (sym)
            {
            // Initialize all symbols with null usedef index
            //
            sym->setLocalIndex(NULL_USEDEF_SYMBOL_INDEX);

            // Volatile symbols must not be considered since they don't have
            // any specifiable def points.
            //
            if (!shouldIndexVolatileSym(symRef,aux))
               {
               if (trace())
                  traceMsg(comp(), "Ignoring Symbol [%p] because it is volatile %d or aliased to a volatile %d\n",sym,sym->isVolatile(),aux._volatileOrAliasedToVolatileSymbols.get(symRefNumber));
               continue;
               }

            if (excludedGlobals(sym))
               continue;

         if (!sym->isStatic() &&
             !sym->isMethodMetaData() &&
             !sym->isShadow())
               continue;

            // Index this symbol
            //
            if (trace())
               traceMsg(comp(), "Symbol [%p] has index %d\n", sym, _numSymbols);
            _sideTableToSymRefNumMap[_numSymbols] = symRefNumber;
            sym->setLocalIndex(_numSymbols++);
         if (!isTrivialUseDefSymRef(symRef, aux) &&
             ((sym->isParm() && !aux._neverWrittenSymbols.get(symRef->getReferenceNumber())) ||
              (aux._onceReadSymbolsIndices[symRef->getReferenceNumber()].IsNull() &&
               aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()].IsNull())))
               _numNonTrivialSymbols++;
            }
         }
      }

   _numStaticsAndFields = _numSymbols;

   refIter.reset();
   while (refIter.hasMoreElements())
      {
      symRefNumber = refIter.getNextElement();

      symRef = symRefTab->getSymRef(symRefNumber);

      if (trace())
         {
         traceMsg(comp(), "Non Trivial symbol Ref [%p:%d] isTrivial=%d isParm=%d _neverWritten=%d",
               symRef, symRef->getReferenceNumber(), isTrivialUseDefSymRef(symRef, aux),
               symRef->getSymbol()->isParm(), aux._neverWrittenSymbols.get(symRef->getReferenceNumber()));
         (*comp()) << " _onceRead= " << aux._onceReadSymbolsIndices[symRef->getReferenceNumber()];
         (*comp()) << " _onceWritten= " << aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()] << "\n";
         }

      if (symRef &&
          (!isTrivialUseDefSymRef(symRef, aux) &&
           ((symRef->getSymbol()->isParm() && !aux._neverWrittenSymbols.get(symRef->getReferenceNumber())) ||
            (aux._onceReadSymbolsIndices[symRef->getReferenceNumber()].IsNull() &&
             aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()].IsNull()))))
         {
         sym = symRef->getSymbol();
         if (trace())
            {
               traceMsg(comp(), "   sym=%p localIndex=%d \n", sym, sym->getLocalIndex());
            }
         if (sym && sym->getLocalIndex() == NULL_USEDEF_SYMBOL_INDEX)
            {
            if (!shouldIndexVolatileSym(symRef,aux))
               continue;

            if (sym->isStatic())
               {
               if (!_indexStatics)
                  continue;
               }
            if (sym->isMethod())
               {
               if (!_hasCallsAsUses)
                  continue;
               }
            else if (!isValidAutoOrParm(symRef))
               continue;

            // Index this symbol
            //
            if (trace())
               {
                  traceMsg(comp(), "Non Trivial symbol [%p] has index %d\n", sym, _numSymbols);
               }
            _sideTableToSymRefNumMap[_numSymbols] = symRefNumber;
            sym->setLocalIndex(_numSymbols++);
            _numNonTrivialSymbols++;
            }
         }
      }

   _numDefsOnEntry = _numSymbols;

   int numRegisters = _useDefForRegs ? comp()->cg()->getNumberOfGlobalRegisters() : 0;
   _numDefsOnEntry += numRegisters;

   refIter.reset();
   while (refIter.hasMoreElements())
      {
      symRefNumber = refIter.getNextElement();
      symRef = symRefTab->getSymRef(symRefNumber);
      if (symRef &&
          (isTrivialUseDefSymRef(symRef, aux) ||
           ((symRef->getSymbol()->isParm() &&
             aux._neverWrittenSymbols.get(symRef->getReferenceNumber()))) ||
            !aux._onceReadSymbolsIndices[symRef->getReferenceNumber()].IsNull() ||
            !aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()].IsNull()))
         {
         sym = symRef->getSymbol();
         if (sym && sym->getLocalIndex() == NULL_USEDEF_SYMBOL_INDEX)
            {
            if (!shouldIndexVolatileSym(symRef,aux))
               continue;

            if (sym->isStatic())
               {
               if (!_indexStatics)
                  continue;
               }
            else if (!isValidAutoOrParm(symRef))
               continue;

            // Index this symbol
            //
            if (trace())
               traceMsg(comp(), "Trivial symbol [%p] has index %d\n", sym, _numSymbols);
            sym->setLocalIndex(_numSymbols++);
            }
         }
      }

   // If there are too many symbols, do not build use/def info
   //
   if (_numSymbols + numRegisters >= MAX_SYMBOLS)
      {
      dumpOptDetails(comp(), "   use/def failed, too many symbols (%d)\n", _numSymbols);
      return false;
      }

   // Determine how many def nodes and use nodes there are.
   // Two calculations are made, one for the expanded set where each call or
   // unresolved reference contains a separate index for each alias, and the other
   // for the normal set where there is one index per node.
   // The expanded set is used during reaching definitions analysis and the normal
   // set is used to build the final use/def info.<
   // The expanded index is kept in the node's local index and the normal
   // index is kept in the node's use/def index.
   // Reserve the initial node index slots for the definitions coming from the
   // method entry.
   // Make sure that real indices start at least at 1 so that 0 can be used as a
   // null index.
   //
   if (_numDefsOnEntry)
      _numExpandedDefOnlyNodes = _numDefsOnEntry;
   else
      _numExpandedDefOnlyNodes = 1;

   if (!_uniqueIndexForDefsOnEntry)
      _numDefOnlyNodes = 1;
   else
      _numDefOnlyNodes = _numDefsOnEntry;
   _numDefUseNodes = _numExpandedDefUseNodes = 0;
   _numUseOnlyNodes = _numExpandedUseOnlyNodes = _numIrrelevantStores = 0;
   comp()->incVisitCount();
   TR::TreeTop *treeTop = NULL;
   TR::Block *block = NULL;

   // A cleared symRefToLocalIndexMap is required for each call to indexSymbolsAndNodes - so we create it as a local variable here,
   // and pass it down to findUseDefNodes, as findUseDefNodes is recursive.
   TR::deque<uint32_t, TR::Region&> symRefToLocalIndexMap(comp()->getSymRefCount(), _region);

   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      if (treeTop->getNode()->getOpCodeValue() == TR::BBStart)
         {
         block = treeTop->getNode()->getBlock();
         }
      if (!findUseDefNodes(block, treeTop->getNode(), NULL, treeTop, aux, symRefToLocalIndexMap))
         return false; // indices overflowed, can't build
      }

   if (_indexStatics)
      {
      _numUseOnlyNodes += 1;
      //   traceMsg(comp(), "Incrementing _numuseONlyNodes to %d because of _indexStatics\n",_numUseOnlyNodes);
      }

   // If there are too many node indices, do not build use/def info
   int32_t maxDefByUseNodes = ((comp()->getMethodHotness() >= hot) ) ?
         MAX_DEF_BY_USE_NODES_FOR_HIGH_OPT : MAX_DEF_BY_USE_NODES_FOR_LOW_OPT;
   int32_t maxExpandedDefOrUseNodes = MAX_EXPANDED_DEF_OR_USE_NODES;
   if (comp()->getOption(TR_ProcessHugeMethods))
      {
      // be carefull not to overflow (see macros defined above)
      maxExpandedDefOrUseNodes *= 100;
      maxDefByUseNodes *= 100;
      }
   if (getNumExpandedDefNodes() >= maxExpandedDefOrUseNodes)
      {
      dumpOptDetails(comp(), "   use/def failed, too many expanded def nodes (%d)\n", getNumExpandedDefNodes());
      return false;
      }
   if (getNumExpandedUseNodes() >= maxExpandedDefOrUseNodes)
      {
      dumpOptDetails(comp(), "   use/def failed, too many expanded use nodes (%d)\n", getNumExpandedUseNodes());
      return false;
      }
   if ((getNumDefNodes() - getNumIrrelevantStores()) * getNumUseNodes() >= maxDefByUseNodes)
      {
      dumpOptDetails(comp(), "   use/def failed, too many def/use combinations (%d)\n", (getNumDefNodes() - getNumIrrelevantStores())*getNumUseNodes());
      return false;
      }

   if (getExpandedTotalNodes() > MAX_EXPANDED_TOTAL_NODES)
      {
      dumpOptDetails(comp(), "   use/def failed, too many expanded nodes in total (%d)\n", getExpandedTotalNodes());
      return false;
      }

   if (!canComputeReachingDefs())
      {
      return false;
      }

   if (trace())
      dumpOptDetails(comp(), "   use/def:  indexing symbols and nodes succeeded\n");
   if (trace())
      {
      traceMsg(comp(), "      expanded nodes          = %d\n", getExpandedTotalNodes());
      traceMsg(comp(), "      def/use combinations    = %d\n", (getNumDefNodes() - getNumIrrelevantStores())*getNumUseNodes());
      traceMsg(comp(), "      irrelevant stores       = %d\n", getNumIrrelevantStores());
      traceMsg(comp(), "      expanded use nodes      = %d\n", getNumExpandedUseNodes());
      traceMsg(comp(), "      expanded def nodes      = %d\n", getNumExpandedDefNodes());
      traceMsg(comp(), "      local index level  = %d\n", _numExpandedDefUseNodes);
      traceMsg(comp(), "      use/def index level     = %d\n", _numDefOnlyNodes);
      }
   return true;
   }


bool TR_UseDefInfo::skipAnalyzingForCompileTime(TR::Node *node, TR::Block *block, TR::Compilation *comp, AuxiliaryData &aux)
   {
   if (isTrivialUseDefNode(node, aux))
      return true;

   if (comp->getMethodHotness() >= hot)
      return false;

   if (block->isCold())
      return true;

   return false;
   }



bool TR_UseDefInfo::findUseDefNodes(
   TR::Block *block,
   TR::Node *node,
   TR::Node *parent,
   TR::TreeTop *treeTop,
   AuxiliaryData &aux,
   TR::deque<uint32_t, TR::Region&> &symRefToLocalIndexMap,
   bool considerImplicitStores
   )
   {
   vcount_t visitCount = comp()->getVisitCount();
   if (visitCount == node->getVisitCount())
      return true;
   node->setVisitCount(visitCount);

   // Process children
   //
   bool parentCouldHaveImplicitStoreChildren = false;
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      bool shouldConsiderLoadAddrChildrenImplicitStores = parentCouldHaveImplicitStoreChildren && childIndexIndicatesImplicitStore(node, i);
      if (!findUseDefNodes(block, node->getChild(i), node, treeTop, aux, symRefToLocalIndexMap, shouldConsiderLoadAddrChildrenImplicitStores))
         return false;
      }

   TR::ILOpCode &opCode = node->getOpCode();
   TR::SymbolReference *symRef;
   uint32_t symIndex;
   uint32_t num_aliases;
   int32_t localIndex = 0;
   int32_t useDefIndex = 0;

   if (_useDefForRegs &&
       (opCode.isLoadReg() ||
       opCode.isStoreReg()))
      {
      symRef = NULL;
      num_aliases = 1;
      symIndex = _numSymbols + node->getGlobalRegisterNumber();
      _numNonTrivialSymbols++;
      }
   else
      {
      symRef = node->getOpCode().hasSymbolReference() ? node->getSymbolReference() : NULL;
      if (symRef == NULL)
         {
         node->setLocalIndex(0);
         node->setUseDefIndex(0);
         return true;
         }
      num_aliases = aux._numAliases[symRef->getReferenceNumber()];
      symIndex = symRef->getSymbol()->getLocalIndex();
      }

   if (symIndex != NULL_USEDEF_SYMBOL_INDEX)
      {
      if (parent && parent->getOpCode().isResolveCheck() &&
          num_aliases > 1)
         {
         localIndex = _numExpandedDefOnlyNodes;
         _numExpandedDefOnlyNodes += num_aliases;
         useDefIndex = _numDefOnlyNodes++;
         }
      else if (opCode.isLoadVarDirect() && _hasLoadsAsDefs)
         {
         if (!skipAnalyzingForCompileTime(node, block, comp(), aux))
            {
            localIndex = _numExpandedDefUseNodes++;
            }
         //else
         //   localIndex = 0;

         useDefIndex = _numDefUseNodes++;
         }
      else if (isLoadAddrUse(node) ||
               opCode.isLoadVar() ||
               (_useDefForRegs && opCode.isLoadReg()))
         {
         if (!skipAnalyzingForCompileTime(node, block, comp(), aux))
            localIndex = _numExpandedUseOnlyNodes++;
         //else
         //   localIndex = 0;

         useDefIndex = _numUseOnlyNodes++;
         //       traceMsg(comp(), "UDI: setting useDefIndex to %d _numUseOnlyNodes = %d\n",useDefIndex,_numUseOnlyNodes);
         }
      else if (opCode.isCall() || opCode.isFence())
         {
         if (num_aliases > 0)
            {
            if (symRefToLocalIndexMap[symRef->getReferenceNumber()] == 0)
               {
               localIndex = _numExpandedDefUseNodes;
               _numExpandedDefUseNodes += num_aliases;
               //traceMsg(comp(), "num_aliases #%d  %d %d %d\n", node->getSymbolReference()->getReferenceNumber(), num_aliases, opCode.isFence(), node->getSymbolReference()->getSymbol()->isMethod());
               useDefIndex = _numDefUseNodes++;
               //             traceMsg(comp(), "UDI: setting useDefIndex to %d _numDefUseNodes = %d\n",useDefIndex,_numDefUseNodes);

               symRefToLocalIndexMap[symRef->getReferenceNumber()] = localIndex;
               }
            else
               {
               localIndex = symRefToLocalIndexMap[symRef->getReferenceNumber()];
               useDefIndex = _numDefUseNodes++;
               }
            }
         //else
         //   {
         //   node->setLocalIndex(0);
         //   node->setUseDefIndex(0);
         //   }
         }
      else if (opCode.isStore() ||
               (_useDefForRegs && opCode.isStoreReg()) ||
                (considerImplicitStores && node->getOpCodeValue() == TR::loadaddr))
         {
         if (!isTrivialUseDefNode(node, aux))
            {
            localIndex = _numExpandedDefOnlyNodes;
            _numExpandedDefOnlyNodes += num_aliases;
            }
         //else
         //   {
         //   node->setLocalIndex(0);
         //   }

         useDefIndex = _numDefOnlyNodes++;
         if (opCode.isStore() && node->storedValueIsIrrelevant())
            _numIrrelevantStores++;

         if (isTrivialUseDefNode(node, aux))
            {
            if (symRef && aux._onceReadSymbolsIndices[symRef->getReferenceNumber()].ValueAt(node->getGlobalIndex()))
               {
               if (aux._onceReadSymbols[symRef->getReferenceNumber()] == NULL)
                  aux._onceReadSymbols[symRef->getReferenceNumber()] = new (aux._region) TR_BitVector(aux._region);
               aux._onceReadSymbols[symRef->getReferenceNumber()]->set(useDefIndex);
               }

            if (symRef && aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()].ValueAt(node->getGlobalIndex()))
               {
               if (aux._onceWrittenSymbols[symRef->getReferenceNumber()] == NULL)
                  aux._onceWrittenSymbols[symRef->getReferenceNumber()] = new (aux._region) TR_BitVector(aux._region);
               aux._onceWrittenSymbols[symRef->getReferenceNumber()]->set(useDefIndex);
               }
            }
         }
      //else
      //   {
      //   node->setLocalIndex(0);
      //   node->setUseDefIndex(0);
      //   }
      }
   else if (!_tempsOnly && (opCode.isCall() || opCode.isFence()) && num_aliases > 0)
      {
      if (!aux._volatileOrAliasedToVolatileSymbols.get(symRef->getReferenceNumber()))
         {
         localIndex = _numExpandedDefOnlyNodes;
         _numExpandedDefOnlyNodes += _numStaticsAndFields;
         useDefIndex = _numDefOnlyNodes++;
         }
      }
   else if (_indexFields && node->isGCSafePointWithSymRef() &&
            comp()->getOptions()->realTimeGC())
      {
      localIndex = _numExpandedDefOnlyNodes;
      _numExpandedDefOnlyNodes += TR::NumTypes + 1; // == arraylet shadows + read barrier....encode this where?  SymRefTab probably
      useDefIndex = _numDefOnlyNodes++;
      }
   //else
   //   {
   //   node->setLocalIndex(0);
   //   node->setUseDefIndex(0);
   //   }

   // Make sure there is no overflow in either index before setting them
   if (localIndex > MAX_SCOUNT)
      {
      dumpOptDetails(comp(), "   use/def failed, local index overflow (%d)\n", localIndex);
      return false;
      }
   if (useDefIndex > SIDE_TABLE_LIMIT)
      {
      dumpOptDetails(comp(), "   use/def failed, use/def index overflow (%d)\n", useDefIndex);
      return false;
      }
   node->setLocalIndex(localIndex);

   // traceMsg(comp(), "UDI: For node %p setting useDefIndex to %d\n",node,useDefIndex);
   node->setUseDefIndex(useDefIndex);
   return true;
   }

bool TR_UseDefInfo::assignAdjustedNodeIndex(TR::Block *block, TR::Node *node, TR::Node *parent, TR::TreeTop *treeTop, AuxiliaryData &aux,bool considerImplicitStores)
   {
   vcount_t visitCount = comp()->getVisitCount();
   if (visitCount == node->getVisitCount())
      return true;
   node->setVisitCount(visitCount);

   // Process children
   //
   bool parentCouldHaveImplicitStoreChildren = false;
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      bool shouldConsiderLoadAddrChildrenImplicitStores = parentCouldHaveImplicitStoreChildren && childIndexIndicatesImplicitStore(node, i);
      if (!assignAdjustedNodeIndex(block, node->getChild(i), node, treeTop,aux, shouldConsiderLoadAddrChildrenImplicitStores))
         return false;
      }

   TR::ILOpCode &opCode = node->getOpCode();
   TR::SymbolReference *symRef;
   int32_t symIndex;
   uint32_t num_aliases;

   if (_useDefForRegs &&
       (opCode.isLoadReg() ||
       opCode.isStoreReg()))
      {
      symRef = NULL;
      num_aliases = 1;
      symIndex = _numSymbols + node->getGlobalRegisterNumber();
      }
   else
      {
      if (!opCode.hasSymbolReference())
         return true;

      symRef = node->getSymbolReference();
      if (!symRef)
         return true;

      symIndex = symRef->getSymbol()->getLocalIndex();
      num_aliases = aux._numAliases[symRef->getReferenceNumber()];
      }

   uint32_t nodeIndex = node->getUseDefIndex();
   int32_t adjustment = 0, expandedAdjustment = 0;

   if (symIndex != NULL_USEDEF_SYMBOL_INDEX)
      {
      if (parent && parent->getOpCode().isResolveCheck() &&
          num_aliases > 1)
         {
         expandedAdjustment = adjustment = 0;
         }
      else if (opCode.isLoadVarDirect() && _hasLoadsAsDefs)
         {
         if (!skipAnalyzingForCompileTime(node, block, comp(), aux))
            {
            expandedAdjustment = _numExpandedDefOnlyNodes;
            }
         else
            {
            expandedAdjustment = 0;
            }
         adjustment = _numDefOnlyNodes;
         }
      else if (isLoadAddrUse(node) ||
               opCode.isLoadVar() ||
               (_useDefForRegs && opCode.isLoadReg()))
         {
         if (!skipAnalyzingForCompileTime(node, block, comp(), aux))
            expandedAdjustment = _numExpandedDefOnlyNodes + _numExpandedDefUseNodes;
         else
            {
            expandedAdjustment = 0;
            }
         adjustment = _numDefOnlyNodes + _numDefUseNodes;
         //         traceMsg(comp(), "node %p setting adjustment to %d _numDefOnlyNodes = %d, _numDefUseNodes = %d\n",node, adjustment,_numDefOnlyNodes,_numDefUseNodes);
         }
      else if (opCode.isCall() || opCode.isFence())
         {
         if (num_aliases)
            {
            expandedAdjustment = _numExpandedDefOnlyNodes;
            adjustment = _numDefOnlyNodes;
            }
         else
            return true;
         }
      else if (opCode.isStore() ||
               (_useDefForRegs && opCode.isStoreReg()) ||
               (considerImplicitStores && node->getOpCodeValue() == TR::loadaddr))
         {
         expandedAdjustment = adjustment = 0;
         }
      else
         return true;
      }
   else if (nodeIndex)
      {
      if (opCode.isCall() || opCode.isFence())
         {
         expandedAdjustment = adjustment = 0;
         }
      else if (_indexFields)
         {
         TR_ASSERT(false, "error in assignIndex");
         expandedAdjustment = adjustment = 0;
         }
      }
   else
      return true;


   if (symRef && aux._onceReadSymbolsIndices[symRef->getReferenceNumber()].ValueAt(node->getGlobalIndex()))
      {
      if (aux._onceReadSymbols[symRef->getReferenceNumber()] == NULL)
         aux._onceReadSymbols[symRef->getReferenceNumber()] = new (aux._region) TR_BitVector(aux._region);
      aux._onceReadSymbols[symRef->getReferenceNumber()]->reset(nodeIndex);
      aux._onceReadSymbols[symRef->getReferenceNumber()]->set(nodeIndex+adjustment);
      }

   if (symRef && aux._onceWrittenSymbolsIndices[symRef->getReferenceNumber()].ValueAt(node->getGlobalIndex()))
      {
      if (aux._onceWrittenSymbols[symRef->getReferenceNumber()] == NULL)
         aux._onceWrittenSymbols[symRef->getReferenceNumber()] = new (aux._region) TR_BitVector(aux._region);
      aux._onceWrittenSymbols[symRef->getReferenceNumber()]->reset(nodeIndex);
      aux._onceWrittenSymbols[symRef->getReferenceNumber()]->set(nodeIndex+adjustment);
      }

   // Make sure there is no overflow in either index before setting them
   if (nodeIndex + adjustment > SIDE_TABLE_LIMIT)
      {
      dumpOptDetails(comp(), "   use/def failed, use/def index overflow (%d)\n", nodeIndex+adjustment);
      return false;
      }
   //   traceMsg(comp(), "UDI: Adjusting node %p index to %d\n",node,nodeIndex+adjustment);
   node->setUseDefIndex(nodeIndex + adjustment);
   if (node->getLocalIndex() + expandedAdjustment > MAX_SCOUNT)
      {
      dumpOptDetails(comp(), "   use/def failed, local index overflow (%d)\n", node->getLocalIndex()+expandedAdjustment);
      return false;
      }
   node->setLocalIndex(node->getLocalIndex() + expandedAdjustment);
   return true;
   }

bool TR_UseDefInfo::childIndexIndicatesImplicitStore(TR::Node *node, int32_t childIndex)
   {
   return false;
   }

void TR_UseDefInfo::insertData(TR::Block *block, TR::Node *node,TR::Node *parent, TR::TreeTop *treeTop, AuxiliaryData &aux, TR::SparseBitVector &expandedLIndexes, bool considerImplicitStores)
   {
   vcount_t visitCount = comp()->getVisitCount();
   if (visitCount == node->getVisitCount())
      return;
   node->setVisitCount(visitCount);

   // Process children
   //
   bool parentCouldHaveImplicitStoreChildren = false;
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      bool shouldConsiderLoadAddrChildrenImplicitStores = parentCouldHaveImplicitStoreChildren && childIndexIndicatesImplicitStore(node, i);
      insertData(block, node->getChild(i), node, treeTop, aux, expandedLIndexes, shouldConsiderLoadAddrChildrenImplicitStores);
      }

   TR::ILOpCode &opCode = node->getOpCode();
   TR::SymbolReference *symRef;
   uint32_t symIndex;
   uint32_t num_aliases;

   if (_useDefForRegs &&
       (opCode.isLoadReg() ||
       opCode.isStoreReg()))
      {
      symRef = NULL;
      num_aliases = 1;
      symIndex = _numSymbols + node->getGlobalRegisterNumber();
      }
   else
      {
      if (!opCode.hasSymbolReference())
         return;

      symRef = node->getSymbolReference();
      if (!symRef)
         return;

      symIndex = symRef->getSymbol()->getLocalIndex();
      num_aliases = aux._numAliases[symRef->getReferenceNumber()];
      }

   uint32_t nodeIndex = node->getUseDefIndex();
   bool definesSingleSymbol = false;
   bool adjustArray = true;
   bool definesMultipleSymbols = false;
   bool restrictRegLoadVar = false;

//   traceMsg(comp(), "For node %p nodeIndex = %d symIndex = %d\n",node,nodeIndex,symIndex);

   if (symIndex != NULL_USEDEF_SYMBOL_INDEX)
      {
      LexicalTimer tlex("insertData_nonNullUseDefSym", comp()->phaseTimer());
      if (parent && parent->getOpCode().isResolveCheck() &&
          num_aliases > 1)
         {
         definesMultipleSymbols = true;
         }
      else if (opCode.isLoadVarDirect() && _hasLoadsAsDefs)
         {
         if (skipAnalyzingForCompileTime(node, block, comp(), aux))
            {
            adjustArray = false;
            }
         if (num_aliases > 1)
            definesMultipleSymbols = true;

         definesSingleSymbol = (definesMultipleSymbols == false);
         }
      else if (isLoadAddrUse(node) ||
               opCode.isLoadVar() ||
               (_useDefForRegs && opCode.isLoadReg()))
         {
         if (!skipAnalyzingForCompileTime(node, block, comp(), aux))
            ;
         else
            {
            adjustArray = false;
            }
         }
      else if (opCode.isCall() || opCode.isFence())
         {
         if (num_aliases)
            {
            definesMultipleSymbols = true;
            }
         else
            return;
         }
      else if (opCode.isStore() ||
               (_useDefForRegs && opCode.isStoreReg()) ||
               (considerImplicitStores && node->getOpCodeValue() == TR::loadaddr))
         {
         if (num_aliases > 1)
            definesMultipleSymbols = true;
         else
            definesSingleSymbol = true;
         }
      else
         return;
      }
   else if (nodeIndex)
      {
      if (opCode.isCall() || opCode.isFence())
         {
         definesMultipleSymbols = true;
         }
      else if (_indexFields)
         {
         TR_ASSERT(false, "error in assignIndex");
         }
      }
   else
      return;

   aux._sideTableToUseDefMap[node->getLocalIndex()] = node->getUseDefIndex();
   _atoms[node->getUseDefIndex()] = std::make_pair(node, treeTop);
   if (!isTrivialUseDefNode(node, aux) && adjustArray)
      aux._expandedAtoms[node->getLocalIndex()] = std::make_pair(node, treeTop);

   if (trace())
      {
      if (!definesMultipleSymbols )
         traceMsg(comp(), "Node : %p   opCode = %s useDefIndex = %d localIndex = %d definesMultipleSymbols=%d isTrivialUseDefNode=%d adjustArray=%d \n", node, opCode.getName(), node->getUseDefIndex(), node->getLocalIndex(), definesMultipleSymbols, isTrivialUseDefNode(node, aux), adjustArray);
      else
         traceMsg(comp(), "Node : %p   opCode = %s useDefIndex = %d localIndex = %d-%d definesMultipleSymbols=%d isTrivialUseDefNode=%d adjustArray=%d \n", node, opCode.getName(), node->getUseDefIndex(), node->getLocalIndex(), node->getLocalIndex()+num_aliases-1, definesMultipleSymbols, isTrivialUseDefNode(node, aux), adjustArray);
      }

   nodeIndex = node->getLocalIndex();

   if (definesMultipleSymbols &&
       !isTrivialUseDefNode(node, aux) && adjustArray)

      {
      LexicalTimer tlex("insertData_multiNonTrivial", comp()->phaseTimer());
      TR::SparseBitVector aliases(comp()->allocator());
      symRef->getUseDefAliases().getAliases(aliases);

      int32_t i = 0;
      int32_t ii = -1; // bit for methods and shadows (if any)

      int32_t j = getMemorySymbolIndex(node);
      if (j != -1)
         {
         int32_t k = nodeIndex;
         i++;

         aux._defsForSymbol[j]->set(k);
         aux._expandedAtoms[k] = std::make_pair(node, treeTop);
         }

      TR::Symbol *callSym = NULL;
      TR::Method *callMethod = NULL;

      TR::SparseBitVector::Cursor aliasesCursor(aliases);
      for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
         {
         TR::SymbolReference *aliasedSymRef = comp()->getSymRefTab()->getSymRef(aliasesCursor);

         if (!aliasedSymRef) continue;
         TR::Symbol *aliasedSym = aliasedSymRef->getSymbol();
         if (!aliasedSym) continue;
         if (excludedGlobals(aliasedSymRef->getSymbol()))
            continue;

         int32_t j = aliasedSymRef->getSymbol()->getLocalIndex();
         if (j == NULL_USEDEF_SYMBOL_INDEX) continue;

         if (aux._neverReferencedSymbols.get(aliasedSymRef->getReferenceNumber()))
            continue;


         // RTC_50153
         if (symRef->getSymbol() != aliasedSymRef->getSymbol() &&
             (symRef->getSymbol()->isAutoOrParm() || symRef->getSymbol()->isStatic()) &&
             (aliasedSymRef->getSymbol()->isAutoOrParm() || aliasedSymRef->getSymbol()->isStatic()))
            continue;

         if (0 && trace())
            traceMsg(comp(), "defines symRef #%d (symbol %d)\n", aliasedSymRef->getReferenceNumber(), j);

         int k;
         if (opCode.isLoadVarDirect())
            {
            k = nodeIndex;
            }
         else if (aliasedSymRef->getSymbol()->isMethod() || aliasedSymRef->getSymbol()->isRegularShadow())
            {
            if (ii == -1)
               {
               ii = i;
               k = nodeIndex + i;
               i++;
               }
            else
               {
               k = nodeIndex + ii;
               }
            }
         else
            {
            k = nodeIndex + i;
            i++;
            }

         if (!restrictRegLoadVar)
            {
            aux._defsForSymbol[j]->set(k);
            if (aux._expandedAtoms[k].first == NULL)
               aux._expandedAtoms[k] = std::make_pair(node, treeTop);
            }

         if (trace())
            traceMsg(comp(), "    symbol (u/d index=%d) is defined by node with localIndex %d \n",j, k);
         }

      if (restrictRegLoadVar)
         {
         aux._defsForSymbol[symIndex]->set(nodeIndex);
         }

      if (node->getOpCode().isStoreDirect())
         {
         // Store to variable should kill another store to that variable completely
         int32_t i;
         int32_t num = num_aliases;
         for (i = 0; i < num; i++)
            {
            aux._defsForSymbol[symRef->getSymbol()->getLocalIndex()]->set(nodeIndex + i);
            aux._expandedAtoms[nodeIndex+i] = std::make_pair(node, treeTop);
            }
         }
      }
#if 0
   else if (definesAllStaticsAndFields &&
         !isTrivialUseDefNode(node, aux) && adjustArray)
      {
      LexicalTimer tlex("insertData_staticsNonTrivial", comp()->phaseTimer());
      for (int32_t i = 0; i < _numStaticsAndFields; ++i)
         {
         aux._defsForSymbol[i].resize(getNumExpandedDefNodes());
         aux._defsForSymbol[i][nodeIndex+i] = true;
         aux._expandedAtoms[nodeIndex+i] = std::make_pair(node, treeTop);
         }
      }
#endif
   else if (definesSingleSymbol &&
            !isTrivialUseDefNode(node, aux) && adjustArray)
      {
      LexicalTimer tlex("insertData_singleNonTrivial", comp()->phaseTimer());
      aux._defsForSymbol[symIndex]->set(nodeIndex);
      }
   else if (considerImplicitStores && node->getOpCodeValue() == TR::loadaddr)
      {
      LexicalTimer tlex("insertData_implicitStore", comp()->phaseTimer());
      aux._defsForSymbol[symIndex]->set(nodeIndex);
      }
   }

bool isSymRefFromInlinedMethod(TR::Compilation *comp, TR::ResolvedMethodSymbol *currentMethod, TR::SymbolReference *symRef)
   {
   while (true)
      {
      TR::ResolvedMethodSymbol *method = symRef->getOwningMethodSymbol(comp);
      if (method == currentMethod)
         return true;

      mcount_t methodIndex = method->getResolvedMethodIndex();
      if (methodIndex <= JITTED_METHOD_INDEX)
         return false;
      symRef = comp->getResolvedMethodSymbolReference(methodIndex);
      if (!symRef)
         return false;
      }
   return false;
   }

void TR_UseDefInfo::processReachingDefinition(void* vblockInfo, AuxiliaryData &aux)
   {
   buildUseDefs(vblockInfo, aux);
   }

void TR_UseDefInfo::buildUseDefs(void *vblockInfo, AuxiliaryData &aux)
   {
   TR::Method *method = comp()->getMethodSymbol()->getMethod();
   TR::Block *block;
   TR::TreeTop *treeTop;
   TR_ReachingDefinitions::ContainerType *analysisInfo = NULL;
   TR_ReachingDefinitions::ContainerType **blockInfo = (TR_ReachingDefinitions::ContainerType **)vblockInfo;

   TR_BitVector nodesToBeDereferenced(getNumUseNodes(), comp()->trMemory()->currentStackRegion());

   comp()->incVisitCount();

   for (treeTop = comp()->getStartTree(); treeTop != NULL; treeTop = treeTop->getNextTreeTop())
      {
      LexicalTimer tlex("buildUseDefs_treeTop", comp()->phaseTimer());
      TR::Node *node = treeTop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();

         if (trace())
            traceMsg(comp(), "\nBuilding use/def info for block %d\n", block->getNumber());

         if (blockInfo)
            {
            analysisInfo = blockInfo[block->getNumber()];
            if (trace())
               {
               traceMsg(comp(), "In set:\n");
               analysisInfo->print(comp());
               traceMsg(comp(), "\n");
               }
            }
         // Cannot skip over processing a BBStart as it may have a child GlRegDeps
         }

      buildUseDefs(node, analysisInfo, nodesToBeDereferenced, NULL, aux);
      }

   if (_indexStatics && blockInfo)
      {
      /* Create exit uses at exit block for statics */
      analysisInfo = blockInfo[_cfg->getEnd()->getNumber()];
      if (trace())
         {
         traceMsg(comp(), "Found exit block_%d", _cfg->getEnd()->getNumber());
         analysisInfo->print(comp());
         traceMsg(comp(), " Use index %d\n", getLastUseIndex()-getFirstUseIndex());
         traceMsg(comp(), "\n");
         }

      int32_t i, ii;
      TR::Method *method = comp()->getMethodSymbol()->getMethod();
      TR_BitVectorIterator bvi(*analysisInfo);
      while (bvi.hasMoreElements())
         {
         // Convert from expanded index to normal index
         //
         i = bvi.getNextElement();
         bool externalAutoParm = false;

         if (i >= _numDefsOnEntry)
            {
            TR::Node *defNode = aux._expandedAtoms[i].first;
            if (defNode == NULL) continue;

            ii = i;
            i = defNode->getUseDefIndex();

            if (i == 0 || i > getLastDefIndex())
               continue;

            if (!(getNode(i)->getOpCode().hasSymbolReference() && getNode(i)->getSymbolReference()->getSymbol()->isStatic())
                  && !externalAutoParm)
               continue;
            }
         else if (!_uniqueIndexForDefsOnEntry)
            {
            i = 0;
            }
         //         traceMsg(comp(), "UDI: setting _useDefInfo[%d}[i=%d] to true getLastUseIndex = %d getFirstUseIndex = %d numberOfNodes = %d\n",getLastUseIndex()-getFirstUseIndex(),i,getLastUseIndex(),getFirstUseIndex(),getNumUseNodes());
         _useDefInfo[getLastUseIndex() - getFirstUseIndex()][i] = true;
         }
      }

   if (_hasLoadsAsDefs)
      {
      _useDerefDefInfo.resize(getNumUseNodes(), NULL);

      // Go through any use nodes whose use/def info must be dereferenced.
      //
      // During use/def processing defs are factored, i.e. a load is considered a
      // def point for subsequent loads of the same symbol.
      // This is only maintained if the defining load is the only def node for the
      // use - otherwise any defining loads are dereferenced back to real def nodes
      //
      TR_UseDefInfo::BitVector nodesLookedAt(comp()->allocator());
      TR_UseDefInfo::BitVector loadDefs(comp()->allocator());
      TR_BitVectorIterator cursor(nodesToBeDereferenced);
      while (cursor.hasMoreElements())
         {
         int32_t useIndex = cursor.getNextElement();
         if (getNode(useIndex + getFirstUseIndex())->getSymbolReference()->getSymbol()->isMethod())
            continue;
         dereferenceDefs(useIndex, nodesLookedAt, loadDefs);
         }
      }

   if (trace())
      {
      traceMsg(comp(), "\nUse/Def info: firstDefIndex=%d lastDefIndex=%d firstUseIndex=%d lastUseIndex=%d getNumDefsOnEntry()=%d\n",
            getFirstDefIndex(), getLastDefIndex(), getFirstUseIndex(), getLastUseIndex(), getNumDefsOnEntry());
      }

   int32_t lastUseIndex = getLastUseIndex();
   for (int32_t j = getFirstUseIndex(); j <= lastUseIndex; j++)
      {
      TR_UseDefInfo::BitVector &info = _useDefInfo[j - getFirstUseIndex()];

      TR::Node *node = getNode(j);

      if (info.IsZero() && j != lastUseIndex)
         {
         dumpOptDetails(comp(), "   Use #%d[%p] has no definitions\n",j,getNode(j));

         if (!getNode(j)->getOpCode().isLoadReg() && !node->getOpCode().isFence())
            TR_ASSERT(false, "No defs for a use [0x%p]", getNode(j));

         }
      if (trace())
         {
         // I haven't narrowed down the problem but this is basically the same assert as the
         // one above, not guarded by trace().  It looks like that one has been adjusted without
         // a corresponding adjustment here.  So, I'm just taking this one out.
         //if (info.IsZero() && j != lastUseIndex && !node->getOpCode().isFence())
         //  TR_ASSERT(false, "No defs for a use\n");
         //traceMsg(comp(), "No defs for a use\n");

         traceMsg(comp(), "   Use #%d[%p] is defined by:\n",j,getNode(j));
         TR_UseDefInfo::BitVector::Cursor cursor(info);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t defIndex = cursor;
            if (defIndex >= getFirstUseIndex())
               traceMsg(comp(), "      Single defining load #%d[%p]",defIndex,getNode(defIndex));
            else
               traceMsg(comp(), "      Def #%d[%p]",defIndex,getNode(defIndex));
            if (defIndex < getNumDefsOnEntry())
               traceMsg(comp(), " (from method entry) ");
            traceMsg(comp(), "\n");
            }
         }
      }

   }

void TR_UseDefInfo::dereferenceDefs(int32_t useIndex, TR_UseDefInfo::BitVector &nodesLookedAt, TR_UseDefInfo::BitVector &loadDefs)
   {
   if (trace())
      {
      traceMsg(comp(), "Dereferencing defs for use index %d : ",useIndex+getFirstUseIndex());
      (*comp()) << _useDefInfo[useIndex];
      traceMsg(comp(), "\n");
      }

   // First see if all the def nodes are loads that all have a single defining
   // load. If so, set that up as the single defining load for this node.
   //
   TR_UseDefInfo::BitVector &useDefInfo = _useDefInfo[useIndex];
   nodesLookedAt.Clear();
   loadDefs.Clear();
   int32_t commonLoad = setSingleDefiningLoad(useIndex, nodesLookedAt, loadDefs);

   if (!loadDefs.IsZero())
      {
      useDefInfo.Clear();
      useDefInfo |= loadDefs;
      if (trace())
         {
         //traceMsg(comp(), "      Changing use index %d to have single defining load %d\n", useIndex+getFirstUseIndex(), commonLoad);
         traceMsg(comp(), "      Changing use index %d to have defining loads : \n", useIndex+getFirstUseIndex());
         (*comp()) << loadDefs;
         traceMsg(comp(), "\n");
         }
      }
   // Otherwise, for each def node that is also a use find its real defs and
   // substitute them into the useDefInfo.
   //
   else
      {
      nodesLookedAt.Clear();
      nodesLookedAt[useIndex] = true;
      useDefInfo[useIndex + getFirstUseIndex()] = false;

      for (int32_t i = getFirstUseIndex(); i < getNumDefNodes(); ++i)
         {
         if (useDefInfo.ValueAt(i))
            dereferenceDef(useDefInfo, i, nodesLookedAt);
         }

      // save dereferenced info
      if (_hasLoadsAsDefs)
         _useDerefDefInfo[useIndex] = &useDefInfo;
      }

   if (trace())
      {
      traceMsg(comp(), "New defs for use index %d : ",useIndex+getFirstUseIndex());
      (*comp()) << _useDefInfo[useIndex];
      traceMsg(comp(), "\n");
      }
   }

// See if there is a single defining load for the given use.
// Return the def index of the common load if there is one.
//
// Return -1 if all the defs are uses that define each other (i.e. complete
// circularity)
//
// Return -2 if there is definitely not a single defining load
//
int32_t TR_UseDefInfo::setSingleDefiningLoad(int32_t useIndex, TR_UseDefInfo::BitVector &nodesLookedAt, TR_UseDefInfo::BitVector &loadDefs)
   {
   TR_UseDefInfo::BitVector &useDefInfo = _useDefInfo[useIndex];
   nodesLookedAt[useIndex] = true;

   if (useDefInfo.IsZero())
      return -2;

   TR_UseDefInfo::BitVector::Cursor cursor(useDefInfo);

   cursor.SetToFirstOne();
   int32_t defIndex = cursor;

   if (trace())
      {
      traceMsg(comp(), "   Checking use index %d for single defining load : ",useIndex+getFirstUseIndex());
      (*comp()) << useDefInfo;
      traceMsg(comp(), "\n");
      }

   // If all defs are not loads, there can't be a single defining load
   //
   if ((defIndex < getFirstUseIndex()) ||
       !getNode(defIndex)->getOpCode().isLoadVar())
      return -2;

   // If this is the only def node, it is either common defining load or there
   // is a circularity.
   //
   if (0 && !cursor.Valid())
      {
      if (nodesLookedAt.ValueAt(defIndex - getFirstUseIndex()))
         {
         if (trace())
            traceMsg(comp(), "      Use index %d has circular defining loads\n", useIndex+getFirstUseIndex());
         return -1;
         }
      if (trace())
         traceMsg(comp(), "      Use index %d has single defining load %d\n", useIndex+getFirstUseIndex(), defIndex);

      //loadDefs.set(defIndex);
      return defIndex;
      }

   // All the defs must have a single common def. If they do, it is the common
   // defining load for this use.
   //
   int32_t commonDef = -1;
   for (; cursor.Valid(); cursor.SetToNextOne())
      {
      defIndex = cursor;
      defIndex -= getFirstUseIndex();

      // Ignore circularities - if there is a single defining load except for
      // circularities then the single defining load applies to all the
      // circular defs.
      //
      if (!nodesLookedAt.ValueAt(defIndex))
         {
         int32_t singleDef = setSingleDefiningLoad(defIndex, nodesLookedAt, loadDefs);
         if (singleDef == -2)
            {
            loadDefs[defIndex + getFirstUseIndex()] = true;
            if (trace())
               traceMsg(comp(), "      Use index %d has defining load %d\n", useIndex+getFirstUseIndex(), defIndex+getFirstUseIndex());

            //return -2;
            }
         if (singleDef >= 0)
            {
            commonDef = singleDef;

            //if (commonDef < 0)
            //   commonDef = singleDef;
            //else if (commonDef != singleDef)
            //   return -2;
            }
         }
      }

   if (0 && trace())
      {
      if (commonDef >= 0)
         traceMsg(comp(), "      Use index %d has single defining load %d\n", useIndex+getFirstUseIndex(), commonDef);
      else
         traceMsg(comp(), "      Use index %d has circular defining loads\n", useIndex+getFirstUseIndex());
      }

   return commonDef;
   }

/*
 void TR_UseDefInfo::dereferenceDef(TR_BitVector *useDefInfo, int32_t defIndex, TR_BitVector &nodesLookedAt)
 {
 int32_t useIndex = defIndex - getFirstUseIndex();

 if (trace())
 {
 traceMsg(comp(), "   De-referencing use index %d : ",defIndex);
 useDefInfo->print(comp());
 traceMsg(comp(), "\n");
 }

 if (nodesLookedAt.get(useIndex))
 return;
 nodesLookedAt.set(useIndex);

 if (trace())
 traceMsg(comp(), "      Resetting def index %d\n", defIndex);
 useDefInfo->reset(defIndex);

 TR_BitVector *myDefs = _useDefInfo[useIndex];
 TR_BitVectorIterator bvi(*myDefs);
 while (bvi.hasMoreElements())
 {
 int32_t myDef = bvi.getNextElement();
 if (myDef < getFirstUseIndex())
 {
 if (trace())
 traceMsg(comp(), "      Setting def index %d\n", myDef);
 useDefInfo->set(myDef);
 }
 else
 dereferenceDef(useDefInfo, myDef, nodesLookedAt);
 }
 }
 */

/**
 * Note that this method is a non-recursive version of the original
 * recursive implementation above; the method was made non recursive
 * to avoid potential problems with deep stacks (that may overflow);
 * in particular a JCK test exposed a pblm with going up single
 * defining uses in a large basic block with no commoning done yet.
 * As a consequence many different loads of the same symbol all exist
 * in the same block
 */
void TR_UseDefInfo::dereferenceDef(TR_UseDefInfo::BitVector &useDefInfo, int32_t defIndex, TR_UseDefInfo::BitVector &nodesLookedAt)
   {
   TR_ASSERT(defIndex < getTotalNodes(), "TR_UseDefInfo::getNode index(%d) is bigger than total(%d)\n", defIndex, getTotalNodes());

   TR::list<std::pair<TR::Node *,TR::TreeTop *>, TR::Region&> defIndices(_region);
   defIndices.push_front(_atoms[defIndex]);
   nodesLookedAt[defIndex - getFirstUseIndex()] = true;

   while (!defIndices.empty())
      {
      defIndex = defIndices.front().first->getUseDefIndex();
      defIndices.pop_front();
      int32_t useIndex = defIndex - getFirstUseIndex();

      if (getNode(defIndex)->getSymbolReference()->getSymbol()->isMethod() ||
          getNode(defIndex)->getOpCode().isCall())
         {
         useDefInfo[defIndex] = true;
         continue;
         }

      if (trace())
         {
         traceMsg(comp(), "   De-referencing use index %d : ",defIndex);
         (*comp()) << useDefInfo;
         traceMsg(comp(), "\n");
         }

         {
         if (trace())
            traceMsg(comp(), "      Resetting def index %d\n", defIndex);
         useDefInfo[defIndex] = false;

         if (_hasLoadsAsDefs && _useDerefDefInfo[useIndex])
            {
            useDefInfo |= *_useDerefDefInfo[useIndex];
            continue;
            }

         TR_UseDefInfo::BitVector &myDefs = _useDefInfo[useIndex];
         TR_UseDefInfo::BitVector::Cursor cursor(myDefs);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t myDef = cursor;
            if (myDef < getFirstUseIndex() ||
                getNode(myDef)->getSymbolReference()->getSymbol()->isMethod())
               {
               if (trace())
                  traceMsg(comp(), "      Setting def index %d\n", myDef);
               useDefInfo[myDef] = true;
               }
            else if (!nodesLookedAt.ValueAt(myDef - getFirstUseIndex()))
               {
               if (trace())
                  traceMsg(comp(), "      Adding def index %d\n", myDef);
               defIndices.push_front(_atoms[myDef]);
               nodesLookedAt[myDef - getFirstUseIndex()] = true;
               }
            }
         }
      }
   }


// PLX only
bool TR_UseDefInfo::isChildUse(TR::Node* node, int32_t childIndex)
   {
   TR_ASSERT(childIndex < node->getNumChildren(), "Bad child index");
   return true;
   }

void TR_UseDefInfo::buildUseDefs(TR::Node *node, void *vanalysisInfo, TR_BitVector &nodesToBeDereferenced, TR::Node *parent, AuxiliaryData &aux)
   {
   vcount_t visitCount = comp()->getVisitCount();
   if (node->getVisitCount() == visitCount)
      return;
   if (trace())
      traceMsg(comp(), "looking at node %p\n", node);

   node->setVisitCount(visitCount);
   TR_ReachingDefinitions::ContainerType *analysisInfo = (TR_ReachingDefinitions::ContainerType *)vanalysisInfo;

   // Look in the children first.
   //
   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      buildUseDefs(node->getChild(i), analysisInfo, nodesToBeDereferenced, node, aux);
      }

   TR::Method *method = comp()->getMethodSymbol()->getMethod();
   uint32_t nodeIndex = node->getUseDefIndex();
   if (node->getOpCode().hasSymbolReference() &&
       isTrivialUseDefNode(node, aux) )
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *sym = symRef->getSymbol();
      bool defsKnownTrivially = false;
      if (node->getOpCode().isLoadVarDirect() ||
          (isLoadAddrUse(node)))
         {
         int32_t realIndex = nodeIndex - getFirstUseIndex();
         if (trace())
            traceMsg(comp(), "For node %p index = %d and first use index = %d\n", node, nodeIndex, getFirstUseIndex());
         if (aux._neverWrittenSymbols.get(node->getSymbolReference()->getReferenceNumber()))
            {
            TR_ASSERT(realIndex >= 0, "Out of bounds negative index, realIndex = %d\n", realIndex);
            //           traceMsg(comp(), "UDI: setting _useDefInfo[realIndex=%d][0] to true\n",realIndex);
            _useDefInfo[realIndex][0] = true;
            defsKnownTrivially = true;
            if (trace())
               traceMsg(comp(), "Reached here (entry) for use node %p\n", node);
            }
         else if (!aux._onceReadSymbolsIndices[node->getSymbolReference()->getReferenceNumber()].IsNull())
            {
            if (trace())
               traceMsg(comp(), "Use node %p is of a symbol read only once\n", node);

            TR_ASSERT(realIndex >= 0, "Out of bounds negative index, realIndex = %d\n", realIndex);
            //            traceMsg(comp(), "UDI: oring in _useDefInfo[realIndex=%d] with ",realIndex);
            //           *(comp()) << aux._onceReadSymbols[node->getSymbolReference()->getReferenceNumber()];
            if (aux._onceReadSymbols[node->getSymbolReference()->getReferenceNumber()])
               _useDefInfo[realIndex] |= CS2_TR_BitVector(*(aux._onceReadSymbols[node->getSymbolReference()->getReferenceNumber()]));
            defsKnownTrivially = true;

            }
         else if (!aux._onceWrittenSymbolsIndices[node->getSymbolReference()->getReferenceNumber()].IsNull())
            {
            if (trace())
               traceMsg(comp(), "Use node %p is of a symbol write only once\n", node);

            TR_ASSERT(realIndex >= 0, "Out of bounds negative index, realIndex = %d\n", realIndex);
            //           traceMsg(comp(), "UDI: oring2 in _useDefInfo[realIndex=%d] with ",realIndex);
            //           *(comp()) << aux._onceWrittenSymbols[node->getSymbolReference()->getReferenceNumber()];
            if (aux._onceWrittenSymbols[node->getSymbolReference()->getReferenceNumber()])
               _useDefInfo[realIndex] |= CS2_TR_BitVector(*(aux._onceWrittenSymbols[node->getSymbolReference()->getReferenceNumber()]));
            defsKnownTrivially = true;
            }
         else
            {
            //if (_onceWrittenSymbols[node->getSymbolReference()->getReferenceNumber()])
            //   {
            //   if (trace())
            //      dumpOptDetails(comp(), "Use node %p is of a symbol written only once\n", node);

            //   TR::TreeTop *storeTree = _onceWrittenSymbols[node->getSymbolReference()->getReferenceNumber()];
            //   TR::Node *storeNode = storeTree->getNode();
            //   if (!storeNode->getOpCode().isStore() &&
            //       (storeNode->getNumChildren() > 0))
            //       storeNode = storeNode->getFirstChild();

            //   if (trace())
            //      dumpOptDetails(comp(), "Lone store node is %p\n", storeNode);

            //   TR_ASSERT(realIndex >= 0, "Out of bounds negative index, realIndex = %d\n", realIndex);
            //   _useDefInfo[realIndex][storeNode->getUseDefIndex()] = true;
            //    }
            }
         }
      if (_hasLoadsAsDefs || defsKnownTrivially)
         return;
      }

   if (!nodeIndex)
      return;

   TR::ILOpCode &opCode = node->getOpCode();
   TR::Symbol *sym;
   uint32_t symIndex;
   int32_t memSymIndex = -1;
   uint32_t num_aliases;

   if (_useDefForRegs &&
       (opCode.isLoadReg() ||
       opCode.isStoreReg()))
      {
      sym = NULL;
      symIndex = _numSymbols + node->getGlobalRegisterNumber();
      num_aliases = 1;
      }
   else
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      sym = symRef->getSymbol();
      symIndex = sym->getLocalIndex();
      memSymIndex = getMemorySymbolIndex(node);
      num_aliases = aux._numAliases[symRef->getReferenceNumber()];
      }

   scount_t expandedNodeIndex = node->getLocalIndex();

   // If this node is a use node set up its def information.
   // If it has more than one def node and some of them are use nodes too,
   // they will have to be later dereferenced to their real def nodes.
   //
   if (isExpandedUseIndex(expandedNodeIndex) ||
       ((expandedNodeIndex == 0) &&
         (!node->getOpCode().isStore() ||
          !node->getSymbol()->isAutoOrParm() ||
          !node->storedValueIsIrrelevant())))
      {
      LexicalTimer tlex("buildUseDefs_setUpDef", comp()->phaseTimer());
      TR_ASSERT(symIndex != NULL_USEDEF_SYMBOL_INDEX, "Use node %p should have an indexed symbol", node);
      int32_t numDefs = 0;
      bool hasUseAsDef = false;
      int32_t realIndex = nodeIndex - getFirstUseIndex();

      if (!aux._defsForSymbol[symIndex]->isEmpty())
         {
         // Use the temporary work BitVector
         TR_BitVector *defs = &(aux._workBitVector);
         defs->empty();

         // assume aux._defsForSymbol[symIndex] is never NULL if _possibleDefs is non-NULL
         if (trace())
            {
            traceMsg(comp(), "defs for symbol %d node:%p \n", symIndex, node);
            aux._defsForSymbol[symIndex]->print(comp());
            traceMsg(comp(), "\n");
            }

         *defs = *(aux._defsForSymbol[symIndex]);

         if (memSymIndex != -1 && !aux._defsForSymbol[memSymIndex]->isEmpty())
            {
            if (trace())
               {
               traceMsg(comp(), "defs for memory symbol %d \n", memSymIndex);
               aux._defsForSymbol[memSymIndex]->print(comp());
               traceMsg(comp(), "\n");
               }
            *defs |= *(aux._defsForSymbol[memSymIndex]);
            }

         if (analysisInfo)
            *defs &= *analysisInfo;

         bool ignoreDefsOnEntry = false;
         if (memSymIndex != -1 && !defs->get(memSymIndex))
            ignoreDefsOnEntry = true;

#if 0
         if (trace())
            {
            traceMsg(comp(), " reaching defs for symbol %d at node:%p ignoreDefsOnEntry=%d \n", symIndex, node, ignoreDefsOnEntry);
            (*comp()) << aux._defsForSymbol[symIndex];
            traceMsg(comp(), "\n");
            }
#endif

         TR::Method *method = comp()->getMethodSymbol()->getMethod();

         TR_BitVectorIterator cursor(*defs);
         while (cursor.hasMoreElements())
            {
            i = cursor.getNextElement();
            // Convert from expanded index to normal index
            //
            if (i >= _numDefsOnEntry)
               {
               TR::Node *defNode = aux._expandedAtoms[i].first;
               //verifySnapshots(comp(), node, defNode);
               if (trace())
                  traceMsg(comp(), "reached by expanded index %d [0x%p]\n", i, defNode);


               auto j = defNode->getUseDefIndex();
               if (isUseIndex(j))
                  hasUseAsDef = true;

               if (memSymIndex == -1 ||
                   getMemorySymbolIndex(defNode) != memSymIndex ||
                   i == defNode->getLocalIndex())
                  {
                  //              traceMsg(comp(), "UDI: setting _useDefInfo[realIndex=%d][j=%d] to true\n",realIndex,j);
                  _useDefInfo[realIndex][j] = true;
                  }
               }
            else if (!_uniqueIndexForDefsOnEntry && !ignoreDefsOnEntry)
               {
               //          traceMsg(comp(), "UDI: setting _useDefInfo[realIndex=%d][0] to true\n",realIndex);
               _useDefInfo[realIndex][0] = true;
               }
            else if (!ignoreDefsOnEntry)
               {
               //          traceMsg(comp(), "UDI: setting _useDefInfo[realIndex=%d][i=%d] to true\n",realIndex,i);
               _useDefInfo[realIndex][i] = true;
               }
            TR_ASSERT(realIndex >= 0, "realIndex is negative (%d - %d)", nodeIndex, getFirstUseIndex() );
            ++numDefs;
            }
         }

      // If this is a loadaddr with no defs, it is a loadaddr for a local. This
      // does not have to be dominated by stores, so just make the method entry
      // a def for it so that it has one.
      //
      if (numDefs == 0)
         {
         if (isLoadAddrUse(node))
            {
            //        traceMsg(comp(), "UDI: setting _useDefInfo[realIndex=%d][0] to true because of loadaddr\n",realIndex);
            _useDefInfo[realIndex][0] = true;
            }
         }

      // Remember if this node's use/def info must be dereferenced
      //
      else if (numDefs > 1 && hasUseAsDef)
         {
         if ( ! node->getOpCode().isCall() )
            nodesToBeDereferenced.set(realIndex);
         }
      }

   // Update the analysis info
   //
   if (analysisInfo == NULL)
      return; // no analysisInfo to update
   int32_t numDefNodes;

   if (symIndex == NULL_USEDEF_SYMBOL_INDEX || node->getOpCode().isCall() || node->getOpCode().isFence()  ||
         (parent && parent->getOpCode().isResolveCheck() && num_aliases > 1))
      {
      // A call or unresolved reference is a definition of all
      // symbols it is aliased with
      //
      numDefNodes = num_aliases;
      }
   else if (isExpandedUseDefIndex(expandedNodeIndex))
      {
      // load of a symbol defines only symbol itself
      //
      numDefNodes = 1;
      if (!aux._defsForSymbol[symIndex]->isEmpty() &&
          (!sym ||
          (!sym->isShadow() &&
          !sym->isMethod())))
         {
            {
            *analysisInfo -= *(aux._defsForSymbol[symIndex]);
            }
         }

      if (node->getOpCode().isStoreIndirect())
         {
         int32_t symIndex = getMemorySymbolIndex(node);
         if (symIndex != -1 && !aux._defsForSymbol[symIndex]->isEmpty())
            *analysisInfo -= *(aux._defsForSymbol[symIndex]);
         }
      }
   else if (isExpandedDefIndex(expandedNodeIndex))
      {
      // Store to a symbol is a definition of all symbols it is aliased with
      //
      numDefNodes = num_aliases;
      if (!aux._defsForSymbol[symIndex]->isEmpty() &&
          (!sym ||
          (!sym->isShadow() &&
            !sym->isMethod())))
         {
            {
            *analysisInfo -= *(aux._defsForSymbol[symIndex]);
            }
         }

      if (node->getOpCode().isStoreIndirect())
         {
         int32_t symIndex = getMemorySymbolIndex(node);
         if (symIndex != -1 && !aux._defsForSymbol[symIndex]->isEmpty())
            *analysisInfo -= *(aux._defsForSymbol[symIndex]);
         }
      }
   else
      {
      numDefNodes = 0;
      }

   for (i = 0; i < numDefNodes; ++i)
      analysisInfo->set(expandedNodeIndex + i);

#if 0
   if (trace())
      {
      traceMsg(comp(), "updated reaching def:\n");
      analysisInfo->print(comp());
      traceMsg(comp(), "\n");
      }
#endif
   }

TR::Node *TR_UseDefInfo::getSingleDefiningLoad(TR::Node *node)
   {
   TR_UseDefInfo::BitVector &info = _useDefInfo[node->getUseDefIndex()-getFirstUseIndex()];
   if (info.PopulationCount() == 1)
      {
      TR_UseDefInfo::BitVector::Cursor cursor(info);
      TR::Node *n = 0;

      cursor.SetToFirstOne();
      int32_t firstDef = cursor;
      if ((firstDef >= getFirstUseIndex()) &&
          (n = getNode(firstDef)) &&
          (n->getUseDefIndex() > 0) &&
          (n->getOpCode().isLoadVar()) &&
          (n->getOpCode().hasSymbolReference() && !n->getSymbol()->isVolatile()))
         return n;
      }
   return NULL;
   }

bool TR_UseDefInfo::getDefiningLoads(BitVector &definingLoads, TR::Node *node)
   {
   definingLoads.Or(getDefiningLoads_ref(node));
   return !definingLoads.IsZero();
   }

const TR_UseDefInfo::BitVector &TR_UseDefInfo::getDefiningLoads_ref(TR::Node *node)
   {
   TR_UseDefInfo::BitVector &info = _useDefInfo[node->getUseDefIndex()-getFirstUseIndex()];
   if (!info.IsZero())
      {
      TR_UseDefInfo::BitVector::Cursor cursor(info);
      cursor.SetToFirstOne();
      int32_t useAsDef = cursor;
      if (useAsDef < getFirstUseIndex())
         return _EMPTY;
      TR_UseDefInfo::BitVector temp1(comp()->allocator());
      const TR_UseDefInfo::BitVector &useDefTemp1 = getUseDef_ref(useAsDef, &temp1);
      for (cursor.SetToNextOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         useAsDef = cursor;
         if (useAsDef < getFirstUseIndex())
            return _EMPTY;

         TR_UseDefInfo::BitVector temp2(comp()->allocator());
         const TR_UseDefInfo::BitVector &useDefTemp2 = getUseDef_ref(useAsDef, &temp2);
         if (!(useDefTemp1 == useDefTemp2))
            return _EMPTY;
         }
      return info;
      }
   return _EMPTY;
   }

bool TR_UseDefInfo::getUseDefIsZero(int32_t useIndex)
   {
   const TR_UseDefInfo::BitVector &useDef = getUseDef_ref(useIndex);
   return useDef.IsZero();
   }

bool TR_UseDefInfo::getUseDef(BitVector &useDef, int32_t useIndex)
   {
   useDef.Or(getUseDef_ref(useIndex));
   return !useDef.IsZero();
   }

bool TR_UseDefInfo::getUseDef_noExpansion(BitVector &useDef, int32_t useIndex)
   {
   useDef.Or(_useDefInfo[useIndex - getFirstUseIndex()]);
   return !useDef.IsZero();
   }

const TR_UseDefInfo::BitVector &TR_UseDefInfo::getUseDef_ref(int32_t useIndex, TR_UseDefInfo::BitVector *defs)
   {
   _defsChecklist->empty();
   return getUseDef_ref_body(useIndex, _defsChecklist, defs);
   }

const TR_UseDefInfo::BitVector & TR_UseDefInfo::getUseDef_ref_body(int32_t useIndex, TR_BitVector *visitedDefs, TR_UseDefInfo::BitVector *defs)
   {
   TR_ASSERT(isUseIndex(useIndex), "useIndex is invalid");
   bool save = (defs == NULL);

   if (visitedDefs->get(useIndex))
      return _EMPTY;
   visitedDefs->set(useIndex);

   //   traceMsg(comp(), "UDI: getUseDef_ref for useIndex %d defs = ",useIndex);
   //   if(defs)
   //      *(comp()) << *defs;
   //   traceMsg(comp(), "\n");

   if (_hasLoadsAsDefs && _useDerefDefInfo[useIndex - getFirstUseIndex()])
      {
      //     traceMsg(comp(), "UDI: _hasLoadsAsDefs is true\n");
      if (defs)
         {
         *defs |= *_useDerefDefInfo[useIndex - getFirstUseIndex()];
         return *defs;
         }
      else
         {
         return *_useDerefDefInfo[useIndex - getFirstUseIndex()];
         }
      }

   TR_UseDefInfo::BitVector &info = _useDefInfo[useIndex - getFirstUseIndex()];

   //   traceMsg(comp(), "UDI: info =");
   //   *(comp()) << info;
   //  traceMsg(comp(), "\n");

   if (!info.IsZero())
      {
      TR_UseDefInfo::BitVector::Cursor cursor(info);
      int32_t firstDef = -1;
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         // Convert from expanded index to normal index
         //
         int32_t i = cursor;
         //        traceMsg(comp(), "UDI: cursor = %d\n",i);
         if (firstDef < 0)
            {
            firstDef = i;

            if ((firstDef < getFirstUseIndex()) ||
                (getNode(useIndex) &&
                getNode(useIndex)->getSymbolReference() &&
                getNode(useIndex)->getSymbolReference()->getSymbol()->isMethod()))
               {
               //               traceMsg(comp(), "UDI, special case firstDef = %d getFirstUseIndex() = %d getNode(useIndex) = %p\n",firstDef,getFirstUseIndex(),getNode(useIndex));
               if (!defs)
                  {
                  if (_hasLoadsAsDefs)
                     _useDerefDefInfo[useIndex - getFirstUseIndex()] = &info;
                  return info;
                  }
               else
                  {
                  *defs |= info;
                  return *defs;
                  }
               }
            }

         if (!(info.PopulationCount() > 1))
            {
            //           traceMsg(comp(), "UDI: population count less than 1\n");
            if (!defs)
               {
               const TR_UseDefInfo::BitVector &info = getUseDef_ref_body(i, visitedDefs);
               if (_hasLoadsAsDefs)
                  _useDerefDefInfo[useIndex - getFirstUseIndex()] = &info;
               return info;
               }
            else
               {
               getUseDef_ref_body(i, visitedDefs, defs);
               //return *defs;
               }
            }
         else
            {
            if (!defs)
               {
               //              traceMsg(comp(), "UDI: caching bit vector 2\n");
               // cache bit vector by copy construction, and get reference to it
               _infoCache.push_back(TR_UseDefInfo::BitVector(comp()->allocator()));
               defs = &(_infoCache.back());
               }
            getUseDef_ref_body(i, visitedDefs, defs);
            }
         }
      }

   if (save && _hasLoadsAsDefs)
      _useDerefDefInfo[useIndex - getFirstUseIndex()] = defs;

   if (!defs)
      return _EMPTY;

   return *defs;
   }

bool TR_UseDefInfo::getUsesFromDefIsZero(int32_t defIndex, bool loadAsDef)
   {
   const TR_UseDefInfo::BitVector &usesFromDef = getUsesFromDef_ref(defIndex, loadAsDef);
   return usesFromDef.IsZero();
   }

bool TR_UseDefInfo::getUsesFromDef(BitVector &usesFromDef, int32_t defIndex, bool loadAsDef)
   {
   usesFromDef.Or(getUsesFromDef_ref(defIndex, loadAsDef));
   return !usesFromDef.IsZero();
   }

const TR_UseDefInfo::BitVector &TR_UseDefInfo::getUsesFromDef_ref(int32_t defIndex, bool loadAsDef)
   {
   if ((_defUseInfo.size() > 0) && !loadAsDef)
      {
      //      traceMsg(comp(), "UDI: returning _defUseInfo at index %d numberOfElements = %d\n_defUseInfo = ",defIndex,_defUseInfo.NumberOfElements());
      //      *(comp()) << _defUseInfo;
      return _defUseInfo[defIndex];
      }
   else if ((_loadDefUseInfo.size() > 0) && loadAsDef)
      {
      //      traceMsg(comp(), "UDI: returning _loadDefUseInfo at index %d\n",defIndex);
      return _loadDefUseInfo[defIndex];
      }
   else
      {
      int32_t i;
      TR_UseDefInfo::BitVector *usesForDef = NULL;
      for (i = getNumUseNodes() - 1; i >= 0; --i)
         {
         int32_t useIndex = i + getFirstUseIndex();
         if (getNode(useIndex) == NULL)
            continue;
         bool isDef = false;
         if (loadAsDef)
            {
            const TR_UseDefInfo::BitVector &defs = _useDefInfo[useIndex-getFirstUseIndex()];
            if (defs.ValueAt(defIndex))
               isDef = true;
            }
         else
            {
            const TR_UseDefInfo::BitVector &defs = getUseDef_ref(useIndex);
            if (defs.ValueAt(defIndex))
               isDef = true;
            }
         if (isDef)
            {
            if (!usesForDef)
               {
               //              traceMsg(comp(), "UDI: Adding to infocache\n");
               _infoCache.push_back(TR_UseDefInfo::BitVector(comp()->allocator()));
               usesForDef = &(_infoCache.back());
               }
            (*usesForDef)[i] = true;
            }
         }
      return (usesForDef) ? *usesForDef : _EMPTY;
      }
   }

void TR_UseDefInfo::setUseDef(int32_t useIndex, int32_t defIndex)
   {
   int32_t realIndex = useIndex - getFirstUseIndex();
   _useDefInfo[realIndex][defIndex] = true;

   //   traceMsg(comp(), "UDI: setUseDef _useDefInfo[realIndex=%d][defIndex=%d] to true\n",realIndex,defIndex);


   if (_hasLoadsAsDefs && _useDerefDefInfo[realIndex])
      {
      // _useDerefDefInfo[realIndex]->set(defIndex);
      _useDerefDefInfo[realIndex] = NULL;
      }
   }

void TR_UseDefInfo::resetUseDef(int32_t useIndex, int32_t defIndex)
   {
   int32_t realIndex = useIndex - getFirstUseIndex();
   _useDefInfo[realIndex][defIndex] = false;

   if (_hasLoadsAsDefs && _useDerefDefInfo[realIndex])
      _useDerefDefInfo[realIndex] = NULL;
   }

void TR_UseDefInfo::clearUseDef(int32_t useIndex)
   {
   int32_t realIndex = useIndex - getFirstUseIndex();
   _useDefInfo[realIndex].Clear();

   if (_hasLoadsAsDefs && _useDerefDefInfo[realIndex])
      {
      _useDerefDefInfo[realIndex] = NULL;
      }
   }

TR::Node *TR_UseDefInfo::getNode(int32_t index)
   {
   TR_ASSERT(index < getTotalNodes(), "TR_UseDefInfo::getNode index(%d) is bigger than total(%d)\n", index, getTotalNodes());

   TR_UseDef *ud = &_useDefs[index];
   if (ud->isDef())
      {
      TR::Node *node = ud->getDef()->getNode();
      // Sometimes a DEF node is removed (by, for example, ValueNumbering)
      // When that happens, it may not have any children. However, we must
      // still return the first child to ensure functional correctness.
      if ((node->getOpCode().isCheck() || node->getOpCodeValue() == TR::treetop))
         {
         // Set numChildren of the node to 1 so that it passes the assertion in getFirstChild
         // But we must save and restore the number of children here, because if VP deletes unreachable code,
         // it may expect the number of children to be set to zero.
         //
         int32_t oldNumChildren = node->getNumChildren();
         node->setNumChildren(1);
         TR::Node *child = node->getFirstChild();
         node->setNumChildren(oldNumChildren);
         node = child;
         }

      return node;
      }
   return ud->getUseDef();
   }

TR::TreeTop *TR_UseDefInfo::getTreeTop(int32_t index)
   {
   TR_UseDef *ud = &_useDefs[index];
   if (!ud->isDef())
      {
      TR_ASSERT(ud->getUseDef() == NULL, "assertion failure");
      return NULL;
      }
   return ud->getDef();
   }

void TR_UseDefInfo::buildDefUseInfo(bool loadAsDef)
   {
   LexicalTimer tlex("buildDefUseInfo", comp()->phaseTimer());

   // Build def/use info from the use/def info
   //
   //We build uses for real defs (stores) only.
   if ((_defUseInfo.size() > 0) &&
       ((_loadDefUseInfo.size() > 0) || !loadAsDef))
      return;

   _defUseInfo.resize(getNumDefNodes(), TR_UseDefInfo::BitVector(comp()->allocator()));

   if (loadAsDef)
      _loadDefUseInfo.resize(getNumDefNodes(), TR_UseDefInfo::BitVector(allocator()));

   for (int32_t i = getFirstUseIndex(); i <= getLastUseIndex(); i++)
      {
      const TR_UseDefInfo::BitVector &defs = getUseDef_ref(i);

      if (!defs.IsZero())
         {
         //the getUseDef() returns a bitvector that contains only store definitions
         TR_UseDefInfo::BitVector::Cursor cursor(defs);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t defIndex = cursor;
            TR_ASSERT((defIndex < getNumDefNodes()), "USEDEF: found def which is not store or call: useIndex = %d, defIndex = %d", i, defIndex);
            //         traceMsg(comp(), "UDI: for bit vector at index %d setting %d  (i = %d) to true\n",defIndex,i-getFirstUseIndex(),i);
            _defUseInfo[defIndex][i - getFirstUseIndex()] = true;
            }
         }

      if (loadAsDef)
         {
         const TR_UseDefInfo::BitVector &loadDefs = _useDefInfo[i - getFirstUseIndex()];
         if (!loadDefs.IsZero())
            {
            TR_UseDefInfo::BitVector::Cursor cursor(loadDefs);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t defIndex = cursor;
               //TR_ASSERT((defIndex < getNumDefNodes()), "USEDEF: found def which is not store or call %d", defIndex);
               _loadDefUseInfo[defIndex][i - getFirstUseIndex()] = true;
               }
            }
         }
      }
   }

bool TR_UseDefInfo::canComputeReachingDefs()
   {
   uint32_t numNodes = comp()->getFlowGraph()->getNumberOfNodes();
   uint32_t numDefs = getNumExpandedDefNodes();
   uint64_t costPerSet = ((numDefs >> 3) * numNodes);
   if (costPerSet > REACHING_DEFS_LIMIT)
      {
      dumpOptDetails(comp(), "   use/def failed, Reaching defs set too large(%d)\n",costPerSet);
      return false;
      }
   return true;
   }

int32_t TR_UseDefInfo::getSymRefIndexFromUseDefIndex(int32_t udIndex)
   {
   TR_ASSERT( udIndex < _sideTableToSymRefNumMap.size(), "index into _sideTableToSymRefNumMap out of bounds");
   return _sideTableToSymRefNumMap[udIndex];
   }
