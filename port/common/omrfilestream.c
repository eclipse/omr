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

/**
 * @file omrfilestream.c
 * @ingroup Port
 * @brief Buffered file output through streams.
 *
 * This file implements file I/O using streams.  It is similar in API to omrfile, and functionally
 * similar to C standard IO.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include "omriconvhelpers.h"
#include "omrport.h"
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"


#if defined(J9ZOS390)
/* Needed for a translation table from ascii -> ebcdic */
#if !defined(OMR_EBCDIC)
#include "atoe.h"
#endif

/*
 * a2e overrides many functions to use ASCII strings.
 * We need the native EBCDIC string
 */
#if defined(fwrite)
#undef fwrite
#endif
#if defined(fread)
#undef fread
#endif

#endif /* defined(J9ZOS390) */

/**
 * @internal
 * Take a portable buffering mode, and turn it into a platform-specific buffering mode.
 *
 * @param[in] mode Portable buffering mode. See @ref omrfilestream_setbuffer for more mode information.
 * @return Platform specific buffering mode
 *
 * @note This will not check to make sure that the mode is valid
 */
static int
translateBufferingMode(int32_t mode)
{
	/* On most systems the modes are already mapped correctly */
	return (int) mode;
}

/**
 * @internal
 * Turns portable open flags into system-specific open flags. These flags will be suitable for the
 * function fdopen. fdopen does not accept 'b', and does not truncate a file when using 'w' or 'w+'.
 *
 * Most flags are ignored by this function: EsOpenCreate, EsOpenCreateAlways, EsOpenCreateNew,
 * EsOpenTrunc, EsOpenText, EsOpenSync, EsOpenForInherit.
 *
 * @param[in] flags The open flags for the functions.  At least one of the reading or writing open
 * flags should be used: EsOpenWrite, EsOpenRead.
 *
 * @return String flags for the filestream. NULL if the flags do not open the file for reading or
 * writing.
 */
static const char *
EsTranslateOpenFlags(int32_t flags)
{
	/* Ignore all other flags: they are taken care of earlier */
	flags &= (EsOpenWrite | EsOpenRead | EsOpenAppend);

#if defined(J9ZOS390)
	/* On Z/OS, stdio functions are shadowed to take ASCII strings, and turn them into EBCDIC.
	 * Since fdopen is a POSIX function, it needs EBCDIC strings.
	 */
#pragma convlit(suspend)
#endif /* defined(J9ZOS390) */

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

#if defined(J9ZOS390)
#pragma convlit(resume)
#endif /* defined(J9ZOS390) */

}

/**
 * @internal
 * Determines the proper portable error code to return given a native error code.
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return the (negative) portable error code
 */
static int32_t
findError (int32_t errorCode)
{
	switch (errorCode) {
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

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the filestream operations may be created here.  All resources created here will be destroyed in
 * @ref omrfilestream_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative portable error code on failure.
 */
int32_t
omrfilestream_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by
 * @ref omrfilestream_startup will be destroyed here.
 *
 * @param[in] portLibrary The port library.
 */
void
omrfilestream_shutdown(struct OMRPortLibrary *portLibrary)
{
	return;
}

/**
 * Opens a buffered filestream.  All filestreams opened this way start fully buffered.
 * Opening a filestream with EsOpenRead is not supported, and will return an error code.
 *
 * @param[in] portLibrary The port library
 * @param[in] path Name of the file to be opened.
 * @param[in] flags Portable file read/write attributes.
 * @param[in] mode Platform file permissions.
 *
 * @return The filestream for the newly opened file.  NULL on failure.
 *
 * @note accesses the "current working directory" property of the process if the path is relative.
 */
OMRFileStream *
omrfilestream_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	OMRFileStream *fileStream = NULL;
	const char *realFlags = NULL;
	intptr_t file = -1;
	intptr_t fileDescriptor = -1;

	Trc_PRT_filestream_open_Entry(path, flags, mode);

	if (NULL == path) {
		Trc_PRT_filestream_open_invalidPath(path, flags, mode);
		portLibrary->error_set_last_error(portLibrary, EINVAL, findError(EINVAL));
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	realFlags = EsTranslateOpenFlags(flags);
	if ((NULL == realFlags) || (EsOpenRead == (flags & EsOpenRead))) {
		Trc_PRT_filestream_open_invalidOpenFlags(path, flags, mode);
		portLibrary->error_set_last_error(portLibrary, EINVAL, findError(EINVAL));
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	/* Attempt to portably honor the open flags and mode by using the portlibrary */
	file = portLibrary->file_open(portLibrary, path, flags, mode);
	if (-1 == file) {
		int32_t error = portLibrary->error_last_error_number(portLibrary);
		Trc_PRT_filestream_open_failedToOpen(path, flags, mode, error);
		Trc_PRT_filestream_open_Exit(NULL);
		return NULL;
	}

	/* Get the native file descriptor */
	fileDescriptor = portLibrary->file_convert_omrfile_fd_to_native_fd(portLibrary, file);

	/* Convert the file to a fileDescriptor */
	fileStream = fdopen(fileDescriptor, realFlags);
	if (NULL == fileStream) {
		int savedErrno = errno;
		portLibrary->file_close(portLibrary, file);
		Trc_PRT_filestream_open_fdopenFailed(path, flags, mode, findError(savedErrno));
		portLibrary->error_set_last_error(portLibrary, savedErrno, findError(savedErrno));
	}

	Trc_PRT_filestream_open_Exit(fileStream);
	return fileStream;
}

/**
 * Closes a filestream.
 *
 * @param[in] portLibrary The port library
 * @param[in] fileStream The filestream to be closed
 *
 * @return 0 on success, negative portable error code on failure
 * @note It is an error to close a filestream which was previously closed.
 */
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
			rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		}
	}

	Trc_PRT_filestream_close_Exit(rc);
	return rc;
}

/**
 * Flush the buffer for the filestream to the underlying file.
 *
 * @param[in] portLibrary The port library.
 * @param[in] fileStream The filestream.
 *
 * @return 0 on success, negative portable error code on failure.
 */
int32_t
omrfilestream_sync(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream)
{
	int32_t rc = 0;

	Trc_PRT_filestream_sync_Entry(fileStream);

	if (NULL == fileStream) {
		Trc_PRT_filestream_sync_invalidFileStream(fileStream);
		rc = OMRPORT_ERROR_FILE_BADF;
	} else if (0 != fflush(fileStream)) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		Trc_PRT_filestream_sync_failedToFlush(fileStream, rc);
	}

	Trc_PRT_filestream_sync_Exit(rc);
	return rc;
}

/**
 * Write to a file. Writes up to nbytes from the provided buffer to the file referenced by the
 * filestream. It is not an error if not all bytes requested are written to the file.
 *
 * @param[in] portLibrary The port library
 * @param[in] fileStream Filestream to write to.
 * @param[in] buf Buffer to be written from.
 * @param[in] nbytes Positive size of buffer.
 *
 * @return Number of bytes written on success, negative portable error code on failure.
 *
 * @note If an error occurred, the current state of the file is unknown.
 */
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
			rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_filestream_write_failedToWrite(fileStream, buf, nbytes, (int32_t) rc);
		}
	}

	Trc_PRT_filestream_write_Exit(rc);
	return rc;
}


/**
 * Set the buffer and mode to use for the stream.
 *
 * @param[in] portLibrary The port library.
 * @param[in] fileStream An open stream.
 * @param[in] buffer User allocated buffer.  It must be size bytes long.  If buffer is NULL, a buffer
 * is automatically allocated, and deallocated when the stream is closed.  If a buffer is passed in,
 * it is the caller's responsibility to free this buffer after closing the stream.
 * @param[in] mode The mode of buffering used for the stream.  Must be one of:
 *   \arg OMRPORT_FILESTREAM_FULL_BUFFERING - Buffer all file writes and reads.
 *   \arg OMRPORT_FILESTREAM_LINE_BUFFERING - On output, buffer until a newline is written.
 *   \arg OMRPORT_FILESTREAM_NO_BUFFERING - Do not buffer file writes
 * If mode is OMRPORT_FILESTREAM_NO_BUFFERING, the buffer and size parameters are ignored.
 * OMRPORT_FILESTREAM_LINE_BUFFERING has limited support, and is not guaranteed that it will flush the buffer at
 * the newline.  It should be treated similarly to full buffering.
 * @param[in] size The size of buffer. If buffer is NULL, this may set the size of the buffer
 * automatically allocated.  It must be greater than 0, unless mode is OMRPORT_FILESTREAM_NO_BUFFERING.
 *
 * @return 0 on success, negative portable error code on failure.
 *
 * @note It is not guaranteed that the entire buffer will be used.
 */
int32_t
omrfilestream_setbuffer(OMRPortLibrary *portLibrary, OMRFileStream *fileStream, char *buffer, int32_t mode, uintptr_t size)
{
	int localMode = 0;
	int32_t rc = 0;

	Trc_PRT_filestream_setbuffer_Entry(fileStream, buffer, mode, size);

	localMode = translateBufferingMode(mode);

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

/**
 * Create a buffered filestream from a portable file descriptor.
 *
 * @param[in] portLibrary The port library.
 * @param[in] fd The file descriptor.
 * @param[in] flags Open flags
 *
 * @return the resulting filestream.  On failure, will return NULL.
 */
OMRFileStream *
omrfilestream_fdopen(OMRPortLibrary *portLibrary, intptr_t fd, int32_t flags)
{
	intptr_t fileDescriptor = 0;
	intptr_t rc = 0;
	OMRFileStream *fileStream = NULL;
	const char *realFlags = NULL;

	Trc_PRT_filestream_fdopen_Entry(fd, flags);

	/* translate the open flags */
	realFlags = EsTranslateOpenFlags(flags);
	if (NULL == realFlags) {
		portLibrary->error_set_last_error(portLibrary, EINVAL, findError(EINVAL));
		Trc_PRT_filestream_fdopen_invalidArgs(fd, flags);
		Trc_PRT_filestream_fdopen_Exit(NULL);
		return NULL;
	}

	/* Get the native file descriptor */
	fileDescriptor = portLibrary->file_convert_omrfile_fd_to_native_fd(portLibrary, fd);

	/* Convert the file descriptor to a FILE */
	fileStream = fdopen(fileDescriptor, realFlags);
	if (NULL == fileStream) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		Trc_PRT_filestream_fdopen_failed(fd, flags, (int32_t) rc);
	}

	Trc_PRT_filestream_fdopen_Exit(fileStream);

	return fileStream;
}

/**
 * Get the portable file descriptor backing a filestream.
 *
 * @param[in] portLibrary The port library.
 * @param[in] fileStream The filestream;
 * @return The portable file descriptor. Will return OMRPORT_INVALID_FD on error. Calling this
 * function on an invalid filestream will result in undefined behaviour.
 */
intptr_t
omrfilestream_fileno(OMRPortLibrary *portLibrary, OMRFileStream *fileStream)
{
	intptr_t omrfile = OMRPORT_INVALID_FD;

	Trc_PRT_filestream_fileno_Entry(fileStream);

	if (NULL == fileStream) {
		portLibrary->error_set_last_error(portLibrary, EINVAL, findError(EINVAL));
		Trc_PRT_filestream_fileno_invalidArgs(fileStream);
	} else {
		/* Get the C library file descriptor backing the stream */
		intptr_t fileDescriptor = fileno(fileStream);
		if (-1 == fileDescriptor) {
			int32_t rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_filestream_fileno_failed(fileStream, rc);
		} else {
			omrfile = portLibrary->file_convert_native_fd_to_omrfile_fd(portLibrary, fileDescriptor);
		}
	}

	Trc_PRT_filestream_fileno_Exit(omrfile);
	return omrfile;
}
