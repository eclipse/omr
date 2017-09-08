/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

/**
 * @file
 * @ingroup Port
 * @brief process introspection support
 */

#ifndef J9INTROSPECT_COMMON_H_
#define J9INTROSPECT_COMMON_H_

/* to add errors to this set of defines, include the textual error message in omrintrospect_common.c */
#define ALLOCATION_FAILURE		0x1
#define TIMEOUT					0x2
#define THREAD_COUNT_FAILURE	0x3
#define SUSPEND_FAILURE			0x4
#define RESUME_FAILURE			0x5
#define COLLECTION_FAILURE		0x6
#define INITIALIZATION_ERROR	0x7
#define INVALID_STATE			0x8
#define SIGNAL_SETUP_ERROR		0x9
#define USER_SIGNAL				0xA
#define UNSUPPORTED_PLATFORM	0xB
#define FAULT_DURING_BACKTRACE  0xC
#define CONCURRENT_COLLECTION   0xD
/* to add errors to this set of defines, include the textual error message in omrintrospect_common.c */

/* we overwrite prior speculation, but not previous errors */
#define RECORD_ERROR(state, err, detail) \
	if ((state)->error <= 0) {\
		(state)->error = err; \
		(state)->error_detail = detail; \
		if ((state)->error < 0) { \
			(state)->error = -(err); \
		} \
		(state)->error_string = error_descriptions[(state)->error]; \
	}

/* set an error before hand, just in case we fail so badly we can't record it after. error is negative to note speculation */
#define SPECULATE_ERROR(state, err, detail) \
	if (!(state)->error) {\
		(state)->error = -err; \
		(state)->error_detail = detail; \
		(state)->error_string = error_descriptions[err]; \
	}

/* clears any speculative error that's set */
#define CLEAR_ERROR(state) \
	if ((state)->error < 0) { \
		(state)->error = 0; \
		(state)->error_detail = 0; \
		(state)->error_string = NULL; \
	}

extern const char *error_descriptions[];


#endif /* J9INTROSPECT_COMMON_H_ */
