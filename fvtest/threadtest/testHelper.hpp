/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#if !defined(TESTHELPER_HPP_INCLUDED)
#define TESTHELPER_HPP_INCLUDED

#include "omrTest.h"
#include "omrTestHelpers.h"
#include "testEnvironment.hpp"

class ThreadTestEnvironment: public PortEnvironment
{
/*
 * Data members
 */
private:
protected:
public:
	bool realtime;

/*
* Function members
*/
private:
protected:
public:
	ThreadTestEnvironment(int argc, char **argv)
		: PortEnvironment(argc, argv)
		, realtime(false)
	{
		for (int i = 1; i < argc; i++) {
			if (0 == strcmp(argv[i], "-realtime")) {
				realtime = true;
				break;
			}
		}
	}
};

extern ThreadTestEnvironment *omrTestEnv;

#endif /* TESTHELPER_HPP_INCLUDED */
