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

#ifndef OSRDEFANALYSIS_INCL
#define OSRDEFANALYSIS_INCL

#include <stdint.h>
#include "il/Node.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/UseDefInfo.hpp"

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
   virtual const char * optDetailString() const throw();

   private:

   bool requiresAnalysis();
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
   virtual const char * optDetailString() const throw();

   private:

   bool shouldPerformAnalysis();
   int32_t fullAnalysis(bool sharedParm, bool containsPendingPush);
   int32_t partialAnalysis();

   void buildOSRLiveRangeInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint,
      int32_t *liveLocalIndexToSymRefNumberMap, int32_t numBits, TR_OSRMethodData *osrMethodData, bool containsPendingPushes);
   void buildOSRSlotSharingInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint,
      int32_t *liveLocalIndexToSymRefNumberMap, TR_BitVector *slotSharingVars);

   void buildDeadSlotsInfo(TR::Node *node, TR_BitVector *liveVars, TR_OSRPoint *osrPoint,
      int32_t *liveLocalIndexToSymRefNumberMap, bool containsPendingPush);

   void buildDeadPendingPushSlotsInfo(TR::Node *node, TR_BitVector *livePendingPushSymRefs, TR_OSRPoint *osrPoint);
   void pendingPushLiveRangeInfo(TR::Node *node, TR_BitVector *liveSymRefs,
      TR_BitVector *allPendingPushSymRefs, TR_OSRPoint *osrPoint, TR_OSRMethodData *osrMethodData);
   void pendingPushSlotSharingInfo(TR::Node *node, TR_BitVector *liveSymRefs,
      TR_BitVector *slotSharingSymRefs, TR_OSRPoint *osrPoint);

   void maintainLiveness(TR::Node *node, TR::Node *parent, int32_t childNum, vcount_t  visitCount,
       TR_Liveness *liveLocals, TR_BitVector *liveVars, TR::Block *block);
   TR::TreeTop *collectPendingPush(TR_ByteCodeInfo bci, TR::TreeTop *pps, TR_BitVector *liveVars);
   void intersectWithExistingDeadSlots (TR_OSRPoint *osrPoint, TR_BitVector *deadPPSSlots, TR_BitVector *deadAutoSlots, bool containsPendingPush);

   TR_BitVector *_liveVars;
   TR_BitVector *_pendingPushSymRefs;
   TR_BitVector *_sharedSymRefs;
   TR_BitVector *_workBitVector;
   TR_BitVector *_workDeadSymRefs;
   TR_BitVector *_visitedBCI;    
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

   TR_BitVector *_seenDeadStores;

   bool addDeadStores(TR::Block* osrBlock, TR_BitVector& dead, bool init);
   void removeDeadStores(TR::Block* osrBlock, TR_BitVector& dead);
   virtual int32_t perform();
   virtual const char * optDetailString() const throw();
   };

#endif
