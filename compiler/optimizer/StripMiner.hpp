/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "optimizer/LoopCanonicalizer.hpp"

#include <stdint.h>
#include "env/jittypes.h"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/List.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_ParentOfChildNode;
class TR_PrimaryInductionVariable;
class TR_RegionStructure;
class TR_Structure;
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class Optimization; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

#define DEFAULT_STRIP_LENGTH 1024

class TR_StripMiner : public TR_LoopTransformer
   {
   public:
   TR_StripMiner(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_StripMiner(manager);
      }

   virtual bool    shouldPerform();
   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   /* types */
   typedef enum
      {
      unknownLoop,
      preLoop,
      mainLoop,
      postLoop,
      residualLoop,
      offsetLoop
      } TR_ClonedLoopType;

   /* data */
   struct LoopInfo
      {
      TR_RegionStructure *_region;
      intptr_t _regionNum;
      intptr_t _arrayDataSize;
      bool _increasing;
      bool _branchToExit;
      bool _canMoveAsyncCheck;
      bool _needOffsetLoop;
      intptr_t _preOffset;
      intptr_t _postOffset;
      intptr_t _offset;
      intptr_t _stripLen;

      TR::Block *_preHeader;
      TR::Block *_loopTest;
      TR_PrimaryInductionVariable *_piv;
      TR::TreeTop *_asyncTree;

      TR_ScratchList<TR_ParentOfChildNode> _mainParentsOfLoads;
      TR_ScratchList<TR_ParentOfChildNode> _mainParentsOfStores;
      TR_ScratchList<TR_ParentOfChildNode> _residualParentsOfLoads;
      TR_ScratchList<TR_ParentOfChildNode> _residualParentsOfStores;

      TR_ScratchList<TR::CFGEdge> _edgesToRemove;
      };

   intptr_t _nodesInCFG;
   TR::TreeTop *_endTree;

   TR_ScratchList<LoopInfo> _loopInfos;
   TR::Block **_origBlockMapper;
   TR::Block **_mainBlockMapper;
   TR::Block **_preBlockMapper;
   TR::Block **_postBlockMapper;
   TR::Block **_residualBlockMapper;
   TR::Block **_offsetBlockMapper;

   /* methods */
   void collectLoops(TR_Structure *str);
   void transformLoops();

   void duplicateLoop(LoopInfo *li, TR_ClonedLoopType type);
   void transformLoop(LoopInfo *li);
   TR::Block *stripMineLoop(LoopInfo *li, TR::Block *outerHeader);

   TR::Block *createGotoBlock(TR::Block *source, TR::Block *dest);
   void redirect(TR::Block *source, TR::Block *oldDest, TR::Block *newDest);
   TR::Block *createLoopTest(LoopInfo *li, TR_ClonedLoopType type);
   TR::Block *createStartOffsetLoop(LoopInfo *li, TR::Block *outerHeader);

   void examineLoop(LoopInfo *li, TR_ClonedLoopType type, bool checkClone = true);
   void examineNode(LoopInfo *li, TR::Node *parent, TR::Node *node, TR::SymbolReference *oldSymRef,
                    vcount_t visitCount, TR_ClonedLoopType type, bool checkClone, int32_t childNum);
   void replaceLoopPivs(LoopInfo *li, TR::ILOpCodes newOpCode, TR::Node *newConst,
                        TR::SymbolReference *newSymRef, TR_ClonedLoopType type);

   TR::Block *getLoopPreHeader(TR_Structure *str);
   TR::Block *getLoopTest(TR_Structure *str, TR::Block *preHeader);
   bool checkIfIncrementalIncreasesOfPIV(LoopInfo *li);
   void findLeavesInList();
   bool findPivInSimpleForm(TR::Node *node,  TR::SymbolReference *pivSym);
   };
