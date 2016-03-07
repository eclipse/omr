/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 * @brief Dump formatting
 */
#include "omrport.h"



/**
 * Create a dump file of the OS state.
 *
 * @param[in] portLibrary The port library.
 * @param[in] filename Buffer for filename optionally containing the filename where dump is to be output.
 * @param[out] filename filename used for dump file or error message.
 * @param[in] dumpType Type of dump to perform.
 * @param[in] userData Implementation specific data.
 *
 * @return 0 on success, non-zero otherwise.
 *
 * @note filename buffer can not be NULL.
 * @note user allocates and frees filename buffer.
 * @note filename buffer length is platform dependent, assumed to be EsMaxPath/MAX_PATH
 *
 * @note if filename buffer is empty, a filename will be generated.
 * @note if J9UNIQUE_DUMPS is set, filename will be unique.
 */
uintptr_t
omrdump_create(struct OMRPortLibrary *portLibrary, char *filename, char *dumpType, void *userData)
{
	/* noop */
	return 1;
}

/**
 * Set up the environment to create the dump file.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, non-zero otherwise. Error code values returned are platform dependent.
 * On AIX, on error, it returns OMRPORT_ERROR_STARTUP_AIX_PROC_ATTR on failure.
 */
int32_t
omrdump_startup(struct OMRPortLibrary *portLibrary)
{
	/* noop */
	return 0;
}

/**
 * Called during shutdown of shared library to clear up any resources reserved by @ref omrdump_startup.
 *
 * @param[in] portLibrary The port library.
 */
void
omrdump_shutdown(struct OMRPortLibrary *portLibrary)
{
	/* noop */
}


