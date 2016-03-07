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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef OMRUTIL_H_INCLUDED
#define OMRUTIL_H_INCLUDED

/**
* This file contains public function prototypes and
* type definitions for the util_core module.
*
*/

#include "omrcomp.h"
#include "omrport.h"
#include "omr.h"
#include "pool_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(J9ZOS390)
#pragma map(getdsa, "GETDSA")
/* ----------------- j9getdsa.s ---------------- */
/**
 * Returns the caa and dsa respectively on z/OS
 */
void *getdsa(void);
#endif


/* ---------------- utf8decode.c ---------------- */

/**
* @brief
* @param input
* @param result
* @return uint32_t
*/
uint32_t
decodeUTF8Char(const uint8_t *input, uint16_t *result);


/**
* @brief
* @param input
* @param result
* @param bytesRemaining
* @return uint32_t
*/
uint32_t
decodeUTF8CharN(const uint8_t *input, uint16_t *result, uintptr_t bytesRemaining);


/* ---------------- utf8encode.c ---------------- */

/**
* @brief
* @param unicode
* @param result
* @return uint32_t
*/
uint32_t
encodeUTF8Char(uintptr_t unicode, uint8_t *result);


/**
* @brief
* @param unicode
* @param result
* @param bytesRemaining
* @return uint32_t
*/
uint32_t
encodeUTF8CharN(uintptr_t unicode, uint8_t *result, uint32_t bytesRemaining);



/* ---------------- xml.c ---------------- */

/**
 * Escapes a string for use with XML.
 * @param portLibrary[in]
 * @param outBuf[out] A buffer in which to place the escaped string.
 * @param outBufLen[in] The length of the output buffer.
 * @param string[in] The string that should be escaped.
 * @param StringLen[in] The length of the string excluding any null termination.
 * @return The number of characters of the input string that were processed before filling the output buffer.
 */
uintptr_t
escapeXMLString(OMRPortLibrary *portLibrary, char *outBuf, uintptr_t outBufLen, const char *string, uintptr_t stringLen);



/* ----------------- primeNumberHelper.c ---------------- */
/*
 * PRIMENUMBERHELPER_OUTOFRANGE is used when primeNumberHelper functions are being used
 * by a number that is not in the supported range of primeNumberHelper.
 * For such cases, these functions return PRIMENUMBERHELPER_OUTOFRANGE.
 */
#define PRIMENUMBERHELPER_OUTOFRANGE 1
/**
 * @brief
 * @param number
 * @return uintptr_t
 */
uintptr_t findLargestPrimeLessThanOrEqualTo(uintptr_t number);

/**
 * @brief
 * @param number
 * @return uintptr_t
 */
uintptr_t findSmallestPrimeGreaterThanOrEqualTo(uintptr_t number);

/**
 * @brief
 * @param void
 * @return uintptr_t
 */
uintptr_t getSupportedBiggestNumberByPrimeNumberHelper(void);

#if defined(WIN32) && !defined(WIN32_IBMC)

/* ---------------- j9getdbghelp.c ---------------- */

/**
* @brief Load the version of dbghelp.dll that shipped with the JRE. If we can't find the shipped version, try to find it somewhere else.
* @return A handle to dbghelp.dll if we were able to find one, NULL otherwise.
*/
uintptr_t j9getdbghelp_loadDLL(void);

/**
* @brief Get a previously loaded version of dbghelp.dll that shipped with the JRE.
* @return A handle to dbghelp.dll if we were able to find a previously loaded version, NULL otherwise.
*/
uintptr_t j9getdbghelp_getDLL(void);

/**
* @brief Free the supplied version of dbgHelpDLL
* @param dbgHelpDLL
* @return 0 if the library was freed, non-zero otherwise.
*/
void j9getdbghelp_freeDLL(uintptr_t dbgHelpDLL);

#endif  /* defined(WIN32) && !defined(WIN32_IBMC) */


/* ---------------- stricmp.c ---------------- */

/*
 * Converts characters to lowercase.
 * This is intended only to be used for command line processing.
 * Although currently implemented to do ASCII conversion,
 * it may be changed in the future if additional behavior is required.
 * This is used to address locale issues such as in Turkish where
 * the uppercase of 'i' does not equal 'I'.
 *
 *
 * @param c input character
 * @return lowercase version or returns input if a valid ASCII
 *         character wasn't given.
 */
int
j9_cmdla_tolower(int c);

/*
 * Converts ASCII character to lowercase.
 * This is used to address locale issues such as in Turkish where
 * the uppercase of 'i' does not equal 'I'.
 *
 * @param c input character
 * @return lowercase version or returns input if a valid ASCII
 *         character wasn't given.
 */
int
j9_ascii_tolower(int c);

/*
 * Converts characters to uppercase.
 * This is intended only to be used for command line processing.
 * Although currently implemented to do ASCII conversion,
 * it may be changed in the future if additional behavior is required.
 * This is used to address locale issues such as in Turkish where
 * the uppercase of 'i' does not equal 'I'.
 *
 * @param c input character
 * @return uppercase version or returns input if a valid ASCII
 *         character wasn't given.
 */
int
j9_cmdla_toupper(int c);

/*
 * Converts ASCII character to uppercase.
 * This is used to address locale issues such as in Turkish where
 * the uppercase of 'i' does not equal 'I'.
 *
 * @param c input character
 * @return uppercase version or returns input if a valid ASCII
 *         character wasn't given.
 */
int
j9_ascii_toupper(int c);

/*
 * Compare the bytes of two strings, ignoring case. Intended to be used
 * with command line arguments.
 * This is used to address locale issues such as in Turkish where
 * the uppercase of 'i' does not equal 'I'.
 *
 * @param *s1 string1
 * @param *s2 string2
 * @return a positive integer if s1 is greater, zero if the strings
 *         are equal, or a negative value if s1 is less.
 */
int
j9_cmdla_stricmp(const char *s1, const char *s2);

/*
 * Compare the bytes of two strings, ignoring case. Intended to be used
 * with command line arguments.
 * This is used to address locale issues such as in Turkish where
 * the uppercase of 'i' does not equal 'I'.
 *
 * @param *s1 string1
 * @param *s2 string2
 * @param length of string
 * @return a positive integer if s1 is greater, zero if the strings
 *         are equal, or a negative value if s1 is less.
 */
int
j9_cmdla_strnicmp(const char *s1, const char *s2, size_t length);

/* ---------------- argscan.c ---------------- */

/**
* @brief
* @param **scan_start
* @param *search_string
* @return uintptr_t
*/
uintptr_t try_scan(char **scan_start, const char *search_string);


/* ---------------- detectVMDirectory.c ---------------- */
#if defined(WIN32)
/**
 * @brief Detect the directory where the VM library or executable resides.
 *
 * Uses the glue function OMR_Glue_GetVMDirectoryToken().
 *
 * @param[in,out] vmDirectory The buffer to where the VM directory will be written. Must be non-NULL.
 * @param[in] vmDirectoryLength The length of vmDirectory, in wchar_t.
 * @param[out] vmDirectoryEnd A pointer to the nul-terminator of the VM directory string. This cursor
 * can be used to append a string to the VM directory. Must be non-NULL.
 *
 * @return OMR_ERROR_NONE on success, an OMR error code otherwise.
 */
omr_error_t detectVMDirectory(wchar_t *vmDirectory, size_t vmDirectoryLength, wchar_t **vmDirectoryEnd);
#endif /* defined(WIN32) */

#if defined(J9ZOS390)
/* ---------------- getstoragekey.c ---------------- */

/**
 * Get the storage key for current process on z/OS.
 *
 * @return The current z/OS storage protection key.
 *
 */

uintptr_t getStorageKey(void);

#endif /*if defined(J9ZOS390)*/

/**
 * Returns a string representing the type of page indicated by the given pageFlags.
 * Useful when printing page type.
 *
 * @param[in] pageFlags indicates type of the page.
 *
 * @return pointer to string representing the page type.
 */
const char *
getPageTypeString(uintptr_t pageFlags);

/**
 * Returns a string (with a leading space) representing the type of page indicated by the given pageFlags.
 * Useful when printing page type.
 *
 * @param[in] pageFlags indicates type of the page.
 *
 * @return pointer to string (with a leading space) representing the page type.
 */
const char *
getPageTypeStringWithLeadingSpace(uintptr_t pageFlags);


/* ---------------- j9memclr.c ---------------- */

/**
* @brief
* @param void
* @return uintptr_t
*/
uintptr_t getCacheLineSize(void);


/**
* @brief
* @param *ptr
* @param length
* @return void
*/
void OMRZeroMemory(void *ptr, uintptr_t length);


/**
* @brief
* @param *dest
* @param value
* @param size
* @return void
*/
void j9memset(void *dest, intptr_t value, uintptr_t size);

/* ---------------- archinfo.c ---------------- */
/**
 * @brief
 * @return int32_t
 */

#define G5                              (9672)  /* Not Supported as of Java 1.5 */
#define MULTIPRISE7000                  (7060)  /* Not Supported as of Java 1.5 */
#define FREEWAY                         (2064)
#define Z800                            (2066)
#define MIRAGE                          (1090)
#define TREX                            (2084)
#define Z890                            (2086)
#define GOLDEN_EAGLE                    (2094)
#define DANU_GA2                        (2094)  /* type doesn't change from GOLDEN_EAGLE */
#define Z9BC                            (2096)
#define Z10                             (2097)
#define Z10BC                           (2098)  /* zMR */

int32_t get390zLinuxMachineType(void);

/* ---------------- poolForPort.c ---------------- */

#define POOL_FOR_PORT(portLib) (omrmemAlloc_fptr_t)pool_portLibAlloc, (omrmemFree_fptr_t)pool_portLibFree, (portLib)
#if defined(OMR_ENV_DATA64)
#define POOL_FOR_PORT_PUDDLE32(portLib) (omrmemAlloc_fptr_t)pool_portLibAlloc32, (omrmemFree_fptr_t)pool_portLibFree32, (portLib)
#else /* defined(OMR_ENV_DATA64) */
#define POOL_FOR_PORT_PUDDLE32(portLib) NULL, NULL, NULL
#endif /* defined(OMR_ENV_DATA64) */

void *
pool_portLibAlloc(OMRPortLibrary *portLibrary, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit);

void
pool_portLibFree(OMRPortLibrary *portLibrary, void *address, uint32_t type);

#if defined(OMR_ENV_DATA64)

void *
pool_portLibAlloc32(OMRPortLibrary *portLibrary, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit);

void
pool_portLibFree32(OMRPortLibrary *portLibrary, void *address, uint32_t type);
#endif /* defined(OMR_ENV_DATA64) */


/* ---------------- thrname_core.c ---------------- */

struct OMR_VMThread;

/**
 * Lock and get the thread name. Must be paired with releaseOMRVMThreadName().
 *
 * @param[in] vmThread The vmthread.
 * @return The thread name.
 */
char *getOMRVMThreadName(struct OMR_VMThread *vmThread);

/**
 * Try to lock and get the thread name. Fails if locking requires blocking.
 * If this call succeeds, it must be paired with releaseOMRVMThreadName().
 * Don't call releaseOMRVMThreadName() if this call failed.
 *
 * @param[in] vmThread The vmthread.
 * @return NULL if we failed to get the lock, a non-NULL thread name if we succeeded.
 */
char *tryGetOMRVMThreadName(struct OMR_VMThread *vmThread);

/**
 * Unlock the thread name that was obtained using getOMRVMThreadName() or tryGetOMRVMThreadName().
 * @param[in] vmThread The vmthread.
 */
void releaseOMRVMThreadName(struct OMR_VMThread *vmThread);

/**
 * Lock the thread name, set it, and then unlock it.
 * The thread name may be freed by the VM.
 *
 * @param[in] currentThread The current vmthread.
 * @param[in] vmThread The vmthread whose name should be set.
 * @param[in] name The new thread name.
 * @param[in] nameIsStatic If non-zero, this indicates that the VM should not free the thread name.
 */
void setOMRVMThreadNameWithFlag(struct OMR_VMThread *currentThread, struct OMR_VMThread *vmThread, char *name, uint8_t nameIsStatic);

/**
 * Set the thread name without locking it.
 *
 * @param[in] vmThread The vmthread whose name should be set.
 * @param[in] name The new thread name.
 * @param[in] nameIsStatic If non-zero, this indicates that the VM should not free the thread name.
 */
void setOMRVMThreadNameWithFlagNoLock(struct OMR_VMThread *vmThread, char *name, uint8_t nameIsStatic);

/* ---------------- threadhelp.c ---------------- */

/**
 * Helper function to create a thread with a specific thread category.
 *
 * @param[out] handle a pointer to a omrthread_t which will point to the thread (if successfully created)
 * @param[in] stacksize the size of the new thread's stack (bytes)<br>
 *			0 indicates use default size
 * @param[in] priority priorities range from J9THREAD_PRIORITY_MIN to J9THREAD_PRIORITY_MAX (inclusive)
 * @param[in] suspend set to non-zero to create the thread in a suspended state.
 * @param[in] entrypoint pointer to the function which the thread will run
 * @param[in] entryarg a value to pass to the entrypoint function
 * @param[in] category category of the thread to be created
 *
 * @return success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_xxx failure
 *
 * @see omrthread_create
 */
intptr_t
createThreadWithCategory(omrthread_t *handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend,
	omrthread_entrypoint_t entrypoint, void *entryarg, uint32_t category);

/**
 * Helper function to attach a thread with a specific category.
 *
 * If the OS thread is already attached, handle is set to point to the existing omrthread_t.
 *
 * @param[out] handle a pointer to a omrthread_t which will point to the thread (if successfully attached)
 * @param[in]  category thread category
 *
 * @return success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_xxx failure
 *
 * @see omrthread_attach
 */
intptr_t
attachThreadWithCategory(omrthread_t *handle, uint32_t category);

#ifdef __cplusplus
}
#endif

#endif /* OMRUTIL_H_INCLUDED */

