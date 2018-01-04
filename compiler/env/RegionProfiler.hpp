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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_REGION_PROFILER_HPP
#define OMR_REGION_PROFILER_HPP

#pragma once

#include "env/Region.hpp"
#include "env/SegmentProvider.hpp"
#include "compile/Compilation.hpp"

namespace TR {

/**
 * @class
 * @brief The RegionProfiler class provides a mechanism for profiling between
 * two points of execution determined lexically by the lifetime of the profiler
 * object.
 *
 * This class makes use of the compiler's debug counter facility to record the
 * difference in memory usage for a region and its segment provider between the
 * two points of execution determined by the invocation of its constructor and
 * the invocation of its destructor. The lifetime of the region tracked by the
 * profiler object must comprehend the lifetime of the profiler itself. The
 * implementation requires a compilation object in order to determine whether
 * or not the facility is active.
 */

class RegionProfiler
   {
public:
   RegionProfiler(TR::Region &region, TR::Compilation &compilation, const char *format, ...) :
      _region(region),
      _initialRegionSize(_region.bytesAllocated()),
      _initialSegmentProviderSize(_region._segmentProvider.bytesAllocated()),
      _compilation(compilation)
      {
      if (_compilation.getOption(TR_ProfileMemoryRegions))
         {
         va_list args;
         va_start(args, format);
         int len = vsnprintf(_identifier, sizeof(_identifier), format, args);
         TR_ASSERT(len < sizeof(_identifier), "Region profiler identifier truncated as it exceeded max length %d", sizeof(_identifier));
         _identifier[sizeof(_identifier) - 1] = '\0';
         va_end(args);
         }
      }

   ~RegionProfiler()
      {
      if (_compilation.getOption(TR_ProfileMemoryRegions))
         {
         TR::DebugCounter::incStaticDebugCounter(
            &_compilation,
            TR::DebugCounter::debugCounterName(
               &_compilation,
               "kbytesAllocated.details/%s",
               _identifier
               ),
            (_region.bytesAllocated() - _initialRegionSize) / 1024
            );
         TR::DebugCounter::incStaticDebugCounter(
            &_compilation,
            TR::DebugCounter::debugCounterName(
               &_compilation,
               "segmentAllocation.details/%s",
                _identifier
                ),
            (_region._segmentProvider.bytesAllocated() - _initialSegmentProviderSize) / 1024
            );
         }
      }

private:
   TR::Region &_region;
   size_t const _initialRegionSize;
   size_t const _initialSegmentProviderSize;
   TR::Compilation &_compilation;
   char _identifier[256];
   };

}

#endif
