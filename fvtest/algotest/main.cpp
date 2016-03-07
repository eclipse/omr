/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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

#include "omrTest.h"
#include "omrport.h"
#include "testEnvironment.hpp"

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
