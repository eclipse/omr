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

#include "optimizer/Structure.hpp"

#include <algorithm>
#include <set>
#include <vector>
#include <limits.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "cs2/sparsrbit.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O STRUCTURE: "


// The "getKind" methods are made non-inline to identify this as the home for the
// vfts for this class hierarchy.
//
TR_Structure::Kind TR_Structure::getKind()                  {return Blank;}
TR_Structure::Kind TR_BlockStructure::getKind()             {return Block;}
TR_Structure::Kind TR_RegionStructure::getKind()            {return Region;}


TR_BlockStructure::TR_BlockStructure(TR::Compilation * comp, int32_t index, TR::Block *b) :
   TR_Structure(comp, index), _block(b)
   {
   b->setStructureOf(this);
   }


bool TR_BlockStructure::doDataFlowAnalysis(TR_DataFlowAnalysis *dfa, bool checkForChanges)
   {
   // LexicalTimer tlex("blockStructure_doDFA", comp()->phaseTimer());
   return dfa->analyzeBlockStructure(this, checkForChanges);
   }

bool TR_RegionStructure::doDataFlowAnalysis(TR_DataFlowAnalysis *dfa, bool checkForChanges)
   {
   // LexicalTimer tlex("regionStructure_doDFA", comp()->phaseTimer());
   return dfa->analyzeRegionStructure(this, checkForChanges);
   }

void TR_BlockStructure::clearAnalysisInfo()
   {
   setAnalysisInfo(NULL);
   }

void TR_RegionStructure::clearAnalysisInfo()
   {
   setAnalysisInfo(NULL);
   TR_RegionStructure::Cursor subs(*this);
   for (TR_StructureSubGraphNode *p = subs.getFirst(); p != NULL; p = subs.getNext())
      p->getStructure()->clearAnalysisInfo();
   }

bool TR_Structure::contains(TR_Structure *other, TR_Structure *commonParent)
   {
   for (TR_Structure *s = other; s != NULL && s != commonParent; s = s->getParent())
      if (s == this)
         return true;
   return false;
   }

TR_RegionStructure *TR_Structure::getContainingLoop()
   {
   for (TR_RegionStructure *p = getParent(); p != NULL ; p = p->getParent())
      if (p->isNaturalLoop())
         return p;
   return NULL;
   }

TR_RegionStructure *TR_Structure::getContainingCyclicRegion()
   {
   for (TR_RegionStructure *p = getParent(); p != NULL; p = p->getParent())
      {
      if (!p->isAcyclic())
         return p;
      }

   return NULL;
   }

// Note that if A contains B, the common parent is parent(A)
//
TR_RegionStructure *TR_Structure::findCommonParent(TR_Structure *other, TR::CFG *cfg)
   {
   for (TR_RegionStructure *parent = getParent()->asRegion();
        parent;
        parent = parent->getParent()->asRegion())
      {
      if (parent->contains(other))
         return parent;
      }
   TR_ASSERT(this == cfg->getStructure() ||
          other == cfg->getStructure(),
          "A and B MUST have a common ancestor unless either of them is region 0");
   return 0;
   }

void TR_BlockStructure::replacePart(TR_Structure *from, TR_Structure *to)
   {
   TR_ASSERT(0,"bad structure found trying to replace a substructure");
   }

void TR_RegionStructure::addSubNode(TR_StructureSubGraphNode *subNode)
   {
   _subNodes.push_back(subNode);
   TR_Structure *node = subNode->getStructure();
   node->setParent(this);
   }


void TR_RegionStructure::cleanupAfterEdgeRemoval(TR::CFGNode *node)
   {
   TR_BlockStructure *blockSt = toStructureSubGraphNode(node)->getStructure()->asBlock();
   if (node != getEntry() &&
       node->getPredecessors().empty() &&
       node->getExceptionPredecessors().empty())
      {
      // The node is now isolated and should be removed
      //
      if (node->getSuccessors().empty() &&
          node->getExceptionSuccessors().empty())
         {
         // The node is completely isolated. It can be removed from the
         // structure.
         //
         if (node->getNumber() != comp()->getFlowGraph()->getEnd()->getNumber())
            removeSubNode(toStructureSubGraphNode(node));
         }
      else
         {
         // The node is isolated but it still has successor edges.
         // It must be part of an unreachable cycle. Tell the CFG so that it
         // can run its pass to remove unreachable cycles later.
         //
         comp()->getFlowGraph()->setHasUnreachableBlocks();
         }
      }

   if (containsInternalCycles())
      checkForInternalCycles();
   else if ((numSubNodes() == 1) && !isNaturalLoop() &&
            !getEntry()->hasSuccessor(getEntry()) &&
            !getEntry()->hasExceptionSuccessor(getEntry()))
      {
      // This may be required if an acyclic region was created by
      // getting rid of the back edge of a natural loop (say with only one block);
      // in that case we want to collapse the entry into the parent
      //
      TR_RegionStructure::Cursor si(*this);
      TR_ASSERT(si.getFirst() == getEntry(),"bad structure found trying to remove a block");
      if (getParent() && (getEntry()->getStructure()->getParent() == this))
         getParent()->replacePart(this, getEntry()->getStructure());
      }
   }

// Implementation of TR_RegionStructure::extractUnconditionalExits() below
class TR_RegionStructure::ExitExtraction
   {
   public:
   ExitExtraction(TR::Compilation * const comp, TR::Region &memRegion)
      : _comp(comp)
      , _cfg(_comp->getFlowGraph())
      , _trace(_comp->getOption(TR_TraceExitExtraction))
      , _memRegion(memRegion)
      , _structureMemoryRegion(_cfg->structureMemoryRegion())
      , _workStack(_memRegion)
      , _queued(std::less<TR_Structure*>(), _memRegion)
      , _regionContents(RcCmp(), _memRegion)
      , _allPreds(_memRegion)
      , _succs(_memRegion)
      , _excSuccs(_memRegion)
      { }

   void extractUnconditionalExits(const TR::list<TR::Block*, TR::Region&> &blocks);

   private:
   typedef TR::typed_allocator<TR_Structure*, TR::Region&> StructureAlloc;
   typedef std::vector<TR_Structure*, StructureAlloc> StructureVec;
   typedef std::set<TR_Structure*, std::less<TR_Structure*>, StructureAlloc> StructureSet;

   typedef TR::typed_allocator<TR::CFGEdge*, TR::Region&> EdgeVecAlloc;
   typedef std::vector<TR::CFGEdge*, EdgeVecAlloc> EdgeVec;

   typedef std::pair<TR_RegionStructure* const, TR_BitVector> RcEntry;
   typedef TR::typed_allocator<RcEntry, TR::Region&> RcAlloc;
   typedef std::less<TR_RegionStructure*> RcCmp;
   typedef std::map<TR_RegionStructure*, TR_BitVector, RcCmp, RcAlloc> RegionContents;

   void enqueue(TR_Structure*);
   void collectWork(const TR::list<TR::Block*, TR::Region&> &blocks);
   void collectWorkFromRegion(TR_RegionStructure *region, const StructureSet &relevant);
   void extractStructure(TR_Structure*);
   TR_BitVector &regionContents(TR_RegionStructure *loop);
   void removeContentsFromRegion(TR_Structure *removed, TR_RegionStructure *region);
   void traceBitVector(TR_BitVector &bv);
   void moveNodeIntoParent(TR_StructureSubGraphNode *node, TR_RegionStructure *region, TR_RegionStructure *parent);
   void moveOutgoingEdgeToParent(TR_RegionStructure *region, TR_RegionStructure *parent, TR_StructureSubGraphNode *node, TR::CFGEdge *edge, bool isExceptionEdge);

   TR::Compilation * const _comp;
   TR::CFG * const _cfg;
   const bool _trace;

   TR::Region &_memRegion;
   TR::Region &_structureMemoryRegion;
   StructureVec _workStack;
   StructureSet _queued;
   RegionContents _regionContents;

   // Reuse these vectors to avoid allocating three new vectors on each
   // iteration of extractStructure().
   EdgeVec _allPreds;
   EdgeVec _succs;
   EdgeVec _excSuccs;
   };

/**
 * Identify and extract subgraph nodes that no longer belong within loops.
 *
 * After deleting edges from the control flow graph, there may be blocks, or
 * structure subgraph nodes more generally, from which execution is guaranteed
 * to exit a containing loop. A fresh structural analysis would consider such
 * nodes to be outside the loop, which is preferable for the sake of later loop
 * optimizations.
 *
 * Given a list of blocks from which outgoing edges have been deleted, try to
 * find these unconditionally exiting nodes, and extract them from as many
 * containing loops as possible.
 *
 * Nodes are extracted one at a time, and a given node is moved to its
 * outermost position before continuing on. When a node is moved out of a
 * region, its (ex-)predecessors might now directly exit the same region, so
 * they will also be considered.
 *
 * For example, suppose that an edge from \c block_7 to \c block_4 was removed,
 * leaving the following structure:
 *
   \verbatim
               0
               |
               v
               2
               |
               v
       +----(loop 3)------+
       |       v          |
       |  +--->3--+       |
       |  |    |  |       |
       |  |    |  |       |
       |  |    v  v       |
       |  +----4  5--->6  |
       |          |    |  |
       |          v    |  |
       |          7<---+  |
       |          |       |
       +----------|-------+
                  |
                  v
                  8
                  |
                  v
                  1
   \endverbatim
 *
 * First, 7 would be removed from the loop region:
 *
   \verbatim
               0
               |
               v
               2
               |
               v
       +----(loop 3)------+
       |       v          |
       |  +--->3--+       |
       |  |    |  |       |
       |  |    |  |       |
       |  |    v  v       |
       |  +----4  5--->6  |
       |          |    |  |
       +----------|----|--+
                  |    |
                  v    |
                  7<---+
                  |
                  v
                  8
                  |
                  v
                  1
   \endverbatim
 *
 * Then 6 would follow 7:
 *
   \verbatim
               0
               |
               v
               2
               |
               v
       +----(loop 3)------+
       |       v          |
       |  +--->3--+       |
       |  |    |  |       |
       |  |    |  |       |
       |  |    v  v       |
       |  +----4  5----+  |
       |          |    |  |
       +----------|----|--+
                  |    |
                  v    v
                  7<---6
                  |
                  v
                  8
                  |
                  v
                  1
   \endverbatim
 *
 * Finally, 5 would follow as well, leaving just 3 and 4 in the loop:
 *
   \verbatim
               0
               |
               v
               2
               |
               v
       +-(loop 3)-+
       |       v  |
       |  +--->3------>5--->6
       |  |    |  |    |    |
       |  |    |  |    v    |
       |  |    v  |    7<---+
       |  +----4  |    |
       |          |    v
       +----------+    8
                       |
                       v
                       1
   \endverbatim
 *
 * This extraction is not automatic as part of structure maintenance because it
 * would sometimes be triggered unexpectedly by intermediate states of the CFG.
 * Instead it must be requested explicitly by calling this method.
 *
 * JIT options:
 * - \c disableExitExtraction to disable
 * - \c traceExitExtraction to trace
 * - \c checkStructureDuringExitExtraction to \c checkStructure() in non-debug builds
 * - \c extractExitsByInvalidatingStructure for invalidation instead of repair
 *
 * \param comp    the compilation in progress
 * \param blocks  the blocks from which outgoing edges have been deleted
 *
 * Note that an unconditionally exiting node will only be discovered if it (or
 * a block it contains) is listed in \p blocks. If \c block_7 were absent from
 * \p blocks in the example above, the structure would be left unchanged.
 */
/* static */ void TR_RegionStructure::extractUnconditionalExits(
   TR::Compilation * const comp,
   const TR::list<TR::Block*, TR::Region&> &blocks)
   {
   if (blocks.empty())
      return;

   if (comp->getOption(TR_DisableExitExtraction))
      return;

   if (comp->getFlowGraph()->getStructure() == NULL)
      return;

   ExitExtraction xform(comp, comp->trMemory()->currentStackRegion());
   xform.extractUnconditionalExits(blocks);
   }

void TR_RegionStructure::ExitExtraction::extractUnconditionalExits(
   const TR::list<TR::Block*, TR::Region&> &blocks)
   {
   collectWork(blocks);
   if (_workStack.size() == 0)
      return;

   if (_trace)
      _comp->dumpMethodTrees("Trees before unconditional exit extraction");

   while (_workStack.size() > 0)
      {
      if (_trace)
         {
         traceMsg(_comp, "work stack:");
         for (auto s = _workStack.begin(); s != _workStack.end(); ++s)
            traceMsg(_comp, " %d:%p", (*s)->getNumber(), *s);

         traceMsg(_comp, "\n");
         }

      TR_Structure * const s = _workStack.back();
      _workStack.pop_back();
      _queued.erase(s);

      if (_trace)
         traceMsg(_comp, "attempting to extract %d:%p\n", s->getNumber(), s);

      extractStructure(s);

      if (_cfg->getStructure() == NULL)
         return;
      }
   }

void TR_RegionStructure::ExitExtraction::enqueue(TR_Structure * const s)
   {
   if (_trace)
      traceMsg(_comp, "enqueueing %d:%p\n", s->getNumber(), s);

   if (_queued.find(s) == _queued.end())
      {
      _workStack.push_back(s);
      _queued.insert(s);
      }
   }

void TR_RegionStructure::ExitExtraction::collectWork(
   const TR::list<TR::Block*, TR::Region&> &blocks)
   {
   StructureSet relevant(std::less<TR_Structure*>(), _memRegion);

   for (auto b = blocks.begin(); b != blocks.end(); ++b)
      {
      TR_Structure *s = (*b)->getStructureOf();
      while (s != NULL && relevant.find(s) == relevant.end())
         {
         TR_Structure * const parent = s->getParent();
         if (_trace)
            {
            traceMsg(_comp,
               "found relevant structure %d:%p, parent %d:%p\n",
               s->getNumber(),
               s,
               parent == NULL ? -1 : parent->getNumber(),
               parent);
            }

         relevant.insert(s);
         s = parent;
         }
      }

   auto * const root = _cfg->getStructure()->asRegion();
   if (root != NULL) // in case the root might sometimes be a block structure
      collectWorkFromRegion(root, relevant);
   }

// Get the structures to process in post-order. Since the pending work is
// treated as a stack, that means that regions will be processed before their
// contents. That's a good order because moving a node out of a region never
// makes that region movable, but it might make it immovable.
void TR_RegionStructure::ExitExtraction::collectWorkFromRegion(
   TR_RegionStructure *region,
   const StructureSet &relevant)
   {
   TR_RegionStructure::Cursor it(*region);
   for (auto node = it.getFirst(); node != NULL; node = it.getNext())
      {
      TR_Structure * const s = node->getStructure();
      if (relevant.find(s) == relevant.end())
         continue;

      if (s->asRegion() != NULL)
         collectWorkFromRegion(s->asRegion(), relevant);
      else
         enqueue(s);
      }

   enqueue(region);
   }

void TR_RegionStructure::ExitExtraction::extractStructure(
   TR_Structure * const initialStructure)
   {
   TR_RegionStructure *region = initialStructure->getParent();
   if (region == NULL)
      return; // ignore initialStructure because it has been detached

   TR_RegionStructure *loop = initialStructure->getContainingLoop();
   if (loop == NULL)
      return;

   TR_StructureSubGraphNode *node = region->subNodeFromStructure(initialStructure);
   for (;;)
      {
      TR_ASSERT_FATAL(
         node->getStructure()->getParent() == region,
         "removeUnconditionalExit: node %p not (directly) in region %p\n",
         node,
         region);

      if (!region->isNaturalLoop() && !region->isAcyclic())
         return;

      TR_RegionStructure * const parent = region->getParent();
      if (parent == NULL)
         return;

      // Try to find a remaining successor of node in this region.
      TR_SuccessorIterator sit(node);
      for (auto e = sit.getFirst(); e != NULL; e = sit.getNext())
         {
         if (!region->isExitEdge(e))
            return;
         }

      // None of this node's remaining successors are in this region, so it
      // unconditionally exits, and we can move it into the parent region.
      // However, we'd like to do so only if all of the remaining successors
      // are outside the innermost containing loop. Otherwise we'll spend time
      // dissolving acyclic regions, or worse, invalidate structure, for no
      // useful purpose.
      //
      // This test is delayed until we know that all outgoing edges are exit
      // edges in the hope that it will be unnecessary to build the set of node
      // numbers contained in the loop.
      //
      // Note that if (region == loop), then we already know that all
      // successors are outside loop, because they're outside region.
      if (region != loop)
         {
         TR_BitVector &loopBlocks = regionContents(loop);
         for (auto e = sit.getFirst(); e != NULL; e = sit.getNext())
            {
            if (loopBlocks.isSet(e->getTo()->getNumber()))
               return;
            }
         }

      // This node doesn't belong in the loop!
      if (!performTransformation(_comp,
            "%sMoving unconditional exit node %d "
            "from region %d:%p into parent %d:%p\n",
            OPT_DETAILS,
            node->getNumber(),
            region->getNumber(),
            region,
            parent->getNumber(),
            parent))
         return;

      if (_comp->getOption(TR_ExtractExitsByInvalidatingStructure))
         {
         // There is a node that would be extracted; just kill structure instead
         if (_trace)
            traceMsg(_comp, "invalidating structure instead of fixing it\n");

         // FIXME this should call invalidateStructure(), but doing so does not
         // clear out blocks' getStructureOf(), and not everything that looks
         // at getStructureOf() checks beforehand that structure is valid. So
         // at the moment, invalidateStructure() can cause subsequent
         // optimizations to dereference dangling pointers.
         _cfg->setStructure(NULL);
         return;
         }

      moveNodeIntoParent(node, region, parent);
      removeContentsFromRegion(initialStructure, region);

      if (_trace && _comp->getOutFile() != NULL)
         {
         traceMsg(_comp, "Structure after moving node into parent:\n<structure>\n");
         _comp->getDebug()->print(_comp->getOutFile(), _cfg->getStructure(), 0);
         traceMsg(_comp, "</structure>\n");
         }

#ifdef DEBUG
      const bool checkStructure = true;
#else
      const bool checkStructure =
         _comp->getOption(TR_CheckStructureDuringExitExtraction);
#endif
      if (checkStructure)
         {
         auto * const mem = _comp->trMemory();
         TR::StackMemoryRegion stackMemoryRegion(*mem);
         TR_BitVector blocks(_cfg->getNextNodeNumber(), mem, stackAlloc);
         _cfg->getStructure()->checkStructure(&blocks);
         }

      if (region == loop)
         {
         loop = loop->getContainingLoop();
         if (loop == NULL)
            return; // no more containing loops
         }

      region = parent;
      }
   }

TR_BitVector &TR_RegionStructure::ExitExtraction::regionContents(
   TR_RegionStructure * const region)
   {
   auto existing = _regionContents.find(region);
   if (existing != _regionContents.end())
      return existing->second;

   // Insert an empty bitvector into the map to avoid extra copying. It is
   // populated in-place below.
   auto new_entry = std::make_pair(region, TR_BitVector(_memRegion));
   TR_BitVector &contents = _regionContents.insert(new_entry).first->second;

   TR_RegionStructure::Cursor it(*region);
   for (auto node = it.getFirst(); node != NULL; node = it.getNext())
      {
      TR_Structure * const s = node->getStructure();
      if (s->asBlock() != NULL)
         contents.set(s->getNumber());
      else
         contents |= regionContents(s->asRegion());
      }

   if (_trace)
      {
      traceMsg(_comp, "contents of region %d:%p:", region->getNumber(), region);
      traceBitVector(contents);
      }

   return contents;
   }

void TR_RegionStructure::ExitExtraction::removeContentsFromRegion(
   TR_Structure * const removed,
   TR_RegionStructure * const region)
   {
   auto entry = _regionContents.find(region);
   if (entry == _regionContents.end())
      return;

   TR_BitVector &blocks = entry->second;
   if (removed->asBlock() != NULL)
      {
      blocks.reset(removed->getNumber());
      }
   else
      {
      auto subregionEntry = _regionContents.find(removed->asRegion());
      TR_ASSERT_FATAL(
         subregionEntry != _regionContents.end(),
         "region %d:%p has contents, "
         "but (previously) contained subregion %d:%p does not\n",
         region->getNumber(),
         region,
         removed->getNumber(),
         removed);

      blocks -= subregionEntry->second;
      }

   if (_trace)
      {
      traceMsg(_comp,
         "adjusted contents of region %d:%p:",
         region->getNumber(),
         region);

      traceBitVector(blocks);
      }
   }

void TR_RegionStructure::ExitExtraction::traceBitVector(TR_BitVector &bv)
   {
   TR_BitVectorIterator bvi(bv);
   while (bvi.hasMoreElements())
      traceMsg(_comp, " %d", bvi.getNextElement());

   traceMsg(_comp, "\n");
   }

void TR_RegionStructure::ExitExtraction::moveNodeIntoParent(
   TR_StructureSubGraphNode * const node,
   TR_RegionStructure * const region,
   TR_RegionStructure * const parent)
   {
   if (node == region->getEntry())
      {
      // Replace this entire region in the parent, since entry can't be removed
      TR_ASSERT_FATAL(
         region->numSubNodes() == 1,
         "removeUnconditionalExit: all successors of region %p entry are "
         "outside region, but there are additional sub-nodes\n",
         region);

      parent->replacePart(region, node->getStructure());
      return;
      }

   // Copy the original predecessors and successors
   _allPreds.clear();
      {
      TR_PredecessorIterator pit(node);
      for (auto e = pit.getFirst(); e != NULL; e = pit.getNext())
         _allPreds.push_back(e);
      }

   _succs.clear();
      {
      TR::CFGEdgeList &l = node->getSuccessors();
      _succs.insert(_succs.end(), l.begin(), l.end());
      }

   _excSuccs.clear();
      {
      TR::CFGEdgeList &l = node->getExceptionSuccessors();
      _excSuccs.insert(_excSuccs.end(), l.begin(), l.end());
      }

   // Remove all incoming edges
   for (auto e = _allPreds.begin(); e != _allPreds.end(); ++e)
      {
      region->removeEdgeWithoutCleanup(*e, false);
      if (_trace)
         {
         traceMsg(_comp,
            "removed edge (%d->%d):%p from region %d:%p\n",
            (*e)->getFrom()->getNumber(),
            (*e)->getTo()->getNumber(),
            *e,
            region->getNumber(),
            region);
         }
      }

   // Move node into parent
   region->removeSubNodeWithoutCleanup(node);
   parent->addSubNode(node);
   if (_trace)
      traceMsg(_comp, "moved node into parent\n");

   // For each edge that was incoming, region has an exit edge
   const bool nodeIsHandler =
      node->getStructure()->getEntryBlock()->isCatchBlock();

   for (auto e = _allPreds.begin(); e != _allPreds.end(); ++e)
      {
      auto * const src = toStructureSubGraphNode((*e)->getFrom());
      region->addExitEdge(src, node->getNumber(), nodeIsHandler);
      if (_trace)
         {
         traceMsg(_comp,
            "added exit edge (%d->%d) to region %d:%p\n",
            src->getNumber(),
            node->getNumber(),
            region->getNumber(),
            region);
         }
      }

   // Parent needs an edge from region to node
   auto * const regionNode = parent->subNodeFromStructure(region);
   if (nodeIsHandler)
      TR::CFGEdge::createExceptionEdge(regionNode, node, _structureMemoryRegion);
   else
      TR::CFGEdge::createEdge(regionNode, node, _structureMemoryRegion);

   if (_trace)
      {
      traceMsg(_comp,
         "added %sedge (%d->%d) to region %d:%p\n",
         nodeIsHandler ? "exception " : "",
         regionNode->getNumber(),
         node->getNumber(),
         parent->getNumber(),
         parent);
      }

   // Replace all outgoing (exit) edges with edges originating in parent
   for (auto e = _succs.begin(); e != _succs.end(); ++e)
      moveOutgoingEdgeToParent(region, parent, node, *e, false);

   for (auto e = _excSuccs.begin(); e != _excSuccs.end(); ++e)
      moveOutgoingEdgeToParent(region, parent, node, *e, true);

   region->cleanupAfterNodeRemoval();

   // Cleanup may have eliminated the current region.
   if (region->getParent() == NULL)
      {
      if (_trace)
         {
         traceMsg(_comp,
            "region %d:%p was eliminated by cleanupAfterNodeRemoval\n",
            region->getNumber(),
            region);
         }

      return;
      }

   TR_ASSERT_FATAL(
      region->getParent() == parent,
      "removeUnconditionalExit: region %p parent changed unexpectedly "
      "from %p to %p\n",
      region,
      parent,
      region->getParent());

   // Each of the predecessors had edges removed within the subgraph.
   // They've been replaced by exit edges, but these nodes may now
   // themselves be unconditional exits from the region, so enqueue them.
   for (auto e = _allPreds.begin(); e != _allPreds.end(); ++e)
      {
      auto * const src = toStructureSubGraphNode((*e)->getFrom());
      region->cleanupAfterEdgeRemoval(src);
      enqueue(src->getStructure());
      }
   }

void TR_RegionStructure::ExitExtraction::moveOutgoingEdgeToParent(
   TR_RegionStructure * const region,
   TR_RegionStructure * const parent,
   TR_StructureSubGraphNode * const node,
   TR::CFGEdge * const edge,
   bool isExceptionEdge)
   {
   TR_ASSERT_FATAL(
      region->isExitEdge(edge),
      "moveOutgoingEdgeToParent: unconditional exit %p node has "
      "non-exit edge %p outgoing\n",
      node,
      edge);

   TR_ASSERT_FATAL(
      toStructureSubGraphNode(edge->getFrom()) == node,
      "moveOutgoingEdgeToParent: expected edge %p to originate from node %p\n",
      edge,
      node);

   auto * const exitTgt = toStructureSubGraphNode(edge->getTo());
   const int32_t tgtNum = exitTgt->getNumber();

   region->removeEdgeWithoutCleanup(edge, /* isExitEdge = */ true);
   if (_trace)
      {
      traceMsg(_comp,
         "removed exit edge (%d->%d):%p from region %d:%p\n",
         edge->getFrom()->getNumber(),
         edge->getTo()->getNumber(),
         edge,
         region->getNumber(),
         region);
      }

   // Determine whether region still exits to tgtNum
   bool regionStillHasThisExit = false;
   ListIterator<TR::CFGEdge> rExits(&region->getExitEdges());
   for (auto exit = rExits.getFirst(); exit != NULL; exit = rExits.getNext())
      {
      if (exit->getTo()->getNumber() == tgtNum)
         {
         regionStillHasThisExit = true;
         break;
         }
      }

   // In case region no longer exits to tgtNum, we have to remove the
   // corresponding edge in parent (which may itself be an exit from parent)
   if (!regionStillHasThisExit)
      {
      // Find the edge in parent outgoing from region that corresponds to this
      // (now stale) exit from region.
      auto * const regionNode = parent->subNodeFromStructure(region);
      TR::CFGEdge *staleEdge = NULL;
      TR_SuccessorIterator sit(regionNode);
      for (auto e = sit.getFirst(); e != NULL; e = sit.getNext())
         {
         if (e->getTo()->getNumber() == tgtNum)
            {
            staleEdge = e;
            break;
            }
         }

      TR_ASSERT_FATAL(
         staleEdge != NULL,
         "moveOutgoingEdgeToParent: unable to find parent %p edge for "
         "stale exit from region %p to %d\n",
         parent,
         region,
         tgtNum);

      // Remove the stale edge. NOTE: If this edge is an exit from parent, it
      // may seem appropriate to check whether it too was the last such exit,
      // and if so, continue removing edges in the parent of parent, and so
      // forth toward the root. However, in that case parent will definitely
      // have an exit edge from node to tgtNum, to be created below.
      parent->removeEdgeWithoutCleanup(staleEdge, parent->isExitEdge(staleEdge));
      if (_trace)
         {
         traceMsg(_comp,
            "original region %d:%p no longer exits to %d - "
            "removed corresponding exit from parent\n",
            region->getNumber(),
            region,
            tgtNum);
         }
      }

   auto * const redirect = parent->findSubNodeInRegion(tgtNum);
   if (redirect == NULL)
      {
      // This is also an exit from parent.
      parent->addExitEdge(node, tgtNum, isExceptionEdge);
      if (_trace)
         {
         traceMsg(_comp,
            "successor %d does not exist in parent - created new exit %sedge\n",
            tgtNum,
            isExceptionEdge ? "exception " : "");
         }
      }
   else
      {
      if (isExceptionEdge)
         TR::CFGEdge::createExceptionEdge(node, redirect, _structureMemoryRegion);
      else
         TR::CFGEdge::createEdge(node, redirect, _structureMemoryRegion);

      if (_trace)
         {
         traceMsg(_comp,
            "parent region contains %d - created internal %sedge\n",
            tgtNum,
            isExceptionEdge ? "exception " : "");
         }
      }
   }

void TR_RegionStructure::cleanupAfterNodeRemoval()
   {
   // If the region now consists of a single node, replace this node in its
   // parent by the subnode.
   //
   // If there is no parent, this node has already been removed from its parent
   // region so don't bother replacing it.
   //
   if ((numSubNodes() == 1) && !isNaturalLoop() &&
       !getEntry()->hasSuccessor(getEntry()) &&
       !getEntry()->hasExceptionSuccessor(getEntry()))
      {
      TR_RegionStructure::Cursor si(*this);
      TR_ASSERT(si.getFirst() == getEntry(),"bad structure found trying to remove a block");

      auto parent = getParent();
      if (!parent)
         return;

      auto structure = getEntry()->getStructure();
      if (!structure)
         {
         collapseIntoParent();
         return;
         }

      parent->replacePart(this, structure);
      }
   }

void TR_RegionStructure::removeSubNodeWithoutCleanup(TR_StructureSubGraphNode *node)
   {
   for (auto itr = _subNodes.begin(), end = _subNodes.end(); itr != end; ++itr)
      {
      if (*itr == node)
         {
         _subNodes.erase(itr);
         break;
         }
      }
   node->getStructure()->setParent(NULL);
   }

void TR_RegionStructure::removeSubNode(TR_StructureSubGraphNode *node)
   {
   removeSubNodeWithoutCleanup(node);
   cleanupAfterNodeRemoval();
   }

TR_StructureSubGraphNode *TR_RegionStructure::findSubNodeInRegion(int32_t num)
   {
   TR_RegionStructure::Cursor it(*this);
   for (TR_StructureSubGraphNode *node = it.getCurrent(); node; node = it.getNext())
      if (node->getNumber() == num)
         return node;
   return 0;
   }

TR_StructureSubGraphNode *TR_RegionStructure::subNodeFromStructure(
   TR_Structure * const structure)
   {
   const int32_t num = structure->getNumber();
   TR_StructureSubGraphNode * const node = findSubNodeInRegion(num);
   TR_ASSERT_FATAL(
      node != NULL && node->getStructure() == structure,
      "subNodeFromStructure: in region %p, "
      "expected node %d to have structure %p, but found %p\n",
      this,
      num,
      structure,
      node->getStructure());

   return node;
   }

void TR_RegionStructure::replacePart(TR_Structure *from, TR_Structure *to)
   {
   TR_ASSERT(from != NULL, "target for structure replacement is null");
   TR_ASSERT(to != NULL, "replacement for structure %d is null", from->getNumber());

   // Find the sub-graph node for the sub-structure
   //
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (node->getStructure() == from)
         break;
      }

   TR_ASSERT(node != NULL, "structure %d doesn't include replacee node %d", _nodeIndex, from->getNumber());
   TR_ASSERT(node != getEntry() || _nodeIndex == to->getNumber(), "cannot replace the entry node in %d with node %d", _nodeIndex, to->getNumber());
   node->setStructure(to);
   to->setParent(this);
   from->setParent(NULL); // The replaced structure is now detached

   // If the node number has changed, replace the exit part in all predecessors
   // that are themselves regions.
   //
   if (from->getNumber() != to->getNumber())
      {
      for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
         {
         TR_RegionStructure *pred = toStructureSubGraphNode((*edge)->getFrom())->getStructure()->asRegion();
         if (pred)
            pred->replaceExitPart(from->getNumber(), to->getNumber());
         }
      for (auto edge = node->getExceptionPredecessors().begin(); edge != node->getExceptionPredecessors().end(); ++edge)
         {
         TR_RegionStructure *pred = toStructureSubGraphNode((*edge)->getFrom())->getStructure()->asRegion();
         if (pred)
            pred->replaceExitPart(from->getNumber(), to->getNumber());
         }
      }
   }

void TR_RegionStructure::replaceExitPart(int32_t fromNumber, int32_t toNumber)
   {
   // Find the sub-graph node for the exit node and change its number.
   // Allow for multiple exit nodes with the same number.
   //
   bool haveReplaced = false;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   TR::CFGEdge *edge;

   // First see if there are any region subnodes that exit to the node
   // being replaced. If so, their exit edges have to be fixed up too.
   //
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      if (edge->getTo()->getNumber() == fromNumber)
         {
         TR_RegionStructure *pred = toStructureSubGraphNode(edge->getFrom())->getStructure()->asRegion();
         if (pred)
            pred->replaceExitPart(fromNumber, toNumber);
         }
      }

   // Now change the number on the relevant exit node(s)
   //
   ei.reset();
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {

      ///////if (!haveReplaced)
         {
         if (edge->getTo()->getNumber() == fromNumber)
            {
            edge->getTo()->setNumber(toNumber);
            haveReplaced = true;
            }
         }
      }
   TR_ASSERT(haveReplaced,"bad structure found trying to replace a substructure");
   }




void TR_RegionStructure::collectExitBlocks(List<TR::Block> *exitBlocks, List<TR::CFGEdge> *exitEdges)
   {
   TR_BitVector *seenNodes = new (trStackMemory()) TR_BitVector(1, trMemory(), stackAlloc, growable);
   TR::CFGEdge *edge;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      if (!seenNodes->get(edge->getFrom()->getNumber()))
         {
         seenNodes->set(edge->getFrom()->getNumber());
         TR_Structure *s = edge->getFrom()->asStructureSubGraphNode()->getStructure();
         int32_t toNum = edge->getTo()->getNumber();
         s->collectExitBlocks(exitBlocks);
         if (s->asBlock() && exitEdges)
            {
            TR::Block *exitBlock = s->asBlock()->getBlock();
            TR_SuccessorIterator exitIt(exitBlock);
            for (TR::CFGEdge * exitEdge = exitIt.getFirst(); exitEdge != NULL; exitEdge = exitIt.getNext())
               {
               if (toNum == exitEdge->getTo()->getNumber())
                  exitEdges->add(exitEdge);
               }
            }
         }
      }
   }


void TR_BlockStructure::collectExitBlocks(List<TR::Block> *exitBlocks, List<TR::CFGEdge> *exitEdges)
   {
   exitBlocks->add(getBlock());
   }


void TR_RegionStructure::addGlobalRegisterCandidateToExits(TR::RegisterCandidate *inductionCandidate)
   {
   TR_ScratchList<TR::Block> exitBlocks(trMemory());
   collectExitBlocks(&exitBlocks);

   TR::Block *nextBlock;
   ListIterator<TR::Block> bi(&exitBlocks);
   for (nextBlock = bi.getFirst(); nextBlock; nextBlock = bi.getNext())
      {
      int32_t blockWeight = 1;
      if (nextBlock->getStructureOf())
         {
         nextBlock->getStructureOf()->calculateFrequencyOfExecution(&blockWeight);
         inductionCandidate->addBlock(nextBlock, blockWeight);
         }
      }
   }

static bool findCycleDEPRECATED(TR_StructureSubGraphNode *node, TR_BitVector &regionNodes, TR_BitVector &nodesSeenOnPath, TR_BitVector &nodesCleared, int32_t entryNode)
   {
   if (nodesSeenOnPath.get(node->getNumber()))
      return true;             // An internal cycle found
   if (nodesCleared.get(node->getNumber()))
      return false;            // This node is already known not to be in a cycle

   nodesSeenOnPath.set(node->getNumber());

   for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
      {
      TR_ASSERT((*edge)->getTo()->asStructureSubGraphNode(),"Expecting a CFG node which can be downcast to StructureSubGraphNode");
      TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
      if (succ->getNumber() != entryNode && regionNodes.get(succ->getNumber()) &&
          findCycleDEPRECATED(succ,regionNodes,nodesSeenOnPath,nodesCleared,entryNode))
         return true;
      }
   for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
      {
      TR_ASSERT((*edge)->getTo()->asStructureSubGraphNode(),"Expecting a CFG node which can be downcast to StructureSubGraphNode");
      TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
      if (/* succ->getNumber() != entryNode && */ regionNodes.get(succ->getNumber()) &&
          findCycleDEPRECATED(succ,regionNodes,nodesSeenOnPath,nodesCleared,entryNode))
         return true;
      }

   nodesSeenOnPath.reset(node->getNumber());
   nodesCleared.set(node->getNumber());
   return false;
   }

   /*
    * findCycle is a depth first traversal of the graph in search for a cycle.
    * Returns true if a cycle is found, otherwise returns false.
    */
static bool findCycle(TR::Compilation *comp, TR_StructureSubGraphNode *origin, TR_BitVector &regionNodes, int32_t entryNode)
   {
   TR::StackMemoryRegion memRegion(*(comp->trMemory()));

   int32_t numNodes = comp->getFlowGraph()->getNextNodeNumber();

   /*
    * First element of the pair is the subgraph node being looked at.
    * Second element of the pair is a bool that indicates if the node's children have already been processed.
    */
   TR::deque<std::pair<TR_StructureSubGraphNode *, bool>, TR::Region&> nodeStack(0, memRegion);

   /*
    * nodesDiscovered is a bit vector that tracks the path of subgraph nodes from origin to the node being looked at.
    * If a node is set to Discovered and its next child to examine is also set to Discovered, a cycle has been found.
    * Processed flag supersedes Discovered flag. If the node is marked as Processed, it counts as being in the Processed state.
    */
   TR_BitVector nodesDiscovered(numNodes, memRegion);

   /*
    * nodesProcessed is a bit vector that tracks which subgraph nodes have been confirmed to not be part of a cycle.
    * If a node is set to Processed, both that node and its children have already been looked at and a cycle was not found.
    */
   TR_BitVector nodesProcessed(numNodes, memRegion);

   /*
    * Two pairs for each node are added to nodeStack.
    * The first one pushed has childrenProcessed set to true and the second one has it set to false.
    * Note that this is a stack so the pair where childrenProcessed is false is pushed second but handled first.
    * When the node where childrenProcessed is false is being handled, that node's children will be added to nodeStack.
    * After all the children have been handled and set to Processed the node on the top of the stack will have childrenProcessed
    * set to true which sets the node to being Processed.
    */
   nodeStack.push_front(std::make_pair(origin,true));
   nodeStack.push_front(std::make_pair(origin,false));

   while (!nodeStack.empty())
      {
      TR_StructureSubGraphNode *node = nodeStack.front().first;
      int32_t nodeNum = node->getNumber();
      bool childrenProcessed = nodeStack.front().second;
      nodeStack.pop_front();

      /*
       * If all children are set to processed and a cycle was not found, the current node is also not part of a cycle
       * and should be set to processed as well.
       */
      if (childrenProcessed)
         {
         nodesProcessed.set(nodeNum);
         continue;
         }

      /*
       * If the current node is set to processed than it is already known to NOT be part of a cycle.
       */
      if (nodesProcessed.get(nodeNum))
         {
         continue;
         }

      /*
       * If the current node is set to Discovered then that means it was seen before as part of the path between origin and the current
       * node. This means a cycle has been found. findCycle can end immediately and return true.
       *
       * If the current node is not set to Discovered, set it to Discovered to indicate it is now part of the path back to origin.
       */
      if (nodesDiscovered.get(nodeNum))
         {
         return true;
         }
      else
         {
         nodesDiscovered.set(nodeNum);
         }

      /*
       * Iterate over the children nodes and add them to nodeStack.
       * Similar to before, two pairs for each node are added to nodeStack.
       * The first one pushed has childrenProcessed set to true and the second one has it set to false.
       * This is a stack so the pair where childrenProcessed is false is pushed second but handled first.
       */
      for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         int32_t succNumber = succ->getNumber();

         if (regionNodes.get(succNumber))
            {
            nodeStack.push_front(std::make_pair(succ, true));
            nodeStack.push_front(std::make_pair(succ, false));
            }
         }

      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         int32_t succNumber = succ->getNumber();

         if (succNumber != entryNode && regionNodes.get(succNumber))
            {
            nodeStack.push_front(std::make_pair(succ, true));
            nodeStack.push_front(std::make_pair(succ, false));
            }
         }
      }

   return false;
   }


void TR_RegionStructure::checkForInternalCycles()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t numNodes = comp()->getFlowGraph()->getNextNodeNumber();
   TR_BitVector regionNodes(numNodes, stackMemoryRegion);
   for (auto itr = _subNodes.begin(), end = _subNodes.end(); itr != end; ++itr)
      regionNodes.set((*itr)->getNumber());

   /*
    * findCycleDEPRECATED is the old recursive implentation while findCycle is the new iterative implementation.
    * findCycleDEPRECATED is still available via the TR_UseOldFindCycle envvar but will be removed in a future release.
    */
   static bool useOldFindCycle = (feGetEnv("TR_UseOldFindCycle") != NULL);

   if (!useOldFindCycle)
      {
      setContainsInternalCycles(findCycle(comp(), getEntry(), regionNodes, getNumber()));
      }
   else
      {
      TR_BitVector nodesSeenOnPath(numNodes, stackMemoryRegion);
      TR_BitVector nodesCleared(numNodes, stackMemoryRegion);
      setContainsInternalCycles(findCycleDEPRECATED(getEntry(), regionNodes, nodesSeenOnPath, nodesCleared, getNumber()));
      }
   }

bool TR_RegionStructure::hasExceptionOutEdges()
   {
   ListIterator<TR::CFGEdge> ei(&getExitEdges());
   for (TR::CFGEdge *edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
      {
      TR_StructureSubGraphNode *edgeTo = toStructureSubGraphNode(edge->getTo());
      if (!edgeTo->getExceptionPredecessors().empty())
         return true;
      }
   return false;
   }

void TR_BlockStructure::removeEdge(TR_Structure *from, TR_Structure *to)
   {
   TR_ASSERT(to == this,"bad structure trying to remove a block");

   // If this structure has no parent, it must represent the exit block. In this
   // case there is no structure edge to remove.
   //
   TR_Structure *parent = getParent();

   // TR_ASSERT(parent != NULL || comp()->getFlowGraph()->getEnd() == _block,
   //       "bad structure (no parent) trying to remove a block");

   if (parent)
      parent->removeEdge(from, to);
   }

void TR_RegionStructure::removeEdgeWithoutCleanup(TR::CFGEdge *edge, bool isExitEdge)
   {
   // Remove this edge from the region's subgraph
   //
   TR::CFGNode *fromNode = edge->getFrom();
   TR::CFGNode *toNode   = edge->getTo();
   if(std::find(fromNode->getSuccessors().begin(), fromNode->getSuccessors().end(), edge) != fromNode->getSuccessors().end())
      {
      fromNode->getSuccessors().remove(edge);
      TR_ASSERT((std::find(toNode->getPredecessors().begin(), toNode->getPredecessors().end(), edge) != toNode->getPredecessors().end())
                ,"bad structure found trying to remove an out edge");
      toNode->getPredecessors().remove(edge);
      }
   else
      {
      TR_ASSERT((std::find(fromNode->getExceptionSuccessors().begin(), fromNode->getExceptionSuccessors().end(), edge) != fromNode->getExceptionSuccessors().end())
                ,"bad structure found trying to remove an out edge");
      fromNode->getExceptionSuccessors().remove(edge);
      TR_ASSERT((std::find(toNode->getExceptionPredecessors().begin(), toNode->getExceptionPredecessors().end(), edge) != toNode->getExceptionPredecessors().end())
                ,"bad structure found trying to remove an out edge");
      toNode->getExceptionPredecessors().remove(edge);
      }

   if (isExitEdge)
      _exitEdges.remove(edge);
   }

void TR_RegionStructure::removeEdge(TR::CFGEdge *edge, bool isExitEdge)
   {
   removeEdgeWithoutCleanup(edge, isExitEdge);

   bool cleanupAlreadyDone = false;
   if (!isExitEdge)
      {
      cleanupAfterEdgeRemoval(edge->getTo());
      if (edge->getTo() == edge->getFrom())
         cleanupAlreadyDone = true;
      }

   if (!cleanupAlreadyDone)
      cleanupAfterEdgeRemoval(edge->getFrom());
   }

void TR_RegionStructure::removeEdge(TR_Structure *from, TR_Structure *to)
   {
   // Find the sub-graph nodes for the "to" and possibly the "from" structure.
   //
   TR_StructureSubGraphNode *fromNode = NULL, *toNode = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (TR_StructureSubGraphNode *node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (fromNode == NULL && node->getStructure()->contains(from, this))
         {
         fromNode = node;
         if (toNode != NULL)
            break;
         }
      if (toNode == NULL && node->getNumber() == to->getNumber())
         {
         toNode = node;
         if (fromNode != NULL)
            break;
         }
      }

   // If the toNode has already been removed from this structure
   // don't do anything.
   //
   if (toNode == NULL)
       return;

   // If the edge to be removed is one coming into this region, let the parent
   // handle it.
   //
   if (fromNode == NULL)
      {
      TR_ASSERT(toNode == getEntry(),"bad structure found trying to remove a block");
      if (getParent())
         getParent()->removeEdge(from, to);
      return;
      }

   // The edge is internal. See if it is a regular edge or an exception edge.
   //
   int removedEdgeType = fromNode->getStructure()->removeExternalEdgeTo(from, to->getNumber());

   if (removedEdgeType == NO_MORE_EDGES_EXIST)
      {
      // Find the edge to be removed
      //
      auto edge = fromNode->getSuccessors().begin();
      for (; edge != fromNode->getSuccessors().end(); ++edge)
         if ((*edge)->getTo()->getNumber() == to->getNumber())
            break;

      if (edge == fromNode->getSuccessors().end())
         {
         edge = fromNode->getExceptionSuccessors().begin();
         for (; edge != fromNode->getExceptionSuccessors().end(); ++edge)
            if ((*edge)->getTo()->getNumber() == to->getNumber())
               break;

         if(edge == fromNode->getExceptionSuccessors().end())
            TR_ASSERT(false, "bad structure found trying to remove a block");
         }

        removeEdge(*edge, false);
      }
   }

int TR_BlockStructure::removeExternalEdgeTo(TR_Structure *from, int32_t toNumber)
   {
   TR_ASSERT(getNumber() == from->getNumber(),"bad structure found trying to remove an out edge");

   return NO_MORE_EDGES_EXIST;
   }

int TR_RegionStructure::removeExternalEdgeTo(TR_Structure *from, int32_t toNumber)
   {
   // Find the node containing the from structure
   //
   TR_StructureSubGraphNode *node = NULL;
   TR_RegionStructure::Cursor ni(*this);
   for (TR::CFGNode *n = ni.getCurrent(); n; n = ni.getNext())
      {
      node = toStructureSubGraphNode(n);
      if (node->getStructure()->contains(from, this))
         break;
      }
   TR_ASSERT(node != NULL,"bad structure found trying to remove an out edge");

   int removedEdgeType;
   TR_Structure *subNode = node->getStructure();

   // Find the exit edge to be removed.
   //
   TR::CFGEdge *edge;
   TR::CFGEdge *relevantEdge = NULL;
   TR::CFGNode *toNode = NULL;
   bool oneExitEdge = false, moreThanOneExitEdgeToSameToNumber = false;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
      {
      if (edge->getTo()->getNumber() == toNumber)
         {
         if (!toNode && edge->getFrom() == node)
            {
            toNode = edge->getTo();
            relevantEdge = edge;
            }

         if (oneExitEdge)
            moreThanOneExitEdgeToSameToNumber = true;
         oneExitEdge = true;
         }
      }

   TR_ASSERT(relevantEdge != NULL,"bad structure found trying to remove an out edge");

   removedEdgeType = subNode->removeExternalEdgeTo(from, toNumber);
   if (removedEdgeType == NO_MORE_EDGES_EXIST)
      {
      // Remove the edge
      //
      removeEdge(relevantEdge, true);

      if (moreThanOneExitEdgeToSameToNumber)
         removedEdgeType = EDGES_STILL_EXIST;
      else
         {
         if (!toNode->getExceptionPredecessors().empty() ||
             !toNode->getPredecessors().empty())
            removedEdgeType = EDGES_STILL_EXIST;
         }
      }

   return removedEdgeType;
   }


void TR_Structure::setNestingDepths(int32_t *currentNestingDepth)
   {
   if (getParent())
      getParent()->setNestingDepths(currentNestingDepth);

   TR_RegionStructure *region = asRegion();
   if (region && region->isNaturalLoop())
      *currentNestingDepth = (*currentNestingDepth) + 1;

   setNestingDepth(*currentNestingDepth);
   }

void TR_Structure::setAnyCyclicRegionNestingDepths(int32_t *currentNestingDepth)
   {
   if (getParent())
      getParent()->setAnyCyclicRegionNestingDepths(currentNestingDepth);

   TR_RegionStructure *region = asRegion();
   if (region && region->containsInternalCycles())
      *currentNestingDepth = (*currentNestingDepth) + 1;

   setAnyCyclicRegionNestingDepth(*currentNestingDepth);
   }


int32_t TR_Structure::getMaxNestingDepth(int32_t *currentNestingDepth, int32_t *currentMaxNestingDepth)
   {
   return *currentMaxNestingDepth;
   }


int32_t TR_RegionStructure::getMaxNestingDepth(int32_t *currentNestingDepth, int32_t *currentMaxNestingDepth)
   {
   bool isLoop = isNaturalLoop();
   if (isLoop)
      *currentNestingDepth = (*currentNestingDepth) + 1;

   if (*currentNestingDepth > *currentMaxNestingDepth)
       *currentMaxNestingDepth = *currentNestingDepth;

   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      subStruct->getMaxNestingDepth(currentNestingDepth, currentMaxNestingDepth);
      }

   if (isLoop)
      *currentNestingDepth = (*currentNestingDepth) - 1;

   return *currentMaxNestingDepth;
   }



void TR_Structure::calculateFrequencyOfExecution(int32_t *currentWeight)
   {
   if (getParent())
      getParent()->calculateFrequencyOfExecution(currentWeight);

   TR_RegionStructure *region = asRegion();
   if (region && !region->isAcyclic() && (*currentWeight < (INT_MAX/10)))                // careful: don't overflow 32b int!
      *currentWeight = (*currentWeight)*10;
   }


void TR_Structure::setConditionalityWeight(int32_t *currentWeight)
   {
   TR_RegionStructure *region = asRegion();
   bool isAcyclicRegion = false;
   if (region &&
       (!region->isAcyclic() ||
        (region == comp()->getFlowGraph()->getStructure())))
      {
      adjustWeightForBranches(region->getEntry(), region->getEntry(), currentWeight);
      }
   else if (region->isAcyclic())
      isAcyclicRegion = true;

   if (isAcyclicRegion &&
       getParent())
      getParent()->setConditionalityWeight(currentWeight);
   }



void TR_Structure::adjustWeightForBranches(TR_StructureSubGraphNode *node, TR_StructureSubGraphNode *entry, int32_t *currentWeight)
   {
   int32_t startingWeight = *currentWeight;
   if (node->getPredecessors().size() > 1)
      startingWeight = 10*startingWeight/9;

   int32_t weight = startingWeight;

   if (node->getStructure() &&
       (weight > node->getStructure()->getWeight()))
      {
      node->getStructure()->setWeight(weight);
      //dumpOptDetails(comp(), "0Node %p(%d) Structure %p Weight %d\n", node, node->getNumber(), node->getStructure(), weight);
      }
   else
      return;

   if (node->getStructure()->asRegion())
      {
      TR_StructureSubGraphNode *subNode;
      TR_Structure             *subStruct = NULL;
      TR_RegionStructure::Cursor si(*(node->getStructure()->asRegion()));
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         weight = startingWeight;
         subStruct = subNode->getStructure();
         if (!subStruct->asRegion())
            {
            if (weight > subStruct->getWeight())
               {
               subStruct->setWeight(weight);
               //dumpOptDetails(comp(), "1Node %p(%d) Structure %p Weight %d\n", subNode, subNode->getNumber(), subStruct, weight);
               }
            }
         else if (subStruct->asRegion()->isAcyclic())
            adjustWeightForBranches(subStruct->asRegion()->getEntry(), subStruct->asRegion()->getEntry(), &weight);
         }
      }

   if (node->getSuccessors().size() > 1)
      startingWeight = std::max(1, startingWeight*9/10);

   for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
      {
      weight = startingWeight;
      if ((*edge)->getTo() != entry)
         adjustWeightForBranches(toStructureSubGraphNode((*edge)->getTo()), entry, &weight);
      }

   for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
      {
      weight = startingWeight;
      if ((*edge)->getTo() != entry)
         adjustWeightForBranches(toStructureSubGraphNode((*edge)->getTo()), entry, &weight);
      }
   }

TR_StructureSubGraphNode *TR_Structure::findNodeInHierarchy(TR_RegionStructure *region, int32_t num)
   {
   if (!region)
      return NULL;
   TR_RegionStructure::Cursor sIt(*region);
   for (TR_StructureSubGraphNode *n = sIt.getFirst(); n; n = sIt.getNext())
      {
      if (n->getNumber() == num)
         return n;
      }
   return findNodeInHierarchy(region->getParent()->asRegion(), num);
   }


bool TR_RegionStructure::containsOnlyAcyclicRegions()
   {
   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      TR_RegionStructure *region = subStruct->asRegion();
      if (region &&
          (!region->isAcyclic() ||
           !region->containsOnlyAcyclicRegions()))
         return false;
      }

   return true;
   }



TR_Structure *TR_BlockStructure::cloneStructure(TR::Block **correspondingBlocks, TR_StructureSubGraphNode **correspodingSubNodes, List<TR_Structure> *whileLoops, List<TR_Structure> *correspondingWhileLoops)
   {
   TR::Block *correspondingBlock = correspondingBlocks[getNumber()];
   TR_BlockStructure *clonedBlockStructure = new (cfg()->structureMemoryRegion()) TR_BlockStructure(comp(), correspondingBlock->getNumber(), correspondingBlock);
   clonedBlockStructure->setNestingDepth(getNestingDepth());
   clonedBlockStructure->setMaxNestingDepth(getMaxNestingDepth());
   clonedBlockStructure->setDuplicatedBlock(this);
   return clonedBlockStructure;
   }




TR_Structure *TR_RegionStructure::cloneStructure(TR::Block **correspondingBlocks, TR_StructureSubGraphNode **correspondingSubNodes, List<TR_Structure> *whileLoops, List<TR_Structure> *correspondingWhileLoops)
   {
   TR::Region &structureMemoryRegion = cfg()->structureMemoryRegion();
   TR_RegionStructure *clonedRegionStructure = new (structureMemoryRegion) TR_RegionStructure(comp(), correspondingBlocks[getNumber()]->getNumber());
   clonedRegionStructure->setAsCanonicalizedLoop(isCanonicalizedLoop());
   clonedRegionStructure->setContainsInternalCycles(containsInternalCycles());

   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      TR_Structure *clonedSubStruct = subStruct->cloneStructure(correspondingBlocks, correspondingSubNodes, whileLoops, correspondingWhileLoops);
      TR_StructureSubGraphNode *clonedSubNode = new (structureMemoryRegion) TR_StructureSubGraphNode(clonedSubStruct);
      clonedRegionStructure->addSubNode(clonedSubNode);
      if (subNode == getEntry())
         clonedRegionStructure->setEntry(clonedSubNode);

      correspondingSubNodes[subNode->getNumber()] = clonedSubNode;
      }

   TR_ScratchList<TR_StructureSubGraphNode> seenExitNodes(trMemory());
   si.reset();
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      TR_StructureSubGraphNode *correspondingSubNode = correspondingSubNodes[subNode->getNumber()];

      for (auto edge = subNode->getSuccessors().begin(); edge != subNode->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (!isExitEdge(*edge))
            {
            TR::CFGEdge::createEdge(correspondingSubNode, correspondingSubNodes[succ->getNumber()], trMemory());
            }
         else
            {
            clonedRegionStructure->addExitEdge(correspondingSubNode, succ->getNumber());
            }
         }

      for (auto edge = subNode->getExceptionSuccessors().begin(); edge != subNode->getExceptionSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *succ = toStructureSubGraphNode((*edge)->getTo());
         if (!isExitEdge(*edge))
            {
            TR::CFGEdge::createExceptionEdge(correspondingSubNode, correspondingSubNodes[succ->getNumber()], trMemory());
            }
         else
            {
            clonedRegionStructure->addExitEdge(correspondingSubNode, succ->getNumber(), 1);
            }
         }
      }

   clonedRegionStructure->setNestingDepth(getNestingDepth());
   clonedRegionStructure->setMaxNestingDepth(TR_Structure::getMaxNestingDepth());

   if (isNaturalLoop() &&
       whileLoops->find(this))
      {
      correspondingWhileLoops->add(clonedRegionStructure);
      }

   TR_InductionVariable *currInductionVariable = getFirstInductionVariable();
   TR_InductionVariable *prevClonedInductionVariable = NULL;
   while (currInductionVariable)
      {
      TR_InductionVariable *currClonedInductionVariable = new (structureMemoryRegion) TR_InductionVariable();
      memcpy(currClonedInductionVariable, currInductionVariable, sizeof(TR_InductionVariable));
      if (!prevClonedInductionVariable)
         clonedRegionStructure->addInductionVariable(currClonedInductionVariable);
      else
         clonedRegionStructure->addAfterInductionVariable(prevClonedInductionVariable, currClonedInductionVariable);

      prevClonedInductionVariable = currClonedInductionVariable;
      currInductionVariable = currInductionVariable->getNext();
      }

   return clonedRegionStructure;
   }





void TR_BlockStructure::cloneStructureEdges(TR::Block **correspondingBlocks)
   {
   }




void TR_RegionStructure::cloneStructureEdges(TR::Block **correspondingBlocks)
   {
   TR_RegionStructure::Cursor si(*this);
   TR_StructureSubGraphNode *subNode;
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subNode->getStructure()->cloneStructureEdges(correspondingBlocks);
      }

   TR_ScratchList<TR::CFGNode> seenExitNodes(trMemory());
   ListIterator<TR::CFGEdge> ei(&getExitEdges());
   TR::CFGEdge *edge;
   for (edge = ei.getFirst(); edge; edge= ei.getNext())
      {
      if (isExitEdge(edge))
         {
         TR::CFGNode *exitNode = edge->getTo();
         if (!seenExitNodes.find(exitNode))
           {
           seenExitNodes.add(exitNode);

           TR::Block   *correspondingBlock = correspondingBlocks[exitNode->getNumber()];
           if (correspondingBlock)
              {
              exitNode->setNumber(correspondingBlock->getNumber());
              }
           }
         }
      }
   }

List<TR::Block> *TR_BlockStructure::getBlocks(List<TR::Block> *blocksInRegion)
   {
   vcount_t visitCount = comp()->incVisitCount();
   return getBlocks(blocksInRegion, visitCount);
   }

List<TR::Block> *TR_RegionStructure::getBlocks(List<TR::Block> *blocksInRegion)
   {
   vcount_t visitCount = comp()->incVisitCount();
   return getBlocks(blocksInRegion, visitCount);
   }



//
// The following code adds the blocks so that the 'next' logical block is added
// after this block to ensure a consistent ordering of the blocks.
// This ordering is required by some users of getBlocks(), namely loopReducer.
//
List<TR::Block> *TR_BlockStructure::getBlocks(List<TR::Block> *blocksInRegion, vcount_t visitCount)
   {
   TR::Block * block = getBlock();
   if (block->getVisitCount() == visitCount)
      {
      return blocksInRegion;
      }

   block->setVisitCount(visitCount);
   blocksInRegion->add(block);

   if (block->getEntry())
      {
      TR::Block * nextBlock = block->getNextBlock();
      if (nextBlock && block->getStructureOf() != NULL && nextBlock->getStructureOf() &&
       block->getStructureOf()->getParent() == nextBlock->getStructureOf()->getParent() &&
          nextBlock->getVisitCount() != visitCount)
         {
         blocksInRegion->add(block->getNextBlock());
         block->getNextBlock()->setVisitCount(visitCount);
         }
      }

   return blocksInRegion;
   }

List<TR::Block> *TR_RegionStructure::getBlocks(List<TR::Block> *blocksInRegion, vcount_t visitCount)
   {
   TR_StructureSubGraphNode *subNode;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*this);

   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      subStruct->getBlocks(blocksInRegion, visitCount);
      }

   return blocksInRegion;
   }



void TR_Structure::addEdge(TR::CFGEdge *edge, bool isExceptionEdge)
   {
   TR_ASSERT(false, "addEdge called for non-region");
   }

void TR_RegionStructure::addEdge(TR::CFGEdge *edge, bool isExceptionEdge)
   {
   // Find the subnode containing the from node
   //
   TR::Block *from = toBlock(edge->getFrom());
   TR::Block *to = toBlock(edge->getTo());
   TR_StructureSubGraphNode *fromNode;
   TR_Structure             *fromStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (fromNode = si.getCurrent(); fromNode != NULL; fromNode = si.getNext())
      {
      fromStruct = fromNode->getStructure();
      if (fromStruct->contains(from->getStructureOf(), this))
         break;
      }
   TR_ASSERT(fromStruct != NULL,"Structure: bad structure when adding an edge");
   TR_ASSERT(fromNode != NULL, "Structure: fromNode of the edge not found in the structure");

   if (fromStruct->asRegion())
      {
      // If the subnode also contains the to block, let it handle the new edge
      //
      if (fromStruct->contains(to->getStructureOf(), this))
         {
         fromStruct->addEdge(edge, isExceptionEdge);
         return;
         }

      // Tell the from node to add an exit edge to the to node
      //
      fromStruct->addExternalEdge(from->getStructureOf(), to->getNumber(), isExceptionEdge);
      }

   TR::Region &structureMemoryRegion = cfg()->structureMemoryRegion();
   // Find the subgraph node for the to block
   //
   TR_StructureSubGraphNode *toNode;
   TR_Structure             *toStruct = NULL;
   while (1)
      {
      TR_RegionStructure::Cursor si(*this);
      for (toNode = si.getCurrent(); toNode != NULL; toNode = si.getNext())
         {
         toStruct = toNode->getStructure();
         if (toStruct->contains(to->getStructureOf(), this))
            break;
         }

      if (!toNode)
         {
         if (to == comp()->getFlowGraph()->getEnd() /* &&
             to->getPredecessors().empty() */)
            {
            toStruct = to->getStructureOf();
            if (!toStruct)
               toStruct = new ((structureMemoryRegion)) TR_BlockStructure(comp(), to->getNumber(), to);
            toStruct->setNumber(to->getNumber());
            toNode = new (structureMemoryRegion) TR_StructureSubGraphNode(toStruct);
            addSubNode(toNode);
            toNode->setNumber(to->getNumber());
            }
         else
            TR_ASSERT(0,"Structure: bad structure when adding an edge");
         }

      // If the edge does not go into the head of the "to" structure, collapse it
      // into this region and try again.
      //
      if (toStruct->getNumber() == to->getNumber())
         break;
      toStruct->asRegion()->collapseIntoParent();
      }

   // Add an edge between the nodes if there isn't one already
   //
   TR::CFGEdgeList* list;
   if (isExceptionEdge)
      list = &(fromNode->getExceptionSuccessors());
   else
      list = &(fromNode->getSuccessors());

   auto iter = list->begin();
   for (; iter != list->end(); ++iter)
      if ((*iter)->getTo() == toNode)
         break;

   if (iter == list->end())
      {
      if (isExceptionEdge)
         TR::CFGEdge::createExceptionEdge(fromNode,toNode,trMemory());
      else
         TR::CFGEdge::createEdge(fromNode, toNode, trMemory());

      // Adding this edge may have given this region internal cycles.
      //
      if (!containsInternalCycles())
         checkForInternalCycles();
      }
   }

void TR_BlockStructure::addExternalEdge(TR_Structure *from, int32_t toNumber, bool isExceptionEdge)
   {
   }

void TR_RegionStructure::addExternalEdge(TR_Structure *from, int32_t toNumber, bool isExceptionEdge)
   {
   // Find the sub-node containing the "from" structure
   //
   TR_StructureSubGraphNode *fromNode;
   TR_Structure             *fromStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (fromNode = si.getCurrent(); fromNode != NULL; fromNode = si.getNext())
      {
      fromStruct = fromNode->getStructure();
      if (fromStruct->contains(from, this))
         break;
      }
   TR_ASSERT(fromNode,"Structure: bad structure when adding an edge");
   fromStruct->addExternalEdge(from, toNumber, isExceptionEdge);

   // Find the exit node for the node to be merged into.
   // If it doesn't exist, create it.  If it exists, return (nothing to do)
   //
   TR::CFGEdge *edge;
   ListIterator<TR::CFGEdge> ei(&_exitEdges);
   si.reset();
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      if (edge->getTo()->getNumber() == toNumber &&
          edge->getFrom() == fromNode)
         return;
      }

   addExitEdge(fromNode, toNumber, isExceptionEdge);
   }

void TR_RegionStructure::collapseIntoParent()
   {
   if (debug("traceCollapseRegion"))
      {
      diagnostic("==> Structure before collapsing %d into parent %d\n",
                  getNumber(), getParent()->getNumber());
      comp()->getDebug()->print(comp()->getOutFile(), getParent(), 6);
      }

   TR_StructureSubGraphNode *node;
   TR::CFGNode               *toNode;
   TR_RegionStructure::Cursor nodes(*this);
   TR_StructureSubGraphNode *target;

   // Build up a bit vector for each node in this region
   //
   TR_BitVector regionNodes(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
   for (node = nodes.getCurrent(); node; node = nodes.getNext())
      regionNodes.set(node->getNumber());

   // Find this node in the parent region and build a bit vector for each node
   // in the parent region.
   //
   TR_RegionStructure *parent = getParent()->asRegion();
   TR_RegionStructure::Cursor targets(*parent);
   TR_BitVector parentNodes(comp()->getFlowGraph()->getNextNodeNumber(), trMemory(), stackAlloc);
   TR_StructureSubGraphNode *nodeInParent = NULL;
   TR_RegionStructure::Cursor pNodes(*parent);
   for (node = pNodes.getCurrent(); node; node = pNodes.getNext())
      {
      parentNodes.set(node->getNumber());
      if (node->getNumber() == getNumber())
         nodeInParent = node;
      }

   // If this region has internal cycles, the parent now has them too.
   // If this region is a natural loop and is not the entry node of
   // the parent, the parent now has internal cycles.
   //
   if (!parent->containsInternalCycles())
      {
      if (containsInternalCycles())
         parent->setContainsInternalCycles(true);
      else if (isNaturalLoop() && nodeInParent != parent->getEntry())
         parent->setContainsInternalCycles(true);
      }

   // Go through each subnode of this region and move it to the parent region.
   //
   TR_RegionStructure::Cursor subNodes(*this);
   for (node = subNodes.getCurrent(); node; node = subNodes.getNext())
      {
      parent->addSubNode(node);

      // Handle successor edges of this subnode.
      // Internal edges will stay as internal edges.
      // Exit edges to nodes in the parent region are turned into internal
      //    edges.
      // Other exit edges are made into exit edges of the parent region.
      //
      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
         {
         toNode = (*edge)->getTo();
         int32_t toNum = toNode->getNumber();
         if (regionNodes.get(toNum))
            {
            // Internal edge, just do nothing
            }
         else if (parentNodes.get(toNum))
            {
            // An exit edge that must be turned into an internal edge in the
            // parent region.
            //
            // Find the node of the parent's graph that represents the
            // same structure as the edge target and change the edge to
            // point to this node.
            // Remove the original edge from the child node to this target; the
            // target node in the child region is discarded.
            //
            TR_RegionStructure::Cursor pTargets(*parent);
            for (target = pTargets.getCurrent(); target; target = pTargets.getNext())
               {
               if (target->getNumber() == toNum)
                  {
                  // We have found the target node in the parent region.
                  // Find the edge from the child region to this node in the
                  // parent CFG and remove it if it is still there. Processing
                  // a previous subnode of the child region may have already
                  // removed it.
                  //
                  for (auto pred = target->getPredecessors().begin(); pred != target->getPredecessors().end(); ++pred)
                     {
                     if ((*pred)->getFrom()->getNumber() == getNumber() &&
                         toStructureSubGraphNode((*pred)->getFrom())->getStructure() == this)
                        {
                        nodeInParent->getSuccessors().remove(*pred);
                        target->getPredecessors().remove(*pred);
                        break;
                        }
                     }

                  // Link up the edge from the child node to the target in the
                  // parent region.
                  //
                  (*edge)->setTo(target);
                  break;
                  }
               }
            TR_ASSERT(target, "Structure error collapsing regions");

            // Disconnect this edge from the original target node in the child
            // region. This target node is not used in the parent region.
            //
            toNode->getPredecessors().remove(*edge);
            }
         else
            {
            // An exit edge that is to remain as an exit edge for the parent
            // region.
            // Just add it as an exit edge.
            //
            parent->addExitEdge(node, toNum, false, *edge);
            }
         }

      // Do the same as the above, but for exception successors of the node
      // that is being moved from the child region to the parent region.
      //
      for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
         {
         toNode = (*edge)->getTo();
         int32_t toNum = toNode->getNumber();
         if (regionNodes.get(toNum))
            {
            // Internal edge, do nothing
            }
         else if (parentNodes.get(toNum))
            {
            TR_RegionStructure::Cursor pTargets(*parent);
            for (target = pTargets.getCurrent(); target; target = pTargets.getNext())
               {
               if (target->getNumber() == toNum)
                  {
                  for (auto pred = target->getExceptionPredecessors().begin(); pred != target->getExceptionPredecessors().end(); ++pred)
                     {
                     if (((*pred)->getFrom()->getNumber() == getNumber()) &&
                         (toStructureSubGraphNode((*pred)->getFrom())->getStructure() == this))
                        {
                        nodeInParent->getExceptionSuccessors().remove(*pred);
                        target->getExceptionPredecessors().remove(*pred);
                        break;
                        }
                     }
                  (*edge)->setExceptionTo(target);
                  break;
                  }
               }
            TR_ASSERT(target, "Structure error collapsing regions");

            toNode->getExceptionPredecessors().remove(*edge);
            }
         else
            {
            // An exit edge that is to remain as an exit edge for the parent
            // region.
            // Just add it as an exit edge.
            //
            parent->addExitEdge(node, toNum, true, *edge);
            }
         }
      }

   // Change all predecessors of the child region in the parent's CFG to go to
   // the entry node of the child region instead.
   //
   for (auto edge = nodeInParent->getPredecessors().begin(); edge != nodeInParent->getPredecessors().end(); ++edge)
      (*edge)->setTo(getEntry());

   // Ditto for exception predecessors
   //
   for (auto edge = nodeInParent->getExceptionPredecessors().begin(); edge != nodeInParent->getExceptionPredecessors().end(); ++edge)
      (*edge)->setExceptionTo(getEntry());

   // Remove all remaining successors of the child region from the parent's
   // exit edge list.
   // Exit edges from the individual subnodes of the child region have already
   // been added as exit edges to the parent region.
   //
   for (auto edge = nodeInParent->getSuccessors().begin(); edge != nodeInParent->getSuccessors().end();)
      {
	  TR::CFGEdge* current = *(edge++);
      parent->getExitEdges().remove(current);
      TR::CFGNode *toNode = current->getTo();
      nodeInParent->removeSuccessor(current);
      toNode->removePredecessor(current);
      }
   for (auto edge = nodeInParent->getExceptionSuccessors().begin(); edge != nodeInParent->getExceptionSuccessors().end();)
      {
      TR::CFGEdge* current = *(edge++);
      parent->getExitEdges().remove(current);
      TR::CFGNode *toNode = current->getTo();
      nodeInParent->removeExceptionSuccessor(current);
      toNode->removeExceptionPredecessor(current);
      }

   // If the child region was the entry node of the parent, change the entry
   // node to be the child region's entry node.
   //
   if (parent->getEntry() == nodeInParent)
      parent->setEntry(getEntry());

   // Remove the subnode in the parent region that represents the child region.
   // All the subnodes of the child region have now been added to the parent
   // region.
   //
   parent->removeSubNode(nodeInParent);
   }


TR::CFGEdge *
TR_RegionStructure::addExitEdge(TR_StructureSubGraphNode *from, int32_t to, bool isExceptionEdge, TR::CFGEdge *origEdge)
   {
   ListIterator<TR::CFGEdge> it(&getExitEdges());
   TR::CFGEdge *cursor;
   for (cursor = it.getFirst(); cursor; cursor = it.getNext())
      {
      if (cursor->getTo()->getNumber() == to)
         break;
      }

   TR::CFGEdge *edge;
   TR::CFGNode *toNode = cursor ? cursor->getTo() : new (cfg()->structureMemoryRegion()) TR_StructureSubGraphNode(to, cfg()->structureMemoryRegion());

   if (origEdge)
      {
      if (isExceptionEdge)
         origEdge->setExceptionTo(toNode);
      else
         origEdge->setTo(toNode);
      edge = origEdge;
      }
   else
      {
      edge = isExceptionEdge ?
         TR::CFGEdge::createExceptionEdge(from, toNode,trMemory()) :
         TR::CFGEdge::createEdge(from,  toNode, trMemory());
      }
   _exitEdges.add(edge);
   return edge;
   }

TR_StructureSubGraphNode *
TR_StructureSubGraphNode::create(int32_t num, TR_RegionStructure *region)
   {
   ListIterator<TR::CFGEdge> it(&region->getExitEdges());
   TR::CFGEdge *edge;
   for (edge = it.getFirst(); edge; edge = it.getNext())
      {
      if (edge->getTo()->getNumber() == num)
         break;
      }
   if (edge)
      return edge->getTo()->asStructureSubGraphNode();

   return new (region->cfg()->structureMemoryRegion()) TR_StructureSubGraphNode(num, region->cfg()->structureMemoryRegion());
   }

TR_StructureSubGraphNode *TR_StructureSubGraphNode::asStructureSubGraphNode() {return this;}

void TR_BlockStructure::checkStructure(TR_BitVector *_blockNumbers)
   {
   TR_ASSERT_FATAL(this->getNumber()==_block->getNumber(), "Number of BlockStructure is NOT the same as that of the block");
   TR_ASSERT_FATAL(_blockNumbers->get(this->getNumber())==0, "Structure, Two blocks with the same number");
   //set the value of the bit representing this block number
   //
   _blockNumbers->set(this->getNumber());
   }

void TR_RegionStructure::checkStructure(TR_BitVector* _blockNumbers)
   {
   TR_StructureSubGraphNode *node;
   TR_ASSERT_FATAL(this->getNumber()==getEntry()->getStructure()->getNumber(), "Entry node does not have same number as this RegionStructure");
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      TR_ASSERT_FATAL(this==node->getStructure()->getParent(), "subGraphNode does not have this RegionStructure as its parent");
      TR_ASSERT_FATAL(node->getNumber()==node->getStructure()->getNumber(), "subGraphNode does not have the same node number as its structure");
      TR_StructureSubGraphNode *pred, *succ;
      bool isConsistent;
      for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
         {
         pred = toStructureSubGraphNode((*edge)->getFrom());
         isConsistent = false;
         for (auto succEdge = pred->getSuccessors().begin(); succEdge != pred->getSuccessors().end(); ++succEdge)
            {
            if (*succEdge == *edge)
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT_FATAL(isConsistent, "predecessor of this subGraph node not not contain this node in its successors");
         }
      for (auto edge = node->getExceptionPredecessors().begin(); edge != node->getExceptionPredecessors().end(); ++edge)
         {
         pred = toStructureSubGraphNode((*edge)->getFrom());
         isConsistent = false;
         for (auto succEdge = pred->getExceptionSuccessors().begin(); succEdge != pred->getExceptionSuccessors().end(); ++succEdge)
            {
            if ((*succEdge) == (*edge))
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT_FATAL(isConsistent, "exception predecessor of this subGraph node not not contain this node in its exception successors");
         }

      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
         {
         succ = toStructureSubGraphNode((*edge)->getTo());
         isConsistent = false;
         for (auto predEdge = succ->getPredecessors().begin(); predEdge != succ->getPredecessors().end(); ++predEdge)
            {
            if (*predEdge == *edge)
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT_FATAL(isConsistent, "successor of this subGraph node not not contain this node in its predecessors");
         }
      for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
         {
         succ = toStructureSubGraphNode((*edge)->getTo());
         isConsistent = false;
         for (auto predEdge = succ->getExceptionPredecessors().begin(); predEdge != succ->getExceptionPredecessors().end(); ++predEdge)
            {
            if ((*predEdge) == *edge)
               {
               isConsistent = true;
               break;
               }
            }
         TR_ASSERT_FATAL(isConsistent, "exception successor of this subGraph node not not contain this node in its exception predecessors");
         }
      node->getStructure()->checkStructure(_blockNumbers);

      if (node->getStructure()->asRegion() != NULL)
         {
         // Check that node's outgoing edges correspond to the subregion's exits
         TR_RegionStructure * const subregion = node->getStructure()->asRegion();

         // All normal successors of node must have corresponding normal exits
         auto &succs = node->getSuccessors();
         for (auto edge = succs.begin(); edge != succs.end(); ++edge)
            {
            const int32_t num = (*edge)->getTo()->getNumber();
            TR::CFGEdge *exit = NULL;
            ListIterator<TR::CFGEdge> exits(&subregion->getExitEdges());
            for (exit = exits.getFirst(); exit != NULL; exit = exits.getNext())
               {
               if (exit->getTo()->getNumber() == num)
                  break;
               }

            TR_ASSERT_FATAL(
               exit != NULL,
               "subnode %d:%p edge to %d has no corresponding exit edge from "
               "subregion %p\n",
               node->getNumber(),
               node,
               num,
               subregion);

            auto * const exitNode = toStructureSubGraphNode(exit->getTo());
            auto &preds = exitNode->getPredecessors();
            TR_ASSERT_FATAL(
               std::find(preds.begin(), preds.end(), exit) != preds.end(),
               "exit in subregion %p corresponding to subnode %d:%p edge to %d "
               "not found in normal predecessors of exit node %p\n",
               subregion,
               node->getNumber(),
               node,
               num,
               exitNode);
            }

         // All exception successors of node must have corresponding exception exits
         auto &excSuccs = node->getExceptionSuccessors();
         for (auto edge = excSuccs.begin(); edge != excSuccs.end(); ++edge)
            {
            const int32_t num = (*edge)->getTo()->getNumber();
            TR::CFGEdge *exit = NULL;
            ListIterator<TR::CFGEdge> exits(&subregion->getExitEdges());
            for (exit = exits.getFirst(); exit != NULL; exit = exits.getNext())
               {
               if (exit->getTo()->getNumber() == num)
                  break;
               }

            TR_ASSERT_FATAL(
               exit != NULL,
               "subnode %d:%p edge to %d has no corresponding exit edge from "
               "subregion %p\n",
               node->getNumber(),
               node,
               num,
               subregion);

            auto * const exitNode = toStructureSubGraphNode(exit->getTo());
            auto &excPreds = exitNode->getExceptionPredecessors();
            TR_ASSERT_FATAL(
               std::find(excPreds.begin(), excPreds.end(), exit) != excPreds.end(),
               "exit in subregion %p corresponding to subnode %d:%p edge to %d "
               "not found in exception predecessors of exit node %p\n",
               subregion,
               node->getNumber(),
               node,
               num,
               exitNode);
            }

         // For each exit node must have a corresponding successor of the
         // appropriate type
         ListIterator<TR::CFGEdge> exits(&subregion->getExitEdges());
         for (auto exit = exits.getFirst(); exit != NULL; exit = exits.getNext())
            {
            auto * const exitNode = toStructureSubGraphNode(exit->getTo());
            const int32_t num = exitNode->getNumber();
            auto &excPreds = exitNode->getExceptionPredecessors();
            const bool isExc =
               std::find(excPreds.begin(), excPreds.end(), exit) != excPreds.end();

            TR::CFGEdgeList &succs =
               isExc ? node->getExceptionSuccessors() : node->getSuccessors();

            auto found = succs.begin();
            for (; found != succs.end(); ++found)
               {
               if ((*found)->getTo()->getNumber() == num)
                  break;
               }

            TR_ASSERT_FATAL(
               found != succs.end(),
               "exit from subregion %p to %d has no corresponding edge in "
               "parent region outgoing from subnode %d:%p\n",
               subregion,
               num,
               node->getNumber(),
               node);
            }
         }
      }

   TR_ScratchList<TR_StructureSubGraphNode> exitNodes(trMemory());
   ListIterator<TR::CFGEdge> exitEdges(&_exitEdges);
   TR::CFGEdge *edge;
   for (edge = exitEdges.getFirst(); edge; edge = exitEdges.getNext())
      {
      TR_StructureSubGraphNode *toNode = toStructureSubGraphNode(edge->getTo());

      TR_StructureSubGraphNode *seenNode = NULL;
      ListIterator<TR_StructureSubGraphNode> exitNodesIt(&exitNodes);
      for (seenNode = exitNodesIt.getFirst(); seenNode; seenNode = exitNodesIt.getNext())
         {
         if ((seenNode->getNumber() == toNode->getNumber()) &&
             (seenNode != toNode))
            TR_ASSERT_FATAL(0, "Exit edges to the same node number %d in region %p must have a unique structure subgraph node (node1 %p node2 %p)\n", seenNode->getNumber(), this, seenNode, toNode);
         }
      if (!exitNodes.find(toNode))
         exitNodes.add(toNode);
      }
   }





void TR_BlockStructure::resetAnalysisInfo()
   {
   setAnalysisInfo(NULL);
   }



void TR_RegionStructure::resetAnalysisInfo()
   {
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      node->getStructure()->resetAnalysisInfo();
   setAnalysisInfo(NULL);
   }




void TR_BlockStructure::resetAnalyzedStatus()
   {
   setAnalyzedStatus(false);
   }



void TR_RegionStructure::resetAnalyzedStatus()
   {
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      node->getStructure()->resetAnalyzedStatus();
   setAnalyzedStatus(false);
   }


bool TR_BlockStructure::markStructuresWithImproperRegions()
   {
   setContainsImproperRegion(false);
   return false;
   }



bool TR_RegionStructure::markStructuresWithImproperRegions()
   {
   bool b = false;
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (node->getStructure()->markStructuresWithImproperRegions())
         b = true;
      }

   if (containsInternalCycles())
      b = true;

   if (b)
      setContainsImproperRegion(true);
   else
      setContainsImproperRegion(false);

   return b;
   }




TR::Block *TR_BlockStructure::setBlock(TR::Block *b)
   {
   _block = b;
   TR_BlockStructure *old = b->getStructureOf();
   if (old)
      {
      if (old->isEntryOfShortRunningLoop())
         setIsEntryOfShortRunningLoop();
      }
   return b;
   }


bool TR_BlockStructure::changeContinueLoopsToNestedLoops(TR_RegionStructure *root)
   {
   return false;
   }

bool TR_RegionStructure::changeContinueLoopsToNestedLoops(TR_RegionStructure *root)
   {
   static bool traceMyself=(debug("TR_traceChangeContinues") != NULL);

   bool changed = false;
   TR_RegionStructure::Cursor si(*this);
   TR_StructureSubGraphNode *subNode;
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getStructure()->changeContinueLoopsToNestedLoops(root))
         changed = true;
      }

   if (isNaturalLoop())
      {
      TR::CFG *cfg=comp()->getFlowGraph();
      TR::CFGEdge *backEdgeToHeader = NULL;
      TR::Block *entryBlock = getEntryBlock();
      TR_ScratchList<TR::CFGEdge> splitEdges(trMemory());
      TR_ScratchList<TR::CFGEdge> incomingEdges(trMemory());
      unsigned numNewHeaders=0;
      for (auto nextEdge = entryBlock->getPredecessors().begin(); nextEdge != entryBlock->getPredecessors().end(); ++nextEdge)
         {
         TR::Block *pred = (*nextEdge)->getFrom()->asBlock();
         if (contains(pred->getStructureOf(), getParent()))
            {
            // special case backedge: if back-edge is a fall-through, don't do anything to this loop
            if (pred->getExit()->getNextTreeTop() != entryBlock->getEntry())
               {
               numNewHeaders = 0;
               break;
               }
            if (traceMyself)
               diagnostic("found backedge %d\n", pred->asBlock()->getNumber());
            if (backEdgeToHeader)
               {
               if (traceMyself)
                  diagnostic("\tsplitting old backedge pred %d\n", backEdgeToHeader->getFrom()->asBlock()->getNumber());
               splitEdges.add(backEdgeToHeader);
               backEdgeToHeader = *nextEdge;
               numNewHeaders++;
               }
            else
               backEdgeToHeader = *nextEdge;
            }
         else
            {
            if (traceMyself)
               diagnostic("found incoming edge from %d\n", pred->asBlock()->getNumber());
            TR_ASSERT((*nextEdge)->getTo()->asBlock() == entryBlock,"Natural loop shouldn't have incoming edge to nonheader block");
            incomingEdges.add(*nextEdge);
            }
         }

      if (numNewHeaders == 1 && performTransformation(comp(), "%s transforming continues in loop %d to %d new nested loops\n", OPT_DETAILS, getEntryBlock()->asBlock()->getNumber(), numNewHeaders))
         {
         TR::Block *newHeader = NULL;
         TR::Block *prevHeader = entryBlock;
         ListIterator<TR::CFGEdge> seIterator(&splitEdges);
         // for each split edge, change the branch to point to one of the new header blocks
         for (TR::CFGEdge *splitEdge = seIterator.getFirst(); splitEdge; splitEdge=seIterator.getNext())
            {
            TR::Block *newHeader = TR::Block::createEmptyBlock(entryBlock->getEntry()->getNode(), comp(), splitEdge->getFrequency(), prevHeader);

            cfg->addNode(newHeader, entryBlock->getParentStructureIfExists(cfg));
            cfg->addEdge(newHeader, prevHeader);

            if (traceMyself)
               diagnostic("Rerouting edge %d->old header %d to new header %d\n", splitEdge->getFrom()->asBlock()->getNumber(), splitEdge->getTo()->asBlock()->getNumber(), newHeader->getNumber());

            // change destination at the end of splitEdge->getFrom()->asBlock() to branch to newHeader rather than entryBlock, updating edges as needed
            TR::Block::redirectFlowToNewDestination(comp(), splitEdge, newHeader, false);

            // paste trees for this header ahead of prevHeader
            TR::TreeTop *beforePrevHeader = prevHeader->getEntry()->getPrevTreeTop();
            newHeader->getExit()->join(prevHeader->getEntry());
            if (beforePrevHeader)
               beforePrevHeader->join(newHeader->getEntry());
            else
               comp()->setStartTree(newHeader->getEntry());

            prevHeader = newHeader;
            }

         // for each incoming edge to the original header, redirect to the outermost new header block
         // note: newHeader here already holds the header for the outermost loop
         ListIterator<TR::CFGEdge> ieIterator(&incomingEdges);
         for (TR::CFGEdge *incomingEdge = ieIterator.getFirst(); incomingEdge; incomingEdge=ieIterator.getNext())
            {
            if (traceMyself)
               {
               diagnostic("Incoming [%p] edge %d->%d\n", incomingEdge, incomingEdge->getFrom()->asBlock()->getNumber(), incomingEdge->getTo()->asBlock()->getNumber());
               diagnostic("\tassuming it should be ->%d\n", entryBlock->getNumber());
               diagnostic("\tredirecting to ->%d\n", newHeader->getNumber());
               }

            TR::Block::redirectFlowToNewDestination(comp(), incomingEdge, newHeader, false);
            }

         changed = true;
         }
      }

   return changed;
   }


TR_StructureSubGraphNode *
TR_RegionStructure::findNodeInHierarchy(int32_t num)
   {
   TR_RegionStructure::Cursor it(*this);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getNumber() == num)
         return node;
      }
   return getParent() ?
      getParent()->findNodeInHierarchy(num) : 0;
   }

int32_t
TR_Structure::getNumberOfLoops()
   {
   TR_RegionStructure * region = this->asRegion();
   int32_t numberOfLoops = 0;

   if (region == NULL)
      {
      return 0;
      }

   if (!region->isAcyclic())
      {
      numberOfLoops += 1;
      }

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *n = it.getFirst(); n; n = it.getNext())
      {
      numberOfLoops += n->getStructure()->getNumberOfLoops();
      }

   return numberOfLoops;
   }


// merged  -- block to be merged and disappear
// mergedInto -- block to be keeped, but keep the number of merged,
//               reason: we may merge block 2 to block 3, but first block should be numbered as 2
void TR_Structure::mergeBlocks(TR::Block *merged, TR::Block *mergedInto)
   {
   // Grab the block numbers before they are merged
   //
   if (debug("dumpStructure"))
      dumpOptDetails(comp(), "\nStructures after merging blocks %d and %d:\n", merged->getNumber(), mergedInto->getNumber());

   // Update the structure to reflect the block merging
   //
   int16_t newFrequency = std::max(merged->getFrequency(), mergedInto->getFrequency());
   merged->setFrequency(newFrequency);

   mergeInto(merged, mergedInto);

   // Make the block structure for the merged into block represent the merged block.
   //
   TR_BlockStructure *s = mergedInto->getStructureOf()->asBlock();
   s->setBlock(merged);
   merged->setStructureOf(s);

   TR_ASSERT(s->getNumber() == merged->getNumber(),"Structure: bad structure when merging blocks");
   if (debug("dumpStructure"))
      comp()->getDebug()->print(comp()->getOutFile(), comp()->getFlowGraph()->getStructure(), 6);
   }


void TR_Structure::mergeInto(TR::Block *merged, TR::Block *mergedInto)
   {
   // If we have reached this point the structure is invalid since the parent
   // should have handled it.
   //
   TR_ASSERT(false,"Structure: bad structure when merging blocks");
   }


// merged  -- block to be merged and disappear
// mergedInto -- block to be keeped, but keep the number of merged,
//               reason: we may merge block 2 to block 3, but first block should be numbered as 2
void TR_RegionStructure::mergeInto(TR::Block *merged, TR::Block *mergedInto)
   {
   bool mergedIsCatch = false;
   if (merged->isCatchBlock())
      mergedIsCatch = true;


   // Find the subnode containing the block to be removed
   //
   TR_StructureSubGraphNode *fromNode;
   TR_Structure             *fromStruct = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (fromNode = si.getCurrent(); fromNode != NULL; fromNode = si.getNext())
      {
      fromStruct = fromNode->getStructure();
      if (fromStruct->contains(merged->getStructureOf(), this))
         break;
      }
   TR_ASSERT(fromStruct != NULL,"Structure: bad structure when merging blocks");

   // If the subnode also contains the block merged into, let it deal with it.
   //
   if (fromStruct->contains(mergedInto->getStructureOf(), this))
      {
      int32_t mergedIntoNum = mergedInto->getNumber();
      fromStruct->mergeInto(merged, mergedInto);
      if (fromStruct->getKind() == TR_Structure::Region && fromStruct->getNumber() == mergedIntoNum)
         {
         fromStruct->renumber(merged->getNumber());
         fromNode->setNumber(merged->getNumber());
         }
      return;
      }

   // Find the subgraph node for the node to be merged into
   //
   int32_t toBlockNum = mergedInto->getNumber();
   TR_StructureSubGraphNode *toNode = NULL;
   si.reset();
   for (toNode = si.getCurrent(); toNode != NULL; toNode = si.getNext())
      {
      if (toNode->getStructure()->getNumber() == toBlockNum)
         break;
      }
   TR_ASSERT(toNode != NULL,"Structure: bad structure when merging blocks");

   // If the subnode containing the merged block is a block structure it must
   // be removed. Otherwise it must repair itself.
   //
   TR_BlockStructure *bs = fromStruct->asBlock();
   if (bs == NULL)
      {
      fromStruct->removeMergedBlock(merged, mergedInto);

      // Change the "to" structure to become the merged block
      //
      toNode->getStructure()->renumber(merged->getNumber());
      toNode->setNumber(merged->getNumber());
      if (mergedIsCatch)
         {
         while (!toNode->getPredecessors().empty())
            {
            TR::CFGEdge *normalEdge = toNode->getPredecessors().front();
            TR::CFGNode *normalPred = normalEdge->getFrom();
            normalPred->removeSuccessor(normalEdge);
            toNode->removePredecessor(normalEdge);
            normalPred->addExceptionSuccessor(normalEdge);
            toNode->addExceptionPredecessor(normalEdge);
            }
         }
      return;
      }

   if (fromNode == getEntry())
      setEntry(toNode);

   // Change the "to" structure to become the merged block
   //
   toNode->getStructure()->renumber(merged->getNumber());
   toNode->setNumber(merged->getNumber());

   while (!fromNode->getPredecessors().empty())
   {
	  TR::CFGEdge * edge = fromNode->getPredecessors().front();
	  fromNode->getPredecessors().pop_front();
      edge->setTo(toNode);
   }

   while (!fromNode->getExceptionPredecessors().empty())
   {
	   TR::CFGEdge * edge = fromNode->getExceptionPredecessors().front();
	   fromNode->getExceptionPredecessors().pop_front();
	   edge->setExceptionTo(toNode);
   }

   // Remove the 'removed' node from the list of predecessors of its successors
   //
   ListIterator<TR::CFGEdge> exitEdges(&_exitEdges);
   TR::CFGEdge *edge;
   for (edge = exitEdges.getFirst(); edge; )
      {
      TR::CFGEdge *nextEdge = exitEdges.getNext();
      if (edge->getFrom() == fromNode)
         removeEdge(edge, true);
      edge = nextEdge;
      }

   while (!fromNode->getSuccessors().empty())
      {
      edge = fromNode->getSuccessors().front();
      removeEdge(edge, false);
      }

   while (!fromNode->getExceptionSuccessors().empty())
      {
      edge = fromNode->getExceptionSuccessors().front();
      removeEdge(edge, false);
      }

   if (!toNode->getStructure()->asBlock())
      {
      for (auto edge = toNode->getSuccessors().begin(); edge != toNode->getSuccessors().end();)
         {
         if ((*edge)->getTo() == toNode)
            removeEdge(*(edge++), true);
         else
            ++edge;
         }

      for (auto edge = toNode->getExceptionSuccessors().begin(); edge != toNode->getExceptionSuccessors().end();)
         {
         if ((*edge)->getTo() == toNode)
            removeEdge(*(edge++), true);
         else
            ++edge;
         }
      }

   // If the "from" node has not yet been removed, remove it explicitly
   //
   if (fromNode->getStructure()->getParent())
      removeSubNode(fromNode);
   }








void TR_Structure::removeMergedBlock(TR::Block *merged, TR::Block *mergedInto)
   {
   TR_ASSERT(false,"Structure: bad structure when merging blocks");
   }



void TR_RegionStructure::removeMergedBlock(TR::Block *merged, TR::Block *mergedInto)
   {
   // Find the subnode containing the block to be removed
   //
   TR_StructureSubGraphNode *node;
   TR_Structure             *s = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      s = node->getStructure();
      if (s->contains(merged->getStructureOf(), this))
         break;
      }
   TR_ASSERT(s != NULL,"Structure: bad structure when merging blocks");

   // If the subnode is not a block, let it deal with the removal
   // Otherwise remove the block structure from the region.
   //
   ListElement<TR::CFGEdge> *edge;
   TR_StructureSubGraphNode *toNode;
   TR_BlockStructure *bs = s->asBlock();
   if (bs == NULL)
      {
      s->removeMergedBlock(merged, mergedInto);

      bool mergedIsCatch = false;
      if (merged->isCatchBlock())
         mergedIsCatch = true;

      // Change the node number of the exit node(s) to be the number that the
      // merged block will eventually have.
      //
      for (edge = _exitEdges.getListHead(); edge != NULL; edge = edge->getNextElement())
         {
         toNode = toStructureSubGraphNode(edge->getData()->getTo());

         if (toNode->getNumber() == mergedInto->getNumber())
            {
	    if (mergedIsCatch)
	       {
               TR::CFGNode *normalPred;
               for (auto normalEdge = toNode->getPredecessors().begin(); normalEdge != toNode->getPredecessors().end();)
                  {
                  TR::CFGEdge *ne = (*(normalEdge)++);
                  normalPred = ne->getFrom();
                  normalPred->removeSuccessor(ne);
                  toNode->removePredecessor(ne);
                  normalPred->addExceptionSuccessor(ne);
                  toNode->addExceptionPredecessor(ne);
                  }
	       }

           toNode->setNumber(merged->getNumber());
           break;
            }
         }
      return;
      }

   // Find the exit edge representing the edge being merged. There should be
   // only one regular exit edge, but there can also be exception exit edges.
   // These should be removed too.
   //
   TR_StructureSubGraphNode *mergedIntoNode = NULL;
   for (edge = _exitEdges.getListHead(); edge != NULL; edge = edge->getNextElement())
      {
      if (edge->getData()->getFrom() == node)
         {
         toNode = toStructureSubGraphNode(edge->getData()->getTo());
         if (toNode->getNumber() == mergedInto->getNumber())
            mergedIntoNode = toNode;
         removeEdge(edge->getData(), true);
         }
      }

   // The resulting block with have the merged block number
   //
   TR_ASSERT(mergedIntoNode, "bad structure merging blocks");
   mergedIntoNode->setNumber(merged->getNumber());

   // Make all edges into this node into exit edges and remove the subnode
   //
   for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
      {
      _exitEdges.add((*edge));
      }

   for (auto edge = node->getExceptionPredecessors().begin(); edge != node->getExceptionPredecessors().end(); ++edge)
      {
      _exitEdges.add((*edge));
      }

   removeSubNode(node);

   // The node now acts as an exit node, so clear out its structure reference
   //
   node->setStructure(NULL);
   }



void TR_BlockStructure::collectCFGEdgesTo(int32_t toNum, List<TR::CFGEdge> *cfgEdges)
   {
   TR::CFGEdge *edge;
   TR_SuccessorIterator sit(getBlock());
   for (edge = sit.getFirst(); edge; edge = sit.getNext())
      {
      TR::CFGNode *succ = edge->getTo();
      if (succ->getNumber() == toNum)
         cfgEdges->add(edge);
      }
   }


void TR_RegionStructure::collectCFGEdgesTo(int32_t toNum, List<TR::CFGEdge> *cfgEdges)
   {
   ListElement<TR::CFGEdge> *edge;
   for (edge = _exitEdges.getListHead(); edge != NULL; edge = edge->getNextElement())
      {
      if (edge->getData()->getTo()->getNumber() == toNum)
        toStructureSubGraphNode(edge->getData()->getFrom())->getStructure()->collectCFGEdgesTo(toNum, cfgEdges);
      }
   }







void TR_BlockStructure::renumber(int32_t num)
   {
   _nodeIndex = num;
   _block->setNumber(num);
   }


void TR_RegionStructure::renumber(int32_t num)
   {
   ListElement<TR::CFGEdge> *edge = NULL, *nextEdge = NULL, *prevEdge = NULL;
   edge = _exitEdges.getListHead();
   //for (edge = _exitEdges.getListHead(); edge != NULL; prevEdge = edge, edge = nextEdge)
   while (edge)
      {
      nextEdge = edge->getNextElement();
      if (edge->getData()->getTo()->getNumber() == num)
         {
         bool isExceptionEdge = true;
         if (std::find(edge->getData()->getFrom()->getExceptionSuccessors().begin(), edge->getData()->getFrom()->getExceptionSuccessors().end(), edge->getData()) == edge->getData()->getFrom()->getExceptionSuccessors().end())
            isExceptionEdge = false;

         if (isExceptionEdge)
              edge->getData()->setExceptionTo(getEntry());
         else
            edge->getData()->setTo(getEntry());

         if (prevEdge)
            prevEdge->setNextElement(nextEdge);
         else
            _exitEdges.setListHead(nextEdge);
         edge = nextEdge;
         continue;
         }
      prevEdge = edge;
      edge = nextEdge;
      }

   _nodeIndex = num;
   getEntry()->setNumber(num);
   getEntry()->getStructure()->renumber(num);
   }


/*
void TR_RegionStructure::renumber(int32_t num)
   {
   ListElement<TR::CFGEdge> *edge, *nextEdge = NULL, *prevEdge = NULL;
   for (edge = _exitEdges.getListHead(); edge != NULL; prevEdge = edge, edge = nextEdge)
      {
      nextEdge = edge->getNextElement();
      if (edge->getData()->getTo()->getNumber() == num)
         {
         bool isExceptionEdge = true;
         if (!edge->getData()->getFrom()->getExceptionSuccessors().find(edge->getData()))
            isExceptionEdge = false;

         if (isExceptionEdge)
              edge->getData()->setExceptionTo(getEntry());
         else
            edge->getData()->setTo(getEntry());

         if (prevEdge)
            prevEdge->setNextElement(nextEdge);
         else
            _exitEdges.setListHead(nextEdge);
         }
      }

   _nodeIndex = num;
   getEntry()->setNumber(num);
   getEntry()->getStructure()->renumber(num);
   }
*/



void TR_BlockStructure::resetVisitCounts(vcount_t num)
   {
   }

void TR_RegionStructure::resetVisitCounts(vcount_t num)
   {
   TR_StructureSubGraphNode *node;
   TR_Structure             *s = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      node->setVisitCount(num);
      TR::CFGEdge *edge;
      TR_SuccessorIterator sit(node);
      for (edge = sit.getFirst(); edge; edge = sit.getNext())
         edge->setVisitCount(num);
      s = node->getStructure();
      s->resetVisitCounts(num);
      }
   }






bool TR_BlockStructure::renumberRecursively(int32_t origNum, int32_t num)
   {
   if (origNum == getNumber())
      {
      renumber(num);
      return true;
      }
   return false;
   }

bool TR_RegionStructure::renumberRecursively(int32_t origNum, int32_t num)
   {
   TR_StructureSubGraphNode *node;
   TR_Structure             *s = NULL;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      s = node->getStructure();
      if (s->renumberRecursively(origNum, num))
        node->setNumber(num);
      }

   ListElement<TR::CFGEdge> *edge, *nextEdge = NULL;
   for (edge = _exitEdges.getListHead(); edge != NULL; edge = nextEdge)
      {
      nextEdge = edge->getNextElement();
      if (edge->getData()->getTo()->getNumber() == origNum)
         edge->getData()->getTo()->setNumber(num);
      }


   if (origNum == getNumber())
     {
     _nodeIndex = num;
     return true;
     }

   return false;
   }





void TR_RegionStructure::computeInvariantSymbols()
   {
   int32_t symRefCount = comp()->getSymRefCount();
   _invariantSymbols = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
   _invariantSymbols->setAll(symRefCount);
   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   getBlocks(&blocksInRegion);
   vcount_t visitCount = comp()->incVisitCount();
   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      TR::TreeTop *entryTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();

      TR::TreeTop *currentTree = entryTree->getNextTreeTop();

      while (!(currentTree == exitTree))
         {
         TR::Node *currentNode = currentTree->getNode();
         updateInvariantSymbols(currentNode, visitCount);
         currentTree = currentTree->getNextRealTreeTop();
         }
      }
   }

void TR_RegionStructure::updateInvariantSymbols(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();
   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();

      if (symReference->getSymbol()->isVolatile())
         _invariantSymbols->reset(symReference->getReferenceNumber());

      if (opCode.isResolveCheck())
         {
         TR::SymbolReference *childSymRef = node->getFirstChild()->getSymbolReference();
         _invariantSymbols->reset(childSymRef->getReferenceNumber());
         childSymRef->getUseDefAliases().getAliasesAndSubtractFrom(*_invariantSymbols);
         }

      if (!opCode.isLoadVar() && (opCode.getOpCodeValue() != TR::loadaddr))
         {
         if (!opCode.isCheck())
            {
            symReference->getUseDefAliases().getAliasesAndSubtractFrom(*_invariantSymbols);
            }

         if (opCode.isStore())
            _invariantSymbols->reset(symReference->getReferenceNumber());
         }
      }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      updateInvariantSymbols(node->getChild(i), visitCount);
   }


void TR_RegionStructure::computeInvariantExpressions()
   {
   computeInvariantSymbols();

   int32_t nodeCount = comp()->getNodeCount();
   _invariantExpressions = new (trStackMemory()) TR_BitVector(nodeCount, trMemory(), stackAlloc, growable);

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   getBlocks(&blocksInRegion);
   vcount_t visitCount = comp()->incVisitCount();
   ListIterator<TR::Block> blocksIt(&blocksInRegion);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      TR::TreeTop *entryTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();

      TR::TreeTop *currentTree = entryTree->getNextTreeTop();

      while (!(currentTree == exitTree))
         {
         TR::Node *currentNode = currentTree->getNode();
         updateInvariantExpressions(currentNode, visitCount);
         currentTree = currentTree->getNextRealTreeTop();
         }
      }
   }

void TR_RegionStructure::updateInvariantExpressions(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (isExprTreeInvariant(node))
      _invariantExpressions->set(node->getGlobalIndex());

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      updateInvariantExpressions(node->getChild(i), visitCount);
   }


void TR_RegionStructure::resetInvariance()
   {
   _invariantSymbols = NULL;
   _invariantExpressions = NULL;
   }

bool TR_RegionStructure::isExprInvariant(TR::Node *expr, bool usePrecomputed)
   {
   if (_invariantExpressions && usePrecomputed)
      return _invariantExpressions->get(expr->getGlobalIndex()) != 0;

   return isExprTreeInvariant(expr);
   }

bool TR_RegionStructure::isExprTreeInvariant(TR::Node *expr)
   {
   if (!_invariantSymbols)
      computeInvariantSymbols();

   vcount_t visitCount = comp()->incOrResetVisitCount();

   return isSubtreeInvariant(expr, visitCount);
   }

  bool TR_RegionStructure::isSymbolRefInvariant(TR::SymbolReference *symRef)
   {
   if(!_invariantSymbols)
      computeInvariantSymbols();
   return _invariantSymbols->get(symRef->getReferenceNumber()) != 0;
   }

bool TR_RegionStructure::isSubtreeInvariant(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return true;

   if (node->getOpCode().isCall())
      return false;

   if (node->getOpCode().hasSymbolReference())
      {
      if (!_invariantSymbols->get(node->getSymbolReference()->getReferenceNumber()))
         return false;
      }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      {
      if (!isSubtreeInvariant(node->getChild(i), visitCount))
         return false;
      }

   return true;
   }



/****************************************/





void TR_BlockStructure::hoistInvariantsOutOfNestedLoops(TR_LocalTransparency *transparencyInfo, TR_BitVector **optSetInfo, bool insideLoop, TR_BlockStructure *outerLoopInvariantBlock, TR_RegionStructure *outerLoop, int32_t bitVectorSize)
   {
   }


#if 0
void
TR_SequenceStructure::hoistInvariantsOutOfNestedLoops(
   TR_LocalTransparency *transparencyInfo, TR_BitVector **optSetInfo, bool insideLoop, TR_Structure *outerLoop, int32_t bitVectorSize)
   {
   bool isInvariantLoop = false;

   if (_count > 1)
      {
      if ((_subNodes[0]->asBlock() != NULL) && (_subNodes[1]->asDoWhile() != NULL))
         {
         for (int32_t i = 0; i < _count; i++)
            _subNodes[i]->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, true, this, bitVectorSize);

         isInvariantLoop = true;

         TR_BlockStructure *loopInvariantBlock = _subNodes[0]->asBlock();

         if (insideLoop)
            {
            TR_SequenceStructure *outerSequenceStructure = outerLoop->asSequence();
            TR_BlockStructure *outerLoopInvariantBlock = outerSequenceStructure->getSubNode(0)->asBlock();

            if (!(optSetInfo[loopInvariantBlock->getNumber()])->isEmpty())
               {
               TR_BitVector *optMask = new (trStackMemory()) TR_BitVector(bitVectorSize, trMemory(), stackAlloc);

               TR_BitVectorIterator bvi(*(optSetInfo[loopInvariantBlock->getNumber()]));
               int32_t nextOptimalComputation = -1;
               while (bvi.hasMoreElements())
                  {
                  nextOptimalComputation = bvi.getNextElement();
                  if (!(optSetInfo[outerLoopInvariantBlock->getNumber()])->get(nextOptimalComputation))
                     {
                     if (outerSequenceStructure->getSubNode(1)->isExpressionTransparentIn(nextOptimalComputation, transparencyInfo))
                        {
                        TR::Block *innerLoopInvariantBlock = loopInvariantBlock->getBlock();
                        TR::TreeTop *exitTree = innerLoopInvariantBlock->getExit();
                        TR::TreeTop *currentTree = innerLoopInvariantBlock->getEntry();
                        TR::TreeTop *treeToBeHoisted = NULL;
                        while (currentTree != exitTree)
                           {
                           if (currentTree->getNode()->getOpCode().isCheck())
                              {
                              if (currentTree->getNode()->getLocalIndex() == nextOptimalComputation)
                                 {
                                 treeToBeHoisted = currentTree;
                                 break;
                                 }
                              }
                           else if (currentTree->getNode()->getOpCode().isStore())
                              {
                              TR::Node *currentNode = currentTree->getNode();

                              if (currentNode->getSymbolReference()->getSymbol()->isAuto())
                                 {
                                 if (currentNode->getFirstChild()->getLocalIndex() == nextOptimalComputation)
                                    {
                                    treeToBeHoisted = currentTree;
                                    break;
                                    }
                                 }
                              }
                           currentTree = currentTree->getNextTreeTop();
                           }

                        if (treeToBeHoisted != NULL)
                           {
                           TR::TreeTop *prevTree = treeToBeHoisted->getPrevTreeTop();
                           TR::TreeTop *nextTree = treeToBeHoisted->getNextTreeTop();
                           prevTree->setNextTreeTop(nextTree);
                           nextTree->setPrevTreeTop(prevTree);

                           TR::TreeTop *outerExitTree = outerLoopInvariantBlock->getBlock()->getLastRealTreeTop();

                           TR::TreeTop *outerPrevTree = outerExitTree->getPrevTreeTop();
                           treeToBeHoisted->setNextTreeTop(outerExitTree);
                           treeToBeHoisted->setPrevTreeTop(outerPrevTree);
                           outerPrevTree->setNextTreeTop(treeToBeHoisted);
                           outerExitTree->setPrevTreeTop(treeToBeHoisted);
                           }

                        if (debug("tracePR"))
                           {
                           dumpOptDetails(comp(), "Hoisting loop invariant computation %d over nested loops (lifted from inner block_%d to outer block_%d)\n", nextOptimalComputation, loopInvariantBlock->getNumber(), outerLoopInvariantBlock->getNumber());
                           }

                        /////actually hoist the invariant
                        /////also remember to change the optSetInfo for the
                        /////two invariant blocks
                        optMask->set(nextOptimalComputation);
                        }
                     }
                  }
               *(optSetInfo[loopInvariantBlock->getNumber()]) -= *optMask;
               *(optSetInfo[outerLoopInvariantBlock->getNumber()]) |= *optMask;
               }
            }
         }
      }

   if (!isInvariantLoop)
      {
      for (int32_t i = 0; i < _count; i++)
         _subNodes[i]->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, insideLoop, outerLoop, bitVectorSize);
      }
   }
#endif



void
TR_RegionStructure::hoistInvariantsOutOfNestedLoops(
   TR_LocalTransparency *transparencyInfo, TR_BitVector **optSetInfo, bool insideLoop,
   TR_BlockStructure *outerLoopInvariantBlock, TR_RegionStructure *outerLoop, int32_t bitVectorSize)
   {
   bool isInvariantLoop = false;
   TR_RegionStructure *loopStructure = NULL;
   TR_BlockStructure *loopInvariantBlock = NULL;
   TR_StructureSubGraphNode *loopInvariantNode = NULL;

   if (isCanonicalizedLoop() && getEntry()->getSuccessors().size() == 2)
       {
       for (auto succ = getEntry()->getSuccessors().begin(); succ != getEntry()->getSuccessors().end(); ++succ)
          {
          TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) (*succ)->getTo();
          TR_BlockStructure *succStructure = succNode->getStructure()->asBlock();
          if (succStructure == NULL)
             break;
          else
             {
             if (succStructure->isLoopInvariantBlock())
                {
                loopInvariantNode = succNode;
                loopInvariantBlock = succStructure;
                }
             }
          }

       if (loopInvariantBlock)
          {
          if (loopInvariantNode->getSuccessors().size() == 1)
             {
             loopStructure = ((TR_StructureSubGraphNode *) loopInvariantNode->getSuccessors().front()->getTo())->getStructure()->asRegion();
             if (loopStructure)
                {
                if (!loopStructure->getEntry()->getPredecessors().empty())
                   isInvariantLoop = true;
                }
             }
          }

       if (isInvariantLoop)
          {
          TR_StructureSubGraphNode *node;
          TR_RegionStructure::Cursor si(*this);
          for (node = si.getCurrent(); node != NULL; node = si.getNext())
             node->getStructure()->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, true, loopInvariantBlock, loopStructure, bitVectorSize);

          if (insideLoop)
             {
             if (!(optSetInfo[loopInvariantBlock->getNumber()])->isEmpty())
                {
                TR_BitVector *optMask = new (trStackMemory()) TR_BitVector(bitVectorSize, trMemory(), stackAlloc);

                TR_BitVectorIterator bvi(*(optSetInfo[loopInvariantBlock->getNumber()]));
                int32_t nextOptimalComputation = -1;
                while (bvi.hasMoreElements())
                   {
                   nextOptimalComputation = bvi.getNextElement();
                   if (!(optSetInfo[outerLoopInvariantBlock->getNumber()])->get(nextOptimalComputation))
                      {
                      if (outerLoop->isExpressionTransparentIn(nextOptimalComputation, transparencyInfo))
                         {
                         TR::Block *innerLoopInvariantBlock = loopInvariantBlock->getBlock();
                         TR::TreeTop *exitTree = innerLoopInvariantBlock->getExit();
                         TR::TreeTop *currentTree = innerLoopInvariantBlock->getEntry();
                         TR::TreeTop *treeToBeHoisted = NULL;
                         while (currentTree != exitTree)
                            {
                            if (currentTree->getNode()->getOpCode().isCheck())
                               {
                               if (currentTree->getNode()->getLocalIndex() == nextOptimalComputation)
                                  {
                                  treeToBeHoisted = currentTree;
                                  break;
                                  }
                               }
                            else if (currentTree->getNode()->getOpCode().isStore())
                               {
                               TR::Node *currentNode = currentTree->getNode();

                               if (currentNode->getSymbolReference()->getSymbol()->isAuto())
                                   {
                                  if (currentNode->getFirstChild()->getLocalIndex() == nextOptimalComputation)
                                     {
                                     treeToBeHoisted = currentTree;
                                     break;
                                     }
                                  }
                               }
                            currentTree = currentTree->getNextTreeTop();
                            }

                         if (treeToBeHoisted != NULL)
                            {
                            TR::TreeTop *prevTree = treeToBeHoisted->getPrevTreeTop();
                            TR::TreeTop *nextTree = treeToBeHoisted->getNextTreeTop();
                            prevTree->setNextTreeTop(nextTree);
                            nextTree->setPrevTreeTop(prevTree);

                            TR::TreeTop *outerExitTree = outerLoopInvariantBlock->getBlock()->getLastRealTreeTop();

                            TR::TreeTop *outerPrevTree = outerExitTree->getPrevTreeTop();
                            treeToBeHoisted->setNextTreeTop(outerExitTree);
                            treeToBeHoisted->setPrevTreeTop(outerPrevTree);
                            outerPrevTree->setNextTreeTop(treeToBeHoisted);
                            outerExitTree->setPrevTreeTop(treeToBeHoisted);
                            }

                        dumpOptDetails(comp(), "\nO^O PARTIAL REDUNDANCY ELIMINATION: Hoisting loop invariant computation %d over nested loops (lifted from inner block_%d to outer block_%d)\n", nextOptimalComputation, loopInvariantBlock->getNumber(), outerLoopInvariantBlock->getNumber());

                        /////actually hoist the invariant
                        /////also remember to change the optSetInfo for the
                        /////two invariant blocks
                        optMask->set(nextOptimalComputation);
                        }
                     }
                  *(optSetInfo[loopInvariantBlock->getNumber()]) -= *optMask;
                  *(optSetInfo[outerLoopInvariantBlock->getNumber()]) |= *optMask;
                  }
               }
            }
         }
      }

   if (!isInvariantLoop)
      {
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*this);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         node->getStructure()->hoistInvariantsOutOfNestedLoops(transparencyInfo, optSetInfo, insideLoop, outerLoopInvariantBlock, outerLoop, bitVectorSize);
      }
   }




bool TR_BlockStructure::isExpressionTransparentIn(int32_t nextOptimalComputation, TR_LocalTransparency *transparencyInfo)
   {
   TR_LocalTransparency::ContainerType *info = transparencyInfo->getAnalysisInfo(getNumber());
   if (info && info->get(nextOptimalComputation))
      return true;

   return false;
   }



bool TR_RegionStructure::isExpressionTransparentIn(int32_t nextOptimalComputation, TR_LocalTransparency *transparencyInfo)
   {
   TR_StructureSubGraphNode *node;
   TR_RegionStructure::Cursor si(*this);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      if (!node->getStructure()->isExpressionTransparentIn(nextOptimalComputation, transparencyInfo))
         return false;
      }

   return true;
   }


TR_InductionVariable* TR_RegionStructure::findMatchingIV(TR::SymbolReference *symRef)
   {
   TR::Symbol* symbol = symRef->getSymbol();
   for(TR_InductionVariable *v = getFirstInductionVariable(); v; v = v->getNext())
      {
      if (symbol == v->getLocal())
         return v;
      }
   return NULL;
   }
