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
 * @brief Process shutdown.
 */
#include <stdlib.h>

#include "omrport.h"

#if !defined(WIN32)
#if defined(OMRPORT_OMRSIG_SUPPORT)
extern void omrsig_chain_at_shutdown_and_exit(struct OMRPortLibrary *portLibrary);
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */
#endif /* !defined(WIN32) */


/**
 * Block until the portlibary has been exited and return the error code.
 *
 * @param[in] portLibrary The port library
 *
 * @return exit code.
 * @note Most implementations will be empty.
 */
int32_t
omrexit_get_exit_code(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Terminate a process.
 *
 * Perform any shutdown which is absolutely necessary before terminating the process,
 * and terminate the process.  Unblock any callers to @ref omrexit_get_exit_code
 *
 * @param[in] portLibrary The port library
 * @param[in] exitCode The exit code to be used to terminate the process.
 */
void OMRNORETURN
omrexit_shutdown_and_exit(struct OMRPortLibrary *portLibrary, int32_t exitCode)
{
#if defined(J9VM_OPT_CUDA)
	/* Because we're exiting we don't expect the port library to be shutdown normally,
	 * but we don't want to omit the cuda shutdown step because it resets the devices
	 * (if any were discovered) avoiding resource leaks that may lead to trouble for
	 * future processes. If CUDA support was never requested, this is effectively a nop.
	 * The function is also idempotent so a subsequent call would do no harm.
	 */
	portLibrary->cuda_shutdown(portLibrary);
#endif /* J9VM_OPT_CUDA */

#if !defined(WIN32)
#if defined(OMRPORT_OMRSIG_SUPPORT)
	omrsig_chain_at_shutdown_and_exit(portLibrary);
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */
#endif

	exit((int)exitCode);
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrexit_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Most implementations will be empty.
 */
void
omrexit_shutdown(struct OMRPortLibrary *portLibrary)
{
}

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the exit operations may be created here.  All resources created here should be destroyed
 * in @ref omrexit_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_EXIT
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrexit_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}


