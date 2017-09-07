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
