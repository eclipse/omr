/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "infra/Monitor.hpp"

#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"

TR::MonitorTable *OMR::MonitorTable::_instance = 0;

TR::Monitor *memoryAllocMonitor = NULL;

void *
OMR::Monitor::operator new(size_t size)
   {
   return TR::Compiler->persistentAllocator().allocate(size);
   }

void
OMR::Monitor::operator delete(void *p)
   {
   TR::Compiler->persistentAllocator().deallocate(p);
   }

TR::Monitor *
OMR::Monitor::create(char *name)
   {
   TR::Monitor * monitor = new TR::Monitor();
   monitor->init(name);
   return monitor;
   }

OMR::Monitor::~Monitor()
   {
   self()->destroy();
   }

TR::Monitor *
OMR::Monitor::self()
   {
   return static_cast<TR::Monitor*>(this);
   }

bool
OMR::Monitor::init(char *name)
   {
   _name = name;
#if defined(OMR_OS_WINDOWS)
   MUTEX_INIT(_monitor);
#else
   bool rc = MUTEX_INIT(_monitor);
   TR_ASSERT(rc == true, "error initializing monitor\n");
#endif /* defined(OMR_OS_WINDOWS) */
   return true;
   }

void
OMR::Monitor::destroy(TR::Monitor *monitor)
   {
   delete monitor;
   }

void
OMR::Monitor::destroy()
   {
#if defined(OMR_OS_WINDOWS)
   MUTEX_DESTROY(_monitor);
#else
   int32_t rc = MUTEX_DESTROY(_monitor);
   TR_ASSERT(rc == 0, "error destroying monitor\n");
#endif /* defined(OMR_OS_WINDOWS) */
   }

void
OMR::Monitor::enter()
   {
#if defined(OMR_OS_WINDOWS)
   MUTEX_ENTER(_monitor);
#else
   int32_t rc = MUTEX_ENTER(_monitor);
   TR_ASSERT(rc == 0, "error locking monitor\n");
#endif /* defined(OMR_OS_WINDOWS) */
   }

int32_t
OMR::Monitor::exit()
   {
#if defined(OMR_OS_WINDOWS)
   MUTEX_EXIT(_monitor);
   return 0;
#else
   int32_t rc = MUTEX_EXIT(_monitor);
   TR_ASSERT(rc == 0, "error unlocking monitor\n");
   return rc;
#endif /* defined(OMR_OS_WINDOWS) */
   }

char const *
OMR::Monitor::getName()
   {
   return _name;
   }
