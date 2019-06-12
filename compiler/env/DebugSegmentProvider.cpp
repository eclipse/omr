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

#if ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX)
#include <sys/mman.h>
#if (HOST_OS == OMR_OSX) || !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif
#elif (HOST_OS == OMR_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <string.h>
#endif /* (HOST_OS == OMR_WINDOWS) */

#include "env/MemorySegment.hpp"
#include "env/DebugSegmentProvider.hpp"

TR::DebugSegmentProvider::DebugSegmentProvider(size_t segmentSize, TR::RawAllocator rawAllocator) :
   TR::SegmentAllocator(segmentSize),
   _rawAllocator(rawAllocator),
   _segments(std::less< TR::MemorySegment >(), SegmentSetAllocator(rawAllocator))
   {
   }

TR::DebugSegmentProvider::~DebugSegmentProvider() throw()
   {
   for ( auto it = _segments.begin(); it != _segments.end(); it = _segments.begin() )
      {
#if ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX)
      munmap(it->base(), it->size());
#elif (HOST_OS == OMR_WINDOWS)
      VirtualFree(it->base(), 0, MEM_RELEASE);
#else
      _rawAllocator.deallocate(it->base(), it->size());
#endif /* ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX) */
      _segments.erase(it);
      }
   }

TR::MemorySegment &
TR::DebugSegmentProvider::request(size_t requiredSize)
   {
   size_t adjustedSize = ( ( requiredSize + (defaultSegmentSize() - 1) ) / defaultSegmentSize() ) * defaultSegmentSize();
#if ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX)
   void *newSegmentArea = mmap(NULL, adjustedSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
   if (newSegmentArea == MAP_FAILED) throw std::bad_alloc();
#elif (HOST_OS == OMR_WINDOWS)
   void *newSegmentArea = VirtualAlloc(NULL, adjustedSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
   if (!newSegmentArea) throw std::bad_alloc();
#else
   void *newSegmentArea = _rawAllocator.allocate(requiredSize);
#endif /* ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX) */
   try
      {
      auto result = _segments.insert( TR::MemorySegment(newSegmentArea, adjustedSize) );
      TR_ASSERT(result.first != _segments.end(), "Bad iterator");
      TR_ASSERT(result.second, "Insertion failed");
      _bytesAllocated += adjustedSize;
      return const_cast<TR::MemorySegment &>(*(result.first));
      }
   catch (...)
      {
#if ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX)
      munmap(newSegmentArea, adjustedSize);
#elif (HOST_OS == OMR_WINDOWS)
      VirtualFree(newSegmentArea, 0, MEM_RELEASE);
#else
     _rawAllocator.deallocate(newSegmentArea, adjustedSize);
#endif /* ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX) */
      throw;
      }
   }

void
TR::DebugSegmentProvider::release(TR::MemorySegment &segment) throw()
   {
#if ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX)
   void * remap = mmap(segment.base(), segment.size(), PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
   TR_ASSERT(remap == segment.base(), "Remapping of memory failed!");
#elif (HOST_OS == OMR_WINDOWS)
   VirtualFree(segment.base(), segment.size(), MEM_DECOMMIT);
   VirtualAlloc(segment.base(), segment.size(), MEM_COMMIT, PAGE_NOACCESS);
#else
   memset(segment.base(), 0xEF, segment.size());
#endif /* ((HOST_OS == OMR_LINUX) && !defined(OMRZTPF)) || (HOST_OS == OMR_OSX) || (HOST_OS == OMR_AIX) */
   }

size_t
TR::DebugSegmentProvider::bytesAllocated() const throw()
   {
   return _bytesAllocated;
   }

size_t
TR::DebugSegmentProvider::regionBytesAllocated() const throw()
   {
   return _bytesAllocated;
   }

size_t
TR::DebugSegmentProvider::systemBytesAllocated() const throw()
   {
   return _bytesAllocated;
   }

size_t
TR::DebugSegmentProvider::allocationLimit() const throw()
   {
   return static_cast<size_t>(-1);
   }

void
TR::DebugSegmentProvider::setAllocationLimit(size_t allocationLimit)
   {
   return;
   }
