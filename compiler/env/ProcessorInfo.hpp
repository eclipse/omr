/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef TR_PROCESSOR_INFO_INCL
#define TR_PROCESSOR_INFO_INCL

#include <stdint.h>

// Private struct for interfacing with the assembly helper.
// WARNING: Possible alignment issues with non-MS compilers.
//
struct TR_X86CPUIDBuffer
   {
   char     _vendorId[12];          // not NULL-terminated
   uint32_t _processorSignature;
   uint32_t _brandIdEtc;
   uint32_t _featureFlags;
   uint32_t _featureFlags2;
   struct
      {
      uint32_t l1instr;
      uint32_t l1data;
      uint32_t l2;
      uint32_t l3;
      } _cacheDescription;
   uint32_t _featureFlags8;
   uint32_t _featureFlags10;
   };

enum TR_X86ProcessorVendors
   {
   TR_AuthenticAMD                  = 0x01,
   TR_GenuineIntel                  = 0x02,
   TR_UnknownVendor                 = 0x04
   };

enum TR_X86_CPUID_edx_eax01
   {
   TR_BuiltInFPU                    = 0x00000001,
   TR_VirtualModeExtension          = 0x00000002,
   TR_DebuggingExtension            = 0x00000004,
   TR_PageSizeExtension             = 0x00000008,
   TR_RDTSCInstruction              = 0x00000010,
   TR_ModelSpecificRegisters        = 0x00000020,
   TR_PhysicalAddressExtension      = 0x00000040,
   TR_MachineCheckException         = 0x00000080,
   TR_CMPXCHG8BInstruction          = 0x00000100,
   TR_APICHardware                  = 0x00000200,
   // Reserved by Intel             = 0x00000400,
   TR_FastSystemCalls               = 0x00000800,
   TR_MemoryTypeRangeRegisters      = 0x00001000,
   TR_PageGlobalFlag                = 0x00002000,
   TR_MachineCheckArchitecture      = 0x00004000,
   TR_CMOVInstructions              = 0x00008000,
   TR_PageAttributeTable            = 0x00010000,
   TR_36BitPageSizeExtension        = 0x00020000,
   TR_ProcessorSerialNumber         = 0x00040000, // GenuineIntel only
   TR_CLFLUSHInstruction            = 0x00080000, // GenuineIntel only
   // Reserved by Intel             = 0x00100000,
   TR_DebugTraceStore               = 0x00200000, // GenuineIntel only
   TR_ACPIRegisters                 = 0x00400000, // GenuineIntel only
   TR_MMXInstructions               = 0x00800000,
   TR_FastFPSavesRestores           = 0x01000000,
   TR_SSE                           = 0x02000000,
   TR_SSE2                          = 0x04000000, // GenuineIntel Only
   TR_SelfSnoop                     = 0x08000000, // GenuineIntel Only
   TR_HyperThreading                = 0x10000000, // GenuineIntel Only
   TR_ThermalMonitor                = 0x20000000, // GenuineIntel Only
   // Reserved by Intel             = 0x40000000, // IA64-specific
   // Reserved by Intel             = 0x80000000
   TR_X86ProcessorInfoInitialized   = 0x80000000  // FIXME: Using a reserved bit for our purposes.
   };

inline uint32_t getFeatureFlagsMask()
   {
   return  TR_BuiltInFPU
         | TR_CMPXCHG8BInstruction
         | TR_CMOVInstructions
         | TR_MMXInstructions
         | TR_SSE
         | TR_SSE2;
   }

enum TR_X86_CPUID_ecx_eax01
   {
   TR_SSE3                          = 0x00000001,
   TR_CLMUL                         = 0x00000002,
   TR_DTES64                        = 0x00000004,
   TR_Monitor                       = 0x00000008,
   TR_DSCPL                         = 0x00000010,
   TR_VMX                           = 0x00000020,
   TR_SMX                           = 0x00000040,
   TR_EIST                          = 0x00000080,
   TR_TM2                           = 0x00000100,
   TR_SSSE3                         = 0x00000200,
   TR_CNXT_ID                       = 0x00000400,
   TR_SDBG                          = 0x00000800,
   TR_FMA                           = 0x00001000,
   TR_CMPXCHG16BInstruction         = 0x00002000,
   TR_xTPRUpdateControl             = 0x00004000,
   TR_PDCM                          = 0x00008000,
   // Reserved by Intel             = 0x00010000,
   TR_PCID                          = 0x00020000,
   TR_DCA                           = 0x00040000,
   TR_SSE4_1                        = 0x00080000,
   TR_SSE4_2                        = 0x00100000,
   TR_x2APIC                        = 0x00200000,
   TR_MOVBE                         = 0x00400000,
   TR_POPCNT                        = 0x00800000,
   TR_TSC_Deadline                  = 0x01000000,
   TR_AESNI                         = 0x02000000,
   TR_XSAVE                         = 0x04000000,
   TR_OSXSAVE                       = 0x08000000,
   TR_AVX                           = 0x10000000,
   TR_F16C                          = 0x20000000,
   TR_RDRAND                        = 0x40000000,
   // Not used by Intel             = 0x80000000,
   };

inline uint32_t getFeatureFlags2Mask()
   {
   return  TR_SSE3
         | TR_SSSE3
         | TR_SSE4_1
         | TR_SSE4_2
         | TR_POPCNT
         | TR_AESNI
         | TR_OSXSAVE
         | TR_AVX
         | TR_FMA;
   }

enum TR_X86_CPUID_ebx_eax07_ecx00
   {
   TR_FSGSBASE                = 0x00000001,
   TR_IA32_TSC_ADJUST         = 0x00000002,
   TR_SGX                     = 0x00000004,
   TR_BMI1                    = 0x00000008,
   TR_HLE                     = 0x00000010,
   TR_AVX2                    = 0x00000020,
   TR_FDP_EXCPTN_ONLY         = 0x00000040,
   TR_SMEP                    = 0x00000080,
   TR_BMI2                    = 0x00000100,
   TR_ERMSB                   = 0x00000200,
   TR_INVPCID                 = 0x00000400,
   TR_RTM                     = 0x00000800,
   TR_RDT_M                   = 0x00001000,
   TR_DeprecatesFPUCSDS       = 0x00002000,
   TR_MPX                     = 0x00004000,
   TR_RDT_A                   = 0x00008000,
   TR_AVX512F                 = 0x00010000,
   TR_AVX512DQ                = 0x00020000,
   TR_RDSEED                  = 0x00040000,
   TR_ADX                     = 0x00080000,
   TR_SMAP                    = 0x00100000,
   // Reserved by Intel       = 0x00200000,
   // Reserved by Intel       = 0x00400000,
   TR_CLFLUSHOPT              = 0x00800000,
   TR_CLWB                    = 0x01000000,
   TR_IntelProcessorTrace     = 0x02000000,
   // Reserved by Intel       = 0x04000000,
   // Reserved by Intel       = 0x08000000,
   TR_AVX512CD                = 0x10000000,
   TR_SHA                     = 0x20000000,
   TR_AVX512BW                = 0x40000000,
   TR_AVX512VL                = 0x80000000,
   };

inline uint32_t getFeatureFlags8Mask()
   {
   return  TR_HLE
         | TR_RTM
         | TR_CLWB
         | TR_AVX2
         | TR_AVX512F
         | TR_AVX512VL
         | TR_AVX512BW
         | TR_AVX512DQ
         | TR_AVX512CD;
   }

enum TR_X86_CPUID_ecx_eax07_ecx00
   {
   TR_PREFETCHWT1         = 0x00000001,
   TR_AVX512_VBMI         = 0x00000002,
   TR_UMIP                = 0x00000004,
   TR_PKU                 = 0x00000008,
   TR_OSPKE               = 0x00000010,
   TR_WAITPKG             = 0x00000020,
   TR_AVX512_VBMI2        = 0x00000040,
   TR_CET_SS              = 0x00000080,
   TR_GFNI                = 0x00000100,
   TR_VAES                = 0x00000200,
   TR_VPCLMULQDQ          = 0x00000400,
   TR_AVX512_VNNI         = 0x00000800,
   TR_AVX512_BITALG       = 0x00001000,
   TR_TME_EN              = 0x00002000,
   TR_AVX512_VPOPCNTDQ    = 0x00004000,
   // Reserved by Intel   = 0x00008000,
   TR_LA57                = 0x00010000,
   TR_MAWAU_0             = 0x00020000,
   TR_MAWAU_1             = 0x00040000,
   TR_MAWAU_2             = 0x00080000,
   TR_MAWAU_3             = 0x00100000,
   TR_MAWAU_4             = 0x00200000,
   TR_RDPID               = 0x00400000,
   TR_KL                  = 0x00800000,
   TR_BUS_LOCK_DETECT     = 0x01000000,
   TR_CLDEMOTE            = 0x02000000,
   // Reserved by Intel   = 0x04000000,
   TR_MOVDIRI             = 0x08000000,
   TR_MOVDIR64B           = 0x10000000,
   TR_ENQCMD              = 0x20000000,
   TR_SGX_LC              = 0x40000000,
   TR_PKS                 = 0x80000000,
   };

inline uint32_t getFeatureFlags10Mask()
   {
   return  TR_AVX512_VBMI2
         | TR_AVX512_BITALG
         | TR_AVX512_VPOPCNTDQ;
   }


enum TR_ProcessorDescription
   {
   TR_ProcessorUnknown             = 0x00000000,
   TR_ProcessorIntelPentium        = 0x00000001,
   TR_ProcessorIntelP6             = 0x00000002,
   TR_ProcessorIntelPentium4       = 0x00000003,

   TR_ProcessorAMDK5               = 0x00000004,
   TR_ProcessorAMDK6               = 0x00000005,
   TR_ProcessorAMDAthlonDuron      = 0x00000006,
   TR_ProcessorAMDOpteron          = 0x00000007,

   TR_ProcessorIntelCore2          = 0x00000008,
   TR_ProcessorIntelTulsa          = 0x00000009,
   TR_ProcessorIntelNehalem        = 0x0000000a,

   TR_ProcessorAMDFamily15h        = 0x0000000b,
   TR_ProcessorIntelWestmere       = 0x0000000c,
   TR_ProcessorIntelSandyBridge    = 0x0000000d,
   TR_ProcessorIntelIvyBridge      = 0x0000000e,
   TR_ProcessorIntelHaswell        = 0x0000000f,
   TR_ProcessorIntelBroadwell      = 0x00000010,
   TR_ProcessorIntelSkylake        = 0x00000011,
   TR_ProcessorIntelCascadeLake    = 0x00000012,
   TR_ProcessorIntelIceLake        = 0x00000013,
   TR_ProcessorIntelSapphireRapids = 0x00000014,
   TR_ProcessorIntelEmeraldRapids  = 0x00000015,
   };

#endif
