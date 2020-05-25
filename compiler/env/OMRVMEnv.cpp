/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "env/ObjectModel.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/VMEnv.hpp"
#include "infra/Assert.hpp"
#include "env/JitConfig.hpp"
#include "env/VerboseLog.hpp"

#ifdef LINUX
#include <sys/time.h>
#endif

namespace TR { class Node; }

TR::VMEnv *
OMR::VMEnv::self()
   {
   return static_cast<TR::VMEnv *>(this);
   }


uintptr_t
OMR::VMEnv::heapBaseAddress()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


uintptr_t
OMR::VMEnv::heapTailPaddingSizeInBytes()
   {
   return 0;
   }

// Some of these functions should live in the OMR port library where operating
// system specialization comes naturally.

uint64_t
OMR::VMEnv::getUSecClock()
   {
#if defined(LINUX) || defined(OSX)
   struct timeval tp;
   struct timezone tzp;

   gettimeofday (&tp, &tzp);
   return (tp.tv_sec * 1000000) + tp.tv_usec;
#else
   // TODO: need Windows, AIX, zOS support
   return 0;
#endif
   }

uint64_t
OMR::VMEnv::getUSecClock(TR::Compilation *comp)
   {
   return self()->getUSecClock();
   }

uint64_t
OMR::VMEnv::getUSecClock(OMR_VMThread *omrVMThread)
   {
   return self()->getUSecClock();
   }

uint64_t
OMR::VMEnv::getHighResClock(TR::Compilation *comp)
   {
   return self()->getUSecClock();
   }

uint64_t
OMR::VMEnv::getHighResClock(OMR_VMThread *omrVMThread)
   {
   return self()->getUSecClock();
   }

static uint64_t highResClockResolution()
   {
   return 1000000ull; // micro sec
   }

uint64_t
OMR::VMEnv::getHighResClockResolution()
   {
   return ::highResClockResolution();
   }

bool
OMR::VMEnv::canAnyMethodEventsBeHooked(TR::Compilation *comp)
   {
   return self()->canMethodEnterEventBeHooked(comp) || self()->canMethodExitEventBeHooked(comp);
   }
