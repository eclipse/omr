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

#ifndef OMR_PREEXISTENCE_INCL
#define OMR_PREEXISTENCE_INCL

#include <stdint.h>
#include <string.h>
#include "env/KnownObjectTable.hpp"
#include "env/TRMemory.hpp"
#include "ras/LogTracer.hpp"

// Temporary macro to coordinate changes between omr and openj9.
#define TR_PREXARGINFO_TRACER_CLASS TR_LogTracer

class TR_CallSite;
class TR_InlinerTracer;
class TR_OpaqueClassBlock;
class TR_VirtualGuard;
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }

enum PrexKnowledgeLevel { NONE, PREEXISTENT, FIXED_CLASS, KNOWN_OBJECT };

class TR_PrexArgument
   {
   friend class TR_DebugExt;

   public:

   TR_ALLOC(TR_Memory::LocalOpts);

   enum ClassKind { ClassIsUnknown, ClassIsFixed, ClassIsPreexistent };

   TR_PrexArgument(
         ClassKind classKind,
         TR_OpaqueClassBlock *clazz = 0,
         TR_OpaqueClassBlock *profiledClazz = 0):
      _classKind(classKind),
      _class(clazz),
      _profiledClazz(profiledClazz),
      _knownObjectIndex(TR::KnownObjectTable::UNKNOWN),
      _isTypeInfoForInlinedBody(false)
      { TR_ASSERT_FATAL(_classKind != ClassIsFixed || _class, "Fixed type must have a class"); }

   static const char *priorKnowledgeStrings[];
   static PrexKnowledgeLevel knowledgeLevel(TR_PrexArgument *pa);

   TR_PrexArgument(TR::KnownObjectTable::Index knownObjectIndex, TR::Compilation *comp);

   bool classIsFixed() { return _classKind == ClassIsFixed; }

   void setClassIsFixed(TR_OpaqueClassBlock *fixedClass=0, TR_OpaqueClassBlock *profiledClazz=0)
      {
      _classKind     = ClassIsFixed;
      _class    = fixedClass;
      _profiledClazz = profiledClazz;
      }

   bool classIsPreexistent() { return _classKind == ClassIsPreexistent; }

   bool usedProfiledInfo() { return _profiledClazz != NULL; }

   TR_OpaqueClassBlock *getClass() { return _class;    }
   TR_OpaqueClassBlock *getFixedProfiledClass() { return _profiledClazz; }

   TR::KnownObjectTable::Index getKnownObjectIndex() { return _knownObjectIndex; }
   bool hasKnownObjectIndex(){ return getKnownObjectIndex() != TR::KnownObjectTable::UNKNOWN; }

   bool isTypeInfoForInlinedBody () { return _isTypeInfoForInlinedBody; }
   void setTypeInfoForInlinedBody() { _isTypeInfoForInlinedBody = true; }

   private:

   ClassKind _classKind;

   // optionally provided - when ClassIsFixed and the type is known to be
   // more specialized and different from the declared type
   //
   TR_OpaqueClassBlock  *_class;
   TR_OpaqueClassBlock  *_profiledClazz;

   // optionally provided - when ObjectIsKnown
   //
   TR::KnownObjectTable::Index _knownObjectIndex;
   bool _isTypeInfoForInlinedBody; // The prex arg info only apply to the inlined body but not on the taken side
   };

class TR_PrexArgInfo
   {
   public:

#ifdef J9_PROJECT_SPECIFIC
   static TR_PrexArgInfo* enhance(TR_PrexArgInfo *dest, TR_PrexArgInfo *source, TR::Compilation *comp);

   static void propagateReceiverInfoIfAvailable (TR::ResolvedMethodSymbol* methodSymbol, TR_CallSite* callsite,
                                              TR_PrexArgInfo* argInfo, TR_LogTracer* tracer);

   static void propagateArgsFromCaller(TR::ResolvedMethodSymbol* methodSymbol, TR_CallSite* callsite,
      TR_PrexArgInfo* argInfo, TR_LogTracer* tracer);

   static bool validateAndPropagateArgsFromCalleeSymbol(TR_PrexArgInfo* argsFromSymbol, TR_PrexArgInfo* argsFromTarget, TR_LogTracer* tracer);

   static TR_PrexArgInfo* buildPrexArgInfoForMethodSymbol(TR::ResolvedMethodSymbol* methodSymbol, TR_LogTracer* tracer);
   void clearArgInfoForNonInvariantArguments(TR::ResolvedMethodSymbol* methodSymbol, TR_LogTracer* tracer);
   /**
    * \brief
    *    Get arg info for arguments of callNode that are parameters of the caller
    *
    * \return
    *    TR_PrexArgInfo contianing arg info for arguments from caller parameters
    */
   static TR_PrexArgInfo* argInfoFromCaller(TR::Node* callNode, TR_PrexArgInfo* callerArgInfo);
#endif

   TR_ALLOC(TR_Memory::LocalOpts);
   TR_PrexArgInfo(int32_t numArgs, TR_Memory * m) : _numArgs(numArgs)
      {
      _args = (TR_PrexArgument **) m->allocateHeapMemory(sizeof(TR_PrexArgument*) * numArgs);
      memset(_args, 0, sizeof(TR_PrexArgument*) * numArgs);
      }

   void set(int32_t index, TR_PrexArgument *info) { _args[index] = info; }
   TR_PrexArgument *get(int32_t index) { return _args[index]; }

   int32_t getNumArgs() { return _numArgs; }

   void dumpTrace();

   private:

   int32_t _numArgs;
   TR_PrexArgument **_args;
   //
#ifdef J9_PROJECT_SPECIFIC
   static TR::Node* getCallNode (TR::ResolvedMethodSymbol* methodSymbol, class TR_CallSite* callsite, class TR_LogTracer* tracer);
   static bool hasArgInfoForChild (TR::Node *child, TR_PrexArgInfo * argInfo);
   static TR_PrexArgument* getArgForChild(TR::Node *child, TR_PrexArgInfo* argInfo);
#endif
   };


class TR_InnerAssumption
   {
   public:

   TR_ALLOC(TR_Memory::VirtualGuard)
      ;

   TR_InnerAssumption(int32_t ordinal, TR_VirtualGuard *guard) :
      _ordinal(ordinal),
      _guard(guard) {}

   int32_t _ordinal;
   TR_VirtualGuard *_guard;
   };

#endif
