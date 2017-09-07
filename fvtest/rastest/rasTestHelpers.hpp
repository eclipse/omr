/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
