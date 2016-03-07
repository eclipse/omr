/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#include <limits.h>
#include "omrTest.h"
#include "omrport.h"
#include "portTestHelpers.hpp"

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
	omrtty_printf("freePhysicalMemorySize = %llu\n", freePhysicalMemorySize);
#else /*  defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(OSX) */
	ASSERT_EQ(OMRPORT_ERROR_VMEM_NOT_SUPPORTED, status) << "omrvmem_get_available_physical_memory should not be supported";
#endif /*  defined(LINUX) || defined(AIXPPC) || defined(WIN32) || defined(OSX) */
}
