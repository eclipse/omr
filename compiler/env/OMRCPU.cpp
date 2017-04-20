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

#include "env/CPU.hpp"
#include "env/CompilerEnv.hpp"
#include "infra/Assert.hpp"
#include "env/defines.h"

#ifndef GLUELESS
#include "omrcfg.h"
#endif

TR::CPU *
OMR::CPU::self()
   {
   return static_cast<TR::CPU*>(this);
   }


void
OMR::CPU::initializeByHostQuery()
   {

   char SwapTest[2] = { 1, 0 };
   bool isHostLittleEndian = (*(short *)SwapTest == 1);

#ifndef GLUELESS
#ifdef OMR_ENV_LITTLE_ENDIAN
   _endianness = TR::endian_little;
#else
   _endianness = TR::endian_big;
#endif
   // Validate host endianness #define with an additional compile-time test
   //
   if (_endianness == TR::endian_little)
      {
      TR_ASSERT(isHostLittleEndian, "expecting host to be little endian");
      }
   else
      {
      TR_ASSERT(!isHostLittleEndian, "expecting host to be big endian");
      }
#else 
   // Don't have OMR glue, so we're not relying on OMR's processor detection,
   // so we just use the computed endianness
   
   _endianness = isHostLittleEndian ? TR::endian_little : TR::endian_big; 
#endif

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
