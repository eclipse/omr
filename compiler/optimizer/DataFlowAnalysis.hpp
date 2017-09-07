/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
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

#ifndef DFA_INCL
#define DFA_INCL

#include <stddef.h>                              // for NULL, size_t
#include <stdint.h>                              // for int32_t, int8_t, etc
#include "compile/Compilation.hpp"               // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"                      // for TR_Memory, etc
#include "il/Node.hpp"                           // for Node, vcount_t
#include "infra/Array.hpp"                       // for TR_Array
#include "infra/Assert.hpp"                      // for TR_ASSERT
#include "infra/BitVector.hpp"                   // for TR_BitVector
#include "infra/Flags.hpp"                       // for flags16_t, flags8_t
#include "infra/HashTab.hpp"                     // for TR_HashTab
#include "infra/Link.hpp"
#include "infra/List.hpp"                        // for List, etc
#include "optimizer/Structure.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "optimizer/UseDefInfo.hpp"     // for TR_UseDefInfo, etc

class Candidate;
class FlushCandidate;
class TR_CFGEdgeAllocationPair;
class TR_Debug;
class TR_Delayedness;
class TR_Earliestness;
class TR_EscapeAnalysis;
class TR_ExceptionCheckMotion;
class TR_FlowSensitiveEscapeAnalysis;
class TR_FrontEnd;
class TR_GlobalAnticipatability;
class TR_Isolatedness;
class TR_Latestness;
class TR_LiveOnAllPaths;
class TR_Liveness;
class TR_RedundantExpressionAdjustment;
class TR_RegisterAnticipatability;
class TR_RegisterAvailability;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class CodeGenerator; }
namespace TR { class Optimizer; }
namespace TR { class Register; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

#define INVALID_LIVENESS_INDEX 65535

// This file contains the declarations for the base class
// DataFlowAnalysis and some of the classes that extend it.
// The most important ones are BitVectorAnalysis,
// BackwardBitVectorAnalysis and their corresponding Union
// and Intersection analysis versions. These classes can be
// used by users to implement new bit vector analyses with
// minimum effort (basically the rules for a basic block).
//
//
// Many of these data flow classes are template-ized to abstract
// out the container that they operate on.  An example of a container
// is TR_BitVector
template<class Container>class TR_BasicDFSetAnalysis { };
template<class Container>class TR_BasicDFSetAnalysis<Container *>;
template<class Container>class TR_ForwardDFSetAnalysis { };
template<class Container>class TR_ForwardDFSetAnalysis<Container *>;
template<class Container>class TR_IntersectionDFSetAnalysis { };
template<class Container>class TR_IntersectionDFSetAnalysis<Container *>;
template<class Container>class TR_UnionDFSetAnalysis { };
template<class Container>class TR_UnionDFSetAnalysis<Container *>;
template<class Container>class TR_BackwardDFSetAnalysis { };
template<class Container>class TR_BackwardDFSetAnalysis<Container *>;
template<class Container>class TR_BackwardIntersectionDFSetAnalysis { };
template<class Container>class TR_BackwardIntersectionDFSetAnalysis<Container *>;
template<class Container>class TR_BackwardUnionDFSetAnalysis { };
template<class Container>class TR_BackwardUnionDFSetAnalysis<Container *>;

// The root of the DataFlowAnalysis hierarchy. All
// dataflow analysis classes extend this class. This class
// also contains some general helper methods used by
// the analyses to perform TR::Node and TR::TreeTop related computations.
//
//
class TR_DataFlowAnalysis
   {
   public:

   static void *operator new(size_t size, TR::Allocator a)
      { return a.allocate(size); }
   static void  operator delete(void *ptr, size_t size)
      { ((TR_DataFlowAnalysis*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() better return the same allocator as used for new */

   /* Virtual destructor is necessary for the above delete operator to work
    * See "Modern C++ Design" section 4.7
    */
   virtual ~TR_DataFlowAnalysis() {}


   TR_DataFlowAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : _comp(comp),
        _cfg(cfg),
        _optimizer(optimizer),
        _trace(trace),
        _analysisQueue(comp->trMemory()),
        _changedSetsQueue(comp->trMemory())
      {
      _analysisInterrupted = false;
      }

   TR::Optimizer *           optimizer()                     { return _optimizer; }
   TR::Compilation *        comp()                          { return _comp; }
   TR::CodeGenerator *      cg()                            { return _comp->cg(); }
   TR_FrontEnd *             fe()                            { return _comp->fe(); }
   TR_Debug *            getDebug()                      { return _comp->getDebug(); }

   TR_Memory *               trMemory()                      { return _comp->trMemory(); }
   TR_StackMemory            trStackMemory()                 { return _comp->trStackMemory(); }
   TR_HeapMemory             trHeapMemory()                  { return _comp->trHeapMemory(); }
   TR_PersistentMemory *     trPersistentMemory()            { return _comp->trPersistentMemory(); }

   TR::Allocator             allocator()                     { return _comp->allocator(); }

   bool                      trace()                         { return _trace; }
   void                      setTrace(bool t = true)         { _trace = t; }

   virtual int32_t perform() { TR_ASSERT(0, "perform not implemented"); return 0; }

   enum Kind
      {
      #include "optimizer/DataFlowAnalysis.enum"
      };

   #ifdef DEBUG
      char *getAnalysisName();
   #else
      char *getAnalysisName() { return 0; }
   #endif

   virtual Kind getKind() = 0;

   #if 0
   virtual TR_ReachingDefinitions           *asReachingDefinitions();
   #endif
   virtual TR_ExceptionCheckMotion           *asExceptionCheckMotion();
   virtual TR_RedundantExpressionAdjustment *asRedundantExpressionAdjustment();

   //virtual TR_BitVectorAnalysis             *asBitVectorAnalysis();
   //virtual TR_ForwardBitVectorAnalysis      *asForwardBitVectorAnalysis();
   //virtual TR_UnionBitVectorAnalysis        *asUnionBitVectorAnalysis();
   //virtual TR_IntersectionBitVectorAnalysis *asIntersectionBitVectorAnalysis();
   //virtual TR_BackwardBitVectorAnalysis *asBackwardBitVectorAnalysis();
   //virtual TR_BackwardIntersectionBitVectorAnalysis *asBackwardIntersectionBitVectorAnalysis();
   //virtual TR_BackwardUnionBitVectorAnalysis *asBackwardUnionBitVectorAnalysis();
   virtual TR_GlobalAnticipatability *asGlobalAnticipatability();
   virtual TR_Earliestness *asEarliestness();
   virtual TR_Delayedness *asDelayedness();
   virtual TR_Latestness *asLatestness();
   virtual TR_Isolatedness *asIsolatedness();
   virtual TR_Liveness *asLiveness();
   virtual TR_LiveOnAllPaths *asLiveOnAllPaths();
   virtual TR_FlowSensitiveEscapeAnalysis *asFlowSensitiveEscapeAnalysis();
   virtual TR_RegisterAnticipatability *asRegisterAnticipatability();
   virtual TR_RegisterAvailability *asRegisterAvailability();

   void addToAnalysisQueue(TR_StructureSubGraphNode *, uint8_t);
   void removeHeadFromAnalysisQueue();

   virtual bool analyzeBlockStructure(TR_BlockStructure *, bool) = 0;
   virtual bool analyzeRegionStructure(TR_RegionStructure *, bool) = 0;

   bool isSameAsOrAliasedWith(TR::SymbolReference *, TR::SymbolReference *);
   bool collectCallSymbolReferencesInTreeInto(TR::Node *, List<TR::SymbolReference> *);
   bool collectAllSymbolReferencesInTreeInto(TR::Node *, List<TR::SymbolReference> *);
   bool treeHasChecks(TR::TreeTop *tt, TR::Node *node = NULL)
      {
      if (node != NULL && tt->getNode() != node)
         return node->exceptionsRaised() != 0;
      return (tt->getNode()->exceptionsRaised() != 0 ||
              (comp()->isPotentialOSRPointWithSupport(tt)));
      }
   bool areSyntacticallyEquivalent(TR::Node *, TR::Node *);
   template<class Container>static void copyFromInto(Container *from, Container *to)
      {
      if (from)
         *to = *from;
      else
         to->empty();
      }

   TR_ScratchList<TR_StructureSubGraphNode> _analysisQueue;
   TR_ScratchList<uint8_t> _changedSetsQueue;
   bool _analysisInterrupted;

   protected:

   TR::Compilation *         _comp;
   TR::CFG *                 _cfg;
   TR::Optimizer *         _optimizer;
   bool                       _trace;
   };

// The root of the bit vector analysis hierarchy. Extended
// by Forward and Backward versions. Contains methods that are shared for both
// directions.
//
//
template<class Container>class TR_BasicDFSetAnalysis<Container *> :
   public TR_DataFlowAnalysis
   {
   public:

   TR_BasicDFSetAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_DataFlowAnalysis(comp, cfg, optimizer, trace),
        _traceBVA(comp->getOption(TR_TraceBVA))
      {
      initialize();
      }

   void initialize()
      {
      _numberOfBits         = Container::nullContainerCharacteristic;
      _regularGenSetInfo           = 0;
      _regularKillSetInfo          = 0;
      _exceptionGenSetInfo  = 0;
      _exceptionKillSetInfo = 0;
      _blockAnalysisInfo    = 0;
      _hasImproperRegion    = false;
      _nodesInCycle         = NULL;
      }

   bool traceBVA() { return _traceBVA;}

   virtual Kind getKind();

   //virtual TR_BitVectorAnalysis *asBitVectorAnalysis();

   virtual bool supportsGenAndKillSets();
   virtual void initializeGenAndKillSetInfo();
   virtual typename Container::containerCharacteristic getNumberOfBits() = 0;
   virtual void initializeBasicDFSetAnalysis();
   virtual Container * initializeInfo(Container *) = 0;
   virtual Container * inverseInitializeInfo(Container *) = 0;
   virtual void compose(Container *, Container *) {}
   virtual void inverseCompose(Container *, Container *) {}

   void initializeBlockInfo(bool allocateLater = false);

   // Perform the analysis including initialization
   // Return true if the analysis was actually done
   bool performAnalysis(TR_Structure *rooStructure, bool checkForChanges);

   // Analysis specific initializations
   // Returns true if the analysis is to continue
   virtual bool postInitializationProcessing() {return true;}

   bool doAnalysis(TR_Structure *rootStructure, bool checkForChanges)
      {
      return rootStructure->doDataFlowAnalysis(this, checkForChanges);
      }

   virtual void initializeDFSetAnalysis() = 0;

   class TR_ContainerNodeNumberPair : public TR_Link<TR_ContainerNodeNumberPair>
      {
      public:
      TR_ALLOC(TR_Memory::DataFlowAnalysis)

      TR_ContainerNodeNumberPair() : _container(0), _nodeNumber(-1) {}
      TR_ContainerNodeNumberPair(Container *b, int32_t n) : _container(b), _nodeNumber(n) {}

      Container *_container;
      int32_t _nodeNumber;
      };

   struct ExtraAnalysisInfo
      {
      TR_ALLOC(TR_Memory::DataFlowAnalysis)

      TR_LinkHead<TR_ContainerNodeNumberPair>  *_regularGenSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_regularKillSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_exceptionGenSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_exceptionKillSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_currentRegularGenSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_currentRegularKillSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_currentExceptionGenSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_currentExceptionKillSetInfo;


      Container *getContainer(TR_LinkHead<TR_ContainerNodeNumberPair>  *list, int32_t n);
      TR_ContainerNodeNumberPair *getContainerNodeNumberPair(TR_LinkHead<TR_ContainerNodeNumberPair>  *list, int32_t n);
      Container *_inSetInfo;
      //Container **_outSetInfo;
      TR_LinkHead<TR_ContainerNodeNumberPair>  *_outSetInfo;
      bool _containsExceptionTreeTop;
      };

   ExtraAnalysisInfo *getAnalysisInfo(TR_Structure *s);
   ExtraAnalysisInfo *createAnalysisInfo();
   void               initializeAnalysisInfo(ExtraAnalysisInfo *info, TR_Structure *s);
   void               initializeAnalysisInfo(ExtraAnalysisInfo *info, TR_RegionStructure *region);
   void               initializeAnalysisInfo(ExtraAnalysisInfo *info, TR::Block *block);
   void               clearAnalysisInfo(ExtraAnalysisInfo *info);

   void initializeGenAndKillSetInfoForStructures();
   void initializeGenAndKillSetInfoForStructure(TR_Structure *);
   void initializeGenAndKillSetInfoPropertyForStructure(TR_Structure *, bool);

   virtual void initializeGenAndKillSetInfoForRegion(TR_RegionStructure *) = 0;
   virtual void initializeGenAndKillSetInfoForBlock(TR_BlockStructure *) = 0;
   virtual bool canGenAndKillForStructure(TR_Structure *) = 0;
   virtual bool supportsGenAndKillSetsForStructures() {return true;}

   virtual void allocateContainer(Container **result, bool isSparse = true, bool lock = false);
   virtual void allocateBlockInfoContainer(Container **result, bool isSparse = true, bool lock = false);
   virtual void allocateBlockInfoContainer(Container **result, Container *other);
   virtual void allocateTempContainer(Container **result, Container *other);

   Container *_regularInfo;
   Container *_exceptionInfo;
   Container **_blockAnalysisInfo;
   Container **_regularGenSetInfo;
   Container **_regularKillSetInfo;
   Container **_exceptionGenSetInfo;
   Container **_exceptionKillSetInfo;
   Container *_temp, *_temp2;
   TR_BitVector *_nodesInCycle;
   bool _containsExceptionTreeTop;
   bool _firstIteration;
   bool _traceBVA;
   typename Container::containerCharacteristic _numberOfBits;
   int32_t _numTreeTops;
   int32_t _numberOfNodes;
   int32_t _maxReferenceNumber;
   TR::Node **_supportedNodesAsArray;
   bool _hasImproperRegion;
   };


// The root of the Forward bit vector analysis hierarchy. Extended
// by Union and Intersection versions. Contains methods that actually
// define the bit vector analysis rules for each kind of structure.
//
//
template<class Container>class TR_ForwardDFSetAnalysis<Container *> :
   public TR_BasicDFSetAnalysis<Container *>
   {
   public:

   TR_ForwardDFSetAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_BasicDFSetAnalysis<Container *>(comp, cfg, optimizer, trace) { initialize(); }

   void initialize()
      {
      _currentInSetInfo     = 0;
      _originalInSetInfo    = 0;
      _currentRegularGenSetInfo    = 0;
      _currentRegularKillSetInfo   = 0;
      }

   virtual TR_DataFlowAnalysis::Kind getKind();

   virtual bool analyzeBlockStructure(TR_BlockStructure *, bool);
   virtual void analyzeBlockZeroStructure(TR_BlockStructure *);
   virtual bool analyzeRegionStructure(TR_RegionStructure *, bool);

   virtual void compose(Container *, Container *);
   virtual void inverseCompose(Container *, Container *);

   virtual void initializeInSetInfo() = 0;
   virtual void initializeCurrentGenKillSetInfo() = 0;
   virtual Container * initializeInfo(Container *) = 0;
   virtual Container * inverseInitializeInfo(Container *) = 0;
	virtual void initializeDFSetAnalysis();
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, Container *);

   bool analyzeNodeIfPredecessorsAnalyzed(TR_RegionStructure *, TR_BitVector &);

   virtual void initializeGenAndKillSetInfo(TR_RegionStructure *, TR_BitVector &);
   virtual void initializeGenAndKillSetInfoForRegion(TR_RegionStructure *);
   virtual void initializeGenAndKillSetInfoForBlock(TR_BlockStructure *);
   virtual bool canGenAndKillForStructure(TR_Structure *);

   Container *_currentInSetInfo;
   Container *_originalInSetInfo;
   Container *_currentRegularGenSetInfo;
   Container *_currentRegularKillSetInfo;
   };

// Forward intersection bit vector analysis
//
template<class Container>class TR_IntersectionDFSetAnalysis<Container *> :
   public TR_ForwardDFSetAnalysis<Container *>
   {
   public:

   TR_IntersectionDFSetAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace) :
      TR_ForwardDFSetAnalysis<Container *>(comp, cfg, optimizer, trace) {}

   virtual TR_DataFlowAnalysis::Kind getKind();

   virtual void compose(Container *, Container *);
   virtual void inverseCompose(Container *, Container *);
   virtual void initializeInSetInfo();
   virtual void initializeCurrentGenKillSetInfo();
   virtual Container * initializeInfo(Container *);
   virtual Container * inverseInitializeInfo(Container *);
   };

class TR_IntersectionBitVectorAnalysis : public TR_IntersectionDFSetAnalysis<TR_BitVector *>
   {
   public:
   typedef TR_BitVector ContainerType;
   TR_IntersectionBitVectorAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_IntersectionDFSetAnalysis<TR_BitVector *>(comp, cfg, optimizer, trace) {}
   virtual void compose(TR_BitVector *a, TR_BitVector *b)
      {
      TR_IntersectionDFSetAnalysis<TR_BitVector *>::compose(a,b);
      }
   };

// Forward union bit vector analysis
//
template<class Container>class TR_UnionDFSetAnalysis<Container *> : public TR_ForwardDFSetAnalysis<Container *>
   {
   public:

   TR_UnionDFSetAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_ForwardDFSetAnalysis<Container *>(comp, cfg, optimizer, trace) {}

   virtual TR_DataFlowAnalysis::Kind getKind();

   virtual void compose(Container *, Container *);
   virtual void inverseCompose(Container *, Container *);
   virtual void initializeInSetInfo();
   virtual void initializeCurrentGenKillSetInfo();
   virtual Container * initializeInfo(Container *);
   virtual Container * inverseInitializeInfo(Container *);
   };

class TR_UnionBitVectorAnalysis : public TR_UnionDFSetAnalysis<TR_BitVector *>
   {
   public:
   typedef TR_BitVector ContainerType;
   TR_UnionBitVectorAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace) :
   	TR_UnionDFSetAnalysis<TR_BitVector *>(comp, cfg, optimizer, trace) {}
   };

class TR_UnionSingleBitContainerAnalysis : public TR_UnionDFSetAnalysis<TR_SingleBitContainer *>
  {
  public:
  typedef TR_SingleBitContainer ContainerType;
  TR_UnionSingleBitContainerAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace) :
      TR_UnionDFSetAnalysis<TR_SingleBitContainer *>(comp, cfg, optimizer, trace) {}
  };

class TR_ReachingDefinitions : public TR_UnionBitVectorAnalysis
   {
   public:

   TR_ReachingDefinitions(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, TR_UseDefInfo *, TR_UseDefInfo::AuxiliaryData &aux, bool trace);

   bool traceRD() { return _traceRD; }

   virtual int32_t perform();

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   virtual void analyzeBlockZeroStructure(TR_BlockStructure *);
   virtual bool supportsGenAndKillSets();
   virtual void initializeGenAndKillSetInfo();

   private:

   void initializeGenAndKillSetInfoForNode(TR::Node *node, TR_UseDefInfo::BitVector &defsKilled, bool seenException, int32_t blockNum, TR::Node *parent);

   TR_UseDefInfo *_useDefInfo;
   TR_UseDefInfo::AuxiliaryData &_aux;
   bool           _traceRD;
   };

class TR_ReachingBlocks : public TR_UnionBitVectorAnalysis
   {
   public:

   TR_ReachingBlocks(TR::Compilation *comp, TR::Optimizer *optimizer, bool trace = false);

   virtual int32_t perform();
   virtual Kind getKind();
   virtual int32_t getNumberOfBits();
   virtual void analyzeBlockZeroStructure(TR_BlockStructure *);
   virtual bool supportsGenAndKillSets();
   virtual void initializeGenAndKillSetInfo();

   private:

   int32_t _numberOfBlocks;
   };

// The root of the Backward bit vector analysis hierarchy. Extended
// by Union and Intersection versions. Contains methods that actually
// define the bit vector analysis rules for each kind of structure.
//
//
template<class Container>class TR_BackwardDFSetAnalysis<Container *> :
   public TR_BasicDFSetAnalysis<Container *>
   {
   public:

   TR_BackwardDFSetAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_BasicDFSetAnalysis<Container *>(comp, cfg, optimizer, trace),
        _traceBBVA(comp->getOption(TR_TraceBBVA))
      {
      _currentOutSetInfo    = 0;
      _originalOutSetInfo   = 0;
      }

   virtual TR_DataFlowAnalysis::Kind getKind();

   bool traceBBVA() { return _traceBBVA; }

   virtual bool analyzeBlockStructure(TR_BlockStructure *, bool);
   virtual bool analyzeRegionStructure(TR_RegionStructure *, bool);

   virtual void compose(Container *, Container *);
   virtual void inverseCompose(Container *, Container *) {}

   virtual void initializeOutSetInfo() = 0;
   virtual Container * initializeInfo(Container *) = 0;
   virtual Container * inverseInitializeInfo(Container *) = 0;
   virtual void initializeCurrentGenKillSetInfo() = 0;
	virtual void initializeDFSetAnalysis();
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, Container *);

   bool analyzeNodeIfSuccessorsAnalyzed(TR_RegionStructure *, TR_BitVector &, TR_BitVector &);

   virtual void initializeGenAndKillSetInfo(TR_RegionStructure *, TR_BitVector &, TR_BitVector &, bool);
   virtual void initializeGenAndKillSetInfoForRegion(TR_RegionStructure *);
   virtual void initializeGenAndKillSetInfoForBlock(TR_BlockStructure *);
   virtual bool canGenAndKillForStructure(TR_Structure *);

   Container **_currentOutSetInfo;
   Container **_originalOutSetInfo;
   bool _areInsideRegion;
   bool _traceBBVA;
   };

// Backward intersection bit vector analysis
//
template<class Container>class TR_BackwardIntersectionDFSetAnalysis<Container *> :
   public TR_BackwardDFSetAnalysis<Container *>
   {
   public:

   TR_BackwardIntersectionDFSetAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_BackwardDFSetAnalysis<Container *>(comp, cfg, optimizer, trace)
      {}

   virtual TR_DataFlowAnalysis::Kind getKind();

   virtual void compose(Container *, Container *);
   virtual void inverseCompose(Container *, Container *);
   virtual void initializeOutSetInfo();
   virtual Container * initializeInfo(Container *);
   virtual Container * inverseInitializeInfo(Container *);
   virtual void initializeCurrentGenKillSetInfo();
   };

class TR_BackwardIntersectionBitVectorAnalysis :
   public TR_BackwardIntersectionDFSetAnalysis<TR_BitVector *>
   {
   public:
   typedef TR_BitVector ContainerType;
   TR_BackwardIntersectionBitVectorAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_BackwardIntersectionDFSetAnalysis<TR_BitVector *>(comp, cfg, optimizer, trace) { }
   };

// Backward union bit vector analysis
//
template<class Container>class TR_BackwardUnionDFSetAnalysis<Container *> :
   public TR_BackwardDFSetAnalysis<Container *>
   {
   public:
   TR_BackwardUnionDFSetAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_BackwardDFSetAnalysis<Container *>(comp, cfg, optimizer, trace)
      {}

   virtual TR_DataFlowAnalysis::Kind getKind();

   virtual void compose(Container *, Container *);
   virtual void inverseCompose(Container *, Container *);
   virtual void initializeOutSetInfo();
   virtual Container * initializeInfo(Container *);
   virtual Container * inverseInitializeInfo(Container *);
   virtual void initializeCurrentGenKillSetInfo();
   };

class TR_BackwardUnionBitVectorAnalysis :
   public TR_BackwardUnionDFSetAnalysis<TR_BitVector *>
   {
   public:
   TR_BackwardUnionBitVectorAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_BackwardUnionDFSetAnalysis<TR_BitVector *>(comp, cfg, optimizer, trace) { }
   };

class TR_BackwardUnionSingleBitContainerAnalysis :
   public TR_BackwardUnionDFSetAnalysis<TR_SingleBitContainer *>
   {
   public:
   TR_BackwardUnionSingleBitContainerAnalysis(TR::Compilation *comp, TR::CFG *cfg, TR::Optimizer *optimizer, bool trace)
      : TR_BackwardUnionDFSetAnalysis<TR_SingleBitContainer *>(comp, cfg, optimizer, trace) { }
   };

// First dataflow analysis in Partial Redundancy Elimination
//
class TR_GlobalAnticipatability
   : public TR_BackwardIntersectionBitVectorAnalysis
   {
   public:
   TR_GlobalAnticipatability(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *, bool trace);

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *);
   virtual TR_GlobalAnticipatability *asGlobalAnticipatability();
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   ContainerType * getCheckExpressionsInBlock(int32_t);
   bool isExceptionalInBlock(TR::Node *, int32_t, ContainerType *, vcount_t);
   void killBasedOnSuccTransparency(TR::Block *);
   virtual bool postInitializationProcessing();

   TR_LocalAnalysisInfo _localAnalysisInfo;
   TR_LocalTransparency _localTransparency; // _localTransparency should be before _localAnticipatability
   TR_LocalAnticipatability _localAnticipatability;

   private:
   // Not exported to callers
   ContainerType **_outSetInfo;
   ContainerType *_scratch, *_scratch2, *_scratch3;
   ContainerType **_checkExpressionsInBlock;
   };

// Second dataflow analysis in Partial Redundancy Elimination
//
class TR_Earliestness
   : public TR_UnionBitVectorAnalysis
   {
   public:
   TR_Earliestness(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *, bool trace);

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *);
   virtual TR_Earliestness *asEarliestness();
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   TR_GlobalAnticipatability *_globalAnticipatability;
   ContainerType **_inSetInfo;
   ContainerType *_temp;
   };

// Third dataflow analysis in Partial Redundancy Elimination
//
class TR_Delayedness
   : public TR_IntersectionBitVectorAnalysis
   {
   public:
   TR_Delayedness(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *, bool trace);

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, ContainerType *);
   virtual TR_Delayedness *asDelayedness();
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   TR_Earliestness *_earliestness;
   ContainerType **_inSetInfo;
   ContainerType *_temp;
   };

// Fourth dataflow analysis in Partial Redundancy Elimination
//
class TR_Latestness
   : public TR_BackwardIntersectionBitVectorAnalysis
   {
   public:

   TR_Latestness(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *, bool trace);

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, ContainerType *);
   virtual TR_Latestness *asLatestness();
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);

   TR_Delayedness *_delayedness;
   ContainerType **_inSetInfo;
   };

// Fifth dataflow analysis in Partial Redundancy Elimination
//
class TR_Isolatedness
   : public TR_BackwardIntersectionBitVectorAnalysis
   {
   public:

   TR_Isolatedness(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *, bool trace);

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   //virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *);
   virtual TR_Isolatedness *asIsolatedness();
   //virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   TR_Latestness *_latestness;
   //ContainerType **_outSetInfo;
   //ContainerType * _temp;
   };

// Factored functionality for liveness analyses to set up local variable indices
class TR_LiveVariableInformation
   {
   public:

   TR_ALLOC(TR_Memory::DataFlowAnalysis)

   TR_LiveVariableInformation(TR::Compilation *comp,
                              TR::Optimizer *,
                              TR_Structure *,
                              bool splitLongs = false,
                              bool includeParms = false,
                              bool includeMethodMetaDataSymbols = false,
                              bool ignoreOSRUses = false);

   bool traceLiveVarInfo()           { return _traceLiveVariableInfo; }

   TR::Compilation *comp()            { return _compilation; }

   TR_Memory *    trMemory()         { return _trMemory; }
   TR_StackMemory trStackMemory()    { return _trMemory; }
   TR_HeapMemory  trHeapMemory()     { return _trMemory; }

   int32_t numLocals()                 { return _numLocals; }
   bool includeParms()                 { return _includeParms; }
   bool splitLongs()                   { return _splitLongs; }
   bool includeMethodMetaDataSymbols() { return _includeMethodMetaDataSymbols; }

   int32_t numNodes()                { return _numNodes; }

   TR_BitVector *liveCommonedLoads() { return _liveCommonedLoads; }
   void createGenAndKillSetCaches();
   void initializeGenAndKillSetInfo(TR_BitVector **genSetInfo, TR_BitVector **killSetInfo,
                                    TR_BitVector **exceptionGenSetInfo, TR_BitVector **exceptionKillSetInfo);

   void trackLiveCommonedLoads();
   void findCommonedLoads(TR::Node *node, TR_BitVector *commonedLoads,
                          bool movingForwardThroughTrees, vcount_t visitCount);
   void findAllUseOfLocal(TR::Node *node, int32_t blockNum,
                          TR_BitVector *genSetInfo, TR_BitVector *killSetInfo,
                          TR_BitVector *commonedLoads, bool movingForwardThroughTrees, vcount_t visitCount);
   void findLocalUsesInBackwardsTreeWalk(TR::Node *node, int32_t blockNum,
                                         TR_BitVector *genSetInfo, TR_BitVector *killSetInfo,
                                         TR_BitVector *commonedLoads, vcount_t visitCount);
   void findUseOfLocal(TR::Node *node, int32_t blockNum,
                       TR_BitVector **genSetInfo, TR_BitVector **killSetInfo,
                       TR_BitVector *commonedLoads, bool movingForwardThroughTrees, vcount_t visitCount);

   private:

   void visitTreeForLocals(TR::Node *node, TR_BitVector **blockGenSetInfo, TR_BitVector *blockKillSetInfo,
                           bool movingForwardThroughTrees, bool visitEntireTree, vcount_t visitCount,
                           TR_BitVector *commonedLoads, bool belowCommonedNode);

   TR::Compilation *_compilation;
   TR_Memory *     _trMemory;
   int32_t         _numLocals;
   bool            _includeParms;
   bool            _includeMethodMetaDataSymbols;
   bool            _splitLongs;
   bool            _traceLiveVariableInfo;
   bool            _ignoreOSRUses;

   // these are caches to generate this information once and share among multiple analyses
   // once they are allocated via createGenAndKillSetCaches(), the next call to initializeGenAndKillSetInfo() will store
   //   the sets into these caches and set _haveCachedGenAndKillSets.  Subsequent calls to initializeGenAndKillSetInfo()
   //   will change the arrays passed in to refer to the bit vectors in these caches.
   bool            _haveCachedGenAndKillSets;
   int32_t         _numNodes;
   TR_BitVector  **_cachedRegularGenSetInfo;
   TR_BitVector  **_cachedRegularKillSetInfo;
   TR_BitVector  **_cachedExceptionGenSetInfo;
   TR_BitVector  **_cachedExceptionKillSetInfo;

   // this bit vector keeps track of locals that are associated with local objects
   TR_BitVector   *_localObjects;

   // this bit vector tracks which commoned loads are currently live
   TR_BitVector   *_liveCommonedLoads;
   };

// Global Collected Reference analysis - find which collected reference locals
// are alive at GC points. This requires a live variable analysis, which is a
// Backward Union BitVector analysis.
//
class TR_Liveness : public TR_BackwardUnionBitVectorAnalysis
   {
   public:

   TR_Liveness(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *,
               bool ignoreOSRUses = false,
               TR_LiveVariableInformation *liveVariableInfo = NULL, bool splitLongs = false, bool includeParms = false);

   TR_LiveVariableInformation *getLiveVariableInfo() { return _liveVariableInfo; }
   virtual Kind getKind();
   virtual TR_Liveness *asLiveness();

   bool traceLiveness() { return _traceLiveness; }


   virtual int32_t getNumberOfBits();
   virtual bool supportsGenAndKillSets();
   virtual void initializeGenAndKillSetInfo();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *);
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   private:

   TR_LiveVariableInformation *_liveVariableInfo;

   bool    _traceLiveness;
   };

// Live on all paths (LOAP) - analysis that identifies when a variable definition is
// live on all subsequent paths.
// Backward Intersection BitVector analysis.
//
class TR_LiveOnAllPaths : public TR_BackwardIntersectionBitVectorAnalysis
   {
   public:

   TR_LiveOnAllPaths(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *,
                     TR_LiveVariableInformation *liveVariableInfo = 0, bool splitLongs = false, bool includeParms = false);

   TR_LiveVariableInformation *getLiveVariableInfo() { return _liveVariableInfo; }
   virtual Kind getKind();
   virtual TR_LiveOnAllPaths *asLiveOnAllPaths();

   virtual int32_t getNumberOfBits();
   virtual bool supportsGenAndKillSets();
   virtual void initializeGenAndKillSetInfo();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *);
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   TR_BitVector **_inSetInfo;
   TR_BitVector **_outSetInfo;

   private:
   TR_LiveVariableInformation *_liveVariableInfo;
   };

// Flow sensitive escape analysis - find which allocations
// have not escaped at a given control flow point, which is an
// Intersection BitVector analysis.
//
class TR_FlowSensitiveEscapeAnalysis : public TR_IntersectionBitVectorAnalysis
   {
   public:

   TR_FlowSensitiveEscapeAnalysis(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure, TR_EscapeAnalysis *e);

   virtual Kind getKind();
   virtual TR_FlowSensitiveEscapeAnalysis *asFlowSensitiveEscapeAnalysis();

   virtual int32_t getNumberOfBits();
   virtual bool supportsGenAndKillSets();
   //virtual void initializeGenAndKillSetInfo();

   virtual void analyzeNode(TR::Node *, bool, int32_t, TR::Node *);
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   private:
   bool getCFGBackEdgesAndLoopEntryBlocks(TR_Structure *structure);
   void collectCFGBackEdges(TR_StructureSubGraphNode *loopEntry);
   //TR_DependentAllocations *getDependentAllocationsFor(Candidate *c);
   //void initializeGenAndKillSetInfoForNode(TR::Node *node, bool seenException, int32_t blockNum, TR::Node *parent);
   TR::Block *findOrSplitEdge(TR::Block *from, TR::Block *to);

   int32_t _numAllocations;
   TR_LinkHead<Candidate> *_candidates;
   TR_LinkHead<FlushCandidate> *_flushCandidates;
   TR_EscapeAnalysis *_escapeAnalysis;
   bool _newlyAllocatedObjectWasLocked;
   TR_BitVector *_blocksWithFlushes, *_blocksWithSyncs; //, *_blocksThatNeedFlush;
   List<TR::CFGEdge> _cfgBackEdges;
   TR_BitVector *_loopEntryBlocks;
   TR_BitVector *_catchBlocks;
   TR_BitVector **_successorInfo;
   TR_BitVector **_predecessorInfo;
   TR_BitVector *_scratch, *_scratch2;
   List<TR_CFGEdgeAllocationPair> _flushEdges;
   List<TR::CFGNode> _splitBlocks;
   //TR_ScratchList<TR_DependentAllocations> _dependentAllocations;
   };

// First dataflow analysis for Shrink Wrapping
//
class TR_RegisterAnticipatability : public TR_BackwardIntersectionBitVectorAnalysis
   {
   public:
   TR_RegisterAnticipatability(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *, TR_BitVector **, bool trace = false);

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *);
   virtual TR_RegisterAnticipatability *asRegisterAnticipatability();
   // actually a misnomer for this analysis because there aren't any
   // treetops to analyze by now
   //
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   void initializeRegisterUsageInfo();

   TR_BitVector **_outSetInfo;

   private:
   TR_BitVector **_registerUsageInfo;
   };

// Second dataflow analysis for Shrink Wrapping
//
class TR_RegisterAvailability : public TR_IntersectionBitVectorAnalysis
   {
   public:
   TR_RegisterAvailability(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *, TR_BitVector **, bool trace = false);

   virtual Kind getKind();

   virtual int32_t getNumberOfBits();
   virtual void analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *);
   virtual TR_RegisterAvailability *asRegisterAvailability();
   virtual void analyzeBlockZeroStructure(TR_BlockStructure *);
   // again a misnomer for this analysis because there aren't any
   // treetops to analyze by now
   //
   virtual void analyzeTreeTopsInBlockStructure(TR_BlockStructure *);
   virtual bool postInitializationProcessing();

   void initializeRegisterUsageInfo();

   TR_BitVector **_inSetInfo;

   private:
   TR_BitVector **_registerUsageInfo;
   };

#endif
