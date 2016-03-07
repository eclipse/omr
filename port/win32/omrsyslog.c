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
 * @brief System logging support
 */
#include <windows.h>

#include "omrportpriv.h"
#include "omrsyslogmessages.h"
#include "ut_omrport.h"

/**
 * Write a message to the system log.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - to indicate what type of message (e.g. ERROR) is to be output
 * @param[in] message - text of the message
 *
 * @return Boolean: true on success, false on failure
 */
static uintptr_t
omrsyslog_write_utf8(struct OMRPortLibrary *portLibrary, uintptr_t flags, const char *message)
{
	if (NULL != portLibrary->portGlobals && NULL != PPG_syslog_handle) {
		switch (flags) {
		case J9NLS_ERROR:
			return ReportEvent(PPG_syslog_handle, EVENTLOG_ERROR_TYPE, 0, MSG_ERROR, NULL, 1, 0, (LPCTSTR *)&message, NULL);
		case J9NLS_WARNING:
			return ReportEvent(PPG_syslog_handle, EVENTLOG_WARNING_TYPE, 0, MSG_WARNING, NULL, 1, 0, (LPCTSTR *)&message, NULL);
		case J9NLS_INFO:
		default:
			return ReportEvent(PPG_syslog_handle, EVENTLOG_INFORMATION_TYPE, 0, MSG_INFO, NULL, 1, 0, (LPCTSTR *)&message, NULL);
		}
	}
	return FALSE;
}

/**
 * Write a message to the system log.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags - to indicate what type of message (e.g. ERROR) is to be output
 * @param[in] message - text of the message
 *
 * @return Boolean: true on success, false on failure
 */
static uintptr_t
omrsyslog_write_unicode(struct OMRPortLibrary *portLibrary, uintptr_t flags, const wchar_t *message)
{
	if (NULL != portLibrary->portGlobals && NULL != PPG_syslog_handle) {
		switch (flags) {
		case J9NLS_ERROR:
			return ReportEventW(PPG_syslog_handle, EVENTLOG_ERROR_TYPE, 0, MSG_ERROR, NULL, 1, 0, (LPCWSTR *)&message, NULL);
		case J9NLS_WARNING:
			return ReportEventW(PPG_syslog_handle, EVENTLOG_WARNING_TYPE, 0, MSG_WARNING, NULL, 1, 0, (LPCWSTR *)&message, NULL);
		case J9NLS_INFO:
		default:
			return ReportEventW(PPG_syslog_handle, EVENTLOG_INFORMATION_TYPE, 0, MSG_INFO, NULL, 1, 0, (LPCWSTR *)&message, NULL);
		}
	}
	return FALSE;
}

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
	const size_t messageLen = strlen(message);
	/* Adding one more wchar for the null terminating char */
	const size_t unicodeMsgLen = (messageLen * sizeof(wchar_t)) + sizeof(wchar_t);
	BOOLEAN unicodeSuccess = FALSE;
	uintptr_t retval = FALSE;

	wchar_t *const unicodeMsg =
		portLibrary->mem_allocate_memory(portLibrary, unicodeMsgLen, OMR_GET_CALLSITE(),
										 OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL != unicodeMsg) {
		/* If we got here, we are guaranteed to have enough space for the converted
		 * string and the null termination char. Under these circumstances, str_convert
		 * routine guarantees that the resulting string will be null terminated.
		 */
		const int32_t convRes =
			portLibrary->str_convert(portLibrary, J9STR_CODE_MUTF8, J9STR_CODE_WIDE, message,
									 messageLen, (char *) unicodeMsg, unicodeMsgLen);

		unicodeSuccess = (BOOLEAN)(convRes >= 0);

		if (! unicodeSuccess) {
			Trc_PRT_omrsyslog_failed_str_convert(convRes);
		}
	}

	if (unicodeSuccess) {
		retval = omrsyslog_write_unicode(portLibrary, flags, unicodeMsg);
	} else {
		retval = omrsyslog_write_utf8(portLibrary, flags, message);
	}

	if (NULL != unicodeMsg) {
		portLibrary->mem_free_memory(portLibrary, unicodeMsg);
	}

	return retval;
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
	/* now register against that source name */
	/* note that a registry entry for that event source name needs to be found for the messages to get logged correctly */
	/* registry key: HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog\Application\IBM Java */
	char defaultEventSource[] = "IBM Java";
	char *syslogEventSource;
	BOOL rc = FALSE;

	if (NULL != portLibrary->portGlobals) {

		if (NULL == PPG_syslog_handle) {
			/* look for the key name in an environment variable (internal use only) */
			syslogEventSource = getenv("IBM_JAVA_SYSLOG_NAME");

			if (NULL == syslogEventSource) {
				/* no event source name so register with the default */
				PPG_syslog_handle = RegisterEventSource(NULL, TEXT(defaultEventSource));
			} else {
				/* found the event source name so register using it */
				PPG_syslog_handle = RegisterEventSource(NULL, TEXT(syslogEventSource));
			}

			if (NULL != PPG_syslog_handle) {
				rc = TRUE;
			}
		}
	}
	return rc;
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
	BOOL rc = FALSE;

	if (NULL != portLibrary->portGlobals) {
		if (NULL != PPG_syslog_handle) {
			rc = DeregisterEventSource(PPG_syslog_handle);
			PPG_syslog_handle = NULL;
		}
	}

	return rc;
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
	uintptr_t options;

	/* query the logging options here */
	options = PPG_syslog_flags;

	return options;
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

