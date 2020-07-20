/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrTest.h"
#include "omrport.h"
#include "portTestHelpers.hpp"
#include "testHelpers.hpp"

extern int omrfile_runTests(struct OMRPortLibrary *portLibrary, char *argv0, char *omrfile_child, BOOLEAN asynch); /** @see omrfileTest.c::omrfile_runTests */
extern int omrsig_runTests(struct OMRPortLibrary *portLibrary, char *exeName, char *argument); /** @see omrsignalTest.c::omrsig_runTests */
extern int omrmmap_runTests(struct OMRPortLibrary *portLibrary, char *argv0, char *omrmmap_child);

PortTestEnvironment *portTestEnv;

extern "C" int
omr_main_entry(int argc, char **argv, char **envp)
{
	bool isChild = false;
	bool isVerbose = false;
	bool earlyExit = false;
	int i = 0;
	char testName[99] = "";
	int result = 0;

	for (i = 1; i < argc; i += 1) {
		if (NULL != strstr(argv[i], "-child_")) {
			isChild = true;
			strcpy(testName, &argv[i][7]);
		} else if (0 == strcmp(argv[i], "-earlyExit")) {
			earlyExit = true;
		} else if (0 == strcmp(argv[i], "-verbose")) {
			isVerbose = true;
		}
	}

	if (!isChild) {
		::testing::InitGoogleTest(&argc, argv);
	}

	if (!isVerbose) {
		OMREventListener::setDefaultTestListener();
	}

	INITIALIZE_THREADLIBRARY_AND_ATTACH();

	portTestEnv = new PortTestEnvironment(argc, argv);
	testing::AddGlobalTestEnvironment(portTestEnv);

	if (isChild) {
		portTestEnv->initPort();
		if (startsWith(testName, "omrfile")) {
			return omrfile_runTests(portTestEnv->getPortLibrary(), argv[0], testName, FALSE);
#if defined(OMR_OS_WINDOWS)
		} else if (startsWith(testName, "omrfile_blockingasync")) {
			return omrfile_runTests(portTestEnv->getPortLibrary(), argv[0], testName, TRUE);
#endif /* defined(OMR_OS_WINDOWS) */
		} else if (startsWith(testName, "omrmmap")) {
			return omrmmap_runTests(portTestEnv->getPortLibrary(), argv[0], testName);
		} else if (startsWith(testName, "omrsig")) {
			return omrsig_runTests(portTestEnv->getPortLibrary(), argv[0], testName);
		}
		portTestEnv->shutdownPort();
	} else {
		result = RUN_ALL_TESTS();
	}

	DETACH_AND_DESTROY_THREADLIBRARY();

	if (earlyExit) {
		printf("exiting from omr_main_entry\n");
		exit(result);
	}

	return result;
}
