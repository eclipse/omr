/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "infra/CriticalSection.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/PersistentAllocator.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"

namespace TR { class Compilation; }
namespace TR { class PersistentInfo; }

extern TR::Monitor *memoryAllocMonitor;
extern const char * objectName[];

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
   TR::PersistentAllocator &persistentAllocator
   ) :
   TR_MemoryBase(),
   _signature(MEMINFO_SIGNATURE),
   _persistentInfo(this),
   _persistentAllocator(TR::ref(persistentAllocator)),
   _totalPersistentAllocations()
   {
   }

TR_PersistentMemory::TR_PersistentMemory(
   void *   jitConfig,
   TR::PersistentAllocator &persistentAllocator
   ) :
   TR_MemoryBase(),
   _signature(MEMINFO_SIGNATURE),
   _persistentInfo(this),
   _persistentAllocator(TR::ref(persistentAllocator)),
   _totalPersistentAllocations()
   {
   }

void
TR_PersistentMemory::printMemStats()
   {
   fprintf(stderr, "TR_PersistentMemory Stats:\n");
   for (uint32_t i = 0; i < TR_MemoryBase::NumObjectTypes; i++)
      {
      fprintf(stderr, "\t_totalPersistentAllocations[%s]=%lu\n", objectName[i], (unsigned long)_totalPersistentAllocations[i]);
      }
   fprintf(stderr, "\n");
   }

void
TR_PersistentMemory::printMemStatsToVlog()
   {
   TR_VerboseLog::vlogAcquire();
   TR_VerboseLog::writeLine(TR_Vlog_MEMORY, "TR_PersistentMemory Stats:");
   for (uint32_t i = 0; i < TR_MemoryBase::NumObjectTypes; i++)
      {
      TR_VerboseLog::writeLine(TR_Vlog_MEMORY, "\t_totalPersistentAllocations[%s]=%lu", objectName[i], (unsigned long)_totalPersistentAllocations[i]);
      }
   TR_VerboseLog::vlogRelease();
   }
