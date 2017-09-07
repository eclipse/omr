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

#include "infra/Assert.hpp"  // for TR_ASSERT

#define VALID_PROCESSOR TR_ASSERT(id() >= TR_FirstPPCProcessor && id() <= TR_LastPPCProcessor, "Not a valid PPC Processor Type")

namespace OMR
{

namespace Power
{

class CPU : public OMR::CPU
   {
protected:

   CPU() : OMR::CPU() {}

public:

   bool getSupportsHardwareSQRT() { VALID_PROCESSOR; return id() >= TR_FirstPPCHwSqrtProcessor; }
   bool getSupportsHardwareRound() { VALID_PROCESSOR; return id() >= TR_FirstPPCHwRoundProcessor; }
   bool getSupportsHardwareCopySign() { VALID_PROCESSOR; return id() >= TR_FirstPPCHwCopySignProcessor;}

   bool getPPCis64bit();
   bool getPPCSupportsVMX() { return false; }
   bool getPPCSupportsVSX() { return false; }
   bool getPPCSupportsAES() { return false; }
   bool getPPCSupportsTM()  { return false; }
   bool getPPCSupportsLM()  { return false; }
   };

}

}

#endif
