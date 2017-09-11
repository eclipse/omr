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

#include "omrTest.h"
#include "omrport.h"

#include "sigTestHelpers.hpp"

extern "C" {
int testMain(int argc, char **argv, char **envp);
}

PortEnvironment *omrTestEnv;


int
testMain(int argc, char **argv, char **envp)
{
	::testing::InitGoogleTest(&argc, argv);

	ATTACH_J9THREAD();
	omrTestEnv = (PortEnvironment *)testing::AddGlobalTestEnvironment(new PortEnvironment(argc, argv));
	int result = RUN_ALL_TESTS();
	DETACH_J9THREAD();
	return result;
}
