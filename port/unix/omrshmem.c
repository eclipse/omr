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
 * @brief Shared Memory
 */


#include "omrport.h"
#include "ut_omrport.h"

#include <sys/types.h>
#include <sys/stat.h>
#if !defined(OMRZTPF)
#include <sys/ipc.h>
#include <sys/shm.h>
#else /* !defined(OMRZTPF) */
#include <tpf/i_shm.h>
#endif /* !defined(OMRZTPF) */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>

#include "portnls.h"
#include "omrportpriv.h"
#include "omrshmem.h"
#include "omrsharedhelper.h"
#include "protect_helpers.h"
#include "omrsysv_ipcwrappers.h"
#include "omrshchelp.h"
#include "omrportpg.h"

#define CREATE_ERROR -10
#define OPEN_ERROR  -11
#define WRITE_ERROR -12
#define READ_ERROR -13
#define MALLOC_ERROR -14

#define OMRSH_NO_DATA -21
#define OMRSH_BAD_DATA -22

#define RETRY_COUNT 10

#define SHMEM_FTOK_PROJID 0x61
#define OMRSH_DEFAULT_CTRL_ROOT "/tmp"

/*
 * The following set of defines construct the flags required for shmget
 * for the various sysvipc shared memory platforms.
 * The settings are determined by:
 *    o  whether we are running on zOS
 *    o  if on zOS whether APAR OA11519 is installed 
 *    o  if zOS whether we are building a 31 or 64 bit JVM
 *  There are default values for all other platforms.
 * 
 * Note in particular the inclusion of __IPC_MEGA on zOS systems.  If we 
 * are building a 31 bit JVM this is required.  However, it must NOT be 
 * included for the 64 bit zOS JVM.  This is because __IPC_MEGA allocates 
 * the shared memory below the 2GB line.  This is OK for processes running
 * in key 8 (such as Websphere server regions), but key 2 processes, such as
 * the Websphere control region, can only share memory ABOVE the 2GB line.
 * Therefore __IPC_MEGA must not be specified for the zOS 64 bit JVM.
 */


#define SHMFLAGS_BASE_COMMON (IPC_CREAT | IPC_EXCL)

#if defined(OMRZOS390)
#if (__EDC_TARGET >= __EDC_LE4106) /* For zOS system with APAR OA11519 */
#if defined(OMRZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS_BASE (SHMFLAGS_BASE_COMMON | __IPC_SHAREAS)
#else  /* defined(OMRZOS39064) */
#define SHMFLAGS_BASE (SHMFLAGS_BASE_COMMON | __IPC_SHAREAS | __IPC_MEGA)
#endif /* defined(OMRZOS39064) */
#else  /* (__EDC_TARGET >= __EDC_LE4106) */
#if defined(OMRZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS_BASE SHMFLAGS_BASE_COMMON
#else  /* defined(OMRZOS39064) */
#define SHMFLAGS_BASE (SHMFLAGS_BASE_COMMON | __IPC_MEGA)
#endif /* defined(OMRZOS39064) */
#endif /* (__EDC_TARGET >= __EDC_LE4106) */
#else  /* defined(OMRZOS390) */
#define SHMFLAGS_BASE SHMFLAGS_BASE_COMMON
#endif /* defined(OMRZOS390) */

#if defined(OMRZOS390)
#if (__EDC_TARGET >= __EDC_LE4106) /* For zOS system with APAR OA11519 */
#if defined(OMRZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | __IPC_SHAREAS | S_IRUSR | S_IWUSR)
#define SHMFLAGS_NOSHAREAS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | __IPC_SHAREAS | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define SHMFLAGS_GROUP_NOSHAREAS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#else
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | __IPC_SHAREAS | S_IRUSR | S_IWUSR)
#define SHMFLAGS_NOSHAREAS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | __IPC_MEGA | __IPC_SHAREAS | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define SHMFLAGS_GROUP_NOSHAREAS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#endif
#else /* For zOS systems without APAR OA11519 */
#if defined(OMRZOS39064) /* For 64 bit systems must not specify __IPC_MEGA */
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#else
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | __IPC_MEGA | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#endif
#endif
#elif defined(OMRZTPF)
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | TPF_IPC64)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | TPF_IPC64 )
#else /* For non-zOS systems */
#define SHMFLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define SHMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#endif

#define SHMFLAGS_READONLY (S_IRUSR | S_IRGRP | S_IROTH)
#define SHMFLAGS_USERRW (SHMFLAGS_READONLY | S_IWUSR)
#define SHMFLAGS_GROUPRW (SHMFLAGS_USERRW | S_IWGRP)

#if !defined(OMRZOS390)
#define __errno2() 0
#endif

static int32_t omrshmem_createSharedMemory(OMRPortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, const char *baseFile, int32_t size, int32_t perm, struct omrshmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct omrshmem_handle *handle);
static intptr_t omrshmem_checkSize(OMRPortLibrary *portLibrary, int shmid, int64_t size);
static intptr_t omrshmem_openSharedMemory (OMRPortLibrary *portLibrary, intptr_t fd, const char *baseFile, int32_t perm, struct omrshmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct omrshmem_handle *handle, BOOLEAN *canUnlink, uintptr_t cacheFileType);
static void omrshmem_getControlFilePath(struct OMRPortLibrary* portLibrary, const char* cacheDirName, char* buffer, uintptr_t size, const char* name);
static void omrshmem_createshmHandle(OMRPortLibrary *portLibrary, int32_t shmid, const char *controlFile, OMRMemCategory * category, uintptr_t size, omrshmem_handle * handle, uint32_t perm);
static intptr_t omrshmem_checkGid(OMRPortLibrary *portLibrary, int shmid, int32_t gid);
static intptr_t omrshmem_checkUid(OMRPortLibrary *portLibrary, int shmid, int32_t uid);
static uintptr_t omrshmem_isControlFileName(struct OMRPortLibrary* portLibrary, char* buffer);
static intptr_t omrshmem_readControlFile(OMRPortLibrary *portLibrary, intptr_t fd, omrshmem_controlFileFormat * info);
static intptr_t omrshmem_readOlderNonZeroByteControlFile(OMRPortLibrary *portLibrary, intptr_t fd, omrshmem_controlBaseFileFormat * info);
static void omrshmem_initShmemStatsBuffer(OMRPortLibrary *portLibrary, OMRPortShmemStatistic *statbuf);
static intptr_t omrshmem_getShmStats(OMRPortLibrary *portLibrary, int shmid, struct OMRPortShmemStatistic* statbuf);

/* In omrshchelp.c */
extern uintptr_t omrshmem_isCacheFileName(OMRPortLibrary *, const char *, uintptr_t, const char *);

intptr_t
omrshmem_openDeprecated (struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle, const char* rootname, uint32_t perm, uintptr_t cacheFileType, uint32_t categoryCode)
{
	intptr_t retval = OMRPORT_ERROR_SHMEM_OPFAILED;
	intptr_t handleStructLength = sizeof(struct omrshmem_handle);
	char baseFile[OMRSH_MAXPATH];
	struct omrshmem_handle * tmphandle = NULL;
	intptr_t baseFileLength = 0;
	intptr_t fd = -1;
	BOOLEAN fileIsLocked = FALSE;
	BOOLEAN isReadOnlyFD = FALSE;
	OMRMemCategory * category = omrmem_get_category(portLibrary, categoryCode);
	struct shmid_ds shminfo;

	Trc_PRT_shmem_omrshmem_openDeprecated_Entry();

	omr_clearPortableError(portLibrary);

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_omrshmem_openDeprecated_ExitNullCacheDirName();
		return OMRPORT_ERROR_SHMEM_OPFAILED;
	}

	omrshmem_getControlFilePath(portLibrary, cacheDirName, baseFile, OMRSH_MAXPATH, rootname);

	baseFileLength = strlen(baseFile) + 1;
	tmphandle = omrmem_allocate_memory(portLibrary, (handleStructLength + baseFileLength), OMR_GET_CALLSITE(),
					   categoryCode);
	if (NULL == tmphandle) {
		Trc_PRT_shmem_omrshmem_openDeprecated_Message("Error: could not alloc handle.");
		goto error;
	}
	tmphandle->baseFileName = (char *) (((char *) tmphandle) + handleStructLength);
	tmphandle->controlStorageProtectKey = 0;
	tmphandle->currentStorageProtectKey = 0;

	if (omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, baseFile, 0) != OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_openDeprecated_Message("Error: could not lock shared memory control file.");
		goto error;
	} else {
		fileIsLocked = TRUE;
	}
	if (cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		int shmid = -1;
		int32_t shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
		int projid = 0xde;
		key_t fkey;
		uintptr_t size = 0;

		fkey = omr_ftokWrapper(portLibrary, baseFile, projid);
		if (fkey == -1) {
			Trc_PRT_shmem_omrshmem_openDeprecated_Message("Error: ftok failed.");
			goto error;
		}
		shmflags &= ~IPC_CREAT;

		/*size of zero means use current size of existing object*/
		shmid = omr_shmgetWrapper(portLibrary, fkey, 0, shmflags);

#if defined (OMRZOS390)
		if(shmid == -1) {
			/* zOS doesn't ignore shmget flags that is doesn't understand. Doh. So for share classes to work */
			/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
			shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
			shmflags &= ~IPC_CREAT;
			shmid = omr_shmgetWrapper(portLibrary, fkey, 0, shmflags);
		}
#endif
		if (-1 == shmid) {
			int32_t lasterrno = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			const char * errormsg = omrerror_last_error_message(portLibrary);
			Trc_PRT_shmem_omrshmem_openDeprecated_shmgetFailed(fkey,lasterrno,errormsg);
			if (lasterrno == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
				Trc_PRT_shmem_omrshmem_openDeprecated_shmgetEINVAL(fkey);
				retval = OMRPORT_INFO_SHMEM_PARTIAL;
				/*Clean up the control file since its SysV object is deleted.*/
				omrfile_unlink(portLibrary, baseFile);
			}
			goto error;
		}

		if (omr_shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &shminfo) == -1) {
			Trc_PRT_shmem_omrshmem_openDeprecated_Message("Error: shmctl failed.");
			goto error;
		}
		size = shminfo.shm_segsz;

		omrshmem_createshmHandle(portLibrary, shmid, baseFile, category, size, tmphandle, perm);
		tmphandle->timestamp = omrfile_lastmod(portLibrary,baseFile);
		retval = OMRPORT_INFO_SHMEM_OPENED;
	} else if (cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE) {
		BOOLEAN canUnlink = FALSE;
		omrshmem_controlBaseFileFormat controlinfo;
		uintptr_t size = 0;
		struct shmid_ds shminfo;
		shminfo.shm_segsz = 0; // MARK: this initialization
				       // may screw things up. but on
				       // volvo, it's necessary for
				       // OMR to compile.
		
		if (omrshmem_readOlderNonZeroByteControlFile(portLibrary, fd, &controlinfo) != OMRSH_SUCCESS) {
			Trc_PRT_shmem_omrshmem_openDeprecated_Message("Error: could not read deprecated control file.");
			goto error;
		}

		retval = omrshmem_openSharedMemory(portLibrary, fd, baseFile, perm, (omrshmem_controlFileFormat *)&controlinfo, groupPerm, tmphandle, &canUnlink, cacheFileType);
		if (OMRPORT_INFO_SHMEM_OPENED != retval) {
			if (FALSE == canUnlink) {
				Trc_PRT_shmem_omrshmem_openDeprecated_Message("Control file was not unlinked.");
				retval = OMRPORT_ERROR_SHMEM_OPFAILED;
			} else {
				/*Clean up the control file */
				if (-1 == omrfile_unlink(portLibrary, baseFile)) {
					Trc_PRT_shmem_omrshmem_openDeprecated_Message("Control file could not be unlinked.");
				} else {
					Trc_PRT_shmem_omrshmem_openDeprecated_Message("Control file was unlinked.");
				}
				retval = OMRPORT_INFO_SHMEM_PARTIAL;
			}
			goto error;
		}

		size = shminfo.shm_segsz;

		omrshmem_createshmHandle(portLibrary, controlinfo.shmid, baseFile, category, size, tmphandle, perm);
		tmphandle->timestamp = omrfile_lastmod(portLibrary,baseFile);
		retval = OMRPORT_INFO_SHMEM_OPENED;
	} else {
		Trc_PRT_shmem_omrshmem_openDeprecated_BadCacheType(cacheFileType);
	}
	
error:
	if (fileIsLocked == TRUE) {
		if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
			Trc_PRT_shmem_omrshmem_openDeprecated_Message("Error: could not unlock shared memory control file.");
			retval = OMRPORT_ERROR_SHMEM_OPFAILED;
		}
	}
	if (retval != OMRPORT_INFO_SHMEM_OPENED){
		if (NULL != tmphandle) {
			omrmem_free_memory(portLibrary, tmphandle);
		}
		*handle = NULL;
		Trc_PRT_shmem_omrshmem_openDeprecated_Exit("Exit: failed to open older shared memory");
	} else {
		*handle = tmphandle;
		Trc_PRT_shmem_omrshmem_openDeprecated_Exit("Open old generation of shared memory successfully.");
	}
	return retval;
}

intptr_t
omrshmem_open (OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle, const char *rootname, uintptr_t size, uint32_t perm, uint32_t categoryCode, uintptr_t flags, OMRControlFileStatus *controlFileStatus)
{
	/* TODO: needs to be longer? dynamic?*/
	char baseFile[OMRSH_MAXPATH];
	intptr_t fd;
	struct omrshmem_controlFileFormat controlinfo;
	int32_t rc;
	BOOLEAN isReadOnlyFD;
	intptr_t baseFileLength = 0;
	intptr_t handleStructLength = sizeof(struct omrshmem_handle);
	char * exitmsg = 0;
	intptr_t retryIfReadOnlyCount = 10;
	struct omrshmem_handle * tmphandle = NULL;
	OMRMemCategory * category = omrmem_get_category(portLibrary, categoryCode);
	BOOLEAN freeHandleOnError = TRUE;

	Trc_PRT_shmem_omrshmem_open_Entry(rootname, size, perm);

	omr_clearPortableError(portLibrary);

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_omrshmem_open_ExitNullCacheDirName();
		return OMRPORT_ERROR_SHMEM_OPFAILED;
	}

	omrshmem_getControlFilePath(portLibrary, cacheDirName, baseFile, OMRSH_MAXPATH, rootname);

	if (handle == NULL) {
		Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Error: handle is NULL.");
		return OMRPORT_ERROR_SHMEM_OPFAILED;
	}
	baseFileLength = strlen(baseFile) + 1;
	*handle = NULL;
	tmphandle = omrmem_allocate_memory(portLibrary, (handleStructLength + baseFileLength), OMR_GET_CALLSITE(),
					   categoryCode);
	if (NULL == tmphandle) {
		Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Error: could not alloc handle.");
		return OMRPORT_ERROR_SHMEM_OPFAILED;
	}
	tmphandle->baseFileName = (char *) (((char *) tmphandle) + handleStructLength);
	tmphandle->controlStorageProtectKey = 0;
	tmphandle->currentStorageProtectKey = 0;
	tmphandle->flags = flags;
	
#if defined(OMRZOS390)
	if ((flags & OMRSHMEM_STORAGE_KEY_TESTING) != 0) {
		tmphandle->currentStorageProtectKey = (flags >> OMRSHMEM_STORAGE_KEY_TESTING_SHIFT) & OMRSHMEM_STORAGE_KEY_TESTING_MASK;			
	} else {
		tmphandle->currentStorageProtectKey = getStorageKey();
	}	
#endif
	
	if (NULL != controlFileStatus) {
		memset(controlFileStatus, 0, sizeof(OMRControlFileStatus));
	}

	for (retryIfReadOnlyCount = 10; retryIfReadOnlyCount > 0; retryIfReadOnlyCount -= 1) {
		/*Open control file with write lock*/
		BOOLEAN canCreateFile = TRUE;
		if ((perm == OMRSH_SHMEM_PERM_READ) || OMR_ARE_ANY_BITS_SET(flags, OMRSHMEM_OPEN_FOR_STATS | OMRSHMEM_OPEN_FOR_DESTROY | OMRSHMEM_OPEN_DO_NOT_CREATE)) {
			canCreateFile = FALSE;
		}
		if (omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, canCreateFile, baseFile, groupPerm) != OMRSH_SUCCESS) {
			Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Error: could not lock memory control file.");
			omrmem_free_memory(portLibrary, tmphandle);
			return OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_LOCK_FAILED;
		}
		/*Try to read data from the file*/
		rc = omrshmem_readControlFile(portLibrary, fd, &controlinfo);
		if (rc == OMRSH_ERROR) {
			/* The file is empty.*/
			if (OMR_ARE_NO_BITS_SET(flags, OMRSHMEM_OPEN_FOR_STATS | OMRSHMEM_OPEN_FOR_DESTROY | OMRSHMEM_OPEN_DO_NOT_CREATE)) {
				rc = omrshmem_createSharedMemory(portLibrary, fd, isReadOnlyFD, baseFile, size, perm, &controlinfo, groupPerm, tmphandle);
				if (rc == OMRPORT_ERROR_SHMEM_OPFAILED) {
					exitmsg = "Error: create of the memory has failed.";
					if (perm == OMRSH_SHMEM_PERM_READ) {
						Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was not unlinked (OMRSH_SHMEM_PERM_READ).");
					} else if (isReadOnlyFD == FALSE) {
						if (omr_unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file is unlinked after call to omrshmem_createSharedMemory.");
						} else {
							Trc_PRT_shmem_omrshmem_open_Msg("Info: Failed to unlink control file after call to omrshmem_createSharedMemory.");
						}
					} else {
						Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was not unlinked (isReadOnlyFD == TRUE).");
					}
				}
			} else {
				Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file cannot be read. Do not attempt to create shared memory as OMRSHMEM_OPEN_FOR_STATS, OMRSHMEM_OPEN_FOR_DESTROY or OMRSHMEM_OPEN_DO_NOT_CREATE is set");
				rc = OMRPORT_ERROR_SHMEM_OPFAILED;
			}
		} else if (rc == OMRSH_SUCCESS) {
			BOOLEAN canUnlink = FALSE;
			
			/*The control file contains data, and we have successfully read in our structure*/
			rc = omrshmem_openSharedMemory(portLibrary, fd, baseFile, perm, &controlinfo, groupPerm, tmphandle, &canUnlink, OMRSH_SYSV_REGULAR_CONTROL_FILE);
			if (OMRPORT_INFO_SHMEM_OPENED != rc) {
				exitmsg = "Error: open of the memory has failed.";

				if ((TRUE == canUnlink)
					&& (OMRPORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY != rc)
					&& OMR_ARE_NO_BITS_SET(flags, OMRSHMEM_OPEN_FOR_STATS | OMRSHMEM_OPEN_DO_NOT_CREATE)
				) {
					if (perm == OMRSH_SHMEM_PERM_READ) {
						Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was not unlinked (OMRSH_SHMEM_PERM_READ is set).");
					} else {
						/* PR 74306: If the control file is read-only, do not unlink it unless the control file was created by an older JVM without this fix,
						 * which is identified by the check for major and minor level in the control file.
						 * Reason being if the control file is read-only, JVM acquires a shared lock in omr_ControlFileOpenWithWriteLock().
						 * This implies in multiple JVM scenario it is possible that when one JVM has unlinked and created a new control file,
						 * another JVM unlinks the newly created control file, and creates another control file.
						 * For shared class cache, this may result into situations where the two JVMs will have their own shared memory regions
						 * but would be using same semaphore set for synchronization, thereby impacting each other's performance.
						 */
						if ((FALSE == isReadOnlyFD)
							|| ((OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo.common.modlevel) == OMRSH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
							&& (OMRSH_SHM_GET_MOD_MINOR_LEVEL(controlinfo.common.modlevel) <= OMRSH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK)
							&& (OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND == rc))
						) {
							if (omr_unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
								Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was unlinked after call to omrshmem_openSharedMemory.");
								if (OMR_ARE_ALL_BITS_SET(flags, OMRSHMEM_OPEN_FOR_DESTROY)) {
									if (OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND != rc) {
										tmphandle->shmid = controlinfo.common.shmid;
										freeHandleOnError = FALSE;
									}
								} else if (retryIfReadOnlyCount > 1) {
									rc = OMRPORT_INFO_SHMEM_OPEN_UNLINKED;
								}
							} else {
								Trc_PRT_shmem_omrshmem_open_Msg("Info: Failed to unlink control file after call to omrshmem_openSharedMemory.");
							}
						} else {
							Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was not unlinked after omrshmem_openSharedMemory because it is read-only and (its modLevel does not allow unlinking or rc != SHARED_MEMORY_NOT_FOUND).");
						}
					}
				} else {
					Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was not unlinked after call to omrshmem_openSharedMemory.");
				}
			}
		} else {
			/*Should never get here ... this means the control file was corrupted*/
			exitmsg = "Error: Memory control file is corrupted.";
			rc = OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT;

			if (OMR_ARE_NO_BITS_SET(flags, OMRSHMEM_OPEN_FOR_STATS | OMRSHMEM_OPEN_DO_NOT_CREATE)) {
				if (perm == OMRSH_SHMEM_PERM_READ) {
					Trc_PRT_shmem_omrshmem_open_Msg("Info: Corrupt control file was not unlinked (OMRSH_SHMEM_PERM_READ is set).");
				} else {
					if (isReadOnlyFD == FALSE) {
						if (omr_unlinkControlFile(portLibrary, baseFile, controlFileStatus)) {
							Trc_PRT_shmem_omrshmem_open_Msg("Info: The corrupt control file was unlinked.");
							if (retryIfReadOnlyCount > 1) {
								rc = OMRPORT_INFO_SHMEM_OPEN_UNLINKED;
							}
						} else {
							Trc_PRT_shmem_omrshmem_open_Msg("Info: Failed to remove the corrupt control file.");
						}
					} else {
						Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was not unlinked(isReadOnlyFD == TRUE).");
					}
				}
			} else {
				Trc_PRT_shmem_omrshmem_open_Msg("Info: Control file was found to be corrupt but was not unlinked (OMRSHMEM_OPEN_FOR_STATS or OMRSHMEM_OPEN_DO_NOT_CREATE is set).");
			}
		}
		if ((OMRPORT_INFO_SHMEM_CREATED != rc) && (OMRPORT_INFO_SHMEM_OPENED != rc)) {
			if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
				Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Error: could not unlock memory control file.");
				omrmem_free_memory(portLibrary, tmphandle);
				return OMRPORT_ERROR_SHMEM_OPFAILED;
			}
			if (OMR_ARE_ANY_BITS_SET(flags, OMRSHMEM_OPEN_FOR_STATS | OMRSHMEM_OPEN_FOR_DESTROY | OMRSHMEM_OPEN_DO_NOT_CREATE)
				|| (OMRPORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY == rc)
			) {
				Trc_PRT_shmem_omrshmem_open_ExitWithMsg(exitmsg);
				/* If we get an error while opening shared memory and thereafter unlinked control file successfully,
				 * then we may have stored the shmid in tmphandle to be used by caller for error reporting.
				 * Do not free tmphandle in such case. Caller is responsible for freeing it.
				 */
				if (TRUE == freeHandleOnError) {
					omrmem_free_memory(portLibrary, tmphandle);
				}
				return rc;
			}
			if (((isReadOnlyFD == TRUE) || (rc == OMRPORT_INFO_SHMEM_OPEN_UNLINKED)) && (retryIfReadOnlyCount > 1)) {
				/*Try loop again ... we sleep first to mimic old retry loop*/
				if (rc != OMRPORT_INFO_SHMEM_OPEN_UNLINKED) {
					Trc_PRT_shmem_omrshmem_open_Msg("Retry with usleep(100).");
					usleep(100);
				} else {
					Trc_PRT_shmem_omrshmem_open_Msg("Retry.");
				}
				continue;
			} else {
				if (retryIfReadOnlyCount <= 1) {
					Trc_PRT_shmem_omrshmem_open_Msg("Info no more retries.");
				}
				Trc_PRT_shmem_omrshmem_open_ExitWithMsg(exitmsg);
				omrmem_free_memory(portLibrary, tmphandle);
				/* Here 'rc' can be:
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED (returned either by omrshmem_createSharedMemory or omrshmem_openSharedMemory)
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED_CONTROL_FILE_CORRUPT
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH (returned by omrshmem_openSharedMemory)
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH (returned by omrshmem_openSharedMemory)
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED (returned by omrshmem_openSharedMemory)
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED (returned by omrshmem_openSharedMemory)
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED (returned by omrshmem_openSharedMemory)
				 * 	- OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND (returned by omrshmem_openSharedMemory)
				 */
				return rc;
			}
		}
		/*If we hit here we break out of the loop*/
		break;
	}
	omrshmem_createshmHandle(portLibrary, controlinfo.common.shmid, baseFile, category, size, tmphandle, perm);
	if (rc == OMRPORT_INFO_SHMEM_CREATED) {
		tmphandle->timestamp = omrfile_lastmod(portLibrary, baseFile);
	}
	if (omrshmem_attach(portLibrary, tmphandle, categoryCode) == NULL) {
		/* The call must clean up the handle in this case by calling omrshxxx_destroy */
		if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
			Trc_PRT_shmem_omrshmem_open_Msg("Error: could not unlock memory control file.");
		}
		if (rc == OMRPORT_INFO_SHMEM_CREATED) {
			rc = OMRPORT_ERROR_SHMEM_CREATE_ATTACHED_FAILED;
			Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Error: Created shared memory, but could not attach.");
		} else {
			rc = OMRPORT_ERROR_SHMEM_OPEN_ATTACHED_FAILED;
			Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Error: Opened shared memory, but could not attach.");;
		}
		*handle = tmphandle;
		return rc;
	} else {
		Trc_PRT_shmem_omrshmem_open_Msg("Attached to shared memory");
	}
	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Error: could not unlock memory control file.");
		omrmem_free_memory(portLibrary, tmphandle);
		return OMRPORT_ERROR_SHMEM_OPFAILED;
	}
	if (rc == OMRPORT_INFO_SHMEM_CREATED) {
		Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Successfully created a new memory.");
	} else {
		Trc_PRT_shmem_omrshmem_open_ExitWithMsg("Successfully opened an existing memory.");
	}
	*handle = tmphandle;
	return rc;
}

void 
omrshmem_close(struct OMRPortLibrary *portLibrary, struct omrshmem_handle **handle)
{
	Trc_PRT_shmem_omrshmem_close_Entry1(*handle, (*handle)->shmid);
	portLibrary->shmem_detach(portLibrary, handle);
	omrmem_free_memory(portLibrary, *handle);
	
	*handle=NULL;
	Trc_PRT_shmem_omrshmem_close_Exit();
}

void*
omrshmem_attach(struct OMRPortLibrary *portLibrary, struct omrshmem_handle *handle, uint32_t categoryCode)
{
	void* region;
	int32_t shmatPerm = 0;
	OMRMemCategory * category = omrmem_get_category(portLibrary, categoryCode);
	Trc_PRT_shmem_omrshmem_attach_Entry1(handle, handle ? handle->shmid : -1);

	if (NULL == handle) {
		Trc_PRT_shmem_omrshmem_attach_Exit1();
		return NULL;
	}

	Trc_PRT_shmem_omrshmem_attach_Debug1(handle->shmid);

	if(NULL != handle->regionStart) {
		Trc_PRT_shmem_omrshmem_attach_Exit(handle->regionStart);
		return handle->regionStart;
	}
	if (handle->perm == OMRSH_SHMEM_PERM_READ) {
		/* shmat perms should be 0 for read/write and SHM_RDONLY for read-only */
		shmatPerm = SHM_RDONLY;
	}

#if defined(OMRZOS390)
	if (((handle->flags & OMRSHMEM_STORAGE_KEY_TESTING) != 0) &&
		(handle->currentStorageProtectKey != handle->controlStorageProtectKey) &&
		(!((8 == handle->currentStorageProtectKey) && (9 == handle->controlStorageProtectKey)))
	) {
		omrerror_set_last_error(portLibrary, EACCES, OMRPORT_ERROR_SYSV_IPC_SHMAT_ERROR + OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES);
		region = (void*)-1;
	} else {
#endif
		region = omr_shmatWrapper(portLibrary, handle->shmid, 0, shmatPerm);
#if defined(OMRZOS390)
	}
	if ((void*)-1 == region) {
		if ((handle->flags & OMRSHMEM_PRINT_STORAGE_KEY_WARNING) != 0) {
			if (handle->currentStorageProtectKey != handle->controlStorageProtectKey) {
				int32_t myerrno = omrerror_last_error_number(portLibrary);
				if ((OMRPORT_ERROR_SYSV_IPC_SHMAT_ERROR + OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) == myerrno) {
					omrnls_printf(OMRNLS_WARNING, OMRNLS_PORT_ZOS_SHMEM_STORAGE_KEY_MISMATCH, handle->controlStorageProtectKey, handle->currentStorageProtectKey);
				}
			}
		}
	}
#endif

	/* Do not use shmctl to release the shared memory here, just in case of ^C or crash */
	if((void *)-1 == region) {
		int32_t myerrno = omrerror_last_error_number(portLibrary);
		Trc_PRT_shmem_omrshmem_attach_Exit2(myerrno);
		return NULL;
	} else {
		handle->regionStart = region;
		omrmem_categories_increment_counters(category, handle->size);
		Trc_PRT_shmem_omrshmem_attach_Exit(region);
		return region;
	} 
}

intptr_t 
omrshmem_destroy (struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle)
{
	intptr_t rc;
	intptr_t lockFile;
	intptr_t fd;
	BOOLEAN isReadOnlyFD;
	omrshmem_controlBaseFileFormat controlinfo;

	Trc_PRT_shmem_omrshmem_destroy_Entry1(*handle, (*handle) ? (*handle)->shmid : -1);

	if (NULL == *handle) {
		Trc_PRT_shmem_omrshmem_destroy_Exit();
		return 0;
	}

	lockFile = omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFileName, 0);
	if (lockFile == OMRSH_FILE_DOES_NOT_EXIST) {
		Trc_PRT_shmem_omrshmem_destroy_Msg("Error: control file not found");
		if (omr_shmctlWrapper(portLibrary, TRUE, (*handle)->shmid, IPC_RMID, 0) == -1) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if ((OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL == lastError) || (OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM == lastError)) {
				Trc_PRT_shmem_omrshmem_destroy_Msg("SysV obj is already deleted");
				rc = 0;
				goto omrshmem_destroy_exitWithSuccess;
			} else {
				Trc_PRT_shmem_omrshmem_destroy_Msg("Error: could not delete SysV obj");
				rc = -1;
				goto omrshmem_destroy_exitWithError;
			}
		} else {
			Trc_PRT_shmem_omrshmem_destroy_Msg("Deleted SysV obj");
			rc = 0;
			goto omrshmem_destroy_exitWithSuccess;
		}
	} else if (lockFile != OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_destroy_Msg("Error: could not open and lock control file.");
		goto omrshmem_destroy_exitWithError;
	}

	/*Try to read data from the file*/
	rc = omrfile_read(portLibrary, fd, &controlinfo, sizeof(omrshmem_controlBaseFileFormat));
	if (rc != sizeof(omrshmem_controlBaseFileFormat)) {
		Trc_PRT_shmem_omrshmem_destroy_Msg("Error: can not read control file");
		goto omrshmem_destroy_exitWithErrorAndUnlock;
	}

	/* check that the modlevel and version is okay */
	if (OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) != OMRSH_MOD_MAJOR_LEVEL) {
		Trc_PRT_shmem_omrshmem_destroy_BadMajorModlevel(controlinfo.modlevel, OMRSH_MODLEVEL);
		goto omrshmem_destroy_exitWithErrorAndUnlock;
	}

	if (controlinfo.shmid != (*handle)->shmid) {
		Trc_PRT_shmem_omrshmem_destroy_Msg("Error: mem id does not match contents of the control file");
		goto omrshmem_destroy_exitWithErrorAndUnlock;
	}

	portLibrary->shmem_detach(portLibrary, handle);

	if (omr_shmctlWrapper(portLibrary, TRUE, (*handle)->shmid, IPC_RMID, NULL) == -1) {
		Trc_PRT_shmem_omrshmem_destroy_Msg("Error: shmctl(IPC_RMID) returned -1 ");
		goto omrshmem_destroy_exitWithErrorAndUnlock;
	}

	/* PR 74306: Allow unlinking of readonly control file if it was created by an older JVM without this fix
	 * which is identified by different modelevel.
	 */
	if ((FALSE == isReadOnlyFD)
		|| ((OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo.modlevel) == OMRSH_MOD_MAJOR_LEVEL_ALLOW_READONLY_UNLINK)
		&& (OMRSH_SHM_GET_MOD_MINOR_LEVEL(controlinfo.modlevel) <= OMRSH_MOD_MINOR_LEVEL_ALLOW_READONLY_UNLINK))
	) {
		rc = omrfile_unlink(portLibrary, (*handle)->baseFileName);
		Trc_PRT_shmem_omrshmem_destroy_Debug1((*handle)->baseFileName, rc, omrerror_last_error_number(portLibrary));
	}
	
	portLibrary->shmem_close(portLibrary, handle);

	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_destroy_Msg("Error: failed to unlock control file.");
		goto omrshmem_destroy_exitWithError;
	}

omrshmem_destroy_exitWithSuccess:
	Trc_PRT_shmem_omrshmem_destroy_Exit();
	return 0;

omrshmem_destroy_exitWithErrorAndUnlock:
	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_destroy_Msg("Error: failed to unlock control file");
	}
omrshmem_destroy_exitWithError:
	Trc_PRT_shmem_omrshmem_destroy_Exit1();
	return -1;

}

intptr_t 
omrshmem_destroyDeprecated (struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle, uintptr_t cacheFileType)
{
	intptr_t retval = OMRSH_SUCCESS;
	intptr_t fd;
	BOOLEAN isReadOnlyFD = FALSE;
	BOOLEAN fileIsLocked = FALSE;
	
	Trc_PRT_shmem_omrshmem_destroyDeprecated_Entry(*handle, (*handle)->shmid);
	
	if (cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		Trc_PRT_shmem_omrshmem_destroyDeprecated_Message("Info: cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE.");

		if (omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, (*handle)->baseFileName, 0) != OMRSH_SUCCESS) {
			Trc_PRT_shmem_omrshmem_destroyDeprecated_Message("Error: could not lock shared memory control file.");
			retval = OMRSH_FAILED;
			goto error;
		} else {
			fileIsLocked = TRUE;
		}
		portLibrary->shmem_detach(portLibrary, handle);

		if (omr_shmctlWrapper(portLibrary, TRUE, (*handle)->shmid, IPC_RMID, NULL) == -1) {
			Trc_PRT_shmem_omrshmem_destroyDeprecated_Message("Error: failed to remove SysV object.");
			retval = OMRSH_FAILED;
			goto error;
		}

		if (0 == omrfile_unlink(portLibrary, (*handle)->baseFileName)) {
			Trc_PRT_shmem_omrshmem_destroyDeprecated_Message("Unlinked control file");
		} else {
			Trc_PRT_shmem_omrshmem_destroyDeprecated_Message("Failed to unlink control file");
		}
		portLibrary->shmem_close(portLibrary, handle);
	error:
		if (fileIsLocked == TRUE) {
			if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
				Trc_PRT_shmem_omrshmem_destroyDeprecated_Message("Error: could not unlock shared memory control file.");
				retval = OMRSH_FAILED;
			}
		}
	} else if (cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE) {
		Trc_PRT_shmem_omrshmem_destroyDeprecated_Message("Info: cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE.");
		retval = omrshmem_destroy(portLibrary, cacheDirName, groupPerm, handle);
	} else {
		Trc_PRT_omrshmem_destroyDeprecated_BadCacheType(cacheFileType);
		retval = OMRSH_FAILED;
	}

	if (retval == OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_destroyDeprecated_ExitWithMessage("Exit successfully");
	} else {
		Trc_PRT_shmem_omrshmem_destroyDeprecated_ExitWithMessage("Exit with failure");
	}
	return retval;
}

intptr_t
omrshmem_detach(struct OMRPortLibrary* portLibrary, struct omrshmem_handle **handle) 
{
	Trc_PRT_shmem_omrshmem_detach_Entry1(*handle, (*handle)->shmid);
	if((*handle)->regionStart == NULL) {
		Trc_PRT_shmem_omrshmem_detach_Exit();
		return OMRSH_SUCCESS;
	}

	if (-1 == omr_shmdtWrapper(portLibrary, ((*handle)->regionStart))) {
		Trc_PRT_shmem_omrshmem_detach_Exit1();
		return OMRSH_FAILED;
	}

	omrmem_categories_decrement_counters((*handle)->category, (*handle)->size);

	(*handle)->regionStart = NULL;
	Trc_PRT_shmem_omrshmem_detach_Exit();
	return OMRSH_SUCCESS;
}

void
omrshmem_findclose(struct OMRPortLibrary *portLibrary, uintptr_t findhandle)
{
	Trc_PRT_shmem_omrshmem_findclose_Entry(findhandle);
	omrfile_findclose(portLibrary, findhandle);
	Trc_PRT_shmem_omrshmem_findclose_Exit();
}

uintptr_t
omrshmem_findfirst(struct OMRPortLibrary *portLibrary, char *cacheDirName, char *resultbuf)
{
	uintptr_t findHandle;
	char file[EsMaxPath];
	
	Trc_PRT_shmem_omrshmem_findfirst_Entry();
	
	if (cacheDirName == NULL) {
		Trc_PRT_shmem_omrshmem_findfirst_Exit3();
		return -1;
	}
	
	findHandle = omrfile_findfirst(portLibrary, cacheDirName, file);
	
	if(findHandle == -1) {
		Trc_PRT_shmem_omrshmem_findfirst_Exit1();
		return -1;
	}
	
	while (!omrshmem_isControlFileName(portLibrary, file)) {
		if (-1 == omrfile_findnext(portLibrary, findHandle, file)) {
			omrfile_findclose(portLibrary, findHandle);
			Trc_PRT_shmem_omrshmem_findfirst_Exit2();
			return -1;
		}		
	}
	
	strcpy(resultbuf, file);
	Trc_PRT_shmem_omrshmem_findfirst_file(resultbuf);
	Trc_PRT_shmem_omrshmem_findfirst_Exit();
	return findHandle;
}

int32_t
omrshmem_findnext(struct OMRPortLibrary *portLibrary, uintptr_t findHandle, char *resultbuf)
{
	char file[EsMaxPath];

	Trc_PRT_shmem_omrshmem_findnext_Entry(findHandle);

	if (-1 == omrfile_findnext(portLibrary, findHandle, file)) {
		Trc_PRT_shmem_omrshmem_findnext_Exit1();
		return -1;
	}
	
	while (!omrshmem_isControlFileName(portLibrary, file)) {
		if (-1 == omrfile_findnext(portLibrary, findHandle, file)) {
			Trc_PRT_shmem_omrshmem_findnext_Exit2();
			return -1;
		}		
	}

	strcpy(resultbuf, file);
	Trc_PRT_shmem_omrshmem_findnext_file(resultbuf);
	Trc_PRT_shmem_omrshmem_findnext_Exit();
	return 0;
}

uintptr_t
omrshmem_stat(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct OMRPortShmemStatistic* statbuf)
{
	int32_t rc;
	intptr_t fd;
	char controlFile[OMRSH_MAXPATH];
	struct omrshmem_controlFileFormat controlinfo;
	BOOLEAN isReadOnlyFD;
	int shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
	int shmid;
	char * exitMsg = "Exiting";
	intptr_t lockFile;

	shmflags &= ~IPC_CREAT;

	Trc_PRT_shmem_omrshmem_stat_Entry(name);
	if (cacheDirName == NULL) {
		Trc_PRT_shmem_omrshmem_stat_ExitNullCacheDirName();
		return -1;
	}
	if (statbuf == NULL) {
		Trc_PRT_shmem_omrshmem_stat_ExitNullStat();
		return -1;
	} else {
		omrshmem_initShmemStatsBuffer(portLibrary, statbuf);
	}

	omrshmem_getControlFilePath(portLibrary, cacheDirName, controlFile, OMRSH_MAXPATH, name);

	/*Open control file with write lock*/
	lockFile = omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, controlFile, 0);
	if (lockFile == OMRSH_FILE_DOES_NOT_EXIST) {
		Trc_PRT_shmem_omrshmem_stat_Exit1(controlFile);
		return -1;
	} else if (lockFile != OMRSH_SUCCESS) {
		exitMsg = "Error: can not open & lock control file";
		goto omrshmem_stat_exitWithError;
	}

	/*Try to read data from the file*/
	if (OMRSH_SUCCESS != omrshmem_readControlFile(portLibrary, fd, &controlinfo)) {
		exitMsg = "Error: can not read control file";
		goto omrshmem_stat_exitWithErrorAndUnlock;
	}

	shmid = omr_shmgetWrapper(portLibrary, controlinfo.common.ftok_key, controlinfo.size, shmflags);

#if defined (OMRZOS390)
	if(-1 == shmid) {
		/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
		/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
		shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
		shmflags &= ~IPC_CREAT;
		shmid = omr_shmgetWrapper(portLibrary, controlinfo.common.ftok_key, controlinfo.size, shmflags);
	}
#endif

	if (controlinfo.common.shmid != shmid) {
		exitMsg = "Error: mem id does not match contents of the control file";
		goto omrshmem_stat_exitWithErrorAndUnlock;
	}

	if (omrshmem_checkGid(portLibrary, controlinfo.common.shmid, controlinfo.gid) != 1) {
		/*If no match then this is someone elses sysv obj. The one our filed referred to was destroy and recreated by someone/thing else*/
		exitMsg = "Error: omrshmem_checkGid failed";
		goto omrshmem_stat_exitWithErrorAndUnlock;
	}

	if (omrshmem_checkUid(portLibrary, controlinfo.common.shmid, controlinfo.uid) != 1) {
		/*If no match then this is someone elses sysv obj. The one our filed referred to was destroy and recreated by someone/thing else*/
		exitMsg = "Error: omrshmem_checkUid failed";
		goto omrshmem_stat_exitWithErrorAndUnlock;
	}

	rc = omrshmem_checkSize(portLibrary, controlinfo.common.shmid, controlinfo.size);
	if (1 != rc) {
#if defined(OMRZOS390)
		if (-2 != rc)
#endif /* defined(OMRZOS390) */
		{
			/*If no match then this is someone elses sysv obj. The one our filed referred to was destroy and recreated by someone/thing else*/
			exitMsg = "Error: omrshmem_checkSize failed";
			goto omrshmem_stat_exitWithErrorAndUnlock;
		}
#if defined(OMRZOS390)
		else {
			/* JAZZ103 PR 79909 + PR 88930: shmctl(..., IPC_STAT, ...) returned 0 as the shared memory size.
			 * This can happen if shared memory segment is created by a 64-bit JVM and the caller is 31-bit JVM.
			 * Do not treat it as failure. Get the shared memory stats (which includes the size = 0) and let the caller handle
			 * this case as appropriate.
			 */
		}
#endif /* defined(OMRZOS390) */
	}

	statbuf->shmid = controlinfo.common.shmid;

	rc = omrshmem_getShmStats(portLibrary, statbuf->shmid, statbuf);
	if (OMRPORT_INFO_SHMEM_STAT_PASSED != rc) {
		exitMsg = "Error: omrshmem_getShmStats failed";
		goto omrshmem_stat_exitWithErrorAndUnlock;
	}

	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		exitMsg = "Error: can not close & unlock control file (we were successful other than this)";
		goto omrshmem_stat_exitWithError;
	}

	Trc_PRT_shmem_omrshmem_stat_Exit();
	return 0;

omrshmem_stat_exitWithErrorAndUnlock:
	if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_stat_Msg(exitMsg);
		exitMsg = "Error: can not close & unlock control file";
	}
omrshmem_stat_exitWithError:
	Trc_PRT_shmem_omrshmem_stat_ExitWithMsg(exitMsg);
	return -1;
}

uintptr_t
omrshmem_statDeprecated (struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct OMRPortShmemStatistic* statbuf, uintptr_t cacheFileType)
{
	intptr_t rc;
	uintptr_t retval = (uintptr_t)OMRSH_SUCCESS;
	char controlFile[OMRSH_MAXPATH];
	intptr_t fd = -1;
	BOOLEAN fileIsLocked = FALSE;
	BOOLEAN isReadOnlyFD = FALSE;

	Trc_PRT_shmem_omrshmem_statDeprecated_Entry();

	if (cacheDirName == NULL) {
		Trc_PRT_shmem_omrshmem_statDeprecated_ExitNullCacheDirName();
		return -1;
	}
	omrshmem_initShmemStatsBuffer(portLibrary, statbuf);

	omrshmem_getControlFilePath(portLibrary, cacheDirName, controlFile, OMRSH_MAXPATH, name);
	if (omr_ControlFileOpenWithWriteLock(portLibrary, &fd, &isReadOnlyFD, FALSE, controlFile, 0) != OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_statDeprecated_Message("Error: could not lock shared memory control file.");
		retval = OMRSH_FAILED;
		goto error;
	} else {
		fileIsLocked = TRUE;
	}

	if (cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE) {
		int shmid = -1;
		int32_t shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
		int projid = 0xde;
		key_t fkey;

		Trc_PRT_shmem_omrshmem_statDeprecated_Message("Info: cacheFileType == OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE.");

		fkey = omr_ftokWrapper(portLibrary, controlFile, projid);
		if (fkey == -1) {
			Trc_PRT_shmem_omrshmem_statDeprecated_Message("Error: omr_ftokWrapper failed.");
			retval = OMRSH_FAILED;
			goto error;
		}
		shmflags &= ~IPC_CREAT;

		 /*size of zero means use current size of existing object*/
		shmid = omr_shmgetWrapper(portLibrary, fkey, 0, shmflags);

#if defined (OMRZOS390)
		if(shmid == -1) {
			/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
			/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
			shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
			shmflags &= ~IPC_CREAT;
			shmid = omr_shmgetWrapper(portLibrary, fkey, 0, shmflags);
		}
#endif
		if (shmid == -1) {
			Trc_PRT_shmem_omrshmem_statDeprecated_Message("Error: omr_shmgetWrapper failed.");
			retval = OMRSH_FAILED;
			goto error;
		}
		statbuf->shmid = shmid;
		
	} else if (cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE) {
		omrshmem_controlBaseFileFormat info;
		
		Trc_PRT_shmem_omrshmem_statDeprecated_Message("Info: cacheFileType == OMRSH_SYSV_OLDER_CONTROL_FILE.");

		if (omrshmem_readOlderNonZeroByteControlFile(portLibrary, fd, &info) != OMRSH_SUCCESS) {
			Trc_PRT_shmem_omrshmem_statDeprecated_Message("Error: can not read control file.");
			retval = OMRSH_FAILED;
			goto error;
		}
		statbuf->shmid = info.shmid;
		
	} else {
		Trc_PRT_shmem_omrshmem_statDeprecated_BadCacheType(cacheFileType);
		retval = OMRSH_FAILED;
		goto error;			
	}

	rc = omrshmem_getShmStats(portLibrary, statbuf->shmid, statbuf);
	if (OMRPORT_INFO_SHMEM_STAT_PASSED != rc) {
		Trc_PRT_shmem_omrshmem_statDeprecated_Message("Error: omrshmem_getShmStats failed");
		retval = OMRSH_FAILED;
		goto error;
	}

error:
	if (fileIsLocked == TRUE) {
		if (omr_ControlFileCloseAndUnLock(portLibrary, fd) != OMRSH_SUCCESS) {
			Trc_PRT_shmem_omrshmem_statDeprecated_Message("Error: could not unlock shared memory control file.");
			retval = OMRSH_FAILED;
		}
	}
	if (retval == OMRSH_SUCCESS) {
		Trc_PRT_shmem_omrshmem_statDeprecated_Exit("Successful exit.");
	} else {
		Trc_PRT_shmem_omrshmem_statDeprecated_Exit("Exit with Error.");
	}
	return retval;
}

intptr_t
omrshmem_handle_stat(struct OMRPortLibrary *portLibrary, struct omrshmem_handle *handle, struct OMRPortShmemStatistic *statbuf)
{
	intptr_t rc = OMRPORT_ERROR_SHMEM_STAT_FAILED;

	Trc_PRT_shmem_omrshmem_handle_stat_Entry1(handle, handle ? handle->shmid : -1);

	omr_clearPortableError(portLibrary);

	if (NULL == handle) {
		Trc_PRT_shmem_omrshmem_handle_stat_ErrorNullHandle();
		rc = OMRPORT_ERROR_SHMEM_HANDLE_INVALID;
		goto _end;
	}
	if (NULL == statbuf) {
		Trc_PRT_shmem_omrshmem_handle_stat_ErrorNullBuffer();
		rc = OMRPORT_ERROR_SHMEM_STAT_BUFFER_INVALID;
		goto _end;
	} else {
		omrshmem_initShmemStatsBuffer(portLibrary, statbuf);
	}

	rc = omrshmem_getShmStats(portLibrary, handle->shmid, statbuf);
	if (OMRPORT_INFO_SHMEM_STAT_PASSED != rc) {
		Trc_PRT_shmem_omrshmem_handle_stat_ErrorGetShmStatsFailed(handle->shmid);
	}

_end:
	Trc_PRT_shmem_omrshmem_handle_stat_Exit(rc);
	return rc;
}

void
omrshmem_shutdown(struct OMRPortLibrary *portLibrary)
{
}

int32_t
omrshmem_startup(struct OMRPortLibrary *portLibrary)
{
#if defined(OMRZOS39064)
#if (__EDC_TARGET >= __EDC_LE410A)
	__mopl_t mymopl;
	int  mos_rv;
	void * mymoptr;
#endif
#endif

	if (NULL != portLibrary->portGlobals) {
#if defined(AIXPPC)
		PPG_pageProtectionPossible = PAGE_PROTECTION_NOTCHECKED;
#endif
#if defined(OMRZOS39064)
#if (__EDC_TARGET >= __EDC_LE410A)
		memset(&mymopl, 0, sizeof(__mopl_t));

		/* Set a shared memory dump priority for subsequent shmget()
		 *  setting dump priority to __MO_DUMP_PRIORITY_STACK ensures that shared memory regions
		 *  gets included in the first tdump (On 64 bit, tdumps gets divided into chunks < 2G, this
		 *  makes sure shared memory gets into the first chunk).
		 */
		mymopl.__mopldumppriority = __MO_DUMP_PRIORITY_STACK;
		mos_rv = __moservices(__MO_SHMDUMPPRIORITY, sizeof(mymopl),
		                      &mymopl , &mymoptr);
		if (mos_rv != 0) {
			return (int32_t)OMRPORT_ERROR_STARTUP_SHMEM_MOSERVICE;
		}
#endif
#endif
		return 0;
	}
	return (int32_t)OMRPORT_ERROR_STARTUP_SHMEM;
}

/**
 * @internal
 * Create a sharedmemory handle for omrshmem_open() caller ...
 *
 * @param[in] portLibrary
 * @param[in] shmid id of SysV object
 * @param[in] controlFile file being used for control file
 * @param[in] category Memory category structure for allocation
 * @param[in] size size of shared memory region
 * @param[out] handle the handle obj
 *
 * @return	void
 */
static void
omrshmem_createshmHandle(OMRPortLibrary *portLibrary, int32_t shmid, const char *controlFile, OMRMemCategory * category, uintptr_t size, omrshmem_handle * handle, uint32_t perm)
{
	intptr_t cfstrLength = strlen(controlFile);

	Trc_PRT_shmem_omrshmem_createshmHandle_Entry(controlFile, shmid);
	    
	handle->shmid = shmid;
	omrstr_printf(portLibrary, handle->baseFileName, cfstrLength+1, "%s", controlFile);
	handle->regionStart = NULL;
	handle->size = size;
	handle->category = category;
	handle->perm = (int32_t)perm;

	Trc_PRT_shmem_omrshmem_createshmHandle_Exit(OMRSH_SUCCESS);
}

/* Note that, although it's the responsibility of the caller to generate the
 * version prefix and generation postfix, the function this calls strictly checks
 * the format used by shared classes. It would be much better to have a more flexible
 * API that takes a call-back function, but we decided not to change the API this release */ 
static uintptr_t
omrshmem_isControlFileName(struct OMRPortLibrary* portLibrary, char* nameToTest)
{
        /* this needs to be overloaded with a custom predicate function to decide if the name of the control is that of a cache. See omrshmem_isCacheFileName in j9shmem.c of OpenJ9. */
        return 0; 
}

/**
 * @internal
 * Get the full path for a shared memory area
 *
 * @param[in] portLibrary The port library
 * @param[in] cacheDirName path to directory where control files are present
 * @param[out] buffer where the path will be stored
 * @param[in] size size of the buffer
 * @param[in] name of the shared memory area
 *
 */
static void 
omrshmem_getControlFilePath(struct OMRPortLibrary* portLibrary, const char* cacheDirName, char* buffer, uintptr_t size, const char* name)
{
	omrstr_printf(portLibrary, buffer, size, "%s%s", cacheDirName, name);

#if defined(OMRSHMEM_DEBUG)
	omrtty_printf("getControlFileName returns: %s\n",buffer);
#endif
}

intptr_t
omrshmem_getDir(struct OMRPortLibrary* portLibrary, const char* ctrlDirName, uint32_t flags, char* buffer, uintptr_t bufLength)
{

	const char* rootDir = NULL;
	char homeDirBuf[OMRSH_MAXPATH];
	const char* homeDir = NULL;
	BOOLEAN appendBaseDir = OMR_ARE_ALL_BITS_SET(flags, OMRSHMEM_GETDIR_APPEND_BASEDIR);

	Trc_PRT_omrshmem_getDir_Entry();

	memset(homeDirBuf, 0, sizeof(homeDirBuf));

	if (ctrlDirName != NULL) {
		rootDir = ctrlDirName;
	} else {
		if (OMR_ARE_ALL_BITS_SET(flags, OMRSHMEM_GETDIR_USE_USERHOME)) {
			Assert_PRT_true(TRUE == appendBaseDir);
			uintptr_t baseDirLen = strlen(OMRSH_BASEDIR);
			/*
			 *  User could define/change their notion of the home directory by setting environment variable "HOME".
			 *  So check "HOME" first, if failed, then check getpwuid(getuid())->pw_dir.
			 */
			if (0 == omrsysinfo_get_env(portLibrary, "HOME", homeDirBuf, OMRSH_MAXPATH)) {
				uintptr_t dirLen = strlen((const char*)homeDirBuf);
				if (0 < dirLen
					&& ((dirLen + baseDirLen) < bufLength))
				{
					homeDir = homeDirBuf;
				} else {
					Trc_PRT_omrshmem_getDir_tryHomeDirFailed_homeDirTooLong(dirLen, bufLength - baseDirLen);
				}
			} else {
				Trc_PRT_omrshmem_getDir_tryHomeDirFailed_getEnvHomeFailed();
			}
			if (NULL == homeDir) {
				struct passwd *pwent = getpwuid(getuid());
				if (NULL != pwent) {
					uintptr_t dirLen = strlen((const char*)pwent->pw_dir);
					if (0 < dirLen
						&& ((dirLen + baseDirLen) < bufLength))
					{
						homeDir = pwent->pw_dir;
					} else {
						Trc_PRT_omrshmem_getDir_tryHomeDirFailed_pw_dirDirTooLong(dirLen, bufLength - baseDirLen);
					}
				} else {
					Trc_PRT_omrshmem_getDir_tryHomeDirFailed_getpwuidFailed();
				}
			}
			if (NULL != homeDir) {
				struct J9FileStat statBuf = {0};
				if (0 == omrfile_stat(portLibrary, homeDir, 0, &statBuf)) {
					if (!statBuf.isRemote) {
						rootDir = homeDir;
					} else {
						Trc_PRT_omrshmem_getDir_tryHomeDirFailed_homeOnNFS(homeDir);
					}
				} else {
					Trc_PRT_omrshmem_getDir_tryHomeDirFailed_cannotStat(homeDir);
				}
			}
		} else {
			rootDir = OMRSH_DEFAULT_CTRL_ROOT;
		}
	}
	if (NULL == rootDir) {
		Assert_PRT_true(OMR_ARE_ALL_BITS_SET(flags, OMRSHMEM_GETDIR_USE_USERHOME));
		return -1;
	} else {
		if (appendBaseDir) {
			if (omrstr_printf(portLibrary, buffer, bufLength, "%s/%s", rootDir, OMRSH_BASEDIR) == bufLength - 1) {
				Trc_PRT_omrshmem_getDir_ExitFailedOverflow();
				return -1;
			}
		} else {
			/* Avoid appending two slashes; this leads to problems in matching full file names. */
			if (omrstr_printf(portLibrary, buffer,
								bufLength,
								('/' == rootDir[strlen(rootDir)-1]) ? "%s" : "%s/",
										rootDir) == bufLength - 1) {
				Trc_PRT_omrshmem_getDir_ExitFailedOverflow();
				return -1;
			}
		}
	}

	Trc_PRT_omrshmem_getDir_Exit(buffer);
	return 0;
}

intptr_t
omrshmem_createDir(struct OMRPortLibrary* portLibrary, char* cacheDirName, uintptr_t cacheDirPerm, BOOLEAN cleanMemorySegments)
{

	intptr_t rc, rc2;
	char pathCopy[OMRSH_MAXPATH];

	Trc_PRT_omrshmem_createDir_Entry();

	rc = omrfile_attr(portLibrary, cacheDirName);
	switch(rc) {
	case EsIsFile:
		Trc_PRT_omrshmem_createDir_ExitFailedFile();
		break;
	case EsIsDir:
#if !defined(OMRZOS390)
		if (OMRSH_DIRPERM_ABSENT == cacheDirPerm) {
			/* cacheDirPerm option is not specified. Change permission to OMRSH_DIRPERM. */
			rc2 = omr_changeDirectoryPermission(portLibrary, cacheDirName, OMRSH_DIRPERM);
			if (OMRSH_FILE_DOES_NOT_EXIST == rc2) {
				Trc_PRT_shared_omr_createDir_Exit3(errno);
				return -1;
			} else {
				Trc_PRT_shared_omr_createDir_Exit4(errno);
				return 0;
			}
		} else
#endif
		{
			Trc_PRT_shared_omr_createDir_Exit7();
			return 0;
		}
	default: /* Directory is not there */
		strncpy(pathCopy, cacheDirName, OMRSH_MAXPATH);
		pathCopy[OMRSH_MAXPATH - 1] = '\0';
		/* Note that the createDirectory call may change pathCopy */
		rc = omr_createDirectory(portLibrary, (char*)pathCopy, cacheDirPerm);
		if (OMRSH_FAILED != rc) {
			if (OMRSH_DIRPERM_ABSENT == cacheDirPerm) {
				rc2 = omr_changeDirectoryPermission(portLibrary, cacheDirName, OMRSH_DIRPERM);
			} else {
				rc2 = omr_changeDirectoryPermission(portLibrary, cacheDirName, cacheDirPerm);
			}
			if (OMRSH_FILE_DOES_NOT_EXIST == rc2) {
				Trc_PRT_shared_omr_createDir_Exit5(errno);
				return -1;
			} else {
				Trc_PRT_shared_omr_createDir_Exit6(errno);
				return 0;
			}
		}
		Trc_PRT_omrshmem_createDir_ExitFailed();
		return -1;
	}

	Trc_PRT_omrshmem_createDir_ExitFailed();
	return -1;
}

intptr_t
omrshmem_getFilepath(struct OMRPortLibrary* portLibrary, char* cacheDirName, char* buffer, uintptr_t length, const char* cachename)
{
	if (cacheDirName == NULL) {
		Trc_PRT_shmem_omrshmem_getFilepath_ExitNullCacheDirName();
		return -1;
	}
	omrshmem_getControlFilePath(portLibrary, cacheDirName, buffer, length, cachename);
	return 0;
}

intptr_t
omrshmem_protect(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address, uintptr_t length, uintptr_t flags)
{
	intptr_t rc = OMRPORT_PAGE_PROTECT_NOT_SUPPORTED;

#if defined(OMRZTPF)
	return rc;
#endif /* defined(OMRZTPF) */

#if defined(OMRZOS390) && !defined(OMRZOS39064)
	Trc_PRT_shmem_omrshmem_protect(address, length);
	rc = protect_memory(portLibrary, address, length, flags);

#elif defined(AIXPPC) /* defined(OMRZOS390) && !defined(OMRZOS39064) */


	if (PAGE_PROTECTION_NOTCHECKED == PPG_pageProtectionPossible) {
		if (NULL == cacheDirName) {
			Trc_PRT_shmem_omrshmem_protect_ExitNullCacheDirName();
			return -1;
		}
		portLibrary->shmem_get_region_granularity(portLibrary, cacheDirName, groupPerm, address);
	}
	if (PAGE_PROTECTION_NOTAVAILABLE == PPG_pageProtectionPossible) {
		return rc;
	}
	Trc_PRT_shmem_omrshmem_protect(address, length);
	rc = omrmmap_protect(portLibrary, address, length, flags);

#elif defined(LINUX) /* defined(AIXPPC) */


	Trc_PRT_shmem_omrshmem_protect(address, length);
	rc = omrmmap_protect(portLibrary, address, length, flags);
#endif /* defined(OMRZOS390) && !defined(OMRZOS39064) */

	return rc;
}
 
uintptr_t 
omrshmem_get_region_granularity(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address)
{
	uintptr_t rc = 0;
	
#if defined(OMRZOS390) && !defined(OMRZOS39064)
	rc = protect_region_granularity(portLibrary, address);
	
#elif defined(AIXPPC) /* defined(OMRZOS390) && !defined(OMRZOS39064) */

	
	if (PAGE_PROTECTION_NOTCHECKED == PPG_pageProtectionPossible) {
		intptr_t envrc = 0;
		char buffer[OMRSH_MAXPATH];
		envrc = omrsysinfo_get_env(portLibrary, "MPROTECT_SHM", buffer, OMRSH_MAXPATH);
	
	  	if ((0 != envrc) || (0 != strcmp(buffer,"ON"))) {
			Trc_PRT_shmem_omrshmem_get_region_granularity_MPROTECT_SHM_Not_Set();
			PPG_pageProtectionPossible = PAGE_PROTECTION_NOTAVAILABLE;
		}
    }

	if (PAGE_PROTECTION_NOTAVAILABLE == PPG_pageProtectionPossible) {
		return rc;
	}
	rc = omrmmap_get_region_granularity(portLibrary, address);

#elif defined(LINUX) /* defined(AIXPPC) */

	
	rc = omrmmap_get_region_granularity(portLibrary, address);
#endif /* defined(OMRZOS390) && !defined(OMRZOS39064) */

	return rc;
}

int32_t
omrshmem_getid (struct OMRPortLibrary *portLibrary, struct omrshmem_handle* handle)
{
	if (NULL != handle) {
		return handle->shmid;
	} else {
		return 0;
	}
}

static intptr_t
omrshmem_readControlFile(OMRPortLibrary *portLibrary, intptr_t fd, omrshmem_controlFileFormat * info)
{

	intptr_t rc;
	/* Ensure the buffer is big enough to hold an optional padding word */
	uint8_t buffer[sizeof(omrshmem_controlFileFormat) + sizeof(uint32_t)];
	BOOLEAN hasPadding = sizeof(omrshmem_controlBaseFileFormat) != ((uintptr_t)&info->size - (uintptr_t)info);

	rc = omrfile_read(portLibrary, fd, buffer, sizeof(buffer));
	if (rc < 0) {
		return OMRSH_ERROR;
	}

	if (rc == sizeof(omrshmem_controlFileFormat)) {
		/* Control file size is an exact match */
		memcpy(info, buffer, sizeof(omrshmem_controlFileFormat));
		return OMRSH_SUCCESS;
	}

	if (hasPadding) {
		if (rc == (sizeof(omrshmem_controlFileFormat) - sizeof(uint32_t))) {
			/* Control file does not have the padding, but this JVM uses padding */
			memcpy(info, buffer, sizeof(omrshmem_controlBaseFileFormat));
			memcpy(&info->size, buffer + sizeof(omrshmem_controlBaseFileFormat), sizeof(omrshmem_controlFileFormat) - ((uintptr_t)&info->size - (uintptr_t)info));
			return OMRSH_SUCCESS;
		}
	} else {
		if (rc == (sizeof(omrshmem_controlFileFormat) + sizeof(uint32_t))) {
			/* Control file has the padding, but this JVM doesn't use padding */
			memcpy(info, buffer, sizeof(omrshmem_controlBaseFileFormat));
			memcpy(&info->size, buffer + sizeof(omrshmem_controlBaseFileFormat) + sizeof(uint32_t), sizeof(omrshmem_controlFileFormat) - ((uintptr_t)&info->size - (uintptr_t)info));
			return OMRSH_SUCCESS;
		}
	}
	return OMRSH_FAILED;
}

static intptr_t 
omrshmem_readOlderNonZeroByteControlFile(OMRPortLibrary *portLibrary, intptr_t fd, omrshmem_controlBaseFileFormat * info)
{

	intptr_t rc;

	rc = omrfile_read(portLibrary, fd, info, sizeof(omrshmem_controlBaseFileFormat));

	if (rc < 0) {
		return OMRSH_FAILED;/*READ_ERROR;*/
	} else if (rc == 0) {
		return OMRSH_FAILED;/*OMRSH_NO_DATA;*/
	} else if (rc != sizeof(struct omrshmem_controlBaseFileFormat)) {
		return OMRSH_FAILED;/*OMRSH_BAD_DATA;*/
	}
	return OMRSH_SUCCESS;
}

/**
 * @internal
 * Open a SysV shared memory based on information in control file.
 *
 * @param[in] portLibrary
 * @param[in] fd of file to write info to ...
 * @param[in] baseFile file being used for control file
 * @param[in] perm permission for the region.
 * @param[in] controlinfo control file info (copy of data last written to control file)
 * @param[in] groupPerm group permission of the shared memory
 * @param[in] handle used on z/OS to check storage key
 * @param[out] canUnlink on failure it is set to TRUE if caller can unlink the control file, otherwise set to FALSE
 * @param[in] cacheFileType type of control file (regular or older or older-empty)
 *
 * @return OMRPORT_INFO_SHMEM_OPENED on success
 */
static intptr_t 
omrshmem_openSharedMemory (OMRPortLibrary *portLibrary, intptr_t fd, const char *baseFile, int32_t perm, struct omrshmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct omrshmem_handle *handle, BOOLEAN *canUnlink, uintptr_t cacheFileType)
{

	/*The control file contains data, and we have sucessfully reading our structure*/
	int shmid = -1;
	int shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
	intptr_t rc = -1;
	struct shmid_ds buf;
#if defined (OMRZOS390)
	IPCQPROC info;
#endif
	Trc_PRT_shmem_omrshmem_openSharedMemory_EnterWithMessage("Start");

	/* omrshmem_openSharedMemory can only open sysv memory with the same major level */
	if ((OMRSH_SYSV_REGULAR_CONTROL_FILE == cacheFileType) &&
		(OMRSH_GET_MOD_MAJOR_LEVEL(controlinfo->common.modlevel) != OMRSH_MOD_MAJOR_LEVEL)
	) {
		Trc_PRT_shmem_omrshmem_openSharedMemory_ExitWithMessage("Error: major modlevel mismatch.");
		/* Clear any stale portlibrary error code */
		omr_clearPortableError(portLibrary);
		rc = OMRPORT_ERROR_SHMEM_OPFAILED;
		goto failDontUnlink;
	}

#if defined (OMRZOS390)
	handle->controlStorageProtectKey = OMRSH_SHM_GET_MODLEVEL_STORAGEKEY(controlinfo->common.modlevel);
	Trc_PRT_shmem_omrshmem_openSharedMemory_storageKey(handle->controlStorageProtectKey, handle->currentStorageProtectKey);
	/* Switch to readonly mode if memory was created in storage protection key 9 */
	if ((9 == handle->controlStorageProtectKey) &&
	    (8 == handle->currentStorageProtectKey) &&
	    (perm != OMRSH_SHMEM_PERM_READ)
	) {
		/* Clear any stale portlibrary error code */
		omr_clearPortableError(portLibrary);
		rc = OMRPORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY;
		goto failDontUnlink;
	}
#endif

	shmflags &= ~IPC_CREAT;
	if (perm == OMRSH_SHMEM_PERM_READ) {
		shmflags &= ~(S_IWUSR | S_IWGRP);
	}

	/*size of zero means use current size of existing object*/
	shmid = omr_shmgetWrapper(portLibrary, controlinfo->common.ftok_key, 0, shmflags);

#if defined (OMRZOS390)
	if(-1 == shmid) {
		/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
		/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
		shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
		shmflags &= ~IPC_CREAT;
		if (perm == OMRSH_SHMEM_PERM_READ) {
			shmflags &= ~(S_IWUSR | S_IWGRP);
		}
		Trc_PRT_shmem_omrshmem_call_shmget_zos(controlinfo->common.ftok_key, controlinfo->size, shmflags);
		/*size of zero means use current size of existing object*/
		shmid = omr_shmgetWrapper(portLibrary, controlinfo->common.ftok_key, 0, shmflags);
	}
#endif

	if (-1 == shmid) {
		int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOENT) || (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
			Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The shared SysV obj was deleted, but the control file still exists.");
			rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND;
			goto fail;
		} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
			Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("Info: EACCES occurred.");
			/*Continue looking for opportunity to re-create*/
		} else {
			/*Any other errno will not result is an error*/
			Trc_PRT_shmem_omrshmem_openSharedMemory_MsgWithError("Error: can not open shared memory (shmget failed), portable errorCode = ", lastError);
			rc = OMRPORT_ERROR_SHMEM_OPFAILED;
			goto failDontUnlink;
		}
	}

	do {

		if (shmid != -1 && shmid != controlinfo->common.shmid) {
			Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The SysV id does not match our control file.");
			/* Clear any stale portlibrary error code */
			omr_clearPortableError(portLibrary);
			rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHMID_MISMATCH;
			goto fail;
		}

		/*If our sysv obj id is gone, or our <key, id> pair is invalid, then we can also recover*/
		rc = omr_shmctlWrapper(portLibrary, TRUE, controlinfo->common.shmid, IPC_STAT, &buf);
		if (-1 == rc) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			rc = OMRPORT_ERROR_SHMEM_OPFAILED;
			if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) || (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM)) {
				Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The SysV obj may have been removed just before, or during, our sXmctl call.");
				rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND;
				goto fail;
			} else {
				if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (shmid != -1)) {
					Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The SysV obj may have been modified since the call to sXmget.");
					goto fail;
				}/*Any other error and we will terminate*/

				/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
				Trc_PRT_shmem_omrshmem_openSharedMemory_MsgWithError("Error: shmctl failed. Can not open shared shared memory, portable errorCode = ", lastError);
				goto failDontUnlink;
			}
		} else {
#if (defined(__GNUC__) || defined(AIXPPC)) && !defined(J9ZOS390)
#if defined(OSX)
			/*Use ._key for OSX*/
			if (buf.shm_perm._key != controlinfo->common.ftok_key)
#elif defined(AIXPPC)
			/*Use .key for AIXPPC*/
			if (buf.shm_perm.key != controlinfo->common.ftok_key)
#elif defined(__GNUC__)
			/*Use .__key for __GNUC__*/
			if (buf.shm_perm.__key != controlinfo->common.ftok_key)
#endif
			{
				Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The <key,id> pair in our control file is no longer valid.");
				/* Clear any stale portlibrary error code */
				omr_clearPortableError(portLibrary);
				rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH;
				goto fail;
			}
#endif
#if defined (OMRZOS390)
			if (omr_getipcWrapper(portLibrary, controlinfo->common.shmid, &info, sizeof(IPCQPROC), IPCQALL) == -1) {
				int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
				rc = OMRPORT_ERROR_SHMEM_OPFAILED;
				if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
					Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("__getipc(): The shared SysV obj was deleted, but the control file still exists.");
					rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHARED_MEMORY_NOT_FOUND;
					goto fail;
				} else {
					if ((lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) && (shmid != -1)) {
						Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("__getipc(): has returned EACCES.");
						goto fail;
					}/*Any other error and we will terminate*/

					/*If sXmctl fails our checks below will also fail (they use the same function) ... so we terminate with an error*/
					Trc_PRT_shmem_omrshmem_openSharedMemory_MsgWithError("Error: __getipc(): failed. Can not open shared shared memory, portable errorCode = ", lastError);
					goto failDontUnlink;
				}
			} else {
				if (info.shm.ipcqkey != controlinfo->common.ftok_key) {
					Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The <key,id> pair in our control file is no longer valid.");
					rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHM_KEY_MISMATCH;
					goto fail;
				}
			}
#endif
			/*Things are looking good so far ...  continue on and do other checks*/
		}

		/* Check for gid, uid and size only for regular control files.
		 * These fields in older control files cannot be read.
		 * See comments in omrshmem.h
		 */
		if (OMRSH_SYSV_REGULAR_CONTROL_FILE == cacheFileType) {
			rc = omrshmem_checkGid(portLibrary, controlinfo->common.shmid, controlinfo->gid);
			if (rc == 0) {
				Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The creator gid does not match our control file.");
				/* Clear any stale portlibrary error code */
				omr_clearPortableError(portLibrary);
				rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHM_GROUPID_CHECK_FAILED;
				goto fail;
			} else if (rc == -1) {
				Trc_PRT_shmem_omrshmem_openSharedMemory_ExitWithMessage("Error: omrshmem_checkGid failed during shmctl.");
				rc = OMRPORT_ERROR_SHMEM_OPFAILED;
				goto failDontUnlink;
			} /*Continue looking for opportunity to re-create*/

			rc = omrshmem_checkUid(portLibrary, controlinfo->common.shmid, controlinfo->uid);
			if (rc == 0) {
				Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The creator uid does not match our control file.");
				/* Clear any stale portlibrary error code */
				omr_clearPortableError(portLibrary);
				rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHM_USERID_CHECK_FAILED;
				goto fail;
			} else if (rc == -1) {
				Trc_PRT_shmem_omrshmem_openSharedMemory_ExitWithMessage("Error: omrshmem_checkUid failed during shmctl.");
				rc = OMRPORT_ERROR_SHMEM_OPFAILED;
				goto failDontUnlink;
			} /*Continue looking for opportunity to re-create*/

			rc = omrshmem_checkSize(portLibrary, controlinfo->common.shmid, controlinfo->size);
			if (rc == 0) {
				Trc_PRT_shmem_omrshmem_openSharedMemory_Msg("The size does not match our control file.");
				rc = OMRPORT_ERROR_SHMEM_OPFAILED_SHM_SIZE_CHECK_FAILED;
				/* Clear any stale portlibrary error code */
				omr_clearPortableError(portLibrary);
				goto fail;
			} else if (rc == -1) {
				Trc_PRT_shmem_omrshmem_openSharedMemory_ExitWithMessage("Error: omrshmem_checkSize failed during shmctl.");
				rc = OMRPORT_ERROR_SHMEM_OPFAILED;
				goto failDontUnlink;
			} /*Continue looking for opportunity to re-create*/
		}

		/*No more checks ... We want this loop to exec only once ... so we break ...*/
	} while (FALSE);

	if (-1 == shmid) {
		rc = OMRPORT_ERROR_SHMEM_OPFAILED;
		goto failDontUnlink;
	}
	Trc_PRT_shmem_omrshmem_openSharedMemory_ExitWithMessage("Successfully opened shared memory.");
	return OMRPORT_INFO_SHMEM_OPENED;
fail:
	*canUnlink = TRUE;
	Trc_PRT_shmem_omrshmem_openSharedMemory_ExitWithMessage("Error: can not open shared memory (shmget failed). Caller MAY unlink control file.");
	return rc;
failDontUnlink:
	*canUnlink = FALSE;
	Trc_PRT_shmem_omrshmem_openSharedMemory_ExitWithMessage("Error: can not open shared memory (shmget failed). Caller should not unlink control file");
	return rc;
}

/**
 * @internal
 * Create a SysV shared memory 
 *
 * @param[in] portLibrary
 * @param[in] fd of file to write info to ...
 * @param[in] isReadOnlyFD set to TRUE if control file is read-only, FALSE otherwise
 * @param[in] baseFile file being used for control file
 * @param[in] size size of the the shared memory
 * @param[in] perm permission for the region
 * @param[in] controlinfo control file info (copy of data last written to control file)
 * @param[in] groupPerm group permission of the shared memory
 * @param[out] handle used on z/OS to store current storage key
 *
 * @return	OMRPORT_INFO_SHMEM_CREATED on success
 */
static int32_t 
omrshmem_createSharedMemory(OMRPortLibrary *portLibrary, intptr_t fd, BOOLEAN isReadOnlyFD, const char *baseFile, int32_t size, int32_t perm, struct omrshmem_controlFileFormat * controlinfo, uintptr_t groupPerm, struct omrshmem_handle *handle)
{

	intptr_t i = 0;
	intptr_t projId = SHMEM_FTOK_PROJID;
	intptr_t maxProjIdRetries = 20;
	int shmflags = groupPerm ? SHMFLAGS_GROUP : SHMFLAGS;
#if defined (OMRZOS390)
	int32_t oldsize = size;
#endif

	Trc_PRT_shmem_omrshmem_createSharedMemory_EnterWithMessage("start");

	if ((TRUE == isReadOnlyFD) || (OMRSH_SHMEM_PERM_READ == perm)) {
		Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessage("Error: can not write control file b/c either file descriptor is opened read only or shared memory permission is set to read-only.");
		return OMRPORT_ERROR_SHMEM_OPFAILED;
	}

	if (controlinfo == NULL) {
		Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessage("Error: controlinfo == NULL");
		return OMRPORT_ERROR_SHMEM_OPFAILED;
	}

#if defined (OMRZOS390)
	/* round size up to next megabyte */
	if ((0 == size) || ((size & 0xfff00000) != size)) {
		if ( size > 0x7ff00000) {
			size = 0x7ff00000;
		} else {
			size = (size + 0x100000) & 0xfff00000;
		}
	}
	Trc_PRT_shmem_omrshmem_createSharedMemory_round_size( oldsize, size);
#endif

	for (i = 0; i < maxProjIdRetries; i += 1, projId += 1) {
		int shmid = -1;
		key_t fkey = -1;
		intptr_t rc = 0;

		fkey = omr_ftokWrapper(portLibrary, baseFile, projId);
		if (-1 == fkey) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOENT) {
				Trc_PRT_shmem_omrshmem_createSharedMemory_Msg("ftok: ENOENT");
			} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shmem_omrshmem_createSharedMemory_Msg("ftok: EACCES");
			} else {
				/*By this point other errno's should not happen*/
				Trc_PRT_shmem_omrshmem_createSharedMemory_MsgWithError("ftok: unexpected errno, portable errorCode = ", lastError);
			}
			Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessage("Error: fkey == -1");
			return OMRPORT_ERROR_SHMEM_OPFAILED;
		}

		shmid = omr_shmgetWrapper(portLibrary, fkey, size, shmflags);

#if defined (OMRZOS390)
		if(-1 == shmid) {
			/* zOS doesn't ignore shmget flags that is doesn't understand. So for share classes to work */
			/* on zOS system without APAR OA11519, we need to retry without __IPC_SHAREAS flags */
			shmflags = groupPerm ? SHMFLAGS_GROUP_NOSHAREAS : SHMFLAGS_NOSHAREAS;
			Trc_PRT_shmem_omrshmem_call_shmget_zos(fkey, size, shmflags);
			shmid = omr_shmgetWrapper(portLibrary, fkey, size, shmflags);
		}
#endif

		if (-1 == shmid) {
			int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
			if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EEXIST || lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES) {
				Trc_PRT_shmem_omrshmem_createSharedMemory_Msg("Retry (errno == EEXIST || errno == EACCES) was found.");
				continue;
			}
			if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
				omrerror_set_last_error(portLibrary, EINVAL, OMRPORT_ERROR_SHMEM_TOOBIG);
			}
			Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessageAndError("Error: portable errorCode = ", lastError);
			return OMRPORT_ERROR_SHMEM_OPFAILED;
		}

#if defined (OMRZOS390)
		handle->controlStorageProtectKey = handle->currentStorageProtectKey;
		Assert_PRT_true(0xF000 > OMRSH_MODLEVEL);
		controlinfo->common.modlevel = OMRSH_SHM_CREATE_MODLEVEL_WITH_STORAGEKEY(handle->controlStorageProtectKey);
		Trc_PRT_shmem_omrshmem_createSharedMemory_storageKey(handle->controlStorageProtectKey);
#else
		controlinfo->common.modlevel = OMRSH_MODLEVEL;
#endif
		controlinfo->common.version = OMRSH_VERSION;
		controlinfo->common.proj_id = projId;
		controlinfo->common.ftok_key = fkey;
		controlinfo->common.shmid = shmid;
		controlinfo->size = size;
		controlinfo->uid = geteuid();
		controlinfo->gid = getegid();

		if (-1 == omrfile_seek(portLibrary, fd, 0, EsSeekSet)) {
			Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessage("Error: could not seek to start of control file");
			/* Do not set error code if shmctl fails, else omrfile_seek(portLibrary) error code will be lost. */
			if (omr_shmctlWrapper(portLibrary, FALSE, shmid, IPC_RMID, NULL) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return OMRPORT_ERROR_SHMEM_OPFAILED;
			}
			return OMRPORT_ERROR_SHMEM_OPFAILED;
		}

		rc = omrfile_write(portLibrary, fd, controlinfo, sizeof(omrshmem_controlFileFormat));
		if (rc != sizeof(omrshmem_controlFileFormat)) {
			Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessage("Error: write to control file failed");
			/* Do not set error code if shmctl fails, else omrfile_write(portLibrary) error code will be lost. */
			if (omr_shmctlWrapper(portLibrary, FALSE, shmid, IPC_RMID, NULL) == -1) {
				/*Already in an error condition ... don't worry about failure here*/
				return OMRPORT_ERROR_SHMEM_OPFAILED;
			}
			return OMRPORT_ERROR_SHMEM_OPFAILED;
		}

		Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessage("Successfully created memory");
		return OMRPORT_INFO_SHMEM_CREATED;
	}

	Trc_PRT_shmem_omrshmem_createSharedMemory_ExitWithMessage("Error: end of retries. Clean up SysV using 'ipcs'");
	return OMRPORT_ERROR_SHMEM_OPFAILED;
}

/**
 * @internal
 * Check the size of the shared memory
 *
 * @param[in] portLibrary The port library
 * @param[in] shmid shared memory set id
 * @param[in] size
 *
 * @return	-1 if we can't get info from shmid, 1 if it match is found, 0 if it does not match, -2 if shmctl() returned size as 0
 */
static intptr_t 
omrshmem_checkSize(OMRPortLibrary *portLibrary, int shmid, int64_t size)
{

	intptr_t rc;
	struct shmid_ds buf;

	rc = omr_shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &buf);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	/* Jazz103 88930 : On 64-bit z/OS the size of a 31-bit shared memory segment 
	 * is returned in shmid_ds.shm_segsz_31 when queried by a 64-bit process.
	 */
	if (((int64_t) buf.shm_segsz == size)
#if defined(OMRZOS39064)
		|| ((int64_t)*(int32_t *)buf.shm_segsz_31 == size)
#endif /* defined(OMRZOS39064) */
	) {
		return 1;
	} else {
#if defined(OMRZOS390)
		if (0 == buf.shm_segsz) {
			/* JAZZ103 PR 79909 + PR 88903 : shmctl(IPC_STAT) can return segment size as 0 on z/OS 
			 * if the shared memory segment was created by a 64-bit JVM and the caller is a 31-bit JVM.
			 */
			return -2;
		}
#endif /* defined(OMRZOS390) */
		return 0;
	}
}

/**
 * @internal
 * Check the creator uuid of the shared memory
 *
 * @param[in] portLibrary The port library
 * @param[in] shmid shared memory set id
 * @param[in] uid
 *
 * @return	-1 if we can't get info from shmid, 1 if it match is found, 0 if it does not match
 */
static intptr_t 
omrshmem_checkUid(OMRPortLibrary *portLibrary, int shmid, int32_t uid)
{

	intptr_t rc;
	struct shmid_ds buf;

	rc = omr_shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &buf);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (buf.shm_perm.cuid == uid) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Check the creator gid of the shared memory
 *
 * @param[in] portLibrary The port library
 * @param[in] shmid shared memory set id
 * @param[in] gid
 *
 * @return	-1 if we can't get info from shmid, 1 if it match is found, 0 if it does not match
 */
static intptr_t 
omrshmem_checkGid(OMRPortLibrary *portLibrary, int shmid, int32_t gid)
{

	intptr_t rc;
	struct shmid_ds buf;

	rc = omr_shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &buf);
	if (-1 == rc) {
		int32_t lastError = omrerror_last_error_number(portLibrary) | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
		if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL) {
			return 0;
		} else if (lastError == OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM) {
			return 0;
		}
		return -1;
	}

	if (buf.shm_perm.cgid == gid) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @internal
 * Initialize OMRPortShmemStatistic with default values.
 *
 * @param[in] portLibrary
 * @param[out] statbuf Pointer to OMRPortShmemStatistic to be initialized
 *
 * @return void
 */
static void
omrshmem_initShmemStatsBuffer(OMRPortLibrary *portLibrary, OMRPortShmemStatistic *statbuf)
{
	if (NULL != statbuf) {
		memset(statbuf, 0, sizeof(OMRPortShmemStatistic));
	}
}

/**
 * @internal
 * Retrieves stats for the shared memory region identified by shmid, and populates OMRPortShmemStatistic.
 * Only those fields in OMRPortShmemStatistic are touched which have corresponding fields in OS data structures.
 * Rest are left unchanged (eg file, controlDir).
 *
 * @param[in] portLibrary the Port Library.
 * @param[in] shmid shared memory id
 * @param[out] statbuf the statistics returns by the operating system
 *
 * @return	OMRSH_FAILED if failed to get stats, OMRSH_SUCCESS on success.
 */
static intptr_t
omrshmem_getShmStats(OMRPortLibrary *portLibrary, int shmid, struct OMRPortShmemStatistic* statbuf)
{
	intptr_t rc = OMRPORT_ERROR_SHMEM_STAT_FAILED;
	struct shmid_ds shminfo;

	rc = omr_shmctlWrapper(portLibrary, TRUE, shmid, IPC_STAT, &shminfo);
	if (OMRSH_FAILED == rc) {
		int32_t lasterrno = omrerror_last_error_number(portLibrary);
		const char *errormsg = omrerror_last_error_message(portLibrary);
		Trc_PRT_shmem_omrshmem_getShmStats_shmctlFailed(shmid, lasterrno, errormsg);
		rc = OMRPORT_ERROR_SHMEM_STAT_FAILED;
		goto _end;
	}

	statbuf->shmid = shmid;
	statbuf->ouid = shminfo.shm_perm.uid;
	statbuf->ogid = shminfo.shm_perm.gid;
	statbuf->cuid = shminfo.shm_perm.cuid;
	statbuf->cgid = shminfo.shm_perm.cgid;
	statbuf->lastAttachTime = shminfo.shm_atime;
	statbuf->lastDetachTime = shminfo.shm_dtime;
	statbuf->lastChangeTime = shminfo.shm_ctime;
	statbuf->nattach = shminfo.shm_nattch;
	/* Jazz103 88930 : On z/OS a 64-bit process needs to access shminfo.shm_segsz_31 
	 * to get size of a 31-bit shared memory segment. 
	 */
#if defined(OMRZOS39064)
	if (0 != shminfo.shm_segsz) {
		statbuf->size = (uintptr_t) shminfo.shm_segsz;
	} else {
		statbuf->size = (uintptr_t)*(int32_t *) shminfo.shm_segsz_31;
	}
#else /* defined(OMRZOS39064) */
	statbuf->size = (uintptr_t) shminfo.shm_segsz;
#endif /* defined(OMRZOS39064) */

	/* Same code as in omrfile_stat(portLibrary). Probably a new function to convert OS modes to OMR modes is required. */
	if (shminfo.shm_perm.mode & S_IWUSR) {
		statbuf->perm.isUserWriteable = 1;
	}
	if (shminfo.shm_perm.mode & S_IRUSR) {
		statbuf->perm.isUserReadable = 1;
	}
	if (shminfo.shm_perm.mode & S_IWGRP) {
		statbuf->perm.isGroupWriteable = 1;
	}
	if (shminfo.shm_perm.mode & S_IRGRP) {
		statbuf->perm.isGroupReadable = 1;
	}
	if (shminfo.shm_perm.mode & S_IWOTH) {
		statbuf->perm.isOtherWriteable = 1;
	}
	if (shminfo.shm_perm.mode & S_IROTH) {
		statbuf->perm.isOtherReadable = 1;
	}

	rc = OMRPORT_INFO_SHMEM_STAT_PASSED;

_end:
	return rc;
}
