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

#ifndef OMR_MONITORTABLE_INCL
#define OMR_MONITORTABLE_INCL

#ifndef OMR_MONITORTABLE_CONNECTOR
#define OMR_MONITORTABLE_CONNECTOR
namespace OMR { class MonitorTable; }
namespace OMR { typedef MonitorTable MonitorTableConnector; }
#endif

#include "infra/Annotations.hpp"
#include "infra/Assert.hpp"

namespace TR { class MonitorTable; }
namespace TR { class Monitor; }

namespace OMR
{
// singleton
class OMR_EXTENSIBLE MonitorTable
   {
   public:

   static TR::MonitorTable *get() { return _instance; }

   void free() { TR_UNIMPLEMENTED(); }
   void removeAndDestroy(TR::Monitor *monitor) { TR_UNIMPLEMENTED(); }

   TR::Monitor *getMemoryAllocMonitor() { return _memoryAllocMonitor; }
   TR::Monitor *getScratchMemoryPoolMonitor() { return _scratchMemoryPoolMonitor; }

   protected:

   TR::MonitorTable *self();

   static TR::MonitorTable *_instance;

   // Used by TR_PersistentMemory
   //
   TR::Monitor *_memoryAllocMonitor;

   // Used by SCRATCH segments allocations
   // A copy of this goes into TR_PersistentMemory as well
   //
   TR::Monitor *_scratchMemoryPoolMonitor;

   private:

   friend class TR::Monitor;
   friend class TR::MonitorTable;

   TR::Monitor *create(char *name) { TR_UNIMPLEMENTED(); return 0; }
   };

}

#endif
