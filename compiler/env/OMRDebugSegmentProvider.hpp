/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#ifndef OMR_DEBUGSEGMENTPROVIDER_INCL
#define OMR_DEBUGSEGMENTPROVIDER_INCL

#pragma once

#include <stddef.h>
#include <set>
#include "env/TypedAllocator.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "env/SegmentAllocator.hpp"
#include "env/RawAllocator.hpp"
#include "env/SegmentProvider.hpp"

namespace OMR
{

/** @class DebugSegmentProvider
 *  @brief The DebugSegmentProvider class provides a facility for verifying the
 *  correctness of compiler scratch memory use.
 *
 *  Using the native facilities available on each platform, a DebugSegmentProvider
 *  provides an alternative allocation mechanism for the TR::MemorySegments used
 *  by the compiler's scratch memory regions.  Instead of releasing virtual address
 *  segments back to the operating system, this implementation instead either
 *  remaps the segment, freeing the underlying physical pages, and changing the
 *  memory protection to trap on any access [preferred], or, if such facilities
 *  are not available, paints the memory segment with a value that should cause
 *  pointer dereferences to be unaligned, and resolve to the high-half of the
 *  virtual address space often reserved for kernel / supervisor use.
 **/
class DebugSegmentProvider : public TR::SegmentProvider
   {
public:
   DebugSegmentProvider(size_t defaultSegmentSize, size_t allocateSegmentSize, size_t allocationLimit, TR::SegmentAllocator &segmentAllocator, TR::RawAllocator rawAllocator)
      : TR::SegmentProvider(defaultSegmentSize, allocateSegmentSize, allocationLimit, segmentAllocator, rawAllocator)
      { }

   virtual TR::MemorySegment &request(size_t requiredSize);
   virtual void release(TR::MemorySegment &segment) throw();
   };

} // namespace OMR

#endif // OMR_DEBUGSEGMENTPROVIDER_INCL
