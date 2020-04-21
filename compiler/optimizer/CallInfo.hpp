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

///////////////////////////////////////////////////////////////////////////
// CallGraph.hpp contains the class definitions for:
// TR_CallSite --> represents a callsite  (ex, a virtual call)
// TR_CallTarget --> represents a concrete implementation (ex a specific method)
///////////////////////////////////////////////////////////////////////////

#ifndef CALLGRAPH_INCL
#define CALLGRAPH_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/deque.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/InlinerFailureReason.hpp"

namespace TR { class CompilationFilters;}
class TR_FrontEnd;
class TR_InlineBlocks;
class TR_InlinerBase;
class TR_InlinerTracer;
class TR_InnerPreexistenceInfo;
class TR_PrexArgInfo;
class TR_ResolvedMethod;
namespace TR { class AutomaticSymbol; }
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class Method; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
struct TR_CallSite;
struct TR_VirtualGuardSelection;

class TR_CallStack : public TR_Link<TR_CallStack>
   {
   public:
      TR_CallStack(TR::Compilation *, TR::ResolvedMethodSymbol *, TR_ResolvedMethod *, TR_CallStack *, int32_t, bool safeToAddSymRefs = false);
      ~TR_CallStack();
      void commit();

      TR::Compilation * comp()                   { return _comp; }
      TR_Memory * trMemory()                    { return _trMemory; }
      TR_StackMemory trStackMemory()            { return _trMemory; }
      TR_HeapMemory  trHeapMemory()             { return _trMemory; }
      TR_PersistentMemory * trPersistentMemory(){ return _trMemory->trPersistentMemory(); }

      void initializeControlFlowInfo(TR::ResolvedMethodSymbol *);
      TR_CallStack * isCurrentlyOnTheStack(TR_ResolvedMethod *, int32_t);
      bool           isAnywhereOnTheStack (TR_ResolvedMethod *, int32_t); // Even searches calls already inlined by previous opts
      void updateState(TR::Block *);
      void addAutomatic(TR::AutomaticSymbol * a);
      void addTemp(TR::SymbolReference *);
      void addInjectedBasicBlockTemp(TR::SymbolReference *);
      void makeTempsAvailable(List<TR::SymbolReference> &);
      void makeTempsAvailable(List<TR::SymbolReference> &, List<TR::SymbolReference> &);
      void makeBasicBlockTempsAvailable(List<TR::SymbolReference> &);

      struct BlockInfo
         {
         TR_ALLOC(TR_Memory::Inliner);

         BlockInfo() : _alwaysReached(false) { }

         bool           _inALoop;
         bool           _alwaysReached;
         };

      BlockInfo & blockInfo(int32_t);

      class SetCurrentCallNode
         {
         TR_CallStack &_cs;
         TR::Node      *_originalValue;

         public:
         SetCurrentCallNode(TR_CallStack &cs, TR::Node *callNode)
            :_cs(cs),_originalValue(cs._currentCallNode)
            { _cs._currentCallNode = callNode; }

         ~SetCurrentCallNode()
            { _cs._currentCallNode = _originalValue; }
         };

      TR::Compilation *          _comp;
      TR_Memory *               _trMemory;
      TR::ResolvedMethodSymbol * _methodSymbol;
      TR_ResolvedMethod *       _method;
      TR::Node *                 _currentCallNode;
      BlockInfo *               _blockInfo;
      List<TR::AutomaticSymbol>  _autos;
      List<TR::SymbolReference>  _temps;
      List<TR::SymbolReference>  _injectedBasicBlockTemps;
      TR_InnerPreexistenceInfo *_innerPrexInfo;
      TR::CompilationFilters *   _inlineFilters;
      int32_t                   _maxCallSize;
      bool                      _inALoop;
      bool                      _alwaysCalled;
      bool                      _safeToAddSymRefs;

   };

struct TR_CallTarget : public TR_Link<TR_CallTarget>
   {
   TR_ALLOC(TR_Memory::Inliner);

   friend class TR_InlinerTracer;

   TR_CallTarget(TR_CallSite *callsite,
                 TR::ResolvedMethodSymbol *calleeSymbol,
                 TR_ResolvedMethod *calleeMethod,
                 TR_VirtualGuardSelection *guard,
                 TR_OpaqueClassBlock *receiverClass,
                 TR_PrexArgInfo *ecsPrexArgInfo,
                 float freqAdj=1.0);

   const char* signature(TR_Memory *trMemory);

   TR_CallSite *                   _myCallSite;
   // Target Specific

   TR::ResolvedMethodSymbol *   _calleeSymbol;          //must have this by the time we actually do the inlinining
   TR_ResolvedMethod *          _calleeMethod;
   TR::MethodSymbol::Kinds      _calleeMethodKind;
   TR_VirtualGuardSelection *   _guard;
   int32_t							  _size;
   int32_t                      _weight;
   float                        _callGraphAdjustedWeight;
   TR_OpaqueClassBlock*         _receiverClass;

   float                        _frequencyAdjustment; // may need this for multiple call targets.
   bool                         _alreadyInlined;      // so we don't keep trying to inline the same method repeatedly

   void            setComp(TR::Compilation *comp){ _comp = comp; }
   TR::Compilation *comp()                       { return _comp; }
   TR::ResolvedMethodSymbol *getSymbol()         { return _calleeSymbol; }
   void            setNumber(int32_t num)       { _nodeNumber = num; }
   int32_t         getNumber()                  { return _nodeNumber;}

   void addCallee(TR_CallSite *cs)
      {
      _myCallees.add(cs);
      _numCallees++;
      }

   void removeCallee(TR_CallSite *cs)
      {
      _myCallees.remove(cs);
      _numCallees--;
      _deletedCallees.add(cs);
      _numDeletedCallees++;
      }

   TR_InlineBlocks             *_partialInline;
   TR_LinkHead<TR_CallSite>     _myCallees;
   int32_t                      _numCallees;
   int32_t                      _partialSize;
   int32_t                      _fullSize;
   bool                         _isPartialInliningCandidate;
   TR::Block                    *_originatingBlock;
   TR::CFG                      *_cfg;

   TR_PrexArgInfo              *_prexArgInfo;   // used by computePrexInfo to calculate prex on generatedIL and transform IL
   TR_PrexArgInfo              *_ecsPrexArgInfo; // used by ECS and findInlineTargets to assist in choosing correct inline targets

   void addDeadCallee(TR_CallSite *cs)
      {
      _deletedCallees.add(cs);
      _numDeletedCallees++;
      }

   TR_InlinerFailureReason getCallTargetFailureReason() { return _failureReason; }

   TR_LinkHead<TR_CallSite>     _deletedCallees;
   int32_t                      _numDeletedCallees;
   int32_t                      _nodeNumber;

   TR_InlinerFailureReason      _failureReason;
   TR::Compilation              *_comp;

   };

#define TR_CALLSITE_DEFAULT_ALLOC \
   TR_ALLOC(TR_Memory::Inliner);

#define TR_CALLSITE_INHERIT_CONSTRUCTOR_COMMON(EXTENDED,BASE) \
   EXTENDED (TR_ResolvedMethod *callerResolvedMethod,  \
                  TR::TreeTop *callNodeTreeTop,  \
                  TR::Node *parent,  \
                  TR::Node *callNode,  \
                  TR::Method * interfaceMethod,  \
                  TR_OpaqueClassBlock *receiverClass,  \
                  int32_t vftSlot,  \
                  int32_t cpIndex,  \
                  TR_ResolvedMethod *initialCalleeMethod,  \
                  TR::ResolvedMethodSymbol * initialCalleeSymbol,  \
                  bool isIndirectCall,  \
                  bool isInterface,  \
                  TR_ByteCodeInfo & bcInfo, \
                  TR::Compilation *comp, \
                  int32_t depth=-1, \
                  bool allConsts = false) :  \
                     BASE (callerResolvedMethod, \
                                 callNodeTreeTop, \
                                 parent, \
                                 callNode, \
                                 interfaceMethod, \
                                 receiverClass, \
                                 vftSlot, \
                                 cpIndex, \
                                 initialCalleeMethod, \
                                 initialCalleeSymbol, \
                                 isIndirectCall, \
                                 isInterface, \
                                 bcInfo, \
                                 comp, \
                                 depth, \
                                 allConsts)

#define TR_CALLSITE_EMPTY_CONSTRUCTOR_BODY { };

/*
 * \def TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR (EXTENDED,BASE)
 *    Declare the allocation and a constructor with an empty method body.
 *    Used by call sites constructors that initialize given fields and do nothing else.
 */
#define TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR(EXTENDED,BASE) \
   TR_CALLSITE_DEFAULT_ALLOC \
   TR_CALLSITE_INHERIT_CONSTRUCTOR_COMMON (EXTENDED,BASE) \
   TR_CALLSITE_EMPTY_CONSTRUCTOR_BODY

/*
 * \def TR_CALLSITE_TR_ALLOC_AND_INHERIT_CONSTRUCTOR(EXTENDED,BASE)
 *    Declare the allocation and a constructor WITHOUT a method body.
 *    Used by call sites constructors that do more complicated things other than just initializing given field.
 */
#define TR_CALLSITE_TR_ALLOC_AND_INHERIT_CONSTRUCTOR(EXTENDED,BASE) \
   TR_CALLSITE_DEFAULT_ALLOC \
   TR_CALLSITE_INHERIT_CONSTRUCTOR_COMMON (EXTENDED,BASE)

struct TR_CallSite : public TR_Link<TR_CallSite>
   {
      TR_ALLOC(TR_Memory::Inliner);
      friend class TR_InlinerTracer;

      TR_CallSite(TR_ResolvedMethod *callerResolvedMethod,
                  TR::TreeTop *callNodeTreeTop,
                  TR::Node *parent,
                  TR::Node *callNode,
                  TR::Method * interfaceMethod,
                  TR_OpaqueClassBlock *receiverClass,
                  int32_t vftSlot,
                  int32_t cpIndex,
                  TR_ResolvedMethod *initialCalleeMethod,
                  TR::ResolvedMethodSymbol * initialCalleeSymbol,
                  bool isIndirectCall,
                  bool isInterface,
                  TR_ByteCodeInfo & bcInfo,
                  TR::Compilation *comp,
                  int32_t depth=-1,
                  bool allConsts = false);

      TR_CallSite(const TR_CallSite& other) :
         _allConsts (other._allConsts),
         _bcInfo (other._bcInfo),
         _byteCodeIndex (other._byteCodeIndex),
         _callerBlock (other._callerBlock),
         _callerResolvedMethod (other._callerResolvedMethod),
         _callNode (other._callNode),
         _callNodeTreeTop (other._callNodeTreeTop),
         _cursorTreeTop (other._cursorTreeTop),
         _comp (other._comp),
         _cpIndex (other._cpIndex),
         _depth (other._depth),
         _ecsPrexArgInfo (other._ecsPrexArgInfo),
         _failureReason (other._failureReason),
         _forceInline (other._forceInline),
         _initialCalleeMethod (other._initialCalleeMethod),
         _initialCalleeSymbol (other._initialCalleeSymbol),
         _interfaceMethod (other._interfaceMethod),
         _isBackEdge (other._isBackEdge),
         _isIndirectCall (other._isIndirectCall),
         _isInterface (other._isInterface),
         _mytargets (0, other._comp->allocator()),
         _myRemovedTargets(0, other._comp->allocator()),
         _parent (other._parent),
         _receiverClass (other._receiverClass),
         _stmtNo (other._stmtNo),
         _unavailableTemps (other._comp->trMemory()),
         _unavailableBlockTemps (other._comp->trMemory()),
         _vftSlot (other._vftSlot),
         _visitCount (other._visitCount)
         {
         }

      TR_InlinerFailureReason getCallSiteFailureReason() { return _failureReason; }
      //Call Site Specific

      bool                    isBackEdge()               { return _isBackEdge; }
      void                    setIsBackEdge()            {  _isBackEdge = true; }
      bool                    isIndirectCall()           { return _isIndirectCall; }
      bool                    isInterface()              { return _isInterface; }
      int32_t                 numTargets()               { return _mytargets.size(); }
      int32_t                 numRemovedTargets()        { return _myRemovedTargets.size(); }

      bool                    isForceInline()            { return _forceInline; }

      void                    tagcalltarget(int32_t index, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason );
      void                    tagcalltarget(TR_CallTarget *ct, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason);
      void                    removecalltarget(int32_t index, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason );
      void                    removecalltarget(TR_CallTarget *ct, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason);
      void                    removeAllTargets(TR_InlinerTracer *tracer, TR_InlinerFailureReason reason);
      void                    removeTargets(TR_InlinerTracer *tracer, int index, TR_InlinerFailureReason reason);
      TR_CallTarget *         getTarget(int32_t i)
         {
         TR_ASSERT(i>=0 && i<_mytargets.size(), "indexing a target out of range.");
         return _mytargets[i];
         }

     void                     setTarget(int32_t i, TR_CallTarget * ct)
         {
         TR_ASSERT(i>=0 && i<_mytargets.size(), "indexing a target out of range.");
         _mytargets[i] = ct;
         }

      TR_CallTarget *         addTarget(TR_Memory* , TR_InlinerBase*, TR_VirtualGuardSelection *, TR_ResolvedMethod *, TR_OpaqueClassBlock *,TR_AllocationKind allocKind=stackAlloc,float ratio=1.0);
      bool                    addTarget0(TR_Memory*, TR_InlinerTracer *, TR_VirtualGuardSelection *, TR_ResolvedMethod *, TR_OpaqueClassBlock *,TR_AllocationKind allocKind=stackAlloc,float ratio=1.0);
      void                    addTarget(TR_CallTarget *target)
         {
         _mytargets.push_back(target);
         }
      void                    addTargetToFront(TR_CallTarget *target)
         {
         _mytargets.push_front(target);
         }

      TR_CallTarget *         getRemovedTarget(int32_t i)
         {
         TR_ASSERT( i >= 0 && i<_myRemovedTargets.size(), "indexing a target out of range.");
         return _myRemovedTargets[i];
         }

      const char*             signature(TR_Memory *trMemory);

      TR_OpaqueClassBlock    *calleeClass();

      TR::Compilation *             _comp;
      TR_ResolvedMethod *          _callerResolvedMethod;
      TR::TreeTop *                 _callNodeTreeTop;
      TR::TreeTop *                 _cursorTreeTop;
      TR::Node *                    _parent;    /* tree top node of the call site callNode */
      TR::Node *                    _callNode;

      // Initial Information We Need to Calculate a CallTarget
      TR::Method *                 _interfaceMethod;       // If we have an interface, we'll only have a TR::Method until we determine others
      TR_OpaqueClassBlock *        _receiverClass;         // for interface calls, we might know this?
      int32_t                      _vftSlot;               //
      int32_t                      _cpIndex;               //
      TR_ResolvedMethod *          _initialCalleeMethod;    // alot of times we might already know the resolved method.
      TR::ResolvedMethodSymbol *    _initialCalleeSymbol;   // and in those cases, we should also know the methodsymbol..
      TR_ByteCodeInfo             _bcInfo;
      int32_t                     _stmtNo;
      // except we don't always have a call node! (ie when we come from ECS)
      int32_t                      _byteCodeIndex;

      bool                         _isIndirectCall;
      bool                         _isInterface;
      bool                         _forceInline;
      bool                         _isBackEdge;

      TR::Block *                   _callerBlock;

      vcount_t                      _visitCount;    // a woefully bad choice of name.  The number of times we have visited this callsite during inlining.  In theory, when you have multiple targets for a callsite, you will have multiple visits to a callsite.
      List<TR::SymbolReference>     _unavailableTemps;
      List<TR::SymbolReference>     _unavailableBlockTemps;

      TR_InlinerFailureReason      _failureReason;                //used for inliner report!

                                                     // we propagate from calltarget to callee callsite when appropriate in ecs
      TR_PrexArgInfo              *_ecsPrexArgInfo; // used by ECS and findInlineTargets to assist in choosing correct inline targets

      //new findInlineTargets API
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_CallSite"; }

      static TR_CallSite*
      create(TR::TreeTop* callNodeTreeTop,
                                 TR::Node *parent,
                                 TR::Node* callNode,
                                 TR_OpaqueClassBlock *receiverClass,
                                 TR::SymbolReference *symRef,
                                 TR_ResolvedMethod *resolvedMethod,
                                 TR::Compilation* comp,
                                 TR_Memory* trMemory,
                                 TR_AllocationKind kind,
                                 TR_ResolvedMethod* caller = NULL,
                                 int32_t depth=-1,
                                 bool allConsts = false);

   protected:
      TR::Compilation* comp()              {return _comp; }
      TR_FrontEnd* fe()                   {return _comp->fe(); }

   private:
      TR::deque<TR_CallTarget *> _mytargets;
      TR::deque<TR_CallTarget *> _myRemovedTargets;

      //Partial Inlining Stuff
   public:
      void                    setDepth(int32_t d) { _depth = d; }
      int32_t                 getDepth()          { return _depth; }

      bool _allConsts;
   private:
      int32_t                      _depth;
   };

class TR_DirectCallSite : public TR_CallSite
   {
   public:
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR(TR_DirectCallSite, TR_CallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
      virtual const char*  name () { return "TR_DirectCallSite"; }
   };

class TR_IndirectCallSite : public TR_CallSite
   {

   public:
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR(TR_IndirectCallSite, TR_CallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_IndirectCallSite"; }
      virtual TR_ResolvedMethod* findSingleJittedImplementer (TR_InlinerBase* inliner);

   protected:
      bool hasFixedTypeArgInfo();
      bool hasResolvedTypeArgInfo();
      TR_OpaqueClassBlock* getClassFromArgInfo();
      bool tryToRefineReceiverClassBasedOnResolvedTypeArgInfo(TR_InlinerBase* inliner);

      virtual bool findCallTargetUsingArgumentPreexistence(TR_InlinerBase* inliner);
      virtual TR_ResolvedMethod* getResolvedMethod (TR_OpaqueClassBlock* klass);
      TR_OpaqueClassBlock * extractAndLogClassArgument(TR_InlinerBase* inliner);
		//capabilities
		bool addTargetIfMethodIsNotOverriden (TR_InlinerBase* inliner);
		bool addTargetIfMethodIsNotOverridenInReceiversHierarchy (TR_InlinerBase* inliner);
		bool addTargetIfThereIsSingleImplementer(TR_InlinerBase* inliner);

      virtual TR_OpaqueClassBlock* getClassFromMethod ()
         {
         TR_ASSERT(0, "TR_CallSite::getClassFromMethod is not implemented");
         return NULL;
         }

   };

class TR_FunctionPointerCallSite : public  TR_IndirectCallSite
   {
   protected :
      TR_CALLSITE_TR_ALLOC_AND_INHERIT_EMPTY_CONSTRUCTOR(TR_FunctionPointerCallSite, TR_IndirectCallSite)
   };

#endif
