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

#ifndef PARTIALREDUNDANCY_INCL
#define PARTIALREDUNDANCY_INCL

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/DataFlowAnalysis.hpp"

class TR_BlockStructure;
class TR_ExceptionCheckMotion;
class TR_RegionStructure;
class TR_RegisterCandidate;
class TR_Structure;
namespace TR { class Block; }
namespace TR { class CFGNode; }
namespace TR { class Compilation; }
namespace TR { class ILOpCode; }
namespace TR { class Optimizer; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }


/**
 * Class TR_PartialRedundancy
 * ==========================
 *
 * Partial Redundancy Elimination (PRE) is a traditional optimization 
 * that aims to compute optimal placement points for computations that 
 * are chosen to participate in the analysis. This technique works quite 
 * well for computations that are unrelated to exception checks since 
 * they can be moved around independently of other computations. 
 * Expressions that are dependent on some other exception checking being 
 * done, e.g. an array access needs to either be explicitly bounds checked 
 * or it can only be moved across a region of code where it is known that 
 * the array access will not result in an exception. Because exceptions 
 * complicate code motion done by PRE, we implemented an enhancement to 
 * the traditional algorithm described in: 
 * http://dl.acm.org/citation.cfm?id=1356077&dl=&coll=&preflayout=tabs
 *
 * PRE is quite expensive to run and so it only runs at optimization 
 * levels > warm and so we also rely on other cheaper optimizations 
 * specifically directed at exception checks to optimize them out of 
 * loops. One example is loop versioning.
 */

class TR_PartialRedundancy : public TR::Optimization
   {
   public:
   typedef TR_Isolatedness::ContainerType ContainerType;

   TR_PartialRedundancy(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_PartialRedundancy(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   void printTrees();
   TR::TreeTop *placeComputationsOptimally(TR::Block *, TR::Node ***);
   void invalidateOptimalComputation(int32_t nextOptimalComputation);
   void placeInGlobalRegisters(TR::Block *);
   void eliminateRedundantComputations(TR::Block *, TR::Node **, ContainerType **, TR::TreeTop *);
   bool isOpCodeAnImplicitNoOp(TR::ILOpCode &opCode);
   bool isNodeAnImplicitNoOp(TR::Node *node);
   TR::Node *duplicateExact(TR::Node *, List<TR::Node> *, List<TR::Node> *, vcount_t);
   void collectAllNodesToBeAnchored(List<TR::Node> *, TR::Node *, vcount_t);
   bool eliminateRedundantSupportedNodes(TR::Node *, TR::Node *, bool, TR::TreeTop *, int32_t, vcount_t, ContainerType *, ContainerType *, TR::Node **);
   TR::TreeTop *replaceOptimalSubNodes(TR::TreeTop *curTree, TR::Node *, TR::Node *, int32_t, TR::Node *, TR::Node *, bool, int32_t, vcount_t);
   void processReusedNode(TR::Node *node, TR::ILOpCodes newOpCode, TR::SymbolReference *newSymRef, int newNumChildren);
   bool isNotPrevTreeStoreIntoFloatTemp(TR::Symbol *);
   bool isSupportedOpCode(TR::Node *, TR::Node *);
   int32_t getNumberOfBits() { return _numberOfBits; }
   ContainerType **getOptSetInfo() { return _optSetInfo;}
   ContainerType **getRednSetInfo() { return _rednSetInfo;}
   ContainerType **getOrigOptSetInfo() { return _origOptSetInfo;}
   ContainerType *getSymOptimalNodes() { return _symOptimalNodes;}
   ContainerType **getUnavailableSetInfo() { return _unavailableSetInfo;}
   int32_t *getGlobalRegisterWeights() { return _globalRegisterWeights;}
   void calculateGlobalRegisterWeightsBasedOnStructure(TR_Structure *, int32_t *);
   int32_t getNumNodes() { return _numNodes;}
   TR::Node *getAlreadyPresentOptimalNode(TR::Node *, int32_t, vcount_t, bool &);
   ContainerType *allocateContainer(int32_t size);

   TR_Isolatedness *_isolatedness;

   bool ignoreNode (TR::Node *node);

   private:

   int32_t _numNodes, _numProfilingsAllowed;
   vcount_t _visitCount;
   //vcount_t _visitCount1;
   int32_t _counter;
   int32_t _numberOfBits;
   int32_t *_newSymbolsMap;
   TR::Node *_nullCheckNode;
   TR::TreeTop *_prevTree;
   TR::Symbol **_newSymbols;
   TR::SymbolReference **_newSymbolReferences;
   TR_RegisterCandidate **_registerCandidates;
   ContainerType **_optSetInfo;
   ContainerType **_rednSetInfo;
   ContainerType **_origOptSetInfo;
   ContainerType **_unavailableSetInfo;
   ContainerType *_temp;
   ContainerType *_symOptimalNodes;
   int32_t *_globalRegisterWeights;
   int32_t **_orderedOptNumbersList;
   bool _useAliasSetsNotGuaranteedToBeCorrect;
   TR_ExceptionCheckMotion *_exceptionCheckMotion;
   bool _profilingWalk;

   bool _loadaddrPRE;
   bool _ignoreLoadaddrOfLitPool;

   private: // local index -> common node

  };

class TR_ExceptionCheckMotion : public TR_DataFlowAnalysis
   {
   public:

   struct ExprDominanceInfo
      {
      TR_ALLOC(TR_Memory::PartialRedundancy)
      List<TR::Node> *_inList;
      List<TR::Node> **_outList;
      bool _containsExceptionTreeTop;
      };

   typedef TR_BitVector ContainerType;

   TR_ExceptionCheckMotion(TR::Compilation *, TR::Optimizer *, TR_PartialRedundancy *);
   virtual Kind getKind();
   virtual TR_ExceptionCheckMotion *asExceptionCheckMotion() {return this;}

   virtual bool analyzeBlockStructure(TR_BlockStructure *, bool);
   virtual bool analyzeRegionStructure(TR_RegionStructure *, bool);
   bool analyzeNodeIfSuccessorsAnalyzed(TR::CFGNode *, TR_RegionStructure *, ContainerType *, ContainerType *);
   int32_t perform();
   void initializeGenAndKillSetInfo();
   void analyzeNodeToInitializeGenAndKillSets(TR::TreeTop *, vcount_t, TR_BlockStructure *);
   void initializeAnalysisInfo(ExprDominanceInfo *, TR_Structure *);
   void initializeAnalysisInfo(ExprDominanceInfo *, TR::Block *);
   void initializeAnalysisInfo(ExprDominanceInfo *, TR_RegionStructure *);

   TR_ExceptionCheckMotion::ExprDominanceInfo *getAnalysisInfo(TR_Structure *);
   TR_ExceptionCheckMotion::ExprDominanceInfo *createAnalysisInfo();
   void copyListFromInto(List<TR::Node> *fromList, List<TR::Node> *);
   void composeLists(List<TR::Node> *, List<TR::Node> *, ContainerType *);
   void appendLists(List<TR::Node> *, List<TR::Node> *);
   bool compareLists(List<TR::Node> *, List<TR::Node> *);
   void initializeOutLists(List<TR::Node> **);
   void initializeOutList(List<TR::Node> *);
   void createAndAddListElement(TR::Node *, int32_t);
   void removeFromList(ListElement<TR::Node> *, List<TR::Node> *, ListElement<TR::Node> *);
   bool includeRelevantNodes(TR::Node *, vcount_t, int32_t);
   bool isNodeKilledByChild(TR::Node *, TR::Node *, int32_t);
   ContainerType **getActualOptSetInfo() { return _actualOptSetInfo;}
   ContainerType **getActualRednSetInfo() { return _actualRednSetInfo;}
   ContainerType **getOptimisticOptSetInfo() { return _optimisticOptSetInfo;}
   ContainerType **getOptimisticRednSetInfo() { return _optimisticRednSetInfo;}
   int32_t **getOrderedOptNumbersList() { return _orderedOptNumbersList;}
   List<TR::Node> **getOrderedOptList() { return _orderedOptList;}
   List<TR::Node> **getGenSetList() { return _regularGenSetInfo;}
   bool getTrySecondBestSolution() { return _trySecondBestSolution;}
   void setBlockFenceStatus(TR::Block *);
   int32_t areExceptionSuccessorsIdentical(TR::CFGNode *, TR::CFGNode *);
   TR_PartialRedundancy *getPartialRedundancy() {return _partialRedundancy;}
   ContainerType *getExprsUnaffectedByOrder() {return _exprsUnaffectedByOrder;}
   bool checkIfNodeCanSurvive(TR::Node *, ContainerType *);
   bool checkIfNodeCanSomehowSurvive(TR::Node *, ContainerType *);
   void markNodeAsSurvivor(TR::Node *, ContainerType *);
   bool isNodeValueZero(TR::Node *);
   ContainerType *allocateContainer(int32_t size);

   bool trace() { return _partialRedundancy->trace(); }

   private:
   TR_PartialRedundancy *_partialRedundancy;
   List<TR::Node> **_currentList;
   ListElement<TR::Node> *_lastGenSetElement;
   int32_t _numberOfNodes;
   int32_t _numNodes;
   int32_t _numberOfBits;
   vcount_t _currentVisitCount;
   bool _firstIteration;
   bool _tryAnotherIteration;
   bool _trySecondBestSolution;
   bool _hasExceptionSuccessor;
   bool _moreThanOneExceptionPoint;
   bool _atLeastOneExceptionPoint;
   bool _seenImmovableExceptionPoint;
   List<TR::Node> **_regularGenSetInfo;
   TR_ScratchList<TR::Node> _workingList;
   //////List<TR::Node> *_regularKillSetInfo;
   List<TR::Node> **_exceptionGenSetInfo;
   List<TR::Node> **_blockInfo;
   List<TR::Node> **_orderedOptList;
   int32_t **_orderedOptNumbersList;
   //////List<TR::Node> *_exceptionKillSetInfo;
   ContainerType *_nodesInCycle;
   ContainerType *_tempContainer;
   ContainerType *_indirectAccessesThatSurvive;
   ContainerType *_arrayAccessesThatSurvive;
   ContainerType *_dividesThatSurvive;
   ContainerType *_unresolvedAccessesThatSurvive;
   ContainerType *_knownIndirectAccessesThatSurvive;
   ContainerType *_knownArrayAccessesThatSurvive;
   ContainerType *_knownDividesThatSurvive;
   ContainerType *_knownUnresolvedAccessesThatSurvive;
   ContainerType *_exprsUnaffectedByOrder;
   ContainerType *_nullCheckKilled;
   ContainerType *_resolveCheckKilled;
   ContainerType *_boundCheckKilled;
   ContainerType *_divCheckKilled;
   ContainerType *_arrayStoreCheckKilled;
   ContainerType *_arrayCheckKilled;
   ContainerType *_checkCastKilled;
   ContainerType *_indirectAccessesKilled;
   ContainerType *_unresolvedAccessesKilled;
   ContainerType *_arrayAccessesKilled;
   ContainerType *_dividesKilled;
   ContainerType *_callOrGlobalStoreKilled;
   ContainerType *_relevantNodes;
   ContainerType *_exprsContainingIndirectAccess;
   ContainerType *_exprsContainingUnresolvedAccess;
   ContainerType *_exprsContainingArrayAccess;
   ContainerType *_exprsContainingDivide;
   ContainerType **_optimisticOptSetInfo;
   ContainerType **_optimisticRednSetInfo;
   ContainerType **_actualOptSetInfo;
   ContainerType **_actualRednSetInfo;
   ContainerType **_killedGenExprs;
   ContainerType *_genSetHelper;
   ContainerType *_appendHelper;
   ContainerType *_composeHelper;
   ContainerType *_definitelyNotKilled;
   ContainerType *_blockWithFencesAtEntry;
   ContainerType *_blockWithFencesAtExit;
   ContainerType *_firstSucc;
   ContainerType *_secondSucc;
   ContainerType *_comparison;
   ContainerType *_pendingList;
   };

class TR_RedundantExpressionAdjustment
   : public TR_IntersectionBitVectorAnalysis
   {
   public:

   TR_RedundantExpressionAdjustment(TR::Compilation *, TR::Optimizer *, TR_Structure *, TR_ExceptionCheckMotion *);

   virtual Kind getKind();
   virtual TR_RedundantExpressionAdjustment *asRedundantExpressionAdjustment();

   virtual int32_t getNumberOfBits();
   virtual bool supportsGenAndKillSets();
   virtual bool supportsGenAndKillSetsForStructures();
   virtual void initializeGenAndKillSetInfo();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, ContainerType *);
   ////virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool analyzeBlockStructure(TR_BlockStructure *, bool);
   virtual bool postInitializationProcessing();

   private:
   ContainerType *_optSetHelper;
   TR_PartialRedundancy *_partialRedundancy;
   TR_ExceptionCheckMotion *_exceptionCheckMotion;
   int32_t _numberOfNodes;
   };

#endif
