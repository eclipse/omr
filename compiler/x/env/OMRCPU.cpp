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

#include <stdlib.h>
#include <string.h>
#include "env/CPU.hpp"
#include "env/JitConfig.hpp"
#include "env/ProcessorInfo.hpp"
#include "infra/Flags.hpp"
#include "x/runtime/X86Runtime.hpp"

TR::CPU
OMR::X86::CPU::detect(OMRPortLibrary * const omrPortLib)
   {   
   // Only enable the features that compiler currently uses
   uint32_t enabledFeatures [] = {OMR_FEATURE_X86_FPU, OMR_FEATURE_X86_CX8, OMR_FEATURE_X86_CMOV,
                                  OMR_FEATURE_X86_MMX, OMR_FEATURE_X86_SSE, OMR_FEATURE_X86_SSE2,
                                  OMR_FEATURE_X86_SSSE3, OMR_FEATURE_X86_SSE4_1, OMR_FEATURE_X86_POPCNT,
                                  OMR_FEATURE_X86_AESNI, OMR_FEATURE_X86_OSXSAVE, OMR_FEATURE_X86_AVX,
                                  OMR_FEATURE_X86_HLE, OMR_FEATURE_X86_RTM};

   OMRPORT_ACCESS_FROM_OMRPORT(omrPortLib);
   OMRProcessorDesc featureMasks;
   memset(featureMasks.features, 0, OMRPORT_SYSINFO_FEATURES_SIZE*sizeof(uint32_t));
   for (size_t i = 0; i < sizeof(enabledFeatures)/sizeof(uint32_t); i++)
      {
      omrsysinfo_processor_set_feature(&featureMasks, enabledFeatures[i], TRUE);
      }

   OMRProcessorDesc processorDescription;
   omrsysinfo_get_processor_description(&processorDescription);
   for (size_t i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
      {
      processorDescription.features[i] &= featureMasks.features[i];
      }

   return TR::CPU(processorDescription);
   }

TR_X86CPUIDBuffer *
OMR::X86::CPU::queryX86TargetCPUID()
   {
   static TR_X86CPUIDBuffer *buf = NULL;
   auto jitConfig = TR::JitConfig::instance();

   if (jitConfig && jitConfig->getProcessorInfo() == NULL)
      {
      buf = (TR_X86CPUIDBuffer *) malloc(sizeof(TR_X86CPUIDBuffer));
      if (!buf)
         return 0;
      jitGetCPUID(buf);
      jitConfig->setProcessorInfo(buf);
      }
   else
      {
      if (!buf)
         {
         if (jitConfig && jitConfig->getProcessorInfo())
            {
            buf = (TR_X86CPUIDBuffer *)jitConfig->getProcessorInfo();
            }
         else
            {
            buf = (TR_X86CPUIDBuffer *) malloc(sizeof(TR_X86CPUIDBuffer));

            if (!buf)
               memcpy(buf->_vendorId, "Poughkeepsie", 12); // 12 character dummy string (NIL term not used)

            buf->_processorSignature = 0;
            buf->_brandIdEtc = 0;
            buf->_featureFlags = 0x00000000;
            buf->_cacheDescription.l1instr = 0;
            buf->_cacheDescription.l1data  = 0;
            buf->_cacheDescription.l2      = 0;
            buf->_cacheDescription.l3      = 0;
            }
         }
      }

   return buf;
   }

const char *
OMR::X86::CPU::getX86ProcessorVendorId()
   {
   static char buf[13];

   // Terminate the vendor ID with NULL before returning.
   //
   strncpy(buf, self()->queryX86TargetCPUID()->_vendorId, 12);
   buf[12] = '\0';

   return buf;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorSignature()
   {
   return self()->queryX86TargetCPUID()->_processorSignature;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorFeatureFlags()
   {
   return self()->queryX86TargetCPUID()->_featureFlags;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorFeatureFlags2()
   {
   return self()->queryX86TargetCPUID()->_featureFlags2;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorFeatureFlags8()
   {
   return self()->queryX86TargetCPUID()->_featureFlags8;
   }

bool
OMR::X86::CPU::getSupportsHardwareSQRT()
   {
   return true;
   }

bool
OMR::X86::CPU::testOSForSSESupport()
   {
   return false;
   }

bool
OMR::X86::CPU::supportsTransactionalMemoryInstructions()
   {
   flags32_t processorFeatureFlags8(self()->getX86ProcessorFeatureFlags8());
   return processorFeatureFlags8.testAny(TR_RTM);
   }

bool
OMR::X86::CPU::isGenuineIntel()
   {
   return self()->isAtLeast(OMR_PROCESSOR_X86_INTEL_FIRST) && self()->isAtMost(OMR_PROCESSOR_X86_INTEL_LAST);
   }

bool
OMR::X86::CPU::isAuthenticAMD()
   {
   return self()->isAtLeast(OMR_PROCESSOR_X86_AMD_FIRST) && self()->isAtMost(OMR_PROCESSOR_X86_AMD_LAST);
   }

bool
OMR::X86::CPU::requiresLFence()
   {
   return false;  /* Dummy for now, we may need LFENCE in future processors*/
   }

bool
OMR::X86::CPU::supportsFCOMIInstructions()
   {
   return self()->supportsFeature(OMR_FEATURE_X86_FPU) || self()->supportsFeature(OMR_FEATURE_X86_CMOV);
   }

bool
OMR::X86::CPU::supportsMFence()
   {
   return self()->supportsFeature(OMR_FEATURE_X86_SSE2);
   }

bool
OMR::X86::CPU::supportsLFence()
   {
   return self()->supportsFeature(OMR_FEATURE_X86_SSE2);
   }

bool
OMR::X86::CPU::supportsSFence()
   {
   return self()->supportsFeature(OMR_FEATURE_X86_SSE2) || self()->supportsFeature(OMR_FEATURE_X86_MMX);
   }

bool
OMR::X86::CPU::prefersMultiByteNOP()
   {
   return self()->isGenuineIntel() && !self()->is(OMR_PROCESSOR_X86_INTELPENTIUM);
   }

