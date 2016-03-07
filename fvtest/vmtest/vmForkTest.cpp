/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "omrTest.h"
#include "omrthread.h"
#include "omr.h"
#include "omrTestHelpers.h"
#include "omrvm.h"
#include "testEnvironment.hpp"

static void omrVmResetStart();

extern PortEnvironment *vmTestEnv;

TEST(ThreadForkResetTest, OmrVmReset)
{
	omrVmResetStart();
}

static void
omrVmResetStart()
{
	intptr_t rc;
	int pipedata[2];
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	OMRPORT_ACCESS_FROM_OMRPORT(vmTestEnv->getPortLibrary());
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "vmForkTest"));

	/* Pre-fork */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	omr_vm_preFork(&testVM.omrVM);
	if (0 == fork()) {
		omr_vm_postForkChild(&testVM.omrVM);
		/* Post-fork child process. */

		rc = OMR_Thread_Free(vmthread);
		if (OMR_ERROR_NONE == rc) {
			rc = omrTestVMFini(&testVM);
		}

		J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&rc, sizeof(rc)));
		close(pipedata[0]);
		close(pipedata[1]);
		exit(0);
	}
	omr_vm_postForkParent(&testVM.omrVM);

	/* After fork, notify the sibling thread and wait for it to unlock the global mutex. */
	ssize_t readBytes = read(pipedata[0], (int *)&rc, sizeof(rc));
	EXPECT_TRUE(readBytes == sizeof(rc)) << "Didn't read back enough from pipe";
	EXPECT_TRUE(0 == rc) << "Failure occurred in child process.";
	close(pipedata[0]);
	close(pipedata[1]);

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
}
