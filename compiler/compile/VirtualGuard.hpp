/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef OMR_VIRTUALGUARD_INCL
#define OMR_VIRTUALGUARD_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/KnownObjectTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"
#include "infra/List.hpp"

class TR_InnerAssumption;
class TR_OpaqueClassBlock;
class TR_VirtualGuardSite;
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

enum TR_VirtualGuardKind
   {
   TR_NoGuard,
   TR_ProfiledGuard,
   TR_RemovedProfiledGuard,
   TR_InterfaceGuard,
   TR_RemovedInterfaceGuard,
   TR_AbstractGuard,
   TR_HierarchyGuard,
   TR_NonoverriddenGuard,
   TR_RemovedNonoverriddenGuard,
   TR_SideEffectGuard,
   TR_DummyGuard,
   TR_HCRGuard,
   TR_MutableCallSiteTargetGuard,
   TR_MethodEnterExitGuard,
   TR_DirectMethodGuard,
   TR_InnerGuard,
   TR_ArrayStoreCheckGuard,
   TR_OSRGuard,
   TR_BreakpointGuard
   };

enum TR_VirtualGuardTestType
   {
   TR_DummyTest,
   TR_VftTest,
   TR_MethodTest,
   TR_NonoverriddenTest,
   TR_FSDTest /**< used in debugging mode to test if a breakpoint is set for the inlined callee */
   };



class TR_VirtualGuard
   {
   public:

   TR_ALLOC(TR_Memory::VirtualGuard)

   TR_VirtualGuard(
         TR_VirtualGuardTestType test,
         TR_VirtualGuardKind kind,
         TR::Compilation *comp,
         TR::Node *callNode,
         TR::Node *guardNode,
         int16_t calleeIndex,
         int32_t currentSiteIndex,
         TR_OpaqueClassBlock *thisClass = 0);

   // Used for guarded devirtualizations
   TR_VirtualGuard(
         TR_VirtualGuardTestType test,
         TR_VirtualGuardKind kind,
         TR::Compilation *comp,
         TR::Node *callNode,
         TR::Node *guardNode=NULL,
         int32_t currentSiteIndex=-2);

   static TR::Node *createVftGuard(
         TR_VirtualGuardKind,
         TR::Compilation *,
         int16_t calleeIndex,
         TR::Node *,
         TR::TreeTop *,
         TR_OpaqueClassBlock *);

   static TR::Node *createVftGuardWithReceiver(
         TR_VirtualGuardKind,
         TR::Compilation *,
         int16_t calleeIndex,
         TR::Node *,
         TR::TreeTop *,
         TR_OpaqueClassBlock *,
         TR::Node* receiverNode);

   static TR::Node *createMethodGuard(
         TR_VirtualGuardKind,
         TR::Compilation *,
         int16_t calleeIndex,
         TR::Node *,
         TR::TreeTop *,
         TR::ResolvedMethodSymbol *,
         TR_OpaqueClassBlock *thisClass);

   static TR::Node *createMethodGuardWithReceiver(
         TR_VirtualGuardKind,
         TR::Compilation *,
         int16_t calleeIndex,
         TR::Node *,
         TR::TreeTop *,
         TR::ResolvedMethodSymbol *,
         TR_OpaqueClassBlock *thisClass,
         TR::Node* receiverNode);

   static TR::Node *createNonoverriddenGuard(
         TR_VirtualGuardKind,
         TR::Compilation *,
         int16_t calleeIndex,
         TR::Node *,
         TR::TreeTop *,
         TR::ResolvedMethodSymbol *,
         bool forInling = true);

   static TR::Node *createMutableCallSiteTargetGuard(
         TR::Compilation * comp,
         int16_t calleeIndex,
         TR::Node * node,
         TR::TreeTop * destination,
         uintptr_t *mcsObject,
         TR::KnownObjectTable::Index mcsEpoch);

   static TR::Node *createDummyOrSideEffectGuard(TR::Compilation *, TR::Node *, TR::TreeTop *);

   static TR::Node *createSideEffectGuard(TR::Compilation *, TR::Node *, TR::TreeTop *);

   static TR::Node *createAOTInliningGuard(TR::Compilation *, int16_t, TR::Node *, TR::TreeTop *, TR_VirtualGuardKind);

   static TR::Node *createAOTGuard(TR::Compilation *, int16_t, TR::Node *, TR::TreeTop *, TR_VirtualGuardKind);

   static TR::Node *createDummyGuard(TR::Compilation *, int16_t, TR::Node *, TR::TreeTop *);

   static TR_VirtualGuard *createGuardedDevirtualizationGuard(TR_VirtualGuardKind, TR::Compilation *, TR::Node *callNode);

   static TR_VirtualGuard *createArrayStoreCheckGuard(TR::Compilation *, TR::Node *node, TR_OpaqueClassBlock *clazz);

   static TR::Node *createOSRGuard(TR::Compilation *, TR::TreeTop *destination);

   static TR::Node *createHCRGuard(
         TR::Compilation *comp,
         int16_t calleeIndex,
         TR::Node *node,
         TR::TreeTop *destination,
         TR::ResolvedMethodSymbol * symbol,
         TR_OpaqueClassBlock *thisClass);

   static TR::Node *createBreakpointGuard(TR::Compilation * comp, int16_t calleeIndex, TR::Node* callNode, TR::TreeTop * destination, TR::ResolvedMethodSymbol * calleeSymbol);
   static TR::Node *createBreakpointGuardNode(TR::Compilation * comp, int16_t calleeIndex, TR::Node* callNode, TR::TreeTop * destination, TR::ResolvedMethodSymbol * calleeSymbol);

   static void setGuardKind(TR::Node *guard, TR_VirtualGuardKind kind, TR::Compilation * comp);

   bool isNopable()
      {
      switch (_kind)
         {
         case TR_ProfiledGuard:
            return false;
         default:
            return true;
         }
      }

   bool mustBeNoped()
      {
      switch (_kind)
         {
         case TR_MutableCallSiteTargetGuard:
         case TR_InterfaceGuard:
            return true;
         default:
            return false;
         }
      }

   bool indirectsTheThisPointer()
      {
      if (_kind == TR_HCRGuard)
         return false;

      switch (_test)
         {
         case TR_DummyTest:
         case TR_NonoverriddenTest:
         case TR_FSDTest:
            return false;
         default:
            return true;
         }
      }

   int16_t                 getCalleeIndex() { return _calleeIndex; }
   int32_t                 getByteCodeIndex() { return _byteCodeIndex; }
   TR_VirtualGuardTestType getTestType()    { return _test; }
   TR_VirtualGuardKind     getKind()        { return _kind; }
   TR_OpaqueClassBlock    *getThisClass()   { return _thisClass; }
   void                    setThisClass(TR_OpaqueClassBlock *thisClass)   { _thisClass = thisClass; }

   uintptr_t *mutableCallSiteObject()
      {
      TR_ASSERT(_kind == TR_MutableCallSiteTargetGuard, "mutableCallSiteObject only defined for TR_MutableCallSiteTargetGuard");
      return _mutableCallSiteObject;
      }

   TR::KnownObjectTable::Index mutableCallSiteEpoch()
      {
      TR_ASSERT(_kind == TR_MutableCallSiteTargetGuard, "mutableCallSiteEpoch only defined for TR_MutableCallSiteTargetGuard");
      return _mutableCallSiteEpoch;
      }

   bool                    isInlineGuard()  { return _callNode == NULL; }
   TR::Node                *getCallNode()    { TR_ASSERT(!isInlineGuard(), "assertion failure"); return _callNode; }
   TR_ByteCodeInfo         &getByteCodeInfo() { return _bcInfo; }
   void                    setCannotBeRemoved() { _cannotBeRemoved = true; }
   bool                    canBeRemoved(bool ignoreMerged=false)   { return ignoreMerged ? !_cannotBeRemoved : !(_cannotBeRemoved || _mergedWithHCRGuard || _mergedWithOSRGuard); }

#ifdef J9_PROJECT_SPECIFIC
   TR_VirtualGuardSite *addNOPSite();
   List<TR_VirtualGuardSite> &getNOPSites()   { return _sites; }
#endif

   TR::SymbolReference *getSymbolReference() { return _guardedMethod; }
   void setSymbolReference(TR::SymbolReference* sr) { _guardedMethod = sr; }

   void addInnerAssumption(TR_InnerAssumption *a);
   void addInnerAssumption(TR::Compilation *comp, int32_t ordinal, TR_VirtualGuard *guard);
   List<TR_InnerAssumption> &getInnerAssumptions() { return _innerAssumptions; }

   bool                    shouldGenerateChildrenCode() { return _evalChildren; }
   void                    dontGenerateChildrenCode()   { _evalChildren = false; }
   void                    setMergedWithHCRGuard(bool merged=true) { _mergedWithHCRGuard=merged; }
   bool                    mergedWithHCRGuard() { return _mergedWithHCRGuard; }

   void                    setMergedWithOSRGuard(bool merged=true) { _mergedWithOSRGuard=merged; }
   bool                    mergedWithOSRGuard() { return _mergedWithOSRGuard; }

   int32_t                 getCurrentInlinedSiteIndex() { return _currentInlinedSiteIndex; }
   void                    setCurrentInlinedSiteIndex(int32_t index) { _currentInlinedSiteIndex = index; }

   TR::Node *getGuardNode() { return _guardNode; }
   void setGuardNode(TR::Node *guardNode) { _guardNode = guardNode; }

   private:

#ifdef J9_PROJECT_SPECIFIC
   List<TR_VirtualGuardSite> _sites;
#endif
   TR_VirtualGuardTestType   _test;
   TR_VirtualGuardKind       _kind;
   int16_t                   _calleeIndex;
   int32_t                   _byteCodeIndex;
   TR::SymbolReference       *_guardedMethod;

   // Non-null for guarded-devirtualizations only
   TR::Node                  *_callNode;

   // used for AOT
   TR::Node                  *_guardNode;
   int32_t                   _currentInlinedSiteIndex;

   // used for Interface/Method, Abstract/Method, Hierarchy/Method
   TR_OpaqueClassBlock      *_thisClass;

   bool                      _cannotBeRemoved;
   List<TR_InnerAssumption>  _innerAssumptions;
   bool                      _evalChildren;
   bool                      _mergedWithHCRGuard;
   bool                      _mergedWithOSRGuard;

   // These reference locations are non-null only for MutableCallSiteGuards
   uintptr_t                *_mutableCallSiteObject;
   TR::KnownObjectTable::Index _mutableCallSiteEpoch;
   TR_ByteCodeInfo           _bcInfo;
   };

#endif
