/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_X86_CPU_INCL
#define OMR_X86_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CPU_CONNECTOR
#define OMR_CPU_CONNECTOR
namespace OMR { namespace X86 { class CPU; } }
namespace OMR { typedef OMR::X86::CPU CPUConnector; }
#else
#error OMR::X86::CPU expected to be a primary connector, but an OMR connector is already defined
#endif

#include <stdint.h>
#include "compiler/env/OMRCPU.hpp"
#include "env/jittypes.h"
#include "omrport.h"

struct TR_X86CPUIDBuffer;
namespace TR { class Compilation; }


namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE CPU : public OMR::CPU
   {
protected:

   CPU() : OMR::CPU() {}
   CPU(const OMRProcessorDesc& processorDescription) : OMR::CPU(processorDescription) {}

public:

   static TR::CPU detect(OMRPortLibrary * const omrPortLib);

   static void initializeTargetProcessorInfo(bool force = false);

   TR_X86CPUIDBuffer *queryX86TargetCPUID();
   const char *getX86ProcessorVendorId();
   uint32_t getX86ProcessorSignature();
   uint32_t getX86ProcessorFeatureFlags();
   uint32_t getX86ProcessorFeatureFlags2();
   uint32_t getX86ProcessorFeatureFlags8();
   uint32_t getX86ProcessorFeatureFlags10();

   bool getSupportsHardwareSQRT();
   bool hasBitCompressInstruction();
   bool hasBitExpandInstruction();

   /** @brief Determines whether the Transactional Memory (TM) facility is available on the current processor.
    *
    *  @return true if TM is available, false otherwise.
    */
   bool supportsTransactionalMemoryInstructions();

   /**
    * @brief Answers whether the distance between a target and source address
    *        is within the reachable RIP displacement range.
    *
    * @param[in] : targetAddress : the address of the target
    *
    * @param[in] : sourceAddress : the address of the instruction following the
    *                 branch instruction.
    *
    * @return true if the target is within range; false otherwise.
    */
   bool isTargetWithinRIPRange(intptr_t targetAddress, intptr_t sourceAddress)
      {
      return targetAddress == sourceAddress + (int32_t)(targetAddress - sourceAddress);
      }
   bool isGenuineIntel();
   bool isAuthenticAMD();

   bool requiresLFence();
   bool supportsMFence();
   bool supportsLFence();
   bool supportsSFence();
   bool prefersMultiByteNOP();
   bool supportsAVX();

   /**
    * It is generally safe to assume that all modern operating systems
    * support preserving the SSE state.  However, to be strictly
    * correct, this support should be verified.
    *
    * See issue #5964.
    */
   bool testOSForSSESupport() { return true; }

   /**
    * @brief Determines whether 32bit integer rotate is available
    *
    * @details
    *    Returns true if 32bit integer rotate to left is available when requireRotateToLeft is true.
    *    Returns true if 32bit integer rotate (right or left) is available when requireRotateToLeft is false.
    *
    * @param requireRotateToLeft if true, returns true if rotate to left operation is available.
    */
   bool getSupportsHardware32bitRotate(bool requireRotateToLeft=false) { return true; }
   /**
    * @brief Determines whether 64bit integer rotate is available
    *
    * @details
    *    Returns true if 64bit integer rotate to left is available when requireRotateToLeft is true.
    *    Returns true if 64bit integer rotate (right or left) is available when requireRotateToLeft is false.
    *
    * @param requireRotateToLeft if true, returns true if rotate to left operation is available.
    */
   bool getSupportsHardware64bitRotate(bool requireRotateToLeft=false) { return true; }

   // Will be removed once we no longer need the old processor detection apis
   bool is(OMRProcessorArchitecture p);
   bool is_old_api(OMRProcessorArchitecture p);
   bool is_test(OMRProcessorArchitecture p);

   bool supportsFeature(uint32_t feature);
   bool supports_feature_old_api(uint32_t feature);
   bool supports_feature_test(uint32_t feature);

   /**
    * @brief Returns name of the current processor
    * @returns const char* string representing the name of the current processor
    */
   const char* getProcessorName();
   };
}

}

#endif
