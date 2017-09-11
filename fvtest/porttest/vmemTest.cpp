/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <limits.h>
#include "omrTest.h"
#include "omrport.h"
#include "portTestHelpers.hpp"

extern PortTestEnvironment *portTestEnv;

/**
 * Verify virtual memory functions
 *
 */
/**
 * Sanity test of function to obtain available physical memory.
 */
TEST(PortVMemTest, omrvmem_get_available_physical_memory)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uint64_t freePhysicalMemorySize = 0;
	int32_t status = omrvmem_get_available_physical_memory(&freePhysicalMemorySize);
#if defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(OSX)
	ASSERT_EQ(0, status) << "Non-zero status from omrvmem_get_available_physical_memory";
	portTestEnv->log("freePhysicalMemorySize = %llu\n", freePhysicalMemorySize);
#else /*  defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(OSX) */
	ASSERT_EQ(OMRPORT_ERROR_VMEM_NOT_SUPPORTED, status) << "omrvmem_get_available_physical_memory should not be supported";
#endif /*  defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(OSX) */
}
