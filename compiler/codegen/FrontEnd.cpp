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

#include "codegen/FrontEnd.hpp"

#include <stdarg.h>                            // for va_end, va_list, etc
#include <stdint.h>                            // for int32_t
#include "env/KnownObjectTable.hpp"        // for KnownObjectTable, etc
#include "compile/Compilation.hpp"             // for Compilation
#include "compile/CompilationTypes.hpp"        // for TR_CallingContext
#include "compile/Method.hpp"                  // for TR_Method
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"                         // for Cpu
#include "env/jittypes.h"                      // for uintptrj_t, etc
#include "env/PersistentInfo.hpp"              // for PersistentInfo

#if defined(LINUX) || defined(OSX)
#include <unistd.h>                 // for getpid, intptr_t, pid_t
#endif


#define notImplemented(A) TR_ASSERT(0, "TR_FrontEnd::%s is undefined", (A) )


TR_ResolvedMethod *
TR_FrontEnd::createResolvedMethod(TR_Memory *, TR_OpaqueMethodBlock *, TR_ResolvedMethod *, TR_OpaqueClassBlock *)
   {
   notImplemented("createResolvedMethod");
   return 0;
   }

uint32_t
TR_FrontEnd::offsetOfIsOverriddenBit()
   {
   notImplemented("offsetOfIsOverriddenBit");
   return 0;
   }

TR_Debug *
TR_FrontEnd::createDebug(TR::Compilation *comp)
   {
   notImplemented("createDebug");
   return 0;
   }

void
TR_FrontEnd::acquireLogMonitor()
   {
   notImplemented("acquireLogMonitor");
   }

void
TR_FrontEnd::releaseLogMonitor()
   {
   notImplemented("releaseLogMonitor");
   }

bool
TR_FrontEnd::classHasBeenExtended(TR_OpaqueClassBlock *)
   {
   notImplemented("classHasBeenExtended");
   return false;
   }

bool
TR_FrontEnd::classHasBeenReplaced(TR_OpaqueClassBlock *)
   {
   notImplemented("classHasBeenReplaced");
   return false;
   }

uint8_t *
TR_FrontEnd::allocateCodeMemory(TR::Compilation *, uint32_t warmCodeSize, uint32_t coldCodeSize, uint8_t ** coldCode, bool isMethodHeaderNeeded)
   {
   notImplemented("allocateCodeMemory");
   return 0;
   }

void
TR_FrontEnd::resizeCodeMemory(TR::Compilation *, uint8_t *, uint32_t numBytes)
   {
   notImplemented("resizeCodeMemory");
   }

/*
 * Return conservative approximation of code-cache base.
 */
uint8_t * TR_FrontEnd::getCodeCacheBase()                          { return 0; }
uint8_t * TR_FrontEnd::getCodeCacheBase(TR::CodeCache *codeCache)  { return 0; }

/*
 * Return conservative approximation of code cache top.
 */
uint8_t * TR_FrontEnd::getCodeCacheTop()                           { return (uint8_t*)(~0); }
uint8_t * TR_FrontEnd::getCodeCacheTop(TR::CodeCache *codeCache)   { return (uint8_t*)(~0); }

void
TR_FrontEnd::releaseCodeMemory(void *, uint8_t)
   {
   notImplemented("releaseCodeMemory");
   }

uint8_t *
TR_FrontEnd::allocateRelocationData(TR::Compilation * comp, uint32_t numBytes)
   {
   notImplemented("allocateRelocationData");
   return 0;
   }

bool
TR_FrontEnd::isMethodEnterTracingEnabled(TR_OpaqueMethodBlock *method)
   {
   return false;
   }

bool
TR_FrontEnd::isMethodExitTracingEnabled(TR_OpaqueMethodBlock *method)
   {
   return false;
   }

int32_t
TR_FrontEnd::getArraySpineShift(int32_t)
   {
   notImplemented("getArraySpineShift");
   return 0;
   }

int32_t
TR_FrontEnd::getArrayletMask(int32_t)
   {
   notImplemented("getArrayletMask");
   return 0;
   }

int32_t
TR_FrontEnd::getArrayletLeafIndex(int32_t, int32_t)
   {
   notImplemented("getArrayletLeafIndex");
   return 0;
   }


uintptrj_t TR_FrontEnd::getObjectHeaderSizeInBytes()              { notImplemented("getObjectHeaderSizeInBytes"); return 0; }
uintptrj_t TR_FrontEnd::getOffsetOfContiguousArraySizeField()     { notImplemented("getOffsetOfContiguousArraySizeField"); return 0; }
uintptrj_t TR_FrontEnd::getOffsetOfDiscontiguousArraySizeField()  { notImplemented("getOffsetOfDiscontiguousArraySizeField"); return 0; }

uintptrj_t TR_FrontEnd::getOffsetOfIndexableSizeField()           { notImplemented("getOffsetOfIndexableSizeField"); return 0; }


int32_t
TR_FrontEnd::getObjectAlignmentInBytes()
   {
   notImplemented("getObjectAlignmentInBytes");
   return 0;
   }


char *
TR_FrontEnd::getFormattedName(
      char *buf,
      int32_t bufLength,
      char *name,
      char *format,
      bool suffix)
   {

#if defined(LINUX) || defined(OSX)
   // FIXME: TODO: This is a temporary implementation -- we ignore the suffix format and
   // use the pid only

   if(suffix)
      {
      pid_t pid = getpid();
      snprintf(buf, bufLength, "%s-%d", name, pid);

      // FIXME: proper error handling for snprintf
      return buf;
      }

#endif

   return strncpy(buf, name, bufLength);

   }

uintptrj_t
TR_FrontEnd::getClassDepthAndFlagsValue(TR_OpaqueClassBlock * classPointer)
   {
   notImplemented("getClassDepthAndFlagsValue");
   return 0;
   }

TR_OpaqueMethodBlock*
TR_FrontEnd::getMethodFromName(char * className, char *methodName, char *signature, TR_OpaqueMethodBlock *callingMethod)
   {
   notImplemented("getMethodFromName");
   return 0;
   }


TR_OpaqueClassBlock *
TR_FrontEnd::getClassOfMethod(TR_OpaqueMethodBlock *method)
   {
   notImplemented("getClassOfMethod");
   return 0;
   }


TR_OpaqueClassBlock *
TR_FrontEnd::getComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass)
   {
   notImplemented("getComponentClassFromArrayClass");
   return 0;
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getArrayClassFromComponentClass(TR_OpaqueClassBlock * componentClass)
   {
   notImplemented("getArrayClassFromComponentClass");
   return 0;
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass)
   {
   notImplemented("getLeafComponentClassFromArrayClass");
   return 0;
   }

int32_t
TR_FrontEnd::getNewArrayTypeFromClass(TR_OpaqueClassBlock *clazz)
   {
   notImplemented("getNewArrayTypeFromClass");
   return 0;
   }

/**
 * Return the class pointer for an array type.
 *
 * This query may return null if the type cannot be determined.
 */
TR_OpaqueClassBlock *
TR_FrontEnd::getClassFromNewArrayType(int32_t arrayType)
   {
   return 0;
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getClassFromSignature(const char * sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT)
   {
   notImplemented("getClassFromSignature");
   return 0;
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT)
   {
   notImplemented("getClassFromSignature");
   return 0;
   }


TR_YesNoMaybe
TR_FrontEnd::isInstanceOf(TR_OpaqueClassBlock *instanceClass, TR_OpaqueClassBlock * castClass, bool instanceIsFixed, bool castIsFixed, bool optimizeForAOT)
   {
   notImplemented("isInstanceOf");
   return TR_maybe;
   }


TR_OpaqueClassBlock *
TR_FrontEnd::getSuperClass(TR_OpaqueClassBlock * classPointer)
   {
   notImplemented("getSuperClass");
   return 0;
   }

bool
TR_FrontEnd::isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *)
   {
   notImplemented("isUnloadAssumptionRequired");
   return true;
   }

const char *
TR_FrontEnd::sampleSignature(TR_OpaqueMethodBlock * aMethod, char * bug, int32_t bufLen, TR_Memory *memory)
   {
   notImplemented("sampleSignature");
   return 0;
   }


int32_t
TR_FrontEnd::getLineNumberForMethodAndByteCodeIndex(TR_OpaqueMethodBlock *, int32_t)
   {
   return -1;
   }


TR_OpaqueClassBlock *
TR_FrontEnd::getClassFromStatic(void *p)
   {
   notImplemented("getClassFromStatic");
   return 0;
   }


TR_OpaqueMethodBlock *
TR_FrontEnd::getInlinedCallSiteMethod(TR_InlinedCallSite *ics)
   {
   return (TR_OpaqueMethodBlock *)(ics->_vmMethodInfo);
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getClassFromMethodBlock(TR_OpaqueMethodBlock *mb)
   {
   notImplemented("getClassFromMethodBlock");
   return NULL;
   }

int32_t
TR_FrontEnd::getStringLength(uintptrj_t objectPointer)
   {
   notImplemented("getStringLength");
   return 0;
   }

intptrj_t
TR_FrontEnd::getStringUTF8Length(uintptrj_t objectPointer)
   {
   notImplemented("getStringUTF8Length");
   return -1;
   }

char *
TR_FrontEnd::getStringUTF8(uintptrj_t objectPointer, char *buffer, intptrj_t bufferSize)
   {
   notImplemented("getStringUTF8");
   return NULL;
   }



TR_OpaqueClassBlock *
TR_FrontEnd::getClassClassPointer(TR_OpaqueClassBlock *objectClassPointer)
   {
   notImplemented("getClassClassPointer");
   return 0;
   }


TR::CodeCache *
TR_FrontEnd::getDesignatedCodeCache(TR::Compilation *comp)
   {
   notImplemented("getDesignatedCodeCache");
   return 0;
   }


void
TR_FrontEnd::reserveTrampolineIfNecessary(TR::Compilation *, TR::SymbolReference *symRef, bool inBinaryEncoding)
   {
   notImplemented("reserveTrampolineIfNecessary");
   }

intptrj_t
TR_FrontEnd::methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference *symRef, void * callSite)
   {
   notImplemented("methodTrampolineLookup");
   return 0;
   }

intptrj_t
TR_FrontEnd::indexedTrampolineLookup(int32_t helperIndex, void * callSite)
   {
   notImplemented("indexedTrampolineLookup");
   return 0;
   }

bool
TR_FrontEnd::needsMethodTrampolines()
   {
   notImplemented("needsMethodTrampolines");
   return 0;
   }


void
TR_FrontEnd::reserveNTrampolines(TR::Compilation *, int32_t n, bool inBinaryEncoding)
   {
   notImplemented("reserveNTrampolines");
   }
