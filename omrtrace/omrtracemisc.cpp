/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1998, 2016
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

#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include "omrcomp.h"
#include "omrtrace_internal.h"
#include "pool_api.h"

/*******************************************************************************
 * name        - getTraceLock
 * description - Obtains the trace lock
 * parameters  - OMR_TraceThread
 * returns     - True or false
 ******************************************************************************/
int32_t
getTraceLock(OMR_TraceThread *thr)
{
	incrementRecursionCounter(thr);
	omrthread_monitor_enter(OMR_TRACEGLOBAL(traceLock));
	return TRUE;
}

/*******************************************************************************
 * name        - freeTraceLock
 * description - Frees the trace lock
 * parameters  - OMR_TraceThread
 * returns     - True or false
 ******************************************************************************/
int32_t
freeTraceLock(OMR_TraceThread *thr)
{
	omrthread_monitor_exit(OMR_TRACEGLOBAL(traceLock));
	decrementRecursionCounter(thr);
	return TRUE;
}

void
incrementRecursionCounter(OMR_TraceThread *thr)
{
	thr->recursion++;
}

void
decrementRecursionCounter(OMR_TraceThread *thr)
{
	thr->recursion--;
}

/**
 * Acquire the global trace lock if the trace engine's initialization phase
 * has completed.
 */
void
checkGetTraceLock(OMR_TraceThread *thr)
{
	if (NULL != thr) {
		getTraceLock(thr);
	} else {
		UT_ASSERT(OMR_TRACE_ENGINE_MT_ENABLED != OMR_TRACEGLOBAL(initState));
	}
}

/**
 * Release the global trace lock if the trace engine's initialization phase
 * has completed.
 */
void
checkFreeTraceLock(OMR_TraceThread *thr)
{
	if (NULL != thr) {
		freeTraceLock(thr);
	} else {
		UT_ASSERT(OMR_TRACE_ENGINE_MT_ENABLED != OMR_TRACEGLOBAL(initState));
	}
}

/*******************************************************************************
 * name        - expandString
 * description - take a string and substutue pid number for %p, date for
 *               %d and time for %t
 * parameters  - returnBuffer - the expanded string as return
 *               value - the original string
 * returns     - OMR_ERROR_NONE or OMR_ERROR_ILLEGAL_ARGUMENT
 ******************************************************************************/
omr_error_t
expandString(char *returnBuffer, const char *original, BOOLEAN atRuntime)
{
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	const char *pidTokenString = "%pid";
	const char *dateTokenString = "%Y" "%m" "%d";
	const char *timeTokenString = "%H" "%M" "%S";
	char formatString[MAX_IMAGE_PATH_LENGTH];
	char resultString[MAX_IMAGE_PATH_LENGTH];
	size_t originalLen, originalCursor = 0, formatCursor = 0;
	struct J9StringTokens *stringTokens;
	int64_t curTime;

	if ((returnBuffer == NULL) || (original == NULL)) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	originalLen = strlen(original);

	for (; originalCursor < originalLen ; originalCursor++) {
		if (original[originalCursor] == '%') {
			originalCursor++;
			if (original[originalCursor] == 'p') {
				strncpy(&formatString[formatCursor], pidTokenString, strlen(pidTokenString));
				formatCursor += strlen(pidTokenString);
			} else if (original[originalCursor] == 'd') {
				strncpy(&formatString[formatCursor], dateTokenString, strlen(dateTokenString));
				formatCursor += strlen(dateTokenString);
			} else if (original[originalCursor] == 't') {
				strncpy(&formatString[formatCursor], timeTokenString, strlen(timeTokenString));
				formatCursor += strlen(timeTokenString);
			} else {
				/* character following % was not 'p', 'd' or 't' */
				reportCommandLineError(atRuntime, "Invalid special character '%%%c' in a trace filename. Only %%p, %%d and %%t are allowed.", original[originalCursor]);
				returnBuffer[0] = '\0';
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		} else {
			formatString[formatCursor] = original[originalCursor];
			formatCursor++;
		}
		if (formatCursor >= MAX_IMAGE_PATH_LENGTH - 1) {
			formatCursor = MAX_IMAGE_PATH_LENGTH - 1;
			break;
		}
	}
	formatString[formatCursor] = '\0';
	curTime = omrtime_current_time_millis();
	stringTokens = omrstr_create_tokens(curTime);
	if (NULL == stringTokens) {
		returnBuffer[0] = '\0';
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (omrstr_subst_tokens(resultString, MAX_IMAGE_PATH_LENGTH, formatString, stringTokens) > MAX_IMAGE_PATH_LENGTH) {
		returnBuffer[0] = '\0';
		omrstr_free_tokens(stringTokens);
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	omrstr_free_tokens(stringTokens);

	strncpy(returnBuffer, resultString, 255);
	returnBuffer[255] = '\0';
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - listCounters
 * description - Print out the entire trace counter table.
 * parameters  - void
 * returns     - void
 ******************************************************************************/
void
listCounters(void)
{
	int           i;
#define TEMPBUFLEN 150
	char          tempBuf[TEMPBUFLEN];
	intptr_t         f;
	UtComponentData *compData = OMR_TRACEGLOBAL(componentList)->head;
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> Listing trace counters\n"));

	if ((f = omrfile_open("utTrcCounters", EsOpenText | EsOpenWrite | EsOpenTruncate | EsOpenCreateNoTag, 0)) < 0) {
		if ((f = omrfile_open("utTrcCounters", EsOpenText | EsOpenWrite | EsOpenCreate | EsOpenCreateNoTag, 0666)) < 0) {
			/* Unable to open tracepoint counter file %s, counters redirected to stderr. */
		}
	}

	/* Writing trace count info to %s */
	/* list currently loaded components */
	while (compData != NULL) {
		if (compData->tracepointcounters != NULL) {
			for (i = 0; i < compData->tracepointCount; i++) {
				if (compData->tracepointcounters[i] > 0) {
					if (f < 0) {
						omrtty_err_printf("%s.%d %ld \n", compData->qualifiedComponentName, i, compData->tracepointcounters[i]);
					} else {
						omrstr_printf(tempBuf, TEMPBUFLEN, "%s.%d %lld \n", compData->qualifiedComponentName, i, compData->tracepointcounters[i]);
						/* convert to ebcdic if on zos */
						omrfile_write_text(f, tempBuf, strlen(tempBuf));
					}
				}
			}
		}
		compData = compData->next;
	}

	/* list the unloaded components */
	compData = OMR_TRACEGLOBAL(unloadedComponentList)->head;
	while (compData != NULL) {
		if (compData->tracepointcounters != NULL) {
			for (i = 0; i < compData->tracepointCount; i++) {
				if (compData->tracepointcounters[i] > 0) {
					if (f < 0) {
						omrtty_err_printf("%s.%d %ld \n", compData->qualifiedComponentName, i, compData->tracepointcounters[i]);
					} else {
						omrstr_printf(tempBuf, TEMPBUFLEN, "%s.%d %lld \n", compData->qualifiedComponentName, i, compData->tracepointcounters[i]);
						/* convert to ebcdic if on zos */
						omrfile_write_text(f, tempBuf, strlen(tempBuf));
					}
				}
			}
		}
		compData = compData->next;
	}

	if (f > 0) {
		omrfile_close(f);
	}
#undef TEMPBUFLEN
}

/*******************************************************************************
 * name        - getTimestamp
 * description - Get the time in various component units
 * parameters  - pointers to result for hours, minutes, seconds and milliseconds
 * returns     - void
 *
 ******************************************************************************/
void
getTimestamp(int64_t time, uint32_t *pHours, uint32_t *pMinutes, uint32_t *pSeconds, uint32_t *pMillis)
{
	uint32_t hours, minutes, seconds, millis;

#define MILLIS2SECONDS 1000
#define SECONDS2MINUTES 60
#define MINUTES2HOURS 60
#define HOURS2DAYS 24

	seconds = (uint32_t)(time / MILLIS2SECONDS);

	millis = (uint32_t)(time % MILLIS2SECONDS);

	minutes = seconds / SECONDS2MINUTES;
	seconds = seconds % SECONDS2MINUTES;

	hours = minutes / MINUTES2HOURS;
	minutes = minutes % MINUTES2HOURS;

	hours = hours % HOURS2DAYS;

	*pHours = hours;
	*pMinutes = minutes;
	*pSeconds = seconds;
	*pMillis = millis;

#undef MILLIS2SECONDS
#undef SECONDS2MINUTES
#undef MINUTES2HOURS
#undef HOURS2DAYS
}

/*******************************************************************************
 * name        - addTraceConfig
 * description - add trace configuration command to list stored in utglobal data
 * parameters  - OMR_TraceThread, trace configuration command
 * returns     - return code
 ******************************************************************************/
omr_error_t
addTraceConfig(OMR_TraceThread *thr, const char *cmd)
{
	UtTraceCfg *tmp;
	omr_error_t rc = OMR_ERROR_NONE;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	/*
	 *  Obtain and initialize a config buffer
	 */
	UtTraceCfg *cfg = (UtTraceCfg *)omrmem_allocate_memory(sizeof(UtTraceCfg) + strlen(cmd) + 1, OMRMEM_CATEGORY_TRACE);
	if (NULL != cfg) {
		initHeader(&cfg->header, UT_TRACE_CONFIG_NAME, sizeof(UtTraceCfg) +
				   strlen(cmd) + 1);
		cfg->next = NULL;
		strcpy(cfg->command, cmd);
		/*
		 * Add it to the end of config chain while holding traceLock
		 */
		checkGetTraceLock(thr);
		if ((tmp = OMR_TRACEGLOBAL(config)) == NULL) {
			OMR_TRACEGLOBAL(config) = cfg;
		} else {
			while (tmp->next != NULL) {
				tmp = tmp->next;
			}
			tmp->next = cfg;
		}

		/* We should update the trace header here (if it has already been initialised) to
		 * reflect the change in trace settings.
		 * Unfortunately it may be in use by a snap dump or we may have returned a pointer to
		 * it via trcGetTraceMetadata so freeing it or nulling it is unsafe.
		 * For now we are leaving it unchanged.
		 * If nothing has requested the trace header yet and it is NULL then it will be created
		 * correctly when it is first used.
		 */

		checkFreeTraceLock(thr);
	} else {
		UT_DBGOUT(1, ("<UT> Out of memory in addTraceConfig\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	return rc;
}

/*******************************************************************************
 * name        - addTraceConfigKeyValuePair
 * description - add trace configuration command key/value pair to list
 * parameters  - OMR_TraceThread, trace configuration command key, trace configuration
 * 			value
 * 			value can safely be null, but null key is an error.
 * returns     - OMR_ERROR_NONE if it worked,
 * 			OMR_ERROR_OUT_OF_NATIVE_MEMORY or OMR_ERROR_ILLEGAL_ARGUMENT if problem
 ******************************************************************************/
omr_error_t
addTraceConfigKeyValuePair(OMR_TraceThread *thr, const char *cmdKey, const char *cmdValue)
{
	uintptr_t cmdLen = 1; /* terminating null */
	char *cmd = NULL;
	omr_error_t rc = OMR_ERROR_NONE;
	int32_t addBraces = FALSE;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	/* could use a sprintf equivalent, but would have to mock up empty strings
	 * for cases where cmdValue was null
	 */
	if (cmdKey != NULL) {
		cmdLen += strlen(cmdKey);
	} else {
		UT_DBGOUT(1, ("<UT> Out of memory recording option : \"%s\"\n", cmdKey));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (cmdValue != NULL) {
		cmdLen += strlen("=");
		cmdLen += strlen(cmdValue);
		if (strchr(cmdValue, ',') != NULL) {
			/* we have a comma separated item whose braces will have been stripped */
			addBraces = TRUE;
			cmdLen += strlen("{}");
		}
	}

	cmd = (char *)omrmem_allocate_memory(cmdLen, OMRMEM_CATEGORY_TRACE);
	if (NULL == cmd) {
		UT_DBGOUT(1, ("<UT> Out of memory recording option : \"%s\"\n", cmdKey));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	if (cmdKey != NULL) {
		strcpy(cmd, cmdKey);
	}
	if (cmdValue != NULL) {
		strcat(cmd, "=");
		if (addBraces == TRUE) {
			strcat(cmd, "{");
		}
		strcat(cmd, cmdValue);
		if (addBraces == TRUE) {
			strcat(cmd, "}");
		}
	}

	rc = addTraceConfig(thr, cmd);
	omrmem_free_memory(cmd);
	return rc;
}



/*******************************************************************************
 * name        - initHeader
 * description - Header initialization routine
 * parameters  - UtDataHeader, name and size
 * returns     - None
 ******************************************************************************/
void
initHeader(UtDataHeader *header, const char *name, uintptr_t size)
{
	memcpy(header->eyecatcher, name, 4);
	header->length = (int32_t)size;
	header->version = UT_VERSION;
	header->modification = UT_MODIFICATION;
}

/*******************************************************************************
 * name        - hexStringLength
 * description - Return the number of consecutive hex characters
 * parameters  - string pointer
 * returns     - Length of hex string
 ******************************************************************************/
int
hexStringLength(const char *str)
{
	int i;
	for (i = 0;
		 ((str[i] >= '0' &&
		   str[i] <= '9') ||
		  (str[i] >= 'A' &&
		   str[i] <= 'F') ||
		  (str[i] >= 'a' &&
		   str[i] <= 'f'));
		 i++) {
	}
	return i;
}
