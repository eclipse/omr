/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

///////////////////////////////////////////////////////////////////////////
// Inliner.hpp contains the class definitions for:
//    TR_Inliner
//    TR_TransformInlinedFunction
//    TR_ParameterToArgumentMapper
//    TR_HandleInjectedBasicBlock
///////////////////////////////////////////////////////////////////////////

#ifndef INLINER_INCL
#define INLINER_INCL

#include "optimizer/CallInfo.hpp"

#include <stddef.h>                            // for NULL
#include <stdint.h>                            // for int32_t, uint32_t, etc
#include "env/KnownObjectTable.hpp"        // for KnownObjectTable, etc
#include "codegen/RecognizedMethods.hpp"       // for RecognizedMethod
#include "compile/Compilation.hpp"             // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"         // for TR::Options, etc
#include "env/TRMemory.hpp"                    // for TR_Memory, etc
#include "env/jittypes.h"
#include "il/DataTypes.hpp"                    // for DataTypes
#include "il/ILOpCodes.hpp"                    // for ILOpCodes
#include "il/Node.hpp"                         // for vcount_t, rcount_t
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/Flags.hpp"                     // for flags16_t
#include "infra/Link.hpp"                      // for TR_LinkHead, TR_Link
#include "infra/List.hpp"                      // for List, etc
#include "infra/Random.hpp"                    // for TR_HasRandomGenerator
#include "optimizer/InlinerFailureReason.hpp"
#include "optimizer/Optimization.hpp"          // for Optimization
#include "optimizer/OptimizationManager.hpp"   // for OptimizationManager
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "ras/LogTracer.hpp"                   // for TR_LogTracer


#define MIN_PROFILED_CALL_FREQUENCY (.65f) // lowered this from .80f since opportunities were being missed in WAS; in those cases getting rid of the call even in 65% of the cases was beneficial probably due to the improved icache impact
#define TWO_CALL_PROFILED_FREQUENCY (.30f)
#define MAX_FAN_IN (.60f)
const float SECOND_BEST_MIN_CALL_FREQUENCY = .2275f; //65% of 35%

#define partialTrace(r, ...) \
      if (true) {\
         if ((r)->partialLevel())\
            {\
            (r)->partialTraceM(__VA_ARGS__);\
            }\
      } else \
         (void)0

class TR_BitVector;
class TR_CallStack;
class TR_FrontEnd;
class TR_HashTabInt;
class TR_InlineBlocks;
class TR_InnerAssumption;
class TR_Method;
class TR_PrexArgInfo;
class TR_ResolvedMethod;
class TR_TransformInlinedFunction;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
struct TR_CallSite;
struct TR_CallTarget;
struct TR_ParameterMapping;
struct TR_VirtualGuardSelection;
class OMR_InlinerPolicy;
class OMR_InlinerUtil;

class TR_InlinerTracer : public TR_LogTracer
   {
   public:
      TR_InlinerTracer(TR::Compilation *comp, TR_FrontEnd *fe, TR::Optimization *opt);

      TR_ALLOC(TR_Memory::Inliner);

      TR_FrontEnd * fe()                        { return _fe;   }

      TR_Memory * trMemory()                    { return _trMemory; }
      TR_StackMemory trStackMemory()            { return _trMemory; }
      TR_HeapMemory  trHeapMemory()             { return _trMemory; }
      TR_PersistentMemory * trPersistentMemory(){ return _trMemory->trPersistentMemory(); }

      // determine the tracing level
      bool partialLevel()                            { return comp()->getOption(TR_TracePartialInlining); }        // the > ensures partial tracing gets turned on for debug


      // trace statements for specific tracing levels
      void partialTraceM ( const char *fmt, ...);              // trace statement for investigating partial ininling heuristic

      // counter insertion

      void insertCounter (TR_InlinerFailureReason reason, TR::TreeTop *tt);


      // some dumping routines

      void dumpCallGraphs (TR_LinkHead<TR_CallTarget> *targets);
      void dumpCallStack (TR_CallStack *, const char *fmt, ...);
      void dumpCallSite (TR_CallSite *, const char *fmt, ...);
      void dumpCallTarget (TR_CallTarget *, const char *fmt, ...);
      void dumpInline (TR_LinkHead<TR_CallTarget> *targets, const char *fmt, ...);
      void dumpPartialInline (TR_InlineBlocks *partialInline);
      void dumpPrexArgInfo(TR_PrexArgInfo* argInfo);

      void dumpDeadCalls(TR_LinkHead<TR_CallSite> *sites);

      // failure handling assistance

      const char * getFailureReasonString(TR_InlinerFailureReason reason)       { return TR_InlinerFailureReasonStr[reason]; }

      const char * getGuardKindString(TR_VirtualGuardSelection *guard);
      const char * getGuardTypeString(TR_VirtualGuardSelection *guard);

      template <typename ObjWithSignatureMethod>
      const char* traceSignature(ObjWithSignatureMethod *obj)
         {
         return heuristicLevel() ? obj->signature(trMemory()) : "";
         }

   protected:

      TR_Memory *             _trMemory;
      TR_FrontEnd *           _fe;

   };

class TR_InlinerDelimiter
   {

   public:
   TR_InlinerDelimiter(TR_InlinerTracer * tracer, char * tag);
   ~TR_InlinerDelimiter();

   protected:
   TR_InlinerTracer * _tracer;
   char *_tag;
   };

class TR_Inliner : public TR::Optimization
   {
   public:
      TR_Inliner(TR::OptimizationManager *manager);
      static TR::Optimization *create(TR::OptimizationManager *manager)
         {
         return new (manager->allocator()) TR_Inliner(manager);
         }

      virtual int32_t perform();
      virtual const char * optDetailString() const throw();
   };

class TR_TrivialInliner : public TR::Optimization
   {
   public:
      TR_TrivialInliner(TR::OptimizationManager *manager);
      static TR::Optimization *create(TR::OptimizationManager *manager)
         {
         return new (manager->allocator()) TR_TrivialInliner(manager);
         }

      virtual int32_t perform();
      virtual const char * optDetailString() const throw();
   };

class TR_InnerPreexistenceInfo
   {
   public:
      TR_ALLOC(TR_Memory::Inliner);

      TR_InnerPreexistenceInfo(TR::Compilation * c, TR::ResolvedMethodSymbol *methodSymbol, TR_CallStack *callStack,
                               TR::TreeTop *callTree, TR::Node *callNode,
                               TR_VirtualGuardKind _guardKind);

      TR::Compilation * comp()                  { return _comp; }
      TR_Memory * trMemory()                    { return _trMemory; }
      TR_StackMemory trStackMemory()            { return _trMemory; }
      TR_HeapMemory  trHeapMemory()             { return _trMemory; }
      TR_PersistentMemory * trPersistentMemory(){ return _trMemory->trPersistentMemory(); }

      bool hasInnerAssumptions()                                { return !_assumptions.isEmpty(); }

      virtual bool perform(TR::Compilation *comp, TR::Node *guardNode, bool & disableTailRecursion);

   protected:
      TR::Compilation *         _comp;
      TR_Memory *              _trMemory;
      TR::ResolvedMethodSymbol *_methodSymbol; // the method being called
      TR_CallStack            *_callStack;    // the stack of callers (not including _methodSymbol)
      TR::TreeTop              *_callTree;
      TR::Node                 *_callNode;
      int32_t                  _numArgs;      // number of arguments of methodSymbol
      TR_VirtualGuardKind      _guardKind;

      List<TR_InnerAssumption> _assumptions;  // any inner assumptions
   };


struct TR_VirtualGuardSelection
   {
   TR_ALLOC(TR_Memory::Inliner);

   TR_VirtualGuardSelection
   (TR_VirtualGuardKind kind, TR_VirtualGuardTestType type = TR_NonoverriddenTest, TR_OpaqueClassBlock *thisClass= 0)
      : _kind(kind), _type(type), _thisClass(thisClass), _highProbabilityProfiledGuard(false) {}
   TR_VirtualGuardKind  _kind;
   TR_VirtualGuardTestType  _type;

   // the type is provided only when the test type is a vft test
   TR_OpaqueClassBlock *_thisClass;

   // only for a MutableCallSiteTargetGuard
   uintptrj_t                *_mutableCallSiteObject;
   TR::KnownObjectTable::Index _mutableCallSiteEpoch;

   // this flag is used to signify a profiled guard for which
   // we believe it's very likely to be true. It's used to mark
   // the slow path as cold so optimizations like escape analysis can
   // work on the fast path.
   bool _highProbabilityProfiledGuard;

   void setIsHighProbablityProfiledGuard(bool b=true)
      {
      TR_ASSERT(_kind==TR_ProfiledGuard, "only valid for profiled guards");
      _highProbabilityProfiledGuard = b;
      }
   bool isHighProbablityProfiledGuard()
      {
      return _highProbabilityProfiledGuard;
      }
   };

class TR_InlinerBase: public TR_HasRandomGenerator
   {
   protected:
      virtual bool supportsMultipleTargetInlining () { return false ; }

   public:
      vcount_t getVisitCount() { return _visitCount;}

      void setPolicy(OMR_InlinerPolicy *policy){ _policy = policy; }
      void setUtil(OMR_InlinerUtil *util){ _util = util; }
      OMR_InlinerPolicy *getPolicy(){ TR_ASSERT(_policy != NULL, "inliner policy must be set"); return _policy; }
      OMR_InlinerUtil *getUtil(){ TR_ASSERT(_util != NULL, "inliner util must be set"); return _util; }

      bool allowBiggerMethods() { return comp()->isServerInlining(); }


      bool isEDODisableInlinedProfilingInfo () {return _EDODisableInlinedProfilingInfo; }

      void setInlineThresholds(TR::ResolvedMethodSymbol *callerSymbol);

      void performInlining(TR::ResolvedMethodSymbol *);

      int32_t getCurrentNumberOfNodes() { return _currentNumberOfNodes; }
      int32_t getMaxInliningCallSites() { return _maxInliningCallSites; }
      int32_t getNumAsyncChecks() { return _numAsyncChecks; }
      int32_t getNodeCountThreshold() { return _nodeCountThreshold;}
      int32_t getMaxRecursiveCallByteCodeSizeEstimate() { return _maxRecursiveCallByteCodeSizeEstimate;}
      void incCurrentNumberOfNodes(int32_t i) { _currentNumberOfNodes+=i; }

      bool inlineVirtuals()                     { return _flags.testAny(InlineVirtuals); }
      void setInlineVirtuals(bool b)            { _flags.set(InlineVirtuals, b); }
      bool inlineSynchronized()                 { return _flags.testAny(InlineSynchronized); }
      void setInlineSynchronized(bool b)        { _flags.set(InlineSynchronized, b); }
      bool firstPass()                          { return _flags.testAny(FirstPass); }
      void setFirstPass(bool b=true)            { _flags.set(FirstPass, b); }
      uint32_t getSizeThreshold()               { return _methodByteCodeSizeThreshold; }
      void     setSizeThreshold(uint32_t size);
      virtual bool inlineRecognizedMethod(TR::RecognizedMethod method);


      void createParmMap(TR::ResolvedMethodSymbol *calleeSymbol, TR_LinkHead<TR_ParameterMapping> &map);

      TR::Compilation * comp()                   { return _optimizer->comp();}
      TR_Memory * trMemory()                    { return _trMemory; }
      TR_StackMemory trStackMemory()            { return _trMemory; }
      TR_HeapMemory  trHeapMemory()             { return _trMemory; }
      TR_PersistentMemory * trPersistentMemory(){ return _trMemory->trPersistentMemory(); }
      TR_FrontEnd    * fe()                     { return _optimizer->comp()->fe(); }


      TR::Optimizer * getOptimizer()         { return _optimizer; }


      TR_InlinerTracer* tracer()                 { return _tracer; }

      void setStoreToCachedPrivateStatic(TR::Node *node) { _storeToCachedPrivateStatic = node; }
      TR::Node *getStoreToCachedPrivateStatic() { return _storeToCachedPrivateStatic; }
      bool alwaysWorthInlining(TR_ResolvedMethod * calleeMethod, TR::Node *callNode);

      void getSymbolAndFindInlineTargets(TR_CallStack *, TR_CallSite *, bool findNewTargets=true);

      void applyPolicyToTargets(TR_CallStack *, TR_CallSite *);
      bool callMustBeInlinedRegardlessOfSize(TR_CallSite *callsite);

      bool forceInline(TR_CallTarget *calltarget);
      bool forceVarInitInlining(TR_CallTarget *calltarget);
      bool forceCalcInlining(TR_CallTarget *calltarget);

   protected:

      TR_InlinerBase(TR::Optimizer *, TR::Optimization *);

      TR_PrexArgInfo* buildPrexArgInfoForOutermostMethod(TR::ResolvedMethodSymbol* methodSymbol);
      bool inlineCallTarget(TR_CallStack *,TR_CallTarget *calltarget, bool inlinefromgraph, TR_PrexArgInfo *argInfo = 0, TR::TreeTop** cursorTreeTop = NULL);
      bool inlineCallTarget2(TR_CallStack *, TR_CallTarget *calltarget, TR::TreeTop** cursorTreeTop, bool inlinefromgraph, int32_t);

      //inlineCallTarget2 methods
      bool tryToInlineTrivialMethod (TR_CallStack* callStack, TR_CallTarget* calltarget);

      bool tryToGenerateILForMethod (TR::ResolvedMethodSymbol* calleeSymbol, TR::ResolvedMethodSymbol* callerSymbol, TR_CallTarget* calltarget);

      void inlineFromGraph(TR_CallStack *, TR_CallTarget *calltarget, TR_InnerPreexistenceInfo *);
      TR_CallSite* findAndUpdateCallSiteInGraph(TR_CallStack *callStack, TR_ByteCodeInfo &bcInfo, TR::TreeTop *tt, TR::Node *parent, TR::Node *callNode, TR_CallTarget *calltarget);

      void initializeControlFlowInfo(TR_CallStack *,TR::ResolvedMethodSymbol *);

      void cleanup(TR::ResolvedMethodSymbol *, bool);


      int checkInlineableWithoutInitialCalleeSymbol(TR_CallSite* callsite, TR::Compilation* comp);

      void getBorderFrequencies(int32_t &hotBorderFrequency, int32_t &coldBorderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode);
      int32_t scaleSizeBasedOnBlockFrequency(int32_t bytecodeSize, int32_t frequency, int32_t borderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode, int32_t coldBorderFrequency = 0); // not virtual because we want base class always calling base version of this
      virtual bool exceedsSizeThreshold(TR_CallSite *callSite, int bytecodeSize, TR::Block * callNodeBlock, TR_ByteCodeInfo & bcInfo,  int32_t numLocals=0, TR_ResolvedMethod * caller = 0, TR_ResolvedMethod * calleeResolvedMethod = 0, TR::Node * callNode = 0, bool allConsts = false);
      virtual bool inlineCallTargets(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *info)
         {
         TR_ASSERT(0, "invalid call to TR_InlinerBase::inlineCallTargets");
         return false;
         }
      TR::TreeTop * addGuardForVirtual(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::TreeTop *, TR::Node *,
                                      TR_OpaqueClassBlock *, TR::TreeTop *, TR::TreeTop *, TR::TreeTop *, TR_TransformInlinedFunction&,
                                      List<TR::SymbolReference> &, TR_VirtualGuardSelection *, TR::TreeTop **, TR_CallTarget *calltarget=0);

      void addAdditionalGuard(TR::Node *callNode, TR::ResolvedMethodSymbol * calleeSymbol, TR_OpaqueClassBlock *thisClass, TR::Block *prevBlock, TR::Block *inlinedBody, TR::Block *slowPath, TR_VirtualGuardKind kind, TR_VirtualGuardTestType type, bool favourVftCompare, TR::CFG *callerCFG);
      void rematerializeCallArguments(TR_TransformInlinedFunction & tif,
   TR_VirtualGuardSelection *guard, TR::Node *callNode, TR::Block *block1, TR::TreeTop *rematPoint);
      void replaceCallNode(TR::ResolvedMethodSymbol *, TR::Node *, rcount_t, TR::TreeTop *, TR::Node *, TR::Node *);
      void linkOSRCodeBlocks();
      bool IsOSRFramesSizeUnderThreshold();
      bool heuristicForUsingOSR(TR::Node *, TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, bool);

      TR::Node * createVirtualGuard(TR::Node *, TR::ResolvedMethodSymbol *, TR::TreeTop *, int16_t, TR_OpaqueClassBlock *, bool, TR_VirtualGuardSelection *);
   private:

      void replaceCallNodeReferences(TR::Node *, TR::Node *, uint32_t, TR::Node *, TR::Node *, rcount_t &);
      void cloneChildren(TR::Node *, TR::Node *, uint32_t);

   protected:
      enum
         {
         //Available                 = 0x0001,
         InlineVirtuals              = 0x0002,
         InlineSynchronized          = 0x0004,
         FirstPass                   = 0x0008,
         lastDummyEnum
         };

      TR::Optimizer *       _optimizer;
      TR_Memory *              _trMemory;
      List<TR::SymbolReference> _availableTemps;
      List<TR::SymbolReference> _availableBasicBlockTemps;
      flags16_t                _flags;
      vcount_t                 _visitCount;
      bool                     _inliningAsWeWalk;
      bool                     _EDODisableInlinedProfilingInfo;
      bool                     _disableTailRecursion;
      bool                     _disableInnerPrex;
      bool                      _isInLoop;

      int32_t                  _maxRecursiveCallByteCodeSizeEstimate;
      int32_t                  _callerWeightLimit;
      int32_t                  _methodByteCodeSizeThreshold;
      int32_t                  _methodInColdBlockByteCodeSizeThreshold;
      int32_t                  _methodInWarmBlockByteCodeSizeThreshold;
      int32_t                  _nodeCountThreshold;
      int32_t                  _maxInliningCallSites;

      int32_t                  _numAsyncChecks;
      bool                      _aggressivelyInlineInLoops;

      int32_t                   _currentNumberOfNodes;

      TR_LinkHead<TR_CallSite> _deadCallSites;

      TR::Node *_storeToCachedPrivateStatic;

      TR_InlinerTracer*         _tracer;
      List<TR::Block>                           _GlobalLabels;
      List<TR::Block > * getSuccessorsWithGlobalLabels(){ return &_GlobalLabels; }
      OMR_InlinerPolicy *_policy;
      OMR_InlinerUtil*_util;
   };

class TR_DumbInliner : public TR_InlinerBase
   {
   public:
      TR_DumbInliner(TR::Optimizer *, TR::Optimization *, uint32_t initialSize, uint32_t dumbReductionIncrement = 5);
      virtual bool inlineCallTargets(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *);
      bool tryToInline(char *message, TR_CallTarget *calltarget);
   protected:
      virtual bool analyzeCallSite(TR_CallStack *, TR::TreeTop *, TR::Node *, TR::Node *);

      uint32_t _initialSize;
      uint32_t _dumbReductionIncrement;
   };

class TR_InlineCall : public TR_DumbInliner
   {
   public:
      TR_InlineCall(TR::Optimizer *, TR::Optimization *);

      virtual bool inlineRecognizedMethod(TR::RecognizedMethod method);
      bool inlineCall(TR::TreeTop * callNodeTreeTop, TR_OpaqueClassBlock * thisType = 0, bool recursiveInlining = false, TR_PrexArgInfo *argInfo = 0, int32_t initialMaxSize = 0);
   };

struct TR_ParameterMapping : TR_Link<TR_ParameterMapping>
   {
   TR_ParameterMapping(TR::ParameterSymbol * ps)
      : _parmSymbol(ps), _replacementSymRef(0), _replacementSymRef2(0),
         _parameterNode(0),_replacementSymRef3(0),
        _parmIsModified(false), _addressTaken(false), _isConst(false)
      { }

   TR::ParameterSymbol * _parmSymbol;
   TR::SymbolReference * _replacementSymRef;
   TR::Node *            _parameterNode;                 //The Node under the call which matches the argument at argIndex
   TR::SymbolReference * _replacementSymRef2;
   TR::SymbolReference * _replacementSymRef3;
   uint32_t             _argIndex;
   bool                 _parmIsModified;
   bool                 _isConst;
   bool                 _addressTaken;
   };

class TR_ParameterToArgumentMapper
   {
   public:
      TR_ALLOC(TR_Memory::Inliner)

      TR_ParameterToArgumentMapper(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::Node *,
                                   List<TR::SymbolReference> &, List<TR::SymbolReference> &,
                                   List<TR::SymbolReference> &, TR_InlinerBase *);

      void                  initialize(TR_CallStack *callStack);
      TR::Node *             map(TR::Node *, TR::ParameterSymbol *, bool);
      void                   mapOSRCallSiteRematTable(uint32_t siteIndex);
      void                  adjustReferenceCounts();
      TR::Node *             fixCallNodeArgs(bool);
      TR::TreeTop *          firstTempTreeTop()   { return _firstTempTreeTop; }
      TR::TreeTop *          lastTempTreeTop()    { return _lastTempTreeTop;  }
      TR_ParameterMapping * getFirstMapping()    { return _mappings.getFirst(); }

      void                  printMapping();

      TR::Compilation * comp()                   { return _inliner->comp(); }
      TR_Memory * trMemory()                    { return comp()->trMemory(); }
      TR_StackMemory trStackMemory()            { return trMemory(); }
      TR_HeapMemory  trHeapMemory()             { return trMemory(); }
      TR_InlinerTracer * tracer()               { return _inliner->tracer(); }

      TR_ParameterMapping * findMapping(TR::Symbol *);

   private:

      void lookForModifiedParameters();
      void lookForModifiedParameters(TR::Node *);

      TR::ResolvedMethodSymbol *        _calleeSymbol;
      TR::ResolvedMethodSymbol *        _callerSymbol;
      TR::SymbolReference *             _vftReplacementSymRef;
      TR::Node *                        _callNode;
      TR_LinkHead<TR_ParameterMapping> _mappings;
      TR::TreeTop *                     _firstTempTreeTop;
      TR::TreeTop *                     _lastTempTreeTop;
      List<TR::SymbolReference> &       _tempList;
      List<TR::SymbolReference> &       _availableTemps;
      List<TR::SymbolReference> &       _availableTemps2;
      TR_InlinerTracer *               _tracer;
      TR_InlinerBase *                 _inliner;
   };

class OMR_InlinerHelper
   {
   friend class TR_InlinerBase;
   public:
      TR_InlinerTracer * tracer()               { return _inliner->tracer(); }
      TR_InlinerBase *inliner()                 { return _inliner; }
   protected:
      TR_InlinerBase * _inliner;
      void setInliner(TR_InlinerBase *inliner) { _inliner = inliner; }
   };

class OMR_InlinerUtil : public TR::OptimizationUtil, public OMR_InlinerHelper
   {
   friend class TR_InlinerBase;
   public:
      OMR_InlinerUtil(TR::Compilation *comp);
      static TR::TreeTop * storeValueInATemp(TR::Compilation *comp, TR::Node *, TR::SymbolReference * &, TR::TreeTop *, TR::ResolvedMethodSymbol *, List<TR::SymbolReference> & tempList, List<TR::SymbolReference> & availableTemps, List<TR::SymbolReference> * moreTemps, bool behavesLikeTemp = true, TR::TreeTop ** newStoreValueATreeTop = NULL, bool isIndirect = false, int32_t offset = 0);
      virtual bool addTargetIfMethodIsNotOverridenInReceiversHierarchy(TR_IndirectCallSite *callsite);
      virtual TR_ResolvedMethod *findSingleJittedImplementer(TR_IndirectCallSite *callsite);
      virtual bool addTargetIfThereIsSingleImplementer (TR_IndirectCallSite *callsite);
      virtual TR_PrexArgInfo* createPrexArgInfoForCallTarget(TR_VirtualGuardSelection *guard, TR_ResolvedMethod *implementer);
      virtual TR_InnerPreexistenceInfo *createInnerPrexInfo(TR::Compilation * c, TR::ResolvedMethodSymbol *methodSymbol, TR_CallStack *callStack, TR::TreeTop *callTree, TR::Node *callNode, TR_VirtualGuardKind guardKind);
      virtual TR_InlinerTracer * getInlinerTracer(TR::Optimization *optimization);
      virtual void adjustByteCodeSize(TR_ResolvedMethod *calleeResolvedMethod, bool isInLoop, TR::Block *block, int &bytecodeSize){ return; }
      virtual void adjustCallerWeightLimit(TR::ResolvedMethodSymbol *callSymbol, int &callerWeightLimit){ return; }
      virtual void adjustMethodByteCodeSizeThreshold(TR::ResolvedMethodSymbol *callSymbol, int &methodByteCodeSizeThreshold){ return; }
      virtual TR_PrexArgInfo *computePrexInfo(TR_CallTarget *target);
      virtual void collectCalleeMethodClassInfo(TR_ResolvedMethod *calleeMethod);

      /**
       * \brief
       *    Check if another pass of targeted inlining is needed if the given method is inlined.
       *
       * \parm callee
       *    The method symbol of the callee.
       *
       * \return
       *    True if another pass of targeted inlining is needed, otherwise false.
       *
       * \note
       *    Targeted inlining is meant to deal with method call chains where the receiver of current call stores the information
       *    of the next call, which is usually the receiver or the method of the next call. Knowing the receiver of the first call
       *    means we can devirtualize all the calls along the chain.
       *
       *    Each method except the last one in the chain usually contains simple code that manipulates the arguments for the next
       *    call. The last method in the chain is usually the most important one, it does the real job and can contain arbitrary
       *    code.
       *
       *    The goal of targeted inlining is to inline along the call chain to get the most important method inlined, which is the
       *    last one in the chain. It does not care about other callsites.
       */
      virtual bool needTargetedInlining(TR::ResolvedMethodSymbol *callee);
   protected:
      virtual void refineColdness (TR::Node* node, bool& isCold);
      virtual void computeMethodBranchProfileInfo (TR::Block * cfgBlock, TR_CallTarget* calltarget, TR::ResolvedMethodSymbol* callerSymbol);
      virtual int32_t getCallCount(TR::Node *callNode);
      virtual void calleeTreeTopPreMergeActions(TR::ResolvedMethodSymbol *calleeResolvedMethodSymbol, TR_CallTarget* calltarget);
      virtual void refineInlineGuard(TR::Node *callNode, TR::Block *&block1, TR::Block *&block2,
                   bool &appendTestToBlock1, TR::ResolvedMethodSymbol * callerSymbol, TR::TreeTop *cursorTree,
                   TR::TreeTop *&virtualGuard, TR::Block *block4);
      virtual void refineInliningThresholds(TR::Compilation *comp, int32_t &callerWeightLimit, int32_t &maxRecursiveCallByteCodeSizeEstimate, int32_t &methodByteCodeSizeThreshold, int32_t &methodInWarmBlockByteCodeSizeThreshold, int32_t &methodInColdBlockByteCodeSizeThreshold, int32_t &nodeCountThreshold, int32_t size){ }
      virtual void estimateAndRefineBytecodeSize(TR_CallSite* callsite, TR_CallTarget* target, TR_CallStack *callStack, int32_t &bytecodeSize);
      virtual TR_TransformInlinedFunction *getTransformInlinedFunction(TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::Block *, TR::TreeTop *,
                                  TR::Node *, TR_ParameterToArgumentMapper &, TR_VirtualGuardSelection *, List<TR::SymbolReference> &,
                                  List<TR::SymbolReference> &, List<TR::SymbolReference> &);
   };

class OMR_InlinerPolicy : public TR::OptimizationPolicy, public OMR_InlinerHelper
   {
   friend class TR_InlinerBase;
   public:
      OMR_InlinerPolicy(TR::Compilation *comp);
      virtual bool inlineRecognizedMethod(TR::RecognizedMethod method);
      int32_t getInitialBytecodeSize(TR::ResolvedMethodSymbol * methodSymbol, TR::Compilation *comp);
      virtual int32_t getInitialBytecodeSize(TR_ResolvedMethod *feMethod, TR::ResolvedMethodSymbol * methodSymbol, TR::Compilation *comp);
      virtual bool tryToInline(TR_CallTarget *, TR_CallStack *, bool);
      virtual bool inlineMethodEvenForColdBlocks(TR_ResolvedMethod *method);
      bool aggressiveSmallAppOpts() { return TR::Options::getCmdLineOptions()->getOption(TR_AggressiveOpts); }
      virtual bool willBeInlinedInCodeGen(TR::RecognizedMethod method);
      virtual bool canInlineMethodWhileInstrumenting(TR_ResolvedMethod *method);

      /** \brief
       *     Determines whether to skip generation of HCR guards for a particular callee during inlining.
       *
       *  \param callee
       *     The callee to be inlined.
       *
       *  \return
       *     true if we can skip generating HCR guards for this \p callee; false otherwise.
       */
      virtual bool skipHCRGuardForCallee(TR_ResolvedMethod* callee)
         {
         return false;
         }

      virtual bool dontPrivatizeArgumentsForRecognizedMethod(TR::RecognizedMethod recognizedMethod);
      virtual bool replaceSoftwareCheckWithHardwareCheck(TR_ResolvedMethod *calleeMethod);
   protected:
      virtual bool tryToInlineTrivialMethod (TR_CallStack* callStack, TR_CallTarget* calltarget);
      virtual bool alwaysWorthInlining(TR_ResolvedMethod * calleeMethod, TR::Node *callNode);
      virtual void determineInliningHeuristic(TR::ResolvedMethodSymbol *callerSymbol);
      bool tryToInlineGeneral(TR_CallTarget *, TR_CallStack *, bool);
      virtual bool callMustBeInlined(TR_CallTarget *calltarget);
      bool mustBeInlinedEvenInDebug(TR_ResolvedMethod * calleeMethod, TR::TreeTop *callNodeTreeTop);
      virtual bool supressInliningRecognizedInitialCallee(TR_CallSite* callsite, TR::Compilation* comp);
      virtual TR_InlinerFailureReason checkIfTargetInlineable(TR_CallTarget* target, TR_CallSite* callsite, TR::Compilation* comp);
      virtual bool suitableForRemat(TR::Compilation *comp, TR::Node *node, TR_VirtualGuardSelection *guard);
   };

class TR_TransformInlinedFunction
   {
   public:
      TR_TransformInlinedFunction(TR::Compilation*, TR_InlinerTracer *tracer, TR::ResolvedMethodSymbol *, TR::ResolvedMethodSymbol *, TR::Block *, TR::TreeTop *,
                                  TR::Node *, TR_ParameterToArgumentMapper &, TR_VirtualGuardSelection *, List<TR::SymbolReference> &,
                                  List<TR::SymbolReference> &, List<TR::SymbolReference> &);
      virtual void transform();

      TR::Node *            resultNode()                 { return _resultNode; }
      TR::TreeTop *         firstBBEnd()                 { return _firstBBEnd; }
      TR::TreeTop *         lastBBStart()                { return _lastBBStart; }
      TR::TreeTop *         lastMainLineTreeTop()        { return _lastMainLineTreeTop; }
      TR::Block *           firstCatchBlock()            { return _firstCatchBlock; }
      bool                 crossedBasicBlock()          { return _crossedBasicBlock; }
      TR::TreeTop *         simpleCallReferenceTreeTop() { return _simpleCallReferenceTreeTop; }
      TR::SymbolReference * resultTempSymRef()           { return _resultTempSymRef; }
      List<TR::TreeTop> &   treeTopsToRemove()           { return _treeTopsToRemove; }
      List<TR::Block> &     blocksWithEdgesToTheEnd()    { return _blocksWithEdgesToTheEnd; }
      TR::TreeTop *         callNodeTreeTop()            { return _callNodeTreeTop; }
      TR::Node *            getCallNode()                { return _callNode; }
      void                 setResultNode(TR::Node * n)   { _resultNode = n; }
      bool                 favourVftCompare()           { return _favourVftCompare; }

      TR::ResolvedMethodSymbol *getCallerSymbol()   { return _callerSymbol; }

      TR_ParameterToArgumentMapper &getParameterMapper(){ return _parameterMapper; }

      bool traceVIP() { return _traceVIP; }

#define MAX_FIND_SIMPLE_CALL_REFERENCE_DEPTH 12
      void setFindCallNodeRecursionDepthToMax() { findCallNodeRecursionDepth = MAX_FIND_SIMPLE_CALL_REFERENCE_DEPTH; }
      void setMultiRefNodeIsCallRecursionDepthToMax() { onlyMultiRefNodeIsCallNodeRecursionDepth = MAX_FIND_SIMPLE_CALL_REFERENCE_DEPTH; }

      bool findCallNodeInTree (TR::Node * callNode, TR::Node * node);
      TR::TreeTop * findSimpleCallReference(TR::TreeTop * callNodeTreeTop, TR::Node * callNode);
      bool findReturnValueInTree(TR::Node * rvStoreNode, TR::Node * node, TR::Compilation *comp);
      bool onlyMultiRefNodeIsCallNode(TR::Node * callNode, TR::Node * node);

      TR::Compilation * comp()                   { return _comp; }
      TR_InlinerTracer * tracer()               { return _tracer; }
      TR_Memory * trMemory()                    { return comp()->trMemory(); }
      TR_StackMemory trStackMemory()            { return trMemory(); }
      TR_HeapMemory  trHeapMemory()             { return trMemory(); }

   protected:
      void                 transformNode(TR::Node *, TR::Node *, uint32_t);
      void                 transformReturn(TR::Node *, TR::Node *);

      TR::Compilation *               _comp;
      TR_InlinerTracer *             _tracer;
      TR::ResolvedMethodSymbol *      _calleeSymbol;
      TR::ResolvedMethodSymbol *      _callerSymbol;
      TR::TreeTop *                   _callNodeTreeTop;
      TR::Node *                      _callNode;
      TR_ParameterToArgumentMapper & _parameterMapper;
      TR::TreeTop *                   _currentTreeTop;
      TR::TreeTop *                   _firstBBEnd;
      TR::TreeTop *                   _lastBBStart;
      TR::TreeTop *                   _penultimateTreeTop;
      TR::TreeTop *                   _lastMainLineTreeTop;
      TR::Node *                      _resultNode;
      TR::SymbolReference *           _resultTempSymRef;
      TR::Block *                     _generatedLastBlock;
      TR::Block *                     _firstCatchBlock;
      TR::TreeTop *                   _simpleCallReferenceTreeTop;
      List<TR::SymbolReference> &     _tempList;
      List<TR::SymbolReference> &     _availableTemps;
      List<TR::SymbolReference> &     _availableTemps2;
      List<TR::TreeTop>               _treeTopsToRemove;
      List<TR::Block >                _blocksWithEdgesToTheEnd;
      bool                           _processingExceptionHandlers;
      bool                           _favourVftCompare;
      bool                           _determineIfReturnCanBeReplacedWithCallNodeReference;
      bool                           _traceVIP; // traceValueInitPropagation
      bool                           _crossedBasicBlock;

      int32_t findCallNodeRecursionDepth;
      int32_t onlyMultiRefNodeIsCallNodeRecursionDepth;

   };

class TR_HandleInjectedBasicBlock
   {
   public:
      TR_HandleInjectedBasicBlock(TR::Compilation *, TR_InlinerTracer *, TR::ResolvedMethodSymbol *, List<TR::SymbolReference> &, List<TR::SymbolReference> &,
                                  List<TR::SymbolReference> &, TR_ParameterMapping * = 0);

      void findAndReplaceReferences(TR::TreeTop *, TR::Block *, TR::Block *);
      TR::Node * findNullCheckReferenceSymbolReference(TR::TreeTop *);
      TR::Compilation *comp()                    { return _comp; }
      TR_InlinerTracer *tracer()                { return _tracer; }

   private:
      void printNodesWithMultipleReferences();
      void collectNodesWithMultipleReferences(TR::TreeTop *, TR::Node *, TR::Node *);
      void replaceNodesReferencedFromAbove(TR::Block *, vcount_t);
      void replaceNodesReferencedFromAbove(TR::TreeTop *, TR::Node *, TR::Node *, uint32_t, vcount_t);
      void createTemps(bool);

      struct MultiplyReferencedNode : TR_Link<MultiplyReferencedNode>
         {
         MultiplyReferencedNode(TR::Node *node, TR::TreeTop *tt, uint32_t refsToBeFound,bool symCanBeReloaded);
         TR::Node *                _node;
         TR::TreeTop *             _treeTop;
         TR::SymbolReference *     _replacementSymRef;
         TR::SymbolReference *     _replacementSymRef2;
         TR::SymbolReference *     _replacementSymRef3;
         uint32_t                 _referencesToBeFound;
         bool                     _isConst;
         bool                     _symbolCanBeReloaded;
         };

      MultiplyReferencedNode * find(TR::Node *);
      void                     remove(MultiplyReferencedNode *);
      void                     add(TR::TreeTop *, TR::Node *);
      void                     replace(MultiplyReferencedNode *, TR::TreeTop *, TR::Node *, uint32_t);

      TR::Compilation *                    _comp;
      TR_InlinerTracer *                  _tracer;
      TR_LinkHead<MultiplyReferencedNode> _multiplyReferencedNodes;
      TR_LinkHead<MultiplyReferencedNode> _fixedNodes;
      List<TR::SymbolReference> &          _tempList;
      List<TR::SymbolReference> &          _injectedBasicBlockTemps;
      List<TR::SymbolReference> &          _availableTemps;
      TR::ResolvedMethodSymbol *           _methodSymbol;
      TR_ParameterMapping *               _mappings;
   };
#endif
