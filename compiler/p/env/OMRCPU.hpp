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

#ifndef OMR_POWER_CPU_INCL
#define OMR_POWER_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CPU_CONNECTOR
#define OMR_CPU_CONNECTOR
namespace OMR { namespace Power { class CPU; } }
namespace OMR { typedef OMR::Power::CPU CPUConnector; }
#else
#error OMR::Power::CPU expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/env/OMRCPU.hpp"

#include "env/jittypes.h"
#include "infra/Assert.hpp"

namespace OMR
{

namespace Power
{

class OMR_EXTENSIBLE CPU : public OMR::CPU
   {
protected:

   CPU() : OMR::CPU() {}
   CPU(const OMRProcessorDesc& processorDescription) : OMR::CPU(processorDescription) {}

public:

   bool getSupportsHardwareSQRT();
   bool getSupportsHardwareRound();
   bool getSupportsHardwareCopySign();

   bool getPPCis64bit();
   bool getPPCSupportsVMX() { return false; }
   bool getPPCSupportsVSX() { return false; }
   bool getPPCSupportsAES() { return false; }
   bool getPPCSupportsTM()  { return false; }

   /** @brief Determines whether the Transactional Memory (TM) facility is available on the current processor.
    *         Alias of getPPCSupportsTM() as a platform agnostic query.
    *
    *  @return true if TM is available, false otherwise.
    */
   bool supportsTransactionalMemoryInstructions();

   /**
    * @brief Provides the maximum forward branch displacement in bytes reachable
    *        with an I-Form branch instruction.
    *
    * @return Maximum forward branch displacement in bytes.
    */
   int32_t maxIFormBranchForwardOffset() { return 0x01fffffc; }

   /**
    * @brief Provides the maximum backward branch displacement in bytes reachable
    *        with an I-Form branch instruction.
    *
    * @return Maximum backward branch displacement in bytes.
    */
   int32_t maxIFormBranchBackwardOffset() { return 0xfe000000; }

   /**
    * @brief Answers whether the distance between a target and source address
    *        is within the reachable displacement range of an I-form branch
    *        instruction.
    *
    * @param[in] : targetAddress : the address of the target
    *
    * @param[in] : sourceAddress : the address of the I-form branch instruction
    *                 from which the displacement range is measured.
    *
    * @return true if the target is within range; false otherwise.
    */
   bool isTargetWithinIFormBranchRange(intptr_t targetAddress, intptr_t sourceAddress);

   };

}

}

#endif
