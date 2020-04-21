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

#include "env/FrontEnd.hpp"

#include <stdarg.h>
#include <stdint.h>
#include "env/KnownObjectTable.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "env/jittypes.h"
#include "env/PersistentInfo.hpp"

#if defined(LINUX) || defined(OSX)
#include <unistd.h>
#endif

TR_ResolvedMethod *
TR_FrontEnd::createResolvedMethod(TR_Memory *, TR_OpaqueMethodBlock *, TR_ResolvedMethod *, TR_OpaqueClassBlock *)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

uint32_t
TR_FrontEnd::offsetOfIsOverriddenBit()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR_Debug *
TR_FrontEnd::createDebug(TR::Compilation *comp)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

void
TR_FrontEnd::acquireLogMonitor()
   {
   TR_UNIMPLEMENTED();
   }

void
TR_FrontEnd::releaseLogMonitor()
   {
   TR_UNIMPLEMENTED();
   }

bool
TR_FrontEnd::classHasBeenExtended(TR_OpaqueClassBlock *)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

bool
TR_FrontEnd::classHasBeenReplaced(TR_OpaqueClassBlock *)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

uint8_t *
TR_FrontEnd::allocateRelocationData(TR::Compilation * comp, uint32_t numBytes)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

bool
TR_FrontEnd::isMethodTracingEnabled(TR_OpaqueMethodBlock *method)
   {
   return false;
   }

int32_t
TR_FrontEnd::getArraySpineShift(int32_t)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

int32_t
TR_FrontEnd::getArrayletMask(int32_t)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

int32_t
TR_FrontEnd::getArrayletLeafIndex(int32_t, int32_t)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


uintptr_t TR_FrontEnd::getObjectHeaderSizeInBytes()              { TR_UNIMPLEMENTED(); return 0; }
uintptr_t TR_FrontEnd::getOffsetOfContiguousArraySizeField()     { TR_UNIMPLEMENTED(); return 0; }
uintptr_t TR_FrontEnd::getOffsetOfDiscontiguousArraySizeField()  { TR_UNIMPLEMENTED(); return 0; }

uintptr_t TR_FrontEnd::getOffsetOfIndexableSizeField()           { TR_UNIMPLEMENTED(); return 0; }


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


TR_OpaqueMethodBlock*
TR_FrontEnd::getMethodFromName(char * className, char *methodName, char *signature)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


TR_OpaqueClassBlock *
TR_FrontEnd::getClassOfMethod(TR_OpaqueMethodBlock *method)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


TR_OpaqueClassBlock *
TR_FrontEnd::getComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getArrayClassFromComponentClass(TR_OpaqueClassBlock * componentClass)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

int32_t
TR_FrontEnd::getNewArrayTypeFromClass(TR_OpaqueClassBlock *clazz)
   {
   TR_UNIMPLEMENTED();
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
   TR_UNIMPLEMENTED();
   return 0;
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


TR_YesNoMaybe
TR_FrontEnd::isInstanceOf(TR_OpaqueClassBlock *instanceClass, TR_OpaqueClassBlock * castClass, bool instanceIsFixed, bool castIsFixed, bool optimizeForAOT)
   {
   TR_UNIMPLEMENTED();
   return TR_maybe;
   }


TR_OpaqueClassBlock *
TR_FrontEnd::getSuperClass(TR_OpaqueClassBlock * classPointer)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

bool
TR_FrontEnd::isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *)
   {
   TR_UNIMPLEMENTED();
   return true;
   }

const char *
TR_FrontEnd::sampleSignature(TR_OpaqueMethodBlock * aMethod, char * bug, int32_t bufLen, TR_Memory *memory)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


int32_t
TR_FrontEnd::getLineNumberForMethodAndByteCodeIndex(TR_OpaqueMethodBlock *, int32_t)
   {
   return -1;
   }

TR_OpaqueMethodBlock *
TR_FrontEnd::getInlinedCallSiteMethod(TR_InlinedCallSite *ics)
   {
   return (TR_OpaqueMethodBlock *)(ics->_vmMethodInfo);
   }

TR_OpaqueClassBlock *
TR_FrontEnd::getClassFromMethodBlock(TR_OpaqueMethodBlock *mb)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

intptr_t
TR_FrontEnd::getStringUTF8Length(uintptr_t objectPointer)
   {
   TR_UNIMPLEMENTED();
   return -1;
   }

char *
TR_FrontEnd::getStringUTF8(uintptr_t objectPointer, char *buffer, intptr_t bufferSize)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }



TR_OpaqueClassBlock *
TR_FrontEnd::getClassClassPointer(TR_OpaqueClassBlock *objectClassPointer)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

void
TR_FrontEnd::reserveTrampolineIfNecessary(TR::Compilation *, TR::SymbolReference *symRef, bool inBinaryEncoding)
   {
   TR_UNIMPLEMENTED();
   }

intptr_t
TR_FrontEnd::methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference *symRef, void * callSite)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }
