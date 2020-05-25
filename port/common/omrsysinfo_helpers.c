/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
/**
 * @file
 * @ingroup Port
 * @brief System information
 */

#include "omrsysinfo_helpers.h"
 
#include "omrport.h"
#include "omrporterror.h"
#include "portnls.h"

#include <string.h>

#if defined(OMR_OS_WINDOWS) || defined(J9X86) || defined(J9HAMMER)
#if defined(OMR_OS_WINDOWS)
#include <intrin.h>
#endif /* defined(OMR_OS_WINDOWS) */

/* defines for the CPUID instruction */
#define CPUID_EAX                                         0
#define CPUID_EBX                                         1
#define CPUID_ECX                                         2
#define CPUID_EDX                                         3

#define CPUID_VENDOR_INFO                                 0
#define CPUID_FAMILY_INFO                                 1
#define CPUID_STRUCTURED_EXTENDED_FEATURE_INFO            7

#define CPUID_VENDOR_INTEL                                "GenuineIntel"
#define CPUID_VENDOR_AMD                                  "AuthenticAMD"
#define CPUID_VENDOR_LENGTH                               12

#define CPUID_SIGNATURE_STEPPING                          0x0000000F
#define CPUID_SIGNATURE_MODEL                             0x000000F0
#define CPUID_SIGNATURE_FAMILY                            0x00000F00
#define CPUID_SIGNATURE_PROCESSOR                         0x00003000
#define CPUID_SIGNATURE_EXTENDEDMODEL                     0x000F0000
#define CPUID_SIGNATURE_EXTENDEDFAMILY                    0x0FF00000

#define CPUID_SIGNATURE_STEPPING_SHIFT                    0
#define CPUID_SIGNATURE_MODEL_SHIFT                       4
#define CPUID_SIGNATURE_FAMILY_SHIFT                      8
#define CPUID_SIGNATURE_PROCESSOR_SHIFT                   12
#define CPUID_SIGNATURE_EXTENDEDMODEL_SHIFT               16
#define CPUID_SIGNATURE_EXTENDEDFAMILY_SHIFT              20

#define CPUID_FAMILYCODE_INTELPENTIUM                     0x05
#define CPUID_FAMILYCODE_INTELCORE                        0x06
#define CPUID_FAMILYCODE_INTELPENTIUM4                    0x0F

#define CPUID_MODELCODE_INTELSKYLAKE                      0x55
#define CPUID_MODELCODE_INTELBROADWELL                    0x4F
#define CPUID_MODELCODE_INTELHASWELL_1                    0x3F
#define CPUID_MODELCODE_INTELHASWELL_2                    0x3C
#define CPUID_MODELCODE_INTELIVYBRIDGE_1                  0x3E
#define CPUID_MODELCODE_INTELIVYBRIDGE_2                  0x3A
#define CPUID_MODELCODE_INTELSANDYBRIDGE                  0x2A
#define CPUID_MODELCODE_INTELSANDYBRIDGE_EP               0x2D
#define CPUID_MODELCODE_INTELWESTMERE_EP                  0x2C
#define CPUID_MODELCODE_INTELWESTMERE_EX                  0x2F
#define CPUID_MODELCODE_INTELNEHALEM                      0x1A
#define CPUID_MODELCODE_INTELCORE2_HARPERTOWN             0x17
#define CPUID_MODELCODE_INTELCORE2_WOODCREST_CLOVERTOWN   0x0F

#define CPUID_FAMILYCODE_AMDKSERIES                       0x05
#define CPUID_FAMILYCODE_AMDATHLON                        0x06
#define CPUID_FAMILYCODE_AMDOPTERON                       0x0F

#define CPUID_MODELCODE_AMDK5                             0x04

#define CUPID_EXTENDEDFAMILYCODE_AMDOPTERON               0x06

/**
 * @internal
 * Populates OMRProcessorDesc *desc on Windows and Linux (x86)
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
intptr_t
omrsysinfo_get_x86_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
	uint32_t CPUInfo[4] = {0};
	char vendor[12];
	uint32_t familyCode = 0;
	uint32_t processorSignature = 0;

	desc->processor = OMR_PROCESSOR_X86_UNKNOWN;

	/* vendor */
	omrsysinfo_get_x86_cpuid(CPUID_VENDOR_INFO, CPUInfo);
	memcpy(vendor + 0, &CPUInfo[CPUID_EBX], sizeof(uint32_t));
	memcpy(vendor + 4, &CPUInfo[CPUID_EDX], sizeof(uint32_t));
	memcpy(vendor + 8, &CPUInfo[CPUID_ECX], sizeof(uint32_t));

	/* family and model */
	omrsysinfo_get_x86_cpuid(CPUID_FAMILY_INFO, CPUInfo);
	processorSignature = CPUInfo[CPUID_EAX];
	familyCode = (processorSignature & CPUID_SIGNATURE_FAMILY) >> CPUID_SIGNATURE_FAMILY_SHIFT;
	if (0 == strncmp(vendor, CPUID_VENDOR_INTEL, CPUID_VENDOR_LENGTH)) {
		switch (familyCode) {
		case CPUID_FAMILYCODE_INTELPENTIUM:
			desc->processor = OMR_PROCESSOR_X86_INTELPENTIUM;
			break;
		case CPUID_FAMILYCODE_INTELCORE:
		{
			uint32_t modelCode  = omrsysinfo_get_cpu_model(processorSignature);
			uint32_t extendedModelCode = omrsysinfo_get_cpu_extended_model(processorSignature);
			uint32_t totalModelCode = modelCode + (extendedModelCode << 4);

			switch (totalModelCode) {
			case CPUID_MODELCODE_INTELSKYLAKE:
				desc->processor = OMR_PROCESSOR_X86_INTELSKYLAKE;
				break;
			case CPUID_MODELCODE_INTELBROADWELL:
				desc->processor = OMR_PROCESSOR_X86_INTELBROADWELL;
				break;
			case CPUID_MODELCODE_INTELHASWELL_1:
			case CPUID_MODELCODE_INTELHASWELL_2:
				desc->processor = OMR_PROCESSOR_X86_INTELHASWELL;
				break;
			case CPUID_MODELCODE_INTELIVYBRIDGE_1:
			case CPUID_MODELCODE_INTELIVYBRIDGE_2:
				desc->processor = OMR_PROCESSOR_X86_INTELIVYBRIDGE;
				break;
			case CPUID_MODELCODE_INTELSANDYBRIDGE:
			case CPUID_MODELCODE_INTELSANDYBRIDGE_EP:
				desc->processor = OMR_PROCESSOR_X86_INTELSANDYBRIDGE;
				break;
			case CPUID_MODELCODE_INTELWESTMERE_EP:
			case CPUID_MODELCODE_INTELWESTMERE_EX:
				desc->processor = OMR_PROCESSOR_X86_INTELWESTMERE;
				break;
			case CPUID_MODELCODE_INTELNEHALEM:
				desc->processor = OMR_PROCESSOR_X86_INTELNEHALEM;
				break;
			case CPUID_MODELCODE_INTELCORE2_HARPERTOWN:
			case CPUID_MODELCODE_INTELCORE2_WOODCREST_CLOVERTOWN:
				desc->processor = OMR_PROCESSOR_X86_INTELCORE2;
				break;
			default:
				desc->processor = OMR_PROCESSOR_X86_INTELP6;
				break;
			}
			break;
		}
		case CPUID_FAMILYCODE_INTELPENTIUM4:
			desc->processor = OMR_PROCESSOR_X86_INTELPENTIUM4;
			break;
		}
	} else if (0 == strncmp(vendor, CPUID_VENDOR_AMD, CPUID_VENDOR_LENGTH)) {
		switch (familyCode) {
		case CPUID_FAMILYCODE_AMDKSERIES:
		{
			uint32_t modelCode  = omrsysinfo_get_cpu_model(processorSignature);
			if (modelCode < CPUID_MODELCODE_AMDK5) {
				desc->processor = OMR_PROCESSOR_X86_AMDK5;
			}
			else {
				desc->processor = OMR_PROCESSOR_X86_AMDK6;
			}
			break;
		}
		case CPUID_FAMILYCODE_AMDATHLON:
			desc->processor = OMR_PROCESSOR_X86_AMDATHLONDURON;
			break;
		case CPUID_FAMILYCODE_AMDOPTERON:
		{
			uint32_t extendedFamilyCode  = omrsysinfo_get_cpu_extended_family(processorSignature);
			if (extendedFamilyCode < CUPID_EXTENDEDFAMILYCODE_AMDOPTERON) {
				desc->processor = OMR_PROCESSOR_X86_AMDOPTERON;
			}
			else {
				desc->processor = OMR_PROCESSOR_X86_AMDFAMILY15H;
			}
			break;
		}
		}
	}

	desc->physicalProcessor = desc->processor;

	/* features */
	desc->features[0] = CPUInfo[CPUID_EDX];
	desc->features[1] = CPUInfo[CPUID_ECX];
	desc->features[2] = 0; /* reserved for future expansion */
	/* extended features */
	omrsysinfo_get_x86_cpuid_ext(CPUID_STRUCTURED_EXTENDED_FEATURE_INFO, 0, CPUInfo); /* 0x0 is the only valid subleaf value for this leaf */
	desc->features[3] = CPUInfo[CPUID_EBX]; /* Structured Extended Feature Flags in EBX */

	desc->features[4] = 0; /* reserved for future expansion */
	return 0;
}

/**
 * Assembly code to get the register data from CPUID instruction
 * This function executes the CPUID instruction based on which we can detect
 * if the environment is virtualized or not, and also get the Hypervisor Vendor
 * Name based on the same instruction. The leaf value specifies what information
 * to return.
 *
 * @param[in]   leaf        The leaf value to the CPUID instruction and the value
 *                          in EAX when CPUID is called. A value of 0x1 returns basic
 *                          information including feature support.
 * @param[out]  cpuInfo     Reference to the an integer array which holds the data
 *                          of EAX,EBX,ECX and EDX registers.
 *              cpuInfo[0]  To hold the EAX register data, value in this register at
 *                          the time of CPUID tells what information to return
 *                          EAX=0x1,returns the processor Info and feature bits
 *                          in EBX,ECX,EDX registers.
 *                          EAX=0x40000000 returns the Hypervisor Vendor Names
 *                          in the EBX,ECX,EDX registers.
 *              cpuInfo[1]  For EAX = 0x40000000 hold first 4 characters of the
 *                          Hypervisor Vendor String
 *              cpuInfo[2]  For EAX = 0x1, the 31st bit of ECX tells if its
 *                          running on Hypervisor or not,For EAX = 0x40000000 holds the second
 *                          4 characters of the the Hypervisor Vendor String
 *              cpuInfo[3]  For EAX = 0x40000000 hold the last 4 characters of the
 *                          Hypervisor Vendor String
 *
 */
void
omrsysinfo_get_x86_cpuid(uint32_t leaf, uint32_t *cpuInfo)
{
	cpuInfo[0] = leaf;

#if defined(OMR_OS_WINDOWS)
	/* Specific CPUID instruction available in Windows */
	__cpuid(cpuInfo, cpuInfo[0]);

#elif defined(LINUX) || defined(OSX)
#if defined(J9X86)
	__asm volatile(
		"mov %%ebx, %%edi;"
		"cpuid;"
		"mov %%ebx, %%esi;"
		"mov %%edi, %%ebx;"
		:"+a" (cpuInfo[0]), "=S" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
		: /* None */
		:"edi");

#elif defined(J9HAMMER)
	__asm volatile(
		"cpuid;"
		:"+a" (cpuInfo[0]), "=b" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
		);
#endif /* defined(LINUX) || defined(OSX) */
#endif /* defined(OMR_OS_WINDOWS) */
}

/**
 * Similar to omrsysinfo_get_x86_cpuid() above, but with a second subleaf parameter for the
 * leaves returned by the 'cpuid' instruction which are further divided into
 * subleaves.
 * 
 * @param[in]   leaf        Value in EAX when cpuid is called to determine what info
 *                          is returned.
 *              subleaf     Value in ECX when cpuid is called which is needed by some
 *                          leafs returned in order to further specify what is returned
 * @param[out]  cpuInfo     Reference to the an integer array which holds the data
 *                          of EAX,EBX,ECX and EDX registers returned by cpuid.
 *                          cpuInfo[0] holds EAX and cpuInfo[4] holds EDX.
 */
void
omrsysinfo_get_x86_cpuid_ext(uint32_t leaf, uint32_t subleaf, uint32_t *cpuInfo)
{
	cpuInfo[0] = leaf;
	cpuInfo[2] = subleaf;

#if defined(OMR_OS_WINDOWS)
	/* Specific CPUID instruction available in Windows */
	__cpuidex(cpuInfo, cpuInfo[0], cpuInfo[2]);

#elif defined(LINUX) || defined(OSX)
#if defined(J9X86)
	__asm volatile(
		"mov %%ebx, %%edi;"
		"cpuid;"
		"mov %%ebx, %%esi;"
		"mov %%edi, %%ebx;"
		:"+a" (cpuInfo[0]), "=S" (cpuInfo[1]), "+c" (cpuInfo[2]), "=d" (cpuInfo[3])
		: /* None */
		:"edi");

#elif defined(J9HAMMER)
	__asm volatile(
		"cpuid;"
		:"+a" (cpuInfo[0]), "=b" (cpuInfo[1]), "+c" (cpuInfo[2]), "=d" (cpuInfo[3])
		);
#endif /* defined(LINUX) || defined(OSX) */
#endif /* defined(OMR_OS_WINDOWS) */
}

uint32_t
omrsysinfo_get_cpu_family(uint32_t processorSignature)
{
	return (processorSignature & CPUID_SIGNATURE_FAMILY) >> CPUID_SIGNATURE_FAMILY_SHIFT;
}

uint32_t
omrsysinfo_get_cpu_extended_family(uint32_t processorSignature)
{
	return (processorSignature & CPUID_SIGNATURE_EXTENDEDFAMILY) >> CPUID_SIGNATURE_EXTENDEDFAMILY_SHIFT;
}

uint32_t
omrsysinfo_get_cpu_model(uint32_t processorSignature)
{
	return (processorSignature & CPUID_SIGNATURE_MODEL) >> CPUID_SIGNATURE_MODEL_SHIFT;
}

uint32_t
omrsysinfo_get_cpu_extended_model(uint32_t processorSignature)
{
	return (processorSignature & CPUID_SIGNATURE_EXTENDEDMODEL) >> CPUID_SIGNATURE_EXTENDEDMODEL_SHIFT;
}

#endif /* defined(OMR_OS_WINDOWS) || defined(J9X86) || defined(J9HAMMER) */
