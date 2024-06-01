/*******************************************************************************
 * Copyright IBM Corp. and others 2015
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

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <stdio.h>
#include <windows.h>
#include "omrfilehelpers.h"
#include "omrport.h"
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"


static int
translateBufferingMode(int32_t mode)
{
	/* On most systems the modes are already mapped correctly */
	return (int) mode;
}

static int
EsTranslateOpenFlagsToSystemFlags(int32_t flags)
{
	/* Ignore all other flags; they are taken care of earlier */
	flags &= (EsOpenWrite | EsOpenRead | EsOpenAppend);

	switch (flags) {
	case (EsOpenWrite):
		return _O_WRONLY;
	case (EsOpenRead):
		return _O_RDONLY;
	case (EsOpenWrite | EsOpenRead):
		return _O_RDWR;
	case (EsOpenAppend | EsOpenWrite):
		return _O_WRONLY | _O_APPEND;
	case (EsOpenAppend | EsOpenRead):
		return _O_RDONLY;
	case (EsOpenAppend | EsOpenWrite | EsOpenRead):
		return _O_RDWR | _O_APPEND;
	default:
		return _O_RDONLY;
	}
}

static const char *
EsTranslateOpenFlagsToStandardFlags(int32_t flags)
{
	/* Ignore all other flags; they are taken care of earlier */
	flags &= (EsOpenWrite | EsOpenRead | EsOpenAppend);

	switch (flags) {
	case (EsOpenWrite):
		return "w";
	case (EsOpenRead):
		return "r";
	case (EsOpenWrite | EsOpenRead):
		return "w+";
	case (EsOpenAppend | EsOpenWrite):
		return "a";
	case (EsOpenAppend | EsOpenRead):
		return "r";
	case (EsOpenAppend | EsOpenWrite | EsOpenRead):
		return "a+";
	default:
		return NULL;
	}
}


/**
 * @internal
 * Determines the proper portable error code to return given a standard C library error code.
 *
 * @param[in] errorCode The error code reported in errno
 *
 * @return the (negative) portable error code
 */
static int32_t
findErrorFromErrno (int32_t errorCode)
{
	switch (errorCode)
	{
	case EACCES:
		/* FALLTHROUGH */
	case EPERM:
		return OMRPORT_ERROR_FILE_NOPERMISSION;
	case ENAMETOOLONG:
		return OMRPORT_ERROR_FILE_NAMETOOLONG;
	case ENOENT:
		return OMRPORT_ERROR_FILE_NOENT;
	case ENOTDIR:
		return OMRPORT_ERROR_FILE_NOTDIR;
	case ELOOP:
		return OMRPORT_ERROR_FILE_LOOP;
	case EBADF:
		return OMRPORT_ERROR_FILE_BADF;
	case EEXIST:
		return OMRPORT_ERROR_FILE_EXIST;
	case ENOSPC:
		/* FALLTHROUGH */
	case EFBIG:
		return OMRPORT_ERROR_FILE_DISKFULL;
	case EINVAL:
		return OMRPORT_ERROR_FILE_INVAL;
	case EISDIR:
		return OMRPORT_ERROR_FILE_ISDIR;
	case EAGAIN:
		return OMRPORT_ERROR_FILE_EAGAIN;
	case EFAULT:
		return OMRPORT_ERROR_FILE_EFAULT;
	case EINTR:
		return OMRPORT_ERROR_FILE_EINTR;
	case EIO:
		return OMRPORT_ERROR_FILE_IO;
	case EOVERFLOW:
		return OMRPORT_ERROR_FILE_OVERFLOW;
	case ESPIPE:
		return OMRPORT_ERROR_FILE_SPIPE;
	default:
		return OMRPORT_ERROR_FILE_OPFAILED;
	}
}

int32_t
omrfilestream_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

void
omrfilestream_shutdown(struct OMRPortLibrary *portLibrary)
{
	return;
}

OMRFileStream *
omrfilestream_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	OMRFileStream *fileStream = NULL;
	const char *standardFlags = NULL;
	int systemFlags = 0;
	HANDLE handle = (HANDLE) -1;
	intptr_t file = -1;
	int fileDescriptor = -1;

	Trc_PRT_filestream_open_Entry(path, flags, mode);

	if (NULL == path) {
		Trc_PRT_filestream_open_invalidPath(path, flags, mode);
		portLibrary->error_set_last_error(portLibrary, ERROR_INVALID_PARAMETER, findError(ERROR_INVALID_PARAMETER));
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	standardFlags = EsTranslateOpenFlagsToStandardFlags(flags);
	if ((NULL == standardFlags) || (EsOpenRead == (flags & EsOpenRead))) {
		Trc_PRT_filestream_open_invalidOpenFlags(path, flags, mode);
		portLibrary->error_set_last_error(portLibrary, ERROR_INVALID_PARAMETER, findError(ERROR_INVALID_PARAMETER));
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	/* This will default to read only if there is a problem */
	systemFlags = EsTranslateOpenFlagsToSystemFlags(flags);

	/* Attempt to portably honor the open flags and mode by opening the file descriptor */
	file = portLibrary->file_open(portLibrary, path, flags, mode);
	if (-1 == file) {
		int32_t error = portLibrary->error_last_error_number(portLibrary);
		Trc_PRT_filestream_open_failedToOpen(path, flags, mode, error);
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	/* Get the native file descriptor, on windows this is actually a handle */
	handle = (HANDLE) portLibrary->file_convert_omrfile_fd_to_native_fd(portLibrary, file);
	if (INVALID_HANDLE_VALUE == handle) {
		int32_t error = portLibrary->error_set_last_error(portLibrary, ERROR_INVALID_HANDLE, findError(ERROR_INVALID_HANDLE));
		Trc_PRT_filestream_open_failedToOpen(path, flags, mode, error);
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	/* Turn the handle into a C file descriptor */
	fileDescriptor = _open_osfhandle((intptr_t) handle, systemFlags);
	if (-1 == fileDescriptor) {
		/* Does this change GetLastError() or errno ? */
		int savedErrno = errno;
		int32_t error = 0;
		portLibrary->file_close(portLibrary, file);
		error = portLibrary->error_set_last_error(portLibrary, savedErrno, findErrorFromErrno(savedErrno));
		Trc_PRT_filestream_open_failedToOpen(path, flags, mode, error);
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	/* Convert the C file descriptor to a filestream */
	fileStream = _fdopen(fileDescriptor, standardFlags);
	if (NULL == fileStream) {
		int savedErrno = errno;
		int32_t error = 0;
		/* _close will close the C file descriptor and the underlying HANDLE */
		_close(fileDescriptor);
		error = portLibrary->error_set_last_error(portLibrary, savedErrno, findErrorFromErrno(savedErrno));
		Trc_PRT_filestream_open_fdopenFailed(path, flags, mode, error);
	}

	Trc_PRT_filestream_open_Exit(fileStream);
	return fileStream;
}

int32_t
omrfilestream_close(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream)
{
	int32_t rc = 0;

	Trc_PRT_filestream_close_Entry(fileStream);

	if (NULL == fileStream) {
		Trc_PRT_filestream_close_invalidFileStream(fileStream);
		rc = OMRPORT_ERROR_FILE_BADF;
	} else {
		rc = fclose(fileStream);
		if (0 != rc) {
			rc = portLibrary->error_set_last_error(portLibrary, errno, findErrorFromErrno(errno));
		}
	}

	Trc_PRT_filestream_close_Exit(rc);
	return rc;
}

int32_t
omrfilestream_sync(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream)
{
	int32_t rc = 0;

	Trc_PRT_filestream_sync_Entry(fileStream);

	if (NULL == fileStream) {
		Trc_PRT_filestream_sync_invalidFileStream(fileStream);
		rc = OMRPORT_ERROR_FILE_BADF;
	} else if (0 != fflush(fileStream)) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, findErrorFromErrno(errno));
		Trc_PRT_filestream_sync_failedToFlush(fileStream, rc);
	}

	Trc_PRT_filestream_sync_Exit(rc);
	return rc;
}

intptr_t
omrfilestream_write(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const void *buf, intptr_t nbytes)
{
	intptr_t rc = 0;

	Trc_PRT_filestream_write_Entry(fileStream, buf, nbytes);

	if ((nbytes < 0) || (NULL == buf) || (NULL == fileStream)) {
		Trc_PRT_filestream_write_invalidArgs(fileStream, buf, nbytes);
		rc = OMRPORT_ERROR_FILE_INVAL;
	} else {
		/* nbytes may be truncated during this cast, resulting in smaller writes than intended */
		rc = (intptr_t) fwrite(buf, 1, (size_t) nbytes, fileStream);
		if (0 != ferror(fileStream)) {
			rc = portLibrary->error_set_last_error(portLibrary, errno, findErrorFromErrno(errno));
			Trc_PRT_filestream_write_failedToWrite(fileStream, buf, nbytes, (int32_t) rc);
		}
	}

	Trc_PRT_filestream_write_Exit(rc);
	return rc;
}

int32_t
omrfilestream_setbuffer(OMRPortLibrary *portLibrary, OMRFileStream *fileStream, char *buffer, int32_t mode, uintptr_t size)
{
	int localMode = 0;
	int32_t rc = 0;

	Trc_PRT_filestream_setbuffer_Entry(fileStream, buffer, mode, size);

	localMode = translateBufferingMode(mode);

	/* On windows, size must be greater than 2.  This is different than the C standard.
	 * In this case, ignore the user's buffer and allocate a new one. */
	if (size < 2) {
		size = 2;
		buffer = NULL;
	}

	if ((NULL == fileStream) || (-1 == localMode)) {
		Trc_PRT_filestream_setbuffer_invalidArgs(fileStream, buffer, mode, size);
		rc = OMRPORT_ERROR_FILE_INVAL;
	} else if (0 != setvbuf(fileStream, (char *) buffer, mode, size)) {
		Trc_PRT_filestream_setbuffer_failed(fileStream, buffer, mode, size);
		rc = OMRPORT_ERROR_FILE_OPFAILED;
	}

	Trc_PRT_filestream_setbuffer_Exit(rc);
	return rc;
}

OMRFileStream *
omrfilestream_fdopen(OMRPortLibrary *portLibrary, intptr_t fd, int32_t flags)
{
	const char *standardFlags = NULL;
	int systemFlags = 0;
	OMRFileStream *fileStream = NULL;
	HANDLE handle = 0;
	int fileDescriptor = 0;

	Trc_PRT_filestream_fdopen_Entry(fd, flags);

	standardFlags = EsTranslateOpenFlagsToStandardFlags(flags);
	if (NULL == standardFlags) {
		Trc_PRT_filestream_fdopen_invalidArgs(fd, flags);
		Trc_PRT_filestream_fdopen_Exit(NULL);
		return NULL;
	}

	systemFlags = EsTranslateOpenFlagsToSystemFlags(flags);

	/* Get the C library file descriptor. */
	switch (fd) {
	case OMRPORT_TTY_IN:
	case OMRPORT_TTY_OUT:
	case OMRPORT_TTY_ERR:
		/* These 3 files are already C library file descriptor. */
		fileDescriptor = (int) fd;
		break;
	default:
		/* Turn the OMRFile into a Windows HANDLE */
		handle = (HANDLE) portLibrary->file_convert_omrfile_fd_to_native_fd(portLibrary, fd);

		/* Turn the HANDLE into C library file descriptor */
		fileDescriptor = _open_osfhandle((intptr_t) handle, systemFlags);
		if (-1 == fileDescriptor) {
			/* Does this change GetLastError() or errno ? */
			int savedErrno = errno;
			int32_t error = 0;
			error = portLibrary->error_set_last_error(portLibrary, savedErrno, findErrorFromErrno(savedErrno));
			Trc_PRT_filestream_fdopen_failed(fd, flags, error);
			Trc_PRT_filestream_fdopen_Exit(NULL);
			return NULL;
		}
		break;
	}

	/* Open a OMRfilestream on the C library file descriptor */
	fileStream = _fdopen(fileDescriptor, standardFlags);
	if (NULL == fileStream) {
		/* _close will close the C file descriptor and the underlying HANDLE, there is no API for
		 * closing the fileDescriptor without closing the HANDLE.  This may leak. */
		int32_t error = portLibrary->error_set_last_error(portLibrary, errno, findErrorFromErrno(errno));
		Trc_PRT_filestream_fdopen_failed(fd, flags, error);
	}

	Trc_PRT_filestream_fdopen_Exit(fileStream);
	return fileStream;
}


intptr_t
omrfilestream_fileno(OMRPortLibrary *portLibrary, OMRFileStream *fileStream)
{
	intptr_t omrFile = OMRPORT_INVALID_FD;

	Trc_PRT_filestream_fileno_Entry(fileStream);

	if (NULL == fileStream) {
		portLibrary->error_set_last_error(portLibrary, ERROR_INVALID_PARAMETER, findError(ERROR_INVALID_PARAMETER));
		Trc_PRT_filestream_fileno_invalidArgs(fileStream);
	} else {
		intptr_t fileDescriptor = _fileno(fileStream);
		if (-1 == fileDescriptor) {
			int32_t rc = portLibrary->error_set_last_error(portLibrary, errno, findErrorFromErrno(errno));
			Trc_PRT_filestream_fileno_failed(fileStream, rc);
		} else {
			intptr_t fileHandle;
			/* Get the file descriptor. */
			switch (fileDescriptor) {
			case OMRPORT_TTY_IN:
			case OMRPORT_TTY_OUT:
			case OMRPORT_TTY_ERR:
				/* these 3 files are already FDs, not HANDLES. */
				omrFile = fileDescriptor;
				break;
			default:
				/* turn the FD into a HANDLE */
				fileHandle = _get_osfhandle((int) fileDescriptor);
				if (((intptr_t) INVALID_HANDLE_VALUE) == fileHandle) {
					int32_t rc = portLibrary->error_set_last_error(portLibrary, errno, findErrorFromErrno(errno));
					Trc_PRT_filestream_fileno_failed(fileStream, rc);
				} else {
					/* Turn the HANDLE into a OMRFileStream */
					omrFile = portLibrary->file_convert_native_fd_to_omrfile_fd(portLibrary, fileHandle);
				}
				break;
			}
		}
	}

	Trc_PRT_filestream_fileno_Exit(omrFile);
	return omrFile;
}
