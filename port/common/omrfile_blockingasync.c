/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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
 * @brief file
 */

#include "omrport.h"
#include "omrstdarg.h"
#include "portnls.h"
#include "ut_omrport.h"

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
omrfile_blockingasync_close(struct OMRPortLibrary *portLibrary, intptr_t fd)
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
 */
intptr_t
omrfile_blockingasync_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	return -1;
}

/**
 * This function will acquire a lock of the requested type on the given file, starting at offset bytes
 * from the start of the file and continuing for length bytes
 *
 * @param [in]   portLibrary            The port library
 * @param [in]   fd                     The file descriptor/handle of the file to be locked
 * @param [in]   lockFlags              Flags indicating the type of lock required and whether the call should block
 * @args                                	OMRPORT_FILE_READ_LOCK
 * @args                                    OMRPORT_FILE_WRITE_LOCK
 * @args                                    OMRPORT_FILE_WAIT_FOR_LOCK
 * @args                                    OMRPORT_FILE_NOWAIT_FOR_LOCK
 * @param [in]   offest                 Offset from start of file to start of locked region
 * @param [in]   length                 Number of bytes to be locked
 *
 * @return                              0 on success, -1 on failure
 */
int32_t
omrfile_blockingasync_lock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length)
{
	return -1;
}


/**
 * This function will release the lock on the given file, starting at offset bytes
 * from the start of the file and continuing for length bytes
 *
 * @param [in]   portLibrary            The port library
 * @param [in]   fd                     The file descriptor/handle of the file to be locked
 * @param [in]   offest                 Offset from start of file to start of locked region
 * @param [in]   length                 Number of bytes to be unlocked
 *
 * @return                              0 on success, -1 on failure
 */
int32_t
omrfile_blockingasync_unlock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length)
{
	return -1;
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
omrfile_blockingasync_read(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
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
 * @return Number of bytes written on success, portable error return code (which is negative) on failure.
 */
intptr_t
omrfile_blockingasync_write(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
{
	return -1;
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
omrfile_blockingasync_set_length(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength)
{
	return -1;
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
omrfile_blockingasync_flength(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	return -1;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrfile_blockingasync_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Most implementations will be empty.
 */
void
omrfile_blockingasync_shutdown(struct OMRPortLibrary *portLibrary)
{
}

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the file operations may be created here.  All resources created here should be destroyed
 * in @ref omrfile_blockingasync_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_FILE
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrfile_blockingasync_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}
