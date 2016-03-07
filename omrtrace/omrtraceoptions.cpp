/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1998, 2015
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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "omrutil.h"

#include "omrtrace_internal.h"

typedef omr_error_t (*OMR_TraceOptionFunction)(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);

/**
 * Structure representing a trace option such maximal or debug
 */
typedef struct OMR_TraceOption {
	const char *name;
	const int32_t runtimeModifiable;
	const OMR_TraceOptionFunction optionFunction;
} OMR_TraceOption;

/* Prototypes for option configuration functions
 *
 * All of these functions require omrTraceGlobal to be initialized.
 *
 * thr is used to mutex runtime option updates.
 *
 * During the trace engine's initialization phase, when no threads have yet
 * attached to the trace engine, a NULL thr value may provided. In this case,
 * no mutexing will be performed.
 */
static omr_error_t setMinimal(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setMaximal(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setCount(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setPrint(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setNone(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setIprint(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setException(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
#if OMR_ALLOW_OUTPUT_OPTION
static omr_error_t setOutput(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
#endif /* OMR_ALLOW_OUTPUT_OPTION */
static omr_error_t setBuffers(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t setSuspendResumeCount(OMR_TraceThread *thr, const char *value, int32_t resume, BOOLEAN atRuntime);
static omr_error_t processSuspendOption(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t processResumeOption(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static omr_error_t processSuspendCountOption(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime);
static int selectComponent(const char *cmd, int32_t *first, char traceType, int32_t setActive, BOOLEAN atRuntime);
static omr_error_t setFatalAssert(OMR_TraceThread *thr, const char *spec, BOOLEAN atRuntime);
static omr_error_t clearFatalAssert(OMR_TraceThread *thr, const char *spec, BOOLEAN atRuntime);

static const char *getPositionalParm(int pos, const char *string, int *size);
static int getParmNumber(const char *string);
static int decimalString2Int(const char *decString, int32_t signedAllowed, omr_error_t *rc, BOOLEAN atRuntime);


const OMR_TraceOption UTE_OPTIONS[] = {
	/* { Keyword, Can be configured at runtime, Configuration function } */

	/* These four don't have any action */
	{UT_DEBUG_KEYWORD, FALSE, NULL},
	{UT_SUFFIX_KEYWORD, FALSE, NULL},
	{UT_LIBPATH_KEYWORD, FALSE, NULL},
	{UT_FORMAT_KEYWORD, FALSE, NULL},

	{UT_MINIMAL_KEYWORD, TRUE, setMinimal},
	{UT_MAXIMAL_KEYWORD, TRUE, setMaximal},
	{UT_EXCEPTION_KEYWORD, TRUE, setException},
	{UT_COUNT_KEYWORD, TRUE, setCount},
	{UT_PRINT_KEYWORD, TRUE, setPrint},
	{UT_NONE_KEYWORD, TRUE, setNone},
	{UT_IPRINT_KEYWORD, TRUE, setIprint},
#if OMR_ALLOW_OUTPUT_OPTION
	{UT_OUTPUT_KEYWORD, FALSE, setOutput},
#endif /* OMR_ALLOW_OUTPUT_OPTION */
	{UT_BUFFERS_KEYWORD, TRUE, setBuffers}, /* Not all buffers functions are exposed - but are controlled in the set function*/
	{UT_SUSPEND_KEYWORD, TRUE, processSuspendOption},
	{UT_RESUME_KEYWORD, TRUE, processResumeOption},
	{UT_RESUME_COUNT_KEYWORD, TRUE, processResumeOption},
	{UT_SUSPEND_COUNT_KEYWORD, TRUE, processSuspendCountOption},
	{UT_FATAL_ASSERT_KEYWORD, TRUE, setFatalAssert},
	{UT_NO_FATAL_ASSERT_KEYWORD, TRUE, clearFatalAssert},
};

#define NUMBER_OF_UTE_OPTIONS (sizeof(UTE_OPTIONS) / sizeof(UTE_OPTIONS[0]))

/*******************************************************************************
 * name        - addTraceCmd
 * description - Internal routine to setup a trace cmd
 * parameters  - thr, trace command and value
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
addTraceCmd(OMR_TraceThread *thr, const char *cmd, const char *value, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	char *str;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	str = (char *)omrmem_allocate_memory(strlen(cmd) + (value == NULL ? 1 :
										 strlen(value) + 2), OMRMEM_CATEGORY_TRACE);
	if (str != NULL) {
		strcpy(str, cmd);
		if (value != NULL && strlen(value) > 0) {
			strcat(str, "=");
			strcat(str, value);
		}

		/* Set the trace state under the global trace lock */
		checkGetTraceLock(thr);
		rc = setTraceState(str, atRuntime);
		checkFreeTraceLock(thr);

		omrmem_free_memory(str);
	} else {
		UT_DBGOUT(1, ("<UT> Out of memory in addTraceCmd\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	return rc;
}

static omr_error_t
parseBufferSize(const char *const str, const int argSize, BOOLEAN atRuntime)
{
	/* It's either invalid input or a number with an optional suffix
	 * Find the position of the first digit and non-digit character.
	 */
	int32_t newBufferSize;
	intptr_t firstNonDigit = -1;
	intptr_t firstDigit = -1;
	const char *p = str;

	while (*p) {
		if (isdigit(*p)) {
			if (-1 == firstDigit) {
				firstDigit = p - str;
			}
		} else {
			if (-1 == firstNonDigit) {
				firstNonDigit = p - str;
			}
		}

		p++;
	}

	/* The only valid place for a non-digit is the final character */
	if (firstNonDigit != -1) {
		if (firstNonDigit == (argSize - 1) && firstDigit != -1) {
			int multiplier = 1;
			switch (j9_cmdla_toupper(str[argSize - 1])) {
			case 'K':
				multiplier = 1024;
				break;
			case 'M':
				multiplier = 1024 * 1024;
				break;
			default:
				reportCommandLineError(atRuntime, "Unrecognised suffix %c specified for buffer size", str[argSize - 1]);
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}

			newBufferSize = atoi(str) * multiplier;
		} else {
			/* Invalid */
			reportCommandLineError(atRuntime, "Invalid option for -Xtrace:buffers - \"%s\"", str);
			return OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	} else {
		/* The string contains no non-digits */
		newBufferSize = atoi(str);
	}

	if (newBufferSize < UT_MINIMUM_BUFFERSIZE) {
		reportCommandLineError(atRuntime, "Specified buffer size %d bytes is too small. Minimum is %d bytes.", newBufferSize, UT_MINIMUM_BUFFERSIZE);
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		OMR_TRACEGLOBAL(bufferSize) = newBufferSize;
	}

	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - setBuffers
 * description - Set the buffer size and type
 * parameters  - thr, string value of the property (nnnk|nnnm[,dynamic]), atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setBuffers(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	char *localBuffer = NULL;
	omr_error_t rc = OMR_ERROR_NONE;
	const int numberOfArgs = getParmNumber(value);
	int i;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	if (NULL == value) {
		reportCommandLineError(atRuntime, "-Xtrace:buffers expects an argument.");
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	localBuffer = (char *)omrmem_allocate_memory(strlen(value) + 1, OMRMEM_CATEGORY_TRACE);
	if (NULL == localBuffer) {
		UT_DBGOUT(1, ("<UT> Out of memory in setBuffers\n"));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	for (i = 0; i < numberOfArgs; i++) {
		int argSize = 0;
		const char *startOfThisArg = getPositionalParm(i + 1, value, &argSize);

		if (argSize == 0) {
			reportCommandLineError(atRuntime, "Empty option passed to -Xtrace:buffers");
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			goto end;
		}

		strncpy(localBuffer, startOfThisArg, argSize);
		localBuffer[argSize] = '\0';

		if (j9_cmdla_stricmp(localBuffer, "DYNAMIC") == 0) {
			OMR_TRACEGLOBAL(dynamicBuffers) = TRUE;
		} else if (j9_cmdla_stricmp(localBuffer, "NODYNAMIC") == 0) {
			OMR_TRACEGLOBAL(dynamicBuffers) = FALSE;
		} else {
			if (!atRuntime) {
				rc = parseBufferSize(localBuffer, argSize, atRuntime);

				if (rc != OMR_ERROR_NONE) {
					goto end;
				}
			} else {
				/* You can't change the buffer size at runtime */
				UT_DBGOUT(1, ("<UT> Buffer size cannot be changed at run-time\n"));
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				goto end;
			}
		}
	}

	UT_DBGOUT(1, ("<UT> Trace buffer size: %d\n", OMR_TRACEGLOBAL(bufferSize)));

end:
	if (localBuffer != NULL) {
		omrmem_free_memory(localBuffer);
	}

	return rc;
}

/*******************************************************************************
 * name        - setMinimal
 * description - Set the minimal trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setMinimal(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_MINIMAL_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setMaximal
 * description - Set the maximal trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setMaximal(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_MAXIMAL_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setCount
 * description - Set the counter trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setCount(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	OMR_TRACEGLOBAL(traceCount) = TRUE;
	return addTraceCmd(thr, UT_COUNT_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setPrint
 * description - Set the printf trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setPrint(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_PRINT_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setNone
 * description - Set the "off" trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setNone(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_NONE_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setIprint
 * description - Set the indented printf trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setIprint(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_IPRINT_KEYWORD, value, atRuntime);
}

/*******************************************************************************
 * name        - setException
 * description - Set exception trace options
 * parameters  - thr, trace options, atRuntime
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setException(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return addTraceCmd(thr, UT_EXCEPTION_KEYWORD, value, atRuntime);
}

#if OMR_ALLOW_OUTPUT_OPTION
/*******************************************************************************
 * name        - setOutput
 * description - Set the output filename and options
 * parameters  - thr, string value of the property
 *               (filename[,nnnm][,generations]])
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
setOutput(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return OMR_ERROR_NONE;
}
#endif /* OMR_ALLOW_OUTPUT_OPTION */

/*******************************************************************************
 * name        - setFormat
 * description - Set the formatting file path
 * parameters  - datDir
 * returns     - UTE return code
 ******************************************************************************/
omr_error_t
setFormat(const char *datDir)
{
	omr_error_t rc = OMR_ERROR_NONE;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	OMR_TRACEGLOBAL(traceFormatSpec) = (char *)omrmem_allocate_memory(strlen(datDir) + 1, OMRMEM_CATEGORY_TRACE);
	if (OMR_TRACEGLOBAL(traceFormatSpec) != NULL) {
		strcpy(OMR_TRACEGLOBAL(traceFormatSpec), datDir);
	} else {
		UT_DBGOUT(1, ("<UT> Out of memory for FormatSpecPath\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	return rc;
}

/**************************************************************************
 * name        - setSuspendResumeCount
 * description - This handles the resumecount and suspendcount properties.
 *               It sets the value initialSuspendResume in OMR_TraceGlobal
 *               This is read by all starting threads and used as their
 *               initial value for thread suspend/resume.  If at any given
 *               time,  it is 0 or greater, trace is enabled for that
 *               thread.
 * parameters  - thr, trace options, Resume (TRUE=resume, FALSE=suspend)
 * returns     - UTE return code
 *************************************************************************/
omr_error_t
setSuspendResumeCount(OMR_TraceThread *thr, const char *value, int32_t resume, BOOLEAN atRuntime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const char *p;
	int length;
	int maxLength = 5;

	/*
	 * check for basic formatting errors
	 */
	p = getPositionalParm(1, value, &length);

	if (getParmNumber(value) != 1) {  /* problem if not just the one parm */
		rc = OMR_ERROR_INTERNAL;
	} else if (length == 0) {         /* first (only) parm must be non null */
		rc = OMR_ERROR_INTERNAL;
	}

	if (OMR_ERROR_NONE == rc) {               /* max 5 chars long            */
		if (*p == '+' || *p == '-') { /* but must allow 6 if signed  */
			maxLength++;
		}
		if (length > maxLength) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	/*
	 * If any of the above checks failed, issue message.
	 */
	if (OMR_ERROR_NONE != rc) {
		if (resume) {
			reportCommandLineError(atRuntime, "resumecount takes a single integer value from -99999 to +99999");
		} else {
			reportCommandLineError(atRuntime, "suspendcount takes a single integer value from -99999 to +99999");
		}
	}

	if (OMR_ERROR_NONE == rc) {
		/*
		 * Enforce that users may not use resumecount and suspendcount together !
		 */
		if (OMR_TRACEGLOBAL(initialSuspendResume) != 0) {
			reportCommandLineError(atRuntime, "resumecount and suspendcount may not both be set.");
			rc = OMR_ERROR_INTERNAL;
		} else {
			/* issues error and sets rc if p points to bad number */
			int value = decimalString2Int(p, TRUE, &rc, atRuntime);

			if (OMR_ERROR_NONE == rc) {
				if (resume) {
					OMR_TRACEGLOBAL(initialSuspendResume) = (0 - value);
				} else {
					OMR_TRACEGLOBAL(initialSuspendResume) = (value - 1);
				}
			}
		}
	}

	if (NULL != thr) {
		/* the current thread (presumably the primordial thread) needs updating too */
		thr->suspendResume = OMR_TRACEGLOBAL(initialSuspendResume);
	} else {
		UT_ASSERT(OMR_TRACE_ENGINE_MT_ENABLED != OMR_TRACEGLOBAL(initState));
	}
	return rc;
}

/*******************************************************************************
 * name        - processEarlyOptions
 * description - Process the startup
 * parameters  - char **options
 * returns     - Nothing
 ******************************************************************************/
omr_error_t
processEarlyOptions(const char **opts)
{
	int i;
	omr_error_t rc = OMR_ERROR_NONE;

	/*
	 *  Process the options passed
	 */


	/*
	 *  Options are in name / value pairs, and termainate with a NULL
	 */

	for (i = 0; opts[i] != NULL; i += 2) {
		/* DEBUG is handled elsewhere, SUFFIX and LIBPATH are obsolete. */
		if (j9_cmdla_stricmp((char *)opts[i], UT_DEBUG_KEYWORD) == 0) {
		} else if (j9_cmdla_stricmp((char *)opts[i], UT_SUFFIX_KEYWORD) == 0) {
		} else if (j9_cmdla_stricmp((char *)opts[i], UT_LIBPATH_KEYWORD) == 0) {
		} else if (j9_cmdla_stricmp((char *)opts[i], UT_FORMAT_KEYWORD) == 0) {
			if (opts[i + 1] == NULL) {
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}
			rc = setFormat(opts[i + 1]);
		} else {
			UT_DBGOUT(1, ("<UT> EarlyOptions skipping :%s\n", opts[i]));
		}
	}
	return rc;
}

static omr_error_t
processSuspendCountOption(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return setSuspendResumeCount(thr, value, FALSE, atRuntime);
}

static omr_error_t
processResumeOption(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	return setSuspendResumeCount(thr, value, TRUE, atRuntime);
}

static omr_error_t
processSuspendOption(OMR_TraceThread *thr, const char *value, BOOLEAN atRuntime)
{
	OMR_TRACEGLOBAL(traceSuspend) = UT_SUSPEND_USER;
	return OMR_ERROR_NONE;
}

static omr_error_t
setFatalAssert(OMR_TraceThread *thr, const char *spec, BOOLEAN atRuntime)
{
	OMR_TRACEGLOBAL(fatalassert) = 1;
	return OMR_ERROR_NONE;
}

static omr_error_t
clearFatalAssert(OMR_TraceThread *thr, const char *spec, BOOLEAN atRuntime)
{
	OMR_TRACEGLOBAL(fatalassert) = 0;
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - processOptions
 * description - Process the startup
 * parameters  - OMR_TraceThread, char **options, atRuntime
 * returns     - Nothing
 ******************************************************************************/
omr_error_t
processOptions(OMR_TraceThread *thr, const char **opts, BOOLEAN atRuntime)
{
	int i;
	omr_error_t rc = OMR_ERROR_NONE;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	/*
	 *  Check options for the debug switch. Options are in name / value pairs
	 *  so only look at the first of each pair
	 */

	if (!atRuntime) {
		for (i = 0; opts[i] != NULL; i += 2) {
			if (j9_cmdla_stricmp((char *)opts[i], UT_DEBUG_KEYWORD) == 0) {
				if (opts[i + 1] != NULL &&
					strlen(opts[i + 1]) == 1 &&
					*opts[i + 1] >= '0' &&
					*opts[i + 1] <= '9'
				) {
					OMR_TRACEGLOBAL(traceDebug) = atoi(opts[i + 1]);
				} else {
					OMR_TRACEGLOBAL(traceDebug) = 9;
				}
				UT_DBGOUT(1, ("<UT> Debug information requested\n"));
			}
		}
	}

	/* NULL is allowed for thr only during trace initialization, when only one thread
	 * can be accessing the trace engine.
	 */
	if ((NULL == thr) && (OMR_TRACE_ENGINE_MT_ENABLED == OMR_TRACEGLOBAL(initState))) {
		UT_DBGOUT(1, ("<UT> processOptions invoked with NULL thread\n"));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/*
	 *  Process the options passed
	 *  Options are in name / value pairs, and terminate with a NULL
	 */

	for (i = 0; opts[i] != NULL; i += 2) {
		int32_t found = 0;
		const char *optName = opts[i];
		char *optValue = (char *)opts[i + 1];
		size_t optNameLen = strlen(optName);
		UT_DBGOUT(1, ("<UT> Processing option %s=%s\n", optName, (optValue == NULL)? "NULL" : optValue));

		if (NULL != optValue) {
			size_t optValueLen = strlen(optValue);
			/* Values enclosed in {...} should have the brackets removed */
			if (optValue[0] == '{' && optValue[optValueLen - 1] == '}') {
				optValue = (char *)omrmem_allocate_memory(optValueLen - 1, OMRMEM_CATEGORY_TRACE);
				if (!optValue) {
					rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
					break;
				}
				strncpy(optValue, opts[i + 1] + 1, optValueLen - 2);
				optValue[optValueLen - 2] = '\0';
			}
		}

		for (size_t j = 0; j < NUMBER_OF_UTE_OPTIONS; j++) {
			if (optNameLen == strlen(UTE_OPTIONS[j].name) && 0 == j9_cmdla_stricmp((char *)optName, (char *)UTE_OPTIONS[j].name)) {
				found = 1;

				if (atRuntime && ! UTE_OPTIONS[j].runtimeModifiable) {
					reportCommandLineError(atRuntime, "Option \"%s\" cannot be set at run-time. Set it on the command line at start-up.", optName);

					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					if (NULL != UTE_OPTIONS[j].optionFunction) {
						rc = UTE_OPTIONS[j].optionFunction(thr, optValue, atRuntime);
					}
				}

				break;
			}
		}

		if (! found) {
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}


		if (OMR_ERROR_NONE != rc) {
			break;
		} else {
			/* Add to trace config if the option was valid and actually set. */
			addTraceConfigKeyValuePair(thr, optName, optValue);
		}

		/* Check if we allocated a new string for optValue and free */
		if (optValue != opts[i + 1]) {
			omrmem_free_memory(optValue);
		}
	}

	return rc;
}

/**************************************************************************
 * name        - reportCommandLineError
 * description - Report an error in a command line option and put the given
 * 			   - string in as detail in an NLS enabled message.
 * parameters  - detailStr, args for formatting into detailStr.
 *             - suppressMessages, a flag that says whether messages should
 *               be displayed at the moment.
 *************************************************************************/
void
reportCommandLineError(BOOLEAN suppressMessages, const char *detailStr, ...)
{
	OMR_TraceCommandLineErrorFunc errorCallback = OMR_TRACEGLOBAL(languageIntf).ReportCommandLineError;

	if (NULL != errorCallback && !suppressMessages) {
		va_list arg_ptr;
		va_start(arg_ptr, detailStr);
		errorCallback(OMR_TRACEGLOBAL(portLibrary), detailStr, arg_ptr);
		va_end(arg_ptr);
	}
}

/**************************************************************************
 * name        - selectSpecial
 * description - Selects special option - provides argument parsing only to
 * 				 allow backwards command line compatability.
 * parameters  - Component name, component array,
 * returns     - size of the match
 *************************************************************************/
static int
selectSpecial(const char *string)
{
#define DEFAULT_BACKTRACE_LEVEL 4
	int depth = DEFAULT_BACKTRACE_LEVEL;
	const char *p;

	UT_DBGOUT(2, ("<UT> selectSpecial: %s\n", string));
	p = string;
	if (*p == '\0') {
		return (0);
	}
	if ((0 == j9_cmdla_strnicmp(p, UT_BACKTRACE, strlen(UT_BACKTRACE))) &&
		((p[strlen(UT_BACKTRACE)] == ',') ||
		 (p[strlen(UT_BACKTRACE)] == '\0'))
	) {
		UT_DBGOUT(3, ("<UT> Backtrace specifier found\n"));
		p += strlen(UT_BACKTRACE);
		if ((*p == ',') && ((*(p + 1) >= '0') && (*(p + 1) <= '9'))) {
			depth = 0;
			p++;
			while ((*p != '\0') && (*p >= '0') && (*p <= '9')) {
				depth *= 10;
				depth += (*p - '0');
				p++;
			}
		}
		UT_DBGOUT(3, ("<UT> Depth set to %d\n", depth));
	}
	if (*p == ',') {
		p++;
	}
	return ((int)(p - string));
}


/*******************************************************************************
 * name        - selectComponent
 * description - Selects a component name
 * parameters  - Component name, component array, first time
  *              through flag
 * returns     - size of the match
 ******************************************************************************/
static int
selectComponent(const char *cmd, int32_t *first, char traceType, int32_t setActive, BOOLEAN atRuntime)
{
	int length = 0;

	/*
	 *  If no component specified and first time through, default to all
	 */
	UT_DBGOUT(2, ("<UT> selectComponent: %s\n", cmd));
	if (*cmd == '\0') {
		if (*first) {
			omr_error_t rc = OMR_ERROR_NONE;

			UT_DBGOUT(1, ("<UT> Defaulting to All components\n"));
			/*
			 *  Set all components and applications, but ignore groups
			 */
			rc = setTracePointsTo((const char *) UT_ALL, OMR_TRACEGLOBAL(componentList), TRUE, 0, 0, traceType, -1, NULL, atRuntime, setActive);
			if (OMR_ERROR_NONE != rc) {
				UT_DBGOUT(1, ("<UT> Can't turn on all tracepoints\n"));
				return -1;
			}
		}
		*first = FALSE;
	} else {
		omr_error_t rc = OMR_ERROR_NONE;

		*first = FALSE;

		UT_DBGOUT(2, ("<UT> Component %s selected\n", cmd));
		rc = setTracePointsTo(cmd, OMR_TRACEGLOBAL(componentList), TRUE, 0, 0, traceType, -1, NULL, atRuntime, setActive);
		if (OMR_ERROR_NONE != rc) {
			UT_DBGOUT(1, ("<UT> Can't turn on all tracepoints\n"));
			return -1;
		}

		length = (int) strlen(cmd);

		if (length == 0) {
			length = -1;
		}
	}
	return length;
}

/*******************************************************************************
 * name        - setTraceState
 * description - Set trace activity state
 * parameters  - trace configuration command
 * returns     - return code
 ******************************************************************************/
omr_error_t
setTraceState(const char *cmd, BOOLEAN atRuntime)
{
	omr_error_t	rc = OMR_ERROR_NONE;
	int32_t	first = TRUE;
	int	context = UT_CONTEXT_INITIAL;
	char	traceType = UT_NONE;
	int	length;
	const char  *p;
	int32_t	explicitClass;
	int32_t	setActive = TRUE;

	/*
	 *  Initialize temporary boolean arrays to reflect whether a particular
	 *  component or class is the target of this command
	 */
	UT_DBGOUT(1, ("<UT> setTraceState %s\n", cmd));


	/*
	 * Parse the command using a finite state machine
	 *   Possible state transitions:
	 *
	 *   Initial > Component > Class > Process > Final
	 *                 V__________________V        |
	 *                 |___________________________|
	 */

	p = cmd;
	explicitClass = FALSE;
	while ((OMR_ERROR_NONE == rc) && (UT_CONTEXT_FINAL != context)) {
		switch (context) {
		/*
		 *  Start of line
		 */
		case UT_CONTEXT_INITIAL:
			UT_DBGOUT(2, ("<UT> setTraceState: Initial\n"));
			if (0 == j9_cmdla_strnicmp(p, UT_MINIMAL_KEYWORD, strlen(UT_MINIMAL_KEYWORD))) {
				if (!OMR_TRACE_ENGINE_IS_ENABLED(OMR_TRACEGLOBAL(initState))) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_MINIMAL;
				p += strlen(UT_MINIMAL_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_MAXIMAL_KEYWORD, strlen(UT_MAXIMAL_KEYWORD))) {
				if (!OMR_TRACE_ENGINE_IS_ENABLED(OMR_TRACEGLOBAL(initState))) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_MAXIMAL;
				p += strlen(UT_MAXIMAL_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_COUNT_KEYWORD, strlen(UT_COUNT_KEYWORD))) {
				if (!OMR_TRACE_ENGINE_IS_ENABLED(OMR_TRACEGLOBAL(initState))) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_COUNT;
				OMR_TRACEGLOBAL(traceCount) = TRUE;
				p += strlen(UT_COUNT_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_PRINT_KEYWORD, strlen(UT_PRINT_KEYWORD))) {
				traceType = UT_PRINT;
				OMR_TRACEGLOBAL(indentPrint) = FALSE;
				p += strlen(UT_PRINT_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_NONE_KEYWORD, strlen(UT_NONE_KEYWORD))) {
				traceType = UT_NONE;
				p += strlen(UT_NONE_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_IPRINT_KEYWORD, strlen(UT_IPRINT_KEYWORD))) {
				traceType = UT_PRINT;
				OMR_TRACEGLOBAL(indentPrint) = TRUE;
				p += strlen(UT_IPRINT_KEYWORD);
			} else if (0 == j9_cmdla_strnicmp(p, UT_EXCEPTION_KEYWORD, strlen(UT_EXCEPTION_KEYWORD))) {
				if (!OMR_TRACE_ENGINE_IS_ENABLED(OMR_TRACEGLOBAL(initState))) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
					break;
				}
				traceType = UT_EXCEPTION;
				p += strlen(UT_EXCEPTION_KEYWORD);
			} else {
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				break;
			}

			if (*p == '=') {
				p++;
			}

			if (*p == '\0' && traceType != UT_NONE) {
				/* Only the keyword has been specified and the option isn't -Xtrace:none */
				reportCommandLineError(atRuntime, "Option %s requires an argument.", cmd);
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}

			context = UT_CONTEXT_COMPONENT;
			break;

		/*
		 *  Looking for a component or TPID
		 */
		case UT_CONTEXT_COMPONENT:
			UT_DBGOUT(2, ("<UT> setTraceState: Component\n"));
			setActive = TRUE;

			if (*p == '!') {
				UT_DBGOUT(2, ("<UT> Reset of tracepoints requested\n"));
				p++;
				setActive = FALSE;
			}
			/* Parse deprecated option -Xtrace:maximal={backtrace[,depth]} */
			if ((length = selectSpecial(p)) != 0) {
				if (length < 0) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					p += length;
				}
			} else {
				/*
				 * Check for component specification
				 */

				length = selectComponent(p, &first,
										 traceType, setActive, atRuntime);
				if (length < 0) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					p += length;
					if (!first) {
						context = UT_CONTEXT_CLASS;
					}
					if (*p == '(') {
						p++;
						explicitClass = TRUE;
					}
				}
			}
			break;
		/*
		 *  Looking for classes
		 */
		case UT_CONTEXT_CLASS:
			UT_DBGOUT(2, ("<UT> setTraceState: Class\n"));
			/* handled in selectComponent */
			context = UT_CONTEXT_PROCESS;
			break;
		/*
		 *  Process the state so far.
		 */
		case UT_CONTEXT_PROCESS:
			UT_DBGOUT(2, ("<UT> setTraceState: Process\n"));
			if (*p == '\0') {
				context = UT_CONTEXT_FINAL;
			} else {
				setActive = TRUE;
				context = UT_CONTEXT_COMPONENT;
			}
			break;
		}


		/*
		 *  Check for more input
		 */
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			if (*p == '\0') {
				if (context == UT_CONTEXT_COMPONENT &&
					!first) {
					context = UT_CONTEXT_FINAL;
				}
			}
			if (*p == ',') {
				p++;
				if (*p == '\0') {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				}
			}
		}
	}

	/*
	 *  Expecting more input ?
	 */
	if (explicitClass) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (OMR_ERROR_NONE == rc) {
		if (context != UT_CONTEXT_FINAL) {
			reportCommandLineError(atRuntime, "Trace selection specification incomplete: %s", cmd);
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	} else {
		reportCommandLineError(atRuntime, "Syntax error encountered at offset %d in: %s", p - cmd, cmd);
	}

	return rc;
}

/*******************************************************************************
 * name        - setOptions
 * description - Universal trace options
 * parameters  - char **options, atRuntime
 * returns     - OMR return code
 ******************************************************************************/

omr_error_t
setOptions(OMR_TraceThread *thr, const char **opts, BOOLEAN atRuntime)
{
	omr_error_t	rc = OMR_ERROR_NONE;

	UT_DBGOUT(1, ("<UT> Initializing options \n"));

	if (!atRuntime) {
		/*
		 *  Process any additional early options
		 */
		rc = processEarlyOptions(opts);
		if (OMR_ERROR_NONE != rc) {
			/* Can only fail due to OOM copying path to format file, no message required. */
			return OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	}

	/*
	 *  Process the rest of the options
	 */
	rc = processOptions(thr, opts, atRuntime);
	if (OMR_ERROR_NONE != rc) {
		/* An error message will have been reported by processOptions
		 * J9VMDLLMain will display general -Xtrace:help output.
		 */
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	return OMR_ERROR_NONE;
}

/**************************************************************************
 * name        - getPositionalParm
 * description - Get a positional parameter
 * parameters  - string value of the property
 * returns     - Pointer to keyword or null. If not null, size is set
 *************************************************************************/
const char *
getPositionalParm(int pos, const char *string, int *size)
{
	const char *p, *q;
	int i;

	/* Find the parameter */
	p = string;
	for (i = 1; i < pos; i++) {
		p = strchr(p, ',');
		if (p != NULL) {
			p++;
		} else {
			break;
		}
	}

	/* Find its size */
	if (NULL != p) {
		q = strchr(p, ',');
		if (NULL == q) {
			*size = (int)strlen(p);
		} else {
			*size = (int)(q - p);
		}
	}
	return p;
}

/**************************************************************************
 * name        - getParmNumber
 * description - Gets the number of positional parameters
 * parameters  - string
 * returns     - Number of parameters
 *************************************************************************/
int
getParmNumber(const char *string)
{
	const char *p;
	int i;

	p = string;
	for (i = 0; p != NULL; i++) {
		p = strchr(p, ',');
		if (NULL != p) {
			p++;
		}
	}
	return i;
}

/**************************************************************************
 * name        - decimalString2Int
 * description - Convert a decimal string into an int
 * parameters  - decString - ptr to the decimal string
 *               rc        - ptr to return code
 * returns     - void
 *************************************************************************/
int
decimalString2Int(const char *decString, int32_t signedAllowed, omr_error_t *rc, BOOLEAN atRuntime)
{
	const char *p = decString;
	int num = -1;
	int32_t inputIsSigned = FALSE;
	int min_string_length = 1;
	int max_string_length = 7;

	/*
	 * is first character a sign character
	 */
	if (*p == '+' || *p == '-') {
		inputIsSigned = TRUE;
		min_string_length++;
		max_string_length++;
		p++;
	}

	/*
	 * check whether an invalid sign character was entered
	 */
	if (inputIsSigned && !signedAllowed) {
		reportCommandLineError(atRuntime, "Signed number not permitted in this context \"%s\".", decString);
		*rc = OMR_ERROR_INTERNAL;
	}

	/*
	 * walk to end of the string
	 */
	if (OMR_ERROR_NONE == *rc) {
		for (; ((*p != '\0') && strchr("0123456789", *p) != NULL); p++) { /*empty */
		}

		/*
		 * check what followed the string.  If it wasn't a ',' or '}' or NULL
		 * then there's garbage in the string
		 */
		if ((*p != ',') && (*p != '}') && (*p != '\0') && (*p != ' ')) {
			reportCommandLineError(atRuntime, "Invalid character(s) encountered in decimal number \"%s\".", decString);
			*rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == *rc) {
		/*
		 * if it was too long or too short, error, else parse it as num
		 */
		if ((p - decString) < min_string_length || (p - decString) > max_string_length) {
			*rc = OMR_ERROR_INTERNAL;
			reportCommandLineError(atRuntime, "Number too long or too short \"%s\".", decString);
		} else {
			sscanf(decString, "%d", &num);
		}
	}

	return num;
}
