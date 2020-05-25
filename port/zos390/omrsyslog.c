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
 * @brief System logging support
 */
#include "omrportpriv.h"
#include "atoe.h"
#include <string.h>
#include <sys/__messag.h>

uintptr_t writeToZOSLog(const char *message);

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
	if (NULL != portLibrary->portGlobals && TRUE == PPG_syslog_enabled) {
		return writeToZOSLog(message);
	}
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
	if (NULL != portLibrary->portGlobals) {
		PPG_syslog_enabled = TRUE;
		return TRUE;
	}

	return FALSE;
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
	if (NULL != portLibrary->portGlobals) {
		PPG_syslog_enabled = FALSE;
		return TRUE;
	}

	return FALSE;
}

uintptr_t
writeToZOSLog(const char *message)
{
	char *ebcdicbuf = NULL;
	int rc = 0;
	struct __cons_msg2 cons;
	int modcmd = 0;
	unsigned int routeCodes[2] = {2, 0}; /* routing code 2 = Operator Information */
	unsigned int descCodes[2] = {12, 0}; /* descriptor code 12 = Important Information, no operator action reqd */

#if !defined(OMR_EBCDIC)
	/* Convert from the internal ascii format to ebcdic */
	ebcdicbuf = a2e_func((char *) message, strlen(message));
	if (NULL == ebcdicbuf) {
		return FALSE;
	}
#else
   ebcdicbuf = message;
#endif /* !defined(OMR_EBCDIC) */

	/* Re-implemented using _console2() instead of WTO, to provided proper multi-line messages. See
	 * http://publib.boulder.ibm.com/infocenter/zos/v1r9/index.jsp?topic=/com.ibm.zos.r9.bpxbd00/consol2.htm
	 */
	cons.__cm2_format = __CONSOLE_FORMAT_2;
	cons.__cm2_msglength = strlen(message);
	cons.__cm2_msg = ebcdicbuf;
	cons.__cm2_routcde = routeCodes;
	cons.__cm2_descr = descCodes;
	cons.__cm2_mcsflag = 0;
	cons.__cm2_token = 0;
	cons.__cm2_msgid = NULL;
	cons.__cm2_dom_token = 0;
	cons.__cm2_dom_msgid = NULL;

	rc = __console2(&cons, NULL, &modcmd);

#if !defined(OMR_EBCDIC)
	free(ebcdicbuf);
#endif /* !defined(OMR_EBCDIC) */

	if (0 == rc) {
		return TRUE;
	} else {
		return FALSE;
	}
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
	/* query the logging options here */
	return PPG_syslog_flags;
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
	/* set the logging options here */
	PPG_syslog_flags = options;
}
