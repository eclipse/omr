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
#include "infra/List.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_BasicInductionVariable;
class TR_PrimaryInductionVariable;
class TR_RegionStructure;
class TR_Structure;
namespace TR { class Block; }
namespace TR { class Node; }
namespace TR { class Optimization; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

class TR_PrefetchInsertion : public TR_LoopTransformer
   {
   public:
   TR_PrefetchInsertion(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_PrefetchInsertion(manager);
      }

   virtual int32_t perform();
   const char * optDetailString() const throw();

   private:
   struct ArrayAccessInfo
      {
      TR::TreeTop *_treeTop;
      TR::Node *_aaNode;
      TR::Node *_addressNode;
      TR::Node *_bivNode;
      TR_BasicInductionVariable *_biv;
      };

   TR_ScratchList<ArrayAccessInfo> _arrayAccessInfos;

   void collectLoops(TR_Structure *str);
   void insertPrefetchInstructions();
   TR::Node *createDeltaNode(TR::Node *node, TR::Node *pivNode, int32_t deltaOnBackEdge);
   void examineLoop(TR_RegionStructure *loop);
   void examineNode(TR::TreeTop *treeTop, TR::Block *block, TR::Node *node, intptr_t visitCount, TR_RegionStructure *loop);
   TR_PrimaryInductionVariable *getClosestPIV(TR::Block *block);
   bool isBIV(TR::SymbolReference* symRef, TR::Block *block, TR_BasicInductionVariable* &biv);
   };
