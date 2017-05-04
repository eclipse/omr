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

#ifndef OSRDEFANALYSIS_INCL
#define OSRDEFANALYSIS_INCL

#include <stdint.h>                           // for int32_t
#include "il/Node.hpp"                        // for Node, vcount_t
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager
#include "optimizer/UseDefInfo.hpp"  // for TR_UseDefInfo

class TR_BitVector;
class TR_Liveness;
class TR_OSRMethodData;
class TR_OSRPoint;
namespace TR { class Block; }
namespace TR { class ResolvedMethodSymbol; }

class TR_OSRDefInfo : public TR_UseDefInfo
   {
   public:

   TR_OSRDefInfo(TR::OptimizationManager *manager);

   private:
   virtual bool performAnalysis(AuxiliaryData &aux);
   void performFurtherAnalysis(AuxiliaryData &aux);
   virtual void processReachingDefinition(void* vblockInfo, AuxiliaryData &aux);
   void buildOSRDefs(void *blockInfo, AuxiliaryData &aux);
   void buildOSRDefs(TR::Node *node, void *analysisInfo, TR_OSRPoint *osrPoint, TR_OSRPoint *osrPoint2, TR::Node *parent, AuxiliaryData &aux);
   void addSharingInfo(AuxiliaryData &aux);

   TR::ResolvedMethodSymbol *_methodSymbol;
   };

class TR_OSRDefAnalysis : public TR::Optimization
   {
   public:

   TR_OSRDefAnalysis(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_OSRDefAnalysis(manager);
      }

   virtual int32_t perform();

   private:

   bool requiresAnalysis();
   };


struct ParentInfo
   {
   int32_t             _childNum;
   TR::Node            *_parent;
   ParentInfo      *_next;
   };

struct NodeParentInfo
   {
   TR::Node *_node;
   ParentInfo *_parentInfo;
   };

class TR_OSRLiveRangeAnalysis : public TR::Optimization
   {
   public:

   TR_OSRLiveRangeAnalysis(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_OSRLiveRangeAnalysis(manager);
      }

   virtual int32_t perform();

   private:

   bool canAffordAnalysis();
   void buildOSRLiveRangeInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint,
      int32_t *liveLocalIndexToSymRefNumberMap, int32_t maxSymRefNumber, int32_t numBits,
      TR_OSRMethodData *osrMethodData, TR::OSRTransitionTarget target);
   void maintainLiveness(TR::Node *node, TR::Node *parent, int32_t childNum, vcount_t  visitCount,
       TR_Liveness *liveLocals, TR_BitVector *liveVars, TR::Block *block);
   TR::TreeTop *collectPendingPush(TR_ByteCodeInfo bci, TR::TreeTop *pps, TR_BitVector *liveVars);

   TR_BitVector *_deadVars;
   TR_BitVector *_liveVars;
   TR_BitVector *_pendingPushVars;
   NodeParentInfo **_pendingSlotValueParents;
   };

class TR_OSRExceptionEdgeRemoval : public TR::Optimization
   {
   public:

   TR_OSRExceptionEdgeRemoval(TR::OptimizationManager *manager)
     : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_OSRExceptionEdgeRemoval(manager);
      }

   virtual int32_t perform();
   };

#endif
