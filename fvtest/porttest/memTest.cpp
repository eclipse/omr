/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
