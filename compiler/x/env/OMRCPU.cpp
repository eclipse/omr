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
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "env/CPU.hpp"
#include "env/JitConfig.hpp"
#include "env/ProcessorInfo.hpp"
#include "x/runtime/X86Runtime.hpp"

TR_X86CPUIDBuffer *
OMR::X86::CPU::queryX86TargetCPUID(TR::Compilation *comp)
   {
#ifdef J9_PROJECT_SPECIFIC
   return NULL;
#else
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
#endif
   }


const char *
OMR::X86::CPU::getX86ProcessorVendorId(TR::Compilation *comp)
   {
   static char buf[13];

   // Terminate the vendor ID with NULL before returning.
   //
   strncpy(buf, self()->queryX86TargetCPUID(comp)->_vendorId, 12);
   buf[12] = '\0';

   return buf;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorSignature(TR::Compilation *comp)
   {
   return self()->queryX86TargetCPUID(comp)->_processorSignature;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorFeatureFlags(TR::Compilation *comp)
   {
   return self()->queryX86TargetCPUID(comp)->_featureFlags;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorFeatureFlags2(TR::Compilation *comp)
   {
   return self()->queryX86TargetCPUID(comp)->_featureFlags2;
   }

uint32_t
OMR::X86::CPU::getX86ProcessorFeatureFlags8(TR::Compilation *comp)
   {
   return self()->queryX86TargetCPUID(comp)->_featureFlags8;
   }

bool
OMR::X86::CPU::testOSForSSESupport(TR::Compilation *comp)
   {
   return false;
   }

bool
OMR::X86::CPU::getX86OSSupportsSSE(TR::Compilation *comp)
   {
   // If the FXSR (bit 24) and SSE (bit 25) bits in the feature flags are not set, SSE is unavailable
   // on this processor.
   //
   uint32_t flags = self()->getX86ProcessorFeatureFlags(comp);

   if ((flags & 0x03000000) != 0x03000000)
      return false;

   return self()->testOSForSSESupport(comp);
   }

bool
OMR::X86::CPU::getX86OSSupportsSSE2(TR::Compilation *comp)
   {
   // If the FXSR (bit 24) and SSE2 (bit 26) bits in the feature flags are not set, SSE is unavailable
   // on this processor.
   //
   uint32_t flags = self()->getX86ProcessorFeatureFlags(comp);
   if ((flags & 0x05000000) != 0x05000000)
      return false;

   return self()->testOSForSSESupport(comp);
   }

bool
OMR::X86::CPU::getX86SupportsTM(TR::Compilation *comp)
   {
   uint32_t flags8 = self()->getX86ProcessorFeatureFlags8(comp);
   if ((flags8 & TR_RTM) != 0x00000000)
         return true;
   else return false;
   }
