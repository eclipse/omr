/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "runtime/Runtime.hpp"

TR_RuntimeHelperTable runtimeHelpers;

#if (defined(__IBMCPP__) || defined(__IBMC__) && !defined(MVS)) && !defined(LINUXPPC64)
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

extern "C" void *PyTuple_GetItem(void*,int);

void*
TR_RuntimeHelperTable::translateAddress(void * a)
   {
   return a;
   }

void
initializeJitRuntimeHelperTable(char isSMP)
   {
   //runtimeHelpers.setAddress(PyHelper_TupleGetItem,                    (void*)PyTuple_GetItem);
   }
