/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief shared library
 */


/*
 *	Standard unix shared libraries
 *	(AIX: 4.2 and higher only)
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ldr.h>
#include <load.h>
#include <dlfcn.h>
#include "ut_omrport.h"

/* Start copy from omrfiletext.c */
/* __STDC_ISO_10646__ indicates that the platform wchar_t encoding is Unicode */
/* but older versions of libc fail to set the flag, even though they are Unicode */
#if defined(__STDC_ISO_10646__)
#define J9VM_USE_MBTOWC
#else
#include "omriconvhelpers.h"
#endif


#if defined(J9VM_USE_MBTOWC) || defined(J9VM_USE_ICONV)
#include <nl_types.h>
#include <langinfo.h>

/* Some older platforms (Netwinder) don't declare CODESET */
#ifndef CODESET
#define CODESET _NL_CTYPE_CODESET_NAME
#endif
#endif

/* End copy */

#include "omrport.h"
#include "portnls.h"

#if defined(J9OS_I5)
#include "Xj9I5OSInterface.H"
#endif

#define PLATFORM_DLL_EXTENSION ".so"

#if (defined(J9VM_USE_MBTOWC)) /* priv. proto (autogen) */

static void convertWithMBTOWC(struct OMRPortLibrary *portLibrary, char *error, char *errBuf, uintptr_t bufLen);
#endif /* J9VM_USE_MBTOWC (autogen) */



#if (defined(J9VM_USE_ICONV)) /* priv. proto (autogen) */

static void convertWithIConv(struct OMRPortLibrary *portLibrary, const char *error, char *errBuf, uintptr_t bufLen);
#endif /* J9VM_USE_ICONV (autogen) */



static void getDLError(struct OMRPortLibrary *portLibrary, char *errBuf, uintptr_t bufLen);
static uintptr_t getDirectoryOfLibrary(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen);

/**
 * Returns if the path specified is a file or a folder.
 * This function has the same logic as omrfile_attr(). However, this function does not
 * overwrite the portlib err buffer as omrfile_attr() does.
 *
 * @param[in] portLibrary 	- The port library
 * @param[in] path 			- path to validate
 *
 * @return 	-1 			stat returns error on path
 * 			EsIsDIR 	path is a folder
 * 			EsIsFile	path is a file
 */
static int32_t VMINLINE
isFile(struct OMRPortLibrary *portLibrary, const char *path)
{
	struct stat buffer;

	if (0 != stat(path, &buffer)) {
		return -1;
	}
	if (S_ISDIR(buffer.st_mode)) {
		return EsIsDir;
	}
	return EsIsFile;
}

/**
 * Searches for a given file in the paths listed in Env variable LIBPATH
 * This function does not use any portlib file API to avoid accidentally
 * overwritting the portlib error buffer.
 *
 * @parma[in] fileName - the file to be searched
 * @param[in] portLibrary - The port library

 * @return 1 if file is found, 0 otherwise
 */
static uintptr_t
isFileinLibPath(struct OMRPortLibrary *portLibrary, char *fileName)
{
	/*
	 * Because redirector adds onto LIBPATH, fetching
	 * LIBPATH via getenv is more accurate than fetching LIBPATH from loadquery using L_GETLIBATH flag.
	 */
	char *envLibPath = getenv("LIBPATH");
	uintptr_t envLibPathLen = 0;
	uintptr_t fileFound = 0;
	uintptr_t fileLen = 0;
	char portLibDir[EsMaxPath];
	char *path = NULL;
	char *libPath = NULL;
	char *strTokPtr = NULL;

	if ((NULL == envLibPath) || (NULL == fileName)) {
		return 0;
	}

	envLibPathLen = strlen(envLibPath) + 1;
	fileLen = strlen(fileName) + 1;

	libPath = portLibrary->mem_allocate_memory(portLibrary, envLibPathLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL == libPath) {
		return 0;
	}

	memcpy(libPath, envLibPath, envLibPathLen);

	path = strtok_r(libPath, ":", &strTokPtr);
	while (NULL != path) {
		uintptr_t totalPathLen = strlen(path) + fileLen + sizeof("/");
		char *tmpPath = NULL;
		char *pathBuf = NULL;

		if (totalPathLen >= EsMaxPath) {
			tmpPath = portLibrary->mem_allocate_memory(portLibrary, totalPathLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == tmpPath) {
				/* move on to next path in LIBPATH if we can't allocate enough memory to inspect current path */
				continue;
			}
			pathBuf = tmpPath;
		} else {
			pathBuf = portLibDir;
		}

		portLibrary->str_printf(portLibrary, pathBuf, totalPathLen, "%s/%s", path, fileName);

		if (EsIsFile == isFile(portLibrary, pathBuf)) {
			fileFound = 1;
			portLibrary->mem_free_memory(portLibrary, tmpPath);
			break;
		}
		portLibrary->mem_free_memory(portLibrary, tmpPath);
		path = strtok_r(NULL, ":", &strTokPtr);
	}

	portLibrary->mem_free_memory(portLibrary, libPath);
	return fileFound;
}

/* This macro will seek out whether specified path is indeed a file in current folder or in any folders specified by LIBPATH */
#define IS_FILE_VISIBLE(portlib,path) ((EsIsFile == isFile(portLibrary, path)) || (1 == isFileinLibPath(portLibrary, path)))

/**
 * Close a shared library.
 *
 * @param[in] portLibrary The port library.
 * @param[in] descriptor Shared library handle to close.
 *
 * @return 0 on success, any other value on failure.
 */
uintptr_t omrsl_close_shared_library(struct OMRPortLibrary *portLibrary, uintptr_t descriptor)
{
	uintptr_t result = 0;

	Trc_PRT_sl_close_shared_library_Entry(descriptor);
#if defined(J9OS_I5)
	result = (uintptr_t) Xj9dlclose((void *)descriptor);
#else
	result = (uintptr_t) dlclose((void *)descriptor);
#endif

	Trc_PRT_sl_close_shared_library_Exit(result);
	return result;
}

uintptr_t
omrsl_open_shared_library(struct OMRPortLibrary *portLibrary, char *name, uintptr_t *descriptor, uintptr_t flags)
{
	void *handle = NULL;
	char *openName = name;
	char *fileName = strrchr(name, '/');
	char mangledName[EsMaxPath + 1];
	char errBuf[512];
	uintptr_t result;
	int lazyOrNow = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_LAZY) ? RTLD_LAZY : RTLD_NOW;
	BOOLEAN decorate = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_DECORATE);
	uintptr_t lastErrno = 0;
	BOOLEAN openExec = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_OPEN_EXECUTABLE);
	uintptr_t pathLength = 0;

	Trc_PRT_sl_open_shared_library_Entry(name, flags);

	errBuf[0] = '\0';

	/* No need to name mangle if a handle to the executable is requested for. */
	if (!openExec && decorate) {
		if (NULL != fileName) {
			/* the names specifies a path */
			pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "%.*slib%s" PLATFORM_DLL_EXTENSION, (uintptr_t)fileName + 1 - (uintptr_t)name, name, fileName + 1);
		} else {
			pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "lib%s" PLATFORM_DLL_EXTENSION, name);
		}
		if (pathLength >= EsMaxPath) {
			result = OMRPORT_SL_UNSUPPORTED;
			goto exit;
		}
		openName = mangledName;
	}

	Trc_PRT_sl_open_shared_library_Event1(openName);

	/* CMVC 137341:
	 * dlopen() searches for libraries using the LIBPATH envvar as it was when the process
	 * was launched.  This causes multiple issues such as:
	 *  - finding 32 bit binaries for libomrsig.so instead of the 64 bit binary needed and vice versa
	 *  - finding compressed reference binaries instead of non-compressed ref binaries
	 *
	 * calling loadAndInit(libname, 0 -> no flags, NULL -> use the currently defined LIBPATH) allows
	 * us to load the library with the current libpath instead of the one at process creation
	 * time. We can then call dlopen() as per normal and the just loaded library will be found.
	 */
	loadAndInit(openName, L_RTLD_LOCAL, NULL);

	/* dlopen(2) called with NULL filename opens a handle to current executable. */
	handle = dlopen(openExec ? NULL : openName, lazyOrNow);
	if (NULL == handle) {
		/* get the initial error message, which may be more
		accurate than the errors from subsequent attempts */
		lastErrno = errno;
		getDLError(portLibrary, errBuf, sizeof(errBuf));
		if ((ENOENT != lastErrno) && (IS_FILE_VISIBLE(portLibrary, openName))) {
			result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
		} else {
			result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_NOT_FOUND, errBuf);
		}
	}

	if ((NULL == handle) && !openExec) {
		char portLibDir[1024];
		/* +1 for '/' separator and +1 for trailing NUL */
		size_t suffixLength = strlen(openName) + 1 + 1;

		/* last ditch, try dir port lib DLL is in */
		if ((suffixLength < sizeof(portLibDir)) && (0 != getDirectoryOfLibrary(portLibrary, portLibDir, sizeof(portLibDir) - suffixLength))) {
			strcat(portLibDir, "/");
			strcat(portLibDir, openName);
			loadAndInit(portLibDir, L_RTLD_LOCAL, NULL);
			handle = dlopen(portLibDir, lazyOrNow);
			if (NULL == handle) {
				lastErrno = errno;
				/* update the error string and return code only if a file was actually found */
				if ((ENOENT != lastErrno) && (IS_FILE_VISIBLE(portLibrary, portLibDir))) {
					getDLError(portLibrary, errBuf, sizeof(errBuf));
					result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
				}
			}
		}

		if (NULL == handle) {
#if defined(J9OS_I5)
			/* now check to see if it is an iSeries library. */
			handle = Xj9LoadIleLibrary(openName, errBuf, sizeof(errBuf));
			if (NULL == handle) {
				/* was an error message return?  */
				if ('\0' != errBuf[0]) {
					/* Yes, save it */
					result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
				}
#endif
				if (decorate) {
					if (NULL != fileName) {
						/* the names specifies a path */
						pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "%.*slib%s.a", (uintptr_t)fileName + 1 - (uintptr_t)name, name, fileName + 1);
					} else {
						pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "lib%s.a", name);
					}
					if (pathLength >= EsMaxPath) {
						result = OMRPORT_SL_UNSUPPORTED;
						goto exit;
					}

					Trc_PRT_sl_open_shared_library_Event1(mangledName);
					loadAndInit(mangledName, L_RTLD_LOCAL, NULL);
					handle = dlopen(mangledName, lazyOrNow);
					if (NULL == handle) {
						char *actualFileName = NULL;
						lastErrno = errno;
						/* update the error string and return code only if a file was actually found */
						if ((ENOENT != lastErrno) && (IS_FILE_VISIBLE(portLibrary, mangledName))) {
							getDLError(portLibrary, errBuf, sizeof(errBuf));
							result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
						}
#if defined(J9OS_I5)
						/* now check to see if it is an iSeries library */
						handle = Xj9LoadIleLibrary(mangledName, errBuf, sizeof(errBuf));
						if (NULL == handle) {
							/* was an error message return?  */
							if ('\0' != errBuf[0]) {
								/* Yes, save it */
								result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
							}
							if (NULL != fileName) {
								/* the names specifies a path */
								pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "%.*s%s.srvpgm", (uintptr_t)fileName + 1 - (uintptr_t)name, name, fileName + 1);
							} else {
								pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "%s.srvpgm", name);
							}
							if (pathLength >= EsMaxPath) {
								result = OMRPORT_SL_UNSUPPORTED;
								goto exit;
							}

							Trc_PRT_sl_open_shared_library_Event1(mangledName);
							handle = dlopen(mangledName, lazyOrNow);
							if (NULL == handle) {
								lastErrno = errno;
								/* update the error string and return code only if a file was actually found */
								if ((ENOENT != lastErrno) && (IS_FILE_VISIBLE(portLibrary, mangledName))) {
									getDLError(portLibrary, errBuf, sizeof(errBuf));
									result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
								}
								/* now check to see if it is an iSeries library */
								handle = Xj9LoadIleLibrary(mangledName, errBuf, sizeof(errBuf));
								if (NULL == handle) {
									/* was an error message return?  */
									if ('\0' != errBuf[0]) {
										/* Yes, save it */
										result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
									}
								}
#endif
								actualFileName = strrchr(fileName, '(');
								if ((NULL != actualFileName) && (NULL != strchr(fileName, ')'))) {
									Trc_PRT_sl_open_shared_library_Event1(name);

									/*
									 * reference : http://publib.boulder.ibm.com/infocenter/pseries/v5r3/index.jsp?topic=/com.ibm.aix.basetechref/doc/basetrf1/load.htm
									 * In case where library name contains '(' and ')', we are specifying an archive member.
									 * eg archive(member).
									 * This translates into a file of name 'archive', to which dlopen will look to load 'member' since RTLD_MEMBER is specified.
									 */

									loadAndInit(name, L_RTLD_LOCAL, NULL);
									handle = dlopen(name, lazyOrNow | RTLD_MEMBER);
									if (NULL == handle) {
										lastErrno = errno;

										pathLength = portLibrary->str_printf(
																		portLibrary, mangledName, (EsMaxPath + 1), "%.*s%.*s",
																		(uintptr_t)fileName + 1 - (uintptr_t)name,
																		name,
																		(uintptr_t)actualFileName - 1 - (uintptr_t)fileName,
																		fileName + 1);
										if (pathLength >= EsMaxPath) {
											result = OMRPORT_SL_UNSUPPORTED;
											goto exit;
										}
										if ((ENOENT != lastErrno) && (IS_FILE_VISIBLE(portLibrary, mangledName))) {
											getDLError(portLibrary, errBuf, sizeof(errBuf));
											result = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
										}
									}
								}
#if defined(J9OS_I5)
							}
						}
#endif
					}
				}
#if defined(J9OS_I5)
			}
#endif
		}
	}

exit:
	if (NULL == handle) {
		Trc_PRT_sl_open_shared_library_Event2(errBuf);
		Trc_PRT_sl_open_shared_library_Exit2(result);
		return result;
	}

	Trc_PRT_sl_open_shared_library_Exit1(handle);
	*descriptor = (uintptr_t) handle;
	return 0;
}

/**
 * Search for a function named 'name' taking argCount in the shared library 'descriptor'.
 *
 * @param[in] portLibrary The port library.
 * @param[in] descriptor Shared library to search.
 * @param[in] name Function to look up.
 * @param[out] func Pointer to the function.
 * @param[in] argSignature Argument signature.
 *
 * @return 0 on success, any other value on failure.
 *
 * argSignature is a C (ie: NUL-terminated) string with the following possible values for each character:
 *
 *		V	- void
 *		Z	- boolean
 *		B	- byte
 *		C	- char (16 bits)
 *		I	- integer (32 bits)
 *		J	- long (64 bits)
 *		F	- float (32 bits)
 *		D	- double (64 bits)
 *		L	- object / pointer (32 or 64, depending on platform)
 *		P	- pointer-width platform data. (in J9 terms an intptr_t)
 *
 * Lower case signature characters imply unsigned value.
 * Upper case signature characters imply signed values.
 * If it doesn't make sense to be signed/unsigned (eg: V, L, F, D Z) the character is upper case.
 *
 * argList[0] is the return type from the function.
 * The argument list is as it appears in english: list is left (1) to right (argCount)
 *
 * @note contents of func are undefined on failure.
 */
uintptr_t
omrsl_lookup_name(struct OMRPortLibrary *portLibrary, uintptr_t descriptor, char *name, uintptr_t *func, const char *argSignature)
{
	void *address = NULL;

	Trc_PRT_sl_lookup_name_Entry(descriptor, name, argSignature);

#if defined(J9OS_I5)
	address = Xj9dlsym((void *)descriptor, name, (char *)argSignature);
#else
	address = dlsym((void *)descriptor, name);
#endif
	if (NULL == address) {
		Trc_PRT_sl_lookup_name_Exit2(name, argSignature, descriptor, 1);
		return 1;
	}
	*func = (uintptr_t) address;

	Trc_PRT_sl_lookup_name_Exit1(*func);
	return 0;
}

static void
getDLError(struct OMRPortLibrary *portLibrary, char *errBuf, uintptr_t bufLen)
{
	const char *error;
	char charbuf[1024];
	uintptr_t dlopen_errno = errno;

	if (0 == bufLen) {
		return;
	}

	error = dlerror();

	/* dlerror can be misleading on AIX.  If the file we tried to dlopen exists maybe
	 * loadquery will generate a better message.
	 */
	if (ENOENT == dlopen_errno) {
		char *buffer[1024];
		if (loadquery(L_GETMESSAGES, buffer, sizeof(buffer)) != -1) {
			/* This checks for an error loading a dependant module which dlopen seems to report incorrectly */
			if ((L_ERROR_NOLIB == atoi(buffer[0])) && (L_ERROR_DEPENDENT == atoi(buffer[1]))) {
				error = portLibrary->nls_lookup_message(portLibrary,
														J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
														J9NLS_PORT_SL_ERROR_LOADING_DEPENDANT_MODULE,
														NULL);
				portLibrary->str_printf(portLibrary, errBuf, bufLen, error, &buffer[1][3]);
				return;
			}
		}
	} else if ((ENOEXEC == dlopen_errno) && (((NULL == error) || ('\0' == error[0])))) {
		/* Testing shows that this special case occurs
		 * when there is a symbol resolution problem
		 */
		error = portLibrary->nls_lookup_message(portLibrary,
												J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
												J9NLS_PORT_SL_SYMBOL_RESOLUTION_FAILURE,
												NULL);
		strncpy(errBuf, error, bufLen);
		errBuf[bufLen - 1] = '\0';
		return;
	}

	if ((NULL == error) || ('\0' == error[0])) {
		/* just in case another thread consumed our error message */
		error = portLibrary->nls_lookup_message(portLibrary,
												J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
												J9NLS_PORT_SL_UNKOWN_ERROR,
												NULL);
		strncpy(errBuf, error, bufLen);
		errBuf[bufLen - 1] = '\0';
		return;
	}

#if defined(J9VM_USE_MBTOWC)
	convertWithMBTOWC(portLibrary, error, errBuf, bufLen);
#elif defined(J9VM_USE_ICONV)
	convertWithIConv(portLibrary, error, errBuf, bufLen);
#else
	strncpy(errBuf, error, bufLen);
	errBuf[bufLen - 1] = '\0';
#endif
}

/**
 * determine the path to the port library
 *
 * @param[out] buf Pointer to memory which is filled in with path to port library
 *
 * @return 0 on failure, any other value on failure
 */
static uintptr_t
getDirectoryOfLibrary(struct OMRPortLibrary *portLib, char *buf, uintptr_t bufLen)
{
	struct ld_info *linfo, *linfop;
	int             linfoSize, rc;
	char           *myAddress;
	uintptr_t pathLength = 0;

	/* get loader information */
	linfoSize = 1024;
	linfo = portLib->mem_allocate_memory(portLib, linfoSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == linfo) {
		return 0;
	}
	for (;;) {
		rc = loadquery(L_GETINFO, linfo, linfoSize);
		if (rc != -1) {
			break;
		}
		linfoSize *= 2; /* insufficient buffer size - increase */
		linfop = portLib->mem_reallocate_memory(portLib, linfo, linfoSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == linfop) {
			portLib->mem_free_memory(portLib, linfo);
			return 0;
		}
		linfo = linfop;
	}

	/* find entry for my loaded object */
	myAddress = ((char **)&omrsl_open_shared_library)[0];
	for (linfop = linfo;;) {
		char *textorg  = (char *)linfop->ldinfo_textorg;
		char *textend  = textorg + (unsigned long)linfop->ldinfo_textsize;
		if ((myAddress >= textorg) && (myAddress < textend)) {
			break;
		}
		if (!linfop->ldinfo_next) {
			portLib->mem_free_memory(portLib, linfo);
			return 0;
		}
		linfop = (struct ld_info *)((char *)linfop + linfop->ldinfo_next);
	}

	/* +1 includes the NUL terminator */
	pathLength = strlen(linfop->ldinfo_filename) + 1;
	if (pathLength > bufLen) {
		/* a truncated copy of linfop->ldinfo_filename will be stored in buf */
		return 0;
	}

	strncpy(buf, linfop->ldinfo_filename, bufLen);
	/* remove libj9prt...so, reused myAddress here */
	myAddress = strrchr(buf, '/');
	if (NULL != myAddress) {
		*myAddress = '\0';
	}

	portLib->mem_free_memory(portLib, linfo);

	return 1;
}

#if (defined(J9VM_USE_ICONV)) /* priv. proto (autogen) */

static void
convertWithIConv(struct OMRPortLibrary *portLibrary, const char *error, char *errBuf, uintptr_t bufLen)
{
	iconv_t converter;
	size_t inbytesleft, outbytesleft;
	char *inbuf, *outbuf;

	converter = iconv_get(portLibrary, J9SL_ICONV_DESCRIPTOR, "UTF-8", nl_langinfo(CODESET));

	if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
		/* no converter available for this code set. Just dump the platform chars */
		strncpy(errBuf, error, bufLen);
		errBuf[bufLen - 1] = '\0';
		return;
	}

	inbuf = (char *)error; /* for some reason this argument isn't const */
	outbuf = errBuf;
	inbytesleft = strlen(error);
	outbytesleft = bufLen - 1;

	while ((outbytesleft > 0) && (inbytesleft > 0)) {
		if ((size_t)-1 == iconv(converter, &inbuf, &inbytesleft, &outbuf, &outbytesleft)) {
			if (errno == E2BIG) {
				break;
			}

			/* if we couldn't translate this character, copy one byte verbatim */
			*outbuf = *inbuf;
			outbuf++;
			inbuf++;
			inbytesleft--;
			outbytesleft--;
		}
	}

	*outbuf = '\0';
	iconv_free(portLibrary, J9SL_ICONV_DESCRIPTOR, converter);
}

#endif /* J9VM_USE_ICONV (autogen) */

#if (defined(J9VM_USE_MBTOWC)) /* priv. proto (autogen) */

static void
convertWithMBTOWC(struct OMRPortLibrary *portLibrary, char *error, char *errBuf, uintptr_t bufLen)
{
	char *out, *end, *walk;
	wchar_t ch;
	int ret;

	out = errBuf;
	end = &errBuf[bufLen - 1];

	walk = error;

	/* reset the shift state */
	mbtowc(NULL, NULL, 0);

	while ('\0' != *walk) {
		ret = mbtowc(&ch, walk, MB_CUR_MAX);
		if (ret < 0) {
			ch = *walk++;
		} else if (0 == ret) {
			break;
		} else {
			walk += ret;
		}

		if ('\r' == ch) {
			continue;
		}
		if ('\n' == ch) {
			ch = ' ';
		}
		if (ch < 0x80) {
			if ((out + 1) > end) {
				break;
			}
			*out++ = (char)ch;
		} else if (ch < 0x800) {
			if ((out + 2) > end) {
				break;
			}
			*out++ = (char)(0xc0 | ((ch >> 6) & 0x1f));
			*out++ = (char)(0x80 | (ch & 0x3f));
		} else {
			if ((out + 3) > end) {
				break;
			}
			*out++ = (char)(0xe0 | ((ch >> 12) & 0x0f));
			*out++ = (char)(0x80 | ((ch >> 6) & 0x3f));
			*out++ = (char)(0x80 | (ch & 0x3f));
		}
	}

	*out = '\0';
}

#endif /* J9VM_USE_MBTOWC (autogen) */

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrsl_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
omrsl_shutdown(struct OMRPortLibrary *portLibrary)
{
}

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the shared library operations may be created here.  All resources created here should be destroyed
 * in @ref omrsl_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_SL
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrsl_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}
