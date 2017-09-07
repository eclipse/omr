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
#else
   TR_ASSERT(0, "unknown host architecture");
   _majorArch = TR::arch_unknown;
#endif

   }
