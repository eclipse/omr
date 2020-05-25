/*******************************************************************************
 * Copyright (c) 1998, 2020 IBM Corp. and others
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

#include <stdlib.h>

#include "omrstdarg.h"

#include "omrtrace_internal.h"
#include "pool_api.h"
#include "omrutil.h"

#include "AtomicSupport.hpp"

#define MAX_QUALIFIED_NAME_LENGTH 16


static OMR_TraceBuffer *allocateTraceBuffer(OMR_TraceThread *currentThread);
static UtProcessorInfo *getProcessorInfo(void);
static void raiseAssertion(void);

char pointerSpec[2] = {(char)sizeof(char *), '\0'};

#define UNKNOWN_SERVICE_LEVEL "Unknown version"

/*******************************************************************************
 * name        - initTraceHeader
 * description - Inititializes the trace header.
 * parameters  - void
 * returns     - OMR error code
 ******************************************************************************/
omr_error_t
initTraceHeader(void)
{
	int                 size;
	UtTraceFileHdr     *trcHdr;
	char               *ptr;
	UtTraceCfg         *cfg;
	int                 actSize, srvSize, startSize;
	UtProcessorInfo			*procinfo;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	/*
	 *  Return if it already exists
	 */
	if (OMR_TRACEGLOBAL(traceHeader) != NULL) {
		return OMR_ERROR_NONE;
	}

	size = offsetof(UtTraceFileHdr, traceSection);
	size += sizeof(UtTraceSection);

	/*
	 *  Calculate length of service section
	 */
	srvSize = offsetof(UtServiceSection, level);
	if (OMR_TRACEGLOBAL(serviceInfo) == NULL) {

		/* Make sure we allocate this so we can free it on shutdown. */
		OMR_TRACEGLOBAL(serviceInfo) = (char *)omrmem_allocate_memory(sizeof(UNKNOWN_SERVICE_LEVEL) + 1, OMRMEM_CATEGORY_TRACE);
		if (NULL == OMR_TRACEGLOBAL(serviceInfo)) {
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(OMR_TRACEGLOBAL(serviceInfo), UNKNOWN_SERVICE_LEVEL);
	}
	srvSize += (int)strlen(OMR_TRACEGLOBAL(serviceInfo)) + 1;
	srvSize = ((srvSize +
				UT_STRUCT_ALIGN - 1) / UT_STRUCT_ALIGN) * UT_STRUCT_ALIGN;
	size += srvSize;


	/*
	 *  Calculate length of startup options
	 */
	startSize = offsetof(UtStartupSection, options);
	if (OMR_TRACEGLOBAL(properties) != NULL) {
		startSize += (int)strlen(OMR_TRACEGLOBAL(properties)) + 1;
	}
	startSize = ((startSize +
				  UT_STRUCT_ALIGN - 1) / UT_STRUCT_ALIGN) * UT_STRUCT_ALIGN;
	startSize = ((startSize + 3) / 4) * 4;
	size += startSize;

	/*
	 *  Calculate length of trace activation commands
	 */
	actSize = offsetof(UtActiveSection, active);

	for (cfg = OMR_TRACEGLOBAL(config);
		 cfg != NULL;
		 cfg = cfg->next) {
		actSize += (int)strlen(cfg->command) + 1;
	}
	actSize = ((actSize +
				UT_STRUCT_ALIGN - 1) / UT_STRUCT_ALIGN) * UT_STRUCT_ALIGN;
	actSize = ((actSize + 3) / 4) * 4;
	size += actSize;


	/*
	 *  Add length of UtProcSection
	 */
	size += sizeof(UtProcSection);
	trcHdr = (UtTraceFileHdr *)omrmem_allocate_memory(size, OMRMEM_CATEGORY_TRACE);
	if (NULL == trcHdr) {
		UT_DBGOUT(1, ("<UT> Out of memory in initTraceHeader\n"));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	memset(trcHdr, '\0', size);
	initHeader(&trcHdr->header, UT_TRACE_HEADER_NAME, size);

	trcHdr->bufferSize       = OMR_TRACEGLOBAL(bufferSize);
	trcHdr->endianSignature  = UT_ENDIAN_SIGNATURE;
	trcHdr->traceStart       = offsetof(UtTraceFileHdr, traceSection);
	trcHdr->serviceStart     = trcHdr->traceStart + sizeof(UtTraceSection);
	trcHdr->startupStart     = trcHdr->serviceStart + srvSize;
	trcHdr->activeStart      = trcHdr->startupStart + startSize;
	trcHdr->processorStart   = trcHdr->activeStart + actSize;


	/*
	 * Initialize trace section
	 */
	ptr = (char *)trcHdr + trcHdr->traceStart;
	initHeader((UtDataHeader *)ptr, UT_TRACE_SECTION_NAME,
			   sizeof(UtTraceSection));
	((UtTraceSection *)ptr)->startPlatform = OMR_TRACEGLOBAL(startPlatform);
	((UtTraceSection *)ptr)->startSystem = OMR_TRACEGLOBAL(startSystem);
	((UtTraceSection *)ptr)->type = OMR_TRACEGLOBAL(traceInCore) ? UT_TRACE_INTERNAL : UT_TRACE_EXTERNAL;
	((UtTraceSection *)ptr)->generations = 0;
	((UtTraceSection *)ptr)->pointerSize = sizeof(void *);

	/*
	 * Initialize service level section
	 */
	ptr = (char *)trcHdr + trcHdr->serviceStart;
	initHeader((UtDataHeader *)ptr, UT_SERVICE_SECTION_NAME, srvSize);
	ptr += offsetof(UtServiceSection, level);
	strcpy(ptr, OMR_TRACEGLOBAL(serviceInfo));

	/*
	 * Initialize startup option section
	 */
	ptr = (char *)trcHdr + trcHdr->startupStart;
	initHeader((UtDataHeader *)ptr, UT_STARTUP_SECTION_NAME, startSize);
	ptr += offsetof(UtStartupSection, options);
	if (OMR_TRACEGLOBAL(properties) != NULL) {
		strcpy(ptr, OMR_TRACEGLOBAL(properties));
		/*ptr += strlen(OMR_TRACEGLOBAL(properties)) + 1;*/
	}

	/*
	 *  Fill in UtActiveSection with trace activation commands
	 */
	ptr = (char *)trcHdr + trcHdr->activeStart;
	initHeader((UtDataHeader *)ptr, UT_ACTIVE_SECTION_NAME, actSize);
	ptr += offsetof(UtActiveSection, active);
	for (cfg = OMR_TRACEGLOBAL(config);
		 cfg != NULL;
		 cfg = cfg->next) {
		strcpy(ptr, cfg->command);
		ptr += strlen(cfg->command) + 1;
	}

	/*
	 *  Initialize UtProcSection
	 */
	ptr = (char *)trcHdr + trcHdr->processorStart;
	initHeader((UtDataHeader *)ptr, UT_PROC_SECTION_NAME,
			   sizeof(UtProcSection));
	ptr += offsetof(UtProcSection, processorInfo);
	procinfo = getProcessorInfo();
	if (procinfo == NULL) {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	} else {
		memcpy(ptr, procinfo, sizeof(UtProcessorInfo));
		omrmem_free_memory(procinfo);
	}

	OMR_TRACEGLOBAL(traceHeader) = trcHdr;
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - getTrcBuf
 * description - Get and initialize a TraceBuffer
 * parameters  - thr, current buffer pointer or null, buffer type
 * returns     - TraceBuffer pointer or NULL
 ******************************************************************************/
static OMR_TraceBuffer *
getTrcBuf(OMR_TraceThread *thr, OMR_TraceBuffer *oldBuf, int bufferType)
{
	OMR_TraceBuffer *trcBuf;
	const uint32_t typeFlags = UT_TRC_BUFFER_ACTIVE | UT_TRC_BUFFER_NEW;
	uint64_t writePlatform;
	uint64_t writeSystem;

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));
	writePlatform = omrtime_hires_clock();
	writeSystem = ((uint64_t) omrtime_current_time_millis());
	writePlatform = (writePlatform >> 1) + (omrtime_hires_clock() >> 1);

	if (oldBuf != NULL) {
		/*
		 *  Update write timestamp in case of a dump being taken before
		 *  the buffer is ever written to disk for any reason
		 */
		oldBuf->record.writeSystem = writeSystem;
		oldBuf->record.writePlatform = writePlatform;

		/* Null the thread reference as there's no guarantee it's valid after this point */
		oldBuf->thr = NULL;

		if (OMR_TRACEGLOBAL(traceInCore)) {
			/*
			 *  Incore trace mode so reuse existing buffer, wrapping to the top
			 */
			trcBuf = oldBuf;
			goto out;

		} else {
			publishTraceBuffer(thr, oldBuf);
		}
	}

	/*
	 * Reuse buffer if there is one
	 */
	trcBuf = recycleTraceBuffer(thr);

	/*
	 *  If no buffers, try to obtain one
	 */
	if (trcBuf == NULL) {
		/*
		 * CMVC 164637: Don't use memory allocation routines when handling signals.
		 * Returning NULL will make us spill some trace points.
		 */
		if (omrsig_get_current_signal()) {
			return NULL;
		}

		trcBuf = allocateTraceBuffer(thr);
		if (trcBuf == NULL) {
			if (OMR_TRACEGLOBAL(dynamicBuffers) == TRUE) {
				OMR_TRACEGLOBAL(dynamicBuffers) = FALSE;
				/* Native memory allocation failure, falling back to nodynamic trace settings. */
				trcBuf = getTrcBuf(thr, oldBuf, bufferType);
			}
			return trcBuf;
		}

		VM_AtomicSupport::addU32((volatile uint32_t *)&OMR_TRACEGLOBAL(allocatedTraceBuffers), 1);
		UT_DBGOUT(1, ("<UT> Allocated buffer %i " UT_POINTER_SPEC "\n", OMR_TRACEGLOBAL(allocatedTraceBuffers), trcBuf));
	}


	/*
	 *  Initialize buffer for this thread
	 */
	UT_DBGOUT(5, ("<UT> Buffer " UT_POINTER_SPEC " obtained for thread " UT_POINTER_SPEC "\n", trcBuf, thr));
	trcBuf->next = NULL;
	trcBuf->bufferType = bufferType;
	trcBuf->thr = NULL;

	if (bufferType == UT_NORMAL_BUFFER) {
		trcBuf->record.threadId   = (uint64_t)(uintptr_t)thr->id;
		trcBuf->record.threadSyn1 = (uint64_t)(uintptr_t)thr->synonym1;
		trcBuf->record.threadSyn2 = (uint64_t)(uintptr_t)thr->synonym2;
		strncpy(trcBuf->record.threadName, thr->name, UT_MAX_THREAD_NAME_LENGTH);
		trcBuf->record.threadName[UT_MAX_THREAD_NAME_LENGTH] = '\0';
		thr->trcBuf = trcBuf;
#if OMR_ENABLE_EXCEPTION_OUTPUT
	} else if (bufferType == UT_EXCEPTION_BUFFER) {
		trcBuf->record.threadId = 0;
		trcBuf->record.threadSyn1 = 0;
		trcBuf->record.threadSyn2 = 0;

		strcpy(trcBuf->record.threadName, UT_EXCEPTION_THREAD_NAME);
		OMR_TRACEGLOBAL(exceptionTrcBuf) = trcBuf;
#endif
	}

	trcBuf->record.firstEntry = offsetof(UtTraceRecord, threadName) +
								(int32_t)strlen(trcBuf->record.threadName) + 1;

out:

	trcBuf->flags = typeFlags;

	/* we reset the contents of the buffer first so that even if we're in the middle of
	 * flushing/writing this buffer, the record maintains its consistency.
	 */
	trcBuf->record.nextEntry = trcBuf->record.firstEntry;
	VM_AtomicSupport::readBarrier();
	*((char *)&trcBuf->record + trcBuf->record.firstEntry) = '\0';

	trcBuf->record.sequence = writePlatform;
	trcBuf->record.wrapSequence = trcBuf->record.sequence;

	/* in case this buffer is dumped rather than queued we want non zero values for the write times */
	trcBuf->record.writeSystem = writeSystem;
	trcBuf->record.writePlatform = writePlatform;

	return trcBuf;
}

/*******************************************************************************
 * name        - copyToBuffer
 * description - Routine to copy data to trace buffer if it may wrap
 * parameters  - OMR_TraceThread,  buffer type, trace data, current tracebuffer
 *               pointer, length of the data to copy, traceentry length and
 *               start of the buffer
 * returns     - void
 ******************************************************************************/
static void
copyToBuffer(OMR_TraceThread *thr,
			 int bufferType,
			 const char *var,
			 char **p,
			 int length,
			 int *entryLength,
			 OMR_TraceBuffer **trcBuf)
{
	int bufLeft = (int)((char *)&(*trcBuf)->record + OMR_TRACEGLOBAL(bufferSize) - *p);
	int remainder;
	const char *str;

	/*
	 * Will this exceed maximum trace record length ?
	 */
	if ((*entryLength + length) > UT_MAX_EXTENDED_LENGTH) {
		length = UT_MAX_EXTENDED_LENGTH - *entryLength;
		if (length <= 0) {
			return;
		}
	}

	/*
	 *  Will the record fit in the buffer ?
	 */
	if (length < bufLeft) {
		memcpy(*p, var, length);
		*entryLength += length;
		*p += length;
	} else {
		/*
		 *  Otherwise, copy what will fit...
		 */
		str = var;
		remainder = length;
		if (bufLeft > 0) {
			memcpy(*p, str, bufLeft);
			remainder -= bufLeft;
			*entryLength += bufLeft;
			*p += bufLeft;
			str += bufLeft;
		}
		/*
		 *  ... and get another buffer
		 */
		while (remainder > 0) {
			*trcBuf = getTrcBuf(thr, *trcBuf, bufferType);
			if (*trcBuf != NULL) {
				/* we're about to copy stuff into it so mark the buffer as not new so it can be queued */
				VM_AtomicSupport::bitAndU32((volatile uint32_t *)(&(*trcBuf)->flags), ~UT_TRC_BUFFER_NEW);
				(*trcBuf)->thr = thr;

				/* we need to update p and bufLeft irrespective of what happens next */
				*p = (char *)&(*trcBuf)->record + (*trcBuf)->record.nextEntry;
				bufLeft = OMR_TRACEGLOBAL(bufferSize) - (*trcBuf)->record.nextEntry;

				/* if there's already a tracepoint in the buffer we need to use nextEntry +1 */
				if ((*trcBuf)->record.nextEntry != (*trcBuf)->record.firstEntry) {
					(*p)++;
					bufLeft--;
				} else {
					/* there is a possibility that we will span this entire buffer with the current trace point
					 * but not with the data we are currently copying so we need to set this here as a precaution.
					 * nextEntry is always reconstructed at the end of the trace point from the cursor so this is
					 * overwritten if it's not needed.
					 */
					(*trcBuf)->record.nextEntry = -1;
				}

				if (remainder < bufLeft) {
					memcpy(*p, str, remainder);
					*p += remainder;
					*entryLength += remainder;
					remainder = 0;
				} else {
					memcpy(*p, str, bufLeft);
					*entryLength += bufLeft;
					*p += bufLeft;
					remainder -= bufLeft;
					str += bufLeft;
				}
			} else {
				remainder = 0;
			}
		}
	}
}

/*******************************************************************************
 * name        - tracePrint
 * description - Print a tracepoint directly
 * parameters  - OMR_TraceThread, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
tracePrint(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, va_list var)
{
	int             id;
	char           *format;
	uint32_t          hh, mm, ss, millis;
	char            threadSwitch = ' ';
	char							entryexit = ' ';
	char							excpt = ' ';
	static char     blanks[] = "                    "
							   "                    "
							   "                    "
							   "                    "
							   "                    ";
	char qualifiedModuleName[MAX_QUALIFIED_NAME_LENGTH + 1];
	const char *moduleName = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	memset(qualifiedModuleName, '\0', sizeof(qualifiedModuleName));
	if (modInfo == NULL) {
		strcpy(qualifiedModuleName, "dg");
		moduleName = "dg";
	} else 	if (modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL) {
		omrstr_printf(qualifiedModuleName, MAX_QUALIFIED_NAME_LENGTH, "%s(%s)", modInfo->name, modInfo->containerModule->name);
		moduleName = modInfo->name;
	} else {
		strncpy(qualifiedModuleName, modInfo->name, MAX_QUALIFIED_NAME_LENGTH);
		moduleName = modInfo->name;
	}

	/*
	 *  Split tracepoint id into component and tracepoint within component
	 */

	id   = (traceId >> 8) & UT_TRC_ID_MASK;

	/*
	 * Find the appropriate format for this tracepoint
	 */
	format = getFormatString(moduleName, id);
	if (format == NULL) {
		return;
	}

	/*
	 *  Get the time in GMT
	 */
	getTimestamp(omrtime_current_time_millis(), &hh, &mm, &ss, &millis);

	/*
	 *  Hold the traceLock to avoid a trace data corruption
	 */
	getTraceLock(thr);

	/*
	 *  Check for thread switch since last tracepoint
	 */
	if (OMR_TRACEGLOBAL(lastPrint) != thr) {
		OMR_TRACEGLOBAL(lastPrint) = thr;
		threadSwitch = '*';
	}

	if (OMR_TRACEGLOBAL(indentPrint)) {
		/*
		 *  If indented print was requested
		 */
		char *indent;
		excpt = format[0];      /* Pick up exception flag, if any */
		entryexit = format[1];

		/*
		 *  If this is an exit type tracepoint, decrease the indentation,
		 *  but make sure it doesn't go negative
		 */
		if (format[1] == '<' &&
			thr->indent > 0) {
			thr->indent--;
		}

		/*
		 *  Set indent, limited by the size of the array of blanks above
		 */
		indent = blanks + sizeof(blanks) - 1 - thr->indent;
		indent = (indent < blanks) ? blanks : indent;

		/*
		 *  If this is an entry type tracepoint, increase the indentation
		 */
		if (format[1] == '>') {
			thr->indent++;
		}

		if (format[1] == ' ') {
			entryexit = '-';
		}

		/*
		 *  Indented print
		 */
		omrtty_err_printf("%02d:%02d:%02d.%03d%c" UT_POINTER_SPEC
						  "%16s.%-6d %c %s %c ", hh, mm, ss, millis, threadSwitch, thr->id, qualifiedModuleName,
						  id, excpt, indent, entryexit);
		omrtty_err_vprintf(format + 2, var);

	} else {
		/*
		 *  Non-indented print
		 */
		excpt = format[0];
		format[1] == ' ' ? (entryexit = '-') : (entryexit = format[1]);

		omrtty_err_printf("%02d:%02d:%02d.%03d%c" UT_POINTER_SPEC
						  "%16s.%-6d %c %c ", hh, mm, ss, millis, threadSwitch, thr->id, qualifiedModuleName,
						  id, excpt, entryexit);
		omrtty_err_vprintf(format + 2, var);

	}
	omrtty_err_printf("\n");
	freeTraceLock(thr);
}

/*******************************************************************************
 * name        - traceAssertion
 * description - Print a tracepoint directly
 * parameters  - OMR_TraceThread, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
traceAssertion(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, va_list var)
{
	int             id;
	const char     *format;
	uint32_t   hh, mm, ss, millis;
	char qualifiedModuleName[MAX_QUALIFIED_NAME_LENGTH + 1];
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	memset(qualifiedModuleName, '\0', sizeof(qualifiedModuleName));
	if (modInfo == NULL) {
		strcpy(qualifiedModuleName, "dg");
	} else if (modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL) {
		omrstr_printf(qualifiedModuleName, MAX_QUALIFIED_NAME_LENGTH, "%s(%s)", modInfo->name, modInfo->containerModule->name);
	} else {
		strncpy(qualifiedModuleName, modInfo->name, MAX_QUALIFIED_NAME_LENGTH);
	}

	getTimestamp(omrtime_current_time_millis(), &hh, &mm, &ss, &millis);

	/*
	 *  Split tracepoint id into component and tracepoint within component
	 */

	id   = (traceId >> 8) & UT_TRC_ID_MASK;

	format = "* ** ASSERTION FAILED ** at %s:%d: %s";

	/*
	 *  Hold the traceLock to avoid a trace data corruption
	 */
	getTraceLock(thr);

	/*
	 *  Non-indented print
	 */
	omrtty_err_printf("%02d:%02d:%02d.%03d " UT_POINTER_SPEC
					  "%8s.%-6d *   ", hh, mm, ss, millis, thr->id, qualifiedModuleName,
					  id);
	omrtty_err_vprintf(format + 2, var);

	omrtty_err_printf("\n");

	freeTraceLock(thr);
}

/*******************************************************************************
 * name        - traceCount
 * description - Increment the count for a tracepoint
 * parameters  - moduleInfo, tracepoint identifier
 * returns     - void
 ******************************************************************************/
static void
traceCount(UtModuleInfo *moduleInfo, uint32_t traceId)
{
	int traceNum = (traceId >> 8) & UT_TRC_ID_MASK;
	incrementTraceCounter(moduleInfo, OMR_TRACEGLOBAL(componentList), traceNum);
}

/*******************************************************************************
 * name        - utTraceV
 * description - Make a tracepoint
 * parameters  - OMR_TraceThread, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
traceV(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec,
	   va_list var, int bufferType)
{
	OMR_TraceBuffer   *trcBuf;
	int                lastSequence;
	int                entryLength;
	int                length;
	char              *p;
	const signed char *str;
	char              *format = NULL;
	int32_t               intVar;
	char               charVar;
	unsigned short     shortVar;
	int64_t               i64Var;
	double             doubleVar;
	char              *ptrVar;
	const char        *stringVar;
	size_t             stringVarLen;
	char              *containerModuleVar = NULL;
	size_t             containerModuleVarLen = 0;
	char               temp[3];
	static char        lengthConversion[] = {0,
											 sizeof(char),
											 sizeof(short),
											 0,
											 sizeof(int32_t),
											 sizeof(float),
											 sizeof(char *),
											 sizeof(double),
											 sizeof(int64_t),
											 sizeof(long double),
											 0
											};
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	if (modInfo != NULL) {
		traceId += (257 << 8); /* this will be translated back by the formatter */
		stringVar = modInfo->name;
		stringVarLen = length = modInfo->namelength;
		if (modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL) {
			containerModuleVar = modInfo->containerModule->name;
			containerModuleVarLen = modInfo->containerModule->namelength;
			length += (int)containerModuleVarLen + sizeof("()") - 1;
		}
	} else {
		/* modInfo == NULL means this is a tracepoint for the trace formatter */
		stringVar = "INTERNALTRACECOMPONENT";
		stringVarLen = length = sizeof("INTERNALTRACECOMPONENT") - 1;
	}

	/*
	 *  Trace buffer available ?
	 */
	if (bufferType == UT_NORMAL_BUFFER) {
		if (((trcBuf = thr->trcBuf) == NULL)
		 && ((trcBuf = getTrcBuf(thr, NULL, bufferType)) == NULL)
		) {
			return;
		}
#if OMR_ENABLE_EXCEPTION_OUTPUT
	} else if (bufferType == UT_EXCEPTION_BUFFER) {
		if (((trcBuf = OMR_TRACEGLOBAL(exceptionTrcBuf)) == NULL)
		 && ((trcBuf = getTrcBuf(thr, NULL, bufferType)) == NULL)
		) {
			return;
		}
#endif
	} else {
		return;
	}

	if (trcBuf->flags & UT_TRC_BUFFER_NEW) {
		VM_AtomicSupport::bitAndU32((volatile uint32_t *)(&trcBuf->flags), ~UT_TRC_BUFFER_NEW);
		trcBuf->thr = thr;
	}
	lastSequence = (int32_t)(trcBuf->record.sequence >> 32);
	trcBuf->record.sequence = omrtime_hires_clock();
	p = (char *)&trcBuf->record + trcBuf->record.nextEntry + 1;

	/* additional sanity check */
	if ((p < (char *)&trcBuf->record) || (p > ((char *)&trcBuf->record + OMR_TRACEGLOBAL(bufferSize)))) {
		/* the buffer's been mangled so free it for reuse and acquire another */
		UT_DBGOUT(1, ("<UT> invalid nextEntry value in record. Freed trace buffer for thread " UT_POINTER_SPEC " and reinitialized\n", thr));

		releaseTraceBuffer(thr, trcBuf);
		thr->trcBuf = NULL;
		trcBuf = getTrcBuf(thr, NULL, bufferType);
		if (trcBuf == NULL) {
			return;
		}

		p = (char *)&trcBuf->record + trcBuf->record.nextEntry + 1;
	}

	if ((OMR_TRACEGLOBAL(bufferSize) - trcBuf->record.nextEntry) > (UT_FASTPATH + length + 4)) {
		/* there is space in the buffer for a minimal entry, so avoid the overhead of copyToBuffer func	*/
		if (lastSequence != (int32_t)(trcBuf->record.sequence >> 32)) {
			*p       = UT_TRC_SEQ_WRAP_ID[0];
			*(p + 1) = UT_TRC_SEQ_WRAP_ID[1];
			*(p + 2) = UT_TRC_SEQ_WRAP_ID[2];
			memcpy(p + 3, &lastSequence, sizeof(int32_t));
			*(p + 7) = UT_TRC_SEQ_WRAP_LENGTH;
			trcBuf->record.nextEntry += UT_TRC_SEQ_WRAP_LENGTH;
			p += UT_TRC_SEQ_WRAP_LENGTH;
		}
		*p = (char)(traceId >> 24);
		*(p + 1) = (char)(traceId >> 16);
		*(p + 2) = (char)(traceId >> 8);
		{
			int32_t temp = (int32_t)(trcBuf->record.sequence & 0xFFFFFFFF);
			memcpy(p + 3, &temp, sizeof(int32_t));
		}
		p += 7;

		/* write name length into buffer */
		memcpy(p, &length, 4);

		/* write name into buffer */
		memcpy(p + 4, stringVar, stringVarLen);

		p += (stringVarLen + 4);

		/* if tracepoint is part of other component - write container's name into buffer */
		if (containerModuleVar != NULL) {
			*p++ = '(';
			memcpy(p, containerModuleVar, containerModuleVarLen);
			p += containerModuleVarLen;
			*p++ = ')';
		}

		entryLength = length + 12;
		*p = 12 + length;

	} else {
		/* approaching end of buffer, use copyToBuffer to handle the wrap */
		/*
		 *  Handle sequence counter wrap
		 *
		 *  It's not a problem if the sequence counter write is aborted/discarded
		 *  by nodynamic because it applies to the *preceeding* tracepoints and
		 *  they'll have been discarded as well.
		 */
		if (lastSequence != (int32_t)(trcBuf->record.sequence >> 32)) {
			char len = UT_TRC_SEQ_WRAP_LENGTH;
			entryLength = 0;
			copyToBuffer(thr, bufferType, UT_TRC_SEQ_WRAP_ID, &p, 3,
						 &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, (char *)&lastSequence, &p, 4,
						 &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, &len, &p, 1,
						 &entryLength, &trcBuf);
			/* copyToBuffer increments p past the last byte written, but nextEntry
			 * needs to point to the length byte so we need -1 here.
			 */
			trcBuf->record.nextEntry = (int32_t)(p - (char *)&trcBuf->record - 1);
		}
		entryLength = 1;
		temp[0] = (char)(traceId >> 24);
		temp[1] = (char)(traceId >> 16);
		temp[2] = (char)(traceId >> 8);
		copyToBuffer(thr, bufferType, temp, &p, 3, &entryLength, &trcBuf);
		intVar = (int32_t)trcBuf->record.sequence;
		copyToBuffer(thr, bufferType, (char *)&intVar,
					 &p, 4, &entryLength, &trcBuf);

		/* write name length to buffer */
		copyToBuffer(thr, bufferType, (char *)&length,
					 &p, 4, &entryLength, &trcBuf);

		/* write name to buffer */
		copyToBuffer(thr, bufferType, stringVar, &p,
					 (int)stringVarLen, &entryLength, &trcBuf);

		if (containerModuleVar != NULL) {
			copyToBuffer(thr, bufferType, "(", &p, 1, &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, containerModuleVar, &p, (int)containerModuleVarLen, &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, ")", &p, 1, &entryLength, &trcBuf);
		}

		charVar = (char)entryLength;
		copyToBuffer(thr, bufferType, &charVar, &p, 1, &entryLength,
					 &trcBuf);

		p--;
		entryLength--;
	}

	/*
	 * Process maximal trace
	 */
	str = (const signed char *)spec;
	if (OMR_ARE_ANY_BITS_SET(thr->currentOutputMask, UT_MAXIMAL | UT_EXCEPTION) && (str != NULL)) {
		int i;

		/*
		 * Calculate the length
		 */
		length = 0;
		for (i = 0; str[i] > UT_TRACE_DATA_TYPE_END; i++) {
			length += lengthConversion[str[i]];
		}
		if (str[i] != UT_TRACE_DATA_TYPE_END) {
			length = -1;
		}
		/*
		 *  If we know the length and it will fit without a wrap
		 */
		if ((length > 0)  &&
			((p + length + 1) < (char *)&trcBuf->record + OMR_TRACEGLOBAL(bufferSize))) {
			while (*str != '\0') {
				switch (*str) {
				/*
				 *  Words
				 */
				case UT_TRACE_DATA_TYPE_INT32: {
					intVar = va_arg(var, int32_t);
					memcpy(p, &intVar, sizeof(int32_t));
					p += sizeof(int32_t);
					entryLength += sizeof(int32_t);
					break;
				}
				/*
				 *  Pointers
				 */
				case UT_TRACE_DATA_TYPE_POINTER: {
					ptrVar = va_arg(var, char *);
					memcpy(p, &ptrVar, sizeof(char *));
					p += sizeof(char *);
					entryLength += sizeof(char *);
					break;
				}
				/*
				 *  64 bit integers
				 */
				case UT_TRACE_DATA_TYPE_INT64: {
					i64Var = va_arg(var, int64_t);
					memcpy(p, &i64Var, sizeof(int64_t));
					p += sizeof(int64_t);
					entryLength += sizeof(int64_t);
					break;
				}
				/*
				 *  Doubles
				 */
				/* CMVC 164940 All %f tracepoints are internally promoted to double.
				 * Affects:
				 *  TraceFormat  com/ibm/jvm/format/Message.java
				 *  TraceFormat  com/ibm/jvm/trace/format/api/Message.java
				 *  VM_Common    ute/ut_trace.c
				 *  TraceGen     OldTrace.java
				 * Intentional fall through to next case.
				 */
				case UT_TRACE_DATA_TYPE_DOUBLE: {
					doubleVar = va_arg(var, double);
					memcpy(p, &doubleVar, sizeof(double));
					p += sizeof(double);
					entryLength += sizeof(double);
					break;
				}
				/*
				 *  Bytes
				 */
				case UT_TRACE_DATA_TYPE_CHAR: {
					charVar = (char)va_arg(var, int32_t);
					*p = charVar;
					p += sizeof(char);
					entryLength += sizeof(char);
					break;
				}
				/*
				 *  Shorts
				 */
				case UT_TRACE_DATA_TYPE_SHORT: {
					shortVar = (short)va_arg(var, int32_t);
					memcpy(p, &shortVar, sizeof(short));
					p += sizeof(short);
					entryLength += sizeof(short);
					break;
				}
				}
				str++;
			}
			*p = (unsigned char)entryLength;
			/*
			 *  There's a chance of a wrap, so take it slow..
			 */
		} else {
			int32_t left;
			while (*str != '\0') {
				left = (int32_t)((char *)&trcBuf->record + OMR_TRACEGLOBAL(bufferSize) - p);

				switch (*str) {
				/*
				 *  Words
				 */
				case UT_TRACE_DATA_TYPE_INT32: {
					intVar = va_arg(var, int32_t);
					if (left > (int32_t)sizeof(int32_t)) {
						memcpy(p, &intVar, sizeof(int32_t));
						p += sizeof(int32_t);
						entryLength += sizeof(int32_t);
					} else {
						copyToBuffer(thr, bufferType, (char *)&intVar,
									 &p, sizeof(int32_t), &entryLength,
									 &trcBuf);
					}
					break;
				}
				/*
				 *  Pointers
				 */
				case UT_TRACE_DATA_TYPE_POINTER: {
					ptrVar = va_arg(var, char *);
					if (left > (int32_t)sizeof(char *)) {
						memcpy(p, &ptrVar, sizeof(char *));
						p += sizeof(char *);
						entryLength += sizeof(char *);
					} else {
						copyToBuffer(thr, bufferType, (char *)&ptrVar,
									 &p, sizeof(char *), &entryLength,
									 &trcBuf);
					}
					break;
				}
				/*
				 *  Strings
				 */
				case UT_TRACE_DATA_TYPE_STRING: {
					if ((modInfo != NULL) && (UT_APPLICATION_TRACE_MODULE == modInfo->moduleId)) {
						stringVar = format;
					} else {
						stringVar = va_arg(var, char *);
					}
					if (stringVar == NULL) {
						stringVar = UT_NULL_POINTER;
					}
					length = (int32_t)strlen(stringVar) + 1;
					if (left > length) {
						memcpy(p, stringVar, length);
						p += length;
						entryLength += length;
					} else {
						copyToBuffer(thr, bufferType, stringVar, &p,
									 length, &entryLength, &trcBuf);
					}
					break;
				}
				/*
				 *  UTF8 strings
				 */
				case UT_TRACE_DATA_TYPE_PRECISION: {
					length = va_arg(var, int32_t);
					str++;
					if (*str == UT_TRACE_DATA_TYPE_STRING) {
						stringVar = va_arg(var, char *);
						if (stringVar == NULL) {
							length = 0;
						}
						shortVar = (unsigned short)length;
						if (left > (int32_t)sizeof(short)) {
							memcpy(p, &shortVar, sizeof(short));
							p += sizeof(short);
							entryLength += sizeof(short);
							left -= (int32_t)sizeof(short);
						} else {
							copyToBuffer(thr, bufferType, (char *)&shortVar,
										 &p, sizeof(short), &entryLength,
										 &trcBuf);
							left = (int32_t)((char *)&trcBuf->record +
											 OMR_TRACEGLOBAL(bufferSize) - p);
						}
						if (length > 0) {
							if (left > length) {
								memcpy(p, stringVar, length);
								p += length;
								entryLength += length;
							} else {
								copyToBuffer(thr, bufferType, stringVar, &p,
											 length, &entryLength, &trcBuf);
							}
						}
					}
					break;
				}
				/*
				 *  64 bit integers
				 */
				case UT_TRACE_DATA_TYPE_INT64: {
					i64Var = va_arg(var, int64_t);
					if (left > (int32_t)sizeof(int64_t)) {
						memcpy(p, &i64Var, sizeof(int64_t));
						p += sizeof(int64_t);
						entryLength += sizeof(int64_t);
					} else {
						copyToBuffer(thr, bufferType, (char *)&i64Var,
									 &p, sizeof(int64_t), &entryLength,
									 &trcBuf);
					}
					break;
				}
				/*
				 *  Bytes
				 */
				case UT_TRACE_DATA_TYPE_CHAR: {
					charVar = (char)va_arg(var, int32_t);
					if (left > (int32_t)sizeof(char)) {
						*p = charVar;
						p += sizeof(char);
						entryLength += sizeof(char);
					} else {
						copyToBuffer(thr, bufferType, &charVar, &p,
									 sizeof(char), &entryLength, &trcBuf);
					}
					break;
				}
				/*
				 *  Doubles
				 */
				case UT_TRACE_DATA_TYPE_DOUBLE: {
					doubleVar = va_arg(var, double);
					if (left > (int32_t)sizeof(double)) {
						memcpy(p, &doubleVar, sizeof(double));
						p += sizeof(double);
						entryLength += sizeof(double);
					} else {
						copyToBuffer(thr, bufferType, (char *)&doubleVar,
									 &p, sizeof(double), &entryLength,
									 &trcBuf);
					}
					break;
				}
				/*
				 *  Shorts
				 */
				case UT_TRACE_DATA_TYPE_SHORT: {
					shortVar = (short)va_arg(var, int32_t);
					if (left > (int32_t)sizeof(short)) {
						memcpy(p, &shortVar, sizeof(short));
						p += sizeof(short);
						entryLength += sizeof(short);
					} else {
						copyToBuffer(thr, bufferType, (char *)&shortVar,
									 &p, sizeof(short), &entryLength,
									 &trcBuf);
					}
					break;
				}
				}
				str++;
			}
			/*
			 *  Set the entry length
			 */
			if ((char *)&trcBuf->record + OMR_TRACEGLOBAL(bufferSize) - p >
				(int32_t)sizeof(char)) {
				*p = (unsigned char)entryLength;
			} else {
				charVar = (unsigned char)entryLength;
				copyToBuffer(thr, bufferType, &charVar, &p, sizeof(char),
							 &entryLength, &trcBuf);
				entryLength--;
				p--;
			}
		}
	}

	/*
	 *  Most tracepoints should now be complete, so we might bail out now.
	 *  We don't need a -1 in the nextEntry assignment as we do elsewhere when
	 *  copyToBuffer's been involved because p is decremented above.
	 */
	if (entryLength <= UT_MAX_TRC_LENGTH) {
		trcBuf->record.nextEntry =
			(int32_t)(p - (char *)&trcBuf->record);
		return;
	} else {
		/*
		 *  Handle long trace records
		 */
		char temp[4];
		p++;
		temp[0] = 0;
		temp[1] = 0;
		temp[2] = (char)(entryLength >> 8);
		temp[3] = UT_TRC_EXTENDED_LENGTH;
		copyToBuffer(thr, bufferType, temp, &p, 4, &entryLength, &trcBuf);
		/* copyToBuffer increments p past the last byte written, but nextEntry
		 * needs to point to the length byte so we need -1 here.
		 */
		trcBuf->record.nextEntry =
			(int32_t)(p - (char *)&trcBuf->record - 1);
	}
}

#if OMR_ENABLE_EXCEPTION_OUTPUT
/*******************************************************************************
 * name        - trace
 * description - Call traceV passing trace data as a va_list
 * parameters  - OMR_TraceThread, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
trace(OMR_TraceThread *thr, UtModuleInfo *modinfo, uint32_t traceId, int bufferType, const char *spec, ...)
{
	va_list      var;

	va_start(var, spec);
	traceV(thr, modinfo, traceId, spec, var, bufferType);
	va_end(var);

}
#endif /* OMR_ENABLE_EXCEPTION_OUTPUT */

static void
logTracePoint(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs)
{
	va_list var;

	if ((thr->currentOutputMask & (UT_MINIMAL | UT_MAXIMAL)) != 0) {
		COPY_VA_LIST(var, varArgs);
		traceV(thr, modInfo, traceId, spec, var, UT_NORMAL_BUFFER);
	}

	if ((thr->currentOutputMask & UT_COUNT) != 0) {
		traceCount(modInfo, traceId);
	}

	if ((traceId & UT_SPECIAL_ASSERTION) != 0) {
		COPY_VA_LIST(var, varArgs);
		traceAssertion(thr, modInfo, traceId, var);
	} else if ((thr->currentOutputMask & UT_PRINT) != 0) {
		COPY_VA_LIST(var, varArgs);
		tracePrint(thr, modInfo, traceId, var);
	}

#if OMR_ENABLE_EXCEPTION_OUTPUT
	/* Write tracepoint to the global exception buffer. (Usually GC History) */
	if ((thr->currentOutputMask & UT_EXCEPTION) != 0) {
		COPY_VA_LIST(var, varArgs);
		getTraceLock(thr);
		if (*thr != OMR_TRACEGLOBAL(exceptionContext)) {
			/* This tracepoint is going to the global exception buffer, not to
			 * a per-thread buffer so, record if we aren't on the same thread
			 * as the last time we wrote a tracepoint to this buffer by writing
			 * an extra tracepoint, dg.259, and update the exceptionContext in
			 * UtGlobal for the next time we check this.
			 */
			OMR_TRACEGLOBAL(exceptionContext) = *thr;
			/* Write Trc_TraceContext_Event1, dg.259 */
			trace(thr, NULL, (UT_TRC_CONTEXT_ID << 8) | UT_MAXIMAL, UT_EXCEPTION_BUFFER, pointerSpec, thr);
		}
		traceV(thr, modInfo, traceId, spec, var, UT_EXCEPTION_BUFFER);
		freeTraceLock(thr);
	}
#endif /* OMR_ENABLE_EXCEPTION_OUTPUT */

	if ((traceId & UT_SPECIAL_ASSERTION) != 0) {
		raiseAssertion();
	}
}

void
omrTrace(void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...)
{
	OMR_TraceThread *thr = OMR_TRACE_THREAD_FROM_ENV(env);
	if (NULL != thr) {
		va_list var;
		va_start(var, spec);
		doTracePoint(thr, modInfo, traceId, spec, var);
		va_end(var);
	}
}

/*******************************************************************************
 * name        - doTracePoint
 * description - Make a tracepoint, not called directly outside of rastrace
 * parameters  - OMR_TraceThread, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
void
doTracePoint(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs)
{
	unsigned char savedOutputMask = '\0';
	BOOLEAN isRegular = FALSE; /* is this a regular tracepoint, and not an auxiliary tracepoint? */

	if ((NULL == omrTraceGlobal) || (OMR_TRACE_ENGINE_SHUTDOWN_STARTED == OMR_TRACEGLOBAL(initState))) {
		return;
	}

	if (NULL == thr) {
		return;
	}

	/* modInfo == NULL is for internal ute tracepoints */
	isRegular = (NULL == modInfo) || !MODULE_IS_AUXILIARY(modInfo);
	if (isRegular) {
		/* Recursion protection only applies to regular (not auxiliary) tracepoints. */
		if (thr->recursion) {
			return;
		}
		incrementRecursionCounter(thr);

		/* Auxiliary tracepoints go where the regular tracepoint that created them went. Regular tracepoints
		 * need to record where they are about to go in case they generate auxiliary tracepoints.
		 */
		thr->currentOutputMask = (unsigned char)(traceId & 0xFF);
	} else {
		/* This block is only executed for auxiliary tracepoints */

		/* The primary user of auxiliary tracepoints is jstacktrace. Sending jstacktrace data to
		 * minimal (i.e. throwing away all the stack data) makes no sense - so it is converted to
		 * maximal. currentOutputMask is reset below.
		 */
		savedOutputMask = thr->currentOutputMask;
		if (thr->currentOutputMask & UT_MINIMAL) {
			thr->currentOutputMask = (savedOutputMask & ~UT_MINIMAL) | UT_MAXIMAL;
		}
	}

	if ((OMR_TRACEGLOBAL(traceSuspend) == 0) && (thr->suspendResume >= 0)) {
		/* logTracePoint writes the trace point to the appropriate location */
		logTracePoint(thr, modInfo, traceId, spec, varArgs);
	}

	if (isRegular) {
		/* This block is only executed for regular tracepoints */
		decrementRecursionCounter(thr);
	} else {
		/* This block is only executed for auxiliary tracepoints*/
		thr->currentOutputMask = savedOutputMask;
	}
}

/*******************************************************************************
 * name        - internalTrace
 * description - Make an tracepoint, not called outside rastrace
 * parameters  - OMR_TraceThread, tracepoint identifier and trace data.
 * returns     - void
 ******************************************************************************/
void
internalTrace(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...)
{
	va_list      var;

	va_start(var, spec);
	doTracePoint(thr, modInfo, traceId, spec, var);
	va_end(var);
}

static UtProcessorInfo *
getProcessorInfo(void)
{
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));
	UtProcessorInfo *ret = NULL;
	const char *osarch;

	UT_DBGOUT(1, ("<UT> GetProcessorInfo called\n"));

	ret = (UtProcessorInfo *) omrmem_allocate_memory(sizeof(UtProcessorInfo), OMRMEM_CATEGORY_TRACE);
	if (ret == NULL) {
		return NULL;
	}
	memset(ret, '\0', sizeof(UtProcessorInfo));
	initHeader(&ret->header, "PINF", sizeof(UtProcessorInfo));
	initHeader(&ret->procInfo.header, "PIN", sizeof(UtProcInfo));
	osarch = omrsysinfo_get_CPU_architecture();
	if (NULL != osarch) {
		if ((strcmp(osarch, OMRPORT_ARCH_PPC) == 0)
		 || (strcmp(osarch, OMRPORT_ARCH_PPC64) == 0)
		 || (strcmp(osarch, OMRPORT_ARCH_PPC64LE) == 0)
		) {
			ret->architecture = UT_POWER;
			ret->procInfo.subtype = UT_POWERPC;
		} else if (strcmp(osarch, OMRPORT_ARCH_S390) == 0) {
			ret->architecture = UT_S390;
			ret->procInfo.subtype = UT_ESA;
		} else if (strcmp(osarch, OMRPORT_ARCH_S390X) == 0) {
			ret->architecture = UT_S390X;
			ret->procInfo.subtype = UT_TREX;
		} else if (strcmp(osarch, OMRPORT_ARCH_HAMMER) == 0) {
			ret->architecture = UT_AMD64;
			ret->procInfo.subtype = UT_OPTERON;
		} else if (strcmp(osarch, OMRPORT_ARCH_X86) == 0) {
			ret->architecture = UT_X86;
			ret->procInfo.subtype = UT_PIV;
		} else if (0 == strcmp(osarch, OMRPORT_ARCH_RISCV)) {
			ret->architecture = UT_RISCV;
			ret->procInfo.subtype = UT_RV64G;
		} else {
			ret->architecture = UT_UNKNOWN;
		}
	} else {
		ret->architecture = UT_UNKNOWN;
	}
#ifdef OMR_ENV_LITTLE_ENDIAN
	ret->isBigEndian =  FALSE;
#else
	ret->isBigEndian =  TRUE;
#endif
	ret->procInfo.traceCounter = UT_J9_TIMER;
	ret->wordsize = sizeof(uintptr_t) * 8;
	ret->onlineProcessors = (uint32_t)omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);

	return ret;
}

static void
raiseAssertion(void)
{
	if (OMR_TRACEGLOBAL(fatalassert)) {
		abort();
	}
}

/**
 * It is never safe to allow trace points while operating on the
 * buffer pool lock or while allocating a new trace buffer from
 * the pool because it cannot be done recursively. This helper
 * does these operations together while preventing trace points.
 */
OMR_TraceBuffer *
allocateTraceBuffer(OMR_TraceThread *currentThread)
{
	OMR_TraceBuffer *newTrcBuffer = NULL;

	if (NULL != currentThread) {
		incrementRecursionCounter(currentThread);
	}
	omrthread_monitor_enter(OMR_TRACEGLOBAL(bufferPoolLock));
	newTrcBuffer = (OMR_TraceBuffer *)pool_newElement(OMR_TRACEGLOBAL(bufferPool));
	omrthread_monitor_exit(OMR_TRACEGLOBAL(bufferPoolLock));
	if (NULL != currentThread) {
		decrementRecursionCounter(currentThread);
	}
	return newTrcBuffer;
}
