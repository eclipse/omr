/*******************************************************************************
 * Copyright IBM Corp. and others 2020
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Shared Semaphores
 */

#include <errno.h>
#include <stddef.h>
#include "omrportpriv.h"
#include "omrport.h"
#include "portnls.h"
#include "ut_omrport.h"
#include "omrsharedhelper.h"
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "omrshsem.h"
#include "omrshsemun.h"
#include "omrsysv_ipcwrappers.h"

/*These flags are only used internally*/
#define CREATE_ERROR -10
#define OPEN_ERROR  -11
#define WRITE_ERROR -12
#define READ_ERROR -13
#define MALLOC_ERROR -14

#define OMRSH_NO_DATA -21
#define OMRSH_BAD_DATA -22

#define RETRY_COUNT 10
#define SLEEP_TIME 5

#define SEMMARKER_INITIALIZED 769

#define SHSEM_FTOK_PROJID 0x81

static intptr_t omrshsem_checkMarker(OMRPortLibrary *portLibrary, int semid, int semsetsize);
static intptr_t omrshsem_checkSize(OMRPortLibrary *portLibrary, int semid, int32_t numsems);
static intptr_t omrshsem_createSemaphore(struct OMRPortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, char *baseFile, int32_t setSize, omrshsem_baseFileFormat* controlinfo, uintptr_t groupPerm);
static intptr_t omrshsem_openSemaphore(struct OMRPortLibrary *portLibrary, intptr_t fd, char *baseFile, omrshsem_baseFileFormat* controlinfo, uintptr_t groupPerm, uintptr_t cacheFileType);
static void omrshsem_createSemHandle(struct OMRPortLibrary *portLibrary, int semid, int nsems, char* baseFile, omrshsem_handle * handle);
static intptr_t omrshsem_readOlderNonZeroByteControlFile(OMRPortLibrary *portLibrary, intptr_t fd, omrshsem_baseFileFormat * info);
static void omrshsem_initShsemStatsBuffer(OMRPortLibrary *portLibrary, OMRPortShsemStatistic *statbuf);

intptr_t
omrshsem_deprecated_openDeprecated (struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshsem_handle** handle, const char* semname, uintptr_t cacheFileType)
{
	intptr_t retval = OMRPORT_ERROR_SHSEM_OPFAILED;
	omrshsem_baseFileFormat controlinfo;
	char baseFile[OMRSH_MAXPATH];
	struct omrshsem_handle * tmphandle = NULL;
	intptr_t baseFileLength = 0;
	intptr_t handleStructLength = sizeof(struct omrshsem_handle);
	intptr_t fd = -1;
	intptr_t fileIsLocked = OMRSH_SUCCESS;
	BOOLEAN isReadOnlyFD = FALSE;
	union semun semctlArg;
	struct semid_ds buf;
	semctlArg.buf = &buf;
	*handle = NULL;
	Trc_PRT_shsem_omrshsem_openDeprecated_Entry();

	omr_clearPortableError(portLibrary);

	if (cacheDirName == NULL) {
		Trc_PRT_shsem_omrshsem_deprecated_openDeprecated_ExitNullCacheDirName();
		goto error;
	}

	omrstr_printf(portLibrary, baseFile, OMRSH_MAXPATH, "%s%s", cacheDirName, semname);

	baseFileLength = strlen(baseFile) + 1;
	tmphandle = omrmem_allocate_memory(portLibrary, (handleStructLength + baseFileLength), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == tmphandle) {
		Trc_PRT_shsem_omrshsem_openDeprecated_Message("Error: could not alloc handle.");
		goto error;
	}

	tmphandle->baseFile = (char *) (((char *) tmphandle) + handleStructLength);

	fileIsLocked = omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, baseFile, 0);
	
	if (fileIsLocked == OMRSH_FILE_DOES_NOT_EXIST) {
		/* This will occur if the shared memory control files, exists but the semaphore file does not.
		 *
		 * In this case OMRPORT_INFO_SHSEM_PARTIAL is returned to suggest the
		 * caller try doing there work without a lock (if it can).
		 */
		Trc_PRT_shsem_omrshsem_openDeprecated_Message("Error: control file does not exist.");
		retval = OMRPORT_INFO_SHSEM_PARTIAL;
		goto error;
	} else if ( fileIsLocked != OMRSH_SUCCESS) {
		Trc_PRT_shsem_omrshsem_openDeprecated_Message("Error: could not lock semaphore control file.");
		goto error;
	}

	if (cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		int semid = -1;
		int projid = 0xad;
		key_t fkey;
		int semflags_open = groupPerm ? OMRSHSEM_SEMFLAGS_GROUP_OPEN : OMRSHSEM_SEMFLAGS_OPEN;

		fkey = omr_ftokWrapper(portLibrary, baseFile, projid);
		if (fkey == -1) {
			Trc_PRT_shsem_omrshsem_openDeprecated_Message("Error: ftok failed.");
			goto error;
		}
		
		/*Check that the semaphore is still on the system.*/
		semid = omr_semgetWrapper(portLibrary, fkey, 0, semflags_open);
		if (-1 == semid) {
			int32_t lasterrno = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			const char * errormsg = omrerror_last_error_message(portLibrary);
			Trc_PRT_shsem_omrshsem_openDeprecated_semgetFailed(fkey, lasterrno, errormsg);			
			if (lasterrno == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
				Trc_PRT_shsem_omrshsem_openDeprecated_semgetEINVAL(fkey);
				retval = OMRPORT_INFO_SHSEM_PARTIAL;
				/*Clean up the control file since its SysV object is deleted.*/
				omrfile_unlink(portLibrary, baseFile);
			}
			goto error;
		}

		/*get buf.sem_nsems size for omrshsem_createSemHandle()*/
		if (omr_semctlWrapper(portLibrary, TRUE, semid, 0, IPC_STAT, semctlArg) == -1) {
			int32_t lasterrno = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			const char * errormsg = omrerror_last_error_message(portLibrary);
			Trc_PRT_shsem_omrshsem_openDeprecated_semctlFailed(semid, lasterrno, errormsg);
			goto error;
		}

		omrshsem_createSemHandle(portLibrary, semid, buf.sem_nsems, baseFile, tmphandle);
		tmphandle->timestamp = omrfile_lastmod(portLibrary, baseFile);

		retval = OMRPORT_INFO_SHSEM_OPENED;

	} else if (cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE) {

		if (omrshsem_readOlderNonZeroByteControlFile(portLibrary, fd, &controlinfo) != OMRSH_SUCCESS) {
			Trc_PRT_shsem_omrshsem_openDeprecated_Message("Error: could not read deprecated control file.");
			goto error;
		}

		retval = omrshsem_openSemaphore(portLibrary, fd, baseFile, &controlinfo, groupPerm, cacheFileType);
		if (OMRPORT_INFO_SHSEM_OPENED != retval) {
			if (OMRPORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK == retval) {
				Trc_PRT_shsem_omrshsem_openDeprecated_Message("Control file was not unlinked.");
				retval = OMRPORT_ERROR_SHSEM_OPFAILED;
			} else {
				/* Clean up the control file */
				if (-1 == omrfile_unlink(portLibrary, baseFile)) {
					Trc_PRT_shsem_omrshsem_openDeprecated_Message("Control file could not be unlinked.");
				} else {
					Trc_PRT_shsem_omrshsem_openDeprecated_Message("Control file was unlinked.");
				}
				retval = OMRPORT_INFO_SHSEM_PARTIAL;
			}
			goto error;
		}
		
		omrshsem_createSemHandle(portLibrary, controlinfo.semid, controlinfo.semsetSize, baseFile, tmphandle);
		tmphandle->timestamp = omrfile_lastmod(portLibrary, baseFile);

		retval = OMRPORT_INFO_SHSEM_OPENED;
	} else {
		Trc_PRT_shsem_omrshsem_openDeprecated_BadCacheType(cacheFileType);
	}

error:
	if (fileIsLocked == OMRSH_SUCCESS) {
		if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
			Trc_PRT_shsem_omrshsem_openDeprecated_Message("Error: could not unlock semaphore control file.");
			retval = OMRPORT_ERROR_SHSEM_OPFAILED;
		}
	}
	if (OMRPORT_INFO_SHSEM_OPENED != retval){
		if (NULL != tmphandle) {
			omrmem_free_memory(portLibrary, tmphandle);
		}
		*handle = NULL;
		Trc_PRT_shsem_omrshsem_openDeprecated_Exit("Exit: failed to open older semaphore");
	} else {
		*handle = tmphandle;
		Trc_PRT_shsem_omrshsem_openDeprecated_Exit("Opened shared semaphore.");
	}

	return retval;
}

intptr_t 
omrshsem_deprecated_open (struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshsem_handle **handle, const char *semname, int setSize, int permission, uintptr_t flags, OMRControlFileStatus *controlFileStatus)
{
	/* TODO: needs to be longer? dynamic?*/
	char baseFile[OMRSH_MAXPATH];
	intptr_t fd;
	omrshsem_baseFileFormat controlinfo;
	int32_t rc;
	BOOLEAN isReadOnlyFD;
	intptr_t baseFileLength = 0;
	intptr_t handleStructLength = sizeof(struct omrshsem_handle);
	char * exitmsg = 0;
	intptr_t retryIfReadOnlyCount = 10;
	struct omrshsem_handle * tmphandle = NULL;
	BOOLEAN freeHandleOnError = TRUE;

	Trc_PRT_shsem_omrshsem_open_Entry(semname, setSize, permission);

	omr_clearPortableError(portLibrary);

	if (cacheDirName == NULL) {
		Trc_PRT_shsem_omrshsem_deprecated_open_ExitNullCacheDirName();
		return OMRPORT_ERROR_SHSEM_OPFAILED;
	}

	omrstr_printf(portLibrary, baseFile, OMRSH_MAXPATH, "%s%s", cacheDirName, semname);

	Trc_PRT_shsem_omrshsem_open_Debug1(baseFile);

	baseFileLength = strlen(baseFile) + 1;
	*handle = NULL;
	tmphandle = omrmem_allocate_memory(portLibrary, (handleStructLength + baseFileLength), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == tmphandle) {
		Trc_PRT_shsem_omrshsem_open_ExitWithMsg("Error: could not alloc handle.");
		return OMRPORT_ERROR_SHSEM_OPFAILED;
	}
	tmphandle->baseFile = (char *) (((char *) tmphandle) + handleStructLength);

	if (NULL != controlFileStatus) {
		memset(controlFileStatus, 0, sizeof(OMRControlFileStatus));
	}

	for (retryIfReadOnlyCount = 10; retryIfReadOnlyCount > 0; retryIfReadOnlyCount -= 1) {
		/*Open control file with write lock*/
		BOOLEAN canCreateFile = TRUE;
		if (OMR_ARE_ANY_BITS_SET(flags, OMRSHSEM_OPEN_FOR_STATS | OMRSHSEM_OPEN_FOR_DESTROY | OMRSHSEM_OPEN_DO_NOT_CREATE)) {
			canCreateFile = FALSE;
		}
		if (omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, canCreateFile, baseFile, groupPerm) != OMRSH_SUCCESS) {
			omrmem_free_memory(portLibrary, tmphandle);
			Trc_PRT_shsem_omrshsem_open_ExitWithMsg("Error: could not lock semaphore control file.");
			if (OMR_ARE_ANY_BITS_SET(flags, OMRSHSEM_OPEN_FOR_STATS | OMRSHSEM_OPEN_FOR_DESTROY | OMRSHSEM_OPEN_DO_NOT_CREATE)) {
				return OMRPORT_INFO_SHSEM_PARTIAL;
			}
			return OMRPORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_LOCK_FAILED;
		}

		/*Try to read data from the file*/
		rc = omrfile_read(portLibrary, fd, &controlinfo, sizeof(omrshsem_baseFileFormat));

		if (rc == -1) {
			/* The file is empty.*/
			if (OMR_ARE_NO_BITS_SET(flags, OMRSHSEM_OPEN_FOR_STATS | OMRSHSEM_OPEN_FOR_DESTROY | OMRSHSEM_OPEN_DO_NOT_CREATE)) {
				rc = omrshsem_createSemaphore(portLibrary, fd, isReadOnlyFD, baseFile, setSize, &controlinfo, groupPerm);
				if (rc == OMRPORT_ERROR_SHSEM_OPFAILED) {
					exitmsg = "Error: create of the semaphore has failed.";
					if (isReadOnlyFD == FALSE) {
						if (omr_unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shsem_omrshsem_open_Msg("Info: Control file is unlinked after call to omrshsem_createSemaphore.");
						} else {
							Trc_PRT_shsem_omrshsem_open_Msg("Info: Failed to unlink control file after call to omrshsem_createSemaphore.");
						}
					} else {
						Trc_PRT_shsem_omrshsem_open_Msg("Info: Control file is not unlinked after call to omrshsem_createSemaphore(isReadOnlyFD == TRUE).");
					}
				}
			} else {
				Trc_PRT_shsem_omrshsem_open_Msg("Info: Control file cannot be read. Do not attempt to create semaphore as OMRSHSEM_OPEN_FOR_STATS, OMRSHSEM_OPEN_FOR_DESTROY or OMRSHSEM_OPEN_DO_NOT_CREATE is set");
				rc = OMRPORT_ERROR_SHSEM_OPFAILED;
			}
		} else if (rc == sizeof(omrshsem_baseFileFormat)) {
			/*The control file contains data, and we have successfully read in our structure*/
			/*Note: there is no unlink here if the file is read-only ... the next function call manages this behaviour*/
			rc = omrshsem_openSemaphore(portLibrary, fd, baseFile, &controlinfo, groupPerm, OMRSH_SYSV_REGULAR_CONTROL_FILE);
			if (OMRPORT_INFO_SHSEM_OPENED != rc) {
				exitmsg = "Error: open of the semaphore has failed.";

				if ((OMRPORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK != rc)
					&& OMR_ARE_NO_BITS_SET(flags, OMRSHSEM_OPEN_FOR_STATS | OMRSHSEM_OPEN_DO_NOT_CREATE)
				) {
					/* PR 74306: If the control file is read-only, do not unlink it unless the control file was created by an older JVM without this fix,
					 * which is identified by the check for major and minor level in the control file.
					 * Reason being if the control file is read-only, JVM acquires a shared lock in omr_ControlFileOpenWithWriteLock().
					 * This implies in multiple JVM scenario it is possible that when one JVM has unlinked and created a new control file,
					 * another JVM unlinks the newly created control file, and creates another control file.
					 * For shared class cache, this may result into situations where the two JVMs, accessing the same cache,
					 * would create two different semaphore sets for same shared memory region causing one of them to detect cache as corrupt
					 * due to semid mismatch.
					 */
					if ((FALSE == isReadOnlyFD)
						|| ((OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) == OMRSH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
						&& (OMRSH_SEM_GET_MOD_MINOR_LEVEL(controlinfo.modlevel) <= OMRSH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK)
						&& (OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND == rc))
					) {
						if (omr_unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shsem_omrshsem_open_Msg("Info: Control file is unlinked after call to omrshsem_openSemaphore.");
							if (OMR_ARE_ALL_BITS_SET(flags, OMRSHSEM_OPEN_FOR_DESTROY)) {
								if (OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND != rc) {
									tmphandle->semid = controlinfo.semid;
									freeHandleOnError = FALSE;
								}
							} else if (retryIfReadOnlyCount > 1) {
								rc = OMRPORT_INFO_SHSEM_OPEN_UNLINKED;
							}
						} else {
							Trc_PRT_shsem_omrshsem_open_Msg("Info: Failed to unlink control file after call to openSemaphoe.");
						}
					} else {
						Trc_PRT_shsem_omrshsem_open_Msg("Info: Control file is not unlinked after omrshsem_openSemaphore because it is read-only and (its modLevel does not allow unlinking or rc != SEMAPHORE_NOT_FOUND).");
					}
				} else {
					Trc_PRT_shsem_omrshsem_open_Msg("Info: Control file is not unlinked after call to omrshsem_openSemaphore.");
				}
			}
		} else {
			/*Should never get here ... this means the control file was corrupted*/
			exitmsg = "Error: Semaphore control file is corrupted.";
			rc = OMRPORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_CORRUPT;

			if (OMR_ARE_NO_BITS_SET(flags, OMRSHSEM_OPEN_FOR_STATS | OMRSHSEM_OPEN_DO_NOT_CREATE)) {
				if (isReadOnlyFD == FALSE) {
					if (omr_unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
						Trc_PRT_shsem_omrshsem_open_Msg("Info: The corrupt control file was unlinked.");
						if (retryIfReadOnlyCount > 1) {
							rc = OMRPORT_INFO_SHSEM_OPEN_UNLINKED;
						}
					} else {
						Trc_PRT_shsem_omrshsem_open_Msg("Info: Failed to remove the corrupt control file.");
					}
				} else {
					Trc_PRT_shsem_omrshsem_open_Msg("Info: Corrupt control file is not unlinked (isReadOnlyFD == TRUE).");
				}
			} else {
				Trc_PRT_shsem_omrshsem_open_Msg("Info: Control file is found to be corrupt but is not unlinked (OMRSHSEM_OPEN_FOR_STATS or OMRSHSEM_OPEN_DO_NOT_CREATE is set).");
			}
		}

		if ((OMRPORT_INFO_SHSEM_CREATED != rc) && (OMRPORT_INFO_SHSEM_OPENED != rc)) {
			if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
				Trc_PRT_shsem_omrshsem_open_ExitWithMsg("Error: could not unlock semaphore control file.");
				omrmem_free_memory(portLibrary, tmphandle);
				return OMRPORT_ERROR_SHSEM_OPFAILED;
			}
			if (OMR_ARE_ANY_BITS_SET(flags, OMRSHSEM_OPEN_FOR_STATS | OMRSHSEM_OPEN_DO_NOT_CREATE)) {
				Trc_PRT_shsem_omrshsem_open_ExitWithMsg(exitmsg);
				omrmem_free_memory(portLibrary, tmphandle);
				return OMRPORT_INFO_SHSEM_PARTIAL;
			}
			if (OMR_ARE_ALL_BITS_SET(flags, OMRSHSEM_OPEN_FOR_DESTROY)) {
				Trc_PRT_shsem_omrshsem_open_ExitWithMsg(exitmsg);
				/* If we get an error while opening semaphore and thereafter unlinked control file successfully,
				 * then we may have stored the semid in tmphandle to be used by caller for error reporting.
				 * Do not free tmphandle in such case. Caller is responsible for freeing it.
				 */
				if (TRUE == freeHandleOnError) {
					omrmem_free_memory(portLibrary, tmphandle);
				}
				if (OMRPORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK == rc) {
					rc = OMRPORT_ERROR_SHSEM_OPFAILED;
				}
				return rc;
			}
			if (((isReadOnlyFD == TRUE) || (rc == OMRPORT_INFO_SHSEM_OPEN_UNLINKED)) && (retryIfReadOnlyCount > 1)) {
				/*Try loop again ... we sleep first to mimic old retry loop*/
				if (rc != OMRPORT_INFO_SHSEM_OPEN_UNLINKED) {
					Trc_PRT_shsem_omrshsem_open_Msg("Retry with usleep(100).");
					usleep(100);
				} else {
					Trc_PRT_shsem_omrshsem_open_Msg("Retry.");
				}
				continue;
			} else {
				if (retryIfReadOnlyCount <= 1) {
					Trc_PRT_shsem_omrshsem_open_Msg("Info no more retries.");
				}
				Trc_PRT_shsem_omrshsem_open_ExitWithMsg(exitmsg);
				omrmem_free_memory(portLibrary, tmphandle);
				if (OMRPORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK == rc) {
					rc = OMRPORT_ERROR_SHSEM_OPFAILED;
				}
				/* Here 'rc' can be:
				 * 	- OMRPORT_ERROR_SHSEM_OPFAILED (returned either by omrshsem_createSemaphore or omrshsem_openSemaphore)
				 * 	- OMRPORT_ERROR_SHSEM_OPFAILED_CONTROL_FILE_CORRUPT
				 * 	- OMRPORT_ERROR_SHSEM_OPFAILED_SEMID_MISMATCH (returned by omrshsem_openSemaphore)
				 * 	- OMRPORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH (returned by omrshsem_openSemaphore)
				 * 	- OMRPORT_ERROR_SHSEM_OPFAILED_SEM_SIZE_CHECK_FAILED (returned by omrshsem_openSemaphore)
				 * 	- OMRPORT_ERROR_SHSEM_OPFAILED_SEM_MARKER_CHECK_FAILED (returned by omrshsem_openSemaphore)
				 *  - OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND ( returned by omrshsem_openSemaphore)
				 */
				return rc;
			}
		}

		/*If we hit here we break out of the loop*/
		break;
	}

	omrshsem_createSemHandle(portLibrary, controlinfo.semid, controlinfo.semsetSize, baseFile, tmphandle);

	if (rc == OMRPORT_INFO_SHSEM_CREATED) {
		tmphandle->timestamp = omrfile_lastmod(portLibrary, baseFile);
	}
	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		Trc_PRT_shsem_omrshsem_open_ExitWithMsg("Error: could not unlock semaphore control file.");
		omrmem_free_memory(portLibrary, tmphandle);
		return OMRPORT_ERROR_SHSEM_OPFAILED;
	}

	if (rc == OMRPORT_INFO_SHSEM_CREATED) {
		Trc_PRT_shsem_omrshsem_open_ExitWithMsg("Successfully created a new semaphore.");
	} else {
		Trc_PRT_shsem_omrshsem_open_ExitWithMsg("Successfully opened an existing semaphore.");
	}
	*handle = tmphandle;
	return rc;
}

intptr_t 
omrshsem_deprecated_post(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	struct sembuf buffer;
	intptr_t rc;

	Trc_PRT_shsem_omrshsem_post_Entry1(handle, semset, flag, handle ? handle->semid : -1);
	if(handle == NULL) {
		Trc_PRT_shsem_omrshsem_post_Exit1();
		return OMRPORT_ERROR_SHSEM_HANDLE_INVALID;
	}

	if(semset >= handle->nsems) {
		Trc_PRT_shsem_omrshsem_post_Exit2();
		return OMRPORT_ERROR_SHSEM_SEMSET_INVALID;
	}

	buffer.sem_num = semset;
	buffer.sem_op = 1; /* post */
	if(flag & OMRPORT_SHSEM_MODE_UNDO) {
		buffer.sem_flg = SEM_UNDO;
	} else {
		buffer.sem_flg = 0;
	}

	rc = omr_semopWrapper(portLibrary,handle->semid, &buffer, 1);
	
	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number(portLibrary);
		Trc_PRT_shsem_omrshsem_post_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_omrshsem_post_Exit(rc);
	}

	return rc;
}

intptr_t
omrshsem_deprecated_wait(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, uintptr_t flag)
{
	struct sembuf buffer;
	intptr_t rc;

	Trc_PRT_shsem_omrshsem_wait_Entry1(handle, semset, flag, handle ? handle->semid : -1);
	if(handle == NULL) {
		Trc_PRT_shsem_omrshsem_wait_Exit1();
		return OMRPORT_ERROR_SHSEM_HANDLE_INVALID;
	}

	if(semset >= handle->nsems) {
		Trc_PRT_shsem_omrshsem_wait_Exit2();
		return OMRPORT_ERROR_SHSEM_SEMSET_INVALID;
	}

	buffer.sem_num = semset;
	buffer.sem_op = -1; /* wait */

	if( flag & OMRPORT_SHSEM_MODE_UNDO ) {
		buffer.sem_flg = SEM_UNDO;
	} else {
		buffer.sem_flg = 0;
	}

	if( flag & OMRPORT_SHSEM_MODE_NOWAIT ) {
		buffer.sem_flg = buffer.sem_flg | IPC_NOWAIT;
	}
    
	rc = omr_semopWrapper(portLibrary,handle->semid, &buffer, 1);
	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number(portLibrary);
		Trc_PRT_shsem_omrshsem_wait_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_omrshsem_wait_Exit(rc);
	}
	
	return rc;
}

intptr_t 
omrshsem_deprecated_getVal(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset) 
{
	intptr_t rc;
	Trc_PRT_shsem_omrshsem_getVal_Entry1(handle, semset, handle ? handle->semid : -1);
	if(handle == NULL) {
		Trc_PRT_shsem_omrshsem_getVal_Exit1();
		return OMRPORT_ERROR_SHSEM_HANDLE_INVALID;
	}

	if(semset >= handle->nsems) {
		Trc_PRT_shsem_omrshsem_getVal_Exit2();
		return OMRPORT_ERROR_SHSEM_SEMSET_INVALID; 
	}

	rc = omr_semctlWrapper(portLibrary, TRUE, handle->semid, semset, GETVAL);
	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number(portLibrary);
		Trc_PRT_shsem_omrshsem_getVal_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_omrshsem_getVal_Exit(rc);
	}
	return rc;
}

intptr_t 
omrshsem_deprecated_setVal(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, intptr_t value)
{
	semctlUnion sem_union;
	intptr_t rc;
	
	Trc_PRT_shsem_omrshsem_setVal_Entry1(handle, semset, value, handle ? handle->semid : -1);

	if(handle == NULL) {
		Trc_PRT_shsem_omrshsem_setVal_Exit1();
		return OMRPORT_ERROR_SHSEM_HANDLE_INVALID;
	}
	if(semset >= handle->nsems) {
		Trc_PRT_shsem_omrshsem_setVal_Exit2();
		return OMRPORT_ERROR_SHSEM_SEMSET_INVALID; 
	}
    
	sem_union.val = value;
	
	rc = omr_semctlWrapper(portLibrary, TRUE, handle->semid, semset, SETVAL, sem_union);

	if (-1 == rc) {
		int32_t myerrno = omrerror_last_error_number(portLibrary);
		Trc_PRT_shsem_omrshsem_setVal_Exit3(rc, myerrno);
	} else {
		Trc_PRT_shsem_omrshsem_setVal_Exit(rc);
	}
	return rc;
}

intptr_t
omrshsem_deprecated_handle_stat(struct OMRPortLibrary *portLibrary, struct omrshsem_handle *handle, struct OMRPortShsemStatistic *statbuf)
{
	intptr_t rc = OMRPORT_ERROR_SHSEM_STAT_FAILED;
	struct semid_ds seminfo;
	union semun semctlArg;

	Trc_PRT_shsem_omrshsem_deprecated_handle_stat_Entry1(handle, handle ? handle->semid : -1);

	omr_clearPortableError(portLibrary);

	if (NULL == handle) {
		Trc_PRT_shsem_omrshsem_deprecated_handle_stat_ErrorNullHandle();
		rc = OMRPORT_ERROR_SHSEM_HANDLE_INVALID;
		goto _end;
	}

	if (NULL == statbuf) {
		Trc_PRT_shsem_omrshsem_deprecated_handle_stat_ErrorNullBuffer();
		rc = OMRPORT_ERROR_SHSEM_STAT_BUFFER_INVALID;
		goto _end;
	} else {
		omrshsem_initShsemStatsBuffer(portLibrary, statbuf);
	}

	semctlArg.buf = &seminfo;
	rc = omr_semctlWrapper(portLibrary, TRUE, handle->semid, 0, IPC_STAT, semctlArg);
	if (OMRSH_FAILED == rc) {
		int32_t lasterrno = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		const char *errormsg = omrerror_last_error_message(portLibrary);
		Trc_PRT_shsem_omrshsem_deprecated_handle_stat_semctlFailed(handle->semid, lasterrno, errormsg);
		rc = OMRPORT_ERROR_SHSEM_STAT_FAILED;
		goto _end;
	}

	statbuf->semid = handle->semid;
	statbuf->ouid = seminfo.sem_perm.uid;
	statbuf->ogid = seminfo.sem_perm.gid;
	statbuf->cuid = seminfo.sem_perm.cuid;
	statbuf->cgid = seminfo.sem_perm.cgid;
	statbuf->lastOpTime = seminfo.sem_otime;
	statbuf->lastChangeTime = seminfo.sem_ctime;
	statbuf->nsems = seminfo.sem_nsems;

	/* Same code as in j9file_stat(). Probably a new function to translate operating system modes to J9 modes is required. */
	if (seminfo.sem_perm.mode & S_IWUSR) {
		statbuf->perm.isUserWriteable = 1;
	}
	if (seminfo.sem_perm.mode & S_IRUSR) {
		statbuf->perm.isUserReadable = 1;
	}
	if (seminfo.sem_perm.mode & S_IWGRP) {
		statbuf->perm.isGroupWriteable = 1;
	}
	if (seminfo.sem_perm.mode & S_IRGRP) {
		statbuf->perm.isGroupReadable = 1;
	}
	if (seminfo.sem_perm.mode & S_IWOTH) {
		statbuf->perm.isOtherWriteable = 1;
	}
	if (seminfo.sem_perm.mode & S_IROTH) {
		statbuf->perm.isOtherReadable = 1;
	}

	rc = OMRPORT_INFO_SHSEM_STAT_PASSED;

_end:
	Trc_PRT_shsem_omrshsem_deprecated_handle_stat_Exit(rc);
	return rc;
}

void 
omrshsem_deprecated_close (struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle)
{
	Trc_PRT_shsem_omrshsem_close_Entry1(*handle, (*handle) ? (*handle)->semid : -1);

	/* On Unix you don't need to close the semaphore handles*/
	if (NULL == *handle) {
		Trc_PRT_shsem_omrshsem_close_ExitNullHandle();
		return;
	}
	omrmem_free_memory(portLibrary, *handle);
	*handle=NULL;

	Trc_PRT_shsem_omrshsem_close_Exit();
}

intptr_t 
omrshsem_deprecated_destroy (struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle)
{
	/*pre: user has not closed the handle, and assume user has perm to remove */
	intptr_t rc = -1;
	intptr_t lockFile, rcunlink;
	intptr_t fd;
	BOOLEAN isReadOnlyFD;
	omrshsem_baseFileFormat controlinfo;

	Trc_PRT_shsem_omrshsem_destroy_Entry1(*handle, (*handle) ? (*handle)->semid : -1);

	if (*handle == NULL) {
		Trc_PRT_shsem_omrshsem_destroy_ExitNullHandle();
		return 0;
	}

	lockFile = omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFile, 0);
	if (lockFile == OMRSH_FILE_DOES_NOT_EXIST) {
		Trc_PRT_shsem_omrshsem_destroy_Msg("Error: control file not found");
		if (omr_semctlWrapper(portLibrary, TRUE, (*handle)->semid, 0, IPC_RMID, 0) == -1) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if ((OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL == lastError) || (OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM == lastError)) {
				Trc_PRT_shsem_omrshsem_destroy_Msg("SysV obj is already deleted");
				rc = 0;
			} else {
				Trc_PRT_shsem_omrshsem_destroy_Msg("Error: could not delete SysV obj");
				rc = -1;
			}
		} else {
			Trc_PRT_shsem_omrshsem_destroy_Msg("Deleted SysV obj");
			rc = 0;
		}
		goto omrshsem_deprecated_destroy_exitWithRC;
	} else if (lockFile != OMRSH_SUCCESS) {
		Trc_PRT_shsem_omrshsem_destroy_Msg("Error: could not open and lock control file");
		goto omrshsem_deprecated_destroy_exitWithError;
	}

	/*Try to read data from the file*/
	rc = omrfile_read(portLibrary, fd, &controlinfo, sizeof(omrshsem_baseFileFormat));
	if (rc != sizeof(omrshsem_baseFileFormat)) {
		Trc_PRT_shsem_omrshsem_destroy_Msg("Error: can not read control file");
		goto omrshsem_deprecated_destroy_exitWithErrorAndUnlock;
	}

	/* check that the modlevel and version is okay */
	if (OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) != OMRSH_MOD_MAJOR_LEVEL) {
		Trc_PRT_shsem_omrshsem_destroy_BadMajorModlevel(controlinfo.modlevel, OMRSH_MODLEVEL);
		goto omrshsem_deprecated_destroy_exitWithErrorAndUnlock;
	}

	if (controlinfo.semid == (*handle)->semid) {
		if (omr_semctlWrapper(portLibrary, TRUE, (*handle)->semid, 0, IPC_RMID, 0) == -1) {
			Trc_PRT_shsem_omrshsem_destroy_Debug2((*handle)->semid, omrerror_last_error_number(portLibrary));
			rc = -1;
		} else {
			rc = 0;
		}
	}

	if (0 == rc) {
		/* PR 74306: Allow unlinking of readonly control file if it was created by an older JVM without this fix 
		 * which is identified by different modlevel.
		 */
		if ((FALSE == isReadOnlyFD)
			|| ((OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) == OMRSH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
			&& (OMRSH_SEM_GET_MOD_MINOR_LEVEL(controlinfo.modlevel) <= OMRSH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK))
		) {
			rcunlink = omrfile_unlink(portLibrary, (*handle)->baseFile);
			Trc_PRT_shsem_omrshsem_destroy_Debug1((*handle)->baseFile, rcunlink, omrerror_last_error_number(portLibrary));
		}
	}

	omrshsem_deprecated_close(portLibrary, handle);

	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		Trc_PRT_shsem_omrshsem_destroy_Msg("Error: failed to unlock control file.");
		goto omrshsem_deprecated_destroy_exitWithError;
	}

	if (0 == rc) {
		Trc_PRT_shsem_omrshsem_destroy_Exit();
	} else {
		Trc_PRT_shsem_omrshsem_destroy_Exit1();
	}
	
omrshsem_deprecated_destroy_exitWithRC:
	return rc;

omrshsem_deprecated_destroy_exitWithErrorAndUnlock:
	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		Trc_PRT_shsem_omrshsem_destroy_Msg("Error: failed to unlock control file (after version check fail).");
	}
omrshsem_deprecated_destroy_exitWithError:
	Trc_PRT_shsem_omrshsem_destroy_Exit1();
	return -1;

}

intptr_t 
omrshsem_deprecated_destroyDeprecated (struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle, uintptr_t cacheFileType)
{
	intptr_t retval = -1;
	intptr_t fd;
	BOOLEAN isReadOnlyFD = FALSE;
	BOOLEAN fileIsLocked = FALSE;
	
	Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_Entry(*handle, (*handle)->semid);
	
	if (cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_Message("Info: cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE.");
		if (omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFile, 0) != OMRSH_SUCCESS) {
			Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_ExitWithMessage("Error: could not lock semaphore control file.");
			goto error;
		} else {
			fileIsLocked = TRUE;
		}
		if (omr_semctlWrapper(portLibrary, TRUE, (*handle)->semid, 0, IPC_RMID, 0) == -1) {
			Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_ExitWithMessage("Error: failed to remove SysV object.");
			goto error;
		}

		if (0 == omrfile_unlink(portLibrary, (*handle)->baseFile)) {
			Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_Message("Unlinked control file");
		} else {
			Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_Message("Failed to unlink control file");
		}

		omrshsem_deprecated_close(portLibrary, handle);
		retval = 0;



	error:
		if (fileIsLocked == TRUE) {
			if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
				Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_Message("Error: could not unlock semaphore control file.");
				retval = -1;
			}
		}
	} else if (cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE) {
		Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_Message("Info: cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE.");
		retval = omrshsem_deprecated_destroy(portLibrary, handle);
	} else {
		Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_BadCacheType(cacheFileType);
	}
	
	Trc_PRT_shsem_omrshsem_deprecated_destroyDeprecated_ExitWithMessage("Exit");
	return retval;
}

int32_t
omrshsem_deprecated_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

void
omrshsem_deprecated_shutdown(struct OMRPortLibrary *portLibrary)
{
	/* Don't need to do anything for now, but maybe we will need to clean up
	 * some directories, etc over here.
	 */
}

static intptr_t 
omrshsem_readOlderNonZeroByteControlFile(OMRPortLibrary *portLibrary, intptr_t fd, omrshsem_baseFileFormat * info)
{
	intptr_t rc;
	rc = omrfile_read(portLibrary, fd, info, sizeof(omrshsem_baseFileFormat));
	if (rc < 0) {
		return OMRSH_FAILED;/*READ_ERROR;*/
	} else if (rc == 0) {
		return OMRSH_FAILED;/*OMRSH_NO_DATA;*/
	} else if (rc < sizeof(omrshsem_baseFileFormat)) {
		return OMRSH_FAILED;/*OMRSH_BAD_DATA;*/
	}
	return OMRSH_SUCCESS;
}

/**
 * @internal
 * Create a new SysV semaphore
 *
 * @param[in] portLibrary
 * @param[in] fd read/write file descriptor 
 * @param[in] isReadOnlyFD set to TRUE if control file is read-only, FALSE otherwise
 * @param[in] baseFile filename matching fd
 * @param[in] setSize size of semaphore set
 * @param[out] controlinfo information about the new sysv object
 * @param[in] groupPerm group permission of the semaphore
 *
 * @return	OMRPORT_INFO_SHSEM_CREATED on success
 */
static intptr_t
omrshsem_createSemaphore(struct OMRPortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, char *baseFile, int32_t setSize, omrshsem_baseFileFormat* controlinfo, uintptr_t groupPerm)
{
	union semun sem_union;
	intptr_t i = 0;
	intptr_t j = 0;
	intptr_t projId = SHSEM_FTOK_PROJID;
	intptr_t maxProjIdRetries = 20;
	int semflags = groupPerm ? OMRSHSEM_SEMFLAGS_GROUP : OMRSHSEM_SEMFLAGS;

	Trc_PRT_shsem_omrshsem_createsemaphore_EnterWithMessage("start");

	if (isReadOnlyFD == TRUE) {
		Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Error: can not write control file b/c file descriptor is opened read only.");
		return OMRPORT_ERROR_SHSEM_OPFAILED;
	}

	if (controlinfo == NULL) {
		Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Error: controlinfo == NULL");
		return OMRPORT_ERROR_SHSEM_OPFAILED;
	}

	for (i = 0; i < maxProjIdRetries; i += 1, projId += 1) {
		int semid = -1;
		key_t fkey = -1;
		intptr_t rc = 0;

		fkey = omr_ftokWrapper(portLibrary, baseFile, projId);
		if (-1 == fkey) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOENT) {
				Trc_PRT_shsem_omrshsem_createsemaphore_Msg("ftok: ENOENT");
			} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shsem_omrshsem_createsemaphore_Msg("ftok: EACCES");
			} else {
				/*By this point other errno's should not happen*/
				Trc_PRT_shsem_omrshsem_createsemaphore_MsgWithError("ftok: unexpected errno, portable errorCode = ", lastError);
			}
			Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Error: fkey == -1");
			return OMRPORT_ERROR_SHSEM_OPFAILED;
		}

		semid = omr_semgetWrapper(portLibrary, fkey, setSize + 1, semflags);
		if (-1 == semid) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EEXIST || lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shsem_omrshsem_createsemaphore_Msg("Retry (errno == EEXIST || errno == EACCES) was found.");
				continue;
			}
			Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessageAndError("Error: semid = -1 with portable errorCode = ", lastError);
			return OMRPORT_ERROR_SHSEM_OPFAILED;
		}

		controlinfo->version = OMRSH_VERSION;
		controlinfo->modlevel = OMRSH_MODLEVEL;
		controlinfo->proj_id = projId;
		controlinfo->ftok_key = fkey;
		controlinfo->semid = semid;
		controlinfo->creator_pid = omrsysinfo_get_pid(portLibrary);
		controlinfo->semsetSize = setSize;

		if (-1 == omrfile_seek(portLibrary, fd, 0, EsSeekSet)) {
			Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Error: could not seek to start of control file");
			/* Do not set error code if semctl fails, else j9file_seek() error code will be lost. */
			if (omr_semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return OMRPORT_ERROR_SHSEM_OPFAILED;
			}
			return OMRPORT_ERROR_SHSEM_OPFAILED;
		}

		rc = omrfile_write(portLibrary, fd, controlinfo, sizeof(omrshsem_baseFileFormat));
		if (rc != sizeof(omrshsem_baseFileFormat)) {
			Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Error: write to control file failed");
			/* Do not set error code if semctl fails, else j9file_write() error code will be lost. */
			if (omr_semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return OMRPORT_ERROR_SHSEM_OPFAILED;
			}
			return OMRPORT_ERROR_SHSEM_OPFAILED;
		}

		for (j=0; j<setSize; j++) {
			struct omrshsem_handle tmphandle;
			tmphandle.semid=semid;
			tmphandle.nsems = setSize;
			tmphandle.baseFile = baseFile;
			if (portLibrary->shsem_deprecated_post(portLibrary, &tmphandle, j, OMRPORT_SHSEM_MODE_DEFAULT) != 0) {
				/* The code should never ever fail here b/c we just created, and obviously own the new semaphore.
				 * If semctl call below fails we are already in an error condition, so 
				 * don't worry about failure here.
				 * Do not set error code if semctl fails, else omrshsem_deprecated_post() error code will be lost.
				 */
				omr_semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0);
				return OMRPORT_ERROR_SHSEM_OPFAILED;
			}
		}
			
		sem_union.val = SEMMARKER_INITIALIZED;
		if (omr_semctlWrapper(portLibrary, TRUE, semid, setSize, SETVAL, sem_union) == -1) {
			Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Could not mark semaphore as initialized initialized.");
			/* The code should never ever fail here b/c we just created, and obviously own the new semaphore.
			 * If semctl call below fails we are already in an error condition, so 
			 * don't worry about failure here.
			 * Do not set error code if the next semctl fails, else the previous semctl error code will be lost.
			 */
			omr_semctlWrapper(portLibrary, FALSE, semid, 0, IPC_RMID, 0);
			return OMRPORT_ERROR_SHSEM_OPFAILED;
		}
		Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Successfully created semaphore");
		return OMRPORT_INFO_SHSEM_CREATED;
	}

	Trc_PRT_shsem_omrshsem_createsemaphore_ExitWithMessage("Error: end of retries. Clean up SysV using 'ipcs'");
	return OMRPORT_ERROR_SHSEM_OPFAILED;
}

/**
 * @internal
 * Open a new SysV semaphore
 *
 * @param[in] portLibrary
 * @param[in] fd read/write file descriptor 
 * @param[in] baseFile filename matching fd
 * @param[in] controlinfo copy of information in control file
 * @param[in] groupPerm the group permission of the semaphore
 * @param[in] cacheFileType type of control file (regular or older or older-empty)
 *
 * @return	OMRPORT_INFO_SHSEM_OPENED on success
 */
static intptr_t
omrshsem_openSemaphore(struct OMRPortLibrary *portLibrary, intptr_t fd, char *baseFile, omrshsem_baseFileFormat* controlinfo, uintptr_t groupPerm, uintptr_t cacheFileType)
{
	/*The control file contains data, and we have successfully read in our structure*/
	int semid = -1;
	int semflags = groupPerm ? OMRSHSEM_SEMFLAGS_GROUP_OPEN : OMRSHSEM_SEMFLAGS_OPEN;
	intptr_t rc = -1;
#if defined (J9ZOS390)
	IPCQPROC info;
#endif
	union semun semctlArg;
	struct semid_ds buf;
	semctlArg.buf = &buf;

	Trc_PRT_shsem_omrshsem_opensemaphore_EnterWithMessage("Start");

	/* omrshsem_openSemaphore can only open semaphores with the same major level */
	if ((OMRSH_SYSV_REGULAR_CONTROL_FILE == cacheFileType) &&
		(OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo->modlevel) != OMRSH_MOD_MAJOR_LEVEL)
	) {
		Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("Error: major modlevel mismatch.");
		/* Clear any stale portlibrary error code */
		omr_clearPortableError(portLibrary);
		goto failDontUnlink;
	}

	semid = omr_semgetWrapper(portLibrary, controlinfo->ftok_key, 0/*don't care*/, semflags);
	if (-1 == semid) {
		int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOENT) || (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
			/*The semaphore set was deleted, but the control file still exists*/
			Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("The shared SysV obj was deleted, but the control file still exists.");
			rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND;
			goto fail;
		} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
			Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("Info: EACCES occurred.");
		} else {
			/*Any other errno will result is an error*/
			Trc_PRT_shsem_omrshsem_opensemaphore_MsgWithError("Error: can not open shared semaphore (semget failed), portable errorCode = ", lastError);
			goto failDontUnlink;
		}
	}

	do {

		if (semid != -1 && semid != controlinfo->semid) {
			/*semid does not match value in the control file.*/
			Trc_PRT_shsem_omrshsem_opensemaphore_Msg("The SysV id does not match our control file.");
			/* Clear any stale portlibrary error code */
			omr_clearPortableError(portLibrary);
			rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEMID_MISMATCH;
			goto fail;
		}

		/*If our sysv obj id is gone, or our <key, id> pair is invalid, then we can also recover*/
		rc = omr_semctlWrapper(portLibrary, TRUE, controlinfo->semid, 0, IPC_STAT, semctlArg);
		if (-1 == rc) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) || (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
				/*the sysv obj may have been removed just before, or during, our sXmctl call. In this case our key is free to be reused.*/
				Trc_PRT_shsem_omrshsem_opensemaphore_Msg("The shared SysV obj was deleted, but the control file still exists.");
				rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND;
				goto fail;
			} else {
				if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (semid != -1)) {
					Trc_PRT_shsem_omrshsem_opensemaphore_Msg("The SysV obj may have been modified since the call to sXmget.");
					rc = OMRPORT_ERROR_SHSEM_OPFAILED;
					goto fail;
				}

				/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
				Trc_PRT_shsem_omrshsem_opensemaphore_MsgWithError("Error: semctl failed. Can not open shared semaphore, portable errorCode = ", lastError);
				goto failDontUnlink;
			}
		} else {
#if (defined(__GNUC__) || defined(AIXPPC) || defined(J9ZTPF)) && !defined(J9ZOS390)
#if defined(OSX)
			/*Use _key for OSX*/
			if (buf.sem_perm._key != controlinfo->ftok_key)
#elif defined(AIXPPC)
			/*Use .key for AIXPPC*/
			if (buf.sem_perm.key != controlinfo->ftok_key)
#elif defined(OMRZTPF)
			/*Use .key for z/TPF */
			if (buf.key != controlinfo->ftok_key)
#elif defined(__GNUC__)
			/*Use .__key for __GNUC__*/
			if (buf.sem_perm.__key != controlinfo->ftok_key)
#endif
			{
				Trc_PRT_shsem_omrshsem_opensemaphore_Msg("The <key,id> pair in our control file is no longer valid.");
				/* Clear any stale portlibrary error code */
				omr_clearPortableError(portLibrary);
				rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH;
				goto fail;
			}
#endif
#if defined (J9ZOS390)
			if (omr_getipcWrapper(portLibrary, controlinfo->semid, &info, sizeof(IPCQPROC), IPCQALL) == -1) {
				int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
				if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
					Trc_PRT_shsem_omrshsem_opensemaphore_Msg("__getipc(): The shared SysV obj was deleted, but the control file still exists.");
					rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEMAPHORE_NOT_FOUND;
					goto fail;
				} else {
					if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (semid != -1)) {
						Trc_PRT_shsem_omrshsem_opensemaphore_Msg("__getipc(): has returned EACCES.");
						rc = OMRPORT_ERROR_SHSEM_OPFAILED;
						goto fail;
					}/*Any other error and we will terminate*/

					/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
					Trc_PRT_shsem_omrshsem_opensemaphore_MsgWithError("Error: __getipc() failed. Can not open shared shared semaphore, portable errorCode = ", lastError);
					goto failDontUnlink;
				}
			} else {
				if (info.sem.ipcqkey != controlinfo->ftok_key) {
					Trc_PRT_shsem_omrshsem_opensemaphore_Msg("The <key,id> pair in our control file is no longer valid.");
					rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEM_KEY_MISMATCH;
					goto fail;
				}
			}
#endif
			/*Things are looking good so far ...  continue on and do other checks*/
		}

		rc = omrshsem_checkSize(portLibrary, controlinfo->semid, controlinfo->semsetSize);
		if (rc == 0) {
			Trc_PRT_shsem_omrshsem_opensemaphore_Msg("The size does not match our control file.");
			/* Clear any stale portlibrary error code */
			omr_clearPortableError(portLibrary);
			rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEM_SIZE_CHECK_FAILED;
			goto fail;
		} else if (rc == -1) {
			Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("Error: checkSize failed during semctl.");
			goto failDontUnlink;
		} /*Continue looking for opportunity to re-create*/

		rc = omrshsem_checkMarker(portLibrary, controlinfo->semid, controlinfo->semsetSize);
		if (rc == 0) {
			Trc_PRT_shsem_omrshsem_opensemaphore_Msg("The marker semaphore does not match our expected value.");
			/* Clear any stale portlibrary error code */
			omr_clearPortableError(portLibrary);
			rc = OMRPORT_ERROR_SHSEM_OPFAILED_SEM_MARKER_CHECK_FAILED;
			goto fail;
		} else if (rc == -1) {
			Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("Error: checkMarker failed during semctl.");
			goto failDontUnlink;
		}
		/*No more checks ... We want this loop to exec only once ... so we break ...*/
	} while (FALSE);

	if (-1 == semid) {
		goto failDontUnlink;
	}
	
	Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("Successfully opened semaphore.");
	return OMRPORT_INFO_SHSEM_OPENED;
fail:
	Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("Error: can not open shared semaphore (semget failed). Caller MAY unlink the control file");
	return rc;
failDontUnlink:
	Trc_PRT_shsem_omrshsem_opensemaphore_ExitWithMessage("Error: can not open shared semaphore (semget failed). Caller should not unlink the control file");
	return OMRPORT_ERROR_SHSEM_OPFAILED_DONT_UNLINK;
}

/**
 * @internal
 * Create a semaphore handle for omrshsem_open() caller ...
 *
 * @param[in] portLibrary
 * @param[in] semid id of SysV object
 * @param[in] nsems number of semaphores in the semaphore set
 * @param[in] baseFile file being used for control file
 * @param[out] handle the handle obj
 *
 * @return	void
 */
static void
omrshsem_createSemHandle(struct OMRPortLibrary *portLibrary, int semid, int nsems, char* baseFile, omrshsem_handle * handle) 
{
	intptr_t baseFileLength = strlen(baseFile) + 1;

	Trc_PRT_shsem_omrshsem_createSemHandle_Entry(baseFile, semid, nsems);

	handle->semid = semid;
	handle->nsems = nsems;

	omrstr_printf(portLibrary, handle->baseFile, baseFileLength, "%s", baseFile);

	Trc_PRT_shsem_omrshsem_createSemHandle_Exit(OMRSH_SUCCESS);
}

/**
 * @internal
 * Check for SEMMARKER at the last semaphore in the set 
 *
 * @param[in] portLibrary The port library
 * @param[in] semid semaphore set id
 * @param[in] semsetsize last semaphore in set
 *
 * @return	-1 if we can not get the value of the semaphore, 1 if it matches SEMMARKER, 0 if it does not match
 */
static intptr_t 
omrshsem_checkMarker(OMRPortLibrary *portLibrary, int semid, int semsetsize)
{
	intptr_t rc;
	rc = omr_semctlWrapper(portLibrary, TRUE, semid, semsetsize, GETVAL);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (rc == SEMMARKER_INITIALIZED) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Check for size of the semaphore set
 *
 * @param[in] portLibrary The port library
 * @param[in] semid semaphore set id
 * @param[in] numsems
 *
 * @return	-1 if we can get info from semid, 1 if it match is found, 0 if it does not match
 */
static intptr_t 
omrshsem_checkSize(OMRPortLibrary *portLibrary, int semid, int32_t numsems)
{
	intptr_t rc;
	union semun semctlArg;
	struct semid_ds buf;

	semctlArg.buf = &buf;

	rc = omr_semctlWrapper(portLibrary, TRUE, semid, 0, IPC_STAT, semctlArg);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (buf.sem_nsems == (numsems + 1)) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Initialize OMRPortShsemStatistic with default values.
 *
 * @param[in] portLibrary
 * @param[out] statbuf Pointer to OMRPortShsemStatistic to be initialized
 *
 * @return void
 */
static void
omrshsem_initShsemStatsBuffer(OMRPortLibrary *portLibrary, OMRPortShsemStatistic *statbuf)
{
	if (NULL != statbuf) {
		memset(statbuf, 0, sizeof(OMRPortShsemStatistic));
	}
}

int32_t 
omrshsem_deprecated_getid (struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle) 
{
	return handle->semid;
}

