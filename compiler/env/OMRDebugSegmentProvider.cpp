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
#include "env/DebugSegmentProvider.hpp"

TR::MemorySegment &
TR::DebugSegmentProvider::request(size_t requiredSize)
   {
   size_t const roundedSize = round(requiredSize);
   BackingSegment &newSegment = _segmentAllocator.allocate(roundedSize);
   TR::reference_wrapper<BackingSegment> newSegmentRef = TR::ref(newSegment);

   try
      {
      _allocatedSegments.push_back(newSegmentRef);
      _bytesAllocated += requiredSize;
      }
   catch (...)
      {
      _segmentAllocator.deallocate(newSegment);
      throw;
      }

   return provideNewSegment(roundedSize, newSegmentRef);
   }

void
TR::DebugSegmentProvider::release(TR::MemorySegment &segment) throw()
   {
   _segmentAllocator.protect(*reinterpret_cast<BackingSegment *>(segment.base()));
   }
