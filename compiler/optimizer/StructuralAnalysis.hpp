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

#ifndef STRUCTURALANALYSIS_INCL
#define STRUCTURALANALYSIS_INCL

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "infra/Cfg.hpp"
#include "infra/deque.hpp"

class TR_Dominators;
class TR_RegionStructure;
class TR_Structure;
class TR_StructureSubGraphNode;
namespace TR { class ResolvedMethodSymbol; }
template <class T> class TR_Stack;

/**
 * Perform Region Analysis to break the CFG down into sub-regions.
 */
class TR_RegionAnalysis
   {
   public:
   TR_ALLOC(TR_Memory::RegionAnalysis)

   TR::Compilation * comp() { return _compilation; }

   TR_Memory *   trMemory()     { return comp()->trMemory(); }
   TR_HeapMemory trHeapMemory() { return trMemory(); }
   bool trace() { return _trace; }

   static TR_Structure *getRegions(TR::Compilation *);
   static TR_Structure *getRegions(TR::Compilation *, TR::ResolvedMethodSymbol *);
   static TR_Structure *getRegions(TR::Compilation *, TR::CFG *, bool acceptUnreachableBlocks = true);

   friend class TR_Debug;

   private:

   class StructInfo;
   typedef StructInfo** InfoTable;
   typedef TR_BitVector StructureBitVector;
   typedef TR_BitVector WorkBitVector;
   typedef TR::deque<TR_StructureSubGraphNode*, TR::Region&> SubGraphNodes;

   void simpleIterator(TR_Stack<int32_t>& workStack,
                       StructureBitVector& vector,
                       WorkBitVector &regionNodes,
                       WorkBitVector &nodesInPath,
                       bool &cyclesFound,
                       TR::Block *hdrBlock,
                       bool doThisCheck = false);

   class StructInfo
      {
      public:
      StructInfo(TR::Region &region)
         : _pred(region), _succ(region), _exceptionPred(region), _exceptionSucc(region)
         {
         }
      StructureBitVector     _pred;
      StructureBitVector     _succ;
      StructureBitVector     _exceptionPred;
      StructureBitVector     _exceptionSucc;
      TR_Structure       *_structure;
      TR::Block           *_originalBlock;
      int32_t             _nodeIndex;

      void initialize(TR::Compilation *, int32_t index, TR::Block *block);
      int32_t getNumber() { return _originalBlock ? _originalBlock->getNumber() : -1; }
      };

   TR_RegionAnalysis(TR::Compilation *comp, TR_Dominators &dominators, TR::CFG * cfg, TR::Region &workingMemoryRegion) :
      _workingMemoryRegion(workingMemoryRegion),
      _structureMemoryRegion(comp->getFlowGraph()->structureMemoryRegion()),
      _compilation(comp),
      _infoTable(NULL),
      _dominators(dominators),
      _cfg(cfg)
      {
      }
   TR::Region &_workingMemoryRegion;
   TR::Region &_structureMemoryRegion;
   TR::Compilation *_compilation;

   /** The StructInfoTable is 1-based */
   StructInfo &getInfo(int32_t index) { return *(_infoTable[index+1]); }
   InfoTable       _infoTable;

   /** Total number of nodes in the flow graph */
   int32_t     _totalNumberOfNodes;

   /** Number of active nodes left in the flow graph */
   int32_t     _numberOfActiveNodes;

   /** Dominator information */
   TR_Dominators &_dominators;

   TR::CFG *      _cfg;

   bool _trace;
   bool _useNew;
   void createLeafStructures(TR::CFG *cfg, TR::Region &region);

   static TR_Structure *getRegionsImpl(TR::Compilation *comp, TR_Dominators& dominators, TR::CFG* cfg);

   TR_Structure       *findRegions(TR::Region &region);
   TR_RegionStructure *findNaturalLoop(StructInfo &node,
                                       WorkBitVector &regionNodes,
                                       WorkBitVector &nodesInPath);
   void                addNaturalLoopNodes(StructInfo &node,
                                           WorkBitVector &regionNodes,
                                           WorkBitVector &nodesInPath,
                                           bool &cyclesFound,
                                           TR::Block *hdrBlock);

   void                addNaturalLoopNodesIterativeVersion(StructInfo &node,
                                                           WorkBitVector &regionNodes,
                                                           WorkBitVector &nodesInPath,
                                                           bool &cyclesFound,
                                                           TR::Block *hdrBlock);

   TR_RegionStructure *findRegion(StructInfo &node,
                                  WorkBitVector &regionNodes,
                                  WorkBitVector &nodesInPath);
   void                addRegionNodes(StructInfo &node,
                                      WorkBitVector &regionNodes,
                                      WorkBitVector &nodesInPath,
                                      bool &cyclesFound,
                                      TR::Block *hdrBlock);

   void                addRegionNodesIterativeVersion(StructInfo &node,
                                                      WorkBitVector &regionNodes,
                                                      WorkBitVector &nodesInPath,
                                                      bool &cyclesFound,
                                                      TR::Block *hdrBlock);

   void                buildRegionSubGraph(TR_RegionStructure *region,
                                           StructInfo &entryNode,
                                           WorkBitVector &regionNodes,
                                           SubGraphNodes &cfgNodes, TR::Region &memRegion);

   };

#endif
