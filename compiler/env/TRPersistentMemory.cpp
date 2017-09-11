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

#include <algorithm>                    // for std::min
#include <stddef.h>                     // for size_t, NULL
#include <stdint.h>                     // for int32_t, uint32_t, uintptr_t, etc
#include <stdio.h>                      // for fflush, fprintf, stderr
#include <stdlib.h>                     // for free, malloc
#include <string.h>                     // for memset
#include "infra/CriticalSection.hpp"    // for CriticalSection
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"  // for TR::Options, etc
#include "env/PersistentAllocator.hpp"  // for PersistentAllocator
#include "env/TRMemory.hpp"             // for TR_PersistentMemory, etc
#include "il/DataTypes.hpp"             // for pointer_cast
#include "infra/Assert.hpp"             // for TR_ASSERT
#include "infra/Monitor.hpp"            // for Monitor
#include "infra/MonitorTable.hpp"       // for MonitorTable

namespace TR { class Compilation; }
namespace TR { class PersistentInfo; }

extern TR::Monitor *memoryAllocMonitor;

namespace TR
   {
   namespace Internal
      {
      // See TRMemory.hpp for rationale.
      PersistentNewType persistent_new_object;
      }
   }


// **************************************************************************
//
// deprecated uses of the global trMemory (not thread safe)
//

TR_PersistentMemory * trPersistentMemory = NULL;

void * TR_MemoryBase::jitPersistentAlloc(size_t size, ObjectType ot) { return ::trPersistentMemory ? ::trPersistentMemory->allocatePersistentMemory(size, ot) : 0 ; }
void   TR_MemoryBase::jitPersistentFree(void * mem)                  { ::trPersistentMemory->freePersistentMemory(mem); }

TR::PersistentInfo * TR_PersistentMemory::getNonThreadSafePersistentInfo() { return ::trPersistentMemory->getPersistentInfo(); }

TR_PersistentMemory::TR_PersistentMemory(
   void *   jitConfig,
   TR::PersistentAllocator &persistentAllocator
   ) :
   TR_MemoryBase(jitConfig),
   _signature(MEMINFO_SIGNATURE),
   _persistentInfo(this),
   _persistentAllocator(TR::ref(persistentAllocator)),
   _totalPersistentAllocations()
   {
   }
