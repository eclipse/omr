/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef TR_DEADTREESELIMINATION_INCL
#define TR_DEADTREESELIMINATION_INCL

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"

namespace TR { class Block; }
namespace TR { class OptimizationManager; }
namespace TR { class TreeTop; }

namespace OMR
{

class TreeInfo
   {
   public:
   TR_ALLOC(TR_Memory::LocalOpts)

   TreeInfo(TR::TreeTop *treeTop, int32_t height)
      : _tree(treeTop),
        _height(height)
      {
      }

   TR::TreeTop *getTreeTop()  {return _tree;}
   void setTreeTop(TR::TreeTop *tree)  {_tree = tree;}

   int32_t getHeight()    {return _height;}
   void setHeight(int32_t height) {_height = height;}

   private:

   int32_t _height;
   TR::TreeTop *_tree;
   };

}


namespace TR
{

/*
 * Class DeadTreesElimination
 * ==========================
 *
 * Expressions that are evaluated and not used for a few instructions can 
 * actually be evaluated exactly where required (subject to the constraint 
 * that their values should be the same if evaluated later). This optimization 
 * attempts to simply delay evaluation of expressions and has the effect of 
 * reducing register pressure by shortening live ranges. It has the effect 
 * of removing some IL trees that are simply anchors for (already) evaluated 
 * expressions and making the trees more compact.
 */

class DeadTreesElimination : public TR::Optimization
   {
   public:

   DeadTreesElimination(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager);

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual bool fixUpTree(
		TR::Node *node,
		TR::TreeTop *treeTop,
		TR::NodeChecklist &visited,
		bool &highGlobalIndex,
		vcount_t evaluatedVisitCount);

   virtual const char * optDetailString() const throw();

   protected:

   virtual TR::TreeTop *findLastTreetop(TR::Block *block, TR::TreeTop *prevTree);

   private:

   int32_t process(TR::TreeTop *, TR::TreeTop *);

   bool _cannotBeEliminated;
   bool _delayedRegStores;
   };

}

#endif
