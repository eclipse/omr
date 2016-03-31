/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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

/* These headers are required for ChildrenTest */
#if defined(WIN32)
#include <process.h>
#include <Windows.h>
#else /* defined(WIN32) */
#include <fcntl.h>
#include <stdlib.h>
#endif /* defined(WIN32) */

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#define PROCESS_HELPER_SEM_KEY 0x05CAC8E /* Reads OSCACHE */
#endif /* defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX) */

#if defined(J9ZOS390)
#include <spawn.h>
#include <stdlib.h> /* for malloc for atoe */
#include "atoe.h"
#endif

#include <string.h>

#include "testProcessHelpers.hpp"
#include "testHelpers.hpp"
#include "omrport.h"

#if 0
#define PORTTEST_PROCESS_HELPERS_DEBUG
#endif

#if defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)

#if (defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)) || defined(OSX)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#endif

#if !defined(WIN32)
static int setFdCloexec(int fd);
#if !defined(OSX)
static intptr_t translateModifiedUtf8ToPlatform(OMRPortLibrary *portLibrary, const char *inBuffer, uintptr_t inBufferSize, char **outBuffer);
#endif /* !defined(OSX) */
#endif /* !defined(WIN32) */

intptr_t
openLaunchSemaphore(OMRPortLibrary *portLibrary, const char *name, uintptr_t nProcess)
{
	intptr_t semid = semget(PROCESS_HELPER_SEM_KEY, 1, 0660 | IPC_CREAT | IPC_EXCL);

	if (-1 == semid) {
		if (errno == EEXIST) {
			semid = semget(PROCESS_HELPER_SEM_KEY, 1, 0660);
		}
	} else {
		union semun sem_union;

		sem_union.val = 0;
		if (-1 == semctl(semid, 0, SETVAL, sem_union)) {
			semid = -1;
		}
	}

	return semid;
}

intptr_t
SetLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore, uintptr_t nProcess)
{
	union semun sem_union;

	sem_union.val = nProcess;

	return semctl(semaphore, 0, SETVAL, sem_union);
}

intptr_t
ReleaseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore, uintptr_t nProcess)
{
	/* we want to subtract from the launch semaphore so negate the value */
	const intptr_t val = ((intptr_t) nProcess * (-1));
	struct sembuf buffer;

	buffer.sem_num = 0;
	buffer.sem_op = val;
	buffer.sem_flg = 0;

	return semop(semaphore, &buffer, 1);
}

intptr_t
WaitForLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore)
{
	struct sembuf buffer;

	/* set semop() to wait for zero */
	buffer.sem_num = 0;
	buffer.sem_op = 0;
	buffer.sem_flg = 0;

	return semop(semaphore, &buffer, 1);
}

intptr_t
CloseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore)
{
	return semctl(semaphore, 0, IPC_RMID, 0);
}

#elif defined(WIN32)
intptr_t
openLaunchSemaphore(OMRPortLibrary *portLibrary, const char *name, uintptr_t nProcess)
{
	HANDLE semaphore = CreateSemaphore(NULL, 0, (LONG)nProcess, name);
	if (NULL == semaphore) {
		return -1;
	}

	return (intptr_t)semaphore;
}

intptr_t
SetLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore, uintptr_t nProcess)
{
	intptr_t rc = 0;
	uintptr_t i = 0;
	HANDLE semHandle = (HANDLE)semaphore;

	for (i = 0; i < nProcess; i++) {
		if (WaitForSingleObject(semHandle, 0) == WAIT_FAILED) {
			rc = -1;
			break;
		}
	}

	return rc;
}

intptr_t
ReleaseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore, uintptr_t nProcess)
{
	HANDLE semHandle = (HANDLE) semaphore;
	if (ReleaseSemaphore(semHandle, (LONG)nProcess, NULL)) {
		return 0;
	} else {
		return -1;
	}
}

intptr_t
WaitForLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore)
{
	HANDLE semHandle = (HANDLE)semaphore;
	if (WaitForSingleObject(semHandle, INFINITE) == WAIT_FAILED) {
		return -1;
	} else {
		return 0;
	}
}

intptr_t
CloseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore)
{
	HANDLE semHandle = (HANDLE)semaphore;
	if (CloseHandle(semHandle)) {
		return 0;
	}
	return -1;
}
#else /* defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX) */
/* Not supported on anything else */
intptr_t
openLaunchSemaphore(OMRPortLibrary *portLibrary, const char *name, uintptr_t nProcess)
{
	return -1;
}
intptr_t
ReleaseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore, uintptr_t nProcess)
{
	return -1;
}
intptr_t
WaitForLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore)
{
	return -1;
}
intptr_t
CloseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore)
{
	return -1;
}

#endif /* defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX) */

OMRProcessHandle
launchChildProcess(OMRPortLibrary *portLibrary, const char *testname, const char *argv0, const char *options)
{
	OMRProcessHandle processHandle = NULL;
	char *command[2];
	uintptr_t commandLength = 2;
	uint32_t exeoptions = 0;
	intptr_t retVal;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	retVal = omrsysinfo_get_executable_name((char *)argv0, &(command[0]));

	if (retVal != 0) {
		int32_t portableErrno = omrerror_last_error_number();
		const char *errMsg = omrerror_last_error_message();
		if (NULL == errMsg) {
			omrtty_printf("%s: launchChildProcess: omrsysinfo_get_executable_name failed!\n\tportableErrno = %d\n", testname, portableErrno);
		} else {
			omrtty_printf("%s: launchChildProcess: omrsysinfo_get_executable_name failed!\n\tportableErrno = %d portableErrMsg = %s\n", testname, portableErrno, errMsg);
		}
		goto done;

	}

	command[1] = (char *)options;

#if defined(PORTTEST_PROCESS_HELPERS_DEBUG)
	/* print our the commandline options for the process being launched
	* and have the child write its to the console by inheriting stdout and stderr
	*/

	{
		int i;
		omrtty_printf("\n\n");

		for (i = 0; i < commandLength; i++) {
			omrtty_printf("\t\tcommand[%i]: %s\n", i, command[i]);
		}
		omrtty_printf("\n\n");

		exeoptions = OMRPROCESS_DEBUG;
	}
#endif

	retVal = j9process_create(OMRPORTLIB, (const char **)command, commandLength, ".", exeoptions, &processHandle);

	if (0 != retVal) {
		int32_t portableErrno = omrerror_last_error_number();
		const char *errMsg = omrerror_last_error_message();
		if (NULL == errMsg) {
			omrtty_printf("%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d\n", testname, command[0], command[1], portableErrno);
		} else {
			omrtty_printf("%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d portableErrMsg = %s\n", testname, command[0], command[1], portableErrno, errMsg);
		}

		processHandle = NULL;

		goto done;
	}

done:
	return processHandle;

}

intptr_t
waitForTestProcess(OMRPortLibrary *portLibrary, OMRProcessHandle processHandle)
{
	intptr_t retval = -1;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (NULL == processHandle) {
		omrtty_printf("waitForTestProcess: processHandle == NULL\n");
		goto done;
	}

	if (0 != j9process_waitfor(OMRPORTLIB, processHandle)) {
		int32_t portableErrno = omrerror_last_error_number();
		const char *errMsg = omrerror_last_error_message();
		if (NULL == errMsg) {
			omrtty_printf("waitForTestProcess: j9process_waitfor() failed\n\tportableErrno = %d\n" , portableErrno);
		} else {
			omrtty_printf("waitForTestProcess: j9process_waitfor() failed\n\tportableErrno = %d portableErrMsg = %s\n" , portableErrno, errMsg);
		}

		goto done;
	}

	retval = j9process_get_exitCode(OMRPORTLIB, processHandle);

	if (0 != j9process_close(OMRPORTLIB, &processHandle, 0)) {
		int32_t portableErrno = omrerror_last_error_number();
		const char *errMsg = omrerror_last_error_message();
		if (NULL == errMsg) {
			omrtty_printf("waitForTestProcess: j9process_close() failed\n\tportableErrno = %d\n" , portableErrno);
		} else {
			omrtty_printf("waitForTestProcess: j9process_close() failed\n\tportableErrno = %d portableErrMsg = %s\n" , portableErrno, errMsg);
		}
		goto done;
	}

done:
	return retval;
}

void
SleepFor(intptr_t second)
{
#if defined(WIN32)
	Sleep((DWORD)second * 1000);
#elif defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX)
	sleep(second);
#endif /* defined(LINUX) || defined(J9ZOS390) || defined(AIXPPC) || defined(OSX) */
}

#if !defined(WIN32)
static int
setFdCloexec(int fd)
{
	int rc = -1;
	int flags;

	flags = fcntl(fd, F_GETFD);
	if (flags >= 0) {
		flags |= FD_CLOEXEC;
		rc = fcntl(fd, F_SETFD, flags);
	}
	return rc;
}

#if !defined(OSX)
static intptr_t
translateModifiedUtf8ToPlatform(OMRPortLibrary *portLibrary, const char *inBuffer, uintptr_t inBufferSize, char **outBuffer)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	char *result = 0;
	int32_t bufferLength = 0;
	int32_t resultLength = 0;

	*outBuffer = NULL;
	bufferLength = 	omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
								   inBuffer, inBufferSize, NULL, 0);
	/* get the size of the platform string */

	if (bufferLength < 0) {
		return bufferLength; /* some error occurred */
	} else {
		bufferLength += MAX_STRING_TERMINATOR_LENGTH;
	}

	result = (char *)omrmem_allocate_memory(bufferLength, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == result)  {
		return OMRPORT_ERROR_STRING_MEM_ALLOCATE_FAILED;
	}

	resultLength = omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
								  inBuffer, inBufferSize, result, bufferLength);
	/* do the conversion */

	if (resultLength < 0) {
		omrmem_free_memory(result);
		return resultLength; /* some error occurred */
	} else {
		memset(result + resultLength, 0, MAX_STRING_TERMINATOR_LENGTH);

		*outBuffer = result;
		return 0;
	}
}
#endif /* !defined(OSX) */
#endif /*!defined(WIN32) */

intptr_t
j9process_close(OMRPortLibrary *portLibrary, OMRProcessHandle *processHandle, uint32_t options)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	int32_t rc = 0;
	OMRProcessHandleStruct *processHandleStruct = (OMRProcessHandleStruct *)*processHandle;

#if defined(WIN32)
	if (!CloseHandle((HANDLE)processHandleStruct->procHandle)) {
		rc = OMRPROCESS_ERROR;
	}
	if (OMRPROCESS_INVALID_FD != processHandleStruct->inHandle) {
		if (!CloseHandle((HANDLE)processHandleStruct->inHandle)) {
			rc = OMRPROCESS_ERROR;
		}
	}
	if (OMRPROCESS_INVALID_FD != processHandleStruct->outHandle) {
		if (!CloseHandle((HANDLE)processHandleStruct->outHandle)) {
			rc = OMRPROCESS_ERROR;
		}
	}

	if (OMRPROCESS_INVALID_FD != processHandleStruct->errHandle) {
		if (!CloseHandle((HANDLE)processHandleStruct->errHandle)) {
			rc = OMRPROCESS_ERROR;
		}
	}
#else /* defined(WIN32) */
	if (OMRPROCESS_INVALID_FD != processHandleStruct->inHandle) {
		if (0 != close((int) processHandleStruct->inHandle)) {
			rc = OMRPROCESS_ERROR;
		}
	}
	if (OMRPROCESS_INVALID_FD != processHandleStruct->outHandle) {
		if (0 != close((int) processHandleStruct->outHandle)) {
			rc = OMRPROCESS_ERROR;
		}
	}
	if (OMRPROCESS_INVALID_FD != processHandleStruct->errHandle) {
		if (0 != (close((int) processHandleStruct->errHandle) != 0)) {
			rc = OMRPROCESS_ERROR;
		}
	}
#endif /* defined(WIN32) */
	omrmem_free_memory(processHandleStruct);
	processHandleStruct = *processHandle = NULL;

	return rc;
}

intptr_t j9process_waitfor(OMRPortLibrary *portLibrary, OMRProcessHandle processHandle)
{
	OMRProcessHandleStruct *processHandleStruct = (OMRProcessHandleStruct *) processHandle;
	intptr_t rc = OMRPROCESS_ERROR;
#if defined(WIN32)
	if (WAIT_OBJECT_0 == WaitForSingleObject((HANDLE) processHandleStruct->procHandle, INFINITE)) {
		DWORD procstat = 0;

		if (0 != GetExitCodeProcess((HANDLE)processHandleStruct->procHandle, (LPDWORD)&procstat)) {
			processHandleStruct->exitCode = (intptr_t) procstat;
			rc = 0;
		}
	}

	return rc;
#else /* defined(WIN32) */
	int statusLocation = -1;
	pid_t retVal = waitpid((pid_t)processHandleStruct->procHandle, &statusLocation, 0);

	if (retVal == (pid_t)processHandleStruct->procHandle) {
		if (0 != WIFEXITED(statusLocation)) {
			processHandleStruct->exitCode = WEXITSTATUS(statusLocation);
		}
		rc = 0;
	} else {
		rc = OMRPROCESS_ERROR;
	}
#endif /* defined(WIN32) */
	return rc;
}

intptr_t
j9process_get_exitCode(OMRPortLibrary *portLibrary, OMRProcessHandle processHandle)
{
	OMRProcessHandleStruct *processHandleStruct = (OMRProcessHandleStruct *)processHandle;
	return processHandleStruct->exitCode;
}

#if defined(WIN32)
typedef struct OMRProcessWin32Pipes {
	HANDLE inR;
	HANDLE inW;
	HANDLE outR;
	HANDLE outW;
	HANDLE errR;
	HANDLE errW;
	HANDLE inDup;
	HANDLE outDup;
	HANDLE errDup;
} OMRProcessWin32Pipes;

/**
 * Closes and destroys all pipes in J9ProcessWin32Pipes struct
 */
static void
closeAndDestroyPipes(OMRProcessWin32Pipes *pipes)
{
	intptr_t i, numHandles;
	HANDLE *handles[] = { &pipes->inR, &pipes->inW, &pipes->inDup,
						  &pipes->outR, &pipes->outW, &pipes->outDup,
						  &pipes->errR, &pipes->errW, &pipes->errDup
						};

	numHandles = sizeof(handles) / sizeof(HANDLE *);

	for (i = 0; i < numHandles; i += 1) {
		if (NULL != *handles[i]) {
			CloseHandle(*handles[i]);
			handles[i] = NULL;
		}
	}

	ZeroMemory(pipes, sizeof(OMRProcessWin32Pipes));
}

wchar_t *
convertFromUTF8(OMRPortLibrary *portLibrary, const char *string, wchar_t *unicodeBuffer, uintptr_t unicodeBufferSize)
{
	wchar_t *unicodeString;
	uintptr_t length;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (NULL == string) {
		return NULL;
	}
	length = (uintptr_t)strlen(string);
	if (length < unicodeBufferSize) {
		unicodeString = unicodeBuffer;
	} else {
		unicodeString = (wchar_t *)omrmem_allocate_memory((length + 1) * 2, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == unicodeString) {
			return NULL;
		}
	}
	if (0 == MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, string, -1, unicodeString, (int)length + 1)) {
		omrerror_set_last_error(GetLastError(), OMRPORT_ERROR_OPFAILED);
		if (unicodeString != unicodeBuffer) {
			omrmem_free_memory(unicodeString);
		}
		return NULL;
	}

	return unicodeString;
}

/**
 * Converts command string array into a single unicode string command line
 *
 * @returns 0 upon success, negative portable error code upon failure
 */
static intptr_t
getUnicodeCmdline(struct OMRPortLibrary *portLibrary, const char *command[], uintptr_t commandLength, wchar_t **unicodeCmdline)
{
	char *needToBeQuoted = NULL;
	size_t length, l;
	intptr_t rc = 0;
	intptr_t i;
	char *ptr = NULL;
	const char *argi = NULL;
	char *commandAsString = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	/*
	 * Windows needs a char* command line unlike regular C exec* functions
	 * Therefore we need to rebuild the line that has been sliced in java...
	 * Subtle : if a token embbeds a <space>, the token will be quoted (only
	 * 			if it hasn't been quoted yet) The quote char is "
	 * Note (see "XXX references in the code)
		 * Our CDev scanner/parser does not handle '"' correctly. A workaround is to close
		 * the '"' with another " , embedded in a C comment.
	 */
	needToBeQuoted = (char *)omrmem_allocate_memory(commandLength, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == needToBeQuoted) {
		rc = OMRPROCESS_ERROR;
	} else {
		memset(needToBeQuoted, '\0', commandLength);

		length = commandLength; /*add 1 <blank> between each token + a reserved place for the last NULL*/
		for (i = (intptr_t)commandLength; --i >= 0;) {
			intptr_t j;
			size_t commandILength;
			const char *commandStart;
			commandILength = strlen(command[i]);
			length += commandILength;
			/* check_for_embbeded_space */
			if (commandILength > 0) {
				commandStart = command[i];
				if (commandStart[0] != '"' /*"XXX*/) {
					for (j = 0; j < (intptr_t)commandILength ; j += 1) {
						if (commandStart[j] == ' ') {
							needToBeQuoted[i] = '\1'; /* a random value, different from zero though*/
							length += 2; /* two quotes are added */
							if (commandILength > 1 && commandStart[commandILength - 1] == '\\'
								&& commandStart[commandILength - 2] != '\\') {
								length++;    /* need to double slash */
							}
							break;
						}
					}
				}
			} /* end of check_for_embedded_space */
		}

		ptr = commandAsString = (char *)omrmem_allocate_memory(length, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == commandAsString) {
			omrmem_free_memory(needToBeQuoted);
			rc = OMRPROCESS_ERROR;
		} else {
			uintptr_t k;

			for (k = 0; k < commandLength ; k += 1) {
				l = strlen(argi = command[k]);
				if (needToBeQuoted[k]) {
					*ptr++ = '"' /*"XXX*/ ;
				}
				memcpy(ptr, argi, l);
				ptr += l;
				if (needToBeQuoted[k]) {
					if (l > 1 && *(ptr - 1) == '\\' && *(ptr - 2) != '\\') {
						*ptr++ = '\\';
					}
					*ptr++ = '"' /*"XXX*/ ;
				}
				*ptr++ = ' '; /* put a <blank> between each token */
			}
			*(ptr - 1) = '\0'; /*commandLength > 0 ==> valid operation*/
			omrmem_free_memory(needToBeQuoted);

			*unicodeCmdline = (wchar_t *)omrmem_allocate_memory((length + 1) * 2, OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == *unicodeCmdline) {
				rc = OMRPROCESS_ERROR;
			} else {
				convertFromUTF8(OMRPORTLIB, commandAsString, *unicodeCmdline, length + 1);
				omrmem_free_memory(commandAsString);
			}
		}
	}

	return rc;
}

#endif /* defined(WIN32) */

intptr_t
j9process_create(OMRPortLibrary *portLibrary, const char *command[], uintptr_t commandLength, const char *dir, uint32_t options, OMRProcessHandle *processHandle)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	intptr_t rc = 0;
	OMRProcessHandleStruct *processHandleStruct = NULL;

#if defined(WIN32)
	STARTUPINFOW sinfo;
	PROCESS_INFORMATION pinfo;
	SECURITY_ATTRIBUTES sAttrib;
	wchar_t *unicodeDir = NULL;
	wchar_t *unicodeCmdline = NULL;
	wchar_t *unicodeEnv = NULL;
	OMRProcessWin32Pipes pipes;
	DWORD dwMode = PIPE_NOWAIT;	/* it's only used if OMRPORT_PROCESS_NONBLOCKING_IO is set */

	if (0 == commandLength) {
		return OMRPROCESS_ERROR;
	}

	processHandleStruct = (OMRProcessHandleStruct *)omrmem_allocate_memory(sizeof(OMRProcessHandleStruct), OMRMEM_CATEGORY_PORT_LIBRARY);

	ZeroMemory(&sinfo, sizeof(sinfo));
	ZeroMemory(&pinfo, sizeof(pinfo));
	ZeroMemory(&sAttrib, sizeof(sAttrib));

	sinfo.cb = sizeof(sinfo);
	sinfo.dwFlags = STARTF_USESTDHANDLES;

	/* Allow handle inheritance */
	sAttrib.bInheritHandle = 1;
	sAttrib.nLength = sizeof(sAttrib);

	ZeroMemory(&pipes, sizeof(OMRProcessWin32Pipes));

	// Create input pipe
	if (0 == CreatePipe(&(pipes.inR), &(pipes.inW), &sAttrib, 512)) {
		closeAndDestroyPipes(&pipes);
		return OMRPROCESS_ERROR;
	} else {
		if (0 == DuplicateHandle(GetCurrentProcess(), pipes.inW, GetCurrentProcess(),
								 &(pipes.inDup), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
			closeAndDestroyPipes(&pipes);
			return OMRPROCESS_ERROR;
		}
		if (0 == CloseHandle(pipes.inW)) {
			closeAndDestroyPipes(&pipes);
			return OMRPROCESS_ERROR;
		}
		pipes.inW = NULL;
	}
	sinfo.hStdInput = pipes.inR;
	processHandleStruct->inHandle = (intptr_t)pipes.inDup;

	if (OMRPROCESS_DEBUG == options) {
		processHandleStruct->outHandle = OMRPROCESS_INVALID_FD;
		if (sinfo.dwFlags == STARTF_USESTDHANDLES) {
			sinfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		}
		processHandleStruct->errHandle = OMRPROCESS_INVALID_FD;
		if (sinfo.dwFlags == STARTF_USESTDHANDLES) {
			sinfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		}
	} else {
		// Create output pipe
		if (0 == CreatePipe(&(pipes.outR), &(pipes.outW), &sAttrib, 512)) {
			closeAndDestroyPipes(&pipes);
			return OMRPROCESS_ERROR;
		} else {
			if (0 == DuplicateHandle(GetCurrentProcess(), pipes.outR, GetCurrentProcess(),
									 &(pipes.outDup), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
				closeAndDestroyPipes(&pipes);
				return OMRPROCESS_ERROR;
			}
			if (0 == CloseHandle(pipes.outR)) {
				closeAndDestroyPipes(&pipes);
				return OMRPROCESS_ERROR;
			}
			pipes.outR = NULL;
		}
		processHandleStruct->outHandle = (intptr_t)pipes.outDup;
		sinfo.hStdOutput = pipes.outW;

		// create error pipe
		if (0 == CreatePipe(&(pipes.errR), &(pipes.errW), &sAttrib, 512)) {
			closeAndDestroyPipes(&pipes);
			return OMRPROCESS_ERROR;
		} else {
			if (0 == DuplicateHandle(GetCurrentProcess(), pipes.errR, GetCurrentProcess(),
									 &(pipes.errDup), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
				closeAndDestroyPipes(&pipes);
				return OMRPROCESS_ERROR;
			}
			if (NULL != pipes.errR) {
				if (0 == CloseHandle(pipes.errR)) {
					closeAndDestroyPipes(&pipes);
					return OMRPROCESS_ERROR;
				}
			}
			pipes.errR = NULL;
		}
		processHandleStruct->errHandle = (intptr_t)pipes.errDup;
		sinfo.hStdError = pipes.errW;
	}

	if (0 == rc) {
		rc = getUnicodeCmdline(OMRPORTLIB, command, commandLength, &unicodeCmdline);
		if (0 == rc) {
			if (NULL != dir) {
				size_t length = strlen(dir);
				unicodeDir = (wchar_t *)omrmem_allocate_memory((length + 1) * 2, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (NULL == unicodeDir) {
					rc = OMRPROCESS_ERROR;
				} else {
					convertFromUTF8(OMRPORTLIB, dir, unicodeDir, length + 1);
				}
			}

			if (0 == rc) {
				DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;

				if (0 == CreateProcessW( /* CreateProcessW returns 0 upon failure */
						NULL,
						unicodeCmdline,
						NULL,
						NULL,
						TRUE,
						creationFlags,		/* add DEBUG_ONLY_THIS_PROCESS for smoother debugging */
						unicodeEnv,
						unicodeDir,
						&sinfo,
						&pinfo)				/* Pointer to PROCESS_INFORMATION structure; */
				) {
					rc = OMRPROCESS_ERROR;
				} else {
					processHandleStruct->procHandle = (intptr_t) pinfo.hProcess;
					/* Close Handles passed to child if there is any */
					CloseHandle(pipes.inR);
					if (OMRPROCESS_DEBUG != options) {
						CloseHandle(pipes.outW);
						CloseHandle(pipes.errW);
					}
					CloseHandle(pinfo.hThread);	/* Implicitly created, a leak otherwise*/
				}
			}
		}
	}

	if (NULL != unicodeCmdline) {
		omrmem_free_memory(unicodeCmdline);
	}
	if (NULL != unicodeDir) {
		omrmem_free_memory(unicodeDir);
	}
	if (NULL != unicodeEnv) {
		omrmem_free_memory(unicodeEnv);
	}

	if (0 != rc) {
		/* If an error occurred close all handles */
		closeAndDestroyPipes(&pipes);
	} else {
		*processHandle = processHandleStruct;
	}

	return rc;
#else /* defined(WIN32) */
	const char *cmd = NULL;
	int grdpid;
	unsigned int i;
	int newFD[3][2];
	int forkedChildProcess[2];
	int pipeFailed = 0;
	int errorNumber = 0;
	char **newCommand;
	uintptr_t newCommandSize;

	if (0 == commandLength) {
		return OMRPROCESS_ERROR;
	}

	for (i = 0; i < 3; i += 1) {
		newFD[i][0] = OMRPROCESS_INVALID_FD;
		newFD[i][1] = OMRPROCESS_INVALID_FD;
	}
	/* Build the new io pipes (in/out/err) */
	if (-1 == pipe(newFD[0])) {
		pipeFailed = 1;
	}
	if (OMRPROCESS_DEBUG != options) {
		if (-1 == pipe(newFD[1])) {
			pipeFailed = 1;
		}
		if (-1 == pipe(newFD[2])) {
			pipeFailed = 1;
		}
	}
	if (-1 == pipe(forkedChildProcess)) { /* pipe for synchronization */
		forkedChildProcess[0] = OMRPROCESS_INVALID_FD;
		forkedChildProcess[1] = OMRPROCESS_INVALID_FD;
		pipeFailed = 1;
	}

	if (pipeFailed) {
		for (i = 0; i < 3; i++) {
			if (OMRPROCESS_INVALID_FD != newFD[i][0]) {
				if (-1 != rc) {
					rc = close(newFD[i][0]);
				}
				if (-1 != rc) {
					rc = close(newFD[i][1]);
				}
			}
		}
		if (OMRPROCESS_INVALID_FD != forkedChildProcess[0]) {
			if (-1 != rc) {
				rc = close(forkedChildProcess[0]);
			}
			if (-1 != rc) {
				rc = close(forkedChildProcess[1]);
			}
		}
		return OMRPROCESS_ERROR;
	}

	if (-1 != rc) {
		rc = setFdCloexec(forkedChildProcess[0]);
	}
	if (-1 != rc) {
		rc = setFdCloexec(forkedChildProcess[1]);
	}

	/* Create a NULL terminated array to contain the command for call to execv[p|e]()
	 * We could do this in the child, but if mem_allocate_memory fails, it's easier to diagnose
	 * failure in parent. Remember to free the memory
	 */
	newCommandSize = (commandLength + 1) * sizeof(uintptr_t);
	newCommand = (char **)omrmem_allocate_memory(newCommandSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == newCommand) {
		return OMRPROCESS_ERROR;
	}
	memset(newCommand, 0, newCommandSize);

	for (i = 0 ; i < commandLength; i += 1) {
#if defined(OSX)
		newCommand[i] = (char *)omrmem_allocate_memory(strlen(command[i]) + 1, OMRMEM_CATEGORY_PORT_LIBRARY);
		omrstr_printf(newCommand[i], strlen(command[i]) + 1, command[i]);
#else /* defined(OSX) */
		intptr_t translateStatus = translateModifiedUtf8ToPlatform(OMRPORTLIB, command[i], strlen(command[i]), &(newCommand[i]));
		if (0 != translateStatus) {
			unsigned int j = 0;
			/* most likely out of memory, free the strings we just converted. */
			for (j = 0; j < i; j += 1) {
				if (NULL != newCommand[j]) {
					omrmem_free_memory(newCommand[j]);
				}
			}
			return translateStatus;
		}
#endif /* defined(OSX) */
	}
	cmd = newCommand[0];

	newCommand[commandLength] = NULL;

	grdpid = fork();
	if (grdpid == 0) {
		/* Child process */
		char dummy = '\0';

		/* Redirect pipes so grand-child inherits new pipes */
		rc = dup2(newFD[0][0], 0);
		if (-1 != rc) {
			if (OMRPROCESS_DEBUG != options) {
				rc = dup2(newFD[1][1], 1);
				if (-1 != rc) {
					rc = dup2(newFD[2][1], 2);
				}
			}
		}

		/* tells the parent that that very process is running */
		rc = write(forkedChildProcess[1], (void *) &dummy, 1);

		if ((-1 != rc) && dir) {
			rc = chdir(dir);
		}

		if (-1 != rc) {
			/* Try to perform the execv; on success, it does not return */
			rc = execvp(cmd, newCommand);
		}

		/* If we get here, tell the parent that the execv failed! Send the error number. */
		if (-1 != rc) {
			rc = write(forkedChildProcess[1], &errno, sizeof(errno));
		}
		if (-1 != rc) {
			rc = close(forkedChildProcess[0]);
		}
		if (-1 != rc) {
			rc = close(forkedChildProcess[1]);
		}
		/* If the exec failed, we must exit or there will be two VM processes running. */
		exit(rc);
	} else {
		/* In the parent process */
		char dummy;

		for (i = 0; i < commandLength; i++) {
			if (NULL != newCommand[i]) {
				omrmem_free_memory(newCommand[i]);
			}
		}
		omrmem_free_memory(newCommand);

		if ((OMRPROCESS_INVALID_FD != newFD[0][0]) && (-1 != rc)) {
			rc = close(newFD[0][0]);
		}
		if ((OMRPROCESS_INVALID_FD != newFD[1][1]) && (-1 != rc)) {
			rc = close(newFD[1][1]);
		}
		if ((OMRPROCESS_INVALID_FD != newFD[2][1]) && (-1 != rc)) {
			rc = close(newFD[2][1]);
		}

		if (grdpid == -1) { /* the fork failed */
			/* close the open pipes */
			if (-1 != rc) {
				rc = close(forkedChildProcess[0]);
			}
			if (-1 != rc) {
				rc = close(forkedChildProcess[1]);
			}
			if ((OMRPROCESS_INVALID_FD != newFD[0][1]) && (-1 != rc)) {
				rc = close(newFD[0][1]);
			}
			if ((OMRPROCESS_INVALID_FD != newFD[1][0]) && (-1 != rc)) {
				rc = close(newFD[1][0]);
			}
			if ((OMRPROCESS_INVALID_FD != newFD[2][0]) && (-1 != rc)) {
				rc = close(newFD[2][0]);
			}
			return OMRPROCESS_ERROR;
		}

		/* Store the rw handles to the childs io */
		processHandleStruct = (OMRProcessHandleStruct *)omrmem_allocate_memory(sizeof(OMRProcessHandleStruct), OMRMEM_CATEGORY_PORT_LIBRARY);
		processHandleStruct->inHandle = (intptr_t)newFD[0][1];
		if (OMRPROCESS_DEBUG == options) {
			processHandleStruct->outHandle = OMRPROCESS_INVALID_FD;
			processHandleStruct->errHandle = OMRPROCESS_INVALID_FD;
		} else {
			processHandleStruct->outHandle = (intptr_t)newFD[1][0];
			processHandleStruct->errHandle = (intptr_t)newFD[2][0];
		}

		processHandleStruct->procHandle = (intptr_t)grdpid;
		processHandleStruct->pid = (int32_t)grdpid;

		/* let the forked child start. */
		rc = close(forkedChildProcess[1]);
		if (-1 != rc) {
			rc = read(forkedChildProcess[0], &dummy, 1);
		}

		/* [PR CMVC 143339]
		 * Instead of using timeout to determine if child process has been created successfully,
		 * a single block read/write pipe call is used, if process creation failed, errorNumber
		 * with sizeof(errno) will be returned, otherwise, read will fail due to pipe closure.
		 */
		if (-1 != rc) {
			rc = read(forkedChildProcess[0], &errorNumber, sizeof(errno));
		}
		if (-1 != rc) {
			rc = close(forkedChildProcess[0]);
		}

		if (rc == sizeof(errno)) {
			return OMRPROCESS_ERROR;
		}
		*processHandle = processHandleStruct;
		return rc;
	}

	return OMRPROCESS_ERROR;
#endif /* defined(WIN32) */
}
