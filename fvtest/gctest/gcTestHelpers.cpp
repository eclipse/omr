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

#include "gcTestHelpers.hpp"
#if defined(WIN32) || defined(WIN64)
/* windows.h defined uintptr_t.  Ignore its definition */
#define UDATA UDATA_win_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#include <psapi.h>
#endif /* defined(WIN32) || defined(WIN64) */

void
GCTestEnvironment::initParams()
{
	for (int i = 1; i < _argc; i++) {
		if (0 == strcmp(_argv[i], "-keepVerboseLog")) {
			keepLog = true;
		}
	}
}

void
GCTestEnvironment::clearParams()
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	while (!params.empty()) {
		const char *elem = params.back();
		params.pop_back();
		omrmem_free_memory((void *)elem);
	}
}

void
GCTestEnvironment::GCTestSetUp()
{
	exampleVM._omrVM = NULL;
	exampleVM.self = NULL;
	exampleVM._omrVMThread = NULL;
	exampleVM._vmAccessMutex = NULL;
	exampleVM._vmExclusiveAccessCount = 0;

	/* Attach main test thread */
	intptr_t irc = omrthread_attach_ex(&exampleVM.self, J9THREAD_ATTR_DEFAULT);
	ASSERT_EQ(0, irc) << "Setup(): omrthread_attach failed, rc=" << irc;

	/* Initialize the VM */
	omr_error_t rc = OMR_Initialize(&exampleVM, &exampleVM._omrVM);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "GCTestSetUp(): OMR_Initialize failed, rc=" << rc;

	/* Set up the vm access mutex */
	omrthread_rwmutex_init(&exampleVM._vmAccessMutex, 0, "VM exclusive access");

	portLib = ((exampleVM._omrVM)->_runtime)->_portLibrary;

	ASSERT_NO_FATAL_FAILURE(initParams());
}

void
GCTestEnvironment::GCTestTearDown()
{
	ASSERT_NO_FATAL_FAILURE(clearParams());

	ASSERT_EQ((uintptr_t)0, exampleVM._vmExclusiveAccessCount) << "GCTestTearDown(): _vmExclusiveAccessCount not 0, _vmExclusiveAccessCount=" << exampleVM._vmExclusiveAccessCount;
	if (NULL != exampleVM._vmAccessMutex) {
		omrthread_rwmutex_destroy(exampleVM._vmAccessMutex);
		exampleVM._vmAccessMutex = NULL;
	}

	omrthread_detach(exampleVM.self);

	/* Shut down VM */
	omr_error_t rc = OMR_Shutdown(exampleVM._omrVM);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "TearDown(): OMR_Shutdown failed, rc=" << rc;

}

void
printMemUsed(const char *where, OMRPortLibrary *portLib)
{
#if defined(WIN32) || defined(WIN64)
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	PROCESS_MEMORY_COUNTERS_EX pmc;
	GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&pmc, sizeof(pmc));
	/* result in bytes */
	gcTestEnv->log(LEVEL_VERBOSE, "%s: phys: %ld; virt: %ld\n", where, pmc.WorkingSetSize, pmc.PrivateUsage);
#elif defined(LINUX)
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	const char *statm_path = "/proc/self/statm";
	intptr_t fileDescriptor = omrfile_open(statm_path, EsOpenRead, 0444);
	if (-1 == fileDescriptor) {
		gcTestEnv->log(LEVEL_ERROR, "%s: failed to open /proc/self/statm.\n", where);
		return;
	}

	char lineStr[2048];
	if (NULL == omrfile_read_text(fileDescriptor, lineStr, sizeof(lineStr))) {
		gcTestEnv->log(LEVEL_ERROR, "%s: failed to read from /proc/self/statm.\n", where);
		return;
	}

	unsigned long size, resident, share, text, lib, data, dt;
	int numOfTokens = sscanf(lineStr, "%ld %ld %ld %ld %ld %ld %ld", &size, &resident, &share, &text, &lib, &data, &dt);
	if (7 != numOfTokens) {
		gcTestEnv->log(LEVEL_ERROR, "%s: failed to query memory info from /proc/self/statm.\n", where);
		return;
	}

	/* result in pages */
	gcTestEnv->log(LEVEL_VERBOSE,"%s: phys: %ld; virt: %ld\n", where, resident, size);
	omrfile_close(fileDescriptor);
#else
	/* memory info not supported */
#endif
}
