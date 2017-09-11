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

#ifndef OMR_CODECACHEMEMORYSEGMEN_INCL
#define OMR_CODECACHEMEMORYSEGMEN_INCL

/*
 * The following #defines and typedefs must appear before any #includes in this file
 */

#ifndef OMR_CODECACHEMEMORYSEGMENT_COMPOSED
#define OMR_CODECACHEMEMORYSEGMENT_COMPOSED
namespace OMR { class CodeCacheMemorySegment; }
namespace OMR { typedef CodeCacheMemorySegment CodeCacheMemorySegmentConnector; }
#endif

#include <stddef.h>               // for size_t
#include <stdint.h>               // for uint8_t, int32_t, etc
#include "infra/Annotations.hpp"  // for OMR_EXTENSIBLE
#include "runtime/CodeCacheTypes.hpp"


namespace TR { class CodeCacheMemorySegment; }
namespace TR { class CodeCacheManager; }

namespace OMR
{

class OMR_EXTENSIBLE CodeCacheMemorySegment
   {
public:
   CodeCacheMemorySegment() : _base(NULL), _alloc(NULL), _top(NULL) { }
   CodeCacheMemorySegment(uint8_t *memory, size_t size) : _base(memory), _alloc(memory), _top(memory+size) { }
   CodeCacheMemorySegment(uint8_t *memory, uint8_t *top) : _base(memory), _alloc(memory), _top(top) { }

   TR::CodeCacheMemorySegment *self();

   void *operator new(size_t size, TR::CodeCacheMemorySegment *segment) { return segment; }

   uint8_t *segmentBase()  { return _base; }
   uint8_t *segmentAlloc() { return _alloc; }
   uint8_t *segmentTop()   { return _top; }

   void adjustAlloc(int64_t adjust);

   void setSegmentBase(uint8_t *newBase)   { _base = newBase; }
   void setSegmentAlloc(uint8_t *newAlloc) { _alloc = newAlloc; }
   void setSegmentTop(uint8_t *newTop)     { _top = newTop; }

   // memory is backed by something else
   void free(TR::CodeCacheManager *manager);

   uint8_t *_base;
   uint8_t *_alloc;
   uint8_t *_top;
   };

}

#endif // OMR_CODECACHEMEMORYSEGMEN_INCL
