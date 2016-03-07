/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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
 * Verify port library heap sub-allocator.
 *
 * Ensure the port library is properly setup to run heap operations.
 */
TEST(PortHeapTest, Initialization)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->heap_create);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->heap_allocate);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->heap_free);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->heap_reallocate);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->heap_query_size);
}
