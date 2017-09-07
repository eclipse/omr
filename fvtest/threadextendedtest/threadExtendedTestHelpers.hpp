/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#ifndef THREADEXTENDEDTESTHELPERS_HPP_INCLUDED
#define THREADEXTENDEDTESTHELPERS_HPP_INCLUDED

#include <string.h>

#include "omrport.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "testEnvironment.hpp"

class ThreadExtendedTestEnvironment: public BaseEnvironment
{
/*
 * Data members
 */
private:
protected:
public:
	uint32_t grindTime;

/*
* Function members
*/
private:
protected:
public:
	ThreadExtendedTestEnvironment(int argc, char **argv, OMRPortLibrary *portLibrary) :
		BaseEnvironment(argc, argv),
		grindTime(0)
	{
		for (int i = 1; i < argc; i++) {
			if (strncmp(argv[i], "-grind=", strlen("-grind=")) == 0) {
				sscanf(&argv[i][strlen("-grind=")], "%u", &grindTime);
			}
		}
		portLib = portLibrary;
	}
};

/**
 * Initialize port library and attach thread
 */
void thrExtendedTestSetUp(OMRPortLibrary *portLibrary);

/**
 * Shutdown port library and detach thread
 */
void thrExtendedTestTearDown(OMRPortLibrary *portLibrary);

extern ThreadExtendedTestEnvironment *omrTestEnv;

#endif /* THREADEXTENDEDTESTHELPERS_HPP_INCLUDED */
