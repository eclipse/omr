/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include "omrcomp.h"
#include "omrport.h"
#include "omrrasinit.h"
#include "omrtrace.h"
#include "ut_omrvm.h"

#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrti.h"


#define OMR_TRACE_OPT_DELIM ':'
#define OMR_TRACE_OPTS_SIZE 55

#define OMR_TRACE_ENABLE_DEBUG 1

#if OMR_TRACE_ENABLE_DEBUG
#undef NDEBUG
#include <assert.h>
#endif /* OMR_TRACE_ENABLE_DEBUG */

/*
 * OMR trace uses these environment variables:
 *
 *	 OMR_TRACE_OPTIONS
 *	 - Passes startup options to trace engine.
 *	 - e.g. OMR_TRACE_OPTIONS=maximal=all
 *
 *	 TRACEDEBUG
 *	 - Enables trace debug code if set to "1".
 *	 - e.g. TRACEDEBUG=1
 *	 - Support for this env var is enabled by the compile flag OMR_TRACE_ENABLE_DEBUG.
 */

static omr_error_t freeTraceOptions(OMRPortLibrary *portLibrary, char *trcOpts[]);
static omr_error_t getEnvVar(OMRPortLibrary *portLibrary, const char *envVar, char *buf, size_t bufLen, const char **envVarValue);
static BOOLEAN keyNotNull(const char *key);
static omr_error_t parseTraceOptions(OMRPortLibrary *portLibrary, char *trcOpts[], const char *traceOptString);
static void reportOMRCommandLineError(OMRPortLibrary *portLibrary, const char *detailStr, va_list args);

omr_error_t
omr_ras_initTraceEngine(OMR_VM *omrVM, const char *traceOptString, const char *datDir)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	OMR_TraceLanguageInterface languageIntf;
	char *trcOpts[OMR_TRACE_OPTS_SIZE];

	rc = parseTraceOptions(OMRPORTLIB, trcOpts, traceOptString);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf(
			"parseTraceOptions error, rc=%d, traceOptString=\"%s\"\n",
			rc, (NULL == traceOptString) ? "(NULL)" : traceOptString);
		goto done;
	}

	memset(&languageIntf, 0, sizeof(languageIntf));
	languageIntf.ReportCommandLineError = reportOMRCommandLineError;

	if ((NULL == datDir) || ('\0' == *datDir)) {
		datDir = ".";
	}

	rc = omr_trc_startup(omrVM, &languageIntf, datDir, (const char **)trcOpts);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("omr_trc_startup error, rc=%d\n", rc);
		goto done;
	}
	rc = freeTraceOptions(OMRPORTLIB, trcOpts);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("freeTraceOptions error, rc=%d\n", rc);
		goto done;
	}

	/* Load module for port library tracepoints */
	omrport_control(OMRPORT_CTLDATA_TRACE_START, (uintptr_t)omrVM->_trcEngine->utIntf);

	/* Load module for thread library tracepoints */
	omrthread_lib_control(J9THREAD_LIB_CONTROL_TRACE_START, (uintptr_t)omrVM->_trcEngine->utIntf);

	UT_OMRVM_MODULE_LOADED(omrVM->_trcEngine->utIntf);
	UT_OMRTI_MODULE_LOADED(omrVM->_trcEngine->utIntf);

	omr_trc_startMultiThreading(omrVM);
done:
	return rc;
}

omr_error_t
omr_ras_cleanupTraceEngine(OMR_VMThread *currentThread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRTraceEngine *trcEngine = currentThread->_vm->_trcEngine;
	if (NULL != trcEngine) {
		OMRPORT_ACCESS_FROM_OMRVMTHREAD(currentThread);
		UtInterface *utIntf = trcEngine->utIntf;

		/* stop tracing the portlib */
		omrport_control(OMRPORT_CTLDATA_TRACE_STOP, (uintptr_t)utIntf);

		/* stop tracing in the thread library */
		omrthread_lib_control(J9THREAD_LIB_CONTROL_TRACE_STOP, (uintptr_t)utIntf);

		UT_OMRTI_MODULE_UNLOADED(utIntf);
		UT_OMRVM_MODULE_UNLOADED(utIntf);

		rc = omr_trc_shutdown(currentThread);
	}
	return rc;
}

static BOOLEAN
keyNotNull(const char *key)
{
	return (NULL != key && ('\0' != *key));
}

static omr_error_t
freeTraceOptions(OMRPortLibrary *portLibrary, char *trcOpts[])
{
	omr_error_t rc = OMR_ERROR_NONE;
	if (NULL != trcOpts) {
		OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
		omrmem_free_memory(trcOpts[0]);
	}
	return rc;
}

/*
 * Format of TRACEOPTS environment variaable is
 *	   -trace:key1=val1:key2=val2:...
 */
static omr_error_t
parseTraceOptions(OMRPortLibrary *portLibrary, char *trcOpts[], const char *traceOptString)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	char *copyOfOptString = NULL;
	uintptr_t i = 0;
	uintptr_t optIndex = 0;

	trcOpts[0] = NULL;

	if ((NULL != traceOptString) && ('\0' != *traceOptString)) {
		const char *firstKey = traceOptString;
		char *key = NULL;

		/* strip extra :'s */
		while (OMR_TRACE_OPT_DELIM == *firstKey) {
			firstKey += 1;
		}
		if ('\0' == *firstKey) {
			goto done;
		}

		copyOfOptString = omrmem_allocate_memory(strlen(firstKey) + 1, OMRMEM_CATEGORY_TRACE);
		if (NULL == copyOfOptString) {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			goto done;
		}
		/* copy stripped traceOptString into copyOfOptString */
		memcpy(copyOfOptString, firstKey, strlen(firstKey) + 1);

		/* trcOpts[0] must point to the start of allocated memory so that freeTraceOptions() works correctly */
		key = copyOfOptString;
		/* assert('=' != *key); */
		/* assert(OMR_TRACE_OPT_DELIM != *key); */
		while (keyNotNull(key)) {
			if ('=' == *key) {
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				/* Setting key to NULL exits the while loop */
				key = NULL;

			} else if (OMR_TRACE_OPT_DELIM == *key) {
				/* Skip this character and continue looping */
				key += 1;

			} else {
				char *value = NULL;
				char *nextKey = NULL;

				/* ensure we have enough room in the trcOpts array for a key/value pair and the NULL terminator */
				if ((optIndex + 3) > OMR_TRACE_OPTS_SIZE) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}

				value = strchr(key + 1, '=');
				nextKey = strchr(key + 1, OMR_TRACE_OPT_DELIM);

				if (NULL == nextKey) {
					/* looking at the last key/value pair */

					/* set delimiter '=' to '\0' if found */
					if (NULL != value) {
						*value = '\0';
						value += 1;
					}
					trcOpts[optIndex] = key;
					optIndex += 1;
					trcOpts[optIndex] = value;
					optIndex += 1;

					key = NULL;
				} else {
					if (NULL != value) {
						if (value < nextKey) {
							*value = '\0';
							value += 1;
						} else {
							value = NULL;
						}
					}
					*nextKey = '\0';

					trcOpts[optIndex] = key;
					optIndex += 1;
					trcOpts[optIndex] = value;
					optIndex += 1;

					key = nextKey + 1;
				}
			}
		}

		if (OMR_ERROR_NONE == rc) {
			/* last entry of array must be indicated by NULL */
			trcOpts[optIndex] = NULL;

			/* replace empty strings ("") with NULL entries */
			for (i = 0; i < optIndex; i++) {
				if (NULL != trcOpts[i]) {
					if ('\0' == trcOpts[i][0]) {
						trcOpts[i] = NULL;
					}
				}
			}
		} else {
			omrmem_free_memory(copyOfOptString);
		}
	}

done:
#if OMR_TRACE_ENABLE_DEBUG
	if (OMR_ERROR_NONE == rc) {
		/* Trace assertions can't be used here because the trace engine has not been started yet. */
		const char *traceDebugEnv = NULL;
		char traceDebugBuf[2] = "";

		if (OMR_ERROR_NONE == getEnvVar(OMRPORTLIB, "TRACEDEBUG", traceDebugBuf, sizeof(traceDebugBuf), &traceDebugEnv)) {
			if ((NULL != traceDebugEnv) && ('1' == traceDebugEnv[0]) && ('\0' == traceDebugEnv[1])) {
				for (i = 0; i < optIndex; i += 2) {
					omrtty_printf("%d: key = %s\n", i, (NULL == trcOpts[i]) ? NULL : trcOpts[i]);
					omrtty_printf("%d: val = %s\n", i + 1, (NULL == trcOpts[i + 1]) ? NULL : trcOpts[i + 1]);

					assert(NULL != trcOpts[i]);			/* key must not be NULL */
					assert('\0' != trcOpts[i][0]);		/* key must not be empty string */
					assert((NULL == trcOpts[i + 1])		/* value must be NULL or non-empty string */
						   || ('\0' != trcOpts[i + 1][0]));
				}

				assert(NULL == trcOpts[optIndex]);		/* The last element of trcOpts must be NULL */
				assert(optIndex < OMR_TRACE_OPTS_SIZE); /* Don't write past the end of the trcOpts array */
				if (optIndex > 0) {
					assert(trcOpts[0] == copyOfOptString);	/* trcOpts[0] must point to the start of the memory block */
				}
			}
			if ((NULL != traceDebugEnv) && (traceDebugEnv != traceDebugBuf)) {
				omrmem_free_memory((void *)traceDebugEnv);
				traceDebugEnv = NULL;
			}
		}
	}
#endif /* OMR_TRACE_ENABLE_DEBUG */
	return rc;
}

static void
reportOMRCommandLineError(OMRPortLibrary *portLibrary, const char *detailStr, va_list args)
{
	char buffer[1024];
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	omrstr_vprintf(buffer, sizeof(buffer), detailStr, args);

	omrtty_err_printf("Invalid option: %s\n", buffer);
}


static omr_error_t
getEnvVar(OMRPortLibrary *portLibrary, const char *envVar, char *buf, size_t bufLen, const char **envVarValue)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	omr_error_t rc = OMR_ERROR_NONE;

	intptr_t portRc = omrsysinfo_get_env(envVar, buf, bufLen);
	if (-1 == portRc) {
		/* env var wasn't found */
		*envVarValue = NULL;
	} else if (0 == portRc) {
		/* env var value was obtained */
		*envVarValue = buf;
	} else {
		/* buffer was too small */
		char *newBuf = omrmem_allocate_memory(portRc, OMRMEM_CATEGORY_VM);
		if (NULL != newBuf) {
			if (0 == omrsysinfo_get_env(envVar, newBuf, portRc)) {
				*envVarValue = newBuf;
			} else {
				omrmem_free_memory(newBuf);
				rc = OMR_ERROR_INTERNAL;
			}
		} else {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
	}
	return rc;
}

