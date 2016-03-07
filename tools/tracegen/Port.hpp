/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#ifndef PORT_HPP_
#define PORT_HPP_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#if defined(J9ZOS390)
#include <strings.h>
#endif

#if defined(WIN32)
#include <windows.h>
#include <sys/utime.h>
#else /* defined(WIN32) */
#include <limits.h>
#include <unistd.h>
#endif /* defined(WIN32) */

#if defined(WIN32)
#define strdup _strdup
#define stat _stat
#define snprintf _snprintf
#define PATH_SEP "\\"
#define dirstat struct _stat
#define PORT_INVALID_FIND_FILE_HANDLE ((intptr_t) INVALID_HANDLE_VALUE)
#define EsMaxPath 	MAX_PATH
#define OS_ENCODING_CODE_PAGE CP_UTF8
#define OS_ENCODING_MB_FLAGS 0
#define OS_ENCODING_WC_FLAGS 0
#define UNICODE_BUFFER_SIZE EsMaxPath
#else /* defined(WIN32) */
#define PATH_SEP  "/"
#define dirstat struct stat
#define PORT_INVALID_FIND_FILE_HANDLE ((intptr_t) 0)
#if defined(PATH_MAX)
#define EsMaxPath 	PATH_MAX
#else /* defined(PATH_MAX) */
/* EsMaxPath was chosen from unix MAXPATHLEN. */
#define EsMaxPath 	1024
#endif /* defined(PATH_MAX) */
#endif /* defined(WIN32) */

typedef enum RCType {
	RC_OK 		= 0,
	RC_FAILED	= -1,
	RCType_EnsureWideEnum = 0x1000000 /* force 4-byte enum */
} RCType;

typedef struct J9Permission {
	unsigned int isUserWriteable : 1;
	unsigned int isUserReadable : 1;
	unsigned int isGroupWriteable : 1;
	unsigned int isGroupReadable : 1;
	unsigned int isOtherWriteable : 1;
	unsigned int isOtherReadable : 1;
	unsigned int : 26; /* future use */
} J9Permission;

/**
 * Holds properties relating to a file. Can be added to in the future
 * if needed.
 * Note that the ownerUid and ownerGid fields are 0 on Windows.
 */
typedef struct J9FileStat {
	unsigned int isFile : 1;
	unsigned int isDir : 1;
	unsigned int isFixed : 1;
	unsigned int isRemote : 1;
	unsigned int isRemovable : 1;
	unsigned int : 27; /* future use */
	J9Permission perm;
	int ownerUid;
	int ownerGid;
} J9FileStat;

class Port
{
	/*
	 * Data members
	 */
private:
protected:
public:

	/*
	 * Function members
	 */
private:
protected:
public:

#if defined(WIN32)
	/**
	 * Uses convertFromUTF8 and if the resulting unicode path is longer than the Windows defined MAX_PATH,
	 * the path is converted to an absolute path and prepended with //?/
	 *
	 * @note	Relative paths are not thread-safe as the current directory is a process global that may change.
	 *
	 * @param[in] 	utf8Path			the string to be converted
	 * @param[out]	unicodeBuffer		buffer to hold the unicode representation of @ref utf8Path
	 * @param[in]	unicodeBufferSize	size of @ref unicodeBuffer.
	 *
	 * @return 		NULL on failure
	 * 			 	otherwise the converted string.
	 * 					If the return value is not the same as @ref unicodeBuffer, file_get_unicode_path had to allocate memory to hold the
	 * 					unicode path - the caller is responsible for freeing this memory by calling @ref omrmem_free on the returned value.
	 */
	static wchar_t *file_get_unicode_path(const char *utf8Path, wchar_t *unicodeBuffer, unsigned int unicodeBufferSize);

	/**
	 * @internal
	 * Converts the UTF8 encoded string to Unicode. Use the provided buffer if possible
	 * or allocate memory as necessary. Return the new string.
	 *
	 * @param[in] portLibrary The port library
	 * @param[in] string The UTF8 encoded string
	 * @param[in] unicodeBuffer The unicode buffer to attempt to use
	 * @param[in] unicodeBufferSize The number of bytes available in the unicode buffer
	 *
	 * @return	the pointer to the Unicode string, or NULL on failure.
	 *
	 * If the returned buffer is not the same as @ref unicodeBuffer, the returned buffer needs to be freed
	 * 	using @ref omrmem_free_memory
	 */
	static wchar_t *convertFromUTF8(const char *string, wchar_t *unicodeBuffer, unsigned int unicodeBufferSize);

	/**
	 * @internal
	 * Converts the Unicode string to UTF8 encoded data.
	 *
	 * @param[in] unicodeString The unicode buffer to convert
	 *  - unicodeString has to be NUL terminated.
	 * @param[in] utf8Buffer Address of a buffer to store the UTF8 encoded bytes into.
	 * Buffer of appropriate size is allocated to hold UTF8 encoded string.
	 * - utf8 string will be NUL terminated.
	 * - the caller is responsible for freeing this memory by calling @ref omrmem_free on the returned value.
	 * - in case of failure buffer is NULL'ed.
	 * @return RC_OK on success, RC_FAILED on failure.
	 */
	static RCType convertToUTF8(const wchar_t *unicodeString, char **utf8Buffer);
#endif /* #if defined(WIN32) */

	static RCType omrfile_findfirst(const char *path, char **resultbuf, intptr_t *handle);

	/**
	 * Find the next filename and path matching a given handle.
	 *
	 * @param[in] portLibrary The port library
	 * @param[in] findhandle handle returned from @ref omrfile_findfirst.
	 * @param[out] resultbuf next filename and path matching findhandle.
	 *
	 * @return RC_OK on success, RC_FAILED on failure or if no matching entries.
	 * @internal @todo return negative portable return code on failure.
	 */
	static RCType omrfile_findnext(intptr_t findhandle, char **resultbuf);

	/**
	 * Close the handle returned from @ref omrfile_findfirst.
	 *
	 * @param[in] portLibrary The port library
	 * @param[in] findhandle  Handle returned from @ref omrfile_findfirst.
	 */
	static RCType omrfile_findclose(intptr_t findhandle);

	static FILE *fopen(const char *path, const char *mode);
	static RCType omrfile_stat(const char *path, unsigned int flags, struct J9FileStat *buf);

	static RCType stat(const char *path, dirstat *statbuf);

	static char *omrfile_getcwd();
	static RCType omrfile_move(const char *pathExist, const char *pathNew);

	/**
	 * Resolve full path to the file. The caller should deallocate this buffer using omrmem_free.
	 * @param path NUL terminated path
	 * @return Canonical absolute path
	 */
	static char *omrfile_realpath(const char *path);

	/**
	 * compare two strings ignoring case
	 * @param s1 null terminated string
	 * @param s2 null terminated string
	 * @param n compare first n characters of s1.
	 * @see strncasecmp
	 * @return negative value, zero or positive value if s1 is less then, equal to or greater than s2.
	 */
	static int strncasecmp(const char *s1, const char *s2, size_t n);

	static inline void *omrmem_malloc(size_t size)
	{
		return malloc(size);
	}

	static inline void *omrmem_calloc(size_t nmemb, size_t size)
	{
		return calloc(nmemb, size);
	}

	static inline void omrmem_free(void **ptr)
	{
		if (NULL != ptr && NULL != *ptr) {
			free(*ptr);
			*ptr = NULL;
		}
	}
};

#endif /* PORT_HPP_ */
