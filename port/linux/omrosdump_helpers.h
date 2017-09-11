/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * @brief Dump formatting
 */

#include "omrport.h"
#include "omrportpriv.h"

/* TODO: move out struct to omrosdump_helpers.c to complete refactoring */
typedef struct MarkAllPagesWritableHeader {
	uint32_t eyeCatcher;
	uint32_t checksum;
	uintptr_t size;
	uintptr_t maxSize;
} MarkAllPagesWritableHeader;

uintptr_t renameDump(struct OMRPortLibrary *portLibrary, char *filename, pid_t pid, int signalNumber);
char *markAllPagesWritable(struct OMRPortLibrary *portLibrary);




