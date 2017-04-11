/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#include "env/ObjectModel.hpp"

#include <stddef.h>                   // for NULL
#include <stdint.h>                   // for int32_t, int64_t, uint32_t
#include "codegen/CodeGenerator.hpp"  // for CodeGenerator
#include "compile/Compilation.hpp"    // for Compilation
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"             // for uintptrj_t, intptrj_t
#include "env/VMEnv.hpp"
#include "infra/Assert.hpp"           // for TR_ASSERT
#include "env/JitConfig.hpp"
#include "env/VerboseLog.hpp"

#ifdef LINUX
#include <sys/time.h>
#endif

namespace TR { class Node; }

#define notImplemented(A) TR_ASSERT(0, "OMR::VMEnv::%s is undefined", (A) )


TR::VMEnv *
OMR::VMEnv::self()
   {
   return static_cast<TR::VMEnv *>(this);
   }


uintptrj_t
OMR::VMEnv::heapBaseAddress()
   {
   notImplemented("heapBaseAddress");
   return 0;
   }


uintptrj_t
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
