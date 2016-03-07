/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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


#include "rasTestHelpers.hpp"

void
reportOMRCommandLineError(OMRPortLibrary *portLibrary, const char *detailStr, va_list args)
{
	char buffer[1024];
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	omrstr_vprintf(buffer, sizeof(buffer), detailStr, args);

	omrtty_err_printf("Error in trace %s\n", buffer);

}

/* Find the directory where the *TraceFormat.dat is located.
 * In standalone OMR build, the .dat file is located in the same
 * directory as the test executable.
 */
static char datDirBuf[1024] = ".";
char *
getTraceDatDir(intptr_t argc, const char **argv)
{
	if (argc > 0) {
		intptr_t i = 0;

		strncpy(datDirBuf, argv[0], sizeof(datDirBuf));
		datDirBuf[1023] = '\0';
		for (i = strlen(datDirBuf) - 1; i >= 0; i--) {
			if ('/' == datDirBuf[i] || '\\' == datDirBuf[i]) {
				break;
			}
			datDirBuf[i] = '\0';
		}
	}
	return datDirBuf;
}

void
createThread(omrthread_t *newThread, uintptr_t suspend, omrthread_detachstate_t detachstate,
			 omrthread_entrypoint_t entryProc, void *entryArg)
{
	omrthread_attr_t attr = NULL;
	intptr_t rc = 0;

	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_init(&attr));
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_set_detachstate(&attr, detachstate));
	EXPECT_EQ(J9THREAD_SUCCESS,
			  rc = omrthread_create_ex(newThread, &attr, suspend, entryProc, entryArg));
	if (rc & J9THREAD_ERR_OS_ERRNO_SET) {
		printf("omrthread_create_ex() returned os_errno=%d\n", (int)omrthread_get_os_errno());
	}
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_destroy(&attr));
}

intptr_t
joinThread(omrthread_t threadToJoin)
{
	intptr_t rc = J9THREAD_SUCCESS;

	EXPECT_EQ(J9THREAD_SUCCESS, rc = omrthread_join(threadToJoin));
	if (rc & J9THREAD_ERR_OS_ERRNO_SET) {
		printf("omrthread_join() returned os_errno=%d\n", (int)omrthread_get_os_errno());
	}
	return rc;
}
