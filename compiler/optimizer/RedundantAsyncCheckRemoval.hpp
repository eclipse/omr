/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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

#ifndef REDUNDANT_ASYNC_CHECK_REMOVAL_H
#define REDUNDANT_ASYNC_CHECK_REMOVAL_H

#include <stdint.h>                           // for int32_t, int64_t, etc
#include "compile/Compilation.hpp"            // for Compilation
#include "env/TRMemory.hpp"                   // for TR_Memory, etc
#include "il/ILOpCodes.hpp"                   // for ILOpCodes
#include "infra/Cfg.hpp"                      // for CFG
#include "infra/List.hpp"                     // for List, etc
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_BitVector;
class TR_BlockStructure;
class TR_RegionStructure;
class TR_Structure;
class TR_StructureSubGraphNode;
namespace TR { class SymbolReference; }
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class Node; }


class TR_LoopEstimator
   {
   public:
   TR_ALLOC(TR_Memory::RedundantAsycCheckRemoval)

   TR_LoopEstimator(TR::CFG *cfg, TR_RegionStructure *loop, bool trace)
      : _cfg(cfg), _loop(loop), _trace(trace), _comp(cfg->comp()), _trMemory(cfg->comp()->trMemory())
      {
      }

   uint32_t estimateLoopIterationsUpperBound();

   TR::Compilation * comp() {return _comp;}

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }

   bool trace() { return _trace; }

   enum TR_ProgressionKind {Identity = 0, Arithmetic, Geometric};

   private:

   class IncrementInfo
      {
      public:
      TR_ALLOC(TR_Memory::RedundantAsycCheckRemoval)
      IncrementInfo(int32_t incr) : _incr(incr), _unknown(false), _kind(Identity) {}
      IncrementInfo(IncrementInfo *other)
         { _incr = other->_incr; _unknown = other->_unknown; _kind = other->_kind; }


      void setUnknownValue() { _unknown = true; }
      bool isUnknownValue() { return _unknown; }
      TR_ProgressionKind getKind() {return _kind;}
      void arithmeticIncrement(int32_t incr)
         {
         if (_kind == Geometric) _unknown = true;
         else if (_kind == Identity) _kind = Arithmetic;
         if (!_unknown) _incr += incr;
         }
      void geometricIncrement(int32_t incr)
         {
         if (_kind == Arithmetic) _unknown = true;
         else if (_kind == Identity) _kind = Geometric;
         if (!_unknown) _incr += incr;
         }

      void merge(IncrementInfo *other);

      int32_t _incr;
      TR_ProgressionKind _kind;
      bool _unknown;
      };

   class EntryInfo
      {
      public:
      TR_ALLOC(TR_Memory::RedundantAsycCheckRemoval)
      EntryInfo(int32_t val) : _val(val), _unknown(false) {}
      EntryInfo(EntryInfo *other): _val(other->_val), _unknown(other->_unknown) {}
      EntryInfo() : _unknown(true) {}

      void setUnknownValue() { _unknown = true; }
      bool isUnknownValue () { return _unknown; }
      void merge(EntryInfo *other);

      int32_t _val;
      bool _unknown;
      };

   bool isRecognizableExitEdge(TR::CFGEdge *edge, TR::ILOpCodes *op,
                               TR::SymbolReference **ref, TR_ProgressionKind *kind, int64_t *limit);
   void getLoopIncrements(TR_BitVector &candidates, IncrementInfo **loopIncrements);
   void processBlock(TR::Block *block, TR_BitVector &candidates);
   bool isProgressionalStore(TR::Node *node, TR_ProgressionKind *kind, int32_t *incr);

   bool getProgression(TR::Node *node, TR::SymbolReference **ref, TR_ProgressionKind *kind, int32_t *incr);

   EntryInfo *getEntryValueForSymbol(TR::SymbolReference *ref);
   EntryInfo *getEntryValue(TR::Block *block, TR::SymbolReference *symRef, TR_BitVector &nodesDone, EntryInfo **entryInfos);


   void mergeWithLoopIncrements(TR::Block *block, IncrementInfo **loopIncrements);

   IncrementInfo ***getBlockInfoArray();
   IncrementInfo  **getIncrementInfoArray();
   EntryInfo      **getEntryInfoArray();

   TR::Compilation *    _comp;
   TR_Memory *         _trMemory;
   TR::CFG             *_cfg;
   TR_RegionStructure *_loop;
   IncrementInfo    ***_blockInfo;

   uint32_t _numCandidates;
   uint32_t _numBlocks;
   bool     _trace;

   class ExitCondition
      {
      public:
      TR_ALLOC(TR_Memory::RedundantAsycCheckRemoval)
      ExitCondition(TR::ILOpCodes opCode, TR::SymbolReference *local, int64_t limit)
         : _opCode(opCode), _local(local), _limit(limit) {}

      int64_t _limit;
      TR::SymbolReference *_local;
      TR::ILOpCodes _opCode;
      };
   };


/*
 * Class TR_RedundantAsyncCheckRemoval
 * ===================================
 *
 * Redundant async check removal optimization minimizes the number of async
 * checks in a given loop. It uses an algorithm that attempts to achieve its 
 * goal by adopting two distinct methods:
 *
 * 1. Check if the loop is known to be short running (either based on induction 
 * variable information obtained from value propagation) or if the special 
 * versioning test for short running loops is found. In either case, since 
 * the loop is provably not long running and does not require an async check at all.
 *
 * 2. If async checks cannot be skipped completely for the loop, attempt to
 * place the sync checks at points within the loop such that there is 
 * minimal redundancy (two async checks along a control flow path are avoided 
 * as far as possible). Note that non-INL calls are implicit async checks 
 * and the placement algorithm takes them into account as well.
 */

class TR_RedundantAsyncCheckRemoval : public TR::Optimization
   {
   public:
   TR_RedundantAsyncCheckRemoval(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_RedundantAsyncCheckRemoval(manager);
      }

   virtual bool    shouldPerform();
   virtual int32_t perform();

   enum YieldPointKind { NoYieldPoint, SoftYieldPoint, HardYieldPoint };
   enum CoverageKind { NotCovered, PartiallyCovered, FullyCovered };

   private:

   // Used to construct the partial ordering of the nodes
   //
   class Relationship
      {
      public:
      TR_ALLOC(TR_Memory::RedundantAsycCheckRemoval)
      TR_StructureSubGraphNode *a;
      TR_StructureSubGraphNode *b;
      };

   class AsyncInfo
      {
      public:
      TR_ALLOC(TR_Memory::RedundantAsycCheckRemoval)

      AsyncInfo(TR_Memory * m)
         : _coverage(NotCovered), _yieldPoint(NoYieldPoint), _isAncestor(false),
           _canHaveAYieldPoint(true), _coveredOnBackwardPaths(false),
           _children(m),
           _parents(m)
         {
         }

      CoverageKind getCoverage() { return _coverage; }
      void setCoverage(CoverageKind coverage) { _coverage = coverage; }
      bool hasYieldPoint() { return _yieldPoint != NoYieldPoint; }
      bool hasSoftYieldPoint() { return _yieldPoint == SoftYieldPoint; }
      void setHardYieldPoint() { _yieldPoint = HardYieldPoint; }
      void setSoftYieldPoint() { _yieldPoint = SoftYieldPoint; }
      void removeYieldPoint() { _yieldPoint = NoYieldPoint; }

      void setReverseCoverageInfo(bool b) { _coveredOnBackwardPaths = b; }
      bool getReverseCoverageInfo() { return _coveredOnBackwardPaths; }
      bool isCoveredOnBackwardPaths() { return _coveredOnBackwardPaths; }

      bool isAncestor() { return _isAncestor; }
      void markAsAncestor() { _isAncestor = true; }

      bool canHaveAYieldPoint()          { return _canHaveAYieldPoint; }
      void setCanHaveAYieldPoint(bool v) { _canHaveAYieldPoint = v; }

      void setVisitMarker(TR_StructureSubGraphNode *node) { _visitMarker = node; }
      bool isAlreadyVisited(TR_StructureSubGraphNode *node)
            { return _visitMarker == node; }

      void addChild(TR_StructureSubGraphNode *child)
            { _children.add(child); }
      void addParent(TR_StructureSubGraphNode *parent)
            { _parents.add(parent); }


      List<TR_StructureSubGraphNode> &getChildren() {return _children; }
      List<TR_StructureSubGraphNode> &getParents() {return _parents; }

      private:

      // marker to keep track of visits, in getNearestAncestor
      //
      TR_StructureSubGraphNode *_visitMarker;

      List<TR_StructureSubGraphNode> _children;  //nodes that we are are greater than
      List<TR_StructureSubGraphNode> _parents; //nodes that we are less than

      CoverageKind       _coverage;
      YieldPointKind     _yieldPoint;

      bool               _isAncestor;
      bool               _canHaveAYieldPoint;
      bool               _coveredOnBackwardPaths;
      };

   int32_t perform(TR_Structure *str, bool insideImproperRegion = false);
   void    initialize(TR_Structure *str);

   int32_t processBlockStructure   (TR_BlockStructure *block);
   int32_t processAcyclicRegion    (TR_RegionStructure *region);
   int32_t processNaturalLoop      (TR_RegionStructure *region, bool isInsideImproperRegion=false);
   int32_t processImproperRegion   (TR_RegionStructure *region);

   void propagateBackwards(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry);
   void computeCoverageInfo (TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry);
   void markAncestors (TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry);
   void getNearestAncestors(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *current, TR_StructureSubGraphNode *entry);
   TR_StructureSubGraphNode *findSmallestAncestor();
   void insertAsyncCheckOnSubTree(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry);
   void enqueueSinks(TR_RegionStructure *region, TR_Queue<TR_StructureSubGraphNode> *q, bool in);
   bool performRegionalBackwardAnalysis(TR_RegionStructure *region, bool in);
   void solidifySoftAsyncChecks(TR_StructureSubGraphNode *node);
   int32_t estimateLoopIterations(TR_RegionStructure *loop);
   bool isMaxLoopIterationGuardedLoop(TR_RegionStructure *loop);

   bool hasEarlyExit(TR_RegionStructure *region);

   bool callDoesAnImplicitAsyncCheck(TR::Node *callNode);

   void markExtendees(TR::Block *block, bool v);
   bool containsImplicitInternalPointer(TR::Node *node);
   bool originatesFromShortRunningMethod(TR_RegionStructure* region);
   int32_t findShallowestCommonCaller(int32_t callSiteIndex1, int32_t callSiteIndex2);

   TR::CFG *_cfg;
   List<TR_StructureSubGraphNode> _ancestors;

   bool    _asyncCheckInCurrentLoop;
   int32_t _numAsyncChecksInserted;
   bool    _foundShortRunningLoops;
   };
#endif
