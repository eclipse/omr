/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef X86RUNTIME_INCL
#define X86RUNTIME_INCL

#include "env/ProcessorInfo.hpp"

#if defined(OMR_OS_WINDOWS)
#include <intrin.h>
#define cpuid(CPUInfo, EAXValue)             __cpuid(CPUInfo, EAXValue)
#define cpuidex(CPUInfo, EAXValue, ECXValue) __cpuidex(CPUInfo, EAXValue, ECXValue)
#else
#include <cpuid.h>
#include <emmintrin.h>
#define cpuid(CPUInfo, EAXValue)             __cpuid(EAXValue, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3])
#define cpuidex(CPUInfo, EAXValue, ECXValue) __cpuid_count(EAXValue, ECXValue, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3])
inline unsigned long long _xgetbv(unsigned int ecx)
   {
   unsigned int eax, edx;
   __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(ecx));
   return ((unsigned long long)edx << 32) | eax;
   }
#endif /* defined(OMR_OS_WINDOWS) */

/**
 * @brief maskProcessorFlags
 * @param pBuffer
 *
 * Masks out the processor features the compiler does not
 * care about.
 */
inline void maskProcessorFlags(TR_X86CPUIDBuffer* pBuffer)
   {
   pBuffer->_featureFlags  &= getFeatureFlagsMask();
   pBuffer->_featureFlags2 &= getFeatureFlags2Mask();
   pBuffer->_featureFlags8 &= getFeatureFlags8Mask();
   }

char* feGetEnv(const char*);
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

      // Check for XSAVE
      if(pBuffer->_featureFlags2 & TR_OSXSAVE)
         {
         if(((6 & _xgetbv(0)) != 6) || feGetEnv("TR_DisableAVX")) // '6' = mask for XCR0[2:1]='11b' (XMM state and YMM state are enabled)
            {
            // Unset OSXSAVE if not enabled via CR0
            pBuffer->_featureFlags2 &= ~TR_OSXSAVE;
            }
         }

      /* Mask out the bits the compiler does not care about.
       * This is necessary for relocatable compilations; without
       * this step, validations might fail because of mismatches
       * in unused hardware features */
      maskProcessorFlags(pBuffer);
      return true;
      }
   else
      {
      return false;
      }
   }

inline bool AtomicCompareAndSwap(volatile uint32_t* ptr, uint32_t old_val, uint32_t new_val)
   {
#if defined(OMR_OS_WINDOWS)
   return old_val == (uint32_t)_InterlockedCompareExchange((volatile long*)ptr, (long)new_val, (long)old_val);
#else
   return __sync_bool_compare_and_swap(ptr, old_val, new_val);
#endif /* defined(OMR_OS_WINDOWS) */
   }

inline bool AtomicCompareAndSwap(volatile uint16_t* ptr, uint16_t old_val, uint16_t new_val)
   {
#if defined(OMR_OS_WINDOWS)
   return old_val == (uint16_t)_InterlockedCompareExchange16((volatile short*)ptr, (short)new_val, (short)old_val);
#else
   return __sync_bool_compare_and_swap(ptr, old_val, new_val);
#endif /* defined(OMR_OS_WINDOWS) */
   }

inline void patchingFence16(void* addr)
   {
   _mm_mfence();
   _mm_clflush(addr);
   _mm_clflush(static_cast<char*>(addr)+8);
   _mm_mfence();
   }

#endif
