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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"
#include "compile/Compilation.hpp"

TR::ClassEnv *
OMR::ClassEnv::self()
   {
   return static_cast<TR::ClassEnv *>(this);
   }

const char *
OMR::ClassEnv::classNameChars(TR::Compilation *comp, TR::SymbolReference *symRef, int32_t &len)
   {
   const char *name = "<no class name>";
   len = static_cast<int32_t>(strlen(name));
   return name;
   }

uintptr_t
OMR::ClassEnv::getArrayElementWidthInBytes(TR::Compilation *comp, TR_OpaqueClassBlock *arrayClass)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

intptr_t
OMR::ClassEnv::getVFTEntry(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, int32_t offset)
   {
   // There is no project-agnostic way to determine whether or not offset is a
   // valid VFT offset for clazz, so return 0 to be safe. If offset were valid,
   // the result would be:
   //
   //    *(intptr_t*) (((uint8_t *)clazz) + offset)
   //
   return 0;
   }

bool
OMR::ClassEnv::classUnloadAssumptionNeedsRelocation(TR::Compilation *comp)
   {
   return comp->compileRelocatableCode();
   }

bool
OMR::ClassEnv::containsZeroOrOneConcreteClass(TR::Compilation *comp, List<TR_PersistentClassInfo>* subClasses)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

char *
OMR::ClassEnv::classNameToSignature(const char *name, int32_t &len, TR::Compilation *comp, TR_AllocationKind allocKind, TR_OpaqueClassBlock *clazz)
   {
   char *sig = (char *)comp->trMemory()->allocateMemory(len+1, allocKind);
   memcpy(sig,name,len);
   sig[len] = '\0';
   return sig;
   }
