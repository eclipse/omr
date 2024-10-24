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

#include "runtime/Runtime.hpp"

TR_RuntimeHelperTable runtimeHelpers;

#if (defined(__IBMCPP__) || defined(__IBMC__) && !defined(MVS)) && !defined(LINUXPPC64) || defined(__open_xl__)
 #if defined(AIXPPC)
  #define JIT_HELPER(x) extern "C" void *x
 #else
  #define JIT_HELPER(x) extern "C" __cdecl x()
 #endif
#else
 #if defined(LINUXPPC64)
  #define JIT_HELPER(x) extern "C" void *x
 #elif defined(LINUX)
  #define JIT_HELPER(x) extern "C" void x()
 #else
  #define JIT_HELPER(x) extern "C" x()
 #endif
#endif


void*
TR_RuntimeHelperTable::translateAddress(void * a)
   {
   return a;
   }

void* TR_RuntimeHelperTable::getFunctionEntryPointOrConst(TR_RuntimeHelper h)
   {
   if (h < TR_numRuntimeHelpers)
      {
      if (_linkage[h] == TR_Helper)
         return translateAddress(_helpers[h]);
      else
         return _helpers[h];
      }
   else
      return reinterpret_cast<void*>(TR_RuntimeHelperTable::INVALID_FUNCTION_POINTER);
   }

void*
TR_RuntimeHelperTable::getFunctionPointer(TR_RuntimeHelper h)
   {
   if ((h < TR_numRuntimeHelpers) && (_linkage[h] == TR_Helper))
      return _helpers[h];
   else
      return reinterpret_cast<void*>(TR_RuntimeHelperTable::INVALID_FUNCTION_POINTER);
   }

void
initializeJitRuntimeHelperTable(char isSMP)
   {
   }
