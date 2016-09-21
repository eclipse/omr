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

#ifndef OMR_DEADTREESELIMATION_INCL
#define OMR_DEADTREESELIMATION_INCL

#include <stdint.h>                    // for int32_t, uint32_t
#include "env/TRMemory.hpp"            // for Allocator, TR_Memory, etc
#include "infra/List.hpp"              // for TR_ScratchList
#include "optimizer/Optimization.hpp"  // for Optimization

namespace TR { class Block; }
namespace TR { class DeadTreesElimination; }
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

// Removes dead code under treetops.
//
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

   protected:

   virtual TR::TreeTop *findLastTreetop(TR::Block *block, TR::TreeTop *prevTree);

   private:

   int32_t process(TR::TreeTop *, TR::TreeTop *);

   List<TreeInfo> _targetTrees;
   bool _cannotBeEliminated;
   bool _delayedRegStores;
   };

}

#endif
