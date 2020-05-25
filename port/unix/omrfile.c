/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief file
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#if defined(LINUX) && !defined(OMRZTPF)
#include <sys/vfs.h>
#elif defined(OSX)
#include <sys/param.h>
#include <sys/mount.h>
#endif /*  defined(LINUX)  && !defined(OMRZTPF) */
#if !defined(OMRZTPF)
#include <sys/statvfs.h>
#endif /* !defined(OMRZTPF) */
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include "omrport.h"
#include "omrportpriv.h"
#include <errno.h>
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"
#include <sys/stat.h>

#ifdef J9ZOS390
/* The following undef is to address CMVC 95221 */
#undef fwrite
#undef fread
#endif

#if defined(LINUX) || defined(OSX)
typedef struct statfs PlatformStatfs;
#elif defined(AIXPPC)
typedef struct statvfs PlatformStatfs;
#endif /* defined(LINUX) || defined(OSX) */


static const char *const fileFStatErrorMsgPrefix = "fstat : ";
static const char *const fileFStatFSErrorMsgPrefix = "fstatfs : ";
#if defined(AIXPPC) && !defined(J9OS_I5)
static const char *const fileFStatVFSErrorMsgPrefix = "fstatvfs : ";
#endif /* defined(AIXPPC) && !defined(J9OS_I5) */

static int32_t EsTranslateOpenFlags(int32_t flags);
static void setPortableError(OMRPortLibrary *portLibrary, const char *funcName, int32_t portlibErrno, int systemErrno);
static int32_t findError(int32_t errorCode);
#if (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || (defined(AIXPPC) && !defined(J9OS_I5))
static void updateJ9FileStat(struct OMRPortLibrary *portLibrary, J9FileStat *j9statBuf, struct stat *statBuf, PlatformStatfs *statfsBuf);
#else /* (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || (defined(AIXPPC) && !defined(J9OS_I5)) */
static void updateJ9FileStat(struct OMRPortLibrary *portLibrary, J9FileStat *j9statBuf, struct stat *statBuf);
#endif /* (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || (defined(AIXPPC) && !defined(J9OS_I5)) */

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
	if ((flags & EsOpenCreate) || (flags & EsOpenCreateAlways)) {
		realFlags |= O_CREAT;
	}
	if (flags & EsOpenCreateNew) {
		realFlags |= O_EXCL | O_CREAT;
	}
#ifdef O_SYNC
	if (flags & EsOpenSync) {
		realFlags |= O_SYNC;
	}
#endif
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

static void
setPortableError(OMRPortLibrary *portLibrary, const char *funcName, int32_t portlibErrno, int systemErrno)
{
	char *errmsgbuff = NULL;
	int32_t errmsglen = 0;
	int32_t portableErrno = portlibErrno + findError(systemErrno);

	/*Get size of str_printf buffer (it includes null terminator)*/
	errmsglen = portLibrary->str_printf(portLibrary, NULL, 0, "%s%s", funcName, strerror(systemErrno));
	if (errmsglen <= 0) {
		/*Set the error with no message*/
		portLibrary->error_set_last_error(portLibrary, systemErrno, portableErrno);
		return;
	}

	/*Alloc the buffer*/
	errmsgbuff = portLibrary->mem_allocate_memory(portLibrary, errmsglen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == errmsgbuff) {
		/*Set the error with no message*/
		portLibrary->error_set_last_error(portLibrary, systemErrno, portableErrno);
		return;
	}

	/*Fill the buffer using str_printf*/
	portLibrary->str_printf(portLibrary, errmsgbuff, errmsglen, "%s%s", funcName,	strerror(systemErrno));

	/*Set the error message*/
	portLibrary->error_set_last_error_with_message(portLibrary, portableErrno, errmsgbuff);

	/*Free the buffer*/
	portLibrary->mem_free_memory(portLibrary, errmsgbuff);

	return;
}


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
	case 0:
		return OMRPORT_ERROR_FILE_EOF;
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
 * Populate J9FileStat using system specific structures.
 *
 * @param[in] portLibrary The port library
 * @param[out] j9statBuf Pointer to J9FileStat to be populated
 * @param[in] statBuf Pointer to struct stat that contains system specific information about the file
 * @param[in] statfsBuf Pointer to PlatformStatfs that contains system specific information about mounted file system
 *
 * @return void
 */
#if (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || (defined(AIXPPC) && !defined(J9OS_I5))
static void
updateJ9FileStat(struct OMRPortLibrary *portLibrary, J9FileStat *j9statBuf, struct stat *statBuf, PlatformStatfs *statfsBuf)
#else /* (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || (defined(AIXPPC) && !defined(J9OS_I5)) */
static void
updateJ9FileStat(struct OMRPortLibrary *portLibrary, J9FileStat *j9statBuf, struct stat *statBuf)
#endif /* (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || (defined(AIXPPC) && !defined(J9OS_I5)) */
{
	if (S_ISDIR(statBuf->st_mode)) {
		j9statBuf->isDir = 1;
	} else {
		j9statBuf->isFile = 1;
	}

	if (statBuf->st_mode & S_IWUSR) {
		j9statBuf->perm.isUserWriteable = 1;
	}
	if (statBuf->st_mode & S_IRUSR) {
		j9statBuf->perm.isUserReadable = 1;
	}
	if (statBuf->st_mode & S_IWGRP) {
		j9statBuf->perm.isGroupWriteable = 1;
	}
	if (statBuf->st_mode & S_IRGRP) {
		j9statBuf->perm.isGroupReadable = 1;
	}
	if (statBuf->st_mode & S_IWOTH) {
		j9statBuf->perm.isOtherWriteable = 1;
	}
	if (statBuf->st_mode & S_IROTH) {
		j9statBuf->perm.isOtherReadable = 1;
	}

	j9statBuf->ownerUid = statBuf->st_uid;
	j9statBuf->ownerGid = statBuf->st_gid;

#if (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX)
	if (NULL != statfsBuf) {
		switch (statfsBuf->f_type) {
		/* Detect remote filesystem types */
		case 0x6969: /* NFS_SUPER_MAGIC */
		case 0x517B: /* SMB_SUPER_MAGIC */
		case 0xFF534D42: /* CIFS_MAGIC_NUMBER */
			j9statBuf->isRemote = 1;
			break;
		default:
			j9statBuf->isFixed = 1;
			break;
		}
	}
#elif defined(AIXPPC) && !defined(J9OS_I5) /* defined(LINUX) || defined(OSX) */
	if (NULL != statfsBuf) {
		/* Detect NFS filesystem */
		if (NULL != strstr(statfsBuf->f_basetype, "nfs")) {
			j9statBuf->isRemote = 1;
		} else {
			j9statBuf->isFixed = 1;
		}
	}
#else /* defined(AIXPPC) && !defined(J9OS_I5) */
	j9statBuf->isFixed = 1;
#endif /* defined(AIXPPC) && !defined(J9OS_I5) */

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
omrfile_close(struct OMRPortLibrary *portLibrary, intptr_t inFD)
{
	int32_t rc = 0;
	int fd = (int)inFD;

	Trc_PRT_file_close_Entry(fd);

#if (FD_BIAS != 0)
	if (fd < FD_BIAS) {
		/* Cannot close STD streams, and no other FD's should exist <FD_BIAS */
		Trc_PRT_file_close_ExitFail(-1);
		return -1;
	}
#endif

	rc = (int32_t)close(fd - FD_BIAS);

	Trc_PRT_file_close_Exit(rc);
	return rc;
}

intptr_t
omrfile_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	struct stat buffer;
	int32_t fd = 0;
	int32_t realFlags = EsTranslateOpenFlags(flags);
	int32_t fdflags;
	const char *errMsg;

	Trc_PRT_file_open_Entry(path, flags, mode);

	if (-1 == realFlags) {
		Trc_PRT_file_open_Exception1(path, flags);
		Trc_PRT_file_open_Exit(-1);
		portLibrary->error_set_last_error(portLibrary, EINVAL, findError(EINVAL));
		return -1;
	}

	/* The write open system call on a directory path returns different
	 * errors from platform to platform (e.g. AIX and zOS return FILE_EXIST
	 * while Linux returns FILE_ISDIR). As such, if the path is a directory,
	 * we always returns the FILE_ISDIR error even if the operation is not
	 * read only.
	 */
	if (0 == stat(path, &buffer)) {
		if (S_ISDIR(buffer.st_mode)) {
			Trc_PRT_file_open_Exception4(path);
			Trc_PRT_file_open_Exit(-1);
			errMsg = portLibrary->nls_lookup_message(portLibrary,
					 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
					 J9NLS_PORT_FILE_OPEN_FILE_IS_DIR,
					 NULL);
			portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_FILE_ISDIR, errMsg);
			return -1;
		}
	}

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
	/* On zOS use the non-tagging version of atoe_open for VM generated files that are always platform
	 * encoded (EBCDIC), e.g. javacore dumps. See CMVC 199888
	 */
	if (flags & EsOpenCreateNoTag) {
		fd = atoe_open_notag(path, realFlags, mode);
	} else {
		fd = open(path, realFlags, mode);
	}
#else /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */
	fd = open(path, realFlags, mode);
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

	if (-1 == fd) {
		Trc_PRT_file_open_Exception2(path, errno, findError(errno));
		Trc_PRT_file_open_Exit(-1);
		portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		return -1;
	}

	/* PR 95345 - Tag this descriptor as being non-inheritable */
	fdflags = fcntl(fd, F_GETFD, 0);
	fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC);

	fd += FD_BIAS;
	Trc_PRT_file_open_Exit(fd);
	return (intptr_t) fd;
}

int32_t
omrfile_unlink(struct OMRPortLibrary *portLibrary, const char *path)
{
	int32_t rc;

	rc = unlink(path);
	if (-1 == rc) {
		portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}
	return rc;
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
 * @return Number of bytes written on success, portable error return code (which is negative) on failure.
 */
intptr_t
omrfile_write(struct OMRPortLibrary *portLibrary, intptr_t inFD, const void *buf, intptr_t nbytes)
{
	int fd = (int)inFD;
	intptr_t rc;

	Trc_PRT_file_write_Entry(fd, buf, nbytes);

#ifdef AIXPPC
	/* Avoid hang on AIX when calling write() with 0 bytes on standard streams. */
	if ((0 == nbytes) && ((fd == OMRPORT_TTY_OUT) || (fd == OMRPORT_TTY_ERR))) {
		Trc_PRT_file_write_Exit(0);
		return 0;
	}
#endif

#ifdef J9ZOS390
	if (fd == OMRPORT_TTY_OUT) {
		rc = fwrite(buf, sizeof(char), nbytes, stdout);
	} else if (fd == OMRPORT_TTY_ERR) {
		rc = fwrite(buf, sizeof(char), nbytes, stderr);
	} else if (fd < FD_BIAS) {
		/* Cannot fsync STDIN, and no other FD's should exist <FD_BIAS */
		Trc_PRT_file_write_Exit(-1);
		return -1;
	} else
#elif (FD_BIAS != 0)
#error FD_BIAS must be 0
#endif
	{
		/* CMVC 178203 - Restart system calls interrupted by EINTR */
		do {
			/* write will just do the right thing for OMRPORT_TTY_OUT and OMRPORT_TTY_ERR */
			rc = write(fd - FD_BIAS, buf, nbytes);
		} while ((-1 == rc) && (EINTR == errno));

	}

	if (rc == -1) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}

	Trc_PRT_file_write_Exit(rc);
	return rc;
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
omrfile_read(struct OMRPortLibrary *portLibrary, intptr_t inFD, void *buf, intptr_t nbytes)
{
	int fd = (int)inFD;
	intptr_t result;

	Trc_PRT_file_read_Entry(fd, buf, nbytes);

	if (nbytes == 0) {
		Trc_PRT_file_read_Exit(0);
		return 0;
	}

#ifdef J9ZOS390
	if (fd == OMRPORT_TTY_IN) {
		result = fread(buf, sizeof(char), nbytes, stdin);
	}  else	if (fd < FD_BIAS) {
		/* Cannot read from STDOUT/ERR, and no other FD's should exist <FD_BIAS */
		Trc_PRT_file_read_Exit(-1);
		return -1;
	} else
#elif (FD_BIAS != 0)
#error FD_BIAS must be 0
#endif
	{
		/* in case of EINTR try again */
		do {
			result = read(fd - FD_BIAS, buf, nbytes);
		} while ((-1 == result) && (EINTR == errno));
	}

	if ((0 == result) || (-1 == result)) {
		if (-1 == result) {
			portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		} else {
			/*
			 *  number of bytes read is zero and no error occur - should be EOF
			 *  use error code 0 in findError()
			 */
			portLibrary->error_set_last_error(portLibrary, 0, findError(0));
			result = -1;
		}
	}

	Trc_PRT_file_read_Exit(result);
	return result;

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
omrfile_seek(struct OMRPortLibrary *portLibrary, intptr_t inFD, int64_t offset, int32_t whence)
{
	int fd = (int)inFD;
	off_t localOffset = (off_t)offset;
	int64_t result;

	Trc_PRT_file_seek_Entry(fd, offset, whence);

	if ((whence < EsSeekSet) || (whence > EsSeekEnd)) {
		int32_t errorCode = portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_FILE_INVAL);
		Trc_PRT_file_seek_Exit(errorCode);
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

#if (FD_BIAS != 0)
	if (fd < FD_BIAS) {
		/* Cannot seek on STD streams, and no other FD's should exist <FD_BIAS */
		int32_t errorCode = portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_FILE_BADF);
		Trc_PRT_file_seek_Exit(errorCode);
		return -1;
	}
#endif

	result = (int64_t)lseek(fd - FD_BIAS, localOffset, whence);
	if (-1 == result) {
		portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}
	Trc_PRT_file_seek_Exit(result);
	return result;
}

int32_t
omrfile_attr(struct OMRPortLibrary *portLibrary, const char *path)
{
	struct stat buffer;

	Trc_PRT_file_attr_Entry(path);

	/* Neutrino does not handle NULL for stat */
	if (stat(path, &buffer)) {
		int32_t setError = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		Trc_PRT_file_attr_ExitFail(setError);
		return setError;
	}
	if (S_ISDIR(buffer.st_mode)) {
		Trc_PRT_file_attr_ExitDir(EsIsDir);
		return EsIsDir;
	}
	Trc_PRT_file_attr_ExitFile(EsIsFile);
	return EsIsFile;
}

int32_t
omrfile_chmod(struct OMRPortLibrary *portLibrary, const char *path, int32_t mode)
{
	int32_t result;
	intptr_t actualMode = -1;
	struct stat buffer;
	const int32_t MODEBITS = 07777; /* bits from  S_ISUID down: lstat can return mode with the high-order bits set. */

	Trc_PRT_file_chmod_Entry(path, mode);
	result = chmod(path, mode);
	if (0 != result) {
		result = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		Trc_PRT_file_chmod_setAttributesFailed(result);
	} else if (0 != lstat(path, &buffer)) {
		result = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		Trc_PRT_file_chmod_getAttributesFailed(result);
	} else {
		actualMode = buffer.st_mode & MODEBITS;
	}
	Trc_PRT_file_chmod_Exit(actualMode);
	return actualMode;
}

intptr_t
omrfile_chown(struct OMRPortLibrary *portLibrary, const char *path, uintptr_t owner, uintptr_t group)
{
	int rc;
	Trc_PRT_file_chown_entry(path, owner, group);
#if defined(J9ZOS390)
	if ((OMRPORT_FILE_IGNORE_ID == owner) || (OMRPORT_FILE_IGNORE_ID == group)) {
		struct stat st;
		if (0 != stat(path, &st)) {
			return -1;
		}
		owner = (OMRPORT_FILE_IGNORE_ID == owner)? st.st_uid : owner;
		group = (OMRPORT_FILE_IGNORE_ID == group)? st.st_gid : group;
	}
#endif
	rc = chown(path, owner, group);
	if (0 != rc) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}
	Trc_PRT_file_chown_exit(path, owner, group, rc);
	return rc;
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
#if defined(AIXPPC)
	closedir64((DIR64 *)findhandle);
#else
	closedir((DIR *)findhandle);
#endif
}

uintptr_t
omrfile_findfirst(struct OMRPortLibrary *portLibrary, const char *path, char *resultbuf)
{
#if defined(AIXPPC)
	DIR64 *dirp = NULL;
#else
	DIR *dirp = NULL;
#endif

#if defined(AIXPPC)
	struct dirent64 *entry;
#else
	struct dirent *entry;
#endif

	Trc_PRT_file_findfirst_Entry2(path, resultbuf);

#if defined(AIXPPC)
	dirp = opendir64(path);
#else
	dirp = opendir(path);
#endif
	if (dirp == NULL) {
		Trc_PRT_file_findfirst_ExitFail(-1);
		return (uintptr_t)-1;
	}
#if defined(AIXPPC)
	entry = readdir64(dirp);
#else
	entry = readdir(dirp);
#endif
	if (entry == NULL) {
#if defined(AIXPPC)
		closedir64(dirp);
#else
		closedir(dirp);
#endif
		Trc_PRT_file_findfirst_ExitFail(-1);
		return (uintptr_t)-1;
	}
	strcpy(resultbuf, entry->d_name);
	Trc_PRT_file_findfirst_Exit((uintptr_t)dirp);
	return (uintptr_t)dirp;
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
#if defined(AIXPPC)
	struct dirent64 *entry;
#else
	struct dirent *entry;
#endif

	Trc_PRT_file_findnext_Entry2(findhandle, resultbuf);

#if defined(AIXPPC)
	entry = readdir64((DIR64 *)findhandle);
#else
	entry = readdir((DIR *)findhandle);
#endif
	if (entry == NULL) {
		Trc_PRT_file_findnext_ExitFail(-1);
		return -1;
	}
	strcpy(resultbuf, entry->d_name);
	Trc_PRT_file_findnext_Exit(0);
	return 0;
}

int64_t
omrfile_lastmod(struct OMRPortLibrary *portLibrary, const char *path)
{
	struct stat st;

	Trc_PRT_file_lastmod_Entry(path);

	tzset();

	/* Neutrino does not handle NULL for stat */
	if (0 != stat(path, &st)) {
		Trc_PRT_file_lastmod_Exit(-1);
		return -1;
	}
	Trc_PRT_file_lastmod_Exit(st.st_mtime);
	return st.st_mtime;
}

int64_t
omrfile_length(struct OMRPortLibrary *portLibrary, const char *path)
{
	struct stat st;

	Trc_PRT_file_length_Entry(path);

	/* Neutrino does not handle NULL for stat */
	if (0 != stat(path, &st)) {
		int32_t errorCode = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		Trc_PRT_file_length_Exit(errorCode);
		return errorCode;
	}
	Trc_PRT_file_length_Exit((int64_t)st.st_size);
	return (int64_t) st.st_size;
}

int64_t
omrfile_flength(struct OMRPortLibrary *portLibrary, intptr_t inFD)
{
	struct stat st;
	int fd = (int)inFD;

	Trc_PRT_file_flength_Entry(inFD);

	if (0 != fstat(fd - FD_BIAS, &st)) {
		int32_t errorCode = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
		Trc_PRT_file_flength_ExitFail(errorCode);
		return errorCode;
	}

	Trc_PRT_file_flength_Exit((int64_t) st.st_size);
	return (int64_t) st.st_size;
}

int32_t
omrfile_mkdir(struct OMRPortLibrary *portLibrary, const char *path)
{
	int32_t rc = 0;

	Trc_PRT_file_mkdir_entry2(path);
	if (-1 == mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO)) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}
	Trc_PRT_file_mkdir_exit2(rc);
	return rc;
}

int32_t
omrfile_move(struct OMRPortLibrary *portLibrary, const char *pathExist, const char *pathNew)
{
#ifndef OMRZTPF
	return rename(pathExist, pathNew);
#else
	return rename(pathExist, (char *)pathNew);
#endif
}

int32_t
omrfile_unlinkdir(struct OMRPortLibrary *portLibrary, const char *path)
{
	/*[PR 111252] Call remove() so symbolic links to directories are removed */

	/* QNX has modified the API for remove and rmdir.*/
	/* Remove does not call rmdir automagically like every other Unix.*/
#ifndef OMRZTPF
	return remove(path);
#else
	return rmdir(path);
#endif
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
omrfile_sync(struct OMRPortLibrary *portLibrary, intptr_t inFD)
{
	int fd = (int)inFD;
	int32_t result = 0;

	Trc_PRT_file_sync_Entry(inFD);

#ifdef J9ZOS390
	if (fd == OMRPORT_TTY_OUT) {
		result =  fflush(stdout);
		Trc_PRT_file_sync_Exit(result);
		return result;
	} else if (fd == OMRPORT_TTY_ERR) {
		result =  fflush(stderr);
		Trc_PRT_file_sync_Exit(result);
		return result;
	} else if (fd < FD_BIAS) {
		Trc_PRT_file_sync_Exit(-1);
		/* Cannot fsync STDIN, and no other FD's should exist <FD_BIAS */
		return -1;
	}
#elif (FD_BIAS != 0)
#error FD_BIAS must be 0
#endif

	result = fsync(fd - FD_BIAS);
	Trc_PRT_file_sync_Exit(result);
	return result;
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
	portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	return portLibrary->error_last_error_message(portLibrary);
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
omrfile_set_length(struct OMRPortLibrary *portLibrary, intptr_t inFD, int64_t newLength)
{
	int fd = (int)inFD;
	int32_t rc;
	off_t length  = (off_t)newLength;

	Trc_PRT_file_setlength_Entry(inFD, newLength);

	/* If file offsets are 32 bit, truncate the newLength to that range */
	if (sizeof(off_t) < sizeof(int64_t)) {
		if (newLength > 0x7FFFFFFF) {
			length =  0x7FFFFFFF;
		} else if (newLength < -0x7FFFFFFF) {
			length = -0x7FFFFFFF;
		}
	}

#if (FD_BIAS != 0)
	if (fd < FD_BIAS) {
		/* Cannot ftruncate on STD streams, and no other FD's should exist <FD_BIAS */
		Trc_PRT_file_setlength_Exit(-1);
		return -1;
	}
#endif

	rc = ftruncate(fd - FD_BIAS, length);
	if (0 != rc) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}
	Trc_PRT_file_setlength_Exit(rc);
	return rc;
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
	int fcntlFd = (int)fd - FD_BIAS;
	int fcntlCmd = 0;
	struct flock fcntlFlock;
	int rc = 0;
	const char *errMsg;
	char errBuf[512];

	Trc_PRT_file_lock_bytes_unix_entered(fd, lockFlags, offset, length);
	if (!(lockFlags & OMRPORT_FILE_READ_LOCK) && !(lockFlags & OMRPORT_FILE_WRITE_LOCK)) {
		Trc_PRT_file_lock_bytes_unix_failed_noReadWrite();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_FILE_LOCK_INVALID_FLAG,
				 NULL);
		portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errMsg, lockFlags);
		portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_FILE_LOCK_NOREADWRITE, errBuf);
		Trc_PRT_file_lock_bytes_unix_exiting();
		return -1;
	}
	if (!(lockFlags & OMRPORT_FILE_WAIT_FOR_LOCK) && !(lockFlags & OMRPORT_FILE_NOWAIT_FOR_LOCK)) {
		Trc_PRT_file_lock_bytes_unix_failed_noWaitNoWait();
		errMsg = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_FILE_LOCK_INVALID_FLAG,
				 NULL);
		portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errMsg, lockFlags);
		portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_FILE_LOCK_NOWAITNOWAIT, errBuf);
		Trc_PRT_file_lock_bytes_unix_exiting();
		return -1;
	}
	if (lockFlags & OMRPORT_FILE_WAIT_FOR_LOCK) {
		fcntlCmd = F_SETLKW;
	} else {
		fcntlCmd = F_SETLK;
	}
	memset(&fcntlFlock, '\0', sizeof(struct flock));
	if (lockFlags & OMRPORT_FILE_WRITE_LOCK) {
		fcntlFlock.l_type = F_WRLCK;
	} else {
		fcntlFlock.l_type = F_RDLCK;
	}
	fcntlFlock.l_whence = SEEK_SET;
	fcntlFlock.l_start = offset;
	fcntlFlock.l_len = length;
	Trc_PRT_file_lock_bytes_unix_callingFcntl(fcntlCmd, fcntlFlock.l_type, fcntlFlock.l_whence, fcntlFlock.l_start, fcntlFlock.l_len);
	rc = fcntl(fcntlFd, fcntlCmd, &fcntlFlock);
	if (rc == -1) {
		Trc_PRT_file_lock_bytes_unix_failed_badFcntl(errno);
		if (errno == EDEADLK) {
			portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_FILE_LOCK_EDEADLK);
		} else {
			portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_FILE_LOCK_BADLOCK);
		}
		Trc_PRT_file_lock_bytes_unix_exiting();
		return -1;
	}
	Trc_PRT_file_lock_bytes_unix_exiting();
	return 0;
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
	int fcntlFd = (int)fd - FD_BIAS;
	int fcntlCmd = 0;
	struct flock fcntlFlock;
	int rc = 0;
	Trc_PRT_file_unlock_bytes_unix_entered(fd, offset, length);
	fcntlCmd = F_SETLK;
	memset(&fcntlFlock, '\0', sizeof(struct flock));
	fcntlFlock.l_type = F_UNLCK;
	fcntlFlock.l_whence = SEEK_SET;
	fcntlFlock.l_start = offset;
	fcntlFlock.l_len = length;
	Trc_PRT_file_unlock_bytes_unix_callingFcntl(fcntlCmd, fcntlFlock.l_type, fcntlFlock.l_whence, fcntlFlock.l_start, fcntlFlock.l_len);
	rc = fcntl(fcntlFd, fcntlCmd, &fcntlFlock);
	if (rc == -1) {
		Trc_PRT_file_unlock_bytes_unix_failed_badFcntl(errno);
		portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_FILE_UNLOCK_BADUNLOCK);
		Trc_PRT_file_unlock_bytes_unix_exiting();
		return -1;
	}
	Trc_PRT_file_unlock_bytes_unix_exiting();
	return 0;
}

int32_t
omrfile_fstat(struct OMRPortLibrary *portLibrary, intptr_t fd, struct J9FileStat *buf)
{
	struct stat statbuf;
#if (!defined(OMRZTPF) && defined(LINUX)) || defined(AIXPPC) || defined(OSX)
	PlatformStatfs statfsbuf;
#endif /* (!defined(OMRZTPF) && defined(LINUX)) || defined(AIXPPC) || defined(OSX) */
	int32_t rc = 0;
	int localfd = (int)fd;

	Trc_PRT_file_fstat_Entry(fd);

	portLibrary->error_set_last_error(portLibrary, 0, 0);

	memset(buf, 0, sizeof(J9FileStat));

	if (0 != fstat(localfd - FD_BIAS, &statbuf)) {
		intptr_t myerror = errno;
		Trc_PRT_file_fstat_fstatFailed(myerror);
		setPortableError(portLibrary, fileFStatErrorMsgPrefix, OMRPORT_ERROR_FILE_FSTAT_ERROR, myerror);
		rc = -1;
		goto _end;
	}

#if (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX)
	if (0 != fstatfs(localfd - FD_BIAS, &statfsbuf)) {
		intptr_t myerror = errno;
		Trc_PRT_file_fstat_fstatfsFailed(myerror);
		setPortableError(portLibrary, fileFStatFSErrorMsgPrefix, OMRPORT_ERROR_FILE_FSTATFS_ERROR, myerror);
		rc = -1;
		goto _end;
	}
	updateJ9FileStat(portLibrary, buf, &statbuf, &statfsbuf);
#elif defined(AIXPPC) && !defined(J9OS_I5) /* defined(LINUX) || defined(OSX) */
	if (0 != fstatvfs(localfd - FD_BIAS, &statfsbuf)) {
		intptr_t myerror = errno;
		Trc_PRT_file_fstat_fstatvfsFailed(myerror);
		setPortableError(portLibrary, fileFStatVFSErrorMsgPrefix, OMRPORT_ERROR_FILE_FSTATVFS_ERROR, myerror);
		rc = -1;
		goto _end;
	}
	updateJ9FileStat(portLibrary, buf, &statbuf, &statfsbuf);
#else /* defined(AIXPPC) && !defined(J9OS_I5) */
	updateJ9FileStat(portLibrary, buf, &statbuf);
#endif /* defined(LINUX) || defined(OSX) */

_end:
	Trc_PRT_file_fstat_Exit(rc);
	return rc;

}

int32_t
omrfile_stat(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStat *buf)
{
	struct stat statbuf;
#if (defined(LINUX) && !defined(OMRZTPF)) || defined(AIXPPC) || defined(OSX)
	PlatformStatfs statfsbuf;
#endif /* defined(LINUX) || defined(AIXPPC) || defined(OSX) */

	memset(buf, 0, sizeof(J9FileStat));

	/* Neutrino does not handle NULL for stat */
	if (stat(path, &statbuf)) {
		return portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}

#if (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX)
	if (statfs(path, &statfsbuf)) {
		return portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}
	updateJ9FileStat(portLibrary, buf, &statbuf, &statfsbuf);
#elif defined(AIXPPC) && !defined(J9OS_I5) /* defined(LINUX) || defined(OSX) */
	if (statvfs(path, &statfsbuf)) {
		return portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}
	updateJ9FileStat(portLibrary, buf, &statbuf, &statfsbuf);
#else /* defined(AIXPPC) && !defined(J9OS_I5) */
	updateJ9FileStat(portLibrary, buf, &statbuf);
#endif /* defined(LINUX) || defined(OSX) */

	return 0;
}


int32_t
omrfile_stat_filesystem(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStatFilesystem *buf)
{
#ifndef OMRZTPF
	struct statvfs statvfsbuf;

	if (statvfs(path, &statvfsbuf)) {
		return portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	}

	buf->totalSizeBytes = (uint64_t)statvfsbuf.f_bsize * (uint64_t)statvfsbuf.f_blocks;
	if (0 == omrsysinfo_get_euid(portLibrary)) {
		/* superuser */
		buf->freeSizeBytes = (uint64_t)statvfsbuf.f_bsize * (uint64_t)statvfsbuf.f_bfree;
	} else {
		/* non-superuser */
		buf->freeSizeBytes = (uint64_t)statvfsbuf.f_bsize * (uint64_t)statvfsbuf.f_bavail;
	}

	return 0;
#else
    return -4;
#endif
}

intptr_t
omrfile_convert_native_fd_to_omrfile_fd(struct OMRPortLibrary *portLibrary, intptr_t nativeFD)
{

#if (FD_BIAS != 0)
	/* Do NOT add the FD_BIAS to standard streams */
	switch (nativeFD) {
	case OMRPORT_TTY_IN:
		/* FALLTHROUGH */
	case OMRPORT_TTY_OUT:
		/* FALLTHROUGH */
	case OMRPORT_TTY_ERR:
		break;
	default:
		nativeFD = nativeFD + FD_BIAS;
		break;
	}
#endif /* (FD_BIAS != 0) */

	return nativeFD;
}

intptr_t
omrfile_convert_omrfile_fd_to_native_fd(struct OMRPortLibrary *portLibrary, intptr_t omrfileFD)
{

#if (FD_BIAS != 0)
	if (omrfileFD >= FD_BIAS) {
		omrfileFD = omrfileFD - FD_BIAS;
	}
#endif /* (FD_BIAS != 0) */

	return omrfileFD;
}
