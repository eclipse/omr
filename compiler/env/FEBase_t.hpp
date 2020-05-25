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

#if defined(OMR_OS_WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#endif /* OMR_OS_WINDOWS */
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/FEBase.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "runtime/CodeCacheManager.hpp"

namespace TR
{

// This code does not really belong here (along with allocateRelocationData, really)
// We should be relying on the port library to allocate memory, but this connection
// has not yet been made, so as a quick workaround for platforms like OS X <= 10.9,
// where MAP_ANONYMOUS is not defined, is to map MAP_ANON to MAP_ANONYMOUS ourselves
#if defined(__APPLE__)
   #if !defined(MAP_ANONYMOUS)
      #define NO_MAP_ANONYMOUS
      #if defined(MAP_ANON)
         #define MAP_ANONYMOUS MAP_ANON
      #else
         #error unexpectedly, no MAP_ANONYMOUS or MAP_ANON definition
      #endif
   #endif
#endif /* defined(__APPLE__) */

template <class Derived>
uint8_t *
FEBase<Derived>::allocateRelocationData(TR::Compilation* comp, uint32_t size)
   {
   /* FIXME: using an mmap without much thought into whether that is the best
      way to allocate this */
   if (size == 0) return 0;
   TR_ASSERT(size >= 2048, "allocateRelocationData should be used for whole-sale memory allocation only");

#if defined(OMR_OS_WINDOWS)
   return reinterpret_cast<uint8_t *>(
         VirtualAlloc(NULL,
            size,
            MEM_COMMIT,
            PAGE_READWRITE));
// TODO: Why is there no OMR_OS_ZOS? Or any other OS for that matter?
#elif defined(J9ZOS390)
   // TODO: This is an absolute hack to get z/OS JITBuilder building and even remotely close to working. We really
   // ought to be using the port library to allocate such memory. This was the quickest "workaround" I could think
   // of to just get us off the ground.
   return reinterpret_cast<uint8_t *>(
         __malloc31(size));
#else
   return reinterpret_cast<uint8_t *>(
         mmap(0,
              size,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS,
              -1,
              0));
#endif /* OMR_OS_WINDOWS */
   }

// keep the impact of this fix localized
#if defined(NO_MAP_ANONYMOUS)
  #undef MAP_ANONYMOUS
  #undef NO_MAP_ANONYMOUS
#endif

} /* namespace TR */
