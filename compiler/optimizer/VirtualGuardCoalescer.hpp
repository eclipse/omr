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

#ifndef VIRTUALGUARDCOALESCER_HPP
#define VIRTUALGUARDCOALESCER_HPP

#include <stddef.h>                           // for NULL
#include <stdint.h>                           // for uint32_t, int32_t, etc
#include "compile/Method.hpp"                 // for MAX_SCOUNT
#include "env/TRMemory.hpp"                   // for TR_Memory, etc
#include "il/Block.hpp"                       // for Block
#include "il/Node.hpp"                        // for Node
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"                     // for TreeTop
#include "il/TreeTop_inlines.hpp"             // for TreeTop::getNode
#include "infra/BitVector.hpp"                // for TR_BitVector
#include "infra/List.hpp"                     // for List
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

#define MALFORMED_GUARD MAX_SCOUNT

class TR_ValueNumberInfo;
namespace TR { class CFG; }
namespace TR { class Compilation; }
namespace TR { class SymbolReference; }

struct TR_SymNodePair {
   TR::SymbolReference *_symRef;
   TR::Node *_node;
   };

class TR_VirtualGuardTailSplitter : public TR::Optimization
   {
   public:
   TR_VirtualGuardTailSplitter(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_VirtualGuardTailSplitter(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:

   class VGInfo
      {
      public:
      TR_ALLOC(TR_Memory::VirtualGuardTailSplitter)

      VGInfo(TR::Block *branch, TR::Block *call, TR::Block *inlined,
             TR::Block *merge, VGInfo *parent)
         : _branch(branch), _call(call), _inlined(inlined),
           _merge(merge), _referenceCount(0), _valid(true)
            {
            if (!parent)
               _parent = this;
            else
               {
               _parent = parent;
               parent->addChild(this);
               }
            }

      VGInfo *getRoot()
            { return _parent == this?  this : _parent->getRoot(); }

      bool stillExists() { return _valid; }
      void markRemoved();
      bool isRoot() { return _parent == this; }
      bool isLeaf() { return _referenceCount == 0; }

      bool isNonTrivialRoot() { return _parent == this && _referenceCount != 0; }
      bool isNonTrivialLeaf() { return _parent != this && _referenceCount == 0; } // a leaf that is not a root

      void addChild(VGInfo *) { ++_referenceCount; }
      void removeChild(VGInfo *) { --_referenceCount; }

      bool isExceptionSafe() { return _call->getExceptionSuccessors().empty(); }

      TR::Block *getBranchBlock() { return _branch; }
      TR::Block *getMergeBlock() { return _merge; }
      TR::Block *getCallBlock() { return _call; }
      TR::Block *getFirstInlinedBlock() { return _inlined; }

      VGInfo *getParent() { return _parent; }

      scount_t getIndex() { return _branch->getLastRealTreeTop()->getNode()->getLocalIndex(); }
      uint32_t getNumber() { return _branch->getNumber(); }

      private:
      VGInfo   *_parent;
      TR::Block *_branch;
      TR::Block *_call;
      TR::Block *_inlined;
      TR::Block *_merge;

      uint8_t   _referenceCount;
      bool      _valid;
      };

   void initializeDataStructures();
   void splitLinear(TR::Block *start, TR::Block *end);

   void remergeGuard(TR_BlockCloner&, VGInfo *);

   bool    isBlockInInlinedCallTreeOf(VGInfo *, TR::Block *block);
   VGInfo *recognizeVirtualGuard(TR::Block *, VGInfo *parent);
   VGInfo *getVirtualGuardInfo(TR::Block *);

   TR::Block *lookAheadAndSplit(VGInfo *info, List<TR::Block> *stack);
   void transformLinear(TR::Block *first, TR::Block *last);

   bool isKill(TR::Block *);
   bool isKill(TR::Node  *);

   TR::Node *getFirstCallNode(TR::Block *block);

   TR::CFG *_cfg;

   // Virtual Guard Information Table and Accessor Methods
   //
   uint32_t  _numGuards;
   vcount_t  _visitCount;
   VGInfo *getGuard(uint32_t index)   { return index == MALFORMED_GUARD ? NULL : _table[index]; }
   VGInfo *getGuard(TR::Block *branch) { return getGuard(branch->getLastRealTreeTop()->getNode()); }
   VGInfo *getGuard(TR::Node  *branch) { return getGuard(branch->getLocalIndex()); }
   void    putGuard(uint32_t index, VGInfo *info);
   VGInfo **_table;
   bool _splitDone;
   };

class TR_InnerPreexistence : public TR::Optimization
   {
   public:
   TR_InnerPreexistence(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_InnerPreexistence(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   class GuardInfo
      {
      public:
      TR_ALLOC(TR_Memory::VirtualGuardTailSplitter)
      GuardInfo(TR::Compilation *, TR::Block *block, GuardInfo *parent, TR_ValueNumberInfo *vnInfo, uint32_t numInlinedSites);

      TR::Block *getBlock() { return _block; }
      TR::Node  *getGuardNode() { return _block->getLastRealTreeTop()->getNode(); }
      TR::Node  *getCallNode () { return getGuardNode()->getVirtualCallNodeForGuard(); }

      GuardInfo *getParent() { return _parent; }
      TR_BitVector *getArgVNs() { return _argVNs; }
      int32_t getThisVN() { return _thisVN; }

      void setVisible(uint32_t inner) { _innerSubTree->set(inner); }
      bool isVisible (uint32_t inner) { return _innerSubTree->isSet(inner); }
      TR_BitVector *getVisibleGuards() { return _innerSubTree; }

      void setHasBeenDevirtualized() { _hasBeenDevirtualized = true; }
      bool getHasBeenDevirtualized() { return _hasBeenDevirtualized; }


      private:
      GuardInfo    *_parent;
      TR::Block     *_block;
      uint32_t      _thisVN;
      TR_BitVector *_argVNs;
      bool          _hasBeenDevirtualized;
      TR_BitVector *_innerSubTree;
      };

   private:
   int32_t initialize();
   void    transform();
   void    devirtualize(GuardInfo *guard);

   int32_t     _numInlinedSites;
   GuardInfo **_guardTable;
   TR_ValueNumberInfo *_vnInfo;
   };

#endif
