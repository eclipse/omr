/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/
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

