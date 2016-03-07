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

#if !defined(PORTTESTHELPERS_H_INCLUDED)
#define PORTTESTHELPERS_H_INCLUDED

#include <string.h>

#include "omrport.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "testEnvironment.hpp"

#define APPEND_ERRORINFO  "\tlastErrorNumber: " << omrerror_last_error_number() << "\n\tLastErrorMessage: " << (char *)omrerror_last_error_message() << "\n"

/*Nonfatal assertion macros*/
#define OMRTEST_EXPECT_NOT_NULL(funcPointer)	EXPECT_FALSE(NULL == funcPointer) << APPEND_ERRORINFO
#define OMRTEST_EXPECT_FAILURE(info)	ADD_FAILURE() << info << APPEND_ERRORINFO

/*Fatal assertion macros*/
#define OMRTEST_ASSERT_TRUE(condition, info)	ASSERT_TRUE(condition) << info << APPEND_ERRORINFO

class PortTestEnvironment: public PortEnvironment
{
public:
	char *hypervisor;
	BOOLEAN negative;

	PortTestEnvironment(int argc, char **argv)
		: PortEnvironment(argc, argv)
		, hypervisor(NULL)
		, negative(FALSE)
	{
		for (int i = 1; i < argc; i++) {
			if (strStartsWith(argv[i], "-hypervisor:")) {
				hypervisor = &(argv[i][12]);
			} else if (strcmp(argv[i], "-negative") == 0) {
				/* Option "-negative" does /not/ take sub-options so we don't do startsWith() but an
				 * exact match. Should this need sub-options, use startsWith instead.
				 */
				negative = TRUE;
			}
		}
	}

	void initPort()
	{
		SetUp();
	}

	void shutdownPort()
	{
		TearDown();
	}
};

extern PortTestEnvironment *portTestEnv;

#endif /* PORTTESTHELPERS_H_INCLUDED */
