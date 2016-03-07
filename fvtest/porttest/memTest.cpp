/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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
 * Verify port library memory management.
 *
 * Ensure the port library is properly setup to run string operations.
 */
TEST(PortMemTest, Initialization)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	/* Verify that the memory management function pointers are non NULL */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibary, it is not re-entrant safe
	 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_startup);

	/*  Not tested, implementation dependent.  No known functionality */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_shutdown);

	/* omrmem_test2 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_allocate_memory);

	/* omrmem_test1 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_free_memory);

	/* omrmem_test4 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_reallocate_memory);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_allocate_memory32);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_free_memory32);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->mem_walk_categories);
}
