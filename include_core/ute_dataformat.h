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

#ifndef UTE_DATAFORMAT_H_INCLUDED
#define UTE_DATAFORMAT_H_INCLUDED

/*
 * @ddr_namespace: map_to_type=UteDataformatConstants
 */

#include <limits.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/*
 * =============================================================================
 *  Constants
 * =============================================================================
 */
#define UT_DEFAULT_BUFFERSIZE         8192
#define UT_MINIMUM_BUFFERSIZE         1000
#define UT_MAX_THREAD_NAME_LENGTH     512
#define UT_FASTPATH                   17
#define UT_TRACE_INTERNAL             0
#define UT_TRACE_EXTERNAL             1
#define UT_STRUCT_ALIGN               4
#define UT_TRACE_BUFFER_NAME          "UTTB"
#define UT_TRACE_HEADER_NAME          "UTTH"
#define UT_TRACE_SECTION_NAME         "UTTS"
#define UT_STARTUP_SECTION_NAME       "UTSO"
#define UT_SERVICE_SECTION_NAME       "UTSS"
#define UT_ACTIVE_SECTION_NAME        "UTTA"
#define UT_PROC_SECTION_NAME          "UTPR"
#define UT_NULL_POINTER               "[Null Pointer]"

#define UT_ENDIAN_SIGNATURE           0x12345678

#define UT_TRC_SPECIAL_MASK           0x3ff
#define UT_TRC_ID_MASK                0x3fff
#define UT_TRC_EXTENDED_LENGTH        4
#define UT_TRC_SEQ_WRAP_LENGTH        8
#define UT_MAX_TRC_LENGTH             255
#define UT_MAX_EXTENDED_LENGTH        ((64 * 1024) - 9)
#define UT_TRC_SEQ_WRAP_ID            "\x0\x0\x0"
#define UT_TRC_LOST_COUNT_ID          "\x0\x1\x0"
/* dg.259 */
#define UT_TRC_CONTEXT_ID             0x103
/* dg.260 */
#define UT_TRC_RESET_ID               0x104
/* dg.262 */
#define UT_TRC_PURGE_ID               0x106
/* dg.327 */
#define UT_TRC_LAST_SPECIAL_ID        0x147

/*
 *  Trace data type identifiers
 */
#define UT_TRACE_DATA_TYPE_END          0
#define UT_TRACE_DATA_TYPE_CHAR         1
#define UT_TRACE_DATA_TYPE_SHORT        2
#define UT_TRACE_DATA_TYPE_INT32        4
#define UT_TRACE_DATA_TYPE_INT64        8
#define UT_TRACE_DATA_TYPE_STRING      -1
/* The above are for compatibility */

#define UT_TRACE_DATA_TYPE_FLOAT        5
#define UT_TRACE_DATA_TYPE_POINTER      6
#define UT_TRACE_DATA_TYPE_DOUBLE       7
#define UT_TRACE_DATA_TYPE_LONG_DOUBLE  9
#define UT_TRACE_DATA_TYPE_PRECISION    10

/* In a UtTraceRecord, offsets from the previous tracepoint's length byte */
#define TRACEPOINT_RAW_DATA_TP_ID_OFFSET 1
#define TRACEPOINT_RAW_DATA_TIMER_WRAP_UPPER_WORD_OFFSET 4
#define TRACEPOINT_RAW_DATA_TIMESTAMP_OFFSET 4
#define TRACEPOINT_RAW_DATA_MODULE_NAME_LENGTH_OFFSET 8
#define TRACEPOINT_RAW_DATA_MODULE_NAME_DATA_OFFSET 12

/*
 * =============================================================================
 *   Processor architecture descriptors
 * =============================================================================
 */
typedef enum {
	UT_I486,
	UT_I586,
	UT_PII,
	UT_PIII,
	UT_MERCED,
	UT_MCKINLEY,
	UT_POWERRS,
	UT_POWERPC,
	UT_GIGAPROCESSOR,
	UT_ESA,
	UT_PIV,
	UT_TREX,
	UT_OPTERON,
	UT_RV64G,
	UT_SUBTYPE_FORCE_INTEGER = INT_MAX
} UtSubtype;

typedef enum {
	UT_SEQ_COUNTER,
	UT_SPECIAL,
	UT_RDTSC_TIMER,
	UT_AIX_TIMER,
	UT_MFSPR_TIMER,
	UT_MFTB_TIMER,
	UT_STCK_TIMER,
	UT_J9_TIMER,
	UT_COUNTER_FORCE_INTEGER = INT_MAX
} UtTraceCounter;

typedef struct {
	UtDataHeader header;
	UtSubtype subtype;
	UtTraceCounter traceCounter;
} UtProcInfo;

typedef enum {
	UT_UNKNOWN = 0,
	UT_X86,
	UT_S390,
	UT_POWER,
	UT_IA64,
	UT_S390X,
	UT_AMD64,
	UT_RISCV,
	UT_ARCHITECTURE_FORCE_INTEGER = INT_MAX
} UtArchitecture;

typedef struct UtProcessorInfo {
	UtDataHeader header;
	UtArchitecture architecture;
	int32_t isBigEndian;
	uint32_t wordsize;
	uint32_t onlineProcessors;
	UtProcInfo procInfo;
	uint32_t pagesize;
	int32_t cpuCountVaries;
} UtProcessorInfo;

/*
 * =============================================================================
 * UtProcSection (UTPR)
 * =============================================================================
 */
#define UT_TRACE_PROC_SECTION_NAME "UTPR"
typedef struct UtProcSection {
	UtDataHeader header; /* Eyecatcher, version etc        */
	UtProcessorInfo processorInfo; /* Processor info                 */
} UtProcSection;

/*
 * =============================================================================
 * Active (UTTA)
 * =============================================================================
 */
#define UT_TRACE_ACTIVE_SECTION_NAME "UTTA"
typedef struct UtActiveSection {
	UtDataHeader header; /* Eyecatcher, version etc        */
	char active[1]; /* Trace activation commands      */
} UtActiveSection;

/*
 * =============================================================================
 * UtServiceSection (UTSS)
 * =============================================================================
 */
#define UT_TRACE_SERVICE_SECTION_NAME "UTSS"
typedef struct UtServiceSection {
	UtDataHeader header; /* Eyecatcher, version etc        */
	char level[1]; /* Service level info             */
} UtServiceSection;

/*
 * =============================================================================
 * UtStartupSection (UTSO)
 * =============================================================================
 */
#define UT_TRACE_STARTUP_SECTION_NAME "UTSO"
typedef struct UtStartupSection {
	UtDataHeader header; /* Eyecatcher, version etc        */
	char options[1]; /* Startup options                */
} UtStartupSection;

/*
 * =============================================================================
 * UtTraceSection  (UTTS)
 * =============================================================================
 */
#define UT_TRACE_SECTION_NAME "UTTS"
typedef struct UtTraceSection {
	UtDataHeader header; /* Eyecatcher, version etc        */
	uint64_t startPlatform; /* Platform timer                 */
	uint64_t startSystem; /* Time relative 1/1/1970         */
	int32_t type; /* Internal / External trace      */
	int32_t generations; /* Number of generations          */
	int32_t pointerSize; /* Size in bytes of a pointer     */
} UtTraceSection;

/*
 * =============================================================================
 * UtTraceFileHdr (UTTH)
 * =============================================================================
 */
#define UT_TRACEFILE_HEADER_NAME "UTTH"
typedef struct UtTraceFileHdr {
	UtDataHeader header; /* Eyecatcher, version etc        */
	int32_t bufferSize; /* Trace buffer size              */
	int32_t endianSignature; /* 0x12345678 in host order       */
	/* If any of these offsets are    */
	/* zero, the section isn't        */
	/* present.                       */
	int32_t traceStart; /* Offset UtTraceSection          */
	int32_t serviceStart; /* Offset UtServiceSection        */
	int32_t startupStart; /* Offset UtStartupSection        */
	int32_t activeStart; /* Offset UtActiveSection         */
	int32_t processorStart; /* Offset UtProcSection           */
	UtTraceSection traceSection;
	/*
	 * These are attached to the UtTraceFileHdr but are variable
	 * length so specifying them as fields was incorrect.
	 * UtServiceSection  serviceSection;
	 * UtStartupSection  startupSection;
	 * UtActiveSection   activeSection;
	 * UtProcSection     procSection;
	 */
} UtTraceFileHdr;

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* !UTE_DATAFORMAT_H_INCLUDED */
