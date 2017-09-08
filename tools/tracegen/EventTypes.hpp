/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef EVENTTYPES_HPP_
#define EVENTTYPES_HPP_

/*
 * =============================================================================
 * Event types to match events type constants in com.ibm.jvm.Trace, used
 * to index into UT_FORMAT_TYPES when creating formatting strings for AppTrace.
 * =============================================================================
 */
typedef enum TraceEventType {
	UT_EVENT_TYPE 		= 0,
	UT_EXCEPTION_TYPE	= 1,
	UT_ENTRY_TYPE		= 2,
	UT_ENTRY_EXCPT_TYPE	= 3,
	UT_EXIT_TYPE		= 4,
	UT_EXIT_EXCPT_TYPE	= 5,
	UT_MEM_TYPE			= 6,
	UT_MEM_EXCPT_TYPE	= 7,
	UT_DEBUG_TYPE		= 8,
	UT_DEBUG_EXCPT_TYPE	= 9,
	UT_PERF_TYPE		= 10,
	UT_PERF_EXCPT_TYPE	= 11,
	UT_ASSERT_TYPE		= 12,
	UT_MAX_TYPES		= 13,
	TraceEventType_EnsureWideEnum = 0x1000000 /* force 4-byte enum */
} TraceEventType;

/*
 *  Trace data type identifiers (octal strings)
 */
#define TRACE_DATA_TYPE_END "\\0"
#define TRACE_DATA_TYPE_CHAR "\\1"
#define TRACE_DATA_TYPE_SHORT "\\2"
#define TRACE_DATA_TYPE_INT32 "\\4"
#define TRACE_DATA_TYPE_INT64 "\\10"
#define TRACE_DATA_TYPE_STRING "\\377"

/* The above are defined for compatibility */
#define TRACE_DATA_TYPE_FLOAT "\\5"
#define TRACE_DATA_TYPE_POINTER "\\6"
#define TRACE_DATA_TYPE_DOUBLE "\\7"
#define TRACE_DATA_TYPE_LONG_DOUBLE "\\11"
#define TRACE_DATA_TYPE_PRECISION "\\12"

#endif /* EVENTTYPES_HPP_ */
