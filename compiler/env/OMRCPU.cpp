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

#pragma csect(CODE,"OMRCPUBase#C")
#pragma csect(STATIC,"OMRCPUBase#S")
#pragma csect(TEST,"OMRCPUBase#T")

#include "env/CPU.hpp"
#include "env/CompilerEnv.hpp"
#include "infra/Assert.hpp"
#include "env/defines.h"
#include "omrcfg.h"

TR::CPU *
OMR::CPU::self()
   {
   return static_cast<TR::CPU*>(this);
   }

TR::CPU
OMR::CPU::detect(OMRPortLibrary * const omrPortLib)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(omrPortLib);
   OMRProcessorDesc processorDescription;
   omrsysinfo_get_processor_description(&processorDescription);
   return TR::CPU(processorDescription);
   }

void
OMR::CPU::initializeByHostQuery()
   {

#ifdef OMR_ENV_LITTLE_ENDIAN
   _endianness = TR::endian_little;
#else
   _endianness = TR::endian_big;
#endif

   // Validate host endianness #define with an additional compile-time test
   //
   char SwapTest[2] = { 1, 0 };
   bool isHostLittleEndian = (*(short *)SwapTest == 1);

   if (_endianness == TR::endian_little)
      {
      TR_ASSERT(isHostLittleEndian, "expecting host to be little endian");
      }
   else
      {
      TR_ASSERT(!isHostLittleEndian, "expecting host to be big endian");
      }

#if defined(TR_HOST_X86)
   _majorArch = TR::arch_x86;
   #if defined (TR_HOST_64BIT)
   _minorArch = TR::m_arch_amd64;
   #else
   _minorArch = TR::m_arch_i386;
   #endif
#elif defined (TR_HOST_POWER)
   _majorArch = TR::arch_power;
#elif defined(TR_HOST_ARM)
   _majorArch = TR::arch_arm;
#elif defined(TR_HOST_S390)
   _majorArch = TR::arch_z;
#elif defined(TR_HOST_ARM64)
   _majorArch = TR::arch_arm64;
#elif defined(TR_HOST_RISCV)
   _majorArch = TR::arch_riscv;
   #if defined (TR_HOST_64BIT)
   _minorArch = TR::m_arch_riscv64;
   #else
   _minorArch = TR::m_arch_riscv32;
   #endif
#else
   TR_ASSERT(0, "unknown host architecture");
   _majorArch = TR::arch_unknown;
#endif

   }

bool
OMR::CPU::supportsFeature(uint32_t feature)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   BOOLEAN supported = omrsysinfo_processor_has_feature(&_processorDescription, feature);
   return (TRUE == supported);
   }

