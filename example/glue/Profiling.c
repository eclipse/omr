/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "omr.h"
#include "omrprofiler.h"

void ex_omr_checkSampleStack(OMR_VMThread *omrVMThread, const void *context);
void ex_omr_insertMethodEntryInMethodDictionary(OMR_VM *omrVM, const void *method);

static void ex_omr_sampleStack(OMR_VMThread *omrVMThread, const void *context);

#define EX_OMR_SAMPLESTACK_BACKOFF_MAX 10
#define EX_OMR_SAMPLESTACK_BACKOFF_TIMER_DECR 1

#define EX_OMR_PROF_METHOD_NAME_IDX 0
#define EX_OMR_PROF_FILE_NAME_IDX 1
#define EX_OMR_PROF_LINE_NUMBER_IDX 2

#define EX_METHOD_PROPERTY_COUNT 3

static const char *methodPropertyNames[EX_METHOD_PROPERTY_COUNT] = {
		"methodName",
		"fileName",
		"lineNumber"
};

typedef struct EX_OMR_MethodDictionaryEntry {
	const void *key;
	const char *propertyValues[EX_METHOD_PROPERTY_COUNT];
} EX_OMR_MethodDictionaryEntry;

int
OMR_Glue_GetMethodDictionaryPropertyNum(void)
{
	return EX_METHOD_PROPERTY_COUNT;
}

const char * const *
OMR_Glue_GetMethodDictionaryPropertyNames(void)
{
	return methodPropertyNames;
}

/**
 * This is an example of how the language runtime can iterate the omrVMThread's language callstack.
 *
 * Iterate from top to bottom. For the top-most stack frame, call omr_ras_sampleStackTraceStart()
 * with the frame's method key. For each successive stack frame, call omr_ras_sampleStackTraceContinue()
 * with the frame's method key.
 *
 * The method key must be the same as the key value that was used to insert the method in the method dictionary
 * by omr_ras_insertMethodDictionary(). The context represents a language-specific data structure which
 * contains the callstack's method keys for each stack frame.
 *
 * This function is only an example, and may be completely customized by the language runtime. It
 * may be omitted if method profiling is not implemented.
 */
static void
ex_omr_sampleStack(OMR_VMThread *omrVMThread, const void *context)
{
#if 0
	omr_ras_sampleStackTraceStart(omrVMThread, /* method key from top-most stack frame */);

	For each successive stack frame:
		omr_ras_sampleStackTraceContinue(omrVMThread, /* method key from stack frame */);
#endif
}

/**
 * This is an example of how the language runtime can sample callstacks to produce method profiles.
 *
 * This function should be called periodically by the interpreter to sample the currently running
 * thread's stack. If a JIT is implemented, then JITted code may also need to call this function
 * periodically.
 *
 * In this example, a backoff counter is used to control the sampling frequency and restrict the
 * overhead of callstack sampling. The context parameter represents a language-specific data structure
 * containing the current callstack, such as the current thread.
 *
 * This function is only an example, and may be completely customized by the language runtime. It
 * may be omitted if method profiling is not implemented.
 */
void
ex_omr_checkSampleStack(OMR_VMThread *omrVMThread, const void *context)
{
	if (0 == omrVMThread->_sampleStackBackoff) {
		omrVMThread->_sampleStackBackoff = EX_OMR_SAMPLESTACK_BACKOFF_MAX;
		if (omr_ras_sampleStackEnabled()) {
			ex_omr_sampleStack(omrVMThread, context);
		}
	}
	if (EX_OMR_SAMPLESTACK_BACKOFF_TIMER_DECR > omrVMThread->_sampleStackBackoff) {
		omrVMThread->_sampleStackBackoff = 0;
	} else {
		omrVMThread->_sampleStackBackoff -= EX_OMR_SAMPLESTACK_BACKOFF_TIMER_DECR;
	}
}

/**
 * This is an example of how the language runtime can insert a method entry into the method
 * dictionary.
 *
 * Method dictionary entries are needed to provide the names and locations of methods that
 * are sampled (see omr_ras_sampleStackTraceStart(), omr_ras_sampleStackTraceContinue()).
 * The method parameter represents a language-specific data structure which contains the
 * properties for the method entry.
 *
 * This function is only an example, and may be completely customized by the language runtime.
 * It may be omitted if method profiling is not implemented.
*/
void
ex_omr_insertMethodEntryInMethodDictionary(OMR_VM *omrVM, const void *method)
{
	omr_error_t rc = OMR_ERROR_NONE;
	if (NULL != omrVM->_methodDictionary) {
		EX_OMR_MethodDictionaryEntry tempEntry;

		memset(&tempEntry, 0, sizeof(tempEntry));
		tempEntry.key = method;
		/* These properties should be extracted from the language-specific method structure. */
		tempEntry.propertyValues[EX_OMR_PROF_METHOD_NAME_IDX] = "exampleMethod";
		tempEntry.propertyValues[EX_OMR_PROF_FILE_NAME_IDX] = "exampleFile";
		tempEntry.propertyValues[EX_OMR_PROF_LINE_NUMBER_IDX] = "1";

		rc = omr_ras_insertMethodDictionary(omrVM, (OMR_MethodDictionaryEntry *)&tempEntry);
		if (OMR_ERROR_NONE != rc) {
			fprintf(stderr, "omr_insertMethodEntryInMethodDictionary failed.\n");
		}
	}
}
