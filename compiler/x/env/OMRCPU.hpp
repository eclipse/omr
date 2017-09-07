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
#error OMR::X86::CPU expected to be a primary connector, but a OMR connector is already defined
#endif

#include <stdint.h>
#include "compiler/env/OMRCPU.hpp"

struct TR_X86CPUIDBuffer;
namespace TR { class Compilation; }


namespace OMR
{

namespace X86
{

class CPU : public OMR::CPU
   {
protected:

   CPU() : OMR::CPU() {}

public:

   TR_X86CPUIDBuffer *queryX86TargetCPUID(TR::Compilation *comp);
   const char *getX86ProcessorVendorId(TR::Compilation *comp);
   uint32_t getX86ProcessorSignature(TR::Compilation *comp);
   uint32_t getX86ProcessorFeatureFlags(TR::Compilation *comp);
   uint32_t getX86ProcessorFeatureFlags2(TR::Compilation *comp);
   uint32_t getX86ProcessorFeatureFlags8(TR::Compilation *comp);

   bool testOSForSSESupport(TR::Compilation *comp);
   bool getX86OSSupportsSSE(TR::Compilation *comp);
   bool getX86OSSupportsSSE2(TR::Compilation *comp);
   bool getX86SupportsTM(TR::Compilation *comp);

   };

}

}

#endif /* OMR_X86_CPU_BASE_INCL */
