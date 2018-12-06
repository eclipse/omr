/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#if (defined(LINUX) && !defined(OMRZTPF)) || defined(__APPLE__) || defined(_AIX)
#include <sys/mman.h>
#if defined(__APPLE__) || !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif
#elif defined(OMR_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <string.h>
#endif /* defined(OMR_OS_WINDOWS) */

#include "env/MemorySegment.hpp"
#include "env/DebugSegmentAllocator.hpp"


TR::BackingSegment &
OMR::DebugSegmentAllocator::allocate(size_t size, void * hint)
   {
   BackingSegment *newSegment = NULL;

#if (defined(LINUX) && !defined(OMRZTPF)) || defined(__APPLE__) || defined(_AIX)
   newSegment = (BackingSegment *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
   if ((void *)newSegment == MAP_FAILED) throw std::bad_alloc();
#elif defined(OMR_OS_WINDOWS)
   newSegment = (BackingSegment *)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
   if (!newSegment) throw std::bad_alloc();
#else
   newSegment = TR::SegmentAllocator::.allocate(size); // will throw if cannot allocate
#endif /* (defined(LINUX) && !defined(OMRZTPF)) || defined(__APPLE__) || defined(_AIX) */

   return *newSegment;
   }

void
OMR::DebugSegmentAllocator::deallocate(BackingSegment &p) throw()
   {
#if (defined(LINUX) && !defined(OMRZTPF)) || defined(__APPLE__) || defined(_AIX)
   munmap(p.base(), p.size());
#elif defined(OMR_OS_WINDOWS)
   VirtualFree(p.base(), 0, MEM_RELEASE);
#else
   TR::SegmentAllocator::deallocate(p.base(), p.size());
#endif /* (defined(LINUX) && !defined(OMRZTPF)) || defined(__APPLE__) || defined(_AIX) */
   }

void
OMR::DebugSegmentAllocator::protect(BackingSegment & p) throw()
   {
#if (defined(LINUX) && !defined(OMRZTPF)) || defined(__APPLE__) || defined(_AIX)
   void * remap = mmap(p.base(), p.size(), PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
   TR_ASSERT(remap == p.base(), "Remapping of memory failed!");
#elif defined(OMR_OS_WINDOWS)
   VirtualFree(p.base(), p.size(), MEM_DECOMMIT);
   VirtualAlloc(p.base(), p.size(), MEM_COMMIT, PAGE_NOACCESS);
#else
   memset(p.base(), 0xEF, p.size());
#endif /* (defined(LINUX) && !defined(OMRZTPF)) || defined(__APPLE__) || defined(_AIX) */
   }
