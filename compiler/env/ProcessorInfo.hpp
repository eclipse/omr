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

#ifndef TR_PROCESSOR_INFO_INCL
#define TR_PROCESSOR_INFO_INCL

#include <stdint.h>  // for uint32_t


enum TR_S390MachineType
   {
   TR_UNDEFINED_S390_MACHINE  =  0,
   TR_G5                      =  9672,
   TR_MULTIPRISE7000          =  7060,
   TR_FREEWAY                 =  2064,  // z900
   TR_Z800                    =  2066,  // z800 - entry-level, less powerful variant of the z900
   TR_MIRAGE                  =  1090,
   TR_MIRAGE2                 =  1091,
   TR_TREX                    =  2084,  // z990
   TR_Z890                    =  2086,  // z890 - kneecapped version of z990
   TR_GOLDEN_EAGLE            =  2094,  // z9
   TR_DANU_GA2                =  2094,  // doesn't change from GoldenEagle
   TR_Z9BC                    =  2096,  // z9 BC
   TR_Z10                     =  2097,
   TR_Z10BC                   =  2098,  // zMR
   TR_ZG                      =  2817,  // zGryphon HE - z196
   TR_ZGMR                    =  2818,  // zGryphon MR - z114
   TR_ZG_RESERVE              =  2819,  // reserve for zGryphon
   TR_ZEC12                   =  2827,  // zEC12 / zHelix
   TR_ZEC12MR                 =  2828,  // zHelix MR
   TR_ZEC12_RESERVE           =  2829,  // reserve for zHelix
   TR_Z13                     =  2964,  // z13
   TR_Z13s                    =  2965,  // z13s
   TR_ZNEXT                   =  3906,  // zNext
   TR_ZNEXTs                  =  3907,  // zNexts

   TR_ZH                      =  2458,  // reserve for zHybrid
   TR_DATAPOWER               =  2462,  // reserve for DataPower on 2458
   TR_ZH_RESERVE1             =  2459,  // reserve for zHybrid
   TR_ZH_RESERVE2             =  2461,  // reserve for zHybrid
   };

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
   };

enum TR_X86ProcessorVendors
   {
   TR_AuthenticAMD                  = 0x01,
   TR_GenuineIntel                  = 0x02,
   TR_UnknownVendor                 = 0x04
   };

enum TR_X86ProcessorFeatures
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

enum TR_X86ProcessorFeatures2
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

enum TR_X86ProcessorFeatures8
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
   // Reserved by Intel       = 0x00010000,
   // Reserved by Intel       = 0x00020000,
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
   // Reserved by Intel       = 0x10000000,
   TR_SHA                     = 0x20000000,
   // Reserved by Intel       = 0x40000000,
   // Reserved by Intel       = 0x80000000,
   };

enum TR_ProcessorDescription
   {
   TR_ProcessorUnknown          = 0x00000000,
   TR_ProcessorIntelPentium     = 0x00000001,
   TR_ProcessorIntelP6          = 0x00000002,
   TR_ProcessorIntelPentium4    = 0x00000003,

   TR_ProcessorAMDK5            = 0x00000004,
   TR_ProcessorAMDK6            = 0x00000005,
   TR_ProcessorAMDAthlonDuron   = 0x00000006,
   TR_ProcessorAMDOpteron       = 0x00000007,

   TR_ProcessorIntelCore2       = 0x00000008,
   TR_ProcessorIntelTulsa       = 0x00000009,
   TR_ProcessorIntelNehalem     = 0x0000000a,

   TR_ProcessorAMDFamily15h     = 0x0000000b,
   TR_ProcessorIntelWestmere    = 0x0000000c,
   TR_ProcessorIntelSandyBridge = 0x0000000d,
   TR_ProcessorIntelIvyBridge   = 0x0000000e,
   TR_ProcessorIntelHaswell     = 0x0000000f,
   TR_ProcessorIntelBroadwell   = 0x00000010,
   };

#endif
