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

#ifndef SYSINFOHELPERS_H_
#define SYSINFOHELPERS_H_

#include "omrport.h"

#if defined(OMR_OS_WINDOWS) || defined(J9X86) || defined(J9HAMMER)
extern intptr_t
omrsysinfo_get_x86_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);

extern void
omrsysinfo_get_x86_cpuid(uint32_t leaf, uint32_t *cpuInfo);

extern void
omrsysinfo_get_x86_cpuid_ext(uint32_t leaf, uint32_t subleaf, uint32_t *cpuInfo);

extern uint32_t
omrsysinfo_get_cpu_family(uint32_t processorSignature);

extern uint32_t
omrsysinfo_get_cpu_extended_family(uint32_t processorSignature);

extern uint32_t
omrsysinfo_get_cpu_model(uint32_t processorSignature);

extern uint32_t
omrsysinfo_get_cpu_extended_model(uint32_t processorSignature);
#endif /* defined(OMR_OS_WINDOWS) || defined(J9X86) || defined(J9HAMMER) */

#endif /* SYSINFOHELPERS_H_ */
