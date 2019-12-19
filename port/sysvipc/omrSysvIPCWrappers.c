/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#include "omrSysvIPCWrappers.h"

static void setPortableError(OMRPortLibrary *portLibrary, const char * sysvfxName, int32_t sysvfx, int sysverrno);

static const char * ftokErrorMsgPrefix = "ftok : ";
static const char * semgetErrorMsgPrefix ="semget : ";
static const char * semctlErrorMsgPrefix ="semctl : ";
static const char * semopErrorMsgPrefix ="semop : ";
static const char * shmgetErrorMsgPrefix ="shmget : ";
static const char * shmctlErrorMsgPrefix ="shmctl : ";
static const char * shmatErrorMsgPrefix ="shmat : ";
static const char * shmdtErrorMsgPrefix ="shmdt : ";
#if defined (J9ZOS390)
static const char * getipcErrorMsgPrefix = "__getipc : ";
#endif

int 
ftokWrapper(OMRPortLibrary *portLibrary, const char *baseFile, int proj_id) 
{
	int result = ftok(baseFile, proj_id);
	if (result == -1) {
		int myerror = errno;
		setPortableError(portLibrary, ftokErrorMsgPrefix,
				OMRPORT_ERROR_SYSV_IPC_FTOK_ERROR, myerror);
	}
	return result;
}

int 
semgetWrapper(OMRPortLibrary *portLibrary, key_t key, int nsems, int semflg) 
{
	int result = semget(key, nsems, semflg);
	if (result == -1) {
		int myerror = errno;
		setPortableError(portLibrary, semgetErrorMsgPrefix,
				OMRPORT_ERROR_SYSV_IPC_SEMGET_ERROR, myerror);
	}
	return result;
}

int 
semctlWrapper(OMRPortLibrary *portLibrary, BOOLEAN storeError, int semid, int semnum, int cmd, ...)
{
	int result = -1;
	va_list argp;
	semctlUnion arg;
	int myerror;

	/*NOTE: we only handle an arg for the below command (since this is all we use with args)*/
	if ((cmd == IPC_STAT) || (cmd == SETVAL)) {
		va_start(argp, cmd);
		arg = va_arg(argp, semctlUnion);
		result = semctl(semid, semnum, cmd, arg);
		myerror = errno;
		va_end(argp);
	} else {
		result = semctl(semid, semnum, cmd);
		myerror = errno;
	}

	if (result == -1) {
		if (TRUE == storeError) {
			setPortableError(portLibrary, semctlErrorMsgPrefix,
					OMRPORT_ERROR_SYSV_IPC_SEMCTL_ERROR, myerror);
		} else {
			Trc_PRT_sysvipc_semctlWrapper_Failed(myerror);
		}
	}

	return result;
}

int 
semopWrapper(OMRPortLibrary *portLibrary, int semid, struct sembuf *sops, size_t nsops) 
{
	int result = semop(semid, sops, nsops);
	if (result == -1) {
		int myerror = errno;
		setPortableError(portLibrary, semopErrorMsgPrefix,
				OMRPORT_ERROR_SYSV_IPC_SEMOP_ERROR, myerror);
	}
	return result;
}

int 
shmgetWrapper(OMRPortLibrary *portLibrary, key_t key, size_t size, int shmflg)
{
	int result = shmget(key, size, shmflg);
	if (result == -1) {
		int myerror = errno;
		setPortableError(portLibrary, shmgetErrorMsgPrefix,
				OMRPORT_ERROR_SYSV_IPC_SHMGET_ERROR, myerror);
	}
	return result;
}

int 
shmctlWrapper(OMRPortLibrary *portLibrary,  BOOLEAN storeError, int shmid, int cmd, struct shmid_ds *buf)
{
	int result = shmctl(shmid, cmd, buf);
	if (result == -1) {
		if (TRUE == storeError) {
			int myerror = errno;
			setPortableError(portLibrary, shmctlErrorMsgPrefix,
					OMRPORT_ERROR_SYSV_IPC_SHMCTL_ERROR, myerror);
		} else {
			Trc_PRT_sysvipc_shmctlWrapper_Failed(errno);
		}
	}
	return result;
}
void * 
shmatWrapper(OMRPortLibrary *portLibrary, int shmid, const void *shmaddr, int shmflg)
{
	void * result = shmat(shmid, shmaddr, shmflg);
	if (result == (void*)-1) {
		int myerror = errno;
		setPortableError(portLibrary, shmatErrorMsgPrefix,
				OMRPORT_ERROR_SYSV_IPC_SHMAT_ERROR, myerror);
	}
	return result;
}

int 
shmdtWrapper(OMRPortLibrary *portLibrary, const void *shmaddr)
{
	int32_t result = shmdt(shmaddr);
	if (result == -1) {
		int myerror = errno;
		setPortableError(portLibrary, shmdtErrorMsgPrefix,
				OMRPORT_ERROR_SYSV_IPC_SHMDT_ERROR, myerror);
	}
	return result;
}

#if defined (J9ZOS390)
int 
getipcWrapper(OMRPortLibrary *portLibrary, int id, IPCQPROC *info, size_t length, int cmd) 
{
	int result = __getipc(id, info, length, cmd);
	if (result == -1) {
		int myerror = errno;
		setPortableError(portLibrary, getipcErrorMsgPrefix,
				OMRPORT_ERROR_SYSV_IPC_GETIPC_ERROR, myerror);
	}
	return result;
}
#endif

static void 
setPortableError(OMRPortLibrary *portLibrary, const char * sysvfxName, int32_t sysvfx, int sysverrno) 
{
  //	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	char * errmsgbuff = NULL;
	int32_t errmsglen = J9ERROR_DEFAULT_BUFFER_SIZE;
	int32_t portableErrno = sysvfx;

	switch (sysverrno) {
#if defined(EACCES)
	case EACCES:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EACCES;
		break;
#endif
	case EEXIST:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EEXIST;
		break;
	case ENOENT:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOENT;
		break;
	case EINVAL:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EINVAL;
		break;
	case ENOMEM:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOMEM;
		break;
	case ENOSPC:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOSPC;
		break;
	case ELOOP:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_ELOOP;
		break;
	case ENAMETOOLONG:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_ENAMETOOLONG;
		break;
	case ENOTDIR:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_ENOTDIR;
		break;
#if defined(EPERM)
		case EPERM:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM;
		break;
#endif
	case ERANGE:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_ERANGE;
		break;
	case E2BIG:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_E2BIG;
		break;
	case EAGAIN:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EAGAIN;
		break;
	case EFBIG:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EFBIG;
		break;
#if defined(EIDRM)
		case EIDRM:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EIDRM;
		break;
#endif
	case EINTR:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EINTR;
		break;
	case EMFILE:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_EMFILE;
		break;
	default:
		portableErrno += OMRPORT_ERROR_SYSV_IPC_ERRNO_UNMAPPED;
		break;
	}
	
	/*Get size of str_printf buffer (it includes null terminator)*/
	errmsglen = omrstr_printf(portLibrary, NULL, 0,
			"%s%s", sysvfxName,
			strerror(sysverrno));
	if (errmsglen <= 0) {
		/*Set the error with no message*/
		omrerror_set_last_error(portLibrary, sysverrno, portableErrno);
		return;
	}
	
	/*Alloc the buffer*/
	errmsgbuff = omrmem_allocate_memory(portLibrary, errmsglen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == errmsgbuff) {
		/*Set the error with no message*/
		omrerror_set_last_error(portLibrary, sysverrno, portableErrno);
		return;
	}
	
	/*Fill the buffer using str_printf*/
	omrstr_printf(portLibrary, errmsgbuff,
			errmsglen, "%s%s", sysvfxName,
			strerror(sysverrno));

	/*Set the error message*/
	omrerror_set_last_error_with_message(portLibrary,
			portableErrno,
			errmsgbuff);

	/*Free the buffer*/
	omrmem_free_memory(portLibrary, errmsgbuff);

	return;
}
