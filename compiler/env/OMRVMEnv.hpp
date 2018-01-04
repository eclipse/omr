/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef OMR_VMENV_INCL
#define OMR_VMENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_VMENV_CONNECTOR
#define OMR_VMENV_CONNECTOR
namespace OMR { class VMEnv; }
namespace OMR { typedef OMR::VMEnv VMEnvConnector; }
#endif

#include <stdint.h>        // for int32_t, int64_t, uint32_t
#include "infra/Annotations.hpp"
#include "env/jittypes.h"

namespace TR { class VMEnv; }

struct OMR_VMThread;
namespace TR { class Compilation; }

namespace OMR
{

class OMR_EXTENSIBLE VMEnv
   {
public:

   TR::VMEnv * self();

   int64_t maxHeapSizeInBytes() { return -1; }

   uintptrj_t heapBaseAddress();

   uintptrj_t heapTailPaddingSizeInBytes();

   // Perhaps 'false' would be a better default
   bool hasResumableTrapHandler(TR::Compilation *comp) { return true; }
   bool hasResumableTrapHandler(OMR_VMThread *omrVMThread) { return true; }

   uint64_t getUSecClock();
   uint64_t getUSecClock(TR::Compilation *comp);
   uint64_t getUSecClock(OMR_VMThread *omrVMThread);

   uint64_t getHighResClock(TR::Compilation *comp);
   uint64_t getHighResClock(OMR_VMThread *omrVMThread);

   uint64_t getHighResClockResolution();

   uintptrj_t thisThreadGetPendingExceptionOffset() { return 0; }

   // Is specified thread permitted to access the VM?
   //
   bool hasAccess(OMR_VMThread *omrVMThread) { return true; }
   bool hasAccess(TR::Compilation *comp) { return true; }

   // Acquire access to the VM
   //
   bool acquireVMAccessIfNeeded(OMR_VMThread *omrVMThread) { return true; }
   bool acquireVMAccessIfNeeded(TR::Compilation *comp) { return true; }

   bool tryToAcquireAccess(OMR_VMThread *omrVMThread, bool *) { return true; }
   bool tryToAcquireAccess(TR::Compilation *, bool *) { return true; }

   // Release access to the VM
   //
   void releaseVMAccessIfNeeded(TR::Compilation *comp, bool haveAcquiredVMAccess) {}
   void releaseVMAccessIfNeeded(OMR_VMThread *omrVMThread, bool haveAcquiredVMAccess) {}

   void releaseAccess(TR::Compilation *comp) {}
   void releaseAccess(OMR_VMThread *omrVMThread) {}

   bool canMethodEnterEventBeHooked(TR::Compilation *comp) { return false; }
   bool canMethodExitEventBeHooked(TR::Compilation *comp) { return false; }
   bool canAnyMethodEventsBeHooked(TR::Compilation *comp);

   // Largest object that can be safely allocated without overflowing the heap.
   //
   uintptrj_t getOverflowSafeAllocSize(TR::Compilation *comp) { return 0; }

   int64_t cpuTimeSpentInCompilationThread(TR::Compilation *comp) { return -1; } // -1 means unavailable

   // On-stack replacement
   //
   uintptrj_t OSRFrameHeaderSizeInBytes(TR::Compilation *comp) { return 0; }
   uintptrj_t OSRFrameSizeInBytes(TR::Compilation *comp, TR_OpaqueMethodBlock* method) { return 0; }
   bool ensureOSRBufferSize(TR::Compilation *comp, uintptrj_t osrFrameSizeInBytes, uintptrj_t osrScratchBufferSizeInBytes, uintptrj_t osrStackFrameSizeInBytes) { return false; }
   uintptrj_t thisThreadGetOSRReturnAddressOffset(TR::Compilation *comp) { return 0; }

   /**
    * @brief Returns offset from the current thread to the intermediate result field.
    * The field contains intermediate result from the latest guarded load during concurrent scavenge.
    */
   uintptrj_t thisThreadGetGSIntermediateResultOffset(TR::Compilation *comp) { return 0; }
   /**
    * @brief Returns offset from the current thread to the flags to check if concurrent scavenge is active
    */
   uintptrj_t thisThreadGetConcurrentScavengeActiveByteAddressOffset(TR::Compilation *comp) { return 0; }
   /**
    * @brief Returns offset from the current thread to the field with the base address of the evacuate memory region
    */
   uintptrj_t thisThreadGetEvacuateBaseAddressOffset(TR::Compilation *comp) { return 0; }
   /**
    * @brief Returns offset from the current thread to the field with the top address of the evacuate memory region
    */
   uintptrj_t thisThreadGetEvacuateTopAddressOffset(TR::Compilation *comp) { return 0; }
   /**
    * @brief Returns offset from the current thread to the operand address field.
    * It contains data from the most recent guarded load during concurrent scavenge
    */
   uintptrj_t thisThreadGetGSOperandAddressOffset(TR::Compilation *comp) { return 0; }
   /**
    * @brief Returns offset from the current thread to the filed with the read barrier handler address
    */
   uintptrj_t thisThreadGetGSHandlerAddressOffset(TR::Compilation *comp) { return 0; }

   };

}

#endif
