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

#ifndef LOOPVERSIONER_INCL
#define LOOPVERSIONER_INCL

#include <stddef.h>
#include <stdint.h>
#include <map>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/BitVector.hpp"
#include "infra/Checklist.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/LoopCanonicalizer.hpp"

class TR_PostDominators;
class TR_RegionStructure;
class TR_Structure;
namespace TR { class Block; }
namespace TR { class Optimization; }

class TR_NodeParentSymRef
   {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentSymRef(TR::Node *node, TR::Node *parent, TR::SymbolReference *symRef)
      : _node(node), _parent(parent), _symRef(symRef)
      {
      }

   TR::Node *_node;
   TR::Node *_parent;
   TR::SymbolReference *_symRef;
   };

class TR_NodeParentSymRefWeightTuple
   {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentSymRefWeightTuple(TR::Node *node, TR::Node *parent, TR::SymbolReference *symRef, int32_t weight)
      : _node(node), _parent(parent), _symRef(symRef), _weight(weight)
      {
      }

   TR::Node *_node;
   TR::Node *_parent;
   TR::SymbolReference *_symRef;
   int32_t _weight;
   };


struct VirtualGuardPair
   {
   TR::Block *_hotGuardBlock;
   TR::Block *_coldGuardBlock;
   bool _isGuarded;
   TR::Block *_coldGuardLoopEntryBlock;
   bool _isInsideInnerLoop;
   };

struct VGLoopInfo
   {
   VGLoopInfo(): _count(0), _coldGuardLoopInvariantBlock(NULL), _loopTempSymRef(NULL), _targetNum(-1) {}
   int32_t _count;
   TR::Block *_coldGuardLoopInvariantBlock;
   TR::SymbolReference *_loopTempSymRef;
   int32_t _targetNum;
   };

struct VirtualGuardInfo : public TR_Link<VirtualGuardInfo>
   {
   VirtualGuardInfo(TR::Compilation *c)
      :_virtualGuardPairs(c->trMemory())
      {

      _virtualGuardPairs.deleteAll();
      _coldLoopEntryBlock = NULL;
      _coldLoopInvariantBlock = NULL;
      _loopEntry = NULL;
      }
   List<VirtualGuardPair> _virtualGuardPairs;
   TR::Block *_coldLoopEntryBlock;
   TR::Block *_coldLoopInvariantBlock;
   TR::Block *_loopEntry;
   };

 class TR_NodeParentBlockTuple
    {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentBlockTuple(TR::Node *node, TR::Node *parent,TR::Block *block)
      : _node(node), _parent(parent),_block(block)
      {
      }

    TR::Node  *_node;
    TR::Node  *_parent;
    TR::Block *_block;
   };

struct LoopTemps : public TR_Link<LoopTemps>
   {
   LoopTemps(): _symRef(NULL), _maxValue(0) {}
   TR::SymbolReference *_symRef;
   int32_t _maxValue;
   };


/**
 * Class TR_LoopVersioner
 * ======================
 *
 * The loop versioner optimization eliminates loop invariant null checks, 
 * bound checks, div checks, checkcasts and inline guards (where 
 * the "this" pointer does not change in the loop) from loops by 
 * placing equivalent tests outside the loop. Also eliminates bound 
 * checks where the range of values of the index inside the loop is 
 * discovered (must be compile-time constant) and checks are inserted 
 * outside the loop that ensure that no bound checks fail inside the loop. 
 * This analysis uses induction variable information found by value 
 * propagation. 
 * 
 * Another versioning test that is emitted ensures the loop will not 
 * run for a very long period of time (in which case the async check 
 * inside the loop can be removed). This special versioning test depends 
 * on the number of trees (code) inside the loop.
 *
 *
 * Privatization and Deferred Transformations
 * ------------------------------------------
 *
 * Privatization allows versioning with respect to mutable data writable by
 * other threads by ensuring a stable value throughout the series of versioning
 * tests and the loop body. To illustrate, consider the following loop (which
 * is written in Java-like syntax, though the principle is general):
 *
 *     for (int i = 0; i < n; i++) {
 *         ... arrayWrapper.array[i] ... // implicit BNDCHK
 *     }
 *
 * In order to remove the bound check, it's necessary to generate tests ahead
 * of the loop that verify that <tt>i >= 0</tt> and that
 * <tt>n <= arrayWrapper.array.length</tt>. However, these tests are
 * insufficient so long as <tt>arrayWrapper.array</tt> is re-read on each loop
 * iteration. If another thread writes to <tt>arrayWrapper.array</tt> while the
 * loop is running, it can invalidate the result of the versioning test
 * comparing against <tt>arrayWrapper.array.length</tt>. However, by
 * privatizing it's still possible to remove the bound check as long as the
 * loop contains no calls and no synchronization (after all versioning
 * transformations). Before the versioning test, <tt>arrayWrapper.array</tt>
 * can be loaded and the result saved into a temp. Then if the versioning test
 * and the loop body both use the temp instead of <tt>arrayWrapper.array</tt>,
 * then each iteration is guaranteed to access the same array against which the
 * versioning test was run.
 *
 * Because privatization is not always possible, transformations relying on it
 * cannot be performed until it has been determined that privatization is
 * allowed. This determination is not made until all desirable transformations
 * have been identified, because it may be those very transformations that make
 * privatization possible, e.g. by removing a cold call from the hot loop.
 * Therefore, versioner can no longer transform the loop immediately after
 * generating a versioning test. Instead, the transformation must be deferred.
 * Furthermore, in cases where privatization is impossible, and where as a
 * result some transformations must be abandoned, any versioning tests created
 * for those transformations are useless and should not be emitted. So it is
 * also desirable to defer the addition of the versioning tests to the IL (via
 * \c comparisonTrees).
 *
 * When versioner wants to transform a loop based on a versioning test, it will
 * now remember the versioning test (as a LoopEntryPrep) and the transformation
 * (as a LoopImprovement) for later. The LoopEntryPrep is created from a
 * TR::Node using createLoopEntryPrep(), which is allowed to fail. If no
 * LoopEntryPrep could be created, then the transformation must not be
 * performed. The LoopEntryPrep captures all tests and privatizations necessary
 * to allow the transformation. Then if the transformation would completely
 * remove a check or a branch, versioner will use nodeWillBeRemovedIfPossible()
 * to remember that fact. This way a later analysis can determine what the loop
 * \em would look like if all such transformations were performed. **Note that
 * to ensure that later analysis results are reliable, it becomes mandatory at
 * this point to perform the transformation if at all possible.** Finally,
 * versioner will create a LoopImprovement to carry out the transformation.
 * Each LoopImprovement knows the LoopEntryPrep that it requires.
 *
 * After generating all of the \ref LoopImprovement "LoopImprovements" and
 * their corresponding \ref LoopEntryPrep "LoopEntryPreps", versioner will use
 * LoopBodySearch to analyze the loop and determine whether privatization is
 * possible, i.e. whether after versioning there will be any function calls
 * remaining in the loop. This analysis does not look for synchronization,
 * because in the presence of synchronization inside the loop, loads that would
 * need to be privatized are not considered to be invariant.
 *
 * Having determined whether or not privatization is allowed, versioner will
 * then look at each LoopImprovement that is still possible, emit its
 * LoopEntryPrep, and make the improvement.
 *
 * Finally, versioner will search the body of the hot loop for occurrences of
 * expressions that have been privatized, and substitute loads of the temps
 * into which their values have been stored (substitutePrivTemps()).
 *
 * The representation of expressions in LoopEntryPrep is Expr, which is
 * interned to help efficiently deduplicate preparations, and efficiently
 * identify occurrences of privatized expressions within the loop.
 *
 * All data pertaining to the deferral of transformations, interned
 * expressions, nodes that may be removed, privatization temps, etc., including
 * instances of LoopImprovement and LoopEntryPrep, for a particular loop are
 * scoped to the consideration of that loop. Data structures tracking this
 * information are in CurLoop, and they generally allocate using its \ref
 * CurLoop::_memRegion "_memRegion", so that they will naturally be freed
 * before versioner considers a different loop.
 *
 * Privatization-related environment variables **for debugging purposes only**:
 * - \c TR_nothingRequiresPrivatizationInVersioner: assume that privatization
 *   is unnecessary *and* that cold calls will be removed from the loop (this
 *   is what versioner used to assume)
 * - \c TR_assumeSingleThreadedVersioning: assume that privatization is
 *   unnecessary, but check to ensure no cold calls could overwrite state
 *   observed in the versioning tests, as though the program is single-threaded
 * - \c TR_assertRepresentableInVersioner: crash on unrepresentable expressions
 * - \c TR_allowGuardForVersioning: allow versioning certain guards based on
 *   kind and test type (see guardOkForExpr())
 *
 * Privatization-related environment variables that are safe/conservative:
 * - \c TR_paranoidVersioning: assume that globals are non-invariant,
 *   preventing transformations that require privatization
 * - \c TR_forbidGuardForVersioning: forbid versioning certain guards based on
 *   kind and test type (see guardOkForExpr())
 *
 * Privatization-related debug counters:
 * - \c staticDebugCounters={loopVersioner.unrepresentable/*}
 *
 * \see LoopImprovement
 * \see LoopEntryPrep
 * \see Expr
 * \see CurLoop
 * \see LoopBodySearch
 */

class TR_LoopVersioner : public TR_LoopTransformer
   {
   public:

   TR_LoopVersioner(TR::OptimizationManager *manager, bool onlySpecialize = false, bool refineAliases = false);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopVersioner(manager);
      }

   virtual int32_t perform();
   virtual TR_LoopVersioner *asLoopVersioner() {return this;}

   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t);
   virtual bool isStoreInRequiredForm(int32_t, TR_Structure *);

   virtual const char * optDetailString() const throw();

   protected:

   /**
    * \brief An expression that may be emitted outside the loop.
    *
    * These expressions are interned to easily avoid generating duplicate
    * \ref LoopEntryPrep "LoopEntryPreps", and to facilitate the bottom-up
    * identification of occurrences of each expression within the loop for
    * privatization.
    *
    * The canonical copy of an expression could have been represented as a
    * TR::Node, but it is necessary nonetheless to have a type to serve as the
    * key with which expressions are associated, and furthermore, that key must
    * capture enough data to reproduce the desired node outside of the loop.
    * Otherwise, two meaningfully distinct expressions could be identified with
    * one another, causing one to be lost.
    *
    * There are a few limitations on the expressions that are representable,
    * most notably \ref MAX_CHILDREN, enforced in initExprFromNode(). If an
    * expression is unrepresentable, the loop will not be transformed in such a
    * way as to require that expression to be generated outside the loop. To
    * this end, transformations using LoopEntryPrep must be prepared for
    * createLoopEntryPrep() to fail. For transformations that still use
    * collectAllExpressionsToBeChecked(), which is now deprecated, an
    * unrepresentable subtree will cause a compilation failure. This is a
    * drastic measure, but such transformations have no other recourse.
    *
    * Unrepresentable expressions are very rarely encountered amongst those
    * undergoing code motion during versioning. To see that this is the case,
    * it is possible to assert in makeCanonicalExpr() that all expressions that
    * might undergo code motion are representable by <tt>#define</tt>-ing \c
    * ASSERT_REPRESENTABLE_IN_VERSIONER or setting the environment variable \c
    * TR_assertRepresentableInVersioner.
    *
    * Furthermore, to help detect cases where optimization is inhibited because
    * of an unrepresentable expression, static debug counters are available:
    * <tt>staticDebugCounters={loopVersioner.unrepresentable/*}</tt>.
    *
    * For an overview of privatization and the deferral of transformations see
    * TR_LoopVersioner.
    *
    * \see makeCanonicalExpr()
    * \see findCanonicalExpr()
    * \see emitExpr()
    * \see substitutePrivTemps()
    */
   struct Expr
      {
      TR_ALLOC(TR_Memory::LoopTransformer);

      enum
         {
         /// The maximum number of children representable.
         MAX_CHILDREN = 3,
         };

      /// The operation at the root of this expression.
      TR::ILOpCode _op;

      union
         {
         /// The constant value, when <tt>_op.isLoadConst()</tt>.
         int64_t _constValue;

         /// The symbol reference, when <tt>_op.hasSymbolReference()</tt>.
         TR::SymbolReference *_symRef;
         };

      /**
       * \brief Flags that can't be cleared,
       * e.g. TR::Node::isClassPointerConstant().
       *
       * Only flags that affect the semantics of a node should be included.
       * Otherwise irrelevant flags set on original nodes in the loop will
       * prevent substitution of privatization temps.
       */
      flags32_t _mandatoryFlags;

      /**
       * \brief The immediate subexpressions.
       *
       * If there are fewer than \ref MAX_CHILDREN children, trailing elements
       * are set to null.
       */
      const Expr *_children[MAX_CHILDREN];

      /**
       * \brief Best-effort bytecode info.
       *
       * This is ignored in comparisons, so it comes from an arbitrary matching
       * node. It is only used to ensure that nodes generated outside the loop
       * have \em some valid bytecode info.
       */
      TR_ByteCodeInfo _bci;

      /**
       * \brief Best-effort flags.
       *
       * This must be a superset of \ref _mandatoryFlags. It is ignored in
       * comparisons, so it comes from an arbitrary matching node. This
       * preserves optional flags such as TR::Node::isMaxLoopIterationGuard().
       */
      flags32_t _flags;

      bool operator<(const Expr &rhs) const;
      };

   /**
    * \brief A deferred transformation consisting of an expression to be
    * emitted outside the loop in preparation for loop entry.
    *
    * Each such expression is either a versioning test (\ref TEST), or a value
    * to be privatized (\ref PRIVATIZE).
    *
    * Some expressions are not safe to evaluate unconditionally and need
    * prerequisite safety tests, and some rely on one or more privatizations,
    * so \ref LoopEntryPrep "LoopEntryPreps" form a dependency graph, and they
    * are emitted in dependency order. The prerequisite safety tests are the
    * ones that were generated by collectAllExpressionsToBeChecked(), which is
    * now deprecated.
    *
    * Instances are deduplicated to avoid generating (in some cases large
    * numbers of) redundant versioning tests.
    *
    * For an overview of privatization and the deferral of transformations see
    * TR_LoopVersioner.
    *
    * \see createLoopEntryPrep()
    * \see createChainedLoopEntryPrep()
    * \see depsForLoopEntryPrep()
    * \see emitPrep()
    * \see unsafelyEmitAllTests()
    */
   struct LoopEntryPrep
      {
      TR_ALLOC(TR_Memory::LoopTransformer);

      enum Kind
         {
         TEST, ///< A versioning test
         PRIVATIZE, ///< An expression to be privatized
         };

      /**
       * \brief Construct an instance of LoopEntryPrep. This is probably not
       * what you want! Use createLoopEntryPrep().
       *
       * \param kind The kind of LoopEntryPrep to initialize
       * \param expr The expression to be emitted ahead of the loop
       * \param memoryRegion The memory region to use for the dependency list
       */
      LoopEntryPrep(Kind kind, const Expr *expr, TR::Region &memoryRegion)
         : _kind(kind)
         , _expr(expr)
         , _deps(memoryRegion)
         , _emitted(false)
         , _requiresPrivatization(false)
         , _unsafelyEmitted(false)
         {}

      /// Distinguishes versioning tests from privatizations.
      const Kind _kind;

      /**
       * \brief The expression to emitted ahead of the loop.
       *
       * For tests, this is the entire conditional. For privatizations, this is
       * only the value, not including the store, which is generated later.
       */
      const Expr * const _expr;

      /**
       * \brief Dependencies of this LoopEntryPrep.
       *
       * These are other tests and privatizations that must run before this one
       * to ensure that this is safe to evaluate.
       */
      TR::list<LoopEntryPrep*, TR::Region&> _deps;

      /**
       * \brief True if this LoopEntryPrep has already been emitted.
       *
       * This is used to ensure that no LoopEntryPrep is emitted multiple times.
       *
       * \see emitPrep()
       */
      bool _emitted;

      /// True when either this LoopEntryPrep or a (transitive) dependency privatizes.
      bool _requiresPrivatization;

      /**
       * \brief True if this LoopEntryPrep has already been "unsafely emitted."
       *
       * This is used to ensure that no LoopEntryPrep is unsafely emitted
       * multiple times. It can be removed once all versioning transformations
       * have been updated to use LoopEntryPrep and LoopImprovement.
       *
       * \see unsafelyEmitAllTests()
       */
      bool _unsafelyEmitted;
      };

   /**
    * \brief A deferred transformation that can improve a loop body, so long as
    * a particular LoopEntryPrep (typically a versioning test) is emitted.
    *
    * For an overview of privatization and the deferral of transformations see
    * TR_LoopVersioner.
    */
   class LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      /**
       * Construct this LoopImprovement.
       *
       * \param versioner The optimization pass object
       * \param prep The LoopEntryPrep required to allow for this improvement
       */
      LoopImprovement(TR_LoopVersioner *versioner, LoopEntryPrep *prep)
         : _versioner(versioner), _prep(prep)
         {}

      /// Improve the loop, for example by removing a check.
      virtual void improveLoop() = 0;

      TR::Compilation *comp() { return _versioner->comp(); }
      TR_FrontEnd *fe() { return _versioner->fe(); }

      TR_LoopVersioner * const _versioner;
      LoopEntryPrep * const _prep;
      };

   /**
    * \brief An iterator over all trees in the loop body reachable with certain
    * assumptions, in breadth-first order.
    *
    * This iterator accepts a set of (root-level) nodes to ignore and provides
    * a view of the loop as though those nodes were already removed. If a
    * branch is to be ignored, it is assumed to fall through unless it is
    * present in a set of taken branches, in which case it is assumed to be
    * taken. If a branch is not in the set of ignored nodes, but it \em is an
    * integer equality comparison (\c ificmpeq or \c ificmpne) where both
    * children are constant (\c iconst), then the dead outgoing edge will be
    * ignored as well. This ignores paths that have already been logically
    * eliminated by versioner, e.g. while processing an outer loop, since
    * versioner puts branches into this form instead of folding them outright.
    *
    * The sets of ignored nodes and taken branches can be modified while
    * iteration is underway, and the modifications will be respected so long as
    * the affected nodes have not yet been encountered (where ignoring a branch
    * affects the \c BBEnd node rather than the branch itself, since the search
    * waits until finding \c BBEnd before enqueueing the successors).
    *
    * Example usage (with no nodes assumed to be removed):
    *
    *     TR::NodeChecklist empty(comp());
    *     LoopBodySearch search(comp, memRegion, loop, &empty, &empty);
    *     for (; search.hasTreeTop(); search.advance())
    *        {
    *        TR::TreeTop *tt = search.currentTreeTop();
    *        // ...
    *        }
    *
    * This search is used to analyze a loop as though a number of improvements
    * have already been made to it, to determine whether after making those
    * improvements there will be code in the loop interfering with certain
    * transformations.
    *
    * For an overview of privatization and the deferral of transformations see
    * TR_LoopVersioner.
    */
   class LoopBodySearch
      {
      public:
      LoopBodySearch(
         TR::Compilation *comp,
         TR::Region &memRegion,
         TR_RegionStructure *loop,
         TR::NodeChecklist *removedNodes,
         TR::NodeChecklist *takenBranches);

      /**
       * \brief Determine whether iteration is still in progress.
       *
       * \return true when iteration is still in progress and there is a
       * current tree
       */
      bool hasTreeTop() { return _currentTreeTop != NULL; }

      /**
       * \brief Get the block containing the current tree.
       *
       * \return the block, or null if iteration has finished
       */
      TR::Block *currentBlock() { return _currentBlock; }

      /**
       * \brief Get the current tree.
       *
       * \return the tree, or null if iteration has finished
       */
      TR::TreeTop *currentTreeTop() { return _currentTreeTop; }

      void advance();

      private:
      bool isBranchConstant(TR::Node *ifNode);
      bool isConstantBranchTaken(TR::Node *ifNode);
      void enqueueReachableSuccessorsInLoop();
      void enqueueReachableSuccessorsInLoopFrom(TR::CFGEdgeList &outgoingEdges);
      void enqueueBlockIfInLoop(TR::TreeTop *entry);
      void enqueueBlockIfInLoop(TR::Block *block);

      TR_RegionStructure * const _loop;
      TR::NodeChecklist * const _removedNodes;
      TR::NodeChecklist * const _takenBranches;
      TR::list<TR::Block*, TR::Region&> _queue;
      TR::BlockChecklist _alreadyEnqueuedBlocks;
      TR::Block *_currentBlock;
      TR::TreeTop *_currentTreeTop;
      bool _blockHasExceptionPoint;
      };

   struct PrepKey
      {
      PrepKey(LoopEntryPrep::Kind kind, const Expr *expr, LoopEntryPrep *prev)
         : _kind(kind), _expr(expr), _prev(prev) {}

      const LoopEntryPrep::Kind _kind;
      const Expr * const _expr;
      LoopEntryPrep * const _prev;

      bool operator<(const PrepKey &rhs) const;
      };

   struct PrivTemp
      {
      PrivTemp(TR::SymbolReference *symRef, TR::DataType type)
         : _symRef(symRef), _type(type) {}

      TR::SymbolReference * const _symRef;
      const TR::DataType _type;
      };

   typedef TR::typed_allocator<std::pair<const Expr, const Expr*>, TR::Region&> ExprTableAlloc;
   typedef std::map<Expr, const Expr*, std::less<Expr>, ExprTableAlloc> ExprTable;

   typedef TR::typed_allocator<std::pair<TR::Node * const, const Expr*>, TR::Region&> NodeToExprAlloc;
   typedef std::map<TR::Node*, const Expr*, std::less<TR::Node*>, NodeToExprAlloc> NodeToExpr;

   typedef TR::typed_allocator<std::pair<const PrepKey, LoopEntryPrep*>, TR::Region&> PrepTableAlloc;
   typedef std::map<PrepKey, LoopEntryPrep*, std::less<PrepKey>, PrepTableAlloc> PrepTable;

   typedef TR::typed_allocator<std::pair<const Expr * const, LoopEntryPrep*>, TR::Region&> NullTestPrepMapAlloc;
   typedef std::map<const Expr*, LoopEntryPrep*, std::less<const Expr*>, NullTestPrepMapAlloc> NullTestPrepMap;

   typedef TR::typed_allocator<std::pair<TR::Node * const, LoopEntryPrep*>, TR::Region&> NodePrepMapAlloc;
   typedef std::map<TR::Node*, LoopEntryPrep*, std::less<TR::Node*>, NodePrepMapAlloc> NodePrepMap;

   typedef TR::typed_allocator<std::pair<const Expr * const, PrivTemp>, TR::Region&> PrivTempMapAlloc;
   typedef std::map<const Expr*, PrivTemp, std::less<const Expr*>, PrivTempMapAlloc> PrivTempMap;

   typedef TR::typed_allocator<std::pair<const Expr * const, TR::Node*>, TR::Region&> EmitExprMemoAlloc;
   typedef std::map<const Expr*, TR::Node*, std::less<const Expr*>, EmitExprMemoAlloc> EmitExprMemo;

   /**
    * \brief Information about the loop currently under consideration by
    * TR_LoopVersioner.
    *
    * For an overview of privatization and the deferral of transformations see
    * TR_LoopVersioner.
    */
   struct CurLoop
      {
      CurLoop(TR::Compilation *comp, TR::Region &memRegion, TR_RegionStructure *loop);

      /// A memory region that is freed when versioner finishes processing this loop.
      TR::Region &_memRegion;

      /// The structure of the current loop.
      TR_RegionStructure * const _loop;

      /// The intern table for Expr.
      ExprTable _exprTable;

      /// Memoized results for the Expr corresponding to a TR::Node.
      NodeToExpr _nodeToExpr;

      /// Table of LoopEntryPrep objects for deduplication.
      PrepTable _prepTable;

      /// Map from null-tested Expr to the null test LoopEntryPrep.
      NullTestPrepMap _nullTestPreps;

      /**
       * \brief Map from \c BNDCHKwithSpineCheck node to bound check LoopEntryPrep.
       *
       * This allows the LoopEntryPrep for spine check removal to depend on the
       * one for bound check removal.
       */
      NodePrepMap _boundCheckPrepsWithSpineChecks;

      /// Check and branch nodes that will definitely be removed.
      TR::NodeChecklist _definitelyRemovableNodes;

      /// Check and branch nodes that will be removed if privatization is allowed.
      TR::NodeChecklist _optimisticallyRemovableNodes;

      /// Branch nodes that, if removed, will be taken.
      TR::NodeChecklist _takenBranches;

      /// All \ref LoopImprovement "LoopImprovements" for the current loop
      TR::list<LoopImprovement*, TR::Region&> _loopImprovements;

      /// A map from canonical Expr to privatization temp. \see emitPrep()
      PrivTempMap _privTemps;

      /// True if any privatizing LoopEntryPrep has been created.
      bool _privatizationsRequested;

      /// The result of the analysis to say whether privatization can be done.
      bool _privatizationOK;
      };

   class Hoist : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      Hoist(TR_LoopVersioner *versioner, LoopEntryPrep *prep)
         : LoopImprovement(versioner, prep)
         {}

      virtual void improveLoop()
         {
         // Nothing to do. This just pulls in a privatization, which, once
         // emitted, makes the substitution mandatory throughout the entire
         // loop body.
         }
      };

   class RemoveAsyncCheck : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveAsyncCheck(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::TreeTop *asyncCheckTree)
         : LoopImprovement(versioner, prep)
         , _asyncCheckTree(asyncCheckTree)
         {}

      virtual void improveLoop();

      private:
      TR::TreeTop * const _asyncCheckTree;
      };

   class RemoveNullCheck : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveNullCheck(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::Node *nullCheckNode)
         : LoopImprovement(versioner, prep)
         , _nullCheckNode(nullCheckNode)
         {}

      virtual void improveLoop();

      private:
      TR::Node * const _nullCheckNode;
      };

   class RemoveBoundCheck : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveBoundCheck(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::TreeTop *boundCheckTree)
         : LoopImprovement(versioner, prep)
         , _boundCheckTree(boundCheckTree)
         {}

      virtual void improveLoop();

      private:
      TR::TreeTop * const _boundCheckTree;
      };

   class RemoveSpineCheck : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveSpineCheck(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::TreeTop *spineCheckTree)
         : LoopImprovement(versioner, prep)
         , _spineCheckTree(spineCheckTree)
         {}

      virtual void improveLoop();

      private:
      TR::TreeTop * const _spineCheckTree;
      };

   class RemoveDivCheck : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveDivCheck(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::Node *divCheckNode)
         : LoopImprovement(versioner, prep)
         , _divCheckNode(divCheckNode)
         {}

      virtual void improveLoop();

      private:
      TR::Node * const _divCheckNode;
      };

   class RemoveWriteBarrier : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveWriteBarrier(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::Node *awrtbariNode)
         : LoopImprovement(versioner, prep)
         , _awrtbariNode(awrtbariNode)
         {}

      virtual void improveLoop();

      private:
      TR::Node * const _awrtbariNode;
      };

   class RemoveCheckCast : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveCheckCast(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::TreeTop *checkCastTree)
         : LoopImprovement(versioner, prep)
         , _checkCastTree(checkCastTree) {}

      virtual void improveLoop();

      private:
      TR::TreeTop * const _checkCastTree;
      };

   class RemoveArrayStoreCheck : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      RemoveArrayStoreCheck(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::TreeTop *arrayStoreCheckTree)
         : LoopImprovement(versioner, prep)
         , _arrayStoreCheckTree(arrayStoreCheckTree)
         {}

      virtual void improveLoop();

      private:
      TR::TreeTop * const _arrayStoreCheckTree;
      };

   class FoldConditional : public LoopImprovement
      {
      public:
      TR_ALLOC(TR_Memory::LoopTransformer)

      FoldConditional(
         TR_LoopVersioner *versioner,
         LoopEntryPrep *prep,
         TR::Node *conditionalNode,
         bool reverseBranch,
         bool original)
         : LoopImprovement(versioner, prep)
         , _conditionalNode(conditionalNode)
         , _reverseBranch(reverseBranch)
         , _original(original)
         {}

      virtual void improveLoop();

      private:
      TR::Node * const _conditionalNode;
      const bool _reverseBranch;
      const bool _original;
      };

   bool shouldOnlySpecializeLoops() { return _onlySpecializingLoops; }
   void setOnlySpecializeLoops(bool b) { _onlySpecializingLoops = b; }

   /* currently do not support wcode methods
   * or realtime since aliasing of arraylets are not handled
   */
   bool refineAliases() { return _refineLoopAliases && !comp()->generateArraylets(); }

   virtual bool processArrayAliasCandidates();

   virtual void collectArrayAliasCandidates(TR::Node *node,vcount_t visitCount);
   virtual void buildAliasRefinementComparisonTrees(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, TR_ScratchList<TR::Node> *, TR::Block *);
   virtual void initAdditionalDataStructures();
   virtual void refineArrayAliases(TR_RegionStructure *);

   int32_t performWithDominators();
   int32_t performWithoutDominators();

   bool isStoreInSpecialForm(int32_t, TR_Structure *);
   bool isConditionalTreeCandidateForElimination(TR::TreeTop * curTree) { return (!curTree->getNode()->getFirstChild()->getOpCode().isLoadConst() ||
                                                                                  !curTree->getNode()->getSecondChild()->getOpCode().isLoadConst()); };
   TR::Node *isDependentOnInductionVariable(TR::Node *, bool, bool &, TR::Node* &, TR::Node* &, bool &);
   TR::Node *isDependentOnInvariant(TR::Node *);
   bool boundCheckUsesUnchangedValue(TR::TreeTop *, TR::Node *, TR::SymbolReference *, TR_RegionStructure *);
   bool findStore(TR::TreeTop *start, TR::TreeTop *end, TR::Node *node, TR::SymbolReference *symRef, bool ignoreLoads = false, bool lastTimeThrough = false);
   TR::Node *findLoad(TR::Node *node, TR::SymbolReference *symRef, vcount_t origVisitCount);
   void versionNaturalLoop(TR_RegionStructure *, List<TR::Node> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, List<TR_NodeParentSymRef> *, List<TR_NodeParentSymRefWeightTuple> *, List<TR_Structure> *whileLoops, List<TR_Structure> *clonedInnerWhileLoops, bool skipVersioningAsynchk, SharedSparseBitVector &reverseBranchInLoops);

   TR::Node *findCallNodeInBlockForGuard(TR::Node *node);
   bool loopIsWorthVersioning(TR_RegionStructure *naturalLoop);

   bool detectInvariantChecks(List<TR::Node> *, List<TR::TreeTop> *);
   bool detectInvariantBoundChecks(List<TR::TreeTop> *);
   bool detectInvariantSpineChecks(List<TR::TreeTop> *);
   bool detectInvariantDivChecks(List<TR::TreeTop> *);
   bool detectInvariantAwrtbaris(List<TR::TreeTop> *);
   bool detectInvariantTrees(TR_RegionStructure *whileLoop, List<TR::TreeTop> *, bool, bool *, SharedSparseBitVector &reverseBranchInLoops);
   bool detectInvariantNodes(List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodes);
   bool detectInvariantSpecializedExprs(List<TR::Node> *);
   bool detectInvariantArrayStoreChecks(List<TR::TreeTop> *);
   bool isVersionableArrayAccess(TR::Node *);

   bool isExprInvariant(TR::Node *, bool ignoreHeapificationStore = false);          // ignoreHeapificationStore flags for both
   bool isExprInvariantRecursive(TR::Node *, bool ignoreHeapificationStore = false); // methods need to be in sync!

   bool isDependentOnAllocation(TR::Node *, int32_t);
   bool hasWrtbarBeenSeen(List<TR::TreeTop> *, TR::Node *);

   bool checkProfiledGuardSuitability(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *guardNode, TR::SymbolReference *callSymRef, TR::Compilation *comp);
   bool isBranchSuitableToVersion(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *node, TR::Compilation *comp);
   bool isBranchSuitableToDoLoopTransfer(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *node, TR::Compilation *comp);

   bool detectChecksToBeEliminated(TR_RegionStructure *, List<TR::Node> *, List<TR::TreeTop> *, List<int32_t> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<int32_t> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, List<TR_NodeParentSymRef> *,List<TR_NodeParentSymRefWeightTuple> *, bool &);

   void buildNullCheckComparisonsTree(List<TR::Node> *, List<TR::TreeTop> *);
   void buildBoundCheckComparisonsTree(List<TR::TreeTop> *, List<TR::TreeTop> *, bool);
   void createRemoveBoundCheck(TR::TreeTop *, LoopEntryPrep *, List<TR::TreeTop> *);
   void buildSpineCheckComparisonsTree(List<TR::TreeTop> *);
   void buildDivCheckComparisonsTree(List<TR::TreeTop> *);
   void buildAwrtbariComparisonsTree(List<TR::TreeTop> *);
   void buildCheckCastComparisonsTree(List<TR::TreeTop> *);
   void buildConditionalTree(List<TR::TreeTop> *, SharedSparseBitVector &reverseBranchInLoops);
   void buildArrayStoreCheckComparisonsTree(List<TR::TreeTop> *);
   bool buildSpecializationTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, List<TR::Node> *, TR::Block *, TR::SymbolReference **);
   bool buildLoopInvariantTree(List<TR_NodeParentSymRef> *);
   void convertSpecializedLongsToInts(TR::Node *, vcount_t, TR::SymbolReference **);
   void collectAllExpressionsToBeChecked(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, TR::Node *, List<TR::Node> *, TR::Block *, vcount_t);
   void collectAllExpressionsToBeChecked(TR::Node *, List<TR::Node> *);
   bool requiresPrivatization(TR::Node *);
   bool suppressInvarianceAndPrivatization(TR::SymbolReference *);
   void dumpOptDetailsCreatingTest(const char *, TR::Node *);
   void dumpOptDetailsFailedToCreateTest(const char *, TR::Node *);
   bool depsForLoopEntryPrep(TR::Node *, TR::list<LoopEntryPrep*, TR::Region&> *, TR::NodeChecklist *, bool);
   LoopEntryPrep *addLoopEntryPrepDep(LoopEntryPrep::Kind, TR::Node *, TR::list<LoopEntryPrep*, TR::Region&> *, TR::NodeChecklist *);

   void copyOnWriteNode(TR::Node *original, TR::Node **current);

   void updateDefinitionsAndCollectProfiledExprs(TR::Node *,TR::Node *, vcount_t, List<TR::Node> *, List<TR_NodeParentSymRef> *, List<TR_NodeParentSymRefWeightTuple> *, TR::Node *, bool, TR::Block *, int32_t);
   void findAndReplaceContigArrayLen(TR::Node *, TR::Node *, vcount_t);
   void performLoopTransfer();

   bool replaceInductionVariable(TR::Node *, TR::Node *, int, int, TR::Node *, int);

   TR::Block *createEmptyGoto(TR::Block *source, TR::Block *dest, TR::TreeTop *endTree);
   TR::Block *createClonedHeader(TR::Block *origHeader, TR::TreeTop **endTree);
   TR::Node *createSwitchNode(TR::Block *clonedHeader, TR::SymbolReference *tempSymRef, int32_t numCase);
   bool canPredictIters(TR_RegionStructure* whileLoop, const TR_ScratchList<TR::Block>& blocksInWhileLoop,
      bool& isIncreasing, TR::SymbolReference*& firstChildSymRef);
   bool isInverseConversions(TR::Node* node);

   void recordCurrentBlock(TR::Block *b) { _currentBlock = b; }
   TR::Block * getCurrentBlock(TR::Block *b) { return _currentBlock; }

   bool initExprFromNode(Expr *expr, TR::Node *node, bool onlySearching);
   bool guardOkForExpr(TR::Node *node, bool onlySearching);
   const Expr *makeCanonicalExpr(TR::Node *node);
   const Expr *findCanonicalExpr(TR::Node *node);
   const Expr *substitutePrivTemps(TR::TreeTop *tt, TR::Node *node, TR::NodeChecklist *visited);
   TR::Node *emitExpr(const Expr *node);
   TR::Node *emitExpr(const Expr *node, EmitExprMemo &memo);
   void emitPrep(LoopEntryPrep *prep, List<TR::Node> *comparisonTrees);
   void unsafelyEmitAllTests(const TR::list<LoopEntryPrep*, TR::Region&> &, List<TR::Node> *);
   void setAndIncChildren(TR::Node *node, int n, TR::Node **children);
   void nodeWillBeRemovedIfPossible(TR::Node *node, LoopEntryPrep *prep);

   LoopEntryPrep *createLoopEntryPrep(LoopEntryPrep::Kind, TR::Node *, TR::NodeChecklist * = NULL, LoopEntryPrep * = NULL);
   LoopEntryPrep *createChainedLoopEntryPrep(LoopEntryPrep::Kind, TR::Node *, LoopEntryPrep *);
   void visitSubtree(TR::Node *node, TR::NodeChecklist *visited);

   TR_BitVector *_seenDefinedSymbolReferences;
   TR_BitVector *_additionInfo;
   TR::Node *_nullCheckReference, *_conditionalTree, *_duplicateConditionalTree;
   TR_RegionStructure *_currentNaturalLoop;

   List<int32_t> _versionableInductionVariables, _specialVersionableInductionVariables, _derivedVersionableInductionVariables;
   ////List<VirtualGuardPair> _virtualGuardPairs;
   TR_LinkHead<VirtualGuardInfo> _virtualGuardInfo;

   bool _containsGuard, _containsUnguardedCall, _nonInlineGuardConditionalsWillNotBeEliminated;

   int32_t _counter, _origNodeCount, _origBlockCount;
   bool _onlySpecializingLoops;
   bool _inNullCheckReference, _containsCall, _neitherLoopCold, _loopConditionInvariant;
   bool _refineLoopAliases;
   bool _addressingTooComplicated;
   List<TR::Node> _guardedCalls;
   List<TR::Node> *_arrayAccesses;
   List<TR_NodeParentBlockTuple> *_arrayLoadCandidates;
   List<TR_NodeParentBlockTuple> *_arrayMemberLoadCandidates;
   List<TR::TreeTop> _checksInDupHeader;
   TR_BitVector *_unchangedValueUsedInBndCheck;
   TR::Block *    _currentBlock;
   TR_BitVector *_disqualifiedRefinementCandidates;
   TR_BitVector               _visitedNodes; //used to track nodes visited by isExprInvariant

   TR_PostDominators *_postDominators;
   bool _loopTransferDone;

   /**
    * \brief The entry of the preheader of the duplicate (usually cold) loop.
    *
    * This is set in versionNaturalLoop() for each loop versioned.
    */
   TR::TreeTop *_exitGotoTarget;

   /// Data local to each \ref _currentNaturalLoop
   CurLoop *_curLoop;

   /// Whether to invalidate alias sets at the end of the pass.
   bool _invalidateAliasSets;
   };


/**
 * Class TR_LoopSpecializer
 * ========================
 *
 * The loop specializer optimization replaces loop-invariant expressions 
 * that are profiled and found to be constants, by the constant value 
 * after inserting a test outside the loop that compares the value to 
 * the constant. Note that this cannot be done in the the absence of 
 * value profiling infrastructure.
 */

class TR_LoopSpecializer : public TR_LoopVersioner
   {
   public:

   TR_LoopSpecializer(TR::OptimizationManager *manager);

   virtual const char * optDetailString() const throw();

   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopSpecializer(manager);
      }
   };


#endif
