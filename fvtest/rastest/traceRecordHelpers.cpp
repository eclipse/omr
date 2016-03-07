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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rasTestHelpers.hpp"

#include "omrcomp.h"
#include "omrport.h"
#include "ute_dataformat.h"

/*
 * Helpers for processing trace records sent to subscribers.
 * OMRTODO Some of this should be pushed back into omrtraceformatter.c
 */

typedef struct TracePointIterator {
	int32_t currentPos;
	int32_t isBigEndian;
	uint32_t longTracePointLength;
	TracePointCallback userCallback;
	void *userData;
} TracePointIterator;

typedef enum TracePointIteratorState {
	CONTINUE,
	STOP,
	STOP_WITH_ERROR
} TracePointIteratorState;

static TracePointIteratorState nextTracePoint(PerThreadWrapBuffer *wrapBuffer, const UtTraceRecord *record, TracePointIterator *iter);
static TracePointIteratorState parseTracePoint(const UtTraceRecord *record, uint32_t offset, uint32_t tpLength, TracePointIterator *iter);
static uint32_t getTraceIdFromBuffer(const UtTraceRecord *record, uint32_t offset);


void
initWrapBuffer(PerThreadWrapBuffer *wrapBuffer)
{
	wrapBuffer->lastRecord = NULL;
	wrapBuffer->lastRecordBytes = 0;
	wrapBuffer->lastRecordPos = 0;
}

void
freeWrapBuffer(PerThreadWrapBuffer *wrapBuffer)
{
	if (NULL != wrapBuffer->lastRecord) {
		free(wrapBuffer->lastRecord);
	}
}

omr_error_t
processTraceRecord(PerThreadWrapBuffer *wrapBuffer, UtSubscription *subscriptionID, TracePointCallback userCallback, void *userData)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const UtTraceRecord *record = (const UtTraceRecord *)subscriptionID->data;
	TracePointIterator iter;

	int32_t recordSize = record->nextEntry + 1;
	if (recordSize > subscriptionID->dataLength) {
		fprintf(stderr, "ERROR: trace buffer (%d bytes) doesn't contain last entry (%d bytes)\n", subscriptionID->dataLength, recordSize);
		abort();
	}

	if (-1 == record->nextEntry) {
		/*
		 * If (record->nextEntry == -1), then this entire buffer is spanned by a tracepoint. Add it to the lastRecord cache.
		 * TODO test this
		 */

		size_t bytesNeeded = subscriptionID->dataLength - record->firstEntry;
		if ((wrapBuffer->lastRecordPos + bytesNeeded) < wrapBuffer->lastRecordBytes) {
			wrapBuffer->lastRecordBytes += bytesNeeded;
			wrapBuffer->lastRecord = (uint8_t *)realloc(wrapBuffer->lastRecord, wrapBuffer->lastRecordBytes);
		}
		memcpy(wrapBuffer->lastRecord + wrapBuffer->lastRecordPos, (uint8_t *)record + record->firstEntry, bytesNeeded);
		wrapBuffer->lastRecordPos += bytesNeeded;

	} else {
		if (record->nextEntry == record->firstEntry) {
			fprintf(stderr, "ERROR: empty trace record\n");
			abort();
		}
		if (record->nextEntry < record->firstEntry) {
			fprintf(stderr, "ERROR: invalid nextEntry\n");
			abort();
		}

		/*
		 * We iterate tracepoints backwards, starting from the last tracepoint (the newest tracepoint) in the buffer.
		 * Each tracepoint's length is recorded at the end of its data.
		 */

		/* Set up the iterator */
		iter.currentPos = record->nextEntry; /* offset to the length byte of the last tracepoint in the buffer */
#ifdef OMR_ENV_LITTLE_ENDIAN
		iter.isBigEndian = FALSE;
#else
		iter.isBigEndian = TRUE;
#endif
		iter.longTracePointLength = 0;
		iter.userCallback = userCallback;
		iter.userData = userData;

		while (CONTINUE == nextTracePoint(wrapBuffer, record, &iter)) {
			/* continue */
		}

		/* We might have cached some data from the previous trace record even if there was no wrapped tracepoint.
		 * Throw away the cached data.
		 */
		wrapBuffer->lastRecordPos = 0;

		/*
		 * Cache the data in the record after the last complete tracepoint, in case the last tracepoint wrapped to the next buffer.
		 *
		 * record->nextEntry points to the last byte of the last complete tracepoint. Wrapped data starts at (record->nextEntry + 1).
		 */
		size_t wrappedDataStart = record->nextEntry + 1;
		size_t wrappedDataSize = subscriptionID->dataLength - wrappedDataStart;
		if (wrappedDataSize > 0) {
			/* bytesNeeded = (sizeof UtTraceRecord + thread name) + (data following the last complete trace record) */
			size_t bytesNeeded = record->firstEntry + wrappedDataSize;

			if (wrapBuffer->lastRecordBytes < bytesNeeded) {
				wrapBuffer->lastRecordBytes = bytesNeeded;
				if (NULL != wrapBuffer->lastRecord) {
					free(wrapBuffer->lastRecord);
				}
				wrapBuffer->lastRecord = (uint8_t *)malloc(wrapBuffer->lastRecordBytes);
			}
			memcpy(wrapBuffer->lastRecord, record, record->firstEntry);
			memcpy(wrapBuffer->lastRecord + record->firstEntry, (uint8_t *)record + wrappedDataStart, wrappedDataSize);
			wrapBuffer->lastRecordPos = bytesNeeded;
			((UtTraceRecord *)wrapBuffer->lastRecord)->nextEntry = -1;
		}
	}
	return rc;
}


static TracePointIteratorState
nextTracePoint(PerThreadWrapBuffer *wrapBuffer, const UtTraceRecord *record, TracePointIterator *iter)
{
	uint32_t offset = iter->currentPos;
	uint32_t tpLength = 0;

	if (iter->currentPos <= record->firstEntry) {
		/* currentPos is at or before the start of the data - not possible to continue,
		 * or the normal exit condition.
		 */
		return STOP;
	}

	tpLength = getU8FromTraceRecord(record, offset);

	if (0 != iter->longTracePointLength) {
		/* The previous tracepoint was a long tracepoint marker, so it
		 * contained the upper word of a two byte length descriptor.
		 */
		tpLength |= (iter->longTracePointLength << 8);
		iter->longTracePointLength = 0;
	}

	/* Long trace points have length stored in 2bytes, so check we've not overflowed that
	 * limit.
	 */
	if ((0 == tpLength) || (tpLength > UT_MAX_EXTENDED_LENGTH)) {
		/* prevent looping in case of bad data */
		return STOP;
	}

	/* Check whether we are about to process a wrapped tracepoint (a tracepoint that spans multiple buffers,
	 * not a circular trace buffer).
	 *
	 * Subscribers shouldn't be given circular buffers.
	 * TODO When transitioning from internal to external trace, could the first trace buffer be circular?
	 *
	 * 'offset' points to location of 'tpLength' for current tracepoint.
	 * 'offset - tpLength' points to previous tracepoint's 'tpLength', if it exists.
	 * 'offset + 1 - tpLength' points to start of current tracepoint's data, which must be at least 'record->firstEntry'
	 * if the tracepoint fits in this buffer.
	 */
	if ((tpLength > offset) || ((int32_t)(offset + 1 - tpLength) < record->firstEntry)) {
		/* This tracepoint is wrapped from the previous buffer.
		 * This must also be the first tracepoint in this buffer, so we'll stop processing this buffer
		 * after this tracepoint.
		 */
		if (0 == wrapBuffer->lastRecordPos) {
			fprintf(stderr, "ERROR: no cached wrapped buffer data\n");
			abort();
		}


		size_t partialBytes = offset - record->firstEntry + 1;
		if (wrapBuffer->lastRecordBytes < (wrapBuffer->lastRecordPos + partialBytes)) {
			wrapBuffer->lastRecordBytes += partialBytes;
			wrapBuffer->lastRecord = (uint8_t *)realloc(wrapBuffer->lastRecord, wrapBuffer->lastRecordBytes);
		}
		memcpy(wrapBuffer->lastRecord + wrapBuffer->lastRecordPos, (uint8_t *)record + record->firstEntry, partialBytes);
//		printf("thread %p wrapped tpLength %u partialBytes %lu\n", omrthread_self(), tpLength, partialBytes);
		wrapBuffer->lastRecordPos += partialBytes;

		parseTracePoint((const UtTraceRecord *)wrapBuffer->lastRecord, (uint32_t)(wrapBuffer->lastRecordPos - tpLength - 1), tpLength, iter);

		/* mark the temp buffer reusable */
		wrapBuffer->lastRecordPos = 0;
		return STOP;
	}

	/* move currentPos to the end of the next tracepoint, where its length is written */
	iter->currentPos -= tpLength;

	/* parse the relevant section into a tracepoint */
	return parseTracePoint(record, offset - tpLength, tpLength, iter);
}


/**
 * OMRTODO Some cases may not be handled; cross-check with omrtraceformatter.c
 *
 * @param[in] record	One buffer of tracepoints from one thread
 * @param[in] offset	Current tracepoint's data offset from record (actually, the length marker of the prev tp)
 * @param[in] tpLength  Length of current tracepoint's data
 */
static TracePointIteratorState
parseTracePoint(const UtTraceRecord *record, uint32_t offset, uint32_t tpLength, TracePointIterator *iter)
{
	uint32_t traceId = getTraceIdFromBuffer(record, offset + TRACEPOINT_RAW_DATA_TP_ID_OFFSET);

	/* check for all the control/internal tracepoints */
	if (traceId == 0x0010) { /* 0x0010nnnn8 == lost record tracepoint */
		fprintf(stderr, "ERROR: *** lost records found in trace buffer - use external formatting tools for details.\n");
		return STOP;

	} else if ((traceId == 0x0000) && (tpLength == UT_TRC_SEQ_WRAP_LENGTH)) {
		/* skip to next tracepoint */

	} else if (tpLength == 4) { /* 0x0000nnnn8 == sequence wrap tracepoint */
		/* this is a long tracepoint */
		uint8_t longTracePointLength;

		longTracePointLength = getU8FromTraceRecord(record, offset + TRACEPOINT_RAW_DATA_TP_ID_OFFSET + 2);
		/* the next call to nextTracePoint() will pick this up and use it appropriately */
		iter->longTracePointLength = (uint32_t)longTracePointLength;

	} else if (tpLength == UT_TRC_SEQ_WRAP_LENGTH) {
		/* skip to next tracepoint */

	} else {
		/* otherwise all is well and we have a normal tracepoint */
		uint32_t modNameLength = getU32FromTraceRecord(record, offset + TRACEPOINT_RAW_DATA_MODULE_NAME_LENGTH_OFFSET, iter->isBigEndian);

		/* Module name must not be longer than (the tracepoint length - start of module name string) */
		/* modNameLength == 0 -- no modname - most likely partially overwritten - it's unformattable as a result! */
		if ((0 != modNameLength) && (tpLength >= TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET) && (modNameLength <= (uint32_t)(tpLength - TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET))) {
			if (traceId > 257) {
				traceId -= 257;

				/* the modName was written straight to buf as (char *) */
				const char *recordAsCharBuf = (const char *)record;
				const char *modName = &recordAsCharBuf[offset + TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET];
				uint32_t parameterOffset = offset + TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET + modNameLength;
				uint32_t parameterDataLength = tpLength - (TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET + modNameLength);
				if (OMR_ERROR_NONE !=
					iter->userCallback(iter->userData, modName, modNameLength, traceId, record,
									   parameterOffset, parameterDataLength, iter->isBigEndian)
				) {
					return STOP_WITH_ERROR;
				}
			}
		}
	}
	return CONTINUE;
}

static uint32_t
getTraceIdFromBuffer(const UtTraceRecord *record, uint32_t offset)
{
	uint32_t ret = 0;
	uint8_t data[3];

	data[0] = getU8FromTraceRecord(record, offset);
	data[1] = getU8FromTraceRecord(record, offset + 1);
	data[2] = getU8FromTraceRecord(record, offset + 2);

	/* trace ID's are stored in hard coded big endian order */
	ret = (data[0] << 16) | (data[1] << 8) | (data[2]);

	/* remove comp Id's */
	ret &= 0x3FFF;
	return ret;
}

uint8_t
getU8FromTraceRecord(const UtTraceRecord *record, uint32_t offset)
{
	const uint8_t *tempPtr = (const uint8_t *)record;
	return tempPtr[offset];
}

uint32_t
getU32FromTraceRecord(const UtTraceRecord *record, uint32_t offset, int32_t isBigEndian)
{
	uint32_t ret = 0;
	uint8_t data[4];

	data[0] = getU8FromTraceRecord(record, offset);
	data[1] = getU8FromTraceRecord(record, offset + 1);
	data[2] = getU8FromTraceRecord(record, offset + 2);
	data[3] = getU8FromTraceRecord(record, offset + 3);

	/* integer values are written out in platform endianess */
	if (isBigEndian) {
		ret = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
	} else {
		ret = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | (data[0]);
	}
	return ret;
}

uint64_t
getU64FromTraceRecord(const UtTraceRecord *record, uint32_t offset, int32_t isBigEndian)
{
	uint64_t ret = 0;
	uint8_t data[8];

	data[0] = getU8FromTraceRecord(record, offset);
	data[1] = getU8FromTraceRecord(record, offset + 1);
	data[2] = getU8FromTraceRecord(record, offset + 2);
	data[3] = getU8FromTraceRecord(record, offset + 3);
	data[4] = getU8FromTraceRecord(record, offset + 4);
	data[5] = getU8FromTraceRecord(record, offset + 5);
	data[6] = getU8FromTraceRecord(record, offset + 6);
	data[7] = getU8FromTraceRecord(record, offset + 7);

	/* integer values are written out in platform endianess */
	if (isBigEndian) {
		ret = ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) | ((uint64_t)data[2] << 40) | ((uint64_t)data[3] << 32)
				| ((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) | ((uint64_t)data[6] << 8) | ((uint64_t)data[7]);
	} else {
		ret = ((uint64_t)data[7] << 56) | ((uint64_t)data[6] << 48) | ((uint64_t)data[5] << 40) | ((uint64_t)data[4] << 32)
				| ((uint64_t)data[3] << 24) | ((uint64_t)data[2] << 16) | ((uint64_t)data[1] << 8) | ((uint64_t)data[0]);
	}
	return ret;
}
