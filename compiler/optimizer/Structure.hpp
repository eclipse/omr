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

#ifndef STRUCTURE_INCL
#define STRUCTURE_INCL

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "infra/vector.hpp"

#include "optimizer/VPConstraint.hpp"

class TR_BasicInductionVariable;
class TR_BlockStructure;
class TR_DataFlowAnalysis;
class TR_LocalTransparency;
class TR_PrimaryInductionVariable;
class TR_RegionStructure;
class TR_RegisterCandidate;
class TR_StructureSubGraphNode;
namespace TR { class VPConstraint; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class SymbolReference; }

class TR_Structure
   {
   public:
   TR_ALLOC(TR_Memory::Structure)

   enum Kind
      {
      Blank       = 0,
      Block       = 1,
      Region      = 2
      };

   TR_Structure(TR::Compilation * c, int32_t index)
      : _comp(c), _nodeIndex(index), _parent(NULL), _weight(-1), _versionedStructure(NULL), _trMemory(c->trMemory()), _maxNestingDepth(0), _nestingDepth(0), _analyzedBefore(false), _containsImproperRegion(false)
      {
      }

   TR::Compilation * comp() { return _comp; }
   TR::CFG *cfg() { return _comp->getFlowGraph(); }

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }
   TR_PersistentMemory *     trPersistentMemory()          { return _trMemory->trPersistentMemory(); }

   virtual Kind getKind();
   virtual TR_BlockStructure        *asBlock() { return NULL; }
   virtual TR_RegionStructure       *asRegion() { return NULL; }

   virtual TR::Block *getEntryBlock() = 0;

//   TR_Structure *getParent()                {return _parent;}
   TR_RegionStructure *getParent() { return _parent; }
   TR_RegionStructure *setParent(TR_RegionStructure *b) {return (_parent = b);}

   TR_StructureSubGraphNode * getSubGraphNode() {return _graphNode;}
   void setSubGraphNode(TR_StructureSubGraphNode * node) {_graphNode = node;}

   int32_t getNumber()                {return _nodeIndex;}
   void    setNumber(int32_t n)       {_nodeIndex = n;}

   // Does this structure contain the other? If the "commonParent" is known it can
   // be specified to make the search faster.
   //
   bool  contains(TR_Structure *other, TR_Structure *commonParent = NULL);

   TR_RegionStructure *getContainingLoop();

   // Finds the common parent of this and other
   // note that if A contains B, then the common parent is parent(A)
   //
   TR_RegionStructure *findCommonParent(TR_Structure *other, TR::CFG *cfg);


   // Perform the data flow analysis on the structure.
   // Return true if the analysis info has changed as a result of analysis.
   //
   virtual bool doDataFlowAnalysis(TR_DataFlowAnalysis *, bool checkForChanges) = 0;

   virtual bool changeContinueLoopsToNestedLoops(TR_RegionStructure *) = 0;

   int32_t getNumberOfLoops();

   virtual void checkStructure(TR_BitVector *) = 0;
   void         mergeBlocks(TR::Block *first, TR::Block *second);
   virtual void mergeInto(TR::Block *first, TR::Block *second);
   virtual void removeMergedBlock(TR::Block *merged, TR::Block *mergedInto);
   virtual void renumber(int32_t num) = 0;
   virtual bool renumberRecursively(int32_t origNum, int32_t num) = 0;

   virtual void resetVisitCounts(vcount_t) = 0;

   // Remove an edge between the given structures. The "to" structure is part
   // of this structure; the "from" structure may or may not be.
   //
   virtual void removeEdge(TR_Structure *from, TR_Structure *to) = 0;

   virtual void addEdge(TR::CFGEdge *edge, bool isExceptionEdge);
   virtual void addExternalEdge(TR_Structure *from, int32_t toNumber, bool isExceptionEdge) = 0;

   // Replace one of the sub-structures of this structure by another.
   //
   virtual void replacePart(TR_Structure *from, TR_Structure *to) = 0;

   // Analysis info can be used by any data flow analysis to store information
   // relevant to this structure. It is not necessarily valid at the start of
   // an analysis.
   //
   void *getAnalysisInfo()                      {return _analysisInfo;}
   void  setAnalysisInfo(void *newInfo)         { _analysisInfo = newInfo; }

   virtual void clearAnalysisInfo() = 0;
   virtual void resetAnalysisInfo() = 0;
   virtual void resetAnalyzedStatus() = 0;
   virtual bool markStructuresWithImproperRegions() = 0;
   virtual void collectExitBlocks(List<TR::Block> *exitBlocks, List<TR::CFGEdge> *exitEdges = NULL) = 0;
   virtual TR_Structure *cloneStructure(TR::Block **, TR_StructureSubGraphNode **,  List<TR_Structure> *, List<TR_Structure> *) = 0;
   virtual void cloneStructureEdges(TR::Block **) = 0;
   virtual void collectCFGEdgesTo(int32_t, List<TR::CFGEdge> *) = 0;
   virtual int32_t getMaxNestingDepth(int32_t *, int32_t *);
   void setNestingDepths(int32_t *);
   void setAnyCyclicRegionNestingDepths(int32_t *);
   void calculateFrequencyOfExecution(int32_t *);

   void setConditionalityWeight(int32_t *);
   void adjustWeightForBranches(TR_StructureSubGraphNode *, TR_StructureSubGraphNode *, int32_t *);

   TR_StructureSubGraphNode *findNodeInHierarchy(TR_RegionStructure *, int32_t);

   void setWeight(int32_t w) {_weight = w;}
   int32_t getWeight() {return _weight;}

   // Determine if this region contains improper region
   //
   virtual bool containsImproperRegion() { return _containsImproperRegion; }
   virtual void setContainsImproperRegion(bool b) { _containsImproperRegion = b; }

   bool hasBeenAnalyzedBefore() { return _analyzedBefore; }
   void setAnalyzedStatus(bool analyzedStatus) { _analyzedBefore = analyzedStatus; }

   int32_t getNestingDepth()    { return _nestingDepth; }

   void setNestingDepth(int16_t f)
      {
      if (f > SHRT_MAX-1)
         {
         TR_ASSERT(0, "nesting depth must be less than or equal to SHRT_MAX-1");
         comp()->failCompilation<TR::CompilationException>("nesting depth must be less than or equal to SHRT_MAX-1");
         }
      _nestingDepth = f;
      }

   int32_t getAnyCyclicRegionNestingDepth()    { return _anyCyclicRegionNestingDepth; }

   void setAnyCyclicRegionNestingDepth(int16_t f)
      {
      if (f > SHRT_MAX-1)
         {
         TR_ASSERT(0, "nesting depth must be less than or equal to SHRT_MAX-1");
         comp()->failCompilation<TR::CompilationException>("nesting depth must be less than or equal to SHRT_MAX-1");
         }
      _anyCyclicRegionNestingDepth = f;
      }

   int32_t getMaxNestingDepth()
      {
      return _maxNestingDepth;
      }

   void setMaxNestingDepth(int16_t f)
      {
      if (f > SHRT_MAX-1)
         {
         TR_ASSERT(0, "max nesting depth must be less than or equal to SHRT_MAX-1");
         comp()->failCompilation<TR::CompilationException>("max nesting depth must be less than or equal to SHRT_MAX-1");
         }
      _maxNestingDepth = f;
      }

   virtual void hoistInvariantsOutOfNestedLoops(TR_LocalTransparency *, TR_BitVector **, bool, TR_BlockStructure *, TR_RegionStructure *, int32_t) = 0;
   virtual bool isExpressionTransparentIn(int32_t, TR_LocalTransparency *) = 0;

   // Remove an outgoing edge from this structure to another. The "from" structure
   // is part of this structure; the "to" structure is not.
   // This method should only be called by "removeEdge" methods and by recursion.
   // Return value is:
   //    EDGES_STILL_EXIST          if there are still edges from the "from"
   //                               structure to the "to" structure.
   //    NO_MORE_EDGES_EXIST        if there are now no edges from the "from"
   //                               structure to the "to" structure and the edge
   //                               removed was an explicit edge.
   //
   #define EDGES_STILL_EXIST          0
   #define NO_MORE_EDGES_EXIST        1
   //
   virtual int  removeExternalEdgeTo(TR_Structure *from, int32_t toNumber) = 0;

   public:
   virtual List<TR::Block> *getBlocks(List<TR::Block> *) = 0;
   virtual List<TR::Block> *getBlocks(List<TR::Block> *, vcount_t) = 0;

   int32_t            _nodeIndex;

   protected:
   TR_Structure            *_versionedStructure;

   private:

   TR::Compilation *         _comp;
   TR_Memory *                _trMemory;
   TR_StructureSubGraphNode * _graphNode;
   int32_t                    _weight;
   TR_RegionStructure *       _parent;
   void *                     _analysisInfo;
   bool                       _containsImproperRegion;
   bool                       _analyzedBefore;
   int16_t                    _nestingDepth;
   int16_t                    _maxNestingDepth;
   int16_t                    _anyCyclicRegionNestingDepth;
   };

// **********************************************************************
//
// A node of a region's sub-graph.
// For internal nodes the node references the corresponding child structure.
// For exit nodes only the node number is valid, there is no structure
// referenced from the node.
//

// Pseudo-safe downcast from a CFG node to a TR_StructureSubGraphNode
//
static TR_StructureSubGraphNode *toStructureSubGraphNode(TR::CFGNode *node)
   {
   #if DEBUG
      TR_ASSERT(node->asStructureSubGraphNode() != NULL,"Bad downcast from TR::CFGNode to TR_StructureSubGraphNode");
   #endif
   return (TR_StructureSubGraphNode *)node;
   }

class TR_StructureSubGraphNode : public TR::CFGNode
   {
   public:
   // Create a node for a concrete structure
   //
   TR_StructureSubGraphNode(TR_Structure *s)
      : TR::CFGNode(s->getNumber(), s->cfg()->structureRegion()), _structure(s)  {s->setSubGraphNode(this);}

   TR_StructureSubGraphNode(int32_t n, TR::Region &region)
      : TR::CFGNode(n, region), _structure(0) {}

   static TR_StructureSubGraphNode *create(int32_t num, TR_RegionStructure *region);

   TR_Structure *getStructure()                {return _structure;}
   void          setStructure(TR_Structure *s) {_structure = s; if (s) {setNumber(s->getNumber()); s->setSubGraphNode(this);}}

   virtual TR_StructureSubGraphNode *asStructureSubGraphNode();

   private:
   TR_Structure *_structure;
   };

// **********************************************************************
//
// A Basic Block structure. This represents a leaf node of the control
// tree.
//

class TR_BlockStructure : public TR_Structure
   {
   public:

   TR_BlockStructure(TR::Compilation * comp, int32_t index, TR::Block *b);

   virtual Kind getKind();
   virtual TR_BlockStructure *asBlock() {return this;}

   TR::Block *getBlock()            {return _block;}
   TR::Block *setBlock(TR::Block *b);// {return (_block = b);}

   virtual bool doDataFlowAnalysis(TR_DataFlowAnalysis *, bool checkForChanges);
   virtual void resetAnalysisInfo();
   virtual void resetAnalyzedStatus();
   virtual bool markStructuresWithImproperRegions();
   virtual void collectExitBlocks(List<TR::Block> *exitBlocks, List<TR::CFGEdge> *exitEdges = NULL);
   virtual void checkStructure(TR_BitVector *);
   virtual void renumber(int32_t num);
   virtual bool renumberRecursively(int32_t origNum, int32_t num);

   virtual void resetVisitCounts(vcount_t num);
   virtual bool changeContinueLoopsToNestedLoops(TR_RegionStructure *);

   virtual void removeEdge(TR_Structure *from, TR_Structure *to);

   virtual void addExternalEdge(TR_Structure *from, int32_t toNumber, bool isExceptionEdge);
   virtual void replacePart(TR_Structure *from, TR_Structure *to);
   virtual int  removeExternalEdgeTo(TR_Structure *from, int32_t toNumber);
   virtual void clearAnalysisInfo();
   virtual void collectCFGEdgesTo(int32_t, List<TR::CFGEdge> *);
   virtual TR_Structure *cloneStructure(TR::Block **, TR_StructureSubGraphNode **, List<TR_Structure> *, List<TR_Structure> *);
   virtual void cloneStructureEdges(TR::Block **);
   virtual void hoistInvariantsOutOfNestedLoops(TR_LocalTransparency *, TR_BitVector **, bool, TR_BlockStructure *, TR_RegionStructure *, int32_t);
   virtual bool isExpressionTransparentIn(int32_t, TR_LocalTransparency *);

   virtual List<TR::Block> *getBlocks(List<TR::Block> *);

   bool isLoopInvariantBlock()          { return _block->isLoopInvariantBlock(); }
   void setAsLoopInvariantBlock(bool b) { _block->setAsLoopInvariantBlock(b);    }

   bool isCreatedByVersioning()         { return _block->isCreatedByVersioning(); }
   void setCreatedByVersioning(bool b)  { _block->setCreatedByVersioning(b);     }

   bool isEntryOfShortRunningLoop()     { return _block->isEntryOfShortRunningLoop(); }
   void setIsEntryOfShortRunningLoop()  { _block->setIsEntryOfShortRunningLoop();     }

   bool wasHeaderOfCanonicalizedLoop()          { return _block->wasHeaderOfCanonicalizedLoop();  }
   void setWasHeaderOfCanonicalizedLoop(bool b) { _block->setWasHeaderOfCanonicalizedLoop(b);    }

   virtual TR::Block *getEntryBlock() { return getBlock(); }

   TR_BlockStructure *getDuplicatedBlock()
      {
      if (_versionedStructure)
         return _versionedStructure->asBlock();
      return NULL;
      }
   void setDuplicatedBlock(TR_BlockStructure *r) {_versionedStructure = r;}

   public:
   virtual List<TR::Block> *getBlocks(List<TR::Block> *, vcount_t);

   private:
   TR::Block          *_block;
   };

// **********************************************************************
//
// Information about an induction variable for a natural loop
//
class TR_InductionVariable : public TR_Link<TR_InductionVariable>
   {
   public:

   TR_InductionVariable() {}

   TR_InductionVariable(TR::RegisterMappedSymbol *s, TR::VPConstraint *entry, TR::VPConstraint *exit, TR::VPConstraint *incr, TR_YesNoMaybe isSigned)
      : _local(s), _isSigned(isSigned)
      {
      setEntry(entry);
      setExit(exit);
      setIncr(incr);
      }

   TR::RegisterMappedSymbol *getLocal() {return _local;}
   void                     setLocal(TR::RegisterMappedSymbol *s) { _local = s;}
   TR::VPConstraint *getEntry() {return _entry;}
   TR::VPConstraint *getIncr() {return _incr;}
   TR::VPConstraint *getExit() {return _exit;}
   void setEntry(TR::VPConstraint *entry) { _entry = entry; }
   void setExit(TR::VPConstraint *exit) { _exit = exit; }
   void setIncr(TR::VPConstraint *incr) { _incr = incr; }
   TR_YesNoMaybe isSigned() { return _isSigned; }

   private:

   TR::RegisterMappedSymbol    *_local;
   TR::VPConstraint            *_entry;
   TR::VPConstraint            *_incr;
   TR::VPConstraint            *_exit;
   TR_YesNoMaybe               _isSigned; //FIXME// CRITICAL Value Propgation should set this up.. right now this is not being done
   };

// **********************************************************************
//
// A Region structure. This represents structures that have sub-graphs.
// There is always a single entry node that dominates the rest of the sub-graph.
//
//

class TR_RegionStructure : public TR_Structure
   {
   public:
   typedef TR::vector<TR_StructureSubGraphNode *, TR::Region&> SubNodeList;

   TR_RegionStructure(TR::Compilation * c, int32_t index)
      : TR_Structure(c, index), _invariantSymbols(NULL), _blocksAtSameNestingLevel(NULL), _piv(NULL),
        _basicIVs(c->getFlowGraph()->structureRegion()), _exitEdges(c->getFlowGraph()->structureRegion()),
         _subNodes(c->getFlowGraph()->structureRegion()), _invariantExpressions(NULL)
      {
      }

   virtual Kind getKind();
   virtual TR_RegionStructure *asRegion() {return this;}

   TR_StructureSubGraphNode *getEntry() {return _entryNode;}
   TR_StructureSubGraphNode *setEntry(TR_StructureSubGraphNode *entry)
      {
      _entryNode = entry;
      entry->getStructure()->setParent(this);

      TR::Block * entryBlock = getEntryBlock();

      if (entryBlock != NULL)
        _unrollFactor = getEntryBlock()->getUnrollFactor();
      return entry;
      }

   // add a new exit edge
   // (or modify an existing TR::CFGEdge and add it to the list of exit edges)
   //
   TR::CFGEdge *addExitEdge(TR_StructureSubGraphNode *from, int32_t to, bool isExceptionEdge = false, TR::CFGEdge *origEdge = 0);

   List<TR::CFGEdge>& getExitEdges() {return _exitEdges;}
   TR_StructureSubGraphNode *findNodeInHierarchy(int32_t num);

   virtual bool doDataFlowAnalysis(TR_DataFlowAnalysis *, bool checkForChanges);
   virtual void resetAnalysisInfo();
   virtual void resetAnalyzedStatus();
   virtual bool markStructuresWithImproperRegions();
   virtual void collectExitBlocks(List<TR::Block> *exitBlocks, List<TR::CFGEdge> *exitEdges = NULL);
   virtual void checkStructure(TR_BitVector *);
   virtual void mergeInto(TR::Block *first, TR::Block *second);
   virtual void removeMergedBlock(TR::Block *merged, TR::Block *mergedInto);
   virtual void renumber(int32_t num);
   virtual bool renumberRecursively(int32_t origNum, int32_t num);

   virtual void resetVisitCounts(vcount_t num);
   virtual bool changeContinueLoopsToNestedLoops(TR_RegionStructure *);

   virtual void removeEdge(TR_Structure *from, TR_Structure *to);
   virtual void addEdge(TR::CFGEdge *edge, bool isExceptionEdge);
   virtual void addExternalEdge(TR_Structure *from, int32_t toNumber, bool isExceptionEdge);
   virtual void replacePart(TR_Structure *from, TR_Structure *to);
   virtual int  removeExternalEdgeTo(TR_Structure *from, int32_t toNumber);
   virtual void clearAnalysisInfo();
   virtual TR_Structure *cloneStructure(TR::Block **, TR_StructureSubGraphNode **, List<TR_Structure> *, List<TR_Structure> *);
   virtual void cloneStructureEdges(TR::Block **);
   virtual List<TR::Block> *getBlocks(List<TR::Block> *);
   virtual void collectCFGEdgesTo(int32_t, List<TR::CFGEdge> *);
   void replaceExitPart(int32_t fromNumber, int32_t toNumber);

   static void extractUnconditionalExits(TR::Compilation * const comp, const TR::list<TR::Block*, TR::Region&> &blocks);

   // Collapse this region into its parent
   //
   void collapseIntoParent();

  // Determine if this region contains internal cycles
   //
   bool containsInternalCycles() {return _regionFlags.testAny(_containsInternalCycles);}
   void setContainsInternalCycles(bool b) {_regionFlags.set(_containsInternalCycles, b);}

   // Determine if this is a natural loop region, i.e a region with back-edges
   // to the entry node
   //
   bool isNaturalLoop() {return !containsInternalCycles() && !_entryNode->getPredecessors().empty();}

   TR_RegionStructure *getVersionedLoop()
      {
      if (_versionedStructure)
         return _versionedStructure->asRegion();
      return NULL;
      }
   void setVersionedLoop(TR_RegionStructure *r) {_versionedStructure = r;}

   bool containsOnlyAcyclicRegions();

   // List of induction variables for a natural loop
   //
   TR_InductionVariable *getFirstInductionVariable() {return _inductionVariables.getFirst();}
   void addInductionVariable(TR_InductionVariable *v) {_inductionVariables.add(v);}
   void addAfterInductionVariable(TR_InductionVariable *prev, TR_InductionVariable *v) {_inductionVariables.insertAfter(prev, v);}
   void clearInductionVariables() {_inductionVariables.setFirst(NULL);}
   TR_InductionVariable* findMatchingIV(TR::SymbolReference *symRef);


   TR_PrimaryInductionVariable *getPrimaryInductionVariable() { return _piv; }
   void setPrimaryInductionVariable(TR_PrimaryInductionVariable *piv) { _piv = piv; }

   void addInductionVariable(TR_BasicInductionVariable *biv) { _basicIVs.add(biv); }
   List<TR_BasicInductionVariable> &getBasicInductionVariables() { return _basicIVs; }

   bool hasExceptionOutEdges();

   // Determine if this is an acyclic region
   //
   bool isAcyclic() {return !containsInternalCycles() && _entryNode->getPredecessors().empty();}
   bool isExitEdge(TR::CFGEdge * exitEdge)
      {
      TR_StructureSubGraphNode *toNode = exitEdge->getTo()->asStructureSubGraphNode();
      if (toNode->getStructure())
         return false;

      return true;

      /*
      ListIterator<TR::CFGEdge> ei(&getExitEdges());
      for (TR::CFGEdge * edge = ei.getCurrent(); edge; edge = ei.getNext())
         if (edge == exitEdge)
            return true;
      return false;
      */
      }

   void cleanupAfterEdgeRemoval(TR::CFGNode *);
   void cleanupAfterNodeRemoval();

   bool isCanonicalizedLoop() {return _regionFlags.testAny(_isCanonicalizedLoop);}
   void setAsCanonicalizedLoop(bool b) {_regionFlags.set(_isCanonicalizedLoop, b);}

   bool isInvertible() {return _regionFlags.testAny(_isInvertible);}
   void setAsInvertible(bool b) {_regionFlags.set(_isInvertible, b);}

   void computeInvariantSymbols();
   void updateInvariantSymbols(TR::Node *, vcount_t);
   void computeInvariantExpressions();
   void updateInvariantExpressions(TR::Node *, vcount_t);

   void resetInvariance();

   bool isExprInvariant(TR::Node *, bool usePrecomputed = true);
   bool isExprTreeInvariant(TR::Node *);
   bool isSubtreeInvariant(TR::Node *, vcount_t);

   TR_BitVector *getInvariantExpressions() { return _invariantExpressions; }
   void setExprInvariant(TR::Node *expr) { TR_ASSERT(_invariantExpressions != NULL, "assertion failure"); _invariantExpressions->set(expr->getGlobalIndex());}
   void resetExprInvariant(TR::Node *expr) {TR_ASSERT(_invariantExpressions != NULL, "assertion failure"); _invariantExpressions->reset(expr->getGlobalIndex());}

   bool isSymbolRefInvariant(TR::SymbolReference *);

   void addGlobalRegisterCandidateToExits(TR_RegisterCandidate *);

   void addSubNode(TR_StructureSubGraphNode *subNode);
   void removeSubNode(TR_StructureSubGraphNode *subNode);
   uint32_t numSubNodes() {return _subNodes.size();}

   // Find the subnode numbered 'number' in the current region
   // Returns null, if none of the subnodes match
   //
   TR_StructureSubGraphNode *findSubNodeInRegion(int32_t number);

   virtual int32_t getMaxNestingDepth(int32_t *, int32_t *);

   virtual void hoistInvariantsOutOfNestedLoops(TR_LocalTransparency *, TR_BitVector **, bool, TR_BlockStructure *, TR_RegionStructure *, int32_t);
   virtual bool isExpressionTransparentIn(int32_t, TR_LocalTransparency *);

   TR_BitVector *getBlocksAtSameNestingLevel() {return _blocksAtSameNestingLevel;}
   void setBlocksAtSameNestingLevel(TR_BitVector *blocksInLoop) {_blocksAtSameNestingLevel = blocksInLoop;}


   virtual TR::Block *getEntryBlock()
     {
     TR_RegionStructure *region = _entryNode->getStructure()->asRegion();
      if (!region)
         return _entryNode->getStructure()->asBlock()->getBlock();
     return region->getEntryBlock();
     }

   float getFrequencyEntryFactor() { return _frequencyEntryFactor; }
   void setFrequencyEntryFactor(float factor) { _frequencyEntryFactor = factor; }

   uint16_t   getUnrollFactor()           {return _unrollFactor;}
   void       setUnrollFactor(int factor) {_unrollFactor = factor;}
   bool       canBeUnrolled()               {return _unrollFactor != 1;}
   public:
   virtual List<TR::Block> *getBlocks(List<TR::Block> *, vcount_t);

   enum  // flags
      {
      _containsInternalCycles       = 0x01,
      _isCanonicalizedLoop          = 0x02,
      _isInvertible                 = 0x04
      };

   SubNodeList::iterator begin() { return _subNodes.begin(); }
   SubNodeList::iterator end()   { return _subNodes.end();   }

   // The Cursor provides an immutable view of the elements of the region unlike the normal iterators
   // above, note that there is a non-trivial cost to this Cursor because it must copy the structure's
   // sublist into itself for the purposes of isolation
   class Cursor
      {
      public:

      Cursor(TR_RegionStructure & region): _nodes(region._subNodes), _iter(_nodes.begin()) { }

      TR_StructureSubGraphNode * getCurrent() { return (_iter != _nodes.end()) ? *_iter : NULL; }
      TR_StructureSubGraphNode * getNext()  { _iter++; return getCurrent(); }
      TR_StructureSubGraphNode * getFirst() { return getCurrent(); }
      void reset() { _iter = _nodes.begin(); }

      private:
      SubNodeList _nodes;
      SubNodeList::iterator _iter;
      };

   friend class Cursor;
   private:

   friend class TR_RegionAnalysis;
   friend class TR_LoopUnroller;

   void addExitEdge(TR::CFGEdge *edge)
	 {
	 TR_ASSERT(toStructureSubGraphNode(edge->getTo())->getStructure() == NULL,
		"Region exit edge must not have structure");
	 _exitEdges.add(edge);
	 }

   void checkForInternalCycles();
   void removeEdge(TR::CFGEdge *edge, bool isExitEdge);
   void removeEdgeWithoutCleanup(TR::CFGEdge *edge, bool isExitEdge);
   void removeSubNodeWithoutCleanup(TR_StructureSubGraphNode *subNode);

   // Find a subnode with the given structure. As a precondition, such a node
   // must exist. This method returns non-null.
   TR_StructureSubGraphNode *subNodeFromStructure(TR_Structure*);


   // Implementation of extractUnconditionalExits(). Class definition is private
   // to Structure.cpp
   class ExitExtraction;

   TR_StructureSubGraphNode         *_entryNode;
   TR_BitVector                     *_invariantSymbols;
   TR_BitVector                     *_blocksAtSameNestingLevel;

   List<TR::CFGEdge>                 _exitEdges;
   SubNodeList                       _subNodes;
   TR_LinkHead<TR_InductionVariable> _inductionVariables;


   TR_PrimaryInductionVariable      *_piv;
   List<TR_BasicInductionVariable> _basicIVs;
   flags8_t   _regionFlags;
   float      _frequencyEntryFactor;

   TR_BitVector                     *_invariantExpressions;
   uint16_t   _unrollFactor;
   };


#endif
