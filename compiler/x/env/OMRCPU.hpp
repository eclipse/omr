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

   TR_X86CPUIDBuffer *queryX86TargetCPUID();
   const char *getX86ProcessorVendorId();
   uint32_t getX86ProcessorSignature();
   uint32_t getX86ProcessorFeatureFlags();
   uint32_t getX86ProcessorFeatureFlags2();
   uint32_t getX86ProcessorFeatureFlags8();

   bool getSupportsHardwareSQRT();

   bool testOSForSSESupport();


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
   bool supportsFCOMIInstructions();
   bool supportsMFence();
   bool supportsLFence();
   bool supportsSFence();
   bool prefersMultiByteNOP();
   };
}

}

#endif
