/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
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
 *******************************************************************************/

#ifndef X86RUNTIME_INCL
#define X86RUNTIME_INCL

#include "env/ProcessorInfo.hpp"

#ifdef WINDOWS
#include <intrin.h> // for __cpuid, __cpuidex
#define cpuid(CPUInfo, EAXValue)             __cpuid(CPUInfo, EAXValue)
#define cpuidex(CPUInfo, EAXValue, ECXValue) __cpuidex(CPUInfo, EAXValue, ECXValue)
#else
#include <cpuid.h>
#define cpuid(CPUInfo, EAXValue)             __cpuid(EAXValue, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3] )
#define cpuidex(CPUInfo, EAXValue, ECXValue) __cpuid_count(EAXValue, ECXValue, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3] )
#endif

inline bool jitGetCPUID(TR_X86CPUIDBuffer* pBuffer)
   {
   enum
      {
      EAX = 0,
      EBX = 1,
      ECX = 2,
      EDX = 3,
      };
   int CPUInfo[4];

   // EAX = 0
   cpuid(CPUInfo, 0);
   int* VendorID = (int*)pBuffer->_vendorId;
   VendorID[0] = CPUInfo[EBX];
   VendorID[1] = CPUInfo[EDX];
   VendorID[2] = CPUInfo[ECX];

   if (CPUInfo[EAX] > 0)
      {
      // EAX = 1
      cpuid(CPUInfo, 1);
      pBuffer->_processorSignature = CPUInfo[EAX];
      pBuffer->_brandIdEtc         = CPUInfo[EBX];
      pBuffer->_featureFlags       = CPUInfo[EDX];
      pBuffer->_featureFlags2      = CPUInfo[ECX];
      // EAX = 7, ECX = 0
      cpuidex(CPUInfo, 7, 0);
      pBuffer->_featureFlags8 = CPUInfo[EBX];
      return true;
      }
   else
      {
      return false;
      }
   }

#endif
