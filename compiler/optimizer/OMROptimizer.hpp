/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_OPTIMIZER_INCL
#define OMR_OPTIMIZER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_OPTIMIZER_CONNECTOR
#define OMR_OPTIMIZER_CONNECTOR
namespace OMR { class Optimizer; }
namespace OMR { typedef OMR::Optimizer OptimizerConnector; }
#endif

#include <stddef.h>                      // for NULL
#include <stdint.h>                      // for int32_t, uint16_t, uint32_t
#include "compile/CompilationTypes.hpp"  // for TR_Hotness
#include "env/TRMemory.hpp"              // for TR_Memory, etc
#include "il/Node.hpp"                   // for vcount_t
#include "il/TreeTop.hpp"                // for TreeTop
#include "il/TreeTop_inlines.hpp"        // for TreeTop::getNode
#include "infra/Assert.hpp"              // for TR_ASSERT
#include "infra/List.hpp"                // for List
#include "optimizer/Optimizations.hpp"   // for Optimizations, etc
#include "optimizer/OptimizationStrategies.hpp"

class TR_BitVector;
class TR_Debug;
class TR_Structure;
class TR_UseDefInfo;
class TR_ValueNumberInfo;
namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class OptimizationManager; }
namespace TR { class Optimizer; }
namespace TR { class ResolvedMethodSymbol; }
struct OptimizationStrategy;
class OMR_InlinerPolicy;
class OMR_InlinerUtil;


// ValueNumberInfo Build type
//
enum ValueNumberInfoBuildType
   {
        PrePartitionVN,
        HashVN
   };



// Define OPT_TIMING to collect statistics on the time taken by individual optimizations.
// Also use -Xjit:timing to collect statistics.
//#undef OPT_TIMING

#ifdef OPT_TIMING
#include "infra/Statistics.hpp"
extern TR_Stats statOptTiming[];
extern TR_Stats statStructuralAnalysisTiming;
extern TR_Stats statUseDefsTiming;
extern TR_Stats statGlobalValNumTiming;
#endif

namespace OMR
{

#define HIGH_BASIC_BLOCK_COUNT 2500
#define HIGH_LOOP_COUNT 65
#define VERY_HOT_HIGH_LOOP_COUNT 95

// Optimization Options
//
enum
   {
   Always = 0,
   IfLoops,
   IfNoLoops,
   IfProfiling,
   IfNotProfiling,
   IfNotJitProfiling,
   IfNews,
   IfEnabledAndOptServer,
   IfOptServer,
   IfNotClassLoadPhase,
   IfNotClassLoadPhaseAndNotProfiling,
   IfEnabledAndLoops,
   IfEnabledAndNoLoops,
   IfNoLoopsOREnabledAndLoops, // ie. do this opt if no loops; if loops, it must be enabled
   IfEnabledAndProfiling,
   IfEnabledAndNotProfiling,
   IfEnabledAndNotJitProfiling,
   IfLoopsAndNotProfiling,
   MustBeDone,
   IfEnabled,
   IfLoopsMarkLastRun,
   IfEnabledMarkLastRun,
   IfMoreThanOneBlock,
   IfOneBlock,
   IfEnabledAndMoreThanOneBlock,
   IfEnabledAndMoreThanOneBlockMarkLastRun,
   IfAOTAndEnabled,
   IfMethodHandleInvokes, // JSR292: Extra analysis required to optimize MethodHandle.invoke
   IfNotQuickStart,
   IfFullInliningUnderOSRDebug,
   IfNotFullInliningUnderOSRDebug,
   IfOSR,
   IfVoluntaryOSR,
   IfInvoluntaryOSR,
   IfMonitors,
   IfEnabledAndMonitors,
   IfEAOpportunities,
   IfEAOpportunitiesMarkLastRun,
   IfEAOpportunitiesAndNotOptServer,
   IfAggressiveLiveness,
   MarkLastRun
   };

class Optimizer
   {
   friend class ::TR_DebugExt;

   public:

   TR_ALLOC(TR_Memory::Machine)

   // Create an optimizer object.
   static TR::Optimizer *createOptimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen);

   Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
         const OptimizationStrategy *strategy = NULL, uint16_t VNType = 0);

   // Optimize the current method
   void optimize();

   // Get the method symbol
   TR::ResolvedMethodSymbol *getMethodSymbol()  { return _methodSymbol; }

   // The IL optimizer, performs tree-to-tree optimizing transformations.
   bool isIlGenOpt() { return _isIlGen; }

   void performVeryLateOpts();

   TR::Compilation *      comp()           { return _compilation; }
   TR::CodeGenerator *    cg()             { return _cg; }
   TR_Debug *          getDebug();

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }
   TR_PersistentMemory *     trPersistentMemory()          { return _trMemory->trPersistentMemory(); }

   static const char * getOptimizationName(OMR::Optimizations opt);

   static const OptimizationStrategy *optimizationStrategy( TR::Compilation *c);

   /**
    * Override what #OptimizationStrategy to use. While this has the same
    * functionality as the `optTest=` parameter, it does not require
    * reinitializing the JIT, nor manually changing the optFile.
    *
    * If _mockStrategy is NULL, has no effect on the optimizer. 
    *
    * @param strategy The #OptimizationStrategy to return when requested. 
    */
   static void setMockStrategy(const OptimizationStrategy *strategy) { _mockStrategy = strategy; };


   static ValueNumberInfoBuildType valueNumberInfoBuildType();

   void enableAllLocalOpts();

   bool getAliasSetsAreValid()       { return _aliasSetsAreValid; }
   void setAliasSetsAreValid(bool b, bool setForWCode = false);

   bool getInlineSynchronized()       { return _inlineSynchronized; }
   void setInlineSynchronized(bool b) { _inlineSynchronized = b; }

   int32_t *getSymReferencesTable();
   void setSymReferencesTable(int32_t *symReferencesTable) { _symReferencesTable = symReferencesTable; }

   TR_UseDefInfo *getUseDefInfo()             { return _useDefInfo; }
   TR_UseDefInfo *setUseDefInfo(TR_UseDefInfo *u);

   bool cantBuildGlobalsUseDefInfo()          { return _cantBuildGlobalsUseDefInfo; }
   bool cantBuildLocalsUseDefInfo()           { return _cantBuildLocalsUseDefInfo; }
   void setCantBuildGlobalsUseDefInfo(bool v) { _cantBuildGlobalsUseDefInfo = v; }
   void setCantBuildLocalsUseDefInfo(bool v)  { _cantBuildLocalsUseDefInfo = v; }

   // Get value number information
   TR_ValueNumberInfo * getValueNumberInfo()   { return _valueNumberInfo; }
   TR_ValueNumberInfo *createValueNumberInfo(bool requiresGlobals = false, bool preferGlobals = true, bool noUseDefInfo = false );
   TR_ValueNumberInfo *setValueNumberInfo(TR_ValueNumberInfo *v);

   bool cantBuildGlobalsValueNumberInfo()          { return _cantBuildGlobalsValueNumberInfo; }
   bool cantBuildLocalsValueNumberInfo()           { return _cantBuildLocalsValueNumberInfo; }
   void setCantBuildGlobalsValueNumberInfo(bool v) { _cantBuildGlobalsValueNumberInfo = v; }
   void setCantBuildLocalsValueNumberInfo(bool v)  { _cantBuildLocalsValueNumberInfo = v; }

   bool canRunBlockByBlockOptimizations()          { return _canRunBlockByBlockOptimizations; }
   void setCanRunBlockByBlockOptimizations(bool v) { _canRunBlockByBlockOptimizations = v; }

   bool prepareForNodeRemoval(TR::Node *node , bool deferInvalidatingUseDefInfo = false);
   void prepareForTreeRemoval(TR::TreeTop *treeTop) { prepareForNodeRemoval(treeTop->getNode()); }

   bool cachedExtendedBBInfoValid()                { return _cachedExtendedBBInfoValid; }
   void setCachedExtendedBBInfoValid(bool b);

   TR::Block *getEnclosingFinallyBlock()           { return _enclosingFinallyBlock; }
   void setEnclosingFinallyBlock(TR::Block *block) { _enclosingFinallyBlock = block; }

   void getStaticFrequency(TR::Block *, int32_t *);

   int32_t doStructuralAnalysis();

   bool switchToProfiling(uint32_t f, uint32_t c) { return false; }
   bool switchToProfiling() { return false; }
   int32_t changeContinueLoopsToNestedLoops();

   List<TR::Node>& getEliminatedCheckcastNodes() { return _eliminatedCheckcastNodes; }
   List<TR::Node>& getClassPointerNodes() { return _classPointerNodes; }

   void dumpPostOptTrees();

#ifdef DEBUG
   int32_t     getDumpGraphsIndex() { return _dumpGraphsIndex; }
   void        doStructureChecks();
#else
   void        doStructureChecks() { }
#endif

   TR_Hotness checkMaxHotnessOfInlinedMethods(TR::Compilation *comp);

   bool checkNumberOfLoopsAndBasicBlocks(TR::Compilation *, TR_Structure *);
   void countNumberOfLoops(TR_Structure *);

   bool getLastRun(OMR::Optimizations opt);

   void setRequestOptimization(OMR::Optimizations optNum, bool value = true, TR::Block *block = NULL);

   int32_t getOptMessageIndex() { return _optMessageIndex; }
   int32_t incOptMessageIndex() { return ++_optMessageIndex; }

   bool optsThatCanCreateLoopsDisabled() { return _disableLoopOptsThatCanCreateLoops; }

   // allowBCDSignPromotion -- if true and node1 has conservatively 'better' sign state then node2 then also consider
   // nodes equivalent (used only by certain optimizations such as CSE)
   static bool areNodesEquivalent(TR::Node *, TR::Node *, TR::Compilation *, bool allowBCDSignPromotion=false);
   static bool areBCDAggrConstantNodesEquivalent(TR::Node *, TR::Node *, TR::Compilation *);
   bool areNodesEquivalent(TR::Node * node1, TR::Node * node2, bool allowBCDSignPromotion=false)
      {
      return areNodesEquivalent(node1, node2, comp(), allowBCDSignPromotion);
      // WCodeLinkageFixup runs a version of LocalCSE that is not owned by
      // an optimizer, so it has to pass in a TR::Compilation
      }

   bool areSyntacticallyEquivalent(TR::Node *, TR::Node *, vcount_t);

   TR_BitVector *getSeenBlocksGRA()                        { return _seenBlocksGRA; }
   void setSeenBlocksGRA(TR_BitVector *bv)                 { _seenBlocksGRA = bv; }
   TR_BitVector *getResetExitsGRA()                        { return _resetExitsGRA; }
   void setResetExitsGRA(TR_BitVector *bv)                 { _resetExitsGRA = bv; }
   TR_BitVector *getSuccessorBitsGRA()                     { return _successorBitsGRA; }
   void setSuccessorBitsGRA(TR_BitVector *bv)              { _successorBitsGRA = bv; }

   OMR_InlinerPolicy *getInlinerPolicy();
   OMR_InlinerUtil *getInlinerUtil();
   TR::OptimizationManager *getOptimization(OMR::Optimizations i)
      {
      TR_ASSERT(i < OMR::numGroups, "Optimization index should be less than %d", OMR::numGroups);
      return _opts[i];
      }

   bool isEnabled(OMR::Optimizations i);

   enum // RAS
      {
      // Analyses start with "A", but not "A0" because that's "After Optimization"
      BUILDING_VALUE_NUMBERS          = 0xA1,
      BUILDING_STRUCTURE              = 0xA5, // "5"tructure
      BUILDING_ALIASES                = 0xAA, // "A"liases
      BUILDING_ACCURATE_NODE_COUNT    = 0xAC, // "C"ount
      BUILDING_USE_DEFS               = 0xAD, // "D"efs
      BUILDING_FREQUENCIES            = 0xAF, // "F"requencies

      // When no analysis is running
      BEFORE_OPTIMIZATION             = 0xB0,
      PERFORMING_OPTIMIZATION         = 0xFF,
      AFTER_OPTIMIZATION              = 0xA0,

      HWPROFILE_PEEPHOLE_BLOCKS       = 0xCB,
      PERFORMING_CHECKS               = 0xCC,

      } AnalysisPhases;

   protected:
   TR::OptimizationManager *      _opts[OMR::numGroups];

   private:

   TR::Optimizer *self();

   int32_t performOptimization(const OptimizationStrategy *, int32_t firstOptIndex, int32_t lastOptIndex, int32_t doTiming);

   void dumpStrategy(const OptimizationStrategy *);


   TR::Compilation *            _compilation;
   TR_Memory *                   _trMemory;
   TR::CodeGenerator *          _cg;

   TR::ResolvedMethodSymbol *   _methodSymbol;
   bool                          _isIlGen;

   const OptimizationStrategy *          _strategy;

   /* 
    * Since mock strategies are only used in testing right now, we make this
    * static to ease implementation. 
    *
    * This is currently not a thread-safe implementation beause doing a
    * thread-safe implementation would require a more invasive compilation
    * control modification (since the strategy would need to be injected into
    * the Compilation object, but due to where it's created, there's little
    * opportunity for this.)
    */
   static const OptimizationStrategy *_mockStrategy;

   int32_t *                     _symReferencesTable;
   TR_UseDefInfo *               _useDefInfo;
   TR_ValueNumberInfo *          _valueNumberInfo;
   uint16_t                      _vnInfoType;

   TR::Block *                   _enclosingFinallyBlock;

   List<TR::Node>               _eliminatedCheckcastNodes;
   List<TR::Node>               _classPointerNodes;

   int32_t                       _dumpGraphsIndex;
   int32_t                       _firstDumpOptPhaseTrees;
   int32_t                       _lastDumpOptPhaseTrees;
   int32_t                       _optMessageIndex;

   bool                          _aliasSetsAreValid;
   bool                          _cantBuildGlobalsUseDefInfo;
   bool                          _cantBuildLocalsUseDefInfo;
   bool                          _cantBuildGlobalsValueNumberInfo;
   bool                          _cantBuildLocalsValueNumberInfo;
   bool                          _canRunBlockByBlockOptimizations;
   bool                          _inlineSynchronized;
   bool                          _dumpGraphs;
   bool                          _stackedOptimizer;
   bool                          _cachedExtendedBBInfoValid;

   int32_t                       _numBasicBlocksInMethod;
   int32_t                       _numLoopsInMethod;

   bool                          _firstTimeStructureIsBuilt;
   bool                          _disableLoopOptsThatCanCreateLoops;

   TR_BitVector *                _seenBlocksGRA; // used during the GRA as a global
   TR_BitVector *                _resetExitsGRA; // used during the GRA as a global
   TR_BitVector *                _successorBitsGRA; // used during the GRA as a global
   };

}

#endif
