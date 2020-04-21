/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

/* defines for the CPUID instruction */
#define CPUID_EAX                                         0
#define CPUID_EBX                                         1
#define CPUID_ECX                                         2
#define CPUID_EDX                                         3

#define CPUID_VENDOR_INFO                                 0
#define CPUID_FAMILY_INFO                                 1

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
	cpuid(CPUInfo, CPUID_VENDOR_INFO);
	memcpy(vendor + 0, &CPUInfo[CPUID_EBX], sizeof(uint32_t));
	memcpy(vendor + 4, &CPUInfo[CPUID_EDX], sizeof(uint32_t));
	memcpy(vendor + 8, &CPUInfo[CPUID_ECX], sizeof(uint32_t));

	/* family and model */
	cpuid(CPUInfo, CPUID_FAMILY_INFO);
	processorSignature = CPUInfo[CPUID_EAX];
	familyCode = (processorSignature & CPUID_SIGNATURE_FAMILY) >> CPUID_SIGNATURE_FAMILY_SHIFT;
	if (0 == strncmp(vendor, CPUID_VENDOR_INTEL, CPUID_VENDOR_LENGTH)) {
		switch (familyCode) {
		case CPUID_FAMILYCODE_INTELPENTIUM:
			desc->processor = OMR_PROCESSOR_X86_INTELPENTIUM;
			break;
		case CPUID_FAMILYCODE_INTELCORE:
		{
			uint32_t modelCode  = (processorSignature & CPUID_SIGNATURE_MODEL) >> CPUID_SIGNATURE_MODEL_SHIFT;
			uint32_t extendedModelCode = (processorSignature & CPUID_SIGNATURE_EXTENDEDMODEL) >> CPUID_SIGNATURE_EXTENDEDMODEL_SHIFT;
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
			uint32_t modelCode  = (processorSignature & CPUID_SIGNATURE_FAMILY) >> CPUID_SIGNATURE_MODEL_SHIFT;
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
			uint32_t extendedFamilyCode  = (processorSignature & CPUID_SIGNATURE_EXTENDEDFAMILY) >> CPUID_SIGNATURE_EXTENDEDFAMILY_SHIFT;
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

	cpuidex(CPUInfo, 7, 0);
	desc->features[2] = CPUInfo[CPUID_EBX];

	return 0;
}
#endif /* defined(OMR_OS_WINDOWS) || defined(J9X86) || defined(J9HAMMER) */
