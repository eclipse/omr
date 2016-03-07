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
/**
 * @file
 * @ingroup Port
 * @brief System logging support
 */
#include "omrport.h"

/**
 * Write a message to the system log.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - to indicate what type of message (e.g. ERROR) is to be output
 * @param[in] message - text of the message
 *
 * @return Boolean: true on success, false on failure
 */
uintptr_t
omrsyslog_write(struct OMRPortLibrary *portLibrary, uintptr_t flags, const char *message)
{
	/* noop */
	return 0;
}

/**
 * Open the system log.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - for future expansion
 *
 * @return Boolean: true on success, false on failure
 */
uintptr_t
syslogOpen(struct OMRPortLibrary *portLibrary, uintptr_t flags)
{
	/* noop */
	return 0;
}

/**
 * Close the system log.
 *
 * @param[in] portLibrary The port library
 *
 * @return Boolean: true on success, false on failure
 */
uintptr_t
syslogClose(struct OMRPortLibrary *portLibrary)
{
	/* noop */
	return 0;
}

/**
 * Query the settings of the system log.
 *
 * @param[in] portLibrary The port library
 *
 * @return uintptr_t: the current logging options
 */
uintptr_t
omrsyslog_query(struct OMRPortLibrary *portLibrary)
{
	/* noop */
	return 0;
}

/**
 * Set the system log flags.
 *
 * @param[in] portLibrary The port library
 * @param[in] options - uintptr_t containing the options to set
 *
 * @return void
 */
void
omrsyslog_set(struct OMRPortLibrary *portLibrary, uintptr_t options)
{
	/* noop */
}

