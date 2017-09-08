/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief file
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "omrport.h"
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"

static int32_t EsTranslateOpenFlags(int32_t flags);
static int32_t findError(int32_t errorCode);

/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return	the (negative) portable error code
 */
static int32_t
findError(int32_t errorCode)
{
	switch (errorCode) {
	case EACCES:
		return OMRPORT_ERROR_FILE_NOPERMISSION;
	case EEXIST:
		return OMRPORT_ERROR_FILE_EXIST;
	case EINTR:
		return OMRPORT_ERROR_FILE_INTERRUPTED;
	case EISDIR:
		return OMRPORT_ERROR_FILE_ISDIR;
	case EMFILE:
		return OMRPORT_ERROR_FILE_PROCESS_MAX_OPEN;
	case ENFILE:
		return OMRPORT_ERROR_FILE_SYSTEMFULL;
	case ENOENT:
		return OMRPORT_ERROR_FILE_NOENT;
	case ENOSPC:
		return OMRPORT_ERROR_FILE_DISKFULL;
	case ENXIO:
		return OMRPORT_ERROR_FILE_XIO;
	case EROFS:
		return OMRPORT_ERROR_FILE_ROFS;
	default:
		return OMRPORT_ERROR_FILE_OPFAILED;
	}
}

/**
 * Determine whether path is a file or directory.
 *
 * @param[in] portLibrary The port library
 * @param[in] path file/path name being queried.
 *
 * @deprecated Use @ref omrfile_stat
 *
 * @return EslsFile if a file, EslsDir if a directory, negative portable error code on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 */
int32_t
omrfile_attr(struct OMRPortLibrary *portLibrary, const char *path)
{
	return -1;
}

/**
 * Change the permissions of a file or directory.
 *
 * @param[in] portLibrary The port library
 * @param[in] path Name of the file to be modified.
 * @param[in] mode file permissions, set per the Posix convention, see man chmod (http://linux.die.net/man/2/chmod).
 * 			Windows only: - File access permissions are implemented via the Windows file system READONLY bit.
 * 						The only supported permissions are read-only (for everyone) or read/write (for everyone).
 * 						The sticky, setgid, and setuid bits are ignored, as are the read and execute bits.
 *                      If the mode argument requests write access for all of owner, group, and world, the READONLY attribute is cleared.
 *                      Otherwise READONLY is set.
 *
 * @return The new permissions of the  file or -1 on failure.
 * 			Windows only: The return value is (octal) 0444 (owner, group, and world readable)
 * 					if the Windows READONLY bit is set and 0666 (owner, group, and world readable and writable)if READONLY is not set.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 *
 */

int32_t
omrfile_chmod(struct OMRPortLibrary *portLibrary, const char *path, int32_t mode)
{
	return -1;
}

/**
 * Change owner and group of a file or directory.
 *
 * Windows:
 * 	Effectively a noop, success is always returned,
 *
 * Unix-derived platforms:
 *  Follows the rules of chown. Please see opengroup chown man pages for more details.
 *
 * @param[in] portLibrary The port library
 * @param[in] path path to directory or file
 * @param[in] owner desired owner
 * \arg OMRPORT_FILE_IGNORE_ID  to leave the owner unchanged
 * @param[in] group desired group,
 * \arg OMRPORT_FILE_IGNORE_ID to leave the group unchanged
 *
 * @return negative portable error code on failure, 0 on success. On Windows, 0 is always returned
 * 	and the operation has no effect
 */
intptr_t
omrfile_chown(struct OMRPortLibrary *portLibrary, const char *path, uintptr_t owner, uintptr_t group)
{
	return -1;
}

/**
 * Closes a file descriptor.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd The file descriptor.
 *
 * @return 0 on success, -1 on failure.
 * @internal @todo return negative portable return code on failure.
 */
int32_t
omrfile_close(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	int32_t rc = 0;

	Trc_PRT_file_close_Entry(fd);

	rc = (int32_t)close((int)fd);

	Trc_PRT_file_close_Exit(rc);
	return rc;
}

/**
 * Return an error message describing the last OS error that occurred.  The last
 * error returned is not thread safe, it may not be related to the operation that
 * failed for this thread.
 *
 * @param[in] portLibrary The port library
 *
 * @return	error message describing the last OS error, may return NULL.
 *
 * @internal
 * @note  This function gets the last error code from the OS and then returns
 * the corresponding string.  It is here as a helper function for JCL.  Once omrerror
 * is integrated into the port library this function should probably disappear.
 */
const char *
omrfile_error_message(struct OMRPortLibrary *portLibrary)
{
	return portLibrary->error_last_error_message(portLibrary);
}

/**
 * Close the handle returned from @ref omrfile_findfirst.
 *
 * @param[in] portLibrary The port library
 * @param[in] findhandle  Handle returned from @ref omrfile_findfirst.
 */
void
omrfile_findclose(struct OMRPortLibrary *portLibrary, uintptr_t findhandle)
{
	return;
}

/**
 * Find the first occurrence of a file identified by path.  Answers a handle
 * to be used in subsequent calls to @ref omrfile_findnext and @ref omrfile_findclose.
 *
 * @param[in] portLibrary The port library
 * @param[in] path file/path name being queried.
 * @param[out] resultbuf filename and path matching path.
 *
 * @return valid handle on success, -1 on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 *
 */
uintptr_t
omrfile_findfirst(struct OMRPortLibrary *portLibrary, const char *path, char *resultbuf)
{
	return (uintptr_t)-1;
}
/**
 * Find the next filename and path matching a given handle.
 *
 * @param[in] portLibrary The port library
 * @param[in] findhandle handle returned from @ref omrfile_findfirst.
 * @param[out] resultbuf next filename and path matching findhandle.
 *
 * @return 0 on success, -1 on failure or if no matching entries.
 * @internal @todo return negative portable return code on failure.
 */
int32_t
omrfile_findnext(struct OMRPortLibrary *portLibrary, uintptr_t findhandle, char *resultbuf)
{
	return -1;
}

/**
 *  Return the last modification time of the file path in seconds.
 *
 * @param[in] portLibrary The port library
 * @param[in] path file/path name being queried.
 *
 * @return last modification time on success, -1 on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 *
 */
int64_t
omrfile_lastmod(struct OMRPortLibrary *portLibrary, const char *path)
{
	return -1;
}
/**
 * Answer the length in bytes of the file.
 *
 * @param[in] portLibrary The port library
 * @param[in] path file/path name being queried.
 *
 * @return Length in bytes of the file on success, negative portable error code on failure
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 */
int64_t
omrfile_length(struct OMRPortLibrary *portLibrary, const char *path)
{
	intptr_t fd;
	int64_t length;

	Trc_PRT_file_length_Entry(path);

	fd = portLibrary->file_open(portLibrary, path, EsOpenRead, 0666);
	if (-1 == fd) {
		Trc_PRT_file_length_Exit(-1);
		return -1;
	}
	length =  portLibrary->file_seek(portLibrary, fd, 0, EsSeekEnd);
	portLibrary->file_close(portLibrary, fd);
	Trc_PRT_file_length_Exit(length);
	return length;
}

/**
 * Answer the length in bytes of the file.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd The file descriptor.
 *
 * @return Length in bytes of the file on success, negative portable error code on failure
 */
int64_t
omrfile_flength(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	int64_t previousOffset = 0;
	int64_t length = 0;

	Trc_PRT_file_flength_Entry(fd);

	/* The file was opened at this place */
	previousOffset = portLibrary->file_seek(portLibrary, fd, 0, EsSeekCur);
	length = portLibrary->file_seek(portLibrary, fd, 0, EsSeekEnd);
	/* Set the cursor back to where it was */
	portLibrary->file_seek(portLibrary, fd, previousOffset, EsSeekSet);

	Trc_PRT_file_flength_Exit(length);
	return length;
}

/**
 * Create a directory.
 *
 * @param[in] portLibrary The port library
 * @param[in] path Directory to be created.
 *
 * @return 0 on success, portable error code on failure.
 *
 * @note Assumes all components of path up to the last directory already exist.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 *
 * @internal @todo return negative portable return code on failure.
 */
int32_t
omrfile_mkdir(struct OMRPortLibrary *portLibrary, const char *path)
{
	return -1;
}

/**
 * Move the file pathExist to a new name pathNew.
 *
 * @param[in] portLibrary The port library
 * @param[in] pathExist The existing file name.
 * @param[in] pathNew The new file name.
 *
 * @return 0 on success, -1 on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 *
 * @internal @todo return negative portable return code on failure.
 */
int32_t
omrfile_move(struct OMRPortLibrary *portLibrary, const char *pathExist, const char *pathNew)
{
	return -1;
}

/**
 * Convert a pathname into a file descriptor.
 *
 * @param[in] portLibrary The port library
 * @param[in] path Name of the file to be opened.
 * @param[in] flags Portable file read/write attributes.
 * @param[in] mode Platform file permissions.
 *
 * @return The file descriptor of the newly opened file, -1 on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 */
intptr_t
omrfile_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	int32_t fd = 0;
	int32_t realFlags = EsTranslateOpenFlags(flags);

	Trc_PRT_file_open_Entry(path, flags, mode);

	if (-1 == realFlags) {
		Trc_PRT_file_open_Exception1(path, flags);
		Trc_PRT_file_open_Exit(-1);
		return -1;
	}

	fd = open(path, realFlags, mode);

	if (-1 == fd) {
		Trc_PRT_file_open_Exception2(path, errno, findError(errno));
		portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}

	Trc_PRT_file_open_Exit(fd);
	return fd;
}

/**
 * Read bytes from a file descriptor into a user provided buffer.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd The file descriptor.
 * @param[in,out] buf Buffer to read into.
 * @param[in] nbytes Size of buffer.
 *
 * @return The number of bytes read, or -1 on failure.
 */
intptr_t
omrfile_read(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
{
	intptr_t result;

	if (nbytes == 0) {
		return 0;
	}

	result = read((int)fd, buf, nbytes);
	if ((0 == result) || (-1 == result)) {
		portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		return -1;
	} else {
		return result;
	}
}

/**
 * Repositions the offset of the file descriptor to a given offset as per directive whence.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd The file descriptor.
 * @param[in] offset The offset in the file to position to.
 * @param[in] whence Portable constant describing how to apply the offset.
 *
 * @return The resulting offset on success, -1 on failure.
 * @note whence is one of EsSeekSet (seek from beginning of file),
 * EsSeekCur (seek from current file pointer) or EsSeekEnd (seek backwards from
 * end of file).
 * @internal @note seek operations return -1 on failure.  Negative offsets
 * can be returned when seeking beyond end of file.
 */
int64_t
omrfile_seek(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t offset, int32_t whence)
{
	int64_t result = 0;
	off_t localOffset = (off_t)offset;

	Trc_PRT_file_seek_Entry(fd, offset, whence);

	if ((whence < EsSeekSet) || (whence > EsSeekEnd)) {
		Trc_PRT_file_seek_Exit(-1);
		return -1;
	}

	/* If file offsets are 32 bit, truncate the seek to that range */
	if (sizeof(off_t) < sizeof(int64_t)) {
		if (offset > 0x7FFFFFFF) {
			localOffset =  0x7FFFFFFF;
		} else if (offset < -0x7FFFFFFF) {
			localOffset = -0x7FFFFFFF;
		}
	}

	result = (int64_t)lseek((int)fd, localOffset, whence);
	Trc_PRT_file_seek_Exit(result);
	return result;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrfile_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Most implementations will be empty.
 */
void
omrfile_shutdown(struct OMRPortLibrary *portLibrary)
{
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the file operations may be created here.  All resources created here should be destroyed
 * in @ref omrfile_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_FILE
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrfile_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}
/**
 * Synchronize a file's state with the state on disk.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd The file descriptor.
 *
 * @return 0 on success, -1 on failure.
 * @internal @todo return negative portable return code on failure.
 */
int32_t
omrfile_sync(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	return -1;
}

/**
 * Remove a file from the file system.
 *
 * @param[in] portLibrary The port library
 * @param[in] path file/path name to remove.
 *
 * @return 0 on success, -1 on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 *
 * @internal @todo return negative portable return code on failure.
 */
int32_t
omrfile_unlink(struct OMRPortLibrary *portLibrary, const char *path)
{
	return -1;
}

/**
 * Remove the trailing directory of the path. If the path is a symbolic link to a directory, remove
 * the symbolic link.
 *
 * @param[in] portLibrary The port library
 * @param[in] path directory name being removed.
 *
 * @return 0 on success, -1 on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behaviour is undefined
 *
 * @internal @todo return negative portable return code on failure..
 */
int32_t
omrfile_unlinkdir(struct OMRPortLibrary *portLibrary, const char *path)
{
	return -1;
}


/**
 * Write to a file.
 *
 * Writes up to nbytes from the provided buffer  to the file referenced by the file descriptor.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd File descriptor to write.
 * @param[in] buf Buffer to be written.
 * @param[in] nbytes Size of buffer.
 *
 * @return Number of bytes written on success, negative portable error code on failure.
 */
intptr_t
omrfile_write(struct OMRPortLibrary *portLibrary, intptr_t fd, const void *buf, intptr_t nbytes)
{
	intptr_t rc;

	/* write will just do the right thing for OMRPORT_TTY_OUT and OMRPORT_TTY_ERR */
	rc = write((int)fd, buf, nbytes);

	if (rc == -1) {
		return portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}

	return rc;
}

static int32_t
EsTranslateOpenFlags(int32_t flags)
{
	int32_t realFlags = 0;

	if (flags & EsOpenAppend) {
		realFlags |= O_APPEND;
	}
	if (flags & EsOpenTruncate) {
		realFlags |= O_TRUNC;
	}
	if (flags & EsOpenCreate) {
		realFlags |= O_CREAT;
	}
	if (flags & EsOpenCreateNew) {
		realFlags |= O_EXCL | O_CREAT;
	}
	if (flags & EsOpenSync) {
		realFlags |= O_SYNC;
	}
	if (flags & EsOpenRead) {
		if (flags & EsOpenWrite) {
			return (O_RDWR | realFlags);
		}
		return (O_RDONLY | realFlags);
	}
	if (flags & EsOpenWrite) {
		return (O_WRONLY | realFlags);
	}
	return -1;
}



/**
 * Write to a file.
 *
 * Writes formatted output  to the file referenced by the file descriptor.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd File descriptor to write.
 * @param[in] format The format String.
 * @param[in] args Variable argument list.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
omrfile_vprintf(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, va_list args)
{
	char localBuffer[256];
	char *writeBuffer = NULL;
	uintptr_t bufferSize = 0;
	uintptr_t stringSize = 0;

	/* str_vprintf(.., NULL, ..) result is size of buffer required including the null terminator */
	bufferSize = portLibrary->str_vprintf(portLibrary, NULL, 0, format, args);

	/* use local buffer if possible, allocate a buffer from system memory if local buffer not large enough */
	if (sizeof(localBuffer) >= bufferSize) {
		writeBuffer = localBuffer;
	} else {
		writeBuffer = portLibrary->mem_allocate_memory(portLibrary, bufferSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	}

	/* format and write out the buffer (truncate into local buffer as last resort) without null terminator */
	if (NULL != writeBuffer) {
		stringSize = portLibrary->str_vprintf(portLibrary, writeBuffer, bufferSize, format, args);
		portLibrary->file_write_text(portLibrary, fd, writeBuffer, stringSize);
		/* dispose of buffer if not on local */
		if (writeBuffer != localBuffer) {
			portLibrary->mem_free_memory(portLibrary, writeBuffer);
		}
	} else {
		portLibrary->nls_printf(portLibrary, J9NLS_ERROR, J9NLS_PORT_FILE_MEMORY_ALLOCATE_FAILURE);
		stringSize = portLibrary->str_vprintf(portLibrary, localBuffer, sizeof(localBuffer), format, args);
		portLibrary->file_write_text(portLibrary, fd, localBuffer, stringSize);
	}
}
/**
 * Write to a file.
 *
 * Writes formatted output  to the file referenced by the file descriptor.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd File descriptor to write to
 * @param[in] format The format string to be output.
 * @param[in] ... arguments for format.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
omrfile_printf(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, ...)
{
	va_list args;

	Trc_PRT_file_printf_Entry(fd, format);

	va_start(args, format);
	portLibrary->file_vprintf(portLibrary, fd, format, args);
	va_end(args);

	Trc_PRT_file_printf_Exit();
}
/**
 * Set the length of a file to a specified value.
 *
 * @param[in] portLibrary The port library
 * @param[in] fd The file descriptor.
 * @param[in] newLength Length to be set
 *
 * @return 0 on success, negative portable error code on failure
 */
int32_t
omrfile_set_length(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength)
{
	off_t length  = (off_t)newLength;
	int32_t result = 0;

	Trc_PRT_file_setlength_Entry(fd, newLength);

	/* If file offsets are 32 bit, truncate the newLength to that range */
	if (sizeof(off_t) < sizeof(int64_t)) {
		if (newLength > 0x7FFFFFFF) {
			length =  0x7FFFFFFF;
		} else if (newLength < -0x7FFFFFFF) {
			length = -0x7FFFFFFF;
		}
	}

	result =  ftruncate(fd, length);
	Trc_PRT_file_setlength_Exit(result);
	return result;
}
/**
 * This function will acquire a lock of the requested type on the given file, starting at offset bytes
 * from the start of the file and continuing for length bytes
 *
 * @param [in]   portLibrary            The port library
 * @param [in]   fd                              The file descriptor/handle of the file to be locked
 * @param [in]   lockFlags              Flags indicating the type of lock required and whether the call should block
 * @args                                               OMRPORT_FILE_READ_LOCK
 * @args                                               OMRPORT_FILE_WRITE_LOCK
 * @args                                               OMRPORT_FILE_WAIT_FOR_LOCK
 * @args                                               OMRPORT_FILE_NOWAIT_FOR_LOCK
 * @param [in]   offest                       Offset from start of file to start of locked region
 * @param [in]   length                      Number of bytes to be locked
 *
 * @return                                              0 on success, -1 on failure
 */
int32_t
omrfile_lock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length)
{
	return -1;
}
/**
 * This function will release the lock on the given file, starting at offset bytes
 * from the start of the file and continuing for length bytes
 *
 * @param [in]   portLibrary            The port library
 * @param [in]   fd                              The file descriptor/handle of the file to be locked
 * @param [in]   offest                       Offset from start of file to start of locked region
 * @param [in]   length                      Number of bytes to be unlocked
 *
 * @return                                              0 on success, -1 on failure
 */
int32_t
omrfile_unlock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length)
{
	return -1;
}

/**
 * Query the properties of the specified file using file descriptor
 *
 * @parm [in]    portLib	the port library
 * @parm [in]    fd			file descriptor of the file
 * @parm [out]   buf		J9FileStat struct to be populated
 *
 * @return 0 on success, -1 on failure
 *
 */
int32_t
omrfile_fstat(struct OMRPortLibrary *portLibrary, intptr_t fd, struct J9FileStat *buf)
{
	return -1;
}

/**
 * Query the properties of the specified file
 *
 * @parm [in]    portLib	the port library
 * @parm [in]    path		file/name being queried
 * @parm [in]    flags		flags; currently ignored
 * @parm [out]   buf		J9FileStat struct to be populated
 *
 * @return 0 on success, negative portable error code on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behavior is undefined
 */
int32_t
omrfile_stat(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStat *buf)
{
	return -1;
}

/**
 * Query the properties of the specified file system
 *
 * @parm [in]    portLib	the port library
 * @parm [in]    path		file/name on the file system being queried
 * @parm [in]    flags		flags; currently ignored
 * @parm [out]   buf		J9FileStatFilesystem struct to be populated
 *
 * @return 0 on success, negative portable error code on failure.
 *
 * @note If a multi-threaded application passes in a relative path the caller must synchronize on the current working directory, otherwise the behavior is undefined
 */
int32_t
omrfile_stat_filesystem(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStatFilesystem *buf)
{
	return -1;
}

/**
 * Convert a file descriptor, that was not obtained from the port library via omrfile_open(),
 * to one that can be used by the omrfile APIs
 *
 * @param[in] portLibrary 	The port library
 * @param[in] nativeFD		The file descriptor to be converted

 * @return A file descriptor that can be used by the port library omrfile APIs
 *
 */
intptr_t
omrfile_convert_native_fd_to_omrfile_fd(struct OMRPortLibrary *portLibrary, intptr_t nativeFD)
{
	return nativeFD;
}

/**
 * Convert a file descriptor that was obtained from the port library via omrfile_open(),
 * to one that can be used by the native APIs
 *
 * @param[in] portLibrary 	The port library
 * @param[in] omrfileFD		The file descriptor to be converted

 * @return A file descriptor that can be used by the native platform APIs
 *
 */
intptr_t
omrfile_convert_omrfile_fd_to_native_fd(struct OMRPortLibrary *portLibrary, intptr_t omrfileFD)
{
	return omrfileFD;
}
