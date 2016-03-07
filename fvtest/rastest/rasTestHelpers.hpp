/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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


#if !defined(RASTESTHELPERS_H_INCLUDED)
#define RASTESTHELPERS_H_INCLUDED

#include "omr.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "omrrasinit.h"
#include "ute_core.h"
#include "testEnvironment.hpp"

extern PortEnvironment *rasTestEnv;

char *getTraceDatDir(intptr_t argc, const char **argv);
void reportOMRCommandLineError(OMRPortLibrary *portLibrary, const char *detailStr, va_list args);

void createThread(omrthread_t *newThread, uintptr_t suspend, omrthread_detachstate_t detachstate,
				  omrthread_entrypoint_t entryProc, void *entryArg);
intptr_t joinThread(omrthread_t threadToJoin);

/* traceRecordHelpers */

typedef struct PerThreadWrapBuffer {
	uint8_t *lastRecord; /* cache for wrapped tracepoints */
	size_t lastRecordBytes; /* size of lastRecord buffer, in bytes */
	size_t lastRecordPos; /* position in lastRecord after end of existing contents */
} PerThreadWrapBuffer;

typedef omr_error_t (*TracePointCallback)(void *userData, const char *tpMod, const uint32_t tpModLength, const uint32_t tpId,
		const UtTraceRecord *record, uint32_t firstParameterOffset, uint32_t parameterDataLength, int32_t isBigEndian);

void initWrapBuffer(PerThreadWrapBuffer *wrapBuffer);
void freeWrapBuffer(PerThreadWrapBuffer *wrapBuffer);

omr_error_t processTraceRecord(PerThreadWrapBuffer *wrapBuffer, UtSubscription *subscriptionID, TracePointCallback userCallback, void *userData);
uint8_t getU8FromTraceRecord(const UtTraceRecord *record, uint32_t offset);
uint32_t getU32FromTraceRecord(const UtTraceRecord *record, uint32_t offset, int32_t isBigEndian);
uint64_t getU64FromTraceRecord(const UtTraceRecord *record, uint32_t offset, int32_t isBigEndian);


#endif /* RASTESTHELPERS_H_INCLUDED */
