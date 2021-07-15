/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrsharedhelper.h"
#include "omrport.h"
#include "omrportpriv.h"
#include "ut_omrport.h"
#include "portnls.h"
#include "omrportptb.h"
#include <errno.h>

#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>

#if defined(J9ZOS390)
#if !defined(OMR_EBCDIC)
#include "atoe.h"
#endif
#include "spawn.h"
#include "sys/wait.h"
#endif

#if defined(AIXPPC)
#include "protect_helpers.h"
#endif

#define J9VERSION_NOT_IN_DEFAULT_DIR_MASK 0x10000000

#if defined(AIXPPC)
typedef struct protectedWriteInfo {
	char *writeAddress;
	uintptr_t length;
} protectedWriteInfo;
#endif

#if !defined(J9ZOS390)
#define __errno2() 0
#endif

static BOOLEAN omr_IsFileReadWrite(struct J9FileStat * statbuf);

void
omr_clearPortableError(OMRPortLibrary *portLibrary) {
   omrerror_set_last_error(portLibrary, 0, 0);
}

intptr_t
omr_changeDirectoryPermission(struct OMRPortLibrary *portLibrary, const char* pathname, uintptr_t permission)
{
	struct stat statbuf;
	Trc_PRT_shared_omr_changeDirectoryPermission_Entry( pathname, permission);
	if (OMRSH_DIRPERM_DEFAULT != permission) {
		/* check if sticky bit is to be enabled */
		if (OMRSH_DIRPERM_DEFAULT_WITH_STICKYBIT == permission) {
			if (stat(pathname, &statbuf) == -1) {
#if defined(ENOENT) && defined(ENOTDIR)
				if (ENOENT == errno || ENOTDIR == errno) {
					Trc_PRT_shared_omr_changeDirectoryPermission_Exit3(errno);
					return OMRSH_FILE_DOES_NOT_EXIST;
				} else {
#endif
					Trc_PRT_shared_omr_changeDirectoryPermission_Exit4(errno);
					return OMRSH_FAILED;
#if defined(ENOENT) && defined(ENOTDIR)
				}
#endif
			} else {
				permission |= statbuf.st_mode;
			}
		}
		if(-1 == chmod(pathname, permission)) {
#if defined(ENOENT) && defined(ENOTDIR)
			if (ENOENT == errno || ENOTDIR == errno) {
				Trc_PRT_shared_omr_changeDirectoryPermission_Exit2(errno);
				return OMRSH_FILE_DOES_NOT_EXIST;
			} else {
#endif
				Trc_PRT_shared_omr_changeDirectoryPermission_Exit1(errno);
				return OMRSH_FAILED;
#if defined(ENOENT) && defined(ENOTDIR)
			}
#endif
		} else {
			Trc_PRT_shared_omr_changeDirectoryPermission_Exit();
			return OMRSH_SUCCESS;
		}
	} else {
		Trc_PRT_shared_omr_changeDirectoryPermission_Exit();
		return OMRSH_SUCCESS;
	}
}
/* Note that the "pathname" parameter may be changed by this function */
intptr_t
omr_createDirectory(struct OMRPortLibrary *portLibrary, char *pathname, uintptr_t permission)
{
	char tempPath[OMRSH_MAXPATH];
	char *current;

	Trc_PRT_shared_omr_createDirectory_Entry( pathname );

	if (0 == omrfile_mkdir(portLibrary, pathname)) {
		Trc_PRT_shared_omr_createDirectory_Exit();
		return OMRSH_SUCCESS;
	} else if (OMRPORT_ERROR_FILE_EXIST == omrerror_last_error_number(portLibrary)) {
		Trc_PRT_shared_omr_createDirectory_Exit1();
		return OMRSH_SUCCESS; 
	}
		
	omrstr_printf(portLibrary, tempPath, OMRSH_MAXPATH, "%s", pathname);
		
	current = strchr(tempPath+1, DIR_SEPARATOR); /* skip the first '/' */

	if (OMRSH_DIRPERM_ABSENT == permission) {
		permission = OMRSH_PARENTDIRPERM;
	}

	while ((NULL != current) && (omrfile_attr(portLibrary, pathname) != EsIsDir)) {
		char *previous;
			
		*current='\0';

#if defined(OMRSHSEM_DEBUG)    		
		portLibrary->tty_printf(portLibrary, "mkdir %s\n",tempPath);
#endif

		if (0 == omrfile_mkdir(portLibrary, tempPath)) {
			Trc_PRT_shared_omr_createDirectory_Event1(tempPath);
		} else {
			/* check to see whether the directory has already been created, if it is, it should
			return file exist error. If not we should return */
			if (OMRPORT_ERROR_FILE_EXIST != omrerror_last_error_number(portLibrary)) {
				Trc_PRT_shared_omr_createDirectory_Exit3(tempPath);
				return OMRSH_FAILED;
			}
			Trc_PRT_shared_omr_createDirectory_Event2(tempPath);
  		}

		previous = current;    		
		current = strchr(current+1, DIR_SEPARATOR);
		*previous=DIR_SEPARATOR;
	}
	
	Trc_PRT_shared_omr_createDirectory_Exit2();	
	return OMRSH_SUCCESS;
}
		
/**
 * @internal
 * @brief This function opens a file and takes a writer lock on it. If the file has been removed we repeat the process.
 *
 * This function is used to help manage files used to control System V objects (semaphores and memory)
 * 
 * @param[in] portLibrary The port library
 * @param[out] fd the file descriptor we locked on (if we are successful)
 * @param[out] isReadOnlyFD is the file opened read only (we do this only if we can not obtain write access)
 * @param[out] canCreateNewFile if true this function may create the file if it does not exist.
 * @param[in] filename the file we are opening
 * @param[in] groupPerm 1 if the file needs to have group read-write permission, 0 otherwise; used only when creating new control file
 *
 * @return 0 on sucess or less than 0 in the case of an error
 */
intptr_t 
omr_ControlFileOpenWithWriteLock(struct OMRPortLibrary* portLibrary, intptr_t * fd, BOOLEAN * isReadOnlyFD, BOOLEAN canCreateNewFile, const char * filename, uintptr_t groupPerm)
{
	BOOLEAN weAreDone = FALSE;
	struct stat statafter;
	int32_t openflags =  EsOpenWrite | EsOpenRead;
	int32_t exclcreateflags =  EsOpenCreate | EsOpenWrite | EsOpenRead | EsOpenCreateNew;	
	int32_t lockType = OMRPORT_FILE_WRITE_LOCK;

	Trc_PRT_shared_omr_ControlFileFDWithWriteLock_EnterWithMessage("Start");

	if (fd == NULL) {
		Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: fd is null");
		return OMRSH_FAILED;
	}

	while (weAreDone == FALSE) {
		BOOLEAN fileExists = TRUE;
		struct J9FileStat statbuf;
		BOOLEAN tryOpenReadOnly = FALSE;

		*fd = 0;
		lockType = OMRPORT_FILE_WRITE_LOCK;

		if (0 != omrfile_stat(portLibrary, filename, 0, &statbuf)) {
			int32_t porterrno = omrerror_last_error_number(portLibrary);
			switch (porterrno) {
				case OMRPORT_ERROR_FILE_NOENT:
					if (TRUE == canCreateNewFile) {
						fileExists = FALSE;
					} else {
						Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: j9file_stat() has failed because the file does not exist, and 'FALSE == canCreateNewFile'");
						return OMRSH_FILE_DOES_NOT_EXIST;
					}
					break;
				default:
					Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: j9file_stat() has failed");
					return OMRSH_FAILED;
			}
		}

		/*Only create a new file if it does not exist, and if we are allowed to.*/
		if ((FALSE == fileExists) && (TRUE == canCreateNewFile)) {
			int32_t mode = (1 == groupPerm) ? OMRSH_BASEFILEPERM_GROUP_RW_ACCESS : OMRSH_BASEFILEPERM;

			*fd = omrfile_open(portLibrary, filename, exclcreateflags, mode);
			if (*fd != -1) {		
				if (omrfile_chown(portLibrary, filename, OMRPORT_FILE_IGNORE_ID, getegid()) == -1) {
					/*If this fails it is not fatal ... but we may have problems later ...*/
					Trc_PRT_shared_omr_ControlFileFDWithWriteLock_Message("Info: could not chown file.");
				}
				if (-1 == omrfile_close(portLibrary, *fd)) {
					Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: could not close exclusively created file");
					return OMRSH_FAILED;
				}
			} else {
				int32_t porterrno = omrerror_last_error_number(portLibrary);
				if (OMRPORT_ERROR_FILE_EXIST != porterrno) {
					Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: j9file_open() failed to create new control file");
					return OMRSH_FAILED;
				}
			}
			if (0 != omrfile_stat(portLibrary, filename, 0, &statbuf)) {
				int32_t porterrno = omrerror_last_error_number(portLibrary);
				switch (porterrno) {
					case OMRPORT_ERROR_FILE_NOENT:
						/*Our unlinking scheme may cause the file to be deleted ... we go back to the beginning of the loop in this case*/
						continue;
					default:
						Trc_PRT_shared_omr_ControlFileFDWithWriteLock_Message("Error: j9file_stat() failed after successfully creating file.");
						return OMRSH_FAILED;
				}
			}
		}

		/*Now get a file descrip for caller ... and obtain a writer lock ... */
		*fd = 0;
		*isReadOnlyFD=FALSE;

		if (TRUE == omr_IsFileReadWrite(&statbuf)) {
			/*Only open file rw if stat says it is not likely to fail*/
			*fd = omrfile_open(portLibrary, filename, openflags, 0);
			if (-1 == *fd) {
				int32_t porterrno = omrerror_last_error_number(portLibrary);
				if (porterrno == OMRPORT_ERROR_FILE_NOPERMISSION) {
					tryOpenReadOnly = TRUE;
				}
			} else {
				*isReadOnlyFD=FALSE;
			}
		} else {
			tryOpenReadOnly = TRUE;
		}
		
		if (TRUE == tryOpenReadOnly) {
			/*If opening the file rw will fail then default to r*/
			*fd = omrfile_open(portLibrary, filename, EsOpenRead, 0);
			if (-1 != *fd) {
				*isReadOnlyFD=TRUE;
			}
		}

		if (-1 == *fd) {
			int32_t porterrno = omrerror_last_error_number(portLibrary);
			if ((porterrno == OMRPORT_ERROR_FILE_NOENT) || (porterrno == OMRPORT_ERROR_FILE_NOTFOUND)) {
				/*Our unlinking scheme may cause the file to be deleted ... we go back to the beginning of the loop in this case*/
				if (canCreateNewFile == TRUE) {
					continue;
				} else {
					Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: file does not exist");
					return OMRSH_FILE_DOES_NOT_EXIST;
				}
			}
			Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: failed to open control file");
			return OMRSH_FAILED;
		}

		/*Take a reader lock if we open the file read only*/
		if (*isReadOnlyFD == TRUE) {
			lockType = OMRPORT_FILE_READ_LOCK;
		}
		if (omrfile_lock_bytes(portLibrary, *fd, lockType | OMRPORT_FILE_WAIT_FOR_LOCK, 0, 0) == -1) {
			if (-1 == omrfile_close(portLibrary, *fd)) {
				Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: closing file after fcntl error has failed");
				return OMRSH_FAILED;
			}
			Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: failed to take a lock on the control file");
			return OMRSH_FAILED;
		} else {
			/*CMVC 99667: there is no stat function in the port lib ... so we are careful to handle bias used by z/OS*/
			if (fstat( (((int)*fd) - FD_BIAS), &statafter) == -1) {
				omrerror_set_last_error(portLibrary, errno, OMRPORT_ERROR_FILE_FSTAT_FAILED);
				if (-1 == omrfile_close(portLibrary, *fd)) {
					Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: closing file after fstat error has failed");
					return OMRSH_FAILED;
				}
				Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: failed to stat file descriptor");
				return OMRSH_FAILED;
			}
			if (0 == statafter.st_nlink) {
				/* We restart this process if the file was deleted */
				if (-1 == omrfile_close(portLibrary, *fd)) {
					Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Error: closing file after checking link count");
					return OMRSH_FAILED;
				}
				continue;
			}

			/*weAreDone=TRUE;*/
			Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Success");
			return OMRSH_SUCCESS;
		}
	}
	/*we should never reach here*/
	Trc_PRT_shared_omr_ControlFileFDWithWriteLock_ExitWithMessage("Failed to open file");
	return OMRSH_FAILED;
}
/**
 * @internal
 * @brief This function closes a file and releases a writer lock on it. 
 *
 * This function is used to help manage files used to control System V objects (semaphores and memory)
 * 
 * @param[in] portLibrary The port library
 * @param[in] fd the file descriptor we wish to close
 *
 * @return 0 on sucess or less than 0 in the case of an error
 */

intptr_t
omr_ControlFileCloseAndUnLock(struct OMRPortLibrary* portLibrary, intptr_t fd)
{
	Trc_PRT_shared_omr_ControlFileFDUnLock_EnterWithMessage("Start");
	if (-1 == omrfile_close(portLibrary, fd)) {
		Trc_PRT_shared_omr_ControlFileFDUnLock_ExitWithMessage("Error: failed to close control file.");
		return OMRSH_FAILED;
	}
	Trc_PRT_shared_omr_ControlFileFDUnLock_ExitWithMessage("Success");
	return OMRSH_SUCCESS;
}

/**
 * @internal
 * @brief This function validates if the current uid has rw access to a file.
 *
 * This function is used to help manage files used to control System V objects (semaphores and memory)
 *
 * @param[in] statbuf stat infor the file in question
 *
 * @return TRUE if current uuid has rw access to the file
 */
static BOOLEAN
omr_IsFileReadWrite(struct J9FileStat * statbuf)
{
	if (statbuf->ownerUid == geteuid()) {
		if (statbuf->perm.isUserWriteable == 1 && statbuf->perm.isUserReadable == 1) {
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		if (statbuf->perm.isGroupWriteable == 1 && statbuf->perm.isGroupReadable == 1) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
}

/**
 * @internal
 * @brief This function is used to unlink a control file in an attempt to recover from an error condition.
 * To avoid overwriting the original error code/message, it creates a copy of existing error code/message,
 * unlinks the file and restores the error code/message.
 * Error code/message about an error that occurs during unlinking is stored in OMRControlFileStatus parameter.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] controlFile file to be unlinked
 * @param[out] controlFileStatus pointer to OMRControlFileStatus. Stores error code and message if any error occurs during unlinking
 *
 * @return TRUE if successfully unlinked the control file, or control file does not exist, FALSE otherwise
 */
BOOLEAN
omr_unlinkControlFile(struct OMRPortLibrary* portLibrary, const char *controlFile, OMRControlFileStatus *controlFileStatus)
{
	char originalErrMsg[J9ERROR_DEFAULT_BUFFER_SIZE];
	int32_t originalErrCode = omrerror_last_error_number(portLibrary);
	const char *currentErrMsg = omrerror_last_error_message(portLibrary);
	int32_t msgLen = strlen(currentErrMsg);
	BOOLEAN rc = FALSE;

	if (msgLen + 1 > sizeof(originalErrMsg)) {
		msgLen = sizeof(originalErrMsg) - 1;
	}
	strncpy(originalErrMsg, currentErrMsg, msgLen);
	originalErrMsg[msgLen] = '\0';
	if (-1 == omrfile_unlink(portLibrary, controlFile)) {
		/* If an error occurred during unlinking, store the unlink error code in 'controlFileStatus' if available,
		 * and restore previous error.
		 */
		int32_t unlinkErrCode = omrerror_last_error_number(portLibrary);

		if (OMRPORT_ERROR_FILE_NOENT != unlinkErrCode) {
			if (NULL != controlFileStatus) {
				const char *unlinkErrMsg = omrerror_last_error_message(portLibrary);
				msgLen = strlen(unlinkErrMsg);

				/* clear any stale status */
				memset(controlFileStatus, 0, sizeof(*controlFileStatus));
				controlFileStatus->status = OMRPORT_INFO_CONTROL_FILE_UNLINK_FAILED;
				controlFileStatus->errorCode = unlinkErrCode;
				controlFileStatus->errorMsg = omrmem_allocate_memory(portLibrary, msgLen+1, OMR_GET_CALLSITE(), 
OMRMEM_CATEGORY_PORT_LIBRARY);
				if (NULL != controlFileStatus->errorMsg) {
					memcpy(controlFileStatus->errorMsg, unlinkErrMsg, msgLen+1);
				}
			}
			rc = FALSE;
		} else {
			/* Control file is already deleted; treat it as success. */
			if (NULL != controlFileStatus) {
				/* clear any stale status */
				memset(controlFileStatus, 0, sizeof(*controlFileStatus));
				controlFileStatus->status = OMRPORT_INFO_CONTROL_FILE_UNLINKED;
			}
			rc = TRUE;
		}
	} else {
		if (NULL != controlFileStatus) {
			/* clear any stale status */
			memset(controlFileStatus, 0, sizeof(*controlFileStatus));
			controlFileStatus->status = OMRPORT_INFO_CONTROL_FILE_UNLINKED;
		}
		rc = TRUE;
	}
	/* restore error code and message as before */
	omrerror_set_last_error_with_message(portLibrary, originalErrCode, originalErrMsg);
	return rc;
}
